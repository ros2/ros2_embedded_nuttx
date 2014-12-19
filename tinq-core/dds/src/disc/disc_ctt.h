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

/* disc_ctt.h -- Defines the interface to the secure crypto token transport
		 mechanism. */

#ifndef __disc_ctt_h_
#define __disc_ctt_h_

#include "dds.h"
#include "dds_data.h"
#include "sec_data.h"

int ctt_start (Domain_t *dp);

/* Create and register the Crypto Token transport endpoints. */

void ctt_disable (Domain_t *dp);

/* Disable the Crypto Token transport endpoints. */

void ctt_stop (Domain_t *dp);

/* Stop the Crypto Token transport reader/writer.
   On entry/exit: domain and global lock taken. */

void ctt_connect (Domain_t *dp, Participant_t *rpp);

/* Connect the Crypto Token transport endpoints to the peer. */

void ctt_disconnect (Domain_t *dp, Participant_t *rpp);

/* Disconnect the Crypto Token transport endpoints from the peer. */

void ctt_send (Domain_t                             *dp,
	       Participant_t                        *pp,
	       Endpoint_t                           *sep,
	       Endpoint_t                           *dep,
	       DDS_ParticipantVolatileSecureMessage *msg);

/* Send crypto tokens to a peer participant. */

void ctt_event (Reader_t *rp, NotificationType_t t);

/* New participant to participant volatile secure writer message data available
   callback function. Locked on entry/exit: DP + R(rp). */

void ctt_assert (Domain_t *dp);

/* Check the validity of the volatile secure reader and writer. */

#endif /* !__disc_ctt_h_ */

