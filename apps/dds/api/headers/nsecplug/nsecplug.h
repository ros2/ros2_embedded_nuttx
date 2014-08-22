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

#ifndef __nsecplug_h_
#define __nsecplug_h_

#include "dds/dds_security.h"
#include "dds/dds_error.h"

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

/* secure transport types on both sides*/
#define TRANS_BOTH_NONE		0
#define TRANS_BOTH_DTLS_UDP	0x10001
#define TRANS_BOTH_TLS_TCP	0x20002
#define TRANS_BOTH_DDS_SEC	0x40004

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

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_policy (void);

DDS_EXPORT DDS_ReturnCode_t DDS_SP_init_engine(const char *engine_id, 
				    void* (*engine_constructor_callback)(void));

DDS_EXPORT void DDS_SP_engine_cleanup (void);

DDS_EXPORT void DDS_SP_init_library (void);

typedef DDS_ReturnCode_t (*sp_extra_authentication_check_fct) (void *context, const char *name);

DDS_EXPORT void DDS_SP_set_extra_authentication_check (sp_extra_authentication_check_fct fct);

/* Set a certificate validation callback, for extra specific certificate validation checks */

/* Domain functions */

DDS_EXPORT DomainHandle_t DDS_SP_add_domain (void);

/* Add a domain rule where everything is allowed. The return value is the domain handle */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_domain (DomainHandle_t handle);

/* Removes the domain rule and all it's topic and partition rules associated with it */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_domain_access (DomainHandle_t handle,
						      DDS_DomainId_t domain_id,
						      MSAccess_t     access,
						      int            exclusive,
						      int            controlled,
						      int            msg_encrypt,
						      uint32_t       transport,
						      int            blacklist);

/* Set a more refined domain access control */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_get_domain_access (DomainHandle_t domain_handle,
						      DDS_DomainId_t *domain_id,
						      MSAccess_t     *access,
						      int            *exclusive,
						      int            *controlled,
						      int            *msg_encrypt,
						      uint32_t       *transport,
						      int            *blacklist);

/* Get the domain access rules */

DDS_EXPORT int DDS_SP_get_domain_handle (DDS_DomainId_t domain_id);

/* Get the domain handle, based on the domain_id */

/* Partitipant functions */

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

/* Topic functions */

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
						     int                 controlled,
						     int                 disc_enc,
						     int                 submsg_enc,
						     int                 payload_enc,
						     DDS_DomainId_t      id,
						     int                 blacklist);

/* Set a more refined topic access control, to either the domain or participant, but not both */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_get_topic_access (ParticipantHandle_t participant_handle,
						     DomainHandle_t      domain_handle,
						     TopicHandle_t       topic_handle, 
						     char                *name,
						     MSMode_t            *mode,
						     int                 *controlled,
						     int                 *disc_enc,
						     int                 *submsg_enc,
						     int                 *payload_enc,
						     DDS_DomainId_t      *id,
						     int                 *blacklist);

/* Get the topic access rules */

DDS_EXPORT int DDS_SP_get_topic_handle (ParticipantHandle_t participant_handle,
					DomainHandle_t      domain_handle,
					char                *name,
					MSMode_t            mode);

/* Get the topic handle for either participant or domain, based on name */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_fine_grained_topic (ParticipantHandle_t participant_handle,
						DomainHandle_t      domain_handle,
						TopicHandle_t       handle,
						ParticipantHandle_t read [MAX_ID_HANDLES],
						int                 nb_read,
						ParticipantHandle_t write [MAX_ID_HANDLES],
						int                 nb_write);

/* Set fine grained rules defined by the admin */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_fine_grained_topic (ParticipantHandle_t participant_handle,
						   DomainHandle_t      domain_handle,
						   TopicHandle_t       handle);

/* Remove fine grained rules defined by the admin */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_set_fine_grained_app_topic (ParticipantHandle_t participant_handle,
						    DomainHandle_t      domain_handle,
						    TopicHandle_t       handle,
						    ParticipantHandle_t read [MAX_ID_HANDLES],
						    int                 nb_read,
						    ParticipantHandle_t write [MAX_ID_HANDLES],
						    int                 nb_write);

/* Set fine grained topic rules defined by the application */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_remove_fine_grained_app_topic (ParticipantHandle_t participant_handle,
						       DomainHandle_t      domain_handle,
						       TopicHandle_t       handle);

/* Remove fine grained topic rules defined by the application */

/* Partition functions */

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

/* DB functions */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_update_start (void);

/* call this before you start updateing the database*/

DDS_EXPORT DDS_ReturnCode_t DDS_SP_update_done (void);

/* This should be called when every change to the database is done */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_access_db_cleanup (void);

/* Cleanup of the database */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_access_db_dump (void);

/* print the access database. */

DDS_EXPORT DDS_ReturnCode_t DDS_SP_parse_xml (char *path);

/* Parse the xml file and fill in the access control database */

/* Policy functions */

typedef DDS_ReturnCode_t (*sp_dds_policy_content_fct) (uintptr_t userdata, uint64_t *seqnr, char *content, size_t *length, int set);

typedef DDS_ReturnCode_t (*sp_dds_userdata_match_fct) (const char *topic_name, const char *r_userdata, const char *w_userdata);

DDS_EXPORT void DDS_SP_set_policy_cb (sp_dds_policy_content_fct policy_cb, uintptr_t userdata);

DDS_EXPORT void DDS_SP_set_userdata_match_cb (sp_dds_userdata_match_fct match_cb);

#endif
