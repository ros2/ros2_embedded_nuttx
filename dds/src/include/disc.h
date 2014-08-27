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

/* disc.h -- Defines the interface to the SPDP and SEDP discovery protocols
	     that are used to discover new participants and endpoints. */

#ifndef __disc_h_
#define __disc_h_

#include "domain.h"
#include "pid.h"

int disc_init (void);

/* Initialize the Discovery subsystem. */

void disc_final (void);

/* Finalize the Discovery subsystem. */

int disc_start (Domain_t *domain);	/* Domain. */

/* Start all the discovery protocols on the participant. */

void disc_stop (Domain_t *domain);

/* Stop the discovery protocols on the participant. */

typedef int (*DMATCHFCT) (LocalEndpoint_t *lep, const Endpoint_t *ep);

/* Notification function for higher layers to follow up on a Discovery match.
   It is up to the higher layer to return either 0, i.e. no RTPS endpoint, or
   return a non-0 value indicating that a valid RTPS endpoint exists.
   Only if a valid endpoint is indicated will the appropriate RTPS proxy be
   created. */

typedef int (*DUMATCHFCT) (LocalEndpoint_t *lep, const Endpoint_t *ep);

/* Same semantics as the previous function, but for following up on unmatching
   of endpoints.  The return value should indicate whether the upper layer
   wishes to be informed after the RTPS proxy is removed (non-0: notify). */

typedef void (*DUMDONEFCT) (LocalEndpoint_t *lep);

/* Final follow-up function after proxy deletion was done. */

void disc_register (DMATCHFCT  notify_match,
		    DUMATCHFCT notify_unmatch,
		    DUMDONEFCT notify_done);

/* Register discovery notification functions. */

int disc_participant_update (Domain_t *domain);

/* Specifies that a domain participant was updated. */

int disc_writer_add (Domain_t *domain, Writer_t *wp);

/* Specifies that a new writer endpoint was added. */

int disc_writer_update (Domain_t             *domain,
			Writer_t             *wp,
			int                  changed,
			DDS_InstanceHandle_t peer);

/* Specifies that a writer endpoint was updated. */

int disc_writer_remove (Domain_t *domain, Writer_t *wp);

/* Specifies that a writer endpoint was removed. */

int disc_reader_add (Domain_t *domain, Reader_t *rp);

/* Specifies that a new reader endpoint was added. */

int disc_reader_update (Domain_t             *domain,
			Reader_t             *rp,
			int                  changed,
			DDS_InstanceHandle_t peer);

/* Specifies that a reader endpoint was updated. */

int disc_reader_remove (Domain_t *domain, Reader_t *rp);

/* Specifies that a reader endpoint was removed. */

int disc_topic_add (Domain_t *domain, Topic_t *tp);

/* Specifies that a Topic was added locally. */

int disc_topic_remove (Domain_t *domain, Topic_t *tp);

/* Specifies that a Topic was removed locally. */

int disc_endpoint_locator (Domain_t         *domain,
		           LocalEndpoint_t  *ep,
			   int              add,
			   int              mcast,
			   const Locator_t  *loc);

/* Add/remove a locator to/from an endpoint. */

int disc_ignore_participant (Participant_t *pp);

/* Ignore a discovered participant. */

int disc_ignore_topic (Topic_t *tp);

/* Ignore a discovered topic. */

int disc_ignore_writer (DiscoveredWriter_t *wp);

/* Ignore a discovered writer. */

int disc_ignore_reader (DiscoveredReader_t *rp);

/* Ignore a discovered reader. */


/* Liveliness support functions: */
/* ----------------------------- */

int disc_send_liveliness_msg (Domain_t *dp, unsigned kind);

/* Send either a manual or automatic liveliness message. */

int disc_send_participant_liveliness (Domain_t *dp);

/* Resend Asserted Participant liveliness. */


/* Manual Discovery support functions: */
/* ----------------------------------- */

Participant_t *disc_remote_participant_add (Domain_t                      *domain,
					    SPDPdiscoveredParticipantData *data,
					    LocatorList_t                 srcs,
					    int                           authorize);

/* Add a new discovered Participant as if it was discovered via one of the
   Discovery protocols. */

void disc_remote_participant_enable (Domain_t *dp, Participant_t *pp, unsigned secret);

/* Enable a discovered participant, when it was previously ignored. */

Topic_t *disc_remote_topic_add (Participant_t *pp,
				DiscoveredTopicData *data);

/* Add a new discovered Topic as if it was discovered via one of the Discovery
   protocols. */

DiscoveredReader_t *disc_remote_reader_add (Participant_t *pp,
					    DiscoveredReaderData *data);

/* Add a new discovered Reader as if it was discovered via one of the Discovery
   protocols. */

DiscoveredWriter_t *disc_remote_writer_add (Participant_t *pp,
					    DiscoveredWriterData *data);

/* Add a new discovered Writer as if it was discovered via one of the Discovery
   protocols. */

int disc_populate_builtin (Domain_t *dp, Builtin_Type_t type);

/* Add already discovered data to a builtin reader. */


/* Security functionality: */
/* ----------------------- */

int disc_participant_rehandshake (Domain_t *domain, int notify_only);


/* Suspend & Resume functionality: */
/* ------------------------------- */

void disc_suspend (void);

/* Suspend discovery. */

void disc_resume (void);

/* Resume discovery. */


/* Trace & Debug functionality: */
/* ---------------------------- */

void disc_pool_dump (size_t sizes []);

/* Dump the Discovery pools. */

void disc_dump (int full);

/* Debug: dump the discovered participants and endpoints. */

#endif /* !__disc_h_ */

