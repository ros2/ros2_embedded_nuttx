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
#include <assert.h>
#include <string.h>
#include <semaphore.h>
#include <jansson.h>
#include <stdlib.h>
#include "verbose.h"
#include <qeo/jsonasync.h>
/*########################################################################
#                                                                       #
#  TYPES and DEFINES SECTION                                            #
#                                                                       #
########################################################################*/
#define KEY_ID "id"
typedef enum {
    ID_FACTORY_CREATE, 
    ID_READER_CREATE, 
    ID_WRITER_CREATE, 
}call_id_t;
/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                           #
#                                                                       #
########################################################################*/
static void async_event_callback (const qeo_json_async_ctx_t *ctx, uintptr_t userdata, const char *id, const char *event, const char *json_data);


/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                              #
#                                                                       #
########################################################################*/
static sem_t _sync;
static qeo_json_async_ctx_t * _ctx = NULL;
static char _factory_id[64] = {};
static char _event_writer_id[64] = {};
static char _args[256] = {};

/*########################################################################
#                                                                       #
#  STATIC FUNCTION IMPLEMENTATION                                       #
#                                                                       #
########################################################################*/
static void async_event_callback (const qeo_json_async_ctx_t *ctx, uintptr_t userdata, const char *id, const char *event, const char *json_data)
{
    json_t  *jsonData       = NULL;
    json_t  *jsonId       = NULL;
    jsonData = json_loads(json_data, 0, NULL);
    assert (NULL != jsonData);
    assert(json_is_object(jsonData));

    log_verbose("%s enter", __FUNCTION__);
    log_verbose("%s id:%s event:%s json_data:%s", __FUNCTION__, id, event,json_data);

    if (0 == strcmp(id,"req_1")) {
        jsonId = json_object_get(jsonData, KEY_ID);
        assert(json_is_string(jsonId)); 
        assert(-1 != sprintf(_factory_id,"%s",json_string_value(jsonId)));
        log_verbose("%s id: %s", __FUNCTION__, _factory_id);
    }

    if (0 == strcmp(id,"req_2")) {
        jsonId = json_object_get(jsonData, KEY_ID);
        assert(json_is_string(jsonId)); 
        assert(-1 != sprintf(_event_writer_id,"%s",json_string_value(jsonId)));
        log_verbose("%s id: %s", __FUNCTION__, _event_writer_id);
    }

    json_decref(jsonData);
    sem_post(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}

/*########################################################################
#                                                                       #
#  PUBLIC FUNCTION IMPLEMENTATION                                       #
#                                                                       #
########################################################################*/
int main(int argc, const char **argv)
{
    
    qeo_json_async_listener_t async_listener = {
        .on_qeo_json_event=async_event_callback
    };

    log_verbose("%s enter", __FUNCTION__);
    sem_init(&_sync, 0, 0);
    _ctx = qeo_json_async_create(&async_listener,0);

    assert(QEO_OK == qeo_json_async_call(_ctx,"create","{\"id\":\"req_1\",\"objType\":\"factory\"}"));
    sem_wait(&_sync);

    sprintf(_args,"{\"id\":\"req_2\",\"factoryId\":\"%s\",\"objType\":\"eventWriter\",\"typeDesc\":{\
        \"topic\": \"org.qeo.sample.simplechat.ChatMessage\",\
        \"behavior\": \"event\",\
        \"properties\": {\
            \"from\": { \
                \"type\": \"string\"\
            },\
            \"message\": {\
                \"type\": \"string\"\
            }\
        } \
    }\
    }",_factory_id);
    assert(QEO_OK == qeo_json_async_call(_ctx,"create",_args));
    sem_wait(&_sync);

    sprintf(_args,"{\"id\":\"%s\",\"factoryId\":\"%s\",\"objType\":\"eventWriter\"}", _event_writer_id, _factory_id);
    assert(QEO_OK == qeo_json_async_call(_ctx,"close",_args));
    sprintf(_args,"{\"id\":\"%s\",\"objType\":\"factory\"}",_factory_id);
    assert(QEO_OK == qeo_json_async_call(_ctx,"close",_args));
    sleep(1);

    qeo_json_async_close(_ctx);
    sem_destroy(&_sync);
    log_verbose("%s exit", __FUNCTION__);
}
