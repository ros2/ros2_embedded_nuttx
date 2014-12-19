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

#include "policy_parser.h"
#include <qeo/error.h>
#include <qeo/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int _test_index;
static int _other_index;

static void on_policy_parser_participant_found_cb(policy_parser_hndl_t parser, uintptr_t *parser_cookie, const char *id);
static void on_policy_parser_coarse_grained_rule_found_cb(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const policy_parser_permission_t *perm);
static void on_policy_parser_sequence_number_found_cb(policy_parser_hndl_t parser, uint64_t sequence_number);
static void on_policy_parser_fine_grained_rule_section_found_cb(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const char *participant_id, const policy_parser_permission_t *perm);
static void init(void){

    policy_parser_init_cfg_t init_cfg = {
        /* callbacks */
        .on_participant_found_cb = on_policy_parser_participant_found_cb,
        .on_coarse_grained_rule_found_cb = on_policy_parser_coarse_grained_rule_found_cb,
        .on_sequence_number_found_cb = on_policy_parser_sequence_number_found_cb,
        .on_fine_grained_rule_section_found_cb = on_policy_parser_fine_grained_rule_section_found_cb
    };
    ck_assert_int_eq(policy_parser_init(&init_cfg), QEO_OK);

}

static void destroy(void){

    policy_parser_destroy();

}

/* http://www.anyexample.com/programming/c/how_to_load_file_into_memory_using_plain_ansi_c_language.xml */
static int ae_load_file_to_memory(const char *filename, char **result) 
{ 
	int size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL) 
	{ 
		*result = NULL;
		return -1; // -1 means file opening fail 
	} 
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (char *)malloc(size+1);
	if (size != fread(*result, sizeof(char), size, f)) 
	{ 
		free(*result);
		return -2; // -2 means file reading fail 
	} 
	fclose(f);
	(*result)[size] = 0;
	return size;
}

struct rule_result_s {
    union {
        struct {
            const char *participant_id;

        } new_participant;
        struct {
            const char *topic_name;
            policy_parser_permission_t perms;
        } new_coarse_grained_rule;
        struct {
            const char *topic_name;
            const char *participant_id;
            policy_parser_permission_t perms;
        } new_fine_grained_rule_section;
    } u;
    enum { PARTICIPANT, COARSE, FINE } selector;
}; 

const struct rule_result_s resultpolicy1[] = {
    {
        .u.new_participant.participant_id = "uid:1",
        .selector = PARTICIPANT
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::topicone",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::homeautomation::lightswitch_request",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = false },
        .selector = COARSE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = false },
        .selector = COARSE,
    },

    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:1",
            .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:2",
            .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:4",
            .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:1",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:3",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:5",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    /* uid: 2 */
    {
        .u.new_participant.participant_id = "uid:2",
        .selector = PARTICIPANT,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::topicone",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::homeautomation::lightswitch_request",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = false },
        .selector = COARSE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = false },
        .selector = COARSE,
    },
    /* uid: 3 */
    {
        .u.new_participant.participant_id = "uid:3",
        .selector = PARTICIPANT,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:1",
            .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:2",
            .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:4",
            .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:1",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:3",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:5",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::topic1",
            .u.new_fine_grained_rule_section.participant_id = "uid:2",
            .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_participant.participant_id = "uid:4",
        .selector = PARTICIPANT,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = false },
        .selector = COARSE,
    },
    {
        .u.new_participant.participant_id = "uid:5",
        .selector = PARTICIPANT,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = false },
        .selector = COARSE,
    },
};

