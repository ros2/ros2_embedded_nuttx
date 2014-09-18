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

DDS_SEQUENCE(int, intseq);

/** Sequence of basic int32_t type **/
struct struct1 {
	intseq anintseq;
};

#define SEQ1_MAX 10
#define SEQ1_N 5
static DDS_TypeSupport_meta tsm1[] = {
    { CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "struct1", sizeof(struct struct1), 0, 1},
    { CDR_TYPECODE_SEQUENCE, 0, "anintseq", 0, offsetof (struct struct1, anintseq), SEQ1_MAX, },
    { CDR_TYPECODE_LONG, },
};

void test_sequences1 (void)
{
	DDS_TypeSupport ts;
	struct struct1 tmp, *tmp2;
	int i;

	v_printf ("test_sequences1 - ");
	ts = DDS_DynamicType_register (tsm1);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT(tmp.anintseq);
	dds_seq_require (&tmp.anintseq, SEQ1_N);
	for (i = 0; i < SEQ1_N; i++)
		DDS_SEQ_ITEM (tmp.anintseq, i) = 'a' + i;

#ifdef DUMP_DATA
	if (dump_data)
		dump_seq ("anintseq", &tmp.anintseq);
#endif
	marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);

	fail_unless (DDS_SEQ_LENGTH (tmp2->anintseq) == SEQ1_N);
	for (i = 0; i < SEQ1_N; i++)
		fail_unless (DDS_SEQ_ITEM (tmp.anintseq, i) ==
			     DDS_SEQ_ITEM (tmp2->anintseq, i));

	dds_seq_cleanup (&tmp.anintseq);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** Sequence of a simple struct **/
#define STRUCT2A_MSGLEN 13

typedef enum {
    a,
    b = 100,
    c
} anenum2;

struct struct2a {
	char ch;
	char message[STRUCT2A_MSGLEN];
	uint16_t u16;
	int16_t i16;
	uint32_t u32;
	int32_t i32;
	uint64_t u64;
	int64_t i64;
	float fl;
	double d;
	unsigned char bool;
	unsigned char octet;
	anenum2 anenumelement;
};

DDS_SEQUENCE(struct struct2a, structseq);

struct struct2b {
	uint32_t anuint;
	structseq astructseq;
	uint32_t anuint2;
};

#define SEQ2_MAX 7
#define SEQ2_N 7
static DDS_TypeSupport_meta tsm2[] = {
    { CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "struct2b", sizeof(struct struct2b), 0, 3, },
    { CDR_TYPECODE_ULONG, 0, "anuint", 0, offsetof (struct struct2b, anuint), },
    { CDR_TYPECODE_SEQUENCE,  0, "astructseq", 0, offsetof (struct struct2b, astructseq), SEQ2_MAX, },
    { CDR_TYPECODE_STRUCT, 0, "struct2a", sizeof(struct struct2a), 0, 13, },
    { CDR_TYPECODE_CHAR, 0, "ch", 0, offsetof (struct struct2a, ch), },
    { CDR_TYPECODE_CSTRING, 0, "message", STRUCT2A_MSGLEN, offsetof(struct struct2a, message), },
    { CDR_TYPECODE_USHORT, 0, "u16", 0, offsetof(struct struct2a, u16), },
    { CDR_TYPECODE_SHORT, 0, "i16", 0, offsetof(struct struct2a, i16), },
    { CDR_TYPECODE_ULONG, 0, "u32", 0, offsetof(struct struct2a, u32), },
    { CDR_TYPECODE_LONG, 0, "i32", 0, offsetof(struct struct2a, i32), },
    { CDR_TYPECODE_ULONGLONG, 0, "u64", 0, offsetof(struct struct2a, u64), },
    { CDR_TYPECODE_LONGLONG, 0, "i64", 0, offsetof(struct struct2a, i64), },
    { CDR_TYPECODE_FLOAT, 0, "fl", 0, offsetof(struct struct2a, fl), },
    { CDR_TYPECODE_DOUBLE, 0, "d", 0, offsetof(struct struct2a, d), },
    { CDR_TYPECODE_BOOLEAN, 0, "bool", 0, offsetof(struct struct2a, bool), },
    { CDR_TYPECODE_OCTET, 0, "octet", 0, offsetof(struct struct2a, octet), },
    { CDR_TYPECODE_LONG, 0, "anenumelement", 0, offsetof(struct struct2a, anenumelement), },
    { CDR_TYPECODE_ULONG, 0, "anuint2", 0, offsetof (struct struct2b, anuint2), },
};

