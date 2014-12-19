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

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "crc32.h"
#include "core.h"
#include "typesupport.h"
#include "core_util.h"

#define VALIDATE_NON_FINAL(type) if ((NULL == (type)) || ((type)->flags.final)) return QEO_EINVAL
#define VALIDATE_FINAL(type) if ((NULL == (type)) || !((type)->flags.final)) return QEO_EINVAL
#define TYPE_NAME(type) ((type)->flags.tsm_based ? (type)->u.tsm_based.name : (type)->u.dynamic.name)

/**
 * Lock to protect type cache.
 */
static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;

#define LOCK pthread_mutex_lock(&_lock)
#define UNLOCK pthread_mutex_unlock(&_lock)

//#define LOG_REFCNT
#ifdef LOG_REFCNT
#define qeo_log_d_refcnt(format, ...) qeo_log_d(format, ##__VA_ARGS__)
#else
#define qeo_log_d_refcnt(format, ...)
#endif

/* ===[ miscs ]============================================================== */

uint32_t calculate_member_id(const char *name)
{
    uint32_t crc = crc32(0, name, strlen(name));

    crc &= 0x0FFFFFFF;
    if (crc < 2) {
        crc += 2;
    }
    return crc;
}

static qeo_retcode_t set_ext_annotation(DDS_DynamicTypeBuilder tb, const char *ext)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OUT_OF_RESOURCES;
    DDS_AnnotationDescriptor ad = { 0 };

    DDS_AnnotationDescriptor__clear(&ad);
    ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation("Extensibility");
    qeo_log_dds_null("DDS_DynamicTypeBuilderFactory_get_builtin_annotation", ad.type);
    if (ad.type != NULL) {
        ddsrc = DDS_AnnotationDescriptor_set_value(&ad, "value", ext);
        qeo_log_dds_rc("DDS_AnnotationDescriptor_set_value", ddsrc);
        if (DDS_RETCODE_OK == ddsrc) {
            ddsrc = DDS_DynamicTypeBuilder_apply_annotation(tb, &ad);
            qeo_log_dds_rc("DDS_AnnotationDescriptor_set_value", ddsrc);
        }
        DDS_AnnotationDescriptor__clear(&ad);
    }
    return ddsrc_to_qeorc(ddsrc);
}

/* ===[ structs ]======================================================= */

static qeo_retcode_t make_key(DDS_DynamicTypeBuilder tb, DDS_MemberId id)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OUT_OF_RESOURCES;
    DDS_DynamicTypeMember member = DDS_DynamicTypeMember__alloc();
    DDS_AnnotationDescriptor ead;

    DDS_AnnotationDescriptor__init(&ead);
    ead.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation("Key");
    qeo_log_dds_null("DDS_DynamicTypeBuilderFactory_get_builtin_annotation", ead.type);
    if (NULL != ead.type) {
        ddsrc = DDS_DynamicTypeBuilder_get_member(tb, member, id);
        qeo_log_dds_rc("DDS_DynamicTypeBuilder_get_member", ddsrc);
        if (DDS_RETCODE_OK == ddsrc) {
            ddsrc = DDS_DynamicTypeMember_apply_annotation(member, &ead);
            qeo_log_dds_rc("DDS_DynamicTypeMember_apply_annotation", ddsrc);
        }
        DDS_AnnotationDescriptor__clear(&ead);
    }
    DDS_DynamicTypeMember__free(member);
    return ddsrc_to_qeorc(ddsrc);
}

/**
 * \pre \a container, \a member and \a id are non-\c NULL
 */
