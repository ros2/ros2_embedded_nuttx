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

/* rtps_ft -- Defines the data used for the forwarding lookup table.

   The forwarding table is a simple hash table with chained buckets in case of
   collisions.  Most functions are safe, except for ft_lookup(), ft_add(),
   ft_get(), ft_set() and ft_clear().  Use ft_entry/exit() around them.
   Example usage of these functions:
   
   	ft_entry();					<-- Lock lookup table
	p = ft_lookup (prefix, &h);			<-- Lookup prefix
	if (!p)						<-- Doesn't exist yet?
		ft_add (h, prefix, locator, mode, flags); <-- Add new entry.
	else
		lp = ft_get (p, mode);			<-- Exists: get data.

	... do some useful work ...

	ft_exit();					<-- Unlock table.
 */

#ifndef __rtps_ft_
#define	__rtps_ft_

#ifndef MAX_FWD_TABLE
#define	MAX_FWD_TABLE	64	/* Max. # of hash table entries (power of 2!).*/
#endif
#define	FWD_HASH_MASK	(MAX_FWD_TABLE - 1)	/* Hash mask value. */
#ifndef MAX_FWD_TTL
#define	MAX_FWD_TTL	200	/* Forwarding entry timeout (in seconds). */
#endif
#ifndef AGE_PERIOD
#define	AGE_PERIOD	5	/* How often to revisit entries for ageing. */
#endif

typedef enum {
	META_MCAST,		/* Discovered meta multicast locators index. */
	META_UCAST,		/* Discovered meta unicast locators index. */
	USER_MCAST,		/* Discovered user multicast locators index. */
	USER_UCAST		/* Discovered user unicast locators index. */
} Mode_t;

#define	MODE_MIN	META_MCAST
#define	MODE_MAX	USER_UCAST

#define	MAX_MODES	4

#define	META_REPLY	0	/* Locator index for unicast meta replies. */
#define	USER_REPLY	1	/* Locator index for unicast user replies. */

#define	LTF_LINKED	1	/* In entry link chain. */
#define	LTF_INFO_REPLY	2	/* Entry learned from InfoReply. */
#define	LTF_AGE		4	/* Entry needs ageing. */

#define	LT_LOCAL_TO	3	/* Directly connected (over UDP) source. */

typedef struct ft_entry_st FTEntry_t;
struct ft_entry_st {
	FTEntry_t	*next;			/* Next in hash chain. */
	GuidPrefix_t	guid_prefix;		/* GUID prefix. */
	unsigned	id;			/* Domain index. */
	unsigned	flags;			/* Entry flags. */
	unsigned	local;			/* Local entry. */
	LocatorKind_t	kinds;			/* Locator kinds. */
	LocatorList_t	locs [MAX_MODES][2];	/* Dest/reply locators. */
	unsigned	ttl;			/* Remaining lifetime. */
	unsigned	nchildren;		/* # of depending children. */
	FTEntry_t	*parent;		/* Parent entry. */
	FTEntry_t	*dlink;			/* Dynamic linkage. */
};

typedef struct ft_table_st FTTable_t;
struct ft_table_st {
	Timer_t		age_timer;		/* Ageing timer. */
	lock_t		lock;			/* Lock. */
	unsigned	users;			/* # of users of this table. */
	FTEntry_t	*ht [MAX_FWD_TABLE];	/* Hash table. */
};


FTTable_t *ft_new (void);

/* Create a new forwarding lookup table . */

void ft_reuse (FTTable_t *tp);

/* Reuse a forwarding lookup table, incrementing its number of users. */

FTTable_t *ft_cleanup (FTTable_t *tp, unsigned id);

/* Cleanup a complete lookup table of all entries with the given id,
   decrementing its number of users, and disposing it if there are no more
   users.  If the table was disposed, NULL is returned, otherwise the
   still existing lookup table pointer is returned. */

#define ft_entry(tp)	lock_take(tp->lock)

/* Start some operations on a lookup table. */

#define ft_exit(tp)	lock_release(tp->lock)

/* Lookup table operations are done for now. */

Locator_t *ft_get_locator (FTEntry_t *p, Mode_t m, int reply);

/* Get a single locator from an existing lookup table entry. */

LocatorList_t ft_get_locators (FTEntry_t *p, Mode_t m, int reply);

/* Get a locator list from an existing lookup table entry. */

void ft_clear_locators (FTEntry_t *p, Mode_t m, int reply);

/* Clear a locator list of an existing lookup table entry. */

void ft_add_locator (FTEntry_t       *p,
		     const Locator_t *locator,
		     Mode_t          m,
		     int             reply);

/* Update/set a locator in an existing lookup table entry. */

void ft_add_locators (FTEntry_t     *p,
		      LocatorList_t list,
		      Mode_t        m,
		      int           reply);

/* Update/set a complete locator list in an entry. */

FTEntry_t *ft_lookup (FTTable_t *tp, GuidPrefix_t *prefix, unsigned *index);

/* Lookup an entry in the forwarding table.
   If not found, the *index parameter is set to the hash index of the entry to
   accelerate a subsequent ft_add() and NULL will be returned.
   Note: use ft_entry() before calling this function. */

#define	HASH_INVALID	-1

FTEntry_t *ft_add (FTTable_t    *tp,
		   int          index,
		   GuidPrefix_t *prefix,
		   unsigned     id,
		   unsigned     flags);

/* Add a new entry to the lookup table.  If index is not significant it should
   be set to HASH_INVALID.  Otherwise it *must* be set to the value returned by
   ft_lookup().
   If successful, the function returns a pointer to the new entry.
   Note: use ft_entry() before calling this function. */

void ft_delete (FTTable_t *tp, GuidPrefix_t *prefix);

/* Delete an entry from the lookup table. */

const char *ft_mode_str (Mode_t m);

void dbg_print_mode (Mode_t m, int reply);

/* Debug: display a mode type. */

void dbg_dump_ft (FTTable_t *tp);

/* Dump the complete contents of the lookup table. */

#endif /* !__rtps_ft_ */
