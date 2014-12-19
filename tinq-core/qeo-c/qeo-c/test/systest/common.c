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

#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <qeo/types.h>
#include <assert.h>
#include "common.h"

static qeo_iterate_action_t count_instances_callback(const void *data,
                                                     uintptr_t userdata)
{
    int *cnt = (int *)userdata;

    (*cnt)++;
    return QEO_ITERATE_CONTINUE;
}

int count_instances(qeo_state_reader_t *reader)
{
    int cnt = 0;

    assert(QEO_OK == qeo_state_reader_foreach(reader, count_instances_callback, (uintptr_t)&cnt));
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
