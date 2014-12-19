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
#include <qeo/log.h>
#include <qeo/device.h>
#include <qeojson/api.h>
#include <qeocore/api.h>
#include <utlist.h>

#include "json_types_util.h"
#include "qeo_json_p.h"

/*########################################################################
#                                                                       #
#  TYPES and DEFINES SECTION                                            #
#                                                                       #
########################################################################*/

#define KEY_BEHAVIOR    "behavior"

typedef struct {
    const void  *listener;
    uintptr_t   userdata;
} factory_dispatcher_cookie;


typedef enum {
    QEOJSON_READER_TYPE_EVENT_READER,
    QEOJSON_READER_TYPE_STATE_READER,
    QEOJSON_READER_TYPE_STATE_CHANGE_READER
}qeojson_reader_type_t;

typedef enum {
    QEOJSON_WRITER_TYPE_EVENT_WRITER,
    QEOJSON_WRITER_TYPE_STATE_WRITER,
}qeojson_writer_type_t;

/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                           #
#                                                                       #
########################################################################*/

static void on_qeocore_on_factory_init_done(qeo_factory_t *factory,
                                            bool          success);
static qeo_retcode_t qeo_json_factory_init(qeo_factory_t                      *factory,
                                           const qeo_json_factory_listener_t  *listener,
                                           uintptr_t                          userdata);

static qeojson_reader_t *qeojson_reader_open(const qeo_factory_t    *factory,
                                             qeojson_reader_type_t  reader_type,
                                             void                   *listener,
                                             const char             *json_type,
                                             uintptr_t              userdata);

static void qeojson_reader_close(qeojson_reader_t *jsonreader);

static qeojson_writer_t *qeojson_writer_open(const qeo_factory_t    *factory,
                                             qeojson_writer_type_t  writer_type,
                                             void                   *listener,
                                             const char             *json_type,
                                             uintptr_t              userdata);

static void qeojson_writer_close(qeojson_writer_t *jsonwriter);

/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                              #
#                                                                       #
########################################################################*/

static const qeocore_factory_listener_t _listener = { .on_factory_init_done = on_qeocore_on_factory_init_done };

/*#######################################################################
 #                                                                       #
 # STATIC FUNCTION IMPLEMENTATION                                        #
 #                                                                       #
 ########################################################################*/

static void on_qeocore_on_factory_init_done(qeo_factory_t *factory, bool success)
{
    uintptr_t                   puserdata = 0;
    factory_dispatcher_cookie   *dc       = NULL;
    qeo_json_factory_listener_t *listener = NULL;

    do {
        if ((qeocore_factory_get_user_data(factory, &puserdata)) != QEO_OK) {
            qeo_log_e("qeocore_factory_get_user_data failed");
            return;
        }

        dc = (factory_dispatcher_cookie *)puserdata;
        if (dc == NULL) {
            qeo_log_e("dc == NULL");
            return;
        }

        listener = (qeo_json_factory_listener_t *)dc->listener;
        if (listener == NULL) {
            qeo_log_e("listener == NULL");
            return;
        }

        if (success) {
            listener->on_factory_init_done(factory, dc->userdata);
        }
        else {
            listener->on_factory_init_done(NULL, dc->userdata);
            qeocore_factory_close(factory);
            free(dc);
        }
    } while (0);
}

