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

/* domain.c -- DDS Domain subsystem implementation.  This subsystem is
	       responsible managing all domain data within DDS.
	       This data is shared between DCPS, RTPS and the Discovery
	       protocols. */

#include <stdio.h>
#ifdef _WIN32
#include "win.h"
#else
#include <arpa/inet.h>
#endif
#include "pool.h"
#include "atomic.h"
#include "log.h"
#include "list.h"
#include "error.h"
#include "handle.h"
#include "strseq.h"
#include "debug.h"
#include "dds.h"
#include "dcps.h"
#include "domain.h"
#include "rtps.h"
#include "pl_cdr.h"
#ifdef DDS_TYPECODE
#include "vtc.h"
#include "xtypes.h"
#endif
#ifdef DDS_NATIVE_SECURITY
#include "sec_data.h"
#include "sec_c_std.h"
#endif
#include "dds/dds_types.h"
#include "dds/dds_aux.h"

/*#define LOG_DOMAIN		** Log domain primitives if defined. */
/*#define DDS_SHOW_QOS		** Show QoS references in Topic dumps. */

static Domain_t	*domains [MAX_DOMAINS + 1];
static unsigned domain_get_enable;
static unsigned	ndomains;

enum mem_block_en {
	MB_DOMAIN,		/* Domain + participant. */
	MB_DPARTICIPANT,	/* Discovered participant. */
	MB_TYPE,		/* Type. */
	MB_TOPIC,		/* Topic. */
	MB_FILTER_TOPIC,	/* Filtered Topic. */
	MB_PUBLISHER,		/* Publisher. */
	MB_SUBSCRIBER,		/* Subscriber. */
	MB_WRITER,		/* Writer. */
	MB_READER,		/* Reader. */
	MB_DWRITER,		/* Discovered Writer. */
	MB_DREADER,		/* Discovered Reader. */
	MB_GUARD,		/* Guard context (Liveliness/Deadline). */
	MB_PREFIX,		/* GUID prefix node. */

	MB_END
};

static const char *mem_names [] = {
	"DOMAIN",
	"DPARTICIPANT",
	"TYPE",
	"TOPIC",
	"FILTER_TOPIC",
	"PUBLISHER",
	"SUBSCRIBER",
	"WRITER",
	"READER",
	"DWRITER",
	"DREADER",
	"GUARD",
	"PREFIX",
};

#ifdef _WIN32
#define VARSIZE	1000000
#else
#define VARSIZE
#endif


static unsigned	min_entities;			/* Min. # of entities. */
static unsigned	cur_entities;			/* Current table size. */
static unsigned max_entities;			/* Max. # of entities. */

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory used by driver. */
static size_t		mem_size;		/* Total memory allocated. */

static lock_t		entities_lock;			/* Handles/entities. */
static Entity_t 	*(*dds_entities) [VARSIZE];	/* Entity table. */
static void		*handles;			/* Endpoint handles. */

int dds_listener_state;				/* Currently in a listener. */

/* domain_pool_init -- Initialize the DDS domain subsystem. */

int domain_pool_init (DomainCfg_t *cp)
{
	POOL_LIMITS	limits;

	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		memset (domains, 0, sizeof (domains));
		ndomains = 0;
		memset (dds_entities, 0, sizeof (Entity_t *) * cur_entities);
		handle_reset (handles);
		return (DDS_RETCODE_OK);
	}

	/* Check some builtin resource constraints. */
	if (cp->ndomains > MAX_DOMAINS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	/* Define the different pool attributes. */
	min_entities = cp->ndomains + cp->dparticipants.reserved +
		       cp->topics.reserved +
		       cp->filter_topics.reserved +
		       cp->publishers.reserved +
		       cp->subscribers.reserved +
		       cp->writers.reserved +
		       cp->readers.reserved +
		       cp->dreaders.reserved +
		       cp->dwriters.reserved;
	if (cp->dparticipants.extra == ~0U ||
	    cp->topics.extra == ~0U ||
	    cp->filter_topics.extra == ~0U ||
	    cp->publishers.extra == ~0U ||
	    cp->subscribers.extra == ~0U ||
	    cp->writers.extra == ~0U ||
	    cp->readers.extra == ~0U ||
	    cp->dwriters.extra == ~0U ||
	    cp->dreaders.extra == ~0U)
		max_entities = ~0U;
	else
		max_entities = cp->dparticipants.extra + cp->topics.extra +
			       cp->filter_topics.extra +
			       cp->publishers.extra + cp->subscribers.extra +
			       cp->writers.extra + cp->readers.extra +
			       cp->dwriters.extra + cp->dreaders.extra;
	log_printf (DOM_ID, 0, "Entities: reserved=%u, maximum=%u\r\n", min_entities, max_entities);
	limits.reserved = cp->ndomains;
	limits.extra = MAX_DOMAINS;
	limits.grow = 0;
	MDS_POOL_TYPE (mem_blocks, MB_DOMAIN, limits, sizeof (Domain_t));
	MDS_POOL_TYPE (mem_blocks, MB_DPARTICIPANT, cp->dparticipants,
						sizeof (Participant_t));
	MDS_POOL_TYPE (mem_blocks, MB_TYPE, cp->types, sizeof (TopicType_t));
	MDS_POOL_TYPE (mem_blocks, MB_TOPIC, cp->topics, sizeof (Topic_t));
	MDS_POOL_TYPE (mem_blocks, MB_FILTER_TOPIC, cp->filter_topics, sizeof (FilteredTopic_t));
	MDS_POOL_TYPE (mem_blocks, MB_PUBLISHER, cp->publishers,
						sizeof (Publisher_t));
	MDS_POOL_TYPE (mem_blocks, MB_SUBSCRIBER, cp->subscribers,
						sizeof (Subscriber_t));
	MDS_POOL_TYPE (mem_blocks, MB_WRITER, cp->writers, sizeof (Writer_t));
	MDS_POOL_TYPE (mem_blocks, MB_READER, cp->readers, sizeof (Reader_t));
	MDS_POOL_TYPE (mem_blocks, MB_DWRITER, cp->dwriters,
						   sizeof (DiscoveredWriter_t));
	MDS_POOL_TYPE (mem_blocks, MB_DREADER, cp->dreaders,
						   sizeof (DiscoveredReader_t));
	MDS_POOL_TYPE (mem_blocks, MB_GUARD, cp->guards, sizeof (Guard_t));
	MDS_POOL_TYPE (mem_blocks, MB_PREFIX, cp->prefixes, sizeof (RemPrefix_t));

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		warn_printf ("domain_init: not enough memory available!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	log_printf (DOM_ID, 0, "DDOM: dom_init: %lu bytes allocated for pools.\r\n", (unsigned long) mem_size);
#endif

	/* Allocate the entity handles. */
	handles = handle_init (min_entities);
	if (!handles) {
		warn_printf ("domain_init: not enough memory for publication handles!\r\n");
		mds_free (mem_blocks, MB_END);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Initialize the entity array. */
	cur_entities = min_entities;
	dds_entities = xmalloc ((cur_entities + 1) * sizeof (Entity_t *));
	if (!dds_entities) {
		warn_printf ("domain_init: not enough memory for entity table!\r\n");
		handle_final (handles);
		mds_free (mem_blocks, MB_END);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	dds_listener_state = 0;
	lock_init_nr (entities_lock, "Entities/Handles");

	return (DDS_RETCODE_OK);
}

/* domain_pool_final -- Finalize domain processing. */

void domain_pool_final (void)
{
	xfree (dds_entities);
	handle_final (handles);
	lock_destroy (entities_lock);
	mds_free (mem_blocks, MB_END);
}

/* entity_ptr -- Get a pointer to an entity based on the entity handle. */

Entity_t *entity_ptr (DDS_HANDLE handle)
{
	Entity_t	*p;

	if (handle <= DDS_HANDLE_NIL || handle > cur_entities)
		return (NULL);
	else {
		p = (*dds_entities) [handle];
		return (p);
	}
}

/* entity_participant -- Get a pointer to the participant of a writer, based on
			 its publication handle. */

Participant_t *entity_participant (DDS_HANDLE handle)
{
	Entity_t	*ep;
	Participant_t	*pp;

	if (handle <= DDS_HANDLE_NIL || handle > cur_entities)
		return (NULL);

	ep = (*dds_entities) [handle];
	if (!ep) {
		warn_printf ("entity_participant: unknown handle (%u)!", handle);
		return (NULL);
	}
	switch (entity_type (ep)) {
		case ET_PARTICIPANT:
			pp = (Participant_t *) ep;
			break;

		case ET_TOPIC:
			pp = NULL;
			break;

		case ET_PUBLISHER:
			pp = &((Publisher_t *) ep)->domain->participant;
			break;

		case ET_SUBSCRIBER:
			pp = &((Subscriber_t *) ep)->domain->participant;
			break;

		case ET_WRITER:
			if (entity_discovered (ep->flags))
				pp = ((Endpoint_t *) ep)->u.participant;
			else
				pp = &((Writer_t *) ep)->w_publisher->domain->participant;
			break;

		case ET_READER:
			if (entity_discovered (ep->flags))
				pp = ((Endpoint_t *) ep)->u.participant;
			else
				pp = &((Reader_t *) ep)->r_subscriber->domain->participant;
			break;

		default:
			pp = NULL;
	}
	return (pp);
}

/* handle_assign -- Assign a handle to a DDS Entity. */

DDS_HANDLE handle_assign (Entity_t *ep)
{
	unsigned	h, n;

	lock_take (entities_lock);
	h = handle_alloc (handles);
	if (!h) {
		n = cur_entities + min_entities;
		if (n > max_entities)
			n = max_entities;
		n -= cur_entities;
		if (!n) {
			fatal_printf ("DDOM: Entity limit reached (1)!");
			return (DDS_HANDLE_NIL);
		}
		handles = handle_extend (handles, n);
		if (!handles) {
			fatal_printf ("DDOM: Entity limit reached (2)!");
			return (DDS_HANDLE_NIL);
		}
		h = handle_alloc (handles);
		if (!h) {
			fatal_printf ("Impossible to allocate a handle!");
			return (DDS_HANDLE_NIL);
		}
		dds_entities = xrealloc (dds_entities,
		       (cur_entities + 1 + n) * sizeof (Entity_t *));
		if (!dds_entities) {
			fatal_printf ("Impossible to extend entity table!");
			return (DDS_HANDLE_NIL);
		}
		memset (&(*dds_entities) [cur_entities + 1], 0,
					sizeof (Entity_t *) * n);
		cur_entities += n;
	}
	(*dds_entities) [h] = ep;
	ep->handle = h;
	lock_release (entities_lock);
	return (h);
}

void handle_unassign (Entity_t *ep, int free_handle)
{
	unsigned	h;

	lock_take (entities_lock);
	h = entity_handle (ep);
	ep->handle = 0;
	if (h < 1 || h > cur_entities || (*dds_entities) [h] != ep) {
		warn_printf ("DDOM: Invalid handle (%u)!", h);
		lock_release (entities_lock);
		return;
	}
	(*dds_entities) [h] = NULL;
	if (free_handle)
		handle_free (handles, h);
	lock_release (entities_lock);
}

void handle_done (handle_t h)
{
	lock_take (entities_lock);
	handle_free (handles, h);
	lock_release (entities_lock);
}

/* domain_lookup -- Lookup a Domain based on the Domain Identifier. */

Domain_t *domain_lookup (DomainId_t id)
{
	unsigned	i;

	for (i = 1; i <= MAX_DOMAINS; i++)
		if (domains [i] && domains [i]->domain_id == id)
			return (domains [i]);

	return (NULL);
}

/* Lookup a Domain based on a GUID prefix. */

Domain_t *domain_from_prefix (const GuidPrefix_t *prefix)
{
	Domain_t	*dp;
	unsigned	i;

	for (i = 1; i <= MAX_DOMAINS; i++)
		if ((dp = domains [i]) != NULL &&
		    !memcmp (&dp->participant.p_guid_prefix,
		    	     prefix, 
			     GUIDPREFIX_SIZE))
			return (dp);

	return (NULL);
}


/* domain_assign -- Assign a domain index. */

static int domain_assign (Domain_t *dp)
{
	int i;

	dds_lock_domains ();
	for (i = 1; i <= MAX_DOMAINS; i++)
		if (!domains [i])
			break;

	if (i > MAX_DOMAINS) {
		dds_unlock_domains ();
		return (0);
	}
	domains [i] = dp;
	ndomains++;
	domain_get_enable |= 1 << i;
	dds_unlock_domains ();

	return (i);
}

/* domain_create -- Create a new Domain participant for the given Domain
		    Identifier.  */

Domain_t *domain_create (DomainId_t id)
{
	Domain_t	*dp;
#ifdef LOCK_TRACE
	char		lock_str [48];
#endif

	if (ndomains >= MAX_DOMAINS ||
	    (dp = mds_pool_alloc (&mem_blocks [MB_DOMAIN])) == NULL) {
		log_printf (DOM_ID, 0, "DDOM: domain_create (%u): out of memory!\r\n", id);
		return (NULL);
	}
	memset (dp, 0, sizeof (Domain_t));
	dp->participant.p_handle = 0;
	dp->participant.p_type = ET_PARTICIPANT;
	dp->participant.p_flags = EF_LOCAL;
	handle_assign (&dp->participant.p_entity);
	dp->participant.p_domain = dp;
	dp->domain_id = id;
	dp->participant.p_builtins = 0;
	locator_list_init (dp->participant.p_def_ucast);
	locator_list_init (dp->participant.p_def_mcast);
	locator_list_init (dp->participant.p_meta_ucast);
	locator_list_init (dp->participant.p_meta_mcast);
	locator_list_init (dp->dst_locs);
	dp->participant.p_man_liveliness = 0;
	dp->participant.p_user_data = NULL;
	memset (&dp->participant.p_lease_duration, 0, sizeof (Duration_t));
	sl_init (&dp->participant.p_endpoints, sizeof (Endpoint_t *));
	sl_init (&dp->participant.p_topics, sizeof (Topic_t *));
	tmr_init (&dp->participant.p_timer, "Participant");
	dp->participant.p_liveliness = NULL;
	memset (&dp->participant.p_builtin_ep, 0, sizeof (dp->participant.p_builtin_ep));
	sl_init (&dp->types, sizeof (TopicType_t *));
	sl_init (&dp->peers, sizeof (Participant_t *));
	dp->publishers.head = dp->publishers.tail = NULL;
	dp->subscribers.head = dp->subscribers.tail = NULL;
	dp->condition = NULL;
	LIST_INIT (dp->prefixes);

#ifdef LOCK_TRACE
	sprintf (lock_str, "participant(%u)", id);
#endif
	lock_init_r (dp->lock, lock_str);
	dp->index = domain_assign (dp);
	lock_take (dp->lock);

	/* Clear the pl_cdr cache. */
	pl_cache_reset ();

	return (dp);
}

/* domain_used -- Previous Domain Participant was already used. */

void domain_used (Domain_t *dp)
{
	dp->participant_id = guid_new_participant (
					&dp->participant.p_guid_prefix,
					dp->domain_id);
}

/* domain_detach -- Detach a domain participant from the list of participants.*/

void domain_detach (Domain_t *dp)
{
	unsigned	i;

	guid_free_participant (dp->participant_id);

	for (i = 1; i <= MAX_DOMAINS; i++)
		if (domains [i] == dp)
			break;

	if (i > MAX_DOMAINS) {
		warn_printf ("domain_delete: no such domain!");
		return;
	}
	domains [i] = NULL;

	/* Free the entity handle. */
	handle_unassign (&dp->participant.p_entity, 1);
	ndomains--;
	memset (&dp->participant.p_entity, 0, sizeof (Entity_t));
	lock_release (dp->lock);
	lock_destroy (dp->lock);
}

/* domain_delete -- Delete a domain participant. */

int domain_delete (Domain_t *dp)
{
	/* Free the domain data structure. */
	mds_pool_free (&mem_blocks [MB_DOMAIN], dp);

	return (DDS_RETCODE_OK);
}

/* domain_ptr -- Lookup a domain based on its index. */

Domain_t *domain_ptr (void *p, int lock, DDS_ReturnCode_t *error)
{
	Domain_t	*dp = (Domain_t *) p;

	if (!dp ||
	    ((uintptr_t) dp & (sizeof (uintptr_t) - 1)) != 0 ||
	    dp->participant.p_type != ET_PARTICIPANT ||
	    domains [dp->index] != dp) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	if (lock) {
		if (lock_take (dp->lock)) {
			if (error)
				*error = DDS_RETCODE_ALREADY_DELETED;
			return (NULL);
		}
		else if (!dp->participant.p_flags) { /* Shutting down. */
			lock_release (dp->lock);
			return (NULL);
		}
	}
	if (error)
		*error = DDS_RETCODE_OK;

	return (dp);
}

/* domain_get -- Get a configured domain by its index. */

Domain_t *domain_get (unsigned index, int lock, DDS_ReturnCode_t *error)
{
	Domain_t	*dp;

	if (!index || index > MAX_DOMAINS) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (lock)
		dds_lock_domains ();
	if ((dp = domains [index]) == NULL) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;

		goto no_domain;
	}
	if ((domain_get_enable & (1 << index)) == 0) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		dp = NULL;
		goto no_domain;
	}
	if (lock && lock_take (dp->lock)) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		dp = NULL;
		goto no_domain;
	}
	if (error)
		*error = DDS_RETCODE_OK;

    no_domain:
	if (lock)
		dds_unlock_domains ();
	return (dp);
}

