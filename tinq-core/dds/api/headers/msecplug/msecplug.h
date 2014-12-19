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

/* msecplug.h -- Minimalistic security plugin code.

   This plugin provides a flexible rule-based security policy, where
   fine-grained authentication is possible, based on rules for different
   DDS objects.

   Rules should be specified in a 'security.xml' file, which can be located
   either in the local directory, in the home directory, or in a well-known
   system-wide location.

   Following DDS objects are authenticated:

	- Local Domain Participants: Credentials must be specified for access
				     to secure domains.  This includes
				     certificates and a private key, derived
				     from a publicly known DDS key.
	- Peer Domain Participants: Access to a secure domain is only granted
				    when the participant has permissions.
	- Local DDS Topics: Topic creation is authenticated via topic rules.
	- Peer DDS Topics: New peer topics are accepted only when the security
			   policy allows this.
	- Local/peer DataWriters/DataReaders: Similar to topic authentication,
					      topic rules specify who can
					      create DataWriters and/or
					      DataReaders in a domain.
	- Partitions: In a secure domain, wildcard partition names are no longer
		      accepted.  Only specific partitions can be given, and
		      these are fully authenticated, both for local and for
		      peer participants.
	- QoS parameters: All DataReader/DataWriter QoS parameters are passed.
			  to the security plugin for optional validation.

   In secure domains, the transport protocol between participants can be
   be specified differently for local (same-node) and remote (different-node)
   participants.  Typically, same-node communication is shielded an can still
   be done unencrypted.  On the other hand, encrypted protocols (such as DTLS)
   are needed between nodes, or between different containers in the same node.

   Domains and participants basically have a security level (at least 4 levels):
   
	Unclassified, Confidential, Secret, TopSecret or higher.

   Domains also have an exclusivity mode flag (open or exclusive).

   Only participants that have a high enough security level can join a secure
   domain (level > Unclassified) if the domain is set to non-exclusive (open).
   In exclusive secure domains, participants need not only the appropriate
   security level but also require explicit domain topic access rules.

   Unknown participants always have a security level of Unclassified and are
   only allowed to join Unclassified domains.

   Multicast is automatically disabled within secure domains.
   Both discovery traffic and user topic traffic is encrypted in a secure
   domain.

   Topic rules can be specified, both for specific participants and on domain
   level.  Participant-level topic checks are verified first, and these can 
   differ for different domains.  If no match is found, the general domain
   topic rules are verified.  Unless specified explicitly, secure domains will
   refuse all unknown topics.

   Domain identifiers can be set in all rules to either '*' which indicates all
   other (i.e. non-specified) domains, or to a specific number (<= 255). 

   Wildcard specifications, both partial as well as complete, can be given
   in all topic rules (see nmatch() for details on allowed wildcards).

   Partition access rules are similar to topic access rules.  They can also be
   specified on both Participant level and on Domain level.
   
   Other QoS parameters for DataReaders and DataWriters could be authenticated
   in the future since the security plugin functions have access to all QoS
   parameters, but this has currently not been worked out yet.
*/

#ifndef __msecplug_h_
#define	__msecplug_h_

#ifndef DDS_NATIVE_SECURITY

#include "dds/dds_security.h"
#include "dds/dds_error.h"
#include "openssl/engine.h"
#include <openssl/ssl.h>

/* secure transport types on both sides*/
#define TRANS_BOTH_NONE		0
#define TRANS_BOTH_DTLS_UDP	0x10001
#define TRANS_BOTH_TLS_TCP	0x20002
#define TRANS_BOTH_DDS_SEC	0x40004

#define	MAX_DOMAINS		4	/* Max. # of domain specifications. */
#define	MAX_DOMAIN_TOPICS	8	/* Max. # of domain topic rules. */
#define	MAX_DOMAIN_PARTITIONS	4	/* Max. # of domain partition rules. */
#define	MAX_PARTICIPANT_NAME	128	/* Max. participant name length. */
#define	MAX_KEY_LENGTH		128	/* Max. key length. */
#define MAX_ID_HANDLES		64	/* Max. # of participants. */
#define	MAX_USER_TOPICS		16	/* Max. # of user topic rules. */
#define	MAX_USER_PARTITIONS	8	/* Max. # of user partition rules. */
#define	MAX_PERM_HANDLES	256	/* Max. # of permissions records. */
#define MAX_ENGINES             8       /* Max. # of engines. */

typedef unsigned DomainHandle_t;
typedef unsigned ParticipantHandle_t;
typedef unsigned TopicHandle_t;
typedef unsigned PartitionHandle_t;
typedef unsigned IdentityHandle_t;
typedef unsigned PermissionsHandle_t;

typedef enum {
	DS_UNCLASSIFIED,
	DS_CONFIDENTIAL,
	DS_SECRET,
	DS_TOP_SECRET
} MSAccess_t;

