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

#include <qeo/factory.h>
#include <qeocore/api.h>
#include <qeocore/identity.h>

#include "core.h"

#include "Mocksamplesupport.h"
#include "Mockdds_aux.h"
#include "Mocksecurity.h"
#include "Mockpolicy.h"
#include "Mockentity_store.h"
#include "Mockuser_data.h"
#include "Mockforwarder.h"

static void dummy_fn(){

}

/* ===[ binding API tests ]================================================== */

START_TEST(test_type_api_inargs)
{
    DDS_TypeSupport_meta  tsm = {0};
    qeo_factory_t f = {{0}};
    qeocore_type_t t = {0}, m = {0};
    qeocore_member_id_t id = QEOCORE_MEMBER_ID_DEFAULT;

    fail_unless(NULL == qeocore_type_register_tsm(NULL, &tsm, "test"));
    fail_unless(NULL == qeocore_type_register_tsm(&f, NULL, "test"));
    fail_unless(NULL == qeocore_type_register_tsm(&f, &tsm, NULL));
    fail_unless(QEO_OK != qeocore_type_register(NULL, &t, "test"));
    fail_unless(QEO_OK != qeocore_type_register(&f, NULL, "test"));
    fail_unless(QEO_OK != qeocore_type_register(&f, &t, NULL));
    qeocore_type_free(NULL);
    fail_unless(NULL == qeocore_type_sequence_new(NULL));
    fail_unless(NULL == qeocore_type_struct_new(NULL));
    fail_unless(QEO_OK != qeocore_type_struct_add(NULL, &m, "test", &id, 0));
    fail_unless(QEO_OK != qeocore_type_struct_add(&t, NULL, "test", &id, 0));
    fail_unless(QEO_OK != qeocore_type_struct_add(&t, &m, "test", &id, 0));
    t.u.intermediate.tc = QEOCORE_TYPECODE_STRUCT;
    fail_unless(QEO_OK != qeocore_type_struct_add(&t, &m, "test", &id, 0));
    m.u.intermediate.dtype = (DDS_DynamicType)0xdeadbeef;
    fail_unless(QEO_OK != qeocore_type_struct_add(&t, &m, NULL, &id, 0));
    fail_unless(QEO_OK != qeocore_type_struct_add(&t, &m, "test", NULL, 0));
    /* non final type */
    fail_unless(QEO_EINVAL == qeocore_type_get_member_id(&t, "test", &id));
    /* final type */
    t.flags.final = 1;
    fail_unless(QEO_EINVAL == qeocore_type_get_member_id(NULL, "test", &id));
    fail_unless(QEO_EINVAL == qeocore_type_get_member_id(&t, NULL, &id));
    fail_unless(QEO_EINVAL == qeocore_type_get_member_id(&t, "test", NULL));
    /* TSM based type */
    t.flags.tsm_based = 1;
    fail_unless(QEO_EINVAL == qeocore_type_get_member_id(&t, "test", &id));
}
END_TEST

START_TEST(test_data_api_inargs)
{
    qeocore_data_t d = { { 0 } };
    void *v = (void *)0xdeadbeef;

    fail_unless(QEO_OK != qeocore_data_reset(NULL));
    qeocore_data_free(NULL);
    fail_unless(QEO_OK != qeocore_data_set_member(NULL, 0, v));
    fail_unless(QEO_OK != qeocore_data_set_member(&d, 0, NULL));
    fail_unless(QEO_OK != qeocore_data_get_member(NULL, 0, v));
    fail_unless(QEO_OK != qeocore_data_get_member(&d, 0, NULL));
    fail_unless(QEOCORE_ERROR == qeocore_data_get_status(NULL));
    qeocore_data_get_instance_handle(NULL);
}
END_TEST

