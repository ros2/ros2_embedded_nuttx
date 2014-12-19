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
#include <assert.h>
#include <string.h>

#include "verbose.h"
#include "tsm_types.h"

#define RSA_PUBLIC_KEY "AAAAB3NzaC1yc2EAAAABIwAAAQEAyyA8wePstPC69PeuHFtOwyTecByonsHFAjH"

const DDS_TypeSupport_meta _tsm_inner_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.Inner",
      .flags = TSMFLAG_GENID|TSMFLAG_MUTABLE, .size = sizeof(inner_t), .nelem = INNER_NELEM },
    { .tc = CDR_TYPECODE_LONG, .name = "i32", .offset = offsetof(inner_t, inner_i32) }
};

DDS_TypeSupport_meta _tsm_enum[] =
{
    { .tc = CDR_TYPECODE_ENUM, .name = "comp.technicolor.test.MyEnum", .nelem = 3 },
    { .name = "ENUM_ZERO", .label = ENUM_ZERO },
    { .name = "ENUM_FIRST", .label = ENUM_FIRST },
    { .name = "ENUM_SECOND", .label = ENUM_SECOND },
};

DDS_TypeSupport_meta _tsm_types[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.Types",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE|TSMFLAG_GENID, .size = sizeof(types_t), .nelem = TYPES_NELEM },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, string) },
   { .tc = CDR_TYPECODE_CSTRING, .name = "other",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, other) },
   { .tc = CDR_TYPECODE_OCTET, .name = "i8", .offset = offsetof(types_t, i8) },
   { .tc = CDR_TYPECODE_SHORT, .name = "i16", .offset = offsetof(types_t, i16) },
   { .tc = CDR_TYPECODE_LONG, .name = "i32", .offset = offsetof(types_t, i32) },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "i64", .offset = offsetof(types_t, i64) },
   { .tc = CDR_TYPECODE_FLOAT, .name = "f32", .offset = offsetof(types_t, f32) },
   { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool", .offset = offsetof(types_t, boolean) },
   { .tc = CDR_TYPECODE_SEQUENCE, .name = "a8",
     .flags = TSMFLAG_DYNAMIC, .nelem = 0, .offset = offsetof(types_t, a8) },
   {     .tc = CDR_TYPECODE_OCTET },
   { .tc = CDR_TYPECODE_TYPEREF, .name = "e", .offset = offsetof(types_t, e), .tsm = _tsm_enum },
   { .tc = CDR_TYPECODE_SEQUENCE, .name = "ae",
       .flags = TSMFLAG_DYNAMIC, .nelem = 0, .offset = offsetof(types_t, ae) },
   { .tc = CDR_TYPECODE_TYPEREF, .tsm = _tsm_enum },
   { .tc = CDR_TYPECODE_TYPEREF, .name = "inner_struct", .offset = offsetof(types_t, inner_struct), .tsm = _tsm_inner_type },
};

// START - QDM version testing
// *****************************************************************************************************
DDS_TypeSupport_meta _tsm_type_qdm_version1[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.QDMTestType",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE|TSMFLAG_GENID, .size = sizeof(types_t), .nelem = 7 },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, string) },
   { .tc = CDR_TYPECODE_OCTET, .name = "i8", .offset = offsetof(types_t, i8) },
   { .tc = CDR_TYPECODE_SHORT, .name = "i16", .offset = offsetof(types_t, i16) },
   { .tc = CDR_TYPECODE_LONG, .name = "i32", .offset = offsetof(types_t, i32) },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "i64", .offset = offsetof(types_t, i64) },
   { .tc = CDR_TYPECODE_FLOAT, .name = "f32", .offset = offsetof(types_t, f32) },
   { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool", .offset = offsetof(types_t, boolean) },
};

DDS_TypeSupport_meta _tsm_type_qdm_version2[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.QDMTestType",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE|TSMFLAG_GENID, .size = sizeof(types_t), .nelem = 14 },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, string) },
   { .tc = CDR_TYPECODE_OCTET, .name = "i8", .offset = offsetof(types_t, i8) },
   { .tc = CDR_TYPECODE_SHORT, .name = "i16", .offset = offsetof(types_t, i16) },
   { .tc = CDR_TYPECODE_LONG, .name = "i32", .offset = offsetof(types_t, i32) },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "i64", .offset = offsetof(types_t, i64) },
   { .tc = CDR_TYPECODE_FLOAT, .name = "f32", .offset = offsetof(types_t, f32) },
   { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool", .offset = offsetof(types_t, boolean) },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string_2",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, string) },
   { .tc = CDR_TYPECODE_OCTET, .name = "i8_2", .offset = offsetof(types_t, i8) },
   { .tc = CDR_TYPECODE_SHORT, .name = "i16_2", .offset = offsetof(types_t, i16) },
   { .tc = CDR_TYPECODE_LONG, .name = "i32_2", .offset = offsetof(types_t, i32) },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "i64_2", .offset = offsetof(types_t, i64) },
   { .tc = CDR_TYPECODE_FLOAT, .name = "f32_2", .offset = offsetof(types_t, f32) },
   { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool_2", .offset = offsetof(types_t, boolean) },
};

