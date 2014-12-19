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
#include "unittest/unittest.h"
#include "curl_easy_mock.h"
#include "qeo_mgmt_urls.h"
#include "json_messages.h"
#include "qeo_mgmt_client_priv.h"
#include "qeo_mgmt_curl_util.h"
#include "curl/curl.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static CURL *s_ctx = NULL;

/* ===[ public API tests ]=================================================== */

static qeo_mgmt_client_retcode_t mock_qeo_mgmt_client_ssl_ctx_cb(SSL_CTX *ctx, void *cookie) {
    return QMGMTCLIENT_OK;
}

extern curl_socket_t qmcu_socket_open_function(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr);
extern int qmcu_socket_close_function(void *clientp, curl_socket_t item);

static int _add_socket(qeo_mgmt_client_ctx_t *ctx) {
    struct curl_sockaddr addr;
    addr.family = AF_LOCAL;
    addr.socktype = SOCK_DGRAM;
    addr.protocol = 0;

    return qmcu_socket_open_function(ctx, CURLSOCKTYPE_IPCXN, &addr);
}

START_TEST(test_valid_json)
{
    int id = 0;
    qeo_mgmt_url_ctx_t ctx = NULL;
    qeo_mgmt_client_ctx_t mg_ctx;
    char* message = NULL;
    const char *url = NULL;
    fail_if(s_ctx == NULL);
    while ((message = get_success_messages(id))){
        id++;
        qeo_log_i("Testing message <%s>", message);
        curl_easy_mock_return_data(message, false);
        ctx = qeo_mgmt_url_init(s_ctx);
        mg_ctx.url_ctx = ctx;
        mg_ctx.curl_ctx = s_ctx;
        fail_if(ctx == NULL);
        ck_assert_int_eq(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_ENROLL_DEVICE, &url), QMGMTCLIENT_OK);
        fail_if(url == NULL);
        qeo_mgmt_url_cleanup(ctx);
    }
}
END_TEST

START_TEST(test_error_json)
{
    int id = 0;
    qeo_mgmt_url_ctx_t ctx = NULL;
    qeo_mgmt_client_ctx_t mg_ctx;
    char* message = NULL;
    const char *url = NULL;
    fail_if(s_ctx == NULL);
    while ((message = get_error_messages(id))){
        id++;
        qeo_log_i("Testing message <%s>", message);
        curl_easy_mock_return_data(message, false);
        ctx = qeo_mgmt_url_init(s_ctx);
        mg_ctx.url_ctx = ctx;
        mg_ctx.curl_ctx = s_ctx;
        fail_if(ctx == NULL);
        fail_if(qeo_mgmt_url_get(&mg_ctx, NULL, NULL, "blabla", QMGMT_URL_ENROLL_DEVICE, &url) == QMGMTCLIENT_OK);
        qeo_mgmt_url_cleanup(ctx);
    }
}
END_TEST

START_TEST(test_inv_args)
{
    fail_unless(qeo_mgmt_url_init(NULL) == NULL);
    qeo_mgmt_url_ctx_t url_ctx = qeo_mgmt_url_init(s_ctx);
    qeo_mgmt_client_ctx_t mg_ctx;
    mg_ctx.url_ctx = url_ctx;
    mg_ctx.curl_ctx = s_ctx;
    fail_if(url_ctx == NULL);
    const char* url = NULL;
    fail_unless(qeo_mgmt_url_get(NULL, NULL, NULL, "bla", QMGMT_URL_ENROLL_DEVICE, &url) == QMGMTCLIENT_EINVAL);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, NULL, NULL, NULL, QMGMT_URL_ENROLL_DEVICE, &url) == QMGMTCLIENT_EINVAL);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, NULL, NULL, "bla", QMGMT_URL_ENROLL_DEVICE, NULL) == QMGMTCLIENT_EINVAL);

    qeo_mgmt_url_cleanup(url_ctx);
}
END_TEST

START_TEST(test_service_present)
{
    qeo_mgmt_url_ctx_t ctx = NULL;
    char* message = NULL;
    const char *url = NULL;
    qeo_mgmt_client_ctx_t mg_ctx;
    fail_if(s_ctx == NULL);
    message = get_services_messages(0);
    qeo_log_i("Testing message <%s>", message);
    curl_easy_mock_return_data(message, false);
    ctx = qeo_mgmt_url_init(s_ctx);
    mg_ctx.url_ctx = ctx;
    mg_ctx.curl_ctx = s_ctx;
    fail_if(ctx == NULL );
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_ENROLL_DEVICE, &url) == QMGMTCLIENT_OK);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_CHECK_POLICY, &url) == QMGMTCLIENT_OK);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_GET_POLICY, &url) == QMGMTCLIENT_OK);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_REGISTER_FORWARDER, &url) == QMGMTCLIENT_OK);
    qeo_mgmt_url_cleanup(ctx);

    message = get_services_messages(1);
    qeo_log_i("Testing message <%s>", message);
    curl_easy_mock_return_data(message, false);
    ctx = qeo_mgmt_url_init(s_ctx);
    mg_ctx.url_ctx = ctx;
    fail_if(ctx == NULL );
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_ENROLL_DEVICE, &url) == QMGMTCLIENT_OK);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_CHECK_POLICY, &url) == QMGMTCLIENT_EBADSERVICE);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_GET_POLICY, &url) == QMGMTCLIENT_OK);
    fail_unless(qeo_mgmt_url_get(&mg_ctx, mock_qeo_mgmt_client_ssl_ctx_cb, NULL, "blabla", QMGMT_URL_REGISTER_FORWARDER, &url) == QMGMTCLIENT_OK);
    qeo_mgmt_url_cleanup(ctx);
}
END_TEST

