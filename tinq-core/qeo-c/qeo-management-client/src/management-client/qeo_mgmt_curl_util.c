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

#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include "qeo/log.h"
#include <qeo/platform.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include "curl_util.h"
#include "errno.h"

#include "qeo/mgmt_client.h"
#include "qeo_mgmt_curl_util.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/
#define DATA_MEMM_MAX 0x08000 /*32kB*/
#define DATA_MEMM_MIN 0x800 /*2kB*/

typedef struct {
    char* data;
    ssize_t offset;
    ssize_t length;
} qmgmt_curl_data_helper;

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/*
 * Intentionally made non static for unit testing.
 */
size_t _write_to_memory_cb( char *buffer, size_t size, size_t nmemb, void *outstream){
    qmgmt_curl_data_helper *data = (qmgmt_curl_data_helper *)outstream;
    size_t totalsize = size * nmemb;
    char *buf = NULL;
    size_t newlength = 0;
    size_t result = 0;

    do {
        if (totalsize == 0){
            break;
        }
        while (totalsize >= data->length - data->offset) {
            if (data->length >= DATA_MEMM_MAX) {
                qeo_log_w("Returned amount of data is too much, exceeding %d", DATA_MEMM_MAX);
                break;
            }
            if (data->length == 0){
                newlength = DATA_MEMM_MIN;
            } else {
                newlength = data->length*2;
            }

            buf = realloc(data->data, newlength);
            if (buf == NULL ) {
                qeo_log_w("Could not allocate buffer of %d bytes", newlength);
                break;
            }
            data->data = buf;
            data->length = newlength;
        }
        if (totalsize >= data->length - data->offset)
            break;
        memcpy(data->data+data->offset, buffer, totalsize);
        data->offset+=totalsize;
        buf=data->data+data->offset;
        *buf = '\0';/*Make it always null terminated. */
        result=totalsize;
    } while (0);

    return result;
}

/*
 * Intentionally made non static for unit testing.
 */
size_t _read_from_memory_cb(char *buffer, size_t size, size_t nmemb, void *instream)
{
    qmgmt_curl_data_helper *data = (qmgmt_curl_data_helper *)instream;
    size_t result = size * nmemb;

    if (result > (data->length - data->offset))
        result = data->length - data->offset;
    memcpy(buffer, data->data + data->offset, result);
    data->offset+=result;
    return result;
}

static int _lock_fd_mutex(qeo_mgmt_client_ctx_t* ctx) {
    return (pthread_mutex_lock(&ctx->fd_mutex) == 0);
}

void _unlock_fd_mutex(qeo_mgmt_client_ctx_t *ctx){
    pthread_mutex_unlock(&ctx->fd_mutex);
}


curl_socket_t qmcu_socket_open_function(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr) {
    curl_socket_t sock = socket(addr->family, addr->socktype, addr->protocol);
    if (sock >= 0 && clientp) {
        qeo_mgmt_client_ctx_t* ctx = (qeo_mgmt_client_ctx_t*) clientp;
        if (_lock_fd_mutex(ctx)) {
            if (ctx->is_closed) {
                qeo_log_d("management thread shutdown requested, not creating socket");
                close(sock);
                _unlock_fd_mutex(ctx);
                return CURL_SOCKET_BAD;
            }
            qeo_mgmt_fd_link_t* new_fd_link = malloc(sizeof(qeo_mgmt_fd_link_t));
            if (new_fd_link) {
                new_fd_link->fd = sock;
                new_fd_link->next = NULL;
                if (ctx->fd_list == NULL) {
                    ctx->fd_list = new_fd_link;
                } else {
                    qeo_mgmt_fd_link_t* link = ctx->fd_list;
                    while (link->next) {
                        link = link->next;
                    }
                    link->next = new_fd_link;
                }
            }
            _unlock_fd_mutex(ctx);
        }
    }
    return sock;
}

