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

/* pool.c -- Exports various memory allocation services for applications. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vgdefs.h"
#include "sys.h"
#include "debug.h"
#include "ctrace.h"
#include "log.h"
#include "error.h"
#include "pool.h"

#define	ALIGNMENT	8

#ifdef CTRACE_USED

#define	POOL_FCTS_OFS	6
#define	MAX_POOL_FCTS	512

enum {
	POOL_XMALLOC,
	POOL_XREALLOC,
	POOL_XFREE,
	POOL_MDS_ALLOC,
	POOL_MDS_RESET,
	POOL_MDS_FREE

	/* MDS functions follow here. */
};

static const char *pool_fct_str [MAX_POOL_FCTS] = {
	"xmalloc",
	"xrealloc",
	"xfree",
	"mds_alloc",
	"mds_reset",
	"mds_free",

	/* MDS functions follow here. */
};

static unsigned ctrc_mds_offset = POOL_FCTS_OFS;

#endif

void pool_pre_init (void)
{
#ifdef CTRACE_USED
	log_fct_str [POOL_ID] = pool_fct_str;
	ctrc_mds_offset = POOL_FCTS_OFS;
#endif
}

void pool_post_final (void)
{
#ifdef CTRACE_USED
	unsigned	i;

	for (i = POOL_FCTS_OFS; i < ctrc_mds_offset; i++)
		if (pool_fct_str [i])
			free ((void *) pool_fct_str [i]);
		else
			break;

	ctrc_mds_offset = POOL_FCTS_OFS;
#endif
}


# if 0
#define POOL_VERIFY	/* Verify pool blocks when alloc/free. */
/*#define FORCE_MALLOC	** Force every allocation via normal heap manager. */
/*#define POOL_SET_MEM	** Initialize allocate memory. */

#ifdef POOL_VERIFY
#define	POOL_RED_ZONE	0x5265645aUL	/* 'RedZ': overwrite protection value.*/
#define POOL_REDZ_SIZE	(sizeof (uint32_t) * 2) /* 8-byte aligned */
#else
#define POOL_REDZ_SIZE	0
#endif

typedef struct pool_st POOL_ST, *POOL;
typedef struct pool_block POOL_BLOCK_ST, *POOL_BLOCK;

#define POOL_MAGIC	0x504d6167	/* Pool Magic number : 'PMag' */

#define	MAX_POOL_NAME	32

struct pool_st {
	char		pool_name [MAX_POOL_NAME]; /* Name of pool.    */
	uintptr_t	pool_magic;	/* Magic number.               */
	POOL_BLOCK	pool_start;	/* First block in pool.        */
	POOL_BLOCK	pool_end;	/* Last block in pool.         */
	POOL_BLOCK	pool_head;	/* First free block in pool.   */
	POOL_BLOCK	pool_tail;	/* Last free block in pool.    */
	POOL		pool_next;	/* Next pool in list of pools. */
	POOL		pool_prev;	/* Previous pool in pool list. */
	unsigned	pool_count;	/* Current #blocks in pool.    */
	unsigned	pool_length;	/* Maximum #blocks in pool.    */
	unsigned	pool_nomem;	/* Out of memory conditions.   */
	size_t		pool_bsize;	/* Block size of pool entries. */
};

struct pool_block {
	POOL_BLOCK	pb_next;
	POOL		pb_pool;
};

static POOL gbl_pool = NULL;
# endif

#ifndef STD_ALLOC

typedef struct memhdr_st {
	size_t		size;	/* Effective block size. */
#if !defined (__WORDSIZE) || (__WORDSIZE == 32)
	unsigned	pad;	/* -- not used -- */
#endif
} MEMHDR;

static uint32_t		nalloc_blocks;		/* # of successful mallocs. */
STATIC_ULLONG		(nalloc_bytes);		/* # of bytes allocated. */
static uint32_t		nalloc_failed;		/* # of failed mallocs.*/
static uint32_t		nrealloc_blocks;	/* # of successful reallocs. */
STATIC_LLONG		(nrealloc_delta);	/* Total block size delta. */
static uint32_t		nrealloc_failed;	/* # of failed reallocs. */
static uint32_t		nfree_blocks;		/* # of free() calls. */
STATIC_ULLONG		(nfree_bytes);		/* # of bytes freed. */
STATIC_ULLONG		(cur_alloced);		/* # of bytes in use. */
static lock_t		pstats_lock;		/* Statistics lock. */
static int		pstats_initialized;	/* Set if lock initialized. */

PoolAllocFcts mm_fcts = { malloc, realloc, free };

void pool_init_stats (void)
{
	if (!pstats_initialized) {
		lock_init_nr (pstats_lock, "pstats");
		pstats_initialized = 1;
	}
}

void pool_final_stats (void)
{
	if (pstats_initialized) {
		lock_destroy (pstats_lock);
		pstats_initialized = 0;
	}
}

void pool_get_malloc_count (unsigned *blocks, size_t *bytes)
{
	lock_take (pstats_lock);
	*blocks = nalloc_blocks;
#if defined (__WORDSIZE) && (__WORDSIZE == 64)
	*bytes = nalloc_bytes;
#else
	*bytes = nalloc_bytes_l;
#endif
	lock_release (pstats_lock);
}

/* xmalloc -- Try to allocate a memory block of the requested size.  If
	      successful, a pointer to the allocated block is returned. */

