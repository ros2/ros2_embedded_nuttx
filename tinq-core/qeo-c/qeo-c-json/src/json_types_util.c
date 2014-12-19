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

/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json_types_util.h>
#include <assert.h>
#include <utlist.h>

#include <qeo/log.h>
#include "dds/dds_seq.h"


/*########################################################################
#                                                                       #
#  TYPES and DEFINES SECTION                                            #
#                                                                       #
########################################################################*/

#define KEY_TOPIC             "topic"
#define KEY_BEHAVIOR          "behavior"
#define KEY_PROPERTIES        "properties"
#define KEY_ITEMS             "items"
#define KEY_ITEM              "item"
#define KEY_ENUM              "enum"
#define KEY_VALUES            "values"
#define KEY_TYPE              "type"
#define KEY_KEY               "key"
#define KEY_ID                "id"

#define KEY_QEO_TYPE_CODE     "qeotypecode"



/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                              #
#                                                                       #
########################################################################*/

/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                           #
#                                                                       #
########################################################################*/

static qeo_retcode_t object_add_member(const qeo_factory_t  *factory,
                                       qeocore_type_t       *type,
                                       const char           *member_name,
                                       json_t               *member);
static qeocore_type_t *build_member_type(const qeo_factory_t  *factory,
                                         json_t               *member);
static qeocore_type_t *build_object(const qeo_factory_t *factory,
                                    json_t              *typedesc);
static qeo_retcode_t object_to_data(const json_t    *typedesc,
                                    json_t          *json_data,
                                    qeocore_data_t  *data);
static qeo_retcode_t member_to_data(json_t          *member,
                                    json_t          *json_data,
                                    qeocore_data_t  *data);
static qeo_retcode_t array_to_data(json_t         *elemtype,
                                   json_t         *json_data,
                                   qeocore_data_t *data);

static json_t *object_data_to_json(const json_t         *typedesc,
                                   const qeocore_data_t *data);
static json_t *member_data_to_json(const json_t         *member,
                                   const qeocore_data_t *data);
static json_t *array_data_to_json(const json_t    *typedesc,
                                  qeocore_data_t  *data);

static void find_and_replace(char *haystack,
                             char *needle,
                             char *replacement);

static int replace(char       *toreplace,
                   int        toreplace_len,
                   const char *replacement,
                   int        rep_len);

/*#######################################################################
 #                                                                       #
 # STATIC FUNCTION IMPLEMENTATION                                        #
 #                                                                       #
 ########################################################################*/

static qeo_retcode_t object_add_member(const qeo_factory_t *factory, qeocore_type_t *type, const char *member_name, json_t *member)
{
    qeo_retcode_t       result        = QEO_EFAIL;
    qeocore_type_t      *qeoType      = NULL;
    qeocore_member_id_t qeo_member_id = QEOCORE_MEMBER_ID_DEFAULT;
    json_t              *member_key   = json_object_get(member, KEY_KEY); // optional  => BOOLEAN
    json_t              *member_id    = json_object_get(member, KEY_ID);  // optional  => INTxx

    assert(factory != NULL);

    if (((NULL != member_key) && !json_is_boolean(member_key)) ||
        ((NULL != member_id) && !json_is_integer(member_id)) ||
        (NULL == member_name)) {
        // syntax error
        return QEO_EINVAL;
    }

    qeo_log_d("Processing %s", member_name);
    qeoType = build_member_type(factory, member);
    if (NULL == qeoType) {
        qeo_log_e("Could not build member_type");
        return result;
    }

    bool is_key = (member_key && json_is_true(member_key));
    do {
        result = qeocore_type_struct_add(type,                            // container
                qeoType,                         // new member to add
                member_name,                     // name of member
                &qeo_member_id,                  // member id
                is_key ? QEOCORE_FLAG_KEY : 0);  // flag
        if (QEO_OK != result) {
            qeo_log_e("qeocore_type_struct_add failed for member %s", member_name);
            break;
        }

        qeocore_type_free(qeoType);

        // Modify the json member to add/update the qeo member id
        json_object_set_new(member, KEY_ID, json_integer(qeo_member_id));
    } while (0);


    return result;
}