int qmcu_socket_close_function(void *clientp, curl_socket_t item) {
    if (clientp) {
        qeo_mgmt_client_ctx_t* ctx = (qeo_mgmt_client_ctx_t*) clientp;
        if (_lock_fd_mutex(ctx)) {
            qeo_mgmt_fd_link_t* link = ctx->fd_list;
            if (link->fd == item) {
                ctx->fd_list = link->next;
                free(link);
            } else {
                qeo_mgmt_fd_link_t* prev = link;
                while (link->next) {
                    link = link->next;
                    if (link->fd == item) {
                        prev->next = link->next;
                        free(link);
                        break;
                    }
                    prev = link;
                }
            }
            _unlock_fd_mutex(ctx);
        }
    }
    return close(item);
}


/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

void qeo_mgmt_curl_util_shutdown_connections(qeo_mgmt_client_ctx_t* ctx) {
    qeo_log_d("qeo_mgmt_curl_util_shutdown_connections");
    if (ctx) {
        if (_lock_fd_mutex(ctx)) {
            qeo_mgmt_fd_link_t* fd_link = ctx->fd_list;
            while (fd_link) {
                shutdown(fd_link->fd, SHUT_RDWR);
                fd_link = fd_link->next;
            }
            ctx->is_closed = true;
            _unlock_fd_mutex(ctx);
        }
        else {
            qeo_log_e("Can't take mgmt ctx lock");
        }
    }
}

void qeo_mgmt_curl_util_clean_fd_list(qeo_mgmt_client_ctx_t* ctx) {
    if (_lock_fd_mutex(ctx)) {
        qeo_mgmt_fd_link_t* link = ctx->fd_list;
        ctx->fd_list = NULL;
        while(link) {
            qeo_mgmt_fd_link_t* freeMe = link;
            link = link->next;
            free(freeMe);
        }
        _unlock_fd_mutex(ctx);
    }
}

