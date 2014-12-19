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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <qeo/log.h>
#include "curl_util.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/
#define HTTP_MAX_REDIRECTS 20
#define HTTP_HEADER_QEO_COR_ID "X-qeo-correlation"
#define HTTP_HEADER_QEO_COR_ID_UNKNOWN "X-qeo-correlation: Unknown"

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/
static char* _get_time_string(void){
    time_t now = time(NULL);
    char* timestring = NULL;
    if (now != -1){
        timestring = ctime(&now);
        if (timestring != NULL){/* ctime adds a newline, remove it */
            char* p = strchr(timestring,'\n');
            if (p != NULL)
            {
                *p = '\0';
            }
        }
    }
    return timestring == NULL ? "?" : timestring;
}

size_t _header_function( void *ptr, size_t size, size_t nmemb, char *cid){
    size_t ret = size * nmemb;
    char *p = NULL;
    char buf[CURL_UTIL_CORRELATION_ID_MAX_SIZE];

    do {
        if ((ret >= CURL_UTIL_CORRELATION_ID_MAX_SIZE) || (cid == NULL) || (ptr == NULL) || (ret == 0)){
            /* Prevent unneeded memory use */
            break;
        }
        memcpy(buf, ptr, ret);
        buf[ret] = '\0';
        p = strstr(buf, HTTP_HEADER_QEO_COR_ID);
        if (p != NULL ) {
            strncpy(cid, p, CURL_UTIL_CORRELATION_ID_MAX_SIZE - 1);
            cid[CURL_UTIL_CORRELATION_ID_MAX_SIZE - 1] = '\0';
            p=strchr(cid, '\r');
            if (p != NULL){
                *p='\0';
            }
            p=strchr(cid, '\n');
            if (p != NULL){
                *p='\0';
            }
            qeo_log_d("Found correlation id <%s>", cid);
        }
    } while (0);
    return ret;
}

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

size_t curl_util_stub_write_function( void *ptr, size_t size, size_t nmemb, void *userdata){
    return size*nmemb;
}

CURLcode curl_util_set_opts(curl_opt_helper *opts, int nropts, CURL *ctx){
    CURLcode ret = CURLE_OK;
    int i = 0;
    for (; i < nropts; i++) {
        ret = curl_easy_setopt(ctx, opts[i].option, opts[i].cookie);
        if (ret != CURLE_OK){
            qeo_log_e("Curl initialization failed (option %d): ", opts[i].option, curl_easy_strerror(ret));
            break;
        }
    }
    return ret;
}

void curl_util_log_http_error_description(CURL *ctx, char* correlation_id){
    long http_status = 0;

    do {
        if (correlation_id == NULL){
            correlation_id = HTTP_HEADER_QEO_COR_ID_UNKNOWN;
        }
        if (curl_easy_getinfo(ctx, CURLINFO_RESPONSE_CODE, &http_status) == CURLE_OK) {
            qeo_log_w("<HTTP status code: %d> <%s>", http_status, correlation_id);
            break;
        }
    } while (0);
}

CURLcode curl_util_perform(CURL *ctx, const char* url, intptr_t verbose, char *cid){
    CURLcode cret = CURLE_OK;
    bool free_cid = (cid == NULL);
    char *correlationid = (free_cid == true) ? (char*) malloc(CURL_UTIL_CORRELATION_ID_MAX_SIZE) : cid;
    curl_opt_helper opts[] = {
        { CURLOPT_URL, (void*)url },
        { CURLOPT_VERBOSE, (void*)verbose },
        { CURLOPT_STDERR, (void*)stderr },
        { CURLOPT_MAXREDIRS, (void*) HTTP_MAX_REDIRECTS},
        { CURLOPT_FOLLOWLOCATION, (void*) 1},
        { CURLOPT_HEADERFUNCTION, (void*) _header_function},
        { CURLOPT_WRITEHEADER, (void*) correlationid}};

    do {
        if (correlationid != NULL) {
            correlationid[0]='\0';
        }

        cret = curl_util_set_opts(opts, sizeof(opts) / sizeof(curl_opt_helper), ctx);
        if (cret != CURLE_OK) {
            break;
        }

        qeo_log_i("Start curl_easy_perform at <%s>", url);
        cret = curl_easy_perform(ctx);

        if (cret != CURLE_OK) {
            qeo_log_w("%s: Contacting <%s> returned <%s>", _get_time_string(), url, curl_easy_strerror(cret));
            if (cret == CURLE_HTTP_RETURNED_ERROR) {
                curl_util_log_http_error_description(ctx, correlationid);
            }
            break;
        }
    } while (0);
    if (free_cid)
        free(correlationid);
    return cret;
}


