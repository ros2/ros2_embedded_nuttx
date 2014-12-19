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
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <qeo/mgmt_client_forwarder.h>
#include <qeo/log.h>
#include "qeo_mgmt_client_priv.h"
#include "qeo_mgmt_cert_util.h"
#include "qeo_mgmt_json_util.h"
#include "qeo_mgmt_curl_util.h"
#include "sscep_api.h"
#include "csr.h"
#include "curl_util.h"

/*#######################################################################
 #                       TYPES SECTION                                   #
 ########################################################################*/
const char* PRINTABLE_OTP_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

/*
 * Wrapper structure used to bind curl data callbacks with the mgmt client callbacks.
 */
typedef struct
{
    qeo_mgmt_client_data_cb cb;
    void *cookie;
} curl_data_helper;

/*#######################################################################
 #                   STATIC FUNCTION DECLARATION                         #
 ########################################################################*/

/*#######################################################################
 #                       STATIC VARIABLE SECTION                         #
 ########################################################################*/

/*#######################################################################
 #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

static void _cert_cb(X509* cert, void* cookie)
{
    STACK_OF(X509) *certs = (STACK_OF(X509) *)cookie;

    /* Take a copy */
    sk_X509_push(certs, X509_dup(cert));
}

static bool _check_otp_validity(const char* otp)
{
    bool ret = true;

    /* testing whether the otc string does not contain any forbidden characters*/
    while ((*otp) != '\0') {
        if (strchr(PRINTABLE_OTP_CHARS, *otp) == NULL ) {
            ret = false;
            break;
        }
        otp++;
    }
    return ret;
}

static size_t _curl_data_cb(char *buffer, size_t size, size_t nitems, void *userdata)
{
    curl_data_helper *datahelper = (curl_data_helper*)userdata;
    ssize_t ret = -1;
    do {
        if (!datahelper) {
            break;
        }

        qeo_mgmt_client_retcode_t rc = datahelper->cb(buffer, size * nitems, datahelper->cookie);
        if (rc != QMGMTCLIENT_OK) {
            break;
        }
        ret = (size * nitems);
    } while (0);

    return ret;
}

static qeo_mgmt_client_retcode_t _translate_scep_rc(int sceprc)
{
    switch (sceprc) {
        case SCEP_PKISTATUS_CONNECT:
            return QMGMTCLIENT_ECONNECT;
        case SCEP_PKISTATUS_SSL:
            return QMGMTCLIENT_ESSL;
        case SCEP_PKISTATUS_FORBIDDEN:
            return QMGMTCLIENT_EOTP;
    }
    return QMGMTCLIENT_EFAIL;
}

