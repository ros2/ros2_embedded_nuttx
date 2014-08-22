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

/* dlist.h -- Defines some common list manipulation macros.
              NOTE: this version uses a NULL-terminated head/tail elements
		    compared to the newer version that uses the list struct as
		    a sentinel node and keeps the list circular. */

#ifndef __dlist_h_
#define __dlist_h_

/* To use these macros, it is enough that each list node contains two pointers,
   i.e. 'next' and 'prev' that point respectively to the next and the previous
   list node, and that a list header structure extists that contains fields
   'head' and 'tail' that point respectively to the first and the last list
   nodes. */
   
#define	LIST_INIT(l)		(l).head = (l).tail = NULL

/* Initialise a list. */

#define LIST_HEAD(l)		(l).head

/* Return a pointer to the first node in the list. */

#define	LIST_NEXT(n)		(n).next

/* Return the successor node of a node. */

#define	LIST_PREV(n)		(n).next

/* Return the predecessor node of a node. */

#define	LIST_ADD_HEAD(l,n)	(n).next = (l).head; (n).prev = NULL;	   \
	if ((l).head) (l).head->prev = &(n); else (l).tail = &(n);         \
	(l).head = &(n)

/* Add a node to the head of a list. */

#define	LIST_ADD_TAIL(l,n)	{ (n).next = NULL;			   \
	if ((l).head) { (l).tail->next = &(n); (n).prev = (l).tail; } 	   \
	else { (l).head = &(n); (n).prev = NULL; } (l).tail = &(n); }

/* Add a node to the tail of a list. */

#define	LIST_REMOVE(l,n)                                                   \
	if(!(n).prev) (l).head = (n).next; else (n).prev->next = (n).next; \
	if(!(n).next) (l).tail = (n).prev; else (n).next->prev = (n).prev

/* Remove any node from a list. */

#define	LIST_FOREACH(l,n) for ((n) = (l).head; (n); (n) = (n)->next)

/* Walk over each list element. */

#define	LIST_EMPTY(l)		!(l).head

/* Returns non-0 if the list is empty. */

#define	LIST_NONEMPTY(l)	(l).head

/* Returns non-0 if the list is non-empty. */

#endif /* !__dlist_h_ */

