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

/* str.h -- Implements an octet string handling library for data strings with
	    sizes up to 64KB where the string remains constant during the
	    lifetime of it. */

#ifndef __str_h_
#define	__str_h_

#include <stdlib.h>
#include "dds/dds_error.h"
#include "pool.h"

#define	STR_OK		DDS_RETCODE_OK
#define	STR_ERR_NOMEM	DDS_RETCODE_OUT_OF_RESOURCES
#define	STR_ERR_BAD	DDS_RETCODE_BAD_PARAMETER

#if defined (__WORDSIZE) && (__WORDSIZE == 64)
#define STRD_SIZE	8
#else
#define STRD_SIZE	4
#endif

typedef struct string_st {
#if defined (BIGDATA) || (STRD_SIZE == 8)
	size_t		length;		/* Octet String length. */
	unsigned	users:30;	/* # of users of string. */
#define	MAX_STR_REFS	0x3fffffffU
#else
	unsigned	length:16;	/* Octet String length. */
	unsigned	users:14;	/* # of users of string. */
#define	MAX_STR_REFS	0x3fffU
#endif
	unsigned	mutable:1;	/* String is changeable. */
	unsigned	dynamic:1;	/* Data is dynamically allocated. */
	union {
	  const char	*dp;		/* String data pointer. */
	  char		b [STRD_SIZE];	/* String itself if <= STRD_SIZE. */
	}		u;
} String_t;


extern size_t str_alloc_max, str_allocs;


int str_pool_init (const POOL_LIMITS *str,
		   const POOL_LIMITS *refs,
	           size_t            dsize,
		   int               extend);

/* Initialize the string library.  The str refs parameters are used for string
   pool dimensioning.
   Up to str.(reserver+extra) strings can be stored.  The string data area size
   is specified by the dsize parameter.  If the extend parameter is non-0,
   strings that no longer fit in the string pool due to it being full will be
   allocated dynamically.  */

void str_pool_free (void);

/* Release all string pool data. */

String_t *str_new (const char *str, size_t length, size_t copy, int mutable);

/* Get a new string containing the given string data and mutable attribute.
   Specifying a NULL pointer will not allocate a new string, but will simply
   return NULL. */

#define	str_new_cstr(s)	str_new(s, (unsigned) (strlen (s)) + 1, 0xFFFFFFFFU, 0)

/* Same as str_new() but a normal constant character string is the argument. */

int str_set_chunk (String_t *sp, const unsigned char *s, size_t ofs, size_t chunk_length);

/* Change part of a string to the given value. */

int str_replace (String_t *s, const char *str, size_t length);

/* Replace a string with the given data.  This is only allowed on mutable
   strings.  The function returns a non-zero error code if there is not enough
   memory to contain the string data. */

String_t *str_ref (String_t *s);

/* Duplicate a string so that another user can reference it also. */

void str_unref (String_t *s);

/* Unreference a string and release the string storage if this was the last
   reference to it. */

#define str_ptr(s)	(((s)->length > STRD_SIZE) ? (s)->u.dp : (s)->u.b)

/* Return the actual string pointer. */

#define	str_len(s)	(s)->length

/* Return the length of a string. */

#define	str_mutable(s)	(s)->mutable

/* Return the string attribute. */

String_t *str_copy (String_t *sp);

/* Copy a string completely. */

void str_dump (void);

/* Debug: dump the string cache. */

void str_pool_dump (size_t sizes []);

/* Debug: dump all string pool statistics. */

#endif	/* !__str_h_ */

