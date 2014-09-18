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

/* domain.h -- Defines all types and functions to access DDS domains and domain
	       entities as needed for RTPS, DCPS, SPDP and SEDP protocols. */

#ifndef __domain_h_
#define	__domain_h_

#include "skiplist.h"
#include "str.h"
#include "timer.h"
#include "pool.h"
#include "guid.h"
#include "locator.h"
#include "uqos.h"
#include "cache.h"
#include "dds_data.h"
#include "dds/dds_dcps.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_data.h"
#endif

#ifndef MAX_DOMAINS
#define	MAX_DOMAINS	4	/* Max. # of supported domains. */
#endif

/* Domain pool parameters: */
typedef struct domain_cfg_st {
	unsigned	ndomains;	/* # of supported domains. */
	POOL_LIMITS	dparticipants;	/* Discovered participants. */
	POOL_LIMITS	types;		/* Unique types. */
	POOL_LIMITS	topics;		/* Topics. */
	POOL_LIMITS	filter_topics;	/* Topics. */
	POOL_LIMITS	publishers;	/* Publishers. */
	POOL_LIMITS	subscribers;	/* Subscribers. */
	POOL_LIMITS	writers;	/* Local writers. */
	POOL_LIMITS	readers;	/* Local readers. */
	POOL_LIMITS	dwriters;	/* Discovered writers. */
	POOL_LIMITS	dreaders;	/* Discovered readers. */
	POOL_LIMITS	guards;		/* Guard contexts. */
	POOL_LIMITS	prefixes;	/* Cached GUID prefix. */
} DomainCfg_t;


int domain_pool_init (DomainCfg_t *cfg);

/* Initialize the domain subsystem with the given parameters. */

void domain_pool_final (void);

/* Finalize domain processing. */


Entity_t *entity_ptr (DDS_HANDLE handle);

/* Get a pointer to an entity based on the entity handle (= publication handle).
   The entity can be any of the following:
   
   	- Participant (Local or Remote).
	- Publisher (Local only).
	- Subscriber (Local only).
	- Topic (Local and/or Remote)
	- DataReader (Local or Remote)
	- DataWriter (Local or Remote).
 */

Participant_t *entity_participant (DDS_HANDLE handle);

/* Get a pointer to the participant of a writer, based on its publication
   handle. */

void handle_unassign (Entity_t *ep, int free_handle);

/* Cleanup handle for a given entity. If free_handle is given, the handle is
   effectively freed immediately, else the handle_done function should be
   called later. */

void handle_done (handle_t h);

/* Free a handle. */

Domain_t *domain_lookup (DomainId_t id);

/* Lookup a Domain based on the Domain Identifier. */

Domain_t *domain_from_prefix (const GuidPrefix_t *prefix);

/* Lookup a Domain based on a GUID prefix. */

Domain_t *domain_create (DomainId_t id);

/* Create a new Domain participant with the given Domain Identifier.
   On completion, the first part of the domain structure is initialized.
   Caller still needs to setup the fields starting from the DCPS part
   and still needs to enable the enitity.  */

void domain_used (Domain_t *dp);

/* A domain participant as created by domain_create() already exists.
   This can happen, for example, if other DDS components based on other
   DDS vendor implementations are present, but also if the shared memory region
   has been removed.  The purpose of this function is to use the next available
   participant Id, so the caller can try again to create the participant. */

void domain_detach (Domain_t *dp);

/* Detach a domain participant from the list of participants. */

int domain_delete (Domain_t *dp);

/* Delete a domain participant. */

Domain_t *domain_ptr (void *p, int lock, DDS_ReturnCode_t *error);

/* Return a domain valid domain pointer or NULL, based on a 
   DDS_DomainParticipant pointer (p). */

void domain_close (unsigned index);

/* Closedown a configured domain.  No more received packets will be handled. */

Domain_t *domain_get (unsigned index, int lock, DDS_ReturnCode_t *error);

/* Return a domain valid domain pointer or NULL, based on the domain index. */

Domain_t *domain_next (unsigned *index, DDS_ReturnCode_t *error);

/* Get the next configured domain. */

unsigned domain_count (void);

/* Return the number of domains used. */

int domains_used (void);

/* Safe (non-locking) method to know if domains are active. */

Participant_t *participant_lookup (const Domain_t *dp,
				   const GuidPrefix_t *prefix);

