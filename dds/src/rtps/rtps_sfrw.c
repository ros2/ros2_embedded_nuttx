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

/* rtps_sfrw.c -- Implements the RTPS Stateful Reliable Writer. */

#include "log.h"
#include "prof.h"
#include "error.h"
#include "ctrace.h"
#include "set.h"
#include "list.h"
#include "random.h"
#include "dcps.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_msg.h"
#include "rtps_trace.h"
#include "rtps_check.h"
#include "rtps_priv.h"
#include "rtps_clist.h"
#ifdef RTPS_FRAGMENTS
#include "rtps_frag.h"
#endif
#include "rtps_sfrw.h"

#ifdef RTPS_PROXY_INST
static unsigned rtps_rr_insts;	/* Counts Remote Reader proxy instances. */
#endif

#define	BACKOFF_RESET_RL	4	/* If backoff > this: reset reply locators. */

#if defined (RTPS_TRC_READER) || defined (RTPS_TRC_WRITER)

/* sfx_dump_ccref -- Display a proxy node. */

void sfx_dump_ccref (CCREF *rp, int show_state)
{
	const char	*state_str [] = {
		"NEW", "REQ", "UNSENT", "UNDERWAY", "UNACKED",
		"ACKED", "MISSING", "RECVD", "LOST"
	};

	if (!rp) {
		log_printf (RTPS_ID, 0, "-");
		return;
	}
	if (rp->relevant)
		log_printf (RTPS_ID, 0, "D(%u.%u)", rp->u.c.change->c_seqnr.high,
						    rp->u.c.change->c_seqnr.low);
	else  {
		log_printf (RTPS_ID, 0, "G(%u.%u", rp->u.range.first.high,
						   rp->u.range.first.low);
		if (!SEQNR_EQ (rp->u.range.first, rp->u.range.last))
			log_printf (RTPS_ID, 0, "..%u.%u", rp->u.range.last.high,
						           rp->u.range.last.low);
		log_printf (RTPS_ID, 0, ")");
	}
	if (show_state)
		log_printf (RTPS_ID, 0, "=%s ", state_str [rp->state]);
}

#endif

#ifdef RTPS_TRC_WRITER

#define	DUMP_WRITER_STATE(rrp)	sfw_dump_rr_state(rrp)

/* sfw_dump_rr_state -- Dump the state of reader proxy. */

static void sfw_dump_rr_state (RemReader_t *rrp)
{
	CCREF		*rp;

	if (!rrp->rr_writer->endpoint.trace_state)
		return;

	log_printf (RTPS_ID, 0, "RR: Unsent=");
	if (rrp->rr_unsent_changes)
		sfx_dump_ccref (rrp->rr_unsent_changes, 0);
	log_printf (RTPS_ID, 0, ", Unacked=%u\r\n", rrp->rr_unacked);
	if (LIST_NONEMPTY (rrp->rr_changes)) {
		log_printf (RTPS_ID, 0, "RR: Samples:");
		LIST_FOREACH (rrp->rr_changes, rp)
			sfx_dump_ccref (rp, 1);
		log_printf (RTPS_ID, 0, "\r\n");
	}
	else
		log_printf (RTPS_ID, 0, "RR: no samples.\r\n");
}

#else
#define	DUMP_WRITER_STATE(rrp)
#endif

#ifdef RTPS_REL_CHECK
#define	REL_CHECK	sfw_rel_check

static void sfw_rel_verify_ptr (RemReader_t *rrp, CCREF *p, int req)
{
	CCREF	*xp;

	if (!p)
		return;

	LIST_FOREACH (rrp->rr_changes, xp) {
		if (xp == p)
			return;
	}
	fatal_printf ("sfw_rel_verify_ptr: pointer not in list(%u)!", req);
}

static void sfw_rel_check (RemReader_t *rrp)
{
	sfw_rel_verify_ptr (rrp, rrp->rr_unsent_changes, 0);
}

#else
#define	REL_CHECK(rrp)
#endif

/* sfw_heartbeat_to -- Time-out of the Heartbeat timer. */

static void sfw_heartbeat_to (uintptr_t user);

/* sfw_stop_heartbeats -- Stop Heartbeat timer - no more need for it. */

static void sfw_stop_heartbeats (WRITER *wp)
{
	if (!wp->rh_timer)
		return;

	HEARTBEAT_TMR_STOP (wp, wp->rh_timer);
	HEARTBEAT_TMR_FREE (wp);
	tmr_free (wp->rh_timer);
	wp->rh_timer = NULL;
}

/* q_seqnr_info -- Get the sequence number range corresponding with a queue. */

#define q_seqnr_info(list, min, max) if (list.head->relevant)			\
					  min = list.head->u.c.change->c_seqnr; \
				     else min = list.head->u.range.first;	\
				     if (list.tail->relevant)			\
					  max = list.tail->u.c.change->c_seqnr; \
				     else max = list.tail->u.range.last

/* sfw_send_heartbeat -- Send a Heartbeat message. */

int sfw_send_heartbeat (WRITER *wp, int alive)
{
	RemReader_t	 	*rrp, *xrrp = NULL;
	DiscoveredReader_t	*drp = NULL;
	unsigned	 	nreaders, nwaiting;
	int			no_mcast;
	SequenceNumber_t 	snr_min, snr_max, min_mc, max_mc, min, max;
#ifdef RTPS_FRAGMENTS
	CCREF			*rp;
	unsigned		n;
#endif
				
	nreaders = nwaiting = 0;
	no_mcast = 0;
	memset (&min_mc, 0, sizeof (min_mc));
	memset (&max_mc, 0, sizeof (max_mc));
	LIST_FOREACH (wp->rem_readers, rrp) {
		if (alive ||
		    (rrp->rr_reliable &&
		     rrp->rr_tstate == RRTS_ANNOUNCING)) {
			nreaders++;
			drp = (DiscoveredReader_t *) rrp->rr_endpoint;
			if (rrp->rr_no_mcast)
				no_mcast = 1;
			xrrp = rrp;
			if (rrp->rr_changes.nchanges) {
				q_seqnr_info (rrp->rr_changes, min, max);
				/*log_printf (RTPS_ID, 0, "QS: min: %u.%u, max: %u.%u!\r\n", min.high, min.low, max.high, max.low);*/
				if (SEQNR_ZERO (min_mc) || SEQNR_LT (min, min_mc))
					min_mc = min;
				if (SEQNR_ZERO (max_mc) || SEQNR_GT (max, max_mc))
					max_mc = max;
				/*log_printf (RTPS_ID, 0, "MS: min: %u.%u, max: %u.%u!\r\n", min_mc.high, min_mc.low, max_mc.high, max_mc.low);*/
			}
#ifdef RTPS_FRAGMENTS
			if ((n = rrp->rr_nfrags) == 0)
				continue;

			for (rp = LIST_HEAD (rrp->rr_changes);
			     rp;
			     rp = LIST_NEXT (rrp->rr_changes, *rp)) {
				if (!rp->fragments)
					continue;

				rtps_msg_add_heartbeat_frag (rrp,
							     drp,
							     &rp->u.c.change->c_seqnr,
							     rp->fragments->total);
				if (!--n)
					break;
			}
#endif
			
		}
	}
	if (!alive && !nreaders) {
		sfw_stop_heartbeats (wp);
		return (0);
	}

	/* Needs to send a Heartbeat - get sequence numbers. */
	hc_seqnr_info (wp->endpoint.endpoint->cache, &snr_min, &snr_max);
	/*log_printf (RTPS_ID, 0, "C: min: %u.%u, max: %u.%u!\r\n", snr_min.high, snr_min.low, snr_max.high, snr_max.low);*/

	/* If single destination or multicast possible, send single message. */
	if (nreaders == 1 || (!no_mcast && nreaders > 1)) {
		if (nreaders > 1)
			drp = NULL;

		/* Send Heartbeat message on first waiting proxy. */
		if (SEQNR_LT (min_mc, snr_min))
			snr_min = min_mc;
		if (SEQNR_LT (max_mc, snr_max))
			snr_max = max_mc;
		rtps_msg_add_heartbeat (xrrp, drp,
				(alive) ? SMF_FINAL | SMF_LIVELINESS : 0,
				&snr_min, &snr_max);
		
		if (wp->backoff > BACKOFF_RESET_RL && !info_reply_rxed ()) {
			lrloc_print ("RTPS: ExcessiveHB1: ");
			proxy_reset_reply_locators (&xrrp->proxy); /* New path. */
		}
	}
	else	/* Send to each individual destination. */
		LIST_FOREACH (wp->rem_readers, rrp)
			if (alive ||
			    (rrp->rr_reliable &&
			     rrp->rr_tstate == RRTS_ANNOUNCING)) {
				if (!rrp->rr_changes.nchanges)
					continue;

				q_seqnr_info (rrp->rr_changes, min, max);
				/*log_printf (RTPS_ID, 0, "Q: min: %u.%u, max: %u.%u!\r\n", min.high, min.low, max.high, max.low);*/
				rtps_msg_add_heartbeat (rrp,
						(DiscoveredReader_t *) rrp->rr_endpoint,
						(alive) ? SMF_FINAL | SMF_LIVELINESS : 0,
						&min, &max);
				if (wp->backoff > BACKOFF_RESET_RL && !info_reply_rxed ()) {
					lrloc_print ("RTPS: ExcessiveHB2: ");
					proxy_reset_reply_locators (&rrp->proxy);
				}
			}
	return (1);
}

