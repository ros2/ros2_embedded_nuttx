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

/* rtps_slbw.c -- Implementation of the RTPS Best-effort Stateless Writer. */

#include "log.h"
#include "prof.h"
#include "error.h"
#include "ctrace.h"
#include "list.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_msg.h"
#include "rtps_trace.h"
#include "rtps_check.h"
#include "rtps_priv.h"
#include "rtps_clist.h"
#include "rtps_slbw.h"

unsigned rtps_sl_retries;

static void slw_be_resend_to (uintptr_t user);

/* unsent_changes_reset -- Reset the changes list so that all samples are
			   retransmitted. */

static void unsent_changes_reset (RemReader_t *rrp)
{
	CCREF		*rp;
	FTime_t		time;

	if (LIST_EMPTY (rrp->rr_changes)) {
		rrp->rr_unsent_changes = NULL;
		return;
	}
	rrp->rr_unsent_changes = rrp->rr_changes.head;
	sys_getftime (&time);
	LIST_FOREACH (rrp->rr_changes, rp) {
		rp->state = CS_UNSENT;
		if (rp->relevant)
			rp->u.c.change->c_time = time;
	}
	proxy_activate (&rrp->proxy);
	NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
}

/* slw_be_resend -- Resend all changes on all ReaderLocators. */

void slw_be_resend (WRITER *wp)
{
	RemReader_t	*rrp;
	Writer_t	*w = (Writer_t *) (wp->endpoint.endpoint);
	Ticks_t		t;

	LIST_FOREACH (wp->rem_readers, rrp) {
		unsent_changes_reset (rrp);
		if (rrp->rr_unsent_changes)
			NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
	}
	if (wp->slw_retries) {
		t = RTPS_SL_RETRY_TO / (TMR_UNIT_MS * MS);
		wp->slw_retries--;
		/*log_printf (RTPS_ID, 0, "Retries remaining=%u\r\n", wp->slw_retries);*/
	}
	else
		t = wp->rh_period;
	RESEND_TMR_START (wp, wp->rh_timer, t, (uintptr_t) wp, 
						slw_be_resend_to, &w->w_lock);
}

/* slw_be_alive -- Update Participant Liveliness. */

void slw_be_alive (WRITER *wp, GuidPrefix_t *prefix)
{
	RemReader_t		*rrp;
	CCREF			*rp;
	const unsigned char	*kp;

	LIST_FOREACH (wp->rem_readers, rrp) {
		for (rp = LIST_HEAD (rrp->rr_changes);
		     rp;
		     rp = LIST_NEXT (rrp->rr_changes, *rp)) {
			if (!rp->relevant)
				continue;

			kp = hc_key_ptr (wp->endpoint.endpoint->cache, rp->u.c.hci);
			if (!kp)
				continue;

			if (!memcmp (kp, prefix->prefix, sizeof (GuidPrefix_t))) {
				rp->state = CS_UNSENT;
				rrp->rr_unsent_changes = rrp->rr_changes.head;
				proxy_activate (&rrp->proxy);
				NEW_RR_TSTATE (rrp, RRTS_PUSHING, 0);
				break;
			}
		}
	}
}

/* slw_be_resend_to -- Time-out of the Resend timer. */

static void slw_be_resend_to (uintptr_t user)
{
	WRITER		*wp = (WRITER *) user;

#ifdef RTPS_MARKERS
	if (wp->endpoint.mark_res_to)
		rtps_marker_notify (&w->w_lep, EM_RESEND_TO, "slw_be_res_to");
#endif

	RESEND_TMR_TO (wp);
	slw_be_resend (wp);
}

/* rtps_stateless_resend -- Resend changes on a stateless writer. */

