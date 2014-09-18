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

/* disc_main.c -- Implements the SPDP and SEDP discovery protocols which are used to
	          discover new DDS participants and endpoints. */

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include "sys.h"
#include "log.h"
#include "str.h"
#include "error.h"
#include "skiplist.h"
#include "pool.h"
#include "timer.h"
#include "debug.h"
#include "guid.h"
#include "parse.h"
#include "pl_cdr.h"
#include "rtps.h"
#include "uqos.h"
#include "dds.h"
#include "guard.h"
#include "dcps.h"
#include "dds/dds_debug.h"
#if defined (NUTTX_RTOS)
#include "dds/dds_plugin.h"
#else
#include "dds/dds_security.h"
#endif
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_access.h"
#include "sec_crypto.h"
#endif
#endif
#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#ifdef DDS_FORWARD
#include "rtps_fwd.h"
#endif
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_match.h"
#include "disc_pub.h"
#include "disc_sub.h"
#include "disc_msg.h"
#ifdef DDS_NATIVE_SECURITY
#include "disc_psmp.h"
#include "disc_ctt.h"
#endif
#include "disc_sedp.h"
#include "disc_spdp.h"
#ifdef DDS_QEO_TYPES
#include "disc_policy_updater.h"
#endif

int	disc_log;		/* General Discovery logging. */

/* disc_rem_topic_add -- Add a new Topic as discovered by a protocol. */

