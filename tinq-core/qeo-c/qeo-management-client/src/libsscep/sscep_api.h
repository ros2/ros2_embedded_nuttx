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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright (c) Jarkko Turkulainen 2003. All rights reserved. 
 *   See the 'sscep License' chapter in the file COPYRIGHT for copyright notice
 *   and original licensing information.
 */


#ifndef SSCEP_API_H_
#define SSCEP_API_H_
#include <curl/curl.h>
#include <openssl/x509.h>

/* SCEP pkiStatus values (also used as SSCEP return values) */
#define SCEP_PKISTATUS_SUCCESS      0
#define SCEP_PKISTATUS_ERROR        1 /* General error */
#define SCEP_PKISTATUS_FAILURE      2
#define SCEP_PKISTATUS_PENDING      3

#define SCEP_PKISTATUS_CONNECT      100
#define SCEP_PKISTATUS_SSL          101
#define SCEP_PKISTATUS_FORBIDDEN    102

/* SCEP operations */
#define SCEP_OPERATION_GETCA    1
#define SCEP_OPERATION_ENROLL   3
#define SCEP_OPERATION_GETCERT  5
#define SCEP_OPERATION_GETCRL   7

typedef struct sscep_ctx *sscep_ctx_t;

typedef struct sscep_operation_info {
    int operation_mode;
    char* url;
    CURL *curl_ctx;
    char *identifier;
    X509_REQ *request;
    X509 *cacert;
    X509 *racert;
    EVP_PKEY *rsa;
} * sscep_operation_info_t;

typedef void (*cert_cb)(X509 *cert, void *cookie);

sscep_ctx_t sscep_init(int verbose, int debug);
int sscep_perform(sscep_ctx_t ctx, sscep_operation_info_t operation, cert_cb cb, void *cookie);
void sscep_cleanup(sscep_ctx_t ctx);

#endif /* SSCEP_API_H_ */
