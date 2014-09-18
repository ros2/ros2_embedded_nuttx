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

/* pid.h -- This module is responsible for handling ParameterId handling in the
            context of:

		- Discovery data (QoS data + extra fields):
			* ParticipantBuiltinTopicData
			* PublicationBuiltinTopicData
			* SubscriptionBuiltinTopicData
			* TopicBuiltinTopicData
			* ParticipantProxy
			* ReaderProxy
			* WriterProxy
			* SPDPdiscoveredParticipantData

		- In-line QoS (QoS subset + extra fields)
 */

#ifndef __pid_h_
#define	__pid_h_

#include "dds_data.h"
#include "rtps_data.h"

#ifdef _WIN32
#include <Windows.h>
#define ssize_t SSIZE_T
#endif

typedef uint16_t ParameterId_t;

#define	PID_VENDOR_SPECIFIC	0x8000	/* Vendor-specific PID. */
#define	PID_MUST_PARSE		0x4000	/* Parameter may not be ignored. */
#define	PID_VALUE		0x3fff	/* Nominal PID value. */

/* Parameter Id definitions: */
#define	PID_PAD				0	/* - For padding only. */
#define	PID_SENTINEL			1	/* - Last parameter. */
#define PID_PARTICIPANT_LEASE_DURATION	2	/* Duration_t */
#define	PID_TIME_BASED_FILTER		4	/* TimeBasedFilterQosPolicy */
#define	PID_TOPIC_NAME			5	/* String [256] */
#define	PID_OWNERSHIP_STRENGTH		6	/* OwnershipStrengthQosPolicy */
#define	PID_TYPE_NAME			7	/* String [256] */
#define	PID_META_MULTICAST_IPADDRESS	11	/* IPv4Address_t */
#define	PID_DEFAULT_UNICAST_IPADDRESS	12	/* IPv4Address_t */
#define	PID_META_UNICAST_PORT		13	/* Port_t */
#define	PID_DEFAULT_UNICAST_PORT	14	/* Port_t */
#define	PID_MULTICAST_IPADDRESS		17	/* IPv4Address_t */
#define	PID_PROTOCOL_VERSION		21	/* ProtocolVersion_t */
#define	PID_VENDOR_ID			22	/* VendorId_t */
#define	PID_RELIABILITY			26	/* ReliabilityQosPolicy */
#define	PID_LIVELINESS			27	/* LivelinessQosPolicy */
#define	PID_DURABILITY			29	/* DurabilityQosPolicy */
#define	PID_DURABILITY_SERVICE		30	/* DurabilityServiceQosPolicy */
#define	PID_OWNERSHIP			31	/* OwnershipQosPolicy */
#define	PID_PRESENTATION		33	/* PresentationQosPolicy */
#define	PID_DEADLINE			35	/* DeadlineQosPolicy */
#define	PID_DESTINATION_ORDER		37	/* DestinationOrderQosPolicy */
#define	PID_LATENCY_BUDGET     		39	/* LatencyBudgetQosPolicy */
#define	PID_PARTITION			41	/* PartitionQosPolicy */
#define	PID_LIFESPAN			43	/* LifespanQosPolicy */
#define	PID_USER_DATA			44	/* UserDataQosPolicy */
#define	PID_GROUP_DATA			45	/* GroupDataQosPolicy */
#define	PID_TOPIC_DATA			46	/* TopicDataQosPolicy */
#define	PID_UNICAST_LOCATOR		47	/* Locator_t */
#define	PID_MULTICAST_LOCATOR		48	/* Locator_t */
#define	PID_DEFAULT_UNICAST_LOCATOR	49	/* Locator_t */
#define	PID_META_UNICAST_LOCATOR	50	/* Locator_t */
#define	PID_META_MULTICAST_LOCATOR	51	/* Locator_t */
#define PID_PARTICIPANT_MAN_LIVELINESS	52	/* Count_t */
#define PID_CONTENT_FILTER_PROPERTY	53	/* ContentFilter_t */
#define	PID_HISTORY			64	/* HistoryQosPolicy */
#define	PID_RESOURCE_LIMITS		65	/* ResourceLimitsQosPolicy */
#define	PID_EXPECTS_INLINE_QOS		67	/* boolean */
#define PID_PARTICIPANT_BUILTIN_EPS	68	/* uint32_t */
#define	PID_META_UNICAST_IPADDRESS	69	/* IPv4Address_t */
#define	PID_META_MULTICAST_PORT		70	/* Port_t */
#define	PID_DEFAULT_MULTICAST_LOCATOR	72	/* Locator_t */
#define	PID_TRANSPORT_PRIORITY		73	/* TransportPriorityQoSPolicy */
#define PID_PARTICIPANT_GUID		80	/* GUID_t */
#define PID_PARTICIPANT_ENTITYID	81	/* EntityId_t */
#define PID_GROUP_GUID			82	/* GUID_t */
#define PID_GROUP_ENTITYID		83	/* EntityId_t */
#define	PID_CONTENT_FILTER_INFO		85	/* ContentFilterInfo_t */
#define	PID_COHERENT_SET		86	/* SequenceNumber_t */
#define	PID_DIRECTED_WRITE		87	/* sequence<GUID_t> */
#define PID_BUILTIN_ENDPOINT_SET	88	/* BuiltinEndpointSet_t */
#define PID_PROPERTY_LIST		89	/* sequence<Property_t> */
#define	PID_ENDPOINT_GUID		90	/* GUID_t */
#define PID_TYPE_MAX_SIZE_SERIALIZED	96	/* long */
#define	PID_ORIGINAL_WRITER_INFO	97	/* OriginalWriterInfo_t */
#define PID_ENTITY_NAME			98	/* EntityName_t */
#define PID_KEY_HASH			112	/* KeyHash_t */
#define PID_STATUS_INFO			113	/* StatusInfo_t */

