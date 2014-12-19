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
#include <stdio.h>

#include <qeocore/api.h>
#include "tsm_types.h"
#include "common.h"

#define LOOPS 100

static qeo_factory_t *_factory = NULL;

typedef struct {
    char *string;
} extra_t;

const DDS_TypeSupport_meta _tsm_extra[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.Extra",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE, .size = sizeof(extra_t), .nelem = 1 },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string", .size = 0, .label = 12345,
     .flags = TSMFLAG_DYNAMIC, .offset = offsetof(extra_t, string) },
};

static int create_destroy(qeocore_reader_t **r,
                          const DDS_TypeSupport_meta *tsm)
{
    int rc = 0;

    if (NULL == *r) {
        qeocore_type_t *type;

        //printf("open  %s\n", td->name);
        assert(NULL != (type = qeocore_type_register_tsm(_factory, tsm, tsm[0].name)));
        *r = qeocore_reader_open(_factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE, NULL, NULL);
        qeocore_type_free(type);
        if (NULL == *r) {
            rc = 1;
        }
    }
    else {
        //printf("close %s\n", td->name);
        qeocore_reader_close(*r);
        *r = NULL;
    }
    return rc;
}

int main(int argc, const char **argv)
{
    qeocore_reader_t *r1 = NULL;
    qeocore_reader_t *r2 = NULL;


    assert(NULL != (_factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(_factory);
    assert(0 == create_destroy(&r1, _tsm_types)); // open 1
    assert(0 == create_destroy(&r1, _tsm_types)); // close 1
    assert(0 == create_destroy(&r2, _tsm_extra)); // open 2
    assert(0 == create_destroy(&r2, _tsm_extra)); // close 2
    assert(0 == create_destroy(&r2, _tsm_extra)); // open 2
    assert(0 == create_destroy(&r1, _tsm_types)); // open 1
    assert(0 == create_destroy(&r1, _tsm_types)); // close 1
    assert(0 == create_destroy(&r1, _tsm_types)); // open 1
    assert(0 == create_destroy(&r1, _tsm_types)); // close 1
    assert(0 == create_destroy(&r2, _tsm_extra)); // close 2
    qeocore_factory_close(_factory);
    return 0;
}