static qeojson_reader_t *qeojson_reader_open(const qeo_factory_t    *factory,
                                             qeojson_reader_type_t  reader_type,
                                             void                   *listener,
                                             const char             *json_type,
                                             uintptr_t              userdata)
{
    qeo_retcode_t                   ret                 = QEO_EFAIL;
    qeojson_reader_t                *jsonreader         = NULL;
    qeocore_type_t                  *typedesc           = NULL;
    json_reader_dispatcher_cookie_t *dc                 = NULL;
    json_t                          *jsonTypeDesc       = NULL;
    json_t                          *behavior           = NULL;
    const char                      *expected_behaviour = NULL;
    int                             flags               = QEOCORE_EFLAG_NONE;
    qeocore_reader_listener_t       l = { 0 };

    if ((NULL == factory) || (NULL == json_type)) {
        return NULL;
    }


    do {
        dc = calloc(1, sizeof(json_reader_dispatcher_cookie_t));
        if (NULL == dc) {
            break;
        }

        dc->userdata = userdata;

        switch (reader_type) {
            case QEOJSON_READER_TYPE_EVENT_READER:
                flags = QEOCORE_EFLAG_EVENT_DATA;
                expected_behaviour = "event";
                qeo_json_event_reader_listener_t *event_listener = (qeo_json_event_reader_listener_t *) listener;
                dc->etype           = QEO_ETYPE_EVENT_DATA;
                dc->listener.event  = *event_listener;
                if (NULL != event_listener->on_policy_update) {
                    l.on_policy_update = qeojson_core_reader_policy_update_callback_dispatcher;
                }
                break;

            case QEOJSON_READER_TYPE_STATE_READER:
                flags = QEOCORE_EFLAG_STATE_UPDATE;
                expected_behaviour = "state";
                qeo_json_state_reader_listener_t *state_listener = (qeo_json_state_reader_listener_t *)listener;
                dc->etype           = QEO_ETYPE_STATE_UPDATE;
                dc->listener.state  = *state_listener;
                if (NULL != state_listener->on_policy_update) {
                    l.on_policy_update = qeojson_core_reader_policy_update_callback_dispatcher;
                }
                if (NULL != state_listener->on_update) {
                    l.on_data = qeojson_core_callback_dispatcher;
                }

                break;

            case QEOJSON_READER_TYPE_STATE_CHANGE_READER:
                flags = QEOCORE_EFLAG_STATE_DATA;
                expected_behaviour = "state";
                qeo_json_state_change_reader_listener_t *state_change_listener = (qeo_json_state_change_reader_listener_t *) listener;
                dc->etype = QEO_ETYPE_STATE_DATA;
                dc->listener.state_change = *state_change_listener;
                if (NULL != state_change_listener->on_policy_update) {
                    l.on_policy_update = qeojson_core_reader_policy_update_callback_dispatcher;
                }
                l.on_data = qeojson_core_callback_dispatcher;

                break;
        }

        jsonTypeDesc = json_loads(json_type, 0, NULL);
        if (NULL == jsonTypeDesc) {
            qeo_log_e("Could not parse json");
            break;
        }

        if (!json_is_object(jsonTypeDesc)) {
            break;
        }

        behavior = json_object_get(jsonTypeDesc, KEY_BEHAVIOR);

        if (!json_is_string(behavior)) {
            break;
        }


        if (0 != strcmp(json_string_value(behavior), expected_behaviour)) {
            break;
        }

        jsonreader = calloc(1, sizeof(qeojson_reader_t));
        if (jsonreader == NULL) {
            break;
        }

        jsonreader->policy_cache = json_object();
        json_object_set_new(jsonreader->policy_cache, "users", json_array());

        jsonreader->policy = json_object();
        json_object_set_new(jsonreader->policy, "users", json_array());

#ifdef __USE_GNU
        /* compile with -D_GNU_SOURCE */
#ifndef NDEBUG
        jsonreader->policy_mutex = (pthread_mutex_t)PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
        jsonreader->policy_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif
#else
        jsonreader->policy_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif

        typedesc = types_from_json(factory, jsonTypeDesc);
        if (NULL == typedesc) {
            qeo_log_e("typedesc == NULL");
            break;
        }

        dc->typedesc        = jsonTypeDesc;
        l.on_data           = qeojson_core_callback_dispatcher;
        l.userdata          = (uintptr_t)dc;
        dc->jsonreader      = jsonreader;
        jsonreader->reader  = qeocore_reader_open(factory,                          // factory
                                                  typedesc,                         // type
                                                  NULL,                             // callback dispatcher
                                                  flags,                            // flags
                                                  &l,                               // callback cookie
                                                  NULL);                            // return code
        qeocore_type_free(typedesc);
        if (NULL == jsonreader->reader) {
            break;
        }

        ret = QEO_OK;
    } while (0);

    if (QEO_OK != ret) {
        json_decref(jsonTypeDesc);
        qeojson_reader_close(jsonreader);
        jsonreader = NULL;
        free(dc);
    }


    return jsonreader;
}

