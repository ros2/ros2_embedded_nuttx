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

#include <qeo/api.h>
#include "tsm_types.h"

static sem_t _sync;
static sem_t _sync_nodata;

static void on_data(const qeo_event_reader_t *reader,
                    const void *data,
                    uintptr_t userdata)
{
    types_t *t = (types_t *)data;
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
    /* release main thread */
    sem_post(&_sync);
}

static void on_no_more_data(const qeo_event_reader_t *reader,
                            uintptr_t userdata)
{
    /* release main thread */
    sem_post(&_sync_nodata);
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeo_event_reader_t *reader;
    qeo_event_writer_t *writer;
    qeo_event_reader_listener_t cbs = {
        .on_data = on_data,
        .on_no_more_data = on_no_more_data
    };


    /* initialize */
    sem_init(&_sync, 0, 0);
    sem_init(&_sync_nodata, 0, 0);
    assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_DEFAULT)));
    /* check that only 1 factory can be created */
    assert(NULL == qeo_factory_create(QEO_IDENTITY_DEFAULT));
    assert(NULL != (reader = qeo_factory_create_event_reader(factory, _tsm_types, &cbs, 0)));
    assert(NULL != (writer = qeo_factory_create_event_writer(factory, _tsm_types, NULL, 0)));
    /* send structure */
    qeo_event_writer_write(writer, &_types1);
    /* wait for reception */
    sem_wait(&_sync);
    sem_wait(&_sync_nodata);
    /* clean up */
    qeo_event_writer_close(writer);
    qeo_event_reader_close(reader);
    qeo_factory_close(factory);
    sem_destroy(&_sync);
    return 0;
}
