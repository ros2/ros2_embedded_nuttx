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

/* heap.c -- Implements a simple storage allocator for user-defined heaps. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vgdefs.h"
#include "error.h"
#include "pool.h"
#include "heap.h"

/*#define FORCE_MALLOC	** Define this to force use of malloc/free(). */

#ifndef MAX_HEAPS
#define	MAX_HEAPS	16	/* Max. # of heaps supported. */
#endif

#define	HEAP_END	~0UL	/* End of heap marker. */
#define	HEAP_ALIGN	sizeof (struct heap_st)	/* 8 or 16 !Do not change! */
#if defined (__WORDSIZE) && (__WORDSIZE == 64)
#define	HEAP_SHIFT	4
#else
#define	HEAP_SHIFT	3
#endif
#ifndef REDZ
#define	REDZ		0
#endif
#define	HEAP_ROUND(s)	(((s+REDZ+REDZ)+HEAP_ALIGN-1)&~(HEAP_ALIGN-1))

typedef struct heap_st HEAP_ST, *HEAP;
struct heap_st {
	HEAP		next;	/* Next block in heap. */
	size_t		size;	/* Size of heap. */
};
typedef struct heap_hdr_st HEAP_HDR_ST, *HEAP_HDR;
struct heap_hdr_st {
	void		*addr;
	size_t		size;
};

static HEAP_HDR_ST heaps [MAX_HEAPS];


/* heap_init -- Create a heap on a previously allocated memory block. */

int heap_init (void *addr, size_t size, unsigned *heap_id)
{
#ifdef FORCE_MALLOC
	ARG_NOT_USED (addr)
	ARG_NOT_USED (size)
	ARG_NOT_USED (heap_id)

	*heap_id = 0;
#else
	HEAP		hp, first, last;
	unsigned	i;

	if (!addr || size < 64)
		return (1);

	for (i = 0; i < MAX_HEAPS; i++)
		if (!heaps [i].size)
			break;

	if (i >= MAX_HEAPS)
		return (2);

	heaps [i].addr = addr;
	heaps [i].size = size;
	hp = (HEAP) addr;
	first = hp + 1;
	last = hp + (size >> HEAP_SHIFT) - 1;
	hp->next = first;
	hp->size = 0;
	first->next = last;
	last->next = hp;
	first->size = (last - first) << HEAP_SHIFT;
	last->size = HEAP_END;
	*heap_id = i + 1;
	VG_NOACCESS (addr, size);
	VG_POOL_CREATE (addr, REDZ, size);
#endif
	return (0);
}

/* heap_discard -- Make sure that the heap with the given heap id. can no longer
		   be used. */

void heap_discard (unsigned heap_id)
{
	if (!heap_id || heap_id > MAX_HEAPS || !heaps [heap_id - 1].size)
		return;

	heaps [heap_id - 1].size = 0;
	heaps [heap_id - 1].addr = NULL;
}

/* heap_alloc -- Allocate a memory region of the given size and with the given
                 attributes. */

void *heap_alloc (unsigned heap_id, size_t size)
{
#ifdef FORCE_MALLOC
	ARG_NOT_USED (heap_id)

	return (mm_fcts.alloc_ (size));
#else
	HEAP	prev, p, sp;
	size_t	nsize;

	if (!heap_id || heap_id > MAX_HEAPS || !heaps [heap_id - 1].size || !size)
		return (NULL);

	nsize = HEAP_ROUND (size) + sizeof (HEAP_ST);
	prev = heaps [heap_id - 1].addr;
	VG_DEFINED (prev, sizeof (HEAP_ST));
	VG_DEFINED (prev->next, sizeof (HEAP_ST));
	while (prev->next->size < nsize) {
		prev = prev->next;
		VG_NOACCESS (hp, sizeof (HEAP_ST));
		VG_DEFINED (prev->next, sizeof (HEAP_ST));
	}
	p = prev->next;
	if (p->size == HEAP_END) {
		VG_NOACCESS (prev, sizeof (HEAP_ST));
		VG_NOACCESS (p, sizeof (HEAP_ST));
		return (NULL);
	}
	VG_UNDEFINED (p + 1, p->size - sizeof (HEAP_ST));
	if (p->size == nsize)	/* Perfect fit. */
		prev->next = p->next;
	else {			/* Split block in two, top half returned. */
		sp = (HEAP) ((char *) p + nsize);
		prev->next = sp;
		sp->next = p->next;
		sp->size = p->size - nsize;
		VG_NOACCESS (sp, sp->size);
	}
	p->size = size;
	VG_NOACCESS (prev, sizeof (HEAP_ST));
	p->next = (HEAP) (uintptr_t) heap_id;
	VG_NOACCESS (p, sizeof (HEAP_ST));
	p = (HEAP) ((char *) p + sizeof (HEAP_ST) + REDZ);
	VG_POOL_ALLOC (heaps [heap_id - 1].addr, p, size);
	return ((void *) p);
#endif
}