/* domain_close -- Closedown a configured domain. */

void domain_close (unsigned index)
{
#ifdef RTPS_USED
	int	active;

#endif
	dds_lock_domains ();
	domain_get_enable &= ~(1 << index);
	dds_unlock_domains ();
#ifdef RTPS_USED
	for (;;) {
		active = atomic_get_w (rtps_rx_active);
		if (!active)
			break;

		thread_yield ();
	}
#endif
}

/* domain_next -- Get the next configured domain. */

Domain_t *domain_next (unsigned *index, DDS_ReturnCode_t *error)
{
	Domain_t	*dp = NULL;

	dds_lock_domains ();
	for (;;) {
		if (++(*index) > MAX_DOMAINS) {
			if (error)
				*error = DDS_RETCODE_BAD_PARAMETER;
			break;
		}
		if (*index >= 1 && domains [*index] && 
		    (domain_get_enable & (1 << *index)) != 0) {
			dp = domains [*index];
			break;
		}
	}
	dds_unlock_domains ();
	return (dp);
}

unsigned domain_count (void)
{
	unsigned	i, n;

	dds_lock_domains ();
	for (i = 1, n = 0; i <= MAX_DOMAINS; i++)
		if (domains [i])
			n++;
	dds_unlock_domains ();
	return (n);
}

int domains_used (void)
{
    return (ndomains != 0);
}

/* prefix_compare -- Comparison function for prefixes in peer participants. */

static int prefix_compare (const void *np, const void *data)
{
	const Participant_t	**prp = (const Participant_t **) np;
	const GuidPrefix_t	*gpp = (const GuidPrefix_t *) data;

	return (memcmp ((*prp)->p_guid_prefix.prefix, gpp->prefix, 12));
}

/* participant_lookup -- Lookup a remote participant, based on the prefix. */

Participant_t *participant_lookup (const Domain_t *dp, const GuidPrefix_t *prefix)
{
	Participant_t	**ppp;

	ppp = sl_search (&dp->peers, prefix, prefix_compare);
	if (!ppp)
		return (NULL);

	return (*ppp);
}

/* participant_create -- Create a remote participant, based on the prefix. */

Participant_t *participant_create (Domain_t           *dp,
				   const GuidPrefix_t *prefix,
				   int                *new)
{
	Participant_t	**ppp, *pp;
	int		new_peer;
	Participant_t	temp_participant;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Participant Create (%04x:%04x:%04x) -> ",
				ntohl (prefix->w [0]),
				ntohl (prefix->w [1]),
				ntohl (prefix->w [2]));
#endif
	ppp = sl_insert (&dp->peers, prefix, &new_peer, prefix_compare);
	if (!ppp) {
#ifdef LOG_DOMAIN
		log_printf (DOM_ID, 0, "out of memory for list node!\r\n");
#endif
		return (NULL);
	}
	if (new)
		*new = new_peer;
	if (!new_peer)
		return (*ppp);

	if ((pp = mds_pool_alloc (&mem_blocks [MB_DPARTICIPANT])) == NULL) {
#ifdef LOG_DOMAIN
		log_printf (DOM_ID, 0, "out of memory!\r\n");
#endif
		temp_participant.p_guid_prefix = *prefix;
		*ppp = &temp_participant;
		sl_delete (&dp->peers, prefix, prefix_compare);
		return (NULL);
	}
	pp->p_type = ET_PARTICIPANT;
	pp->p_flags = EF_REMOTE;
	handle_assign (&pp->p_entity);
	pp->p_domain = dp;
	pp->p_guid_prefix = *prefix;
	sl_init (&pp->p_endpoints, sizeof (Endpoint_t *));
	sl_init (&pp->p_topics, sizeof (Topic_t *));
	tmr_init (&pp->p_timer, "Participant");
	pp->p_liveliness = NULL;
	pp->p_src_locators = NULL;
	memset (&pp->p_builtin_ep, 0, sizeof (pp->p_builtin_ep));
	*ppp = pp;

	/* Clear the pl_cdr cache. */
	pl_cache_reset ();

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "%p\r\n", (void *) pp);
#endif
	return (pp);
}

/* participant_delete -- Delete a previously created participant. */

int participant_delete (Domain_t *dp, Participant_t *pp)
{
#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Participant Delete (%p)\r\n", (void *) pp);
#endif
	/* Remove the peer participant node. */
	sl_delete (&dp->peers, pp->p_guid_prefix.prefix, prefix_compare);

	/* Free the entity handle if it is not in a builtin reader cache */
	handle_unassign (&pp->p_entity, !entity_cached (pp->p_flags));

	/* Remove the peer participant information. */
	mds_pool_free (&mem_blocks [MB_DPARTICIPANT], pp);
	return (DDS_RETCODE_OK);
}


/* type_name_cmp -- Compare a type name in skiplist nodes. */

static int type_name_cmp (const void *np, const void *data)
{
	const TopicType_t	**tpp = (const TopicType_t **) np;
	const char		*str = (const char *) data;

	return (strcmp (str_ptr ((*tpp)->type_name), str));
}

/* type_lookup -- Lookup a type name within a domain. */

TopicType_t *type_lookup (Domain_t *dp, const char *type_name)
{
	TopicType_t	**typepp;

	typepp = sl_search (&dp->types, type_name, type_name_cmp);
	return (typepp ? *typepp : NULL);
}

/* type_create -- Create a new type.  On completion, caller needs to fill all
		  fields except name. */

TopicType_t *type_create (Domain_t *dp, const char *type_name, int *new)
{
	TopicType_t	**typepp, *typep;
	int		new_type;
	String_t	*str, temp_str;
	TopicType_t	temp_type;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Type Create (%s)", type_name);
#endif
	typepp = sl_insert (&dp->types, type_name, &new_type, type_name_cmp);
	if (!typepp) {
#ifdef LOG_DOMAIN
		log_printf (DOM_ID, 0, " - no memory!\r\n");
#endif
		return (NULL);
	}
	if (new)
		*new = new_type;
	if (!new_type) {
#ifdef LOG_DOMAIN
		log_printf (DOM_ID, 0, " - already exists!\r\n");
#endif
		return (*typepp);
	}
	str = str_new_cstr (type_name);
	if (!str)
		goto free_type;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, " - new type!\r\n");
