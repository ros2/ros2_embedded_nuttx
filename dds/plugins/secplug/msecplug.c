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

struct ms_domains_st	domains;
MSDomain_t		*domain_handles [MAX_DOMAINS];
unsigned		num_domains;

ENGINE			*engines [MAX_ENGINES];
int			engine_counter;

struct ms_participants_st	participants;
MSParticipant_t			*id_handles [MAX_ID_HANDLES];
unsigned			num_ids;

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

	dbg_printf ("This is a dump of the access db\r\n");

	if (!num_ids && ! num_domains)
		dbg_printf ("The access db is empty");

	if (num_ids) {
		LIST_FOREACH (participants, p) {
			if ((ret = DDS_SP_get_participant_access (p->handle, &name [0], &access, &blacklist)))
				return (ret);
			
			else {
				if (p->cloned)
					dbg_printf ("%d) Participant: %s | access: %d | blacklist: %d | clone from %d\r\n", 
						    p->handle, name, access, blacklist, p->cloned->handle);
				else
					dbg_printf ("%d) Participant: %s | access: %d | blacklist: %d\r\n", 
						    p->handle, name, access, blacklist);
				if (p->topics.head != NULL) {
					LIST_FOREACH (p->topics, tp) {
						if ((ret = DDS_SP_get_topic_access (p->handle, 0, tp->topic.index, 
										     &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this topic data */
						dbg_printf ("|---> Topic: %s | mode: %d | domId: %u | blacklist: %d\r\n",
							    name, mode, domId, blacklist);
					}
				}
				if (p->partitions.head != NULL) {
					LIST_FOREACH (p->partitions, pt) {
						if ((ret = DDS_SP_get_partition_access (p->handle, 0, 
											 pt->partition.index,
											 &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this partition data */
						dbg_printf ("|---> Partition: %s | mode: %d | domId: %u | blacklist: %d\r\n",
							    name, mode, domId, blacklist);
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
				dbg_printf ("%d) Domain: | domId: %u | access: %d | exclusive: %d | transport: %d | blacklist: %d\r\n", 
					    d->handle, domId, access, exclusive, transport, blacklist);
				if (d->topics.head != NULL) {
					LIST_FOREACH (d->topics, dtp) {
						if ((ret = DDS_SP_get_topic_access (0, d->handle,
										     dtp->index, 
										     &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this topic data */
						dbg_printf ("|---> Topic: %s | mode: %d | domId: %u | blacklist: %d\r\n", 
							    name, mode, domId, blacklist);
					}
				}
				if (d->partitions.head != NULL) {
					LIST_FOREACH (d->partitions, dpt) {
						if ((ret = DDS_SP_get_partition_access (0, d->handle, 
										     dpt->index,
										     &name [0], &mode, &domId, &blacklist)))
							return (ret);
						/* print this partition data */
						dbg_printf ("|---> Partition: %s | mode: %d | domId: %u | blacklist: %d\r\n",
							    name, mode, domId, blacklist);
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

static DDS_ReturnCode_t msp_validate_local_id (const char      *name,
					       DDS_Credentials *credentials,
					       size_t          length,
					       unsigned        *handle)
{
	MSParticipant_t	*p;
	DDS_ReturnCode_t ret;

	if (!credentials) {
		log_printf (SEC_ID, 0, "MSP: Something is wrong with the credentials.\n\r");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	log_printf (SEC_ID, 0, "MSP: Validate local identity for '%s' -> ", name);
	LIST_FOREACH (participants, p) {
		if (p->name [0] == '\0' ||
		    (strchr (p->name, '*') && !nmatch (p->name, name, 0))) {
			log_printf (SEC_ID, 0, "(matched with '%s') ", p->name);
			if (p->blacklist)
				goto blacklist;
			ret = clone_participant (p, name, strlen (name),
						 credentials, length, handle);
			if (!ret)
				log_printf (SEC_ID, 0, "accepted.\r\n");
		     	else
				log_printf (SEC_ID, 0, "error.\r\n");
			return (ret);
		}
		else if (!strcmp (p->name, name)) {

			if (p->blacklist)
				goto blacklist;
			
			/* Found it. */
			if (p->credentials)
				free (p->credentials);

			p->credentials = (DDS_Credentials *) credentials;
			if (!p->key_length) {
				set_md5 (p->key, (unsigned char *) name, strlen (name));
				p->key_length = 16;
			}
			*handle = p->handle;
			log_printf (SEC_ID, 0, "accepted.\r\n");
			return (DDS_RETCODE_OK);
		}
	}
	log_printf (SEC_ID, 0, "denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);
 blacklist:
	log_printf (SEC_ID, 0, " (Blacklisted) ");
	log_printf (SEC_ID, 0, "denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);
}

static DDS_ReturnCode_t msp_set_local_handle (PermissionsHandle_t perm,
					      DDS_DomainId_t      id)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (ret);
	}
	
	local_dom_part_handle = id;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_validate_peer_id (const unsigned char *identity,
					      size_t              length,
					      int                 *action,
					      unsigned char       *challenge,
					      size_t              *ch_length)
{
	MSParticipant_t	 *p, *np;
	unsigned	 handle;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (challenge)

	log_printf (SEC_ID, 0, "MSP: Validate peer identity '%s' -> ", identity);
	LIST_FOREACH (participants, p) {
		if (p->name [0] == '\0' ||
		    (strchr (p->name, '*') &&
		     !nmatch (p->name, (char *) identity, 0))) {
			
			if (p->blacklist)
				goto blacklist;
			
			ret = clone_participant (p, (char *) identity, length - 1,
					       NULL, 0, &handle);
			if (!ret) {
				*action = DDS_AA_ACCEPTED;
				np = id_handles [handle];
				if (!np->key_length) {
					set_md5 (np->key, identity, length - 1);
					np->key_length = 16;
				}
				log_printf (SEC_ID, 0, "cloned and accepted.\r\n");
			}
			else
				log_printf (SEC_ID, 0, "error.\r\n");
			*ch_length = 0;
			return (ret);
		}
		else if (!strcmp (p->name, (char *) identity)) {

			if (p->blacklist)
				goto blacklist;

			/* Found it. */
			*ch_length = 0;
			*action = DDS_AA_ACCEPTED;
			if (!p->key_length) {
				set_md5 (p->key, identity, length - 1);
				p->key_length = 16;
			}
			log_printf (SEC_ID, 0, "accepted.\r\n");
			return (DDS_RETCODE_OK);
		}
	}
	*action = DDS_AA_REJECTED;
	log_printf (SEC_ID, 0, "denied.\r\n");
	return (DDS_RETCODE_OK);

 blacklist:
	*action = DDS_AA_REJECTED;
	log_printf (SEC_ID, 0, " (Blacklisted) ");
	log_printf (SEC_ID, 0, "denied.\r\n");
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_set_peer_handle (PermissionsHandle_t  perm,
					     DDS_InstanceHandle_t part_handle)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (ret);
	}
	
	ip->part_handle = part_handle;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_validate_ssl_connection (PermissionsHandle_t perm,
						     SSL                 *ssl,
						     struct sockaddr     *sp,
						     int                 *action)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;
	X509             *peer_cert;

	struct sockaddr_in	*s4;
#ifdef DDS_IPV6
	struct sockaddr_in6	*s6;
	char			addrbuf [INET6_ADDRSTRLEN];
#endif

	if (sp->sa_family == AF_INET) {
		s4 = (struct sockaddr_in *) sp;
		log_printf (SEC_ID, 0, "DTLS: connection from %s:%d ->",
			     inet_ntoa (s4->sin_addr),
		             ntohs (s4->sin_port));
	}
#ifdef DDS_IPV6
	else if (sp->sa_family == AF_INET6) {
		s6 = (struct sockaddr_in6 *) sp;
		log_printf (SEC_ID, 0, "DTLS: connection from %s:%d ->",
        		     inet_ntop (AF_INET6,
					&s6->sin6_addr,
					addrbuf,
					INET6_ADDRSTRLEN),
			     ntohs (s6->sin6_port));
	}
#endif
	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret)
		goto denied;

	/* The check to see if peer user is who he says he is, needs to be performed in qeo */
	
	if (extra_authentication_check != NULL)
		if (extra_authentication_check ((void *) ssl, ip->name) != DDS_RETCODE_OK) {
			log_printf (SEC_ID, 0, "Extra Authentication check -> Denied.\r\n");
			*action = DDS_AA_REJECTED;
			return (DDS_RETCODE_OK);
		}
		
	if ((peer_cert = SSL_get_peer_certificate (ssl))) {
#ifdef DDS_DEBUG
		X509_NAME_print_ex_fp (stdout, X509_get_subject_name (peer_cert),
						  1, XN_FLAG_MULTILINE);
#endif
		log_printf (SEC_ID, 0, "\nDTLS: cipher: %s ->", SSL_CIPHER_get_name (SSL_get_current_cipher (ssl)));
		log_printf (SEC_ID, 0, "accepted.\r\n");
		*action = DDS_AA_ACCEPTED;
		X509_free (peer_cert);
	}
	else 
		goto denied;
	return (DDS_RETCODE_OK);

 denied:
	log_printf (SEC_ID, 0, "denied.\r\n");
	*action = DDS_AA_REJECTED;
	return (DDS_RETCODE_OK);
	
}

static DDS_ReturnCode_t msp_get_id_token (IdentityHandle_t handle,
				          unsigned char    *identity,
				          size_t           *id_length)
{
	MSParticipant_t	*p;
	unsigned	n;

	log_printf (SEC_ID, 0, "MSP: Get identity token -> ");
	if (!handle || handle > num_ids) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
	if (handle < 1 ||
	    handle > num_ids ||
	    (p = id_handles [handle]) == NULL) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
	n = strlen (p->name) + 1;
	if (*id_length <= n) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memcpy (identity, p->name, n);
	*id_length = n;

	log_printf (SEC_ID, 0, "ok.\r\n");
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_challenge_id (unsigned char *challenge,
				          size_t        challenge_length,
				          unsigned char *response,
				          size_t        *response_length)
{
	ARG_NOT_USED (challenge)
	ARG_NOT_USED (challenge_length)
	ARG_NOT_USED (response)
	ARG_NOT_USED (response_length)
	
	log_printf (SEC_ID, 0, "MSP: Challenge identity (TBC).\r\n");

	/* -- we don't do this yet -- */

	*response_length = 0;

	return (DDS_RETCODE_UNSUPPORTED);
}

static DDS_ReturnCode_t msp_validate_response (unsigned char *response,
				               size_t        response_length,
					       int           *action)
{
	ARG_NOT_USED (response)
	ARG_NOT_USED (response_length)
	ARG_NOT_USED (action)
	
	log_printf (SEC_ID, 0, "MSP: Validate identity challenge (TBC).\r\n");

	/* -- we don't do this yet -- */

	*action = DDS_AA_REJECTED;

	return (DDS_RETCODE_UNSUPPORTED);
}

static int allow_access (MSDomain_t *dp, MSParticipant_t *pp)
{
	MSUTopic_t	*tp;
	MSAccess_t	access;
	
	if (!dp->access)
		return (1);

	if (pp->cloned)
		pp = pp->cloned;

	access = pp->access;

	if (access < dp->access)
		return (0);

	if (!dp->exclusive)
		return (1);

	LIST_FOREACH (pp->topics, tp) {
		if (!tp->topic.mode)
			continue;

		if (tp->id == ~0U || tp->id == dp->domain_id)
			return (1);
	}
	return (0);
}

static DDS_ReturnCode_t msp_validate_local_perm (DDS_DomainId_t   domain_id,
						 IdentityHandle_t *handle)
{
	MSDomain_t	*p;
	MSParticipant_t	*pp;

	log_printf (SEC_ID, 0, "MSP: Validate local permissions -> ");

	p = lookup_domain (domain_id, 1);
	
	if (!p)
		log_printf (SEC_ID, 0, "MSP: not p");
	if (!*handle)
		log_printf (SEC_ID, 0, "MSP: not *handle");
	if (*handle > num_ids)
		log_printf (SEC_ID, 0, "MSP: *handle larger than possible");

	if (!p) {
		log_printf (SEC_ID, 0, ", domain not found -> accepted as unsecure domain.\r\n");
		*handle = 1;
		return (DDS_RETCODE_OK);
	}
    	if (!p || !*handle || *handle > num_ids) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
	pp = id_handles [*handle];
	if (allow_access (p, pp)) {
		*handle = (p->handle << 16) | pp->handle;
		log_printf (SEC_ID, 0, "accepted.\r\n");
		return (DDS_RETCODE_OK);
	}
	else {
		*handle = 0;
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
}

static DDS_ReturnCode_t msp_validate_peer_perm (DDS_DomainId_t      domain_id,
						unsigned char       *permissions,
					        size_t              length,
						PermissionsHandle_t *handle)
{
	MSDomain_t	*p;
	MSParticipant_t	*pp;

	log_printf (SEC_ID, 0, "MSP: Validate peer permissions -> ");

	p = lookup_domain (domain_id, 1);
	if (!p) {
		log_printf (SEC_ID, 0, ", domain not found -> accepted as unsecure domain.\r\n");
		*handle = 1;
		return (DDS_RETCODE_OK);
	}
	LIST_FOREACH (participants, pp) {
		if (pp->key_length == 16 && 
		    length == 16 &&
		    !memcmp (pp->key, permissions, 16)) {
			if (allow_access (p, pp)) {
				*handle = (p->handle << 16) | pp->handle;
				log_printf (SEC_ID, 0, "accepted.\r\n");
				return (DDS_RETCODE_OK);
			}
			else
				break;
		}
	}
	*handle = 0;
	log_printf (SEC_ID, 0, "denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);
}

static DDS_ReturnCode_t msp_perm_pars (PermissionsHandle_t perm,
				       MSDomain_t          **p,
				       MSParticipant_t     **pp)
{
	unsigned	dh, ih;

	dh = perm >> 16;
	ih = perm & 0xffff;
	if (!dh || dh > num_domains ||
	    !ih || ih > num_ids ||
	    (*p = domain_handles [dh]) == NULL ||
	    (*pp = id_handles [ih]) == NULL)
		return (DDS_RETCODE_ACCESS_DENIED);

	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_check_create_participant (PermissionsHandle_t            perm,
						      const DDS_DomainParticipantQos *qos,
						      unsigned                       *secure)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (qos)

	log_printf (SEC_ID, 0, "MSP: Check if local participant may be created -> ");

	if (perm == 1) {
		*secure = 0;
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}
	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (ret);
	}
	*secure = dp->access;
	log_printf (SEC_ID, 0, "accepted.\r\n");
	return (DDS_RETCODE_OK);
}

static MSTopic_t *lookup_topic (MSDomain_t      *dp,
				MSParticipant_t *pp,
				const char      *topic_name)
{
	MSUTopic_t	*up;
	MSTopic_t	*tp;
	
	if (pp->cloned)
		pp = pp->cloned;

	LIST_FOREACH (pp->topics, up) {
		if (up->id != ~0U &&
		    up->id != dp->domain_id)
			continue;

		if (!up->topic.name) {
			log_printf (SEC_ID, 0, " Participant Topic rule for \'%s\' (matched with *)\r\n", topic_name);
			if (!up->topic.blacklist)
				return (&up->topic);
			else
				goto blacklist;
		}
		else if (strchr (up->topic.name, '*')) {
			if (!nmatch (up->topic.name, topic_name, 0)) {
				log_printf (SEC_ID, 0, " Participant Topic rule for \'%s\' (matched with %s)\r\n", topic_name, up->topic.name);
				if (!up->topic.blacklist)
					return (&up->topic);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (up->topic.name, topic_name)) {
			log_printf (SEC_ID, 0, " Participant Topic rule for \'%s' (exact match)\r\n", topic_name);
			if (!up->topic.blacklist)
				return (&up->topic);
			else
				goto blacklist;
		}
	}

	/* No match in participant topic ruleset, check domain topics. */

	LIST_FOREACH (dp->topics, tp) {
		if (!tp->name) {
			log_printf (SEC_ID, 0, "Domain Topic rule for \'%s\' (matched with *)\r\n", topic_name);
			if (!tp->blacklist)
				return (tp);
			else
				goto blacklist;
		}

		else if (strchr (tp->name, '*')) {
			if (!nmatch (tp->name, topic_name, 0)) {
				log_printf (SEC_ID, 0, " Domain Topic rule for \'%s\' (matched with %s)\r\n", topic_name, tp->name);
				if (!tp->blacklist)
					return (tp);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (tp->name, topic_name)) {
			log_printf (SEC_ID, 0, " Domain Topic rule for \'%s\' (exact match)\r\n", topic_name);
			if (!tp->blacklist)
				return (tp);
			else
				goto blacklist;
		}
	}
	log_printf (SEC_ID, 0, " No Topic rule found\r\n");
       	return (NULL);

 blacklist:
	log_printf (SEC_ID, 0, " (Blacklisted) ");
	return (NULL);
}

static MSPartition_t *lookup_partition (MSDomain_t      *dp,
					MSParticipant_t *pp,
					const char      *partition_name,
					MSMode_t        mode)
{
	MSUPartition_t	*up;
	MSPartition_t	*tp;
	
	if (pp->cloned)
		pp = pp->cloned;

	LIST_FOREACH (pp->partitions, up) {
		if (up->id != ~0U &&
		    up->id != dp->domain_id)
			continue;

		if ((up->partition.mode & mode) == 0)
			continue;
		
		if (!up->partition.name) {
			log_printf (SEC_ID, 0, " Participant Partition rule for \'%s\' (matched with *)\r\n", partition_name);
			if (!up->partition.blacklist)
				return (&up->partition);
			else
				goto blacklist;
		}

		else if (strchr (up->partition.name, '*')) {
			if (!nmatch (up->partition.name, partition_name, 0)) {
				log_printf (SEC_ID, 0, " Participant Partition rule for \'%s\' (matched with %s)\r\n", partition_name, up->partition.name);
				if (!up->partition.blacklist)
					return (&up->partition);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (up->partition.name, partition_name)) {
			log_printf (SEC_ID, 0, " Participant Partition rule for \'%s\' (exact match)\r\n", partition_name);
			if (!up->partition.blacklist)
				return (&up->partition);
			else
				goto blacklist;
		}
	}

	/* No match in participant partition ruleset, check domain partitions. */
	LIST_FOREACH (dp->partitions, tp) {
		if ((tp->mode & mode) == 0)
			continue;
		
		if (!tp->name)
			if (!tp->blacklist) {
				log_printf (SEC_ID, 0, " Domain Partition rule for \'%s\' (matched with *)\r\n", partition_name);
				return (tp);
			}
			else
				goto blacklist;

		else if (strchr (tp->name, '*')) {
			if (!nmatch (tp->name, partition_name, 0)) {
				log_printf (SEC_ID, 0, " Domain Partition rule for \'%s\' (matched with %s)\r\n", partition_name, tp->name);
				if (!tp->blacklist)
					return (tp);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (tp->name, partition_name)) {
			log_printf (SEC_ID, 0, " Domain Partition rule for \'%s\' (exact match)\r\n", partition_name);
			if (!tp->blacklist)
				return (tp);
			else
				goto blacklist;
		}
	}
	log_printf (SEC_ID, 0, " No Partition rule found for \'%s\'\r\n", partition_name);
	return (NULL);
 blacklist:
	log_printf (SEC_ID, 0, " (Blacklisted) ");
	return (NULL);
}

static DDS_ReturnCode_t msp_check_create_topic (PermissionsHandle_t perm,
					        const char          *topic_name,
					        const DDS_TopicQos  *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (qos)
	
	log_printf (SEC_ID, 0, "MSP: Topic('%s') create request by local participant -> ", topic_name);

	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}

	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_CREATE) == 0) {
		log_printf (SEC_ID, 0, "--> denied.\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_check_create_writer (PermissionsHandle_t     perm,
					         const char              *topic_name,
					         const DDS_DataWriterQos *qos,
						 const Strings_t         *partitions)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	unsigned         i;
	String_t         **name_ptr;
	const char       *part_ptr;

	ARG_NOT_USED (qos)

	log_printf (SEC_ID, 0, "MSP: DataWriter('%s') create request by local participant -> ", topic_name);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}
	
	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_WRITE) == 0) 
		goto denied;
	
	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {
			/* The partition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip,(char*) part_ptr, TA_WRITE);
			if (!pp || (pp->mode & TA_WRITE) == 0)
				goto denied;
		}
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);
	
}

static DDS_ReturnCode_t msp_check_create_reader (PermissionsHandle_t     perm,
					         const char              *topic_name,
					         const DDS_DataWriterQos *qos,
						 const Strings_t         *partitions)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	unsigned         i;
	String_t **name_ptr;
	const char *part_ptr;
		
	ARG_NOT_USED (qos);

	log_printf (SEC_ID, 0, "MSP: DataReader('%s') create request by local particpant -> ", topic_name);

	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}

	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_READ) == 0)
		goto denied;

	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {

			/* The partition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip, (char*) part_ptr, TA_READ);
			if (!pp || (pp->mode & TA_READ) == 0)
				goto denied;
		}
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);	
}

static DDS_ReturnCode_t msp_check_peer_participant (PermissionsHandle_t perm,
					 	    String_t            *user_data)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (user_data)
	
	log_printf (SEC_ID, 0, "MSP: Check if peer participant is allowed -> ");
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}
	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_check_peer_topic (PermissionsHandle_t      perm,
					      const char               *topic_name,
					      const DiscoveredTopicQos *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (qos)
	
	log_printf (SEC_ID, 0, "MSP: Topic('%s') create request by peer particpant -> ", topic_name);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}

	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_CREATE) == 0) {
		log_printf (SEC_ID, 0, "--> denied.\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_check_peer_writer (PermissionsHandle_t       perm,
					       const char                *topic_name,
					       const DiscoveredWriterQos *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	Strings_t       *partitions;
	String_t        **name_ptr;
	const char      *part_ptr;
	unsigned        i;

	partitions = qos->partition;
	
	log_printf (SEC_ID, 0, "MSP: DataWriter('%s') create request by peer particpant -> ", topic_name);

	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}

	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_WRITE) == 0)
		goto denied;
	
	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {
			/* The parition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip, (char*) part_ptr, TA_WRITE);
			if (!pp || (pp->mode & TA_WRITE) == 0)
				goto denied;
		}
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);
}

static DDS_ReturnCode_t msp_check_peer_reader (PermissionsHandle_t      perm,
					       const char               *topic_name,
					       const DiscoveredReaderQos *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	Strings_t       *partitions;
	String_t        **name_ptr;
	const char      *part_ptr;
	unsigned        i;

	log_printf (SEC_ID, 0, "MSP: DataReader('%s') create request by peer particpant -> ", topic_name);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted");
		return (DDS_RETCODE_OK);
	}
	
	partitions = qos->partition;
	
	ret = msp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_READ) == 0)
		goto denied;

	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {
			/* The partition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip, (char*) part_ptr, TA_READ);
			if (!pp || (pp->mode & TA_READ) == 0)
				goto denied;
		}
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_ACCESS_DENIED);
}

static DDS_ReturnCode_t msp_get_perm_token (PermissionsHandle_t handle,
				            unsigned char       *permissions,
					    size_t              *perm_length)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ret = msp_perm_pars (handle, &dp, &ip);
	if (ret)
		return (ret);

	if (*perm_length < ip->key_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (ip->key_length)
		memcpy (permissions, ip->key, ip->key_length);
	*perm_length = ip->key_length;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_get_domain_sec_caps (DDS_DomainId_t domain_id,
						 unsigned       *sec_caps)
{
	MSDomain_t	*p;

	p = lookup_domain (domain_id, 1);
	if (p)
		*sec_caps = p->transport;
	else
		*sec_caps = 0;

	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_get_certificate (X509             **certificate, 
					     IdentityHandle_t id_handle)
{	
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	int             ret;

	if (credentials->credentialKind == DDS_ENGINE_BASED) {
		dataMap data;

		log_printf(SEC_ID, 0, "MSP: Get the certificate from the engine.\r\n");
	
		data.id = credentials->info.engine.cert_id;
		data.data = (void *) certificate;
		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);
	
		return (ENGINE_ctrl (engines[ret],
				     CMD_LOAD_CERT_CTRL, 
				     0, (void *)&data, NULL));
	}
	else if (credentials->credentialKind == DDS_FILE_BASED) {
		X509 *cert;

		log_printf(SEC_ID, 0, "MSP: Get the certificate from file data.\r\n");

		bio_read (credentials->info.filenames.certificate_chain_file);
		cert = PEM_read_bio_X509 (in,NULL ,NULL, NULL);
		bio_cleanup ();
		if (!cert)
			fatal_printf ("No certificate found");

		*certificate = cert;
	}
	else if (credentials->credentialKind == DDS_SSL_BASED) {
		log_printf (SEC_ID, 0, "MSP: Get the certificate from SSL data.\r\n");
		*certificate = sk_X509_value (credentials->info.sslData.certificate_list, 0);
	}
	else {
		/*TODO: Go get the cert from data*/
	}

	return (DDS_RETCODE_OK); 
}

static DDS_ReturnCode_t msp_get_CA_certificate_list (X509             **CAcertificates, 
						     IdentityHandle_t id_handle)
{
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	int             ret, i;
	X509            *cert;
	
	if (credentials->credentialKind == DDS_ENGINE_BASED) {
		dataMap data;
		
		log_printf(SEC_ID, 0, "MSP: Get the CA certificate list from the engine.\r\n");
		
		data.id = credentials->info.engine.cert_id;
		data.data = (void *) CAcertificates;
		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);
		
		return (ENGINE_ctrl (engines [ret], 
				    CMD_LOAD_CA_CERT_CTRL, 
				     0, (void *)&data, NULL));
	}
	else if (credentials->credentialKind == DDS_FILE_BASED) {
		/* Go get the cert from file*/
		int counter = 0;
		X509 *cert = NULL;

		log_printf(SEC_ID, 0, "MSP: Get the CA certificate list from file data.\r\n");
		
		bio_read (credentials->info.filenames.certificate_chain_file);		
		while ((cert = PEM_read_bio_X509 (in, NULL, NULL, NULL)) != NULL){
			/*Discard the first because it's not a CA cert*/
			if (counter != 0) {
				CAcertificates [counter - 1] = cert;
			}
			else
				X509_free (cert);
			counter++;
		}
		if (cert)
			X509_free (cert);
		bio_cleanup ();
	}
	else if (credentials->credentialKind == DDS_SSL_BASED) {
		log_printf(SEC_ID, 0, "MSP: Get the CA certificate list from SSL data.\r\n");
		
		for (i = 1; i < sk_num ((const _STACK*) credentials->info.sslData.certificate_list); i++) {
			cert = sk_X509_value (credentials->info.sslData.certificate_list, i);
			CAcertificates [i - 1] = cert;
		}
	}
	else {
		/* Go get the cert from data*/
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_get_private_key (void             **privateKey, 
					     IdentityHandle_t id_handle)
{
	/*Get the right private key from the engine or whatever*/
	/*If the setcredentials has received an engine, use that engine*/
	
	/*Get the specific participant*/
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	EVP_PKEY *fetchedKey = NULL;
	int ret;
	ENGINE *e = NULL;
	
	if (credentials->credentialKind == DDS_ENGINE_BASED) {
		log_printf(SEC_ID, 0, "MSP: Get the private key from the engine.\r\n");
		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);

		e = engines[ret];
		fetchedKey = ENGINE_load_private_key (e, 
						      credentials->info.engine.priv_key_id,
						      NULL, 
						      NULL);
		*privateKey = fetchedKey;
	}
	else if (credentials->credentialKind == DDS_FILE_BASED) {
		log_printf(SEC_ID, 0, "MSP: Get the private key from file data.\r\n");
		bio_read (credentials->info.filenames.private_key_file);
		fetchedKey = PEM_read_bio_PrivateKey (in, NULL, NULL, NULL);
		bio_cleanup ();
		if (!fetchedKey)
			fatal_printf ("MSP: Error retrieving the private key");
	
		*privateKey = fetchedKey;
	}
	else if (credentials->credentialKind == DDS_SSL_BASED) {
		log_printf(SEC_ID, 0, "MSP: Get the private key from SSL data.\r\n");
		*privateKey = credentials->info.sslData.private_key;
	}
	else {
		/* Go get the certkey from data*/
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_sign_with_private_key (int                 type, 
						   const unsigned char *m, 
						   size_t              m_len,
						   unsigned char       *sigret, 
						   size_t              *siglen, 
						   IdentityHandle_t    id_handle)
{
	void *privateKey;
	RSA *rsa = NULL;
	unsigned slen = *siglen;

	msp_get_private_key (&privateKey, id_handle);
	rsa = ((EVP_PKEY *) privateKey)->pkey.rsa;

	if (!RSA_sign (type, m, m_len, sigret, &slen, rsa))
		return (DDS_RETCODE_ERROR);

	*siglen = slen;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_verify_signature (int                 type, 
					      const unsigned char *m, 
					      size_t              m_len, 
					      unsigned char       *sigbuf, 
					      size_t              siglen, 
					      SSL                 *ssl) 
{
	RSA *peer_rsa_pub = X509_get_pubkey(SSL_get_peer_certificate (ssl))->pkey.rsa;

	if (!RSA_verify(type, m, m_len, sigbuf, siglen, peer_rsa_pub))
		return (DDS_RETCODE_ERROR);
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_get_nb_of_CA_cert (int              *nb, 
					       IdentityHandle_t id_handle)
{
	/*get nb of CA cert*/
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	int ret;
	
	if (credentials->credentialKind == DDS_ENGINE_BASED) {
		dataMap data;

		log_printf(SEC_ID, 0, "MSP: Get the number of CA certificates from the engine.\r\n");
		data.id = credentials->info.engine.cert_id;
		data.data = NULL;

		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);
		
		*nb = ENGINE_ctrl (engines [ret],
				   CMD_LOAD_CA_CERT_CTRL,
				   0, (void *)&data, NULL);
	}
	else if (credentials->credentialKind == DDS_FILE_BASED) {
		int counter = 0;
		X509 *cert = NULL;

		log_printf(SEC_ID, 0, "MSP: Get the number of CA certificates from file data.\r\n");
		bio_read (credentials->info.filenames.certificate_chain_file);
		while ((cert = PEM_read_bio_X509 (in, NULL,NULL, NULL)) != NULL){
			counter ++;
			X509_free (cert);
		}
		*nb = counter - 1;
		bio_cleanup ();
	}
	else if (credentials->credentialKind == DDS_SSL_BASED) {
		log_printf(SEC_ID, 0, "MSP: Get the number of CA certificates from SSL data.\r\n");
		*nb = sk_num ((const _STACK*) credentials->info.sslData.certificate_list) - 1;
	}
	else {
		/* Go get the cert from data*/
	}
	
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t msp_policy_plugin (DDS_SecurityRequest code,
					   DDS_SecurityReqData *data)
{
	DDS_ReturnCode_t	rc = DDS_RETCODE_OK;
	
	msp_init ();
	lock_take (sp_lock);

	switch (code) {
		case DDS_VALIDATE_LOCAL_ID:
			rc = msp_validate_local_id (data->name,
						    data->data, data->length,
						    &data->handle);
			break;
        	case DDS_SET_LOCAL_HANDLE:
			rc = msp_set_local_handle (data->handle,
						   data->domain_id);
			break;
		case DDS_VALIDATE_PEER_ID:
			rc = msp_validate_peer_id (data->data, data->length,
						   &data->action,
						   data->rdata, &data->rlength);
			break;
	        case DDS_SET_PEER_HANDLE:
			rc = msp_set_peer_handle (data->handle,
						  data->secure);
			break;
		case DDS_ACCEPT_SSL_CX:
			rc = msp_validate_ssl_connection (data->handle, data->data, data->rdata,
							  &data->action);
			break;
		case DDS_GET_ID_TOKEN:
			rc = msp_get_id_token (data->handle,
					       data->rdata, &data->rlength);
			break;
		case DDS_CHALLENGE_ID:
			rc = msp_challenge_id (data->data, data->length,
					       data->rdata, &data->rlength);
			break;
		case DDS_VALIDATE_RESPONSE:
			rc = msp_validate_response (data->data, data->length,
						    &data->action);
			break;
		case DDS_VALIDATE_LOCAL_PERM:
			rc = msp_validate_local_perm (data->domain_id, &data->handle);
			break;
		case DDS_VALIDATE_PEER_PERM:
			rc = msp_validate_peer_perm (data->domain_id,
						     data->data, data->length,
						     &data->handle);
			break;
		case DDS_CHECK_CREATE_PARTICIPANT:
			rc = msp_check_create_participant (data->handle,
							   data->data,
							   &data->secure);
			break;
		case DDS_CHECK_CREATE_TOPIC:
			rc = msp_check_create_topic (data->handle, data->name, data->data);
			break;
		case DDS_CHECK_CREATE_WRITER:
			rc = msp_check_create_writer (data->handle, data->name,
						      data->data, data->rdata);
			break;
		case DDS_CHECK_CREATE_READER:
			rc = msp_check_create_reader (data->handle, data->name,
						      data->data, data->rdata);
			break;
		case DDS_CHECK_PEER_PARTICIPANT:
			rc = msp_check_peer_participant (data->handle, data->data);
			break;
		case DDS_CHECK_PEER_TOPIC:
			rc = msp_check_peer_topic (data->handle, data->name, data->data);
			break;
		case DDS_CHECK_PEER_WRITER:
			rc = msp_check_peer_writer (data->handle, data->name, data->data);
			break;
		case DDS_CHECK_PEER_READER:
			rc = msp_check_peer_reader (data->handle, data->name, data->data);
			break;
		case DDS_GET_PERM_TOKEN:
			rc = msp_get_perm_token (data->handle, data->rdata, &data->rlength);
			break;
		case DDS_GET_DOMAIN_SEC_CAPS:
			rc = msp_get_domain_sec_caps (data->domain_id, &data->secure);
			break;
		case DDS_GET_CERT:
			rc = msp_get_certificate (data->data, data->handle);
			break;
		case DDS_GET_CA_CERT:
			rc = msp_get_CA_certificate_list ((X509 **) data->data, data->handle);
			break;
		case DDS_GET_PRIVATE_KEY:
			rc = msp_get_private_key (data->data, data->handle);
			break;
		case DDS_SIGN_WITH_PRIVATE_KEY:
			rc = msp_sign_with_private_key (data->secure, data->data, 
							data->length, data->rdata, 
							&data->rlength, data->handle);
			break;
		case DDS_VERIFY_SIGNATURE:
			rc = msp_verify_signature (data->secure,
						   (const unsigned char *) data->name, 
						   data->length, data->rdata, 
						   data->rlength, (SSL*) data->data);
			break;
		case DDS_GET_NB_CA_CERT:
			rc = msp_get_nb_of_CA_cert ((int *) data->data, data->handle);
			break;
		default:
	  		rc = DDS_RETCODE_UNSUPPORTED;
			break;
	}
	lock_release (sp_lock);
	return (rc);
}

MSDomain_t *lookup_domain (unsigned domain_id, 
			   int      specific)
{
	MSDomain_t	*p = NULL;

	msp_init ();

	LIST_FOREACH (domains, p) 
		if (p->domain_id == domain_id ||
		    (specific && p->domain_id == ~0U))
			return (p);

	return (NULL);
}

MSParticipant_t *lookup_participant (const char *name)
{
	MSParticipant_t	*p;

	msp_init ();

	for (p = participants.head; p != (MSParticipant_t *) &participants; p = p->next)
		if (!strcmp (p->name, name))
			return (p);

	return (NULL);
}

/* msp_set_policy -- Set security policy to this code. */

#ifdef DDS_SECURITY

DDS_ReturnCode_t DDS_SP_set_policy (void)
{
	return (DDS_Security_set_policy (DDS_SECURITY_LOCAL, msp_policy_plugin));
}

#endif

#if 0
void msp_get_realm_name (char *realmName, IdentityHandle_t handle)
{
	X509_NAME *a;
	X509_NAME_ENTRY *ne;
	char *s;
	char tmp_buf [100];
	int i, n;
	X509 *myCert = NULL;

	msp_get_certificate (&myCert, handle);
	
	a = X509_get_issuer_name (myCert);
	for (i = 0; i < sk_X509_NAME_ENTRY_num (a->entries); i++)
	{
		ne = sk_X509_NAME_ENTRY_value (a->entries, i);
		n = OBJ_obj2nid(ne->object);
		i2t_ASN1_OBJECT(tmp_buf,sizeof(tmp_buf),ne->object);
		s=tmp_buf;

		if (!strcmp (s, "commonName")) {
			strcpy (realmName, (char *) ne->value->data);
			strcat (realmName, "*");
			break;
		}
	}

	X509_free (myCert);
	log_printf (SEC_ID, 0, "MSP: The realm name is %s \r\n", &realmName [0]);
}

#endif
