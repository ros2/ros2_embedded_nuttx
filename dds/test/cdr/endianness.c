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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef XTYPES_USED
#include "dds/dds_tsm.h"
#include "xcdr.h"
#include "typecode.h"
#else
#include "cdr.h"
#endif
#include "test.h"

/* we directly use the cdr functions here in order to be able to set
 * swap, which will result in a change of endianness. */

struct struct1 {
	uint16_t u16;
	char ch[16];
	uint32_t u32;
	int64_t u64;
};

static DDS_TypeSupport_meta tsm1[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct1", sizeof(struct struct1), 0, 4, },
	{ CDR_TYPECODE_USHORT, 0, "u16", 0, offsetof (struct struct1, u16), },
	{ CDR_TYPECODE_CSTRING, 0, "ch", 16, offsetof (struct struct1, ch), },
	{ CDR_TYPECODE_ULONG, 0, "u32", 0, offsetof (struct struct1, u32), },
	{ CDR_TYPECODE_ULONGLONG, 0, "u64", 0, offsetof (struct struct1, u64), },
};

void test_endianness (void)
{
	DDS_TypeSupport ts;
	DDS_ReturnCode_t err = 0;
	struct struct1 tmp, tmp2;
	size_t len, l;
	char *out;
	int swap;

	/* Make sure the entire structure is filled with zero's, because we're going
	 * to use it for comparison. */
	memset (&tmp, '\0', sizeof(tmp));
	tmp.u16 = 0xCAFE;
	strcpy (tmp.ch, "another test");
	tmp.u32 = -200;
	tmp.u64 = 125;

	ts = DDS_DynamicType_register (tsm1);
	fail_unless (NULL != ts);

#ifdef DUMP_TYPE
	if (dump_type)
		DDS_TypeSupport_dump_type (0, ts, XDF_ALL);
#endif
#ifdef DUMP_DATA
	if (dump_data) {
		dbg_printf ("Sample:\r\n");
		dump_region (&tmp, sizeof (tmp));
#ifdef PARSE_DATA
		if (parse_data) {
			dbg_printf ("\t");
			cdr_dump_native (1, &tmp, ts->ts_cdr, 0, 0, 1);
			dbg_printf ("\r\n");
		}
#endif
	}
#endif
	memset (&tmp2, 0, sizeof (tmp2));
	len = cdr_marshalled_size (4, &tmp, ts->ts_cdr, 0, 0, 0, NULL);
	out = malloc (len);
	fail_unless (out != NULL);
	for (swap = 0; swap < 2; swap++) {
		v_printf ("test_endianness(%d) - ", swap);
		fflush (stdout);
		l = cdr_marshall (out, 4, &tmp, ts->ts_cdr, 0, 0, 0, swap, &err);
		fail_unless (0 == err);
#ifdef DUMP_DATA
		if (dump_data) {
			dbg_printf ("\r\nMarshalled(swap=%d):\r\n", swap);
			dump_region (out, len);
#ifdef PARSE_DATA
			if (parse_data) {
				dbg_printf ("\t");
				cdr_dump_cdr (1, out, 4, ts->ts_cdr, 0, 0, swap, 1);
				dbg_printf ("\r\n");
			}
#endif
		}
#endif
		err = cdr_unmarshall (&tmp2, out, 4, ts->ts_cdr, 0, 0, swap, 0);
		fail_unless (0 == err);
#ifdef DUMP_DATA
		if (dump_data) {
			dbg_printf ("Unmarshalled(swap=%d):\r\n", swap);
			dump_region (&tmp2, sizeof (tmp2));
#ifdef PARSE_DATA
			if (parse_data) {
				dbg_printf ("\t");
				cdr_dump_native (1, &tmp2, ts->ts_cdr, 0, 0, 1);
				dbg_printf ("\r\n");
			}
#endif
		}
#endif
		verify_result (&tmp, &tmp2, sizeof (tmp), out, len);
		fail_unless (!memcmp (&tmp, &tmp2, sizeof (tmp)));
		v_printf ("success!\r\n");
	}
	free (out);
	DDS_DynamicType_free (ts);
}

