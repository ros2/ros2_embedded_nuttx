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

/* dcps_pub.c -- DCPS API - Publisher functions. */

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
#include "domain.h"
#include "dds.h"
#include "disc.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "dcps_event.h"
#include "dcps_pub.h"
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
#include "sec_access.h"
#include "sec_crypto.h"
#else
#include "security.h"
#endif
#endif

static DDS_ReturnCode_t delete_datawriter_l (Publisher_t *up, Writer_t *wp);

static int delete_writer (Skiplist_t *list, void *node, void *arg)
{
	Writer_t	*wp, **wpp = (Writer_t **) node;
	Publisher_t	*pp = (Publisher_t *) arg;

	ARG_NOT_USED (list)

	wp = *wpp;
	if (wp->w_publisher == pp) {
		dtrc_printf ("delete_publisher_entitities: delete DataWriter (%s)\r\n", 
						str_ptr (wp->w_topic->name));
		delete_datawriter_l (pp, wp);
	}
	return (pp->nwriters != 0);
}

void delete_publisher_entities (Domain_t *dp, Publisher_t *up)
{
	unsigned	delta, delay = DDS_get_purge_delay ();

	while (up->nwriters) {

		/* Delete all attached writers. */
		sl_walk (&dp->participant.p_endpoints, delete_writer, up);

		/* If all deleted or no delay or waited enough, just exit. */
		if (!up->nwriters || !delay)
			break;

		/* If delay is specified we retry the deletions every 1ms. */
		if (delay >= 1000)
			delta = 1000;
		else
			delta = delay;
		usleep (delta);
		delay -= delta;
	}
}

DDS_ReturnCode_t DDS_Publisher_get_qos (DDS_Publisher up,
					DDS_PublisherQos *qos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_G_QOS, &up, sizeof (up));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	qos_publisher_get (&up->qos, qos);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

void dcps_suspended_publication_add (Publisher_t *pp, Writer_t *wp, int new)
{
	Writer_t	*prev, *xwp;

	for (prev = NULL, xwp = pp->suspended;
	     xwp;
	     prev = xwp, xwp = xwp->w_next_s)
		;
	if (prev)
		prev->w_next_s = wp;
	else
		pp->suspended = wp;
	wp->w_next_s = NULL;
	wp->w_flags |= EF_PUBLISH;
	if (new)
		wp->w_flags |= EF_NEW;
}

void dcps_suspended_publication_remove (Publisher_t *pp, Writer_t *wp)
{
	Writer_t	*prev_wp, *xwp;

	for (prev_wp = NULL, xwp = pp->suspended;
	     xwp;
	     prev_wp = xwp, xwp = xwp->w_next_s)
		if (xwp == wp) {
			if (prev_wp)
				prev_wp->w_next_s = wp->w_next_s;
			else
				pp->suspended = wp->w_next_s;
			wp->w_flags &= ~(EF_PUBLISH | EF_NEW);
			break;
		}
}

static void unsuspend_publications (Publisher_t *pp)
{
	Writer_t	*wp;

	while (pp->suspended) {
		wp = pp->suspended;
		pp->suspended = wp->w_next_s;
		wp->w_next_s = NULL;
		wp->w_flags &= ~EF_PUBLISH;
		if ((wp->w_flags & EF_NEW) != 0) {
			wp->w_flags &= ~EF_NEW;
			disc_writer_add (pp->domain, wp);
		}
		else {
			hc_qos_update (wp->w_cache);
			disc_writer_update (pp->domain, wp, 1, 0);
		}
	}
}

int dcps_update_writer_qos (Skiplist_t *list, void *node, void *arg)
{
	Writer_t        *wp, **wpp = (Writer_t **) node;
	Publisher_t     *pp = (Publisher_t *) arg;

	ARG_NOT_USED (list)     

	wp = *wpp;

	if (wp->w_publisher == pp &&
	    (wp->w_flags & EF_ENABLED) != 0) {
		lock_take (wp->w_topic->lock);
#ifdef RW_LOCKS
		lock_take (wp->w_lock);
#endif
		if ((wp->w_publisher->entity.flags & EF_SUSPEND) == 0) {
			hc_qos_update (wp->w_cache);
			disc_writer_update (pp->domain, wp, 1, 0);
		}
		else if ((wp->w_flags & EF_PUBLISH) == 0)
			dcps_suspended_publication_add (pp, wp, 0);
#ifdef RW_LOCKS
		lock_release (wp->w_lock);
#endif
		lock_release (wp->w_topic->lock);
	}
	return (1);
}

