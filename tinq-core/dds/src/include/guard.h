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

/* guard.h -- Add/remove liveliness/deadline checks on endpoints. */

#ifndef __guard_h_
#define	__guard_h_

#include "domain.h"

/* 1. Liveliness checks. */
/* --------------------- */

#define liveliness_used(qp) ((qp)->liveliness_kind != DDS_AUTOMATIC_LIVELINESS_QOS || \
			     ((qp)->liveliness_lease_duration.sec != DDS_DURATION_INFINITE_SEC && \
			      (qp)->liveliness_lease_duration.nanosec != DDS_DURATION_INFINITE_NSEC))

/* Check if a liveliness protocol is required on a given endpoint and return a
   non-0 result if so. */

int liveliness_enable (Endpoint_t *wp, Endpoint_t *rp);

/* Enable liveliness checks on the association between the given endpoints. */

void liveliness_disable (Endpoint_t *wp, Endpoint_t *rp);

/* Disable the checks that were previously enabled with liveliness_enable(). */

void liveliness_participant_event (Participant_t *pp, int manual);

/* Must be called when a liveliness participant message indicating automatic or
   manual liveliness was received. */

void liveliness_participant_assert (Domain_t *dp);

/* Assert liveliness on a local Domain Participant. */

void liveliness_participant_asserted (Participant_t *pp);

/* Must be called when a liveliness assertion was received. */

void liveliness_restored (LocalEndpoint_t *p, handle_t writer);

/* Liveliness of a local endpoint was restored by an endpoint action such as
   assert_liveliness(), write(), unregister() or dispose(). */


/* 2. Deadline checks. */
/* ------------------- */

#define	deadline_used(qp) ((qp)->deadline.period.sec != DDS_DURATION_INFINITE_SEC && \
			   (qp)->deadline.period.nanosec != DDS_DURATION_INFINITE_NSEC)

int deadline_enable (Endpoint_t *wp, Endpoint_t *rp);

/* Enable deadline checks on the association between the two endpoints. */

void deadline_disable (Endpoint_t *wp, Endpoint_t *rp);

/* Disable deadline checks on the association between the two endpoints. */

void deadline_continue (LocalEndpoint_t *ep);

/* Restart deadline checks on the endpoint. */


/* 3. Lifespan checks. */
/* ------------------- */

#define	lifespan_used(qp) ((qp)->lifespan.duration.sec != DDS_DURATION_INFINITE_SEC && \
			   (qp)->lifespan.duration.nanosec != DDS_DURATION_INFINITE_NSEC)

int lifespan_enable (Endpoint_t *wp, Endpoint_t *rp);

/* Enable lifespan checks on the matching endpoints. */

void lifespan_disable (Endpoint_t *wp, Endpoint_t *rp);

/* Disable lifespan checks on the matching endpoints. */

void lifespan_continue (LocalEndpoint_t *ep);

/* Restart lifespan checks on the endpoint. */


/* 4. Autopurge No-writers checks. */
/* ------------------------------- */

#define	autopurge_no_writers_used(rp) \
	((rp)->r_data_lifecycle.autopurge_nowriter_samples_delay.sec != \
						    DDS_DURATION_INFINITE_SEC && \
	 (rp)->r_data_lifecycle.autopurge_nowriter_samples_delay.nanosec != \
	 					    DDS_DURATION_INFINITE_NSEC)

int autopurge_no_writers_enable (Reader_t *rp);

/* Enable autopurge no-writers checks on the matching endpoints. */

void autopurge_no_writers_disable (Reader_t *rp);

/* Disable autopurge no-writers checks on the matching endpoints. */

void autopurge_no_writers_continue (LocalEndpoint_t *ep);

/* Restart autopurge no-writers checks on the endpoint. */


/* 5. Autopurge Disposed checks. */
/* ----------------------------- */

#define	autopurge_disposed_used(rp) \
	((rp)->r_data_lifecycle.autopurge_disposed_samples_delay.sec != \
						    DDS_DURATION_INFINITE_SEC && \
	 (rp)->r_data_lifecycle.autopurge_disposed_samples_delay.nanosec != \
	 					    DDS_DURATION_INFINITE_NSEC)

int autopurge_disposed_enable (Reader_t *rp);

/* Enable autopurge disposed checks on the matching endpoints. */

void autopurge_disposed_disable (Reader_t *rp);

/* Disable autopurge disposed checks on the matching endpoints. */

void autopurge_disposed_continue (LocalEndpoint_t *ep);

/* Restart autopurge disposed checks on the endpoint. */

#endif /* !__guard_h_ */

