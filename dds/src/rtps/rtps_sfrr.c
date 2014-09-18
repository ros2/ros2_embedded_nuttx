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

/* rtps_sfrr.c -- Implements the RTPS Reliable Stateful Reader. */

#include "log.h"
#include "prof.h"
#include "error.h"
#include "ctrace.h"
#include "set.h"
#include "list.h"
#include "dcps.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_crypto.h"
#endif
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_msg.h"
#include "rtps_trace.h"
#include "rtps_check.h"
#include "rtps_priv.h"
#include "rtps_clist.h"
#ifdef RTPS_FRAGMENTS
#include "rtps_frag.h"
#endif
#include "rtps_sfrr.h"

#ifdef RTPS_PROXY_INST
static unsigned rtps_rw_insts;	/* Counts Remote Writer proxy instances. */
#endif
#ifdef RTPS_INIT_ACKNACK

/* sfr_rel_alive -- Peer became alive. */

static void sfr_rel_alive (RemWriter_t *rwp)
{
	rwp->rw_peer_alive = 1;
	RW_SIGNAL (rwp, "=> ALIVE");
	RALIVE_TMR_STOP (rwp, rwp->rw_hbrsp_timer);
	RALIVE_TMR_FREE (rwp);
	tmr_free (rwp->rw_hbrsp_timer);
	rwp->rw_hbrsp_timer = NULL;
}

/* sfr_rel_init_acknack -- Send an empty ACKNACK message. */

static void sfr_rel_init_acknack (RemWriter_t *rwp)
{
	SequenceNumber_t seqnr;
	unsigned	 nbits;
	uint32_t	 bitmaps [8];

	seqnr.low = 0 /*1*/;
	seqnr.high = 0;
#ifndef RTPS_EMPTY_ACKNACK
	bitmaps [0] = 0UL;
	SET_ADD (bitmaps, 0);
	nbits = 1;
#else
	nbits = 0;
#endif
	rtps_msg_add_acknack (rwp,
			      (DiscoveredWriter_t *) rwp->rw_endpoint,
			      0,
			      &seqnr,
			      nbits,
			      bitmaps);
}

/* sfr_rel_alive_to -- Alive timer elapsed. */

static void sfr_rel_alive_to (uintptr_t user)
{
	RemWriter_t	 *rwp = (RemWriter_t *) user;
	Reader_t	 *r = (Reader_t *) (rwp->rw_reader->endpoint.endpoint);

	ctrc_printd (RTPS_ID, RTPS_SFR_ALIVE_TO, &user, sizeof (user));
	prof_start (rtps_rr_alive_to);

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_alv_to)
		rtps_marker_notify ((LocalEndpoint_t *) r, EM_ALIVE_TO, "sfr_rel_alive_to");
#endif
	if (!info_reply_rxed ()) {
		lrloc_print ("RTPS: RelAliveTO: ");
		proxy_reset_reply_locators (&rwp->proxy);	/* Reselect path. */
	}
	sfr_rel_init_acknack (rwp);
	RALIVE_TMR_START (rwp, rwp->rw_hbrsp_timer, RALIVE_TO,
			   (uintptr_t) rwp, sfr_rel_alive_to, &r->r_lock);
	prof_stop (rtps_rr_alive_to, 1);
}

#endif

/* sfr_rel_start -- Start a reliable protocol session with a remote writer. */

static void sfr_rel_start (RemWriter_t *rwp)
{
	Reader_t	 *r = (Reader_t *) (rwp->rw_reader->endpoint.endpoint);

	ctrc_printd (RTPS_ID, RTPS_SFR_REL_START, &rwp, sizeof (rwp));
	prof_start (rtps_rr_start);

	RW_SIGNAL (rwp, "REL-Start");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_start)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_START, "sfr_rel_start");
#endif
	NEW_RW_CSTATE (rwp, RWCS_INITIAL, 1);
	LIST_INIT (rwp->rw_changes);
	rwp->rw_changes.nchanges = 0;
	NEW_RW_CSTATE (rwp, RWCS_READY, 0);
	NEW_RW_ASTATE (rwp, RWAS_WAITING, 0);
	rwp->rw_hb_no_data = 0;
	rwp->rw_hbrsp_timer = NULL;

#ifdef RTPS_PROXY_INST
	rwp->rw_loc_inst = ++rtps_rw_insts;
#endif
#ifdef RTPS_INIT_ACKNACK

	/* Send an initial ACKNACK to bootstrap the protocol in case the writer
	   was already active, but we missed the initial HEARTBEAT! */
	rwp->rw_peer_alive = 0;
	sfr_rel_init_acknack (rwp);
	RALIVE_TMR_ALLOC (rwp);
	rwp->rw_hbrsp_timer = tmr_alloc ();
	if (rwp->rw_hbrsp_timer) {
		tmr_init (rwp->rw_hbrsp_timer, "RTPS-RAlive");
		RALIVE_TMR_START (rwp, rwp->rw_hbrsp_timer, RALIVE_TO,
				 (uintptr_t) rwp, sfr_rel_alive_to, &r->r_lock);
	}
#endif
	prof_stop (rtps_rr_start, 1);
}

#ifdef RTPS_TRC_READER

#define DUMP_READER_STATE(rwp)	sfr_dump_rw_state(rwp)

/* sfr_dump_rw_state -- Dump the state of writer proxy. */

static void sfr_dump_rw_state (RemWriter_t *rwp)
{
	CCREF		*rp;

	if (!rwp->rw_reader->endpoint.trace_state)
		return;

	log_printf (RTPS_ID, 0, "RW: SNR_Next=%u.%u;\r\n",
			rwp->rw_seqnr_next.high, rwp->rw_seqnr_next.low);
	if (LIST_NONEMPTY (rwp->rw_changes)) {
		log_printf (RTPS_ID, 0, "RW: Samples:");
		LIST_FOREACH (rwp->rw_changes, rp)
			sfx_dump_ccref (rp, 1);
		log_printf (RTPS_ID, 0, "\r\n");
	}
	else
		log_printf (RTPS_ID, 0, "RW: no samples.\r\n");
}

#else
#define	DUMP_READER_STATE(rwp)
#endif

/* sfr_process_samples -- Changes list of remote writer contains samples with
			  state CS_LOST or CS_RECEIVED at the front.  Remove
			  all samples with this state and move them to the
			  history cache (CS_RECEIVED and relevant != 0) or
			  forget about the sample. */