/* sfw_heartbeat_to -- Time-out of the Heartbeat timer. */

static void sfw_heartbeat_to (uintptr_t user)
{
	WRITER		*wp = (WRITER *) user;
	Writer_t	*w = (Writer_t *) (wp->endpoint.endpoint);
	unsigned	n;

	ctrc_printd (RTPS_ID, RTPS_SFW_HB_TO, &user, sizeof (user));
	prof_start (rtps_rw_hb_to);

#ifdef RTPS_MARKERS
	if (wp->endpoint.mark_hb_to)
		rtps_marker_notify (wp->endpoint.endpoint, EM_HEARTBEAT_TO, "sfw_rel_heartbeat_to");
#endif
	HEARTBEAT_TMR_TO (wp);

	/* Send a HEARTBEAT message if required. */
	if (sfw_send_heartbeat (wp, 0)) {

		/* Restart Heartbeat timer with backoff. */
		if (wp->backoff < 7)
			wp->backoff++;

		if (wp->backoff > 1)
			n = 1 << (1 + (fastrandn (wp->backoff)));
		else
			n = 1;

		HEARTBEAT_TMR_START (wp, wp->rh_timer,
				     wp->rh_period * n,
				     (uintptr_t) wp,
				     sfw_heartbeat_to,
				     &w->w_lock);
	}
	prof_stop (rtps_rw_hb_to, 1);
}

/* sfw_announce -- Announce the current cache state. */

static void sfw_announce (RemReader_t *rrp, int urgent)
{
	WRITER		*wp = rrp->rr_writer;
	Writer_t	*w = (Writer_t *) (wp->endpoint.endpoint);

	NEW_RR_TSTATE (rrp, RRTS_ANNOUNCING, 0);

	if (!wp->rh_timer) {

		/* Allocate a heartbeat timer. */
		HEARTBEAT_TMR_ALLOC (wp);
		wp->rh_timer = tmr_alloc ();
		if (!wp->rh_timer) {
			warn_printf ("sfw_announce: no timer available for HEARTBEAT!");
			sfw_send_heartbeat (wp, 0);
			return;
		}
		else
			tmr_init (wp->rh_timer, "RTPS-Heartbeat");

		/* Start Heartbeat timer with a short time (1 tick, e.g. 10ms).
		   This is a trade-off between using piggy-backed acks and just
		   starting a timer for the normal Heartbeat period.  The first,
		   i.e. piggy-backing would lead to too many heartbeats/acks
		   when the send rate is very high (several messages/tick). The
		   latter method buffers data for much too long.
		   The chosen method makes sure that Heartbeats rate will never
		   be higher than the # of ticks/second, but also optimizes the
		   case when bulk transfers occur, since the normal Heartbeat
		   rate will be used during continuous traffic.

		   While the above mechanism is fine for KEEP_LAST history, it
		   falls short however in the case of KEEP_ALL.  In this case,
		   we can safely use the described mechanism while there is
		   still rooom in the cache.  If the amount of remaining buffer
		   capacity in the cache becomes too low, however, it is neces-
		   sary to send HEARTBEATs much faster to prevent a writer from
		   blocking on a full cache.  The latter case is indicated with 
		   the urgent flag in the cache change and this is used to force
		   an immediate heartbeat. */
		HEARTBEAT_TMR_START (wp, wp->rh_timer,
				     (urgent) ? wp->rh_period : 1, 
				     (uintptr_t) wp,
				     sfw_heartbeat_to,
				     &w->w_lock);
	}
	else if (urgent) {
		HEARTBEAT_TMR_STOP (wp, wp->rh_timer);
		HEARTBEAT_TMR_START (wp, wp->rh_timer,
				     wp->rh_period,
				     (uintptr_t) wp,
				     sfw_heartbeat_to,
				     &w->w_lock);
	}
	if (urgent)
		sfw_send_heartbeat (wp, 0);
}

/* sfw_rel_send_alive -- Poll with Heartbeat whether peer can communicate. */

static void sfw_rel_send_alive (RemReader_t *rrp)
{
	WRITER		*wp = rrp->rr_writer;
	Writer_t	*w = (Writer_t *) (wp->endpoint.endpoint);
	SequenceNumber_t snr_min, snr_max;
#ifdef RTPS_HB_WAIT
	unsigned	n;
#endif

	hc_seqnr_info (w->w_cache, &snr_min, &snr_max);
	if (SEQNR_GT (snr_min, snr_max)) {
		snr_max = snr_min;
		rtps_msg_add_heartbeat (rrp,
					(DiscoveredReader_t *) rrp->rr_endpoint, 
					SMF_FINAL | SMF_LIVELINESS,
					&snr_min, &snr_max);
		return;
	}
#ifdef RTPS_HB_WAIT
	rtps_msg_add_heartbeat (rrp,
				(DiscoveredReader_t *) rrp->rr_endpoint, 
				0, &snr_min, &snr_max);
	if (wp->rh_timer) {
		HEARTBEAT_TMR_STOP (wp, wp->rh_timer);
		if (wp->backoff < 7)
			wp->backoff++;
		if (wp->backoff > BACKOFF_RESET_RL && !info_reply_rxed ()) {
			lrloc_print ("RTPS: ExcessiveHBa: ");
			proxy_reset_reply_locators (&rrp->proxy); /* Rechoose path. */
		}

		if (wp->backoff > 1)
			n = 1 << (1 + (fastrandn (wp->backoff)));
		else
			n = 1;

		HEARTBEAT_TMR_START (wp, wp->rh_timer,
				     wp->rh_period * n,
				     (uintptr_t) wp,
				     sfw_heartbeat_to,
				     &w->w_lock);
	}
#else
	if (rrp->rr_unsent_changes) {
		if (wp->endpoint.push_mode) {
			NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
			if (!rrp->rr_active)
				proxy_activate (&rrp->proxy);
		}
		else
			sfw_announce (rrp, 1);
	}
#endif
}

#ifdef RTPS_W_WAIT_ALIVE

/* sfw_rel_alive_to -- Alive timer elapsed. */

static void sfw_rel_alive_to (uintptr_t user)
{
	RemReader_t	*rrp = (RemReader_t *) user;
	Writer_t	*w = (Writer_t *) (rrp->rr_writer->endpoint.endpoint);

	ctrc_printd (RTPS_ID, RTPS_SFW_ALIVE_TO, &user, sizeof (user));
	prof_start (rtps_rw_alive_to);

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_alv_to)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_ALIVE_TO, "sfw_rel_alive_to");
#endif
	WALIVE_TMR_TO (rrp);
	sfw_rel_send_alive (rrp);
	if (rrp->rr_nack_timer)
		WALIVE_TMR_START (rrp, rrp->rr_nack_timer, WALIVE_TO,
				 (uintptr_t) rrp, sfw_rel_alive_to, &w->w_lock);

	rrp->rr_uc_dreply = NULL;	/* Reselect optimal transport. */
	prof_stop (rtps_rw_alive_to, 1);
}

