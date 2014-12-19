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

#include <stdlib.h>
#include <string.h>

#include <dds/dds_xtypes.h>

#include <qeocore/api.h>

#include "core.h"
#include "core_util.h"

DDS_SEQUENCE(qeocore_data_t *, qeo_data_seq_t);

/* HACK: fix me by using DDS_DynamicType_ref(...) */
typedef struct DDS_DynamicType_st {
    unsigned    magic;      /* For verification purposes. */
    unsigned    nrefs;      /* # of references to dynamic type. */
    void  *domain;    /* Owner domain. */
    unsigned    id;     /* Type handle. */
} DynType_t;

static DDS_DynamicType HACK_inc_nrefs(DDS_DynamicType type)
{
    ((DynType_t *)(type))->nrefs++;
    return type;
}

static DDS_DynamicType sequence_element_type(DDS_DynamicType type)
{
    DDS_DynamicType elem_type = NULL;
    DDS_TypeDescriptor tdesc = { 0 };
    DDS_ReturnCode_t ddsrc;

    ddsrc = DDS_DynamicType_get_descriptor(type, &tdesc);
    qeo_log_dds_rc("DDS_DynamicType_get_descriptor", ddsrc);
    if (DDS_RETCODE_OK == ddsrc) {
        elem_type = HACK_inc_nrefs(tdesc.element_type);
        DDS_TypeDescriptor__clear(&tdesc);
    }
    return elem_type;
}

qeocore_data_t *data_alloc(DDS_DynamicType type,
                           int prep_data)
{
    qeocore_data_t *data;

    data = calloc(1, sizeof(qeocore_data_t));
    if (NULL != data) {
        data->flags.is_single = 1;
        data->d.dynamic.single_type = HACK_inc_nrefs(type);
        if (prep_data) {
            data->d.dynamic.single_data = DDS_DynamicDataFactory_create_data(data->d.dynamic.single_type);
            if (NULL == data->d.dynamic.single_data) {
                qeocore_data_free(data);
                data = NULL;
            }
        }
    }
    return data;
}

static size_t type_sizeof(DDS_DynamicType type)
{
    switch (DDS_DynamicType_get_kind(type)) {
        case DDS_BOOLEAN_TYPE:
            return sizeof(qeo_boolean_t);
        case DDS_BYTE_TYPE:
            return sizeof(int8_t);
        case DDS_INT_16_TYPE:
            return sizeof(int16_t);
        case DDS_INT_32_TYPE:
            return sizeof(int32_t);
        case DDS_INT_64_TYPE:
            return sizeof(int64_t);
        case DDS_FLOAT_32_TYPE:
            return sizeof(float);
        case DDS_ENUMERATION_TYPE:
            return sizeof(qeo_enum_value_t);
        case DDS_STRING_TYPE:
            return sizeof(char*);
        case DDS_SEQUENCE_TYPE:
        case DDS_STRUCTURE_TYPE:
            return sizeof(qeocore_data_t*);
        default:
            abort();
    }
}

static qeo_retcode_t sequence_prepare(qeo_sequence_t *sequence,
                                      DDS_DynamicType elem_type,
                                      int prep_data)
{
    qeo_retcode_t rc = QEO_OK;

    /* for non-primitive types create actual qeo_data_t structures */
    switch (DDS_DynamicType_get_kind(elem_type)) {
        case DDS_SEQUENCE_TYPE:
        case DDS_STRUCTURE_TYPE: {
            qeo_data_seq_t *dseq = (qeo_data_seq_t *)sequence;
            qeocore_data_t **data;
            unsigned int i;

            DDS_SEQ_FOREACH_ENTRY(*dseq, i, data) {
                *data = data_alloc(elem_type, prep_data);
                if (NULL == *data) {
                    rc = QEO_ENOMEM;
                    break;
                }
            }
            /* clean up on error */
            if (QEO_OK != rc) {
                DDS_SEQ_FOREACH_ENTRY(*dseq, i, data) {
                    if (NULL == *data) {
                        break;
                    }
                    qeocore_data_free(*data);
                }
            }
            break;
        }
        default:
            break;
    }
    return rc;
}

