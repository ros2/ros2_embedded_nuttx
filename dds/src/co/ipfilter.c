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

/* ipfilter.c -- Utility functions for creating/querying IP address/network
                 filters. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif
#include "pool.h"
#include "ipfilter.h"

#define	ANY_DOMAIN	0x1ffU	/* Applies to any domain. */

typedef struct ipv4_fspec_st {
	unsigned	domain:9;	/* Domain Id. */
	unsigned	prefix:6;	/* Prefix mask length. */
	unsigned	deny:1;		/* Invert result. */
	unsigned	range:16;	/* # of extra addresses (range). */
	unsigned char	addr [4];	/* Address bytes. */
	unsigned char	mask [4];	/* Mask bytes. */
} IPv4FSpec_t;

typedef struct ipv4_filter_st {
	unsigned	nfspec;
	int		defmatch;
	IPv4FSpec_t	fspec [1];
} IPv4Filter_t;


/* getnum -- Parse a number. */

static void getnum (const char **sp,
		    unsigned   *n,
		    unsigned   min,
		    unsigned   max,
		    unsigned   base)
{
	unsigned	i, c, d, value = 0;

	for (i = 0; i < 10; i++) {
		c = **sp;
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else if (i) {
			*n = value;
			return;
		}
		else
			d = base;

		if (d >= base) {
			if (!i || value < min || value > max)
				*n = max + 1;
			else
				*n = value;
			return;
		}
		value = value * base + d;
		(*sp)++;
	}
	*n = max + 1;
}

/* ip_filter_add -- Add filter specifications to an existing filter. */

IpFilter_t ip_filter_add (IpFilter_t f, const char *s, unsigned flags)
{
	IPv4Filter_t	*ipp;
	IPv4FSpec_t	*sp;
	unsigned char	ipaddr [4], ipmask [4];
	unsigned	domain_id, n, i, mask, m, prefix, range;
	int		deny;

	ipp = (IPv4Filter_t *) f;
	if (!ipp) {
		ipp = mm_fcts.alloc_ (sizeof (IPv4Filter_t));
		if (!ipp)
			return (NULL);

		ipp->nfspec = 0;
		ipp->defmatch = 1;
	}
	for (;;) {
		memset (ipaddr, 0, 4);
		memset (ipmask, 255, 4);

		/* A specification can be either a blacklist item or a
		   whitelist item. If preceeded by either a '~' or a '^',
		   it means it's a blacklist item. */
		deny = (*s == '~' || *s == '^');
		if (deny)
			s++;

		/* Get an address or network spec.  A spec starts either with a
		   domain id or the first IP address byte. */
		i = 0;
		getnum (&s, &n, 0, 255, 10);
		if ((flags & IPF_DOMAIN) != 0 && *s == ':') {
			if (n > 230)
				break;

			domain_id = n;
			s++;
			getnum (&s, &n, 0, 256, 10);
		}
		else
			domain_id = ANY_DOMAIN;
		if (n < 256)
			ipaddr [i++] = n;
		else
			break;

		if ((flags & IPF_MASK) == 0 && *s != '.')
			break;

		while (*s == '.' && i < 4) {
			s++;
			getnum (&s, &n, 0, 255, 10);
			if (n < 256)
				ipaddr [i++] = n;
			else
				break;
		}
		if (n >= 256)
			break;

		if ((flags & IPF_MASK) == 0) {
			if (i == 2) {
				ipaddr [3] = ipaddr [1];
				ipaddr [1] = ipaddr [2] = 0;
			}
			else if (i == 3) {
				ipaddr [3] = ipaddr [2];
				ipaddr [2] = 0;
			}
		}
		if ((flags & IPF_MASK) != 0 && *s == '/') {
			range = 0;
			s++;
			getnum (&s, &n, 4, 31, 10);
			if (n > 32)
				break;

			if (n == 32) {
				prefix = 0;
				mask = 0;
			}
			else {
				prefix = n;
				mask = 0;
				for (i = 0, m = 0x80000000U; i < n; i++, m >>= 1)
					mask |= m;
			}
			ipmask [0] = mask >> 24;
			ipmask [1] = (mask >> 16) & 0xff;
			ipmask [2] = (mask >> 8) & 0xff;
			ipmask [3] = mask & 0xff;
		}
		else if ((flags & IPF_MASK) != 0 && *s == '+') {
			s++;
			prefix = 32;
			getnum (&s, &range, 1, 65535, 10);
			if (range > 65535)
				break;
		}
		else if (*s && *s != ';') {
			s--;
			break;
		}
		else {
			range = 0;
			prefix = 32;
		}

		/* Successful {DomainId, Address, [Mask]} retrieval.
		   Save it in the filter list. */
		if (!ipp->nfspec)
			sp = ipp->fspec;
		else {
			ipp = realloc (ipp, sizeof (IPv4Filter_t) +
					    sizeof (IPv4FSpec_t) * ipp->nfspec);
			if (!ipp) {
				printf ("ip_filter_new: not enough memory!\r\n");
				return (NULL);
			}
			sp = &ipp->fspec [ipp->nfspec];
		}
		ipp->nfspec++;
		sp->domain = domain_id;
		sp->prefix = prefix;
		sp->deny = deny;
		sp->range = range;
		memcpy (sp->addr, ipaddr, 4);
		memcpy (sp->mask, ipmask, 4);
		if (!*s || *s++ != ';')
			break;
	}
	if (*s) {
		printf ("ip_filter_new: error at %s\r\n", s);
		mm_fcts.free_ (ipp);
		ipp = NULL;
	}
	return (ipp);
}

