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

#ifndef QEOCORE_REMOTE_REGISTRATION_H_
#define QEOCORE_REMOTE_REGISTRATION_H_

int qeocore_remote_registration_encrypt_otc(const char *otc,
                                            const char *public_key,
                                            unsigned char ** encrypted_otc);

void qeocore_remote_registration_free_encrypted_otc(unsigned char *encrypted_otc);

char * qeocore_remote_registration_get_pub_key_pem();

//allocates memory!
char * qeocore_remote_registration_decrypt_otc(unsigned char *encrypted_otc,
                                              int enrypted_otc_size);


#endif
