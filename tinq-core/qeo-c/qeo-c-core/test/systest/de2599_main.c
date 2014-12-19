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
#include <stdio.h>
#include <qeocore/api.h>
#include <qeocore/dyntype.h>
#include <qeo/factory.h>

#include "common.h"
#include "dyn_types.h"
#include "qeo_RegistrationRequest.h"

int main()
{
    qeo_factory_t *open_qeo = NULL;
    qeo_factory_t *closed_qeo = NULL;
    qeocore_type_t *tsm_type = NULL;
    qeocore_type_t *dyn_type = NULL;

    /* create two factories */
    assert(NULL != (open_qeo = qeocore_factory_new(QEO_IDENTITY_OPEN)));
    init_factory(open_qeo);
    assert(NULL != (closed_qeo = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(closed_qeo);
    /* register TSM type in open domain factory */
    assert(NULL != (tsm_type = qeocore_type_register_tsm(open_qeo, org_qeo_system_RegistrationRequest_type,
                                                         org_qeo_system_RegistrationRequest_type->name)));
    /* register dynamic type in closed domain factory */
    assert(NULL != (dyn_type = types_get(org_qeo_system_RegistrationRequest_type)));
    assert(QEO_OK == qeocore_type_register(closed_qeo, dyn_type, org_qeo_system_RegistrationRequest_type->name));
    /* clean up */
    qeocore_type_free(dyn_type);
    qeocore_type_free(tsm_type);
    qeocore_factory_close(closed_qeo);
    qeocore_factory_close(open_qeo);
    return 0;
}
