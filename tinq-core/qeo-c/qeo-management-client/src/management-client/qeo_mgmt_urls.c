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
#include <qeo/log.h>
#include "qeo_mgmt_curl_util.h"
#include "qeo_mgmt_json_util.h"
#include "qeo_mgmt_urls.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/
#define ROOT_RESOURCE_URL_MAX_DEPTH 3
#define ROOT_RESOURCE_URL_TAG "href"

struct url_describer
{
    qeo_mgmt_url_type_t type;
    const char *service;
    const char *entrypoint;
} url_describers[] = {
    {QMGMT_URL_ENROLL_DEVICE, "PKI", "scep"},
    {QMGMT_URL_REGISTER_FORWARDER, "location", "forwarders"},
    {QMGMT_URL_CHECK_POLICY, "policy", "check"},
    {QMGMT_URL_GET_POLICY, "policy", "pull"}
};


struct qeo_mgmt_url_ctx_s{
    CURL *curl_ctx;
    bool init;
    qeo_mgmt_client_json_value_t values[sizeof(url_describers)/sizeof(url_describers[0])];
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

static const char* get_url(qeo_mgmt_url_ctx_t ctx, qeo_mgmt_url_type_t type){
    int i = 0;

    for (i = 0; i < sizeof(url_describers)/sizeof(url_describers[0]); ++i) {
        if (type == url_describers[i].type){
            if (ctx->values[i].deprecated == true){
                qeo_log_w("You are using a deprecated service of the public server, it might no longer be available in the future.");
            }
            return ctx->values[i].value;
        }
    }
    return NULL;
}

qeo_mgmt_client_retcode_t parse_root_resource(qeo_mgmt_url_ctx_t ctx, char* message, size_t length){
    int i = 0;
    qeo_mgmt_client_retcode_t rc = QMGMTCLIENT_EFAIL;

    do {
        qeo_mgmt_json_hdl_t hdl = qeo_mgmt_json_util_parse_message(message, length);
        if (hdl == NULL){
            break;
        }
        for (i = 0; i < sizeof(url_describers) / sizeof(url_describers[0]); ++i) {
            const char *query[ROOT_RESOURCE_URL_MAX_DEPTH + 1] = { 0 };
            size_t j = 0;
            if (url_describers[i].service != NULL ) {

                query[j] = url_describers[i].service;
                j++;
                if (url_describers[i].entrypoint != NULL ) {
                    query[j] = url_describers[i].entrypoint;
                    j++;
                }
            }
            query[j++] = ROOT_RESOURCE_URL_TAG;
            rc = qeo_mgmt_json_util_get_string(hdl, query, j, &ctx->values[i]);
            if (rc != QMGMTCLIENT_OK){
                qeo_log_w("Failed to retrieve url from %s->%s", (query[0] == NULL) ? "":query[0], (query[1] == NULL) ? "":query[1]);
                /* Be liberal, just continue */
                rc = QMGMTCLIENT_OK;
            }
        }
        qeo_mgmt_json_util_release_handle(hdl);
    }while(0);

    return rc;

}

static qeo_mgmt_client_retcode_t retrieve_url_list(qeo_mgmt_client_ctx_t* ctx,
                                                   qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                   void *ssl_cookie,
                                                   const char* base_url){
    qeo_mgmt_client_retcode_t rc = QMGMTCLIENT_EFAIL;
    char* data = NULL;
    size_t length = 0;

    do {
        rc = qeo_mgmt_curl_util_https_get(ctx, base_url, QMGMTCLIENT_ACCEPT_HEADER_JSON, ssl_cb, ssl_cookie, &data, &length);
        if (rc != QMGMTCLIENT_OK){
            break;
        }

        rc = parse_root_resource(ctx->url_ctx, data, length);
        if (rc != QMGMTCLIENT_OK){
            qeo_log_w("Failed to parse root resource.");
            break;
        }

    } while(0);

    free(data);
    return rc;
}

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/


qeo_mgmt_client_retcode_t qeo_mgmt_url_get(qeo_mgmt_client_ctx_t* mg_ctx, qeo_mgmt_client_ssl_ctx_cb ssl_cb, void *ssl_cookie, const char* base_url, qeo_mgmt_url_type_t service, const char** url){
    qeo_mgmt_client_retcode_t rc = QMGMTCLIENT_EINVAL;

    do {
        qeo_mgmt_url_ctx_t ctx;
        if ((url == NULL) || (base_url == NULL) || (mg_ctx == NULL) || (mg_ctx->url_ctx == NULL) || (mg_ctx->url_ctx->curl_ctx == NULL)){
            break;
        }
        ctx = mg_ctx->url_ctx;
        if (!ctx->init){
            rc = retrieve_url_list(mg_ctx, ssl_cb, ssl_cookie, base_url);
            if (rc != QMGMTCLIENT_OK){
                break;
            }
            ctx->init = true;
        }
        *url = get_url(ctx, service);
        if (*url == NULL){
            rc = QMGMTCLIENT_EBADSERVICE;
            qeo_log_w("URL for specified service could not be found.");
            break;
        }
        rc = QMGMTCLIENT_OK;
    } while(0);

    return rc;
}

qeo_mgmt_url_ctx_t qeo_mgmt_url_init(CURL *cctx){
    qeo_mgmt_url_ctx_t ctx = NULL;

    do {
        if (cctx == NULL){
            break;
        }
        ctx = calloc(1, sizeof(struct qeo_mgmt_url_ctx_s));
        if (!ctx)
            break;

        ctx->curl_ctx = cctx;
    } while (0);

    return ctx;
}

void qeo_mgmt_url_cleanup(qeo_mgmt_url_ctx_t ctx){
    int i = 0;

    if (ctx != NULL){
        for (i = 0; i < sizeof(ctx->values)/sizeof(ctx->values[0]); ++i) {
            free(ctx->values[i].value);
        }
        free(ctx);
    }
}

