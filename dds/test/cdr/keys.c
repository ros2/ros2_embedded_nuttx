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
#include "dds/dds_dcps.h"
#include "crc32.h"
#include "test.h"

#define	MSG_SIZE	100

struct hw_data1 {
	uint64_t	counter;
	uint32_t	key;			/* Key */
	char		message [MSG_SIZE];
};

static DDS_TypeSupport_meta msg_data_tsm1 [] = {
	{ CDR_TYPECODE_STRUCT, 1, "HelloWorld1", sizeof (struct hw_data1), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "counter", 0, offsetof (struct hw_data1, counter), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  1, "key", 0, offsetof (struct hw_data1, key), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "message", MSG_SIZE, offsetof (struct hw_data1, message), 0, 0, NULL }
};

void test_key1 (void)
{
	struct hw_data1 tmp, *tmp2;
	DDS_TypeSupport ts;

	v_printf ("test_key1 - ");
	ts = DDS_DynamicType_register (msg_data_tsm1);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	tmp.counter = 12345678;
	tmp.key = 0xac1dbabe;
	strcpy (tmp.message, "Hi folks!");

	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);
	fail_unless (tmp.counter == tmp2->counter &&
		     tmp.key == tmp2->key &&
		     !strcmp (tmp.message, tmp2->message));
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

struct hw_data2 {
	uint64_t	counter;
	uint32_t	key [5];		/* Key */
	char		*message;
};

static DDS_TypeSupport_meta msg_data_tsm2 [] = {
	{ CDR_TYPECODE_STRUCT, 3, "HelloWorld2", sizeof (struct hw_data2), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "counter", 0, offsetof (struct hw_data2, counter), 0, 0, NULL },
	{ CDR_TYPECODE_ARRAY,  1, "key", sizeof (uint32_t [5]), offsetof (struct hw_data2, key), 5, 0, NULL },
	{ CDR_TYPECODE_ULONG,  1, NULL, 0, 0, 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 2, "message", 0, offsetof (struct hw_data2, message), 0, 0, NULL }
};

void test_key2 (void)
{
	struct hw_data2 tmp, *tmp2;
	DDS_TypeSupport ts;

	v_printf ("test_key2 - ");
	ts = DDS_DynamicType_register (msg_data_tsm2);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	tmp.counter = 32345670;
	tmp.key [0] = 0xaa00bb01;
	tmp.key [1] = 0xcc00bb02;
	tmp.key [2] = 0xee00bb03;
	tmp.key [3] = 0xff00bb04;
	tmp.key [4] = 0x9900bb05;
	tmp.message = "Hi folks!";

	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);
	fail_unless (tmp.counter == tmp2->counter &&
		     !memcmp (tmp.key, tmp2->key, sizeof (tmp.key)) &&
		     !strcmp (tmp.message, tmp2->message));
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

struct hw_data3 {
	uint64_t	counter;		/* Key */
	uint32_t	key;
	char		*message;		/* Key */
};

static DDS_TypeSupport_meta msg_data_tsm3 [] = {
	{ CDR_TYPECODE_STRUCT, 3, "HelloWorldData", sizeof (struct hw_data3), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG, 1, "counter", 0, offsetof (struct hw_data3, counter), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG, 0, "key", 0, offsetof (struct hw_data3, key), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 3, "message", 0, offsetof (struct hw_data3, message), 0, 0, NULL }
};

void test_key3 (void)
{
	struct hw_data3 tmp, *tmp2;
	DDS_TypeSupport ts;

	v_printf ("test_key3 - ");
	ts = DDS_DynamicType_register (msg_data_tsm3);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	tmp.counter = 32345671;
	tmp.key = 0xdd00bb01;
	tmp.message = "Hi folks, again!";

	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);
	fail_unless (tmp.counter == tmp2->counter &&
		     tmp.key == tmp2->key &&
		     !strcmp (tmp.message, tmp2->message));
	xfree (tmp2);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

struct hw_data4 {
	DDS_OctetSeq	key;			/* Key */
	char		*message ;
};

#define SEQ4_MAX 5
#define SEQ4_N 2