void sfr_process_samples (RemWriter_t *rwp)
{
	Change_t	*cp;
	CCREF		*rp;
	ChangeState_t	state;
	SequenceNumber_t snr;
	int		error;
	PROF_ITER	(n);

	ctrc_printd (RTPS_ID, RTPS_SFR_PROCESS, &rwp, sizeof (rwp));
	prof_start (rtps_rr_proc);

	/* Remove valid entries from the changes list and if relevant, store
	   them in the history cache. */
	while ((rp = LIST_HEAD (rwp->rw_changes)) != NULL &&
	       ((state = rp->state) == CS_LOST || 
	        (state == CS_RECEIVED
#ifdef RTPS_FRAGMENTS
			&& !rp->fragments
#endif
					 ))) {
		PROF_INC (n);

		snr = (rp->relevant) ? rp->u.c.change->c_seqnr : rp->u.range.first;
		if (!SEQNR_EQ (snr, rwp->rw_seqnr_next))
			break;

		/* Check if we can add the sample to the cache. */
		if (rp->relevant) {
			cp = rp->u.c.change;
			snr = cp->c_seqnr;
			if (state == CS_RECEIVED) {
				if (rwp->rw_reader->endpoint.cache_acks)  {
					rcl_access (rp->u.c.change);
					rp->u.c.change->c_nrefs++;
					rcl_done (rp->u.c.change);
				}
				error = reader_cache_add_inst (rwp->rw_reader, 
						               rp->u.c.change,
							       rp->u.c.hci,
							       1);

				/* If cache is full, try again later. */
				if (error == DDS_RETCODE_NO_DATA) {
					rwp->rw_blocked = 1;
					break;
				}
				else if (rwp->rw_reader->endpoint.cache_acks)
					hc_change_free (rp->u.c.change);
			}
			rwp->rw_reader->data_queued--;
		}
		else
			snr = rp->u.range.last;

		/* Remove change from changes list. */
		LIST_REMOVE (rwp->rw_changes, *rp);
		rwp->rw_changes.nchanges--;

		/* Free the reference. */
		mds_pool_free (&rtps_mem_blocks [MB_CCREF], rp);

		/* Update the sequence number. */
		SEQNR_INC (snr);
		rwp->rw_seqnr_next = snr;

		RW_SNR_TRACE (rwp, "process_samples");
	}
	prof_stop (rtps_rr_proc, n);
	RANGE_CHECK (&rwp->rw_changes, "sfr_process_samples");
}

#ifdef RTPS_FRAGMENTS

/* sfr_fragment -- A DATAFRAG submessage was received. */

static void sfr_fragment (RemWriter_t     *rwp,
			  CCREF           *refp,
			  DataFragSMsg    *fragp,
			  FragInfo_t      **finfo,
			  const KeyHash_t *hp,
			  Change_t        *cp,
			  int             ooo,
			  int             ignore)
{
	Change_t		*xcp, *ncp;
	FragInfo_t		*fip;
	Reader_t		*rp;
	const TypeSupport_t	*ts;
	unsigned		max_frags, size;
	int			all_key, error;
	HCI			hci;
	InstanceHandle		h;
	RejectCause_t		cause;
	DBW			walk;
	size_t			ofs = 0;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	DB			*dbp;
#endif

	/* If this is the first fragment, create the fragments context. */
	fip = refp->fragments;
	if (!fip) {
		if (refp->relevant) {

			/* Replace change with empty one. */
			ncp = hc_change_new ();
			if (!ncp) {
				refp->state = CS_MISSING;
				return;
			}
			xcp = refp->u.c.change;
			ncp->c_kind = xcp->c_kind;
			ncp->c_writer = xcp->c_writer;
			ncp->c_time = xcp->c_time;
			ncp->c_seqnr = xcp->c_seqnr;
			hc_change_free (xcp);
			refp->u.c.change = ncp;
		}
		else {
			refp->state = CS_RECEIVED;
			goto done;
		}
		fip = *finfo;
		if (fip) {
			rcl_access (fip);
			fip->nrefs++;
			rcl_done (fip);
			refp->fragments = fip;
		}
		else {
			max_frags = (fragp->sample_size + fragp->frag_size - 1) / fragp->frag_size;
			if (fragp->frag_start + fragp->num_fragments - 1 > max_frags) {

			    no_frag_mem:

				refp->state = CS_MISSING;
				if (refp->u.c.change)
					hc_change_free (refp->u.c.change);
				return;
			}

			*finfo = fip = rfraginfo_create (refp, fragp, max_frags);
			if (!fip)
				goto no_frag_mem;
		}
	}

	/* If this is the first fragment and we already have a fragmentation
	   context, and the data needs to be ignored, we abort reception. */
	else if (refp->relevant && fragp->frag_start == 1 && ignore) {
		xcp = refp->u.c.change;
		refp->state = CS_RECEIVED;
		refp->relevant = 0;
		refp->u.range.first = refp->u.range.last = xcp->c_seqnr;
		hc_change_free (xcp);
		goto done_clean_fip;
	}

	/* Update key info if present. */
	if (hp) {
		fip->hash = *hp;
		fip->hp = &fip->hash;
	}

	/* Mark the fragment as correctly received. */
	mark_fragment (fip, fragp, cp);

	/* If more fragments pending, simply exit, waiting for the next. */
	if (fip->num_na)
		return;

	/* Data complete! Derive key info and get a new instance if possible. */
	/* If data type indicates multi-instance data, we need the actual keys
	   in order to lookup instances properly. */
	/*log_printf (RTPS_ID, 0, "sfr_fragment: complete!\r\n");*/
	rp = (Reader_t *) rwp->rw_reader->endpoint.endpoint;
	ts = rp->r_topic->type->type_support;
	if (!refp->relevant || !ts->ts_keys) {
		if (!hc_accepts (rp->r_cache, ooo)) {

			/* Don't see this as rejected -- since there is no ack
			   given, last fragment will simply be retransmitted!
			dcps_sample_rejected ((Reader_t *) ep->endpoint,
					      DDS_REJECTED_BY_SAMPLES_LIMIT, 0);*/
			if (!ooo)
				rwp->rw_blocked = 1;

		    ignore_last_fragment:

			fip->num_na = 1;
			fip->first_na = fragp->frag_start + fragp->num_fragments - 2;
			SET_REM (fip->bitmap, fip->first_na);
			return;
		}
		else
			goto no_keys;
	}

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

	/* Decrypt payload data if an encrypted payload is present. */
	if (rwp->rw_crypto &&
	    fip->length) {
		walk.dbp = fip->data;
		walk.data = fip->data->data;
		walk.length = walk.left = fip->length;
		dbp = sec_decode_serialized_data (&walk,
						  0,
						  rwp->rw_crypto,
						  &fip->length,
						  &ofs,
						  (DDS_ReturnCode_t *) &error);
		if (!dbp)
			goto cleanup;

		fip->data = dbp;
	}
#endif

	/* Key somewhere in either marshalled data or marshalled key.
	   Derive key information if not yet done. */
	if (!fip->key) {
		if (fip->length) {
			walk.dbp = fip->data;
			walk.data = fip->data->data + ofs;
			walk.left = walk.length = fip->length;
		}
		all_key = cp->c_kind != ALIVE;
		size = ts->ts_mkeysize;
		if (!size || !ts->ts_fksize) {
			size = DDS_KeySizeFromMarshalled (walk, ts,
							all_key, NULL);
			if (!size)
				goto cleanup;
		}
		fip->keylen = size;
		if (ts->ts_mkeysize &&
		    ts->ts_fksize &&
		    size <= sizeof (KeyHash_t) &&
		    !ENC_DATA (&rp->r_lep)) {
			if (fip->hp) {
				fip->key = fip->hash.hash;
				goto got_keys;
			}
			if (size < sizeof (KeyHash_t))
				memset (fip->hash.hash + size, 0,
						     sizeof (KeyHash_t) - size);
		}
		else {
			fip->key = xmalloc (size);
			if (!fip->key)
				goto ignore_last_fragment;
		}
		error = DDS_KeyFromMarshalled (fip->key, walk, ts, all_key,
							ENC_DATA (&rp->r_lep));
		if (error) {
			if (fip->key != fip->hash.hash)
				xfree (fip->key);

			fip->key = NULL;
			goto cleanup;
		}
		if (!fip->hp) {
			error = DDS_HashFromKey (fip->hash.hash, fip->key, size,
						    ENC_DATA (&rp->r_lep), ts);
			if (error) {
				if (fip->key != fip->hash.hash)
					xfree (fip->key);

				fip->key = NULL;
				goto cleanup;
			}
			fip->hp = &fip->hash;
		}
	}

    got_keys:

	/* Key information is present in the fragment context now.
	   Derive a new or existing cache instance context. */
	hci = hc_lookup_hash (rp->r_cache,
			      fip->hp, fip->key, fip->keylen,
			      &h, 1, ooo, &cause);
	if (!hci) {

		/* Don't see this as rejected -- since there is no ack
		   given, last fragment will simply be retransmitted!
		dcps_sample_rejected ((Reader_t *) ep->endpoint,
				      (DDS_SampleRejectedStatusKind) cause,
				      h);*/
		if (!ooo)
			rwp->rw_blocked = 1;
		goto ignore_last_fragment;
	}
	refp->u.c.hci = hci;
	refp->u.c.change->c_handle = h;
	hc_inst_inform (rp->r_cache, hci);

    no_keys:

	/* Transform to a valid received sample, as if this was a normal DATA
	   submessage. */
	if (refp->relevant) {
		xcp = refp->u.c.change;
		xcp->c_db = fip->data;
		xcp->c_data = fip->data->data + ofs;
		xcp->c_length = fip->length;
		rcl_access (xcp);
		xcp->c_db->nrefs++;
		rcl_done (xcp);
	}

    done_clean_fip:

    	/* Cleanup fragmentation context. */
	if (fip->nrefs == 1)
		*finfo = NULL;
	rfraginfo_delete (refp);

    done:

	/* If received sample is first, add samples to history cache. */
	sfr_process_samples (rwp);
	return;

    cleanup:
	refp->state = CS_MISSING;
	xcp = refp->u.c.change;
	refp->relevant = 0;
	refp->fragments = NULL;
	refp->u.range.first = refp->u.range.last = xcp->c_seqnr;
	hc_change_free (xcp);
	rfraginfo_delete (refp);
}