void *xmalloc (size_t size)
{
	MEMHDR		*p;
#ifdef POOL_SET_MEM
	unsigned	i;
	unsigned	*wp;
#endif

	p = malloc (size + sizeof (MEMHDR));
	ctrc_begind (POOL_ID, POOL_XMALLOC, &size, sizeof (size));
	ctrc_contd (&p, sizeof (p));
	ctrc_endd ();
	if (!p) {
		lock_take (pstats_lock);
		nalloc_failed++;
		lock_release (pstats_lock);
		return (NULL);
	}
	p->size = size;
	if (pstats_initialized)
		lock_take (pstats_lock);
	nalloc_blocks++;
	ADD_ULLONG (nalloc_bytes, size + sizeof (MEMHDR));
	ADD_ULLONG (cur_alloced, size + sizeof (MEMHDR));
	if (pstats_initialized)
		lock_release (pstats_lock);
	p++;
#ifdef POOL_SET_MEM
	for (i = 0, wp = (unsigned *) p; i < (size + 3) >> 2; i++, wp++)
		*wp = 0xdeadbeef;
#endif
	return (p);
}

/* xrealloc -- Change the size of the memory block pointed to by ptr to the
	       given size. */

void *xrealloc (void *ptr, size_t size)
{
	MEMHDR		*p = (MEMHDR *) ptr;
	size_t		prev_size;
#ifdef POOL_SET_MEM
	unsigned	i;
	unsigned	*wp;
#endif

	if (!p)
		return (xmalloc (size));

	--p;
	prev_size = p->size;
	if (size < prev_size)
		return (++p);

	p = realloc (p, size + sizeof (MEMHDR));

	ctrc_begind (POOL_ID, POOL_XREALLOC, &ptr, sizeof (ptr));
	ctrc_contd (&size, sizeof (size));
	ctrc_contd (&p, sizeof (p));
	ctrc_endd ();

	if (!p) {
		lock_take (pstats_lock);
		nrealloc_failed++;
		lock_release (pstats_lock);
		return (NULL);
	}
	p->size = size;
	if (pstats_initialized)
		lock_take (pstats_lock);
	nrealloc_blocks++;
	SUB_ULLONG (cur_alloced, prev_size + sizeof (MEMHDR));
	ADD_LLONG (nrealloc_delta, size + sizeof (MEMHDR));
	SUB_LLONG (nrealloc_delta, prev_size + sizeof (MEMHDR));
	ADD_ULLONG (cur_alloced, size + sizeof (MEMHDR));
	if (pstats_initialized)
		lock_release (pstats_lock);
	p++;
#ifdef POOL_SET_MEM
	prev_size = (prev_size + 3) & ~3; /* Round to next word boundary. */
	for (i = 0, wp = (unsigned *) ((char *) p + prev_size);
	     i < (size - prev_size + 3) >> 2;
	     i++, wp++)
		*wp = 0xdeadbeef;
#endif
	return (p);
}

/* xfree -- Free the memory block pointed to by ptr. */

void xfree (void *ptr)
{
	MEMHDR	*p = (MEMHDR *) ptr;

	--p;
	ctrc_printd (POOL_ID, POOL_XFREE, &p, sizeof (p));
	if (pstats_initialized)
		lock_take (pstats_lock);
	nfree_blocks++;
	ADD_ULLONG (nfree_bytes, p->size + sizeof (MEMHDR));
	SUB_ULLONG (cur_alloced, p->size + sizeof (MEMHDR));
	if (pstats_initialized)
		lock_release (pstats_lock);
	mm_fcts.free_ (p);
}

#endif

# if 0
/* pool_create_x -- Create a new pool. */

POOL pool_create_x (const char *name,
		    size_t     length,
		    size_t     bsize)
{
	POOL		pool = NULL;
	POOL_BLOCK	pbp, next_pbp, pbp_start;
	unsigned	i, blocks, totalblocks = 0;
	size_t		size, ssize, total, chunk;

	if (!bsize || length < 2)
		return (NULL);

	bsize = ((bsize + sizeof (POOL_BLOCK_ST) + ALIGNMENT - 1) &
					~(ALIGNMENT - 1)) + POOL_REDZ_SIZE;
	size = sizeof (POOL_ST) + length * bsize;
#ifdef POOL_VERIFY
	ssize = (length + 63) >> 6; /* Calculate block set size as a number of
					64-bit segments. */
	ssize <<= 3;
#else
	ssize = 0;
#endif
	total = size + ssize;
	chunk = total;
	blocks = (chunk - sizeof (POOL_ST) - ssize) / bsize;
	chunk = sizeof (POOL_ST) + ssize + blocks * bsize;
	while (totalblocks < length) {
		if (!pool) {
			pool = (POOL) malloc (chunk);
			if (!pool)
				return (NULL);

			pbp = (POOL_BLOCK) ((unsigned char *) (pool + 1) + ssize);
			pool->pool_end = NULL;
		}
		else {
			pbp = (POOL_BLOCK) malloc (chunk);
			if (!pbp)
				return (NULL);
		}
		pbp_start = pbp;
		for (i = 0; i < (blocks - 1); i++) {
			next_pbp = (POOL_BLOCK) ((char *) pbp + bsize);
			pbp->pb_next = next_pbp;
			pbp = next_pbp;
		}
		pbp->pb_next = NULL;
		if (pool->pool_end) {
			pool->pool_end->pb_next = pbp_start;
		}
		else {
			pool->pool_start = pool->pool_head = pbp_start;
		}
		pool->pool_tail = pool->pool_end = pbp;

		/* Counters. */
		totalblocks += blocks;
		total -= chunk;
		chunk = total;
		blocks = chunk / bsize;
		chunk = blocks * bsize;
	}
	if ((size = strlen (name)) < MAX_POOL_NAME)
		memcpy (pool->pool_name, name, size + 1);
	else {
		memcpy (pool->pool_name, name, MAX_POOL_NAME - 1);
		pool->pool_name [MAX_POOL_NAME - 1] = '\0';
		warn_printf ("pool_create_x: name ('%s') truncated!", name);
	}
	pool->pool_magic = POOL_MAGIC;
	pool->pool_count = pool->pool_length = totalblocks;
	pool->pool_bsize = bsize - sizeof (POOL_BLOCK_ST) - POOL_REDZ_SIZE;
#ifdef POOL_VERIFY
	bsize -= POOL_REDZ_SIZE;
	for (pbp = pool->pool_head; pbp; pbp = pbp->pb_next) {
		((unsigned short *) ((unsigned char *) pbp)) [3] = 0xdead;
		*((uint32_t *) ((unsigned char *) pbp + bsize)) = POOL_RED_ZONE;
	}
	memset ((unsigned char *) (pool + 1), 0xff, ssize);
#endif
	return (pool);
}

