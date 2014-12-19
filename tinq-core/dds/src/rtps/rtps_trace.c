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

/* rtps_trace.h -- Implements the optional RTPS tracing functionality. */

#ifdef RTPS_TRACE

#ifdef _WIN32
#include "win.h"
#else
#include <arpa/inet.h>
#endif
#include "log.h"
#include "prof.h"
#include "set.h"
#include "error.h"
#include "nmatch.h"
#include "dds.h"
#include "rtps_data.h"
#include "rtps_cfg.h"
#include "rtps_priv.h"
#include "rtps_trace.h"
#include "dds/dds_debug.h"

int rtps_trace = 1;

#define	MAX_HANDLE_TRACES	8
#define	MAX_NAME_TRACES		4

static struct {
	unsigned	handle;
	unsigned	mode;
} rtps_handle_traces [MAX_HANDLE_TRACES];

static struct {
	char		name [64];
	unsigned	mode;
} rtps_name_traces [MAX_NAME_TRACES];

/*unsigned rtps_dtrace = DEF_RTPS_TRACE;*/

void rtps_handle_trace_set (unsigned handle, unsigned mode)
{
	unsigned	i;

	for (i = 0; i < MAX_HANDLE_TRACES; i++) {
		if (rtps_handle_traces [i].handle == handle) {
			if (mode)
				rtps_handle_traces [i].mode = mode;
			else {
				for (++i; i < MAX_HANDLE_TRACES; i++)
					rtps_handle_traces [i - 1] = rtps_handle_traces [i];
				rtps_handle_traces [MAX_HANDLE_TRACES - 1].handle = 0;
			}
			return;
		}
		else if (!rtps_handle_traces [i].handle) {
			if (mode) {
				rtps_handle_traces [i].handle = handle;
				rtps_handle_traces [i].mode = mode;
			}
			return;
		}
	}
	warn_printf ("RTPS: Handle tracing table full!");
}

void rtps_name_trace_set (const char *name, unsigned mode)
{
	unsigned	i;

	for (i = 0; i < MAX_NAME_TRACES; i++)
		if (!rtps_name_traces [i].name [0]) {
			if (mode) {
				strncpy (rtps_name_traces [i].name, name, 64);
				rtps_name_traces [i].mode = mode;
			}
			return;
		}
		else if (!strcmp (rtps_name_traces [i].name, name)) {
			if (mode)
				rtps_name_traces [i].mode = mode;
			else {
				for (++i; i < MAX_NAME_TRACES; i++)
					rtps_name_traces [i - 1] = rtps_name_traces [i];
				rtps_name_traces [MAX_NAME_TRACES - 1].name [0] = '\0';
			}
			return;
		}
	warn_printf ("RTPS: Name tracing table full!");
}

unsigned rtps_def_trace (unsigned handle, const char *name)
{
	unsigned	i;

	for (i = 0; i < MAX_HANDLE_TRACES && rtps_handle_traces [i].handle; i++)
		if (rtps_handle_traces [i].handle == handle)
			return (rtps_handle_traces [i].mode);

	for (i = 0; i < MAX_NAME_TRACES && rtps_name_traces [i].name [0]; i++)
		if (!nmatch (rtps_name_traces [i].name, name, NM_NOESCAPE))
			return (rtps_name_traces [i].mode);

	return (dds_dtrace);
}

void rtps_trace_w_event (WRITER *wp, char event, const char *s)
{
	const char	*names [2];

	endpoint_names (&wp->endpoint, names);
	log_printf (RTPS_ID, 0, "RTPS: %-32s E -    %c: %s: ",
					names [0], event, s);
}


static const char *rtps_rr_state_str (unsigned type, unsigned s)
{
	const char	*sp;

	switch (type) {
		case 0: sp = rtps_rr_cstate_str [s];
			break;
		case 1: sp = rtps_rr_tstate_str [s];
			break;
		case 2: sp = rtps_rr_astate_str [s];
			break;
		default:
			sp = NULL;
	}
	return (sp);
}

