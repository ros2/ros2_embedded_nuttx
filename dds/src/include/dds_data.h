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

/* dds_data.h -- Defines common data types as used in the DDS middleware. */

#ifndef __dds_data_h_
#define	__dds_data_h_

#include "sys.h"
#include "thread.h"
#include "skiplist.h"
#include "timer.h"
#include "uqos.h"
#include "guid.h"
#include "locator.h"
#include "cache.h"

/*#define RW_LOCKS	** Define this for per reader/writer locks. */
#define RW_TOPIC_LOCK	/* Define this to use the Topic lock i.o. the Publisher/
			   Subcriber lock. */

typedef enum {
	EPB_PARTICIPANT_W,		/* Participant Announcer. */
	EPB_PARTICIPANT_R,		/* Participant Detector. */
	EPB_PUBLICATION_W,		/* Publication Announcer. */
	EPB_PUBLICATION_R,		/* Publication Detector. */
	EPB_SUBSCRIPTION_W,		/* Subscription Announcer. */
	EPB_SUBSCRIPTION_R,		/* Subscription Detector. */
	EPB_PARTICIPANT_PROXY_W,	/* Participant Proxy Announcer. */
	EPB_PARTICIPANT_PROXY_R,	/* Participant Proxy Detector. */
	EPB_PARTICIPANT_STATE_W,	/* Participant State Announcer. */
	EPB_PARTICIPANT_STATE_R,	/* Participant State Detector. */
	EPB_PARTICIPANT_MSG_W,		/* Participant Message Data Writer. */
	EPB_PARTICIPANT_MSG_R,		/* Participant Message Data Reader. */

	/* Guesswork: .. needs to be verified against commercial DDS'es. */
	EPB_TOPIC_W,			/* Topic Writer. */
	EPB_TOPIC_R,			/* Topic Reader. */

#ifdef DDS_NATIVE_SECURITY

	/* Interparticipant Endpoints. */
	EPB_PARTICIPANT_SL_W,		/* Participant Stateless message Writer. */
	EPB_PARTICIPANT_SL_R,		/* Participant Stateless message Reader. */
	EPB_PUBLICATION_SEC_W,		/* Publication Secure Announcer. */
	EPB_PUBLICATION_SEC_R,		/* Publication Secure Detector. */
	EPB_SUBSCRIPTION_SEC_W,		/* Subscription Secure Announcer. */
	EPB_SUBSCRIPTION_SEC_R,		/* Subscription Secure Detector. */
	EPB_PARTICIPANT_MSG_SEC_W,	/* Participant Message Data Secure Writer. */
	EPB_PARTICIPANT_MSG_SEC_R,	/* Participant Message Data Secure Reader. */
	EPB_PARTICIPANT_VOL_SEC_W,	/* Participant Volatile Secure Writer. */
	EPB_PARTICIPANT_VOL_SEC_R,	/* Participant Volatile Secure Reader. */
#endif
#ifdef DDS_QEO_TYPES

	/* Builtin Qeo Endpoints for policy distribution. */
	EPB_POLICY_UPDATER_SEC_W,       /* Policy Updater Secure Writer. */
	EPB_POLICY_UPDATER_SEC_R,       /* Policy Updater Secure Reader. */
#endif

	EPB_MAX
} BUILTIN_INDEX;

#define	MAX_BUILTINS	(unsigned) EPB_MAX

typedef unsigned char ProtocolVersion_t [2];

#define	PROTOCOLVERSION		PROTOCOLVERSION_2_1
#define	PROTOCOLVERSION_1_0	{ 1, 0 }
#define	PROTOCOLVERSION_1_1	{ 1, 1 }
#define	PROTOCOLVERSION_2_0	{ 2, 0 }
#define	PROTOCOLVERSION_2_1	{ 2, 1 }

#define version_compare(v1,v2)	((v1 [0] << 8) | v1 [1]) - ((v2 [0] << 8) | v2 [1])
/*#define version_set(v1,v2)	*((unsigned short *) v1) = *((unsigned short *) v2) */
#define	version_set(v1,v2)	v1 [0] = v2 [0]; v1 [1] = v2 [1]
#define	version_init(v1)	v1 [0] = 2; v1 [1] = 1

typedef unsigned char VendorId_t [2];

#define	VENDORID_H_TECHNICOLOR	1
#define	VENDORID_L_TECHNICOLOR	0x0e

#define	VENDORID_UNKNOWN	{ 0, 0 }
#define	VENDORID_TECHNICOLOR	{ VENDORID_H_TECHNICOLOR, VENDORID_L_TECHNICOLOR }