/* pool_init -- Initialize the pool handler.  A single global pool is created
		that can be referenced with a pool id of 0.  Other pools can be
		created by the user. */

int pool_init (unsigned max_pools, 	/* Max. number of pools. */
	       size_t   gbl_length,	/* Length of global pool. */
	       size_t   gbl_bsize)	/* Block size of global pool. */
{
	if (!max_pools)
		return (POOL_OK);

	gbl_pool = pool_create_x ("PGBL", gbl_length, gbl_bsize);
	if (!gbl_pool) {
		warn_printf ("pool_init: couldn't create global pool!");
		return (POOL_ERR_NOMEM);
	}
	gbl_pool->pool_next = gbl_pool->pool_prev = gbl_pool;

	return (POOL_OK);
}

/* pool_create -- Create a new pool with the specified name and the given number
 		  of blocks. */

int pool_create (const char *name,	/* Pool name. */
		 size_t     length,	/* Number of blocks in pool. */
		 size_t     bsize,	/* Block size. */
		 Pool_t     *poolid)	/* Pool id. */
{
	POOL	pool;

	pool = pool_create_x (name, length, bsize);
	if (!pool) {
		warn_printf ("pool_create: couldn't create pool ('%s', length=%u, bsize=%u)!", name, length, bsize);
		return (POOL_ERR_NOMEM);
	}

	pool->pool_next = gbl_pool->pool_next;
	gbl_pool->pool_next = pool;
	pool->pool_prev = gbl_pool;
	pool->pool_next->pool_prev = pool;
	*poolid = pool;
	return (POOL_OK);
}

/* pool_bsize -- Return the block size of a pool element. */

unsigned pool_bsize (Pool_t pool_id)
{
	POOL	pool;

	if (pool_id)
		pool = (POOL) pool_id;
	else
		pool = gbl_pool;
	return (pool->pool_bsize);
}

/* pool_ident -- Find the pool id of the given pool name. */

int pool_ident (const char *name, Pool_t *poolid)
{
	char		buf [MAX_POOL_NAME];
	POOL		pool;
	size_t		size;

	if ((size = strlen (name)) < MAX_POOL_NAME)
		memcpy (buf, name, size + 1);
	else {
		memcpy (buf, name, MAX_POOL_NAME - 1);
		buf [MAX_POOL_NAME - 1] = '\0';
		warn_printf ("pool_ident: name argument truncated!");
	}
	pool = gbl_pool;
	do {
		if (!strcmp (pool->pool_name, buf)) {
			*poolid = pool;
			return (POOL_OK);
		}
		pool = pool->pool_next;
	}
	while (pool != gbl_pool);
	return (POOL_ERR_NFOUND);
}

#define	POOL_ALIGNED(p)		((((uintptr_t) (p)) & 7) == 0)
#define	POOL_VALID_PTR(p)	((p) && POOL_ALIGNED(p))
#define	POOL_VALID_POOL(p)	(POOL_VALID_PTR(p) && (p)->pool_magic == POOL_MAGIC)
#define	POOL_IN_POOL(pool,p)	((p) >= (pool)->pool_start && (p) <= (pool)->pool_end)
#define POOL_VALID_POOL_HEAD(p)	(!(p)->pool_head || POOL_IN_POOL(p, (p)->pool_head))

/* pool_alloc -- Allocate a block from the specified pool. */

