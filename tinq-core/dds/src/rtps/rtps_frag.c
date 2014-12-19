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

/* rtps_frag.c -- Implements the functions to handle fragments. */

#include "error.h"
#include "prof.h"
#include "set.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_priv.h"
#ifdef RTPS_FRAGMENTS
#include "rtps_frag.h"

FragInfo_t *rfraginfo_create (CCREF        *refp,
			      DataFragSMsg *fragp,
			      unsigned     max_frags)
{
	DB		*dbp;
	FragInfo_t	*fip;

	dbp = db_alloc_data (fragp->sample_size, 1);
	if (!dbp) {
		warn_printf ("rfraginfo_create: no memory for sample data!");
		return (NULL);
	}
	fip = xmalloc (FRAG_INFO_SIZE (max_frags));
	if (!fip) {
		warn_printf ("rfraginfo_create: no memory for fragment info!");
		db_free_data (dbp);
		return (NULL);
	}
	memset (fip, 0, FRAG_INFO_SIZE (max_frags));
	fip->nrefs = 1;
	fip->total = fip->num_na = max_frags;
	fip->fsize = fragp->frag_size;
	if (refp->relevant)
		fip->writer = refp->u.c.change->c_writer;
	else
		fip->writer = 0;
	fip->data = dbp;
	fip->length = fragp->sample_size;
	tmr_init (&fip->timer, "FragInfo");
	refp->fragments = fip;
	return (fip);
}

void rfraginfo_delete (CCREF *refp)
{
	FragInfo_t	*fip;

	if ((fip = refp->fragments) == NULL)
		return;

	refp->fragments = NULL;
	rcl_access (fip);
	fip->nrefs--;
	rcl_done (fip);
	if (fip->nrefs > 0)
		return;

	db_free_data (fip->data);
	tmr_stop (&fip->timer);
	if (fip->key && fip->key != fip->hash.hash)
		xfree (fip->key);
	xfree (fip);
}

FragInfo_t *rfraginfo_update (CCREF *refp, DataFragSMsg *fragp)
{
	FragInfo_t	*fip, *nfip;
	unsigned	max_frags;

	fip = refp->fragments;
	if (!fip)
		return (NULL);

	if (fip->length != fragp->sample_size) {
		max_frags = (fragp->sample_size + fragp->frag_size - 1) / fragp->frag_size;
		if (fragp->frag_start + fragp->num_fragments - 1 > max_frags) {
			rfraginfo_delete (refp);
			return (NULL);
		}
		if (fip->length < fragp->sample_size) {
			db_free_data (fip->data);
			fip->data = db_alloc_data (fragp->sample_size, 1);
			if (!fip->data) {
				warn_printf ("rfraginfo_update: no memory for realloc()!");
				rfraginfo_delete (refp);
				return (NULL);
			}
		}
		fip->length = fragp->sample_size;
		if (fip->total != max_frags) {
			nfip = xrealloc (fip, FRAG_INFO_SIZE (max_frags));
			if (!nfip) {
				rfraginfo_delete (refp);
				return (NULL);
			}
			fip = refp->fragments = nfip;
		}
	}
	else
		max_frags = fip->total;
	memset (fip, 0, FRAG_INFO_SIZE (max_frags));
	fip->total = fip->num_na = max_frags;
	fip->fsize = fragp->frag_size;
	return (fip);
}

void mark_fragment (FragInfo_t *fip, DataFragSMsg *fragp, Change_t *cp)
{
	DB		*dbp;
	unsigned char	*dp, *src;
	unsigned	sleft, f, dleft, left, n;
	int		copy, i;

	/* Check if fragments can be added to sample buffer. */
	dp = fip->data->data;
	dbp = cp->c_db;
	src = cp->c_data;
	if (dbp)
		sleft = dbp->size - (cp->c_data - dbp->data);
	else
		sleft = cp->c_length;
	f = fragp->frag_start - 1;
	if (f) {
		dp += f * fragp->frag_size;
		dleft = fip->length - (dp - fip->data->data);
	}
	else
		dleft = fip->length;

	/* Copy each payload fragment to data buffer. */
	for (i = 0; i < fragp->num_fragments; i++, f++) {
		left = fragp->frag_size;
		if (left > dleft)
			left = dleft;
		if (SET_CONTAINS (fip->bitmap, f))
			copy = 0;
		else {
			SET_ADD (fip->bitmap, f);
			fip->num_na--;
			if (fip->num_na && fip->first_na == f) {
				fip->first_na++;
				while (SET_CONTAINS (fip->bitmap, fip->first_na))
					fip->first_na++;
			}
			copy = 1;
		}
		while (left) {
			n = (sleft > dleft) ? dleft : sleft;
			if (n > left)
				n = left;
			if (n) {
				left -= n;
				dleft -= n;
				if (copy)
					memcpy (dp, src, n);
				dp += n;
			}

			/* Update source pointers. */
			sleft -= n;
			if (sleft)
				src += n;
			else if (dbp && left) {
				dbp = dbp->next;
				sleft = dbp->size;
				src = dbp->data;
			}
		}
	}
}

#endif

