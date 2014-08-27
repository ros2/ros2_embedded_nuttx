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

/* rtps_priv.h -- Defines the internal RTPS data structures. */

#ifndef __rtps_priv_h_
#define	__rtps_priv_h_

#include "rtps_cfg.h"

typedef struct rem_reader_st RemReader_t;
typedef struct rem_writer_st RemWriter_t;

typedef struct rtps_endpoint_st ENDPOINT;
struct rtps_endpoint_st {
	LocalEndpoint_t		*endpoint;	/* Local Reader/Writer. */
	unsigned		is_reader:1;	/* Reader/writer. */
	unsigned		reliability:2;	/* Reliable/Best-effort. */
	unsigned		stateful:1;	/* Stateful protocol. */
	unsigned		multi_inst:1;	/* Multi-instance. */
	unsigned		notify_act:1;	/* Reader: notify pending. */
	unsigned		inline_qos:1;	/* Inline QoS info expected. */
	unsigned		push_mode:1;	/* Push mode. */
	unsigned		cache_act:1;	/* Attached to cache. */
	unsigned		limit_rx:1;	/* Limited # of rxed samples. */
	unsigned		cfilter_rx:1;	/* Content filtered. */
	unsigned		cache_acks:1;	/* Ack only if cache allows. */
	unsigned		resends:1;	/* Stateless can resend. */
	unsigned		tfilter_rx:1;	/* Time-based filter. */
	unsigned		trace_frames:1;	/* Frame tracing. */
	unsigned		trace_sigs:1;	/* Signal tracing. */
	unsigned		trace_state:1;	/* State tracing. */
	unsigned		trace_tmr:1;	/* Timer tracing. */
#ifdef RTPS_MARKERS
	unsigned		mark_start:1;	/* Mark: *_start. */
	unsigned		mark_send:1;	/* Mark: *_send_data. */
	unsigned		mark_newch:1;	/* Mark: *_new_change. */
	unsigned		mark_rmch:1;	/* Mark: *_rem_change. */
	unsigned		mark_res_to:1;	/* Mark: Resend timeout. */
	unsigned		mark_alv_to:1;	/* Mark: Alive timeout. */
	unsigned		mark_hb_to:1;	/* Mark: HEARBEAT timeout. */
	unsigned		mark_nrsp_to:1;	/* Mark: Nack response to. */
	unsigned		mark_data:1;	/* Mark: DATA received. */
	unsigned		mark_gap:1;	/* Mark: GAP received. */
	unsigned		mark_hb:1;	/* Mark: HEARTBEAT received. */
	unsigned		mark_acknack:1;	/* Mark: ACKNACK received. */
	unsigned		mark_finish:1;	/* Mark: *_finish. */
#endif
#ifdef RTPS_OPT_MCAST
	LocatorList_t		mc_locators;	/* Multicast locators. */
#endif
};

#define MAX_RX_HEADER	128

typedef struct receiver_st {
	ProtocolVersion_t	src_version;
	VendorId_t		src_vendor;
	GuidPrefix_t		src_guid_prefix;
	GuidPrefix_t		dst_guid_prefix;
	int			have_prefix;
	int			have_timestamp;
	FTime_t			timestamp;
	unsigned		n_uc_replies;
	unsigned		n_mc_replies;
	unsigned char		reply_locs [MAXLLOCS * MSG_LOCATOR_SIZE];
	Locator_t		src_locator;
	Domain_t		*domain;
	Participant_t		*peer;
	unsigned long		inv_submsgs;
	unsigned long		submsg_too_short;
	unsigned long		inv_qos;
	unsigned long		no_bufs;
	unsigned long		unkn_dest;
	unsigned long		inv_marshall;
	RcvError_t		last_error;
	RME			last_mep;
	size_t			msg_size;
	unsigned char		msg_buffer [MAX_RX_HEADER];
} RECEIVER;

#define MAX_TX_HEADER	128

typedef enum {
	T_NO_ERROR,	/* No error. */
	T_NO_DEST,	/* No destination locator. */
	T_NOMEM		/* Not enough memory. */
} TxError_t;

