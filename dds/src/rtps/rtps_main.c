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

/* rtps_main.c -- Implementation of the RTPS-compliant DDS wire protocol. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
# include <assert.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#define INLINE inline
#endif
#include "sys.h"
#include "config.h"
#include "thread.h"
#include "atomic.h"
#include "pid.h"
#include "str.h"
#include "ipc.h"
#include "log.h"
#include "error.h"
#include "prof.h"
#include "ctrace.h"
#include "pool.h"
#include "list.h"
#include "set.h"
#include "skiplist.h"
#include "sock.h"
#include "timer.h"
#include "md5.h"
#include "locator.h"
#ifdef DDS_TCP
#include "ri_tcp.h"
#endif
#ifdef XTYPES_USED
#include "xcdr.h"
#else
#include "cdr.h"
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
#include "sec_crypto.h"
#endif
#include "pl_cdr.h"
#include "typecode.h"
#include "cache.h"
#include "dds.h"
#include "disc.h"
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"
#include "rtps_clist.h"
#include "rtps_data.h"
#include "rtps_mux.h"
#include "rtps_msg.h"
#include "rtps_priv.h"
#include "rtps_slbw.h"
#include "rtps_slbr.h"
#include "rtps_slrw.h"
#include "rtps_sfbw.h"
#include "rtps_sfbr.h"
#include "rtps_sfrw.h"
#include "rtps_sfrr.h"
#include "rtps_trace.h"
#include "rtps_check.h"
#ifdef DDS_DEBUG
#include "debug.h"
#endif

/*#define DUMP_LOCATORS */

static Duration_t rtps_def_heartbeat  = { DEF_HEARTBEAT_S,  DEF_HEARTBEAT_NS  };
static Duration_t rtps_def_nack_resp  = { DEF_NACK_RSP_S,   DEF_NACK_RSP_NS   };
static Duration_t rtps_def_nack_supp  = { DEF_NACK_SUPP_S,  DEF_NACK_SUPP_NS  };
static Duration_t rtps_def_resend_per = { DEF_RESEND_PER_S, DEF_RESEND_PER_NS };
static Duration_t rtps_def_hb_resp    = { DEF_HB_RSP_S,     DEF_HB_RSP_NS     };
static Duration_t rtps_def_hb_supp    = { DEF_HB_SUPP_S,    DEF_HB_SUPP_NS    };
static Duration_t rtps_def_lease_per  = { DEF_LEASE_PER_S,  DEF_LEASE_PER_NS  };

ProtocolId_t rtps_protocol_id = PROTOCOL_RTPS;
ProtocolVersion_t rtps_protocol_version = PROTOCOLVERSION;
VendorId_t rtps_vendor_id = VENDORID_TECHNICOLOR;

int rtps_used = 1;
int rtps_log;
#ifdef DDS_DEBUG
/* TODO: Remove this and all related code once dtls/tls are fully working */
int rtps_no_security = 0;
#endif

RECEIVER rtps_receiver;
TRANSMITTER rtps_transmitter;

size_t rtps_max_msg_size = DEF_RTPS_MSG_SIZE;
#ifdef RTPS_FRAGMENTS
size_t rtps_frag_size = DEF_RTPS_FRAGMENT_SIZE;
unsigned rtps_frag_burst = DEF_RTPS_FRAGMENT_BURST;
unsigned rtps_frag_delay = DEF_RTPS_FRAGMENT_DELAY;
#endif

typedef uint32_t BuiltinEndpointSet_t;

#define	DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER		0x00000001
#define	DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR		0x00000002
#define	DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER		0x00000004
#define	DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR		0x00000008
#define	DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER		0x00000010
#define	DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR		0x00000020
#define	DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_ANNOUNCER	0x00000040
#define	DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_DETECTOR	0x00000080
#define	DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_ANNOUNCER	0x00000100
#define	DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_DETECTOR	0x00000200
#define	BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER	0x00000400
#define	BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER	0x00000800

#define	SEQUENCENUMBER_UNKNOWN	-1

RR_EVENTS *rtps_rr_event [RR_MAX_TYPES] = {
	&slw_be_events,
	&slw_rel_events,
	&sfw_be_events,
	&sfw_rel_events
};

#if defined (DDS_DEBUG) || defined (RTPS_TRACE)

const char *rtps_rw_cstate_str [] = {
	"INITIAL", "READY", "FINAL"
};

const char *rtps_rw_astate_str [] = {
	"WAITING", "MAY_ACK", "MUST_ACK"
};

const char *rtps_rr_cstate_str [] = {
	"INITIAL", "READY", "FINAL"
};

const char *rtps_rr_tstate_str [] = {
	"IDLE", "PUSHING", "ANNOUNCING"
};

const char *rtps_rr_astate_str [] = {
	"WAITING", "MUST_REPAIR", "REPAIRING"
};

#endif

const EntityId_t rtps_builtin_eids [] = {
	{ ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER },
	{ ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER },
	{ ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER },
	{ ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER },
	{ ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER },
	{ ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER },
	{ { 0, } }, { { 0, } },
	{ { 0, } }, { { 0, } },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER },
	{ ENTITYID_SEDP_BUILTIN_TOPIC_WRITER },
	{ ENTITYID_SEDP_BUILTIN_TOPIC_READER }
#ifdef DDS_NATIVE_SECURITY
      ,	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_WRITER },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_READER },
	{ ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER },
	{ ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER },
	{ ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER },
	{ ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER },
	{ ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER }
#endif
#ifdef DDS_QEO_TYPES
      , { ENTITIYID_QEO_BUILTIN_POLICY_STATE_WRITER },
	{ ENTITIYID_QEO_BUILTIN_POLICY_STATE_READER }
#endif
};

const char *rtps_builtin_endpoint_names [] = {
	"SPDPbuiltinParticipantWriter",
	"SPDPbuiltinParticipantReader",
	"SEDPbuiltinPublicationsWriter",
	"SEDPbuiltinPublicationsReader",
	"SEDPbuiltinSubscriptionsWriter",
	"SEDPbuiltinSubscriptionsReader",
	NULL, NULL, /* Proxy */
	NULL, NULL, /* State */
	"BuiltinParticipantMessageWriter",
	"BuiltinParticipantMessageReader",
	"SEDPbuiltinTopicsWriter",
	"SEDPbuiltinTopicsReader"
#ifdef DDS_NATIVE_SECURITY
      ,	"BuiltinParticipantStatelessMessageWriter",
	"BuiltinParticipantStatelessMessageReader",
	"SEDPbuiltinPublicationsSecureWriter",
	"SEDPbuiltinPublicationsSecureReader",
	"SEDPbuiltinSubscriptionsSecureWriter",
	"SEDPbuiltinSubscriptionsSecureReader",
	"BuiltinParticipantMessageSecureWriter",
	"BuiltinParticipantMessageSecureReader",
	"BuiltinParticipantVolatileMessageSecureWriter",
	"BuiltinParticipantVolatileMessageSecureReader"
#endif
#ifdef DDS_QEO_TYPES
      , "QeobuiltinPolicyStateWriter",
	"QeobuiltinPolicyStateReader"
#endif
};

const char *rtps_builtin_topic_names [] = {
	"ParticipantBuiltinTopicData",
	"ParticipantBuiltinTopicData",
	"PublicationBuiltinTopicData",
	"PublicationBuiltinTopicData",
	"SubscriptionBuiltinTopicData",
	"SubscriptionBuiltinTopicData",
	NULL, NULL, /* Proxy */
	NULL, NULL, /* State */
	"ParticipantMessageData",
	"ParticipantMessageData",
	"TopicBuiltinTopicData",
	"TopicBuiltinTopicData"
#ifdef DDS_NATIVE_SECURITY
      ,	"ParticipantStatelessMessage",
	"ParticipantStatelessMessage",
	"PublicationBuiltinTopicDataSecure",
	"PublicationBuiltinTopicDataSecure",
	"SubscriptionBuiltinTopicDataSecure",
	"SubscriptionBuiltinTopicDataSecure",
	"ParticipantMessageData",
	"ParticipantMessageData",
	"ParticipantVolatileSecureMessage",
	"ParticipantVolatileSecureMessage"
#endif
#ifdef DDS_QEO_TYPES
      , "PolicyUpdaterMessageData",
	"PolicyUpdaterMessageData"
#endif
};

const char *mem_names [] = {
	"READER",
	"WRITER",
	"REM_READER",
	"REM_WRITER",
	"CCREF",
	"MSG_BUF",
	"MSG_ELEM_BUF",
	"MSG_REF",
};

MEM_DESC_ST	rtps_mem_blocks [MB_END];  /* Memory used by RTPS. */
size_t		rtps_mem_size;		/* Total memory allocated. */

#ifdef PROFILE
PUB_PROF_PID (rtps_w_create)
PUB_PROF_PID (rtps_w_delete)
PUB_PROF_PID (rtps_w_new)
PUB_PROF_PID (rtps_w_remove)
PUB_PROF_PID (rtps_w_urgent)
PUB_PROF_PID (rtps_w_write)
PUB_PROF_PID (rtps_w_dispose)
PUB_PROF_PID (rtps_w_unregister)
PUB_PROF_PID (rtps_w_rloc_add)
PUB_PROF_PID (rtps_w_rloc_rem)
PUB_PROF_PID (rtps_w_proxy_add)
PUB_PROF_PID (rtps_w_proxy_rem)
PUB_PROF_PID (rtps_w_resend)
PUB_PROF_PID (rtps_w_update)
PUB_PROF_PID (rtps_do_changes)
PUB_PROF_PID (rtps_send_msgs)
PUB_PROF_PID (rtps_r_create)
PUB_PROF_PID (rtps_r_delete)
PUB_PROF_PID (rtps_r_proxy_add)
PUB_PROF_PID (rtps_r_proxy_rem)
PUB_PROF_PID (rtps_r_unblock)
PUB_PROF_PID (rtps_rx_msgs)
PUB_PROF_PID (rtps_rx_data)
PUB_PROF_PID (rtps_rx_gap)
PUB_PROF_PID (rtps_rx_hbeat)
PUB_PROF_PID (rtps_rx_acknack)
PUB_PROF_PID (rtps_rx_inf_ts)
PUB_PROF_PID (rtps_rx_inf_rep)
PUB_PROF_PID (rtps_rx_inf_dst)
PUB_PROF_PID (rtps_rx_inf_src)
PUB_PROF_PID (rtps_rx_data_frag)
PUB_PROF_PID (rtps_rx_nack_frag)
PUB_PROF_PID (rtps_rx_hbeat_frag)
PUB_PROF_PID (rtps_tx_data)
PUB_PROF_PID (rtps_tx_gap)
PUB_PROF_PID (rtps_tx_hbeat)
PUB_PROF_PID (rtps_tx_acknack)
PUB_PROF_PID (rtps_tx_inf_ts)
PUB_PROF_PID (rtps_tx_inf_rep)
PUB_PROF_PID (rtps_tx_inf_dst)
PUB_PROF_PID (rtps_tx_inf_src)
PUB_PROF_PID (rtps_pw_start)
PUB_PROF_PID (rtps_pw_new)
PUB_PROF_PID (rtps_pw_send)
PUB_PROF_PID (rtps_pw_rem)
PUB_PROF_PID (rtps_pw_finish)
PUB_PROF_PID (rtps_bw_start)
PUB_PROF_PID (rtps_bw_new)
PUB_PROF_PID (rtps_bw_send)
PUB_PROF_PID (rtps_bw_rem)
PUB_PROF_PID (rtps_bw_finish)
PUB_PROF_PID (rtps_rw_start)
PUB_PROF_PID (rtps_rw_new)
PUB_PROF_PID (rtps_rw_send)
PUB_PROF_PID (rtps_rw_rem)
PUB_PROF_PID (rtps_rw_finish)
PUB_PROF_PID (rtps_rw_acknack)
PUB_PROF_PID (rtps_rw_hb_to)
PUB_PROF_PID (rtps_rw_alive_to)
PUB_PROF_PID (rtps_rw_nresp_to)
PUB_PROF_PID (rtps_br_start)
PUB_PROF_PID (rtps_br_data)
PUB_PROF_PID (rtps_br_finish)
PUB_PROF_PID (rtps_rr_start)
PUB_PROF_PID (rtps_rr_data)
PUB_PROF_PID (rtps_rr_finish)
PUB_PROF_PID (rtps_rr_gap)
PUB_PROF_PID (rtps_rr_hbeat)
PUB_PROF_PID (rtps_rr_alive_to)
PUB_PROF_PID (rtps_rr_do_ack)
PUB_PROF_PID (rtps_rr_proc)
#endif

#ifdef CTRACE_USED

const char *rtps_fct_str [] = {
	"writer_create", "writer_delete",
	"writer_new_change", "writer_delete_change",
	"writer_urgent_change", "writer_alive",
	"writer_write", "writer_dispose", "writer_unregister",
	"reader_locator_add", "reader_locator_remove",
	"matched_reader_add", "matched_reader_remove",
	"stateless_resend", "stateless_update",
	"sch_w_prepare", "sch_r_enqueue", "sch_loc_send", "sch_send_done",
	"reader_create", "reader_delete",
	"matched_writer_add", "matched_writer_remove",
	"reader_unblock",
	"receive",
	"rx_data", "rx_gap", "rx_heartbeat", "rx_acknack",
	"rx_info_ts", "rx_info_reply", "rx_info_dest", "rx_info_src",
	"rx_data_frag", "rx_nack_frag", "rx_heartbeat_frag",
	"tx_data", "tx_gap", "tx_heartbeat", "tx_acknack",
	"tx_info_ts", "tx_info_reply", "tx_info_dest", "tx_info_src",
	"tx_nack_frag", "tx_heartbeat_frag",
	"slw_be_start", "slw_be_new_change",
	"slw_be_send", "slw_be_remove_change", "slw_be_finish",
	"sfw_be_start", "sfw_be_new_change",
	"sfw_be_send", "sfw_be_remove_change", "sfw_be_finish",
	"sfw_rel_start", "sfw_rel_new_change",
	"sfw_rel_send", "sfw_rel_remove_change", "sfw_rel_finish",
	"sfw_rel_acknack", "sfw_rel_nackfrag",
	"sfw_heartbeat_to", "sfw_alive_to", "sfw_nack_rsp_to",
	"sfr_be_start", "sfr_be_data", "sfr_be_finish",
	"sfr_rel_start", "sfr_rel_data", "sfr_rel_finish",
	"sfr_rel_gap", "sfr_rel_heartbeat", "sfr_rel_hbfrag",
	"sfr_alive_to", "sfr_rel_do_ack",
	"sfr_process"
};

#endif
int rtps_seid_offsets [MAX_SEID_OFS + 1] = {
	-1, /*PAD*/-1, -1, -1,
	-1, -1, /*ACKNACK*/0, /*HEARTBEAT*/4,
	/*GAP*/4, /*INFO_TS*/-1, -1, -1,
	/*INFO_SRC*/-1, /*INFO_REPLYv4*/-1, /*INFO_DEST*/-1, /*INFO_REPLY*/-1,
	-1, -1, /*NACK_FRAG*/0, /*HB_FRAG*/4,
	-1, /*DATA*/8, /*DATA_FRAG*/8
};

#ifdef RTPS_MARKERS
static RMNTFFCT rtps_markers [EM_FINISH + 1];

static void rtps_marker_notify (LocalEndpoint_t *ep, EndpointMarker_t m, const char *s)
{
	if (rtps_markers [m])
		(*rtps_markers [m]) (ep, m, s);
	else
		log_printf (RTPS_ID, 0, "%s: marker reached!\r\n", s);
}

#endif

/* rtps_init_memory -- Initialize the memory structures required for correct
		       RTPS operation. */

static int rtps_init_memory (const RTPS_CONFIG *limits)
{
	/* Check if already initialized. */
	if (rtps_mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (rtps_mem_blocks, MB_END);
		return (DDS_RETCODE_OK);
	}

	/* Define the different pool attributes. */
	MDS_POOL_TYPE (rtps_mem_blocks, MB_READER, limits->readers, sizeof (READER));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_WRITER, limits->writers, sizeof (WRITER));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_REM_READER, limits->rreaders, sizeof (RemReader_t));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_REM_WRITER, limits->rwriters, sizeof (RemWriter_t));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_CCREF, limits->ccrefs, sizeof (CCREF));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_MSG_BUF, limits->messages, sizeof (RMBUF));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_MSG_ELEM_BUF, limits->msgelements, sizeof (RME));
	MDS_POOL_TYPE (rtps_mem_blocks, MB_MSG_REF, limits->msgrefs, sizeof (RMREF));

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	rtps_mem_size = mds_alloc (rtps_mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!rtps_mem_size) {
		warn_printf ("rtps_init: not enough memory available!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	log_printf (RTPS_ID, 0, "rtps_init: %lu bytes allocated for pools.\r\n",
						(unsigned long) rtps_mem_size);
#endif
	return (DDS_RETCODE_OK);
}

/* rtps_locators_listen -- Listen to the given locator list. */

static int rtps_locators_listen (DomainId_t    domain_id,
				 LocatorList_t locs,
				 unsigned      id)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	int		error;
	int		ret = DDS_RETCODE_OK;

	foreach_locator (locs, rp, np) {
		error = rtps_locator_add (domain_id, np, id, 1);
		if (!ret && error)
			ret = error;
	}
	return (ret);
}

#ifdef DDS_SECURITY

#define	RTPS_DTLS_PORT	4604	/* Default RTPS/DTLS server port. */

/* rtps_dtls_listen -- Listen to incoming DTLS/UDP requests. */

static void rtps_dtls_listen (Domain_t *dp)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np, *snp;
	uint32_t        port;

	log_printf (RTPS_ID, 0, "RTPS: update secure DTLS locators.\r\n");
	foreach_locator (dp->participant.p_def_ucast, rp, np) {
		if (np->locator.kind != LOCATOR_KIND_UDPv4 &&
		    np->locator.kind != LOCATOR_KIND_UDPv6)
			continue;

		port = RTPS_DTLS_PORT +
			dp->participant_id +
			(dp->participant.p_guid_prefix.prefix [11] & 31), /* GUID_COUNT */

		snp = locator_list_add (&dp->participant.p_sec_locs,
					np->locator.kind,
					np->locator.address,
					port,
					np->locator.scope_id,
					np->locator.scope,
					LOCF_DATA | LOCF_META | LOCF_UCAST | LOCF_SECURE | LOCF_SERVER,
					SECC_DTLS_UDP);
		if (!snp) {
			log_printf (RTPS_ID, 0, "RTPS: error adding DTLS/UDP server locator.\r\n");
			return;
		}
	}
	rtps_locators_listen (dp->domain_id,
			      dp->participant.p_sec_locs,
			      dp->index);
}

#ifdef DDS_TCP

/* rtps_tls_enabled -- Indicate that we're listening to incoming TLS/TCP reqs. */

static void rtps_tls_enabled (Domain_t *dp)
{
	LocatorList_t	slist;

	slist = rtps_tcp_secure_servers (dp->participant.p_def_ucast);
	locator_list_append (&dp->participant.p_sec_locs, slist);
}

#endif
#endif

/* rtps_update_kinds -- Update the known locator types. */

static void rtps_update_kinds (Domain_t *dp)
{
	LocatorNode_t	*np;
	LocatorRef_t	*rp;

	dp->kinds = 0;
	foreach_locator (dp->participant.p_def_ucast, rp, np)
		dp->kinds |= np->locator.kind;
	foreach_locator (dp->participant.p_meta_ucast, rp, np)
		dp->kinds |= np->locator.kind;
}

/* rtps_participant_create -- Create a new domain participant. */

