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

/* guid.c -- Implements operations on the Global Unique Identifier type. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include "win.h"
#define random	rand
#define srandom	srand
#elif defined (NUTTX_RTOS)
#include <unistd.h>
#include <arpa/inet.h>
#include "stdlib.h" 
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include "log.h"
#include "sys.h"
#include "ipc.h"
#include "error.h"
#include "guid.h"

#define	CLEANUP_SHM		/* Cleanup shared memory when last participant
				   stops with DDS. */

#define	DDS_SEM_NAME		"DDSS"	/* DDS Node-global Semaphore. */
#define	DDS_SHM_NAME		"DDSM"	/* DDS Node-global Shared memory. */
#define DDS_SHM_VERSION		0x100

#ifndef MAX_CDD_DOMAINS
#define	MAX_CDD_DOMAINS	4	/* Max. # of supported domains. */
#endif

#ifndef MAX_PARTICIPANTS
#define MAX_PARTICIPANTS 256
#endif

#define PARTICIPANT_BITSETS ((MAX_PARTICIPANTS + 31) >> 5)


typedef struct dds_shm_st {
	unsigned	version;			/* DDS Shared memory version. */ 
	unsigned	pids [PARTICIPANT_BITSETS];	/* */
	unsigned	recovering;			/* Host Id. */
	unsigned	host_id;			/* Host Id. */
	unsigned	cdd_pids [MAX_CDD_DOMAINS];	/* Central Discovery daemon PID. */
	unsigned	cdd_domains [MAX_CDD_DOMAINS];	/* Domain Id for Central Discovery. */
} DDS_SHM;

#ifndef NOIPC
static int	dds_sem;	/* DDS Semaphore. */
static DDS_SHM	*dds_shm;	/* DDS Shared Memory. */
#endif

unsigned	dds_exec_env;		/* Execution Environment identifier. */
unsigned	dds_participant_id;	/* Last used participant Id. */


GuidPrefix_t guid_prefix_unknown = { GUIDPREFIX_UNKNOWN };
EntityId_t entity_id_unknown = { ENTITYID_UNKNOWN };
EntityId_t entity_id_participant = { ENTITYID_PARTICIPANT };

/* Locally generated GUIDs will have the following format:

	bytes  0..3 : system host identifier (IPv4 address if found or random).
	byte      4 : domain identifier.
	byte      5 : participant identifier.
	bytes  6..7 : process identifier.
	bytes  8..9 : user identifier.
	byte     10 : execution environment identifier.
	byte     11 : counter.
*/

#define	GUID_HOSTID_OFS		0
#define	GUID_DID_OFS		4
#define	GUID_PARTID_OFS		5
#define	GUID_PID_OFS		6
#define	GUID_UID_OFS		8
#define	GUID_EE_OFS		10
#define	GUID_COUNT		11

static GuidPrefix_t guid_prefix_local;	/* Base prefix (domain/participant Id zeroed).*/

/* random_hid -- Return a random Host id. */

static unsigned random_hid (void)
{
	unsigned	hid;
	FTime_t		t;	/* System time. */

	/* Generate a unique Id. */
	sys_getftime (&t);
#if defined (NUTTX_RTOS)
	srand (2000);
	hid = rand();
#else	
	srandom (FTIME_FRACT (t));
	hid = random ();
#endif
	/* Add some extra variability using PID/Time. */
	sys_gettime ((Time_t *) &t);
	hid ^= (sys_pid () | FTIME_FRACT (t) << 15);
	return (hid);
}

