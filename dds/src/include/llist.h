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

/* llist.h -- Implements common access functions for list manipulations. */

#ifndef __llist_h_
#define __llist_h_

/* Note: the Node_t struct is shown here to prevent compiler errors.  Users
         should *not* attempt to reference the nodes directly! */

typedef struct node_st Node_t;
struct node_st {
	Node_t		*next;
	/* <<user data is stored *after* the pointer>> */
};

typedef struct list_st List_t;
struct list_st {
	Node_t		*head;
	Node_t		*tail;
	size_t		data_size;
	unsigned	length;
};

#define	list_initialized(l)	((l).data_size != 0)

/* Returns a non-zero result if the list is initialized. */

List_t *list_new (size_t node_size);

/* Allocate a new list, with list nodes of the given data size. */

void list_free (List_t *list);

/* Free a complete list, i.e. all nodes + the list header.
   Should only be called for lists created with list_new (). */

void list_init (List_t *list, size_t node_size);

/* Same as list_new(), but doesn't allocate the list header which must exist
   already. */

void list_free_nodes (List_t *list);

/* Free all nodes of a list, i.e. same as list_free(), but without release of
   the list header. */

typedef int (*LISTEQF) (void *np, void *data);

/* Comparison function for node key lookups. */

void *list_search (List_t *list, void *data, LISTEQF eqf);

/* Search an element in the list.  The data argument points to the key to use
   to find the element.  The eqf argument is a function to compare the keys. */

void *list_insert (List_t *list, void *data, int *allocated, LISTEQF eqf);

/* Try to insert an element in a list.  If it already exists, the node is
   returned and *new_node will be set.  If not, a new node is allocated and the
   pointer to it is returned.  */

int list_delete (List_t *list, void *data, LISTEQF eqf);

/* Try to delete an element from a list. If it exists, the node if released and
   a non-0 result is returned.  If it didn't exist, 0 is returned. */

void list_delete_node (List_t *list, void *node);

/* Delete a node from a list. */

typedef int (*VISITF) (void *node, void *arg);

/* Callback function to visit a specific list node. */

void list_walk (List_t *list, VISITF fct, void *arg);

/* Walk over a list, visiting every node, and call the visitf function for each
   node encountered.  If the function returns 0, walking will stop. */

#define	list_length(l)	(l)->length

/* Return the number of nodes in the given list. */

#endif /* !__llist_h_ */