void *pool_alloc (Pool_t pool_id)
{
	POOL		pool;
	POOL_BLOCK	pbp;
#ifdef POOL_VERIFY
	uint32_t	mask;
	uint32_t	*pset;
	unsigned	index;
	unsigned	status = 0;
#endif
#ifdef POOL_SET_MEM
	unsigned	i;
	unsigned	*wp;
#endif

	if (pool_id)
		pool = (POOL) pool_id;
	else
		pool = gbl_pool;

#ifdef POOL_VERIFY
	if (!POOL_VALID_POOL (pool)) {
pool_error:
		fatal_printf ("pool_alloc: invalid pool (%p) detected (status=%u)!", pool_id, status);
	}
#endif
	if (!pool->pool_count)
		return (NULL);

 	pbp = pool->pool_head;
#ifdef POOL_VERIFY
	if (!POOL_VALID_PTR (pbp) || !POOL_IN_POOL(pool,pbp)) {
		status = 1;
		goto pool_error;
	}
#endif
	pool->pool_head = pbp->pb_next;
	pbp->pb_pool = pool;
	pool->pool_count--;
#ifdef POOL_VERIFY
	if (!POOL_ALIGNED (pool->pool_head)) {
		status = 2;
		goto pool_error;
	}
	if (!POOL_VALID_POOL_HEAD (pool)) {
		status = 3;
		goto pool_error;
	}
	index = ((uintptr_t) pbp - (uintptr_t) pool->pool_start) /
		(pool->pool_bsize + sizeof (POOL_BLOCK_ST) + POOL_REDZ_SIZE);
	pset = (uint32_t *) (pool + 1) + (index >> 5);
	mask = (1 << (index & 0x1f));
	*pset &= ~mask;
#endif
	pbp++;

#ifdef POOL_VERIFY
	((unsigned short *) pbp) [1] = 0;
	pset = (uint32_t *)((unsigned char *) pbp + pool->pool_bsize);
	if (*pset != POOL_RED_ZONE) {
		status = 4;
		goto pool_error;
	}
#endif
#ifdef POOL_SET_MEM
	for (i = 0, wp = (unsigned *) pbp; i < (pool->pool_bsize + 3) >> 2; i++, wp++)
		*wp = 0xdeadbeef;
#endif
	return ((void *) pbp);
}

/* pool_free -- Free a block to its pool. */

void pool_free (void *ptr)
{
	POOL_BLOCK	pbp;
	POOL		pool;
#ifdef POOL_VERIFY
	uint32_t	mask;
	uint32_t	*pset;
	unsigned	index, status = 0;
#endif

     /*	Most bugs related to buffer allocation/freeing have desastrous effects
	because : a) mem gets trashed if ptr is freed which was actually not
	             allocated from a pool (e.g. buf from ATM) The hidden pool
	             pointer preceeding pointer by 4 bytes is invalid.
	          b) pool linked list may become circular if block is freed
	             twice.

	On pSOSun+Purify, these problems are easily tracked down by substituting
	a real malloc & free for pool_alloc/pool_free.  This enables Purify's
	malloc tracing/accounting/boundary checking for pool blocks.

	On target, these problems are not easily detected.  The code below
	verifies and detects following errors:
		- Invalid pointer (NULL, unaligned, not in pool).
		- Duplicate release.
      */

	if (!POOL_VALID_PTR (ptr)) {
#ifdef POOL_VERIFY
    pool_error:
		err_printf ("pool_free: error detected (ptr=%p, status=%u)!", ptr, status);
#else
		return;
#endif
	}
#ifdef POOL_VERIFY
	pset = (uint32_t *) ptr;
#endif
	pbp = ((POOL_BLOCK) ptr) - 1;
 	pool = pbp->pb_pool;
#ifdef POOL_VERIFY
	if (!POOL_VALID_POOL (pool)) {
		status = 1;
		goto pool_error;
	}
	if (pool->pool_count >= pool->pool_length) {
		status = 2;
		goto pool_error;
	}
	if (!POOL_IN_POOL (pool, pbp)) {
		status = 3;
		goto pool_error;
	}
	if (*((uint32_t *)((unsigned char *) pset + pool->pool_bsize)) != POOL_RED_ZONE) {
		status = 4;
		goto pool_error;
	}
#endif
#ifdef POOL_VERIFY
	index = ((uintptr_t) pbp - (uintptr_t) pool->pool_start) /
		(pool->pool_bsize + sizeof (POOL_BLOCK_ST) + POOL_REDZ_SIZE);
	pset = (uint32_t *) (pool + 1) + (index >> 5);
	mask = (1 << (index & 0x1f));
#endif
#ifdef POOL_VERIFY
	if ((*pset & mask) != 0) {
		status = 5;
		goto pool_error;
	}
#endif

	/* Add block to end of block list. */
	if (pool->pool_head)
		pool->pool_tail->pb_next = pbp;
	else
		pool->pool_head = pbp;
	pool->pool_tail = pbp;
	pbp->pb_next = NULL;

#ifdef POOL_VERIFY
	*pset |= mask;
	((unsigned short *) ((unsigned char *) ptr)) [1] = 0xdead;
#endif
	pool->pool_count++;
}

/* pool_check -- Verify a pool for consistency/sanity. */

void pool_check (Pool_t pool_id)
{
#ifdef POOL_VERIFY
	POOL		pool = (POOL) pool_id;
	POOL_BLOCK	pbp;
	unsigned	status = 0;
	unsigned	nblocks;

	/* Sanity check */
	if (!POOL_VALID_POOL (pool)) {
pool_error:
		fatal_printf ("pool_check: invalid pool detected (pool=%p, status=%u)!", pool_id, status);
	}
	for (nblocks = 0, pbp = pool->pool_head; pbp; nblocks++, pbp = pbp->pb_next) {
		if (!POOL_VALID_PTR (pbp) || !POOL_IN_POOL (pool, pbp)) {
			status = 1;
			goto pool_error;
		}
		if (nblocks > pool->pool_count) {
			status = 2;
			goto pool_error;
		}
		/* TBD: Might check other things: RedZone/Dead/Bitmap. */
	}
	if (nblocks != pool->pool_count) {
		status = 3;
		goto pool_error;
	}
	/* TBD: Might also loop over EVERY pool block, and for blocks not yet
	   checked above, check their status: Pool_ptr/RedZone/Bitmap. */
#endif
}