static qeo_retcode_t member_add(qeocore_type_t *container,
                                DDS_DynamicType member,
                                const char *name,
                                qeocore_member_id_t *id,
                                unsigned int flags)
{
    qeo_retcode_t rc = QEO_OK;
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OUT_OF_RESOURCES;
    DDS_MemberDescriptor *md;

    md = DDS_MemberDescriptor__alloc();
    qeo_log_dds_null("DDS_MemberDescriptor__alloc", md);
    if (NULL != md) {
        md->name = (DDS_ObjectName)name;
        if (NULL != id) {
            if (QEOCORE_MEMBER_ID_DEFAULT == *id) {
                *id = calculate_member_id(name);
            }
            md->id = *id;
        }
        else {
            /* for enumerator */
            md->id = DDS_MEMBER_ID_INVALID;
            md->index = INT_MAX; /* to force append to existing list */
        }
        md->type = member;
        ddsrc = DDS_DynamicTypeBuilder_add_member(container->u.intermediate.tb, md);
        qeo_log_dds_rc("DDS_DynamicTypeBuilder_add_member", ddsrc);
        if (DDS_RETCODE_OK == ddsrc) {
            if (flags & QEOCORE_FLAG_KEY) {
                rc = make_key(container->u.intermediate.tb, md->id);
                if (QEO_OK == rc) {
                    container->flags.keyed = 1;
                }
            }
        }
        DDS_MemberDescriptor__free(md);
    }
    return (QEO_OK == rc ? ddsrc_to_qeorc(ddsrc) : rc);
}

/* ===[ dynamic type ]================================================== */

static DDS_DynamicType build_primitive(qeocore_typecode_t tc)
{
    DDS_DynamicType dtype = NULL;

    switch (tc) {
        case QEOCORE_TYPECODE_INT8:
            dtype = DDS_DynamicTypeBuilderFactory_get_primitive_type(DDS_BYTE_TYPE);
            break;
        case QEOCORE_TYPECODE_INT16:
            dtype = DDS_DynamicTypeBuilderFactory_get_primitive_type(DDS_INT_16_TYPE);
            break;
        case QEOCORE_TYPECODE_INT32:
            dtype = DDS_DynamicTypeBuilderFactory_get_primitive_type(DDS_INT_32_TYPE);
            break;
        case QEOCORE_TYPECODE_INT64:
            dtype = DDS_DynamicTypeBuilderFactory_get_primitive_type(DDS_INT_64_TYPE);
            break;
        case QEOCORE_TYPECODE_FLOAT32:
            dtype = DDS_DynamicTypeBuilderFactory_get_primitive_type(DDS_FLOAT_32_TYPE);
            break;
        case QEOCORE_TYPECODE_BOOLEAN:
            dtype = DDS_DynamicTypeBuilderFactory_get_primitive_type(DDS_BOOLEAN_TYPE);
            break;
        default:
            qeo_log_e("unsupported type %d", tc);
            abort(); // unsupported for now
            break;
    }
    qeo_log_dds_null("DDS_DynamicTypeBuilderFactory_get_primitive_type", dtype);
    return dtype;
}

static DDS_DynamicType build_string(size_t sz)
{
    DDS_DynamicTypeBuilder tb;
    DDS_DynamicType dtype = NULL;

    tb = DDS_DynamicTypeBuilderFactory_create_string_type(sz);
    qeo_log_dds_null("DDS_DynamicTypeBuilderFactory_create_string_type", tb);
    if (NULL != tb) {
        dtype = DDS_DynamicTypeBuilder_build(tb);
        qeo_log_dds_null("DDS_DynamicTypeBuilder_build", dtype);
        DDS_DynamicTypeBuilderFactory_delete_type(tb);
    }
    return dtype;
}

static DDS_DynamicType build_sequence(DDS_DynamicType elem_dtype)
{
    DDS_DynamicTypeBuilder tb;
    DDS_DynamicType dtype = NULL;

    tb = DDS_DynamicTypeBuilderFactory_create_sequence_type(elem_dtype, 0);
    qeo_log_dds_null("DDS_DynamicTypeBuilderFactory_create_sequence_type", tb);
    if (NULL != tb) {
        dtype = DDS_DynamicTypeBuilder_build(tb);
        qeo_log_dds_null("DDS_DynamicTypeBuilder_build", dtype);
        DDS_DynamicTypeBuilderFactory_delete_type(tb);
    }
    return dtype;
}

