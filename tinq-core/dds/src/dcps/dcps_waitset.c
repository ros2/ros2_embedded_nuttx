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

/* dcps_waitset.c -- DCPS API - WaitSet functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "sys.h"
#include "log.h"
#include "error.h"
#include "prof.h"
#include "ctrace.h"
#include "str.h"
#include "dds/dds_dcps.h"
#include "dds_data.h"
#include "dds.h"
#include "rtps.h"
#include "dcps_priv.h"
#include "dcps_event.h"
#include "dcps_waitset.h"

DDS_ConditionSeq *DDS_ConditionSeq__alloc (void)
{
	DDS_ConditionSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_ConditionSeq));
	if (!p)
		return (NULL);

	DDS_ConditionSeq__init (p);
	return (p);
}

void DDS_ConditionSeq__free (DDS_ConditionSeq *conditions)
{
	if (conditions->_length)
		DDS_ConditionSeq__clear (conditions);
	mm_fcts.free_ (conditions);
}

void DDS_ConditionSeq__init (DDS_ConditionSeq *conditions)
{
	DDS_SEQ_INIT (*conditions);
}

void DDS_ConditionSeq__clear (DDS_ConditionSeq *conditions)
{
	dds_seq_cleanup (conditions);
}

DDS_WaitSet DDS_WaitSet__alloc (void)
{
	WaitSet_t	*wp;

	ctrc_printd (DCPS_ID, DCPS_WS_ALLOC, NULL, 0);
	wp = mds_pool_alloc (&dcps_mem_blocks [MB_WAITSET]);
	if (!wp)
		return (NULL);

	wp->nconditions = 0;
	wp->first = wp->last = NULL;
	lock_init_nr (wp->lock, "WaitSet");
	cond_init (wp->wakeup);
	wp->nwaiting = 0;
	return (wp);
}

void DDS_WaitSet__free (DDS_WaitSet wp)
{
	ctrc_printd (DCPS_ID, DCPS_WS_FREE, &wp, sizeof (wp));
	if (!wp || wp->nwaiting || wp->nconditions)
		return;

	mds_pool_free (&dcps_mem_blocks [MB_WAITSET], wp);
}

static int check_status_condition (StatusCondition_t *scp)
{
	Topic_t		*tp;
	Subscriber_t	*sp;
	Writer_t	*wp;
	Reader_t	*rp;
	int		ready = 0;

	switch (scp->entity->type) {
		case ET_TOPIC:
			tp = (Topic_t *) scp->entity;
			if ((tp->mask & scp->enabled) != 0)
				ready = 1;
			break;
		case ET_SUBSCRIBER:
			sp = (Subscriber_t *) scp->entity;
			if ((sp->mask & scp->enabled) != 0)
				ready = 1;
			break;
		case ET_WRITER:
			wp = (Writer_t *) scp->entity;
			if ((wp->w_mask & scp->enabled) != 0)
				ready = 1;
			break;
		case ET_READER:
			rp = (Reader_t *) scp->entity;
			if ((rp->r_mask & scp->enabled) != 0)
				ready = 1;
			break;
		case ET_PARTICIPANT:
		case ET_PUBLISHER:
		default:
			break;
	}
	return (ready);
}

static int check_read_condition (ReadCondition_t *rcp, lock_t *no_lock)
{
	unsigned	skip;
	int		ready;

	if (&rcp->rp->r_lock != no_lock)
		lock_take (rcp->rp->r_lock);
	skip = dcps_skip_mask (rcp->sample_states,
			       rcp->view_states,
			       rcp->instance_states);
	ready = hc_avail (rcp->rp->r_cache, skip);
	if (&rcp->rp->r_lock != no_lock)
		lock_release (rcp->rp->r_lock);
	return (ready);
}

static int check_query_condition (QueryCondition_t *qcp, lock_t *no_lock)
{
	unsigned	skip;
	int		ready;

	if (&qcp->rc.rp->r_lock != no_lock)
		lock_take (qcp->rc.rp->r_lock);
	skip = dcps_skip_mask (qcp->rc.sample_states,
			       qcp->rc.view_states,
			       qcp->rc.instance_states);
	ready = hc_avail_condition (qcp->rc.rp->r_cache,
				    skip,
				    qcp->expression_pars,
				    &qcp->filter,
				    &qcp->cache);
	if (&qcp->rc.rp->r_lock != no_lock)
		lock_release (qcp->rc.rp->r_lock);
	return (ready);
}

static int waitset_check_condition (Condition_t *cp, lock_t *no_lock)
{
	ReadCondition_t		*rcp;
	QueryCondition_t	*qcp;
	StatusCondition_t	*scp;
	GuardCondition_t	*gcp;
	int			ready = 0;

	switch (cp->class) {
		case CC_STATUS:
			scp = (StatusCondition_t *) cp;
			ready = check_status_condition (scp);
			break;

		case CC_READ:
			rcp = (ReadCondition_t *) cp;
			ready = check_read_condition (rcp, no_lock);
			break;

		case CC_QUERY:
			qcp = (QueryCondition_t *) cp;
			ready = check_query_condition (qcp, no_lock);
			break;

		case CC_GUARD:
			gcp = (GuardCondition_t *) cp;
			if (gcp->value)
				ready = 1;
			break;

		default:
			break;
	}
	return (ready);
}

int DDS_Condition_get_trigger_value (DDS_Condition c)
{
	Condition_t	*cp = (Condition_t *) c;

	if (!cp)
		return (0);

	return (waitset_check_condition (cp, NULL));
}

/* waitset_signal -- Waitset lock taken, signal the blocked waitset client if
		     possible. */