void rtps_rr_state_change (RemReader_t *rrp,
			   unsigned    s,
			   unsigned    prev_s,
			   int         init,
			   unsigned    type)
{
	const char	*sp;
	Locator_t	*lp;

	if (init)
		sp = ".";
	else if (s != prev_s)
		sp = rtps_rr_state_str (type, prev_s);
	else
		return;

	if (rrp->rr_writer->endpoint.stateful) {
		if (rrp->rr_uc_locs)
			lp = &rrp->rr_uc_locs->data->locator;
		else if (rrp->rr_mc_locs)
			lp = &rrp->rr_mc_locs->data->locator;
		else
			lp = NULL;
	}
	else
		lp = &rrp->rr_locator->locator;
	rtps_trace_w_event (rrp->rr_writer, 'S', locator_str (lp));
	log_printf (RTPS_ID, 0, "%s -> %s\r\n", sp, rtps_rr_state_str (type, s));
}

void rtps_rr_signal (RemReader_t *rrp, const char *s)
{
	Locator_t	*lp;

	if (rrp->rr_writer->endpoint.stateful) {
		if (rrp->rr_uc_locs)
			lp = &rrp->rr_uc_locs->data->locator;
		else if (rrp->rr_mc_locs)
			lp = &rrp->rr_mc_locs->data->locator;
		else
			lp = NULL;
	}
	else
		lp = &rrp->rr_locator->locator;
	rtps_trace_w_event (rrp->rr_writer, 'C', locator_str (lp));
	log_printf (RTPS_ID, 0, "%s;\r\n", s);
}

void rtps_w_tmr_action (WRITER     *wp,
			const char *type,
			int        start,
			unsigned   ticks,
			const char *sp)
{
	rtps_trace_w_event (wp, 'T', type);
	log_printf (RTPS_ID, 0, "%s Timer", (start) ? "Start" : "Stop");
	if (sp)
		log_printf (RTPS_ID, 0, "_%s", sp);
	if (start)
		log_printf (RTPS_ID, 0, " (%u.%02u secs)", ticks / 100, ticks % 100);
	log_printf (RTPS_ID, 0, "\r\n");
}

void rtps_trace_r_event (READER *rp, char event, const char *s)
{
	const char	*names [2];

	endpoint_names (&rp->endpoint, names);
	log_printf (RTPS_ID, 0, "RTPS: %-32s E -    %c: %s: ",
				names [0], event, s);
}

static const char *rtps_rw_state_str (unsigned type, unsigned s)
{
	const char	*sp;

	switch (type) {
		case 0: sp = rtps_rw_cstate_str [s];
			break;
		case 1: sp = rtps_rw_astate_str [s];
			break;
		default:
			sp = NULL;
	}
	return (sp);
}

void rtps_rw_state_change (RemWriter_t *rwp,
			   unsigned    s,
			   unsigned    prev_s,
			   int         init,
			   unsigned    type)
{
	const char	*sp;
	Locator_t	*lp;

	if (init)
		sp = ".";
	else if (s != prev_s)
		sp = rtps_rw_state_str (type, prev_s);
	else
		return;

	if (rwp->rw_reader->endpoint.stateful) {
		if (rwp->rw_uc_locs)
			lp = &rwp->rw_uc_locs->data->locator;
		else if (rwp->rw_mc_locs)
			lp = &rwp->rw_mc_locs->data->locator;
		else
			lp = NULL;
	}
	else
		lp = NULL;
	rtps_trace_r_event (rwp->rw_reader, 'S', locator_str (lp));
	log_printf (RTPS_ID, 0, "%s -> %s\r\n", sp, rtps_rw_state_str (type, s));
}

void rtps_rw_signal (RemWriter_t *rwp, const char *s)
{
	Locator_t	*lp;

	if (rwp->rw_uc_locs)
		lp = &rwp->rw_uc_locs->data->locator;
	else if (rwp->rw_mc_locs)
		lp = &rwp->rw_mc_locs->data->locator;
	else
		lp = NULL;
	rtps_trace_r_event (rwp->rw_reader, 'C', locator_str (lp));
	log_printf (RTPS_ID, 0, "%s;\r\n", s);
}