#ifndef FORCE_MALLOC

static void heap_free_chunk (unsigned heap_id, HEAP tp)
{
	HEAP	prev, hp;

	prev = (HEAP) heaps [heap_id - 1].addr;
	VG_DEFINED (prev, sizeof (HEAP_ST));
	VG_DEFINED (prev->next, sizeof (HEAP_ST));
	while (prev->next < tp) {
		hp = prev;
		prev = prev->next;
		VG_NOACCESS (hp, sizeof (HEAP_ST));
		VG_DEFINED (prev->next, sizeof (HEAP_ST));
	}
	if ((char *) prev + prev->size == (char *) tp) { /* Combine! */
		prev->size += tp->size;
		tp = prev;
	}
	else if ((char *) prev + prev->size < (char *) tp) { /* Link after. */
		tp->next = prev->next;
		prev->next = tp;
		VG_NOACCESS (prev, sizeof (HEAP_ST));
	}
	else
		fatal_printf ("heap_free: heap (%u) corrupt!", heap_id);

	if (tp->next->size != HEAP_END &&
	    (char *) tp + tp->size == (char *) tp->next) { /* Combine! */
		hp = tp->next;
		VG_DEFINED (hp, sizeof (HEAP_ST));
		tp->size += hp->size;
		tp->next = hp->next;
		VG_NOACCESS (hp, sizeof (HEAP_ST));
	}
	VG_NOACCESS (tp, sizeof (HEAP_ST));
}
#endif

/* heap_free -- Free a previously allocated memory region. */

void heap_free (void *addr)
{
#ifdef FORCE_MALLOC
	mm_fcts.free_ (addr);
#else
	HEAP		tp;
	unsigned	heap_id;
	size_t		s;

	if (!addr)
		return;

	tp = (HEAP) ((char *) addr - sizeof (HEAP_ST) - REDZ);
	VG_DEFINED (tp, sizeof (HEAP_ST));
	s = tp->size;
	tp->size = HEAP_ROUND (s) + sizeof (HEAP_ST);
	if (!s ||
	    !tp->next ||
	    (heap_id = (uintptr_t) tp->next) > MAX_HEAPS ||
	    !heaps [heap_id - 1].size ||
	    (char *) tp < (char *) heaps [heap_id - 1].addr + sizeof (HEAP_ST) ||
	    ((char *) tp + tp->size) > (char *) heaps [heap_id - 1].addr +
	                                 heaps [heap_id - 1].size - sizeof (HEAP_ST)) {
		fatal_printf ("heap_free: invalid heap block (%p)!", addr);
		return;
	}
	heap_free_chunk (heap_id, tp);
	VG_POOL_FREE (heaps [heap_id - 1].addr, addr);
#endif
}

/* heap_realloc -- Change the size of a previously allocated memory region. */