/*#define vendor_id_set(v1,v2)	*((unsigned short *) v1) = *((unsigned short *) v2)*/
#define	vendor_id_set(v1,v2)	memcpy (v1, v2, sizeof (VendorId_t))
#define	vendor_id_init(v)	v [0] = VENDORID_H_TECHNICOLOR; v [1] = VENDORID_L_TECHNICOLOR

typedef uint32_t Count_t;

typedef struct DDS_DomainParticipant_st Domain_t;
typedef struct endpoint_st Endpoint_t;
typedef struct local_endpoint_st LocalEndpoint_t;
typedef struct DDS_Subscriber_st Subscriber_t;
typedef struct DDS_Publisher_st Publisher_t;
typedef struct DDS_DataWriter_st Writer_t;
typedef struct DDS_DataReader_st Reader_t;
typedef struct discovered_writer_st DiscoveredWriter_t;
typedef struct discovered_reader_st DiscoveredReader_t;


/* Entity type. */
/* ------------ */

/* Every publically accessible object within a DDS domain is derived from the
   Entity type. This type contains the following data fields:

	1. Various flags (see EF_* below).
	2. A type field to distinguish between different entities.
	2. A handle that can be used to lookup the entity.
	3. A pointer to the parent of the object.

	Note: following parent/child relationships can be distinguished:

		Domain > Topic
		Domain > Local Participant > Subscriber > Local DataReader
		Domain > Local Participant > Publisher > Local DataWriter
		Domain > Remote Participant > Remote DataReader
		Domain > Remote Participant > Remote DataWriter
*/

/* Entity flags/handle bits: */
#define	EF_ENABLED	0x0001		/* Entity is locally enabled. */
#define	EF_NOT_IGNORED	0x0002		/* Remote not ignored entity. */
#define	EF_BUILTIN	0x0004		/* Builtin entity. */
#define	EF_LOCAL	0x0008		/* Locally created. */
#define	EF_REMOTE	0x0010		/* Remote entity. */
#define	EF_INLINE_QOS	0x0020		/* Expects QoS in data. */
#define	EF_FILTERED	0x0040		/* Content-filtered Topic. */
#define	EF_LOCAL_COMP	0x0040		/* Local component (CDD). */
#define	EF_MATCHED   	0x0040		/* Used by CDD to see if matching changed. */
#define	EF_INC_TYPE   	0x0040		/* Incompatible learned type. */
#define	EF_SUSPEND	0x0080		/* Publisher suspend. */
#define	EF_PUBLISH	0x0080		/* Publication suspended. */
#define	EF_ALIVE	0x0100		/* Set when data passes. */
#define	EF_LNOTIFY	0x0200		/* Notify liveliness. */
#define EF_NEW		0x0400		/* New while suspended. */
#define EF_SHUTDOWN	0x0800		/* Entity closing down. */
#define EF_CACHED  	0x1000		/* Entity in builtin cache. */

typedef enum {
	ET_UNKNOWN,
	ET_PARTICIPANT,		/* DomainParticipant. */
	ET_TOPIC,		/* Topic. */
	ET_PUBLISHER,		/* Publisher. */
	ET_SUBSCRIBER,		/* Subscriber. */
	ET_WRITER,		/* DataWriter. */
	ET_READER		/* DataReader. */
} EntityType_t;

/* Entity structure - note that these fields are typically embedded directly
   in the specific entity definitions. */
typedef struct DDS_Entity_st {
	unsigned	flags:13;	/* Entity flags. */
	unsigned	type:3;		/* Type of entity. */
#if WORDSIZE == 64 || defined (BIGDATA)
	unsigned	handle;		/* Entity handle. */
#else
	unsigned	handle:16;	/* Entity handle. */
#endif
} Entity_t;


#define	entity_type(e)	((e)->type)

/* Retrieve the type of an entity from the entity field. */

#define	entity_writer(t) ((t) == ET_WRITER)

/* Verify if the entity is a writer. */

#define	entity_reader(t) ((t) == ET_READER)

/* Verify if the entity is a reader. */

#define	entity_handle(e) (handle_t) ((e)->handle)

/* Retrieve the handle of an entity (always positive: 1..n). */

#define	entity_flags(e)	((e)->flags)

/* Retrieve the entity flags of an entity. */

#define	entity_active(f) (((f) & (EF_ENABLED | EF_NOT_IGNORED)) != 0)

/* Check if entity is active. */

#define	entity_ignored(f) (((f) & EF_NOT_IGNORED) == 0)

/* Check if the entity is in ignored state. */

