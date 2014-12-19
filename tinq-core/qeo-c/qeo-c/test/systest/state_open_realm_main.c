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
#include <stdbool.h>

#include <qeo/api.h>
#include "common.h"
#include "tsm_types.h"
#include "qdm/qeo_RegistrationRequest.h"
#include "qdm/qeo_RegistrationCredentials.h"
#include "verbose.h"

static sem_t _sync;
static sem_t _notify_sync;

static org_qeo_system_RegistrationRequest_t *_current_regreq;
static org_qeo_system_RegistrationCredentials_t *_current_regcred;

static void on_data_regreq(const qeo_state_change_reader_t *reader,
                           const void *data,
                           uintptr_t userdata)
{
    log_verbose("%s entry", __FUNCTION__);
    org_qeo_system_RegistrationRequest_t *t = (org_qeo_system_RegistrationRequest_t *)data;
    validate_regreq(_current_regreq, t, false);
    /* release main thread */
    sem_post(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_data_regcred(const qeo_state_change_reader_t *reader,
                           const void *data,
                           uintptr_t userdata)
{
    log_verbose("%s entry", __FUNCTION__);
    org_qeo_system_RegistrationCredentials_t *t = (org_qeo_system_RegistrationCredentials_t *)data;
    validate_regcred(_current_regcred, t, false);
    /* release main thread */
    sem_post(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_no_more_data(const qeo_state_change_reader_t *reader,
                            uintptr_t userdata)
{
    /* nop */
}

static void on_remove_regreq(const qeo_state_change_reader_t *reader,
                             const void *data,
                             uintptr_t userdata)
{
    log_verbose("%s entry", __FUNCTION__);
    org_qeo_system_RegistrationRequest_t *t = (org_qeo_system_RegistrationRequest_t *)data;
    /* verify struct */
    validate_regreq(_current_regreq, t, true);
    /* release main thread */
    sem_post(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_remove_regcred(const qeo_state_change_reader_t *reader,
                              const void *data,
                              uintptr_t userdata)
{
    log_verbose("%s entry", __FUNCTION__);
    org_qeo_system_RegistrationCredentials_t *t = (org_qeo_system_RegistrationCredentials_t *)data;
    validate_regcred(_current_regcred, t, true);
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

int main(int argc, const char **argv)
{
    unsigned int i = 0;
    qeo_factory_t *factory;
    qeo_state_reader_t *reader[2];
    qeo_state_change_reader_t *change[2];
    qeo_state_writer_t *writer[2];
    qeo_state_reader_listener_t sr_cbs = {
        .on_update = on_update
    };
    qeo_state_change_reader_listener_t scr_cbs[2] = {
        {
            .on_data = on_data_regreq,
            .on_no_more_data = on_no_more_data,
            .on_remove = on_remove_regreq
        },
        {
            .on_data = on_data_regcred,
            .on_no_more_data = on_no_more_data,
            .on_remove = on_remove_regcred
        }
    };

    /* initialize */
    log_verbose("initialization start");
    sem_init(&_sync, 0, 0);
    sem_init(&_notify_sync, 0, 0);
    assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_OPEN)));
    assert(NULL != (reader[0] = qeo_factory_create_state_reader(factory, org_qeo_system_RegistrationRequest_type, &sr_cbs, 0)));
    assert(NULL != (change[0] = qeo_factory_create_state_change_reader(factory, org_qeo_system_RegistrationRequest_type, &scr_cbs[0], 0)));
    assert(NULL != (writer[0] = qeo_factory_create_state_writer(factory, org_qeo_system_RegistrationRequest_type, NULL, 0)));
    assert(NULL != (reader[1] = qeo_factory_create_state_reader(factory, org_qeo_system_RegistrationCredentials_type, &sr_cbs, 0)));
    assert(NULL != (change[1] = qeo_factory_create_state_change_reader(factory, org_qeo_system_RegistrationCredentials_type, &scr_cbs[1], 0)));
    assert(NULL != (writer[1] = qeo_factory_create_state_writer(factory, org_qeo_system_RegistrationCredentials_type, NULL, 0)));
    log_verbose("initialization done");

    log_verbose("send registration request");
    _current_regreq = &_regreq[0];
    qeo_state_writer_write(writer[0], _current_regreq);
    /* wait for reception (on data and change listeners) */
    sem_wait(&_sync);
    sem_wait(&_notify_sync);
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader[0]));

    log_verbose("send registration credentials");
    _current_regcred = &_regcred[0];
    qeo_state_writer_write(writer[1], _current_regcred);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(1 == count_instances(reader[1]));


    log_verbose("remove registration request");
    qeo_state_writer_remove(writer[0], _current_regreq);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(0 == count_instances(reader[0]));

    log_verbose("remove registration credentials");
    qeo_state_writer_remove(writer[1], _current_regcred);
    sem_wait(&_sync); /* wait for reception */
    /* iterate */
    log_verbose("count instances");
    assert(0 == count_instances(reader[1]));

    log_verbose("clean up");
    for (i = 0; i < 2; i++) {
        qeo_state_writer_close(writer[i]);
        qeo_state_change_reader_close(change[i]);
        qeo_state_reader_close(reader[i]);
    }
    qeo_factory_close(factory);
    sem_destroy(&_sync);
    sem_destroy(&_notify_sync);
    return 0;
}
