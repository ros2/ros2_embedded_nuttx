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

#include "dds_dcps.h"

struct simple_st {
	uint16_t	u16;
	int16_t		i16;
	uint32_t	u32;
	int32_t		i32;
	uint64_t	u64;
	int64_t		i64;
	float		fl;
	double		d;
	char		*sp;
	char		ch;
};

enum discr_en {
	d_ch = 4,
	d_num = 22,
	d_fnum = 555
};

union info_un {
	char		ch;
	uint32_t	num;
	float		fnum;
};

DDS_UNION (union info_un, enum discr_en, info_u);

enum object {
	bike,
	car,
	plane,
	blimp = -1
};

struct s_st {
	struct simple_st simple;
	char		 *name;
	int32_t		 x, y;
	int32_t		 height;
	enum object	 obj;
	info_u		 info;
} s;

static DDS_TypeSupport_meta s_tsm [] = {
	{ CDR_TYPECODE_STRUCT,   2, "s", sizeof (struct s_st), 0, 7, 0 },
	{ CDR_TYPECODE_CSTRING,  0, "name",   0, offsetof (struct s_st, name), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "x",      0, offsetof (struct s_st, x), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "y",      0, offsetof (struct s_st, y), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "height", 0, offsetof (struct s_st, height), 0, 0 },
	{ CDR_TYPECODE_STRUCT,   0, "simple", sizeof (struct simple_st), 0, 10, 0 },
	{ CDR_TYPECODE_CHAR,     0, "ch",     0, offsetof (struct simple_st, ch), 0, 0 },
	{ CDR_TYPECODE_USHORT,   0, "u16",    0, offsetof (struct simple_st, u16), 0, 0 },
	{ CDR_TYPECODE_SHORT,    0, "i16",    0, offsetof (struct simple_st, i16), 0, 0 },
	{ CDR_TYPECODE_ULONG,    0, "u32",    0, offsetof (struct simple_st, u32), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "i32",    0, offsetof (struct simple_st, i32), 0, 0 },
	{ CDR_TYPECODE_ULONGLONG,0, "u64",    0, offsetof (struct simple_st, u64), 0, 0 },
	{ CDR_TYPECODE_LONGLONG, 0, "i64",    0, offsetof (struct simple_st, i64), 0, 0 },
	{ CDR_TYPECODE_FLOAT,    0, "fl",     0, offsetof (struct simple_st, fl), 0, 0 },
	{ CDR_TYPECODE_DOUBLE,   0, "d",      0, offsetof (struct simple_st, d), 0, 0 },
	{ CDR_TYPECODE_CSTRING,  0, "sp",     0, offsetof (struct simple_st, sp), 0, 0 },
	{ CDR_TYPECODE_ENUM,     0, "obj",    0, offsetof (struct s_st, obj), 4, 0 },
	{ CDR_TYPECODE_LONG,     0, "bike",   0, 0, 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "car",    0, 0, 0, 1 },
	{ CDR_TYPECODE_LONG,     0, "plane",  0, 0, 0, 2 },
	{ CDR_TYPECODE_LONG,     0, "blimp",  0, 0, 0, -1 },
	{ CDR_TYPECODE_UNION,    2, "info",   sizeof (info_u), offsetof (struct s_st, info), 3, 0 },
	{ CDR_TYPECODE_CHAR,     2, "ch",     0, offsetof (info_u, u), 0, d_ch },
	{ CDR_TYPECODE_ULONG,	 2, "num",    0, offsetof (info_u, u), 0, d_num },
	{ CDR_TYPECODE_FLOAT,    2, "fnum",   0, offsetof (info_u, u), 0, d_fnum }
};

int main (int argc, char **argv)
{
	

	return 0;
}
