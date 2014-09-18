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

/* locator.h -- Defines common operations on Locators as used in both RTPS and
                the various discovery protocols. */

#ifndef __locator_h_
#define __locator_h_

#include <stdint.h>
#include "dds/dds_trans.h"
#include "sys.h"
#include "pool.h"

#define	LOC_OK	0
#define	LOC_ERR_NOMEM	1
#define	LOC_ERR_PARAM	4

#define	MAXLLOCS	16	/* Max. # of locators in a message. */

/* Locator flags: */
#define	LOCF_DATA	0x01	/* Can be used for normal data exchanges. */
#define	LOCF_META	0x02	/* Can be used for discovery exchanges. */
#define	LOCF_UCAST	0x04	/* Can be used for Unicast destinations. */
#define	LOCF_MCAST	0x08	/* Can be used for Multicast/broadcast dests. */
#define	LOCF_SECURE	0x10	/* Secure tunnel. */
#define	LOCF_SERVER	0x20	/* Server side of tunnel. */
#define LOCF_FCLIENT	0x40	/* Pick up client role in next connections. */

#define	LOCF_MFLAGS	(LOCF_DATA | LOCF_META | LOCF_UCAST | LOCF_MCAST)

#define	MSG_LOCATOR_SIZE	24	/* Kind/Port/Address. */

typedef struct locator {
	LocatorKind_t	kind;		/* Type of protocol address. */
	uint32_t	port;		/* Native port number. */
	unsigned char	address [16];	/* Address data. */
	uint32_t	scope_id;	/* Scope id (OS-dependent). */
	unsigned	scope:3;	/* Address scope type. */
	unsigned	flags:7;	/* Various locator flags (see LOCF_*).*/
	unsigned	sproto:2;	/* Security protocol if applicable. */
	unsigned	intf:4;		/* Interface index if != 0. */
	unsigned	handle:16;	/* Connection handle if != 0. */
} Locator_t;

#define LOCATOR_PORT_INVALID	0
#define	LOCATOR_ADDRESS_INVALID	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define	LOCATOR_INVALID	{ LOCATOR_KIND_INVALID, LOCATOR_PORT_INVALID, LOCATOR_ADDRESS_INVALID, 0, 0, 0, 0, 0, 0 }

typedef struct locator_udp4_st {
	uint32_t	address;
	uint32_t	port;
} LocatorUdpv4_t;

typedef struct locator_ref_st LocatorRef_t;
typedef struct locator_node_st LocatorNode_t;

struct locator_ref_st {
	LocatorRef_t	*next;
	LocatorNode_t	*data;
};

struct locator_node_st {
	unsigned	users;
	Locator_t	locator;
};

typedef LocatorRef_t *LocatorList_t;

extern unsigned char	loc_addr_invalid [16];
extern Locator_t	locator_invalid;

#define	LOCATOR_KIND_IS_INVALID(l)	((l).type == LT_INVALID)
#define	LOCATOR_KIND_IS_RESERVED(l)	((l).type == LK_RESERVED)
#define	LOCATOR_ADDRESS_INVALIDATE(l)	(l).address[0] = (l).address[1] = 0
#define	LOCATOR_ADDRESS_IS_INVALID(l)	((l).address[0]==0&&(l).address[1]==0)
#define	LOCATOR_PORT_IS_INVALID(l)	((l).port == ~0)

#define	LOCATOR_KINDS_IPv4	(LOCATOR_KIND_UDPv4 | LOCATOR_KIND_TCPv4)
#define	LOCATOR_KINDS_IPv6	(LOCATOR_KIND_UDPv6 | LOCATOR_KIND_TCPv6)
#define	LOCATOR_KINDS_UDP	(LOCATOR_KIND_UDPv4 | LOCATOR_KIND_UDPv6)
#define	LOCATOR_KINDS_TCP	(LOCATOR_KIND_TCPv4 | LOCATOR_KIND_TCPv6)

int locator_pool_init (const POOL_LIMITS *locrefs, const POOL_LIMITS *locators);

/* Initialize the locator list pools. */

void locator_pool_free (void);

/* Free the locator list pools. */

#define locator_is(l1,k,a,p)	((l1)->kind == (k) &&	\
				 (l1)->port==(p) &&	\
				 !memcmp ((l1)->address, a, 16))

/* Compare a locator with the given locator elements. */

