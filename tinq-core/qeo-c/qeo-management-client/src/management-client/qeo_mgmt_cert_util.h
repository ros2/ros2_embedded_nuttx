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

#ifndef QEO_MGMT_CERT_UTIL_H_
#define QEO_MGMT_CERT_UTIL_H_
#include <stdbool.h>
#include <openssl/x509.h>

/**
 * Take a certificate chain and order from leave to root.
 * \retval ::true in case the chain is ordered successfully.
 * \retval ::false in case an error occured.
 */
bool qeo_mgmt_cert_util_order_chain(STACK_OF(X509) *chain);

int64_t qeo_mgmt_util_hex_to_int(const char* hex);
#endif /* QEO_MGMT_CERT_UTIL_H_ */
