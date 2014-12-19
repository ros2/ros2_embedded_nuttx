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

#include "qeo_DeviceInfo.h"

const DDS_TypeSupport_meta org_qeo_system_DeviceInfo_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.system.DeviceInfo", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 9, .size = sizeof(org_qeo_system_DeviceInfo_t) },  
    { .tc = CDR_TYPECODE_TYPEREF, .name = "deviceId", .flags = TSMFLAG_KEY, .offset = offsetof(org_qeo_system_DeviceInfo_t, deviceId), .tsm = org_qeo_system_DeviceId_type },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "manufacturer", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, manufacturer), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "modelName", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, modelName), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "productClass", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, productClass), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "serialNumber", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, serialNumber), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "hardwareVersion", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, hardwareVersion), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "softwareVersion", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, softwareVersion), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "userFriendlyName", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, userFriendlyName), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "configURL", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_DeviceInfo_t, configURL), .size = 0 },  
};