#endif

#if 0 /* Was used i.c.w. RTPS_W_WAIT_ALIVE ... no longer needed? */

/* sfw_rel_zero_ack -- Peer became alive? */

static void sfw_rel_zero_ack (RemReader_t *rrp)
{
	Writer_t	*w = (Writer_t *) (rrp->rr_writer->endpoint.endpoint);

	rrp->rr_peer_alive = 0;
	if (!rrp->rr_nack_timer) {
		WALIVE_TMR_ALLOC (rrp);
		rrp->rr_nack_timer = tmr_alloc ("RTPS-WAlive");
	}
	sfw_rel_send_alive (rrp);
	if (rrp->rr_nack_timer)
		WALIVE_TMR_START(rrp, rrp->rr_nack_timer, WALIVE_TO,
					   (uintptr_t) rrp, sfw_rel_alive_to, &w->w_lock);
}

#endif

/* sfw_rel_start -- Start a Reader proxy in reliable stateful mode. */

static void sfw_rel_start (RemReader_t *rrp)
{
	SequenceNumber_t snr_min, snr_max;
	Writer_t	 *w = (Writer_t *) (rrp->rr_writer->endpoint.endpoint);

	ctrc_printd (RTPS_ID, RTPS_SFW_REL_START, &rrp, sizeof (rrp));
	prof_start (rtps_rw_start);

	RR_SIGNAL (rrp, "REL-Start");

	NEW_RR_CSTATE (rrp, RRCS_INITIAL, 1);
	NEW_RR_CSTATE (rrp, RRCS_READY, 0);
	NEW_RR_TSTATE (rrp, RRTS_IDLE, 1);
	NEW_RR_ASTATE (rrp, RRAS_WAITING, 1);
	rrp->rr_nack_timer = NULL;
	rrp->rr_unacked = 0;

#ifdef RTPS_PROXY_INST
	rrp->rr_loc_inst = ++rtps_rr_insts;
#endif
#ifdef RTPS_W_WAIT_ALIVE
	/* Remote reader is not yet active. */
	rrp->rr_peer_alive = 0;
#else
	rrp->rr_peer_alive = 1;
	RR_SIGNAL (rrp, "=> ALIVE");
#endif
#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_start)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_START, "sfw_rel_start");
#endif

	/* Add existing cache entries to reader locator/proxy queue. */
	hc_seqnr_info (rrp->rr_writer->endpoint.endpoint->cache, &snr_min, &snr_max);
	if (rrp->rr_endpoint->qos->qos.durability_kind) {
		SEQNR_INC (rrp->rr_new_snr);
		hc_replay (rrp->rr_writer->endpoint.endpoint->cache,
					proxy_add_change, (uintptr_t) rrp);
	}
	else {
		rrp->rr_new_snr = snr_max;
		SEQNR_INC (rrp->rr_new_snr);
	}
#ifdef RTPS_INIT_HEARTBEAT
	if (!rrp->rr_unsent_changes) {
		rrp->rr_writer->backoff = 0;
		rtps_msg_add_heartbeat (rrp, (DiscoveredReader_t *) rrp->rr_endpoint, SMF_FINAL, &snr_min, &snr_min);
	}
	else {
#else
	if (rrp->rr_unsent_changes) {
#endif
#ifdef RTPS_W_WAIT_ALIVE

		/* Remote reader is not yet active. */
		/*rtps_msg_add_heartbeat (rrp, (DiscoveredReader_t *) rrp->rr_endpoint, 0, &snr_min, &snr_max);*/
		WALIVE_TMR_ALLOC (rrp);
		rrp->rr_nack_timer = tmr_alloc ();
		if (rrp->rr_nack_timer) {
			tmr_init (rrp->rr_nack_timer, "RTPS-WAlive");
			WALIVE_TMR_START (rrp, rrp->rr_nack_timer, WALIVE_TO,
					   (uintptr_t) rrp, sfw_rel_alive_to, &w->w_lock);
		}
		else
#endif
		if (rrp->rr_writer->endpoint.push_mode) {
			NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
			if (!rrp->rr_active)
				proxy_activate (&rrp->proxy);
		}
		else
			sfw_announce (rrp, 1);
	}
	prof_stop (rtps_rw_start, 1);
	REL_CHECK (rrp);
	RANGE_CHECK (&rrp->rr_changes, "sfw_rel_start");
	DUMP_WRITER_STATE (rrp);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_start");
}

/* sfw_rel_new_change -- A new change should be queued for the remote reader. */

static int sfw_rel_new_change (RemReader_t      *rrp,
			       Change_t         *cp,
			       HCI              hci,
			       SequenceNumber_t *snr)
{
	WRITER		*wp = rrp->rr_writer;
	CCREF		*rp = NULL;
	ChangeState_t	state = (wp->endpoint.push_mode) ? CS_UNSENT : CS_UNACKED;
	SequenceNumber_t gap_first, gap_last;

	ctrc_printd (RTPS_ID, RTPS_SFW_REL_NEW, &rrp, sizeof (rrp));
	prof_start (rtps_rw_new);

	RR_SIGNAL (rrp, "REL-NewChange");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_newch)
		rtps_marker_notify (wp->endpoint.endpoint, EM_NEW_CHANGE, "sfw_rel_new_change");
#endif
	if (LIST_NONEMPTY (rrp->rr_changes))
		if (rrp->rr_changes.tail->relevant) { /* Last sample has data.*/
			gap_first = rrp->rr_changes.tail->u.c.change->c_seqnr;
			SEQNR_INC (gap_first);
			if (!SEQNR_EQ (gap_first, *snr) || !cp) {
				gap_last = *snr;
				if (cp)
					SEQNR_DEC (gap_last);
				rp = change_enqueue_gap (rrp,
							 &gap_first, &gap_last, state);
				if (!rp)
					return (0);
			}
			if (cp)
				rp = change_enqueue (rrp, cp, hci, state);
			RANGE_CHECK (&rrp->rr_changes, "sfw_rel_new_change-0");
		}
		else if (cp) {	/* Last sample was a gap, new contains data. */
			gap_last = *snr;
			SEQNR_DEC (gap_last);
			rrp->rr_changes.tail->u.range.last = gap_last;
			rp = change_enqueue (rrp, cp, hci, state);
			if (!rp)
				return (0);

			RANGE_CHECK (&rrp->rr_changes, "sfw_rel_new_change-1");
		}
		else {	/* Previous && new are both gaps -- combine them. */
			rrp->rr_changes.tail->u.range.last = *snr;
			rrp->rr_changes.tail->state = state;
			RANGE_CHECK (&rrp->rr_changes, "sfw_rel_new_change-2");
		}
	else if (cp) {
		rp = change_enqueue (rrp, cp, hci, state);
		RANGE_CHECK (&rrp->rr_changes, "sfw_rel_new_change-3");
	}
	else {
		rp = change_enqueue_gap (rrp, snr, snr, state);
		RANGE_CHECK (&rrp->rr_changes, "sfw_rel_new_change-4");
	}
	if (!rp)
		return (0);

	if (cp) {
		rp->ack_req = 1;
		rrp->rr_unacked++;
	}
	if (!rrp->rr_unsent_changes)
		rrp->rr_unsent_changes = rp;

	if (rrp->rr_peer_alive && state == CS_UNSENT) {
		NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
		if (!rrp->rr_active)
			proxy_activate (&rrp->proxy);
	}
	else if (rrp->rr_peer_alive)
		sfw_announce (rrp, 0);

	prof_stop (rtps_rw_new, 1);
	DUMP_WRITER_STATE (rrp);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_new_change");
	return (1);
}

#ifdef RTPS_VERIFY_LOOP
static void rtps_writer_proxy_dump (WRITER *wp);
static void sfw_dump_rr_proxy (const char *str, RemReader_t *rrp)
{
	err_printf ("\r\n%s:\r\n", str);
	rtps_writer_proxy_dump (rrp->rr_writer);
	fatal_printf ("\r\nHalting program!");
}
#endif

