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

#include "policy_cache.h"
#include <qeo/error.h>
#include <qeo/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static qeo_policy_cache_hndl_t _cache;

static void init(void)
{
    ck_assert_int_eq(qeo_policy_cache_construct(0,&_cache), QEO_OK);
}

static void destroy(void)
{
    ck_assert_int_eq(qeo_policy_cache_destruct(&_cache), QEO_OK);
}

struct test_item {
    const char *participant;
    const char *topic;
    struct topic_participant_list_node read_participant_list[10];
    struct topic_participant_list_node write_participant_list[10];
};


static unsigned int _result_index;
static unsigned int _cb_counter;
static const struct test_item *_current_test_expected;

static void on_cache_update_partition_string_cb(qeo_policy_cache_hndl_t cache,
                                                uintptr_t cookie,
                                                const char *participant_tag,
                                                const char *topic_name,
                                                unsigned int selector,
                                                struct topic_participant_list_node *read_participant_list,
                                                struct topic_participant_list_node *write_participant_list)
{
    struct topic_participant_list_node *el;
    int index = 0;

//    printf("result_index = %u, cb_counter %u (selector=%x)\r\n", _result_index, _cb_counter, selector);
    if (_current_test_expected == NULL){
        return;
    }
    
    ck_assert_str_eq(topic_name, _current_test_expected[_result_index].topic);
    ck_assert_str_eq(participant_tag, _current_test_expected[_result_index].participant);
    index = 0;
    LL_FOREACH(read_participant_list, el) {
//        printf("check read_participants %s with expected %s\r\n", el->participant_name, _current_test_expected[_result_index].read_participant_list[index].participant_name);
        ck_assert_str_eq(el->participant_name, _current_test_expected[_result_index].read_participant_list[index].participant_name);
//        printf("check read_participants %ld with expected %ld\r\n", el->id.user_id, _current_test_expected[_result_index].read_participant_list[index].id.user_id);
        ck_assert_int_eq(el->id.user_id, _current_test_expected[_result_index].read_participant_list[index].id.user_id);
        index++;
    }
    index = 0;
    LL_FOREACH(write_participant_list, el) {
//        printf("check write_participants %s with expected %s\r\n", el->participant_name, _current_test_expected[_result_index].write_participant_list[index].participant_name);
        ck_assert_str_eq(el->participant_name, _current_test_expected[_result_index].write_participant_list[index].participant_name);
//        printf("check write_participants %ld with expected %ld\r\n", el->id.user_id, _current_test_expected[_result_index].write_participant_list[index].id.user_id);
        ck_assert_int_eq(el->id.user_id, _current_test_expected[_result_index].write_participant_list[index].id.user_id);
        index++;
    }

    ++_cb_counter;
    ++_result_index;
}

START_TEST(api_invalid_args)
{
    qeo_policy_cache_hndl_t cache = (qeo_policy_cache_hndl_t)0xdeadbabe;

    fail_unless(QEO_EINVAL == qeo_policy_cache_get_cookie(NULL, (uintptr_t*)0xdeadbeef));
    fail_unless(QEO_EINVAL == qeo_policy_cache_get_cookie(cache, NULL));

    fail_unless(QEO_EINVAL == qeo_policy_cache_set_seq_number(NULL, 0));

    fail_unless(QEO_EINVAL == qeo_policy_cache_get_number_of_participants(NULL, (unsigned int*)0xdeadbabe));
    fail_unless(QEO_EINVAL == qeo_policy_cache_get_number_of_participants(cache, NULL));

    fail_unless(QEO_EINVAL == qeo_policy_cache_get_number_of_topics(NULL, (unsigned int*)0xdeadbabe));
    fail_unless(QEO_EINVAL == qeo_policy_cache_get_number_of_topics(cache, NULL));

    fail_unless(QEO_EINVAL == qeo_policy_cache_add_participant_tag(NULL, (const char*)0xdeadbabe));
    fail_unless(QEO_EINVAL == qeo_policy_cache_add_participant_tag(cache, NULL));

    fail_unless(QEO_EINVAL == qeo_policy_cache_add_coarse_grained_rule(NULL, (const char*)0xdeadbabe,(const char*)0xdeadbabe, (const policy_parser_permission_t *)0xdeadbabe));

    fail_unless(QEO_EINVAL == qeo_policy_cache_add_fine_grained_rule_section(NULL, (const char*)0xdeadbabe,(const char*)0xdeadbabe, (const policy_parser_permission_t *)0xdeadbabe, (const char*)0xdeadbabe));

    fail_unless(QEO_EINVAL == qeo_policy_cache_get_topic_rules(NULL, (uintptr_t)0xdeadbeef, (const char*)0xdeadbabe,(const char*)0xdeadbabe, 0, (qeo_policy_cache_update_topic_cb)0xdeadcbcb));

    fail_unless(QEO_EINVAL == qeo_policy_cache_finalize(NULL));

    fail_unless(QEO_EINVAL == qeo_policy_cache_get_participants(NULL, (qeo_policy_cache_participant_cb)0xdeadbabe));
    fail_unless(QEO_EINVAL == qeo_policy_cache_get_participants(cache, NULL));

    fail_unless(QEO_EINVAL == qeo_policy_cache_reset(NULL));

    fail_unless(QEO_OK == qeo_policy_cache_destruct(NULL));

    cache = NULL;
    fail_unless(QEO_OK == qeo_policy_cache_destruct(&cache));
}
END_TEST

