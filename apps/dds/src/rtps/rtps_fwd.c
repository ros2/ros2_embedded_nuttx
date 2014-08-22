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

/* rtps_fwd.c -- RTPS forwarding engine with hybrid bridge/router functionality
		 using the following mechanisms for relaying frames:

	- GUID-prefix learning of received RTPS messages.
	- SPDP messages are both received locally as well as forwarded to all
	  ports except the receiving port.
	- Meta/user data messages that include the InfoDestination submessage
	  are either received locally (local GUID-prefix) or are forwarded to
	  the port on which the GUID-prefix was learned.
	- Multicast Meta/user data messages, or meta/user data messages without
	  an embedded InfoDestination submessage are forwarded by examining the
	  local discovery database and deriving from that data the interested
	  participants and the associated ports they own.  This can be a
	  combination of the message being received locally and in addition
	  being forwarded to other ports from the receiving port.
 */

#ifdef DDS_FORWARD

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "log.h"
#include "prof.h"
#include "error.h"
#include "debug.h"
#include "sock.h"
#include "thread.h"
#include "dds/dds_error.h"
#include "ri_tcp.h"
#include "rtps_msg.h"
#include "rtps_mux.h"
#include "rtps_ft.h"
#include "rtps_fwd.h"

#ifdef DDS_DEBUG
/*#define FORWARD_TRC*/
/*#define FORWARD_TRC_FWD*/
#endif
#ifdef FORWARD_TRC
#define	LOG_MESSAGES
#define LOG_MSG_DATA		0
#define	fwd_print_end()		if (fwd_trace && ++fwd_tcnt>=fwd_tmax) fwd_trace=0
#define	fwd_print(s)		if (fwd_trace) log_printf(RTPS_ID, 0, s)
#define	fwd_print1(s,a)		if (fwd_trace) log_printf(RTPS_ID, 0, s, a)
#define	fwd_print2(s,a1,a2)	if (fwd_trace) log_printf(RTPS_ID, 0, s, a1, a2)

#ifndef FWD_TRACE_DEF
#define	FWD_TRACE_DEF	0
#endif
#ifndef FWD_TMAX
#define	FWD_TMAX	1000000
#endif

static int		fwd_trace = FWD_TRACE_DEF;
static unsigned		fwd_tcnt = 0;
static unsigned		fwd_tmax = FWD_TMAX;

static void fwd_print_locator_list (LocatorList_t list)
{
	LocatorNode_t	*np;
	LocatorRef_t	*rp;

	foreach_locator (list, rp, np) {
		fwd_print1 (" %s", locator_str (&np->locator));
	}
}

#else
#define	fwd_print_end()
#define	fwd_print(s)
#define	fwd_print1(s,a)
#define	fwd_print2(s,a1,a2)
#define	fwd_print_locator_list(list)
#endif

typedef struct fwd_stats_st {
	unsigned long	msgs_rxed;
	unsigned long	msgs_data_uc;
	unsigned long	msgs_data_uco;
	unsigned long	msgs_data_mc;
	unsigned long	msgs_data_mco;
	unsigned long	msgs_no_peer;
	unsigned long	msgs_no_ep;
	unsigned long	msgs_add_fwdest;
	unsigned long	msgs_fwded;
	unsigned long	msgs_local;
	unsigned long	msgs_no_dest;
	unsigned long	msgs_looped;
	unsigned long	msgs_loopedi;
	unsigned long	msgs_reqed;
	unsigned long	msgs_h_sent;
	unsigned long	msgs_sent;
	unsigned long	msgs_not_sent;
	unsigned long	fwd_nomem;
} FwdStats_t;

typedef struct fwd_data_st {
	Domain_t	*domain;
	unsigned	dindex;
	FTTable_t	*table;
	FwdStats_t	stats;
} FwdData_t;

static FTTable_t	*fwd_table;
static MEM_DESC		fwd_msg_md;
static MEM_DESC		fwd_el_md;
static RMRXF 		fwd_rtps_rxfct;
static FwdData_t	*fwd [MAX_DOMAINS + 1];


/* fwd_add_locators -- Add a set of destination locators. */

static void fwd_add_locators (LocatorList_t *rem_locators,
			      LocatorList_t list,
			      unsigned      handle,
			      LocatorKind_t kind)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	Locator_t	*lp;

	foreach_locator (list, rp, np) {
		lp = &np->locator;
		if ((handle && lp->handle == handle) ||
		    (kind && (lp->kind & kind) != 0))
			continue;

		locator_list_copy_node (rem_locators, rp->data);
		fwd_print1 (" %s", locator_str (&rp->data->locator));
	}
}

/* fwd_add_dest -- Add a destination locator for a specific GUID prefix. */

static void fwd_add_dest (FTTable_t     *ftp,
			  LocatorList_t *rem_uc_locs,
			  LocatorList_t *rem_mc_locs,
			  int           *local_dsts,
			  GuidPrefix_t  *prefix,
			  unsigned      handle,
			  LocatorKind_t kind,
			  Mode_t        mode,
			  int           local_src)
{
	FTEntry_t	*p;
	LocatorList_t	uc_list, mc_list;
#ifdef FORWARD_TRC
	char		buffer [40];
#endif

	p = ft_lookup (ftp, prefix, NULL);
	if (!p) {
		fwd_print1 ("..FWD-AddDest: prefix (%s) not found!\r\n", guid_prefix_str (prefix, buffer));
		return;
	}
	if (p->id) {
		fwd_print1 ("..FWD-AddDest: local destination prefix (%s)!\r\n", guid_prefix_str (prefix, buffer));
		if (local_dsts)
			(*local_dsts)++;
		return;
	}
	if (local_src && p->local)
		return;

	mode &= ~1;	/* Check MC-list first. */
	mc_list = ft_get_locators (p, mode, 1);
	if (!mc_list)
		mc_list = ft_get_locators (p, mode, 0);
	uc_list = ft_get_locators (p, mode + 1, 1);
	if (!uc_list)
		uc_list = ft_get_locators (p, mode + 1, 0);

	if (!uc_list && !mc_list) {
		fwd_print1 ("..FWD-AddDest: no locators for prefix (%s)!\r\n", guid_prefix_str (prefix, buffer));
		return;
	}
	fwd_print1 ("..FWD-AddDest: add locators for prefix (%s): UC:", guid_prefix_str (prefix, buffer));
	fwd_add_locators (rem_uc_locs, (uc_list) ? uc_list : mc_list, handle, kind);
	fwd_print (" MC:");
	fwd_add_locators (rem_mc_locs, (mc_list) ? mc_list : uc_list, handle, kind);
	fwd_print ("\r\n");
}

/* loc_list_set -- Add locators to a list, copying from another list and
		   filtering the correct ones if necessary. */

static void loc_list_set (LocatorList_t *dest,
			  LocatorList_t src,
			  LocatorKind_t k,
			  int           mcast,
			  LocatorKind_t *kinds)
{
	LocatorNode_t	*np;
	LocatorRef_t	*rp;
	Locator_t	*lp;

	foreach_locator (src, rp, np) {
		lp = &np->locator;
		if ((lp->kind & k) == 0)
			continue;

		if (mcast && (lp->kind & LOCATOR_KINDS_TCP) != 0)
			continue;

		*kinds |= lp->kind;
		locator_list_copy_node (dest, np);
	}
}

/* fwd_populate_locators -- Add the default discovery locators to an entry. */