START_TEST(test_qr_api_inargs)
{
    qeo_factory_t f = {{0}};
    qeocore_type_t t = {0};
    qeocore_data_t d = { { 0 } };
    qeocore_reader_t r = { { 0 } };
    qeo_retcode_t rc;

    /* do not crash when no rc pointer provided */
    fail_unless(NULL == qeocore_reader_open(NULL, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL));
    fail_unless(NULL == qeocore_reader_open(NULL, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    fail_unless(NULL == qeocore_reader_open(&f, NULL, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    /* non-final type */
    fail_unless(NULL == qeocore_reader_open(&f, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    /* keying issues during creation */
    t.flags.final = 1;
    t.flags.keyed = 1;
    fail_unless(NULL == qeocore_reader_open(&f, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    t.flags.keyed = 0;
    fail_unless(NULL == qeocore_reader_open(&f, &t, NULL, QEOCORE_EFLAG_STATE_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    fail_unless(NULL == qeocore_reader_open(&f, &t, NULL, QEOCORE_EFLAG_STATE_UPDATE, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    qeocore_reader_close(NULL);
    /* disabled reader */
    d.rw.reader = &r;
    r.entity.flags.enabled = 0;
    fail_unless(QEO_EBADSTATE == qeocore_reader_read(&r, NULL, &d));
    fail_unless(QEO_EBADSTATE == qeocore_reader_take(&r, NULL, &d));
    /* enabled reader */
    r.entity.flags.enabled = 1;
    fail_unless(QEO_EINVAL == qeocore_reader_enable(NULL));
    fail_unless(QEO_EBADSTATE == qeocore_reader_enable(&r));
    fail_unless(0 == qeocore_reader_get_userdata(NULL));
    fail_unless(NULL == qeocore_reader_data_new(NULL));
    fail_unless(QEO_EINVAL == qeocore_reader_read(NULL, NULL, &d));
    fail_unless(QEO_EINVAL == qeocore_reader_read(&r, NULL, NULL));
    fail_unless(QEO_EINVAL == qeocore_reader_take(NULL, NULL, &d));
    fail_unless(QEO_EINVAL == qeocore_reader_take(&r, NULL, NULL));
    fail_unless(QEO_EINVAL == qeocore_reader_policy_update(NULL));
    /* reader mismatch */
    d.rw.reader = &r - 1;
    fail_unless(QEO_EINVAL == qeocore_reader_read(&r, NULL, &d));
    fail_unless(QEO_EINVAL == qeocore_reader_take(&r, NULL, &d));
}
END_TEST

START_TEST(test_qw_api_inargs)
{
    qeo_factory_t f = {{0}};
    qeocore_type_t t = {0};
    qeocore_data_t d = { { 0 } };
    qeocore_writer_t w = { .entity.type_info = &t };
    qeo_retcode_t rc;

    /* do not crash when no rc pointer provided */
    fail_unless(NULL == qeocore_writer_open(NULL, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL));
    fail_unless(NULL == qeocore_writer_open(NULL, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    fail_unless(NULL == qeocore_writer_open(&f, NULL, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    /* non-final type */
    fail_unless(NULL == qeocore_writer_open(&f, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    /* keying issues during creation */
    t.flags.final = 1;
    t.flags.keyed = 1;
    fail_unless(NULL == qeocore_writer_open(&f, &t, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    t.flags.keyed = 0;
    fail_unless(NULL == qeocore_writer_open(&f, &t, NULL, QEOCORE_EFLAG_STATE_DATA, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    fail_unless(NULL == qeocore_writer_open(&f, &t, NULL, QEOCORE_EFLAG_STATE_UPDATE, NULL, &rc));
    fail_unless(QEO_EINVAL == rc);
    qeocore_writer_close(NULL);
    /* disabled reader */
    d.rw.writer = &w;
    w.entity.flags.enabled = 0;
    fail_unless(QEO_EBADSTATE == qeocore_writer_write(&w, &d));
    fail_unless(QEO_EBADSTATE == qeocore_writer_remove(&w, &d));
    /* enabled writer */
    w.entity.flags.enabled = 1;
    fail_unless(QEO_EINVAL == qeocore_writer_enable(NULL));
    fail_unless(QEO_EBADSTATE == qeocore_writer_enable(&w));
    fail_unless(NULL == qeocore_writer_data_new(NULL));
    fail_unless(QEO_EINVAL == qeocore_writer_write(NULL, &d));
    fail_unless(QEO_EINVAL == qeocore_writer_write(&w, NULL));
    fail_unless(QEO_EINVAL == qeocore_writer_remove(NULL, &d));
    fail_unless(QEO_EINVAL == qeocore_writer_remove(&w, NULL));
    fail_unless(QEO_EINVAL == qeocore_writer_policy_update(NULL));
    /* writer mismatch */
    d.rw.writer = &w - 1;
    fail_unless(QEO_EINVAL == qeocore_writer_write(&w, &d));
    fail_unless(QEO_EINVAL == qeocore_writer_remove(&w, &d));
}
END_TEST

START_TEST(test_factory_api_inargs)
{
    qeo_factory_t f_i = { .flags = { .initialized = 1 }};
    qeo_factory_t f_u = { .flags = { .initialized = 0 }};

    qeocore_factory_listener_t listener = {
        .on_factory_init_done = (qeocore_on_factory_init_done)dummy_fn
    };

    qeocore_atexit(NULL);
    qeocore_factory_close(NULL);
    fail_unless(NULL == qeocore_factory_new((qeo_identity_t*)0xdeadbeef));
    fail_unless(QEO_EINVAL == qeocore_factory_init(NULL, NULL));
    fail_unless(QEO_EBADSTATE == qeocore_factory_init(&f_i, &listener));
    fail_unless(QEO_EINVAL == qeocore_factory_set_domainid(NULL, 0));
    fail_unless(QEO_EBADSTATE == qeocore_factory_set_domainid(&f_i, 0));
    fail_unless(QEO_EINVAL == qeocore_factory_set_intf(NULL, "eth0"));
    fail_unless(QEO_EBADSTATE == qeocore_factory_set_intf(&f_i, "eth0"));
    fail_unless(QEO_EINVAL == qeocore_factory_set_intf(&f_u, NULL));
    fail_unless(QEO_EINVAL == qeocore_factory_refresh_policy(NULL));
    fail_unless(QEO_EBADSTATE == qeocore_factory_refresh_policy(&f_u));
}
END_TEST

START_TEST(test_enum_api_inargs)
{
    DDS_TypeSupport_meta non_enum_tsm[] = {
        { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool",  },
    };
    DDS_TypeSupport_meta enum_tsm[] = {
        { .tc = CDR_TYPECODE_ENUM, .name = "enum", .nelem = 3 },
        { .name = "ZERO", .label = 0 },
        { .name = "ONE", .label = 1 },
        { .name = "TWO", .label = 2 },
    };
    qeo_factory_t *factory;
    qeocore_type_t *enum_type;
    qeocore_type_t *non_enum_type_1;
    qeocore_type_t *non_enum_type_2;
    qeocore_enum_constants_t ec = DDS_SEQ_INITIALIZER(qeocore_enum_constant_t);
    qeocore_member_id_t id = QEOCORE_MEMBER_ID_DEFAULT;
    qeo_enum_value_t value;
    char name[16];
    int i;

    entity_store_init_IgnoreAndReturn(QEO_OK);
    entity_store_add_IgnoreAndReturn(QEO_OK);
    entity_store_fini_IgnoreAndReturn(QEO_OK);
    fwd_destroy_Ignore();
    qeo_security_policy_destroy_IgnoreAndReturn(QEO_OK);
    qeo_security_destroy_IgnoreAndReturn(QEO_OK);
    /* create dynamic types */
    fail_unless(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_OPEN)));
    fail_unless(DDS_RETCODE_OK == dds_seq_require(&ec, 3));
    DDS_SEQ_ITEM(ec, 0).name = "ZERO";
    DDS_SEQ_ITEM(ec, 1).name = "ONE";
    DDS_SEQ_ITEM(ec, 2).name = "TWO";
    fail_unless(NULL != (enum_type = qeocore_type_enum_new("xyz", &ec)));
    dds_seq_cleanup(&ec);
    fail_unless(NULL != (non_enum_type_1 = qeocore_type_primitive_new(QEOCORE_TYPECODE_BOOLEAN)));
    fail_unless(NULL != (non_enum_type_2 = qeocore_type_struct_new("abc")));
    fail_unless(QEO_OK == qeocore_type_struct_add(non_enum_type_2, non_enum_type_1, "b", &id, QEOCORE_FLAG_NONE));
    fail_unless(QEO_OK == qeocore_type_register(factory, non_enum_type_2, "org.qeo.system.RegistrationRequest"));
    /* enumeration constant conversions */
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(NULL, NULL, 0, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(enum_tsm, enum_type, 0, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(NULL, NULL, "ZERO", &value));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(enum_tsm, enum_type, "ZERO", &value));
    /* enumeration constant conversions (tsm based) */
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(non_enum_tsm, NULL, 1, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(enum_tsm, NULL, 0, NULL, 0));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(enum_tsm, NULL, -1, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(enum_tsm, NULL, enum_tsm->nelem, name, sizeof(name)));
    fail_unless(QEO_ENOMEM == qeocore_enum_value_to_string(enum_tsm, NULL, 0, name, 0));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(non_enum_tsm, NULL, "ZERO", &value));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(enum_tsm, NULL, NULL, &value));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(enum_tsm, NULL, "ONE", NULL));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(enum_tsm, NULL, "THREE", &value));
    /* success cases */
    for (i = 0; i < enum_tsm->nelem; i++) {
        fail_unless(QEO_OK == qeocore_enum_value_to_string(enum_tsm, NULL, enum_tsm[i+1].label, name, sizeof(name)));
        fail_unless(0 == strcmp(enum_tsm[i+1].name, name));
        fail_unless(QEO_OK == qeocore_enum_string_to_value(enum_tsm, NULL, enum_tsm[i+1].name, &value));
        fail_unless(enum_tsm[i+1].label == value);
    }
    /* enumeration constant conversions (dynamic) */
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(NULL, non_enum_type_1, 1, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(NULL, non_enum_type_2, 1, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(NULL, enum_type, 0, NULL, 0));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(NULL, enum_type, -1, name, sizeof(name)));
    fail_unless(QEO_EINVAL == qeocore_enum_value_to_string(NULL, enum_type, enum_tsm->nelem, name, sizeof(name)));
    fail_unless(QEO_ENOMEM == qeocore_enum_value_to_string(NULL, enum_type, 0, name, 0));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(NULL, non_enum_type_1, "ZERO", &value));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(NULL, non_enum_type_2, "ZERO", &value));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(NULL, enum_type, NULL, &value));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(NULL, enum_type, "ONE", NULL));
    fail_unless(QEO_EINVAL == qeocore_enum_string_to_value(NULL, enum_type, "THREE", &value));
    /* success cases */
    for (i = 0; i < enum_tsm->nelem; i++) {
        fail_unless(QEO_OK == qeocore_enum_value_to_string(NULL, enum_type, enum_tsm[i+1].label, name, sizeof(name)));
        fail_unless(0 == strcmp(enum_tsm[i+1].name, name));
        fail_unless(QEO_OK == qeocore_enum_string_to_value(NULL, enum_type, enum_tsm[i+1].name, &value));
        fail_unless(enum_tsm[i+1].label == value);
    }
    /* clean up */
    qeocore_type_free(enum_type);
    qeocore_type_free(non_enum_type_1);
    qeocore_type_free(non_enum_type_2);
    qeo_factory_close(factory);
}
END_TEST

START_TEST(test_misc_api_inargs)
{
    fail_unless(-1 == qeo_policy_identity_get_uid(NULL));
}
END_TEST

START_TEST(test_misc_api)
{
    const char *v = qeo_version_string();
    fail_unless(NULL != v);
    fail_unless(NULL == strstr(v, "UNKNOWN"), "invalid version string '%s'", v);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    { .name = "type API input args", .function = test_type_api_inargs },
    { .name = "data API input args", .function = test_data_api_inargs },
    { .name = "reader API input args", .function = test_qr_api_inargs },
    { .name = "writer API input args", .function = test_qw_api_inargs },
    { .name = "factory API input args", .function = test_factory_api_inargs },
    { .name = "enum API input args", .function = test_enum_api_inargs },
    { .name = "misc API input args", .function = test_misc_api_inargs },
    { .name = "misc tests", .function = test_misc_api },
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
    Mocksamplesupport_Init();
    Mockdds_aux_Init();
    Mocksecurity_Init();
    Mockpolicy_Init();
    Mockentity_store_Init();
    Mockuser_data_Init();
    Mockforwarder_Init();
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    Mocksamplesupport_Verify();
    Mocksamplesupport_Destroy();
    Mockdds_aux_Verify();
    Mockdds_aux_Destroy();
    Mocksecurity_Verify();
    Mocksecurity_Destroy();
    Mockpolicy_Verify();
    Mockpolicy_Destroy();
    Mockentity_store_Verify();
    Mockentity_store_Destroy();
    Mockuser_data_Verify();
    Mockuser_data_Destroy();
    Mockforwarder_Verify();
    Mockforwarder_Destroy();
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
