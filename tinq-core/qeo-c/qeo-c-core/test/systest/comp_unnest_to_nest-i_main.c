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

#include <assert.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <qeocore/api.h>
#include "common.h"
#include "verbose.h"

static sem_t _sync;

// population values for outer struct
static int _outer_int8_value = 111;
static int _outer_int32_value = 1111111111;
static char* _outer_string_value = "content of _outer_string_value";
static int _outer_int16_value = 11111;
static int64_t _outer_int64_value = INT64_C(1111111111111111111);

static qeocore_member_id_t _outer_int32_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _outer_int8_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _outer_string_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _outer_int16_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _outer_int64_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _inner_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _inner_int32_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _inner_int8_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _inner_string_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _inner_int16_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _inner_int64_id = QEOCORE_MEMBER_ID_DEFAULT;

static void my_on_data_available(const qeocore_reader_t *reader,
                                 const qeocore_data_t *data,
                                 uintptr_t userdata)
{
    // the reader is a type_nest-a reader
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {

            int outer_int32_value_rx=0, outer_int8_value_rx=0, outer_int16_value_rx=0;
            int inner_int32_value_rx=0, inner_int8_value_rx=0, inner_int16_value_rx=0;
            int64_t outer_int64_value_rx=0;
            int64_t inner_int64_value_rx=0;
            char* outer_string_value_rx="";
            char* inner_string_value_rx="";
            qeocore_data_t *inner_value_rx=NULL;
            log_pid("===================================== reader received data");
            assert(QEO_OK == qeocore_data_get_member(data, _outer_int32_id, &outer_int32_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _outer_int8_id, &outer_int8_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _outer_string_id, &outer_string_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _outer_int16_id, &outer_int16_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _outer_int64_id, &outer_int64_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _inner_id, &inner_value_rx));
            assert(QEO_OK == qeocore_data_get_member(inner_value_rx, _inner_int32_id, &inner_int32_value_rx));
            assert(QEO_OK == qeocore_data_get_member(inner_value_rx, _inner_int8_id, &inner_int8_value_rx));
            assert(QEO_OK == qeocore_data_get_member(inner_value_rx, _inner_string_id, &inner_string_value_rx));
            assert(QEO_OK == qeocore_data_get_member(inner_value_rx, _inner_int16_id, &inner_int16_value_rx));
            assert(QEO_OK == qeocore_data_get_member(inner_value_rx, _inner_int64_id, &inner_int64_value_rx));
            log_verbose(" =================== outer_int32_value_rx = %u \n", outer_int32_value_rx );
            log_verbose(" =================== outer_int8_value_rx = %u \n", outer_int8_value_rx );
            log_verbose(" =================== outer_string_value_rx = \"%s\" \n", outer_string_value_rx );
            log_verbose(" =================== outer_int16_value_rx = %u \n", outer_int16_value_rx );
            log_verbose(" =================== outer_int64_value_rx = %"PRIu64" \n", outer_int64_value_rx );
            log_verbose(" =================== inner_int32_value_rx = %u \n", inner_int32_value_rx );
            log_verbose(" =================== inner_int8_value_rx = %u \n", inner_int8_value_rx );
            log_verbose(" =================== inner_string_value_rx = \"%s\" \n", inner_string_value_rx );
            log_verbose(" =================== inner_int16_value_rx = %u \n", inner_int16_value_rx );
            log_verbose(" =================== inner_int64_value_rx = %"PRIu64" \n", inner_int64_value_rx );
            assert(outer_int32_value_rx==_outer_int32_value);
            assert(outer_int8_value_rx==_outer_int8_value);
            assert(0 == strcmp(_outer_string_value, outer_string_value_rx));
            assert(outer_int16_value_rx==_outer_int16_value);
            assert(outer_int64_value_rx==_outer_int64_value);
            assert(inner_int32_value_rx==0);
            assert(inner_int8_value_rx==0);
            assert(0 == strcmp("", inner_string_value_rx));
            assert(inner_int16_value_rx==0);
            assert(inner_int64_value_rx==0);
            qeocore_data_free(inner_value_rx);
            free(outer_string_value_rx);
            free(inner_string_value_rx);
            sem_post(&_sync); /* release main thread */
            break;
        }
        case QEOCORE_NO_MORE_DATA:
        case QEOCORE_REMOVE:
            /* ignore */
            break;
        default:
            abort();
            break;
    }
}

