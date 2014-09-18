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

/* vgdefs.h -- Defines a set of Valgrind definitions that makes it easier to
	       track incorrect memory accesses. */

#ifndef __vgdefs_h_
#define	__vgdefs_h_

#ifdef VALGRIND_USED

#include "valgrind/valgrind.h"
#include "valgrind/memcheck.h"

#define	VG_NOACCESS	VALGRIND_MAKE_MEM_NOACCESS
#define	VG_DEFINED	VALGRIND_MAKE_MEM_DEFINED
#define	VG_UNDEFINED	VALGRIND_MAKE_MEM_UNDEFINED

#define	VG_POOL_CREATE	VALGRIND_CREATE_MEMPOOL
#define	VG_POOL_ALLOC	VALGRIND_MEMPOOL_ALLOC
#define	VG_POOL_FREE	VALGRIND_MEMPOOL_FREE

#else

#define VG_NOACCESS(addr,size)
#define VG_DEFINED(addr,size)
#define VG_UNDEFINED(addr,size)

#define	VG_POOL_CREATE(pool,redz,size)
#define	VG_POOL_ALLOC(pool,addr,size)
#define	VG_POOL_FREE(pool,addr)

#endif

#endif /* !__vgdefs_h_ */