static void fwd_populate_locators (FTEntry_t *p, Participant_t *pp, int update)
{
	Mode_t		m;
#ifdef FORWARD_TRC
	char		buffer [40];
#endif
	LocatorKind_t	kinds, ekinds;

	fwd_print1 (".FWD-PopulateLocators (%s).\r\n", guid_prefix_str (&p->guid_prefix, buffer));
	if (update)
		for (m = MODE_MIN; m <= MODE_MAX; m++) {
			if (p->locs [m][0])
				locator_list_delete_list (&p->locs [m][0]);
			if (p->locs [m][1])
				locator_list_delete_list (&p->locs [m][1]);
		}

	kinds = pp->p_domain->kinds;
	ekinds = 0;
#ifdef DDS_SECURITY
	if (pp->p_domain->security 
	    /* If native security, allow UDP locators */
#ifdef DDS_NATIVE_SECURITY
	    && (pp->p_sec_caps & (SECC_DDS_SEC | (SECC_DDS_SEC << SECC_LOCAL))) == 0
#endif
	    && (kinds & LOCATOR_KINDS_UDP) != 0) {
		loc_list_set (&p->locs [META_UCAST][0], pp->p_sec_locs, LOCATOR_KINDS_UDP, 0, &ekinds);
		loc_list_set (&p->locs [USER_UCAST][0], pp->p_sec_locs, LOCATOR_KINDS_UDP, 0, &ekinds);
		kinds &= ~LOCATOR_KINDS_UDP;
	}
#endif
	loc_list_set (&p->locs [META_MCAST][0], pp->p_meta_mcast, kinds, 1, &ekinds);
	loc_list_set (&p->locs [META_UCAST][0], pp->p_meta_ucast, kinds, 0, &ekinds);
	loc_list_set (&p->locs [USER_MCAST][0], pp->p_def_mcast, kinds, 1, &ekinds);
	loc_list_set (&p->locs [USER_UCAST][0], pp->p_def_ucast, kinds, 0, &ekinds);
	p->kinds = ekinds;

	/* If there aren't any UDP locators, entry may not be local. */
	if ((p->kinds & LOCATOR_KINDS_UDP) == 0)
		p->local = 0;
}

/* list_add_locator -- Add a new locator to a list from RME message data. */

static LocatorNode_t *list_add_locator (LocatorList_t *list,
					RME           *mep,
					unsigned      ofs)
{
	LocatorNode_t	*np;
	Locator_t	reply_loc, *locp;
	uint32_t	kind, t;

	locp = (Locator_t *) SUBMSG_DATA (mep,
					  ofs,
					  MSG_LOCATOR_SIZE,
					  &reply_loc);
	if (locp != &reply_loc) {
		memcpy (&reply_loc, locp, MSG_LOCATOR_SIZE);
		locp = &reply_loc;
	}
	if ((mep->flags & RME_SWAP) != 0) {
		kind = (unsigned) locp->kind;
		SWAP_UINT32 (kind, t);
		locp->kind = (LocatorKind_t) kind;
		SWAP_UINT32 (locp->port, t);
	}
	np = locator_list_add (list, locp->kind, locp->address, locp->port, 0, 0, 0, 0);
	return (np);
}

/* fwd_participant_lookup -- Lookup a participant. */

static inline Participant_t *fwd_participant_lookup (Domain_t *dp, GuidPrefix_t *prefix)
{
	if (guid_prefix_eq (dp->participant.p_guid_prefix, *prefix))
		return (&dp->participant);
	else
		return (participant_lookup (dp, prefix));
}

/* fwd_check_info_source -- Check the source GUID prefix from an InfoSource
			    submessage.  If our own prefix is in there, then we
			    must have sent this already, and we need to handle
			    this as a looped message. */

static int fwd_check_info_source (FwdData_t *fp, RME *mep, GuidPrefix_t *prefix)
{
	InfoSourceSMsg	smsg, *p;
	unsigned	ofs;

	for (ofs = 0;
	     ofs + sizeof (InfoSourceSMsg) < mep->length;
	     ofs += sizeof (InfoSourceSMsg)) {
		p = (InfoSourceSMsg *) SUBMSG_DATA (mep, ofs, 
						sizeof (InfoSourceSMsg), &smsg);
		if (!p)
			break;

		if (!ofs)
			*prefix = p->guid_prefix;

		if (guid_prefix_eq (fp->domain->participant.p_guid_prefix, 
				    p->guid_prefix))
			return (1);
	}
	return (0);
}

/* fwd_learn_info_reply -- Learn reply locators from an InfoReply submessage. */

static void fwd_learn_info_reply (FwdData_t       *fp,
				  RME	          *mep,
				  GuidPrefix_t    *prefix,
				  Participant_t   **ppp,
				  Mode_t          mode)
{
	FTEntry_t	*p;
	Participant_t	*pp;
	unsigned	minsize;
	uint32_t	nuclocs, nmclocs = 0, i, l, *lp, t, h;
	unsigned	ofs;
	LocatorList_t	uc_list, mc_list;
	Mode_t		rmode;
#ifdef FORWARD_TRC
	char		buffer [40];
#endif

	fwd_print1 ("..FWD-LearnInfoReply (%s).\r\n", guid_prefix_str (prefix, buffer));

	/* Parse the InfoReply message, collecting the given locator lists. */
	if (mep->length < sizeof (uint32_t))
		return;

	lp = (uint32_t *) SUBMSG_DATA (mep, 0, sizeof (uint32_t), &l);
	if (lp && (mep->flags & RME_SWAP) != 0) {
		SWAP_UINT32 (*lp, t);
	}
	nuclocs = (lp) ? *lp : 0;
	minsize = sizeof (uint32_t) + nuclocs * MSG_LOCATOR_SIZE;
	if ((mep->header.flags & SMF_MULTICAST) != 0) {
		if (mep->length < minsize + sizeof (uint32_t))
			minsize = ~0;
		else {
			lp = (uint32_t *) SUBMSG_DATA (mep, minsize,
						    sizeof (uint32_t), &l);
			if (lp && (mep->flags & RME_SWAP) != 0) {
				SWAP_UINT32 (*lp, t);
			}
			nmclocs = (lp) ? *lp : 0;
			minsize += sizeof (uint32_t) + nmclocs * MSG_LOCATOR_SIZE;
		}
	}
	if (mep->length < minsize) /* Invalid message length! */
		return;

	uc_list = mc_list = NULL;
	for (i = 0, ofs = sizeof (uint32_t);
	     i < nuclocs;
	     i++, ofs += MSG_LOCATOR_SIZE)
		if (!list_add_locator (&uc_list, mep, ofs))
			goto clean_exit;

	if (nmclocs)
		for (i = 0, ofs += sizeof (uint32_t);
		     i < nmclocs;
		     i++, ofs += MSG_LOCATOR_SIZE)
			if (!list_add_locator (&mc_list, mep, ofs))
				goto clean_exit;

	/* Create a new lookup table entry if necessary. */
	if (!*ppp)
		*ppp = pp = fwd_participant_lookup (fp->domain, prefix);
	else
		pp = *ppp;
	p = ft_lookup (fp->table, prefix, &h);
	if (!p) {
		fwd_print ("..FWD-LearnInfoReply: add new entry.\r\n");
		p = ft_add (fp->table, h, prefix, 0, LTF_AGE);
		if (!p)
			goto clean_exit;

		if (pp) /* We already know the participant: add locators. */
			fwd_populate_locators (p, pp, 0);
	}
	if (p->parent)
		goto clean_exit;

	/* Lookup table entry exists now.  Add the reply locators. */
	if (mode == META_MCAST || mode == META_UCAST)
		rmode = META_MCAST;
	else
		rmode = USER_MCAST;
	if (mc_list) {
		if (p->locs [rmode][1])
			locator_list_delete_list (&p->locs [rmode][1]);
		p->locs [rmode][1] = mc_list;
	}
	if (uc_list) {
		if (p->locs [rmode + 1][1])
			locator_list_delete_list (&p->locs [rmode + 1][1]);
		p->locs [rmode + 1][1] = uc_list;
	}
	p->flags |= LTF_INFO_REPLY;
	return;

    clean_exit:
	if (uc_list)
		locator_list_delete_list (&uc_list);
	if (mc_list)
		locator_list_delete_list (&mc_list);
}

