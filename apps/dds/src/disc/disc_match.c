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

/* disc_match -- Register discovery notification functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "log.h"
#include "error.h"
#include "dcps.h"
#include "guard.h"
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_access.h"
#include "sec_crypto.h"
#endif
#endif
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#ifdef DDS_NATIVE_SECURITY
#include "disc_ctt.h"
#endif
#include "disc_match.h"

static DMATCHFCT	notify_match;		/* Match notification. */
static DUMATCHFCT	notify_unmatch;		/* Unmatch notification. */
static DUMDONEFCT	notify_done;		/* Done notification. */

void disc_register (DMATCHFCT  n_match,
		    DUMATCHFCT n_unmatch,
		    DUMDONEFCT n_done)
{
	notify_match = n_match;
	notify_unmatch = n_unmatch;
	notify_done = n_done;
}

/* user_topic_notify -- Notify a discovered Topic to discovery listeners. */

void user_topic_notify (Topic_t *tp, int new1)
{
	Domain_t	*dp;
	KeyHash_t	hash;
	Reader_t	*rp;
	Cache_t		cache;
	Change_t	*cp;
	InstanceHandle	h;
	int		error;

	dp = tp->domain;
	rp = dp->builtin_readers [BT_Topic];
	if (!rp)
		return;

	cp = hc_change_new ();
	if (!cp)
		goto notif_err;

	lock_take (rp->r_lock);
	cache = rp->r_cache;
	cp->c_writer = cp->c_handle = tp->entity.handle;
	if (new1) {
		topic_key_from_name (str_ptr (tp->name),
				     str_len (tp->name) - 1,
				     str_ptr (tp->type->type_name),
				     str_len (tp->type->type_name) - 1,
				     &hash);
		memset (&hash.hash [sizeof (GuidPrefix_t)], 0, sizeof (EntityId_t));
		if (hc_lookup_hash (cache, &hash, hash.hash,
		    		     sizeof (DDS_BuiltinTopicKey_t),
				     &h, 0, 0, NULL) &&
		    h != cp->c_handle)
			hc_inst_free (cache, h); /* Remove lingering items. */
		hc_lookup_hash (cache, &hash, hash.hash, 
				sizeof (DDS_BuiltinTopicKey_t),
				&cp->c_handle,
				LH_ADD_SET_H, 0, NULL);
	}
 	cp->c_kind = ALIVE;
 	cp->c_data = cp->c_xdata;
 	cp->c_length = sizeof (cp->c_writer);
 	memcpy (cp->c_xdata, &cp->c_writer, sizeof (cp->c_writer));
	error = hc_add_inst (cache, cp, NULL, 0);
	if (!error)
		tp->entity.flags |= EF_CACHED;
	lock_release (rp->r_lock);
	if (!error)
		return;

    notif_err:
	warn_printf ("Discovered topic notification failed!");
}

/* user_reader_notify -- Notify a discovered Reader to discovery listeners. */

void user_reader_notify (DiscoveredReader_t *rp, int new1)
{
	Domain_t	*dp;
	KeyHash_t	hash;
	Reader_t	*nrp;
	Cache_t		cache;
	Change_t	*cp;
	InstanceHandle	h;
	int		error;

	dp = rp->dr_participant->p_domain;
	nrp = dp->builtin_readers [BT_Subscription];
	if (!nrp)
		return;

	cp = hc_change_new ();
	if (!cp)
		goto notif_err;

	memcpy (hash.hash, &rp->dr_participant->p_guid_prefix, 
						sizeof (GuidPrefix_t) - 4);
	memcpy (&hash.hash [sizeof (GuidPrefix_t) - 4], &rp->dr_entity_id,
							sizeof (EntityId_t));
	memset (&hash.hash [12], 0, 4); 
	lock_take (nrp->r_lock);
	cache = nrp->r_cache;
	cp->c_writer = cp->c_handle = rp->dr_handle;
	if (new1 &&
	    !hc_lookup_hash (cache, &hash, hash.hash,
	    		     sizeof (DDS_BuiltinTopicKey_t),
			     &h, 0, 0, NULL) &&
	    h != cp->c_handle)
		hc_inst_free (cache, h);
	hc_lookup_hash (cache, &hash, hash.hash, sizeof (DDS_BuiltinTopicKey_t),
				&cp->c_handle, (new1) ? LH_ADD_SET_H : 0, 0, NULL);
	cp->c_kind = ALIVE;
	cp->c_data = cp->c_xdata;
	cp->c_length = sizeof (cp->c_writer);
	memcpy (cp->c_xdata, &cp->c_writer, sizeof (cp->c_writer));
	error = hc_add_inst (cache, cp, NULL, 0);
	if (!error)
		rp->dr_flags |= EF_CACHED;
	lock_release (nrp->r_lock);
	if (!error)
		return;

    notif_err:
	warn_printf ("Discovered reader notification failed!");
}

