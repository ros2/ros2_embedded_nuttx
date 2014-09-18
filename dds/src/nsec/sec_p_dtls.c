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

/* sec_p_dtls.c -- Support for legacy, i.e. DTLS-based security
		   Permissions handling plugin implementation. */

#include "error.h"
#include "md5.h"
#include "pid.h"
#include "sec_perm.h"
#include "sec_plugin.h"
#include "sec_p_dtls.h"

/* sd_get_perm_token -- Get a PermissionsToken from a permissions handle. */

static Token_t *sd_get_perm_token (const SEC_PERM *pp, Permissions_t perm)
{
	Token_t			*token;
	DDS_IdentityToken	*tp;
	DDS_OctetSeq		*p;
	PermissionsData_t	*pdp;
	DDS_SecurityReqData	data;
	char			buf [128];
	char			*bp;
	MD5_CONTEXT		md5;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (pp)

	token = xmalloc (sizeof (Token_t));
	if (!token)
		return (NULL);

	pdp = perm_lookup (perm, NULL);
	if (!pdp)
		goto no_perm;

	data.handle = pdp->id;
	data.data = NULL;
	data.length = 0;
	ret = sec_authentication_request (DDS_GET_ID_NAME, &data);
	if (ret || !data.rlength)
		goto no_perm;

	if (data.rlength >= sizeof (buf)) {
		bp = malloc (data.rlength);
		if (!bp)
			goto no_perm;
	}
	else
		bp = buf;

	data.handle = pdp->id;
	data.data = bp;
	data.length = data.rlength;
	ret = sec_authentication_request (DDS_GET_ID_NAME, &data);
	if (ret)
		goto done;

	tp = DDS_DataHolder__alloc (GMCLASSID_SECURITY_DTLS_PERM_TOKEN);
	if (!tp)
		goto done;

	tp->binary_value1 = p = DDS_OctetSeq__alloc ();
	if (!p)
		goto out_of_mem;

	ret = dds_seq_require (p, 16);
	if (ret) {
		DDS_DataHolder__free (tp);
		return (NULL);
	}
	md5_init (&md5);
	md5_update (&md5, (unsigned char *) bp, strlen (bp));
	md5_final (DDS_SEQ_DATA (*p), &md5);
	if (bp != buf)
		free (bp);

	token->data = tp;
	token->encoding = PID_V_PERMS;
	token->integral = 0;
	token->nusers = 1;
	token->next = NULL;
	return (token);

    out_of_mem:
	DDS_DataHolder__free (tp);

    done:
	if (bp != buf)
		free (bp);

    no_perm:
	xfree (token);
	return (NULL);
}

/* sd_free_perm_token -- Release a previously received PermissionsToken. */

static void sd_free_perm_token (const SEC_PERM *pp, Permissions_t h)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (h)
	
	/* ... TBC ... */
}

/* sd_get_perm_creds -- Get a PermissionsCredential token from a permissions
			handle. */

static DDS_PermissionsCredential *sd_get_perm_creds (const SEC_PERM *pp,
					             Permissions_t  id)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	/* ... TBC ... */

	return (NULL);
}

/* sd_free_perm_creds -- Release previously received Permissions Credentials.*/

static void sd_free_perm_creds (const SEC_PERM            *pp, 
			        DDS_PermissionsCredential *creds)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (creds)

	/* ... TBC ... */
}

/* sd_valid_local_perms -- Check a local permissions handle to verify that
			   a token can be produced for it. */

static DDS_ReturnCode_t sd_valid_local_perms (const SEC_PERM *pp,
					      Identity_t     id,
					      Permissions_t  perms)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	return ((perms) ? DDS_RETCODE_OK : DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* sd_check_rem_perms -- Get a permissions handle for a given token. */

static Permissions_t sd_check_rem_perms (const SEC_PERM            *pp,
					 Identity_t                local,
					 Identity_t                remote,
					 DDS_PermissionsToken      *token,
					 DDS_PermissionsCredential *cred)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (pp)
	ARG_NOT_USED (cred)

	data.secure = local;
	data.handle = remote;
	data.name = token->class_id;
	data.data = DDS_SEQ_DATA (*token->binary_value1);
	data.length = DDS_SEQ_LENGTH (*token->binary_value1);
	ret = sec_access_control_request (DDS_VALIDATE_REMOTE_PERM, &data);
	if (ret)
		return (0);
	else
		return (data.handle);
}


/* DTLS-based security permissions handling plugin: */

static SEC_PERM sd_perm_dtls = {
	SECC_DTLS_UDP,

	NULL,
	GMCLASSID_SECURITY_DTLS_PERM_TOKEN,
	16,

	sd_valid_local_perms,
	sd_check_rem_perms,
	sd_get_perm_token,
	sd_free_perm_token,
	sd_get_perm_creds,
	sd_free_perm_creds
};

int sec_perm_add_dtls (void)
{
	return (sec_perm_add (&sd_perm_dtls, 0));
}