unsigned pool_current_block_count (Pool_t pool_id)
{
	return (((POOL) pool_id)->pool_count);
}

unsigned pool_initial_block_count (Pool_t pool_id)
{
	return (((POOL) pool_id)->pool_length);
}

/* pool_walk -- Walk over every pool block of a pool and call the block function
		with a pointer to the pool block and the block size. */

void pool_walk (Pool_t pool_id,
		void (*bfct) (void *ptr, size_t size))
{
	POOL		pool;
	POOL_BLOCK	pbp;
	unsigned char	*xp;

	if (pool_id)
		pool = (POOL) pool_id;
	else
		pool = gbl_pool;

	if (!POOL_VALID_POOL (pool))
		return;

 	for (pbp = pool->pool_head; pbp; pbp = pbp->pb_next) {
		xp = (unsigned char *) (pbp + 1);
		(*bfct) (xp, pool->pool_bsize);
	}
}

# endif

/* mds_alloc -- Allocate a full memory descriptor set in a single operation.
		The mds parameter points to an array of MEM_DESC_ST structs,
		length specifies the number of descriptor structures and flags
		is a pointer to an array of memory allocation flags or NULL if
		default flags (0) are to be used. */

size_t mds_alloc (MEM_DESC mds, const char **names, size_t length)
{
	MEM_DESC	mdp;
	unsigned	i, j;
	unsigned char	*addr;
#ifndef FORCE_MALLOC
	unsigned char	*prev_cp, *cp;
#endif
	size_t		size;
#ifdef CTRACE_USED
	char		buf [64];
#endif

	ctrc_begind (POOL_ID, POOL_MDS_ALLOC, &mds, sizeof (mds));
	ctrc_contd (&names, sizeof (names));
	ctrc_contd (&length, sizeof (length));
	ctrc_endd ();

	/* Pass 1: calculate total memory requirements. */
	for (i = 0, size = 0, mdp = mds; i < length; i++, mdp++) {
		mdp->md_name = (names) ? names [i] : NULL;
		if ((mdp->md_esize & 0x7) != 0) {
			j = mdp->md_size / mdp->md_esize;
			mdp->md_esize = (mdp->md_esize + 7) & ~0x7;
			mdp->md_size = mdp->md_esize * j;
		}
#ifdef FORCE_MALLOC
		if (mdp->md_esize) {
			if (mdp->md_xmax != ~0U)
				mdp->md_xmax += mdp->md_size / mdp->md_esize;
			mdp->md_size = 0;
		}
		else
#endif
			size += mdp->md_size;
		lock_init_nr (mdp->md_lock, mdp->md_name);

#ifdef CTRACE_USED
		mdp->md_trace_id = ctrc_mds_offset;
		snprintf (buf, sizeof (buf), "%s_alloc", mdp->md_name);
		pool_fct_str [ctrc_mds_offset++] = strdup (buf);
		snprintf (buf, sizeof (buf), "%s_free", mdp->md_name);
		pool_fct_str [ctrc_mds_offset++] = strdup (buf);
#endif
	}

	/* Pass 2: allocate required memory regions. */
	if (size) {
		addr = (unsigned char *) malloc (size);
		if (!addr) {
			warn_printf ("mds_alloc (mds=%p, length=%lu) failed!",
					(void *) mds, (unsigned long) length);
			return (0);
		}
#if 0
		memset (addr, 0xff, size);	/* Just for fun - see what happens ... */
#endif

		/* Pass 3: distribute memory over memory descriptors. */
		for (i = 0, mdp = mds; i < length; i++, mdp++) {
			if (!mdp->md_size) {
				mdp->md_addr = mdp->md_pool = NULL;
				mdp->md_count = 0;
				continue;
			}
			mdp->md_addr = addr;
			addr += mdp->md_size;
#ifndef FORCE_MALLOC
			if (mdp->md_esize) {	/* Create pool of elements. */
				mdp->md_count = mdp->md_mcount = mdp->md_size / mdp->md_esize;
				mdp->md_pool = mdp->md_addr;
				for (j = 1, cp = (unsigned char *) mdp->md_pool;
				     j < mdp->md_count;
				     j++) {
					prev_cp = cp;
					cp += mdp->md_esize;
					*((void **) prev_cp) = cp;
				}
				*((void **)cp) = NULL;
				VG_NOACCESS (mdp->md_addr, mdp->md_size);
				VG_POOL_CREATE (mdp, 0, mdp->md_size);
			}
#endif
		}
	}
	VG_NOACCESS (mds, length * sizeof (MEM_DESC_ST));
	return (size);
}

/* mds_reset -- Reset a complete memory descriptor set. */