static void qeojson_reader_close(qeojson_reader_t *jsonreader)
{
    json_reader_dispatcher_cookie_t *dc   = NULL;

    if (jsonreader == NULL) {
        return;
    }

    do {
        if (NULL != jsonreader->reader) {
            dc = (json_reader_dispatcher_cookie_t *)qeocore_reader_get_userdata(jsonreader->reader);
            if (dc == NULL) {
                break;
            }

            json_decref(dc->typedesc);
            qeocore_reader_close(jsonreader->reader);
            free(dc);
        }

        json_decref(jsonreader->policy_cache);
        json_decref(jsonreader->policy);


        free(jsonreader);
    } while (0);
}

static qeojson_writer_t *qeojson_writer_open(const qeo_factory_t    *factory,
                                             qeojson_writer_type_t  writer_type,
                                             void                   *listener,
                                             const char             *json_type,
                                             uintptr_t              userdata)
{
    qeo_retcode_t                   ret                 = QEO_EFAIL;
    qeojson_writer_t                *jsonwriter         = NULL;
    qeocore_type_t                  *typedesc           = NULL;
    json_writer_dispatcher_cookie_t *dc                 = NULL;
    json_t                          *jsonTypeDesc       = NULL;
    json_t                          *behavior           = NULL;
    const char                      *expected_behaviour = NULL;
    int                             flags               = QEOCORE_EFLAG_NONE;
    qeocore_writer_listener_t       l = { 0 };

    if ((NULL == factory) || (NULL == json_type)) {
        return NULL;
    }


    do {
        dc = calloc(1, sizeof(json_writer_dispatcher_cookie_t));
        if (NULL == dc) {
            break;
        }

        dc->userdata = userdata;

        switch (writer_type) {
            case QEOJSON_WRITER_TYPE_EVENT_WRITER:
                flags = QEOCORE_EFLAG_EVENT_DATA;
                expected_behaviour = "event";
                qeo_json_event_writer_listener_t *event_listener = (qeo_json_event_writer_listener_t *) listener;
                dc->etype           = QEO_ETYPE_EVENT_DATA;
                dc->listener.event  = *event_listener;
                if (NULL != event_listener->on_policy_update) {
                    l.on_policy_update = qeojson_core_writer_policy_update_callback_dispatcher;
                }
                break;

            case QEOJSON_WRITER_TYPE_STATE_WRITER:
                flags = QEOCORE_EFLAG_STATE_DATA;
                expected_behaviour = "state";
                qeo_json_state_writer_listener_t *state_listener = (qeo_json_state_writer_listener_t *)listener;
                dc->etype           = QEO_ETYPE_STATE_DATA;
                dc->listener.state  = *state_listener;
                if (NULL != state_listener->on_policy_update) {
                    l.on_policy_update = qeojson_core_writer_policy_update_callback_dispatcher;
                }

                break;
        }

        jsonTypeDesc = json_loads(json_type, 0, NULL);
        if (NULL == jsonTypeDesc) {
            break;
        }

        if (!json_is_object(jsonTypeDesc)) {
            break;
        }

        behavior = json_object_get(jsonTypeDesc, KEY_BEHAVIOR);

        if (!json_is_string(behavior)) {
            break;
        }


        if (0 != strcmp(json_string_value(behavior), expected_behaviour)) {
            qeo_log_e("not matching expected behaviour %s", expected_behaviour);
            break;
        }

        jsonwriter = calloc(1, sizeof(qeojson_writer_t));
        if (jsonwriter == NULL) {
            break;
        }

        jsonwriter->policy_cache = json_object();
        json_object_set_new(jsonwriter->policy_cache, "users", json_array());

        jsonwriter->policy = json_object();
        json_object_set_new(jsonwriter->policy, "users", json_array());

#ifdef __USE_GNU
        /* compile with -D_GNU_SOURCE */
#ifndef NDEBUG
        jsonwriter->policy_mutex = (pthread_mutex_t)PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
        jsonwriter->policy_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif
#else
        jsonwriter->policy_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif

        typedesc = types_from_json(factory, jsonTypeDesc);
        if (NULL == typedesc) {
            qeo_log_e("typedesc == NULL");
            break;
        }

        dc->typedesc        = jsonTypeDesc;
        l.userdata          = (uintptr_t)dc;
        dc->jsonwriter      = jsonwriter;
        jsonwriter->writer  = qeocore_writer_open(factory,                          // factory
                                                  typedesc,                         // type
                                                  NULL,                             // callback dispatcher
                                                  flags,                            // flags
                                                  &l,                               // callback cookie
                                                  NULL);                            // return code
        qeocore_type_free(typedesc);
        if (NULL == jsonwriter->writer) {
            break;
        }


        ret = QEO_OK;
    } while (0);


    if (QEO_OK != ret) {
        json_decref(jsonTypeDesc);
        qeojson_writer_close(jsonwriter);
        jsonwriter = NULL;
        free(dc);
    }


    return jsonwriter;
}