int rtps_participant_create (Domain_t *dp)
{
	memcpy (dp->participant.p_proto_version, rtps_protocol_version, sizeof (ProtocolVersion_t));
	memcpy (dp->participant.p_vendor_id, rtps_vendor_id, sizeof (VendorId_t));
	dp->participant.p_exp_il_qos = 0;
#ifdef DDS_NO_MCAST
	dp->participant.p_no_mcast = 1;
#elif defined (DDS_SECURITY)
#ifdef DDS_SECURITY
	if (dp->security
#ifdef DDS_NATIVE_SECURITY
	    && (dp->participant.p_sec_caps & (SECC_DDS_SEC | (SECC_DDS_SEC << SECC_LOCAL))) == 0
#endif
	    )
#endif
                dp->participant.p_no_mcast = 1;
#endif
	dp->participant.p_sw_version = TDDS_VERSION;
	rtps_transport_locators (dp->domain_id,
				 dp->participant_id,
				 RTLT_USER,
				 &dp->participant.p_def_ucast,
				 &dp->participant.p_def_mcast,
				 NULL);

	if (locator_list_no_mcast (dp->domain_id, dp->participant.p_def_ucast))
		dp->participant.p_no_mcast = 1;

	if (rtps_locators_listen (dp->domain_id,
				  dp->participant.p_def_ucast,
				  dp->index) == DDS_RETCODE_PRECONDITION_NOT_MET)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

#ifdef DDS_SECURITY
	if (!dp->security
#ifdef DDS_NATIVE_SECURITY
		|| (dp->participant.p_sec_caps & (SECC_DDS_SEC | (SECC_DDS_SEC << SECC_LOCAL))) != 0
#endif
	    )
#endif
		rtps_locators_listen (dp->domain_id,
				      dp->participant.p_def_mcast,
				      dp->index);

	log_printf (RTPS_ID, 0, "RTPS: starting Discovery protocols.\r\n");

	rtps_transport_locators (dp->domain_id,
				 dp->participant_id,
				 RTLT_SPDP_SEDP,
				 &dp->participant.p_meta_ucast,
				 &dp->participant.p_meta_mcast,
				 &dp->dst_locs);
	rtps_update_kinds (dp);
#ifdef DDS_TCP
	if ((dp->kinds & LOCATOR_KINDS_TCP) != 0)
		rtps_tcp_add_mcast_locator (dp);
#endif
	rtps_locators_listen (dp->domain_id,
			      dp->participant.p_meta_ucast,
			      dp->index);
	rtps_locators_listen (dp->domain_id,
			      dp->participant.p_meta_mcast,
			      dp->index);
	memset (dp->participant.p_builtin_ep, 0, sizeof (dp->participant.p_builtin_ep));
	dp->participant.p_lease_duration = rtps_def_lease_per;
	dp->resend_per = rtps_def_resend_per;

#ifdef DDS_SECURITY
	if (dp->security) {
		if ((dp->participant.p_sec_caps & SECC_DTLS_UDP) != 0)
			rtps_dtls_listen (dp);
#ifdef DDS_TCP
		if ((dp->participant.p_sec_caps & SECC_TLS_TCP) != 0)
			rtps_tls_enabled (dp);
#endif
	}
#endif
	disc_start (dp);

	return (DDS_RETCODE_OK);
}

static void matched_reader_locators_update (RemReader_t *rrp);
static void matched_writer_locators_update (RemWriter_t *rwp);

static int endpoint_flush_reply_fct (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	ENDPOINT	*rep;
	READER		*rp;
	WRITER		*wp;
	RemReader_t	*rrp;
	RemWriter_t	*rwp;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	ep = *epp;
	if (!ep->rtps)
		return (1);

	rep = (ENDPOINT *) ep->rtps;
	if (rep->is_reader) {
		rp = (READER *) rep;
		LIST_FOREACH (rp->rem_writers, rwp)
			if (arg)
				matched_writer_locators_update (rwp);
			else
				rwp->rw_uc_dreply = NULL;
	}
	else {
		wp = (WRITER *) rep;
		LIST_FOREACH (wp->rem_readers, rrp)
			if (wp->endpoint.stateful) {
				if (arg)
					matched_reader_locators_update (rrp);
				else
					rrp->rr_uc_dreply = NULL;
			}
	}
	return (1);
}

# if 0
static int endpoint_check_guid_fct (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	RemReader_t	*rrp;
	RemWriter_t	*rwp;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	ep = *epp;
	if (entity_writer (ep->fh)) {
		rwp = (RemWriter_t *) ep->rtps;
		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			;
	}
	else {
		rwp = (RemWriter_t *) ep->rtps;
		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			;
	}
	pp = (Proxy_t *) ep->rtps;
	for (; pp; pp = pp->next_guid)
		if (pp)
			assert ((uintptr_t) pp > 0x60000);
	return (1);
}


static int check_peer_guids_fct (Skiplist_t *list, void *node, void *arg)
{
	Participant_t *pp, **ppp = (Participant_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	pp = *ppp;
	sl_walk (&pp->p_endpoints, endpoint_check_guid_fct, NULL);
	return (1);
}

int rtps_check_ep_guids (Domain_t *dp)
{
	sl_walk (&dp->peers, check_peer_guids_fct, NULL);
	return (1);
}

# endif

int rtps_participant_update (Domain_t *dp)
{
	int	old_kinds, ret = DDS_RETCODE_OK;

	/*log_printf (RTPS_ID, 0, "rtps_participant_update().\r\n");*/

	lock_take (dp->lock);

	/* Remember the previous valid locator kinds. */
	old_kinds = dp->kinds;

	/* Notify that locator lists will be updated. */
	rtps_locators_update (dp->domain_id, dp->index);

	/* Clear all locator lists. */
	locator_list_delete_list (&dp->participant.p_def_ucast);
	locator_list_delete_list (&dp->participant.p_def_mcast);
	locator_list_delete_list (&dp->participant.p_meta_ucast);
	locator_list_delete_list (&dp->participant.p_meta_mcast);
#ifdef DDS_SECURITY
	locator_list_delete_list (&dp->participant.p_sec_locs);
#endif

	/* Fetch the new locator lists. */
	rtps_transport_locators (dp->domain_id,
				 dp->participant_id,
				 RTLT_USER,
				 &dp->participant.p_def_ucast,
				 &dp->participant.p_def_mcast,
				 NULL);
	rtps_transport_locators (dp->domain_id,
				 dp->participant_id,
				 RTLT_SPDP_SEDP,
				 &dp->participant.p_meta_ucast,
				 &dp->participant.p_meta_mcast,
				 &dp->dst_locs);
	rtps_update_kinds (dp);

	/* Apply locator lists on transports. */
	rtps_update_begin (dp);
	if (rtps_locators_listen (dp->domain_id,
				  dp->participant.p_def_ucast,
				  dp->index) != DDS_RETCODE_OK)
		ret = DDS_RETCODE_ERROR;

#ifdef DDS_SECURITY
	if (!dp->security || (dp->participant.p_sec_caps & SECC_DDS_SEC) != 0)
#endif
		if (rtps_locators_listen (dp->domain_id,
					  dp->participant.p_def_mcast,
					  dp->index) != DDS_RETCODE_OK)
			ret = DDS_RETCODE_ERROR;

	rtps_update_kinds (dp);
	if (rtps_locators_listen (dp->domain_id,
				  dp->participant.p_meta_ucast,
				  dp->index) != DDS_RETCODE_OK)
		ret = DDS_RETCODE_ERROR;

	if (rtps_locators_listen (dp->domain_id,
				  dp->participant.p_meta_mcast,
				  dp->index) != DDS_RETCODE_OK)
		ret = DDS_RETCODE_ERROR;

#ifdef DDS_SECURITY
	if (dp->security && (dp->participant.p_sec_caps & SECC_DTLS_UDP) != 0)
		rtps_dtls_listen (dp);
#ifdef DDS_TCP
	if (dp->security && (dp->participant.p_sec_caps & SECC_TLS_TCP) != 0)
		rtps_tls_enabled (dp);
#endif
#endif
	rtps_update_end (dp);

	/* Clear all local reply locators. */
	sl_walk (&dp->participant.p_endpoints, 
		 endpoint_flush_reply_fct,
		 (old_kinds != dp->kinds) ? dp : NULL);

	/* Notify peer participants. */
	disc_participant_update (dp);

	lock_release (dp->lock);
	return (ret);
}

# if 0
#define	is_reader(eid)	((eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_READER || \
			 (eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_READER_KEY)
#define	is_writer(eid)	((eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_WRITER || \
			 (eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR) == ENTITY_KIND_WRITER_KEY)

/* rtps_endpoint_delete -- Delete an endpoint while walking in a skiplist. */

