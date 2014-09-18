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

/* disc_spdp.c -- Implements the SPDP Participant Discovery protocol. */

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
#if defined (NUTTX_RTOS)
#include "dds/dds_plugin.h"
#else
#include "dds/dds_security.h"
#endif
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_id.h"
#include "sec_access.h"
#include "sec_crypto.h"
#endif
#endif
#include "guard.h"
#include "rtps_fwd.h"
#include "pl_cdr.h"
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_ep.h"
#include "disc_match.h"
#include "disc_sub.h"
#include "disc_pub.h"
#ifdef DDS_NATIVE_SECURITY
#include "disc_psmp.h"
#include "disc_ctt.h"
#include "disc_policy_updater.h"
#endif
#include "disc_msg.h"
#include "disc_sedp.h"
#include "disc_spdp.h"

#ifdef SIMPLE_DISCOVERY

int spdp_log;

#ifdef DDS_AUTO_LIVELINESS

/* spdp_auto_liveliness_timeout -- Time-out for resending automatic liveliness. */

static void spdp_auto_liveliness_timeout (uintptr_t user)
{
	Domain_t	*dp = (Domain_t *) user;

#if defined (RTPS_USED) && defined (SIMPLE_DISCOVERY)
	if (!dp->participant.p_liveliness)
		msg_send_liveliness (dp, 0);
#endif
	tmr_start_lock (&dp->auto_liveliness,
			dp->resend_per.secs * 2 * TICKS_PER_SEC,
			(uintptr_t) dp,
			spdp_auto_liveliness_timeout,
			&dp->lock);
}

#endif

/* spdp_init -- Initialize the SPDP protocol. */

int spdp_init (void)
{
	DDS_ReturnCode_t	ret;

	ret = msg_init ();
	if (ret)
		return (ret);

#ifdef DDS_NATIVE_SECURITY
#ifdef DDS_QEO_TYPES
	ret = policy_updater_init ();
	if (ret)
		return (ret);
#endif
	if (sec_register_types ()) {
		fatal_printf ("Can't register security types!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
#endif
	return (DDS_RETCODE_OK);
}

/* spdp_final -- Finalize the SPDP types. */

void spdp_final (void)
{
#ifdef DDS_NATIVE_SECURITY
#ifdef DDS_QEO_TYPES
	policy_updater_final ();
#endif
	sec_unregister_types ();
#endif
	msg_final ();
}

/* spdp_start -- Start the SPDP protocol.  On entry: Domain lock taken, */

int spdp_start (Domain_t *dp)
{
	int			error;
	InstanceHandle		handle;
	DDS_HANDLE		endpoint;
	FTime_t			time;
	Reader_t		*rp;
	Writer_t		*wp;
	HCI			hci;
	LocatorList_t		muc_locs;
	LocatorList_t		mmc_locs;
#ifdef DDS_NATIVE_SECURITY
	TopicType_t		*tp;
	DDS_ReturnCode_t	ret;
#endif

	if (spdp_log)
		log_printf (SPDP_ID, 0, "SPDP: starting protocol for domain #%u.\r\n", dp->domain_id);

#ifdef DDS_NATIVE_SECURITY
	if (NATIVE_SECURITY (dp)) {
		error = DDS_DomainParticipant_register_type ((DDS_DomainParticipant) dp,
							     ParticipantGenericMessage_ts,
							     "ParticipantGenericMessage");
		if (error) {
			warn_printf ("disc_start: can't register ParticipantGenericMessage type!");
			return (error);
		}
		error = DDS_DomainParticipant_register_type ((DDS_DomainParticipant) dp,
							     ParticipantStatelessMessage_ts,
							     "ParticipantStatelessMessage");
		if (error) {
			warn_printf ("disc_start: can't register ParticipantStatelessMessage type!");
			return (error);
		}
		error = DDS_DomainParticipant_register_type ((DDS_DomainParticipant) dp,
							     ParticipantVolatileSecureMessage_ts,
							     "ParticipantVolatileSecureMessage");
		if (error) {
			warn_printf ("disc_start: can't register ParticipantVolatileSecureMessage type!");
			return (error);
		}
		if (lock_take (dp->lock)) {
			warn_printf ("disc_start: domain lock error (3)");
			return (DDS_RETCODE_ERROR);
		}
		tp = type_lookup (dp, "ParticipantStatelessMessage");
		if (tp)
			tp->flags |= EF_BUILTIN;
		lock_release (dp->lock);

		dp->participant.p_crypto = sec_register_local_participant (dp, &ret);
		if (!dp->participant.p_crypto) {
			warn_printf ("disc_start: can't register local participant for crypto operations!");
			return (ret);
		}
	}
#endif

	/* Create SPDP builtin endpoints: Participant Announcer and Participant
	   Detector. */
	muc_locs = dp->participant.p_meta_ucast;
	mmc_locs = dp->participant.p_meta_mcast;
	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_W,
					 0, 0,
					 0, 0, 0,
					 &dp->resend_per,
					 muc_locs, mmc_locs,
					 dp->dst_locs);
	if (error)
		return (error);

	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_R,
					 0, 0,
					 0, 0, 0,
					 &dp->resend_per,
					 muc_locs, mmc_locs,
					 NULL);
	if (error)
		return (error);

	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("SPDP: can't register Participant detector!");
		return (error);
	}

	/* Create the builtin endpoints for SEDP. */
	sedp_start (dp);

	/* Create the builtin Participant Message endpoints. */
	msg_start (dp);

#ifdef DDS_NATIVE_SECURITY

	if (NATIVE_SECURITY (dp)) {

		/* Create the builtin InterParticipant Stateless endpoints. */
		psmp_start (dp);

		/* Create the Participant Volatile Message Secure endpoints. */
		ctt_start (dp);
#ifdef DDS_QEO_TYPES
		/* Create the policy updater endpoints */
		policy_updater_start (dp);
#endif
	}