static void qeojson_writer_close(qeojson_writer_t *jsonwriter)
{
    json_writer_dispatcher_cookie_t *dc   = NULL;

    if (jsonwriter == NULL) {
        return;
    }

    do {
        if (NULL != jsonwriter->writer) {
            dc = (json_writer_dispatcher_cookie_t *)qeocore_writer_get_userdata(jsonwriter->writer);
            if (dc == NULL) {
                break;
            }

            if (NULL != dc->typedesc) {
                json_decref(dc->typedesc);
                dc->typedesc = NULL;
            }
            qeocore_writer_close(jsonwriter->writer);
            free(dc);
        }

        if (NULL != jsonwriter->policy_cache) {
            json_decref(jsonwriter->policy_cache);
        }

        if (NULL != jsonwriter->policy) {
            json_decref(jsonwriter->policy);
        }

        free(jsonwriter);
    } while (0);
}

static qeo_retcode_t qeo_json_factory_init(qeo_factory_t                      *factory,
                                           const qeo_json_factory_listener_t  *listener,
                                           uintptr_t                          userdata)
{
    qeo_retcode_t result = QEO_EFAIL;

    if ((NULL != factory) && (NULL != listener)) {
        factory_dispatcher_cookie *dc = calloc(1, sizeof(factory_dispatcher_cookie));
        if (NULL != dc) {
            dc->listener  = listener;
            dc->userdata  = userdata;
            result        = qeocore_factory_set_user_data(factory, (uintptr_t) dc);
            if (QEO_OK == result) {
                result = qeocore_factory_init(factory, &_listener);
            }
        }

        if ((NULL != dc) && (QEO_OK != result)) {
            free(dc);
        }
    }
    else {
        result = QEO_EINVAL;
    }
    return result;
}

/*########################################################################
#                                                                       #
#  PUBLIC FUNCTION IMPLEMENTATION                                       #
#                                                                       #
########################################################################*/

qeo_retcode_t qeo_json_factory_create(const qeo_json_factory_listener_t *listener, uintptr_t userdata)
{
    qeo_factory_t *factory = NULL;

    factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT);

    return qeo_json_factory_init(factory, listener, userdata);
}

qeo_retcode_t qeo_json_factory_create_by_id(const char                        *identity,
                                            const qeo_json_factory_listener_t *listener,
                                            uintptr_t                         userdata)
{
    qeo_factory_t   *factory  = NULL;
    qeo_retcode_t   result    = QEO_EFAIL;
    qeo_identity_t  *id       = NULL;

    result = qeo_identity_from_json(&id, identity);

    if (QEO_OK == result) {
        factory = qeocore_factory_new(id);
        result  = qeo_json_factory_init(factory, listener, userdata);
        //free(id->url); //HACK ALERT : To be put back once qeo_identity_from_json will actually allocate a url string in the identity struct
    }
    return result;
}

void qeo_json_factory_close(qeo_factory_t *factory)
{
    if (NULL != factory) {
        uintptr_t userdata;
        qeocore_factory_get_user_data(factory, &userdata);
        factory_dispatcher_cookie *dc = (factory_dispatcher_cookie *)userdata;
        qeocore_factory_close(factory);
        free(dc);
    }
}

