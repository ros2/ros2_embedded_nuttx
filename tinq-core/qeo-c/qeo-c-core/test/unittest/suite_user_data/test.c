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

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "unittest/unittest.h"

#include "core.h"
#include "list.h"
#include "partitions.h"

#include "Mockpolicy.h"

static enum { SCEN_NO_CALL, SCEN_EMPTY, SCEN_LIST_1, SCEN_LIST_2, SCEN_LIST_3, SCEN_LIST_3b } _scenario;

static struct topic_participant_list_node _list_1 = { 0, {0}, "partition_1", NULL };
static struct topic_participant_list_node _list_2 = { 1, {1}, "partition_2", &_list_1 };
static struct topic_participant_list_node _list_3 = { 0, {0}, "partition_3", &_list_2 };

static qeo_policy_perm_t wr_policy_on_update(const qeocore_writer_t *writer,
                                             const qeo_policy_identity_t *id,
                                             uintptr_t userdata)
{
    qeo_policy_perm_t perm = QEO_POLICY_ALLOW;

    if (NULL != id) {
        if ((SCEN_LIST_3b == _scenario) && (0 != id->user_id)) {
            perm = QEO_POLICY_DENY;
        }
    }
    return perm;
}

static qeo_policy_perm_t rd_policy_on_update(const qeocore_reader_t *reader,
                                             const qeo_policy_identity_t *id,
                                             uintptr_t userdata)
{
    qeo_policy_perm_t perm = QEO_POLICY_ALLOW;

    if (NULL != id) {
        if ((SCEN_LIST_3b == _scenario) && (0 != id->user_id)) {
            perm = QEO_POLICY_DENY;
        }
    }
    return perm;
}

static qeo_retcode_t stub_qeo_security_policy_get_partition_strings(qeo_security_policy_hndl pol,
                                                                    uintptr_t cookie,
                                                                    const char* topic_name,
                                                                    unsigned int selector_mask,
                                                                    qeo_security_policy_update_fine_grained_rules_cb update_cb,
                                                                    int cmock_num_calls)
{
    switch (_scenario) {
        case SCEN_NO_CALL:
            break;
        case SCEN_EMPTY:
            update_cb(pol, cookie, topic_name, selector_mask, NULL, NULL);
            break;
        case SCEN_LIST_1:
            update_cb(pol, cookie, topic_name, selector_mask, &_list_1, &_list_1);
            break;
        case SCEN_LIST_2:
            update_cb(pol, cookie, topic_name, selector_mask, &_list_2, &_list_2);
            break;
        case SCEN_LIST_3:
            update_cb(pol, cookie, topic_name, selector_mask, &_list_3, &_list_3);
            break;
        case SCEN_LIST_3b:
            update_cb(pol, cookie, topic_name, selector_mask, &_list_3, &_list_3);
            break;
    }
    return QEO_OK;
}

static void validate(DDS_StringSeq *seq,
                     bool is_writer)
{
    switch (_scenario) {
        case SCEN_NO_CALL:
        case SCEN_EMPTY:
            fail_unless(1 == DDS_SEQ_LENGTH(*seq));
            if (is_writer) {
                fail_unless(0 == strcmp("DISABLED_WRITER", DDS_SEQ_ITEM(*seq, 0)));
            }
            else {
                fail_unless(0 == strcmp("DISABLED_READER", DDS_SEQ_ITEM(*seq, 0)));
            }
            break;
        case SCEN_LIST_1:
            fail_unless(1 == DDS_SEQ_LENGTH(*seq));
            fail_unless(0 == strcmp(_list_1.participant_name, DDS_SEQ_ITEM(*seq, 0)));
            break;
        case SCEN_LIST_2:
            fail_unless(2 == DDS_SEQ_LENGTH(*seq));
            break;
        case SCEN_LIST_3:
            fail_unless(3 == DDS_SEQ_LENGTH(*seq));
            break;
        case SCEN_LIST_3b:
            fail_unless(2 == DDS_SEQ_LENGTH(*seq));
            break;
    }
}

static void validate_sub(DDS_Subscriber sub)
{
    DDS_SubscriberQos qos;

    fail_unless(DDS_RETCODE_OK == DDS_Subscriber_get_qos(sub, &qos));
    validate(&qos.partition.name, false);
    DDS_StringSeq__clear(&qos.partition.name);
}

static void validate_pub(DDS_Publisher pub)
{
    DDS_PublisherQos qos;

    fail_unless(DDS_RETCODE_OK == DDS_Publisher_get_qos(pub, &qos));
    validate(&qos.partition.name, true);
    DDS_StringSeq__clear(&qos.partition.name);
}

