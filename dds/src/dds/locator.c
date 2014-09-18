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

/* locator.c -- Implements common operations on Locators as used in both RTPS and
                in the various discovery protocols. */

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "win.h"
#include "Ws2IpDef.h"
#include "Ws2tcpip.h"
#include "Iphlpapi.h"
#elif defined (NUTTX_RTOS)
#include <arpa/inet.h>
#include <sys/socket.h> 
#else
#include <arpa/inet.h>
#endif
#include "error.h"
#include "config.h"
#include "skiplist.h"
#include "log.h"
#include "debug.h"
#include "ipfilter.h"
#include "hash.h"
#include "locator.h"

#ifndef DUMP_LOCATORS
/*#define LOG_LOCATORS	** Log locator primitives if set. */
#else
#define	LOG_LOCATORS
#endif
#define LOC_DUMP_COUNT

typedef struct loc_search_data_st {
	LocatorKind_t		kind;
	const unsigned char	*addr;
	uint32_t		port;
} LocSearchData;

enum mem_block_en {
	MB_LOCREF,		/* Locator Reference. */
	MB_LOCATOR,		/* Locator structure. */

	MB_END
};

static const char *mem_names [] = {
	"LOCREF",
	"LOCATOR",
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory used by driver. */
static size_t		mem_size;		/* Total memory allocated. */
static Skiplist_t	loc_list;		/* List of unique locator ptrs*/
static lock_t		loc_lock;		/* Lock on loc_list. */
static int		loc_force_no_mcast;	/* No multicast at all. */
static IpFilter_t	loc_no_mcast;		/* IP addresses without mcast.*/

unsigned char loc_addr_invalid [16] = LOCATOR_ADDRESS_INVALID;
Locator_t locator_invalid = LOCATOR_INVALID;


/* locator_pool_init -- Initialize the locator list pools. */

int locator_pool_init (const POOL_LIMITS *locrefs, const POOL_LIMITS *locators)
{
	const char	*env_str;

	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		return (LOC_OK);
	}
	if (!locrefs || !locrefs->reserved || !locators || !locators->reserved)
		return (LOC_ERR_PARAM);

	MDS_POOL_TYPE (mem_blocks, MB_LOCREF, *locrefs, sizeof (LocatorRef_t));
	MDS_POOL_TYPE (mem_blocks, MB_LOCATOR, *locators, sizeof (LocatorNode_t));

	/* Allocate all pools in one go. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		warn_printf ("locator_pool_init: not enough memory available!\r\n");
		return (LOC_ERR_NOMEM);
	}
	log_printf (LOC_ID, 0, "locator_pool_init: %lu bytes allocated for locators.\r\n", (unsigned long) mem_size);
#endif
	sl_init (&loc_list, sizeof (LocatorNode_t *));
	lock_init_nr (loc_lock, "Locators");

	if ((env_str = config_get_string (DC_IP_NoMCast, NULL)) != NULL) {
		if (!*env_str ||
		    !strcmp (env_str, "any") ||
		    !strcmp (env_str, "ANY")) {
			loc_force_no_mcast = 1;
			loc_no_mcast = NULL;
		}
		else {
			loc_force_no_mcast = 0;
			loc_no_mcast = ip_filter_new (env_str, IPF_DOMAIN | IPF_MASK, 0);
		}
	}
	return (LOC_OK);
}

/* locator_pool_free -- Free the locator list pools. */

void locator_pool_free (void)
{
	lock_destroy (loc_lock);
	mds_free (mem_blocks, MB_END);
}

static int loc_cmp (const void *np, const void *data)
{
	LocatorNode_t		*lnp, **lnpp = (LocatorNode_t **) np;
	const LocSearchData	*sp = (const LocSearchData *) data;

	lnp = *lnpp;
	if (lnp->locator.kind != sp->kind)
		return ((int) lnp->locator.kind - (int) sp->kind);

	if (lnp->locator.port != sp->port)
		return ((long) lnp->locator.port - (long) sp->port);

	return (memcmp (lnp->locator.address, sp->addr, 16));
}

void locator_lock (Locator_t *lp)
{
	ARG_NOT_USED (lp)

	lock_take (loc_lock);
}

void locator_release (Locator_t *lp)
{
	ARG_NOT_USED (lp)

	lock_release (loc_lock);
}

/* locator_list_add -- Add a locator to a locator list specifying its
		       components. */

LocatorNode_t *locator_list_add (LocatorList_t       *list,
				 LocatorKind_t       kind,
				 const unsigned char *addr,
				 uint32_t            port,
				 uint32_t            scope_id,
				 Scope_t             scope,
				 unsigned            flags,
				 unsigned            sproto)
{
	LocSearchData	data;
	LocatorNode_t	**npp, *np;
	LocatorRef_t	*rp, *p;
	int		is_new;

	data.kind = kind;
	data.addr = addr;
	data.port = port;

	lock_take (loc_lock);
	npp = sl_insert (&loc_list, &data, &is_new, loc_cmp);
	if (!npp) {
		warn_printf ("locator_list_add: not enough memory for list node!");
		lock_release (loc_lock);
		return (NULL);
	}
	if (is_new) {
		np = mds_pool_alloc (&mem_blocks [MB_LOCATOR]);
		if (!np) {
			warn_printf ("locator_list_add: not enough memory for locator node!");
#ifdef LIST_DELETE_NODE
			sl_delete_node (&loc_list, npp);
#else
			sl_delete (&loc_list, &data, loc_cmp);
#endif
			lock_release (loc_lock);
			return (NULL);
		}
		np->users = 0;
		np->locator.kind = kind;
		np->locator.port = port;
		memcpy (np->locator.address, addr, sizeof (np->locator.address));
		np->locator.scope_id = scope_id;
		np->locator.scope = scope;
		np->locator.flags = flags;
		np->locator.sproto = sproto;
		np->locator.intf = 0;
		np->locator.handle = 0;
		*npp = np;
	}
	else {
		np = *npp;
		if (np->locator.scope_id != scope_id ||
		    (np->locator.scope && scope && np->locator.scope != scope) ||
		    /*(np->locator.flags && flags && 
		     (np->locator.flags & LOCF_MFLAGS) != (flags & LOCF_MFLAGS)) ||*/
		    (np->locator.sproto && sproto && np->locator.sproto != sproto))
			log_printf (LOC_ID, 0, "locator_list_add: incompatible locator attributes for %s, "
						"%u:%u, %u:%u, 0x%x:0x%x, %u:%u!\r\n",
					locator_str (&np->locator),
					np->locator.scope_id, scope_id,
					np->locator.scope, scope,
					np->locator.flags, flags,
					np->locator.sproto, sproto);

		if (!np->locator.scope_id && scope_id)
			np->locator.scope_id = scope_id;
		if (!np->locator.scope && scope)
			np->locator.scope = scope;
		if (flags && np->locator.flags != flags)
			np->locator.flags |= flags;
		if (!np->locator.sproto && sproto)
			np->locator.sproto = sproto;

		/* Check if already in list. */
		for (rp = *list; rp; rp = rp->next)
			if (rp->data == np) {	/* Already there! */
				lock_release (loc_lock);
				return (0);
			}
	}
#ifdef LOG_LOCATORS
	log_printf (LOC_ID, 0, "LOC: locator_list_add (list=%p, %s)\r\n", (void *) list, locator_str (&np->locator));
#endif
	rp = mds_pool_alloc (&mem_blocks [MB_LOCREF]);
	if (!rp) {
		warn_printf ("locator_list_add: not enough memory for locator reference!\r\n");
		if (is_new) {
			mds_pool_free (&mem_blocks [MB_LOCATOR], np);
#ifdef LIST_DELETE_NODE
			sl_delete_node (&loc_list, npp);
#else
			sl_delete (&loc_list, &data, loc_cmp);
#endif
		}
		lock_release (loc_lock);
		return (NULL);
	}
	rp->next = NULL;
	rp->data = np;
	np->users++;
 	lock_release (loc_lock);
	if (*list) {
		for (p = *list; p->next; p = p->next)
			;
		p->next = rp;
	}
	else
		*list = rp;
	return (np);
}

/* locator_list_copy_node -- Take a copy of an existing node and append it to
			     the locator list. */

int locator_list_copy_node (LocatorList_t *list,
			    LocatorNode_t *np)
{
	LocatorRef_t	*rp, *p;

	for (rp = *list; rp; rp = rp->next)
		if (rp->data == np)	/* Already there! */
			return (0);

#ifdef LOG_LOCATORS
	log_printf (LOC_ID, 0, "LOC: locator_list_copy_node (list=%p, %s)\r\n", (void *) list, locator_str (&np->locator));
#endif
	rp = mds_pool_alloc (&mem_blocks [MB_LOCREF]);
	if (!rp)
		return (1);

	rp->next = NULL;
	rp->data = np;
	np->users++;
	if (*list) {
		for (p = *list; p->next; p = p->next)
			;
		p->next = rp;
	}
	else
		*list = rp;
	return (0);
}

/* locator_list_append -- Append a locator list to another locator_list. */

void locator_list_append (LocatorList_t *list, LocatorList_t new)
{
	LocatorRef_t	*rp;

	if (!*list) {
		*list = new;
		return;
	}
	else {
		for (rp = *list; rp->next; rp = rp->next)
			;
		rp->next = new;
	}
}

/* VALID_LOCATOR_TYPE -- Return 1 if the locator type is valid. */

#define	VALID_LOCATOR_TYPE(t) ((t) == LOCATOR_KIND_UDPv4 || (t) == LOCATOR_KIND_UDPv6)

/* locator_list_create -- Create a new locator list from an array of locators.*/

LocatorList_t locator_list_create (unsigned nlocs, const Locator_t *lp)
{
	LocatorList_t	list;

	locator_list_init (list);
	for (; nlocs; nlocs--, lp++) {
		if (!VALID_LOCATOR_TYPE (lp->kind))
			fatal_printf ("locator_list_create: Invalid locator type!");

		if (!locator_list_add (&list, lp->kind, lp->address, lp->port,
				       lp->scope_id, lp->scope, lp->flags, lp->sproto)) {
			locator_list_delete_list (&list);
			return (NULL);
		}
	}
	return (list);
}

/* locator_list_delete -- Delete an element from a locator list. */

void locator_list_delete (LocatorList_t       *list,
		          LocatorKind_t       kind,
		          const unsigned char *addr,
		          uint32_t            port)
{
	LocSearchData	data;
	LocatorNode_t	**npp, *np;
	LocatorRef_t	*rp, *prev;

	data.kind = kind;
	data.addr = addr;
	data.port = port;
	lock_take (loc_lock);
	npp = sl_search (&loc_list, &data, loc_cmp);
	if (!npp) {
		lock_release (loc_lock);
		return;	/* No such locator in system. */
	}
	np = *npp;
	lock_release (loc_lock);
	for (rp = *list, prev = NULL;
	     rp->data != *npp;
	     prev = rp, rp = rp->next)
		;

	if (!rp)
		return;	/* Not in this list. */

#ifdef LOG_LOCATORS
	log_printf (LOC_ID, 0, "LOC: locator_list_delete (list=%p, %s)\r\n", (void *) list, locator_str (&np->locator));
#endif
	/* Locator found in list! */
	lock_take (loc_lock);
	if (!--np->users) {
		mds_pool_free (&mem_blocks [MB_LOCATOR], np);
#ifdef LIST_DELETE_NODE
		sl_delete_node (&loc_list, npp);
#else
		sl_delete (&loc_list, &data, loc_cmp);
#endif
	}
	lock_release (loc_lock);
	if (prev)
		prev->next = rp->next;
	else
		*list = rp->next;
	mds_pool_free (&mem_blocks [MB_LOCREF], rp);
}

/* locator_free_node -- Free a locator node (lock is assumed to be taken). */

static void locator_free_node (LocatorNode_t *np)
{
	LocSearchData	data;

	data.kind = np->locator.kind;
	data.addr = np->locator.address;
	data.port = np->locator.port;
	sl_delete (&loc_list, &data, loc_cmp);
	mds_pool_free (&mem_blocks [MB_LOCATOR], np);
}

/* locator_unref -- Unreference and delete if necessary a locator node. */

void locator_unref (LocatorNode_t *np)
{
	lock_take (loc_lock);
	if (!--np->users)
		locator_free_node (np);
	lock_release (loc_lock);
}

/* locator_list_delete_list -- Delete a complete Locator list. */

void locator_list_delete_list (LocatorList_t *list)
{
	LocatorNode_t	*np;
	LocatorRef_t	*rp;

	if (!list)
		return;

	lock_take (loc_lock);
	while (*list) {
		rp = *list;
		*list = rp->next;
		np = rp->data;
#ifdef LOG_LOCATORS
		log_printf (LOC_ID, 0, "LOC: locator_list_delete_list (%s)\r\n", locator_str (&np->locator));
#endif
		if (!--np->users)
			locator_free_node (np);
		mds_pool_free (&mem_blocks [MB_LOCREF], rp);
	}
	lock_release (loc_lock);
}

/* locator_list_search -- Search a Locator in a locator list.  If found, the
			  index is returned, else -1. */

int locator_list_search (LocatorList_t       list,
		         LocatorKind_t       kind,
		         const unsigned char *addr,
		         uint32_t            port)
{
	LocatorNode_t	*np;
	LocatorRef_t	*rp;
	int		n = 0;

	if (list)
		for (rp = list, n = 0; rp; rp = rp->next, n++) {
			np = rp->data;
			if (locator_is (&np->locator, kind, addr, port))
				return (n);
		}
	return (-1);
}

/* locator_list_length -- Return the number of locators in a locator list. */

unsigned locator_list_length (LocatorList_t list)
{
	LocatorRef_t	*rp;
	unsigned	n = 0;

	for (rp = list, n = 0; rp; rp = rp->next, n++)
		;

	return (n);
}

void locator_list_flags_set (LocatorList_t l, unsigned mask, unsigned flags)
{
	LocatorList_t	rp;
	LocatorNode_t	*np;

	lock_take (loc_lock);
	foreach_locator (l, rp, np) {
		np->locator.flags &= ~mask;
		np->locator.flags |= flags;
	}
	lock_release (loc_lock);
}

/* locator_list_put -- Put a locator list in an array of locators, setting nlocs
		       to the # of locators in the array.  The number of free
		       entries in the array is specified with maxlocs.  If there
		       is no room to store all the locators, the number of
		       locators not stored will be returned. */

unsigned locator_list_put (unsigned      *nlocs,
		           Locator_t     *loc,
		           unsigned      maxlocs,
		           LocatorList_t list)
{
	LocatorRef_t	*rp;
	unsigned	n_overflow = 0;

	if (nlocs)
		(*nlocs) = 0;
	for (rp = list; rp; rp = rp->next) {
		if (!VALID_LOCATOR_TYPE (rp->data->locator.kind))
			fatal_printf ("locator_list_put: Invalid locator type!");
		
		if (!maxlocs) {
			n_overflow++;
			continue;
		}
		if (nlocs)
			(*nlocs)++;
		*loc++ = rp->data->locator;
		maxlocs--;
	}
	return (n_overflow);
}

/* locator_list_clone -- Clone a complete Locator list. */

LocatorList_t locator_list_clone (LocatorList_t list)
{
	LocatorRef_t	*rp, *nrp;
	LocatorNode_t	*np;
	LocatorList_t	lp = NULL, *lpp = &lp;

	foreach_locator (list, rp, np) {
		nrp = mds_pool_alloc (&mem_blocks [MB_LOCREF]);
		if (!nrp) {
			warn_printf ("locator_list_clone: not enough memory for locator reference!\r\n");
			locator_list_delete_list (&lp);
			break;
		}
		nrp->data = np;
		np->users++;
#ifdef LOG_LOCATORS
		log_printf (LOC_ID, 0, "LOC: locator_list_clone (list=%p, nlist=%p, %s)\r\n", (void *) list, (void *) lp, locator_str (&np->locator));
#endif
		*lpp = nrp;
		lpp = &nrp->next;
		*lpp = NULL;
	}
	return (lp);
}

/* locator_list_equal -- Compare two locator lists for equality. */

int locator_list_equal (LocatorList_t l1, LocatorList_t l2)
{
	LocatorRef_t	*rp, *rp2;
	LocatorNode_t	*np;

	if (locator_list_length (l1) != locator_list_length (l2))
		return (0);

	foreach_locator (l1, rp, np) {
		for (rp2 = l2; rp2; rp2 = rp2->next)
			if (rp2->data == np)
				break;
		if (!rp2)
			return (0);
	}
	return (1);
}

/* locator_list_no_mcast -- Return the multicast status of a locator list to
			    verify if multicasts are permitted. */

int locator_list_no_mcast (unsigned domain, LocatorList_t l)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;

	if (loc_force_no_mcast)
		return (1);

	if (!loc_no_mcast)
		return (0);

	foreach_locator (l, rp, np)
		if (np->locator.kind == LOCATOR_KIND_UDPv4 &&
		    ip_match (loc_no_mcast, domain, np->locator.address + 12))
			return (1);

	return (0);
}