/* ===[ structs ]============================================================ */

/**
 * Convert a non-final Qeo type into a final one and register it towards DDS.
 *
 * \pre Both \a type and \a name should be non-\c NULL.
 */
static qeo_retcode_t type_build_dynamic(const qeo_factory_t *factory,
                                        qeocore_type_t *type,
                                        const char *name,
                                        int do_register)
{
    qeo_retcode_t rc = QEO_EFAIL;

    if (do_register && (NULL == factory)) {
        rc = QEO_EINVAL;
    }
    else if (!type->flags.final && (NULL != type->u.intermediate.tb) &&
             ((QEOCORE_TYPECODE_STRUCT == type->u.intermediate.tc) ||
              (QEOCORE_TYPECODE_ENUM == type->u.intermediate.tc))) {
        DDS_DynamicTypeBuilder tb = type->u.intermediate.tb;
        DDS_DynamicType dtype = NULL;
        DDS_DynamicTypeSupport dts = NULL;

        dtype = DDS_DynamicTypeBuilder_build(tb);
        qeo_log_dds_null("DDS_DynamicTypeBuilder_build", dtype);
        if (NULL != dtype) {
            dts = DDS_DynamicTypeSupport_create_type_support(dtype);
            qeo_log_dds_null("DDS_DynamicTypeSupport_create_type_support", dts);
            if (NULL != dts) {
                rc = QEO_OK;
                if (do_register) {
                    rc = core_register_type(factory, dts, NULL, name);
                }
                if (QEO_OK == rc) {
                    type->factory = factory;
                    type->flags.refcnt = 1;
                    type->flags.final = 1;
                    type->flags.registered = (do_register ? 1 : 0);
                    type->u.dynamic.name = (NULL == name ? NULL : strdup(name));
                    type->u.dynamic.dtype = dtype;
                    type->u.dynamic.ts = dts;
                    qeo_log_d_refcnt("type %s created refcnt -> %d",
                                     type->u.dynamic.name, type->flags.refcnt);
                }
            }
        }
        if (QEO_OK != rc) {
            if (NULL != dtype) {
                DDS_DynamicTypeBuilderFactory_delete_type(dtype);
            }
            if (NULL != dts) {
                DDS_DynamicTypeSupport_delete_type_support(dts);
            }
        }
        else {
            DDS_DynamicTypeBuilderFactory_delete_type(tb);
        }
    }
    return rc;
}

/**
 * Create a Qeo type using the provided TSM and register it towards DDS.
 *
 * \pre Both \a tsm and \a name should be non-\c NULL.
 */
static qeocore_type_t *type_build_tsm(const qeo_factory_t *factory,
                                  const DDS_TypeSupport_meta *tsm,
                                  const char *name)
{
    qeo_retcode_t rc = QEO_EFAIL;
    qeocore_type_t *type = NULL;

    type = calloc(1, sizeof(qeocore_type_t));
    if (NULL != type) {
        DDS_TypeSupport ts = NULL;

        ts = DDS_DynamicType_register(tsm);
        qeo_log_dds_null("DDS_DynamicType_register", ts);
        if (NULL != ts) {
            rc = core_register_type(factory, NULL, ts, name);
            if (QEO_OK == rc) {
                type->factory = factory;
                type->flags.refcnt = 1;
                type->flags.final = 1;
                type->flags.tsm_based = 1;
                type->flags.registered = 1;
                type->flags.keyed = ((tsm->flags & TSMFLAG_KEY) == TSMFLAG_KEY) ? 1 : 0;
                type->u.tsm_based.name = strdup(name);
                type->u.tsm_based.tsm = tsm;
                type->u.tsm_based.ts = ts;
                qeo_log_d_refcnt("type %s created refcnt -> %d",
                                 type->u.tsm_based.name, type->flags.refcnt);
            }
        }
        if (QEO_OK != rc) {
            if (NULL != ts) {
                DDS_DynamicType_free(ts);
            }
            free(type);
            type = NULL;
        }
    }
    return type;
}

