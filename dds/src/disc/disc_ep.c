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

/* disc_ep -- Builtin discovery endpoints support function. */

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
#include "error.h"
#include "dds.h"
#include "rtps.h"
#include "dcps.h"
#include "dds/dds_dcps.h"
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_access.h"
#include "sec_crypto.h"
#include "sec_c_std.h"
#endif
#endif
#include "dds_data.h"
#include "pid.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_msg.h"
#ifdef DDS_NATIVE_SECURITY
#include "disc_ctt.h"
#ifdef DDS_QEO_TYPES
#include "disc_policy_updater.h"
#endif
#endif
#include "disc_ep.h"

/* disc_get_data -- Get a discovery protocol message from the history cache. */

DDS_ReturnCode_t disc_get_data (Reader_t *rp, ChangeData_t *c)
{
	Change_t	*cp;
	void		*auxp;
	unsigned	nchanges;
	DDS_ReturnCode_t error;

	nchanges = 1;
	error = hc_get_data (rp->r_cache, &nchanges, &cp, SKM_READ, 0, 0, 1);
	if (error)
		return (error);

	if (!nchanges)
		return (DDS_RETCODE_NO_DATA);

	c->kind   = cp->c_kind;
	c->is_new = cp->c_vstate == NEW;
	c->h      = cp->c_handle;
	c->writer = cp->c_writer;
	c->time   = cp->c_time;
	if (cp->c_kind == ALIVE) {
		c->data = dcps_get_cdata (NULL, cp,
					  rp->r_topic->type->type_support, 0,
					  &error, &auxp);
		if (error)
			log_printf (DISC_ID, 0, "disc_get_data({%u}) returned error %d!\r\n",
					       rp->r_handle, error);
	}
	else {
		c->data = NULL;
		error = DDS_RETCODE_OK;
	}
	hc_change_free (cp);
	return (error);
}

/* add_locators -- Add locators to an endpoint. */

static void add_locators (LocalEndpoint_t     *ep,
			  int	              writer,
			  const LocatorList_t uc_locs,
			  const LocatorList_t mc_locs,
			  const LocatorList_t dst_locs)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	int		error;

	foreach_locator (uc_locs, rp, np) {
		error = rtps_endpoint_add_locator (ep, 0, &np->locator);
		if (error)
			fatal_printf ("SPDP: can't add endpoint unicast destination!");
	}
	foreach_locator (mc_locs, rp, np) {
		error = rtps_endpoint_add_locator (ep, 1, &np->locator);
		if (error)
	    		fatal_printf ("SPDP: can't add endpoint multicast destination!");
	}
	if (writer)
		foreach_locator (dst_locs, rp, np) {
			if (!lock_take (((Writer_t *) ep)->w_lock)) {
				error = rtps_reader_locator_add ((Writer_t *) ep, np, 0, 1);
				lock_release (((Writer_t *) ep)->w_lock);
				if (error)
					fatal_printf ("SPDP: can't add writer destination!");
			}
		}
}

/* disc_data_available -- Data available indication from cache. */

void disc_data_available (uintptr_t user, Cache_t cdp)
{
	Reader_t	*rp = (Reader_t *) user;
	int		notify = (rp->r_status & DDS_DATA_AVAILABLE_STATUS) == 0;

	ARG_NOT_USED (cdp)

	rp->r_status |= DDS_DATA_AVAILABLE_STATUS;
	if (notify)
		dds_notify (NSC_DISC, (Entity_t *) rp, NT_DATA_AVAILABLE);
}

/* create_builtin_endpoint -- Create a built-in endpoint.
			      On entry/exit: no locks taken. */

