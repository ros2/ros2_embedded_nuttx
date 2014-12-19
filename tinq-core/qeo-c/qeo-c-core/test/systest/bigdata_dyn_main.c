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

#define TEST_SIZE 100*1024
static int _test_size = TEST_SIZE;

DDS_SEQUENCE(uint8_t, byte_array_t);

static qeocore_member_id_t _size_id = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _buf_id = QEOCORE_MEMBER_ID_DEFAULT;

static void my_on_data_available(const qeocore_reader_t *reader,
                                 const qeocore_data_t *data,
                                 uintptr_t userdata)
{
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            qeocore_data_t *seqdata = NULL;
            byte_array_t array;
            int i, size;

            log_pid("reader received data");
            assert(QEO_OK == qeocore_data_get_member(data, _size_id, &size));
            assert(QEO_OK == qeocore_data_get_member(data, _buf_id, &seqdata));
            assert(QEO_OK == qeocore_data_sequence_get(seqdata, (qeo_sequence_t *)&array, 0, QEOCORE_SIZE_UNLIMITED));
            assert(size == DDS_SEQ_LENGTH(array));
            assert(_test_size == DDS_SEQ_LENGTH(array));
            for (i = 0; i < size; i++) {
                assert(DDS_SEQ_ITEM(array, i) == (i & 0xff));
            }
            qeocore_data_sequence_free(seqdata, (qeo_sequence_t *)&array);
            qeocore_data_free(seqdata);
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

static qeocore_type_t *type_register(qeo_factory_t *factory)
{
    qeocore_type_t *bigdata = NULL;
    qeocore_type_t *primitive = NULL;
    qeocore_type_t *sequence = NULL;

    assert(NULL != (bigdata = qeocore_type_struct_new("org.qeo.test.bigdata")));
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(bigdata, primitive, "size", &_size_id, QEOCORE_FLAG_KEY));
    qeocore_type_free(primitive);
    assert(NULL != (primitive = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
    assert(NULL != (sequence = qeocore_type_sequence_new(primitive)));
    assert(QEO_OK == qeocore_type_struct_add(bigdata, sequence, "buf", &_buf_id, QEOCORE_FLAG_NONE));
    qeocore_type_free(sequence);
    qeocore_type_free(primitive);
    assert(QEO_OK == qeocore_type_register(factory, bigdata, "org.qeo.test.bigdata"));
    return bigdata;
}

static void run_writer(pid_t peer)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_writer_t *writer;
    qeocore_data_t *data, *seqdata;
    byte_array_t array;
    int status, i;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = type_register(factory)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    log_pid("writer initialized");
    assert(NULL != (data = qeocore_writer_data_new(writer)));
    assert(QEO_OK == qeocore_data_set_member(data, _size_id, &_test_size));
    /* init sequence */
    DDS_SEQ_INIT(array);
    assert(NULL != (DDS_SEQ_DATA(array) = malloc(_test_size * sizeof(char))));
    DDS_SEQ_LENGTH(array) = DDS_SEQ_MAXIMUM(array) = _test_size;
    for (i = 0; i < _test_size; i++) {
        DDS_SEQ_ITEM(array, i) = i & 0xff;
    }
    assert(QEO_OK == qeocore_data_get_member(data, _buf_id, &seqdata));
    assert(QEO_OK == qeocore_data_sequence_set(seqdata, (const qeo_sequence_t *)&array, 0));
    assert(QEO_OK == qeocore_data_set_member(data, _buf_id, &seqdata));
    /* write */
    assert(QEO_OK == qeocore_writer_write(writer, data));
    log_pid("writer wrote data");
    assert(peer == waitpid(peer, &status, 0));
    assert(0 == status);
    log_pid("writer done");
    /* clean up */
    free(DDS_SEQ_DATA(array));
    qeocore_data_free(seqdata);
    qeocore_data_free(data);
    qeocore_writer_close(writer);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
}

static void run_reader(void)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = my_on_data_available };

    sem_init(&_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = type_register(factory)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    log_pid("reader initialized");
    sem_wait(&_sync); /* wait for sample */
    log_pid("reader done");
    sem_destroy(&_sync);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
}

int main(int argc, const char **argv)
{
    pid_t pidreader;

    if (argc == 2) {
        _test_size = atoi(argv[1]);
    }
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