/* seqnr_delta -- Return the difference between two sequence numbers. */

static unsigned seqnr_delta (SequenceNumber_t *snr1, SequenceNumber_t *snr2)
{
	if (snr2->high - snr1->high > 1)
		return (~0);

	else if (snr2->high - 1 == snr1->high)
		if (snr2->low > snr1->low)
			return (~0);
		else
			return (~0 - snr1->low + snr2->low + 1);
	else
		return (snr2->low - snr1->low);
}

/*static unsigned sfw_rel_send_data_count;*/

/* sfw_rel_send_data -- Send all enqueued changes, ie. not yet sent or requested
			changes to the remote reader. */

static int sfw_rel_send_data (RemReader_t *rrp)
{
	CCREF			*rp, *next_rp;
	GuidPrefix_t		*prefix;
	EntityId_t		*eid;
	unsigned		delta, delta_next, n_urgent = 0;
	int			error, gap_present = 0;
	unsigned		n_gap_bits = 0; /* Prevents compiler warning! */
	SequenceNumber_t 	gap_start, gap_base, snr;
	uint32_t		gap_bits [8];
	DiscoveredReader_t	*drp = NULL;
#ifdef RTPS_VERIFY_LOOP
	unsigned		n_outer, n_inner;
#endif
#ifdef RTPS_FRAGMENTS
	FragInfo_t		*fip;
	unsigned		nfrags;
#endif
	PROF_ITER		(n);

	ctrc_printd (RTPS_ID, RTPS_SFW_REL_SEND, &rrp, sizeof (rrp));
	prof_start (rtps_rw_send);

	RR_SIGNAL (rrp, "REL-SendData");
	/*log_printf (RTPS_ID, 0, "<%u>", sfw_rel_send_data_count);
	if (sfw_rel_send_data_count >= 124)
		log_printf (RTPS_ID, 0, "<Limit Reached>");
	
	sfw_rel_send_data_count++; */
/*	if (rrp->rr_writer->endpoint.trace_state)
		log_printf (RTPS_ID, 0, "First SequenceNumber=(%ld,%lu)!\r\n", seqnr.high, seqnr.low);*/
/*	while (LIST_NONEMPTY (rrp->rr_changes)) { */
#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_send)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_SEND, "sfw_rel_send_data");
#endif
#ifdef RTPS_VERIFY_LOOP
	n_outer = 0;
#endif
	gap_start.high = gap_start.low = 0;
	gap_base.high = gap_base.low = 0;
	rrp->rr_requested_changes = NULL;
	for (; rrp->rr_changes.nchanges; ) {
		PROF_INC (n);

#ifdef RTPS_VERIFY_LOOP
		if (++n_outer > 10000) {
			sfw_dump_rr_proxy ("# of outer loop iterations exceeded limit!", rrp);
			break;
		}
#endif
		if (rrp->rr_tstate == RRTS_PUSHING) {
			rp = rrp->rr_unsent_changes;
			if (!rp) {
				if (gap_present) {
					rtps_msg_add_gap (rrp,
						  (DiscoveredReader_t *) rrp->rr_endpoint,
						  &gap_start, 
						  &gap_base, n_gap_bits, gap_bits,
						  rrp->rr_writer->rh_timer != NULL);
					gap_present = 0;
				}
				sfw_announce (rrp, n_urgent);
				break;
			}
		}
		else if (rrp->rr_astate == RRAS_REPAIRING) {
			if (!rrp->rr_requested_changes)
				for (rp = LIST_HEAD (rrp->rr_changes);
				     rp && rp->state != CS_REQUESTED;
				     rp = LIST_NEXT (rrp->rr_changes, *rp))
					;
			else
				rp = rrp->rr_requested_changes;
			if (!rp) {
				NEW_RR_ASTATE (rrp, RRAS_WAITING, 0);
				break;
			}
		}
		else 
			break;

#ifdef RTPS_VERIFY_LOOP
		n_inner = 0;
#endif
		for (next_rp = LIST_NEXT (rrp->rr_changes, *rp), delta_next = 1;
		     next_rp;
		     next_rp = LIST_NEXT (rrp->rr_changes, *next_rp), delta_next++) {
#ifdef RTPS_VERIFY_LOOP
			if (++n_inner > 10000) {
				sfw_dump_rr_proxy ("# of inner loop iterations exceeded limit!", rrp);
				break;
			}
#endif
			if (rrp->rr_tstate == RRTS_PUSHING)	/* Next one is always ok. */
				break;

			else if (rrp->rr_astate == RRAS_REPAIRING) {
				if (next_rp->state == CS_REQUESTED)
					break;
			}
			else {	/* Unexpected situation! */
				log_printf (RTPS_ID, 0, "sfw_rel_send_data: unexpected state!\r\n");
				break;
			}
		}
		error = DDS_RETCODE_OK;
		if (rp->relevant) { /* Add DATA submessage header. */
			if (rp->u.c.change->c_no_mcast)
				drp = (DiscoveredReader_t *) rrp->rr_endpoint;
			else if (rp->u.c.change->c_wack > 1
#ifdef RTPS_SEDP_MCAST
				 || (rrp->rr_endpoint->fh & EF_BUILTIN) != 0
#endif
				)
				drp = NULL;
			else if (rp->u.c.change->c_wack == 1)
				drp = (DiscoveredReader_t *) rrp->rr_endpoint;

			if (rp->u.c.change->c_wack) {
#ifdef RTPS_FRAGMENTS
				nfrags = rtps_frag_burst;
#endif
				if (drp && drp->dr_participant) {
					prefix = &drp->dr_participant->p_guid_prefix;
					eid = &drp->dr_entity_id;
				}
				else {
					prefix = NULL;
					eid = NULL;
				}
				error = rtps_msg_add_data (rrp,
							   prefix,
							   eid,
							   rp->u.c.change,
							   rp->u.c.hci,
							   !next_rp &&
							   !gap_present &&
							   rrp->rr_writer->rh_timer
#ifdef RTPS_FRAGMENTS
							 , 0, &nfrags
#endif
							   );
#ifdef RTPS_FRAGMENTS
				if (nfrags && !rp->fragments) {
					fip = xmalloc (FRAG_INFO_SIZE (nfrags));
					if (!fip)
						warn_printf ("sfw_rel_send_data: out of memory for fragment info!\r\n");
					else {
						memset (fip, 0, FRAG_INFO_SIZE (nfrags));
						fip->nrefs = 1;
						fip->total = nfrags;
						fip->num_na = nfrags;
						fip->fsize = rtps_frag_size;
						fip->writer = 1;
						rp->fragments = fip;
						rrp->rr_nfrags++;
					}
				}
#endif
				/* Indicates sample was sent. */
				if (rp->u.c.change->c_no_mcast)
					rp->u.c.change->c_wack--;
				else
					rp->u.c.change->c_wack = 0;
				if (rp->u.c.change->c_urgent)
					n_urgent++;
			}
		}

		/* Add to GAP info. */
		else if (rp->state != CS_UNDERWAY) {
			if (gap_present) {
				if (seqnr_delta (&gap_base, &rp->u.range.last) <= 255) {
					snr = rp->u.range.first;
					delta = seqnr_delta (&gap_base, &snr);
					for (;;) {
						SET_ADD (gap_bits, delta);
						if (SEQNR_EQ (snr, rp->u.range.last))
							break;

						SEQNR_INC (snr);
						delta++;
					}
					n_gap_bits = delta + 1;
				}
				else {
					/* Send existing gap. */
					error = rtps_msg_add_gap (rrp,
						  (DiscoveredReader_t *) rrp->rr_endpoint,
						  &gap_start, 
						  &gap_base, n_gap_bits, gap_bits,
						  rrp->rr_writer->rh_timer != NULL);

					/* Start with new gap. */
					n_gap_bits = 0;
					gap_start = rp->u.range.first;
					gap_base = rp->u.range.last;
					SEQNR_INC (gap_base);
					memset (gap_bits, 0, sizeof (gap_bits));
				}
			}
			else {
				gap_present = 1;
				n_gap_bits = 0;
				gap_start = rp->u.range.first;
				gap_base = rp->u.range.last;
				SEQNR_INC (gap_base);
				memset (gap_bits, 0, sizeof (gap_bits));
			}
		}

		rp->state = CS_UNDERWAY;
		if (error) {
			DUMP_WRITER_STATE (rrp);
			return (1);
		}
		if (rrp->rr_tstate == RRTS_PUSHING) {

			/* Remove from unsent list. */
			rrp->rr_unsent_changes = next_rp;
			if (!rrp->rr_unsent_changes && rrp->rr_active) {
				if (gap_present) {
					rtps_msg_add_gap (rrp,
						  (DiscoveredReader_t *) rrp->rr_endpoint,
						  &gap_start, 
						  &gap_base, n_gap_bits, gap_bits,
						  rrp->rr_writer->rh_timer != NULL);
					gap_present = 0;
				}
				sfw_announce (rrp, n_urgent);
				break;
			}
		}
		else if (rrp->rr_astate == RRAS_REPAIRING) {

			/* Remove from requested list. */
			rrp->rr_requested_changes = next_rp;
			if (!rrp->rr_requested_changes && rrp->rr_active) {
				if (gap_present) {
					rtps_msg_add_gap (rrp,
						  (DiscoveredReader_t *) rrp->rr_endpoint,
						  &gap_start, 
						  &gap_base, n_gap_bits, gap_bits,
						  rrp->rr_writer->rh_timer != NULL);
					gap_present = 0;
				}
				NEW_RR_ASTATE (rrp, RRAS_WAITING, 0);
				sfw_announce (rrp, n_urgent);
				break;
			}
		}
		REL_CHECK (rrp);
	}
	if (gap_present)
		rtps_msg_add_gap (rrp,
				  (DiscoveredReader_t *) rrp->rr_endpoint,
				  &gap_start, 
				  &gap_base, n_gap_bits, gap_bits,
				  1);

	prof_stop (rtps_rw_send, n);
	DUMP_WRITER_STATE (rrp);
	RANGE_CHECK (&rrp->rr_changes, "sfw_rel_send_data");
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_send_data");
	return (0);
}

