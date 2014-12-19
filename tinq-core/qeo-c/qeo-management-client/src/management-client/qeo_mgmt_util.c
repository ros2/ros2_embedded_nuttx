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
#include <qeo/log.h>
#include "qeo_mgmt_client_priv.h"


/*#######################################################################
 #                       TYPES SECTION                                   #
 ########################################################################*/

/*#######################################################################
 #                   STATIC FUNCTION DECLARATION                         #
 ########################################################################*/

/*#######################################################################
 #                       STATIC VARIABLE SECTION                         #
 ########################################################################*/

/*#######################################################################
 #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

/*#######################################################################
 #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

int qmc_lock_ctx(qeo_mgmt_client_ctx_t *ctx){
    return (pthread_mutex_lock(&ctx->mutex) == 0);
}

void qmc_unlock_ctx(qeo_mgmt_client_ctx_t *ctx){
    pthread_mutex_unlock(&ctx->mutex);
}

void qmc_clean_ongoing_thread(qeo_mgmt_client_ctx_t *ctx){
    if (ctx->join_thread == true){
        qeo_log_i("Waiting for worker thread to shut down");
        if (pthread_join(ctx->worker_thread, NULL) != 0){
            qeo_log_w("Failed to join worker thread");
        }
        ctx->join_thread = false;
    }
}
