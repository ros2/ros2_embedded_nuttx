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

/* showshm.c -- Utility program to dump DDS shared memory. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
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

	size = sizeof (DDS_SHM);
	memcpy (us.str, DDS_SHM_NAME, 4);
	shm_id = shmget (us.ul, size, 0666 | IPC_NOWAIT);
	if (shm_id < 0) {
		printf ("%s: shared memory region doesn't exist!\n", argv [0]);
		return (0);
	}
	mp = (DDS_SHM *) shmat (shm_id, NULL, SHM_RDONLY);
	printf ("Contents of shared memory segment:\r\n");
	printf ("\t# of instances = %u.\n", mp->num_instances);
	printf ("\tParticipant id = %u.\n", mp->participant_id);
	printf ("\tHost id        = 0x%x\n",mp->host_id);
	printf ("\tCDD pid        = %u.\n", mp->cdd_pid);
	printf ("\tCDD domain     = %u.\n", mp->cdd_domain);
	shmdt (mp);
	return 0;
}
