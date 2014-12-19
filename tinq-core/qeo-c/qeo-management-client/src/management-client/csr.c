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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include "csr.h"

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

/*#######################################################################
 #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

X509_REQ* csr_create(EVP_PKEY *key, const char* otc, const qeo_platform_device_info *info)
{
    X509_REQ *req = X509_REQ_new();
    X509_REQ *ret = NULL;
    X509_NAME *name = NULL;

    do {
        if ((req == NULL )|| (otc == NULL) || (info == NULL)){
            break;
        }

        if (!X509_REQ_set_pubkey(req, key)) {
            break;
        }

        name = cnhelper_create_dn(info);
        if (!name) {
            break;
        }

        if (!X509_REQ_set_subject_name(req, name)) {
            break;
        }

        /* When we add, we do not free */
        if (!X509_REQ_add1_attr_by_NID(req, NID_pkcs9_challengePassword, MBSTRING_ASC, (unsigned char*) otc, -1)) {
            break;
        }

        if (!X509_REQ_sign(req, key, EVP_sha256())) {
            break;
        }

        ret = req;
        req = NULL;
    } while (0);

    X509_REQ_free(req);
    X509_NAME_free(name);
    return ret;
}

