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

#include <qeocore/api.h>

#include "typesupport.h"
#include "Mockcore.h"

static qeo_factory_t *_factory = (qeo_factory_t *)0xdeadbeef;
static DDS_DomainParticipant _dp;

static void stub_init(void)
{
    _dp = DDS_DomainParticipantFactory_create_participant(0, NULL, NULL, 0);
}

static void stub_fini(void)
{
    DDS_DomainParticipant_delete_contained_entities(_dp);
    DDS_DomainParticipantFactory_delete_participant(_dp);
}

/* ===[ tests ]============================================================== */

typedef struct {
    char *string;
    int32_t i32;
} simple_t;

static const DDS_TypeSupport_meta _tsm_simple[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "simple_t",
      .flags = TSMFLAG_KEY|TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE, .size = sizeof(simple_t), .nelem = 2 },
    { .tc = CDR_TYPECODE_CSTRING, .name = "string", .label = 12345, .size = 0,
      .flags = TSMFLAG_DYNAMIC, .offset = offsetof(simple_t, string) },
    { .tc = CDR_TYPECODE_LONG, .name = "i32", .label = 67890,
      .flags = TSMFLAG_KEY, .offset = offsetof(simple_t, i32) },
};

START_TEST(test_construct_inargs)
{
    qeocore_type_t container = {0}, member = {0};
    qeocore_member_id_t id = QEOCORE_MEMBER_ID_DEFAULT;
    qeocore_enum_constants_t econst = DDS_SEQ_INITIALIZER(qeocore_enum_constant_t);

    /* init */
    stub_init();
    /* test */
    fail_unless(NULL == qeocore_type_register_tsm(NULL, _tsm_simple, "test"));
    fail_unless(NULL == qeocore_type_register_tsm(_factory, NULL, "test"));
    fail_unless(NULL == qeocore_type_register_tsm(_factory, _tsm_simple, NULL));
    container.flags.final = 1;
    fail_unless(QEO_EINVAL == qeocore_type_register(NULL, &container, "test"));
    fail_unless(QEO_EINVAL == qeocore_type_register(_factory, NULL, "test"));
    container.flags.final = 0;
    fail_unless(QEO_EINVAL == qeocore_type_register(_factory, &container, NULL));
    container.flags.final = 1;
    fail_unless(QEO_EINVAL == qeocore_type_register(_factory, &container, "test"));
    qeocore_type_free(NULL);
    fail_unless(NULL == qeocore_type_primitive_new(QEOCORE_TYPECODE_STRUCT));
    fail_unless(NULL == qeocore_type_primitive_new(QEOCORE_TYPECODE_STRING));
    fail_unless(NULL == qeocore_type_primitive_new(QEOCORE_TYPECODE_SEQUENCE));
    fail_unless(NULL == qeocore_type_sequence_new(NULL));
    fail_unless(NULL == qeocore_type_enum_new(NULL, &econst));
    fail_unless(NULL == qeocore_type_enum_new("enum", NULL));
    fail_unless(NULL == qeocore_type_enum_new("enum", &econst)); /* no constants in seq */
    fail_unless(NULL == qeocore_type_struct_new(NULL));
    container.flags.final = 0;
    member.flags.final = 0;
    fail_unless(QEO_EINVAL == qeocore_type_struct_add(NULL, &member, "test", &id, 0));
    fail_unless(QEO_EINVAL == qeocore_type_struct_add(&container, NULL, "test", &id, 0));
    fail_unless(QEO_EINVAL == qeocore_type_struct_add(&container, &member, NULL, &id, 0));
    container.flags.final = 1;
    member.flags.final = 1;
    fail_unless(QEO_EINVAL == qeocore_type_struct_add(&container, &member, "test", &id, 0));
    /* fini */
    stub_fini();
}
END_TEST

START_TEST(test_cache_refcnt)
{
    qeocore_type_t *ti1, *ti2;

    /* init */
    stub_init();
    core_register_type_IgnoreAndReturn(QEO_OK);
    core_unregister_type_IgnoreAndReturn(QEO_OK);
    /* test */
    ti1 = qeocore_type_register_tsm(_factory, _tsm_simple, "simple");
    fail_unless(NULL != ti1);
    ti2 = qeocore_type_register_tsm(_factory, _tsm_simple, "simple");
    fail_unless(ti1->u.tsm_based.ts == ti2->u.tsm_based.ts);
    /* fini */
    qeocore_type_free(ti1);
    qeocore_type_free(ti2);
    stub_fini();
}
END_TEST

START_TEST(test_dynamic_type)
{
    qeocore_type_t *structure = NULL, *sequence = NULL, *string = NULL, *primitive = NULL;
    qeocore_member_id_t id;

    /* init */
    stub_init();
    /* test */
    fail_unless(NULL != (structure = qeocore_type_struct_new("a_dynamic_struct")));
    fail_unless(NULL != (string = qeocore_type_string_new(0)));
    fail_unless(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    fail_unless(NULL != (sequence = qeocore_type_sequence_new(primitive)));
    qeocore_type_free(primitive);
    id = QEOCORE_MEMBER_ID_DEFAULT;
    fail_unless(QEO_OK == qeocore_type_struct_add(structure, string, "a_string", &id, QEOCORE_FLAG_KEY));
    qeocore_type_free(string);
    id = QEOCORE_MEMBER_ID_DEFAULT;
    fail_unless(QEO_OK == qeocore_type_struct_add(structure, sequence, "a_sequence", &id, QEOCORE_FLAG_NONE));
    qeocore_type_free(sequence);
    /* clean up */
    qeocore_type_free(structure);
    stub_fini();
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    { .name = "type construction input args", .function = test_construct_inargs },
    { .name = "type construction ref counting", .function = test_cache_refcnt },
    { .name = "dynamic type construction", .function = test_dynamic_type },
    {NULL}
};

void register_type_support_tests(Suite *s)
{
    TCase *tc = tcase_create("type support tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_type_support_tests, .name = "type support" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "type support",
        .desc = "type support tests",
};

/* called before every test case starts */
static void init_tcase(void)
{
    Mockcore_Init();
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    Mockcore_Verify();
    Mockcore_Destroy();
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
