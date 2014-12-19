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

/* dcps_event.h -- Defines the interface to the DCSP event mechanism. */

#ifndef __dcps_event_h_
#define __dcps_event_h_

#include "dds_data.h"

int dcps_notify_match (LocalEndpoint_t *lep, const Endpoint_t *ep);

/* Used by Discovery to inform DCPS on the status of matched endpoints. */

int dcps_notify_unmatch (LocalEndpoint_t *lep, const Endpoint_t *ep);

/* Used by Discovery to inform DCPS on the status of unmatched endpoints. */

void dcps_notify_done (LocalEndpoint_t *lep);

/* Used by Discovery to inform DCPS that the last proxy of an RTPS endpoint
   was removed.

   Locks: On entry, the endpoint, its publisher/subscriber, topic and domain
   should be locked (inherited from rtps_writer_delete/rtps_reader_delete) */


void dcps_notify_listener (Entity_t *ep, NotificationType_t t);

/* Call a listener on the given entity of the type. */

void dcps_data_available (uintptr_t user, Cache_t cdp);

/* Data available indication from cache. */

int dcps_data_available_listener (Reader_t *rp);

/* Returns whether there is an active Data Available listener. */

void dcps_update_listener (Entity_t       *ep, 
			   lock_t         *elock,
			   unsigned short *old_mask, 
			   void           *old_listener_struct, 
			   DDS_StatusMask new_mask, 
			   const void     *new_listener_struct);

/* Update an existing listener.  Called with entity lock taken. */

#endif /* !__dcps_event_h_ */

