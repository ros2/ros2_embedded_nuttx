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

#include "qeo_Forwarder.h"

const DDS_TypeSupport_meta org_qeo_system_ForwarderLocator_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.system.ForwarderLocator", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 3, .size = sizeof(org_qeo_system_ForwarderLocator_t) },  
    { .tc = CDR_TYPECODE_LONG, .name = "type", .offset = offsetof(org_qeo_system_ForwarderLocator_t, type) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "address", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_ForwarderLocator_t, address), .size = 0 },  
    { .tc = CDR_TYPECODE_LONG, .name = "port", .offset = offsetof(org_qeo_system_ForwarderLocator_t, port) },  
};

const DDS_TypeSupport_meta org_qeo_system_Forwarder_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.system.Forwarder", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(org_qeo_system_Forwarder_t) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "deviceId", .flags = TSMFLAG_KEY, .offset = offsetof(org_qeo_system_Forwarder_t, deviceId) },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "locator", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(org_qeo_system_Forwarder_t, locator) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = org_qeo_system_ForwarderLocator_type },  
};
