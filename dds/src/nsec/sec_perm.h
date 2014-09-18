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

/* sec_perm.h -- Permissions handle cache, as used by both authentication and
	         access control security mechanisms. */

#ifndef __sec_perm_h_
#define __sec_perm_h_

#include "sec_data.h"
#include "sec_plugin.h"

#define	MAX_P_PLUGINS	3	/* Max. # of permission plugins/id. */

typedef struct permissions_data_st PermissionsData_t;
struct permissions_data_st {
	Permissions_t	  	  perm;		/* Handle. */
	const SEC_PERM		  *plugins [MAX_P_PLUGINS];
	Identity_t		  id;		/* Identity handle. */
	DDS_PermissionsCredential *perm_creds;	/* Permission credentials. */
	Token_t			  *perm_token;	/* Permissions token. */
	unsigned		  nusers;	/* # of references. */
	PermissionsData_t	  *next;	/* Next perm. in hash list.*/
};

/* Create a new Permissions entry. */

PermissionsData_t *perm_lookup (Permissions_t id, unsigned *hp);

/* Lookup an existing Permissions entry.  The last parameter should be NULL
   for external use. */

PermissionsData_t *perm_create (Permissions_t id, DDS_ReturnCode_t *ret);

/* Create a new Permissions entry for the given handle. */

void perm_release (Permissions_t id);

/* Free an existing Permissions entry by the handle. */

PermissionsData_t *perm_ref (PermissionsData_t *p);

/* Make a virtual copy of an existing Permissions entry. */

void perm_unref (PermissionsData_t **p);

/* No longer needs an Permissions entry. */

int perm_add_authentications (PermissionsData_t *p, SEC_AUTH *ap);

/* Add authentication methods to a Permissions entry. */

int perm_add_permissions (PermissionsData_t *p, SEC_PERM *pp);

/* Add permissions methods to a Permissions entry. */

int perm_add_plugin (PermissionsData_t *p, const SEC_PERM *ap);

/* Add a permissions plugin to a Permissions entry. */

void perm_dump (void);

/* Dump the permissions data cache. */

#endif /* !__sec_perm_h_ */


