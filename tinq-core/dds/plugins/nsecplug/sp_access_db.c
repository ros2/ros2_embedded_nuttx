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

#include "sp_access_db.h"
#include "nsecplug/nsecplug.h"
#include "sp_access.h"
#include <string.h>
#include "dlist.h"
#include "thread.h"
#include "strseq.h"
#include "nmatch.h"
#include "log.h"
#include "dds/dds_error.h"
#include "error.h"
#include "sec_access.h"
#include <stdlib.h>

struct ms_domains_st	domains;
unsigned		num_domains;

struct ms_participants_st	participants;
unsigned			num_ids;

unsigned num_permissions;

unsigned topic_counter;
unsigned partition_counter;
unsigned input_counter = 1;

static int inits = 0;

#define ENGINE_INIT 1
#define DB_INIT     2
#define LOCK_INIT   4

lock_t sp_lock;

int sp_access_has_wildcards (const char *data) 
{
	if (!data)
		return (0);
	if (strchr (data, '*') || strchr (data, '?'))
		return (1);
	return (0);
}

static void db_init (void)
{
	if (inits & DB_INIT)
		return;
	
	LIST_INIT (domains);
	num_domains = 0;
	LIST_INIT (participants);
	num_ids = 0;
	topic_counter = 0;
	partition_counter = 0;
	inits |= DB_INIT;
	num_permissions = 0;	
}

static void lock_init (void)
{
	if (inits & LOCK_INIT)
		return;

	lock_init_nr (sp_lock, "Security Plugin Lock");
	inits |= LOCK_INIT;
}

static void *create_empty_topic (ListTypes type)
{
	MSTopic_t  *dt;
	MSUTopic_t *pt;

	switch (type) {
	case LIST_DOMAIN:
		if (!(dt = calloc (1, sizeof (MSTopic_t))))
			return (NULL);
		if (!(dt->name = malloc (strlen ("*") + 1)))
			return (NULL);
		strcpy (dt->name, "*");
		dt->mode = TA_ALL;
		dt->controlled = 1;
		dt->disc_enc = 1;
		dt->submsg_enc = 0;
		dt->payload_enc = 1;
		dt->crypto_mode = 3; /* AES128_SHA1 */
		dt->index = 0;
		dt->blacklist = 0;
		return ((void *) dt);
	case LIST_PARTICIPANT:
		if (!(pt = calloc (1, sizeof (MSUTopic_t))))
			return (NULL);
		pt->id = ~0; /* all domains */
		if (!(pt->topic.name = malloc (strlen ("*") + 1)))
			return (NULL);
		strcpy (pt->topic.name, "*");
		pt->topic.mode = TA_ALL;
		pt->topic.controlled = 1;
		pt->topic.disc_enc = 1;
		pt->topic.submsg_enc = 0;
		pt->topic.payload_enc = 1;
		pt->topic.crypto_mode = 3;
		pt->topic.index = 0;
		pt->topic.blacklist = 0;
		return ((void *) pt);
	default:
		return (NULL);
	}
}

static void *create_empty_partition (ListTypes type)
{
	MSPartition_t  *dp;
	MSUPartition_t *pp;

	switch (type) {
	case LIST_DOMAIN:
		if (!(dp = calloc (1, sizeof (MSPartition_t))))
			return (NULL);
		dp->name = NULL; /* name == '*' */
		dp->mode = TA_ALL;
		dp->index = 0;
		dp->blacklist = 0;
		return ((void *) dp);
	case LIST_PARTICIPANT:
		if (!(pp = calloc (1, sizeof (MSUPartition_t))))
			return (NULL);
		pp->id = ~0; /* all domains */
		pp->partition.name = NULL; /* name == '*' */
		pp->partition.mode = TA_ALL;
		pp->partition.index = 0;
		pp->partition.blacklist = 0;
		return ((void *) pp);
	default:
		return (NULL);
	}
}

