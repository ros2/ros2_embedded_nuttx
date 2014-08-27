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

/**************************************************************
 ********          THIS IS A GENERATED FILE         ***********
 **************************************************************/

#include "dds/dds_tsm.h"
#include "qeo_types.h"

const DDS_TypeSupport_meta org_qeo_system_DeviceId_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.system.DeviceId", .flags = TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(org_qeo_system_DeviceId_t) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "upper", .offset = offsetof(org_qeo_system_DeviceId_t, upper) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "lower", .offset = offsetof(org_qeo_system_DeviceId_t, lower) },  
};

const DDS_TypeSupport_meta org_qeo_UUID_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.UUID", .flags = TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(org_qeo_UUID_t) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "upper", .offset = offsetof(org_qeo_UUID_t, upper) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "lower", .offset = offsetof(org_qeo_UUID_t, lower) },  
};

