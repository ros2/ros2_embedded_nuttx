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

/* guid.h -- Defines the Global Unique Identifier type as used in DDS. */

#ifndef __guid_h_
#define	__guid_h_

#include "dds/dds_error.h"

#define	GUID_OK		DDS_RETCODE_OK
#define GUID_ERR_NOMEM	DDS_RETCODE_OUT_OF_RESOURCES

#define	MAX_DOMAIN_ID	230		/* Max. # of domains. */
#define ILLEGAL_PID	0xFFFFFFFFU

#define GUIDPREFIX_SIZE	12
typedef union {
	unsigned char	prefix [GUIDPREFIX_SIZE];
	uint32_t	w [GUIDPREFIX_SIZE / 4];
} GuidPrefix_t;

#define	GUIDPREFIX_UNKNOWN	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

extern GuidPrefix_t guid_prefix_unknown;

extern unsigned	dds_exec_env;		/* Execution Environment identifier. */
extern unsigned	dds_participant_id;	/* Last used participant Id. */

#define	ENTITYID_SIZE	4

typedef union {
	unsigned char	id [ENTITYID_SIZE];
	uint32_t	w;
} EntityId_t;

#define	ENTITY_KIND_INDEX	3	/* Index in EntityId_t for entityKind. */

/* entityKind encoding: */
#define	ENTITY_KIND_USER	0x00
#define	ENTITY_KIND_VENDOR	0x40
#define	ENTITY_KIND_BUILTIN	0xc0
#define	ENTITY_KIND_MAJOR	0xc0

#define	ENTITY_KIND_UNKNOWN	0
#define	ENTITY_KIND_PARTICIPANT	1
#define	ENTITY_KIND_WRITER_KEY	2
#define	ENTITY_KIND_WRITER	3
#define	ENTITY_KIND_READER	4
#define	ENTITY_KIND_READER_KEY	7
#define	ENTITY_KIND_MINOR	0x3f

#define	ENTITYID_UNKNOWN	{ 0, 0, 0, 0 }

