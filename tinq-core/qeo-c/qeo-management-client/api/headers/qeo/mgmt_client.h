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

#ifndef QEO_MGMT_CLIENT_H_
#define QEO_MGMT_CLIENT_H_

#include <openssl/x509.h>
#include <inttypes.h>
#include <stdbool.h>
#include <qeo/device.h>

/**
 * Enumeration of the different return values that are supported.
 */
typedef enum
{
    QMGMTCLIENT_OK, /**< Success */
    QMGMTCLIENT_EFAIL, /**< General failure */
    QMGMTCLIENT_EINVAL, /**< Invalid arguments */
    QMGMTCLIENT_EMEM, /**< Not enough memory */
    QMGMTCLIENT_EOTP, /**< Invalid One Time Password */
    QMGMTCLIENT_ECONNECT, /**< Could not setup the connection */
    QMGMTCLIENT_EHTTP, /**< An unsuccessful HTTP status code was returned */
    QMGMTCLIENT_ESSL, /**< SSL handshake issue */
    QMGMTCLIENT_ENOTALLOWED, /**< The server indicates that you are not allowed to do this (HTTP code 401, 403 or 405) */
    QMGMTCLIENT_EBADREQUEST, /**< The provided arguments are not valid on the server. */
    QMGMTCLIENT_EBADSERVICE, /**< A URL for this service was not found. */
    QMGMTCLIENT_EBADREPLY, /**< A reply from the service is not valid. */
} qeo_mgmt_client_retcode_t;

/**
 * Callback that can be used by the user of this library to configure the SSL context accordingly.
 * The global lock held by the qmc library is held while calling this callback.
 *
 * \param[in] ctx The ssl ctx to use.
 * \param[in] cookie Opaque cookie passed as argument to the original function that calls this callback.
 * \retval ::QMGMTCLIENT_OK in case the ctx was correctly configured, all other values result in an error.
 */
typedef qeo_mgmt_client_retcode_t (*qeo_mgmt_client_ssl_ctx_cb)(SSL_CTX *ctx, void *cookie);

/**
 * Opaque handle representing a qeo managment client context containing all the information needed to
 * handle actions.
 */
typedef struct qeo_mgmt_client_ctx_s qeo_mgmt_client_ctx_t;

/**
 * Initialize a new qeo mgmt client handle.
 *
 **************** WARNING *******************
 * This handle can be used by different threads at the same time, internally however a mutex is used
 * to block concurrent actions on it. This means you should be carefull when
 * executing api calls from different threads to make sure that you don't introduce deadlocks.
 *
 * \warning This function will result in a curl_global_init call (without SSL) which is not multi thread safe.
 *
 * \retval The new handle to be used in subsequent calls to this library.
 * \retval ::NULL In case an error occured.
 */
qeo_mgmt_client_ctx_t *qeo_mgmt_client_init(void);

