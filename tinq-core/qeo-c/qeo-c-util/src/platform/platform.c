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
#define _GNU_SOURCE
#ifndef DEBUG
#define NDEBUG
#endif
#include <platform_api/platform_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <qeo/log.h>
#include <qeo/platform.h>
/*#######################################################################
 # TYPES SECTION                                                         #
 ########################################################################*/
#ifndef LINE_MAX
#define LINE_MAX 256
#endif
/*#######################################################################
 # STATIC FUNCTION DECLARATION                                           #
 ########################################################################*/
/*#######################################################################
 # STATIC VARIABLE SECTION                                               #
 ########################################################################*/
static uintptr_t _app_ctx;

static const qeo_platform_device_info *_device_info;
static const char *_device_storage_path;
static const char *_ca_file = NULL;
static const char *_ca_path = NULL;

static qeo_platform_callbacks_t _cbs;

/* in theory these could be different per security context, but in practice not */
static qeo_platform_security_set_registration_credentials_cb _set_reg_cred_cb;
static qeo_platform_security_registration_credentials_cancelled_cb _set_cancelled_cb;
static qeo_platform_security_remote_registration_credentials_feedback_cb _feedback_cb;
/*#######################################################################
 # STATIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/

/*#######################################################################
 # PUBLIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/

qeo_util_retcode_t qeo_platform_init(uintptr_t app_ctx, const qeo_platform_callbacks_t *cbs){

    if (cbs == NULL){
        return QEO_UTIL_EINVAL; 
    }
    
    /* overrides existing stuff ! */
    _app_ctx = app_ctx;
    _cbs = *cbs;

    

    return QEO_UTIL_OK;
}


qeo_util_retcode_t qeo_platform_set_otc_url(qeo_platform_security_context_t sec_context, const char *otc, const char *url){

    if (sec_context == 0){
        return QEO_UTIL_EINVAL;
    }

    /* possible improvement is to store otc and url until we know callback */
    if (_set_reg_cred_cb == NULL){
        return QEO_UTIL_EBADSTATE;
    }

    const qeo_platform_security_registration_credentials reg_cred = {
        .reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP,
        .u.otp.otp = otc,
        .u.otp.url = url,
    };

    return _set_reg_cred_cb(sec_context, &reg_cred);
}

qeo_util_retcode_t qeo_platform_set_remote_registration_params(qeo_platform_security_context_t sec_context, const char *suggested_username, unsigned long registration_window){

    if (sec_context == 0){
        return QEO_UTIL_EINVAL;
    }

    /* possible improvement is to store suggested_username and registration_window until we know callback  */
    if (_set_reg_cred_cb == NULL){
        return QEO_UTIL_EBADSTATE;
    }

    const qeo_platform_security_registration_credentials reg_cred = {
        .reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION,
        .u.remote_registration.registration_window = registration_window,
        .u.remote_registration.suggested_username = suggested_username
    };

    return _set_reg_cred_cb(sec_context, &reg_cred);

}

qeo_util_retcode_t qeo_platform_cancel_registration(qeo_platform_security_context_t sec_context){

    if (sec_context == 0){
        return QEO_UTIL_EINVAL;
    }

    /* possible improvement is to store cancel until we know callback */
    if (_set_cancelled_cb == NULL){
        return QEO_UTIL_EBADSTATE;
    }

    return _set_cancelled_cb(sec_context);

}

void qeo_platform_destroy(void){

}


qeo_util_retcode_t qeo_platform_security_registration_credentials_needed(qeo_platform_security_context_t context,
                                                                         qeo_platform_security_set_registration_credentials_cb set_reg_cred_cb,
                                                                         qeo_platform_security_registration_credentials_cancelled_cb set_cancelled_cb)
{

    if (_cbs.on_reg_params_needed == NULL){
        return QEO_UTIL_EFAIL;
    }

    _set_reg_cred_cb = set_reg_cred_cb;
    _set_cancelled_cb = set_cancelled_cb;

    return _cbs.on_reg_params_needed(_app_ctx, context);

}

void qeo_platform_security_update_state(qeo_platform_security_context_t context, qeo_platform_security_state state, qeo_platform_security_state_reason state_reason)
{
    if (_cbs.on_sec_update == NULL){
        return;
    }

    return _cbs.on_sec_update(_app_ctx, context, state, state_reason);
}

qeo_util_retcode_t qeo_platform_security_remote_registration_confirmation_needed(qeo_platform_security_context_t context,
                                                                                 const qeo_platform_security_remote_registration_credentials_t *rrcred,
                                                                                 qeo_platform_security_remote_registration_credentials_feedback_cb feedback_cb){

    if (_cbs.on_rr_confirmation_needed == NULL){
        return QEO_UTIL_EFAIL;
    }

    _feedback_cb = feedback_cb;

    return _cbs.on_rr_confirmation_needed(_app_ctx, context, rrcred);
}

qeo_util_retcode_t qeo_platform_confirm_remote_registration_credentials(qeo_platform_security_context_t sec_context, bool confirmation){

    if (sec_context == 0){
        return QEO_UTIL_EINVAL;
    }

    if (_feedback_cb == NULL){
        return QEO_UTIL_EBADSTATE;
    }

    return _feedback_cb(sec_context, confirmation);

}

