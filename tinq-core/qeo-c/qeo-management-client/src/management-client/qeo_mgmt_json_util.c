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

/*#######################################################################
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/

#include <string.h>


#include "jansson.h"
#include "qeo/log.h"
#include "qeo_mgmt_json_util.h"
#include "qeo_mgmt_cert_util.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

#define QEO_MGMT_CLIENT_LOCATOR_UDP_V6  "UDPv6"
#define QEO_MGMT_CLIENT_LOCATOR_UDP_V4  "UDPv4"
#define QEO_MGMT_CLIENT_LOCATOR_TCP_V6  "TCPv6"
#define QEO_MGMT_CLIENT_LOCATOR_TCP_V4  "TCPv4"

#define QEO_MGMT_CLIENT_DEPRECATION "deprecation"

struct qeo_mgmt_json_hdl_s{
    json_t *node;
};

struct qeo_mgmt_enum_descriptor_s{
    int value;
    char *description;
};

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/
struct qeo_mgmt_enum_descriptor_s locator_type_desc[]={
    {QMGMT_LOCATORTYPE_TCPV4, QEO_MGMT_CLIENT_LOCATOR_TCP_V4},
    {QMGMT_LOCATORTYPE_TCPV6, QEO_MGMT_CLIENT_LOCATOR_TCP_V6},
    {QMGMT_LOCATORTYPE_UDPV4, QEO_MGMT_CLIENT_LOCATOR_UDP_V4},
    {QMGMT_LOCATORTYPE_UDPV6, QEO_MGMT_CLIENT_LOCATOR_UDP_V6},

};

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/*
 * Intentionally made non static for testing purposes.
 */
const char* _get_locator_string(qeo_mgmt_client_locator_type_t type)
{
    int i = 0;
    for (i = 0; i < sizeof(locator_type_desc)/sizeof(locator_type_desc[0]); ++i) {
        if (type == locator_type_desc[i].value){
            return locator_type_desc[i].description;
        }
    }
    return NULL;//error condition.
}

/*
 * Intentionally made non static for testing purposes.
 */
qeo_mgmt_client_locator_type_t _get_locator_type(const char* stringValue)
{
    int i = 0;
    for (i = 0; i < sizeof(locator_type_desc)/sizeof(locator_type_desc[0]); ++i) {
        if (strcmp(locator_type_desc[i].description, stringValue) == 0){
            return locator_type_desc[i].value;
        }
    }
    return QMGMT_LOCATORTYPE_UNKNOWN;//error condition.
}

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

