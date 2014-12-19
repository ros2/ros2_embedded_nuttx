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

#include "security_fixtures.h"
#include "Mockplatform_security.h"
#include "Mockremote_registration.h"
#include "Mockpolicy.h"


void testInitLib(void)
{
    ck_assert_int_eq(qeo_security_init(), QEO_OK);
}

void testDestroyLib(void)
{
    qeo_security_destroy();

}

void initGlobalMocks(void){

    Mockplatform_security_Init();
    Mockremote_registration_Init();
    qeo_remote_registration_destroy_Ignore();
    qeo_remote_registration_destruct_IgnoreAndReturn(QEO_OK);
}

void destroyGlobalMocks(void){

    Mockremote_registration_Verify();
    Mockremote_registration_Destroy();

    Mockplatform_security_Verify();
    Mockplatform_security_Destroy();
}
