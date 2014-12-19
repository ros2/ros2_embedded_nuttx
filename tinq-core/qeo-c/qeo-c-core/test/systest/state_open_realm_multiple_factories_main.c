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

static sem_t _sync, _notify_sync;

static org_qeo_system_RegistrationRequest_t *_current_regreq;
static org_qeo_system_RegistrationCredentials_t *_current_regcred;

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
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            break;
        case QEOCORE_REMOVE: {
            org_qeo_system_RegistrationRequest_t *t = (org_qeo_system_RegistrationRequest_t *)qeocore_data_get_data(data);
            /* verify struct */
            validate_regreq(_current_regreq, t, true);
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

void on_regcred_available(const qeocore_reader_t *reader,
                         const qeocore_data_t *data,
                         uintptr_t userdata)
{
    log_verbose("%s entry (%d)", __FUNCTION__, qeocore_data_get_status(data));
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            org_qeo_system_RegistrationCredentials_t *t = (org_qeo_system_RegistrationCredentials_t *)qeocore_data_get_data(data);

            /* verify struct */
            validate_regcred(_current_regcred, t, false);

            /* release main thread */
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            break;
        case QEOCORE_REMOVE: {
            org_qeo_system_RegistrationCredentials_t *t = (org_qeo_system_RegistrationCredentials_t *)qeocore_data_get_data(data);

            /* verify struct */
            validate_regcred(_current_regcred, t, true);

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
    // Not possible to register same type now !!!!
    return 0;

    unsigned int i = 0;
    qeo_factory_t *factory[2];
    qeocore_type_t *type[2];
    qeocore_reader_t *reader[2];
    qeocore_reader_t *change[2];
    qeocore_reader_listener_t listener = {.on_data = on_regreq_available };
    qeocore_writer_t *writer[2];

    /* initialize */
    log_verbose("initialization start");
    sem_init(&_sync, 0, 0);
    sem_init(&_notify_sync, 0, 0);

    for (i = 0; i < 2; i++) {
        assert(NULL != (factory[i] = qeocore_factory_new(QEO_IDENTITY_OPEN)));
        init_factory(factory[i]);
        assert(NULL != (type[i] = qeocore_type_register_tsm(factory[i], org_qeo_system_RegistrationRequest_type, org_qeo_system_RegistrationRequest_type[0].name)));
        assert(NULL != (reader[i] = qeocore_reader_open(factory[i], type[i], NULL,
                                                        QEOCORE_EFLAG_STATE_UPDATE | QEOCORE_EFLAG_ENABLE, &listener,
                                                        NULL)));
        assert(NULL != (change[i] = qeocore_reader_open(factory[i], type[i], NULL,
                                                        QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, &listener,
                                                        NULL)));
        assert(NULL != (writer[i] = qeocore_writer_open(factory[i], type[i], NULL,
                                                        QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, NULL, NULL)));
    }
    log_verbose("initialization done");

    /* send registration request */
    log_verbose("send registration request 0 on writer 0");
    _current_regreq = &_regreq[0];
    qeocore_writer_write(writer[0], _current_regreq);
    /* wait for reception (on data and change listeners) */
    for (i = 0; i < 2; i++) {
        sem_wait(&_sync);
        sem_wait(&_notify_sync);
    }
    /* iterate */
    log_verbose("count instances");
    for (i = 0; i < 2; i++) {
        assert(1 == count_instances(reader[i]));
    }

    /* remove registration request */
    log_verbose("remove registration request 0");
    _current_regreq = &_regreq[0];
    qeocore_writer_remove(writer[0], _current_regreq);
    for (i = 0; i < 2; i++) {
        sem_wait(&_sync);
    }
    /* iterate */
    log_verbose("count instances");
    for (i = 0; i < 2; i++) {
        assert(0 == count_instances(reader[i]));
    }

    /* clean up */
    log_verbose("clean up");

    for (i = 0; i < 2; i++) {
        qeocore_writer_close(writer[i]);
        qeocore_reader_close(change[i]);
        qeocore_reader_close(reader[i]);
        qeocore_type_free(type[i]);
        qeocore_factory_close(factory[i]);
    }

    sem_destroy(&_sync);
    sem_destroy(&_notify_sync);
}
