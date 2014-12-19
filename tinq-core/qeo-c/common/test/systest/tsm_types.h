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

#ifndef TEST_TSM_TYPES_H_
#define TEST_TSM_TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <qeocore/api.h>
#include "qdm/qeo_RegistrationRequest.h"
#include "qdm/qeo_RegistrationCredentials.h"

#define TYPES_NELEM 12
#define INNER_NELEM 1

/* ===[ all supported types in a single message ]=== */

DDS_SEQUENCE(int8_t, byte_array_t);

typedef struct {
    int32_t inner_i32;
} inner_t;

typedef enum {
    ENUM_ZERO,
    ENUM_FIRST,
    ENUM_SECOND
} enum_t;

DDS_SEQUENCE(enum_t, enum_array_t);

typedef struct {
    /* primitive types */
    char    *string; // KEY
    char    *other;
    int8_t  i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float   f32;
    qeo_boolean_t boolean;
    /* sequence types */
    byte_array_t a8;
    /* enumeration */
    enum_t e;
    enum_array_t ae;
    /* inner structure */
    inner_t inner_struct;
} types_t;

// START - QDM version testing
// **********************************************************************
// version1 is the basis
typedef struct {
    char    *string; // KEY
    int8_t  i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float   f32;
    qeo_boolean_t boolean;
} type_qdm_version1_t;

// version2 contains extra parameters appended to version1
typedef struct {
    char    *string; // KEY
    int8_t  i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float   f32;
    qeo_boolean_t boolean;
    char    *string_2;
    int8_t  i8_2;
    int16_t i16_2;
    int32_t i32_2;
    int64_t i64_2;
    float   f32_2;
    qeo_boolean_t boolean_2;
} type_qdm_version2_t;

// version3 contains extra parameters inserted between those of version1
typedef struct {
    char    *string; // KEY
    char    *string_2;
    int8_t  i8;
    int16_t i16;
    int8_t  i8_2;
    int16_t i16_2;
    int32_t i32;
    int64_t i64;
    int32_t i32_2;
    int64_t i64_2;
    float   f32;
    qeo_boolean_t boolean;
    float   f32_2;
    qeo_boolean_t boolean_2;
} type_qdm_version3_t;

extern DDS_TypeSupport_meta _tsm_type_qdm_version1[];
extern DDS_TypeSupport_meta _tsm_type_qdm_version2[];
extern DDS_TypeSupport_meta _tsm_type_qdm_version3[];
extern type_qdm_version1_t _type_qdm_version1;
extern type_qdm_version2_t _type_qdm_version2;
extern type_qdm_version3_t _type_qdm_version3;
// **********************************************************************
// END - QDM version testing

extern DDS_TypeSupport_meta _tsm_types[];

extern types_t _types1, _types2, _types3;

extern org_qeo_system_RegistrationRequest_t _regreq[];

extern org_qeo_system_RegistrationCredentials_t _regcred[];

void validate_regreq(org_qeo_system_RegistrationRequest_t *act, org_qeo_system_RegistrationRequest_t *exp, bool keyonly);

void validate_regcred(org_qeo_system_RegistrationCredentials_t *act, org_qeo_system_RegistrationCredentials_t *exp, bool keyonly);

#endif /* TEST_TYPES_H_ */