static void type_free(qeocore_type_t *type)
{
    if (type->flags.tsm_based) {
        if (type->flags.registered) {
            core_unregister_type(type->factory, NULL, type->u.tsm_based.ts, type->u.tsm_based.name);
        }
        DDS_DynamicType_free(type->u.tsm_based.ts);
        if (NULL != type->u.tsm_based.name) {
            free((char *)type->u.tsm_based.name);
        }
    }
    else if (type->flags.final /* final dynamic */) {
        if (type->flags.registered) {
            core_unregister_type(type->factory, type->u.dynamic.ts, NULL, type->u.dynamic.name);
        }
        DDS_DynamicTypeBuilderFactory_delete_type(type->u.dynamic.dtype);
        DDS_DynamicTypeSupport_delete_type_support(type->u.dynamic.ts);
        if (NULL != type->u.dynamic.name) {
            free((char *)type->u.dynamic.name);
        }
    }
    else /* intermediate dynamic */ {
        switch (type->u.intermediate.tc) {
            case QEOCORE_TYPECODE_INT8:
            case QEOCORE_TYPECODE_INT16:
            case QEOCORE_TYPECODE_INT32:
            case QEOCORE_TYPECODE_INT64:
            case QEOCORE_TYPECODE_FLOAT32:
            case QEOCORE_TYPECODE_BOOLEAN:
                /* nop */
                break;
            case QEOCORE_TYPECODE_STRING:
            case QEOCORE_TYPECODE_SEQUENCE:
                DDS_DynamicTypeBuilderFactory_delete_type(type->u.intermediate.dtype);
                break;
            case QEOCORE_TYPECODE_STRUCT:
                DDS_DynamicTypeBuilderFactory_delete_type(type->u.intermediate.tb);
                break;
            default:
                qeo_log_e("unsupported type %d", type->u.intermediate.tc);
                abort(); // unsupported for now
                break;
        }
    }
    free(type);
}

/**
 * \pre ::_lock should be locked when calling this function
 */
static qeo_retcode_t type_register(const qeo_factory_t *factory,
                                   qeocore_type_t *type,
                                   const char *name)
{
    return type_build_dynamic(factory, type, name, 1);
}

static DDS_TypeKind typecode_2_typekind(qeocore_typecode_t tc)
{
    switch (tc) {
        case QEOCORE_TYPECODE_STRUCT:
            return DDS_STRUCTURE_TYPE;
        case QEOCORE_TYPECODE_ENUM:
            return DDS_ENUMERATION_TYPE;
        default:
            break;
    }
    return DDS_NO_TYPE;
}

static qeocore_type_t *type_new(const char *name,
                                qeocore_typecode_t tc)
{
    qeocore_type_t *type = NULL;

    type = calloc(1, sizeof(qeocore_type_t));
    if (NULL != type) {
        DDS_TypeDescriptor *desc;
        qeo_retcode_t rc = QEO_OK;

        desc = DDS_TypeDescriptor__alloc();
        qeo_log_dds_null("DDS_TypeDescriptor__alloc", desc);
        if (NULL != desc) {
            desc->kind = typecode_2_typekind(tc);
            desc->name = (DDS_ObjectName)name;
            type->u.intermediate.tc = tc;
            type->u.intermediate.tb = DDS_DynamicTypeBuilderFactory_create_type(desc);
            qeo_log_dds_null("DDS_DynamicTypeBuilderFactory_create_type", type->u.intermediate.tb);
            if ((NULL != type->u.intermediate.tb) && (QEOCORE_TYPECODE_STRUCT == tc)) {
                /* make structure types mutable (member order is not relevant, but member id is) */
                rc = set_ext_annotation(type->u.intermediate.tb, "MUTABLE_EXTENSIBILITY");
            }
            if ((NULL == type->u.intermediate.tb) || (QEO_OK != rc)) {
                free(type);
                type = NULL;
            }
            DDS_TypeDescriptor__free(desc);
        }
        else {
            free(type);
            type = NULL;
        }
    }
    return type;
}

