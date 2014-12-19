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
#include <stdbool.h>

#include "unittest/unittest.h"

#include <qeocore/api.h>
#include <qeocore/identity.h>

#include "core.h"

#include "Mocksamplesupport.h"
#include "Mocksecurity.h"
#include "Mockpolicy.h"
#include "Mockforwarder.h"
#include "Mockentity_store.h"
#include "Mockuser_data.h"

#define QEO_SEC_HNDL (qeo_security_hndl)0xcafebabe
#define QEO_SEC_POL_HNDL (qeo_security_policy_hndl)0xdeadbabe

static qeo_factory_t *_f = NULL;
static bool _f_success = true;
static void on_factory_init_done(qeo_factory_t *factory, bool success)
{
    fail_unless(_f == factory);
    fail_unless(_f_success == success);
}

static qeocore_factory_listener_t _listener = {
    .on_factory_init_done = (qeocore_on_factory_init_done)on_factory_init_done
};

static qeo_security_identity _sec_identity = {
    .device_id = 0,
    .realm_id = 0,
    .user_id = 0,
    .url = "http://test/url",
    .friendly_name = "test frienly name"
};

static unsigned int _qeo_security_construct_cb_num_expected = 0;
static qeo_retcode_t _qeo_security_construct_ret = QEO_OK;
static qeo_security_config _sec_cfg;
static qeo_retcode_t qeo_security_construct_cb(const qeo_security_config* cfg, qeo_security_hndl* qeoSec, int cmock_num_calls)
{
    _qeo_security_construct_cb_num_expected--;

    printf("received security config\n");
    memcpy(&_sec_cfg, cfg, sizeof(_sec_cfg));

    *qeoSec = (_qeo_security_construct_ret == QEO_OK) ? QEO_SEC_HNDL : NULL;

    return _qeo_security_construct_ret;
}

static unsigned int _qeo_security_policy_construct_cb_num_expected = 0;
static qeo_retcode_t _qeo_security_policy_construct_ret = QEO_OK;
static qeo_security_policy_config _sec_pol_cfg;
static qeo_retcode_t qeo_security_policy_construct_cb(const qeo_security_policy_config* cfg, qeo_security_policy_hndl* qeoSecPol, int cmock_num_calls)
{
    _qeo_security_policy_construct_cb_num_expected--;

    memcpy(&_sec_pol_cfg, cfg, sizeof(_sec_pol_cfg));

    *qeoSecPol = (_qeo_security_policy_construct_ret == QEO_OK) ? QEO_SEC_POL_HNDL : NULL;

    return _qeo_security_policy_construct_ret;
}

static unsigned int _qeo_security_destruct_cb_num_expected = 0;
static qeo_retcode_t qeo_security_destruct_cb(qeo_security_hndl* qeoSec, int cmock_num_calls)
{
    _qeo_security_destruct_cb_num_expected--;

    fail_unless(*qeoSec == QEO_SEC_HNDL);

    return QEO_OK;
}

static unsigned int _qeo_security_authenticate_cb_num_expected = 0;
static qeo_retcode_t _qeo_security_authenticate_ret = QEO_OK;
static qeo_retcode_t qeo_security_authenticate_cb(qeo_security_hndl qeoSec, int cmock_num_calls)
{
    _qeo_security_authenticate_cb_num_expected--;

    return _qeo_security_authenticate_ret ;
}

static unsigned int _qeo_security_get_user_data_cb_num_expected = 0;
static qeo_retcode_t _qeo_security_get_user_data_ret = QEO_OK;
static qeo_retcode_t qeo_security_get_user_data_cb(qeo_security_hndl qeoSec, void* user_data, int cmock_num_calls)
{
    _qeo_security_get_user_data_cb_num_expected--;

    fail_unless(qeoSec == QEO_SEC_HNDL);

    *(void **)user_data = _sec_cfg.user_data;

    return _qeo_security_get_user_data_ret;
}

static unsigned int _qeo_security_get_identity_cb_num_expected = 0;
static qeo_retcode_t _qeo_security_get_identity_ret = QEO_OK;
static qeo_retcode_t qeo_security_get_identity_cb(qeo_security_hndl qeoSec, qeo_security_identity** id, int cmock_num_calls)
{
    _qeo_security_get_identity_cb_num_expected--;

    fail_unless(qeoSec == QEO_SEC_HNDL);
    *id = &_sec_identity;

    return _qeo_security_get_identity_ret;
}