static const struct test_item _result_only_coarse[] = {
    //Read
    {
        .participant = "uid:1",
        .topic = "qeo.org.*",
    },
    {
        .participant = "uid:1",
        .topic = "topic1",
    },
    {
        .participant = "uid:1",
        .topic = "topic2",
    },
    {
        .participant = "uid:1",
        .topic = "*",
    },

    {
        .participant = "uid:2",
        .topic = "qeo.org.topic",
    },
    {
        .participant = "uid:2",
        .topic = "qeo.org.*",
    },

    //Write
    {
        .participant = "uid:1",
        .topic = "topic1",
    },
    {
        .participant = "uid:2",
        .topic = "qeo.org.topic",
    },
};

START_TEST(only_coarse)
{

    unsigned int num_topics;
    unsigned int num_participants;

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  42), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:1"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:2"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:3"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_get_number_of_participants(_cache, &num_participants), QEO_OK);
    ck_assert_int_eq(num_participants, 3);

    policy_parser_permission_t rwperms = { .read = true, .write = true };
    policy_parser_permission_t rperms = { .read = true, .write = false };
    policy_parser_permission_t noperms = { .read = false, .write = false };

    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "topic1", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "topic2", &rperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "qeo.org.topic", &noperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "*", &rperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:2", "qeo.org.*", &rperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:2", "qeo.org.topic", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:2", "topic1", &noperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_get_number_of_topics(_cache, &num_topics), QEO_OK);
    ck_assert_int_eq(num_topics, 5);

    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);
    
    _current_test_expected = _result_only_coarse;
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL, NULL, TOPIC_PARTICIPANT_SELECTOR_READ, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL, NULL, TOPIC_PARTICIPANT_SELECTOR_WRITE, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(_result_index, sizeof(_result_only_coarse) / sizeof(_result_only_coarse[0]));
}
END_TEST

static const struct test_item _result_bruno_example[] = {
    /* uid:1 */
    {
        .participant = "uid:1",
        .topic = "prefix.*",
        .read_participant_list = {{{2}, "uid:2", NULL}, {{1}, "uid:1", NULL}},
        .write_participant_list = {{{2}, "uid:2", NULL}, {{1}, "uid:1", NULL}},
    },
    {
        .participant = "uid:1",
        .topic = "topic1",
    },
    {
        .participant = "uid:1",
        .topic = "topic2",
    },
    {
        .participant = "uid:1",
        .topic = "*",
    },
    /* uid:2 */
    {
        .participant = "uid:2",
        .topic = "prefix.*",
        .read_participant_list = {{{2}, "uid:2", NULL}},
        .write_participant_list = {{{1}, "uid:1", NULL}},
    },
    {
        .participant = "uid:2",
        .topic = "topic1",
    },
    {
        .participant = "uid:2",
        .topic = "topic2",
    },
    {
        .participant = "uid:2",
        .topic = "*",
    },
};

/* note that the original example from Bruno De Bus contained a mistake in the solution for uid: 2
 * For reference, I have included the updated,corrected example in this directory in debusb_corrected_example.txt*/ 
START_TEST(bruno_example){

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  92), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:1"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:2"), QEO_OK);

    policy_parser_permission_t rwperms = { .read = true, .write = true };
    policy_parser_permission_t wperms = { .read = false, .write = true };
    policy_parser_permission_t rperms = { .read = true, .write = false };

    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "topic1", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "*", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:2", "topic2", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:2", "*", &rperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:2", "prefix.*", &rperms, "uid:2"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:2", "prefix.*", &wperms, "uid:1"), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);

    _current_test_expected = _result_bruno_example;
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL,NULL, TOPIC_PARTICIPANT_SELECTOR_ALL, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(_result_index, sizeof(_result_bruno_example) / sizeof(_result_bruno_example[0]));

}
END_TEST