#endif

/* sfr_rel_data -- A DATA or DATAFRAG submessage was received.  Process its
		   contents. */

static void sfr_rel_data (RemWriter_t         *rwp,
			  Change_t            *cp,
			  SequenceNumber_t    *cpsnr,
			  const KeyHash_t     *hp,
			  const unsigned char *key,
			  size_t              keylen,
#ifdef RTPS_FRAGMENTS
			  DataFragSMsg        *fragp,
			  FragInfo_t	      **finfo,
#endif
			  int                 ignore)
{
	ENDPOINT	*ep = &rwp->rw_reader->endpoint;
	CCREF		*rp, *gap_rp;
	HCI		hci;
	Change_t	*ncp;
	InstanceHandle	h;
	SequenceNumber_t gap_first, gap_last, seqnr_first, seqnr_last;
	RejectCause_t	cause;
	/*unsigned	max;*/
	int		error, ooo;

	ctrc_printd (RTPS_ID, RTPS_SFR_REL_DATA, &rwp, sizeof (rwp));
	prof_start (rtps_rr_data);
#if defined (RTPS_FRAGMENTS) && defined (EXTRA_STATS)
	if (fragp)
		STATS_INC (rwp->rw_ndatafrags);
	else
#endif
		STATS_INC (rwp->rw_ndata);

	RW_SIGNAL (rwp, "REL-Data");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_data)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_DATA, "sfr_rel_data");
#endif
#ifdef RTPS_INIT_ACKNACK

	/* If first Data: accept it and become alive. */
	if (!rwp->rw_peer_alive)
		sfr_rel_alive (rwp);