static qeo_retcode_t sequence_alloc(qeo_sequence_t *sequence,
                                    int offset,
                                    int num,
                                    size_t max_sz,
                                    DDS_DynamicType elem_type,
                                    int prepare_data)
{
    qeo_retcode_t rc = QEO_OK;
    unsigned esize = type_sizeof(elem_type);

    /* determine number of elements to be copied */
    if ((QEOCORE_SIZE_UNLIMITED == num) || (offset + num > (int)max_sz)) {
        num = max_sz - offset;
    }
    if (num < 0) {
        num = 0;
    }
    /* allocate and prepare */
    DDS_SEQ_INIT(*sequence);
    if (num > 0) {
        DDS_SEQ_DATA(*sequence) = calloc(num, esize);
        if (NULL == DDS_SEQ_DATA(*sequence)) {
            rc = QEO_ENOMEM;
        }
    }
    if (QEO_OK == rc) {
        DDS_SEQ_MAXIMUM(*sequence) = DDS_SEQ_LENGTH(*sequence) = num;
        DDS_SEQ_ELEM_SIZE(*sequence) = esize;
        if (num > 0) {
            rc = sequence_prepare(sequence, elem_type, prepare_data);
            if (QEO_OK != rc) {
                free(DDS_SEQ_DATA(*sequence));
                DDS_SEQ_DATA(*sequence) = NULL;
            }
        }
    }
    return rc;
}

static qeo_retcode_t sequence_free(qeo_sequence_t *sequence,
                                   DDS_DynamicType elem_type)
{
    qeo_retcode_t rc = QEO_OK;

    /* for non-primitive types create actual qeo_data_t structures */
    switch (DDS_DynamicType_get_kind(elem_type)) {
        case DDS_STRING_TYPE: {
            DDS_StringSeq *seq = (DDS_StringSeq *)sequence;
            char **data;
            unsigned int i;

            DDS_SEQ_FOREACH_ENTRY(*seq, i, data) {
                free(*data);
            }
            break;
        }
        case DDS_SEQUENCE_TYPE:
        case DDS_STRUCTURE_TYPE: {
            qeo_data_seq_t *seq = (qeo_data_seq_t *)sequence;
            qeocore_data_t **data;
            unsigned int i;

            DDS_SEQ_FOREACH_ENTRY(*seq, i, data) {
                qeocore_data_free(*data);
            }
            break;
        }
        default:
            /* nop */
            break;
    }
    free(DDS_SEQ_DATA(*sequence));
    return rc;
}

