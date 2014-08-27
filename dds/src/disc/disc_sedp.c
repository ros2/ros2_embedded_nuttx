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

/* disc_sedp.c -- Implements the SEDP Discovery protocol. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "log.h"
#include "error.h"
#include "dds.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_access.h"
#endif
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_ep.h"
#include "disc_match.h"
#include "disc_sub.h"
#include "disc_pub.h"
#include "disc_sedp.h"

#ifdef SIMPLE_DISCOVERY

int		sedp_log;		/* SEDP event logging. */

#define	CDD_WR_MATCH(wpp,rpp,wqp,rqp) (qos_same_partition (wpp,rpp) && \
				       qos_match (wqp,NULL,rqp,NULL,NULL))

#ifdef TOPIC_DISCOVERY

/* sedp_topic_event -- Receive a topic change from a remote participant. */

void sedp_topic_event (Reader_t *rp, NotificationType_t t)
{
	Domain_t		*dp = rp->r_subscriber->domain;
	Participant_t		*pp;
	ChangeData_t		change;
	DiscoveredTopicData	*info = NULL, tinfo;
	Topic_t			*tp;
	InfoType_t		type;
	int			error, new_node, valid_data = 0;
	const char		*names [2];

	if (t != NT_DATA_AVAILABLE)
		return;

	rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
	for (;;) {
		if (info) {
			pid_topic_data_cleanup (&info);
			xfree (info);
			info = NULL;
		}
		/*dtrc_print0 ("SEDP-Topic: get samples ");*/
		error = disc_get_data (rp, &change);
		if (error) {
			/*dtrc_print0 ("- none\r\n");*/
			break;
		}
		/*dtrc_print1 ("- valid(%u)\r\n", change.kind);*/
		if (change.kind != ALIVE) {
			/* error = hc_get_key (rp->r_cache, change.h, &tinfo, 0);
			if (error)
				continue; */

			type = EI_DELETE;
			hc_inst_free (rp->r_cache, change.h);
		}
		else {
			type = EI_NEW;
			info = change.data;
		}
		pp = entity_participant (change.writer);
		if (!pp ||				/* Not found. */
		    pp == &dp->participant ||		/* Own sent info. */
		    entity_ignored (pp->p_flags)) {	/* Ignored. */
			hc_inst_free (rp->r_cache, change.h);
			continue;	/* Filter out unneeded info. */
		}

		/* Topic from remote participant. */
		if (type == EI_DELETE) {
			/* TBD: Certified not to work correctly. */
			tp = topic_lookup (pp, str_ptr (info.name));
			if (!tp) {
				hc_inst_free (rp->r_cache, change.h);
				continue; /* If doesn't exist - no action. */
			}
			names [0] = str_ptr (tp->name);
			names [1] = str_ptr (tp->type->type_name);
		}
		else {
			tp = topic_create (pp, NULL, names [0], names [1], &new_node);
			if (!tp) {
				hc_inst_free (rp->r_cache, change.h);
				continue; /* Can't create info -- just ignore. */
			}
			if (!new_node) {
				if (entity_ignored (tp->entity.flags)) {
					hc_inst_free (rp->r_cache, change.h);
					continue;
				}
				type = EI_UPDATE;
			}
			names [0] = str_ptr (info->name);
			names [1] = str_ptr (info->type_name);
		}
		if (sedp_log)
			log_printf (SEDP_ID, 0, "SEDP: %s topic (%s/%s) from peer!\r\n",
					    info_type_str [type], names [0], names [1]);
		sedp_topic_info (pp, tp, info, type);
	}
	if (info) {
		pid_topic_data_cleanup (info);
		xfree (info);
	}
}

#endif

/* sedp_subscription_event -- Receive a subscription event.
			      On entry/exit: DP, R(rp) locked. */