static qeocore_type_t *type_unnest_register(qeo_factory_t *factory)
{
    qeocore_type_t *outer = NULL;
    qeocore_type_t *member = NULL;
    // the outer struct
    assert(NULL != (outer = qeocore_type_struct_new("org.qeo.test.NestedTests")));
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int32", &_outer_int32_id, QEOCORE_FLAG_KEY));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int8", &_outer_int8_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_string_new(0)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_string", &_outer_string_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT16)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int16", &_outer_int16_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT64)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int64", &_outer_int64_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(QEO_OK == qeocore_type_register(factory, outer, "org.qeo.test.QDMTestDynType"));
    return outer;
}

static qeocore_type_t *type_nest_i_register(qeo_factory_t *factory)
{
    qeocore_type_t *outer = NULL;
    qeocore_type_t *inner = NULL;
    qeocore_type_t *member = NULL;
    // the outer struct level
    assert(NULL != (outer = qeocore_type_struct_new("org.qeo.test.NestedTests")));
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int32", &_outer_int32_id, QEOCORE_FLAG_KEY));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int8", &_outer_int8_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_string_new(0)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_string", &_outer_string_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);

    // the inner struct is inserted
    assert(NULL != (inner = qeocore_type_struct_new("inner")));
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, "inner_int32", &_inner_int32_id, QEOCORE_FLAG_KEY));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, "inner_int8", &_inner_int8_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_string_new(0)));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, "inner_string", &_inner_string_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT16)));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, "inner_int16", &_inner_int16_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT64)));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, "inner_int64", &_inner_int64_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);

    // add inner to outer
    assert(QEO_OK == qeocore_type_struct_add(outer, inner, "inner", &_inner_id, QEOCORE_FLAG_NONE));

    // the outer struct level - continued
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT16)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int16", &_outer_int16_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT64)));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, "outer_int64", &_outer_int64_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);

    // register the outer type
    assert(QEO_OK == qeocore_type_register(factory, outer, "org.qeo.test.QDMTestDynType"));
    // free
    qeocore_type_free(inner);
    return outer;
}

static void run_writer(pid_t peer)
{
    // writer is a type_unnest writer
    qeo_factory_t *factory;
    qeocore_type_t *type_unnest;
    qeocore_writer_t *writer;
    qeocore_data_t *outer=NULL;

    int status;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type_unnest = type_unnest_register(factory)));
    assert(NULL != (writer = qeocore_writer_open(factory, type_unnest, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    log_pid("=================================== writer initialized");
    assert(NULL != (outer = qeocore_writer_data_new(writer)));
    /* fill outer struct */
    assert(QEO_OK == qeocore_data_set_member(outer, _outer_int32_id, &_outer_int32_value));
    assert(QEO_OK == qeocore_data_set_member(outer, _outer_int8_id, &_outer_int8_value));
    assert(QEO_OK == qeocore_data_set_member(outer, _outer_string_id, &_outer_string_value));
    assert(QEO_OK == qeocore_data_set_member(outer, _outer_int16_id, &_outer_int16_value));
    assert(QEO_OK == qeocore_data_set_member(outer, _outer_int64_id, &_outer_int64_value));

    log_verbose(" =================== _outer_int32_value = %u \n", _outer_int32_value );
    log_verbose(" =================== _outer_int8_value = %u \n", _outer_int8_value );
    log_verbose(" =================== _outer_int16_value = %u \n", _outer_int16_value );
    log_verbose(" =================== _outer_int64_value = %"PRIu64" \n", _outer_int64_value );
    /* write */
    assert(QEO_OK == qeocore_writer_write(writer, outer));
    log_pid("===================================== writer wrote outer data");
    assert(peer == waitpid(peer, &status, 0));
    assert(0 == status);
    log_pid("===================================== writer done");
    /* clean up */
    qeocore_data_free(outer);
    qeocore_writer_close(writer);
    qeocore_type_free(type_unnest);
    qeocore_factory_close(factory);
}

static void run_reader(void)
{
    qeo_factory_t *factory;
    qeocore_type_t *type_nest_i;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = my_on_data_available };

    sem_init(&_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type_nest_i = type_nest_i_register(factory)));
    assert(NULL != (reader = qeocore_reader_open(factory, type_nest_i, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    log_pid("=============================== reader initialized");
    sem_wait(&_sync); /* wait for sample */
    log_pid("=============================== reader done");
    sem_destroy(&_sync);
    qeocore_reader_close(reader);
    qeocore_type_free(type_nest_i);
    qeocore_factory_close(factory);
}

int main(int argc, const char **argv)
{
    log_verbose(" =================== 20140410 \n");
    pid_t pidreader;

    pidreader = fork();
    assert(-1 != pidreader);
    if (0 == pidreader) {
        run_reader();
    }
    else {
        run_writer(pidreader);
    }
    return 0;
}