qeo_mgmt_client_retcode_t qeo_mgmt_json_util_parseGetFWDMessage(const char* data, ssize_t length, qeo_mgmt_client_forwarder_cb callback, void* cookie)
{
    json_error_t json_error = {0};
    qeo_mgmt_client_retcode_t result = QMGMTCLIENT_EBADREPLY;
    json_t* message = json_loadb(data, length, JSON_REJECT_DUPLICATES, &json_error);
    qeo_mgmt_client_forwarder_t* fwd = NULL;
    do {
       json_t* fwdArray;
       ssize_t fwdSize;
       ssize_t i;
       if (message == NULL) {
           qeo_log_w("Failed to parse json message %s (%s:%d:%d)", json_error.text, json_error.source, json_error.line, json_error.column);
           qeo_log_w("Data = (%s)", data);
           //JSON parsing error
           break;
       }
       if (!json_is_object(message)) {
           qeo_log_w("invalid message received - top level is not a JSON object");
           break;
       }
       fwdArray = json_object_get(message, "forwarders");
       if (fwdArray == NULL || !json_is_array(fwdArray)) {
           qeo_log_w("root object did not contain a field 'forwarders' of type array (%p)", fwdArray);
       }
       fwdSize = json_array_size(fwdArray);
       qeo_log_i("Found an array of %d forwarder(s) in message\n", fwdSize);

       for (i = 0; i < fwdSize; i++) {
           qeo_mgmt_client_retcode_t cb_result;
           json_t* fwdObject = json_array_get(fwdArray, i);
           json_t* idString;
           json_t* locatorArray;
           ssize_t nrOfLocators;
           ssize_t j;

           if (!json_is_object(fwdObject)) {
               qeo_log_w("unexpected content in fwdArray - object expected");
               break;
           }
           idString = json_object_get(fwdObject, "id");
           if (idString == NULL || !json_is_string(idString)) {
               qeo_log_w("forwarder object did not contain a string field called 'id' (%p)",  idString);
               break;
           }
           locatorArray = json_object_get(fwdObject, "locators");
           if (locatorArray == NULL || !json_is_array(locatorArray)) {
               qeo_log_w("forwarder object did not contain an array field called 'nrOfLocators' (%p)",  locatorArray);
               break;

           }
           nrOfLocators = json_array_size(locatorArray);
           qeo_log_i("found forwarder with id='%s' and %d locator(s)", json_string_value(idString), nrOfLocators);
           fwd = malloc(sizeof(qeo_mgmt_client_forwarder_t));
           if (fwd == NULL) {
               qeo_log_w("fwd == NULL");
               result = QMGMTCLIENT_EMEM;
               break;
           }
           fwd->nrOfLocators = nrOfLocators;
           fwd->locators = calloc(nrOfLocators, sizeof(qeo_mgmt_client_locator_t));
           fwd->deviceID = qeo_mgmt_util_hex_to_int(json_string_value(idString));
           if (fwd->deviceID == -1){
               qeo_log_w("Invalid device id inside json message");
               break;
           }

           if (fwd->locators == NULL) {
               qeo_log_w("fwd->locators == NULL");
               result = QMGMTCLIENT_EMEM;
               break;
           }

           for (j = 0; j < nrOfLocators; j++) {
               json_t* endpointObj = json_array_get(locatorArray, j);
               json_t* typeString = json_object_get(endpointObj,"type");
               json_t* addrString = json_object_get(endpointObj,"address");
               json_t* portInt = json_object_get(endpointObj,"port");

               if (portInt == NULL || !json_is_integer(portInt)) {
                   qeo_log_w("locator object did not contain a integer field called 'port' (%p)",  portInt);
                   break;
               }
               if (addrString == NULL || !json_is_string(addrString)) {
                   qeo_log_w("locator object did not contain a string field called 'address' (%p)",  addrString);
                   break;
               }
               if (typeString == NULL || !json_is_string(typeString)) {
                   qeo_log_w("locator object did not contain a string field called 'type' (%p)",  typeString);
                   break;
               }
               qeo_log_i("locator object %d = {type = '%s', address = '%s', port = %d}",  j, json_string_value(typeString),
                         json_string_value(addrString), (int) json_integer_value(portInt));
               //valid locator

               fwd->locators[j].port = (int) json_integer_value(portInt);
               if (fwd->locators[j].port < -1 || fwd->locators[j].port > 0xffff){
                   qeo_log_w("Invalid port inside locator");
                   break;
               }
               fwd->locators[j].type = _get_locator_type(json_string_value(typeString));
               fwd->locators[j].address = strdup(json_string_value(addrString)); //check value; don't forget to free!
               if (fwd->locators[j].address == NULL) {
                   qeo_log_w("locator->address == NULL");
                   break;
               }
           }
           if (j != nrOfLocators){
               break;
           }
           cb_result = callback(fwd, cookie);
           fwd = NULL; //pointer is handed over; set it to NULL so we wont free it.
           if (cb_result != QMGMTCLIENT_OK) {//the callback reports an error abort.
               result = cb_result;
               break;

           }
       }
       if (i != fwdSize){
           break;
       }
       qeo_log_i("Successfully walked JSON object tree...");
       result = QMGMTCLIENT_OK;
    }
    while(0);
    if (message) {
        json_decref(message);
    }
    if (fwd) { //if an error occurred, then the 'fwd' is not freed.
        qeo_mgmt_client_free_forwarder(fwd);
    }

    return result;
}

