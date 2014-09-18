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

/* disc_ep.h -- Defines the interface to the management of Discovery Endpoints. */

#ifndef __disc_ep_h_
#define __disc_ep_h_

#include "locator.h"
#include "dds_data.h"

typedef struct change_data_t {
	ChangeKind_t	kind;
	void		*data;
	int		is_new;
	InstanceHandle	h;
	handle_t	writer;
	FTime_t		time;
} ChangeData_t;

DDS_ReturnCode_t disc_get_data (Reader_t *rp, ChangeData_t *c);

/* Get a discovery protocol message from the history cache. */

void disc_data_available (uintptr_t user, Cache_t cdp);

/* Data available indication from cache. */

int create_builtin_endpoint (Domain_t            *dp,
			     BUILTIN_INDEX       index,
			     int                 push_mode,
			     int                 stateful,
			     int                 reliable,
			     int                 keep_all,
			     int                 transient_local,
			     const Duration_t    *resend_per,
			     const LocatorList_t uc_locs,
			     const LocatorList_t mc_locs,
			     const LocatorList_t dst_locs);

/* Create a built-in endpoint with the given parameters.
   On entry/exit: no locks taken. */

void disable_builtin_endpoint (Domain_t *dp, BUILTIN_INDEX index);

/* Turn of notifications on a builtin endpoint, and purge outstanding
   notifications. On entry/exit: domain and global lock taken. */

void delete_builtin_endpoint (Domain_t *dp, BUILTIN_INDEX index);

/* Remove a builtin endpoint. On entry/exit: domain and global lock taken. */

void connect_builtin (Domain_t      *dp,
		      BUILTIN_INDEX l_index,
		      Participant_t *rpp,
		      BUILTIN_INDEX r_index);

/* Connect a local builtin endpoint to a remote. On entry/exit: DP locked. */

void disconnect_builtin (Domain_t      *dp,
			 BUILTIN_INDEX l_index,
			 Participant_t *rpp,
			 BUILTIN_INDEX r_index);

/* Disconnect a local builtin from a remote. On entry/exit: DP locked. */

#endif /* !__disc_ep_h_ */


