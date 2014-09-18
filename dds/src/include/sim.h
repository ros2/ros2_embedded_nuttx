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

/* sim.h -- Simulation functionality interface. */

#ifndef __sim_h_
#define __sim_h_

#include "rtps_data.h"

/* Support functions for Received message injection: */
/* ------------------------------------------------- */

void sim_new_msg (Participant_t *pp, Endpoint_t *src, Endpoint_t *dst, int mcast);

/* Start a new message between the two endpoints. */

void sim_add_info_ts (Time_t *time, int mcast);

/* Add an INFO_TIMESTAMP message element to the message. */

void sim_add_data (SequenceNumber_t *snr,
		   KeyHash_t *hp,
		   StatusInfo_t *sp,
		   String_t *kp,
		   unsigned char *dp,
		   unsigned length);

/* Add a DATA message element to the message. */

void sim_add_info_dest (GuidPrefix_t *prefix);

/* Add an INFO_DESTINATION message element to the message. */

void sim_add_gap (SequenceNumber_t *start,
		  SequenceNumber_t *base,
		  unsigned         nbits,
		  uint32_t         *bits);

/* Add a GAP message element to the message. */

void sim_add_heartbeat (unsigned         flags,
			SequenceNumber_t *first,
			SequenceNumber_t *last,
			unsigned         count);

/* Add a HEARTBEAT message element to the message. */

void sim_add_acknack (int              final,
		      SequenceNumber_t *base,
		      unsigned         nbits,
		      uint32_t         *bits,
		      unsigned         count);

/* Add an ACKNACK message element to the message. */

void sim_rx_msg (void);

/* Inject the constructed message in the receiver logic. */

void sim_create_bitmap (unsigned base_l, unsigned nbits, const char *mask,
			SequenceNumber_t *base, uint32_t bits [8]);

/* Utility function to make it easier to create a bitmap as used in ACKNACK and
   GAP messages. */


/* Support functions for Transmitted message parsing: */
/* -------------------------------------------------- */

RME *sim_wait_data (SequenceNumber_t *snr, DataSMsg **info);

/* Wait until a DATA message element is sent with the given sequence number. */

RME *sim_wait_info_dest (GuidPrefix_t *prefix);

/* Wait until an INFO_DESTINATION message element is sent with the prefix. */

RME *sim_wait_gap (SequenceNumber_t *start,
		   SequenceNumber_t *base,
		   GapSMsg          **info);

/* Wait until a GAP message element is sent with the given start and base
   sequence numbers. */

RME *sim_wait_heartbeat (SequenceNumber_t *first,
			 SequenceNumber_t *last,
			 HeartbeatSMsg    **info);

/* Wait until a HEARTBEAT message element is sent with the given first and
   last sequence numbers. */

RME *sim_wait_acknack (SequenceNumber_t *base,
		       unsigned         nbits,
		       AckNackSMsg      **info);

/* Wait until an ACKNACK message element is sent with the given base sequence
   number and # of bits specified. */

#endif /* !__sim_h_ */

