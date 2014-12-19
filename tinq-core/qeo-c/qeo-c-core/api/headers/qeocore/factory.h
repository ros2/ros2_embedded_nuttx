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

#ifndef QEOCORE_FACTORY_H_
#define QEOCORE_FACTORY_H_

#include <qeo/error.h>
#include <qeo/types.h>
#include <stdbool.h>
#include <inttypes.h>
#include <qeocore/identity.h>

/** \file
 * Qeo factory API
 */

/* ---[ initialization ]---------------------------------------------------- */

/**
 * The ID of a domain.
 */
typedef unsigned qeocore_domain_id_t;

/**
 * Callback function called by the factory when initialization is completed,
 * either successfully or not.
 *
 * \param[in] factory the factory
 * \param[in] success \c true if successfully initialzed, \c false if not
 */
typedef void (*qeocore_on_factory_init_done)(qeo_factory_t *, bool success);

/**
 * Callback function called by a forwarding factory when it needs a publicly
 * available locator address (IP:port).  To provide this locator the function
 * ::qeocore_fwdfactory_set_public_locator should be called either
 * synchronously or asynchronously.
 *
 * \param[in] factory the factory that needs the locator
 */
typedef void (*qeocore_on_fwdfactory_get_public_locator)(qeo_factory_t *factory);

/**
 * Factory listener callback structure.
 */
typedef struct {
    qeocore_on_factory_init_done on_factory_init_done; /**< see ::qeocore_on_factory_init_done */
    qeocore_on_fwdfactory_get_public_locator on_fwdfactory_get_public_locator; /**< see ::qeocore_on_fwdfactory_get_public_locator */
} qeocore_factory_listener_t;

/**
 * Default domain for Qeo.
 */
extern const qeocore_domain_id_t QEOCORE_DEFAULT_DOMAIN;

/**
 * Factory interface to create Qeo readers and writers. The factory instance
 * should be properly closed if none of the readers/writers created are needed
 * anymore to free allocated resources.
 *
 * \return The factory or \c NULL on failure.
 *
 * \see ::qeocore_factory_close
 */
qeo_factory_t *qeocore_factory_new(const qeo_identity_t *id);

/**
 * Close factory and release any resources associated with it.
 *
 * \warning Make sure that any readers and/or writers created with this factory
 *          have been closed before calling this function.
 *
 * \param[in] factory  The factory to be closed.
 */
void qeocore_factory_close(qeo_factory_t *factory);

/**
 * Initialize the Qeo factory.  This function should be called:
 * - after having set any of the (optional) configuration parameters;
 * - before using the factory to create readers and writers.
 *
 * It will create the DDS domain participant and a publisher/subscriber pair.
 *
 * \param[in] factory the factory to initialize
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the factory is already initialized
 * \retval ::QEO_EFAIL on failure
 */
qeo_retcode_t qeocore_factory_init(qeo_factory_t *factory, const qeocore_factory_listener_t *listener);

/**
 * Trigger a refresh of the security policy.  Note that this action will start
 * an asynchronous update of the security policy and that this process can
 * take some time to complete.
 *
 * \param[in] factory the factory for which to trigger the security policy
 *                    update
 *
 * \retval ::QEO_OK on successfully triggering the update
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the factory is not yet initialized
 */
qeo_retcode_t qeocore_factory_refresh_policy(const qeo_factory_t *factory);

/**
 * Configure the DDS domain ID to be used for Qeo.  Pass in
 * ::QEOCORE_DEFAULT_DOMAIN to use Qeo's default domain.  At run-time the domain
 * ID can be overwritten by setting the "QEO_DOMAIN_ID" environment variable
 * to the domain you want to use.
 *
 * \param[in] factory the factory to configure
 * \param[in] id the ID of the domain for which to initialize Qeo
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the factory is already initialized
 */
qeo_retcode_t qeocore_factory_set_domainid(qeo_factory_t *factory,
                                           qeocore_domain_id_t id);

