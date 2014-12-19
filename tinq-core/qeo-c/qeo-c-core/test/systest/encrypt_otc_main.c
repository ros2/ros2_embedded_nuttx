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
#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <qeocore/remote_registration.h>
#include "tsm_types.h"
#include "common.h"



int main(int argc, const char **argv)
{
    char *key = "-----BEGIN PUBLIC KEY-----\n" \
                "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAJ66GpEXMyT2C132qsvRbcqbRE8k1JMx\n"\
                "IVWZZ6hH2d0iLgDypfuw6DAfdxzsUnKwlHliNwDH5jAVpJcQXhvKRmUCAwEAAQ==\n"\
                "-----END PUBLIC KEY-----\n";
    unsigned char *encrypted_otc = NULL;

    assert(-1 == qeocore_remote_registration_encrypt_otc(NULL, NULL, NULL));
    assert(-1 == qeocore_remote_registration_encrypt_otc(NULL, key, NULL));
    assert(-1 == qeocore_remote_registration_encrypt_otc("otc", NULL, NULL));
    int size = qeocore_remote_registration_encrypt_otc("otc", key, &encrypted_otc);
    assert(64 == size); //512 bits key == 64 bytes
    qeocore_remote_registration_free_encrypted_otc(encrypted_otc);
    return 0;
}
