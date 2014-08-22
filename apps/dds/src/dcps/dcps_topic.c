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

/* dcps_topic.c -- DCPS API - Topic functions. */

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
#include "dds_data.h"
#include "domain.h"
#include "dds.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "dcps_event.h"
#include "disc.h"
#include "dcps_topic.h"

void DDS_StringSeq__init (DDS_StringSeq *strings)
{
	DDS_SEQ_INIT (*strings);
}

void DDS_StringSeq__clear (DDS_StringSeq *strings)
{
	dds_seq_cleanup (strings);
}

DDS_StringSeq *DDS_StringSeq__alloc (void)
{
	DDS_StringSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_StringSeq));
	if (!p)
		return (NULL);

	DDS_StringSeq__init (p);
	return (p);
}

void DDS_StringSeq__free (DDS_StringSeq *strings)
{
	if (!strings)
		return;

	DDS_StringSeq__clear (strings);
	mm_fcts.free_ (strings);
}

DDS_DomainParticipant DDS_ContentFilteredTopic_get_participant (
						DDS_ContentFilteredTopic ftp)
{
	DDS_DomainParticipant	part;

	ctrc_printd (DCPS_ID, DCPS_FT_G_PART, &ftp, sizeof (ftp));
	if (!topic_ptr (ftp, 1, NULL))
		return (NULL);

	if ((ftp->topic.entity.flags & EF_FILTERED) != 0)
		part = ftp->topic.domain;
	else
		part = NULL;

	lock_release (ftp->topic.lock);
	return (part);
}

DDS_Topic DDS_ContentFilteredTopic_get_related_topic (DDS_ContentFilteredTopic ftp)
{
	Topic_t		*tp;

	ctrc_printd (DCPS_ID, DCPS_FT_REL, &ftp, sizeof (ftp));
	if (!topic_ptr (ftp, 1, NULL))
		return (NULL);

	if ((ftp->topic.entity.flags & EF_FILTERED) != 0)
		tp = ftp->related;
	else
		tp = NULL;

	lock_release (ftp->topic.lock);
	return (tp);
}

const char *DDS_ContentFilteredTopic_get_filter_expression (DDS_ContentFilteredTopic ftp)
{
	const char	*sp;

	ctrc_printd (DCPS_ID, DCPS_FT_G_EXPR, &ftp, sizeof (ftp));
	if (!topic_ptr (ftp, 1, NULL))
		return (NULL);

	if ((ftp->topic.entity.flags & EF_FILTERED) != 0)
		sp = str_ptr (ftp->data.filter.expression);
	else
		sp = NULL;

	lock_release (ftp->topic.lock);
	return (sp);
}

