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

#ifndef VERBOSE_H_
#define VERBOSE_H_

#include <stdio.h>
#include <unistd.h>

int verbose(void);

#define log_verbose(fmt, ...) if (verbose()) printf(fmt "\n", ##__VA_ARGS__)
#define log_pid(fmt, ...) log_verbose("%d - " fmt, getpid(), ##__VA_ARGS__)

#endif /* VERBOSE_H_ */
