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

/* rtps_mux -- RTPS transport multiplexer. */

#ifndef __rtps_mux_
#define	__rtps_mux_

#include "pool.h"
#include "locator.h"
#include "rtps_data.h"

extern unsigned rtps_forward;

typedef void (* RMRXF) (unsigned        id,	/* Participant id. */
			RMBUF	        *msgs,	/* List of messages. */
			const Locator_t *src);	/* Medium type. */

/* Callback function to indicate the reception of a number of RTPS messages. */

typedef void (* RMNOTF) (NOTIF_DATA *data);

/* Callback function for element notifications. */

typedef int (* RMINITF) (RMRXF		rxfct,		/* Receive function. */
			 MEM_DESC	msg_hdr,	/* Header pool. */
			 MEM_DESC	msg_elem);	/* Element pool. */

/* Transport initialisation function. */

typedef void (* RMFINALF) (void);

/* Transport closedown function. */

typedef int (* RMPARSSF) (LocatorKind_t kind, const void *pars);

/* Transport parameters set function. */

typedef int (* RMPARSGF) (LocatorKind_t kind, void *pars, size_t msize);

/* Transport parameters get function. */

typedef enum {
	RTLT_USER,		/* User data locators. */
	RTLT_SPDP_SEDP		/* SPDP/SEDP endpoint locators. */
} RTPS_LOC_TYPE;

typedef void (* RMLOCGETF) (DomainId_t    domain_id,
			    unsigned      participant_id,
			    RTPS_LOC_TYPE type,
			    LocatorList_t *uc,
			    LocatorList_t *mc,
			    LocatorList_t *dst);

/* Add protocol specific locators to the given locator lists uc, mc and dst,
   derived from the given parameters (domain_id & participant_id) for the
   given type. */

typedef int (* RMLOCADDF) (DomainId_t    domain_id,
			   LocatorNode_t *locator,
			   unsigned      id,
			   int           serve);

/* Add a protocol locator. */

typedef void (* RMLOCREMF) (unsigned id, LocatorNode_t *locator);

/* Remove a previously added locator. */

typedef void (* RMSENDF) (unsigned id, void *dest, int dlist, RMBUF *msgs);

/* Send function for RTPS messages.
   Note: Transmission can be done at any time (the sooner the better), but when
         done, the transport handler *must* call rtps_free_messages(). */

typedef unsigned (* RMUPDATEF) (LocatorKind_t kind, Domain_t *dp, int done);

/* Update begin/done function. */

typedef struct rtps_transport_st {
	LocatorKind_t	kind;		/* Transport kind. */
	RMINITF		init_fct;	/* Initializer. */
	RMFINALF	final_fct;	/* Finalizer. */
	RMPARSSF	pars_set_fct;	/* Set parameters function. */
	RMPARSGF	pars_get_fct;	/* Get parameters function. */
	RMLOCGETF	loc_get_fct;	/* Locator get function. */
	RMLOCADDF	loc_add_fct;	/* Locator add function. */
	RMLOCREMF	loc_rem_fct;	/* Locator remove function. */
	RMUPDATEF	loc_upd_fct;	/* Update locators function. */
	RMSENDF		send_fct;	/* Send data messages function. */
} RTPS_TRANSPORT;

extern const RTPS_UDP_PARS rtps_udp_def_pars;	/* Default UDP parameters. */
extern LocatorKind_t rtps_mux_mode;		/* Default transport medium. */

void rtps_mux_init (RMRXF rxfct,
		    RMNOTF notifyfct,
		    MEM_DESC hdr,
		    MEM_DESC elem,
		    MEM_DESC ref);

/* Initialize the RTPS mux with the parameters needed for the various transport
   protocol handlers. */

void rtps_mux_final (void);

/* Finalize the RTPS transport multiplexer. */

int rtps_transport_add (const RTPS_TRANSPORT *transport);

/* Add a transport subsystem to the RTPS multiplexer. */

void rtps_transport_remove (const RTPS_TRANSPORT *transport);

