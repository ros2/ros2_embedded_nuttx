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

#include <qeocore/dyntype.h>

#include "tsm_types.h"
#include "dyn_types.h"

qeocore_member_id_t _member_id[TYPES_NELEM + INNER_NELEM];

static qeocore_type_t *types_get_r(const DDS_TypeSupport_meta **tsm,
                                   int *member_idx)
{
    qeocore_type_t *type = NULL;

    switch ((*tsm)->tc) {
        case CDR_TYPECODE_CSTRING:
            assert(NULL != (type = qeocore_type_string_new(0)));
            break;
        case CDR_TYPECODE_OCTET:
            assert(NULL != (type = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8)));
            break;
        case CDR_TYPECODE_SHORT:
            assert(NULL != (type = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT16)));
            break;
        case CDR_TYPECODE_LONG:
            assert(NULL != (type = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
            break;
        case CDR_TYPECODE_LONGLONG:
            assert(NULL != (type = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT64)));
            break;
        case CDR_TYPECODE_FLOAT:
            assert(NULL != (type = qeocore_type_primitive_new(QEOCORE_TYPECODE_FLOAT32)));
            break;
        case CDR_TYPECODE_BOOLEAN:
            assert(NULL != (type = qeocore_type_primitive_new(QEOCORE_TYPECODE_BOOLEAN)));
            break;
        case CDR_TYPECODE_SEQUENCE: {
            qeocore_type_t *elem = NULL;

            (*tsm)++; /* skip over sequence container for now */
            assert(NULL != (elem = types_get_r(tsm, member_idx)));
            (*tsm)--; /* go back because real ++ at end of this function */
            assert(NULL != (type = qeocore_type_sequence_new(elem)));
            qeocore_type_free(elem);
            break;
        }
        case CDR_TYPECODE_TYPEREF: {
            const DDS_TypeSupport_meta *nested = (*tsm)->tsm;

            assert(NULL != (type = types_get_r(&nested, member_idx)));
            break;
        }
        case CDR_TYPECODE_STRUCT: {
            unsigned int nelem = (*tsm)->nelem;
            int i;

            assert(NULL != (type = qeocore_type_struct_new((*tsm)->name)));
            (*tsm)++; /* skip over container for now */
            for (i = 0; i < nelem; i++) {
                unsigned int flags = (*tsm)->flags & TSMFLAG_KEY ? QEOCORE_FLAG_KEY : QEOCORE_FLAG_NONE;
                const char *name = (*tsm)->name;
                qeocore_type_t *member = NULL;

                assert(NULL != (member = types_get_r(tsm, member_idx)));
                _member_id[*member_idx] = QEOCORE_MEMBER_ID_DEFAULT;
                assert(QEO_OK == qeocore_type_struct_add(type, member, name, &_member_id[*member_idx], flags));
                (*member_idx)++;
                qeocore_type_free(member);
            }
            (*tsm)--; /* go back because real ++ at end of this function */
            break;
        }
        case CDR_TYPECODE_ENUM: {
            qeocore_enum_constants_t vals = DDS_SEQ_INITIALIZER(qeocore_enum_constant_t);
            const char *name = (*tsm)->name;
            unsigned int nelem = (*tsm)->nelem;
            int i;

            assert(DDS_RETCODE_OK == dds_seq_require(&vals, nelem));
            (*tsm)++; /* skip over container for now */
            for (i = 0; i < nelem; i++) {
                DDS_SEQ_ITEM(vals, i).name = (char*)(*tsm)->name;
                (*tsm)++;
            }
            assert(NULL != (type = qeocore_type_enum_new(name, &vals)));
            dds_seq_cleanup(&vals);
            (*tsm)--; /* go back because real ++ at end of this function */
            break;
        }
        default:
            break; /* not used for now */
    }
    (*tsm)++; /* skip over processed item */
    return type;
}

qeocore_type_t *types_get(const DDS_TypeSupport_meta *tsm)
{
    int member_idx = 0;

    return types_get_r(&tsm, &member_idx);
}

const char *_TC2STR[] = { "int8", "int16", "int32", "int64", "float32", "boolean", "struct", "string", "sequence" };