#endif

	sys_getftime (&time);
	log_printf (SPDP_ID, 0, "SPDP: registering Participant key.\r\n");
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	lock_take (wp->w_lock);
	hci = hc_register (wp->w_cache, dp->participant.p_guid_prefix.prefix,
					sizeof (GuidPrefix_t), &time, &handle);

	log_printf (SPDP_ID, 0, "SPDP: Send Participant data.\r\n");

#ifdef DDS_AUTO_LIVELINESS
	dp->auto_liveliness.name = "DP_ALIVE";
	tmr_start_lock (&dp->auto_liveliness,
			dp->resend_per.secs * 2 * TICKS_PER_SEC,
			(uintptr_t) dp,
			spdp_auto_liveliness_timeout,
			&dp->lock);
#endif
	endpoint = dp->participant.p_handle;
	error = rtps_writer_write (wp,
				   &endpoint, sizeof (endpoint),
				   handle, hci, &time, NULL, 0);
	lock_release (wp->w_lock);
	if (error) {
		fatal_printf ("spdp_start: can't send SPDP Participant Data!");
		return (error);
	}
	return (DDS_RETCODE_OK);
}

/* spdp_update -- Domain participant data was updated. */

int spdp_update (Domain_t *dp)
{
	Writer_t	*wp;
	HCI		hci;
	InstanceHandle	handle;
	FTime_t		time;
	DDS_HANDLE	endpoint;
	int		error;

	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	sys_getftime (&time);
	lock_take (wp->w_lock);

	hci = hc_lookup_key (wp->w_cache, (unsigned char *) &dp->participant.p_guid_prefix,
			sizeof (dp->participant.p_guid_prefix), &handle);
	if (!hci) {
		warn_printf ("spdp_update: failed to lookup instance handle!");
		lock_release (wp->w_lock);
		error = DDS_RETCODE_ALREADY_DELETED;
	}
	else {
		/* Update local domain participant. */
		pl_cache_reset ();

		/* Resend participant data. */
		endpoint = dp->participant.p_handle;
		error = rtps_writer_write (wp,
					   &endpoint, sizeof (endpoint),
					   handle, hci, &time, NULL, 0);
	}
	lock_release (wp->w_lock);
	if (error) {
		fatal_printf ("spdp_update: can't send updated SPDP Participant Data!");
		return (error);
	}
	return (DDS_RETCODE_OK);
}

static void spdp_participant_timeout (uintptr_t user);

#ifdef DDS_NATIVE_SECURITY

static int spdp_reauthorize_participant (Domain_t      *dp,
					 Participant_t *pp,
					 Token_t       *id_tokens,
					 Token_t       *p_tokens,
					 unsigned char rem_key [16],
					 String_t      *user_data)
{
	Token_t		*id_token;
	Token_t		*p_token;
	AuthState_t	state;
	unsigned	caps, rem_id, perm = 0;
	int		authorize;
	DDS_ReturnCode_t error;

	caps = (dp->participant.p_sec_caps & 0xffff) |
	       (dp->participant.p_sec_caps >> 16);
	if (dp->access_protected && !pp->p_p_tokens)
		state = AS_FAILED;
	else
		state = sec_validate_remote_id (dp->participant.p_id,
						dp->participant.p_guid_prefix.prefix,
						caps,
						id_tokens,
						p_tokens,
						&id_token,
						&p_token,
					        rem_key,
					        &rem_id,
					        &error);
	switch (state) {
		case AS_OK:
			if (!dp->access_protected) {
				authorize = DDS_AA_ACCEPTED;
				break;
			}
			perm = sec_validate_remote_permissions (dp->participant.p_id,
								rem_id,
								caps,
								p_token->data,
								id_lookup (rem_id, NULL)->perm_cred,
								&error);
			if (check_peer_participant (perm, user_data)) {
				authorize = DDS_AA_REJECTED;
				state = AS_FAILED;
				log_printf (SPDP_ID, 0, "SPDP: ignore participant (no access)!\r\n");
			}
			else
				authorize = DDS_AA_ACCEPTED;
			break;
		case AS_FAILED:
			authorize = DDS_AA_REJECTED;
			log_printf (SPDP_ID, 0, "SPDP: ignore participant (unknown)!\r\n");
			break;
		case AS_PENDING_RETRY:
		case AS_PENDING_HANDSHAKE_REQ:
		case AS_PENDING_HANDSHAKE_MSG:
		case AS_PENDING_CHALLENGE_MSG:
			/* More work to do: checked after participant creation. */
			authorize = DDS_AA_HANDSHAKE;
			break;
		default:
			/* Just ignore participant. */
			authorize = DDS_AA_REJECTED;
			state = AS_FAILED;
			break;
	}
	pp->p_permissions = perm;
	pp->p_auth_state = state;
	if (authorize == DDS_AA_REJECTED) {
		spdp_end_participant (pp, 1);	/* Ignore participant. */
		return (0);
	}
	return (1);
}

