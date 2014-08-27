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

/* guard.c -- Add/remove guards on endpoints.
	      A guard allows the user to add a specific method to an endpoint
	      pair in order to enable extra functionalities for guarding the
	      usage of this endpoint pair, such as Liveliness, Deadline, etc. */

#include "error.h"
#include "disc.h"
#include "dcps.h"
#include "guard.h"

/*#define LOG_GUARD	** Define this for guard event logging. */

static lock_t *lock_ptr (Guard_t *gp)
{
	LocalEndpoint_t	*lep;
	Domain_t	*dp;

	if (gp->writer) {
		lep = (LocalEndpoint_t *) gp->wep;
		dp = lep->ep.u.publisher->domain;
	}
	else {
		lep = (LocalEndpoint_t *) gp->rep;
		dp = lep->ep.u.subscriber->domain;
	}
	if (gp->type == GT_LIVELINESS && 
	    gp->kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS)
		return (&dp->lock);
	else
#ifdef RW_LOCKS
		return (&lep->lock);
#else
		return (&lep->ep.topic->lock);
#endif
}

/* guard_insert -- Insert a guard into a sorted guard chain, updating the active
		   timer . */

static Guard_t *guard_insert (Guard_t      **list,
			      GuardType_t  type,	
			      unsigned     kind,
			      int          writer,
			      GuardMode_t  mode,
			      unsigned     p,
			      Endpoint_t   *wp,
			      Endpoint_t   *rp,
			      TMR_FCT      fct)
{
	Guard_t		*first_gp, *gp;
	unsigned	rem;

	first_gp = guard_first (*list, type, kind, writer);

	/* Add guard context to appropriate lists. */
	gp = guard_add (list, type, kind, writer, wp, rp, p);
	if (!gp)
		return (NULL);

	gp->mode = mode;
	sys_getftime (&gp->time);
	if (!mode)
		return (gp);

	if (mode == GM_MIXED)
		p = (p * 7) >> 3;

	/* Start appropriate timer if required. */
	if (!first_gp || mode == GM_ONE_SHOT || mode == GM_PROGRESSIVE) {
		if (first_gp && mode == GM_PROGRESSIVE)
			mode = GM_ONE_SHOT;
		gp->timer = tmr_alloc ();
		if (!gp->timer) {
			gp = guard_unlink (list, type, kind, writer, wp, rp);
			guard_free (gp);
			return (NULL);
		}
		tmr_init (gp->timer, "Guard");
		gp->cmode = mode;
		tmr_start_lock (gp->timer, p,
					(uintptr_t) gp, fct, lock_ptr (gp));
	}
	else if ((mode == GM_PERIODIC || mode == GM_MIXED) &&
		 first_gp && 
		 gp->period < first_gp->period &&
		 first_gp->timer) {

		/* New guard context supercedes previous top entry in list. */
		rem = tmr_remain (first_gp->timer);
		tmr_stop (first_gp->timer);
		first_gp->cmode = GM_NONE;
		gp->timer = first_gp->timer;
		first_gp->timer = NULL;
		gp->cmode = mode;
		tmr_start_lock (gp->timer, (rem >= p) ? p : rem,
					(uintptr_t) gp, fct, lock_ptr (gp));
	}
	return (gp);
}

/* guard_extract -- Remove a guard from a sorted timer chain with the timer on
		    top. */

static void guard_extract (Guard_t     **list,
		           GuardType_t type,
		           unsigned    kind,
		           int         writer,
		           Endpoint_t  *wp,
		           Endpoint_t  *rp)
{
	Guard_t		*first_gp, *gp, *next_gp;
	TMR_FCT		fct;
	unsigned	rem;

	first_gp = guard_first (*list, type, kind, writer);
	gp = guard_unlink (list, type, kind, writer, wp, rp);
	if (!gp)
		return;

	if (gp->timer) {
		if ((gp->mode == GM_PERIODIC && gp == first_gp) ||
		    gp->mode == GM_PROGRESSIVE) {
			next_gp = guard_first (*list, type, kind, writer);
			if (next_gp) {
				fct = gp->timer->tcbf;
				rem = tmr_remain (gp->timer);
				tmr_stop (gp->timer);
				next_gp->timer = gp->timer;
				gp->timer = NULL;
				next_gp->cmode = next_gp->mode;
				tmr_start_lock (next_gp->timer,
					   next_gp->period - gp->period + rem,
					   (uintptr_t) next_gp, fct,
					   lock_ptr (gp));
			}
		}
		if (gp->timer) {
			tmr_stop (gp->timer);
			tmr_free (gp->timer);
			gp->timer = NULL;
		}
	}
	guard_free (gp);
}

/* guard_timeout -- Handle all timeouts corresponding to a timers list. */

static void guard_timeout (Guard_t *list, TMR_FCT tmr_fct, TMR_FCT xfct)
{
	Guard_t		*gp = (Guard_t *) list, *prev_gp;
	Timer_t		*tp;
	int		part;

	/* Do requested timer action. */
	tp = gp->timer;
	(*xfct) ((uintptr_t) gp);
	if (gp->mode == GM_PERIODIC) {
		tmr_start_lock (tp, gp->period, (uintptr_t) gp,
							tmr_fct, lock_ptr (gp));
		return;
	}
	gp->timer = NULL;
	gp->cmode = GM_NONE;
	if (gp->mode == GM_ONE_SHOT) {
		tmr_free (tp);
		return;
	}
	part = (gp->type == GT_LIVELINESS &&
	        gp->kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS);

	/* Progressive timeout!
	   Remove timer from context, but don't free. */
	do {
		/* More time-outs? */
		prev_gp = gp;
		gp = guard_first ((part) ? gp->pnext : gp->enext,
					gp->type, gp->kind, gp->writer);
		if (!gp)
			tmr_free (tp);
		else if (gp->period > prev_gp->period) {
			if (gp->timer && gp->mode == GM_ONE_SHOT)
				continue;

			else if (gp->timer)
				tmr_free (tp);
			else {
				gp->timer = tp;
				gp->cmode = GM_PROGRESSIVE;
				tmr_start_lock (tp, gp->period - prev_gp->period,
						       (uintptr_t) gp, tmr_fct,
						       lock_ptr (gp));
			}
			gp = NULL;
		}
		else
			(*xfct) ((uintptr_t) gp);
	}
	while (gp);
}