int guid_init (unsigned eid)
{
	uint32_t	hid;	/* System host Id. */
	uint32_t	uid;	/* System UID. */
	unsigned	pid;	/* Process PID. */
#ifndef NOIPC
	union {
	  uint32_t	ul;
	  char		str [4];
	}		us;
#endif
	unsigned char	ipa [4];
	static unsigned	count = 0;


	/* Setup rtps_guid_prefix_local. */
	dds_exec_env = eid;
	uid = sys_uid ();
	pid = sys_pid ();
	guid_prefix_local.prefix [GUID_DID_OFS] = 0;
	guid_prefix_local.prefix [GUID_PARTID_OFS] = 0;
	guid_prefix_local.prefix [GUID_PID_OFS] = pid >> 8;
	guid_prefix_local.prefix [GUID_PID_OFS + 1] = pid;
	guid_prefix_local.prefix [GUID_UID_OFS] = uid >> 8;
	guid_prefix_local.prefix [GUID_UID_OFS + 1] = uid;
	guid_prefix_local.prefix [GUID_EE_OFS] = eid;
	guid_prefix_local.prefix [GUID_COUNT] = count++;

#ifndef NOIPC
	/* Get DDS shared memory and semaphore. */
	memcpy (us.str, DDS_SEM_NAME, 4);
	dds_sem = sem_get (us.ul, 1, 0666, SEMF_CREATE | SEMF_PUBLIC);
	if (dds_sem < 0) {
		err_printf ("guid_init: can't get DDS semaphore!");
		return (GUID_ERR_NOMEM);
	}
	memcpy (us.str, DDS_SHM_NAME, 4);
	dds_shm = (DDS_SHM *) shm_get (us.ul, sizeof (DDS_SHM), 0666, 
								SHMF_CREATE);
	if (!dds_shm) {
		err_printf ("guid_init: can't get DDS shared memory segment!");
		return (GUID_ERR_NOMEM);
	}

	/* Increment the node-local # of DDS instances. */
	sem_p (dds_sem);
	if (dds_shm->version != DDS_SHM_VERSION) {
		if (dds_shm->version == 0)
			dds_shm->version = DDS_SHM_VERSION;
		else {
			sem_v (dds_sem);
			err_printf ("guid_init: incorrect DDS shared memory version! Please clear using ddsfreeshm.");
			return (GUID_ERR_NOMEM);
		}
	}


	/* Get a unique node-local Identifier for all participants. */
	if (dds_shm->host_id) {
		log_printf (DOM_ID, 0, "Existing host identifier reused.\r\n");
		hid = dds_shm->host_id;
	}
	else {
#endif
		/* if it's not a global ip then use a random number */
		if (!sys_own_ipv4_addr (ipa, sizeof (ipa), 4, 4) ||
		    ipa [0] == 127) { /* Address really available? */
			hid = random_hid ();
			log_printf (DOM_ID, 0, "Random host identifier generated.\r\n");
		}
		else {
			hid = ipa [0] << 24 | ipa [1] << 16 |
			      ipa [2] << 8 | ipa [3];
			log_printf (DOM_ID, 0, "Using first IPv4 address as host identifier.\r\n");
		}
#ifndef NOIPC
		dds_shm->host_id = hid;
	}
	sem_v (dds_sem);
#endif
	guid_prefix_local.w [GUID_HOSTID_OFS] = ntohl (hid);
	return (GUID_OK);
}

GuidPrefix_t *guid_local (void)
{
	return (&guid_prefix_local);
}

void guid_normalise (GuidPrefix_t *gp)
{
	gp->prefix [GUID_DID_OFS] = 0;
	gp->prefix [GUID_PARTID_OFS] = 0;
	gp->prefix [GUID_COUNT] = 0;
}

void guid_finalize (GuidPrefix_t *gp, DomainId_t domain_id, unsigned pid)
{
	gp->prefix [GUID_DID_OFS] = domain_id;
	gp->prefix [GUID_PARTID_OFS] = pid;
	gp->prefix [GUID_COUNT] = 0;
}

void guid_final (void)
{
#ifndef NOIPC
	int i;
	if (!dds_shm)
		return;

#ifdef CLEANUP_SHM
	sem_p (dds_sem);
	for (i = 0; i < PARTICIPANT_BITSETS; i++)
		if (dds_shm->pids [i])
			break;

	if (i >= PARTICIPANT_BITSETS) {
		shm_free (dds_shm);
		dds_shm = NULL;
	}
	else {
		shm_detach (dds_shm);
		dds_shm = NULL;
	}
	sem_v (dds_sem);
#endif
	ipc_final ();
#endif
}

/* guid_new_participant -- Create a GUID based on a new Participant id for the
			   given domain and return the id. */