int create_builtin_endpoint (Domain_t            *dp,
			     BUILTIN_INDEX       index,
			     int                 push_mode,
			     int                 stateful,
			     int                 reliable,
			     int                 keep_all,
			     int                 transient_local,
			     const Duration_t    *resend_per,
			     const LocatorList_t uc_locs,
			     const LocatorList_t mc_locs,
			     const LocatorList_t dst_locs)
{
	unsigned		type;
	int			writer;
	DDS_ReturnCode_t	error;
	static const Builtin_Type_t builtin_types [] = {
		BT_Participant,
		BT_Participant,
		BT_Publication,
		BT_Publication,
		BT_Subscription,
		BT_Subscription,
		0, 0, /* Proxy */
		0, 0, /* State */
		0, 0, /* Message */
		BT_Topic,
		BT_Topic
#ifdef DDS_NATIVE_SECURITY
	      ,	0, 0, /* ParticipantStateless */
		BT_Publication,
		BT_Publication,
		BT_Subscription,
		BT_Subscription,
		0, 0, /* Message */
		0, 0  /* ParticipantVolatileSecure */
#endif
#ifdef DDS_QEO_TYPES
	      , 0, 0
#endif
	};
	static const size_t	builtin_dlen [] = {
		sizeof (Participant_t *),
		sizeof (SPDPdiscoveredParticipantData),
		sizeof (Writer_t *),
		sizeof (DiscoveredWriterData),
		sizeof (Reader_t *),
		sizeof (DiscoveredReaderData),
		0, 0, /* Proxy */
		0, 0, /* State */
		sizeof (ParticipantMessageData),
		sizeof (ParticipantMessageData),
		sizeof (Topic_t *),
		sizeof (DiscoveredTopicData)
#ifdef DDS_NATIVE_SECURITY
	      ,	sizeof (DDS_ParticipantStatelessMessage),
		sizeof (DDS_ParticipantStatelessMessage),
		sizeof (Writer_t *),
		sizeof (DiscoveredWriterData),
		sizeof (Reader_t *),
		sizeof (DiscoveredReaderData),
		sizeof (ParticipantMessageData),
		sizeof (ParticipantMessageData),
		sizeof (DDS_ParticipantStatelessMessage),
		sizeof (DDS_ParticipantStatelessMessage)
#endif
#ifdef DDS_QEO_TYPES
	      , sizeof (PolicyUpdaterMessageData),
		sizeof (PolicyUpdaterMessageData)
#endif
	};
	static const size_t	builtin_klen [] = {
		sizeof (GuidPrefix_t),
		sizeof (GuidPrefix_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		0, 0, /* Proxy */
		0, 0, /* State */
		sizeof (GUID_t),
		sizeof (GUID_t),
		0, 0 /* Topic */
#ifdef DDS_NATIVE_SECURITY
	      ,	0, 0, /* ParticipantStateless */
		sizeof (GUID_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		sizeof (GUID_t),
		0, 0
#endif
#ifdef DDS_QEO_TYPES
              , sizeof (GUID_t),
		sizeof (GUID_t)
#endif
	};
	TypeSupport_t		*ts;
	PL_TypeSupport		*plp;
	Endpoint_t		*ep;
	TopicType_t		*typep;
	Topic_t			*tp;
	const EntityId_t	*eid;
	Writer_t		*wp;
	Reader_t		*rp;
	const char		*type_name;
	Cache_t			cache;
	DDS_DataReaderQos	rqos;
	DDS_DataWriterQos	wqos;

	if (lock_take (dp->lock))
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef DDS_NATIVE_SECURITY
	if (NATIVE_SECURITY (dp) &&
	    (index == EPB_PARTICIPANT_W || index == EPB_PARTICIPANT_R))
		type_name = "ParticipantBuiltinTopicDataSecure";
	else
#endif
		type_name = rtps_builtin_topic_names [index];
	typep = type_lookup (dp, type_name);
	if (!typep) {
		typep = type_create (dp, type_name, NULL);
		if (!typep)
			fatal_printf ("create_builtin_endpoint: can't create builtin type (%s/%s)!",
				rtps_builtin_endpoint_names [index], type_name);

		typep->flags |= EF_BUILTIN | EF_LOCAL;
		ts = xmalloc (sizeof (TypeSupport_t) + sizeof (PL_TypeSupport));
		if (!ts)
			fatal_printf ("create_builtin_endpoint: out of memory for type_support!");

		memset (ts, 0, sizeof (TypeSupport_t) + sizeof (PL_TypeSupport));
		plp = (PL_TypeSupport *) (ts + 1);
		ts->ts_name = type_name;
		ts->ts_prefer = MODE_PL_CDR;
		ts->ts_dynamic = 1;
#ifdef DDS_TYPECODE
		ts->ts_origin = TSO_Builtin;
#endif
		ts->ts_length = builtin_dlen [index];
#ifdef DDS_NATIVE_SECURITY
		if (index == EPB_PARTICIPANT_SL_W ||
		    index == EPB_PARTICIPANT_SL_R ||
		    index == EPB_PARTICIPANT_VOL_SEC_W ||
		    index == EPB_PARTICIPANT_VOL_SEC_R) {
			ts->ts_keys = 0;
			ts->ts_fksize = 0;
			ts->ts_mkeysize = 0;
		}
		else {
#endif
			ts->ts_keys = 1;
			ts->ts_fksize = 1;
			ts->ts_mkeysize = builtin_klen [index];
#ifdef DDS_NATIVE_SECURITY
		}
#endif
		plp->builtin = 1;
		plp->type = builtin_types [index];
#ifdef XTYPES_USED
		plp->xtype = NULL;
#endif
		ts->ts_pl = plp;
		typep->type_support = ts;
	}
	tp = topic_lookup (&dp->participant, rtps_builtin_endpoint_names [index]);
	if (!tp) {
		tp = topic_create (&dp->participant,
				   NULL,
				   rtps_builtin_endpoint_names [index],
				   type_name, NULL);
		if (!tp)
			fatal_printf ("create_builtin_endpoint: can't create builtin topic (%s/%s)!",
				rtps_builtin_endpoint_names [index], type_name);

		tp->entity.flags |= EF_BUILTIN;
	}
	eid = &rtps_builtin_eids [index];
	type = eid->id [ENTITY_KIND_INDEX] & ENTITY_KIND_MINOR;
	writer = (type == ENTITY_KIND_WRITER_KEY || type == ENTITY_KIND_WRITER);
	lock_take (tp->lock);
	ep = endpoint_create (&dp->participant,
			      writer ? (void *) dp->builtin_publisher : 
				       (void *) dp->builtin_subscriber,
			      eid, NULL);
	if (!ep)
		fatal_printf ("create_builtin_endpoint: can't create builtin endpoint (%s/%s)!",
			rtps_builtin_endpoint_names [index], type_name);

	ep->entity.flags |= EF_BUILTIN | EF_ENABLED;
	ep->topic = tp;
	if (ep->entity.type == ET_WRITER) {
		wp = (Writer_t *) ep;
#ifdef RW_LOCKS
		lock_take (wp->w_lock);
#endif
		memset (&wp->w_listener, 0, sizeof (wp->w_listener));
		memset (&wp->w_odm_status, 0, sizeof (wp->w_odm_status));
		memset (&wp->w_oiq_status, 0, sizeof (wp->w_oiq_status));
		memset (&wp->w_ll_status, 0, sizeof (wp->w_ll_status));
		memset (&wp->w_pm_status, 0, sizeof (wp->w_pm_status));

		wp->w_publisher->nwriters++;
		wp->w_next = tp->writers;
		tp->writers = &wp->w_ep;
		wqos = qos_def_writer_qos;
		if (reliable)
			wqos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
		if (keep_all) {
			wqos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
			wqos.history.depth = 0;
		}
		if (transient_local)
			wqos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
		ep->qos = qos_writer_new (&wqos);

		/* Create writer endpoint. */
		wp->w_rtps = NULL;
		memset (&wp->w_listener, 0, sizeof (Writer_t) -
					       offsetof (Writer_t, w_listener));

#ifdef DDS_NATIVE_SECURITY

		/* Set the security attributes. */
		wp->w_access_prot = 0;
		wp->w_disc_prot = 0;
		if (eid->id [0] == 0xff &&
		    index != EPB_PARTICIPANT_VOL_SEC_W) {
			wp->w_submsg_prot = DISC_SUBMSG_PROT;
			wp->w_payload_prot = DISC_PAYLOAD_PROT;
			wp->w_crypto_type = DISC_CRYPTO_TYPE;
			wp->w_crypto = sec_register_local_datawriter (dp->participant.p_crypto,
								      wp,
								      &error);
			if (!wp->w_crypto)
				fatal_printf ("create_builtin_endpoint: can't create writer crypto tokens (%s/%s)!",
						rtps_builtin_endpoint_names [index],
						type_name);
		}
		else {
			wp->w_submsg_prot = 0;
			wp->w_payload_prot = 0;
			wp->w_crypto_type = 0;
			wp->w_crypto = 0;
		}
#endif

		/* Create a history cache. */
		wp->w_cache = cache = hc_new ((LocalEndpoint_t *) ep, 0);

		/* Create RTPS Writer context. */
		error = rtps_writer_create (wp, push_mode, stateful,
					    NULL, NULL, NULL, resend_per);
#ifdef RW_LOCKS
	    	lock_release (wp->w_lock);
#endif
	}
	else {
		rp = (Reader_t *) ep;
#ifdef RW_LOCKS
		lock_take (rp->r_lock);
#endif
		memset (&rp->r_time_based_filter, 0, sizeof (rp->r_time_based_filter));
		memset (&rp->r_data_lifecycle, 0, sizeof (rp->r_data_lifecycle));
		memset (&rp->r_listener, 0, sizeof (rp->r_listener));
		memset (&rp->r_rdm_status, 0, sizeof (rp->r_rdm_status));
		memset (&rp->r_riq_status, 0, sizeof (rp->r_riq_status));
		memset (&rp->r_sl_status, 0, sizeof (rp->r_sl_status));
		memset (&rp->r_sr_status, 0, sizeof (rp->r_sr_status));
		memset (&rp->r_lc_status, 0, sizeof (rp->r_lc_status));
		memset (&rp->r_sm_status, 0, sizeof (rp->r_sm_status));

		rp->r_subscriber->nreaders++;
		rp->r_next = tp->readers;
		tp->readers = &rp->r_ep;
		rqos = qos_def_reader_qos;

		if (reliable)
			rqos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
		if (keep_all) {
			rqos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
			rqos.history.depth = 0;
		}
		if (transient_local)
			rqos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
		ep->qos = qos_reader_new (&rqos);

		/* Create reader endpoint. */
		rp->r_rtps = NULL;
		qos_init_time_based_filter (&rp->r_time_based_filter);
		qos_init_reader_data_lifecycle (&rp->r_data_lifecycle);

#ifdef DDS_NATIVE_SECURITY

		/* Set the security attributes. */
		rp->r_access_prot = 0;
		rp->r_disc_prot = 0;
		if (eid->id [0] == 0xff &&
		    index != EPB_PARTICIPANT_VOL_SEC_R) {
			rp->r_submsg_prot = DISC_SUBMSG_PROT;
			rp->r_payload_prot = DISC_PAYLOAD_PROT;
			rp->r_crypto_type = DISC_CRYPTO_TYPE;
			rp->r_crypto = sec_register_local_datareader (dp->participant.p_crypto,
								      rp,
								      &error);
			if (!rp->r_crypto)
				fatal_printf ("create_builtin_endpoint: can't create reader crypto tokens (%s/%s)!",
						rtps_builtin_endpoint_names [index],
						type_name);
		}
		else {
			rp->r_submsg_prot = 0;
			rp->r_payload_prot = 0;
			rp->r_crypto_type = 0;
			rp->r_crypto = 0;
		}
#endif
		/* Create a history cache. */
		rp->r_cache = cache = hc_new ((LocalEndpoint_t *) ep, 0);


		/* Add RTPS Reader context. */
		error = rtps_reader_create (rp, stateful,
					    NULL, NULL);
#ifdef RW_LOCKS
	    	lock_release (rp->r_lock);
#endif
	}
	lock_release (tp->lock);
	if (error || !ep->qos) {
		fatal_printf ("create_builtin_endpoint: can't create built-in endpoint (%u)!", index);
		lock_release (dp->lock);
		return (error);
	}
	dp->participant.p_builtins |= 1 << index;
	dp->participant.p_builtin_ep [index] = ep;
	add_locators ((LocalEndpoint_t *) dp->participant.p_builtin_ep [index],
					writer, uc_locs, mc_locs, dst_locs);
	lock_release (dp->lock);
	return (DDS_RETCODE_OK);
}

/* disable_builtin_writer -- Disable notifications on builtin writers (and
			     purge outstanding notifications). On entry:
			     domain_lock and global_lock taken. */

static void disable_builtin_writer (Writer_t *wp)
{
	Topic_t		*tp;

	tp = wp->w_topic;
	if (lock_take (tp->lock))
		return;

#ifdef RW_LOCKS
	if (lock_take (wp->w_lock))
		goto done;
#endif

	/* Turn off notifications */
	hc_request_notification (wp->w_cache, NULL, (uintptr_t) 0);

	/* Purge remaining notification */
	while (!dds_purge_notifications ((Entity_t *) wp, DDS_ALL_STATUS, 1)) {

		/* Listener is still running, need to block until it is done! */
		/* Release all locks: */
#ifdef RW_LOCKS
		lock_release (wp->w_lock);
#endif
		lock_release (tp->lock);
		lock_release (tp->domain->lock);

		/* Wait */
		dds_wait_listener ((Entity_t *) wp);

		/* Take all locks back: */
		lock_take (tp->domain->lock);
		lock_take (tp->lock);
#ifdef RW_LOCKS
		lock_take (wp->w_lock);
#endif
	}
	wp->w_flags &= ~EF_ENABLED;

#ifdef RW_LOCKS
	lock_release (wp->w_lock);
    done:
#endif
	lock_release (tp->lock);
}

/* delete_builtin_writer -- Delete all writer-specific builtin endpoint data.
			    On entry: DP and global lock taken. */

static void delete_builtin_writer (Writer_t *wp)
{
	Endpoint_t	*ep, *prev_ep;
	Topic_t		*tp;

	tp = wp->w_topic;
	if (lock_take (tp->lock))
		return;

#ifdef RW_LOCKS
	if (lock_take (wp->w_lock))
		goto done;
#endif

	/* Decrease # of writers of builtin publisher. */
	wp->w_publisher->nwriters--;

	/* Remove writer from topic endpoints list. */
	for (ep = tp->writers, prev_ep = NULL;
	     ep;
	     prev_ep = ep, ep = ep->next)
		if (ep == &wp->w_ep) {

#ifdef DDS_NATIVE_SECURITY
			/* Remove crypto material. */
			if (wp->w_crypto)
				sec_unregister_datawriter (wp->w_crypto);
#endif
			/* Remove RTPS writer. */
			rtps_writer_delete (wp);

			/* Remove from topic list. */
			if (prev_ep)
				prev_ep->next = ep->next;
			else
				tp->writers = ep->next;
			break;
		}

#ifdef RW_LOCKS
	lock_release (wp->w_lock);

    done:
#endif
	lock_release (tp->lock);
}

/* disable_builtin_reader -- Disable notifications on builtin reader (and
			     purge outstanding notifications). On entry:
			     domain_lock and global_lock taken. */

static void disable_builtin_reader (Reader_t *rp)
{
	Topic_t		*tp;

	tp = rp->r_topic;
	if (lock_take (tp->lock))
		return;

#ifdef RW_LOCKS
	if (lock_take (rp->r_lock))
		goto done;
#endif

	/* Turn off notifications */
	hc_request_notification (rp->r_cache, NULL, (uintptr_t) 0);

	/* Purge remaining notification */
	while (!dds_purge_notifications ((Entity_t *) rp, DDS_ALL_STATUS, 1)) {

		/* Listener is still running, need to block until it is done! */
		/* Release all locks: */
#ifdef RW_LOCKS
		lock_release (rp->r_lock);
#endif
		lock_release (tp->lock);
		lock_release (tp->domain->lock);

		/* Wait */
		dds_wait_listener ((Entity_t *) rp);

		/* Take all locks back: */
		lock_take (tp->domain->lock);
		lock_take (tp->lock);
#ifdef RW_LOCKS
		lock_take (rp->r_lock);
#endif
	}
	rp->r_flags &= ~EF_ENABLED;

#ifdef RW_LOCKS
	lock_release (rp->r_lock);

done:
#endif
	lock_release (tp->lock);
}

/* delete_builtin_reader -- Delete all reader-specific builtin endpoint data. */

static void delete_builtin_reader (Reader_t *rp)
{
	Endpoint_t	*ep, *prev_ep;
	Topic_t		*tp;

	tp = rp->r_topic;
	if (lock_take (tp->lock))
		return;

#ifdef RW_LOCKS
	if (lock_take (rp->r_lock))
		goto done;
#endif

	/* Decrease # of readers of builtin subscriber. */
	rp->r_subscriber->nreaders--;

	/* Remove writer from topic endpoints list. */
	for (ep = tp->readers, prev_ep = NULL;
	     ep;
	     prev_ep = ep, ep = ep->next)
		if (ep == &rp->r_ep) {

#ifdef DDS_NATIVE_SECURITY
			/* Remove crypto material. */
			if (rp->r_crypto)
				sec_unregister_datareader (rp->r_crypto);
#endif
			/* Remove RTPS Reader. */
			rtps_reader_delete (rp);

			/* Remove from topic list. */
			if (prev_ep)
				prev_ep->next = ep->next;
			else
				tp->readers = ep->next;
			break;
		}
#ifdef RW_LOCKS
	lock_release (rp->r_lock);

    done:
#endif
	lock_release (tp->lock);
}


/* disable_builtin_endpoint -- Turn of notifications on a builtin endpoint, and
			       purge outstanding notifications. On entry/exit:
			       domain and global lock taken. */

void disable_builtin_endpoint (Domain_t *dp, BUILTIN_INDEX index)
{
	LocalEndpoint_t	*ep = (LocalEndpoint_t *) dp->participant.p_builtin_ep [index];

	if (!ep)
		return;

	if (ep->ep.entity.type == ET_WRITER)
		disable_builtin_writer ((Writer_t *) ep);
	else
		disable_builtin_reader ((Reader_t *) ep);
}

/* delete_builtin_endpoint -- Remove a builtin endpoint.
			      On entry/exit: domain and global lock taken. */

void delete_builtin_endpoint (Domain_t *dp, BUILTIN_INDEX index)
{
	LocalEndpoint_t	*ep = (LocalEndpoint_t *) dp->participant.p_builtin_ep [index];
	TopicType_t	*typep;
	Topic_t		*tp;

	if (!ep)
		return;

	/* Delete Reader/Writer specific data. */
	if (ep->ep.entity.type == ET_WRITER)
		delete_builtin_writer ((Writer_t *) ep);
	else
		delete_builtin_reader ((Reader_t *) ep);

	/* Delete Locator lists. */
	if (ep->ep.ucast)
		locator_list_delete_list (&ep->ep.ucast);
	if (ep->ep.mcast)
		locator_list_delete_list (&ep->ep.mcast);

	/* Delete QoS parameters. */
	if (ep->ep.qos)
		qos_free (ep->ep.qos);

	/* Delete History Cache. */
	hc_free (ep->cache);

	/* Delete Typesupport if last one. */
	tp = ep->ep.topic;
	lock_take (tp->lock);
	typep = tp->type;
	if (typep->nrefs == 1) {
		xfree ((void *) typep->type_support);
		typep->type_support = NULL;
	}

	/* Delete domain endpoint. */
	ep->ep.topic = NULL;
	endpoint_delete (&dp->participant, &ep->ep);

	/* Delete topic (frees/destroys topic lock). */
	topic_delete (&dp->participant, tp, NULL, NULL);

	/* Builtin has disappeared. */
	dp->participant.p_builtins &= ~(1 << index);
	dp->participant.p_builtin_ep [index] = NULL;

	/* lock_release (dp->lock); */
}


/* add_peer_builtin -- Add a builtin to a discovered participant. */

static Endpoint_t *add_peer_builtin (Participant_t   *rpp,
				     BUILTIN_INDEX   index,
				     LocalEndpoint_t *lep)
{
	Endpoint_t	*ep;
	Topic_t		*tp;

	ep = endpoint_create (rpp, rpp, &rtps_builtin_eids [index], NULL);
	if (!ep)
		return (NULL);

	ep->topic = tp = lep->ep.topic;
	ep->qos = lep->ep.qos;
	ep->qos->users++;
	if (entity_writer (entity_type (&ep->entity))) {
		ep->next = tp->writers;
		tp->writers = ep;
	}
	else {
		ep->next = tp->readers;
		tp->readers = ep;
	}
	ep->rtps = NULL;
	ep->entity.flags |= EF_BUILTIN | EF_ENABLED;
	rpp->p_builtin_ep [index] = ep;
	return (ep);
}

static void remove_peer_builtin (Participant_t *rpp, BUILTIN_INDEX index)
{
	Topic_t		*tp;
	Endpoint_t	*ep, **ep_list, *prev, *xep;

	ep = rpp->p_builtin_ep [index];
	rpp->p_builtin_ep [index] = NULL;
	tp = ep->topic;

	/* Remove from topic list. */
	if (entity_writer (entity_type (&ep->entity)))
		ep_list = &tp->writers;
	else
		ep_list = &tp->readers;
	for (prev = NULL, xep = *ep_list;
	     xep && xep != ep;
	     prev = xep, xep = xep->next)
		;

	if (xep) {
		if (prev)
			prev->next = ep->next;
		else
			*ep_list = ep->next;
	}

	/* Free the QoS parameters. */
	qos_free (ep->qos);

	/* Free the endpoint. */
	endpoint_delete (rpp, ep);
}

/* connect_builtin_writer -- Connect a builtin writer. */

static void connect_builtin_writer (Writer_t *wp, DiscoveredReader_t *peer_rp)
{
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t				*dp;
	DataReaderCrypto_t			crypto;
	DDS_ParticipantVolatileSecureMessage	msg;
	DDS_ReturnCode_t			ret;

	dp = wp->w_publisher->domain;
#endif
#ifndef RW_TOPIC_LOCK
	lock_take (wp->w_lock);
#endif
	rtps_matched_reader_add (wp, peer_rp);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp) && (wp->w_submsg_prot || wp->w_payload_prot)) {
		crypto = sec_register_remote_datareader (
					wp->w_crypto,
					peer_rp->dr_participant->p_crypto,
					peer_rp,
					0,
					&ret);
		if (!crypto) {
			warn_printf ("connect_builtin_writer: can't create crypto material!");
			return;
		}
		rtps_peer_reader_crypto_set (wp, peer_rp, crypto);
		memset (&msg, 0, sizeof (msg));
		ret = sec_create_local_datawriter_tokens (wp->w_crypto,
							  crypto,
							  &msg.message_data);
		if (ret) {
			warn_printf ("connect_builtin_writer: cant't create crypto tokens!");
			return;
		}
		msg.message_class_id = GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS;
		ctt_send (dp, peer_rp->dr_participant, &wp->w_ep, &peer_rp->dr_ep, &msg);
		sec_release_tokens (&msg.message_data);
	}
#endif
#ifndef RW_TOPIC_LOCK
	lock_release (wp->w_lock);
#endif
}