void test_sequences2 (void)
{
	DDS_TypeSupport ts;
	struct struct2a *sp, *dp;
	struct struct2b tmp, *tmp2;
	int i, j;

	v_printf ("test_sequences2 - ");
	ts = DDS_DynamicType_register (tsm2);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT (tmp.astructseq);
	dds_seq_require (&tmp.astructseq, SEQ2_N);
	tmp.anuint = (1<<31)+(1<<16);
	tmp.anuint2 = 3;
	for (i = 0; i < SEQ2_N; i++) {
		sp = DDS_SEQ_ITEM_PTR (tmp.astructseq, i);
		sp->ch = 'a' - i;
		memset(sp->message, 0, STRUCT2A_MSGLEN);
		for (j = 0; j < STRUCT2A_MSGLEN-i-1; j++)
			sp->message[j] = 'a'+j*i;
		sp->message[j] = '\0';
		sp->u16 = 17;
		sp->i16 = -17;
		sp->u32 = (1<<18)+6;
		sp->i32 = -5555555;
		sp->u64 = (((uint64_t)1)<<33)-2764;
		sp->i64 = -((((uint64_t)1)<<39)-276412);
		sp->fl = 1e-5;
		sp->d = 1e-15;
		sp->bool = 0;
		sp->octet = 0xf2;
		sp->anenumelement = b;
	}
#ifdef DUMP_DATA
	if (dump_data)
		dump_seq ("astructseq", &tmp.astructseq);
#endif
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);

	fail_unless (tmp.anuint == tmp2->anuint && tmp.anuint2 == tmp2->anuint2);
	for (i = 0; i < SEQ2_N; i++) {
		sp = DDS_SEQ_ITEM_PTR (tmp.astructseq, i);
		dp = DDS_SEQ_ITEM_PTR (tmp2->astructseq, i);
		fail_unless (!strcmp(sp->message, dp->message) &&
				sp->ch == dp->ch);
		fail_unless (sp->u16 == dp->u16 &&
			sp->i16 == dp->i16 &&
			sp->u32 == dp->u32 &&
			sp->i32 == dp->i32 &&
			sp->u64 == dp->u64 &&
			sp->fl == dp->fl &&
			sp->d == dp->d &&
			sp->bool == dp->bool &&
			sp->octet == dp->octet &&
			sp->anenumelement == dp->anenumelement);
	}

	dds_seq_cleanup (&tmp.astructseq);
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** Sequence of sequences of various length **/
DDS_SEQUENCE(int, intseq3);
DDS_SEQUENCE(intseq3, seqseq);

struct struct3 {
	seqseq aseqseq;
	intseq3 anintseq;
};

#define SEQSEQ3_MAX 7
#define SEQSEQ3_N 7
#define SEQ3_MAX 7
#define SEQ3_N 7
#define SEQ3BIS_MAX 13
#define SEQ3BIS_N 11
static DDS_TypeSupport_meta tsm3[] = {
	{ CDR_TYPECODE_STRUCT, 2, "struct3", sizeof(struct struct3), 0, 2, },
	{ CDR_TYPECODE_SEQUENCE, 2, "aseqseq", 0, offsetof (struct struct3, aseqseq), SEQSEQ3_MAX, },
	{ CDR_TYPECODE_SEQUENCE, 2, "anintseqseq", 0, 0, SEQ3_MAX, },
	{ CDR_TYPECODE_LONG, },
	{ CDR_TYPECODE_SEQUENCE, 2,  "anintseq2", 0, offsetof (struct struct3, anintseq), SEQ3BIS_MAX, },
	{ CDR_TYPECODE_LONG, },
};

void test_sequences3 (void)
{
	DDS_TypeSupport ts;
	struct struct3 tmp, *tmp2;
	int i, j;

	v_printf ("test_sequences3 - ");
	ts = DDS_DynamicType_register (tsm3);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT (tmp.aseqseq);
	dds_seq_require (&tmp.aseqseq, SEQSEQ3_N);
	/* include a zero-length sequence */
	fail_unless (SEQ3_N - DDS_SEQ_LENGTH (tmp.aseqseq) == 0);
	for (i = 0; i < DDS_SEQ_LENGTH (tmp.aseqseq); i++) {
		DDS_SEQ_INIT (DDS_SEQ_ITEM (tmp.aseqseq, i));
		dds_seq_require (&DDS_SEQ_ITEM (tmp.aseqseq, i), SEQ3_N-1-i);
		for (j = 0;
		     j < DDS_SEQ_LENGTH (DDS_SEQ_ITEM (tmp.aseqseq, i));
		     j++)
			DDS_SEQ_ITEM (DDS_SEQ_ITEM (tmp.aseqseq, i), j) = 1024+i*i*j;
	}
	DDS_SEQ_INIT (tmp.anintseq);
	dds_seq_require (&tmp.anintseq, SEQ3BIS_N);
	for (i = 0; i < SEQ3BIS_N; i++)
		DDS_SEQ_ITEM (tmp.anintseq, i) = (i-3)*(i-3)*123;

#ifdef DUMP_DATA
	if (dump_data) {
		dump_seq ("aseqseq", &tmp.aseqseq);
		for (i = 0; i < DDS_SEQ_LENGTH (tmp.aseqseq); i++) {
			char buf [20];

			snprintf (buf, 20, "-aseqseq<%d>", i);
			dump_seq (buf, &DDS_SEQ_ITEM (tmp.aseqseq, i));
		}
		dump_seq ("anintseq", &tmp.anintseq);
	}
#endif
	marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);

