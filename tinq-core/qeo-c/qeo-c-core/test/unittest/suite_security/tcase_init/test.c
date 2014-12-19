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
#include <string.h>
#include <stdio.h>

#include "security.h"
#include "qeo/openssl_engine.h"
#include <qeo/error.h>
#include "security_fixtures.h"
#include "Mockplatform_security.h"
#include "Mockremote_registration.h"

START_TEST(successfulInit)
{
    ck_assert_int_eq(qeo_security_init(), QEO_OK);
    qeo_security_destroy();

}
END_TEST

START_TEST(destroy_uninit)
{
    // destroy without init
    qeo_security_destroy();
}
END_TEST


static void initMock(void){

    Mockplatform_security_Init();
    Mockremote_registration_Init();
    qeo_remote_registration_destroy_Ignore();

}

static void destroyMock(void){

    Mockplatform_security_Verify();
    Mockplatform_security_Destroy();

}

void register_inittests(Suite *s)
{
	TCase *testCase = NULL;

    testCase = tcase_create("Security Lib init tests");
    tcase_add_checked_fixture(testCase, initMock, destroyMock);
    tcase_add_test(testCase, successfulInit);
    tcase_add_test(testCase, destroy_uninit);
    suite_add_tcase(s, testCase);
}
