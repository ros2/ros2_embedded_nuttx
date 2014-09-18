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

/* skiplist.h -- Defines common operations on Skiplists. */

#ifndef __skiplist_h_
#define __skiplist_h_

#include "pool.h"

#define MAX_LEVELS	15	/* Max. # of pointer levels. */

/* Note: the Node_t struct is shown here to prevent compiler errors.  Users
         should *not* attempt to reference the nodes directly! */

typedef struct sl_node_st SLNode_t;
struct sl_node_st {
	/* <<user data is stored *before* the pointers>> */
	SLNode_t	*next [MAX_LEVELS + 1];
};

typedef struct skiplist_st {
	unsigned	level;
	unsigned	length;
	size_t		data_size;
	SLNode_t	*header [MAX_LEVELS + 1];
} Skiplist_t;


int sl_pool_init (const POOL_LIMITS *pools, 
		  const POOL_LIMITS *nodes,
		  size_t            data_size);

/* Preallocate a fast pool of skiplist nodes for the given data size. */

void sl_pool_free (void);

/* Free the preallocated pool. */

void sl_pool_dump (size_t sizes []);

/* Debug: dump the memory usage of the skiplist pool. */

#define	sl_initialized(l)	((l).header != NULL)

/* Returns a non-zero result if the list is initialized. */

Skiplist_t *sl_new (size_t data_size);

/* Create a new empty list containing list nodes of the given data size. */

void sl_free (Skiplist_t *list);

/* Free a complete list, i.e. all nodes + the list header.
   Should only be called for lists created with sl_new (). */

void sl_init (Skiplist_t *list, size_t data_size);

/* Same as sl_new(), but doesn't allocate the list header which must exist
   already. */

#define	SL_INIT_DATA(data_size)	{ 0, data_size, 0, { 0, }}

/* Can be used instead of sl_init as a static initializer of a list. */

void sl_free_nodes (Skiplist_t *list);

/* Free all nodes of a list, i.e. same as sl_free(), but without release of
   the list header. */

typedef int (*LISTEQF) (const void *np, const void *data);

/* Comparison function for node key lookups. */

void *sl_search (const Skiplist_t *list, const void *data, LISTEQF eqf);

/* Search an element in the list.  The data argument points to the key to use
   to find the element.  The eqf argument is a function to compare the keys. */

void *sl_insert (Skiplist_t *list, const void *data, int *allocated, LISTEQF eqf);

/* Try to insert an element in a list.  If it already exists, the node is
   returned and *new_node will be cleared.  If not, a new node is allocated and
   a pointer to it is returned. */

int sl_delete (Skiplist_t *list, const void *data, LISTEQF eqf);

/* Try to delete an element from a list. If it exists, the node is released and
   a non-0 result is returned.  If it didn't exist, 0 is returned. */

/* ==> void sl_delete_node (Skiplist_t *list, void *node);
   ==> not really possible with skiplists ... */

/* Delete a node from a list. */

typedef int (*VISITF) (Skiplist_t *list, void *node, void *arg);

/* Callback function to visit a specific list node. Should return 0 to stop
   visiting, or 1 to continue with the next nodes. */

void sl_walk (Skiplist_t *list, VISITF fct, void *arg);

/* Returns the first element in the list or NULL if empty. */

void *sl_head (Skiplist_t *list);

/* Walk over a list, visiting every node, and call the fct function for each
   node encountered.  If the function returns 0, walking will stop. */

#define	sl_length(l)	(l)->length

/* Return the number of nodes in the given list. */

#define sl_foreach(l,n)	for ((n) = (l)->header [0]; (n); (n) = (n)->next [0])

/* Walk over a skiplist following every node in sequence. */

#define	sl_data_ptr(l,n) (void *)((unsigned char *) (n) - (l)->data_size)

/* Convert a node pointer to a user data pointer. */

#endif /* !__skiplist_h_ */