static qeo_retcode_t sequence_accessor(DDS_VoidPtrSeq *seq,
                                          DDS_DynamicData seqdata,
                                          DDS_DynamicType elem_type,
                                          int get)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;

    switch (DDS_DynamicType_get_kind(elem_type)) {
        case DDS_BOOLEAN_TYPE: /* QEOCORE_TYPECODE_BOOLEAN */
            if (get) {
                ddsrc = DDS_DynamicData_get_boolean_values(seqdata, (DDS_BooleanSeq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_boolean_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_boolean_values(seqdata, 0, (DDS_BooleanSeq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_boolean_values", ddsrc);
            }
            break;
        case DDS_BYTE_TYPE: /* QEOCORE_TYPECODE_INT8 */
            if (get) {
                ddsrc = DDS_DynamicData_get_byte_values(seqdata, (DDS_OctetSeq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_byte_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_byte_values(seqdata, 0, (DDS_OctetSeq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_byte_values", ddsrc);
            }
            break;
        case DDS_INT_16_TYPE: /* QEOCORE_TYPECODE_INT16 */
            if (get) {
                ddsrc = DDS_DynamicData_get_int16_values(seqdata, (DDS_Int16Seq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_int16_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_int16_values(seqdata, 0, (DDS_Int16Seq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_int16_values", ddsrc);
            }
            break;
        case DDS_INT_32_TYPE: /* QEOCORE_TYPECODE_INT32 */
        case DDS_ENUMERATION_TYPE: /* QEOCORE_TYPECODE_ENUM */
            if (get) {
                ddsrc = DDS_DynamicData_get_int32_values(seqdata, (DDS_Int32Seq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_int32_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_int32_values(seqdata, 0, (DDS_Int32Seq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_int32_values", ddsrc);
            }
            break;
        case DDS_INT_64_TYPE: /* QEOCORE_TYPECODE_INT64 */
            if (get) {
                ddsrc = DDS_DynamicData_get_int64_values(seqdata, (DDS_Int64Seq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_int64_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_int64_values(seqdata, 0, (DDS_Int64Seq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_int64_values", ddsrc);
            }
            break;
        case DDS_FLOAT_32_TYPE: /* QEOCORE_TYPECODE_FLOAT32 */
            if (get) {
                ddsrc = DDS_DynamicData_get_float32_values(seqdata, (DDS_Float32Seq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_float32_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_float32_values(seqdata, 0, (DDS_Float32Seq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_float32_values", ddsrc);
            }
            break;
        case DDS_STRING_TYPE: /* QEOCORE_TYPECODE_STRING */
            if (get) {
                ddsrc = DDS_DynamicData_get_string_values(seqdata, (DDS_StringSeq *)seq, 0);
                qeo_log_dds_rc("DDS_DynamicData_get_string_values", ddsrc);
            }
            else {
                ddsrc = DDS_DynamicData_set_string_values(seqdata, 0, (DDS_StringSeq *)seq);
                qeo_log_dds_rc("DDS_DynamicData_set_string_values", ddsrc);
            }
            break;
        case DDS_SEQUENCE_TYPE: /* QEOCORE_TYPECODE_ARRAY */
        case DDS_STRUCTURE_TYPE: /* QEOCORE_TYPECODE_STRUCT */
            /* should never happen: not handled here */
            abort();
            break;
        default:
            qeo_log_e("unsupported type %d", DDS_DynamicType_get_kind(elem_type));
            abort(); // unsupported for now
            break;
    }
    return ddsrc_to_qeorc(ddsrc);
}

/* ===[ public ]============================================================= */

// TODO validate that data actually contains a sequence
qeo_retcode_t qeocore_data_sequence_new(const qeocore_data_t *data,
                                        qeo_sequence_t *sequence,
                                        int num)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != data) && (NULL != sequence)) {
        DDS_DynamicType elem_type = sequence_element_type(data->d.dynamic.single_type);

        if (NULL != elem_type) {
            rc = sequence_alloc(sequence, 0, num, QEOCORE_SIZE_UNLIMITED, elem_type, 1);
            DDS_DynamicTypeBuilderFactory_delete_type(elem_type);
        }
    }
    return rc;
}

qeo_retcode_t qeocore_data_sequence_free(const qeocore_data_t *data,
                                         qeo_sequence_t *sequence)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != data) && (NULL != sequence)) {
        DDS_DynamicType elem_type = sequence_element_type(data->d.dynamic.single_type);

        if (NULL != elem_type) {
            rc = sequence_free(sequence, elem_type);
            DDS_DynamicTypeBuilderFactory_delete_type(elem_type);
        }
    }
    return rc;
}

qeo_retcode_t qeocore_data_sequence_set(qeocore_data_t *data,
                                        const qeo_sequence_t *value,
                                        int offset)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != data) && (NULL != value)) {
        DDS_DynamicType elem_type = sequence_element_type(data->d.dynamic.single_type);

        if (NULL != elem_type) {
            DDS_TypeKind kind = DDS_DynamicType_get_kind(elem_type);

            rc = QEO_OK;
            switch (kind) {
                case DDS_BOOLEAN_TYPE: /* QEOCORE_TYPECODE_BOOLEAN */
                case DDS_BYTE_TYPE: /* QEOCORE_TYPECODE_INT8 */
                case DDS_INT_16_TYPE: /* QEOCORE_TYPECODE_INT16 */
                case DDS_INT_32_TYPE: /* QEOCORE_TYPECODE_INT32 */
                case DDS_INT_64_TYPE: /* QEOCORE_TYPECODE_INT64 */
                case DDS_FLOAT_32_TYPE:  /* QEOCORE_TYPECODE_FLOAT32 */
                case DDS_ENUMERATION_TYPE: /* QEOCORE_TYPECODE_ENUM */
                case DDS_STRING_TYPE: { /* QEOCORE_TYPECODE_STRING */
                    rc = sequence_accessor((DDS_VoidPtrSeq *)value, data->d.dynamic.single_data, elem_type, 0);
                    break;
                }
                case DDS_SEQUENCE_TYPE: /* QEOCORE_TYPECODE_ARRAY */
                case DDS_STRUCTURE_TYPE: { /* QEOCORE_TYPECODE_STRUCT */
                    qeo_data_seq_t *dseq = (qeo_data_seq_t *)value;
                    qeocore_data_t **elem;
                    unsigned int i;

                    DDS_SEQ_FOREACH_ENTRY(*dseq, i, elem) {
                        DDS_ReturnCode_t ddsrc;

                        ddsrc = DDS_DynamicData_set_complex_value(data->d.dynamic.single_data, offset + i,
                                                                  (*elem)->d.dynamic.single_data);
                        qeo_log_dds_rc("DDS_DynamicData_set_complex_value", ddsrc);
                        if (DDS_RETCODE_OK != ddsrc) {
                            rc = ddsrc_to_qeorc(ddsrc);
                            break;
                        }
                    }
                    break;
                }
                default:
                    qeo_log_e("unsupported type %d", kind);
                    abort(); // unsupported for now
                    break;
            }
            DDS_DynamicTypeBuilderFactory_delete_type(elem_type);
        }
    }
    return rc;
}

qeo_retcode_t qeocore_data_sequence_get(const qeocore_data_t *data,
                                        qeo_sequence_t *value,
                                        int offset,
                                        int num)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != data) && (NULL != value)) {
        DDS_DynamicType elem_type = sequence_element_type(data->d.dynamic.single_type);

        if (NULL != elem_type) {
            DDS_TypeKind kind = DDS_DynamicType_get_kind(elem_type);
            size_t max_sz = DDS_DynamicData_get_item_count(data->d.dynamic.single_data);

            rc = sequence_alloc(value, offset, num, max_sz, elem_type, 0);
            if ((QEO_OK == rc) && (DDS_SEQ_LENGTH(*value) > 0)) {
                switch (kind) {
                    case DDS_BOOLEAN_TYPE: /* QEOCORE_TYPECODE_BOOLEAN */
                    case DDS_BYTE_TYPE: /* QEOCORE_TYPECODE_INT8 */
                    case DDS_INT_16_TYPE: /* QEOCORE_TYPECODE_INT16 */
                    case DDS_INT_32_TYPE: /* QEOCORE_TYPECODE_INT32 */
                    case DDS_INT_64_TYPE: /* QEOCORE_TYPECODE_INT64 */
                    case DDS_FLOAT_32_TYPE:  /* QEOCORE_TYPECODE_FLOAT32 */
                    case DDS_ENUMERATION_TYPE: /* QEOCORE_TYPECODE_ENUM */
                    case DDS_STRING_TYPE: { /* QEOCORE_TYPECODE_STRING */
                        DDS_VoidPtrSeq seq;

                        DDS_SEQ_INIT(seq);
                        DDS_SEQ_ELEM_SIZE(seq) = DDS_SEQ_ELEM_SIZE(*value);
                        rc = sequence_accessor(&seq, data->d.dynamic.single_data, elem_type, 1);
                        if (QEO_OK == rc) {
                            if (DDS_STRING_TYPE == kind) {
                                unsigned int i;

                                for (i = offset; i < offset + DDS_SEQ_LENGTH(*value); i++) {
                                    const char *str = DDS_SEQ_ITEM(seq, i);

                                    *DDS_SEQ_ITEM_PTR(*(DDS_StringSeq *)value, i - offset) = (NULL == str ? NULL
                                                                                                          : strdup(str));
                                }
                            }
                            else {
                                memcpy(DDS_SEQ_DATA(*value), &DDS_SEQ_ITEM(seq, offset),
                                       DDS_SEQ_LENGTH(*value) * DDS_SEQ_ELEM_SIZE(*value));
                            }
                        }
                        dds_seq_cleanup(&seq);
                        break;
                    }
                    case DDS_SEQUENCE_TYPE: /* QEOCORE_TYPECODE_ARRAY */
                    case DDS_STRUCTURE_TYPE: { /* QEOCORE_TYPECODE_STRUCT */
                        unsigned int i;

                        for (i = offset; i < offset + DDS_SEQ_LENGTH(*value); i++) {
                            qeocore_data_t *elem = DDS_SEQ_ITEM(*(DDS_VoidPtrSeq *)value, i - offset);
                            DDS_ReturnCode_t ddsrc;

                            ddsrc = DDS_DynamicData_get_complex_value(data->d.dynamic.single_data,
                                                                      &elem->d.dynamic.single_data, i);
                            qeo_log_dds_rc("DDS_DynamicData_get_complex_value", ddsrc);
                            if (DDS_RETCODE_OK != ddsrc) {
                                rc = ddsrc_to_qeorc(ddsrc);
                                break;
                            }
                        }
                        break;
                    }
                    default:
                        qeo_log_e("unsupported type %d", kind);
                        abort(); // unsupported for now
                        break;
                }
            }
            DDS_DynamicTypeBuilderFactory_delete_type(elem_type);
        }
    }
    return rc;
}