#endif
	rwp->rw_hb_no_data = 0;

	/* If seqnr already in cache: ignore sample. */
	if (SEQNR_LT (*cpsnr, rwp->rw_seqnr_next)) {
		RW_SIGNAL (rwp, "REL-Data: ignore (SNR < hcache.SNR)");
		return;
	}
	ooo = !SEQNR_EQ (rwp->rw_seqnr_next, *cpsnr);

	/* Create a new instance already, if possible. */
	if (ignore
#ifdef RTPS_FRAGMENTS
	           || fragp
#endif
		           )
		hci = NULL;
	else if (ep->multi_inst) {
		hci = hc_lookup_hash (ep->endpoint->cache, hp, key, keylen,
					  &h, 1, ooo, &cause);
		if (!hci) {
			/* Don't see this as rejected -- since there is no ack
			   given, sample will be retransmitted!
			dcps_sample_rejected ((Reader_t *) ep->endpoint,
					      (DDS_SampleRejectedStatusKind) cause,
					      h);*/
			if (!ooo)
				rwp->rw_blocked = 1;
			return;
		}
	}
	else {
		if (!hc_accepts (ep->endpoint->cache, ooo)) {
			/* Don't see this as rejected -- since there is no ack
			   given, sample will be retransmitted!
			dcps_sample_rejected ((Reader_t *) ep->endpoint,
					      DDS_REJECTED_BY_SAMPLES_LIMIT, 0);*/
			if (!ooo)
				rwp->rw_blocked = 1;
			return;
		}
		h = 0;
		hci = NULL;
	}

	/* Add to changes list of proxy writer context. */
	if (LIST_NONEMPTY (rwp->rw_changes)) { /* Already some samples queued. */

		RW_SIGNAL (rwp, "REL-Data: changes-queued");

		if (hci)
			hc_inst_inform (ep->endpoint->cache, hci);

		if (rwp->rw_changes.head->relevant)
			seqnr_first = rwp->rw_changes.head->u.c.change->c_seqnr;
		else
			seqnr_first = rwp->rw_changes.head->u.range.first;
		if (rwp->rw_changes.tail->relevant)
			seqnr_last = rwp->rw_changes.tail->u.c.change->c_seqnr;
		else
			seqnr_last = rwp->rw_changes.tail->u.range.last;

		if (SEQNR_GT (*cpsnr, seqnr_last)) { /* Seqnr > last in list! */

			/* Add new data sample node after tail of list. */
			RW_SIGNAL (rwp, "REL-Data: add-missing>last");
			gap_first = seqnr_last;
			SEQNR_INC (gap_first);
			if (!SEQNR_EQ (gap_first, *cpsnr)) {

				/* Gap between tail and data: add MISSING sample
				   node between current tail of list and to be
				   added data sample node. */
				gap_last = *cpsnr;
				SEQNR_DEC (gap_last);
				if (!rwp->rw_changes.tail->relevant &&
				    rwp->rw_changes.tail->state == CS_MISSING) {

					/* Extend previous gap node. */
					rwp->rw_changes.tail->u.range.last = gap_last;
					RW_SIGNAL (rwp, "REL-Data: extend-gap");
				}
				else {	/* Add new gap node. */
					gap_rp = ccref_add_gap (&rwp->rw_changes,
							        &gap_first,
							        &gap_last,
							        1,
								CS_MISSING);
					if (!gap_rp)
						return;

					RW_SIGNAL (rwp, "REL-Data: add-gap");
				}
			}

			/* Append data sample node. */
			if (ignore)
				rp = ccref_add_gap (&rwp->rw_changes, cpsnr,
						    cpsnr, 1, CS_RECEIVED);
			else
				rp = ccref_add_received (&rwp->rw_changes, cp,
							 cpsnr, hci, h, 1);
			if (!rp)
				return;

			rwp->rw_reader->data_queued++;
#ifdef RTPS_FRAGMENTS
			if (fragp) {
				sfr_fragment (rwp, rp, fragp, finfo, hp, cp, ooo, ignore);
				return;
			}
#endif
			RANGE_CHECK (&rwp->rw_changes, "REL-Data: >last");
		}
		else if (SEQNR_LT (*cpsnr, seqnr_first)) { /* Seqnr < first! */

			/* Add new data sample node before head of list. */
			RW_SIGNAL (rwp, "REL-Data: add-missing<first");
			gap_last = seqnr_first;
			SEQNR_DEC (gap_last);
			if (!SEQNR_EQ (gap_last, *cpsnr)) {

				/* Gap between data and head: add MISSING sample
				   node between to be added data sample and
				   current head of list. */
				gap_first = *cpsnr;
				SEQNR_INC (gap_first);
				if (!rwp->rw_changes.head->relevant &&
				    rwp->rw_changes.tail->state == CS_MISSING) {

					/* Extended following gap node. */
					rwp->rw_changes.head->u.range.first = gap_first;
					RW_SIGNAL (rwp, "REL-Data: extend-gap");
				}
				else {	/* Add new gap node. */
					gap_rp = ccref_add_gap (&rwp->rw_changes,
							        &gap_first,
							        &gap_last,
							        0,
								CS_MISSING);
					if (!gap_rp)
						return;

					RW_SIGNAL (rwp, "REL-Data: add-gap");
				}
			}

			/* Prepend data sample node. */
			if (ignore)
				rp = ccref_add_gap (&rwp->rw_changes, cpsnr,
							 cpsnr, 0, CS_RECEIVED);
			else
				rp = ccref_add_received (&rwp->rw_changes, cp,
							 cpsnr, hci, h, 0);
			if (!rp)
				return;

			rwp->rw_reader->data_queued++;
#ifdef RTPS_FRAGMENTS
			if (fragp) {
				sfr_fragment (rwp, rp, fragp, finfo, hp, cp, ooo, ignore);
				return;
			}
#endif
			RANGE_CHECK (&rwp->rw_changes, "REL-Data: <first");
		}
		else {	/* Seqnr somewhere in list - lets find it. */
			LIST_FOREACH (rwp->rw_changes, rp)
				if (rp->relevant) {
					if (SEQNR_EQ (rp->u.c.change->c_seqnr, *cpsnr))
						break;
				}
				else if (!SEQNR_LT (*cpsnr, rp->u.range.first) &&
					 !SEQNR_GT (*cpsnr, rp->u.range.last))
					break;

			if (LIST_END (rwp->rw_changes, rp) ||
			    (rp->state != CS_MISSING 
#ifdef RTPS_FRAGMENTS
			       && !rp->fragments
#endif
			       			))
				return;
#ifdef RTPS_FRAGMENTS
			if (rp->fragments) {
				sfr_fragment (rwp, rp, fragp, finfo, hp, cp, ooo, ignore);
				return;
			}
#endif
			RW_SIGNAL (rwp, "REL-Data: in-range");
			RANGE_CHECK (&rwp->rw_changes, "REL-Data: in-range");
			if (SEQNR_GT (*cpsnr, rp->u.range.first)) {

				/* Prepend gap node: range.first .. *cpsnr -1 */
				gap_first = rp->u.range.first;
				gap_last = *cpsnr;
				SEQNR_DEC (gap_last);
				gap_rp = ccref_insert_gap (&rwp->rw_changes,
						           rp->prev,
						           &gap_first,
						           &gap_last,
							   CS_MISSING);
				if (!gap_rp)
					return;

				RW_SIGNAL (rwp, "REL-Data: prepend-gap");
			}
			if (SEQNR_LT (*cpsnr, rp->u.range.last)) {

				/* Append gap node: *cpsnr + 1 .. range.last */
				gap_first = *cpsnr;
				SEQNR_INC (gap_first);
				gap_last = rp->u.range.last;
				gap_rp = ccref_insert_gap (&rwp->rw_changes,
						           rp,
						           &gap_first,
						           &gap_last,
							   CS_MISSING);
				if (!gap_rp)
					return;

				RW_SIGNAL (rwp, "REL-Data: append-gap");
			}

			/* Reuse gap node for data. */
#ifdef RTPS_FRAGMENTS
			rp->fragments = NULL;
#endif
			if (ignore) {
				rp->relevant = 0;
				rp->u.range.first = *cpsnr;
				rp->u.range.last = *cpsnr;
			}
			else {
				if (cp->c_nrefs > 1) {
					ncp = hc_change_clone (cp);
					if (!ncp) {
						warn_printf ("sfr_rel_data (): out of memory for change clone!\r\n");
						return;
					}
				}
				else {
					rcl_access (cp);
					cp->c_nrefs++;
					rcl_done (cp);
					ncp = cp;
				}
				ncp->c_handle = h;
				ncp->c_seqnr = *cpsnr;
				rp->relevant = 1;
				rp->u.c.hci = hci;
				rp->u.c.change = ncp;
				rwp->rw_reader->data_queued++;
			}
			rp->state = CS_RECEIVED;
#ifdef RTPS_FRAGMENTS
			if (fragp) {
				sfr_fragment (rwp, rp, fragp, finfo, hp, cp, ooo, ignore);
				return;
			}
#endif
			RW_SIGNAL (rwp, "REL-Data: missing::received");
			RANGE_CHECK (&rwp->rw_changes, "REL-Data: missing::received");
		}

		/* If the received data sample caused some samples to be valid,
		   add these samples to the history cache and remove them from
		   the changes list. */
		rp = LIST_HEAD (rwp->rw_changes);
		if (rp &&
		    (rp->state == CS_RECEIVED || rp->state == CS_LOST) &&
#ifdef RTPS_FRAGMENTS
		    !rp->fragments &&
#endif
		    rwp->rw_heartbeats) {
			RW_SIGNAL (rwp, "REL-Data: process-samples");
			RANGE_CHECK (&rwp->rw_changes, "REL-Data: process-samples");
			sfr_process_samples (rwp);
		}
	}

	/* Check if equal to next expected sequence number. */
	else if (!ooo
#ifdef RTPS_FRAGMENTS
		 && !fragp
#endif
		          ) {

		/* Add sample immediately to history cache -- no need to store
		   in the samples list since there are no unknown preceeding
		   samples. */
		RW_SIGNAL (rwp, "REL-Data: add-to-cache (SNR == hcache.SNR)");
		if (!ignore) {
			if (cp->c_nrefs > 1) {
				ncp = hc_change_clone (cp);
				if (!ncp) {
					warn_printf ("sfr_rel_data (): out of memory for change clone!\r\n");
					return;
				}
			}
			else {
				rcl_access (cp);
				cp->c_nrefs++;
				rcl_done (cp);
				ncp = cp;
			}
			ncp->c_handle = h;
			error = reader_cache_add_inst (rwp->rw_reader, ncp, hci, 1);
			if (error == DDS_RETCODE_NO_DATA)
				goto add_first_node;
		}
		SEQNR_INC (rwp->rw_seqnr_next);
		RW_SNR_TRACE (rwp, "REL-Data");
	}

	/* No sample nodes yet: add as first node. */
	else {

	    add_first_node:

		if (hci)
			hc_inst_inform (ep->endpoint->cache, hci);

		if (ignore)
			rp = ccref_add_gap (&rwp->rw_changes, cpsnr,
							 cpsnr, 1, CS_RECEIVED);
		else
			rp = ccref_add_received (&rwp->rw_changes, cp, cpsnr,
								     hci, h, 1);
		if (!rp) {
			warn_printf ("sfr_rel_data (): out of memory for sample node!\r\n");
			return;
		}
		rwp->rw_reader->data_queued++;
		RW_SIGNAL (rwp, "REL-Data: first node");
		RANGE_CHECK (&rwp->rw_changes, "REL-Data: first node");
#ifdef RTPS_FRAGMENTS
		if (fragp && !ignore) {
			sfr_fragment (rwp, rp, fragp, finfo, hp, cp, ooo, ignore);
			return;
		}
#endif
	}

	prof_stop (rtps_rr_data, 1);
	DUMP_READER_STATE (rwp);
}