static int disc_rem_topic_add (Participant_t       *pp,
			       Topic_t             *tp,
			       DiscoveredTopicData *info)
{
	tp->qos = qos_disc_topic_new (&info->qos);
	if (!tp->qos) {
		topic_delete (pp, tp, NULL, NULL);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	if (pp->p_domain->builtin_readers [BT_Topic])
		user_topic_notify (tp, 1);

	return (DDS_RETCODE_OK);
}

/* disc_remote_topic_add -- Add a new discovered Topic as if it was discovered
			    via a Discovery protocol. */

Topic_t *disc_remote_topic_add (Participant_t *pp,
				DiscoveredTopicData *data)
{
	Topic_t	*tp;
	int	new_node;

	tp = topic_create (pp, NULL, str_ptr (data->name), str_ptr (data->type_name), &new_node);
	if (!tp || !new_node)
		return (NULL); /* Can't create info -- just ignore. */

	if (disc_rem_topic_add (pp, tp, data))
		return (NULL);

	return (tp);
}

#ifdef SIMPLE_DISCOVERY
#ifdef TOPIC_DISCOVERY

/* disc_rem_topic_remove -- Remove a discovered Topic reference. */

static int disc_rem_topic_remove (Participant_t *pp, Topic_t *tp)
{
	if (pp->p_domain->builtin_readers [BT_Topic])
		user_notify_delete (pp->p_domain, BT_Topic, tp->entity.handle);
	
	topic_delete (pp, tp);
	return (DDS_RETCODE_OK);
}

/* disc_rem_topic_update -- Update a Topic. */

static int disc_rem_topic_update (Participant_t       *pp,
				  Topic_t             *tp,
				  DiscoveredTopicData *info)
{
	/* If the Topic QoS becomes incompatible, simply delete and recreate
	   the topic. */
	if (qos_disc_topic_update (&tp->qos, &info->qos)) {
		disc_rem_topic_remove (pp, tp);
		disc_remote_topic_add (pp, info);
	}
	if (pp->p_domain->builtin_readers [BT_Topic])
		user_topic_notify (tp, 0);

	return (DDS_RETCODE_OK);
}

/* sedp_topic_info -- Add/update received topic info to the topic data that a
		      participant has sent. */

static void sedp_topic_info (Participant_t       *pp,
			     Topic_t             *tp,
			     DiscoveredTopicData *info,
			     InfoType_t          type)
{
	switch (type) {
		case EI_NEW:
			disc_rem_topic_add (pp, tp, info);
			break;

		case EI_UPDATE:
			disc_rem_topic_update (pp, tp, info);
			break;

		case EI_DELETE:
			disc_rem_topic_remove (pp, tp);
			return;
	}
}

#endif
#endif /* SIMPLE_DISCOVERY */

/* disc_remote_participant_add -- Add a peer domain participamt on a domain.
				  Locked on entry/exit: DP. */

Participant_t *disc_remote_participant_add (Domain_t                      *domain,
					    SPDPdiscoveredParticipantData *info,
					    LocatorList_t                 srcs,
					    int                           authorized)
{
	Participant_t	*pp;
	LocatorRef_t	*rp, *srp;
	LocatorNode_t	*np, *snp;

	pp = participant_create (domain, &info->proxy.guid_prefix, NULL);
	if (!pp)
		return (NULL);

	version_set (pp->p_proto_version, info->proxy.proto_version);
	vendor_id_set (pp->p_vendor_id, info->proxy.vendor_id);
	pp->p_exp_il_qos = info->proxy.exp_il_qos;
	pp->p_builtins = info->proxy.builtins;
	pp->p_no_mcast = info->proxy.no_mcast;
	pp->p_sw_version = info->proxy.sw_version;
#ifdef DDS_SECURITY
	pp->p_id_tokens = info->id_tokens;
	pp->p_p_tokens = info->p_tokens;
	info->id_tokens = NULL;
	info->p_tokens = NULL;
#endif
	if (!authorized) {
		pp->p_def_ucast = NULL;
		pp->p_def_mcast = NULL;
		pp->p_meta_ucast = NULL;
		pp->p_meta_mcast = NULL;
#ifdef DDS_SECURITY
		pp->p_permissions = 0;
		pp->p_sec_caps = 0;
		pp->p_sec_locs = NULL;
#endif
		pp->p_forward = 0;
	}
	else {
		if (pp->p_vendor_id [0] == VENDORID_H_TECHNICOLOR &&
		    pp->p_vendor_id [1] == VENDORID_L_TECHNICOLOR) {
#ifdef DDS_SECURITY
			pp->p_permissions = info->proxy.permissions;
			pp->p_sec_caps = info->proxy.sec_caps;
			pp->p_sec_locs = info->proxy.sec_locs;
			info->proxy.sec_locs = NULL;
#endif
			pp->p_forward = info->proxy.forward;
		}
		else {
			pp->p_no_mcast = 0;
			pp->p_sw_version = 0;
#ifdef DDS_SECURITY
			pp->p_permissions = 0;
#ifdef DDS_NATIVE_SECURITY
			pp->p_sec_caps = (SECC_DDS_SEC << SECC_LOCAL) | SECC_DDS_SEC;
#else
			pp->p_sec_caps = 0;
#endif
			pp->p_sec_locs = NULL;
#endif
			pp->p_forward = 0;
		}
		pp->p_def_ucast = info->proxy.def_ucast;
		info->proxy.def_ucast = NULL;
		pp->p_def_mcast = info->proxy.def_mcast;
		info->proxy.def_mcast = NULL;
		pp->p_meta_ucast = info->proxy.meta_ucast;
		info->proxy.meta_ucast = NULL;
		pp->p_meta_mcast = info->proxy.meta_mcast;
		info->proxy.meta_mcast = NULL;
#ifdef DUMP_LOCATORS
		dbg_printf ("DUC:");
		locator_list_dump (pp->p_def_ucast);
		dbg_printf (";DMC:");
		locator_list_dump (pp->p_def_mcast);
		dbg_printf (";MUC:");
		locator_list_dump (pp->p_meta_ucast);
		dbg_printf (";MMC:");
		locator_list_dump (pp->p_meta_mcast);
		dbg_printf (";\r\n");
#endif
	}
	pp->p_man_liveliness = info->proxy.manual_liveliness;
	pp->p_user_data = info->user_data;
	info->user_data = NULL;
	pp->p_entity_name = info->entity_name;
	info->entity_name = NULL;
	pp->p_lease_duration = info->lease_duration;
	sl_init (&pp->p_endpoints, sizeof (Endpoint_t *));
	sl_init (&pp->p_topics, sizeof (Topic_t *));

	pp->p_alive = 0;

	if (!authorized)
		return (pp);

	pp->p_flags |= EF_NOT_IGNORED;

#ifdef DDS_NO_MCAST
	pp->p_no_mcast = 1;
#else
	if (locator_list_no_mcast (domain->domain_id, pp->p_def_ucast))
		pp->p_no_mcast = 1;
#endif

#ifdef DDS_FORWARD
	rfwd_participant_new (pp, 0);
#endif

	/* Notify user of participant existence. */
	if (pp->p_domain->builtin_readers [BT_Participant]) {
		user_participant_notify (pp, 1);

		/* As a result of the notification, user may have done an
		   ignore_participant().  If so, we don't continue. */
		if ((pp->p_flags & EF_NOT_IGNORED) == 0)
			return (NULL);
	}

	/* Remember if locally reachable. */
	pp->p_local = 0;
	if (srcs) {
		foreach_locator (srcs, srp, snp) {
			if (!pp->p_local &&
			    (snp->locator.kind & LOCATOR_KINDS_UDP) != 0)
				foreach_locator (pp->p_def_ucast, rp, np)
					if (locator_addr_equal (&np->locator, &snp->locator)) {
						pp->p_local = sys_ticks_last;
						break;
					}

			/* Remember who sent this. */
			locator_list_copy_node (&pp->p_src_locators, snp);
		}
	}
	return (pp);
}
/* disc_remote_reader_add -- Add a new discovered Reader as if it was discovered
			     via a Discovery protocol.
			     On entry/exit: no locks taken. */

DiscoveredReader_t *disc_remote_reader_add (Participant_t *pp,
					    DiscoveredReaderData *info)
{
	DiscoveredReader_t	*drp;
	UniQos_t		qos;
	int			new_node;

	if (lock_take (pp->p_domain->lock))
		return (NULL);

	drp = (DiscoveredReader_t *) endpoint_create (pp, pp, 
					&info->proxy.guid.entity_id, &new_node);
	if (drp && new_node) {
		qos_disc_reader_set (&qos, &info->qos);
		disc_subscription_add (pp, drp, &qos, NULL, NULL, info);
	}
	lock_release (pp->p_domain->lock);
	return (drp);
}

/* disc_remote_writer_add -- Add a new discovered Writer as if it was discovered
			     via a Discovery protocol. On entry/exit: no locks. */

DiscoveredWriter_t *disc_remote_writer_add (Participant_t        *pp,
					    DiscoveredWriterData *info)
{
	DiscoveredWriter_t	*dwp;
	UniQos_t		qos;
	int			new_node;

	if (lock_take (pp->p_domain->lock))
		return (NULL);

	dwp = (DiscoveredWriter_t *) endpoint_create (pp, pp, 
					&info->proxy.guid.entity_id, &new_node);
	if (dwp && new_node) {
		qos_disc_writer_set (&qos, &info->qos);
		disc_publication_add (pp, dwp, &qos, NULL, NULL, info);
	}
	lock_release (pp->p_domain->lock);
	return (dwp);
}

#ifdef RTPS_USED
#ifdef SIMPLE_DISCOVERY

static void disc_notify_listener (Entity_t *ep, NotificationType_t t)
{
	Reader_t	*rp = (Reader_t *) ep;
	Domain_t	*dp = rp->r_subscriber->domain;
	int		secure;

	if (lock_take (dp->lock)) {
		warn_printf ("disc_notify_listener: domain lock error");
		return;
	}
	if (lock_take (rp->r_lock)) {
		warn_printf ("disc_notify_listener: lock error");
		lock_release (dp->lock);
		return;
	}
	if (rp->r_entity_id.id [1] == 0) {	/* SEDP */
#ifdef DDS_NATIVE_SECURITY
		if (rp->r_entity_id.id [0] == 0xff)
			secure = 1;
		else
#endif
			secure = 0;
		switch (rp->r_entity_id.id [2]) {
#ifdef TOPIC_DISCOVERY
			case 2:			/* SEDP::TOPIC_READER */
				sedp_topic_event (rp, t, secure);
				break;
#endif
			case 3:			/* SEDP::PUBLICATIONS_READER */
				sedp_publication_event (rp, t, 0, secure);
				break;
			case 4:			/* SEDP::SUBSCRIPTIONS_READER */
				sedp_subscription_event (rp, t, 0, secure);
				break;
			default:
				break;
		}
	}
	else if (rp->r_entity_id.id [1] == 1)	/* SPDP */
		spdp_event (rp, t);
	else if (rp->r_entity_id.id [1] == 2) {	/* PMSG */
#ifdef DDS_NATIVE_SECURITY
		if (rp->r_entity_id.id [2] == 0)
#endif
			msg_data_event (rp, t, rp->r_entity_id.id [0] == 0xff);
#ifdef DDS_NATIVE_SECURITY
		else if (rp->r_entity_id.id [2] == 1)
			psmp_event (rp, t);
		else if (rp->r_entity_id.id [2] == 2)
			ctt_event (rp, t);
#endif
	}
#ifdef DDS_QEO_TYPES
	else if (rp->r_entity_id.id [1] == 4) {
		if (rp->r_entity_id.id [2] == 1)
			policy_updater_data_event (rp, t, rp->r_entity_id.id [0] == 0xff);
	}
#endif
	lock_release (rp->r_lock);
	lock_release (dp->lock);
}
#endif /* SIMPLE_DISCOVERY */
#endif /* RTPS_USED */

/* disc_start -- Start the discovery protocols. */

int disc_start (Domain_t *domain)		/* Domain. */
{
#ifdef SIMPLE_DISCOVERY
	Publisher_t	*up;
	Subscriber_t	*sp;
#endif
	DDS_ReturnCode_t error;

	if (lock_take (domain->lock)) {
		warn_printf ("disc_start: domain lock error");
		return (DDS_RETCODE_ERROR);
	}
	disc_log = log_logged (DISC_ID, 0);

#ifdef SIMPLE_DISCOVERY

	if (!rtps_used) {
		lock_release (domain->lock);
		return (DDS_RETCODE_OK);
	}

	/* Create builtin Publisher if was not yet created. */
	if (!domain->builtin_publisher) {
		up = publisher_create (domain, 1);
		if (!up) {
			lock_release (domain->lock);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}

		qos_publisher_new (&up->qos, &qos_def_publisher_qos);
		up->def_writer_qos = qos_def_writer_qos;
	}

	/* Create builtin Subscriber if it was not yet created. */
	if (!domain->builtin_subscriber) {
		sp = subscriber_create (domain, 1);
		if (!sp) {
			lock_release (domain->lock);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		qos_subscriber_new (&sp->qos, &qos_def_subscriber_qos);
		sp->def_reader_qos = qos_def_reader_qos;
	}
	lock_release (domain->lock);


	/* Currently we only have the SPDP and SEDP protocols implemented. */
	spdp_log = log_logged (SPDP_ID, 0);
	sedp_log = log_logged (SEDP_ID, 0);
	error = spdp_start (domain);

	/* Other discovery protocols could be started here. */

#else
	ARG_NOT_USED (domain)

	error = DDS_RETCODE_OK;
#endif
	return (error);
}

/* disc_stop -- Stop the discovery protocols. Called from
		rtps_participant_delete with domain_lock and global_lock taken. */

void disc_stop (Domain_t *domain)
{
#ifdef SIMPLE_DISCOVERY
	if (!rtps_used)
		return;

	spdp_stop (domain);

	/* Delete builtin publisher/subscriber. */
	if (domain->builtin_publisher) {
		publisher_delete (domain->builtin_publisher);
		domain->builtin_publisher = NULL;
	}
	if (domain->builtin_subscriber) {
		subscriber_delete (domain->builtin_subscriber);
		domain->builtin_subscriber = NULL;
	}

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
#endif
}

/* disc_send_participant_liveliness -- Resend Asserted Participant liveliness. */

int disc_send_participant_liveliness (Domain_t *dp)
{
	int	error = DDS_RETCODE_OK;

#ifdef SIMPLE_DISCOVERY
	if (rtps_used)
		error = spdp_send_participant_liveliness (dp);
#else
	ARG_NOT_USED (dp)
#endif
	return (error);
}

/* disc_participant_update -- Specifies that a domain participant was updated.*/

int disc_participant_update (Domain_t *domain)
{
#ifdef SIMPLE_DISCOVERY
	int	error;

	if (rtps_used) {
		error = spdp_update (domain);
		if (error)
			return (error);
	}

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_participant_rehandshake -- Initiate a rehandshake due to policy
				   changes. */

int disc_participant_rehandshake (Domain_t *dp, int notify_only)
{
	int	error = DDS_RETCODE_OK;

#if defined (SIMPLE_DISCOVERY) && defined (DDS_NATIVE_SECURITY)
	if (rtps_used)
		error = spdp_rehandshake (dp, notify_only);
#else
	ARG_NOT_USED (dp)
	ARG_NOT_USED (notify_only)
#endif
	return (error);
}

/* disc_writer_add -- A new writer endpoint was added.
		      On entry/exit: all locks taken (DP,P,T,W). */

int disc_writer_add (Domain_t *domain, Writer_t *wp)
{
#ifdef SIMPLE_DISCOVERY
	int	error;

	if (rtps_used) {
		error = sedp_writer_add (domain, wp);
		if (error)
			return (error);
	}

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (wp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_writer_update -- A new writer endpoint was updated.
		         On entry/exit: all locks taken (DP,P,T,W). */

int disc_writer_update (Domain_t             *domain,
			Writer_t             *wp,
			int                  changed,
			DDS_InstanceHandle_t peer)
{
#ifdef SIMPLE_DISCOVERY
	int	error;

	if (rtps_used) {
		error = sedp_writer_update (domain, wp, changed, peer);
		if (error)
			return (error);
	}

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (wp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_writer_remove -- A writer endpoint was removed.
		         On entry/exit: all locks taken (DP,P,T,W). */

int disc_writer_remove (Domain_t *domain, Writer_t *wp)
{
#ifdef SIMPLE_DISCOVERY
	if (rtps_used)
		sedp_writer_remove (domain, wp);

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (wp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_reader_add -- A new reader endpoint was added. */

int disc_reader_add (Domain_t *domain, Reader_t *rp)
{
#ifdef SIMPLE_DISCOVERY
	int	error;

	if (rtps_used) {
		error = sedp_reader_add (domain, rp);
		if (error)
			return (error);
	}

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (rp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_reader_update -- A new reader endpoint was updateed.
		         On entry/exit: all locks taken (DP,S,T,R) */

int disc_reader_update (Domain_t             *domain,
			Reader_t             *rp,
			int                  changed,
			DDS_InstanceHandle_t peer)
{
#ifdef SIMPLE_DISCOVERY
	int	error;

	if (rtps_used) {
		error = sedp_reader_update (domain, rp, changed, peer);
		if (error)
			return (error);
	}

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (rp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_reader_remove -- A reader endpoint was removed.
		         On entry/exit: all locks taken (DP,S,T,R) */

int disc_reader_remove (Domain_t *domain, Reader_t *rp)
{
#ifdef SIMPLE_DISCOVERY
	if (rtps_used)
		sedp_reader_remove (domain, rp);

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (rp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_topic_add -- A topic was added. */

int disc_topic_add (Domain_t *domain, Topic_t *tp)
{
	ARG_NOT_USED (domain)

#if defined (SIMPLE_DISCOVERY) && defined (TOPIC_DISCOVERY)

	if (rtps_used)
		sedp_topic_add (domain, tp);

#else
	ARG_NOT_USED (tp)

#endif	/* Other discovery protocols should be notified here. */

	return (DDS_RETCODE_OK);
}

/* disc_topic_remove -- A topic was removed. */

int disc_topic_remove (Domain_t *domain, Topic_t *tp)
{
	ARG_NOT_USED (domain)
#if defined (SIMPLE_DISCOVERY) && defined (TOPIC_DISCOVERY)

	if (rtps_used)
		sedp_topic_remove (domain, tp);
#else
	ARG_NOT_USED (tp)

#endif	/* Other discovery protocols should be notified here. */

	return (DDS_RETCODE_OK);
}

/* disc_endpoint_locator -- Add/remove a locator to/from an endpoint. */

int disc_endpoint_locator (Domain_t        *domain,
			   LocalEndpoint_t *ep,
			   int             add,
			   int             mcast,
			   const Locator_t *loc)
{
#ifdef SIMPLE_DISCOVERY
	if (rtps_used)
		sedp_endpoint_locator (domain, ep, add, mcast, loc);

	/* Other discovery protocols should be notified here. */
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (ep)
	ARG_NOT_USED (add)
	ARG_NOT_USED (mcast)
	ARG_NOT_USED (loc)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_ignore_participant -- Ignore a discovered participant. */

int disc_ignore_participant (Participant_t *pp)
{
#ifdef SIMPLE_DISCOVERY
	if (rtps_used)
		spdp_end_participant (pp, 1);
#else
	ARG_NOT_USED (pp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_ignore_topic -- Ignore a discovered topic. */

int disc_ignore_topic (Topic_t *tp)
{
#ifdef SIMPLE_DISCOVERY
	Endpoint_t	*ep, *next_ep;

	if (!rtps_used || entity_ignored (tp->entity.flags))
		return (DDS_RETCODE_OK);

	if ((tp->entity.flags & EF_REMOTE) == 0)
		return (DDS_RETCODE_OK);

	for (ep = tp->readers; ep; ep = next_ep) {
		next_ep = ep->next;
		if ((ep->entity.flags & EF_REMOTE) != 0)
			discovered_reader_cleanup ((DiscoveredReader_t *) ep, 0, NULL, NULL);
	}
	for (ep = tp->writers; ep; ep = next_ep) {
		next_ep = ep->next;
		if ((ep->entity.flags & EF_REMOTE) != 0)
			discovered_writer_cleanup ((DiscoveredWriter_t *) ep, 0, NULL, NULL);
	}
	tp->entity.flags &= ~EF_NOT_IGNORED;
#else
	ARG_NOT_USED (tp)
#endif

	return (DDS_RETCODE_OK);
}

/* disc_ignore_writer -- Ignore a discovered writer. */

int disc_ignore_writer (DiscoveredWriter_t *wp)
{
#ifdef SIMPLE_DISCOVERY
	if (!rtps_used || entity_ignored (wp->dw_flags))
		return (DDS_RETCODE_OK);

	discovered_writer_cleanup (wp, 1, NULL, NULL);
#else
	ARG_NOT_USED (wp)
#endif
	return (DDS_RETCODE_OK);
}

/* disc_ignore_reader -- Ignore a discovered reader. */

int disc_ignore_reader (DiscoveredReader_t *rp)
{
#ifdef SIMPLE_DISCOVERY
	if (!rtps_used || entity_ignored (rp->dr_flags))
		return (DDS_RETCODE_OK);

	discovered_reader_cleanup (rp, 1, NULL, NULL);
#else
	ARG_NOT_USED (rp)
#endif
	return (DDS_RETCODE_OK);
}

#ifdef RTPS_USED

struct pop_bi_st {
	Domain_t	*domain;
	Builtin_Type_t	type;
};

static int populate_endpoint (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	struct pop_bi_st *bip = (struct pop_bi_st *) arg;

	ARG_NOT_USED (list)

	ep = *epp;
	if ((ep->entity.flags & EF_BUILTIN) != 0)
		return (1);

	if (lock_take (ep->topic->lock)) {
		warn_printf ("populate_endpoint: topic lock error");
		return (0);
	}
	if (entity_writer (entity_type (&ep->entity)) && bip->type == BT_Publication)
		user_writer_notify ((DiscoveredWriter_t *) ep, 1);
	else if (entity_reader (entity_type (&ep->entity)) && bip->type == BT_Subscription)
		user_reader_notify ((DiscoveredReader_t *) ep, 1);
	lock_release (ep->topic->lock);

	return (1);
}

static int populate_participant (Skiplist_t *list, void *node, void *arg)
{
	Participant_t	*pp, **ppp = (Participant_t **) node;
	struct pop_bi_st *bip = (struct pop_bi_st *) arg;

	ARG_NOT_USED (list)

	pp = *ppp;
	if (bip->type == BT_Participant)
		user_participant_notify (pp, 1);
	else if (sl_length (&pp->p_endpoints))
		sl_walk (&pp->p_endpoints, populate_endpoint, arg);

	return (1);
}

static int populate_topic (Skiplist_t *list, void *node, void *arg)
{
	Topic_t	*tp, **tpp = (Topic_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	tp = *tpp;
	if (!tp->nlrefs && tp->nrrefs)
		user_topic_notify (tp, 1);

	return (1);
}

/* disc_populate_builtin -- Add already discovered data to a builtin reader. */

int disc_populate_builtin (Domain_t *dp, Builtin_Type_t type)
{
	struct pop_bi_st	data;

	if (lock_take (dp->lock)) {
		warn_printf ("disc_populate_builtin: domain lock error");
		return (DDS_RETCODE_ERROR);
	}
	if (type == BT_Topic) {
		if (sl_length (&dp->participant.p_topics))
			sl_walk (&dp->participant.p_topics, populate_topic, 0);
	}
	else if (sl_length (&dp->peers)) {
		data.domain = dp;
		data.type = type;
		sl_walk (&dp->peers, populate_participant, &data);
	}
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

#endif /* RTPS_USED */

/* disc_send_liveliness_msg == Send either a manual or automatic liveliness
			       message. */

int disc_send_liveliness_msg (Domain_t *dp, unsigned kind)
{
	int	error;

#if defined (RTPS_USED) && defined (SIMPLE_DISCOVERY)
	error = msg_send_liveliness (dp, kind);
#else
	ARG_NOT_USED (dp)
	ARG_NOT_USED (kind)

	error = DDS_RETCODE_OK;
#endif
	return (error);
}

/* disc_suspend_participant -- Suspend activated for a participant. */

static int disc_suspend_participant (Skiplist_t *list, void *node, void *arg)
{
	Participant_t	*pp, **ppp = (Participant_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	pp = *ppp;
	pp->p_alive = 0;
	return (1);
}

/* disc_suspend -- Suspend discovery. */

void disc_suspend (void)
{
	Domain_t	*dp;
	unsigned	i = 0;

	for (i = 0; ; ) {
		dp = domain_next (&i, NULL);
		if (!dp)
			return;

		lock_take (dp->lock);
		sl_walk (&dp->peers, disc_suspend_participant, NULL);
		lock_release (dp->lock);
	}
}


/* disc_resume -- Resume discovery. */

void disc_resume (void)
{
}

/* disc_init -- Initialize the Discovery module. */

int disc_init (void)
{
#ifdef RTPS_USED
#ifdef SIMPLE_DISCOVERY
	if (!rtps_used)
		return (DDS_RETCODE_OK);

	spdp_init ();

	dds_attach_notifier (NSC_DISC, disc_notify_listener);
#endif
#endif
	return (DDS_RETCODE_OK);
}

/* disc_final -- Finalize the Discovery module. */

void disc_final (void)
{
#ifdef SIMPLE_DISCOVERY
	spdp_final ();
#endif
}

#ifdef DDS_DEBUG

/* disc_dump -- Debug: dump the discovered participants and endpoints. */

void disc_dump (int all)
{
	unsigned flags;

	flags = DDF_LOCATORS_L | DDF_LOCATORS_R | DDF_PEERS;
	if (all)
		flags |= DDF_ENDPOINTS_L | DDF_ENDPOINTS_R |
			 DDF_TOPICS_L | DDF_TOPICS_R |
			 DDF_GUARD_L | DDF_GUARD_R;
	dump_domains (flags);
}

#endif

