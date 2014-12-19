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
 * Qeo device API.
 */

#ifndef QEO_DEVICE_H_
#define QEO_DEVICE_H_

#include <inttypes.h>
#include <qeo/util_error.h>

/**
 * DeviceID structure. DeviceID is a UUID of 128 bit.
 */
typedef struct
{
    int64_t upperId; /**< Highest 64 bit of the Qeo Device UUID */
    int64_t lowerId; /**< Lowest  64 bit of the Qeo Device UUID */
} qeo_platform_device_id;

/**
 * DeviceInfo structure.
 */
typedef struct
{
    qeo_platform_device_id qeoDeviceId; /**< DeviceID is a UUID of 128 bit */
    const char *manufacturer; /**< The device manufacturer, e.g., Dell Inc. */
    const char *modelName; /**< Model name of the device (human readable string) */
    const char *productClass; /**< Identifier of the class of product for which the serial number applies */
    const char *serialNumber; /**< Identifier of the particular device that is unique for the indicated class of product and manufacturer. */
    const char *hardwareVersion; /**< A string identifying the particular CPE model and version */
    const char *softwareVersion; /**< A string identifying the software version currently installed in the device */
    const char *userFriendlyName; /**< Descriptive name of the device */
    const char *configURL; /**< URL for configuration page that allows to view and/or configure  device settings */

} qeo_platform_device_info;

/**
 * Get the device info.
 * The returned pointer MUST NOT be free()'d.
 *
 * \retval A pointer to the device info structure
 * */
const qeo_platform_device_info *qeo_platform_get_device_info(void);


#endif 
