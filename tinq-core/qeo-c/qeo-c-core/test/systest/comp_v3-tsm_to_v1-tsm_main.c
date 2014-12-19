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
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <qeocore/api.h>
#include "tsm_types.h"
#include "common.h"
#include "verbose.h"

static sem_t _sync;
static sem_t _sync_nodata;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    log_verbose(" =================== %d on_data_available( is a type1 reader) \n", getpid());
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            log_verbose(" =================== case QEOCORE_DATA \n");
            type_qdm_version1_t *t = (type_qdm_version1_t *)qeocore_data_get_data(data);
            /* verify struct */
            log_verbose(" =================== received t->string = %s \n", t->string );
            log_verbose(" =================== received t->i8 = %u \n", t->i8 );
            log_verbose(" =================== received t->i16 = %u \n", t->i16 );
            log_verbose(" =================== received t->i32 = %u \n", t->i32 );
            log_verbose(" =================== received t->i64 = %" PRIu64 " \n", t->i64 );
            log_verbose(" =================== received t->f32 = %f \n", t->f32 );
            log_verbose(" =================== received t->boolean = %d \n", t->boolean );
            assert(0 == strcmp(_type_qdm_version3.string, t->string));
            assert(_type_qdm_version3.i8 == t->i8);
            assert(_type_qdm_version3.i16 == t->i16);
            assert(_type_qdm_version3.i32 == t->i32);
            assert(_type_qdm_version3.i64 == t->i64);
            assert(_type_qdm_version3.f32 == t->f32);
            assert(_type_qdm_version3.boolean == t->boolean);
            /* release main thread */
            log_verbose(" =================== sem_post(&_sync) \n");
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            log_verbose(" =================== case QEOCORE_NO_MORE_DATA \n");
            /* release main thread */
            log_verbose(" =================== sem_post(&_sync_nodata) \n");
            sem_post(&_sync_nodata);
            break;
        default:
            abort();
            break;
    }
}

int main(int argc, const char **argv)
{

    pid_t pid;

    pid = fork();

    if (pid == -1) {
       perror("fork failed");
       exit(EXIT_FAILURE);
    }
    else if (pid == 0) {
       log_verbose("=================== WRITER of type 3 \n");

       qeo_factory_t *factory;
       qeocore_type_t *type3;
       qeocore_writer_t *writer;

       /* initialize */
       assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
       init_factory(factory);
       log_verbose(" =================== c qeocore_type_register_tsm(factory, _tsm_type_qdm_version3, _tsm_type_qdm_version3[0].name) \n");
       assert(NULL != (type3 = qeocore_type_register_tsm(factory, _tsm_type_qdm_version3, _tsm_type_qdm_version3[0].name)));
       log_verbose(" =================== c qeocore_writer_open(factory, type3, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL) \n");
       assert(NULL != (writer = qeocore_writer_open(factory, type3, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL)));
       /* test late enabling of readers/writers */
       log_verbose(" =================== c qeocore_writer_enable(writer) \n");
       assert(QEO_OK == qeocore_writer_enable(writer));
       /* send structure */
       sleep(2);
       log_verbose(" =================== c qeocore_writer_write(writer, &_type_qdm_version3) \n");
       assert(QEO_OK == qeocore_writer_write(writer, &_type_qdm_version3));
       sleep(2);
       /* clean up */
       qeocore_writer_close(writer);
       qeocore_type_free(type3);
       qeocore_factory_close(factory);
       _exit(EXIT_SUCCESS);
    }
    else {
       log_verbose("=================== READER of type 1 \n");

       qeo_factory_t *factory;
       qeocore_type_t *type1;
       qeocore_reader_t *reader;
       qeocore_reader_listener_t listener = { .on_data = on_data_available };

       /* initialize */
       sem_init(&_sync, 0, 0);
       sem_init(&_sync_nodata, 0, 0);
       assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
       init_factory(factory);
       log_verbose(" =================== p qeocore_type_register_tsm(factory, _tsm_type_qdm_version1, _tsm_type_qdm_version1[0].name) \n");
       assert(NULL != (type1 = qeocore_type_register_tsm(factory, _tsm_type_qdm_version1, _tsm_type_qdm_version1[0].name)));
       log_verbose(" =================== p qeocore_reader_open(factory, type1, NULL, QEOCORE_EFLAG_EVENT_DATA, &listener, NULL)  \n");
       assert(NULL != (reader = qeocore_reader_open(factory, type1, NULL, QEOCORE_EFLAG_EVENT_DATA, &listener, NULL)));
       /* test late enabling of readers/writers */
       log_verbose(" =================== p qeocore_reader_enable(reader) \n");
       assert(QEO_OK == qeocore_reader_enable(reader));
       /* wait for reception */
       log_verbose(" =================== p sem_wait(&_sync) \n");
       sem_wait(&_sync);
       log_verbose(" =================== p sem_wait(&_sync_nodata) \n");
       sem_wait(&_sync_nodata);
       /* clean up */
       qeocore_reader_close(reader);
       qeocore_type_free(type1);
       qeocore_factory_close(factory);
       sem_destroy(&_sync);

       int status;
       (void)waitpid(pid, &status, 0);
    }
    return EXIT_SUCCESS;



}