#define entity_shutting_down(f) (((f) & EF_SHUTDOWN) != 0)

/* Check if the entity is shutting down. */

#define	entity_discovered(f) (((f) & EF_REMOTE) != 0)

/* Check if the entity was discovered. */

#define	entity_cached(f) (((f) & EF_CACHED) != 0)

/* Check if the entity was put in a builtin cache. */

#define	entity_local(f) (((f) & EF_LOCAL) != 0)

/* Check if the entity is local. */


/* Guard types. */
/* ------------ */

typedef enum {
	GT_LIVELINESS,	/* Liveliness check guard. */
	GT_DEADLINE,	/* Deadline check. */
	GT_LIFESPAN,	/* Lifespan checks required. */
	GT_AUTOP_NW,	/* Autopurge no-writer checks required. */
	GT_AUTOP_DISP	/* Autopurge disposed checks required. */
} GuardType_t;

typedef enum {
	GM_NONE,	/* Timer inactive. */
	GM_ONE_SHOT,	/* Timer removed when done. */
	GM_PERIODIC,	/* Timer restarts automatically. */
	GM_PROGRESSIVE,	/* Timer progresses to end of chain. */
	GM_MIXED	/* Periodic + progressive on time-out. */
} GuardMode_t;

/* Guard context: */
typedef struct guard_st Guard_t;
struct guard_st {
	Guard_t		*pnext;			/* Participant Guard list. */
	Guard_t		*enext;			/* Endpoint guards list. */
	unsigned	type:3;			/* Guard type. */
	unsigned	kind:2;			/* Guard kind. */
	unsigned	writer:1;		/* Writer guard. */
	unsigned	mode:3;			/* Defined timer mode. */
	unsigned	cmode:3;		/* Actual timer mode. */
	unsigned	alive:1;		/* Guard still alive. */
	unsigned	critical:1;		/* Critical state. */
	unsigned	mark:1;			/* Marker while processing. */
	unsigned	period;			/* Guard timeout. */
	FTime_t		time;			/* Time of last sample. */
	Endpoint_t	*wep;			/* Writer endpoint. */
	Endpoint_t	*rep;			/* Reader endpoint. */
	Timer_t		*timer;			/* Guard timer. */
};

/* Property list node: */
typedef struct property_st Property_t;
struct property_st {
	String_t	*name;
	String_t	*value;
	Property_t	*next;
};

#ifdef DDS_SECURITY

typedef unsigned Identity_t;
typedef unsigned Permissions_t;

#ifdef DDS_NATIVE_SECURITY
typedef struct DDS_TokenRef_st Token_t;

typedef enum {
	AS_OK,
	AS_FAILED,
	AS_PENDING_RETRY,
	AS_PENDING_HANDSHAKE_REQ,
	AS_PENDING_HANDSHAKE_MSG,
	AS_OK_FINAL_MSG,
	AS_PENDING_CHALLENGE_MSG
} AuthState_t;

#else
typedef String_t	Token_t;
#endif
#endif

/* Participant type. */
/* ----------------- */

#define	SECC_NONE	0		/* No security capabilities. */
#define	SECC_DTLS_UDP	1		/* DTLS/UDP capability. */
#define	SECC_TLS_TCP	2		/* TLS/TCP capability. */
#define	SECC_DDS_SEC	4		/* DDS fine-grained security. */

#define	SECC_LOCAL	16		/* Local capability set. */
#define	SECC_REMOTE	0		/* Remote capability set. */

#define	FWD_UDP_TO_TCP	1		/* May forward from UDP to TCP. */
#define	FWD_TCP_TO_UDP	2		/* May forward from TCP to UDP. */
#define	FWD_TCP_TO_TCP	4		/* May forward from TCP to TCP. */
#define	FWD_BRIDGE	(FWD_UDP_TO_TCP | FWD_TCP_TO_UDP | FWD_TCP_TO_TCP)
					/* DDS-bridge function. */
#define	FWD_ROUTER	8		/* Contains a DDS-router function. */