/* seqnr_diff -- Returns the difference between a low and a high sequence
		 number. */

static unsigned seqnr_diff (SequenceNumber_t *sl, SequenceNumber_t *sh, int *err)
{
	SequenceNumber_t	ofs;

	ofs.low = sh->low - sl->low;
	ofs.high = sh->high - sl->high;
	if (sh->low < sl->low) {
		if (!ofs.high) {
			*err = 1;
			return (0);
		}
		else
			ofs.high--;
	}
	if (ofs.high) {
		*err = 1;
		return (0);
	}
	*err = 0;
	return (ofs.low);
}

/* seqnr_inset -- Returns true if the sequencenumber is located in the set. */

static int seqnr_inset (SequenceNumber_t *snr, SequenceNumberSet *set)
{
	unsigned	i;
	int		error;

	i = seqnr_diff (&set->base, snr, &error);
	if (error)
		return (0);

	if (i >= set->numbits)
		return (0);

	return (SET_CONTAINS (set->bitmap, i));
}

#define	DURATION_NULL(d)	(((d).secs | (d).nanos) == 0)

static void sfw_rel_nack_resp_to (RemReader_t *rrp)
{
	NEW_RR_ASTATE (rrp, RRAS_REPAIRING, 0);
	if (rrp->rr_requested_changes)
		proxy_activate (&rrp->proxy);
}

static void nack_resp_tmr_to (uintptr_t user)
{
	RemReader_t	*rrp = (RemReader_t *) user;

	ctrc_printd (RTPS_ID, RTPS_SFW_NACK_RSP_TO, &user, sizeof (user));
	prof_start (rtps_rw_nresp_to);

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_nrsp_to)
		rtps_marker_notify (&w->w_lep, EM_NACKRSP_TO, "sfw_rel_nack_resp_tmr_to");
#endif
	NACKRSP_TMR_TO (rrp);
	NACKRSP_TMR_FREE (rrp);
	tmr_free (rrp->rr_nack_timer);
	rrp->rr_nack_timer = NULL;

	sfw_rel_nack_resp_to (rrp);
	prof_stop (rtps_rw_nresp_to, 1);
}

static void sfw_rel_all_acked (RemReader_t *rrp)
{
	RemReader_t	*xrrp;

	NEW_RR_TSTATE (rrp, RRTS_IDLE, 0);
	if (rrp->rr_nack_timer) {
		NACKRSP_TMR_STOP (rrp, rrp->rr_nack_timer);
		NACKRSP_TMR_FREE (rrp);
		tmr_free (rrp->rr_nack_timer);
		rrp->rr_nack_timer = NULL;
	}

	/* Waiting state for acknowledgements. */
	NEW_RR_ASTATE (rrp, RRAS_WAITING, 0);

	/* Check if Heartbeat still needed. */
	LIST_FOREACH (rrp->rr_writer->rem_readers, xrrp)
		if (xrrp->rr_reliable &&
		    LIST_NONEMPTY (xrrp->rr_changes))
			return;

	/* No more clients with pending acknowledgements. */
	sfw_stop_heartbeats (rrp->rr_writer);
}

/* sfw_range_split -- Split a range in two parts. */

static CCREF *sfw_range_split (CCLIST           *list,
			       CCREF            *rp,
			       SequenceNumber_t *snr,
			       int              acked)
{
	CCREF	*nrp;

	if (SEQNR_GT (*snr, rp->u.range.last)) {
		if (acked)
			rp->state = CS_ACKED;
		return (rp);
	}
		
	/* Split range in two parts: unacked or acked part and a next one. */
	nrp = ccref_insert_gap (list, rp, snr, &rp->u.range.last, rp->state);
	if (!nrp)
		return (NULL);

	if (acked)
		rp->state = CS_ACKED;
	rp->u.range.last = *snr;
	SEQNR_DEC (rp->u.range.last);
	RANGE_CHECK (list, "sfw_range_split");
	return (nrp);
}

#ifdef RTPS_PROXY_INST

/* sfw_rel_reset -- A remote reader was restarted. */

static void sfw_rel_reset (RemReader_t *rrp)
{
	/*SequenceNumber_t snr_min, snr_max;*/
	Writer_t	 *w = (Writer_t *) (rrp->rr_writer->endpoint.endpoint);
	CCREF		 *rp;

	/* Reset states. */
	NEW_RR_CSTATE (rrp, RRCS_INITIAL, 1);
	NEW_RR_CSTATE (rrp, RRCS_READY, 0);
	NEW_RR_TSTATE (rrp, RRTS_IDLE, 1);
	NEW_RR_ASTATE (rrp, RRAS_WAITING, 1);

	rrp->rr_unacked = 0;
	memset (&rrp->rr_new_snr, 0, sizeof (SequenceNumber_t));

#ifdef RTPS_W_WAIT_ALIVE

	/* Remote reader is not yet active. */
	rrp->rr_peer_alive = 0;
#endif
	rrp->rr_unsent_changes = rrp->rr_requested_changes = NULL;

	/* Mark all unacked cache entries as unsent. */
	LIST_FOREACH (rrp->rr_changes, rp)
		if (rp->state == CS_REQUESTED ||
		    rp->state == CS_UNDERWAY ||
		    rp->state == CS_UNACKED) {
			rp->state = CS_UNSENT;
			if (!rrp->rr_unsent_changes)
				rrp->rr_unsent_changes = rp;
			rrp->rr_unacked++;
		}

	/* Add existing cache entries to reader locator/proxy queue. */
	
#ifdef RTPS_INIT_HEARTBEAT
	if (!rrp->rr_unsent_changes) {
		rrp->rr_writer->backoff = 0;
		rtps_msg_add_heartbeat (rrp, (DiscoveredReader_t *) rrp->rr_endpoint, SMF_FINAL, &snr_min, &snr_max);
	}
#endif
#ifdef RTPS_W_WAIT_ALIVE
	if (rrp->rr_unsent_changes) {

		/* Remote reader is not yet active. */

		/* Don't send Heartbeat yet .. leads to duplicate acks.
		 * rtps_msg_add_heartbeat (rrp, (DiscoveredReader_t *) rrp->rr_endpoint, 0, &snr_min, &snr_max);
		 */
		WALIVE_TMR_ALLOC (rrp);
		rrp->rr_nack_timer = tmr_alloc ();
		if (rrp->rr_nack_timer) {
			tmr_init (rrp->rr_nack_timer, "RTPS-WAlive");
			WALIVE_TMR_START (rrp, rrp->rr_nack_timer, WALIVE_TO,
					   (uintptr_t) rrp, sfw_rel_alive_to, &w->w_lock);
		}
		else
#endif
		     if (rrp->rr_unsent_changes) {
			if (rrp->rr_writer->endpoint.push_mode) {
				NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
				if (!rrp->rr_active)
					proxy_activate (&rrp->proxy);
			}
			else
				sfw_announce (rrp, 1);
		}
#ifdef RTPS_W_WAIT_ALIVE
	}
#endif
}

