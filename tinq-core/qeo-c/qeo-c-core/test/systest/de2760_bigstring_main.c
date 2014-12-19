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
#include "tsm_types.h"
#include "common.h"
#include "dyn_types.h"
#include "tsm_types.h"

#define STRING_LENGTH 66000

static qeocore_type_t *dynamic_get_type(qeo_factory_t *factory,
                                        const DDS_TypeSupport_meta *tsm)
{
    qeocore_type_t *type;

    assert(NULL != (type = types_get(tsm)));
    assert(QEO_OK == qeocore_type_register(factory, type, tsm->name));
    return type;
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_writer_t *writer;
    char *s = (char*) malloc(STRING_LENGTH+1);
    int i = 0;

    /* initialize */
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = dynamic_get_type(factory, _tsm_types)));

    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL)));
    /* test late enabling of readers/writers */
    assert(QEO_OK == qeocore_writer_enable(writer));
    /* send structure */
    void *data = NULL;
    assert(NULL != (data = qeocore_writer_data_new(writer)));
    for (i = 0; i < STRING_LENGTH; ++i) {
        s[i] = (i%100) + 32;
    }
    s[i] = '\0';

    assert(QEO_OK == qeocore_data_set_member(data, _member_id[M_STRING], &s));
    qeocore_data_free(data);
    qeocore_writer_close(writer);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
    free(s);
}
