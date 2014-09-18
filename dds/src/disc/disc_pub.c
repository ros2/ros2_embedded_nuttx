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

/* disc_pub.c -- Implements the publication functions for discovery. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "error.h"
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_access.h"
#endif
#endif
#include "dcps.h"
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_match.h"
#include "disc_tc.h"
#include "disc_pub.h"

static void dw2dt (DiscoveredTopicQos *qp, const DiscoveredWriterData *info)
{
	qp->durability = info->qos.durability;
	qp->durability_service = info->qos.durability_service;
	qp->deadline = info->qos.deadline;
	qp->latency_budget = info->qos.latency_budget;
	qp->liveliness = info->qos.liveliness;
	qp->reliability = info->qos.reliability;
	qp->transport_priority = qos_def_topic_qos.transport_priority;
	qp->lifespan = info->qos.lifespan;
	qp->destination_order = info->qos.destination_order;
	qp->history = qos_def_topic_qos.history;
	qp->resource_limits = qos_def_topic_qos.resource_limits;
	qp->ownership = info->qos.ownership;
	qp->topic_data = str_ref (info->qos.topic_data);
}

/* add_dw_topic -- Add a new topic, based on discovered writer info. */

static Topic_t *add_dw_topic (Participant_t        *pp,
			      Topic_t              *tp,
			      DiscoveredWriterData *info,
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
	tp->entity.flags |= EF_NOT_IGNORED;

	/* Deliver topic info to user topic reader. */
	if (pp->p_domain->builtin_readers [BT_Topic])
		user_topic_notify (tp, 1);

	return (tp);
}

#ifdef SIMPLE_DISCOVERY

/* update_dw_topic -- Update topic QoS, based on discovered writer info. */

static int update_dw_topic_qos (Topic_t *tp, const DiscoveredWriterData *info)
{
	DiscoveredTopicQos	qos_data;
	int			error;

	/* Locally created topic info has precedence: don't update if local. */
	if ((tp->entity.flags & EF_LOCAL) == 0) {
		dw2dt (&qos_data, info);
		error = qos_disc_topic_update (&tp->qos, &qos_data);
		if (!error && tp->domain->builtin_readers [BT_Topic])
			user_topic_notify (tp, 0);
	}
	return (0);
}

#endif /* SIMPLE_DISCOVERY */

/* disc_match_readers_new -- Match local readers to a new remote writer.
			     On entry/exit: DP, TP locked. */

static void disc_match_readers_new (Topic_t            *tp,
				    Reader_t           *mrp, 
				    const UniQos_t     *qp,
				    DiscoveredWriter_t *dwp)
{
	Endpoint_t	*ep;
	Reader_t	*rp;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;

	dp = tp->domain;
#endif

	for (ep = tp->readers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		rp = (Reader_t *) ep;
#ifndef RW_TOPIC_LOCK
		if (lock_take (rp->r_lock)) {
			warn_printf ("disc_match_readers_new: reader lock error");
			continue;
		}
#endif
		if (rp == mrp)
			disc_new_matched_writer (rp, dwp);
		else if (!qos_same_partition (ep->u.subscriber->qos.partition,
						 dwp->dw_qos->qos.partition))
			dcps_requested_incompatible_qos (rp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qp, NULL,
		    	             qos_ptr (rp->r_qos), &ep->u.subscriber->qos,
				     &qid))
			dcps_requested_incompatible_qos (rp, qid);
		else
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		     if (!ACCESS_CONTROL (dp) ||
			 !sec_check_local_reader_match (dp->participant.p_permissions,
			    			        dwp->dw_participant->p_permissions,
						        rp,
						        &dwp->dw_ep))
#endif
			disc_new_matched_writer (rp, dwp);
#ifndef RW_TOPIC_LOCK
		lock_release (rp->r_lock);
#endif
	}
}

/* disc_publication_add -- Add a discovered Writer.
			   On entry/exit: DP locked. */