#define	PID_ID_TOKEN_ADJ		116	/* Identity Token (adjusted). */
#define	PID_PERMISSIONS_TOKEN_ADJ	117	/* Permissions Token (adjusted). */
#define	PID_DATA_TAGS_ADJ		118	/* DataTags Token (adjusted). */

#define	PID_ID_TOKEN			0x1001	/* Identity Token. */
#define	PID_PERMISSIONS_TOKEN		0x1002	/* Permissions Token. */
#define	PID_DATA_TAGS			0x1003	/* DataTags Token. */

#define	PID_ADJUST_P0			(PID_ID_TOKEN - PID_ID_TOKEN_ADJ)

#define	PID_V_VERSION	(PID_VENDOR_SPECIFIC + 0)	/* Software version. */
#define	PID_V_NO_MCAST	(PID_VENDOR_SPECIFIC + 1)	/* Multicast suppress.*/
#define	PID_V_SEC_CAPS	(PID_VENDOR_SPECIFIC + 2)	/* Security capability*/
#define	PID_V_SEC_LOC	(PID_VENDOR_SPECIFIC + 3)	/* Security locator. */
#define	PID_V_TYPECODE	(PID_VENDOR_SPECIFIC + 4)	/* Typecode. */
#define	PID_V_IDENTITY	(PID_VENDOR_SPECIFIC + 5)	/* Identity token. */
#define	PID_V_PERMS	(PID_VENDOR_SPECIFIC + 6)	/* Permissions token. */
#define	PID_V_FORWARD	(PID_VENDOR_SPECIFIC + 7)	/* Forwarding caps. */

typedef struct parameter_st {
	ParameterId_t		parameter_id;
	uint16_t		length;
	unsigned char		value [4];	/* If present: a multiple of 4. */
} Parameter_t;

typedef struct pid_set_st {
	uint32_t		pids [4];	/* Set of PID values. */
} PIDSet_t;

#define	PID_ADD(s,pid)	(s).pids[(pid)>>5]|=(1<<((pid)&0x1f))
#define	PID_REM(s,pid)	(s).pids[(pid)>>5]&=~(1<<((pid)&0x1f))
#define	PID_INSET(s,pid) ((s).pids[(pid)>>5]&(1<<((pid)&0x1f)))

typedef struct content_filter_info_st {
	unsigned		num_bitmaps;
	uint32_t		*bitmaps;
	unsigned		num_signatures;
	uint32_t		*signatures;
} ContentFilterInfo_t;

typedef struct discovered_participant_data_st {
	ParticipantProxy	proxy;
	String_t		*user_data;
	String_t		*entity_name;		/* Entity name. */
} DiscoveredParticipantData;

typedef struct spdp_discovered_participant_data_st {
	ParticipantProxy	proxy;			/* Proxy info. */
	String_t		*user_data;		/* User Data QoS. */
	String_t		*entity_name;		/* Entity name. */
	Duration_t		lease_duration;		/* Timeout to remove. */
#ifdef DDS_SECURITY
	Token_t			*id_tokens;		/* Identity Token(s). */
	Token_t			*p_tokens;		/* Permission Token(s). */
#endif
	Property_t		*properties;		/* Extra properties. */
} SPDPdiscoveredParticipantData;

typedef struct discovered_topic_data_st {
	String_t		*name;
	String_t		*type_name;
	DiscoveredTopicQos	qos;
#ifdef DDS_TYPECODE
	unsigned char		*typecode;
	VendorId_t		vendor_id;
#endif
} DiscoveredTopicData;