static int spdp_reauthorize (Skiplist_t *list, void *node, void *arg)
{
	Participant_t	*pp, **ppp = (Participant_t **) node;
	Domain_t	*dp = (Domain_t *) arg;
	char            buf [32];
	unsigned        ticks = 1;

	ARG_NOT_USED (list)

	pp = *ppp;

	/* Handshake has to either be OK or FAILED */

	log_printf (SEC_ID, 0, "disc_spdp: reauthorize\r\n");

	if (pp->p_auth_state == AS_FAILED) { 

		/* Failed: do nothing */
		log_printf (SEC_ID, 0, "disc_spdp: state = failed \r\n");
		ticks = 1;
	}
	else if (pp->p_auth_state == AS_OK) {

		/* Was already accepted, keeping DDS traffic alive for now. */
		log_printf (SEC_ID, 0, "disc_spdp: handshake is done for %s, get new permissions\r\n",
			    guid_prefix_str ((GuidPrefix_t *) pp->p_guid_prefix.prefix, buf));
		spdp_reauthorize_participant (dp,
					      pp,
					      pp->p_id_tokens,
					      pp->p_p_tokens,
					      pp->p_guid_prefix.prefix,
					      pp->p_user_data);
		ticks = duration2ticks (&pp->p_lease_duration) + 2;
	}
	else {
		/* Handshake still in progress */
		/* We must force the topics reevaluate to wait */
		log_printf (SEC_ID, 0, "disc_spdp: handshake is not yet done for %s\r\n",
			    guid_prefix_str ((GuidPrefix_t *) pp->p_guid_prefix.prefix, buf));	

		return (1);
	}
	tmr_start_lock (&pp->p_timer, 
			ticks, (uintptr_t) pp, 
			spdp_participant_timeout,
			&dp->lock);
	return (1);
}

/* spdp_rehandshake -- Domain participant permissions data was updated. */

int spdp_rehandshake (Domain_t *dp, int notify_only)
{
	Writer_t	*wp;
	HCI		hci;
	InstanceHandle	handle;
	FTime_t		time;
	DDS_HANDLE	endpoint;
	int		error;

	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	sys_getftime (&time);
	lock_take (wp->w_lock);

	hci = hc_lookup_key (wp->w_cache, (unsigned char *) &dp->participant.p_guid_prefix,
			sizeof (dp->participant.p_guid_prefix), &handle);
	if (!hci) {
		warn_printf ("spdp_rehandshake: failed to lookup instance handle!");
		lock_release (wp->w_lock);
		error = DDS_RETCODE_ALREADY_DELETED;
	}
	else {
		/* Update local domain participant. */
		pl_cache_reset ();

		/* Resend participant data. */
		endpoint = dp->participant.p_handle;
		error = rtps_writer_write (wp,
					   &endpoint, sizeof (endpoint),
					   handle, hci, &time, NULL, 0);
	}
	lock_release (wp->w_lock);
	if (error) {
		fatal_printf ("spdp_rehandshake: can't send updated SPDP Participant Data!");
		return (error);
	}
	if (!notify_only)
		sl_walk (&dp->peers, spdp_reauthorize, dp);
	return (DDS_RETCODE_OK);
}

#endif

/* spdp_end_participant -- End participant due to either a time-out or by an
			   explicit unregister by the peer via a NOT_ALIVE_*
			   change of the keyed instance, or from an ignore().
			   Locked on entry/exit: DP. */

void spdp_end_participant (Participant_t *pp, int ignore)
{
	Domain_t	*dp;
	Reader_t	*rp;
	char            buf [32];

	if ((ignore && entity_ignored (pp->p_flags)) ||
	    entity_shutting_down (pp->p_flags))
		return;

	pp->p_flags |= EF_SHUTDOWN;
	if (spdp_log) {
		log_printf (SPDP_ID, 0, "SPDP: Participant");
		if (pp->p_entity_name)
			log_printf (SPDP_ID, 0, " (%s) [%s]", str_ptr (pp->p_entity_name),
				    guid_prefix_str ((GuidPrefix_t *) pp->p_guid_prefix.prefix, buf));
		log_printf (SPDP_ID, 0, " removed!\r\n");
	}
	dp = pp->p_domain;
	lock_required (dp->lock);

	/* Remove relay as default route. */
	if (pp->p_forward)
		rtps_relay_remove (pp);

	/* Disconnect the SPDP/SEDP endpoints from the peer participant. */
	sedp_disconnect (dp, pp);

	/* Disconnect the Participant Message endpoints. */
	msg_disconnect (dp, pp);

#ifdef DDS_NATIVE_SECURITY
	if (NATIVE_SECURITY (dp)) {

#ifdef DDS_QEO_TYPES
		policy_updater_disconnect (dp, pp);
#endif

		/* Remove the participant from the Interparticipant Stateless Reader/
		   Writer handshake list. */
		psmp_delete (dp, pp);

		/* Disconnect the Crypto Token Transport endpoints. */
		ctt_disconnect (dp, pp);

		/* End knowledge of this identity. */
        if (pp->p_id)
    		sec_release_identity (pp->p_id);
	}
#endif

	/* Release the various ReaderLocator instances that were created for
	   the Stateless Writers and the Stateful Reader/Writer proxies. */
	sl_walk (&pp->p_endpoints, sedp_unmatch_peer_endpoint, pp);

	/* Release the discovered Topics. */
/*	sl_walk (&pp->p_topics, sedp_topic_free, pp);   <== should not be needed! */

#ifdef DDS_FORWARD
	rfwd_participant_dispose (pp);
#endif

	/* Notify the user of the participant's removal. */
	if (dp->builtin_readers [BT_Participant])
		user_notify_delete (pp->p_domain, BT_Participant, pp->p_handle);

	/* Release the various locator lists. */
	locator_list_delete_list (&pp->p_def_ucast);
	locator_list_delete_list (&pp->p_def_mcast);
	locator_list_delete_list (&pp->p_meta_ucast);
	locator_list_delete_list (&pp->p_meta_mcast);
	locator_list_delete_list (&pp->p_src_locators);
#ifdef DDS_SECURITY
	locator_list_delete_list (&pp->p_sec_locs);
#endif

	/* Release Participant user data. */
	if (pp->p_user_data)
		str_unref (pp->p_user_data);

	/* Release the entity name. */
	if (pp->p_entity_name) {
		str_unref (pp->p_entity_name);
		pp->p_entity_name = NULL;
	}

	/* Release the timer. */
	tmr_stop (&pp->p_timer);

	/* If ignore, we're done. */
	if (ignore) {

		/* Set ignored status. */
		pp->p_flags &= ~(EF_NOT_IGNORED | EF_SHUTDOWN);
		return;
	}

	/* Cleanup registered but unfilled cache instances created by RTPS. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PUBLICATION_R];
	if (rp)
		hc_reclaim_keyed (rp->r_cache, &pp->p_guid_prefix);
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_SUBSCRIPTION_R];
	if (rp)
		hc_reclaim_keyed (rp->r_cache, &pp->p_guid_prefix);

	/* Remove the peer participant information. */
	participant_delete (pp->p_domain, pp);
	pl_cache_reset ();
}

