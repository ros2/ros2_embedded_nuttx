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

/* dynip.h -- Interface to the dynamic IP address handler. */

#ifndef __dynip_h_
#define __dynip_h_

#include "sys.h"
#include "ipfilter.h"

typedef int (*DI_NOTIFY) (void);

/* Notification callback for address changes. */

int di_init (void);

/* Initialize the Dynamic IP address handler.  If not successful, a non-0 error
   code is returned. */

void di_final (void);

/* Finalize, i.e. remove the connection to the IP address handler. */

int di_attach (unsigned      family,
	       unsigned char *ipa,
	       unsigned      *n,
	       unsigned      max,
	       Scope_t       min_scope,
	       Scope_t       max_scope,
	       DI_NOTIFY     fct);

/* Attach a notification handler to the Dynamic address system for the given
   address family. The {ipa, n} parameters refer to the current list of
   IP addresses, where ipa is a pointer to the first address and n is the number
   of these addresses. 
   The max parameter specifies the maximum number of supported addresses.
   The fct parameter is the notification callback function. */

void di_detach (unsigned family);

/* Detach the notification handler from the Dynamic address system. */

void di_filter_update (void);

/* Take into account interface filter changes. */

void di_dump (void);

/* Dump the current dynamic IP address contexts. */

unsigned di_ipv6_intf (unsigned char *addr);

/* Return an IPv6 interface index from an IPv6 address. */

#endif /* !__dynip_h_ */

