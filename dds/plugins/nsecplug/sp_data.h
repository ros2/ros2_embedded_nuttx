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

#ifndef _sp_data_h_
#define _sp_data_h_

#include "nsecplug/nsecplug.h"

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

typedef struct ms_fine_topic_st MSFTopic_t;
struct ms_fine_topic_st {
	ParticipantHandle_t read [MAX_ID_HANDLES];
	ParticipantHandle_t write [MAX_ID_HANDLES];
};

/* Topic Rule */
typedef struct ms_topic_st MSTopic_t;
struct ms_topic_st {
	MSTopic_t       *next;
	MSTopic_t       *prev;
	unsigned        index;
	char		*name;
	MSMode_t	mode;
	unsigned	controlled:1;
	unsigned	disc_enc:1;
	unsigned	submsg_enc:1;
	unsigned	payload_enc:1;
	unsigned	crypto_mode:4;
	unsigned	blacklist:1;
	unsigned        refreshed:1;
	MSFTopic_t      *fine_topic;
	MSFTopic_t      *fine_app_topic;
};

/* Partition Rule */
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

/* User Topic Rule */
typedef struct ms_user_topic_st MSUTopic_t;
struct ms_user_topic_st {
	MSUTopic_t      *next;
	MSUTopic_t      *prev;
	DDS_DomainId_t	id;
	MSTopic_t	topic;
};

/* User Partition Rule */
typedef struct ms_user_partition_st MSUPartition_t;
struct ms_user_partition_st {
	MSUPartition_t  *next;
	MSUPartition_t  *prev;
	DDS_DomainId_t	id;
	MSPartition_t	partition;
};

/* Topic List */
struct ms_topic_list {
	MSTopic_t *head;
	MSTopic_t *tail;
};

/* User Topic List */
struct ms_utopic_list {
	MSUTopic_t *head;
	MSUTopic_t *tail;
};

/* Partition List */
struct ms_partition_list {
	MSPartition_t *head;
	MSPartition_t *tail;
};

/* User Partition List */
struct ms_upartition_list {
	MSUPartition_t *head;
	MSUPartition_t *tail;
};

/* Domain Rule */
typedef struct ms_domain_st MSDomain_t;
struct ms_domain_st {
	MSDomain_t	*next;
	MSDomain_t	*prev;
	unsigned	handle;
	DDS_DomainId_t	domain_id;
	MSAccess_t	access;
	unsigned	exclusive:1;
	unsigned	controlled:1;
	unsigned	msg_encrypt:4;
	unsigned        blacklist:1;
	uint32_t	transport;
	struct ms_topic_list topics;
	unsigned	ntopics;
	struct ms_partition_list partitions;
	unsigned	npartitions;
	unsigned        refreshed;
};

/* Domain List */
typedef struct ms_domains_st {
	MSDomain_t	*head;
	MSDomain_t	*tail;
} MSDomains_t;

/* Participant Rule */
typedef struct ms_participant_st MSParticipant_t;
struct ms_participant_st {
	MSParticipant_t	*next;
	MSParticipant_t	*prev;
	unsigned	handle;
	unsigned        permissions_handle;
	unsigned        updated_perm_handle;
	unsigned        permissions;
	char		name [MAX_PARTICIPANT_NAME];
	unsigned char	key [MAX_KEY_LENGTH];
	size_t		key_length;
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

/* Participant List */
typedef struct ms_participants_st {
	MSParticipant_t	*head;
	MSParticipant_t	*tail;
} MSParticipants_t;

typedef enum {
	LIST_DOMAIN,
	LIST_PARTICIPANT
} ListTypes;

#endif
