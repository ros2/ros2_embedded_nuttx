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

#include <qeo/api.h>
#include "common.h"
#include "tsm_types.h"
#include "verbose.h"


static void on_data(const qeo_state_change_reader_t *reader,
                    const void *data,
                    uintptr_t userdata)
{
    /* nop */
}

static void on_no_more_data(const qeo_state_change_reader_t *reader,
                            uintptr_t userdata)
{
    /* nop */
}

static void on_remove(const qeo_state_change_reader_t *reader,
                      const void *data,
                      uintptr_t userdata)
{
    /* nop */
}

static void on_update(const qeo_state_reader_t *reader,
                      uintptr_t userdata)
{
    /* nop */
}

static void on_event_data(const qeo_event_reader_t *reader,
                          const void *data,
                          uintptr_t userdata)
{
    /* nop */
}

static void on_no_more_event_data(const qeo_event_reader_t *reader,
                                  uintptr_t userdata)
{
    /* nop */
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeo_state_reader_listener_t sr_cbs = {
        .on_update = on_update
    };
    qeo_state_change_reader_listener_t scr_cbs = {
        .on_data = on_data,
        .on_no_more_data = on_no_more_data,
        .on_remove = on_remove
    };

    qeo_event_reader_listener_t er_cbs = {
        .on_data = on_event_data,
        .on_no_more_data = on_no_more_event_data
    };


    /* initialize */
    log_verbose("initialization start");
    assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_OPEN)));
    _tsm_types[0].flags |= TSMFLAG_KEY;
    _tsm_types[1].flags |= TSMFLAG_KEY; /* makes 'string' key */
    assert(NULL == qeo_factory_create_state_reader(factory, _tsm_types, &sr_cbs, 0));
    assert(NULL == qeo_factory_create_state_change_reader(factory, _tsm_types, &scr_cbs, 0));
    assert(NULL == qeo_factory_create_state_writer(factory, _tsm_types, NULL, 0));
    assert(NULL == qeo_factory_create_event_reader(factory, _tsm_types, &er_cbs, 0));
    assert(NULL == qeo_factory_create_event_writer(factory, _tsm_types, NULL, 0));
    log_verbose("initialization done");
    qeo_factory_close(factory);
    return 0;
}