typedef struct participant_proxy_st {
	GuidPrefix_t	    guid_prefix;	/* Common GUID prefix. */
	ProtocolVersion_t   proto_version;	/* Protocol version. */
	VendorId_t	    vendor_id;		/* Vendor Id. */
	int		    exp_il_qos;		/* Expect Inline QoS. */
	int		    no_mcast;		/* Don't use Multicast. */
	uint32_t	    sw_version;		/* Software version number. */
	uint32_t	    builtins;		/* Supported builtins. */
#ifdef DDS_SECURITY
	Identity_t	    id;			/* Identity handle. */
	Permissions_t	    permissions;	/* Permissions handle. */
	uint32_t	    sec_caps;		/* Security capabilities. */
	LocatorList_t	    sec_locs;		/* Security locators. */
#endif
	LocatorList_t	    def_ucast;		/* Default unicasts. */
	LocatorList_t	    def_mcast;		/* Default multicasts. */
	LocatorList_t	    meta_ucast;		/* Meta unicasts. */
	LocatorList_t	    meta_mcast;		/* Meta multicasts. */
	Count_t		    manual_liveliness;	/* Forces send when ++.*/
	unsigned	    forward;		/* Forward capabilities. */
} ParticipantProxy;

/* Participant entity: */
typedef struct participant_st {

	/* Entity header. */
	Entity_t	 p_entity;		/* Flags/type/handle. */
#define	p_flags		 p_entity.flags		/* Flags. */
#define	p_type		 p_entity.type		/* Type. */
#define	p_handle	 p_entity.handle	/* Handle. */
	Domain_t	 *p_domain;		/* Parent Domain. */

	/* Participant-specific data. */
#ifdef DDS_NATIVE_SECURITY
	Token_t	  	 *p_id_tokens;		/* Identity token list. */
	Token_t	  	 *p_p_tokens;		/* Permissions token list. */
	AuthState_t	 p_auth_state;		/* Authentication state. */
	unsigned	 p_crypto;		/* Participant crypto handle. */
#endif
	ParticipantProxy p_proxy;		/* Proxy info. */
#define	p_guid_prefix	 p_proxy.guid_prefix	/* Common GUID prefix.*/
#define	p_proto_version	 p_proxy.proto_version	/* RTPS protocol version. */
#define	p_vendor_id	 p_proxy.vendor_id	/* Vendor Id. */
#define	p_exp_il_qos	 p_proxy.exp_il_qos	/* Inline-QoS expected. */
#define	p_no_mcast	 p_proxy.no_mcast	/* Don't use Multicast. */
#define	p_sw_version	 p_proxy.sw_version	/* TDDS version. */
#define	p_builtins	 p_proxy.builtins	/* Builtin endpoints. */
#define	p_id		 p_proxy.id		/* Identity. */
#define	p_permissions	 p_proxy.permissions	/* Permissions. */
#define	p_sec_caps	 p_proxy.sec_caps	/* Security capabilities. */
#define	p_sec_locs	 p_proxy.sec_locs	/* Security locators. */
#define	p_forward	 p_proxy.forward	/* Forwarding capabilities. */
#define	p_def_ucast	 p_proxy.def_ucast	/* Default unicasts. */
#define	p_def_mcast	 p_proxy.def_mcast	/* Default multicasts. */
#define	p_meta_ucast	 p_proxy.meta_ucast	/* Meta unicasts. */
#define	p_meta_mcast	 p_proxy.meta_mcast	/* Meta multicasts. */
#define	p_man_liveliness p_proxy.manual_liveliness /* Manual liveliness. */
	String_t	 *p_user_data;		/* User Data QoS. */
	String_t	 *p_entity_name;	/* Entity name. */
	Duration_t	 p_lease_duration;	/* Timeout to remove. */
	Skiplist_t	 p_endpoints;		/* Endpoints [EntityId_t]. */
	Skiplist_t	 p_topics;		/* Topics created/discovered. */
	Timer_t		 p_timer;		/* Timeout/announce timer. */
	Guard_t	 	 *p_liveliness;		/* Liveliness endpoints list. */
	LocatorList_t	 p_src_locators;	/* Source locators. */
	Property_t	 *p_properties;		/* Properties. */
	Endpoint_t	 *p_builtin_ep [MAX_BUILTINS]; /* Builtin endpoints. */
	int		 p_alive;		/* Last alive time. */
	Ticks_t		 p_local;		/* Local source timestamp. */
} Participant_t;


/* Topic types. */
/* ------------ */

typedef struct topic_type_st {
	unsigned	flags;			/* Various flags. */
	unsigned	index;			/* Unique index. */
	unsigned	nrefs;			/* # of references. */
	unsigned	nlrefs;			/* # of local references. */
	String_t	*type_name;		/* Name of type. */
	TypeSupport_t	*type_support;		/* Typecode. */
} TopicType_t;

typedef struct DDS_Topic_st Topic_t;
typedef struct DDS_ContentFilteredTopic_st FilteredTopic_t;

struct DDS_Topic_st {

	/* Entity header. */
	Entity_t	entity;			/* Flags/type/handle. */
	Domain_t	*domain;		/* Parent Domain. */