void mds_reset (MEM_DESC mds, size_t length)
{
	MEM_DESC	mdp;
	unsigned	i;
#ifndef FORCE_MALLOC
	unsigned	j;
	unsigned char	*prev_cp, *cp;
	void		*xp;
#endif

	ctrc_begind (POOL_ID, POOL_MDS_RESET, &mds, sizeof (mds));
	ctrc_contd (&length, sizeof (length));
	ctrc_endd ();

	VG_DEFINED (mds, length * sizeof (MEM_DESC_ST));
	for (i = 0, mdp = mds; i < length; i++, mdp++) {
		if (!mdp->md_size)
			continue;

#ifndef FORCE_MALLOC
		while ((xp = mdp->md_xpool) != NULL) {
			mdp->md_xpool = MDS_NEXT (xp);
			mm_fcts.free_ (xp);
		}
		if (mdp->md_esize) {	/* Create pool of elements. */
			mdp->md_count = mdp->md_mcount = mdp->md_size / mdp->md_esize;
			mdp->md_pool = mdp->md_addr;
			for (j = 1, cp = (unsigned char *) mdp->md_pool;
			     j < mdp->md_count;
			     j++) {
				prev_cp = cp;
				cp += mdp->md_esize;
				*((void **) prev_cp) = cp;
			}
			*((void **)cp) = NULL;
		}
#endif
	}
	VG_NOACCESS (mds, length * sizeof (MEM_DESC_ST));
}

/* mds_free -- Free a complete memory descriptor set. */

void mds_free (MEM_DESC mds, size_t length)
{
	MEM_DESC	mdp;
	void		*addr;
#ifndef FORCE_MALLOC
	void		*xp;
#endif
	unsigned	i;

	ctrc_begind (POOL_ID, POOL_MDS_FREE, &mds, sizeof (mds));
	ctrc_contd (&length, sizeof (length));
	ctrc_endd ();

	VG_DEFINED (mds, length * sizeof (MEM_DESC_ST));
	addr = NULL;

	/* Pass 1: find first block of memory region. */
	for (i = 0, mdp = mds; i < length; i++, mdp++) {
                lock_destroy (mdp->md_lock);
		if (!mdp->md_size)
			continue;

		if (!addr) {
			addr = (unsigned char *) mdp->md_addr;
#ifdef FORCE_MALLOC
			continue;
#else
		}

		/* Free all memory in growth area. */
		while ((xp = mdp->md_xpool) != NULL) {
			VG_DEFINED (xp, sizeof (void *));
			mdp->md_xpool = MDS_NEXT (xp);
			mm_fcts.free_ (xp);
#endif
		}
	}

	/* Pass 2: release memory region. */
	if (addr)
		mm_fcts.free_ (addr);

	/* Clear descriptors. */
	memset (mds, 0, sizeof (MEM_DESC_ST) * length);
	VG_NOACCESS (mds, length * sizeof (MEM_DESC_ST));
}

/* mds_block -- Get the monolithic block that was allocated in mds_alloc(). */

void *mds_block (MEM_DESC mp)
{
	void	*p;

	VG_DEFINED (mp, sizeof (MEM_DESC_ST));
	p = mp->md_addr;
	if (p) {
		VG_UNDEFINED (p, mp->md_size);
	}
	VG_NOACCESS (mp, sizeof (MEM_DESC_ST));
	return (p);
}

#if 0
#define	atrc_print1(s,a)	log_printf (POOL_ID, 0, s, a)
#else
#define	atrc_print1(s,a)
#endif

/* mds_pool_alloc -- Allocate an MDS pool element, either from the pre-created
		     pool, or by extra allocations when the pool is depleted,
		     up to the maximum allowed. */

void *mds_pool_alloc (MEM_DESC mp)
{
	void		*p;
#ifdef POOL_SET_MEM
	unsigned	i;
	unsigned	*wp;
#endif

	VG_DEFINED (mp, sizeof (MEM_DESC_ST));

	atrc_print1 ("alloc(%p:", mp);
	if (lock_take (mp->md_lock)) {
		warn_printf ("mds_pool_alloc: locking error (%s)", mp->md_name);
		return (NULL);
	}

#ifndef FORCE_MALLOC
	if ((p = mp->md_pool) != NULL) { /* Pool not empty? */

		/* Pool non-empty: return pool element. */
		VG_DEFINED (p, sizeof (void *));
		mp->md_pool = MDS_NEXT (p);
		mp->md_count--;
		if (mp->md_count < mp->md_mcount)
			mp->md_mcount = mp->md_count;

		VG_POOL_ALLOC (mp, p, mp->md_esize);
		atrc_print1 ("P):%p;", p);
		goto done;
	}
	if ((p = mp->md_xpool) != NULL) { /* Extra pool not empty? */
		VG_DEFINED (p, sizeof (void *));
		mp->md_xpool = MDS_NEXT (p);
		mp->md_gcount--;
		VG_UNDEFINED (p, mp->md_esize);
		atrc_print1 ("G):%p;", p);
		goto done;
	}
#else
	p = NULL;
#endif
	/* Check if extra allocations are permitted: */
	if (mp->md_xcount < mp->md_xmax) {
		mp->md_xalloc++;
		mp->md_xcount++;
		if (mp->md_xcount > mp->md_mxcount)
			mp->md_mxcount = mp->md_xcount;
		p = malloc (mp->md_esize);
		atrc_print1 ("M):%p;", p);
		goto done;
	}
	mp->md_nomem++;

    done:
	ctrc_printd (POOL_ID, mp->md_trace_id, &p, sizeof (p));

    	lock_release (mp->md_lock);
	VG_NOACCESS (mp, sizeof (MEM_DESC_ST));

#ifdef POOL_SET_MEM
	for (i = 0, wp = (unsigned *) p; i < (mp->md_esize + 3) >> 2; i++, wp++)
		*wp = 0xdeadbeef;
#endif
	return (p);
}

/* mds_pool_free -- Free an MDS pool element that was formerly allocated via
		    mds_pool_alloc(). */

