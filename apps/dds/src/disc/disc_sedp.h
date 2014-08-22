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

/* disc_sedp -- Interface to the SEDP Discovery protocol functions. */

#ifndef __disc_sedp_h_
#define __disc_sedp_h_

#include "dds_data.h"

extern int sedp_log;

int sedp_start (Domain_t *dp);

/* Setup the SEDP protocol builtin endpoints and start the protocol operation.
   On entry/exit: no locks taken, */

void sedp_disable (Domain_t *dp);

/* Stop the SEDP protocol on the participant.
   On entry/exit: domain and global lock taken */

void sedp_stop (Domain_t *dp);

/* Stop the SEDP protocol on the participant.
   On entry/exit: domain and global lock taken. */

void sedp_connect (Domain_t *dp, Participant_t *rpp);

/* Connect this participant to a peer participant.
   On entry/exit: DP locked. */

void sedp_disconnect (Domain_t *dp, Participant_t *rpp);

/* Disconnect this participant from a peer participant.
   On entry/exit: DP locked. */

void sedp_subscription_event (Reader_t           *rp,
			      NotificationType_t t,
			      int                cdd,
			      int                secure);

/* Receive a subscription event. On entry/exit: DP, R(rp) locked. */

void sedp_publication_event (Reader_t *rp,
			     NotificationType_t t,
			     int cdd,
			     int secure);

/* Receive a publication event. On entry/exit: DP, R(rp) locked. */

void sedp_topic_event (Reader_t *rp, NotificationType_t t);

/* Receive a topic change from a remote participant. */


int sedp_writer_add (Domain_t *domain, Writer_t *wp);

/* Add a local writer. On entry/exit: all locks taken (DP,P,T,W). */

int sedp_writer_update (Domain_t             *domain,
			Writer_t             *wp,
			int                  changed,
			DDS_InstanceHandle_t peer);

/* Update a local writer. On entry/exit: all locks taken (DP,P,T,W). */

int sedp_writer_remove (Domain_t *dp, Writer_t *wp);

/* Remove a local writer. On entry/exit: all locks taken (DP,P,T,W). */


int sedp_reader_add (Domain_t *dp, Reader_t *rp);

/* Add a local reader. On entry/exit: all locks taken (DP,S,T,R). */

int sedp_reader_update (Domain_t             *dp,
			Reader_t             *rp,
			int                  changed,
			DDS_InstanceHandle_t peer);

/* Update a local reader. On entry/exit: all locks taken (DP,S,T,R). */

int sedp_reader_remove (Domain_t *dp, Reader_t *rp);

/* Remove a local reader. On entry/exit: all locks taken (DP,S,T,R). */


int sedp_topic_add (Domain_t *dp, Topic_t *tp);

/* Add a local topic. */

int sedp_topic_update (Domain_t *dp, Topic_t *tp);

/* Update a local topic. */

int sedp_topic_remove (Domain_t *dp, Topic_t *tp);

/* Remove a local topic. */


int sedp_unmatch_peer_endpoint (Skiplist_t *list, void *node, void *arg);

/* If the endpoint matches one of ours, end the association since the peer
   participant has gone. */

int sedp_endpoint_locator (Domain_t        *domain,
			   LocalEndpoint_t *ep,
			   int             add,
			   int             mcast,
			   const Locator_t *loc);

/* Add/remove a locator to/from an endpoint. */

#endif /* !__disc_sedp_h_ */

