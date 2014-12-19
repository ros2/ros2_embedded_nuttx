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

/* disc_match.h -- Interface to endpoint match functions for discovery. */

#ifndef __disc_match_h_
#define __disc_match_h_

#include "dds_data.h"
#include "pid.h"

typedef enum {
	EI_NEW,
	EI_UPDATE,
	EI_DELETE
} InfoType_t;

void user_topic_notify (Topic_t *tp, int new1);

/* Notify a discovered Topic to discovery listeners. */

void user_reader_notify (DiscoveredReader_t *rp, int new1);

/* Notify a discovered Reader to discovery listeners. */

void user_writer_notify (DiscoveredWriter_t *wp, int new1);

/* Notify a discovered Writer to discovery listeners. */

void user_participant_notify (Participant_t *pp, int new1);

/* Notify a discovered Participant to discovery listeners.
   Locked on entry/exit: DP. */

void user_notify_delete (Domain_t       *dp,
			 Builtin_Type_t type,
			 InstanceHandle h);

/* Notify deletion of a builtin topic. */

void disc_new_matched_reader (Writer_t *wp, DiscoveredReader_t *peer_rp);

/* A match between one of our writers and a remote reader was detected.
   On entry/exit: DP, TP, R/W locked. */

void disc_end_matched_reader (Writer_t *wp, DiscoveredReader_t *peer_rp);

/* A match between one of our writers and a remote reader was removed.
   On entry/exit: DP, TP, R/W locked. */

void disc_new_matched_writer (Reader_t *rp, DiscoveredWriter_t *peer_wp);

/* A match between one of our readers and a remote writer was detected.
   On entry/exit: DP, TP, R/W locked. */

void disc_end_matched_writer (Reader_t *rp, DiscoveredWriter_t *peer_wp);

/* A match between one of our readers and a remote writer was removed.
   On entry/exit: DP, TP, R/W locked. */

#endif /* !__disc_match_h_ */

