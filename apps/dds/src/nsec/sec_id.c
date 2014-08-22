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

/* sec_id.c -- Identity handle cache, as used by both authentication and
	       access control security mechanisms. */

#include "error.h"
#include "sec_id.h"

#define	MAX_ID_HASH	16
#define	ID_HASH(id)	(id & (MAX_ID_HASH - 1))

static IdentityData_t	*id_ht [MAX_ID_HASH];

static IdentityData_t *id_new (Identity_t id)
{
	IdentityData_t	*p;

	p = xmalloc (sizeof (IdentityData_t));
	if (!p)
		return (NULL);

	memset (p, 0, sizeof (IdentityData_t));
	p->id = id;
	p->nusers = 1;
	return (p);
}

IdentityData_t *id_lookup (Identity_t id, unsigned *hp)
{
	IdentityData_t	*p;
	unsigned	h = ID_HASH (id);

	if (hp)	
		*hp = h;
	for (p = id_ht [h]; p; p = p->next)
		if (p->id == id)
			return (p);

	return (NULL);
}

static void id_add (IdentityData_t *p, unsigned h)
{
	p->next = id_ht [h];
	id_ht [h] = p;
}

IdentityData_t *id_create (Identity_t id, DDS_ReturnCode_t *ret)
{
	IdentityData_t	*p;
	unsigned	h;

	p = id_lookup (id, &h);
	if (p) {
		*ret = DDS_RETCODE_OK;
		return (p);
	}
	p = id_new (id);
	if (!p) {
		*ret = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	id_add (p, h);
	return (p);
}

static void id_release_p (IdentityData_t *p)
{
	IdentityData_t	*xp, *prev;
	unsigned	h;

	if (p->id_creds) {
		DDS_DataHolder__free (p->id_creds);
		p->id_creds = NULL;
	}
	if (p->perm_cred) {
		DDS_DataHolder__free (p->perm_cred);
		p->perm_cred = NULL;
	}
	if (p->id_token) {
		token_unref (p->id_token);
		p->id_token = NULL;
	}
	if (p->perm_token) {
		token_unref (p->perm_token);
		p->perm_token = NULL;
	}
	h = ID_HASH (p->id);
	for (xp = id_ht [h], prev = NULL; xp; prev = xp, xp = xp->next)
		if (xp == p) {
			if (prev)
				prev->next = p->next;
			else
				id_ht [h] = p->next;
			break;
		}
	xfree (p);
}

static void id_free (IdentityData_t *p)
{
	if (!p || --p->nusers)
		return;

	id_release_p (p);
}

void id_release (Identity_t id)
{
	unsigned	h;

	id_free (id_lookup (id, &h));
}

IdentityData_t *id_ref (IdentityData_t *p)
{
	if (!p)
		return (NULL);

	p->nusers++;
	return (p);
}

void id_unref (IdentityData_t **p)
{
	if (!p)
		return;

	if (!--(*p)->nusers) {
		id_release_p (*p);
		*p = NULL;
	}
}

/* id_add_plugin -- Add an authentication plugin to an Identity. */

int id_add_plugin (IdentityData_t *p, const SEC_AUTH *ap)
{
	unsigned	i;

	for (i = 0; i < MAX_A_PLUGINS; i++)
		if (!p->plugins [i]) {
			p->plugins [i] = ap;
			return (DDS_RETCODE_OK);
		}

	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

#ifdef DDS_DEBUG

static void dump_identity (IdentityData_t *p)
{
	dbg_printf ("%4u", p->id);
	if (p->nusers != 1)
		dbg_printf ("*%u", p->nusers);
	dbg_printf (":\r\n");
	if (p->id_token) {
		dbg_printf ("    Identity Token:\t");
		token_dump (1, p->id_token->data, p->id_token->nusers, 0);
	}
	if (p->id_creds) {
		dbg_printf ("    Identity Credential:");
		token_dump (1, p->id_creds, 1, 1);
	}
	if (p->perm_token) {
		dbg_printf ("    Permission Token:\t");
		token_dump (1, p->perm_token->data, p->perm_token->nusers, 0);
	}
	if (p->perm_cred) {
		dbg_printf ("    Permission Credential:");
		token_dump (1, p->perm_cred, 1, 1);
	}
}	

/* id_dump -- Dump the identity data cache. */

void id_dump (void)
{
	IdentityData_t	*p;
	unsigned	h, n = 0;

	dbg_printf ("Identity data:\r\n");
	for (h = 0; h < MAX_ID_HASH; h++)
		for (p = id_ht [h]; p; p = p->next) {
			dump_identity (p);
			n++;
		}
	if (!n)
		dbg_printf ("\tIdentity cache is empty!\r\n");
}

#endif
