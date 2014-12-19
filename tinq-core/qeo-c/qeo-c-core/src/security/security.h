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

#ifndef SECURITY_H_
#define SECURITY_H_


/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#include <qeo/error.h>
#include <inttypes.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <qeo/types.h>
#include <qeo/mgmt_client.h>

/*########################################################################
#                                                                        #
#  TYPES/DEFINES SECTION                                                 #
#                                                                        #
########################################################################*/
#define FRIENDLY_NAME_FORMAT    "<rid:%" PRIx64 "><did:%" PRIx64 "><uid:%" PRIx64 ">"
#define IS_FINAL_SECURITY_STATE(state) \
    (state == QEO_SECURITY_AUTHENTICATION_FAILURE || state == QEO_SECURITY_AUTHENTICATED)

typedef struct qeo_security  *qeo_security_hndl;

/* qeo security states */
typedef enum {
    QEO_SECURITY_UNAUTHENTICATED,
    QEO_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS,
    QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY,
    QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED,
    QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE,
    QEO_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS,
    QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS,
    QEO_SECURITY_STORING_QEO_CREDENTIALS,
    QEO_SECURITY_AUTHENTICATION_FAILURE,
    QEO_SECURITY_AUTHENTICATED
} qeo_security_state;

/* callback which is triggered when the state is updated */
typedef void (*qeo_security_status_cb)(qeo_security_hndl qeoSec, qeo_security_state status);

/* set both realm_id and user_id to zero if you want to force a new registration procedure */
typedef struct {
    int64_t  realm_id;
    int64_t  device_id;
    int64_t  user_id;
    /* URL of the server */
    char     *url;
    /* The friendly name is a concatenation of all identity info and is used for the DDS participant name */
    char     *friendly_name;
} qeo_security_identity;

typedef struct {
    qeo_security_identity                         id;
    qeo_security_status_cb                        security_status_cb;
    void                                          *user_data;
} qeo_security_config;

/*########################################################################
#                                                                       #
#  API FUNCTION SECTION                                             #
#                                                                       #
########################################################################*/

/* init security module */
qeo_retcode_t qeo_security_init(void);

/* destroy security module */
qeo_retcode_t qeo_security_destroy(void);

/* construct security object*/
qeo_retcode_t qeo_security_construct(const qeo_security_config  *cfg,
                                     qeo_security_hndl          *qeoSec);

/* spawns a new thread */
qeo_retcode_t qeo_security_authenticate(qeo_security_hndl qeoSec);

/* can be called at all times */
qeo_retcode_t qeo_security_destruct(qeo_security_hndl *qeoSec);

/* can only be called when authenticated */
qeo_retcode_t qeo_security_get_credentials(qeo_security_hndl  qeoSec,
                                           EVP_PKEY           **key,
                                           STACK_OF(X509)     **certs);

/* Retrieve identity object associated with qeo security object */ 
qeo_retcode_t qeo_security_get_identity(qeo_security_hndl qeoSec, qeo_security_identity **id);

/* Free identity object */
qeo_retcode_t qeo_security_free_identity(qeo_security_identity **id);

/* retrieve an array with identities. TODO: RENAME THIS FUNCTION */
qeo_retcode_t qeo_security_get_realms(qeo_security_identity **id,
                                      unsigned int           *length);                  /* don't forget you have the responsibility to call qeo_security_free_realms() */

/* Free retrieved identitires. TODO: RENAME THIS FUNCTION */ 
qeo_retcode_t qeo_security_free_realms(qeo_security_identity **id, unsigned int length);

/* retrieve user data */
qeo_retcode_t qeo_security_get_user_data(qeo_security_hndl qeoSec,
                                         void              *user_data);

/* Retrieve qeo mgmt client ctx */
qeo_retcode_t qeo_security_get_mgmt_client_ctx(qeo_security_hndl qeoSec,
                                               qeo_mgmt_client_ctx_t **ctx);

#endif /* UTIL_H_ */