static qeocore_type_t *build_enum(const qeo_factory_t *factory, json_t *typedesc){

    qeocore_type_t    *qeoType  = NULL;
    qeocore_enum_constants_t vals = DDS_SEQ_INITIALIZER(qeocore_enum_constant_t);
    const char *enumstr = NULL;
    DDS_ReturnCode_t ddsret;

    assert(typedesc != NULL);
    assert(factory != NULL);

    do {
        json_t *type_enum = json_object_get(typedesc, KEY_ENUM);
        if ((NULL == type_enum) || !json_is_string(type_enum)) {
            qeo_log_e("Invalid type_enum (%p)", type_enum);
            return qeoType;
        }

        json_t *values = json_object_get(typedesc, KEY_VALUES);
        if ((NULL == values) || !json_is_object(values)) {
            qeo_log_e("Invalid values (%p)", values);
            return qeoType;
        }

        enumstr = json_string_value(type_enum);
        //Replace all "::" with ".", because there's a mismatch in topic definitions found in the TSM structs, with "." and the QDM topic definitions with "::"
        find_and_replace((char *) enumstr, "::", ".");

        if ((ddsret = dds_seq_require(&vals, json_object_size(values))) != DDS_RETCODE_OK){
            qeo_log_e("dds_seq_require failed (%d)", ddsret);
            return NULL;
        }

        void *iter = json_object_iter(values);
        while (iter) {
            json_int_t labelint;
            const char *name = json_object_iter_key(iter);
            json_t *label = json_object_iter_value(iter);
            if (!json_is_integer(label)){
                qeo_log_e("not a integer");
            }
            labelint = json_integer_value(label);
            if (labelint >= json_object_size(values)){
                qeo_log_e("Currently we only support 0,1,2..[n-1] as labels");     
                break;
            }

            DDS_SEQ_ITEM(vals, labelint).name = (char *)name;

            iter = json_object_iter_next(values, iter);
        }
    

        qeoType = qeocore_type_enum_new(enumstr, &vals);
        if (qeoType == NULL){
            qeo_log_e("Cannot register enum");
        }
        dds_seq_cleanup(&vals);

    } while(0);

    return qeoType;
}

static qeocore_type_t *build_member_type(const qeo_factory_t *factory, json_t *member)
{
    qeocore_type_t      *memberType = NULL;
    qeocore_typecode_t  qeoTypeCode;
    json_t              *member_type = json_object_get(member, KEY_TYPE);

    if (!((NULL != member_type) && (json_is_string(member_type)))) {
        qeo_log_e("Could not retrieve type");
        return memberType;
    }

    const char *jsonTypeCode = json_string_value(member_type);

    do {
        if (!strcmp(jsonTypeCode, "boolean")) {
            memberType  = qeocore_type_primitive_new(QEOCORE_TYPECODE_BOOLEAN);
            qeoTypeCode = QEOCORE_TYPECODE_BOOLEAN;
        }
        else if (!strcmp(jsonTypeCode, "byte")) {
            memberType  = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT8);
            qeoTypeCode = QEOCORE_TYPECODE_INT8;
        }
        else if (!strcmp(jsonTypeCode, "int16")) {
            memberType  = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT16);
            qeoTypeCode = QEOCORE_TYPECODE_INT16;
        }
        else if (!strcmp(jsonTypeCode, "int32")) {
            memberType  = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32);
            qeoTypeCode = QEOCORE_TYPECODE_INT32;
        }
        else if (!strcmp(jsonTypeCode, "int64")) {
            memberType  = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT64);
            qeoTypeCode = QEOCORE_TYPECODE_INT64;
        }
        else if (!strcmp(jsonTypeCode, "float32")) {
            memberType  = qeocore_type_primitive_new(QEOCORE_TYPECODE_FLOAT32);
            qeoTypeCode = QEOCORE_TYPECODE_FLOAT32;
        }
        else if (!strcmp(jsonTypeCode, "string")) {
            size_t size = 0;
            memberType  = qeocore_type_string_new(size);
            qeoTypeCode = QEOCORE_TYPECODE_STRING;
        }
        else if (!strcmp(jsonTypeCode, "object")) {
            json_t *item = json_object_get(member, KEY_ITEM);
            if ((NULL == item) || !json_is_object(item)) {
                qeo_log_e("Could not find item");
                break;
            }
            memberType  = build_object(factory, item);
            qeoTypeCode = QEOCORE_TYPECODE_STRUCT;
        }
        else if (!strcmp(jsonTypeCode, "array")) {
            json_t *items = json_object_get(member, KEY_ITEMS);
            if ((NULL == items) || !json_is_object(items)) {
                qeo_log_e("Could not find items");
                break;
            }

            qeocore_type_t *elemtype = build_member_type(factory, items);
            if (elemtype == NULL){
                qeo_log_e("Could not build member for array");
                break;
            }

            memberType  = qeocore_type_sequence_new(elemtype);
            qeocore_type_free(elemtype);
            qeoTypeCode = QEOCORE_TYPECODE_SEQUENCE;
        }
        else if (!strcmp(jsonTypeCode, "enum")){
            json_t *item = json_object_get(member, KEY_ITEM);
            if ((NULL == item) || !json_is_object(item)) {
                qeo_log_e("Could not find item");
                break;
            }
            memberType  = build_enum(factory, item);
            qeoTypeCode = QEOCORE_TYPECODE_ENUM;

        }
        else {
            qeo_log_e("Unsupported jsonTypeCode %s", jsonTypeCode);
        }
    } while (0);

    if (NULL == memberType) {
        qeo_log_e("Could not make type (%s)", jsonTypeCode);
        return memberType;
    }

    json_object_set_new(member, KEY_QEO_TYPE_CODE, json_integer(qeoTypeCode));

    return memberType;
}

