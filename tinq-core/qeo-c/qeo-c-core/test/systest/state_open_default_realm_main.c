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
#include "qdm/qeo_RegistrationRequest.h"
#include "verbose.h"
#include "common.h"

static sem_t _sync[2];
static sem_t _notify_sync[2];

static types_t *_expected = NULL;

static org_qeo_system_RegistrationRequest_t *_current_regreq;

static void on_regreq_available(const qeocore_reader_t *reader,
                         const qeocore_data_t *data,
                         uintptr_t userdata)
{
    log_verbose("%s entry (%d)", __FUNCTION__, qeocore_data_get_status(data));
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            org_qeo_system_RegistrationRequest_t *t = (org_qeo_system_RegistrationRequest_t *)qeocore_data_get_data(data);
            /* verify struct */
            validate_regreq(_current_regreq, t, false);
            /* release main thread */
            sem_post(&_sync[0]);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            break;
        case QEOCORE_REMOVE: {
            org_qeo_system_RegistrationRequest_t *t = (org_qeo_system_RegistrationRequest_t *)qeocore_data_get_data(data);
            /* verify struct */
            validate_regreq(_current_regreq, t, true);
            /* release main thread */
            sem_post(&_sync[0]);
            break;
        }
        case QEOCORE_NOTIFY:
            sem_post(&_notify_sync[0]);
            break;
        default:
            abort(); /* error */
    }
    log_verbose("%s exit", __FUNCTION__);
}

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
            assert(0 == strcmp(_expected->string, t->string));
            assert(0 == strcmp(_expected->other, t->other));
            assert(_expected->i8 == t->i8);
            assert(_expected->i16 == t->i16);
            assert(_expected->i32 == t->i32);
            assert(_expected->i64 == t->i64);
            assert(_expected->boolean == t->boolean);
            assert(_expected->f32 == t->f32);
            assert(DDS_SEQ_LENGTH(_expected->a8) == DDS_SEQ_LENGTH(t->a8));
            DDS_SEQ_FOREACH(t->a8, i) {
                assert(DDS_SEQ_ITEM(_expected->a8, i) == DDS_SEQ_ITEM(t->a8, i));
            }
            /* release main thread */
            sem_post(&_sync[1]);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            break;
        case QEOCORE_REMOVE: {
            types_t *t = (types_t *)qeocore_data_get_data(data);

            /* verify struct */
            assert(0 == strcmp(_expected->string, t->string));
            /* release main thread */
            sem_post(&_sync[1]);
            break;
        }
        case QEOCORE_NOTIFY:
            sem_post(&_notify_sync[1]);
            break;
        default:
            abort(); /* error */
    }
    log_verbose("%s exit", __FUNCTION__);
}

int main(int argc, const char **argv)
{
    unsigned int i = 0;
    qeo_factory_t *factory[2];
    qeocore_type_t *type[2];
    qeocore_reader_t *reader[2];
    qeocore_reader_t *change[2];
    qeocore_reader_listener_t listener[2] = { {.on_data = on_regreq_available}, {.on_data = on_data_available} };
    qeocore_writer_t *writer[2];

    /* initialize */
    log_verbose("initialization start");
    for (i = 0; i < 2; i++) {
        sem_init(&_sync[i], 0, 0);
        sem_init(&_notify_sync[i], 0, 0);
    }

    log_verbose("initialization for default realm");
    assert(NULL != (factory[1] = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory[1]);
    _tsm_types[0].flags |= TSMFLAG_KEY;
    _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    assert(NULL != (type[1] = qeocore_type_register_tsm(factory[1], _tsm_types, _tsm_types[1].name)));
    assert(NULL != (reader[1] = qeocore_reader_open(factory[1], type[1], NULL,
                                                    QEOCORE_EFLAG_STATE_UPDATE | QEOCORE_EFLAG_ENABLE, &listener[1],
                                                    NULL)));
    assert(NULL != (change[1] = qeocore_reader_open(factory[1], type[1], NULL,
                                                    QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, &listener[1],
                                                    NULL)));
    assert(NULL != (writer[1] = qeocore_writer_open(factory[1], type[1], NULL,
                                                    QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, NULL, NULL)));

    log_verbose("initialization for open realm");
    assert(NULL != (factory[0] = qeocore_factory_new(QEO_IDENTITY_OPEN)));
    init_factory(factory[0]);
    assert(NULL != (type[0] = qeocore_type_register_tsm(factory[0], org_qeo_system_RegistrationRequest_type, org_qeo_system_RegistrationRequest_type[0].name)));
    assert(NULL != (reader[0] = qeocore_reader_open(factory[0], type[0], NULL,
                                                    QEOCORE_EFLAG_STATE_UPDATE | QEOCORE_EFLAG_ENABLE, &listener[0],
                                                    NULL)));
    assert(NULL != (change[0] = qeocore_reader_open(factory[0], type[0], NULL,
                                                    QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, &listener[0],
                                                    NULL)));
    assert(NULL != (writer[0] = qeocore_writer_open(factory[0], type[0], NULL,
                                                    QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, NULL, NULL)));


    log_verbose("initialization done");

    log_verbose("send registration request on open realm");
    _current_regreq = &_regreq[0];
    qeocore_writer_write(writer[0], _current_regreq);
    /* wait for reception (on data and change listeners) */
    sem_wait(&_sync[0]);
    sem_wait(&_notify_sync[0]);
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader[0]));

    log_verbose("publish something on default realm");
    _expected = &_types1;
    qeocore_writer_write(writer[1], _expected);
    sem_wait(&_sync[1]); /* wait for reception */
    sem_wait(&_notify_sync[1]);
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader[1]));

    log_verbose("remove instance on default realm");
    qeocore_writer_remove(writer[1], _expected);
    sem_wait(&_sync[1]); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(0 == count_instances(reader[1]));

    log_verbose("remove instance on open realm");
    qeocore_writer_remove(writer[0], _current_regreq);
    sem_wait(&_sync[0]); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(0 == count_instances(reader[0]));

    /* clean up */
    log_verbose("clean up");

    sleep(5);

    for (i = 0; i < 2; i++) {
        qeocore_writer_close(writer[i]);
        qeocore_reader_close(change[i]);
        qeocore_reader_close(reader[i]);
        qeocore_type_free(type[i]);
        qeocore_factory_close(factory[i]);
        sem_destroy(&_sync[i]);
        sem_destroy(&_notify_sync[i]);
    }
}
