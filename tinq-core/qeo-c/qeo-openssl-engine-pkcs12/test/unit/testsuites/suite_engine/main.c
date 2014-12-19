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

#include <stdlib.h>
#include "unittest/unittest.h"

#include "qeo/log.h"

extern void register_basictests(Suite *s);

qeo_logger_ptr qeo_logger = NULL;

static testCaseInfo testcases[] =
{
	{.register_testcase = register_basictests,
	 .name = "Openssl PKCS12 engine basic tests"},
	{NULL}
};

static testSuiteInfo testsuite =
{
	.name = "Engine",
	.desc = "PKCS12 engine testsuite",
};

static void init_tcase()
{
}

static void fini_tcase()
{
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