static qeocore_type_t *build_object(const qeo_factory_t *factory, json_t *typedesc)
{
    qeocore_type_t    *qeoType  = NULL;
    const char        *name     = NULL;
    json_t            *value    = NULL;
    qeo_retcode_t     ret       = QEO_EFAIL;
    bool              iret      = true;
    const char        *topic    = NULL;

    assert(typedesc != NULL);
    assert(factory != NULL);

    do {
        json_t *type_topic = json_object_get(typedesc, KEY_TOPIC);
        if ((NULL == type_topic) || !json_is_string(type_topic)) {
            qeo_log_e("Invalid type_topic (%p)", type_topic);
            return qeoType;
        }

        json_t *properties = json_object_get(typedesc, KEY_PROPERTIES);
        if ((NULL == properties) || !json_is_object(properties)) {
            qeo_log_e("Invalid properties (%p)", properties);
            return qeoType;
        }

        topic = json_string_value(type_topic);
        //Replace all "::" with ".", because there's a mismatch in topic definitions found in the TSM structs, with "." and the QDM topic definitions with "::"
        find_and_replace((char *) topic, "::", ".");

        qeoType = qeocore_type_struct_new(topic);

        if (qeoType == NULL) {
            qeo_log_e("qeocore_type_struct_new failed for topic:%s", topic);
            break;
        }
        qeo_log_d("Registered new struct with name %s", topic);

        void *iter = json_object_iter(properties);
        while (iter) {
            name = json_object_iter_key(iter);
            if (name == NULL) {
                qeo_log_e("name == NULL");
                iret = false;
                break;
            }

            value = json_object_iter_value(iter);
            if (value == NULL) {
                qeo_log_e("value == NULL");
                iret = false;
                break;
            }

            if (!json_is_object(value)) {
                qeo_log_e("no json object");
                iret = false;
                break;
            }

            if (QEO_OK != object_add_member(factory, qeoType, name, value)) {
                qeo_log_e("object add member failed");
                iret = false;
                break;
            }

            iter = json_object_iter_next(properties, iter);
        }

        if (true != iret) {
            break;
        }

        if (QEO_OK != qeocore_type_register(factory, qeoType, topic)) {
            qeo_log_e("failed to register type: %s", topic);
        }

        ret = QEO_OK;
    } while (0);

    if (ret != QEO_OK) {
        qeocore_type_free(qeoType);
        qeoType = NULL;
    }

    return qeoType;
}

static qeo_retcode_t object_to_data(const json_t *typedesc, json_t *json_data, qeocore_data_t *data)
{
    qeo_retcode_t result = QEO_OK;

    json_t *properties = json_object_get(typedesc, KEY_PROPERTIES);

    if ((NULL != properties) && (json_is_object(properties))) {
        const char  *prop_name  = NULL;
        json_t      *prop_value = NULL;
        void        *iter       = json_object_iter(properties);
        while (iter) {
            // Get type name and value
            prop_name   = json_object_iter_key(iter);
            prop_value  = json_object_iter_value(iter);

            if ((NULL == prop_value) || (!json_is_object(prop_value))) {
                result = QEO_EFAIL;
                break;
            }

            // Get the corresponding value from the json_data
            json_t *value = json_object_get(json_data, prop_name);
            if (NULL == value) {
                qeo_log_e("json_object_get failed for property: %s", prop_name);
                result = QEO_EFAIL;
                break;
            }

            if (QEO_OK != member_to_data(prop_value, value, data)) {
                qeo_log_e("member_to_data failed for property: %s", prop_name);
                result = QEO_EFAIL;
                break;
            }

            iter = json_object_iter_next(properties, iter);
        }
    }
    return result;
}

static bool is_valid_enum_value(const json_t *member, json_int_t enumval){

    json_t *item = json_object_get(member, KEY_ITEM);
    assert(json_is_object(item));
    json_t *values = json_object_get(item, KEY_VALUES);
    assert(json_is_object(values));

    void *iter = json_object_iter(values);
    while (iter) {
        json_int_t labelint;
        json_t *label = json_object_iter_value(iter);
        assert(json_is_integer(label));
        labelint = json_integer_value(label);

        if (labelint == enumval){
            return true;
        }

        iter = json_object_iter_next(values, iter);
    }

    return false;

}