/* meta_mcast_learn -- Set locators to a lookup table entry from an SPDP message.
		       Determine if this is the 1st hop or an nth hop by checking
		       the transport.  If this is a 1st hop, we don't do anything
		       and we leave it to discovery to populate the table.
		       Otherwise, we copy the locators from the 1st hop entry. */

static void meta_mcast_learn (FwdData_t *fp, FTEntry_t *p, const Locator_t *src)
{
#ifdef DDS_TCP
	FTEntry_t 	*pp;
	GuidPrefix_t	prefix;

	fwd_print ("..FWD-meta_mc_learn: ");
	if ((src->kind & (LOCATOR_KIND_TCPv4 | LOCATOR_KIND_TCPv6)) == 0 ||
	    !rtps_tcp_peer (src->handle,
	    		    fp->domain->domain_id,
			    fp->domain->participant_id,
			    &prefix) ||
	    !memcmp (p->guid_prefix.prefix, &prefix.prefix, GUIDPREFIX_SIZE - 1)) {
    						/* -1 to ignore the count field! */
		fwd_print ("1st-hop!\r\n");
		return;	/* Must be 1st hop! */
	}

	/* Nth hop: this must be a child node -> find parent. */
	pp = ft_lookup (fp->table, &prefix, NULL);
	if (!pp) {
		fwd_print ("Nth-hop, but no parent!\r\n");
		return;	/* No parent: ignore. */
	}

	/* Valid parent. Link child node to the parent so the child locators
	   taken will be those of the parent. */
	p->parent = pp;
	pp->nchildren++;
	fwd_print ("Nth-hop!\r\n");
#else
	ARG_NOT_USED (fp)
	ARG_NOT_USED (p)
	ARG_NOT_USED (src)
#endif
}

/* fwd_learn_src -- Learn a reply locator from a received message. */

static void fwd_learn_source (FwdData_t       *fp,
			      GuidPrefix_t    *prefix,
			      Participant_t   *pp,
			      Mode_t          mode,
			      const Locator_t *src)
{
	FTEntry_t	*p;
	LocatorNode_t	*np;
	LocatorRef_t	*rp;
	unsigned	h;
	int		new_prefix = 0;
#ifdef FORWARD_TRC
	char		buffer [40];
#endif

	if (!pp)
		pp = participant_lookup (fp->domain, prefix);

	p = ft_lookup (fp->table, prefix, &h);
	if (!p) {
		fwd_print1 ("..FWD-LearnSrc: add new entry (%s)!\r\n",
					guid_prefix_str (prefix, buffer));
		p = ft_add (fp->table, h, prefix, 0, LTF_AGE);
		if (!p)
			return;

		new_prefix = 1;
		if (pp)
			fwd_populate_locators (p, pp, 0);
	}
	else if (p->parent || (p->flags & LTF_INFO_REPLY) != 0) {
		p->ttl = MAX_FWD_TTL;
		fwd_print1 ("..FWD-LearnSrc: prefix (%s) overruled by InfoReply!\r\n",
					guid_prefix_str (prefix, buffer));
		return;
	}
	if (p) {
		/*if (src->kind == LOCATOR_KIND_UDPv4 &&
		    p->locs [mode][0] &&
		    p->locs [mode][0]->data->locator.kind != LOCATOR_KIND_UDPv4)
		    	warn_printf ("Incorrect locator kind from learned data (prefix=%s)!", 
								guid_prefix_str (prefix, buffer));*/

		/* Remember the reply locator for this mode. */
		p->ttl = MAX_FWD_TTL;
		if (pp) {
			foreach_locator (p->locs [mode][0], rp, np)
				if (locator_addr_equal (&np->locator, src)) {
					locator_list_copy_node (&p->locs [mode][1], np);
					fwd_print2 ("..FWD-LearnSrc: found locator (%s) for prefix (%s).\r\n",
							locator_str (&np->locator),
							guid_prefix_str (prefix, buffer));
					return;
				}
        }
		else {
			fwd_print2 ("..FWD-LearnSrc: add locator (%s) for prefix (%s)!\r\n",
							locator_str (src),
							guid_prefix_str (prefix, buffer));
			ft_add_locator (p, src, mode, 1);
		}
		if (mode == META_MCAST && new_prefix)

			/* New SPDP participant!  Set locators, either from the
			   participant locators if connected directly, or by
			   deducing them from the reception channel. */
			meta_mcast_learn (fp, p, src);
	}
}

/* fwd_parse -- Parse an RTPS message and figure out all its destinations, local
		or remote.  Optionally learn reply locators from the message. */
		