#endif
	typep = mds_pool_alloc (&mem_blocks [MB_TYPE]);
	if (!typep) {
		warn_printf ("type_create (%s): out of memory for type!\r\n", type_name);
		str_unref (str);
		goto free_type;
	}
	typep->flags = 0;
	typep->nrefs = 0;
	typep->nlrefs = 0;
	typep->type_name = str;
	typep->type_support = NULL;
	*typepp = typep;
	return (typep);

    free_type:
	temp_type.type_name = &temp_str;
	if (strlen (type_name) < STRD_SIZE)
		memcpy (&temp_str.u.b, type_name, strlen (type_name) + 1);
	else
		temp_str.u.dp = type_name;
	*typepp = &temp_type;
    	sl_delete (&dp->types, type_name, type_name_cmp);
	return (NULL);
}

/* type_delete -- Delete a previously created type. */

int type_delete (Domain_t *dp, TopicType_t *type)
{
#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Type Delete (%s)\r\n", str_ptr (type->type_name));
#endif
	if (--type->nrefs)
		return (DDS_RETCODE_OK);

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Type removed completely!\r\n");
#endif
	sl_delete (&dp->types, str_ptr (type->type_name), type_name_cmp);
	str_unref (type->type_name);
#ifdef DDS_TYPECODE
	if (type->type_support &&
	    type->type_support->ts_origin == TSO_Typecode)
		vtc_delete (type->type_support);
#endif
	mds_pool_free (&mem_blocks [MB_TYPE], type);
	return (DDS_RETCODE_OK);
}

/* topic_name_cmp -- Compare a topic name in skiplist nodes. */

static int topic_name_cmp (const void *np, const void *data)
{
	const Topic_t	**tpp = (const Topic_t **) np;
	const char	*str = (const char *) data;

	return (strcmp (str_ptr ((*tpp)->name), str));
}

/* topic_lookup -- Lookup a topic with the given name. */

Topic_t *topic_lookup (const Participant_t *pp,
		       const char          *topic_name)
{
	Topic_t		**tpp;

	/* Check if topic with that name already exists. */
	tpp = sl_search (&pp->p_topics, topic_name, topic_name_cmp);
	return (tpp ? *tpp : NULL);
}

/* topic_create -- Create a topic with the given name and type name. */

Topic_t *topic_create (Participant_t *pp,
		       Topic_t       *tp,
		       const char    *topic_name,
		       const char    *type_name,
		       int           *new)
{
	Domain_t	*dp = pp->p_domain;
	Topic_t		**tpp, **ptpp;
	TopicType_t	**typepp, *typep;
	int		new_type, new_topic;
#ifdef LOCK_TRACE
	char		*lock_str;
#endif

	/* Check if topic with that name already exists in the domain. */
#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Topic Create (Participant=%p, Topic=%s)\r\n", (void *) pp, topic_name);
#endif
	if (!tp) {
		tpp = sl_search (&dp->participant.p_topics, topic_name, topic_name_cmp);
		if (tpp)
			tp = *tpp;
	}
	if (tp) {
		if (!strcmp (str_ptr (tp->type->type_name), type_name)) {
			lock_take (tp->lock);

			/* Found it in domain. */
			if (entity_discovered (pp->p_flags)) {
#ifdef LOG_DOMAIN
				log_printf (DOM_ID, 0, "DDOM: Topic exists: add to participant list.\r\n");
#endif
				ptpp = sl_insert (&pp->p_topics, topic_name, &new_topic, topic_name_cmp);

				if (!ptpp) {
                                        lock_release (tp->lock);
					return (NULL);
                                }

				if (new)
					*new = new_topic;

				if (new_topic) {
					tp->entity.flags |= EF_REMOTE;
					tp->nrrefs++;
					*ptpp = tp;
				}
			}
			else {
				if ((tp->entity.flags & EF_LOCAL) == 0)
					tp->entity.flags |= EF_LOCAL;
				if (new)
					*new = 0;
				tp->nlrefs++;
			}
			lock_release (tp->lock);
			return (tp);
		}
		else {
			log_printf (DOM_ID, 0, "topic_create(%s): incompatible type name!\r\n", topic_name);
			dcps_inconsistent_topic (tp);
			return (NULL);
		}
	}

	/* Check if type exists. */
	typepp = sl_search (&dp->types, type_name, type_name_cmp);
	if (!typepp) {
		if (!entity_discovered (pp->p_flags)) {
			log_printf (DOM_ID, 0, "topic_create(%s): type with that name (%s) not registered.\r\n", topic_name, type_name);
			return (NULL);
		}
		typep = type_create (dp, type_name, &new_type);
		if (!typep)
			return (NULL);

		typep->type_support = NULL;
	}
	else
		typep = *typepp;
	typep->nrefs++;

	/* New topic: create topic context. */
	tp = mds_pool_alloc (&mem_blocks [MB_TOPIC]);
	if (!tp) {
		warn_printf ("topic_create (%s): out of memory for topic!\r\n", topic_name);
		goto free_type;
	}
	tp->entity.type = ET_TOPIC;
	tp->entity.flags = 0;
	handle_assign (&tp->entity);
	tp->domain = dp;
	tp->type = typep;
	tp->qos = NULL;
	tp->readers = NULL;
	tp->writers = NULL;
#ifdef LOCK_TRACE
	lock_str = malloc (strlen (topic_name) + 16);
	sprintf (lock_str, "#%u:T:%s", entity_handle (&tp->entity), topic_name);
#endif
	lock_init_nr (tp->lock, lock_str);
	tp->filters = NULL;
	tp->status = tp->mask = 0;
	tp->condition = NULL;
	memset (&tp->listener, 0, sizeof (tp->listener));
	memset (&tp->inc_status, 0, sizeof (tp->inc_status));

	/* Set topic name. */
	tp->name = str_new_cstr (topic_name);
	if (!tp->name) {
		warn_printf ("topic_create (%s): out of memory for topic name!\r\n", topic_name);
		goto free_topic;
	}

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: New topic: add to domain list.\r\n");
#endif
	tpp = sl_insert (&dp->participant.p_topics, topic_name, new, topic_name_cmp);
	if (!tpp)
		goto free_topic_str;

	*tpp = tp;
	if (entity_discovered (pp->p_flags)) {

		/* Create topic at participant. */
#ifdef LOG_DOMAIN
		log_printf (DOM_ID, 0, "DDOM: New topic: add to participant list.\r\n");
#endif
		ptpp = sl_insert (&pp->p_topics, topic_name, &new_topic, topic_name_cmp);
		if (!ptpp)
			goto free_domain_topic;

		tp->entity.flags |= EF_REMOTE;
		tp->nrrefs = 1;
		tp->nlrefs = 0;
		*ptpp = tp;
	}
	else {
		tp->entity.flags |= EF_LOCAL;
		tp->nlrefs = 1;
		tp->nrrefs = 0;
	}
	return (tp);

    free_domain_topic:
    	sl_delete (&dp->participant.p_topics, topic_name, topic_name_cmp);

    free_topic_str:
	str_unref (tp->name);

    free_topic:
	handle_unassign (&tp->entity, 1);
	tp->entity.type = 0;
	tp->entity.flags = 0;
	lock_destroy (tp->lock);
    	mds_pool_free (&mem_blocks [MB_TOPIC], tp);

    free_type:
	type_delete (dp, typep);

    	return (NULL);
}

int topic_endpoints_from_participant (Participant_t *pp, Topic_t *tp)
{
	Endpoint_t	*ep;

	for (ep = tp->readers; ep; ep = ep->next)
		if (entity_discovered (ep->entity.flags) && ep->u.participant == pp)
			return (1);

	for (ep = tp->writers; ep; ep = ep->next)
		if (entity_discovered (ep->entity.flags) && ep->u.participant == pp)
			return (1);

	return (0);
}

/* topic_delete -- Delete a previously created topic. */

int topic_delete (Participant_t *pp, Topic_t *tp, int *last_ep, int *gone)
{
	Domain_t	*dp = pp->p_domain;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Topic Delete (Participant=%p, Topic=%s)\r\n", (void *) pp, str_ptr (tp->name));
#endif
	if (last_ep)
		*last_ep = 0;
	if (gone)
		*gone = 0;
	if (entity_discovered (pp->p_flags)) {
		tp->nrrefs--;
		if (!topic_endpoints_from_participant (pp, tp)) {
#ifdef LOG_DOMAIN
			log_printf (DOM_ID, 0, "DDOM: Topic removed from participant list.\r\n");
#endif
			if (last_ep)
				*last_ep = 1;
			sl_delete (&pp->p_topics, str_ptr (tp->name), topic_name_cmp);
			if (!tp->nrrefs)
				tp->entity.flags &= ~EF_REMOTE;
		}
	}
	else if (!--tp->nlrefs) {
		if (last_ep)
			*last_ep = 1;
		tp->entity.flags &= ~EF_LOCAL;
	}
	if (tp->nrrefs || tp->nlrefs || tp->writers || tp->readers) {
		lock_release (tp->lock);
		return (DDS_RETCODE_OK);
	}
	if (gone)
		*gone = 1;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Topic removed completely.\r\n");
#endif
	/* Delete the topic type. */
	type_delete (dp, tp->type);

	/* Remove topic from topic list. */
	sl_delete (&dp->participant.p_topics, str_ptr (tp->name), topic_name_cmp);

	/* Delete the topic contents. */
	str_unref (tp->name);

	/* Delete the topic QoS parameters. */
	qos_topic_free (tp->qos);

	/* Free the entity handle if it is not in a builtin reader cache */
	handle_unassign (&tp->entity, !entity_cached (tp->entity.flags));
	tp->entity.type = 0;
	tp->entity.flags = 0;

	/* Destroy the lock. */
	lock_release (tp->lock);
	lock_destroy (tp->lock);

	mds_pool_free (&mem_blocks [MB_TOPIC], tp);

	return (DDS_RETCODE_OK);
}

/* filtered_topic_create -- Create a content-filtered topic based on an existing
			    topic. */

FilteredTopic_t *filtered_topic_create (Domain_t   *dp,
					Topic_t    *tp,
					const char *name)
{
	FilteredTopic_t	*ftp, **tpp;
#ifdef LOCK_TRACE
	char		*lock_str;
#endif
	int		new;

	tpp = sl_search (&dp->participant.p_topics, name, topic_name_cmp);
	if (tpp)
		return (NULL);	/* Name already exists for topic. */

	/* New topic: create filtered topic context. */
	ftp = mds_pool_alloc (&mem_blocks [MB_FILTER_TOPIC]);
	if (!ftp) {
		warn_printf ("filtered_topic_create (%s): out of memory for topic!\r\n", name);
		return (NULL);
	}
	ftp->topic.entity.type = ET_TOPIC;
	ftp->topic.entity.flags = EF_FILTERED | EF_LOCAL;
	if ((tp->entity.flags & EF_ENABLED) != 0)
		ftp->topic.entity.flags |= EF_ENABLED | EF_NOT_IGNORED;

	handle_assign ((Entity_t *) ftp);
	ftp->topic.domain = dp;
	ftp->topic.type = tp->type;
	tp->type->nrefs++;
	ftp->topic.qos = tp->qos;
	tp->qos->users++;
	ftp->topic.readers = NULL;
	ftp->topic.writers = NULL;
#ifdef LOCK_TRACE
	lock_str = malloc (strlen (name) + 16);
	sprintf (lock_str, "#%u:FT:%s", entity_handle (&ftp->topic.entity), name);
#endif
	lock_init_nr (ftp->topic.lock, lock_str);
	memset (&ftp->topic.listener, 0, sizeof (ftp->topic.listener));
	ftp->topic.status = ftp->topic.mask = 0;
	ftp->topic.condition = NULL;

	/* Set topic name. */
	ftp->topic.name = str_new_cstr (name);
	if (!ftp->topic.name) {
		warn_printf ("topic_create (%s): out of memory for topic name!\r\n", name);
		goto free_topic;
	}

	/* Add to list of domain topics. */
	tpp = sl_insert (&dp->participant.p_topics, name, &new, topic_name_cmp);
	if (!tpp)
		goto free_topic_str;

	*tpp = ftp;
	ftp->topic.nlrefs = 1;
	ftp->topic.nrrefs = 0;

	/* Add to parent topic list. */
	ftp->topic.filters = NULL;
	ftp->related = tp;
	ftp->next = tp->filters;
	tp->filters = ftp;
	return (ftp);

    free_topic_str:
	str_unref (ftp->topic.name);

    free_topic:
	handle_unassign (&ftp->topic.entity, 1);
	ftp->topic.entity.type = 0;
	ftp->topic.entity.flags = 0;
	lock_destroy (ftp->topic.lock);
    	mds_pool_free (&mem_blocks [MB_FILTER_TOPIC], ftp);
	tp->type->nrefs--;
    	return (NULL);
}

