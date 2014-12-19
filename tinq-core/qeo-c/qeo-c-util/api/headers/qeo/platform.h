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

#ifndef QEO_PLATFORM_H_
#define QEO_PLATFORM_H_
#include <qeo/util_error.h>

/**
* Get the full storage path given a file name to store your file.
* It is the responsibility of the caller to free full_storage_path.
*
* \param[in]  file_name a pointer to a file name string
* \param[out] full_storage_path a pointer to a full storage path pointer
*
* \retval ::QEO_OK on success
* \retval ::QEO_EINVAL in case of invalid arguments
* \retval ::QEO_ENOMEM in case it failed to allocate memory for the output
* */
qeo_util_retcode_t qeo_platform_get_device_storage_path(const char* file_name, char** full_storage_path);

/**
 * Get the path to the CA certificates to be used for peer verification.
 * At least one of the two arguments needs to be provided.
 *
 * \param[out] ca_file If not \c NULL this will be used to return the path of
 *                     the file that contains a list of CA certificates in PEM
 *                     format (if configured).
 * \param[out] ca_path If not \c NULL this will be used to return the path of
 *                     the directory containing CA certificates in PEM format
 *                     (if configured).
 */
qeo_util_retcode_t qeo_platform_get_cacert_path(const char** ca_file,
                                                const char** ca_path);

/**
 * This function will set a key with it's value in the persistent storage framework
 *
 * \param[in] key the unique identifier of the data
 * \param[in] value The value beloning to the key
 *
 */
qeo_util_retcode_t qeo_platform_set_key_value(const char *key, char *value); 

/**
 * This function will get the value belonging to a key from the persistent storage framework
 *
 * \param[in] key the unique identifier of the data
 * \param[in] value The value beloning to the key
 *
 */
qeo_util_retcode_t qeo_platform_get_key_value(const char *key, char **value); 


typedef struct qeo_der_certificate {
    long size;
    char* cert_data;
} qeo_der_certificate;

typedef qeo_util_retcode_t (*qeo_platform_custom_certificate_validator)(qeo_der_certificate*, int);

qeo_util_retcode_t qeo_platform_set_custom_certificate_validator(qeo_platform_custom_certificate_validator validator_function);
qeo_platform_custom_certificate_validator qeo_platform_get_custom_certificate_validator();

#endif 
