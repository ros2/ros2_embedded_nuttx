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
#include "libsscep/curl_util.h"
#include "curl_easy_mock.h"

#define COOKIE_MAGIC_NUMBER 666
static int s_ssl_cb_hitcount = 0;
static int s_data_cb_hitcount = 0;
const char* s_url = "http://testurl";
const char* s_otp = "invalidotp";
static EVP_PKEY *s_rsakey;
static STACK_OF(X509) *s_certs;
static qeo_platform_device_info s_info;

size_t _write_to_memory_cb( char *buffer, size_t size, size_t nmemb, void *outstream);
size_t _read_from_memory_cb(char *buffer, size_t size, size_t nmemb, void *instream);
size_t _header_function( void *ptr, size_t size, size_t nmemb, char *cid);

static qeo_mgmt_client_retcode_t my_ssl_cb(SSL_CTX *ctx, void *cookie){
    ck_assert((long) cookie == COOKIE_MAGIC_NUMBER);
    s_ssl_cb_hitcount++;
    return QMGMTCLIENT_OK;
}

static qeo_mgmt_client_retcode_t my_data_cb(char *ptr, size_t sz, void *cookie)
{
    ck_assert((long) cookie == COOKIE_MAGIC_NUMBER);
    s_data_cb_hitcount++;
    return QMGMTCLIENT_OK;
}
/* ===[ public API tests ]=================================================== */

START_TEST(test_inv_args_enroll)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(NULL, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, NULL, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, NULL, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, NULL, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, NULL, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, NULL, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, NULL));
    ck_assert_int_eq(QMGMTCLIENT_EOTP, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, "15!#@$@#$", &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    s_info.userFriendlyName = NULL;
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));

    qeo_mgmt_client_clean(NULL);
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, 0, 0, 0, 0, 1);
}
END_TEST

START_TEST(test_inv_args_policy)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_get_policy(NULL, s_url, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_get_policy(ctx, NULL, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_get_policy(ctx, s_url, NULL, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_get_policy(ctx, s_url, my_ssl_cb, NULL, (void*) COOKIE_MAGIC_NUMBER));

    qeo_mgmt_client_clean(NULL);
    curl_easy_mock_expect_called(1, 0, 0, 0, 0, 0);
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, 0, 0, 0, 0, 1);
}
END_TEST

START_TEST(test_inv_args_check_policy)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;
    bool result = false;

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_check_policy(NULL, NULL, NULL, s_url, 0, 0, &result));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_check_policy(ctx, NULL, NULL, NULL, 0, 0, &result));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_check_policy(ctx, NULL, NULL, s_url, -1, 0, &result));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_check_policy(ctx, NULL, NULL, s_url, 0, -1, &result));
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_check_policy(ctx, NULL, NULL, s_url, 0, 0, NULL));

    qeo_mgmt_client_clean(NULL);
    curl_easy_mock_expect_called(1, 0, 0, 0, 0, 0);
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, 0, 0, 0, 0, 1);
}
END_TEST

START_TEST(test_curl_handling_init)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;

    curl_easy_mock_ignore_and_return(CURLE_FAILED_INIT, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx == NULL);

    curl_easy_mock_ignore_and_return(CURLE_OK, false, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx == NULL);

}
END_TEST

START_TEST(test_curl_handling_device)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_FAILED_INIT, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, 1, 0, 0, 1, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_FAILED_INIT, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_EFAIL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 2, 2, 2, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_SSL_CIPHER);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_ESSL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 0, 1, 1, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    ck_assert(ctx != NULL);
    ck_assert_int_eq(QMGMTCLIENT_EFAIL, qeo_mgmt_client_enroll_device(ctx, s_url, s_rsakey, s_otp, &s_info, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_certs));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, CURL_EASY_MOCK_CHECK_CALLED, 2, 2, 1);
}
END_TEST

START_TEST(test_curl_handling_policy)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_FAILED_INIT, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_get_policy(ctx, s_url, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, 1, 0, 0, 1, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_SSL_CERTPROBLEM);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_ESSL, qeo_mgmt_client_get_policy(ctx, s_url, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 0, 1, 1, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_COULDNT_CONNECT);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_ECONNECT, qeo_mgmt_client_get_policy(ctx, s_url, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 0, 1, 1, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_FAILED_INIT);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EFAIL, qeo_mgmt_client_get_policy(ctx, s_url, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 0, 1, 1, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_FAILED_INIT, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_OK, qeo_mgmt_client_get_policy(ctx, s_url, my_ssl_cb, my_data_cb, (void*) COOKIE_MAGIC_NUMBER));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 2, 2, 2, 1);
    /* Callbacks need to be called once */
    ck_assert_int_eq(s_ssl_cb_hitcount, 5);
    ck_assert_int_eq(s_data_cb_hitcount, 1);
}
END_TEST