/* connect_builtin_reader -- Connect a builtin reader. */

static void connect_builtin_reader (Reader_t *rp, DiscoveredWriter_t *peer_wp)
{
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t				*dp;
	DataWriterCrypto_t			crypto;
	DDS_ParticipantVolatileSecureMessage	msg;
	DDS_ReturnCode_t			ret;

	dp = rp->r_subscriber->domain;
#endif
#ifndef RW_TOPIC_LOCK
	lock_take (rp->r_lock);
#endif
	rtps_matched_writer_add (rp, peer_wp);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp) && (rp->r_submsg_prot || rp->r_payload_prot)) {
		crypto = sec_register_remote_datawriter (
					rp->r_crypto,
					peer_wp->dw_participant->p_crypto,
					peer_wp,
					&ret);
		if (!crypto) {
			warn_printf ("disc_new_matched_writer: can't create crypto material!");
			return;
		}
		rtps_peer_writer_crypto_set (rp, peer_wp, crypto);
		memset (&msg, 0, sizeof (msg));
		ret = sec_create_local_datareader_tokens (rp->r_crypto,
							  crypto,
							  &msg.message_data);
		if (ret) {
			warn_printf ("disc_new_matched_writer: cant't create crypto tokens!");
			return;
		}
		msg.message_class_id = GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS;
		ctt_send (dp, peer_wp->dw_participant, &rp->r_ep, &peer_wp->dw_ep, &msg);
		sec_release_tokens (&msg.message_data);
	}