/* filtered_topic_delete -- Delete a content-filtered topic. */

int filtered_topic_delete (FilteredTopic_t *ftp)
{
	Domain_t	*dp = ftp->topic.domain;
	FilteredTopic_t	*xftp, *prev_ftp;
	Topic_t		*tp;

	if (ftp->topic.nlrefs)
		ftp->topic.nlrefs--;

	if (ftp->topic.nlrefs || ftp->topic.writers || ftp->topic.readers)
		return (DDS_RETCODE_OK);

	/* Remove from parent topic list. */
	tp = ftp->related;
	for (xftp = tp->filters, prev_ftp = NULL;
	     xftp && xftp != ftp;
	     prev_ftp = xftp, xftp = xftp->next)
	    	;
 
 	if (xftp) {
		if (prev_ftp)
			prev_ftp->next = xftp->next;
		else
			tp->filters = xftp->next;
	}

	/* Delete the topic type. */
	type_delete (dp, ftp->topic.type);

	/* Remove topic from topic list. */
	sl_delete (&dp->participant.p_topics, str_ptr (ftp->topic.name), topic_name_cmp);

	/* Delete the topic contents. */
	str_unref (ftp->topic.name);

	/* Delete the topic QoS parameters. */
	qos_topic_free (ftp->topic.qos);

	/* Delete the topic node. */
	handle_unassign (&ftp->topic.entity, 1);
	ftp->topic.entity.type = 0;
	ftp->topic.entity.flags = 0;
	lock_release (ftp->topic.lock);
	lock_destroy (ftp->topic.lock);
	mds_pool_free (&mem_blocks [MB_FILTER_TOPIC], ftp);

	return (DDS_RETCODE_OK);
}

/* filter_data_cleanup -- Delete all filter data. */

void filter_data_cleanup (FilterData_t *fp)
{
	if (fp->filter.name)
		str_unref (fp->filter.name);
	if (fp->filter.related_name)
		str_unref (fp->filter.related_name);
	if (fp->filter.class_name)
		str_unref (fp->filter.class_name);
	if (fp->filter.expression)
		str_unref (fp->filter.expression);
	if (fp->filter.expression_pars)
		strings_delete (fp->filter.expression_pars);
	if (fp->program.length)
		xfree (fp->program.buffer);

	bc_cache_flush (&fp->cache);
}


/* topic_ptr -- Return a valid topic pointer (result != NULL) or NULL and sets
		the error code appropriately. */

Topic_t *topic_ptr (void *p, int lock, DDS_ReturnCode_t *error)
{
	Topic_t	*tp = (Topic_t *) p;

	if (!tp || tp->entity.type != ET_TOPIC) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		log_printf (DOM_ID, 0, "topic_ptr: no topic or entity_type not correct\r\n");
		return (NULL);
	}
	if (lock) {
		if (lock_take (tp->lock)) {
			if (error)
				*error = DDS_RETCODE_ALREADY_DELETED;
			log_printf (DOM_ID, 0, "topic_ptr: topic Lock problem\r\n");
			return (NULL);
		}
		else if (!tp->entity.flags) {	/* Shutting down topic. */
			log_printf (DOM_ID, 0, "topic_ptr: no topic->fh\r\n");
			lock_release (tp->lock);
			return (NULL);
		}
	}
	if (error)
		*error = DDS_RETCODE_OK;
	return (tp);
}

/* publisher_create -- Create a new publisher. */

Publisher_t *publisher_create (Domain_t *dp, int builtin)
{
	Publisher_t	*pp;

	/* New publisher: create publisher context. */
	pp = mds_pool_alloc (&mem_blocks [MB_PUBLISHER]);
	if (!pp) {
		warn_printf ("publisher_create: out of memory for publisher!\r\n");
		return (NULL);
	}

	/* Setup Publisher-specific data. */
	pp->entity.type = ET_PUBLISHER;
	pp->entity.flags = EF_LOCAL;
	handle_assign ((Entity_t *) pp);
	pp->domain = dp;
	if (builtin) {
		pp->entity.flags |= EF_BUILTIN | EF_ENABLED | EF_NOT_IGNORED;
		dp->builtin_publisher = pp;
		pp->prev = NULL;
	}
	else {
		if (dp->publishers.head)
			dp->publishers.tail->next = pp;
		else
			dp->publishers.head = pp;
		pp->prev = dp->publishers.tail;
		dp->publishers.tail = pp;
	}
	pp->next = NULL;
	pp->nwriters = 0;
	memset (&pp->listener, 0, sizeof (pp->listener));
	pp->mask = 0;
	pp->condition = NULL;
	pp->suspended = NULL;
	return (pp);
}

/* publisher_delete -- Delete a previously created publisher. */

int publisher_delete (Publisher_t *pp)
{
	Domain_t	*dp;
	Publisher_t	*prev_pp, *xpp;

	dp = pp->domain;
	if ((pp->entity.flags & EF_BUILTIN) != 0) {
		if (pp != dp->builtin_publisher)
			return (DDS_RETCODE_ALREADY_DELETED);

		dp->builtin_publisher = NULL;
	}
	else {
		for (xpp = dp->publishers.head, prev_pp = NULL;
		     xpp;
		     prev_pp = xpp, xpp = xpp->next)
			if (xpp == pp)
				break;

		if (!xpp)
			return (DDS_RETCODE_ALREADY_DELETED);

		if (prev_pp)
			prev_pp->next = pp->next;
		else
			dp->publishers.head = pp->next;
		if (pp->next)
			pp->next->prev = pp->prev;
		else
			dp->publishers.tail = pp->prev;
	}
	handle_unassign (&pp->entity, 1);
	pp->entity.type = 0;
	pp->entity.flags = 0;
	mds_pool_free (&mem_blocks [MB_PUBLISHER], pp);
	return (DDS_RETCODE_OK);
}

Publisher_t *publisher_ptr (DDS_Publisher    publisher,
			    DDS_ReturnCode_t *error)
{
	Publisher_t	*up = (Publisher_t *) publisher;

	if (up == NULL || up->entity.type != ET_PUBLISHER) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (error)
		*error = DDS_RETCODE_OK;
	return (up);
}


/* subscriber_create -- Create a new subscriber. */

Subscriber_t *subscriber_create (Domain_t *dp, int builtin)
{
	Subscriber_t	*sp;

	/* New subscriber: create context. */
	sp = mds_pool_alloc (&mem_blocks [MB_SUBSCRIBER]);
	if (!sp) {
		warn_printf ("subscriber_create: out of memory for subscriber!\r\n");
		return (NULL);
	}

	/* Setup Subscriber-specific data. */
	sp->entity.type = ET_SUBSCRIBER;
	sp->entity.flags = EF_LOCAL;
	handle_assign ((Entity_t *) sp);
	sp->domain = dp;
	if (builtin) {
		sp->entity.flags |= EF_BUILTIN | EF_ENABLED | EF_NOT_IGNORED;
		dp->builtin_subscriber = sp;
		sp->prev = NULL;
	}
	else {
		if (dp->subscribers.head)
			dp->subscribers.tail->next = sp;
		else
			dp->subscribers.head = sp;
		sp->prev = dp->subscribers.tail;
		dp->subscribers.tail = sp;
	}
	sp->next = NULL;
	sp->nreaders = 0;
	memset (&sp->listener, 0, sizeof (sp->listener));
	sp->mask = sp->status = 0;
	sp->condition = NULL;
	sp->def_reader_qos = qos_def_reader_qos;

	return (sp);
}

/* subscriber_delete -- Delete a previously created subscriber. */

int subscriber_delete (Subscriber_t *sp)
{
	Domain_t	*dp;
	Subscriber_t	*prev_sp, *xsp;

	dp = sp->domain;
	if ((sp->entity.flags & EF_BUILTIN) != 0) {
		if (sp != dp->builtin_subscriber)
			return (DDS_RETCODE_ALREADY_DELETED);

		dp->builtin_subscriber = NULL;
	}
	else {
		for (xsp = dp->subscribers.head, prev_sp = NULL;
		     xsp;
		     prev_sp = xsp, xsp = xsp->next)
			if (xsp == sp)
				break;

		if (!xsp)
			return (DDS_RETCODE_ALREADY_DELETED);

		if (prev_sp)
			prev_sp->next = sp->next;
		else
			dp->subscribers.head = sp->next;
		if (sp->next)
			sp->next->prev = sp->prev;
		else
			dp->subscribers.tail = sp->prev;
	}
	handle_unassign (&sp->entity, 1);
	sp->entity.type = 0;
	sp->entity.flags = 0;
	mds_pool_free (&mem_blocks [MB_SUBSCRIBER], sp);
	return (DDS_RETCODE_OK);
}

Subscriber_t *subscriber_ptr (DDS_Subscriber   subscriber,
			      DDS_ReturnCode_t *error)
{
	Subscriber_t	*sp = (Subscriber_t *) subscriber;

	if (sp == NULL || sp->entity.type != ET_SUBSCRIBER) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (error)
		*error = DDS_RETCODE_OK;
	return (sp);
}


/* endpoint_id_cmp -- Compare a EntityId in skiplist nodes. */

static int endpoint_id_cmp (const void *np, const void *data)
{
	const Endpoint_t	**epp = (const Endpoint_t **) np;
	const unsigned char	*eid = (const unsigned char *) data;

	return (memcmp ((*epp)->entity_id.id, eid, sizeof (EntityId_t)));
}

/* endpoint_lookup -- Lookup an endpoint of a participant. */

Endpoint_t *endpoint_lookup (const Participant_t *pp, const EntityId_t *id)
{
	Endpoint_t	**epp;

	epp = sl_search (&pp->p_endpoints, id->id, endpoint_id_cmp);
	return (epp ? *epp : NULL);
}

enum mem_block_en endpoint_pool_type (const EntityId_t *id, int discovered)
{
	unsigned	kind;
	enum mem_block_en mtype;

	kind = id->id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR;
	if (kind == ENTITY_KIND_READER || kind == ENTITY_KIND_READER_KEY)
		if (discovered)
			mtype = MB_DREADER;
		else
			mtype = MB_READER;
	else if (kind == ENTITY_KIND_WRITER || kind == ENTITY_KIND_WRITER_KEY)
		if (discovered)
			mtype = MB_DWRITER;
		else
			mtype = MB_WRITER;
	else
		mtype = (enum mem_block_en) 0;
	return (mtype);
}

/* endpoint_create -- Create an endpoint with the given Entity Id. */