/* sfr_rel_gap_add -- Add a received gap to the changes list. */

static CCREF *sfr_rel_gap_add (RemWriter_t      *rwp,
			       SequenceNumber_t *first,
			       SequenceNumber_t *last,
			       ChangeState_t	state,
			       int              tail)
{
	CCREF	*rp;

	rp = ccref_add_gap (&rwp->rw_changes, first, last, tail, state);
	if (!rp)
		return (NULL);

	return (rp);
}

/* sfr_rel_gap_set -- Update a gap subrange in an existing gap as received. */

static void sfr_rel_gap_set (RemWriter_t      *rwp,
			     CCREF            *rp,
			     SequenceNumber_t *first,
			     SequenceNumber_t *last)
{
	CCREF		*gap_rp, *tail_rp;
	SequenceNumber_t range_last, tail_first;
	ChangeState_t	 rstate;

	rstate = rp->state;
	range_last = rp->u.range.last;
	if (SEQNR_GT (*first, rp->u.range.first)) {
		gap_rp = ccref_insert_gap (&rwp->rw_changes,
				           rp,
				           first,
				           last,
					   CS_RECEIVED);
		if (!gap_rp)
			return;

		rp->u.range.last = *first;
		SEQNR_DEC (rp->u.range.last);
	}
	else {
		gap_rp = rp;
		gap_rp->u.range.last = *last;
		gap_rp->state = CS_RECEIVED;
	}
	if (SEQNR_LT (*last, range_last)) {
		tail_first = *last;
		SEQNR_INC (tail_first);
		tail_rp = ccref_insert_gap (&rwp->rw_changes,
					    gap_rp,
					    &tail_first,
					    &range_last,
					    rstate);
		if (!tail_rp)
			return;
	}
}

/* sfr_rel_gap_range -- Add an irrelevant range to the list of changes. */

static void sfr_rel_gap_range (RemWriter_t      *rwp,
			       SequenceNumber_t *first,
			       SequenceNumber_t *last)
{
	SequenceNumber_t seqnr_first, seqnr_last, s1, s2;
	CCREF		*rp;

	if (SEQNR_LT (*last, rwp->rw_seqnr_next))
		return;

	if (SEQNR_LT (*first, rwp->rw_seqnr_next))
		*first = rwp->rw_seqnr_next;

	if (LIST_NONEMPTY (rwp->rw_changes)) { /* Already some samples queued. */

		RW_SIGNAL (rwp, "REL-Gap: changes-queued");

		if (rwp->rw_changes.head->relevant)
			seqnr_first = rwp->rw_changes.head->u.c.change->c_seqnr;
		else
			seqnr_first = rwp->rw_changes.head->u.range.first;
		if (rwp->rw_changes.tail->relevant)
			seqnr_last = rwp->rw_changes.tail->u.c.change->c_seqnr;
		else
			seqnr_last = rwp->rw_changes.tail->u.range.last;

		if (SEQNR_GT (*first, seqnr_last)) { /* Seqnr > last in list! */
			s1 = seqnr_last;
			SEQNR_INC (s1);
			if (SEQNR_GT (*first, s1)) {
				s2 = *first;
				SEQNR_DEC (s2);
				rp = sfr_rel_gap_add (rwp, &s1, &s2, CS_MISSING, 1);
				if (!rp)
					return;

				RW_SIGNAL (rwp, "REL-Gap: add missing gap after");
			}
			sfr_rel_gap_add (rwp, first, last, CS_RECEIVED, 1);
			RANGE_CHECK (&rwp->rw_changes, "REL-Gap: add after");
		}
		else if (SEQNR_LT (*last, seqnr_first)) { /* Seqnr < first! */
			s2 = seqnr_first;
			SEQNR_DEC (s2);
			if (SEQNR_LT (*last, s2)) {
				s1 = *last;
				SEQNR_INC (s1);
				rp = sfr_rel_gap_add (rwp, &s1, &s2, CS_MISSING, 0);
				if (!rp)
					return;

				RW_SIGNAL (rwp, "REL-Gap: add missing gap before");
			}
			sfr_rel_gap_add (rwp, first, last, CS_RECEIVED, 0);
			RANGE_CHECK (&rwp->rw_changes, "REL-Gap: add before");
		}
		else {	/* Range overlaps with existing changes. */
			LIST_FOREACH (rwp->rw_changes, rp) {
				if (rp->relevant) {

					/* Ignore this sequence number since
					   it was already received! */
					if (!SEQNR_GT (*first, rp->u.c.change->c_seqnr))
						SEQNR_INC (*first);
				}
				else {
					if (SEQNR_GT (*first, rp->u.range.last))
						continue;

					RW_SIGNAL (rwp, "REL-Gap: gap overlap");
					s1 = *first;
					if (SEQNR_GT (*last, rp->u.range.last))
						s2 = rp->u.range.last;
					else
						s2 = *last;
					if (rp->state != CS_RECEIVED)
						sfr_rel_gap_set (rwp, rp, &s1, &s2);
					*first = s2;
					SEQNR_INC (*first);
				}
				if (SEQNR_GT (*first, *last))
					break;
			}
			RANGE_CHECK (&rwp->rw_changes, "REL-Gap: overlap");
		}
	}
	else {	/* Add gap as only range. */
		RW_SIGNAL (rwp, "REL-Gap: set missing gap");
		sfr_rel_gap_add (rwp, first, last, CS_RECEIVED, 1);
		RANGE_CHECK (&rwp->rw_changes, "REL-Gap: set missing");
	}
	DUMP_READER_STATE (rwp);
}

/* sfr_rel_gap -- A GAP submessage was received.  Process its contents. */

static void sfr_rel_gap (RemWriter_t *rwp, GapSMsg *gp)
{
	SequenceNumber_t snr, snr_first, snr_last;
	CCREF		*rp;
	unsigned	i, n;

	ctrc_printd (RTPS_ID, RTPS_SFR_REL_GAP, &rwp, sizeof (rwp));
	prof_start (rtps_rr_gap);
	STATS_INC (rwp->rw_ngap);

	RW_SIGNAL (rwp, "REL-Gap");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_gap)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_GAP, "sfr_rel_gap");
#endif
#ifdef RTPS_INIT_ACKNACK

	/* If first Gap: accept it and become alive. */
	if (!rwp->rw_peer_alive)
		sfr_rel_alive (rwp);
