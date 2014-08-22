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

/* sp_auth.c -- DDS Security Plugin - Authentication plugin implementations. */

#include "dds/dds_error.h"
#include "dds/dds_security.h"
#include "sp_auth.h"
#include "sp_access.h"
#include "sp_cred.h"
#include "sp_cert.h"
#include "log.h"
#include "error.h"
#include <stdlib.h>

#define SP_AUTH_LOG

/* This function shall check and store the local credentials */

DDS_ReturnCode_t sp_validate_local_id (const char          *name,
				       DDS_Credentials     *credential,
				       const unsigned char *pkey,
				       size_t              klength,
				       IdentityHandle_t    *local_id,
				       int                 *validation)
{
	DDS_ReturnCode_t ret;
	unsigned char    *cert, *key;
	size_t           cert_len, key_len;
	IdentityHandle_t id;

	/* Call access control function */
	if ((ret = sp_access_validate_local_id (name, strlen (name), pkey, klength, &id))) {
		validation = DDS_AA_REJECTED;
		return (ret);
	}
	/* Add this credential to the database */
	if ((ret = sp_extract_pem (credential, &cert, &cert_len, &key, &key_len))) {
		validation = DDS_AA_REJECTED;
		return (ret);
	}
	if ((ret = sp_add_credential (id, (char *) name, cert, cert_len, key, key_len)))
		goto add_credential_failed;
	
	if ((ret = sp_validate_certificate (id, id)))
		goto validation_failed;

	free (cert);
	free (key);
	*local_id = id;
	*validation = DDS_AA_HANDSHAKE;
#ifdef SP_AUTH_LOG
		log_printf (SEC_ID, 0, "SP_AUTH: Validate local id returned handshake request\r\n");
#endif
	return (DDS_RETCODE_OK);

 validation_failed:
	sp_remove_credential (id);
 add_credential_failed:
	free (cert);
	free (key);
	*local_id = 0;
	validation = DDS_AA_REJECTED;
#ifdef SP_AUTH_LOG
		log_printf (SEC_ID, 0, "SP_AUTH: Validate local id returned rejected\r\n");
#endif
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* This function shall decide wether the handshake needs to be performed */

DDS_ReturnCode_t sp_validate_remote_id (const char          *method,
					const unsigned char *identity,
					size_t              length,
					const unsigned char *key,
					size_t              klength,
					int                 *validation,
					IdentityHandle_t    *remote_id)
{	
	return (sp_access_validate_remote_id (method,
					      identity,
					      length,
					      key,
					      klength,
					      validation,
					      remote_id));
}

/* Only needs to check the remote certificate and store the credentials */

DDS_ReturnCode_t sp_verify_remote_credentials (IdentityHandle_t local_id,
					       IdentityHandle_t remote_id,
					       unsigned char    *cert,
					       size_t           cert_len,
					       int              *validation)
{
	DDS_ReturnCode_t ret;

	if ((ret = sp_add_credential (remote_id, NULL, cert, cert_len, NULL, 0)))
		return (ret);

	if ((ret = sp_validate_certificate (remote_id, local_id)))
		goto validation_failed;

	if ((ret = sp_access_add_common_name (remote_id, cert, cert_len)))
		goto validation_failed;

	*validation = DDS_AA_ACCEPTED;
#ifdef SP_AUTH_LOG
		log_printf (SEC_ID, 0, "SP_AUTH: verify remote credentials returned accepted\r\n");
#endif
	return (DDS_RETCODE_OK);

 validation_failed:
	sp_remove_credential (remote_id);
	sp_access_remove_unchecked_participant (remote_id);
	*validation = DDS_AA_REJECTED;
#ifdef SP_AUTH_LOG
		log_printf (SEC_ID, 0, "SP_AUTH: verify remote credentials returned rejected\r\n");
#endif
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

DDS_ReturnCode_t sp_auth_get_name (IdentityHandle_t id,
				   char             *name,
				   size_t           length,
				   size_t           *rlength)
{
	char *tmp;

	tmp = sp_get_name (id);

	if (tmp) {
		*rlength = strlen (tmp) + 1;
		if (*rlength > length)
			rlength = 0;
		else if (name)
			strcpy (name, tmp);
	}

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_auth_get_id_credential (IdentityHandle_t id,
					    unsigned char    *cred,
					    size_t           length,
					    size_t           *rlength)
{
	unsigned char *cert;

	cert = sp_get_cert (id, rlength);
	if (cred) {
		if (cert)
			memcpy (cred, cert, length + 1);
		else
			return (DDS_RETCODE_BAD_PARAMETER);
	}

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_release_identity (IdentityHandle_t id)
{
	DDS_ReturnCode_t ret;

	/* Remove credential */
	ret = sp_remove_credential (id);
	
	/* Remove participant from access control */
	ret = sp_access_remove_participant (id);

	return (ret);
}
