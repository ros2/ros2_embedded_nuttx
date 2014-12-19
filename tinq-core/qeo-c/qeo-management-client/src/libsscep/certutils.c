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


/* Misc. cert/crl manipulation routines */

#include "sscep.h"

/* Open the inner, decrypted PKCS7 and try to write cert.  */
int retrieve_local_cert(struct scep *s, cert_cb cb, void *cookie, struct sscep_ctx *ctx, struct sscep_operation_info *op_info) {
    STACK_OF(X509)      *certs;
    X509            *cert = NULL;
    int         i;

    /* Get certs */
    certs = s->reply_p7->d.sign->cert;

    /* Find cert */
    for (i = 0; i < sk_X509_num(certs); i++) {
        cert = sk_X509_value(certs, i);
#if DEBUG == 1
        if (ctx->verbose) {
            char buffer[1024];

            qeo_log_i("found certificate with subject: '%s'",
                X509_NAME_oneline(X509_get_subject_name(cert),
                    buffer, sizeof(buffer)));
            qeo_log_i("     and issuer: %s",
                X509_NAME_oneline(X509_get_issuer_name(cert),
                    buffer, sizeof(buffer)));
            qeo_log_i("     based on request_subject: '%s'",
                X509_NAME_oneline(X509_REQ_get_subject_name(op_info->request),
                                        buffer, sizeof(buffer)));
        }
#endif
        cb(cert, cookie);
    }
    if (sk_X509_num(certs) <= 0){
        return SCEP_PKISTATUS_FAILURE;
    } else {
        return SCEP_PKISTATUS_SUCCESS;
    }
}

int retrieve_ca_ra(struct http_reply *s, cert_cb cb, void* cookie, struct sscep_ctx *ctx) {
    BIO         *bio;
    PKCS7           *p7;
    STACK_OF(X509)      *certs = NULL;
    X509            *cert = NULL;
    int         i;
    unsigned int n;
    unsigned char md[EVP_MAX_MD_SIZE];
    int ret = SCEP_PKISTATUS_FAILURE;

    /* Create read-only memory bio */
    bio = BIO_new_mem_buf(s->payload, s->bytes);
    p7 = d2i_PKCS7_bio(bio, NULL);
    if (p7 == NULL) {
        qeo_log_e("error reading PKCS#7 data");
        goto error;
    }
    /* Get certs */
    i = OBJ_obj2nid(p7->type);
    switch (i) {
        case NID_pkcs7_signed:
            certs = p7->d.sign->cert;
            break;
        default:
            qeo_log_e("wrong PKCS#7 type");
            goto error;
    }
    /* Check  */
    if (certs == NULL) {
        qeo_log_e("cannot find certificates");
        goto error;
    }
    /* Verify the chain
     * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
     */
    /* Find cert */
    for (i = 0; i < sk_X509_num(certs); i++) {
        char buffer[1024];

        memset(buffer, 0, 1024);
        cert = sk_X509_value(certs, i);

        /* Read and print certificate information */
        qeo_log_i("found certificate with subject: %s", X509_NAME_oneline(X509_get_subject_name(cert),
                                                                          buffer, sizeof(buffer)));
        qeo_log_i("     and issuer: %s", X509_NAME_oneline(X509_get_issuer_name(cert),
                                                           buffer, sizeof(buffer)));
        if (!X509_digest(cert, ctx->fp_alg, md, &n)) {
            goto error;
        }

        /* return PEM-formatted file: */
        cb(cert, cookie);
    }
    ret = SCEP_PKISTATUS_SUCCESS;
error:
    PKCS7_free(p7);
    BIO_free(bio);
    return ret;
}