	/* Topic-specific data. */
	unsigned short	nlrefs;			/* # of local create/find(). */
	unsigned short	nrrefs;			/* # of discovered. */
	String_t	*name;			/* Name of topic. */
	TopicType_t	*type;			/* Topic type info. */
	Qos_t		*qos;			/* Topic QoS pars. */
	Endpoint_t	*writers;		/* Writers list. */
	Endpoint_t	*readers;		/* Readers list. */
	lock_t		lock;			/* Topic lock. */
	FilteredTopic_t	*filters;		/* Filtered topic readers. */

	/* DCPS-specific data. */
	unsigned short	status;			/* DDS status bits. */
	unsigned short	mask;			/* DDS status mask. */
	void		*condition;		/* Status condition. */
	DDS_TopicListener listener;		/* Topic listener data. */
	DDS_InconsistentTopicStatus inc_status;	/* Inconsistent Topic status. */
};

typedef struct content_filter_st {
	String_t	*name;			/* ContentFilteredTopic name. */
	String_t	*related_name;		/* Real Topic name. */
	String_t	*class_name;		/* Filter class name. */
	String_t	*expression;		/* Filter expression. */
	Strings_t	*expression_pars;	/* Filter parameters. */
} ContentFilter_t;

typedef struct filter_data_st {
	ContentFilter_t	filter;			/* Filter specification. */
	BCProgram	program;		/* Filter bytecodes. */
	void		*cache;			/* Interpreter cache. */
} FilterData_t;

struct DDS_ContentFilteredTopic_st {
	Topic_t		topic;			/* Basic topic info. */
	Topic_t		*related;		/* Related topic. */
	FilteredTopic_t	*next;			/* Next in list of filtered. */
	FilterData_t	data;			/* Filter data. */
};


/* Publisher/subscriber types (own participant only). */
/* -------------------------------------------------- */

struct DDS_Publisher_st {

	/* Entity header. */
	Entity_t		entity;		/* Flags/type/handle. */
	Domain_t		*domain;	/* Parent Domain. */

	/* Publisher-specific data. */
	Publisher_t		*next;		/* Next in Publisher list. */
	Publisher_t		*prev;		/* Previous in Publisher List.*/
	unsigned short		nwriters;	/* # of writers. */

	/* DCPS-specific data. */
	unsigned short	 	mask;		/* DDS mask. */
	GroupQos_t		qos;		/* Publisher QoS data. */
	void			*condition;	/* Status condition. */
	DDS_PublisherListener	listener;	/* Publisher Listener data. */
	DDS_DataWriterQos	def_writer_qos;	/* Default Writer QoS data. */
	Writer_t		*suspended;	/* Suspended publications. */
};

struct DDS_Subscriber_st {

	/* Entity header. */
	Entity_t		entity;		/* Flags/type/handle. */
	Domain_t		*domain;	/* Parent Domain. */

	/* Subscriber-specific data. */
	Subscriber_t		*next;		/* Next in Subscriber list. */
	Subscriber_t		*prev;		/* Previous in Subscriber list*/
	unsigned		nreaders;	/* # of readers. */

	/* DCPS-specific data. */
	GroupQos_t		qos;		/* Subscriber QoS data. */
	unsigned short		status;		/* DDS status bits. */
	unsigned short		mask;		/* DDS status mask. */
	void			*condition;	/* Status condition. */
	DDS_SubscriberListener	listener;	/* Subscriber Listener data. */
	DDS_DataReaderQos	def_reader_qos;	/* Default Reader QoS data. */
};


/* Endpoint types. */
/* --------------- */

struct endpoint_st {

	/* Entity header. */
	Entity_t	entity;			/* Flags/type/handle. */
	union {
	 Participant_t	*participant;		/* Participant (discovered). */
	 Publisher_t	*publisher;		/* Publisher (local writer). */
	 Subscriber_t	*subscriber;		/* Subscriber (local reader). */
	}		u;

	/* Endpoint-specific data. */
	EntityId_t	entity_id;		/* Entity Id. */
	Topic_t		*topic;			/* Topic pointer. */
	Qos_t		*qos;			/* Reader/Writer QoS pars. */
	LocatorList_t	ucast;			/* Unicast Locator list. */
	LocatorList_t	mcast;			/* Multicast Locator list. */
	Endpoint_t	*next;			/* Topic endpoints chain. */
	void		*rtps;			/* RTPS info. */
};

#define	TC_IS_TS	((unsigned char *) ~0UL)	/* Typecode == Type */

