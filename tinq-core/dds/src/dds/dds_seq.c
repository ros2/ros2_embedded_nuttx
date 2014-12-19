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

/* dds_seq.c -- Implements various sequence operations. */

#include "pool.h"
#include "dds/dds_error.h"
#include "dds/dds_seq.h"

/* dds_seq_extend -- Extend a sequence with the given # of elements. */

static DDS_ReturnCode_t dds_seq_extend (void *seq, unsigned n)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	void		*p;

	if (!sp->_maximum) {
		p = xmalloc ((sp->_maximum + n) * sp->_esize);
		sp->_own = 1;
	}
	else {
		if (!sp->_own)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		p = xrealloc (sp->_buffer, (sp->_maximum + n) * sp->_esize);
	}
	if (!p)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	sp->_buffer = p;
	sp->_maximum += n;
	return (DDS_RETCODE_OK);
}

/* dds_seq_require -- Set sequence maximum/length to the given # of elements. */

DDS_ReturnCode_t dds_seq_require (void *seq, unsigned n)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	int		ret;

	if (sp->_maximum < n) {
		ret = dds_seq_extend (seq, n - sp->_maximum);
		if (ret)
			return (ret);
	}
	if (n > sp->_length)
		memset ((char *) sp->_buffer + sp->_length * sp->_esize,
			0,
			(n - sp->_length) * sp->_esize);
	sp->_length = n;
	return (DDS_RETCODE_OK);
}

/* dds_seq_cleanup  -- Cleanup a sequence.  This will free all element storage. */

void dds_seq_cleanup (void *seq)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;

	if (!sp->_maximum || !sp->_own)
		return;

	if (sp->_buffer) {
		xfree (sp->_buffer);
		sp->_buffer = NULL;
	}
	sp->_length = sp->_maximum = 0;
}

/* dds_seq_reset -- Reset a sequence to no elements. */

void dds_seq_reset (void *seq)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;

	sp->_length = 0;
}

/* dds_seq_append-- Append a value to a sequence. The function will reallocate
		    space and copy, if necessary. */

DDS_ReturnCode_t dds_seq_append (void *seq, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;
	int		ret;

	if (sp->_length == sp->_maximum) {
		ret = dds_seq_extend (seq, 1);
		if (ret)
			return (ret);
	}
	cp = (char *) sp->_buffer;
	cp += sp->_esize * sp->_length;
	memcpy (cp, value, sp->_esize);
	sp->_length++;
	return (DDS_RETCODE_OK);
}

/* dds_seq_prepend -- Add a value to the beginning of the sequence. This
		      function will reallocate space and copy, if necessary. */

DDS_ReturnCode_t dds_seq_prepend (void *seq, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;
	int		ret;

	if (sp->_length == sp->_maximum) {
		ret = dds_seq_extend (seq, 1);
		if (ret)
			return (ret);

	}
	cp = (char *) sp->_buffer;
	memmove (cp + sp->_esize, cp, sp->_esize * sp->_length);
	memcpy (cp, value, sp->_esize);
	sp->_length++;
	return (DDS_RETCODE_OK);
}

/* dds_seq_insert -- Insert a value at the given position of the sequence.  The
		     current sequence must already be large enough to be able to
		     do this. */

