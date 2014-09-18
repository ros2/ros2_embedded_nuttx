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

/* ipc.c -- Implements some system dependent IPC functionalities such as
	    semaphores and shared memory. */

#include <stdio.h>
#ifndef NOIPC
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "winsock2.h"
#else
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#endif
#include "log.h"
#include "error.h"
#include "ipc.h"

#if defined (NUTTX_RTOS)
#include <semaphore.h>
#endif

#ifndef __APPLE__

union semun {
	int				val;
	struct 			semid_ds	*buf;
	unsigned short	*array;
};

#endif

/* sem_get -- Try to get either an existing semaphore or create a new one. */

intptr_t sem_get (unsigned name,	/* Id of semaphore (system-wide). */
	          unsigned val,		/* Initial value (# of users). */
	          unsigned perms,	/* System permissions. */
	          unsigned flags)	/* Various flags - see above. */
{
	intptr_t	sem_id;

#ifdef _WIN32
	char		sem_name [5];

	memcpy (sem_name, &name, 4);
	sem_name [4] = '\0';
	if ((flags & SHMF_CREATE) == 0)
		sem_id = (intptr_t) OpenSemaphoreA (0, FALSE, sem_name);
	else
		sem_id = (intptr_t) CreateSemaphoreA (NULL, 1, 1, sem_name);
	if (!sem_id)
		return (-1);
#elif defined (NUTTX_RTOS)
	/* ToDo - continue here if IPC desires to be ported.
	 		for now the -DNOIPC flag is used. Apparently this
	 		leads to a less efficient performance.
	*/
	int sem_init(sem_t *sem, int pshared, unsigned int value);
	sem_t *sem_open(const char *name, int oflag, ...);

#else
	int		s_flags = (int) perms;
	union 	semun	sem_union;

	if ((flags & SEMF_PRIVATE) != 0)
		name = IPC_PRIVATE;
	if ((flags & SEMF_NOWAIT) != 0)
		s_flags |= IPC_NOWAIT;

	if ((flags & SEMF_CREATE) == 0) { /* Reference to a public semaphore? */
		sem_id = semget ((key_t) name, 1, s_flags);
		return (sem_id);
	}
	if ((flags & SEMF_PRIVATE) == 0) { /* Public semaphore. */

		/* If it already exists -- nothing left to do; creator has
		   already initialized it! */
		sem_id = semget ((key_t) name, 1, s_flags | IPC_NOWAIT);
		if (sem_id >= 0)
			return (sem_id);
	}

	/* Doesn't exist yet -- create new one. */
	sem_id = semget ((key_t) name, 1, s_flags | IPC_CREAT);
	if (sem_id < 0) {
		log_printf (LOG_DEF_ID, 0, "sem_get: failed to create!\r\n");
		return (sem_id);	/* Error creating? */
	}

	/* Successfully created -- now initialize it. */
	sem_union.val = val;
	if (semctl (sem_id, 0, SETVAL, sem_union) < 0) { /* Can't init: abort! */
		log_printf (LOG_DEF_ID, 0, "sem_get: failed to initialize!\r\n");
		sem_free (sem_id);
		sem_id = -1;
	}
#endif
	return (sem_id);
}

/* sem_free -- Free a semaphore. */

void sem_free (intptr_t sem_id)
{

#ifdef _WIN32
	CloseHandle ((HANDLE) sem_id);
#else
	union semun	sem_union;

	semctl (sem_id, 0, IPC_RMID, sem_union);
#endif
}

/* sem_p -- Procure (i.e. take) a semaphore. */

void sem_p (intptr_t sem)
{
#ifdef _WIN32
	WaitForSingleObject ((HANDLE) sem, INFINITE);
#else
	struct sembuf	b;

	b.sem_num = 0;
	b.sem_op = -1;
	b.sem_flg = SEM_UNDO;
	if (semop (sem, &b, 1) < 0)
		log_printf (LOG_DEF_ID, 0, "sem_p: failed!\r\n");
#endif
}

/* sem_v -- Vacate (i.e. release) a semaphore. */

void sem_v (intptr_t sem)
{
#ifdef _WIN32
	ReleaseSemaphore ((HANDLE) sem, 1, NULL);
#else
	struct sembuf	b;

	b.sem_num = 0;
	b.sem_op = 1;
	b.sem_flg = SEM_UNDO;
	if (semop (sem, &b, 1) < 0)
		log_printf (LOG_DEF_ID, 0, "sem_v: failed!\r\n");
#endif
}


#define	SHM_NREFS_INC	16	/* # of references to add to table. */

typedef struct shm_ref_st {
	void		*ptr;		/* Points to block. */
	size_t		size;		/* Size of memory block. */
	unsigned long	name;		/* Name of memory block. */
	uintptr_t	id;		/* Shared memory Id. */
} SHM_REF;

static SHM_REF	*shm_refs = NULL;	/* Pointer to reference array. */
static unsigned	shm_maxrefs;		/* Size of reference array. */
static unsigned	shm_currefs;		/* # of references in array. */

/* shm_add -- Add a new shared memory block to the table of shared memory
	      blocks. */