const struct rule_result_s result_11f_own_rw[] = {
    /* uid:120 */
    {
        .u.new_participant.participant_id = "uid:120",
        .selector = PARTICIPANT
    },
    {
        .u.new_coarse_grained_rule.topic_name = "*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::system::*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    /* uid:108 */
    {
        .u.new_participant.participant_id = "uid:108",
        .selector = PARTICIPANT
    },
    {
        .u.new_coarse_grained_rule.topic_name = "*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "topic",
        .u.new_fine_grained_rule_section.participant_id = "uid:105",
        .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "topic",
        .u.new_fine_grained_rule_section.participant_id = "uid:108",
        .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::system::*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    /* uid:11f */
    {
        .u.new_participant.participant_id = "uid:11f",
        .selector = PARTICIPANT
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::sample::simplechat::ChatMessage",
        .u.new_fine_grained_rule_section.participant_id = "uid:105",
        .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::sample::simplechat::ChatMessage",
        .u.new_fine_grained_rule_section.participant_id = "uid:11f",
        .u.new_fine_grained_rule_section.perms = { .read = true, .write = false },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::sample::simplechat::ChatMessage",
        .u.new_fine_grained_rule_section.participant_id = "uid:105",
        .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_fine_grained_rule_section.topic_name = "org::qeo::sample::simplechat::ChatMessage",
        .u.new_fine_grained_rule_section.participant_id = "uid:11f",
        .u.new_fine_grained_rule_section.perms = { .read = false, .write = true },
        .selector = FINE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::system::*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    /* uid:105 */
    {
        .u.new_participant.participant_id = "uid:105",
        .selector = PARTICIPANT
    },
    {
        .u.new_coarse_grained_rule.topic_name = "*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::system::*",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
    /* rid:4c759526656cfa6c */
    {
        .u.new_participant.participant_id = "rid:4c759526656cfa6c",
        .selector = PARTICIPANT
    },
    {
        .u.new_coarse_grained_rule.topic_name = "org::qeo::system::Policy",
        .u.new_coarse_grained_rule.perms = { .read = true, .write = true },
        .selector = COARSE,
    },
};

const struct test_result_s {
    struct {
        const char *filename;

    } test_setup;
    struct {
        const char *version;
        unsigned int sequence_number;

    } meta;
    const struct rule_result_s *other;
} _expected[] = {
    {
        .test_setup.filename = "policy1nl.txt",
        .meta.version = "1.0",
        .meta.sequence_number = 42,
        .other = resultpolicy1
    },
    {
        .test_setup.filename = "policy1crnl.txt",
        .meta.version = "1.0",
        .meta.sequence_number = 42,
        .other = resultpolicy1 
    },
    {
        .test_setup.filename = "policy1withspacestabs.txt",
        .meta.version = "1.0",
        .meta.sequence_number = 42,
        .other = resultpolicy1 
    },
    {
        .test_setup.filename = "policy_11f_own_rw.txt",
        .meta.version = "1.0",
        .meta.sequence_number = 16,
        .other = result_11f_own_rw 
    }
};

static void on_policy_parser_fine_grained_rule_section_found_cb(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const char *participant_id, const policy_parser_permission_t *perm){

    ck_assert_str_eq(topic_name, _expected[_test_index].other[_other_index].u.new_fine_grained_rule_section.topic_name);
    ck_assert_str_eq(participant_id, _expected[_test_index].other[_other_index].u.new_fine_grained_rule_section.participant_id);
    ck_assert_int_eq(perm->read, _expected[_test_index].other[_other_index].u.new_fine_grained_rule_section.perms.read);
    ck_assert_int_eq(perm->write, _expected[_test_index].other[_other_index].u.new_fine_grained_rule_section.perms.write);
    ck_assert_int_eq(FINE, _expected[_test_index].other[_other_index].selector);
    ++_other_index;

}

static void on_policy_parser_sequence_number_found_cb(policy_parser_hndl_t parser, uint64_t sequence_number){

    ck_assert_int_eq(sequence_number, _expected[_test_index].meta.sequence_number);

}

static void on_policy_parser_participant_found_cb(policy_parser_hndl_t parser, uintptr_t *parser_cookie, const char *participant_id){
        
    ck_assert_str_eq(participant_id, _expected[_test_index].other[_other_index].u.new_participant.participant_id);
    ck_assert_int_eq(PARTICIPANT, _expected[_test_index].other[_other_index].selector);

    ++_other_index;
}

static void on_policy_parser_coarse_grained_rule_found_cb(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const policy_parser_permission_t *perm){

    ck_assert_str_eq(topic_name, _expected[_test_index].other[_other_index].u.new_coarse_grained_rule.topic_name);
    ck_assert_int_eq(perm->read, _expected[_test_index].other[_other_index].u.new_coarse_grained_rule.perms.read);
    ck_assert_int_eq(perm->write, _expected[_test_index].other[_other_index].u.new_coarse_grained_rule.perms.write);
    ck_assert_int_eq(COARSE, _expected[_test_index].other[_other_index].selector);
    ++_other_index;
}


START_TEST(parser_test)
{
    char *buf = NULL;
    char path[512];
    snprintf(path, sizeof(path), "usr/local/share/policy_files/%s", _expected[_i].test_setup.filename);
    ck_assert_int_gt(ae_load_file_to_memory(path,&buf), 0);
    policy_parser_cfg_t cfg = {
        .buf = buf,
    };

    _test_index = _i;
    policy_parser_hndl_t parser = NULL;
    ck_assert_int_eq(policy_parser_construct(&cfg, &parser), QEO_OK);
    ck_assert_int_ne(parser, NULL);
    ck_assert_int_eq(policy_parser_run(parser), QEO_OK);
    ck_assert_int_eq(policy_parser_destruct(&parser), QEO_OK);

    free(buf);
    
}
END_TEST


void register_policyparsertests(Suite *s)
{
	TCase *testCase = NULL;

	testCase = tcase_create("Policy parser construct tests - no init");
    
    suite_add_tcase(s, testCase);

    testCase = tcase_create("Policy parser construct tests");
    suite_add_tcase(s, testCase);

    testCase = tcase_create("Policy parser tests");
    tcase_add_checked_fixture(testCase, init, destroy);
    tcase_add_loop_test(testCase, parser_test, 0, sizeof(_expected)/sizeof(_expected[0]));
    suite_add_tcase(s, testCase);
}