static void waitset_signal (Condition_t *cp, lock_t *no_lock)
{
	if (cp->waitset->nwaiting &&
	    waitset_check_condition (cp, no_lock)) {
		if (cp->waitset->nwaiting > 1)
			cond_signal_all (cp->waitset->wakeup);
		else
			cond_signal (cp->waitset->wakeup);
	}
}

/* dcps_waitset_check -- Check if a condition is ready, and signal a blocked
			 WaitSet if possible. */

void dcps_waitset_check (void *c)
{
	Condition_t	*cp = (Condition_t *) c;

	lock_take (cp->waitset->lock);
	waitset_signal (cp, NULL);
	lock_release (cp->waitset->lock);
}

/* dcps_deferred_waitset_check -- A deferred condition check should be retried. */

void dcps_deferred_waitset_check (void *e, void *cond)
{
	Entity_t	*ep = (Entity_t *) e;
	Domain_t	*dp;
	Topic_t		*tp;
	Publisher_t	*pp;
	Subscriber_t	*sp;
	Writer_t	*wp;
	Reader_t	*rp;
	Condition_t	*cp;
	lock_t		*lock;

	if (!ep || (ep->flags & EF_LOCAL) == 0)
		return;

	switch (ep->type) {
		case ET_PARTICIPANT:
			dp = (Domain_t *) e;
			cp = dp->condition;
			lock = &dp->lock;
			break;
		case ET_TOPIC:
			tp = (Topic_t *) e;
			cp = tp->condition;
			lock = &tp->lock;
			break;
		case ET_PUBLISHER:
			pp = (Publisher_t *) e;
			cp = pp->condition;
			lock = &pp->domain->lock;
			break;
		case ET_SUBSCRIBER:
			sp = (Subscriber_t *) e;
			cp = sp->condition;
			lock = &sp->domain->lock;
			break;
		case ET_WRITER:
			wp = (Writer_t *) e;
			cp = wp->w_condition;
			lock = &wp->w_lock;
			break;
		case ET_READER:
			rp = (Reader_t *) e;
			cp = rp->r_conditions;
			lock = &rp->r_lock;
			break;
		default:
			return;
	}

	/* Safe to take lock since the condition is automatically purged
	   when deleted from an entity, so its parent must still be alive. */
	lock_take (*lock);
	while (cp && cp != (Condition_t *) cond)
		cp = cp->e_next;
	if (cp) {
		if (!cp->waitset)
			cp->deferred = 0;
		else if (!lock_try (cp->waitset->lock))
			dds_defer_waitset_check (e, cp);
		else {
			cp->deferred = 0;
			waitset_signal (cp, lock);
			lock_release (cp->waitset->lock);
		}
	}
	lock_release (*lock);
	return;
}

/*#define ALWAYS_DEFER*/

/* dcps_waitset_wakeup -- Try to wakeup a blocked WaitSet_wait(). The no_lock
		          parameter is a lock already held, if non-NULL. */

void dcps_waitset_wakeup (void *ep, Condition_t *cp, lock_t *no_lock)
{
	for (; cp; cp = cp->e_next)
		if (cp->waitset) {
			if (no_lock) {
#ifndef ALWAYS_DEFER
				if (!lock_try (cp->waitset->lock)) {
#endif
					cp->deferred = 1;
					dds_defer_waitset_check (ep, cp);
#ifndef ALWAYS_DEFER
				}
				else {
					waitset_signal (cp, no_lock);
					lock_release (cp->waitset->lock);
				}
#endif
			}
			else	/* Safe to block now. */
				dcps_waitset_check (cp);
		}
}