DDS_TypeSupport_meta _tsm_type_qdm_version3[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.QDMTestType",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE|TSMFLAG_GENID, .size = sizeof(types_t), .nelem = 14 },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, string) },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string_2",
     .flags = TSMFLAG_DYNAMIC, .size = 0, .offset = offsetof(types_t, string) },
   { .tc = CDR_TYPECODE_OCTET, .name = "i8", .offset = offsetof(types_t, i8) },
   { .tc = CDR_TYPECODE_SHORT, .name = "i16", .offset = offsetof(types_t, i16) },
   { .tc = CDR_TYPECODE_OCTET, .name = "i8_2", .offset = offsetof(types_t, i8) },
   { .tc = CDR_TYPECODE_SHORT, .name = "i16_2", .offset = offsetof(types_t, i16) },
   { .tc = CDR_TYPECODE_LONG, .name = "i32", .offset = offsetof(types_t, i32) },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "i64", .offset = offsetof(types_t, i64) },
   { .tc = CDR_TYPECODE_LONG, .name = "i32_2", .offset = offsetof(types_t, i32) },
   { .tc = CDR_TYPECODE_LONGLONG, .name = "i64_2", .offset = offsetof(types_t, i64) },
   { .tc = CDR_TYPECODE_FLOAT, .name = "f32", .offset = offsetof(types_t, f32) },
   { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool", .offset = offsetof(types_t, boolean) },
   { .tc = CDR_TYPECODE_FLOAT, .name = "f32_2", .offset = offsetof(types_t, f32) },
   { .tc = CDR_TYPECODE_BOOLEAN, .name = "bool_2", .offset = offsetof(types_t, boolean) },
};

type_qdm_version1_t _type_qdm_version1 = {
    .string = "string member of version1",
    .i8 = 111,
    .i16 = 11111,
    .i32 = 1111111111,
    .i64 = INT64_C(1111111111111111111),
    .f32 = 1.11,
    .boolean = 1,
};

type_qdm_version2_t _type_qdm_version2 = {
    .string = "string member of version2",
    .i8 = 112,
    .i16 = 11112,
    .i32 = 1111111112,
    .i64 = INT64_C(1111111111111111112),
    .f32 = 1.12,
    .boolean = 0,
    .string_2 = "string_2 member of version2",
    .i8_2 = 122,
    .i16_2 = 11122,
    .i32_2 = 1111111122,
    .i64_2 = INT64_C(1111111111111111122),
    .f32_2 = 1.22,
    .boolean_2 = 0,
};

type_qdm_version3_t _type_qdm_version3 = {
    .string = "string member of version3",
    .string_2 = "string_2 member of version3",
    .i8 = 113,
    .i16 = 11113,
    .i8_2 = 133,
    .i16_2 = 11133,
    .i32 = 1111111113,
    .i64 = INT64_C(1111111111111111113),
    .i32_2 = 1111111133,
    .i64_2 = INT64_C(1111111111111111133),
    .f32 = 1.12,
    .boolean = 0,
    .f32_2 = 1.33,
    .boolean_2 = 0,
};

// *****************************************************************************************************
// END - QDM version testing

static int8_t _a8_data1[] = { 1, 2, 3 };
static enum_t _ae_data1[] = { ENUM_FIRST, ENUM_SECOND };
types_t _types1 = {
    .string = "test 1 2 3",
    .other = "3 2 1 tset",
    .i8 = 123,
    .i16 = 12345,
    .i32 = 1234567890,
    .i64 = INT64_C(1234567890123456789),
    .f32 = 1.23,
    .a8._maximum = sizeof(_a8_data1) / sizeof(int8_t),
    .a8._length = sizeof(_a8_data1) / sizeof(int8_t),
    .a8._esize = sizeof(int8_t),
    .a8._own = 1,
    .a8._buffer = _a8_data1,
    .boolean = 1,
    .e = ENUM_FIRST,
    .ae._maximum = sizeof(_ae_data1) / sizeof(enum_t),
    .ae._length = sizeof(_ae_data1) / sizeof(enum_t),
    .ae._esize = sizeof(enum_t),
    .ae._own = 1,
    .ae._buffer = _ae_data1,
};

