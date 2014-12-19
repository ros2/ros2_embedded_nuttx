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

/* di_windows -- Dynamic IP address handler for windows. */

#include <stdio.h>
#include "win.h"
#include "Ws2IpDef.h"
#include "Ws2tcpip.h"
#include "dds/dds_error.h"
#include "error.h"
#include "di_data.h"

typedef struct ip_ctrl_st {
	unsigned char	*ipa;
	unsigned	*n;
	unsigned	max;
	Scope_t		min_scope;
	Scope_t		max_scope;
	DI_NOTIFY	fct;
} IP_CTRL;

static IP_CTRL	ipv4;
#ifdef DDS_IPV6
static IP_CTRL	ipv6;
#endif

/* di_sys_init -- Initialize dynamic IP handling. */

int di_sys_init (void)
{
	ipv4.fct = NULL;
#ifdef DDS_IPV6
	ipv6.fct = NULL;
#endif
	return (DDS_RETCODE_OK);
}

/* di_event -- Event handler for changes. */

void di_event (void)
{
	if (ipv4.fct) {
		*ipv4.n = sys_own_ipv4_addr (ipv4.ipa, ipv4.max,
					     ipv4.min_scope, ipv4.max_scope);
		(*ipv4.fct)();
	}
#ifdef DDS_IPV6
	if (ipv6.fct) {
		*ipv6.n = sys_own_ipv6_addr (ipv6.ipa, ipv6.max,
					     ipv6.min_scope, ipv6.max_scope);
		(*ipv6.fct)();
	}
#endif
}

/* di_sys_attach -- Attach the event handler for the given family. */

int di_sys_attach (unsigned      family,
		   unsigned char *ipa,
		   unsigned      *n,
		   unsigned      max,
		   Scope_t       min_scope,
		   Scope_t       max_scope,
		   DI_NOTIFY     fct)
{
	if (family == AF_INET) {
		ipv4.ipa = ipa;
		ipv4.n = n;
		ipv4.max = max;
		ipv4.min_scope = min_scope;
		ipv4.max_scope = max_scope;
		ipv4.fct = fct;
	}
#ifdef DDS_IPV6
	else if (family == AF_INET6) {
		ipv6.ipa = ipa;
		ipv6.n = n;
		ipv6.max = max;
		ipv6.min_scope = min_scope;
		ipv6.max_scope = max_scope;
		ipv6.fct = fct;
	}
#endif
	else
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

/* di_sys_detach -- Detach the event handler from the given family. */

int di_sys_detach (unsigned family)
{
	if (family == AF_INET)
		ipv4.fct = NULL;
#ifdef DDS_IPV6
	else if (family == AF_INET6)
		ipv6.fct = NULL;
#endif
	else
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

/* di_sys_final -- Finalize all event handling and cleanup. */

void di_sys_final (void)
{
	ipv4.fct = NULL;
#ifdef DDS_IPV6
	ipv6.fct = NULL;
#endif
}