static int rtps_endpoint_delete (Skiplist_t *list, void *node, void *arg)
{
	ENDPOINT	*ep, **epp = (ENDPOINT **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	ep = *epp;
	if (ep->is_reader)
		rtps_reader_delete (ep->endpoint_id);
	else
		rtps_writer_delete (ep->endpoint_id);
	return (1);
}
# endif

/* rtps_participant_delete -- Delete a domain participant and all entities
                              contained within it. Called from
                              DDS_DomainParticipantFactory_delete_participant
                              with domain_lock (and global lock) taken. */

int rtps_participant_delete (Domain_t *dp)
{
	locator_list_delete_list (&dp->participant.p_def_ucast);
	locator_list_delete_list (&dp->participant.p_def_mcast);
	locator_list_delete_list (&dp->participant.p_meta_ucast);
	locator_list_delete_list (&dp->participant.p_meta_mcast);
#ifdef DDS_SECURITY
	locator_list_delete_list (&dp->participant.p_sec_locs);
#endif
	locator_list_delete_list (&dp->dst_locs);

	disc_stop (dp);

	return (DDS_RETCODE_OK);
}

/* rtps_participant_locator_add -- Add a default locator. */

int rtps_participant_locator_add (Domain_t        *dp,
				  int             mcast,
				  const Locator_t *loc)
{
	LocatorList_t	*lp;

	lp = (mcast) ? &dp->participant.p_def_mcast : &dp->participant.p_def_ucast;
	if (locator_list_search (*lp, loc->kind, loc->address, loc->port) >= 0)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	if (!locator_list_add (lp, loc->kind, loc->address, loc->port,
				  loc->scope_id, loc->scope, 0, 0))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}

/* rtps_participant_locator_remove -- Remove a default locator. */

int rtps_participant_locator_remove (Domain_t        *dp,
				     int             mcast,
				     const Locator_t *loc)
{
	LocatorList_t	*lp;

	lp = (mcast) ? &dp->participant.p_def_mcast : &dp->participant.p_def_ucast;
	if (!loc)
		locator_list_delete_list (lp);
	else
		locator_list_delete (lp, loc->kind, loc->address, loc->port);
	return (DDS_RETCODE_OK);
}

static void remote_reader_add (WRITER *wp, RemReader_t *rrp)
{
	LIST_ADD_TAIL (wp->rem_readers, *rrp);
	wp->rem_readers.count++;
}

static void remote_reader_remove (WRITER *wp, RemReader_t *rrp)
{
	ARG_NOT_USED (wp)

	LIST_REMOVE (wp->rem_readers, *rrp);
	wp->rem_readers.count--;
}

#if defined (RTPS_TRACE) || defined (DDS_DEBUG)

void endpoint_names (ENDPOINT *ep, const char *names [2])
{
	names [0] = str_ptr (ep->endpoint->ep.topic->name);
	names [1] = str_ptr (ep->endpoint->ep.topic->type->type_name);
}

#endif

typedef struct proxy_list_st Proxies_t;
struct proxy_list_st {
	Proxy_t		*head;
	Proxy_t		*tail;
};

static Proxies_t rtps_active_proxies;	/* All proxy entities that still need
					   to send some messages. */
static lock_t rtps_proxy_lock;		/* Protects the proxy list. */
static Proxy_t *cur_proxy = NULL;


void proxy_activate (Proxy_t *pp)
{
	int	signal_event = 0;

	lock_take (rtps_proxy_lock);
	if (pp->active) {
		lock_release (rtps_proxy_lock);
		return;
	}
	pp->next_active = NULL;
	if (rtps_active_proxies.head) {
		rtps_active_proxies.tail->next_active = pp;
		pp->prev_active = rtps_active_proxies.tail;
	}
	else {
		signal_event = 1;
		rtps_active_proxies.head = pp;
		pp->prev_active = NULL;
	}
	rtps_active_proxies.tail = pp;
	pp->active = 1;
	lock_release (rtps_proxy_lock);
	if (signal_event)
		dds_signal (DDS_EV_PROXY_NE);
}

static void proxy_deactivate (Proxy_t *pp)
{
	if (!pp->active)
		return;

	lock_take (rtps_proxy_lock);

	if (pp == cur_proxy) 
		cur_proxy = cur_proxy->next_active;

	if (pp->prev_active)
		pp->prev_active->next_active = pp->next_active;
	else
		rtps_active_proxies.head = pp->next_active;
	if (pp->next_active)
		pp->next_active->prev_active = pp->prev_active;
	else
		rtps_active_proxies.tail = pp->prev_active;
	pp->active = 0;
	lock_release (rtps_proxy_lock);
}

void proxy_wait_inactive (Proxy_t *pp)
{
	Reader_t 	*rp;
	Writer_t	*wp;

	if (!pp->active)
		return;

	if (thread_id () != dds_core_thread) {
		while (pp->active) {
			if (pp->is_writer) {
				wp = (Writer_t *) pp->u.writer->endpoint.endpoint;
#ifdef RW_LOCKS
				lock_release (wp->w_lock);
#endif
				lock_release (wp->w_topic->lock);
				lock_release (wp->w_publisher->domain->lock);
				thread_yield ();
				lock_take (wp->w_publisher->domain->lock);
				lock_take (wp->w_topic->lock);
#ifdef RW_LOCKS
				lock_take (wp->w_lock);
#endif
			}
			else {
				rp = (Reader_t *) pp->u.reader->endpoint.endpoint;
#ifdef RW_LOCKS
				lock_release (rp->r_lock);
#endif
				lock_release (rp->r_topic->lock);
				lock_release (rp->r_subscriber->domain->lock);
				thread_yield ();
				lock_take (rp->r_subscriber->domain->lock);
				lock_take (rp->r_topic->lock);
#ifdef RW_LOCKS
				lock_take (rp->r_lock);
#endif
			}
		}
	}
	else {
		proxy_deactivate (pp);
		rtps_free_messages (pp->head);
		pp->head = NULL;
	}
}

static Proxy_t *get_first_proxy (void)
{
	Proxy_t *ret;

	lock_take (rtps_proxy_lock);
	ret = cur_proxy = rtps_active_proxies.head;
	lock_release (rtps_proxy_lock);
	return (ret); 
}

static Proxy_t *get_next_proxy (void)
{
	Proxy_t *ret;

	lock_take (rtps_proxy_lock);
	if (cur_proxy)
		ret = cur_proxy = cur_proxy->next_active;
	else
		ret = NULL;
	lock_release (rtps_proxy_lock);
	return (ret);
}

/* rtps_filtered_sample -- Apply a content filter to a sample and return the
			   matching result. */

static int rtps_filtered_sample (Reader_t *rp, Change_t *cp)
{
	FilteredTopic_t	*ftp;
	DBW		dbw;
	int		err, res;

	ftp = (FilteredTopic_t *) rp->r_topic;
	dbw.dbp = cp->c_db;
	dbw.data = cp->c_data;
	dbw.left = cp->c_db->size - ((unsigned char *) cp->c_data - cp->c_db->data);
	dbw.length = cp->c_length;

	err = bc_interpret (&ftp->data.program,
			    ftp->data.filter.expression_pars,
			    &ftp->data.cache,
			    &dbw,
			    NULL,
			    1,
			    ftp->topic.type->type_support,
			    &res);
	return (!err && res);
}

/* rtps_rx_error -- Signal an RTPS reception error. */

void rtps_rx_error (RcvError_t e, unsigned char *dp, size_t length)
{
	RECEIVER	*rxp;

	rxp = &rtps_receiver;
	switch (e) {
		case R_TOO_SHORT:
			rxp->submsg_too_short++;
			break;
		case R_INV_SUBMSG:
			rxp->inv_submsgs++;
			break;
		case R_INV_QOS:
			rxp->inv_qos++;
			break;
		case R_NO_BUFS:
			rxp->no_bufs++;
			break;
		case R_UNKN_DEST:
			rxp->unkn_dest++;
			break;
		case R_INV_MARSHALL:
			rxp->inv_marshall++;
			break;
		default:
			break;
	}
	rxp->last_error = R_INV_MARSHALL;
	rxp->msg_size = (length > MAX_RX_HEADER) ? 
				  MAX_RX_HEADER : length;
	memcpy (rxp->msg_buffer, dp, rxp->msg_size);
}

/* reader_cache_add_inst -- Add a change to the History Cache of the Reader. */

int reader_cache_add_inst (READER *rp, Change_t *cp, HCI hci, int rel)
{
	int		error;

	/* If we need to apply a content filter, do so now. */
	if (cp->c_length &&
	    rp->endpoint.cfilter_rx &&
	    !rtps_filtered_sample ((Reader_t *) rp->endpoint.endpoint, cp)) {
		hc_change_free (cp);
		return (DDS_RETCODE_OK);
	}

	/* Accepted sample: add to cache. */
	if (rp->endpoint.tfilter_rx)
		error = hc_add_received (rp->endpoint.endpoint->cache, cp, hci, rel);
	else
		error = hc_add_inst (rp->endpoint.endpoint->cache, cp, hci, rel);
	return (error);
}

/* reader_cache_add_key -- Add a change to the History Cache of the Reader.  The
			   instance of the data sample is identified via a hash
			   key (if non-NULL) and/or the key fields (if
			   non-NULL). */

void reader_cache_add_key (READER              *rp,
			   Change_t            *cp,
			   const KeyHash_t     *hp,
			   const unsigned char *key,
			   size_t              keylen)
{
	HCI		hci;

	if (rp->endpoint.multi_inst) {
		hci = hc_lookup_hash (rp->endpoint.endpoint->cache,
							hp, key, keylen,
							&cp->c_handle, 1, 0,
							NULL);
		if (!hci) {
			dcps_sample_rejected ((Reader_t *) rp->endpoint.endpoint,
					      DDS_REJECTED_BY_INSTANCES_LIMIT, 0);
			hc_change_free (cp);
			return;
		}
	}
	else
		hci = NULL;

	reader_cache_add_inst (rp, cp, hci, 0);
}

static void remote_writer_add (READER *rp, RemWriter_t *rwp)
{
	LIST_ADD_TAIL (rp->rem_writers, *rwp);
	rp->rem_writers.count++;
}

void remote_writer_remove (READER *rp, RemWriter_t *rwp)
{
	ARG_NOT_USED (rp)

	LIST_REMOVE (rp->rem_writers, *rwp);
	rp->rem_writers.count--;
}


/* remote_reader_new_change -- A new cache change copy is needed for sending. */

static Change_t *remote_reader_new_change (RemReader_t *rrp,
					   Change_t    *cp,
					   Change_t    **mctypes,
					   int         rmarshall,
					   int         no_mcast
#ifdef DDS_NATIVE_SECURITY
					 , unsigned    crypto_handle,
					   unsigned    crypto_type
#endif
					                          )
{
	WRITER			*wp;
	Change_t		*ncp;
	TopicType_t		*ttp;
	const TypeSupport_t	*tsp;
	unsigned char		*dp;
	int			marshall;
#ifdef DDS_NATIVE_SECURITY
	DBW			dbw;
	DB			*ndbp;
	size_t			nlength;
#endif
	DDS_ReturnCode_t 	ret;

	wp = rrp->rr_writer;
	ttp = wp->endpoint.endpoint->ep.topic->type;
	tsp = ttp->type_support;
	if (mctypes && (ncp = mctypes [rmarshall]) != NULL) {
		if (no_mcast)
			ncp->c_no_mcast = 1;
		rcl_access (ncp);
		ncp->c_nrefs++;
		rcl_done (ncp);
		return (ncp);
	}

	/* Allocate a new change record since we need to keep different information
	   at RTPS level compared to Cache level. */
	ncp = hc_change_new ();
	if (!ncp)
		return (NULL);

	TRC_CHANGE (ncp, "remote_reader_new_change", 1);
	ncp->c_wack = 0;
	ncp->c_urgent = cp->c_urgent;
	ncp->c_kind = cp->c_kind;
	ncp->c_linear = 1;
	ncp->c_no_mcast = no_mcast;
	ncp->c_writer = cp->c_writer;
	ncp->c_time = cp->c_time;
	ncp->c_handle = cp->c_handle;
	ncp->c_seqnr = cp->c_seqnr;
	memcpy (ncp->c_dests, cp->c_dests, sizeof (ncp->c_dests));

	/* Do we need to marshall the data? */
	if (!rmarshall || cp->c_kind != ALIVE)
		marshall = 0;
	else if (tsp->ts_prefer == MODE_PL_CDR) {
		if (tsp->ts_pl->builtin) {
			marshall = 1;
			dp = cp->c_data;
		}
		else if ((((cp->c_data [0] << 8) | cp->c_data [1]) >> 1) == MODE_PL_CDR)
			marshall = 0;
		else {
			marshall = 1;
			dp = cp->c_data + 4;
		}
	}
	else if ((((cp->c_data [0] << 8) | cp->c_data [1]) >> 1) == MODE_RAW) {
		marshall = 1;
		dp = cp->c_data + 4;
	}
	else	/* Already marshalled! */
		marshall = 0;

	if (!marshall) {
#ifdef DDS_NATIVE_SECURITY
		if (crypto_type) {
			dbw.dbp = cp->c_db;
			dbw.data = cp->c_data;
			if (cp->c_db && cp->c_db->size)
				dbw.left = cp->c_db->size - 
						(cp->c_data - cp->c_db->data);
			else
				dbw.left = cp->c_length;
			dbw.length = cp->c_length;
			ncp->c_db = sec_encode_serialized_data (&dbw,
								crypto_handle, 
								&ncp->c_length,
								&ret);
			if (!ncp->c_db)
				goto free_change;

			ncp->c_data = ncp->c_db->data;
		}
		else
#endif
		{
			ncp->c_db = cp->c_db;
			if (cp->c_db) {
				rcl_access (ncp->c_db);
				ncp->c_db->nrefs++;
				rcl_done (ncp->c_db);
				ncp->c_data = cp->c_data;
			}
			else if (cp->c_data == cp->c_xdata) {
				memcpy (ncp->c_xdata, cp->c_data, cp->c_length);
				ncp->c_data = ncp->c_xdata;
			}
			else
				ncp->c_data = cp->c_data;
			ncp->c_length = cp->c_length;
		}
#ifdef RTPS_TRC_LRBUFS
		log_printf (RTPS_ID, 0, "remote_reader_new_change: ncp->c_db->nrefs++\r\n");
#endif
		if (mctypes)
			mctypes [rmarshall] = ncp;
		return (ncp);
	}
	else
		ncp->c_db = NULL;

	/* Actual marshalling requires a new data buffer for the
	   marshalled info - allocate buffer and marshall into it. */
	ncp->c_length = DDS_MarshalledDataSize (dp, 0, tsp, &ret);
	if (!ncp->c_length) {
		log_printf (RTPS_ID, 0, "remote_reader_new_change: marshalled buffer size could not be determined (%u)!\r\n", ret);
		goto free_change;
	}
	if ((ncp->c_db = db_alloc_data ((ncp->c_length + 3) & ~3, 1)) == NULL) {
		log_printf (RTPS_ID, 0, "remote_reader_new_change: out of memory for data buffer!\r\n");
		goto free_change;
	}
	ncp->c_data = ncp->c_db->data;
	ret = DDS_MarshallData (ncp->c_data, dp, 0, tsp);

	if ((ncp->c_length & 3) != 0)
		memset (ncp->c_data + ncp->c_length, 0, 4 - (ncp->c_length & 3));

	if (ret) {
		log_printf (RTPS_ID, 0, "remote_reader_new_change: error %u marshalling data!\r\n", ret);
		goto free_change;
	}
#ifdef DDS_NATIVE_SECURITY
	if (crypto_type) {
		dbw.dbp = ncp->c_db;
		dbw.data = ncp->c_data;
		dbw.left = ncp->c_length;
		dbw.length = ncp->c_length;
		ndbp = sec_encode_serialized_data (&dbw,
						   crypto_handle, 
						   &nlength,
						   &ret);
		if (!ndbp)
			goto free_change;

		db_free_data (ncp->c_db);
		ncp->c_db = ndbp;
		ncp->c_data = ndbp->data;
		ncp->c_length = nlength;
	}
#endif
	if (mctypes)
		mctypes [1] = ncp;
	return (ncp);

    free_change:
	TRC_CHANGE (ncp, "remote_reader_new_change", 0);
	hc_change_free (ncp);
	return (NULL);
}

typedef enum {
	FR_ALLOW_TX,
	FR_FILTER_TX,
	FR_DELAYED
} FilterRes_t;

/* DDS_FILTER_WRITE - Returns a non-0 result if the sample is not relevant for
		      the reader. */

/*#define DDS_FILTER_WRITE(rrp,cp)	0*/

static FilterRes_t rtps_filter_write (RemReader_t *rrp, Change_t *cp)
{
	DiscoveredReader_t	*drp = (DiscoveredReader_t *) rrp->rr_endpoint;
	FilterData_t		*fp;
	DBW			dbw;
	int			err, res;

	/* Check remote content filter. */
	if ((fp = drp->dr_content_filter) != NULL) {
		dbw.dbp = cp->c_db;
		dbw.data = cp->c_data;
		dbw.left = cp->c_db->size - ((unsigned char *) cp->c_data - cp->c_db->data);
		dbw.length = cp->c_length;

		err = bc_interpret (&fp->program,
				    fp->filter.expression_pars,
				    &fp->cache,
				    &dbw,
				    NULL,
				    1,
				    drp->dr_topic->type->type_support,
				    &res);
		if (!err && !res)
			return (FR_FILTER_TX);
	}

#if 0	/* Check time-based filter. */
	if ((drp->dr_time_based_filter.sec || drp->dr_time_based_filter.nanosec)) {

		/* Check if new sample would be sent too fast. */
		if (time_delta_allows_tx (rrp, cp->c_handle)) {

			/* Can send - mark last Tx time. */
			mark_sample_time (rrp, cp->c_handle);
			return (FR_ALLOW_TX);
		}

		/* Not allowed to send sample - enqueue for later Tx (after time
		   period).  If there is already a sample enqueued, drop the
		   previous sample. */
		add_delayed_sample (rrp, cp);
		return (FR_DELAYED);
	}
#endif
	/* Not filtered. */
	return (FR_ALLOW_TX);
}

#define	DDS_FILTER_WRITE(rrp,cp) ((rrp->rr_endpoint->entity.flags & EF_FILTERED) != 0) ? \
						rtps_filter_write (rrp, cp) : 0

RR_EVENTS *rtps_rr_event [RR_MAX_TYPES];

/* reader_in_dests -- Return a non-0 result if the proxy is in the list of
		      endpoints. */

static int reader_in_dests (RemReader_t *rrp, handle_t *handles, unsigned max)
{
	Entity_t	*ep;
	Participant_t	*pp;
	unsigned	i;
	int		stateless;

	stateless = !rrp->rr_reliable && !rrp->rr_writer->endpoint.stateful;
	for (i = 0; i < max; i++) {
		if (!*handles)
			break;

		if (stateless) {
			ep = entity_ptr (*handles);
			if (entity_type (ep) == ET_PARTICIPANT) {
				pp = (Participant_t *) ep;
				if (pp->p_src_locators &&
				    pp->p_src_locators->data->locator.kind == 
				    		rrp->rr_locator->locator.kind)
					return (1);
			}
		}
		else if (rrp->rr_endpoint->entity.handle == *handles)
			return (1);

		handles++;
	}
	return (0);
}

/* proxy_add_change -- A cache change should be added to a proxy. */

int proxy_add_change (uintptr_t user, Change_t *cp, HCI hci)
{
	RemReader_t	*rrp = (RemReader_t *) user;
	RRType_t	type;
	Change_t	*ncp;
#ifdef DDS_NATIVE_SECURITY
	LocalEndpoint_t	*wp;
	unsigned	crypto_type;
#endif
	FilterRes_t	res;

	type = rr_type (rrp->rr_writer->endpoint.stateful, rrp->rr_reliable);
	if (cp->c_data) {
		if (cp->c_dests [0] &&
		    !reader_in_dests (rrp, cp->c_dests, MAX_DW_DESTS))
			ncp = NULL;

		else if ((res = DDS_FILTER_WRITE (rrp, cp)) != FR_ALLOW_TX) {
			if (res == FR_DELAYED)
				/* Enqueued sample - just return. */
				return (1);

			ncp = NULL;	/* Continue -> filtered sample! */
		}
		else {
#ifdef DDS_NATIVE_SECURITY
			wp = rrp->rr_writer->endpoint.endpoint;
			crypto_type = (wp->payload_prot) ? wp->crypto_type : 0;
#endif
			ncp = remote_reader_new_change (rrp, cp, NULL, 
							rrp->rr_marshall,
							rrp->rr_no_mcast
#ifdef DDS_NATIVE_SECURITY
						      ,	wp->crypto,
						        crypto_type
#endif
							);
			if (!ncp)
				return (0);	/* No memory/don't send. */
		}
	}
	else {
		ncp = hc_change_clone (cp);
		if (!ncp)
			return (0);
	}
	if (ncp)
		cp->c_wack++;

	if (ncp || rrp->rr_reliable)
		(*rtps_rr_event [type]->add_change) (rrp, ncp, hci, &cp->c_seqnr);

	CACHE_CHECK (&wp->endpoint, "proxy_add_change");
	return (1);
}

RW_EVENTS *rtps_rw_event [] = {
	&sfr_be_events,
	&sfr_rel_events
};

/* rtps_writer_create -- Create a data writer.

   NOTE: This should be extended with QoS awareness.  Currently, the writer
         is configured with the following QoS behaviour:

	 	- HISTORY          : depth = unlimited, kind = KEEP_ALL
		- RESOURCE_LIMITS  : based on pool limits
		- LIFESPAN         : duration = infinite
		- TIME_BASED_FILTER: minimum_separation = 0
 */

int rtps_writer_create (Writer_t          *w,
			int               push_mode,
			int		  stateful,
			const Duration_t  *heartbeat,
			const Duration_t  *nack_rsp,
			const Duration_t  *nack_supp,
			const Duration_t  *resend_per)
{
	WRITER		*wp;
#ifdef RTPS_TRACE
	unsigned	dtrace;
#endif

	ctrc_printd (RTPS_ID, RTPS_W_CREATE, &w, sizeof (w));

	prof_start (rtps_w_create);

	/* Set defaults for unspecified durations. */
	if (!heartbeat)
		heartbeat = &rtps_def_heartbeat;
	if (!nack_rsp)
		nack_rsp = &rtps_def_nack_resp;
	if (!nack_supp)
		nack_supp = &rtps_def_nack_supp;

	/* Check some mandatory fields. */
	if (resend_per &&
	    !resend_per->secs && 
	    resend_per->nanos < MIN_RESEND_TIME_NS) {
		warn_printf ("rtps_writer_create: incorrect resend period!\r\n");
		return (DDS_RETCODE_UNSUPPORTED);
	}

	/* Allocate required memory. */
	if ((wp = mds_pool_alloc (&rtps_mem_blocks [MB_WRITER])) == NULL) {
		warn_printf ("rtps_writer_create: out of memory for writer context!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Connect RTPS-specific endpoint context to Local Endpoint context. */
	wp->endpoint.endpoint = &w->w_lep;
	w->w_rtps = wp;

	/* Setup entity fields. */
	wp->endpoint.is_reader = 0;
	wp->endpoint.reliability = (w->w_qos->qos.reliability_kind == 
						DDS_RELIABLE_RELIABILITY_QOS);

	/* Setup writer fields. */
	wp->endpoint.stateful = stateful;
	wp->endpoint.multi_inst = w->w_topic->type->type_support->ts_keys;
	wp->endpoint.push_mode = push_mode;
	wp->endpoint.cache_act = 0;
#ifdef RTPS_TRACE
	dtrace = rtps_def_trace (w->w_handle, str_ptr (w->w_topic->name));
	wp->endpoint.trace_frames = (dtrace & DDS_RTRC_FTRACE) != 0;
	wp->endpoint.trace_sigs   = (dtrace & DDS_RTRC_STRACE) != 0;
	wp->endpoint.trace_state  = (dtrace & DDS_RTRC_ETRACE) != 0;
	wp->endpoint.trace_tmr    = (dtrace & DDS_RTRC_TTRACE) != 0;
#ifdef RTPS_SEDP_TRACE
	if (w->w_entity_id.id [3] == (ENTITY_KIND_BUILTIN | ENTITY_KIND_WRITER_KEY) &&
	    w->w_entity_id.id [0] == 0 &&
	    (w->w_entity_id.id [1] == 0 || w->w_entity_id.id [1] == 2))
		wp->endpoint.trace_frames = wp->endpoint.trace_sigs =
		wp->endpoint.trace_state  = wp->endpoint.trace_tmr  = 1;
#endif
#ifdef RTPS_SPDP_TRACE
	if (w->w_entity_id.id [3] == (ENTITY_KIND_BUILTIN | ENTITY_KIND_WRITER_KEY) &&
	    w->w_entity_id.id [0] == 0 &&
	    w->w_entity_id.id [1] == 1)
		wp->endpoint.trace_frames = wp->endpoint.trace_sigs =
		wp->endpoint.trace_state  = wp->endpoint.trace_tmr  = 1;
#endif
#ifdef RTPS_CDD_TRACE
	if (w->w_entity_id.id [3] == (ENTITY_KIND_BUILTIN | ENTITY_KIND_WRITER_KEY) &&
	    w->w_entity_id.id [0] == 0 &&
	    w->w_entity_id.id [1] == 3)
		wp->endpoint.trace_frames = wp->endpoint.trace_sigs =
		wp->endpoint.trace_state  = wp->endpoint.trace_tmr  = 1;
#endif
#else
	wp->endpoint.trace_frames = wp->endpoint.trace_sigs =
	wp->endpoint.trace_state  = wp->endpoint.trace_tmr  = 0;
#endif
#ifdef RTPS_OPT_MCAST
	wp->endpoint.mc_locators = NULL;
#endif
	if (stateful) {
		wp->rh_period = duration2ticks (heartbeat);
		wp->heartbeats = w->w_handle;
		wp->nack_resp_delay = duration2ticks (nack_rsp);
		wp->nack_supp_duration = duration2ticks (nack_supp);
	}
	else if (resend_per) {
		wp->rh_period = duration2ticks (resend_per);
		wp->endpoint.resends = 1;
	}
	else
		wp->endpoint.resends = 0;
	wp->backoff = 0;
	wp->rh_timer = NULL;
	wp->backoff = 0;
	wp->prio = w->w_qos->qos.transport_priority.value;
	wp->no_mcast = 0;
	wp->mc_marshall = 0;

	/* Initialize the list of Proxy-Readers/ReaderLocators. */
	LIST_INIT (wp->rem_readers);
	wp->rem_readers.count = 0;

	prof_stop (rtps_w_create, 1);

	if (rtps_log)
		log_printf (RTPS_ID, 0, "RTPS: writer (%s) created.\r\n",
						str_ptr (w->w_topic->name));
	return (DDS_RETCODE_OK);
}

#ifdef DDS_DEBUG

/* rtps_pool_stats -- Display some pool statistics. */

void rtps_pool_stats (int log)
{
	size_t	sizes [PPT_SIZES];

	print_pool_hdr (log);
	memset (sizes, 0, sizeof (sizes));
	rtps_pool_dump (sizes);
	print_pool_end (sizes);
}

#endif

/* rtps_writer_delete -- Delete an RTPS writer. 

   Locks: On entry, the writer, its publisher, topic and domain should be locked
          (inherited from rtps_matched_reader_remove/rtps_reader_locator_remove).
*/

int rtps_writer_delete (Writer_t *w)
{
	WRITER		*wp = (WRITER *) w->w_rtps;
	RemReader_t	*rrp;
	PROF_ITER	(n);

	ctrc_printd (RTPS_ID, RTPS_W_DELETE, &w, sizeof (w));
	prof_start (rtps_w_delete);

	if (!wp || !w->w_rtps)
		return (DDS_RETCODE_ALREADY_DELETED);

	log_printf (RTPS_ID, 0, "RTPS: writer (%s) delete.\r\n", str_ptr (w->w_topic->name));

	while (LIST_NONEMPTY (wp->rem_readers)) {
		PROF_INC (n);

		rrp = LIST_HEAD (wp->rem_readers);
		if (wp->endpoint.stateful)
			rtps_matched_reader_remove (w, (DiscoveredReader_t *) rrp->rr_endpoint);
		else
			rtps_reader_locator_remove (w, &rrp->rr_locator->locator);
	}
	if (wp->rh_timer) {
		/*printf ("rtps_writer_delete: stop timer!\r\n");*/
		tmr_stop (wp->rh_timer);
		tmr_free (wp->rh_timer);
		wp->rh_timer = NULL;
	}
	w->w_rtps = NULL;
	mds_pool_free (&rtps_mem_blocks [MB_WRITER], wp);

	prof_stop (rtps_w_delete, n);
	return (DDS_RETCODE_OK);
}

/* rtps_writer_new_change -- A cache change was added. */

static int rtps_writer_new_change (uintptr_t user, Change_t *cp, HCI hci)
{
	WRITER		*wp = (WRITER *) user;
	RemReader_t	*rrp;
	RRType_t	type;
	Change_t	*ncp = NULL;
	Change_t	*mchanges [2];
#ifdef DDS_NATIVE_SECURITY
	unsigned	crypto_handle;
	unsigned	crypto_type;
#endif
	FilterRes_t	res;
	PROF_ITER	(n);

	ctrc_printd (RTPS_ID, RTPS_W_N_CHANGE, &user, sizeof (user));

	prof_start (rtps_w_new);

	/* If no remote readers -- no need to continue. */
	if (LIST_EMPTY (wp->rem_readers))
		return (DDS_RETCODE_OK);

	/* Add cache change to all existing Remote Readers. */
	mchanges [0] = mchanges [1] = NULL;
#ifdef DDS_NATIVE_SECURITY
	if (wp->endpoint.endpoint->payload_prot) {
		crypto_handle = wp->endpoint.endpoint->crypto;
		crypto_type = wp->endpoint.endpoint->crypto_type;
	}
	else {
		crypto_handle = 0;
		crypto_type = 0;
	}
#endif
	LIST_FOREACH (wp->rem_readers, rrp) {
		PROF_INC (n);

		type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
		if (cp->c_dests [0] &&
		    !reader_in_dests (rrp, cp->c_dests, MAX_DW_DESTS))
			ncp = NULL;

		else if (cp->c_data) {
			if ((res = DDS_FILTER_WRITE (rrp, cp)) != FR_ALLOW_TX) {
				if (res == FR_DELAYED)
					/* Enqueued sample - just return. */
					continue;

				ncp = NULL;	/* Continue -> filtered sample! */
			}
			else {
				ncp = remote_reader_new_change (rrp, cp, 
								mchanges,
								wp->mc_marshall,
								wp->no_mcast
#ifdef DDS_NATIVE_SECURITY
							      , crypto_handle,
							        crypto_type
#endif
								);
				if (!ncp)
					return (DDS_RETCODE_OUT_OF_RESOURCES);
			}
		}
		else if (!ncp) {
			ncp = hc_change_clone (cp);
			if (!ncp)
				return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else {
			rcl_access (ncp);
			ncp->c_nrefs++;
			rcl_done (ncp);
		}
		if (ncp)
			cp->c_wack++;
		if (ncp || rrp->rr_reliable)
			(*rtps_rr_event [type]->add_change) (rrp,
							     ncp,
							     hci,
							     &cp->c_seqnr);
		CACHE_CHECK (&wp->endpoint, "rtps_writer_new_change");
	}
	prof_stop (rtps_w_new, n);
	return (DDS_RETCODE_OK);
}

# if 0
/* rtps_writer_new_change -- A cache change was added. */

static int rtps_writer_new_change (uintptr_t user, Change_t *cp, HCI hci)
{
	WRITER		*wp = (WRITER *) user;
	RemReader_t	*rrp, *head, *tail;
	RRType_t	type;
	Change_t	*ncp = NULL;
	Change_t	*mchanges [2];
	FilterRes_t	res;
	int		marshall;
	PROF_ITER	(n);

	ctrc_printd (RTPS_ID, RTPS_W_N_CHANGE, &user, sizeof (user));

	prof_start (rtps_w_new);

	/* Construct proxy list and determine whether we need to marshall
	   the data. We don't need to if all destinations are local and
	   multicast is not needed. */
	head = tail = NULL;
	marshall = 0;
	LIST_FOREACH (wp->rem_readers, rrp) {
		if (rrp->rr_marshall)
			marshall = 1;

		if (cp->c_dests [0] &&
		    !reader_in_dests (rrp, cp->c_dests, MAX_DW_DESTS))
			continue;

		if (cp->c_data && 
		    (res = DDS_FILTER_WRITE (rrp, cp)) != FR_ALLOW_TX)
			continue;

		if (head)
			tail->rr_link = (Proxy_t *) rrp;
		else
			head = rrp;
		tail = rrp;
	}
	if (!head)
		return (DDS_RETCODE_OK);

	tail->rr_link = NULL;
	if (head->rr_link)	/* Force marshalling if multiple destinations.*/
		marshall = 1;

	/* Assign change to all proxies in list. */
	mchanges [0] = mchanges [1] = NULL;
	for (rrp = head; rrp; rrp = (RemReader_t *) rrp->rr_link) {
		PROF_INC (n);
		type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
		if (cp->c_data) {
			ncp = remote_reader_new_change (rrp, cp, mchanges, wp->mc_marshall, wp->no_mcast);
			if (!ncp)
				return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else if (!ncp) {
			ncp = hc_change_clone (cp);
			if (!ncp)
				return (DDS_RETCODE_OUT_OF_RESOURCES);

			ncp->c_no_mcast = wp->no_mcast;
		}
		else {
			rcl_access (ncp);
			ncp->c_nrefs++;
			rcl_done (ncp);
		}
		if (ncp)
			cp->c_wack++;
		if (ncp || rrp->rr_reliable)
			(*rtps_rr_event [type]->add_change) (rrp,
							     ncp,
							     hci,
							     &cp->c_seqnr);

		CACHE_CHECK (&wp->endpoint, "rtps_writer_new_change");
	}
	prof_stop (rtps_w_new, n);
	return (DDS_RETCODE_OK);
}
# endif

/* rtps_writer_delete_change -- A change has become invalid. */

static int rtps_writer_delete_change (uintptr_t user, Change_t *cp)
{
	WRITER		*wp = (WRITER *) user;
	RemReader_t	*rrp;
	RRType_t	type;
	PROF_ITER	(n);

	ctrc_printd (RTPS_ID, RTPS_W_RM_CHANGE, &user, sizeof (user));
	prof_start (rtps_w_remove);

	/* If no remote readers -- no need to continue. */
	if (LIST_EMPTY (wp->rem_readers))
		return (DDS_RETCODE_OK);

	/* Add cache change to all existing Remote Readers. */
	LIST_FOREACH (wp->rem_readers, rrp) {
		PROF_INC (n);

		type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
		(*rtps_rr_event [type]->rem_change) (rrp, cp);
		CACHE_CHECK (&wp->endpoint, "rtps_writer_clear_change");
	}
	prof_stop (rtps_w_remove, n);
	return (DDS_RETCODE_OK);
}

/* rtps_writer_urgent_change -- Request writer to send HEARTBEAT on reliable
				connections to request an acknowledgement from
				the receiver. */

static int rtps_writer_urgent_change (uintptr_t user, Change_t *cp)
{
	WRITER		*wp = (WRITER *) user;
#if 0
	RemReader_t	*rrp;
	unsigned	nreaders = 0;
#endif
	ARG_NOT_USED (cp)

	ctrc_printd (RTPS_ID, RTPS_W_URG_CHANGE, &user, sizeof (user));
	prof_start (rtps_w_urgent);

	/* If no remote readers -- no need to continue. */
	if (LIST_EMPTY (wp->rem_readers))
		return (DDS_RETCODE_OK);

	/* If not stateful - nothing to do. */
	if (!wp->endpoint.stateful)
		return (DDS_RETCODE_OK);

	/* If heartbeat timer already running -- no action. */
	if (wp->rh_timer)
		return (DDS_RETCODE_OK);

#if 0	/* TODO: To be examined whether this is still needed ... */
	/* Request acknowledgement from all existing Remote Readers. */
	LIST_FOREACH (wp->rem_readers, rrp)
		if (rrp->rr_reliable &&
		    rrp->rr_changes.nchanges &&
		    rrp->rr_tstate != RRTS_ANNOUNCING) {
			rrp->rr_heartbeat = 1;
			nreaders++;
		}

	/* If some readers are ready, send a heartbeat. */
	if (nreaders)
		sfw_send_heartbeat (wp, 0);
#endif
	prof_stop (rtps_w_urgent, 1);
	return (DDS_RETCODE_OK);
}

/* rtps_writer_alive -- Request the writer to send an Alive Heartbeat. */

void rtps_writer_alive (uintptr_t user)
{
	WRITER		*wp = (WRITER *) user;

	ctrc_printd (RTPS_ID, RTPS_W_ALIVE, &user, sizeof (user));

	/* If no remote readers -- no need to continue. */
	if (LIST_EMPTY (wp->rem_readers))
		return;

	/* If not stateful - nothing to do. */
	if (!wp->endpoint.stateful)
		return;

	/* Send ALIVE. */
	sfw_send_heartbeat (wp, 1);
}

/* rtps_writer_write -- Add data to a writer. */

int rtps_writer_write (Writer_t       *w,
		       const void     *data,
		       size_t         length,
		       InstanceHandle h,
		       HCI            hci,
		       const FTime_t  *time,
		       handle_t       dests [],
		       unsigned       ndests)
{
	WRITER		*wp;
	Change_t	*cp;
	int		error;

	ctrc_printd (RTPS_ID, RTPS_W_WRITE, &w, sizeof (w));
	prof_start (rtps_w_write);

	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_writer_write: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_ALREADY_DELETED);
	}

 	/* Check # of destinations. */
	if (ndests > MAX_DW_DESTS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

 	/* Limited functionality: data size is restricted. */
	if (length > C_DSIZE)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Allocate a new change record. */
	cp = hc_change_new ();
	if (!cp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (ndests)
		memcpy (cp->c_dests, dests, ndests * sizeof (handle_t));

	cp->c_db = NULL;
	memcpy (cp->c_xdata, data, length);
	cp->c_data = cp->c_xdata;
     /*	cp->c_wack = 0; */
	cp->c_kind = ALIVE;
	cp->c_linear = 1;
	cp->c_writer = w->w_handle;
	if (time)
		cp->c_time = *time;
	else
		sys_getftime (&cp->c_time);
	cp->c_handle = h;
	cp->c_length = length;
	error = hc_add_inst (w->w_cache, cp, hci, 0);

	prof_stop (rtps_w_write, 1);
	return (error);
}

/* rtps_writer_dispose -- Dispose data from a writer. */

int rtps_writer_dispose (Writer_t       *w,
			 InstanceHandle h,
			 HCI            hci,
			 const FTime_t  *time,
			 handle_t       dests [],
			 unsigned       ndests)
{
	WRITER		*wp;
	int		error;

	ctrc_printd (RTPS_ID, RTPS_W_DISPOSE, &w, sizeof (w));
	prof_start (rtps_w_dispose);
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_writer_dispose: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (wp->endpoint.multi_inst)
		error = hc_dispose (w->w_cache, h, hci, time, dests, ndests);

	else {
		log_printf (RTPS_ID, 0, "rtps_writer_dispose: writer(%s) dispose on key-less topic!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	prof_stop (rtps_w_dispose, 1);
	return (error);
}

/* rtps_writer_unregister -- Unregister data from a writer. */

int rtps_writer_unregister (Writer_t       *w,
			    InstanceHandle h,
			    HCI            hci,
			    const FTime_t  *time,
			    handle_t       dests [],
			    unsigned       ndests)
{
	WRITER		*wp;
	int		error;

	ctrc_printd (RTPS_ID, RTPS_W_UNREG, &w, sizeof (w));
	prof_start (rtps_w_dispose);
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_writer_unregister: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (wp->endpoint.multi_inst)
		error = hc_unregister (w->w_cache, h, hci, time, dests, ndests);
	else {
		log_printf (RTPS_ID, 0, "rtps_writer_unregister: writer(%s) unregister on key-less topic!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	prof_stop (rtps_w_dispose, 1);
	return (error);
}

/* locator_eq -- Check if two locators are identical. */

static int locator_eq (const Locator_t *lp1, const Locator_t *lp2)
{
	return (lp1->kind == lp2->kind &&
	        lp1->port == lp2->port &&
	        !memcmp (lp1->address, lp2->address, 16));
}

/* rtps_reader_locator_add -- Add a destination for a writer. */

int rtps_reader_locator_add (Writer_t      *w,
			     LocatorNode_t *np,
			     int           exp_inline_qos,
			     int           marshall)
{
	/*Domain_t	*dp;*/
	WRITER		*wp;
	RemReader_t	*rrp;
	RRType_t	type;
	/*int		error;*/

	ctrc_printd (RTPS_ID, RTPS_W_RLOC_ADD, &w, sizeof (w));
	prof_start (rtps_w_rloc_add);
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_reader_locator_add: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (wp->endpoint.stateful)	/* Not applicable in stateful mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (wp->rem_readers, rrp)
		if (locator_eq (&rrp->rr_locator->locator, &np->locator))
			return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/*
	dp = w->w_publisher->domain;
	 *
	 * No longer add locators for the reader locator.  The rx-locator has
	 * already been added and for tx, it's not needed at all.  Also, it
	 * creates issues in combination with TCP!
	 *
	 *error = rtps_locator_add (dp->domain_id, np, dp->index, 0);
	 if (error) {
	 	log_printf (RTPS_ID, 0, "rtps_reader_locator_add: can't add locator to transport layer!\r\n");
		return (error);
	 }*/
	if ((rrp = mds_pool_alloc (&rtps_mem_blocks [MB_REM_READER])) == NULL) {
		log_printf (RTPS_ID, 0, "rtps_reader_locator_add: no memory for reader locator.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (rrp, 0, sizeof (RemReader_t));
	rrp->rr_endpoint = &w->w_ep;
	/*rrp->rr_next_active = rrp->rr_prev_active = NULL;
	rrp->rr_active = 0;*/
	rrp->rr_writer = wp;
	LIST_INIT (rrp->rr_changes);
	/*rrp->rr_changes.nchanges = 0;
	rrp->rr_unsent_changes = rrp->rr_requested_changes = NULL;*/
	rrp->rr_locator = np;
	np->users++;
	rrp->rr_inline_qos = exp_inline_qos;
	rrp->rr_is_writer = 1;
	/*rrp->rr_reliable = 0;*/
	rrp->rr_marshall = marshall;
	/*rrp->rr_head = rrp->rr_tail = NULL;
	rrp->rr_link = NULL;*/
	remote_reader_add (wp, rrp);
	wp->mc_marshall = 1;
	type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
	if (rtps_rr_event [type]->start)
		(*rtps_rr_event [type]->start) (rrp);

	/* Start monitoring cache changes if not already enabled. */
	if (!wp->endpoint.cache_act) {
		wp->endpoint.cache_act = 1;
		hc_monitor_start (wp->endpoint.endpoint->cache, (uintptr_t) wp);
	}

	/* Request cache to give us all queued samples. */
	hc_replay (wp->endpoint.endpoint->cache, proxy_add_change, (uintptr_t) rrp);

	prof_stop (rtps_w_rloc_add, 1);
	return (DDS_RETCODE_OK);
}

/* rtps_reader_locator_remove -- Remove a destination for a writer.
  
   Locks: On entry, the writer, its publisher, topic and domain should be
   locked (as statemachine->finish is called.) */

void rtps_reader_locator_remove (Writer_t *w, const Locator_t *lp)
{
	WRITER		*wp;
	RemReader_t	*rrp;
	RRType_t	type;

	ctrc_printd (RTPS_ID, RTPS_W_RLOC_REM, &w, sizeof (w));
	prof_start (rtps_w_rloc_rem);
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_reader_locator_remove: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		return;
	}
	if (wp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return;

	for (rrp = wp->rem_readers.head; rrp; rrp = rrp->next)
		if (locator_eq (&rrp->rr_locator->locator, lp))
			break;

	if (!rrp) {
		log_printf (RTPS_ID, 0, "rtps_reader_locator_remove: destination doesn't exist!\r\n");
		return;
	}

	/* Signal state machine to cleanup, since we're done. */
	type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
	if (rtps_rr_event [type]->finish)
		(*rtps_rr_event [type]->finish) (rrp);

	/* Remove sending socket.  !! see above !! */
	/* rtps_locator_remove (w->w_publisher->domain->participant_id, rrp->rr_locator); */

	/* Cleanup locator. */
	rrp->rr_locator->users--;
	rrp->rr_locator = NULL;

	/* Remove from Writer's list. */
	remote_reader_remove (wp, rrp);

	/* If this was the last remote reader locator, stop monitoring the
	   cache changes. */
	if (LIST_EMPTY (wp->rem_readers)) {
		wp->endpoint.cache_act = 0;
		hc_monitor_end (wp->endpoint.endpoint->cache);
	}

	/* Free the context. */
	mds_pool_free (&rtps_mem_blocks [MB_REM_READER], rrp);
	prof_stop (rtps_w_rloc_rem, 1);
}

#ifdef RTPS_MARSHALL_ALWAYS
#define	MARSHALL(prefix)	1
#else
#define	MARSHALL(prefix)	guid_needs_marshalling (&prefix)
#endif

# if 0

static int guid_info_cmp (const void *np, const void *data)
{
	const GUID_INFO	*gip, **gipp = (const GUID_INFO **) np;
	const GUID_t	*guidp = (const GUID_t *) data;

	gip = *gipp;
	return (memcmp (&gip->guid, guidp, sizeof (GUID_t)));
}

# endif

static void locators_add (LocatorList_t *llp, LocatorList_t slp, LocatorKind_t k)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;

	foreach_locator (slp, rp, np)
		if ((np->locator.kind & k) != 0)
			locator_list_copy_node (llp, np);
}

#ifdef DDS_SECURITY

/* rtps_secure_tunnel -- Check if the proxy connection needs a secure tunnel. */

static int rtps_secure_tunnel (Domain_t *dp, Participant_t *pp)
{
	unsigned	lcaps, rcaps;

	if (!dp->security)
		return (0);

	if (guid_local_component (&pp->p_guid_prefix)) {
		rcaps = pp->p_sec_caps >> SECC_LOCAL;
		lcaps = dp->participant.p_sec_caps >> SECC_LOCAL;
	}
	else {
		rcaps = pp->p_sec_caps & 0xffff;
		lcaps = dp->participant.p_sec_caps & 0xffff;
	}
	if ((rcaps & lcaps) != 0)
		return ((rcaps & lcaps & SECC_DDS_SEC) == 0 &&
			(rcaps & lcaps & SECC_DTLS_UDP) != 0);

#ifdef DDS_DEBUG
	/* TODO: Remove this and all related code once dtls/tls are fully working */
	else if (rtps_no_security)
		return (0);
#endif
	else
		return (-1);
}
#endif

/* add_relay_locators -- Add locators to reach the relays. */

static void add_relay_locators (Domain_t *dp, Proxy_t *prp, LocatorList_t *list)
{
	LocatorList_t	rlist;
	Participant_t	**p, *pp;
	unsigned	i;
	int		meta;

	lrloc_print3 ("RTPS: add relay locators: domain=%p, proxy=%p, locators=%p\r\n", 
						(void *) dp, (void *) prp, (void *) list);
	meta = (prp->endpoint->entity.flags & EF_BUILTIN) != 0;
	for (i = 0, p = dp->relays; i < dp->nr_relays; i++, p++) {
		pp = *p;
#ifdef DDS_SECURITY
		if (prp->tunnel) {
			if ((dp->kinds & LOCATOR_KINDS_UDP) != 0)
				locators_add (list, pp->p_sec_locs, LOCATOR_KINDS_UDP);
			if ((dp->kinds & ~LOCATOR_KINDS_UDP) != 0) {
				if (meta)
					rlist = pp->p_meta_ucast;
				else
					rlist = pp->p_def_ucast;
				locators_add (list, rlist, dp->kinds & ~LOCATOR_KINDS_UDP);
			}
		}
		else {
#endif
			if (meta)
				rlist = pp->p_meta_ucast;
			else
				rlist = pp->p_def_ucast;
			locators_add (list, rlist, dp->kinds);
#ifdef DDS_SECURITY
		}
#endif
	}
}

static void matched_reader_locators_set (RemReader_t *rrp)
{
	DiscoveredReader_t	*dr = (DiscoveredReader_t *) rrp->rr_endpoint;
	Domain_t		*dp;
	unsigned		kinds;

	dp = dr->dr_participant->p_domain;
	kinds = dp->kinds;
#ifdef DDS_SECURITY
	if (rrp->rr_tunnel && (kinds & LOCATOR_KINDS_UDP) != 0) {
		rrp->rr_mc_locs = NULL;
		locators_add (&rrp->rr_uc_locs, dr->dr_participant->p_sec_locs, LOCATOR_KINDS_UDP);
		kinds &= ~LOCATOR_KINDS_UDP;
	}
#endif
	if (dr->dr_mcast)
		locators_add (&rrp->rr_mc_locs, dr->dr_mcast, kinds);
	else if ((dr->dr_flags & EF_BUILTIN) != 0)
		locators_add (&rrp->rr_mc_locs, dr->dr_participant->p_meta_mcast, kinds);
	else
		locators_add (&rrp->rr_mc_locs, dp->participant.p_def_mcast, kinds);
	if (dr->dr_ucast)
		locators_add (&rrp->rr_uc_locs, dr->dr_ucast, kinds);
	else if ((dr->dr_flags & EF_BUILTIN) != 0)
		locators_add (&rrp->rr_uc_locs, dr->dr_participant->p_meta_ucast, kinds);
	else 
		locators_add (&rrp->rr_uc_locs, dr->dr_participant->p_def_ucast, kinds);

	if (dp->nr_relays)
		add_relay_locators (dp, &rrp->proxy, &rrp->rr_uc_locs);
}

static void matched_reader_locators_clear (RemReader_t *rrp)
{
	lrloc_print1 ("RTPS: matched_reader_locators_clear (%p)\r\n", (void *) rrp);
	if (rrp->rr_mc_locs)
		locator_list_delete_list (&rrp->rr_mc_locs);
	if (rrp->rr_uc_locs)
		locator_list_delete_list (&rrp->rr_uc_locs);
}

#ifdef RTPS_CHECK_LOCS
#define	CHECK_R_LOCATORS(rrp, s) log_printf (RTPS_ID, 0, "@%s: Locator list -> {%u} is %s!\r\n", \
					s, rrp->rr_endpoint->entity.handle, \
					(rrp->rr_uc_locs || rrp->rr_mc_locs) ? "filled" : "empty")
#define	CHECK_W_LOCATORS(rwp, s) log_printf (RTPS_ID, 0, "@%s: Locator list -> {%u} is %s!\r\n", \
					s, rwp->rw_endpoint->entity.handle, \
					(rwp->rw_uc_locs || rwp->rw_mc_locs) ? "filled" : "empty")
#else
#define	CHECK_R_LOCATORS(rrp, s)
#define	CHECK_W_LOCATORS(rwp, s)
#endif

static void matched_reader_locators_update (RemReader_t *rrp)
{
	lrloc_print1 ("RTPS: matched_reader_locators_update (%p)\r\n", (void *) rrp);
	CHECK_R_LOCATORS (rrp, "rtps_matched_reader_locators_update(begin)");
	matched_reader_locators_clear (rrp);
	matched_reader_locators_set (rrp);
	CHECK_R_LOCATORS (rrp, "rtps_matched_reader_locators_update(end)");
	rrp->rr_uc_dreply = NULL;
}

/* rtps_matched_reader_add -- Add a proxy reader to a stateful writer. */

int rtps_matched_reader_add (Writer_t *w, DiscoveredReader_t *dr)
{
	WRITER			*wp;
	RemReader_t		*rrp;
	RRType_t		type;
	char            buffer[32];
	const TypeSupport_t	*ts;
#ifdef DDS_SECURITY
	int			tunnel;
#endif

	ctrc_printd (RTPS_ID, RTPS_W_PROXY_ADD, &w, sizeof (w));
	prof_start (rtps_w_proxy_add);
	if (rtps_log)
        	log_printf (RTPS_ID, 0, "RTPS: matched reader add (%s) to %s.\r\n",
                        str_ptr (w->w_topic->name), guid_prefix_str(&dr->dr_participant->p_guid_prefix,buffer));
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_matched_reader_add: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!wp->endpoint.stateful)	/* Not applicable in stateful mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!dr)
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (wp->rem_readers, rrp)
		if ((DiscoveredReader_t *) rrp->rr_endpoint == dr)
			return (DDS_RETCODE_PRECONDITION_NOT_MET);

#ifdef DDS_SECURITY
	tunnel = rtps_secure_tunnel (w->w_publisher->domain, dr->dr_participant);
	if (tunnel < 0)
		return (DDS_RETCODE_BAD_PARAMETER);
#endif
	if ((rrp = mds_pool_alloc (&rtps_mem_blocks [MB_REM_READER])) == NULL) {
		log_printf (RTPS_ID, 0, "rtps_matched_reader_add: no memory for proxy reader.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (rrp, 0, sizeof (RemReader_t));
	/*rrp->rr_next_active = rrp->rr_prev_active = NULL;
	rrp->rr_active = 0; */
	rrp->rr_writer = wp;
#ifdef DDS_SECURITY
	if (tunnel) {
		rrp->rr_no_mcast = 1;
		wp->no_mcast = 1;
	}
	else
#endif
		if (dr->dr_participant->p_no_mcast) {
			rrp->rr_no_mcast = 1;
			wp->no_mcast = 1;
		}

	LIST_INIT (rrp->rr_changes);
	/* rrp->rr_changes.nchanges = 0;
	rrp->rr_unsent_changes = rrp->rr_requested_changes = NULL;*/
	rrp->rr_inline_qos = (dr->dr_flags & EF_INLINE_QOS) != 0;
	rrp->rr_reliable = dr->dr_qos->qos.reliability_kind;
	ts = dr->dr_topic->type->type_support;
	if ((w->w_flags & EF_BUILTIN) != 0 && ts->ts_prefer == MODE_PL_CDR)
		rrp->rr_marshall = 1;
	else if (ts->ts_dynamic || ts->ts_length > 512)		  
		rrp->rr_marshall = 1;
	else if (dr->dr_participant->p_vendor_id [0] != VENDORID_H_TECHNICOLOR ||
		 dr->dr_participant->p_vendor_id [1] != VENDORID_L_TECHNICOLOR)
		rrp->rr_marshall = 1;
	else
		rrp->rr_marshall = MARSHALL (dr->dr_participant->p_guid_prefix);
	if (rrp->rr_marshall)
		wp->mc_marshall = 1;
	rrp->rr_is_writer = 1;
	rrp->rr_endpoint = &dr->dr_ep;
	rrp->rr_next_guid = dr->dr_rtps;
	dr->dr_rtps = rrp;
	/*rrp->rr_last_ack = 0;
	rrp->rr_mc_locs = rrp->rr_uc_locs = NULL;
	rrp->rr_uc_dreply = NULL;*/

#ifdef DDS_SECURITY
	rrp->rr_tunnel = tunnel;
#endif

	/* Add all locators to the proxy in order to reach the endpoint. */
	matched_reader_locators_set (rrp);
	CHECK_R_LOCATORS (rrp, "rtps_matched_reader_add()");

#ifdef RTPS_OPT_MCAST
	if (wp->endpoint.mc_locators)
		locator_list_delete_list (&wp->endpoint.mc_locators);
#endif
#ifdef DUMP_LOCATORS
	dbg_printf ("W{%u}->DR{%u} ", w->w_handle, dr->dr_handle);
	dbg_printf ("MC:");
	locator_list_dump (rrp->rr_mc_locs);
	dbg_printf (";UC:");
	locator_list_dump (rrp->rr_uc_locs);
	dbg_printf (";\r\n");
#endif
	/*rrp->rr_head = rrp->rr_tail = NULL;
	rrp->rr_link = NULL;*/
	remote_reader_add (wp, rrp);
	type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
	if (rtps_rr_event [type]->start)
		(*rtps_rr_event [type]->start) (rrp);

	/* Start monitoring cache changes if not already enabled. */
	if (!wp->endpoint.cache_act) {
		wp->endpoint.cache_act = 1;
		hc_monitor_start (wp->endpoint.endpoint->cache, (uintptr_t) wp);
	}
	prof_stop (rtps_w_proxy_add, 1);
	return (DDS_RETCODE_OK);
}

/* rtps_matched_reader_remove -- Remove a proxy reader from a stateful writer.

   Locks: On entry, the writer, its publisher, topic and domain should be
   locked (as statemachine->finish is called.) */

int rtps_matched_reader_remove (Writer_t *w, DiscoveredReader_t *dr)
{
	WRITER		*wp;
	RemReader_t	*rrp, *xrrp, *prev_rrp;
	RRType_t	type;
	char        buffer[32];

	ctrc_printd (RTPS_ID, RTPS_W_PROXY_REMOVE, &w, sizeof (w));
	prof_start (rtps_w_proxy_rem);

	if (rtps_log)
        	log_printf (RTPS_ID, 0, "RTPS: matched reader remove (%s) to %s.\r\n",
                        str_ptr (w->w_topic->name), guid_prefix_str(&dr->dr_participant->p_guid_prefix,buffer));


	/*log_printf ("Matched reader remove!\r\n");*/
	wp = w->w_rtps;
	if (!wp) {
		/* Already deleted -- no need for warnings! */
		/*log_printf (RTPS_ID, 0, "rtps_matched_reader_remove: writer(%u) doesn't exist!\r\n", w);*/
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!wp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (wp->rem_readers, rrp)
		if ((DiscoveredReader_t *) rrp->rr_endpoint == dr)
			break;

	if (LIST_END (wp->rem_readers, rrp)) {
		log_printf (RTPS_ID, 0, "rtps_matched_reader_remove: destination doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Signal state machine to cleanup, since we're done. */
	type = rr_type (wp->endpoint.stateful, rrp->rr_reliable);
	if (rtps_rr_event [type]->finish)
		(*rtps_rr_event [type]->finish) (rrp);

	if (!dr->dr_rtps) /* We got interrupted & someone removed the proxy! */
		return (DDS_RETCODE_OK);

	/* Cleanup resources. */
	for (xrrp = (RemReader_t *) dr->dr_rtps, prev_rrp = NULL;
	     xrrp && xrrp != rrp;
	     prev_rrp = xrrp, xrrp = (RemReader_t *) xrrp->rr_next_guid)
		;

	if (!xrrp) /* We got interrupted & someone removed the proxy! */
		return (DDS_RETCODE_OK);

	if (prev_rrp)
		prev_rrp->rr_next_guid = rrp->rr_next_guid;
	else
		dr->dr_rtps = rrp->rr_next_guid;

	/* Cleanup the locators. */
#ifdef RTPS_OPT_MCAST
	if (wp->endpoint.mc_locators)
		locator_list_delete_list (&wp->endpoint.mc_locators);
#endif
	matched_reader_locators_clear (rrp);

	/* Remove from Writer's list. */
	remote_reader_remove (wp, rrp);

	/* If this was the last remote reader locator, stop monitoring the
	   cache changes. */
	wp->no_mcast = wp->mc_marshall = 0;
	if (LIST_EMPTY (wp->rem_readers)) {
		wp->endpoint.cache_act = 0;
		hc_monitor_end (wp->endpoint.endpoint->cache);
	}
	else {
		LIST_FOREACH (wp->rem_readers, xrrp) {
			if (xrrp->rr_no_mcast)
				wp->no_mcast = 1;
			if (xrrp->rr_marshall)
				wp->mc_marshall = 1;
		}
	}

	/* Free the context. */
	mds_pool_free (&rtps_mem_blocks [MB_REM_READER], rrp);

	prof_stop (rtps_w_proxy_rem, 1);
	return (DDS_RETCODE_OK);
}

/* rtps_matched_reader_restart -- Restart a matched, i.e. proxy reader for a
				  stateful writer. */

int rtps_matched_reader_restart (Writer_t *w, DiscoveredReader_t *dr)
{
#ifdef RTPS_PROXY_INST
	WRITER		*wp;
	RemReader_t	*rrp;

	wp = w->w_rtps;
	if (!wp) {
		/* Already deleted -- no need for warnings! */
		/*log_printf (RTPS_ID, 0, "rtps_matched_reader_restart: writer(%u) doesn't exist!\r\n", w);*/
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!wp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (wp->rem_readers, rrp)
		if ((DiscoveredReader_t *) rrp->rr_endpoint == dr)
			break;

	if (LIST_END (wp->rem_readers, rrp)) {
		log_printf (RTPS_ID, 0, "rtps_matched_reader_restart: destination doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!rrp->rr_reliable) {
		log_printf (RTPS_ID, 0, "rtps_matched_reader_restart: destination is not reliable!\r\n");
     		return (DDS_RETCODE_UNSUPPORTED);
	}
	sfw_restart (rrp);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (w)
	ARG_NOT_USED (dr)

	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

/* rtps_writer_matches -- Return a non-0 result if the local Writer matches the
			  remote reader. */

int rtps_writer_matches (Writer_t *w, DiscoveredReader_t *dr)
{
	WRITER		*wp;
	RemReader_t	*rrp;

	if (!dr->dr_rtps)
		return (0);

	wp = w->w_rtps;
	if (!wp)
		return (0);

	for (rrp = (RemReader_t *) dr->dr_rtps;
	     rrp;
	     rrp = (RemReader_t *) rrp->rr_next_guid)
		if (rrp->rr_writer == wp)
			return (1);

	return (0);
}

/* rtps_matched_reader_count -- Return the number of matched readers. */

unsigned rtps_matched_reader_count (Writer_t *w)
{
	WRITER		*wp;

	wp = w->w_rtps;
	if (!wp)
		return (0);

	return (wp->rem_readers.count);
}

/* rtps_notify -- Callback function to cleanup RTPS resources. */

static void rtps_notify (NOTIF_DATA *np)
{
	Change_t	*cp;
	DB		*dp;
	String_t	*sp;

	switch (np->type) {
		case NT_CACHE_FREE:
			cp = (Change_t *) np->arg;
			TRC_CHANGE (cp, "rtps_notify", 0);
			hc_change_free (cp);
			break;
		case NT_DATA_FREE:
			dp = (DB *) np->arg;
			db_free_data (dp);
			break;
		case NT_STR_FREE:
			sp = (String_t *) np->arg;
			str_unref (sp);
			break;
		default:
			warn_printf ("rtps_notify: invalid notification type (%u)!", np->type);
			break;
	}
}

#ifndef RTPS_OPT_MCAST

/* best_locator -- Return the best locator from a locator chain. */

static INLINE LocatorList_t best_locator (LocatorList_t list, LocatorKind_t kind)
{
	LocatorList_t	lp;

	for (lp = list; lp; lp = lp->next)
		if (lp->data->locator.kind == kind)
			return (lp);

	return (list);
}

/* writer_best_locator -- Return the best locator for a proxy context. */

static void *writer_best_locator (Domain_t *dp, Proxy_t *pp, RMBUF *mp, int *dlist)
{
	RemReader_t	*rrp;
	Participant_t	*p;
	RME		*mep;
	void		*dest;

	rrp = proxy2rr (pp);
	if (pp->u.writer->endpoint.stateful) {
		*dlist = 1;
		if ((mp->element.flags & RME_MCAST) != 0) {
			if (rrp->rr_mc_locs)
				dest = best_locator (rrp->rr_mc_locs, rtps_mux_mode);
			else if (rrp->rr_uc_locs)
				dest = best_locator (rrp->rr_uc_locs, rtps_mux_mode);
			else
				dest = NULL;
		}
		else if (rrp->rr_uc_locs)
			dest = best_locator (rrp->rr_uc_locs, rtps_mux_mode);
		else if (rrp->rr_mc_locs)
			dest = best_locator (rrp->rr_mc_locs, rtps_mux_mode);
		else
			dest = NULL;
	}
	else if ((mp->element.flags & RME_MCAST) == 0 &&
		 (mep = mp->first) != NULL &&
		 (mep->flags & RME_HEADER) != 0 &&
		 mep->header.id == ST_INFO_DST &&
		 (p = participant_lookup (dp, (GuidPrefix_t *) mep->data)) != NULL &&
		 p->p_src_locators) {
		*dlist = 1;
		dest = best_locator (p->p_src_locators, rtps_mux_mode);
	}
	else {
		*dlist = 0;
		dest = &rrp->rr_locator->locator;
	}
	return (dest);
}

/* reader_best_locator -- Return the best locator for a proxy context. */

static void *reader_best_locator (Proxy_t *pp, int mcast, int *dlist)
{
	RemWriter_t	*rwp;
	void		*dest;

	rwp = proxy2rw (pp);
	*dlist = 1;
	if (mcast) {
		if (rwp->rw_mc_locs)
			dest = best_locator (rwp->rw_mc_locs, rtps_mux_mode);
		else if (rwp->rw_uc_locs)
			dest = best_locator (rwp->rw_uc_locs, rtps_mux_mode);
		else
			dest = NULL;
	}
	else if (rwp->rw_uc_locs)
		dest = best_locator (rwp->rw_uc_locs, rtps_mux_mode);
	else if (rwp->rw_mc_locs)
		dest = best_locator (rwp->rw_mc_locs, rtps_mux_mode);
	else
		dest = NULL;

	return (dest);
}

#else

/* fitting_locator -- Return the first fitting locator from a locator chain. */

static inline LocatorList_t fitting_locator (LocatorList_t list,
					     LocatorKind_t kind,
					     int           *is_list)
{
	LocatorList_t	lp;

	for (lp = list; lp; lp = lp->next)
		if (lp->data->locator.kind == kind) {
			*is_list = 0;
			return ((LocatorList_t) &lp->data->locator);
		}

	*is_list = 1;
	return (list);
}

/* proxy_locator_list -- Return a locator list of a proxy context. */

static inline LocatorList_t proxy_locator_list (int writer, Proxy_t *pp, int mc)
{
	RemReader_t	*rrp;
	RemWriter_t	*rwp;
	LocatorList_t	llist;

	if (writer) {
		rrp = proxy2rr (pp);
		if (mc)
			if (rrp->rr_no_mcast || !rrp->rr_mc_locs)
				llist = NULL;
			else
				llist = rrp->rr_mc_locs;
		else
			llist = rrp->rr_uc_locs;
	}
	else {
		rwp = proxy2rw (pp);
		if (mc)
			if (rwp->rw_no_mcast || !rwp->rw_mc_locs)
				llist = NULL;
			else
				llist = rwp->rw_mc_locs;
		else
			llist = rwp->rw_uc_locs;
	}
	return (llist);
}

#define	MAX_COUNT_LOCS	16

typedef struct loc_ref_st {
	LocatorNode_t	*node;
	unsigned	count;
} LocRef_t;


/* optimal_mcast_locators -- Derive the optimal list of Unicast and Multicast
			     locators to send from a given endpoint, so that
			     all proxies can be reached with as few packets as
			     possible. */

static LocatorList_t optimal_mcast_locators (int writer, void *ep)
{
	LocatorList_t	dlist = NULL, llist;
	LocatorNode_t	*np;
	LocatorRef_t	*p;
	LocRef_t	*lrp;
	Proxy_t		*head, *tail, *pp, *prev;
	WRITER		*wp = (WRITER *) ep;
	READER		*rp = (READER *) ep;
	RemReader_t	*rrp;
	RemWriter_t	*rwp;
	unsigned	nlps, maxlps, mcindex, i;
	int		mcmax;
	size_t		size;
	LocRef_t	*lpp, *nlpp, lbuf [MAX_COUNT_LOCS];

	head = NULL;
	if (writer) {
		LIST_FOREACH (wp->rem_readers, rrp) {
			if (head)
				tail->link = &rrp->proxy;
			else
				head = &rrp->proxy;
			tail = &rrp->proxy;
		}
	}
	else {
		LIST_FOREACH (rp->rem_writers, rwp) {
			if (head)
				tail->link = &rwp->proxy;
			else
				head = &rwp->proxy;
			tail = &rwp->proxy;
		}
	}
	if (!head)
		return (NULL);

	tail->link = NULL;
	nlps = 0;
	maxlps = MAX_COUNT_LOCS;
	lpp = lbuf;

	/* Derive suitable multicast addresses for transmission. */
	do {
		mcmax = -1;
		mcindex = 0;

		/* Process Multicast list of each proxy. */
		for (pp = head; pp; pp = pp->link) {
			llist = proxy_locator_list (writer, pp, 1);
			if (!llist)
				continue;

			/* For each multicast locator found: */
			foreach_locator (llist, p, np) {

				/* Check if locator already in list. */
				for (i = 0, lrp = lpp; i < nlps; i++, lrp++)
					if (lrp->node == np)
						break;

				if (i < nlps) { /* Found it: increment count! */
					lrp->count++;
					if (mcmax < 0 ||
					     lrp->count > lpp [mcindex].count) {
						mcindex = i;
						mcmax = lrp->count;
					}
				}
				else {	/* Add locator to list. */
					if (nlps == maxlps) { /* Enlarge store. */
						size = (maxlps + 16) * sizeof (LocRef_t);
						if (maxlps == MAX_COUNT_LOCS)
							nlpp = xmalloc (size);
						else
							nlpp = xrealloc (lpp, size);
						if (!nlpp) {
							warn_printf ("optimal_mcast_locators: out of memory!");
							if (maxlps > MAX_COUNT_LOCS)
								xfree (lpp);
							return (dlist);
						}
						if (maxlps == MAX_COUNT_LOCS)
							memcpy (nlpp, lpp, sizeof (lbuf));

						lpp = nlpp;
						maxlps += 16;
					}
					lrp = &lpp [nlps++];
					lrp->node = np;
					lrp->count = 0;
				}
			}
		}
		if (mcmax < 0)	/* No locator found?  Exit! */
			break;

		/* Add locator with highest use count to destination list. */
		dbg_printf ("opt:add %s*%u -> ", locator_str (&lpp [mcindex].node->locator),
						lpp [mcindex].node->users);
		if (locator_list_copy_node (&dlist, lpp [mcindex].node)) {
			warn_printf ("optimal_mcast_locators: out of memory!");
			if (maxlps > MAX_COUNT_LOCS)
				xfree (lpp);
			return (dlist);
		}
		dbg_printf ("%u\r\n", lpp [mcindex].node->users);

		/* Remove selected proxies containing the selected locator. */
		for (pp = head, prev = NULL; pp; pp = pp->link) {
			llist = proxy_locator_list (writer, pp, 1);
			if (!llist)
				p = NULL;
			else {

				/* Check each multicast locator. */
				foreach_locator (llist, p, np)
					if (np == lpp [mcindex].node)
						break;
			}
			if (!p) {	/* Not found: check next proxy. */
				prev = pp;
				pp = pp->link;
				continue;
			}

			/* Found it -- remove proxy from list. */
			if (prev)
				prev->link = pp->link;
			else
				head = pp->link;
		}
	}
	while (head);

	/* No longer a need for the locator buffer. */
	if (maxlps > MAX_COUNT_LOCS)
		xfree (lpp);

	/* If not all proxies processed yet : append unicast destinations of
	   those proxies that are still remaining. */
	for (pp = head; pp; pp = pp->link) {
		if (pp->uc_dreply) {
			if (locator_list_copy_node (&dlist, pp->uc_dreply)) {
				warn_printf ("optimal_mcast_locators: out of memory!");
				return (dlist);
			}
		}
		else {
			llist = proxy_locator_list (writer, pp, 0);
			locator_list_append (&dlist, llist);
		}
	}
	return (llist);
}

/* writer_best_locator -- Return the best locator(s) for a proxy context. */

static void *writer_best_locator (Domain_t *dp, Proxy_t *pp, int mcast, int *dlist)
{
	RemReader_t	*rrp;
	void		*dest;

	ARG_NOT_USED (dp)

	rrp = proxy2rr (pp);
	if (pp->u.writer->endpoint.stateful) {
		*dlist = 1;
		if (mcast) {
			if (!pp->u.writer->endpoint.mc_locators)
				pp->u.writer->endpoint.mc_locators = 
				     optimal_mcast_locators (1, pp->u.writer);
			dest = pp->u.writer->endpoint.mc_locators;
		}
		else if (rrp->rr_uc_locs)
			dest = rrp->rr_uc_locs;
		else if (rrp->rr_mc_locs)
			dest = fitting_locator (rrp->rr_mc_locs, rtps_mux_mode, dlist);
		else
			dest = NULL;
	}
	else {
		*dlist = 0;
		dest = &rrp->rr_locator;
	}
	return (dest);
}

/* reader_best_locator -- Return the best locator for a proxy context. */

static void *reader_best_locator (Proxy_t *pp, int mcast, int *dlist)
{
	RemWriter_t	*rwp;
	void		*dest;

	rwp = proxy2rw (pp);
	*dlist = 1;
	if (mcast) {
		if (!pp->u.reader->endpoint.mc_locators)
				pp->u.reader->endpoint.mc_locators = 
				     optimal_mcast_locators (0, pp->u.reader);
		dest = pp->u.reader->endpoint.mc_locators;
	}
	else if (rwp->rw_uc_locs)
		dest = rwp->rw_uc_locs;
	else if (rwp->rw_mc_locs)
		dest = fitting_locator (rwp->rw_mc_locs, rtps_mux_mode, dlist);
	else
		dest = NULL;

	return (dest);
}

#endif /* !RTPS_OPT_MCAST */

#define	ADD_CHUNK(dp,sp,s,n,dmax,total)	n = s; if (n > max) n = max;	\
					memcpy (dp, sp, n);		\
					dp += n; dmax -= n; total += n;	\
					if (!dmax) return (total)
	
static unsigned msg_put (unsigned char *dp, RMBUF *mp, unsigned max)
{
	RME		*mep;
	unsigned	n, total = 0;

	ADD_CHUNK (dp, &mp->header, sizeof (mp->header), n, max, total);
	for (mep = mp->first; mep; mep = mep->next)
		if ((mep->flags & RME_HEADER) != 0) {
			if (mep->data == mep->d) {
				ADD_CHUNK (dp, &mep->header,
					   sizeof (mep->header) + mep->length,
					   n, max, total);
			}
			else {
				ADD_CHUNK (dp, &mep->header, sizeof (mep->header),
								   n, max, total);
				ADD_CHUNK (dp, mep->data, mep->length,
								   n, max, total);
			}
		}
		else {
			ADD_CHUNK (dp, mep->data, mep->length, n, max, total);
		}
	return (total);
}


static void rtps_send_messages (unsigned id, void *dest, int dlist, RMBUF *mp)
{
	TRANSMITTER     *txp = &rtps_transmitter;

	prof_start (rtps_send_msgs);

	/* Message list effectively dequeued from proxy and proxy
	   in an inactive state, new messages can now be added while
	   we transmit the message list. */
#ifdef RTPS_LOG_MSGS
	if (mp) {
		log_printf (RTPS_ID, 0, "==> ");
		if (!dest)
			log_printf (RTPS_ID, 0, "<no destination>");
		else if (dlist)
			locator_list_log (RTPS_ID, 0, dest);
		else
			log_printf (RTPS_ID, 0, "%s", locator_str ((Locator_t *) dest));
		log_printf (RTPS_ID, 0, "\r\n");
		rtps_log_message (RTPS_ID, 0, mp, 't', 0);
	}
#endif
	if (dest) {
		ctrc_printd (RTPS_ID, RTPS_SCH_TX, &dest, sizeof (dest));
		rtps_locator_send (id, dest, dlist, mp);
		rtps_transmitter.nlocsend++;
		ctrc_printd (RTPS_ID, RTPS_SCH_TXD, &dest, sizeof (dest));
	}
	else if (mp) {

		/* No destination + messages to transmit.
		   Cleanup the messages. */
		warn_printf ("rtps_send_changes: no route to remote reader!");
		txp->last_error = T_NO_DEST;
		txp->no_locator++;

		/* Copy message to message buffer. */
		txp->msg_size = msg_put (txp->msg_buffer, mp, MAX_TX_HEADER);

		/* Free the messages. */
		rtps_free_messages (mp);
	}
	prof_stop (rtps_send_msgs, 1);
}

/* rtps_send_changes -- Send all messages that were as yet not sent. */

void rtps_send_changes (void)
{
	Proxy_t		*pp;
	WRITER		*wp;
	Writer_t	*w;
	READER		*rp;
	Reader_t	*r;
	RMBUF		*mp;
	Domain_t	*dp;
	RRType_t	type;
	void		*dest;
	int		dlist;
	TRANSMITTER	*txp = &rtps_transmitter;
	PROF_ITER	(n);

	prof_start (rtps_do_changes);
	rtps_rx_active = 1;
	for (pp = get_first_proxy (); pp; pp = get_next_proxy ()) {
		PROF_INC (n);

		dest = NULL;
		mp = NULL;
		if (pp->is_writer) {

			/* Writer proxy message handling. */
			wp = pp->u.writer;
			w = (Writer_t *) (wp->endpoint.endpoint);
			if (!w) 
				continue;

			ctrc_printd (RTPS_ID, RTPS_SCH_W_PREP, &w, sizeof (w));
			dp = w->w_publisher->domain;
			lock_take (w->w_lock);

			/* Create the RTPS DATA messages to transmit, and
			   queue them into the proxy context. */
			type = rr_type (wp->endpoint.stateful, pp->reliable);
			if ((*rtps_rr_event [type]->send_now) (proxy2rr (pp))) {

				/* Out of memory error - retry later. */
				txp->last_error = T_NOMEM;
				txp->msg_size = 0;
				txp->no_memory++;
			}
			else {
				if ((mp = pp->head) != NULL) {
					pp->tail->next = NULL;
					pp->head = NULL;	/* Detach messages from proxy. */
#ifdef RTPS_TRACE
					if (wp->endpoint.trace_frames)
						mp->element.flags |= RME_TRACE;
#endif
					if (pp->uc_dreply &&
					    (mp->element.flags & RME_MCAST) == 0) {
						dest = &pp->uc_dreply->locator;
						dlist = 0;
					}
					else
						dest = writer_best_locator (dp,
									    pp, 
									    mp,
									    &dlist);
				}
				else
					dlist = 0;

				/* Reset proxy context. */
				proxy_deactivate (pp);
				if (mp) {
					if ((w->w_flags & ENTITY_KIND_BUILTIN) == 0)
						mp->element.flags |= RME_USER;
					rtps_send_messages (w->w_publisher->domain->index,
							    dest, dlist, mp);
				}
			}
			lock_release (w->w_lock);
		}
		else {
			rp = pp->u.reader;
			r = (Reader_t *) (rp->endpoint.endpoint);
			ctrc_printd (RTPS_ID, RTPS_SCH_RDR, &r, sizeof (r));
			lock_take (r->r_lock);
			if ((mp = pp->head) != NULL) {
				pp->tail->next = NULL;
				pp->head = NULL;	/* Detach messages from proxy. */
#ifdef RTPS_TRACE
				if (rp->endpoint.trace_frames)
					mp->element.flags |= RME_TRACE;
#endif
				if (pp->uc_dreply &&
				    (mp->element.flags & RME_MCAST) == 0) {
					dest = &pp->uc_dreply->locator;
					dlist = 0;
				}
				else
					dest = reader_best_locator (pp,
							mp->element.flags & RME_MCAST,
							&dlist);
			}
			else
				dlist = 0;

			/* Reset proxy context. */
			proxy_deactivate (pp);
			if (mp) {
				if ((r->r_flags & ENTITY_KIND_BUILTIN) == 0)
					mp->element.flags |= RME_USER;
				rtps_send_messages (r->r_subscriber->domain->index,
						    dest, dlist, mp);
			}
			lock_release (r->r_lock);
		}
	}

	/* If there was an out-of-memory condition, just retry. */
	if (get_first_proxy ())
		dds_signal (DDS_EV_PROXY_NE);

	rtps_rx_active = 0;
	prof_stop (rtps_do_changes, n);
}

/* rtps_reader_create -- Create an RTPS reader with the given parameters. */

int rtps_reader_create (Reader_t              *r,
			int		      stateful,
			const Duration_t      *heartbeat_resp,
			const Duration_t      *heartbeat_supp)
{
	READER		*rp;
#ifdef RTPS_TRACE
	unsigned	dtrace;
#endif

	ctrc_printd (RTPS_ID, RTPS_R_CREATE, &r, sizeof (r));
	prof_start (rtps_r_create);

	/* Set defaults for unspecified durations. */
	if (!heartbeat_resp)
		heartbeat_resp = &rtps_def_hb_resp;
	if (!heartbeat_supp)
		heartbeat_supp = &rtps_def_hb_supp;

	/* Allocate required memory. */
	if ((rp = mds_pool_alloc (&rtps_mem_blocks [MB_READER])) == NULL) {
		warn_printf ("rtps_reader_create: out of memory for reader context!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Connect RTPS-specific endpoint context to Local Endpoint context. */
	rp->endpoint.endpoint = &r->r_lep;
	r->r_rtps = rp;

	/* Setup entity fields. */
	rp->endpoint.is_reader = 1;
	rp->endpoint.reliability = (r->r_qos->qos.reliability_kind == 
						DDS_RELIABLE_RELIABILITY_QOS);
	rp->endpoint.stateful = stateful;
	rp->endpoint.multi_inst = r->r_topic->type->type_support->ts_keys;
	rp->endpoint.cache_act = 0;
	if (r->r_qos->qos.resource_limits.max_samples == DDS_LENGTH_UNLIMITED)
		rp->endpoint.limit_rx = 0;
	else
		rp->endpoint.limit_rx = 1;
	if (r->r_qos->qos.history_kind == DDS_KEEP_ALL_HISTORY_QOS)
		rp->endpoint.cache_acks = 1;
	else
		rp->endpoint.cache_acks = 0;
	if ((r->r_topic->entity.flags & EF_FILTERED) != 0)
		rp->endpoint.cfilter_rx = 1;
	else
		rp->endpoint.cfilter_rx = 0;
	if (r->r_time_based_filter.minimum_separation.sec ||
	    r->r_time_based_filter.minimum_separation.nanosec)
		rp->endpoint.tfilter_rx = 1;
	else
		rp->endpoint.tfilter_rx = 0;
#ifdef RTPS_TRACE
	dtrace = rtps_def_trace (r->r_handle, str_ptr (r->r_topic->name));
	rp->endpoint.trace_frames = (dtrace & DDS_RTRC_FTRACE) != 0;
	rp->endpoint.trace_sigs   = (dtrace & DDS_RTRC_STRACE) != 0;
	rp->endpoint.trace_state  = (dtrace & DDS_RTRC_ETRACE) != 0;
	rp->endpoint.trace_tmr    = (dtrace & DDS_RTRC_TTRACE) != 0;
#ifdef RTPS_SEDP_TRACE
	if (r->r_entity_id.id [3] == (ENTITY_KIND_BUILTIN | ENTITY_KIND_READER_KEY) &&
	    r->r_entity_id.id [0] == 0 &&
	    (r->r_entity_id.id [1] == 0 || r->r_entity_id.id [1] == 2))
		rp->endpoint.trace_frames = rp->endpoint.trace_sigs =
		rp->endpoint.trace_state  = rp->endpoint.trace_tmr  = 1;
#endif
#ifdef RTPS_SPDP_TRACE
	if (r->r_entity_id.id [3] == (ENTITY_KIND_BUILTIN | ENTITY_KIND_READER_KEY) &&
	    r->r_entity_id.id [0] == 0 &&
	    r->r_entity_id.id [1] == 1)
		rp->endpoint.trace_frames = rp->endpoint.trace_sigs =
		rp->endpoint.trace_state  = rp->endpoint.trace_tmr  = 1;
#endif
#ifdef RTPS_CDD_TRACE
	if (r->r_entity_id.id [3] == (ENTITY_KIND_BUILTIN | ENTITY_KIND_READER_KEY) &&
	    r->r_entity_id.id [0] == 0 &&
	    r->r_entity_id.id [1] == 3)
		rp->endpoint.trace_frames = rp->endpoint.trace_sigs =
		rp->endpoint.trace_state  = rp->endpoint.trace_tmr  = 1;
#endif
#else
	rp->endpoint.trace_frames = rp->endpoint.trace_sigs =
	rp->endpoint.trace_state  = rp->endpoint.trace_tmr  = 0;
#endif
#ifdef RTPS_OPT_MCAST
	rp->endpoint.mc_locators = NULL;
#endif

	/* Setup reader fields. */
	rp->data_queued = 0;
	rp->heartbeat_resp_delay = duration2ticks (heartbeat_resp);
	rp->heartbeat_supp_dur = duration2ticks (heartbeat_supp);
	rp->acknacks = r->r_handle;
#ifdef RTPS_FRAGMENTS
	rp->nackfrags = rp->acknacks;
#endif

	/* Initialize the list of Proxy-Writers. */
	LIST_INIT (rp->rem_writers);
	rp->rem_writers.count = 0;

	prof_stop (rtps_r_create, 1);
	if (rtps_log)
		log_printf (RTPS_ID, 0, "RTPS: reader (%s) created.\r\n",
						str_ptr (r->r_topic->name));

	return (DDS_RETCODE_OK);
}

/* rtps_reader_delete -- Delete a previously created RTPS reader. */

int rtps_reader_delete (Reader_t *r)
{
	READER		*rp;
	RemWriter_t	*rwp;

	ctrc_printd (RTPS_ID, RTPS_R_DELETE, &r, sizeof (r));
	prof_start (rtps_r_delete);

	rp = (READER *) r->r_rtps;
	if (!rp || !r->r_rtps) {
		/* Already deleted -- no need for warnings! */
		/*log_printf (RTPS_ID, 0, "rtps_reader_delete: reader(%u) doesn't exist!\r\n", r);*/
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	log_printf (RTPS_ID, 0, "RTPS: reader (%s) delete.\r\n", str_ptr (r->r_topic->name));

	while (LIST_NONEMPTY (rp->rem_writers)) {
		rwp = LIST_HEAD (rp->rem_writers);
		rtps_matched_writer_remove (r, (DiscoveredWriter_t *) rwp->rw_endpoint);
	}
	r->r_rtps = NULL;
	mds_pool_free (&rtps_mem_blocks [MB_READER], rp);

	prof_stop (rtps_r_delete, 1);
	return (DDS_RETCODE_OK);
}

static void matched_writer_locators_set (RemWriter_t *rwp)
{
	DiscoveredWriter_t	*dw = (DiscoveredWriter_t *) rwp->rw_endpoint;
	Domain_t		*dp;
	unsigned		kinds;

	dp = dw->dw_participant->p_domain;
	kinds = dp->kinds & dw->dw_participant->p_domain->kinds;
#ifdef DDS_SECURITY
	if (rwp->rw_tunnel && (kinds & LOCATOR_KINDS_UDP) != 0) {
		rwp->rw_mc_locs = NULL;
		locators_add (&rwp->rw_uc_locs, dw->dw_participant->p_sec_locs, LOCATOR_KINDS_UDP);
		kinds &= ~LOCATOR_KINDS_UDP;
	}
#endif
	if (dw->dw_mcast)
		locators_add (&rwp->rw_mc_locs, dw->dw_mcast, kinds);
	else if ((dw->dw_flags & EF_BUILTIN) != 0)
		locators_add (&rwp->rw_mc_locs, dw->dw_participant->p_meta_mcast, kinds);
	else
		locators_add (&rwp->rw_mc_locs, dp->participant.p_def_mcast, kinds);
	if (dw->dw_ucast)
		locators_add (&rwp->rw_uc_locs, dw->dw_ucast, kinds);
	if ((dw->dw_flags & EF_BUILTIN) != 0)
		locators_add (&rwp->rw_uc_locs, dw->dw_participant->p_meta_ucast, kinds);
	else
		locators_add (&rwp->rw_uc_locs, dw->dw_participant->p_def_ucast, kinds);

	if (dp->nr_relays)
		add_relay_locators (dp, &rwp->proxy, &rwp->rw_uc_locs);
}

static void matched_writer_locators_clear (RemWriter_t *rwp)
{
	lrloc_print1 ("RTPS: matched_writer_locators_clear (%p)\r\n", (void *) rwp);
	if (rwp->rw_mc_locs)
		locator_list_delete_list (&rwp->rw_mc_locs);
	if (rwp->rw_uc_locs)
		locator_list_delete_list (&rwp->rw_uc_locs);
}

static void matched_writer_locators_update (RemWriter_t *rwp)
{
	lrloc_print1 ("RTPS: matched_writer_locators_update (%p)\r\n", (void *) rwp);
	CHECK_W_LOCATORS (rwp, "rtps_matched_writer_locators_update(begin)");
	matched_writer_locators_clear (rwp);
	matched_writer_locators_set (rwp);
	CHECK_W_LOCATORS (rwp, "rtps_matched_writer_locators_update(end)");
	rwp->rw_uc_dreply = NULL;
}

/* rtps_matched_writer_add -- Add a proxy writer to a stateful reader. */

int rtps_matched_writer_add (Reader_t *r, DiscoveredWriter_t *dw)
{
	READER		*rp;
	RemWriter_t	*rwp;
	RWType_t	type;
	char buffer[32];
#ifdef DDS_SECURITY
	int		tunnel;
#endif

	ctrc_printd (RTPS_ID, RTPS_R_PROXY_ADD, &r, sizeof (r));
	prof_start (rtps_r_proxy_add);

	if (rtps_log)
        	log_printf (RTPS_ID, 0, "RTPS: matched writer add (%s) to %s.\r\n",
                        str_ptr (r->r_topic->name), guid_prefix_str(&dw->dw_participant->p_guid_prefix,buffer));

	rp = (READER *) r->r_rtps;
	if (!rp) {
		log_printf (RTPS_ID, 0, "rtps_matched_writer_add: reader(%s) doesn't exist!\r\n", str_ptr (r->r_topic->name));
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!rp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef DDS_SECURITY
	tunnel = rtps_secure_tunnel (r->r_subscriber->domain, dw->dw_participant);
	if (tunnel < 0)
		return (DDS_RETCODE_BAD_PARAMETER);
#endif
	LIST_FOREACH (rp->rem_writers, rwp)
		if ((DiscoveredWriter_t *) rwp->rw_endpoint == dw)
			return (DDS_RETCODE_PRECONDITION_NOT_MET);

	if ((rwp = mds_pool_alloc (&rtps_mem_blocks [MB_REM_WRITER])) == NULL) {
		log_printf (RTPS_ID, 0, "rtps_matched_writer_add: no memory for proxy writer!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (rwp, 0, sizeof (RemWriter_t));
	/*rwp->rw_next_active = rwp->rw_prev_active = NULL;
	rwp->rw_active = 0;
	rwp->rw_head = rwp->rw_tail = NULL;
	rwp->rw_link = NULL;*/
	rwp->rw_reader = rp;
	/*rwp->rw_active = 0;*/
	rwp->rw_reliable = dw->dw_qos->qos.reliability_kind;
	/*rwp->rw_heartbeats = 0;
	rwp->rw_is_writer = 0;*/
	rwp->rw_endpoint = &dw->dw_ep;
	rwp->rw_next_guid = dw->dw_rtps;
	dw->dw_rtps = rwp;
	rwp->rw_seqnr_next.low = 1;
	/*rwp->rw_seqnr_next.high = 0;
	rwp->rw_last_hb = 0;
	rwp->rw_mc_locs = rwp->rw_uc_locs = NULL;
	rwp->rw_uc_dreply = NULL;*/
#ifdef DDS_SECURITY
	rwp->rw_tunnel = tunnel;
#endif

	/* Add all locators to the proxy in order to reach the endpoint. */
	matched_writer_locators_set (rwp);
	CHECK_W_LOCATORS (rwp, "rtps_matched_writer_add()");

#ifdef RTPS_OPT_MCAST
	if (rp->endpoint.mc_locators)
		locator_list_delete_list (&rp->endpoint.mc_locators);
#endif
#ifdef DUMP_LOCATORS
	dbg_printf ("R{%u}->DW{%u} ", r->r_handle, dw->dw_handle);
	dbg_printf ("MC:");
	locator_list_dump (rwp->rw_mc_locs);
	dbg_printf (";UC:");
	locator_list_dump (rwp->rw_uc_locs);
	dbg_printf (";\r\n");
#endif
	LIST_INIT (rwp->rw_changes);
	/*rwp->rw_changes.nchanges = 0;*/
	remote_writer_add (rp, rwp);
	type = rw_type (rwp->rw_reliable);
	if (rtps_rw_event [type]->start)
		(*rtps_rw_event [type]->start) (rwp);

	/* Start monitoring cache cache changes if not already enabled. */
	if (!rp->endpoint.cache_act) {
		rp->endpoint.cache_act = 1;
		hc_inform_start (rp->endpoint.endpoint->cache, (uintptr_t) rp);
	}
	prof_stop (rtps_r_proxy_add, 1);
	return (DDS_RETCODE_OK);
}

/* rtps_matched_writer_remove -- Remove a proxy writer from a stateful reader.*/

int rtps_matched_writer_remove (Reader_t *r, DiscoveredWriter_t *dw)
{
	READER		*rp;
	RemWriter_t	*rwp, *xrwp, *prev_rwp;
	RWType_t	type;
	char        buffer[32];

	ctrc_printd (RTPS_ID, RTPS_R_PROXY_REMOVE, &r, sizeof (r));
	prof_start (rtps_r_proxy_rem);

	if (rtps_log)
        	log_printf (RTPS_ID, 0, "RTPS: matched writer remove (%s) to %s.\r\n",
                        str_ptr (r->r_topic->name), guid_prefix_str(&dw->dw_participant->p_guid_prefix,buffer));


	/* log_printf ("Matched writer remove!\r\n");*/

	rp = (READER *) r->r_rtps;
	if (!rp) {
		/* Already deleted -- no need for warnings! */
		/*log_printf (RTPS_ID, 0, "rtps_matched_writer_remove: reader(%u) doesn't exist!\r\n", r);*/
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!rp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (rp->rem_writers, rwp)
		if ((DiscoveredWriter_t *) rwp->rw_endpoint == dw)
			break;

	if (LIST_END (rp->rem_writers, rwp)) {
		log_printf (RTPS_ID, 0, "rtps_matched_writer_remove: destination doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Signal state machine to cleanup, since we're done. */
	type = rw_type (rwp->rw_reliable);
	if (rtps_rw_event [type]->finish)
		(*rtps_rw_event [type]->finish) (rwp);

	/* Cleanup resources. */
	for (xrwp = dw->dw_rtps, prev_rwp = NULL;
	     xrwp && xrwp != rwp;
	     prev_rwp = xrwp, xrwp = (RemWriter_t *) xrwp->rw_next_guid)
		;
	if (xrwp) {
		if (prev_rwp)
			prev_rwp->rw_next_guid = rwp->rw_next_guid;
		else
			dw->dw_rtps = rwp->rw_next_guid;
	}

	/* Cleanup the locators. */
#ifdef RTPS_OPT_MCAST
	if (rp->endpoint.mc_locators)
		locator_list_delete_list (&rp->endpoint.mc_locators);
#endif
	matched_writer_locators_clear (rwp);

	/* Remove from Reader's list. */
	remote_writer_remove (rp, rwp);

	/* Free the context. */
	mds_pool_free (&rtps_mem_blocks [MB_REM_WRITER], rwp);

	/* If this was the last remote writer locator, stop monitoring the
	   cache changes. */
	if (LIST_EMPTY (rp->rem_writers) && rp->endpoint.cache_act) {
		rp->endpoint.cache_act = 0;
		hc_inform_end (rp->endpoint.endpoint->cache);
	}
	prof_stop (rtps_r_proxy_rem, 1);
	return (DDS_RETCODE_OK);
}

/* rtps_matched_writer_restart -- Restart a matched, i.e. proxy writer for a
				  stateful reader. */

int rtps_matched_writer_restart (Reader_t *r, DiscoveredWriter_t *dw)
{
#ifdef RTPS_PROXY_INST
	READER		*rp;
	RemWriter_t	*rwp;

	rp = r->r_rtps;
	if (!rp) {
		/* Already deleted -- no need for warnings! */
		/*log_printf (RTPS_ID, 0, "rtps_matched_writer_restart: reader(%u) doesn't exist!\r\n", w);*/
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!rp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (rp->rem_writers, rwp)
		if ((DiscoveredWriter_t *) rwp->rw_endpoint == dw)
			break;

	if (LIST_END (rp->rem_writers, rwp)) {
		log_printf (RTPS_ID, 0, "rtps_matched_writer_restart: destination doesn't exist!\r\n");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!rwp->rw_reliable) {
		log_printf (RTPS_ID, 0, "rtps_matched_writer_restart: destination is not reliable!\r\n");
     		return (DDS_RETCODE_UNSUPPORTED);
	}
	sfr_restart (rwp);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (r)
	ARG_NOT_USED (dw)

	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

/* rtps_reader_matches -- Return a non-0 result if the local Reader matches the
			  remote writer. */

int rtps_reader_matches (Reader_t *r, DiscoveredWriter_t *dw)
{
	READER		*rp;
	RemWriter_t	*rwp;

	if (!dw->dw_rtps)
		return (0);

	rp = r->r_rtps;
	if (!rp)
		return (0);

	for (rwp = (RemWriter_t *) dw->dw_rtps;
	     rwp;
	     rwp = (RemWriter_t *) rwp->rw_next_guid)
		if (rwp->rw_reader == rp)
			return (1);

	return (0);
}

/* rtps_matched_writer_count -- Return the number of matched writers. */

unsigned rtps_matched_writer_count (Reader_t *r)
{
	READER		*rp;

	rp = r->r_rtps;
	if (!rp)
		return (0);

	return (rp->rem_writers.count);
}

static int rtps_reader_unblock (uintptr_t user, Change_t *cp)
{
	READER		*rp = (READER *) user;
	RemWriter_t	*rwp;
	PROF_ITER	(n);

	ARG_NOT_USED (cp)

	ctrc_printd (RTPS_ID, RTPS_R_UNBLOCK, &user, sizeof (user));
	prof_start (rtps_r_unblock);

	if (!rp)
		/* Already deleted -- no need for warnings! */
		return (DDS_RETCODE_ALREADY_DELETED);

	if (!rp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (rp->rem_writers, rwp) {
		PROF_INC (n);
		if (rwp->rw_blocked) {
			rwp->rw_blocked = 0;
			sfr_process_samples (rwp);
		}
	}
	prof_stop (rtps_r_unblock, n);
	return (DDS_RETCODE_OK);
}

/* rtps_wait_data -- Wait for historical data. */

int rtps_wait_data (Reader_t *r, const Duration_t *wait)
{
	READER		*rp;
	RemWriter_t	*rwp;
	unsigned	not_done;
	Ticks_t		d, now, end_time;

	if (!r)
		return (DDS_RETCODE_OK);

	now = sys_getticks ();
	end_time = now + duration2ticks (wait);
	for (;;) {
		d = end_time - now;
		if (d >= 0x7fffffffUL)
			return (DDS_RETCODE_TIMEOUT);

		not_done = 0;
		lock_take (r->r_lock);
		rp = r->r_rtps;
		if (!rp) {
			lock_release (r->r_lock);
			break;
		}
		LIST_FOREACH (rp->rem_writers, rwp)
			if (rwp->rw_reliable &&
			    (!rwp->rw_heartbeats || rwp->rw_changes.nchanges))
				not_done++;
		lock_release (r->r_lock);
		if (!not_done)
			break;

		usleep (1000);
		now = sys_getticks ();
	}
	return (DDS_RETCODE_OK);
}

/* rtps_reader_flush -- Flush the usage of an instance by a reader. */

static void rtps_reader_flush (uintptr_t user, HCI hci)
{
	READER		*rp = (READER *) user;
	RemWriter_t	*rwp;
	CCREF		*crp;

	if (!rp || !rp->endpoint.stateful)
		return;

	LIST_FOREACH (rp->rem_writers, rwp)
		LIST_FOREACH (rwp->rw_changes, crp)
			if (crp->relevant && crp->u.c.hci == hci)
				crp->u.c.hci = 0;
}

#ifdef DDS_DEBUG

static int rtps_writer_assert (Writer_t *w)
{
	WRITER			*wp;
	RemReader_t		*rrp, *xrrp;
	DiscoveredReader_t	*dr;

	wp = w->w_rtps;
	if (!wp->endpoint.stateful)	/* Not applicable in stateless mode. */
		return (DDS_RETCODE_OK);

	/* Check for every discovered reader whether the RTPS proxy is valid. */
	LIST_FOREACH (wp->rem_readers, rrp) {
		dr = (DiscoveredReader_t *) rrp->rr_endpoint;
		for (xrrp = dr->dr_rtps;
		     xrrp && xrrp != rrp;
		     xrrp = (RemReader_t *) xrrp->rr_next_guid)
			;
		if (!xrrp)
			return (DDS_RETCODE_INCONSISTENT_POLICY);
	}
	return (DDS_RETCODE_OK);
}

static int rtps_reader_assert (Reader_t *r)
{
	READER			*rp;
	RemWriter_t		*rwp, *xrwp;
	DiscoveredWriter_t	*dw;

	rp = r->r_rtps;

	/* Check for every discovered reader whether the RTPS proxy is valid. */
	LIST_FOREACH (rp->rem_writers, rwp) {
		dw = (DiscoveredWriter_t *) rwp->rw_endpoint;
		for (xrwp = dw->dw_rtps;
		     xrwp && xrwp != rwp;
		     xrwp = (RemWriter_t *) xrwp->rw_next_guid)
			;
		if (!xrwp)
			return (DDS_RETCODE_INCONSISTENT_POLICY);
	}
	return (DDS_RETCODE_OK);
}

int rtps_endpoint_assert (LocalEndpoint_t *e)
{
	if (!e || !e->ep.rtps)
		return (DDS_RETCODE_OK);

	if (e->ep.entity.type == ET_WRITER)
		return (rtps_writer_assert ((Writer_t *) e));
	else if (e->ep.entity.type == ET_READER)
		return (rtps_reader_assert ((Reader_t *) e));
	else
		return (DDS_RETCODE_BAD_PARAMETER);
}

#endif

/* rtps_endpoint_add_locator -- Add a specific locator to an endpoint. */

int rtps_endpoint_add_locator (LocalEndpoint_t *e, int mcast, const Locator_t *loc)
{
	LocatorList_t	*lp;

	if (!e || !loc)
		return (DDS_RETCODE_BAD_PARAMETER);

	lp = (mcast) ? &e->ep.mcast : &e->ep.ucast;
	if (locator_list_search (*lp, loc->kind, loc->address, loc->port) >= 0)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	if (!locator_list_add (lp, loc->kind, loc->address, loc->port,
				  loc->scope_id, loc->scope, 0, 0))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	disc_endpoint_locator (e->ep.u.publisher->domain, e, 1, mcast, loc);
	return (DDS_RETCODE_OK);
}

/* rtps_endpoint_remove_locator -- Remove a locator from an endpoint. */

int rtps_endpoint_remove_locator (LocalEndpoint_t *e, int mcast, const Locator_t *loc)
{
	LocatorList_t	*lp;

	if (!e)
		return (DDS_RETCODE_BAD_PARAMETER);

	lp = (mcast) ? &e->ep.mcast : &e->ep.ucast;
	if (!loc)
		locator_list_delete_list (lp);
	else
		locator_list_delete (lp, loc->kind, loc->address, loc->port);

	disc_endpoint_locator (e->ep.u.publisher->domain, e, 0, mcast, loc);
	return (DDS_RETCODE_OK);
}

/* rtps_endpoint_locators_update -- Update the locators of the given remote
				    endpoint. */

void rtps_endpoint_locators_update (Endpoint_t *r, int mcast)
{
	RemReader_t	*rrp;
	RemWriter_t	*rwp;

	/*dbg_printf ("rtps_endpoint_locators_update (%p, %u);\r\n", r, mcast);*/
	if (lock_take (r->topic->lock)) {
		warn_printf ("endpoint_locators_update: topic lock error");
		return;
	}
#ifdef RTPS_OPT_MCAST
	if (pp && pp->u.reader->endpoint.mc_locators)
		locator_list_delete_list (&pp->u.reader->endpoint.mc_locators);
#endif
	if (!mcast) {
		if (entity_writer (entity_type (&r->entity))) {
			rwp = (RemWriter_t *) r->rtps;
			for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid) {
				rwp->rw_uc_dreply = NULL;
				rwp->rw_ir_locs = 0;
				matched_writer_locators_update (rwp);
			}
		}
		else {
			rrp = (RemReader_t *) r->rtps;
			for (; rrp; rrp = (RemReader_t *) rrp->rr_next_guid) {
				rrp->rr_uc_dreply = NULL;
				rrp->rr_ir_locs = 0;
				matched_reader_locators_update (rrp);
			}
		}
	}
	lock_release (r->topic->lock);
}

/* rtps_endpoint_locality_update -- Update the locality of the given remote
				    endpoint. */

void rtps_endpoint_locality_update (Endpoint_t *r, int local)
{
	RemReader_t	*rrp;
	RemWriter_t	*rwp;

	/*dbg_printf ("rtps_endpoint_locality_update (%p, %d);\r\n", r, local);*/
	if (lock_take (r->topic->lock)) {
		warn_printf ("endpoint_locality_update: topic lock error");
		return;
	}
	if (entity_writer (entity_type (&r->entity))) {
		rwp = (RemWriter_t *) r->rtps;
		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid) {
			if (local && rwp->rw_ir_locs) {
				rwp->rw_ir_locs = 0;
				matched_writer_locators_update (rwp);
			}
			rwp->rw_uc_dreply = NULL;
		}
	}
	else {
		rrp = (RemReader_t *) r->rtps;
		for (; rrp; rrp = (RemReader_t *) rrp->rr_next_guid) {
			if (local && rrp->rw_ir_locs) {
				rrp->rr_ir_locs = 0;
				matched_reader_locators_update (rrp);
			}
			rrp->rr_uc_dreply = NULL;
		}
	}
	lock_release (r->topic->lock);
}

/* rtps_endpoint_time_filter_update -- Update the remote endpoint time-based
				       filter. */

void rtps_endpoint_time_filter_update (Endpoint_t *r)
{
	ARG_NOT_USED (r)

	/* Nothing to do since we don't do time-based filtering yet! */
	/* TODO - we'll see later. */
}

/* Update the remote endpoint content filter. */

void rtps_endpoint_content_filter_update (Endpoint_t *r)
{
	ARG_NOT_USED (r)

	/* Nothing to do since we don't do source-based content filtering
	   yet! */
	/* TODO - we'll see later. */
}

#ifdef RTPS_MARKERS

/* rtps_endpoint_markers_set -- Set a number of markers (1 << EM_*) on an 
				endpoint. */

void rtps_endpoint_markers_set (LocalEndpoint_t *r, unsigned markers)
{
	READER	*rp = (READER *) r->ep.rtps;

	rp->endpoint.mark_start   = (markers & (1 << EM_START)) != 0;
	rp->endpoint.mark_send    = (markers & (1 << EM_SEND)) != 0;
	rp->endpoint.mark_newch   = (markers & (1 << EM_NEW_CHANGE)) != 0;
	rp->endpoint.mark_rmch    = (markers & (1 << EM_REM_CHANGE)) != 0;
	rp->endpoint.mark_res_to  = (markers & (1 << EM_RESEND_TO)) != 0;
	rp->endpoint.mark_alv_to  = (markers & (1 << EM_ALIVE_TO)) != 0;
	rp->endpoint.mark_hb_to   = (markers & (1 << EM_HEARTBEAT_TO)) != 0;
	rp->endpoint.mark_nrsp_to = (markers & (1 << EM_NACKRSP_TO)) != 0;
	rp->endpoint.mark_data    = (markers & (1 << EM_DATA)) != 0;
	rp->endpoint.mark_gap     = (markers & (1 << EM_GAP)) != 0;
	rp->endpoint.mark_hb      = (markers & (1 << EM_HEARTBEAT)) != 0;
	rp->endpoint.mark_acknack = (markers & (1 << EM_ACKNACK)) != 0;
	rp->endpoint.mark_finish  = (markers & (1 << EM_FINISH)) != 0;
}

/* rtps_endpoint_markers_get -- Get the current endpoint markers. */

unsigned rtps_endpoint_markers_get (LocalEndpoint_t *r)
{
	unsigned	m = 0;
	READER		*rp = (READER *) r->ep.rtps;

	m |= (rp->endpoint.mark_start)  ? (1 << EM_START) : 0;
	m |= (rp->endpoint.mark_send)   ? (1 << EM_SEND) : 0;
	m |= (rp->endpoint.mark_newch)  ? (1 << EM_NEW_CHANGE) : 0;
	m |= (rp->endpoint.mark_rmch)   ? (1 << EM_REM_CHANGE) : 0;
	m |= (rp->endpoint.mark_res_to) ? (1 << EM_RESEND_TO) : 0;
	m |= (rp->endpoint.mark_alv_to) ? (1 << EM_ALIVE_TO) : 0;
	m |= (rp->endpoint.mark_hb_to)  ? (1 << EM_HEARTBEAT_TO) : 0;
	m |= (rp->endpoint.mark_nrsp_to)? (1 << EM_NACKRSP_TO) : 0;
	m |= (rp->endpoint.mark_data)   ? (1 << EM_DATA) : 0;
	m |= (rp->endpoint.mark_gap)    ? (1 << EM_GAP) : 0;
	m |= (rp->endpoint.mark_hb)     ? (1 << EM_HEARTBEAT) : 0;
	m |= (rp->endpoint.mark_acknack)? (1 << EM_ACKNACK) : 0;
	m |= (rp->endpoint.mark_finish) ? (1 << EM_FINISH) : 0;
	return (m);
}

/* rtps_endpoint_marker_notify -- Install a marker notification function.  If
				  fct == NULL, the function is set to the
				  default marker notification. */

void rtps_endpoint_marker_notify (unsigned markers, RMNTFFCT fct)
{
	unsigned	m, i;

	for (i = EM_START, m = 1; i <= EM_FINISH; i++, m <<= 1)
		if ((markers & m) != 0)
			rtps_markers [i] = fct;
}
#endif

static void reply_locators_update (LocatorKind_t kinds,
				   Proxy_t       *pp,
				   LocatorList_t *list,
				   unsigned      nlocs,
				   Locator_t     *locs)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	Locator_t	*lp1, *lp2;
	unsigned	n, len;
	Scope_t		scope;

	/* Quick check if lists have equal contents. */
	n = nlocs;
	lp2 = locs;
	for (n = 0, lp1 = locs, len = 0;
	     n < nlocs;
	     n++, lp1 = (Locator_t *) ((char *) lp1 + MSG_LOCATOR_SIZE))
		if ((lp1->kind & kinds) != 0)
			len++;
	if (!len && !*list)
		return;

	else if (len == locator_list_length (*list)) {
		n = nlocs;
		foreach_locator (*list, rp, np) {
			while ((lp2->kind & kinds) == 0 && n) {
				lp2 = (Locator_t *) ((char *) lp2 + MSG_LOCATOR_SIZE);
				n--;
			}
			lp1 = &np->locator;
			if (!locator_equal (lp1, lp2))
				break;

			len--;
			n--;
			lp2 = (Locator_t *) ((char *) lp2 + MSG_LOCATOR_SIZE);
		}
		if (!len)
			return;		/* No change! */
	}
	lrloc_print1 ("\r\n  ==> updating reply locators list of {%u} -> ", pp->endpoint->entity.handle);
	pp->uc_dreply = NULL;
	locator_list_delete_list (list);
	for (n = 0, lp1 = locs;
	     n < nlocs;
	     n++, lp1 = (Locator_t *) ((char *) lp1 + MSG_LOCATOR_SIZE)) {
		if ((lp1->kind & kinds) == 0)
			continue;

		if ((lp1->kind & (LOCATOR_KIND_UDPv4 | LOCATOR_KIND_TCPv4)) != 0)
			scope = sys_ipv4_scope (lp1->address + 12);
#ifdef DDS_IPV6
		else if ((lp1->kind & (LOCATOR_KIND_UDPv6 | LOCATOR_KIND_TCPv6)) != 0)
			scope = sys_ipv6_scope (lp1->address);
#endif
		else
			scope = UNKNOWN_SCOPE;
		locator_list_add (list, lp1->kind, lp1->address, lp1->port,
						0, scope, 0, 0);
	}
#ifdef RTPS_LOG_REPL_LOC
	if (*list)
		locator_list_log (RTPS_ID, 0, *list);
	else
		lrloc_print ("<empty>");
	lrloc_print ("\r\n");
#else
	ARG_NOT_USED (pp)
#endif
}

void proxy_update_reply_locators (LocatorKind_t kinds, Proxy_t *pp, RECEIVER *rxp)
{
	RemReader_t	*rrp;
	RemWriter_t	*rwp;
	LocatorRef_t	*rp;
	LocatorNode_t	*np;

	lrloc_print ("update reply locators for ");
	if (pp->is_writer) {
		rrp = proxy2rr (pp);
		lrloc_print1 ("RemWriter {%u} -> ", rrp->rr_endpoint->entity.handle);

		/* If an InfoReply was received: always update the locators. */
		if (rxp->n_uc_replies || rxp->n_mc_replies) {
			lrloc_print ("[InfoReply] ");
			pp->ir_locs = 1;
			reply_locators_update (kinds,
					       pp,
					       &rrp->rr_uc_locs,
					       rxp->n_uc_replies,
					       (Locator_t *) rxp->reply_locs);
			reply_locators_update (kinds,
					       pp,
					       &rrp->rr_mc_locs,
					       rxp->n_mc_replies,
					       (Locator_t *) &rxp->reply_locs [rxp->n_uc_replies * MSG_LOCATOR_SIZE]);
		}
		else if (pp->ir_locs) {
			lrloc_print ("[Reset]");
			pp->ir_locs = 0;
			matched_reader_locators_update (rrp);
			pp->uc_dreply = NULL;
		}
		if (!rrp->rr_uc_locs ||
		    (pp->uc_dreply && pp->uc_dreply->locator.kind <= rxp->src_locator.kind)) {
			lrloc_print (" - nothing to do.\r\n");
			return;
		}
#ifdef RTPS_LOG_REPL_LOC
		if (pp->uc_dreply && pp->uc_dreply->locator.kind > rxp->src_locator.kind)
			lrloc_print (" - improved reply locator found!");
#endif
		foreach_locator (rrp->rr_uc_locs, rp, np)
			if (locator_addr_equal (&np->locator, &rxp->src_locator)) {
				pp->uc_dreply = np;
				break;
			}
	}
	else {
		rwp = proxy2rw (pp);
		lrloc_print1 ("RemReader {%u} -> ", rwp->rw_endpoint->entity.handle);

		/* If an InfoReply was received: always update the locators. */
		if (rxp->n_uc_replies || rxp->n_mc_replies) {
			lrloc_print ("[InfoReply] ");
			pp->ir_locs = 1;
			reply_locators_update (kinds,
					       pp,
					       &rwp->rw_uc_locs,
					       rxp->n_uc_replies,
					       (Locator_t *) rxp->reply_locs);
			reply_locators_update (kinds,
					       pp,
					       &rwp->rw_mc_locs,
					       rxp->n_mc_replies,
					       (Locator_t *) &rxp->reply_locs [rxp->n_uc_replies * MSG_LOCATOR_SIZE]);
		}
		else if (pp->ir_locs) {
			lrloc_print ("[Reset] ");
			pp->ir_locs = 0;
			matched_writer_locators_update (rwp);
			pp->uc_dreply = NULL;
		}
		if (!rwp->rw_uc_locs ||
		    (pp->uc_dreply && pp->uc_dreply->locator.kind <= rxp->src_locator.kind)) {
			lrloc_print ("nothing to do.\r\n");
			return;
		}
#ifdef RTPS_LOG_REPL_LOC
		if (pp->uc_dreply && pp->uc_dreply->locator.kind > rxp->src_locator.kind)
			lrloc_print (" - improved reply locator found!");
#endif
		foreach_locator (rwp->rw_uc_locs, rp, np)
			if (locator_addr_equal (&np->locator, &rxp->src_locator)) {
				pp->uc_dreply = np;
				break;
			}
	}
#ifdef RTPS_LOG_REPL_LOC
	if (pp->uc_dreply)
		lrloc_print1 ("%s\r\n", locator_str (&pp->uc_dreply->locator));
	else
		lrloc_print1 ("can't set reply locator from %s!\r\n", locator_str (&rxp->src_locator));
#endif
	lrloc_print ("\r\n");
}

/* proxy_reset_reply_locators -- Reset the locator lists to default values. */

void proxy_reset_reply_locators (Proxy_t *pp)
{
	RemReader_t	*rrp;
	RemWriter_t	*rwp;

	lrloc_print ("RTPS: reset reply locators for ");
	if (pp->is_writer) {
		rrp = proxy2rr (pp);
		lrloc_print1 ("RemWriter {%u}", rrp->rr_endpoint->entity.handle);
		if (pp->ir_locs) {
			pp->ir_locs = 0;
			matched_reader_locators_update (rrp);
		}
		rrp->rr_uc_dreply = NULL;
	}
	else {
		rwp = proxy2rw (pp);
		lrloc_print1 ("RemReader {%u}", rwp->rw_endpoint->entity.handle);
		if (pp->ir_locs) {
			pp->ir_locs = 0;
			matched_writer_locators_update (rwp);
		}
		rwp->rw_uc_dreply = NULL;
	}
	lrloc_print ("\r\n");
}

/* proxy_add_relay -- Update a proxy to use a new relay. */

static int proxy_add_relay (Skiplist_t *list, void *node, void *arg)
{
	Domain_t	*dp;
	Participant_t 	*pp = (Participant_t *) arg;
	Endpoint_t 	*ep, **epp = (Endpoint_t **) node;
	ENDPOINT   	*rep;
	WRITER 		*wp;
	READER 		*rp;
	RemWriter_t 	*rwp;
	RemReader_t 	*rrp;
	Writer_t	*w;
	Reader_t	*r;

	ARG_NOT_USED (list);

	ep = *epp;
	rep = ep->rtps;
	dp = pp->p_domain;
	
	if (rep && rep->stateful) {
		if (rep->is_reader) {
			rp = (READER *) rep;
			r = (Reader_t *) (rp->endpoint.endpoint);
			lock_take (r->r_lock);
			LIST_FOREACH (rp->rem_writers, rwp) {
				add_relay_locators (dp, &rwp->proxy, &rwp->rw_uc_locs);
				rwp->rw_uc_dreply = NULL;
			}
			lock_release (r->r_lock);
		}
		else {
			wp = (WRITER *) rep;
			w = (Writer_t *) (wp->endpoint.endpoint);
			lock_take (w->w_lock);
			LIST_FOREACH (wp->rem_readers, rrp) {
				add_relay_locators (dp, &rrp->proxy, &rrp->rr_uc_locs);
				rrp->rr_uc_dreply = NULL;
			}
			lock_release (w->w_lock);
		}
	}
	return (1);
}

/* rtps_relay_add -- A new relay is added to a DDS domain: update all proxies.*/

void rtps_relay_add (Participant_t *pp)
{
	Domain_t	*dp = pp->p_domain;

	relay_add (pp);
	sl_walk (&dp->participant.p_endpoints, proxy_add_relay, pp);
}

/* proxy_relay_update -- Update a proxy since a relay was removed. */

static int proxy_relay_update (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t 	*ep, **epp = (Endpoint_t **) node;
	ENDPOINT   	*rep;
	WRITER 		*wp;
	READER 		*rp;
	RemWriter_t 	*rwp;
	RemReader_t 	*rrp;
	Writer_t	*w;
	Reader_t	*r;

	ARG_NOT_USED (list);
	ARG_NOT_USED (arg);

	ep = *epp;
	rep = ep->rtps;
	if (rep && rep->stateful) {
		if (rep->is_reader) {
			rp = (READER *) rep;
			r = (Reader_t *) (rp->endpoint.endpoint);
			lock_take (r->r_lock);
			LIST_FOREACH (rp->rem_writers, rwp) {
				matched_writer_locators_update (rwp);
				rwp->rw_uc_dreply = NULL;
			}
			lock_release (r->r_lock);
		}
		else {
			wp = (WRITER *) rep;
			w = (Writer_t *) (wp->endpoint.endpoint);
			lock_take (w->w_lock);
			LIST_FOREACH (wp->rem_readers, rrp) {
				matched_reader_locators_update (rrp);
				rrp->rr_uc_dreply = NULL;
			}
			lock_release (w->w_lock);
		}
	}
	return (1);
}

/* rtps_relay_remove -- An existing relay is gone: update the proxy locators. */

void rtps_relay_remove (Participant_t *pp)
{
	Domain_t	*dp = pp->p_domain;

	relay_remove (pp);
	sl_walk (&dp->participant.p_endpoints, proxy_relay_update, pp);
}

/* rtps_relay_update -- An existing relay was updated: update the proxy locators. */

void rtps_relay_update (Participant_t *pp)
{
	Domain_t	*dp = pp->p_domain;

	sl_walk (&dp->participant.p_endpoints, proxy_relay_update, pp);
}

/* participant_add_prefix -- Update or cache participant locator information. */

void participant_add_prefix (unsigned char *key, unsigned size)
{
	Domain_t	*dp;

	if (size != GUIDPREFIX_SIZE || !key)
		return;

	dp = rtps_receiver.domain;
	prefix_cache (dp, (GuidPrefix_t *) key, &rtps_receiver.src_locator);
}

static void rtps_config_duration (Config_t par, Duration_t *dur)
{
	unsigned	n;

	if (config_defined (par)) {
		n = config_get_number (par, dur->secs * 1000);
		dur->secs = n / 1000;
		dur->nanos = (n % 1000) * 1000000;
	}
}

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

/* rtps_peer_reader_crypto_set -- Set a peer reader crypto handle. */

void rtps_peer_reader_crypto_set (Writer_t *w, DiscoveredReader_t *dr, unsigned h)
{
	WRITER		*wp;
	RemReader_t	*rrp;

	if (!dr->dr_rtps)
		return;

	wp = w->w_rtps;
	if (!wp)
		return;

	for (rrp = (RemReader_t *) dr->dr_rtps;
	     rrp;
	     rrp = (RemReader_t *) rrp->rr_next_guid)
		if (rrp->rr_writer == wp) {
			rrp->rr_crypto = h;
			break;
		}
}

/* rtps_peer_writer_crypto_set -- Set a peer writer crypto handle. */

void rtps_peer_writer_crypto_set (Reader_t *r, DiscoveredWriter_t *dw, unsigned h)
{
	READER		*rp;
	RemWriter_t	*rwp;

	if (!dw->dw_rtps)
		return;

	rp = r->r_rtps;
	if (!rp)
		return;

	for (rwp = (RemWriter_t *) dw->dw_rtps;
	     rwp;
	     rwp = (RemWriter_t *) rwp->rw_next_guid)
		if (rwp->rw_reader == rp) {
			rwp->rw_crypto = h;
			break;
		}
}

/* rtps_peer_reader_crypto_get -- Get a peer reader crypto handle. */

unsigned rtps_peer_reader_crypto_get (Writer_t *w, DiscoveredReader_t *dr)
{
	WRITER		*wp;
	RemReader_t	*rrp;

	if (!dr->dr_rtps)
		return (0);

	rrp = (RemReader_t *) dr->dr_rtps;
	if (!w)
		return (rrp->rr_crypto);

	wp = w->w_rtps;
	if (!wp)
		return (0);

	for (; rrp; rrp = (RemReader_t *) rrp->rr_next_guid)
		if (rrp->rr_writer == wp)
			return (rrp->rr_crypto);

	return (0);
}

/* rtps_peer_writer_crypto_get -- Get a peer writer crypto handle. */

unsigned rtps_peer_writer_crypto_get (Reader_t *r, DiscoveredWriter_t *dw)
{
	READER		*rp;
	RemWriter_t	*rwp;

	if (!dw->dw_rtps)
		return (0);

	rwp = (RemWriter_t *) dw->dw_rtps;
	if (!r)
		return (rwp->rw_crypto);

	rp = r->r_rtps;
	if (!rp)
		return (0);

	for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
		if (rwp->rw_reader == rp)
			return (rwp->rw_crypto);

	return (0);
}

#endif

/* rtps_init -- Initialize the RTPS layer with the given configuration
		parameters. */

int rtps_init (const RTPS_CONFIG *cp)
{
#ifdef CTRACE_USED
	log_fct_str [RTPS_ID] = rtps_fct_str;
#endif
#if defined (RTPS_TRACE) && defined (RTPS_TRACE_BIVS)
	rtps_name_trace_set ("BuiltinParticipantVolatileMessageSecure*", 15);
#endif
	/* Set logging. */
	rtps_log = log_logged (RTPS_ID, 0);

	/* Initialize memory pools. */
	rtps_init_memory (cp);

	/* Initialize the proxy list lock. */
	lock_init_nr (rtps_proxy_lock, "RTPS_Proxies");

	/* Initialize the transport multiplexer. */
	rtps_mux_init (rtps_receive,
		       rtps_notify,
		       &rtps_mem_blocks [MB_MSG_BUF],
		       &rtps_mem_blocks [MB_MSG_ELEM_BUF],
		       &rtps_mem_blocks [MB_MSG_REF]);

	/* Initialize the Parameter Id list parser. */
	pid_init ();

	/* Attach to history cache. */
	hc_monitor_fct (rtps_writer_new_change,
			rtps_writer_delete_change,
			rtps_writer_urgent_change,
			rtps_reader_unblock,
			rtps_writer_alive,
			rtps_reader_flush);

	PROF_INIT ("R:WCreate", rtps_w_create);
	PROF_INIT ("R:WDelete", rtps_w_delete);
	PROF_INIT ("R:WNew", rtps_w_new);
	PROF_INIT ("R:WRemove", rtps_w_remove);
	PROF_INIT ("R:WUrgent", rtps_w_urgent);
	PROF_INIT ("R:WWrite", rtps_w_write);
	PROF_INIT ("R:WDispose", rtps_w_dispose);
	PROF_INIT ("R:WUnreg", rtps_w_unregister);
	PROF_INIT ("R:WRlocAdd", rtps_w_rloc_add);
	PROF_INIT ("R:WRlocRem", rtps_w_rloc_rem);
	PROF_INIT ("R:WProxAdd", rtps_w_proxy_add);
	PROF_INIT ("R:WProxRem", rtps_w_proxy_rem);
	PROF_INIT ("R:WResend", rtps_w_resend);
	PROF_INIT ("R:WUpdate", rtps_w_update);
	PROF_INIT ("R:DoChange", rtps_do_changes);
	PROF_INIT ("R:SendMsgs", rtps_send_msgs);
	PROF_INIT ("R:RCreate", rtps_r_create);
	PROF_INIT ("R:RDelete", rtps_r_delete);
	PROF_INIT ("R:RProxAdd", rtps_r_proxy_add);
	PROF_INIT ("R:RProxRem", rtps_r_proxy_rem);
	PROF_INIT ("R:RUnblock", rtps_r_unblock);
	PROF_INIT ("R:RxMsgs", rtps_rx_msgs);
	PROF_INIT ("R:RxData", rtps_rx_data);
	PROF_INIT ("R:RxGap", rtps_rx_gap);
	PROF_INIT ("R:RxHBeat", rtps_rx_hbeat);
	PROF_INIT ("R:RxAcknak", rtps_rx_acknack);
	PROF_INIT ("R:RxInfTS", rtps_rx_inf_ts);
	PROF_INIT ("R:RxInfRep", rtps_rx_inf_rep);
	PROF_INIT ("R:RxInfDst", rtps_rx_inf_dst);
	PROF_INIT ("R:RxInfSrc", rtps_rx_inf_src);
	PROF_INIT ("R:RxDataFr", rtps_rx_data_frag);
	PROF_INIT ("R:RxNackFr", rtps_rx_nack_frag);
	PROF_INIT ("R:RxHBFr", rtps_rx_hbeat_frag);
	PROF_INIT ("R:TxData", rtps_tx_data);
	PROF_INIT ("R:TxGap", rtps_tx_gap);
	PROF_INIT ("R:TxHBeat", rtps_tx_hbeat);
	PROF_INIT ("R:TxAcknak", rtps_tx_acknack);
	PROF_INIT ("R:TxInfTS", rtps_tx_inf_ts);
	PROF_INIT ("R:TxInfRep", rtps_tx_inf_rep);
	PROF_INIT ("R:TxInfDst", rtps_tx_inf_dst);
	PROF_INIT ("R:TxInfSrc", rtps_tx_inf_src);
	PROF_INIT ("R:PWStart", rtps_pw_start);
	PROF_INIT ("R:PWNew", rtps_pw_new);
	PROF_INIT ("R:PWSend", rtps_pw_send);
	PROF_INIT ("R:PWRem", rtps_pw_rem);
	PROF_INIT ("R:PWFinish", rtps_pw_finish);
	PROF_INIT ("R:BWStart", rtps_bw_start);
	PROF_INIT ("R:BWNew", rtps_bw_new);
	PROF_INIT ("R:BWSend", rtps_bw_send);
	PROF_INIT ("R:BWRem", rtps_bw_rem);
	PROF_INIT ("R:BWFinish", rtps_bw_finish);
	PROF_INIT ("R:RWStart", rtps_rw_start);
	PROF_INIT ("R:RWNew", rtps_rw_new);
	PROF_INIT ("R:RWSend", rtps_rw_send);
	PROF_INIT ("R:RWRem", rtps_rw_rem);
	PROF_INIT ("R:RWFinish", rtps_rw_finish);
	PROF_INIT ("R:RWAcknak", rtps_rw_acknack);
	PROF_INIT ("R:RWHBTO", rtps_rw_hb_to);
	PROF_INIT ("R:RWAliveTO", rtps_rw_alive_to);
	PROF_INIT ("R:RWNRespTO", rtps_rw_nresp_to);
	PROF_INIT ("R:BRStart", rtps_br_start);
	PROF_INIT ("R:BRData", rtps_br_data);
	PROF_INIT ("R:BRFinish", rtps_br_finish);
	PROF_INIT ("R:RRStart", rtps_rr_start);
	PROF_INIT ("R:RRData", rtps_rr_data);
	PROF_INIT ("R:RRFinish", rtps_rr_finish);
	PROF_INIT ("R:RRGap", rtps_rr_gap);
	PROF_INIT ("R:RRHBeat", rtps_rr_hbeat);
	PROF_INIT ("R:RRAliveTO", rtps_rr_alive_to);
	PROF_INIT ("R:RRDoAck", rtps_rr_do_ack);
	PROF_INIT ("R:RRProc", rtps_rr_proc);

#ifdef DDS_DEBUG
	/* TODO: Remove this and all related code once dtls/tls are fully working */
	rtps_no_security = config_get_number (DC_NoSecurity, 0);
#endif

	if (config_defined (DC_RTPS_Mode))
		rtps_used = config_get_mode (DC_RTPS_Mode, MODE_ENABLED);

	rtps_sl_retries = config_get_number (DC_RTPS_StatelessRetries, RTPS_SL_RETRIES);
	rtps_config_duration (DC_RTPS_ResendPer, &rtps_def_resend_per);
	rtps_config_duration (DC_RTPS_HeartbeatPer, &rtps_def_heartbeat);
	rtps_config_duration (DC_RTPS_NackRespTime, &rtps_def_nack_resp);
	rtps_config_duration (DC_RTPS_NackSuppTime, &rtps_def_nack_supp);
	rtps_config_duration (DC_RTPS_LeaseTime, &rtps_def_lease_per);
	rtps_config_duration (DC_RTPS_HeartbeatResp, &rtps_def_hb_resp);
	rtps_config_duration (DC_RTPS_HeartbeatSupp, &rtps_def_hb_supp);

	rtps_max_msg_size = config_get_number (DC_RTPS_MsgSize, DEF_RTPS_MSG_SIZE);
	if (rtps_max_msg_size < MIN_RTPS_MSG_SIZE || rtps_max_msg_size > MAX_RTPS_MSG_SIZE) {
		warn_printf ("rtps_init: Incorrect RTPS message size - value must be in [%u..%u]!",
						MIN_RTPS_MSG_SIZE, MAX_RTPS_MSG_SIZE);
		rtps_max_msg_size = DEF_RTPS_MSG_SIZE;
	}
#ifdef RTPS_FRAGMENTS
	if (config_defined (DC_RTPS_FragSize)) {
		rtps_frag_size = config_get_number (DC_RTPS_FragSize, DEF_RTPS_FRAGMENT_SIZE);
		if (!rtps_frag_size) {
			rtps_frag_size = ~0;
			if (dds_max_sample_size > MAX_SAMPLE_SIZE_NF) {
				warn_printf ("rtps_init: updated DDS sample size to %u due to fragmentation being disabled!", MAX_SAMPLE_SIZE_NF);
				dds_max_sample_size = MAX_SAMPLE_SIZE_NF;
			}
		}
		else if (rtps_frag_size < MIN_RTPS_FRAGMENT_SIZE || rtps_frag_size > MAX_RTPS_FRAGMENT_SIZE) {
			warn_printf ("rtps_init: Incorrect RTPS fragment size - value must be in [%u..%u]!",
						MIN_RTPS_FRAGMENT_SIZE, MAX_RTPS_FRAGMENT_SIZE);
			rtps_frag_size = DEF_RTPS_FRAGMENT_SIZE;
		}
	}
	if (config_defined (DC_RTPS_FragBurst)) {
		rtps_frag_burst = config_get_number (DC_RTPS_FragBurst, DEF_RTPS_FRAGMENT_BURST);
		if (rtps_frag_burst < 1) {
			warn_printf ("rtps_init: Incorrect RTPS fragment size - value must be in [%u..%u]!",
						MIN_RTPS_FRAGMENT_SIZE, MAX_RTPS_FRAGMENT_SIZE);
			rtps_frag_burst = DEF_RTPS_FRAGMENT_SIZE;
		}
	}
	if (config_defined (DC_RTPS_FragDelay))
		rtps_frag_delay = config_get_number (DC_RTPS_FragBurst, 0);
#endif
	return (DDS_RETCODE_OK);
}

/* rtps_final -- Finalize the RTPS layer. */

void rtps_final (void)
{
	hc_monitor_fct (NULL, NULL, NULL, NULL, NULL, NULL);
	lock_destroy (rtps_proxy_lock);
	rtps_mux_final ();
	mds_free (rtps_mem_blocks, MB_END);
}

/* rtps_msg_pools -- Returns the message construction pool descriptors. */

void rtps_msg_pools (MEM_DESC *hdrs, MEM_DESC *elements)
{
	if (hdrs)
		*hdrs = &rtps_mem_blocks [MB_MSG_BUF];
	if (elements)
		*elements = &rtps_mem_blocks [MB_MSG_ELEM_BUF];
}


