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

/* di_data.h -- Common Dynamic IP data and functions. */

#ifndef __di_data_h_
#define __di_data_h_

#include "dynip.h"

typedef struct ip_intf_st IP_Intf_t;
typedef struct ip_addr_st IP_Addr_t;


/* 1. Functions that should be used to propagate interface changes.
   ---------------------------------------------------------------- */

IP_Intf_t *di_intf_lookup (unsigned ifindex);

/* Lookup a link with the given index. */

IP_Intf_t *di_intf_new (unsigned ifindex);

/* Create a new link with the given interface index. */

void di_intf_update (IP_Intf_t *ifp,
		     int       up,
		     int       allow,
		     int       allow6);

/* Should be called when a link status change was detected. */

void di_intf_removed (IP_Intf_t *ifp);

/* An IP interface has disappeared. */

void di_intf_filter_update (void);

/* An interface filter has been updated. */


/* 2. Functions that should be used to propagate address changes.
   -------------------------------------------------------------- */

void di_add_addr (IP_Intf_t *ifp, unsigned family, unsigned char *ipa);

/* Add a new address. */

void di_remove_addr (IP_Intf_t *ifp, unsigned family, unsigned char *ipa);

/* Removes an existing address. */


/* 3. Functions to indicate begin and end of the event handler.
   ------------------------------------------------------------ */

void di_evh_begin (void);

/* Should be called before any changes are done from the event handler. */

void di_evh_end	(void);

/* Should be called after any changes are done from the event handler. */


/* 4. System specific functions.
   ----------------------------- */

int di_sys_init (void);

/* Should do various protocol-independent initializations. */

int di_sys_attach (unsigned      family, 
		   unsigned char *ipa,
		   unsigned      *n,
		   unsigned      max,
		   Scope_t       min_scope,
		   Scope_t       max_scope,
		   DI_NOTIFY     fct);

/* Do protocol-specific initializations and may start propagating family-
   specific address changes. */

int di_sys_detach (unsigned family);

/* Stop propagating events for the protocol family. */

void di_sys_final (void);

/* Finalize everything. */

#endif /* !__di_data_h_ */