typedef struct transmitter_st {
	unsigned long		nlocsend;
	unsigned long		no_locator;
	unsigned long		no_memory;
	TxError_t		last_error;
	size_t			msg_size;
	unsigned char		msg_buffer [MAX_TX_HEADER];
} TRANSMITTER;

extern RECEIVER rtps_receiver;
extern TRANSMITTER rtps_transmitter;

#define	info_reply_rxed()	(rtps_receiver.n_uc_replies || rtps_receiver.n_mc_replies)

typedef struct rtps_writer_st WRITER;
typedef struct rtps_reader_st READER;

/* 1. Per ReaderLocator/ProxyReader, a single list (in proper ordering, i.e.
      arrival order) is used to keep state of each cache change as needed for
      the appropriate Writer behavior.

   2. Various other lists are needed for each ReaderLocator/ProxyReader to
      handle specific sublists of the secondary list such as:

	- The sublist of unsent changes.
	- The sublist of sent changes.
	- The sublist of requested changes.

      These (tertiary) lists are managed using:

  	a. The state as kept for each change in the secondary list.
	b. A pointer to the first element in the secondary list that
	   corresponds to the start of the sublist or NULL if the
	   sublist is empty.
	c. Walking over subsequent entries in the secondary list while
	   skipping entries (using secondary list state information)
	   that are not useful.
*/

/* Change list: change state: */
typedef enum change_state_en {
	/* States used by both. */
	CS_NEW,			/* Initial state. */
	CS_REQUESTED,		/* Reader sent a NACK. */

	/* Writer-only states. */
	CS_UNSENT,		/* Sample is waiting, i.e. not yet sent. */
	CS_UNDERWAY,		/* Data was sent.  No feedback yet. */
	CS_UNACKED,		/* After NACK suppression time-out. */
	CS_ACKED,		/* Acknowledged by reader. */

	/* Reader-only states. */
	CS_MISSING,		/* Missing sample. */
	CS_RECEIVED,		/* Sample received. */
	CS_LOST			/* Sample was lost. */

} ChangeState_t;

#ifdef RTPS_FRAGMENTS

/* Fragmentation info:
   This structure is used temporarily when sending/receiving fragments.  It is
   allocated when the first fragment is sent/received, and disposed when all
   fragments have been successfully processed. */
typedef struct frag_info_st FragInfo_t;
struct frag_info_st {
	unsigned	nrefs;		/* # of references to context. */
	unsigned	total;		/* Total # of fragments. */
	unsigned	first_na;	/* First fragment not acked/rxed. */
	unsigned	num_na;		/* # of not acked/rxed fragments. */
	handle_t	writer;		/* Writer handle. */
	KeyHash_t	hash;		/* Hash value. */
	size_t		fsize;		/* Size of each fragment. */
	DB		*data;		/* Reassembly buffer. */
	size_t		length;		/* Length of reassembly buffer. */
	KeyHash_t	*hp;		/* Hash value pointer if non-NULL. */
	unsigned char	*key;		/* Key buffer. */
	size_t		keylen;		/* Key length. */
	Timer_t		timer;		/* Reassembly timer (stateless). */
	uint32_t	bitmap [1];	/* Fragment bits. */
};

#define	FRAG_INFO_SIZE(n)	(sizeof (FragInfo_t) + ((((n) + 31) >> 5) << 2) \
				- sizeof (uint32_t))

#endif

/* Change list node:
   The first and last fields are used for efficient storage of handling sequence
   number ranges for GAP and ACKNACK purposes.  In the case of a reliable sender
   for example, all consecutive non-significant (relevant=0) samples will be
   combined in 1 node with first and last set to the first and last relevant
   sequence numbers of the range. */
