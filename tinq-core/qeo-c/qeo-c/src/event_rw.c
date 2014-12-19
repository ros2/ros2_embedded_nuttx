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
#include <stdlib.h>

#include <qeo/api.h>
#include <qeocore/api.h>

#include "callback.h"

/* ===[ Event reader ]======================================================= */

qeo_event_reader_t *qeo_factory_create_event_reader(const qeo_factory_t *factory,
                                                    const DDS_TypeSupport_meta *type,
                                                    const qeo_event_reader_listener_t *listener,
                                                    uintptr_t userdata)
{
    qeo_event_reader_t *reader = NULL;

    if ((NULL != factory) && (NULL != type) && (NULL != listener) && (NULL != listener->on_data)) {
        reader_dispatcher_cookie *dc = calloc(1, sizeof(reader_dispatcher_cookie));

        if (NULL != dc) {
            qeocore_type_t *t = qeocore_type_register_tsm(factory, type, type->name);

            if (NULL != t) {
                qeocore_reader_listener_t l = { 0 };

                dc->etype = QEO_ETYPE_EVENT_DATA;
                dc->listener.event = *listener;
                dc->userdata = userdata;
                l.on_data = core_callback_dispatcher;
                if (NULL != listener->on_policy_update) {
                    l.on_policy_update = core_reader_policy_update_callback_dispatcher;
                }
                l.userdata = (uintptr_t)dc;
                reader = (qeo_event_reader_t *)qeocore_reader_open(factory, t, NULL,
                                                                   QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE,
                                                                   &l, NULL);
                qeocore_type_free(t);
            }
            if (NULL == reader) {
                free(dc);
            }
        }
    }
    return reader;
}

void qeo_event_reader_close(qeo_event_reader_t *reader)
{
    if (NULL != reader) {
        reader_dispatcher_cookie *dc = (reader_dispatcher_cookie *)qeocore_reader_get_userdata((qeocore_reader_t *)reader);

        qeocore_reader_close((qeocore_reader_t *)reader);
        free(dc);
    }
}

qeo_retcode_t qeo_event_reader_policy_update(const qeo_event_reader_t *reader)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if (NULL != reader) {
        rc = qeocore_reader_policy_update((qeocore_reader_t *)reader);
    }
    return rc;
}

/* ===[ Event writer ]======================================================= */

qeo_event_writer_t *qeo_factory_create_event_writer(const qeo_factory_t *factory,
                                                    const DDS_TypeSupport_meta *type,
                                                    const qeo_event_writer_listener_t *listener,
                                                    uintptr_t userdata)
{
    qeo_event_writer_t *writer = NULL;

    if ((NULL != factory) && (NULL != type) && ((NULL == listener) || (NULL != listener->on_policy_update))) {
        qeocore_writer_listener_t l = { 0 }, *pl = NULL;
        writer_dispatcher_cookie *dc = NULL;
        qeo_retcode_t rc = QEO_OK;
        qeocore_type_t *t = NULL;

        if (NULL != listener) {
            dc = calloc(1, sizeof(writer_dispatcher_cookie));
            if (NULL != dc) {
                dc->etype = QEO_ETYPE_EVENT_DATA;
                dc->listener.event = *listener;
                dc->userdata = userdata;
                l.on_policy_update = core_writer_policy_update_callback_dispatcher;
                l.userdata = (uintptr_t)dc;
                pl = &l;
            }
            else {
                rc = QEO_ENOMEM;
            }
        }
        if (QEO_OK == rc) {
            t = qeocore_type_register_tsm(factory, type, type->name);
            if (NULL != t) {
                writer = (qeo_event_writer_t *)qeocore_writer_open(factory, t, NULL,
                                                                   QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE,
                                                                   pl, NULL);
                qeocore_type_free(t);
            }
            if (NULL == writer) {
                free(dc);
            }
        }
    }
    return writer;
}

qeo_retcode_t qeo_event_writer_write(const qeo_event_writer_t *writer,
                                     const void *data)
{
    const qeocore_writer_t *w = (qeocore_writer_t *)writer;
    qeo_retcode_t rc = QEO_OK;

    if ((NULL == w) || (NULL == data)) {
        rc = QEO_EINVAL;
    }
    else {
        rc  = qeocore_writer_write(w, data);
    }
    return rc;
}

void qeo_event_writer_close(qeo_event_writer_t *writer)
{
    if (NULL != writer) {
        writer_dispatcher_cookie *dc = (writer_dispatcher_cookie *)qeocore_writer_get_userdata((qeocore_writer_t *)writer);

        qeocore_writer_close((qeocore_writer_t *)writer);
        free(dc);
    }
}

qeo_retcode_t qeo_event_writer_policy_update(const qeo_event_writer_t *writer)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if (NULL != writer) {
        rc = qeocore_writer_policy_update((qeocore_writer_t *)writer);
    }
    return rc;
}
