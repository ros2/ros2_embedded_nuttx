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

/* log.c -- Logging ids. */

#include <stdio.h>

const char *log_id_str [32] = {
	"PROF",
	"DOMAIN", "POOL", "STR", "LOCATOR",
	"TIMER", "DB", "THREAD", "CACHE",
	"IP", "RTPS", "QOS", "SPDP", "SEDP",
	"DISC", "DCPS", "XTYPES", "SEC", "DDS",
	"INFO", "USER",
};

const char **log_fct_str [32] = {NULL,};	/* Function strings. */

