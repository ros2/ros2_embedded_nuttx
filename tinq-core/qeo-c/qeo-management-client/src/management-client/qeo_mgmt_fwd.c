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
#include <string.h>
#include <errno.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <openssl/err.h>
#include <qeo/mgmt_client_forwarder.h>
#include <qeo/log.h>
#include "qeo_mgmt_client_priv.h"
#include "curl_util.h"
#include "qeo_mgmt_json_util.h"
#include "qeo_mgmt_curl_util.h"

/*#######################################################################
 #                       TYPES SECTION                                   #
 ########################################################################*/

struct _run_args
{
    qeo_mgmt_client_ctx_t *ctx;
    char* url;
    qeo_mgmt_client_forwarder_cb fwd_cb;
    qeo_mgmt_client_forwarder_result_cb result_cb;
    void *fwd_cookie;
    qeo_mgmt_client_ssl_ctx_cb ssl_cb;
    void *ssl_cookie;
};

/*#######################################################################
 #                   STATIC FUNCTION DECLARATION                         #
 ########################################################################*/

/*#######################################################################
 #                       STATIC VARIABLE SECTION                         #
 ########################################################################*/

/*#######################################################################
 #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

static void* _run(void* run_arg)
{
    struct _run_args* args = (struct _run_args*)run_arg;

    qeo_log_i("qeo_mgmt_client_get_forwarders_async thread");
    do {
        if (args == NULL) {
            break;
        }
        qeo_mgmt_client_retcode_t ret = qeo_mgmt_client_get_forwarders_sync(args->ctx, args->url, args->fwd_cb,
                                                                            args->fwd_cookie, args->ssl_cb,
                                                                            args->ssl_cookie);
        if (args->result_cb != NULL) {
            args->result_cb(ret, args->fwd_cookie);
        }
        free(args->url);
        free(args);
    } while(0);
    //http://www.openssl.org/support/faq.html#PROG13
    //To prevent a memory leak because openssl keeps thread local storage
    ERR_remove_thread_state(NULL);
    return NULL;
}

/*#######################################################################
 #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

qeo_mgmt_client_retcode_t qeo_mgmt_client_register_forwarder(qeo_mgmt_client_ctx_t *ctx,
                                                             const char* base_url,
                                                             qeo_mgmt_client_locator_t *locators,
                                                             u_int32_t nrOfLocators,
                                                             qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                             void *cookie)
{
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    const char* url = NULL;
    char *message = NULL;
    size_t length = 0;
    bool locked = false;

    do {
        qeo_log_i("Register Forwarder");
        if ((ctx == NULL )|| (base_url == NULL) || (ssl_cb == NULL) || (ctx->curl_ctx == NULL)
                || ((nrOfLocators > 0) && (locators == NULL))){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        ret = qeo_mgmt_json_util_formatLocatorData(locators, nrOfLocators, &message, &length);
        if (ret != QMGMTCLIENT_OK) {
            break;
        }
        if (qmc_lock_ctx(ctx) != true){
            ret = QMGMTCLIENT_EFAIL;
            break;
        }
        locked = true;

        ret = qeo_mgmt_url_get(ctx, ssl_cb,cookie, base_url, QMGMT_URL_REGISTER_FORWARDER, &url);
        if (ret != QMGMTCLIENT_OK) {
            qeo_log_w("Failed to retrieve url from root resource.");
            break;
        }

        ret = qeo_mgmt_curl_util_https_put(ctx, url, QMGMTCLIENT_CONTENT_TYPE_FWD, ssl_cb, cookie, message, length);
        if (ret != QMGMTCLIENT_OK) {
            qeo_log_w("Failed to upload forwarder.");
            break;
        }
        qeo_log_i("Successfully uploaded forwarder info to server");
    } while (0);

    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failure registering forwarder file");
    }

    free(message);
    if (locked == true){
        qmc_unlock_ctx(ctx);
    }

    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_get_forwarders_sync(qeo_mgmt_client_ctx_t *ctx,
                                                              const char* base_url,
                                                              qeo_mgmt_client_forwarder_cb fwd_cb,
                                                              void *fwd_cookie,
                                                              qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                              void *ssl_cookie)
{

    qeo_mgmt_client_retcode_t ret;
    char *data = NULL;
    size_t length = 0;
    const char *url = NULL;
    bool locked = false;
    do {
        qeo_log_i("Get Forwarders");
        if ((ctx == NULL )|| (base_url == NULL) || (ssl_cb == NULL) || (ctx->curl_ctx == NULL) || (fwd_cb == NULL)){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        if (qmc_lock_ctx(ctx) != true){
            ret = QMGMTCLIENT_EFAIL;
            break;
        }
        locked = true;

        ret = qeo_mgmt_url_get(ctx, ssl_cb, ssl_cookie, base_url, QMGMT_URL_REGISTER_FORWARDER, &url);
        if (ret != QMGMTCLIENT_OK) {
            //qeo_log_w("Failed to retrieve url from root resource.");
            break;
        }
        ret = qeo_mgmt_curl_util_https_get(ctx, url, QMGMTCLIENT_ACCEPT_HEADER_FWD, ssl_cb, ssl_cookie, &data, &length);
        if (ret != QMGMTCLIENT_OK) {
            break;
        }

        /* Unlock the mutex to allow the fwd_cb to make qmc api calls. */
        qmc_unlock_ctx(ctx);
        locked = false;

        ret = qeo_mgmt_json_util_parseGetFWDMessage(data, length, fwd_cb, fwd_cookie);
        if (ret != QMGMTCLIENT_OK) {
            qeo_log_w("Failed to parse forwarder list.");
            break;
        }
        qeo_log_i("Successfully retrieved forwarder list");

    } while (0);

    free(data);
    if (locked == true){
        qmc_unlock_ctx(ctx);
    }

    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_get_forwarders(qeo_mgmt_client_ctx_t *ctx,
                                                         const char* url,
                                                         qeo_mgmt_client_forwarder_cb fwd_cb,
                                                         qeo_mgmt_client_forwarder_result_cb result_cb,
                                                         void *fwd_cookie,
                                                         qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                         void *ssl_cookie)
{
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    struct _run_args* args = NULL;
    bool locked = false;

    do {
        if ((ctx == NULL )|| (url == NULL) || (ssl_cb == NULL) || (ctx->curl_ctx == NULL) || (fwd_cb == NULL)){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }

        args = calloc(1, sizeof(struct _run_args));
        if (args == NULL ) {
            ret = QMGMTCLIENT_EMEM;
            break;
        }
        /* There is still a possible race condition here, it can be that the thread is not yet started
         * but
         */
        qmc_clean_ongoing_thread(ctx);

        if (qmc_lock_ctx(ctx) != true){
            ret = QMGMTCLIENT_EFAIL;
            break;
        }
        locked = true;

        args->ctx = ctx;
        args->url = strdup(url);
        args->fwd_cb = fwd_cb;
        args->result_cb = result_cb;
        args->fwd_cookie = fwd_cookie;
        args->ssl_cb = ssl_cb;
        args->ssl_cookie = ssl_cookie;

        if (0 != pthread_create(&ctx->worker_thread, NULL, &_run, args)){
            ret = QMGMTCLIENT_EMEM;
            break;
        }
        ctx->join_thread = true;
        ret = QMGMTCLIENT_OK;
        args = NULL;/*Make sure it isn't freed anymore. */

    } while (0);
    if (args != NULL){
        free(args->url);
        free(args);
    }
    if (locked == true){
        qmc_unlock_ctx(ctx);
    }

    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_free_forwarder(qeo_mgmt_client_forwarder_t* forwarder)
{
    u_int32_t i;

    qeo_log_i("qeo_mgmt_client_free_forwarders: %p", forwarder);
    if (forwarder != NULL ) {
        if (forwarder->locators != NULL ) {
            for (i = 0; i < forwarder->nrOfLocators; i++) {
                free(forwarder->locators[i].address);
            }
            free(forwarder->locators);
        }
        free(forwarder);
    }
    return QMGMTCLIENT_OK;
}

