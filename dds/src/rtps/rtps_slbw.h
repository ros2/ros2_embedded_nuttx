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

/* rtps_slbw.h -- Defines the interface to the Best-effort Stateless Writer. */

#ifndef __rtps_slbw_h_
#define	__rtps_slbw_h_

#include "rtps_priv.h"

extern RR_EVENTS slw_be_events;

extern unsigned rtps_sl_retries;

void slw_be_resend (WRITER *wp);

/* Resend all changes on all ReaderLocators. */

void slw_be_alive (WRITER *wp, GuidPrefix_t *prefix);

/* Update Participant Liveliness. */

int be_send_data (RemReader_t *rrp, DiscoveredReader_t *dest);

/* Send data directly (used by both Stateless and Stateful Best-Effort
   writer state machines). */

#endif /* !__rtps_slbw_h_ */

