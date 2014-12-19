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

#ifndef ENGINE_FIXTURES_H_
#define ENGINE_FIXTURES_H_

#include <openssl/engine.h>

extern BIO* _out;

extern ENGINE* _pkcs12_engine;

void openSSLRoughCleanup(void);
void initBIO(void);
void uninitBIO(void);

void loadEngine(void);
void initOpenSSL(void);
void initEngine(void);
void uninitEngine(void);

#endif
