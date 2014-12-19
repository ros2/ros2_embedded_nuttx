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
#include <unistd.h>

#include <qeo/log.h>
#include <jansson.h>
#include <pthread.h>
#include "unittest/unittest.h"
#include "curl_easy_mock.h"
#include "qeo/mgmt_client_forwarder.h"
#include "json_messages.h"

#define COOKIE_MAGIC_NUMBER 666
#define TEST_URL "blabla"

static qeo_mgmt_client_ctx_t *s_ctx = NULL;
static pthread_mutex_t         s_mutex;
static pthread_cond_t         s_cond;
static qeo_mgmt_client_retcode_t s_result;


static qeo_mgmt_client_retcode_t my_ssl_cb(SSL_CTX *ctx, void *cookie){
    ck_assert_ptr_eq(cookie, (void*) COOKIE_MAGIC_NUMBER);
    return QMGMTCLIENT_OK;
}

typedef struct
{
    int nrForwarders;
    qeo_mgmt_client_forwarder_t **forwarders;
} _forwarder_cb_helper;

static qeo_mgmt_client_retcode_t my_fwd_cb(qeo_mgmt_client_forwarder_t* forwarder, void *cookie) {
    _forwarder_cb_helper *cb_helper = (_forwarder_cb_helper *) cookie;

    cb_helper->nrForwarders++;
    cb_helper->forwarders=realloc(cb_helper->forwarders, sizeof(qeo_mgmt_client_forwarder_t*)*cb_helper->nrForwarders);
    cb_helper->forwarders[cb_helper->nrForwarders-1]=forwarder;
    return QMGMTCLIENT_OK;
}

static qeo_mgmt_client_retcode_t my_fwd_stub_cb(qeo_mgmt_client_forwarder_t* forwarder, void *cookie){
    ck_assert_ptr_eq(cookie, (void*) COOKIE_MAGIC_NUMBER);
    return QMGMTCLIENT_OK;
}
static void my_result_cb(qeo_mgmt_client_retcode_t result, void *cookie){
    qeo_log_i("my_result_cb");
    s_result = result;
    pthread_mutex_lock(&s_mutex);
    pthread_cond_broadcast(&s_cond);
    pthread_mutex_unlock(&s_mutex);
}


