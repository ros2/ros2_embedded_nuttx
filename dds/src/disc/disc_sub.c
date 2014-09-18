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

/* disc_sub.c -- Implements the subscription functions for discovery. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "error.h"
#include "parse.h"
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_access.h"
#endif
#endif
#include "dcps.h"
#include "locator.h"
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_match.h"
#include "disc_tc.h"
#include "disc_sub.h"

static void dr2dt (DiscoveredTopicQos *qp, const DiscoveredReaderData *info)
{
	qp->durability = info->qos.durability;
	qp->durability_service = qos_def_topic_qos.durability_service;
	qp->deadline = info->qos.deadline;
	qp->latency_budget = info->qos.latency_budget;
	qp->liveliness = info->qos.liveliness;
	qp->reliability = info->qos.reliability;
	qp->transport_priority = qos_def_topic_qos.transport_priority;
	qp->lifespan = qos_def_topic_qos.lifespan;
	qp->destination_order = info->qos.destination_order;
	qp->history = qos_def_topic_qos.history;
	qp->resource_limits = qos_def_topic_qos.resource_limits;
	qp->ownership = info->qos.ownership;
	qp->topic_data = str_ref (info->qos.topic_data);
}

/* add_dr_topic -- Add a new topic, based on discovered reader info. */

static Topic_t *add_dr_topic (Participant_t        *pp,
			      Topic_t              *tp,
			      DiscoveredReaderData *info,
			      DiscoveredTopicQos   *qos,
			      int                  ignored)
{
	int	new;

	tp = topic_create (pp,
			   tp,
			   str_ptr (info->topic_name),
			   str_ptr (info->type_name),
			   &new);
	if (!tp || ignored)
		return (tp);

	if (!new || tp->nrrefs > 1 || tp->nlrefs) {
		if (!new &&
		    pp->p_domain->builtin_readers [BT_Topic] && 
		    (tp->entity.flags & EF_REMOTE) != 0)
			user_topic_notify (tp, 0);
		else if (new)
			tp->entity.flags |= EF_NOT_IGNORED;
		return (tp);
	}
	tp->qos = qos_disc_topic_new (qos);
	if (!tp->qos) {
		topic_delete (pp, tp, NULL, NULL);
		return (NULL);
	}
	/*dbg_printf ("!!Qos_disc_topic_new (%p) for %s/%s,\r\n", tp->qos,
			str_ptr (info->topic_name), str_ptr (info->type_name));*/
	tp->entity.flags |= EF_NOT_IGNORED;

	/* Deliver topic info to user topic reader. */
	if ((tp->entity.flags & EF_LOCAL) == 0 &&
	     pp->p_domain->builtin_readers [BT_Topic])
		user_topic_notify (tp, 1);

	return (tp);
}

#ifdef SIMPLE_DISCOVERY

/* update_dr_topic -- Update topic QoS, based on discovered reader info. */

static int update_dr_topic_qos (Topic_t *tp, const DiscoveredReaderData *info)
{
	DiscoveredTopicQos	qos_data;
	int			error;

	/* Locally created topic info has precedence: don't update if local. */
	if ((tp->entity.flags & EF_LOCAL) == 0) {
		dr2dt (&qos_data, info);
		error = qos_disc_topic_update (&tp->qos, &qos_data);
		if (!error && tp->domain->builtin_readers [BT_Topic])
			user_topic_notify (tp, 0);
	}
	return (0);
}

#endif /* SIMPLE_DISCOVERY */

/* disc_subscription_add -- Add a Discovered Reader.
			    On entry/exit: DP locked. */

