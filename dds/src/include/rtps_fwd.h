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

/* rtps_fwd.h -- RTPS forwarding engine with hybrid bridge/router functionality
		 using the following mechanisms for relaying frames:

	- GUID-prefix learning of received RTPS messages.
	- SPDP messages are both received locally as well as forwarded to all
	  ports except the receiving port.
	- Meta/user data messages that include the InfoDestination submessage
	  are either received locally (local GUID-prefix) or are forwarded to
	  the port on which the GUID-prefix was learned.
	- Multicast Meta/user data messages, or meta/user data messages without
	  an embedded InfoDestination submessage are forwarded by examining the
	  local discovery database and deriving from that data the interested
	  participants and the associated ports they own.  This can be a
	  combination of the message being received locally and in addition
	  being forwarded to other ports from the receiving port.
 */

#ifndef __rtps_fwd_h_
#define __rtps_fwd_h_

#include "rtps_mux.h"

/* Following variables/functions are only available if build with -DDDS_FORWARD */


/* Functions used by the RTPS transport multiplexer.
   ------------------------------------------------- */

RMRXF rfwd_init (RMRXF rxfct, MEM_DESC msg_md, MEM_DESC el_md);

/* Initialize the forwarding engine and returns the forwarder receive function. */

void rfwd_final (void);

/* Cleanup all forwarding related resources. */

int rfwd_locators_update (DomainId_t domain_id, unsigned id);

/* Start updating locator lists. */

int rfwd_locator_add (unsigned      domain_id,
		      LocatorNode_t *locator,
		      unsigned      id,
		      int           serve);

/* Add a port to the forwarder based on the given port attributes.
   If not successful, a non-zero error code is returned. */

void rfwd_locator_remove (unsigned id, LocatorNode_t *locator);

/* Remove a port from the forwarder based on the given locator and id. */

void rfwd_send (unsigned id, void *dest, int dlist, RMBUF *msgs);

/* Send a number of messages to the specified locator(s). */


/* Functions used by Discovery.
   ---------------------------- */

void rfwd_participant_new (Participant_t *p, int update);

/* A new DDS domain participant was discovered, or it was updated. */

void rfwd_participant_dispose (Participant_t *p);

/* A DDS domain participant was removed. */


/* Function used by DDS Debugger.
   ------------------------------ */

void rfwd_dump (void);

/* Dump forwarding related information. */

void rfwd_trace (int ntraces);

/* Start forwarding tracing. 
   If ntraces < 0: toggles on/off. If ntraces == 0: switch off tracing.
   if ntraces > 0: trace the specified number of events. */

#endif /* !__rtps_fwd_h_ */

