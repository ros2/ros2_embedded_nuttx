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

/* pid.c -- ParameterId handling functionality. */

#include <stdio.h>
#include <stddef.h>
#include "log.h"
#include "sys.h"
#include "error.h"
#include "set.h"
#include "pid.h"
#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#include "md5.h"
#include "thread.h"
#include "dds/dds_aux.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_data.h"
#include "xcdr.h"
#endif

/*#define INLINE_QOS_SUPPORT	** Define this to add Inline-QoS parameters. */
/*#define LOCK_PARSE		** Define this to lock parsing. */

typedef enum pid_parameter_type {
	PT_Pad,
	PT_End,
	PT_String,
	PT_Boolean,
	PT_Long,
	PT_UnsignedLong,
	PT_SequenceNumber,
	PT_Count,
	PT_Port,
	PT_EntityId,
	PT_Data,
	PT_Duration,
	PT_GUID,
	PT_GUIDPrefix,
	PT_GUIDSequence,
	PT_Locator,
	PT_IPv4Address,
	PT_ProtocolVersion,
	PT_VendorId,
	PT_Reliability,
	PT_Liveliness,
	PT_Durability,
	PT_DurabilityService,
	PT_Ownership,
	PT_OwnershipStrength,
	PT_Presentation,
	PT_Deadline,
	PT_DestinationOrder,
	PT_LatencyBudget,
	PT_Partition,
	PT_Lifespan,
	PT_History,
	PT_TimeBasedFilter,
	PT_ContentFilterInfo,
	PT_ContentFilterProperty,
	PT_ResourceLimits,
	PT_TransportPriority,
	PT_BuiltinEndpointSet,
	PT_PropertySequence,
	PT_OriginalWriterInfo,
	PT_EntityName,
	PT_KeyHash,
	PT_StatusInfo
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
      ,	PT_Token,
	PT_VToken
#endif
      ,	PT_SecLoc
#endif
#ifdef DDS_TYPECODE
      ,	PT_Typecode
#endif
} PIDParType_t;

typedef enum pid_source {
	PST_INLINE_QOS,
	PST_PARTICIPANT,
	PST_READER,
	PST_WRITER,
	PST_TOPIC
} PIDSource_t;

#define	PID_NSRC	((unsigned) PST_TOPIC + 1)

typedef struct pid_desc_st {
	const char	*name;
	unsigned	pid;
	PIDParType_t	ptype;
	short		ofs [PID_NSRC];
} PIDDesc_t;

#define	N_STD_PIDS	(sizeof (known_pids) / sizeof (PIDDesc_t))

static PIDDesc_t known_pids [] = {
	{ "PAD",                PID_PAD,                        PT_Pad,
		{ -1, -1, -1, -1, -1 }},
	{ "USER_DATA",          PID_USER_DATA,                  PT_Data,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, user_data),
		  offsetof (DiscoveredReaderData, qos.user_data),
		  offsetof (DiscoveredWriterData, qos.user_data),
		  -1 }},
	{ "TOPIC_NAME",         PID_TOPIC_NAME,                 PT_String,
		{ offsetof (InlineQos_t, topic_name),
		  -1,
		  offsetof (DiscoveredReaderData, topic_name),
		  offsetof (DiscoveredWriterData, topic_name),
		  offsetof (DiscoveredTopicData, name) }},
	{ "TYPE_NAME",          PID_TYPE_NAME,                  PT_String,
		{ -1, -1, 
		  offsetof (DiscoveredReaderData, type_name),
		  offsetof (DiscoveredWriterData, type_name),
		  offsetof (DiscoveredTopicData, type_name) }},
	{ "GROUP_DATA",         PID_GROUP_DATA,                 PT_Data,
		{ -1, -1, 
		  offsetof (DiscoveredReaderData, qos.group_data),
		  offsetof (DiscoveredWriterData, qos.group_data),
		  -1 }},
	{ "TOPIC_DATA",         PID_TOPIC_DATA,                 PT_Data,
		{ -1, -1, 
		  offsetof (DiscoveredReaderData, qos.topic_data),
		  offsetof (DiscoveredWriterData, qos.topic_data),
		  offsetof (DiscoveredTopicData, qos.topic_data) }},
	{ "DURABILITY",         PID_DURABILITY,                 PT_Durability,
		{ offsetof (InlineQos_t, durability),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.durability),
		  offsetof (DiscoveredWriterData, qos.durability),
		  offsetof (DiscoveredTopicData, qos.durability) }},
	{ "DURABILITY_SERVICE", PID_DURABILITY_SERVICE,         PT_DurabilityService,
		{ -1, -1, -1,
		  offsetof (DiscoveredWriterData, qos.durability_service),
		  offsetof (DiscoveredTopicData, qos.durability_service) }},
	{ "DEADLINE",           PID_DEADLINE,                   PT_Deadline,
		{ offsetof (InlineQos_t, deadline),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.deadline),
		  offsetof (DiscoveredWriterData, qos.deadline),
		  offsetof (DiscoveredTopicData, qos.deadline) }},
	{ "LATENCY_BUDGET",     PID_LATENCY_BUDGET,             PT_LatencyBudget,
		{ offsetof (InlineQos_t, latency_budget),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.latency_budget),
		  offsetof (DiscoveredWriterData, qos.latency_budget),
		  offsetof (DiscoveredTopicData, qos.latency_budget) }},
	{ "LIVELINESS",         PID_LIVELINESS,                 PT_Liveliness,
		{ offsetof (InlineQos_t, liveliness),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.liveliness),
		  offsetof (DiscoveredWriterData, qos.liveliness),
		  offsetof (DiscoveredTopicData, qos.liveliness) }},
	{ "RELIABILITY",        PID_RELIABILITY,                PT_Reliability,
		{ offsetof (InlineQos_t, reliability),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.reliability),
		  offsetof (DiscoveredWriterData, qos.reliability),
		  offsetof (DiscoveredTopicData, qos.reliability) }},
	{ "LIFESPAN",           PID_LIFESPAN,                   PT_Lifespan,
		{ offsetof (InlineQos_t, lifespan),
		  -1, -1, 
		  offsetof (DiscoveredWriterData, qos.lifespan),
		  offsetof (DiscoveredTopicData, qos.lifespan) }},
	{ "DESTINATION_ORDER",  PID_DESTINATION_ORDER,          PT_DestinationOrder,
		{ offsetof (InlineQos_t, destination_order),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.destination_order),
		  offsetof (DiscoveredWriterData, qos.destination_order),
		  offsetof (DiscoveredTopicData, qos.destination_order) }},
	{ "HISTORY",            PID_HISTORY,                    PT_History,
		{ -1, -1, -1, -1,
		  offsetof (DiscoveredTopicData, qos.history) }},
	{ "RESOURCE_LIMITS",    PID_RESOURCE_LIMITS,            PT_ResourceLimits,
		{ -1, -1, -1, -1, 
		  offsetof (DiscoveredTopicData, qos.resource_limits) }},
	{ "OWNERSHIP",          PID_OWNERSHIP,                  PT_Ownership,
		{ offsetof (InlineQos_t, ownership),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.ownership),
		  offsetof (DiscoveredWriterData, qos.ownership),
		  offsetof (DiscoveredTopicData, qos.ownership) }},
	{ "OWNERSHIP_STRENGTH", PID_OWNERSHIP_STRENGTH,         PT_OwnershipStrength,
		{ offsetof (InlineQos_t, ownership_strength),
		  -1, -1, 
		  offsetof (DiscoveredWriterData, qos.ownership_strength),
		  -1 }},
	{ "PRESENTATION",       PID_PRESENTATION,               PT_Presentation,
		{ offsetof (InlineQos_t, presentation),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.presentation),
		  offsetof (DiscoveredWriterData, qos.presentation),
		  -1 }},
	{ "PARTITION",          PID_PARTITION,                  PT_Partition,
		{ offsetof (InlineQos_t, partition),
		  -1, 
		  offsetof (DiscoveredReaderData, qos.partition),
		  offsetof (DiscoveredWriterData, qos.partition),
		  -1 }},
	{ "TIME_BASED_FILTER",  PID_TIME_BASED_FILTER,          PT_TimeBasedFilter,
		{ -1, -1, 
		  offsetof (DiscoveredReaderData, qos.time_based_filter),
		  -1, -1 }},
	{ "TRANSPORT_PRIORITY", PID_TRANSPORT_PRIORITY,         PT_TransportPriority,
		{ offsetof (InlineQos_t, transport_priority),
		  -1, -1, -1, 
		  offsetof (DiscoveredTopicData, qos.transport_priority) }},
	{ "PROTOCOL_VERSION",   PID_PROTOCOL_VERSION,           PT_ProtocolVersion,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.proto_version),
		  -1, -1, -1 }},
	{ "VENDOR_ID",          PID_VENDOR_ID,                  PT_VendorId,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.vendor_id),
#ifdef DDS_TYPECODE
		  offsetof (DiscoveredReaderData, vendor_id),
		  offsetof (DiscoveredWriterData, vendor_id),
#else
		  -1, -1,
#endif
		  -1 }},
	{ "UNICAST_LOC",        PID_UNICAST_LOCATOR,            PT_Locator,
		{ -1, -1, 
		  offsetof (DiscoveredReaderData, proxy.ucast),
		  offsetof (DiscoveredWriterData, proxy.ucast),
		  -1 }},
	{ "MULTICAST_LOC",      PID_MULTICAST_LOCATOR,          PT_Locator,
		{ -1, -1,
		  offsetof (DiscoveredReaderData, proxy.mcast),
		  offsetof (DiscoveredWriterData, proxy.mcast),
		  -1 }},
	{ "MULTICAST_IPv4",     PID_MULTICAST_IPADDRESS,        PT_IPv4Address,
		{ -1, -1, -1, -1, -1 }},
	{ "DEF_UNICAST_LOC",    PID_DEFAULT_UNICAST_LOCATOR,    PT_Locator,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, proxy.def_ucast),
		  -1, -1, -1 }},
	{ "DEF_MULTICAST_LOC",  PID_DEFAULT_MULTICAST_LOCATOR,  PT_Locator,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, proxy.def_mcast),
		  -1, -1, -1 }},
	{ "META_UNICAST_LOC",   PID_META_UNICAST_LOCATOR,       PT_Locator,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, proxy.meta_ucast),
		  -1, -1, -1 }},
	{ "META_MULTICAST_LOC", PID_META_MULTICAST_LOCATOR,     PT_Locator,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, proxy.meta_mcast),
		  -1, -1, -1 }},
	{ "DEF_UNICAST_IPv4",   PID_DEFAULT_UNICAST_IPADDRESS,  PT_IPv4Address,
		{ -1, -1, -1, -1, -1 }},
	{ "DEF_UNICAST_PORT",   PID_DEFAULT_UNICAST_PORT,       PT_Port,
		{ -1, -1, -1, -1, -1 }},
	{ "META_UNICAST_IPv4",  PID_META_UNICAST_IPADDRESS,     PT_IPv4Address,
		{ -1, -1, -1, -1, -1 }},
	{ "META_UNICAST_PORT",  PID_META_UNICAST_PORT,          PT_Port,
		{ -1, -1, -1, -1, -1 }},
	{ "META_MULTICAST_IPv4",PID_META_MULTICAST_IPADDRESS,   PT_IPv4Address,
		{ -1, -1, -1, -1, -1 }},
	{ "META_MULTICAST_PORT",PID_META_MULTICAST_PORT,        PT_Port,
		{ -1, -1, -1, -1, -1 }},
	{ "EXPECTS_INLINE_QOS", PID_EXPECTS_INLINE_QOS,         PT_Boolean,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.exp_il_qos),
		  offsetof (DiscoveredReaderData, proxy.exp_il_qos),
		  -1, -1 }},
	{ "PART_MAN_LIVELINESS",PID_PARTICIPANT_MAN_LIVELINESS, PT_Count,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, proxy.manual_liveliness),
		  -1, -1, -1 }},
	{ "PART_BUILTIN_EPS",   PID_PARTICIPANT_BUILTIN_EPS,    PT_UnsignedLong,
		{ -1, -1, -1, -1, -1 }},
	{ "PART_LEASE_DURATION",PID_PARTICIPANT_LEASE_DURATION, PT_Duration,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, lease_duration),
		  -1, -1, -1 }},
	{ "CONTENT_FILTER_PROP",PID_CONTENT_FILTER_PROPERTY,    PT_ContentFilterProperty,
		{ -1, -1,
		  offsetof (DiscoveredReaderData, filter),
		  -1, -1 }},
	{ "PART_GUID",          PID_PARTICIPANT_GUID,           PT_GUIDPrefix,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.guid_prefix),
		  -1, -1, -1 }},
	{ "PART_ENTITYID",      PID_PARTICIPANT_ENTITYID,       PT_EntityId,
		{ -1, -1, -1, -1, -1 }},
	{ "GROUP_GUID",         PID_GROUP_GUID,                 PT_GUID,
		{ -1, -1, -1, -1, -1 }},
	{ "GROUP_ENTITYID",     PID_GROUP_ENTITYID,             PT_EntityId,
		{ -1, -1, -1, -1, -1 }},
	{ "BUILTIN_ENDPOINTS",  PID_BUILTIN_ENDPOINT_SET,       PT_BuiltinEndpointSet,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.builtins),
		  -1, -1, -1 }},
	{ "PROPERTY_LIST",      PID_PROPERTY_LIST,              PT_PropertySequence,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, properties),
		  -1, -1, -1 }},
	{ "ENDPOINT_GUID",      PID_ENDPOINT_GUID,              PT_GUID,
		{ -1, -1,
		  offsetof (DiscoveredReaderData, proxy.guid),
		  offsetof (DiscoveredWriterData, proxy.guid),
		  -1 }},
	{ "TYPE_MAX_SERIALIZED",PID_TYPE_MAX_SIZE_SERIALIZED,   PT_Long,
		{ -1, -1, -1, -1, -1 }},
	{ "ENTITY_NAME",        PID_ENTITY_NAME,                PT_EntityName,
		{ -1, 
		  offsetof (SPDPdiscoveredParticipantData, entity_name),
		  -1, -1, -1 }},
	{ "CONTENT_FILTER_INFO",PID_CONTENT_FILTER_INFO,        PT_ContentFilterInfo,
		{ offsetof (InlineQos_t, content_filter_info),
		  -1, -1, -1, -1 }},
	{ "COHERENT_SET",       PID_COHERENT_SET,               PT_SequenceNumber,
		{ offsetof (InlineQos_t, coherent_set),
		  -1, -1, -1, -1 }},
	{ "DIRECTED_WRITE",     PID_DIRECTED_WRITE,             PT_GUIDSequence,
		{ offsetof (InlineQos_t, directed_write),
		  -1, -1, -1, -1 }},
	{ "ORIGINAL_WRITER_INFO",PID_ORIGINAL_WRITER_INFO,      PT_OriginalWriterInfo,
		{ offsetof (InlineQos_t, original_writer_info),
		  -1, -1, -1, -1 }},
	{ "KEY_HASH",           PID_KEY_HASH,                   PT_KeyHash,
		{ offsetof (InlineQos_t, key_hash),
		  -1, -1, -1, -1 }},
	{ "STATUS_INFO",        PID_STATUS_INFO,                PT_StatusInfo,
		{ offsetof (InlineQos_t, status_info),
		  -1, -1, -1, -1 }},
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	{ "ID_TOKEN", 		PID_ID_TOKEN,			PT_Token,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, id_tokens),
		  -1, -1, -1 }},
	{ "PERMISSIONS_TOKEN", 	PID_PERMISSIONS_TOKEN,		PT_Token,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, p_tokens),
		  -1, -1, -1 }},
	{ "DATA_TAGS", 		PID_DATA_TAGS,			PT_PropertySequence,
		{ -1, -1,
		  offsetof (DiscoveredReaderData, tags),
		  offsetof (DiscoveredWriterData, tags),
		  -1 }},
#endif
	{ "SENTINEL",           PID_SENTINEL,                   PT_End,
		{  0,  0,  0,  0,  0 }}
};

#define	N_VENDOR_PIDS	(sizeof (vendor_pids) / sizeof (PIDDesc_t))

#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
#define	PT_TOKEN	PT_VToken
#else
#define	PT_TOKEN	PT_Data
#endif
#endif

static PIDDesc_t vendor_pids [] = {
	{ "VERSION",		PID_V_VERSION,			PT_Long,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.sw_version),
		  -1, -1, -1 }},
	{ "NO_MCAST",           PID_V_NO_MCAST,                 PT_Boolean,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.no_mcast),
		  -1, -1, -1 }}