DDS_ReturnCode_t DDS_ContentFilteredTopic_get_expression_parameters (
						DDS_ContentFilteredTopic ftp,
						DDS_StringSeq *expr_pars)
{
	DDS_ReturnCode_t	rc;

	ctrc_begind (DCPS_ID, DCPS_FT_G_PARS, &ftp, sizeof (ftp));
	ctrc_contd (&expr_pars, sizeof (expr_pars));
	ctrc_endd ();

	if (!topic_ptr (ftp, 1, NULL))
		return (DDS_RETCODE_ALREADY_DELETED);

	if ((ftp->topic.entity.flags & EF_FILTERED) == 0) {
		lock_release (ftp->topic.lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	if (!expr_pars) {
		lock_release (ftp->topic.lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_SEQ_INIT (*expr_pars);
	rc = dcps_get_str_pars (expr_pars, ftp->data.filter.expression_pars);
	lock_release (ftp->topic.lock);
	return (rc);
}

DDS_ReturnCode_t DDS_ContentFilteredTopic_set_expression_parameters (
						DDS_ContentFilteredTopic ftp,
						DDS_StringSeq *expr_pars)
{
	DDS_ReturnCode_t	rc;

	ctrc_begind (DCPS_ID, DCPS_FT_S_PARS, &ftp, sizeof (ftp));
	ctrc_contd (&expr_pars, sizeof (expr_pars));
	ctrc_endd ();

	if (!topic_ptr (ftp, 1, NULL))
		return (DDS_RETCODE_ALREADY_DELETED);

	if ((ftp->topic.entity.flags & EF_FILTERED) == 0) {
		rc = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	if (!expr_pars || DDS_SEQ_LENGTH (*expr_pars) < ftp->data.program.npars) {
		rc = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	rc = dcps_update_str_pars (&ftp->data.filter.expression_pars, expr_pars);

    done:
	lock_release (ftp->topic.lock);
	return (rc);
}

const char *DDS_ContentFilteredTopic_get_type_name (DDS_ContentFilteredTopic ftp)
{
	const char	*sp;

	ctrc_printd (DCPS_ID, DCPS_FT_G_TNAME, &ftp, sizeof (ftp));
	if (!topic_ptr (ftp, 1, NULL))
		return (NULL);

	if ((ftp->topic.entity.flags & EF_FILTERED) == 0) {
		lock_release (ftp->topic.lock);
		return (NULL);
	}
	sp = str_ptr (ftp->topic.type->type_name);
	lock_release (ftp->topic.lock);
	return (sp);
}

const char *DDS_ContentFilteredTopic_get_name (DDS_ContentFilteredTopic ftp)
{
	const char	*sp;

	ctrc_printd (DCPS_ID, DCPS_FT_G_NAME, &ftp, sizeof (ftp));
	if (!topic_ptr (ftp, 1, NULL))
		return (NULL);

	if ((ftp->topic.entity.flags & EF_FILTERED) == 0) {
		lock_release (ftp->topic.lock);
		return (NULL);
	}
	sp = str_ptr (ftp->topic.name);
	lock_release (ftp->topic.lock);
	return (sp);
}

DDS_TopicDescription DDS_ContentFilteredTopic_get_topicdescription (DDS_ContentFilteredTopic topic)
{
	return ((DDS_TopicDescription) topic);
}

#ifdef THREADS_USED

void duration2timespec (DDS_Duration_t *timeout, struct timespec *ts)
{
	if (timeout->sec != DDS_DURATION_INFINITE_SEC &&
	    timeout->nanosec != DDS_DURATION_INFINITE_NSEC) {
		clock_gettime (CLOCK_REALTIME, ts);
		ts->tv_sec += timeout->sec;
		ts->tv_nsec += timeout->nanosec;
		if (ts->tv_nsec >= 1000000000) {
			ts->tv_sec++;
			ts->tv_nsec -= 1000000000;
		}
	}
	else {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
	}
}
#endif


/********************************/
/*   TopicDescription methods   */
/********************************/

DDS_DomainParticipant DDS_TopicDescription_get_participant (DDS_TopicDescription dp)
{
	Topic_t			*tp;
	DDS_DomainParticipant	part;
	
	ctrc_printd (DCPS_ID, DCPS_TD_G_PART, &dp, sizeof (dp));
	tp = topic_ptr (dp, 1, NULL);
	if (tp) {
		part = tp->domain;
		lock_release (tp->lock);
		return (part);
	}
	else
		return (NULL);
}

const char *DDS_TopicDescription_get_name (DDS_TopicDescription dp)
{
	Topic_t		*tp;
	const char	*name;

	ctrc_printd (DCPS_ID, DCPS_TD_G_NAME, &dp, sizeof (dp));
	tp = topic_ptr (dp, 1, NULL);
	if (tp) {
		name = str_ptr (tp->name);
		lock_release (tp->lock);
		return (name);
	}
	else
		return (NULL);
}

const char *DDS_TopicDescription_get_type_name (DDS_TopicDescription dp)
{
	Topic_t		*tp;
	const char	*name;

	ctrc_printd (DCPS_ID, DCPS_TD_G_TNAME, &dp, sizeof (dp));
	tp = topic_ptr (dp, 1, NULL);
	if (tp) {
		name = str_ptr (tp->type->type_name);
		lock_release (tp->lock);
		return (name);
	}
	else
		return (NULL);
}

DDS_ReturnCode_t DDS_Topic_get_qos (DDS_Topic tp, DDS_TopicQos *qos)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_T_G_QOS, &tp, sizeof (tp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!topic_ptr (tp, 1, &ret))
		return (ret);

	qos_topic_get (tp->qos, qos);
	lock_release (tp->lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Topic_set_qos (DDS_Topic tp, DDS_TopicQos *qos)
{
	Endpoint_t		*ep;
	Reader_t		*rp;
	Writer_t		*wp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_T_S_QOS, &tp, sizeof (tp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!topic_ptr (tp, 1, &ret))
		return (ret);

	if (qos == DDS_TOPIC_QOS_DEFAULT)
		qos = &tp->domain->def_topic_qos;
	else if (!qos_valid_topic_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = qos_topic_update (&tp->qos, qos);
	if (ret != DDS_RETCODE_OK)
		goto done;

	/* Update all local Readers associated with topic. */
	for (ep = tp->readers; ep; ep = ep->next)
		if ((ep->entity.flags & EF_LOCAL) != 0) {
			rp = (Reader_t *) ep;
#ifdef RW_LOCKS
			lock_take (rp->r_lock);
#endif
			disc_reader_update (tp->domain, rp, 1, 0);
#ifdef RW_LOCKS
			lock_release (rp->r_lock);
#endif
		}

	/* Update all local Writers associated with topic. */
	for (ep = tp->writers; ep; ep = ep->next)
		if ((ep->entity.flags & EF_LOCAL) != 0) {
			wp = (Writer_t *) ep;
#ifdef RW_LOCKS
			lock_take (wp->w_lock);
#endif
			disc_writer_update (tp->domain, wp, 1, 0);
#ifdef RW_LOCKS
			lock_release (wp->w_lock);
#endif
		}

    done:
    	lock_release (tp->lock);
	return (ret);
}

DDS_TopicListener *DDS_Topic_get_listener (DDS_Topic tp)
{
	ctrc_printd (DCPS_ID, DCPS_T_G_LIS, &tp, sizeof (tp));

	if (!topic_ptr (tp, 0, NULL))
		return (NULL);

	return (&tp->listener);
}

DDS_ReturnCode_t DDS_Topic_set_listener (DDS_Topic tp,
					 DDS_TopicListener *listener,
					 DDS_StatusMask mask)
{
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_T_S_LIS, &tp, sizeof (tp));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	if (!topic_ptr (tp, 1, &ret))
		return (ret);

	dcps_update_listener ((Entity_t *) tp, &tp->lock,
			      &tp->mask, &tp->listener,
			      mask, listener);
	lock_release (tp->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_Topic_get_status_changes (DDS_Topic tp)
{
	DDS_StatusMask	m;

	ctrc_printd (DCPS_ID, DCPS_T_G_STAT, &tp, sizeof (tp));

	if (!topic_ptr (tp, 1, NULL))
		return (0);

	m = tp->status;
	lock_release (tp->lock);
	return (m);
}

DDS_ReturnCode_t DDS_Topic_enable (DDS_Topic tp)
{
#ifdef THREADS_USED
	TopicWait_t	*wp;
#endif
	DDS_ReturnCode_t error;

	ctrc_printd (DCPS_ID, DCPS_T_ENABLE, &tp, sizeof (tp));
	if (!topic_ptr (tp, 1, &error))
		return (error);

	if ((tp->domain->participant.p_flags & EF_ENABLED) == 0) {
		lock_release (tp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
	if ((tp->entity.flags & EF_ENABLED) != 0) {
		lock_release (tp->lock);
		return (DDS_RETCODE_OK);
	}
	tp->entity.flags |= (EF_NOT_IGNORED | EF_ENABLED);

#ifdef THREADS_USED
	for (wp = tp->domain->topic_wait;
	     wp && strcmp (wp->name, str_ptr (tp->name));
	     wp = wp->next)
		;
	if (wp) {
		wp->topic = tp;
		if (wp->nthreads > 1)
			cond_signal_all (wp->condition);
		else
			cond_signal (wp->condition);
	}
#endif

	/* Check if topic type is incompatible with learned topics. */
	if ((tp->type->flags & EF_INC_TYPE) != 0) {
		dcps_inconsistent_topic (tp);
		tp->type->flags &= ~EF_INC_TYPE;
	}
	lock_release (tp->lock);
	return (DDS_RETCODE_OK);
}


DDS_StatusCondition DDS_Topic_get_statuscondition (DDS_Topic tp)
{
	StatusCondition_t	*scp;

	ctrc_printd (DCPS_ID, DCPS_T_G_SCOND, &tp, sizeof (tp));

	if (!topic_ptr (tp, 1, NULL))
		return (NULL);

	scp = tp->condition;
	if (!scp) {
		scp = dcps_new_status_condition ();
		if (!scp)
			return (NULL);

		scp->entity = (Entity_t *) tp;
		tp->condition = scp;
	}
	lock_release (tp->lock);
	return ((DDS_StatusCondition) scp);
}

DDS_InstanceHandle_t DDS_Topic_get_instance_handle (DDS_Topic tp)
{
	DDS_InstanceHandle_t	h;

	ctrc_printd (DCPS_ID, DCPS_T_G_HANDLE, &tp, sizeof (tp));

	if (!topic_ptr (tp, 1, NULL))
		return (0);

	h = tp->entity.handle;
	lock_release (tp->lock);
	return (h);
}

DDS_DomainParticipant DDS_Topic_get_participant (DDS_Topic topic)
{
	Topic_t			*tp;
	DDS_DomainParticipant	part;

	ctrc_printd (DCPS_ID, DCPS_T_G_PART, &topic, sizeof (topic));

	tp = topic_ptr (topic, 1, NULL);
	if (!tp)
		return (NULL);

	part = tp->domain;
	lock_release (tp->lock);
	return (part);
}

const char *DDS_Topic_get_name (DDS_Topic topic)
{
	Topic_t			*tp;
	const char		*name;

	ctrc_printd (DCPS_ID, DCPS_T_G_NAME, &topic, sizeof (topic));

	tp = topic_ptr (topic, 1, NULL);
	if (!tp)
		return (NULL);

	name = str_ptr (tp->name);
	lock_release (tp->lock);
	return (name);
}

const char *DDS_Topic_get_type_name (DDS_Topic topic)
{
	Topic_t			*tp;
	const char		*name;

	ctrc_printd (DCPS_ID, DCPS_T_G_TNAME, &topic, sizeof (topic));

	tp = topic_ptr (topic, 1, NULL);
	if (!tp)
		return (NULL);

	name = str_ptr (tp->type->type_name);
	lock_release (tp->lock);
	return (name);
}

DDS_TopicDescription DDS_Topic_get_topicdescription (DDS_Topic topic)
{
	return ((DDS_TopicDescription) topic);
}

#define	ALIGN32(s)	(((s) + 3) & ~3)

unsigned char *dcps_key_data_get (Topic_t          *tp,
			          const void       *data,
				  int              dynamic,
				  int              secure,
				  unsigned char    buf [16],
				  size_t           *size,
				  DDS_ReturnCode_t *ret)
{
	unsigned char		*keys;
	const TypeSupport_t	*ts;

	ts = tp->type->type_support;
	if (!ts->ts_keys) {
		*ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		return (NULL);
	}
	if (!data) {
		*ret = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	*size = ts->ts_mkeysize;
	if (!*size || !ts->ts_fksize) {
		*size = DDS_KeySizeFromNativeData (data, dynamic, ts, ret);
		if (!*size) {
			*ret = DDS_RETCODE_BAD_PARAMETER;
			warn_printf ("key_data_get: DDS_KeySize() returns error!");
			return (NULL);
		}
	}
	if (*size > 16) {
		keys = xmalloc (ALIGN32 (*size));
		if (!keys) {
			*ret = DDS_RETCODE_OUT_OF_RESOURCES;
			warn_printf ("key_data_get: xmalloc() returns NULL!");
			return (NULL);
		}
	}
	else
		keys = buf;
	*ret = DDS_KeyFromNativeData (keys, data, dynamic, secure, ts);
	if (*ret != DDS_RETCODE_OK) {
		if (*size > 16)
			xfree (keys);

		return (NULL);
	}
	*ret = DDS_RETCODE_OK;
	return (keys);
}