static void fwd_parse (FwdData_t       *fp,
		       RMBUF           *msg,
		       LocatorList_t   *remote_dsts,
		       int             *local_dsts,
		       int             learn,
		       const Locator_t *src,
		       Mode_t          mode)
{
	RME		*mep;
	Participant_t	*src_p = NULL, **pp;
	Endpoint_t	*ep, *src_ep;
	Topic_t		*tp;
	SLNode_t	*slp;
	FTEntry_t	*p = NULL;
	LocatorList_t	rem_uc_locs = NULL;
	LocatorList_t	rem_mc_locs = NULL;
	GuidPrefix_t	src_prefix;
	EntityId_t	src_eid, dst_eid;
	unsigned	src_ofs, dst_ofs, b, m, h;
	int		got_dest = 0, learned = 0, udp_src, as_local;
	LocatorKind_t	suppress_kind;

	ft_entry (fp->table);
	if (local_dsts)
		*local_dsts = 0;
	if (src &&
	    (src->kind & LOCATOR_KINDS_UDP) != 0) {
		suppress_kind = LOCATOR_KIND_UDPv4 | LOCATOR_KIND_UDPv6;
		udp_src = as_local = 1;
	}
	else {
		suppress_kind = 0;
		udp_src = as_local = 0;
	}

	fwd_print2 (".FWD-Parse: parse message (suppress=0x%x, udp_src=%d).\r\n",
					suppress_kind, udp_src);

	/*******************************************/
	/*   Parse message and derive all dests.   */
	/*******************************************/

	/* Stop if we sent this ourselves. */
	src_prefix = msg->header.guid_prefix;
	if (learn &&
	    guid_prefix_eq (src_prefix, fp->domain->participant.p_guid_prefix)) {
		*remote_dsts = NULL;
		fwd_print (".FWD-Parse: looped message (MsgHeader).\r\n");
		fp->stats.msgs_looped++;
		ft_exit (fp->table);
		return;
	}

	/* Parse message, checking each submessage in turn.  Collecting
	   destinations as we process them ad= */
	for (mep = msg->first; mep; mep = mep->next) {
		if ((mep->flags & RME_HEADER) == 0)
			continue;

		if (mep->header.id == ST_INFO_DST) {
			if (learn && !p) {
				p = ft_lookup (fp->table, &src_prefix, &h);
				if (p) {
					as_local = (p->local != 0);
					fwd_print1 (".FWD-Parse: INFO_DST: as_local=%d\r\n", as_local);
				}
			}
			fwd_add_dest (fp->table,
				      &rem_uc_locs, &rem_mc_locs,
				      local_dsts,
				      (GuidPrefix_t *) mep->data,
				      (src) ? src->handle : 0,
				      suppress_kind,
				      mode,
				      as_local);
			got_dest = 1;
			fp->stats.msgs_data_uc++;
			fp->stats.msgs_data_uco += mep->header.length;
			continue;
		}
		else if (mep->header.id == ST_INFO_SRC) {

			/* Abort if we sent this ourselves. */
			if (learn &&
			    fwd_check_info_source (fp, mep, &src_prefix)) {
				locator_list_delete_list (&rem_uc_locs);
				locator_list_delete_list (&rem_mc_locs);
				if (local_dsts)
					*local_dsts = 0;
				*remote_dsts = NULL;
				fwd_print (".FWD-Parse: looped message (InfoSrc).\r\n");
				fp->stats.msgs_loopedi++;
				ft_exit (fp->table);
				return;
			}
			continue;
		}
		else if (mep->header.id == ST_INFO_REPLY) {
			if (learn) {
				fwd_learn_info_reply (fp, mep, &src_prefix,
							     &src_p, mode);
				learned = 1;
				udp_src = as_local = 0;
				continue;
			}
		}
		if (got_dest) {	/* When learning: want InfoReply/Source! */
			if (learn && !learned)
				continue;
			else
				break;
		}
		if (mep->header.id == ST_DATA ||
		    mep->header.id == ST_DATA_FRAG) {
			dst_ofs = 4;
			src_ofs = 8;
		}
		else if (mep->header.id == ST_ACKNACK ||
		         mep->header.id == ST_NACK_FRAG) {
			src_ofs = 0;
			dst_ofs = 4;
		}
		else if (mep->header.id == ST_HEARTBEAT ||
			 mep->header.id == ST_GAP ||
			 mep->header.id == ST_HEARTBEAT_FRAG) {
			dst_ofs = 0;
			src_ofs = 4;
		}
		else
			continue;

		memcpy32 (&dst_eid.w, mep->data + dst_ofs);
		memcpy32 (&src_eid.w, mep->data + src_ofs);

		if (mep->header.id == ST_DATA ||
		    mep->header.id == ST_DATA_FRAG) {
			if (!dst_eid.w) {
				fp->stats.msgs_data_mc++;
				fp->stats.msgs_data_mco += mep->header.length;
			}
			else {
				fp->stats.msgs_data_uc++;
				fp->stats.msgs_data_uco += mep->header.length;
			}
		}

		/* Find entity id in list of endpoints belonging to src_p. */
		if (!src_p)
			src_p = fwd_participant_lookup (fp->domain, &src_prefix);
		if (learn && !p) {
			p = ft_lookup (fp->table, &src_prefix, &h);
			if (p) {
				as_local = (p->local != 0);
				fwd_print1 (".FWD-Parse: existing source: as_local=%d\r\n", as_local);
			}
			else if (udp_src) {
				fwd_print ("FWD-Parse: add new (local) entry.\r\n");
				p = ft_add (fp->table, h, &src_prefix, 0, LTF_AGE);
				if (!p) {
					warn_printf ("FWD: ft_add returned NULL!");
					ft_exit (fp->table);
                                        return;
				}
				p->local = LT_LOCAL_TO;
			}
		}

		/* Check if SPDP message. */
		if (src_eid.w == rtps_builtin_eids [EPB_PARTICIPANT_W].w) {
			if (p && udp_src) {
				fwd_print ("FWD-Parse: mark as local.\r\n");
				p->local = LT_LOCAL_TO;
			}
			if (local_dsts)
				*local_dsts = 1;

			fwd_print (".FWD-Parse: add SPDP locators: ");
			/*if (src &&
			    (src->kind & (LOCATOR_KIND_TCPv4 | LOCATOR_KIND_TCPv6)) != 0)
				suppress_kind = LOCATOR_KIND_TCPv4 | LOCATOR_KIND_TCPv6;*/
			fwd_add_locators (&rem_uc_locs, fp->domain->dst_locs,
						(src) ? src->handle: 0, suppress_kind);
			fwd_print ("\r\n");
			rem_mc_locs = locator_list_clone (rem_uc_locs);
			break;
		}

		/* If unknown source, just receive it (probably discovery data). */
		if (!src_p) {
			if (local_dsts)
				*local_dsts = 1;
			fp->stats.msgs_no_peer++;
			continue;
		}
		src_ep = endpoint_lookup (src_p, &src_eid);
		if (!src_ep) {
			fp->stats.msgs_no_ep++;
			continue;
		}
		tp = src_ep->topic;
		if (mep->header.id == ST_ACKNACK ||
		    mep->header.id == ST_NACK_FRAG)	/* Check writers list. */
			ep = tp->writers;
		else
			ep = tp->readers;		/* Check readers list. */
		while (ep) {
			if (!dst_eid.w ||			/* Multicast? */
			    dst_eid.w == ep->entity_id.w) {	/* Real dest? */
				if (entity_local (ep->entity.flags)) {
					if (local_dsts)
						*local_dsts = 1;
				}
				else {
					fwd_add_dest (fp->table,
						      &rem_uc_locs, &rem_mc_locs,
						      local_dsts,
						      &ep->u.participant->p_guid_prefix,
						      (src) ? src->handle : 0,
						      suppress_kind,
						      mode,
						      as_local);
					fp->stats.msgs_add_fwdest++;
				}
			}
			ep = ep->next;
		}
		if ((src_eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MAJOR) ==
							ENTITY_KIND_BUILTIN) {
			for (b = EPB_PARTICIPANT_W, m = 1;
			     b < EPB_MAX;
			     b++, m <<= 1)
				if (src_eid.w == rtps_builtin_eids [b].w)
					break;

			if (b >= EPB_MAX)
				continue;

			sl_foreach (&fp->domain->peers, slp) {
				pp = (Participant_t **) sl_data_ptr (&fp->domain->peers, slp);
				if (((*pp)->p_builtins & m) != 0) {
					fwd_add_dest (fp->table,
						      &rem_uc_locs, &rem_mc_locs,
						      local_dsts,
						      &(*pp)->p_guid_prefix,
						      (src) ? src->handle : 0,
						      suppress_kind,
						      mode,
						      as_local);
					fp->stats.msgs_add_fwdest++;
				}
			}
		}
	}

#if 0
	/* The set of remote destinations is the shortest of the two sets
	   rem_uc_locs and rem_mc_locs with preference for the former. */
	if (rem_uc_locs)
		if (rem_mc_locs)
			if (locator_list_length (rem_mc_locs) <
			    locator_list_length (rem_uc_locs)) {
				*remote_dsts = rem_mc_locs;
				locator_list_delete_list (&rem_uc_locs);
			}
			else {
				*remote_dsts = rem_uc_locs;
				locator_list_delete_list (&rem_mc_locs);
			}
		else
			*remote_dsts = rem_uc_locs;
	else
		*remote_dsts = rem_mc_locs;
#endif
	if (rem_uc_locs) {
		*remote_dsts = rem_uc_locs;
		locator_list_delete_list (&rem_mc_locs);
	}
	else
		*remote_dsts = rem_mc_locs;


	/********************************************/
	/*   Learn GUID/locator info from message.  */
	/********************************************/

	fwd_print (".FWD-Parse: parsing complete.\r\n");
	if (learn && !learned && src && src->port)
		fwd_learn_source (fp, &src_prefix, src_p, mode, src);

	ft_exit (fp->table);
	fwd_print (".FWD-Parse: done.\r\n");
}