Endpoint_t *endpoint_create (Participant_t    *pp,
			     void             *parent,
			     const EntityId_t *id,
			     int              *new)
{
	Endpoint_t	*ep, **epp;
	int		new_ep;
	enum mem_block_en mtype;
	Endpoint_t	temp_ep;
#if defined (LOCK_TRACE) && (defined (READER_LOCKS) || defined (WRITER_LOCKS))
	char		lock_str [48];
#endif
	Reader_t	*rp;
	Writer_t	*wp;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Endpoint Create (Participant=%p, EID=%04x)\r\n", (void *) pp, ntohl (id->w));
#endif
	mtype = endpoint_pool_type (id, entity_discovered (pp->p_flags));
	if (!mtype)
		return (NULL);

	/* Add to parent endpoint list. */
	epp = sl_insert (&pp->p_endpoints, id->id, &new_ep, endpoint_id_cmp);
	if (!epp) {
		warn_printf ("endpoint_create: out of memory for endpoint node!");
		return (NULL);
	}
	if (new)
		*new = new_ep;
	if (!new_ep)
		return (*epp);

	ep = mds_pool_alloc (&mem_blocks [mtype]);
	if (!ep) {
		warn_printf ("endpoint_create: out of memory for endpoint!");
		*epp = &temp_ep;
		temp_ep.entity_id = *id;
		sl_delete (&pp->p_endpoints, id->id, endpoint_id_cmp);
		return (NULL);
	}
	switch (mtype) {
		case MB_DWRITER:
			ep->entity.type = ET_WRITER;
			ep->entity.flags = EF_REMOTE;
			ep->u.participant = pp;
			break;
		case MB_DREADER:
			ep->entity.type = ET_READER;
			ep->entity.flags = EF_REMOTE;
			ep->u.participant = pp;
			break;
		case MB_WRITER:
			ep->entity.type = ET_WRITER;;
			ep->entity.flags = EF_LOCAL;
			ep->u.publisher = parent;
			wp = (Writer_t *) ep;
#ifdef WRITER_LOCKS
#ifdef LOCK_TRACE
			sprintf (lock_str, "#%lu:W", entity_handle (&wp->w_entity));
#endif
			lock_init_nr (wp->w_lock, lock_str);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
			wp->w_crypto = 0;
#endif
			wp->w_status = 0;
			wp->w_condition = NULL;
			wp->w_guard = NULL;
			break;
		case MB_READER:
			ep->entity.type = ET_READER;
			ep->entity.flags = EF_LOCAL;
			ep->u.subscriber = parent;
			rp = (Reader_t *) ep;
#ifdef READER_LOCKS
#ifdef LOCK_TRACE
			sprintf (lock_str, "#%lu:R", entity_handle (&rp->r_entity));
#endif
			lock_init_nr (rp->r_lock, lock_str);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
			rp->r_crypto = 0;
#endif
			rp->r_status = 0;
			rp->r_conditions = NULL;
			rp->r_guard = NULL;
			break;
		default:
			break;
	}
	ep->entity_id = *id;
	ep->topic = NULL;
	ep->qos = NULL;
	locator_list_init (ep->ucast);
	locator_list_init (ep->mcast);
	ep->next = NULL;
	ep->rtps = NULL;
	*epp = ep;

	/* Assign an endpoint handle. */
	handle_assign (&ep->entity);
	return (ep);
}

/* endpoint_delete -- Delete a previously created endpoint. */

int endpoint_delete (Participant_t *pp, Endpoint_t *ep)
{
	enum mem_block_en mtype;

#ifdef LOG_DOMAIN
	log_printf (DOM_ID, 0, "DDOM: Endpoint Delete (Participant=%p, EID=%04x)\r\n", (void *) pp, ntohl (ep->entity_id.w));
#endif
	sl_delete (&pp->p_endpoints, ep->entity_id.id, endpoint_id_cmp);

	/* Free the entity handle if it is not in a builtin reader cache */
	handle_unassign (&ep->entity, !entity_cached (ep->entity.flags));
	mtype = endpoint_pool_type (&ep->entity_id, entity_discovered (pp->p_flags));
	ep->entity.type = 0;
	ep->entity.flags = 0;
#ifdef RW_LOCKS
	if (mtype == MB_WRITER) {
		lock_release (wp->w_lock);
		lock_destroy (wp->w_lock);
	}
	else if (mtype == MB_READER) {
		lock_release (rp->r_lock);
		lock_destroy (rp->r_lock);
	}
#endif
	mds_pool_free (&mem_blocks [mtype], ep);
	return (DDS_RETCODE_OK);
}

/* endpoint_from_guid -- Lookup an endpoint via its GUID. */

Endpoint_t *endpoint_from_guid (const Domain_t *dp, GUID_t *guid)
{
	const Participant_t	*pp;
	Endpoint_t		*ep;

	if (!memcmp (&dp->participant.p_guid_prefix,
					&guid->prefix, sizeof (GuidPrefix_t)))

		/* Local endpoint. */
		pp = &dp->participant;
	else {
		/* Remote endpoint. */
		pp = participant_lookup (dp, &guid->prefix);
		if (!pp)
			return (NULL);
	}
	ep = endpoint_lookup (pp, &guid->entity_id);
	return (ep);
}

/* reader_ptr -- Return a valid reader pointer (result != NULL) or returns NULL
		 and sets the error code appropriately. */

