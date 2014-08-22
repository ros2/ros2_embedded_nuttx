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

/* sec_p_std.c -- Support for standard DDS security.
		  Permissions handling plugin implementation. */

#include "error.h"
#include "sec_plugin.h"
#include "sec_util.h"
#include "sec_perm.h"
#include "sec_p_std.h"

/* sp_valid_local_perms -- Check a local permissions handle to verify that
			   a token can be produced for it. */

static DDS_ReturnCode_t sp_valid_local_perms (const SEC_PERM *pp,
					      Identity_t     id,
					      Permissions_t  perms)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	return ((perms) ? DDS_RETCODE_OK : DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* sp_check_rem_perms -- Get a permissions handle for a given token. */

static Permissions_t sp_check_rem_perms (const SEC_PERM            *pp,
					 Identity_t                local,
					 Identity_t                remote,
					 DDS_PermissionsToken      *token,
					 DDS_PermissionsCredential *cred)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (pp)
	ARG_NOT_USED (token)

	data.handle = remote;
	data.secure = local;
	data.data = DDS_SEQ_DATA (*cred->binary_value1);
	data.length = DDS_SEQ_LENGTH (*cred->binary_value1);
	data.name = cred->class_id;
	ret = sec_access_control_request (DDS_VALIDATE_REMOTE_PERM, &data);
	if (ret)
		return (0);

	return (data.handle);
}

/* sp_get_perm_token -- Get a PermissionsToken from a permissions handle. */

static Token_t *sp_get_perm_token (const SEC_PERM *pp, Permissions_t id)
{
	Token_t			*tp;
	unsigned char		*p;
	DDS_SecurityReqData	data;
	DDS_PermissionsToken	*token;
	size_t			cred_len;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (pp)

	tp = xmalloc (sizeof (Token_t));
	if (!tp)
		return (NULL);

	token = DDS_DataHolder__alloc (GMCLASSID_SECURITY_PERMISSIONS_TOKEN);
	if (!token) {
		xfree (tp);
		return (NULL);
	}
	token->binary_value1 = DDS_OctetSeq__alloc ();
	if (!token->binary_value1)
		goto free_dh;

	error = dds_seq_require (token->binary_value1, 32);
	if (error)
		goto free_dh;

	data.handle = id;
	data.data = NULL;
	data.kdata = NULL;
	data.length = 0;
	data.rlength = 0;
	error = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (error)
		goto free_dh;

	cred_len = data.rlength;
	p = xmalloc (cred_len);
	if (!p)
		goto free_dh;

	data.data = p;
	data.kdata = NULL;
	data.length = cred_len;
	error = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (error)
		goto free_p;

	error = sec_hash_sha256 ((unsigned char *) p,
				 cred_len,
				 DDS_SEQ_DATA (*token->binary_value1));
	xfree (p);
	if (error)
		goto free_dh;

	tp->data = token;
	tp->nusers = 1;
	tp->encoding = 0;
	tp->integral = 0;
	tp->next = NULL;
	return (tp);

    free_p:
    	xfree (p);
    free_dh:
	DDS_DataHolder__free (token);
	xfree (tp);
	return (NULL);
}

/* sp_free_perm_token -- Release a previously received PermissionsToken. */

static void sp_free_perm_token (const SEC_PERM *pp, Permissions_t h)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (h)

	/* ... TBC ... */
}

/* sp_get_perm_creds -- Get a PermissionsCredential token from a permissions
			handle. */

static DDS_PermissionsCredential *sp_get_perm_creds (const SEC_PERM *pp,
					             Permissions_t  id)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	/* ... TBC ... */

	return (NULL);
}

/* sp_free_perm_creds -- Release previously received Permissions Credentials.*/

static void sp_free_perm_creds (const SEC_PERM            *pp, 
			        DDS_PermissionsCredential *creds)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (creds)

	/* ... TBC ... */
}

/* Standard DDS security permissions handling plugin: */

static SEC_PERM sp_perm_std = {
	SECC_DDS_SEC,

	GMCLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN,
	GMCLASSID_SECURITY_PERMISSIONS_TOKEN,
	32,

	sp_valid_local_perms,
	sp_check_rem_perms,
	sp_get_perm_token,
	sp_free_perm_token,
	sp_get_perm_creds,
	sp_free_perm_creds
};

int sec_perm_add_std (void)
{
	return (sec_perm_add (&sp_perm_std, 0));
}


