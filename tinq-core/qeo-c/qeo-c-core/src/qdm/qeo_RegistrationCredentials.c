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

#include "qeo_RegistrationCredentials.h"

const DDS_TypeSupport_meta org_qeo_system_RegistrationCredentials_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.system.RegistrationCredentials", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 5, .size = sizeof(org_qeo_system_RegistrationCredentials_t) },  
    { .tc = CDR_TYPECODE_TYPEREF, .name = "deviceId", .flags = TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationCredentials_t, deviceId), .tsm = org_qeo_system_DeviceId_type },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "requestRSAPublicKey", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(org_qeo_system_RegistrationCredentials_t, requestRSAPublicKey), .size = 0 },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "encryptedOtc", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(org_qeo_system_RegistrationCredentials_t, encryptedOtc) },  
    { .tc = CDR_TYPECODE_OCTET },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "url", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_RegistrationCredentials_t, url), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "realmName", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_system_RegistrationCredentials_t, realmName), .size = 0 },  
};
