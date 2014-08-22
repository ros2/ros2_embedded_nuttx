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

/* rtps_clist.c -- Implements a number of change list manipulations. */

#include "error.h"
#include "prof.h"
#include "list.h"
#include "rtps_data.h"
#include "rtps_priv.h"
#include "rtps_clist.h"

void ccref_delete (CCREF *rp)
{
	if (rp->relevant) {
		TRC_CHANGE (rp->u.c.change, "ccref_delete", 0);
		hc_change_free (rp->u.c.change);
	}
#ifdef RTPS_FRAGMENTS
	if (rp->relevant && rp->fragments) {
		xfree (rp->fragments);
		rp->fragments = NULL;
	}
#endif
	mds_pool_free (&rtps_mem_blocks [MB_CCREF], rp);
}

void ccref_list_delete (CCLIST *list)
{
	CCREF		*rp, *next_rp;

	for (rp = LIST_HEAD (*list); rp; rp = next_rp) {
		next_rp = LIST_NEXT (*list, *rp);
		ccref_delete (rp);
	}
	LIST_INIT (*list);
	list->nchanges = 0;
}

CCREF *ccref_add (CCLIST        *list,
		  Change_t      *cp,
		  HCI           hci,
		  int           tail,
		  ChangeState_t state)
{
	CCREF	*rp;

	if ((rp = mds_pool_alloc (&rtps_mem_blocks [MB_CCREF])) == NULL) {
		warn_printf ("ccref_add (): out of memory!\r\n");
		return (NULL);
	}
	rp->state = state;
	rp->relevant = 1;
	rp->ack_req = 0;
#ifdef RTPS_FRAGMENTS
	rp->fragments = NULL;
#endif
	rp->u.c.hci = hci;
	rp->u.c.change = cp;
	if (tail) {
		LIST_ADD_TAIL (*list, *rp);
	}
	else {
		LIST_ADD_HEAD (*list, *rp);
	}
	if (state != CS_RECEIVED)
		cp->c_wack++;
	list->nchanges++;
	return (rp);
}

CCREF *ccref_add_received (CCLIST           *list,
			   Change_t         *cp,
			   SequenceNumber_t *cpsnr,
			   HCI              hci,
			   InstanceHandle   h,
			   int              tail)
{
	Change_t	*ncp;
	CCREF		*rp;

	if (cp->c_nrefs > 1) {
		ncp = hc_change_clone (cp);
		if (!ncp) {
			warn_printf ("ccref_add_received (): out of memory for change clone!\r\n");
			return (NULL);
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
	if ((rp = mds_pool_alloc (&rtps_mem_blocks [MB_CCREF])) == NULL) {
		warn_printf ("ccref_add (): out of memory!\r\n");
		hc_change_free (ncp);
		return (NULL);
	}
	rp->state = CS_RECEIVED;
	rp->relevant = 1;
	rp->ack_req = 0;
#ifdef RTPS_FRAGMENTS
	rp->fragments = NULL;
#endif
	rp->u.c.hci = hci;
	rp->u.c.change = ncp;
	if (tail) {
		LIST_ADD_TAIL (*list, *rp);
	}
	else {
		LIST_ADD_HEAD (*list, *rp);
	}
	list->nchanges++;
	return (rp);
}

CCREF *ccref_add_gap (CCLIST           *list,
		      SequenceNumber_t *first,
		      SequenceNumber_t *last,
		      int              tail,
		      ChangeState_t    state)
{
	CCREF	*rp, *xrp;

	if (tail)
		xrp = LIST_TAIL (*list);
	else
		xrp = LIST_HEAD (*list);
	if (xrp && !xrp->relevant && xrp->state == state) {
		if (tail)
			xrp->u.range.last = *last;
		else
			xrp->u.range.first = *first;
		return (xrp);
	}
	if ((rp = mds_pool_alloc (&rtps_mem_blocks [MB_CCREF])) == NULL) {
		warn_printf ("ccref_add_gap (): out of memory!\r\n");
		return (NULL);
	}
	rp->state = state;
	rp->relevant = 0;
	rp->ack_req = 0;
#ifdef RTPS_FRAGMENTS
	rp->fragments = NULL;
#endif
	rp->u.range.first = *first;
	rp->u.range.last = *last;
	if (tail) {
		LIST_ADD_TAIL (*list, *rp);
	}
	else {
		LIST_ADD_HEAD (*list, *rp);
	}
	list->nchanges++;
	return (rp);
}

CCREF *ccref_insert_gap (CCLIST           *list,
			 CCREF            *pp,
			 SequenceNumber_t *first,
			 SequenceNumber_t *last,
			 ChangeState_t    state)
{
	CCREF	*rp;

	if ((rp = mds_pool_alloc (&rtps_mem_blocks [MB_CCREF])) == NULL) {
		warn_printf ("ccref_insert_gap (): out of memory!\r\n");
		return (NULL);
	}
	rp->relevant = 0;
	rp->ack_req = 0;
#ifdef RTPS_FRAGMENTS
	rp->fragments = NULL;
#endif
	rp->u.range.first = *first;
	rp->u.range.last = *last;
	rp->state = state;
	LIST_INSERT (*rp, *pp);
	list->nchanges++;
	return (rp);
}

/* change_delete_enqueued -- Signal that all queued samples are successfully
			     acknowledged and free all of them. */

void change_delete_enqueued (RemReader_t *rrp)
{
	Writer_t	*wp;
	CCREF		*rp;
	
	wp = (Writer_t *) rrp->rr_writer->endpoint.endpoint;
	LIST_FOREACH (rrp->rr_changes, rp)
		if (rp->relevant && rp->ack_req)
			hc_acknowledged (wp->w_cache,
					 rp->u.c.hci, &rp->u.c.change->c_seqnr);
	rrp->rr_unsent_changes = NULL;
	rrp->rr_requested_changes = NULL;
	if (LIST_NONEMPTY (rrp->rr_changes))
		ccref_list_delete (&rrp->rr_changes);
	rrp->rr_unacked = 0;
}

void change_remove_ref (RemReader_t *rrp, CCREF *rp)
{
	if (rrp->rr_unsent_changes && rrp->rr_unsent_changes == rp)
		rrp->rr_unsent_changes = LIST_NEXT (rrp->rr_changes, *rp);
	
	if (rrp->rr_requested_changes && rrp->rr_requested_changes == rp)
		do { 
			rrp->rr_requested_changes = LIST_NEXT (rrp->rr_changes, *rp); 
		} 
		while (rrp->rr_requested_changes && 
		       (!rrp->rr_requested_changes->relevant || 
			rrp->rr_requested_changes->state != CS_REQUESTED));

	if (rrp->rr_changes.nchanges == 1) {
		if (rp->relevant)
			rrp->rr_new_snr = rp->u.c.change->c_seqnr;
		else
			rrp->rr_new_snr = rp->u.range.last;
	}
	LIST_REMOVE (rrp->rr_changes, *rp);
	rrp->rr_changes.nchanges--;
	ccref_delete (rp);
}

void change_remove (RemReader_t *rrp, Change_t *cp)
{
	CCREF	*rp;

	LIST_FOREACH (rrp->rr_changes, rp)
		if (rp->relevant && SEQNR_EQ (rp->u.c.change->c_seqnr, cp->c_seqnr))
			break;

	if (LIST_END (rrp->rr_changes, rp))	/* No longer in cache! */
		return;

	change_remove_ref (rrp, rp);
}


