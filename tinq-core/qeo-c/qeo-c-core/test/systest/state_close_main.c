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
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <qeocore/api.h>

#include "common.h"

static int _done = 0;
static int _closed = 0;
static sem_t _sync;

typedef struct {
    int32_t cnt;
} type_t;

const DDS_TypeSupport_meta _tsm[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.CloseReader",
     .flags = TSMFLAG_MUTABLE|TSMFLAG_KEY, .size = sizeof(type_t), .nelem = 1 },
   { .tc = CDR_TYPECODE_LONG, .name = "cnt", .label = 12345,
     .flags = TSMFLAG_KEY, .offset = offsetof(type_t, cnt) },
};

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            struct timespec ts = { 0, 100*1000*1000 /* 100ms*/ };

            assert(!_closed);
            nanosleep(&ts, NULL);
            assert(!_closed);
            /* release main thread */
            sem_post(&_sync);
            break;
        }
        default:
            /*nop*/
            break;
    }
}

static void *run(void *data)
{
    qeocore_writer_t *writer = (qeocore_writer_t *)data;
    struct timespec ts = { 0, 1*1000 /* 1us*/ };
    type_t t = {0};

    while (!_done) {
        qeocore_writer_write(writer, &t);
        t.cnt++;
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;
    pthread_t th = 0;


    /* initialize */
    sem_init(&_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm, _tsm[0].name)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    /* start writer thread */
    assert(0 == pthread_create(&th, NULL, run, writer));
    /* make sure writing has started */
    sem_wait(&_sync);
    /* shutdown reader */
    qeocore_reader_close(reader);
    _closed = 1;
    /* shutdown writer */
    _done = 1;
    pthread_join(th, NULL);
    qeocore_writer_close(writer);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
}
