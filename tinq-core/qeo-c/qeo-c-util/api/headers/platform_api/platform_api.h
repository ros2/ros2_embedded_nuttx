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

/** \file
 * Qeo Platform API.
 */

#ifndef qeo_platform_HELPER_H_
#define qeo_platform_HELPER_H_
/*#######################################################################
# HEADER (INCLUDE) SECTION                                               #
########################################################################*/
#include <qeo/util_error.h>
#include <qeo/platform_security.h>
#include <qeo/device.h>
#include <stdbool.h>

/*#######################################################################
#                       TYPE SECTION                                    #
########################################################################*/

/**
 * Callback that will be called when Qeo registration parameters are needed
 * e.g. OTC and URL
 * 
 */
typedef qeo_util_retcode_t (*qeo_platform_on_registration_params_needed_cb)(uintptr_t app_context, qeo_platform_security_context_t sec_context);

/**
 * Callback to notify when security state machine has reached a new state. Only to be used for user-feedback
 *
 */
typedef void (*qeo_platform_on_security_status_update_cb)(uintptr_t app_context, qeo_platform_security_context_t sec_context, qeo_platform_security_state state, qeo_platform_security_state_reason reason);

/**
 * Callback to notify remote registration credentials have been received.
 * In this state qeo_platform_confirm_remote_registration_credentials MUST be called to continue.
 *
 */
typedef qeo_util_retcode_t (*qeo_platform_on_remote_registration_confirmation_needed_cb)(uintptr_t app_context, qeo_platform_security_context_t sec_context,
                                                                                 const qeo_platform_security_remote_registration_credentials_t *rrcred);

/**
 * 
 *
 */
typedef struct {
    qeo_platform_on_registration_params_needed_cb on_reg_params_needed;
    qeo_platform_on_security_status_update_cb on_sec_update;
    qeo_platform_on_remote_registration_confirmation_needed_cb on_rr_confirmation_needed;
} qeo_platform_callbacks_t;

/*########################################################################
#                       API FUNCTION SECTION                             #
########################################################################*/

/*
 * Initialize the platform helper API.
 * calling of this function will override existing callbacks.
 * This function should be called prior to using the Qeo API (e.g. creating factories)
 *
 * \param[in] app_context Application context which will be passed in each callback
 * \param[in] cbs set of callbacks related to registration
 *
 * */
qeo_util_retcode_t qeo_platform_init(uintptr_t app_context, const qeo_platform_callbacks_t *cbs);

/*
 * Store a pointer to device storage path (only pointer is stored, ownership is not transferred) 
 * This function should be called prior to using the Qeo API (e.g. creating factories)
 *
 * */
qeo_util_retcode_t qeo_platform_set_device_storage_path(const char* path);

/**
 * Store a pointer to the CA certificate storage path (only pointer is stored, ownership is not transferred)
 * This function should be called prior to using the Qeo API (e.g. creating factories)
 *
 * \param[in] ca_file A file containing a list of CA certificates in PEM format, or \c NULL.
 * \param[in] ca_path A directory containing CA certificates in PEM format, or \c NULL.
 *                    See also the man page of OpenSSL's SSL_CTX_load_verify_locations().
 */
qeo_util_retcode_t qeo_platform_set_cacert_path(const char* ca_file,
                                                const char* ca_path);

/*
 * Store a pointer to device info path (only pointer is stored, ownership is not transferred) 
 * This function should be called prior to using the Qeo API (e.g. creating factories)
 *
 * */
qeo_util_retcode_t qeo_platform_set_device_info(const qeo_platform_device_info *device_info);

/* 
 * Set the otc and url. 
 *
 * \param[in] sec_context Security context as one provided in qeo_platform_on_registration_params_needed_cb() 
 * \param[in] otc one-time-code
 * \param[in] url url for Qeo registration
 */
qeo_util_retcode_t qeo_platform_set_otc_url(qeo_platform_security_context_t sec_context, const char *otc, const char *url);

/* 
 * Set the remote registration parameters. 
 *
 * \param[in] sec_context Security context as one provided in qeo_platform_on_registration_params_needed_cb() 
 * \param[in] suggested_username Suggested username as it can be shown on a Qeo management app.
 * \param[in] registration_window Registration window in seconds
 * */
qeo_util_retcode_t qeo_platform_set_remote_registration_params(qeo_platform_security_context_t sec_context, const char *suggested_username, unsigned long registration_window);

/*
 * Cancel the registration. 
 * This will lead to factory creation failure.. 
 *
 * \param[in] sec_context Security context as one provided in qeo_platform_on_registration_params_needed_cb() 
 */
qeo_util_retcode_t qeo_platform_cancel_registration(qeo_platform_security_context_t sec_context);

/*
 * Give positive or negative confirmation on remote registration credentials. 
 *
 * \param[in] sec_context Security context as one provided in qeo_platform_on_registration_params_needed_cb() 
 * \param[in] confirmation boolean to indicate whether you accept the remote registration 
 */
qeo_util_retcode_t qeo_platform_confirm_remote_registration_credentials(qeo_platform_security_context_t sec_context, bool confirmation);

/* Releases resources */
void qeo_platform_destroy(void);

#endif                                                                                                                                                                                                 