/* ip_filter_new -- Create a new filter from the given parameters. */

IpFilter_t ip_filter_new (const char *s, unsigned flags, int defmatch)
{
	IPv4Filter_t	*ipp;

	ipp = (IPv4Filter_t *) ip_filter_add (NULL, s, flags);
	if (ipp)
		ipp->defmatch = defmatch;
	return (ipp);
}

/* ip_filter_free -- Free a filter created with ip_filter_new(). */

void ip_filter_free (IpFilter_t fp)
{
	mm_fcts.free_ (fp);
}

/* ip_filter_dump -- Dump a filter created with ip_filter_new(). */

void ip_filter_dump (IpFilter_t fp)
{
	IPv4Filter_t	*ipp;
	IPv4FSpec_t	*sp;
	unsigned	i;

	if (!fp) {
		printf ("no filter\n");
		return;
	}
	ipp = (IPv4Filter_t *) fp;
	for (i = 0, sp = ipp->fspec; i < ipp->nfspec; i++, sp++) {
		printf ("(%s: ", sp->deny ? "deny" : "allow");
		if (sp->domain == ANY_DOMAIN)
			printf ("any domain, ");
		else
			printf ("domain=%u, ", sp->domain);
		printf ("%u.%u.%u.%u", sp->addr [0], sp->addr [1],
				       sp->addr [2], sp->addr [3]);
		if (sp->range)
			printf ("+%u", sp->range);
		printf (" - %d", sp->prefix);
		if (sp->mask [0] != 0xff ||
		    sp->mask [1] != 0xff ||
		    sp->mask [2] != 0xff ||
		    sp->mask [3] != 0xff)
			printf (", mask %u.%u.%u.%u",
					sp->mask [0], sp->mask [1],
					sp->mask [2], sp->mask [3]);
		printf (")\n");
	}
}

/* ip_match -- An address is accepted if there are whitelist entries
	       and it matches one of them or there are no whitelist
	       entries.  In addition, if there are blacklist entries,
	       it must match none of them. */

int ip_match (IpFilter_t f, unsigned domain, unsigned char *ipa)
{
	IPv4Filter_t	*ipp = (IPv4Filter_t *) f;
	IPv4FSpec_t	*sp, *match_sp;
	uint32_t	ip, mip;
	unsigned	i;
	int		m;

	match_sp = NULL;
	for (i = 0, sp = ipp->fspec; i < ipp->nfspec; i++, sp++) {
		if (domain != IPF_DOMAIN_ANY && 
		    sp->domain != ANY_DOMAIN && 
		    sp->domain != domain)
			continue;

		memcpy (&ip, ipa, sizeof (uint32_t));
		memcpy (&mip, sp->addr, sizeof (uint32_t));
		if (sp->prefix < 32)
			m = (ipa [0] & sp->mask [0]) == sp->addr [0] &&
			    (ipa [1] & sp->mask [1]) == sp->addr [1] &&
			    (ipa [2] & sp->mask [2]) == sp->addr [2] &&
			    (ipa [3] & sp->mask [3]) == sp->addr [3];
		else if (!sp->range)
			m = (ip == mip);
		else {
			ip = ntohl (ip);
			mip = ntohl (mip);
			m = (ip >= mip && ip <= mip + sp->range);
		}
		if (m &&
		    (!match_sp ||
		      (match_sp && match_sp->prefix < sp->prefix)))
			match_sp = sp;
	}
	if (match_sp) {
		/*printf ("Matches with %u!\n", match_sp - ipp->fspec);*/
		return (!match_sp->deny);
	}
	else
		return (ipp->defmatch);
}

