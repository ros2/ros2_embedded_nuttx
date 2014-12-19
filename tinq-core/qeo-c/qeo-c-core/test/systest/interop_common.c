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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "interop_common.h"
#include "dyn_types.h"
#include "tsm_types.h"
#include "verbose.h"

static void validate_data(const types_t *t,
                          int key_only);

/* ===[ static test code ]=================================================== */

static qeocore_type_t *static_get_type(qeo_factory_t *factory,
                                       const DDS_TypeSupport_meta *tsm)
{
    qeocore_type_t *type;

    assert(NULL != (type = qeocore_type_register_tsm(factory, tsm, tsm->name)));
    return type;
}

static void *static_get_data(const qeocore_writer_t *writer)
{
    return &_types3;
}

static void static_free_data(void *data)
{
    /*nop*/
}

static void static_validate_data(const qeocore_data_t *data,
                                 int key_only)
{
    types_t *t;

    assert(NULL != (t = (types_t *)qeocore_data_get_data(data)));
    validate_data(t, key_only);
}

vtable_t _vtable_static = {
    .get_type      = static_get_type,
    .get_data      = static_get_data,
    .free_data     = static_free_data,
    .validate_data = static_validate_data,
};

/* ===[ dynamic test code ]================================================== */

static qeocore_type_t *dynamic_get_type(qeo_factory_t *factory,
                                        const DDS_TypeSupport_meta *tsm)
{
    qeocore_type_t *type;

    assert(NULL != (type = types_get(tsm)));
    assert(QEO_OK == qeocore_type_register(factory, type, tsm->name));
    return type;
}

static void *dynamic_get_data(const qeocore_writer_t *writer)
{
    types_t *t = static_get_data(NULL);
    qeocore_data_t *data;
    qeocore_data_t *seqdata;
    qeocore_data_t *innerdata;
    const char *s = "dynamic";

    assert(NULL != (data = qeocore_writer_data_new(writer)));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_STRING], &s));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_OTHER], &t->other));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_I8], &t->i8));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_I16], &t->i16));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_I32], &t->i32));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_I64], &t->i64));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_F32], &t->f32));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_BOOL], &t->boolean));
    /* the sequence */
    assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_A8], &seqdata));
    assert(QEO_OK == qeocore_data_sequence_set(seqdata, (const qeo_sequence_t *)&t->a8, 0));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_A8], &seqdata));
    qeocore_data_free(seqdata);
    /* the enumeration */
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_E], &t->e));
    /* the enumeration sequence */
    assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_AE], &seqdata));
    assert(QEO_OK == qeocore_data_sequence_set(seqdata, (const qeo_sequence_t *)&t->ae, 0));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_AE], &seqdata));
    qeocore_data_free(seqdata);
    /* the structure */
    assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_INNER], &innerdata));
    assert(QEO_OK == qeocore_data_set_member(innerdata, _member_id[M_INNER_I32], &t->inner_struct.inner_i32));
    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_INNER], &innerdata));
    qeocore_data_free(innerdata);
    return data;
}

static void dynamic_free_data(void *data)
{
    qeocore_data_free((qeocore_data_t *)data);
}

static void dynamic_validate_data(const qeocore_data_t *data,
                                  int key_only)
{
    types_t t = {};
    qeocore_data_t *seqdata_a8 = NULL;
    qeocore_data_t *seqdata_ae = NULL;
    qeocore_data_t *innerdata = NULL;

    assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_STRING], &t.string));
    if (!key_only) {
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_OTHER], &t.other));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_I8], &t.i8));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_I16], &t.i16));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_I32], &t.i32));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_I64], &t.i64));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_F32], &t.f32));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_BOOL], &t.boolean));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_A8], &seqdata_a8));
        assert(QEO_OK == qeocore_data_sequence_get(seqdata_a8, (qeo_sequence_t *)&t.a8, 0, QEOCORE_SIZE_UNLIMITED));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_E], &t.e));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_AE], &seqdata_ae));
        assert(QEO_OK == qeocore_data_sequence_get(seqdata_ae, (qeo_sequence_t *)&t.ae, 0, QEOCORE_SIZE_UNLIMITED));
        assert(QEO_OK == qeocore_data_get_member(data, _member_id[M_INNER], &innerdata));
        assert(QEO_OK == qeocore_data_get_member(innerdata, _member_id[M_INNER_I32], &t.inner_struct.inner_i32));
        qeocore_data_free(innerdata);
    }
    validate_data(&t, key_only);
    if (!key_only) {
        qeocore_data_sequence_free(seqdata_a8, (qeo_sequence_t *)&t.a8);
        qeocore_data_free(seqdata_a8);
        qeocore_data_sequence_free(seqdata_ae, (qeo_sequence_t *)&t.ae);
        qeocore_data_free(seqdata_ae);
        free(t.other);
    }
    free(t.string);
}

