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

#ifndef QEO_MGMT_URLS_H_
#define QEO_MGMT_URLS_H_
#include <curl/curl.h>
#include "qeo/mgmt_client.h"

/**
 * Enumeration of the urls that are supported.
 */
typedef enum
{
    QMGMT_URL_ENROLL_DEVICE,      /**< Get the url for device enrollment. */
    QMGMT_URL_CHECK_POLICY,       /**< Get the url for policy checking. */
    QMGMT_URL_GET_POLICY,         /**< Get the url for policy retrieval. */
    QMGMT_URL_REGISTER_FORWARDER, /**< Get the url for registering a forwarder. */
} qeo_mgmt_url_type_t;

/**
 * Opaque handle representing a qeo managment url handling context containing
 * all the information needed to handle actions.
 */
typedef struct qeo_mgmt_url_ctx_s *qeo_mgmt_url_ctx_t;

/**
 * Get an url for a specific type and using a specific base_url.
 *
 * The first time this function is called, a request is sent to the management server
 * to fetch the list of urls. This list is cached internally in the context.
 *
 * Subsequent calls will first check if the base_url stored in the ctx matches the one
 * passed to this function. If so, the correct url is returned. If not, the list is rebuild.
 *
 * \param[in] ctx The context to store cached data.
 * \param[in] ssl_cb the callback used to handle ssl connections
 * \param[in] ssl_cookie the cookie to hand to the ssl callback when called.
 * \param[in] base_url The base_url to start from.
 * \param[in] service The service for which to retrieve the url.
 * \param[out] url The resulting url, do not store this url as it's memory is freed when url_cleanup is called.
 * \retval ::QMGMTCLIENT_OK in case of success.
 * \retval ::QMGMTCLIENT_ECONNECT in case the list could not be retrieved.
 * \retval ::QMGMTCLIENT_EFAIL in case the url of that type was not found.
 *
 */
qeo_mgmt_client_retcode_t qeo_mgmt_url_get(qeo_mgmt_client_ctx_t* ctx, qeo_mgmt_client_ssl_ctx_cb ssl_cb, void *ssl_cookie, const char* base_url, qeo_mgmt_url_type_t service, const char** url);

/**
 * Return a handle to url context that should be used inside this api.
 * \param[in] cctx Curl ctx to use when retrieving the url list.
 */
qeo_mgmt_url_ctx_t qeo_mgmt_url_init(CURL *cctx);

/**
 * Free all memory cached internally for url discovery.
 * \param[in] ctx
 */
void qeo_mgmt_url_cleanup(qeo_mgmt_url_ctx_t ctx);

#endif /* QEO_MGMT_URLS_H_ */