START_TEST(test_add_fd_to_list)
{
    int i;
    qeo_mgmt_client_ctx_t ctx;
    memset(&ctx,0, sizeof(qeo_mgmt_client_ctx_t));

    fail_if (_add_socket(&ctx) < 0);
    fail_if (_add_socket(&ctx) < 0);
    fail_if (_add_socket(&ctx) < 0);
    fail_if (_add_socket(&ctx) < 0);
    fail_if (_add_socket(&ctx) < 0);

    fail_if(ctx.fd_list == NULL);
    qeo_mgmt_fd_link_t* link = ctx.fd_list;
    for (i = 0 ; i < 5 ; i++) {
        fail_if(link == NULL);
        fail_if(close(link->fd));// make sure to close the FD's we've created.
        link = link->next;
    }
    fail_if(link);

    qeo_mgmt_curl_util_clean_fd_list(&ctx);

    fail_if(ctx.fd_list);

}
END_TEST

START_TEST(test_remove_fd_from_list)
{
    qeo_mgmt_client_ctx_t ctx;
    memset(&ctx,0, sizeof(qeo_mgmt_client_ctx_t));

    int first = _add_socket(&ctx);
    int second = _add_socket(&ctx);
    int third = _add_socket(&ctx);
    int fourth = _add_socket(&ctx);
    int fifth = _add_socket(&ctx);
    fail_if( ctx.fd_list->fd != first);
    fail_if( ctx.fd_list->next->fd != second);
    fail_if( ctx.fd_list->next->next->fd != third);
    fail_if( ctx.fd_list->next->next->next->fd != fourth);
    fail_if( ctx.fd_list->next->next->next->next->fd != fifth);
    fail_if( ctx.fd_list->next->next->next->next->next);

    qmcu_socket_close_function(&ctx, fifth);
    fail_if( ctx.fd_list->fd != first);
    fail_if( ctx.fd_list->next->fd != second);
    fail_if( ctx.fd_list->next->next->fd != third);
    fail_if( ctx.fd_list->next->next->next->fd != fourth);
    fail_if( ctx.fd_list->next->next->next->next);

    qmcu_socket_close_function(&ctx, first);
    fail_if( ctx.fd_list->fd != second);
    fail_if( ctx.fd_list->next->fd != third);
    fail_if( ctx.fd_list->next->next->fd != fourth);
    fail_if( ctx.fd_list->next->next->next);

    qmcu_socket_close_function(&ctx, third);
    fail_if( ctx.fd_list->fd != second);
    fail_if( ctx.fd_list->next->fd != fourth);
    fail_if( ctx.fd_list->next->next);

    qmcu_socket_close_function(&ctx, second);
    fail_if( ctx.fd_list->fd != fourth);
    fail_if( ctx.fd_list->next);

    qmcu_socket_close_function(&ctx, second);
    fail_if( ctx.fd_list->fd != fourth);
    fail_if( ctx.fd_list->next);

    qmcu_socket_close_function(&ctx, fourth);
    fail_if( ctx.fd_list);

}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo jsontests[] =
{
    /* public API */
    { .name = "Make sure all valid json files get parsed", .function = test_valid_json },
    { .name = "Make sure all error json files fail", .function = test_error_json },
    {NULL}
};

static singleTestCaseInfo urltests[] =
{
    /* public API */
    { .name = "test inv args", .function = test_inv_args },
    { .name = "Test the behavior when certain functionality is not present", .function = test_service_present },
    {NULL}
};

static singleTestCaseInfo fd_list_tests[] =
{
    /* public API */
    { .name = "test add fd to list", .function = test_add_fd_to_list },
    { .name = "test remove from list", .function = test_remove_fd_from_list },
    {NULL}
};


void register_json_tests(Suite *s)
{
    TCase *tc = tcase_create("json_parsing");
    tcase_addtests(tc, jsontests);
    suite_add_tcase (s, tc);
}

void register_url_handling_tests(Suite *s)
{
    TCase *tc = tcase_create("url_handling");
    tcase_addtests(tc, urltests);
    suite_add_tcase (s, tc);
}

void register_fd_list_tests(Suite *s)
{
    TCase *tc = tcase_create("fd_list_handling");
    tcase_addtests(tc, fd_list_tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_json_tests, .name = "json_parsing" },
    { .register_testcase = register_url_handling_tests, .name = "url_handling" },
    { .register_testcase = register_fd_list_tests, .name = "fd_list_handling" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "url_discovery",
        .desc = "unit tests of the url discovery/root resource with a mocked libcurl",
};

/* called before every test case starts */
static void init_tcase(void)
{
    curl_easy_mock_ignore_and_return(CURLE_OK, true, CURLE_OK, CURLE_OK, CURLE_OK);
    s_ctx = curl_easy_init();
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    curl_easy_cleanup(s_ctx);
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