#define locator_equal(l1,l2)	locator_is(l1,(l2)->kind,(l2)->address,(l2)->port)

/* Compare 2 locators for equality. */

#define	locator_addr_equal(l1,l2) ((l1)->kind==(l2)->kind && \
				  !memcmp ((l1)->address, (l2)->address, 16))

/* Compare 2 locators for address equality (ignoring the port numbers). */

void locator_lock (Locator_t *loc);

/* Lock the locator in order to modify its fields. */

void locator_release (Locator_t *loc);

/* Release the lock on a locator. */

#define	locator_list_init(l)	(l) = NULL

/* Get a new and empty locator list. */

LocatorList_t locator_list_create (unsigned nlocs, const Locator_t *loc);

/* Create a new locator list from an array of locators. */

unsigned locator_list_put (unsigned      *nlocs,
		           Locator_t     *loc,
		           unsigned      maxlocs,
		           LocatorList_t list);

/* Put a locator list in an array of locators, setting nlocs to the # of
   locators in the array.  The number of free entries in the array is
   specified with maxlocs.  If there is no room to store all the locators,
   the number of locators not stored will be returned. */

LocatorNode_t *locator_list_add (LocatorList_t       *list,
			         LocatorKind_t       kind,
				 const unsigned char *addr,
				 uint32_t            port,
				 uint32_t            scope_id,
				 Scope_t             scope,
				 unsigned            flags,
				 unsigned            sproto);

/* Add a locator to a locator list specifying its components. */

int locator_list_copy_node (LocatorList_t *list,
			    LocatorNode_t *np);

/* Take a copy of an existing node and append it to the list. */
 
void locator_list_delete (LocatorList_t       *list,
		          LocatorKind_t       kind,
		          const unsigned char *addr,
		          uint32_t            port);

/* Delete an element from a locator list. */

void locator_list_delete_list (LocatorList_t *list);

/* Delete a complete Locator list. */

int locator_list_search (LocatorList_t       list,
		         LocatorKind_t       kind,
		         const unsigned char *addr,
		         uint32_t            port);

/* Search a Locator in a locator list.  If found, the index is returned.
   If the locator was not found, the function returns -1. */

unsigned locator_list_length (LocatorList_t list);

/* Return the number of locators in a locator list. */

LocatorList_t locator_list_clone (LocatorList_t list);

/* Clone (i.e. make a copy of) a locator list. */

void locator_list_append (LocatorList_t *list, LocatorList_t new1);

/* Append a locator list to another locator list. */

int locator_list_equal (LocatorList_t l1, LocatorList_t l2);

/* Compare two locator lists for equality. */

#define	foreach_locator(list,rp,np) for (rp = list; \
			rp && (np = rp->data) != NULL; rp = rp->next)

/* Can be used to walk over a locator list.  The np parameter should be of type
   LocatorNode_t and will be valid for usage in the body of the foreach_ loop.*/

int locator_list_no_mcast (unsigned domain, LocatorList_t l);

/* Return the multicast status of a locator list to verify if multicasts are
   permitted. */

void locator_list_flags_set (LocatorList_t l, unsigned mask, unsigned flags);

/* Set the masked bits to the value of flags in all locators of the list. */

void locator_unref (LocatorNode_t *np);

/* Unreference a locator node and free its contents if possible. */

void locators_remove_handle (int h);

/* Reset the handle either in all locators (h == 0) or in all locators that are
   currently referring to it. */

struct sockaddr *loc2sockaddr (LocatorKind_t kind,
			       uint32_t port,
			       unsigned char addr [16],
			       unsigned char *sabuf);

/* Convert a locator to struct sockaddr data. */

Locator_t *sockaddr2loc (Locator_t *lp, struct sockaddr *sa);

/* Initialize a locator from struct sockaddr data. */

const char *locator_str (const Locator_t *lp);

/* Return a string corresponding to the given locator. */

unsigned locator_hash (const Locator_t *lp);

/* Return a hash of a locator. */

void locator_list_log (int id, unsigned level, LocatorList_t list);

/* Log the contents of a locators list. */

void locator_list_dump (LocatorList_t list);

/* Dump a complete locator list. */

void locator_pool_dump (size_t sizes []);

/* Debug: dump the memory usage of the skiplist pool. */

void locator_dump (void);
 
/* Dump all cached locators. */

#endif /* !__locator_h_ */

