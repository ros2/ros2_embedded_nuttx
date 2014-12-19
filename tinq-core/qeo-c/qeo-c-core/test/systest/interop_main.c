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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <qeo/factory.h>
#include <qeocore/api.h>
#include "common.h"
#include "interop_common.h"
#include "tsm_types.h"
#include "verbose.h"

/* ===[ main test code ]===================================================== */

static void run_test(int flags,
                     vtable_t *vtable)
{
    qeo_factory_t *factory;

    /* initialize */
    if (flags & QEOCORE_EFLAG_STATE) {
        _tsm_types[0].flags |= TSMFLAG_KEY;
        _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    }
    flags |= QEOCORE_EFLAG_ENABLE;
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    /* test */
    do_test(factory, flags, vtable);
    /* clean up */
    qeocore_factory_close(factory);
}

static void run_tests(vtable_t *vtable)
{
    sem_init(&vtable->sync, 0, 0);
    log_pid("running event tests");
    run_test(QEOCORE_EFLAG_EVENT_DATA, vtable);
//    log_pid("running state tests");
//    run_test(QEOCORE_EFLAG_STATE_DATA, vtable);
    sem_destroy(&vtable->sync);
}

int main(int argc, const char **argv)
{
    pid_t pidstat, piddyna;
    int status;

    log_verbose("Qeo version is %s", qeo_version_string());
    /* fork two processes: one for static types and another for dynamic types */
    pidstat = fork();
    assert(-1 != pidstat);
    if (0 == pidstat) {
        run_tests(&_vtable_static);
        return 0;
    }
    piddyna = fork();
    assert(-1 != piddyna);
    if (0 == piddyna) {
        run_tests(&_vtable_dynamic);
        return 0;
    }
    assert(pidstat == waitpid(pidstat, &status, 0));
    assert(0 == status);
    assert(piddyna == waitpid(piddyna, &status, 0));
    assert(0 == status);
    return 0;
}