/* rfwd_add_info_reply_locators -- Add a locator list to a InfoReply data. */

static size_t rfwd_add_info_reply_locators (unsigned char *dp, LocatorList_t locs)
{
	LocatorNode_t	*np;
	LocatorRef_t	*rp;
	unsigned char	*orig_dp = dp;
	uint32_t	n;

	n = locator_list_length (locs);
	memcpy32 (dp, &n);
	dp += 4;
	foreach_locator (locs, rp, np) {
		n = np->locator.kind; 
		memcpy32 (dp, &n);
		dp += 4;
		n = np->locator.port;
		memcpy32 (dp, &n);
		dp += 4;
		memcpy (dp, np->locator.address, 16);
		dp += 16;
	}
	return (dp - orig_dp);
}

#ifdef DDS_SECURITY

/* rfwd_uc_secure_locators -- Return a list of secure locators for InfoReply. */

static LocatorList_t rfwd_uc_secure_locators (Participant_t *pp, Mode_t m)
{
	LocatorList_t	slist, list = NULL;
	LocatorRef_t	*rp;
	LocatorNode_t	*np;

	if (m == META_MCAST || m == META_UCAST)
		slist = pp->p_meta_ucast;
	else
		slist = pp->p_def_ucast;

	foreach_locator (slist, rp, np)
		if ((np->locator.kind & LOCATOR_KINDS_UDP) == 0)
			locator_list_copy_node (&list, np);
	foreach_locator (pp->p_sec_locs, rp, np)
		if ((np->locator.kind & LOCATOR_KINDS_UDP) != 0)
			locator_list_copy_node (&list, np);

	return (list);
}

#endif

/* rfwd_forward -- Forward a message to other connections. */

static void rfwd_forward (FwdData_t *fp, LocatorList_t dests, RMBUF *msg, Mode_t m)
{
	RMBUF		*new_mp;
	RME		*new_mep, *mep, *isrc = NULL;
	NOTIF_DATA	*ndp;
	Change_t	*cp;
	String_t	*sp;
	unsigned char	*dp, *p;
	InfoSourceSMsg	*isp;
	LocatorList_t	uc_list, mc_list;
	unsigned	dsize;
	size_t		xlen;
	int		add_info_reply = 0;

	/* Create a new message header with our own data. */
	new_mp = mds_pool_alloc (fwd_msg_md);
	if (!new_mp) {
		fp->stats.fwd_nomem++;
		return;
	}
	new_mp->first = new_mp->last = NULL;
	new_mp->size = msg->size;
	new_mp->prio = msg->prio;
	new_mp->users = 1;
	new_mp->next = NULL;
	new_mp->element.flags = 0;
	protocol_set (new_mp->header.protocol);
	version_init (new_mp->header.version);
	vendor_id_init (new_mp->header.vendor_id);
	guid_prefix_cpy (new_mp->header.guid_prefix,
			 fp->domain->participant.p_guid_prefix);

	/* Copy and append all original message submessages except for
	   InfoSource and InfoReply submessages. */
	for (mep = msg->first; mep; mep = mep->next) {
		if ((mep->flags & RME_HEADER) != 0) {
			if (mep->header.id == ST_INFO_SRC) {
				if (!isrc)
					isrc = mep;
				continue;
			}
			else if (mep->header.id == ST_INFO_REPLY)
				continue;
			else if	(mep->header.id == ST_HEARTBEAT ||
			         mep->header.id == ST_HEARTBEAT_FRAG ||
				 mep->header.id == ST_ACKNACK ||
				 mep->header.id == ST_NACK_FRAG)
				add_info_reply = 1;
		}
		new_mep = mds_pool_alloc (fwd_el_md);
		if (!new_mep) {
			rtps_free_messages (new_mp);
			fp->stats.fwd_nomem++;
			return;
		}
		xlen = RME_HDRSIZE - sizeof (unsigned char *) * 2;
		new_mep->next = NULL;
		if (mep->data == mep->d) {
			new_mep->data = new_mep->d;
			xlen += mep->length;
		}
		else {
			new_mep->data = mep->data;
			if ((mep->flags & RME_NOTIFY) != 0) {
				xlen += sizeof (NOTIF_DATA);
				ndp = (NOTIF_DATA *) mep->d;
				if (ndp->type == NT_CACHE_FREE) {
					cp = (Change_t *) ndp->arg;
					rcl_access (cp);
					cp->c_nrefs++;
					rcl_done (cp);
				}
				else if (ndp->type == NT_STR_FREE) {
					sp = (String_t *) ndp->arg;
					str_ref (sp);
				}
			}
		}
		memcpy (&new_mep->db, &mep->db, xlen);

		if ((new_mep->flags & RME_SWAP) != 0)
			memcswap16 (&new_mep->header.length, &mep->header.length);

		new_mep->flags &= ~RME_CONTAINED;
		if (new_mep->db) {
			rcl_access (new_mep->db);
			new_mep->db->nrefs++;
			rcl_done (new_mep->db);
		}
		if (new_mp->first)
			new_mp->last->next = new_mep;
		else
			new_mp->first = new_mep;
		new_mp->last = new_mep;
	}

	/* Only send InfoReply if a response is required. */
	if (!add_info_reply)
		goto skip_info_reply;

	/* Prepend an InfoReply submessage. */
	new_mep = mds_pool_alloc (fwd_el_md);
	if (!new_mep) {
		rtps_free_messages (new_mp);
		fp->stats.fwd_nomem++;
		return;
	}
	new_mep->next = new_mp->first;
	new_mp->first = new_mep;
	new_mep->flags = RME_HEADER;
	new_mep->header.id = ST_INFO_REPLY;
	new_mep->header.flags = SMF_CPU_ENDIAN;
	new_mep->header.length = 0;
	new_mep->pad = 0;
#ifdef DDS_SECURITY
	if (fp->domain->security && 
	    (fp->domain->participant.p_sec_caps & SECC_DDS_SEC) == 0) {
		uc_list = rfwd_uc_secure_locators (&fp->domain->participant, m);
		mc_list = NULL;
	}
	else {
#endif
		if (m == META_MCAST || m == META_UCAST) {
			uc_list = fp->domain->participant.p_meta_ucast;
			mc_list = fp->domain->participant.p_meta_mcast;
		}
		else {
			uc_list = fp->domain->participant.p_def_ucast;
			mc_list = fp->domain->participant.p_def_mcast;
		}
#ifdef DDS_SECURITY
	}
#endif
	dsize = sizeof (uint32_t) + locator_list_length (uc_list) * MSG_LOCATOR_SIZE;
	if (mc_list) {
		dsize += sizeof (uint32_t) + locator_list_length (mc_list) * MSG_LOCATOR_SIZE;
		new_mep->header.flags |= SMF_MULTICAST;
	}
	if (dsize > MAX_ELEMENT_DATA) {
		new_mep->db = db_alloc_data (dsize, 1);
		if (!new_mep->db) {
			rtps_free_messages (new_mp);
			fp->stats.fwd_nomem++;
			return;
		}
		new_mep->data = new_mep->db->data;
	}
	else {
		new_mep->db = NULL;
		new_mep->data = new_mep->d;
	}
	new_mep->length = new_mep->header.length = dsize;
	dp = new_mep->data;
	dp += rfwd_add_info_reply_locators (dp, uc_list);
	if (mc_list)
		rfwd_add_info_reply_locators (dp, mc_list);
	new_mp->size += new_mep->length + sizeof (SubmsgHeader);
#ifdef DDS_SECURITY
	if (fp->domain->security &&
	    (fp->domain->participant.p_sec_caps & SECC_DDS_SEC) == 0)
		locator_list_delete_list (&uc_list);
#endif

    skip_info_reply:

	/* Prepend an InfoSource submessage, appending the previous source
	   information to the possibly already existing InfoSource info. */
	new_mep = mds_pool_alloc (fwd_el_md);
	if (!new_mep) {
		rtps_free_messages (new_mp);
		fp->stats.fwd_nomem++;
		return;
	}
	new_mep->next = new_mp->first;
	new_mp->first = new_mep;
	new_mep->flags = RME_HEADER;
	new_mep->header.id = ST_INFO_SRC;
	new_mep->header.flags = SMF_CPU_ENDIAN;
	new_mep->header.length = 0;
	new_mep->pad = 0;
	dsize = sizeof (InfoSourceSMsg);
	if (isrc)
		dsize += isrc->length;
	if (dsize > MAX_ELEMENT_DATA) {
		new_mep->db = db_alloc_data (dsize, 1);
		if (!new_mep->db) {
			rtps_free_messages (new_mp);
			fp->stats.fwd_nomem++;
			return;
		}
		new_mep->data = new_mep->db->data;
	}
	else {
		new_mep->db = NULL;
		new_mep->data = new_mep->d;
	}
	new_mep->length = new_mep->header.length = dsize;
	dp = new_mep->data;
	if (isrc) {
		p = (unsigned char *) SUBMSG_DATA (isrc, 0, dsize, dp);
		if (p != dp)
			memcpy (dp, p, dsize);
		dp += dsize;
	}
	isp = (InfoSourceSMsg *) dp;
	isp->unused = 0;
	version_set (isp->version, msg->header.version);
	vendor_id_set (isp->vendor, msg->header.vendor_id);
	guid_cpy (isp->guid_prefix, msg->header.guid_prefix);
	new_mp->size += new_mep->length + sizeof (SubmsgHeader);

	/* Message is complete now: send it to the remote destinations. */
#ifdef LOG_MESSAGES
#ifdef FORWARD_TRC_FWD
	if (fwd_trace) {
		log_printf (RTPS_ID, 0, "==> ");
		locator_list_log (RTPS_ID, 0, dests);
		log_printf (RTPS_ID, 0, "\r\n");
		rtps_log_message (RTPS_ID, 0, new_mp, 'F', LOG_MSG_DATA);
	}
#endif
#endif
	if (dests->next)
		rtps_locator_send_ll (fp->dindex, dests, 1, new_mp);
	else
		rtps_locator_send_ll (fp->dindex, &dests->data->locator, 0, new_mp);
	rtps_free_messages (new_mp);
}