#ifdef _WIN32
#define snprintf sprintf_s
#endif

/* locator_str -- Return a string corresponding to the given locator. */

const char *locator_str (const Locator_t *lp)
{
	unsigned	i, n, avail;
	char		*sp;
	static char	buf [80];
#ifdef DDS_IPV6
	static char	abuf [100];
#endif
#ifdef DDS_SECURITY
	static const char *sec_proto_str [] = {
		NULL,
		"DTLS",
		"TLS",
		NULL,
		"DDSSec"
	};
#endif

	if (!lp)
		return (NULL);

	sp = buf;
	avail = sizeof (buf);
	switch (lp->kind) {
		case LOCATOR_KIND_UDPv4:
			n = snprintf (sp, avail, "UDP:%u.%u.%u.%u:%u",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
			sp += n;
			avail -= n;
			break;
#ifdef DDS_IPV6
		case LOCATOR_KIND_UDPv6:
			n = snprintf (buf, avail, "UDPv6:%s:%u",
				inet_ntop (AF_INET6, (void *) lp->address, abuf, sizeof (abuf)),
				lp->port);
			sp += n;
			avail -= n;
			break;
#endif
#ifdef DDS_TCP
		case LOCATOR_KIND_TCPv4:
			n = snprintf (buf, avail, "TCP:%u.%u.%u.%u:%u",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
			sp += n;
			avail -= n;
			break;
#ifdef DDS_IPV6
		case LOCATOR_KIND_TCPv6:
			n = snprintf (buf, avail, "TCPv6:%s:%u",
				inet_ntop (AF_INET6, (void *) lp->address, abuf, sizeof (abuf)),
				lp->port);
			sp += n;
			avail -= n;
			break;
#endif
#endif
		default:
#ifndef DDS_IPV6
			if (lp->kind == LOCATOR_KIND_UDPv6)
				n = snprintf (sp, avail, "UDPv6:");
			else
#endif
				n = snprintf (sp, avail, "%u?:", lp->kind);
			sp += n;
			avail -= n;
			for (i = 0; i < 15; i++) {
				if (i) {
					snprintf (sp, avail, ":");
					sp++;
					avail--;
				}
				snprintf (sp, avail, "%02x", lp->address [i]);
				sp += 2;
				avail -= 2;
			}
			n = snprintf (sp, avail, ":%u", lp->port);
			sp += n;
			avail -= n;
			break;
	}
#ifdef DDS_SECURITY
	if ((lp->flags & LOCF_SECURE) != 0 && avail) {
		n = snprintf (sp, avail, "-%s(%c)",
				sec_proto_str [lp->sproto],
				(lp->flags & LOCF_SERVER) ? 'S' : 'C');
		sp += n;
		avail -= n;
	}
#endif
	if (lp->intf && avail) {
		n = snprintf (sp, avail, "#%u", lp->intf);
		sp += n;
		avail -= n;
	}
	if (lp->handle && avail)
		n = snprintf (sp, avail, "(%u)", lp->handle);

	return (buf);
}

/* locator_hash -- Return a hash of a locator. */

unsigned locator_hash (const Locator_t *lp)
{
	unsigned	h;

	h = hashf ((const unsigned char *) &lp->kind, sizeof (lp->kind));
	if ((lp->kind & LOCATOR_KINDS_IPv4) != 0)
		h = hashfc (h, &lp->address [12], 4);
	else
		h = hashfc (h, lp->address, 16);
	h = hashfc (h, (const unsigned char *) &lp->port, sizeof (lp->port));
	return (h);
}

/* locator_list_log -- Log the contents of a complete locator list. */

void locator_list_log (int id, unsigned level, LocatorList_t list)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	int		i;

	if (!list)
		log_printf (id, level, "<empty>");
	i = 0;
	foreach_locator (list, rp, np) {
		if (i++)
			log_printf (id, level, ",");
		log_printf (id, level, "%s", locator_str (&np->locator));
#ifdef LOC_DUMP_COUNT
		log_printf (id, level, "*%u", np->users);
#endif
	}
}

/* locator_list_dump -- Dump a complete locator list. */

void locator_list_dump (LocatorList_t list)
{
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	int		i;

	if (!list) {
		dbg_printf ("<empty>");
		return;
	}
	i = 0;
	foreach_locator (list, rp, np) {
		if (i++)
			dbg_printf (",");
		dbg_printf ("%s", locator_str (&np->locator));
#ifdef LOC_DUMP_COUNT
		dbg_printf ("*%u", np->users);
#endif
	}
}

static int remove_handle (Skiplist_t *list, void *node, void *arg)
{
	int 		*handle = (int *) arg; 
	LocatorNode_t	*np, **npp = (LocatorNode_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	np = *npp;
	if (np->locator.handle == *handle || !*handle)
		np->locator.handle = 0;

	return (1);
}

/* locators_remove_handle -- Remove the given handle from all locators. */

void locators_remove_handle (int h)
{
	lock_take (loc_lock);
	sl_walk (&loc_list, remove_handle, (void *) &h);
	lock_release (loc_lock);
}

/* loc2sockaddr -- Convert a locator to struct sockaddr data. */

struct sockaddr *loc2sockaddr (LocatorKind_t kind,
			       uint32_t port,
			       unsigned char addr [16],
			       unsigned char *sabuf)
{
	struct sockaddr_in	*sa4p;
#ifdef DDS_IPV6
	struct sockaddr_in6	*sa6p;
#endif

	if ((kind & LOCATOR_KINDS_IPv4) != 0) {
		uint32_t a = (addr [12] << 24) | (addr [13] << 16) |
			     (addr [14] <<  8) | (addr [15] <<  0);

		sa4p = (struct sockaddr_in *) sabuf;
		sa4p->sin_family = AF_INET;
		sa4p->sin_port = htons (port);
		sa4p->sin_addr.s_addr = htonl (a);
		return ((struct sockaddr *) sa4p);
	}
#ifdef DDS_IPV6
	else if ((kind & LOCATOR_KINDS_IPv6) != 0) {
		sa6p = (struct sockaddr_in6 *) sabuf;
		sa6p->sin6_family = AF_INET6;
		sa6p->sin6_port = htons (port);
		sa6p->sin6_flowinfo = 0;
		memcpy (sa6p->sin6_addr.s6_addr, addr, 16);
		sa6p->sin6_scope_id = 0;
		return ((struct sockaddr *) sa6p);
	}
#endif
	else
		return (NULL);
}

/* sockaddr2loc -- Initialize a locator from struct sockaddr data. */

Locator_t *sockaddr2loc (Locator_t *lp, struct sockaddr *sa)
{
	memset (lp, 0, sizeof (Locator_t));
	if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sa4 = (struct sockaddr_in *) sa;
		uint32_t addr = ntohl(sa4->sin_addr.s_addr);

		lp->kind = LOCATOR_KIND_UDPv4;
		lp->port = ntohs (sa4->sin_port);
		lp->address [12] = (addr >> 24) & 0xff;
		lp->address [13] = (addr >> 16) & 0xff;
		lp->address [14] = (addr >>  8) & 0xff;
		lp->address [15] = (addr >>  0) & 0xff;
	}
#ifdef DDS_IPV6
	else if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) sa;

		lp->kind = LOCATOR_KIND_UDPv6;
		lp->port = ntohs (sa6->sin6_port);
		memcpy (lp->address, sa6->sin6_addr.s6_addr, 16);
	}
#endif
	else
		lp = NULL;

	return (lp);
}

#ifdef DDS_DEBUG

/* locator_pool_dump -- Dump the memory usage of the locator pool. */

void locator_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

/* show_locator -- Display a locator node. */

int show_locator (Skiplist_t *list, void *node, void *arg)
{
	LocatorNode_t	*np, **npp = (LocatorNode_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	np = *npp;
	dbg_print_locator (&np->locator);
	dbg_printf ("*%u\r\n", np->users);
	return (1);
}

/* locator_dump -- Dump all cached locators. */

void locator_dump (void)
{
	lock_take (loc_lock);
	sl_walk (&loc_list, show_locator, NULL);
	lock_release (loc_lock);
}

#endif

