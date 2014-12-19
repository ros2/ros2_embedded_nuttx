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
#include "verbose.h"

#include "qeo_DeviceInfo.h"

int main()
{
    qeo_factory_t *factory;
    qeocore_type_t *devinfo_type = NULL;

    /* create dynamic device info type */
    assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_OPEN)));
    assert(NULL != (devinfo_type = types_get(org_qeo_system_DeviceInfo_type)));

    /* registration of invalid topic in open domain */
    assert(QEO_OK != qeocore_type_register(factory, devinfo_type, "org.qeo.system.DeviceInfo"));
    qeo_factory_close(factory);

    /* let device info type survive factory on purpose !!! */
    
    assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_DEFAULT)));
    /* registration of topic in closed domain should be ok */
    assert(QEO_OK == qeocore_type_register(factory, devinfo_type, "org.qeo.system.DeviceInfo"));
    qeocore_type_free(devinfo_type);
    qeo_factory_close(factory);
    
    log_verbose("OK!");

    return 0;
}
