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

#ifndef QEO_MGMT_CERT_PARSER_H_
#define QEO_MGMT_CERT_PARSER_H_

#include <openssl/x509.h>

/**
 * Enumeration of the different return values that are supported.
 */
typedef enum
{
    QCERT_OK,          /**< Success */
    QCERT_EFAIL,       /**< General failure */
} qeo_mgmt_cert_retcode_t;

/**
 * Structure representing all the contents of a certificate.
 */
typedef struct {
    int64_t realm;/**< The realm id as used on the server. */
    int64_t device;/**< The device id as used on the server. */
    int64_t user;/**< The user id as used on the server. */
} qeo_mgmt_cert_contents;

/**
 * Based on a chain of certificates, retrieve the information which is inside it.
 * This function does not verify the validity of the chain.
 * The chain does not need to be ordered.
 *
 * \pre This function assumes that openssl is already initialized in a proper way.
 *
 * \param[in] chain A list of certificates to parse.
 *                  This must be a chain pointing to a device certificate.
 * \param[out] contents Pointer to a struct that is filled in by this function based on the contents of the chain.
 *                      Fields that are not applicable are set to -1.
 * \retval ::QCERT_OK in case parsing succeeded.
 */
qeo_mgmt_cert_retcode_t qeo_mgmt_cert_parse(STACK_OF(X509) *chain,
                                                qeo_mgmt_cert_contents *contents);

#endif /* QEO_MGMT_CERT_PARSER_H_ */
