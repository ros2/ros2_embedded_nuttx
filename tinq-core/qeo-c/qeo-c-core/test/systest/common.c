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
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>
#include "common.h"


typedef struct {
    sem_t semaphore;
    bool success;
} qeofactory_userdata;

static void on_qeocore_on_factory_init_done(qeo_factory_t *factory, bool success){

    qeofactory_userdata *userdata = NULL;

    qeocore_factory_get_user_data(factory, (uintptr_t *)&userdata);
    userdata->success = success;
    sem_post(&userdata->semaphore);
}

void init_factory(qeo_factory_t *factory)
{
    qeofactory_userdata userdata = { };
    qeocore_factory_listener_t listener = {
        .on_factory_init_done = on_qeocore_on_factory_init_done
    };
    assert(sem_init(&userdata.semaphore, 0, 0) == 0);
    assert(qeocore_factory_set_user_data(factory, (uintptr_t)&userdata)== QEO_OK);

    assert(QEO_OK == qeocore_factory_init(factory, &listener));
    sem_wait(&userdata.semaphore);

    assert(true == userdata.success);
    sem_destroy(&userdata.semaphore);
}


int count_instances(qeocore_reader_t *reader)
{
    qeocore_filter_t filter = { 0 };
    qeocore_data_t *data;
    qeo_retcode_t rc;
    int cnt = 0;

    assert(NULL != (data = qeocore_reader_data_new(reader)));
    while (1) {
        rc = qeocore_reader_read(reader, &filter, data);
        if (QEO_OK == rc) {
            filter.instance_handle = qeocore_data_get_instance_handle(data);
            cnt++;
            qeocore_data_reset(data);
        }
        else if (QEO_ENODATA == rc) {
            break;
        }
        else {
            abort(); /* error */
        }
    }
    qeocore_data_free(data);
    return cnt;
}

/*
 * Make sure openssl does not introduce memory leaks for valgrind
 */
__attribute__((destructor)) void cleanup_openssl (void)
{
    //http://www.openssl.org/support/faq.html#PROG13
    ERR_remove_thread_state(NULL);
    ENGINE_cleanup();
    CONF_modules_unload(1);
    ERR_free_strings();
    OBJ_cleanup();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
}

