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

/* vtc.h -- Interface to the processing of vendor-specific typecode data. */

#ifndef __vtc_h_
#define	__vtc_h_

#define	MIN_MEMBER_SIZE	(2 + 8 + 4 + 8 + 2)
#define	MIN_STRUCT_SIZE	((2 + 4 + 8 + 4) + MIN_MEMBER_SIZE)

#define	NRE_NREFS	0x3fff		/* # of references. */
#define	NRE_EXT_M	0xc000		/* Extensibility (mask). */
#define	NRE_EXT_S	14		/* Shift value for extensibility. */

typedef struct vtc_header_st {
	uint32_t	stype;		/* Struct typecode (msb set). */
	uint16_t	length;		/* Total length of typecode - 6. */
	uint16_t	nrefs_ext;	/* # of references to typecode. */
	unsigned char	data [MIN_MEMBER_SIZE + 10];	/* Remainder. */
} VTC_Header_t;

#include "typecode.h"

int vtc_validate (const unsigned char *vtc,
		  size_t              length,
		  unsigned            *ofs,
		  int                 swap,
		  int                 ext);

/* Validate transported typecode in vendor-specific format.   If swap is set,
   the source data (vtc) needs to be swapped for correct interpretation.
   If ext is set, vendor typecode format is assumed extended for X-types.
   If something went wrong during validation, a non-0 result is returned.
   Otherwise, 0 is returned and the data will be converted to native vendor-
   specific format. */

unsigned char *vtc_create (const TypeSupport_t *ts);

/* Create vendor-specific typecode data from a previously created type. */

void vtc_free (unsigned char *vtc);

/* Free typecode created by vtc_create(). */

TypeSupport_t *vtc_type (TypeLib *lp, unsigned char *vtc);

/* Create a type representation from vendor-specific typecode data. */

void vtc_delete (TypeSupport_t *ts);

/* Delete a type originating from typecode data. */

int vtc_equal (const unsigned char *vtc1, const unsigned char *vtc2);

/* Compare two typecode types for equality. */

int vtc_identical (const TypeSupport_t *ts, const unsigned char *vtc);

/* Verify if the given types are identical and return a non-0 result if so. */

int vtc_compatible (TypeSupport_t *ts, const unsigned char *vtc, int *same);

/* Verify if the given types are compatible and return a non-0 result if so. */

#endif /* !__vtc_h_ */