/* guard_restart -- Restart all guard timers, i.e. reset the Progressive timer,
		    if it still exists, to the head of the list, while resetting
		    every node that previously timed out. */

static void guard_restart (Guard_t *list,
			   GuardType_t type,
			   unsigned kind,
			   int writer,
			   unsigned p,
			   TMR_FCT tmr_fct,
			   TMR_FCT xfct)
{
	Guard_t		*gp, *first_gp;
	Timer_t		*tp;
	int		part;

	part = (type == GT_LIVELINESS &&
	        kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS);
	first_gp = NULL;
	tp = NULL;
	for (gp = list; gp; gp = (part) ? gp->pnext : gp->enext) {
		if (gp->type != type ||
		    gp->kind != kind ||
		    gp->writer != writer)
			continue;

		if (!first_gp)
			first_gp = gp;

		if (xfct)
			(*xfct) ((uintptr_t) gp);

		if (gp->timer) {
			tmr_stop (gp->timer);
			if (tp)
				tmr_free (gp->timer);
			else
				tp = gp->timer;
			gp->timer = NULL;
			gp->cmode = GM_NONE;
		}
	}
	if (first_gp) {
		if (!tp)
			tp = tmr_alloc ();
		first_gp->timer = tp;
		first_gp->cmode = first_gp->mode;
		if (!p)
			p = first_gp->period;
		if (!tp)
			warn_printf ("guard_restart: failed to allocate timer!");
		else {
			tmr_init (tp, "Guard");
			tmr_start_lock (tp, p, (uintptr_t) first_gp, tmr_fct,
							lock_ptr (first_gp));
		}
	}
}


/* 1. Automatic Liveliness. */
/* ------------------------ */

/* wl_auto_timeout -- Automatic liveliness time-out. */

static void wl_auto_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Writer_t	*wp;
	Domain_t	*dp;

	wp = (Writer_t *) gp->wep;
	dp = wp->w_publisher->domain;

	/* Resend the automatic liveliness participant message. */
	disc_send_liveliness_msg (dp, gp->kind);

	/* Trigger the local automatic liveliness domain participant. */
	liveliness_participant_event (&dp->participant, 0);

	/* Restart the timer. */
	tmr_start_lock (gp->timer, gp->period, p, wl_auto_timeout, &dp->lock);
}

/* wl_add_auto -- Add Automatic Liveliness for a local Writer. */

static int wl_add_auto (Domain_t *dp, Writer_t *wp, Endpoint_t *rp, unsigned p)
{
	Guard_t		*gp;

#ifdef LOG_GUARD
	dbg_printf ("wl_add_auto ({%u}->{%u}, %u)\r\n",
			wp->w_handle, rp->entity.handle, p);
#endif
	gp = guard_insert (&dp->participant.p_liveliness,
			   GT_LIVELINESS,
		           DDS_AUTOMATIC_LIVELINESS_QOS,
		           1, GM_PERIODIC, (p * 7) >> 3,
			   &wp->w_ep, rp, 
			   wl_auto_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* wl_rem_part -- Remove Participant Liveliness from a local Writer. */

static void wl_rem_part (Domain_t   *dp,
		         unsigned   kind,
		         Writer_t   *wp,
		         Endpoint_t *rp)
{
	guard_extract (&dp->participant.p_liveliness,
		       GT_LIVELINESS, kind, 1, &wp->w_ep, rp);
}

/* wl_rem_auto -- Remove Automatic Liveliness from a local Writer. */

static void wl_rem_auto (Domain_t *dp, Writer_t *wp, Endpoint_t *rp)
{
#ifdef LOG_GUARD
	dbg_printf ("wl_rem_auto ({%u}->{%u})\r\n",
			wp->w_handle, rp->entity.handle));
#endif
	wl_rem_part (dp, DDS_AUTOMATIC_LIVELINESS_QOS, wp, rp);
}

/* rl_liveliness_timeout -- Reader liveliness timer elapsed. */

static void rl_liveliness_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Reader_t	*rp;
	handle_t	h;

	/* Notify that reader is no longer alive. */
	if (gp->alive) {
		gp->alive = 0;
		rp = (Reader_t *) gp->rep;
		lock_take (rp->r_lock);
		h = gp->wep->entity.handle;
		hc_rem_writer_removed (rp->r_cache, h);
		dcps_liveliness_change (rp, DLI_EXISTS, 0, h);
		lock_release (rp->r_lock);
	}
}

/* rl_timeout -- Liveliness timeout for a number of Readers. */

static void rl_timeout (uintptr_t p)
{
	guard_timeout ((Guard_t *) p, rl_timeout, rl_liveliness_timeout);
}

/* rl_add_part -- Add Liveliness for a local Reader. */