static int8_t _a8_data2[] = { 4, 5, 6, 7 };
static enum_t _ae_data2[] = { ENUM_SECOND, ENUM_FIRST };
types_t _types2 = {
    .string = "test 4 5 6 7",
    .other = "7 6 5 4 tset",
    .i8 = 45,
    .i16 = 45678,
    .i32 = 456701234,
    .i64 = INT64_C(4567012345678901234),
    .f32 = 45.67,
    .a8._maximum = sizeof(_a8_data2) / sizeof(int8_t),
    .a8._length = sizeof(_a8_data2) / sizeof(int8_t),
    .a8._esize = sizeof(int8_t),
    .a8._own = 1,
    .a8._buffer = _a8_data2,
    .boolean = 0,
    .e = ENUM_SECOND,
    .ae._maximum = sizeof(_ae_data2) / sizeof(enum_t),
    .ae._length = sizeof(_ae_data2) / sizeof(enum_t),
    .ae._esize = sizeof(enum_t),
    .ae._own = 1,
    .ae._buffer = _ae_data2,
};

static int8_t _a8_data3[] = { 1, 2, 3 };
static enum_t _ae_data3[] = { ENUM_FIRST, ENUM_SECOND };
types_t _types3 = {
    .string = "test 1 2 3",
    .other = "3 2 1 tset",
    .i8 = 123,
    .i16 = 12345,
    .i32 = 1234567890,
    .i64 = INT64_C(1234567890123456789),
    .f32 = 1.23,
    .a8._maximum = sizeof(_a8_data3) / sizeof(int8_t),
    .a8._length = sizeof(_a8_data3) / sizeof(int8_t),
    .a8._esize = sizeof(int8_t),
    .a8._own = 1,
    .a8._buffer = _a8_data3,
    .boolean = 1,
    .e = ENUM_SECOND,
    .ae._maximum = sizeof(_ae_data3) / sizeof(enum_t),
    .ae._length = sizeof(_ae_data3) / sizeof(enum_t),
    .ae._esize = sizeof(enum_t),
    .ae._own = 1,
    .ae._buffer = _ae_data3,
    .inner_struct.inner_i32 = 123456,
};

org_qeo_system_RegistrationRequest_t _regreq[1] = {
    // 0
    {
        .deviceId = {
            .upper = 1,
            .lower = 0,
        },
        .rsaPublicKey = RSA_PUBLIC_KEY,
        .version = 1,
        .manufacturer = "company x",
        .modelName = "the model",
        .userFriendlyName = "Spongebob Squarepants",
        .userName = "bob123",
        .registrationStatus = 1,
        .errorCode = 15,
        .errorMessage = "Everything OK down here",
    }
};

org_qeo_system_RegistrationCredentials_t _regcred[1] = {
    // 0
    {
        .deviceId = {
            .upper = 1,
            .lower = 0,
        },
        .requestRSAPublicKey = RSA_PUBLIC_KEY,
        .url = "the url",
        .realmName = "your realm",
    }
};

void validate_regreq(org_qeo_system_RegistrationRequest_t *act, org_qeo_system_RegistrationRequest_t *exp, bool keyonly)
{
    assert(0 == memcmp(&act->deviceId, &exp->deviceId, sizeof(act->deviceId)));
    assert(0 == strcmp(act->rsaPublicKey, exp->rsaPublicKey));
    assert(0 == strcmp(act->modelName, exp->modelName));
    assert(0 == strcmp(act->userFriendlyName, exp->userFriendlyName));
    assert(0 == strcmp(act->userName, exp->userName));

    if (keyonly == true) return;

    assert(act->registrationStatus == exp->registrationStatus);
    assert(act->errorCode == exp->errorCode);
    assert(0 == strcmp(act->errorMessage, exp->errorMessage));
}

void validate_regcred(org_qeo_system_RegistrationCredentials_t *act, org_qeo_system_RegistrationCredentials_t *exp, bool keyonly)
{
    assert(0 == memcmp(&act->deviceId, &exp->deviceId, sizeof(act->deviceId)));
    assert(0 == strcmp(act->requestRSAPublicKey, exp->requestRSAPublicKey));

    if (keyonly == true) return;

    assert(0 == strcmp(act->url, exp->url));
    assert(0 == strcmp(act->realmName, exp->realmName));
}