void sedp_subscription_event (Reader_t           *rp,
			      NotificationType_t t,
			      int                cdd,
			      int                secure)
{
	Domain_t		*dp = rp->r_subscriber->domain;
	Participant_t		*pp;
	ChangeData_t		change;
	DiscoveredReaderData	*info = NULL, tinfo;
	Topic_t			*tp;
	DiscoveredReader_t	*drp;
	Writer_t		*mwp;
	UniQos_t		qos;
	InfoType_t		type;
	GUID_t			*guidp;
	int			error;

	if (t != NT_DATA_AVAILABLE)
		return;


	rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
	for (;;) {
		if (info) {
			pid_reader_data_cleanup (info);
			xfree (info);
			info = NULL;
		}
		/*dtrc_print0 ("SEDP-Sub: get samples ");*/
		error = disc_get_data (rp, &change);
		if (error) {
			/*dtrc_print0 ("- none\r\n");*/
			break;
		}
		/*dtrc_print1 ("- valid(%u)\r\n", kind);*/
		if (change.kind != ALIVE) {
			error = hc_get_key (rp->r_cache, change.h, &tinfo, 0);
			if (error)
				continue;

			guidp = &tinfo.proxy.guid;       
			type = EI_DELETE;
			hc_inst_free (rp->r_cache, change.h);
		}
		else {
			info = change.data;
			if (!info->topic_name || !info->type_name) {
				hc_inst_free (rp->r_cache, change.h);
				continue;
			}
			type = EI_NEW;
			guidp = &info->proxy.guid;
		}
			pp = entity_participant (change.writer);
		if (!pp ||				/* Not found. */
		    pp == &dp->participant ||		/* Own sent info. */
		    entity_ignored (pp->p_flags) ||
		    entity_shutting_down (pp->p_flags)) {		/* Ignored. */
			hc_inst_free (rp->r_cache, change.h);
			dtrc_print0 ("SEDP-Sub: unneeded!\r\n");
			continue;	/* Filter out unneeded info. */
		}

		/* Subscription from remote participant. */
		if (type == EI_DELETE) {
			drp = (DiscoveredReader_t *) endpoint_lookup (pp,
						    &guidp->entity_id);
			if (!drp) {
				dtrc_print0 ("SEDP-Sub: DELETE && doesn't exist!\r\n");
				continue; /* If doesn't exist - no action. */
			}
			if (!drp->dr_topic) {
				endpoint_delete (pp, &drp->dr_ep);
				continue; /* Ignored topic -- only endpoint. */
			}
			if (sedp_log)
				log_printf (SEDP_ID, 0, "SEDP: Deleted %ssubscription (%s/%s) from peer!\r\n",
					(secure) ? "secure " : "",
					str_ptr (drp->dr_topic->name),
					str_ptr (drp->dr_topic->type->type_name));
			disc_subscription_remove (pp, drp);
			hc_inst_free (rp->r_cache, change.h);
			continue;
		}

		/* Do we know this topic? */
		tp = topic_lookup (&dp->participant, str_ptr (info->topic_name));
		if (tp && entity_ignored (tp->entity.flags)) {
			hc_inst_free (rp->r_cache, change.h);
			dtrc_print1 ("SEDP: ignored topic (%s)!\r\n", str_ptr (info->topic_name));
			continue;	/* Ignored topic. */
		}

		/* Do we know this endpoint already? */
		drp = (DiscoveredReader_t *) endpoint_lookup (pp, &guidp->entity_id);
		if (drp) {
			if (entity_ignored (drp->dr_flags) || cdd) {
				hc_inst_free (rp->r_cache, change.h);
				continue; /* Ignored endpoint. */
			}
			dtrc_print1 ("Already exists (%s)!\r\n", str_ptr (info->topic_name));
			type = EI_UPDATE;
			disc_subscription_update (pp, drp, info);
			if (sedp_log)
				log_printf (SEDP_ID, 0, "SEDP: Updated %ssubscription (%s/%s) from peer!\r\n",
						(secure) ? "secure " : "",
						str_ptr (info->topic_name),
						str_ptr (info->type_name));
			hc_inst_free (rp->r_cache, change.h);
		}
		else {
			/* Get QoS parameters. */
			qos_disc_reader_set (&qos, &info->qos);
			mwp = NULL;
			/* Create new endpoint. */
			drp = (DiscoveredReader_t *) endpoint_create (pp,
					pp, &guidp->entity_id, NULL);
			if (!drp) {
				dtrc_print1 ("SEDP: Create endpoint (%s) not possible - exit!\r\n", str_ptr (info->topic_name));
				hc_inst_free (rp->r_cache, change.h);
				qos_disc_reader_restore (&info->qos, &qos);
				continue;  /* Can't create -- ignore. */
			}
			disc_subscription_add (pp, drp, &qos, tp, mwp, info);
			hc_inst_free (rp->r_cache, change.h);

			if (sedp_log)
				log_printf (SEDP_ID, 0, "SEDP: New %ssubscription (%s/%s) from %s!\r\n",
						(secure) ? "secure " : "",
						str_ptr (info->topic_name),
						str_ptr (info->type_name),
						(cdd) ? "CDD" : "peer");

		}
	}
	if (info) {
		pid_reader_data_cleanup (info);
		xfree (info);
	}
}


/* sedp_publication_event -- Receive a publication event.
			     On entry/exit: DP, R(rp) locked. */

void sedp_publication_event (Reader_t *rp,
			     NotificationType_t t,
			     int cdd,
			     int secure)
{
	Domain_t		*dp = rp->r_subscriber->domain;
	Participant_t		*pp;
	ChangeData_t		change;
	DiscoveredWriterData	*info = NULL, tinfo;
	Topic_t			*tp;
	DiscoveredWriter_t	*dwp;
	Reader_t		*mrp;
	UniQos_t		qos;
	InfoType_t		type;
	GUID_t			*guidp;
	int			error;

	if (t != NT_DATA_AVAILABLE)
		return;

	rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
	for (;;) {
		if (info) {
			pid_writer_data_cleanup (info);
			xfree (info);
			info = NULL;
		}
		/*dtrc_print0 ("SEDP-Pub: get samples ");*/
		error = disc_get_data (rp, &change);
		if (error) {
			/*dtrc_print0 ("- none\r\n");*/
			break;
		}
		/*dtrc_print1 ("- valid(%u)\r\n", change.kind);*/
		if (change.kind != ALIVE) {
			error = hc_get_key (rp->r_cache, change.h, &tinfo, 0);
			if (error)
				continue;

			guidp = &tinfo.proxy.guid;
			type = EI_DELETE;
			hc_inst_free (rp->r_cache, change.h);
		}
		else {
			info = change.data;
			if (!info->topic_name || !info->type_name) {
				hc_inst_free (rp->r_cache, change.h);
				continue;
			}
			type = EI_NEW;
			guidp = &info->proxy.guid;
		}
			pp = entity_participant (change.writer);
		if (!pp ||				/* Not found. */
		    pp == &dp->participant ||		/* Own sent info. */
		    entity_ignored (pp->p_flags)) {	/* Ignored. */
			if (pp != &dp->participant && !cdd)
				warn_printf ("sedp_publication_rx: invalid change.writer field!\r\n");

			hc_inst_free (rp->r_cache, change.h);
			dtrc_print0 ("SEDP-Pub: unneeded!\r\n");
			continue;	/* Filter out unneeded info. */
		}

		/* Publication from remote participant. */
		if (type == EI_DELETE) {
			dwp = (DiscoveredWriter_t *) endpoint_lookup (pp,
							&guidp->entity_id);
			if (!dwp) {
				dtrc_print0 ("SEDP-Pub: DELETE && doesn't exist!\r\n");
				continue; /* If doesn't exist - no action. */
			}
			if (!dwp->dw_topic) {
				endpoint_delete (pp, &dwp->dw_ep);
				continue; /* Ignored topic -- only endpoint. */
			}
			if (sedp_log)
				log_printf (SEDP_ID, 0, "SEDP: Deleted %spublication (%s/%s) from peer!\r\n",
						(secure) ? "secure " : "",
						str_ptr (dwp->dw_topic->name),
						str_ptr (dwp->dw_topic->type->type_name));
			disc_publication_remove (pp, dwp);

			hc_inst_free (rp->r_cache, change.h);
			continue;
		}

		/* Do we know this topic? */
		tp = topic_lookup (&dp->participant, str_ptr (info->topic_name));
		if (tp && entity_ignored (tp->entity.flags)) {
			hc_inst_free (rp->r_cache, change.h);
			dtrc_print1 ("SEDP: ignored topic (%s)!\r\n", str_ptr (info->topic_name));
			continue;	/* Ignored topic. */
		}

		/* Do we know this endpoint already? */
		dwp = (DiscoveredWriter_t *) endpoint_lookup (pp, &guidp->entity_id);
		if (dwp) {
			if (entity_ignored (dwp->dw_flags) || cdd) {
				hc_inst_free (rp->r_cache, change.h);
				continue; /* Ignored endpoint. */
			}
			dtrc_print1 ("Already exists (%s)!\r\n", str_ptr (info->topic_name));
			type = EI_UPDATE;
			disc_publication_update (pp, dwp, info);
			if (sedp_log)
				log_printf (SEDP_ID, 0, "SEDP: Updated %spublication (%s/%s) from peer!\r\n",
						(secure) ? "secure " : "",
						str_ptr (info->topic_name),
						str_ptr (info->type_name));
			hc_inst_free (rp->r_cache, change.h);
		}
		else {
			/* Get QoS parameters. */
			qos_disc_writer_set (&qos, &info->qos);
			mrp = NULL;
			/* Create new endpoint. */
			dwp = (DiscoveredWriter_t *) endpoint_create (pp,
					pp, &guidp->entity_id, NULL);
			if (!dwp) {
				dtrc_print1 ("SEDP: Create endpoint (%s) not possible - exit!\r\n", str_ptr (info->topic_name));
				hc_inst_free (rp->r_cache, change.h);
				qos_disc_writer_restore (&info->qos, &qos);
				continue;  /* Can't create -- just ignore. */
			}
			disc_publication_add (pp, dwp, &qos, tp, mrp, info);
			hc_inst_free (rp->r_cache, change.h);
			if (sedp_log)
				log_printf (SEDP_ID, 0, "SEDP: New %spublication (%s/%s) from %s!\r\n",
						(secure) ? "secure " : "",
						str_ptr (info->topic_name),
						str_ptr (info->type_name),
						(cdd) ? "CDD" : "peer");

		}
	}
	if (info) {
		pid_writer_data_cleanup (info);
		xfree (info);
	}
}