typedef enum {
	TA_CREATE = 1,
	TA_DELETE = 2,
	TA_WRITE = 4,
	TA_READ = 8
} MSMode_t;

#define	TA_NONE	0
#define	TA_ALL	(TA_CREATE | TA_DELETE | TA_WRITE | TA_READ)

typedef struct ms_topic_st MSTopic_t;
struct ms_topic_st {
	MSTopic_t       *next;
	MSTopic_t       *prev;
	unsigned        index;
	char		*name;
	MSMode_t	mode;
	int             blacklist;
	unsigned        refreshed;
};

typedef struct ms_partition_st MSPartition_t;
struct ms_partition_st {
	MSPartition_t   *next;
	MSPartition_t   *prev;
	unsigned        index;
 	char		*name;
	MSMode_t	mode;
	int             blacklist;
	unsigned        refreshed;
};

typedef struct ms_user_topic_st MSUTopic_t;
struct ms_user_topic_st {
	MSUTopic_t      *next;
	MSUTopic_t      *prev;
	DDS_DomainId_t	id;
	MSTopic_t	topic;
};

typedef struct ms_user_partition_st MSUPartition_t;
struct ms_user_partition_st {
	MSUPartition_t  *next;
	MSUPartition_t  *prev;
	DDS_DomainId_t	id;
	MSPartition_t	partition;
};

struct ms_topic_list {
	MSTopic_t *head;
	MSTopic_t *tail;
};

struct ms_utopic_list {
	MSUTopic_t *head;
	MSUTopic_t *tail;
};

struct ms_partition_list {
	MSPartition_t *head;
	MSPartition_t *tail;
};

struct ms_upartition_list {
	MSUPartition_t *head;
	MSUPartition_t *tail;
};

typedef struct ms_domain_st MSDomain_t;
struct ms_domain_st {
	MSDomain_t	*next;
	MSDomain_t	*prev;
	unsigned	handle;
	DDS_DomainId_t	domain_id;
	MSAccess_t	access;
	int		exclusive;
	uint32_t	transport;
	int             blacklist;
	struct ms_topic_list topics;
	unsigned	ntopics;
	struct ms_partition_list partitions;
	unsigned	npartitions;
	unsigned        refreshed;
};

typedef struct ms_domains_st {
	MSDomain_t	*head;
	MSDomain_t	*tail;
} MSDomains_t;

typedef struct ms_participant_st MSParticipant_t;
struct ms_participant_st {
	MSParticipant_t	*next;
	MSParticipant_t	*prev;
	unsigned	handle;
	char		name [MAX_PARTICIPANT_NAME];
	unsigned char	key [MAX_KEY_LENGTH];
	size_t		key_length;
	DDS_Credentials	*credentials;
	MSParticipant_t	*cloned;
	MSAccess_t	access;
	int             blacklist;
	struct ms_utopic_list topics;
	unsigned	ntopics;
	struct ms_upartition_list partitions;
	unsigned	npartitions;
	unsigned        refreshed;
	unsigned        part_handle;
	unsigned        input_number;
};

typedef struct ms_participants_st {
	MSParticipant_t	*head;
	MSParticipant_t	*tail;
} MSParticipants_t;

extern MSDomain_t		*domain_handles [MAX_DOMAINS];
extern struct ms_domains_st	domains;
extern unsigned			num_domains;

extern struct ms_participants_st	participants;
extern unsigned				num_ids;
extern MSParticipant_t			*id_handles [MAX_ID_HANDLES];

extern MSDomain_t *lookup_domain (unsigned domain_id, int specific);
extern MSParticipant_t *lookup_participant (const char *name);

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_policy (void);

DDS_EXPORT DDS_ReturnCode_t DDS_SP_init_engine(const char *engine_id, 
				    void* (*engine_constructor_callback)(void));

typedef DDS_ReturnCode_t (*sp_extra_authentication_check_fct) (void *context, const char *name);

DDS_EXPORT void DDS_SP_set_extra_authentication_check (sp_extra_authentication_check_fct fct);

/* Set a certificate validation callback, for extra specific certificate validation checks */

/************************************/
/* ACCESS DB MANIPULATION FUNCTIONS */
/************************************/

DDS_EXPORT DDS_ReturnCode_t DDS_SP_update_start (void);

/* call this before you start updateing the database*/

DDS_EXPORT DDS_ReturnCode_t DDS_SP_update_done (void);

/* This should be called when every change to the database is done */

DDS_EXPORT DomainHandle_t DDS_SP_add_domain (void);

/* Add a domain rule where everything is allowed. The return value is the domain handle */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_domain (DomainHandle_t handle);

/* Removes the domain rule and all it's topic and partition rules associated with it */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_domain_access (DomainHandle_t handle,
					   DDS_DomainId_t domain_id,
					   MSAccess_t     access,
					   int            exclusive,
					   uint32_t       transport,
					   int            blacklist);

