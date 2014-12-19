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
#include <unistd.h>
#include <sys/wait.h>

#include <qeocore/api.h>
#include "common.h"
#include "verbose.h"

static sem_t _sync;

#define TEST_SIZE 100*1024
static int _test_size = TEST_SIZE;

DDS_SEQUENCE(uint8_t, byte_array_t);

typedef struct {
    uint32_t size; /* key */
    byte_array_t buf;
} type_t;

DDS_TypeSupport_meta _tsm[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.test.bigdata",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE|TSMFLAG_GENID|TSMFLAG_KEY,
     .size = sizeof(type_t), .nelem = 2 },
   { .tc = CDR_TYPECODE_LONG, .name = "size",
     .flags = TSMFLAG_KEY, .offset = offsetof(type_t, size) },
   { .tc = CDR_TYPECODE_SEQUENCE, .name = "buf",
     .flags = TSMFLAG_DYNAMIC, .nelem = 0, .offset = offsetof(type_t, buf) },
   { .tc = CDR_TYPECODE_OCTET }
};

static void my_on_data_available(const qeocore_reader_t *reader,
                                 const qeocore_data_t *data,
                                 uintptr_t userdata)
{
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            type_t *d = NULL;
            int i;

            log_pid("reader received data");
            assert(NULL != (d = (type_t *)qeocore_data_get_data(data)));
            assert(d->size == DDS_SEQ_LENGTH(d->buf));
            assert(_test_size == DDS_SEQ_LENGTH(d->buf));
            for (i = 0; i < d->size; i++) {
                assert(DDS_SEQ_ITEM(d->buf, i) == (i & 0xff));
            }
            sem_post(&_sync); /* release main thread */
            break;
        }
        case QEOCORE_NO_MORE_DATA:
        case QEOCORE_REMOVE:
            /* ignore */
            break;
        default:
            abort();
            break;
    }
}

static void run_writer(pid_t peer)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_writer_t *writer;
    type_t data;
    int status, i;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm, _tsm->name)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    log_pid("writer initialized");
    DDS_SEQ_INIT(data.buf);
    DDS_SEQ_DATA(data.buf) = malloc(_test_size * sizeof(char));
    assert(DDS_SEQ_DATA(data.buf));
    DDS_SEQ_LENGTH(data.buf) = DDS_SEQ_MAXIMUM(data.buf) = _test_size;
    for (i = 0; i < _test_size; i++) {
        DDS_SEQ_ITEM(data.buf, i) = i & 0xff;
    }
    data.size = _test_size;
    assert(QEO_OK == qeocore_writer_write(writer, &data));
    log_pid("writer wrote data");
    assert(peer == waitpid(peer, &status, 0));
    assert(0 == status);
    log_pid("writer done");
    /* clean up */
    free(DDS_SEQ_DATA(data.buf));
    qeocore_writer_close(writer);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
}

static void run_reader(void)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = my_on_data_available };

    sem_init(&_sync, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm, _tsm->name)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL,
                                                 QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    log_pid("reader initialized");
    sem_wait(&_sync); /* wait for sample */
    log_pid("reader done");
    sem_destroy(&_sync);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
}

int main(int argc, const char **argv)
{
    pid_t pidreader;

    if (argc == 2) {
        _test_size = atoi(argv[1]);
    }
    pidreader = fork();
    assert(-1 != pidreader);
    if (0 == pidreader) {
        run_reader();
    }
    else {
        run_writer(pidreader);
    }
    return 0;
}
