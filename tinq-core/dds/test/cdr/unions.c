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
#include "dds/dds_types.h"
#include "dds/dds_dcps.h"
#include "pool.h"
#include "test.h"


/*************************************************************
 * TEST: unions2
 * sequence of simple union
 *************************************************************/

#define STRLEN1 13

typedef enum {
	anint = 12,
	astring = 345
} u1_disc;

typedef union {
	int i;
	char str[STRLEN1];
} u1_union;

DDS_UNION (u1_union, u1_disc, u1_dds_union);

typedef struct {
	u1_dds_union aunion;
} u1_struct;

static DDS_TypeSupport_meta u1_tsm[] = {
        {CDR_TYPECODE_STRUCT, 0, "union_struct1", sizeof(u1_struct), 0, 1, },
        {CDR_TYPECODE_UNION, 0, "aunion", sizeof(u1_dds_union), offsetof (u1_struct, aunion), 2, },
        {CDR_TYPECODE_LONG, 0, "along", 0, offsetof (u1_dds_union, u), 0, anint, },
        {CDR_TYPECODE_CSTRING, 0, "acstring", STRLEN1, offsetof (u1_dds_union, u), 0, astring, },
  };

/**
 * Basic union test
 */
void test_unions1a (void)
{
	DDS_TypeSupport ts;
	u1_struct data_in, *data_out;

	v_printf ("test_unions1a - ");

	ts = DDS_DynamicType_register (u1_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.aunion.discriminant = anint;
	data_in.aunion.u.i = 12345;

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.aunion.discriminant == data_out->aunion.discriminant);
	fail_unless (data_in.aunion.u.i == data_out->aunion.u.i);

	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

void test_unions1b (void)
{
	DDS_TypeSupport ts;
	u1_struct data_in, *data_out;

	v_printf ("test_unions1b - ");

	ts = DDS_DynamicType_register (u1_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.aunion.discriminant = astring;
	strncpy (data_in.aunion.u.str, "SimpleTest\n", STRLEN1);

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.aunion.discriminant == data_out->aunion.discriminant);
	fail_unless (data_in.aunion.u.i == data_out->aunion.u.i);
	fail_unless (strncmp(data_in.aunion.u.str, data_out->aunion.u.str, strlen(data_in.aunion.u.str)) == 0);

	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}


/*************************************************************
 * TEST: unions2
 * sequence of simple union
 *************************************************************/

#define U2_SEQ2_MAX 15
#define U2_SEQ2_N 7
#define U2_STR_MAX_LEN 16

typedef enum {
	anint2 = 11,
	astring2 = 16,
	afloat2 = 8787
} u2_enum2;

typedef union {
	int i;
	char *str;
	float fl;
} u2_union2;

DDS_UNION (u2_union2, u2_enum2, u2_dds_union2);
DDS_SEQUENCE (u2_dds_union2, u2_seq2);

typedef struct {
	u2_seq2 aseq;
} u2_struct1;

static DDS_TypeSupport_meta u2_tsm2[] = {
        { CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "union_struct2", sizeof(u2_struct1), 0, 1, },
        { CDR_TYPECODE_SEQUENCE, TSMFLAG_DYNAMIC, "seq2", 0, offsetof (u2_struct1, aseq),U2_SEQ2_MAX, },
        { CDR_TYPECODE_UNION, TSMFLAG_DYNAMIC, "aunion", sizeof(u2_dds_union2), 0, 3, },
        { CDR_TYPECODE_LONG, 0, "along", 0, offsetof (u2_dds_union2, u), 0, anint2, },
        { CDR_TYPECODE_CSTRING, 0, "acstring", 0, offsetof (u2_dds_union2, u), 0, astring2, },
        { CDR_TYPECODE_FLOAT, 0, "afloat", 0, offsetof (u2_dds_union2, u), 0, afloat2, },
};

void test_unions2 (void)
{
	DDS_TypeSupport ts;
	u2_struct1 data_in, *data_out;
	u2_dds_union2 u_in, *u_out;
	int   val0 = ~0;
	char *val1 = "Buffer 1\0";
	float val2 = 355 / 113.0;
	int   val3 = 1200;
	char *val4 = "Buffer 4\0";
	float val5 = 2 * 355 / 113.0;
	int   val6 = -1200;

	v_printf ("test_unions2a - ");

	ts = DDS_DynamicType_register (u2_tsm2);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));
	memset (&u_in, 0, sizeof (u_in));

	/* Fill in input data. */
	DDS_SEQ_INIT (data_in.aseq);
	dds_seq_require (&data_in.aseq, U2_SEQ2_N);
	u_in.discriminant = anint2;
	u_in.u.i = val0;
	DDS_SEQ_ITEM (data_in.aseq, 0) = u_in;
	u_in.discriminant = astring2;
	u_in.u.str = val1;
	DDS_SEQ_ITEM (data_in.aseq, 1) = u_in;
	u_in.discriminant = afloat2;
	u_in.u.fl = val2;
	DDS_SEQ_ITEM (data_in.aseq, 2) = u_in;
	u_in.discriminant = anint2;
	u_in.u.i = val3;
	DDS_SEQ_ITEM (data_in.aseq, 3) = u_in;
	u_in.discriminant = astring2;
	u_in.u.str = val4;
	DDS_SEQ_ITEM (data_in.aseq, 4) = u_in;
	u_in.discriminant = afloat2;
	u_in.u.fl = val5;
	DDS_SEQ_ITEM (data_in.aseq, 5) = u_in;
	u_in.discriminant = anint2;
	u_in.u.i = val6;
	DDS_SEQ_ITEM (data_in.aseq, 6) = u_in;

