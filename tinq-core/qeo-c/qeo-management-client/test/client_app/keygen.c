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

#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>

#include "keygen.h"

EVP_PKEY* keygen_create(int bits)
{
    EVP_PKEY *pk = EVP_PKEY_new();
    RSA *rsa = NULL;

    if (bits == -1){
        bits = 1024;
    }
    if (pk == NULL ) {
        goto err;
    }

    rsa = RSA_generate_key(bits, RSA_F4, NULL, NULL );
    if (rsa == NULL ) {
        goto err;
    }
    if (!EVP_PKEY_assign_RSA(pk,rsa))
        goto err;
    rsa = NULL; //Make sure it is no longer freed as ownership is taken by previous function
    return pk;
err:
    if (rsa) RSA_free(rsa);
    if (pk) EVP_PKEY_free(pk);
    return NULL;
}