Reader_t *reader_ptr (DDS_DataReader r, int lock, DDS_ReturnCode_t *error)
{
	Reader_t	*rp = (Reader_t *) r;

	if (rp == NULL ||
	    rp->r_type != ET_READER ||
	    (rp->r_flags & EF_LOCAL) == 0) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (lock) {
		if (lock_take (rp->r_lock)) {
			if (error)
				*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
#ifdef RW_LOCKS
		else if (!rp->r_flags) {	/* Shutting down reader. */
			lock_release (rp->r_lock);
			return (NULL);
		}
#elif defined (RW_TOPIC_LOCK)
		else if (!rp->r_topic->entity.flags) { /* Shutting down topic. */
			lock_release (rp->r_topic->lock);
			return (NULL);
		}
#else
		else if (!rp->r_subscriber->flags) { /* Shutting down subscriber. */
			lock_release (rp->r_subscriber->lock);
			return (NULL);
		}
#endif
	}
	if (error)
		*error = DDS_RETCODE_OK;
	return (rp);
}

/* writer_ptr -- Return a valid reader pointer (result != NULL) or returns NULL
		 and sets the error code appropriately. */

Writer_t *writer_ptr (DDS_DataWriter w, int lock, DDS_ReturnCode_t *error)
{
	Writer_t	*wp = (Writer_t *) w;

	if (wp == NULL ||
	    wp->w_type != ET_WRITER ||
	    (wp->w_flags & EF_LOCAL) == 0) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (lock) {
		if (lock_take (wp->w_lock)) {
			if (error)
				*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
#ifdef RW_LOCKS
		else if (!wp->w_flags) {	/* Shutting down writer. */
			lock_release (wp->w_lock);
			return (NULL);
		}
#elif defined (RW_TOPIC_LOCK)
		else if (!wp->w_topic->entity.flags) { /* Shutting down topic. */
			lock_release (wp->w_topic->lock);
			return (NULL);
		}
#else
		else if (!wp->w_publisher->flags) { /* Shutting down publisher. */
			lock_release (wp->w_publisher->lock);
			return (NULL);
		}
#endif
	}
	if (error)
		*error = DDS_RETCODE_OK;
	return (wp);
}

/* guard_first -- Get the first guard record of the given type. */

Guard_t *guard_first (Guard_t     *list,
		      GuardType_t type,
		      unsigned    kind,
		      int         writer)
{
	Guard_t		*gp;
	int		part;

	part = (type == GT_LIVELINESS &&
	        kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS);
	for (gp = list; gp; gp = (part) ? gp->pnext : gp->enext)
		if (gp->type == type &&
		    gp->kind == kind &&
		    gp->writer == (unsigned) writer)
			break;

	return (gp);
}

/* guard_lookup -- Lookup a guard context. */

Guard_t *guard_lookup (Guard_t     *list,
		       GuardType_t type,
		       unsigned    kind,
		       int         writer,
		       Endpoint_t  *wep,
		       Endpoint_t  *rep)
{
	Guard_t		*gp;
	int		part;

	part = (type == GT_LIVELINESS &&
	        kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS);
	for (gp = list; gp; gp = (part) ? gp->pnext : gp->enext)
		if (gp->type == type &&
		    gp->kind == kind &&
		    gp->writer == writer &&
		    gp->wep == wep &&
		    gp->rep == rep)
			return (gp);

	return (NULL);
}

/* guard_add -- Add a guard context to an entity. */

Guard_t *guard_add (Guard_t     **list,
		    GuardType_t type,
		    unsigned    kind,
		    int         writer,
		    Endpoint_t  *wep,
		    Endpoint_t  *rep,
		    unsigned    period)
{
	Guard_t		*gp, *p, *prev;
	LocalEndpoint_t	*lep;
	int		part;

	if (!list)
		return (NULL);

	gp = mds_pool_alloc (&mem_blocks [MB_GUARD]);
	if (!gp)
		return (NULL);

	part = (type == GT_LIVELINESS &&
	        kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS);

	gp->type = type;
	gp->kind = kind;
	gp->writer = writer;
	gp->mode = gp->cmode = GM_NONE;
	gp->alive = 1;
	gp->critical = 0;
	gp->mark = 0;
	gp->period = period;
	gp->wep = wep;
	gp->rep = rep;
	gp->timer = NULL;
	if ((p = *list) != NULL) {
		for (prev = NULL, p = *list;
		     p && period >= p->period;
		     prev = p, p = (part) ? p->pnext : p->enext)
			;

		if (prev && part)
			prev->pnext = gp;
		else if (prev)
			prev->enext = gp;
		else
			*list = gp;
	}
	else
		*list = gp;
	if (part) {
		gp->pnext = p;
		if (writer)
			lep = (LocalEndpoint_t *) wep;
		else
			lep = (LocalEndpoint_t *) rep;
		gp->enext = lep->guard;
		lep->guard = gp;
	}
	else
		gp->enext = p;
	return (gp);
}

/* guard_unlink -- Unlink a guard context from an entity. */

Guard_t *guard_unlink (Guard_t     **list,
		       GuardType_t type,
		       unsigned    kind,
		       int         writer,
		       Endpoint_t  *wep,
		       Endpoint_t  *rep)
{
	Guard_t		*p, *prev, *xp;
	LocalEndpoint_t	*lep;
	int		part;

	if (!list)
		return (NULL);

	part = (type == GT_LIVELINESS &&
	        kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS);

	for (p = *list, prev = NULL;
	     p && (p->type != type ||
	           p->kind != kind ||
		   p->writer != writer ||
		   p->wep != wep ||
		   p->rep != rep);
	     prev = p, p = (part) ? p->pnext : p->enext)
		;
	if (!p)
		return (NULL);

	if (prev && part)
		prev->pnext = p->pnext;
	else if (prev)
		prev->enext = p->enext;
	else if (part)
		*list = p->pnext;
	else
		*list = p->enext;

	if (part) {
		if (writer)
			lep = (LocalEndpoint_t *) wep;
		else
			lep = (LocalEndpoint_t *) rep;
		for (xp = lep->guard, prev = NULL;
		     xp && xp != p;
		     prev = xp, xp = xp->enext)
			;
		if (prev)
			prev->enext = p->enext;
		else
			lep->guard = p->enext;
	}
	return (p);
}

/* guard_free -- Free a guard node. */

void guard_free (Guard_t *p)
{
	if (p->timer) {
		if (tmr_active (p->timer))
			tmr_stop (p->timer);
		tmr_free (p->timer);
		p->timer = NULL;
		p->cmode = GM_NONE;
	}
	mds_pool_free (&mem_blocks [MB_GUARD], p);
}


/* prefix_lookup -- Lookup a cached prefix for a newly created participant. */

RemPrefix_t *prefix_lookup (Domain_t           *dp,
			    const GuidPrefix_t *prefix)
{
	RemPrefix_t	*p;

	LIST_FOREACH (dp->prefixes, p)
		if (!memcmp (&p->prefix, prefix, GUIDPREFIX_SIZE) && p->locators)
			return (p);

	return (NULL);
}

/* prefix_cache -- Cache remote participant prefix information for future
		   reference. */

void prefix_cache (Domain_t           *dp,
		   const GuidPrefix_t *prefix,
		   const Locator_t    *src)
{
	RemPrefix_t	*p;
	/*char		gbuf [27];

	log_printf (RTPS_ID, 0, "prefix_cache(): %s from %s.\r\n", guid_prefix_str (prefix, gbuf), locator_str (src));*/
	LIST_FOREACH (dp->prefixes, p)
		if (!memcmp (&p->prefix, prefix, GUIDPREFIX_SIZE))
			goto found;

	p = mds_pool_alloc (&mem_blocks [MB_PREFIX]);
	if (!p)
		return;

	p->prefix = *prefix;
	p->locators = NULL;
	LIST_ADD_TAIL (dp->prefixes, *p);

    found:
	locator_list_add (&p->locators,
			  src->kind,
			  src->address,
			  src->port,
			  src->scope_id,
			  src->scope,
			  src->flags,
			  src->sproto);
}

/* prefix_forget -- Remove a cached prefix. */

void prefix_forget (RemPrefix_t *p)
{
	/*char	gbuf [27];

	log_printf (RTPS_ID, 0, "prefix_forget(): %s\r\n", guid_prefix_str (&p->prefix, gbuf));*/
	LIST_REMOVE (0, *p);
	if (p->locators)
		locator_list_delete_list (&p->locators);
	mds_pool_free (&mem_blocks [MB_PREFIX], p);
}

#define	RELAY_INC	4

/* relay_add -- Add a local relay to the set of local relay nodes. */

void relay_add (Participant_t *pp)
{
	Domain_t	*dp;
	Participant_t	**p;
	size_t		nsize;

	dp = pp->p_domain;
	if (dp->nr_relays <= dp->max_relays) {
		nsize = sizeof (Participant_t *) * (dp->max_relays + RELAY_INC);
		if (!dp->max_relays)
			p = (Participant_t **) xmalloc (nsize);
		else
			p = xrealloc (dp->relays, nsize);
		if (!p) {
			warn_printf ("relay_add: no memory for relay registration!\r\n");
			return;
		}
		dp->relays = p;
		dp->max_relays += RELAY_INC;
	}
	dp->relays [dp->nr_relays++] = pp;
}

/* relay_remove -- Remove a local relay from the set of local relays. */

void relay_remove (Participant_t *pp)
{
	Domain_t	*dp;
	Participant_t	**p;
	size_t		nsize;
	unsigned	i;

	dp = pp->p_domain;
	for (i = 0, p = dp->relays; i < dp->nr_relays; i++, p++)
		if (*p == pp)
			break;

	if (i >= dp->nr_relays)
		return;

	dp->nr_relays--;
	if (!dp->nr_relays) {
		dp->max_relays = 0;
		xfree (dp->relays);
	}
	else {
		for (; i < dp->nr_relays; i++, p++)
			*p = p [1];

		if (dp->max_relays > RELAY_INC &&
		    dp->nr_relays == (dp->max_relays >> 1)) {
			nsize = (dp->max_relays >> 1) * sizeof (Participant_t);
			p = xrealloc (dp->relays, nsize);
			if (p) {
				dp->relays = p;
				dp->max_relays >>= 1;
			}
		}
	}
}

#ifdef DDS_DEBUG

static unsigned dump_flags;
static unsigned dump_level;

/* domain_pool_dump -- Display some pool statistics. */

void domain_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

/* dump_locators -- Display a locator list. */

static void dump_locators (int indent, const char *name, const LocatorList_t lp)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	int		comma;
	static const char *spro_str [] = { NULL, "DTLS", "TLS", NULL, "DDSS" };

	dbg_print_indent (indent, name);
	if (!lp) {
		dbg_printf ("(none)\r\n");
		return;
	}
	dbg_printf ("\r\n");
	foreach_locator (lp, rp, np) {
		dbg_print_indent (indent + 1, NULL);
		dbg_print_locator (&np->locator);
		dbg_printf (" {");
		comma = 0;
		if ((np->locator.flags & LOCF_DATA) != 0) {
			dbg_printf ("UD");
			comma = 1;
		}
		if ((np->locator.flags & LOCF_META) != 0) {
			if (comma)
				dbg_printf (",");
			dbg_printf ("MD");
			comma = 1;
		}
		if ((np->locator.flags & LOCF_UCAST) != 0) {
			if (comma)
				dbg_printf (",");
			dbg_printf ("UC");
			comma = 1;
		}
		if ((np->locator.flags & LOCF_MCAST) != 0) {
			if (comma)
				dbg_printf (",");
			dbg_printf ("MC");
			comma = 1;
		}
		if ((np->locator.flags & LOCF_SECURE) != 0) {
			if (comma)
				dbg_printf (",");
			dbg_printf ("E");
			comma = 1;
		}
		if ((np->locator.flags & LOCF_SERVER) != 0) {
			if (comma)
				dbg_printf (",");
			dbg_printf ("S");
		}
		dbg_printf ("}");
		if (np->locator.handle)
			dbg_printf (" H:%u", np->locator.handle);
		if (np->locator.sproto)
			dbg_printf (" %s", spro_str [np->locator.sproto]);
		dbg_printf ("\r\n");
	}
}

/* dump_guard -- Display a guard record. */

static void dump_guard (int indent, Guard_t *gp)
{
	unsigned	  rem;
	static const char *gt_str [] = {
		"Liveliness", "Deadline", "Lifespan", "AutoP_NW", "AutoP_D"
	};
	static const char *gm_str [] = {
		"none", "OS", "Per", "Pro", "Mix"
	};
	static const char *gk_str [] = {
		"Auto", "ManP", "ManT"
	};

	dbg_print_indent (indent, NULL);

	dbg_printf ("  %-10s: ", gt_str [gp->type]);
	if (gp->type == GT_LIVELINESS)
		dbg_printf ("%s-", gk_str [gp->kind]);
	else
		dbg_printf ("     ");

	dbg_printf ("%c = %c%c%c", gp->writer ? 'W' : 'R',
			       gp->alive ? 'A' : '-',
			       gp->critical ? 'C' : '-',
			       gp->mark ? 'M' : '-');
	if (gp->writer)
		dbg_printf (": {%u}->{%u}, ", (gp->wep) ? gp->wep->entity.handle : 0,
					      (gp->rep) ? gp->rep->entity.handle : 0);
	else
		dbg_printf (": {%u}<-{%u}, ", (gp->rep) ? gp->rep->entity.handle : 0,
					      (gp->wep) ? gp->wep->entity.handle : 0);
	dbg_printf ("Period=%u.%02us (%s:%s)", gp->period / 100, gp->period % 100,
				     gm_str [gp->mode], gm_str [gp->cmode]);
	if (gp->timer) {
		rem = tmr_remain (gp->timer);
		dbg_printf (" [%u.%02us]", rem / 100, rem % 100);
	}
	dbg_printf ("\r\n");
}

/* dump_endpoint -- Dump a local or remote endpoint. */

static int dump_endpoint (Skiplist_t *l, void *p, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) p;
	Guard_t		*gp;
	int		indent = *((int *) arg);
	unsigned	dump, loc;

	ARG_NOT_USED (l)
	ARG_NOT_USED (arg)

	ep = *epp;
	if (entity_local (ep->entity.flags)) {
		if ((ep->entity.flags & EF_BUILTIN) != 0)
			dump = dump_flags & DDF_BUILTIN_L;
		else
			dump = dump_flags & DDF_ENDPOINTS_L;
		loc = dump_flags & DDF_LOCATORS_L;
	}
	else {
		if ((ep->entity.flags & EF_BUILTIN) != 0)
			dump = dump_flags & DDF_BUILTIN_R;
		else
			dump = dump_flags & DDF_ENDPOINTS_R;
		loc = dump_flags & DDF_LOCATORS_R;
	}
	if (dump) {
		dbg_print_indent (indent, NULL);
		dbg_print_entity_id (NULL, &ep->entity_id);
		dbg_printf (", {%u}, InlineQoS: %s, %s, %s/%s\r\n", ep->entity.handle,
						((ep->entity.flags & EF_INLINE_QOS) != 0) ? "Yes" : "No",
						(ep->entity.type == ET_WRITER) ? "Writer" : "Reader",
						str_ptr (ep->topic->name),
						str_ptr (ep->topic->type->type_name));
	}
	if (dump && loc) {
		if (ep->ucast)
			dump_locators (indent, "Unicast locators", ep->ucast);
		if (ep->mcast)
			dump_locators (indent, "Multicast locators", ep->mcast);
	}
	if (entity_local (ep->entity.flags) && 
	    (dump_flags & DDF_GUARD_L) != 0 &&
	    ((LocalEndpoint_t *) ep)->guard)
		for (gp = ((LocalEndpoint_t *) ep)->guard; gp; gp = gp->enext)
			dump_guard (indent, gp);
	return (1);
}

/* dbg_print_duration -- Display a duration parameter. */

static void dbg_print_duration (int indent, const char *name, const Duration_t *d)
{
	dbg_print_indent (indent, name);
	dbg_printf ("%u.%09us\r\n", d->secs, d->nanos);
}

/* dump_type -- Dump a type. */

static int dump_type (Skiplist_t *l, void *p, void *arg)
{
	TopicType_t	*ttp, **ttpp = (TopicType_t **) p;
	unsigned	len, *n = (unsigned *) arg;

	ARG_NOT_USED (l)

	ttp = *ttpp;
	len = str_len (ttp->type_name) + 4;
	*n += len;
	if (*n > 68) {
		dbg_printf ("\r\n");
		dbg_print_indent (dump_level, NULL);
		dbg_printf ("    ");
		*n = len;
	}
	dbg_printf ("%s*%u ", str_ptr (ttp->type_name), ttp->nrefs);
	return (1);
}

/* dump_topic -- Dump a topic. */

static int dump_topic (Skiplist_t *l, void *p, void *arg)
{
	Topic_t		*tp, **tpp = (Topic_t **) p;
	unsigned	len, *n = (unsigned *) arg;

	ARG_NOT_USED (l)

	tp = *tpp;
	len = str_len (tp->name) + str_len (tp->type->type_name) + 3;
	*n += len;
	if (*n > 68) {
		dbg_printf ("\r\n");
		dbg_print_indent (dump_level, NULL);
		dbg_printf ("    ");
		*n = len;
	}
	dbg_printf ("%s/%s ", str_ptr (tp->name), str_ptr (tp->type->type_name));
	return (1);
}

static const char *vendor_str [] = {
	NULL,
	"Real-Time Innovations, Inc. - Connext DDS",
	"PrismTech, Inc. - OpenSplice DDS",
	"Object Computing Incorporated, Inc. - OpenDDS",
	"MilSoft",
	"Gallium Visual Systems, Inc. - InterCOM DDS",
	"TwinOaks Computing, Inc. - CoreDX DDS",
	"Lakota Technical Solutions, Inc.",
	"ICOUP Consulting",
	"ETRI Electronics and Telecommunication Research Institute",
	"Real-Time Innovations, Inc. - Connext DDS Micro",
	"PrismTech, Inc. - Mobile",
	"PrismTech, Inc. - Gateway",
	"PrismTech, Inc. - Lite",
	"Technicolor, Inc. - Qeo"
};

#define NVENDORS	(sizeof (vendor_str) / sizeof (char *) - 1)

static int count_readers (Skiplist_t *sl, void *np, void *arg)
{
	unsigned	*n = (unsigned *) arg;
	Endpoint_t	*ep, **epp = (Endpoint_t **) np;

	ARG_NOT_USED (sl)

	ep = *epp;
	if (ep->entity.type == ET_READER)
		(*n)++;
	return (1);
}

static unsigned nreaders (const Skiplist_t *sl)
{
	unsigned	n = 0;

	sl_walk ((Skiplist_t *) sl, count_readers, &n);
	return (n);
}

#ifdef DDS_SECURITY

