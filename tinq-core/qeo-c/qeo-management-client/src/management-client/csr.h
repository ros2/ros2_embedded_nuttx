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

#ifndef CSR_H_
#define CSR_H_

#include "cnhelper.h"

/**
 * Create a certificate request based on a private key,
 * @return
 */
X509_REQ* csr_create(EVP_PKEY *key, const char* otc, const qeo_platform_device_info *info);

#endif /* CSR_H_ */
