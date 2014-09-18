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

#ifndef __test_h_
#define __test_h_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "assert.h"
#include "error.h"
#include "dds/dds_dcps.h"
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"

extern int	trace;
extern int	verbose;
extern unsigned	delay_ms;

#define fail_unless     assert
#define dbg_printf 	if (trace) printf
#define	v_printf	if (verbose) printf
#define delay()		usleep(delay_ms & 1000)

#endif