static qeo_retcode_t member_to_data(json_t *member, json_t *json_data, qeocore_data_t *data)
{
    qeo_retcode_t result = QEO_EINVAL;

    json_t  *id   = json_object_get(member, KEY_ID);            // Mandatory
    json_t  *type = json_object_get(member, KEY_QEO_TYPE_CODE); // Mandatory

    if ((NULL == id) || (!json_is_integer(id)) ||
        (NULL == type) || (!json_is_integer(type))) {
        return result;
    }
    result = QEO_EFAIL;

    qeocore_member_id_t qeo_member_id = (qeocore_member_id_t) json_integer_value(id);
    qeocore_typecode_t  qeoTypeCode   = (qeocore_typecode_t) json_integer_value(type);
/*
    bool isKey = false;
    json_t *key_value = json_object_get(member, KEY_KEY);
    if ((NULL != key_value) && (json_is_true(key_value))) {
        isKey = true;
    }
 */
    switch (qeoTypeCode) {
        case QEOCORE_TYPECODE_BOOLEAN:
        {
            if (json_is_boolean(json_data)) {
                qeo_boolean_t qeo_value = (qeo_boolean_t) json_is_true(json_data);
                result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                if (QEO_OK != result) {
                    qeo_log_e("qeocore_data_set_member failed");
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_INT8:
        {
            if (json_is_integer(json_data)) {
                json_int_t json_value = json_integer_value(json_data);
                if ((-128LL <= json_value) && (json_value <= 127LL)) {
                    int8_t qeo_value = (int8_t)json_value;
                    result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                    if (QEO_OK != result) {
                        qeo_log_e("qeocore_data_set_member failed");
                    }
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_INT16:
        {
            if (json_is_integer(json_data)) {
                json_int_t json_value = json_integer_value(json_data);
                if ((-32768LL <= json_value) && (json_value <= 32767LL)) {
                    int16_t qeo_value = (int16_t)json_value;
                    result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                    if (QEO_OK != result) {
                        qeo_log_e("qeocore_data_set_member failed");
                    }
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_INT32:
        {
            if (json_is_integer(json_data)) {
                json_int_t json_value = json_integer_value(json_data);
                if ((-2147483648LL <= json_value) && (json_value <= 2147483647LL)) {
                    int32_t qeo_value = (int32_t)json_value;
                    result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_INT64:
        {
            if (json_is_integer(json_data)) {
                int64_t qeo_value = (int64_t) json_integer_value(json_data);
                result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                if (QEO_OK != result) {
                    qeo_log_e("qeocore_data_set_member failed");
                }
            }

            if (json_is_string(json_data)) {
                const char *qeo_value = json_string_value(json_data);

                intmax_t num = strtoimax(qeo_value, NULL, 10);
                result = qeocore_data_set_member(data, qeo_member_id, (int64_t *) &num);
                if (QEO_OK != result) {
                    qeo_log_e("qeocore_data_set_member failed");
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_FLOAT32:
        {
            /*
               *Also allow integer values, when float:0.0 is written in a webview then JSON.stringify can result into float:0
               *meaning that the float 0.0 is transformed in an integer like 0, that's why we also need to allow integers, to be
               *able to handle these rounding issues.
             */
            if (json_is_real(json_data) || json_is_integer(json_data)) {
                float qeo_value = (float) json_real_value(json_data);
                result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                if (QEO_OK != result) {
                    qeo_log_e("qeocore_data_set_member failed");
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_STRING:
        {
            if (json_is_string(json_data)) {
                const char *qeo_value = json_string_value(json_data);
                result = qeocore_data_set_member(data, qeo_member_id, &qeo_value);
                if (QEO_OK != result) {
                    qeo_log_e("qeocore_data_set_member failed");
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_STRUCT:
        {
            if (json_is_object(json_data)) {
                qeocore_data_t *qeo_data = NULL;
                do {
                    if (QEO_OK != qeocore_data_get_member(data, qeo_member_id, &qeo_data)) {
                        qeo_log_e("qeocore_data_get_member failed");
                        break;
                    }

                    if (NULL == qeo_data) {
                        qeo_log_e("NULL == qeo_data");
                        break;
                    }

                    json_t *item = json_object_get(member, KEY_ITEM);
                    if (NULL == item) {
                        qeo_log_e("NULL == item");
                        break;
                    }
                    if (!json_is_object(item)) {
                        qeo_log_e("item is not an object");
                        break;
                    }

                    result = object_to_data(item, json_data, qeo_data);
                    if (QEO_OK != result) {
                        qeo_log_e("object_to_data failed");
                        break;
                    }

                    result = qeocore_data_set_member(data, qeo_member_id, &qeo_data);
                    if (QEO_OK != result) {
                        qeo_log_e("qeocore_data_set_member failed: member_id:%u", qeo_member_id);
                        break;
                    }
                } while (0);

                if (NULL != qeo_data) {
                    qeocore_data_free(qeo_data);
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_SEQUENCE:
        {
            if (json_is_array(json_data)) {
                qeocore_data_t *qeo_data = NULL;
                if ((QEO_OK == qeocore_data_get_member(data, qeo_member_id, &qeo_data)) && (NULL != qeo_data)) {
                    json_t *items = json_object_get(member, KEY_ITEMS);
                    if ((NULL != items) && json_is_object(items)) {
                        if (QEO_OK == array_to_data(items, json_data, qeo_data)) {
                            result = qeocore_data_set_member(data, qeo_member_id, &qeo_data);
                        }
                    }
                }
                qeocore_data_free(qeo_data);
            }
        }
        break;
        case QEOCORE_TYPECODE_ENUM:
        {
            if (!json_is_integer(json_data)){
                qeo_log_e("Enum should be integer");
                break;
            }
            qeo_enum_value_t enum_val = (qeo_enum_value_t) json_integer_value(json_data);
            if (is_valid_enum_value(member, enum_val) == false){
                qeo_log_e("Could not convert json to enum");
                break;
            }
            result = qeocore_data_set_member(data, qeo_member_id, &enum_val);
        }
        break;
    }
    return result;
}


static qeo_retcode_t array_to_data(json_t *elemtype, json_t *json_data, qeocore_data_t *data)
{
    qeo_retcode_t result = QEO_EINVAL;

    json_t *type = json_object_get(elemtype, KEY_QEO_TYPE_CODE); // Mandatory

    if ((NULL == type) || (!json_is_integer(type))) {
        return result;
    }

    qeo_sequence_t seq = { 0 };
    if (QEO_OK != qeocore_data_sequence_new(data, &seq, json_array_size(json_data))) {
        result = QEO_EFAIL;
        return result;
    }

    qeocore_typecode_t qeoTypeCode = (qeocore_typecode_t) json_integer_value(type);
    result = QEO_OK;
    int i;
    int numOfElem = json_array_size(json_data);
    for (i = 0; i < numOfElem; ++i) {
        json_t *elem = json_array_get(json_data, i);
        if (NULL == elem) {
            result = QEO_EFAIL;
            break;
        }

        result = QEO_EFAIL;

        switch (qeoTypeCode) {
            case QEOCORE_TYPECODE_BOOLEAN:
            {
                if (json_is_boolean(elem)) {
                    ((qeo_boolean_t *)DDS_SEQ_DATA(seq))[i] = json_is_true(elem);
                    result = QEO_OK;
                }
            }
            break;

            case QEOCORE_TYPECODE_INT8:
            {
                if (json_is_integer(elem)) {
                    json_int_t json_value = json_integer_value(elem);
                    if ((-128LL <= json_value) && (json_value <= 127LL)) {
                        ((int8_t *)DDS_SEQ_DATA(seq))[i] = (int8_t) json_value;
                        result = QEO_OK;
                    }
                }
            }
            break;

            case QEOCORE_TYPECODE_INT16:
            {
                if (json_is_integer(elem)) {
                    json_int_t json_value = json_integer_value(elem);
                    if ((-32768LL <= json_value) && (json_value <= 32767LL)) {
                        ((int16_t *)DDS_SEQ_DATA(seq))[i] = (int16_t) json_value;
                        result = QEO_OK;
                    }
                }
            }
            break;

            case QEOCORE_TYPECODE_INT32:
            {
                if (json_is_integer(elem)) {
                    json_int_t json_value = json_integer_value(elem);
                    if ((-2147483648LL <= json_value) && (json_value <= 2147483647LL)) {
                        ((int32_t *)DDS_SEQ_DATA(seq))[i] = (int32_t) json_value;
                        result = QEO_OK;
                    }
                }
            }
            break;

            case QEOCORE_TYPECODE_INT64:
            {
                if (json_is_integer(elem)) {
                    ((int64_t *)DDS_SEQ_DATA(seq))[i] = (int64_t) json_integer_value(elem);
                    result = QEO_OK;
                }

                if (json_is_string(elem)) {
                    const char  *qeo_value  = json_string_value(elem);
                    intmax_t    num         = strtoimax(qeo_value, NULL, 10);
                    ((int64_t *)DDS_SEQ_DATA(seq))[i] = (int64_t) num;
                    result = QEO_OK;
                }
            }
            break;

            case QEOCORE_TYPECODE_FLOAT32:
            {
                /*
                   *Also allow integer values, when float:0.0 is written in a webview then JSON.stringify can result into float:0
                   *meaning that the float 0.0 is transformed in an integer like 0, that's why we also need to allow integers, to be
                   *able to handle these rounding issues.
                 */
                if (json_is_real(json_data) || json_is_integer(json_data)) {
                    ((float *)DDS_SEQ_DATA(seq))[i] = (float) json_real_value(elem);
                    result = QEO_OK;
                }
            }
            break;

            case QEOCORE_TYPECODE_STRING:
            {
                if (json_is_string(elem)) {
                    ((char **)DDS_SEQ_DATA(seq))[i] = strdup(json_string_value(elem));
                    result = QEO_OK;
                }
            }
            break;

            case QEOCORE_TYPECODE_STRUCT:
            {
                if (json_is_object(elem)) {
                    json_t *item = json_object_get(elemtype, KEY_ITEM);
                    if ((NULL != item) && json_is_object(item)) {
                        qeocore_data_t *qeo_value = ((qeocore_data_t **)DDS_SEQ_DATA(seq))[i];
                        result = object_to_data(item, elem, qeo_value);
                    }
                }
            }
            break;

            case QEOCORE_TYPECODE_SEQUENCE:
            {
                if (json_is_array(elem)) {
                    json_t *items = json_object_get(elemtype, KEY_ITEMS);
                    if ((NULL != items) && json_is_object(items)) {
                        qeocore_data_t *qeo_value = ((qeocore_data_t **)DDS_SEQ_DATA(seq))[i];
                        result = array_to_data(items, elem, qeo_value);
                    }
                }
            }
            break;
            case QEOCORE_TYPECODE_ENUM:
            {
                if (!json_is_integer(json_data)){
                    qeo_log_e("Enum should be integer");
                    break;
                }
                qeo_enum_value_t enum_val = (qeo_enum_value_t) json_integer_value(json_data);
                if (is_valid_enum_value(elemtype, enum_val) == false){
                    qeo_log_e("Could not convert json to enum");
                    break;
                }
                ((qeo_enum_value_t *)DDS_SEQ_DATA(seq))[i] = enum_val;
            }
            break;
        }

        if (QEO_OK != result) {
            break;
        }
    }

    if (QEO_OK == result) {
        result = qeocore_data_sequence_set(data, &seq, 0);
    }

    return result;
}

static json_t *object_data_to_json(const json_t *typedesc, const qeocore_data_t *data)
{
    json_t  *json_data  = NULL;
    json_t  *properties = json_object_get(typedesc, KEY_PROPERTIES);

    if ((NULL != properties) && (json_is_object(properties))) {
        json_data = json_object();
        const char  *prop_name  = NULL;
        json_t      *prop_value = NULL;
        void        *iter       = json_object_iter(properties);
        while (iter) {
            // Get type name and value
            prop_name   = json_object_iter_key(iter);
            prop_value  = json_object_iter_value(iter);

            json_t *value = member_data_to_json(prop_value, data);
            if (NULL != value) {
                json_object_set_new(json_data, prop_name, value);
            }
            else {
                json_decref(json_data);
                json_data = NULL;
                break;
            }

            iter = json_object_iter_next(properties, iter);
        }
    }
    return json_data;
}

static json_t *member_data_to_json(const json_t *member, const qeocore_data_t *data)
{
    json_t  *json_data  = NULL;
    json_t  *id         = json_object_get(member, KEY_ID);            // Mandatory
    json_t  *type       = json_object_get(member, KEY_QEO_TYPE_CODE); // Mandatory

    if ((NULL == id) || (!json_is_integer(id)) ||
        (NULL == type) || (!json_is_integer(type))) {
        return json_data;
    }

    qeocore_member_id_t qeo_member_id = (qeocore_member_id_t) json_integer_value(id);
    qeocore_typecode_t  qeoTypeCode   = (qeocore_typecode_t) json_integer_value(type);

    switch (qeoTypeCode) {
        case QEOCORE_TYPECODE_BOOLEAN:
        {
            qeo_boolean_t bool_value = 0;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &bool_value)) {
                json_data = bool_value ? (json_true()) : (json_false());
            }
        }
        break;

        case QEOCORE_TYPECODE_INT8:
        {
            int8_t int_value;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &int_value)) {
                json_data = json_integer(int_value);
            }
        }
        break;

        case QEOCORE_TYPECODE_INT16:
        {
            int16_t int_value;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &int_value)) {
                json_data = json_integer(int_value);
            }
        }
        break;

        case QEOCORE_TYPECODE_INT32:
        {
            int32_t int_value;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &int_value)) {
                json_data = json_integer(int_value);
            }
        }
        break;

        case QEOCORE_TYPECODE_INT64:
        {
            int64_t int_value;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &int_value)) {
                char *char_value = NULL;
                if (-1 != asprintf(&char_value, "%" PRId64 "", int_value)) {
                    json_data = json_string(char_value);
                    free(char_value);
                }
            }
        }
        break;

        case QEOCORE_TYPECODE_FLOAT32:
        {
            float float_value;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &float_value)) {
                json_data = json_real(float_value);
            }
        }
        break;

        case QEOCORE_TYPECODE_STRING:
        {
            char *char_value;
            if (QEO_OK == qeocore_data_get_member(data, qeo_member_id, &char_value)) {
                json_data = json_string(char_value);
                free(char_value);
            }
        }
        break;

        case QEOCORE_TYPECODE_STRUCT:
        {
            qeocore_data_t *qeo_data = NULL;
            do {
                json_t *item = json_object_get(member, KEY_ITEM);
                if (NULL == item) {
                    qeo_log_e("NULL == item");
                    break;
                }

                if (!json_is_object(item)) {
                    qeo_log_e("not an object");
                    break;
                }

                if (QEO_OK != qeocore_data_get_member(data, qeo_member_id, &qeo_data)) {
                    qeo_log_e("qeocore_data_get_member failed");
                    break;
                }

                if (NULL == qeo_data) {
                    qeo_log_e("NULL == qeo_data");
                    break;
                }

                json_data = object_data_to_json(item, qeo_data);
            } while (0);

            if (NULL != qeo_data) {
                qeocore_data_free(qeo_data);
            }
        }
        break;

        case QEOCORE_TYPECODE_SEQUENCE:
        {
            json_t *items = json_object_get(member, KEY_ITEMS);
            if ((NULL != items) && json_is_object(items)) {
                qeocore_data_t *seqdata = NULL;
                if ((QEO_OK == qeocore_data_get_member(data, qeo_member_id, &seqdata)) && (NULL != seqdata)) {
                    json_data = array_data_to_json(items, seqdata);
                    qeocore_data_free(seqdata);
                }
            }
        }
        break;
        case QEOCORE_TYPECODE_ENUM:
        {
            qeo_enum_value_t enum_value;
            if (QEO_OK != qeocore_data_get_member(data, qeo_member_id, &enum_value)) {
                qeo_log_e("Could not get member");
                break;
            } 
            if (is_valid_enum_value(member, enum_value) == false){
                qeo_log_e("Not a valid enum value");
                break;
            }
            json_data = json_integer(enum_value);

        }
        break;
    }
    return json_data;
}

static json_t *array_data_to_json(const json_t *typedesc, qeocore_data_t *data)
{
    json_t          *json_data  = NULL;
    qeo_sequence_t  seq         = { 0 };

    json_t *type = json_object_get(typedesc, KEY_QEO_TYPE_CODE); // Mandatory

    if ((NULL == type) || (!json_is_integer(type)) ||
        (QEO_OK != qeocore_data_sequence_get(data, &seq, 0, QEOCORE_SIZE_UNLIMITED))) {
        return json_data;
    }
    json_data = json_array();
    qeocore_typecode_t qeoTypeCode = (qeocore_typecode_t) json_integer_value(type);

    int i;
    int numOfElem = DDS_SEQ_LENGTH(seq);
    for (i = 0; i < numOfElem; ++i) {
        switch (qeoTypeCode) {
            case QEOCORE_TYPECODE_BOOLEAN:
            {
                qeo_boolean_t bool_value = ((qeo_boolean_t *)DDS_SEQ_DATA(seq))[i];
                json_array_append_new(json_data, bool_value ? (json_true()) : (json_false()));
            }
            break;

            case QEOCORE_TYPECODE_INT8:
            {
                int8_t int_value = ((int8_t *)DDS_SEQ_DATA(seq))[i];
                json_array_append_new(json_data, json_integer((json_int_t)int_value));
            }
            break;

            case QEOCORE_TYPECODE_INT16:
            {
                int8_t int_value = ((int16_t *)DDS_SEQ_DATA(seq))[i];
                json_array_append_new(json_data, json_integer((json_int_t)int_value));
            }
            break;

            case QEOCORE_TYPECODE_INT32:
            {
                int8_t int_value = ((int32_t *)DDS_SEQ_DATA(seq))[i];
                json_array_append_new(json_data, json_integer((json_int_t)int_value));
            }
            break;

            case QEOCORE_TYPECODE_INT64:
            {
                int64_t int_value   = ((int64_t *)DDS_SEQ_DATA(seq))[i];
                char    *char_value = NULL;
                if (-1 != asprintf(&char_value, "%" PRId64 "", int_value)) {
                    json_array_append_new(json_data, json_string(char_value));
                    free(char_value);
                }
            }
            break;

            case QEOCORE_TYPECODE_STRING:
            {
                char *char_value = ((char **)DDS_SEQ_DATA(seq))[i];
                json_array_append_new(json_data, json_string(char_value));
            }
            break;

            case QEOCORE_TYPECODE_STRUCT:
            {
                json_t *item = json_object_get(typedesc, KEY_ITEM);
                if ((NULL != item) && json_is_object(item)) {
                    qeocore_data_t  *object_value = ((qeocore_data_t **)DDS_SEQ_DATA(seq))[i];
                    json_t          *json_element = object_data_to_json(item, object_value);
                    if (NULL != json_element) {
                        json_array_append_new(json_data, json_element);
                    }
                    else {
                        json_decref(json_data);
                        json_data = NULL;
                    }
                }
                else {
                    json_decref(json_data);
                    json_data = NULL;
                }
            }
            break;

            case QEOCORE_TYPECODE_SEQUENCE:
            {
                json_t *items = json_object_get(typedesc, KEY_ITEMS);
                if ((NULL != items) && json_is_object(items)) {
                    qeocore_data_t  *element_value  = ((qeocore_data_t **)DDS_SEQ_DATA(seq))[i];
                    json_t          *json_element   = array_data_to_json(items, element_value);
                    if (NULL != json_element) {
                        json_array_append_new(json_data, json_element);
                    }
                    else {
                        json_decref(json_data);
                        json_data = NULL;
                    }
                }
                else {
                    json_decref(json_data);
                    json_data = NULL;
                }
            }
            break;
            case QEOCORE_TYPECODE_ENUM:
            {
                qeo_enum_value_t enum_value = ((qeo_enum_value_t *)DDS_SEQ_DATA(seq))[i];
                if (is_valid_enum_value(typedesc, enum_value) == false){
                    qeo_log_e("Not a valid enum value");
                    break;
                }
                json_t *json_enum = json_integer(enum_value);
                json_array_append_new(json_data, json_enum);
            }
            break;
            case QEOCORE_TYPECODE_FLOAT32:
            {
                float float_value = ((float *)DDS_SEQ_DATA(seq))[i];
                json_array_append_new(json_data, json_real(float_value));
            }
            break;

        }

        if (NULL == json_data) {
            break;
        }
    }
    qeocore_data_sequence_free(data, &seq);
    return json_data;
}

static void find_and_replace(char *haystack, char *needle, char *replacement)
{
    int   toreplace_len = strlen(needle);
    int   rep_len       = strlen(replacement);
    char  *toreplace    = NULL;

    while ((toreplace = strstr(haystack, needle)) != NULL) {
        replace(toreplace, toreplace_len, replacement, rep_len);
        haystack = toreplace;
    }
}

/* This function does not take care of reallocating so this function only works if you make the string smaller.! */
static int replace(char *toreplace, int toreplace_len, const char *replacement, int rep_len)
{
    assert(toreplace_len >= rep_len); /* check contract */
    if (toreplace_len != rep_len) {   /* if the lengths don't match, we have to move first... */
        int len = strlen(toreplace);
        memmove(toreplace + toreplace_len + (rep_len - toreplace_len), toreplace + toreplace_len, len + 1 - toreplace_len);
    }
    memcpy(toreplace, replacement, rep_len);
    return rep_len - toreplace_len;
}

/*########################################################################
#                                                                       #
#  PUBLIC FUNCTION IMPLEMENTATION                                       #
#                                                                       #
########################################################################*/

qeocore_type_t *types_from_json(const qeo_factory_t *factory, json_t *typedesc)
{
    qeocore_type_t *qeocore_top_type = NULL;

    do {
        if ((NULL == typedesc) || (NULL == factory)){
            break;
        }

        qeocore_top_type = build_object(factory, typedesc);
        if (NULL == qeocore_top_type) {
            qeo_log_e("build_object failed");
            break;
        }
    } while (0);

    return qeocore_top_type;
}

qeo_retcode_t data_from_json(const json_t *typedesc, json_t *json_data, qeocore_data_t *data)
{
    qeo_retcode_t ret = QEO_EINVAL;

    do {
        if ((NULL == typedesc) || (NULL == json_data) || (NULL == data)) {
            break;
        }

        ret = object_to_data(typedesc, json_data, data);
        if (ret != QEO_OK) {
            qeo_log_e("Could not convert object to data");
            break;
        }
    } while (0);

    return ret;
}

/* the returned string must be freed by the caller */
char *json_from_data(const json_t *typedesc, const qeocore_data_t *data)
{
    char *json_string = NULL;

    if ((NULL != typedesc) || (NULL != data)) {
        json_t *json_data = object_data_to_json(typedesc, data);
        if (NULL != json_data) {
            json_string = json_dumps(json_data, 0);
            json_decref(json_data);
        }
    }
    return json_string;
}

qeo_retcode_t qeo_identity_from_json(qeo_identity_t **id, const char *idstring)
{
    qeo_retcode_t   result  = QEO_EINVAL;
    qeo_identity_t  *tmp_id = NULL;

    int64_t     realm_id;
    int64_t     device_id;
    int64_t     user_id;
    const char  *url = NULL;

    if ((NULL != id) && (NULL != idstring)) {
        /******HACK ALERT********************/
        if (strcmp(idstring, "open") == 0) {
            *id = QEO_IDENTITY_OPEN;
            return QEO_OK;
        }

        if (strcmp(idstring, "default") == 0) {
            *id = QEO_IDENTITY_DEFAULT;
            return QEO_OK;
        }

        else {
            return QEO_EINVAL;
        }
        /************************************/
        tmp_id = calloc(1, sizeof(qeo_identity_t));
        if (tmp_id == NULL) {
            result = QEO_ENOMEM;
            return result;
        }

        json_t *jsonTypeDesc = json_loads(idstring, 0, NULL);

        if (NULL != jsonTypeDesc) {
            json_t *properties = json_object_get(jsonTypeDesc, "properties");
            if (NULL != properties) {
                do {
                    json_t *j_realm_id = json_object_get(properties, "realm_id");
                    if (NULL == j_realm_id) {
                        result = QEO_EFAIL;
                        break;
                    }
                    realm_id = (int64_t)json_integer_value(j_realm_id);

                    json_t *j_device_id = json_object_get(properties, "device_id");
                    if (NULL == j_device_id) {
                        result = QEO_EFAIL;
                        break;
                    }
                    device_id = (int64_t)json_integer_value(j_device_id);

                    json_t *j_user_id = json_object_get(properties, "user_id");
                    if (NULL == j_user_id) {
                        result = QEO_EFAIL;
                        break;
                    }
                    user_id = (int64_t)json_integer_value(j_user_id);

                    json_t *j_url = json_object_get(properties, "url");
                    if (NULL == j_url) {
                        result = QEO_EFAIL;
                        break;
                    }
                    url = json_string_value(j_url);
                    if (NULL == url) {
                        result = QEO_EFAIL;
                        break;
                    }

                    tmp_id->realm_id  = realm_id;
                    tmp_id->device_id = device_id;
                    tmp_id->user_id   = user_id;
                    tmp_id->url       = strdup(url); // url should not be freed by the user and it's valid as long as j_url exists

                    if (NULL == tmp_id->url) {
                        return QEO_ENOMEM;
                    }
                    *id     = tmp_id;
                    result  = QEO_OK;
                } while (0);
            }
        }
    }

    return result;
}
