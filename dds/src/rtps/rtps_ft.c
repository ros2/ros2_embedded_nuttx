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

/* rtps_ft -- RTPS forwarding table functionality. */

#ifdef DDS_FORWARD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "hash.h"
#include "error.h"
#include "debug.h"
#include "sock.h"
#include "thread.h"
#include "rtps_ft.h"

/* ft_add_locator -- Update/set a locator in an existing lookup table entry. */

void ft_add_locator (FTEntry_t       *p,
		     const Locator_t *locator,
		     Mode_t          m,
		     int             reply)
{
	if (!p->parent)
		locator_list_add (&p->locs [m][reply],
				  locator->kind,
				  locator->address,
				  locator->port,
				  locator->scope_id,
				  locator->scope,
				  locator->flags,
				  locator->sproto);
}

/* ft_add_locators -- Update/set a complete locator list in an entry. */

void ft_add_locators (FTEntry_t     *p,
		      LocatorList_t list,
		      Mode_t        m,
		      int           reply)
{
	if (!p->parent) {
		ft_clear_locators (p, m, reply);
		p->locs [m][reply] = locator_list_clone (list);
	}
}

/* ft_get_locator -- Get a single locator from an existing table entry. */

Locator_t *ft_get_locator (FTEntry_t *p, Mode_t m, int reply)
{
	Locator_t	*lp;
	LocatorList_t	list;

	if (p->parent)
		p = p->parent;
	list = p->locs [m][reply];
	if (list)
		lp = &list->data->locator;
	else
		lp = NULL;
	return (lp);
}

/* ft_get_locators -- Get a locator list from an existing table entry. */

LocatorList_t ft_get_locators (FTEntry_t *p, Mode_t m, int reply)
{
	if (p->parent)
		p = p->parent;
	return (p->locs [m][reply]);
}

/* ft_clear_locators -- Clear a locator list of an existing table entry. */

void ft_clear_locators (FTEntry_t *p, Mode_t m, int reply)
{
	if (!p->parent)
		locator_list_delete_list (&p->locs [m][reply]);
}

/* ft_dispose -- Cleanup and release a forwarding table entry. */

static void ft_dispose (FTTable_t *tp, FTEntry_t *p)
{
	FTEntry_t	*cp, *prev, *next_p;
	Mode_t		m;
	unsigned	i;

	if (p->nchildren)

		/* Remove all this node's children! */
		for (i = 0; i < MAX_FWD_TABLE; i++) {
			for (cp = tp->ht [i], prev = NULL; cp; cp = next_p) {
				next_p = cp->next;
				if (cp->parent == p) {
					if (prev)
						prev->next = cp->next;
					else
						tp->ht [i] = cp->next;
					xfree (cp);
					if (!--p->nchildren)
						break;
				}
				else
					prev = cp;
			}
			if (!p->nchildren)
				break;
		}

	if (p->parent)
		p->parent->nchildren--;
	else
		for (i = 0; i < 2; i++)
			for (m = MODE_MIN; m <= MODE_MAX; m++)
				ft_clear_locators (p, m, i);
	xfree (p);
}

/* ft_age -- Ageing function to dispose entries that are too old. */

static void ft_age (uintptr_t user)
{
	FTTable_t	*tp = (FTTable_t *) user;
	FTEntry_t	**fpp, *p, *prev, *next_p;
	unsigned	i;

	ft_entry (tp);
	for (i = 0, fpp = tp->ht; i < MAX_FWD_TABLE; i++, fpp++)
		for (p = *fpp, prev = NULL; p; p = next_p) {
			next_p = p->next;
			if ((p->flags & LTF_AGE) == 0) {
				prev = p;
				continue;
			}
			if (p->ttl <= AGE_PERIOD) {
				if (prev)
					prev->next = next_p;
				else
					*fpp = next_p;
				ft_dispose (tp, p);
			}
			else {
				p->ttl -= AGE_PERIOD;
				prev = p;
				if (p->local)
					p->local--;
			}
		}
	ft_exit (tp);
	tmr_start (&tp->age_timer, AGE_PERIOD * TICKS_PER_SEC, (uintptr_t) tp, ft_age);
}

/* ft_new -- Initialize the forwarding lookup table. */

FTTable_t *ft_new (void)
{
	FTTable_t	*tp;

	tp = xmalloc (sizeof (FTTable_t));
	if (!tp)
		return (NULL);

	lock_init_nr (tp->lock, "FwdTable");
	tp->users = 1;
	memset (tp->ht, 0, sizeof (tp->ht));
	tmr_init (&tp->age_timer, "FwdAgeing");
	tmr_start (&tp->age_timer, AGE_PERIOD * TICKS_PER_SEC, (uintptr_t) tp, ft_age);
	return (tp);
}

/* ft_reuse -- Reuse a forwarding lookup table, incrementing its # of users. */

void ft_reuse (FTTable_t *tp)
{
	if (tp)
		tp->users++;
}

/* ft_lookup -- Lookup an entry in the forwarding table.
		If not found, the *index parameter is set to the hash index
		of the entry to accelerate a subsequent ft_add().
		Note: use ft_entry() before calling this function. */

