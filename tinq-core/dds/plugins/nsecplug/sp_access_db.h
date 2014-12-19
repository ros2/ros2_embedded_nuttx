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

#ifndef __sp_access_db_h_
#define	__sp_access_db_h_

#include "dds/dds_security.h"
#include "sp_auth.h"
#include "sp_data.h"
#include "nsecplug/nsecplug.h"
#include "thread.h"

extern lock_t sp_lock;

void sp_access_init (void);

int sp_access_has_wildcards (const char *data);

/* Domain functions */

MSDomain_t *sp_access_get_domain (DomainHandle_t handle);

int sp_access_get_domain_handle (DDS_DomainId_t id);

MSDomain_t *sp_access_add_domain (DomainHandle_t *handle);

DDS_ReturnCode_t sp_access_remove_domain (DomainHandle_t handle);

MSDomain_t *sp_access_lookup_domain (unsigned domain_id, 
			   int      specific);

/* Participant functions */

MSParticipant_t *sp_access_get_participant (ParticipantHandle_t handle);

MSParticipant_t *sp_access_get_participant_by_perm (PermissionsHandle_t perm);

int sp_access_get_participant_handle (char *name);

MSParticipant_t *sp_access_add_participant (ParticipantHandle_t *handle);

DDS_ReturnCode_t sp_access_remove_participant (ParticipantHandle_t handle);

int sp_access_is_already_cloned (const unsigned char *key,
				 size_t              klength);

DDS_ReturnCode_t sp_access_clone_participant (MSParticipant_t     *wp,
					      const char          *name,
					      size_t              length,
					      const unsigned char *key,
					      size_t              klength,
					      IdentityHandle_t    *handle);

typedef DDS_ReturnCode_t (*SP_ACCESS_PARTICIPANT_CHECK) (MSParticipant_t *p, void *data);

DDS_ReturnCode_t sp_access_participant_walk (SP_ACCESS_PARTICIPANT_CHECK fnc, void *data);

DDS_ReturnCode_t sp_access_add_unchecked_participant (const unsigned char *identity,
						      size_t              length,
						      const unsigned char *key,
						      size_t              klength,
						      IdentityHandle_t    *handle);

DDS_ReturnCode_t sp_access_remove_unchecked_participant (IdentityHandle_t id);

MSParticipant_t *sp_access_lookup_participant (const char *name);

MSParticipant_t *sp_access_lookup_participant_by_perm (unsigned char *permissions, size_t length);

/* Topic functions */

void *sp_access_get_topic (TopicHandle_t handle, unsigned parent_handle, ListTypes type);

int sp_access_get_topic_handle (char *name, MSMode_t mode, unsigned parent_handle, ListTypes type);

void *sp_access_add_topic (TopicHandle_t *handle, unsigned parent_handle, ListTypes type);

DDS_ReturnCode_t sp_access_remove_topic (TopicHandle_t handle, unsigned parent_handle, ListTypes type);

/* Partition functions */

void *sp_access_get_partition (PartitionHandle_t handle, unsigned parent_handle, ListTypes type);

int sp_access_get_partition_handle (char *name, MSMode_t mode, unsigned parent_handle, ListTypes type);

void *sp_access_add_partition (PartitionHandle_t *handle, unsigned parent_handle, ListTypes type);

DDS_ReturnCode_t sp_access_remove_partition (PartitionHandle_t handle, unsigned parent_handle, ListTypes type);

/* DB functions */

DDS_ReturnCode_t sp_access_update_start (void);

DDS_ReturnCode_t sp_access_update_done (void);

DDS_ReturnCode_t sp_access_db_cleanup (void);

#endif