# if 0
#define	spdp_buf(n)		char buf [n]
#define	spdp_print1(s,a)	log_printf (DISC_ID, 0, s, a)
#define	spdp_print2(s,a1,a2)	log_printf (DISC_ID, 0, s, a1, a2)
# else
#define spdp_buf(n)
#define	spdp_print1(s,a)
#define	spdp_print2(s,a1,a2)
# endif

/* spdp_participant_timeout -- A participant doesn't send information anymore. */

static void spdp_participant_timeout (uintptr_t user)
{
	Ticks_t		ticks;
	Participant_t	*pp = (Participant_t *) user;
	spdp_buf	(32);

	spdp_print1 ("SPDP: %s -- Timeout\r\n", guid_prefix_str (&pp->p_guid_prefix, buf));
	if (!entity_ignored (pp->p_flags) && pp->p_alive) {
	
		pp->p_alive = 0;
		ticks = duration2ticks (&pp->p_lease_duration) + 2;
		tmr_start_lock (&pp->p_timer,
				ticks,
				(uintptr_t) pp,
				spdp_participant_timeout,
				&pp->p_domain->lock);
		return;
	}

	/* Cleanup endpoint connectivity. */
	spdp_end_participant (pp, 0);
}

void spdp_timeout_participant (Participant_t *p, Ticks_t ticks)
{
	if (p && p->p_domain)
		tmr_start_lock (&p->p_timer,
				ticks,
				(uintptr_t) p,
				spdp_participant_timeout,
				&p->p_domain->lock);
}

/* spdp_stop -- Stop the SPDP discovery protocol. Called from disc_stop with
 		domain_lock and global_lock taken. */

void spdp_stop (Domain_t *dp)
{
	HCI             hci;
	InstanceHandle  handle;
	Writer_t	*pw;
	FTime_t		time;
	int		error;
	Participant_t	**ppp;

	log_printf (SPDP_ID, 0, "SPDP: ending protocol for domain #%u.\r\n", dp->domain_id);

#ifdef DDS_AUTO_LIVELINESS

	/* Stop automatic liveliness. */
	tmr_stop (&dp->auto_liveliness);
#endif
#ifdef DDS_NATIVE_SECURITY

	if (NATIVE_SECURITY (dp)) {

#ifdef DDS_QEO_TYPES
		policy_updater_disable (dp);
#endif
		/* Delete the builtin Interparticipant Stateless endpoints. */
		psmp_disable (dp);

		/* Delete the builtin Crypto Token transport endpoints. */
		ctt_disable (dp);
	}
#endif

	/* Disable the Participant Message endpoints. */
	msg_disable (dp);

	/* Disable SEDP builtin endpoints. */
	sedp_disable (dp);

	/* Remove all peer participants in domain. */
	while ((ppp = sl_head (&dp->peers)) != NULL)
		spdp_end_participant (*ppp, 0);

	/* Inform peer that we're quitting. */
	pw = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	if (!pw) {
		warn_printf ("spdp_stop: failed to lookup participant writer!");
		return;
	}
	lock_take (pw->w_lock);
	hci = hc_lookup_key (pw->w_cache, (unsigned char *) &dp->participant.p_guid_prefix,
			sizeof (dp->participant.p_guid_prefix), &handle);
	if (!hci) {
		warn_printf ("spdp_stop: failed to lookup participant instance handle!");
		lock_release (pw->w_lock);
	}
	else {
		/* Unregister participant instance. */
		sys_getftime (&time);
		error = rtps_writer_unregister (pw, handle, hci, &time, NULL, 0);
		lock_release (pw->w_lock);
		if (error) 
			warn_printf ("spdp_stop: failed to unregister instance handle!");
		else
			thread_yield ();
	}

	/* Disable participant discovery. */
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_R);
        disable_builtin_endpoint (dp, EPB_PARTICIPANT_W);

#ifdef DDS_NATIVE_SECURITY
	if (NATIVE_SECURITY (dp)) {

#ifdef DDS_QEO_TYPES
		policy_updater_stop (dp);
#endif
		/* Stop the Interparticipant Stateless message protocol. */
		psmp_stop (dp);

		/* Stop the Crypto Token transport endpoints. */
		ctt_stop (dp);

		sec_unregister_participant (dp->participant.p_crypto);
		DDS_DomainParticipant_unregister_type ((DDS_DomainParticipant) dp,
						       ParticipantStatelessMessage_ts,
						       "ParticipantStatelessMessage");
		DDS_DomainParticipant_unregister_type ((DDS_DomainParticipant) dp,
						       ParticipantVolatileSecureMessage_ts,
						       "ParticipantVolatileSecureMessage");
		DDS_DomainParticipant_unregister_type ((DDS_DomainParticipant) dp,
						       ParticipantGenericMessage_ts,
						       "ParticipantGenericMessage");
	}
#endif
	/* Delete the Participant Message endpoints. */
	msg_stop (dp);

	/* Delete SEDP builtin endpoints. */
	sedp_stop (dp);

	/* Delete SPDP builtin endpoints. */
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_W);
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_R);
}

