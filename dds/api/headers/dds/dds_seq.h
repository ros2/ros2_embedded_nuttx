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

/* dds_seq.h -- Implements various sequence operations as used in DDS. */

#ifndef __dds_seq_h_
#define	__dds_seq_h_

#include "dds/dds_error.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* Create a new type as a sequence of the given type: */
#define	DDS_SEQUENCE(type,name)			\
typedef struct name ## _st { 			\
	unsigned _maximum;			\
	unsigned _length;			\
	unsigned _esize;			\
	int      _own;				\
	type	 *_buffer;			\
} name

/* The sequence type used by generic code */
DDS_SEQUENCE(void, DDS_VoidSeq);
DDS_SEQUENCE(void *, DDS_VoidPtrSeq);

/* Initializer for a sequence of the given type: */
#define	DDS_SEQ_INITIALIZER(type)		\
     {	/* _maximum */ 0,			\
	/* _length */ 0,			\
	/* _esize */ sizeof(type),		\
	/* _own */ 1,				\
	/* _buffer */ NULL }

/* Initialize the sequence -- mandatory before first use if DDS_SEQ_INITIALIZER
   wasn't used! */
#define	DDS_SEQ_INIT(seq)			\
	(seq)._maximum = (seq)._length  = 0;	\
	(seq)._esize = sizeof (*(seq)._buffer);	\
	(seq)._own = 1;				\
	(seq)._buffer = NULL

/* Return/set an item from a sequence at the given index. Caller should make
   sure that there is enough room in the sequence to get/set the value: */
#define	DDS_SEQ_ITEM(seq,i)	(seq)._buffer [i]
#define	DDS_SEQ_ITEM_SET(seq,i,v) (seq)._buffer [i] = (v)

/* Return a pointer to the first data element of a sequence. */
#define	DDS_SEQ_DATA(seq)	(seq)._buffer

/* Return a pointer to an item at the given index of the sequence: */
#define	DDS_SEQ_ITEM_PTR(seq,i)	((seq)._length > i ? &(seq)._buffer [i] : NULL)

/* Return the length of a sequence: */
#define DDS_SEQ_LENGTH(seq)	(seq)._length

/* Return the current maximum capacity of a sequence: */
#define DDS_SEQ_MAXIMUM(seq)	(seq)._maximum

/* Get/set the 'owned' field of a sequence: */
#define	DDS_SEQ_OWNED(seq)	(seq)._own
#define	DDS_SEQ_OWNED_SET(seq,o) (seq)._own = (o)

/* Get the element size of a sequence. */
#define DDS_SEQ_ELEM_SIZE(seq) (seq)._esize

/* Initializer for a sequence with 1 element of the given type. */
#define	DDS_SEQ_INIT_PTR(seq,ptr)		\
	(seq)._maximum = (seq)._length = 1;	\
	(seq)._esize = sizeof(*(seq)._buffer);	\
	(seq)._own = 1;				\
	(seq)._buffer = &ptr

/* Set the sequence length to the given number of elements.  Note that new
   sequence elements will simply be zeroed. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_require (
	void *seq,
	unsigned n
);

/* Cleanup a sequence.  This will free all element storage. */
DDS_EXPORT void dds_seq_cleanup (
	void *seq
);

/* Reset a sequence to having no elements. */
DDS_EXPORT void dds_seq_reset (
	void *seq
);

/* Append a value to a sequence. The function will reallocate space and copy, if
   necessary. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_append (
	void *seq,
	void *value
);

/* Push a value on to the beginning of the sequence. This function will 
   reallocate space and copy, if necessary. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_prepend (
	void *seq,
	void *value
);

/* Insert a value at the given position of the sequence.  The current sequence
   must already be large enough to be able to do this.
   Note: use dds_seq_require() if the current length is not large enough. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_insert (
	void *seq,
	unsigned pos,
	void *value
);

/* Replace a value at the given (existing) position of the sequence. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_replace (
	void *seq,
	unsigned pos,
	void *value
);

/* Remove the first value from the sequence, and places it in the location
   pointed to by value. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_remove_first (
	void *seq,
	void *value
);

/* Remove the last value from the sequence, and places it in the location
   pointed to by value. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_remove_last (
	void *seq,
	void *value
);

/* Remove the element at the given position from the sequence, and place it in
   the location pointed to by value. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_remove (
	void *seq,
	unsigned pos,
	void *value
);

/* Call the function func on each element of a sequence, passing data as the
   second argument to func. */
DDS_EXPORT void dds_seq_every (
	void *seq,
	void (*func)(void *, void *),
	void *data
);

/* Walk over every element in the sequence.  The i will give the current
   sequence element index. */
#define	DDS_SEQ_FOREACH(seq,i) for (i = 0; i < DDS_SEQ_LENGTH(seq); i++)

/* Walk over every element in the sequence.  The ptr argument must be a pointer
   to a sequence element and will advance over the sequence, while the i
   will give the current sequence element index. */
#define	DDS_SEQ_FOREACH_ENTRY(seq,i,ptr) for (i = 0, ptr = DDS_SEQ_DATA(seq); \
					    i < DDS_SEQ_LENGTH(seq); i++,     \
					    ptr = (void *) ((char *) ptr + (seq)._esize))

/* Initialize a sequence from an array of length elements. */
DDS_EXPORT DDS_ReturnCode_t dds_seq_from_array (
	void *seq,
	void *aray,
	unsigned length
);

/* Copy a sequence to an array of maximum length elements.  If the array is not
   big enough to hold the complete sequence, only length elements will be
   copied.  The number of valid array elements is returned. */
DDS_EXPORT unsigned dds_seq_to_array (
	void *seq,
	void *aray,
	unsigned length
);

/* Copy a sequence to another sequence, overwriting the destination sequence
   completely.  If the destination sequence already contained data, the data
   is removed using dds_seq_cleanup(). */
DDS_EXPORT DDS_ReturnCode_t dds_seq_copy (
	void *dst,
	void *src
);

/* Compare two sequences for equality. */
DDS_EXPORT int dds_seq_equal (
	void *s1,
	void *s2
);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_seq_h_ */