/* user_writer_notify -- Notify a discovered Writer to discovery listeners. */

void user_writer_notify (DiscoveredWriter_t *wp, int new1)
{
	Domain_t	*dp;
	KeyHash_t	hash;
	Reader_t	*rp;
	Cache_t		cache;
	Change_t	*cp;
	InstanceHandle	h;
	int		error = 0;

	dp = wp->dw_participant->p_domain;
	rp = dp->builtin_readers [BT_Publication];
	if (!rp)
		return;

	cp = hc_change_new ();
	if (!cp)
		goto notif_err;

	memcpy (hash.hash, &wp->dw_participant->p_guid_prefix, 
						sizeof (GuidPrefix_t) - 4);
	memcpy (&hash.hash [sizeof (GuidPrefix_t) - 4], &wp->dw_entity_id,
							sizeof (EntityId_t));
	memset (&hash.hash [12], 0, 4); 
	lock_take (rp->r_lock);
	cache = rp->r_cache;
	cp->c_writer = cp->c_handle = wp->dw_handle;
	if (new1 &&
	    !hc_lookup_hash (cache, &hash, hash.hash,
	    		     sizeof (DDS_BuiltinTopicKey_t),
			     &h, 0, 0, NULL) &&
	    h != cp->c_handle)
		hc_inst_free (cache, h);
	hc_lookup_hash (cache, &hash, hash.hash, sizeof (DDS_BuiltinTopicKey_t),
					&cp->c_handle,
					(new1) ? LH_ADD_SET_H : 0, 0, NULL);
	cp->c_kind = ALIVE;
	cp->c_data = cp->c_xdata;
	cp->c_length = sizeof (cp->c_writer);
	memcpy (cp->c_xdata, &cp->c_writer, sizeof (cp->c_writer));
	error = hc_add_inst (cache, cp, NULL, 0);
	if (!error)
		wp->dw_flags |= EF_CACHED;
	lock_release (rp->r_lock);
	if (!error)
		return;

    notif_err:
	warn_printf ("Discovered writer notification failed! %d", error);
}

typedef struct match_data_st {
	LocatorList_t	cur_uc;
	LocatorList_t	cur_mc;
	LocatorList_t	res_uc;
	Endpoint_t	*ep;
	const char	*names [2];
	unsigned	nmatches;
	int		qos;
	const EntityId_t *eid;
} MatchData_t;

#ifdef SIMPLE_DISCOVERY

void user_notify_delete (Domain_t       *dp,
			 Builtin_Type_t type,
			 InstanceHandle h)
{
	Reader_t	  *rp;
	Cache_t		  cache;
	Change_t	  *cp;
	int		  error;
	static const char *btype_str [] = {
		"participant", "writer", "reader", "topic"
	};

	rp = dp->builtin_readers [type];
	if (!rp)
		return;

	cp = hc_change_new ();
	if (!cp)
		return;

	lock_take (rp->r_lock);
	cache = rp->r_cache;
	cp->c_writer = cp->c_handle = h;
	cp->c_kind = NOT_ALIVE_UNREGISTERED;
	cp->c_data = NULL;
	cp->c_length = 0;
	error = hc_add_inst (cache, cp, NULL, 0);
	lock_release (rp->r_lock);
	if (!error)
		return;

	warn_printf ("Deletion of discovered %s notification failed! (%d)", 
							btype_str [type], error);
}

