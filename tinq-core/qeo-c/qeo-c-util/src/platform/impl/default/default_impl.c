/*
 * Copyright (c) 2014 - Qeo LLC
 *
 * The source code form of this Qeo Open Source Project component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the Clear BSD
 * License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
 * for more details.
 *
 * The Qeo Open Source Project also includes third party Open Source Software.
 * See LICENSE file for more details.
 */

/*#######################################################################
 # HEADER (INCLUDE) SECTION                                              #
 ########################################################################*/
#ifndef DEBUG
#define NDEBUG
#endif

#define _GNU_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <platform_api/platform_api.h>
#include <platform_api/platform_common.h>
#include <qeo/log.h>
#include <qeo/util_error.h>

#include "linux_default_device_p.h"
/*#######################################################################
 # STATIC FUNCTION PROTOTYPE
 ########################################################################*/
static qeo_util_retcode_t default_registration_params_needed(uintptr_t app_context, qeo_platform_security_context_t context);
static qeo_util_retcode_t remote_registration(qeo_platform_security_context_t context,
                                                                    const char *rrf);
static void default_security_update_state(uintptr_t app_context, qeo_platform_security_context_t context, qeo_platform_security_state state, qeo_platform_security_state_reason state_reason);
static qeo_util_retcode_t default_remote_registration_confirmation_needed(uintptr_t app_context, qeo_platform_security_context_t context,
                                                                                 const qeo_platform_security_remote_registration_credentials_t *rrcred);

/*#######################################################################
 # STATIC VARIABLES
 ########################################################################*/
const static qeo_platform_callbacks_t _default_platform_cbs = {
    .on_reg_params_needed = default_registration_params_needed,
    .on_sec_update = default_security_update_state,
    .on_rr_confirmation_needed = default_remote_registration_confirmation_needed
};

/*#######################################################################
 # STATIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/

static qeo_util_retcode_t remote_registration(qeo_platform_security_context_t context,
                                                                    const char *rrf){
    qeo_util_retcode_t ret = QEO_UTIL_EFAIL;
    FILE *f = NULL;

    do {
        char suggested_username[64];
        unsigned long registration_window;
        /* file/fifo should be created in advance */
        if ((f = fopen(rrf, "r")) == NULL){
            qeo_log_e("Could not open remote registration file for reading");
            break;
        }

        if (fscanf(f, "%63s %lu", suggested_username, &registration_window) != 2){
            qeo_log_e("Could not read from remote_registration file");
            break;
        }
    
        ret = qeo_platform_set_remote_registration_params(context, suggested_username, registration_window);

    } while (0);

    if (f != NULL){
        fclose(f);
    }

    return ret;
}

static qeo_util_retcode_t cli_otc_url(qeo_platform_security_context_t context){

    qeo_util_retcode_t retval = QEO_UTIL_OK;
    size_t len = 0;
    char *otc = NULL;
    char *url = NULL;
    ssize_t bytes_read = 0;
    char *lf = NULL;

    do {
        fprintf(stdout, "Please provide the OTC (Press enter to cancel): ");
        fflush(stdout);
        bytes_read = getline(&otc, &len, stdin);
        /* not sure this will work on all terminals - some might return \r\n ... */
        if (bytes_read == -1 || (bytes_read == 1 && otc[0] == '\n')){
            retval = qeo_platform_cancel_registration(context);
            break;
        } else {
            lf = strchr(otc, '\n');
            if (lf != NULL){
                *lf = '\0';
            }
        }

        fprintf(stdout, "Please provide the URL [" QEO_REGISTRATION_URL "]: ");
        fflush(stdout);
        bytes_read = getline(&url, &len, stdin);
        if (bytes_read == -1 || (bytes_read == 1 && url[0] == '\n')){
            if (bytes_read != -1){
                free(url);
            }
            url = strdup(QEO_REGISTRATION_URL);
        } else {
            lf = strchr(url, '\n');
            if (lf != NULL){
                *lf = '\0';
            }
        }

        retval = qeo_platform_set_otc_url(context, otc, url);
    } while (0);

    free(otc);
    free(url);

    return retval;
}

static qeo_util_retcode_t default_registration_params_needed(uintptr_t app_context, qeo_platform_security_context_t context){


    qeo_util_retcode_t retval = QEO_UTIL_OK;
    char *rrf = NULL;

    if ((rrf = getenv("REMOTE_REGISTRATION_FILE")) != NULL){
        if (remote_registration(context, rrf) != QEO_UTIL_OK){
            qeo_log_w("Fallback to prompt");
            return cli_otc_url(context);
        }
    } else {
        return cli_otc_url(context);
    }

    return retval;
}

static void default_security_update_state(uintptr_t app_context, qeo_platform_security_context_t context, qeo_platform_security_state state, qeo_platform_security_state_reason state_reason)
{
    if (state == QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE){
        fprintf(stderr, "Could not authenticate QEO (reason = %s) !\r\n", platform_security_state_reason_to_string(state_reason));
    }
    /* ignore all other states */
}

static qeo_util_retcode_t default_remote_registration_confirmation_needed(uintptr_t app_context, qeo_platform_security_context_t context,
                                                                                 const qeo_platform_security_remote_registration_credentials_t *rrcred){
                                                                                 

    const char *auto_confirmation = getenv("REMOTE_REGISTRATION_AUTO_CONFIRM");
    char reply[4];


    if (auto_confirmation != NULL){
        bool feedback = (bool)(auto_confirmation[0] - '0');
        fprintf(stdout, "Automatic confirmation (%c) of remote registration credentials (realm name = %s, url = %s)\r\n", feedback ? 'Y' : 'N', rrcred->realm_name, rrcred->url);
        return qeo_platform_confirm_remote_registration_credentials(context, feedback);
    }

    fprintf(stdout, "Management app wants to register us in realm %s, URL: %s. [Y/n]\r\n", rrcred->realm_name, rrcred->url);

    if (scanf("%3s", reply) == 1){
        if (reply[0] == 'n'){
            return qeo_platform_confirm_remote_registration_credentials(context, false);
        } else {
            return qeo_platform_confirm_remote_registration_credentials(context, true);
        }
    }

    return QEO_UTIL_OK;
}

/*#######################################################################
 # PUBLIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/


#ifdef __mips__
void __attribute__ ((constructor)) default_impl_init(void){
#else
void __attribute__ ((constructor(1000))) default_impl_init(void){
#endif

    qeo_util_retcode_t ret;
    const char *ca_file = NULL;
    const char *ca_path = NULL;

    if ((ret = qeo_platform_init(0, &_default_platform_cbs)) != QEO_UTIL_OK){
        qeo_log_e("Could not init qeo platform layer with default implementation");
        return;
    }

    if (qeo_platform_set_device_info(get_default_device_info()) != QEO_UTIL_OK){
        qeo_log_e("Could not set device info");
        return;

    }

    if (qeo_platform_set_device_storage_path(get_default_device_storage_path()) != QEO_UTIL_OK){
        qeo_log_e("Could not set device storage path");
        return;
    }
    get_default_cacert_path(&ca_file, &ca_path);
    if (qeo_platform_set_cacert_path(ca_file, ca_path) != QEO_UTIL_OK) {
        qeo_log_e("Could not set CA certificates path");
        return;
    }
}


#ifdef __mips__
void __attribute__ ((destructor)) default_impl_destroy(void){
#else
void __attribute__ ((destructor(1000))) default_impl_destroy(void){
#endif

    free_default_paths();
    free_default_device_info();

}
