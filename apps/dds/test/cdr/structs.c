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
#include "pool.h"
#include "dds/dds_tsm.h"
#include "test.h"

/** very basic struct **/
struct struct1 {
	uint32_t a;
	int64_t b;
};

static DDS_TypeSupport_meta tsm1[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct1", sizeof(struct struct1), 0, 2, },
	{ CDR_TYPECODE_LONG, 0, "a", 0, offsetof(struct struct1, a), },
	{ CDR_TYPECODE_LONGLONG, 0, "b", 0, offsetof(struct struct1, b), },
};

void test_struct1 (void)
{
	DDS_TypeSupport ts;
	static struct struct1 tmp = {0xCAFEBABE, -1};
	struct struct1 *tmp2;

	v_printf ("test_struct1 - ");
	ts = DDS_DynamicType_register (tsm1);
	fail_unless (NULL != ts);
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free(ts);
	v_printf ("success!\r\n");
}

/** struct with basic types **/
struct struct2 {
	uint16_t u16;
	int16_t i16;
	uint32_t u32;
	int32_t i32;
	uint64_t u64;
	int64_t i64;
	float fl;
	double d;
	char ch;
};

static DDS_TypeSupport_meta tsm2[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct2", sizeof(struct struct2), 0, 9},
	{ CDR_TYPECODE_USHORT, 0, "u16", 0, offsetof(struct struct2, u16), },
	{ CDR_TYPECODE_SHORT, 0, "i16", 0, offsetof(struct struct2, i16), },
	{ CDR_TYPECODE_ULONG, 0, "u32", 0, offsetof(struct struct2, u32), },
	{ CDR_TYPECODE_LONG, 0, "i32", 0, offsetof(struct struct2, i32), },
	{ CDR_TYPECODE_ULONGLONG, 0, "u64", 0, offsetof(struct struct2, u64), },
	{ CDR_TYPECODE_LONGLONG, 0, "i64", 0, offsetof(struct struct2, i64), },
	{ CDR_TYPECODE_FLOAT, 0, "fl", 0, offsetof(struct struct2, fl), },
	{ CDR_TYPECODE_DOUBLE, 0, "d", 0, offsetof(struct struct2, d), },
	{ CDR_TYPECODE_CHAR, 0, "ch", 0, offsetof(struct struct2, ch), },
};

void test_struct2 (void)
{
	DDS_TypeSupport ts;
	static struct struct2 tmp = {
		0xDEAD, INT16_MIN, UINT32_MAX, -5, 5010000, 100, 0.5f, 100e-5, 'd'
	};
	struct struct2 *tmp2;

	v_printf ("test_struct2 - ");
	ts = DDS_DynamicType_register (tsm2);
	fail_unless (NULL != ts);
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** struct of structs **/
#define STRUCT3A_MESSAGE_LEN 100
struct struct3a {
	char message[STRUCT3A_MESSAGE_LEN];
	uint32_t i;
};
struct struct3b {
	float fl;
};
struct struct3 {
	struct struct3a s_3a;
	struct struct3b s_3b;
};

static DDS_TypeSupport_meta tsm3[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct3", sizeof(struct struct3), 0, 2, },
	{ CDR_TYPECODE_STRUCT, 0, "s_3a", sizeof(struct struct3a), offsetof(struct struct3, s_3a), 2, },
	{ CDR_TYPECODE_CSTRING, 0, "message", STRUCT3A_MESSAGE_LEN, offsetof(struct struct3a, message), },
	{ CDR_TYPECODE_ULONG, 0, "i", 0, offsetof(struct struct3a, i), },
	{ CDR_TYPECODE_STRUCT, 0, "s_3b", sizeof(struct struct3b), offsetof(struct struct3, s_3b), 1, },
	{ CDR_TYPECODE_FLOAT, 0, "fl", 0, offsetof(struct struct3b, fl), },
};

void test_struct3 (void)
{
	struct struct3 tmp, *tmp2;
	DDS_TypeSupport ts;

	v_printf ("test_struct3 - ");
	ts = DDS_DynamicType_register (tsm3);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	strncpy (tmp.s_3a.message, "Testing 1,2,3", sizeof (tmp.s_3a));
	tmp.s_3a.i = 25;
	tmp.s_3b.fl = 0.3e9;
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);
	fail_unless (tmp.s_3a.i == tmp2->s_3a.i &&
		     tmp.s_3b.fl == tmp2->s_3b.fl &&
		     !strcmp (tmp.s_3a.message, tmp2->s_3a.message));
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** multiple use of same type **/
struct struct4a {
	uint32_t i;
	uint32_t j;
};
struct struct4 {
	struct struct4a s_4a1;
	uint64_t i;
	struct struct4a s_4a2;
};

static DDS_TypeSupport_meta tsm4[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct4", sizeof(struct struct4), 0, 3, },
	{ CDR_TYPECODE_STRUCT, 0, "s_4a1", sizeof(struct struct4a), offsetof(struct struct4, s_4a1), 2, },
	{ CDR_TYPECODE_ULONG, 0, "i", 0, offsetof(struct struct4a, i), },
	{ CDR_TYPECODE_ULONG, 0, "j", 0, offsetof(struct struct4a, j), },
	{ CDR_TYPECODE_ULONGLONG, 0, "i", 0, offsetof(struct struct4, i), },
	{ CDR_TYPECODE_STRUCT, 0, "s_4a2", sizeof(struct struct4a), offsetof(struct struct4, s_4a2), 2, },
	{ CDR_TYPECODE_ULONG, 0, "i", 0, offsetof(struct struct4a, i), },
	{ CDR_TYPECODE_ULONG, 0, "j", 0, offsetof(struct struct4a, j), },
};

void test_struct4 (void)
{
	struct struct4 tmp, *tmp2;
	DDS_TypeSupport ts;

	v_printf ("test_struct4 - ");
	ts = DDS_DynamicType_register (tsm4);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	tmp.s_4a1.i = 25;
	tmp.s_4a1.j = 22225;
	tmp.s_4a2.i = 225;
	tmp.s_4a2.j = 223325;
	tmp.i = 2288833399u;
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 1);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** struct with char pointer **/
#define BOUNDEDSTR "This is a bounded string\0"
#define BOUNDEDSTR_MAXLEN 100

struct struct5 {
	char s1[BOUNDEDSTR_MAXLEN];
	char *s2;
};

static DDS_TypeSupport_meta tsm5[] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct5", sizeof(struct struct5), 0, 2, },
	{ CDR_TYPECODE_CSTRING, 0, "s1", BOUNDEDSTR_MAXLEN, offsetof(struct struct5, s1), },
	{ CDR_TYPECODE_CSTRING, 0, "s2", 0, offsetof(struct struct5, s2), },
};

void test_struct5 (void)
{
	struct struct5 tmp, *tmp2;
	DDS_TypeSupport ts;

	v_printf ("test_struct5 - ");

	ts = DDS_DynamicType_register (tsm5);
	fail_unless(NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	strncpy (tmp.s1, BOUNDEDSTR, sizeof (tmp.s1));
	tmp.s2 = "This is an unbounded string!";

	// (Un)Marchall without verify because verification is done with memcmp
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);

	fail_unless(NULL != tmp2);
	fail_unless(NULL != tmp2->s1);
	fail_unless(NULL != tmp2->s2);
	fail_unless(!strcmp(tmp.s1, tmp2->s1));
	fail_unless(!strcmp(tmp.s2, tmp2->s2));

	xfree (tmp2);
	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

void test_structs (void)
{
	test_struct1 ();
	test_struct2 ();
	test_struct3 ();
	test_struct4 ();
	test_struct5 ();
}