/* sedp_publication_add -- Add a publication to the Publication Writer.
			   On entry/exit: DP,P(wp),W(wp) locked. */

static int sedp_publication_add (Domain_t *dp, Writer_t *wp)
{
	GUID_t		guid;
	Writer_t	*pw;
	InstanceHandle	handle;
	HCI		hci;
	DDS_HANDLE	endpoint;
	int		error;

	/* Derive key and publication endpoint. */
	guid.prefix = dp->participant.p_guid_prefix;
	guid.entity_id = wp->w_entity_id;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (SECURE_DISCOVERY (dp, wp->w_disc_prot))
		pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_SEC_W];
	else
#endif
		pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_W];
	if (!pw)
		return (DDS_RETCODE_ALREADY_DELETED);

	/* Register instance. */
	lock_take (pw->w_lock);
	hci = hc_register (pw->w_cache,
			   (unsigned char *) &guid,
			   sizeof (guid),
			   NULL,
			   &handle);
	if (!hci) {
		warn_printf ("sedp_publication_add: failed to register instance handle!");
		lock_release (pw->w_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Write publication data. */
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Send %spublication (%s/%s)\r\n",
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
				(SECURE_DISCOVERY (dp, wp->w_disc_prot)) ? "secure " : 
#endif
								           "",
				str_ptr (wp->w_topic->name), 
				str_ptr (wp->w_topic->type->type_name));
	endpoint = wp->w_handle;
	error = rtps_writer_write (pw, &endpoint, sizeof (endpoint), handle,
							hci, NULL, NULL, 0);
	lock_release (pw->w_lock);
	if (error)
		warn_printf ("sedp_publication_add: write failure!");
	return (error);
}

/* sedp_publication_update -- Update a publication to the Publication Writer.
			      On entry/exit: DP,P(wp),W(wp) locked. */

static int sedp_publication_update (Domain_t *dp, Writer_t *wp)
{
	GUID_t		guid;
	Writer_t	*pw;
	HCI		hci;
	InstanceHandle	handle;
	DDS_HANDLE	endpoint;
	FTime_t		time;
	int		error;

	/* Derive key and publication endpoint. */
	guid.prefix = dp->participant.p_guid_prefix;
	guid.entity_id = wp->w_entity_id;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (SECURE_DISCOVERY (dp, wp->w_disc_prot))
		pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_SEC_W];
	else
#endif
		pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_W];
	if (!pw)
		return (DDS_RETCODE_ALREADY_DELETED);

	/* Lookup instance. */
	lock_take (pw->w_lock);
	hci = hc_lookup_key (pw->w_cache, (unsigned char *) &guid,
							sizeof (guid), &handle);
	if (!hci) {
		warn_printf ("sedp_publication_update: failed to lookup instance handle!");
		lock_release (pw->w_lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Write publication data. */
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Resend %spublication (%s/%s)\r\n",
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
				(SECURE_DISCOVERY (dp, wp->w_disc_prot)) ? "secure " : 
#endif
									   "",
				str_ptr (wp->w_topic->name), 
				str_ptr (wp->w_topic->type->type_name));
	endpoint = wp->w_handle;
	sys_getftime (&time);
	error = rtps_writer_write (pw, &endpoint, sizeof (endpoint), handle,
							hci, &time, NULL, 0);
	lock_release (pw->w_lock);
	if (error)
		warn_printf ("sedp_publication_update: write failure!");

	return (error);
}