DDS_ReturnCode_t DDS_Publisher_set_qos (DDS_Publisher up,
					DDS_PublisherQos *qos)
{
	Domain_t	*dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_S_QOS, &up, sizeof (up));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = up->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	if (qos == DDS_PUBLISHER_QOS_DEFAULT)
		qos = &up->domain->def_publisher_qos;
	else if (!qos_valid_publisher_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = qos_publisher_update (&up->qos, qos);
	if (ret == DDS_RETCODE_OK)
		sl_walk (&up->domain->participant.p_endpoints, dcps_update_writer_qos, up);

    done:
    	lock_release (dp->lock);
	return (ret);
}

DDS_PublisherListener *DDS_Publisher_get_listener (DDS_Publisher up)
{
	ctrc_printd (DCPS_ID, DCPS_P_G_LIS, &up, sizeof (up));

	if (!publisher_ptr (up, NULL))
		return (NULL);

	return (&up->listener);
}

DDS_ReturnCode_t DDS_Publisher_set_listener (DDS_Publisher up,
					     DDS_PublisherListener *listener,
					     DDS_StatusMask mask)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_S_LIS, &up, sizeof (up));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	dcps_update_listener ((Entity_t *) up, &dp->lock,
			      &up->mask, &up->listener,
			      mask, listener);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_Publisher_get_status_changes (DDS_Publisher up)
{
	Domain_t	*dp;
	DDS_StatusMask	m;
	
	ctrc_printd (DCPS_ID, DCPS_P_G_STAT, &up, sizeof (up));

	if (!publisher_ptr (up, NULL))
		return (0);

	dp = domain_ptr (up->domain, 1, NULL);
	if (!dp)
		return (0);

	/* No status mask on publisher for now! */
	m = 0;

	lock_release (dp->lock);
	return (m);
}

