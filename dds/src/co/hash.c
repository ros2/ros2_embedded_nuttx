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

/* hash.h -- Hash functions used in DDS. */

#include <stdio.h>
#include "hash.h"

/* hashfc -- Performs a hash on a data block of the given length. This function
             is based on P.J. Weinberger's hash function, except that the modulo
	     function is still required afterwards and the input data can be any
	     data block, ASCII or binary. */

unsigned hashfc (unsigned h, const unsigned char *dp, size_t len)
{
	unsigned	g;

	for (; len; dp++, len--) {
		h = (h << 4) + *dp;
		if ((g = (h & 0xf0000000UL)) != 0) {
			h ^= g >> 24;
			h ^= ~g;
		}
	}
	return (h);
}

