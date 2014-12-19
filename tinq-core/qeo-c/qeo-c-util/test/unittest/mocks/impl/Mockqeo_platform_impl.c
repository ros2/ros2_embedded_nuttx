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

#define _GNU_SOURCE
#include <stdio.h>

#include "Mockplatform_impl.h"

#include <unistd.h>
#include <sys/stat.h>

#define STORAGE_DIR "/tmp/files/test/"

static const char* get_device_storage_path()
{
    if (access(STORAGE_DIR, R_OK | W_OK) != 0) {
        if (mkdir(STORAGE_DIR, 0770) != 0) {
            return NULL ;
        }
    }

    return STORAGE_DIR;
}

qeo_util_retcode_t _qeo_platform_get_device_storage_path_rc = QEO_UTIL_OK;

qeo_util_retcode_t qeo_platform_get_device_storage_path_callback(const char* file_name, char** full_storage_path, int cmock_num_calls)
{
    ck_assert_int_ne(file_name, NULL);
    ck_assert_int_ne(full_storage_path, NULL);

    if (_qeo_platform_get_device_storage_path_rc != QEO_UTIL_OK) {
        return _qeo_platform_get_device_storage_path_rc;
    }

    if ((asprintf(full_storage_path, "%s%s", get_device_storage_path(), file_name) < 0)
            || (*full_storage_path == NULL )) {
        return QEO_UTIL_ENOMEM;
    }


    return _qeo_platform_get_device_storage_path_rc;
}

