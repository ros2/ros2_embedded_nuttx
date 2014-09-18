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

/* rtps_clist.h -- Defines a number of RTPS change list manipulations. */

#ifndef __rtps_clist_h_
#define __rtps_clist_h_

#include "rtps_priv.h"

#define	c_no_mcast	c_cached	/* Change may not be multicasted. */

CCREF *ccref_add (CCLIST        *list,
		  Change_t      *cp,
		  HCI           hci,
		  int           tail,
		  ChangeState_t state);

void ccref_delete (CCREF *rp);

CCREF *ccref_add_gap (CCLIST           *list,
		      SequenceNumber_t *first,
		      SequenceNumber_t *last,
		      int              tail,
		      ChangeState_t    state);

CCREF *ccref_add_received (CCLIST           *list,
			   Change_t         *cp,
			   SequenceNumber_t *cpsnr,
			   HCI              hci,
			   InstanceHandle   h,
			   int              tail);

CCREF *ccref_insert_gap (CCLIST           *list,
			 CCREF            *pp,
			 SequenceNumber_t *first,
			 SequenceNumber_t *last,
			 ChangeState_t    state);

void ccref_list_delete (CCLIST *list);

#define	change_enqueue(rrp,cp,hci,state) ccref_add (&rrp->rr_changes, cp, hci, 1, state)

void change_delete_enqueued (RemReader_t *rrp);

#define	change_enqueue_gap(rrp,first,last,state) ccref_add_gap (&rrp->rr_changes, \
							 first, last, 1, state)

void change_remove_ref (RemReader_t *rrp, CCREF *rp);

#endif /* !__rtps_clist_h_ */

