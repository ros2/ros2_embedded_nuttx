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

#ifndef QEO_OPENSSL_ENGINE_H
#define QEO_OPENSSL_ENGINE_H
/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/

/**
 * This header file must be implemented by every Qeo-openssl implementation.
 *
 */

#include "openssl/engine.h"
#include <qeo/openssl_engine_error.h>
/*########################################################################
#                                                                       #
#  TYPES SECTION                                             #
#                                                                       #
########################################################################*/

#define QEO_OPENSSL_ENGINE_CMD_BASE           (ENGINE_CMD_BASE+10)
#define QEO_OPENSSL_ENGINE_CMD_LOAD_CERT_CHAIN      (QEO_OPENSSL_ENGINE_CMD_BASE+ 0)
#define QEO_OPENSSL_ENGINE_CMD_SAVE_CREDENTIALS      (QEO_OPENSSL_ENGINE_CMD_BASE+ 1)
#define QEO_OPENSSL_ENGINE_CMD_GET_FRIENDLY_NAMES      (QEO_OPENSSL_ENGINE_CMD_BASE+ 2)

typedef struct 
{
        const char *friendlyName; /*IN*/
        STACK_OF(X509) *chain;   /*OUT*/
} qeo_openssl_engine_cmd_load_cert_chain_t;

typedef struct 
{
        const char *friendlyName; /*IN*/
         EVP_PKEY *pkey;
        STACK_OF(X509) *chain;   /*OUT*/
} qeo_openssl_engine_cmd_save_credentials_t;

typedef struct
{
    char **friendly_names; /* OUT (array of strings) */
    unsigned int number_of_friendly_names; /* OUT number of aliases */
} qeo_openssl_engine_cmd_get_friendly_names_t;

/*########################################################################
#                                                                       #
#  API FUNCTION SECTION                                             #
#                                                                       #
########################################################################*/



/**
 * Init openssl engine
 */
qeo_openssl_engine_retcode_t qeo_openssl_engine_init(void);

/**
 * Destroy openssl engine
 */
void qeo_openssl_engine_destroy(void);

/**
 * This function will allocate a string (to be freed by the caller) which contains a string representation of the OpenSSL - engine to be used 
 *
 * \param[out] engine_id Will contain the engine-id. Caller has responsbility to free the memory.
 *
 * \retval ::QEO_OPENSSL_ENGINE_UTIL_OK on success
 * \retval ::QEO_OPENSSL_ENGINE_EINVAL in case of invalid arguments
 * \retval ::QEO_OPENSSL_ENGINE_ENOMEM when no memory could be allocated
 * 
 */
qeo_openssl_engine_retcode_t qeo_openssl_engine_get_engine_id(char **engine_id); /* to be free()'d */

#endif
