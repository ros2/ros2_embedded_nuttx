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

/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#include <assert.h>
#include <string.h>
#include <semaphore.h>
#include <jansson.h>
#include "verbose.h"
#include <qeojson/api.h>

/*########################################################################
#                                                                       #
#  TYPES and DEFINES SECTION                                            #
#                                                                       #
########################################################################*/

/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                           #
#                                                                       #
########################################################################*/
static int are_equal(json_t *obj1,
                     json_t *obj2,
                     int    key_only);

static void qeo_json_factory_init_done(qeo_factory_t  *factory,
                                       uintptr_t      userdata);

static void on_update(const qeo_json_state_reader_t *reader,
                      uintptr_t                     userdata);

static qeo_iterate_action_t on_reader_iterate(const char  *json_data,
                                              uintptr_t   userdata);

static void on_data(const qeo_json_state_change_reader_t  *reader,
                    const char                            *json_data,
                    uintptr_t                             userdata);

static void on_no_more_data(const qeo_json_state_change_reader_t  *reader,
                            uintptr_t                             userdata);

static void on_remove(const qeo_json_state_change_reader_t  *reader,
                      const char                            *json_data,
                      uintptr_t                             userdata);

static void on_reader_policy_update(const qeo_json_state_reader_t *reader,
                                    const char                    *json_policy,
                                    uintptr_t                     userdata);

static void on_writer_policy_update(const qeo_json_state_writer_t *writer,
                                    const char                    *json_policy,
                                    uintptr_t                     userdata);



/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                              #
#                                                                       #
########################################################################*/
static qeo_factory_t  *_factory = NULL;
static sem_t          _sync;
static sem_t          _sync_update;
static sem_t          _sync_nodata;
static sem_t          _sync_data;
static sem_t          _sync_remove;

static const char *simplestate_type = "{\
    \"topic\" : \"org::qeo::junit::State\",\
    \"behavior\" : \"state\",\
    \"properties\" : {\
        \"id\" : {\
            \"type\" : \"int32\",\
            \"key\" : true\
        },\
        \"value\" : {\
            \"type\" : \"string\"\
        }\
    }\
}";


static const char *teststate_template = "{\
    \"id\": %d,\
    \"value\":\"%s\"\
}";

/*########################################################################
#                                                                       #
#  STATIC FUNCTION IMPLEMENTATION                                       #
#                                                                       #
########################################################################*/
char _value[150] = {};

static qeo_json_state_reader_t        *_reader      = NULL;
static qeo_json_state_change_reader_t *_screader    = NULL;
static qeo_json_state_writer_t        *_writer      = NULL;
static uintptr_t                      _r_userdata   = (uintptr_t)0xdeadbabe;
static uintptr_t                      _ri_userdata  = (uintptr_t)0xbabedead;
static uintptr_t                      _scr_userdata = (uintptr_t)0xbabecafe;
static uintptr_t                      _w_userdata   = (uintptr_t)0xcafebabe;
static uintptr_t                      _f_userdata   = (uintptr_t)0xac1dcafe;

static int are_equal(json_t *obj1, json_t *obj2, int key_only)
{
    if (key_only == 0) {
        return json_equal(obj1, obj2);
    }
    else {
        void    *iter1  = json_object_iter_at(obj1, "id");
        void    *iter2  = json_object_iter_at(obj2, "id");
        json_t  *val1   = json_object_iter_value(iter1);
        json_t  *val2   = json_object_iter_value(iter2);
        return json_equal(val1, val2);
    }
}

static void on_update(const qeo_json_state_reader_t *reader,
                      uintptr_t                     userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(reader == _reader);
    assert(userdata == _r_userdata);

    assert(QEO_OK == qeo_json_state_reader_foreach(reader,
                                                   on_reader_iterate,
                                                   _ri_userdata));

    /* release main thread */
    sem_post(&_sync_update);
    log_verbose("%s exit", __FUNCTION__);
}

static qeo_iterate_action_t on_reader_iterate(const char  *json_data,
                                              uintptr_t   userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(userdata == _ri_userdata);

    json_t *json_data_json = json_loads(json_data, 0, NULL);
    assert(json_data_json != NULL);
    log_verbose("on_iterate : %s\r\n", json_data);
    json_t *value_json = json_loads(_value, 0, NULL);
    assert(value_json != NULL);

    assert(are_equal(json_data_json, value_json, 0) == 1);
    json_decref(json_data_json);
    json_decref(value_json);

    log_verbose("%s exit", __FUNCTION__);

    return QEO_ITERATE_CONTINUE;
}

