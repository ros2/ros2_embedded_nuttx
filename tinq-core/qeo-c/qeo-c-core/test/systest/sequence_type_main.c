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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qeocore/api.h>

#include "dyn_types.h"
#include "verbose.h"
#include "common.h"

static sem_t _sync;

static qeo_factory_t *_factory;

static qeocore_data_status_t expected_status = QEOCORE_NO_MORE_DATA;
static qeocore_typecode_t _tc = -1;
static unsigned int _nelem = 0;

static char *_str_data[] = {
    "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"
};

/* Internally DDS has room for approx. 12 items, for more the buffer has to grow. */
static int _arr_sz[] = { 0, 1, 5, 20 };
#define NUM_SZ (sizeof(_arr_sz)/sizeof(int))

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    qeocore_data_t *sequence_data = NULL;
    qeo_sequence_t sequence;
    qeocore_data_status_t status;
    int i, nelem;

    status = qeocore_data_get_status(data);
    log_verbose("  status = %d", status);
    if (QEOCORE_NO_MORE_DATA == status) {
        return;
    }
    assert(expected_status == status);
    /* check sequence */

    assert(QEO_OK == qeocore_data_get_member(data, _sequence_id, &sequence_data));
    assert(QEO_OK == qeocore_data_sequence_get(sequence_data, &sequence, 0, QEOCORE_SIZE_UNLIMITED));
    nelem = DDS_SEQ_LENGTH(sequence);
    assert(_nelem == nelem);
    log_verbose("  size = %d", nelem);
    DDS_SEQ_FOREACH(sequence, i) {
        switch (_tc) {
            case QEOCORE_TYPECODE_BOOLEAN: assert((i % 2) == ((qeo_boolean_t *)DDS_SEQ_DATA(sequence))[i]); break;
            case QEOCORE_TYPECODE_INT8: assert((i + 1) == ((int8_t *)DDS_SEQ_DATA(sequence))[i]); break;
            case QEOCORE_TYPECODE_INT16: assert((i + 1) == ((int16_t *)DDS_SEQ_DATA(sequence))[i]); break;
            case QEOCORE_TYPECODE_INT32: assert((i + 1) == ((int32_t *)DDS_SEQ_DATA(sequence))[i]); break;
            case QEOCORE_TYPECODE_INT64: assert((i + 1) == ((int64_t *)DDS_SEQ_DATA(sequence))[i]); break;
            case QEOCORE_TYPECODE_FLOAT32: assert((i + 1) == ((float *)DDS_SEQ_DATA(sequence))[i]); break;
            case QEOCORE_TYPECODE_STRING: {
                char *elem = ((char **)DDS_SEQ_DATA(sequence))[i];

                assert(0 == strcmp(_str_data[i % 10], elem));
                break;
            }
            case QEOCORE_TYPECODE_SEQUENCE: {
                qeocore_data_t *elem = ((qeocore_data_t **)DDS_SEQ_DATA(sequence))[i];
                qeo_sequence_t inner;
                unsigned int j;

                assert(QEO_OK == qeocore_data_sequence_get(elem, &inner, 0, QEOCORE_SIZE_UNLIMITED));
                assert(_nelem == DDS_SEQ_LENGTH(sequence));
                for (j = 0; j < DDS_SEQ_LENGTH(inner); j++) {
                    assert(((i * _nelem) + j + 1) == ((int32_t *)DDS_SEQ_DATA(inner))[j]);
                }
                qeocore_data_sequence_free(elem, &inner);
                break;
            }
            case QEOCORE_TYPECODE_STRUCT: {
                qeocore_data_t *elem = ((qeocore_data_t **)DDS_SEQ_DATA(sequence))[i];
                int32_t num;

                assert(QEO_OK == qeocore_data_get_member(elem, _num_id, &num));
                assert((i + 1) == num);
                break;
            }
            default:
                abort();
        }
    }
    qeocore_data_sequence_free(sequence_data, &sequence);
    qeocore_data_free(sequence_data);
    log_verbose("  reception done");
    /* release main thread */
    sem_post(&_sync);
}