typedef struct ccref_st CCREF;
struct ccref_st {
	CCREF		*next;		/* Next in list of changes. */
	CCREF		*prev;		/* Previous in list of changes. */
	unsigned	state:8;	/* Current state in stateful mode. */
	unsigned	relevant:1;	/* Change is relevant for transmit. */
	unsigned	mcdata:1;	/* Multicasted destination. */
	unsigned	ack_req:1;	/* Ack requested from cache. */
#ifdef RTPS_FRAGMENTS
	FragInfo_t	*fragments;	/* Fragment status information. */
#endif
	union {
	  struct {
	   Change_t	*change;	/* Cache change data - marshalled. */
	   HCI          hci;		/* Instance reference. */
	  }             c;
	  struct {
	   SequenceNumber_t first;	/* First sequence number. */
	   SequenceNumber_t last;	/* Last sequence number. */
	  }		range;		/* Range of sequence numbers. */
	}		u;
};

/* List of cache changes: */
typedef struct cclist_st {
	CCREF		*head;		/* First cache change. */
	CCREF		*tail;		/* Last cache change. */
	unsigned	nchanges;	/* # of changes. */
} CCLIST;

typedef struct proxy_st Proxy_t;
struct proxy_st {
	Proxy_t		*next_active;
	Proxy_t		*prev_active;
	union {
	  READER	*reader;
	  WRITER	*writer;
	} u;
	unsigned	cstate:2;
	unsigned	tstate:2;
	unsigned	astate:2;
	unsigned	inline_qos:1;
	unsigned	reliable:1;
	unsigned	active:1;
	unsigned	peer_alive:1;
	unsigned	is_writer:1;
	unsigned	heartbeats:1;
	unsigned	marshall:1;
	unsigned	blocked:1;
	unsigned	msg_time:1;
	unsigned	no_mcast:1;
	unsigned	id_prefix:1;
	unsigned	ir_locs:1;
#ifdef DDS_SECURITY
	unsigned	tunnel:1;
#endif
	unsigned	unacked:13;
	CCLIST		changes;
	Endpoint_t	*endpoint;
	Proxy_t		*next_guid;
	RMBUF		*head;
	RMBUF		*tail;
	Proxy_t		*link;
	Timer_t		*timer;
	LocatorNode_t	*uc_dreply;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	unsigned	crypto;
#endif
#ifdef RTPS_PROXY_INST
	uint32_t	loc_inst;
	uint32_t	rem_inst;
#endif
#ifdef EXTRA_STATS
	unsigned	nmsg;
	unsigned	ndata;
	unsigned	ngap;
	unsigned	nheartbeat;
	unsigned	nacknack;
#ifdef RTPS_FRAGMENTS
	unsigned	ndatafrags;
	unsigned	nheartbeatfrags;
	unsigned	nnackfrags;
#endif
#endif
};

#ifdef EXTRA_STATS
#define	STATS_INC(n)	n++
#else
#define	STATS_INC(n)
#endif

typedef enum rr_cstate_en {
	RRCS_INITIAL,
	RRCS_READY,
	RRCS_FINAL
} RRCState_t;

typedef enum rr_astate_en {
	RRAS_WAITING,
	RRAS_MUST_REPAIR,
	RRAS_REPAIRING
} RRAState_t;

typedef enum rr_tstate_en {
	RRTS_IDLE,
	RRTS_PUSHING,
	RRTS_ANNOUNCING
} RRTState_t;

extern const char *rtps_rr_cstate_str [];
extern const char *rtps_rr_tstate_str [];
extern const char *rtps_rr_astate_str [];