/* ===[ public ]============================================================= */

qeocore_type_t *qeocore_type_struct_new(const char *name)
{
    qeocore_type_t *type = NULL;

    if (NULL != name) {
        type = type_new(name, QEOCORE_TYPECODE_STRUCT);
    }
    return type;
}

qeo_retcode_t qeocore_type_struct_add(qeocore_type_t *container,
                                  qeocore_type_t *member,
                                  const char *name,
                                  qeocore_member_id_t *id,
                                  unsigned int flags)
{
    qeo_retcode_t rc = QEO_OK;
    DDS_DynamicType dtype = NULL;

    VALIDATE_NON_FINAL(container);
    VALIDATE_NON_NULL(member);
    VALIDATE_NON_NULL(name);
    VALIDATE_NON_NULL(id);
    if (QEOCORE_TYPECODE_STRUCT != container->u.intermediate.tc) {
        rc = QEO_EINVAL;
    }
    else {
        if (member->flags.final) {
            dtype = member->u.dynamic.dtype;
        }
        else {
            if (QEOCORE_TYPECODE_STRUCT == member->u.intermediate.tc) {
                /* adding nested struct, first build it */
                rc = type_build_dynamic(NULL, member, name, 0);
            }
            dtype = member->u.intermediate.dtype;
        }
        if (QEO_OK == rc) {
            if (NULL == dtype) {
                rc = QEO_EINVAL;
            }
            else {
                rc = member_add(container, dtype, name, id, flags);
            }
        }
    }
    return rc;
}

qeocore_type_t *qeocore_type_string_new(size_t sz)
{
    qeocore_type_t *type = NULL;

    type = calloc(1, sizeof(qeocore_type_t));
    if (NULL != type) {
        type->u.intermediate.tc = QEOCORE_TYPECODE_STRING;
        type->u.intermediate.dtype = build_string(sz);
        if (NULL == type->u.intermediate.dtype) {
            free(type);
            type = NULL;
        }
    }
    return type;
}

qeocore_type_t *qeocore_type_primitive_new(qeocore_typecode_t tc)
{
    qeocore_type_t *type = NULL;

    if (tc < QEOCORE_TYPECODE_STRING) {
        /* only primitive types */
        type = calloc(1, sizeof(qeocore_type_t));
        if (NULL != type) {
            type->u.intermediate.tc = tc;
            type->u.intermediate.dtype = build_primitive(tc);
            if (NULL == type->u.intermediate.dtype) {
                free(type);
                type = NULL;
            }
        }
    }
    return type;
}

qeocore_type_t *qeocore_type_sequence_new(qeocore_type_t *elem_type)
{
    qeocore_type_t *type = NULL;

    if (NULL != elem_type) {
        DDS_DynamicType dtype = NULL;

        if (elem_type->flags.final) {
            dtype = elem_type->u.dynamic.dtype;
        }
        else {
            qeo_retcode_t rc = QEO_OK;

            if (QEOCORE_TYPECODE_STRUCT == elem_type->u.intermediate.tc) {
                /* sequence of struct, first build it */
                rc = type_build_dynamic(NULL, elem_type, NULL, 0);
                if (QEO_OK == rc) {
                    dtype = elem_type->u.dynamic.dtype;
                }
            }
            else {
                dtype = elem_type->u.intermediate.dtype;
            }
        }
        if (NULL != dtype) {
            type = calloc(1, sizeof(qeocore_type_t));
            if (NULL != type) {
                type->u.intermediate.tc = QEOCORE_TYPECODE_SEQUENCE;
                type->u.intermediate.dtype = build_sequence(dtype);
                if (NULL == type->u.intermediate.dtype) {
                    free(type);
                    type = NULL;
                }
            }
        }
    }
    return type;
}