static DDS_TypeSupport_meta msg_data_tsm4 [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC | TSMFLAG_KEY, "OctetSeqTest", sizeof (struct hw_data4), 0, 2, 0, NULL },
	{ CDR_TYPECODE_SEQUENCE, TSMFLAG_KEY, "key", 0, offsetof (struct hw_data4, key), SEQ4_MAX, },
	{ CDR_TYPECODE_OCTET, },
	{ CDR_TYPECODE_CSTRING, TSMFLAG_DYNAMIC | TSMFLAG_KEY, "message", 0, offsetof (struct hw_data4, message), 0, 0, NULL }
};

void test_key4 (void)
{
	DDS_TypeSupport ts;
	struct hw_data4 tmp, *tmp2;
	unsigned char i;

	v_printf ("test_key4 - ");
	ts = DDS_DynamicType_register (msg_data_tsm4);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT(tmp.key);
	tmp.message = "Hi folks, again!";
	dds_seq_require (&tmp.key, SEQ4_N);
	for (i = 0; i < SEQ4_N; i++)
		DDS_SEQ_ITEM (tmp.key, i) = 79; /*0x4f*/

	marshallUnmarshall(&tmp, (void **) &tmp2, ts, 0);
	
	fail_unless (!strcmp (tmp.message, tmp2->message));
	fail_unless (DDS_SEQ_LENGTH (tmp2->key) == SEQ4_N);
	for (i = 0; i < SEQ4_N; i++)
		fail_unless (DDS_SEQ_ITEM (tmp.key, i) ==
			     DDS_SEQ_ITEM (tmp2->key, i));

	dds_seq_cleanup (&tmp.key);
	xfree (tmp2);
	
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

DDS_SEQUENCE(int, intSeq);

struct hw_data5 {
	intSeq		key;			/* Key */
	char		message [MSG_SIZE];
};
#define SEQ5_MAX 7
#define SEQ5_N 4
static DDS_TypeSupport_meta msg_data_tsm5 [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC | TSMFLAG_KEY, "IntSeqTest", sizeof (struct hw_data5), 0, 2, 0, NULL },
	{ CDR_TYPECODE_SEQUENCE, TSMFLAG_KEY, "key", 0, offsetof (struct hw_data5, key), SEQ5_MAX, },
	{ CDR_TYPECODE_LONG, },
	{ CDR_TYPECODE_CSTRING, 2, "message", MSG_SIZE, offsetof (struct hw_data5, message), 0, 0, NULL }
};

void test_key5 (void)
{
	DDS_TypeSupport ts;
	struct hw_data5 tmp, *tmp2;
	unsigned char i;

	v_printf ("test_key5 - ");
	ts = DDS_DynamicType_register (msg_data_tsm5);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT (tmp.key);
	dds_seq_require (&tmp.key, SEQ5_N);
	for (i = 0; i < SEQ5_N; i++)
		DDS_SEQ_ITEM (tmp.key, i) = 0x3f;
	strcpy (tmp.message, "Hi folks, again!");

	marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);
	
	fail_unless (DDS_SEQ_LENGTH (tmp2->key) == SEQ5_N);
	for (i = 0; i < SEQ5_N; i++)
		fail_unless (DDS_SEQ_ITEM (tmp.key, i) ==
			     DDS_SEQ_ITEM (tmp2->key, i));

	fail_unless (!strcmp (tmp.message, tmp2->message));

	dds_seq_cleanup (&tmp.key);
	xfree (tmp2);
	
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

struct testStruct{
	int16_t		i16;
};

DDS_SEQUENCE(struct testStruct, structSeq);

struct hw_data6 {
	structSeq	key;			/* Key */
	char		message [MSG_SIZE];
};

#define SEQ6_MAX 7
#define SEQ6_N 4