struct rem_reader_st {
	RemReader_t	   *next;
	RemReader_t	   *prev;
	Proxy_t		   proxy;
#define	rr_next_active	   proxy.next_active
#define	rr_prev_active	   proxy.prev_active
#define	rr_writer	   proxy.u.writer
#define	rr_cstate	   proxy.cstate
#define	rr_tstate	   proxy.tstate
#define	rr_astate	   proxy.astate
#define	rr_inline_qos	   proxy.inline_qos
#define	rr_reliable	   proxy.reliable
#define	rr_active	   proxy.active
#define	rr_peer_alive	   proxy.peer_alive
#define	rr_is_writer	   proxy.is_writer
#define	rr_heartbeat	   proxy.heartbeats
#define	rr_no_mcast	   proxy.no_mcast
#define	rr_ir_locs	   proxy.ir_locs
#define	rr_id_prefix	   proxy.id_prefix
#define	rr_tunnel	   proxy.tunnel
#define	rr_unacked	   proxy.unacked
#define	rr_nelements	   proxy.nelements;
#define	rr_marshall	   proxy.marshall
#define	rr_changes	   proxy.changes
#define	rr_endpoint	   proxy.endpoint
#define	rr_next_guid	   proxy.next_guid
#define	rr_head		   proxy.head
#define	rr_tail		   proxy.tail
#define	rr_link		   proxy.link
#define	rr_nack_timer	   proxy.timer
#define	rr_uc_dreply	   proxy.uc_dreply
#define	rr_crypto	   proxy.crypto
#define	rr_loc_inst	   proxy.loc_inst
#define	rr_rem_inst	   proxy.rem_inst
#define	rr_nmsg		   proxy.nmsg
#define	rr_ndata	   proxy.ndata
#define	rr_ngap		   proxy.ngap
#define	rr_nheartbeat	   proxy.nheartbeat
#define	rr_nacknack	   proxy.nacknack
#define	rr_ndatafrags	   proxy.ndatafrags
#define	rr_nheartbeatfrags proxy.nheartbeatfrags
#define	rr_nnackfrags      proxy.nnackfrags
	CCREF		   *rr_unsent_changes;
	CCREF		   *rr_requested_changes;
	SequenceNumber_t   rr_new_snr;
	union  {
	 LocatorNode_t	   *u_locator;
#define	rr_locator	   u.u_locator
	 struct {
	  LocatorList_t	   us_uc_locs;
	  LocatorList_t	   us_mc_locs;
	  Count_t	   us_last_ack;
#ifdef RTPS_FRAGMENTS
	  Count_t	   us_last_nackfrag;
	  unsigned	   us_nfrags;
#endif
	 }		   sf;
#define	rr_uc_locs	   u.sf.us_uc_locs
#define	rr_mc_locs	   u.sf.us_mc_locs
#define	rr_last_ack	   u.sf.us_last_ack
#define	rr_last_nackfrag   u.sf.us_last_nackfrag
#define	rr_nfrags	   u.sf.us_nfrags
	}		   u;
};

#define proxy2rr(p)	(RemReader_t *) ((char *) (p) - offsetof (RemReader_t, proxy))

typedef struct rem_reader_list_st RemReaders_t;
struct rem_reader_list_st {
	RemReader_t	*head;
	RemReader_t	*tail;
	unsigned	count;
};

typedef enum rr_type_en {
	RRT_SL_BE,
	RRT_SL_REL,
	RRT_SF_BE,
	RRT_SF_REL,

	RRT_MARKER
} RRType_t;

#define	rr_type(stateful,reliable) (RRType_t) ((stateful << 1) | reliable)

#define	RR_MAX_TYPES		   (unsigned) RRT_MARKER

typedef void (*RRB_STARTF) (RemReader_t *rrp);

/* Start the state machine. */

typedef int (*RRB_NCHANGEF) (RemReader_t      *rrp,
			     Change_t         *cp,
			     HCI              hci,
			     SequenceNumber_t *snr);

/* Add a new cache change.  The sequence number is always valid. The change can
   be NULL for reliable mode connections. */

typedef int (*RRB_SENDF) (RemReader_t *rrp);

/* Send what is ready for sending. */

typedef void (*RRB_ACKNACKF) (RemReader_t *rrp, AckNackSMsg *ap, int final);

/* AckNack message received. */

#ifdef RTPS_FRAGMENTS

typedef void (*RRB_NACKFRAGF) (RemReader_t *rrp, NackFragSMsg *np);

/* NackFrag message received. */

#endif

typedef void (*RRB_RMCHANGEF) (RemReader_t *rrp, Change_t *cp);

/* Cache entry removed. */

typedef void (*RRB_FINISHF) (RemReader_t *rrp);

/* End of state machine operation. */