START_TEST(test_update_reader)
{
    qeo_factory_t factory = { .flags.initialized = 1 };
    qeocore_reader_t reader = { .entity.factory = &factory };
    DDS_Topic topic;

    /* init */
    fail_unless(NULL != (factory.dp = DDS_DomainParticipantFactory_create_participant(0, NULL, NULL, 0)));
    fail_unless(NULL != (reader.sub = DDS_DomainParticipant_create_subscriber(factory.dp, NULL, NULL, 0)));
    fail_unless(NULL != (topic = DDS_DomainParticipant_create_topic(factory.dp, "test", "test", NULL, NULL, 0)));
    fail_unless(NULL != (reader.dr = DDS_Subscriber_create_datareader(reader.sub, topic, NULL, NULL, 0)));
    qeo_security_policy_get_fine_grained_rules_StubWithCallback(stub_qeo_security_policy_get_partition_strings);
    /* test 1 : expect disabled to be inserted */
    _scenario = SCEN_NO_CALL;
    fail_unless(QEO_OK == reader_user_data_update(&reader));
    validate_sub(reader.sub);
    /* test 2 : expect single partition string */
    _scenario = SCEN_LIST_1;
    fail_unless(QEO_OK == reader_user_data_update(&reader));
    validate_sub(reader.sub);
    /* test 3 : expect three partition strings */
    _scenario = SCEN_LIST_3;
    fail_unless(QEO_OK == reader_user_data_update(&reader));
    validate_sub(reader.sub);
    /* test 3b : expect two partition strings (one denied by callback) */
    _scenario = SCEN_LIST_3b;
    reader.listener.on_policy_update = rd_policy_on_update;
    fail_unless(QEO_OK == reader_user_data_update(&reader));
    reader.listener.on_policy_update = NULL;
    validate_sub(reader.sub);
    /* test 4 : expect two partition strings */
    _scenario = SCEN_LIST_2;
    fail_unless(QEO_OK == reader_user_data_update(&reader));
    validate_sub(reader.sub);
    /* test 5 : expect disabled to be inserted */
    _scenario = SCEN_EMPTY;
    fail_unless(QEO_OK == reader_user_data_update(&reader));
    validate_sub(reader.sub);
    /* fini */
    DDS_DomainParticipant_delete_subscriber(factory.dp, reader.sub);
    DDS_DomainParticipantFactory_delete_participant(factory.dp);
}
END_TEST

START_TEST(test_update_writer)
{
    qeo_factory_t factory = { .flags.initialized = 1 };
    qeocore_writer_t writer = { .entity.factory = &factory };

    /* init */
    fail_unless(NULL != (factory.dp = DDS_DomainParticipantFactory_create_participant(0, NULL, NULL, 0)));
    fail_unless(NULL != (writer.pub = DDS_DomainParticipant_create_publisher(factory.dp, NULL, NULL, 0)));
    qeo_security_policy_get_fine_grained_rules_StubWithCallback(stub_qeo_security_policy_get_partition_strings);
    /* test 1 : expect disabled to be inserted */
    _scenario = SCEN_NO_CALL;
    fail_unless(QEO_OK == writer_user_data_update(&writer));
    validate_pub(writer.pub);
    /* test 2 : expect single partition string */
    _scenario = SCEN_LIST_1;
    fail_unless(QEO_OK == writer_user_data_update(&writer));
    validate_pub(writer.pub);
    /* test 3 : expect three partition strings */
    _scenario = SCEN_LIST_3;
    fail_unless(QEO_OK == writer_user_data_update(&writer));
    validate_pub(writer.pub);
    /* test 3b : expect two partition strings (one denied by callback) */
    _scenario = SCEN_LIST_3b;
    writer.listener.on_policy_update = wr_policy_on_update;
    fail_unless(QEO_OK == writer_user_data_update(&writer));
    writer.listener.on_policy_update = NULL;
    validate_pub(writer.pub);
    /* test 4 : expect two partition strings */
    _scenario = SCEN_LIST_2;
    fail_unless(QEO_OK == writer_user_data_update(&writer));
    validate_pub(writer.pub);
    /* test 5 : expect disabled to be inserted */
    _scenario = SCEN_EMPTY;
    fail_unless(QEO_OK == writer_user_data_update(&writer));
    validate_pub(writer.pub);
    /* fini */
    DDS_DomainParticipant_delete_publisher(factory.dp, writer.pub);
    DDS_DomainParticipantFactory_delete_participant(factory.dp);
}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    { .name = "update reader", .function = test_update_reader },
    { .name = "update writer", .function = test_update_writer },
    {NULL}
};

void register_entity_store_tests(Suite *s)
{
    TCase *tc = tcase_create("partitions tests");
    tcase_addtests(tc, tests);
    suite_add_tcase (s, tc);
}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_entity_store_tests, .name = "partitions" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "partitions",
        .desc = "partitions tests",
};

/* called before every test case starts */
static void init_tcase(void)
{
    Mockpolicy_Init();
}

/* called after every test case finishes */
static void fini_tcase(void)
{
    Mockpolicy_Verify();
    Mockpolicy_Destroy();
}

__attribute__((constructor))
void my_init(void)
{
    register_testsuite(&testsuite, testcases, init_tcase, fini_tcase);
}