void spdp_remote_participant_enable (Domain_t      *dp,
				     Participant_t *pp,
				     unsigned      hs_handle)
{
	Writer_t	  *wp;
#ifdef DDS_NATIVE_SECURITY
	DDS_ReturnCode_t  ret;
	DDS_ParticipantVolatileSecureMessage msg;
#else
	ARG_NOT_USED (hs_handle)
#endif

	log_printf (SPDP_ID, 0, "SPDP: Connecting builtin endpoints.\r\n");

	pp->p_flags |= EF_NOT_IGNORED;

	/* If this is a relay node, use it for routing. */
	if (pp->p_forward) {
		/*if (!rtps_local_node (pp, src))
			pp->p_forward = 0;
		else*/
			rtps_relay_add (pp);
	}

	/* Resend participant info. */
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	rtps_stateless_resend (wp);

#ifdef DDS_NATIVE_SECURITY
	if (NATIVE_SECURITY (dp)) {

		/* Create remote participant crypto material. */
		pp->p_crypto = sec_register_remote_participant (
							dp->participant.p_crypto,
							pp,
							sec_get_auth_plugin (hs_handle),
							sec_get_shared_secret (hs_handle),
							&ret);
		if (!pp->p_crypto) {
			warn_printf ("spdp_remote_participant_enable: can't register crypto for remote participant!");
			return;
		}

		/* Connect the Crypto Token Transport endpoints. */
		ctt_connect (dp, pp);

		/* Create and transfer participant crypto tokens. */
		memset (&msg, 0, sizeof (msg));
		ret = sec_create_local_participant_tokens (dp->participant.p_crypto,
							   pp->p_crypto,
							   &msg.message_data);
		if (ret) {
			warn_printf ("spdp_remote_participant_enable: can't create crypto tokens for remote participant!");
			return;
		}
		msg.message_class_id = GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS;
		wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_W];
		ctt_send (dp, pp, NULL, NULL, &msg);
		sec_release_tokens (&msg.message_data);

#ifdef DDS_QEO_TYPES
		policy_updater_connect (dp, pp);
#endif
	}
#endif

	/* Connect the Participant Message endpoints. */
	msg_connect (dp, pp);

	/* Connect SEDP endpoints to new participant. */
	sedp_connect (dp, pp);

}

/* spdp_new_participant -- Add a new peer participant as discovered by the
			   participant discovery algorithm.
			   On entry/exit: DP locked. */

static void spdp_new_participant (Domain_t                      *dp,
				  SPDPdiscoveredParticipantData *info,
				  LocatorList_t                 srcs)
{
	Participant_t		*pp;
	unsigned		ticks;
	int			authorize;
	char                    buf [32];
#ifdef DDS_SECURITY
	unsigned		perm = 0;
#endif
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	AuthState_t		state;
	Identity_t		rem_id;
	DDS_ReturnCode_t	error;
	Token_t			*id_token;
	Token_t			*p_token;
	unsigned		caps;
#else
	size_t			clen;
#endif
#endif
	if (spdp_log) {
		log_printf (SPDP_ID, 0, "SPDP: New participant");
		if (info->entity_name)
		log_printf (SPDP_ID, 0, " (%s) [%s]", str_ptr (info->entity_name),
			    guid_prefix_str ((GuidPrefix_t *) info->proxy.guid_prefix.prefix, buf));
		log_printf (SPDP_ID, 0, " detected!\r\n");
	}
	authorize = DDS_AA_ACCEPTED;
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
    rem_id = 0;
#endif
	if (dp->security && info->id_tokens) {
#ifdef DDS_NATIVE_SECURITY
		caps = (dp->participant.p_sec_caps & 0xffff) |
				(dp->participant.p_sec_caps >> 16);
		if (dp->access_protected && !info->p_tokens) {
			state = AS_FAILED;
			authorize = DDS_AA_REJECTED;
		}
		else
			state = sec_validate_remote_id (dp->participant.p_id,
							dp->participant.p_guid_prefix.prefix,
							caps,
							info->id_tokens,
							info->p_tokens,
							&id_token,
							&p_token,
						        info->proxy.guid_prefix.prefix,
						        &rem_id,
						        &error);
		/*log_printf (DISC_ID, 0, "SPDP: state is %d\r\n", state);*/
		switch (state) {
			case AS_OK:
				if (!dp->access_protected) {
					perm = 0;
					break;
				}
				perm = sec_validate_remote_permissions (dp->participant.p_id,
									rem_id,
									caps,
									p_token->data,
									NULL,
									&error);
				if (check_peer_participant (perm, info->user_data)) {
					state = AS_FAILED;
					authorize = DDS_AA_REJECTED;
					log_printf (SPDP_ID, 0, "SPDP: ignore participant (no access)!\r\n");
				}
				else
					authorize = DDS_AA_ACCEPTED;
				break;
			case AS_FAILED:
				authorize = DDS_AA_REJECTED;
				log_printf (SPDP_ID, 0, "SPDP: ignore participant (unknown)!\r\n");
				break;
			case AS_PENDING_RETRY:
			case AS_PENDING_HANDSHAKE_REQ:
			case AS_PENDING_HANDSHAKE_MSG:
			case AS_PENDING_CHALLENGE_MSG:
				/* More work to do: checked after participant creation. */
				authorize = DDS_AA_HANDSHAKE;
				break;
			default:
				/* Just ignore participant. */
				authorize = DDS_AA_REJECTED;
				state = AS_FAILED;
				break;
		}
#else
		clen = 0;
		authorize = validate_peer_identity (dp->participant.p_id,
						    NULL,
						    (unsigned char *) str_ptr (info->id_tokens),
						    str_len (info->id_tokens),
						    NULL,
						    &clen);
		if (authorize == DDS_AA_ACCEPTED) {
			perm = validate_peer_permissions (dp->domain_id,
							  (unsigned char *) str_ptr (info->p_tokens),
							  str_len (info->p_tokens));
			if (check_peer_participant (perm, info->user_data) !=
								DDS_RETCODE_OK)	{
				log_printf (SPDP_ID, 0, "SPDP: ignore participant!\r\n");
				authorize = DDS_AA_REJECTED;
			}
		}
#endif
	}
	else if (dp->security) {
		authorize = DDS_AA_REJECTED;
#ifdef DDS_NATIVE_SECURITY
		state = AS_FAILED;
#endif
	}
	else {
#ifdef DDS_NATIVE_SECURITY
		state = AS_OK;
#endif
#endif
		authorize = DDS_AA_ACCEPTED;
#ifdef DDS_SECURITY
	}
#endif
	pp = disc_remote_participant_add (dp, info, srcs, authorize);
	if (!pp)
		return;

#ifdef DDS_SECURITY
	pp->p_permissions = perm;
#ifdef DDS_NATIVE_SECURITY
	pp->p_auth_state = state;
	pp->p_id = rem_id;
#endif
#endif
	if (authorize == DDS_AA_ACCEPTED)
		spdp_remote_participant_enable (dp, pp, 0);
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	else if (state == AS_PENDING_RETRY) {
		if (entity_active (entity_flags (&dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_W]->entity)) &&
		    entity_active (entity_flags (&dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_R]->entity)))	
			psmp_retry_validate (dp, pp, 0);
	}
	else if (state == AS_PENDING_HANDSHAKE_REQ) {
		if (entity_active (entity_flags (&dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_W]->entity)) &&
		    entity_active (entity_flags (&dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_R]->entity)))
			psmp_handshake_initiate (dp, pp, id_token, p_token, 0);
	}
	else if (state == AS_PENDING_HANDSHAKE_MSG ||
		 state == AS_PENDING_CHALLENGE_MSG) {
		if (entity_active (entity_flags (&dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_W]->entity)) &&
		    entity_active (entity_flags (&dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_R]->entity))) {
			pp->p_id = rem_id;
			psmp_handshake_wait (dp, pp, id_token, p_token, 0);
		}
	}
