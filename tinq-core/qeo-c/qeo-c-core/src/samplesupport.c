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
#include <stdio.h>

#include "core.h"
#include "samplesupport.h"
#include "typesupport.h"
#include "core_util.h"

static DDS_ReturnCode_t struct_get(qeocore_data_t **data,
                                   const DDS_DynamicData dyndata,
                                   qeocore_member_id_t id,
                                   DDS_MemberDescriptor *mdesc)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OUT_OF_RESOURCES;

    *data = calloc(1, sizeof(qeocore_data_t));
    if (NULL != *data) {
        (*data)->flags.is_single = 1;
        ddsrc = DDS_DynamicData_get_complex_value(dyndata, &(*data)->d.dynamic.single_data, id);
        if (DDS_RETCODE_NO_DATA == ddsrc) {
            /* no nested structure there yet, create empty one */
            (*data)->d.dynamic.single_data = DDS_DynamicDataFactory_create_data(mdesc->type);
            qeo_log_dds_null("DDS_DynamicDataFactory_create_data", (*data)->d.dynamic.single_data);
            if (NULL != (*data)->d.dynamic.single_data) {
                ddsrc = DDS_RETCODE_OK;
            }
        }
        else {
            qeo_log_dds_rc("DDS_DynamicData_get_complex_value", ddsrc);
        }
    }
    if (DDS_RETCODE_OK == ddsrc) {
        /* take ownership of type for future use */
        (*data)->d.dynamic.single_type = mdesc->type;
        mdesc->type = NULL;
    }
    else {
        if (NULL != *data) {
            if (NULL != (*data)->d.dynamic.single_data) {
                DDS_DynamicDataFactory_delete_data((*data)->d.dynamic.single_data);
            }
            free(*data);
        }
    }
    return ddsrc;
}

static DDS_ReturnCode_t struct_set(qeocore_data_t **data, const DDS_DynamicData dyndata, qeocore_member_id_t id)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OUT_OF_RESOURCES;

    if ((*data != NULL )&& ((*data)->d.dynamic.single_data != NULL)){
        ddsrc = DDS_DynamicData_set_complex_value(dyndata, id, (*data)->d.dynamic.single_data);
        qeo_log_dds_rc("DDS_DynamicData_set_complex_value", ddsrc);
    }
    return ddsrc;
}

static DDS_ReturnCode_t sequence_get(qeocore_data_t **value,
                                     const DDS_DynamicData dyndata,
                                     qeocore_member_id_t id,
                                     DDS_DynamicType type)
{
    DDS_ReturnCode_t ddsrc;

    *value = data_alloc(type, 1);
    if (NULL == *value) {
        ddsrc = DDS_RETCODE_OUT_OF_RESOURCES;
    }
    else {
        DDS_DynamicData seqdata;

        ddsrc = DDS_DynamicData_get_complex_value(dyndata, &seqdata, id);
        if (DDS_RETCODE_OK == ddsrc) {
            /* actual data was returned */
            DDS_DynamicDataFactory_delete_data((*value)->d.dynamic.single_data);
            (*value)->d.dynamic.single_data = seqdata;
        }
        /* in case of DDS_RETCODE_NO_DATA we keep the preallocated data */
        else if (DDS_RETCODE_NO_DATA != ddsrc) {
            qeo_log_dds_rc("DDS_DynamicData_get_complex_value", ddsrc);
        }
    }
    return ddsrc;
}

static DDS_ReturnCode_t sequence_set(const qeocore_data_t **value,
                                     DDS_DynamicData dyndata,
                                     qeocore_member_id_t id)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_ERROR;

    ddsrc = DDS_DynamicData_set_complex_value(dyndata, id, (*value)->d.dynamic.single_data);
    qeo_log_dds_rc("DDS_DynamicData_set_complex_value", ddsrc);
    return ddsrc;
}

