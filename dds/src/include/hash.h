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

#ifndef __hash_h_
#define	__hash_h_

#define hashf(dp, len)	hashfc(0, dp, len)

/* This function performs a hash on a data block of the given length.
   The return value is a large number which should be converted to a limited
   range, either by doing a modulo operation with a prime number (slow), or just
   anded to simply use the lower bits.  This hash function version is meant for
   both binary data blocks and ASCII strings. */

unsigned hashfc (unsigned h, const unsigned char *dp, size_t len);

/* Continue a hash with another data chunk. */

#endif /* !__hash_h_ */

