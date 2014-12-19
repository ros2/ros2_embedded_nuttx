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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qeocore/api.h>
#include "tsm_types.h"
#include "verbose.h"
#include "common.h"

static sem_t _sync;
static sem_t _notify_sync;

static types_t *_current = NULL;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    log_verbose("%s entry (%d)", __FUNCTION__, qeocore_data_get_status(data));
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            types_t *t = (types_t *)qeocore_data_get_data(data);
            int i;

            /* verify struct */
            assert(0 == strcmp(_current->string, t->string));
            assert(0 == strcmp(_current->other, t->other));
            assert(_current->i8 == t->i8);
            assert(_current->i16 == t->i16);
            assert(_current->i32 == t->i32);
            assert(_current->i64 == t->i64);
            assert(_current->boolean == t->boolean);
            assert(_current->f32 == t->f32);
            assert(DDS_SEQ_LENGTH(_current->a8) == DDS_SEQ_LENGTH(t->a8));
            DDS_SEQ_FOREACH(t->a8, i) {
                assert(DDS_SEQ_ITEM(_current->a8, i) == DDS_SEQ_ITEM(t->a8, i));
            }
            assert(_current->e == t->e);
            assert(DDS_SEQ_LENGTH(_current->ae) == DDS_SEQ_LENGTH(t->ae));
            DDS_SEQ_FOREACH(t->ae, i) {
                assert(DDS_SEQ_ITEM(_current->ae, i) == DDS_SEQ_ITEM(t->ae, i));
            }
            /* release main thread */
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            break;
        case QEOCORE_REMOVE: {
            types_t *t = (types_t *)qeocore_data_get_data(data);

            /* verify struct */
            assert(0 == strcmp(_current->string, t->string));
            /* release main thread */
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NOTIFY:
            sem_post(&_notify_sync);
            break;
        default:
            abort(); /* error */
    }
    log_verbose("%s exit", __FUNCTION__);
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_reader_t *reader;
    qeocore_reader_t *change;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;

    /* initialize */
    log_verbose("initialization start");
    sem_init(&_sync, 0, 0);
    sem_init(&_notify_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    _tsm_types[0].flags |= TSMFLAG_KEY;
    _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_STATE_UPDATE | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    assert(NULL != (change = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    log_verbose("initialization done");
    /* send structure for instance 1 */
    log_verbose("send instance 1");
    _current = &_types1;
    qeocore_writer_write(writer, _current);
    /* wait for reception (on data and change listeners) */
    sem_wait(&_sync);
    sem_wait(&_notify_sync);
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader));
    /* send structure for instance 2 */
    log_verbose("send instance 2");
    _current = &_types2;
    qeocore_writer_write(writer, _current);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(2 == count_instances(reader));
    /* remove instance 1 */
    log_verbose("remove instance 1");
    _current = &_types1;
    qeocore_writer_remove(writer, _current);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader));
    /* remove instance 2 */
    log_verbose("remove instance 2");
    _current = &_types2;
    qeocore_writer_remove(writer, _current);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(0 == count_instances(reader));
    /* clean up */
    log_verbose("clean up");
    qeocore_writer_close(writer);
    qeocore_reader_close(change);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
    sem_destroy(&_sync);
}
