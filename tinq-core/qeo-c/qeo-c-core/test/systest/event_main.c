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

#include <qeocore/api.h>
#include "tsm_types.h"
#include "common.h"

static sem_t _sync;
static sem_t _sync_nodata;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            types_t *t = (types_t *)qeocore_data_get_data(data);
            int i;

            /* verify struct */
            assert(0 == strcmp(_types1.string, t->string));
            assert(_types1.i8 == t->i8);
            assert(_types1.i16 == t->i16);
            assert(_types1.i32 == t->i32);
            assert(_types1.i64 == t->i64);
            assert(_types1.f32 == t->f32);
            assert(_types1.boolean == t->boolean);
            assert(DDS_SEQ_LENGTH(_types1.a8) == DDS_SEQ_LENGTH(t->a8));
            DDS_SEQ_FOREACH(t->a8, i) {
                assert(DDS_SEQ_ITEM(_types1.a8, i) == DDS_SEQ_ITEM(t->a8, i));
            }
            assert(_types1.e == t->e);
            assert(DDS_SEQ_LENGTH(_types1.ae) == DDS_SEQ_LENGTH(t->ae));
            DDS_SEQ_FOREACH(t->ae, i) {
                assert(DDS_SEQ_ITEM(_types1.ae, i) == DDS_SEQ_ITEM(t->ae, i));
            }
            /* release main thread */
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            /* release main thread */
            sem_post(&_sync_nodata);
            break;
        default:
            abort();
            break;
    }
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;

    /* initialize */
    sem_init(&_sync, 0, 0);
    sem_init(&_sync_nodata, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA, &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL)));
    /* test late enabling of readers/writers */
    assert(QEO_OK == qeocore_reader_enable(reader));
    assert(QEO_OK == qeocore_writer_enable(writer));
    /* send structure */
    assert(QEO_OK == qeocore_writer_write(writer, &_types1));
    /* wait for reception */
    sem_wait(&_sync);
    sem_wait(&_sync_nodata);
    /* clean up */
    qeocore_writer_close(writer);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
    sem_destroy(&_sync);
}
