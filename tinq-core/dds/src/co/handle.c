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

/* handle.c -- Manages the handles functionality. */

#include <stdio.h>
#include <string.h>
#include "pool.h"
#include "error.h"
#include "thread.h"
#include "handle.h"

/*#define HANDLE_LOG	** Define this for handle logging. */
#ifdef HANDLE_LOG
#define	h_printf(s)		printf(s)
#define	h_printf1(s,a)		printf(s,a)
#define	h_printf2(s,a1,a2)	printf(s,a1,a2)
#else
#define	h_printf(s)
#define	h_printf1(s,a)
#define	h_printf2(s,a1,a2)
#endif

#ifndef HBITMAP_USED

typedef struct {
	unsigned short	next;
	unsigned short	prev;
} Index_t;

typedef struct {
	unsigned	nelements;
	Index_t		elem [1];
} HandleTable_t;


#define	HANDLE_TABLE_SIZE(n)	(sizeof (HandleTable_t) + \
				 sizeof (Index_t) * ((n)))

/* handle_init -- Initialize a handle table for the given number of handles. */

void *handle_init (unsigned nhandles)
{
	HandleTable_t	*ht;

	h_printf1 ("handle_init (%u): ", nhandles);
	if (nhandles < 2 || nhandles >= 0xffff) {
		h_printf ("0\r\n");
		return (NULL);
	}
	ht = xmalloc (HANDLE_TABLE_SIZE (nhandles));
	if (!ht) {
		h_printf ("0\r\n");
		return (NULL);
	}
	ht->nelements = nhandles;
	h_printf1 ("%p\r\n", (void *) ht);
	handle_reset (ht);
	return (ht);
}

/* handle_extend -- Extend the handle table for the new number of handles. */

void *handle_extend (void *table, unsigned nhandles)
{
	HandleTable_t	*nht, *ht = (HandleTable_t *) table;
	unsigned	i, n;

	h_printf1 ("handle_extend (t, %u): ", nhandles);
	n = ht->nelements + nhandles;
	if (n < 2 || n >= 0xffff) {
		h_printf ("0\r\n");
		return (NULL);
	}
	nht = xrealloc (ht, HANDLE_TABLE_SIZE (n));
        if (!nht) {
		h_printf ("0\r\n");
                return (NULL);
	}
	ht = nht;
	for (i = ht->nelements + 2; i <= n; i++) {
		ht->elem [i - 1].next = i;
		ht->elem [i].prev = i - 1;
	}
	if (ht->elem [0].prev) {
		ht->elem [ht->elem [0].prev].next = ht->nelements + 1;
		ht->elem [ht->nelements + 1].prev = ht->elem [0].prev;
	}
	else {
		ht->elem [0].next = ht->nelements + 1;
		ht->elem [ht->nelements + 1].prev = 0;
	}
	ht->elem [0].prev = n;
	ht->elem [n].next = 0;
	ht->nelements = n;
	h_printf1 ("%p\r\n", (void *) ht);
	return (ht);
}

/* handle_reset -- Reset a handle table so that it is full again. */

void handle_reset (void *table)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	unsigned	i;

	h_printf ("handle_reset (t);\r\n");
	for (i = 1; i <= ht->nelements; i++) {
		ht->elem [i - 1].next = i;
		ht->elem [i].prev = i - 1;
	}
	ht->elem [ht->nelements].next = 0;
	ht->elem [0].prev = ht->nelements;
}

/* handle_final -- Free a handle table containing the given number of handles.*/

void handle_final (void *table)
{
	HandleTable_t	*ht = (HandleTable_t *) table;

	h_printf ("handle_final (t);\r\n");
	ht->nelements = 0xfffff;
	xfree (ht);
}

/* handle_alloc -- Allocate a handle from the handle table. */

handle_t handle_alloc (void *table)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	Index_t		*np, *pp, *p;
	unsigned	index;

	h_printf ("handle_alloc (t): ");
	if ((index = ht->elem [0].next) == 0) {
		h_printf ("0\r\n");
		return (0);
	}
	p = &ht->elem [index];
	np = &ht->elem [p->next];
	pp = &ht->elem [p->prev];
	pp->next = p->next;
	np->prev = p->prev;
	p->next = p->prev = 0;
	h_printf1 ("%u\r\n", index);
	return (index);
}

/* handle_free -- Free a previously allocated handle. */

