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
static int _check_id = 0;
static int _check_inner = 0;
static int _check_num = 0;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    qeocore_data_t *inner = NULL;
    int32_t i = 0;

    if (QEOCORE_NO_MORE_DATA == qeocore_data_get_status(data))
        return;

    log_verbose("Expect: %d, got: %d", expected_status, qeocore_data_get_status(data));
    assert(expected_status == qeocore_data_get_status(data));
    /* check outer struct */
    if ((expected_status == QEOCORE_DATA) || _check_id) {
        assert(QEO_OK == qeocore_data_get_member(data, _id_id, &i));
        assert(123 == i);
    }
    /* check inner struct */
    if ((expected_status == QEOCORE_DATA) || _check_inner) {
        assert(QEO_OK == qeocore_data_get_member(data, _inner_id, &inner));
        assert(QEO_OK == qeocore_data_get_member(inner, _num_id, &i));
        assert(456 == i);
        qeocore_data_free(inner);
    }
    /* release main thread */
    sem_post(&_sync);
}

static void fill_data(qeocore_data_t *data)
{
    qeocore_data_t *inner = NULL;
    int32_t i;

    /* fill outer struct */
    i = 123;
    assert(QEO_OK == qeocore_data_set_member(data, _id_id, &i));
    /* fill inner struct */
    assert(QEO_OK == qeocore_data_get_member(data, _inner_id, &inner));
    i = 456;
    assert(QEO_OK == qeocore_data_set_member(inner, _num_id, &i));
    assert(QEO_OK == qeocore_data_set_member(data, _inner_id, &inner));
    qeocore_data_free(inner);
}

static void test(int id_is_keyed, int inner_is_keyed, int num_is_keyed)
{
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;
    qeocore_type_t *type;
    qeocore_data_t *data;
    int flags = QEOCORE_EFLAG_ENABLE;

    flags |= ((id_is_keyed || inner_is_keyed) ? QEOCORE_EFLAG_STATE_DATA : QEOCORE_EFLAG_EVENT_DATA);

    _check_id = id_is_keyed;
    _check_inner = inner_is_keyed;
    _check_num = num_is_keyed;
    log_verbose("Running test %d-%d-%d", id_is_keyed, inner_is_keyed, num_is_keyed);
    assert(NULL != (type = nested_type_get(id_is_keyed, inner_is_keyed, num_is_keyed)));
    assert(QEO_OK == qeocore_type_register(_factory, type, "nested"));
    assert(NULL != (reader = qeocore_reader_open(_factory, type, NULL, flags, &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(_factory, type, NULL, flags, NULL, NULL)));
    /* send data */
    assert(NULL != (data = qeocore_writer_data_new(writer)));
    fill_data(data);
    log_verbose("  write");
    assert(QEO_OK == qeocore_writer_write(writer, data));
    expected_status = QEOCORE_DATA;
    /* wait for reception */
    sem_wait(&_sync);
    if (id_is_keyed || inner_is_keyed) {
        log_verbose("  remove");
        assert(QEO_OK == qeocore_writer_remove(writer, data));
        expected_status = QEOCORE_REMOVE;
        /* wait for reception */
        sem_wait(&_sync);
    }
    /* clean up */
    qeocore_data_free(data);
    qeocore_writer_close(writer);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
}


int main(int argc, const char **argv)
{

    /* initialize */
    sem_init(&_sync, 0, 0);
    assert(NULL != (_factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(_factory);

    /* event test (nothing keyed) */
    test(0, 0, 0);
    /* state test (keyed member in inner struct) */
    test(0, 0, 1); // equivalent to unkeyed -> remove not possible
    /* state test (keyed inner struct) */
    test(0, 1, 0); // equivalent to 011
    /* state test (keyed inner struct and member in inner struct) */
    test(0, 1, 1);
    /* state test (keyed member in outer struct) */
    test(1, 0, 0);
    /* state test (keyed member in inner and outer struct) */
    test(1, 0, 1); // equivalent to 100
    /* state test (keyed member in outer struct and keyed inner struct) */
    test(1, 1, 0); // equivalent to 111
    /* state test (keyed all over) */
    test(1, 1, 1);

    qeocore_factory_close(_factory);
    sem_destroy(&_sync);
}