/**
 * Get the DDS domain ID  used for Qeo.
 *
 * \param[in] factory the factory to configure
 * \param[out] id the ID of the domain 
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the factory is already initialized
 */
qeo_retcode_t qeocore_factory_get_domainid(qeo_factory_t *factory,
                                           qeocore_domain_id_t *id);

/**
 * Get the ID of the realm in which the factory is running.
 *
 * \param[in] factory the factory
 *
 * \return the realm ID or -1 when the input arguments are invalid
 */
int64_t qeocore_factory_get_realm_id(qeo_factory_t *factory);

/**
 * Get the ID of the user in which the factory is running.
 *
 * \param[in] factory the factory
 *
 * \return the user ID or -1 when the input arguments are invalid
 */
int64_t qeocore_factory_get_user_id(qeo_factory_t *factory);

/**
 * Get the realm url of the management server in which the factory is running.
 *
 * \param[in] factory the factory
 *
 * \return the url or NULL when the input arguments are invalid
 */
const char * qeocore_factory_get_realm_url(qeo_factory_t *factory);


/**
 * Configure which network interfaces to use for Qeo.  This needs to be done
 * before a call to ::qeocore_factory_init.  By default (if not calling this
 * function) all network interfaces will be used.
 *
 * \param[in] factory the factory to configure
 * \param[in] interfaces A colon separated list of interface names
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 * \retval ::QEO_EBADSTATE when the factory is already initialized
 */
qeo_retcode_t qeocore_factory_set_intf(qeo_factory_t *factory,
                                       const char *interfaces);

qeo_retcode_t qeocore_factory_set_tcp_server(qeo_factory_t *factory,
                                             const char *tcp_server);
qeo_retcode_t qeocore_factory_set_local_tcp_port(qeo_factory_t *factory,
                                                 const char *local_port);

qeo_retcode_t qeocore_factory_set_user_data(qeo_factory_t *factory,
                                       uintptr_t userdata);

qeo_retcode_t qeocore_factory_get_user_data(qeo_factory_t *factory,
                                       uintptr_t *userdata);

/**
 * Returns a Qeo factory instance with forwarding capability that can be used for creating Qeo readers and
 * writers.  The factory instance should be properly closed if none of the
 * readers and/or writers that have been created, are needed anymore.  This will
 * free any allocated resources associated with that factory.
 *
 * \param[in] cb callback function (see ::qeocore_on_fwdfactory_get_public_locator)
 * \param[in] local_port the local TCP port to be used
 *
 * \return The factory or \c NULL on failure.
 *
 * \see ::qeocore_fwdfactory_close
 */
qeo_factory_t *qeocore_fwdfactory_new(qeocore_on_fwdfactory_get_public_locator cb,
                                      const char *local_port);

/**
 * Close factory and release any resources associated with it.
 *
 * \param[in] factory the factory to be closed
 */
void qeocore_fwdfactory_close(qeo_factory_t *factory);

/**
 * Update a forwarding factory's publicly available locator (IP address and port).
 *
 * \param[in] factory the factory to configure
 * \param[in] ip_address the publicly available IP address (IPv4 dotted-decimal)
 * \param[in] port the publicly available TCP port
 *
 * \retval ::QEO_OK on success
 * \retval ::QEO_EINVAL when the input arguments are invalid
 */
qeo_retcode_t qeocore_fwdfactory_set_public_locator(qeo_factory_t *factory,
                                                    const char *ip_address,
                                                    int port);

/**
 * Callback prototype of the function to be called when shutting down the DDS
 * messaging thread.
 *
 * \see ::qeocore_atexit
 */
typedef void (*qeocore_exit_cb)(void);

/**
 * Register a function that needs to be called when when shutting down the DDS
 * messaging thread.
 */
void qeocore_atexit(const qeocore_exit_cb cb);

/** 
 * Get the number of factories currently created 
 */ 
int qeocore_get_num_factories(); 


#endif /* QEOCORE_FACTORY_H_ */