/**
 * Enroll a new device to become part of the Qeo REALM for which the otp was created.
 *
 * There are 2 use cases of this API.
 * 1. Enroll based on a newly generated private key and a valid otp, Typically used to enroll an untrusted GUI device.
 * 2. Enroll based on a trusted device key/certificate without an otp. Typically used to enroll a trusted headless device.
 *
 * \param[in] ctx The management client ctx to use for performing this action.
 * \param[in] url base URL of the server to connect (this library will discovery the actual url to use itself.)
 * \param[in] pkey key used to identify this device
 * \param[in] otp One Time Password which can be used to enroll a new device to a specific realm.
 *                This otp can be left NULL in which case a trusted device certificate and private key must be used to
 *                generate the request.
 * \param[in] info Information about the device that is enrolled. (TODO discuss what to send/how and when)
 * \param[in] cb Callback called once before setting up the https connection.
 *               Client authentication is not required at this stage, it is however important to check the server certificate
 *               either via a fingerprint check or via a trust store.
 * \param[in] cookie Cookie which is transparently forwarded to the callback.
 * \param[in,out] certs A list of certificates representing the chain from device to master certificate. 
 *                      The caller has ownership of this argument so is responsible for freeing it afterwards.
 *                      The device certificate MUST be the first certificate in the list.
 *
 * \retval ::QMGMTCLIENT_OK in case enrollment succeeded, certs contains the resulting certificate chain.
 * \retval ::QMGMTCLIENT_EOTP in case the passed otp is invalid.
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_enroll_device(qeo_mgmt_client_ctx_t *ctx,
                                                        const char *url,
                                                        const EVP_PKEY *pkey,
                                                        const char *otp,
                                                        const qeo_platform_device_info *info,
                                                        qeo_mgmt_client_ssl_ctx_cb cb,
                                                        void *cookie,
                                                        STACK_OF(X509) *certs);

/**
 * Callback called whenever new data comes in.
 * The global lock held by the qmc library is held while calling this callback.
 *
 * \param data The data
 * \param size Size of the data
 * \param cookie Cookie passed to the function calling this callback
 *
 * \retval ::QMGMTCLIENT_OK to continue retrieving new data, all other return values result in an error.
 */
typedef qeo_mgmt_client_retcode_t (*qeo_mgmt_client_data_cb)(char *data, size_t size, void *cookie);

/**
 * Retrieve policy info from the server.
 * \param[in] ctx The management client ctx to use for performing this action.
 * \param[in] url base URL of the server to connect (this library will discovery the actual url to use itself.)
 * \param[in] ssl_cb Callback called once before setting up the https connection.
 *               Client authentication is required using the device certificate
 *               resulting from the enrollment.
 * \param[in] data_cb Callback called whenever new data comes in (stream based)
 * \param[in] cookie Cookie which is transparently forwarded to the callbacks.
 *
 * \retval ::QMGMTCLIENT_OK Successfully retrieved the policy file.
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_get_policy(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     qeo_mgmt_client_data_cb data_cb,
                                                     void *cookie);

/**
 * Based on the sequence number of the last received policy file, check whether the
 * policy file for this realm is still up to date with the version on the server.
 *
 * \param[in] ctx The management client ctx to use for performing this action.
 * \param[in] ssl_cb Callback called once before setting up the https connection.
 *               Client authentication is required using the device certificate
 *               resulting from the enrollment.
 * \param[in] ssl_cookie Cookie which is transparently forwarded to the callback.
 * \param[in] url base URL of the server to connect (this library will discovery the actual url to use itself.)
 * \param[in] sequence_nr The sequence number of the last retrieved policy file. Must be bigger than 0.
 * \param[in] realm_id The id of the realm to check for. Must be bigger than 0.
 * \param[out] result true in case the current policy file is still valid or false in case it
 *                     is no longer valid and needs to be retrieved back from the server.
 *                     Only use this param in case the function returned QMGMTCLIENT_OK.
 *
 * \retval ::QMGMTCLIENT_OK Successfully checked the policy state.
 * \retval ::QMGMTCLIENT_EBADREQUEST There is something wrong with the realmid
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_check_policy(qeo_mgmt_client_ctx_t *ctx,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie,
                                                     const char* url,
                                                     int64_t sequence_nr,
                                                     int64_t realm_id,
                                                     bool *result);
/**
 * Clean up all resources linked to a specific handle.
 *
 * \warning This function results in a curl_global_cleanup function call which is not multi thread safe.
 *
 * \param[in] The handle to clean all resources for.
 */
void qeo_mgmt_client_clean(qeo_mgmt_client_ctx_t *ctx);

/**
 * Ask ths management context to stop it's operations. Ongoing request will be stopped as soon as possible returning with errors.
 * 
 * \param[in] The handle to clean all resources for.
 */
void qeo_mgmt_client_ctx_stop(qeo_mgmt_client_ctx_t *ctx);

#endif /* QEO_MGMT_CLIENT_H_ */