unsigned guid_new_participant (GuidPrefix_t *gp, DomainId_t did)
{
#ifdef NOIPC
	unsigned	pid = random_hid () & 0x7f;
#else
	unsigned	pid = ILLEGAL_PID;
#endif
	static int	counter = 0;

#ifndef NOIPC
	int 		restarted = 0;
	unsigned 	i, j;
	unsigned 	recovering;

	sem_p (dds_sem);
	recovering = dds_shm->recovering;

	/* Another process is recovering. Release sem, wait.... */
	if (recovering != 0) {
		sem_v (dds_sem);
		usleep (5000000);
		sem_p (dds_sem);
		if (recovering == dds_shm->recovering)
			dds_shm->recovering = 0;
	}

restart:
	for (i = 0; i < PARTICIPANT_BITSETS; i++) {
		if (dds_shm->pids [i] != 0xffffffffU) {
			for (j = 0; j < 32; j++)
				if ((dds_shm->pids [i] & (1 << j)) == 0) {
					dds_shm->pids [i] |= 1 << j;
					break;
				}

			pid = (i << 5) + j;

			break;
		}
	}

	if (i >= PARTICIPANT_BITSETS && !restarted) {

		if (!dds_shm->recovering) {
			for (i = 0; i < PARTICIPANT_BITSETS; i++)
				dds_shm->pids [i] = 0;

			dds_shm->recovering = sys_pid ();
			sem_v (dds_sem);
			usleep (3000000);
			sem_p (dds_sem);
			dds_shm->recovering = 0;
			restarted = 1;
			goto restart;
		}
		else {
			sem_v (dds_sem);
			usleep (5000000);
			sem_p (dds_sem);

			/* The other process finished recovering. */
			if (dds_shm->recovering == 0) {
				restarted = 1;
				goto restart;
			}
		}
	}
	dds_participant_id = pid;
	sem_v (dds_sem);
#endif
	*gp = guid_prefix_local;
	gp->prefix [GUID_DID_OFS] = did & 0xff;
	gp->prefix [GUID_PARTID_OFS] = pid & 0xff;
	gp->prefix [GUID_COUNT] = counter++ & 0xff;
	return (pid++);
}


/* guid_restate_participant -- Rewrites the bit identifying the provided
			       participant id in the shared memory region. */

void guid_restate_participant (unsigned pid)
{
#ifndef NOIPC
	sem_p (dds_sem);
	dds_shm->pids [pid >> 5] |= (1 << (pid & 31));
	sem_v (dds_sem);
#else
    ARG_NOT_USED(pid)
#endif
}

/* guid_free_participant -- Clear the bit identifying the provided participant
                            id in the shared memory region. */

void guid_free_participant (unsigned pid)
{
#ifndef NOIPC
	sem_p (dds_sem);
	dds_shm->pids [pid >> 5] &= ~(1 << (pid & 31));
	sem_v (dds_sem);
#else
    ARG_NOT_USED(pid)
#endif
}

/* guid_needs_marshalling -- Returns a non-0 result if the given GUID requires
			     marshalling. */

int guid_needs_marshalling (GuidPrefix_t *gp)
{
	/* Should be compatible if host identifier and execution environment
	   are similar.  We simply ignore the domain id, user id, process id
	   and participant id since these may differ without requiring
	   marshalling. */
	return (gp->w [0] != guid_prefix_local.w [0] ||
		gp->prefix [GUID_EE_OFS] !=
			guid_prefix_local.prefix [GUID_EE_OFS]);
}

#define	HEXCHAR(n)	((n) > 9) ? (n) - 10 + 'a' : (n) + '0'

/* Return a GUID prefix string. */

char *guid_prefix_str (const GuidPrefix_t *gp, char buffer [])
{
	unsigned	i, h, l, ofs;

	for (i = 0, ofs = 0; i < 12; i++) {
		if (i && (i & 0x3) == 0)
			buffer [ofs++] = ':';
		h = gp->prefix [i] >> 4;
		l = gp->prefix [i] & 0xf;
		buffer [ofs++] = HEXCHAR (h);
		buffer [ofs++] = HEXCHAR (l);
	}
	buffer [ofs] = '\0';
	return (buffer);
}

/* Return a GUID string. */

char *guid_str (const GUID_t *g, char buffer [])
{
	size_t	l;

	guid_prefix_str (&g->prefix, buffer);
	l = strlen (buffer);
	buffer [l] = '-';
	entity_id_str (&g->entity_id, &buffer [l + 1]);
	return (buffer);
}

/* Return an entity id string. */

char *entity_id_str (const EntityId_t *ep, char buffer [])
{
	unsigned	i, h, l, ofs;

	for (i = 0, ofs = 0; i < 4; i++) {
		if (i == 3)
			buffer [ofs++] = '-';
		h = ep->id [i] >> 4;
		l = ep->id [i] & 0xf;
		if (h || i < 3)
			buffer [ofs++] = HEXCHAR (h);
		buffer [ofs++] = HEXCHAR (l);
	}
	buffer [ofs] = '\0';
	return (buffer);
}


/* guid_local_component -- Return a non-0 result if the GUID prefix is from a
			   local component. */

int guid_local_component (GuidPrefix_t *gp)
{
	return (gp->w [0] == guid_prefix_local.w [0] &&
	        gp->prefix [GUID_EE_OFS] != 0xcd);
}