int rtps_stateless_resend (Writer_t *w)
{
	WRITER		*wp;
	int		ret;

	ctrc_printd (RTPS_ID, RTPS_W_RESEND, &w, sizeof (w));
	prof_start (rtps_w_resend);
	lock_take (w->w_lock);
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_stateless_resend: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	if (wp->endpoint.stateful ||!wp->endpoint.resends) {

		/* Not applicable in stateless mode. */
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	wp->slw_retries = rtps_sl_retries;
	slw_be_resend (wp);
	ret = DDS_RETCODE_OK;

    done:
	lock_release (w->w_lock);
	prof_stop (rtps_w_resend, 1);
	return (ret);
}

/* rtps_stateless_update -- An entry in the stateless writer cache was
			    updated by the application.  Resend it. */

int rtps_stateless_update (Writer_t *w, GuidPrefix_t *prefix)
{
	WRITER		*wp;
	int		ret;

	ctrc_printd (RTPS_ID, RTPS_W_UPDATE, &w, sizeof (w));
	prof_start (rtps_w_update);
	lock_take (w->w_lock);
	wp = w->w_rtps;
	if (!wp) {
		log_printf (RTPS_ID, 0, "rtps_stateless_update: writer(%s) doesn't exist!\r\n", str_ptr (w->w_topic->name));
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	if (wp->endpoint.stateful) {	/* Not applicable in stateless mode. */
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	slw_be_alive (wp, prefix);
	ret = DDS_RETCODE_OK;

    done:
	lock_release (w->w_lock);
	prof_stop (rtps_w_update, 1);
	return (ret);
}


/* 1. Stateless Best-effort Writer. */
/* -------------------------------- */

static void slw_be_start (RemReader_t *rrp)
{
	WRITER		*wp = rrp->rr_writer;
	Writer_t	*w = (Writer_t *) (wp->endpoint.endpoint);
	Ticks_t		t;

	ctrc_printd (RTPS_ID, RTPS_SLW_BE_START, &rrp, sizeof (rrp));
	prof_start (rtps_pw_start);

#ifdef RTPS_MARKERS
	if (wp->endpoint.mark_start)
		rtps_marker_notify (wp->endpoint.endpoint, EM_START, "slw_be_start");
#endif
	NEW_RR_CSTATE (rrp, RRCS_INITIAL, 1);
	if (wp->endpoint.resends && !wp->rh_timer) {

		/* Start a new Resend timer. */
		RESEND_TMR_ALLOC (wp);
		wp->rh_timer = tmr_alloc ();
		if (wp->rh_timer) {
			tmr_init (wp->rh_timer, "RTPS-Resend");
			wp->slw_retries = rtps_sl_retries;
			if (wp->slw_retries)
				t = RTPS_SL_RETRY_TO / (TMR_UNIT_MS * MS);
			else
				t = wp->rh_period;
			RESEND_TMR_START (wp, wp->rh_timer, t, (uintptr_t) wp,
						slw_be_resend_to, &w->w_lock);
		}
		else
			warn_printf ("slw_be_start: no more timer contexts!");
	}
	NEW_RR_CSTATE (rrp, RRCS_READY, 0);
	NEW_RR_TSTATE (rrp, RRTS_IDLE, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "slw_be_start");
	prof_stop (rtps_pw_start, 1);
}

static int slw_be_new_change (RemReader_t      *rrp,
			      Change_t         *cp,
			      HCI              hci,
			      SequenceNumber_t *snr)
{
	CCREF	*rp;

	ARG_NOT_USED (snr)

	ctrc_printd (RTPS_ID, RTPS_SLW_BE_NEW, &rrp, sizeof (rrp));
	prof_start (rtps_pw_new);

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_newch)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_NEW_CHANGE, "slw_be_new_change");
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
	prof_stop (rtps_pw_new, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "slw_be_new_change");
	return (1);
}

int be_send_data (RemReader_t *rrp, DiscoveredReader_t *reader)
{
	CCREF			*rp, *next_rp;
	DiscoveredReader_t	*drp;
	Participant_t		*pp;
	GuidPrefix_t		*prefix;
	const EntityId_t	*eid;
	DDS_HANDLE		dest;
	int			error;

	while ((rp = rrp->rr_unsent_changes) != NULL) {
		next_rp = LIST_NEXT (rrp->rr_changes, *rp);
		rp->state = CS_UNDERWAY;

		/* Remove change from unsent changes list. */
		rrp->rr_unsent_changes = next_rp;

		/* Check if this change should be sent. */
		if (rp->u.c.change->c_no_mcast)
			drp = reader;
		else if (rp->u.c.change->c_wack > 1)
			drp = NULL;
		else if (rp->u.c.change->c_wack == 1)
			drp = reader;
		else if (rrp->rr_writer->endpoint.stateful)
			goto skip_tx;	/* Already sent! */
		else
			drp = NULL;

		if (drp && drp->dr_participant) {
			prefix = &drp->dr_participant->p_guid_prefix;
			eid = &drp->dr_entity_id;
		}
		else if (!rrp->rr_writer->endpoint.stateful &&
		         (dest = rp->u.c.change->c_dests [0]) != 0 &&
			 (pp = (Participant_t *) entity_ptr (dest)) != NULL) {
			prefix = &pp->p_guid_prefix;
#ifdef DDS_NATIVE_SECURITY
			if (entity_id_eq (rrp->rr_writer->endpoint.endpoint->ep.entity_id,
					  rtps_builtin_eids [EPB_PARTICIPANT_SL_W]))
				eid = &rtps_builtin_eids [EPB_PARTICIPANT_SL_R];
			else
#endif
			if (entity_id_eq (rrp->rr_writer->endpoint.endpoint->ep.entity_id,
					  rtps_builtin_eids [EPB_PARTICIPANT_W]))
				eid = &rtps_builtin_eids [EPB_PARTICIPANT_R];
			else
				eid = NULL;
		}
		else {
			prefix = NULL;
			eid = NULL;
		}

		/* Add DATA submessage (possibly preceeded by INFO_TS). */
		error = rtps_msg_add_data (rrp,
					   prefix,
					   eid,
					   rp->u.c.change,
					   rp->u.c.hci,
					   next_rp == NULL
#ifdef RTPS_FRAGMENTS
					 , 0, NULL
#endif
					   );
		if (error) {
			CACHE_CHECK (&rrp->rr_writer->endpoint, "be_send_data-loop");
			return (1);
		}

		/* If stateless best-effort writer, we're done. */
	    	if (!rrp->rr_writer->endpoint.stateful &&
		    rrp->rr_writer->endpoint.resends)
			continue;

	    skip_tx:

		/* Indicate that sample should not be sent anymore. */
		if (rp->u.c.change->c_no_mcast)
			rp->u.c.change->c_wack--;
		else
			rp->u.c.change->c_wack = 0;

		/* If stateful best-effort: remove sample. */
		if (rp->ack_req)
			hc_acknowledged (rrp->rr_writer->endpoint.endpoint->cache,
					 rp->u.c.hci, &rp->u.c.change->c_seqnr);
		LIST_REMOVE (rrp->rr_changes, *rp);
	    	rrp->rr_changes.nchanges--;
		ccref_delete (rp);
	}
	NEW_RR_TSTATE (rrp, RRTS_IDLE, 0);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "be_send_data-done");
	return (0);
}