/* sfw_restart -- Restart a stateful reliable remote reader proxy . */

void sfw_restart (RemReader_t *rrp)
{
	rrp->rr_loc_inst = ++rtps_rr_insts;
	sfw_rel_reset (rrp);
}

#endif

/* sfw_rel_acknack -- An ACKNACK was received from a remote reader. */

static void sfw_rel_acknack (RemReader_t *rrp, AckNackSMsg *ap, int final)
{
	CCREF		*rp, *nrp;
	unsigned	t, i, n_gap_bits, n_words;
	int		requested, ack_range, error;
	SequenceNumber_t snr, max, snr_min, snr_max;
	Writer_t	*w = (Writer_t *) (rrp->rr_writer->endpoint.endpoint);
	uint32_t	gap_bits [8];
#ifdef RTPS_PROXY_INST_TX
	Count_t		rem_proxy_inst;
#endif
	PROF_ITER	(n);

	ctrc_printd (RTPS_ID, RTPS_SFW_REL_ACKNACK, &rrp, sizeof (rrp));
	prof_start (rtps_rw_acknack);
	STATS_INC (rrp->rr_nacknack);

	RR_SIGNAL (rrp, "REL-AckNack");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_acknack)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_NACKRSP_TO, "sfw_rel_nack_resp_tmr_to");
#endif
	/* Reset backoff. */
	rrp->rr_writer->backoff = 0;

	/* Get count value. */
	n_words = (ap->reader_sn_state.numbits + 31) >> 5;
	i = ap->reader_sn_state.bitmap [n_words];
#ifdef RTPS_PROXY_INST_TX
	rem_proxy_inst = ap->reader_sn_state.bitmap [n_words + 1];
	if (rem_proxy_inst) {
		if (rrp->rr_rem_inst && rrp->rr_rem_inst != rem_proxy_inst) {

			/* Remote reader has restarted! Reset the proxy. */
			RR_SIGNAL (rrp, "REL-Peer-restart!");
			sfw_rel_reset (rrp);
		}
		rrp->rr_rem_inst = rem_proxy_inst;
	}
/*#if defined (RTPS_PROXY_INST) && !defined (RTPS_PROXY_INST_TX)
#endif*/
#endif

#ifdef RTPS_W_WAIT_ALIVE
	if (!rrp->rr_peer_alive) { /* Valid ack, process it normally. */

		/* Peer is alive. */
		rrp->rr_peer_alive = 1;
		RR_SIGNAL (rrp, "=> ALIVE");

		if (rrp->rr_nack_timer) {
			WALIVE_TMR_STOP (rrp, rrp->rr_nack_timer);
			WALIVE_TMR_FREE (rrp);
			tmr_free (rrp->rr_nack_timer);
			rrp->rr_nack_timer = NULL;
		}
		if (rrp->rr_changes.nchanges && 
		    ((SEQNR_ZERO (ap->reader_sn_state.base) && 
		      !ap->reader_sn_state.numbits) ||
		     rrp->rr_unsent_changes)) {
			NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
			if (!rrp->rr_active)
				proxy_activate (&rrp->proxy);
			rrp->rr_last_ack = i;
			return;
		}
	}
