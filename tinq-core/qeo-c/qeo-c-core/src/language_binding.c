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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <dds/dds_aux.h>

#include <qeocore/api.h>
#include <qeo/log.h>

#include "core.h"
#include "entity_store.h"
#include "user_data.h"
#include "samplesupport.h"

/* ---[ data API ]---------------------------------------------------------- */

qeo_retcode_t qeocore_data_reset(qeocore_data_t *data)
{
    qeo_retcode_t rc = QEO_OK;

    if (NULL == data) {
        rc = QEO_EINVAL;
    }
    else {
        core_data_clean(data);
        rc = core_data_init(data, (data->flags.is_writer ? NULL : data->rw.reader),
                            (data->flags.is_writer ? data->rw.writer : NULL));
    }
    return rc;
}

void qeocore_data_free(qeocore_data_t *data)
{
    if (NULL != data) {
        core_data_clean(data);
        free(data);
    }
}

qeo_retcode_t qeocore_data_set_member(qeocore_data_t *data,
                                  qeocore_member_id_t id,
                                  const void *value)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != data) && (NULL != value) && !data->flags.is_tsm_based) {
        rc = data_set_member(data, id, value);
    }
    return rc;
}

qeo_retcode_t qeocore_data_get_member(const qeocore_data_t *data,
                                  qeocore_member_id_t id,
                                  void *value)
{
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != data) && !data->flags.is_tsm_based && (NULL != value) && (DDS_RETCODE_OK == data->ddsrc) &&
        ((1 == DDS_SEQ_LENGTH(data->d.dynamic.seq_data)) || data->flags.is_single)) {
        rc = data_get_member(data, id, value);
    }
    return rc;
}

qeocore_data_status_t qeocore_data_get_status(const qeocore_data_t *data)
{
    qeocore_data_status_t status;

    if (NULL == data) {
        status = QEOCORE_ERROR;
    }
    else if (data->flags.notify_only) {
        status = QEOCORE_NOTIFY;
    }
    else if (DDS_RETCODE_NO_DATA == data->ddsrc) {
        status = QEOCORE_NO_MORE_DATA;
    }
    else if (DDS_RETCODE_OK != data->ddsrc) {
        status = QEOCORE_ERROR;
    }
    else if (DDS_ALIVE_INSTANCE_STATE == info_from_data(data)->instance_state) {
        status = QEOCORE_DATA;
    }
    else {
        status = QEOCORE_REMOVE;
    }
    return status;
}

unsigned int qeocore_data_get_instance_handle(const qeocore_data_t *data)
{
    unsigned int ih = 0;

    if ((NULL != data) && (data->flags.needs_return_loan)) {
        ih = info_from_data(data)->instance_handle;
    }
    return ih;
}

const void *qeocore_data_get_data(const qeocore_data_t *data)
{
    const void *tsm_data = NULL;

    if ((NULL != data) && (data->flags.is_tsm_based)) {
        tsm_data = sample_from_data(data);
    }
    return tsm_data;
}

/* ---[ policy API ]-------------------------------------------------------- */

int64_t qeo_policy_identity_get_uid(const qeo_policy_identity_t *identity)
{
    int64_t uid = -1;

    if (NULL != identity) {
        uid = identity->user_id;
    }
    return uid;
}

/* ---[ reader API ]-------------------------------------------------------- */

static bool valid_keying(int flags,
                         const qeocore_type_t *type)
{
    bool valid = false;

    if (flags & QEOCORE_EFLAG_STATE) {
        /* should be keyed */
        valid = (type->flags.keyed ? true : false);
        if (!valid) {
            qeo_log_e("Error in datatype. This type must contain at least 1 keyed member");
        }
    }
    else if (flags & QEOCORE_EFLAG_EVENT) {
        /* should not be keyed */
        valid = (type->flags.keyed ? false : true);
        if (!valid) {
            qeo_log_e("Error in datatype. This type must not contain keyed members");
        }
    }
    return valid;
}

qeocore_reader_t *qeocore_reader_open(const qeo_factory_t *factory,
                                      qeocore_type_t *type,
                                      const char *topic_name,
                                      int flags,
                                      const qeocore_reader_listener_t *listener,
                                      qeo_retcode_t *prc)
{
    qeocore_reader_t *reader = NULL;
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != factory) && (NULL != type) && type->flags.final && valid_keying(flags, type)) {
        reader = core_create_reader(factory, type, topic_name, flags, listener, prc);
        if (NULL != reader) {
            rc = entity_store_add(&reader->entity);
            if (QEO_OK != rc) {
                qeocore_reader_close(reader);
                reader = NULL;
            }
        }
    }
    if (NULL != prc) {
        *prc = rc;
    }
    return reader;
}

qeo_retcode_t qeocore_reader_enable(qeocore_reader_t *reader)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_NON_NULL(reader);
    if (reader->entity.flags.enabled) {
        rc = QEO_EBADSTATE;
    }
    else {
        rc = core_enable_reader(reader);
    }
    return rc;
}

void qeocore_reader_close(qeocore_reader_t *reader)
{
    if (NULL != reader) {
        entity_store_remove(&reader->entity);
        core_delete_reader(reader, true);
    }
}

uintptr_t qeocore_reader_get_userdata(const qeocore_reader_t *reader)
{
    return (NULL != reader ? reader->listener.userdata : 0);
}