#endif
#ifndef RW_TOPIC_LOCK
	lock_release (rp->r_lock);
#endif
}

/* connect_builtin -- Connect a local builtin endpoint to a remote.
		      On entry/exit: DP locked. */

void connect_builtin (Domain_t      *dp,
		      BUILTIN_INDEX l_index,
		      Participant_t *rpp,
		      BUILTIN_INDEX r_index)
{
	LocalEndpoint_t	*ep;
	Endpoint_t	*rep;

	ep = (LocalEndpoint_t *) dp->participant.p_builtin_ep [l_index];
	lock_take (ep->ep.topic->lock);
	rep = rpp->p_builtin_ep [r_index];
	if (!rep) {
		rep = add_peer_builtin (rpp, r_index, ep);
		if (!rep)
			goto done;
	}
	else if (rep->rtps)
		goto done;	/* Already connected! */
	
	if (entity_writer (entity_type (&ep->ep.entity)))
		connect_builtin_writer ((Writer_t *) ep, (DiscoveredReader_t *) rep);
	else
		connect_builtin_reader ((Reader_t *) ep, (DiscoveredWriter_t *) rep);

    done:
	lock_release (ep->ep.topic->lock);
}

/* disconnect_builtin -- Disconnect a local builtin from a remote.
			 On entry/exit: DP locked. */

