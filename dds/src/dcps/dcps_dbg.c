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

/* dcps_dbg.c -- DCPS API - Debug functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "pool.h"
#include "error.h"
#include "prof.h"
#include "dds/dds_dcps.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "debug.h"
#include "dcps_dbg.h"

#ifdef DDS_DEBUG

static int ep_dump_fct (Skiplist_t *list, void *node, void *arg)
{
	LocalEndpoint_t	*ep, **epp = (LocalEndpoint_t **) node;
	DomainId_t	*dip = (DomainId_t *) arg;
	int		n;

	ARG_NOT_USED (list)

	ep = *epp;
	dbg_printf ("    %u/%02x%02x%02x-%02x %c{%u}\t",
			*dip,
			ep->ep.entity_id.id [0], 
			ep->ep.entity_id.id [1],
			ep->ep.entity_id.id [2],
			ep->ep.entity_id.id [3],
			entity_writer (entity_type (&ep->ep.entity)) ? 'W' : 'R',
			ep->ep.entity.handle);
	dbg_printf ("%u", hc_total_changes (ep->cache));
	if ((n = hc_total_instances (ep->cache)) >= 0)
		dbg_printf ("/%d", n);
	dbg_printf ("\t%s/%s\r\n", str_ptr (ep->ep.topic->name),
				   str_ptr (ep->ep.topic->type->type_name));
	return (1);
}

/* dcps_endpoints_dump -- Display the registered endpoints. */

void dcps_endpoints_dump (void)
{
	Domain_t	*dp;
	unsigned	n, i = 0;

	n = domain_count ();
	while ((dp = domain_next (&i, NULL)) != NULL) {
		if (n > 1) {
			dbg_print_guid_prefix (&dp->participant.p_guid_prefix);
			dbg_printf (":\r\n");
		}
		sl_walk (&dp->participant.p_endpoints,
			 ep_dump_fct,
			 &dp->domain_id);
	}
}

/* dcps_cache_dump -- Dump the cache of a single endpoint. */

void dcps_cache_dump (Endpoint_t *p)
{
	LocalEndpoint_t	*ep = (LocalEndpoint_t *) p;

	hc_cache_dump (ep->cache);
}

/* dcps_pool_dump -- Display some pool statistics. */

void dcps_pool_dump (size_t sizes [])
{
	print_pool_table (dcps_mem_blocks, (unsigned) MB_END, sizes);
}

#endif