qeocore_data_t *qeocore_reader_data_new(const qeocore_reader_t *reader)
{
    qeocore_data_t *data = NULL;

    if (NULL != reader) {
        data = core_data_alloc(reader, NULL);
    }
    return data;
}

qeo_retcode_t qeocore_reader_read(const qeocore_reader_t *reader,
                              const qeocore_filter_t *filter,
                              qeocore_data_t *data)
{
    qeo_retcode_t rc;

    if ((NULL == reader) || (NULL == data) || (data->rw.reader != reader)) {
        rc  = QEO_EINVAL;
    }
    else if (!reader->entity.flags.enabled) {
        rc = QEO_EBADSTATE;
    }
    else {
        rc = core_read_or_take(reader, filter, data, 0);
    }
    return rc;
}

qeo_retcode_t qeocore_reader_take(const qeocore_reader_t *reader,
                              const qeocore_filter_t *filter,
                              qeocore_data_t *data)
{
    qeo_retcode_t rc;

    if ((NULL == reader) || (NULL == data) || (data->rw.reader != reader)) {
        rc  = QEO_EINVAL;
    }
    else if (!reader->entity.flags.enabled) {
        rc = QEO_EBADSTATE;
    }
    else {
        rc = core_read_or_take(reader, filter, data, 1);
    }
    return rc;
}

qeo_retcode_t qeocore_reader_policy_update(const qeocore_reader_t *reader)
{
    VALIDATE_NON_NULL(reader);
    VALIDATE_NON_NULL(reader->listener.on_policy_update);
    return reader_user_data_update(reader);
}

/* ---[ writer API ]-------------------------------------------------------- */

qeocore_writer_t *qeocore_writer_open(const qeo_factory_t *factory,
                                      qeocore_type_t *type,
                                      const char *topic_name,
                                      int flags,
                                      const qeocore_writer_listener_t *listener,
                                      qeo_retcode_t *prc)
{
    qeocore_writer_t *writer = NULL;
    qeo_retcode_t rc = QEO_EINVAL;

    if ((NULL != factory) && (NULL != type) && type->flags.final && valid_keying(flags, type)) {
        writer = core_create_writer(factory, type, topic_name, flags, listener, prc);
        if (NULL != writer) {
            rc = entity_store_add(&writer->entity);
            if (QEO_OK != rc) {
                qeocore_writer_close(writer);
                writer = NULL;
            }
        }
    }
    if (NULL != prc) {
        *prc = rc;
    }
    return writer;
}

qeo_retcode_t qeocore_writer_enable(qeocore_writer_t *writer)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_NON_NULL(writer);
    if (writer->entity.flags.enabled) {
        rc = QEO_EBADSTATE;
    }
    else {
        rc = core_enable_writer(writer);
    }
    return rc;
}

void qeocore_writer_close(qeocore_writer_t *writer)
{
    if (NULL != writer) {
        entity_store_remove(&writer->entity);
        core_delete_writer(writer, true);
    }
}

uintptr_t qeocore_writer_get_userdata(const qeocore_writer_t *writer)
{
    return (NULL != writer ? writer->listener.userdata : 0);
}

qeocore_data_t *qeocore_writer_data_new(const qeocore_writer_t *writer)
{
    qeocore_data_t *data = NULL;

    if (NULL != writer) {
        data = core_data_alloc(NULL, writer);
    }
    return data;
}

qeo_retcode_t qeocore_writer_write(const qeocore_writer_t *writer,
                               const void *data)
{
    qeo_retcode_t rc = QEO_OK;
    const qeocore_data_t *qeo_data = NULL;
    const void *tsm_data = NULL;

    VALIDATE_NON_NULL(writer);
    VALIDATE_NON_NULL(data);
    if (!writer->entity.flags.enabled) {
        rc = QEO_EBADSTATE;
    }
    else {
        if (writer->entity.type_info->flags.tsm_based) {
            tsm_data = data;
        }
        else {
            qeo_data = (qeocore_data_t *)data;

            if (qeo_data->rw.writer != writer) {
                rc = QEO_EINVAL;
            }
        }
        if (QEO_OK == rc) {
            rc  = core_write(writer, qeo_data, tsm_data);
        }
    }
    return rc;
}

qeo_retcode_t qeocore_writer_remove(const qeocore_writer_t *writer,
                                const void *data)
{
    qeo_retcode_t rc = QEO_OK;
    const qeocore_data_t *qeo_data = NULL;
    const void *tsm_data = NULL;

    VALIDATE_NON_NULL(writer);
    VALIDATE_NON_NULL(data);
    if (!writer->entity.flags.enabled) {
        rc = QEO_EBADSTATE;
    }
    else {
        if (writer->entity.type_info->flags.tsm_based) {
            tsm_data = data;
        }
        else {
            qeo_data = (qeocore_data_t *)data;

            if (qeo_data->rw.writer != writer) {
                rc = QEO_EINVAL;
            }
        }
        if (QEO_OK == rc) {
            rc = core_remove(writer, qeo_data, tsm_data);
        }
    }
    return rc;
}

void qeocore_atexit(const qeocore_exit_cb cb)
{
    if (NULL != cb) {
        DDS_atexit((DDS_exit_cb)cb);
    }
}

qeo_retcode_t qeocore_writer_policy_update(const qeocore_writer_t *writer)
{
    VALIDATE_NON_NULL(writer);
    VALIDATE_NON_NULL(writer->listener.on_policy_update);
    return writer_user_data_update(writer);
}