qeo_json_event_reader_t *qeo_json_factory_create_event_reader(const qeo_factory_t                     *factory,
                                                              const char                              *json_type,
                                                              const qeo_json_event_reader_listener_t  *listener,
                                                              uintptr_t                               userdata)
{
    qeo_json_event_reader_t *reader     = NULL;
    qeojson_reader_t        *jsonreader = NULL;

    if ((NULL == factory) || (NULL == json_type) || (NULL == listener) || (NULL == listener->on_data)) {
        return NULL;
    }


    do {
        jsonreader = qeojson_reader_open(
            factory,
            QEOJSON_READER_TYPE_EVENT_READER,
            (void *) listener,
            json_type,
            userdata);

        if (jsonreader == NULL) {
            qeo_log_e("qeojson_reader_open failed");
            break;
        }

        reader = (qeo_json_event_reader_t *) jsonreader;
    } while (0);

    return reader;
}

qeo_retcode_t qeo_json_event_reader_enable(qeo_json_event_reader_t *reader)
{
    qeo_retcode_t     ret         = QEO_EINVAL;
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;

    do {
        if (reader == NULL) {
            break;
        }

        ret = qeocore_reader_enable(jsonreader->reader);
        if (ret != QEO_OK) {
            break;
        }

        ret = QEO_OK;
    } while (0);

    return ret;
}

void qeo_json_event_reader_close(qeo_json_event_reader_t *reader)
{
    qeojson_reader_close((qeojson_reader_t *)reader);
}

qeo_retcode_t qeo_json_event_reader_policy_update(const qeo_json_event_reader_t *reader, const char *json_policy)
{
    qeo_retcode_t     rc          = QEO_EINVAL;
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;


    if (NULL == reader) {
        return rc;
    }

    do {
        if (NULL != json_policy) {
            qeojson_policy_lock(&jsonreader->policy_mutex);
            rc = qeojson_policy_update(jsonreader->policy_cache, json_policy);
            qeojson_policy_unlock(&jsonreader->policy_mutex);
            if (rc != QEO_OK) {
                qeo_log_e("qeojson_policy_update failed");
                break;
            }
        }

        rc = qeocore_reader_policy_update(jsonreader->reader);
    } while (0);

    return rc;
}

qeo_json_event_writer_t *qeo_json_factory_create_event_writer(const qeo_factory_t                     *factory,
                                                              const char                              *json_type,
                                                              const qeo_json_event_writer_listener_t  *listener,
                                                              uintptr_t                               userdata)
{
    qeo_json_event_writer_t *writer     = NULL;
    qeojson_writer_t        *jsonwriter = NULL;

    if ((NULL == factory) || (json_type == NULL)) {
        return NULL;
    }

    do {
        jsonwriter = qeojson_writer_open(
            factory,
            QEOJSON_WRITER_TYPE_EVENT_WRITER,
            (void *) listener,
            json_type,
            userdata);

        if (jsonwriter == NULL) {
            qeo_log_e("qeojson_writer_open failed");
            break;
        }

        writer = (qeo_json_event_writer_t *) jsonwriter;
    } while (0);

    return writer;
}

qeo_retcode_t qeo_json_event_writer_enable(qeo_json_event_writer_t *writer)
{
    qeo_retcode_t     ret         = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;

    do {
        if (writer == NULL) {
            break;
        }

        ret = qeocore_writer_enable(jsonwriter->writer);
        if (ret != QEO_OK) {
            break;
        }

        ret = QEO_OK;
    } while (0);

    return ret;
}

void qeo_json_event_writer_close(qeo_json_event_writer_t *writer)
{
    qeojson_writer_close((qeojson_writer_t *)writer);
}

qeo_retcode_t qeo_json_event_writer_write(const qeo_json_event_writer_t *writer, const char *json_data)
{
    qeo_retcode_t     result      = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;
    json_t            *jsonData   = json_loads(json_data, 0, NULL);

    if ((NULL != jsonData) && (NULL != writer)) {
        json_writer_dispatcher_cookie_t *dc   = (json_writer_dispatcher_cookie_t *)qeocore_writer_get_userdata(jsonwriter->writer);
        qeocore_data_t                  *data = qeocore_writer_data_new(jsonwriter->writer);
        if (QEO_OK == data_from_json(dc->typedesc, jsonData, data)) {
            result = qeocore_writer_write(jsonwriter->writer, data);
        }
        qeocore_data_free(data);
        json_decref(jsonData);
    }
    return result;
}

