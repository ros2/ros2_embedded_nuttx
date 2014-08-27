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

/* list.c -- Implements common access functions for single linked list manipulations. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pool.h"
#include "error.h"
#include "list.h"

#ifdef STANDALONE
#define	warn_printf	printf
#endif

/* list_init -- Same as list_new(), but doesn't allocate the list header. */

void list_init (List_t *lp, size_t data_size)
{
	lp->head = lp->tail = NULL;
	lp->data_size = data_size;
	lp->length = 0;
}

/* list_new -- Allocate a new list, with list nodes containing the given data size. */

List_t *list_new (size_t data_size)
{
	List_t	*lp;

	lp = (List_t *) xmalloc (sizeof (List_t));
	if (!lp)
		warn_printf ("list_new(%u): failed to allocate list header!", data_size);

	list_init (lp, data_size);
	return (lp);
}

/* list_free_nodes -- Free all nodes of a list, i.e. same as list_free(), but
		      without release of the list header. */

void list_free_nodes (List_t *lp)
{
	Node_t	*np, *next_np;

	for (np = lp->head; np; np = next_np) {
		next_np = np->next;
		xfree (np);
	}
	lp->head = lp->tail = NULL;
	lp->length = 0;
}

/* list_free -- Free a complete list, i.e. all nodes + the list header.
		Should only be called for lists created with list_new (). */

void list_free (List_t *lp)
{
	list_free_nodes (lp);
	xfree (lp);
}

/* list_search -- Search an element in the list. */

void *list_search (List_t *lp, void *data, LISTEQF fct)
{
	Node_t		*np;
	int		res;

	for (np = lp->head; np; np = np->next) {
		res = (*fct) (np + 1, data);
		if (res >= 0)
			return (res ? NULL : np + 1);
	}
	return (NULL);
}

/* list_insert -- Try to insert an element in a list.  If it already exists,
		  the node is returned and *new_node will be set.  If not, a
		  new node is allocated and the pointer to it is returned.  */

void *list_insert (List_t *lp, void *data, int *allocated, LISTEQF fct)
{
	Node_t		*np, *prev, *new_np;
	int		res;

	for (np = lp->head, prev = NULL; np; prev = np, np = np->next) {
		res = (*fct) (np + 1, data);
		if (res > 0)
			break;

		else if (!res) {
			*allocated = 0;
			return (np + 1);
		}
	}
	new_np = xmalloc (lp->data_size + sizeof (Node_t));
	if (!new_np) {
		warn_printf ("list_insert: can't allocate a new node!");
		return (NULL);
	}
	if (!np)
		lp->tail = new_np;
	new_np->next = np;
	if (prev)
		prev->next = new_np;
	else
		lp->head = new_np;
	lp->length++;
	*allocated = 1;
	return (new_np + 1);
}

/* list_delete -- Try to delete an element from a list. If it exists, the node
		  is released and a non-0 result is returned.  If it didn't
		  exist, 0 is returned. */

int list_delete (List_t *lp, void *data, LISTEQF fct)
{
	Node_t		*np, *prev, *new_np;
	int		res;

	for (np = lp->head, prev = NULL; np; prev = np, np = np->next) {
		res = (*fct) (np + 1, data);
		if (!res)
			break;
	}
	if (!np)
		return (0);

	if (prev)
		prev->next = np->next;
	else
		lp->head = np->next;
	xfree (np);
	lp->length--;
	return (1);
}

/* list_delete_node --Delete a node from a list. */

void list_delete_node (List_t *lp, void *node)
{
	Node_t		*np, *prev, *new_np;
	int		res;

	for (np = lp->head, prev = NULL;
	     np && (np + 1) != node;
	     prev = np, np = np->next)
		;
	if (!np)
		return;

	if (prev)
		prev->next = np->next;
	else
		lp->head = np->next;
	xfree (np);
	lp->length--;
}

/* list_walk -- Walk over a list, visiting every node, and call the visitf
		function for each node encountered.  If the function returns 0,
		walking will stop. */

void list_walk (List_t *lp, VISITF fct, void *arg)
{
	Node_t		*np, *next_np;
	int		res;

	for (np = lp->head; np; np = next_np) {
		next_np = np->next;
		if (!(*fct) (np + 1, arg))
			break;
	}
}

#ifdef STANDALONE

/*#define VERBOSE*/
#ifdef VERBOSE
#define	SAMPLE_SIZE 20
#else
#define SAMPLE_SIZE 10000
#endif

typedef struct data_node {
	unsigned	key;
	unsigned	*data;
} DATA_NODE;

int cmp_fct (void *np, void *data)
{
	return (((DATA_NODE *) np)->key - *((unsigned *) data));
}

#ifdef VERBOSE
int disp_fct (void *np)
{
	printf ("%u ", ((DATA_NODE *) np)->key);
	return (1);
}
#endif

int main (int argc, char **argv)
{
	List_t		*l;
	DATA_NODE	*v;
	unsigned	i, k;
	int		new_node;
	unsigned	keys [SAMPLE_SIZE];

	l = list_new (sizeof (DATA_NODE));

	for (k = 0; k < SAMPLE_SIZE; k++) {
		do {
			keys [k] = random () % (SAMPLE_SIZE * 10);
			v = (DATA_NODE *) list_insert (l, &keys [k], &new_node, cmp_fct);
		}
		while (!new_node);
		v->key = keys [k];
		v->data = &keys [k];
#ifdef VERBOSE
		printf ("+%u ", keys [k]);
		fflush (stdout);
#endif
        };
#ifdef VERBOSE
	printf ("\n");
	list_walk (l, disp_fct);
	printf ("\n");
#endif
	for (i = 0; i < 4; i++) {
		for (k = 0; k < SAMPLE_SIZE; k++) {
			if ((v = (DATA_NODE *) list_search (l, &keys [k], cmp_fct)) == NULL)
				printf ("error in search #%u,#%u\n", i, k);

			if (v->data != &keys [k])
				printf ("search returned wrong value\n");
		};
		for (k = 0; k < SAMPLE_SIZE; k++) {
#ifdef VERBOSE
			printf ("-%u ", keys [k]);
			fflush (stdout);
#endif
			if (!list_delete (l, &keys [k], cmp_fct))
				printf ("error in delete\n");

			do {
				keys [k] = random () % (SAMPLE_SIZE * 10);
				v = (DATA_NODE*) list_insert (l, &keys [k], &new_node, cmp_fct);
			}
			while (!new_node);
			v->key = keys [k];
			v->data = &keys [k];
#ifdef VERBOSE
			printf ("+%u ", keys [k]);
			fflush (stdout);
#endif
		};
#ifdef VERBOSE
		printf ("\n");
		list_walk (l, disp_fct);
		printf ("\n");
#endif
        };

	list_free (l);

	return (0);
}

#endif

