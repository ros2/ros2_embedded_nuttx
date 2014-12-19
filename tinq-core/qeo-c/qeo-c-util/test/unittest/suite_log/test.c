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

//
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "unittest/unittest.h"

#include <qeo/util_error.h>
#include <qeo/log.h>

unsigned int my_test_logger_ctr = 0;

static void my_test_logger(qeo_loglvl_t lvl, const char* fileName, const char* functionName, int lineNumber, const char *format, ...)
{
    my_test_logger_ctr++;
}

/* ===[ public API tests ]=================================================== */

START_TEST(test_log)
{
    // default logger
    qeo_log_e("Test error");
    qeo_log_w("Test warning");
    qeo_log_d("Test debug");
    qeo_log_i("Test info");

    // our own logger
    my_test_logger_ctr = 0;
    qeo_log_set_logger(my_test_logger);

    qeo_log_e("Test error");
    ck_assert_int_eq(my_test_logger_ctr, 1);
    qeo_log_w("Test warning");
    ck_assert_int_eq(my_test_logger_ctr, 2);
    qeo_log_d("Test debug");

#if DEBUG == 1
    ck_assert_int_eq(my_test_logger_ctr, 3);
#else
    ck_assert_int_eq(my_test_logger_ctr, 2);
#endif

    qeo_log_i("Test info");

#if DEBUG == 1
    ck_assert_int_eq(my_test_logger_ctr, 4);
#else
    ck_assert_int_eq(my_test_logger_ctr, 2);
#endif

    // no logger
    my_test_logger_ctr = 0;
    qeo_log_set_logger(NULL);
    qeo_log_e("Test error");
    qeo_log_w("Test warning");
    qeo_log_d("Test debug");
    qeo_log_i("Test info");
    ck_assert_int_eq(my_test_logger_ctr, 0);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "set logger", .function = test_log },
    {NULL}
};

void register_type_support_tests(Suite *s)
{
    TCase *tc = tcase_create("log tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_type_support_tests, .name = "LOG" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "LOG",
        .desc = "LOG tests",
};

/* called before every test case starts */
static void init_tcase(void)
{

}

/* called after every test case finishes */
static void fini_tcase(void)
{

}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
