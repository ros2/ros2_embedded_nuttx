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

/* rtps_sfrr.h -- Defines the interface to the Reliable Stateful Reader. */

#ifndef __rtps_sfrr_h_
#define __rtps_sfrr_h_

extern RW_EVENTS sfr_rel_events;

void sfr_process_samples (RemWriter_t *rwp);

/* Changes list of remote writer contains samples with state CS_LOST or 
   CS_RECEIVED at the front.  Remove all samples with this state and move them
   to the history cache (CS_RECEIVED and relevant != 0) or forget about the
   sample. */

void sfr_restart (RemWriter_t *rwp);

/* Restart a stateful reliable remote writer proxy . */

#endif /* __rtps_sfrr_h_ */