/* Lookup a discovered participant, based on the given prefix. */

Participant_t *participant_create (Domain_t           *dp,
				   const GuidPrefix_t *prefix,
				   int                *new1);

/* Create a discovered participant, based on the given prefix.
   On completion, all parameters except the entity header, guid_prefix and the
   endpoints list still need to be filled in and still needs to enable the
   entity.  */

int participant_delete (Domain_t *dp, Participant_t *pp);

/* Delete a previously created participant. */


TopicType_t *type_lookup (Domain_t *dp, const char *type_name);

/* Lookup a type name within a domain. */

TopicType_t *type_create (Domain_t *dp, const char *type_name, int *new1);

/* Create a new type.  On completion, caller needs to fill all fields except
   name. */

int type_delete (Domain_t *dp, TopicType_t *type);

/* Delete a previously created type. */


Topic_t *topic_lookup (const Participant_t *pp,
		       const char          *topic_name);

/* Lookup a topic with the given name within the participant. */

Topic_t *topic_create (Participant_t *pp,
		       Topic_t       *tp,
		       const char    *topic_name,
		       const char    *type_name,
		       int           *new1);

/* Create a topic with the given name and type name.  If tp is non-NULL, this
   specifies that the topic is already present in the domain and just needs to
   be added to the peer participant. On completion, and if *new is set, the
   caller needs to fill in only the QoS and DCPS fields and needs to enable
   the entity. */

int topic_delete (Participant_t *pp, Topic_t *topic, int *last_ep, int *gone);

/* Delete a previously created topic.
   The two booleans specify on return whether this was the last topic
   reference for the participant (last_ep) and whether the topic has  */

Topic_t *topic_ptr (void *p, int lock, DDS_ReturnCode_t *ret);

/* Return a valid topic pointer (result != NULL) or returns NULL and sets
   the error code appropriately. */

FilteredTopic_t *filtered_topic_create (Domain_t   *dp,
					Topic_t    *tp,
					const char *name);

/* Create a content-filtered topic based on the existing topic. */

int filtered_topic_delete (FilteredTopic_t *tp);


/* Delete a content-filtered topic. */

void filter_data_cleanup (FilterData_t *fp);

/* Delete all filter data. */


Publisher_t *publisher_create (Domain_t *dp, int builtin);

/* Create a new publisher.  On completion, all fields, except the DCPS fields
   still need to be filled in and the entity still needs to be enabled. */

int publisher_delete (Publisher_t *pp);

/* Delete a previously created publisher. */

Publisher_t *publisher_ptr (DDS_Publisher    publisher,
			    DDS_ReturnCode_t *error);

/* Return a valid publisher pointer (result != NULL) or returns NULL and sets
   the error code appropriately. */


Subscriber_t *subscriber_create (Domain_t *dp, int builtin);

/* Create a new subscriber.  On completion, all fields, except the DCPS fields
   still need to be filled in and the entity still needs to be enabled. */

int subscriber_delete (Subscriber_t *sp);

/* Delete a previously created subscriber. */

Subscriber_t *subscriber_ptr (DDS_Subscriber   subscriber,
			      DDS_ReturnCode_t *error);

/* Return a valid subscriber pointer (result != NULL) or returns NULL and sets
   the error code appropriately. */


Endpoint_t *endpoint_lookup (const Participant_t *pp, const EntityId_t *id);

/* Lookup an endpoint of a participant. */

Endpoint_t *endpoint_create (Participant_t    *pp,
			     void             *parent,
			     const EntityId_t *id,
			     int              *new1);

/* Create an endpoint with the given Entity Id.  For local endpoints, the parent
   pointer must be a publisher or subscriber.  For discovered endpoints, this
   will be the Peer participant. On completion, caller needs to fill in all
   fields except the Endpoint header and still needs to enable the entity. */

int endpoint_delete (Participant_t *pp, Endpoint_t *ep);

/* Delete a previously created endpoint. */

Endpoint_t *endpoint_from_guid (const Domain_t *dp, GUID_t *guid);

/* Lookup an endpoint via its GUID. */


Reader_t *reader_ptr (DDS_DataReader r, int lock, DDS_ReturnCode_t *ret);

/* Return a valid reader pointer (result != NULL) or returns NULL and sets
   the error code appropriately. */

