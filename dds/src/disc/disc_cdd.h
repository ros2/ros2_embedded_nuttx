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

/* disc_cdd.h -- Defines the interface to the Central Discovery functions. */

#ifndef __disc_cdd_h_
#define __disc_cdd_h_

#include "dds_data.h"
#include "pid.h"

int cdd_start (Domain_t *dp);

/* Start the central discovery protocol. */

void cdd_disable (Domain_t *dp);

/* Disable the central discovery reader/writer. */

void cdd_stop (Domain_t *dp);

/* Stop the central discovery reader/writer. */

void cdd_connect (Domain_t *dp, Participant_t *rpp);

/* Connect the central discovery endpoints to a peer. */

void cdd_disconnect (Domain_t *dp, Participant_t *rpp);

/* Disconnect the central discovery endpoints from the peer. */

void cdd_publication_add (Domain_t           *dp,
			  DiscoveredWriter_t *dwp,
			  Participant_t      *pp);

/* Add a publication to the CDD matched publications writer.
   On entry/exit: DP, DW(dwp) locked. */

void cdd_publication_remove (Domain_t *dp, GUID_t *gp);

/* Remove a publication from the CDD publication notification channel. */

void cdd_subscription_add (Domain_t           *dp,
			   DiscoveredReader_t *drp,
			   Participant_t      *pp);

/* Add a subscription to the CCD matched subscriptions writer.
   On entry/exit: DP, DW(dwp) locked. */

void cdd_subscription_remove (Domain_t *dp, GUID_t *gp);

/* Remove a subscription from the CDD subscription notification channel. */

#endif /* !__disc_cdd_h_ */