qeocore_type_t *qeocore_type_enum_new(const char *name,
                                      const qeocore_enum_constants_t *values)
{
    qeocore_type_t *type = NULL;

    if ((NULL != name) && (NULL != values) && (0 != DDS_SEQ_LENGTH(*values))) {
        qeocore_type_t *val = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32);

        if (NULL != val) {
            type = type_new(name, QEOCORE_TYPECODE_ENUM);
            if (NULL != type ) {
                qeocore_enum_constant_t *enumerator;
                qeo_retcode_t rc = QEO_OK;
                int i;

                DDS_SEQ_FOREACH_ENTRY(*values, i, enumerator) {
                    rc = member_add(type, val->u.intermediate.dtype, enumerator->name, NULL, QEOCORE_FLAG_NONE);
                    if (QEO_OK != rc) {
                        break;
                    }
                }
                if (QEO_OK == rc) {
                    rc = type_build_dynamic(NULL, type, NULL, 0);
                }
                if (QEO_OK != rc) {
                    qeocore_type_free(type);
                    type = NULL;
                }
            }
            qeocore_type_free(val);
        }
    }
    return type;
}

static qeo_retcode_t string_copy(char *dst, size_t sz, const char *src)
{
    qeo_retcode_t rc = QEO_OK;

    if (strlen(src) < sz) {
        strcpy(dst, src);
    }
    else {
        rc = QEO_ENOMEM;
    }
    return rc;
}

qeo_retcode_t qeocore_enum_value_to_string(const DDS_TypeSupport_meta *enum_tsm,
                                           const qeocore_type_t *enum_type,
                                           qeo_enum_value_t value,
                                           char *name,
                                           size_t sz)
{
    qeo_retcode_t rc = QEO_EINVAL;

    VALIDATE_NON_NULL(name);
    if ((NULL != enum_tsm) && (NULL == enum_type)) {
        if (CDR_TYPECODE_ENUM == enum_tsm->tc) {
            int i = 0;

            for (i = 1; i <= enum_tsm->nelem; i++) {
                if (value == enum_tsm[i].label) {
                    rc = string_copy(name, sz, enum_tsm[i].name);
                    break;
                }
            }
        }
    }
    else if ((NULL != enum_type) && (NULL == enum_tsm)) {
        if (enum_type->flags.final && (DDS_ENUMERATION_TYPE == DDS_DynamicType_get_kind(enum_type->u.dynamic.dtype))) {
            DDS_DynamicTypeMembersById members;
            DDS_ReturnCode_t ddsrc;

            ddsrc = DDS_DynamicType_get_all_members(enum_type->u.dynamic.dtype, &members);
            qeo_log_dds_rc("DDS_DynamicType_get_all_members", ddsrc);
            if (DDS_RETCODE_OK == ddsrc) {
                MapEntry_DDS_MemberId_DDS_DynamicTypeMember *elem;
                int i;

                DDS_SEQ_FOREACH_ENTRY(members, i, elem) {
                    if (value == DDS_DynamicTypeMember_get_id(elem->value)) {
                        char *p = DDS_DynamicTypeMember_get_name(elem->value);
                        rc = string_copy(name, sz, p);
                        free(p);
                        break;
                    }
                }
                DDS_DynamicTypeMembersById__clear (&members);
            }
        }
    }
    return rc;
}

