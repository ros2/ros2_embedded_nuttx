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

/* skiplist.c -- Implements various Skiplist operations.

   Skiplists are a probabilistic alternative to balanced trees, as
   described in the June 1990 issue of CACM and were invented by 
   William Pugh in 1987. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef USE_MALLOC
#include "error.h"
#include "pool.h"
#endif
#include "thread.h"
#include "random.h"
#include "skiplist.h"

static unsigned	randoms_left;
static unsigned	random_value;
#ifndef USE_MALLOC
static size_t pool_dsize, total_pool_size;
static MEM_DESC_ST pool [(MAX_LEVELS >> 1) + 1];
static MEM_DESC_ST list_pool [1];
#endif

#define	DATA_PTR(np,ofs)	((unsigned char *) np - ofs)
#define	NODE_SIZE(l)		(sizeof (SLNode_t *) * ((l) + 1))

#ifdef USE_MALLOC
#define	xmalloc	malloc
#define	xfree free
#else

/* list_pool_init -- Preallocate a fast pool of skiplist nodes. */

int sl_pool_init (const POOL_LIMITS *pools, 
		  const POOL_LIMITS *nodes,
		  size_t            data_size)
{
	unsigned	i;
	size_t		min, max, s, m, grow, s2;
	POOL_LIMITS	limits;
	static const char *lname [] = { "SLIST" };
	static const char *names [(MAX_LEVELS >> 1) + 1];
	static char	buf [(MAX_LEVELS >> 1) + 1][4];

	MDS_POOL_TYPE (list_pool, 0, *pools, sizeof (Skiplist_t));
	s2 = mds_alloc (list_pool, lname, 1);
#ifndef FORCE_MALLOC
	if (!s2) {
		warn_printf ("sl_pool_init: not enough memory available!\r\n");
		return (1);
	}
#endif
	pool_dsize = data_size;
	if (nodes->extra != ~0U)
		max = nodes->extra >> 1;
	else
		max = ~0U;
	if (nodes->grow != ~0U)
		grow = nodes->grow >> 1;
	else
		grow = ~0U;
	for (i = 0, min = nodes->reserved; i <= MAX_LEVELS; i += 2, min >>= 2) {

		/* Calculate block size: Base node + user data + # of ptrs. */
		s = NODE_SIZE (i + 1) + data_size;

		/* Add extra field *before* user data to remember level for
		   pool 'grow' purposes. */
		s += sizeof (unsigned long);

		/* Get minimum # of blocks. */
		m = (min >> 1) + (min >> 2);
		if (!m)
			m = 1;

		/* Setup pool. */
		limits.reserved = m;
		limits.extra = max;
		limits.grow = grow;
		MDS_POOL_TYPE (pool, i >> 1, limits, s);
		if (max != ~0U)
			max >>= 2;
		buf [i >> 1][0] = 'S';
		buf [i >> 1][1] = 'L';
		buf [i >> 1][2] = (i >> 1) + '0';
		buf [i >> 1][3] = '\0';
		names [i >> 1] = buf [i >> 1];
	}
	total_pool_size = mds_alloc (pool, names, (MAX_LEVELS >> 1) + 1);
	if (total_pool_size)
		total_pool_size += s2;
#ifndef FORCE_MALLOC
	log_printf (0, 0, "sl_pool_init: allocated %lu bytes of memory for pools.\r\n", 
		       (unsigned long) total_pool_size);
	return (!total_pool_size);
#else
	return (0);
#endif
}

/* sl_pool_free -- Free the preallocated pool. */

void sl_pool_free (void)
{
	if (!pool_dsize)
		return;

	mds_free (list_pool, 1);
	mds_free (pool, (MAX_LEVELS >> 1) + 1);
}

#ifdef DDS_DEBUG

/* sl_pool_dump -- Dump the memory usage of the skiplist pool. */

void sl_pool_dump (size_t sizes [PPT_SIZES])
{
	unsigned	i;

	if (!pool_dsize)
		return;

	print_pool_table (list_pool, 1, sizes);
	for (i = 0; i <= (MAX_LEVELS >> 1); i++)
		print_pool_table (&pool [i], 1, sizes);
}

