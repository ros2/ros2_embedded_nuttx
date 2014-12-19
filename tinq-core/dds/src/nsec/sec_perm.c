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

/* sec_perm.c -- Permissions handle cache, as used by both authentication and
	         access control security mechanisms. */

#include "error.h"
#include "sec_perm.h"

#define	MAX_PERM_HASH	16
#define	PERM_HASH(perm)	(perm & (MAX_PERM_HASH - 1))

static PermissionsData_t	*perm_ht [MAX_PERM_HASH];

static PermissionsData_t *perm_new (Permissions_t perm)
{
	PermissionsData_t	*p;

	p = xmalloc (sizeof (PermissionsData_t));
	if (!p)
		return (NULL);

	memset (p, 0, sizeof (PermissionsData_t));
	p->perm = perm;
	p->nusers = 1;
	return (p);
}

PermissionsData_t *perm_lookup (Permissions_t perm, unsigned *hp)
{
	PermissionsData_t	*p;
	unsigned		h = PERM_HASH (perm);

	if (hp)	
		*hp = h;
	for (p = perm_ht [h]; p; p = p->next)
		if (p->perm == perm)
			return (p);

	return (NULL);
}

static void perm_add (PermissionsData_t *p, unsigned h)
{
	p->next = perm_ht [h];
	perm_ht [h] = p;
}

PermissionsData_t *perm_create (Permissions_t perm, DDS_ReturnCode_t *ret)
{
	PermissionsData_t	*p;
	unsigned		h;

	p = perm_lookup (perm, &h);
	if (p) {
		*ret = DDS_RETCODE_OK;
		return (p);
	}
	p = perm_new (perm);
	if (!p) {
		*ret = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	perm_add (p, h);
	*ret = DDS_RETCODE_OK;
	return (p);
}

static void perm_release_p (PermissionsData_t *p)
{
	PermissionsData_t	*xp, *prev;
	unsigned		h;

	if (p->perm_creds) {
		DDS_DataHolder__free (p->perm_creds);
		p->perm_creds = NULL;
	}
	if (p->perm_token)
		token_unref (p->perm_token);
	h = PERM_HASH (p->perm);
	for (xp = perm_ht [h], prev = NULL; xp; prev = xp, xp = xp->next)
		if (xp == p) {
			if (prev)
				prev->next = p->next;
			else
				perm_ht [h] = p->next;
			break;
		}
	xfree (p);
}

static void perm_free (PermissionsData_t *p)
{
	if (!p || --p->nusers)
		return;

	perm_release_p (p);
}

void perm_release (Permissions_t perm)
{
	unsigned	h;

	perm_free (perm_lookup (perm, &h));
}

PermissionsData_t *perm_ref (PermissionsData_t *p)
{
	if (!p)
		return (NULL);

	p->nusers++;
	return (p);
}

void perm_unref (PermissionsData_t **p)
{
	if (!p)
		return;

	if (!--(*p)->nusers) {
		perm_release_p (*p);
		*p = NULL;
	}
}

/* perm_add_plugin -- Add permissions methods to a Permissions entry. */

int perm_add_plugin (PermissionsData_t *p, const SEC_PERM *pp)
{
	unsigned	i;

	for (i = 0; i < MAX_P_PLUGINS; i++)
		if (!p->plugins [i]) {
			p->plugins [i] = pp;
			return (DDS_RETCODE_OK);
		}

	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

#ifdef DDS_DEBUG

static void dump_permissions (PermissionsData_t *p)
{
	dbg_printf ("%4u", p->perm);
	if (p->nusers != 1)
		dbg_printf ("*%u", p->nusers);
	dbg_printf (":\r\n    Identity: %u\r\n", p->id);
	if (p->perm_token) {
		dbg_printf ("    Permissions Token:\t");
		token_dump (1, p->perm_token->data, p->perm_token->nusers, 0);
	}
	if (p->perm_creds) {
		dbg_printf ("    Permissions Credential:");
		token_dump (1, p->perm_creds, 1, 1);
	}
}

/* perm_dump -- Dump the permissions data cache. */

void perm_dump (void)
{
	PermissionsData_t	*p;
	unsigned		h, n = 0;

	dbg_printf ("Permissions data:\r\n");
	for (h = 0; h < MAX_PERM_HASH; h++)
		for (p = perm_ht [h]; p; p = p->next) {
			dump_permissions (p);
			n++;
		}
	if (!n)
		dbg_printf ("\tPermission cache is empty!\r\n");
}

#endif
