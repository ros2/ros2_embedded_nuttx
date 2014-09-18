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

/* dcps_event.c -- Implements the DCPS event handling functions. */

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
#include "dcps_waitset.h"
#include "dcps_event.h"

#ifdef RTPS_USED

/* dcps_notify_match -- Used by Discovery to inform DCPS on the status of
			matched endpoints. */

int dcps_notify_match (LocalEndpoint_t *lep, const Endpoint_t *ep)
{
	Writer_t	*wp;
	Reader_t	*rp;
	int		error;

	ARG_NOT_USED (ep)

	if (lep->ep.entity.type == ET_WRITER) {
		wp = (Writer_t *) lep;
		if (!wp->w_rtps && (ep->entity.flags & EF_REMOTE) != 0) {

			/* Create RTPS writer. */
			error = rtps_writer_create (wp,
						    1, 1,
						    NULL, NULL, NULL, NULL);
			if (error) {
				warn_printf ("dcps_notify_match: can't create RTPS writer.\r\n");
				return (0);
			}
			/*rtps_endpoint_markers_set (&wp->w_lep, 1 << EM_SEND);*/
		}
		return (1);
	}
	else {
		rp = (Reader_t *) lep;
		if (!rp->r_rtps && (ep->entity.flags & EF_REMOTE) != 0) {

			/* Create the RTPS reader. */
			error = rtps_reader_create (rp,
						    1, 
						    NULL, NULL);
			if (error) {
				warn_printf ("dcps_notify_match: can't create RTPS reader.\r\n");
				return (0);
			}
		}
		return (1);
	}
}

/* dcps_notify_unmatch -- Used by Discovery to inform DCPS on the status of
			  unmatched endpoints. */

int dcps_notify_unmatch (LocalEndpoint_t *lep, const Endpoint_t *ep)
{
	Writer_t	*wp;
	Reader_t	*rp;

	ARG_NOT_USED (ep)

	if (lep->ep.entity.type == ET_WRITER) {
		wp = (Writer_t *) lep;
		return (wp->w_rtps &&
		        (ep->entity.flags & EF_REMOTE) != 0 &&
			rtps_matched_reader_count (wp) == 1);
	}
	else {
		rp = (Reader_t *) lep;
		return (rp->r_rtps &&
			(ep->entity.flags & EF_REMOTE) != 0 &&
			rtps_matched_writer_count (rp) == 1);
	}
}

/* dcps_notify_done -- Used by Discovery to inform DCPS that the last proxy of
		       an RTPS endpoint was removed.

   Locks: On entry, the endpoint, its publisher/subscriber, topic and domain
   should be locked (inherited from rtps_writer_delete/rtps_reader_delete) */

void dcps_notify_done (LocalEndpoint_t *lep)
{
	if (lep->ep.entity.type == ET_WRITER)
		rtps_writer_delete ((Writer_t *) lep);
	else
		rtps_reader_delete ((Reader_t *) lep);
}

#endif

/* dcps_inconsistent_topic -- Notify an inconsistent topic. */