#endif
#endif

	/* Start participant timer. */
	ticks = duration2ticks (&pp->p_lease_duration) + 2;
	tmr_init (&pp->p_timer, "DiscParticipant");
	tmr_start_lock (&pp->p_timer,
		        ticks,
		        (uintptr_t) pp,
		        spdp_participant_timeout,
		        &dp->lock);

}

/* update_locators -- Update a locator list if the new one is different. */

static int update_locators (LocatorList_t *dlp, LocatorList_t *slp)
{
	if (locator_list_equal (*dlp, *slp))
		return (0);

	locator_list_delete_list (dlp);
	*dlp = *slp;
	*slp = NULL;
	return (1);
}

/* endpoint_locators_update -- Update the locators of an endpoint due to changed
			       locators. */

static int endpoint_locators_update (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	unsigned	ofs, *n = (unsigned *) arg;

	ARG_NOT_USED (list)

	ep = *epp;
	if ((ep->entity.flags & EF_BUILTIN) != 0)
		ofs = 2;
	else
		ofs = 0;
	if (!n [ofs] && !n [ofs + 1])
		return (1);

	if (n [ofs])
		rtps_endpoint_locators_update (ep, 0);
	if (n [ofs + 1])
		rtps_endpoint_locators_update (ep, 1);

	return (1);
}

/* endpoint_locators_local -- Update the locators of an endpoint due to locality
			      changes. */

static int endpoint_locators_local (Skiplist_t *list, void *node, void *arg)
{
	Endpoint_t	*ep, **epp = (Endpoint_t **) node;
	Ticks_t		local, *p_local = (Ticks_t *) arg;

	ARG_NOT_USED (list)

	ep = *epp;
	local = *p_local;
	rtps_endpoint_locality_update (ep, local);

	return (1);
}

#ifdef DDS_SECURITY

unsigned ntokens (Token_t *tp)
{
	Token_t		*p;
	unsigned	n = 0;

	for (p = tp; p; p = p->next)
		n++;
	return (n);
}

int same_token (Token_t *tp1, Token_t *tp2)
{
	if (!tp1 || !tp2)
		return (0);

#ifdef DDS_NATIVE_SECURITY
	if (!tp1->data ||
	    !tp2->data ||
	    !tp1->data->class_id ||
	    !tp2->data->class_id ||
	    !tp1->data->binary_value1 ||
	    !tp2->data->binary_value1)
		return (0);

	if (strcmp (tp1->data->class_id, tp2->data->class_id))
		return (0);

	if (DDS_SEQ_LENGTH (*tp1->data->binary_value1) != 
	    DDS_SEQ_LENGTH (*tp2->data->binary_value1))
		return (0);

	return (!memcmp (DDS_SEQ_DATA (*tp1->data->binary_value1),
			 DDS_SEQ_DATA (*tp2->data->binary_value1),
			 DDS_SEQ_LENGTH (*tp1->data->binary_value1)));
#else
	if (str_len (tp1) != str_len (tp2))
		return (0);

	return (!memcmp (str_ptr (tp1), str_ptr (tp2)));
#endif	
}

static int spdp_tokens_changed (Token_t *otp, Token_t *ntp)
{
	Token_t	*tp, *xp;
	int	found;

	if (ntokens (otp) != ntokens (ntp))
		return (1);

	for (tp = ntp; tp; tp = tp->next) {
		found = 0;
		for (xp = otp; xp; xp = xp->next)
			if (same_token (tp, xp)) {
				found = 1;
				break;
			}
		if (!found)
			return (1);
	}
	return (0);
}

#endif

#define	MAX_LOCAL_TICKS		(TICKS_PER_SEC * 45)

/* spdp_update_participant -- Update an existing peer participant.
			      On entry/exit: DP locked. */