/* sedp_publication_remove -- Remove a publication from the Publication Writer.
			      On entry/exit: DP,P(wp),W(wp) locked. */

static int sedp_publication_remove (Domain_t *dp, Writer_t *wp)
{
	GUID_t		guid;
	Writer_t	*pw;
	HCI		hci;
	InstanceHandle	handle;
	FTime_t		time;
	int		error;

	/* Derive key and publication endpoint. */
	guid.prefix = dp->participant.p_guid_prefix;
	guid.entity_id = wp->w_entity_id;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (SECURE_DISCOVERY (dp, wp->w_disc_prot))
		pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_SEC_W];
	else
#endif
		pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_W];
	if (!pw)
		return (DDS_RETCODE_ALREADY_DELETED);

	/* Lookup instance. */
	lock_take (pw->w_lock);
	hci = hc_lookup_key (pw->w_cache, (unsigned char *) &guid,
							sizeof (guid), &handle);
	if (!hci) {
		/* Don't warn: perfectly ok if suspended(). */
		/*warn_printf ("sedp_publication_remove: failed to lookup instance handle!");*/
		lock_release (pw->w_lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Unregister instance. */
	sys_getftime (&time);
	error = rtps_writer_unregister (pw, handle, hci, &time, NULL, 0);
	lock_release (pw->w_lock);
	if (error) {
		warn_printf ("sedp_publication_remove: failed to unregister instance handle!");
		return (error);
	}
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: %spublication (%s/%s) removed.\r\n",
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
				(SECURE_DISCOVERY (dp, wp->w_disc_prot)) ? "Secure " : 
#endif
									   "",
				str_ptr (wp->w_topic->name), 
				str_ptr (wp->w_topic->type->type_name));
	return (error);
}

/* sedp_writer_add -- Add a local writer.
		      On entry/exit: all locks taken (DP,P,T,W). */

int sedp_writer_add (Domain_t *dp, Writer_t *wp)
{
	Endpoint_t	*ep;
	int		ret;
	DDS_QOS_POLICY_ID qid;

	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Writer (%s/%s) added.\r\n",
				str_ptr (wp->w_topic->name),
				str_ptr (wp->w_topic->type->type_name));

	/* Add publication. */
	if ((ret = sedp_publication_add (dp, wp)) != DDS_RETCODE_OK)
		return (ret);

	/* Can we match discovered readers with the writer? */
	for (ep = wp->w_topic->readers; ep; ep = ep->next) {
		if (!remote_active (ep->entity.flags))
			continue;

		if (!qos_same_partition (wp->w_publisher->qos.partition,
					 ep->qos->qos.partition))
			dcps_offered_incompatible_qos (wp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qos_ptr (wp->w_qos), &wp->w_publisher->qos,
	    				      qos_ptr (ep->qos), NULL, &qid))
			dcps_offered_incompatible_qos (wp, qid);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		else if (ACCESS_CONTROL (dp) &&
			 sec_check_local_writer_match (dp->participant.p_permissions,
			    			       ep->u.participant->p_permissions,
						       wp,
						       ep))
			continue;
#endif
		else
			disc_new_matched_reader (wp, (DiscoveredReader_t *) ep);
	}
	return (DDS_RETCODE_OK);
}

/* sedp_writer_update -- Update a local writer.
		         On entry/exit: all locks taken (DP,P,T,W). */

int sedp_writer_update (Domain_t             *domain,
			Writer_t             *wp,
			int                  changed,
			DDS_InstanceHandle_t peer)
{
	Endpoint_t	*ep;
	int		ret;
	DDS_QOS_POLICY_ID qid;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t	*dp;

	dp = wp->w_publisher->domain;
#endif
	if (sedp_log && changed)
		log_printf (SEDP_ID, 0, "SEDP: Writer (%s/%s) updated.\r\n",
				str_ptr (wp->w_topic->name),
				str_ptr (wp->w_topic->type->type_name));

	/* Update publication. */
	if (changed && 
	    (ret = sedp_publication_update (domain, wp)) != DDS_RETCODE_OK)
		return (ret);

	/* Can we match discovered readers with the writer? */
	for (ep = wp->w_topic->readers; ep; ep = ep->next) {
		int old_match;
		int new_match;

		if (peer && ep->u.participant->p_handle != peer)
			continue;

		if (!remote_active (ep->entity.flags))
			continue;

		old_match = rtps_writer_matches (wp, (DiscoveredReader_t *) ep);
		new_match = 0;
		if (!qos_same_partition (wp->w_publisher->qos.partition,
					 ep->qos->qos.partition))
			dcps_offered_incompatible_qos (wp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qos_ptr (wp->w_qos), &wp->w_publisher->qos,
				     qos_ptr (ep->qos), NULL, &qid))
			dcps_offered_incompatible_qos (wp, qid);
		else 
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		     if (!ACCESS_CONTROL (dp) ||
			 !sec_check_local_writer_match (dp->participant.p_permissions,
			    			        ep->u.participant->p_permissions,
						        wp,
						        ep))
#endif
			new_match = 1;
		if (old_match && !new_match) 
			disc_end_matched_reader (wp, (DiscoveredReader_t *) ep);
		else if (!old_match && new_match)
			disc_new_matched_reader (wp, (DiscoveredReader_t *) ep);
	}
	return (DDS_RETCODE_OK);
}

/* sedp_writer_remove -- Remove a local writer.
		         On entry/exit: all locks taken (DP,P,T,W). */

int sedp_writer_remove (Domain_t *dp, Writer_t *wp)
{
	Endpoint_t	*ep;

	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Writer (%s/%s) removed.\r\n",
				str_ptr (wp->w_topic->name),
				str_ptr (wp->w_topic->type->type_name));

	/* Remove publication. */
	sedp_publication_remove (dp, wp);

	/* Do we have to unmatch discovered readers from the writer? */
	for (ep = wp->w_topic->readers; ep; ep = ep->next)
		if (remote_active (ep->entity.flags) &&
		    rtps_writer_matches (wp, (DiscoveredReader_t *) ep))
			disc_end_matched_reader (wp, (DiscoveredReader_t *) ep);

	return (DDS_RETCODE_OK);
}