typedef struct rr_behavior_st {
	RRB_STARTF	start;		/* Start the state machine. */
	RRB_NCHANGEF	add_change;	/* Add a new cache change. */
	RRB_SENDF	send_now;	/* Send what is ready for sending. */
	RRB_ACKNACKF	acknack;	/* AckNack message received. */
#ifdef RTPS_FRAGMENTS
	RRB_NACKFRAGF	nackfrag;	/* NackFrag message received. */
#endif
	RRB_RMCHANGEF	rem_change;	/* Cache entry removed. */
	RRB_FINISHF	finish;		/* End of state machine operation. */
} RR_EVENTS;

extern RR_EVENTS *rtps_rr_event [];

struct rtps_writer_st {
	ENDPOINT	endpoint;
	RemReaders_t	rem_readers;
	Ticks_t		rh_period;	/* Resend/Heartbeat period. */
	Ticks_t		nack_resp_delay;
	Ticks_t		nack_supp_duration;
	Count_t		heartbeats;
#define	slw_retries	heartbeats
	Timer_t		*rh_timer;
	unsigned	prio:27;
	unsigned	backoff:3;
	unsigned	no_mcast:1;	/* Don't use multicast. */
	unsigned	mc_marshall:1;	/* Marshall multicast. */
};

typedef enum rw_cstate_en {
	RWCS_INITIAL,		/* Reader creation. */
	RWCS_READY,		/* Ready for reception. */
	RWCS_FINAL		/* Closing down. */
} RWCState_t;

typedef enum rw_astate_en {
	RWAS_WAITING,		/* Waiting for HEARTBEAT. */
	RWAS_MAY_ACK,		/* May send an ACKNACK message. */
	RWAS_MUST_ACK		/* Must send an ACKNACK message. */
} RWAState_t;

extern const char *rtps_rw_cstate_str [];
extern const char *rtps_rw_astate_str [];

struct rem_writer_st {
	RemWriter_t	   *next;
	RemWriter_t	   *prev;
	Proxy_t		   proxy;
#define rw_next_active	   proxy.next_active
#define	rw_prev_active	   proxy.prev_active
#define rw_reader	   proxy.u.reader
#define	rw_cstate	   proxy.cstate
#define	rw_hb_no_data	   proxy.tstate
#define	rw_astate	   proxy.astate
#define	rw_reliable	   proxy.reliable
#define	rw_active	   proxy.active
#define	rw_peer_alive	   proxy.peer_alive
#define	rw_heartbeats	   proxy.heartbeats
#define	rw_is_writer	   proxy.is_writer
#define	rw_no_mcast	   proxy.no_mcast
#define	rw_blocked	   proxy.blocked
#define	rw_ir_locs	   proxy.ir_locs
#define	rw_tunnel	   proxy.tunnel
#define	rw_changes	   proxy.changes
#define	rw_endpoint	   proxy.endpoint
#define	rw_next_guid	   proxy.next_guid
#define	rw_head		   proxy.head
#define	rw_tail		   proxy.tail
#define	rw_link		   proxy.link
#define	rw_hbrsp_timer	   proxy.timer
#define	rw_uc_dreply	   proxy.uc_dreply
#define	rw_crypto	   proxy.crypto
#define	rw_loc_inst	   proxy.loc_inst
#define	rw_rem_inst	   proxy.rem_inst
#define	rw_nmsg		   proxy.nmsg
#define	rw_ndata	   proxy.ndata
#define	rw_ngap		   proxy.ngap
#define	rw_nheartbeat	   proxy.nheartbeat
#define	rw_nacknack	   proxy.nacknack
#define	rw_ndatafrags	   proxy.ndatafrags
#define	rw_nheartbeatfrags proxy.nheartbeatfrags
#define	rw_nnackfrags      proxy.nnackfrags
	LocatorList_t	   rw_uc_locs;
	LocatorList_t	   rw_mc_locs;
	Count_t		   rw_last_hb;
#ifdef RTPS_FRAGMENTS
	Count_t		   rw_last_hbfrag;
#endif
	SequenceNumber_t   rw_seqnr_next;
};

#define proxy2rw(p)	(RemWriter_t *) ((char *) (p) - offsetof (RemWriter_t, proxy))

typedef struct rem_writer_list_st {
	RemWriter_t	*head;
	RemWriter_t	*tail;
	unsigned	count;
} RemWriters_t;