static qeo_retcode_t data_member_accessor(qeocore_data_t *data,
                                          qeocore_member_id_t id,
                                          void *value,
                                          int get)
{
    qeo_retcode_t rc = QEO_EINVAL;
    DDS_ReturnCode_t ddsrc;
    DDS_DynamicTypeMember mtype = DDS_DynamicTypeMember__alloc();
    DDS_DynamicType dyntype = dtype_from_data(data, &data->d.dynamic);
    DDS_DynamicData dyndata = (DDS_DynamicData)sample_from_data(data);

    if (NULL != mtype) {
        ddsrc = DDS_DynamicType_get_member(dyntype, mtype, id);
        qeo_log_dds_rc("DDS_DynamicType_get_member", ddsrc);
        if (DDS_RETCODE_OK == ddsrc) {
            DDS_ReturnCode_t ddsrc = DDS_RETCODE_BAD_PARAMETER;
            DDS_MemberDescriptor mdesc = { 0 };
            DDS_TypeDescriptor tdesc = { 0 };

            ddsrc = DDS_DynamicTypeMember_get_descriptor(mtype, &mdesc);
            qeo_log_dds_rc("DDS_DynamicTypeMember_get_descriptor", ddsrc);
            ddsrc = DDS_DynamicType_get_descriptor(mdesc.type, &tdesc);
            qeo_log_dds_rc("DDS_DynamicType_get_descriptor", ddsrc);
            switch (tdesc.kind) {
                case DDS_BOOLEAN_TYPE: {
                    if (get) {
                        ddsrc = DDS_DynamicData_get_boolean_value(dyndata, (qeo_boolean_t *)value, id);
                        qeo_log_dds_rc("DDS_DynamicData_get_boolean_value", ddsrc);
                    }
                    else {
                        ddsrc = DDS_DynamicData_set_boolean_value(dyndata, id, *((qeo_boolean_t *)value));
                        qeo_log_dds_rc("DDS_DynamicData_set_boolean_value", ddsrc);
                    }
                    break;
                }
                case DDS_BYTE_TYPE: /* QEOCORE_TYPECODE_INT8 */
                    if (get) {
                        ddsrc = DDS_DynamicData_get_byte_value(dyndata, (unsigned char *)value, id);
                        qeo_log_dds_rc("DDS_DynamicData_get_byte_value", ddsrc);
                    }
                    else {
                        ddsrc = DDS_DynamicData_set_byte_value(dyndata, id, *((unsigned char *)value));
                        qeo_log_dds_rc("DDS_DynamicData_set_byte_value", ddsrc);
                    }
                    break;
                case DDS_INT_16_TYPE: /* QEOCORE_TYPECODE_INT16 */
                    if (get) {
                        ddsrc = DDS_DynamicData_get_int16_value(dyndata, (int16_t *)value, id);
                        qeo_log_dds_rc("DDS_DynamicData_get_int16_value", ddsrc);
                    }
                    else {
                        ddsrc = DDS_DynamicData_set_int16_value(dyndata, id, *((int16_t *)value));
                        qeo_log_dds_rc("DDS_DynamicData_set_int16_value", ddsrc);
                    }
                    break;
                case DDS_INT_32_TYPE: /* QEOCORE_TYPECODE_INT32 */
                case DDS_ENUMERATION_TYPE: /* QEOCORE_TYPECODE_ENUM */
                    if (get) {
                        ddsrc = DDS_DynamicData_get_int32_value(dyndata, (int32_t *)value, id);
                        qeo_log_dds_rc("DDS_DynamicData_get_int32_value", ddsrc);
                    }
                    else {
                        ddsrc = DDS_DynamicData_set_int32_value(dyndata, id, *((int32_t *)value));
                        qeo_log_dds_rc("DDS_DynamicData_set_int32_value", ddsrc);
                    }
                    break;
                case DDS_INT_64_TYPE: /* QEOCORE_TYPECODE_INT64 */
                    if (get) {
                        ddsrc = DDS_DynamicData_get_int64_value(dyndata, (int64_t *)value, id);
                        qeo_log_dds_rc("DDS_DynamicData_get_int64_value", ddsrc);
                    }
                    else {
                        ddsrc = DDS_DynamicData_set_int64_value(dyndata, id, *((int64_t *)value));
                        qeo_log_dds_rc("DDS_DynamicData_set_int64_value", ddsrc);
                    }
                    break;
                case DDS_FLOAT_32_TYPE: /* QEOCORE_TYPECODE_FLOAT32 */
                    if (get) {
                        ddsrc = DDS_DynamicData_get_float32_value(dyndata, (float *)value, id);
                        qeo_log_dds_rc("DDS_DynamicData_get_float32_value", ddsrc);
                    }
                    else {
                        ddsrc = DDS_DynamicData_set_float32_value(dyndata, id, *((float *)value));
                        qeo_log_dds_rc("DDS_DynamicData_set_float32_value", ddsrc);
                    }
                    break;
                case DDS_STRING_TYPE: /* QEOCORE_TYPECODE_STRING */
                    if (get) {
                        char **string = (char **)value;
                        int len = DDS_DynamicData_get_string_length(dyndata, id);

                        if (len > -1) {
                            *string = malloc(len + 1);
                            if (NULL != *string) {
                                ddsrc = DDS_DynamicData_get_string_value(dyndata, *string, id);
                                qeo_log_dds_rc("DDS_DynamicData_get_string_value", ddsrc);
                                if (DDS_RETCODE_OK != ddsrc) {
                                    free(*string);
                                    *string = NULL;
                                }
                            }
                        }
                    }
                    else {
                        char **string = (char **)value;

                        ddsrc = DDS_DynamicData_set_string_value(dyndata, id, *string);
                        qeo_log_dds_rc("DDS_DynamicData_set_string_value", ddsrc);
                    }
                    break;
                case DDS_SEQUENCE_TYPE: { /* QEOCORE_TYPECODE_SEQUENCE */
                    if (get) {
                        ddsrc = sequence_get(value, dyndata, id, mdesc.type);
                    }
                    else {
                        ddsrc = sequence_set(value, dyndata, id);
                    }
                    break;
                }
                case DDS_STRUCTURE_TYPE: { /* QEOCORE_TYPECODE_STRUCT */
                    qeocore_data_t **inner_data = (qeocore_data_t **)value;

                    if (get) {
                        ddsrc = struct_get(inner_data, dyndata, id, &mdesc);
                    }
                    else {
                        ddsrc = struct_set(inner_data, dyndata, id);
                    }
                    break;
                }
                default:
                    qeo_log_e("unsupported type %d", tdesc.kind);
                    abort(); // unsupported for now
                    break;
            }
            DDS_MemberDescriptor__clear(&mdesc);
            DDS_TypeDescriptor__clear(&tdesc);

            //TODO: Make the following code more generic and use the overall error translation function
            if ((DDS_RETCODE_OK == ddsrc) || (DDS_RETCODE_NO_DATA == ddsrc)){
                rc = QEO_OK;
            } else if (DDS_RETCODE_OUT_OF_RESOURCES == ddsrc) {
                rc = QEO_ENOMEM;
            } else {
                rc = QEO_EFAIL;
            }
        }
        DDS_DynamicTypeMember__free(mtype);
    }
    return rc;
}

/* ===[ public ]============================================================= */

qeo_retcode_t data_get_member(const qeocore_data_t *data, qeocore_member_id_t id, void *value)
{
    return data_member_accessor((qeocore_data_t *)data, id, value, 1);
}

qeo_retcode_t data_set_member(qeocore_data_t *data, qeocore_member_id_t id, const void *value)
{
    return data_member_accessor(data, id, (void *)value, 0);
}