#endif
	rwp->rw_hb_no_data = 0;

	/* Set all sequence numbers from gap_start to gap_list.base - 1 to
	   IRRELEVANT. */
	if (SEQNR_LT (gp->gap_start, gp->gap_list.base)) {
		snr_last = gp->gap_list.base;
		SEQNR_DEC (snr_last);
		sfr_rel_gap_range (rwp, &gp->gap_start, &snr_last);
	}

	/* Set all sequence numbers in gap_list to IRRELEVANT. */
	n = 0;
	for (snr = gp->gap_list.base, i = 0; i < gp->gap_list.numbits; i++) {
		if (SET_CONTAINS (gp->gap_list.bitmap, i)) {
			if (!n++)
				snr_first = snr;
			snr_last = snr;
		}
		else if (n) {
			sfr_rel_gap_range (rwp, &snr_first, &snr_last);
			DUMP_READER_STATE (rwp);
			n = 0;
		}
		SEQNR_INC (snr);
	}
	if (n)
		sfr_rel_gap_range (rwp, &snr_first, &snr_last);

	/* If the received gap info caused some samples to be valid, add these
	   samples to the history cache and remove them from the changes list.*/
	if ((rp = LIST_HEAD (rwp->rw_changes)) != NULL &&
	    (rp->state == CS_RECEIVED || rp->state == CS_LOST))
		sfr_process_samples (rwp);

	prof_stop (rtps_rr_gap, 1);
	RANGE_CHECK (&rwp->rw_changes, "REL-Gap: process samples");
	DUMP_READER_STATE (rwp);
}

/* sfr_rel_do_ack_nl -- Called when the HEARTBEAT-Response timer has elapsed to
		        send an ACKNACK message. */

static void sfr_rel_do_ack_nl (RemWriter_t *rwp)
{
	CCREF		*rp;
	SequenceNumber_t snr;
	unsigned	n;
	uint32_t	bitmaps [8];

	n = 0;

	/* Indicate which samples are received but not yet cached in the
	   ACKNACK bitmap as zeroes. */
	bitmaps [0] = 0;
	LIST_FOREACH (rwp->rw_changes, rp) {
		if (rp->relevant) {
			if (rp->state == CS_RECEIVED) {
				if (rwp->rw_reader->endpoint.cache_acks
#ifdef RTPS_FRAGMENTS
				        || rp->fragments
#endif
				                        )
					break;
			}
			if (rwp->rw_blocked)
				break;

			if (rp->state == CS_MISSING)
				SET_ADD (bitmaps, n);
			n++;
			if (n >= 8 * 32)	/* Max. set size reached! */
				break;

			if ((n & 0x1f) == 0)
				bitmaps [n >> 5] = 0;
		}
		else {
			for (snr = rp->u.range.first;
			     !SEQNR_GT (snr, rp->u.range.last); ) {
				if (rp->state == CS_RECEIVED && rwp->rw_reader->endpoint.cache_acks) 
					break;

				if ((rwp->rw_blocked) && (!SEQNR_EQ(rwp->rw_seqnr_next, snr)))
					break;

				if (rp->state == CS_MISSING)
					SET_ADD (bitmaps, n);
				SEQNR_INC (snr);
				n++;
				if (n >= 8 * 32)	/* Max. set size reached! */
					break;

				if ((n & 0x1f) == 0)
					bitmaps [n >> 5] = 0;
			}
			if (n >= 8 * 32)
				break;
		}
	}

#ifndef RTPS_EMPTY_ACKNACK
	if (!n) {
		bitmaps [0] = 0;
		n = 1;
		SET_ADD (bitmaps, 0);
	}
#endif
	rtps_msg_add_acknack (rwp,
			      (DiscoveredWriter_t *) rwp->rw_endpoint,
			      0,
			      &rwp->rw_seqnr_next,
			      n,
			      bitmaps);
	RANGE_CHECK (&rwp->rw_changes, "do_ack: add_acknack");
}

/* sfr_rel_do_ack -- Called when the HEARTBEAT-Response timer has elapsed to
		     send an ACKNACK message. */

static void sfr_rel_do_ack (uintptr_t user)
{
	RemWriter_t	*rwp = (RemWriter_t *) user;

	ctrc_printd (RTPS_ID, RTPS_SFR_REL_DO_ACK, &user, sizeof (user));
	prof_start (rtps_rr_do_ack);

	if (rwp->rw_reader->heartbeat_resp_delay) {
		HBRSP_TMR_TO (rwp);
		if (rwp->rw_hbrsp_timer) {
			HBRSP_TMR_FREE (rwp);
			tmr_free (rwp->rw_hbrsp_timer);
			rwp->rw_hbrsp_timer = NULL;
		}
	}
	sfr_rel_do_ack_nl (rwp);
	prof_stop (rtps_rr_do_ack, 1);
}

#ifdef RTPS_PROXY_INST

/* sfr_rel_reset -- Reset a reliable writer proxy due to the writer being
		    restarted. */

static void sfr_rel_reset (RemWriter_t *rwp)
{
#ifdef RTPS_INIT_ACKNACK
	Reader_t	 *r = (Reader_t *) (rwp->rw_reader->endpoint.endpoint);
#endif

	/* Cleanup resources. */
	if (LIST_NONEMPTY (rwp->rw_changes))
		ccref_list_delete (&rwp->rw_changes);
	
	NEW_RW_CSTATE (rwp, RWCS_INITIAL, 1);
	LIST_INIT (rwp->rw_changes);
	rwp->rw_changes.nchanges = 0;
	NEW_RW_CSTATE (rwp, RWCS_READY, 0);
	NEW_RW_ASTATE (rwp, RWAS_WAITING, 0);
	rwp->rw_hb_no_data = 0;
	memset (&rwp->rw_seqnr_next, 0, sizeof (SequenceNumber_t));

#ifdef RTPS_INIT_ACKNACK
	rwp->rw_peer_alive = 0;
	sfr_rel_init_acknack (rwp);
	if (!rwp->rw_hbrsp_timer) {
		RALIVE_TMR_ALLOC (rwp);
		rwp->rw_hbrsp_timer = tmr_alloc ();
		if (rwp->rw_hbrsp_timer)
			tmr_init (rwp->rw_hbrsp_timer, "RTPS-RAlive");
	}
	if (rwp->rw_hbrsp_timer)
		RALIVE_TMR_START (rwp, rwp->rw_hbrsp_timer, RALIVE_TO,
				 (uintptr_t) rwp, sfr_rel_alive_to, &r->r_lock);
#else
	if (rwp->rw_hbrsp_timer)
		HBRSP_TMR_STOP (rwp, rwp->rw_hbrsp_timer);
#endif
}

/* sfr_restart -- Restart a stateful reliable remote writer proxy . */

void sfr_restart (RemWriter_t *rwp)
{
	rwp->rw_loc_inst = ++rtps_rw_insts;
	sfr_rel_reset (rwp);
}

#endif

/* sfr_rel_heartbeat -- A HEARTBEAT submessage was received.  Process its
			contents. */

static void sfr_rel_heartbeat (RemWriter_t   *rwp,
			       HeartbeatSMsg *hp,
			       unsigned      flags)
{
	CCREF		*rp, *gap_rp;
	SequenceNumber_t snr, seqnr_first, seqnr_last;
	unsigned	nlost = 0;
	int		send_ack = 0;
	int		wakeup = 0;
	Reader_t	*r = (Reader_t *) (rwp->rw_reader->endpoint.endpoint);

	ctrc_printd (RTPS_ID, RTPS_SFR_REL_HBEAT, &rwp, sizeof (rwp));
	prof_start (rtps_rr_hbeat);
	STATS_INC (rwp->rw_nheartbeat);

	RW_SIGNAL (rwp, "REL-Heartbeat");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_hb)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_HEARTBEAT, "sfr_rel_heartbeat");