void disconnect_builtin (Domain_t      *dp,
			 BUILTIN_INDEX l_index,
			 Participant_t *rpp,
			 BUILTIN_INDEX r_index)
{
	LocalEndpoint_t	*ep;
	Endpoint_t	*rep;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	unsigned	crypto;
#endif

	ep = (LocalEndpoint_t *) dp->participant.p_builtin_ep [l_index];
	if (ep && (rep = rpp->p_builtin_ep [r_index]) != NULL) {
		lock_take (ep->ep.topic->lock);
		if (entity_writer (entity_type (&ep->ep.entity))) {
#ifndef RW_TOPIC_LOCK
			if (lock_take (((Writer_t *) ep)->w_lock)) {
				lock_release (ep->ep.topic->lock);
				return;
			}
#endif

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
			crypto = rtps_peer_reader_crypto_get ((Writer_t *) ep,
						      (DiscoveredReader_t *) rep);
			if (crypto)
				sec_unregister_datareader (crypto);
#endif
			rtps_matched_reader_remove ((Writer_t *) ep,
						    (DiscoveredReader_t *) rep);
#ifndef RW_TOPIC_LOCK
			lock_release (((Writer_t *) ep)->w_lock);
#endif
		}
		else {
#ifndef RW_TOPIC_LOCK
			if (lock_take (((Reader_t *) ep)->r_lock)) {
				lock_release (ep->ep.topic->lock);
				return;
			}
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
			crypto = rtps_peer_writer_crypto_get ((Reader_t *) ep,
						      (DiscoveredWriter_t *) rep);
			if (crypto)
				sec_unregister_datawriter (crypto);
#endif
			rtps_matched_writer_remove ((Reader_t *) ep,
						    (DiscoveredWriter_t *) rep);
			hc_rem_writer_removed (ep->cache, rep->entity.handle);
#ifndef RW_TOPIC_LOCK
			lock_release (((Reader_t *) ep)->r_lock);
#endif
		}
		remove_peer_builtin (rpp, r_index);
		lock_release (ep->ep.topic->lock);
	}
}


