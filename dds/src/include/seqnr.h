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

/* seqnr.h -- Sequence number type definitions and operations. */

#ifndef __seqnr_h_
#define	__seqnr_h_

#ifdef SEQNR64
typedef int64_t SequenceNumber_t;
#else
typedef struct seqnr_st {
	int32_t		high;		/* High part of sequence number. */
	uint32_t	low;		/* Low part of sequence number. */
} SequenceNumber_t;
#endif

typedef struct seqnr_set_st {
	SequenceNumber_t	base;
	uint32_t		numbits;
	uint32_t		bitmap [1];	/* Up to 8 -> 256 bits. */
} SequenceNumberSet;

#define	MIN_SEQNR_SET_SIZE	sizeof (SequenceNumberSet)
#define	MAX_SEQNR_SET_SIZE	(sizeof (SequenceNumberSet) + sizeof (uint32_t) * 7)

#ifdef SEQNR64

#define SEQNR_INC(sn)	(sn)++

/* Increment a sequence number. */

#define	SEQNR_DEC(sn)	(sn)--

/* Decrement a sequence number. */

#define	SEQNR_ADD(sn,d)	(sn) += (d)

/* Add a delta to a sequence number. */

#define	SEQNR_EQ(s1, s2) ((s1) == (s2))

/* Compare two sequence numbers for equality. */

#define	SEQNR_GT(s1, s2) ((s1) > (s2))

/* Compare two sequence numbers for a 'greater than' condition. */

#define	SEQNR_LT(s1, s2) ((s1) < (s2))

/* Compare two sequence numbers for a 'less than' condition. */

#define	SEQNR_VALID(snr) ((s1) > 0)

/* Return true if the sequence number is valid. */

#define	SEQNR_ZERO(snr)	!(snr)

/* Returns true if the sequence number is 0. */

#define	SEQNR_DELTA(s1, s2) ((s2) - (s1))

/* Returns the delta between (s1) and (s2), where (s2) > (s1). */

#else

#define SEQNR_INC(sn)	if(!++(sn).low) (sn).high++

/* Increment a sequence number. */

#define	SEQNR_DEC(sn)	if(!(sn).low--) (sn).high--

/* Decrement a sequence number. */

#define	SEQNR_ADD(sn,d)	STMT_BEG (sn).low += (d); if ((sn).low < (d)) (sn).high++; STMT_END

/* Add a delta to a sequence number. */

#define	SEQNR_EQ(s1, s2) ((s1).low == (s2).low && (s1).high == (s2).high)

/* Compare two sequence numbers for equality. */

#define	SEQNR_GT(s1, s2) ((s1).high > (s2).high || \
	((s1).high == (s2).high && (s1).low > (s2).low))

/* Compare two sequence numbers for a 'greater than' condition. */

#define	SEQNR_LT(s1, s2) ((s1).high < (s2).high || \
	((s1).high == (s2).high && (s1).low < (s2).low))

/* Compare two sequence numbers for a 'less than' condition. */

#define	SEQNR_VALID(snr) ((!snr.high && snr.low) || snr.high > 0)

/* Return true if the sequence number is valid. */

#define	SEQNR_ZERO(snr)	(!(snr).high && !(snr).low)

/* Returns true if the sequence number is 0. */

#define	SEQNR_DELTA(s1, s2) (((s2).high == (s1).high) ? (s2).low - (s1).low : \
				((s2).high == ((s1).high + 1)) ? \
					~0U - (s1).low + ((s2).low + 1): ~0U)

/* Returns the delta between (s1) and (s2), where (s2) > (s1). */

#endif

#endif /* !__seqnr_h_ */