static void fill_sequence(qeo_sequence_t *sequence,
                          qeocore_typecode_t tc,
                          int start)
{
    int i;

    DDS_SEQ_FOREACH(*sequence, i) {
        switch (tc) {
            case QEOCORE_TYPECODE_BOOLEAN: ((qeo_boolean_t *)DDS_SEQ_DATA(*sequence))[i] = (qeo_boolean_t)(i % 2); break;
            case QEOCORE_TYPECODE_INT8: ((int8_t *)DDS_SEQ_DATA(*sequence))[i] = (int8_t)(i + 1); break;
            case QEOCORE_TYPECODE_INT16: ((int16_t *)DDS_SEQ_DATA(*sequence))[i] = (int16_t)(i + 1); break;
            case QEOCORE_TYPECODE_INT32: ((int32_t *)DDS_SEQ_DATA(*sequence))[i] = (int32_t)(start + i + 1); break;
            case QEOCORE_TYPECODE_INT64: ((int64_t *)DDS_SEQ_DATA(*sequence))[i] = (int64_t)(i + 1); break;
            case QEOCORE_TYPECODE_FLOAT32: ((float *)DDS_SEQ_DATA(*sequence))[i] = (float)(i + 1); break;
            case QEOCORE_TYPECODE_STRING: {
                char **elem = &((char **)DDS_SEQ_DATA(*sequence))[i];

                /* we use strdup here for easy freeing of the complete sequence afterwards */
                *elem = strdup(_str_data[i % 10]);
                assert(NULL != *elem);
                break;
            }
            case QEOCORE_TYPECODE_SEQUENCE: {
                qeocore_data_t *data = ((qeocore_data_t **)DDS_SEQ_DATA(*sequence))[i];
                qeo_sequence_t inner;

                /* allocate buffer of _nelem elements for inner sequence */
                assert(QEO_OK == qeocore_data_sequence_new(data, &inner, _nelem));
                fill_sequence(&inner, QEOCORE_TYPECODE_INT32, i * _nelem);
                assert(QEO_OK == qeocore_data_sequence_set(data, &inner, 0));
                qeocore_data_sequence_free(data, &inner);
                break;
            }
            case QEOCORE_TYPECODE_STRUCT: {
                qeocore_data_t *elem = ((qeocore_data_t **)DDS_SEQ_DATA(*sequence))[i];
                int32_t num = i + 1;

                assert(QEO_OK == qeocore_data_set_member(elem, _num_id, &num));
                break;
            }
            default:
                abort();
        }
    }
}

static void fill_data(qeocore_data_t *data)
{
    qeocore_data_t *sequence_data = NULL;
    qeo_sequence_t sequence;

    /* first fetch a data representation of the sequence */
    assert(QEO_OK == qeocore_data_get_member(data, _sequence_id, &sequence_data));
    /* now allocate a buffer of _nelem elements */
    assert(QEO_OK == qeocore_data_sequence_new(sequence_data, &sequence, _nelem));
    /* populate buffer */
    fill_sequence(&sequence, _tc, 0);
    /* set buffer in data representation of the sequence */
    assert(QEO_OK == qeocore_data_sequence_set(sequence_data, &sequence, 0));
    /* and set the populated sequence in the struct */
    assert(QEO_OK == qeocore_data_set_member(data, _sequence_id, &sequence_data));
    /* clean up */
    qeocore_data_sequence_free(sequence_data, &sequence);
    qeocore_data_free(sequence_data);
}

static void test_1(int flags,
                   qeocore_typecode_t tc,
                   int reg_struct)
{
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;
    qeocore_type_t *type;
    qeocore_data_t *data;
    int i;

    _tc = tc;
    assert(NULL != (type = seq_type_get(_factory, _tc, (flags & QEOCORE_EFLAG_STATE ? 1 : 0) /*keyed?*/, reg_struct)));
    assert(QEO_OK == qeocore_type_register(_factory, type, "sequence"));
    flags |= QEOCORE_EFLAG_ENABLE;
    assert(NULL != (reader = qeocore_reader_open(_factory, type, NULL, flags, &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(_factory, type, NULL, flags, NULL, NULL)));
    /* send data */
    for (i = 0; i < NUM_SZ; i++) {
        _nelem = _arr_sz[i];
        log_verbose("%s test for sequence of %s with %d elements",
                    (flags & QEOCORE_EFLAG_STATE ? "State" : "Event"),
                    _TC2STR[_tc], _nelem);
        assert(NULL != (data = qeocore_writer_data_new(writer)));
        fill_data(data);
        log_verbose("  write");
        expected_status = QEOCORE_DATA;
        assert(QEO_OK == qeocore_writer_write(writer, data));
        /* wait for reception */
        sem_wait(&_sync);
        if (flags & QEOCORE_EFLAG_STATE) {
            log_verbose("  remove");
            expected_status = QEOCORE_REMOVE;
            assert(QEO_OK == qeocore_writer_remove(writer, data));
            /* wait for reception */
            sem_wait(&_sync);
        }
        qeocore_data_free(data);
    }
    /* clean up */
    qeocore_writer_close(writer);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
}

static void test_all(int flags)
{
    test_1(flags, QEOCORE_TYPECODE_BOOLEAN, 0);
    test_1(flags, QEOCORE_TYPECODE_INT8, 0);
    test_1(flags, QEOCORE_TYPECODE_INT16, 0);
    test_1(flags, QEOCORE_TYPECODE_INT32, 0);
    test_1(flags, QEOCORE_TYPECODE_INT64, 0);
    test_1(flags, QEOCORE_TYPECODE_FLOAT32, 0);
    test_1(flags, QEOCORE_TYPECODE_STRING, 0);
    test_1(flags, QEOCORE_TYPECODE_SEQUENCE, 0);
    test_1(flags, QEOCORE_TYPECODE_STRUCT, 0);
    test_1(flags, QEOCORE_TYPECODE_STRUCT, 1);
}

int main(int argc, const char **argv)
{
    /* initialize */
    sem_init(&_sync, 0, 0);
    assert(NULL != (_factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(_factory);

    /* event test */
    test_all(QEOCORE_EFLAG_EVENT_DATA);
    /* state test */
    test_all(QEOCORE_EFLAG_STATE_DATA);

    qeocore_factory_close(_factory);
    sem_destroy(&_sync);
}