#endif

	/* Ignore duplicate acks. */
	if (i == rrp->rr_last_ack) {
		RR_SIGNAL (rrp, "Ignore (count == rr_last_ack)");
		return;
	}

	/* If peer indicates it became alive, reply with a Heartbeat message. */
	rrp->rr_last_ack = i;
	if (SEQNR_ZERO (ap->reader_sn_state.base) && 
	    !ap->reader_sn_state.numbits) {
		if (!final && rrp->rr_tstate == RRTS_IDLE)
			sfw_rel_send_alive (rrp);
		return;
	}

	/* Get the range of valid sequence numbers. */
	hc_seqnr_info (rrp->rr_writer->endpoint.endpoint->cache, &snr_min, &snr_max);
	max = ap->reader_sn_state.base;

	/* Requested samples that are located between snr_min and the samples
	   list invoke an immediate GAP, since they must be cleaned up gaps or
	   already acknowledged samples.  If we don't send this GAP, the reader
	   will be trapped in a blocked state forever. */
	if (ap->reader_sn_state.numbits) {
		if (LIST_EMPTY (rrp->rr_changes)) {
			snr = rrp->rr_new_snr;
			n_gap_bits = ap->reader_sn_state.numbits;
		}
		else if (rrp->rr_changes.head->relevant &&
		         SEQNR_LT (max, rrp->rr_changes.head->u.c.change->c_seqnr)) {
			n_gap_bits = SEQNR_DELTA (max, rrp->rr_changes.head->u.c.change->c_seqnr);
			if (n_gap_bits > ap->reader_sn_state.numbits)
				n_gap_bits = ap->reader_sn_state.numbits;
		}
		else if (!rrp->rr_changes.head->relevant &&
		         SEQNR_LT (max, rrp->rr_changes.head->u.range.first)) {
			n_gap_bits = SEQNR_DELTA (max, rrp->rr_changes.head->u.range.first);
			if (n_gap_bits > ap->reader_sn_state.numbits)
				n_gap_bits = ap->reader_sn_state.numbits;
		}
		else
			n_gap_bits = 0;

		if (n_gap_bits) {
			RR_SIGNAL (rrp, "Immediate GAP");
			n_words = (n_gap_bits + 31) >> 5;
			memcpy (gap_bits, ap->reader_sn_state.bitmap, n_words << 2);
			rtps_msg_add_gap (rrp,
					  (DiscoveredReader_t *) rrp->rr_endpoint,
					  &max, &max, n_gap_bits, gap_bits, 1);
		}
	}
	else
		n_gap_bits = 0;

	/* Set min. sequence number for implicitly requested samples. */
	SEQNR_ADD (max, ap->reader_sn_state.numbits);
	if (LIST_EMPTY (rrp->rr_changes))
		snr = rrp->rr_new_snr;
	else if (rrp->rr_changes.tail->relevant) {
		snr = rrp->rr_changes.tail->u.c.change->c_seqnr;
		SEQNR_INC (snr); 
	}
	else {
		snr = rrp->rr_changes.tail->u.range.last;
		SEQNR_INC (snr);
	}

	/* Requested samples that are located after the samples list get an
	   implicit gap added to the changes list. */
	if (!SEQNR_LT (max, snr) && SEQNR_GT (snr_max, snr)) {
		if (rrp->rr_changes.nchanges &&
		    !rrp->rr_changes.tail->relevant &&
		    rrp->rr_changes.tail->state == CS_UNSENT)
			rrp->rr_changes.tail->u.range.last = snr_max;
		else {
			rp = change_enqueue_gap (rrp, &snr, &snr_max, CS_UNSENT);
			if (!rrp->rr_unsent_changes)
				rrp->rr_unsent_changes = rp;
		}
	}

	/* Update state of each change in the change list. */
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_acknack-1");
	rrp->rr_requested_changes = NULL;	/* Reset requested changes. */
	LIST_FOREACH (rrp->rr_changes, rp) {
		PROF_INC (n);

		if (rp->state == CS_ACKED)
			continue;

		if (rp->relevant) {
			if (SEQNR_LT (rp->u.c.change->c_seqnr,
				      ap->reader_sn_state.base))
				requested = 0;
			else if (SEQNR_LT (rp->u.c.change->c_seqnr, max)) {
				requested = seqnr_inset (&rp->u.c.change->c_seqnr,
							 &ap->reader_sn_state);
				if (requested)
					rp->u.c.change->c_wack++;
			}
			else
				requested = -1;
		}
		else {
			if (SEQNR_LT (rp->u.range.last,
				      ap->reader_sn_state.base))
				requested = 0;
			else if (SEQNR_LT (rp->u.range.first, max)) {

				/* Range overlaps acknowledgements. */
				snr = ap->reader_sn_state.base;
				if (SEQNR_LT (rp->u.range.first, snr)) {
					i = 0;
					ack_range = 1;
				}
				else {
					i = seqnr_diff (&snr, &rp->u.range.first, &error);
					if (SET_CONTAINS (ap->reader_sn_state.bitmap, i))
						ack_range = 0;
					else
						ack_range = 1;
					SEQNR_ADD (snr, i);
				}
				RANGE_CHECK (&rrp->rr_changes, "-0");
				for (; i < ap->reader_sn_state.numbits; i++) {
					if (SET_CONTAINS (
						     ap->reader_sn_state.bitmap,
						     i)) {
						if (ack_range) {
							ack_range = 0;
							nrp = sfw_range_split (
							       &rrp->rr_changes,
							       rp, &snr, 1);
							if (!nrp)
								break;

							rp = nrp;
							RANGE_CHECK (&rrp->rr_changes, "-a");
						}
					}
					else if (!ack_range) {
						ack_range = 1;
						nrp = sfw_range_split (
							      &rrp->rr_changes,
							      rp, &snr, 0);
						if (!nrp)
							break;

						rp->state = CS_REQUESTED;
						if (!rrp->rr_requested_changes)
							rrp->rr_requested_changes = rp;
						rp = nrp;
						RANGE_CHECK (&rrp->rr_changes, "-b");
					}
					SEQNR_INC (snr);
					if (SEQNR_LT (rp->u.range.last, snr))
						break;
				}
				if (ack_range) {
					if (SEQNR_LT (rp->u.range.last, snr))
						requested = 0;
					else {
						nrp = sfw_range_split (&rrp->rr_changes,
								       rp, &snr, 1);
						if (!nrp)
							break;

						rp = nrp;
						RANGE_CHECK (&rrp->rr_changes, "-c");
						requested = -1;
					}
				}
				else
					requested = 1;
			}
			else
				requested = -1;
		}
		if (requested == 1) {
			rp->state = CS_REQUESTED;
			if (!rrp->rr_requested_changes)
				rrp->rr_requested_changes = rp;
		}
		else if (!requested && rp->state != CS_ACKED) {
			rp->state = CS_ACKED;
			if (rp->relevant) {
				rrp->rr_unacked--;
#ifdef RTPS_FRAGMENTS
				if (rp->fragments) {
					xfree (rp->fragments);
					rp->fragments = NULL;
					rrp->rr_nfrags--;
				}
#endif
			}
		}
		RANGE_CHECK (&rrp->rr_changes, "-1");
	}
	REL_CHECK (rrp);

	/* If some samples were requested, must repair. */
	if (rrp->rr_requested_changes && rrp->rr_astate != RRAS_MUST_REPAIR) {
		NEW_RR_ASTATE (rrp, RRAS_MUST_REPAIR, 0);
		t = rrp->rr_writer->nack_resp_delay;
		if (t) {
			if (!rrp->rr_nack_timer) {
				NACKRSP_TMR_ALLOC (rrp);
				rrp->rr_nack_timer = tmr_alloc ();
				if (!rrp->rr_nack_timer)
					sfw_rel_nack_resp_to (rrp);
				else
					tmr_init (rrp->rr_nack_timer, "RTPS-NackRsp");
			}
			if (rrp->rr_nack_timer)
				NACKRSP_TMR_START (rrp,
						   rrp->rr_nack_timer,
						   t,
						   (uintptr_t) rrp,
						   nack_resp_tmr_to,
						   &w->w_lock);
		}
		else
			sfw_rel_nack_resp_to (rrp);
	}

	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_acknack-2");
	REL_CHECK (rrp);
	DUMP_WRITER_STATE (rrp);

	/* If changes are acknowledged, remove them from the change list. */
	while ((rp = LIST_HEAD (rrp->rr_changes)) != NULL && rp->state == CS_ACKED) {

		/* Remove change from changes list. */
		if (rp->relevant && rp->ack_req)
			hc_acknowledged (rrp->rr_writer->endpoint.endpoint->cache,
					 rp->u.c.hci, &rp->u.c.change->c_seqnr);
		change_remove_ref (rrp, rp);
		CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_acknack-x");
		REL_CHECK (rrp);
	}

	/* If all acked stop sending Heartbeats. */
	if (LIST_EMPTY (rrp->rr_changes) && !n_gap_bits)
		sfw_rel_all_acked (rrp);

	prof_stop (rtps_rw_acknack, n);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_acknack-done");
	DUMP_WRITER_STATE (rrp);
}

#ifdef RTPS_FRAGMENTS

static void sfw_rel_nackfrag (RemReader_t *rrp, NackFragSMsg *np)
{
	CCREF			*rp;
	FragInfo_t		*fip;
	GuidPrefix_t		*prefix;
	EntityId_t		*eid;
	SequenceNumber_t	snr;
	unsigned		i, nf, start, n;
	int			error;

	ctrc_printd (RTPS_ID, RTPS_SFW_REL_NACKFRAG, &rrp, sizeof (rrp));
	STATS_INC (rrp->rr_nnackfrags);

	RR_SIGNAL (rrp, "REL_NackFrag");

	/* Get count value. */
	i = (np->frag_nr_state.numbits + 31) >> 5;
	i = np->frag_nr_state.bitmap [i];

	/* Ignore duplicate NackFrags. */
	if (i == rrp->rr_last_nackfrag) {
		RR_SIGNAL (rrp, "Ignore (count == rr_last_nackfrag)");
		return;
	}
	rrp->rr_last_nackfrag = i;

	/* Find sample with given sequence number. */
	if (!rrp->rr_changes.nchanges || !rrp->rr_nfrags) {
		RR_SIGNAL (rrp, "Ignore (no changes)");
		return;
	}
	if (rrp->rr_changes.head->relevant)
		snr = rrp->rr_changes.head->u.c.change->c_seqnr;
	else
		snr = rrp->rr_changes.head->u.range.last;
	if (SEQNR_LT (np->writer_sn, snr)) {
		RR_SIGNAL (rrp, "Ignore (too old)");
		return;
	}
	if (rrp->rr_changes.tail->relevant)
		snr = rrp->rr_changes.tail->u.c.change->c_seqnr;
	else
		snr = rrp->rr_changes.tail->u.range.first;
	if (SEQNR_GT (np->writer_sn, snr)) {
		RR_SIGNAL (rrp, "Ignore (not in list)");
		return;
	}
	LIST_FOREACH (rrp->rr_changes, rp) {
		if (rp->relevant) {
			if (SEQNR_EQ (rp->u.c.change->c_seqnr, np->writer_sn))
				break;
		}
		else if (!SEQNR_GT (np->writer_sn, rp->u.range.last)) {
			RR_SIGNAL (rrp, "Ignore (not relevant)");
			return;
		}
	}
	if (!rp || (fip = rp->fragments) == NULL) {
		RR_SIGNAL (rrp, "Ignore (no fragment context)");
		return;
	}

	/* Sample found.  Send all requested fragments. */
	nf = start = 0;
	if (rrp->rr_endpoint && rrp->rr_endpoint->u.participant) {
		prefix = &rrp->rr_endpoint->u.participant->p_guid_prefix;
		eid = &rrp->rr_endpoint->entity_id;
	}
	else {
		prefix = NULL;
		eid = NULL;
	}
	for (i = np->frag_nr_state.base, n = 0;
	     i <= fip->total && n < np->frag_nr_state.numbits;
	     i++, n++) {
		if (SET_CONTAINS (np->frag_nr_state.bitmap, n)) {
			if (!nf)
				start = i;
			nf++;
		}
		else if (nf) {

			/* Send all fragments in [start..(start+nf)] */
			/*log_printf (RTPS_ID, 0, "==> Sending fragments: %u*%u!\r\n", start, nf);*/
			error = rtps_msg_add_data (rrp,
						   prefix,
						   eid,
						   rp->u.c.change,
						   rp->u.c.hci,
						   1,
						   start - 1,
						   &nf);
			nf = 0;
			if (error)
				break;
		}
	}
	if (nf) {
		/* Send all fragments in [start..(start+nf)] */
		/*log_printf (RTPS_ID, 0, "==> Sending fragments: %u*%u!\r\n", start, nf);*/
		rtps_msg_add_data (rrp,
				   prefix,
				   eid,
				   rp->u.c.change,
				   rp->u.c.hci,
				   1,
				   start - 1,
				   &nf);
	}
	RR_SIGNAL (rrp, "Requested fragments sent");
}

