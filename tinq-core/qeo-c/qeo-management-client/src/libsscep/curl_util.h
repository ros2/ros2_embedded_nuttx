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

#ifndef CURL_UTIL_H_
#define CURL_UTIL_H_
#include <stdint.h>
#include <curl/curl.h>

/**
 * Maximal size of a header field containing the correlation id (including null termination)
 */
#define CURL_UTIL_CORRELATION_ID_MAX_SIZE 1025

typedef struct
{
    CURLoption  option;
    void*       cookie;
} curl_opt_helper;

CURLcode curl_util_set_opts(curl_opt_helper *opts, int nropts, CURL *ctx);

/**
 * Stub function to use when not interested in the actual data
 */
size_t curl_util_stub_write_function( void *ptr, size_t size, size_t nmemb, void *userdata);

/**
 * Give more detailed logging in case an http error occured.
 * @param ctx CURL ctx to use
 * @param correlation_id
 */
void curl_util_log_http_error_description(CURL *ctx, char* correlation_id);

/**
 * Basic function for performing a curl action. This function is called by all other
 * perform functions under the hood and contains all configuration options that are common.
 *
 * WARNING This function does not reset the curl ctx as it is meant to be used together with other
 * CURL configuration.
 *
 * \param[in] ctx
 * \param[in] url
 * \param[out] cid The correlation id corresponding with this request, the called must make sure to pass a
 *                 buffer that can contain at least CURL_UTIL_CORRELATION_ID_MAX_SIZE characters before calling
 *                 this function or NULL in case we are not interested.
 * \result
 */
CURLcode curl_util_perform(CURL *ctx, const char* url, intptr_t verbose, char *cid);
#endif /* CURL_UTIL_H_ */