int disc_subscription_add (Participant_t        *pp,
			   DiscoveredReader_t   *drp,
			   const UniQos_t       *qp,
			   Topic_t              *tp,
			   Writer_t             *wp,
			   DiscoveredReaderData *info)
{
	Endpoint_t		*ep;
	Topic_t			*ptp;
	int			new_topic = 0, ret = DDS_RETCODE_OK, ignored;
	DDS_QOS_POLICY_ID	qid;
	DiscoveredTopicQos	qos_data;
	int			incompatible = 0;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t		*dp;

	dp = pp->p_domain;
#endif
	ignored = 0;
	ptp = topic_lookup (pp, str_ptr (info->topic_name));
	if (!ptp) {
		/*log_printf (DISC_ID, 0, "Discovery: add_dr_topic (%s)\r\n", str_ptr (info->topic_name));*/
		dr2dt (&qos_data, info);
#ifdef DDS_SECURITY
		if (ACCESS_CONTROL (pp->p_domain) &&
		    check_peer_topic (pp->p_permissions,
				      str_ptr (info->topic_name),
				      &qos_data) != DDS_RETCODE_OK) {
			ignored = 1;
			endpoint_delete (pp, &drp->dr_ep);
			return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
		}
#endif
		tp = add_dr_topic (pp, tp, info, &qos_data, ignored);
		if (!tp || entity_ignored (tp->entity.flags)) {
			drp->dr_flags &= ~EF_NOT_IGNORED;
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		new_topic = 1;
	}
#ifdef DDS_TYPECODE
	if (info->typecode) {
		if (!tp->type->type_support) {
			tp->type->type_support = tc_typesupport (info->typecode,
						 str_ptr (tp->type->type_name));
			drp->dr_tc = info->typecode;
		}
		else
			drp->dr_tc = tc_unique (tp, &drp->dr_ep,
						info->typecode, &incompatible);
		info->typecode = NULL;
	}
	else
		drp->dr_tc = NULL;
#endif
	if (lock_take (tp->lock)) {
		warn_printf ("disc_subscription_add: topic lock error");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!new_topic)
		tp->nrrefs++;
	drp->dr_topic = tp;

#ifdef DDS_SECURITY
	if (ACCESS_CONTROL (pp->p_domain)) {
		info->qos.partition = qp->partition;
		info->qos.topic_data = qp->topic_data;
		info->qos.user_data = qp->user_data;
		info->qos.group_data = qp->group_data;
		
		if (check_peer_reader (pp->p_permissions,
				   str_ptr (info->topic_name),
				   &info->qos) != DDS_RETCODE_OK)
			ignored = 1;
		info->qos.partition = NULL;
		info->qos.topic_data = NULL;
		info->qos.user_data = NULL;
		info->qos.group_data = NULL;
	}
#endif
	if (ignored) {
		drp->dr_qos = NULL;
		drp->dr_ucast = NULL;
		drp->dr_mcast = NULL;
		drp->dr_flags &= ~EF_NOT_IGNORED;
		drp->dr_content_filter = NULL;
		ret = DDS_RETCODE_NOT_ALLOWED_BY_SEC;
		goto done;
	}
	drp->dr_qos = qos_add (qp);
	if (!drp->dr_qos) {
		drp->dr_flags &= ~EF_NOT_IGNORED;
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
		goto done;
	}
	drp->dr_ucast = info->proxy.ucast;
	info->proxy.ucast = NULL;
	drp->dr_mcast = info->proxy.mcast;
	info->proxy.mcast = NULL;
	if (info->proxy.exp_il_qos)
		drp->dr_flags |= EF_INLINE_QOS;

	drp->dr_time_based_filter = info->qos.time_based_filter;
	drp->dr_content_filter = info->filter;
	info->filter = NULL;
	if (drp->dr_content_filter) {
			ret = sql_parse_filter (tp->type->type_support,
						str_ptr (drp->dr_content_filter->filter.expression),
						&drp->dr_content_filter->program);
			bc_cache_init (&drp->dr_content_filter->cache);
			if (!ret)
				drp->dr_flags |= EF_FILTERED;
			else
				ret = DDS_RETCODE_OK;
	}
	drp->dr_flags |= EF_NOT_IGNORED;
	if (pp->p_domain->builtin_readers [BT_Subscription])
		user_reader_notify (drp, 1);

	/* Check if not ignored. */
	if ((drp->dr_flags & EF_NOT_IGNORED) == 0)
		goto done;

	/* Hook into Topic Readers list. */
	drp->dr_next = tp->readers;
	tp->readers = &drp->dr_ep;

	/* Check for matching local endpoints. */
	if (incompatible) {
		dcps_inconsistent_topic (tp);
		goto done;
	}
	for (ep = tp->writers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

#ifndef RW_TOPIC_LOCK
		if (lock_take (((Writer_t *) ep)->w_lock)) {
			warn_printf ("disc_subscription_add: writer lock error");
			continue;
		}
#endif
		if ((Writer_t *) ep == wp)
			disc_new_matched_reader (wp, drp);
		else if (!qos_same_partition (ep->u.publisher->qos.partition,
					         drp->dr_qos->qos.partition))
			dcps_offered_incompatible_qos ((Writer_t *) ep, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qos_ptr (ep->qos), &ep->u.publisher->qos,
			     	    qp, NULL, &qid))
			dcps_offered_incompatible_qos ((Writer_t *) ep, qid);
		else
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		     if (!ACCESS_CONTROL (dp) ||
			 !sec_check_local_writer_match (dp->participant.p_permissions,
			    			        drp->dr_participant->p_permissions,
						        (Writer_t *) ep,
						        &drp->dr_ep))
#endif
			disc_new_matched_reader ((Writer_t *) ep, drp);
#ifndef RW_TOPIC_LOCK
		lock_release (((Writer_t *) ep)->w_lock);
#endif
	}

    done:
	lock_release (tp->lock);
	return (ret);
}

