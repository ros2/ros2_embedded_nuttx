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
#include "policy_fixtures.h"

void initLib(void)
{
    ck_assert_int_eq(qeo_security_policy_init(), QEO_OK);
}

void destroyLib(void)
{
    ck_assert_int_eq(qeo_security_policy_destroy(), QEO_OK);

}
