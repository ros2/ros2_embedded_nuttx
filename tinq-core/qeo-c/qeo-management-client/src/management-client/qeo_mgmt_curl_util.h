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

#ifndef QEO_MGMT_CURL_UTIL_H_
#define QEO_MGMT_CURL_UTIL_H_

#include "qeo_mgmt_client_priv.h"
#include "curl_util.h"

#define QMGMTCLIENT_ACCEPT_HEADER_JSON "Accept: application/json"
#define QMGMTCLIENT_CONTENT_TYPE_FWD "Content-Type: application/vnd.qeo.forwarders-v1+json"
#define QMGMTCLIENT_ACCEPT_HEADER_FWD "Accept: application/vnd.qeo.forwarders-v1+json"

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_translate_rc(int curlrc);

CURLcode qeo_mgmt_curl_sslctx_cb(CURL * curl, void* sslctx, void* userdata);

/**
 * Basic function for performing a curl action. This function is callec by all other
 * perform functions under the hood and contains all configuration options that are common.
 *
 * WARNING This function does not reset the curl ctx as it is meant to be used together with other
 * CURL configuration.
 *
 * \param[in] ctx
 * \param[in] url
 * \param[out] cid buffer containing the correlation id of the curl http request if applicable
 * \result
 */
qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_perform(CURL *ctx,
                                                     const char* url,
                                                     char *cid);

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_http_get_with_cb(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     char *header,
                                                     curl_write_callback data_cb,
                                                     void *write_cookie);

/**
 * Do an http get and download the data to a newly allocated memory buffer.
 * \param[in] ctx Curl ctx to use
 * \param[in] url URL to download from
 * \param[in] header Optional HTTP header to add, if NULL don't do anything
 * \param[out] data Newly allocated memory buffer that contains the data.
 * \param[out] length The length of the resulting data.
 * \result
 */
qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_http_get(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     char *header,
                                                     char **data,
                                                     size_t *length);

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_get_with_cb(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     curl_write_callback data_cb,
                                                     void *write_cookie);

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_get(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     char **data,
                                                     size_t *length);

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_put(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     char *data,
                                                     size_t length);


CURLcode qeo_mgmt_curl_util_set_opts(curl_opt_helper *opts,
                                     int nropts,
                                     qeo_mgmt_client_ctx_t* ctx);

void qeo_mgmt_curl_util_shutdown_connections(qeo_mgmt_client_ctx_t* ctx);
void qeo_mgmt_curl_util_clean_fd_list(qeo_mgmt_client_ctx_t* ctx);

#endif /* QEO_MGMT_CURL_UTIL_H_ */