#endif
 
/* disc_new_matched_reader -- A match between one of our writers and a remote
			      reader was detected.
			      On entry/exit: DP, TP, R/W locked. */

void disc_new_matched_reader (Writer_t *wp, DiscoveredReader_t *peer_rp)
{
	int					e;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t				*dp;
	DDS_ParticipantVolatileSecureMessage	msg;
	DataReaderCrypto_t			crypto;
	DDS_ReturnCode_t			ret;

	dp = wp->w_publisher->domain;
#endif
	if (disc_log)
		log_printf (DISC_ID, 0, "Discovery: Matched reader detected!\r\n");

	e = (*notify_match) (&wp->w_lep, &peer_rp->dr_ep);
	if (!e)
		return;

	rtps_matched_reader_add (wp, peer_rp);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp) && (wp->w_submsg_prot || wp->w_payload_prot)) {
		crypto = sec_register_remote_datareader (wp->w_crypto,
						 peer_rp->dr_participant->p_crypto,
						 peer_rp,
						 0,
						 &ret);
		if (!crypto) {
			warn_printf ("disc_new_matched_reader: can't create crypto material!");
			return;
		}
		rtps_peer_reader_crypto_set (wp, peer_rp, crypto);
		memset (&msg, 0, sizeof (msg));
		ret = sec_create_local_datawriter_tokens (wp->w_crypto,
							  crypto,
							  &msg.message_data);
		if (ret) {
			warn_printf ("disc_new_matched_reader: cant't create crypto tokens!");
			return;
		}
		msg.message_class_id = GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS;
		ctt_send (dp, peer_rp->dr_participant, &wp->w_ep, &peer_rp->dr_ep, &msg);
		sec_release_tokens (&msg.message_data);
	}
#endif
	dcps_publication_match (wp, 1, &peer_rp->dr_ep);
	liveliness_enable (&wp->w_ep, &peer_rp->dr_ep);
	deadline_enable (&wp->w_ep, &peer_rp->dr_ep);
}

/* disc_new_matched_writer -- A match between one of our readers and a remote
			      writer was detected.
			      On entry/exit: DP, TP, R/W locked. */