CURLcode qeo_mgmt_curl_util_set_opts(curl_opt_helper *opts, int nropts, qeo_mgmt_client_ctx_t* ctx) {
    curl_opt_helper opts_sock[] = {
        { CURLOPT_OPENSOCKETFUNCTION, &qmcu_socket_open_function},
        { CURLOPT_CLOSESOCKETFUNCTION, &qmcu_socket_close_function},
        { CURLOPT_OPENSOCKETDATA, ctx },
        { CURLOPT_CLOSESOCKETDATA, ctx}};

   CURLcode ret = curl_util_set_opts(opts, nropts, ctx->curl_ctx);
   if (ret == CURLE_OK) {
       ret = curl_util_set_opts(opts_sock, sizeof(opts_sock) / sizeof(curl_opt_helper), ctx->curl_ctx);
   }
   return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_translate_rc(int curlrc)
{
    switch (curlrc) {
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
            return QMGMTCLIENT_ESSL;
        case CURLE_COULDNT_CONNECT:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
            return QMGMTCLIENT_ECONNECT;
        case CURLE_HTTP_RETURNED_ERROR:
            return QMGMTCLIENT_EHTTP;
        case CURLE_OK:
            return QMGMTCLIENT_OK;
    }
    return QMGMTCLIENT_EFAIL;
}

CURLcode qeo_mgmt_curl_sslctx_cb(CURL * curl, void* sslctx, void* userdata)
{
    curl_ssl_ctx_helper *sslctxhelper = (curl_ssl_ctx_helper*)userdata;
    qeo_mgmt_client_retcode_t rc = QMGMTCLIENT_EFAIL;

    do {
        if (!sslctxhelper) {
            break;
        }
        rc = sslctxhelper->cb(sslctx, sslctxhelper->cookie);
    } while (0);

    if (rc != QMGMTCLIENT_OK){
        return CURLE_USE_SSL_FAILED;
    } else {
        return CURLE_OK;

    }
}

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_perform(CURL *ctx,
                                                     const char* url,
                                                     char* cid){
    CURLcode cret = curl_util_perform(ctx, url, OP_VERBOSE, cid);

    if (cret == CURLE_HTTP_RETURNED_ERROR){
        long http_status = 0;
        if (CURLE_OK == curl_easy_getinfo(ctx, CURLINFO_RESPONSE_CODE, &http_status)){
            if ((http_status == 403) || (http_status == 401) || (http_status == 405)){
                return QMGMTCLIENT_ENOTALLOWED;
            }
        }
    }
    return qeo_mgmt_curl_util_translate_rc(cret);
}


qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_http_get_with_cb(qeo_mgmt_client_ctx_t* mg_ctx,
                                                     const char* url,
                                                     char *header,
                                                     curl_write_callback data_cb,
                                                     void *write_cookie){
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    struct curl_slist *chunk = (header != NULL)?curl_slist_append(NULL, header):NULL;
    long http_status = 0;
    char correlation_id[CURL_UTIL_CORRELATION_ID_MAX_SIZE];
    curl_opt_helper opts[] = {
        { CURLOPT_WRITEFUNCTION, (void*) data_cb },
        { CURLOPT_WRITEDATA, (void*)write_cookie},
        { CURLOPT_FAILONERROR, (void*)1 },
        { CURLOPT_HTTPHEADER, (void*) chunk}};
    bool reset = false;

    do {
        CURL* ctx;
        if ((mg_ctx == NULL) || (mg_ctx->curl_ctx == NULL) || (url == NULL) || (data_cb == NULL)){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        ctx = mg_ctx->curl_ctx;
        if ((header != NULL) && (chunk == NULL)) {
            ret = QMGMTCLIENT_EMEM;
            break;
        }
        reset = true;
        if (CURLE_OK != qeo_mgmt_curl_util_set_opts(opts, sizeof(opts) / sizeof(curl_opt_helper), mg_ctx)) {
            ret = QMGMTCLIENT_EINVAL;
            break;
        }

        qeo_log_i("Start fetching data from <%s>", url);
        ret = qeo_mgmt_curl_util_perform(ctx, url, NULL);
        if (ret != QMGMTCLIENT_OK) {
            break;
        }
        if (curl_easy_getinfo(ctx, CURLINFO_RESPONSE_CODE, &http_status) == CURLE_OK) {
            if (http_status >= 400){
                ret = qeo_mgmt_curl_util_translate_rc(CURLE_HTTP_RETURNED_ERROR);
                curl_util_log_http_error_description(ctx, correlation_id);
                break;
            }
        }
        qeo_log_i("Successfully downloaded data");
    } while (0);

    if (reset == true){
        /* Make sure we reset all configuration for next calls */
        curl_easy_reset(mg_ctx->curl_ctx);
    }
    if (chunk != NULL){
        curl_slist_free_all(chunk);
    }

    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failure getting %s",url);
    }

    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_http_get(qeo_mgmt_client_ctx_t *mg_ctx,
                                                     const char* url,
                                                     char *header,
                                                     char **data,
                                                     size_t *length)

{
    qmgmt_curl_data_helper data_helper = {0};
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;

    ret = qeo_mgmt_curl_util_http_get_with_cb(mg_ctx, url, header, _write_to_memory_cb, &data_helper);
    if (ret == QMGMTCLIENT_OK) {
        *data = data_helper.data;
        *length = data_helper.offset;
    } else {
        free(data_helper.data);
    }
    return ret;

}

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_get_with_cb(qeo_mgmt_client_ctx_t *mg_ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     curl_write_callback data_cb,
                                                     void *write_cookie)
{
    curl_ssl_ctx_helper curlsslhelper = { ssl_cb, ssl_cookie };
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    const char *cafile = NULL;
    const char *capath = NULL;
    //Setting CURLOPT_CAINF and PATH is mandatory; otherwise SSL_CTX_FUNCTION will not be called.
    curl_opt_helper opts[] = {
        { CURLOPT_SSL_VERIFYHOST, (void*)2 }, /* ensure certificate matches host */
        { CURLOPT_SSL_VERIFYPEER, (void*)0 }, /* ensure certificate is valid */
        { CURLOPT_CAINFO, NULL },
        { CURLOPT_CAPATH, NULL },
        { CURLOPT_SSL_CTX_FUNCTION, (void*)qeo_mgmt_curl_sslctx_cb },
        { CURLOPT_SSL_CTX_DATA, (void*)&curlsslhelper }
    };
    bool reset = false;

    do {
        if ((mg_ctx == NULL ) || (mg_ctx->curl_ctx == NULL) ||  (url == NULL) || (ssl_cb == NULL) || (data_cb == NULL)){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        if (QEO_UTIL_OK != qeo_platform_get_cacert_path(&cafile, &capath)) {
            ret = QMGMTCLIENT_EFAIL;
            break;
        }
        /* insert values into options array */
        opts[2].cookie = (void*)cafile;
        opts[3].cookie = (void*)capath;
        reset = true;
        if (CURLE_OK != qeo_mgmt_curl_util_set_opts(opts, sizeof(opts) / sizeof(curl_opt_helper), mg_ctx)) {
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        ret = qeo_mgmt_curl_util_http_get_with_cb(mg_ctx, url, header, data_cb, write_cookie);
        reset = false;/* Already done. */
    } while (0);

    if (reset == true){
        /* Make sure we reset all configuration for next calls */
        curl_easy_reset(mg_ctx->curl_ctx);
    }

    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failure in https_get_%s",url);
    }
    return ret;

}


qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_get(qeo_mgmt_client_ctx_t *mg_ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     char **data,
                                                     size_t *length)
{
    qmgmt_curl_data_helper data_helper = {0};
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;

    ret = qeo_mgmt_curl_util_https_get_with_cb(mg_ctx, url, header, ssl_cb, ssl_cookie, _write_to_memory_cb, &data_helper);
    if (ret == QMGMTCLIENT_OK) {
        *data = data_helper.data;
        *length = data_helper.offset;
    } else {
        free(data_helper.data);
    }
    return ret;
}

static qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_put_with_cb(qeo_mgmt_client_ctx_t *mg_ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     curl_read_callback data_cb,
                                                     void *read_cookie,
                                                     intptr_t length)
{
    curl_ssl_ctx_helper curlsslhelper = { ssl_cb, ssl_cookie };
    char correlation_id[CURL_UTIL_CORRELATION_ID_MAX_SIZE];
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    struct curl_slist *chunk = (header != NULL)?curl_slist_append(NULL, header):NULL;
    long http_status = 0;
    curl_opt_helper opts[] = {
        { CURLOPT_INFILESIZE, (void*) length }, /* keep this at index 0; value is update in code */
        { CURLOPT_UPLOAD, (void*)1 },
        { CURLOPT_READFUNCTION, (void*) data_cb },
        { CURLOPT_READDATA, (void*)read_cookie},
        { CURLOPT_HTTPHEADER, (void*) chunk},
        { CURLOPT_SSL_VERIFYPEER, (void*)0 },
        { CURLOPT_SSL_CTX_FUNCTION, (void*)qeo_mgmt_curl_sslctx_cb },
        { CURLOPT_SSL_CTX_DATA, (void*)&curlsslhelper },
    };

    bool reset = false;
    do {
        CURL* ctx;
        if ((mg_ctx == NULL ) || (mg_ctx->curl_ctx == NULL) ||  (url == NULL) || (ssl_cb == NULL) || (data_cb == NULL)){
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        ctx = mg_ctx->curl_ctx;
        reset = true;
        if (CURLE_OK != qeo_mgmt_curl_util_set_opts(opts, sizeof(opts) / sizeof(curl_opt_helper), mg_ctx)) {
            ret = QMGMTCLIENT_EINVAL;
            break;
        }
        ret = qeo_mgmt_curl_util_perform(ctx, url, correlation_id);

        if (curl_easy_getinfo(ctx, CURLINFO_RESPONSE_CODE, &http_status) == CURLE_OK) {
            qeo_log_d("returned status code %ld", http_status);
            if (http_status >= 400){
                ret = qeo_mgmt_curl_util_translate_rc(CURLE_HTTP_RETURNED_ERROR);
                curl_util_log_http_error_description(ctx, correlation_id);
                break;
            }
        }
    } while (0);

    if (reset == true){
        /* Make sure we reset all configuration for next calls */
        curl_easy_reset(mg_ctx->curl_ctx);
    }
    if (chunk != NULL){
        curl_slist_free_all(chunk);
    }

    if (ret != QMGMTCLIENT_OK) {
        qeo_log_w("Failure in https_put_%s",url);
    }

    return ret;
}

qeo_mgmt_client_retcode_t qeo_mgmt_curl_util_https_put(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     char *header,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     char *data,
                                                     size_t length)
{
    qmgmt_curl_data_helper data_helper = {0};

    data_helper.data = data;
    data_helper.length = length;
    return qeo_mgmt_curl_util_https_put_with_cb(ctx, url, header, ssl_cb, ssl_cookie, _read_from_memory_cb, &data_helper, length);
}