/* Remove a transport subsystem from the RTPS multiplexer. */

void rtps_transport_locators (DomainId_t    domain_id,
			      unsigned      participant_id,
			      RTPS_LOC_TYPE type,
			      LocatorList_t *uc,
			      LocatorList_t *mc,
			      LocatorList_t *dst);

/* Retrieve protocol specific locators for the specified use case in
   uc/mc/dst.  The first two are for reception purposes, the last one is
   as a default destination for a writer. */

int rtps_parameters_set (LocatorKind_t kind, const void *pars);

/* Set transport parameters. */

int rtps_parameters_get (LocatorKind_t kind, void *pars, size_t msize);

/* Get transport parameters. */

int rtps_locators_update (DomainId_t domain_id, unsigned id);

/* Start updating locator lists. */

int rtps_locator_add (DomainId_t    domain_id,
		      LocatorNode_t *locator,
		      unsigned      id,
		      int           serve);

/* Add an active locator. */

void rtps_locator_remove (unsigned id, LocatorNode_t *locator);

/* Remove an active locator. */

unsigned rtps_update_begin (Domain_t *dp);

/* This function is handy when a lot of locators need to be updated.
   It marks each existing connection as redundant, so that after a number of
   calls to rtps_locator_add () that implicitly clear this flag, all remaining
   connections with the flag still set can be cleaned up with the
   rtps_update_end () function.
   The advantage of using this mechanism is that connections that are still
   needed are not accidentally reset, which would be the case if we would simply
   be clearing and recreating the connections.
   Returns the number of existing locators. */

unsigned rtps_update_end (Domain_t *dp);

/* Done updating the locators.  The connections still marked as redundant can
   now be cleaned up.  Returns the number of existing locators. */

void rtps_locator_send (unsigned id, void *dest, int dlist, RMBUF *msgs);

/* Send a number of messages to the specified locator(s). If dlist, then dest is
   of type (LocatorList_t) otherwise it is of type (Locator_t *).
   The id parameter specifies the domain participant index.
   If forwarding is enabled, the forwarder may override the destinations,
   depending on learned information. When sending is complete, the messages will
   be freed automatically. */

void rtps_locator_send_ll (unsigned id, void *dest, int dlist, RMBUF *msgs);

/* Similar to rtps_locator_send, but sends messages to lower layers directly
   without involving the forwarder and without freeing them. */

void rtps_free_elements (RME *mep);

/* Release a list of message elements. */

void rtps_free_messages (RMBUF *mp);

/* Release a list of messages. */

RMBUF *rtps_copy_message (RMBUF *mp);

/* Make an exact copy of a single message.
   This function should only be used when a message needs to be modified.
   If a message needs to be referenced only, without having to be changed,
   the rtps_ref_message() function should be used instead, since it's a lot
   faster. */

RMBUF *rtps_copy_messages (RMBUF *msgs);

/* Make an exact copy of a list of messages. */

RMREF *rtps_ref_message (RMBUF *mp);

/* Get a message reference to a single message.  Message references can be
   used to queue messages in multiple distinct lists. */

RMREF *rtps_ref_messages (RMBUF *mp);

/* Get a message reference list from a message list. */

void rtps_unref_message (RMREF *mp);

/* Cleanup a previously created message reference. */

void rtps_unref_messages (RMREF *mp);

/* Cleanup a list of message references. */

int rtps_local_node (Participant_t *pp, Locator_t *src);

/* Returns a non-0 value if the Participant is directly reachable, i.e. without
   passing thru a relay node. */

void rtps_log_message (unsigned id,
		       unsigned level,
		       RMBUF    *mp,
		       char     dir,
		       int      data);

/* Log a single message.  The dir character should be either 'T', 'R' or 'F'
   to specify transmitted, received and forwarded data respectively.
   If data is set, message data will be dumped completely.*/

void rtps_log_messages (unsigned id,
		        unsigned level,
		        RMBUF    *msgs,
			char     dir,
			int      data);

/* Log a list of messages. */

#endif /* !__rtps_mux_ */