void rtps_r_tmr_action (READER          *rp,
			const Locator_t *lp,
			const char      *type,
			int             start,
			unsigned        ticks,
			const char      *sp)
{
	rtps_trace_r_event (rp, 'T', type);
	if (lp)
		log_printf (RTPS_ID, 0, "%s: ", locator_str (lp));
	log_printf (RTPS_ID, 0, "%s Timer", (start) ? "Start" : "Stop");
	if (sp)
		log_printf (RTPS_ID, 0, "_%s", sp);
	if (start)
		log_printf (RTPS_ID, 0, " (%u.%02u secs)", ticks / 100, ticks % 100);
	log_printf (RTPS_ID, 0, "\r\n");
}

static void rtps_info_ts_dump (InfoTimestampSMsg *tp, unsigned flags)
{
	if (tp)
		log_printf (RTPS_ID, 0, "%d.%u", tp->seconds,
						 tp->fraction);
	log_printf (RTPS_ID, 0, ") %c\r\n",
					(flags & SMF_INVALIDATE) ? 'I' : '-');
}

static void rtps_info_dst_dump (InfoDestinationSMsg *dp)
{
	uint32_t	*lp = (uint32_t *) &dp->guid_prefix;

	log_printf (RTPS_ID, 0, "%08x:%08x:%08x)\r\n",
				htonl (lp [0]), htonl (lp [1]), htonl (lp [2]));
}

static void rtps_acknack_dump (AckNackSMsg *ap, unsigned flags)
{
	unsigned	i;

	log_printf (RTPS_ID, 0, "%u.%u", ap->reader_sn_state.base.high,
				     ap->reader_sn_state.base.low);
	if (ap->reader_sn_state.numbits) {
		log_printf (RTPS_ID, 0, "/%u:", ap->reader_sn_state.numbits);
		for (i = 0; i < ap->reader_sn_state.numbits; i++)
			if (SET_CONTAINS (ap->reader_sn_state.bitmap, i))
				log_printf (RTPS_ID, 0, " .%u", i + ap->reader_sn_state.base.low);
	}
	log_printf (RTPS_ID, 0, ") %c\r\n", (flags & SMF_FINAL) ? 'F' : '-');
}

static void rtps_data_dump (DataSMsg *dp, unsigned flags)
{
	log_printf (RTPS_ID, 0, "%u.%u) %c%c%c\r\n",
				dp->writer_sn.high, dp->writer_sn.low,
				(flags & SMF_DATA) ? 'D' : '-',
				(flags & SMF_KEY) ? 'K' : '-',
				(flags & SMF_INLINE_QOS) ? 'Q' : '-');
}

static void rtps_gap_dump (GapSMsg *gp, unsigned flags)
{
	SequenceNumber_t	snr;
	unsigned		i;

	log_printf (RTPS_ID, 0, "%u.%u", gp->gap_start.high, gp->gap_start.low);
	snr = gp->gap_list.base;
	SEQNR_DEC (snr);
	if (SEQNR_GT (snr, gp->gap_start))
		log_printf (RTPS_ID, 0, "..%u.%u", snr.high, snr.low);
	if (gp->gap_list.numbits)
		log_printf (RTPS_ID, 0, "/%u:", gp->gap_list.numbits);
	for (i = 0; i < gp->gap_list.numbits; i++)
		if (SET_CONTAINS (gp->gap_list.bitmap, i))
			log_printf (RTPS_ID, 0, " .%u", i + gp->gap_list.base.low);
	log_printf (RTPS_ID, 0, ") %c%c%c\r\n",
				(flags & SMF_DATA) ? 'D' : '-',
				(flags & SMF_KEY) ? 'K' : '-',
				(flags & SMF_INLINE_QOS) ? 'Q' : '-');
}

static void rtps_heartbeat_dump (HeartbeatSMsg *hp, unsigned flags)
{
	log_printf (RTPS_ID, 0, "%u.%u-%u.%u) %c%c\r\n",
				     hp->first_sn.high, hp->first_sn.low,
				     hp->last_sn.high, hp->last_sn.low,
				     (flags & SMF_FINAL) ? 'F' : '-',
				     (flags & SMF_LIVELINESS) ? 'L' : '-');
}

#ifdef RTPS_FRAGMENTS

