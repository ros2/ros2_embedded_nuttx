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
#include <stdbool.h>

#include <qeo/log.h>
#include <qeo/mgmt_cert_parser.h>
#include "unittest/unittest.h"
#include "certstore.h"

/* Prototypes of not really public functions that can be tested separately */
bool get_ids_from_common_name(char *cn, int64_t *realm_id, int64_t *device_id, int64_t *user_id);

/* ===[ public API tests ]=================================================== */
typedef struct {
    char* name;
    bool result;
    int64_t realmid;
    int64_t deviceid;
    int64_t userid;
} name_parsing_test_helper;

START_TEST(test_common_name_parsing)
{
    int i = 0;
    int64_t realmid;
    int64_t deviceid;
    int64_t userid;
    name_parsing_test_helper helper[]={
        {"6 7 8", true, 0x6, 0x7, 0x8},
        {" 9 10 1B0", true, 0x9, 0x10, 0x1b0},
        {"  6a6   777  133 ", true, 0x6A6, 0x777, 0x133},
        {"  fff   777  133 ", true, 0xFFF, 0x777, 0x133},
        {"realm FF", false, -1, -1, -1},
        {" FF policy", false, -1, -1, -1},
        {"1 1", false, -1, -1, -1},
        {"G", false, -1, -1, -1},
        {"G G", false, -1, -1, -1},
        {"G G 1", false, -1, -1, -1},
        {"1", false, -1, -1, -1},
        {"1 1 Fr", false, -1, -1, -1},
        {" 1r ", false, -1, -1, -1},
        {"1 1 Fr", false, -1, -1, -1},
        {"1 1 1 1", false, -1, -1, -1},
        {"1 1 1 a", false, -1, -1, -1},
    };

    for (i = 0; i < sizeof(helper)/sizeof(name_parsing_test_helper); ++i) {

        fail_unless(helper[i].result == get_ids_from_common_name(helper[i].name, &realmid, &deviceid, &userid));
        if (helper[i].result == true){
            fail_unless(realmid == helper[i].realmid, "Expected realmid (%llx) is different than real realmid (%llx) for row (%s)", helper[i].realmid, realmid, helper[i].name);
            fail_unless(deviceid == helper[i].deviceid, "Expected deviceid (%llx) is different than real deviceid (%llx) for row (%s)", helper[i].realmid, realmid, helper[i].name);
            fail_unless(userid == helper[i].userid, "Expected userid (%llx) is different than real userid (%llx) for row (%s)", helper[i].realmid, realmid, helper[i].name);
        }
    }
}
END_TEST

START_TEST(test_sunny_day)
{
    int deviceids[]={CERTSTORE_MASTER , CERTSTORE_REALM, CERTSTORE_DEVICE, -1};
    STACK_OF(X509) *devicechain = get_cert_store(deviceids);
    qeo_mgmt_cert_contents qmcc;

    fail_unless(qeo_mgmt_cert_parse(devicechain, &qmcc) == QCERT_OK);
    ck_assert_int_eq(qmcc.device, 327);
    ck_assert_int_eq(qmcc.user, 20);
    ck_assert_int_eq(qmcc.realm, 20);

    sk_X509_free(devicechain);
}
END_TEST

START_TEST(test_cert_ordening_sunny)
{
    qeo_mgmt_cert_contents qmcc;
    int ids1[]={CERTSTORE_MASTER , CERTSTORE_REALM, CERTSTORE_DEVICE, -1};
    int ids2[]={CERTSTORE_REALM , CERTSTORE_MASTER, CERTSTORE_DEVICE, -1};
    int ids3[]={CERTSTORE_DEVICE , CERTSTORE_REALM, CERTSTORE_MASTER, -1};
    int ids4[]={CERTSTORE_REALM , CERTSTORE_DEVICE, CERTSTORE_MASTER, -1};
    STACK_OF(X509) *chain1 = get_cert_store(ids1);
    STACK_OF(X509) *chain2 = get_cert_store(ids2);
    STACK_OF(X509) *chain3 = get_cert_store(ids3);
    STACK_OF(X509) *chain4 = get_cert_store(ids4);

    fail_unless(qeo_mgmt_cert_parse(chain1, &qmcc) == QCERT_OK);
    fail_unless(qeo_mgmt_cert_parse(chain2, &qmcc) == QCERT_OK);
    fail_unless(qeo_mgmt_cert_parse(chain3, &qmcc) == QCERT_OK);
    fail_unless(qeo_mgmt_cert_parse(chain4, &qmcc) == QCERT_OK);

    sk_X509_free(chain1);
    sk_X509_free(chain2);
    sk_X509_free(chain3);
    sk_X509_free(chain4);
}
END_TEST

START_TEST(test_cert_ordening_rainy)
{
    qeo_mgmt_cert_contents qmcc;
    int ids1[]={CERTSTORE_MASTER , CERTSTORE_RANDOM, CERTSTORE_DEVICE, -1};
    int ids2[]={CERTSTORE_REALM , CERTSTORE_MASTER, CERTSTORE_RANDOM, -1};
    int ids3[]={CERTSTORE_DEVICE , CERTSTORE_REALM, -1};
    int ids4[]={CERTSTORE_DEVICE, CERTSTORE_MASTER, -1};
    int ids5[]={CERTSTORE_MASTER, -1};
    int ids6[]={CERTSTORE_REALM, CERTSTORE_MASTER, -1};
    STACK_OF(X509) *chain1 = get_cert_store(ids1);
    STACK_OF(X509) *chain2 = get_cert_store(ids2);
    STACK_OF(X509) *chain3 = get_cert_store(ids3);
    STACK_OF(X509) *chain4 = get_cert_store(ids4);
    STACK_OF(X509) *chain5 = get_cert_store(ids5);
    STACK_OF(X509) *chain6 = get_cert_store(ids6);

    fail_if(qeo_mgmt_cert_parse(chain1, &qmcc) == QCERT_OK);
    fail_if(qeo_mgmt_cert_parse(chain2, &qmcc) == QCERT_OK);
    fail_if(qeo_mgmt_cert_parse(chain3, &qmcc) == QCERT_OK);
    fail_if(qeo_mgmt_cert_parse(chain4, &qmcc) == QCERT_OK);
    fail_if(qeo_mgmt_cert_parse(chain5, &qmcc) == QCERT_OK);
    fail_if(qeo_mgmt_cert_parse(chain6, &qmcc) == QCERT_OK);

    sk_X509_free(chain1);
    sk_X509_free(chain2);
    sk_X509_free(chain3);
    sk_X509_free(chain4);
    sk_X509_free(chain5);
    sk_X509_free(chain6);
}
END_TEST


/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "test CN parsing", .function = test_common_name_parsing },
    { .name = "test sunny day", .function = test_sunny_day },
    { .name = "test cert ordening sunny", .function = test_cert_ordening_sunny },
    { .name = "test cert ordening rainy", .function = test_cert_ordening_rainy },
    {NULL}
};

void register_public_api_tests(Suite *s)
{
    TCase *tc = tcase_create("certificate parsing tests");
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
        .name = "qeo_mgmgt_cert_parsing",
        .desc = "unit tests about retrieving information from certificates",
};

/* called before every test case starts */
static void init_tcase(void)
{

}

/* called after every test case finishes */
static void fini_tcase(void)
{
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
