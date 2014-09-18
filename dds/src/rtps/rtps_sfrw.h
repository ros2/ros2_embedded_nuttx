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

/* rtps_sfrw.h -- Defines the interface to the Reliable Stateful Writer. */

#ifndef __rtps_sfrw_h_
#define	__rtps_sfrw_h_

#include "rtps_priv.h"

extern RR_EVENTS sfw_rel_events;

int sfw_send_heartbeat (WRITER *wp, int alive);

/* Send a Heartbeat message. */

void sfw_restart (RemReader_t *rrp);

/* Restart a stateful reliable remote reader proxy . */

#endif /* !__rtps_sfrw_h_ */

