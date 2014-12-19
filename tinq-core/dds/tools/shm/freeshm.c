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

/* freeshm.c -- Utility program to cleanup DDS shared memory. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#ifndef NOIPC
#include <sys/ipc.h>
#include <sys/shm.h>

#define	DDS_SHM_NAME	"DDSM"	/* DDS Node-global Shared memory. */

typedef struct dds_shm_st {
	unsigned	num_instances;	/* # of active DDS instances. */
	unsigned	participant_id;	/* Node-local participant id. counter.*/
	unsigned	host_id;	/* Host Id. */
	unsigned	cdd_pid;	/* Central Discovery daemon PID. */
	unsigned	cdd_domain;	/* Domain Id for Central Discovery. */
} DDS_SHM;

int main (int argc, char **argv)
{
	int		shm_id;
	unsigned	size;
	DDS_SHM		*mp;
	union {
		uint32_t	ul;
		char		str [4];
	}		us;

	/* Allow args in the future? */
	(void) argc;

	size = sizeof (DDS_SHM);
	memcpy (us.str, DDS_SHM_NAME, 4);
	shm_id = shmget (us.ul, size, 0666 | IPC_NOWAIT);
	if (shm_id < 0) {
		printf ("%s: DDS shared memory region doesn't exist!\n", argv [0]);
		return (0);
	}
	mp = (DDS_SHM *) shmat (shm_id, NULL, SHM_RDONLY);
	shmdt (mp);
	if (shmctl (shm_id, IPC_RMID, 0) == -1)
		printf ("%s: can't delete DDS shared memory region!\n", argv [0]);
	else
		printf ("%s: DDS shared memory segment deleted.\n", argv [0]);

	return (0);
}

#else
int main (int argc, char **argv)
{
	printf ("No IPC support on this target!\r\n");
	return (0);
}
#endif

