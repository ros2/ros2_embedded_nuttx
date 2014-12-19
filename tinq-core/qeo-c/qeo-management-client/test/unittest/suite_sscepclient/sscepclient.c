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

#include <qeo/log.h>
#include <qeo/mgmt_client.h>
#include "unittest/unittest.h"
#include "keygen.h"
#include "sscep_mock.h"
#include "certstore.h"

#define COOKIE_MAGIC_NUMBER 666
const char* s_url = "http://testurl";
const char* s_otp = "invalidotp";
static EVP_PKEY *s_rsakey;
static STACK_OF(X509) *s_certs;
static qeo_platform_device_info s_info;
static int raids[] = {CERTSTORE_MASTER, CERTSTORE_RANDOM, -1};
static int deviceids[] = {CERTSTORE_REALM, CERTSTORE_DEVICE, -1};
static int brokendeviceids[] = {CERTSTORE_RANDOM, CERTSTORE_DEVICE, -1};

static int s_ssl_cb_hitcount = 0;
static qeo_mgmt_client_retcode_t my_ssl_cb(SSL_CTX *ctx, void *cookie){
    ck_assert_ptr_eq(cookie, (void*) COOKIE_MAGIC_NUMBER);
    s_ssl_cb_hitcount++;
    return QMGMTCLIENT_OK;
}

/* ===[ public API tests ]=================================================== */
START_TEST(test_sscep_handling_init)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;

    sscep_mock_ignore_and_return(false, SCEP_PKISTATUS_FAILURE, NULL, SCEP_PKISTATUS_FAILURE, NULL);
    ctx = qeo_mgmt_client_init();
    fail_unless(ctx == NULL);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_FAILURE, NULL, SCEP_PKISTATUS_FAILURE, NULL);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);

    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 0, 1);
}
END_TEST

START_TEST(test_sscep_handling_perform_sunny)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;
    STACK_OF(X509) *racerts = get_cert_store(raids);
    STACK_OF(X509) *devicecerts = get_cert_store(deviceids);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_SUCCESS, racerts, SCEP_PKISTATUS_SUCCESS, devicecerts);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);

    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_OK);

    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 2, 1);
    sk_X509_free(racerts);
    sk_X509_free(devicecerts);
}
END_TEST

START_TEST(test_sscep_handling_perform_errors)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;
    STACK_OF(X509) *racerts = get_cert_store(raids);
    STACK_OF(X509) *devicecerts = get_cert_store(deviceids);
    STACK_OF(X509) *brokendevicecerts = get_cert_store(brokendeviceids);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_CONNECT, racerts, SCEP_PKISTATUS_FAILURE, devicecerts);
    ctx = qeo_mgmt_client_init();
    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_ECONNECT);
    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 1, 1);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_SUCCESS, racerts, SCEP_PKISTATUS_FAILURE, devicecerts);
    ctx = qeo_mgmt_client_init();
    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_EFAIL);
    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 2, 1);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_SUCCESS, racerts, SCEP_PKISTATUS_FORBIDDEN, devicecerts);
    ctx = qeo_mgmt_client_init();
    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_EOTP);
    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 2, 1);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_SUCCESS, racerts, SCEP_PKISTATUS_CONNECT, devicecerts);
    ctx = qeo_mgmt_client_init();
    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_ECONNECT);
    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 2, 1);

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_SUCCESS, racerts, SCEP_PKISTATUS_SUCCESS, brokendevicecerts);
    ctx = qeo_mgmt_client_init();
    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_EFAIL);
    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 2, 1);

    sk_X509_free(racerts);
    sk_X509_free(devicecerts);
}
END_TEST


START_TEST(test_sscep_handling_long_name)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;
    STACK_OF(X509) *racerts = get_cert_store(raids);
    STACK_OF(X509) *devicecerts = get_cert_store(deviceids);
    s_info.userFriendlyName = "extreeeeeeeeeeeeeeeemely loooooooooooooooooooooooooooong naaaaaaaaaaaaaaaaaaameeeeeeeeeeeee";

    sscep_mock_ignore_and_return(true, SCEP_PKISTATUS_SUCCESS, racerts, SCEP_PKISTATUS_SUCCESS, devicecerts);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);

    fail_unless(qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs) == QMGMTCLIENT_OK);

    qeo_mgmt_client_clean(ctx);
    sscep_mock_expect_called(1, 2, 1);
    sk_X509_free(racerts);
    sk_X509_free(devicecerts);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "test sscep handling init", .function = test_sscep_handling_init },
    { .name = "test sscep handling perform sunny", .function = test_sscep_handling_perform_sunny },
    { .name = "test sscep handling perform errors", .function = test_sscep_handling_perform_errors },
    { .name = "test sscep handling long name", .function = test_sscep_handling_long_name },
    {NULL}
};

void register_public_api_tests(Suite *s)
{
    TCase *tc = tcase_create("mocked sscep tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_public_api_tests, .name = "mocked sscep" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "sscep_mock",
        .desc = "unit tests of the qeo_mgmt_client with a mocked sscep",
};

/* called before every test case starts */
static void init_tcase(void)
{
    /* Add algorithms and init random pool */
    OpenSSL_add_all_algorithms();
    s_rsakey = keygen_create(1024);
    s_certs = sk_X509_new(NULL);
    memset(&s_info, 0, sizeof(s_info));
    s_info.userFriendlyName = "friendlytestdevice";
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    EVP_PKEY_free(s_rsakey);
    sk_X509_free(s_certs);
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
