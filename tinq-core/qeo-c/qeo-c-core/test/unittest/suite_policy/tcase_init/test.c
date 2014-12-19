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

#include "policy.h"

START_TEST(successfulInit)
{
    ck_assert_int_eq(qeo_security_policy_init(), QEO_OK);
    ck_assert_int_eq(qeo_security_policy_destroy(), QEO_OK);
}
END_TEST

void register_inittests(Suite *s)
{
	TCase *testCase = NULL;

    testCase = tcase_create("Policy init tests");
    tcase_add_test(testCase, successfulInit);
    suite_add_tcase(s, testCase);
}
