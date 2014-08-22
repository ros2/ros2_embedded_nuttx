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

/* rtps_sfbw.c -- Implements the RTPS Stateful Best-effort Writer. */

#include "log.h"
#include "prof.h"
#include "error.h"
#include "ctrace.h"
#include "list.h"
#include "dcps.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_msg.h"
#include "rtps_trace.h"
#include "rtps_check.h"
#include "rtps_priv.h"
#include "rtps_clist.h"
#include "rtps_slbw.h"
#include "rtps_sfbw.h"

static void sfw_be_start (RemReader_t *rrp)
{
	ctrc_printd (RTPS_ID, RTPS_SFW_BE_START, &rrp, sizeof (rrp));
	prof_start (rtps_bw_start);

	RR_SIGNAL (rrp, "BE-Start");
	NEW_RR_CSTATE (rrp, RRCS_INITIAL, 1);
	NEW_RR_CSTATE (rrp, RRCS_READY, 0);
	NEW_RR_TSTATE (rrp, RRTS_IDLE, 1);
	rrp->rr_nack_timer = NULL;

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_start)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_START, "sfw_be_start");
#endif

	/* Add existing cache entries to reader locator/proxy queue. */
	if (rrp->rr_endpoint->qos->qos.durability_kind)
		hc_replay (rrp->rr_writer->endpoint.endpoint->cache,
					proxy_add_change, (uintptr_t) rrp);

	if ((rrp->rr_unsent_changes = LIST_HEAD (rrp->rr_changes)) != NULL &&
	    rrp->rr_writer->endpoint.push_mode) {
		NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
		proxy_activate (&rrp->proxy);
	}
	prof_stop (rtps_bw_start, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_be_start");
}

static int sfw_be_new_change (RemReader_t      *rrp,
			      Change_t         *cp,
			      HCI              hci,
			      SequenceNumber_t *snr)
{
	CCREF	*rp;

	ARG_NOT_USED (snr)

	ctrc_printd (RTPS_ID, RTPS_SFW_BE_NEW, &rrp, sizeof (rrp));
	prof_start (rtps_bw_new);

	RR_SIGNAL (rrp, "BE-NewChange");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_newch)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_NEW_CHANGE, "sfw_be_new_change");
#endif
	rp = change_enqueue (rrp, cp, hci, CS_UNSENT);
	if (!rp)
		return (0);

	rp->ack_req = 1;
	rrp->rr_unacked++;
	if (!rrp->rr_unsent_changes) {
		rrp->rr_unsent_changes = rp;
		proxy_activate (&rrp->proxy);
	}
	NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_be_new_change");
	prof_stop (rtps_bw_new, 1);
	return (1);
}

static int sfw_be_send_data (RemReader_t *rrp)
{
	int	error;

	ctrc_printd (RTPS_ID, RTPS_SFW_BE_SEND, &rrp, sizeof (rrp));
	prof_start (rtps_bw_send);

	RR_SIGNAL (rrp, "BE-SendData");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_send)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_SEND, "sfw_be_send_data");
#endif
	error = be_send_data (rrp, (DiscoveredReader_t *) &rrp->rr_endpoint);
	prof_stop (rtps_bw_send, 1);
	return (error);
}

static void sfw_be_rem_change (RemReader_t *rrp, Change_t *cp)
{
	ctrc_printd (RTPS_ID, RTPS_SFW_BE_REM, &rrp, sizeof (rrp));
	prof_start (rtps_bw_rem);

	RR_SIGNAL (rrp, "BE-RemChange");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_rmch)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_REM_CHANGE, "sfw_be_rem_change");
#endif
	change_remove (rrp, cp);
	prof_stop (rtps_bw_rem, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_be_rem_change");
}

/* sfw_be_finish -- Stop a best-effort statefull writer's reader proxy.

   Locks: On entry, the writer associated with the remote reader, and its
          domain, publisher and topic should be locked. */

static void sfw_be_finish (RemReader_t *rrp)
{
	ctrc_printd (RTPS_ID, RTPS_SFW_BE_FINISH, &rrp, sizeof (rrp));
	prof_start (rtps_bw_finish);

	RR_SIGNAL (rrp, "BE-Finish");

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_finish)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_FINISH, "sfw_be_finish");
#endif
	/* Block until reader proxy is no longer active. */
	proxy_wait_inactive (&rrp->proxy);

	/* Cleanup samples queue. */
	change_delete_enqueued (rrp);

	/* Finally we're done. */
	NEW_RR_CSTATE (rrp, RRCS_FINAL, 0);

	prof_stop (rtps_bw_finish, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "sfw_be_finish");
}

RR_EVENTS sfw_be_events = {
	sfw_be_start,
	sfw_be_new_change,
	sfw_be_send_data,
	NULL,
#ifdef RTPS_FRAGMENTS
	NULL,
#endif
	sfw_be_rem_change,
	sfw_be_finish
};


