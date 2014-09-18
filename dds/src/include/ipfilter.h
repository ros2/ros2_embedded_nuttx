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

/* ipfilter.h -- Utility functions for creating/querying IP address/network
		 filters. 

   <filter> 	 = <filter_item> { ';' <filter_item> }
   <filter_item> = [<negate>] <fspec>
   <negate>	 = '~' | '^'
   <fspec>	 = [<domain> ':'] <addr> | <addr_mask> | <addr_range>
   <addr_mask>   = <addr> ['/' <mask>]
   <addr_range>  = <addr> ['+' <number>]
 */

#ifndef __ipfilter_h_
#define __ipfilter_h_

typedef void *IpFilter_t;

#define	IPF_MASK	1	/* Allow subnet masks in filter specs. */
#define	IPF_DOMAIN	2	/* Allow domain specification in filters. */

IpFilter_t ip_filter_new (const char *s, unsigned flags, int dmatch);

/* Create an IP filters spec that is able to check for valid IP addresses. */

IpFilter_t ip_filter_add (IpFilter_t f, const char *s, unsigned flags);

/* Add extra filter specifications to an IP filters spec that is able to check
   for valid IP addresses. */

#define	IPF_DOMAIN_ANY	~0U

int ip_match (IpFilter_t f, unsigned did, unsigned char *ipa);

/* Validate an optional DDS Domain Id and IP address whether this combination
   may be used. */

void ip_filter_free (IpFilter_t fp);

/* Free a filter created with ip_filter_new(). */

void ip_filter_dump (IpFilter_t fp);

/* Dump a filter created with ip_filter_new(). */

#endif /* !__ipfilter_h_ */

