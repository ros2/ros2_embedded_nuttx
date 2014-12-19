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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <qeo/factory.h>
#include <qeocore/factory.h>
#include "common.h"
#include "verbose.h"
#include "core.h"

static const char *_ip = "111.111.111.111";
static int _port = 12345;

static void check_forwarder(qeo_factory_t *factory,
                            const char *ip,
                            int port)
{
    assert(factory->fwd.locator);
    assert(0 == strcmp(factory->fwd.locator->address, ip));
    assert(port == factory->fwd.locator->port);
}

static void get_public_locator_callback(qeo_factory_t *factory)
{
    assert(QEO_OK == qeocore_fwdfactory_set_public_locator(factory, _ip, 12345));
}

int main(int argc, const char **argv)
{
    pid_t pidforwarder;
    int status;
    qeo_factory_t *factory = NULL;

    /* fork a forwarder process */
    pidforwarder = fork();
    assert(-1 != pidforwarder);
    if (0 == pidforwarder) {
        assert(NULL != (factory = qeocore_fwdfactory_new(get_public_locator_callback, "10101")));
        sleep(20);
        qeocore_fwdfactory_close(factory);
        return 0;
    }
    else {
        sleep(5);
        factory = qeo_factory_create();
        sleep(10);
        check_forwarder(factory, _ip, _port);
        qeo_factory_close(factory);
        assert(pidforwarder == waitpid(pidforwarder, &status, 0));
        assert(0 == status);
        return 0;
    }
}