qeo_retcode_t qeocore_enum_string_to_value(const DDS_TypeSupport_meta *enum_tsm,
                                           const qeocore_type_t *enum_type,
                                           const char *name,
                                           qeo_enum_value_t *value)
{
    qeo_retcode_t rc = QEO_EINVAL;

    VALIDATE_NON_NULL(name);
    VALIDATE_NON_NULL(value);
    if ((NULL != enum_tsm) && (NULL == enum_type)) {
        if (CDR_TYPECODE_ENUM == enum_tsm->tc) {
            int i;

            for (i = 1; i <= enum_tsm->nelem; i++) {
                if (0 == strcmp(name, enum_tsm[i].name)) {
                    *value = enum_tsm[i].label;
                    rc = QEO_OK;
                    break;
                }
            }
        }
    }
    else if ((NULL != enum_type) && (NULL == enum_tsm)) {
        if (enum_type->flags.final && (DDS_ENUMERATION_TYPE == DDS_DynamicType_get_kind(enum_type->u.dynamic.dtype))) {
            qeocore_member_id_t id;

            if (QEO_OK == qeocore_type_get_member_id(enum_type, name, &id)) {
                *value = (qeo_enum_value_t)id;
                rc = QEO_OK;
            }
        }
    }
    return rc;
}

qeocore_type_t *qeocore_type_register_tsm(const qeo_factory_t *factory,
                                          const DDS_TypeSupport_meta *tsm,
                                          const char *name)
{
    qeocore_type_t *type = NULL;

    if ((NULL != factory) && (NULL != tsm) && (NULL != name)) {
        type = type_build_tsm(factory, tsm, name);
    }
    return type;
}

qeo_retcode_t qeocore_type_use(qeocore_type_t *type)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_FINAL(type);
    LOCK;
    if (type->flags.refcnt < TYPE_REFCNT_MAX) {
        type->flags.refcnt++;
        qeo_log_d_refcnt("type %s refcnt++ -> %d",
                         type->flags.tsm_based ? type->u.tsm_based.name : type->u.dynamic.name,
                         type->flags.refcnt);
    }
    else {
        qeo_log_e("type %s refcnt too large",
                  type->flags.tsm_based ? type->u.tsm_based.name : type->u.dynamic.name);
        rc = QEO_ENOMEM;
    }
    UNLOCK;
    return rc;
}

static qeo_retcode_t qeocore_type_unuse(qeocore_type_t *type)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_FINAL(type);
    LOCK;
    type->flags.refcnt--;
    qeo_log_d_refcnt("type %s refcnt-- -> %d",
                     type->flags.tsm_based ? type->u.tsm_based.name : type->u.dynamic.name,
                     type->flags.refcnt);
    if (0 == type->flags.refcnt) {
        type_free(type);
    }
    UNLOCK;
    return rc;
}

qeo_retcode_t qeocore_type_register(const qeo_factory_t *factory,
                                    qeocore_type_t *type,
                                    const char *name)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_NON_NULL(factory);
    VALIDATE_NON_FINAL(type);
    VALIDATE_NON_NULL(name);
    if (QEOCORE_TYPECODE_STRUCT != type->u.intermediate.tc) {
        rc = QEO_EINVAL;
    }
    else {
        rc = type_register(factory, type, name);
    }
    return rc;
}

qeo_retcode_t qeocore_type_get_member_id(const qeocore_type_t *type,
                                         const char *name,
                                         qeocore_member_id_t *id)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_FINAL(type);
    VALIDATE_NON_NULL(name);
    VALIDATE_NON_NULL(id);
    if (type->flags.tsm_based) {
        rc = QEO_EINVAL;
    }
    else {
        DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;
        DDS_DynamicTypeMember member = DDS_DynamicTypeMember__alloc();

        ddsrc = DDS_DynamicType_get_member_by_name(type->u.dynamic.dtype, member, name);
        qeo_log_dds_rc("DDS_DynamicType_get_member_by_name", ddsrc);
        if (DDS_RETCODE_OK == ddsrc) {
            *id = DDS_DynamicTypeMember_get_id(member);
        }
        DDS_DynamicTypeMember__free(member);
        rc = ddsrc_to_qeorc(ddsrc);
    }
    return rc;
}

void qeocore_type_free(qeocore_type_t *type)
{
    if (NULL != type) {
        if (type->flags.final && type->flags.registered) {
            qeocore_type_unuse(type);
        }
        else {
            type_free(type);
        }
    }
}
