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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <assert.h>
#include "check.h"

#include "unittest/unittest.h"

#include <qeojson/api.h>

/* ===[ public API tests ]=================================================== */

START_TEST(test_factory_api_inv_inargs)
{
    qeo_json_factory_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_json_factory_create_by_id(NULL, NULL, 0));
    fail_unless(QEO_EINVAL == qeo_json_factory_create_by_id("open", NULL, 0));
}
END_TEST


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

static char *_event_type;
static char *_state_type;
static qeo_factory_t *_factory = NULL;
static sem_t _sync;

static void event_on_data(const qeo_json_event_reader_t *reader,
                    const char *json_data,
                    uintptr_t userdata)
{

}

static void event_on_no_more_data(const qeo_json_event_reader_t *reader,
                            uintptr_t userdata)
{

}

static void event_reader_on_policy_update (const qeo_json_event_reader_t *reader,
                                           const char *json_policy,
                                           uintptr_t userdata)
{

}

static void event_writer_on_policy_update (const qeo_json_event_writer_t *writer,
                                           const char *json_policy,
                                           uintptr_t userdata)
{

}

static void state_on_update(const qeo_json_state_reader_t *reader,
                                                  uintptr_t userdata){

}

static void  state_reader_on_policy_update (const qeo_json_state_reader_t *reader,
                                           const char *json_policy,
                                           uintptr_t userdata)
{

}

static void state_writer_on_policy_update (const qeo_json_state_writer_t *writer,
                                           const char *json_policy,
                                           uintptr_t userdata)
{

}


START_TEST(test_event_api_inv_inargs)
{
    qeo_factory_t *f = (qeo_factory_t *)0xdeadbeef;
    qeo_json_event_writer_t *w = (qeo_json_event_writer_t *)0xdeadbeef;
    qeo_json_event_reader_listener_t rcbs = {0};
    qeo_json_event_writer_listener_t wcbs = {0};

    /* reader */
    fail_unless(NULL == qeo_json_factory_create_event_reader(NULL, _event_type, &rcbs, 0));
    fail_unless(NULL == qeo_json_factory_create_event_reader(f, NULL, &rcbs, 0));
    fail_unless(NULL == qeo_json_factory_create_event_reader(f, _event_type, NULL, 0));
    fail_unless(NULL == qeo_json_factory_create_event_reader(f, _event_type, &rcbs, 0)); /* on_data non-NULL */
    qeo_json_event_reader_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_json_event_reader_policy_update(NULL, NULL));

    /* writer */
    fail_unless(NULL == qeo_json_factory_create_event_writer(NULL, _event_type, NULL, 0));
    fail_unless(NULL == qeo_json_factory_create_event_writer(f, NULL, &wcbs, 0));
    /* TBD: WILL WE CHECK ON on_policy_update ?? */
    //fail_unless(NULL == qeo_json_factory_create_event_writer(f, _event_type, &wcbs, 0)); /* on_policy_update non-NULL */
    fail_unless(QEO_EINVAL == qeo_json_event_writer_write(w, NULL));
    qeo_json_event_writer_close(NULL); /* don't crash */
    fail_unless(QEO_EINVAL == qeo_json_event_writer_policy_update(NULL, NULL));
}
END_TEST

START_TEST(test_event_reader_api_inargs)
{
    qeo_json_event_reader_listener_t rcbs = {
        .on_data = event_on_data,
        .on_no_more_data = event_on_no_more_data,
        .on_policy_update = event_reader_on_policy_update
    };
    qeo_json_event_reader_t *reader;


    /* reader */
    reader = qeo_json_factory_create_event_reader(_factory, _state_type, &rcbs, 0);
    ck_assert_int_eq(reader, NULL);
    reader = qeo_json_factory_create_event_reader(_factory, _event_type, &rcbs, 0);
    ck_assert_int_ne(reader, NULL);
    qeo_json_event_reader_close(reader); /* don't crash */
    reader = NULL;
    fail_unless(QEO_EINVAL == qeo_json_event_reader_policy_update(reader, NULL));

}
END_TEST

START_TEST(test_event_writer_api_inargs)
{
    qeo_json_event_writer_listener_t wcbs = {
        .on_policy_update = event_writer_on_policy_update
    };
    qeo_json_event_writer_t *writer;


    /* writer */
    writer = qeo_json_factory_create_event_writer(_factory, _state_type, &wcbs, 0);
    ck_assert_int_eq(writer, NULL);
    writer = qeo_json_factory_create_event_writer(_factory, _event_type, &wcbs, 0);
    ck_assert_int_ne(writer, NULL);
    qeo_json_event_writer_close(writer); /* don't crash */
    writer = NULL;
    fail_unless(QEO_EINVAL == qeo_json_event_writer_policy_update(writer, NULL));

}
END_TEST

