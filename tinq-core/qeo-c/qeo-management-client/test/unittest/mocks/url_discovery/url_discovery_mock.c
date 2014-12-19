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
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <qeo/log.h>
#include "unittest/unittest.h"
#include "url_discovery_mock.h"
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
 #                   PUBLIC FUNCTION IMPLEMENTATION                #
 ########################################################################*/

qeo_mgmt_client_retcode_t qeo_mgmt_url_get(qeo_mgmt_client_ctx_t* ctx, qeo_mgmt_client_ssl_ctx_cb ssl_cb, void *ssl_cookie, const char* base_url, qeo_mgmt_url_type_t service, const char** url){
    *url=base_url;
    ck_assert_ptr_eq(ctx->url_ctx, (void*)1);
    return QMGMTCLIENT_OK;
}

qeo_mgmt_url_ctx_t qeo_mgmt_url_init(CURL *cctx){
    return (qeo_mgmt_url_ctx_t) 1;
}

void qeo_mgmt_url_cleanup(qeo_mgmt_url_ctx_t ctx){
    if (ctx != NULL){
        ck_assert_ptr_eq(ctx, (void*)1);
    }
}
