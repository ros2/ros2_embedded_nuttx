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
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <qeocore/api.h>

#include "dyn_types.h"
#include "common.h"

static qeocore_data_status_t expected_status = QEOCORE_NO_MORE_DATA;

static void validate_data(const qeocore_data_t *data)
{
    qeocore_data_t *inner = NULL;
    int32_t i = 0;

    assert(expected_status == qeocore_data_get_status(data));
    /* check outer struct */
    assert(QEO_OK == qeocore_data_get_member(data, _id_id, &i));
    assert(123 == i);
    if (expected_status == QEOCORE_DATA) {
        /* check inner struct */
        assert(QEO_OK == qeocore_data_get_member(data, _inner_id, &inner));
        assert(QEO_OK == qeocore_data_get_member(inner, _num_id, &i));
        assert(456 == i);
        qeocore_data_free(inner);
    }
}

static void fill_data(qeocore_data_t *data)
{
    qeocore_data_t *inner = NULL;
    int32_t i;

    /* fill outer struct */
    i = 123;
    assert(QEO_OK == qeocore_data_set_member(data, _id_id, &i));
    /* fill inner struct */
    assert(QEO_OK == qeocore_data_get_member(data, _inner_id, &inner));
    i = 456;
    assert(QEO_OK == qeocore_data_set_member(inner, _num_id, &i));
    assert(QEO_OK == qeocore_data_set_member(data, _inner_id, &inner));
    qeocore_data_free(inner);
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeocore_reader_t *reader;
    qeocore_writer_t *writer;
    qeocore_type_t *type;
    qeocore_data_t *rdata, *wdata;
    int i;

    for (i = 0; i < 2; i++) {
        /* initialize */
        assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
        init_factory(factory);
        assert(NULL != (type = nested_type_get(1, 0, 0)));
        assert(QEO_OK == qeocore_type_register(factory, type, "nested"));
        assert(NULL != (reader = qeocore_reader_open(factory,  type, NULL,
                                                     QEOCORE_EFLAG_STATE_UPDATE | QEOCORE_EFLAG_ENABLE, NULL, NULL)));
        assert(NULL != (writer = qeocore_writer_open(factory, type, NULL,
                                                     QEOCORE_EFLAG_STATE_UPDATE | QEOCORE_EFLAG_ENABLE, NULL, NULL)));
        /* write */
        assert(NULL != (wdata = qeocore_writer_data_new(writer)));
        fill_data(wdata);
        assert(QEO_OK == qeocore_writer_write(writer, wdata));
        /* check data */
        assert(NULL != (rdata = qeocore_reader_data_new(reader)));
        assert(QEO_OK == qeocore_reader_read(reader, NULL, rdata));
        expected_status = QEOCORE_DATA;
        validate_data(rdata);
        /* remove */
        assert(QEO_OK == qeocore_writer_remove(writer, wdata));
        /* clean up */
        qeocore_data_free(wdata);
        qeocore_data_free(rdata);
        qeocore_writer_close(writer);
        qeocore_reader_close(reader);
        qeocore_type_free(type);
        qeocore_factory_close(factory);
    }
}