START_TEST(test_state_reader_api_inargs)
{
    qeo_json_state_reader_listener_t rcbs = {
        .on_update = state_on_update,
        .on_policy_update = state_reader_on_policy_update
    };
    qeo_json_state_reader_t *reader;


    /* reader */
    reader = qeo_json_factory_create_state_reader(_factory, _event_type, &rcbs, 0);
    ck_assert_int_eq(reader, NULL);
    reader = qeo_json_factory_create_state_reader(_factory, _state_type, &rcbs, 0);
    ck_assert_int_ne(reader, NULL);
    qeo_json_state_reader_close(reader); /* don't crash */
    reader = NULL;
    fail_unless(QEO_EINVAL == qeo_json_state_reader_policy_update(reader, NULL));

}
END_TEST

START_TEST(test_state_writer_api_inargs)
{
    qeo_json_state_writer_listener_t wcbs = {
        .on_policy_update = state_writer_on_policy_update
    };
    qeo_json_state_writer_t *writer;
    qeo_json_state_writer_t *copy_writer;


    /* writer */
    writer = qeo_json_factory_create_state_writer(_factory, _event_type, &wcbs, 0);
    ck_assert_int_eq(writer, NULL);
    writer = qeo_json_factory_create_state_writer(_factory, _state_type, &wcbs, 0);
    ck_assert_int_ne(writer, NULL);
    copy_writer = writer;
    writer = NULL;
    qeo_json_state_writer_close(writer); /* don't crash */
    writer = NULL;
    fail_unless(QEO_EINVAL == qeo_json_state_writer_policy_update(writer, NULL));

    qeo_json_state_writer_close(copy_writer);

}
END_TEST

START_TEST(test_device_id_api_inargs)
{
    char * json_device_id = NULL;

    fail_unless(QEO_EINVAL == qeo_json_get_device_id(NULL));
    fail_unless(QEO_OK == qeo_json_get_device_id(&json_device_id));
    fail_unless(strlen(json_device_id) > 0);
    
    free(json_device_id);


}
END_TEST

/* ===[ test setup ]========================================================= */

static singleTestCaseInfo tests[] =
{
    /* public API */
    { .name = "event reader API input args", .function = test_event_reader_api_inargs },
    { .name = "factory API input args", .function = test_factory_api_inv_inargs },
    { .name = "event API invalid input args", .function = test_event_api_inv_inargs },
    { .name = "event writer API input args", .function = test_event_writer_api_inargs },
    { .name = "state reader API input args", .function = test_state_reader_api_inargs },
    { .name = "state writer API input args", .function = test_state_writer_api_inargs },
    { .name = "device id API input args", .function = test_device_id_api_inargs },
    {NULL},
};

static void qeo_json_factory_init_done (qeo_factory_t *factory,
                                               uintptr_t userdata){

    _factory = factory;
    /* release main thread */
    sem_post(&_sync);

}

static void load_type(void){

    char path[333];
    snprintf(path, sizeof(path), "usr/local/share/event_type.json");
    ck_assert_int_gt(ae_load_file_to_memory(path, &_event_type), 0);
    ck_assert_int_ne(_event_type, NULL);

    snprintf(path, sizeof(path), "usr/local/share/state_type.json");
    ck_assert_int_gt(ae_load_file_to_memory(path, &_state_type), 0);
    ck_assert_int_ne(_state_type, NULL);
}

static void free_type(void){

    free(_event_type);
    free(_state_type);
}

static void start_factory(void){

    qeo_json_factory_listener_t factory_listener = {
        .on_factory_init_done=qeo_json_factory_init_done
    };
    assert(QEO_OK == qeo_json_factory_create(&factory_listener, 0));
    sem_wait(&_sync);
}

static void close_factory(void){

    qeo_json_factory_close(_factory);
}

void register_type_support_tests(Suite *s)
{
    TCase *tc = tcase_create("API input args tests");
    tcase_addtests(tc, tests);
    tcase_add_checked_fixture (tc, load_type, free_type);
    tcase_add_checked_fixture (tc, start_factory, close_factory);
    suite_add_tcase (s, tc);

}

static testCaseInfo testcases[] =
{
    { .register_testcase = register_type_support_tests, .name = "API" },
    {NULL}
};

static testSuiteInfo testsuite =
{
        .name = "API",
        .desc = "API tests",
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