#ifdef DDS_SECURITY
      ,	{ "SEC_CAPS",		PID_V_SEC_CAPS,			PT_Long,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.sec_caps),
		  -1, -1, -1 }},
	{ "SEC_LOCS",		PID_V_SEC_LOC,			PT_SecLoc,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.sec_locs),
		  -1, -1, -1 }},
	{ "SEC_ID",		PID_V_IDENTITY,			PT_TOKEN,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, id_tokens),
		  -1, -1, -1 }},
	{ "SEC_PERMS",		PID_V_PERMS,			PT_TOKEN,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, p_tokens),
		  -1, -1, -1 }}
#endif
#ifdef DDS_TYPECODE
      ,	{ "TYPECODE",		PID_V_TYPECODE,			PT_Typecode,
		{ -1, -1,
		  offsetof (DiscoveredReaderData, typecode),
		  offsetof (DiscoveredWriterData, typecode),
		  -1 }}
#endif
#ifdef DDS_FORWARD
      , { "FORWARD",		PID_V_FORWARD,			PT_Long,
		{ -1,
		  offsetof (SPDPdiscoveredParticipantData, proxy.forward),
		  -1, -1, -1 }}
#endif
};

/* pid_data_ptr -- Copy and skip a number of data bytes from a message buffer.*/

static void *pid_data_ptr (DBW *walk, size_t n, unsigned char *bp)
{
	void	*ret = (void *) bp;
	size_t	csize;

	do {
		csize = walk->left;
		if (csize > n)
			csize = n;

		if (csize) {
			memcpy (bp, walk->data, csize);
			bp += csize;
			n -= csize;
			walk->data += csize;
			walk->left -= csize;
		}
		if (!walk->left && walk->dbp) {
			walk->dbp = walk->dbp->next;
			if (walk->dbp) {
				walk->data = walk->dbp->data;
				walk->left = walk->dbp->size;
			}
		}
	}
	while (n);
	return (ret);
}

/* pid_data_ptr -- Skip a number of data bytes from a message buffer. */

static void pid_data_skip (DBW *walk, size_t n)
{
	size_t	csize;

	do {
		csize = walk->left;
		if (csize > n)
			csize = n;

		if (csize) {
			n -= csize;
			walk->data += csize;
			walk->left -= csize;
		}
		if (!walk->left && walk->dbp) {
			walk->dbp = walk->dbp->next;
			if (walk->dbp) {
				walk->data = walk->dbp->data;
				walk->left = walk->dbp->size;
			}
		}
	}
	while (n);
}

/* DB_DATA_GET -- Sets (p) to a linear data range {(wp)->data..data+(n)}.
		  Side-effect: (wp) is incremented with (n) bytes.
		  Note that if the data range was scattered over multiple
		  buffers, the data is copied in (buf) which should be large
		  enough to contain the data range.  If the data range was
		  not scattered (p) will point directly to the buffer data and
		  no copy will be done. */

#define	DB_DATA_GET(p,t,wp,n,buf)	\
	if ((n) < (wp)->left) {		\
		(p) = (t) (wp)->data;	\
		(wp)->left -= (n);	\
		(wp)->data += (n);	\
	} else  (p) = (t) pid_data_ptr (wp,n,buf)

/* DB_DATA_SKIP -- Increment (wp) with (n) bytes. */

#define	DB_DATA_SKIP(wp,n)		\
	if ((n) < (wp)->left) {		\
		(wp)->left -= (n);	\
		(wp)->data += (n);	\
	} else pid_data_skip (wp, n)

typedef int (*PP_PARSE_FCT) (ParameterId_t       id,
			     void                *dst,
			     const unsigned char *p,
			     unsigned            maxsize,
			     int                 swap);

/* Parse a type and store it at *dst.  The data is found at *p in a field of at
   the maximum max_size bytes.  The swap parameter indicates that the data to be
   parsed is in other-endian format and data should be swapped where it is
   appropriate.
   If successful, 0 is returned.  If not, a non-0 error code is returned. */

typedef ssize_t (*PP_SIZE_FCT) (const void *src);

/* Return the effective size of the (single) parameter pointed at by *src. */

typedef ssize_t (*PP_WRITE_FCT) (unsigned char *dst, const void *src);

/* Write the parameter at *dst.  The parameter data is at *src. */

/* Various PID parameter flags: */
#define	PPF_SWAP	1	/* Data can be swapped. */
#define	PPF_MULTIPLE	2	/* Parameter may occur more than once. */
#define	PPF_LONG	4	/* Parse function expects an DBW pointer. */

typedef struct pid_par_desc_st {
	short		flags;	/* Various parameter flags. */
	unsigned short	size;	/* Fixed size parameter (if >0 ). */
	PP_PARSE_FCT	parsef;	/* Parse function. */
	PP_SIZE_FCT	wsizef;	/* If not fixed, to assertain write size. */
	PP_WRITE_FCT	writef;	/* Write function. */
} PIDParDesc_t;


/* PID functions on strings:
   ------------------------- */

/* Strings are encoded as:
	ulong length;				{uint32}
	char data [];				{n*char}
   The string data field is aligned on a *4 boundary.
   The actual # of characters in the string, including 1 terminating '\0' is
   given by length.

   Example: "responses" is as follows:
   		0a 00 00 00		Little-endian: 10 chars.
		52 65 73 70		'r' 'e' 's' 'p'
		6f 6e 73 65		'o' 'n' 's' 'e'
		73 00 00 00		's' '\0' <pad*2>
 */