/* fmode -- Convert locator flags to a valid mode index. */

static inline Mode_t fmode (unsigned flags, RMBUF *msgs)
{
	Mode_t	m;

	if ((msgs->element.flags & RME_USER) == 0)
		if ((flags & LOCF_MCAST) != 0)
			m = META_MCAST;
		else
			m = META_UCAST;
	else
		if ((flags & LOCF_MCAST) != 0)
			m = USER_MCAST;
		else
			m = USER_UCAST;
	return (m);
}

/* rfwd_receive -- Receive function for messages received by RTPS transports. */

static void rfwd_receive (unsigned id, RMBUF *msg, const Locator_t *src)
{
	FwdData_t	*fp;
	LocatorList_t	remote_dests;
	int		local_dests;
	Mode_t		m;

	/*********************/
	/*   Derive domain.  */
	/*********************/

	fp = (id <= MAX_DOMAINS) ? fwd [id] : NULL;
	if (!fp) {

		/* We don't have a context for this domain yet.
		   Just pretend its for us. */
		(*fwd_rtps_rxfct) (id, msg, src);
		return;
	}
	fwd_print1 ("FWD-Receive: <== %s\r\n", locator_str (src));
	fp->stats.msgs_rxed++;

#ifdef LOG_MESSAGES
#ifdef FORWARD_TRC_FWD
	if (fwd_trace)
       		rtps_log_message (RTPS_ID, 0, msg, 'R', LOG_MSG_DATA);
#endif
#endif

	/*************************************************/
	/*   Derive all source/dest info from message.   */
	/*************************************************/

	m = fmode (src->flags, msg);
	fwd_print1 ("FWD-Receive: mode = %u.\r\n", m);
	fwd_parse (fp, msg, &remote_dests, &local_dests, 1, src, m);

	/*************************************************/
	/*   Send message to all non-host destinations.	 */
	/*************************************************/

	if (remote_dests) { /* Send message on all locators. */
		fp->stats.msgs_fwded++;
		fwd_print ("FWD-Receive: forward to destinations:");
		fwd_print_locator_list (remote_dests);
		fwd_print ("\r\n");
		rfwd_forward (fp, remote_dests, msg, m);
		locator_list_delete_list (&remote_dests);
	}

	/*************************************************/
	/*   Send message to local host destinations.    */
	/*************************************************/

	if (local_dests && fwd_rtps_rxfct) {
		fp->stats.msgs_local++;
		fwd_print ("FWD-Receive: local receive.\r\n");
		fwd_print_end ();
		(*fwd_rtps_rxfct) (id, msg, src);
	}

	/*************************************************/
	/*   Cleanup messages if no destinations.        */
	/*************************************************/

	else {
		fwd_print ("FWD-Receive: not a local destination.\r\n");
		fwd_print_end ();
		fp->stats.msgs_no_dest++;
		rtps_free_messages (msg);
		return;
	}
}

/* rfwd_send -- Send a number of messages to the specified locator(s). */