#endif
#ifdef RTPS_PROXY_INST_TX
	if (hp->instance_id) {
		if (rwp->rw_rem_inst && rwp->rw_rem_inst != hp->instance_id) {

			/* Remote writer has restarted! Reset the proxy. */			
			RW_SIGNAL (rwp, "REL-Peer-restart!");
			sfr_rel_reset (rwp);
		}
		rwp->rw_rem_inst = hp->instance_id;
	}
#endif
#ifdef RTPS_INIT_ACKNACK

	/* If first Heartbeat: accept it and become alive. */
	if (!rwp->rw_peer_alive) {
		sfr_rel_alive (rwp);
		wakeup = 1;
	}
#endif
	if ((flags & SMF_LIVELINESS) != 0) {
		hc_alive (rwp->rw_reader->endpoint.endpoint->cache);
		if ((flags & SMF_FINAL) != 0)
			return;
	}

	/* Ignore duplicate heartbeats. */
	if (hp->count == rwp->rw_last_hb)
		return;

	rwp->rw_last_hb = hp->count;

	/* Increment Heartbeat without Data/Gap counter.
	   If threshold reached, reselect transport. */
	if (rwp->rw_hb_no_data < 3)
		rwp->rw_hb_no_data++;
	else if (!info_reply_rxed ()) {
		lrloc_print ("RTPS: HBwoData: ");
		proxy_reset_reply_locators (&rwp->proxy);	/* Reselect path. */
	}

	/* Update state based on flags. */
	if ((flags & SMF_FINAL) == 0) {
		NEW_RW_ASTATE (rwp, RWAS_MUST_ACK, 0);
		send_ack = 1;
		if (!wakeup && rwp->rw_reader->heartbeat_resp_delay) {
			if (!rwp->rw_hbrsp_timer) {
				HBRSP_TMR_ALLOC (rwp);
				rwp->rw_hbrsp_timer = tmr_alloc ();
				if (!rwp->rw_hbrsp_timer) {
					warn_printf ("sfr_rel_heartbeat: out of memory for Heartbeat Response Timer!");
				}
				else
					tmr_init (rwp->rw_hbrsp_timer, "RTPS-HBRsp");
			}
			if (rwp->rw_hbrsp_timer) {
				send_ack = 0;
				HBRSP_TMR_START (rwp,
						 rwp->rw_hbrsp_timer,
						 rwp->rw_reader->heartbeat_resp_delay,
						 (uintptr_t) rwp,
						 sfr_rel_do_ack,
						 &r->r_lock);
			}
		}
	}
	else {
		send_ack = 0;
		if ((flags & SMF_LIVELINESS) == 0)
			NEW_RW_ASTATE (rwp, RWAS_MAY_ACK, 0);
	}

	DUMP_READER_STATE (rwp);
	RANGE_CHECK (&rwp->rw_changes, "rel_heartbeat-0");

	/* Set all samples with sequence number < first sequence number to
	   LOST status if the status was MISSING. */
	rwp->rw_heartbeats = 1;
	if (LIST_NONEMPTY (rwp->rw_changes)) {
		rp = rwp->rw_changes.head;
                if (rp) {
		        if (rp->relevant)
			        snr = rp->u.c.change->c_seqnr;
		        else
			        snr = rp->u.range.first;
		        seqnr_first = snr;
                }
		while (rp && SEQNR_LT (snr, hp->first_sn)) {
			if (rp->relevant) {
				if (rp->state != CS_RECEIVED
#ifdef RTPS_FRAGMENTS
				    || rp->fragments
#endif
				                            ) {
					rp->state = CS_LOST;
#ifdef RTPS_FRAGMENTS
					if (rp->fragments)
						rfraginfo_delete (rp);
#endif
					nlost++;
				}
			}
			else {
				if (!SEQNR_LT (rp->u.range.last, hp->first_sn)) {

					/* Split gap range in two since there is
					   an overlap! */
					snr = rp->u.range.last;
					rp->u.range.last = hp->first_sn;
					SEQNR_DEC (rp->u.range.last);
					ccref_insert_gap (&rwp->rw_changes,
							  rp, &hp->first_sn,
							  &snr, rp->state);
				}
				if (rp->state != CS_RECEIVED)
					rp->state = CS_LOST;

				snr = rp->u.range.last;
			}
			SEQNR_INC (snr);
			rp = LIST_NEXT (rwp->rw_changes, *rp);
		}
		if (nlost)
			dcps_samples_lost ((Reader_t *) rwp->rw_reader->endpoint.endpoint,
					   nlost);
		if (!SEQNR_GT (seqnr_first, hp->first_sn) &&
		    SEQNR_LT (rwp->rw_seqnr_next, hp->first_sn)) {
			RW_SIGNAL (rwp, "REL-Heartbeat: adjust seqnr_next (1)");
			rwp->rw_seqnr_next = seqnr_first;
			RW_SNR_TRACE (rwp, "REL-Heartbeat: adjust seqnr_next (1)");
			sfr_process_samples (rwp);
		}
	}
	if (SEQNR_LT (rwp->rw_seqnr_next, hp->first_sn)) {
		RW_SIGNAL (rwp, "REL-Heartbeat: adjust seqnr_next (2)");
		dcps_samples_lost ((Reader_t *) rwp->rw_reader->endpoint.endpoint,
				   SEQNR_DELTA (rwp->rw_seqnr_next, hp->first_sn));
		rwp->rw_seqnr_next = hp->first_sn;
		RW_SNR_TRACE (rwp, "REL-Heartbeat: adjust seqnr_next (2)");
	}

	if (SEQNR_LT (hp->first_sn, rwp->rw_seqnr_next)) {
		RW_SIGNAL (rwp, "REL-Heartbeat: adjust hp->first_sn");
		hp->first_sn = rwp->rw_seqnr_next;
	}
	DUMP_READER_STATE (rwp);
	RANGE_CHECK (&rwp->rw_changes, "rel_heartbeat-1");

        /* If Last seq. number < last seq. number of reader cache: all acked. */
	if (LIST_NONEMPTY (rwp->rw_changes) && (!SEQNR_LT (hp->last_sn, rwp->rw_seqnr_next))) {
		if (rwp->rw_changes.head->relevant)
			seqnr_first = rwp->rw_changes.head->u.c.change->c_seqnr;
		else
			seqnr_first = rwp->rw_changes.head->u.range.first;
		if (rwp->rw_changes.tail->relevant)
			seqnr_last = rwp->rw_changes.tail->u.c.change->c_seqnr;
		else
			seqnr_last = rwp->rw_changes.tail->u.range.last;
		if (SEQNR_LT (hp->first_sn, seqnr_first)) {
			RW_SIGNAL (rwp, "REL-Heartbeat: add gap before");
			if (!rwp->rw_changes.head->relevant &&
			    rwp->rw_changes.head->state == CS_MISSING)
				rwp->rw_changes.head->u.range.first = hp->first_sn;
			else {
				SEQNR_DEC (seqnr_first);
				gap_rp = ccref_add_gap (&rwp->rw_changes,
					        &hp->first_sn, &seqnr_first, 0, CS_MISSING);
				if (!gap_rp)
					return;

			}
			RANGE_CHECK (&rwp->rw_changes, "rel_heartbeat-2");
		}
		if (SEQNR_GT (hp->last_sn, seqnr_last)) {
			RW_SIGNAL (rwp, "REL-Heartbeat: add gap after");
			if (!rwp->rw_changes.tail->relevant &&
			    rwp->rw_changes.tail->state == CS_MISSING)
				rwp->rw_changes.tail->u.range.last = hp->last_sn;
			else {
				SEQNR_INC (seqnr_last);
				gap_rp = ccref_add_gap (&rwp->rw_changes,
					        &seqnr_last, &hp->last_sn, 1, CS_MISSING);
				if (!gap_rp)
					return;
			}
			RANGE_CHECK (&rwp->rw_changes, "rel_heartbeat-3");
		}
	}
	else if (!SEQNR_LT (hp->last_sn, rwp->rw_seqnr_next)) {
		RW_SIGNAL (rwp, "REL-Heartbeat: is gap");
		gap_rp = ccref_add_gap (&rwp->rw_changes,
				        &hp->first_sn,
				        &hp->last_sn,
				        1,
					CS_MISSING);
		if (!gap_rp)
			return;
	}

	DUMP_READER_STATE (rwp);
	RANGE_CHECK (&rwp->rw_changes, "rel_heartbeat-4");

	/* Update the state if there are no missing samples. */
	if (rwp->rw_astate == RWAS_MAY_ACK) {
		RW_SIGNAL (rwp, "REL-Heartbeat: some missing?");
		if (LIST_NONEMPTY (rwp->rw_changes)) { /* Some items still missing! */
			NEW_RW_ASTATE (rwp, RWAS_MUST_ACK, 0);
			send_ack = 1;
			if (rwp->rw_reader->heartbeat_resp_delay) {
				if (!rwp->rw_hbrsp_timer) {
					HBRSP_TMR_ALLOC (rwp);
					rwp->rw_hbrsp_timer = tmr_alloc ();
					if (!rwp->rw_hbrsp_timer)
						warn_printf ("sfr_rel_heartbeat: out of memory for Heartbeat Response Timer!");
					else
						tmr_init (rwp->rw_hbrsp_timer, "RTPS-HBRsp");
				}
				if (rwp->rw_hbrsp_timer) {
					HBRSP_TMR_START (rwp,
							 rwp->rw_hbrsp_timer,
							 rwp->rw_reader->heartbeat_resp_delay,
							 (uintptr_t) rwp,
							 sfr_rel_do_ack,
							 &r->r_lock);
					send_ack = 0;
				}
			}
		}
		else
			NEW_RW_ASTATE (rwp, RWAS_WAITING, 1);
	}
	if (send_ack)
		sfr_rel_do_ack_nl (rwp);

	prof_stop (rtps_rr_hbeat, 1);
}

