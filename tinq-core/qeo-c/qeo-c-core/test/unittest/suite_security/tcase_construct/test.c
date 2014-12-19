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

#include "unittest/unittest.h"

#include "security.h"
#include <qeo/error.h>
#include <qeo/log.h>

#include "Mockmgmt_client.h"

#include "security_fixtures.h"

static const qeo_security_config CFG = {
    .id = {
        .realm_id = 0,
        .device_id = 0,
        .user_id = 0,
        .friendly_name = ""
    },
    .security_status_cb = NULL,
};

START_TEST(invalidInput)
{
    ck_assert_int_eq(qeo_security_construct(NULL, NULL), QEO_EINVAL);
    ck_assert_int_eq(qeo_security_construct((qeo_security_config*)0xdeadbabe, NULL), QEO_EINVAL);
    ck_assert_int_eq(qeo_security_construct(NULL, (qeo_security_hndl*)0xdeadbabe), QEO_EINVAL);
}
END_TEST

START_TEST(successfulConstruct)
{
    qeo_security_config cfg = CFG;
    qeo_security_hndl qeo_sec;

    qeo_mgmt_client_init_ExpectAndReturn((qeo_mgmt_client_ctx_t *)0xdeadbabe);
    qeo_mgmt_client_clean_Expect((qeo_mgmt_client_ctx_t *)0xdeadbabe);

    ck_assert_int_eq(qeo_security_construct(&cfg, &qeo_sec), QEO_OK);
    ck_assert_int_ne(qeo_sec, NULL);

    ck_assert_int_eq(qeo_security_destruct(&qeo_sec), QEO_OK);
    ck_assert_int_eq(qeo_sec, NULL);
}
END_TEST

START_TEST(destructAfterInit)
{
    qeo_security_hndl qeo_sec = NULL;
    ck_assert_int_eq(qeo_security_destruct(&qeo_sec), QEO_OK);
    ck_assert_int_eq(qeo_sec, NULL);
}
END_TEST

START_TEST(destructBeforeInit)
{
    qeo_security_hndl qeo_sec = (qeo_security_hndl)0xdeadbabe;
    ck_assert_int_eq(qeo_security_destruct(&qeo_sec), QEO_EBADSTATE);
}
END_TEST

void register_constructtests(Suite *s)
{
	TCase *testCase = NULL;

	testCase = tcase_create("Security Lib construct tests - no init");
    tcase_add_test(testCase, destructBeforeInit);
    suite_add_tcase(s, testCase);

    testCase = tcase_create("Security Lib construct tests");
    tcase_add_checked_fixture(testCase, initGlobalMocks, destroyGlobalMocks);
    tcase_add_checked_fixture(testCase, testInitLib, testDestroyLib);
    tcase_add_test(testCase, invalidInput);
    tcase_add_test(testCase, successfulConstruct);
    tcase_add_test(testCase, destructAfterInit);
    suite_add_tcase(s, testCase);
}