struct discovered_writer_st {
	Endpoint_t	dw_ep;			/* Endpoint data. */
#define	dw_flags	dw_ep.entity.flags	/* Flags. */
#define dw_type		dw_ep.entity.type	/* Type. */
#define dw_handle	dw_ep.entity.handle	/* Handle. */
#define	dw_participant	dw_ep.u.participant	/* Participant. */
#define	dw_entity_id	dw_ep.entity_id		/* Entity Id. */
#define dw_topic	dw_ep.topic		/* Topic pointer. */
#define dw_qos		dw_ep.qos		/* Writer QoS parameters. */
#define dw_ucast	dw_ep.ucast		/* Unicast Locator list. */
#define dw_mcast	dw_ep.mcast		/* Multicast Locator list. */
#define dw_next		dw_ep.next		/* Topic endpoints chain. */
#define	dw_rtps		dw_ep.rtps		/* RTPS.RemoteWriter_t * */
#ifdef DDS_TYPECODE
	unsigned char	*dw_tc;			/* Discovered Typecode. */
#endif
};

struct discovered_reader_st {
	Endpoint_t	dr_ep;			/* Endpoint data. */
#define	dr_flags	dr_ep.entity.flags	/* Flags. */
#define dr_type		dr_ep.entity.type	/* Type. */
#define dr_handle	dr_ep.entity.handle	/* Handle. */
#define	dr_participant	dr_ep.u.participant	/* Participant. */
#define	dr_entity_id	dr_ep.entity_id		/* Entity Id. */
#define dr_topic	dr_ep.topic		/* Topic pointer. */
#define dr_qos		dr_ep.qos		/* Reader QoS parameters. */
#define dr_ucast	dr_ep.ucast		/* Unicast Locator list. */
#define dr_mcast	dr_ep.mcast		/* Multicast Locator list. */
#define dr_next		dr_ep.next		/* Topic endpoints chain. */
#define	dr_rtps		dr_ep.rtps		/* RTPS.RemoteReader_t * */
#ifdef DDS_TYPECODE
	unsigned char	*dr_tc;			/* Discovered Typecode. */
#endif
	DDS_TimeBasedFilterQosPolicy dr_time_based_filter;
	FilterData_t	*dr_content_filter;	/* Content filter data. */
};

struct local_endpoint_st {

	/* Entity and common endpoint info. */
	Endpoint_t	ep;

	/* Local endpoint specific data. */
	Cache_t		cache;			/* Reader/writer cache. */
	unsigned short	status;			/* DDS status bits. */
	unsigned short	mask;			/* DDS status mask. */
	void		*conditions;		/* Status/Read conditions. */
	Guard_t		*guard;			/* Endpoint guards list. */
#ifdef DDS_NATIVE_SECURITY
	unsigned	access_prot:1;		/* Use access control. */
	unsigned	disc_prot:1;		/* Use secure discovery. */
	unsigned	submsg_prot:1;		/* Encrypt submessages. */
	unsigned	payload_prot:1;		/* Encrypt payload. */
	unsigned	crypto_type:4;		/* Encryption method. */
	unsigned	crypto;			/* Local crypto handle. */
#endif
#ifdef RW_LOCKS
	lock_t		lock;			/* Reader/writer lock. */
#endif
};

/* Use the ENC_DATA() macro to check for encrypted payload data. */
#ifdef DDS_NATIVE_SECURITY
#define	ENC_DATA(lep)	((lep)->payload_prot && (lep)->crypto_type >= DDS_CRYPT_AES128_HMAC_SHA1)
#else
#define	ENC_DATA(lep)	0
#endif

typedef struct inc_qos_st {
	int		   total_count;
	int		   total_count_change;
	DDS_QosPolicyId_t  last_policy_id;
	DDS_QosPolicyCount *policies;
	unsigned 	   n_policies;
} IncompatibleQosStatus;

typedef struct sample_rej_st {
	int                total_count;
	int                total_count_change;
	unsigned	   last_reason: 2; 
	unsigned	   last_instance_handle: 30;
} SampleRejectedStatus;

struct DDS_DataReader_st {

	/* Entity and Endpoint-specific data. */
	LocalEndpoint_t	r_lep;			/* Local Endpoint data. */
#define	r_ep		r_lep.ep		/* Common Endpoint data. */
#define	r_flags		r_lep.ep.entity.flags	/* Flags. */
#define r_type		r_lep.ep.entity.type	/* Type. */
#define r_handle	r_lep.ep.entity.handle	/* Handle. */
#define	r_subscriber	r_lep.ep.u.subscriber	/* Subscriber. */
#define	r_entity_id	r_lep.ep.entity_id	/* Entity Id. */
#define r_topic		r_lep.ep.topic		/* Topic pointer. */
#define r_qos		r_lep.ep.qos		/* Reader QoS parameters. */
#define r_ucast		r_lep.ep.ucast		/* Unicast Locator list. */
#define r_mcast		r_lep.ep.mcast		/* Multicast Locator list. */
#define r_next		r_lep.ep.next		/* Topic endpoints chain. */
#define	r_rtps		r_lep.ep.rtps		/* RTPS.READER * */