#ifdef RTPS_FRAGMENTS

static void sfr_rel_hbfrag (RemWriter_t *rwp, HeartbeatFragSMsg *hp)
{
	CCREF			*rp;
	FragInfo_t		*fip;
	SequenceNumber_t	snr;
	unsigned		n, i, ofs, last;
	int			missing;
	uint32_t		bitmaps [8];

	ctrc_printd (RTPS_ID, RTPS_SFR_REL_HBFRAG, &rwp, sizeof (rwp));
	STATS_INC (rwp->rw_nheartbeatfrags);

	RW_SIGNAL (rwp, "REL_HeartbeatFrag");

	/* Ignore duplicate heartbeats. */
	if (hp->count == rwp->rw_last_hbfrag)
		return;

	rwp->rw_last_hbfrag = hp->count;
	if (SEQNR_LT (hp->writer_sn, rwp->rw_seqnr_next))
		return;

	/* Find sample with given sequence number. */
	else if (LIST_NONEMPTY (rwp->rw_changes)) {
		rp = rwp->rw_changes.head;
		if (rp->relevant)
			snr = rp->u.c.change->c_seqnr;
		else
			snr = rp->u.range.first;
		while (rp && SEQNR_LT (snr, hp->writer_sn)) {
			if (!rp->relevant) {
				snr = rp->u.range.last;
				if (!SEQNR_LT (snr, hp->writer_sn))
					break;
			}
			SEQNR_INC (snr);
			rp = LIST_NEXT (rwp->rw_changes, *rp);
		}
		if (!rp ||
		    !rp->relevant ||
		    rp->state != CS_RECEIVED ||
		    !rp->fragments)
			return;
	}
	else
		return;

	/* Indicate which fragments are still missing in the NACKFRAG bitmap
	   as bits that are set. */
	fip = rp->fragments;
	missing = 0;
	n = ofs = last = 0;
	for (i = fip->first_na; i < fip->total; i++) {
		if (!SET_CONTAINS (fip->bitmap, i)) {
			if (!missing) {
				missing = 1;
				ofs = i;
				bitmaps [0] = 0;
				n = 0;
			}
			SET_ADD (bitmaps, n);
			last = n;
		}
		if (missing) {
			n++;
			if (n >= 8 * 32) {	/* Max. set size reached! */
				rtps_msg_add_nack_frag (rwp,
						        (DiscoveredWriter_t *) rwp->rw_endpoint,
			        			&rp->u.c.change->c_seqnr,
						        ofs,
					        	last,
						        bitmaps);
				missing = 0;
			}
			else if ((n & 0x1f) == 0)
				bitmaps [n >> 5] = 0;
		}
	}
	if (missing)
		rtps_msg_add_nack_frag (rwp,
				        (DiscoveredWriter_t *) rwp->rw_endpoint,
				        &rp->u.c.change->c_seqnr,
				        ofs + 1,
			        	last + 1,
				        bitmaps);
}

#endif


/* sfr_rel_finish -- Stop a Reliable stateful Reader's writer proxy.

   Locks: On entry, the reader associated with the remote writer, its
          domain, subscriber and topic should be locked. */

static void sfr_rel_finish (RemWriter_t *rwp)
{
	ctrc_printd (RTPS_ID, RTPS_SFR_REL_FINISH, &rwp, sizeof (rwp));
	prof_start (rtps_rr_finish);

	RW_SIGNAL (rwp, "REL-Finish");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_finish)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_FINISH, "sfr_rel_finish");
#endif

	/* Cleanup Heartbeat Response timer if it is still active. */
	if (rwp->rw_hbrsp_timer) {
		HBRSP_TMR_STOP (rwp, rwp->rw_hbrsp_timer);
		HBRSP_TMR_FREE (rwp);
		tmr_free (rwp->rw_hbrsp_timer);
		rwp->rw_hbrsp_timer = NULL;
	}

	/* Wait until proxy has done sending data. */
	proxy_wait_inactive (&rwp->proxy);

	/* Cleanup resources -- closing down! */
	if (LIST_NONEMPTY (rwp->rw_changes))
		ccref_list_delete (&rwp->rw_changes);
	NEW_RW_CSTATE (rwp, RWCS_FINAL, 0);
	prof_stop (rtps_rr_finish, 1);
}

RW_EVENTS sfr_rel_events = {
	sfr_rel_start,
	sfr_rel_data,
	sfr_rel_gap,
	sfr_rel_heartbeat,
#ifdef RTPS_FRAGMENTS
	sfr_rel_hbfrag,
#endif
	sfr_rel_finish
};