static void rtps_hb_frag_dump (HeartbeatFragSMsg *hp)
{
	log_printf (RTPS_ID, 0, "%u.%u,%u)\r\n", 
					hp->writer_sn.high,
					hp->writer_sn.low,
					hp->last_frag);
}

static void rtps_nack_frag_dump (NackFragSMsg *np)
{
	unsigned	i;

	log_printf (RTPS_ID, 0, "%u.%u %u",
				np->writer_sn.high,
				np->writer_sn.low,
				np->frag_nr_state.base);
	if (np->frag_nr_state.numbits) {
		log_printf (RTPS_ID, 0, "/%u:", np->frag_nr_state.numbits);
		for (i = 0; i < np->frag_nr_state.numbits; i++)
			if (SET_CONTAINS (np->frag_nr_state.bitmap, i))
				log_printf (RTPS_ID, 0, " %u", i + np->frag_nr_state.base);
	}
	log_printf (RTPS_ID, 0, ")\r\n");
}

static void rtps_data_frag_dump (DataFragSMsg *dp, unsigned flags)
{
	log_printf (RTPS_ID, 0, "%u.%u %u,%u,%u,%u) %c%c%c\r\n",
				dp->writer_sn.high, dp->writer_sn.low,
				dp->frag_start, dp->num_fragments,
				dp->frag_size, dp->sample_size,
				(flags & SMF_DATA) ? 'D' : '-',
				(flags & SMF_KEY) ? 'K' : '-',
				(flags & SMF_INLINE_QOS) ? 'Q' : '-');
}

#endif

const char	*frame_type_str [] = {
	"?(0)", "PAD", "?(2)", "?(3)", "?(4)", "?(5)",
	"ACKNACK", "HEARTBEAT", "GAP", "INFO_TS", "?(10)", "?(11)",
	"INFO_SRC", "INFO_REPLY_IP4", "INFO_DST", "INFO_REPLY", "?(16)",
	"?(17)", "NACK_FRAG", "HEARTBEAT_FRAG", "?(20)", "DATA", "DATA_FRAG"
};

void rtps_r_frame (READER        *rp,
		   int           rx,
		   unsigned      type,
		   Participant_t *pp,
		   void          *p,
		   unsigned      flags)
{
	const char	*names [2];

	endpoint_names (&rp->endpoint, names);
	log_printf (RTPS_ID, 0, "RTPS: %-32s %c - %04x: %s(",
			names [0], (rx) ? 'R' : 'T',
			(pp) ? ntohl (pp->p_guid_prefix.w [2]) & 0xffff : 0, 
			frame_type_str [type]);
	switch (type) {
		case ST_INFO_TS:
			rtps_info_ts_dump (p, flags);
			break;
		case ST_INFO_DST:
			rtps_info_dst_dump (p);
			break;
		case ST_ACKNACK:
			rtps_acknack_dump (p, flags);
			break;
		case ST_HEARTBEAT:
			rtps_heartbeat_dump (p, flags);
			break;
		case ST_GAP:
			rtps_gap_dump (p, flags);
			break;
		case ST_DATA:
			rtps_data_dump (p, flags);
			break;
#ifdef RTPS_FRAGMENTS
		case ST_HEARTBEAT_FRAG:
			rtps_hb_frag_dump (p);
			break;
		case ST_NACK_FRAG:
			rtps_nack_frag_dump (p);
			break;
		case ST_DATA_FRAG:
			rtps_data_frag_dump (p, flags);
			break;
#endif
		default:
			break;
	}
}

