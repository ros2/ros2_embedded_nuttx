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

#ifndef ENGINE_FS_H
#define ENGINE_FS_H

#include "openssl/engine.h"

/*Defines*/
#define FS_ENGINE_ID	"fs"
#define FS_ENGINE_NAME	"engine fs"

#define CMD_LOAD_CERT_CTRL	ENGINE_CMD_BASE
#define CMD_LOAD_CA_CERT_CTRL   (ENGINE_CMD_BASE+1)
#define CMD_VERIFY_CERT_CTL	(ENGINE_CMD_BASE+2)

typedef struct {
	const char		*id;
	void			*data;
} dataMap;

void engine_fs_load (void);
void *init_engine_fs (void);
#endif

