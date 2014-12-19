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
 * Qeo device API.
 */

#ifndef QEO_PLATFORM_SECURITY_H_
#define QEO_PLATFORM_SECURITY_H_

/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#include <inttypes.h>
#include <stdbool.h>
#include <qeo/util_error.h>

/*########################################################################
#                                                                       #
#  TYPES SECTION                                             #
#                                                                       #
########################################################################*/
#define QEO_REGISTRATION_URL "https://join.qeo.org/"

typedef uintptr_t qeo_platform_security_context_t;

/**
 * Platform security states.
 *
 *
 * \note Now these contain the same values as in security.h but this is by no means
 * per definition. It is likely platforms are not interested in all the states.
 * Maybe it is not even desirable to expose all the internal states...
 * if you wish to have different enum values than in security.h, be sure to adapt the code .. 
 * */
typedef enum {
    QEO_PLATFORM_SECURITY_UNAUTHENTICATED,
    QEO_PLATFORM_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS,
    QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY,
    QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED,
    QEO_PLATFORM_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE,
    QEO_PLATFORM_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS,
    QEO_PLATFORM_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS,
    QEO_PLATFORM_SECURITY_STORING_QEO_CREDENTIALS,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE,
    QEO_PLATFORM_SECURITY_AUTHENTICATED
} qeo_platform_security_state;

/**
 * Platform security state reasons.
 *
 *
 * \note Currently mainly used to indicate why something failed
 */
typedef enum {
    QEO_PLATFORM_SECURITY_REASON_UNKNOWN,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_CANCELLED,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_PLATFORM_FAILURE,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INVALID_OTP,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_NETWORK_FAILURE,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_SSL_HANDSHAKE_FAILURE,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_RECEIVED_INVALID_CREDENTIALS,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_STORE_FAILURE,
    QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_UNKNOWN,

} qeo_platform_security_state_reason;

/** 
 * Different registration methods. 
 *
 */
typedef enum {
/**
 * \brief Default value
 */
    QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_NONE,

/**
 * \brief One Time Password
 */
    QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP,

/**
 * \brief One Time Password
 */
    QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION,
} qeo_platform_security_registration_method;

/** 
 * Obtained registration credentials
 */
typedef struct {
    union {
        struct {
            const char *url;
            const char *otp;
        } otp;
        struct {
            unsigned long registration_window; /* length of window in seconds */
            const char *suggested_username;  /* mgmt app can create user account on back-end*/
        } remote_registration;
    } u;

    qeo_platform_security_registration_method  reg_method;
} qeo_platform_security_registration_credentials;

/**
 * Callback to call to pass obtained registration credentials
 *
 * \param[in] context context to pass which you got from qeo_platform_security_registration_credentials_needed()
 *
 * \retval ::QEO_UTIL_OK on success
 * \retval ::QEO_UTIL_EINVAL in case of invalid arguments
 * \retval ::QEO_UTIL_EBADSTATE when qeo security api has already been destructed or when qeo_platform_security_set_registration_credentials_cb() was already called
 * \retval ::QEO_UTIL_ENOMEM No memory available
 *
 */
typedef qeo_util_retcode_t (*qeo_platform_security_set_registration_credentials_cb)(qeo_platform_security_context_t context, const qeo_platform_security_registration_credentials *cred);

/**
 * Callback to call when obtaining of registration credentials was cancelled
 *
 * \param[in] context context to pass which you got from qeo_platform_security_registration_credentials_needed()
 *
 * \retval ::QEO_UTIL_OK on success
 * \retval ::QEO_UTIL_EINVAL in case of invalid arguments
 * \retval ::QEO_UTIL_EBADSTATE when qeo security api has already been destructed or when qeo_platform_security_set_registration_credentials_cb() was already called
 *
 */
typedef qeo_util_retcode_t (*qeo_platform_security_registration_credentials_cancelled_cb)(qeo_platform_security_context_t context);

/**
 * Structure with remote registration information
 *
 */
typedef struct {
    const char *realm_name;
    const char *url;
} qeo_platform_security_remote_registration_credentials_t;

typedef qeo_util_retcode_t (*qeo_platform_security_remote_registration_credentials_feedback_cb)(qeo_platform_security_context_t context, bool ok);
/*########################################################################
#                                                                       #
#  API FUNCTION SECTION                                             #
#                                                                       #
########################################################################*/

/**
 * This function will be called when qeo-c-core requires Qeo registration credentials (e.g. OTP). 
 * It is the responsibility of the implementer to call either set_reg_cred_cb to pass the obtained registration credentials OR
 * to call set_cancelled_cb to indicate no registration credentials were obtained.
 * Calling this callbacks may be done in an asynchronous way. In other words qeo_platform_security_registration_credentials_needed()
 * may return immediately. 
 *
 * \pre security state SHOULD be 
 *  ::QEO_PLATFORM_SECURITY_UNAUTHENTICATED, 
 *  ::QEO_PLATFORM_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS, 
 *  ::QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY, 
 *  ::QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED
 *
 *  \pre They MUST NOT be called after 
 *  ::QEO_PLATFORM_SECURITY_AUTHENTICATED,
 *  ::QEO_SECURITY_AUTHENTICATION_FAILURE 
 *
 * \param[in] context This parameter must be passed on in the callbacks.
 * \param[in] set_reg_cred_cb Callback which should be called to pass registration credentials.
 * \param[in] set_cancelled_cb Callback which should be called when user cancelled.
 *
 * \retval ::QEO_UTIL_OK on success
 * 
 */
qeo_util_retcode_t qeo_platform_security_registration_credentials_needed(qeo_platform_security_context_t context, 
                                                                         qeo_platform_security_set_registration_credentials_cb set_reg_cred_cb,
                                                                         qeo_platform_security_registration_credentials_cancelled_cb set_cancelled_cb);


/**
 * This function will indicate in which state the qeo security state machine currently is.
 * This information should only be used for diagnostics, and user feedback, not to implement any logic.
 *
 * \param[in] context This parameter must be passed on in the callbacks.
 * \param[in] state Current state of qeo security state machine
 * \param[in] state_reason Reason for the state transition (currently only used in case of failure)
 *
 *
 */
void qeo_platform_security_update_state(qeo_platform_security_context_t context, qeo_platform_security_state state, qeo_platform_security_state_reason state_reason); 

/**
 * This function will be called when qeo-c-core has received 'remote qeo registration credentials' from a Qeo management application.
 * To continue, a positive or negative confirmation must be given by calling the provided callback
 *
 * \param[in] context This parameter must be passed on in the callbacks.
 * \param[in] rrcred Contains remote registration information
 * \param[in] cb Callback which must be used to give confirmation feedback
 *
 * \retval ::QEO_UTIL_OK on success
 */
qeo_util_retcode_t qeo_platform_security_remote_registration_confirmation_needed(qeo_platform_security_context_t context,
                                                                                 const qeo_platform_security_remote_registration_credentials_t *rrcred,
                                                                                 qeo_platform_security_remote_registration_credentials_feedback_cb cb);
                                                                                 

#endif 