DDS_ReturnCode_t dds_seq_insert (void *seq, unsigned pos, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;
	int		ret;

	if (!pos)
		return (dds_seq_prepend (seq, value));

	if (pos > sp->_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (pos == sp->_length)
		return (dds_seq_append (seq, value));

	if (sp->_length == sp->_maximum) {
		ret = dds_seq_extend (seq, 1);
		if (ret)
			return (ret);
	}
	cp = (char *) sp->_buffer + sp->_esize * pos;
	memmove (cp + sp->_esize, cp, (sp->_length - pos) * sp->_esize);
	memcpy (cp, value, sp->_esize);
	sp->_length++;
	return (DDS_RETCODE_OK);
}

/* dds_seq_replace -- Replace a value at the given (existing) position of the
		      sequence. */

DDS_ReturnCode_t dds_seq_replace (void *seq, unsigned pos, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;

	if (pos >= sp->_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	cp = (char *) sp->_buffer + sp->_esize * pos;
	memcpy (cp, value, sp->_esize);
	return (DDS_RETCODE_OK);
}

/* dds_seq_remove_first -- Remove the first value from the sequence, and places
			   it in the location pointed to by value. */

DDS_ReturnCode_t dds_seq_remove_first (void *seq, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;

	if (!sp->_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (value)
		memcpy (value, sp->_buffer, sp->_esize);
	cp = (char *) sp->_buffer;
	memmove (cp, cp + sp->_esize, sp->_length * sp->_esize);
	sp->_length--;
	return (DDS_RETCODE_OK);
}

/* dds_seq_remove_last -- Remove the last value from the sequence, and places
			  it in the location pointed to by value. */

DDS_ReturnCode_t dds_seq_remove_last (void *seq, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;

	if (!sp->_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	sp->_length--;
	cp = (char *) sp->_buffer;
	cp += sp->_length * sp->_esize;
	if (value)
		memcpy (value, cp, sp->_esize);
	return (DDS_RETCODE_OK);
}

/* dds_seq_remove -- Remove the element at the given position from the sequence,
		     and place it in the location pointed to by value. */

DDS_ReturnCode_t dds_seq_remove (void *seq, unsigned pos, void *value)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;

	if (pos >= sp->_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (pos == sp->_length - 1)
		return (dds_seq_remove_last (seq, value));

	cp = (char *) sp->_buffer + (pos * sp->_esize);
	if (value)
		memcpy (value, cp, sp->_esize);
	sp->_length--;
	memmove (cp, cp + sp->_esize, (sp->_length - pos) * sp->_esize);
	return (DDS_RETCODE_OK);
}

/* dds_seq_every -- Call the function func on each element of a sequence,
		    passing data as the second argument to func. */

void dds_seq_every (void *seq, void (*func)(void *, void *), void *data)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;
	char		*cp;
	unsigned	i;

	for (i = 0, cp = sp->_buffer; i < sp->_length; i++, cp += sp->_esize)
		(*func) (cp, data);
}

/* dds_seq_from_array -- Initialize a sequence from an array of length
			 elements. */

DDS_ReturnCode_t dds_seq_from_array (void *seq, void *array, unsigned length)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;

	if (dds_seq_require (seq, length))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	memcpy (sp->_buffer, array, length * sp->_esize);
	return (DDS_RETCODE_OK);
}


/* dds_seq_to_array -- Copy a sequence to an array of maximum length elements.*/

unsigned dds_seq_to_array (void *seq, void *array, unsigned length)
{
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) seq;

	if (sp->_length < length)
		length = sp->_length;
	if (length)
		memcpy (array, sp->_buffer, length * sp->_esize);
	return (length);
}

/* dds_seq_copy -- Copy a sequence to another sequence, overwriting the
		   destination sequence completely.  If the destination
		   sequence already contained data, this data is removed
		   using dds_seq_cleanup(). */

DDS_ReturnCode_t dds_seq_copy (void *dst, void *src)
{
	DDS_VoidSeq	*dp = (DDS_VoidSeq *) dst;
	DDS_VoidSeq	*sp = (DDS_VoidSeq *) src;

	if (!dp || !sp || dp->_esize != sp->_esize)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp->_length && dds_seq_require (dst, sp->_length))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	dp->_length = sp->_length;
	if (sp->_buffer)
		memcpy (dp->_buffer, sp->_buffer, sizeof (uint32_t) * sp->_length);
	return (DDS_RETCODE_OK);
}

/* dds_seq_equal -- Compare two sequences for equality. */

int dds_seq_equal (void *seq1, void *seq2)
{
	DDS_VoidSeq	*s1 = (DDS_VoidSeq *) seq1;
	DDS_VoidSeq	*s2 = (DDS_VoidSeq *) seq2;

	if (s1->_esize != s2->_esize ||
	    s1->_length != s2->_length)
		return (0);

	if (!s1->_length)
		return (1);

	return (!memcmp (s1->_buffer, s2->_buffer, s1->_esize * s1->_length));
}

