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

#include <qeo/api.h>
#include <qeo/log.h>
#include <qeocore/api.h>

#include "callback.h"

/* ===[ C specific callback dispatching ]==================================== */

static void call_on_update(const qeocore_reader_t *reader,
                           const reader_dispatcher_cookie *dc)
{
    switch (dc->etype) {
        case QEO_ETYPE_STATE_UPDATE:
            if (NULL != dc->listener.state.on_update) {
                dc->listener.state.on_update((const qeo_state_reader_t *)reader,
                                              dc->userdata);
            }
            break;
        case QEO_ETYPE_UNKNOWN:
        case QEO_ETYPE_EVENT_DATA:
        case QEO_ETYPE_STATE_DATA:
            break;
    }
}

static void call_on_no_more_data(const qeocore_reader_t *reader,
                                 const reader_dispatcher_cookie *dc)
{
    switch (dc->etype) {
        case QEO_ETYPE_EVENT_DATA:
            if (NULL != dc->listener.event.on_no_more_data) {
                dc->listener.event.on_no_more_data((const qeo_event_reader_t *)reader, dc->userdata);
            }
            break;
        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_no_more_data) {
                dc->listener.state_change.on_no_more_data((const qeo_state_change_reader_t *)reader, dc->userdata);
            }
            break;
        case QEO_ETYPE_UNKNOWN:
        case QEO_ETYPE_STATE_UPDATE:
            break;
    }
}

static void call_on_data(const qeocore_reader_t *reader,
                         const qeocore_data_t *data,
                         const reader_dispatcher_cookie *dc)
{
    switch (dc->etype) {
        case QEO_ETYPE_EVENT_DATA:
            if (NULL != dc->listener.event.on_data) {
                dc->listener.event.on_data((const qeo_event_reader_t *)reader, qeocore_data_get_data(data),
                                            dc->userdata);
            }
            break;
        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_data) {
                dc->listener.state_change.on_data((const qeo_state_change_reader_t *)reader,
                                                   qeocore_data_get_data(data), dc->userdata);
            }
            break;
        case QEO_ETYPE_UNKNOWN:
        case QEO_ETYPE_STATE_UPDATE:
            break;
    }
}

static void call_on_remove(const qeocore_reader_t *reader,
                           const qeocore_data_t *data,
                           const reader_dispatcher_cookie *dc)
{
    switch (dc->etype) {
        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_remove) {
                dc->listener.state_change.on_remove((const qeo_state_change_reader_t *)reader,
                                                     qeocore_data_get_data(data), dc->userdata);
            }
            break;
        case QEO_ETYPE_UNKNOWN:
        case QEO_ETYPE_EVENT_DATA:
        case QEO_ETYPE_STATE_UPDATE:
            break;
    }
}

void core_callback_dispatcher(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    reader_dispatcher_cookie *dc = (reader_dispatcher_cookie *)userdata;

    switch (qeocore_data_get_status(data)) {
        case QEOCORE_NOTIFY:
            call_on_update(reader, dc);
            break;
        case QEOCORE_DATA:
            call_on_data(reader, data, dc);
            break;
        case QEOCORE_NO_MORE_DATA:
            call_on_no_more_data(reader, dc);
            break;
        case QEOCORE_REMOVE:
            call_on_remove(reader, data, dc);
            break;
        case QEOCORE_ERROR:
            qeo_log_e("no callback called due to prior error");
            break;
    }
}

qeo_policy_perm_t core_reader_policy_update_callback_dispatcher(const qeocore_reader_t *reader,
                                                                const qeo_policy_identity_t *identity,
                                                                uintptr_t userdata)
{
    reader_dispatcher_cookie *dc = (reader_dispatcher_cookie *)userdata;
    qeo_policy_perm_t perm = QEO_POLICY_DENY;

    switch (dc->etype) {
        case QEO_ETYPE_EVENT_DATA:
            if (NULL != dc->listener.event.on_policy_update) {
                perm  = dc->listener.event.on_policy_update((qeo_event_reader_t *)reader, identity, userdata);
            }
            break;
        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_policy_update) {
                perm = dc->listener.state_change.on_policy_update((qeo_state_change_reader_t *)reader, identity,
                                                                   userdata);
            }
            break;
        case QEO_ETYPE_STATE_UPDATE:
            if (NULL != dc->listener.state.on_policy_update) {
                perm = dc->listener.state.on_policy_update((qeo_state_reader_t *)reader, identity, userdata);
            }
            break;
        case QEO_ETYPE_UNKNOWN:
            break;
    }
    return perm;
}

qeo_policy_perm_t core_writer_policy_update_callback_dispatcher(const qeocore_writer_t *writer,
                                                                const qeo_policy_identity_t *identity,
                                                                uintptr_t userdata)
{
    writer_dispatcher_cookie *dc = (writer_dispatcher_cookie *)userdata;
    qeo_policy_perm_t perm = QEO_POLICY_DENY;

    switch (dc->etype) {
        case QEO_ETYPE_EVENT_DATA:
            if (NULL != dc->listener.event.on_policy_update) {
                perm = dc->listener.event.on_policy_update((qeo_event_writer_t *)writer, identity, userdata);
            }
            break;
        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state.on_policy_update) {
                perm = dc->listener.state.on_policy_update((qeo_state_writer_t *)writer, identity, userdata);
            }
            break;
        case QEO_ETYPE_UNKNOWN:
        case QEO_ETYPE_STATE_UPDATE:
            break;
    }
    return perm;
}