static int rl_add_part (Domain_t   *dp,
			unsigned   kind,
			unsigned   p,
			Reader_t   *rp,
			Endpoint_t *wp)
{
	Guard_t		*gp;
	Participant_t	*pp;

	if (entity_local (wp->entity.flags))
		pp = &dp->participant;
	else
		pp = wp->u.participant;
	gp = guard_insert (&pp->p_liveliness,
			   GT_LIVELINESS,
			   kind, 0,
			   GM_PROGRESSIVE, p,
			   wp, &rp->r_ep,
			   rl_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* rl_add_auto -- Add Automatic Liveliness for a local Reader. */

static int rl_add_auto (Domain_t *dp, Reader_t *rp, Endpoint_t *wp, unsigned p)
{
#ifdef LOG_GUARD
	dbg_printf ("rl_add_auto ({%u}<-{%u}, %u)\r\n",
			rp->r_handle, wp->entity.handle, p);
#endif
	return (rl_add_part (dp, DDS_AUTOMATIC_LIVELINESS_QOS, p, rp, wp));
}

/* rl_rem_part -- Remove a Reader Liveliness record from a merged chain. */

static void rl_rem_part (Domain_t   *dp,
		         unsigned   kind,
		         Reader_t   *rp,
		         Endpoint_t *wp)
{
	Participant_t	*pp;

	if (entity_local (wp->entity.flags))
		pp = &dp->participant;
	else
		pp = wp->u.participant;
	guard_extract (&pp->p_liveliness, 
		       GT_LIVELINESS,
		       kind, 0,
		       wp, &rp->r_ep);
}

/* rl_rem_auto -- Remove Automatic Liveliness from a local Reader. */

static void rl_rem_auto (Domain_t *dp, Reader_t *rp, Endpoint_t *wp)
{
#ifdef LOG_GUARD
	dbg_printf ("rl_rem_auto ({%u}<-{%u})\r\n",
			rp->r_entity.handle, wp->entity.handle));
#endif
	rl_rem_part (dp, DDS_AUTOMATIC_LIVELINESS_QOS, rp, wp); 
}

/* rl_restart -- Called when the reader liveliness list is restarted for each
		 set of Reader/Writer associations. */

static void rl_restart (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Reader_t	*rp;

	if (!gp->alive) {
		gp->alive = 1;
		rp = (Reader_t *) gp->rep;
		lock_take (rp->r_lock);
		dcps_liveliness_change (rp,
					DLI_EXISTS, 1,
					gp->wep->entity.handle);
		lock_release (rp->r_lock);
	}
}

/* rl_participant_event -- Participant message indicates new liveliness. */

void rl_participant_event (Participant_t *pp, unsigned kind)
{
	guard_restart (pp->p_liveliness,
		       GT_LIVELINESS,
		       kind, 0,
		       0,
		       rl_timeout, rl_restart);
}

/* liveliness_participant_event -- Participant message indicates new liveliness. */

void liveliness_participant_event (Participant_t *pp, int manual)
{
	rl_participant_event (pp, (manual) ? DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS : 
					     DDS_AUTOMATIC_LIVELINESS_QOS);
}


/* 2. Manual-by-Participant Liveliness. */
/* ------------------------------------ */

/* wl_lost_timeout -- Time-out on a Manual-by-Participant/Topic Writer. */

static void wl_lost_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Writer_t	*wp;

	if (!gp->alive)
		return;

	gp->alive = 0;
	wp = (Writer_t *) gp->wep;
	lock_take (wp->w_lock);
	dcps_liveliness_lost (wp);
	lock_release (wp->w_lock);
}

/* wl_lost_restore -- Restore a Writer in lost state. */

static void wl_lost_restore (uintptr_t p)
{
	Guard_t	*gp = (Guard_t *) p;

	if (gp->alive)
		return;

	gp->alive = 1;
}

/* time_delta_ticks -- Return a ticks delta between two absolute timestamps. */

Ticks_t time_delta_ticks (Time_t *new, Time_t *old)
{
	Ticks_t		delta;

	if (old->seconds > new->seconds)
		return (0);

	if (old->seconds == new->seconds) {
		if (old->nanos >= new->nanos)
			return (0);

		delta = (new->nanos - old->nanos) / (TMR_UNIT_MS * 1000 * 1000);
	}
	else {
		if (old->nanos > new->nanos) {
			new->nanos += 1000000000;
			new->seconds--;
		}
		delta = (new->seconds - old->seconds) * TICKS_PER_SEC +
		        (new->nanos - old->nanos) / (TMR_UNIT_MS * 1000 * 1000);
	}
	return (delta);
}

/* wl_manual_timeout -- Manual by Participant writer Liveliness time-out.*/

static void wl_manual_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p, *next_gp;
	Writer_t	*wp;
	Domain_t	*dp;
	FTime_t		ntime, now, ptime;
	Ticks_t		delta;
	unsigned	nalive;
	unsigned	period;
	int		part;

	wp = (Writer_t *) gp->wep;
	lock_take (wp->w_lock);
	dp = wp->w_publisher->domain;
	lock_release (wp->w_lock);
	part = gp->kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS;
	if (gp->timer && gp->cmode == GM_MIXED) {
		if (gp->critical)
			goto critical;

		/* Check if there was traffic on the endpoint(s). */
		for (nalive = 0; gp; ) {
			wp = (Writer_t *) gp->wep;
			lock_take (wp->w_lock);
			if ((wp->w_flags & EF_ALIVE) != 0) {
				nalive++;
				wp->w_flags &= ~EF_ALIVE;
			}
			lock_release (wp->w_lock);
			if (!part)
				break;

			next_gp = gp->pnext;
			if (!next_gp)
				break;

			gp = guard_first (next_gp, GT_LIVELINESS, gp->kind, 1);
		}
		gp = (Guard_t *) p;
		wp = (Writer_t *) gp->wep;
		lock_take (wp->w_lock);
		ntime = ((LocalEndpoint_t *) gp->wep)->guard->time;
		period = (gp->period * 7) >> 3;
		FTIME_SETT (ptime, period);
		FTIME_ADD (ntime, ptime);
		lock_release (wp->w_lock);
		sys_getftime (&now);
		if (FTIME_GT (ntime, now)) {
			FTIME_SUB (ntime, now);
			delta = FTIME_TICKS (ntime);
		}
		else
			delta = 1;
		if (nalive) { /* At least one writer sent data - restart. */
			tmr_start_lock (gp->timer, delta ? delta : 1, p, wl_manual_timeout, &dp->lock);
			liveliness_participant_event (&dp->participant, 1);
			if (part)
				disc_send_liveliness_msg (dp, 
					DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
			return;
		}

		/* Request for notification of any endpoint in the list when
		   data is sent. */
		gp->critical = 1;
		while (gp) {
			wp = (Writer_t *) gp->wep;
			lock_take (wp->w_lock);
			wp->w_flags |= EF_LNOTIFY;
			lock_release (wp->w_lock);
			if (!part)
				break;

			next_gp = gp->pnext;
			if (!next_gp)
				break;

			gp = guard_first (next_gp, GT_LIVELINESS, gp->kind, 1);
		}

		/* Restart timer for the remaining period. */
		gp = (Guard_t *) p;
		delta += gp->period >> 3;
		if (delta) {
			tmr_start_lock (gp->timer, delta, p, wl_manual_timeout, &dp->lock);
			return;
		}

	    critical:
	    	
		/* No data on selected endpoints -> convert timer to 
		   progressive mode to timeout all writers that are not
		   obeying the agreed liveliness contract. */
		gp->critical = 0;
		gp->cmode = GM_PROGRESSIVE;
		guard_timeout (gp, wl_manual_timeout, wl_lost_timeout);
		return;
	}

	/* Alert every writer according to its liveliness contract. */
	guard_timeout ((Guard_t *) p, wl_manual_timeout, wl_lost_timeout);
}