static DDS_ReturnCode_t reorder_participants (void)
{
	MSParticipant_t *p;
	unsigned i = 1;

	for (i = 1; i < input_counter; i++) {
		LIST_FOREACH (participants, p) {
			if (p->input_number == i) {
				LIST_REMOVE (participants, *p);
				LIST_ADD_TAIL (participants, *p);
				break;
			}
		}
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t optimize_db (void)
{
	MSDomain_t      *d, *dlast, *dnext;
	MSParticipant_t *p, *plast, *pnext;
	MSTopic_t       *dt, *dtlast, *dtnext;
	MSUTopic_t      *pt, *ptlast, *ptnext;
	MSPartition_t   *dp, *dplast, *dpnext;
	MSUPartition_t  *pp, *pplast, *ppnext;

	if (participants.head) {
		plast = participants.tail->next;
		for (p = participants.head; p != plast; p = pnext) {
			pnext = p->next;
			
			if (p->topics.head) {				
				/* optimize the topics and partitions */
				ptlast = p->topics.tail->next;
				for (pt = p->topics.head; pt != ptlast; pt = ptnext) {
					ptnext = pt->next;
					if (!sp_access_has_wildcards (pt->topic.name)) {
						LIST_REMOVE (p->topics, *pt);
						LIST_ADD_HEAD (p->topics, *pt);
					}
				}
			} else {
				/* If there are no more topics for this participant
				   add one to allow everything but only on not cloned participants */
				if (!p->cloned) {
					if (!(pt = (MSUTopic_t *) create_empty_topic (LIST_PARTICIPANT)))
						fatal_printf ("Out of memory");
					LIST_INIT (p->topics);
					LIST_ADD_TAIL (p->topics, *pt);
					p->ntopics ++;
				}
			}
			if (p->partitions.head) {
				pplast = p->partitions.tail->next;
				for (pp = p->partitions.head; pp != pplast; pp = ppnext) {
					ppnext = pp->next;
					if (!sp_access_has_wildcards (pp->partition.name)) {
						LIST_REMOVE (p->partitions, *pp);
						LIST_ADD_HEAD (p->partitions, *pp);
					}
				}		
			} else {
				/* If there are no more partitions for this participant
				   add one to allow everything but only on not cloned participants */
				if (!p->cloned) {
					if (!(pp = (MSUPartition_t *) create_empty_partition (LIST_PARTICIPANT)))
						fatal_printf ("Out of memory");
					LIST_INIT (p->partitions);
					LIST_ADD_TAIL (p->partitions, *pp);
					p->npartitions++;
				}
			}
		}
	}
	if (domains.head) {
		dlast = domains.tail->next;
		for (d = domains.head; d != dlast; d = dnext) {
			dnext = d->next;
			
			if (d->topics.head) {
				/* optimize the topics and partitions */
				dtlast = d->topics.tail->next;
				for (dt = d->topics.head; dt != dtlast; dt = dtnext) {
					dtnext = dt->next;
					if (!sp_access_has_wildcards (dt->name)) {
						LIST_REMOVE (d->topics, *dt);
						LIST_ADD_HEAD (d->topics, *dt);
					}
				}
			}  else {
				/* If there are no more topics for this participant
				   add one to allow everything */
				if (!(dt = (MSTopic_t *) create_empty_topic (LIST_DOMAIN)))
					fatal_printf ("Out of memory");
				LIST_INIT (d->topics);
				LIST_ADD_TAIL (d->topics, *dt);
				d->ntopics ++;
			}
			if (d->partitions.head) {
				dplast = d->partitions.tail->next;
				for (dp = d->partitions.head; dp != dplast; dp = dpnext) {
					dpnext = dp->next;
					if (!sp_access_has_wildcards (dp->name)) {
						LIST_REMOVE (d->partitions, *dp);
						LIST_ADD_HEAD (d->partitions, *dp);
					}
				}
			} else {
				/* If there are no more partitions for this participant
				   add one to allow everything */
				if (!(dp = (MSPartition_t *) create_empty_partition (LIST_PARTICIPANT)))
					fatal_printf ("Out of memory");
				LIST_INIT (d->partitions);
				LIST_ADD_TAIL (d->partitions, *dp);
				d->npartitions++;
			}

		}
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t cleanup_stale_participant (MSParticipant_t *participant)
{
	DDS_ReturnCode_t ret;	
	MSUTopic_t       *t, *tnext, *tlast;
	MSUPartition_t   *p, *pnext, *plast;

	if (!participants.head)
		return (DDS_RETCODE_OK);

	if (participant->topics.head) {
		tlast = participant->topics.tail->next;
		for (t = participant->topics.head; t != tlast; t = tnext) {
			tnext = t->next;
			if (t->topic.refreshed == 0) {
				if (t->topic.index)
					if ((ret = sp_access_remove_topic (t->topic.index, 
									   participant->handle, 
									   LIST_PARTICIPANT)))
						return (ret);
			} else
				t->topic.refreshed = 0;
		}
	}
	
	if (participant->partitions.head) {
		plast = participant->partitions.tail->next;
		for (p = participant->partitions.head; p != plast; p = pnext) {
			pnext = p->next;
			if (p->partition.refreshed == 0) {
				if (p->partition.index)
					if ((ret = sp_access_remove_partition (p->partition.index,
									       participant->handle,
									       LIST_PARTICIPANT)))
						return (ret);
			} else
				p->partition.refreshed = 0;
		}
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t cleanup_stale_domain (MSDomain_t *d)
{
	DDS_ReturnCode_t ret;
	MSTopic_t        *t, *tnext, *tlast;
	MSPartition_t    *p, *pnext, *plast;

	if (!domains.head)
		return (DDS_RETCODE_OK);
	
	if (d->topics.head) {
		tlast = d->topics.tail->next;
		for (t = d->topics.head; t != tlast; t = tnext) {
			tnext = t->next;
			if (t->refreshed == 0) {
				if (t->index)
					if ((ret = sp_access_remove_topic (t->index,
									   d->handle,
									   LIST_DOMAIN)))
						return (ret);
			} else
				t->refreshed = 0;
		}
	}

	if (d->partitions.head) {
		plast = d->partitions.tail->next;
		for (p = d->partitions.head; p != plast; p = pnext) {
			pnext = p->next;
			if (p->refreshed == 0) {
				if (p->index)
					if ((ret = sp_access_remove_partition (p->index,
									       d->handle,
									       LIST_DOMAIN)))
						return (ret);
			} else
				p->refreshed = 0;
		}
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t cleanup_stale_db (void)
{
	MSParticipant_t  *p, *pnext, *plast;
	MSDomain_t       *d, *dnext, *dlast;
	DDS_ReturnCode_t ret;

       	log_printf (SEC_ID, 0, "SP_ACCESS_DB: Cleanup stale entries in the db\r\n");

	/* This might give problems when the parent is gone, but the clone has a ref to the parent */
	if (participants.head) {
		plast = participants.tail->next;
		for (p = participants.head; p != plast; p = pnext) {
			pnext = p->next;
			if (p->refreshed == 0) {
				if (p->cloned) {
					/* this means it is an unchecked participant */
					if (p->cloned == p)
						continue;
					/* If the clone is not refreshed, we can remove this participant */
					if (p->cloned->refreshed == 0) {
						if ((ret = DDS_SP_remove_participant (p->handle)))
							return (ret);
					}
					else {
						p->updated_perm_handle = ++num_permissions;
					}
				} else
					if ((ret = DDS_SP_remove_participant (p->handle)))
						return (ret);
			} else {
				if ((ret = cleanup_stale_participant (p)))
					return (ret);
				p->refreshed = 0;
			}
		}
	}
	if (domains.head) {
		dlast = domains.tail->next;
		for (d = domains.head; d != dlast; d = dnext) {
			dnext = d->next;
			if (d->refreshed == 0) {
				if ((ret = DDS_SP_remove_domain (d->handle)))
					return (ret);
			} else { 
				if ((ret = cleanup_stale_domain (d)))
					return (ret);
				d->refreshed = 0;
			}
		}
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t rematch_cloned_participants (void)
{
	MSParticipant_t *pclone, *porigin;

	LIST_FOREACH (participants, pclone) {
		if (pclone->cloned && pclone->cloned != pclone) {
			LIST_FOREACH (participants, porigin) {
				/* They are not equal */
				if (!porigin->cloned) {
					if (porigin->name [0] == '\0' ||
					    (strchr (porigin->name, '*') && !nmatch (porigin->name, pclone->name, 0))) {
						log_printf (SEC_ID, 0, "%s has been rematched with %s and no longer with %s ", pclone->name, porigin->name, pclone->cloned->name);
						pclone->cloned = porigin;
						break;
					}
					else if (!strcmp (porigin->name, pclone->name)) {
						log_printf (SEC_ID, 0, "%s has been rematched with %s and no longer with %s ", pclone->name, porigin->name, pclone->cloned->name);
						pclone->cloned = porigin;
						break;
					}
				}
			}
		}
	}
	return (DDS_RETCODE_OK);
}

void sp_access_init (void)
{
	if ((inits & DB_INIT) && (inits & LOCK_INIT))
		return;

	db_init ();
	lock_init ();
}

/* This should be called before every change made to the database,
   even when removing the complete database */

DDS_ReturnCode_t sp_access_update_start (void)
{
	log_printf (SEC_ID, 0, "SP_ACCESS_DB: Update access db start\r\n");

	sp_access_init ();
	input_counter = 1;

	/* Take access db lock */
	lock_take (sp_lock);
	return (DDS_RETCODE_OK);
}

/* This should be called when every change to the database is done */
DDS_ReturnCode_t sp_access_update_done (void)
{
	/* Call listener functions if changes have happened */
	reorder_participants ();
	cleanup_stale_db ();
	rematch_cloned_participants ();
	optimize_db ();

	/* release access db lock */
	lock_release (sp_lock);

	/* update local permissions, is already locked */
	dds_sec_local_perm_update (0);
		
	/* lock_destroy (sp_lock); */
	
	/* This should actually be done in some other function */
	/* inits &= ~LOCK_INIT;  */
	log_printf (SEC_ID, 0, "SP_ACCESS_DB: Update access db done\r\n");
	return (DDS_RETCODE_OK);
}

MSDomain_t *sp_access_get_domain (DomainHandle_t handle)
{
	MSDomain_t *d;

	LIST_FOREACH (domains, d)
		if (d->handle == handle)
			return (d);

	return (NULL);
}

int sp_access_get_domain_handle (DDS_DomainId_t id)
{
	MSDomain_t *d;

	LIST_FOREACH (domains, d)
		if (d->domain_id == id)
			return (d->handle);

	return (-1);
}

MSDomain_t *sp_access_add_domain (DomainHandle_t *handle)
{
	MSDomain_t     *d;
	MSTopic_t      *tp;
	MSPartition_t  *pp;

	d = calloc (1, sizeof (MSDomain_t));
	if (!d)
		return (NULL);

	d->domain_id = ~0; /* all other domains */
	d->access = DS_SECRET;
	d->exclusive = 0; /* open */
#ifdef DDS_SECURITY
	d->transport = TRANS_BOTH_DTLS_UDP;
#else
	d->transport = TRANS_BOTH_NONE;
#endif
	d->blacklist = 0;

	/* allow all topics in the domain */
	LIST_INIT (d->topics);
	if (!(tp = (MSTopic_t *) create_empty_topic (LIST_DOMAIN)))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (d->topics, *tp);
	d->ntopics++;

	/* allow all partitions in the domain */
	LIST_INIT (d->partitions);
	if (!(pp = (MSPartition_t *) create_empty_partition (LIST_DOMAIN)))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (d->partitions, *pp);
	d->npartitions++;

	d->handle = ++num_domains;
	*handle = num_domains;
	LIST_ADD_TAIL (domains, *d);
	return (d);
}

/* Removes only the domain, not the topics and partitions added to this domain */
DDS_ReturnCode_t sp_access_remove_domain (DomainHandle_t handle)
{
	MSDomain_t *d;

	if (!(d = sp_access_get_domain (handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (LIST_HEAD (d->topics) &&
	    LIST_HEAD (d->topics)->index == 0)
		sp_access_remove_topic (0, d->handle, LIST_DOMAIN);

	if (LIST_HEAD (d->partitions) &&
	    LIST_HEAD (d->partitions)->index == 0)
		sp_access_remove_partition (0, d->handle, LIST_DOMAIN);

	if (d->npartitions ||
	    d->ntopics)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	LIST_REMOVE (domains, *d);
	free (d);
	return (DDS_RETCODE_OK);
}

MSDomain_t *sp_access_lookup_domain (unsigned domain_id, 
				     int      specific)
{
	MSDomain_t	*p = NULL;

	LIST_FOREACH (domains, p) 
		if (p->domain_id == domain_id ||
		    (specific && p->domain_id == ~0U))
			return (p);

	return (NULL);
}

/* Participant functions */
MSParticipant_t *sp_access_get_participant (ParticipantHandle_t handle)
{
	MSParticipant_t *p;

	LIST_FOREACH (participants, p)
		if (p->handle == handle)
			return (p);

	return (NULL);
}

MSParticipant_t *sp_access_get_participant_by_perm (PermissionsHandle_t perm)
{
	MSParticipant_t *p;

	if (!perm)
		return (NULL);

	LIST_FOREACH (participants, p)
		if (p->updated_perm_handle == perm || 
		    p->permissions_handle == perm)
			return (p);

	return (NULL);
}

int sp_access_get_participant_handle (char *name)
{
	MSParticipant_t *p;

	LIST_FOREACH (participants, p)
		if (!(strcmp (name, p->name)))
			return (p->handle);

	return (-1);	
}

MSParticipant_t *sp_access_add_participant (ParticipantHandle_t *handle)
{
	MSParticipant_t *p;
	MSUTopic_t	*tp = NULL;
	MSUPartition_t	*pp = NULL;

	p = calloc (1, sizeof (MSParticipant_t));
	if (!p)
		return (NULL);

	strcpy (p->name, "*");
	p->access = DS_SECRET;
	p->blacklist = 0;

	LIST_INIT (p->topics);
	if (!(tp = (MSUTopic_t *) create_empty_topic (LIST_PARTICIPANT) ))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (p->topics, *tp);
	p->ntopics++;

	LIST_INIT (p->partitions);
	if (!(pp = (MSUPartition_t *) create_empty_partition (LIST_PARTICIPANT)))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (p->partitions, *pp);
	p->npartitions++;

	p->handle = ++num_ids;
	*handle = num_ids;
	LIST_ADD_TAIL (participants, *p);
	return (p);
}

DDS_ReturnCode_t sp_access_remove_participant (ParticipantHandle_t handle)
{
	MSParticipant_t *p;

	if (!(p = sp_access_get_participant (handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	log_printf (SEC_ID, 0, "SP_ACCESS_DB: remove paricipant [%d]\r\n", handle);

	LIST_REMOVE (participants, *p);
	free (p);
	return (DDS_RETCODE_OK);
}

int sp_access_is_already_cloned (const unsigned char *key,
				 size_t              klength)
{
	MSParticipant_t *p;

	LIST_FOREACH (participants, p)
		if (p->key_length == klength &&
		    !memcmp (p->key, key, klength))
			return (p->handle);
	return (-1);
}

DDS_ReturnCode_t sp_access_clone_participant (MSParticipant_t     *wp,
					      const char          *name,
					      size_t              length,
					      const unsigned char *key,
					      size_t              klength,
					      IdentityHandle_t    *handle)
{
	MSParticipant_t	*p;

	if (length >= MAX_PARTICIPANT_NAME) {
		warn_printf ("MSP: identity name too long!");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	p = malloc (sizeof (MSParticipant_t));
	if (!p)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	memset (p, 0, sizeof (MSParticipant_t));
	p->handle = ++num_ids;
	memcpy (p->name, name, length + 1);
	memcpy (p->key, key, klength);
	p->key_length = klength;
	p->cloned = wp;
	p->input_number = 0;
	LIST_ADD_HEAD (participants, *p);
	p->permissions_handle = ++num_permissions;
	p->updated_perm_handle = 0;
	*handle = p->handle;

	return (DDS_RETCODE_OK);
}

/* If ret = DDS_RETCODE_BAD_PARAMETER, continue looping */
/* If there are no participants return DDS_RETCODE_ERROR */

DDS_ReturnCode_t sp_access_participant_walk (SP_ACCESS_PARTICIPANT_CHECK fnc, void *data)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_ERROR;
	MSParticipant_t *p;

	LIST_FOREACH (participants, p) {
		if ((ret = fnc (p, data)) == DDS_RETCODE_OK)
			break;
		else if (ret == DDS_RETCODE_BAD_PARAMETER)
			continue;
		else
			break;
	}
	return (ret);
}

DDS_ReturnCode_t sp_access_add_unchecked_participant (const unsigned char *identity,
						      size_t              length,
						      const unsigned char *key,
						      size_t              klength,
						      IdentityHandle_t    *handle)
{
	MSParticipant_t *p;

	p = malloc (sizeof (MSParticipant_t));
	if (!p)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	memset (p, 0, sizeof (MSParticipant_t));
	p->handle = *handle = ++num_ids;
	memcpy (p->name, identity, length);
	memcpy (p->key, key, klength);
	p->key_length = klength;
	p->cloned = p;
	p->permissions_handle = ++num_permissions;
	p->updated_perm_handle = 0;
	LIST_ADD_HEAD (participants, *p);

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_access_remove_unchecked_participant (IdentityHandle_t id)
{
	MSParticipant_t *p;

	if (!(p = sp_access_get_participant (id)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (p->cloned != p) {
		log_printf (SEC_ID, 0, "SP_ACCESS_DB: This is not an unauthenticated participant [%d]!\r\n", id);
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	log_printf (SEC_ID, 0, "SP_ACCESS: remove unauthenticated participant [%d]\r\n", id);
	return (sp_access_remove_participant (id));
}

MSParticipant_t *sp_access_lookup_participant (const char *name)
{
	MSParticipant_t	*p;

	for (p = participants.head; p != (MSParticipant_t *) &participants; p = p->next)
		if (!strcmp (p->name, name))
			return (p);

	return (NULL);
}

MSParticipant_t *sp_access_lookup_participant_by_perm (unsigned char *permissions, size_t length)
{
	MSParticipant_t *pp;

	LIST_FOREACH (participants, pp) {
		if (pp->key_length == 16 && 
		    length == 16 &&
		    !memcmp (pp->key, permissions, 16)) {
			return (pp);
		}
	}
	return (NULL);
}

/* Topic functions */

void *sp_access_get_topic (TopicHandle_t handle, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSTopic_t       *dt;
	MSUTopic_t      *pt;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (NULL);
		else
			LIST_FOREACH (d->topics, dt) 
				if (dt->index == handle)
					return ((void *) dt);
		return (NULL);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (NULL);
		else
			LIST_FOREACH (p->topics, pt)
				if (pt->topic.index == handle)
					return ((void *) pt);
		return (NULL);
	default:
		return (NULL);
	}
}

int sp_access_get_topic_handle (char *name, MSMode_t mode, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSTopic_t       *dt;
	MSUTopic_t      *pt;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (-1);
		else
			LIST_FOREACH (d->topics, dt) 
				if ((dt->name == NULL && name == NULL) ||
				    (dt->name != NULL && name != NULL &&
				     !strcmp (name, dt->name)))
					if (dt->mode == mode)
						if (dt->index)
							return (dt->index);
		return (-1);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (-1);
		else
			LIST_FOREACH (p->topics, pt)
				if ((pt->topic.name == NULL && name == NULL) ||
				    (pt->topic.name != NULL && name != NULL && 
				     !strcmp (name, pt->topic.name)))
					if (pt->topic.mode == mode)
						if ( pt->topic.index)
							return (pt->topic.index);
		return (-1);
	default:
		return (-1);
	}
}

void *sp_access_add_topic (TopicHandle_t *handle, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSTopic_t       *dt;
	MSUTopic_t      *pt;
	
	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (NULL);
		else {
			if (!(dt = (MSTopic_t *) create_empty_topic (type)))
				return (NULL);

			/* Remove the standard topic assigned to a domain */
			if ( LIST_HEAD (d->topics) &&
			     LIST_HEAD (d->topics)->index == 0)
				sp_access_remove_topic (0, parent_handle, LIST_DOMAIN);

			/* Add the new topic */
			LIST_ADD_TAIL (d->topics, *dt);
			dt->index = ++topic_counter;
			*handle = topic_counter;
			d->ntopics++;

			return ((void *) dt);
		}
		return (NULL);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (NULL);
		else {
			if (!(pt = (MSUTopic_t *) create_empty_topic (type)))
				return (NULL);

			/* Remove the standard topic assigned to a domain */
			if ( LIST_HEAD (p->topics) &&
			     LIST_HEAD (p->topics)->topic.index == 0)
				sp_access_remove_topic (0, parent_handle, LIST_PARTICIPANT);

			LIST_ADD_TAIL (p->topics, *pt);
			pt->topic.index = ++topic_counter;
			*handle = topic_counter;
			p->ntopics++;

			return ((void *) pt);
		}
		return (NULL);
	default:
		return (NULL);
	}
}

DDS_ReturnCode_t sp_access_remove_topic (TopicHandle_t handle, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSTopic_t       *dt;
	MSUTopic_t      *pt;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (DDS_RETCODE_BAD_PARAMETER);
		else {
			if ((dt = (MSTopic_t *) sp_access_get_topic (handle, 
								      parent_handle, 
								      type))) {
				if (dt->name)
					free (dt->name);
				if (dt->fine_topic)
					free (dt->fine_topic);
				if (dt->fine_app_topic)
					free (dt->fine_app_topic);
				LIST_REMOVE (d->topics, *dt);
				free (dt);
				d->ntopics--;
				return (DDS_RETCODE_OK);
			}
		}
		return (DDS_RETCODE_BAD_PARAMETER);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (DDS_RETCODE_BAD_PARAMETER);
		else {
			if ((pt = (MSUTopic_t *) sp_access_get_topic (handle,
								      parent_handle,
								      type))) {
				if (pt->topic.name)
					free (pt->topic.name);
				if (pt->topic.fine_topic)
					free (pt->topic.fine_topic);
				if (pt->topic.fine_app_topic)
					free (pt->topic.fine_app_topic);
				LIST_REMOVE (p->topics, *pt);
				free (pt);
				p->ntopics--;
				return (DDS_RETCODE_OK);
			}
		}
		return (DDS_RETCODE_BAD_PARAMETER);
	default:
		return (DDS_RETCODE_BAD_PARAMETER);
	}
}

/* Partition functions */
void *sp_access_get_partition (PartitionHandle_t handle, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSPartition_t   *dt;
	MSUPartition_t  *pt;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (NULL);
		else
			LIST_FOREACH (d->partitions, dt) 
				if (dt->index == handle)
					return ((void *) dt);
		return (NULL);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (NULL);
		else
			LIST_FOREACH (p->partitions, pt)
				if (pt->partition.index == handle)
					return ((void *) pt);
		return (NULL);
	default:
		return (NULL);
	}
}

int sp_access_get_partition_handle (char *name, MSMode_t mode, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSPartition_t   *dt;
	MSUPartition_t  *pt;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (-1);
		else
			LIST_FOREACH (d->partitions, dt) 
				if ((dt->name == NULL && name == NULL) ||
				    (dt->name != NULL && name != NULL &&
				     !strcmp (name , dt->name)))
					if (dt->mode == mode)
						if (dt->index)
							return (dt->index);
		return (-1);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (-1);
		else
			LIST_FOREACH (p->partitions, pt)
				if ((pt->partition.name == NULL && name == NULL) ||
				    (pt->partition.name != NULL && name != NULL &&
				     !strcmp (name, pt->partition.name)))
					if (pt->partition.mode == mode)
						if (pt->partition.index)
							return (pt->partition.index);
		return (-1);
	default:
		return (-1);
	}
}

void *sp_access_add_partition (PartitionHandle_t *handle, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSPartition_t   *dp;
	MSUPartition_t  *pp;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (NULL);
		else {
			if (!(dp = (MSPartition_t *) create_empty_partition (type)))
			      return (NULL);

			/* Remove the standard topic assigned to a domain */
			if ( LIST_HEAD (d->partitions) &&
			     LIST_HEAD (d->partitions)->index == 0)
				sp_access_remove_partition (0, parent_handle, LIST_DOMAIN);

			LIST_ADD_TAIL (d->partitions, *dp);
			dp->index = ++partition_counter;
			*handle = partition_counter;
			d->npartitions++;
			return ((void *) dp);
		}
		return (NULL);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (NULL);
		else {
			if (!(pp = (MSUPartition_t *) create_empty_partition (type)))
				return (NULL);

			/* Remove the standard topic assigned to a domain */
			if ( LIST_HEAD (p->partitions) &&
			     LIST_HEAD (p->partitions)->partition.index == 0)
				sp_access_remove_partition (0, parent_handle, LIST_PARTICIPANT);

			LIST_ADD_TAIL (p->partitions, *pp);
			pp->partition.index = ++partition_counter;
			*handle = partition_counter;
			p->npartitions++;
			return ((void *) pp);
		}
		return (NULL);
	default:
		return (NULL);
	}
}

DDS_ReturnCode_t sp_access_remove_partition (PartitionHandle_t handle, unsigned parent_handle, ListTypes type)
{
	MSDomain_t      *d;
	MSParticipant_t *p;
	MSPartition_t   *dp;
	MSUPartition_t  *pp;

	switch (type) {
	case LIST_DOMAIN:
		if (!(d = sp_access_get_domain (parent_handle)))
			return (DDS_RETCODE_BAD_PARAMETER);
		else {
			if ((dp = (MSPartition_t *) sp_access_get_partition (handle, 
									     parent_handle, 
									     type))) {
				if (dp->name)
					free (dp->name);
				LIST_REMOVE (d->partitions, *dp);
				free (dp);
				d->npartitions--;
				return (DDS_RETCODE_OK);
			}
		}
		return (DDS_RETCODE_BAD_PARAMETER);
	case LIST_PARTICIPANT:
		if (!(p = sp_access_get_participant (parent_handle)))
			return (DDS_RETCODE_BAD_PARAMETER);
		else {
			if ((pp = (MSUPartition_t *) sp_access_get_partition (handle, 
									      parent_handle,
									      type))) {
				if (pp->partition.name)
					free (pp->partition.name);
				LIST_REMOVE (p->partitions, *pp);
				free (pp);
				p->npartitions--;
				return (DDS_RETCODE_OK);
			}
		}
		return (DDS_RETCODE_BAD_PARAMETER);
	default:
		return (DDS_RETCODE_BAD_PARAMETER);
	}
}


/* DB functions */

/* access db cleanup function */

DDS_ReturnCode_t sp_access_db_cleanup (void) 
{

	MSParticipant_t  *p, *pnext, *plast;
	MSDomain_t       *d, *dnext, *dlast;
	DDS_ReturnCode_t ret;

       	log_printf (SEC_ID, 0, "SP_ACCESS_DB: Cleanup of the access db\r\n");

	if (num_ids) {
		plast = participants.tail->next;
		for (p = participants.head; p != plast; p = pnext) {
			pnext = p->next;
			if ((ret = DDS_SP_remove_participant (p->handle)))
				return (ret);
		}
	}
	if (num_domains) {
		dlast = domains.tail->next;
		for (d = domains.head; d != dlast; d = dnext) {
			dnext = d->next;
			if ((ret = DDS_SP_remove_domain (d->handle)))
				return (ret);
		}
	}
	inits &= ~DB_INIT;
	db_init ();
	return (DDS_RETCODE_OK);
}

static char *dom_wcs (unsigned domain_id)
{
	static char	buffer [8];

	if (domain_id == ~0U) {
		buffer [0] = '*';
		buffer [1] = '\0';
	}
	else
		sprintf (buffer, "%u", domain_id);
	return (buffer);
}

static char *transp_s (unsigned transport)
{
	unsigned	loc, rem;
	static const char *sec_s [] = {
		"no", "DTLS", "TLS", "DTLS+TLS", "DDS", "DDS+DTLS", "DDS+TLS", "any"
	};
	static char buffer [32];

	loc = transport >> 16;
	rem = transport & 0xffff;
	sprintf (buffer, "Local=%s/Remote=%s", sec_s [loc], sec_s [rem]);
	return (buffer);
}

static char *acc_s (unsigned access)
{
	static char buffer [16];

	if (access == DS_UNCLASSIFIED)
		sprintf (buffer, "Unclassified");
	else if (access == DS_CONFIDENTIAL)
		sprintf (buffer, "Confidential");
	else if (access == DS_SECRET)
		sprintf (buffer, "Secret");
	else if (access == DS_TOP_SECRET)
		sprintf (buffer, "Top-Secret");
	else
		sprintf (buffer, "L%d", access);
	return (buffer);
}

static char *mode_s (unsigned mode)
{
	static char	buffer [6];
	unsigned	i;

	if (!mode)
		sprintf (buffer, "none");
	else if (mode == TA_ALL)
		sprintf (buffer, "*");
	else {
		i = 0;
		buffer [i++] = '{';
		if ((mode & TA_CREATE) != 0)
			buffer [i++] = 'C';
		if ((mode & TA_DELETE) != 0)
			buffer [i++] = 'D';
		if ((mode & TA_READ) != 0)
			buffer [i++] = 'R';
		if ((mode & TA_WRITE) != 0)
			buffer [i++] = 'W';
		buffer [i++] = '}';
		buffer [i] = '\0';
	}
	return (buffer);
}

static const char *encrypt_mode_s (int mode)
{
	static const char *mode_str [] = {
		"none",
		"HMAC_SHA1",
		"HMAC_SHA256",
		"AES128_HMAC_SHA1",
		"AES256_HMAC_SHA256"
	};

	if (mode <= 4)
		return (mode_str [mode]);
	else
		return ("???");
}

static char *encrypt_s (int submsg, int payload)
{
	static char buffer [32];

	if (submsg)
		sprintf (buffer, "submessage(%s)", encrypt_mode_s (submsg));
	else if (payload)
		sprintf (buffer, "payload(%s)", encrypt_mode_s (payload));
	else
		sprintf (buffer, "none");

	return (buffer);
}

static char *msg_encrypt_s (int message)
{
	static char buffer [32];

	if (message)
		sprintf (buffer, "message(%s)", encrypt_mode_s (message));
	else
		sprintf (buffer, "none");
	return (buffer);
}

static char *read_s (unsigned data [MAX_ID_HANDLES])
{
	int i;
	static char buffer [129];

	for (i = 0; i < MAX_ID_HANDLES; i++)
		if (data [i] != 0)
			sprintf (&buffer [2*i], "%d ", data [i]);
		else {
			buffer [2*i] = '\0';
			break;
		}
	return (buffer);
}

static char *write_s (unsigned data [MAX_ID_HANDLES])
{
	int i;
	static char buffer [129];

	for (i = 0; i < MAX_ID_HANDLES; i++)
		if (data [i] != 0)
			sprintf (&buffer [2*i], "%d ", data [i]);
		else {
			buffer [2*i] = '\0';
			break;
		}
	return (buffer);
}

DDS_ReturnCode_t DDS_SP_access_db_dump (void)
{
	char           name [256];
	MSMode_t       mode;
	DDS_DomainId_t domId;
	MSAccess_t     access;
	int            exclusive;
	int            controlled;
	int            msg_encrypt;
	int	       disc_enc;
	int            submsg_enc;
	int            payload_enc;
	uint32_t       transport;
	int            blacklist;
	
	MSParticipant_t *p;
	MSDomain_t *d;
	MSUTopic_t *tp;
	MSTopic_t  *dtp;
	MSUPartition_t *pt;
	MSPartition_t  *dpt;
	DDS_ReturnCode_t ret;

	if (!num_ids && ! num_domains) {
		dbg_printf ("The security policy database is empty!\r\n");
		return (DDS_RETCODE_OK);
	}
	dbg_printf ("Security policy database rules: \r\n");

	/* Topic and partition values */
	if (num_ids) {
		LIST_FOREACH (participants, p) {
			if ((ret = DDS_SP_get_participant_access (p->handle, &name [0], &access, &blacklist)))
				return (ret);

			else {
				if (p->cloned)
					dbg_printf ("%d) Participant [%d]: '%s' | access: %s | blacklist: %d | clone from %d\r\n", 
						    p->handle, p->permissions_handle, name, acc_s (access), blacklist, p->cloned->handle);
				else
					dbg_printf ("%d) Participant: '%s' | access: %s | blacklist: %d\r\n", 
						    p->handle, name, acc_s (access), blacklist);
				if (p->topics.head != NULL) {
					LIST_FOREACH (p->topics, tp) {
						if ((ret = DDS_SP_get_topic_access (p->handle, 0, tp->topic.index, 
										    &name [0], &mode, &controlled, &disc_enc,
										    &submsg_enc, &payload_enc, &domId, &blacklist)))
							return (ret);

						/* print this topic data */
						dbg_printf ("|---> Topic: %s | mode: %s | controlled: %d | secure_disc: %d | encrypt: %s | domId: %s | blacklist: %d\r\n",
							    name, mode_s (mode), controlled, disc_enc, encrypt_s (submsg_enc, payload_enc), dom_wcs (domId), blacklist);
						if (tp->topic.fine_topic)
							dbg_printf ("  |---> Fine grained: read: %s | write: %s\r\n", 
								    read_s (tp->topic.fine_topic->read), 
								    write_s (tp->topic.fine_topic->write));
						if (tp->topic.fine_app_topic)
							dbg_printf ("  |---> Fine grained app: read: %s | write: %s\r\n",
								    read_s (tp->topic.fine_app_topic->read),
								    write_s (tp->topic.fine_app_topic->write));
					}
				}
				if (p->partitions.head != NULL) {
					LIST_FOREACH (p->partitions, pt) {
						if ((ret = DDS_SP_get_partition_access (p->handle, 0, 
											 pt->partition.index,
											 &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this partition data */
						dbg_printf ("|---> Partition: %s | mode: %s | domId: %s | blacklist: %d\r\n",
							    name, mode_s (mode), dom_wcs (domId), blacklist);
					}
				}
			}
		}
	}

	if (num_domains) {
		LIST_FOREACH (domains, d) {
			if ((ret = DDS_SP_get_domain_access (d->handle, &domId, &access, &exclusive, &controlled, &msg_encrypt, &transport, &blacklist)))
				return (ret);
			else {
				/* Print the domain data */
				dbg_printf ("%d) Domain %s: access: %s | exclusive: %d | controlled: %d | encrypt: %s | secure transport: %s | blacklist: %d\r\n", 
					    d->handle, dom_wcs (domId), acc_s (access), exclusive, controlled, msg_encrypt_s (msg_encrypt), transp_s (transport), blacklist);
				if (d->topics.head != NULL) {
					LIST_FOREACH (d->topics, dtp) {
						if ((ret = DDS_SP_get_topic_access (0, d->handle,
										     dtp->index, 
										     &name [0], &mode, &controlled, &disc_enc,
										     &submsg_enc, &payload_enc, &domId, &blacklist)))
							return (ret);

						/* print this topic data */
						dbg_printf ("|---> Topic: %s | mode: %s | controlled: %d | secure_disc: %d | encrypt: %s | domId: %s | blacklist: %d\r\n", 
							    name, mode_s (mode), controlled, disc_enc, encrypt_s (submsg_enc, payload_enc), dom_wcs (domId), blacklist);
					}
				}
				if (d->partitions.head != NULL) {
					LIST_FOREACH (d->partitions, dpt) {
						if ((ret = DDS_SP_get_partition_access (0, d->handle, 
										     dpt->index,
										     &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this partition data */
						dbg_printf ("|---> Partition: %s | mode: %s | domId: %s | blacklist: %d\r\n",
							    name, mode_s (mode), dom_wcs (domId), blacklist);
					}
				}
			}
		}
	}
	return (DDS_RETCODE_OK);
}
