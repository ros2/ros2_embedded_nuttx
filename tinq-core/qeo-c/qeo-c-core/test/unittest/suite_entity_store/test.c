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

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "unittest/unittest.h"

#include "entity_store.h"
#include "list.h"

#include "Mockuser_data.h"

START_TEST(test_store_init_fini)
{
    fail_unless(QEO_EBADSTATE == entity_store_fini());
    fail_unless(QEO_OK == entity_store_init());
    fail_unless(QEO_EBADSTATE == entity_store_init());
    fail_unless(QEO_OK == entity_store_fini());
    fail_unless(QEO_EBADSTATE == entity_store_fini());
}
END_TEST

START_TEST(test_store_add)
{
    fail_unless(QEO_OK == entity_store_init());
    fail_unless(QEO_OK == entity_store_add((entity_t*)0xbabababa));
    fail_unless(QEO_OK == entity_store_fini());
}
END_TEST

START_TEST(test_store_add_error)
{
    fail_unless(QEO_EBADSTATE == entity_store_add((entity_t*)0xbabababa));
}
END_TEST

START_TEST(test_store_add_remove)
{
    fail_unless(QEO_OK == entity_store_init());
    fail_unless(QEO_OK == entity_store_add((entity_t*)0xbabababa));
    fail_unless(QEO_OK == entity_store_remove((entity_t*)0xbabababa));
    fail_unless(QEO_OK == entity_store_fini());
}
END_TEST

START_TEST(test_store_remove_error)
{
    fail_unless(QEO_EBADSTATE == entity_store_remove((entity_t*)0xbabababa));
    fail_unless(QEO_OK == entity_store_init());
    fail_unless(QEO_EBADSTATE == entity_store_remove((entity_t*)0xbabababa));
    fail_unless(QEO_OK == entity_store_fini());
}
END_TEST

START_TEST(test_update_user_data)
{
    qeo_factory_t factory = {};
    qeocore_reader_t reader = { .entity.flags.is_writer = 0 };
    qeocore_writer_t writer = { .entity.flags.is_writer = 1 };

    /* failues */
    fail_unless(QEO_EBADSTATE == entity_store_update_user_data(&factory));
    /* no failures */
    fail_unless(QEO_OK == entity_store_init());
    fail_unless(QEO_OK == entity_store_update_user_data(&factory));
    /* add reader */
    fail_unless(QEO_OK == entity_store_add(&reader.entity));
    reader_user_data_update_ExpectAndReturn(&reader, QEO_OK);
    fail_unless(QEO_OK == entity_store_update_user_data(&factory));
    /* add writer */
    fail_unless(QEO_OK == entity_store_add(&writer.entity));
    reader_user_data_update_ExpectAndReturn(&reader, QEO_OK);
    writer_user_data_update_ExpectAndReturn(&writer, QEO_OK);
    fail_unless(QEO_OK == entity_store_update_user_data(&factory));
    /* clean up */
    fail_unless(QEO_OK == entity_store_fini());
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    { .name = "init", .function = test_store_init_fini },
    { .name = "add", .function = test_store_add },
    { .name = "add - error", .function = test_store_add_error },
    { .name = "add - remove", .function = test_store_add_remove },
    { .name = "remove - error", .function = test_store_remove_error },
    { .name = "update user_data", .function = test_update_user_data },
    {NULL}
};

void register_entity_store_tests(Suite *s)
{
    TCase *tc = tcase_create("entity store tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_entity_store_tests, .name = "entity store" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "entity store",
        .desc = "entity store tests",
};

/* called before every test case starts */
static void init_tcase(void)
{
    Mockuser_data_Init();
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    Mockuser_data_Verify();
    Mockuser_data_Destroy();
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