/* sedp_subscription_add -- Add a subscription to the Subscription Writer.
			    On entry/exit: DP,S(rp),R(rp) locked. */

static int sedp_subscription_add (Domain_t *dp, Reader_t *rp)
{
	GUID_t		guid;
	Writer_t	*sw;
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	HCI		hci;
	InstanceHandle	handle;
	DDS_HANDLE	endpoint;
	int		error;

	/* Derive key and Subscription endpoint. */
	guid.prefix = dp->participant.p_guid_prefix;
	guid.entity_id = rp->r_entity_id;

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (SECURE_DISCOVERY (dp, rp->r_disc_prot))
		sw = (Writer_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_SEC_W];
	else
#endif
		sw = (Writer_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_W];
	if (!sw)
		return (DDS_RETCODE_ALREADY_DELETED);

	/* Register instance. */
	lock_take (sw->w_lock);
	hci = hc_register (sw->w_cache, (unsigned char *) &guid,
						sizeof (guid), NULL, &handle);
	if (!hci) {
		warn_printf ("sedp_subscription_add: failed to register instance handle!");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Write subscription data. */
	tp = rp->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
	}
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Send %ssubscription (%s/%s)\r\n",
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
				(SECURE_DISCOVERY (dp, rp->r_disc_prot)) ? "secure " : 
#endif
									   "",
				str_ptr (tp->name), 
				str_ptr (tp->type->type_name));
	endpoint = rp->r_handle;
	error = rtps_writer_write (sw, &endpoint, sizeof (endpoint), handle,
							hci, NULL, NULL, 0);
	lock_release (sw->w_lock);
	if (error)
		warn_printf ("sedp_subscription_add: write failure!");
	return (error);
}

/* sedp_subscription_update -- Update a subscription to the Subscription Writer.
			       On entry/exit: DP,S(rp),R(rp) locked. */

static int sedp_subscription_update (Domain_t *dp, Reader_t *rp)
{
	GUID_t		guid;
	Writer_t	*sw;
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	HCI		hci;
	InstanceHandle	handle;
	DDS_HANDLE	endpoint;
	FTime_t		time;
	int		error;

	/* Derive key and publication endpoint. */
	guid.prefix = dp->participant.p_guid_prefix;
	guid.entity_id = rp->r_entity_id;

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (SECURE_DISCOVERY (dp, rp->r_disc_prot))
		sw = (Writer_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_SEC_W];
	else
#endif
		sw = (Writer_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_W];
	if (!sw)
		return (DDS_RETCODE_ALREADY_DELETED);

	/* Lookup instance. */
	lock_take (sw->w_lock);
	hci = hc_lookup_key (sw->w_cache, (unsigned char *) &guid,
							sizeof (guid), &handle);
	if (!hci) {
		warn_printf ("sedp_subscription_update: failed to lookup instance handle!");
		lock_release (sw->w_lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Write subscription data. */
	tp = rp->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
	}
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Resend %ssubscription (%s/%s)\r\n",
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
				(SECURE_DISCOVERY (dp, rp->r_disc_prot)) ? "secure " : 
#endif
									   "",
				str_ptr (tp->name), 
				str_ptr (tp->type->type_name));
	endpoint = rp->r_handle;
	sys_getftime (&time);
	error = rtps_writer_write (sw, &endpoint, sizeof (endpoint), handle,
							hci, &time, NULL, 0);
	lock_release (sw->w_lock);
	if (error)
		warn_printf ("sedp_subscription_update: write failure!");

	return (DDS_RETCODE_OK);
}

/* sedp_subscription_remove -- Remove a subscription from the Subscription Writer.
			       On entry/exit: DP,S(rp),R(rp) locked. */

static int sedp_subscription_remove (Domain_t *dp, Reader_t *rp)
{
	GUID_t		guid;
	Writer_t	*sw;
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	HCI		hci;
	InstanceHandle	handle;
	FTime_t		time;
	int		error;

	/* Derive key and subscription endpoint. */
	guid.prefix = dp->participant.p_guid_prefix;
	guid.entity_id = rp->r_entity_id;

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (SECURE_DISCOVERY (dp, rp->r_disc_prot))
		sw = (Writer_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_SEC_W];
	else
#endif
		sw = (Writer_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_W];
	if (!sw)
		return (DDS_RETCODE_ALREADY_DELETED);

	/* Lookup instance. */
	lock_take (sw->w_lock);
	hci = hc_lookup_key (sw->w_cache, (unsigned char *) &guid,
							sizeof (guid), &handle);
	if (!hci) {
		warn_printf ("sedp_subscription_remove: failed to lookup instance handle!");
		lock_release (sw->w_lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}

	/* Unregister instance. */
	sys_getftime (&time);
	error = rtps_writer_unregister (sw, handle, hci, &time, NULL, 0);
	lock_release (sw->w_lock);
	if (error) {
		warn_printf ("sedp_subscription_remove: failed to unregister instance handle!");
		return (error);
	}
	tp = rp->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
	}
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: %ssubscription (%s/%s) removed.\r\n",
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
				(SECURE_DISCOVERY (dp, rp->r_disc_prot)) ? "secure " : 
#endif
									   "",
				str_ptr (tp->name), 
				str_ptr (tp->type->type_name));
	return (error);
}

/* sedp_reader_add -- Add a local reader.
		      On entry/exit: all locks taken (DP,S,T,R). */

