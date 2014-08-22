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

/* rtps_sfbr.c -- Implements the RTPS Stateful Best-effort Reader. */

#include "log.h"
#include "prof.h"
#include "error.h"
#include "ctrace.h"
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
#include "rtps_sfbr.h"

/* DDS_FILTER_READ - Returns a non-0 result if the sample is not relevant for
		     the reader. */

#define	DDS_FILTER_READ(rwp,cp)		0

static void sfr_be_start (RemWriter_t *rwp)
{
	ctrc_printd (RTPS_ID, RTPS_SFR_BE_START, &rwp, sizeof (rwp));
	prof_start (rtps_br_start);

	RW_SIGNAL (rwp, "BE-Start");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_start)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_START, "sfr_be_start");
#endif
	NEW_RW_CSTATE (rwp, RWCS_INITIAL, 1);
	LIST_INIT (rwp->rr_changes);
	rwp->rw_changes.nchanges = 0;
	NEW_RW_CSTATE (rwp, RWCS_READY, 0);
	NEW_RW_ASTATE (rwp, RWAS_WAITING, 1);
	prof_stop (rtps_br_start, 1);
}

#ifdef RTPS_FRAGMENTS

static void sfr_be_frag_to (uintptr_t user)
{
	RemWriter_t	*rwp = (RemWriter_t *) user;
	CCREF		*refp;

	log_printf (RTPS_ID, 0, "sfr_be_frag_to: %p\r\n", (void *) rwp);
	refp = LIST_HEAD (rwp->rw_changes);
	hc_change_free (refp->u.c.change);
	rfraginfo_delete (refp);
	mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
}

#endif