qeo_util_retcode_t qeo_platform_set_device_storage_path(const char* path){

    _device_storage_path = path;

    return QEO_UTIL_OK;
}

qeo_util_retcode_t qeo_platform_set_cacert_path(const char* ca_file,
                                                const char* ca_path)
{
    _ca_file = ca_file;
    _ca_path = ca_path;
    return QEO_UTIL_OK;
}

qeo_util_retcode_t qeo_platform_set_device_info(const qeo_platform_device_info *device_info){

    _device_info = device_info;

    return QEO_UTIL_OK;
}

const qeo_platform_device_info *qeo_platform_get_device_info(void)
{
    return _device_info;
}

qeo_util_retcode_t qeo_platform_get_device_storage_path(const char* file_name, char** full_storage_path)
{
    qeo_util_retcode_t rc = QEO_UTIL_OK;

    if (file_name == NULL || full_storage_path == NULL ) {
        qeo_log_e("Called with NULL pointer argument");
        return QEO_UTIL_EINVAL;
    }

    do {
        if (_device_storage_path == NULL){
            qeo_log_e("Device storage path not set");
            break;
        }

        if ((asprintf(full_storage_path, "%s/%s", _device_storage_path, file_name) < 0)
                || (*full_storage_path == NULL )) {
            qeo_log_e("Failed to allocate memory for the full_storage_path string !");
            rc = QEO_UTIL_ENOMEM;
            break;
        }
    } while (0);

    return rc;
}

qeo_util_retcode_t qeo_platform_get_cacert_path(const char** ca_file,
                                                const char** ca_path)
{
    qeo_util_retcode_t rc = QEO_UTIL_EINVAL;

    if ((ca_file == NULL) && (ca_path == NULL)) {
        qeo_log_e("Called with NULL pointer argument");
    }
    else {
        if (NULL != ca_file) {
            *ca_file = _ca_file;
        }
        if (NULL != ca_path) {
            *ca_path = _ca_path;
        }
        rc = QEO_UTIL_OK;
    }
    return rc;
}

qeo_util_retcode_t qeo_platform_set_key_value(const char *key, char *value)
{
    qeo_util_retcode_t ret = QEO_UTIL_EFAIL;
    FILE *fp = NULL;
    char *storage_path = NULL;

    do {

        if(key == NULL) {
            ret = QEO_UTIL_EINVAL;
            break;
        }

        if(value == NULL) {
            ret = QEO_UTIL_EINVAL;
            break;
        }

        //Don't care about key there will only be one entry in the config file
        ret = qeo_platform_get_device_storage_path(key,&storage_path);
        if(ret != QEO_UTIL_OK) {
            qeo_log_e("qeo_platform_get_device_storage_path");
            break;
        }
        fp = fopen(storage_path, "w");
        
        if (fp == NULL){
            qeo_log_e("Could not open %s for writing", storage_path);
            break;
        }
        fprintf(fp,"%s",value);


        ret = QEO_UTIL_OK;

    }while(0);

    free(storage_path);

    if(fp != NULL) {
        fclose(fp);
    }

    return ret;
} 

qeo_util_retcode_t qeo_platform_get_key_value(const char *key, char **value)
{
    qeo_util_retcode_t ret = QEO_UTIL_EFAIL;
    FILE *fp = NULL;
    char *storage_path = NULL;
    char line[LINE_MAX]={};

    do {
        if(key == NULL) {
            ret = QEO_UTIL_EINVAL;
            break;
        }

        if(value == NULL) {
            ret = QEO_UTIL_EINVAL;
            break;
        }
        *value = NULL;

        //Don't care about key there will only be one entry in the config file
        ret = qeo_platform_get_device_storage_path(key,&storage_path);
        if(ret != QEO_UTIL_OK) {
            qeo_log_e("qeo_platform_get_device_storage_path");
            break;
        }
        fp = fopen(storage_path, "r");
        if (fp == NULL){
            qeo_log_w("Could not open %s for reading", storage_path);
            ret = QEO_UTIL_ENODATA;
            break;
        }

        if(fgets(line, LINE_MAX, fp) == NULL) {
            qeo_log_e("empty file");
            break;
        }
        
        *value=strdup(line);
        if (*value == NULL){
            ret = QEO_UTIL_ENOMEM;
            break;
        }
        qeo_log_i("value:%s", *value);
        ret = QEO_UTIL_OK;

    }while(0);

    free(storage_path);

    if(fp != NULL) {
        fclose(fp);
    }

    return ret;
} 

static qeo_platform_custom_certificate_validator validator = NULL;

qeo_util_retcode_t qeo_platform_set_custom_certificate_validator(qeo_platform_custom_certificate_validator validator_function) {
    if (validator) {
        return QEO_UTIL_EBADSTATE;
    }
    qeo_log_i("Setting custom certificate validator %p", validator_function);
    validator = validator_function;
    return QEO_UTIL_OK;
}

qeo_platform_custom_certificate_validator qeo_platform_get_custom_certificate_validator() {
    return validator;
}