void rfwd_send (unsigned id, void *dest, int dlist, RMBUF *msgs)
{
	FwdData_t	*fp;
	RMBUF		*msg, *next_msg;
	Locator_t	*lp;
	LocatorList_t	listp;
	LocatorList_t	remote_dests;
	Mode_t		mode;

	fp = (id <= MAX_DOMAINS) ? fwd [id] : NULL;
	if (!dlist)
		lp = (Locator_t *) dest;
	else {
		listp = (LocatorList_t) dest;
		lp = &listp->data->locator;
	}
	if (!fp || lp->handle) {
		if (fp)
			fp->stats.msgs_h_sent++;
		rtps_locator_send_ll (id, dest, dlist, msgs);
		rtps_free_messages (msgs);
		return;
	}
	mode = fmode (lp->flags, msgs);
	for (msg = msgs; msg; msg = next_msg) {
		next_msg = msg->next;
		msg->next = NULL;
		fp->stats.msgs_reqed++;
#ifdef FORWARD_TRC
		fwd_print ("FWD-Send: ==> ");
		if (dlist)
			fwd_print_locator_list (dest);
		else
			fwd_print1 ("%s", locator_str (lp));
		fwd_print ("\r\n");
#endif
#ifdef LOG_MESSAGES
#ifdef FORWARD_TRC_FWD
		if (fwd_trace)
			rtps_log_message (RTPS_ID, 0, msg, 'T', LOG_MSG_DATA);
#endif
#endif

		/*************************************************/
		/*   Derive all destinations from message.       */
		/*************************************************/

		fwd_parse (fp, msg, &remote_dests, NULL, 0, NULL, mode);

		/*************************************************/
		/*   Send message to all non-host destinations.	 */
		/*************************************************/

		if (remote_dests) { /* Send message on all locators. */
			fwd_print ("FWD-Send: send to:");
			fwd_print_locator_list (remote_dests);
			fwd_print ("\r\n");
			if (remote_dests->next)
				rtps_locator_send_ll (id, remote_dests, 1, msg);
			else
				rtps_locator_send_ll (id, &remote_dests->data->locator, 0, msg);
			locator_list_delete_list (&remote_dests);
			fp->stats.msgs_sent++;
		}
		else {
			fwd_print1 ("FWD-Send: no destinations for %s msg.\r\n", ft_mode_str (mode));
			fp->stats.msgs_not_sent++;
		}

		/*************************************************/
		/*   Cleanup messages when done.                 */
		/*************************************************/

		fwd_print_end ();
		rtps_free_messages (msg);
	}
}

/* rfwd_init -- Initialize the forwarding engine and returns the forwarder
		receive function. */

RMRXF rfwd_init (RMRXF rxfct, MEM_DESC msg_md, MEM_DESC el_md)
{
	fwd_rtps_rxfct = rxfct;
	fwd_msg_md = msg_md;
	fwd_el_md = el_md;
	return (rfwd_receive);
}

/* rfwd_domain_init -- Initialize forwarder data for a new domain. */

