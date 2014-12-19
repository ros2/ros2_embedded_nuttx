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

#ifndef _MOCKQEO_PLATFORM_IMPL_H
#define _MOCKQEO_PLATFORM_IMPL_H

#include "Mockplatform.h"

#include <qeo/util_error.h>

extern qeo_util_retcode_t _qeo_platform_get_device_storage_path_rc;

qeo_util_retcode_t qeo_platform_get_device_storage_path_callback(const char* file_name, char** full_storage_path, int cmock_num_calls);

#endif
