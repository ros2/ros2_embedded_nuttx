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

#ifndef DEBUG
#define NDEBUG
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "unittest/unittest.h"

#include <qeo/util_error.h>
#include <qeo/platform.h>
#include "platform_api/platform_common.h"

START_TEST(test_device_info)
{
	const qeo_platform_device_info* qeo_dev_info = qeo_platform_get_device_info();
    ck_assert_int_ne(qeo_dev_info, NULL);
    ck_assert_str_ne(qeo_dev_info->manufacturer, "");
    ck_assert_str_ne(qeo_dev_info->modelName, "");
    ck_assert_str_ne(qeo_dev_info->productClass, "");
    ck_assert_str_ne(qeo_dev_info->softwareVersion, "");
    ck_assert_str_ne(qeo_dev_info->userFriendlyName, "");
    ck_assert_str_ne(qeo_dev_info->serialNumber, "");
    ck_assert_str_ne(qeo_dev_info->hardwareVersion, "");
    ck_assert_str_ne(qeo_dev_info->configURL, "");
    ck_assert_int_ne(qeo_dev_info->qeoDeviceId.upperId, 0);
    ck_assert_int_ne(qeo_dev_info->qeoDeviceId.lowerId, 0);
}
END_TEST



START_TEST(test_invalid_get_device_storage_path)
{
	char* filep = NULL;
    ck_assert_int_eq(qeo_platform_get_device_storage_path(NULL, NULL), QEO_UTIL_EINVAL);
    ck_assert_int_eq(qeo_platform_get_device_storage_path("test_file", NULL), QEO_UTIL_EINVAL);
    ck_assert_int_eq(qeo_platform_get_device_storage_path(NULL, &filep), QEO_UTIL_EINVAL);
}
END_TEST

START_TEST(test_valid_get_device_storage_path)
{
	char* filep = NULL;
	char* expected_filep = NULL;
	char* expected_dir_path = NULL;

    expected_dir_path = getenv("QEO_STORAGE_DIR");
    asprintf(&expected_filep, "%s/%s", expected_dir_path, "test_file");

    ck_assert_int_eq(qeo_platform_get_device_storage_path("test_file", &filep), QEO_UTIL_OK);

    ck_assert_str_eq(filep, expected_filep);
    free(filep);
    free(expected_filep);
}
END_TEST

static singleTestCaseInfo tests[] =
{
    { .name = "invalid input for qeo_get_device_info", .function = test_device_info  },
    { .name = "invalid input for qeo_get_device_storage_path", .function = test_invalid_get_device_storage_path },
    { .name = "valid input for qeo_get_device_storage_path", .function = test_valid_get_device_storage_path },
    {NULL}
};

void register_device_api_tests(Suite *s)
{
	TCase *tc = tcase_create("platform API tests");
	tcase_addtests(tc, tests);
    suite_add_tcase(s, tc);
}
