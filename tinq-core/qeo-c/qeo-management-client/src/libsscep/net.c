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

/* HTTP routine */

#include "sscep.h"
#include "curl_util.h"

#include <stdbool.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

ssize_t scep_receive_data(void *buffer, ssize_t size, ssize_t nmemb, void *userp)
{
    ssize_t realsize = size * nmemb;
    struct http_reply *reply = (struct http_reply *)userp;

    reply->payload = realloc(reply->payload, reply->bytes + realsize + 1);
    if (reply->payload == NULL ) {
        qeo_log_i("Not enough memory for HTTP data. Aborting");
        return 0;
    }

    memcpy(&(reply->payload[reply->bytes]), buffer, realsize);
    reply->bytes += realsize;
    reply->payload[reply->bytes] = '\0';
    return realsize;
}

int send_msg(struct http_reply *http, char *url, int operation, CURL *cctx, int verbose)
{
    CURLcode cret = CURLE_OK;
    char *content_type = NULL;
    int ret = SCEP_PKISTATUS_FAILURE;
    bool log_http_error = false;
    char correlation_id[CURL_UTIL_CORRELATION_ID_MAX_SIZE];
    curl_opt_helper opts[] = {
        {CURLOPT_WRITEFUNCTION, (void*) scep_receive_data},
        {CURLOPT_WRITEDATA, (void*) http},
    };


    do {
        cret = curl_util_set_opts(opts, sizeof(opts)/sizeof(curl_opt_helper), cctx);
        if (cret != CURLE_OK) {
            break;
        }


        cret = curl_util_perform(cctx, url, verbose, correlation_id);
        if (cret != CURLE_OK) {
            qeo_log_e("curl failed to perform <%s>", curl_easy_strerror(cret));
            switch (cret){
                case CURLE_SSL_CACERT:
                case CURLE_SSL_CACERT_BADFILE:
                case CURLE_SSL_CERTPROBLEM:
                case CURLE_SSL_CIPHER:
                case CURLE_SSL_CONNECT_ERROR:
                case CURLE_SSL_CRL_BADFILE:
                case CURLE_SSL_ENGINE_INITFAILED:
                case CURLE_SSL_ENGINE_NOTFOUND:
                case CURLE_SSL_ENGINE_SETFAILED:
                case CURLE_SSL_ISSUER_ERROR:
                case CURLE_SSL_SHUTDOWN_FAILED:
                case CURLE_SSL_PEER_CERTIFICATE:
                case CURLE_USE_SSL_FAILED:
                    ret = SCEP_PKISTATUS_SSL;
                    break;
                case CURLE_COULDNT_CONNECT:
                case CURLE_COULDNT_RESOLVE_HOST:
                case CURLE_COULDNT_RESOLVE_PROXY:
                    ret = SCEP_PKISTATUS_CONNECT;
                    break;
                default:
                    ret = SCEP_PKISTATUS_FAILURE;
                    break;
            }
            break;
        }

        cret = curl_easy_getinfo(cctx, CURLINFO_RESPONSE_CODE, &(http->status));
        if (cret != CURLE_OK) {
            qeo_log_e("curl failed to get response code <%s>", curl_easy_strerror(cret));
            break;
        }
        log_http_error = true;

        if (http->status == 403){
            ret = SCEP_PKISTATUS_FORBIDDEN;
            break;
        }

        cret = curl_easy_getinfo(cctx, CURLINFO_CONTENT_TYPE, &content_type);
        if (cret != CURLE_OK) {
            qeo_log_e("curl failed to get content type <%s>", curl_easy_strerror(cret));
            break;
        }

        if (!content_type) {
            break;
        }

        /* Set SCEP reply type */
        if (operation == SCEP_OPERATION_GETCA) {
            if (strstr(content_type, MIME_GETCA)) {
                http->type = SCEP_MIME_GETCA;
            }
            else if (strstr(content_type, MIME_GETCA_RA) || strstr(content_type, MIME_GETCA_RA_ENTRUST)) {
                http->type = SCEP_MIME_GETCA_RA;
            }
            else {
                break;
            }
        } else {
            if (!strstr(content_type, MIME_PKI)) {
                break;
            }
            http->type = SCEP_MIME_PKI;
        }
        ret = SCEP_PKISTATUS_SUCCESS;
    } while (0);
    if (log_http_error && (ret != SCEP_PKISTATUS_SUCCESS)){
        curl_util_log_http_error_description(cctx, correlation_id);
    }
    return ret;
}

/* URL-encode the input and return back encoded string */
char * url_encode(char *s, ssize_t n)
{
    char *r;
    ssize_t len;
    int i;
    char ch[2];

    /* Allocate 2 times bigger space than the original string */
    len = 2 * n;
    r = (char *)malloc(len);
    if (r == NULL ) {
        return NULL ;
    }
    strcpy(r, "");

    /* Copy data */
    for (i = 0; i < n; i++) {
        switch (*(s + i)) {
            case '+':
                strncat(r, "%2B", len);
                break;
            case '-':
                strncat(r, "%2D", len);
                break;
            case '=':
                strncat(r, "%3D", len);
                break;
            case '\n':
                strncat(r, "%0A", len);
                break;
            default:
                ch[0] = *(s + i);
                ch[1] = '\0';
                strncat(r, ch, len);
                break;
        }
    }
    r[len - 1] = '\0';
    return r;
}