vtable_t _vtable_dynamic = {
    .get_type      = dynamic_get_type,
    .get_data      = dynamic_get_data,
    .free_data     = dynamic_free_data,
    .validate_data = dynamic_validate_data,
};

/* ===[ main test code ]===================================================== */

static void validate_data(const types_t *t,
                          int key_only)
{
    types_t *exp_t = static_get_data(NULL);
    int i;

    assert((0 == strcmp(exp_t->string, t->string)) || (0 == strcmp("dynamic", t->string)));
    if (!key_only) {
        assert(0 == strcmp(exp_t->other, t->other));
        assert(exp_t->i8 == t->i8);
        assert(exp_t->i16 == t->i16);
        assert(exp_t->i32 == t->i32);
        assert(exp_t->i64 == t->i64);
        assert(exp_t->f32 == t->f32);
        assert(exp_t->boolean == t->boolean);
        assert(DDS_SEQ_LENGTH(exp_t->a8) == DDS_SEQ_LENGTH(t->a8));
        DDS_SEQ_FOREACH(t->a8, i) {
            assert(DDS_SEQ_ITEM(exp_t->a8, i) == DDS_SEQ_ITEM(t->a8, i));
        }
        assert(exp_t->e == t->e);
        assert(DDS_SEQ_LENGTH(exp_t->ae) == DDS_SEQ_LENGTH(t->ae));
        DDS_SEQ_FOREACH(t->ae, i) {
            assert(DDS_SEQ_ITEM(exp_t->ae, i) == DDS_SEQ_ITEM(t->ae, i));
        }
        assert(exp_t->inner_struct.inner_i32 == t->inner_struct.inner_i32);
    }
    log_pid("received data '%s' (key_only = %d)", t->string, key_only);
}

void on_data_available(const qeocore_reader_t *reader,
                       const qeocore_data_t *data,
                       uintptr_t userdata)
{
    vtable_t *vtable = (vtable_t *)userdata;

    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            vtable->validate_data(data, 0);
            sem_post(&vtable->sync); /* release main thread */
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            /* ignore */
            break;
        case QEOCORE_REMOVE:
            vtable->validate_data(data, 1);
            sem_post(&vtable->sync); /* release main thread */
            break;
        default:
            abort();
            break;
    }
}

void do_test(qeo_factory_t *factory,
             int flags,
             vtable_t *vtable)
{
    qeocore_type_t *type;
    qeocore_writer_t *writer;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available,
                                           .userdata = (uintptr_t)vtable };
    void *data;

    /* initialize */
    type = vtable->get_type(factory, _tsm_types);
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, flags, &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, flags, NULL, NULL)));
    log_pid("initialized");
    /* only in case we are testing events, we have to wait until the remote writers/readers are discovered.
     * we sleep for 10 seconds because if we run with valgrind, discovery is a bit slower. */
    if (!(flags & QEOCORE_EFLAG_STATE)) {
        log_pid("sleeping for discovery");
        sleep(2); /* wait for mutual discovery */
    }
    /* send sample (static) and wait for reception on both readers */
    data = vtable->get_data(writer);
    log_pid("writing");
    qeocore_writer_write(writer, data);
    log_pid("wait for reception");
    sem_wait(&vtable->sync);
    sem_wait(&vtable->sync);
    /* remove sample and wait for reception from both writers */
    if (flags & QEOCORE_EFLAG_STATE) {
        log_pid("removing");
        qeocore_writer_remove(writer, data);
        log_pid("wait for removal");
        sem_wait(&vtable->sync);
        sem_wait(&vtable->sync);
    }
    /* clean up */
    vtable->free_data(data);
    qeocore_writer_close(writer);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    log_pid("done");
}