void mds_pool_free (MEM_DESC mp, void *p)
{
	ctrc_printd (POOL_ID, mp->md_trace_id + 1, &p, sizeof (p));

	VG_DEFINED (mp, sizeof (MEM_DESC_ST));
	if (lock_take (mp->md_lock)) {
		warn_printf ("mds_pool_free: locking error (%s)", mp->md_name);
		return;
	}

#ifndef FORCE_MALLOC

	/* Check if pool block. */
	atrc_print1 ("free(%p);", p);
	if ((uintptr_t) p >= (uintptr_t) mp->md_addr &&
	    (uintptr_t) p < (uintptr_t) mp->md_addr + mp->md_size) {

		/* Points to a real pool block -- add back to pool list. */
#ifdef POOL_VERIFY
		if (mp->md_count == mp->md_size / mp->md_esize)
			fatal_printf ("mds_pool_free: pool already full!");
#endif
		MDS_NEXT (p) = mp->md_pool;
		mp->md_pool = p;
		mp->md_count++;
		VG_POOL_FREE (mp, p);
		goto done;
	}

	/* If in growable range, store in grow pool. */
	if (mp->md_gcount < mp->md_xgrow) {
		MDS_NEXT (p) = mp->md_xpool;
		mp->md_xpool = p;
		mp->md_gcount++;
		VG_NOACCESS (p, mp->md_esize);
		goto done;
	}
#endif

#ifdef POOL_VERIFY
	if (!mp->md_xcount)
		fatal_printf ("mds_pool_free: no extra elements?");
#endif

	/* Element was extra allocated? */
	mp->md_xcount--;
	mm_fcts.free_ (p);

#ifndef FORCE_MALLOC
    done:
#endif
	lock_release (mp->md_lock);
	VG_NOACCESS (mp, sizeof (MEM_DESC_ST));
}

/* mds_pool_contains -- Verify whether an MDS pool contains the given block. */

int mds_pool_contains (MEM_DESC mp, void *ptr)
{
#ifdef FORCE_MALLOC
	ARG_NOT_USED (mp)
	ARG_NOT_USED (ptr)

	return (0);
#else
	void		*p;
	unsigned	n = 0;

	VG_DEFINED (mp, sizeof (MEM_DESC_ST));
	if (lock_take (mp->md_lock)) {
		warn_printf ("mds_pool_contains: locking error (%s)", mp->md_name);
		return (0);
	}
	if ((uintptr_t) ptr >= (uintptr_t) mp->md_addr &&
	    (uintptr_t) ptr < (uintptr_t) mp->md_addr + mp->md_size)
		for (p = mp->md_pool, n = 0; p && n < 0xffff; p = MDS_NEXT (p), n++)
			if (p == ptr) {
				lock_release (mp->md_lock);
				VG_NOACCESS (mp, sizeof (MEM_DESC_ST));
				return (1);
			}
	if (n >= 0xffff)
		fatal_printf ("mds_pool_contains: memory pool is corrupted!");

	lock_release (mp->md_lock);
	VG_NOACCESS (mp, sizeof (MEM_DESC_ST));
	return (0);
#endif
}

#ifdef DDS_DEBUG

static int		pool_log = 0;
static POOL_DISPLAY	pool_format = /*PDT_NORMAL*/ PDT_LONG;
static char		buf [132];

/* print -- Output a string, either as a response to a debug command, or as
	    logging output. */

static void print (const char *s)
{
	if (pool_log)
		log_printf (LOG_DEF_ID, 0, "%s", s);
	else
		dbg_printf ("%s", s);
}

/* print_pool_format -- Specify the pool display format. */

void print_pool_format (POOL_DISPLAY type)
{
	pool_format = type;
}

/* print_pool_hdr -- Start display of a memory pool. */

void print_pool_hdr (int log)
{
	unsigned	i;
	static const char	*str_normal1 [] = {
		"  Name        ",
		"  ----        "
	};
	static const char	*str_ext1 [] = {
		"       Addr",
		"       ----"
	};
	static const char	*str_normal2 [] = {
		"  PSize  BSize  Rsrvd    Max MPUse CPUse MXUse CXUse Alloc  NoMem",
		"  -----  -----  -----    --- ----- ----- ----- ----- -----  -----"
	};
	static const char	*str_ext2 [] = {
		"  Block",
		"  -----"
	};
	static const char	*str_final [] = {
		"\r\n",
		"\r\n"
	};

	pool_log = log;
	if (pool_format == PDT_SUMMARY)
		return;

	for (i = 0; i < 2; i++) {
		print (str_normal1 [i]);
		if (pool_format == PDT_LONG)
			print (str_ext1 [i]);
		print (str_normal2 [i]);
		if (pool_format == PDT_LONG)
			print (str_ext2 [i]);
		print (str_final [i]);
	}
}

/* print_pool_table -- Display a pool table.  Set *tsize = *msize = *usize = 0
		       for the first pool display. */

