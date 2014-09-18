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

#include "rtps_check.h"

#ifdef RTPS_CACHE_CHECK

static const char *check_name;

static void rtps_verify_change (Change_t *cp)
{
	if (mds_pool_contains (&mem_blocks [MB_CACHE_CHANGE], cp))
		fatal_printf ("rtps_check: CacheChange already released (%s)!", check_name);

	if (cp->c_nrefs == 0 || cp->c_nrefs > 256)
		fatal_printf ("rtps_check: cache is corrupt (%s)!", check_name);
}

static void rtps_verify_ref (CCREF *rp)
{
	if (mds_pool_contains (&mem_blocks [MB_CCREF], rp))
		fatal_printf ("rtps_check: Reference already released (%s)!", check_name);
	if (rp->change)
		rtps_verify_change (rp->change);
}

static void rtps_verify_cclist (CCLIST *lp)
{
	CCREF		*rp;
	unsigned	n = 0;

	if (LIST_NONEMPTY (*lp)) {
		if (lp->head->prev || lp->tail->next)
			fatal_printf ("rtps_check: ref-list is corrupt-a (%s)!", check_name);
		LIST_FOREACH (*lp, rp) {
			rtps_verify_ref (rp);
			n++;
			if (!rp->next && lp->tail != rp)
				fatal_printf ("rtps_check: ref-list is corrupt-b (%s)!", check_name);
		}
	}
	if (n != lp->nchanges)
		fatal_printf ("rtps_check: ref-list is corrupt-c (%s)!", check_name);
}

/* rtps_writer_proxy_check -- Check the ReaderLocator/Proxy contexts of a 
			      writer. */

static void rtps_writer_proxy_check (WRITER *wp)
{
	RemReader_t	*rrp;
	CCREF		*rp;
	unsigned	nunacked, nunsent, nrequested, i;

	if (mds_pool_contains (&mem_blocks [MB_WRITER], wp))
		fatal_printf ("rtps_check: writer already released (%s)!", check_name);
		
	LIST_FOREACH (wp->rem_readers, rrp) {
		if (mds_pool_contains (&mem_blocks [MB_REM_READER], rrp))
			fatal_printf ("rtps_check: WriterProxy already released (%s)!", check_name);

		rtps_verify_cclist (&rrp->rr_changes);
		if (!rrp->rr_reliable)
			continue;

		nunacked = 0;
		nunsent = ~0U;
		nrequested = ~0U;
		for (rp = LIST_HEAD (rrp->rr_changes), i = 0;
		     rp;
		     rp = LIST_NEXT (rrp->rr_next, *rp), i++)
			switch (rp->state) {
				case CS_NEW:
					if (rp->relevant)
						nunacked++;
					break;
				case CS_UNSENT:
					if (nunsent == ~0U)
						nunsent = i;
					if (rp->relevant)
						nunacked++;
					break;
				case CS_REQUESTED:
					if (nrequested == ~0U)
						nrequested = i;
					if (rp->relevant)
						nunacked++;
					break;
				case CS_UNDERWAY:
				case CS_UNACKED:
					if (rp->relevant)
						nunacked++;
					break;
				case CS_ACKED:
				default:
				  	break;
			}
		if (nunacked != rrp->rr_unacked)
			fatal_printf ("rtps_check: unacked proxy counter is incorrect (%s)!", check_name);
		if (nunsent != ~0U && nunsent != rrp->rr_unsent)
			fatal_printf ("rtps_check: unsent proxy counter is incorrect (%s)!", check_name);
		if (nrequested != ~0U && nrequested != rrp->rr_requested)
			fatal_printf ("rtps_check: requested proxy counter is incorrect (%s)!", check_name);
	}
}

/* rtps_reader_proxy_check -- Check the Writer Proxy contexts of a reader. */

static void rtps_reader_proxy_check (READER *rp)
{
	RemWriter_t	*rwp;

	if (mds_pool_contains (&mem_blocks [MB_READER], rp))
		fatal_printf ("rtps_check: reader already released (%s)!", check_name);
		
	if (!rp->endpoint.stateful)
		return;

	LIST_FOREACH (rp->rem_writers, rwp) {
		if (mds_pool_contains (&mem_blocks [MB_REM_WRITER], rwp))
			fatal_printf ("rtps_check: ReaderLocator/Proxy already released (%s)!", check_name);

		rtps_verify_cclist (&rwp->rw_changes);
	}
}

/* rtps_proxy_check -- Check the proxy contexts of an endpoint. */

void rtps_proxy_check (ENDPOINT *ep)
{
	if (ep->is_reader)
		rtps_reader_proxy_check ((READER *) ep);
	else
		rtps_writer_proxy_check ((WRITER *) ep);
}

void rtps_cache_check (ENDPOINT *ep, const char *name)
{
	check_name = name;
	hc_check (ep->endpoint->cache);
	rtps_proxy_check (ep);
}

#endif

#ifdef RTPS_RANGE_CHECK

void rtps_range_check (CCLIST *lp, const char *id)
{
	CCREF		*rp, *prev_rp = NULL;
	SequenceNumber_t prev_snr = { 0, 0};

	LIST_FOREACH (*lp, rp) {
		if (rp->relevant) {
			if (!rp->u.c.change->c_seqnr.high &&
			    !rp->u.c.change->c_seqnr.low)
				fatal_printf ("range_check(%s): 0-seqnr!", id);
			if (prev_rp && !SEQNR_GT (rp->u.c.change->c_seqnr, prev_snr))
				fatal_printf ("range_check(%s): prev_seqnr>seqnr!", id);
			if (prev_rp) {
				SEQNR_INC (prev_snr);
				if (!SEQNR_EQ (prev_snr, rp->u.c.change->c_seqnr))
					fatal_printf ("range_check(%s): prev_seqnr+1!=seqnr!", id);
			}
			prev_rp = rp;
			prev_snr = rp->u.c.change->c_seqnr;
		}
		else {
			if (!rp->u.range.first.high && !rp->u.range.first.low)
				fatal_printf ("range_check(%s): 0-seqnr-first!", id);
			if (!rp->u.range.last.high && !rp->u.range.last.low)
				fatal_printf ("range_check(%s): 0-seqnr-last!", id);
			if (SEQNR_GT (rp->u.range.first, rp->u.range.last))
				fatal_printf ("range_check(%s): seqnr-first>seqnr-last!", id);
			if (prev_rp && !SEQNR_GT (rp->u.range.first, prev_snr))
				fatal_printf ("range_check(%s): prev_seqnr>seqnr-first!", id);
			if (prev_rp) {
				SEQNR_INC (prev_snr);
				if (!SEQNR_EQ (prev_snr, rp->u.range.first))
					fatal_printf ("range_check(%s): prev_seqnr+1!=seqnr-first!", id);
			}
			prev_rp = rp;
			prev_snr = rp->u.range.last;
		}
	}
}

#else
int avoid_emtpy_translation_unit_rtps_check_c;
#endif