typedef enum rw_type_en {
	RWT_SF_BE,
	RWT_SF_REL,

	RWT_MARKER
} RWType_t;

#define	rw_type(reliable) (RWType_t) (reliable)

#define	RW_MAX_TYPES		   (unsigned) RWT_MARKER

typedef void (*RWB_STARTF) (RemWriter_t *rwp);

/* Start the state machine. */

typedef void (*RWB_DATAF) (RemWriter_t         *rwp,
			   Change_t            *cp,
			   SequenceNumber_t    *cpsnr,
			   const KeyHash_t     *hp,
			   const unsigned char *key,
			   size_t              keylen,
#ifdef RTPS_FRAGMENTS
			   DataFragSMsg        *fragp,
			   FragInfo_t	       **finfo,
#endif
			   int                 ignore);

/* Data message received. */

typedef void (*RWB_GAPF) (RemWriter_t *rwp, GapSMsg *gp);

/* Gap message received. */

typedef void (*RWB_HEARTBEATF) (RemWriter_t   *rwp,
				HeartbeatSMsg *hp,
				unsigned      flags);

/* Heartbeat message received. */

/* Data message received. */

#ifdef RTPS_FRAGMENTS

typedef void (*RWB_HBFRAGF) (RemWriter_t *rwp, HeartbeatFragSMsg *hp);

/* HeartbeatFrag message received. */

#endif

typedef void (*RWB_FINISHF) (RemWriter_t *rwp);

/* End of state machine operation. */

typedef struct rw_behavior_st {
	RWB_STARTF	start;		/* Start the state machine. */
	RWB_DATAF	data;		/* Data message received. */
	RWB_GAPF	gap;		/* Gap message received. */
	RWB_HEARTBEATF	heartbeat;	/* Heartbeat message received. */
#ifdef RTPS_FRAGMENTS
	RWB_HBFRAGF	hbfrag;		/* HeartbeatFrag message received. */
#endif
	RWB_FINISHF	finish;		/* End of state machine operation. */
} RW_EVENTS;

extern RW_EVENTS *rtps_rw_event [];

struct rtps_reader_st {
	ENDPOINT	endpoint;		/* Endpoint struct. */
	RemWriters_t	rem_writers;		/* Remote writers. */
	unsigned	data_queued;		/* # of data blocks queued. */
	Ticks_t		heartbeat_resp_delay;	/* Heartbeat Response delay. */
	Ticks_t		heartbeat_supp_dur;	/* Heartbeat Suppression delay*/
	Count_t		acknacks;
#ifdef RTPS_FRAGMENTS
	Count_t		nackfrags;
#endif
};

void participant_add_prefix (unsigned char *key, unsigned size);

/* Update or cache participant locator information. */

void proxy_activate (Proxy_t *pp);

void proxy_wait_inactive (Proxy_t *pp);

void proxy_reset_reply_locators (Proxy_t *pp);

void proxy_update_reply_locators (LocatorKind_t kinds, Proxy_t *pp, RECEIVER *rxp);

int proxy_add_change (uintptr_t user, Change_t *cp, HCI hci);

void change_remove (RemReader_t *rrp, Change_t *cp);

int reader_cache_add_inst (READER   *rp,
			   Change_t *cp,
			   HCI      hci,
			   int      rel);

void reader_cache_add_key (READER              *rp,
			   Change_t            *cp,
			   const KeyHash_t     *hp,
			   const unsigned char *key,
			   size_t              keylen);

/* Add a change to the History Cache of the Reader.  The instance of the data
   sample is identified via a hash key (if non-NULL) and/or the key fields (if
   non-NULL). */

void remote_writer_remove (READER *rp, RemWriter_t *rwp);

#ifdef RTPS_TRC_CHANGES
#define TRC_CHANGE(cp,s,alloc)	log_printf (CACHE_ID, 0, "%s(%p) %s\r\n", (alloc) ? "alloc" : "free", (void *) cp, s)
#else
#define	TRC_CHANGE(cp,s,alloc)
#endif

void endpoint_names (ENDPOINT *ep, const char *names [2]);