#ifdef DUMP_DATA
	if (dump_data) {
		dump_seq ("aseqseq", &tmp2->aseqseq);
		for (i = 0; i < DDS_SEQ_LENGTH (tmp2->aseqseq); i++) {
			char buf [20];

			snprintf (buf, 20, "-aseqseq<%d>", i);
			dump_seq (buf, &DDS_SEQ_ITEM (tmp2->aseqseq, i));
		}
		dump_seq ("anintseq", &tmp2->anintseq);
	}
#endif
	for (i = 0; i < SEQSEQ3_N; i++) {
		fail_unless (DDS_SEQ_LENGTH (DDS_SEQ_ITEM (tmp.aseqseq, i)) ==
			     DDS_SEQ_LENGTH (DDS_SEQ_ITEM (tmp2->aseqseq, i)) &&
			     DDS_SEQ_ELEM_SIZE (DDS_SEQ_ITEM (tmp.aseqseq, i)) ==
			     DDS_SEQ_ELEM_SIZE (DDS_SEQ_ITEM (tmp2->aseqseq, i)));
		
		for (j = 0;
		     j < DDS_SEQ_LENGTH (DDS_SEQ_ITEM (tmp.aseqseq, i));
		     j++)
			fail_unless (DDS_SEQ_ITEM (DDS_SEQ_ITEM (tmp.aseqseq, i), j) ==
				     DDS_SEQ_ITEM (DDS_SEQ_ITEM (tmp2->aseqseq, i), j));
	}
	fail_unless (DDS_SEQ_LENGTH (tmp.anintseq) == DDS_SEQ_LENGTH (tmp2->anintseq) &&
		     DDS_SEQ_ELEM_SIZE (tmp.anintseq) == DDS_SEQ_ELEM_SIZE (tmp2->anintseq));
	for (i = 0; i < SEQ3BIS_N; i++)
		fail_unless (DDS_SEQ_ITEM (tmp.anintseq, i) == DDS_SEQ_ITEM (tmp2->anintseq, i));

	for (i = 0; i < DDS_SEQ_LENGTH (tmp.aseqseq); i++)
		dds_seq_cleanup (&DDS_SEQ_ITEM (tmp.aseqseq, i));
	dds_seq_cleanup (&tmp.aseqseq);
	dds_seq_cleanup (&tmp.anintseq);
	xfree(tmp2);

	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** Sequence of unbounded strings **/
DDS_SEQUENCE(char *, stringseq4);

struct struct4 {
	stringseq4 astringseq;
};

#define SEQ4_MAX 18
#define SEQ4_N 14
static DDS_TypeSupport_meta tsm4[] = {
    { CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "struct4", sizeof(struct struct4), 0, 1, },
    { CDR_TYPECODE_SEQUENCE, TSMFLAG_DYNAMIC, "astringseq", 0, offsetof (struct struct4, astringseq), SEQ4_MAX, },
    { CDR_TYPECODE_CSTRING, },
};

void test_sequences4 (void)
{
	DDS_TypeSupport ts;
	struct struct4 tmp, *tmp2;
	char *strings[SEQ4_N];
	int i;

	v_printf ("test_sequences4 - ");
	ts = DDS_DynamicType_register(tsm4);
	fail_unless (NULL != ts);

	strings[0] = "Testiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiing!!!";
	strings[1] = "One two three\t\tFOUR";
	strings[2] = "qwertyqwertyqwertyqwertyqwerty";
	strings[3] = "";
	strings[4] = "Another test";
	strings[5] = "Writer's block :-(";
	strings[6] = "bAcK online !";
	strings[7] = "Testing is phun";
	strings[8] = "Why did I pick 13, why oh why";
	strings[9] = "Almost there now";
	strings[10] = "Really, just a few more to go!!!";
	strings[11] = "I'm excited !";
	strings[12] = "Here we are. Now that wasn't so difficult, was it?";
	strings[13] = NULL;

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT (tmp.astringseq);
	dds_seq_from_array (&tmp.astringseq, strings, SEQ4_N);
#ifdef DUMP_DATA
	if (dump_data)
		dump_seq ("astringseq", &tmp.astringseq);
#endif
	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);

#ifdef DUMP_DATA
	if (dump_data)
		dump_seq ("astringseq", &tmp2->astringseq);
#endif
	for (i = 0; i < SEQ4_N-1; i++)
		fail_unless (!strcmp (DDS_SEQ_ITEM (tmp.astringseq, i),
				      DDS_SEQ_ITEM (tmp2->astringseq, i)));

	dds_seq_cleanup (&tmp.astringseq);
	xfree (tmp2);

	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

void test_sequences (void)
{
	test_sequences1 ();
	test_sequences2 ();
	test_sequences3 ();
	test_sequences4 ();
}
