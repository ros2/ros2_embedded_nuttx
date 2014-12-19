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

#ifndef QEO_MGMT_CLIENT_PRIV_H_
#define QEO_MGMT_CLIENT_PRIV_H_
#include <curl/curl.h>
#include <pthread.h>
#include "qeo/mgmt_client.h"
#include "qeo_mgmt_urls.h"
#include "sscep_api.h"

#if DEBUG == 1
#define OP_VERBOSE 1
#define OP_DEBUG 1
#else
#define OP_VERBOSE 0
#define OP_DEBUG 0
#endif

typedef struct qeo_mgmt_fd_link_s {
    int fd;
    struct qeo_mgmt_fd_link_s* next;
} qeo_mgmt_fd_link_t;

struct qeo_mgmt_client_ctx_s{
    sscep_ctx_t sscep_ctx;
    CURL *curl_ctx;
    bool curl_global_init;
    qeo_mgmt_url_ctx_t url_ctx;
    pthread_mutex_t mutex;
    pthread_mutex_t fd_mutex;
    bool mutex_init;
    pthread_t worker_thread;
    bool join_thread;
    qeo_mgmt_fd_link_t* fd_list;
    bool is_closed;
};

/*
 * Wrapper structure used to bind curl ssl callbacks with the mgmt client callbacks.
 */
typedef struct
{
    qeo_mgmt_client_ssl_ctx_cb cb;
    void *cookie;
} curl_ssl_ctx_helper;

int qmc_lock_ctx(qeo_mgmt_client_ctx_t *ctx);
void qmc_unlock_ctx(qeo_mgmt_client_ctx_t *ctx);
void qmc_clean_ongoing_thread(qeo_mgmt_client_ctx_t *ctx);
#endif /* QEO_MGMT_CLIENT_PRIV_H_ */