START_TEST(test_curl_handling_checkpolicy)
{
    qeo_mgmt_client_ctx_t *ctx = NULL;
    bool result = false;

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_FAILED_INIT, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EINVAL, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 0, 0, 2, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_FAILED_INIT, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EFAIL, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 2, 2, 4, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_FAILED_INIT);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EFAIL, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 0, 1, 2, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EFAIL, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 3, 2, 4, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    curl_easy_mock_return_getinfo(400,1);
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_EBADREQUEST, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 3, 2, 4, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    curl_easy_mock_return_getinfo(410,1);//410 Gone
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_OK, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    fail_unless(result == false);
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 2, 2, 4, 1);

    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ctx = qeo_mgmt_client_init();
    curl_easy_mock_return_getinfo(304,1);//304 Not Modified
    fail_if(ctx == NULL);
    ck_assert_int_eq(QMGMTCLIENT_OK, qeo_mgmt_client_check_policy(ctx, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER, s_url, 0, 0, &result));
    fail_unless(result == true);
    qeo_mgmt_client_clean(ctx);
    curl_easy_mock_expect_called(1, CURL_EASY_MOCK_CHECK_CALLED, 2, 2, 4, 1);
}
END_TEST

/*
 * TODO, keep this lined up with the implementation in curl_util
 */
typedef struct {
    char* data;
    ssize_t offset;
    ssize_t length;
} qmgmt_curl_data_helper;

START_TEST(test_curl_download_memory)
{
    qmgmt_curl_data_helper helper={0};
    char *data=NULL;
    size_t len = 0;

    for (len = 0x2; len <= 0x4000; len*=2) {
        qeo_log_i("add buffer of size %d", len);
        data = calloc(len, sizeof(char));
        memset(data, 'a', len);
        ck_assert_int_eq(_write_to_memory_cb(data, sizeof(char), len, &helper), (sizeof(char) * len));
        free(data);
    }
    ck_assert_int_eq(helper.offset, 0x7FFE);
    ck_assert_int_eq(helper.length, 0x8000);
    ck_assert_int_eq(strlen(helper.data), 0x7FFE);

    ck_assert_int_eq(_write_to_memory_cb("a", sizeof(char), 1, &helper), (sizeof(char) * 1));
    ck_assert_int_eq(helper.offset, 0x7FFF);
    ck_assert_int_eq(helper.length, 0x8000);
    ck_assert_int_eq(strlen(helper.data), 0x7FFF);

    ck_assert_int_eq(_write_to_memory_cb("a", sizeof(char), 1, &helper), 0);
    ck_assert_int_eq(helper.offset, 0x7FFF);
    ck_assert_int_eq(helper.length, 0x8000);
    ck_assert_int_eq(strlen(helper.data), 0x7FFF);
}
END_TEST

START_TEST(test_curl_upload_memory)
{
    qmgmt_curl_data_helper helper={0};
    size_t len = 0;
    helper.length = 0x8000;
    helper.data = calloc(helper.length, sizeof(char));
    char *data = calloc(helper.length, sizeof(char));
    memset(helper.data, 'a', helper.length-1);

    for (len = 0x1; len <= 0x4000; len*=2) {
        qeo_log_i("read buffer of size %d", len);
        ck_assert_int_eq(_read_from_memory_cb(data, sizeof(char), len, &helper), (sizeof(char) * len));
        ck_assert_int_eq(strlen(data), len);
    }
    ck_assert_int_eq(helper.offset, 0x7FFF);

    ck_assert_int_eq(_read_from_memory_cb(data, sizeof(char), 1, &helper), (sizeof(char) * 1));
    ck_assert_int_eq(helper.offset, 0x8000);

    ck_assert_int_eq(_read_from_memory_cb(data, sizeof(char), 1, &helper), 0);
    ck_assert_int_eq(helper.offset, 0x8000);
}
END_TEST

START_TEST(test_curl_header_cb)
{
    char corid[CURL_UTIL_CORRELATION_ID_MAX_SIZE];
    ck_assert_int_eq(_header_function("blabbla", 8, 1, NULL), 8);
    ck_assert_int_eq(_header_function(NULL, 8, 1, corid), 8);
    ck_assert_int_eq(_header_function("blabbla", 8, 1, corid), 8);
    ck_assert_int_eq(_header_function("X-qeo-correlation: blabla", 10, 5, NULL), 50);
    ck_assert_int_eq(_header_function("X-qeo-correlation: blabla", 10, 5, corid), 50);
    ck_assert_str_eq("X-qeo-correlation: blabla", corid);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "test invalid arguments device", .function = test_inv_args_enroll },
    { .name = "test invalid arguments policy", .function = test_inv_args_policy },
    { .name = "test invalid arguments check policy", .function = test_inv_args_check_policy },
    { .name = "test curl handling init", .function = test_curl_handling_init },
    { .name = "test curl handling device", .function = test_curl_handling_device },
    { .name = "test curl handling policy", .function = test_curl_handling_policy },
    { .name = "test curl handling checkpolicy", .function = test_curl_handling_checkpolicy },
    { .name = "test curl download to memory", .function = test_curl_download_memory },
    { .name = "test curl upload from memory", .function = test_curl_upload_memory },
    { .name = "test curl header cb", .function = test_curl_header_cb },
    {NULL}
};

void register_public_api_tests(Suite *s)
{
    TCase *tc = tcase_create("public api tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_public_api_tests, .name = "public API" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "qeo_mgmgt_client",
        .desc = "unit tests of the qeo_mgmt_client with a mocked libcurl",
};

/* called before every test case starts */
static void init_tcase(void)
{
    s_rsakey = keygen_create(1024);
    s_certs = sk_X509_new(NULL);
    memset(&s_info, 0, sizeof(s_info));
    s_info.userFriendlyName = "friendlytestdevice";
    s_ssl_cb_hitcount = 0;
    s_data_cb_hitcount = 0;
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
