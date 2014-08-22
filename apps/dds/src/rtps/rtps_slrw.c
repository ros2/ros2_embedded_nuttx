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

/* rtps_slrw.c -- Implements the RTPS Reliable Stateless Writer functionality. */

#include "prof.h"
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_priv.h"

#ifdef RTPS_SL_REL

static void slw_rel_start (RemReader_t *rrp)
{
	/* ... TBC ... */
}

static void slw_rel_new_change (RemReader_t *rrp, Change_t *cp)
{
	/* ... TBC ... */
}

static int slw_rel_send_data (RemReader_t *rrp)
{
	/* ... TBC ... */

	return (0);
}

static void slw_rel_acknack (RemReader_t       *rrp,
			     SequenceNumberSet *r_state,
			     int               final)
{
	/* ... TBC ... */
}

#ifdef RTPS_FRAGMENTS

static void slw_rel_nackfrag (RemReader_t *rrp, NackFragSMsg *np)
{
	/* ... TBC ... */
}

#endif

static void slw_rel_nack_resp_to (RemReader_t *rrp)
{
	/* ... TBC ... */
}

static void slw_rel_rem_change (RemReader_t *rrp, Change_t *cp)
{
	/* ... TBC ... */
}

static void slw_rel_finish (RemReader_t *rrp)
{
	/* ... TBC ... */
}

RR_EVENTS slw_rel_events = {
	slw_rel_start,
	slw_rel_new_change,
	slw_rel_send_data,
	slw_rel_acknack,
#ifdef RTPS_FRAGMENTS
	slw_rel_nackfrag,
#endif
	slw_rel_rem_change,
	slw_rel_finish
};

#else

RR_EVENTS slw_rel_events = {
	NULL,
	NULL,
	NULL,
	NULL,
#ifdef RTPS_FRAGMENTS
	NULL,
#endif
	NULL,
	NULL
};

#endif

