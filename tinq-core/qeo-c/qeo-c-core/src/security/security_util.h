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

#ifndef SECURITY_UTIL_H_
#define SECURITY_UTIL_H_

#include <openssl/ssl.h>

void security_util_configure_ssl_ctx(SSL_CTX *ssl_ctx);

void dump_openssl_error_stack(const char *msg);

#endif /* SECURITY_UTIL_H_ */