static DDS_TypeSupport_meta msg_data_tsm6 [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC | TSMFLAG_KEY, "StructSeqTest", sizeof (struct hw_data6), 0, 2, 0, NULL },
	{ CDR_TYPECODE_SEQUENCE, TSMFLAG_KEY, "key", 0, offsetof (struct hw_data6, key), SEQ6_MAX, },
	{ CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "testStruct", sizeof(struct testStruct), 0, 1, },
	{ CDR_TYPECODE_SHORT, TSMFLAG_KEY, "i16", 0, offsetof(struct testStruct, i16), },
	{ CDR_TYPECODE_CSTRING, 2, "message", MSG_SIZE, offsetof (struct hw_data6, message), 0, 0, NULL }
};

void test_key6 (void)
{
	DDS_TypeSupport ts;
	struct testStruct *sp, *dp;
	struct hw_data6 tmp, *tmp2;
	unsigned char i;

	v_printf ("test_key6 - ");
	ts = DDS_DynamicType_register (msg_data_tsm6);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT (tmp.key);
	dds_seq_require (&tmp.key, SEQ6_N);
	for (i = 0; i < SEQ6_N; i++) {
		sp = DDS_SEQ_ITEM_PTR (tmp.key, i);
		sp->i16 = -17;
	}
	strcpy (tmp.message , "Hi folks, again!");

	marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);
	
	fail_unless (DDS_SEQ_LENGTH (tmp2->key) == SEQ6_N);
	for (i = 0; i < SEQ6_N; i++) {
		sp = DDS_SEQ_ITEM_PTR (tmp.key, i);
		dp = DDS_SEQ_ITEM_PTR (tmp2->key, i);
		fail_unless (sp->i16 == dp->i16);
	}
	fail_unless (!strcmp (tmp.message, tmp2->message));

	dds_seq_cleanup (&tmp.key);
	xfree (tmp2);
	
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

struct struct3b {
	char c;
	uint64_t ull;
};
struct struct3 {
	int32_t l_3;
	struct struct3b s_3b;			/* Key */
};

static DDS_TypeSupport_meta tsm3 [] = {
	{ CDR_TYPECODE_STRUCT, 0,  "struct3", sizeof(struct struct3), 0, 2, 0, NULL },
	{ CDR_TYPECODE_LONG, 0, "l_3", 0, offsetof (struct struct3, l_3), 0, 0, NULL },
	{ CDR_TYPECODE_STRUCT, 0, "s_3b", sizeof(struct struct3b), offsetof (struct struct3, s_3b), 2, 0, NULL },
	{ CDR_TYPECODE_CHAR, 0, "c",  0, offsetof (struct struct3b, c), 0, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG, 0, "ull",  0, offsetof (struct struct3b, ull), 0, 0, NULL }
};

void test_key7 (void)
{
	DDS_TypeSupport ts;
	struct struct3 tmp, *tmp2;
	unsigned char s;

	memset (&tmp, 0, sizeof (tmp));
	tmp.l_3 = 123;
	tmp.s_3b.c = 'j';
	tmp.s_3b.ull = 0xac1dbabedeadb0d1ull;
	for (s = 0; s < 16; s++) {

		v_printf ("test_key7.%u - ", s);

		/* Setup correct key field attributes. */
		if ((s & 8) != 0)
			tsm3 [0].flags |= TSMFLAG_KEY;
		else
			tsm3 [0].flags &= ~TSMFLAG_KEY;
		if ((s & 4) != 0)
			tsm3 [1].flags |= TSMFLAG_KEY;
		else
			tsm3 [1].flags &= ~TSMFLAG_KEY;
		if ((s & 2) != 0)
			tsm3 [2].flags |= TSMFLAG_KEY;
		else
			tsm3 [2].flags &= ~TSMFLAG_KEY;
		if ((s & 1) != 0)
			tsm3 [4].flags |= TSMFLAG_KEY;
		else
			tsm3 [4].flags &= ~TSMFLAG_KEY;

		ts = DDS_DynamicType_register (tsm3);
		fail_unless (NULL != ts);

		marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);

		fail_unless (tmp.l_3 == tmp2->l_3);
		fail_unless (tmp.s_3b.c == tmp2->s_3b.c);
		fail_unless (tmp.s_3b.ull == tmp2->s_3b.ull);
		xfree (tmp2);
	
		DDS_DynamicType_free (ts);

		if (s == 2)
			s = 7;

		v_printf ("success!\r\n");
	}
}