typedef struct writer_proxy_st {
	GUID_t			guid;
	LocatorList_t		ucast;
	LocatorList_t		mcast;
} WriterProxy;

typedef struct discovered_writer_data_st {
	WriterProxy		proxy;
	String_t		*topic_name;
	String_t		*type_name;
	DiscoveredWriterQos	qos;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Property_t		*tags;
#endif
#ifdef DDS_TYPECODE
	unsigned char		*typecode;
	VendorId_t		vendor_id;
#endif
} DiscoveredWriterData;

typedef struct reader_proxy_st {
	GUID_t			guid;
	int			exp_il_qos;
	LocatorList_t		ucast;
	LocatorList_t		mcast;
} ReaderProxy;

typedef struct discovered_reader_data_st {
	ReaderProxy		proxy;
	String_t		*topic_name;
	String_t		*type_name;
	DiscoveredReaderQos	qos;
	FilterData_t		*filter;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Property_t		*tags;
#endif
#ifdef DDS_TYPECODE
	unsigned char		*typecode;
	VendorId_t		vendor_id;
#endif
} DiscoveredReaderData;


DDS_SEQUENCE(GUID_t, DDS_GUIDSeq);

typedef struct status_info_st {
	unsigned char		value [4];
} StatusInfo_t;

typedef struct original_writer_info_st {
	GUID_t			guid;
	SequenceNumber_t	seqnr;
	Parameter_t		**qos;
} OriginalWriterInfo_t;

typedef struct inline_qos_st {
	String_t			*topic_name;
	DDS_DurabilityQosPolicy		durability;
	DDS_PresentationQosPolicy	presentation;
	DDS_DeadlineQosPolicy		deadline;
	DDS_LatencyBudgetQosPolicy	latency_budget;
	DDS_OwnershipQosPolicy		ownership;
	DDS_OwnershipStrengthQosPolicy	ownership_strength;
	DDS_LivelinessQosPolicy		liveliness;
	Strings_t			partition;
	DDS_ReliabilityQosPolicy	reliability;
	DDS_TransportPriorityQosPolicy	transport_priority;
	DDS_LifespanQosPolicy		lifespan;
	DDS_DestinationOrderQosPolicy	destination_order;
	ContentFilterInfo_t		content_filter_info;
	SequenceNumber_t		coherent_set;
	DDS_GUIDSeq			directed_write;
	OriginalWriterInfo_t		original_writer_info;
	KeyHash_t			key_hash;
	StatusInfo_t			status_info;
} InlineQos_t;


void pid_init (void);

/* Initialize various PID parsing data structures. */


/* Message parsing functions.
   -------------------------- */

ssize_t pid_parse_inline_qos (DBW         *walk,
			      InlineQos_t *qp,
			      PIDSet_t    *pids,
			      int         swap);

/* Parse a Parameter Id list as used in the InlineQoS field until either the end
   of the list is reached or the data size (max) is not sufficient to contain
   the list or an error occurred.
   If swap is set, it specifies that endian swapping is needed.
   If parsing is successful, the list size is returned and pids will specify
   which PIDs were successfully parsed.  If errors occurred, a negative error
   code is returned. */


ssize_t pid_parse_participant_data (DBW                           *walk,
				    SPDPdiscoveredParticipantData *dp,
				    PIDSet_t                      pids [2],
				    int                           swap);

/* Parse a Parameter Id list as used in the SPDPdiscoveredParticipantData of the
   builtin SPDPbuiltinParticipantReader/Writer endpoints.
   See pid_parse_inline_qos() for parse function details. */

ssize_t pid_parse_reader_data (DBW                  *walk,
			       DiscoveredReaderData *dp,
			       PIDSet_t             pids [2],
			       int                  swap);

/* Parse a Parameter Id list as used in the DiscoveredReaderData of the builtin
   SEDPbuiltinSubscriptionsReader/Writer endpoints.
   The same handling is done as with pid_parse_inline_qos(). */

ssize_t pid_parse_writer_data (DBW                  *walk,
			       DiscoveredWriterData *dp,
			       PIDSet_t             pids [2],
			       int                  swap);

/* Parse a Parameter Id list as used in the DiscoveredWriterData of the builtin
   SEDPbuiltinPublicationsReader/Writer endpoints.
   The same handling is done as with pid_parse_inline_qos(). */

ssize_t pid_parse_topic_data (DBW                 *walk,
			      DiscoveredTopicData *dp,
			      PIDSet_t            pids [2],
			      int                 swap);

/* Parse a Parameter Id list as used in the DiscoveredTopicData of the builtin
   SEDPbuiltinTopicsReader/Writer endpoints.
   The same handling is done as with pid_parse_inline_qos(). */