/* Set a more refined domain access control */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_get_domain_access (DomainHandle_t domain_handle,
					   DDS_DomainId_t *domain_id,
					   MSAccess_t     *access,
					   int            *exclusive,
					   uint32_t       *transport,
					   int            *blacklist);

/* Get the domain access rules */

DDS_EXPORT int DDS_SP_get_domain_handle (DDS_DomainId_t domain_id);

/* Get the domain handle, based on the domain_id */

DDS_EXPORT ParticipantHandle_t DDS_SP_add_participant (void);

/* Add a participant rule where everything is allowed */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_participant (ParticipantHandle_t handle);

/* Removes the participant rule and all it's topic and patition rules associated with it */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_participant_access (ParticipantHandle_t handle, 
						char                *new_name, 
						MSAccess_t          access,
						int                 blacklist);

/* Set a more refined participant access control */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_get_participant_access (ParticipantHandle_t participant_handle,
						char                *name, 
						MSAccess_t          *access,
						int                 *blacklist);

/* Get the participant access rules */

DDS_EXPORT int DDS_SP_get_participant_handle (char *name);

/* Get the participant handle, based on the name */

DDS_EXPORT TopicHandle_t DDS_SP_add_topic (ParticipantHandle_t participant_handle, 
				DomainHandle_t       domain_handle);

/* Add a new allow all topic rule to either the domain or participant, but not both.
   Return the topic handle */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_topic (ParticipantHandle_t participant_handle,
				      DomainHandle_t      domain_handle,
				      TopicHandle_t       topic_handle);

/* Removes the topic rule from either the domain or participant, but not both */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_topic_access (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  TopicHandle_t       topic_handle,
					  char                *name,
					  MSMode_t            mode,
					  DDS_DomainId_t      id,
					  int                 blacklist);

/* Set a more refined topic access control, to either the domain or participant, but not both */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_get_topic_access (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  TopicHandle_t       topic_handle, 
					  char                *name,
					  MSMode_t            *mode,
					  DDS_DomainId_t      *id,
					  int                 *blacklist);

/* Get the topic access rules */

DDS_EXPORT int DDS_SP_get_topic_handle (ParticipantHandle_t participant_handle,
					DomainHandle_t      domain_handle,
					char                *name,
					MSMode_t            mode);

/* Get the topic handle for either participant or domain, based on name */

DDS_EXPORT PartitionHandle_t DDS_SP_add_partition (ParticipantHandle_t participant_handle, 
					DomainHandle_t      domain_handle);

/* Add a new allow all partition rule to either the domain or participant, but not both
   Return the partition handle*/

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_partition (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  PartitionHandle_t   partition_id);

/* Remove the partition rule from either the domain or participant, but not both */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_partition_access (ParticipantHandle_t participant_handle,
					      DomainHandle_t      domain_handle,
					      PartitionHandle_t   partition_id,
					      char                *name,
					      MSMode_t            mode,
					      DDS_DomainId_t      id,
					      int                 blacklist);

/* Set a more refined partition access control for either the domain or participant,
   but not both */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_get_partition_access (ParticipantHandle_t participant_handle,
					      DomainHandle_t      domain_handle,
					      PartitionHandle_t   partition_id,
					      char                *name,
					      MSMode_t            *mode,
					      DDS_DomainId_t      *id,
					      int                 *blacklist);

/* Get the partition access rules */

DDS_EXPORT int DDS_SP_get_partition_handle (ParticipantHandle_t participant_handle,
					    DomainHandle_t      domain_handle,
					    char                *name,
					    MSMode_t            mode);

/* Get the partition handle for either participant or domain, based on name */ 

DDS_EXPORT DDS_ReturnCode_t DDS_SP_access_db_cleanup (void);

/* Cleanup the access db */

DDS_EXPORT void DDS_SP_engine_cleanup (void);

/* Cleanup the engines */

void DDS_SP_dump (void);

/* Dumps the access database to log_printf */

int DDS_SP_parse_xml (const char *fn);

/* Parse security rules from a security.xml file. */

DDS_EXPORT void DDS_SP_init_library (void);

/*********************************/
/* DDS CALLBACK SETTER FUNCTIONS */
/*********************************/

typedef DDS_ReturnCode_t (*msp_auth_revoke_listener_fct) (IdentityHandle_t id);

DDS_ReturnCode_t msp_set_auth_listener (msp_auth_revoke_listener_fct fct);

/* On revoke identity listener for DDS */

typedef DDS_ReturnCode_t (*msp_acc_revoke_listener_fct) (PermissionsHandle_t perm);

DDS_ReturnCode_t msp_set_acc_listener (msp_acc_revoke_listener_fct fct);

/* On revoke permissions listener for DDS */


/* Handle DDS security via this plugin. */

#endif

#endif /* !__msecplug_h_ */