#ifdef DUMP_DATA
	if (dump_data)
		dump_seq ("aseq", &data_in.aseq);
#endif
	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);

	fail_unless (DDS_SEQ_LENGTH (data_out->aseq) == U2_SEQ2_N);
	fail_unless (DDS_SEQ_MAXIMUM (data_out->aseq) == U2_SEQ2_N);
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 0);
	fail_unless (u_out->discriminant == anint2 &&
		     u_out->u.i == val0);
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 1);
	fail_unless (u_out->discriminant == astring2 &&
		     u_out->u.str != val1 &&
		     !strcmp (u_out->u.str, val1));
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 2);
	fail_unless (u_out->discriminant == afloat2 &&
		     u_out->u.fl == val2);
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 3);
	fail_unless (u_out->discriminant == anint2 &&
		     u_out->u.i == val3);
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 4);
	fail_unless (u_out->discriminant == astring2 &&
		     u_out->u.str != val4 &&
		     !strcmp (u_out->u.str, val4));
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 5);
	fail_unless (u_out->discriminant == afloat2 &&
		     u_out->u.fl == val5);
	u_out = DDS_SEQ_ITEM_PTR (data_out->aseq, 6);
	fail_unless (u_out->discriminant == anint2 &&
		     u_out->u.i == val6);

	dds_seq_cleanup (&data_in.aseq);
	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

/*************************************************************
 * TEST: unions3
 * union of struct
 *************************************************************/

#define U3_STRLEN3 25
#define U3_STRLEN3b 17

typedef enum {
	i3 = 111,
	struct3 = 55,
	d3 = 989345
} u3_disc;

typedef struct {
	char ch1[U3_STRLEN3];
	double d;
	char ch2[U3_STRLEN3b];
} u3_substruct;

typedef union {
	int i;
	u3_substruct astruct3;
	double d;
} u3_union;

DDS_UNION (u3_union, u3_disc, u3_dds_union);

typedef struct {
	u3_dds_union u3;
} u3_struct;

