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
#include "dds/dds_types.h"
#include "dds/dds_tsm.h"
#include "pool.h"
#include "test.h"

struct struct1 {
	char *message;
};

static DDS_TypeSupport_meta tsm1[] = {
	{ CDR_TYPECODE_STRUCT, 2, "struct1", sizeof(struct struct1), 0, 1, },
	{ CDR_TYPECODE_CSTRING, 2, "message", 0, offsetof(struct struct1, message), },
};


/**
 * Basic Unbounded CSTRING test
 */
void test_cstrings1 (void)
{
	DDS_TypeSupport ts;
	struct struct1 tmp, *tmp2;
	v_printf ("test_cstrings1 - ");

	ts = DDS_DynamicType_register (tsm1);
	fail_unless(NULL != ts);

	tmp.message = "This is an unbounded string!";

	marshallUnmarshall(&tmp, (void **)&tmp2, ts, 0);

	fail_unless(NULL != tmp2);
	fail_unless(NULL != tmp2->message);
	fail_unless(tmp.message != tmp2->message);
	fail_unless(!strcmp(tmp.message, tmp2->message));

	xfree(tmp2);

	DDS_DynamicType_free (ts);

	v_printf ("success!\r\n");
}


DDS_SEQUENCE(char*, char_ptr_seq);

/** Sequence of basic int32_t type **/
struct struct2 {
	char_ptr_seq achar_ptr_seq;
};

#define SEQ1_MAX 10
#define SEQ1_N 5
static DDS_TypeSupport_meta tsm2[] = {
	{ CDR_TYPECODE_STRUCT, 2, "struct2", sizeof(struct struct2), 0, 1, },
	{ CDR_TYPECODE_SEQUENCE, 2, "achar_ptr_seq", 0, offsetof (struct struct2, achar_ptr_seq), SEQ1_MAX, },
	{ CDR_TYPECODE_CSTRING, },
};

static char* test_strings[] = {
		"1 One", "2 Two", "3 Three", "4 Four", "5 Five"
};

/**
 * Sequence of char* test
 */
void test_cstrings2 (void)
{
	DDS_TypeSupport ts;
	struct struct2 tmp, *tmp2;
	int i;

	v_printf ("test_cstrings2 - ");
	ts = DDS_DynamicType_register (tsm2);
	fail_unless (NULL != ts);

	memset (&tmp, 0, sizeof (tmp));
	DDS_SEQ_INIT(tmp.achar_ptr_seq);
	dds_seq_require (&tmp.achar_ptr_seq, SEQ1_N);
	for (i = 0; i < SEQ1_N; i++) {
		DDS_SEQ_ITEM_SET(tmp.achar_ptr_seq, i, test_strings[i]);
	}

	marshallUnmarshall (&tmp, (void **)&tmp2, ts, 0);

	fail_unless(NULL != tmp2);
	fail_unless(DDS_SEQ_LENGTH (tmp2->achar_ptr_seq));

	for (i = 0; i < SEQ1_N; i++) {
		// Check for NULL
		fail_unless(NULL != DDS_SEQ_ITEM (tmp.achar_ptr_seq, i));
		fail_unless(NULL != DDS_SEQ_ITEM (tmp2->achar_ptr_seq, i));

		// Check for content
		fail_unless(!strcmp(DDS_SEQ_ITEM (tmp.achar_ptr_seq, i),
				    DDS_SEQ_ITEM (tmp2->achar_ptr_seq, i)));
		fail_unless(!strcmp(DDS_SEQ_ITEM (tmp2->achar_ptr_seq, i),
				    test_strings[i]));

		// Check that the buffer of tmp and tmp2 is not the same buffer .
		fail_unless(DDS_SEQ_ITEM (tmp.achar_ptr_seq, i) != 
			    DDS_SEQ_ITEM (tmp2->achar_ptr_seq, i));
	}

	xfree (tmp2);
	dds_seq_cleanup (&tmp.achar_ptr_seq);

	DDS_TypeSupport_data_free (ts, &tmp, 0);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}

/** Array of basic char type **/
#define ARRAY_SIZE 5
struct struct3 {
	char* char_ptr_array[ARRAY_SIZE];
};

static DDS_TypeSupport_meta tsm3[] = {
	{ CDR_TYPECODE_STRUCT, 2, "struct3", sizeof(struct struct3), 0, 1, },
	{ CDR_TYPECODE_ARRAY,  0, "char_ptr_array", 0, offsetof (struct struct3, char_ptr_array), ARRAY_SIZE, },
	{ CDR_TYPECODE_CSTRING, },
};

static char* test_strings3[] = {
		"This is a", "test sentence", "that is cut", "into several", "pieces."
};

/**
 * Test array of char*
 */
void test_cstrings3 (void)
{
	DDS_TypeSupport ts;
	struct struct3 input, *output;
	int i;

	v_printf ("test_cstrings3 - ");
	ts = DDS_DynamicType_register (tsm3);
	fail_unless (NULL != ts);

	memset (&input, 0, sizeof (input));
	for (i = 0; i < ARRAY_SIZE; i++) {
		input.char_ptr_array[i] = test_strings3[i];
	}

	marshallUnmarshall (&input, (void **) &output, ts, 0);

	fail_unless(NULL != output);
	fail_unless(0 != output->char_ptr_array);

	for (i = 0; i < ARRAY_SIZE; i++) {
		// Check for NULL
		fail_unless(0 != input.char_ptr_array[i]);
		fail_unless(0 != output->char_ptr_array[i]);

		// Check for content
		fail_unless(!strcmp(input.char_ptr_array[i], output->char_ptr_array[i]));
		fail_unless(!strcmp(output->char_ptr_array[i], test_strings3[i]));

		// Check that the buffer of tmp and tmp2 is not the same buffer.
		fail_unless(input.char_ptr_array[i] != output->char_ptr_array[i]);
	}

	xfree (output);
	DDS_DynamicType_free (ts);
	v_printf ("success!\r\n");
}


/**
 *  cstrings tests
 */
void test_cstrings (void)
{
	test_cstrings1 ();
	test_cstrings2 ();
	test_cstrings3 ();
}