static unsigned int _qeo_security_get_credentials_cb_num_expected = 0;
static qeo_retcode_t _qeo_security_get_credentials_ret = QEO_OK;
static qeo_retcode_t qeo_security_get_credentials_cb(qeo_security_hndl qeoSec, EVP_PKEY** key, STACK_OF(X509)** certs, int cmock_num_calls)
{
    _qeo_security_get_credentials_cb_num_expected--;

    fail_unless(qeoSec == QEO_SEC_HNDL);

    *key = (EVP_PKEY*)0xdeadbabe;
    *certs = (STACK_OF(X509)*)0xdeadbeef;

    return _qeo_security_get_credentials_ret;
}

/* ===[ factory tests ]================================================== */

START_TEST(test_factory_open_new_close_success)
{
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_OPEN));
    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_open_new_init_close_success)
{
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_OPEN));
    fail_unless(QEO_OK == qeocore_factory_init(_f, &_listener));
    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_internal_open_new_init_close_success)
{
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_OPEN));
    fail_unless(QEO_OK == qeocore_factory_init(_f, &_listener));

    core_get_open_domain_factory();

    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_new_close_success)
{
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_init_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_policy_init_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_DEFAULT));
    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_new_init_close_security_construct_failure)
{
    _f_success = false;

    fwd_init_pre_auth_IgnoreAndReturn(QEO_OK);
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_init_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_policy_init_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    _qeo_security_construct_cb_num_expected = 1;
    _qeo_security_construct_ret = QEO_EBADSTATE;
    qeo_security_construct_StubWithCallback(qeo_security_construct_cb);

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_DEFAULT));
    fail_unless(QEO_EBADSTATE == qeocore_factory_init(_f, &_listener));
    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_new_init_close_security_authenticate_failure)
{
    _f_success = false;

    fwd_init_pre_auth_IgnoreAndReturn(QEO_OK);
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_init_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_policy_init_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    _qeo_security_construct_cb_num_expected = 1;
    qeo_security_construct_StubWithCallback(qeo_security_construct_cb);
    _qeo_security_authenticate_cb_num_expected = 1;
    _qeo_security_authenticate_ret = QEO_EFAIL;
    qeo_security_authenticate_StubWithCallback(qeo_security_authenticate_cb);
    _qeo_security_destruct_cb_num_expected = 1;
    qeo_security_destruct_StubWithCallback(qeo_security_destruct_cb);

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_DEFAULT));
    fail_unless(QEO_EFAIL == qeocore_factory_init(_f, &_listener));
    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_new_init_close_security_status_authentication_failure)
{
    _f_success = false;

    fwd_init_pre_auth_IgnoreAndReturn(QEO_OK);
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_init_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_policy_init_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    _qeo_security_construct_cb_num_expected = 1;
    qeo_security_construct_StubWithCallback(qeo_security_construct_cb);
    _qeo_security_authenticate_cb_num_expected = 1;
    qeo_security_authenticate_StubWithCallback(qeo_security_authenticate_cb);
    _qeo_security_get_user_data_cb_num_expected = 1;
    qeo_security_get_user_data_StubWithCallback(qeo_security_get_user_data_cb);

    _qeo_security_destruct_cb_num_expected = 1;
    qeo_security_destruct_StubWithCallback(qeo_security_destruct_cb);

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_DEFAULT));
    fail_unless(QEO_OK == qeocore_factory_init(_f, &_listener));

    _sec_cfg.security_status_cb(QEO_SEC_HNDL, QEO_SECURITY_AUTHENTICATION_FAILURE);

    qeocore_factory_close(_f);
}
END_TEST

