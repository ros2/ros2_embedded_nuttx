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
# HEADER (INCLUDE) SECTION                                               #
########################################################################*/
#ifndef REMOTE_REGISTRATION_H_
#define REMOTE_REGISTRATION_H_
#include <qeo/error.h>
#include <qeo/device.h>
#include <openssl/ssl.h>
#include <stdint.h>

/*#######################################################################
#                       TYPE SECTION                                    #
########################################################################*/
typedef struct qeo_remote_registration_s *qeo_remote_registration_hndl_t;

/* no assumptions on which thread it runs in */
typedef void (*qeo_registration_credentials_cb)(qeo_remote_registration_hndl_t remote_reg, const char *otp, const char *realm_name, const char *url);

/* no assumptions on which thread it runs in */
typedef void (*qeo_registration_timeout_cb)(qeo_remote_registration_hndl_t remote_reg);

typedef struct {
    unsigned long registration_window; /* seconds */ 
    char          *suggested_username;  
    EVP_PKEY *pkey;

    uintptr_t user_data;

} qeo_remote_registration_cfg_t;

typedef struct {
    qeo_registration_credentials_cb on_qeo_registration_credentials;
    qeo_registration_timeout_cb on_qeo_registration_timeout;
} qeo_remote_registration_init_cfg_t;

typedef enum {
    QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED,
    QEO_REMOTE_REGISTRATION_STATUS_REGISTERING,
} qeo_remote_registration_status_t;

typedef enum {
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_CANCELLED,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_NEGATIVE_CONFIRMATION,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_PLATFORM_FAILURE,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_INVALID_OTP,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_INTERNAL_ERROR,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_NETWORK_FAILURE,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_SSL_HANDSHAKE_FAILURE,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_RECEIVED_INVALID_CREDENTIALS,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_STORE_FAILURE,
    QEO_REMOTE_REGISTRATION_FAILURE_REASON_UNKNOWN,
} qeo_remote_registration_failure_reason_t;



/*########################################################################
#                       API FUNCTION SECTION                             #
########################################################################*/

qeo_retcode_t qeo_remote_registration_init(const qeo_remote_registration_init_cfg_t *cfg);

void qeo_remote_registration_destroy(void);

/* pointers in cfg struct are copied as-is (shallow copy). We assume remote_reg object will live at least as long as the security object */
qeo_retcode_t qeo_remote_registration_construct(const qeo_remote_registration_cfg_t *cfg, qeo_remote_registration_hndl_t *remote_reg);

qeo_retcode_t qeo_remote_registration_get_user_data(qeo_remote_registration_hndl_t remote_reg, uintptr_t *user_data);

/* disposes request, stops timer and frees resources */
qeo_retcode_t qeo_remote_registration_destruct(qeo_remote_registration_hndl_t *remote_reg);

/* starts timer and publishes initial request */
qeo_retcode_t qeo_remote_registration_start(qeo_remote_registration_hndl_t remote_reg);

/* updates request */
qeo_retcode_t qeo_remote_registration_update_registration_status(qeo_remote_registration_hndl_t remote_reg, qeo_remote_registration_status_t status, qeo_remote_registration_failure_reason_t reason);

/* Enable using new registration credentials upon reception */
qeo_retcode_t qeo_remote_registration_enable_using_new_registration_credentials(qeo_remote_registration_hndl_t remote_reg);

void qeocore_remote_registration_init_cond();

void qeocore_remote_registration_set_key(EVP_PKEY * key);


#endif