#define	entity_id_writer(eid)	\
	(((eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_WRITER_KEY) \
      || ((eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_WRITER))

#define	entity_id_reader(eid)	\
	(((eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_READER_KEY) \
      || ((eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_READER))

#define	entity_id_unknown(eid)	((eid).w == 0)

#define	entity_id_eq(eid1,eid2)	((eid1).w == (eid2).w)

#define	entity_id_cpy(eid1,eid2) eid1 = eid2

/* Standard builtin topic endpoints: */
#define	ENTITYID_PARTICIPANT					{ 0, 0, 1, 0xc1 }
#define	ENTITYID_SEDP_BUILTIN_TOPIC_WRITER			{ 0, 0, 2, 0xc2 }
#define	ENTITYID_SEDP_BUILTIN_TOPIC_READER			{ 0, 0, 2, 0xc7 }
#define ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER		{ 0, 0, 3, 0xc2 }
#define ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER		{ 0, 0, 3, 0xc7 }
#define ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER 		{ 0, 0, 4, 0xc2 }
#define ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER		{ 0, 0, 4, 0xc7 }
#define ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER		{ 0, 1, 0, 0xc2 }
#define ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER		{ 0, 1, 0, 0xc7 }
#define ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER		{ 0, 2, 0, 0xc2 }
#define ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER		{ 0, 2, 0, 0xc7 }

/* Security - clear topic endpoints for handshake: */
#define	ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_WRITER	{ 0, 2, 1, 0xc3 }
#define	ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_READER	{ 0, 2, 1, 0xc4 }

/* Security - standard discovery encrypted topic endpoints: */
#define	ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER	{ 0xff, 0, 3, 0xc2 }
#define	ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER	{ 0xff, 0, 3, 0xc7 }
#define	ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER	{ 0xff, 0, 4, 0xc2 }
#define	ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER	{ 0xff, 0, 4, 0xc7 }
#define	ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER	{ 0xff, 2, 0, 0xc2 }
#define ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER	{ 0xff, 2, 0, 0xc7 }

/* Security - encrypted topic endpoints for token transfers: */
#define	ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER	{ 0xff, 2, 2, 0xc3 }
#define	ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER	{ 0xff, 2, 2, 0xc4 }

/* Policy - encrypted topic endpoints for policy state transfer */
#define ENTITIYID_QEO_BUILTIN_POLICY_STATE_WRITER               { 0xff, 4, 1, 0xc2 }
#define ENTITIYID_QEO_BUILTIN_POLICY_STATE_READER               { 0xff, 4, 1, 0xc7 }

/* Central Discovery Daemon topic endpoints: */
#define	ENTITYID_CDD_BUILTIN_PUBLICATIONS_WRITER		{ 0, 3, 0, 0xc2 }
#define ENTITYID_CDD_BUILTIN_PUBLICATIONS_READER		{ 0, 3, 1, 0xc7 }
#define ENTITYID_CDD_BUILTIN_SUBSCRIPTIONS_WRITER 		{ 0, 3, 2, 0xc2 }
#define ENTITYID_CDD_BUILTIN_SUBSCRIPTIONS_READER		{ 0, 3, 3, 0xc7 }

#define	GUID_SIZE	(GUIDPREFIX_SIZE + ENTITYID_SIZE)

typedef struct guid_st {	/* 16-octets */
	GuidPrefix_t	prefix;
	EntityId_t	entity_id;
} GUID_t;

#define	GUID_UNKNOWN		{ GUIDPREFIX_UNKNOWN, ENTITYID_UNKNOWN }

/* GUID prefixes will be encoded in the following manner:
	Bytes  0.. 3: Host Id
	Bytes      4: Domain Id
	Bytes      5: Participant Id.
	Bytes  6.. 7: Process Id
	Bytes  8.. 9: User Id
	Byte      10: Execution environment identifier.
	Byte      11: Counter
 */

typedef unsigned DomainId_t;

extern GuidPrefix_t	guid_prefix_unknown;	/* Unknown GUID prefix. */
extern EntityId_t	entity_id_unknown;	/* Unknown Entity Id. */
extern EntityId_t	entity_id_participant;	/* Participant Entity Id. */


int guid_init (unsigned eid);

/* Initialize a new base GUID prefix. */

void guid_final (void);

/* Finalize allocated GUID memory. */

GuidPrefix_t *guid_local (void);

/* Return the common GUID prefix without embedded Domain and Participant Id
   information. */

void guid_normalise (GuidPrefix_t *gp);

/* Normalise, i.e. create a generic GUID prefix from a specific prefix, so
   that it is without embedded Domain and Participant Id information. */

void guid_finalize (GuidPrefix_t *gp, DomainId_t domain_id, unsigned pid);

/* Convert a normalized GUID prefix to an actual prefix. */

unsigned guid_new_participant (GuidPrefix_t *gp, DomainId_t id);

/* Create a GUID based on a new Domain id for the given domain and
   return the new Participant Id. */

void guid_restate_participant (unsigned pid);

/* Restate that this pid is used by one of the participants of this
   application. */

void guid_free_participant (unsigned pid);

/* Return the participant pid so it can be reused by other participants. */

int guid_needs_marshalling (GuidPrefix_t *gp);

/* Returns a non-0 result if the given GUID requires marshalling. */

int guid_local_component (GuidPrefix_t *gp);

/* Return a non-0 result if the GUID prefix is from a local component. */

int guid_local_cdd (GuidPrefix_t *gp);

/* Return a non-0 result if the GUID prefix is from a local Central Discovery
   Daemon. */

#define	guid_prefix_eq(g1,g2)	((g1).w [0] == (g2).w [0] && \
				 (g1).w [1] == (g2).w [1] && \
				 (g1).w [2] == (g2).w [2])

/* Compare two GUID prefixes for equality. */

#define	guid_prefix_cpy(g1,g2)	g1 = g2

/* Copy a GUID prefix. */

char *guid_prefix_str (const GuidPrefix_t *gp, char buffer []);

/* Return a GUID prefix string. */

char *guid_str (const GUID_t *g, char buffer []);

/* Return a GUID string. */

char *entity_id_str (const EntityId_t *ep, char buffer []);

/* Return an entity id string. */

#define	guid_eq(g1,g2)	(guid_prefix_eq ((g1).prefix, (g2).prefix) && \
			 entity_id_eq ((g1).entity_id, (g2).entity_id))

/* Compare two GUIDs for equality. */

#define	guid_cpy(g1,g2)	g1 = g2

/* Copy a GUID to another. */

int entity_id_names (DomainId_t       domain,
		     const EntityId_t *eid,
		     const char       *names [2]);

/* Retrieve the Entity Identifier Topic and Type names. */


#endif /* !__guid_h_ */