	/* DCPS-specific data. */
#define	r_cache		r_lep.cache		/* History Cache. */
#define	r_status	r_lep.status		/* DDS Status bits. */
#define	r_mask		r_lep.mask		/* DDS Mask bits. */
#define r_conditions	r_lep.conditions	/* Status conditions. */
#define	r_guard		r_lep.guard		/* Reader guards. */
#define	r_access_prot	r_lep.access_prot	/* Use access control. */
#define	r_disc_prot	r_lep.disc_prot		/* Use secure discovery. */
#define	r_submsg_prot	r_lep.submsg_prot	/* Encrypt submessages. */
#define	r_payload_prot	r_lep.payload_prot	/* Encrypt payload. */
#define	r_crypto_type	r_lep.crypto_type	/* Encryption type. */
#define	r_crypto	r_lep.crypto		/* Reader crypto handle. */
#ifdef RW_LOCKS
#define r_lock		r_lep.lock		/* Use endpoint lock. */
#elif defined (RW_TOPIC_LOCK)
#define	r_lock		r_topic->lock		/* Use topic lock. */
#else
#define r_lock		r_subscriber->lock	/* Use subscriber lock. */
#endif
	DDS_TimeBasedFilterQosPolicy       r_time_based_filter; /* Timed-filter.*/
	DDS_ReaderDataLifecycleQosPolicy   r_data_lifecycle; /* Data Lifecycle. */
	DDS_DataReaderListener             r_listener;	/* Reader Listener data.*/
	DDS_RequestedDeadlineMissedStatus  r_rdm_status;
	IncompatibleQosStatus              r_riq_status;
	DDS_SampleLostStatus		   r_sl_status;
	SampleRejectedStatus	   	   r_sr_status;
	DDS_LivelinessChangedStatus	   r_lc_status;
	DDS_SubscriptionMatchedStatus	   r_sm_status;

	Changes_t	r_changes;		/* Rx samples buffer. */
	DDS_SampleInfo	**r_prev_info;		/* Previously used SampleInfos. */
	void		*r_prev_data;		/* Previously used ReceivedData.*/
	unsigned	r_n_prev;		/* # of previously used samples.*/
};

struct DDS_DataWriter_st {

	/* Entity and Endpoint-specific data. */
	LocalEndpoint_t	w_lep;			/* Endpoint data. */
#define	w_ep		w_lep.ep		/* Common Endpoint data. */
#define	w_flags		w_lep.ep.entity.flags	/* Flags. */
#define w_type		w_lep.ep.entity.type	/* Type. */
#define w_handle	w_lep.ep.entity.handle	/* Handle. */
#define	w_fh		w_lep.ep.fh		/* Flags/Type/Handle. */
#define	w_publisher	w_lep.ep.u.publisher	/* Publisher. */
#define	w_entity_id	w_lep.ep.entity_id	/* Entity Id. */
#define w_topic		w_lep.ep.topic		/* Topic pointer. */
#define w_qos		w_lep.ep.qos		/* Reader QoS parameters. */
#define w_ucast		w_lep.ep.ucast		/* Unicast Locator list. */
#define w_mcast		w_lep.ep.mcast		/* Multicast Locator list. */
#define w_next		w_lep.ep.next		/* Topic endpoints chain. */
#define	w_rtps		w_lep.ep.rtps		/* RTPS.WRITER * */