int sedp_reader_add (Domain_t *dp, Reader_t *rp)
{
	Endpoint_t	*ep;
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	int		ret;
	DDS_QOS_POLICY_ID qid;

	tp = rp->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
	}
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Reader (%s/%s) added.\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));

	/* Add subscription. */
	if ((ret = sedp_subscription_add (dp, rp)) != DDS_RETCODE_OK)
		return (ret);

	/* Can we match/unmatch discovered writers with the reader? */
	for (ep = tp->writers; ep; ep = ep->next) {
		if (!remote_active (ep->entity.flags))
			continue;

		if (!qos_same_partition (rp->r_subscriber->qos.partition,
					 ep->qos->qos.partition))
			dcps_requested_incompatible_qos (rp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qos_ptr (ep->qos), NULL,
				     qos_ptr (rp->r_qos), &rp->r_subscriber->qos,
				     &qid))
			dcps_requested_incompatible_qos (rp, qid);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		else if (ACCESS_CONTROL (dp) &&
			 sec_check_local_reader_match (dp->participant.p_permissions,
			    			       ep->u.participant->p_permissions,
						       rp,
						       ep))
			continue;
#endif
		else
			disc_new_matched_writer (rp, (DiscoveredWriter_t *) ep);
	}
	return (DDS_RETCODE_OK);
}

/* sedp_reader_update -- Update a local reader.
		         On entry/exit: all locks taken (DP,S,T,R). */

int sedp_reader_update (Domain_t             *dp,
			Reader_t             *rp,
			int                  changed,
			DDS_InstanceHandle_t peer)
{
	Endpoint_t	*ep;
	Topic_t		*tp;
	FilteredTopic_t	*ftp;
	int		ret;
	DDS_QOS_POLICY_ID qid;

	tp = rp->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
	}
	if (changed && sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Reader (%s/%s) updated.\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));

	/* Update subscription. */
	if (changed && 
	    (ret = sedp_subscription_update (dp, rp)) != DDS_RETCODE_OK)
		return (ret);

	/* Can we match/unmatch discovered writers with the reader? */
	for (ep = tp->writers; ep; ep = ep->next) {
		int old_match;
		int new_match;

		if (peer && ep->u.participant->p_handle != peer)
			continue;

		if (!remote_active (ep->entity.flags))
			continue;

		old_match = rtps_reader_matches (rp, (DiscoveredWriter_t *)ep);
		new_match = 0;
		if (!qos_same_partition (rp->r_subscriber->qos.partition,
					 ep->qos->qos.partition))
			dcps_requested_incompatible_qos (rp, DDS_PARTITION_QOS_POLICY_ID);
		else if (!qos_match (qos_ptr (ep->qos), NULL,
				     qos_ptr (rp->r_qos), &rp->r_subscriber->qos,
				     &qid))
			dcps_requested_incompatible_qos (rp, qid);
		else
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
		     if (!ACCESS_CONTROL (dp) ||
			 !sec_check_local_reader_match (dp->participant.p_permissions,
			    			        ep->u.participant->p_permissions,
						        rp,
						        ep))
#endif
			new_match = 1;

		if (old_match && !new_match) 
			disc_end_matched_writer (rp, (DiscoveredWriter_t *) ep);
		else if (!old_match && new_match)
			disc_new_matched_writer (rp, (DiscoveredWriter_t *) ep);
	}
	return (DDS_RETCODE_OK);
}

/* sedp_reader_remove -- Remove a local reader.
		         On entry/exit: all locks taken (DP,S,T,R). */

int sedp_reader_remove (Domain_t *dp, Reader_t *rp)
{
	Endpoint_t	*ep;
	Topic_t		*tp;
	FilteredTopic_t	*ftp;

	tp = rp->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
	}
	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Reader (%s/%s) removed.\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));

	/* Remove the subscription. */
	sedp_subscription_remove (dp, rp);

	/* Can we match/unmatch discovered writers with the reader? */
	for (ep = tp->writers; ep; ep = ep->next)
		if (remote_active (ep->entity.flags) &&
		    rtps_reader_matches (rp, (DiscoveredWriter_t *) ep))
			disc_end_matched_writer (rp, (DiscoveredWriter_t *) ep);

	return (DDS_RETCODE_OK);
}

#ifdef TOPIC_DISCOVERY

/* sedp_topic_add -- Add a local topic. */

static int sedp_topic_add (Domain_t *dp, Topic_t *tp)
{
	ARG_NOT_USED (dp)
	ARG_NOT_USED (tp)

	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Topic (%s/%s) added.\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));

	/* ... TBC ... */

	return (DDS_ERR_UNIMPL);
}

/* sedp_topic_update -- Update a local topic. */

static int sedp_topic_update (Domain_t *dp, Topic_t *tp)
{
	ARG_NOT_USED (dp)
	ARG_NOT_USED (tp)

	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Topic (%s/%s) updated.\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));

	/* ... TBC ... */

	return (DDS_ERR_UNIMPL);
}

/* sedp_topic_remove -- Remove a local topic. */

static int sedp_topic_remove (Domain_t *dp, Topic_t *tp)
{
	ARG_NOT_USED (dp)
	ARG_NOT_USED (tp)

	if (sedp_log)
		log_printf (SEDP_ID, 0, "SEDP: Topic (%s/%s) removed.\r\n",
				str_ptr (tp->name),
				str_ptr (tp->type->type_name));

	/* ... TBC ... */

	return (DDS_ERR_UNIMPL);
}

#endif /* TOPIC_DISCOVERY */

/* sedp_endpoint_locator -- Add/remove a locator to/from an endpoint. */

int sedp_endpoint_locator (Domain_t        *domain,
			   LocalEndpoint_t *ep,
			   int             add,
			   int             mcast,
			   const Locator_t *loc)
{
	LocatorList_t	*lp;
	int		ret;

	lp = (mcast) ? &ep->ep.mcast : &ep->ep.ucast;
	if (add)
		locator_list_add (lp, loc->kind, loc->address, loc->port,
				  loc->scope_id, loc->scope, 0, 0);
	else
		locator_list_delete (lp, loc->kind, loc->address, loc->port);

	if ((ep->ep.entity.flags & EF_BUILTIN) != 0)
		return (DDS_RETCODE_OK);

	/* Notify peer participants of changed data. */
	if (rtps_used) {
		if (entity_type (&ep->ep.entity) == ET_WRITER)
			ret = sedp_publication_update (domain, (Writer_t *) ep);
		else
			ret = sedp_subscription_update (domain, (Reader_t *) ep);
	}
	else
		ret = DDS_RETCODE_OK;
	return (ret);
}

