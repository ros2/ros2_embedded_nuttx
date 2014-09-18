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

/* disc_spdp.h -- Interface to SPDP Participant Discover protocol functions. */

#ifndef __disc_spdp_h_
#define __disc_spdp_h_

int spdp_init (void);

/* Initialize the SPDP types. */

void spdp_final (void);

/* Finalize the SPDP types. */

int spdp_start (Domain_t *dp);

/* Start the SPDP protocol.  On entry: Domain lock taken. */

int spdp_update (Domain_t *dp);

/* Domain participant data was updated. */

int spdp_rehandshake (Domain_t *dp, int notify_only);

/* Security permissions data was updated. */

void spdp_timeout_participant (Participant_t *p, Ticks_t ticks);

/* Force a participant to timeout after the given number of ticks. */

void spdp_end_participant (Participant_t *pp, int ignore);

/* End participant due to either a time-out or by an explicit unregister by the
   peer via a NOT_ALIVE_* change of the keyed instance, or from an ignore().
   Locked on entry/exit: DP. */

void spdp_stop (Domain_t *dp);

/* Stop the SPDP discovery protocol. Called from disc_stop with domain_lock and
   global_lock taken. */

void spdp_event (Reader_t *rp, NotificationType_t t);

/* New participant data available to be read callback function.
   Locked on entry/exit: DP + R(rp). */

int spdp_send_participant_liveliness (Domain_t *dp);

/* Resend Asserted Participant liveliness. */

void spdp_remote_participant_enable (Domain_t      *dp,
				     Participant_t *pp,
				     unsigned      secret);

/* Finally, a remote participant is successfully authenticated. */

#endif /* !__disc_spdp_h_ */
