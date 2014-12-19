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

#ifndef QEO_MGMT_CLIENT_FWD_H_
#define QEO_MGMT_CLIENT_FWD_H_

#include <qeo/mgmt_client.h>

/**
 * Type of locator
 */
typedef enum
{
    QMGMT_LOCATORTYPE_UNKNOWN, /**< the locator description for unknown locator types.*/   //!< QMGMT_LOCATORTYPE_UNKNOWN
    QMGMT_LOCATORTYPE_TCPV4, /**< a locator description of an TCP IPv4 reachable service *///!< QMGMT_LOCATORTYPE_TCPV4
    QMGMT_LOCATORTYPE_TCPV6, /**< a locator description of an TCP IPv6 reachable service *///!< QMGMT_LOCATORTYPE_TCPV6
    QMGMT_LOCATORTYPE_UDPV4, /**< a locator description of an UDP IPv4 reachable service *///!< QMGMT_LOCATORTYPE_UDPV4
    QMGMT_LOCATORTYPE_UDPV6, /**< a locator description of an UDP IPv6 reachable service *///!< QMGMT_LOCATORTYPE_UDPV6
} qeo_mgmt_client_locator_type_t;

/**
 * Structure representing a locator for a forwarder.
 * A forwarder can be located on one of its locators.
 */
typedef struct qeo_mgmt_client_locator_s
{
  qeo_mgmt_client_locator_type_t type;   /**< Specifies what type of locator this is. */
  char* address;                            /**< Specifies the address of the locator. This can be an ip address or a hostname. */
  int32_t port;                             /**< Specifies the port of the locator. */
} qeo_mgmt_client_locator_t;

/**
 * Structure representing a single forwarder device.
 */
typedef struct qeo_mgmt_client_forwarder_s
{
  int64_t deviceID; /** < the device ID  of the forwarder known the SMS. (The same id embedded in the device certificate) */
  u_int32_t nrOfLocators;/** < the nr of locators present on this forwarder. */
  qeo_mgmt_client_locator_t* locators;/** < Array of locators present on this forwarder. */
} qeo_mgmt_client_forwarder_t;

/**
 * Register this device as a Qeo forwarder for your realm.
 * This means your device needs to have a public ip address and must be reachable
 * on the URL provided to this function.
 *
 * \param[in] ctx The management client ctx to use for performing this action.
 * \param[in] url base URL of the server to connect (this library will discovery the actual url to use itself.)
 * \param[in] locators The locators where other devices can reach this forwarder. These locators must be reachable from the WAN.
 * \param[in] nrOfLocators, the total number of locators to register. If nrOfLocators is 0, then this device
 *               will be unregistered as forwarder.
 * \param[in] ssl_cb Callback called once before setting up the https connection.
 *               Client authentication is required using the device certificate
 *               resulting from the enrollment.
 * \param[in] cookie Cookie which is transparently forwarded to the callback.
 *
 * \retval ::QMGMTCLIENT_OK Successfully registered the forwarder.
 *  TODO: list other potential error codes
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_register_forwarder(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     qeo_mgmt_client_locator_t *locators,
                                                     u_int32_t nrOfLocators,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *cookie);


/**
 * Callback called for each found forwarder.
 * The global lock held by the qmc library is not held while calling this callback.
 *
 * \param[in] forwarder The description of a forwarder and its locators.
 *  The ownership of the pointer is transferred to the callback.
 *  It must free the memory using 'qeo_mgmt_client_free_forwarder' even if the callback returns an error.
 * \param[in] cookie Opaque cookie passed as argument to the original function that calls this callback.
 *
 * \retval ::QMGMTCLIENT_OK Successfully accepted the forwarder.
 *
 */
typedef qeo_mgmt_client_retcode_t (*qeo_mgmt_client_forwarder_cb)(qeo_mgmt_client_forwarder_t* forwarder, void *cookie);

/**
 * Callback called each time the get_forwarders function is called exactly once.
 * The global lock held by the qmc library is not held while calling this callback.
 *
 *
 * \param[in] result Indicates whether get_forwarders succeeded or not.
 * \param[in] cookie Opaque cookie passed as argument to the original function that calls this callback.
 *
 */
typedef void (*qeo_mgmt_client_forwarder_result_cb)(qeo_mgmt_client_retcode_t result, void *cookie);

/**
 * Retrieve the location information of the Qeo forwarder(s) for your realm.
 * This call will schedule the actual retrieving of the list in a background thread.
 * All arguments (except url) have to remain valid untill the result_cb is called.
 * No ownership of memory is taken by this function.
 * The caller of this function should release all memory
 * after/during the result callback is called.
 *
 * \param[in] ctx The management client ctx to use for performing this action.
 * \param[in] url URL of the server to connect to. This must be the same url as the one used for device enrollment.
 *                This url is copied by this function.
 * \param[in] fwd_cb Callback used to provide forwarder information to asynchronously in a background task.
 * \param[in] result_cb This optional callback will be called when the background task is finished.
 *                      This is guaranteed in case this function returns QMGMTCLIENT_OK.
 * \param[in] fwd_cookie Cookie which is transparently forwarded to forwarder and forwarder result callback.
 * \param[in] ssl_cb Callback called once before setting up the https connection.
 *               Client authentication is required using the device certificate
 *               resulting from the enrollment.
 * \param[in] ssl_cookie Cookie which is transparently forwarded to the SSL callback.
 *
 * \retval ::QMGMTCLIENT_OK Successfully retrieved the forwarder. This is mapped to the return value of the callback.
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_get_forwarders(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     qeo_mgmt_client_forwarder_cb fwd_cb,
                                                     qeo_mgmt_client_forwarder_result_cb result_cb,
                                                     void *fwd_cookie,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie);



/**
 * Retrieve the location information of the Qeo forwarder(s) for your realm. This is a blocking call.
 *
 * \param[in] ctx The management client ctx to use for performing this action.
 * \param[in] url URL of the server to connect to. This must be the same url as the one used for device enrollment.
 * \param[in] fwd_cb Callback used to provide forwarder information to.
 * \param[in] fwd_cookie Cookie which is transparently forwarded to forwarder callback.
 * \param[in] ssl_cb Callback called once before setting up the https connection.
 *               Client authentication is required using the device certificate
 *               resulting from the enrollment.
 * \param[in] ssl_cookie Cookie which is transparently forwarded to the SSL callback.
 *
 * \retval ::QMGMTCLIENT_OK If successfully retrieved the forwarder. All other return codes represent failures.
 *  This is mapped to the return values of the callbacks.
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_get_forwarders_sync(qeo_mgmt_client_ctx_t *ctx,
                                                     const char* url,
                                                     qeo_mgmt_client_forwarder_cb fwd_cb,
                                                     void *fwd_cookie,
                                                     qeo_mgmt_client_ssl_ctx_cb ssl_cb,
                                                     void *ssl_cookie);

/**
 *
 * \param[in] Free all memory consumed by this forwarder
 * \retval ::QMGMTCLIENT_OK always
 */
qeo_mgmt_client_retcode_t qeo_mgmt_client_free_forwarder(qeo_mgmt_client_forwarder_t* forwarder);

#endif /* QEO_MGMT_CLIENT_FWD_H_ */