#define MEMBER_ID "id"
#define MEMBER_INNER "inner"
#define MEMBER_NUM "num"
#define MEMBER_SEQUENCE "sequence"

qeocore_member_id_t _id_id = QEOCORE_MEMBER_ID_DEFAULT;
qeocore_member_id_t _inner_id = QEOCORE_MEMBER_ID_DEFAULT;
qeocore_member_id_t _num_id = QEOCORE_MEMBER_ID_DEFAULT;
qeocore_member_id_t _sequence_id = QEOCORE_MEMBER_ID_DEFAULT;

qeocore_type_t *nested_type_get(int id_is_keyed, int inner_is_keyed, int num_is_keyed)
{
    qeocore_type_t *outer = NULL, *inner = NULL;
    qeocore_type_t *member = NULL;

    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(NULL != (outer = qeocore_type_struct_new("outer")));
    assert(QEO_OK == qeocore_type_struct_add(outer, member, MEMBER_ID, &_id_id, (id_is_keyed ? QEOCORE_FLAG_KEY : QEOCORE_FLAG_NONE)));
    assert(NULL != (inner = qeocore_type_struct_new("inner")));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, MEMBER_NUM, &_num_id, (num_is_keyed ? QEOCORE_FLAG_KEY : QEOCORE_FLAG_NONE)));
    qeocore_type_free(member);
    assert(QEO_OK == qeocore_type_struct_add(outer, inner, MEMBER_INNER, &_inner_id, (inner_is_keyed ? QEOCORE_FLAG_KEY : QEOCORE_FLAG_NONE)));
    qeocore_type_free(inner);
    return outer;
}

static qeocore_type_t *seq_new(qeo_factory_t *factory,
                           qeocore_typecode_t typecode,
                           int reg_struct)
{
    qeocore_type_t *sequence = NULL;
    qeocore_type_t *member = NULL;

    switch (typecode) {
        case QEOCORE_TYPECODE_INT8:
        case QEOCORE_TYPECODE_INT16:
        case QEOCORE_TYPECODE_INT32:
        case QEOCORE_TYPECODE_INT64:
        case QEOCORE_TYPECODE_FLOAT32:
        case QEOCORE_TYPECODE_BOOLEAN:
            assert(NULL != (member = qeocore_type_primitive_new(typecode)));
            break;
        case QEOCORE_TYPECODE_STRING:
            assert(NULL != (member = qeocore_type_string_new(0)));
            break;
        case QEOCORE_TYPECODE_SEQUENCE:
            assert(NULL != (member = seq_new(factory, QEOCORE_TYPECODE_INT32, reg_struct)));
            break;
        case QEOCORE_TYPECODE_STRUCT: {
            qeocore_type_t *inner = NULL;
            qeocore_type_t *num = NULL;

            assert(NULL != (inner = qeocore_type_struct_new("inner")));
            assert(NULL != (num = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
            assert(QEO_OK == qeocore_type_struct_add(inner, num, MEMBER_NUM, &_num_id, QEOCORE_FLAG_NONE));
            if (reg_struct) {
                assert(QEO_OK == qeocore_type_register(factory, inner, "inner"));
            }
            qeocore_type_free(num);
            member = inner;
            break;
        }
        default:
            abort();
            break;
    }
    assert(NULL != (sequence = qeocore_type_sequence_new(member)));
    qeocore_type_free(member);
    return sequence;
}

qeocore_type_t *seq_type_get(qeo_factory_t *factory,
                         qeocore_typecode_t typecode,
                         int is_keyed,
                         int reg_struct)
{
    qeocore_type_t *outer = NULL;
    qeocore_type_t *sequence = NULL;

    assert(NULL != (outer = qeocore_type_struct_new("outer")));
    sequence = seq_new(factory, typecode, reg_struct);
    assert(QEO_OK == qeocore_type_struct_add(outer, sequence, MEMBER_SEQUENCE, &_sequence_id,
                                         (is_keyed ? QEOCORE_FLAG_KEY : QEOCORE_FLAG_NONE)));
    qeocore_type_free(sequence);
    return outer;
}