qeo_retcode_t qeo_json_event_writer_policy_update(const qeo_json_event_writer_t *writer, const char *json_policy)
{
    qeo_retcode_t     rc          = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;

    if (NULL == writer) {
        return rc;
    }

    do {
        if (NULL != json_policy) {
            qeojson_policy_lock(&jsonwriter->policy_mutex);
            rc = qeojson_policy_update(jsonwriter->policy_cache, json_policy);
            qeojson_policy_unlock(&jsonwriter->policy_mutex);
            if (rc != QEO_OK) {
                qeo_log_e("qeojson_policy_update failed");
                break;
            }
        }

        rc = qeocore_writer_policy_update(jsonwriter->writer);
    } while (0);

    return rc;
}

qeo_json_state_reader_t *qeo_json_factory_create_state_reader(const qeo_factory_t                     *factory,
                                                              const char                              *json_type,
                                                              const qeo_json_state_reader_listener_t  *listener,
                                                              uintptr_t                               userdata)
{
    qeo_json_state_reader_t *reader     = NULL;
    qeojson_reader_t        *jsonreader = NULL;

    if ((NULL == json_type) || (NULL == factory) || (NULL == listener) || (NULL == listener->on_update)) {
        return NULL;
    }

    do {
        jsonreader = qeojson_reader_open(
            factory,
            QEOJSON_READER_TYPE_STATE_READER,
            (void *) listener,
            json_type,
            userdata);

        if (jsonreader == NULL) {
            qeo_log_e("qeojson_reader_open failed");
            break;
        }

        reader = (qeo_json_state_reader_t *) jsonreader;
    } while (0);

    return reader;
}

qeo_retcode_t qeo_json_state_reader_enable(qeo_json_state_reader_t *reader)
{
    qeo_retcode_t     ret         = QEO_EINVAL;
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;

    do {
        if (reader == NULL) {
            break;
        }

        ret = qeocore_reader_enable(jsonreader->reader);
        if (ret != QEO_OK) {
            break;
        }

        ret = QEO_OK;
    } while (0);

    return ret;
}

void qeo_json_state_reader_close(qeo_json_state_reader_t *reader)
{
    qeojson_reader_close((qeojson_reader_t *)reader);
}

qeo_retcode_t qeo_json_state_reader_foreach(const qeo_json_state_reader_t *reader,
                                            qeo_json_iterate_callback     cb,
                                            uintptr_t                     userdata)
{
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;
    qeo_retcode_t     rc          = QEO_OK;
    char              *datastr    = NULL;

    if ((NULL == reader) || (NULL == cb)) {
        rc = QEO_EINVAL;
    }
    else {
        qeocore_filter_t      filter  = { 0 };
        qeo_iterate_action_t  action  = QEO_ITERATE_CONTINUE;
        qeocore_data_t        *data;

        data = qeocore_reader_data_new(jsonreader->reader);
        if (NULL == data) {
            rc = QEO_ENOMEM;
        }
        else {
            json_reader_dispatcher_cookie_t *dc = (json_reader_dispatcher_cookie_t *)qeocore_reader_get_userdata(jsonreader->reader);
            filter.instance_handle = DDS_HANDLE_NIL;
            while (1) {
                rc = qeocore_reader_read(jsonreader->reader, &filter, data);
                if (QEO_OK == rc) {
                    filter.instance_handle = qeocore_data_get_instance_handle(data);
                    datastr = json_from_data(dc->typedesc, data);
                    action  = cb(datastr, userdata);
                    free(datastr);
                    if (QEO_ITERATE_CONTINUE == action) {
                        qeocore_data_reset(data);
                        continue;
                    }
                }
                else if (QEO_ENODATA == rc) {
                    rc = QEO_OK;
                }
                /* QEO_ENODATA or ITERATE_ABORT or error */
                break;
            }
            qeocore_data_free(data);
        }
    }
    return rc;
}

