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

#include "unittest/unittest.h"

#include <qeo/api.h>

/* ===[ public API tests ]=================================================== */

START_TEST(test_factory_api_inargs)
{
    qeo_factory_close(NULL); /* don't crash */
    fail_unless(NULL == qeo_factory_create_by_id((qeo_identity_t*)0xdeadbeef));
}
END_TEST

START_TEST(test_event_api_inargs)
{
    qeo_factory_t *f = (qeo_factory_t *)0xdeadbeef;
    qeo_event_writer_t *w = (qeo_event_writer_t *)0xdeadbeef;
    qeo_event_reader_listener_t rcbs = {0};
    qeo_event_writer_listener_t wcbs = {0};
    DDS_TypeSupport_meta  tsm = {0};
    void *data = (void *)0xdeadbeef;

    /* reader */
    fail_unless(NULL == qeo_factory_create_event_reader(NULL, &tsm, &rcbs, 0));
    fail_unless(NULL == qeo_factory_create_event_reader(f, NULL, &rcbs, 0));
    fail_unless(NULL == qeo_factory_create_event_reader(f, &tsm, NULL, 0));
    fail_unless(NULL == qeo_factory_create_event_reader(f, &tsm, &rcbs, 0)); /* on_data non-NULL */
    qeo_event_reader_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_event_reader_policy_update(NULL));
    /* writer */
    fail_unless(NULL == qeo_factory_create_event_writer(NULL, &tsm, NULL, 0));
    fail_unless(NULL == qeo_factory_create_event_writer(f, NULL, &wcbs, 0));
    fail_unless(NULL == qeo_factory_create_event_writer(f, &tsm, &wcbs, 0)); /* on_policy_update non-NULL */
    fail_unless(QEO_EINVAL == qeo_event_writer_write(NULL, data));
    fail_unless(QEO_EINVAL == qeo_event_writer_write(w, NULL));
    qeo_event_writer_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_event_writer_policy_update(NULL));
}
END_TEST

START_TEST(test_state_api_inargs)
{
    qeo_factory_t *f = (qeo_factory_t *)0xdeadbeef;
    qeo_state_reader_t *r = (qeo_state_reader_t *)0xdeadbeef;
    qeo_state_writer_t *w = (qeo_state_writer_t *)0xdeadbeef;
    qeo_iterate_callback cb = (qeo_iterate_callback)0xdeadbeef;
    qeo_state_reader_listener_t rcbs1 = {0};
    qeo_state_change_reader_listener_t rcbs2 = {0};
    qeo_state_writer_listener_t wcbs = {0};
    DDS_TypeSupport_meta  tsm = {0};
    void *data = (void *)0xdeadbeef;

    /* reader */
    fail_unless(NULL == qeo_factory_create_state_reader(NULL, &tsm, &rcbs1, 0));
    fail_unless(NULL == qeo_factory_create_state_reader(f, NULL, &rcbs1, 0));
    fail_unless(NULL == qeo_factory_create_state_reader(f, &tsm, NULL, 0));
    fail_unless(NULL == qeo_factory_create_state_reader(f, &tsm, &rcbs1, 0)); /* on_update non-NULL */
    fail_unless(QEO_EINVAL == qeo_state_reader_foreach(NULL, cb, 0));
    fail_unless(QEO_EINVAL == qeo_state_reader_foreach(r, NULL, 0));
    qeo_state_reader_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_state_reader_policy_update(NULL));
    /* change reader */
    fail_unless(NULL == qeo_factory_create_state_change_reader(NULL, &tsm, &rcbs2, 0));
    fail_unless(NULL == qeo_factory_create_state_change_reader(f, NULL, &rcbs2, 0));
    fail_unless(NULL == qeo_factory_create_state_change_reader(f, &tsm, NULL, 0));
    fail_unless(NULL == qeo_factory_create_state_change_reader(f, &tsm, &rcbs2, 0)); /* on_data non-NULL */
    qeo_state_change_reader_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_state_change_reader_policy_update(NULL));
    /* writer */
    fail_unless(NULL == qeo_factory_create_state_writer(NULL, &tsm, NULL, 0));
    fail_unless(NULL == qeo_factory_create_state_writer(f, NULL, &wcbs, 0));
    fail_unless(NULL == qeo_factory_create_state_writer(f, &tsm, &wcbs, 0)); /* on_policy_update non-NULL */
    fail_unless(QEO_EINVAL == qeo_state_writer_write(NULL, data));
    fail_unless(QEO_EINVAL == qeo_state_writer_write(w, NULL));
    fail_unless(QEO_EINVAL == qeo_state_writer_remove(NULL, data));
    fail_unless(QEO_EINVAL == qeo_state_writer_remove(w, NULL));
    qeo_state_writer_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_state_writer_policy_update(NULL));
}
END_TEST

START_TEST(test_util_api_inargs)
{
    DDS_TypeSupport_meta non_enum_type[] = {
        { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool",  },
    };
    DDS_TypeSupport_meta enum_type[] = {
        { .tc = CDR_TYPECODE_ENUM, .name = "enum", .nelem = 1 },
        { .name = "ZERO", .label = 0 },
    };
    qeo_enum_value_t val;
    char buf[16];

    /* qeo_enum_value_to_string */
    fail_unless(QEO_EINVAL == qeo_enum_value_to_string(NULL, 0, buf, sizeof(buf)));
    fail_unless(QEO_EINVAL == qeo_enum_value_to_string(non_enum_type, 0, buf, sizeof(buf)));
    fail_unless(QEO_EINVAL == qeo_enum_value_to_string(enum_type, 0, NULL, 0));
    fail_unless(QEO_EINVAL == qeo_enum_value_to_string(enum_type, -1, buf, sizeof(buf)));
    fail_unless(QEO_EINVAL == qeo_enum_value_to_string(enum_type, enum_type->nelem, buf, sizeof(buf)));
    fail_unless(QEO_ENOMEM == qeo_enum_value_to_string(enum_type, 0, buf, 0));
    /* qeo_enum_string_to_value */
    fail_unless(QEO_EINVAL == qeo_enum_string_to_value(NULL, "ZERO", &val));
    fail_unless(QEO_EINVAL == qeo_enum_string_to_value(non_enum_type, "ZERO", &val));
    fail_unless(QEO_EINVAL == qeo_enum_string_to_value(enum_type, "ONE", NULL));
    fail_unless(QEO_EINVAL == qeo_enum_string_to_value(enum_type, "ONE", &val));
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "factory API input args", .function = test_factory_api_inargs },
    { .name = "event API input args", .function = test_event_api_inargs },
    { .name = "state API input args", .function = test_state_api_inargs },
    { .name = "util API input args", .function = test_util_api_inargs },
    {NULL}
};

void register_type_support_tests(Suite *s)
{
    TCase *tc = tcase_create("API tests");
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