/* ===[ public API tests ]=================================================== */
START_TEST(test_inv_args_register)
{
    qeo_mgmt_client_locator_t locators[1]={};

    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(NULL, TEST_URL, locators, 1, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, NULL, locators, 1, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, NULL, 1, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, 1, NULL, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
}
END_TEST

START_TEST(test_inv_args_get_sync)
{
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(NULL, TEST_URL, my_fwd_stub_cb, (void*) COOKIE_MAGIC_NUMBER, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(s_ctx, NULL, my_fwd_stub_cb, (void*) COOKIE_MAGIC_NUMBER, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(s_ctx, TEST_URL, NULL, (void*) COOKIE_MAGIC_NUMBER, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(s_ctx, TEST_URL, my_fwd_stub_cb, (void*) COOKIE_MAGIC_NUMBER, NULL, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
}
END_TEST

START_TEST(test_inv_args_get_async)
{
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders(NULL, TEST_URL, my_fwd_stub_cb, my_result_cb, (void*) COOKIE_MAGIC_NUMBER, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders(s_ctx, NULL, my_fwd_stub_cb, my_result_cb, (void*) COOKIE_MAGIC_NUMBER, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders(s_ctx, TEST_URL, NULL, my_result_cb, (void*) COOKIE_MAGIC_NUMBER, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders(s_ctx, TEST_URL, my_fwd_stub_cb, my_result_cb, (void*) COOKIE_MAGIC_NUMBER, NULL, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_EINVAL);
}
END_TEST

static void check_json_equality(char* expected, char* received, int linenumber){
    json_error_t error1, error2;
    json_t *received_json=json_loads(received, 0, &error1);
    json_t *expected_json=json_loads(expected, 0, &error2);

    fail_if(received_json == NULL, "Json parsing failed <%s: %d> at code line <%d>", error1.text, error1.line, linenumber);
    fail_if(expected_json == NULL, "Json parsing failed <%s: %d> at code line <%d>", error2.text, error2.line, linenumber);

    fail_unless(json_equal(received_json, expected_json) == 1, "Json not equal at line <%d> expected <%s> received <%s>", linenumber, expected, received);
    json_decref(received_json);
    json_decref(expected_json);
}

START_TEST(test_register_forwarders)
{
    qeo_mgmt_client_locator_t locators[10]={};
    char addresses[10][10];
    int i=0;

    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, NULL, 0, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    check_json_equality(get_success_register_fwd(0), curl_easy_mock_get_uploaded_data(), __LINE__);

    locators[0].address="212.118.224.153";
    locators[0].type=QMGMT_LOCATORTYPE_TCPV4;
    locators[0].port=7400;
    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, 1, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    check_json_equality(get_success_register_fwd(1), curl_easy_mock_get_uploaded_data(), __LINE__);

    locators[0].address="212.118.224.153";
    locators[0].type=QMGMT_LOCATORTYPE_TCPV4;
    locators[0].port=8080;
    locators[1].address="hostname";
    locators[1].type=QMGMT_LOCATORTYPE_TCPV6;
    locators[1].port=7400;
    locators[2].address="212.118.224.153";
    locators[2].type=QMGMT_LOCATORTYPE_UDPV4;
    locators[2].port=6666;
    locators[3].address="fe80::250:bfff:feb7:c61d";
    locators[3].type=QMGMT_LOCATORTYPE_UDPV6;
    locators[3].port=7777;
    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, 4, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    check_json_equality(get_success_register_fwd(2), curl_easy_mock_get_uploaded_data(), __LINE__);

    for (i = 0; i < 10; ++i) {
        sprintf(addresses[i], "%d", 200+i);
        locators[i].address=addresses[i];
        locators[i].port=100+i;
        locators[i].type=(qeo_mgmt_client_locator_type_t) (i%4)+1;
    }
    ck_assert_int_eq(qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, 10, my_ssl_cb, (void*) COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    check_json_equality(get_success_register_fwd(3), curl_easy_mock_get_uploaded_data(), __LINE__);

}
END_TEST


START_TEST(test_register_error_forwarders)
{
    qeo_mgmt_client_locator_t locators[4]={};
    int i = 0;

    for (i = 0; i < 4; i++) {
        locators[i].address = "212.118.224.153";
        locators[i].type = QMGMT_LOCATORTYPE_TCPV4;
        locators[i].port = 7400;

        locators[i].type = QMGMT_LOCATORTYPE_UNKNOWN;
        ck_assert_int_eq(
                qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, i+1, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER),
                QMGMTCLIENT_EINVAL);
        locators[i].type = QMGMT_LOCATORTYPE_TCPV4;

        locators[i].port = -1;
        ck_assert_int_eq(
                qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, i+1, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER),
                QMGMTCLIENT_OK);
        locators[i].port = 0xFFFF;
        ck_assert_int_eq(
                qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, i+1, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER),
                QMGMTCLIENT_OK);
        locators[i].port = -2;
        ck_assert_int_eq(
                qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, i+1, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER),
                QMGMTCLIENT_EINVAL);
        locators[i].port = 0x10000;
        ck_assert_int_eq(
                qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, i+1, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER),
                QMGMTCLIENT_EINVAL);
        locators[i].port = 6666;

        locators[i].address = NULL;
        ck_assert_int_eq(
                qeo_mgmt_client_register_forwarder(s_ctx, TEST_URL, locators, i+1, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER),
                QMGMTCLIENT_EINVAL);
        locators[i].address = "212.118.224.153";
    }

}
END_TEST

static void list_forwarders_and_expect(int nrForwarders, char* message, int line){
    _forwarder_cb_helper helper = {0};
    int i = 0;

    qeo_log_i("Testing message <%s> at line <%d> using sync api", message, line);
    curl_easy_mock_clean();
    curl_easy_mock_return_data(message, true);
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(s_ctx, TEST_URL, my_fwd_cb, (void*)&helper, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    ck_assert_int_eq(helper.nrForwarders, nrForwarders);

    for (i = 0; i < helper.nrForwarders; ++i) {
        qeo_mgmt_client_free_forwarder(helper.forwarders[i]);
    }
    free(helper.forwarders);
}
static void list_forwarders_and_expect_async(int nrForwarders, char* message, int line){
    _forwarder_cb_helper helper = {0};
    int i = 0;

    qeo_log_i("Testing message <%s> at line <%d> using async api", message, line);
    curl_easy_mock_clean();
    curl_easy_mock_return_data(message, true);
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders(s_ctx, TEST_URL, my_fwd_cb, my_result_cb, (void*)&helper, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    pthread_cond_wait(&s_cond, &s_mutex);

    ck_assert_int_eq(helper.nrForwarders, nrForwarders);

    for (i = 0; i < helper.nrForwarders; ++i) {
        qeo_mgmt_client_free_forwarder(helper.forwarders[i]);
    }
    free(helper.forwarders);
}


static void list_forwarders_and_expect_error(qeo_mgmt_client_retcode_t expected, char* message, int line){
    _forwarder_cb_helper helper = {0};
    int i = 0;

    qeo_log_i("Testing message <%s> at line <%d>", message, line);
    curl_easy_mock_clean();
    curl_easy_mock_return_data(message, true);
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(s_ctx, TEST_URL, my_fwd_cb, (void*)&helper, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER), expected);

    for (i = 0; i < helper.nrForwarders; ++i) {
        qeo_mgmt_client_free_forwarder(helper.forwarders[i]);
    }
    free(helper.forwarders);
}


static void list_forwarders_and_expect_error_async(qeo_mgmt_client_retcode_t expected, char* message, int line){
    _forwarder_cb_helper helper = {0};
    int i = 0;

    qeo_log_i("Testing message <%s> at line <%d>", message, line);
    curl_easy_mock_clean();
    curl_easy_mock_return_data(message, true);
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders(s_ctx, TEST_URL, my_fwd_cb, my_result_cb, (void*)&helper, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    pthread_cond_wait(&s_cond, &s_mutex);

    for (i = 0; i < helper.nrForwarders; ++i) {
        qeo_mgmt_client_free_forwarder(helper.forwarders[i]);
    }
    free(helper.forwarders);
    ck_assert_int_eq(s_result, expected);
}

START_TEST(test_get_forwarders_sunny)
{
    fail_if(s_ctx == NULL);

    list_forwarders_and_expect(0, get_success_list_fwds(0), __LINE__);
    list_forwarders_and_expect(1, get_success_list_fwds(1), __LINE__);
    list_forwarders_and_expect(4, get_success_list_fwds(2), __LINE__);
    list_forwarders_and_expect(4, get_success_list_fwds(3), __LINE__);
}
END_TEST

START_TEST(test_get_forwarders_sunny_async)
{
    fail_if(s_ctx == NULL);
    pthread_mutex_lock(&s_mutex);
    list_forwarders_and_expect_async(0, get_success_list_fwds(0), __LINE__);
    list_forwarders_and_expect_async(1, get_success_list_fwds(1), __LINE__);
    list_forwarders_and_expect_async(4, get_success_list_fwds(2), __LINE__);
    list_forwarders_and_expect_async(4, get_success_list_fwds(3), __LINE__);

}
END_TEST

START_TEST(test_get_forwarders_detailed)
{
    _forwarder_cb_helper helper = {0};
    char* message = get_success_list_fwds(3);

    curl_easy_mock_clean();
    curl_easy_mock_return_data(message, true);
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    ck_assert_int_eq(qeo_mgmt_client_get_forwarders_sync(s_ctx, TEST_URL, my_fwd_cb, (void*)&helper, my_ssl_cb, (void*)COOKIE_MAGIC_NUMBER), QMGMTCLIENT_OK);
    ck_assert_int_eq(helper.nrForwarders, 4);


    fail_unless(helper.forwarders[0]->deviceID == 1);
    fail_unless(helper.forwarders[0]->nrOfLocators == 2);
    fail_unless(helper.forwarders[0]->locators[0].port == 7400);
    fail_unless(helper.forwarders[0]->locators[0].type == QMGMT_LOCATORTYPE_TCPV4);
    fail_unless(strcmp(helper.forwarders[0]->locators[0].address, "212.118.224.153") == 0);
    fail_unless(helper.forwarders[0]->locators[1].port == 111);
    fail_unless(helper.forwarders[0]->locators[1].type == QMGMT_LOCATORTYPE_UDPV4);
    fail_unless(strcmp(helper.forwarders[0]->locators[1].address, "212.118.224.152") == 0);

    fail_unless(helper.forwarders[1]->deviceID == 2);
    fail_unless(helper.forwarders[1]->nrOfLocators == 1);
    fail_unless(helper.forwarders[1]->locators[0].port == 8080);
    fail_unless(helper.forwarders[1]->locators[0].type == QMGMT_LOCATORTYPE_UDPV6);
    fail_unless(strcmp(helper.forwarders[1]->locators[0].address, "fe80::250:bfff:feb7:c61e") == 0);

    free(helper.forwarders);
}
END_TEST

START_TEST(test_get_forwarders_error)
{
    fail_if(s_ctx == NULL);

    list_forwarders_and_expect_error(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(0), __LINE__);
    list_forwarders_and_expect_error(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(1), __LINE__);
    list_forwarders_and_expect_error(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(2), __LINE__);
    list_forwarders_and_expect_error(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(3), __LINE__);
}
END_TEST


START_TEST(test_get_forwarders_error_async)
{
    fail_if(s_ctx == NULL);
    pthread_mutex_lock(&s_mutex);
    list_forwarders_and_expect_error_async(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(0), __LINE__);
    list_forwarders_and_expect_error_async(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(1), __LINE__);
    list_forwarders_and_expect_error_async(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(2), __LINE__);
    list_forwarders_and_expect_error_async(QMGMTCLIENT_EBADREPLY, get_error_list_fwds(3), __LINE__);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "test get forwarders sunny async", .function = test_get_forwarders_sunny_async },
    { .name = "test get forwarders sunny", .function = test_get_forwarders_sunny },
    { .name = "test get forwarders error", .function = test_get_forwarders_error },
    { .name = "test get forwarders error async", .function = test_get_forwarders_error_async },
    { .name = "test inv args register", .function = test_inv_args_register },
    { .name = "test inv args get sync", .function = test_inv_args_get_sync },
    { .name = "test inv args get async", .function = test_inv_args_get_async },
    { .name = "test register forwarders", .function = test_register_forwarders },
    { .name = "test register error forwarders", .function = test_register_error_forwarders },
    { .name = "test get forwarders detailed", .function = test_get_forwarders_detailed },
    {NULL}
};

void register_tests(Suite *s)
{
    TCase *tc = tcase_create("fwd_handling");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_tests, .name = "fwd_handling" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "forwarders",
        .desc = "unit tests for registering or listing fwds with a mocked libcurl",
};

/* called before every test case starts */
static void init_tcase(void)
{
    pthread_mutex_init(&s_mutex, NULL);
    pthread_cond_init(&s_cond, NULL);
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    s_ctx = qeo_mgmt_client_init();
    s_result = QMGMTCLIENT_ESSL;
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    qeo_mgmt_client_clean(s_ctx);
    s_ctx = NULL;
    pthread_cond_destroy(&s_cond);
    pthread_mutex_destroy(&s_mutex);
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