static void on_data(const qeo_json_state_change_reader_t  *reader,
                    const char                            *json_data,
                    uintptr_t                             userdata)
{
    log_verbose("%s enter", __FUNCTION__);

    assert(reader == _screader);
    assert(userdata == _scr_userdata);

    json_t *json_data_json = json_loads(json_data, 0, NULL);
    assert(json_data_json != NULL);
    log_verbose("on_data : %s\r\n", json_data);
    json_t *value_json = json_loads(_value, 0, NULL);
    assert(value_json != NULL);

    assert(are_equal(json_data_json, value_json, 0) == 1);
    json_decref(json_data_json);
    json_decref(value_json);

    /* release main thread */
    sem_post(&_sync_data);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_no_more_data(const qeo_json_state_change_reader_t  *reader,
                            uintptr_t                             userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(reader == _screader);
    assert(userdata == _scr_userdata);

    /* release main thread */
    sem_post(&_sync_nodata);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_remove(const qeo_json_state_change_reader_t  *reader,
                      const char                            *json_data,
                      uintptr_t                             userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(reader == _screader);
    assert(userdata == _scr_userdata);

    log_verbose("on_remove : %s\r\n", json_data);
    json_t *json_data_json = json_loads(json_data, 0, NULL);
    assert(json_data_json != NULL);
    json_t *value_json = json_loads(_value, 0, NULL);
    assert(value_json != NULL);

    assert(are_equal(json_data_json, value_json, 1) == 1);
    json_decref(json_data_json);
    json_decref(value_json);

    /* release main thread */
    sem_post(&_sync_remove);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_reader_policy_update(const qeo_json_state_reader_t *reader,
                                    const char                    *json_policy,
                                    uintptr_t                     userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(userdata == _r_userdata);
    assert(NULL != json_policy);
    log_verbose("%s json_policy:%s\r\n", __FUNCTION__, json_policy);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_screader_policy_update(const qeo_json_state_change_reader_t  *reader,
                                      const char                            *json_policy,
                                      uintptr_t                             userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(userdata == _scr_userdata);
    assert(NULL != json_policy);
    log_verbose("%s json_policy:%s\r\n", __FUNCTION__, json_policy);
    log_verbose("%s exit", __FUNCTION__);
}

static void on_writer_policy_update(const qeo_json_state_writer_t *writer,
                                    const char                    *json_policy,
                                    uintptr_t                     userdata)
{
    log_verbose("%s enter", __FUNCTION__);
    assert(userdata == _w_userdata);
    assert(NULL != json_policy);
    log_verbose("%s json_policy:%s\r\n", __FUNCTION__, json_policy);
    log_verbose("%s exit", __FUNCTION__);
}

static void qeo_json_factory_init_done(qeo_factory_t  *factory,
                                       uintptr_t      userdata)
{
    qeo_json_state_reader_listener_t reader_cbs =
    {
        .on_update        = on_update,
        .on_policy_update = on_reader_policy_update
    };
    qeo_json_state_change_reader_listener_t screader_cbs =
    {
        .on_data          = on_data,
        .on_no_more_data  = on_no_more_data,
        .on_remove        = on_remove,
        .on_policy_update = on_screader_policy_update
    };
    qeo_json_state_writer_listener_t writer_cbs =
    {
        .on_policy_update = on_writer_policy_update
    };

    log_verbose("%s enter", __FUNCTION__);
    assert(userdata == _f_userdata);

    /* initialize */
    assert(NULL != (_reader = qeo_json_factory_create_state_reader(factory, simplestate_type, &reader_cbs, _r_userdata)));
    assert(QEO_OK == qeo_json_state_reader_enable(_reader));
    assert(NULL != (_screader = qeo_json_factory_create_state_change_reader(factory, simplestate_type, &screader_cbs, _scr_userdata)));
    assert(QEO_OK == qeo_json_state_change_reader_enable(_screader));
    assert(NULL != (_writer = qeo_json_factory_create_state_writer(factory, simplestate_type, &writer_cbs, _w_userdata)));
    assert(QEO_OK == qeo_json_state_writer_enable(_writer));

    /* write */
    sprintf(_value, teststate_template, 1, "level 1");
    log_verbose("Writing sample %s\n", _value);
    assert(QEO_OK == qeo_json_state_writer_write(_writer, _value));

    /* wait for reception */
    sem_wait(&_sync_update);
    sem_wait(&_sync_data);
    sem_wait(&_sync_nodata);

    /* remove */
    assert(QEO_OK == qeo_json_state_writer_remove(_writer, _value));

    /* wait for reception */
    sem_wait(&_sync_remove);

    /* clean up */
    qeo_json_state_writer_close(_writer);
    qeo_json_state_reader_close(_reader);
    qeo_json_state_change_reader_close(_screader);
    _factory = factory;
    /* release main thread */
    sem_post(&_sync);

    log_verbose("%s exit", __FUNCTION__);
}

/*########################################################################
#                                                                       #
#  PUBLIC FUNCTION IMPLEMENTATION                                       #
#                                                                       #
########################################################################*/
int main(int argc, const char **argv)
{
    qeo_json_factory_listener_t factory_listener =
    {
        .on_factory_init_done = qeo_json_factory_init_done
    };

    log_verbose("%s enter", __FUNCTION__);
    sem_init(&_sync_data, 0, 0);
    sem_init(&_sync_data, 0, 0);
    sem_init(&_sync_nodata, 0, 0);
    assert(QEO_OK == qeo_json_factory_create(&factory_listener, _f_userdata));
    sem_wait(&_sync);
    qeo_json_factory_close(_factory);
    assert(QEO_OK == qeo_json_factory_create_by_id("default", &factory_listener, _f_userdata));
    sem_wait(&_sync);
    qeo_json_factory_close(_factory);
    sem_destroy(&_sync);
    sem_destroy(&_sync_data);
    sem_destroy(&_sync_nodata);

    log_verbose("%s exit", __FUNCTION__);
}
