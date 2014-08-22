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

/* sp_auth.h -- DDS Security Plugin - Authentication plugin definitions. */

#ifndef __sp_auth_h_
#define __sp_auth_h_

#include "dds/dds_error.h"
#include "dds/dds_plugin.h"
#include "dds/dds_security.h"
#include "nsecplug/nsecplug.h"

DDS_ReturnCode_t sp_validate_local_id (const char          *name,
				       DDS_Credentials     *credential,
				       const unsigned char *key,
				       size_t              klength,
				       IdentityHandle_t    *local_id,
				       int                 *validation);

/* Validate local credentials.
   When successful, a new local identity handle is returned. */

DDS_ReturnCode_t sp_validate_remote_id (const char          *method,
					const unsigned char *identity,
					size_t              length,
					const unsigned char *key,
					size_t              klength,
					int                 *validation,
					IdentityHandle_t    *remote_id);

/* Return a remote identity handle. */

DDS_ReturnCode_t sp_verify_remote_credentials (IdentityHandle_t local_id,
					       IdentityHandle_t remote_id,
					       unsigned char    *cert,
					       size_t           cert_len,
					       int              *validation);

/* Validate remote credentials. */

DDS_ReturnCode_t sp_auth_get_name (IdentityHandle_t id, 
				   char             *name,
				   size_t           length,
				   size_t           *rlength);

/* Get the participant name */


DDS_ReturnCode_t sp_auth_get_id_credential (IdentityHandle_t id,
					    unsigned char    *cred,
					    size_t           length,
					    size_t           *rlength);

/* Get the id credential */

DDS_ReturnCode_t sp_auth_remote_accepted (IdentityHandle_t    id,
					  const unsigned char *identity,
					  size_t              length,
					  int                 *action);

/* The remote is authenticated by means of a handshake, 
   now check if the access control plugin will allow the remote */

DDS_ReturnCode_t sp_release_identity (IdentityHandle_t identity);

/* Remove a stored identity. */

#endif /* !__sp_auth_h_ */

