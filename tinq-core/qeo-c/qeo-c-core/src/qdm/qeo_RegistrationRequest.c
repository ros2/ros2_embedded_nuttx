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

#include "qeo_RegistrationRequest.h"

const DDS_TypeSupport_meta org_qeo_system_RegistrationRequest_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.system.RegistrationRequest", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 10, .size = sizeof(org_qeo_system_RegistrationRequest_t) },  
    { .tc = CDR_TYPECODE_TYPEREF, .name = "deviceId", .flags = TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, deviceId), .tsm = org_qeo_system_DeviceId_type },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "rsaPublicKey", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, rsaPublicKey), .size = 0 },  
    { .tc = CDR_TYPECODE_SHORT, .name = "version", .flags = TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, version) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "manufacturer", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, manufacturer), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "modelName", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, modelName), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "userFriendlyName", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, userFriendlyName), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "userName", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationRequest_t, userName), .size = 0 },  
    { .tc = CDR_TYPECODE_SHORT, .name = "registrationStatus", .offset = offsetof(org_qeo_system_RegistrationRequest_t, registrationStatus) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "errorCode", .offset = offsetof(org_qeo_system_RegistrationRequest_t, errorCode) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "errorMessage", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_RegistrationRequest_t, errorMessage), .size = 0 },  
};
