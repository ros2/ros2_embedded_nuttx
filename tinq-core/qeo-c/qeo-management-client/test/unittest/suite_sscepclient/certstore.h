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

#ifndef CERTSTORE_H_
#define CERTSTORE_H_
#include <openssl/x509.h>

#define CERTSTORE_MASTER 0
#define CERTSTORE_REALM 1
#define CERTSTORE_DEVICE 2
#define CERTSTORE_RANDOM 3

X509* get_cert(int id);

/**
 * ids is an array of ints ending with a -1 int
 */
STACK_OF(X509) *get_cert_store(int *ids);

#endif /* CERTSTORE_H_ */
