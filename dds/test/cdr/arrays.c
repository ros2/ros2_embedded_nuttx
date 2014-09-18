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
#include "dds/dds_tsm.h"
#include "pool.h"
#include "test.h"

/** Array of basic char type **/
typedef char chararray[20];
struct struct1 {
	chararray char_a;
};

static DDS_TypeSupport_meta tsm1[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct1", sizeof(struct struct1), 0, 1, },
	{ CDR_TYPECODE_ARRAY, 0, "char_a",  sizeof (chararray), offsetof (struct struct1, char_a), 20, },
	{ CDR_TYPECODE_CHAR, },
};

void test_arrays1 (void)
{
	DDS_TypeSupport ts;
	struct struct1 tmp, *tmp2;

	v_printf ("test_arrays1 - ");
	ts = DDS_DynamicType_register(tsm1);
	fail_unless(NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	memcpy (tmp.char_a, "een test123        ", sizeof (tmp.char_a));
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

/** Array of struct **/
#define TA2_I64X	5

typedef int64_t i64array [TA2_I64X];
struct struct2 {
	i64array i64;
	char ch;
};

#define TA2_STR2X	7

typedef struct struct2 struct2array [TA2_STR2X];

struct struct3 {
	char ch;
	struct2array str2;
	uint16_t u16;
};

static DDS_TypeSupport_meta tsm2[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct3", sizeof(struct struct3), 0, 3, },
	{ CDR_TYPECODE_CHAR, 0, "ch", 0, offsetof(struct struct3, ch), },
	{ CDR_TYPECODE_ARRAY, 0, "str2", sizeof(struct2array), offsetof(struct struct3, str2), TA2_STR2X, },
	{ CDR_TYPECODE_STRUCT, 0, "struct2", sizeof(struct struct2), 0, 2, },
	{ CDR_TYPECODE_ARRAY, 0, "i64", sizeof(i64array), offsetof(struct struct2, i64), TA2_I64X,},
	{ CDR_TYPECODE_LONGLONG, },
	{ CDR_TYPECODE_CHAR, 0, "ch", 0, offsetof(struct struct2, ch), },
	{ CDR_TYPECODE_USHORT, 0, "u16", 0, offsetof(struct struct3, u16), },
};

void test_array2 (void)
{
	DDS_TypeSupport ts;
	struct struct3 tmp, *tmp2;
	int i, j;

	v_printf ("test_arrays2 - ");
	ts = DDS_DynamicType_register(tsm2);
	fail_unless(NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	tmp.ch = 'A';
	tmp.u16 = 1366;
	for (i = 0; i < TA2_STR2X; i++) {
		for (j = 0; j < TA2_I64X; j++) {
			tmp.str2 [i].i64 [j] = ((i-6) * j) << i;
		}
		tmp.str2 [i].ch = (char)(i + 0x30);
	}
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** Array of fixed-length strings **/
#define TA3_NSTR	7
#define TA3_STRLEN	11

typedef char strings_t [TA3_NSTR][TA3_STRLEN];
struct struct4 {
	char ch;
	strings_t strings;
};

static DDS_TypeSupport_meta tsm3[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct4", sizeof(struct struct4), 0, 2, },
	{ CDR_TYPECODE_CHAR, 0, "ch", 0, offsetof(struct struct4, ch), },
	{ CDR_TYPECODE_ARRAY, 0, "strings", sizeof(strings_t), offsetof(struct struct4, strings), TA3_NSTR,},
	{ CDR_TYPECODE_CSTRING, 0, "__", TA3_STRLEN, },
};

void test_array3 (void)
{
	DDS_TypeSupport ts;
	struct struct4 tmp, *tmp2;
	int i, j;

	v_printf ("test_arrays3 - ");
	ts = DDS_DynamicType_register(tsm3);
	fail_unless(NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	tmp.ch = 'A';
	for (i = 0; i < TA3_NSTR; i++) {
		for (j = 0; j < TA3_STRLEN - 1 - i; j++) {
			tmp.strings [i][j] = (char)('A' + ((i * TA3_STRLEN + j) % 26));
		}
		for (j = TA3_STRLEN - 1 - i; j < TA3_STRLEN; j++)
			tmp.strings [i][j] = '\0';
	}
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** Array of array of array of int16 **/
#define TA4_ALEN1	7
#define TA4_ALEN2	11
#define TA4_ALEN3	3
#define TA4_ALEN4	14

struct struct5bis {
	char str[100];
};

typedef uint16_t ints_t1 [TA4_ALEN3];
typedef uint16_t ints_t2 [TA4_ALEN2][TA4_ALEN3];
typedef uint16_t ints_t [TA4_ALEN1][TA4_ALEN2][TA4_ALEN3];
typedef struct struct5bis structbis_t1[TA4_ALEN4];
typedef struct struct5bis structbis_t[TA4_ALEN4][TA4_ALEN4];
struct struct5 {
	ints_t ints;
	char ch;
	structbis_t structbis;
};

static DDS_TypeSupport_meta tsm4[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct5", sizeof(struct struct5), 0, 3, },
	{ CDR_TYPECODE_ARRAY, 0, "ints", sizeof(ints_t), offsetof(struct struct5, ints), TA4_ALEN1, },
	{ CDR_TYPECODE_ARRAY, 0, NULL, sizeof(ints_t2), 0, TA4_ALEN2, },
	{ CDR_TYPECODE_ARRAY, 0, NULL, sizeof(ints_t1), 0, TA4_ALEN3},
	{ CDR_TYPECODE_USHORT, },
	{ CDR_TYPECODE_CHAR, 0, "ch", 0, offsetof(struct struct5, ch), },
	{ CDR_TYPECODE_ARRAY, 0, "structbis", sizeof(structbis_t), offsetof(struct struct5, structbis), TA4_ALEN4, },
	{ CDR_TYPECODE_ARRAY, 0, NULL, sizeof(structbis_t1), 0, TA4_ALEN4, },
	{ CDR_TYPECODE_STRUCT, 0, NULL, sizeof(struct struct5bis), 0, 1},
	{ CDR_TYPECODE_CSTRING, 0, "str", 100, offsetof(struct struct5bis, str), },
};

void test_array4 (void)
{
	DDS_TypeSupport ts;
	struct struct5 tmp, *tmp2;
	int i, j, k;

	v_printf ("test_arrays4 - ");
	ts = DDS_DynamicType_register(tsm4);
	fail_unless(NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	for (i = 0; i < TA4_ALEN1; i++)
		for (j = 0; j < TA4_ALEN2; j++)
			for (k = 0; k < TA4_ALEN3; k++)
				tmp.ints [i][j][k] = (j - 5) * (i - 7) * (k + 2);
	for (i = 0; i < TA4_ALEN4; i++)
		for (j = 0; j < TA4_ALEN4; j++) {
			for (k = 0; k < 99; k++)
				tmp.structbis[i][j].str[k] = 64+((i*j*k) % 24);
			tmp.structbis[i][j].str[99] = '\0';
		}

	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

void test_array (void)
{
	test_arrays1 ();
	test_array2 ();
	test_array3 ();
	test_array4 ();
}

