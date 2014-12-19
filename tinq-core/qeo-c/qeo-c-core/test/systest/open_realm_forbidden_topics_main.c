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

#include <qeocore/api.h>
#include "tsm_types.h"
#include "verbose.h"
#include "common.h"


int main(int argc, const char **argv)
{
    qeo_factory_t *factory;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_OPEN)));
    init_factory(factory);
    assert(NULL == qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name));
    _tsm_types[0].flags |= TSMFLAG_KEY;
    _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    assert(NULL == qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name));

    qeocore_factory_close(factory);

    return 0;
}
