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

#ifndef CALLBACK_H_
#define CALLBACK_H_

#include <qeocore/api.h>
#include <jansson.h>
#include <qeojson/api.h>
#include <pthread.h>

typedef enum {
    QEO_ETYPE_UNKNOWN,             /**< unknown entity */
    QEO_ETYPE_EVENT_DATA,          /**< event type entity (data notification) */
    QEO_ETYPE_STATE_DATA,          /**< state type entity (data notification) */
    QEO_ETYPE_STATE_UPDATE,        /**< state type entity (change notification) */
} qeocore_entity_t;


typedef struct {
    qeocore_reader_t *reader;
    json_t          *policy_cache;
    json_t          *policy;
    pthread_mutex_t  policy_mutex;
} qeojson_reader_t;

typedef struct {
    qeocore_writer_t *writer;
    json_t          *policy_cache;
    json_t          *policy;
    pthread_mutex_t  policy_mutex;
} qeojson_writer_t;

typedef struct {
    qeocore_entity_t  etype;
    union {
        qeo_json_event_reader_listener_t        event;
        qeo_json_state_reader_listener_t        state;
        qeo_json_state_change_reader_listener_t state_change;
    }                 listener;
    uintptr_t         userdata;
    json_t            *typedesc;
    qeojson_reader_t  *jsonreader;
} json_reader_dispatcher_cookie_t;

typedef struct {
    qeocore_entity_t  etype;
    union {
        qeo_json_event_writer_listener_t  event;
        qeo_json_state_writer_listener_t  state;
    }                 listener;
    uintptr_t         userdata;
    json_t            *typedesc;
    qeojson_writer_t  *jsonwriter;
} json_writer_dispatcher_cookie_t;

qeo_retcode_t qeojson_policy_update(json_t * policy, const char *json_policy);

void qeojson_policy_lock(pthread_mutex_t *mutex);
void qeojson_policy_unlock(pthread_mutex_t *mutex);

void qeojson_core_callback_dispatcher(const qeocore_reader_t  *reader,
                              const qeocore_data_t    *data,
                              uintptr_t               userdata);

qeo_policy_perm_t qeojson_core_reader_policy_update_callback_dispatcher(const qeocore_reader_t      *reader,
                                                                const qeo_policy_identity_t *identity,
                                                                uintptr_t                   userdata);

qeo_policy_perm_t qeojson_core_writer_policy_update_callback_dispatcher(const qeocore_writer_t      *writer,
                                                                const qeo_policy_identity_t *identity,
                                                                uintptr_t                   userdata);

#endif /* CALLBACK_H_ */