	/* DCPS-specific data. */
#define	w_cache		w_lep.cache		/* History Cache. */
#define	w_status	w_lep.status		/* DDS Status bits. */
#define	w_mask		w_lep.mask		/* DDS Mask bits. */
#define	w_condition	w_lep.conditions	/* Status condition. */
#define	w_guard		w_lep.guard		/* Writer guards. */
#define	w_access_prot	w_lep.access_prot	/* Use access control. */
#define	w_disc_prot	w_lep.disc_prot		/* Use secure discovery. */
#define	w_submsg_prot	w_lep.submsg_prot	/* Encrypt submessages. */
#define	w_payload_prot	w_lep.payload_prot	/* Encrypt payload. */
#define	w_crypto_type	w_lep.crypto_type	/* Encryption type. */
#define	w_crypto	w_lep.crypto		/* Writer crypto handle. */
#ifdef RW_LOCKS
#define w_lock		w_lep.lock		/* Use endpoint lock. */
#elif defined (RW_TOPIC_LOCK)
#define	w_lock		w_topic->lock		/* Use topic lock. */
#else
#define w_lock		w_publisher->lock	/* Use publisher lock. */
#endif
	DDS_DataWriterListener 		 w_listener; /* Writer Listener data. */
	DDS_OfferedDeadlineMissedStatus	 w_odm_status;
	IncompatibleQosStatus            w_oiq_status;
	DDS_LivelinessLostStatus	 w_ll_status;
	DDS_PublicationMatchedStatus	 w_pm_status;
	Writer_t	*w_next_s;		/* Next suspended publication.*/
};


/* Domain type. */
/* ------------ */

typedef struct publisher_list_st {
	Publisher_t	*head;			/* First Publisher. */
	Publisher_t	*tail;			/* Last Publisher. */
} PublisherList_t;

typedef struct subscriber_list_st {
	Subscriber_t	*head;			/* First Subscriber. */
	Subscriber_t	*tail;			/* Last Subscriber. */
} SubscriberList_t;

typedef struct topic_wait_st TopicWait_t;
struct topic_wait_st {
	TopicWait_t	*next;
	cond_t		condition;
	const char	*name;
	Topic_t		*topic;
	unsigned	nthreads;
};

typedef struct rem_prefix_st RemPrefix_t;
struct rem_prefix_st {
	RemPrefix_t	*next;
	RemPrefix_t	*prev;
	GuidPrefix_t	prefix;
	LocatorList_t	locators;
};

typedef struct prefix_list_st {
	RemPrefix_t	*head;
	RemPrefix_t	*tail;
	unsigned	count;
} PrefixList_t;

struct DDS_DomainParticipant_st {
	Participant_t	  participant;		/* Local participant. */
	DomainId_t	  domain_id;		/* Domain Identifier. */
	unsigned	  participant_id;	/* Participant Identifier. */
	unsigned	  index;		/* Index. */
	Skiplist_t	  types;		/* Types [type_name]. */
	Skiplist_t	  peers;		/* Participants [GuidPrefix]. */
	PublisherList_t	  publishers;		/* Publishers list. */
	SubscriberList_t  subscribers;		/* Subscribers list. */
	Publisher_t	  *builtin_publisher;	/* Builtin Publisher. */
	Subscriber_t	  *builtin_subscriber;	/* Builtin Subscriber. */
	Reader_t	  *builtin_readers [4];	/* Builtin Readers. */
#ifdef THREADS_USED
	TopicWait_t	  *topic_wait;		/* Topic waiting threads. */
#endif
	lock_t		  lock;			/* Domain lock. */
	void		  *rtps;		/* RTPS Participant info. */
	/* DCPS-specificic data: */
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	unsigned char	  participant_key [16];	/* Participant key. */
#endif
	unsigned	  security:16;		/* Security level. */
#ifdef DDS_NATIVE_SECURITY
	unsigned	  access_protected:1;	/* Use validate_rem_perms(). */
	unsigned	  rtps_protected:4;	/* Encrypt all RTPS messages. */
#endif
#endif
	unsigned	  autoenable:1;		/* Auto-enable entities. */
	unsigned short	  mask;			/* Status Mask. */
	void		  *condition;		/* Status condition. */
	DDS_DomainParticipantListener listener;	/* Listener data. */
	DDS_TopicQos	  def_topic_qos;	/* Default Topic QoS data. */
	DDS_PublisherQos  def_publisher_qos;	/* Default Publisher QoS data.*/
	DDS_SubscriberQos def_subscriber_qos;	/* Default Subscriber QoS data*/

	/* Other subsystem domain data: */
#ifdef DDS_AUTO_LIVELINESS
	Timer_t		  auto_liveliness;	/* Default liveliness timer. */
#endif
	Duration_t	  resend_per;		/* Resend period. */
	LocatorKind_t	  kinds;		/* Supported locator types. */
	LocatorList_t	  dst_locs;		/* Destination locators. */
	PrefixList_t	  prefixes;		/* Participant prefix info. */
	Participant_t	  **relays;		/* Local relay nodes. */
	unsigned	  nr_relays;		/* Current # of relay nodes. */
	unsigned	  max_relays;		/* Maximum # of relay nodes. */
};

extern int dds_listener_state;

#endif /* !__dds_data_h_ */