void print_pool_table (const MEM_DESC_ST *pools,	/* Pool descriptors. */
		       unsigned          n,		/* Number of elements in pd. */
		       size_t            sizes [PPT_SIZES]) /* Sizes. */
{
	MEM_DESC_ST	*pdp = (MEM_DESC_ST *) pools;
	unsigned	pt, t;

	for (pt = 0; pt < n; pt++, pdp++) {
		VG_DEFINED (pdp, sizeof (MEM_DESC_ST));
		sizes [PPT_TOTAL] += pdp->md_size;
		if (!pdp->md_esize) {
			sizes [PPT_MUSE] += pdp->md_size;
			sizes [PPT_CUSE] += pdp->md_size;
			continue;
		}
		if (lock_take (pdp->md_lock)) {
			warn_printf ("print_pool_table: locking error (%s)", pdp->md_name);
			continue;
		}
		t = pdp->md_size / pdp->md_esize;
		if (pool_format != PDT_SUMMARY) {
			snprintf (buf, sizeof (buf), "  %-12s", pdp->md_name);
			print (buf);
			if (pool_format == PDT_LONG) {
				snprintf (buf, sizeof (buf), " %10p", pdp->md_addr);
				print (buf);
			}
			snprintf (buf, sizeof (buf), "%7lu%7lu%7u", 
						(unsigned long) pdp->md_size,
						(unsigned long) pdp->md_esize,
						t);
			print (buf);
			if (pdp->md_xmax == ~0U)
				snprintf (buf, sizeof (buf), "      *");
			else
				snprintf (buf, sizeof (buf), "%7u", t + pdp->md_xmax);
			print (buf);
			snprintf (buf, sizeof (buf), "%6u%6u",
							     t - pdp->md_mcount,
							     t - pdp->md_count);
			print (buf);
			snprintf (buf, sizeof (buf), "%6d%6d%6d%7d",
								pdp->md_mxcount,
								pdp->md_xcount,
								pdp->md_xalloc,
								pdp->md_nomem);
			print (buf);
			if (pool_format == PDT_LONG) {
				snprintf (buf, sizeof (buf), "  %8p",
								  pdp->md_pool);
				print (buf);
			}
			snprintf (buf, sizeof (buf), "\r\n");
			print (buf);
		}
		sizes [PPT_MUSE] += pdp->md_esize * (t - pdp->md_mcount);
		sizes [PPT_CUSE] += pdp->md_esize * (t - pdp->md_count);
		sizes [PPT_MXUSE] += pdp->md_esize * pdp->md_mxcount;
		sizes [PPT_XUSE] += pdp->md_esize * pdp->md_xcount;
		sizes [PPT_XCNT] += pdp->md_xalloc;
		lock_release (pdp->md_lock);
		VG_NOACCESS (pdp, sizeof (MEM_DESC_ST));
	}
}

/* print_alloc_stats -- Display allocation statistics. */

void print_alloc_stats (void)
{
	STATIC_ULLONG (heap_alloc);

	lock_take (pstats_lock);
	snprintf (buf, sizeof (buf), "Dynamically allocated: ");
	print (buf);
	DBG_PRINT_ULLONG (cur_alloced);
	ADD2_ULLONG (heap_alloc, cur_alloced);
	snprintf (buf, sizeof (buf), " bytes.\r\nmalloc statistics: %u blocks, ", nalloc_blocks);
	print (buf);
	DBG_PRINT_ULLONG (nalloc_bytes);
	snprintf (buf, sizeof (buf), " bytes, %u failures.\r\nrealloc statistics: %u blocks, ", nalloc_failed, nrealloc_blocks);
	print (buf);
	DBG_PRINT_ULLONG (nrealloc_delta);
	snprintf (buf, sizeof (buf), " bytes, %u failures.\r\nfree statistics: %u blocks, ", nrealloc_failed, nfree_blocks);
	print (buf);
	DBG_PRINT_ULLONG (nfree_bytes);
	snprintf (buf, sizeof (buf), " bytes.\r\n");
	print (buf);
	db_xpool_stats ();
	lock_release (pstats_lock);
}

/* print_pool_end -- End pool display, printing totals. */

void print_pool_end (size_t sizes [PPT_SIZES])
{
	STATIC_ULLONG (heap_alloc);

	snprintf (buf, sizeof (buf), "\r\nPool/max/xmax/used/xused memory = %lu/%lu/%lu/%lu/%lu bytes (%lu mallocs)",
					(unsigned long) sizes [PPT_TOTAL],
					(unsigned long) sizes [PPT_MUSE],
					(unsigned long) sizes [PPT_MXUSE],
					(unsigned long) sizes [PPT_CUSE],
					(unsigned long) sizes [PPT_XUSE],
					(unsigned long) sizes [PPT_XCNT]);
	print (buf);
	if (sizes [PPT_TOTAL]) {
		snprintf (buf, sizeof (buf), " (%lu%%/%lu%%)",
			(unsigned long) ((sizes [PPT_MUSE] + sizes [PPT_MXUSE]) * 100) / sizes [PPT_TOTAL],
			(unsigned long) ((sizes [PPT_CUSE] + sizes [PPT_XUSE]) * 100) / sizes [PPT_TOTAL]);
		print (buf);
	}
	snprintf (buf, sizeof (buf), "\r\n");
	print (buf);
	CLR_ULLONG (heap_alloc);
	ADD_ULLONG (heap_alloc, sizes [PPT_TOTAL] + sizes [PPT_MXUSE]);
#ifndef STD_ALLOC
	print_alloc_stats ();
#endif
#ifdef STR_ALLOC
	snprintf (buf, sizeof (buf), "Maximum/current extended string heap space: %lu/%lu bytes.\r\n",
				(unsigned long) str_alloc_max, (unsigned long) str_allocs);
	print (buf);
#endif
	snprintf (buf, sizeof (buf), "Total heap memory: ");
	print (buf);
	DBG_PRINT_ULLONG (heap_alloc);
	snprintf (buf, sizeof (buf), " bytes.\r\n");
	print (buf);
	pool_log = 0;
}

#endif