/*#######################################################################
 #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

qeo_mgmt_client_ctx_t *qeo_mgmt_client_init(void)
{
    qeo_mgmt_client_ctx_t *ctx = (qeo_mgmt_client_ctx_t *)calloc(1, sizeof(struct qeo_mgmt_client_ctx_s));

    do {
        if (ctx == NULL ) {
            break;
        }
        /* Do not init openssl, this should be done by the user of this library.*/
        if (curl_global_init(CURL_GLOBAL_NOTHING) != CURLE_OK) {
            break;
        }
        ctx->curl_global_init = true;
        ctx->sscep_ctx = sscep_init(OP_VERBOSE, OP_DEBUG);
        if (ctx->sscep_ctx == NULL ) {
            break;
        }
        ctx->curl_ctx = curl_easy_init();
        if (ctx->curl_ctx == NULL ) {
            break;
        }
        ctx->url_ctx = qeo_mgmt_url_init(ctx->curl_ctx);
        if (ctx->url_ctx == NULL ) {
            break;
        }
        if (pthread_mutex_init(&ctx->mutex, NULL ) != 0) {
            break;
        }
        if (pthread_mutex_init(&ctx->fd_mutex, NULL ) != 0) {
            pthread_mutex_destroy(&ctx->mutex);
            break;
        }
        ctx->mutex_init = true;
        return ctx;
    } while (0);

    qeo_mgmt_client_clean(ctx);
    return NULL ;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_enroll_device(qeo_mgmt_client_ctx_t *ctx,
                                                        const char *base_url,
                                                        const EVP_PKEY *pkey,
                                                        const char *otp,
                                                        const qeo_platform_device_info *info,
                                                        qeo_mgmt_client_ssl_ctx_cb cb,
                                                        void *cookie,
                                                        STACK_OF(X509) *certs)
{

    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EINVAL;
    STACK_OF(X509) *racerts = NULL;
    char* url = NULL;
    int scepret = SCEP_PKISTATUS_FAILURE;
    curl_ssl_ctx_helper curlsslhelper = { cb, cookie };
    curl_opt_helper opts[] = {
      { CURLOPT_VERBOSE, (void*)OP_VERBOSE },
      { CURLOPT_STDERR, (void*)stderr },
      { CURLOPT_SSL_VERIFYPEER, (void*)0 },
      { CURLOPT_SSL_CTX_FUNCTION, (void*)qeo_mgmt_curl_sslctx_cb },
      { CURLOPT_SSL_CTX_DATA, (void*)&curlsslhelper },
    };
    struct sscep_operation_info *op_info = NULL;
    bool reset = false;
    bool locked = false;

    do {
        if ((ctx == NULL )|| (ctx->sscep_ctx == NULL) || (ctx->curl_ctx == NULL) || (base_url == NULL)
        || (pkey == NULL) || (otp == NULL) || (info == NULL) || (info->userFriendlyName == NULL) || (cb == NULL) || (certs == NULL)){
        ret = QMGMTCLIENT_EINVAL;
        break;
    }
    op_info = (struct sscep_operation_info *)calloc(1, sizeof(struct sscep_operation_info));
    racerts = sk_X509_new(NULL);

    if ((racerts == NULL) || (op_info == NULL)) {
        ret = QMGMTCLIENT_EMEM;
        break;
    }
    if (_check_otp_validity(otp) != true) {
        /* The otp contains forbidden chars */
        ret = QMGMTCLIENT_EOTP;
        break;
    }
    if (qmc_lock_ctx(ctx) != true) {
        ret = QMGMTCLIENT_EFAIL;
        break;
    }
    locked = true;
    ret = qeo_mgmt_url_get(ctx, cb, cookie, base_url, QMGMT_URL_ENROLL_DEVICE, (const char**)&url);
    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failed to retrieve url from root resource.");
        break;
    }

    op_info->url = url;
    op_info->curl_ctx = ctx->curl_ctx;

    reset = true;
    if (qeo_mgmt_curl_util_set_opts(opts, sizeof(opts) / sizeof(curl_opt_helper), ctx) != CURLE_OK) {
        ret = QMGMTCLIENT_EINVAL;
        break;
    }
    /* TODO: additional input validation for device_info */
    qeo_log_i("Fetching root certificate from RA");
    /* First retrieve the ca of the ra */
    op_info->operation_mode = SCEP_OPERATION_GETCA;
    op_info->identifier = "";
    scepret = sscep_perform(ctx->sscep_ctx, op_info, _cert_cb, (void*)racerts);
    if (scepret != SCEP_PKISTATUS_SUCCESS) {
        ret = _translate_scep_rc(scepret);
        break;
    }

    /*
     * There must be 2 certificates, the first one is the master certificate of the ca,
     * the second one is the master certificate of the ra.
     */
    if (sk_X509_num(racerts) != 2) {
        ret = QMGMTCLIENT_EFAIL;
        break;
    }
    op_info->cacert = sk_X509_value(racerts, 0); if (ctx->url_ctx == NULL ) {
        break;
    }

    op_info->racert = sk_X509_value(racerts, 1);

    if ((op_info->cacert == NULL) || (op_info->racert == NULL)) {
        ret = QMGMTCLIENT_EFAIL;
        break;
    }

    qeo_log_i("Enrolling device <%s>", info->userFriendlyName);
    op_info->operation_mode = SCEP_OPERATION_ENROLL;
    op_info->rsa = (EVP_PKEY*)pkey;
    op_info->request = csr_create(op_info->rsa, otp, info);
    if (op_info->request == NULL) {
        ret = QMGMTCLIENT_EINVAL;/* Most likely cause is invalid args */
        break;
    }
    scepret = sscep_perform(ctx->sscep_ctx, op_info, _cert_cb, (void*)certs);
    if (scepret != SCEP_PKISTATUS_SUCCESS) {
        ret = _translate_scep_rc(scepret);
        break;
    }

    /* Add the ca master certificate to the back off the chain */
    _cert_cb(op_info->cacert, (void*)certs);

    /* Make sure the resulting chain is valid */
    if (qeo_mgmt_cert_util_order_chain(certs) == false) {
        ret = QMGMTCLIENT_EFAIL;
        break;
    }
    ret = QMGMTCLIENT_OK;
    qeo_log_i("Successfully retrieved device certificate and chain");
} while (0);

    if (reset == true) {
        /* Make sure we reset all configuration for next calls */
        curl_easy_reset(ctx->curl_ctx);
    }

    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failure requesting device certificate");
    }
    if (op_info && op_info->request) {
        X509_REQ_free(op_info->request);
    }
    if (racerts) {
        sk_X509_pop_free(racerts, X509_free);
    }
    free(op_info);

    if (locked == true) {
        qmc_unlock_ctx(ctx);
    }
    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_get_policy(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* base_url,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     qeo_mgmt_client_data_cb data_cb,
                                                     void *cookie)
{
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    curl_data_helper curldatahelper = { data_cb, cookie };
    const char *url = NULL;
    bool locked = false;

    do {
        if ((ctx == NULL )|| (ctx->url_ctx == NULL)|| (base_url == NULL) || (ssl_cb == NULL) || (data_cb == NULL)){
        return QMGMTCLIENT_EINVAL;
    }
    if (qmc_lock_ctx(ctx) != true) {
        ret = QMGMTCLIENT_EFAIL;
        break;
    }
    locked = true;
    ret = qeo_mgmt_url_get(ctx, ssl_cb, cookie, base_url, QMGMT_URL_GET_POLICY, &url);
    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failed to retrieve url from root resource.");
        break;
    }
    ret = qeo_mgmt_curl_util_https_get_with_cb(ctx, url, NULL, ssl_cb, cookie, &_curl_data_cb, &curldatahelper);
    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failed to retrieve policy file from <%s>.", url);
        break;
    }
} while(0);

    if (locked == true) {
        qmc_unlock_ctx(ctx);
    }
    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_check_policy(qeo_mgmt_client_ctx_t *ctx,
                                                       qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                       void *ssl_cookie,
                                                       const char* base_url,
                                                       int64_t sequence_nr,
                                                       int64_t realm_id,
                                                       bool *result)
{
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    ssize_t sz = 0;
    char* cpurl = NULL;
    CURLcode cret = CURLE_OK;
    curl_opt_helper opts[] = { { CURLOPT_FAILONERROR, (void*)0 },
        { CURLOPT_WRITEFUNCTION, (void*) curl_util_stub_write_function} };
    long http_status = 0;
    const char *url = NULL;
    bool reset = false;
    bool locked = false;
    char correlation_id[CURL_UTIL_CORRELATION_ID_MAX_SIZE];

    do {
        if ((ctx == NULL )|| (base_url == NULL) || (result == NULL) || (sequence_nr < 0) || (realm_id < 0) || (ctx->curl_ctx == NULL)){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        if (qmc_lock_ctx(ctx) != true) {
            ret = QMGMTCLIENT_EFAIL;
            break;
        }
        locked = true;
        reset = true;
        ret = qeo_mgmt_url_get(ctx, ssl_cb, ssl_cookie, base_url, QMGMT_URL_CHECK_POLICY, &url);
        if (ret != QMGMTCLIENT_OK) {
            qeo_log_w("Failed to retrieve url from root resource.");
            break;
        }
        curl_easy_reset(ctx->curl_ctx);

        sz = strlen(url) + 128;/* Some extra length for containing the query and some other stuff */
        cpurl = malloc(sz);
        if (cpurl == NULL) {
            ret = QMGMTCLIENT_EMEM;
            break;
        }

        if (snprintf(cpurl, sz, "%s?realmid=%" PRIx64 "&sequencenr=%" PRId64, url, realm_id, sequence_nr) >= (sz-1)) {
            ret = QMGMTCLIENT_EINVAL;/* Should not happen */
            break;
        }

        reset = true;
        curl_easy_reset(ctx->curl_ctx);
        if (CURLE_OK != qeo_mgmt_curl_util_set_opts(opts, sizeof(opts) / sizeof(curl_opt_helper), ctx)) {
            ret = QMGMTCLIENT_EINVAL;
            break;
        }

        qeo_log_i("Checking policy based on url <%s>", cpurl);
        ret = qeo_mgmt_curl_util_perform(ctx->curl_ctx, cpurl, correlation_id);
        if (ret != QMGMTCLIENT_OK) {
            qeo_log_w("Could not check for policy file");
            break;
        }

        cret = curl_easy_getinfo(ctx->curl_ctx, CURLINFO_RESPONSE_CODE, &http_status);
        if (cret != CURLE_OK) {
            ret = qeo_mgmt_curl_util_translate_rc(cret);
            qeo_log_w("curl failed to get response code <%s>", curl_easy_strerror(cret));
            break;
        }

        qeo_log_i("Retrieved <%ld> status code from server.", http_status);
        if (http_status == 304) {
            *result=true;
        }
        else if (http_status == 410) {
            *result=false;
        }
        else {
            if (http_status == 400) {
                ret = QMGMTCLIENT_EBADREQUEST;
            }
            else {
                ret = QMGMTCLIENT_EFAIL;
            }
            curl_util_log_http_error_description(ctx->curl_ctx, correlation_id);
            break;
        }
        ret = QMGMTCLIENT_OK;
    } while(0);

    if (reset == true) {
        /* Make sure we reset all configuration for next calls */
        curl_easy_reset(ctx->curl_ctx);
    }

    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failure checking for policy file");
    }
    free(cpurl);

    if (locked == true) {
        qmc_unlock_ctx(ctx);
    }
    return ret;
}

void qeo_mgmt_client_clean(qeo_mgmt_client_ctx_t *ctx)
{
    if (ctx != NULL ) {
        qmc_clean_ongoing_thread(ctx);
        if (ctx->curl_ctx != NULL ) {
            curl_easy_cleanup(ctx->curl_ctx);
        }
        sscep_cleanup(ctx->sscep_ctx);
        qeo_mgmt_url_cleanup(ctx->url_ctx);
        if (ctx->curl_global_init) {
            curl_global_cleanup();
        }
        qeo_mgmt_curl_util_clean_fd_list(ctx);
        if (ctx->mutex_init) {
            ctx->mutex_init = false;
            pthread_mutex_destroy(&ctx->mutex);
            pthread_mutex_destroy(&ctx->fd_mutex);
        }
        free(ctx);
    }
}

void qeo_mgmt_client_ctx_stop(qeo_mgmt_client_ctx_t *ctx) {
    qeo_mgmt_curl_util_shutdown_connections(ctx);
}