struct struct4a {
    int64_t upper;
    int64_t lower;
};
const DDS_TypeSupport_meta tsm4a [] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "idstruct", .flags = TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(struct struct4a) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "upper", .offset = offsetof(struct struct4a, upper) },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "lower", .offset = offsetof(struct struct4a, lower) },  
};

struct struct4 {
        struct struct4a id;
        struct struct4a id2;
        char * str;
        unsigned char bval;
};
const DDS_TypeSupport_meta tsm4 [] = {
        { .tc = CDR_TYPECODE_STRUCT, .name = "doublekey", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 4, .size = sizeof(struct struct4) },
        { .tc = CDR_TYPECODE_TYPEREF, .name = "id", .flags = TSMFLAG_KEY, .offset = offsetof(struct struct4, id), .tsm = tsm4a },
        { .tc = CDR_TYPECODE_TYPEREF, .name = "id2", .flags = TSMFLAG_KEY, .offset = offsetof(struct struct4, id2), .tsm = tsm4a },
        { .tc = CDR_TYPECODE_CSTRING, .name = "str", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(struct struct4, str), .size = 0 },
        { .tc = CDR_TYPECODE_BOOLEAN, .name = "bval", .offset = offsetof(struct struct4, bval) },
};

void test_key8 (void)
{
	DDS_TypeSupport ts;
	struct struct4 tmp, *tmp2;
	unsigned char i;

        DDS_set_generate_callback(crc32_char);

	v_printf ("test_key8 - ");
	ts = DDS_DynamicType_register (tsm4);
	fail_unless (NULL != ts);

        /* fill in tmp */
	memset (&tmp, 0, sizeof (tmp));
        tmp.id.upper = 1;
        tmp.id.lower = 2;
        tmp.id2.upper = 3;
        tmp.id2.lower = 4;

	marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);
	
	xfree (tmp2);
	
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

# if 0
typedef enum {
	struct4 = 1,
	seq4 = 0,
} u4_disc;

typedef struct {
	char  *str;
	double d;
} u4_substruct;

typedef char chararr [3];

DDS_SEQUENCE (chararr, u4_seq4);

typedef union {
	u4_substruct astruct4;
	u4_seq4 aseq4;
} u4_union;

DDS_UNION (u4_union, u4_disc, u4_dds_union);

typedef struct {
	u4_dds_union u4;			/* Key */
	int32_t key;				/* Key */
} u4_struct;

static DDS_TypeSupport_meta u4_tsm[] = {
        { CDR_TYPECODE_STRUCT, .flags = 3,  "union_struct4", .flags = TSMFLAG_DYNAMIC, .size = sizeof(u4_struct), .nelem = 1 },
        { CDR_TYPECODE_UNION, .flags = 1,  "u4", .flags = TSMFLAG_DYNAMIC, .nelem = 2,.size = sizeof(u4_dds_union) },
        { CDR_TYPECODE_STRUCT,  "u", .flags = TSMFLAG_DYNAMIC,.offset = offsetof (u4_dds_union, u), .size = sizeof(u4_substruct), .nelem = 2, .label = struct4 },
        { CDR_TYPECODE_CSTRING,  "str", .size = 0, .offset = offsetof(u4_substruct, str) },
        { CDR_TYPECODE_DOUBLE,  "d", .offset = offsetof (u4_substruct, d) },
        { CDR_TYPECODE_SEQUENCE, .flags = 2,  "aseq4", .nelem = U4_SEQ4_MAX, .offset = offsetof (u4_dds_union, u), .label = seq4 },
        { CDR_TYPECODE_CSTRING, .size = U4_STRLEN4 },
	{ CDR_TYPECODE_LONG, .flags = 1,  "key", .offset = offsetof(u4_struct, key) }
};

# endif

void test_keys (void)
{
	test_key1 ();
	test_key2 ();
	test_key3 ();
	test_key4 ();
	test_key5 ();
	test_key6 ();
	test_key7 ();
	test_key8 ();
}

