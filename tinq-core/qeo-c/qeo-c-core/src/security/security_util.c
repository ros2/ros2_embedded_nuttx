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
#include <qeo/platform.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

#include <string.h>

#include "security_util.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/
static int verify_server_cb(int ok, X509_STORE_CTX *ctx);

static int verify_certificate_chain(X509_STORE_CTX *,void *);

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/
static int verify_server_cb(int ok, X509_STORE_CTX *ctx)
{
    qeo_log_d("Verifying server, pre-verify is %s", ok ? "ok" : "not ok");
    if (!ok) {
        X509 *err_cert = X509_STORE_CTX_get_current_cert(ctx);
        int err = X509_STORE_CTX_get_error(ctx);
        int depth = X509_STORE_CTX_get_error_depth(ctx);
        char subj[256], issuer[256];

        X509_NAME_oneline(X509_get_subject_name(err_cert), subj, sizeof(subj));
        X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), issuer, sizeof(issuer));
        qeo_log_e("peer certificate verification failed: %s (@depth=%d, subject=%s, issuer=%s)",
                  X509_verify_cert_error_string(err), depth, subj, issuer);
    }
    /* no extra verification needed */
    return ok;
}

static int verify_certificate_chain(X509_STORE_CTX * x509_ctx, void * ignored) {
    qeo_platform_custom_certificate_validator custom_cert_validator_cb = qeo_platform_get_custom_certificate_validator();
    qeo_der_certificate certificate_chain[10];
    BIO* bios[10];
    int rc = 0;

    /** We need access to unchecked chain of certificates
     * No obvious API is found to get a hold of it. The API's available to get certificates
     * expect to do the verification first and only then you can get the chain.
     * As we want to do the validation ourselves, we just pull them out the struct to get
     * the untrusted chain.
     */
    STACK_OF(X509) *sk = x509_ctx->untrusted;

    if (sk) {
        //Lets check the stack.
        qeo_util_retcode_t retcode = QEO_UTIL_EFAIL;
        int certs = sk_X509_num(sk);
        int i;

        if (certs > 10) { //to many certificates;
            //there is also a limit of 10 in openssl for the maximum certificate chain length. We should not hit this; Still better safe then sorry.
            return 0;
        }
        memset(bios, 0, sizeof(BIO*) * 10);
        for (i = 0; i < certs ; i++) {
            int result;
            X509* cert = sk_X509_value(sk, i);
            //create a memory BIO
            BIO *mem = BIO_new(BIO_s_mem());
            if (NULL == mem) {
                goto out; //failed to create BIO
            }
            bios[i] = mem;
            //write to bio int i2d_X509_bio(BIO *bp, X509 *x);
            result = i2d_X509_bio(mem, cert);

            if (result < 0) {
                qeo_log_e("Failed to write certificate data to mem bio %d\n", result);
                goto out;
            }
            // add to array
            certificate_chain[i].size = BIO_get_mem_data(mem, &certificate_chain[i].cert_data);
        }
        //call the callback
        retcode = custom_cert_validator_cb(certificate_chain, certs);
        if (retcode == QEO_UTIL_OK) {
            rc = 1;
        } else {
            qeo_log_e("Custom certificate verification callback returned %d - Treating this as a verification error\n", retcode);
        }
out:
        //free memory
        for (i = 0; i < certs ; i++) {
            if (bios[i])
               BIO_vfree(bios[i]); //we take the void version; not much we can do if the free fails
        }
    }
    return rc;
}

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/
void security_util_configure_ssl_ctx(SSL_CTX *ssl_ctx)
{
    const char *cafile = NULL;
    const char *capath = NULL;
    qeo_platform_custom_certificate_validator custom_cert_validator_cb;

    SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2);
    custom_cert_validator_cb = qeo_platform_get_custom_certificate_validator();

    if (NULL != custom_cert_validator_cb) {
        SSL_CTX_set_cert_verify_callback(ssl_ctx, &verify_certificate_chain, NULL);
    } else {
        qeo_platform_get_cacert_path(&cafile, &capath);
        if ((NULL != cafile) || (NULL != capath)) {
             SSL_CTX_load_verify_locations(ssl_ctx, cafile, capath);
        }
    }
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_server_cb);
}

void dump_openssl_error_stack(const char *msg)
{
    unsigned long err;
    const char    *file, *data;
    int           line, flags;
    char          buf[256];

    qeo_log_e("%s", msg);
    while ((err = ERR_get_error_line_data(&file, &line, &data, &flags))) {
        qeo_log_e("err %lu @ %s:%d -- %s\n", err, file, line, ERR_error_string(err, buf));
    }

    ERR_clear_error();
}