static int rfwd_domain_init (unsigned index)
{
	FwdData_t		*fp;
	DDS_ReturnCode_t	err;

	fp = fwd [index] = xmalloc (sizeof (FwdData_t));
	if (!fp) {
		warn_printf ("FWD: Can't create domain data for new domain!");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	fp->domain = domain_get (index, 0, &err);
	if (!fp->domain) {
		warn_printf ("FWD: Can't get domain pointer (error: %s)!", DDS_error (err));
		xfree (fp);
		fwd [index] = NULL;
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	fp->dindex = fp->domain->index;
	if (fwd_table) {
		fp->table = fwd_table;
		ft_reuse (fp->table);
	}
	else {
		fwd_table = fp->table = ft_new ();
		if (!fwd_table) {
			warn_printf ("FWD: Can't create lookup table for GUID prefixes!");
			xfree (fp);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
	}
	memset (&fp->stats, 0, sizeof (fp->stats));
	return (DDS_RETCODE_OK);
}

/* rfwd_domain_final -- Free forwarder data for a domain. */

static void rfwd_domain_final (unsigned index)
{
	FwdData_t	*fp;

	fp = fwd [index];
	if (!fp)
		return;

	fwd [index] = NULL;
	fwd_table = ft_cleanup (fp->table, fp->dindex);
	fp->domain = NULL;
	xfree (fp);
}

/* rfwd_final -- Cleanup all forwarding related resources. */

void rfwd_final (void)
{
	unsigned	i;

	fwd_rtps_rxfct = NULL;
	for (i = 0; i <= MAX_DOMAINS; i++)
		if (fwd [i])
			rfwd_domain_final (i);
	fwd_msg_md = NULL;
	fwd_el_md = NULL;
#ifdef FORWARD_TRC
	fwd_trace = FWD_TRACE_DEF;
	fwd_tcnt = 0;
	fwd_tmax = FWD_TMAX;
#endif
}

/* rfwd_locators_update -- All locators will be updated. */

int rfwd_locators_update (unsigned domain_id, unsigned id)
{
	FwdData_t	*fp;
	FTEntry_t	*p;
	unsigned	h;

	ARG_NOT_USED (domain_id)

	fwd_print1 ("FWD(%u): Locators update!\r\n", id);
	if (id > MAX_DOMAINS) {
		warn_printf ("FWD: invalid domain index (%u)!", id);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	fp = fwd [id];
	if (!fp)
		return (DDS_RETCODE_OK);

	ft_entry (fp->table);
	p = ft_lookup (fp->table, &fp->domain->participant.p_guid_prefix, &h);
	if (!p) {
		ft_exit (fp->table);
		return (DDS_RETCODE_OK);
	}
	locator_list_delete_list (&p->locs [META_MCAST][0]);
	locator_list_delete_list (&p->locs [META_UCAST][0]);
	locator_list_delete_list (&p->locs [USER_MCAST][0]);
	locator_list_delete_list (&p->locs [USER_UCAST][0]);
	ft_exit (fp->table);
	fwd_print_end ();
	return (DDS_RETCODE_OK);
}

/* rfwd_locator_add -- Add a port to the forwarder based on the given port
		       attributes. If not successful, a non-zero error code
		       is returned. */

int rfwd_locator_add (unsigned      domain_id,
		      LocatorNode_t *locator,
		      unsigned      id,
		      int           serve)
{
	FwdData_t	*fp;
	FTEntry_t	*p;
#ifdef DDS_SECURITY
	Domain_t	*dp;
#endif
	DDS_ReturnCode_t ret;
	unsigned	h;

	ARG_NOT_USED (domain_id)
	ARG_NOT_USED (serve)

	fwd_print2 ("FWD(%u): Add locator: %s\r\n", id, locator_str (&locator->locator));
	if (id > MAX_DOMAINS) {
		warn_printf ("FWD: invalid domain index (%u)!", id);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
#ifdef DDS_SECURITY
	dp = domain_get (id, 0, &ret);
	if (dp && dp->security &&
	    (((locator->locator.kind & LOCATOR_KINDS_UDP) != 0 && !locator->locator.sproto) ||
	     ((locator->locator.kind & ~LOCATOR_KINDS_UDP) != 0 && locator->locator.sproto))) {
		fwd_print1 ("FWD(%u): ignore locator!\r\n", id);
		return (DDS_RETCODE_OK);
	}
	fwd_print2 ("FWD(%u): flags = 0x%x!\r\n", id, locator->locator.flags);
#endif
	fp = fwd [id];

	if (!fp) {
		ret = rfwd_domain_init (id);
		if (ret)
			return (ret);

		fp = fwd [id];
	}
	if (fp) {
		ft_entry (fp->table);
		p = ft_lookup (fp->table, &fp->domain->participant.p_guid_prefix, &h);
		if (!p) {
			fwd_print1 ("FWD(%u): add new entry for own locator.\r\n", id);
			p = ft_add (fp->table, h, &fp->domain->participant.p_guid_prefix, id, 0);
		}
		if (p) {
			if ((locator->locator.flags & (LOCF_META | LOCF_MCAST)) == (LOCF_META | LOCF_MCAST))
				locator_list_copy_node (&p->locs [META_MCAST][0], locator);
			if ((locator->locator.flags & (LOCF_META | LOCF_UCAST)) == (LOCF_META | LOCF_UCAST))
				locator_list_copy_node (&p->locs [META_UCAST][0], locator);
			if ((locator->locator.flags & (LOCF_DATA | LOCF_MCAST)) == (LOCF_DATA | LOCF_MCAST))
				locator_list_copy_node (&p->locs [USER_MCAST][0], locator);
			if ((locator->locator.flags & (LOCF_DATA | LOCF_UCAST)) == (LOCF_DATA | LOCF_UCAST))
				locator_list_copy_node (&p->locs [USER_UCAST][0], locator);
		}
		ft_exit (fp->table);
	}
	fwd_print_end ();
	return (DDS_RETCODE_OK);
}

/* rfwd_locator_remove -- Remove a port from the forwarder based on the given
			  locator and id. */

void rfwd_locator_remove (unsigned id, LocatorNode_t *locator)
{
	FwdData_t	*fp;
	FTEntry_t	*p;
	unsigned	h;

	fwd_print2 ("FWD(%u): Remove locator: %s\r\n", id, locator_str (&locator->locator));
	if (id > MAX_DOMAINS) {
		warn_printf ("FWD: invalid domain index (%u)!", id);
		return;
	}
	fp = fwd [id];
	if (fp) {
		ft_entry (fp->table);
		p = ft_lookup (fp->table, &fp->domain->participant.p_guid_prefix, &h);
		if (p) {
			if ((locator->locator.flags & (LOCF_META | LOCF_MCAST)) == (LOCF_META | LOCF_MCAST))
				locator_list_delete (&p->locs [META_MCAST][0],
						     locator->locator.kind,
						     locator->locator.address,
						     locator->locator.port);
			if ((locator->locator.flags & (LOCF_META | LOCF_UCAST)) == (LOCF_META | LOCF_UCAST))
				locator_list_delete (&p->locs [META_UCAST][0],
						     locator->locator.kind,
						     locator->locator.address,
						     locator->locator.port);
			if ((locator->locator.flags & (LOCF_DATA | LOCF_MCAST)) == (LOCF_DATA | LOCF_MCAST))
				locator_list_delete (&p->locs [USER_MCAST][0],
						     locator->locator.kind,
						     locator->locator.address,
						     locator->locator.port);
			if ((locator->locator.flags & (LOCF_DATA | LOCF_UCAST)) == (LOCF_DATA | LOCF_UCAST))
				locator_list_delete (&p->locs [USER_UCAST][0],
						     locator->locator.kind,
						     locator->locator.address,
						     locator->locator.port);
		}
		ft_exit (fp->table);
	}
	fwd_print_end ();
}

/* rfwd_participant_new -- A new DDS domain participant was discovered. */

void rfwd_participant_new (Participant_t *pp, int update)
{
	FwdData_t	*fp;
	FTEntry_t	*p;
	unsigned	id, h;
#ifdef FORWARD_TRC
	char		buf [40];
#endif
	fwd_print2 ("FWD-ParticipantNew: Prefix: %s, update=%d\r\n",
			guid_prefix_str (&pp->p_guid_prefix, buf), update);
	for (id = 1; id <= MAX_DOMAINS; id++)
		if ((fp = fwd [id]) != NULL && fp->domain == pp->p_domain)
			break;

	if (id > MAX_DOMAINS)
		return;

	ft_entry (fp->table);
	p = ft_lookup (fp->table, &pp->p_guid_prefix, &h);
	if (!p) {
		fwd_print ("FWD-ParticipantNew: add new entry\r\n");
		p = ft_add (fp->table, h, &pp->p_guid_prefix, 0, LTF_AGE);
	}
	if (p)
		fwd_populate_locators (p, pp, update);

	ft_exit (fp->table);
	fwd_print_end ();
}

/* rfwd_participant_dispose -- A DDS domain participant was removed. */

void rfwd_participant_dispose (Participant_t *pp)
{
	FwdData_t	*fp;
	unsigned	id;
#ifdef FORWARD_TRC
	char		buf [40];
#endif
	fwd_print1 ("FWD-ParticipantDispose: Prefix: %s\r\n",
			guid_prefix_str (&pp->p_guid_prefix, buf));
	for (id = 1; id <= MAX_DOMAINS; id++)
		if ((fp = fwd [id]) != NULL && fp->domain == pp->p_domain)
			break;

	if (id <= MAX_DOMAINS) {
		fwd_print ("FWD-ParticipantDispose: remove entry\r\n");
		ft_delete (fp->table, &pp->p_guid_prefix);
	}
	fwd_print_end ();
}

#ifdef DDS_DEBUG

/* rfwd_dump -- Dump forwarding related information. */

void rfwd_dump (void)
{
	FwdData_t	*fp;
	unsigned	id;

	for (id = 1; id <= MAX_DOMAINS; id++)
		if ((fp = fwd [id]) != NULL) {
			dbg_printf ("DomainParticipant %u:\r\n", id);
			dbg_printf ("\tMessage receives      = %lu.\r\n", fp->stats.msgs_rxed);
			dbg_printf ("\t  -> Data (unicast)   = %lu/%lu msgs/octets.\r\n",
					fp->stats.msgs_data_uc, fp->stats.msgs_data_uco);
			dbg_printf ("\t  -> Data (multicast) = %lu/%lu msgs/octets.\r\n",
					fp->stats.msgs_data_mc, fp->stats.msgs_data_mco);
			dbg_printf ("\t  -> no participant   = %lu.\r\n", fp->stats.msgs_no_peer);
			dbg_printf ("\t  -> no endpoint      = %lu.\r\n", fp->stats.msgs_no_ep);
			dbg_printf ("\t  -> add fwd dest     = %lu.\r\n", fp->stats.msgs_add_fwdest);
			dbg_printf ("\t  -> forwarded        = %lu.\r\n", fp->stats.msgs_fwded);
			dbg_printf ("\t  -> local messages   = %lu.\r\n", fp->stats.msgs_local);
			dbg_printf ("\t  -> no destination   = %lu.\r\n", fp->stats.msgs_no_dest);
			dbg_printf ("\t  -> direct loops     = %lu.\r\n", fp->stats.msgs_looped);
			dbg_printf ("\t  -> indirect loops   = %lu.\r\n", fp->stats.msgs_loopedi);
			dbg_printf ("\tMessage sends         = %lu.\r\n", fp->stats.msgs_reqed);
			dbg_printf ("\t  -> handle sent      = %lu.\r\n", fp->stats.msgs_h_sent);
			dbg_printf ("\t  -> sent             = %lu.\r\n", fp->stats.msgs_sent);
			dbg_printf ("\t  -> not sent         = %lu.\r\n", fp->stats.msgs_not_sent);
			dbg_printf ("\r\n");
		}
	dbg_dump_ft (fwd_table);
}

/* rfwd_trace -- Trace frame processing in the forwarder. */

void rfwd_trace (int ntraces)
{
#ifdef FORWARD_TRC
	if (ntraces < 0) {
		fwd_trace = !fwd_trace;
		if (fwd_trace)
			fwd_tcnt = 0;
	}
	else if (!ntraces)
		fwd_trace = 0;
	else {
		fwd_tmax = ntraces;
		fwd_tcnt = 0;
		fwd_trace = 1;
	}
#else
	ARG_NOT_USED (ntraces)
#endif	
}

#endif

#else
int avoid_emtpy_translation_unit_rtps_fwd_c;
#endif