static const struct test_item _result_mixed[] = {
    /* uid:1 */
    {
        .participant = "uid:1",
        .topic = "topic1",
        .read_participant_list = {{{2}, "uid:2", NULL}, {{1}, "uid:1", NULL}},
        .write_participant_list = {{{2}, "uid:2", NULL}, {{1}, "uid:1", NULL}},
    },
    /* uid:2 */
    {
        .participant = "uid:2",
        .topic = "topic1",
        .read_participant_list = {{{2}, "uid:2", NULL}},
    },
};


START_TEST(mix_coarse_fine){

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  UINT64_MAX), QEO_OK);

    policy_parser_permission_t rwperms = { .read = true, .write = true };
    policy_parser_permission_t rperms = { .read = true, .write = false };

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:1"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:2"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:1", "topic1", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:2", "topic1", &rperms, "uid:2"), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);
    _current_test_expected = _result_mixed;
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL,NULL, TOPIC_PARTICIPANT_SELECTOR_ALL, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(_result_index, sizeof(_result_mixed) / sizeof(_result_mixed[0]));

}
END_TEST

static const struct test_item _result_own_rw[] = {
    /* uid:1 */
    {
        .participant = "uid:1",
        .topic = "topic1",
        .read_participant_list = {{{2}, "uid:2", NULL}, {{1}, "uid:1", NULL}},
        .write_participant_list = {{{2}, "uid:2", NULL}, {{1}, "uid:1", NULL}},
    },
};


START_TEST(own_rw){

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  2), QEO_OK);

    policy_parser_permission_t rperms = { .read = true, .write = false };
    policy_parser_permission_t wperms = { .read = false, .write = true };

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:1"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:2"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:1", "topic1", &rperms, "uid:1"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:1", "topic1", &rperms, "uid:2"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:1", "topic1", &wperms, "uid:1"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:1", "topic1", &wperms, "uid:2"), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);
    _current_test_expected = _result_own_rw;
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL,NULL, TOPIC_PARTICIPANT_SELECTOR_ALL, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(_result_index, sizeof(_result_own_rw) / sizeof(_result_own_rw[0]));
}
END_TEST


static const struct test_item _result_mix_coarse_fine_wc[] = {
    /* uid:37c */
    {
        .participant = "uid:37c",
        .topic = "org.qeo.sample.simplechat.ChatMessage",
        .read_participant_list = {{{892}, "uid:37c", NULL}, {{893}, "uid:37d", NULL}},
        .write_participant_list = {{{892}, "uid:37c", NULL}, {{893}, "uid:37d", NULL}},
    },
    {
        .participant = "uid:37c",
        .topic = "org.qeo.sample.simplechat.*",
        .read_participant_list = {{{893}, "uid:37d", NULL}, {{892}, "uid:37c", NULL}},
        .write_participant_list = {{{893}, "uid:37d", NULL}, {{892}, "uid:37c", NULL}},
    },
    {
        .participant = "uid:37c",
        .topic = "org.qeo.system.Policy",
    },
    {
        .participant = "uid:37c",
        .topic = "org.qeo.system.*",
    },
    /* uid:37d */
    {
        .participant = "uid:37d",
        .topic = "org.qeo.sample.simplechat.ChatMessage",
        .read_participant_list = {{{893}, "uid:37d", NULL}, {{892}, "uid:37c", NULL}},
        .write_participant_list = {{{893}, "uid:37d", NULL}, {{892}, "uid:37c", NULL}},
    },
    {
        .participant = "uid:37d",
        .topic = "org.qeo.sample.simplechat.*",
        .read_participant_list = {{{893}, "uid:37d", NULL}, {{892}, "uid:37c", NULL}},
    },
    {
        .participant = "uid:37d",
        .topic = "org.qeo.system.Policy",
    },
    {
        .participant = "uid:37d",
        .topic = "org.qeo.system.*",
    },
    /* rid:a64a3c30bf365dc */
    {
        .participant = "rid:a64a3c30bf365dc",
        .topic = "org.qeo.system.Policy",
    },
};


/*
 * [meta]
version=1.0
seqnr=7
[uid:37c]
org::qeo::sample::simplechat::*=r<uid:37c;uid:37d>w<uid:37c;uid:37d>
org::qeo::system::*=rw
[uid:37d]
org::qeo::sample::simplechat::ChatMessage=rw
org::qeo::sample::simplechat::*=r
org::qeo::system::*=rw
[rid:a64a3c30bf365dc]
org::qeo::system::Policy=rw

*/

