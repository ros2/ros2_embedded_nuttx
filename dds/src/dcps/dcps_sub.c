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

/* dcps_sub.c -- DCPS API - Subscriber functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "sys.h"
#include "ctrace.h"
#include "prof.h"
#include "log.h"
#include "pool.h"
#include "str.h"
#include "error.h"
#include "dds/dds_dcps.h"
#include "dds/dds_aux.h"
#include "dds_data.h"
#include "dds.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "domain.h"
#include "xtypes.h"
#include "disc.h"
#include "dcps_dpfact.h"
#include "dcps_event.h"
#include "dcps_waitset.h"
#include "dcps_builtin.h"
#include "dcps_pub.h"
#include "dcps_sub.h"
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
#include "sec_access.h"
#include "sec_crypto.h"
#else
#include "security.h"
#endif
#endif

DDS_ReturnCode_t DDS_Subscriber_get_qos (DDS_Subscriber sp,
					 DDS_SubscriberQos *qos)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_S_G_QOS, &sp, sizeof (sp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	qos_subscriber_get (&sp->qos, qos);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

static int update_reader_qos (Skiplist_t *list, void *node, void *arg)
{
	Reader_t        *rp, **rpp = (Reader_t **) node;
	Subscriber_t    *sp = (Subscriber_t *) arg;

	ARG_NOT_USED (list)     

	rp = *rpp;

	if (rp->r_subscriber == sp && (rp->r_flags & EF_ENABLED) != 0) {
		lock_take (rp->r_topic->lock);
#ifdef RW_LOCKS
		lock_take (rp->r_lock);
#endif
		hc_qos_update (rp->r_cache);
		disc_reader_update (sp->domain, rp, 1, 0);
#ifdef RW_LOCKS
		lock_release (rp->r_lock);
#endif
		lock_release (rp->r_topic->lock);
	}
	return (1);
}

DDS_ReturnCode_t DDS_Subscriber_set_qos (DDS_Subscriber sp,
					 DDS_SubscriberQos *qos)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_S_S_QOS, &sp, sizeof (sp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	if (qos == DDS_PUBLISHER_QOS_DEFAULT)
		qos = &sp->domain->def_subscriber_qos;
	else if (!qos_valid_subscriber_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = qos_subscriber_update (&sp->qos, qos);
	if (ret == DDS_RETCODE_OK)
		sl_walk (&sp->domain->participant.p_endpoints, update_reader_qos, sp);

    done:
	lock_release (dp->lock);
	return (ret);
}

DDS_SubscriberListener *DDS_Subscriber_get_listener (DDS_Subscriber sp)
{
	Domain_t		*dp;
	DDS_SubscriberListener	*lp;

	ctrc_printd (DCPS_ID, DCPS_S_G_LIS, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, NULL))
		return (NULL);

	dp = domain_ptr (sp->domain, 1, NULL);
	if (!dp)
		return (NULL);

	lp = &sp->listener;
	lock_release (dp->lock);
	return (lp);
}

DDS_ReturnCode_t DDS_Subscriber_set_listener (DDS_Subscriber sp,
					      DDS_SubscriberListener *listener,
					      DDS_StatusMask mask)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_S_S_LIS, &sp, sizeof (sp));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	dcps_update_listener ((Entity_t *) sp, &dp->lock,
			      &sp->mask, &sp->listener,
			      mask, listener);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_Subscriber_get_status_changes (DDS_Subscriber sp)
{
	Domain_t	*dp;
	DDS_StatusMask	m;
	
	ctrc_printd (DCPS_ID, DCPS_S_G_STAT, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, NULL))
		return (0);

	dp = domain_ptr (sp->domain, 1, NULL);
	if (!dp)
		return (0);

	m = sp->status;
	lock_release (dp->lock);
	return (m);
}

DDS_ReturnCode_t DDS_Subscriber_enable (DDS_Subscriber sp)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_S_ENABLE, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	if ((sp->entity.flags & EF_ENABLED) == 0) {

		/* ... todo ... */

		sp->entity.flags |= EF_ENABLED | EF_NOT_IGNORED;
	}
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusCondition DDS_Subscriber_get_statuscondition (DDS_Subscriber sp)
{
	Domain_t		*dp;
	StatusCondition_t	*scp;

	ctrc_printd (DCPS_ID, DCPS_S_G_SCOND, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, NULL))
		return (NULL);

	dp = domain_ptr (sp->domain, 1, NULL);
	if (!dp)
		return (NULL);

	scp = sp->condition;
	if (!scp) {
		scp = dcps_new_status_condition ();
		if (!scp)
			return (NULL);

		scp->entity = (Entity_t *) sp;
		sp->condition = scp;
	}
	lock_release (dp->lock);
	return ((DDS_StatusCondition) scp);
}