static void spdp_update_participant (Domain_t                      *dp,
				     Participant_t                 *pp,
				     SPDPdiscoveredParticipantData *info,
				     LocatorList_t                 srcs)
{
	unsigned	n [4];
#ifdef DDS_SECURITY
	unsigned	ns;
#endif
	LocatorRef_t	*rp, *srp;
	LocatorNode_t	*np, *snp;
	int		proxy_local_update;
	int		relay_update;
	int		locators_update;
	unsigned	is_local;
	Ticks_t		ticks;
	char		buf [30];

	ARG_NOT_USED (dp)

	if (entity_ignored (pp->p_flags))
		return;	/* Let it timeout if timer is active! */

# if 0
	log_printf (SPDP_ID, 0, "SPDP: Update participant");
	if (srcs) {
		log_printf (SPDP_ID, 0, " (from");
		foreach_locator (srcs, srp, snp)
			log_printf (SPDP_ID, 0, " %s", locator_str (&snp->locator));
		log_printf (SPDP_ID, 0, ")");
	}
	log_printf (SPDP_ID, 0, "\r\n");
# endif

	pp->p_no_mcast = info->proxy.no_mcast;

#ifdef DDS_NATIVE_SECURITY
	if (dp->security) {

		/* Identity tokens changed? */
		if (spdp_tokens_changed (pp->p_id_tokens, info->id_tokens)) {

			/* Just restart as if a new host connected. */
			log_printf (SPDP_ID, 0, "SPDP: Update participant: IdTokens changed!\r\n");
			psmp_delete (dp, pp);
			spdp_end_participant (pp, 0);
			spdp_new_participant (dp, info, srcs);
			return;
		}
#if 0
		/* Permission tokens changed? */
		if (spdp_tokens_changed (pp->p_p_tokens, info->p_tokens)) {

			/* Need to restart handshake. */
			log_printf (SPDP_ID, 0, "SPDP: Update participant: PermTokens changed!\r\n");
			if (pp->p_auth_state == AS_OK_FINAL_MSG)
				pp->p_auth_state = AS_OK;
			psmp_delete (dp, pp);

			if (pp->p_auth_state != AS_OK) {

				/* If previous handshake wasn't successful,
				   just restart. */
				log_printf (SPDP_ID, 0, "SPDP: Update participant: need restart!\r\n");
				spdp_end_participant (pp, 0);
				spdp_new_participant (dp, info, srcs);
				return;
			}
			else if (!spdp_reauthorize_participant (dp,
								pp,
							        info->id_tokens,
							        info->p_tokens,
							        info->proxy.guid_prefix.prefix,
							        info->user_data)) {
				log_printf (SPDP_ID, 0, "SPDP: Update participant: reauthorize failed!\r\n");
				return;
			}
			else {
				log_printf (SPDP_ID, 0, "SPDP: Update participant: use new permissions!\r\n");
				token_unref (pp->p_p_tokens);
				pp->p_p_tokens = info->p_tokens;
				info->p_tokens = NULL;

				/* Fall through to take into account participant data. */
			}
			log_printf (SPDP_ID, 0, "SPDP: Update participant: use other data!\r\n");
		}
#endif
	}
#endif
	/* Update unicast participant locators if necessary. */
	n [0] = update_locators (&pp->p_def_ucast, &info->proxy.def_ucast);
	n [1] = update_locators (&pp->p_def_mcast, &info->proxy.def_mcast);
	n [2] = update_locators (&pp->p_meta_ucast, &info->proxy.meta_ucast);
	n [3] = update_locators (&pp->p_meta_mcast, &info->proxy.meta_mcast);
#ifdef DDS_SECURITY
	if (dp->security) {
		ns = update_locators (&pp->p_sec_locs, &info->proxy.sec_locs);
		n [0] += ns;
		n [2] += ns;
	}
#endif
	proxy_local_update = 0;
	locators_update = ((n [0] + n [1] + n [2] + n [3]) != 0);

	/* Remember who sent this. */
	is_local = 0;
	if (srcs) {
		foreach_locator (srcs, srp, snp) {
			if (!is_local &&
			    (snp->locator.kind & LOCATOR_KINDS_UDP) != 0)
				foreach_locator (pp->p_def_ucast, rp, np)
					if (locator_addr_equal (&np->locator, &snp->locator)) {
						is_local = 1;
						/*log_printf (SPDP_ID, 0, "SPDP: Found match!\r\n");*/
						break;
					}

			/* Remember who sent this. */
			locator_list_copy_node (&pp->p_src_locators, snp);
		}
		if (is_local) {
			if (!pp->p_local)
				proxy_local_update = 1;
			pp->p_local = sys_ticks_last;
		}
	}
	if (!is_local &&
	    pp->p_local && 
	    sys_ticksdiff (pp->p_local, sys_ticks_last) > MAX_LOCAL_TICKS) {
		proxy_local_update = 1;
		pp->p_local = 0;
		log_printf (SPDP_ID, 0, "SPDP: Participant locality time-out (%s).\r\n",
						guid_prefix_str (&pp->p_guid_prefix, buf));
	}
	relay_update = (pp->p_forward != info->proxy.forward ||
			(pp->p_forward && locators_update));

	/* If any locators have changed, update all proxies connected to
	   endpoints for this participant! */
	if (locators_update && !relay_update) {
		log_printf (SPDP_ID, 0, "SPDP: Participant locators update (%s).\r\n",
						guid_prefix_str (&pp->p_guid_prefix, buf));
		sl_walk (&pp->p_endpoints, endpoint_locators_update, n);
		locator_list_delete_list (&pp->p_src_locators);
	}

	/* Else if locality changes, update all related proxies as well. */
	else if (proxy_local_update && !relay_update) {
		log_printf (SPDP_ID, 0, "SPDP: Participant locality update (%s is %s now).\r\n",
			guid_prefix_str (&pp->p_guid_prefix, buf), (pp->p_local) ? "local" : "remote");
		sl_walk (&pp->p_endpoints, endpoint_locators_local, &pp->p_local);
	}

	/* Else if relay info is updated, update all proxies. */
	else if (relay_update) {
		log_printf (SPDP_ID, 0, "SPDP: Relay update (%s).\r\n",
						guid_prefix_str (&pp->p_guid_prefix, buf));
		/*if (info->proxy.forward && !rtps_local_node (pp, src))
			info->proxy.forward = 0;*/
		if (pp->p_forward && !info->proxy.forward) {
			pp->p_forward = info->proxy.forward;
			rtps_relay_remove (pp);
		}
		else if (!pp->p_forward && info->proxy.forward) {
			pp->p_forward = info->proxy.forward;
			rtps_relay_add (pp);
		}
		else 
			rtps_relay_update (pp);
	}

#ifdef DDS_FORWARD
	rfwd_participant_new (pp, 1);
#endif

	/* Update liveliness if changed. */
	if (pp->p_man_liveliness != info->proxy.manual_liveliness) {
		pp->p_man_liveliness = info->proxy.manual_liveliness;
#ifdef LOG_LIVELINESS
		log_printf (SPDP_ID, 0, "SPDP: Participant asserted (%s).\r\n",
						guid_prefix_str (&pp->p_prefix, buf));
#endif
		liveliness_participant_asserted (pp);
	}

	/* Check if user-data has changed. */
	if ((!pp->p_user_data && info->user_data) ||
	    (pp->p_user_data && !info->user_data) ||
	    (pp->p_user_data && info->user_data &&
	     (str_len (pp->p_user_data) != str_len (info->user_data) ||
	      !memcmp (str_ptr (pp->p_user_data),
		       str_ptr (info->user_data),
		       str_len (pp->p_user_data))))) {
		if (pp->p_user_data)
			str_unref (pp->p_user_data);
		pp->p_user_data = info->user_data;
		info->user_data = NULL;
		if (pp->p_domain->builtin_readers [BT_Participant])
			user_participant_notify (pp, 0);
	}
	pp->p_alive = 0;
	ticks = duration2ticks (&pp->p_lease_duration) + 2;
	tmr_start_lock (&pp->p_timer,
		        ticks,
		        (uintptr_t) pp,
		        spdp_participant_timeout,
			&dp->lock);
}

