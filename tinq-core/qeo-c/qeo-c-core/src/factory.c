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

#include <semaphore.h>
#include <errno.h>
#include <qeo/log.h>
#include <string.h>
#include <stdbool.h>

#include <qeo/factory.h>
#include <qeocore/api.h>
#include "forwarder.h"

static void on_qeocore_on_factory_init_done(qeo_factory_t *factory, bool success);

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    bool finished;

    bool success;
} qeofactory_userdata;

static qeocore_factory_listener_t _listener = {
    .on_factory_init_done = on_qeocore_on_factory_init_done
};

static qeo_factory_t* _factory = NULL;

static void on_qeocore_on_factory_init_done(qeo_factory_t *factory, bool success){

    uintptr_t puserdata;

    qeocore_factory_get_user_data(factory, &puserdata);
    qeofactory_userdata *userdata = (qeofactory_userdata *)puserdata;

    pthread_mutex_lock(&userdata->mutex);
    userdata->success = success;
    userdata->finished = true;
    pthread_cond_signal(&userdata->cond);
    pthread_mutex_unlock(&userdata->mutex);

}

static qeo_factory_t *factory_create(const qeo_identity_t *id,
                                     qeocore_on_fwdfactory_get_public_locator cb,
                                     const char *local_port)
{
    qeo_factory_t *factory = NULL;
    qeo_retcode_t ret = QEO_EFAIL;
    qeofactory_userdata userdata = { 
        .cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER,
        .mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER,
        .finished = false,
        .success = false
    };

    if (_factory == NULL) {
        do {
            if (NULL != local_port) {
                _listener.on_fwdfactory_get_public_locator = cb;
                if (QEO_OK != (ret = qeocore_factory_set_local_tcp_port(NULL, local_port))) {
                    qeo_log_e("Factory set local TCP port failed");
                    break;
                }
            }
            factory = qeocore_factory_new(id);
            if (factory == NULL) {
                ret = QEO_ENOMEM;
                qeo_log_e("Failed to construct new factory");
                break;
            }


            if ((ret = qeocore_factory_set_user_data(factory, (uintptr_t)&userdata)) != QEO_OK){
                qeo_log_e("Factory set user data failed");
                break;
            }

            if ((ret = qeocore_factory_init(factory, &_listener)) != QEO_OK ) {
                qeo_log_e("Factory init failed");
                break;
            }

            /* wait until the mutex is unlocked through the callback ! */
            pthread_mutex_lock(&userdata.mutex);
            while (userdata.finished == false){
                pthread_cond_wait(&userdata.cond, &userdata.mutex);
            }
            pthread_mutex_unlock(&userdata.mutex);

            ret = QEO_OK;

        } while (0);

        qeocore_factory_set_user_data(factory, 0);
        pthread_mutex_destroy(&userdata.mutex);
        pthread_cond_destroy(&userdata.cond);

        if (ret != QEO_OK || userdata.success == false){
            qeocore_factory_close(factory);
            factory = NULL;
        }

        _factory = factory;
    }
    else {
        qeo_log_e("Factory can only be created once");
    }

    return factory;
}



qeo_factory_t *qeo_factory_create()
{
    return qeo_factory_create_by_id(QEO_IDENTITY_DEFAULT);
}

qeo_factory_t *qeo_factory_create_by_id(const qeo_identity_t *id)
{
    return factory_create(id, NULL, NULL);
}


void qeo_factory_close(qeo_factory_t *factory)
{
    if ((_factory != NULL) && (_factory == factory)) {
        qeocore_factory_close(factory);
        _factory = NULL;
    }
    else {
        qeo_log_e("Trying to close an invalid factory");
    }
}

qeo_factory_t *qeocore_fwdfactory_new(qeocore_on_fwdfactory_get_public_locator cb,
                                      const char *local_port)
{
    qeo_factory_t *factory = NULL;

    if ((NULL != cb) && (NULL != local_port)) {
        factory = factory_create(QEO_IDENTITY_DEFAULT, cb, local_port);
    }
    return factory;
}

void qeocore_fwdfactory_close(qeo_factory_t *factory)
{
    qeo_factory_close(factory);
}

qeo_retcode_t qeocore_fwdfactory_set_public_locator(qeo_factory_t *factory,
                                                    const char *ip_address,
                                                    int port)
{
    return fwd_server_reconfig(factory, ip_address, port);
}

const char *qeo_version_string(void)
{
#ifdef QEO_VERSION
    return QEO_VERSION;
#else
    return "x.x.x-UNKNOWN";
#endif
}
