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

/* rtps_slbr.c -- Implements the RTPS Best-effort Reader. */

#include "log.h"
#include "prof.h"
#include "error.h"
#include "list.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_check.h"
#include "rtps_priv.h"
#include "rtps_trace.h"
#include "rtps_clist.h"
#ifdef RTPS_FRAGMENTS
#include "rtps_frag.h"
#endif
#include "rtps_slbr.h"

#ifdef RTPS_FRAGMENTS

static void reader_frag_to (uintptr_t user)
{
	RemWriter_t	*rwp = (RemWriter_t *) user;
	READER		*rp = rwp->rw_reader;
	CCREF		*refp;

	log_printf (RTPS_ID, 0, "reader_frag_to: %p\r\n", (void *) rwp);
	refp = LIST_HEAD (rwp->rw_changes);
	hc_change_free (refp->u.c.change);
	rfraginfo_delete (refp);
	mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
	remote_writer_remove (rp, rwp);
	mds_pool_free (&rtps_mem_blocks [MB_REM_WRITER], rwp);
}

void reader_add_fragment (READER       *rp,
			  Change_t     *cp,
			  KeyHash_t    *hp,
			  DataFragSMsg *fragp)
{
	Reader_t	*r = (Reader_t *) rp->endpoint.endpoint;
	RemWriter_t	*rwp, *fwp;
	CCREF		*refp;
	FragInfo_t	*fip;
	Change_t	*ncp;
	unsigned	max_frags;

	/*log_printf (RTPS_ID, 0, "reader_add_fragment: snr:%u, start:%u, num:%u, fsize:%u, tsize:%u\r\n",
				fragp->writer_sn.low, 
				fragp->frag_start, fragp->num_fragments,
				fragp->frag_size, fragp->sample_size);*/
	/* Check if we already got fragments from writer. */
	fwp = NULL;
	LIST_FOREACH (rp->rem_writers, rwp)
		if ((refp = LIST_HEAD (rwp->rw_changes)) != NULL &&
		    refp->u.c.change->c_writer == cp->c_writer) {
			fwp = rwp;
			break;
		}

	/* If this is the first, add a new RemWriter for the fragment,
	   allocate all relevant data, and start the fragment timer. */
	if (!fwp) {
		max_frags = (fragp->sample_size + fragp->frag_size - 1) / fragp->frag_size;
		if (fragp->frag_start + fragp->num_fragments - 1 > max_frags)
			return;

		if ((rwp = mds_pool_alloc (&rtps_mem_blocks [MB_REM_WRITER])) == NULL) {
			warn_printf ("reader_add_fragment: no memory for fragment!");
			return;
		}
		memset (rwp, 0, sizeof (RemWriter_t));
		rwp->rw_reader = rp;
		LIST_INIT (rwp->rw_changes);
		rp->rem_writers.count++;
		fwp = rwp;
		LIST_ADD_TAIL (rp->rem_writers, *rwp);
		ncp = hc_change_new ();
		if (!ncp) {
			warn_printf ("reader_add_fragment: no memory for change!");
			goto no_change_mem;
		}
		ncp->c_kind = cp->c_kind;
		ncp->c_writer = cp->c_writer;
		ncp->c_time = cp->c_time;
		ncp->c_seqnr = cp->c_seqnr;
		refp = ccref_add (&rwp->rw_changes, ncp, 0, 1, CS_RECEIVED);
		if (!refp) {
			warn_printf ("reader_add_fragment: no memory for list element!");
			goto no_ref_mem;
		}
		fip = rfraginfo_create (refp, fragp, max_frags);
		if (!fip) {
			warn_printf ("reader_add_fragment: no memory for fragment info!");
			goto no_frag_mem;
		}
	}
	else {
		refp = LIST_HEAD (rwp->rw_changes);
		fip = refp->fragments;
		ncp = refp->u.c.change;
		if (fip->fsize != fragp->frag_size ||
		    fip->length != fragp->sample_size ||
		    !SEQNR_EQ (ncp->c_seqnr, fragp->writer_sn)) {

			/* Incorrect fragment context: reset it. */
			fip = rfraginfo_update (refp, fragp);
			if (!fip)
				goto cleanup;
		}
		else if (fragp->frag_start + fragp->num_fragments - 1 > fip->total)
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

	/* If all fragments received correctly, cleanup the context. */
	if (!fip->num_na) {
		ncp->c_db = fip->data;
		ncp->c_length = fip->length;
		ncp->c_data = fip->data->data;
		rcl_access (fip->data);
		fip->data->nrefs++;
		rcl_done (fip->data);
		rfraginfo_delete (refp);
		reader_cache_add_key (rp, ncp, &fip->hash, fip->key, fip->keylen);

	    cleanup:

		if (fip) {		
			FRAGSC_TMR_STOP (rwp, &fip->timer);
			rfraginfo_delete (refp);
		}
		mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
		remote_writer_remove (rp, rwp);
		mds_pool_free (&rtps_mem_blocks [MB_REM_WRITER], rwp);
	}
	else {
		FRAGSC_TMR_START (rwp,
				  &fip->timer,
				  TICKS_PER_SEC * 2,
				  (uintptr_t) rwp,
				  reader_frag_to,
				  &r->r_lock);
	}
	return;

    no_frag_mem:
	mds_pool_free (&rtps_mem_blocks [MB_CCREF], refp);
    no_ref_mem:
    	hc_change_free (ncp);
    no_change_mem:
	mds_pool_free (&rtps_mem_blocks [MB_REM_WRITER], rwp);
	return;
}

#endif

