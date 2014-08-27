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

/* handle.h -- Handle management functionality definitions. */

#ifndef __handle_h_
#define __handle_h_

#ifdef HBITMAP_USED
typedef unsigned handle_t;	/* Non-zero handle. */
#else
typedef unsigned short handle_t;		/* Non-zero handle. */
#endif

void *handle_init (unsigned nhandles);

/* Initialize a handle table for the given number of handles. */

void *handle_extend (void *ht, unsigned nhandles);

/* Extend the handle table with the new number of handles.
   Note that the new number of handles must be larger than the previous. */

void handle_reset (void *ht);

/* Reset the handle table so it is full again. */

void handle_final (void *ht);

/* Free a handle table containing the given number of handles. */

handle_t handle_alloc (void *ht);

/* Allocate a handle from the handle table. */

void handle_free (void *ht, handle_t handle);

/* Free a previously allocated handle. */

#endif /* !__handle_h_ */

