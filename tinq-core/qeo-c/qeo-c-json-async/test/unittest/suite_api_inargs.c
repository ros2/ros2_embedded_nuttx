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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <assert.h>
#include "check.h"

#include "unittest/unittest.h"

#include <qeo/jsonasync.h>

/* ===[ public API tests ]=================================================== */

START_TEST(test_async_invargs)
{
    qeo_json_async_close(NULL); /* don't crash */
    fail_unless(NULL == qeo_json_async_create(NULL, 0));
    fail_unless(QEO_EINVAL == qeo_json_async_call(NULL, NULL, NULL));
    fail_unless(QEO_EINVAL == qeo_json_async_call((qeo_json_async_ctx_t *)0xdeadbeef, NULL, NULL));
    fail_unless(QEO_EINVAL == qeo_json_async_call((qeo_json_async_ctx_t *)0xdeadbeef, (const char *) 0xdeadbeef, (const char *) NULL));
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "async invalid input args", .function = test_async_invargs },
    {NULL}
};

void register_type_support_tests(Suite *s)
{
    TCase *tc = tcase_create("API input args tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);

}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_type_support_tests, .name = "API" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "API",
        .desc = "API tests",
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