/* slw_be_send_data -- Send data on a ReaderLocator. */

static int slw_be_send_data (RemReader_t *rrp)
{
	int	error;

	ctrc_printd (RTPS_ID, RTPS_SLW_BE_SEND, &rrp, sizeof (rrp));
	prof_start (rtps_pw_send);

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_send)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_SEND, "slw_be_send_data");
#endif
	error = be_send_data (rrp, NULL);
	prof_stop (rtps_pw_send, 1);
	return (error);
}

static void slw_be_rem_change (RemReader_t *rrp, Change_t *cp)
{
	ctrc_printd (RTPS_ID, RTPS_SLW_BE_REM, &rrp, sizeof (rrp));
	prof_start (rtps_pw_rem);

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_rmch)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_REM_CHANGE, "slw_be_rem_change");
#endif
	change_remove (rrp, cp);
	prof_stop (rtps_pw_rem, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "slw_be_rem_change");
}

/* slw_be_finish -- Finish the Stateless best-effort writer's reader-locator.

   Locks: On entry, the writer associated with the remote reader, its
          domain, publisher and topic should be locked. */

static void slw_be_finish (RemReader_t *rrp)
{
	ctrc_printd (RTPS_ID, RTPS_SLW_BE_FINISH, &rrp, sizeof (rrp));
	prof_start (rtps_pw_finish);

#ifdef RTPS_MARKERS
	if (rrp->rr_writer->endpoint.mark_finish)
		rtps_marker_notify (rrp->rr_writer->endpoint.endpoint, EM_FINISH, "slw_be_finish");
#endif
	/* Wait until remote reader becomes inactive. */
	proxy_wait_inactive (&rrp->proxy);

	/* Stop timers. */
	if (LIST_SINGLE (*rrp) && rrp->rr_writer->rh_timer) {

		/* Stop Resend timer. */
		RESEND_TMR_STOP (rrp->rr_writer, rrp->rr_writer->rh_timer);
		RESEND_TMR_FREE (rrp->rr_writer);
		tmr_free (rrp->rr_writer->rh_timer);
		rrp->rr_writer->rh_timer = NULL;
	}

	/* Cleanup sample queues. */
	change_delete_enqueued (rrp);

	/* Finally we're done. */
	NEW_RR_CSTATE (rrp, RRCS_FINAL, 0);

	prof_stop (rtps_pw_finish, 1);
	CACHE_CHECK (&rrp->rr_writer->endpoint, "slw_be_finish-1");
}

RR_EVENTS slw_be_events = {
	slw_be_start,
	slw_be_new_change,
	slw_be_send_data,
	NULL,
#ifdef RTPS_FRAGMENTS
	NULL,
#endif
	slw_be_rem_change,
	slw_be_finish
};