qeo_retcode_t qeo_json_state_reader_policy_update(const qeo_json_state_reader_t *reader, const char *json_policy)
{
    qeo_retcode_t     rc          = QEO_EINVAL;
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;

    if (NULL == reader) {
        return rc;
    }

    do {
        if (NULL != json_policy) {
            qeojson_policy_lock(&jsonreader->policy_mutex);
            rc = qeojson_policy_update(jsonreader->policy_cache, json_policy);
            qeojson_policy_unlock(&jsonreader->policy_mutex);
            if (rc != QEO_OK) {
                qeo_log_e("qeojson_policy_update failed");
                break;
            }
        }

        rc = qeocore_reader_policy_update(jsonreader->reader);
    } while (0);

    return rc;
}

qeo_json_state_change_reader_t *qeo_json_factory_create_state_change_reader(const qeo_factory_t                           *factory,
                                                                            const char                                    *json_type,
                                                                            const qeo_json_state_change_reader_listener_t *listener,
                                                                            uintptr_t                                     userdata)
{
    qeo_json_state_change_reader_t  *reader     = NULL;
    qeojson_reader_t                *jsonreader = NULL;

    if ((NULL == json_type) || (NULL == factory) || (NULL == listener) ||
        ((NULL == listener->on_data) && (NULL == listener->on_remove))) {
        return NULL;
    }

    do {
        jsonreader = qeojson_reader_open(
            factory,
            QEOJSON_READER_TYPE_STATE_CHANGE_READER,
            (void *) listener,
            json_type,
            userdata);

        if (jsonreader == NULL) {
            qeo_log_e("qeojson_reader_open failed");
            break;
        }

        reader = (qeo_json_state_change_reader_t *) jsonreader;
    } while (0);

    return reader;
}

qeo_retcode_t qeo_json_state_change_reader_enable(qeo_json_state_change_reader_t *reader)
{
    qeo_retcode_t     ret         = QEO_EINVAL;
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;

    do {
        if (reader == NULL) {
            break;
        }

        ret = qeocore_reader_enable(jsonreader->reader);
        if (ret != QEO_OK) {
            break;
        }

        ret = QEO_OK;
    } while (0);

    return ret;
}

void qeo_json_state_change_reader_close(qeo_json_state_change_reader_t *reader)
{
    qeojson_reader_close((qeojson_reader_t *)reader);
}

qeo_retcode_t qeo_json_state_change_reader_policy_update(const qeo_json_state_change_reader_t *reader, const char *json_policy)
{
    qeo_retcode_t     rc          = QEO_EINVAL;
    qeojson_reader_t  *jsonreader = (qeojson_reader_t *) reader;

    if (NULL == reader) {
        return rc;
    }

    do {
        if (NULL != json_policy) {
            qeojson_policy_lock(&jsonreader->policy_mutex);
            rc = qeojson_policy_update(jsonreader->policy_cache, json_policy);
            qeojson_policy_unlock(&jsonreader->policy_mutex);
            if (rc != QEO_OK) {
                qeo_log_e("qeojson_policy_update failed");
                break;
            }
        }

        rc = qeocore_reader_policy_update(jsonreader->reader);
    } while (0);

    return rc;
}

qeo_json_state_writer_t *qeo_json_factory_create_state_writer(const qeo_factory_t                     *factory,
                                                              const char                              *json_type,
                                                              const qeo_json_state_writer_listener_t  *listener,
                                                              uintptr_t                               userdata)
{
    qeo_json_state_writer_t *writer     = NULL;
    qeojson_writer_t        *jsonwriter = NULL;

    if ((NULL == factory) || (json_type == NULL)) {
        return NULL;
    }

    do {
        jsonwriter = qeojson_writer_open(
            factory,
            QEOJSON_WRITER_TYPE_STATE_WRITER,
            (void *) listener,
            json_type,
            userdata);

        if (jsonwriter == NULL) {
            qeo_log_e("qeojson_writer_open failed");
            break;
        }

        writer = (qeo_json_state_writer_t *) jsonwriter;
    } while (0);

    return writer;
}

qeo_retcode_t qeo_json_state_writer_enable(qeo_json_state_writer_t *writer)
{
    qeo_retcode_t     ret         = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;

    do {
        if (writer == NULL) {
            break;
        }

        ret = qeocore_writer_enable(jsonwriter->writer);
        if (ret != QEO_OK) {
            break;
        }

        ret = QEO_OK;
    } while (0);

    return ret;
}