static int pid_cdr_string_parse (String_t	**spp,
				 DBW		*walk,
				 unsigned	*maxsize,
				 int		swap)
{
	uint32_t		len;
	unsigned		n;
	unsigned char		buffer [4];
	const unsigned char	*q;
	size_t			csize;
	size_t			off = 0;

	*spp = NULL;
	if (*maxsize < sizeof (uint32_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	DB_DATA_GET (q, unsigned char *, walk, 4, buffer);
	if (swap)
		memcswap32 (&len, q);
	else
		memcpy32 (&len, q);

	*maxsize -= sizeof (uint32_t);

	n = (len + 3) & ~3;
	if (*maxsize < n)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (len > 1) {
		csize = walk->left >= len ? len : walk->left;
		*spp = str_new ((char *) walk->data, len, csize, 0);

		if (!*spp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		off += csize;
		len -= csize;
		n -= csize;
		*maxsize -= csize;

		while (len > 0 && walk->dbp->next) {
			walk->dbp = walk->dbp->next;
			walk->data = walk->dbp->data;
			walk->left = walk->dbp->size;

			csize = walk->dbp->size >= len ? len : walk->dbp->size;

			if (str_set_chunk (*spp, walk->data, off, csize))
				return (DDS_RETCODE_BAD_PARAMETER);

			off += csize;
			len -= csize;
			n -= csize;
			*maxsize -= csize;
		}

		if (len != 0)
			return (DDS_RETCODE_BAD_PARAMETER);

		walk->left -= csize;
		walk->data += csize;
	}

	DB_DATA_SKIP (walk, n);
	*maxsize -= n;
	return (DDS_RETCODE_OK);
}

static int pid_str_parse (ParameterId_t       id,
		          void                *dst,
			  const unsigned char *p,
			  unsigned            maxsize,
			  int                 swap)
{
	int	ret;
	DBW	walk;

	ARG_NOT_USED (id)

	walk.left = walk.length = maxsize;
	walk.dbp = NULL;
	walk.data = (unsigned char *) p;

	ret = pid_cdr_string_parse (dst, &walk, &maxsize, swap);
	if (ret)
		return (ret);

	if (maxsize)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static ssize_t pid_cdr_string_wsize (const String_t *sp)
{
	unsigned	len;

	if (sp)
		len = sizeof (uint32_t) + str_len (sp);
	else
		len = sizeof (uint32_t) + 1;
	len = (len + 3) & ~3;
	return (len);
}

static ssize_t pid_str_wsize (const void *str)
{
	return (pid_cdr_string_wsize (str));
}

static ssize_t pid_cdr_string_write (unsigned char *dst, const String_t *sp)
{
	unsigned char	*start = dst;
	unsigned	len;

	/* Add string length. */
	if (sp)
		len = str_len (sp);
	else
		len = 1;
	memcpy32 (dst, &len);
	dst += sizeof (uint32_t);

	/* Add string data. */
	if (sp)
		memcpy (dst, str_ptr (sp), len);
	else
		*dst = '\0';
	dst += len;

	/* Pad with '\0' chars. */
	while (((uintptr_t) dst & 0x3) != 0)
		*dst++ = '\0';

	return (dst - start);
}

ssize_t pid_str_write (unsigned char *dst, const void *src)
{
	return (pid_cdr_string_write (dst, src));
}

static PIDParDesc_t pid_string_desc = {
	PPF_SWAP,
	0,
	pid_str_parse,
	pid_str_wsize,
	pid_str_write
};


/* PID functions on two 8-bit unsigned chars:
   ------------------------------------------ */

static int pid_duint8_parse (ParameterId_t       id,
		             void                *dst,
			     const unsigned char *p,
			     unsigned            maxsize,
			     int                 swap)
{
	ARG_NOT_USED (id)
	ARG_NOT_USED (swap)

	if (maxsize != sizeof (uint32_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	memcpy16 (dst, p);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_duint8_write (unsigned char *dst, const void *src)
{
	memcpy16 (dst, src);
	dst [2] = dst [3] = 0;
	return (sizeof (uint32_t));
}


static PIDParDesc_t pid_duint8_desc = {
	0,
	sizeof (uint32_t),
	pid_duint8_parse,
	NULL,
	pid_duint8_write
};


/* PID functions on 32-bit ints and unsigned ints:
   ----------------------------------------------- */

static int pid_uint32_parse (ParameterId_t       id,
		             void                *dst,
			     const unsigned char *p,
			     unsigned            maxsize,
			     int                 swap)
{
	ARG_NOT_USED (id)

	if (maxsize != sizeof (uint32_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap)
		memcswap32 ((unsigned char *) dst, p);
	else
		memcpy32 ((unsigned char *) dst, p);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_uint32_write (unsigned char *dst, const void *src)
{
	memcpy32 (dst, src);
	return (sizeof (uint32_t));
}


static PIDParDesc_t pid_uint32_desc = {
	PPF_SWAP,
	sizeof (uint32_t),
	pid_uint32_parse,
	NULL,
	pid_uint32_write
};


/* PID functions on SequenceNumber_t:
   --------------------------------- */

/* Both types are encoded as:
	ulong high;				{uint32}
	ulong low;				{uint32}
 */

static int pid_duint32_parse (ParameterId_t       id,
		              void                *dst,
			      const unsigned char *p,
			      unsigned            maxsize,
			      int                 swap)
{
	unsigned char	*dp = (unsigned char *) dst;

	ARG_NOT_USED (id)

	if (maxsize != sizeof (uint32_t) * 2)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap) {
		dp = (unsigned char *) dst;
		memcswap32 (dp, p);
		memcswap32 (dp + sizeof (uint32_t), p + sizeof (uint32_t));
	}
	else
		memcpy (dst, p, sizeof (uint32_t) * 2);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_duint32_write (unsigned char *dst, const void *src)
{
	memcpy (dst, src, sizeof (uint32_t) * 2);
	return (sizeof (uint32_t) * 2);
}

static PIDParDesc_t pid_duint32_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 2,
	pid_duint32_parse,
	NULL,
	pid_duint32_write
};


/* PID functions on Duration_t:
   ---------------------------- */

/* Both types are encoded as:
	ulong seconds;				{uint32}
	ulong nanos;				{uint32}
 */

#define get_duration(d,s,ns)	if ((s) == DDS_DURATION_INFINITE_SEC &&	       \
			(ns) == ~0U) (d).nanosec = DDS_DURATION_INFINITE_NSEC; \
			else (d).nanosec = ns; (d).sec = s

#define	set_duration(s,ns,d)	if ((d).sec == DDS_DURATION_INFINITE_SEC &&    \
			(d).nanosec == DDS_DURATION_INFINITE_NSEC) (ns) = ~0U; \
			else (ns) = (d).nanosec; (s) = (d).sec

static int pid_duration_parse (ParameterId_t       id,
		               void                *dst,
			       const unsigned char *p,
			       unsigned            maxsize,
			       int                 swap)
{
	DDS_Duration_t	*dp = (DDS_Duration_t *) dst;
	int32_t		s;
	uint32_t	ns;

	ARG_NOT_USED (id)

	if (maxsize != sizeof (uint32_t) * 2)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap) {
		memcswap32 (&s, p);
		memcswap32 (&ns, p + sizeof (uint32_t));
	}
	else {
		memcpy (&s, p, sizeof (uint32_t));
		memcpy (&ns, p + sizeof (uint32_t), sizeof (uint32_t));
	}
	get_duration (*dp, s, ns);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_duration_write (unsigned char *dst, const void *src)
{
	DDS_Duration_t	*sp = (DDS_Duration_t *) src, *dp = (DDS_Duration_t *) dst;

	set_duration (dp->sec, dp->nanosec, *sp);	
	return (sizeof (uint32_t) * 2);
}

static PIDParDesc_t pid_duration_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 2,
	pid_duration_parse,
	NULL,
	pid_duration_write
};


/* PID functions on Data fields (User/Group/Topic):
   ------------------------------------------------ */

/* Data fields are encoded as:
	ulong length;				{uint32}
	octet data [*];				{n*octet}
 */

static int pid_data_parse (ParameterId_t       id,
		           void                *dst,
			   const unsigned char *p,
			   unsigned            maxsize,
			   int                 swap)
{
	DBW		*walk = (DBW *) p; 
	String_t	**strpp = (String_t **) dst;
	uint32_t	len;
	size_t		csize;
	size_t		off = 0;
	unsigned char 	*q;
	unsigned char 	buffer [4];

	ARG_NOT_USED (id)

	if (maxsize < sizeof (len))
		return (DDS_RETCODE_BAD_PARAMETER);

	DB_DATA_GET (q, unsigned char *, walk, 4, buffer); 

	if (swap)
		memcswap32 (&len, q);
	else
		memcpy32 (&len, q);

	maxsize -= sizeof (uint32_t);

	if (len > maxsize)
		return (DDS_RETCODE_BAD_PARAMETER);

	csize = walk->left >= len ? len : walk->left;

	*strpp = str_new ((char *) walk->data, len, csize, 0);

	if (!*strpp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	off += csize;
	len -= csize;
	maxsize -= csize;

	while (len > 0 && walk->dbp->next) {
		walk->dbp = walk->dbp->next;
		walk->data = walk->dbp->data;
		walk->left = walk->dbp->size;

		csize = (walk->dbp->size >= len) ? len : walk->dbp->size;

		if (str_set_chunk (*strpp, walk->data, off, csize))
			return (DDS_RETCODE_BAD_PARAMETER);

		off += csize;
		len -= csize;
		maxsize -= csize;
	}
	if (len)
		return (DDS_RETCODE_BAD_PARAMETER);

	walk->left -= csize;
	walk->data += csize;

	DB_DATA_SKIP (walk, maxsize);

	return (DDS_RETCODE_OK);
}

static ssize_t pid_data_wsize (const void *str)
{
	const String_t	*sp = (const String_t *) str;
	unsigned	n;

	n = sizeof (uint32_t) * 2 - 1 + str_len (sp);
	n &= ~(sizeof (uint32_t) - 1);
	return (n);
}

static ssize_t pid_data_write (unsigned char *dst, const void *src)
{
	const String_t	*strp = (const String_t *) src;
	uint32_t	len;
	unsigned char	*start = dst;

	/* Add data length. */
	len = str_len (strp);
	memcpy32 (dst, &len);
	dst += sizeof (uint32_t);

	/* Add actual data. */
	memcpy (dst, str_ptr (strp), len);
	dst += len;

	/* Pad with '\0' chars. */
	while (((uintptr_t) dst & 0x3) != 0)
		*dst++ = '\0';

	return (dst - start);
}

static PIDParDesc_t pid_data_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_data_parse,
	pid_data_wsize,
	pid_data_write
};


/* PID functions on GUIDPrefix fields:
   ----------------------------------- */

/* GUIDPrefix is encoded as:
	GUIDPrefix_t prefix;			{12*uint8}
	Zeroes;					{4*uint8}
 */

static int pid_guid_prefix_parse (ParameterId_t       id,
		                  void                *dst,
			          const unsigned char *p,
			          unsigned            maxsize,
			          int                 swap)
{
	GUID_t		*gp = (GUID_t *) p;

	ARG_NOT_USED (id)
	ARG_NOT_USED (swap)

	if (maxsize != sizeof (GUID_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!entity_id_eq (gp->entity_id, entity_id_participant))
		return (DDS_RETCODE_BAD_PARAMETER);

	memcpy (dst, p, sizeof (GuidPrefix_t));
	return (DDS_RETCODE_OK);
}

static ssize_t pid_guid_prefix_write (unsigned char *dst, const void *src)
{
	memcpy (dst, src, sizeof (GuidPrefix_t));
	memcpy (dst + sizeof (GuidPrefix_t),
		&entity_id_participant, sizeof (EntityId_t));
	return (sizeof (GUID_t));
}

static PIDParDesc_t pid_guid_prefix_desc = {
	0,
	sizeof (GUID_t),
	pid_guid_prefix_parse,
	NULL,
	pid_guid_prefix_write
};


/* PID functions on GUID fields:
   ----------------------------- */

/* GUIDs are encoded as:
	GUIDPrefix_t prefix;			{12*uint8}
	EntityId_t entity_id;			{4*uint8}
 */

static int pid_guid_parse (ParameterId_t       id,
		           void                *dst,
			   const unsigned char *p,
			   unsigned            maxsize,
			   int                 swap)
{
	ARG_NOT_USED (id)
	ARG_NOT_USED (swap)

	if (maxsize != sizeof (GUID_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	memcpy (dst, p, sizeof (GUID_t));
	return (DDS_RETCODE_OK);
}

static ssize_t pid_guid_write (unsigned char *dst, const void *src)
{
	memcpy (dst, src, sizeof (GUID_t));
	return (sizeof (GUID_t));
}

static PIDParDesc_t pid_guid_desc = {
	0,
	sizeof (GUID_t),
	pid_guid_parse,
	NULL,
	pid_guid_write
};


/* PID functions on GUID sequence fields:
   --------------------------------------- */

static int pid_guidseq_parse (ParameterId_t       id,
		              void                *dst,
			      const unsigned char *p,
			      unsigned            maxsize,
			      int                 swap)
{
	DDS_GUIDSeq	*gsp = (DDS_GUIDSeq *) dst;
	GUID_t		*gp;
	uint32_t	len;

	ARG_NOT_USED (id)

	if (maxsize < sizeof (uint32_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap)
		memcswap32 (&len, p);
	else
		memcpy32 (&len, p);
	p += sizeof (uint32_t);
	maxsize -= sizeof (uint32_t);

	DDS_SEQ_INIT (*gsp);
	if (maxsize != len * sizeof (GUID_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (maxsize) {
		gsp->_buffer = (GUID_t *) xmalloc (maxsize);
		if (!gsp->_buffer)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		for (gp = gsp->_buffer; maxsize; gp++) {
			memcpy (gp, p, sizeof (GUID_t));
			p += sizeof (GUID_t);
			maxsize -= sizeof (GUID_t);
		}
		gsp->_maximum = gsp->_length = len;
	}
	return (DDS_RETCODE_OK);
}

static ssize_t pid_guidseq_wsize (const void *data)
{
	const DDS_GUIDSeq	*gsp = (const DDS_GUIDSeq *) data;

	return (sizeof (uint32_t) + sizeof (GUID_t) * gsp->_length);
}

static ssize_t pid_guidseq_write (unsigned char *dst, const void *src)
{
	const DDS_GUIDSeq	*gsp = (const DDS_GUIDSeq *) src;
	GUID_t			*gp;
	unsigned char		*start = dst;
	unsigned		i;

	memcpy32 (dst, &gsp->_length);
	dst += sizeof (uint32_t);
	for (i = 0, gp = gsp->_buffer; i < gsp->_length; i++, gp++) {
		memcpy (dst, gp, sizeof (GUID_t));
		dst += sizeof (GUID_t);
	}
	return (dst - start);
}

static PIDParDesc_t pid_guidseq_desc = {
	PPF_SWAP,
	0,
	pid_guidseq_parse,
	pid_guidseq_wsize,
	pid_guidseq_write
};


/* PID functions on Locator fields:
   -------------------------------- */

/* Locators are encoded as:
	LocatorKind kind;			{uint32}
	ulong port;				{uint32}
	unsigned char address [16];		{16*uint8}
 */

#define	PID_LOCATOR_SIZE	(sizeof (uint32_t) * 2 + 16)

static int pid_locator_parse (ParameterId_t       id,
		              void                *dst,
			      const unsigned char *p,
			      unsigned            maxsize,
			      int                 swap)
{
	Locator_t	loc;
	LocatorList_t	*lp = (LocatorList_t *) dst;
	Scope_t		scope;

	ARG_NOT_USED (id)

	if (maxsize != PID_LOCATOR_SIZE)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap) {
		memcswap32 (&loc.kind, p);
		p += sizeof (uint32_t);
		memcswap32 (&loc.port, p);
	}
	else {
		memcpy (&loc.kind, p, sizeof (uint32_t));
		p += sizeof (uint32_t);
		memcpy (&loc.port, p, sizeof (uint32_t));
	}
	p += sizeof (uint32_t);
	memcpy (loc.address, p, sizeof (loc.address));

	if (loc.kind == LOCATOR_KIND_UDPv4)
		scope = sys_ipv4_scope (loc.address + 12);
#ifdef DDS_IPV6
	else if (loc.kind == LOCATOR_KIND_UDPv6)
		scope = sys_ipv6_scope (loc.address);
#endif
	else
		scope = UNKNOWN_SCOPE;
	if (!locator_list_add (lp, loc.kind, loc.address, loc.port, 0, scope, 0, 0))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}

static ssize_t pid_locator_write (unsigned char *dst, const void *src)
{
	const Locator_t	*lp = (const Locator_t *) src;

	memcpy (dst, &lp->kind, sizeof (uint32_t));
	dst += sizeof (uint32_t);
	memcpy (dst, &lp->port, sizeof (uint32_t));
	dst += sizeof (uint32_t);
	memcpy (dst, lp->address, sizeof (lp->address));
	return (sizeof (uint32_t) * 2 + sizeof (lp->address));
}

static PIDParDesc_t pid_locator_desc = {
	PPF_SWAP | PPF_MULTIPLE,
	sizeof (uint32_t) * 2 + 16,
	pid_locator_parse,
	NULL,
	pid_locator_write
};

/* pid_uia_parse -- Parse an array of 32-bit integers. */

static int pid_uia_parse (uint32_t            *dst,
			  const unsigned char *src,
			  unsigned            n,
			  unsigned            maxsize,
			  int                 swap)
{
	if ((n << 2) != maxsize)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap)
		for (; n; dst++, n--) {
			memcswap32 (dst, src);
			src += sizeof (uint32_t);
		}
	else
		memcpy (dst, src, n << 2);
	return (DDS_RETCODE_OK);
}


/* PID functions on Reliability fields:
   ------------------------------------ */

/* Liveliness is encoded as:
	ReliabilityQosPolicyKind kind;		{uint32}
	Duration_t max_blocking_time;		{uint32(sec),uint32(nanos)}
 */

static int pid_reliability_parse (ParameterId_t       id,
		                  void                *dst,
			          const unsigned char *p,
			          unsigned            maxsize,
			          int                 swap)
{
	uint32_t			uia [3];
	DDS_ReliabilityQosPolicy	*rp = (DDS_ReliabilityQosPolicy *) dst;
	int				error;

	ARG_NOT_USED (id)

	error = pid_uia_parse (uia, p, 3, maxsize, swap);
	if (error)
		return (error);

	rp->kind = (DDS_ReliabilityQosPolicyKind) (uia [0] - 1);
	get_duration (rp->max_blocking_time, uia [1], uia [2]);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_reliability_write (unsigned char *dst, const void *src)
{
	uint32_t			*uip = (uint32_t *) dst;
	const DDS_ReliabilityQosPolicy	*rp = (const DDS_ReliabilityQosPolicy *) src;

	*uip++ = ((uint32_t) rp->kind) + 1;
	set_duration (uip [0], uip [1], rp->max_blocking_time);
	return (sizeof (uint32_t) * 3);
}

static PIDParDesc_t pid_reliability_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 3,
	pid_reliability_parse,
	NULL,
	pid_reliability_write
};


/* PID functions on Liveliness fields:
   ----------------------------------- */

/* Liveliness is encoded as:
	LivelinessQosPolicyKind kind;		{uint32}
	Duration_t duration;			{uint32(sec),uint32(nanos)}
 */

static int pid_liveliness_parse (ParameterId_t       id,
		                 void                *dst,
			         const unsigned char *p,
			         unsigned            maxsize,
			         int                 swap)
{
	uint32_t		uia [3];
	DDS_LivelinessQosPolicy	*lp = (DDS_LivelinessQosPolicy *) dst;
	int			error;

	ARG_NOT_USED (id)

	error = pid_uia_parse (uia, p, 3, maxsize, swap);
	if (error)
		return (error);

	lp->kind = (DDS_LivelinessQosPolicyKind) uia [0];
	get_duration (lp->lease_duration, uia [1], uia [2]);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_liveliness_write (unsigned char *dst, const void *src)
{
	uint32_t			*uip = (uint32_t *) dst;
	const DDS_LivelinessQosPolicy	*rp = (const DDS_LivelinessQosPolicy *) src;

	*uip++ = (uint32_t) rp->kind;
	set_duration (uip [0], uip [1], rp->lease_duration);
	return (sizeof (uint32_t) * 3);
}

static PIDParDesc_t pid_liveliness_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 3,
	pid_liveliness_parse,
	NULL,
	pid_liveliness_write
};


/* PID functions on Durability Service fields:
   ------------------------------------------- */

/* Durability service is encoded as:
	Duration_t service_cleanup_delay;	{uint32(sec),uint32(nanos)}
	HistoryQosPolicyKind history_kind;	{int32}
	HistoryDepth history_depth;		{uint32}
	long max_samples;			{int32}
	long max_instances;			{int32}
	long max_samples_per_instance;		{int32}
 */


static int pid_durability_service_parse (ParameterId_t       id,
		                         void                *dst,
					 const unsigned char *p,
					 unsigned            maxsize,
					 int                 swap)
{
	uint32_t			uia [7];
	DDS_DurabilityServiceQosPolicy	*dsp = (DDS_DurabilityServiceQosPolicy *) dst;
	int				error;

	ARG_NOT_USED (id)

	error = pid_uia_parse (uia, p, 7, maxsize, swap);
	if (error)
		return (error);

	get_duration (dsp->service_cleanup_delay, uia [1], uia [2]);
	dsp->history_kind = (DDS_HistoryQosPolicyKind) uia [2];
	dsp->history_depth = uia [3];
	dsp->max_samples = uia [4];
	dsp->max_instances = uia [5];
	dsp->max_samples_per_instance = uia [6];
	return (DDS_RETCODE_OK);
}

static ssize_t pid_durability_service_write (unsigned char *dst, const void *src)
{
	uint32_t				*uip = (uint32_t *) dst;
	const DDS_DurabilityServiceQosPolicy	*rp = (const DDS_DurabilityServiceQosPolicy *) src;

	set_duration (uip [0], uip [1], rp->service_cleanup_delay);
	uip += 2;
	*uip++ = (uint32_t) rp->history_kind;
	*uip++ = rp->history_depth;
	*uip++ = rp->max_samples;
	*uip++ = rp->max_instances;
	*uip   = rp->max_samples_per_instance;
	return (sizeof (uint32_t) * 7);
}

static PIDParDesc_t pid_durability_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 7,
	pid_durability_service_parse,
	NULL,
	pid_durability_service_write
};


/* PID functions on Presentation fields:
   ------------------------------------- */

/* Presentation QoS is encoded as:
	PresentationQosPolicy access_scope;	{uint32}
	Bit coherent_access;			{1b\_1uchar}
	Bit ordered_access;			{1b/       }
 */

static int pid_presentation_parse (ParameterId_t       id,
		                   void                *dst,
				   const unsigned char *p,
				   unsigned            maxsize,
				   int                 swap)
{
	uint32_t			scope;
	DDS_PresentationQosPolicy	*pp = (DDS_PresentationQosPolicy *) dst;

	ARG_NOT_USED (id)

	if (maxsize != sizeof (uint32_t) * 2)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap)
		memcswap32 (&scope, p);
	else
		memcpy32 (&scope, p);
	pp->access_scope = (DDS_PresentationQosPolicyAccessScopeKind) scope;
	pp->coherent_access = p [4] & 1;
	pp->ordered_access = (p [4] & 2) >> 1;
	return (DDS_RETCODE_OK);
}

static ssize_t pid_presentation_write (unsigned char *dst, const void *src)
{
	uint32_t			*ulp = (uint32_t *) dst;
	const DDS_PresentationQosPolicy	*pp = (const DDS_PresentationQosPolicy *) src;

	ulp [0] = (uint32_t) pp->access_scope;
	ulp [1] = 0;
	dst [4] = pp->coherent_access | (pp->ordered_access << 1);
	return (sizeof (uint32_t) * 2);
}

static PIDParDesc_t pid_presentation_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 2,
	pid_presentation_parse,
	NULL,
	pid_presentation_write
};


/* PID functions on Partition fields:
   ---------------------------------- */

/* Partition QoS (if present) is encoded as a sequence of strings:
	ulong n;				{uint32}
	<ulong length;				<{uint32}
	 char data [];>*n			 {length*char}>*n
 */

static int pid_cdr_strings_parse (Strings_t           **spp,
				  DBW		      *walk,
				  unsigned	      *maxsize,
				  int                 swap)
{
	uint32_t		nstrings;
	unsigned		i;
	Strings_t		*sp;
	int			ret;
	unsigned char		buffer [4];
	const unsigned char	*q;

	if (*maxsize < sizeof (uint32_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	DB_DATA_GET (q, unsigned char *, walk, 4, buffer);

	if (swap)
		memcswap32 (&nstrings, q);
	else
		memcpy32 (&nstrings, q);

	*maxsize -= 4;

	if (!nstrings) {
		*spp = NULL;
		return (DDS_RETCODE_OK);
	}

	*spp = sp = xmalloc (sizeof (Strings_t) +
				sizeof (String_t *) * nstrings);
	if (!sp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	sp->_length = 0;
	sp->_maximum = nstrings;
	sp->_esize = sizeof (String_t *);
	sp->_buffer = (String_t **) (sp + 1);

	for (i = 0; i < nstrings; i++) {
		ret = pid_cdr_string_parse (&sp->_buffer [i], walk, maxsize, swap);
		if (ret)
			goto free_strings;

		sp->_length++;
	}
	return (DDS_RETCODE_OK);

    free_strings:
	for (i = 0; i < sp->_length; i++)
		str_unref (sp->_buffer [i]);
	xfree (sp);
	*spp = NULL;
	return (ret);
}

static int pid_partition_parse (ParameterId_t       id,
		                void                *dst,
			        const unsigned char *p,
			        unsigned            maxsize,
			        int                 swap)
{
	int	ret;
	DBW     *walk = (DBW *) p;

	ARG_NOT_USED (id)

	ret = pid_cdr_strings_parse (dst, walk, &maxsize, swap);
	if (ret)
		return (ret);

	if (maxsize)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static ssize_t pid_cdr_strings_wsize (const Strings_t *sp)
{
	unsigned	i, size;

	if (!sp)
		return (sizeof (uint32_t));

	size = sizeof (uint32_t);
	for (i = 0; i < sp->_length; i++)
		size += pid_cdr_string_wsize (sp->_buffer [i]);
	return (size);
}

static ssize_t pid_partition_wsize (const void *data)
{
	return (pid_cdr_strings_wsize (data));
}

static ssize_t pid_cdr_strings_write (unsigned char *dst, const Strings_t *sp)
{
	uint32_t	*lp = (uint32_t *) dst;
	unsigned	i;
	ssize_t		n;
	unsigned char	*start = dst;

	if (!sp) {
		*lp = 0;
		return (sizeof (uint32_t));
	}
	*lp = sp->_length;
	dst += sizeof (uint32_t);
	for (i = 0; i < sp->_length; i++) {
		n = pid_cdr_string_write (dst, sp->_buffer [i]);
		dst += n;
	}
	return (dst - start);
}

static ssize_t pid_partition_write (unsigned char *dst, const void *src)
{
	return (pid_cdr_strings_write (dst, src));
}

static PIDParDesc_t pid_partition_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_partition_parse,
	pid_partition_wsize,
	pid_partition_write
};


/* PID functions on History fields:
   -------------------------------- */

/* History QoS is encoded as:
	HistoryQosKind kind;			{uint32}
	int depth;				{int32}
 */

static int pid_history_parse (ParameterId_t       id,
		              void                *dst,
			      const unsigned char *p,
			      unsigned            maxsize,
			      int                 swap)
{
	uint32_t		u = 0;
	DDS_HistoryQosPolicy	*hp = (DDS_HistoryQosPolicy *) dst;

	ARG_NOT_USED (id)

	if (maxsize != sizeof (uint32_t) * 2)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap)
		memcswap32 (&u, p);
	else
		memcpy32 (&u, p);
	p += sizeof (uint32_t);
	hp->kind = (DDS_HistoryQosPolicyKind) u;
	if (swap)
		memcswap32 (&u, p);
	else
		memcpy32 (&u, p);
	hp->depth = u;
	return (DDS_RETCODE_OK);
}

static ssize_t pid_history_write (unsigned char *dst, const void *src)
{
	uint32_t			*lp = (uint32_t *) dst;
	const DDS_HistoryQosPolicy	*hp = (const DDS_HistoryQosPolicy *) src;

	*lp++ = hp->kind;
	*lp   = hp->depth;
	return (sizeof (uint32_t) * 2);
}

static PIDParDesc_t pid_history_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 2,
	pid_history_parse,
	NULL,
	pid_history_write
};


/* PID functions on ContentFilterInfo fields:
   ------------------------------------------ */

/* ContentFilterInfo QoS is encoded as:
	sequence<long32> result;		{uint32+n*uint32}
	sequence<long32[4]> signatures;		{uint32+m*uint32*4}
 */

static int pid_content_f_info_parse (ParameterId_t       id,
		                     void                *dst,
				     const unsigned char *p,
				     unsigned            maxsize,
				     int                 swap)
{
	uint32_t		nres, nsig, *wp, *uip;
	unsigned		i;
	ContentFilterInfo_t	**fipp = (ContentFilterInfo_t **) dst, *fip;

	ARG_NOT_USED (id)

	if (maxsize < sizeof (uint32_t) * 2)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap)
		memcswap32 (&nres, p);
	else
		memcpy32 (&nres, p);
	maxsize -= sizeof (uint32_t);
	p += sizeof (uint32_t);
	if (maxsize < ((nres + 1) << 2))
		return (DDS_RETCODE_BAD_PARAMETER);

	wp = (uint32_t *) p;
	wp += nres;
	if (swap)
		memcswap32 (&nsig, wp);
	else
		memcpy32 (&nsig, wp);
	maxsize -= sizeof (uint32_t);
	if (!nres && !nsig) {
		*fipp = NULL;
		return (DDS_RETCODE_OK);
	}
	*fipp = fip = (ContentFilterInfo_t *) xmalloc (sizeof (ContentFilterInfo_t) +
						       (nres << 2) + (nsig << 4));
	if (!fip)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (maxsize < (nres << 2) + (nsig << 4))
		return (DDS_RETCODE_BAD_PARAMETER);

	fip->num_bitmaps = nres;
	wp = (uint32_t *) p;
	uip = (uint32_t *) (fip + 1);
	fip->bitmaps = uip;
	for (i = 0; i < nres; i++, uip++, wp++)
		if (swap)
			memcswap32 (uip, wp);
		else
			memcpy32 (uip, wp);
	wp++;
	fip->num_signatures = nsig;
	fip->signatures = uip;
	for (i = 0; i < nsig << 2; i++, uip++, wp++)
		if (swap)
			memcswap32 (uip, wp);
		else
			memcpy32 (uip, wp);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_content_f_info_wsize (const void *data)
{
	const ContentFilterInfo_t *fip = (const ContentFilterInfo_t *) data;
	unsigned		  n = sizeof (uint32_t) * 2;

	if (fip) {
		n += fip->num_bitmaps << 2;
		n += fip->num_signatures << 4;
	}
	return (n);
}

static ssize_t pid_content_f_info_write (unsigned char *dst, const void *src)
{
	const ContentFilterInfo_t *fip = (const ContentFilterInfo_t *) src;
	uint32_t		  *wp = (uint32_t *) dst;

	if (!fip) {
		wp [0] = wp [1] = 0;
		return (DDS_RETCODE_OK);
	}
	*wp++ = fip->num_bitmaps;
	if (fip->num_bitmaps) {
		memcpy (wp, fip->bitmaps, fip->num_bitmaps << 2);
		wp += fip->num_bitmaps;
	}
	*wp++ = fip->num_signatures;
	if (fip->num_signatures)
		memcpy (wp, fip->signatures, fip->num_signatures << 4);

	return (DDS_RETCODE_OK);
}

static PIDParDesc_t pid_contentfinfo_desc = {
	PPF_SWAP,
	0,
	pid_content_f_info_parse,
	pid_content_f_info_wsize,
	pid_content_f_info_write
};


/* PID functions on ContentFilterProperty fields:
   ---------------------------------------------- */

static int pid_content_f_property_parse (ParameterId_t       id,
					 void                *dst,
					 const unsigned char *p,
					 unsigned            maxsize,
					 int                 swap)
{
	ContentFilter_t	*fp, **fpp = (ContentFilter_t **) dst;
	int		ret;
	DBW		*walk = (DBW *) p;

	ARG_NOT_USED (id)

	/* Allocate enough space to contain the extra program/cache data
	   parameters that will be filled in later. */
	*fpp = fp = xmalloc (sizeof (FilterData_t));
	if (!fp || maxsize < sizeof (uint32_t))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	ret = pid_cdr_string_parse (&fp->name, walk, &maxsize, swap);
	if (ret)
		goto error;

	ret = pid_cdr_string_parse (&fp->related_name, walk, &maxsize, swap);
	if (ret)
		goto error;

	ret = pid_cdr_string_parse (&fp->class_name, walk, &maxsize, swap);
	if (ret)
		goto error;

	ret = pid_cdr_string_parse (&fp->expression, walk, &maxsize, swap);
	if (ret)
		goto error;

	ret = pid_cdr_strings_parse (&fp->expression_pars, walk, &maxsize, swap);
	if (ret)
		goto error;

	memset ((char *)(fp + 1), 0,
		sizeof (FilterData_t) - sizeof (ContentFilter_t));
	if (!maxsize)
		return (DDS_RETCODE_OK);

    error:
	return (ret);
}

static ssize_t pid_content_f_property_wsize (const void *data)
{
	ContentFilter_t	*fp = (ContentFilter_t *) data;
	ssize_t		len;

	len = pid_cdr_string_wsize (fp->name);
	len += pid_cdr_string_wsize (fp->related_name);
	len += pid_cdr_string_wsize (fp->class_name);
	len += pid_cdr_string_wsize (fp->expression);
	len += pid_cdr_strings_wsize (fp->expression_pars);
	return (len);
}

static ssize_t pid_content_f_property_write (unsigned char *dst, const void *src)
{
	ContentFilter_t	*fp = (ContentFilter_t *) src;
	ssize_t		len;
	unsigned char	*start = dst;

	len = pid_cdr_string_write (dst, fp->name);
	dst += len;
	len = pid_cdr_string_write (dst, fp->related_name);
	dst += len;
	len = pid_cdr_string_write (dst, fp->class_name);
	dst += len;
	len = pid_cdr_string_write (dst, fp->expression);
	dst += len;
	len = pid_cdr_strings_write (dst, fp->expression_pars);
	dst += len;

	return (dst - start);
}

static PIDParDesc_t pid_contentfprop_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_content_f_property_parse,
	pid_content_f_property_wsize,
	pid_content_f_property_write
};


/* PID functions on Resource Limits fields:
   ---------------------------------------- */

/* Resource Limits QoS is encoded as:
	int max_samples;			{int32}
	int max_instances;			{int32}
	int max_samples_per_instance;		{int32}
 */

static int pid_res_limits_parse (ParameterId_t       id,
				 void                *dst,
				 const unsigned char *p,
				 unsigned            maxsize,
				 int                 swap)
{
	uint32_t			uia [3];
	DDS_ResourceLimitsQosPolicy	*lp = (DDS_ResourceLimitsQosPolicy *) dst;
	int				error;

	ARG_NOT_USED (id)

	error = pid_uia_parse (uia, p, 3, maxsize, swap);
	if (error)
		return (error);

	lp->max_samples = uia [0];
	lp->max_instances = uia [1];
	lp->max_samples_per_instance = uia [2];
	return (DDS_RETCODE_OK);
}

static ssize_t pid_res_limits_write (unsigned char *dst, const void *src)
{
	uint32_t				*uip = (uint32_t *) dst;
	const DDS_ResourceLimitsQosPolicy	*lp = (const DDS_ResourceLimitsQosPolicy *) src;

	*uip++ = lp->max_samples;
	*uip++ = lp->max_instances;
	*uip   = lp->max_samples_per_instance;
	return (sizeof (uint32_t) * 3);
}

static PIDParDesc_t pid_res_limits_desc = {
	PPF_SWAP,
	sizeof (uint32_t) * 3,
	pid_res_limits_parse,
	NULL,
	pid_res_limits_write
};


/* PID functions on Property list fields:
   -------------------------------------- */

/* The Property list is encoded as:
	sequence <
	 struct {
	   string name;
	   string value;
	 } Property_t
	> */

static void free_properties (Property_t *list)
{
	Property_t	*pp;

	while (list) {
		pp = list;
		list = list->next;
		if (pp->name)
			str_unref (pp->name);
		if (pp->value)
			str_unref (pp->value);
		xfree (pp);
	}
}

static int pid_propertyseq_parse (ParameterId_t       id,
		                  void                *dst,
			          const unsigned char *p,
			          unsigned            maxsize,
			          int                 swap)
{
	Property_t		*head, *tail, *pp, **ppp = (Property_t **) dst;
	uint32_t		n, i;
	unsigned char		buffer [4];
	const unsigned char	*q;
	DBW			*walk = (DBW *) p;
	int			ret;

	ARG_NOT_USED (id)

	if (maxsize < sizeof (uint32_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	DB_DATA_GET (q, unsigned char *, walk, 4, buffer);
	if (swap)
		memcswap32 (&n, q);
	else
		memcpy32 (&n, q);
	maxsize -= sizeof (uint32_t);
	head = tail = NULL;
	for (i = 0; i < n; i++) {
		pp = xmalloc (sizeof (Property_t));
		if (!pp)
			goto failed;

		pp->name = pp->value = NULL;
		pp->next = NULL;
		ret = pid_cdr_string_parse (&pp->name, walk, &maxsize, swap);
		if (ret) {
			xfree (pp);
			goto failed;
		}
		ret = pid_cdr_string_parse (&pp->value, walk, &maxsize, swap);
		if (ret) {
			if (pp->name)
				str_unref (pp->name);
			xfree (pp);
			goto failed;
		}
		if (head)
			tail->next = pp;
		else
			head = pp;
		tail = pp;
	}
	return (DDS_RETCODE_OK);

    failed:
	free_properties (head);
	*ppp = NULL;
	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

static ssize_t pid_propertyseq_wsize (const void *data)
{
	Property_t	*pp;
	ssize_t		len = sizeof (uint32_t);

	for (pp = (Property_t *) data; pp; pp = pp->next) {
		len += pid_cdr_string_wsize (pp->name);
		len += pid_cdr_string_wsize (pp->value);
	}
	return (len);
}

static ssize_t pid_propertyseq_write (unsigned char *dst, const void *src)
{
	Property_t	*pp = (Property_t *) src;
	ssize_t		len;
	unsigned char	*start = dst;
	uint32_t	*lenp = (uint32_t *) dst;

	len = sizeof (uint32_t);
	while (pp) {
		len = pid_cdr_string_write (dst, pp->name);
		dst += len;
		len = pid_cdr_string_write (dst, pp->value);
		dst += len;
		(*lenp)++;
	}
	return (dst - start);
}

static PIDParDesc_t pid_propertyseq_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_propertyseq_parse,
	pid_propertyseq_wsize,
	pid_propertyseq_write
};


/* PID functions on Original Writer Info fields:
   --------------------------------------------- */

/* The Original Writer Info data is encoded as:
	struct {
	  GUID_t           original_writer_GUID;
	  SequenceNumber_t original_writer_seqnr;
	  ParameterList    original_writer_QoS;
	}
 */

static int pid_orig_writer_parse (ParameterId_t       id,
				  void                *dst,
				  const unsigned char *p,
				  unsigned            maxsize,
				  int                 swap)
{
	ARG_NOT_USED (id)
	ARG_NOT_USED (dst)
	ARG_NOT_USED (p)
	ARG_NOT_USED (maxsize)
	ARG_NOT_USED (swap)

	/* ... TBC ... */

	return (DDS_RETCODE_UNSUPPORTED);
}

static ssize_t pid_orig_writer_wsize (const void *data)
{
	ARG_NOT_USED (data)

	/* ... TBC ... */

	return (-DDS_RETCODE_UNSUPPORTED);
}

static ssize_t pid_orig_writer_write (unsigned char *dst, const void *src)
{
	ARG_NOT_USED (dst)
	ARG_NOT_USED (src)

	/* ... TBC ... */

	return (-DDS_RETCODE_UNSUPPORTED);
}

static PIDParDesc_t pid_origwriter_desc = {
	PPF_SWAP,
	0,
	pid_orig_writer_parse,
	pid_orig_writer_wsize,
	pid_orig_writer_write
};


/* PID functions on HashKey field:
   ------------------------------- */

static int pid_hash_key_parse (ParameterId_t       id,
		               void                *dst,
			       const unsigned char *p,
			       unsigned            maxsize,
			       int                 swap)
{
	ARG_NOT_USED (id)
	ARG_NOT_USED (swap)

	if (maxsize != sizeof (KeyHash_t))
		return (DDS_RETCODE_BAD_PARAMETER);

	memcpy (dst, p, sizeof (KeyHash_t));
	return (DDS_RETCODE_OK);
}

static ssize_t pid_hash_key_write (unsigned char *dst, const void *src)
{
	memcpy (dst, src, sizeof (KeyHash_t));
	return (sizeof (KeyHash_t));
}

static PIDParDesc_t pid_hash_desc = {
	0,
	sizeof (KeyHash_t),
	pid_hash_key_parse,
	NULL,
	pid_hash_key_write
};

/* PID functions on StatusInfo_t:
   ------------------------------ */

static int pid_sinfo_parse (ParameterId_t       id,
			    void                *dst,
			    const unsigned char *p,
			    unsigned            maxsize,
			    int                 swap)
{
	ARG_NOT_USED (id)
	ARG_NOT_USED (maxsize)
	ARG_NOT_USED (swap)

	memcpy32 ((unsigned char *) dst, p);
	return (DDS_RETCODE_OK);
}

static ssize_t pid_sinfo_write (unsigned char *dst, const void *src)
{
	memcpy32 (dst, src);

	return (4);
}


static PIDParDesc_t pid_sinfo_desc = {
	PPF_SWAP,
	4,
	pid_sinfo_parse,
	NULL,
	pid_sinfo_write
};

#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY

/* PID functions on Security Tokens with CDR-ed DataHolder encoding.
   ----------------------------------------------------------------- */

static int pid_token_parse (ParameterId_t       id,
			    void                *dst,
			    const unsigned char *p,
			    unsigned            maxsize,
			    int                 swap)
{
	DDS_DataHolder	 	*hp;
	Token_t		 	*token, **prev_tokenp = (Token_t **) dst;
	Type		 	*tp = DataHolder_ts->ts_pl->xtype;
	DBW		 	*walk = (DBW *) p;
	unsigned char	 	*bp;
	const unsigned char	*dp;
	size_t		 	s;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (id);
	ARG_NOT_USED (maxsize);

	if (walk->left >= maxsize) {
		dp = walk->data;
		bp = NULL;
	}
	else {
		bp = xmalloc (maxsize);
		if (!bp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		DB_DATA_GET (dp, unsigned char *, walk, maxsize, bp);
	}
	s = cdr_unmarshalled_size (dp, 0, tp, 0, 0, swap, sizeof (DDS_DataHolder), &error);
	if (!s) {
		if (bp)
			xfree (bp);
		return (error);
	}
	token = xmalloc (sizeof (Token_t));
	if (!token)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	hp = xmalloc (s);
	if (!hp) {
		xfree (token);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (hp, 0, s);
	error = cdr_unmarshall (hp, dp, 0, tp, 0, 0, swap, sizeof (DDS_DataHolder));
	if (error) {
		if (bp)
			xfree (bp);
		xfree (hp);
		xfree (token);
		return (error);
	}
	token->data = hp;
	token->encoding = 0;
	token->integral = 1;
	token->nusers = 1;
	token->next = *prev_tokenp;
	*prev_tokenp = token;

	DB_DATA_SKIP (walk, maxsize);

	return (DDS_RETCODE_OK);
}

static ssize_t pid_token_wsize (const void *src)
{
	Token_t			*token = (Token_t *) src;
	Type			*tp = DataHolder_ts->ts_pl->xtype;
	size_t			s;
	DDS_ReturnCode_t	error;

	s = cdr_marshalled_size (0, token->data, tp, 0, 0, 0, &error);
	return (s);
}

static ssize_t pid_token_write (unsigned char *dst, const void *src)
{
	Token_t			*token = (Token_t *) src;
	Type			*tp = DataHolder_ts->ts_pl->xtype;
	size_t			s;
	DDS_ReturnCode_t	error;

	s = cdr_marshall (dst, 0, token->data, tp, 0, 0, 0, 0, &error);
	if (!s)
		return (-error);
	else
		return (s);
}

static PIDParDesc_t pid_token_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_token_parse,
	pid_token_wsize,
	pid_token_write
};


/* PID functions on Security Tokens with vendor data encoding.
   ----------------------------------------------------------- */

static int pid_vtoken_parse (ParameterId_t       id,
			     void                *dst,
			     const unsigned char *p,
			     unsigned            maxsize,
			     int                 swap)
{
	DDS_DataHolder	*hp;
	Token_t		*token, **prev_tokenp = (Token_t **) dst;
	unsigned char 	*q, *dp;
	uint32_t	len;
	size_t		csize, s;
	DDS_OctetSeq	*sp;
	DBW		*walk = (DBW *) p;
	unsigned char 	buffer [4];

	ARG_NOT_USED (id)

	if (maxsize < sizeof (len))
		return (DDS_RETCODE_BAD_PARAMETER);

	DB_DATA_GET (q, unsigned char *, walk, 4, buffer); 

	if (swap)
		memcswap32 (&len, q);
	else
		memcpy32 (&len, q);

	maxsize -= sizeof (uint32_t);

	if (!len || len > maxsize)
		return (DDS_RETCODE_BAD_PARAMETER);

	token = xmalloc (sizeof (Token_t));
	if (!token)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	s = sizeof (DDS_DataHolder) + sizeof (DDS_OctetSeq) + len;
	hp = xmalloc (s);
	if (!hp) {
		xfree (token);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (hp, 0, sizeof (DDS_DataHolder));
	hp->class_id = (id == PID_V_IDENTITY) ?
				GMCLASSID_SECURITY_DTLS_ID_TOKEN :
				GMCLASSID_SECURITY_DTLS_PERM_TOKEN;

	hp->binary_value1 = sp = (DDS_OctetSeq *) (hp + 1);
	DDS_SEQ_INIT (*sp);
	DDS_SEQ_DATA (*sp) = dp = (unsigned char *) (sp + 1);
	DDS_SEQ_LENGTH (*sp) = DDS_SEQ_MAXIMUM (*sp) = len;

	csize = walk->left >= len ? len : walk->left;
	while (len) {
		memcpy (dp, walk->data, csize);
		len -= csize;
		walk->left -= csize;
		walk->data += csize;
		maxsize -= csize;

		if (!len || !walk->dbp->next)
			break;

		dp += csize;

		walk->dbp = walk->dbp->next;
		walk->data = walk->dbp->data;
		walk->left = walk->dbp->size;

		csize = (walk->dbp->size >= len) ? len : walk->dbp->size;
	}
	if (len) {
		xfree (token);
		xfree (hp);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DB_DATA_SKIP (walk, maxsize);

	token->data = hp;
	token->encoding = id;
	token->integral = 1;
	token->nusers = 1;
	token->next = *prev_tokenp;
	*prev_tokenp = token;
	return (DDS_RETCODE_OK);
}

static ssize_t pid_vtoken_wsize (const void *src)
{
	const Token_t		*token = (const Token_t *) src;
	const DDS_DataHolder	*hp = token->data;
	unsigned		n;

	n = sizeof (uint32_t) * 2 - 1 + DDS_SEQ_LENGTH (*hp->binary_value1);
	n &= ~(sizeof (uint32_t) - 1);
	return (n);
}

static ssize_t pid_vtoken_write (unsigned char *dst, const void *src)
{
	const Token_t		*token = (const Token_t *) src;
	const DDS_DataHolder	*hp = token->data;
	uint32_t		len;
	unsigned char		*start = dst;

	/* Add data length. */
	len = DDS_SEQ_LENGTH (*hp->binary_value1);
	memcpy32 (dst, &len);
	dst += sizeof (uint32_t);

	/* Add actual data. */
	memcpy (dst, DDS_SEQ_DATA (*hp->binary_value1), len);
	dst += len;

	/* Pad with '\0' chars. */
	while (((uintptr_t) dst & 0x3) != 0)
		*dst++ = '\0';

	return (dst - start);
}

static PIDParDesc_t pid_vtoken_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_vtoken_parse,
	pid_vtoken_wsize,
	pid_vtoken_write
};

#endif /* DDS_NATIVE_SECURITY */


/* PID functions on Security Locator fields:
   ----------------------------------------- */

/* Security locators are encoded as:
	SecurityProtocol proto;			{uint32}
	LocatorKind kind;			{uint32}
	ulong port;				{uint32}
	unsigned char address [16];		{16*uint8}
 */

#define	PID_SECLOC_SIZE	(sizeof (uint32_t) * 3 + 16)

static int pid_secloc_parse (ParameterId_t       id,
		             void                *dst,
			     const unsigned char *p,
			     unsigned            maxsize,
			     int                 swap)
{
	Locator_t	loc;
	LocatorList_t	*lp = (LocatorList_t *) dst;
	LocatorNode_t	*np;
	Scope_t		scope;
	unsigned	flags;
	uint32_t	proto;

	ARG_NOT_USED (id)

	if (maxsize != PID_SECLOC_SIZE)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap) {
		memcswap32 (&proto, p);
		p += sizeof (uint32_t);
		memcswap32 (&loc.kind, p);
		p += sizeof (uint32_t);
		memcswap32 (&loc.port, p);
	}
	else {
		memcpy (&proto, p, sizeof (uint32_t));
		p += sizeof (uint32_t);
		memcpy (&loc.kind, p, sizeof (uint32_t));
		p += sizeof (uint32_t);
		memcpy (&loc.port, p, sizeof (uint32_t));
	}
	p += sizeof (uint32_t);
	memcpy (loc.address, p, sizeof (loc.address));

	if (loc.kind == LOCATOR_KIND_UDPv4)
		scope = sys_ipv4_scope (loc.address + 12);
#ifdef DDS_IPV6
	else if (loc.kind == LOCATOR_KIND_UDPv6)
		scope = sys_ipv6_scope (loc.address);
#endif
	else
		scope = UNKNOWN_SCOPE;
	flags = LOCF_DATA | LOCF_META | LOCF_UCAST | LOCF_SECURE | LOCF_SERVER;
	np = locator_list_add (lp, loc.kind, loc.address, loc.port, 0, scope, flags, proto);
	return ((np) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

static ssize_t pid_secloc_write (unsigned char *dst, const void *src)
{
	const Locator_t	*lp = (const Locator_t *) src;
	uint32_t	proto;

	proto = lp->sproto;
	memcpy (dst, &proto, sizeof (uint32_t));
	dst += sizeof (uint32_t);
	memcpy (dst, &lp->kind, sizeof (uint32_t));
	dst += sizeof (uint32_t);
	memcpy (dst, &lp->port, sizeof (uint32_t));
	dst += sizeof (uint32_t);
	memcpy (dst, lp->address, sizeof (lp->address));
	return (sizeof (uint32_t) * 3 + sizeof (lp->address));
}

static PIDParDesc_t pid_secloc_desc = {
	PPF_SWAP | PPF_MULTIPLE,
	sizeof (uint32_t) * 3 + 16,
	pid_secloc_parse,
	NULL,
	pid_secloc_write
};

#endif /* DDS_SECURITY */

#ifdef DDS_TYPECODE

/* PID functions on Typecode.
   -------------------------- */

/* Typecode fields are encoded as:
	enum typecode;				specific type
	uint16 length;				length of type info.
	<info>					depends on specific type.

   This can be parsed recursively in the case of structs, unions and arrays/
   sequences.
 */

/* pid_typecode_parse -- Copy valid native typecode data in *dst. */

static int pid_typecode_parse (ParameterId_t       id,
		               void                *dst,
			       const unsigned char *p,
			       unsigned            maxsize,
			       int                 swap)
{
	DBW			*walk = (DBW *) p; 
	unsigned char		*bp, **dstp = (unsigned char **) dst;
	const unsigned char	*tcp;
	VTC_Header_t		*hp;
	unsigned		ofs;
	uint16_t		ext;

	ARG_NOT_USED (id)

	if (maxsize < MIN_STRUCT_SIZE)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (walk->left >= maxsize) {
		tcp = walk->data;
		bp = NULL;
	}
	else {
		bp = xmalloc (maxsize);
		if (!bp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		DB_DATA_GET (tcp, unsigned char *, walk, maxsize, bp);
	}

	/* Validate and convert to non-swapped representation, while doing so.*/
	ofs = 0;
	if (swap)
		memcswap16 (&ext, tcp + 6);
	else
		memcpy16 (&ext, tcp + 6);
	if (!vtc_validate (tcp, maxsize, &ofs, swap, ext >> NRE_EXT_S))
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Valid typecode, which is in native representation now.
	   Save the contents in a new buffer if not yet done. */
	if (!bp) {
		bp = xmalloc (maxsize);
		if (!bp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		memcpy (bp, tcp, maxsize);
		DB_DATA_SKIP (walk, maxsize);
	}
	hp = (VTC_Header_t *) bp;
	hp->nrefs_ext = (hp->nrefs_ext & ~NRE_NREFS) | 1;
	*dstp = bp;
	return (DDS_RETCODE_OK);
}

static ssize_t pid_typecode_wsize (const void *data)
{
	const unsigned char	*tcp = (const unsigned char *) data;
	uint16_t		len;

	memcpy (&len, tcp + 4, 2);
	len += 6;
	return ((len + 3) & ~3);
}

static ssize_t pid_typecode_write (unsigned char *dst, const void *src)
{
	const unsigned char	*tcp = (const unsigned char *) src;
	unsigned char		*start = dst;
	uint16_t		len, ext;

	memcpy (&len, tcp + 4, 2);
	len += 6;

	/* Add actual data. */
	memcpy (dst, tcp, len);
	memcpy (&ext, tcp + 6, 2);
	ext &= ~NRE_NREFS;
	memcpy (dst + 6, &ext, 2);
	dst += len;
	while (((uintptr_t) dst & 0x3) != 0)
		*dst++ = '\0';

	return (dst - start);
}

static PIDParDesc_t pid_typecode_desc = {
	PPF_SWAP | PPF_LONG,
	0,
	pid_typecode_parse,
	pid_typecode_wsize,
	pid_typecode_write
};

#endif

static const PIDParDesc_t *pid_type_desc [] = {
	NULL,			/* End. */
	NULL,			/* Pad. */
	&pid_string_desc, 	/* String. */
	&pid_uint32_desc,	/* Boolean. */
	&pid_uint32_desc,	/* Long. */
	&pid_uint32_desc,	/* UnsignedLong. */
	&pid_duint32_desc,	/* SequenceNumber. */
	&pid_uint32_desc,	/* Count. */
	&pid_uint32_desc,	/* Port. */
	&pid_uint32_desc,	/* EntityId - no swap! */
	&pid_data_desc,		/* Data. */
	&pid_duint32_desc,	/* Duration. */
	&pid_guid_desc,		/* GUID. */
	&pid_guid_prefix_desc,	/* GUIDPrefix. */
	&pid_guidseq_desc,	/* GUIDSequence. */
	&pid_locator_desc,	/* Locator. */
	&pid_uint32_desc,	/* IPv4Address. */
	&pid_duint8_desc,	/* ProtocolVersion. */
	&pid_duint8_desc,	/* VendorId. */
	&pid_reliability_desc,	/* Reliability. */
	&pid_liveliness_desc,	/* Liveliness. */
	&pid_uint32_desc,	/* Durability. */
	&pid_durability_desc,	/* DurabilityService. */
	&pid_uint32_desc,	/* Ownership. */
	&pid_uint32_desc,	/* OwnershipStrength. */
	&pid_presentation_desc,	/* Presentation. */
	&pid_duration_desc,	/* Deadline. */
	&pid_uint32_desc,	/* DestinationOrder. */
	&pid_duration_desc,	/* LatencyBudget. */
	&pid_partition_desc,	/* Partition. */
	&pid_duration_desc,	/* Lifespan. */
	&pid_history_desc,	/* History. */
	&pid_duration_desc,	/* TimeBasedFilter. */
	&pid_contentfinfo_desc,	/* ContentFilterInfo. */
	&pid_contentfprop_desc,	/* ContentFilterProperty. */
	&pid_res_limits_desc,	/* ResourceLimits. */
	&pid_uint32_desc,	/* TransportPriority. */
	&pid_uint32_desc,	/* BuiltinEndpointSet. */
	&pid_propertyseq_desc,	/* PropertySequence. */
	&pid_origwriter_desc,	/* OriginalWriterInfo. */
	&pid_string_desc,	/* EntityName. */
	&pid_hash_desc,		/* KeyHash. */
	&pid_sinfo_desc		/* StatusInfo. */
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
      ,	&pid_token_desc,	/* DataHolder (CDR). */
	&pid_vtoken_desc	/* DataHolder (Data). */
#endif
      ,	&pid_secloc_desc	/* Security Locator. */
#endif
#ifdef DDS_TYPECODE
      , &pid_typecode_desc	/* Typecode. */
#endif
};

static PIDDesc_t *std_pids [120];	/* Standard PIDs. */
static PIDDesc_t *non_std_pids [8];	/* Vendor-specific PIDs. */
static PIDDesc_t **pid_descs [2] = { std_pids, non_std_pids };
static PIDSet_t pid_sets [PID_NSRC][2];	/* PID sets. */
static unsigned pid_max [2];
static int log_pids = 0;		/* Set this to log PIDs. */

#ifdef LOCK_PARSE

static lock_t parse_lock;

#define parse_entry()	lock_take (parse_lock)
#define parse_exit()	lock_release (parse_lock)

#else

#define parse_entry()
#define parse_exit()

#endif

static void pid_init_entry (PIDDesc_t *pp)
{
	int		vendor;
	ParameterId_t	value;
	PIDSource_t	t;

	vendor = (pp->pid & PID_VENDOR_SPECIFIC) >> 15;
	value = pp->pid & PID_VALUE;
	if (!vendor && pp->pid >= PID_ID_TOKEN && pp->pid <= PID_DATA_TAGS)
		value -= PID_ADJUST_P0;
	if (value > pid_max [vendor])
		pid_max [vendor] = value;
	if ((vendor &&
	     value >= sizeof (non_std_pids) / sizeof (PIDDesc_t *)) ||
	    (!vendor &&
	     value >= sizeof (std_pids) / sizeof (PIDDesc_t *)))
		return;

	if (vendor)
		non_std_pids [value] = pp;
	else
		std_pids [value] = pp;
	for (t = PST_INLINE_QOS; t <= PST_TOPIC; t++)
		if (pp->ofs [t] >= 0)
			PID_ADD (pid_sets [t][vendor], value);
}

/* pid_init -- Initialize various PID parsing data structures. */

void pid_init (void)
{
	PIDDesc_t	*pp;
	unsigned	i;

	for (pp = known_pids, i = 0; i < N_STD_PIDS; pp++, i++)
		pid_init_entry (pp);

	for (pp = vendor_pids, i = 0; i < N_VENDOR_PIDS; pp++, i++)
		pid_init_entry (pp);

#ifdef LOCK_PARSE
	lock_init_nr (parse_lock, "pid_parse");
#endif
}

#ifdef _WIN32
#define snprintf sprintf_s
#endif

static const char *pid_name (unsigned pid)
{
	unsigned	value;
	static char	buf [32];

	if ((pid & PID_VENDOR_SPECIFIC) == 0) {
		value = (pid & PID_VALUE);
		if (value >= PID_ID_TOKEN && value <= PID_DATA_TAGS)
			value -= PID_ADJUST_P0;

		if (value < sizeof (std_pids) / sizeof (PIDDesc_t *) &&
		    std_pids [value] &&
		    std_pids [value]->name)
			return (std_pids [value]->name);
	}
	snprintf (buf, sizeof (buf), "?(0x%x)", pid);
	return (buf);
}

#define	SHORT_PID_SIZE	256	/* Maximum length of a short parameter. */

/* pid_parse -- Parse a parameter id list. */

static ssize_t pid_parse (DBW         *walk,
		          PIDSource_t type,
		          void        *data,
		          PIDSet_t    *pids,
		          PIDSet_t    *match,
		          int         swap)
{
	Parameter_t		*pp;
	PIDDesc_t		*dp;
	const PIDParDesc_t	*tp;
	int			vendor_bit;
	unsigned		value;
	size_t			prev_max = walk->length;
	unsigned char		*sp = NULL;
	void			*dst;
	int			error, vendor, tcode;
	uint16_t		parameter_id;
	uint16_t		length;
	unsigned char		buffer [SHORT_PID_SIZE];

	vendor = tcode = 0;
	while (walk->length) {
		if (log_pids)
			log_printf (RTPS_ID, 0, "pid_parse: max=%lu\r\n", (unsigned long) walk->length);

		if (walk->length < 4)
			return (-DDS_RETCODE_BAD_PARAMETER);

		DB_DATA_GET (pp, Parameter_t *, walk, 4, buffer);
		if (!pp)
			return (-DDS_RETCODE_OUT_OF_RESOURCES);

		walk->length -= 4;
		if (swap) {
			memcswap16 (&parameter_id, &pp->parameter_id);
			memcswap16 (&length, &pp->length);
		}
		else {
			parameter_id = pp->parameter_id;
			length = pp->length;
		}
		vendor_bit = (parameter_id & PID_VENDOR_SPECIFIC) >> 15;
		value = parameter_id & PID_VALUE;
		if (value >= PID_ID_TOKEN && value <= PID_DATA_TAGS)
			value -= PID_ADJUST_P0;
		if (log_pids)
			log_printf (RTPS_ID, 0, "pid_parse: parameter_id=%s, length=%u\r\n",
				pid_name (parameter_id), length);

		/* PID is valid if it is a either a standard PID or a vendor-
		   specific one and we are the vendor, and it is a known PID
		   that is applicable to the data argument. */
		if ((!vendor_bit ||
		     (vendor_bit && 
		      (vendor ||
		       (tcode && value == (PID_V_TYPECODE & PID_VALUE))))) &&
		      value <= pid_max [vendor_bit]) {
			dp = pid_descs [vendor_bit][value];
			/*if (vendor_bit && value == 4)
				dbg_printf ("TC");*/
			if (dp && dp->ofs [type] < 0)
				dp = NULL;
		}
		else
			dp = NULL;

		/* Don't check the length of a sentinel (not significant! */
		if (parameter_id == PID_SENTINEL)
			break;

		if (walk->length < length ||
		    (!dp && (parameter_id & PID_MUST_PARSE) != 0))
			return (-DDS_RETCODE_BAD_PARAMETER);

		walk->length -= length;

		/* If pid should be skipped, do so. */
		if (!dp || parameter_id == PID_PAD) {
			DB_DATA_SKIP (walk, length);
			continue;
		}

		/* Valid PID length and it fits in the data area; parse it. */
		tp = pid_type_desc [dp->ptype];
		if (tp->size && length != tp->size)
			return (-DDS_RETCODE_BAD_PARAMETER);

		/* If only selected PIDs are required, skip the others. */
		if (match && !PID_INSET (match [vendor_bit], value)) {
			DB_DATA_SKIP (walk, length);
			continue;
		}
		dst = (unsigned char *) data + dp->ofs [type];
		if ((tp->flags & PPF_LONG) != 0)
			error = (*tp->parsef) (parameter_id, dst,
					       (unsigned char *) walk,
					       length, swap);
		else {
			if (length >= SHORT_PID_SIZE) {
				warn_printf ( "pid_parse: parameter_id=%s too long for short mode: %u > %u\r\n", pid_name (parameter_id), length, SHORT_PID_SIZE);
				return (-DDS_RETCODE_BAD_PARAMETER);
			}
			DB_DATA_GET (sp, unsigned char *, walk, length, buffer);
			if (!sp)
				error = DDS_RETCODE_OUT_OF_RESOURCES;
			else
				error = (*tp->parsef) (parameter_id, dst,
					       	       sp, length, swap);
		}
		PID_ADD (pids [vendor_bit], value);
		if (!vendor_bit && value == PID_VENDOR_ID && sp) {
			if (sp [0] == VENDORID_H_TECHNICOLOR &&
			    sp [1] == VENDORID_L_TECHNICOLOR)
				vendor = tcode = 1;

			/* Some foreign typecode is also understood. */
			else if ((sp [0] == 1 && sp [1] == 1) ||       /* RTI */
				 (sp [0] == 1 && sp [1] == 6))	       /* TOC */
				tcode = 1;
		}
		if (error)
			return (-error);

		if (match && !memcmp (pids, match, sizeof (PIDSet_t) * 2))
			break;
	}
	return (prev_max - walk->length);
}

/* pid_parse_inline_qos -- Parse a Parameter Id list as used in the InlineQoS
			   field until either the end of the list is reached
			   or the data size (max) is not sufficient to contain
			   the list or an error occurred. */

ssize_t pid_parse_inline_qos (DBW         *walk,
			      InlineQos_t *qp,
			      PIDSet_t    pids [2],
			      int         swap)
{
	ssize_t		s;

	memset (qp, 0, sizeof (InlineQos_t));
	memset (pids, 0, sizeof (PIDSet_t) * 2);

	parse_entry ();
	s = pid_parse (walk, PST_INLINE_QOS, qp, pids, NULL, swap);
	parse_exit ();
	return (s);
}

/* pid_parse_participant_data -- Parse a Parameter Id list as used in the
				 SPDPdiscoveredParticipantData of the
				 builtin SPDPbuiltinParticipantReader/Writer
				 endpoints. */

ssize_t pid_parse_participant_data (DBW                           *walk,
				    SPDPdiscoveredParticipantData *dp,
				    PIDSet_t                      pids [2],
				    int                           swap)
{
	ssize_t		s;

	memset (dp, 0, sizeof (SPDPdiscoveredParticipantData));
	memset (pids, 0, sizeof (PIDSet_t));
	parse_entry ();
	s = pid_parse (walk, PST_PARTICIPANT, dp, pids, NULL, swap);
	parse_exit ();
	if (s < 0)
		return (s);

#define LOCF_MASK LOCF_DATA | LOCF_META | LOCF_UCAST | LOCF_MCAST | LOCF_SECURE | LOCF_SERVER
	locator_list_flags_set (dp->proxy.def_ucast, LOCF_MASK, LOCF_DATA | LOCF_UCAST);
	locator_list_flags_set (dp->proxy.def_mcast, LOCF_MASK, LOCF_DATA | LOCF_MCAST);
	locator_list_flags_set (dp->proxy.meta_ucast, LOCF_MASK, LOCF_META | LOCF_UCAST);
	locator_list_flags_set (dp->proxy.meta_mcast, LOCF_MASK, LOCF_META | LOCF_MCAST);
#undef LOCF_MASK
#ifdef DDS_NATIVE_SECURITY
	if (!PID_INSET (pids [1], (PID_V_SEC_CAPS ^ PID_VENDOR_SPECIFIC)))
		dp->proxy.sec_caps = (SECC_DDS_SEC << SECC_LOCAL) | SECC_DDS_SEC;
#endif
	return (s);
}

/* pid_parse_participant_key -- Parse a Parameter Id list as used in the
				SPDPdiscoveredParticipantData of the
				builtin SPDPbuiltinParticipantReader/Writer
				endpoints to retrieve the key fields. */

int pid_parse_participant_key (DBW walk, unsigned char *key, int swap)
{
	SPDPdiscoveredParticipantData	data;
	PIDSet_t			pids [2], req_pids [2];
	ssize_t				size;

	memset (&data, 0, sizeof (SPDPdiscoveredParticipantData));
	memset (pids, 0, sizeof (pids));
	memset (req_pids, 0, sizeof (req_pids));
	PID_ADD (req_pids [0], PID_PARTICIPANT_GUID);
	parse_entry ();
	size = pid_parse (&walk, PST_PARTICIPANT, &data, pids, req_pids, swap);
	parse_exit ();
	if (size < 0)
		return (-size);

	if (!PID_INSET (pids [0], PID_PARTICIPANT_GUID))
		return (DDS_RETCODE_ALREADY_DELETED);

	memcpy (key, &data.proxy.guid_prefix, pid_participant_key_size ());
	return (DDS_RETCODE_OK);
}

/* pid_parse_reader_data -- Parse a Parameter Id list as used in the
			    DiscoveredReaderData of the builtin
			    SEDPbuiltinSubscriptionsReader/Writer endpoints. */

ssize_t pid_parse_reader_data (DBW                  *walk,
			       DiscoveredReaderData *dp,
			       PIDSet_t             pids [2],
			       int                  swap)
{
	ssize_t		ss;

	memset (dp, 0, offsetof (DiscoveredReaderData, qos));
	dp->qos = qos_def_disc_reader_qos;
	dp->filter = NULL;
#ifdef DDS_TYPECODE
	dp->typecode = NULL;
#endif
	memset (pids, 0, sizeof (PIDSet_t) * 2);
	parse_entry ();
	ss = pid_parse (walk, PST_READER, dp, pids, NULL, swap);
	parse_exit ();
	return (ss);
}

/* pid_parse_reader_key -- Parse a Parameter Id list as used in the
			   DiscoveredReaderData of the builtin
			   SEDPbuiltinSubscriptionsReader/Writer endpoints
			   to retrieve the key fields. */

int pid_parse_reader_key (DBW walk, unsigned char *key, int swap)
{
	DiscoveredReaderData	data;
	PIDSet_t		pids [2], req_pids [2];
	ssize_t			size;

	memset (&data.proxy.guid, 0, sizeof (GUID_t));
	memset (pids, 0, sizeof (pids));
	memset (req_pids, 0, sizeof (req_pids));
	PID_ADD (req_pids [0], PID_ENDPOINT_GUID);
	parse_entry ();
	size = pid_parse (&walk, PST_READER, &data, pids, req_pids, swap);
	parse_exit ();
	if (size < 0)
		return (-size);

	if (!PID_INSET (pids [0], PID_ENDPOINT_GUID))
		return (DDS_RETCODE_ALREADY_DELETED);

	memcpy (key, &data.proxy.guid, pid_reader_key_size ());
	return (DDS_RETCODE_OK);
}

/* pid_parse_writer_data -- Parse a Parameter Id list as used in the
			    DiscoveredWriterData of the builtin
			    SEDPbuiltinPublicationsReader/Writer endpoints. */

ssize_t pid_parse_writer_data (DBW                  *walk,
			       DiscoveredWriterData *dp,
			       PIDSet_t             pids [2],
			       int                  swap)
{
	ssize_t	ss;

	memset (dp, 0, offsetof (DiscoveredWriterData, qos));
	dp->qos = qos_def_disc_writer_qos;
#ifdef DDS_TYPECODE
	dp->typecode = NULL;
#endif
	memset (pids, 0, sizeof (PIDSet_t) * 2);
	parse_entry ();
	ss = pid_parse (walk, PST_WRITER, dp, pids, NULL, swap);
	parse_exit ();
	return (ss);
}

/* pid_parse_writer_key -- Parse a Parameter Id list as used in the
			   DiscoveredWriterData of the builtin
			   SEDPbuiltinPublicationsReader/Writer endpoints
			   to retrieve the key fields. */

int pid_parse_writer_key (DBW walk, unsigned char *key, int swap)
{
	DiscoveredWriterData	data;
	PIDSet_t		pids [2], req_pids [2];
	ssize_t			size;

	memset (&data.proxy.guid, 0, sizeof (GUID_t));
	memset (pids, 0, sizeof (pids));
	memset (req_pids, 0, sizeof (req_pids));
	PID_ADD (req_pids [0], PID_ENDPOINT_GUID);
	parse_entry ();
	size = pid_parse (&walk, PST_WRITER, &data, pids, req_pids, swap);
	parse_exit ();
	if (size < 0)
		return (-size);

	if (!PID_INSET (pids [0], PID_ENDPOINT_GUID))
		return (DDS_RETCODE_ALREADY_DELETED);

	memcpy (key, &data.proxy.guid, pid_writer_key_size ());
	return (DDS_RETCODE_OK);
}

/* pid_parse_topic_data -- Parse a Parameter Id list as used in the
			   DiscoveredTopicData of the builtin
			   SEDPbuiltinTopicsReader/Writer endpoints. */

ssize_t pid_parse_topic_data (DBW                 *walk,
			      DiscoveredTopicData *dp,
			      PIDSet_t            pids [2],
			      int                 swap)
{
	ssize_t		ss;

	memset (dp, 0, offsetof (DiscoveredTopicData, qos));
	dp->qos = qos_def_disc_topic_qos;
#ifdef DDS_TYPECODE
	dp->typecode = NULL;
#endif
	memset (pids, 0, sizeof (PIDSet_t) * 2);

	parse_entry ();
	ss = pid_parse (walk, PST_TOPIC, dp, pids, NULL, swap);
	parse_exit ();
	return (ss);
}

/* topic_key_from_name -- Get a topic key from the topic name and type name. */

void topic_key_from_name (const char *name,
			  unsigned   name_length,
			  const char *type_name,
			  unsigned   type_name_length,
			  KeyHash_t  *key)
{
	MD5_CONTEXT	mdc;

	md5_init (&mdc);
#if (ENDIAN_CPU == ENDIAN_LITTLE)
	memcswap32 (key->hash, &name_length);
#else
	memcpy32 (key->hash, &name_length);
#endif
	md5_update (&mdc, key->hash, 4);
	md5_update (&mdc, (unsigned char *) name, name_length + 1);
#if (ENDIAN_CPU == ENDIAN_LITTLE)
	memcswap32 (key->hash, &type_name_length);
#else
	memcpy32 (key->hash, &type_name_length);
#endif
	md5_update (&mdc, key->hash, 4);
	md5_update (&mdc, (unsigned char *) type_name, type_name_length + 1);
	md5_final (key->hash, &mdc);
}

/* pid_parse_topic_key -- Parse topic key fields and return the key data. */

int pid_parse_topic_key (DBW walk, unsigned char *key, int swap)
{
	DiscoveredTopicData	data;
	PIDSet_t		pids [2], req_pids [2];
	ssize_t			size;

	if (!key)
		return (DDS_RETCODE_BAD_PARAMETER);

	data.name = data.type_name = NULL;
	memset (pids, 0, sizeof (pids));
	memset (req_pids, 0, sizeof (req_pids));
	PID_ADD (req_pids [0], PID_TOPIC_NAME);
	PID_ADD (req_pids [0], PID_TYPE_NAME);
	parse_entry ();
	size = pid_parse (&walk, PST_TOPIC, &data, pids, req_pids, swap);
	parse_exit ();
	if (size > 0 &&
	    PID_INSET (pids [0], PID_TOPIC_NAME) &&
	    PID_INSET (pids [0], PID_TYPE_NAME)) {
		topic_key_from_name (str_ptr (data.name),
				     str_len (data.name) - 1,
				     str_ptr (data.type_name),
				     str_len (data.type_name) - 1,
				     (KeyHash_t *) key);
	}
	else if (size < 0)
		return (-size);

	if (data.name)
		str_unref (data.name);
	if (data.type_name)
		str_unref (data.type_name);
	return (DDS_RETCODE_OK);
}

#ifdef DDS_NATIVE_SECURITY

static void free_tokens (Token_t *list)
{
	token_unref (list);
}

#endif

/* pid_participant_data_cleanup -- Cleanup received participant data extra
				   allocations. */

void pid_participant_data_cleanup (SPDPdiscoveredParticipantData *dp)
{
	if (dp->proxy.def_ucast)
		locator_list_delete_list (&dp->proxy.def_ucast);
	if (dp->proxy.def_mcast)
		locator_list_delete_list (&dp->proxy.def_mcast);
	if (dp->proxy.meta_ucast)
		locator_list_delete_list (&dp->proxy.meta_ucast);
	if (dp->proxy.meta_mcast)
		locator_list_delete_list (&dp->proxy.meta_mcast);
	if (dp->user_data) {
		str_unref (dp->user_data);
		dp->user_data = NULL;
	}
	if (dp->entity_name) {
		str_unref (dp->entity_name);
		dp->entity_name = NULL;
	}
#ifdef DDS_SECURITY
	if (dp->proxy.sec_locs)
		locator_list_delete_list (&dp->proxy.sec_locs);
#ifdef DDS_NATIVE_SECURITY
	free_tokens (dp->id_tokens);
	free_tokens (dp->p_tokens);
#else
	if (dp->id_tokens)
		str_unref (dp->id_tokens);
	if (dp->p_tokens)
		str_unref (dp->p_tokens);
#endif
	dp->id_tokens = NULL;
	dp->p_tokens = NULL;
#endif
}

/* pid_topic_data_cleanup -- Cleanup received topic data extra allocations. */

void pid_topic_data_cleanup (DiscoveredTopicData *tp)
{
	if (tp->name) {
		str_unref (tp->name);
		tp->name = NULL;
	}
	if (tp->type_name) {
		str_unref (tp->type_name);
		tp->type_name = NULL;
	}
	if (tp->qos.topic_data) {
		str_unref (tp->qos.topic_data);
		tp->qos.topic_data = NULL;
	}
#ifdef DDS_TYPECODE
	if (tp->typecode) {
		vtc_free (tp->typecode);
		tp->typecode = NULL;
	}
#endif
}

/* pid_reader_data_cleanup -- Cleanup received subscription data extra
			      allocations. */

void pid_reader_data_cleanup (DiscoveredReaderData *rp)
{
	unsigned	i;
	String_t	*sp;

	if (rp->proxy.ucast)
		locator_list_delete_list (&rp->proxy.ucast);
	if (rp->proxy.mcast)
		locator_list_delete_list (&rp->proxy.mcast);
	if (rp->topic_name) {
		str_unref (rp->topic_name);
		rp->topic_name = NULL;
	}
	if (rp->type_name) {
		str_unref (rp->type_name);
		rp->type_name = NULL;
	}
	if (rp->qos.user_data) {
		str_unref (rp->qos.user_data);
		rp->qos.user_data = NULL;
	}
	if (rp->qos.partition) {
		for (i = 0; i < rp->qos.partition->_length; i++) {
			sp = rp->qos.partition->_buffer [i];
			str_unref (sp);
		}
		xfree (rp->qos.partition);
	}
	if (rp->qos.topic_data) {
		str_unref (rp->qos.topic_data);
		rp->qos.topic_data = NULL;
	}
	if (rp->qos.group_data) {
		str_unref (rp->qos.group_data);
		rp->qos.group_data = NULL;
	}
	if (rp->filter) {
		filter_data_cleanup (rp->filter);
		xfree (rp->filter);
		rp->filter = NULL;
	}
#ifdef DDS_TYPECODE
	if (rp->typecode) {
		vtc_free (rp->typecode);
		rp->typecode = NULL;
	}
#endif
}

/* pid_writer_data_cleanup -- Cleanup received publication data extra
			      allocations. */

void pid_writer_data_cleanup (DiscoveredWriterData *wp)
{
	unsigned	i;
	String_t	*sp;

	if (wp->proxy.ucast)
		locator_list_delete_list (&wp->proxy.ucast);
	if (wp->proxy.mcast)
		locator_list_delete_list (&wp->proxy.mcast);
	if (wp->topic_name) {
		str_unref (wp->topic_name);
		wp->topic_name = NULL;
	}
	if (wp->type_name) {
		str_unref (wp->type_name);
		wp->type_name = NULL;
	}
	if (wp->qos.user_data) {
		str_unref (wp->qos.user_data);
		wp->qos.user_data = NULL;
	}
	if (wp->qos.partition) {
		for (i = 0; i < wp->qos.partition->_length; i++) {
			sp = wp->qos.partition->_buffer [i];
			str_unref (sp);
		}
		xfree (wp->qos.partition);
	}
	if (wp->qos.topic_data) {
		str_unref (wp->qos.topic_data);
		wp->qos.topic_data = NULL;
	}
	if (wp->qos.group_data) {
		str_unref (wp->qos.group_data);
		wp->qos.group_data = NULL;
	}
#ifdef DDS_TYPECODE
	if (wp->typecode) {
		vtc_free (wp->typecode);
		wp->typecode = NULL;
	}
#endif
}


/* Message creation functions.
   --------------------------- */

/* pid_size -- Return the minimum size required for the given Parameter Id. */

ssize_t pid_size (unsigned pid, const void *data)
{
	PIDDesc_t		*dp;
	const PIDParDesc_t	*tp;
	size_t			size;
	int			vendor_bit = (pid & PID_VENDOR_SPECIFIC) >> 15;
	unsigned		value = pid & PID_VALUE;

	if (!vendor_bit && value >= PID_ID_TOKEN && value <= PID_DATA_TAGS)
		value -= PID_ADJUST_P0;
	if (value > pid_max [vendor_bit] ||
	    (dp = pid_descs [vendor_bit][value]) == NULL)
	    	return (-DDS_RETCODE_BAD_PARAMETER);

	tp = pid_type_desc [dp->ptype];
	if (tp) {
		if (tp->size)
			size = tp->size;
		else
			size = (*tp->wsizef) (data);
	}
	else if (dp->ptype == PT_End)
		size = 0;
	else
		return (-DDS_RETCODE_BAD_PARAMETER);

	return (size + sizeof (uint16_t) * 2);
}

/* pid_add -- Put a Parameter Id in the buffer at the given offset.
	      The resulting buffer size will be returned. */

ssize_t pid_add (unsigned char *buf, unsigned pid, const void *data)
{
	PIDDesc_t		*dp;
	const PIDParDesc_t	*tp;
	Parameter_t		*pp = (Parameter_t *) buf;
	ssize_t			size;
	int			vendor_bit = (pid & PID_VENDOR_SPECIFIC) >> 15;
	unsigned		value = pid & PID_VALUE;

	if (!vendor_bit && value >= PID_ID_TOKEN && value <= PID_DATA_TAGS)
		value -= PID_ADJUST_P0;
	if (value > pid_max [vendor_bit] ||
	    (dp = pid_descs [vendor_bit][value]) == NULL)
	    	return (-DDS_RETCODE_BAD_PARAMETER);

	tp = pid_type_desc [dp->ptype];
	if (!tp)
		return (-DDS_RETCODE_BAD_PARAMETER);

	buf += sizeof (uint16_t) * 2;
	size = (*tp->writef) (buf, data);
	if (size < 0)
		return (size);

	pp->parameter_id = pid;
	pp->length = (unsigned) size;
	return (size + sizeof (uint16_t) * 2);
}

/* pid_add_locators -- Add a locator list. */

ssize_t pid_add_locators (unsigned char *buf, unsigned pid, const LocatorRef_t *lp)
{
	PIDDesc_t		*dp;
	const PIDParDesc_t	*tp;
	Parameter_t		*pp;
	ssize_t			size;
	size_t			total = 0;
	int			vendor_bit = (pid & PID_VENDOR_SPECIFIC) >> 15;
	unsigned		value = pid & PID_VALUE;

	if (value > pid_max [vendor_bit] ||
	    (dp = pid_descs [vendor_bit][value]) == NULL)
	    	return (-DDS_RETCODE_BAD_PARAMETER);

	tp = pid_type_desc [dp->ptype];
	if (!tp)
		return (-DDS_RETCODE_BAD_PARAMETER);

	while (lp) {
		pp = (Parameter_t *) buf;
		buf += sizeof (uint16_t) * 2;
		size = (*tp->writef) (buf, &lp->data->locator);
		buf += size;
		pp->length = (unsigned) size;
		total += size + sizeof (uint16_t) * 2;
		lp = lp->next;
	}
	return (total);
}

/* pid_finish -- Same semantics as pid_add (), but adds the final sentinel. */

size_t pid_finish (unsigned char *buf)
{
	uint16_t	*sp = (uint16_t *) buf;

	*sp++ = PID_SENTINEL;
	*sp = 0;
	return (sizeof (uint16_t) * 2);
}

/* pid_locators_size -- Return the size of a locators list. */

ssize_t pid_locators_size (unsigned pid, const LocatorRef_t *lp)
{
	ssize_t		size, s;

	size = 0;
	for (; lp; lp = lp->next) {
		s = pid_size (pid, &lp->data->locator);
		if (s < 0)
			return (s);

		size += s;
	}
	return (size);
}

#define	SCHK(fct,s,x) { if ((s = fct) < 0) return (s); else x += s; }

/* pid_participant_data_size -- Return the size of the data field for the given
				instance of the builtin
				SPDPbuiltinParticipantReader/Writer endpoints.*/

ssize_t pid_participant_data_size (const SPDPdiscoveredParticipantData *dp)
{
	ssize_t		s, size = 0;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Token_t		*tp;
#endif
	uint32_t	version;

	SCHK (pid_size (PID_PARTICIPANT_GUID, &dp->proxy.guid_prefix), s, size);
	SCHK (pid_size (PID_BUILTIN_ENDPOINT_SET, &dp->proxy.builtins), s, size);
	SCHK (pid_size (PID_PROTOCOL_VERSION, &dp->proxy.proto_version), s, size);
	SCHK (pid_size (PID_VENDOR_ID, &dp->proxy.vendor_id), s, size);
	SCHK (pid_size (PID_EXPECTS_INLINE_QOS, &dp->proxy.exp_il_qos), s, size);
	if (dp->proxy.def_ucast)
		SCHK (pid_locators_size (PID_DEFAULT_UNICAST_LOCATOR, dp->proxy.def_ucast), s, size);
	if (dp->proxy.def_mcast)
		SCHK (pid_locators_size (PID_DEFAULT_MULTICAST_LOCATOR, dp->proxy.def_mcast), s, size);
	if (dp->proxy.meta_ucast)
		SCHK (pid_locators_size (PID_META_UNICAST_LOCATOR, dp->proxy.meta_ucast), s, size);
	if (dp->proxy.meta_mcast)
		SCHK (pid_locators_size (PID_META_MULTICAST_LOCATOR, dp->proxy.meta_mcast), s, size);
	SCHK (pid_size (PID_PARTICIPANT_MAN_LIVELINESS, &dp->proxy.manual_liveliness), s, size);
	if (dp->user_data)
		SCHK (pid_size (PID_USER_DATA, dp->user_data), s, size);
	SCHK (pid_size (PID_PARTICIPANT_LEASE_DURATION, &dp->lease_duration), s, size);
	if (dp->entity_name)
		SCHK (pid_size (PID_ENTITY_NAME, dp->entity_name), s, size);
	SCHK (pid_size (PID_V_VERSION, &version), s, size);
	if (dp->proxy.no_mcast)
		SCHK (pid_size (PID_V_NO_MCAST, &dp->proxy.no_mcast), s, size);
#ifdef DDS_SECURITY
	if (dp->proxy.sec_caps)
		SCHK (pid_size (PID_V_SEC_CAPS, &dp->proxy.sec_caps), s, size);
	if (dp->proxy.sec_locs)
		SCHK (pid_locators_size (PID_V_SEC_LOC, dp->proxy.sec_locs), s, size);
#ifdef DDS_NATIVE_SECURITY
	for (tp = dp->id_tokens; tp; tp = tp->next)
		if (tp->encoding) {
			SCHK (pid_size (tp->encoding, tp), s, size);
		}
		else {
			SCHK (pid_size (PID_ID_TOKEN, tp), s, size);
		}
	for (tp = dp->p_tokens; tp; tp = tp->next)
		if (tp->encoding) {
			SCHK (pid_size (tp->encoding, tp), s, size);
		}
		else {
			SCHK (pid_size (PID_PERMISSIONS_TOKEN, tp), s, size);
		}
#else
	if (dp->id_tokens)
		SCHK (pid_size (PID_V_IDENTITY, dp->id_tokens), s, size);
	if (dp->p_tokens)
		SCHK (pid_size (PID_V_PERMS, dp->p_tokens), s, size);
#endif
#endif
#ifdef DDS_FORWARD
	if (dp->proxy.forward)
		SCHK (pid_size (PID_V_FORWARD, &dp->proxy.forward), s, size);
#endif
	SCHK (pid_size (PID_SENTINEL, NULL), s, size);
	/*printf ("pid-participant-size=%lu\r\n", (unsigned long) size);*/
	return (size);
}

/* pid_reader_data_size -- Return the size of the data field for the given
			   instance of the builtin
			   SEDPbuiltinSubscriptionsReader/Writer endpoints. */

ssize_t pid_reader_data_size (const DiscoveredReaderData *dp)
{
	ssize_t	s, size = 0;

	SCHK (pid_size (PID_ENDPOINT_GUID, &dp->proxy.guid), s, size);
#ifdef INLINE_QOS_SUPPORT
	if (dp->proxy.exp_il_qos)
		SCHK (pid_size (PID_EXPECTS_INLINE_QOS, &dp->proxy.expects_inline_qos), s, size);
#endif
	if (dp->proxy.ucast)
		SCHK (pid_locators_size (PID_UNICAST_LOCATOR, dp->proxy.ucast), s, size);
	if (dp->proxy.mcast)
		SCHK (pid_locators_size (PID_MULTICAST_LOCATOR, dp->proxy.mcast), s, size);
	SCHK (pid_size (PID_TOPIC_NAME, dp->topic_name), s, size);
	SCHK (pid_size (PID_TYPE_NAME, dp->type_name), s, size);
	SCHK (pid_size (PID_DURABILITY, &dp->qos.durability), s, size);
	SCHK (pid_size (PID_DEADLINE, &dp->qos.deadline), s, size);
	SCHK (pid_size (PID_LATENCY_BUDGET, &dp->qos.latency_budget), s, size);
	SCHK (pid_size (PID_LIVELINESS, &dp->qos.liveliness), s, size);
	SCHK (pid_size (PID_RELIABILITY, &dp->qos.reliability), s, size);
	SCHK (pid_size (PID_OWNERSHIP, &dp->qos.ownership), s, size);
	SCHK (pid_size (PID_DESTINATION_ORDER, &dp->qos.destination_order), s, size);
	if (dp->qos.user_data)
		SCHK (pid_size (PID_USER_DATA, dp->qos.user_data), s, size);
	SCHK (pid_size (PID_TIME_BASED_FILTER, &dp->qos.time_based_filter), s, size);
	SCHK (pid_size (PID_PRESENTATION, &dp->qos.presentation), s, size);
	if (dp->qos.partition)
		SCHK (pid_size (PID_PARTITION, dp->qos.partition), s, size);
	if (dp->qos.topic_data)
		SCHK (pid_size (PID_TOPIC_DATA, dp->qos.topic_data), s, size);
	if (dp->qos.group_data)
		SCHK (pid_size (PID_GROUP_DATA, dp->qos.group_data), s, size);
	if (dp->filter)
		SCHK (pid_size (PID_CONTENT_FILTER_PROPERTY, dp->filter), s, size);
#ifdef DDS_TYPECODE
	if (dp->typecode) {
		SCHK (pid_size (PID_VENDOR_ID, &dp->vendor_id), s, size);
		SCHK (pid_size (PID_V_TYPECODE, dp->typecode), s, size);
	}
#endif
	SCHK (pid_size (PID_SENTINEL, NULL), s, size);
	return (size);
}

/* pid_writer_data_size -- Return the size of the data field for the given
			   instance of the builtin
			   SEDPbuiltinPublicationsReader/Writer endpoints. */

ssize_t pid_writer_data_size (const DiscoveredWriterData *dp)
{
	ssize_t	s, size = 0;

	SCHK (pid_size (PID_ENDPOINT_GUID, &dp->proxy.guid), s, size);
	if (dp->proxy.ucast)
		SCHK (pid_locators_size (PID_UNICAST_LOCATOR, dp->proxy.ucast), s, size);
	if (dp->proxy.mcast)
		SCHK (pid_locators_size (PID_MULTICAST_LOCATOR, dp->proxy.mcast), s, size);
	SCHK (pid_size (PID_TOPIC_NAME, dp->topic_name), s, size);
	SCHK (pid_size (PID_TYPE_NAME, dp->type_name), s, size);
	SCHK (pid_size (PID_DURABILITY, &dp->qos.durability), s, size);
	SCHK (pid_size (PID_DURABILITY_SERVICE, &dp->qos.durability_service), s, size);
	SCHK (pid_size (PID_DEADLINE, &dp->qos.deadline), s, size);
	SCHK (pid_size (PID_LATENCY_BUDGET, &dp->qos.latency_budget), s, size);
	SCHK (pid_size (PID_LIVELINESS, &dp->qos.liveliness), s, size);
	SCHK (pid_size (PID_RELIABILITY, &dp->qos.reliability), s, size);
	SCHK (pid_size (PID_LIFESPAN, &dp->qos.lifespan), s, size);
	if (dp->qos.user_data)
		SCHK (pid_size (PID_USER_DATA, dp->qos.user_data), s, size);
	SCHK (pid_size (PID_OWNERSHIP, &dp->qos.ownership), s, size);
	SCHK (pid_size (PID_OWNERSHIP_STRENGTH, &dp->qos.ownership_strength), s, size);
	SCHK (pid_size (PID_DESTINATION_ORDER, &dp->qos.destination_order), s, size);
	SCHK (pid_size (PID_PRESENTATION, &dp->qos.presentation), s, size);
	if (dp->qos.partition)
		SCHK (pid_size (PID_PARTITION, dp->qos.partition), s, size);
	if (dp->qos.topic_data)
		SCHK (pid_size (PID_TOPIC_DATA, dp->qos.topic_data), s, size);
	if (dp->qos.group_data)
		SCHK (pid_size (PID_GROUP_DATA, dp->qos.group_data), s, size);
#ifdef DDS_TYPECODE
	if (dp->typecode) {
		SCHK (pid_size (PID_VENDOR_ID, &dp->vendor_id), s, size);
		SCHK (pid_size (PID_V_TYPECODE, dp->typecode), s, size);
	}
#endif
	SCHK (pid_size (PID_SENTINEL, NULL), s, size);
	return (size);
}

/* pid_topic_data_size -- Return the size of the data field for the given
			  instance of the builtin SEDPbuiltinTopicReader/Writer
			  endpoints. */

ssize_t pid_topic_data_size (const DiscoveredTopicData *dp)
{
	ssize_t	s, size = 0;

	SCHK (pid_size (PID_TOPIC_NAME, dp->name), s, size);
	SCHK (pid_size (PID_TYPE_NAME, dp->type_name), s, size);
	SCHK (pid_size (PID_DURABILITY, &dp->qos.durability), s, size);
	SCHK (pid_size (PID_DURABILITY_SERVICE, &dp->qos.durability_service), s, size);
	SCHK (pid_size (PID_DEADLINE, &dp->qos.deadline), s, size);
	SCHK (pid_size (PID_LATENCY_BUDGET, &dp->qos.latency_budget), s, size);
	SCHK (pid_size (PID_LIVELINESS, &dp->qos.liveliness), s, size);
	SCHK (pid_size (PID_RELIABILITY, &dp->qos.reliability), s, size);
	SCHK (pid_size (PID_TRANSPORT_PRIORITY, &dp->qos.transport_priority), s, size);
	SCHK (pid_size (PID_LIFESPAN, &dp->qos.lifespan), s, size);
	SCHK (pid_size (PID_DESTINATION_ORDER, &dp->qos.destination_order), s, size);
	SCHK (pid_size (PID_HISTORY, &dp->qos.history), s, size);
	SCHK (pid_size (PID_RESOURCE_LIMITS, &dp->qos.resource_limits), s, size);
	SCHK (pid_size (PID_OWNERSHIP, &dp->qos.ownership), s, size);
	if (dp->qos.topic_data)
		SCHK (pid_size (PID_TOPIC_DATA, dp->qos.topic_data), s, size);
#ifdef DDS_TYPECODE
	if (dp->typecode) {
		SCHK (pid_size (PID_VENDOR_ID, &dp->vendor_id), s, size);
		SCHK (pid_size (PID_V_TYPECODE, dp->typecode), s, size);
	}
#endif
	SCHK (pid_size (PID_SENTINEL, NULL), s, size);
	return (size);
}

/* pid_locators_add -- Add a locators list. */

ssize_t pid_locators_add (unsigned char *p, unsigned pid, const LocatorRef_t *lp)
{
	ssize_t		s;
	unsigned char	*sp = p;

	for (; lp; lp = lp->next)
		SCHK (pid_add (p, pid, &lp->data->locator), s, p);

	return (p - sp);
}

/* pid_add_participant_data -- Adds all Parameter Ids as needed for the data
			       fieldof the SPDPbuiltinParticipantReader/Writer
			       endpoints. */

ssize_t pid_add_participant_data (unsigned char                       *p,
			          const SPDPdiscoveredParticipantData *dp)
{
	ssize_t		s;
	unsigned char	*sp;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Token_t		*tp;
#endif
	static uint32_t	version = TDDS_VERSION;

	sp = p;
	SCHK (pid_add (p, PID_PARTICIPANT_GUID, &dp->proxy.guid_prefix), s, p);
	SCHK (pid_add (p, PID_BUILTIN_ENDPOINT_SET, &dp->proxy.builtins), s, p);
	SCHK (pid_add (p, PID_PROTOCOL_VERSION, &dp->proxy.proto_version), s, p);
	SCHK (pid_add (p, PID_VENDOR_ID, &dp->proxy.vendor_id), s, p);
	SCHK (pid_add (p, PID_EXPECTS_INLINE_QOS, &dp->proxy.exp_il_qos), s, p);
	if (dp->proxy.def_ucast)
		SCHK (pid_locators_add (p, PID_DEFAULT_UNICAST_LOCATOR, dp->proxy.def_ucast), s, p);
	if (dp->proxy.def_mcast)
		SCHK (pid_locators_add (p, PID_DEFAULT_MULTICAST_LOCATOR, dp->proxy.def_mcast), s, p);
	if (dp->proxy.meta_ucast)
		SCHK (pid_locators_add (p, PID_META_UNICAST_LOCATOR, dp->proxy.meta_ucast), s, p);
	if (dp->proxy.meta_mcast)
		SCHK (pid_locators_add (p, PID_META_MULTICAST_LOCATOR, dp->proxy.meta_mcast), s, p);
	SCHK (pid_add (p, PID_PARTICIPANT_MAN_LIVELINESS, &dp->proxy.manual_liveliness), s, p);
	if (dp->user_data)
		SCHK (pid_add (p, PID_USER_DATA, dp->user_data), s, p);
	SCHK (pid_add (p, PID_PARTICIPANT_LEASE_DURATION, &dp->lease_duration), s, p);
	if (dp->entity_name)
		SCHK (pid_add (p, PID_ENTITY_NAME, dp->entity_name), s, p);
	SCHK (pid_add (p, PID_V_VERSION, &version), s, p);
	if (dp->proxy.no_mcast)
		SCHK (pid_add (p, PID_V_NO_MCAST, &dp->proxy.no_mcast), s, p);
#ifdef DDS_SECURITY
	if (dp->proxy.sec_caps)
		SCHK (pid_add (p, PID_V_SEC_CAPS, &dp->proxy.sec_caps), s, p);
	if (dp->proxy.sec_locs)
		SCHK (pid_locators_add (p, PID_V_SEC_LOC, dp->proxy.sec_locs), s, p);
#ifdef DDS_NATIVE_SECURITY
	for (tp = dp->id_tokens; tp; tp = tp->next)
		if (tp->encoding) {
			SCHK (pid_add (p, tp->encoding, tp), s, p);
		}
		else {
			SCHK (pid_add (p, PID_ID_TOKEN, tp), s, p);
		}
	for (tp = dp->p_tokens; tp; tp = tp->next)
		if (tp->encoding) {
			SCHK (pid_add (p, tp->encoding, tp), s, p);
		}
		else {
			SCHK (pid_add (p, PID_PERMISSIONS_TOKEN, tp), s, p);
		}
#else
	if (dp->id_tokens)
		SCHK (pid_add (p, PID_V_IDENTITY, dp->id_tokens), s, p);
	if (dp->p_tokens)
		SCHK (pid_add (p, PID_V_PERMS, dp->p_tokens), s, p);
#endif
#endif
#ifdef DDS_FORWARD
	if (dp->proxy.forward)
		SCHK (pid_add (p, PID_V_FORWARD, &dp->proxy.forward), s, p);
#endif
	SCHK (pid_finish (p), s, p);
	return (p - sp);
}

/* pid_add_reader_data -- Adds all Parameter Ids as needed for the data field
			  of the builtin SPDPbuiltinSubscriptionsReader/Writer
			  endpoints. */

ssize_t pid_add_reader_data (unsigned char *p, const DiscoveredReaderData *dp)
{
	ssize_t		s;
	unsigned char	*sp;

	sp = p;
	SCHK (pid_add (p, PID_ENDPOINT_GUID, &dp->proxy.guid), s, p);
#ifdef INLINE_QOS_SUPPORT
	if (dp->proxy.exp_il_qos)
		SCHK (pid_add (p, PID_EXPECTS_INLINE_QOS, &dp->proxy.exp_il_qos), s, p);
#endif
	if (dp->proxy.ucast)
		SCHK (pid_locators_add (p, PID_UNICAST_LOCATOR, dp->proxy.ucast), s, p);
	if (dp->proxy.mcast)
		SCHK (pid_locators_add (p, PID_MULTICAST_LOCATOR, dp->proxy.mcast), s, p);
	SCHK (pid_add (p, PID_TOPIC_NAME, dp->topic_name), s, p);
	SCHK (pid_add (p, PID_TYPE_NAME, dp->type_name), s, p);
	SCHK (pid_add (p, PID_DURABILITY, &dp->qos.durability), s, p);
	SCHK (pid_add (p, PID_DEADLINE, &dp->qos.deadline), s, p);
	SCHK (pid_add (p, PID_LATENCY_BUDGET, &dp->qos.latency_budget), s, p);
	SCHK (pid_add (p, PID_LIVELINESS, &dp->qos.liveliness), s, p);
	SCHK (pid_add (p, PID_RELIABILITY, &dp->qos.reliability), s, p);
	SCHK (pid_add (p, PID_OWNERSHIP, &dp->qos.ownership), s, p);
	SCHK (pid_add (p, PID_DESTINATION_ORDER, &dp->qos.destination_order), s, p);
	if (dp->qos.user_data)
		SCHK (pid_add (p, PID_USER_DATA, dp->qos.user_data), s, p);
	SCHK (pid_add (p, PID_TIME_BASED_FILTER, &dp->qos.time_based_filter), s, p);
	SCHK (pid_add (p, PID_PRESENTATION, &dp->qos.presentation), s, p);
	if (dp->qos.partition)
		SCHK (pid_add (p, PID_PARTITION, dp->qos.partition), s, p);
	if (dp->qos.topic_data)
		SCHK (pid_add (p, PID_TOPIC_DATA, dp->qos.topic_data), s, p);
	if (dp->qos.group_data)
		SCHK (pid_add (p, PID_GROUP_DATA, dp->qos.group_data), s, p);
	if (dp->filter)
		SCHK (pid_add (p, PID_CONTENT_FILTER_PROPERTY, dp->filter), s, p);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (dp->tags)
		SCHK (pid_add (p, PID_DATA_TAGS, dp->tags), s, p);
#endif
#ifdef DDS_TYPECODE
	if (dp->typecode) {
		SCHK (pid_add (p, PID_VENDOR_ID, dp->vendor_id), s, p);
		SCHK (pid_add (p, PID_V_TYPECODE, dp->typecode), s, p);
	}
#endif
	SCHK (pid_finish (p), s, p);
	return (p - sp);
}

/* pid_add_writer_data -- Adds all Parameter Ids as needed for the data field of
			  the builtin SPDPbuiltinPublicationsReader/Writer
			  endpoints. */

ssize_t pid_add_writer_data (unsigned char *p, const DiscoveredWriterData *dp)
{
	ssize_t		s;
	unsigned char	*sp;

	sp = p;
	SCHK (pid_add (p, PID_ENDPOINT_GUID, &dp->proxy.guid), s, p);
	if (dp->proxy.ucast)
		SCHK (pid_locators_size (PID_UNICAST_LOCATOR, dp->proxy.ucast), s, p);
	if (dp->proxy.mcast)
		SCHK (pid_locators_size (PID_MULTICAST_LOCATOR, dp->proxy.mcast), s, p);
	SCHK (pid_add (p, PID_TOPIC_NAME, dp->topic_name), s, p);
	SCHK (pid_add (p, PID_TYPE_NAME, dp->type_name), s, p);
	SCHK (pid_add (p, PID_DURABILITY, &dp->qos.durability), s, p);
	SCHK (pid_add (p, PID_DURABILITY_SERVICE, &dp->qos.durability_service), s, p);
	SCHK (pid_add (p, PID_DEADLINE, &dp->qos.deadline), s, p);
	SCHK (pid_add (p, PID_LATENCY_BUDGET, &dp->qos.latency_budget), s, p);
	SCHK (pid_add (p, PID_LIVELINESS, &dp->qos.liveliness), s, p);
	SCHK (pid_add (p, PID_RELIABILITY, &dp->qos.reliability), s, p);
	SCHK (pid_add (p, PID_LIFESPAN, &dp->qos.lifespan), s, p);
	if (dp->qos.user_data)
		SCHK (pid_add (p, PID_USER_DATA, dp->qos.user_data), s, p);
	SCHK (pid_add (p, PID_OWNERSHIP, &dp->qos.ownership), s, p);
	SCHK (pid_add (p, PID_OWNERSHIP_STRENGTH, &dp->qos.ownership_strength), s, p);
	SCHK (pid_add (p, PID_DESTINATION_ORDER, &dp->qos.destination_order), s, p);
	SCHK (pid_add (p, PID_PRESENTATION, &dp->qos.presentation), s, p);
	if (dp->qos.partition)
		SCHK (pid_add (p, PID_PARTITION, dp->qos.partition), s, p);
	if (dp->qos.topic_data)
		SCHK (pid_add (p, PID_TOPIC_DATA, dp->qos.topic_data), s, p);
	if (dp->qos.group_data)
		SCHK (pid_add (p, PID_GROUP_DATA, dp->qos.group_data), s, p);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (dp->tags)
		SCHK (pid_add (p, PID_DATA_TAGS, dp->tags), s, p);
#endif
#ifdef DDS_TYPECODE
	if (dp->typecode) {
		SCHK (pid_add (p, PID_VENDOR_ID, dp->vendor_id), s, p);
		SCHK (pid_add (p, PID_V_TYPECODE, dp->typecode), s, p);
	}
#endif
	SCHK (pid_finish (p), s, p);
	return (p - sp);
}

/* pid_add_topic_data -- Adds all Parameter Ids as needed for the data field of
			 the builtin SPDPbuiltinTopicReader/Writer endpoints. */

ssize_t pid_add_topic_data (unsigned char *p, const DiscoveredTopicData *dp)
{
	ssize_t		s;
	unsigned char	*sp;

	sp = p;
	SCHK (pid_add (p, PID_TOPIC_NAME, dp->name), s, p);
	SCHK (pid_add (p, PID_TYPE_NAME, dp->type_name), s, p);
	SCHK (pid_add (p, PID_DURABILITY, &dp->qos.durability), s, p);
	SCHK (pid_add (p, PID_DURABILITY_SERVICE, &dp->qos.durability_service), s, p);
	SCHK (pid_add (p, PID_DEADLINE, &dp->qos.deadline), s, p);
	SCHK (pid_add (p, PID_LATENCY_BUDGET, &dp->qos.latency_budget), s, p);
	SCHK (pid_add (p, PID_LIVELINESS, &dp->qos.liveliness), s, p);
	SCHK (pid_add (p, PID_RELIABILITY, &dp->qos.reliability), s, p);
	SCHK (pid_add (p, PID_TRANSPORT_PRIORITY, &dp->qos.transport_priority), s, p);
	SCHK (pid_add (p, PID_LIFESPAN, &dp->qos.lifespan), s, p);
	SCHK (pid_add (p, PID_DESTINATION_ORDER, &dp->qos.destination_order), s, p);
	SCHK (pid_add (p, PID_HISTORY, &dp->qos.history), s, p);
	SCHK (pid_add (p, PID_RESOURCE_LIMITS, &dp->qos.resource_limits), s, p);
	SCHK (pid_add (p, PID_OWNERSHIP, &dp->qos.ownership), s, p);
	if (dp->qos.topic_data)
		SCHK (pid_add (p, PID_TOPIC_DATA, dp->qos.topic_data), s, p);
#ifdef DDS_TYPECODE
	if (dp->typecode) {
		SCHK (pid_add (p, PID_VENDOR_ID, dp->vendor_id), s, p);
		SCHK (pid_add (p, PID_V_TYPECODE, dp->typecode), s, p);
	}
#endif
	SCHK (pid_finish (p), s, p);
	return (p - sp);
}