static void print_transports (unsigned protocols)
{
	int	sep = 0;

	if ((protocols & SECC_DTLS_UDP) != 0) {
		dbg_printf ("DTLS");
		sep = 1;
	}
	if ((protocols & SECC_TLS_TCP) != 0) {
		if (sep)
			dbg_printf ("+");
		dbg_printf ("TLS");
		sep = 1;
	}
	if ((protocols & SECC_DDS_SEC) != 0) {
		if (sep)
			dbg_printf ("+");
		dbg_printf ("DDSSEC");
	}
}

#ifdef DDS_NATIVE_SECURITY

void token_dump (unsigned indent, DDS_Token *tp, unsigned nusers, int pem)
{
	unsigned char	*dp;
	unsigned	i;

	dbg_print_indent (indent, NULL);
	dbg_printf ("%s", tp->class_id);
	if (nusers != 1)
		dbg_printf (" (*%u)", nusers);
	dbg_printf (":\r\n");
	if (pem) {
		for (i = 0, dp = DDS_SEQ_DATA (*tp->binary_value1);
		     i < DDS_SEQ_LENGTH (*tp->binary_value1);
		     i++, dp++) {
			dbg_printf ("%c", *dp);
		}
	}
	else {
		dbg_print_indent (indent + 1, NULL);
		for (i = 0, dp = DDS_SEQ_DATA (*tp->binary_value1);
		     i < DDS_SEQ_LENGTH (*tp->binary_value1);
		     i++, dp++)
			dbg_printf ("%02x", *dp);
	}
	dbg_printf ("\r\n");
}

static void print_tokens (unsigned indent, const char *name, Token_t *p)
{
	Token_t		*tp;

	dbg_print_indent (indent, NULL);
	dbg_printf ("%s tokens:\r\n", name);
	for (tp = p; tp; tp = tp->next)
		token_dump (indent + 1, tp->data, tp->nusers, 0);
}

#endif
#endif

/* dbg_print_participant_info -- Display peer/own participant information. */

static void dump_participant_info (int                 peer,
				   const Participant_t *pp,
				   unsigned            flags)
{
	Guard_t		*gp;
	unsigned	ofs, i, indent;
#ifdef RTPS_USED
	const char	*s;
	unsigned	l;
#endif
	int		comma;
#ifdef DDS_NATIVE_SECURITY
	static const char *hs_state_str [] = {
		"Authenticated",
		"Failed",
		"PendingRetry",
		"PendingHandshakeRequest",
		"PendingHandshakeMessage",
		"OkFinalMessage",
		"PendingChallengeMessage"
	};
#endif

	indent = peer + 1;
	dbg_print_indent (indent, "GUID prefix");
	dbg_print_guid_prefix (&pp->p_guid_prefix);
	dbg_printf ("\r\n");
	dbg_print_indent (indent, "RTPS Protocol version");
	dbg_printf ("v%u.%u\r\n", pp->p_proto_version [0], pp->p_proto_version [1]);
	dbg_print_indent (indent, "Vendor Id");
	dbg_printf ("%u.%u", pp->p_vendor_id [0], pp->p_vendor_id [1]);
	if (pp->p_vendor_id [0] == 1 &&
	    pp->p_vendor_id [1] && pp->p_vendor_id [1] <= (NVENDORS + 1))
		dbg_printf (" - %s", vendor_str [pp->p_vendor_id [1]]);
	dbg_printf ("\r\n");
	if (pp->p_vendor_id [0] == VENDORID_H_TECHNICOLOR &&
	    pp->p_vendor_id [1] == VENDORID_L_TECHNICOLOR) {
		dbg_print_indent (indent, "Technicolor DDS version");
		dbg_printf ("%u.%u-%u", pp->p_sw_version >> 24,
					(pp->p_sw_version >> 16) & 0xff,
					pp->p_sw_version & 0xffff);
#ifdef DDS_FORWARD
		dbg_printf (", Forward: %u", pp->p_forward);
#endif
		dbg_printf ("\r\n");
#ifdef DDS_SECURITY
		dbg_print_indent (indent, "SecureTransport");
		if (!pp->p_sec_caps)
			dbg_printf ("none");
		else {
			if ((pp->p_sec_caps & 0xffff) != 0) {
				dbg_printf ("remote: ");
				print_transports (pp->p_sec_caps & 0xffff);
				if ((pp->p_sec_caps >> 16) != 0)
					dbg_printf (", ");
			}
			if ((pp->p_sec_caps >> 16) != 0) {
				dbg_printf ("local: ");
				print_transports (pp->p_sec_caps >> 16);
			}
		}
#ifdef DDS_NATIVE_SECURITY
		dbg_printf ("\r\n");
		dbg_print_indent (indent, "Authorisation");
		dbg_printf ("%s", hs_state_str [pp->p_auth_state]);
#endif
		dbg_printf ("\r\n");
#endif
	}
#ifdef DDS_NATIVE_SECURITY
	if (pp->p_id_tokens)
		print_tokens (indent, "Identity", pp->p_id_tokens);
	if (pp->p_p_tokens)
		print_tokens (indent, "Permission", pp->p_p_tokens);
#endif
	if (pp->p_entity_name) {
		dbg_print_indent (indent, "Entity name");
		dbg_printf ("%s\r\n", str_ptr (pp->p_entity_name));
	}
	if (!peer ||
	    (pp->p_flags & (EF_INLINE_QOS | EF_NOT_IGNORED)) != EF_NOT_IGNORED ||
	    pp->p_no_mcast) {
		dbg_print_indent (indent, "Flags");
		comma = 0;
		if ((pp->p_flags & EF_INLINE_QOS) != 0) {
			dbg_printf ("ExpectInlineQoS");
			comma = 1;
		}
		if (pp->p_no_mcast) {
			if (comma)
				dbg_printf (", ");
			dbg_printf ("NoMCast");
			comma = 1;
		}
		if ((pp->p_flags & EF_REMOTE) != 0) {
			if ((pp->p_flags & EF_NOT_IGNORED) == 0) {
				if (comma)
					dbg_printf (", ");
				dbg_printf ("Ignored");
			}
		}
		else {
			if (comma)
				dbg_printf (", ");
			if ((pp->p_flags & EF_ENABLED) != 0)
				dbg_printf ("Enabled");
			else
				dbg_printf ("Created");
		}
		dbg_printf ("\r\n");
	}
	if (peer && (pp->p_flags & EF_NOT_IGNORED) == 0)
		return;

	if ((!peer && (flags & DDF_LOCATORS_L) != 0) ||
	    (peer && (flags & DDF_LOCATORS_R) != 0)) {
		if (pp->p_meta_ucast)
			dump_locators (indent, "Meta Unicast", pp->p_meta_ucast);
		if (pp->p_meta_mcast)
			dump_locators (indent, "Meta Multicast", pp->p_meta_mcast);
		if (pp->p_def_ucast)
			dump_locators (indent, "Default Unicast", pp->p_def_ucast);
		if (pp->p_def_mcast)
			dump_locators (indent, "Default Multicast", pp->p_def_mcast);
#ifdef DDS_SECURITY
		if (pp->p_sec_locs)
			dump_locators (indent, "Secure Tunnel", pp->p_sec_locs);
#endif
	}
	dbg_print_indent (indent, "Manual Liveliness");
	dbg_printf ("%u\r\n", pp->p_man_liveliness);
	dbg_print_duration (indent, "Lease duration", &pp->p_lease_duration);
	dbg_print_indent (indent, "Endpoints");
	if (sl_length (&pp->p_endpoints)) {
		i = nreaders (&pp->p_endpoints);
		dbg_printf ("%u entries (%u readers, %u writers).\r\n", 
			sl_length (&pp->p_endpoints), i, 
			sl_length (&pp->p_endpoints) - i);
		if ((!peer && (flags & (DDF_ENDPOINTS_L | DDF_BUILTIN_L)) != 0) ||
		    (peer && (flags & (DDF_ENDPOINTS_R | DDF_BUILTIN_R)) != 0)) {
			indent++;
			dump_flags = flags;
			sl_walk ((Skiplist_t *) &pp->p_endpoints, dump_endpoint, &indent);
			indent--;
		}
	}
	else
		dbg_printf ("<empty>\r\n");
	dump_level = indent;
	if ((peer && (dump_flags & DDF_TOPICS_R) != 0) ||
	    (!peer && (dump_flags & DDF_TOPICS_L) != 0)) {
		dbg_print_indent (indent, "Topics");
		if (sl_length (&pp->p_topics)) {
			dbg_printf ("\r\n");
			dbg_print_indent (indent, NULL);
			dbg_printf ("    ");
			ofs = 0;
			sl_walk ((Skiplist_t *) &pp->p_topics, dump_topic, &ofs);
		}
		else
			dbg_printf (" <none>");
		dbg_printf ("\r\n");
	}
#ifdef RTPS_USED
	if ((peer && (dump_flags & DDF_BUILTIN_R) != 0) ||
	    (!peer && (dump_flags & DDF_BUILTIN_L) != 0)) {
		dbg_print_indent (indent, "Builtin Endpoints");
		/*dbg_printf (" (set: 0x%x)\r\n", pp->p_builtins);*/
		dbg_printf ("\r\n");
		dbg_print_indent (indent, NULL);
		dbg_printf ("    ");
		ofs = 0;
		for (i = 0; i < MAX_BUILTINS; i++) {
			if ((pp->p_builtins & (1 << i)) == 0)
				continue;

			s = rtps_builtin_endpoint_names [i];
			if (s)
				l = strlen (s);
			else
				l = 5;
			if (i) {
				dbg_printf (", ");
				ofs += 2;
			}
			ofs += l;
			if (ofs > 68) {
				dbg_printf ("\r\n");
				dbg_print_indent (indent, NULL);
				dbg_printf ("    ");
				ofs = l;
			}
			if (s)
				dbg_printf ("%s", s);
			else
				dbg_printf ("?(%u)", i);
		}
		dbg_printf ("\r\n");
	}
#endif
	if (pp->p_liveliness &&
	    ((peer && (dump_flags & DDF_GUARD_R) != 0) ||
	     (!peer && (dump_flags & DDF_GUARD_L) != 0))) {
		dbg_print_indent (indent, "Guard:\r\n");
		for (gp = pp->p_liveliness; gp; gp = gp->pnext)
			dump_guard (indent, gp);
	}
	if (pp->p_src_locators)
		dump_locators (indent, "Source", pp->p_src_locators);
}

/* dump_peer -- Dump peer participant callback function. */

static int dump_peer (Skiplist_t *list, void *p, void *arg)
{
	Participant_t	*pp, **ppp = (Participant_t **) p;
	unsigned	*i = (unsigned *) arg;
	Ticks_t		left;

	ARG_NOT_USED (list)

	pp = *ppp;
	dbg_printf ("\t    Peer #%u: {%u}", (*i)++, pp->p_handle);
	if (pp->p_local) {
		left = sys_ticksdiff (pp->p_local, sys_ticks_last);
		dbg_printf (" - Local activity: %lu.%02lus", left / TICKS_PER_SEC, 
						(left % TICKS_PER_SEC) / 10);
	}
	dbg_printf ("\r\n");
	dump_participant_info (1, pp, dump_flags);
	if (tmr_active (&pp->p_timer)) {
		dbg_printf ("\t\tTimer = ");
		dbg_print_timer (&pp->p_timer);
		dbg_printf ("\r\n");
	}
	return (1);
}