void dcps_inconsistent_topic (Topic_t *tp)
{
	/*dbg_printf ("DCPS: Inconsistent topic (%s)!\r\n", str_ptr (tp->name));*/
	if ((tp->entity.flags & EF_ENABLED) == 0)
		return;

	tp->status |= DDS_INCONSISTENT_TOPIC_STATUS;
	tp->inc_status.total_count++;
	if (!tp->inc_status.total_count_change++ &&
	    ((tp->listener.on_inconsistent_topic &&
	      (tp->mask & DDS_INCONSISTENT_TOPIC_STATUS) != 0) ||
	     (tp->domain->listener.on_inconsistent_topic &&
	      (tp->domain->mask & DDS_INCONSISTENT_TOPIC_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) tp, NT_INCONSISTENT_TOPIC);
	else if (tp->condition)
		dcps_waitset_wakeup (tp, tp->condition, &tp->lock);
		
}

/* dcps_notify_inconsistent_topic -- Call inconsistent topic listener. */

static void dcps_notify_inconsistent_topic (Topic_t *tp)
{
	DDS_InconsistentTopicStatus	st;

	if (lock_take (tp->lock))
		return;

	st = tp->inc_status;
	if (tp->listener.on_inconsistent_topic &&
	    (tp->mask & DDS_INCONSISTENT_TOPIC_STATUS) != 0) {
		DDS_TopicListener_on_inconsistent_topic oit =
			tp->listener.on_inconsistent_topic;
		tp->inc_status.total_count_change = 0;
		tp->status &= ~DDS_INCONSISTENT_TOPIC_STATUS;
		lock_release (tp->lock);
		(*oit) (&tp->listener, (DDS_Topic) tp, &st);
	}
	else if (tp->domain->listener.on_inconsistent_topic &&
	         (tp->domain->mask & DDS_INCONSISTENT_TOPIC_STATUS) != 0) {
		DDS_DomainParticipantListener_on_inconsistent_topic oit =
			tp->domain->listener.on_inconsistent_topic;
		tp->inc_status.total_count_change = 0;
		tp->status &= ~DDS_INCONSISTENT_TOPIC_STATUS;
		lock_release (tp->lock);
		(*oit) (&tp->domain->listener, (DDS_Topic) tp, &st);
	}
	else
		lock_release (tp->lock);

	if (tp->condition)
		dcps_waitset_wakeup (tp, tp->condition, NULL);
}

/* DDS_Topic_get_inconsistent_topic_status -- Retrieve inconsistent topic status. */

DDS_ReturnCode_t DDS_Topic_get_inconsistent_topic_status (DDS_Topic tp,
							  DDS_InconsistentTopicStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_T_G_INC_ST, &tp, sizeof (tp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!topic_ptr (tp, 1, &ret))
		return (ret);

	*st = tp->inc_status;
	tp->inc_status.total_count_change = 0;
	tp->status &= ~DDS_INCONSISTENT_TOPIC_STATUS;
	lock_release (tp->lock);
	return (DDS_RETCODE_OK);
}

/* dcps_offered_deadline_missed -- Notify an Offered deadline missed event. */

void dcps_offered_deadline_missed (Writer_t *wp, DDS_InstanceHandle_t handle)
{
	if ((wp->w_flags & EF_ENABLED) == 0)
		return;

	wp->w_status |= DDS_OFFERED_DEADLINE_MISSED_STATUS;
	wp->w_odm_status.total_count++;
	wp->w_odm_status.last_instance_handle = handle;
	if (!wp->w_odm_status.total_count_change++ &&
	    ((wp->w_listener.on_offered_deadline_missed && 
	      (wp->w_mask & DDS_OFFERED_DEADLINE_MISSED_STATUS) != 0) ||
	     (wp->w_publisher->listener.on_offered_deadline_missed && 
	      (wp->w_publisher->mask & DDS_OFFERED_DEADLINE_MISSED_STATUS) != 0) ||
	     (wp->w_publisher->domain->listener.on_offered_deadline_missed && 
	      (wp->w_publisher->domain->mask & DDS_OFFERED_DEADLINE_MISSED_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) wp, NT_OFFERED_DEADLINE_MISSED);
	else if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, &wp->w_lock);
}

/* dcps_notify_offered_deadline_missed -- Call offered deadline missed listener. */

static void dcps_notify_offered_deadline_missed (Writer_t *wp)
{
	DDS_OfferedDeadlineMissedStatus	st;

	if (lock_take (wp->w_lock))
		return;

	st = wp->w_odm_status;
	if (wp->w_listener.on_offered_deadline_missed && 
	      (wp->w_mask & DDS_OFFERED_DEADLINE_MISSED_STATUS) != 0) {
		DDS_DataWriterListener_on_offered_deadline_missed oodm =
			wp->w_listener.on_offered_deadline_missed;
		wp->w_odm_status.total_count_change = 0;
		wp->w_status &= ~DDS_OFFERED_DEADLINE_MISSED_STATUS;
		lock_release (wp->w_lock);
		(*oodm) (&wp->w_listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->listener.on_offered_deadline_missed && 
	      (wp->w_publisher->mask & DDS_OFFERED_DEADLINE_MISSED_STATUS) != 0) {
		DDS_PublisherListener_on_offered_deadline_missed oodm =
			wp->w_publisher->listener.on_offered_deadline_missed;
		wp->w_odm_status.total_count_change = 0;
		wp->w_status &= ~DDS_OFFERED_DEADLINE_MISSED_STATUS;
		lock_release (wp->w_lock);
		(*oodm) (&wp->w_publisher->listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->domain->listener.on_offered_deadline_missed && 
	         (wp->w_publisher->domain->mask & DDS_OFFERED_DEADLINE_MISSED_STATUS) != 0) {
		DDS_DomainParticipantListener_on_offered_deadline_missed oodm =
			wp->w_publisher->domain->listener.on_offered_deadline_missed;
		wp->w_odm_status.total_count_change = 0;
		wp->w_status &= ~DDS_OFFERED_DEADLINE_MISSED_STATUS;
		lock_release (wp->w_lock);
		(*oodm) (&wp->w_publisher->domain->listener, (DDS_DataWriter) wp, &st);
	}
	else
		lock_release (wp->w_lock);

	if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, NULL);
}

DDS_ReturnCode_t DDS_DataWriter_get_offered_deadline_missed_status (DDS_DataWriter w,
						   DDS_OfferedDeadlineMissedStatus *st)
{
	Writer_t		*wp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_DLM_ST, &w, sizeof (w));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	wp = writer_ptr (w, 1, &ret);
	if (!wp)
		return (ret);

	*st = wp->w_odm_status;
	wp->w_odm_status.total_count_change = 0;
	wp->w_status &= ~DDS_OFFERED_DEADLINE_MISSED_STATUS;
	lock_release (wp->w_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_requested_deadline_missed -- Notify a Requested deadline missed event. */

void dcps_requested_deadline_missed (Reader_t *rp, DDS_InstanceHandle_t handle)
{
	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	rp->r_status |= DDS_REQUESTED_DEADLINE_MISSED_STATUS;
	rp->r_rdm_status.total_count++;
	rp->r_rdm_status.last_instance_handle = handle;
	if (!rp->r_rdm_status.total_count_change++ &&
	    ((rp->r_listener.on_requested_deadline_missed && 
	      (rp->r_mask & DDS_REQUESTED_DEADLINE_MISSED_STATUS) != 0) ||
	     (rp->r_subscriber->listener.on_requested_deadline_missed && 
	      (rp->r_subscriber->mask & DDS_REQUESTED_DEADLINE_MISSED_STATUS) != 0) ||
	     (rp->r_subscriber->domain->listener.on_requested_deadline_missed && 
	      (rp->r_subscriber->domain->mask & DDS_REQUESTED_DEADLINE_MISSED_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_REQUESTED_DEADLINE_MISSED);
	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
}

/* dcps_notify_requested_deadline_missed -- Call requested deadline missed listener. */

static void dcps_notify_requested_deadline_missed (Reader_t *rp)
{
	DDS_RequestedDeadlineMissedStatus	st;

	if (lock_take (rp->r_lock))
		return;

	st = rp->r_rdm_status;
	if (rp->r_listener.on_requested_deadline_missed && 
	    (rp->r_mask & DDS_REQUESTED_DEADLINE_MISSED_STATUS) != 0) {
		DDS_DataReaderListener_on_requested_deadline_missed ordm =
			rp->r_listener.on_requested_deadline_missed;
		rp->r_rdm_status.total_count_change = 0;
		rp->r_status &= ~DDS_REQUESTED_DEADLINE_MISSED_STATUS;
		lock_release (rp->r_lock);
		(*ordm) (&rp->r_listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->listener.on_requested_deadline_missed && 
	         (rp->r_subscriber->mask & DDS_REQUESTED_DEADLINE_MISSED_STATUS) != 0) {
		DDS_SubscriberListener_on_requested_deadline_missed ordm =
			rp->r_subscriber->listener.on_requested_deadline_missed;
		rp->r_rdm_status.total_count_change = 0;
		rp->r_status &= ~DDS_REQUESTED_DEADLINE_MISSED_STATUS;
		lock_release (rp->r_lock);
		(*ordm) (&rp->r_subscriber->listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->domain->listener.on_requested_deadline_missed && 
	         (rp->r_subscriber->domain->mask & DDS_REQUESTED_DEADLINE_MISSED_STATUS) != 0) {
		DDS_DomainParticipantListener_on_requested_deadline_missed ordm =
			rp->r_subscriber->domain->listener.on_requested_deadline_missed;
		rp->r_rdm_status.total_count_change = 0;
		rp->r_status &= ~DDS_REQUESTED_DEADLINE_MISSED_STATUS;
		lock_release (rp->r_lock);
		(*ordm) (&rp->r_subscriber->domain->listener, (DDS_DataReader) rp, &st);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

DDS_ReturnCode_t DDS_DataReader_get_requested_deadline_missed_status (DDS_DataReader rp,
						   DDS_RequestedDeadlineMissedStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_RDM_ST, &rp, sizeof (rp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	*st = rp->r_rdm_status;
	rp->r_rdm_status.total_count_change = 0;
	rp->r_status &= ~DDS_REQUESTED_DEADLINE_MISSED_STATUS;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

/* policy_id_add -- Increment the counter of a policy_id. */

static void policy_id_add (IncompatibleQosStatus *sp, DDS_QosPolicyId_t id)
{
	unsigned		i;
	DDS_QosPolicyCount	*p;

	for (i = 0; i < sp->n_policies; i++)
		if (sp->policies [i].policy_id == id) {
			sp->policies [i].count++;
			return;
		}
	p = xrealloc (sp->policies, sizeof (DDS_QosPolicyCount) * (sp->n_policies + 1));
	if (!p)
		return;

	sp->policies = p;
	sp->policies [sp->n_policies].policy_id = id;
	sp->policies [sp->n_policies++].count = 1;
}

/* dcps_offered_incompatible_qos -- Notify Incompatible QoS offered event. */

void dcps_offered_incompatible_qos (Writer_t *wp, DDS_QosPolicyId_t policy_id)
{
	if ((wp->w_flags & EF_ENABLED) == 0)
		return;

	wp->w_status |= DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
	wp->w_oiq_status.total_count++;
	wp->w_oiq_status.last_policy_id = policy_id;
	policy_id_add (&wp->w_oiq_status, policy_id);
	if (!wp->w_oiq_status.total_count_change++ &&
	    ((wp->w_listener.on_offered_incompatible_qos &&
	      (wp->w_mask & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) != 0) ||
	     (wp->w_publisher->listener.on_offered_incompatible_qos &&
	      (wp->w_publisher->mask & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) != 0) ||
	     (wp->w_publisher->domain->listener.on_offered_incompatible_qos &&
	      (wp->w_publisher->domain->mask & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) wp, NT_OFFERED_INCOMPATIBLE_QOS);
	else if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, &wp->w_lock);
}

static void inc_qos_status_get (DDS_OfferedIncompatibleQosStatus *st,
				IncompatibleQosStatus            *iq)
{
	st->total_count = iq->total_count;
	st->total_count_change = iq->total_count_change;
	st->last_policy_id = iq->last_policy_id;
	st->policies._buffer = iq->policies;
	st->policies._maximum = st->policies._length = iq->n_policies;
	st->policies._esize = sizeof (DDS_QosPolicyCount);
	st->policies._own = 0;
}

/* dcps_notify_offered_incompatible_qos -- Call offered incompatible qos listener. */

static void dcps_notify_offered_incompatible_qos (Writer_t *wp)
{
	DDS_OfferedIncompatibleQosStatus	st;

	if (lock_take (wp->w_lock))
		return;

	inc_qos_status_get (&st, &wp->w_oiq_status);
	if (wp->w_listener.on_offered_incompatible_qos &&
	    (wp->w_mask & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) != 0) {
		DDS_DataWriterListener_on_offered_incompatible_qos ooiq =
			wp->w_listener.on_offered_incompatible_qos;
		wp->w_status &= ~DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
		wp->w_oiq_status.total_count_change = 0;
		lock_release (wp->w_lock);
		(*ooiq) (&wp->w_listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->listener.on_offered_incompatible_qos &&
	         (wp->w_publisher->mask & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) != 0) {
		DDS_PublisherListener_on_offered_incompatible_qos ooiq =
			wp->w_publisher->listener.on_offered_incompatible_qos;
		wp->w_status &= ~DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
		wp->w_oiq_status.total_count_change = 0;
		lock_release (wp->w_lock);
		(*ooiq) ( &wp->w_publisher->listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->domain->listener.on_offered_incompatible_qos &&
	         (wp->w_publisher->domain->mask & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) != 0) {
		DDS_DomainParticipantListener_on_offered_incompatible_qos ooiq =
			wp->w_publisher->domain->listener.on_offered_incompatible_qos;
		wp->w_status &= ~DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
		wp->w_oiq_status.total_count_change = 0;
		lock_release (wp->w_lock);
		(*ooiq) (&wp->w_publisher->domain->listener, (DDS_DataWriter) wp, &st);
	}
	else
		lock_release (wp->w_lock);

	if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, NULL);
}

DDS_ReturnCode_t DDS_DataWriter_get_offered_incompatible_qos_status (DDS_DataWriter wp,
						   DDS_OfferedIncompatibleQosStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_OIQ_ST, &wp, sizeof (wp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	inc_qos_status_get (st, &wp->w_oiq_status);
	wp->w_oiq_status.total_count_change = 0;
	wp->w_status &= ~DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
	lock_release (wp->w_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_requested_incompatible_qos -- Notify Incompatible QoS requested event. */

void dcps_requested_incompatible_qos (Reader_t *rp, DDS_QosPolicyId_t policy_id)
{
	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	rp->r_status |= DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
	rp->r_riq_status.total_count++;
	rp->r_riq_status.last_policy_id = policy_id;
	policy_id_add (&rp->r_riq_status, policy_id);
	if (!rp->r_riq_status.total_count_change++ &&
	    ((rp->r_listener.on_requested_incompatible_qos &&
	      (rp->r_mask & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) != 0) ||
	     (rp->r_subscriber->listener.on_requested_incompatible_qos &&
	      (rp->r_subscriber->mask & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) != 0) ||
	     (rp->r_subscriber->domain->listener.on_requested_incompatible_qos &&
	      (rp->r_subscriber->domain->mask & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_REQUESTED_INCOMPATIBLE_QOS);
	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
}

/* dcps_notify_requested_incompatible_qos -- Call requested incompatible QoS listener. */

static void dcps_notify_requested_incompatible_qos (Reader_t *rp)
{
	DDS_RequestedIncompatibleQosStatus	st;

	if (lock_take (rp->r_lock))
		return;

	inc_qos_status_get ((DDS_OfferedIncompatibleQosStatus *) &st, &rp->r_riq_status);
	if (rp->r_listener.on_requested_incompatible_qos &&
	    (rp->r_mask & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) != 0) {
		DDS_DataReaderListener_on_requested_incompatible_qos oriq =
			rp->r_listener.on_requested_incompatible_qos;
		rp->r_riq_status.total_count_change = 0;
		rp->r_status &= ~DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
		lock_release (rp->r_lock);
		(*oriq) (&rp->r_listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->listener.on_requested_incompatible_qos &&
	         (rp->r_subscriber->mask & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) != 0) {
		DDS_SubscriberListener_on_requested_incompatible_qos oriq =
			rp->r_subscriber->listener.on_requested_incompatible_qos;
		rp->r_riq_status.total_count_change = 0;
		rp->r_status &= ~DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
		lock_release (rp->r_lock);
		(*oriq) (&rp->r_subscriber->listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->domain->listener.on_requested_incompatible_qos &&
	         (rp->r_subscriber->domain->mask & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) != 0) {
		DDS_DomainParticipantListener_on_requested_incompatible_qos oriq =
			rp->r_subscriber->domain->listener.on_requested_incompatible_qos;
		rp->r_riq_status.total_count_change = 0;
		rp->r_status &= ~DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
		lock_release (rp->r_lock);
		(*oriq) (&rp->r_subscriber->domain->listener, (DDS_DataReader) rp, &st);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

DDS_ReturnCode_t DDS_DataReader_get_requested_incompatible_qos_status (DDS_DataReader rp,
						   DDS_RequestedIncompatibleQosStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_RIQ_ST, &rp, sizeof (rp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	inc_qos_status_get ((DDS_OfferedIncompatibleQosStatus *) st, &rp->r_riq_status);
	rp->r_riq_status.total_count_change = 0;
	rp->r_status &= ~DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_samples_lost -- Notify some samples lost event. */

void dcps_samples_lost (Reader_t *rp, unsigned nsamples)
{
	unsigned	prev_nsamples;

	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	rp->r_status |= DDS_SAMPLE_LOST_STATUS;
	rp->r_sl_status.total_count += nsamples;
	prev_nsamples = rp->r_sl_status.total_count_change;
	rp->r_sl_status.total_count_change += nsamples;
	if (!prev_nsamples &&
	    ((rp->r_listener.on_sample_lost &&
	      (rp->r_mask & DDS_SAMPLE_LOST_STATUS) != 0) ||
	     (rp->r_subscriber->listener.on_sample_lost &&
	      (rp->r_subscriber->mask & DDS_SAMPLE_LOST_STATUS) != 0) ||
	     (rp->r_subscriber->domain->listener.on_sample_lost &&
	      (rp->r_subscriber->domain->mask & DDS_SAMPLE_LOST_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_SAMPLE_LOST);
	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
}

/* dcps_notify_sample_lost -- Call the sample lost listener. */

static void dcps_notify_sample_lost (Reader_t *rp)
{
	DDS_SampleLostStatus	st;

	if (lock_take (rp->r_lock))
		return;

	st = rp->r_sl_status;
	if (rp->r_listener.on_sample_lost &&
	    (rp->r_mask & DDS_SAMPLE_LOST_STATUS) != 0) {
                DDS_DataReaderListener_on_sample_lost osl = 
                        rp->r_listener.on_sample_lost;
		rp->r_sl_status.total_count_change = 0;
		rp->r_status &= ~DDS_SAMPLE_LOST_STATUS;
		lock_release (rp->r_lock);
		(*osl) (&rp->r_listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->listener.on_sample_lost &&
	         (rp->r_subscriber->mask & DDS_SAMPLE_LOST_STATUS) != 0) {
                DDS_SubscriberListener_on_sample_lost osl = 
                        rp->r_subscriber->listener.on_sample_lost;
		rp->r_sl_status.total_count_change = 0;
		rp->r_status &= ~DDS_SAMPLE_LOST_STATUS;
		lock_release (rp->r_lock);
		(*osl) (&rp->r_subscriber->listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->domain->listener.on_sample_lost &&
	         (rp->r_subscriber->domain->mask & DDS_SAMPLE_LOST_STATUS) != 0) {
                DDS_DomainParticipantListener_on_sample_lost osl = 
                        rp->r_subscriber->domain->listener.on_sample_lost;
		rp->r_sl_status.total_count_change = 0;
		rp->r_status &= ~DDS_SAMPLE_LOST_STATUS;
		lock_release (rp->r_lock);
		(*osl) ( &rp->r_subscriber->domain->listener, (DDS_DataReader) rp, &st);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

DDS_ReturnCode_t DDS_DataReader_get_sample_lost_status (DDS_DataReader rp,
						   DDS_SampleLostStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_SL_ST, &rp, sizeof (rp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	*st = rp->r_sl_status;
	rp->r_sl_status.total_count_change = 0;
	rp->r_status &= ~DDS_SAMPLE_LOST_STATUS;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_sample_rejected -- Notify that a Sample was rejected for the given reason. */

void dcps_sample_rejected (Reader_t                     *rp,
			   DDS_SampleRejectedStatusKind kind,
			   DDS_InstanceHandle_t         handle)
{
	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	rp->r_status |= DDS_SAMPLE_REJECTED_STATUS;
	rp->r_sr_status.total_count++;
	rp->r_sr_status.last_reason = kind;
	rp->r_sr_status.last_instance_handle = handle;
	if (!rp->r_sr_status.total_count_change++ &&
	    ((rp->r_listener.on_sample_rejected &&
	      (rp->r_mask & DDS_SAMPLE_REJECTED_STATUS) != 0) ||
	     (rp->r_subscriber->listener.on_sample_rejected &&
	      (rp->r_subscriber->mask & DDS_SAMPLE_REJECTED_STATUS) != 0) ||
	     (rp->r_subscriber->domain->listener.on_sample_rejected &&
	      (rp->r_subscriber->domain->mask & DDS_SAMPLE_REJECTED_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_SAMPLE_REJECTED);
	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
}

/* dcps_notify_sample_rejected -- Call the sample rejected listener. */

static void dcps_notify_sample_rejected (Reader_t *rp)
{
	DDS_SampleRejectedStatus	st;

	if (lock_take (rp->r_lock))
		return;

	st.total_count = rp->r_sr_status.total_count;
	st.total_count_change = rp->r_sr_status.total_count_change;
	st.last_reason = rp->r_sr_status.last_reason;
	st.last_instance_handle = rp->r_sr_status.last_instance_handle;
	if (rp->r_listener.on_sample_rejected &&
	    (rp->r_mask & DDS_SAMPLE_REJECTED_STATUS) != 0) {
                DDS_DataReaderListener_on_sample_rejected osr =
                        rp->r_listener.on_sample_rejected;
		rp->r_sr_status.total_count_change = 0;
		rp->r_status &= ~DDS_SAMPLE_REJECTED_STATUS;
		lock_release (rp->r_lock);
		(*osr) (&rp->r_listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->listener.on_sample_rejected &&
	         (rp->r_subscriber->mask & DDS_SAMPLE_REJECTED_STATUS) != 0) {
                DDS_SubscriberListener_on_sample_rejected osr =
                        rp->r_subscriber->listener.on_sample_rejected;
		rp->r_sr_status.total_count_change = 0;
		rp->r_status &= ~DDS_SAMPLE_REJECTED_STATUS;
		lock_release (rp->r_lock);
		(*osr) ( &rp->r_subscriber->listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->domain->listener.on_sample_rejected &&
	         (rp->r_subscriber->domain->mask & DDS_SAMPLE_REJECTED_STATUS) != 0) {
                DDS_DomainParticipantListener_on_sample_rejected osr =
                        rp->r_subscriber->domain->listener.on_sample_rejected;
		rp->r_sr_status.total_count_change = 0;
		rp->r_status &= ~DDS_SAMPLE_REJECTED_STATUS;
		lock_release (rp->r_lock);
		(*osr) (&rp->r_subscriber->domain->listener, (DDS_DataReader) rp, &st);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

DDS_ReturnCode_t DDS_DataReader_get_sample_rejected_status (DDS_DataReader rp,
						   DDS_SampleRejectedStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_SR_ST, &rp, sizeof (rp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	st->total_count = rp->r_sr_status.total_count;
	st->total_count_change = rp->r_sr_status.total_count_change;
	st->last_reason = rp->r_sr_status.last_reason;
	st->last_instance_handle = rp->r_sr_status.last_instance_handle;
	rp->r_sr_status.total_count_change = 0;
	rp->r_status &= ~DDS_SAMPLE_REJECTED_STATUS;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_notify_data_on_readers -- Call the data on readers listener. */

static void dcps_notify_data_on_readers (Subscriber_t *sp)
{
	if (lock_take (sp->domain->lock))
		return;

	ctrc_printd (DCPS_ID, DCPS_NTF_D_OR_IND, &sp, sizeof (sp));
	if (sp->listener.on_data_on_readers &&
	    (sp->mask & DDS_DATA_ON_READERS_STATUS) != 0) {
                DDS_SubscriberListener_on_data_on_readers odr =
                        sp->listener.on_data_on_readers;
		sp->status &= ~DDS_DATA_AVAILABLE_STATUS;
		lock_release (sp->domain->lock);
		(*odr) (&sp->listener, (DDS_Subscriber) sp);
	}
	else if (sp->domain->listener.on_data_on_readers &&
	         (sp->domain->mask & DDS_DATA_ON_READERS_STATUS) != 0) {
                DDS_DomainParticipantListener_on_data_on_readers odr =
                        sp->domain->listener.on_data_on_readers;
		sp->status &= ~DDS_DATA_AVAILABLE_STATUS;
		lock_release (sp->domain->lock);
		(*odr) (&sp->domain->listener, (DDS_Subscriber) sp);
	}
	else
		lock_release (sp->domain->lock);

	if (sp->condition)
		dcps_waitset_wakeup (sp, sp->condition, NULL);
}

/* dcps_notify_data_available -- Call the data available listener. */

static void dcps_notify_data_available (Reader_t *rp)
{
	if (lock_take (rp->r_lock))
		return;

	ctrc_printd (DCPS_ID, DCPS_NTF_D_AV_IND, &rp, sizeof (rp));
	if (rp->r_listener.on_data_available &&
	    (rp->r_mask & DDS_DATA_AVAILABLE_STATUS) != 0) {
                DDS_DataReaderListener_on_data_available oda =
                        rp->r_listener.on_data_available;
		rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
		lock_release (rp->r_lock);
		(*oda) (&rp->r_listener, rp);
	}
	else if (rp->r_subscriber->listener.on_data_available &&
	         (rp->r_subscriber->mask & DDS_DATA_AVAILABLE_STATUS) != 0) {
                DDS_SubscriberListener_on_data_available oda =
                        rp->r_subscriber->listener.on_data_available;
		rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
		lock_release (rp->r_lock);
		(*oda) (&rp->r_subscriber->listener, rp);
	}
	else if (rp->r_subscriber->domain->listener.on_data_available &&
	         (rp->r_subscriber->domain->mask & DDS_DATA_AVAILABLE_STATUS) != 0) {
                DDS_DomainParticipantListener_on_data_available oda =
                       rp->r_subscriber->domain->listener.on_data_available;
		rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
		lock_release (rp->r_lock);
		(*oda) (&rp->r_subscriber->domain->listener, rp);
	}
	else if (rp->r_subscriber->listener.on_data_on_readers &&
	         (rp->r_subscriber->mask & DDS_DATA_ON_READERS_STATUS) != 0) {
                DDS_SubscriberListener_on_data_on_readers oda =
                        rp->r_subscriber->listener.on_data_on_readers;
		rp->r_status &= ~DDS_DATA_ON_READERS_STATUS;
		lock_release (rp->r_lock);
		(*oda) (&rp->r_subscriber->listener, rp->r_subscriber);
	}
	else if (rp->r_subscriber->domain->listener.on_data_on_readers &&
	         (rp->r_subscriber->domain->mask & DDS_DATA_ON_READERS_STATUS) != 0) {
                DDS_DomainParticipantListener_on_data_on_readers oda =
                       rp->r_subscriber->domain->listener.on_data_on_readers;
		rp->r_status &= ~DDS_DATA_ON_READERS_STATUS;
		lock_release (rp->r_lock);
		(*oda) (&rp->r_subscriber->domain->listener, rp->r_subscriber);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

/* dcps_liveliness_lost -- Notify that Reader liveliness was lost. */

void dcps_liveliness_lost (Writer_t *wp)
{
	if ((wp->w_flags & EF_ENABLED) == 0)
		return;

	wp->w_status |= DDS_LIVELINESS_LOST_STATUS;
	wp->w_ll_status.total_count++;
	if (!wp->w_ll_status.total_count_change++ &&
	    ((wp->w_listener.on_liveliness_lost &&
	      (wp->w_mask & DDS_LIVELINESS_LOST_STATUS) != 0) ||
	     (wp->w_publisher->listener.on_liveliness_lost &&
	      (wp->w_publisher->mask & DDS_LIVELINESS_LOST_STATUS) != 0) ||
	     (wp->w_publisher->domain->listener.on_liveliness_lost &&
	      (wp->w_publisher->domain->mask & DDS_LIVELINESS_LOST_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) wp, NT_LIVELINESS_LOST);
	else if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, &wp->w_lock);
}

/* dcps_notify_liveliness_lost -- Call the liveliness lost listener. */

static void dcps_notify_liveliness_lost (Writer_t *wp)
{
	DDS_LivelinessLostStatus	st;

	if (lock_take (wp->w_lock))
		return;

	st = wp->w_ll_status;
	if (wp->w_listener.on_liveliness_lost &&
	    (wp->w_mask & DDS_LIVELINESS_LOST_STATUS) != 0) {
		DDS_DataWriterListener_on_liveliness_lost oll =
			wp->w_listener.on_liveliness_lost;
		wp->w_ll_status.total_count_change = 0;
		wp->w_status &= ~DDS_LIVELINESS_LOST_STATUS;
		lock_release (wp->w_lock);
		(*oll) (&wp->w_listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->listener.on_liveliness_lost &&
	         (wp->w_publisher->mask & DDS_LIVELINESS_LOST_STATUS) != 0) {
                DDS_PublisherListener_on_liveliness_lost oll =
			wp->w_publisher->listener.on_liveliness_lost;
		wp->w_ll_status.total_count_change = 0;
		wp->w_status &= ~DDS_LIVELINESS_LOST_STATUS;
		lock_release (wp->w_lock);
		(*oll) (&wp->w_publisher->listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->domain->listener.on_liveliness_lost &&
	         (wp->w_publisher->domain->mask & DDS_LIVELINESS_LOST_STATUS) != 0) {
		DDS_DomainParticipantListener_on_liveliness_lost oll =
			wp->w_publisher->domain->listener.on_liveliness_lost;
		wp->w_ll_status.total_count_change = 0;
		wp->w_status &= ~DDS_LIVELINESS_LOST_STATUS;
		lock_release (wp->w_lock);
		(*oll) (&wp->w_publisher->domain->listener, (DDS_DataWriter) wp, &st);
	}
	else
		lock_release (wp->w_lock);

	if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, NULL);
}

DDS_ReturnCode_t DDS_DataWriter_get_liveliness_lost_status (DDS_DataWriter wp,
						   DDS_LivelinessLostStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_LL_ST, &wp, sizeof (wp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	*st = wp->w_ll_status;
	wp->w_ll_status.total_count_change = 0;
	wp->w_status &= ~DDS_LIVELINESS_LOST_STATUS;
	lock_release (wp->w_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_liveliness_change -- Notify that a liveliness change occurred. */

void dcps_liveliness_change (Reader_t             *rp,
			     int                  mode,
			     int                  alive,
			     DDS_InstanceHandle_t handle)
{
	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	rp->r_status |= DDS_LIVELINESS_CHANGED_STATUS;
	if (alive) {
		if (mode == DLI_REMOVE) {
			rp->r_lc_status.alive_count--;
			rp->r_lc_status.alive_count_change--;
		}
		else {
			rp->r_lc_status.alive_count++;
			rp->r_lc_status.alive_count_change++;
		}
		if (mode == DLI_EXISTS) {
			rp->r_lc_status.not_alive_count--;
			rp->r_lc_status.not_alive_count_change--;
		}
	}
	else {
		if (mode == DLI_EXISTS) {
			rp->r_lc_status.alive_count--;
			rp->r_lc_status.alive_count_change--;
		}
		else if (mode == DLI_REMOVE) {
			rp->r_lc_status.not_alive_count--;
			rp->r_lc_status.not_alive_count_change--;
		}
		if (mode != DLI_REMOVE) {
			rp->r_lc_status.not_alive_count++;
			rp->r_lc_status.not_alive_count_change++;
		}
	}
	rp->r_lc_status.last_publication_handle = handle;
	if ((rp->r_lc_status.alive_count_change ||
	     rp->r_lc_status.not_alive_count_change) &&
	    ((rp->r_listener.on_liveliness_changed &&
	      (rp->r_mask & DDS_LIVELINESS_CHANGED_STATUS) != 0) ||
	     (rp->r_subscriber->listener.on_liveliness_changed &&
	      (rp->r_subscriber->mask & DDS_LIVELINESS_CHANGED_STATUS) != 0) ||
	     (rp->r_subscriber->domain->listener.on_liveliness_changed &&
	       (rp->r_subscriber->domain->mask & DDS_LIVELINESS_CHANGED_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_LIVELINESS_CHANGED);
	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
}

/* dcps_notify_liveliness_change -- Call the liveliness change listener. */

static void dcps_notify_liveliness_change (Reader_t *rp)
{
	DDS_LivelinessChangedStatus	st;

	if (lock_take (rp->r_lock))
		return;

	st = rp->r_lc_status;
	if (rp->r_listener.on_liveliness_changed &&
	    (rp->r_mask & DDS_LIVELINESS_CHANGED_STATUS) != 0) {
		DDS_DataReaderListener_on_liveliness_changed olc =
			rp->r_listener.on_liveliness_changed;
		rp->r_status &= ~DDS_LIVELINESS_CHANGED_STATUS;
		rp->r_lc_status.alive_count_change = 
		rp->r_lc_status.not_alive_count_change = 0;
		lock_release (rp->r_lock);
		(*olc) (&rp->r_listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->listener.on_liveliness_changed &&
	         (rp->r_subscriber->mask & DDS_LIVELINESS_CHANGED_STATUS) != 0) {
		DDS_SubscriberListener_on_liveliness_changed olc =
			rp->r_subscriber->listener.on_liveliness_changed;
		rp->r_status &= ~DDS_LIVELINESS_CHANGED_STATUS;
		rp->r_lc_status.alive_count_change = 
		rp->r_lc_status.not_alive_count_change = 0;
		lock_release (rp->r_lock);
		(*olc) (&rp->r_subscriber->listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->domain->listener.on_liveliness_changed &&
	         (rp->r_subscriber->domain->mask & DDS_LIVELINESS_CHANGED_STATUS) != 0) {
		DDS_DomainParticipantListener_on_liveliness_changed olc =
			rp->r_subscriber->domain->listener.on_liveliness_changed;
		rp->r_status &= ~DDS_LIVELINESS_CHANGED_STATUS;
		rp->r_lc_status.alive_count_change = 
		rp->r_lc_status.not_alive_count_change = 0;
		lock_release (rp->r_lock);
		(*olc) (&rp->r_subscriber->domain->listener, (DDS_DataReader) rp, &st);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

DDS_ReturnCode_t DDS_DataReader_get_liveliness_changed_status (DDS_DataReader rp,
						   DDS_LivelinessChangedStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_LC_ST, &rp, sizeof (rp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	*st = rp->r_lc_status;
	rp->r_lc_status.alive_count_change = 
	rp->r_lc_status.not_alive_count_change = 0;
	rp->r_status &= ~DDS_LIVELINESS_CHANGED_STATUS;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_notify_publication_match -- Used by Discovery/Cache to notify a change
				    in the # of reader proxies of a writer. */

void dcps_publication_match (Writer_t         *wp,
			     int              add, 
			     const Endpoint_t *ep)
{
	if ((wp->w_flags & EF_ENABLED) == 0)
		return;

	wp->w_status |= DDS_PUBLICATION_MATCHED_STATUS;
	wp->w_pm_status.last_subscription_handle = ep->entity.handle;
	if (add) {
		wp->w_pm_status.total_count++;
		wp->w_pm_status.total_count_change++;
		wp->w_pm_status.current_count++;
	}
	else
		wp->w_pm_status.current_count--;
	if (!wp->w_pm_status.current_count_change++ &&
	    ((wp->w_listener.on_publication_matched &&
	      (wp->w_mask & DDS_PUBLICATION_MATCHED_STATUS) != 0) ||
	     (wp->w_publisher->listener.on_publication_matched &&
	      (wp->w_publisher->mask & DDS_PUBLICATION_MATCHED_STATUS) != 0) ||
	     (wp->w_publisher->domain->listener.on_publication_matched &&
	      (wp->w_publisher->domain->mask & DDS_PUBLICATION_MATCHED_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) wp, NT_PUBLICATION_MATCHED);
	else if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, &wp->w_lock);
}

/* dcps_notify_publication_match -- Call the publication match listener. */

static void dcps_notify_publication_match (Writer_t *wp)
{
	DDS_PublicationMatchedStatus	st;

	if (lock_take (wp->w_lock))
		return;

	st = wp->w_pm_status;
	if (wp->w_listener.on_publication_matched &&
	    (wp->w_mask & DDS_PUBLICATION_MATCHED_STATUS) != 0) {
		DDS_DataWriterListener_on_publication_matched opm =
			wp->w_listener.on_publication_matched;
		wp->w_status &= ~DDS_PUBLICATION_MATCHED_STATUS;
		wp->w_pm_status.total_count_change = 
		wp->w_pm_status.current_count_change = 0;
		lock_release (wp->w_lock);
		(*opm) (&wp->w_listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->listener.on_publication_matched &&
	         (wp->w_publisher->mask & DDS_PUBLICATION_MATCHED_STATUS) != 0) {
		DDS_PublisherListener_on_publication_matched opm =
			wp->w_publisher->listener.on_publication_matched;
		wp->w_status &= ~DDS_PUBLICATION_MATCHED_STATUS;
		wp->w_pm_status.total_count_change = 
		wp->w_pm_status.current_count_change = 0;
		lock_release (wp->w_lock);
		(*opm) (&wp->w_publisher->listener, (DDS_DataWriter) wp, &st);
	}
	else if (wp->w_publisher->domain->listener.on_publication_matched &&
	         (wp->w_publisher->domain->mask & DDS_PUBLICATION_MATCHED_STATUS) != 0) {
		DDS_DomainParticipantListener_on_publication_matched opm =
			wp->w_publisher->domain->listener.on_publication_matched;
		wp->w_status &= ~DDS_PUBLICATION_MATCHED_STATUS;
		wp->w_pm_status.total_count_change = 
		wp->w_pm_status.current_count_change = 0;
		lock_release (wp->w_lock);
		(*opm) (&wp->w_publisher->domain->listener, (DDS_DataWriter) wp, &st);
	}
	else
		lock_release (wp->w_lock);

	if (wp->w_condition)
		dcps_waitset_wakeup (wp, wp->w_condition, NULL);
}

DDS_ReturnCode_t DDS_DataWriter_get_publication_matched_status (DDS_DataWriter wp,
						   DDS_PublicationMatchedStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_PM_ST, &wp, sizeof (wp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	*st = wp->w_pm_status;
	wp->w_pm_status.total_count_change =
	wp->w_pm_status.current_count_change = 0;
	wp->w_status &= ~DDS_PUBLICATION_MATCHED_STATUS;
	lock_release (wp->w_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_notify_subscription_match -- Used by Discovery/Cache to notify a change
				     in the # of writer proxies of a reader. */

void dcps_subscription_match (Reader_t         *rp,
			      int              add, 
			      const Endpoint_t *ep)
{
	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	rp->r_status |= DDS_SUBSCRIPTION_MATCHED_STATUS;
	rp->r_sm_status.last_publication_handle = ep->entity.handle;
	if (add) {
		rp->r_sm_status.total_count++;
		rp->r_sm_status.total_count_change++;
		rp->r_sm_status.current_count++;
	}
	else
		rp->r_sm_status.current_count--;
	if (!rp->r_sm_status.current_count_change++ &&
	    ((rp->r_listener.on_subscription_matched && 
	     (rp->r_mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0) ||
	    (rp->r_subscriber->listener.on_subscription_matched &&
	     (rp->r_subscriber->mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0) ||
	    (rp->r_subscriber->domain->listener.on_subscription_matched &&
	     (rp->r_subscriber->domain->mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0)))
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_SUBSCRIPTION_MATCHED);
	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
}

/* dcps_notify_subscription_match -- Call the subscription match listener. */

static void dcps_notify_subscription_match (Reader_t *rp)
{
	DDS_SubscriptionMatchedStatus	st;

	if (lock_take (rp->r_lock))
		return;

	st = rp->r_sm_status;
	if (rp->r_listener.on_subscription_matched && 
	    (rp->r_mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0) {
		DDS_DataReaderListener_on_subscription_matched osm = 
			rp->r_listener.on_subscription_matched;
		rp->r_status &= ~DDS_SUBSCRIPTION_MATCHED_STATUS;
		rp->r_sm_status.total_count_change = 
		rp->r_sm_status.current_count_change = 0;
		lock_release (rp->r_lock);
		(*osm) (&rp->r_listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->listener.on_subscription_matched &&
	         (rp->r_subscriber->mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0) {
		DDS_SubscriberListener_on_subscription_matched osm = 
			rp->r_subscriber->listener.on_subscription_matched;
		rp->r_status &= ~DDS_SUBSCRIPTION_MATCHED_STATUS;
		rp->r_sm_status.total_count_change = 
		rp->r_sm_status.current_count_change = 0;
		lock_release (rp->r_lock);
		(*osm) (&rp->r_subscriber->listener, (DDS_DataReader) rp, &st);
	}
	else if (rp->r_subscriber->domain->listener.on_subscription_matched &&
	         (rp->r_subscriber->domain->mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0) {
		DDS_DomainParticipantListener_on_subscription_matched osm =
			rp->r_subscriber->domain->listener.on_subscription_matched;
		lock_take (rp->r_lock);
		rp->r_status &= ~DDS_SUBSCRIPTION_MATCHED_STATUS;
		rp->r_sm_status.total_count_change = 
		rp->r_sm_status.current_count_change = 0;
		lock_release (rp->r_lock);
		(*osm) (&rp->r_subscriber->domain->listener, (DDS_DataReader) rp, &st);
	}
	else
		lock_release (rp->r_lock);

	if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, NULL);
}

DDS_ReturnCode_t DDS_DataReader_get_subscription_matched_status (DDS_DataReader rp,
						   DDS_SubscriptionMatchedStatus *st)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_SM_ST, &rp, sizeof (rp));
	ctrc_contd (&st, sizeof (st));
	ctrc_endd ();

	if (!st)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	*st = rp->r_sm_status;
	rp->r_sm_status.total_count_change = 
	rp->r_sm_status.current_count_change = 0;
	rp->r_status &= ~DDS_SUBSCRIPTION_MATCHED_STATUS;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

/* dcps_notify_listener -- Call a listener on the given entity of the type. */

void dcps_notify_listener (Entity_t *ep, NotificationType_t t)
{
	if (t == NT_INCONSISTENT_TOPIC) { /* Topic */
		if (ep->type != ET_TOPIC)
			return;

		dcps_notify_inconsistent_topic ((Topic_t *) ep);
	}
	else if (t == NT_DATA_ON_READERS) {
		if (ep->type != ET_SUBSCRIBER)
			return;

		dcps_notify_data_on_readers ((Subscriber_t *) ep);
	}
	else if (t == NT_OFFERED_DEADLINE_MISSED ||
		 t == NT_OFFERED_INCOMPATIBLE_QOS ||
		 t == NT_LIVELINESS_LOST ||
		 t == NT_PUBLICATION_MATCHED) {	/* Writer */
		if (ep->type != ET_WRITER)
			return;

		switch (t) {
			case NT_OFFERED_DEADLINE_MISSED:
				dcps_notify_offered_deadline_missed ((Writer_t *) ep);
				break;
			case NT_OFFERED_INCOMPATIBLE_QOS:
				dcps_notify_offered_incompatible_qos ((Writer_t *) ep);
				break;
			case NT_LIVELINESS_LOST:
				dcps_notify_liveliness_lost ((Writer_t *) ep);
				break;
			case NT_PUBLICATION_MATCHED:
				dcps_notify_publication_match ((Writer_t *) ep);
				break;
			default:
				break;
		}
	}
	else {
		if (ep->type != ET_READER)
			return;

		switch (t) {
			case NT_REQUESTED_DEADLINE_MISSED:
				dcps_notify_requested_deadline_missed ((Reader_t *) ep);
				break;
			case NT_REQUESTED_INCOMPATIBLE_QOS:
				dcps_notify_requested_incompatible_qos ((Reader_t *) ep);
				break;
			case NT_SAMPLE_LOST:
				dcps_notify_sample_lost ((Reader_t *) ep);
				break;
			case NT_SAMPLE_REJECTED:
				dcps_notify_sample_rejected ((Reader_t *) ep);
				break;
			case NT_DATA_AVAILABLE:
				dcps_notify_data_available ((Reader_t *) ep);
				break;
			case NT_LIVELINESS_CHANGED:
				dcps_notify_liveliness_change ((Reader_t *) ep);
				break;
			case NT_SUBSCRIPTION_MATCHED:
				dcps_notify_subscription_match ((Reader_t *) ep);
				break;
			default:
				break;
		}
	}
}

/* dcps_data_available_listener -- Returns whether there is an active Data 
				   Available listener. */

int dcps_data_available_listener (Reader_t *rp)
{
	if ((rp->r_listener.on_data_available &&
	     (rp->r_mask & DDS_DATA_AVAILABLE_STATUS) != 0) ||
	    (rp->r_subscriber->listener.on_data_available &&
	     (rp->r_subscriber->mask & DDS_DATA_AVAILABLE_STATUS) != 0) ||
	    (rp->r_subscriber->domain->listener.on_data_available &&
	     (rp->r_subscriber->domain->mask & DDS_DATA_AVAILABLE_STATUS) != 0))
		return (1);
	else
		return (0);
}

/* data_on_readers_listener -- Returns whether there is an active Data On
			       Readers listener. */

static int data_on_readers_listener (Reader_t *rp)
{
	if ((rp->r_subscriber->listener.on_data_on_readers &&
	     (rp->r_subscriber->mask & DDS_DATA_ON_READERS_STATUS) != 0) ||
	    (rp->r_subscriber->domain->listener.on_data_on_readers &&
	     (rp->r_subscriber->domain->mask & DDS_DATA_ON_READERS_STATUS) != 0))
		return (1);
	else
		return (0);
}

/* dcps_data_available -- Data available indication from cache. */

void dcps_data_available (uintptr_t user, Cache_t cdp)
{
	Reader_t	*rp = (Reader_t *) user;
	int		reader_notify = (rp->r_status & DDS_DATA_AVAILABLE_STATUS) == 0;
	int		subscriber_notify = (rp->r_subscriber->status & DDS_DATA_AVAILABLE_STATUS) == 0;

	if ((rp->r_flags & EF_ENABLED) == 0)
		return;

	ARG_NOT_USED (cdp)

	/*dbg_printf ("DATA_Avail(%lu)!\r\n", rp->r_handle);*/

	/* Need to raise 2 different events: - on_data_on_readers, and 
					     - on_data_available. */
	ctrc_printd (DCPS_ID, DCPS_NTF_D_AVAIL, &rp, sizeof (rp));
	rp->r_status |= DDS_DATA_AVAILABLE_STATUS;
	rp->r_subscriber->status |= DDS_DATA_AVAILABLE_STATUS;

	/* Check if on_data_on_readers needs to be notified. */
	if (data_on_readers_listener (rp)) {
		if (subscriber_notify)
			dds_notify (NSC_DCPS, (Entity_t *) rp->r_subscriber, NT_DATA_ON_READERS);
	}
	else if (dcps_data_available_listener (rp)) {
		if (reader_notify)
			dds_notify (NSC_DCPS, (Entity_t *) rp, NT_DATA_AVAILABLE);
	}
	else {
		/*dbg_printf ("dcps_data_available --");*/
		if (rp->r_subscriber->condition) {
			dcps_waitset_wakeup (rp->r_subscriber,
					     rp->r_subscriber->condition,
					     &rp->r_subscriber->domain->lock);
			/*dbg_printf (" wakeup-subscriber");*/
		}
		if (rp->r_conditions) {
			dcps_waitset_wakeup (rp, rp->r_conditions, &rp->r_lock);
			/*dbg_printf (" wakeup-reader");*/
		}
		/*dbg_printf ("!\r\n");*/
	}
}

#define	OFS_NA	-1

static const size_t listener_offsets [6][15] =
{
	/* ET_PARTICIPANT */
	{
		/* DDS_INCONSISTENT_TOPIC_STATUS 	 */	offsetof (DDS_DomainParticipantListener, on_inconsistent_topic),
		/* DDS_OFFERED_DEADLINE_MISSED_STATUS	 */	offsetof (DDS_DomainParticipantListener, on_offered_deadline_missed),
		/* DDS_REQUESTED_DEADLINE_MISSED_STATUS	 */	offsetof (DDS_DomainParticipantListener, on_requested_deadline_missed),
		/* gap */					OFS_NA,
		/* gap */					OFS_NA,
		/* DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   */     offsetof (DDS_DomainParticipantListener, on_offered_incompatible_qos),
		/* DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */	offsetof (DDS_DomainParticipantListener, on_requested_incompatible_qos),
		/* DDS_SAMPLE_LOST_STATUS              	 */	offsetof (DDS_DomainParticipantListener, on_sample_lost),
		/* DDS_SAMPLE_REJECTED_STATUS            */     offsetof (DDS_DomainParticipantListener, on_sample_rejected),
		/* DDS_DATA_ON_READERS_STATUS          	 */	offsetof (DDS_DomainParticipantListener, on_data_on_readers), 
		/* DDS_DATA_AVAILABLE_STATUS             */	offsetof (DDS_DomainParticipantListener, on_data_available),
		/* DDS_LIVELINESS_LOST_STATUS            */	offsetof (DDS_DomainParticipantListener, on_liveliness_lost),
		/* DDS_LIVELINESS_CHANGED_STATUS       	 */	offsetof (DDS_DomainParticipantListener, on_liveliness_changed), 
		/* DDS_PUBLICATION_MATCHED_STATUS        */	offsetof (DDS_DomainParticipantListener, on_publication_matched),
		/* DDS_SUBSCRIPTION_MATCHED_STATUS       */	offsetof (DDS_DomainParticipantListener, on_subscription_matched)
	},
	/* ET_TOPIC */
	{
		/* DDS_INCONSISTENT_TOPIC_STATUS 	 */	offsetof (DDS_TopicListener, on_inconsistent_topic),
		/* DDS_OFFERED_DEADLINE_MISSED_STATUS	 */	OFS_NA,
		/* DDS_REQUESTED_DEADLINE_MISSED_STATUS	 */	OFS_NA,
		/* gap */					OFS_NA,
		/* gap */					OFS_NA,
		/* DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   */     OFS_NA,
		/* DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */	OFS_NA,
		/* DDS_SAMPLE_LOST_STATUS              	 */	OFS_NA,
		/* DDS_SAMPLE_REJECTED_STATUS            */     OFS_NA,
		/* DDS_DATA_ON_READERS_STATUS          	 */	OFS_NA,
		/* DDS_DATA_AVAILABLE_STATUS             */	OFS_NA,
		/* DDS_LIVELINESS_LOST_STATUS            */	OFS_NA,
		/* DDS_LIVELINESS_CHANGED_STATUS       	 */	OFS_NA,
		/* DDS_PUBLICATION_MATCHED_STATUS        */	OFS_NA,
		/* DDS_SUBSCRIPTION_MATCHED_STATUS       */	OFS_NA
	},
	/* ET_PUBLISHER */
	{
		/* DDS_INCONSISTENT_TOPIC_STATUS 	 */	OFS_NA,
		/* DDS_OFFERED_DEADLINE_MISSED_STATUS	 */	offsetof (DDS_PublisherListener, on_offered_deadline_missed),
		/* DDS_REQUESTED_DEADLINE_MISSED_STATUS	 */	OFS_NA,
		/* gap */					OFS_NA,
		/* gap */					OFS_NA,
		/* DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   */     OFS_NA,
		/* DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */	offsetof (DDS_PublisherListener, on_offered_incompatible_qos),
		/* DDS_SAMPLE_LOST_STATUS              	 */	OFS_NA,
		/* DDS_SAMPLE_REJECTED_STATUS            */     OFS_NA,
		/* DDS_DATA_ON_READERS_STATUS          	 */	OFS_NA,
		/* DDS_DATA_AVAILABLE_STATUS             */	OFS_NA,
		/* DDS_LIVELINESS_LOST_STATUS            */	offsetof (DDS_PublisherListener, on_liveliness_lost),
		/* DDS_LIVELINESS_CHANGED_STATUS       	 */	OFS_NA,
		/* DDS_PUBLICATION_MATCHED_STATUS        */	offsetof (DDS_PublisherListener, on_publication_matched),
		/* DDS_SUBSCRIPTION_MATCHED_STATUS       */	OFS_NA
	},
	/* ET_SUBSCRIBER */
	{
		/* DDS_INCONSISTENT_TOPIC_STATUS 	 */	OFS_NA,
		/* DDS_OFFERED_DEADLINE_MISSED_STATUS	 */	OFS_NA,
		/* DDS_REQUESTED_DEADLINE_MISSED_STATUS	 */	offsetof (DDS_SubscriberListener, on_requested_deadline_missed),
		/* gap */					OFS_NA,
		/* gap */					OFS_NA,
		/* DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   */     OFS_NA,
		/* DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */	offsetof (DDS_SubscriberListener, on_requested_incompatible_qos),
		/* DDS_SAMPLE_LOST_STATUS              	 */	offsetof (DDS_SubscriberListener, on_sample_lost),
		/* DDS_SAMPLE_REJECTED_STATUS            */     offsetof (DDS_SubscriberListener, on_sample_rejected),
		/* DDS_DATA_ON_READERS_STATUS          	 */	offsetof (DDS_SubscriberListener, on_data_on_readers), 
		/* DDS_DATA_AVAILABLE_STATUS             */	offsetof (DDS_SubscriberListener, on_data_available),
		/* DDS_LIVELINESS_LOST_STATUS            */	OFS_NA,
		/* DDS_LIVELINESS_CHANGED_STATUS       	 */	offsetof (DDS_SubscriberListener, on_liveliness_changed), 
		/* DDS_PUBLICATION_MATCHED_STATUS        */	OFS_NA,
		/* DDS_SUBSCRIPTION_MATCHED_STATUS       */	offsetof (DDS_SubscriberListener, on_subscription_matched)
	},
	/* ET_WRITER */
	{
		/* DDS_INCONSISTENT_TOPIC_STATUS 	 */	OFS_NA,
		/* DDS_OFFERED_DEADLINE_MISSED_STATUS	 */	offsetof (DDS_DataWriterListener, on_offered_deadline_missed),
		/* DDS_REQUESTED_DEADLINE_MISSED_STATUS	 */	OFS_NA,
		/* gap */					OFS_NA,
		/* gap */					OFS_NA,
		/* DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   */     offsetof (DDS_DataWriterListener, on_offered_incompatible_qos),
		/* DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */	OFS_NA,
		/* DDS_SAMPLE_LOST_STATUS              	 */	OFS_NA,
		/* DDS_SAMPLE_REJECTED_STATUS            */     OFS_NA,
		/* DDS_DATA_ON_READERS_STATUS          	 */	OFS_NA,
		/* DDS_DATA_AVAILABLE_STATUS             */	OFS_NA,
		/* DDS_LIVELINESS_LOST_STATUS            */	offsetof (DDS_DataWriterListener, on_liveliness_lost),
		/* DDS_LIVELINESS_CHANGED_STATUS       	 */	OFS_NA,
		/* DDS_PUBLICATION_MATCHED_STATUS        */	offsetof (DDS_DataWriterListener, on_publication_matched),
		/* DDS_SUBSCRIPTION_MATCHED_STATUS       */	OFS_NA
	},
	/* ET_READER */
	{
		/* DDS_INCONSISTENT_TOPIC_STATUS 	 */	OFS_NA,
		/* DDS_OFFERED_DEADLINE_MISSED_STATUS	 */	OFS_NA,
		/* DDS_REQUESTED_DEADLINE_MISSED_STATUS	 */	offsetof (DDS_DataReaderListener, on_requested_deadline_missed),
		/* gap */					OFS_NA,
		/* gap */					OFS_NA,
		/* DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   */     OFS_NA,
		/* DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */	offsetof (DDS_DataReaderListener, on_requested_incompatible_qos),
		/* DDS_SAMPLE_LOST_STATUS              	 */	offsetof (DDS_DataReaderListener, on_sample_lost),
		/* DDS_SAMPLE_REJECTED_STATUS            */     offsetof (DDS_DataReaderListener, on_sample_rejected),
		/* DDS_DATA_ON_READERS_STATUS          	 */	OFS_NA,
		/* DDS_DATA_AVAILABLE_STATUS             */	offsetof (DDS_DataReaderListener, on_data_available),
		/* DDS_LIVELINESS_LOST_STATUS            */	OFS_NA,
		/* DDS_LIVELINESS_CHANGED_STATUS       	 */	offsetof (DDS_DataReaderListener, on_liveliness_changed), 
		/* DDS_PUBLICATION_MATCHED_STATUS        */	OFS_NA,
		/* DDS_SUBSCRIPTION_MATCHED_STATUS       */	offsetof (DDS_DataReaderListener, on_subscription_matched)
	}
};

/* dcps_update_listener-- Called with entity lock taken. */

void dcps_update_listener (Entity_t       *ep, 
			   lock_t         *elock,
			   unsigned short *old_mask, 
			   void           *old_listener_struct, 
			   DDS_StatusMask new_mask, 
			   const void     *new_listener_struct)
{
	int		i;
	int		block = 0;
	int		in_listener;
	EntityType_t	et = ep->type;
	DDS_StatusMask	i_old_mask = *old_mask;
	DDS_StatusMask	i_new_mask = new_mask;
	void		**old_cb, **new_cb;

	if (et < ET_PARTICIPANT || et > ET_READER)
		return;

	et--;

	/* Set new listeners, and verify if we need to block until active 
	   listeners are finished. */
	for (i = 0; i < 15; i++, i_old_mask >>= 1, i_new_mask >>= 1) {
		if (listener_offsets [et][i] == (size_t) OFS_NA)
			continue;

		old_cb = ((void **) (((char *) old_listener_struct) + 
						listener_offsets [et][i]));
		new_cb = new_listener_struct ?
			 ((void **) (((char *) new_listener_struct) + 
						listener_offsets [et][i])) 
					     : NULL;

		/* If we had a listener in the previous situation and we do not
		   have one in the current situation: */
		if ((i_old_mask & 1) != 0 &&
		    *old_cb != NULL &&
		    (!new_listener_struct || (i_new_mask & 1) == 0 || !*new_cb))

			/* We will need to purge events pending on this listener */
			block = 1;

		*old_cb = new_cb ? *new_cb : NULL;
	}

	/* Remove the events from the notification queue in which we are no
	   longer interested. */
	in_listener = dds_purge_notifications (ep, *old_mask & ~new_mask, 0) == 0;

	/* If dds_purge_notifications flagged we have a running listener,
	   and we are not in the core thread, the call to set_listener() was not
	   done from within a listener. This means we need to verify if we need
	   to block. */
	if (in_listener && block && thread_id () != dds_core_thread) {
		lock_release (*elock);
		dds_wait_listener (ep);
		lock_take (*elock);
	}
	*old_mask = new_mask;
}