static DDS_TypeSupport_meta u3_tsm[] = {
	{ CDR_TYPECODE_STRUCT, 0, "union_struct3", sizeof(u3_struct), 0, 1, },
        { CDR_TYPECODE_UNION, 0, "u3", sizeof(u3_dds_union), 0, 3, },
        { CDR_TYPECODE_LONG, 0, "along", 0, offsetof (u3_dds_union, u), 0, i3, },
        { CDR_TYPECODE_STRUCT, 0, "astruct", sizeof(u3_substruct), offsetof (u3_dds_union, u), 3, struct3, },
        { CDR_TYPECODE_CSTRING, 0, "ch1", U3_STRLEN3, offsetof(u3_substruct, ch1), },
        { CDR_TYPECODE_DOUBLE, 0, "d", 0, offsetof (u3_substruct, d), },
        { CDR_TYPECODE_CSTRING, 0, "ch2", U3_STRLEN3b, offsetof(u3_substruct, ch2), },
        { CDR_TYPECODE_DOUBLE, 0, "adouble", 0, offsetof (u3_dds_union, u), 0, d3 },
};


void test_unions3a (void)
{
	DDS_TypeSupport ts;
	u3_struct data_in, *data_out;

	v_printf ("test_unions3a - ");

	ts = DDS_DynamicType_register (u3_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.u3.discriminant = i3;
	data_in.u3.u.i = -56789;

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.u3.discriminant == data_out->u3.discriminant);
	fail_unless (data_in.u3.u.i == data_out->u3.u.i);

	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

void test_unions3b (void)
{
	DDS_TypeSupport ts;
	u3_struct data_in, *data_out;
	char  *val0 = "Lorem ipsum";
	double val1 = 355 / 113.0;
	char  *val2 = "ante prandium";


	v_printf ("test_unions3b - ");

	ts = DDS_DynamicType_register (u3_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.u3.discriminant = struct3;
	strncpy(data_in.u3.u.astruct3.ch1, val0, U3_STRLEN3);
	data_in.u3.u.astruct3.d = val1;
	strncpy(data_in.u3.u.astruct3.ch2, val2, U3_STRLEN3b);

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.u3.discriminant == data_out->u3.discriminant);
	fail_unless (strcmp (data_out->u3.u.astruct3.ch1, val0) == 0);
	fail_unless (data_out->u3.u.astruct3.d == val1);
	fail_unless (strcmp (data_out->u3.u.astruct3.ch2, val2) == 0);

	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

void test_unions3c (void)
{
	DDS_TypeSupport ts;
	u3_struct data_in, *data_out;

	v_printf ("test_unions3c - ");

	ts = DDS_DynamicType_register (u3_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.u3.discriminant = d3;
	data_in.u3.u.d = 355 / 133.0;

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.u3.discriminant == data_out->u3.discriminant);
	fail_unless (data_in.u3.u.d == data_out->u3.u.d);

	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

/*************************************************************
 * TEST: unions4
 * union of sequence and struct
 *************************************************************/

#define U4_STRLEN4 	187
#define U4_SEQ4_MAX 	7
#define U4_SEQ4_N 	7

typedef enum {
	struct4 = 1,
	seq4 = 0,
} u4_disc;

typedef struct {
	char  *str;
	double d;
} u4_substruct;

typedef char chararr [U4_STRLEN4];

DDS_SEQUENCE (chararr, u4_seq4);

typedef union {
	u4_substruct astruct4;
	u4_seq4 aseq4;
} u4_union;

DDS_UNION (u4_union, u4_disc, u4_dds_union);

typedef struct {
	u4_dds_union u4;
} u4_struct;

static DDS_TypeSupport_meta u4_tsm[] = {
        { CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "union_struct4", sizeof(u4_struct), 0, 1, },
        { CDR_TYPECODE_UNION, TSMFLAG_DYNAMIC, "u4", sizeof(u4_dds_union), 0, 2, },
        { CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "u", sizeof(u4_substruct), offsetof (u4_dds_union, u), 2, struct4, },
        { CDR_TYPECODE_CSTRING, 0, "str", 0, offsetof(u4_substruct, str), },
        { CDR_TYPECODE_DOUBLE, 0, "d", 0, offsetof (u4_substruct, d), },
        { CDR_TYPECODE_SEQUENCE, 0, "aseq4", 0, offsetof (u4_dds_union, u), U4_SEQ4_MAX, seq4, },
        { CDR_TYPECODE_CSTRING, 0, "chararr", U4_STRLEN4, }
};

void test_unions4a (void)
{
	DDS_TypeSupport ts;
	u4_struct data_in, *data_out;
	char  *val0 = "Arcanum boni tenoris animae";
	double val1 = 2 * 355 / 113.0;

	v_printf ("test_unions4a - ");

	ts = DDS_DynamicType_register (u4_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.u4.discriminant = struct4;
	data_in.u4.u.astruct4.str = val0;
	data_in.u4.u.astruct4.d = val1;

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.u4.discriminant == data_out->u4.discriminant);
	fail_unless (data_out->u4.u.astruct4.str != val0);
	fail_unless (strcmp (data_out->u4.u.astruct4.str, val0) == 0);
	fail_unless (data_out->u4.u.astruct4.d == val1);

	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

void test_unions4b (void)
{
	DDS_TypeSupport ts;
	u4_struct data_in, *data_out;
	unsigned i;
	char *val [] = {
		"Lorem ipsum dolor sit amet, consectetur adipisicing elit,",
		"sed do eiusmod tempor incididunt ut labore et dolore ",
		"agna aliqua. Ut enim ad minim veniam, quis nostrud exercitation",
		"ullamco laboris nisi ut aliquip ex ea commodo consequat.",
		"Duis aute irure dolor in reprehenderit in voluptate velit esse cillum",
		"dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat",
		"non proident,unt in culpa qui officia deserunt mollit anim id est laborum."
	};

	v_printf ("test_unions4b - ");

	ts = DDS_DynamicType_register (u4_tsm);
	fail_unless (NULL != ts);

	memset (&data_in, 0, sizeof (data_in));

	data_in.u4.discriminant = seq4;
	DDS_SEQ_INIT (data_in.u4.u.aseq4);
	dds_seq_require (&data_in.u4.u.aseq4, U4_SEQ4_N);
	fail_unless (DDS_SEQ_MAXIMUM (data_in.u4.u.aseq4) == U4_SEQ4_N);

	for (i = 0; i < U4_SEQ4_N; i++)
		strncpy (DDS_SEQ_ITEM (data_in.u4.u.aseq4, i), val [i], U4_STRLEN4);

	marshallUnmarshall (&data_in, (void **) &data_out, ts, 0);

	fail_unless (NULL != data_out);
	fail_unless (&data_in != data_out);
	fail_unless (data_in.u4.discriminant == data_out->u4.discriminant);
	fail_unless (DDS_SEQ_ITEM_PTR (data_in.u4.u.aseq4, 0) != 
		     DDS_SEQ_ITEM_PTR (data_out->u4.u.aseq4, 0));
	fail_unless (DDS_SEQ_ITEM_PTR (data_out->u4.u.aseq4, 0) != NULL);
	fail_unless (DDS_SEQ_ELEM_SIZE (data_out->u4.u.aseq4) == sizeof (chararr));
	fail_unless (DDS_SEQ_MAXIMUM (data_out->u4.u.aseq4) == U4_SEQ4_N);
	fail_unless (DDS_SEQ_LENGTH (data_out->u4.u.aseq4) == U4_SEQ4_N);
	for (i = 0; i < U4_SEQ4_N; i++)
		fail_unless (strcmp (DDS_SEQ_ITEM (data_out->u4.u.aseq4, i), val [i]) == 0);

	dds_seq_cleanup (&data_in.u4.u.aseq4);
	xfree (data_out);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}

void test_unions (void)
{
	test_unions1a ();
	test_unions1b ();
	test_unions2 (); 
	test_unions3a ();
	test_unions3b ();
	test_unions3c ();
	test_unions4a ();
	test_unions4b ();
}

