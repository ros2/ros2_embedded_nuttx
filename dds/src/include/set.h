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

/* set.h -- Some set manipulation macros. */

#ifndef __set_h_
#define	__set_h_

#define	SET_EMPTY(bm)	memset (bm, 0, 8 * sizeof (uint32_t))

/* Empty a set. */

#define	SET_ADD(bm,i)	((bm) [(i) >> 5] |= (1U << (31 - ((i) & 0x1f))))

/* Add an element to a set. */

#define	SET_REM(bm,i)	((bm) [(i) >> 5] &= ~((1U << (31 - ((i) & 0x1f)))))

/* Remove an element from a set. */

#define	SET_CONTAINS(bm,i) (((bm) [(i) >> 5] & (1U << (31 - ((i) & 0x1f)))) != 0)

/* Verify if an element is in a set. */

#endif /* !__set_h_ */