#ifdef SIMPLE_DISCOVERY

/* discovered_reader_cleanup -- Cleanup a previously discovered reader. */

void discovered_reader_cleanup (DiscoveredReader_t *rp,
				int                ignore,
				int                *p_last_topic,
				int                *topic_gone)
{
	Topic_t		*tp;
	Endpoint_t	*ep, *prev;
	Participant_t	*pp;

	/* Break all existing endpoint matches. */
	tp = rp->dr_topic;
	if (lock_take (tp->lock)) {
		warn_printf ("discovered_reader_cleanup: topic lock error");
		return;
	}
	if (rp->dr_rtps)
		for (ep = tp->writers; ep; ep = ep->next) {
			if ((ep->entity.flags & EF_LOCAL) == 0)
				continue;

#ifndef RW_TOPIC_LOCK
			if (lock_take (((Writer_t *) ep)->w_lock)) {
				warn_printf ("discovered_reader_cleanup: writer lock error");
				continue;
			}
#endif
			if (rtps_writer_matches ((Writer_t *) ep, rp))
				disc_end_matched_reader ((Writer_t *) ep, rp);
#ifndef RW_TOPIC_LOCK
			lock_release (((Writer_t *) ep)->w_lock);
#endif
		}

	/* Remove from topic list. */
	for (prev = NULL, ep = tp->readers;
	     ep && ep != &rp->dr_ep;
	     prev = ep, ep = ep->next)
		;

	if (ep) {
		/* Still in topic list: remove it. */
		if (prev)
			prev->next = ep->next;
		else
			tp->readers = ep->next;
	}

	/* Cleanup all endpoint data. */
	if (rp->dr_content_filter) {
		filter_data_cleanup (rp->dr_content_filter);
		xfree (rp->dr_content_filter);
		rp->dr_content_filter = NULL;
	}
	locator_list_delete_list (&rp->dr_ucast);
	locator_list_delete_list (&rp->dr_mcast);
	qos_disc_reader_free (rp->dr_qos);
	rp->dr_qos = NULL;
#ifdef DDS_TYPECODE
	if (rp->dr_tc && rp->dr_tc != TC_IS_TS) {
		vtc_free (rp->dr_tc);
		rp->dr_tc = NULL;
	}
#endif

	pp = rp->dr_participant;
	if (ignore) {
		rp->dr_flags &= ~EF_NOT_IGNORED;
		lock_release (tp->lock);
	}
	else {
		/* Free the endpoint. */
		endpoint_delete (pp, &rp->dr_ep);
		topic_delete (pp, tp, p_last_topic, topic_gone);
	}
}

