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
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/
#include <qeo/log.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include "qeo_mgmt_cert_util.h"


/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/
const char* HEX_NUMBER_CHARS = "ABCDEFabcdef0123456789";

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

static bool is_valid_hex_number(const char* hex){
    bool numberfound = false;
    if (hex == NULL){
        return false;
    }
    while(((*hex) != '\0') && (!isspace(*hex))){
        if (strchr(HEX_NUMBER_CHARS, *hex) == NULL){
            return false;/* Found a non number char */
        }
        numberfound = true;
        hex++;
    }
    return numberfound;
}
/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

int64_t qeo_mgmt_util_hex_to_int(const char* hex)
{
    u_int64_t id;
    int ret = EOF;
    do {
        if (is_valid_hex_number(hex) == false){
            break;
        }
        errno = 0;
        ret = sscanf(hex, "%" PRIx64 " ", &id);
        if ((errno != 0) || (ret <= 0)) {
            break;
        }
        return (int64_t)id;
    } while (0);
    return -1;
}

bool qeo_mgmt_cert_util_order_chain(STACK_OF(X509) *chain)
{
    X509 *cert = NULL;
    int i = 0;
    STACK_OF(X509) *tmpchain = sk_X509_new(NULL);
    bool update = false;
    X509_NAME *x509name = NULL;
    bool ret = false;

    do {
        while (sk_X509_num(chain) > 0) {
            update = false;
            for (i = 0; i < sk_X509_num(chain); ++i) {
                cert = sk_X509_value(chain, i);
                if (X509_NAME_cmp(X509_get_issuer_name(cert),
                                  (x509name == NULL )? X509_get_subject_name(cert) : x509name)== 0) {
                                      sk_X509_push(tmpchain, cert);
                                      x509name = X509_get_subject_name(cert);
                                      /* Remove from the original chain */
                                      (void) sk_X509_delete(chain, i);
                                      update = true;
                                      break;
                                  }
                              }
            if (!update) {
                break;
            }
        }
        if (!update) {
            break;
        }

        /* Repopulate the original chain in the correct order */
        while (sk_X509_num(tmpchain) > 0) {
            sk_X509_push(chain, sk_X509_pop(tmpchain));
        }
        ret = true;
    } while (0);

    sk_X509_free(tmpchain);
    if (!ret){
        qeo_log_w("Failed to order list of certificates to a valid chain.");
    }
    return ret;
}
