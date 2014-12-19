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

// population values for version 1
static int _int8_value = 111;
static int _int32_value = 1111111111;
static char* _string_value = "content of _string_value";
// population values for version 2
static int _int8_2_value = 222;
static int _int32_2_value = 2222222222;
static char* _string_2_value = "content of _string_2_value";

static qeocore_member_id_t _int32_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _int8_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _string_id = QEOCORE_MEMBER_ID_DEFAULT;

static qeocore_member_id_t _int32_2_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _int8_2_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _string_2_id = QEOCORE_MEMBER_ID_DEFAULT;

static void my_on_data_available(const qeocore_reader_t *reader,
                                 const qeocore_data_t *data,
                                 uintptr_t userdata)
{
    // the reader is a type1 reader
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {

            int int32_value_rx=0, int8_value_rx=0;
            char* string_value_rx="";
            log_pid("reader received data");
            assert(QEO_OK == qeocore_data_get_member(data, _int32_id, &int32_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _int8_id, &int8_value_rx));
            assert(QEO_OK == qeocore_data_get_member(data, _string_id, &string_value_rx));
            log_verbose(" =================== int32_value_rx = %u \n", int32_value_rx );
            log_verbose(" =================== int8_value_rx = %u \n", int8_value_rx );
            log_verbose(" =================== string_value_rx = \"%s\" \n", string_value_rx );
            assert(int32_value_rx==_int32_value);
            assert(int8_value_rx==_int8_value);
            assert(0 == strcmp(_string_value, string_value_rx));
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

static qeocore_type_t *type1_register(qeo_factory_t *factory)
{
    qeocore_type_t *dyndata = NULL;
    qeocore_type_t *primitive = NULL;

    assert(NULL != (dyndata = qeocore_type_struct_new("org.qeo.test.QDMTestDynType")));
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "int32", &_int32_id, QEOCORE_FLAG_KEY));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "int8", &_int8_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_string_new(0)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "string", &_string_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);

    assert(QEO_OK == qeocore_type_register(factory, dyndata, "org.qeo.test.QDMTestDynType"));
    return dyndata;
}

static qeocore_type_t *type2_register(qeo_factory_t *factory)
{
    qeocore_type_t *dyndata = NULL;
    qeocore_type_t *primitive = NULL;

    assert(NULL != (dyndata = qeocore_type_struct_new("org.qeo.test.QDMTestDynType")));
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "int32", &_int32_id, QEOCORE_FLAG_KEY));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "int8", &_int8_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_string_new(0)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "string", &_string_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);

    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "int32_2", &_int32_2_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "int8_2", &_int8_2_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_string_new(0)));
    assert(QEO_OK == qeocore_type_struct_add(dyndata, primitive, "string_2", &_string_2_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(primitive);

    assert(QEO_OK == qeocore_type_register(factory, dyndata, "org.qeo.test.QDMTestDynType"));
    return dyndata;
}


static void run_writer(pid_t peer)
{
    // writer is a type 2 writer
    qeo_factory_t *factory;
    qeocore_type_t *type2;
    qeocore_writer_t *writer;
    qeocore_data_t *data;
    int status;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type2 = type2_register(factory)));
    assert(NULL != (writer = qeocore_writer_open(factory, type2, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    log_pid("writer initialized");
    assert(NULL != (data = qeocore_writer_data_new(writer)));
    assert(QEO_OK == qeocore_data_set_member(data, _int32_id, &_int32_value));
    assert(QEO_OK == qeocore_data_set_member(data, _int8_id, &_int8_value));
    assert(QEO_OK == qeocore_data_set_member(data, _string_id, &_string_value));
    assert(QEO_OK == qeocore_data_set_member(data, _int32_2_id, &_int32_2_value));
    assert(QEO_OK == qeocore_data_set_member(data, _int8_2_id, &_int8_2_value));
    assert(QEO_OK == qeocore_data_set_member(data, _string_2_id, &_string_2_value));
    /* write */
    assert(QEO_OK == qeocore_writer_write(writer, data));
    log_pid("writer wrote data");
    assert(peer == waitpid(peer, &status, 0));
    assert(0 == status);
    log_pid("writer done");
    /* clean up */
    qeocore_data_free(data);
    qeocore_writer_close(writer);
    qeocore_type_free(type2);
    qeocore_factory_close(factory);
}

static void run_reader(void)
{
    // reader is a type 1 reader
    qeo_factory_t *factory;
    qeocore_type_t *type1;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = my_on_data_available };

    sem_init(&_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type1 = type1_register(factory)));
    assert(NULL != (reader = qeocore_reader_open(factory, type1, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    log_pid("reader initialized");
    sem_wait(&_sync); /* wait for sample */
    log_pid("reader done");
    sem_destroy(&_sync);
    qeocore_reader_close(reader);
    qeocore_type_free(type1);
    qeocore_factory_close(factory);
}

int main(int argc, const char **argv)
{
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