static void shm_add (void *p, size_t size, unsigned name, uintptr_t id)
{
	SHM_REF		*rp, *np;
	unsigned	i;

	if (shm_currefs == shm_maxrefs) {
		shm_maxrefs += SHM_NREFS_INC;
		np = realloc (shm_refs, sizeof (SHM_REF) * shm_maxrefs);
		if (!np)
			fatal_printf ("shm_add: can't realloc(%u) shared memory references array!", shm_maxrefs);

		shm_refs = np;
		rp = &shm_refs [shm_currefs++];
		memset (rp + 1, 0, sizeof (SHM_REF) * (SHM_NREFS_INC - 1));
	}
	else
		for (i = 0, rp = shm_refs; i < shm_currefs; i++, rp++)
			if (!rp->ptr)
				break;
	rp->ptr  = p;
	rp->size = size;
	rp->name = name;
	rp->id   = id;
}

/* shm_lookup -- Lookup a pointer in the table of shared memory blocks and
		 a reference to the descriptor. */

static SHM_REF *shm_lookup (void *p)
{
	SHM_REF		*rp;
	unsigned	i;

	for (i = 0, rp = shm_refs; i < shm_currefs; i++, rp++)
		if (rp->ptr == p)
			return (rp);

	log_printf (LOG_DEF_ID, 0, "shm_lookup: pointer not in table!\r\n");

	return (NULL);
}

/* shm_get -- Try to get either an existing shared memory region or create a
	      new one. */

void *shm_get (unsigned name,	/* Id of memory region. */
	       size_t   size,	/* Size of memory region. */
	       unsigned perms,	/* System permissions. */
	       unsigned flags)	/* Various flags - see above. */
{
	void		*p;
#ifdef _WIN32
	uintptr_t	shm_id;
	HANDLE		map_file;
	char		mem_name [5];

	memcpy (mem_name, &name, 4);
	mem_name [4] = '\0';
	if ((flags & SHMF_CREATE) == 0)
		map_file = OpenFileMappingA (FILE_MAP_ALL_ACCESS, FALSE, mem_name);
	else
		map_file = CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, 
						PAGE_READWRITE,
						0, size, mem_name);
	if (!map_file)
		return (NULL);

	p = MapViewOfFile (map_file, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (!p)
		CloseHandle (map_file);

	shm_id = (uintptr_t) map_file;
#else
	int	shm_id, s_flags = (int) perms;

	if ((flags & SHMF_NOWAIT) != 0)
		s_flags |= IPC_NOWAIT;

	if ((flags & SHMF_CREATE) == 0) { /* Reference to a public memory? */
		shm_id = shmget ((key_t) name, size, s_flags);
		if (shm_id < 0)
			return (NULL);
	}
	else {	/* If it already exists -- nothing left to do; creator has
		   already initialized it! */
		shm_id = shmget ((key_t) name, size, s_flags | IPC_NOWAIT);
		if (shm_id < 0) {

			/* Doesn't exist yet -- create new one. */
			shm_id = shmget ((key_t) name, size, s_flags | IPC_CREAT);
			if (shm_id < 0) {
				log_printf (LOG_DEF_ID, 0, "shm_get: failed to create!\r\n");
				return (NULL);	/* Error creating? */
			}
		}
	}
	p = shmat (shm_id, NULL, 0);
	if (p == (void *) -1) {
		err_printf ("shm_get: failed to attach!\n");
		p = NULL;
		return (NULL);
	}

#endif
	shm_add (p, size, name, shm_id);
	return (p);
}

/* shm_free -- Free a shared memory region. Since the shared memory might still
	       be in use by some consumers, this should only be done when really
	       needed. */

void shm_free (void *p)
{
	SHM_REF	*rp;

	if ((rp = shm_lookup (p)) == NULL) {
		log_printf (LOG_DEF_ID, 0, "shm_free: no such shared memory!\r\n");
		return;
	}
#ifdef _WIN32
	UnmapViewOfFile (rp->ptr);
	CloseHandle ((HANDLE) rp->id);
#else
	if (shmdt (p) < 0) {
		log_printf (LOG_DEF_ID, 0, "shm_free: shmdt(%p) returned error!\r\n", p);
		return;
	}
	if (shmctl (rp->id, IPC_RMID, 0) == -1)
		log_printf (LOG_DEF_ID, 0, "shm_free: can't delete shared memory region!\r\n");
#endif
	shm_currefs--;
	rp->id = -1;
	rp->ptr = NULL;
}

void shm_detach (void *p)
{
	SHM_REF	*rp;

	if ((rp = shm_lookup (p)) == NULL) {
		log_printf (LOG_DEF_ID, 0, "shm_free: no such shared memory!\r\n");
		return;
	}
#ifdef _WIN32
	UnmapViewOfFile (rp->ptr);
	CloseHandle ((HANDLE) rp->id);
#else
	if (shmdt (p) < 0)
		log_printf (LOG_DEF_ID, 0, "shm_free: shmdt(%p) returned error!\r\n", p);
#endif
}

void ipc_final (void)
{
#ifdef _WIN32
	SHM_REF		*rp;
	unsigned	i;

	for (i = 0, rp = shm_refs; i < shm_maxrefs; i++, rp++)
		if (rp->ptr)
			shm_free (rp->ptr);
#endif
	if (!shm_refs)
		return;

	free (shm_refs);
	shm_refs = NULL;
	shm_currefs = shm_maxrefs = 0;
}
#endif