#define pid_participant_key_size()	sizeof (GuidPrefix_t)

/* Returns the size of a Participant key. */

#define pid_reader_key_size()	sizeof (GUID_t)

/* Returns the size of a Reader key. */

#define pid_writer_key_size()	sizeof (GUID_t)

/* Returns the size of a Writer key. */

#define pid_topic_key_size()	sizeof (KeyHash_t)

/* Returns the size of the topic key. */

void topic_key_from_name (const char *name,
			  unsigned   name_length,
			  const char *type_name,
			  unsigned   type_name_length,
			  KeyHash_t  *key);

/* Get a topic key from the topic name and type name. */

int pid_parse_participant_key (DBW walk, unsigned char *key, int swap);

/* Parse a Parameter Id list as used in the SPDPdiscoveredParticipantData of the
   builtin SEDPbuiltinParticipantReader/Writer endpoints and set key to a GUID
   prefix field. */

int pid_parse_reader_key (DBW walk, unsigned char *key, int swap);

/* Parse a Parameter Id list as used in the DiscoveredReaderData of the
   builtin SEDPbuiltinSubscriptionsReader/Writer endpoints and set key to the
   GUID field. */

int pid_parse_writer_key (DBW walk, unsigned char *key, int swap);

/* Parse a Parameter Id list as used in the DiscoveredWriterData of the
   builtin SEDPbuiltinPublicationsReader/Writer endpoints and set key to the
   GUID field. */

int pid_parse_topic_key (DBW walk, unsigned char *key, int swap);

/* Parse a Parameter Id list as used in the DiscoveredTopicData of the builtin
   SEDPbuiltinTopicsReader/Writer endpoints and return a pointer to the key
   field. */


void pid_participant_data_cleanup (SPDPdiscoveredParticipantData *dp);

/* Cleanup received participant data extra allocations. */

void pid_topic_data_cleanup (DiscoveredTopicData *tp);

/* Cleanup received topic data extra allocations. */

void pid_reader_data_cleanup (DiscoveredReaderData *rp);

/* Cleanup received subscription data extra allocations. */

void pid_writer_data_cleanup (DiscoveredWriterData *wp);

/* Cleanup received publication data extra allocations. */


/* Message creation functions.
   --------------------------- */

ssize_t pid_str_write (unsigned char *dst, const void *src);

/* Copy a String_t * to PID data in CDR format. */

ssize_t pid_size (unsigned pid, const void *data);

/* Return the minimum size required for the given Parameter Id. */

ssize_t pid_add (unsigned char *buf, unsigned pid, const void *data);

/* Put a Parameter Id in the buffer at the given offset.
   The resulting buffer size will be returned. */

size_t pid_finish (unsigned char *buf);

/* Same semantics as pid_add (), but adds the final sentinel. */

ssize_t pid_participant_data_size (const SPDPdiscoveredParticipantData *dp);

/* Return the size of the data field for the given instance of the builtin
   SPDPbuiltinParticipantReader/Writer endpoints. */

ssize_t pid_reader_data_size (const DiscoveredReaderData *dp);

/* Return the size of the data field for the given instance of the builtin
   SEDPbuiltinSubscriptionsReader/Writer endpoints. */

ssize_t pid_writer_data_size (const DiscoveredWriterData *dp);

/* Return the size of the data field for the given instance of the builtin
   SEDPbuiltinPublicationsReader/Writer endpoints. */

ssize_t pid_topic_data_size (const DiscoveredTopicData *dp);

/* Return the size of the data field for the given instance of the builtin
   SEDPbuiltinTopicReader/Writer endpoints. */

ssize_t pid_add_participant_data (unsigned char                       *dp,
				  const SPDPdiscoveredParticipantData *pp);

/* Adds all Parameter Ids as needed for the data field of the builtin
   SPDPbuiltinParticipantReader/Writer endpoints. */

ssize_t pid_add_reader_data (unsigned char *dp, const DiscoveredReaderData *rp);

/* Adds all Parameter Ids as needed for the data field of the builtin
   SPDPbuiltinSubscriptionsReader/Writer endpoints. */

ssize_t pid_add_writer_data (unsigned char *dp, const DiscoveredWriterData *wp);

/* Adds all Parameter Ids as needed for the data field of the builtin
   SPDPbuiltinPublicationsReader/Writer endpoints. */

ssize_t pid_add_topic_data (unsigned char *dp, const DiscoveredTopicData *tp);

/* Adds all Parameter Ids as needed for the data field of the builtin
   SPDPbuiltinTopicReader/Writer endpoints. */

#endif /* !__pid_h_ */