void dump_domain (Domain_t *dp, unsigned flags)
{
	unsigned	ofs, i;
	Publisher_t	*up;
	Subscriber_t	*sp;
	RemPrefix_t	*prp;
	Participant_t	**p;
#ifdef DDS_SECURITY
	static const char *sec_mode [] = {
		"Unclassified", "Confidential", "Secret", "Top-Secret"
	};
#endif
	lock_take (dp->lock);
	dbg_printf ("Domain %u (pid=%u): {%u}\r\n",
				dp->domain_id,
				dp->participant_id,
				dp->participant.p_handle);
	dump_participant_info (0, &dp->participant, flags);
#ifdef DDS_SECURITY
	dbg_printf ("\tSecurity: level=");
	if (dp->security <= 3)
		dbg_printf ("%s", sec_mode [dp->security]);
	else
		dbg_printf ("%u", dp->security);
#ifdef DDS_NATIVE_SECURITY
	dbg_printf (", access=%s", (dp->access_protected) ? "enforce" : "any");
	dbg_printf (", RTPS=%s", (dp->rtps_protected) ? "encrypt" : "clear");
	if (dp->rtps_protected)
		dbg_printf ("(%s)", (dp->rtps_protected > 4) ? "?" : crypt_std_str [dp->rtps_protected]);
#endif
	dbg_printf ("\r\n");
#endif
	if ((flags & (DDF_TYPES_L | DDF_TYPES_R)) != 0) {
		dbg_printf ("\tTypes:");
		if (sl_length (&dp->types)) {
			dbg_printf ("\r\n\t    ");
			ofs = 0;
			sl_walk (&dp->types, dump_type, &ofs);
		}
		else
			dbg_printf (" <none>");
		dbg_printf ("\r\n");
	}
	dbg_print_duration (1, "Resend period", &dp->resend_per);
	if ((flags & DDF_LOCATORS_L) != 0)
		dump_locators (1, "Destination Locators", dp->dst_locs);
	if (dp->nr_relays) {
		dbg_printf ("\tRelays:\r\n");
		for (i = 0, p = dp->relays; i < dp->nr_relays; i++, p++) {
			dbg_printf ("\t\t");
			dbg_print_guid_prefix (&(*p)->p_guid_prefix);
			dbg_printf ("\r\n");
		}
	}
	if ((flags & DDF_PUBSUB) != 0) {
		for (up = dp->publishers.head; up; up = up->next)
			dbg_printf ("\tPublisher: %u writers.\r\n", up->nwriters);
		for (sp = dp->subscribers.head; sp; sp = sp->next)
			dbg_printf ("\tSubscriber: %u readers.\r\n", sp->nreaders);
	}
	if (/*(flags & DDF_PREFIX) != 0 &&*/ LIST_NONEMPTY (dp->prefixes)) {
		dbg_printf ("\tPrefixes:\r\n");
		LIST_FOREACH (dp->prefixes, prp) {
			dbg_printf ("\t\t[");
			dbg_print_guid_prefix (&prp->prefix);
			dbg_printf (", ");
			dump_locators (0, "Prefix locators", prp->locators);
			dbg_printf ("]\r\n");
		}
	}
	if ((flags & DDF_PEERS) != 0) {
		dbg_printf ("\tDiscovered participants:");
		if (sl_length (&dp->peers)) {
			dbg_printf ("\r\n");
			i = 0;
			dump_flags = flags;
			sl_walk (&dp->peers, dump_peer, &i);
		}
		else
			dbg_printf (" <none>\r\n");
	}
	lock_release (dp->lock);
}

void dump_domains (unsigned flags)
{
	Domain_t	*dp;
	unsigned	i;

	dds_lock_domains ();
	for (i = 1; i <= MAX_DOMAINS; i++)
		if ((dp = domains [i]) != NULL)
			dump_domain (dp, flags);
	dds_unlock_domains ();
}

/* filter_info_dump -- Dump the contents of a filter. */

static void filter_info_dump (unsigned indent, FilterData_t *fdp, int dump_all)
{
	unsigned	i;
	String_t	**sp;

	dbg_print_indent (indent, NULL);
	dbg_printf ("Filter name: %s\r\n", str_ptr (fdp->filter.name));
	dbg_print_indent (indent, NULL);
	dbg_printf ("Class name: %s\r\n", str_ptr (fdp->filter.class_name));
	dbg_print_indent (indent, NULL);
	dbg_printf ("Expression: %s\r\n", str_ptr (fdp->filter.expression));
	dbg_print_indent (indent, NULL);
	dbg_printf ("Parameters: ");
	if (fdp->filter.expression_pars &&
	    DDS_SEQ_LENGTH (*fdp->filter.expression_pars)) {
		dbg_printf ("\r\n");
		indent++;
		DDS_SEQ_FOREACH_ENTRY (*fdp->filter.expression_pars, i, sp) {
			dbg_print_indent (indent, NULL);
			dbg_printf ("%%%u = \"%s\"\r\n", i, str_ptr (*sp));
		}
		indent--;
	}
	else
		dbg_printf ("none\r\n");
	dbg_print_indent (indent, NULL);
	if (dump_all) {
		dbg_printf ("Bytecode program:\r\n");
		bc_dump (indent - 1, &fdp->program);
	}
	else
		dbg_printf ("Bytecode program size: %lu bytes.\r\n", (unsigned long) fdp->program.length);
}

/* topic_endpoint_dump -- Dump an endpoint referred to by a topic. */

static void topic_endpoint_dump (Domain_t   *dp,
				 Endpoint_t *ep,
				 const char type,
				 int        dump_all)
{
	DiscoveredReader_t	*drp;

	dbg_printf ("\r\n\t\t");
	if (entity_discovered (ep->entity.flags)) {
		dbg_printf ("D%c: {%u}\t", type, ep->entity.handle);
		dbg_print_guid_prefix (&ep->u.participant->p_guid_prefix);
	}
	else {
		dbg_printf ("%c:  {%u}\t", type, ep->entity.handle);
		dbg_print_guid_prefix (&dp->participant.p_guid_prefix);
	}
	dbg_printf ("-");
	dbg_print_entity_id (NULL, &ep->entity_id);
#ifdef DDS_SHOW_QOS
	if (ep->qos)
		dbg_printf ("  Qos: %p", (void *) ep->qos);
#endif
	if (entity_reader (entity_type (&ep->entity)) &&
	    (ep->entity.flags & (EF_LOCAL | EF_FILTERED)) == EF_FILTERED) {
		dbg_printf ("\r\n");
		drp = (DiscoveredReader_t *) ep;
		filter_info_dump (3, drp->dr_content_filter, dump_all);
	}
}

#ifdef DDS_TYPECODE

#define	MAX_DTC	64

/* tc_list_add -- Add a typecode reference to a list of typecodes. */

static unsigned tc_list_add (unsigned char *dtc [],
			     unsigned      n,
			     Endpoint_t    *ep,
			     unsigned char *vtc)
{
	unsigned char	*tc;
	unsigned	i;

	for (; ep; ep = ep->next) {
		if (!entity_discovered (ep->entity.flags))
			continue;

		if (entity_reader (entity_type (&ep->entity)))
			tc = ((DiscoveredReader_t *) ep)->dr_tc;
		else
			tc = ((DiscoveredWriter_t *) ep)->dw_tc;
		if (!tc || tc == (unsigned char *) ~0UL || (vtc && tc == vtc))
			continue;

		for (i = 0; i < n; i++)
			if (dtc [i] == tc)
				break;

		if (i < n || n == MAX_DTC)
			continue;

		dtc [n++] = tc;
	}
	return (n);
}

static unsigned tc_handles (unsigned char *xtc, unsigned n, Endpoint_t *ep)
{
	unsigned char	*tc;

	for (; ep; ep = ep->next) {
		if (!entity_discovered (ep->entity.flags))
			continue;

		if (entity_reader (entity_type (&ep->entity)))
			tc = ((DiscoveredReader_t *) ep)->dr_tc;
		else
			tc = ((DiscoveredWriter_t *) ep)->dw_tc;
		if (!tc || tc == (unsigned char *) ~0UL || tc != xtc)
			continue;

		if (n)
			dbg_printf (", ");

		dbg_printf ("{%u}", ep->entity.handle);
		n++;
	}
	return (n);
}

#endif

/* topic_info_dump -- Dump the contents of a single topic. */

static void topic_info_dump (unsigned indent, 
			     Domain_t *dp,
			     Topic_t  *tp,
			     int      typecode,
			     unsigned flags)
{
	Endpoint_t	*ep;
	FilteredTopic_t	*ftp;
#ifdef DDS_TYPECODE
	TypeLib		*lp;
	TypeSupport_t	*nts;
	unsigned char	*vtc, *dtc [MAX_DTC];
	unsigned	i, n, ndtc;
#endif
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		dbg_printf ("%s/%s:\r\n", str_ptr (ftp->related->name), str_ptr (tp->type->type_name));
		filter_info_dump (1, &ftp->data, typecode);
	}
	else
		dbg_printf ("%s/%s:\r\n", str_ptr (tp->name), str_ptr (tp->type->type_name));
	if (typecode)
		DDS_TypeSupport_dump_type (1, tp->type->type_support, flags);
	dbg_print_indent (indent, NULL);
	dbg_printf ("# of local create/find_topic() calls = %u, # of discoveries = %u\r\n",
					tp->nlrefs, tp->nrrefs);
#ifdef DDS_SHOW_QOS
	if (tp->qos) {
		dbg_print_indent (indent, NULL);
		dbg_printf ("\tQoS at %p (1 of %u)\r\n", (void *) tp->qos, (tp->qos) ? tp->qos->users : 0);
	}
#endif
	dbg_print_indent (indent, NULL);
	dbg_printf ("Writers:");
	if (!tp->writers)
		dbg_printf (" <none>\r\n");
	else {
		for (ep = tp->writers; ep; ep = ep->next)
			topic_endpoint_dump (dp, ep, 'W', typecode);
		dbg_printf ("\r\n");
	}
	dbg_print_indent (indent, NULL);
	dbg_printf ("Readers:");
	if (!tp->readers)
		dbg_printf (" <none>\r\n");
	else {
		for (ep = tp->readers; ep; ep = ep->next)
			topic_endpoint_dump (dp, ep, 'R', typecode);
		dbg_printf ("\r\n");
	}
#ifdef DDS_TYPECODE
	if (typecode) {
		ndtc = 0;
		if (tp->type->type_support &&
		    tp->type->type_support->ts_prefer >= MODE_V_TC)
			vtc = (unsigned char *) tp->type->type_support->ts_vtc;
		else
			vtc = NULL;
		ndtc = tc_list_add (dtc, 0, tp->writers, vtc);
		ndtc = tc_list_add (dtc, ndtc, tp->readers, vtc);
		for (i = 0; i < ndtc; i++) {

			/* Create a new type library to prevent clashes. */
			lp = xt_lib_create (NULL);
			if (!lp) {
				dbg_printf ("\r\nCan't create type library!\r\n");
				continue;
			}
			nts = vtc_type (lp, dtc [i]);
			if (!nts) {
				dbg_printf ("\r\nCan't convert typecode to real type!\r\n");
				xt_lib_delete (lp);
				continue;
			}
			dbg_printf ("\tAlternative type: (used by ");
			n = tc_handles (dtc [i], 0, tp->writers);
			tc_handles (dtc [i], n, tp->readers);
			dbg_printf (")\r\n");
			DDS_TypeSupport_dump_type (2, nts, 0);
			xt_type_delete (nts->ts_cdr);
			xfree (nts);
			xt_lib_delete (lp);
		}
	}
#endif
}

/* topic_node_dump -- Dump the contents of a topic node. */

static int topic_node_dump (Skiplist_t *list, void *node, void *arg)
{
	Topic_t		**tpp = (Topic_t **) node;

	ARG_NOT_USED (list)

	topic_info_dump (1, (Domain_t *) arg, *tpp, 0, 0);
	return (1);
}

/* topic_dump -- Dump the topic contents. */

void topic_dump (Domain_t *dp, const char *name, unsigned flags)
{
	Topic_t		*tp;

	if (!dp) {
		dbg_printf ("No such domain!\r\n");
		return;
	}
	lock_take (dp->lock);
	if (name && *name) {
		tp = topic_lookup (&dp->participant, name);
		if (!tp) {
			lock_release (dp->lock);
			dbg_printf ("No such topic!\r\n");
			return;
		}
		topic_info_dump (1, dp, tp, 1, flags);
	}
	else
		sl_walk (&dp->participant.p_topics, topic_node_dump, dp);
	lock_release (dp->lock);
}

#endif