void qeo_json_state_writer_close(qeo_json_state_writer_t *writer)
{
    qeojson_writer_close((qeojson_writer_t *)writer);
}

qeo_retcode_t qeo_json_state_writer_write(const qeo_json_state_writer_t *writer, const char *json_data)
{
    qeo_retcode_t     result      = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;
    json_t            *jsonData   = json_loads(json_data, 0, NULL);

    if ((NULL != jsonData) && (NULL != writer)) {
        json_writer_dispatcher_cookie_t *dc   = (json_writer_dispatcher_cookie_t *)qeocore_writer_get_userdata(jsonwriter->writer);
        qeocore_data_t                  *data = qeocore_writer_data_new(jsonwriter->writer);
        if (QEO_OK == data_from_json(dc->typedesc, jsonData, data)) {
            result = qeocore_writer_write(jsonwriter->writer, data);
        }
        qeocore_data_free(data);
        json_decref(jsonData);
    }
    return result;
}

qeo_retcode_t qeo_json_state_writer_remove(const qeo_json_state_writer_t *writer, const char *json_data)
{
    qeo_retcode_t     result      = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;
    json_t            *jsonData   = json_loads(json_data, 0, NULL);

    if ((NULL != jsonData) && (NULL != writer)) {
        json_writer_dispatcher_cookie_t *dc   = (json_writer_dispatcher_cookie_t *)qeocore_writer_get_userdata(jsonwriter->writer);
        qeocore_data_t                  *data = qeocore_writer_data_new(jsonwriter->writer);
        if (QEO_OK == data_from_json(dc->typedesc, jsonData, data)) {
            result = qeocore_writer_remove(jsonwriter->writer, data);
        }
        qeocore_data_free(data);
        json_decref(jsonData);
    }
    return result;
}

qeo_retcode_t qeo_json_state_writer_policy_update(const qeo_json_state_writer_t *writer, const char *json_policy)
{
    qeo_retcode_t     rc          = QEO_EINVAL;
    qeojson_writer_t  *jsonwriter = (qeojson_writer_t *) writer;

    if (NULL == writer) {
        return rc;
    }

    do {
        if (NULL != json_policy) {
            qeojson_policy_lock(&jsonwriter->policy_mutex);
            rc = qeojson_policy_update(jsonwriter->policy_cache, json_policy);
            qeojson_policy_unlock(&jsonwriter->policy_mutex);
            if (rc != QEO_OK) {
                qeo_log_e("qeojson_policy_update failed");
                break;
            }
        }

        rc = qeocore_writer_policy_update(jsonwriter->writer);
    } while (0);

    return rc;
}

qeo_retcode_t qeo_json_get_device_id(char **json_device_id)
{
    qeo_retcode_t                   ret           = QEO_EINVAL;
    const qeo_platform_device_info  *qeo_dev_info = NULL;
    json_t                          *device_id    = NULL;
    char                            *upper        = NULL;
    char                            *lower        = NULL;

    do {
        if (NULL == json_device_id) {
            qeo_log_e("NULL == json_device_id");
            break;
        }

        qeo_dev_info = qeo_platform_get_device_info();
        if (qeo_dev_info == NULL) {
            qeo_log_d("qeo_platform_get_device_info failed");
            ret = QEO_EFAIL;
            break;
        }

        device_id = json_object();
        if (NULL == device_id) {
            qeo_log_e("unable to create new JSON object");
            break;
        }

        if (-1 == asprintf(&upper, "%" PRId64, qeo_dev_info->qeoDeviceId.upperId)) {
            qeo_log_e("asprintf failed");
            break;
        }

        if (-1 == asprintf(&lower, "%" PRId64, qeo_dev_info->qeoDeviceId.lowerId)) {
            qeo_log_e("asprintf failed");
            break;
        }

        if (0 != json_object_set_new(device_id, "upper", json_string(upper))) {
            qeo_log_e("json_object_set_new failed");
            break;
        }

        if (0 != json_object_set_new(device_id, "lower", json_string(lower))) {
            qeo_log_e("json_object_set_new failed");
            break;
        }

        *json_device_id = json_dumps(device_id, JSON_INDENT(4));

        ret = QEO_OK;
    } while (0);

    free(upper);
    free(lower);

    return ret;
}
