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


#ifndef __sp_cred_h_
#define __sp_cred_h_

#include "dds/dds_error.h"
#include "nsecplug/nsecplug.h"

DDS_ReturnCode_t sp_add_credential (IdentityHandle_t id,
				    char             *name,
				    unsigned char    *cert,
				    size_t           cert_len,
				    unsigned char    *key,
				    size_t           key_len);

/* Add a credential to the database */

char *sp_get_name (IdentityHandle_t id);

/* Get the participant name */

unsigned char *sp_get_cert (IdentityHandle_t id,
			    size_t           *len);

/* Get a certificate in pem from the database */

unsigned char *sp_get_key (IdentityHandle_t id,
			   size_t           *len);

/* Get a private key in pem from the database */

DDS_ReturnCode_t sp_remove_credential (IdentityHandle_t id);

/* remove a credential from the database */

DDS_ReturnCode_t sp_extract_pem (DDS_Credentials *cred,
				 unsigned char   **cert,
				 size_t          *cert_len,
				 unsigned char   **key,
				 size_t          *key_len);

#endif /* !__sp_cred_h_ */