/* wl_manual_alive -- Liveliness is restored for a specific manual writer 
		      guard. */

static void wl_manual_alive (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Writer_t	*wp;

	wp = (Writer_t *) gp->wep;
	gp->alive = 1;
	gp->critical = 0;
	gp->mark = 0;
	lock_take (wp->w_lock);
	gp->wep->entity.flags &= ~(EF_LNOTIFY | EF_ALIVE);
	lock_release (wp->w_lock);
}

/* wl_manual_restored -- Liveliness is restored for a manual mode guard. */

static void wl_manual_restored (Guard_t *gp)
{
	Domain_t	*dp;
	Writer_t	*wp;

	wp = (Writer_t *) gp->wep;
	dp = wp->w_publisher->domain;
	lock_release (wp->w_lock);
	if (gp->kind < DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS) {

		disc_send_liveliness_msg (dp, 
					DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
		liveliness_participant_event (&dp->participant, 1);
		guard_restart (dp->participant.p_liveliness, GT_LIVELINESS,
			       gp->kind, 1, 0, 
			       wl_manual_timeout, wl_manual_alive);
	}
	else {
		lock_release (wp->w_lock);
		guard_restart (wp->w_guard, GT_LIVELINESS,
			       gp->kind, 1, 0, 
			       wl_manual_timeout, wl_manual_alive);
		lock_release (dp->lock);
	}
	lock_take (wp->w_lock);
}

/* liveliness_participant_asserted -- Participant assertion received. */

void liveliness_participant_asserted (Participant_t *pp)
{
	rl_participant_event (pp, DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS);
}

/* liveliness_participant_assert -- Assert liveliness on a local Domain
				    Participant. */

void liveliness_participant_assert (Domain_t *dp)
{
	/* Restart local Manual-by-Participant writer livelines guards. */
	guard_restart (dp->participant.p_liveliness, GT_LIVELINESS,
				DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS, 1, 0,
				wl_manual_timeout, wl_manual_alive);

	/* Resend participant info. */
	disc_send_participant_liveliness (dp);

	/* Trigger the local automatic liveliness domain participant. */
	liveliness_participant_asserted (&dp->participant);
}

/* wl_add_man_participant -- Add Manual by Participant Liveliness for a local
			     Writer. */

static int wl_add_man_participant (Domain_t   *dp,
				   Writer_t   *wp,
				   Endpoint_t *rp,
				   unsigned   p)
{
	Guard_t		*gp;

#ifdef LOG_GUARD
	dbg_printf ("wl_add_man_participant ({%u}->{%u}, %u)\r\n",
			wp->w_handle, rp->entity.handle, p);
#endif
	gp = guard_insert (&dp->participant.p_liveliness,
			   GT_LIVELINESS,
		           DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS,
		           1, GM_MIXED, p,
			   &wp->w_ep, rp,
			   wl_manual_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* wl_rem_man_participant -- Remove Manual by Participant Liveliness from a
			     local Writer. */

static void wl_rem_man_participant (Domain_t *dp, Writer_t *wp, Endpoint_t *rp)
{
#ifdef LOG_GUARD
	dbg_printf ("wl_rem_man_participant ({%u}->{%u})\r\n",
			wp->w_handle, rp->entity.handle);
#endif
	wl_rem_part (dp, DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS, wp, rp);
}

/* rl_add_man_participant -- Add Manual by Participant Liveliness for a local
			     Reader. */

static int rl_add_man_participant (Domain_t   *dp,
				   Reader_t   *rp,
				   Endpoint_t *wp,
				   unsigned   p)
{
#ifdef LOG_GUARD
	dbg_printf ("rl_add_man_participant ({%u}<-{%u}, %u)\r\n",
			rp->r_handle, wp->entity.handle, p);
#endif
	return (rl_add_part (dp, DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS, p,
				rp, wp));
}

/* rl_rem_man_participant -- Remove Manual by Participant Liveliness from a
			     local Reader. */

static void rl_rem_man_participant (Domain_t *dp, Reader_t *rp, Endpoint_t *wp)
{
#ifdef LOG_GUARD
	dbg_printf ("rl_rem_man_participant ({%u}<-{%u})\r\n",
			rp->r_handle, wp->entity.handle);
#endif
	rl_rem_part (dp, DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS, rp, wp);
}


/* 3. Manual-by-Topic liveliness. */
/* ------------------------------ */

/* wl_topic_timeout -- Manual by Topic writer Liveliness time-out. */

static void wl_topic_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Writer_t	*wp;
	FTime_t		ntime, ptime, now;
	Ticks_t		delta;

	wp = (Writer_t *) gp->wep;
	if ((wp->w_flags & EF_ALIVE) != 0) {

		/* Writer still sending: calculate timestamp for lost time-out
		   in case no samples would be received anymore. */
		wp->w_flags &= ~EF_ALIVE;

		ntime = wp->w_guard->time;
		FTIME_SETT (ptime, gp->period);
		FTIME_ADD (ntime, ptime);
		sys_getftime (&now);
		if (FTIME_GT (ntime, now)) {
			FTIME_SUB (ntime, now);
			delta = FTIME_TICKS (ntime);
			if (!delta)
				delta = 1;
		}
		else
			delta = 1;
		tmr_start_lock (gp->timer, delta, p, wl_topic_timeout, &wp->w_lock);
		return;
	}
	gp->wep->entity.flags |= EF_LNOTIFY;

	/* Alert writer liveliness lost according to its liveliness contract. */
	guard_timeout ((Guard_t *) p, wl_topic_timeout, wl_lost_timeout);
}


/* wl_topic_restored -- Writer Liveliness restored for a topic. */

static void wl_topic_restored (Guard_t *gp)
{
	Guard_t		*first_gp;
	Writer_t	*wp = (Writer_t *) gp->wep;

	first_gp = guard_first (wp->w_guard, GT_LIVELINESS, DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS, 1);
	guard_restart (first_gp,
		       GT_LIVELINESS,
		       DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS, 1,
		       0,
		       wl_topic_timeout, wl_lost_restore);
}

/* wl_add_man_topic -- Add a Manual by Topic Liveliness from a local
		       Writer. */

static int wl_add_man_topic (Writer_t *wp, Endpoint_t *rp, unsigned p)
{
	Guard_t		*gp;

#ifdef LOG_GUARD
	dbg_printf ("wl_add_man_topic ({%u}->{%u}, %u)\r\n",
			wp->w_handle, rp->entity.handle, p);
#endif
	gp = guard_insert (&wp->w_guard,
			   GT_LIVELINESS,
		           DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS,
		           1, GM_PROGRESSIVE, p,
			   &wp->w_ep, rp,
			   wl_topic_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* wl_rem_man_topic -- Remove Manual by Participant Liveliness from a local
		       Writer. */

static void wl_rem_man_topic (Writer_t *wp, Endpoint_t *rp)
{
#ifdef LOG_GUARD
	dbg_printf ("wl_rem_man_topic ({%u}->{%u})\r\n",
			wp->w_handle, rp->entity.handle);
#endif
	guard_extract (&wp->w_guard, 
		       GT_LIVELINESS,
		       DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS,
		       1, &wp->w_ep, rp);
}

/* rl_topic_timeout -- Check Reader liveliness. */

static void rl_topic_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Reader_t	*rp;
	FTime_t		ntime, now, ptime;
	Ticks_t		delta;

	rp = (Reader_t *) gp->rep;
	if ((rp->r_flags & EF_ALIVE) != 0) {

		/* Reader still receiving: calculate timestamp for lost time-out
		   in case no samples would be received anymore. */
		rp->r_flags &= ~EF_ALIVE;

		ntime = rp->r_guard->time;
		FTIME_SETT (ptime, gp->period);
		FTIME_ADD (ntime, ptime);
		sys_getftime (&now);
		if (FTIME_GT (ntime, now)) {
			FTIME_SUB (ntime, now);
			delta = FTIME_TICKS (ntime);
			if (!delta)
				delta = 1;
		}
		else
			delta = 1;
		tmr_start_lock (gp->timer, delta, p, rl_topic_timeout, &rp->r_lock);
		return;
	}
	rp->r_flags |= EF_LNOTIFY;

	/* Alert writer liveliness lost according to its liveliness contract. */
	guard_timeout ((Guard_t *) p, rl_topic_timeout, rl_liveliness_timeout);
}

/* rl_topic_restored -- Reader Liveliness restored for a topic. */

static void rl_topic_restored (Guard_t *gp)
{
	Guard_t		*first_gp;

	first_gp = ((Reader_t *) gp->rep)->r_guard;
	guard_restart (first_gp, GT_LIVELINESS, gp->kind, 0, 0,
			rl_topic_timeout, rl_restart);
}

/* rl_add_man_topic -- Add Manual by Topic Liveliness for a local Reader. */

static int rl_add_man_topic (Reader_t   *rp,
			     Endpoint_t *wp,
			     unsigned   p)
{
	Guard_t		*gp;

#ifdef LOG_GUARD
	dbg_printf ("rl_add_man_topic ({%u}<-{%u}, %u)\r\n",
			rp->r_handle, wp->entity.handle, p);
#endif
	gp = guard_insert (&rp->r_guard,
			   GT_LIVELINESS,
			   DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS,
			   0, GM_PROGRESSIVE, p,
			   wp, &rp->r_ep,
			   rl_topic_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* rl_rem_man_topic -- Remove Manual by Topic Liveliness from a local Reader. */

static void rl_rem_man_topic (Reader_t *rp, Endpoint_t *wp)
{
#ifdef LOG_GUARD
	dbg_printf ("rl_rem_man_topic ({%u}<-{%u})\r\n",
			rp->r_handle, wp->entity.handle);
#endif
	guard_extract (&rp->r_guard, 
		       GT_LIVELINESS,
		       DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS,
		       0, wp, &rp->r_ep);
}


/* Add/remove Liveness operations. */
/* ------------------------------- */

/* wl_add -- Add liveliness for a local Writer for the given endpoint and QoS. */

static int wl_add (Domain_t *dp, Writer_t *wp, Endpoint_t *rp, UniQos_t *qp)
{
	unsigned	period;
	int		error;

	period = duration2ticks ((const Duration_t *) 
					&qp->liveliness_lease_duration);
	switch (qp->liveliness_kind) {
		case DDS_AUTOMATIC_LIVELINESS_QOS:
			error = wl_add_auto (dp, wp, rp, period);
			break;
		case DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS:
			error = wl_add_man_participant (dp, wp, rp, period);
			break;
		case DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS:
			error = wl_add_man_topic (wp, rp, period);
			break;
		default:
			error = DDS_RETCODE_ERROR;
	}
	return (error);
}

/* wl_remove -- Remove liveliness for a local Writer for the given endpoint/QoS.*/

static void wl_remove (Domain_t *dp, Writer_t *wp, Endpoint_t *rp, UniQos_t *qp)
{
	switch (qp->liveliness_kind) {
		case DDS_AUTOMATIC_LIVELINESS_QOS:
			wl_rem_auto (dp, wp, rp);
			break;
		case DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS:
			wl_rem_man_participant (dp, wp, rp);
			break;
		case DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS:
			wl_rem_man_topic (wp, rp);
			break;
		default:
			break;
	}
}

/* rl_add -- Add liveliness for a local Reader for the given endpoint and QoS. */

static int rl_add (Domain_t *dp, Reader_t *rp, Endpoint_t *wp, UniQos_t *qp)
{
	unsigned	period;
	int		error;

	dcps_liveliness_change (rp, DLI_ADD, 1, wp->entity.handle);
	if (!liveliness_used (qp))
		return (DDS_RETCODE_OK);

	period = duration2ticks ((const Duration_t *) 
					&qp->liveliness_lease_duration);
	switch (qp->liveliness_kind) {
		case DDS_AUTOMATIC_LIVELINESS_QOS:
			error = rl_add_auto (dp, rp, wp, period);
			break;
		case DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS:
			error = rl_add_man_participant (dp, rp, wp, period);
			break;
		case DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS:
			error = rl_add_man_topic (rp, wp, period);
			break;
		default:
			error = DDS_RETCODE_ERROR;
			break;
	}
	return (error);
}

/* rl_remove -- Remove liveliness for a local Reader for the given endpoint/QoS.*/

static void rl_remove (Domain_t *dp, Reader_t *rp, Endpoint_t *wp, UniQos_t *qp)
{
	if (liveliness_used (qp))
		switch (qp->liveliness_kind) {
			case DDS_AUTOMATIC_LIVELINESS_QOS:
				rl_rem_auto (dp, rp, wp);
				break;
			case DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS:
				rl_rem_man_participant (dp, rp, wp);
				break;
			case DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS:
				rl_rem_man_topic (rp, wp);
				break;
			default:
				break;
		}
	else
		dcps_liveliness_change (rp, DLI_REMOVE, 1, wp->entity.handle);
}

/* liveliness_enable -- Enable liveliness checks on the association between the
		        given endpoints. */

int liveliness_enable (Endpoint_t *wp, Endpoint_t *rp)
{
	Domain_t	*dp;
	int		error;

	if (!liveliness_used (&rp->qos->qos))
		return (DDS_RETCODE_OK);

	if ((wp->entity.flags & EF_LOCAL) != 0) {
		dp = wp->u.publisher->domain;
		error = wl_add (dp, (Writer_t *) wp, rp, &rp->qos->qos);
	}
	else
		error = DDS_RETCODE_OK;
	if (!error) {
		if ((rp->entity.flags & EF_LOCAL) != 0) {
			dp = rp->u.subscriber->domain;
			error = rl_add (dp, (Reader_t *) rp, wp, &rp->qos->qos);
		}
		if (error && (wp->entity.flags & EF_LOCAL) != 0)
			wl_remove (dp, (Writer_t *) wp, rp, &rp->qos->qos);
	}
	return (error);
}

/* liveliness_disable -- Disable the checks that were previously enabled with
		         liveliness_enable(). */

void liveliness_disable (Endpoint_t *wp, Endpoint_t *rp)
{
	Domain_t	*dp;

	if (!liveliness_used (&rp->qos->qos))
		return;

	if ((wp->entity.flags & EF_LOCAL) != 0) {
		dp = wp->u.publisher->domain;
		wl_remove (dp, (Writer_t *) wp, rp, &rp->qos->qos);
	}
	if ((rp->entity.flags & EF_LOCAL) != 0) {
		dp = wp->u.subscriber->domain;
		rl_remove (dp, (Reader_t *) rp, wp, &rp->qos->qos);
	}
}

/* actions_timeout -- Actions timer time-out. */

static void actions_timeout (uintptr_t p)
{
	Guard_t		*gp = (Guard_t *) p;
	Writer_t	*wp = NULL;
	Reader_t	*rp = NULL;
	LocalEndpoint_t	*lep;
	lock_t		*lp;
	Ticks_t		delta_ticks;
	handle_t	w;

	if (gp->writer) {
		wp = (Writer_t *) gp->wep;
		lep = &wp->w_lep;
		lp = &wp->w_lock;
		w = 0;
	}
	else {
		rp = (Reader_t *) gp->rep;
		lep = &rp->r_lep;
		lp = &rp->r_lock;
		if (gp->type == GT_LIFESPAN)
			w = gp->wep->entity.handle;
		else
			w = 0;
	}
	delta_ticks = hc_handle_xqos (lep->cache, gp->type, w, gp->period); 
	if (delta_ticks)
		tmr_start_lock (gp->timer, delta_ticks, (uintptr_t) gp,
							actions_timeout, lp);
	else {
		tmr_free (gp->timer);
		gp->timer = NULL;
		gp->cmode = GM_NONE;
		lep->ep.entity.flags |= EF_LNOTIFY;
	}
}

/* liveliness_restored -- Liveliness of a local endpoint was restored by an
			  endpoint action such as assert_liveliness(), write(),
			  unregister() or dispose(), originated from the given
			  participant. */

void liveliness_restored (LocalEndpoint_t *ep, handle_t wh)
{
	Guard_t		*gp;

	ARG_NOT_USED (wh)

	/* Reset notification flag. */
	ep->ep.entity.flags &= ~EF_LNOTIFY;

	/* Specific handling per type of Liveliness/Deadline. */
	for (gp = ep->guard; gp; gp = gp->enext) {
		if (gp->type == GT_LIVELINESS) {
			if (gp->kind == DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS &&
			    gp->writer)
				wl_manual_restored (gp);
			else if (gp->kind == DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS) {
				if (gp->writer)
					wl_topic_restored (gp);
				else
					rl_topic_restored (gp);
			}
		}
	}
}
 

/* Add/remove Deadline operations. */
/* ------------------------------- */

/* wd_add -- Add deadline for a local Writer for the given endpoint and QoS. */

static int wd_add (Writer_t *wp, Endpoint_t *rp, UniQos_t *qp)
{
	Guard_t		*gp;
	unsigned	period;

	period = duration2ticks ((const Duration_t *) &qp->deadline);
#ifdef LOG_GUARD
	dbg_printf ("wd_add ({%u}->{%u}, %u)\r\n",
			wp->w_handle, rp->entity.handle, period);
#endif
	gp = guard_insert (&wp->w_guard, GT_DEADLINE, 0,
		           1, GM_PERIODIC, period,
			   &wp->w_ep, rp, 
			   actions_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* wd_remove -- Remove deadline for a local Writer for the given endpoint/QoS.*/

static void wd_remove (Writer_t *wp, Endpoint_t *rp)
{
#ifdef LOG_GUARD
	dbg_printf ("wd_remove ({%u}->{%u})\r\n",
			wp->w_handle, rp->entity.handle);
#endif
	guard_extract (&wp->w_guard, GT_DEADLINE, 0, 1, &wp->w_ep, rp);
}

/* rd_add -- Add deadline for a local Reader for the given endpoint and QoS. */

static int rd_add (Reader_t *rp, Endpoint_t *wp, UniQos_t *qp)
{
	Guard_t		*gp;
	unsigned	period;

	period = duration2ticks ((const Duration_t *) &qp->deadline);
#ifdef LOG_GUARD
	dbg_printf ("rd_add ({%u}<-{%u}, %u)\r\n",
			rp->r_handle, wp->entity.handle, period);
#endif
	gp = guard_insert (&rp->r_guard, GT_DEADLINE, 0,
		           0, GM_PERIODIC, period,
			   wp, &rp->r_ep,
			   actions_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* rd_remove -- Remove deadline for a local Reader for the given endpoint/QoS.*/

static void rd_remove (Reader_t *rp, Endpoint_t *wp)
{
#ifdef LOG_GUARD
	dbg_printf ("rd_remove ({%u}<-{%u})\r\n",
			rp->r_handle, wp->entity.handle);
#endif
	guard_extract (&rp->r_guard, GT_DEADLINE, 0, 0, wp, &rp->r_ep);
}

/* deadline_enable -- Enable deadline checks on the association between the two
		      endpoints. */

int deadline_enable (Endpoint_t *wp, Endpoint_t *rp)
{
	int	error;

	if (!deadline_used (&rp->qos->qos))
		return (DDS_RETCODE_OK);

	if ((wp->entity.flags & EF_LOCAL) != 0)
		error = wd_add ((Writer_t *) wp, rp, &rp->qos->qos);
	else
		error = DDS_RETCODE_OK;
	if (!error) {
		if ((rp->entity.flags & EF_LOCAL) != 0)
			error = rd_add ((Reader_t *) rp, wp, &rp->qos->qos);
		if (error && (wp->entity.flags & EF_LOCAL) != 0)
			wd_remove ((Writer_t *) wp, rp);
	}
	return (error);
}


/* deadline_disable -- Disable deadline checks on the association between the
		       two endpoints. */

void deadline_disable (Endpoint_t *wp, Endpoint_t *rp)
{
	if (!deadline_used (&rp->qos->qos))
		return;

	if ((wp->entity.flags & EF_LOCAL) != 0)
		wd_remove ((Writer_t *) wp, rp);
	if ((rp->entity.flags & EF_LOCAL) != 0)
		rd_remove ((Reader_t *) rp, wp);
}

/* action_continue -- Continue with checks of the given type on an endpoint. */

void action_continue (LocalEndpoint_t *ep, GuardType_t type)
{
	Guard_t		*gp;

	for (gp = ep->guard; gp && gp->type != type; gp = gp->enext)
		;

	if (!gp)
		return;

	if (!gp->timer) {
		gp->timer = tmr_alloc ();
		if (!gp->timer)
			return;

		tmr_init (gp->timer, "Guard");
		gp->cmode = gp->mode;
	}
	else if (tmr_active (gp->timer))
		return;

	tmr_start_lock (gp->timer, gp->period,
			(uintptr_t) gp, actions_timeout,
			lock_ptr (gp));
}

/* deadline_continue -- Continue with Deadline checking on an endpoint. */

void deadline_continue (LocalEndpoint_t *ep)
{
	action_continue (ep, GT_DEADLINE);
}


/* Add/remove Lifespan operations. */
/* ------------------------------- */

/* wls_add -- Add lifespan for a local Writer for the given endpoint and QoS. */

static int wls_add (Writer_t *wp, UniQos_t *qp)
{
	Guard_t		*gp;
	unsigned	period;

	period = duration2ticks ((const Duration_t *) &qp->lifespan);
#ifdef LOG_GUARD
	dbg_printf ("wls_add ({%u}, %u)\r\n",
			wp->w_handle, period);
#endif
	gp = guard_insert (&wp->w_guard, GT_LIFESPAN, 0,
		           1, GM_ONE_SHOT, period,
			   &wp->w_ep, NULL, 
			   actions_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* wls_remove -- Remove lifespan for a local Writer for the given endpoint/QoS.*/

static void wls_remove (Writer_t *wp)
{
#ifdef LOG_GUARD
	dbg_printf ("wls_remove ({%u})\r\n",
			wp->w_handle);
#endif
	guard_extract (&wp->w_guard, GT_LIFESPAN, 0, 1, &wp->w_ep, NULL);
}

/* rls_add -- Add lifespan for a local Reader for the given endpoint and QoS. */

static int rls_add (Reader_t *rp, Endpoint_t *wp, UniQos_t *qp)
{
	Guard_t		*gp;
	unsigned	period;

	period = duration2ticks ((const Duration_t *) &qp->lifespan);
#ifdef LOG_GUARD
	dbg_printf ("rls_add ({%u}<-{%u}, %u)\r\n",
			rp->r_handle, wp->entity.handle, period);
#endif
	gp = guard_insert (&rp->r_guard, GT_LIFESPAN, 0,
		           0, GM_ONE_SHOT, period,
			   wp, &rp->r_ep,
			   actions_timeout);
	if (gp)
		hc_reader_lifespan (((Reader_t *) rp)->r_cache, 1);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* rls_remove -- Remove lifespan for a local Reader for the given endpoint/QoS.*/

static void rls_remove (Reader_t *rp, Endpoint_t *wp)
{
#ifdef LOG_GUARD
	dbg_printf ("rls_remove ({%u}<-{%u})\r\n",
			rp->r_handle, wp->entity.handle);
#endif
	guard_extract (&rp->r_guard, GT_LIFESPAN, 0, 0, wp, &rp->r_ep);
	if (!guard_first (rp->r_guard, GT_LIFESPAN, 0, 0))
		hc_reader_lifespan (((Reader_t *) rp)->r_cache, 0);
}

/* lifespan_enable -- Enable lifespan checks on the matching endpoints. */

int lifespan_enable (Endpoint_t *wp, Endpoint_t *rp)
{
	int	error;

	if (!lifespan_used (&wp->qos->qos))
		return (DDS_RETCODE_OK);

	if (!rp && (wp->entity.flags & EF_LOCAL) != 0)
		error = wls_add ((Writer_t *) wp, &wp->qos->qos);
	else
		error = DDS_RETCODE_OK;
	if (!error && rp) {
		if ((rp->entity.flags & EF_LOCAL) != 0)
			error = rls_add ((Reader_t *) rp, wp, &wp->qos->qos);
		if (error && (wp->entity.flags & EF_LOCAL) != 0)
			wls_remove ((Writer_t *) wp);
	}
	return (error);
}

/* lifespan_disable -- Disable lifespan checks on the matching endpoints. */

void lifespan_disable (Endpoint_t *wp, Endpoint_t *rp)
{
	if (!lifespan_used (&wp->qos->qos))
		return;

	if (!rp && (wp->entity.flags & EF_LOCAL) != 0)
		wls_remove ((Writer_t *) wp);
	if (rp && (rp->entity.flags & EF_LOCAL) != 0)
		rls_remove ((Reader_t *) rp, wp);
}

/* lifespan_continue -- Continue with Deadline checking on an endpoint. */

void lifespan_continue (LocalEndpoint_t *ep)
{
	action_continue (ep, GT_LIFESPAN);
}


/* Add/remove Autopurge No-writers operations. */
/* ------------------------------------------- */

/* autopurge_no_writers_enable -- Enable autopurge no-writer checks on reader.*/

int autopurge_no_writers_enable (Reader_t *rp)
{
	Guard_t		*gp;
	unsigned	period;

	if (!autopurge_no_writers_used (rp))
		return (DDS_RETCODE_OK);

	period = duration2ticks ((const Duration_t *) &rp->r_data_lifecycle.
					      autopurge_nowriter_samples_delay);
#ifdef LOG_GUARD
	dbg_printf ("autopurge_nw_add ({%u}, %u)\r\n",
			rp->r_handle, NULL, period);
#endif
	gp = guard_insert (&rp->r_guard, GT_AUTOP_NW, 0,
		           0, GM_ONE_SHOT, period,
			   NULL, &rp->r_ep,
			   actions_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* autopurge_no_writers_disable -- Disable autopurge no-writer checks on reader.*/

void autopurge_no_writers_disable (Reader_t *rp)
{
	if (!autopurge_no_writers_used (rp))
		return;

#ifdef LOG_GUARD
	dbg_printf ("autopurge_nw_remove ({%u})\r\n", rp->r_handle);
#endif
	guard_extract (&rp->r_guard, GT_AUTOP_NW, 0, 0, NULL, &rp->r_ep);
}

/* autopurge_no_writers_continue -- Continue with Deadline checking on an endpoint. */

void autopurge_no_writers_continue (LocalEndpoint_t *ep)
{
	action_continue (ep, GT_AUTOP_NW);
}


/* Add/remove Autopurge Disposed operations. */
/* ----------------------------------------- */

/* autopurge_disposed_enable -- Enable autopurge disposed checks on reader.*/

int autopurge_disposed_enable (Reader_t *rp)
{
	Guard_t		*gp;
	unsigned	period;

	if (!autopurge_disposed_used (rp))
		return (DDS_RETCODE_OK);

	period = duration2ticks ((const Duration_t *) &rp->r_data_lifecycle.
					      autopurge_disposed_samples_delay);
#ifdef LOG_GUARD
	dbg_printf ("autopurge_disp_add ({%u}, %u)\r\n",
			rp->r_handle, NULL, period);
#endif
	gp = guard_insert (&rp->r_guard, GT_AUTOP_DISP, 0,
		           0, GM_ONE_SHOT, period,
			   NULL, &rp->r_ep,
			   actions_timeout);
	return ((gp) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
}

/* autopurge_disposed_disable -- Disable autopurge disposed checks on reader.*/

void autopurge_disposed_disable (Reader_t *rp)
{
	if (!autopurge_disposed_used (rp))
		return;

#ifdef LOG_GUARD
	dbg_printf ("autopurge_disp_remove ({%u})\r\n", rp->r_handle);
#endif
	guard_extract (&rp->r_guard, GT_AUTOP_DISP, 0, 0, NULL, &rp->r_ep);
}

/* autopurge_disposed_continue -- Continue with Autopurge checking on an endpoint. */

void autopurge_disposed_continue (LocalEndpoint_t *ep)
{
	action_continue (ep, GT_AUTOP_DISP);
}


