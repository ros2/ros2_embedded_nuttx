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

#ifndef CNHELPER_H_
#define CNHELPER_H_

#include <openssl/x509.h>
#include <qeo/device.h>
#include <qeo/mgmt_cert_parser.h>

X509_NAME* cnhelper_create_dn(const qeo_platform_device_info *info);

#endif /* CNHELPER_H_ */
