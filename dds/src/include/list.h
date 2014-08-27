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

/* list.h -- Defines some common list manipulation macros. */

#ifndef __list_h_
#define __list_h_

/* To use these macros, it is enough that each list node contains two pointers,
   i.e. 'next' and 'prev' that point respectively to the next and the previous
   list node, and that a list header structure exists that contains fields
   'head' and 'tail' that point respectively to the first and the last list
   nodes.
   Note that the offsets of head and tail in the list structure *must* be
   identical to the offsets of next and prev in the node structure for these
   macros to work correctly. */

typedef struct l_node_st ListNode_t;
struct l_node_st {
	ListNode_t	*next;
	ListNode_t	*prev;
};

typedef struct l_list_st List_t;
struct l_list_st {
	ListNode_t	*head;
	ListNode_t	*tail;
};

#define	LIST_INIT(l)		(l).head = (l).tail = (void *) &(l)

/* Initialise a list. */

#define LIST_HEAD(l)		(((l).head == (void *) &(l)) ? NULL : (l).head)

/* Return a pointer to the first node in the list. */

#define LIST_TAIL(l)		(((l).head == (void *) &(l)) ? NULL : (l).tail)

/* Return a pointer to the first node in the list. */

#define	LIST_NEXT(l,n)		(((n).next == (void *) &(l)) ? NULL : (n).next)

/* Return the successor node of a node or NULL if it is the last one. */

#define	LIST_PREV(l,n)		(((n).prev == (void *) &(l)) ? NULL : (n).prev)

/* Return the predecessor node of a node or NULL if it is the last one. */

#define	LIST_ADD_HEAD(l,n)	do { (n).next = (l).head; (n).prev = (void *) &(l); \
				(l).head->prev = &(n); (l).head = &(n); } while (0)

/* Add a node to the head of a list. */

#define	LIST_ADD_TAIL(l,n)	do { (n).next = (void *) &(l); (n).prev = (l).tail; \
				(l).tail->next = &(n); (l).tail = &(n); } while (0)

/* Add a node to the tail of a list. */

#define LIST_IS_SINGLETON(l,n)	do { (l).head = (l).tail = &(n); \
				(n).prev = (n).next = (void *) &l; } while (0)

/* Update a list to contain a single element, whatever the previous state. */

#define	LIST_INSERT(n,pn)	do { (n).next = (pn).next; (n).prev = &(pn); \
				(pn).next->prev = &(n); (pn).next = &(n); } while (0)

/* Insert a node between (pn) and the successor of (pn). */

#define	LIST_REMOVE(l,n)	do { (n).prev->next = (n).next; \
				(n).next->prev = (n).prev; } while (0)

/* Remove a node from a list. */

#define	LIST_FOREACH(l,n)	for ((n) = (l).head; (n) != (void *) &(l); \
						(n) = (n)->next)

/* Walk over each list element. */

#define	LIST_END(l,n)		(n) == (void *) &(l)

/* Check if walk completed successfully. */

#define	LIST_EMPTY(l)		((l).head == (void *) &(l))

/* Returns non-0 if the list is empty. */

#define	LIST_NONEMPTY(l)	((l).head != (void *) &(l))

/* Returns non-0 if the list is non-empty. */

#define	LIST_SINGLE(n)		((n).next->next == &(n))

/* Returns a non-0 result if the list contains a single element. */

#endif /* !__list_h_ */

