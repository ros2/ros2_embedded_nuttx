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

/* disc_pub.h -- Interface to the publication functions for discovery. */

#ifndef __disc_pub_h_
#define __disc_pub_h_

#include "dds_data.h"
#include "pid.h"

int disc_publication_add (Participant_t        *pp,
			  DiscoveredWriter_t   *dwp,
			  const UniQos_t       *qp,
			  Topic_t              *tp,
			  Reader_t             *rp,
			  DiscoveredWriterData *info);

/* Add a Discovered Writer.  On entry/exit: DP locked. */

int disc_publication_update (Participant_t        *pp,
			     DiscoveredWriter_t   *dwp,
			     DiscoveredWriterData *info);

/* Update a Discovered Writer.  On entry/exit: DP locked. */

void disc_publication_remove (Participant_t      *pp,
			      DiscoveredWriter_t *wp);

/* Remove a Discovered Writer. On entry/exit: DP locked. */

void discovered_writer_cleanup (DiscoveredWriter_t *wp,
				int                ignore,
				int                *p_last_topic,
				int                *topic_gone);

/* Cleanup a previously discovered writer.
   On entry/exit: DP locked. */

#endif /* !__disc_sub_h_ */


