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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "qeo/mgmt_client_forwarder.h"

#define LOCATOR_FILE "/tmp/locators.txt"

qeo_mgmt_client_retcode_t qeo_mgmt_client_register_forwarder(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     qeo_mgmt_client_locator_t *locators,
                                                     u_int32_t nrOfLocators,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *cookie)
{
    FILE *fp = NULL;

    printf("qeo_mgmt_client_register_forwarder overloaded\n");
    fp = fopen(LOCATOR_FILE, "w");
    if (fp != NULL) {
        if ((nrOfLocators == 0) || (locators == NULL)) {
            fprintf(fp, "none");
        }
        else {
            fprintf(fp, "%s %d\n", locators->address, locators->port);
        }
        fclose(fp);
    }

    return QMGMTCLIENT_OK;
}

qeo_mgmt_client_retcode_t qeo_mgmt_client_get_forwarders_sync(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     qeo_mgmt_client_forwarder_cb fwd_cb,
                                                     void *fwd_cookie,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie)
{
    FILE *fp = NULL;
    qeo_mgmt_client_forwarder_t *forwarder = calloc(1, sizeof(qeo_mgmt_client_forwarder_t));
    qeo_mgmt_client_locator_t   *locator = calloc(1, sizeof(qeo_mgmt_client_locator_t));
    char address[100];
    int n = 0;

    printf("qeo_mgmt_client_get_forwarders_sync overloaded\n");
    fp = fopen(LOCATOR_FILE, "r");
    if (fp != NULL) {
        locator->address = address;
        n = fscanf(fp, "%s %d\n", locator->address, &locator->port);
        if (n == 2) {
            forwarder->nrOfLocators = 1;
            forwarder->locators = locator;
            fwd_cb(forwarder, fwd_cookie);
        }
        fclose(fp);
    }

    return QMGMTCLIENT_OK;
}
