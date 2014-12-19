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

#include <qeo/api.h>
#include "common.h"
#include "tsm_types.h"
#include "verbose.h"

static sem_t _sync;
static sem_t _notify_sync;

static types_t *_current = NULL;

static void on_data(const qeo_state_change_reader_t *reader,
                    const void *data,
                    uintptr_t userdata)
{
    types_t *t = (types_t *)data;
    int i;

    log_verbose("%s entry", __FUNCTION__);
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
    /* release main thread */
    sem_post(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_no_more_data(const qeo_state_change_reader_t *reader,
                            uintptr_t userdata)
{
    /* nop */
}

static void on_remove(const qeo_state_change_reader_t *reader,
                      const void *data,
                      uintptr_t userdata)
{
    types_t *t = (types_t *)data;

    log_verbose("%s entry", __FUNCTION__);
    /* verify struct */
    assert(0 == strcmp(_current->string, t->string));
    /* release main thread */
    sem_post(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_update(const qeo_state_reader_t *reader,
                      uintptr_t userdata)
{
    log_verbose("%s entry", __FUNCTION__);
    sem_post(&_notify_sync);
    log_verbose("%s exit", __FUNCTION__);
}

static qeo_iterate_action_t it_abort_callback(const void *data,
                                                     uintptr_t userdata)
{
    int *cnt = (int *)userdata;

    (*cnt)++;
    return QEO_ITERATE_ABORT;
}

static void iterate_with_abort(const qeo_state_reader_t *reader)
{
    int cnt = 0;
    assert(QEO_OK == qeo_state_reader_foreach(reader, it_abort_callback, (uintptr_t)&cnt));
    assert(cnt == 1); /* should always be 1 as aborted after first sample */
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeo_state_reader_t *reader;
    qeo_state_reader_t *readernl; /* NULL-listener */
    qeo_state_change_reader_t *change;
    qeo_state_writer_t *writer;
    qeo_state_reader_listener_t sr_cbs = {
        .on_update = on_update
    };
    qeo_state_change_reader_listener_t scr_cbs = {
        .on_data = on_data,
        .on_no_more_data = on_no_more_data,
        .on_remove = on_remove
    };


    /* initialize */
    log_verbose("initialization start");
    sem_init(&_sync, 0, 0);
    sem_init(&_notify_sync, 0, 0);
    assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_DEFAULT)));
    _tsm_types[0].flags |= TSMFLAG_KEY;
    _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    assert(NULL != (reader = qeo_factory_create_state_reader(factory, _tsm_types, &sr_cbs, 0)));
    assert(NULL != (readernl = qeo_factory_create_state_reader(factory, _tsm_types, NULL, 0)));
    assert(NULL != (change = qeo_factory_create_state_change_reader(factory, _tsm_types, &scr_cbs, 0)));
    assert(NULL != (writer = qeo_factory_create_state_writer(factory, _tsm_types, NULL, 0)));
    log_verbose("initialization done");
    /* send structure for instance 1 */
    log_verbose("send instance 1");
    _current = &_types1;
    assert(0 == count_instances(readernl));
    qeo_state_writer_write(writer, _current);
    /* wait for reception (on data and change listeners) */
    sem_wait(&_sync);
    sem_wait(&_notify_sync);
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader));
    /* send structure for instance 2 */
    log_verbose("send instance 2");
    _current = &_types2;
    qeo_state_writer_write(writer, _current);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(2 == count_instances(reader));
    /* start iteration but abort */
    log_verbose("iterate with abort");
    iterate_with_abort(reader);
    /* check iterate again to see it's still intact*/
    log_verbose("count instances");
    assert(2 == count_instances(reader));

    /* remove instance 1 */
    log_verbose("remove instance 1");
    _current = &_types1;
    qeo_state_writer_remove(writer, _current);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader));
    /* remove instance 2 */
    log_verbose("remove instance 2");
    _current = &_types2;
    qeo_state_writer_remove(writer, _current);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(0 == count_instances(reader));
    /* clean up */
    log_verbose("clean up");
    qeo_state_writer_close(writer);
    qeo_state_change_reader_close(change);
    qeo_state_reader_close(reader);
    qeo_state_reader_close(readernl);
    qeo_factory_close(factory);
    sem_destroy(&_sync);
    return 0;
}