void rtps_w_frame (WRITER        *wp,
		   int           rx,
		   unsigned      type,
		   Participant_t *pp,
		   void          *p,
		   unsigned      flags)
{
	const char	*names [2];

	endpoint_names (&wp->endpoint, names);
	log_printf (RTPS_ID, 0, "RTPS: %-32s %c - %04x: %s(",
			names [0], (rx) ? 'R' : 'T',
			(pp) ? ntohl (pp->p_guid_prefix.w [2]) & 0xffff : 0, 
			frame_type_str [type]);
	switch (type) {
		case ST_INFO_TS:
			rtps_info_ts_dump (p, flags);
			break;
		case ST_INFO_DST:
			rtps_info_dst_dump (p);
			break;
		case ST_ACKNACK:
			rtps_acknack_dump (p, flags);
			break;
		case ST_HEARTBEAT:
			rtps_heartbeat_dump (p, flags);
			break;
		case ST_GAP:
			rtps_gap_dump (p, flags);
			break;
		case ST_DATA:
			rtps_data_dump (p, flags);
			break;
#ifdef RTPS_FRAGMENTS
		case ST_HEARTBEAT_FRAG:
			rtps_hb_frag_dump (p);
			break;
		case ST_NACK_FRAG:
			rtps_nack_frag_dump (p);
			break;
		case ST_DATA_FRAG:
			rtps_data_frag_dump (p, flags);
			break;
#endif
		default:
			break;
	}
}

static int trace_set_fct (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	unsigned	*mode = (unsigned *) arg;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	ep = *epp;
	if (!ep->rtps)
		return (1);

	rtps_trace_set (ep, *mode);
	return (1);
}

/* rtps_trace_set -- Update the tracing mode of either a single endpoint or all
		     endpoints. */

int rtps_trace_set (Endpoint_t *r, unsigned mode)
{
	Domain_t	*dp;
	ENDPOINT	*ep;
	unsigned	index;

	if (r == DDS_TRACE_ALL_ENDPOINTS) {
		index = 0;
		for (;;) {
			dp = domain_next (&index, NULL);
			if (!dp)
				break;
	
			sl_walk (&dp->participant.p_endpoints, trace_set_fct, &mode);
		}
		return (DDS_RETCODE_OK);
	}
	ep = r->rtps;
	if (!ep)
		return (DDS_RETCODE_OK);

	if (mode == DDS_TRACE_MODE_TOGGLE) {
		if (ep->trace_frames || ep->trace_sigs ||
		    ep->trace_tmr || ep->trace_state)
			mode = 0;
		else
			mode = DDS_TRACE_ALL;
	}
	ep->trace_frames = mode & DDS_RTRC_FTRACE;
	ep->trace_sigs   = mode & DDS_RTRC_STRACE;
	ep->trace_tmr    = mode & DDS_RTRC_ETRACE;
	ep->trace_state  = mode & DDS_RTRC_TTRACE;

	return (DDS_RETCODE_OK);
}

/* rtps_trace_get -- Get the tracing mode of an endpoint. */

int rtps_trace_get (Endpoint_t *r, unsigned *mode)
{
	ENDPOINT	*ep;

	ep = r->rtps;
	if (!ep)
		return (DDS_RETCODE_ALREADY_DELETED);

	if (mode) {
		*mode = 0;
		if (ep->trace_frames)
			*mode |= DDS_RTRC_FTRACE;
		if (ep->trace_sigs)
			*mode |= DDS_RTRC_STRACE;
		if (ep->trace_state)
			*mode |= DDS_RTRC_ETRACE;
		if (ep->trace_tmr)
			*mode |= DDS_RTRC_TTRACE;
	}
	return (DDS_RETCODE_OK);
}

# if 0
/* rtps_dtrace_set -- Update the default tracing mode of new endpoints. */

void rtps_dtrace_set (unsigned mode)
{
	if (mode != DDS_TRACE_MODE_TOGGLE)
		rtps_dtrace = mode;
	else if (rtps_dtrace)
		rtps_dtrace = 0;
	else
		rtps_dtrace = DDS_TRACE_ALL;
}

/* rtps_dtrace_get -- Get the default trace mode. */

void rtps_dtrace_get (unsigned *mode)
{
	if (mode)
		*mode = rtps_dtrace;
}

# endif
#else
# if 0

int rtps_trace = 0;

# endif
#endif /* !RTPS_TRACE */

#ifdef RTPS_TRC_SEQNR

void trc_seqnr (RemWriter_t *rwp, const char *s)
{
	log_printf (RTPS_ID, 0, "RW: rw_seqnr_next=%d.%u (%s)\r\n",
			rwp->rw_seqnr_next.high, rwp->rw_seqnr_next.low, s);
}

#endif
