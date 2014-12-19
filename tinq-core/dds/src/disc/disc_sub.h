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

/* disc_sub.h -- Interface to the subscription functions for discovery. */

#ifndef __disc_sub_h_
#define __disc_sub_h_

#include "dds_data.h"
#include "pid.h"

int disc_subscription_add (Participant_t        *pp,
			   DiscoveredReader_t   *drp,
			   const UniQos_t       *qp,
			   Topic_t              *tp,
			   Writer_t             *wp,
			   DiscoveredReaderData *info);

/* Add a Discovered Reader.  On entry/exit: DP locked. */

int disc_subscription_update (Participant_t        *pp,
			      DiscoveredReader_t   *drp,
			      DiscoveredReaderData *info);

/* Update a Discovered Reader.  On entry/exit: DP locked. */

void disc_subscription_remove (Participant_t      *pp,
			       DiscoveredReader_t *rp);

/* Remove a Discovered Reader. On entry/exit: DP locked. */

void discovered_reader_cleanup (DiscoveredReader_t *rp,
				int                ignore,
				int                *p_last_topic,
				int                *topic_gone);

/* Cleanup a previously discovered reader. */


#endif /* !__disc_sub_h_ */