static void sfr_be_data (RemWriter_t         *rwp,
			 Change_t            *cp,
			 SequenceNumber_t    *cpsnr,
			 const KeyHash_t     *hp,
			 const unsigned char *key,
			 size_t              keylen,
#ifdef RTPS_FRAGMENTS
			 DataFragSMsg        *fragp,
			 FragInfo_t	     **finfo,
#endif
			 int                 ignore)
{
	Change_t	*ncp;
	READER		*rp = rwp->rw_reader;
	Reader_t	*r = (Reader_t *) rp->endpoint.endpoint;
#ifdef RTPS_FRAGMENTS
	CCREF		*refp;
	FragInfo_t	*fip;
	unsigned	max_frags = 0;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	DB		*dbp;
	DBW		walk;
	int		error;
#endif
	size_t		ofs;
#endif

	ctrc_printd (RTPS_ID, RTPS_SFR_BE_DATA, &rwp, sizeof (rwp));
	prof_start (rtps_br_data);
#ifdef RTPS_FRAGMENTS
	if (rwp->rw_changes.nchanges) {	/* Lingering fragment? */
		refp = LIST_HEAD (rwp->rw_changes);
		fip = refp->fragments;
	}
	else {
		refp = NULL;
		fip = NULL;
	}
	if (fragp) {
		STATS_INC (rwp->rw_ndatafrags);
		if (!fip) {
			max_frags = (fragp->sample_size + fragp->frag_size - 1) / fragp->frag_size;
			if (fragp->frag_start != 1 ||
			    fragp->frag_start + fragp->num_fragments - 1 > max_frags)
				return;

			ncp = hc_change_new ();
			if (!ncp) {
				warn_printf ("sfr_be_data: no memory for change!");
				return;
			}
			ncp->c_kind = cp->c_kind;
			ncp->c_writer = cp->c_writer;
			ncp->c_time = cp->c_time;
			ncp->c_seqnr = cp->c_seqnr;
			refp = ccref_add (&rwp->rw_changes, ncp, 0, 1, CS_RECEIVED);
			if (!refp) {
				warn_printf ("sfr_be_data: no memory for list element!");
				goto no_ref_mem;
			}

		    new_finfo:

			if (*finfo) {
				fip = *finfo;
				rcl_access (fip);
				fip->nrefs++;
				rcl_done (fip);
			}
			else {
				fip = *finfo = rfraginfo_create (refp, fragp, max_frags);
				if (!fip) {
					goto no_frag_mem;
				}
			}
		}
		else {
			ncp = refp->u.c.change;
			if (fip->fsize != fragp->frag_size ||
			    fip->length != fragp->sample_size ||
			    !SEQNR_EQ (ncp->c_seqnr, fragp->writer_sn)) {

				if (fragp->frag_start != 1)
					return;

				/* Incorrect fragment context: reset it. */
				if (fip->nrefs == 1) {
					fip = rfraginfo_update (refp, fragp);
					if (!fip)
						goto no_frag_mem;
				}
				else {
					rfraginfo_delete (refp);
					goto new_finfo;
				}
			}
			else if (fragp->frag_start != fip->first_na + 1 ||
				 fragp->frag_start + fragp->num_fragments - 1 >
								fip->total)
				return;
		}

		/* Update key info if present. */
		if (hp) {
			fip->hash = *hp;
			fip->hp = &fip->hash;
			fip->key = fip->hash.hash;
			fip->keylen = 12;
		}

		/* Mark the fragment as correctly received. */
		mark_fragment (fip, fragp, cp);
		fip->first_na++;

		/* Check if all fragments are received correctly. */
		if (fip->num_na) {
			FRAGSC_TMR_START (rwp,
					  &fip->timer,
					  TICKS_PER_SEC * 2,
					  (uintptr_t) rwp,
					  sfr_be_frag_to,
					  &r->r_lock);
			return;
		}

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

		/* Decrypt payload data if an encrypted payload is present. */
		if (rwp->rw_endpoint &&
		    rwp->rw_crypto &&
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
				return;

			fip->data = dbp;
		}
		else
#endif
			ofs = 0;

		/* Cleanup the context. */
		ncp->c_db = fip->data;
		ncp->c_length = fip->length;
		ncp->c_data = fip->data->data + ofs;
		rcl_access (fip->data);
		fip->data->nrefs++;
		rcl_done (fip->data);
		cp = ncp;
		FRAGSC_TMR_STOP (rwp, &fip->timer);
		rfraginfo_delete (refp);
		mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
		LIST_INIT (rwp->rw_changes);
		rwp->rw_changes.nchanges = 0;
	}
	else {
#endif
		STATS_INC (rwp->rw_ndata);

#ifdef RTPS_FRAGMENTS
		if (fip) { /* Lingering fragment?  Cleanup context. */
			FRAGSC_TMR_STOP (rwp, &fip->timer);
			rfraginfo_delete (refp);
			mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
			LIST_INIT (rwp->rw_changes);
			rwp->rw_changes.nchanges = 0;
		}
	}
#endif
	RW_SIGNAL (rwp, "BE-Data");

#ifdef RTPS_MARKERS
	if (rp->endpoint.mark_data)
		rtps_marker_notify (r, EM_DATA, "sfr_be_data");
#endif
	if (SEQNR_LT (*cpsnr, rwp->rw_seqnr_next))
		return;

	if (!SEQNR_EQ (*cpsnr, rwp->rw_seqnr_next))
		dcps_samples_lost (r, SEQNR_DELTA (rwp->rw_seqnr_next, *cpsnr));

	if (!ignore) {
		if (cp->c_nrefs > 1) {
			ncp = hc_change_clone (cp);
			if (!ncp) {
				warn_printf ("sfr_be_data: out of memory for change clone!\r\n");
				return;
			}
		}
		else {
			rcl_access (cp);
			cp->c_nrefs++;
			rcl_done (cp);
			ncp = cp;
		}
		ncp->c_seqnr = *cpsnr;
		reader_cache_add_key (rp, ncp, hp, key, keylen);
	}
	rwp->rw_seqnr_next = cp->c_seqnr;
	SEQNR_INC (rwp->rw_seqnr_next);
	prof_stop (rtps_br_data, 1);
	RW_SNR_TRACE (rwp, "BE-data: update");

#ifdef RTPS_FRAGMENTS
	return;

    no_frag_mem:
	warn_printf ("sfr_be_data: no memory for fragment info!");
	LIST_INIT (rwp->rw_changes);
	rwp->rw_changes.nchanges = 0;
	hc_change_free (refp->u.c.change);
	mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
    no_ref_mem:
    	hc_change_free (ncp);
#endif
}

static void sfr_be_finish (RemWriter_t *rwp)
{
	ctrc_printd (RTPS_ID, RTPS_SFR_BE_FINISH, &rwp, sizeof (rwp));
	prof_start (rtps_br_finish);

	RW_SIGNAL (rwp, "BE-Finish");

#ifdef RTPS_MARKERS
	if (rwp->rw_reader->endpoint.mark_finish)
		rtps_marker_notify (rwp->rw_reader->endpoint.endpoint, EM_FINISH, "sfr_be_finish");
#endif

	/* We're done. */
	NEW_RW_CSTATE (rwp, RWCS_FINAL, 0);
	prof_stop (rtps_br_finish, 1);
}

RW_EVENTS sfr_be_events = {
	sfr_be_start,
	sfr_be_data,
	NULL,
	NULL,
#ifdef RTPS_FRAGMENTS
	NULL,
#endif
	sfr_be_finish
};