enum mem_block_en {
	MB_READER,		/* RTPS Reader. */
	MB_WRITER,		/* RTPS Writer. */
	MB_REM_READER,		/* Reader Locator or Reader Proxy. */
	MB_REM_WRITER,		/* Writer Proxy. */
	MB_CCREF,		/* Cache Change reference. */
	MB_MSG_BUF,		/* Message buffer. */
	MB_MSG_ELEM_BUF,	/* Message Element buffer. */
	MB_MSG_REF,		/* Message buffer reference. */

	MB_END
};

extern MEM_DESC_ST	rtps_mem_blocks [MB_END];  /* Memory used by RTPS. */
extern size_t		rtps_mem_size;		/* Total memory allocated. */
extern const char	*rtps_mem_names [];	/* Names of memory blocks. */

#ifdef PROFILE
EXT_PROF_PID (rtps_w_create)
EXT_PROF_PID (rtps_w_delete)
EXT_PROF_PID (rtps_w_new)
EXT_PROF_PID (rtps_w_remove)
EXT_PROF_PID (rtps_w_urgent)
EXT_PROF_PID (rtps_w_write)
EXT_PROF_PID (rtps_w_dispose)
EXT_PROF_PID (rtps_w_unregister)
EXT_PROF_PID (rtps_w_rloc_add)
EXT_PROF_PID (rtps_w_rloc_rem)
EXT_PROF_PID (rtps_w_proxy_add)
EXT_PROF_PID (rtps_w_proxy_rem)
EXT_PROF_PID (rtps_w_resend)
EXT_PROF_PID (rtps_w_update)
EXT_PROF_PID (rtps_do_changes)
EXT_PROF_PID (rtps_send_msgs)
EXT_PROF_PID (rtps_r_create)
EXT_PROF_PID (rtps_r_delete)
EXT_PROF_PID (rtps_r_proxy_add)
EXT_PROF_PID (rtps_r_proxy_rem)
EXT_PROF_PID (rtps_r_unblock)
EXT_PROF_PID (rtps_rx_msgs)
EXT_PROF_PID (rtps_rx_data)
EXT_PROF_PID (rtps_rx_gap)
EXT_PROF_PID (rtps_rx_hbeat)
EXT_PROF_PID (rtps_rx_acknack)
EXT_PROF_PID (rtps_rx_inf_ts)
EXT_PROF_PID (rtps_rx_inf_rep)
EXT_PROF_PID (rtps_rx_inf_dst)
EXT_PROF_PID (rtps_rx_inf_src)
EXT_PROF_PID (rtps_rx_data_frag)
EXT_PROF_PID (rtps_rx_nack_frag)
EXT_PROF_PID (rtps_rx_hbeat_frag)
EXT_PROF_PID (rtps_tx_data)
EXT_PROF_PID (rtps_tx_gap)
EXT_PROF_PID (rtps_tx_hbeat)
EXT_PROF_PID (rtps_tx_acknack)
EXT_PROF_PID (rtps_tx_inf_ts)
EXT_PROF_PID (rtps_tx_inf_rep)
EXT_PROF_PID (rtps_tx_inf_dst)
EXT_PROF_PID (rtps_tx_inf_src)
EXT_PROF_PID (rtps_pw_start)
EXT_PROF_PID (rtps_pw_new)
EXT_PROF_PID (rtps_pw_send)
EXT_PROF_PID (rtps_pw_rem)
EXT_PROF_PID (rtps_pw_finish)
EXT_PROF_PID (rtps_bw_start)
EXT_PROF_PID (rtps_bw_new)
EXT_PROF_PID (rtps_bw_send)
EXT_PROF_PID (rtps_bw_rem)
EXT_PROF_PID (rtps_bw_finish)
EXT_PROF_PID (rtps_rw_start)
EXT_PROF_PID (rtps_rw_new)
EXT_PROF_PID (rtps_rw_send)
EXT_PROF_PID (rtps_rw_rem)
EXT_PROF_PID (rtps_rw_finish)
EXT_PROF_PID (rtps_rw_acknack)
EXT_PROF_PID (rtps_rw_hb_to)
EXT_PROF_PID (rtps_rw_alive_to)
EXT_PROF_PID (rtps_rw_nresp_to)
EXT_PROF_PID (rtps_br_start)
EXT_PROF_PID (rtps_br_data)
EXT_PROF_PID (rtps_br_finish)
EXT_PROF_PID (rtps_rr_start)
EXT_PROF_PID (rtps_rr_data)
EXT_PROF_PID (rtps_rr_finish)
EXT_PROF_PID (rtps_rr_gap)
EXT_PROF_PID (rtps_rr_hbeat)
EXT_PROF_PID (rtps_rr_alive_to)
EXT_PROF_PID (rtps_rr_do_ack)
EXT_PROF_PID (rtps_rr_proc)
#endif