START_TEST(mix_coarse_fine_wc){

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  7), QEO_OK);

    policy_parser_permission_t rperms = { .read = true, .write = false };
    policy_parser_permission_t wperms = { .read = false, .write = true };
    policy_parser_permission_t rwperms = { .read = true, .write = true };
    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:37c"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:37c", "org.qeo.sample.simplechat.*", &rperms, "uid:37c"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:37c", "org.qeo.sample.simplechat.*", &rperms, "uid:37d"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:37c", "org.qeo.sample.simplechat.*", &wperms, "uid:37c"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:37c", "org.qeo.sample.simplechat.*", &wperms, "uid:37d"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:37c", "org.qeo.system.*", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:37d"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:37d", "org.qeo.sample.simplechat.ChatMessage",&rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:37d", "org.qeo.sample.simplechat.*",&rperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:37d", "org.qeo.system.*", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "rid:a64a3c30bf365dc"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "rid:a64a3c30bf365dc", "org.qeo.system.Policy", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);
    _current_test_expected = _result_mix_coarse_fine_wc;
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL,NULL, TOPIC_PARTICIPANT_SELECTOR_ALL, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(_result_index, sizeof(_result_mix_coarse_fine_wc) / sizeof(_result_mix_coarse_fine_wc[0]));

}
END_TEST

static const struct test_item _result_policy_topic[] = {
    /* uid:37c */
    {
        .participant = "uid:37c",
        .topic = "org.qeo.system.Policy",
        .read_participant_list = {{{892}, "uid:37c", NULL}},
        .write_participant_list = {{{892}, "uid:37c", NULL}},
    },
    {
        .participant = "uid:37c",
        .topic = "*",
        .write_participant_list = {{{892}, "uid:37c", NULL}},
    },
};
/*
 * [meta]
version=1.0
seqnr=7
[uid:37c]
org::qeo::system::Policy=rw
*=r<uid:37c> w<uid:37c>

*/

START_TEST(policy_topic){

    policy_parser_permission_t wperms = { .read = false, .write = true };
    policy_parser_permission_t rwperms = { .read = true, .write = true };

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  7), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:37c"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:37c", "org.qeo.system.Policy", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:37c", "*", &wperms, "uid:37c"), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);
    _current_test_expected = _result_policy_topic;
    ck_assert_int_eq(qeo_policy_cache_get_topic_rules(_cache, 0, NULL,NULL, TOPIC_PARTICIPANT_SELECTOR_ALL, on_cache_update_partition_string_cb), QEO_OK);
    ck_assert_int_eq(_result_index, sizeof(_result_policy_topic) / sizeof(_result_policy_topic[0]));
}
END_TEST


/* This 'test' was only added to better analyze de2525 */
#if 0
START_TEST(de2525_44b){

    setenv("POLICY_DUMP", "1", 1);
    policy_parser_permission_t rperms = { .read = true, .write = false };
    policy_parser_permission_t wperms = { .read = false, .write = true };
    policy_parser_permission_t rwperms = { .read = true, .write = true };

    ck_assert_int_eq(qeo_policy_cache_set_seq_number(_cache,  36), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b7", "*", &rperms, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b7", "*", &rperms, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b7", "*", &rperms, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b7", "*", &wperms, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b7", "*", &wperms, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b7", "*", &wperms, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:5b7", "org.qeo.system.*", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:5b7", "org.qeo.system.Policy", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b5", "*", &rperms, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b5", "*", &rperms, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b5", "*", &rperms, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b5", "*", &wperms, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b5", "*", &wperms, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b5", "*", &wperms, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:5b5", "org.qeo.system.*", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:5b5", "org.qeo.system.Policy", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b6", "*", &rperms, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b6", "*", &rperms, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b6", "*", &rperms, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b6", "*", &wperms, "uid:5b5"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b6", "*", &wperms, "uid:5b6"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_fine_grained_rule_section(_cache, "uid:5b6", "*", &wperms, "uid:5b7"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:5b6", "org.qeo.system.*", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "uid:5b6", "org.qeo.system.Policy", &rwperms), QEO_OK);

    ck_assert_int_eq(qeo_policy_cache_add_participant_tag(_cache, "rid:44b061e03331c796"), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_add_coarse_grained_rule(_cache, "rid:44b061e03331c796", "org.qeo.system.Policy", &rwperms), QEO_OK);
    ck_assert_int_eq(qeo_policy_cache_finalize(_cache), QEO_OK);

}
END_TEST
#endif

void register_policycachetests(Suite *s)
{

	TCase *testCase = NULL;

	testCase = tcase_create("Policy cache API tests");
    tcase_add_test(testCase, api_invalid_args);
    suite_add_tcase(s, testCase);

    testCase = tcase_create("Policy cache tests");
    tcase_add_checked_fixture(testCase, init, destroy);
    tcase_add_test(testCase, only_coarse);
    tcase_add_test(testCase, bruno_example);
    tcase_add_test(testCase, mix_coarse_fine);
    tcase_add_test(testCase, own_rw);
    tcase_add_test(testCase, mix_coarse_fine_wc);
    tcase_add_test(testCase, policy_topic);
    //tcase_add_test(testCase, de2525_44b);
    suite_add_tcase(s, testCase);
}