#endif

#define OPT_RR_LIST	/* Define this to optimize the transmit list. */

/* Note: although this used to cause problems under heavy load, the latest
         version of this code, in combination with immediate GAPs (see 
	 sfw_rel_acknack(), doesn't seem to cause problems anymore, even
	 when stressed exceptionally hard. */

/* sfw_rel_rem_change -- Remove a sample from a writer's proxy reader list. */

static void sfw_rel_rem_change (RemReader_t *rrp, Change_t *cp)
{
	CCREF			*rp, *next_rp, *nrp, *fp, *lp;
#ifdef OPT_RR_LIST
	CCREF			*prev_rp;
#endif
	SequenceNumber_t	snr;

	ctrc_printd (RTPS_ID, RTPS_SFW_REL_REM, &rrp, sizeof (rrp));
	prof_start (rtps_rw_rem);

	RR_SIGNAL (rrp, "REL-RemChange");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_rmch)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_NEW_CHANGE, "sfw_rel_rem_change");
#endif
	fp = LIST_HEAD (rrp->rr_changes);
	lp = LIST_TAIL (rrp->rr_changes);
	if (!fp)
		return;	/* No list. */

	/* Lookup change in list of changes, oldest first. */
	snr = cp->c_seqnr;
	if ((fp->relevant && SEQNR_LT (snr, fp->u.c.change->c_seqnr)) ||
	    (!fp->relevant && SEQNR_LT (snr, fp->u.range.first)))
		return;	/* No longer in list (too old). */

	else if ((fp->relevant && SEQNR_EQ (fp->u.c.change->c_seqnr, snr)) ||
		 (!fp->relevant &&
		  !SEQNR_LT (snr, fp->u.range.first) &&
	    	  !SEQNR_GT (snr, fp->u.range.last))) {
		rp = fp; /* Oldest element. */
#ifdef OPT_RR_LIST
		prev_rp = NULL;
#endif
	}
	else if ((lp->relevant && SEQNR_GT (snr, lp->u.c.change->c_seqnr)) ||
		 (!lp->relevant && SEQNR_GT (snr, lp->u.range.last)))
		return;	/* Not yet in list (filtered?). */

	else if ((lp->relevant && SEQNR_EQ (lp->u.c.change->c_seqnr, snr)) ||
		 (!lp->relevant &&
		  !SEQNR_LT (snr, lp->u.range.first) &&
	    	  !SEQNR_GT (snr, lp->u.range.last))) {
		rp = lp; /* Newest list entry. */
#ifdef OPT_RR_LIST
		prev_rp = LIST_PREV (rrp->rr_changes, *rp);
#endif
	}
	else { /* Somewhere between first and last -> need to search. */
#ifdef OPT_RR_LIST
		prev_rp = fp;
#endif
		rp = fp->next;
		while (rp) {
			if (rp->relevant) {
				if (SEQNR_EQ (rp->u.c.change->c_seqnr, snr))
					break;
			}
			else
				if (!SEQNR_LT (snr, rp->u.range.first) &&
				    !SEQNR_GT (snr, rp->u.range.last))
					break;
#ifdef OPT_RR_LIST
			prev_rp = rp;
#endif
			rp = rp->next;
			if (LIST_END (rrp->rr_changes, rp)) {
				rp = NULL;
				break;
			}
		}
		if (!rp)
			return; /* Not in list! */
	}

	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_rem_change-1");
	hc_change_free (rp->u.c.change);
#ifdef RTPS_FRAGMENTS
	if (rp->fragments) {
		xfree (rp->fragments);
		rp->fragments = NULL;
		rrp->rr_nfrags--;
	}
#endif
	rp->relevant = 0;
	rp->u.range.first = snr;
	rp->u.range.last = snr;

	next_rp = LIST_NEXT (rrp->rr_changes, *rp);

	/* Update sample pointers if necessary. */
	if (rrp->rr_unsent_changes == rp) {
		for (nrp = next_rp;
		     nrp;
		     nrp = LIST_NEXT (rrp->rr_changes, *nrp))
			if (nrp->relevant) {
				rrp->rr_unsent_changes = nrp;
				break;
			}
		if (!nrp)
			rrp->rr_unsent_changes = NULL;
	}
	if (rrp->rr_astate == RRAS_REPAIRING &&
	    rrp->rr_requested_changes == rp) {
		for (nrp = next_rp;
		     nrp;
		     nrp = LIST_NEXT (rrp->rr_changes, *nrp))
			if (nrp->state == CS_REQUESTED) {
				rrp->rr_requested_changes = nrp;
				break;
			}
		if (!nrp)
			rrp->rr_requested_changes = NULL;
	}

#ifdef OPT_RR_LIST
	/* If there are GAP neighbours, try to coagulate the gaps. */
	if (prev_rp && !prev_rp->relevant) {
		prev_rp->u.range.last = snr;
		LIST_REMOVE (rrp->rr_changes, *rp);
		rrp->rr_changes.nchanges--;
		ccref_delete (rp);
		rp = prev_rp;
	}
	if (next_rp && !next_rp->relevant) {
		next_rp->u.range.first = rp->u.range.first;
		LIST_REMOVE (rrp->rr_changes, *rp);
		rrp->rr_changes.nchanges--;
		ccref_delete (rp);
	}

	/* Cleanup all initial GAP entries. */
	while ((rp = LIST_HEAD (rrp->rr_changes)) != NULL && !rp->relevant) {
		LIST_REMOVE (rrp->rr_changes, *rp);
		rrp->rr_changes.nchanges--;
		ccref_delete (rp);
	}
#endif
	prof_stop (rtps_rw_rem, 1);
}

/* sfw_rel_finish -- Stop a Reliable stateful writer's reader proxy.

   Locks: On entry, the writer associated with the remote reader, its
          domain, publisher and topic should be locked. */

static void sfw_rel_finish (RemReader_t *rrp)
{
	ctrc_printd (RTPS_ID, RTPS_SFW_REL_FINISH, &rrp, sizeof (rrp));
	prof_start (rtps_rw_finish);

	RR_SIGNAL (rrp, "REL-Finish");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_finish)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_FINISH, "sfw_rel_finish");
#endif
	/* Block until reader proxy is no longer active. */
	proxy_wait_inactive (&rrp->proxy);

	/* Cleanup all queued samples. */
	change_delete_enqueued (rrp);

	/* Stop sending Heartbeats if this is the last proxy. */
	sfw_rel_all_acked (rrp);

	/* Finally we're done. */
	NEW_RR_CSTATE (rrp, RRCS_FINAL, 0);

	prof_stop (rtps_rw_finish, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_rel_finish");
	REL_CHECK (rrp);
}

RR_EVENTS sfw_rel_events = {
	sfw_rel_start,
	sfw_rel_new_change,
	sfw_rel_send_data,
	sfw_rel_acknack,
#ifdef RTPS_FRAGMENTS
	sfw_rel_nackfrag,
#endif
	sfw_rel_rem_change,
	sfw_rel_finish
};