#endif
#endif

static SLNode_t *new_node (unsigned level, size_t data_size)
{
#ifndef USE_MALLOC
	unsigned long	*up;
#endif
	unsigned char	*dp;
	SLNode_t	*np;
	size_t		s;

#ifndef USE_MALLOC
	if (data_size == pool_dsize) {
		up = mds_pool_alloc (&pool [level >> 1]);
		if (!up)
			return (NULL);

		*up = level;
		dp = (unsigned char *) (++up);
	}
	else {
#endif
		s = NODE_SIZE (level) + data_size;
		dp = (unsigned char *) xmalloc (s);
#ifndef USE_MALLOC
	}
#endif
	if (!dp)
		return (NULL);

	np = (SLNode_t *) (dp + data_size);
	return (np);
}

static void free_node (SLNode_t *node, size_t data_size)
{
	unsigned char	*dp;
#ifndef USE_MALLOC
	unsigned long	*up;
#endif

	dp = DATA_PTR (node, data_size);
#ifndef USE_MALLOC
	if (data_size == pool_dsize) {
		up = (unsigned long *) (dp - sizeof (unsigned long));
		mds_pool_free (&pool [*up >> 1], up);
	}
	else
#endif
		xfree (dp);
}

/* sl_init -- Same as sl_new(), but doesn't allocate the list header. */

void sl_init (Skiplist_t *lp, size_t data_size)
{
	memset (lp, 0, sizeof (Skiplist_t));
	lp->data_size = data_size;
}

/* sl_new -- Allocate a new list, with list nodes containing the given data size. */

Skiplist_t *sl_new (size_t data_size)
{
	Skiplist_t	*lp;

#ifndef USE_MALLOC
	if (data_size == pool_dsize)
		lp = (Skiplist_t *) mds_pool_alloc (list_pool);
	else
#endif
		lp = xmalloc (sizeof (Skiplist_t));
	if (!lp)
		return (NULL);

	sl_init (lp, data_size);
	return (lp);
} 

/* sl_free_nodes -- Free all nodes of a list, i.e. same as sl_free(), but
		    without release of the list header. */

void sl_free_nodes (Skiplist_t *lp)
{
	SLNode_t	*p, *q;

	for (p = lp->header [0]; p; p = q) {
		q = p->next [0];
		free_node (p, lp->data_size);
	}
}

/* sl_free -- Free a complete list, i.e. all nodes + the list header.
	      Should only be called for lists created with list_new (). */

void sl_free (Skiplist_t *lp)
{
	sl_free_nodes (lp);
#ifndef USE_MALLOC
	if (pool_dsize)
		mds_pool_free (list_pool, lp);
	else
#endif
		xfree (lp);
}

/* sl_search -- Search an element in the list. */

void *sl_search (const Skiplist_t *l, const void *data, LISTEQF eqf)
{
	int		k, res = 1;
	SLNode_t	*p, *q;

	if (!l->length)
		return (NULL);

	p = (SLNode_t *) &l->header;
	k = l->level;
	do {
		while (q = p->next [k],
		       q && (res = eqf (DATA_PTR (q, l->data_size), data)) < 0)
			p = q;
	}
	while (--k >= 0);
	if (!q || res)
		return (NULL);

	return (DATA_PTR (q, l->data_size));
}

#ifdef _WIN32
#define INLINE
#else
#define INLINE inline
#endif

#define	MAX_RAND_BITS	15	/* # of significant bits in random number. */

/* random_level -- Get a random level, based on the result of a random number
		   generator.  As an optimisation, we use as level the number
		   of consecutive 0-bits that are encountered. */

static unsigned random_level (void)
{
	unsigned	level = 0;
	unsigned	b;
#ifndef CDR_ONLY
	static lock_t	random_lock;
	static int	init_lock = 1;

	if (init_lock) {
		init_lock = 0;
		lock_init_nr (random_lock, "random");
	}
	lock_take (random_lock);
#endif
	do {
		if (!randoms_left) {
			random_value = fastrand ();
			randoms_left = MAX_RAND_BITS;
		}
		b = random_value & 1;
		if (!b)
			level++;
		random_value >>= 1;
		randoms_left--;
	}
	while (!b);
#ifndef CDR_ONLY
	lock_release (random_lock);
#endif
	return ((level >= MAX_LEVELS) ? MAX_LEVELS - 1 : level);
}

