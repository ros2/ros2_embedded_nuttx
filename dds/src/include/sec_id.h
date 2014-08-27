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

/* sec_id.h -- Identity handle cache, as used by both authentication and
	       access control security mechanisms. */

#ifndef __sec_id_h_
#define __sec_id_h_

#include "sec_data.h"
#include "sec_plugin.h"

#define	MAX_A_PLUGINS	4	/* Max. # of authentication plugins/id. */

typedef struct identity_data_st IdentityData_t;
struct identity_data_st {
	Identity_t		  id;		/* Handle. */
	const SEC_AUTH		  *plugins [MAX_A_PLUGINS];
	DDS_IdentityCredential    *id_creds;	/* Identity credentials. */
	Token_t			  *id_token;	/* Identity token. */
	DDS_PermissionsCredential *perm_cred;	/* Permission credentials. */
	Token_t			  *perm_token;	/* Permission token. */
	unsigned		  nusers;	/* # of references. */
	IdentityData_t		  *next;	/* Next id. in hash list. */
};

/* Create a new Identity entry. */

IdentityData_t *id_lookup (Identity_t id, unsigned *hp);

/* Lookup an existing Identity entry.  The last parameter should be NULL
   for external use. */

IdentityData_t *id_create (Identity_t id, DDS_ReturnCode_t *ret);

/* Create a new Identity entry for the given handle. */

void id_release (Identity_t id);

/* Free an existing Identity entry by the handle. */

IdentityData_t *id_ref (IdentityData_t *p);

/* Make a virtual copy of an existing Identity entry. */

void id_unref (IdentityData_t **p);

/* No longer needs an Identity entry. */

int id_add_plugin (IdentityData_t *p, const SEC_AUTH *ap);

/* Add an authentication plugin to an Identity. */

void id_dump (void);

/* Dump the identity data cache. */

#endif /* !__sec_id_h_ */

