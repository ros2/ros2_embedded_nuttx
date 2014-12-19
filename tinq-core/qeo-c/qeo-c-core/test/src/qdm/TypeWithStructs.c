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

#include "TypeWithStructs.h"

const DDS_TypeSupport_meta org_qeo_dynamic_qdm_test_Substruct1_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.dynamic.qdm.test.Substruct1", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(org_qeo_dynamic_qdm_test_Substruct1_t) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "msubstring", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct1_t, msubstring), .size = 0 },  
    { .tc = CDR_TYPECODE_LONG, .name = "msubint32", .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct1_t, msubint32) },  
};

const DDS_TypeSupport_meta org_qeo_dynamic_qdm_test_Substruct2_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.dynamic.qdm.test.Substruct2", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(org_qeo_dynamic_qdm_test_Substruct2_t) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "msubshort", .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct2_t, msubshort) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "msubstring", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct2_t, msubstring), .size = 0 },  
};

const DDS_TypeSupport_meta org_qeo_dynamic_qdm_test_Substruct3_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.dynamic.qdm.test.Substruct3", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 3, .size = sizeof(org_qeo_dynamic_qdm_test_Substruct3_t) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "msubstring", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct3_t, msubstring), .size = 0 },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "msubstruct2", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct3_t, msubstruct2) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = org_qeo_dynamic_qdm_test_Substruct2_type },  
    { .tc = CDR_TYPECODE_FLOAT, .name = "msubfloat", .offset = offsetof(org_qeo_dynamic_qdm_test_Substruct3_t, msubfloat) },  
};

const DDS_TypeSupport_meta org_qeo_dynamic_qdm_test_TypeWithStructs_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.qeo.dynamic.qdm.test.TypeWithStructs", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 4, .size = sizeof(org_qeo_dynamic_qdm_test_TypeWithStructs_t) },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "msubstruct1", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(org_qeo_dynamic_qdm_test_TypeWithStructs_t, msubstruct1) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = org_qeo_dynamic_qdm_test_Substruct1_type },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "msubstruct3", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(org_qeo_dynamic_qdm_test_TypeWithStructs_t, msubstruct3) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = org_qeo_dynamic_qdm_test_Substruct3_type },  
    { .tc = CDR_TYPECODE_FLOAT, .name = "mfloat32", .offset = offsetof(org_qeo_dynamic_qdm_test_TypeWithStructs_t, mfloat32) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "mstring", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_qeo_dynamic_qdm_test_TypeWithStructs_t, mstring), .size = 0 },  
};