int disc_publication_add (Participant_t        *pp,
			  DiscoveredWriter_t   *dwp,
			  const UniQos_t       *qp,
			  Topic_t              *tp,
			  Reader_t             *rp,
			  DiscoveredWriterData *info)
{
	FilteredTopic_t		*ftp;
	Topic_t			*ptp;
	int			new_topic = 0, ret = DDS_RETCODE_OK, ignored;
	DiscoveredTopicQos	qos_data;
	int			incompatible = 0;

	ignored = 0;
	ptp = topic_lookup (pp, str_ptr (info->topic_name));
	if (!ptp) {
		/* log_printf (DISC_ID, 0, "Discovery: add_dw_topic (%s)\r\n", str_ptr (info->topic_name)); */
		dw2dt (&qos_data, info);
#ifdef DDS_SECURITY
		if (ACCESS_CONTROL (pp->p_domain) &&
		    check_peer_topic (pp->p_permissions,
		    		      str_ptr (info->topic_name),
				      &qos_data) != DDS_RETCODE_OK) {
			ignored = 1;
			endpoint_delete (pp, &dwp->dw_ep);
			return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
		}
#endif
		tp = add_dw_topic (pp, tp, info, &qos_data, ignored);
		if (!tp || entity_ignored (tp->entity.flags)) {
			dwp->dw_flags &= ~EF_NOT_IGNORED;
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		new_topic = 1;
	}
#ifdef DDS_TYPECODE
	if (info->typecode) {
		if (!tp->type->type_support) {
			tp->type->type_support = tc_typesupport (info->typecode,
						 str_ptr (tp->type->type_name));
			dwp->dw_tc = info->typecode;
		}
		else
			dwp->dw_tc = tc_unique (tp, &dwp->dw_ep,
						info->typecode, &incompatible);
		info->typecode = NULL;
	}
	else
		dwp->dw_tc = NULL;
#endif
	if (lock_take (tp->lock)) {
		warn_printf ("disc_publication_add: topic lock error");
		return (DDS_RETCODE_ERROR);
	}
	if (!new_topic)
		tp->nrrefs++;
	dwp->dw_topic = tp;

#ifdef DDS_SECURITY
	if (ACCESS_CONTROL (pp->p_domain)) {
		info->qos.partition = qp->partition;
		info->qos.topic_data = qp->topic_data;
		info->qos.user_data = qp->user_data;
		info->qos.group_data = qp->group_data;
		
		if (check_peer_writer (pp->p_permissions,
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
		dwp->dw_qos = NULL;
		dwp->dw_ucast = NULL;
		dwp->dw_mcast = NULL;
		dwp->dw_flags &= ~EF_NOT_IGNORED;
		ret = DDS_RETCODE_NOT_ALLOWED_BY_SEC;
		goto done;
	}
	dwp->dw_qos = qos_add (qp);
	if (!dwp->dw_qos) {
		dwp->dw_flags &= ~EF_NOT_IGNORED;
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
		goto done;
	}
	dwp->dw_ucast = info->proxy.ucast;
	info->proxy.ucast = NULL;
	dwp->dw_mcast = info->proxy.mcast;
	info->proxy.mcast = NULL;
	dwp->dw_flags |= EF_NOT_IGNORED;
	if (pp->p_domain->builtin_readers [BT_Publication])
		user_writer_notify (dwp, 1);

	/* Check if not ignored. */
	if ((dwp->dw_flags & EF_NOT_IGNORED) == 0)
		goto done;

	/* Hook into the Topic Writers list. */
	dwp->dw_next = tp->writers;
	tp->writers = &dwp->dw_ep;

	/* Can we match local readers with new publication? */
	if (incompatible)
		dcps_inconsistent_topic (tp);
	else {
		disc_match_readers_new (tp, rp, qp, dwp);
		for (ftp = tp->filters; ftp; ftp = ftp->next) {
			if (lock_take (ftp->topic.lock)) {
				warn_printf ("disc_publication_add: topic lock error");
				continue;
			}
			disc_match_readers_new (&ftp->topic, rp, qp, dwp);
			lock_release (ftp->topic.lock);
		}
	}

    done:
    	lock_release (tp->lock);
	return (ret);
}

#ifdef SIMPLE_DISCOVERY

/* disc_match_readers_end -- Remove all matching readers.
			     On entry/exit: DP, TP locked. */

static void disc_match_readers_end (Topic_t *tp, DiscoveredWriter_t *wp)
{
	Endpoint_t	*ep;

	for (ep = tp->readers; ep; ep = ep->next) {
		if ((ep->entity.flags & EF_LOCAL) == 0)
			continue;

#ifndef RW_TOPIC_LOCK
		if (lock_take (((Reader_t *) ep)->r_lock)); {
			warn_printf ("disc_match_readers_end: reader lock error");
			continue;
		}
#endif
		if (rtps_reader_matches ((Reader_t *) ep, wp))
			disc_end_matched_writer ((Reader_t *) ep, wp);
#ifndef RW_TOPIC_LOCK
		lock_release (((Reader_t *) ep)->r_lock);
#endif
	}
}

/* discovered_writer_cleanup -- Cleanup a previously discovered writer.
				On entry/exit: DP locked. */

void discovered_writer_cleanup (DiscoveredWriter_t *wp,
				int                ignore,
				int                *p_last_topic,
				int                *topic_gone)
{
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	Endpoint_t	*ep, *prev;
	Participant_t	*pp;

	/* Break all existing matches. */
	tp = wp->dw_topic;
	if (lock_take (tp->lock)) {
		warn_printf ("discovered_writer_cleanup: topic lock error");
		return;
	}
	if (wp->dw_rtps) {
		disc_match_readers_end (tp, wp);
		for (ftp = tp->filters; ftp; ftp = ftp->next) {
			if (lock_take (ftp->topic.lock)) {
				warn_printf ("discovered_writer_cleanup: filter topic lock error");
				continue;
			}
			disc_match_readers_end (&ftp->topic, wp);
			lock_release (ftp->topic.lock);
		}
	}

	/* Remove from topic list. */
	for (prev = NULL, ep = wp->dw_topic->writers;
	     ep && ep != &wp->dw_ep;
	     prev = ep, ep = ep->next)
		;

	if (ep) {
		/* Still in topic list: remove it. */
		if (prev)
			prev->next = ep->next;
		else
			wp->dw_topic->writers = ep->next;
	}

	/* Cleanup all Endpoint data. */
	locator_list_delete_list (&wp->dw_ucast);
	locator_list_delete_list (&wp->dw_mcast);
	qos_disc_writer_free (wp->dw_qos);
	wp->dw_qos = NULL;
#ifdef DDS_TYPECODE
	if (wp->dw_tc && wp->dw_tc != TC_IS_TS) {
		vtc_free (wp->dw_tc);
		wp->dw_tc = NULL;
	}
#endif

	if (ignore) {
		wp->dw_flags &= ~EF_NOT_IGNORED;
		lock_release (tp->lock);
	}
	else {
		/* Free the endpoint. */
		pp = wp->dw_participant;
		endpoint_delete (pp, &wp->dw_ep);
		topic_delete (pp, tp, p_last_topic, topic_gone);
	}
}

/* disc_publication_remove -- Remove a Discovered Writer.
			      On entry/exit: DP locked. */

void disc_publication_remove (Participant_t      *pp,
			      DiscoveredWriter_t *wp)
{
	Topic_t		*tp = wp->dw_topic;
	Domain_t	*dp = tp->domain;
	InstanceHandle	topic_handle = tp->entity.handle;
	int		last_p_topic, topic_gone;

	if (entity_shutting_down (wp->dw_flags))
		return;

	wp->dw_flags |= EF_SHUTDOWN;
	if (pp->p_domain->builtin_readers [BT_Publication])
		user_notify_delete (dp, BT_Publication, wp->dw_handle);

	discovered_writer_cleanup (wp, 0, &last_p_topic, &topic_gone);

	if (topic_gone && pp->p_domain->builtin_readers [BT_Topic])
		user_notify_delete (dp, BT_Topic, topic_handle);
}

/* disc_match_readers_update -- Update matches with local readers.
				On entry/exit: DP, T locked. */

static void disc_match_readers_update (Topic_t            *tp,
				       DiscoveredWriter_t *dwp,
				       const UniQos_t     *qp,
				       int                incompatible)
{
	Endpoint_t	*ep;
	Reader_t	*rp;
	int 		old_match, new_match;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;

	dp = tp->domain;
#endif

	for (ep = tp->readers; ep; ep = ep->next) {
		if (!local_active (ep->entity.flags))
			continue;

		rp = (Reader_t *) ep;
#ifndef RW_TOPIC_LOCK
		if (lock_take (rp->r_lock)) {
			warn_printf ("disc_match_readers_update: reader lock error");
			continue;
		}
#endif
		old_match = (dwp->dw_rtps &&
			     rtps_reader_matches (rp, dwp));
		new_match = 0;
		if (incompatible)
			/* Different types: cannot match! */;
		else if (!qos_same_partition (rp->r_subscriber->qos.partition,
					      qp->partition))
			dcps_requested_incompatible_qos (rp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qp, NULL,
			     	     qos_ptr (rp->r_qos), &rp->r_subscriber->qos,
				     &qid))
			dcps_requested_incompatible_qos (rp, qid);
		else
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		     if (!ACCESS_CONTROL (dp) ||
			 !sec_check_local_reader_match (dp->participant.p_permissions,
			    			        dwp->dw_participant->p_permissions,
						        rp,
						        &dwp->dw_ep))
#endif
			new_match = 1;

		if (old_match && !new_match)
			disc_end_matched_writer (rp, dwp);
		else if (!old_match && new_match)
			disc_new_matched_writer (rp, dwp);
#ifndef RW_TOPIC_LOCK
		lock_release (rp->r_lock);
#endif
	}
}

/* disc_publication_update -- Update a Discovered Writer.
			      On entry/exit: DP locked. */

int disc_publication_update (Participant_t        *pp,
			     DiscoveredWriter_t   *dwp,
			     DiscoveredWriterData *info)
{
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	LocatorList_t	tlp;
	UniQos_t	qp;
	int		incompatible = 0;

	/* If the topic name/type name changes or the Writer/
	   Topic QoS becomes incompatible, simply delete and
	   recreate the endpoint. */
	if (strcmp (str_ptr (dwp->dw_topic->name),
		    str_ptr (info->topic_name)) ||
	    strcmp (str_ptr (dwp->dw_topic->type->type_name),
		    str_ptr (info->type_name)) ||
	    update_dw_topic_qos (dwp->dw_topic, info) ||
	    qos_disc_writer_update (&dwp->dw_qos, &info->qos)
#ifdef DDS_TYPECODE
	 || (info->typecode && 
	     !tc_update (&dwp->dw_ep, &dwp->dw_tc,
	     		 &info->typecode, &incompatible))
#endif
	    ) {
		tp = dwp->dw_topic;
		disc_publication_remove (pp, dwp);
		qos_disc_writer_set (&qp, &info->qos);
		return (disc_publication_add (pp, dwp, &qp, tp, NULL, info) ?
					 DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES);
	}
	tp = dwp->dw_topic;
	if (lock_take (tp->lock)) {
		warn_printf ("disc_publication_update: topic lock error");
		return (DDS_RETCODE_ERROR);
	}

	/* Update locator lists - notify if changed. */
	if (!locator_list_equal (dwp->dw_ucast, info->proxy.ucast)) {
		locator_list_swap (dwp->dw_ucast, info->proxy.ucast, tlp);
		if (dwp->dw_rtps)
			rtps_endpoint_locators_update (&dwp->dw_ep, 0);
	}
	if (!locator_list_equal (dwp->dw_mcast, info->proxy.mcast)) {
		locator_list_swap (dwp->dw_mcast, info->proxy.mcast, tlp);
		if (dwp->dw_rtps)
			rtps_endpoint_locators_update (&dwp->dw_ep, 1);
	}
	if (pp->p_domain->builtin_readers [BT_Publication])
		user_writer_notify (dwp, 0);

	/* Check if not ignored. */
	if ((dwp->dw_flags & EF_NOT_IGNORED) == 0) {
		lock_release (tp->lock);
		return (DDS_RETCODE_OK);
	}

	/* No match yet -- can we match local readers with the publication? */
	if (incompatible)
		dcps_inconsistent_topic (tp);

	disc_match_readers_update (dwp->dw_topic, dwp,
					&dwp->dw_qos->qos, incompatible);
	for (ftp = tp->filters; ftp; ftp = ftp->next)
		disc_match_readers_update (&ftp->topic, dwp, 
					&dwp->dw_qos->qos, incompatible);

	lock_release (tp->lock);
	return (DDS_RETCODE_OK);
}

#endif /* SIMPLE_DISCOVERY */


