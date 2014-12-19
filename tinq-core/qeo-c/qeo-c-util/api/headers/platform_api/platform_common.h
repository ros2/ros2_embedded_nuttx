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
 * Qeo device API common (private).
 */

#ifndef QEO_PLATFORM_COMMON_H_
#define QEO_PLATFORM_COMMON_H_

#include <qeo/device.h>
#include <qeo/platform_security.h>


/**
 * This function prints the device info struct in a pretty manner.
 *
 * \param[in] qeo_dev_info a pointer to a qeo_platform_device_info struct
 */
void qeo_pprint_device_info(const qeo_platform_device_info* qeo_dev_info);

/**
 * Returns a human-readable version of the qeo_platform_security_state
 *
 */
const char *platform_security_state_to_string(qeo_platform_security_state state);

/**
 * Returns a human-readable version of the qeo_platform_security_state_reason
 *
 */
const char *platform_security_state_reason_to_string(qeo_platform_security_state_reason reason);

/**
 * Like strdup but will return NULL if NULL is passed as an argument (rather than crashing) 
 *
 */
char* qeo_strdup_ret(const char* ret_str);
#endif /* QEO_PLATFORM_COMMON_H_ */