static unsigned waitset_check_ready (Condition_t *cp,
				     DDS_ConditionSeq *conditions,
				     DDS_ReturnCode_t *ret)
{
	unsigned	n = 0;

	*ret = DDS_RETCODE_OK;
	for (; cp; cp = cp->next) {
		if (waitset_check_condition (cp, NULL)) {
			*ret = dds_seq_append (conditions, &cp);
			if (*ret)
				return (0);

			n++;
		}
	}
	return (n);
}

DDS_ReturnCode_t DDS_WaitSet_wait (DDS_WaitSet wp,
				   DDS_ConditionSeq *conditions,
				   DDS_Duration_t *timeout)
{
#ifdef THREADS_USED
	unsigned	n;
	int		r;
	struct timespec	ts;
#else
	Ticks_t		d, now, end_time;	/* *10ms */
#endif
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_WS_WAIT, &wp, sizeof (wp));
	ctrc_contd (&conditions, sizeof (conditions));
	ctrc_contd (timeout, sizeof (DDS_Duration_t));
	ctrc_endd ();

	prof_start (dcps_ws_wait);

	if (!wp || !conditions || !timeout)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!wp->first)
		return (DDS_RETCODE_OK);

	/* User needs to do this : DDS_SEQ_INIT (*conditions); !! */
	conditions->_length = 0;

#ifdef THREADS_USED
	duration2timespec (timeout, &ts);
	if (lock_take (wp->lock))
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((n = waitset_check_ready (wp->first, conditions, &ret)) != 0) {
		lock_release (wp->lock);
		prof_stop (dcps_ws_wait, 1);
		return (ret);
	}
	wp->nwaiting++;
	do {
		if (ts.tv_sec || ts.tv_nsec)
			r = cond_wait_to (wp->wakeup, wp->lock, ts);
		else
			r = cond_wait (wp->wakeup, wp->lock);
		if (!r && !waitset_check_ready (wp->first, conditions, &ret) && ret)
			break;
	}
	while (!conditions->_length && !r);
	wp->nwaiting--;
	lock_release (wp->lock);
	if (ret)
		return (ret);

	if (!r && conditions->_length) {
		prof_stop (dcps_ws_wait, 1);
		return (DDS_RETCODE_OK);
	}
#else
	if (dds_listener_state)
		return (DDS_RETCODE_TIMEOUT);

	now = sys_getticks ();
	end_time = now + duration2ticks ((Duration_t *) timeout);
	for (;;) {
		d = end_time - now;
		if (d >= 0x7fffffffUL)
			break;

		DDS_schedule (d * TMR_UNIT_MS);
		if (!waitset_check_ready (wp->first, conditions, &ret) && ret)
			break;

		if (conditions->_length)
			return (DDS_RETCODE_OK);

		now = sys_getticks ();
	}
#endif
	prof_stop (dcps_ws_wait, 1);
	return (DDS_RETCODE_TIMEOUT);
}