DDS_ReturnCode_t DDS_Publisher_enable (DDS_Publisher up)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_P_ENABLE, &up, sizeof (up));

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	if ((dp->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (dp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	if ((up->entity.flags & EF_ENABLED) == 0) {

		/* ... todo ... */

		up->entity.flags |= EF_ENABLED | EF_NOT_IGNORED;
	}
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusCondition DDS_Publisher_get_statuscondition (DDS_Publisher up)
{
	Domain_t		*dp;
	StatusCondition_t	*scp;

	ctrc_printd (DCPS_ID, DCPS_P_G_SCOND, &up, sizeof (up));

	if (!publisher_ptr (up, NULL))
		return (NULL);

	dp = domain_ptr (up->domain, 1, NULL);
	if (!dp)
		return (NULL);

	scp = up->condition;
	if (!scp) {
		scp = dcps_new_status_condition ();
		if (!scp)
			return (NULL);

		scp->entity = (Entity_t *) up;
		up->condition = scp;
	}
	lock_release (dp->lock);
	return ((DDS_StatusCondition) scp);
}

DDS_InstanceHandle_t DDS_Publisher_get_instance_handle (DDS_Publisher up)
{
	ctrc_printd (DCPS_ID, DCPS_P_G_HANDLE, &up, sizeof (up));

	if (!publisher_ptr (up, NULL))
		return (0);

	return (up->entity.handle);
}

DDS_ReturnCode_t DDS_Publisher_delete_contained_entities (DDS_Publisher up)
{
	Domain_t		*dp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_P_D_CONT, &up, sizeof (up));

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = up->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	delete_publisher_entities (dp, up);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

typedef struct pub_gw_st {
	Publisher_t		*up;
	DDS_InstanceHandleSeq	seq;
} PubGWriters_t;

static int publisher_get_writer_handle (Skiplist_t *list, void *node, void *args)
{
	Endpoint_t		*ep, **epp = (Endpoint_t **) node;
	PubGWriters_t		*sp = (PubGWriters_t *) args;
	DDS_InstanceHandle_t	h;

	ARG_NOT_USED (list)

	ep = *epp;
	if (!entity_writer (entity_type (&ep->entity)) || ep->u.publisher != sp->up)
		return (1);

	h = ep->entity.handle;
	if (dds_seq_append (&sp->seq, &h))
		return (0);

	return (1);
}

DDS_ReturnCode_t DDS_Publisher_wait_for_acknowledgments (DDS_Publisher  up,
							 DDS_Duration_t *max_wait)
{
	Domain_t		*dp;
	Duration_t		duration;
	Ticks_t			now, d, end_time, ticks;
	unsigned		i;
	PubGWriters_t		pgw;
	Entity_t		*ep;
	Writer_t		*wp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_P_WACK, &up, sizeof (up));
	ctrc_contd (max_wait, sizeof (*max_wait));
	ctrc_endd ();

	pgw.up = publisher_ptr (up, &ret);
	if (!pgw.up)
		return (ret);

	dp = pgw.up->domain;
	ticks = duration2ticks ((Duration_t *) max_wait);
	now = sys_getticks ();
	end_time = now + ticks;

	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	DDS_SEQ_INIT (pgw.seq);
	sl_walk (&dp->participant.p_endpoints,
		 publisher_get_writer_handle,
		 &pgw);
	lock_release (dp->lock);
	ret = DDS_RETCODE_OK;
	for (i = 0; i < DDS_SEQ_LENGTH (pgw.seq); i++) {
		ep = entity_ptr (DDS_SEQ_ITEM (pgw.seq, i));
		if (!ep) /* Already gone? */
			continue;

		wp = writer_ptr ((DDS_DataWriter) ep, 1, &ret);
		if (!wp) /* Gone already? */
			continue;

		now = sys_getticks ();
		d = end_time - now;
		if (d >= 0x7fffffffUL) {
			ret = DDS_RETCODE_TIMEOUT;
			lock_release (wp->w_lock);
			break;
		}
		duration.secs = d / TICKS_PER_SEC;
		duration.nanos = (d % TICKS_PER_SEC) * TMR_UNIT_MS * 1000000;
		ret = hc_wait_acks (wp->w_cache, &duration);
		lock_release (wp->w_lock);
		if (ret)
			break;
	}
	dds_seq_cleanup (&pgw.seq);
	return (ret);
}

DDS_DomainParticipant DDS_Publisher_get_participant (DDS_Publisher up)
{
	DDS_DomainParticipant	part;

	ctrc_printd (DCPS_ID, DCPS_P_G_PART, &up, sizeof (up));

	if (!publisher_ptr (up, NULL))
		return (NULL);

	if (up->domain)
		part = up->domain;
	else
		part = 0;
	return (part);
}

DDS_ReturnCode_t DDS_Publisher_get_default_datawriter_qos (DDS_Publisher up,
							   DDS_DataWriterQos *qos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_G_DW_QOS, &up, sizeof (up));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos) {
		log_printf (DCPS_ID, 0, "get_default_datawriter_qos: invalid parameters!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	*qos = up->def_writer_qos;
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos (DDS_Publisher up,
							   DDS_DataWriterQos *qos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_S_DW_QOS, &up, sizeof (up));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	if (qos == DDS_DATAWRITER_QOS_DEFAULT)
		qos = (DDS_DataWriterQos *) &qos_def_writer_qos;
	else if (!qos_valid_writer_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	up->def_writer_qos = *qos;

    done:
    	lock_release (dp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_Publisher_copy_from_topic_qos (DDS_Publisher up,
						    DDS_DataWriterQos *wqos,
						    DDS_TopicQos *tqos)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_DW_F_TQOS, &up, sizeof (up));
	ctrc_contd (&wqos, sizeof (wqos));
	ctrc_contd (&tqos, sizeof (tqos));
	ctrc_endd ();

	if (!wqos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	if (!tqos)
		tqos = &dp->def_topic_qos;

	wqos->durability = tqos->durability;
	wqos->durability_service = tqos->durability_service;
	wqos->deadline = tqos->deadline;
	wqos->latency_budget = tqos->latency_budget;
	wqos->liveliness = tqos->liveliness;
	wqos->reliability = tqos->reliability;
	wqos->destination_order = tqos->destination_order;
	wqos->history = tqos->history;
	wqos->resource_limits = tqos->resource_limits;
	wqos->transport_priority = tqos->transport_priority;
	wqos->lifespan = tqos->lifespan;
	wqos->ownership = tqos->ownership;

	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Publisher_suspend_publications (DDS_Publisher up)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_printd (DCPS_ID, DCPS_P_SUSP, &up, sizeof (up));

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	if ((up->entity.flags & EF_SUSPEND) == 0) {
		up->entity.flags |= EF_SUSPEND;
		up->suspended = NULL;
	}
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Publisher_resume_publications (DDS_Publisher up)
{
	Domain_t	 *dp;
	DDS_ReturnCode_t ret;

	ctrc_printd (DCPS_ID, DCPS_P_RES, &up, sizeof (up));

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = domain_ptr (up->domain, 1, &ret);
	if (!dp)
		return (ret);

	if ((up->entity.flags & EF_SUSPEND) != 0) {
		up->entity.flags &= ~EF_SUSPEND;
		unsuspend_publications (up);
	}
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Publisher_begin_coherent_changes (DDS_Publisher up)
{
	DDS_ReturnCode_t ret;

	ctrc_printd (DCPS_ID, DCPS_P_BC, &up, sizeof (up));

	if (!publisher_ptr (up, &ret))
		return (ret);

	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t DDS_Publisher_end_coherent_changes (DDS_Publisher up)
{
	DDS_ReturnCode_t ret;

	ctrc_printd (DCPS_ID, DCPS_P_EC, &up, sizeof (up));

	if (!publisher_ptr (up, &ret))
		return (ret);

	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_DataWriter DDS_Publisher_create_datawriter (DDS_Publisher                up,
						DDS_Topic                    tp,
						const DDS_DataWriterQos      *qos,
						const DDS_DataWriterListener *listener,
						DDS_StatusMask               mask)
{
	Domain_t	*dp;
	Participant_t	*pp;
	Writer_t	*wp;
	Cache_t		cp;
	EntityId_t	eid;
#ifdef DDS_NATIVE_SECURITY
	unsigned	secure;
	DDS_ReturnCode_t ret;
#endif
	const TypeSupport_t *ts;

	ctrc_begind (DCPS_ID, DCPS_P_C_DW, &up, sizeof (up));
	ctrc_contd (&tp, sizeof (tp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	prof_start (dcps_create_writer);

	if (!publisher_ptr (up, NULL))
		return (NULL);

	dp = up->domain;
	if (lock_take (dp->lock))
		return (NULL);

	if (!topic_ptr (tp, 1, NULL))
		goto inv_topic;

	wp = NULL;
	if (up->domain != dp)
		goto done; /*DDS_RETCODE_BAD_PARAMETER*/

	/* Don't check whether writer is already created!  Multiple readers and
	   or writers on the same topic, even on the same subscriber are
	   definitely allowed! */

	if (qos == DDS_DATAWRITER_QOS_DEFAULT)
		qos = &up->def_writer_qos;
	else if (qos != DDS_DATAWRITER_QOS_USE_TOPIC_QOS &&
		 !qos_valid_writer_qos (qos))
		goto done;

#ifdef DDS_SECURITY

	/* Check if security policy allows this datawriter. */

#ifdef DDS_NATIVE_SECURITY
	if (sec_check_create_writer (dp->participant.p_permissions,
				     str_ptr (tp->name),
				     NULL, qos, up->qos.partition,
				     NULL, &secure)) {
#else
	if (check_create_writer (dp->participant.p_permissions, tp,
						qos, up->qos.partition)) {
#endif
		log_printf (DCPS_ID, 0, "create_data_writer: reader create not allowed!\r\n");
		goto done;
	}
#endif

	/* Create an Entity Identifier. */
	++dcps_entity_count;
	eid.id [0] = (dcps_entity_count >> 16) & 0xff;
	eid.id [1] = (dcps_entity_count >> 8) & 0xff;
	eid.id [2] = dcps_entity_count & 0xff;
	ts = tp->type->type_support;
	if (ts->ts_keys)
		eid.id [ENTITY_KIND_INDEX] = ENTITY_KIND_USER | ENTITY_KIND_WRITER_KEY;
	else
		eid.id [ENTITY_KIND_INDEX] = ENTITY_KIND_USER | ENTITY_KIND_WRITER;

	pp = &dp->participant;
	wp = (Writer_t *) endpoint_create (pp, up, &eid, NULL);
	if (!wp) {
		warn_printf ("create_data_writer: out of memory for domain writer structure.\r\n");
		goto done;
	}
#ifdef RW_LOCKS
	lock_take (wp->w_lock);
#endif
	if (qos == DDS_DATAWRITER_QOS_USE_TOPIC_QOS) {
		wp->w_qos = tp->qos;
		tp->qos->users++;
	}
	else
		wp->w_qos = qos_writer_new (qos);
	if (!wp->w_qos)
		goto free_pool;

	wp->w_mask = mask;
	if (listener)
		wp->w_listener = *listener;
	else
		memset (&wp->w_listener, 0, sizeof (wp->w_listener));

	/* Allocate a history cache. */
	memset (&wp->w_odm_status, 0, sizeof (wp->w_odm_status));
	memset (&wp->w_oiq_status, 0, sizeof (wp->w_oiq_status));
	memset (&wp->w_ll_status, 0, sizeof (wp->w_ll_status));
	memset (&wp->w_pm_status, 0, sizeof (wp->w_pm_status));
	wp->w_topic = tp;
	wp->w_next = tp->writers;
	tp->writers = &wp->w_ep;

#ifdef DDS_NATIVE_SECURITY

	/* Set security attributes. */
	wp->w_access_prot = 0;
	wp->w_disc_prot = 0;
	wp->w_submsg_prot = 0;
	wp->w_payload_prot = 0;
	wp->w_crypto_type = 0;
	wp->w_crypto = 0;
	if (secure && 
	    (dp->participant.p_sec_caps & (SECC_DDS_SEC | (SECC_DDS_SEC << SECC_LOCAL))) != 0) {
		log_printf (DCPS_ID, 0, "DDS: Writer security attributes: 0x%x\r\n", secure);
		wp->w_access_prot = (secure & DDS_SECA_ACCESS_PROTECTED) != 0;
		wp->w_disc_prot = (secure & DDS_SECA_DISC_PROTECTED) != 0;
		wp->w_submsg_prot = (secure & DDS_SECA_SUBMSG_PROTECTED) != 0;
		wp->w_payload_prot = (secure & DDS_SECA_PAYLOAD_PROTECTED) != 0;
		if (wp->w_submsg_prot || wp->w_payload_prot) {
			wp->w_crypto_type = secure >> DDS_SECA_ENCRYPTION_SHIFT;
			wp->w_crypto = sec_register_local_datawriter (dp->participant.p_crypto,
								             wp, &ret);
		}
	}
#endif

	/* Create history cache. */
	wp->w_cache = cp = hc_new (&wp->w_lep, 1);
	if (!cp) {
		warn_printf ("create_data_writer: out of memory for history cache!\r\n");
		goto free_node;
	}
	up->nwriters++;

#ifdef RW_LOCKS
    	lock_release (wp->w_lock);
#endif
    	lock_release (tp->lock);
    	lock_release (dp->lock);

	if (!up->qos.no_autoenable)
		DDS_DataWriter_enable ((DDS_DataWriter) wp);

	prof_stop (dcps_create_writer, 1);
	return (wp);

    free_node:
	tp->writers = wp->w_next;
	qos_free (wp->w_qos);

    free_pool:
#ifdef RW_LOCKS
    	lock_release (wp->w_lock);
#endif
	endpoint_delete (pp, &wp->w_ep);

    done:
    	lock_release (tp->lock);

    inv_topic:
    	lock_release (dp->lock);
	return (NULL);
}

/* delete_datawriter_l -- Delete a DataWriter with DP and P locks taken. */

static DDS_ReturnCode_t delete_datawriter_l (Publisher_t *up, Writer_t *wp)
{
	Topic_t		*tp;
	Endpoint_t	*ep, *prev_ep;
	Domain_t	*dp = up->domain;
	DDS_ReturnCode_t ret;

	tp = wp->w_topic;
	if (lock_take (tp->lock)) {
		log_printf (DCPS_ID, 0, "delete_datawriter: can't take topic lock!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

#ifdef RW_LOCKS
	lock_take (wp->w_lock);
#endif

	/* Check if it still exists? */
	for (prev_ep = NULL, ep = tp->writers;
	     ep && ep != &wp->w_ep;
	     prev_ep = ep, ep = ep->next)
		;
	if (!ep) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto no_writer_locked;
	}

	/* Remove outstanding listener callbacks. */
        if (!dds_purge_notifications ((Entity_t *) wp, DDS_ALL_STATUS, 1)) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		log_printf (DCPS_ID, 0, "delete_datawriter_l: active listener - can't delete!\r\n");
		goto no_writer_locked;
	}

	/* Writer definitely exists, all locks taken and no active listeners. */

	/* Signal that writer is shutting down. */
	wp->w_flags |= EF_SHUTDOWN;
#if 0
	printf ("DDS: deleting datawriter (%s/%s)\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));
#endif

	/* Remove publication endpoint from discovery subsystem. */
	if ((up->entity.flags & EF_BUILTIN) == 0 && (wp->w_flags & EF_ENABLED) != 0)
		disc_writer_remove (dp, wp);

#ifdef RTPS_USED

	/* Remove writer endpoint from RTPS subsystem. */
	if (rtps_used && wp->w_rtps)
		rtps_writer_delete (wp);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

	/* Unregister for crypto operations. */
	if (wp->w_crypto)
		sec_unregister_datawriter (wp->w_crypto);
#endif

	/* If publisher was suspended, remove from suspend list. */
	if ((wp->w_flags & EF_PUBLISH) != 0)
		dcps_suspended_publication_remove (up, wp);

	wp->w_flags &= ~EF_ENABLED;

	up->nwriters--;

	/* Delete StatusCondition if it exists. */
	if (wp->w_condition) {
		dcps_delete_status_condition (wp->w_condition);
		wp->w_condition = NULL;
	}

	/* Delete history cache. */
	hc_free (wp->w_cache);

	/* Remove QoS parameters. */
	qos_writer_free (wp->w_qos);
	if (wp->w_oiq_status.policies)
		xfree (wp->w_oiq_status.policies);

	/* Remove endpoint from topic endpoints list. */
	if (prev_ep)
		prev_ep->next = ep->next;
	else
		tp->writers = ep->next;

	/* And finally: delete domain endpoint. */
	endpoint_delete (&dp->participant, &wp->w_ep);
	ret = DDS_RETCODE_OK;

    no_writer_locked:
#ifdef RW_LOCKS
	lock_release (wp->w_lock);
#endif
	lock_release (tp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_Publisher_delete_datawriter (DDS_Publisher  up,
						  DDS_DataWriter wp)
{
	Domain_t	*dp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_P_D_DW, &up, sizeof (up));
	ctrc_contd (&wp, sizeof (wp));
	ctrc_endd ();

	prof_start (dcps_delete_writer_p);

	if (!publisher_ptr (up, &ret))
		return (ret);

	dp = up->domain;
	if (!dp || lock_take (dp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

	if (!writer_ptr (wp, 0, &ret))
		goto no_writer;

	if (wp->w_publisher != up) {
		log_printf (DCPS_ID, 0, "delete_datawriter: invalid parameters!\r\n");
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto no_writer;
	}
	ret = delete_datawriter_l (up, wp);

    no_writer:
	lock_release (dp->lock);
	prof_stop (dcps_delete_writer_p, 1);
	return (ret);
}

DDS_DataWriter DDS_Publisher_lookup_datawriter (DDS_Publisher up,
						const char *topic_name)
{
	Domain_t	*dp;
	Topic_t		*tp;
	Endpoint_t	*ep;

	ctrc_begind (DCPS_ID, DCPS_P_L_DW, &up, sizeof (up));
	ctrc_contd (topic_name, strlen (topic_name));
	ctrc_endd ();

	if (!publisher_ptr (up, NULL))
		return (NULL);

	dp = up->domain;
	if (!dp || lock_take (dp->lock))
		return (NULL);

	tp = topic_lookup (&dp->participant, topic_name);
	if (!tp) {
		lock_release (dp->lock);
		return (NULL);
	}
	lock_take (tp->lock);
	for (ep = tp->writers; ep; ep = ep->next)
		if ((ep->entity.flags & EF_LOCAL) != 0 && ep->u.publisher == up)
			break;

	lock_release (tp->lock);
	lock_release (dp->lock);
	return ((DDS_DataWriter) ep);
}