/* disc_subscription_remove -- Remove a Discovered Reader.
			       On entry/exit: DP locked. */

void disc_subscription_remove (Participant_t      *pp,
			       DiscoveredReader_t *rp)
{
	Topic_t		*tp = rp->dr_topic;
	Domain_t	*dp;
	InstanceHandle	topic_handle;
	int		last_p_topic, topic_gone;

	if (entity_shutting_down (rp->dr_flags) || !tp)
		return;

	dp = tp->domain;
	topic_handle = tp->entity.handle;
	rp->dr_flags |= EF_SHUTDOWN;
	if (pp->p_domain->builtin_readers [BT_Subscription])
		user_notify_delete (dp, BT_Subscription, rp->dr_handle);

	discovered_reader_cleanup (rp, 0, &last_p_topic, &topic_gone);

	if (topic_gone && pp->p_domain->builtin_readers [BT_Topic])
		user_notify_delete (dp, BT_Topic, topic_handle);
}

/* disc_subscription_update -- Update a Discovered Reader.
			       On entry/exit: DP locked. */

int disc_subscription_update (Participant_t        *pp,
			      DiscoveredReader_t   *drp,
			      DiscoveredReaderData *info)
{
	Endpoint_t	*ep;
	Writer_t	*wp;
	Topic_t		*tp;
	LocatorList_t	tlp;
	FilterData_t	*fp;
	Strings_t	*parsp;
	DDS_QOS_POLICY_ID qid;
	int		old_match, new_match, ret;
	UniQos_t	qp;
	int		incompatible = 0;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;

	dp = pp->p_domain;
#endif

	/* If the topic name/type name changes or the Reader/Topic QoS becomes
	   incompatible, simply delete and recreate the endpoint. */
	if (strcmp (str_ptr (drp->dr_topic->name),
		    str_ptr (info->topic_name)) ||
	    strcmp (str_ptr (drp->dr_topic->type->type_name),
		    str_ptr (info->type_name)) ||
	    update_dr_topic_qos (drp->dr_topic, info) ||
	    qos_disc_reader_update (&drp->dr_qos, &info->qos)
#ifdef DDS_TYPECODE
	 || (info->typecode && 
	     !tc_update (&drp->dr_ep, &drp->dr_tc,
	     		 &info->typecode, &incompatible))
#endif
	    ) {
		tp = drp->dr_topic;
		disc_subscription_remove (pp, drp);
		qos_disc_reader_set (&qp, &info->qos);        
		return (disc_subscription_add (pp, drp, &qp, tp, NULL, info));
	}
	tp = drp->dr_topic;
	if (lock_take (tp->lock)) {
		warn_printf ("disc_subscription_update: topic lock error");
		return (DDS_RETCODE_ERROR);
	}
	if (info->proxy.exp_il_qos)
		drp->dr_flags |= EF_INLINE_QOS;
	else
		drp->dr_flags &= ~EF_INLINE_QOS;

	/* Update locator lists - notify if changed. */
	if (!locator_list_equal (drp->dr_ucast, info->proxy.ucast)) {
		locator_list_swap (drp->dr_ucast, info->proxy.ucast, tlp);
		if (drp->dr_rtps)
			rtps_endpoint_locators_update (&drp->dr_ep, 0);
	}
	if (!locator_list_equal (drp->dr_mcast, info->proxy.mcast)) {
		locator_list_swap (drp->dr_mcast, info->proxy.mcast, tlp);
		if (drp->dr_rtps)
			rtps_endpoint_locators_update (&drp->dr_ep, 1);
	}

	/* Update time-based filter - notify if changed. */
	if (memcmp (&drp->dr_time_based_filter.minimum_separation,
		    &info->qos.time_based_filter.minimum_separation,
		    sizeof (DDS_TimeBasedFilterQosPolicy))) {
		drp->dr_time_based_filter.minimum_separation = 
		       info->qos.time_based_filter.minimum_separation;
		if (drp->dr_rtps)
			rtps_endpoint_time_filter_update (&drp->dr_ep);
	}

	/* Update content filter. */
	if (drp->dr_content_filter &&
	    info->filter &&
	    !strcmp (str_ptr (drp->dr_content_filter->filter.class_name),
		     str_ptr (info->filter->filter.class_name)) &&
	    !strcmp (str_ptr (drp->dr_content_filter->filter.expression),
		     str_ptr (info->filter->filter.expression))) {

		/* Filter wasn't changed - simply update filter parameters. */
		parsp = drp->dr_content_filter->filter.expression_pars;
		drp->dr_content_filter->filter.expression_pars = info->filter->filter.expression_pars;
		info->filter->filter.expression_pars = parsp;
		bc_cache_flush (&drp->dr_content_filter->cache);
	}
	else {
		/* Simply swap active filter with new parameters. */
		fp = drp->dr_content_filter;
		drp->dr_content_filter = info->filter;
		info->filter = fp;
		if (drp->dr_content_filter &&
		    (drp->dr_flags & EF_NOT_IGNORED) != 0) {
			ret = sql_parse_filter (drp->dr_topic->type->type_support,
						str_ptr (drp->dr_content_filter->filter.expression),
						&drp->dr_content_filter->program);
			bc_cache_init (&drp->dr_content_filter->cache);
			if (!ret)
				drp->dr_flags |= EF_FILTERED;
			else
				drp->dr_flags &= ~EF_FILTERED;
		}
		else
			drp->dr_flags &= ~EF_FILTERED;
	}
	if (pp->p_domain->builtin_readers [BT_Subscription])
		user_reader_notify (drp, 0);

	/* Check if not ignored. */
	if ((drp->dr_flags & EF_NOT_IGNORED) == 0) {
		lock_release (tp->lock);
		return (DDS_RETCODE_OK);
	}

	/* Check for matching local endpoints. */
	if (incompatible)
		dcps_inconsistent_topic (tp);
	for (ep = tp->writers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		wp = (Writer_t *) ep;
#ifndef RW_TOPIC_LOCK
		if (lock_take (wp->w_lock)) {
			warn_printf ("disc_subscription_update: writer lock error");
			continue;
		}
#endif
		old_match = (drp->dr_rtps &&
			     rtps_writer_matches (wp, drp));
		new_match = 0;
		if (incompatible)
			/* Different types: cannot match! */;
		else if (!qos_same_partition (wp->w_publisher->qos.partition,
					 drp->dr_qos->qos.partition))
			dcps_offered_incompatible_qos (wp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qos_ptr (wp->w_qos), &wp->w_publisher->qos,
			     	     &drp->dr_qos->qos, NULL, &qid))
			dcps_offered_incompatible_qos (wp, qid);
		else
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		     if (!ACCESS_CONTROL (dp) ||
			 !sec_check_local_writer_match (dp->participant.p_permissions,
			    			        drp->dr_participant->p_permissions,
						        wp,
						        &drp->dr_ep))
#endif
			new_match = 1;
		if (old_match && !new_match)
			disc_end_matched_reader (wp, drp);
		else if (!old_match && new_match)
			disc_new_matched_reader (wp, drp);
#ifndef RW_TOPIC_LOCK
		lock_release (wp->w_lock);
#endif
	}
	lock_release (tp->lock);
	return (DDS_RETCODE_OK);
}

#endif /* SIMPLE_DISCOVERY */

