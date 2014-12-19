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

#include "unittest/unittest.h"

#include "qeo/openssl_engine.h"

#include "engine_fixtures.h"

#include <qeo/util_error.h>

#include "Mockplatform.h"
#include "Mockplatform_impl.h"

#include "openssl/conf.h"
#include <unistd.h>

ENGINE* _pkcs12_engine = NULL;

BIO* _out = NULL;

void openSSLRoughCleanup(void)
{
    // Some clean-up to avoid memory leaks : see http://www.openssl.org/support/faq.html#PROG13
    ERR_remove_state(getpid());
    CONF_modules_unload(0);

    EVP_cleanup();
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
}

void initBIO(void)
{
    _out = BIO_new_fd(fileno(stdout), BIO_NOCLOSE);
}

void uninitBIO(void)
{
    ck_assert_msg(BIO_free(_out) == 1, "BIO_free failed");
}

void loadEngine(void)
{
    printf("**** Loading engine ****\n");
    qeo_openssl_engine_init();
}

void initOpenSSL(void)
{
    OpenSSL_add_all_algorithms();
}

static char *_engine_id;
void initEngine(void)
{
    // init
    printf("**** Load engine ****\n");
    qeo_openssl_engine_init();
    qeo_openssl_engine_get_engine_id(&_engine_id); /* to be free()'d */
    _pkcs12_engine = ENGINE_by_id(_engine_id);

    qeo_platform_get_device_storage_path_StubWithCallback(qeo_platform_get_device_storage_path_callback);

    printf("**** Init engine ****\n");
    ck_assert_msg(ENGINE_init(_pkcs12_engine), "Failed to init engine");
}

void uninitEngine(void)
{
    // finish
    printf("**** Finish engine ****\n");
    ck_assert_msg(ENGINE_finish(_pkcs12_engine), "Failed to finish engine");

    printf("**** Free engine ****\n");
    ck_assert_msg(ENGINE_free(_pkcs12_engine), "Failed to free engine");

    ENGINE_cleanup();
    qeo_openssl_engine_destroy();
    free(_engine_id);
}
