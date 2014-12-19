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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <qeocore/api.h>

#include "tsm_types.h"
#include "common.h"
#include "verbose.h"

static sem_t _sync;
static int _rc = 1;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    static int data_cnt = 0;
    static int remove_cnt = 0;

    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA:
            data_cnt++;
            log_verbose("data count = %d", data_cnt);
            if (2 == data_cnt) {
                /* release main thread */
                sem_post(&_sync);
            }
            break;
        case QEOCORE_REMOVE:
            remove_cnt++;
            log_verbose("removal count = %d", remove_cnt);
            if (2 == remove_cnt) {
                _rc = 0;
                /* release main thread */
                sem_post(&_sync);
            }
            break;
        default:
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
    struct timespec ts = {0};
    struct timeval tv = {0};

    /* initialize */
    sem_init(&_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    _tsm_types[0].flags |= TSMFLAG_KEY;
    _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    /* write instances */
    log_verbose("writing instances");
    qeocore_writer_write(writer, &_types1);
    qeocore_writer_write(writer, &_types2);
    /* wait for reception of all instances */
    sem_wait(&_sync);
    /* shutdown writer without removing instances */
    log_verbose("closing writer");
    qeocore_writer_close(writer);
    /* wait for reception of all removes (max 2 seconds) */
    log_verbose("waiting for removals");
    gettimeofday(&tv, 0);
    ts.tv_sec = tv.tv_sec + 2;
    ts.tv_nsec += (tv.tv_usec * 1000);
    sem_timedwait(&_sync, &ts);
    log_verbose("done waiting rc = %d", _rc);
    /* clean up */
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
    sem_destroy(&_sync);
    return _rc;
}
