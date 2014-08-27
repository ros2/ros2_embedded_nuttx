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

#include "error.h"
#include "assert.h"

#define fail_unless	assert

#define DUMP_TYPE	/* Set this to dump type info. */
#define DUMP_TYPECODE	/* Set this to dump typecode data. */
#define	COPY_DATA	/* Set this to test data_free/copy/equals() functions.*/
#ifdef XTYPES_USED
#define DUMP_DATA	/* Set this to dump sample+marshalled data. */
#define PARSE_DATA	/* Set this to interpret fields. */
#define INTROSPECTION	/* Perform introspection on the constructed type. */

#include "type_data.h"
#include "error.h"
#endif

void verify_result (const void *src, const void *dst, size_t len,
		    const void *cdr, size_t clen);
void marshallUnmarshall(const void *sample, void **sample_out,
			DDS_TypeSupport ts, int clear);
void marshallDynamic(const DDS_DynamicData sample, DDS_DynamicData *sample_out,
						DDS_DynamicTypeSupport ts);
void dump_region (const void *sample, size_t length);
void dump_seq (const char *name, const void *seq);

#define v_printf	if(verbose) dbg_printf

extern int dump_type, dump_data, parse_data, verbose;

#endif /* !__test_h_ */

