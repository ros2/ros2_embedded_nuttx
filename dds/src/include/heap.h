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

/* heap.h -- Simple storage allocator for user-defined heaps. */

#ifndef __heap_h_
#define	__heap_h_

int heap_init (void *addr, size_t size, unsigned *heap_id);

/* Create a heap on a previously allocated memory block. */

void heap_discard (unsigned heap_id);

/* Make sure that the heap with the given heap id. can no longer be used. */

void *heap_alloc (unsigned heap_id, size_t size);

/* Allocate a memory region of the given size. */

void heap_free (void *addr);

/* Free a previously allocated memory region. */

void *heap_realloc (void *addr, size_t new_size);

/* Change the size of a previously allocated memory region. */

size_t heap_avail (unsigned heap_id);

/* Return the amount of bytes available in the heap. */

size_t heap_max (unsigned heap_id);

/* Return the size of the largest free block. */

void heap_dump (unsigned heap_id);

/* For debug purposes: dumps the current heap contents. */

#endif /* !__heap_h_ */

