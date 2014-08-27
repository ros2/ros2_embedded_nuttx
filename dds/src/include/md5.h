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

/* md5.h -- Implements the MD5 hash as specified in RFC1321. */

#ifndef __md5_h_
#define	__md5_h_

#include <stdint.h>

typedef struct md5_context_st {
	uint32_t	state [4];	/* State (ABCD). */
	uint32_t	count [2];	/* # of bits, modulo 2^64 (lsb first). */
	unsigned char	buffer [64];
} MD5_CONTEXT;

void md5_init (MD5_CONTEXT *context);

/* Initialize an MD5 context. */

void md5_update (MD5_CONTEXT         *context,
		 const unsigned char *buffer,
		 size_t              length);

/* Update the MD5 checksum with another data block.
   Should be called at least once. */

void md5_final (unsigned char result [16], MD5_CONTEXT *context);

/* Get the resulting MD5 checksum in the result argument. */

#endif /* !__md5_h_ */