/* sedp_start -- Setup the SEDP protocol builtin endpoints and start the
		 protocol.  On entry/exit: no locks taken, */

int sedp_start (Domain_t *dp)
{
	Reader_t	*rp;
	int		error;

	/* Create builtin Publications Reader. */
	error = create_builtin_endpoint (dp, EPB_PUBLICATION_R,
					 0, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Attach to builtin Publications Reader. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("sedp_start: can't register SEDP Publications Reader!");
		return (error);
	}

	/* Create builtin Subscriptions Reader. */
	error = create_builtin_endpoint (dp, EPB_SUBSCRIPTION_R,
					 0, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Attach to builtin Subscriptions Reader. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("sedp_start: can't register SEDP Subscriptions Reader!");
		return (error);
	}

#ifdef TOPIC_DISCOVERY

	/* Create builtin Topic Reader. */
	error = create_builtin_endpoint (dp, EPB_TOPIC_R,
					 0, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Attach to builtin Topics Reader. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_TOPIC_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("sedp_start: can't register SEDP Topics Reader!");
		return (error);
	}

#endif

	/* Create builtin Publications Writer. */
	error = create_builtin_endpoint (dp, EPB_PUBLICATION_W,
					 1, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Create builtin Subscriptions Writer. */
	error = create_builtin_endpoint (dp, EPB_SUBSCRIPTION_W,
					 1, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

#ifdef TOPIC_DISCOVERY

	/* Create builtin Topics Writer. */
	error = create_builtin_endpoint (dp, EPB_TOPIC_W,
					 1, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

	if (NATIVE_SECURITY (dp)) {

		/* Create builtin secure Publications Reader. */
		error = create_builtin_endpoint (dp, EPB_PUBLICATION_SEC_R,
						 0, 1,
						 1, 0, 1,
						 NULL,
						 dp->participant.p_meta_ucast,
						 dp->participant.p_meta_mcast,
						 NULL);
		if (error)
			return (error);

		/* Attach to builtin secure Publications Reader. */
		rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_SEC_R];
		error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
		if (error) {
			fatal_printf ("sedp_start: can't register SEDP Secure Publications Reader!");
			return (error);
		}

		/* Create builtin secure Subscriptions Reader. */
		error = create_builtin_endpoint (dp, EPB_SUBSCRIPTION_SEC_R,
						 0, 1,
						 1, 0, 1,
						 NULL,
						 dp->participant.p_meta_ucast,
						 dp->participant.p_meta_mcast,
						 NULL);
		if (error)
			return (error);

		/* Attach to builtin secure Subscriptions Reader. */
		rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_SEC_R];
		error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
		if (error) {
			fatal_printf ("sedp_start: can't register SEDP Secure Subscriptions Reader!");
			return (error);
		}

		/* Create builtin secure Publications Writer. */
		error = create_builtin_endpoint (dp, EPB_PUBLICATION_SEC_W,
						 1, 1,
						 1, 0, 1,
						 NULL,
						 dp->participant.p_meta_ucast,
						 dp->participant.p_meta_mcast,
						 NULL);
		if (error)
			return (error);

		/* Create builtin secure Subscriptions Writer. */
		error = create_builtin_endpoint (dp, EPB_SUBSCRIPTION_SEC_W,
						 1, 1,
						 1, 0, 1,
						 NULL,
						 dp->participant.p_meta_ucast,
						 dp->participant.p_meta_mcast,
						 NULL);
		if (error)
			return (error);
	}
#endif
	return (DDS_RETCODE_OK);
}

/* sedp_disable -- Stop the SEDP protocol on the participant.
		   On entry/exit: domain and global lock taken */

void sedp_disable (Domain_t *dp)
{
	disable_builtin_endpoint (dp, EPB_PUBLICATION_R);
	disable_builtin_endpoint (dp, EPB_PUBLICATION_W);
	disable_builtin_endpoint (dp, EPB_SUBSCRIPTION_R);
	disable_builtin_endpoint (dp, EPB_SUBSCRIPTION_W);
#ifdef TOPIC_DISCOVERY
	disable_builtin_endpoint (dp, EPB_TOPIC_R);
	disable_builtin_endpoint (dp, EPB_TOPIC_W);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		disable_builtin_endpoint (dp, EPB_PUBLICATION_SEC_R);
		disable_builtin_endpoint (dp, EPB_PUBLICATION_SEC_W);
		disable_builtin_endpoint (dp, EPB_SUBSCRIPTION_SEC_R);
		disable_builtin_endpoint (dp, EPB_SUBSCRIPTION_SEC_W);
	}
#endif
}

/* sedp_stop -- Stop the SEDP protocol on the participant.
		On entry/exit: domain and global lock taken */

void sedp_stop (Domain_t *dp)
{
	delete_builtin_endpoint (dp, EPB_PUBLICATION_R);
	delete_builtin_endpoint (dp, EPB_PUBLICATION_W);
	delete_builtin_endpoint (dp, EPB_SUBSCRIPTION_R);
	delete_builtin_endpoint (dp, EPB_SUBSCRIPTION_W);
#ifdef TOPIC_DISCOVERY
	delete_builtin_endpoint (dp, EPB_TOPIC_R);
	delete_builtin_endpoint (dp, EPB_TOPIC_W);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		delete_builtin_endpoint (dp, EPB_PUBLICATION_SEC_R);
		delete_builtin_endpoint (dp, EPB_PUBLICATION_SEC_W);
		delete_builtin_endpoint (dp, EPB_SUBSCRIPTION_SEC_R);
		delete_builtin_endpoint (dp, EPB_SUBSCRIPTION_SEC_W);
	}