qeo_mgmt_client_retcode_t qeo_mgmt_json_util_formatLocatorData(qeo_mgmt_client_locator_t *locators,
                                                     u_int32_t nrOfLocators,
                                                     char **message,
                                                     size_t *length)
{
    int i = 0;
    qeo_mgmt_client_retcode_t rc = QMGMTCLIENT_EFAIL;
    //TODO: should there be a limitation on the number of locators that can be registered?
    json_t* msg = NULL;
    json_t* array = json_array();
    json_error_t json_error = {0};

    do {
        for (i = 0; i < nrOfLocators; i++) {
            const char* typeString = _get_locator_string(locators[i].type);
            json_t *entry = NULL;
            if (locators[i].port < -1 || locators[i].port > 0xffff || typeString == NULL  || locators[i].address == NULL) {
                //-1 is allowed representing the default port.
                rc = QMGMTCLIENT_EINVAL;
                break;
            }
            entry = json_pack_ex(&json_error, 0, "{sssssi}", "type", typeString, "address", locators[i].address, "port",
                                 (int)locators[i].port);
            if (entry == NULL ) {
                qeo_log_w("Failed to create json locator object %s (%s:%d:%d)", json_error.text, json_error.source,
                          json_error.line, json_error.column);
                break;
            }
            if (json_array_append_new(array, entry) == -1){
                rc = QMGMTCLIENT_EMEM;
                json_decref(entry);
                break;
            }
        }
        if (i != nrOfLocators){
            break;
        }

        msg = json_pack_ex(&json_error, 0, "{so}", "locators", array);
        if (msg == NULL ) {
            qeo_log_w("Failed to create json locators list %s (%s:%d:%d)", json_error.text, json_error.source,
                      json_error.line, json_error.column);
            break;
        }
        array = NULL; /* No need to free it anymore */
        *message = json_dumps(msg, JSON_PRESERVE_ORDER); //data can be NULL in case of error. If not NULL data should be freed!

        if (*message == NULL) {
            rc = QMGMTCLIENT_EMEM;
            break;
        }
        *length = strlen(*message);
        rc = QMGMTCLIENT_OK;
    } while (0);

    if (msg != NULL){
        json_decref(msg);
    }
    if (array != NULL){
        json_decref(array);
    }
    return rc;
}

qeo_mgmt_json_hdl_t qeo_mgmt_json_util_parse_message(const char* message, size_t length){
    json_error_t json_error;
    qeo_mgmt_json_hdl_t result = NULL;

    do {
        if (message == NULL){
            break;
        }
        result = (qeo_mgmt_json_hdl_t) calloc(1, sizeof(struct qeo_mgmt_json_hdl_s));
        if (result == NULL){
            break;
        }
        result->node = json_loadb(message, length, JSON_REJECT_DUPLICATES, &json_error);
        if (result->node == NULL) {
            qeo_log_w("Failed to parse json message %s (%s:%d:%d)", json_error.text, json_error.source, json_error.line, json_error.column);
            free(result);
            result = NULL;
            break;
        }
    } while(0);
    return result;
}


qeo_mgmt_client_retcode_t qeo_mgmt_json_util_get_string(qeo_mgmt_json_hdl_t hdl, const char **query, size_t nrquery, qeo_mgmt_client_json_value_t *value){
    qeo_mgmt_client_retcode_t rc = QMGMTCLIENT_EINVAL;
    json_t* node = NULL;
    json_t* deprecation = NULL;
    int i = 0;
    const char* buf = NULL;

    do {
        if ((hdl == NULL) || (nrquery <= 0) || (query == NULL) || (*query == NULL) || (value == NULL)){
            break;
        }
        value->deprecated=false;
        node = hdl->node;
        while (i < nrquery){
            deprecation = json_object_get(node, QEO_MGMT_CLIENT_DEPRECATION);
            node = json_object_get(node, query[i]);
            if (node == NULL){
                break;
            }
            if (deprecation != NULL){
                value->deprecated=true;
            }
            i++;
        }
        if (node == NULL) {
            rc = QMGMTCLIENT_EFAIL;
            qeo_log_w("Failed to retrieve json object (%s)", query[i]);
            break;
        }
        if (!json_is_string(node)) {
            qeo_log_w("Expected string but found another json type (%d)", json_typeof(node));
            break;
        }

        buf = json_string_value(node);
        if (buf == NULL){
            break;
        }
        value->value = strdup(buf);
        if (value->value == NULL){
            rc = QMGMTCLIENT_EMEM;
            break;
        }
        rc = QMGMTCLIENT_OK;
    } while(0);
    return rc;
}

void qeo_mgmt_json_util_release_handle(qeo_mgmt_json_hdl_t hdl){
    json_decref(hdl->node);
    free(hdl);
}

