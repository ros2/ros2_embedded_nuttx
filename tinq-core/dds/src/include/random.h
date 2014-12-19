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

/* random.h -- Fast pseudo random number generator, based on a LCG algorithm. */

#ifndef __fastrand_h_
#define __fastrand_h_

#define MAX_FRAND_BITS	15	/* Max. # of significant bits. */

void fastsrand (unsigned seed);

/* Seed the pseudo random number generator. */

unsigned fastrand (void);

/* Return a pseudo random number with 15 significant bits. */

unsigned fastrandn (unsigned n);

/* Return a pseudo random number in the range (0..n-1) where n < 2^16. */

#endif /* !__fastrand_h_ */