void *heap_realloc (void *addr, size_t new_size)
{
#ifdef FORCE_MALLOC
	return (realloc (addr, new_size));
#else
	HEAP		tp, prev, sp;
	void		*naddr;
	unsigned	heap_id, ext_size;
	size_t		s, nsize;

	if (!new_size || !addr)
		return (NULL);

	tp = (HEAP) (char *) addr - REDZ - sizeof (HEAP_ST);
	s = tp->size;
	tp->size = HEAP_ROUND (s) + sizeof (HEAP_ST);
	if (!s ||
	    (heap_id = (uintptr_t) tp->next) > MAX_HEAPS ||
	    !heaps [heap_id - 1].size ||
	    (char *) addr < (char *) heaps [heap_id - 1].addr ||
	    ((char *) addr + tp->size) > (char *) heaps [heap_id - 1].addr +
					 heaps [heap_id - 1].size - sizeof (HEAP_ST)) {
		fatal_printf ("heap_free: invalid heap block (%p)!", addr);
		return (NULL);
	}
	nsize = HEAP_ROUND (new_size) + sizeof (HEAP_ST);
	if (tp->size == nsize) {
		tp->size = new_size;
		VG_POOL_FREE (heaps [heap_id - 1].addr, addr);
		VG_POOL_ALLOC (heaps [heap_id - 1].addr, addr, new_size);
		return (addr);
	}
	if (tp->size > nsize) {
		sp = (HEAP) ((unsigned char *) tp + nsize);
		sp->size = tp->size - nsize;
		heap_free_chunk (heap_id, sp);
		tp->size = new_size;
		VG_POOL_FREE (heaps [heap_id - 1].addr, addr);
		VG_POOL_ALLOC (heaps [heap_id - 1].addr, addr, new_size);
		return (addr);
	}
	ext_size = nsize - tp->size;
	prev = heaps [heap_id - 1].addr;
	while (prev->next < tp)
		prev = prev->next;
	if (prev->next->size != HEAP_END &&
	    (unsigned char *) prev->next == (unsigned char *) tp + tp->size &&
	    ext_size <= prev->next->size) {
		if (ext_size == prev->next->size)
			prev->next = prev->next->next;
		else {
			sp = (HEAP) ((unsigned char *) tp + nsize);
			sp->next = prev->next->next;
			sp->size = prev->next->size - ext_size;
			prev->next = sp;
		}
		tp->size = new_size;
		VG_POOL_FREE (heaps [heap_id - 1].addr, addr);
		VG_POOL_ALLOC (heaps [heap_id - 1].addr, addr, new_size);
		return (addr);
	}
	naddr = heap_alloc (heap_id, new_size);
	if (!naddr)
		return (NULL);

	memcpy (naddr, addr, tp->size - sizeof (HEAP_ST));
	heap_free_chunk (heap_id, tp);
	VG_POOL_FREE (heaps [heap_id - 1].addr, addr);
	return (naddr);
#endif
}

/* heap_avail -- Return the amount of bytes available in the heap. */

size_t heap_avail (unsigned heap_id)
{
#ifdef FORCE_MALLOC
	ARG_NOT_USED (heap_id)

	return (0x1000000);
#else
	HEAP		hp, nhp;
	unsigned long	size = 0UL;

	if (!heap_id || heap_id > MAX_HEAPS)
		return (0);

	hp = heaps [heap_id - 1].addr;
	VG_DEFINED (hp, sizeof (HEAP_ST));
	while (hp->size != HEAP_END) {
		size += hp->size;
		nhp = hp->next;
		VG_NOACCESS (hp, sizeof (HEAP_ST));
		hp = nhp;
		VG_DEFINED (hp, sizeof (HEAP_ST));
	}
	VG_NOACCESS (hp, sizeof (HEAP_ST));
	return (size - sizeof (HEAP_ST));
#endif
}

/* heap_max -- Return the size of the largest free block. */

size_t heap_max (unsigned heap_id)
{
#ifdef FORCE_MALLOC
	ARG_NOT_USED (heap_id)

	return (0x1000000);
#else
	HEAP		hp, nhp;
	unsigned long	size = 0UL;

	if (!heap_id || heap_id > MAX_HEAPS)
		return (0);

	hp = heaps [heap_id - 1].addr;
	VG_DEFINED (hp, sizeof (HEAP_ST));
	while (hp->size != HEAP_END) {
		if (hp->size > size)
			size = hp->size;
		nhp = hp->next;
		VG_NOACCESS (hp, sizeof (HEAP_ST));
		hp = nhp;
		VG_DEFINED (hp, sizeof (HEAP_ST));
	}
	VG_NOACCESS (hp, sizeof (HEAP_ST));
	return (size - sizeof (HEAP_ST));
#endif
}

#ifdef DDS_DEBUG

void heap_dump (unsigned heap_id)
{
	HEAP		hp;

	if (heap_id >= MAX_HEAPS)
		return;

	dbg_printf ("Heap free blocks: ");
	for (hp = heaps [heap_id - 1].addr; ; hp = hp->next) {
		dbg_printf ("{%p:", (void *) hp);
		if (hp->size == HEAP_END) {
			dbg_printf ("NIL}\n");
			break;
		}
		else
			dbg_printf ("%lu}", (unsigned long) hp->size);
	}
	dbg_printf ("\n");
}

#endif /* DDS_DEBUG */