#ifdef CTRACE_USED

enum {
	RTPS_W_CREATE, RTPS_W_DELETE,
	RTPS_W_N_CHANGE, RTPS_W_RM_CHANGE,
	RTPS_W_URG_CHANGE, RTPS_W_ALIVE,
	RTPS_W_WRITE, RTPS_W_DISPOSE, RTPS_W_UNREG,
	RTPS_W_RLOC_ADD, RTPS_W_RLOC_REM,
	RTPS_W_PROXY_ADD, RTPS_W_PROXY_REMOVE,
	RTPS_W_RESEND, RTPS_W_UPDATE,
	RTPS_SCH_W_PREP, RTPS_SCH_RDR, RTPS_SCH_TX, RTPS_SCH_TXD,
	RTPS_R_CREATE, RTPS_R_DELETE,
	RTPS_R_PROXY_ADD, RTPS_R_PROXY_REMOVE,
	RTPS_R_UNBLOCK,
	RTPS_RX_MSGS,
	RTPS_RX_DATA, RTPS_RX_GAP, RTPS_RX_HBEAT, RTPS_RX_ACKNACK,
	RTPS_RX_INFO_TS, RTPS_RX_INFO_REPLY, RTPS_RX_INFO_DEST, RTPS_RX_INFO_SRC,
	RTPS_RX_DFRAG, RTPS_RX_NACK_FRAG, RTPS_RX_HBEAT_FRAG,
	RTPS_TX_DATA, RTPS_TX_GAP, RTPS_TX_HBEAT, RTPS_TX_ACKNACK,
	RTPS_TX_INFO_TS, RTPS_TX_INFO_REPLY, RTPS_TX_INFO_DEST, RTPS_TX_INFO_SRC,
	RTPS_TX_NACK_FRAG, RTPS_TX_HBEAT_FRAG,
	RTPS_SLW_BE_START, RTPS_SLW_BE_NEW,
	RTPS_SLW_BE_SEND, RTPS_SLW_BE_REM, RTPS_SLW_BE_FINISH,
	RTPS_SFW_BE_START, RTPS_SFW_BE_NEW,
	RTPS_SFW_BE_SEND, RTPS_SFW_BE_REM, RTPS_SFW_BE_FINISH,
	RTPS_SFW_REL_START, RTPS_SFW_REL_NEW,
	RTPS_SFW_REL_SEND, RTPS_SFW_REL_REM, RTPS_SFW_REL_FINISH,
	RTPS_SFW_REL_ACKNACK, RTPS_SFW_REL_NACKFRAG,
	RTPS_SFW_HB_TO, RTPS_SFW_ALIVE_TO, RTPS_SFW_NACK_RSP_TO,
	RTPS_SFR_BE_START, RTPS_SFR_BE_DATA, RTPS_SFR_BE_FINISH,
	RTPS_SFR_REL_START, RTPS_SFR_REL_DATA, RTPS_SFR_REL_FINISH,
	RTPS_SFR_REL_GAP, RTPS_SFR_REL_HBEAT, RTPS_SFR_REL_HBFRAG, 
	RTPS_SFR_ALIVE_TO, RTPS_SFR_REL_DO_ACK,
	RTPS_SFR_PROCESS
};

extern const char *rtps_fct_str [];

#endif /* CTRACE_USED */

#endif /* !__rtps_priv_h_ */

