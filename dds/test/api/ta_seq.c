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

#include "test.h"
#include "ta_seq.h"

void test_oseq (void)
{
	DDS_OctetSeq		*osp;
	DDS_ReturnCode_t	r;
	unsigned		n;
	unsigned char		c, buffer [30], exp [] = {
		35, 0, 0, 0, 38, 0, 0, 0,
		37, 0, 0, 0,  0, 0, 0, 0,
		 0, 0, 0, 0,  0
	};

	osp = DDS_OctetSeq__alloc ();
	fail_unless (osp != NULL &&
		     DDS_SEQ_ELEM_SIZE (*osp) == 1 &&
		     DDS_SEQ_OWNED (*osp) &&
		     DDS_SEQ_LENGTH (*osp) == 0 &&
		     DDS_SEQ_MAXIMUM (*osp) == 0);
	r = dds_seq_require (osp, 100);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 100 &&
		     DDS_SEQ_MAXIMUM (*osp) == 100);
	r = dds_seq_require (osp, 10);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 10 &&
		     DDS_SEQ_MAXIMUM (*osp) == 100);
	dds_seq_cleanup (osp);
	fail_unless (DDS_SEQ_LENGTH (*osp) == 0 &&
		     DDS_SEQ_MAXIMUM (*osp) == 0);
	r = dds_seq_require (osp, 30);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 30 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	r = dds_seq_require (osp, 20);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 20 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	c = 34;
	r = dds_seq_append (osp, &c);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 21 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	c = 35;
	r = dds_seq_prepend (osp, &c);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 22 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	c = 36;
	r = dds_seq_prepend (osp, &c);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 23 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	c = 37;
	r = dds_seq_insert (osp, 10, &c);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 24 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	c = 38;
	r = dds_seq_replace (osp, 5, &c);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 24 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	r = dds_seq_remove_last (osp, &c);
	fail_unless (r == 0 &&
		     c == 34 &&
		     DDS_SEQ_LENGTH (*osp) == 23 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	r = dds_seq_remove_first (osp, &c);
	fail_unless (r == 0 &&
		     c == 36 &&
		     DDS_SEQ_LENGTH (*osp) == 22 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	r = dds_seq_remove (osp, 5, &c);
	fail_unless (r == 0 &&
		     c == 0 &&
		     DDS_SEQ_LENGTH (*osp) == 21 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	n = dds_seq_to_array (osp, buffer, sizeof (buffer));
	fail_unless (n == 21 && !memcmp (buffer, exp, n));

	dds_seq_reset (osp);
	fail_unless (DDS_SEQ_LENGTH (*osp) == 0 &&
		     DDS_SEQ_MAXIMUM (*osp) == 30);
	c = 37;
	r = dds_seq_insert (osp, 10, &c);
	fail_unless (r == DDS_RETCODE_OUT_OF_RESOURCES);

	DDS_OctetSeq__free (osp);
}

void test_sseq (void)
{
	DDS_StringSeq		*ssp;
	DDS_ReturnCode_t	r;
	char			s0 [] = "Hi", s1 [] = "Folks", s2 [] = "!";
	char			*sp;

	ssp = DDS_StringSeq__alloc ();
	fail_unless (ssp != NULL &&
		     DDS_SEQ_ELEM_SIZE (*ssp) == sizeof (char *) &&
		     DDS_SEQ_OWNED (*ssp) &&
		     DDS_SEQ_LENGTH (*ssp) == 0 &&
		     DDS_SEQ_MAXIMUM (*ssp) == 0);

	sp = s0;
	r = dds_seq_prepend (ssp, &sp);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*ssp) == 1 &&
		     DDS_SEQ_MAXIMUM (*ssp) == 1);
	sp = s1;
	r = dds_seq_prepend (ssp, &sp);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*ssp) == 2 &&
		     DDS_SEQ_MAXIMUM (*ssp) == 2);
	sp = s2;
	r = dds_seq_insert (ssp, 0, &sp);
	fail_unless (r == 0 &&
		     DDS_SEQ_LENGTH (*ssp) == 3 &&
		     DDS_SEQ_MAXIMUM (*ssp) == 3 &&
		     !strcmp (DDS_SEQ_ITEM (*ssp, 0), s2) &&
		     !strcmp (DDS_SEQ_ITEM (*ssp, 1), s1) &&
		     !strcmp (DDS_SEQ_ITEM (*ssp, 2), s0));
	DDS_StringSeq__free (ssp);
}

void test_sequences (void)
{
	dbg_printf ("Sequences ");
	if (trace)
		fflush (stdout);
	if (verbose)
		printf ("\r\n");
	test_oseq ();
	test_sseq ();
	dbg_printf (" - success!\r\n");
}