void handle_free (void *table, handle_t handle)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	Index_t		*p;

	h_printf1 ("handle_free (t, %u)\r\n", handle);
	if (!handle ||
	    handle > ht->nelements ||
	    ht->elem [handle].next ||
	    ht->elem [handle].prev) {
		fatal_printf ("handle_free: invalid handle (%u)!", handle);
		return;
	}
	p = &ht->elem [handle];
	p->prev = ht->elem [0].prev;
	ht->elem [0].prev = handle;
	ht->elem [p->prev].next = handle;
	p->next = 0;
}

#else

typedef unsigned long	Word_t;

#if defined (WORDSIZE) && (WORDSIZE == 64)
#define	WORD_SHIFT	6		/* 64-bit words. */
#define	WORD_INDEX_M	0x3f
#else
#define	WORD_SHIFT	5		/* 32-bit words. */
#define	WORD_INDEX_M	0x1f
#endif

#define BITSPWORD	WORDSIZE	/* # of bits/word. */
#define	WORDSPBLOCK	128		/* # of words/block: . */
#define	BITSPBLOCK	(WORDSPBLOCK * WORDSIZE)
#define	BLOCK_SHIFT	(WORD_SHIFT + 7)

#define	BPBLOCK	
typedef struct handle_block_st {
	unsigned	offset;		/* Handle block index. */
	unsigned	free_bits;	/* # of available handles in block. */
	unsigned	first_free;	/* Index of first free word. */
	Word_t		bitmap [WORDSPBLOCK];	/* Handle bitmap. */
} HandleBlock_t;

typedef struct handle_table_st {
	unsigned	nblocks;	/* Total # of blocks in table. */
	unsigned	free_blocks;	/* # of free blocks in table. */
	unsigned	first_free;	/* Index of first free handles block. */
	unsigned long	free_handles;	/* Cur. # of free handles in table. */
	unsigned long	max_handles;	/* Max. # of free handles in table. */
	HandleBlock_t	**blocks;	/* Handle blocks. */
} HandleTable_t;

#ifdef __GNUC__
#define clz(x)	__builtin_clzl(x)
#define ctz(x)	__builtin_ctzl(x)
#elif defined (_WIN32)

static unsigned __inline clz (unsigned long x)
{
	unsigned long	r;

#if defined (WORDSIZE) && (WORDSIZE == 64)
	_BitScanForward64 (&r, x);
#else
	_BitScanForward32 (&r, x);
#endif
	return (r);
}

static unsigned __inline ctz (unsigned long x)
{
	unsigned	r;

#if defined (WORDSIZE) && (WORDSIZE == 64)
	_BitScanReverse64 (&r, x);
#else
	_BitScanReverse32 (&r, x);
#endif
	return (r);
}

#else

static inline unsigned clz (unsigned long x)
{
	unsigned	i;
	unsigned long	m;

	if (x == 0)
		return (WORDSIZE);

	for (i = 0, m = (1 << (WORDSIZE - 1)); i < WORDSIZE; i++, m >>= 1)
		if (x & m) != 0)
			break;

	return (i);
}

static inline unsigned ctz (unsigned long x)
{
	unsigned	i;
	unsigned long	m;

	if (x == 0)
		return (WORDSIZE);

	for (i = 0, m = 1; i < WORDSIZE; i++, m <<= 1)
		if ((x & m) != 0)
			break;

	return (i);
}

#endif

/* handle_init -- Initialize a handle table for the given number of handles. */

void *handle_init (unsigned nhandles)
{
	HandleTable_t	*ht;
	HandleBlock_t	*bp;
	unsigned	nb, i;

	if (!nhandles)
		return (NULL);

	h_printf1 ("handle_init (%u): ", nhandles);
	ht = xmalloc (sizeof (HandleTable_t));
	if (!ht) {
		h_printf ("0\r\n");
		return (NULL);
	}
	nb = (nhandles + BITSPBLOCK - 1) >> BLOCK_SHIFT;
	ht->blocks = xmalloc (nb * sizeof (HandleBlock_t *));
	if (!ht->blocks) {
		xfree (ht);
		h_printf ("0\r\n");
		return (NULL);
	}
	for (i = 0; i < nb; i++) {
		ht->blocks [i] = bp = xmalloc (sizeof (HandleBlock_t));
		if (!bp) {
			while (i)
				xfree (ht->blocks [--i]);
			xfree (ht->blocks);
			xfree (ht);
			h_printf ("0\r\n");
			return (NULL);
		}
		bp->offset = i;
		bp->free_bits = BITSPBLOCK;
		bp->first_free = 0;
		memset (bp->bitmap, ~0U, sizeof (bp->bitmap));
	}
	ht->nblocks = ht->free_blocks = nb;
	ht->first_free = 0;
	ht->free_handles = ht->max_handles = nhandles;
	h_printf1 ("%p\r\n", (void *) ht);
	return (ht);
}