FTEntry_t *ft_lookup (FTTable_t *tp, GuidPrefix_t *prefix, unsigned *index)
{
	FTEntry_t	*p;
	unsigned	h;

	h = hashf (prefix->prefix, GUIDPREFIX_SIZE) & FWD_HASH_MASK;
	for (p = tp->ht [h]; p; p = p->next)
		if (guid_prefix_eq (p->guid_prefix, *prefix))
			return (p);

	if (index)
		*index = h;
	return (NULL);
}

/* ft_add -- Add a new entry to the lookup table.  If index is not significant
	     it should be set to HASH_INVALID.  Otherwise it *must* be set to
	     the value returned by ft_lookup().
	     If successful, the function returns a pointer to the new entry.
	     Note: use ft_entry() before calling this function. */


FTEntry_t *ft_add (FTTable_t    *tp,
		   int          index,
		   GuidPrefix_t *prefix,
		   unsigned     id,
		   unsigned     flags)
{
	FTEntry_t	*p;
	unsigned	h;

	if (index < 0)
		h = hashf (prefix->prefix, GUIDPREFIX_SIZE) & FWD_HASH_MASK;
	else
		h = index;
	p = xmalloc (sizeof (FTEntry_t));
	if (!p)
		return (NULL);

	p->guid_prefix = *prefix;
	p->id = id;
	memset (p->locs, 0, sizeof (p->locs));
	p->flags = flags;
	p->local = 0;
	p->ttl = MAX_FWD_TTL;
	p->nchildren = 0;
	p->parent = NULL;
	p->next = tp->ht [h];
	p->dlink = NULL;
	tp->ht [h] = p;
	return (p);
}

/* ft_delete -- Delete an entry from the lookup table. */

void ft_delete (FTTable_t *tp, GuidPrefix_t *prefix)
{
	FTEntry_t	*p, *prev;
	unsigned	h;

	ft_entry (tp);
	h = hashf (prefix->prefix, GUIDPREFIX_SIZE) & FWD_HASH_MASK;
	for (p = tp->ht [h], prev = NULL; p; prev = p, p = p->next)
		if (guid_prefix_eq (p->guid_prefix, *prefix)) {
			if (prev)
				prev->next = p->next;
			else
				tp->ht [h] = p->next;
			ft_dispose (tp, p);
			break;
		}
	ft_exit (tp);
}

/* ft_cleanup -- Cleanup the complete lookup table. */

FTTable_t *ft_cleanup (FTTable_t *tp, unsigned id)
{
	FTEntry_t	*p, *next_p, *prev;
	unsigned	i;

	ft_entry (tp);
	for (i = 0; i < MAX_FWD_TABLE; i++)
		for (p = tp->ht [i], prev = NULL; p; p = next_p) {
			next_p = p->next;
			if (p->id == id || !p->id) {
				if (prev)
					prev->next = p->next;
				else
					tp->ht [i] = p->next;
				ft_dispose (tp, p);
			}
			else
				prev = p;
		}
	ft_exit (tp);
	if (!--tp->users) {
		lock_destroy (tp->lock);
		xfree (tp);
		tp = NULL;
	}
	return (tp);
}

const char *ft_mode_str (Mode_t m)
{
	static const char *mode_str [] = {
		"MetaMC", "MetaUC", "UserMC", "UserUC"
	};

	return ((m <= MODE_MAX) ? mode_str [m] : "?");
}

void dbg_print_mode (Mode_t m, int reply)
{
	if (/*m < MODE_MIN || */m > MODE_MAX)
		dbg_printf ("?%d", m);
	else
		dbg_printf ("%c-%s", (reply) ? 'R': 'D', ft_mode_str (m));
}

#ifdef DDS_DEBUG

/* dbg_dump_ft -- Dump the complete contents of the lookup table. */

void dbg_dump_ft (FTTable_t *tp)
{
	FTEntry_t	*p;
	Mode_t		m;
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	unsigned	i, j, n;

	if (!tp)
		return;

	dbg_printf ("GUID prefix lookup table:\r\n");
	ft_entry (tp);
	for (i = 0; i < MAX_FWD_TABLE; i++)
		for (p = tp->ht [i]; p; p = p->next) {
			dbg_printf ("\t");
			dbg_print_guid_prefix (&p->guid_prefix);
			dbg_printf ("\tId=%u, TTL=%u, Local=%u, #ch=%u", p->id, p->ttl,
								p->local, p->nchildren);
			if (p->parent)
				dbg_printf (", Parent=%p", (void *) p->parent);
			if ((p->flags & LTF_LINKED) != 0)
				dbg_printf (", Linked");
			if ((p->flags & LTF_INFO_REPLY) != 0)
				dbg_printf (", InfoReply");
			if ((p->flags & LTF_AGE) != 0)
				dbg_printf (", Age");
			for (j = 0; j <= 1; j++) {
				for (m = MODE_MIN; m <= MODE_MAX; m++) {
					if (!p->locs [m][j])
						continue;

					n = 0;
					foreach_locator (p->locs [m][j], rp, np) {
						dbg_printf ("\r\n\t\t");
						if (!n) {
							dbg_print_mode (m, j);
							dbg_printf (":");
						}
						else
							dbg_printf ("\t");
						dbg_printf ("\t");
						dbg_print_locator (&np->locator);
						dbg_printf (" ");
						n++;
					}
				}
			}
			dbg_printf ("\r\n");
		}
	ft_exit (tp);
}

#endif

#else
int avoid_emtpy_translation_unit_rtps_ft_c;
#endif /* DDS_FORWARD */