/* spdp_event -- New participant data available to be read callback function.
		 Locked on entry/exit: DP + R(rp). */

void spdp_event (Reader_t *rp, NotificationType_t t)
{
	Domain_t			*dp = rp->r_subscriber->domain;
	Participant_t			*pp;
	ChangeData_t			change;
	SPDPdiscoveredParticipantData	*info = NULL, tinfo;
	GuidPrefix_t	  		*guidprefixp;
	RemPrefix_t			*prp;
	InfoType_t                      type;
	int				error;
	spdp_buf			(32);

	lock_required (dp->lock);
	lock_required (rp->r_lock);

	if (t != NT_DATA_AVAILABLE)
		return;

	rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
	for (;;) {
		change.data = &info;
		if (info) {
			pid_participant_data_cleanup (info);
			xfree (info);
			info = NULL;
		}
		/*dtrc_print0 ("SPDP: get samples");*/
		error = disc_get_data (rp, &change);
		if (error) {
			/*dtrc_print0 ("- none\r\n");*/
			break;
		}
		/*dtrc_print1 ("- valid(%u)\r\n", change.kind);*/
		if (change.kind == ALIVE) {
			info = change.data;
			type = EI_NEW;
			guidprefixp = &info->proxy.guid_prefix;
		}
		else {
			error = hc_get_key (rp->r_cache, change.h, &tinfo, 0);
			if (error)
				continue;

			guidprefixp = &tinfo.proxy.guid_prefix;
			type = EI_DELETE;
		}
		spdp_print2 ("SPDP: %s => %u\r\n", guid_prefix_str (guidprefixp, buf), type);
		prp = prefix_lookup (dp, guidprefixp);
		if (memcmp (&dp->participant.p_guid_prefix,
			    guidprefixp,
			    sizeof (GuidPrefix_t)) != 0/* && prp*/) {
			pp = participant_lookup (dp, guidprefixp);
			if (type == EI_DELETE) {
				if (pp)
					spdp_end_participant (pp, 0);
			}
			else if (!pp)
				spdp_new_participant (dp, info,
						(prp) ? prp->locators : NULL);
			else
				spdp_update_participant (dp, pp, info, 
						 (prp) ? prp->locators : NULL);
		}
		if (prp)
			prefix_forget (prp);
		hc_inst_free (rp->r_cache, change.h);
	}
	if (info) {
		pid_participant_data_cleanup (info);
		xfree (info);
	}
}

/* spdp_send_participant_liveliness -- Resend Asserted Participant liveliness. */

int spdp_send_participant_liveliness (Domain_t *dp)
{
	Writer_t	*wp;
	HCI		hci;
	InstanceHandle	handle;
	FTime_t		time;
	DDS_HANDLE	endpoint;
	int		error;

	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
#ifdef LOG_LIVELINESS
	log_printf (SPDP_ID, 0, "SPDP: Assert Participant.\r\n");
#endif
	sys_getftime (&time);
	lock_take (wp->w_lock);
	hci = hc_register (wp->w_cache, dp->participant.p_guid_prefix.prefix,
					sizeof (GuidPrefix_t), &time, &handle);

	endpoint = dp->participant.p_handle;

	/* Update local domain participant assertion count. */
	dp->participant.p_man_liveliness++;
	pl_cache_reset ();

	/* Resend participant data. */
	error = rtps_writer_write (wp,
				   &endpoint, sizeof (endpoint),
				   handle, hci, &time, NULL, 0);
	lock_release (wp->w_lock);
	if (error) {
		fatal_printf ("spdp_start: can't send updated SPDP Participant Data!");
		return (error);
	}
	return (DDS_RETCODE_OK);
}

#endif

