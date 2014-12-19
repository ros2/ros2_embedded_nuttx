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

/*#######################################################################
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/

#include <pthread.h>

#include <qeo/log.h>

#include "entity_store.h"
#include "list.h"
#include "user_data.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/

/**
 * Locks the entity store and any partition updates that might get
 * triggered.
 */
static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK pthread_mutex_lock(&_lock);
#define UNLOCK pthread_mutex_unlock(&_lock);

static list_t *_store = NULL;

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

static list_iterate_action_t update_user_data(void *data,
                                              uintptr_t userdata)
{
    entity_t *entity = (entity_t *)data;
    qeo_retcode_t rc = QEO_OK;

    if (entity->flags.is_writer) {
        rc = writer_user_data_update((qeocore_writer_t *)entity);
    }
    else {
        rc = reader_user_data_update((qeocore_reader_t *)entity);
    }
    if (QEO_OK != rc) {
        qeo_log_e("Failed to update policy for %s", entity->flags.is_writer ? "writer" : "reader");
    }
    return LIST_ITERATE_CONTINUE;
}

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

qeo_retcode_t entity_store_init(void)
{
    qeo_retcode_t rc = QEO_OK;

    LOCK;
    if (NULL != _store) {
        qeo_log_e("bad state: already initialized");
        rc = QEO_EBADSTATE;
    }
    else {
        _store = list_new();
        if (NULL == _store) {
            qeo_log_e("not enough memory to create entity_store");
            rc = QEO_ENOMEM;
        }
    }
    UNLOCK;
    return rc;
}

qeo_retcode_t entity_store_add(entity_t *entity)
{
    qeo_retcode_t rc = QEO_EBADSTATE;

    LOCK;
    if (NULL != _store) {
        rc = list_add(_store, entity);
    }
    UNLOCK;
    return rc;
}

qeo_retcode_t entity_store_remove(const entity_t *entity)
{
    qeo_retcode_t rc = QEO_EBADSTATE;

    LOCK;
    if (NULL != _store) {
        rc = list_remove(_store, entity);
    }
    UNLOCK;
    return rc;
}

qeo_retcode_t entity_store_fini(void)
{
    qeo_retcode_t rc = QEO_OK;

    LOCK;
    if (NULL != _store) {
        list_free(_store);
        _store = NULL;
    }
    else {
        qeo_log_e("entity_store_fini failed");
        rc = QEO_EBADSTATE;
    }
    UNLOCK;
    return rc;
}

qeo_retcode_t entity_store_update_user_data(const qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_EBADSTATE;

    LOCK;
    if (NULL != _store) {
        rc = list_foreach(_store, update_user_data, (uintptr_t)factory);
    }
    UNLOCK;
    return rc;
}
