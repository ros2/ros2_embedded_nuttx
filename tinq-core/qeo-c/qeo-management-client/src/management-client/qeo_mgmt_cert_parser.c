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

#include <stdbool.h>
#include <qeo/log.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <qeo/mgmt_cert_parser.h>
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

/*#######################################################################
 #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

char* find_next_token(char* ptr)
{
    if (ptr == NULL) return ptr;
    ptr = strchr(ptr, ' ');
    if (ptr != NULL ) {
        while (isspace(*ptr)) {
            // Trim leading spaces
            ptr++;
        }
    }
    return ptr;
}

/*
 * This function is deliberately made non static to allow it to be used from within tests.
 *
 * Example common name of device:
 * CN=<realmid> <device id> <user id>
 */
bool get_ids_from_common_name(char *cn, int64_t *realm_id, int64_t *device_id, int64_t *user_id)
{
    char *ptr = cn;
    int64_t id = -1;

    *realm_id = -1;
    *device_id = -1;
    *user_id = -1;

    while (isspace(*ptr)) {
        // Trim leading spaces
        ptr++;
    }

    id = qeo_mgmt_util_hex_to_int(ptr);
    if (id == -1) {
        return false;
    }
    *realm_id = id;

    /* It can still be a device or policy certificate, go forward */
    ptr = find_next_token(ptr);
    id = qeo_mgmt_util_hex_to_int(ptr);
    if (id == -1) {
        return false;
    }
    *device_id = id;

    ptr = find_next_token(ptr);
    id = qeo_mgmt_util_hex_to_int(ptr);
    if (id == -1) {
        return false;
    }
    *user_id = id;

    ptr = find_next_token(ptr);
    if ((ptr == NULL) || (*ptr == '\0'))
        return true;
    return false;
}

static char *get_common_name_from_dn(X509_NAME *dn, char *buf, ssize_t len)
{
    /* TODO WARNING: THIS FUNCTION CAN ONLY BE USED IF YOU ARE SURE THE COMMON NAME IS A PLAIN ASCII STRING - NO FANCY BMPSTRING OR UTF8STRING */
    if (X509_NAME_get_text_by_NID(dn, NID_commonName, NULL, 0) > len) {
        qeo_log_w("Certificate common name is too long");
        return NULL ;
    }
    if (X509_NAME_get_text_by_NID(dn, NID_commonName, buf, len) <= 0) {
        qeo_log_w("Could not copy common name from subject");
        return NULL ;
    }

    return buf;
}

static char *get_common_name_from_certificate(X509 *cert, char *buf, ssize_t len)
{
    X509_NAME *cert_subject = NULL;

    cert_subject = X509_get_subject_name(cert);
    if (cert_subject == NULL ) {
        qeo_log_w("Certificate subject was not set");
        return NULL ;
    }

    return get_common_name_from_dn(cert_subject, buf, len);
}


/*#######################################################################
 #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

qeo_mgmt_cert_retcode_t qeo_mgmt_cert_parse(STACK_OF(X509) *chain, qeo_mgmt_cert_contents *contents)
{
    char cert_common_name[65]; /* According to rfc3280, the max length of a common name is 64 (+ 0 termination) */

    do {
        if ((chain == NULL )|| (contents == NULL))break;

        /* First order the list from leave to root */
        if (!qeo_mgmt_cert_util_order_chain(chain)){
            break;
        }

        /* Now fetch the info from the last certificate */
        if (get_common_name_from_certificate(sk_X509_value(chain, 0), cert_common_name,
                        sizeof(cert_common_name) - 1) == NULL ) {
            break;
        }

        if (!get_ids_from_common_name(cert_common_name, &(contents->realm), &(contents->device), &(contents->user))) {
            break;
        }
        return QCERT_OK;
    } while (0);
    return QCERT_EFAIL;
}