DDS_ReturnCode_t DDS_WaitSet_attach_condition (DDS_WaitSet   wp,
					       void          *c)
{
	Condition_t	*cp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_WS_ATT_COND, &wp, sizeof (wp));
	ctrc_contd (&c, sizeof (c));
	ctrc_endd ();

	if (!wp || !c)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (lock_take (wp->lock))
		return (DDS_RETCODE_BAD_PARAMETER);

	for (cp = wp->first; cp; cp = cp->next)
		if (cp == (Condition_t *) c) {
			ret = DDS_RETCODE_PRECONDITION_NOT_MET;
			goto done;
		}

	cp = (Condition_t *) c;
	if (cp->waitset) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (wp->first)
		wp->last->next = cp;
	else
		wp->first = cp;
	wp->last = cp;
	wp->nconditions++;
	cp->waitset = wp;
	cp->next = NULL;
	ret = DDS_RETCODE_OK;

    done:
	lock_release (wp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_WaitSet_detach_condition (DDS_WaitSet   wp,
					       DDS_Condition c)
{
	Condition_t	*cp, *prev_cp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_WS_DET_COND, &wp, sizeof (wp));
	ctrc_contd (&c, sizeof (c));
	ctrc_endd ();

	cp = (Condition_t *) c;
	if (!wp || !cp || cp->waitset != wp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (lock_take (wp->lock))
		return (DDS_RETCODE_BAD_PARAMETER);

	for (cp = wp->first, prev_cp = NULL; cp; prev_cp = cp, cp = cp->next)
		if (cp == (Condition_t *) c)
			break;

	if (!cp) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (prev_cp)
		prev_cp->next = cp->next;
	else
		wp->first = cp->next;
	if (!cp->next)
		wp->last = cp;
	wp->nconditions--;
	cp->waitset = NULL;
	ret = DDS_RETCODE_OK;

    done:
	lock_release (wp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_WaitSet_get_conditions (DDS_WaitSet      wp,
					     DDS_ConditionSeq *conditions)
{
	Condition_t	*cp;
	unsigned	i;

	ctrc_begind (DCPS_ID, DCPS_WS_G_CONDS, &wp, sizeof (wp));
	ctrc_contd (&conditions, sizeof (conditions));
	ctrc_endd ();

	if (!wp || !conditions)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_SEQ_INIT (*conditions);
	if (lock_take (wp->lock))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dds_seq_require (conditions, wp->nconditions)) {
		lock_release (wp->lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	for (i = 0, cp = wp->first; i < wp->nconditions; i++, cp = cp->next)
		conditions->_buffer [i] = cp;
	lock_release (wp->lock);
	return (DDS_RETCODE_OK);
}

DDS_GuardCondition DDS_GuardCondition__alloc (void)
{
	GuardCondition_t	*gc;

	ctrc_printd (DCPS_ID, DCPS_GC_ALLOC, NULL, 0);
	gc = mds_pool_alloc (&dcps_mem_blocks [MB_GUARD_COND]);
	if (!gc)
		return (NULL);

	gc->c.waitset = NULL;
	gc->c.class = CC_GUARD;
	gc->c.deferred = 0;
	gc->value = 0;
	return (gc);
}

void DDS_GuardCondition__free (DDS_GuardCondition gc)
{
	ctrc_printd (DCPS_ID, DCPS_GC_FREE, &gc, sizeof (gc));
	if (!gc || gc->c.class != CC_GUARD)
		return;

	mds_pool_free (&dcps_mem_blocks [MB_GUARD_COND], gc);
}

int DDS_GuardCondition_get_trigger_value (DDS_GuardCondition gc)
{
	ctrc_printd (DCPS_ID, DCPS_GC_G_TRIG, &gc, sizeof (gc));
	if (!gc || gc->c.class != CC_GUARD)
		return (0);

	return (gc->value);
}

DDS_ReturnCode_t DDS_GuardCondition_set_trigger_value (DDS_GuardCondition gc,
						       int                value)
{
	ctrc_begind (DCPS_ID, DCPS_GC_S_TRIG, &gc, sizeof (gc));
	ctrc_contd (&value, sizeof (value));
	ctrc_endd ();

	if (!gc || gc->c.class != CC_GUARD)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!gc->value && value) {
		gc->value = 1;
		if (gc->c.waitset && 
		    gc->c.waitset->nwaiting) {
			if (gc->c.waitset->nwaiting > 1)
				cond_signal_all (gc->c.waitset->wakeup);
			else
				cond_signal (gc->c.waitset->wakeup);
		}
	}
	else if (!value)
		gc->value = 0;
	return (DDS_RETCODE_OK);
}

int DDS_StatusCondition_get_trigger_value (DDS_StatusCondition scp)
{
	ctrc_printd (DCPS_ID, DCPS_SC_G_TRIG, &scp, sizeof (scp));

	if (!scp || scp->c.class != CC_STATUS)
		return (0);

	return (check_status_condition (scp));
}

DDS_ReturnCode_t DDS_StatusCondition_set_enabled_statuses (DDS_StatusCondition scp,
							   DDS_StatusMask mask)
{
	ctrc_begind (DCPS_ID, DCPS_SC_S_STAT, &scp, sizeof (scp));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	if (!scp || scp->c.class != CC_STATUS)
		return (DDS_RETCODE_BAD_PARAMETER);

	scp->enabled = mask;
	if (check_status_condition (scp) &&
	    scp->c.waitset &&
	    scp->c.waitset->nwaiting) {
		if (scp->c.waitset->nwaiting > 1)
			cond_signal_all (scp->c.waitset->wakeup);
		else
			cond_signal (scp->c.waitset->wakeup);
	}
	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_StatusCondition_get_enabled_statuses (DDS_StatusCondition scp)
{
	ctrc_printd (DCPS_ID, DCPS_SC_G_STAT, &scp, sizeof (scp));

	if (!scp || scp->c.class != CC_STATUS)
		return (0);

	return (scp->enabled);
}

DDS_Entity DDS_StatusCondition_get_entity (DDS_StatusCondition scp)
{
	ctrc_printd (DCPS_ID, DCPS_SC_G_ENT, &scp, sizeof (scp));

	if (!scp || scp->c.class != CC_STATUS)
		return (NULL);

	return (scp->entity);
}


int DDS_ReadCondition_get_trigger_value (DDS_ReadCondition rcp)
{
	ctrc_printd (DCPS_ID, DCPS_RC_G_TRIG, &rcp, sizeof (rcp));

	if (!rcp || rcp->c.class != CC_READ)
		return (0);

	return (check_read_condition (rcp, NULL));
}

DDS_DataReader DDS_ReadCondition_get_datareader (DDS_ReadCondition rcp)
{
	ctrc_printd (DCPS_ID, DCPS_RC_G_READ, &rcp, sizeof (rcp));

	if (!rcp || rcp->c.class != CC_READ)
		return (0);

	return (rcp->rp);
}

DDS_ViewStateMask DDS_ReadCondition_get_view_state_mask (DDS_ReadCondition rcp)
{
	ctrc_printd (DCPS_ID, DCPS_RC_G_VMASK, &rcp, sizeof (rcp));

	if (!rcp || rcp->c.class != CC_READ)
		return (0);

	return (rcp->view_states);
}

DDS_InstanceStateMask DDS_ReadCondition_get_instance_state_mask (DDS_ReadCondition rcp)
{
	ctrc_printd (DCPS_ID, DCPS_RC_G_IMASK, &rcp, sizeof (rcp));

	if (!rcp || rcp->c.class != CC_READ)
		return (0);

	return (rcp->instance_states);
}

DDS_SampleStateMask DDS_ReadCondition_get_sample_state_mask (DDS_ReadCondition rcp)
{
	ctrc_printd (DCPS_ID, DCPS_RC_G_SMASK, &rcp, sizeof (rcp));

	if (!rcp || rcp->c.class != CC_READ)
		return (0);

	return (rcp->sample_states);
}


int DDS_QueryCondition_get_trigger_value (DDS_QueryCondition qcp)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_TRIG, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (0);

	return (check_query_condition (qcp, NULL));
}

DDS_ReturnCode_t DDS_QueryCondition_set_query_parameters (DDS_QueryCondition qcp,
							  DDS_StringSeq *pars)
{
	DDS_ReturnCode_t	rc;

	ctrc_printd (DCPS_ID, DCPS_QC_S_PARS, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!pars || qcp->filter.npars > DDS_SEQ_LENGTH (*pars))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (lock_take (qcp->rc.rp->r_lock))
		return (DDS_RETCODE_BAD_PARAMETER);

	rc = dcps_update_str_pars (&qcp->expression_pars, pars);
	bc_cache_reset (qcp->cache);
	lock_release (qcp->rc.rp->r_lock);
	return (rc);
}

DDS_ReturnCode_t DDS_QueryCondition_get_query_parameters (DDS_QueryCondition qcp,
							  DDS_StringSeq *pars)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_PARS, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY || !pars)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_SEQ_INIT (*pars);
	if (qcp->expression_pars) {
		if (lock_take (qcp->rc.rp->r_lock))
			return (DDS_RETCODE_BAD_PARAMETER);

		dcps_get_str_pars (pars, qcp->expression_pars);
		lock_release (qcp->rc.rp->r_lock);
	}
	return (DDS_RETCODE_OK);
}

const char *DDS_QueryCondition_get_query_expression (DDS_QueryCondition qcp)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_EXPR, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (0);

	return (str_ptr (qcp->expression));
}

DDS_ViewStateMask DDS_QueryCondition_get_view_state_mask (DDS_QueryCondition qcp)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_VMASK, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (0);

	return (qcp->rc.view_states);
}

DDS_DataReader DDS_QueryCondition_get_datareader (DDS_QueryCondition qcp)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_READ, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (0);

	return (qcp->rc.rp);
}

DDS_SampleStateMask DDS_QueryCondition_get_sample_state_mask (DDS_QueryCondition qcp)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_SMASK, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (0);

	return (qcp->rc.sample_states);
}

DDS_InstanceStateMask DDS_QueryCondition_get_instance_state_mask (DDS_QueryCondition qcp)
{
	ctrc_printd (DCPS_ID, DCPS_QC_G_IMASK, &qcp, sizeof (qcp));

	if (!qcp || qcp->rc.c.class != CC_QUERY)
		return (0);

	return (qcp->rc.instance_states);
}



