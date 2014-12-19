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
#include <stdbool.h>
#include <string.h>

#include <qeocore/api.h>

#include "tsm_types.h"
#include "common.h"

static qeocore_member_id_t _id_num = QEOCORE_MEMBER_ID_DEFAULT;
static qeocore_member_id_t _id_nested = QEOCORE_MEMBER_ID_DEFAULT;

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    assert(0); /* not reading or writing so no data expected */
}

static qeocore_type_t *dyn_type_reg_and_get(const qeo_factory_t *factory,
                                            const char *outer_name,
                                            const char *inner_name)
{
    qeocore_type_t *outer = NULL;
    qeocore_type_t *inner = NULL;
    qeocore_type_t *member = NULL;

    assert(NULL != (outer = qeocore_type_struct_new(outer_name)));
    assert(NULL != (inner = qeocore_type_struct_new(inner_name)));
    assert(NULL != (member = qeocore_type_primitive_new(QEOCORE_TYPECODE_INT32)));
    assert(QEO_OK == qeocore_type_struct_add(inner, member, "num", &_id_num, QEOCORE_FLAG_NONE));
    qeocore_type_free(member);
    assert(QEOCORE_MEMBER_ID_DEFAULT != _id_num);
    assert(QEO_OK == qeocore_type_struct_add(outer, inner, "nested", &_id_nested, QEOCORE_FLAG_NONE));
    qeocore_type_free(inner);
    assert(QEOCORE_MEMBER_ID_DEFAULT != _id_nested);
    assert(QEO_OK == qeocore_type_register(factory, outer, outer_name));
    return outer;
}

static void test(bool tsm_based,
                 const char *outer1,
                 const char *inner1,
                 const char *outer2,
                 const char *inner2)
{

    qeo_factory_t *factory;
    qeocore_type_t *type, *type2;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    if (tsm_based) {
        assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name)));
    }
    else {
        assert(NULL != (type = dyn_type_reg_and_get(factory, outer1, inner1)));
        /* validate double registration */
        assert(NULL != (type2 = dyn_type_reg_and_get(factory, outer1, inner1)));
        assert(type != type2);
        qeocore_type_free(type2);
    }
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE,
                                                 &listener, NULL)));
    qeocore_type_free(type);
    /* test */
    if (tsm_based) {
        assert(NULL != (type = qeocore_type_register_tsm(factory, _tsm_types, _tsm_types[0].name)));
    }
    else {
        assert(NULL != (type2 = dyn_type_reg_and_get(factory, outer2, inner2)));
        /* validate double registration */
        assert(NULL != (type = dyn_type_reg_and_get(factory, outer2, inner2)));
        assert(type != type2);
        qeocore_type_free(type2);
    }
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE,
                                                 NULL, NULL)));
    qeocore_type_free(type);
    /* clean up */
    qeocore_writer_close(writer);
    qeocore_reader_close(reader);
    qeocore_factory_close(factory);
}

int main(int argc, const char **argv)
{
    test(true, NULL, NULL, NULL, NULL);
    test(false, "outer", "inner", "outer", "inner");
    test(false, "outer", "inner", "outer2", "inner"); /* test reuse of inner class */
    return 0;
}