#endif
}

/* sedp_connect -- Connect this participant to a peer participant.
		   On entry/exit: DP locked. */

void sedp_connect (Domain_t *dp, Participant_t *rpp)
{
	if ((rpp->p_builtins & (1 << EPB_PUBLICATION_R)) != 0)
		connect_builtin (dp, EPB_PUBLICATION_W, rpp, EPB_PUBLICATION_R);
	if ((rpp->p_builtins & (1 << EPB_PUBLICATION_W)) != 0)
		connect_builtin (dp, EPB_PUBLICATION_R, rpp, EPB_PUBLICATION_W);
	if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_R)) != 0)
		connect_builtin (dp, EPB_SUBSCRIPTION_W, rpp, EPB_SUBSCRIPTION_R);
	if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_W)) != 0)
		connect_builtin (dp, EPB_SUBSCRIPTION_R, rpp, EPB_SUBSCRIPTION_W);
#ifdef TOPIC_DISCOVERY
	if ((rpp->p_builtins & (1 << EPB_TOPIC_R)) != 0)
		connect_builtin (dp, EPB_TOPIC_W, rpp, EPB_TOPIC_R);
	if ((rpp->p_builtins & (1 << EPB_TOPIC_W)) != 0)
		connect_builtin (dp, EPB_TOPIC_R, rpp, EPB_TOPIC_W);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		if ((rpp->p_builtins & (1 << EPB_PUBLICATION_SEC_R)) != 0)
			connect_builtin (dp, EPB_PUBLICATION_SEC_W, rpp, EPB_PUBLICATION_SEC_R);
		if ((rpp->p_builtins & (1 << EPB_PUBLICATION_SEC_W)) != 0)
			connect_builtin (dp, EPB_PUBLICATION_SEC_R, rpp, EPB_PUBLICATION_SEC_W);
		if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_SEC_R)) != 0)
			connect_builtin (dp, EPB_SUBSCRIPTION_SEC_W, rpp, EPB_SUBSCRIPTION_SEC_R);
		if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_SEC_W)) != 0)
			connect_builtin (dp, EPB_SUBSCRIPTION_SEC_R, rpp, EPB_SUBSCRIPTION_SEC_W);
	}
#endif
}

/* sedp_disconnect -- Disconnect this participant from a peer participant.
		      On entry/exit: DP locked. */

void sedp_disconnect (Domain_t *dp, Participant_t *rpp)
{
	if ((rpp->p_builtins & (1 << EPB_PUBLICATION_R)) != 0)
		disconnect_builtin (dp, EPB_PUBLICATION_W, rpp, EPB_PUBLICATION_R);
	if ((rpp->p_builtins & (1 << EPB_PUBLICATION_W)) != 0)
		disconnect_builtin (dp, EPB_PUBLICATION_R, rpp, EPB_PUBLICATION_W);
	if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_R)) != 0)
		disconnect_builtin (dp, EPB_SUBSCRIPTION_W, rpp, EPB_SUBSCRIPTION_R);
	if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_W)) != 0)
		disconnect_builtin (dp, EPB_SUBSCRIPTION_R, rpp, EPB_SUBSCRIPTION_W);
#ifdef TOPIC_DISCOVERY
	if ((rpp->p_builtins & (1 << EPB_TOPIC_R)) != 0)
		disconnect_builtin (dp, EPB_TOPIC_W, rpp, EPB_TOPIC_R);
	if ((rpp->p_builtins & (1 << EPB_TOPIC_W)) != 0)
		disconnect_builtin (dp, EPB_TOPIC_R, rpp, EPB_TOPIC_W);
#endif
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		if ((rpp->p_builtins & (1 << EPB_PUBLICATION_SEC_R)) != 0)
			disconnect_builtin (dp, EPB_PUBLICATION_SEC_W, rpp, EPB_PUBLICATION_SEC_R);
		if ((rpp->p_builtins & (1 << EPB_PUBLICATION_SEC_W)) != 0)
			disconnect_builtin (dp, EPB_PUBLICATION_SEC_R, rpp, EPB_PUBLICATION_SEC_W);
		if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_SEC_R)) != 0)
			disconnect_builtin (dp, EPB_SUBSCRIPTION_SEC_W, rpp, EPB_SUBSCRIPTION_SEC_R);
		if ((rpp->p_builtins & (1 << EPB_SUBSCRIPTION_SEC_W)) != 0)
			disconnect_builtin (dp, EPB_SUBSCRIPTION_SEC_R, rpp, EPB_SUBSCRIPTION_SEC_W);
	}
#endif
}

/* sedp_unmatch_peer_endpoint -- If the endpoint matches one of ours, end the
				 association since the peer participant has
				 gone. */

int sedp_unmatch_peer_endpoint (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	ep = *epp;
	/*log_printf (DISC_ID, 0, "sedp_unmatch_peer_endpoint (%s)!\r\n", str_ptr (ep->topic->name));*/
	if (entity_type (&ep->entity) == ET_WRITER) {
		disc_publication_remove (ep->u.participant, (DiscoveredWriter_t *) ep);
	}
	else {
		disc_subscription_remove (ep->u.participant, (DiscoveredReader_t *) ep);
	}
	return (1);
}

# if 0
/* sedp_topic_free -- Free a previously created topic. */

static int sedp_topic_free (Skiplist_t *list, void *node, void *arg)
{
	Topic_t		*tp, **tpp = (Topic_t **) node;
	Participant_t	*pp = (Participant_t *) arg;

	ARG_NOT_USED (list)

	tp = *tpp;
	lock_take (tp->lock);
	/*log_printf (DISC_ID, 0, "sedp_topic_free (%s)!\r\n", str_ptr (tp->name));*/
	if (pp->p_domain->builtin_readers [BT_Topic])
		user_topic_notify_delete (tp, tp->entity.handle);
	topic_delete (pp, tp, NULL, NULL);
	return (1);
}
# endif

#endif

