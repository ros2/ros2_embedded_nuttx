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

/* random.c -- Fast pseudo random number generator, based on a LCG algorithm. */

#include "random.h"

static unsigned	g_seed;

/* fastsrand -- Seed the random number generator. */

void fastsrand (unsigned seed)
{
	g_seed = seed;
}

/* fastrand -- Return a random number with 15 significant bits. */

unsigned fastrand (void)
{
	g_seed = (214013 * g_seed + 2531011); 

	return ((g_seed >> 16) & 0x7fff);
}

/* fastrandn -- Return a random number in the range (0..n-1) where n < 2^16. */

unsigned fastrandn (unsigned n)
{
	return ((fastrand () * n) >> 15);
}