/* handle_extend -- Extend the handle table for the new number of handles. */

void *handle_extend (void *table, unsigned nhandles)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	HandleBlock_t	**p, *bp;
	unsigned	nb, i;

	h_printf1 ("handle_extend (t, %u): ", nhandles);
	if (!ht) {
		h_printf ("0\r\n");
		return (NULL);
	}
	nb = (ht->max_handles + nhandles + BITSPBLOCK - 1) >> BLOCK_SHIFT;
	if (nb > ht->nblocks) {
		p = xrealloc (ht->blocks, nb & sizeof (HandleBlock_t *));
		if (!p) {
			h_printf ("0\r\n");
			return (NULL);
		}
		ht->blocks = p;
		for (i = ht->nblocks; i < nb; i++) {
			ht->blocks [i] = bp = xmalloc (sizeof (HandleBlock_t));
			if (!bp) {
				while (i > ht->nblocks)
					xfree (ht->blocks [--i]);

		 		h_printf ("0\r\n");
				return (NULL);
			}
			bp->offset = i;
			bp->free_bits = BITSPBLOCK;
			bp->first_free = 0;
			memset (bp->bitmap, ~0U, sizeof (bp->bitmap));
		}
		ht->free_blocks += nb;
		ht->nblocks = nb;
	}
	ht->free_handles += nhandles;
	ht->max_handles += nhandles;
	h_printf1 ("%p\r\n", (void *) ht);
	return (ht);
}

/* handle_reset -- Reset a handle table so that it is full again. */

void handle_reset (void *table)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	HandleBlock_t	*bp;
	unsigned	i;

	h_printf ("handle_reset (t);\r\n");
	if (!ht)
		return;

	for (i = 0; i < ht->nblocks; i++) {
		bp = ht->blocks [i];
		bp->free_bits = BITSPBLOCK;
		bp->first_free = 0;
		memset (bp->bitmap, ~0U, sizeof (bp->bitmap));
	}
	ht->free_blocks = ht->nblocks;
	ht->first_free = 0;
	ht->free_handles = ht->max_handles;
}

/* handle_final -- Free a handle table containing the given number of handles.*/

void handle_final (void *table)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	unsigned	i;

	h_printf ("handle_final (t);\r\n");
	if (!ht)
		return;

	for (i = 0; i < ht->nblocks; i++)
		xfree (ht->blocks [i]);
	xfree (ht->blocks);
	xfree (ht);
}

/* handle_alloc -- Allocate a handle from the handle table. */

handle_t handle_alloc (void *table)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	HandleBlock_t	*bp;
	unsigned long	*wp;
	unsigned	h;

	h_printf ("handle_alloc (t): ");
	if (!ht || !ht->free_handles) {
		h_printf ("0\r\n");
		return (0);
	}
	bp = ht->blocks [ht->first_free];
	wp = &bp->bitmap [bp->first_free];
	h = clz (*wp);
	h |= bp->first_free << WORD_SHIFT;
	h |= bp->offset << BLOCK_SHIFT;
	*wp &= ~(1UL << (WORDSIZE - 1UL - h));
	if (--bp->free_bits) {
		while (*wp == 0UL) {
			bp->first_free++;
			wp++;
		}
	}
	else
		while (!ht->blocks [++ht->first_free]->free_bits)
			;
	ht->free_handles--;
	h_printf1 ("%u\r\n", h + 1);
	return (h + 1);
}

/* handle_free -- Free a previously allocated handle. */

void handle_free (void *table, handle_t handle)
{
	HandleTable_t	*ht = (HandleTable_t *) table;
	HandleBlock_t	*bp;
	Word_t		*wp;
	unsigned	b, i;
	unsigned long	m;

	h_printf1 ("handle_free (t, %u)\r\n", handle);
	if (!ht ||
	    ht->free_handles == ht->max_handles ||
	    !handle ||
	    handle > ht->max_handles)
		return;

	handle--;
	b = handle >> BLOCK_SHIFT;
	bp = ht->blocks [b];
	i = handle >> WORD_SHIFT;
	wp = &bp->bitmap [i];
	m = 1 << (WORDSIZE - 1 - (handle & WORD_INDEX_M));
	if ((*wp & m) != 0)
		return;

	*wp |= m;
	bp->free_bits++;
	if (i < bp->first_free)
		bp->first_free = i;
	if (b < ht->first_free)
		ht->first_free = b;
	ht->free_handles++;
}

#endif

