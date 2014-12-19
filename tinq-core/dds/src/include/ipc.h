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

/* ipc.h -- Defines some system dependent IPC functionalities.
	    Following IPC classes are currently defined:

	    	- Semaphores
			- Shared Memory
 */

#ifndef __ipc_h_
#define __ipc_h_
#ifndef NOIPC
#include <stdint.h>


/* 1. Semaphores. 
   -------------- */

/* Various semaphore flags: */
#define	SEMF_CREATE	1	/* Create if it doesn't exist yet. */
#define	SEMF_NOWAIT	2	/* Don't wait if it doesn't exist. */
#define	SEMF_PUBLIC	0	/* A public semaphore. */
#define	SEMF_PRIVATE	4	/* A private semaphore. */

intptr_t sem_get (uint32_t name,	/* Id of semaphore (system-wide). */
	          unsigned val,		/* Initial value (# of users). */
	          unsigned perms,	/* System permissions. */
	          unsigned flags);	/* Various flags - see above. */

/* Try to get either an existing semaphore or create a new one.
   The function returns either: -1, i.e. it failed, or a semaphore reference. */

void sem_free (intptr_t sem);

/* Free a semaphore. */

void sem_p (intptr_t sem);

/* Procure (i.e. take) a semaphore. */

void sem_v (intptr_t sem);

/* Vacate (i.e. release) a semaphore. */


/* 2. Shared memory.
   ----------------- */

/* Various shared memory flags: */
#define	SHMF_CREATE	1	/* Create if it doesn't exist yet. */
#define	SHMF_NOWAIT	2	/* Don't wait if it doesn't exist. */

void *shm_get (uint32_t name,	/* Id of memory region (system-wide). */
	       size_t   size,	/* Size of memory region. */
	       unsigned perms,	/* System permissions. */
	       unsigned flags);	/* Various flags - see above. */

/* Try to get either an existing shared memory region or create a new one.
   The function returns either: NULL, i.e. it failed, or a pointer to the
   shared memory region. */

void shm_free (void *p);

/* Free a shared memory region. Since the shared memory might still be in use by
   some consumers, this should only be done when really needed. */

void shm_detach (void *p);

/* Detach from a shared memory region. */

/* 3. Housekeeping functions.
   -------------------------- */

void ipc_final (void);

/* On program exit, this function can be used to cleanup some resources. */

#endif
#endif /* !__ipc_h_ */