START_TEST(test_factory_new_init_close_policy_construct_failure)
{
    _f_success = false;

    fwd_init_pre_auth_IgnoreAndReturn(QEO_OK);
    entity_store_init_ExpectAndReturn(QEO_OK);
    entity_store_fini_ExpectAndReturn(QEO_OK);
    qeo_security_init_ExpectAndReturn(QEO_OK);
    qeo_security_destroy_ExpectAndReturn(QEO_OK);
    qeo_security_policy_init_ExpectAndReturn(QEO_OK);
    qeo_security_policy_destroy_ExpectAndReturn(QEO_OK);
    fwd_destroy_Ignore();

    _qeo_security_construct_cb_num_expected = 1;
    qeo_security_construct_StubWithCallback(qeo_security_construct_cb);
    _qeo_security_authenticate_cb_num_expected = 1;
    qeo_security_authenticate_StubWithCallback(qeo_security_authenticate_cb);
    _qeo_security_get_user_data_cb_num_expected = 1;
    qeo_security_get_user_data_StubWithCallback(qeo_security_get_user_data_cb);
    _qeo_security_get_identity_cb_num_expected = 1;
    qeo_security_get_identity_StubWithCallback(qeo_security_get_identity_cb);
    _qeo_security_get_credentials_cb_num_expected = 1;
    qeo_security_get_credentials_StubWithCallback(qeo_security_get_credentials_cb);
    _qeo_security_policy_construct_cb_num_expected = 1;
    _qeo_security_policy_construct_ret = QEO_EINVAL;
    qeo_security_policy_construct_StubWithCallback(qeo_security_policy_construct_cb);

    qeo_security_free_identity_IgnoreAndReturn(QEO_OK);

    _qeo_security_destruct_cb_num_expected = 1;
    qeo_security_destruct_StubWithCallback(qeo_security_destruct_cb);

    ck_assert_int_ne(NULL, _f =  qeocore_factory_new(QEO_IDENTITY_DEFAULT));
    fail_unless(QEO_OK == qeocore_factory_init(_f, &_listener));

    _sec_cfg.security_status_cb(QEO_SEC_HNDL, QEO_SECURITY_AUTHENTICATED);

    qeocore_factory_close(_f);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    { .name = "factory for open identity : new, close",
        .function = test_factory_open_new_close_success },
    { .name = "factory for open identity : new, init, close",
        .function = test_factory_open_new_init_close_success },
    { .name = "factory for open identity + internal factory: new, init, close",
        .function = test_factory_internal_open_new_init_close_success },
    { .name = "factory for default identity : new, close", .function = test_factory_new_close_success },
    { .name = "factory for default identity : new, init, close ; security construct failure",
        .function = test_factory_new_init_close_security_construct_failure },
    { .name = "factory for default identity : new, init, close ; security authenticate failure",
        .function = test_factory_new_init_close_security_authenticate_failure },
    { .name = "factory for default identity : new, init, close ; security status authentication failure",
        .function = test_factory_new_init_close_security_status_authentication_failure },
    { .name = "factory for default identity : new, init, close ; policy construct failure",
        .function = test_factory_new_init_close_policy_construct_failure },
    {NULL}
};

void register_factory_tests(Suite *s)
{
    TCase *tc = tcase_create("factory tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_factory_tests, .name = "factory" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "factory",
        .desc = "factory tests",
};

/* called before every test case starts */
static void init_tcase(void)
{
    Mocksamplesupport_Init();
    Mocksecurity_Init();
    Mockpolicy_Init();
    Mockentity_store_Init();
    Mockuser_data_Init();
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    ck_assert_int_eq(0, _qeo_security_construct_cb_num_expected);
    ck_assert_int_eq(0, _qeo_security_destruct_cb_num_expected);
    ck_assert_int_eq(0, _qeo_security_authenticate_cb_num_expected);
    ck_assert_int_eq(0, _qeo_security_get_user_data_cb_num_expected);
    ck_assert_int_eq(0, _qeo_security_get_identity_cb_num_expected);
    ck_assert_int_eq(0, _qeo_security_get_credentials_cb_num_expected);
    ck_assert_int_eq(0, _qeo_security_policy_construct_cb_num_expected);

    Mocksamplesupport_Verify();
    Mocksamplesupport_Destroy();
    Mocksecurity_Verify();
    Mocksecurity_Destroy();
    Mockpolicy_Verify();
    Mockpolicy_Destroy();
    Mockentity_store_Verify();
    Mockentity_store_Destroy();
    Mockuser_data_Verify();
    Mockuser_data_Destroy();
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