/* sl_insert -- Try to insert an element in a list.  If it already exists,
		the node is returned and *new_node will be set.  If not, a
		new node is allocated and the pointer to it is returned.  */

void *sl_insert (Skiplist_t *l, const void *data, int *allocated, LISTEQF fct)
{
	int		k, res = -1;
	SLNode_t	*update [MAX_LEVELS];
	SLNode_t	*p, *q;

	if (l->length) {
		p = (SLNode_t *) &l->header;
		k = l->level;
		do {
			while (q = p->next [k],
			       q && (res = fct (DATA_PTR (q, l->data_size), data)) < 0)
				p = q;
			update [k] = p;
		}
		while (--k >= 0);

		if (q && !res) {
			if (allocated)
				*allocated = 0;
			return (DATA_PTR (q, l->data_size));
		}
		k = random_level ();
		if (k > (int) l->level) {
			k = ++l->level;
			update [k] = (SLNode_t *) &l->header;
		}
	}
	else {
		k = l->level = 0;
		update [0] = (SLNode_t *) &l->header;
	}
	q = new_node (k, l->data_size);
	if (!q)
		return (NULL);

	do {
		p = update [k];
		q->next [k] = p->next [k];
		p->next [k] = q;
	}
	while (--k >= 0);
	if (allocated)
		*allocated = 1;
	l->length++;
	return (DATA_PTR (q, l->data_size));
}

/* sl_delete -- Try to delete an element from a list. If it exists, the node
		is released and a non-0 result is returned.  If it didn't
		exist, 0 is returned. */

int sl_delete (Skiplist_t *l, const void *data, LISTEQF fct) 
{
	int		k, m, res = 1;
	SLNode_t	*update [MAX_LEVELS];
	SLNode_t	*p, *q;

	if (!l->length)
		return (0);

	p = (SLNode_t *) &l->header;
	k = m = l->level;
	do {
		while (q = p->next [k],
		       q && (res = fct (DATA_PTR (q, l->data_size), data)) < 0)
			p = q;
		update [k] = p;
	}
	while (--k >= 0);

	if (q && !res) {
		for (k = 0; k <= m && (p = update [k])->next [k] == q; k++)
			p->next [k] = q->next [k];
		free_node (q, l->data_size);
		while (l->header [m] == NULL && m > 0)
			m--;
		l->level = m;
		l->length--;
		return (1);
	}
	else
		return (0);
}

#if 0
/* sl_delete_node -- Delete a node from a list. */

void sl_delete_node (Skiplist_t *l, void *node)
{
	int		k, m;
	SLNode_t	*update [MAX_LEVELS];
	SLNode_t	*p, *q;

	p = (SLNode_t *) &l->header;
	k = m = l->level;
	do {
		while (q = p->next [k],
		       q && (unsigned char *) q - l->data_size != (unsigned char *) node)
			p = q;
		update [k] = p;
	}
	while (--k >= 0);

	for (k = 0;
	     k <= (int) l->level && (p = update [k])->next [k] == q;
	     k++)
		p->next [k] = q->next [k];
	free_node (q, l->data_size);
	while (l->header [m] == NULL && m > 0)
		m--;
	l->level = m;
	l->length--;
}
#endif

/* list_walk -- Walk over a list, visiting every node, and call the visitf
		function for each node encountered.  If the function returns 0,
		walking will stop. */

void sl_walk (Skiplist_t *lp, VISITF fct, void *arg)
{
	SLNode_t	*np, *next_np;

	if (!lp->length)
		return;

	for (np = lp->header [0]; np; np = next_np) {
		next_np = np->next [0];
		if (!(*fct) (lp, DATA_PTR (np, lp->data_size), arg))
			break;
	}
}

/* Returns the first element in the list or NULL if empty. */

void *sl_head (Skiplist_t *lp)
{
	return (lp->length ? DATA_PTR (lp->header [0], lp->data_size) : NULL);
}