Writer_t *writer_ptr (DDS_DataWriter w, int lock, DDS_ReturnCode_t *ret);

/* Return a valid reader pointer (result != NULL) or returns NULL and sets
   the error code appropriately. */


Guard_t *guard_first (Guard_t     *list,
		      GuardType_t type,
		      unsigned    kind,
		      int         writer);

/* Get the first Guard record of the given type/kind. */


Guard_t *guard_lookup (Guard_t     *list,
		       GuardType_t type,
		       unsigned    kind,
		       int         writer,
		       Endpoint_t  *wep,
		       Endpoint_t  *rep);

/* Lookup a guard context with the given parameters. */

Guard_t *guard_add (Guard_t     **list,
		    GuardType_t type,
		    unsigned    kind,
		    int         writer,
		    Endpoint_t  *wep,
		    Endpoint_t  *rep,
		    unsigned    period);

/* Add a guard context to an entity. If successful, the first context of the
   same type in the list is returned.  Otherwise it returns NULL. */

Guard_t *guard_unlink (Guard_t     **list,
		       GuardType_t type,
		       unsigned    kind,
		       int         writer,
		       Endpoint_t  *wep,
		       Endpoint_t  *rep);

/* Get the guard with the given parameters. */

void guard_free (Guard_t *gp);

/* Free a guard node. */


void prefix_cache (Domain_t           *dp,
		   const GuidPrefix_t *prefix,
		   const Locator_t    *src);

/* Add remote participant prefix information for future reference. */

RemPrefix_t *prefix_lookup (Domain_t           *dp,
		            const GuidPrefix_t *prefix);

/* Lookup a cached prefix. */

void prefix_forget (RemPrefix_t *prefix);

/* Remove a cached prefix. */


void relay_add (Participant_t *pp);

/* Add a local relay to the set of local relays in a domain.
   Local relay entities are neighbouring participants that are able to forward
   traffic to non-local entities that would otherwise not be reachable.
   Their locators are always added to the normal/default locators for a
   destination, in case the destinations are only indirectly accessible.
   In that case, the relay will update the destination locators using the
   InfoReply/InfoSource mechanisms once a connection is properly
   established with the peer entity. */

void relay_remove (Participant_t *pp);

/* Remove a local relay from the set of local relays in a domain. */


/* Trace/Debug support: */
/* -------------------  */

#define	DDF_LOCATORS_L	0x0001		/* Dump local locator lists. */
#define	DDF_LOCATORS_R	0x0002		/* Dump remote locator lists. */
#define	DDF_BUILTIN_L	0x0004		/* Dump local builtins. */
#define	DDF_BUILTIN_R	0x0008		/* Dump remote builtins. */
#define DDF_ENDPOINTS_L	0x0010		/* Dump local endpoints. */
#define DDF_ENDPOINTS_R	0x0020		/* Dump remote endpoints. */
#define DDF_TYPES_L	0x0040		/* Dump local types. */
#define DDF_TYPES_R	0x0080		/* Dump remote types. */
#define DDF_TOPICS_L	0x0100		/* Dump local topics. */
#define DDF_TOPICS_R	0x0200		/* Dump remote topics. */
#define	DDF_GUARD_L	0x0400		/* Dump local guards. */
#define	DDF_GUARD_R	0x0800		/* Dump remote guards. */
#define DDF_PEERS	0x1000		/* Dump peer participants. */
#define	DDF_PUBSUB	0x2000		/* Dump publishers/subscribers. */
#define	DDF_PREFIX	0x4000		/* Dump prefix cache. */

void dump_domain (Domain_t *dp, unsigned flags);

/* Dump DDS domain data of the given domain.  To avoid displaying too much data,
   the flags field specifies what information should be displayed. */

void dump_domains (unsigned flags);

/* Dump the DDS domain data.  To avoid displaying too much data, the flags field
   specifies what information should be displayed. */

void domain_pool_dump (size_t sizes []);

/* Dump the domain pool statistics. */

void topic_dump (Domain_t *dp, const char *name, unsigned flags);

/* Dump the topic contents. */

#ifdef DDS_SECURITY

void token_dump (unsigned indent, DDS_Token *tp, unsigned nusers, int pem);

/* Dump a security token. */

#endif
#endif /* !__domain_h_ */

