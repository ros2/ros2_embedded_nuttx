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

#include <string.h>
#include <time.h>
#include <unistd.h>

#include <qeocore/api.h>

#include "common.h"

#define LOOPS 100

static qeo_factory_t *_factory = NULL;

typedef struct {
    char *string;
} type_t;

const DDS_TypeSupport_meta _tsm[] =
{
   { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.test.Extra",
     .flags = TSMFLAG_DYNAMIC|TSMFLAG_MUTABLE, .size = sizeof(type_t), .nelem = 1 },
   { .tc = CDR_TYPECODE_CSTRING, .name = "string", .size = 0, .label = 12345,
     .flags = TSMFLAG_DYNAMIC, .offset = offsetof(type_t, string) },
};

static int create_destroy(qeocore_writer_t **w,
                          const DDS_TypeSupport_meta *tsm,
                          const char *name)
{
    int rc = 0;

    if (NULL == name) {
        name = tsm->name;
    }
    if (NULL == *w) {
        qeocore_type_t *type;

        //printf("open  %s\n", name);
        assert(NULL != (type = qeocore_type_register_tsm(_factory, tsm, tsm[0].name)));
        *w = qeocore_writer_open(_factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA | QEOCORE_EFLAG_ENABLE, NULL, NULL);
        qeocore_type_free(type);
        if (NULL == *w) {
            rc = 1;
        }
    }
    else {
        //printf("close %s\n", name);
        qeocore_writer_close(*w);
        *w = NULL;
    }
    return rc;
}

int main(int argc, const char **argv)
{
    qeocore_writer_t *base = NULL; // to prevent dp clean up
    qeocore_writer_t *w[32] = {0};
    int loops = LOOPS, i;

    assert(NULL != (_factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(_factory);
    assert(0 == create_destroy(&base, _tsm, "base")); // open base
    srand(time(NULL));
    for (i = 0; i < loops; i++) {
        int b = rand() %32;
        char name[32];

        snprintf(name, sizeof(name), "TYPE_%02d", b);
        assert(0 == create_destroy(&w[b], _tsm, name));
    }
    // clean up
    for (i = 0; i < 32; i++) {
        if (NULL != w[i]) {
            assert(0 == create_destroy(&w[i], _tsm, NULL));
        }
    }
    assert(0 == create_destroy(&base, _tsm, NULL)); // close base
    qeocore_factory_close(_factory);
    return 0;
}
