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
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <qeocore/api.h>
#include "tsm_types.h"
#include "common.h"

#define NUM_W 5
#define NUM_R 5
#define NUM_E 1000

static qeo_factory_t *_factory = NULL;
static qeocore_type_t *_type = NULL;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
#if 0
    switch (qeo_data_get_status(data)) {
        case QEOCORE_DATA: {
            types_t *t = (types_t *)qeo_data_get_data(data);

            printf("%"PRIx64", %"PRId32"\n", t->i64, t->i32);
            break;
        }
        default:
            break;
    }
#endif
}

void *run(void *cookie)
{
    int *id = (int *)cookie;
    qeocore_writer_t *wr;
    types_t data = { .string = "", .other = "", .i64 = 0LL + *id };

    assert(NULL != (wr = qeocore_writer_open(_factory, _type, NULL, QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE,
                                             NULL, NULL)));
    for (data.i32 = 0; data.i32 < NUM_E; data.i32++) {
        assert(QEO_OK == qeocore_writer_write(wr, &data));
    }
    qeocore_writer_close(wr);
    return NULL;
}

int main(int argc, const char **argv)
{
    pthread_t th[NUM_W];
    qeocore_reader_t *rd[NUM_R];
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    int i;


    /* create readers */
    assert(NULL != (_factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(_factory);
    assert(NULL != (_type = qeocore_type_register_tsm(_factory, _tsm_types, _tsm_types[0].name)));
    for (i = 0; i < NUM_R; i++) {
        assert(NULL != (rd[i] = qeocore_reader_open(_factory, _type, NULL,
                                                    QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE, &listener, NULL)));
    }
    /* create writers */
    for (i = 0; i < NUM_W; i++) {
        assert(0 == pthread_create(&th[i], NULL, run, &i));
    }
    /* wait for writers to complete their task */
    for (i = 0; i < NUM_W; i++) {
        pthread_join(th[i], NULL);
    }
    sleep(3);
    /* clean up */
    for (i = 0; i < NUM_R; i++) {
        qeocore_reader_close(rd[i]);
    }
    qeocore_type_free(_type);
    qeocore_factory_close(_factory);
    return 0;
}
