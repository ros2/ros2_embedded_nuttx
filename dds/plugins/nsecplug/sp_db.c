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

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sys.h"
#include "log.h"
#include "dlist.h"
#include "error.h"
#include "md5.h"
#include "nmatch.h"
#include "msecplug/msecplug.h"
#include "../security/engine_fs.h"
#include <openssl/x509v3.h>
#include <openssl/rsa.h>
#include "thread.h"
#include "strseq.h"
#include "uqos.h"

#ifdef MSECPLUG_WITH_SECXML
#include "xmlparse.h"
#endif

static DDS_ReturnCode_t cleanup_stale_db (void);
static DDS_ReturnCode_t rematch_cloned_participants (void);
static DDS_ReturnCode_t reorder_participants (void);

unsigned local_dom_part_handle;
unsigned input_counter = 1;

lock_t sp_lock;

int inits = 0;

#define ENGINE_INIT 1
#define DB_INIT     2
#define LOCK_INIT   4

static BIO *in;

/* these variables are to make sure every topic and partition have a unique index.
   The value does not matter, as long as it is unique*/
static unsigned topic_counter = 0;
static unsigned partition_counter = 0;

static DDS_ReturnCode_t msp_perm_pars (PermissionsHandle_t perm,
				       MSDomain_t          **p,
				       MSParticipant_t     **pp);

static sp_extra_authentication_check_fct extra_authentication_check = NULL;
static msp_auth_revoke_listener_fct on_revoke_identity = NULL;
static msp_acc_revoke_listener_fct on_revoke_permissions = NULL;

typedef struct engineList_st {
	char	*name;
	int	index;
	struct engineList_st *next;
} EngineList;

EngineList *engineListTop;

void DDS_SP_set_extra_authentication_check (sp_extra_authentication_check_fct f)
{
	extra_authentication_check = f;
}