DDS_InstanceHandle_t DDS_Subscriber_get_instance_handle (DDS_Subscriber sp)
{
	ctrc_printd (DCPS_ID, DCPS_S_G_HANDLE, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, NULL))
		return (0);

	return (sp->entity.handle);
}


typedef struct sub_gr_st {
	Subscriber_t		*sp;
	DDS_DataReaderSeq	*rseq;
	unsigned		skip;
	DDS_ReturnCode_t	ret;
} SubGReaders_t;

static int subscriber_get_reader (Skiplist_t *list, void *node, void *args)
{
	Endpoint_t		*ep, **epp = (Endpoint_t **) node;
	SubGReaders_t		*sp = (SubGReaders_t *) args;
	Reader_t		*rp;

	ARG_NOT_USED (list)

	ep = *epp;
	if (!entity_reader (entity_type (&ep->entity)) || ep->u.subscriber != sp->sp)
		return (1);

	rp = (Reader_t *) ep;
	if (!hc_avail (rp->r_cache, sp->skip))
		return (1);

	if (dds_seq_append (sp->rseq, (DDS_DataReader *) &rp)) {
		sp->ret = DDS_RETCODE_OUT_OF_RESOURCES;
		return (0);
	}
	return (1);
}

DDS_ReturnCode_t DDS_Subscriber_get_datareaders (DDS_Subscriber sp,
						 DDS_DataReaderSeq *readers,
						 DDS_SampleStateMask sample_states,
						 DDS_ViewStateMask view_states,
						 DDS_InstanceStateMask instance_states)
{
	Domain_t	*dp;
	SubGReaders_t	srd;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_S_G_DR_S, &sp, sizeof (sp));
	ctrc_contd (&readers, sizeof (readers));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!readers)
		return (DDS_RETCODE_BAD_PARAMETER);

	srd.sp = subscriber_ptr (sp, &ret);
	if (!srd.sp)
		return (ret);

	DDS_SEQ_INIT (*readers);
	srd.rseq = readers;
	srd.skip = dcps_skip_mask (sample_states, view_states, instance_states);
	srd.ret = DDS_RETCODE_OK;

	dp = srd.sp->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	sl_walk (&dp->participant.p_endpoints,
	         subscriber_get_reader,
		 &srd);
	lock_release (dp->lock);
	return (srd.ret);
}

static int subscriber_notify_reader (Skiplist_t *list, void *node, void *args)
{
	Endpoint_t		*ep, **epp = (Endpoint_t **) node;
	Subscriber_t		*sp = (Subscriber_t *) args;
	Reader_t		*rp;

	ARG_NOT_USED (list)

	ep = *epp;
	if (!entity_reader (entity_type (&ep->entity)) || ep->u.subscriber != sp)
		return (1);

	rp = (Reader_t *) ep;
	if (dcps_data_available_listener (rp) &&
	    (rp->r_status & DDS_DATA_AVAILABLE_STATUS) != 0)
		dds_notify (NSC_DCPS, (Entity_t *) rp, NT_DATA_AVAILABLE);

	else if (rp->r_conditions)
		dcps_waitset_wakeup (rp, rp->r_conditions, &sp->domain->lock);

	return (1);
}

DDS_ReturnCode_t DDS_Subscriber_notify_datareaders (DDS_Subscriber sp)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_S_NOTIF_DR, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = sp->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	sl_walk (&dp->participant.p_endpoints,
		 subscriber_notify_reader,
		 sp);
	lock_release (dp->lock);
	return (ret);
}

DDS_DomainParticipant DDS_Subscriber_get_participant (DDS_Subscriber sp)
{
	Domain_t		*dp;
	DDS_DomainParticipant	part;

	ctrc_printd (DCPS_ID, DCPS_S_G_PART, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, NULL))
		return (NULL);

	dp = domain_ptr (sp->domain, 1, NULL);
	if (!dp)
		return (NULL);

	part = sp->domain;
	lock_release (dp->lock);
	return (part);
}