void disc_new_matched_writer (Reader_t *rp, DiscoveredWriter_t *peer_wp)
{
	int					e;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	Domain_t				*dp;
	DDS_ParticipantVolatileSecureMessage	msg;
	DataWriterCrypto_t			crypto;
	DDS_ReturnCode_t			ret;

	dp = rp->r_subscriber->domain;
#endif
	if (disc_log)
		log_printf (DISC_ID, 0, "Discovery: Match writer detected!\r\n");

	e = (*notify_match) (&rp->r_lep, &peer_wp->dw_ep);
	if (!e)
		return;

	hc_rem_writer_add (rp->r_cache, peer_wp->dw_handle);
	rtps_matched_writer_add (rp, peer_wp);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp) && (rp->r_submsg_prot || rp->r_payload_prot)) {
		crypto = sec_register_remote_datawriter (rp->r_crypto,
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
	dcps_subscription_match (rp, 1, &peer_wp->dw_ep);
	liveliness_enable (&peer_wp->dw_ep, &rp->r_ep);
	deadline_enable (&peer_wp->dw_ep, &rp->r_ep);
	lifespan_enable (&peer_wp->dw_ep, &rp->r_ep);
}

#ifdef SIMPLE_DISCOVERY

/* disc_end_matched_reader -- A match between one of our writers and a remote
			      reader was removed.
			      On entry/exit: DP, TP, R/W locked. */

void disc_end_matched_reader (Writer_t *wp, DiscoveredReader_t *peer_rp)
{
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	DataReaderCrypto_t	crypto;
#endif
	int			e;

	if (disc_log)
		log_printf (DISC_ID, 0, "Discovery: Matched reader removed!\r\n");

	e = (*notify_unmatch) (&wp->w_lep, &peer_rp->dr_ep);
	liveliness_disable (&wp->w_ep, &peer_rp->dr_ep);
	deadline_disable (&wp->w_ep, &peer_rp->dr_ep);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	crypto = rtps_peer_reader_crypto_get (wp, peer_rp);
	if (crypto)
		sec_unregister_datareader (crypto);
#endif
	rtps_matched_reader_remove (wp, peer_rp);
	dcps_publication_match (wp, 0, &peer_rp->dr_ep);
	if (e)
		(*notify_done) (&wp->w_lep);
}

/* disc_end_matched_writer -- A match between one of our readers and a remote
			      writer was removed.
			      On entry/exit: DP, TP, R/W locked. */

void disc_end_matched_writer (Reader_t *rp, DiscoveredWriter_t *peer_wp)
{
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	DataWriterCrypto_t	crypto;
#endif
	int			e;

	if (disc_log)
		log_printf (DISC_ID, 0, "Discovery: Match writer removed!\r\n");

	e = (*notify_unmatch) (&rp->r_lep, &peer_wp->dw_ep);
	lifespan_disable (&peer_wp->dw_ep, &rp->r_ep);
	deadline_disable (&peer_wp->dw_ep, &rp->r_ep);
	liveliness_disable (&peer_wp->dw_ep, &rp->r_ep);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	crypto = rtps_peer_writer_crypto_get (rp, peer_wp);
	if (crypto)
		sec_unregister_datawriter (crypto);
#endif
	rtps_matched_writer_remove (rp, peer_wp);
	hc_rem_writer_removed (rp->r_cache, peer_wp->dw_handle);
	dcps_subscription_match (rp, 0, &peer_wp->dw_ep);
	if (e)
		(*notify_done) (&rp->r_lep);
}

/* user_participant_notify -- Notify a discovered Participant to discovery
			      listeners.
			      Locked on entry/exit: DP. */

void user_participant_notify (Participant_t *pp, int new1)
{
	Domain_t	*dp;
	KeyHash_t	hash;
	Cache_t		cache;
	Change_t	*cp;
	InstanceHandle	h;
	Reader_t	*rp;
	int		error;

	dp = pp->p_domain;
	rp = dp->builtin_readers [BT_Participant];
	if (!rp)
		return;

	cp = hc_change_new ();
	if (!cp)
		goto notif_err;

	memcpy (hash.hash, &pp->p_guid_prefix, sizeof (GuidPrefix_t));
	memset (&hash.hash [sizeof (GuidPrefix_t)], 0, sizeof (EntityId_t));
	lock_take (rp->r_lock);
	cache = rp->r_cache;
	cp->c_writer = cp->c_handle = pp->p_handle;
	if (new1 &&
	    hc_lookup_hash (cache, &hash, hash.hash,
	    		     sizeof (DDS_BuiltinTopicKey_t),
			     &h, 0, 0, NULL) &&
	    h != cp->c_handle)
		hc_inst_free (cache, h);
	hc_lookup_hash (cache, &hash, hash.hash, sizeof (DDS_BuiltinTopicKey_t),
					&cp->c_handle,
					(new1) ? LH_ADD_SET_H : 0, 0, NULL);
	cp->c_kind = ALIVE;
	cp->c_data = cp->c_xdata;
	cp->c_length = sizeof (cp->c_writer);
	memcpy (cp->c_xdata, &cp->c_writer, sizeof (cp->c_writer));
	error = hc_add_inst (cache, cp, NULL, 0);
	if (!error)
		pp->p_flags |= EF_CACHED;
	lock_release (rp->r_lock);
	if (!error)
		return;

    notif_err:
	warn_printf ("Discovered participant notification failed!");
}

#endif /* SIMPLE_DISCOVERY */
