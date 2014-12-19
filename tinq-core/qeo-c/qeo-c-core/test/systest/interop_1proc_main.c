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
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <qeocore/api.h>
#include "common.h"
#include "interop_common.h"
#include "tsm_types.h"
#include "verbose.h"

static int _flags;

/* ===[ main test code ]===================================================== */

static void run_test(qeo_factory_t *factory,
                     vtable_t *vtable)
{
    do_test(factory, _flags, vtable);
}

static void run_tests(qeo_factory_t *factory,
                      vtable_t *vtable)
{
    sem_init(&vtable->sync, 0, 0);
    run_test(factory, vtable);
    sem_destroy(&vtable->sync);
}

static void *run_tests_stat(void *data)
{
    qeo_factory_t *factory = (qeo_factory_t *)data;

    run_tests(factory, &_vtable_static);
    pthread_exit(NULL);
}

static void *run_tests_dyna(void *data)
{
    qeo_factory_t *factory = (qeo_factory_t *)data;

    run_tests(factory, &_vtable_dynamic);
    pthread_exit(NULL);
}

static void run_tests_etype()
{
    pthread_t tidstat = 0, tiddyna = 0;
    qeo_factory_t *factory;
    void *status;

    log_pid("running %s tests", (_flags & QEOCORE_EFLAG_STATE ? "state" : "event"));
    /* initialize */
    if (_flags & QEOCORE_EFLAG_STATE) {
        _tsm_types[0].flags |= TSMFLAG_KEY;
        _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    }
    _flags |= QEOCORE_EFLAG_ENABLE;
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    /* start two threads: one for static types and another for dynamic types */
    assert(0 == pthread_create(&tidstat, NULL, run_tests_stat, factory));
    sleep(1); /* avoid DE3420 */
    assert(0 == pthread_create(&tiddyna, NULL, run_tests_dyna, factory));
    pthread_join(tidstat, &status);
    assert(NULL == status);
    pthread_join(tiddyna, &status);
    assert(NULL == status);
    qeocore_factory_close(factory);
}

int main(int argc, const char **argv)
{
    _flags = QEOCORE_EFLAG_EVENT_DATA;
    run_tests_etype();
    //_flags = QEOCORE_EFLAG_STATE_DATA;
    //run_tests_etype();
    return 0;
}