DDS_ReturnCode_t DDS_Subscriber_get_default_datareader_qos (DDS_Subscriber sp,
							    DDS_DataReaderQos *qos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_S_G_DR_QOS, &sp, sizeof (sp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos) {
		log_printf (DCPS_ID, 0, "get_default_datareader_qos: invalid parameters!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	*qos = sp->def_reader_qos;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos (DDS_Subscriber sp,
							    DDS_DataReaderQos *qos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_S_S_DR_QOS, &sp, sizeof (sp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	if (qos == DDS_DATAREADER_QOS_DEFAULT)
		qos = (DDS_DataReaderQos *) &qos_def_reader_qos;
	else if (!qos_valid_reader_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	sp->def_reader_qos = *qos;

    done:
    	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_Subscriber_copy_from_topic_qos (DDS_Subscriber sp,
						     DDS_DataReaderQos *rqos,
						     DDS_TopicQos *tqos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_S_DR_F_TQOS, &sp, sizeof (sp));
	ctrc_contd (&rqos, sizeof (rqos));
	ctrc_contd (&tqos, sizeof (tqos));
	ctrc_endd ();

	if (!rqos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = domain_ptr (sp->domain, 1, &ret);
	if (!dp)
		return (ret);

	if (!tqos)
		tqos = &dp->def_topic_qos;

	rqos->durability = tqos->durability;
	rqos->deadline = tqos->deadline;
	rqos->latency_budget = tqos->latency_budget;
	rqos->liveliness = tqos->liveliness;
	rqos->reliability = tqos->reliability;
	rqos->destination_order = tqos->destination_order;
	rqos->history = tqos->history;
	rqos->resource_limits = tqos->resource_limits;
	rqos->ownership = tqos->ownership;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_DataReader DDS_Subscriber_create_datareader (DDS_Subscriber               sp,
						 DDS_TopicDescription         topic,
						 const DDS_DataReaderQos      *qos,
						 const DDS_DataReaderListener *listener,
						 DDS_StatusMask               mask)
{
	Domain_t	*dp;
	Topic_t		*tp;
	Participant_t	*pp;
	Reader_t	*rp;
	Cache_t		cp;
	EntityId_t	eid;
#ifdef DDS_NATIVE_SECURITY
	unsigned	secure;
	DDS_ReturnCode_t ret;
#endif
	const TypeSupport_t *ts;
	int		error;

	ctrc_begind (DCPS_ID, DCPS_S_C_DR, &sp, sizeof (sp));
	ctrc_contd (&topic, sizeof (topic));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	prof_start (dcps_create_reader);

	if (!subscriber_ptr (sp, NULL))
		return (NULL);

	dp = sp->domain;
	if (!dp || lock_take (dp->lock))
		return (NULL);

	if (sp->domain != dp)
		goto inv_topic; /*DDS_RETCODE_BAD_PARAMETER*/

	tp = topic_ptr (topic, 1, NULL);
	if (!tp)
		goto inv_topic;

	rp = NULL;

	/* Don't check whether reader is already created!  Multiple readers and
	   or writers on the same topic are definitely allowed! */

	if (qos == DDS_DATAREADER_QOS_DEFAULT)
		qos = &sp->def_reader_qos;
	else if (qos != DDS_DATAREADER_QOS_USE_TOPIC_QOS &&
	         !qos_valid_reader_qos (qos))
		goto done;

#ifdef DDS_SECURITY

	/* Check if security policy allows this datareader. */
#ifdef DDS_NATIVE_SECURITY
	if (sec_check_create_reader (dp->participant.p_permissions,
				     str_ptr (tp->name),
				     NULL, qos, sp->qos.partition,
				     NULL, &secure)) {
#else
	if (check_create_reader (dp->participant.p_permissions, tp,
						qos, sp->qos.partition)) {
#endif
		log_printf (DCPS_ID, 0, "create_datareader: reader create not allowed!\r\n");
		goto done;
	}
#endif

	/* Create an Entity Identifier. */
	ts = tp->type->type_support;
	if ((sp->entity.flags & EF_BUILTIN) != 0) {
		eid.id [0] = eid.id [1] = 0;
		eid.id [2] = 0x10 + tp->type->index;
		eid.id [ENTITY_KIND_INDEX] = ENTITY_KIND_BUILTIN | ENTITY_KIND_READER_KEY;
	}
	else {
		++dcps_entity_count;
		eid.id [0] = (dcps_entity_count >> 16) & 0xff;
		eid.id [1] = (dcps_entity_count >> 8) & 0xff;
		eid.id [2] = dcps_entity_count & 0xff;
		if (ts->ts_keys)
			eid.id [ENTITY_KIND_INDEX] = ENTITY_KIND_USER | ENTITY_KIND_READER_KEY;
		else
			eid.id [ENTITY_KIND_INDEX] = ENTITY_KIND_USER | ENTITY_KIND_READER;
	}
	pp = &dp->participant;
	rp = (Reader_t *) endpoint_create (pp, sp, &eid, NULL);
	if (!rp) {
		warn_printf ("create_datareader: out of memory for DataReader structure.\r\n");
		goto done;
	}
#ifdef RW_LOCKS
	lock_take (rp->r_lock);
#endif
	if ((sp->entity.flags & EF_BUILTIN) != 0)
		rp->r_flags |= EF_BUILTIN;

	if (qos == DDS_DATAREADER_QOS_USE_TOPIC_QOS) {
		rp->r_qos = tp->qos;
		tp->qos->users++;
		qos_init_time_based_filter (&rp->r_time_based_filter);
		qos_init_reader_data_lifecycle (&rp->r_data_lifecycle);
	}
	else {
		rp->r_qos = qos_reader_new (qos);
		rp->r_time_based_filter = qos->time_based_filter;
		rp->r_data_lifecycle = qos->reader_data_lifecycle;
	}
	if (!rp->r_qos)
		goto free_pool;

	rp->r_mask = mask;
	if (listener)
		rp->r_listener = *listener;
	else
		memset (&rp->r_listener, 0, sizeof (rp->r_listener));

	memset (&rp->r_rdm_status, 0, sizeof (rp->r_rdm_status));
	memset (&rp->r_riq_status, 0, sizeof (rp->r_riq_status));
	memset (&rp->r_sl_status, 0, sizeof (rp->r_sl_status));
	memset (&rp->r_sr_status, 0, sizeof (rp->r_sr_status));
	memset (&rp->r_lc_status, 0, sizeof (rp->r_lc_status));
	memset (&rp->r_sm_status, 0, sizeof (rp->r_sm_status));
	rp->r_changes.buffer = NULL;
	rp->r_changes.length = rp->r_changes.maximum = 0;
	rp->r_prev_info = NULL;
	rp->r_prev_data = NULL;
	rp->r_n_prev = 0;
	rp->r_topic = tp;
	rp->r_next = tp->readers;
	tp->readers = &rp->r_ep;

#ifdef DDS_NATIVE_SECURITY

	/* Set security attributes. */
	rp->r_access_prot = 0;
	rp->r_disc_prot = 0;
	rp->r_submsg_prot = 0;
	rp->r_payload_prot = 0;
	rp->r_crypto_type = 0;
	rp->r_crypto = 0;
	if (secure && 
	    (dp->participant.p_sec_caps & (SECC_DDS_SEC | (SECC_DDS_SEC << SECC_LOCAL))) != 0) {
		log_printf (DCPS_ID, 0, "DDS: Reader security attributes: 0x%x\r\n", secure);
		rp->r_access_prot = (secure & DDS_SECA_ACCESS_PROTECTED) != 0;
		rp->r_disc_prot = (secure & DDS_SECA_DISC_PROTECTED) != 0;
		rp->r_submsg_prot = (secure & DDS_SECA_SUBMSG_PROTECTED) != 0;
		rp->r_payload_prot = (secure & DDS_SECA_PAYLOAD_PROTECTED) != 0;
		if (rp->r_submsg_prot || rp->r_payload_prot) {
			rp->r_crypto_type = secure >> DDS_SECA_ENCRYPTION_SHIFT;
			rp->r_crypto = sec_register_local_datareader (dp->participant.p_crypto,
								             rp, &ret);
		}
	}
#endif

	/* Allocate a history cache. */
	rp->r_cache = cp = hc_new (&rp->r_lep, ts != NULL);
	if (!cp) {
		warn_printf ("create_datareader: out of memory for history cache!\r\n");
		goto free_node;
	}
	sp->nreaders++;

	error = hc_request_notification (rp->r_cache, dcps_data_available, (uintptr_t) rp);
	if (error)
		warn_printf ("create_data_reader: RTPS reader data available listener registration problem.\r\n");

#ifdef RW_LOCKS
    	lock_release (rp->r_lock);
#endif
    	lock_release (tp->lock);
    	lock_release (dp->lock);

	if (!sp->qos.no_autoenable)
		DDS_DataReader_enable ((DDS_DataReader) rp);
		
	prof_stop (dcps_create_reader, 1);
	return (rp);

    free_node:
	tp->readers = rp->r_next;
    	qos_free (rp->r_qos);

    free_pool:
	endpoint_delete (pp, &rp->r_ep);
	rp = NULL;

    done:
    	lock_release (tp->lock);

    inv_topic:
	lock_release (dp->lock);
	return (NULL);
}

static DDS_ReturnCode_t delete_datareader_l (Subscriber_t *sp, Reader_t *rp, int all)
{
	Endpoint_t	*ep, *prev_ep;
	Topic_t		*tp;
	Domain_t	*dp = sp->domain;
	Condition_t	*cp;
	unsigned	i;
	DDS_ReturnCode_t ret;

	if (all)
		DDS_DataReader_delete_contained_entities (rp);

	tp = rp->r_topic;
	if (lock_take (tp->lock)) {
		log_printf (DCPS_ID, 0, "delete_datareader: can't take topic lock!\r\n");
		return (DDS_RETCODE_ERROR);
	}
#ifdef RW_LOCKS
	lock_take (rp->r_lock);
#endif
	if (rp->r_changes.length) {
		log_printf (DCPS_ID, 0, "delete_datareader: has outstanding loans.\r\n");
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto no_reader_locked;
	}

	/* Still exists? */
	for (prev_ep = NULL, ep = tp->readers;
	     ep != &rp->r_ep;
	     prev_ep = ep, ep = ep->next)
		;
	if (!ep) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto no_reader_locked;
	}

	/* Check if still conditions attached to reader, which is only allowed
	   from a *_delete_contained_entities() function call. */
	if (!all)
		for (cp = rp->r_conditions; cp; cp = cp->e_next)
			if (cp->class == CC_READ || cp->class == CC_QUERY) {
				ret = DDS_RETCODE_PRECONDITION_NOT_MET;
				goto no_reader_locked;
			}

	if (!dds_purge_notifications ((Entity_t *) rp, DDS_ALL_STATUS, 1)) {
		log_printf (DCPS_ID, 0, "delete_datareader_l: has an active listener.\r\n");
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto no_reader_locked;
	}

	/* Reader definitely exists and all locks taken. */

#if 0
	/* Reader definitely exists and all locks taken. */
	printf ("DDS: deleting datareader (%s/%s)\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));
#endif
	/* Signal that reader is shutting down. */
	rp->r_flags |= EF_SHUTDOWN;

	/* Remove subscription endpoint from discovery subsystem. */
	if ((sp->entity.flags & EF_BUILTIN) == 0 && (rp->r_flags & EF_ENABLED) != 0)
		disc_reader_remove (dp, rp);

#ifdef RTPS_USED

	/* Remove reader endpoint from RTPS subsystem. */
	if (rtps_used && rp->r_rtps)
		rtps_reader_delete (rp);
#endif

	rp->r_flags &= ~EF_ENABLED;

	/* Delete StatusCondition if it exists. */
	if (rp->r_conditions) {
		dcps_delete_status_condition (rp->r_conditions);
		rp->r_conditions = NULL;
	}
	sp->nreaders--;

	/* Check if still changes unfreed. */
	if (rp->r_changes.maximum)
		xfree (rp->r_changes.buffer);
	if (rp->r_riq_status.policies)
		xfree (rp->r_riq_status.policies);

	/* Delete history cache. */
	hc_free (rp->r_cache);

	/* Remove QoS parameters. */
	qos_reader_free (rp->r_qos);

	/* Remove endpoint from topic list. */
	if (prev_ep)
		prev_ep->next = ep->next;
	else
		tp->readers = ep->next;

	/* If we still have attached receive buffers, release them. */
	if (rp->r_n_prev) {
		for (i = 0; i < rp->r_n_prev; i++)
			mds_pool_free (&dcps_mem_blocks [MB_SAMPLE_INFO],
				       rp->r_prev_info [i]);
		xfree (rp->r_prev_info);
		rp->r_prev_info = NULL;
		xfree (rp->r_prev_data);
		rp->r_prev_data = NULL;
		rp->r_n_prev = 0;
	}

	/* And finally: delete domain endpoint. */
	endpoint_delete (&dp->participant, &rp->r_ep);
	lock_release (tp->lock);
	return (DDS_RETCODE_OK);

    no_reader_locked:
#ifdef RW_LOCKS
	lock_release (rp->r_lock);
#endif
	lock_release (tp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_Subscriber_delete_datareader (DDS_Subscriber sp,
						   DDS_DataReader rp)
{
	Domain_t	*dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_S_D_DR, &sp, sizeof (sp));
	ctrc_contd (&rp, sizeof (rp));
	ctrc_endd ();

	prof_start (dcps_delete_reader);

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = sp->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	if (!reader_ptr (rp, 0, &ret))
		goto no_reader;

	if (rp->r_subscriber != sp) {
		log_printf (DCPS_ID, 0, "delete_datareader: invalid parameters!\r\n");
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto no_reader;
	}
	ret = delete_datareader_l (sp, rp, 0);

    no_reader:
	lock_release (dp->lock);

	prof_stop (dcps_delete_reader, 1);
	return (ret);
}

DDS_DataReader DDS_Subscriber_lookup_datareader (DDS_Subscriber sp,
						 const char *topic_name)
{
	Domain_t	*dp;
	Topic_t		*tp;
	Endpoint_t	*ep;

	ctrc_begind (DCPS_ID, DCPS_S_L_DR, &sp, sizeof (sp));
	ctrc_contd (topic_name, strlen (topic_name));
	ctrc_endd ();

	if (!subscriber_ptr (sp, NULL))
		return (NULL);

	dp = sp->domain;
	if (!dp || lock_take (dp->lock))
		return (NULL);

	tp = topic_lookup (&dp->participant, topic_name);
	if (!tp) {
#ifdef DCPS_BUILTIN_READERS
		/* Create a builtin reader if possible for builtin topics. */
		if ((sp->entity.flags & EF_BUILTIN) != 0)
			ep = dcps_new_builtin_reader (dp, topic_name);
		else
#endif
			ep = NULL;
		lock_release (dp->lock);
		return ((DDS_DataReader) ep);
	}
	lock_take (tp->lock);
	for (ep = tp->readers; ep; ep = ep->next)
		if ((ep->entity.flags & EF_LOCAL) != 0 && ep->u.subscriber == sp)
			break;

	lock_release (tp->lock);
	lock_release (dp->lock);
	return ((DDS_DataReader) ep);
}

DDS_ReturnCode_t DDS_Subscriber_begin_access (DDS_Subscriber sp)
{
	DDS_ReturnCode_t 	ret;

	ctrc_printd (DCPS_ID, DCPS_S_B_ACC, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t DDS_Subscriber_end_access (DDS_Subscriber sp)
{
	DDS_ReturnCode_t 	ret;

	ctrc_printd (DCPS_ID, DCPS_S_E_ACC, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	return (DDS_RETCODE_UNSUPPORTED);
}

static int delete_reader (Skiplist_t *list, void *node, void *arg)
{
	Reader_t	*rp, **rpp = (Reader_t **) node;
	Subscriber_t	*sp = (Subscriber_t *) arg;

	ARG_NOT_USED (list)

	rp = *rpp;
	if (rp->r_subscriber == sp) {
		dtrc_printf ("delete_subscriber_entitities: delete DataReader (%s)\r\n", 
						str_ptr (rp->r_topic->name));
		delete_datareader_l (sp, rp, 1);
	}
	return (sp->nreaders != 0);
}

void delete_subscriber_entities (Domain_t *dp, Subscriber_t *sp)
{
	unsigned	delta, delay = DDS_get_purge_delay ();

	while (sp->nreaders) {

		/* Delete all attached readers. */
		sl_walk (&dp->participant.p_endpoints, delete_reader, sp);

		/* If all deleted or no delay or waited enough, just exit. */
		if (!sp->nreaders || !delay)
			break;

		/* If delay is speficied we retry the deletions every 1ms. */
		if (delay >= 1000)
			delta = 1000;
		else
			delta = delay;
		usleep (delta);
		delay -= delta;
	}
}

DDS_ReturnCode_t DDS_Subscriber_delete_contained_entities (DDS_Subscriber sp)
{
	Domain_t		*dp;
	DDS_ReturnCode_t 	ret;

	ctrc_printd (DCPS_ID, DCPS_S_D_CONT, &sp, sizeof (sp));

	if (!subscriber_ptr (sp, &ret))
		return (ret);

	dp = sp->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ERROR);

	delete_subscriber_entities (dp, sp);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}