DDS_ReturnCode_t msp_set_auth_listener (msp_auth_revoke_listener_fct f)
{
	if (!f)
		return (DDS_RETCODE_BAD_PARAMETER);

	on_revoke_identity = f;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t msp_set_acc_listener (msp_acc_revoke_listener_fct f)
{
	if (!f)
		return (DDS_RETCODE_BAD_PARAMETER);

	on_revoke_permissions = f;
	return (DDS_RETCODE_OK);
}

static int has_wildcards (const char *data) 
{
	if (!data)
		return (0);
	if (strchr (data, '*') || strchr (data, '?'))
		return (1);
	return (0);
}

static void engine_init (void)
{
	if (inits & ENGINE_INIT)
		return;

      	engine_counter = 0;
	engineListTop = NULL;
	inits |= ENGINE_INIT;
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
}

static void lock_init (void)
{
	if (inits & LOCK_INIT)
		return;

	lock_init_nr (sp_lock, "Security Plugin Lock");
	inits |= LOCK_INIT;
}

static void msp_init (void)
{
	if ((inits & ENGINE_INIT) && (inits & DB_INIT) && (inits & LOCK_INIT))
		return;

	engine_init ();
	db_init ();
	lock_init ();
}

/* This should be called before every change made to the database,
   even when removing the complete database */

DDS_ReturnCode_t DDS_SP_update_start (void)
{
	msp_init ();
	input_counter = 1;

	/* Take access db lock */
	lock_init ();
	lock_take (sp_lock);
	return (DDS_RETCODE_OK);
}

/* This should be called when every change to the database is done */
DDS_ReturnCode_t DDS_SP_update_done (void)
{
	/* Call listener functions if changes have happened */
	reorder_participants ();
	cleanup_stale_db ();
	rematch_cloned_participants ();
	
	/* release access db lock */
	lock_release (sp_lock);
	
	/* lock_destroy (sp_lock); */
	
	/* This should actually be done in some other function */
	/* inits &= ~LOCK_INIT;  */
	return (DDS_RETCODE_OK);
}

/* Domain functions */

DomainHandle_t DDS_SP_add_domain (void)
{
	MSDomain_t	*d;
	MSTopic_t	*tp;
	MSPartition_t	*pp;

	if (!domains.head)
		msp_init ();

	/* insert a default, allow all policy. */
	/* log_printf (SEC_ID, 0, "MSP: Creating a default 'allow none' policy for all domains\r\n"); */
	d = calloc (1, sizeof (MSDomain_t));
	if (!d)
		fatal_printf ("out-of-memory for domain policy!\r\n");
	
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
	if (!(tp = calloc (1, sizeof (MSTopic_t))))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (d->topics, *tp);
	tp->name = 0; /* name == '*' */
	tp->mode = TA_ALL;
	tp->index = 0;
	tp->blacklist = 0;
	d->ntopics++;

	/* allow all partitions in the domain */
	LIST_INIT (d->partitions);
	if (!(pp = calloc (1, sizeof (MSPartition_t))))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (d->partitions, *pp);
	pp->name = NULL; /* name == '*' */
	pp->mode = TA_ALL;
	pp->index = 0;
	pp->blacklist = 0;
	d->npartitions++;
	
	LIST_ADD_TAIL (domains, *d);
	d->handle = ++num_domains;
	domain_handles [d->handle] = d;
	
	return (d->handle);
}

DDS_ReturnCode_t DDS_SP_remove_domain (DomainHandle_t domain_handle)
{
	MSDomain_t       *d;
	unsigned         i, ntopics, npartitions;
	DDS_ReturnCode_t ret;

	/* log_printf (SEC_ID, 0, "MSP: Remove domain\r\n"); */

	if (!(d = domain_handles [domain_handle]))
		return (DDS_RETCODE_BAD_PARAMETER);

	ntopics = d->ntopics;
	for (i = 0; i < ntopics; i++)
		if ((ret = DDS_SP_remove_topic (0, domain_handle,
						LIST_HEAD (d->topics)->index)))
			return (ret);

	npartitions = d->npartitions;
	for (i = 0; i < npartitions; i++)
		if ((ret = DDS_SP_remove_partition (0, domain_handle, 
						    LIST_HEAD (d->partitions)->index)))
			return (ret);

	LIST_REMOVE (domains, *d);
	free (d);

	/*if it is the last one in the list, we can reuse the memory*/
	/*perhaps later we should make it possible to realign the array,
	 when we remove an element*/
	if (domain_handle == num_domains)
		num_domains--;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_set_domain_access (DomainHandle_t domain_handle,
					   DDS_DomainId_t domain_id,
					   MSAccess_t     access,
					   int            exclusive,
					   uint32_t       transport,
					   int            blacklist)
{
	MSDomain_t     *d;
	
	/* log_printf (SEC_ID, 0, "MSP: Set domain access\r\n"); */

	if (!(d = domain_handles [domain_handle]))
		return (DDS_RETCODE_BAD_PARAMETER);

	d->domain_id = domain_id;
	d->access = access;
	d->exclusive = exclusive;
	d->transport = transport;
	d->blacklist = blacklist;
	d->refreshed = 1;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_domain_access (DomainHandle_t domain_handle,
					   DDS_DomainId_t *domain_id,
					   MSAccess_t     *access,
					   int            *exclusive,
					   uint32_t       *transport,
					   int            *blacklist)
{
	MSDomain_t *d;

	if (!(d = domain_handles [domain_handle]))
	    return (DDS_RETCODE_BAD_PARAMETER);
	*domain_id = d->domain_id;
	*access = d->access;
	*exclusive = d->exclusive;
	*transport = d->transport;
	*blacklist = d->blacklist;

	return (DDS_RETCODE_OK);
}

int DDS_SP_get_domain_handle (DDS_DomainId_t domain_id)
{
	unsigned i;

	/* log_printf (SEC_ID, 0, "MSP: Get domain handle\r\n"); */

	for (i = 1; i <= num_domains; i++)
		if (domain_id == domain_handles [i]->domain_id)
			return (domain_handles [i]->handle);
	return (-1);
}

/* Participant functions */

ParticipantHandle_t DDS_SP_add_participant (void)
{
	MSParticipant_t	*p;
	MSUTopic_t	*tp = NULL;
	MSUPartition_t	*pp = NULL;

	msp_init ();

	/* log_printf (SEC_ID, 0, "MSP: Creating a default 'allow all' policy for participant\r\n"); */

	/* insert a default, allow all policy */
	p = calloc (1, sizeof (MSParticipant_t));

	if (!p)
		fatal_printf ("out-of-memory for participant (user) policy!\r\n");
	memcpy (p->name, "*", strlen ("*") + 1);
	p->access = DS_SECRET;
	p->blacklist = 0;

	LIST_INIT (p->topics);
	if (!(tp = calloc (1, sizeof (MSUTopic_t))))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (p->topics, *tp);
	tp->id = ~0; /* all domains */
	tp->topic.name = NULL; /* name == '*' */
	tp->topic.mode = TA_ALL;
	tp->topic.index = 0;
	tp->topic.blacklist = 0;
	p->ntopics++;

	LIST_INIT (p->partitions);
	if (!(pp = calloc (1, sizeof (MSUPartition_t))))
		fatal_printf ("Out of memory!\r\n");
	LIST_ADD_TAIL (p->partitions, *pp);
	pp->id = ~0; /* all domains */
	pp->partition.name = NULL; /* name == '*' */
	pp->partition.mode = TA_ALL;
	pp->partition.index = 0;
	pp->partition.blacklist = 0;
	p->npartitions++;
	
	LIST_ADD_TAIL (participants, *p);
	p->handle = ++num_ids;
	id_handles [p->handle] = p;
	
	return (p->handle);
}

DDS_ReturnCode_t DDS_SP_remove_participant (ParticipantHandle_t participant_handle) 
{
	MSParticipant_t  *p;
	unsigned         i, ntopics, npartitions;
	DDS_ReturnCode_t ret;
	
	/* log_printf (SEC_ID, 0, "MSP: Remove participant\r\n"); */

	if (!(p = id_handles [participant_handle]))
		return (DDS_RETCODE_BAD_PARAMETER);

	ntopics = p->ntopics;
	for (i = 0; i < ntopics; i++) 
		if ((ret = DDS_SP_remove_topic (participant_handle, 0, 
					     LIST_HEAD (p->topics)->topic.index)))
			return (ret);

	npartitions = p->npartitions;
	for (i = 0;  i < npartitions; i++)
		if ((ret = DDS_SP_remove_partition (participant_handle, 0, 
						 LIST_HEAD (p->partitions)->partition.index)))
			return (ret);
	/* if the credential is of the DDS_SSL_BASED kind
	   we need to remove the certificate stack as well */
	if (p->cloned && p->credentials && p->credentials->credentialKind == DDS_SSL_BASED) {
		sk_X509_pop_free (p->credentials->info.sslData.certificate_list, X509_free);
		EVP_PKEY_free (p->credentials->info.sslData.private_key);
	}

	if (p->cloned) {
		/* DDS_revoke_participant (local_dom_part_handle, p->part_handle); */
		if (p->credentials) {
			free (p->credentials);
			p->credentials = NULL;
		}
	}
	LIST_REMOVE (participants, *p);
	free (p);
	id_handles [participant_handle] = NULL;

	/*if it is the last one in the list, we can reuse the memory*/
	if (participant_handle == num_ids)
		num_ids--;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_set_participant_access (ParticipantHandle_t participant_handle, 
						char                *new_name, 
						MSAccess_t          access,
						int                 blacklist)
{
	MSParticipant_t     *p;
	
	/* log_printf (SEC_ID, 0, "MSP: Set participant access\r\n"); */

	if (!(p = id_handles [participant_handle]))
		return (DDS_RETCODE_BAD_PARAMETER);

	memcpy (p->name, new_name, strlen (new_name) + 1);
	p->access = access;
	p->blacklist = blacklist;
	p->refreshed = 1;
	p->input_number = input_counter ++;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_participant_access (ParticipantHandle_t participant_handle,
						char                *name, 
						MSAccess_t          *access,
						int                 *blacklist)
{	
	MSParticipant_t *p;

	if (!(p = id_handles [participant_handle]))
		return (DDS_RETCODE_BAD_PARAMETER);
	strcpy (name, p->name);
	*access = p->access;
	*blacklist = p->blacklist;

	return (DDS_RETCODE_OK);
}

int DDS_SP_get_participant_handle (char *name)
{
	unsigned i;
	
	/* log_printf (SEC_ID, 0, "MSP: Get participant handle\r\n"); */

	/* Possible problem: what to do with cloned participants, their
	   handle is not returned, so possible fault entries can occur
	   when this handle is used to set new data */
	
	for (i = 1; i <= num_ids; i++)
		if (id_handles [i] != NULL)
			if (!strcmp(name, id_handles [i]->name))
				return (id_handles [i]->handle);

	return (-1);
}

/* Topic Functions */

TopicHandle_t DDS_SP_add_topic (ParticipantHandle_t participant_handle, 
				DomainHandle_t      domain_handle)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	/* log_printf (SEC_ID, 0, "MSP: Add default 'allow all' policy for topic\r\n"); */

	if (participant_handle > 0) {
		/*ADD this topic for participant*/
		if (!(p = id_handles [participant_handle]))
			return (-1);

		if ( LIST_HEAD (p->topics) &&
		     LIST_HEAD (p->topics)->topic.index == 0)
			DDS_SP_remove_topic ( participant_handle, 0, 0);

		if (!(tp = calloc (1, sizeof (MSUTopic_t))))
			fatal_printf ("Out of memory!\r\n");
		LIST_ADD_TAIL (p->topics, *tp);
		tp->id = ~0; /* all domains */
		tp->topic.name = NULL; /* name == '*' */
		tp->topic.mode = TA_ALL;
	        tp->topic.index = ++topic_counter;
		tp->topic.blacklist = 0;
		p->ntopics++;

		return (tp->topic.index);
	}
	else if (domain_handle > 0) {
		/*ADD this topic for domain*/

		if (!(d = domain_handles [domain_handle]))
			return (-1);

		if (LIST_HEAD (d->topics) &&
		    LIST_HEAD (d->topics)->index == 0)
			DDS_SP_remove_topic (0, domain_handle, 0);

		if (!(dtp = calloc (1, sizeof (MSTopic_t))))
			fatal_printf ("Out of memory!\r\n");
		LIST_ADD_TAIL (d->topics, *dtp);
		dtp->name = NULL; /* name == '*' */
		dtp->mode = TA_ALL;
		dtp->index = ++topic_counter;
		dtp->blacklist = 0;
		d->ntopics++;

		return (dtp->index);
	}
		
	return (-1);
}

DDS_ReturnCode_t DDS_SP_remove_topic (ParticipantHandle_t participant_handle,
				      DomainHandle_t      domain_handle,
				      TopicHandle_t       topic_id)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	/* log_printf (SEC_ID, 0, "MSP: Remove topic access\r\n"); */

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		LIST_FOREACH (p->topics, tp)
			if (tp->topic.index == topic_id) {
				free(tp->topic.name);
				LIST_REMOVE (p->topics, *tp);
				free (tp);
				p->ntopics--;
				break;
			}
	} 
	else if (domain_handle > 0) {
		if (!(d = domain_handles [domain_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);

		LIST_FOREACH (d->topics, dtp) {
			if (dtp->index == topic_id) {
				free (dtp->name);
				LIST_REMOVE (d->topics, *dtp);
				free (dtp);
				d->ntopics--;
				break;
			}
		}
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_set_topic_access (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  TopicHandle_t       topic_id, 
					  char                *name,
					  MSMode_t            mode,
					  DDS_DomainId_t      id,
					  int                 blacklist)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	/* log_printf (SEC_ID, 0, "MSP: Set topic access\r\n"); */
	
	/* We can add an extra check to see if the topic rule already exists */
	/* if (DDS_SP_get_topic_handle (participant_handle, domain_handle, name) != -1)
	   return (DDS_RETCODE_BAD_PARAMETER);
	*/

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		LIST_FOREACH (p->topics, tp) 
			if (tp->topic.index == topic_id)
				goto found1;

		return (DDS_RETCODE_BAD_PARAMETER);
 found1:
		tp->id = id;
		if (name) {
			if (!(tp->topic.name = malloc (strlen ((char *) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (tp->topic.name, name);
		} else 
			tp->topic.name = NULL;
				
		tp->topic.mode = mode;
		tp->topic.blacklist = blacklist;
		tp->topic.refreshed = 1;
		
		/* when a topic for a participant is changed, we must update the participant as refreshed */
		p->refreshed = 1;

		/* if there are no wildcards inside the name
		   then add the topic rule to the head of the list */
		if (!has_wildcards (tp->topic.name)) {
			LIST_REMOVE (p->topics, *tp);
			LIST_ADD_HEAD (p->topics, *tp);
		}
	}
	else if (domain_handle > 0) {
		if (!(d = domain_handles [domain_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);

		LIST_FOREACH (d->topics, dtp)
			if (dtp->index == topic_id)
				goto found2;
		
		return (DDS_RETCODE_BAD_PARAMETER);
 found2:
		if (name) {
			if (!(dtp->name = malloc (strlen ((char *) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (dtp->name, name);
		} else
			dtp->name = NULL;
		
		dtp->mode = mode;
		dtp->blacklist = blacklist;
		dtp->refreshed = 1;

		/* when a topic for a domain is changed, we must update the domain as refreshed */
		d->refreshed = 1;

		if (!has_wildcards (dtp->name)) {
			LIST_REMOVE (d->topics, *dtp);
			LIST_ADD_HEAD (d->topics, *dtp);
		}
	}

	/* Data changed, so we need to rematch */

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_topic_access (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  TopicHandle_t       topic_id, 
					  char                *name,
					  MSMode_t            *mode,
					  DDS_DomainId_t      *id,
					  int                 *blacklist)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	if (participant_handle > 0) {
		
		if (!(p = id_handles [participant_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		LIST_FOREACH (p->topics, tp)
			if (tp->topic.index == topic_id)
			    goto found1;

		return (DDS_RETCODE_BAD_PARAMETER);
 found1:
 		*id = tp->id;
		if (tp->topic.name)
			strcpy (name, tp->topic.name);
		else
			strcpy (name, "*");
		*mode = tp->topic.mode;
		*blacklist = tp->topic.blacklist;
	}
	else if (domain_handle > 0) {

		if (!(d = domain_handles [domain_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);

		LIST_FOREACH (d->topics, dtp) 
			if (dtp->index == topic_id)
				goto found2;

		return (DDS_RETCODE_BAD_PARAMETER);
 found2:
		if (dtp->name)
			strcpy (name, dtp->name);
		else
			strcpy (name, "*");
		*mode = dtp->mode;
		*blacklist = dtp->blacklist;
	}

	return (DDS_RETCODE_OK);
}

int DDS_SP_get_topic_handle (ParticipantHandle_t participant_handle,
			     DomainHandle_t      domain_handle,
			     char                *name,
			     MSMode_t            mode)
{
	MSParticipant_t *p;
	MSDomain_t      *d;
	MSUTopic_t      *tp;
	MSTopic_t       *dtp;

	/* log_printf (SEC_ID, 0, "MSP: Get topic handle\r\n"); */

	if (name)
		if (!strcmp (name, "*"))
			name = NULL;

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (-1);
		
		LIST_FOREACH (p->topics, tp) {
			if ((tp->topic.name == NULL && name == NULL) ||
			    (tp->topic.name != NULL && name != NULL && 
			     !strcmp (name, tp->topic.name)))
				if (tp->topic.mode == mode)
					if ( tp->topic.index)
						return (tp->topic.index);
		}
	}
	else if (domain_handle > 0) {
		if (!(d = domain_handles [domain_handle]))
			return (-1);

		LIST_FOREACH (d->topics, dtp) {
			if ((dtp->name == NULL && name == NULL) ||
			    (dtp->name != NULL && name != NULL &&
			     !strcmp (name, dtp->name)))
				if (dtp->mode == mode)
					if (dtp->index)
						return (dtp->index);
		}
	}

	return (-1);
}

/* Partition Functions */

PartitionHandle_t DDS_SP_add_partition (ParticipantHandle_t participant_handle, 
					DomainHandle_t      domain_handle)
{
   	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUPartition_t	*pp = NULL;
	MSPartition_t	*dpp = NULL;

	/* log_printf (SEC_ID, 0, "MSP: Add default 'allow all' policy for partition\r\n"); */

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (-1);

		if ( LIST_HEAD (p->partitions) &&
		     LIST_HEAD (p->partitions)->partition.index == 0)
			DDS_SP_remove_partition ( participant_handle, 0, 0);

		if (!(pp = calloc (1, sizeof (MSUPartition_t))))
			fatal_printf ("Out of memory!\r\n");

		LIST_ADD_TAIL (p->partitions, *pp);
		pp->id = ~0; /* all domains */
		pp->partition.name = NULL; /* name == '*' */
		pp->partition.mode = TA_ALL;
		pp->partition.index = ++partition_counter;
		pp->partition.blacklist = 0;
		p->npartitions++;

		return (pp->partition.index);
	}
	else if (domain_handle > 0) {
		if (!(d = domain_handles [domain_handle]))
			return (-1);
	       
		if ( LIST_HEAD (d->partitions) &&
		     LIST_HEAD (d->partitions)->index == 0)
			DDS_SP_remove_partition ( 0, domain_handle, 0);

		if (!(dpp = calloc (1, sizeof (MSPartition_t))))
			fatal_printf ("Out of memory!\r\n");

		LIST_ADD_TAIL (d->partitions, *dpp);
		dpp->name = NULL; /* name == '*' */
		dpp->mode = TA_ALL;
		dpp->index = ++partition_counter;
		dpp->blacklist = 0;
		d->npartitions++;

		return (dpp->index);
	}
	return (-1);
}

DDS_ReturnCode_t DDS_SP_remove_partition (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  PartitionHandle_t   partition_id)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUPartition_t	*pp = NULL;
	MSPartition_t	*dpp = NULL;

	/* log_printf (SEC_ID, 0, "MSP: Remove partition access\r\n"); */

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		LIST_FOREACH (p->partitions, pp) {
			if (pp->partition.index == partition_id) {
				free(pp->partition.name);
				LIST_REMOVE (p->partitions, *pp);
				free (pp);
				p->npartitions--;
				break;
			}
		}
	} 

	else if (domain_handle > 0) {
		if(!(d = domain_handles [domain_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);


		LIST_FOREACH (d->partitions, dpp) {
			if (dpp->index == partition_id) {
				free (dpp->name);
				LIST_REMOVE (d->partitions, *dpp);
				free (dpp);
				d->npartitions--;
				break;
			}
		}
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_set_partition_access (ParticipantHandle_t participant_handle,
					      DomainHandle_t      domain_handle,
					      PartitionHandle_t   partition_id,
					      char                *name,
					      MSMode_t            mode,
					      DDS_DomainId_t      id,
					      int                 blacklist)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUPartition_t	*pp = NULL;
	MSPartition_t	*dpp = NULL;

	/* We can add an extra check to see if the topic rule already exists */
	/* if (DDS_SP_get_partition_handle (participant_handle, domain_handle, name) != -1)
	   return (DDS_RETCODE_BAD_PARAMETER);
	*/

	/*	log_printf (SEC_ID, 0, "MSP: Set partition access\r\n"); */

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);

		LIST_FOREACH (p->partitions, pp)
			if (pp->partition.index == partition_id)
				goto found1;

		return (DDS_RETCODE_BAD_PARAMETER);
 found1:
		pp->id = id;
		
		if (name) {
			if (!(pp->partition.name = malloc (strlen ((char *) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (pp->partition.name, name);
		} else
			pp->partition.name = NULL;

		pp->partition.mode = mode;
		pp->partition.blacklist = blacklist;
		pp->partition.refreshed = 1;

		/* refresh partitipant as well */
		p->refreshed = 1;

		if (!has_wildcards (pp->partition.name)) {
			LIST_REMOVE (p->partitions, *pp);
			LIST_ADD_HEAD (p->partitions, *pp);
		}
	}
	else if (domain_handle > 0) {
		if(!(d = domain_handles [domain_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);

		LIST_FOREACH (d->partitions, dpp)
			if (dpp->index == partition_id)
				goto found2;

		return (DDS_RETCODE_BAD_PARAMETER);
 found2:
		if (name) {
			if (!(dpp->name = malloc (strlen ((char*) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (dpp->name, name);
		} else
			dpp->name = NULL;

		dpp->mode = mode;
		dpp->blacklist = blacklist;
		dpp->refreshed = 1;

		d->refreshed = 1;

		/* if the name has no wildcards, add it to the head of the list */
		if (!has_wildcards (dpp->name)) {
			LIST_REMOVE (d->partitions, *dpp);
			LIST_ADD_HEAD (d->partitions, *dpp);
		}
	}

	/* Data changed so we need to rematch */

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_partition_access (ParticipantHandle_t participant_handle,
					      DomainHandle_t      domain_handle,
					      PartitionHandle_t   partition_id,
					      char                *name,
					      MSMode_t            *mode,
					      DDS_DomainId_t      *id,
					      int                 *blacklist)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUPartition_t	*pp = NULL;
	MSPartition_t	*dpp = NULL;

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);

		LIST_FOREACH (p->partitions, pp)
			if (pp->partition.index == partition_id)
				goto found1;

		return (DDS_RETCODE_BAD_PARAMETER);
 found1:
		*id = pp->id;
		if (pp->partition.name)
			strcpy (name, pp->partition.name);
		else
			strcpy (name, "*");
		*mode = pp->partition.mode;
		*blacklist = pp->partition.blacklist;
	}
	else if (domain_handle > 0) {
		if (!(d = domain_handles [domain_handle]))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		LIST_FOREACH (d->partitions, dpp)
			if (dpp->index == partition_id)
				goto found2;

		return (DDS_RETCODE_BAD_PARAMETER);
 found2:
		if (dpp->name)
			strcpy (name, dpp->name);
		else
			strcpy (name, "*");
		*mode = dpp->mode;
		*blacklist = dpp->blacklist;
	}
	return (DDS_RETCODE_OK);
}

int DDS_SP_get_partition_handle (ParticipantHandle_t participant_handle,
				 DomainHandle_t      domain_handle,
				 char                *name,
				 MSMode_t            mode)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUPartition_t  *pp;
	MSPartition_t   *dpp;
	
	if (name)
		if (!strcmp (name, "*"))
			name = NULL;

	if (participant_handle > 0) {
		if (!(p = id_handles [participant_handle]))
			return (-1);
		LIST_FOREACH (p->partitions, pp)
			if ((pp->partition.name == NULL && name == NULL) ||
			    (pp->partition.name != NULL && name != NULL &&
			     !strcmp (name, pp->partition.name)))
				if (pp->partition.mode == mode)
					if (pp->partition.index)
						return (pp->partition.index);
	}
	else if (domain_handle > 0) {
		if (!(d = domain_handles [domain_handle]))
			return (-1);
		LIST_FOREACH (d->partitions, dpp)
			if ((dpp->name == NULL && name == NULL) ||
			    (dpp->name != NULL && name != NULL &&
			     !strcmp (name , dpp->name)))
				if (dpp->mode == mode)
					if (dpp->index)
						return (dpp->index);
	}
	return (-1);
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

static DDS_ReturnCode_t cleanup_stale_participant (MSParticipant_t *participant)
{
	DDS_ReturnCode_t ret;	
	MSUTopic_t       *t, *tnext, *tlast;
	MSUPartition_t   *p, *pnext, *plast;

	tlast = participant->topics.tail->next;
	for (t = participant->topics.head; t != tlast; t = tnext) {
		tnext = t->next;
		if (t->topic.refreshed == 0) {
			if (t->topic.index)
				if ((ret = DDS_SP_remove_topic (participant->handle, 0, 
								t->topic.index)))
					return (ret);
		} else
			t->topic.refreshed = 0;
	}

	plast = participant->partitions.tail->next;
	for (p = participant->partitions.head; p != plast; p = pnext) {
		pnext = p->next;
		if (p->partition.refreshed == 0) {
			if (p->partition.index)
				if ((ret = DDS_SP_remove_partition (participant->handle, 0, 
								    p->partition.index)))
					return (ret);
		} else
			p->partition.refreshed = 0;
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t cleanup_stale_domain (MSDomain_t *d)
{
	DDS_ReturnCode_t ret;
	MSTopic_t        *t, *tnext, *tlast;
	MSPartition_t    *p, *pnext, *plast;

	tlast = d->topics.tail->next;
	for (t = d->topics.head; t != tlast; t = tnext) {
		tnext = t->next;
		if (t->refreshed == 0) {
			if (t->index)
				if ((ret = DDS_SP_remove_topic (0, d->handle,
								t->index)))
					return (ret);
		} else
			t->refreshed = 0;
	}

	plast = d->partitions.tail->next;
	for (p = d->partitions.head; p != plast; p = pnext) {
		pnext = p->next;
		if (p->refreshed == 0) {
			if (p->index)
				if ((ret = DDS_SP_remove_partition (0, d->handle, 
								    p->index)))
					return (ret);
		} else
			p->refreshed = 0;
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t cleanup_stale_db (void)
{
	MSParticipant_t  *p, *pnext, *plast;
	MSDomain_t       *d, *dnext, *dlast;
	DDS_ReturnCode_t ret;

       	log_printf (SEC_ID, 0, "MSP: Cleanup stale entries in the db\r\n");

	/* This might give problems when the parent is gone, but the clone has a ref to the parent */
	if (num_ids) {
		plast = participants.tail->next;
		for (p = participants.head; p != plast; p = pnext) {
			pnext = p->next;
			if (p->refreshed == 0) {
				if (p->cloned) {
					if (p->cloned->refreshed == 0)
						if ((ret = DDS_SP_remove_participant (p->handle)))
							return (ret);
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
	if (num_domains) {
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
		if (pclone->cloned) {
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

/* access db cleanup function */

DDS_ReturnCode_t DDS_SP_access_db_cleanup (void) 
{

	MSParticipant_t  *p, *pnext, *plast;
	MSDomain_t       *d, *dnext, *dlast;
	DDS_ReturnCode_t ret;

       	log_printf (SEC_ID, 0, "MSP: Cleanup of the access db\r\n");

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
		"none", "DTLS", "TLS", "DDS"
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

DDS_ReturnCode_t access_db_dump (void)
{
	char           name [256];
	MSMode_t       mode;
	DDS_DomainId_t domId;
	MSAccess_t     access;
	int            exclusive;
	uint32_t       transport;
	int            blacklist;
	
	MSParticipant_t *p;
	MSDomain_t *d;
	MSUTopic_t *tp;
	MSTopic_t  *dtp;
	MSUPartition_t *pt;
	MSPartition_t  *dpt;
	DDS_ReturnCode_t ret;

	/* Topic and partition values */

	if (!num_ids && ! num_domains) {
		dbg_printf ("The security policy database is empty");
		return (DDS_RETCODE_OK);
	}
	dbg_printf ("Security policy database rules: \r\n");
	if (num_ids) {
		LIST_FOREACH (participants, p) {
			if ((ret = DDS_SP_get_participant_access (p->handle, &name [0], &access, &blacklist)))
				return (ret);
			
			else {
				if (p->cloned)
					dbg_printf ("%d) Participant: %s | access: %s | blacklist: %d | clone from %d\r\n", 
						    p->handle, name, acc_s (access), blacklist, p->cloned->handle);
				else
					dbg_printf ("%d) Participant: %s | access: %s | blacklist: %d\r\n", 
						    p->handle, name, acc (access), blacklist);
				if (p->topics.head != NULL) {
					LIST_FOREACH (p->topics, tp) {
						if ((ret = DDS_SP_get_topic_access (p->handle, 0, tp->topic.index, 
										     &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this topic data */
						dbg_printf ("|---> Topic: %s | mode: %s | domId: %s | blacklist: %d\r\n",
							    name, mode_s (mode), dom_wcs (domId), blacklist);
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
			if ((ret = DDS_SP_get_domain_access (d->handle, &domId, &access, &exclusive, &transport, &blacklist)))
				return (ret);
			else {
				/* Print the domain data */
				dbg_printf ("%d) Domain %s: access: %s | exclusive: %d | transport: %s | blacklist: %d\r\n", 
					    d->handle, dom_wcs (domId), acc_s (access), exclusive, transp_s (transport), blacklist);
				if (d->topics.head != NULL) {
					LIST_FOREACH (d->topics, dtp) {
						if ((ret = DDS_SP_get_topic_access (0, d->handle,
										     dtp->index, 
										     &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this topic data */
						dbg_printf ("|---> Topic: %s | mode: %s | domId: %s | blacklist: %d\r\n", 
							    name, mode_s (mode), dom_wcs (domId), blacklist);
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

/* engine functions */

DDS_ReturnCode_t engineAdd (const char *engine_id, int index)
{
	EngineList *newEngine = malloc (sizeof (EngineList));

	if (!newEngine)
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	newEngine->name = malloc (strlen (engine_id) + 1);
	if (!newEngine->name) {
		free (newEngine);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	strcpy (newEngine->name, engine_id);
	newEngine->index = index;
	newEngine->next = engineListTop->next;
	engineListTop->next = newEngine;

	return (DDS_RETCODE_OK);
}

static void engineRemove (const char *engine_id)
{
	EngineList *tmp, *prev = engineListTop;

	for (tmp = engineListTop->next; tmp; tmp = tmp->next) {
		if (strcmp(engine_id, tmp->name) == 0) {
			prev->next = tmp->next;
			free(tmp->name);
			free(tmp);
			return;
		}
		prev = tmp;
	}
}

/* Find if the engine is already present in the list,
   if it is, return it's index */
static int engineSeek (const char *engine_id)
{
	EngineList *tmp;

	for (tmp = engineListTop->next; tmp; tmp = tmp->next) {
		if (strcmp (engine_id, tmp->name) == 0)
			return (tmp->index);
	}
	return (-1);
}
 
DDS_ReturnCode_t DDS_SP_init_engine(const char *engine_id, 
				    void* (*engine_constructor_callback)(void))
{
	log_printf (SEC_ID, 0, "MSP: init engine\n\r");

	if (!engineListTop) {
		if (!(engineListTop = malloc (sizeof (EngineList))))
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		engineListTop->name = NULL;
		engineListTop->index = -1;
		engineListTop->next = NULL;
	}
	if (engineSeek (engine_id) == -1) {
		engines [engine_counter] = (ENGINE *) engine_constructor_callback ();
		if (!engines [engine_counter]) {
			log_printf (SEC_ID, 0, "MSP: problem creating the engine\n\r");
			return (DDS_RETCODE_ERROR);
		}
		if (!ENGINE_init (engines [engine_counter])) {
			ENGINE_free (engines [engine_counter]);
			log_printf (SEC_ID, 0, "MSP: problem initialising engine\n\r");
			return (DDS_RETCODE_ERROR);
		}
		else {
			if (engineAdd (ENGINE_get_id (engines [engine_counter]),
				       engine_counter))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			engine_counter++;
			return (DDS_RETCODE_OK);
		}
	}
	else
		log_printf (SEC_ID, 0, "MSP: engine already created\n\r");

	return (DDS_RETCODE_OK);
}


void DDS_SP_engine_cleanup (void)
{
	int i = 0;

	log_printf (SEC_ID, 0, "MSP: Cleanup of the ENGINES\r\n");
	
	for (i = 0; i < engine_counter; i++ ) {
		engineRemove (ENGINE_get_id (engines [i]));
		ENGINE_free (engines [i]);
	}

	if (engineListTop)
		free (engineListTop);

	inits &= ~ENGINE_INIT;
	engine_init ();
}

/* bio functions */

static int bio_read (const char *file)
{
	in = BIO_new (BIO_s_file_internal());
	if (in == NULL) {
		log_printf (SEC_ID, 0, "MSP: BIO problem\n\r");
		return (0);
	}
	if (BIO_read_filename(in, file) <= 0) {
		log_printf (SEC_ID, 0, "MSP: BIO read problem\n\r");
		return (0);
	}
	return (1);
}

static void bio_cleanup (void)
{
	if (in)
		BIO_free(in);
}

static void set_md5 (unsigned char       *dst,
		     const unsigned char *identity, 
		     size_t              length)
{
	MD5_CONTEXT	md5;

	md5_init (&md5);
	md5_update (&md5, identity, length);
	md5_final (dst, &md5);
}

static DDS_ReturnCode_t clone_participant (MSParticipant_t *wp,
					   const char      *name,
					   size_t          n,
					   DDS_Credentials *credentials,
					   size_t          length,
					   unsigned        *handle)
{
	MSParticipant_t	*p;

	ARG_NOT_USED (length)

	if (n >= MAX_PARTICIPANT_NAME) {
		warn_printf ("MSP: identity name too long!");
		free (credentials);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	p = malloc (sizeof (MSParticipant_t));
	if (!p)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	memset (p, 0, sizeof (MSParticipant_t));
	p->handle = ++num_ids;
	memcpy (p->name, name, n + 1);
	set_md5 (p->key, (unsigned char *) p->name, n);
	p->key_length = 16;
	p->credentials = credentials;
	p->cloned = wp;
	p->input_number = 0;
	id_handles [p->handle] = p;
	LIST_ADD_HEAD (participants, *p);
	*handle = p->handle;
	return (DDS_RETCODE_OK);
}


