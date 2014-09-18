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

/* sec_a_dtls.c -- Support for legacy, i.e. DTLS-based security. */

#include "error.h"
#include "pid.h"
#include "sec_data.h"
#include "sec_auth.h"
#include "sec_plugin.h"
#include "sec_a_dtls.h"

/* sd_check_local -- Check a local participant. */

static DDS_ReturnCode_t sd_check_local (const SEC_AUTH *ap,
					Identity_t     local,
					unsigned char  participant_key [16])
{
	ARG_NOT_USED (ap)
	ARG_NOT_USED (local)
	ARG_NOT_USED (participant_key)

	return (DDS_RETCODE_OK);
}

/* sd_validate_rem -- Validate a remote participant.  If successful, the
		      replier identity handle will be set. */

static AuthState_t sd_validate_rem (const SEC_AUTH       *ap,
				    Identity_t           initiator,
				    unsigned char        init_key [16],
				    DDS_IdentityToken    *rem_id_tok,
				    DDS_PermissionsToken *rem_perm_tok,
				    unsigned char        rem_key [16],
				    Identity_t           *replier)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (ap)
	ARG_NOT_USED (initiator)
	ARG_NOT_USED (init_key)
	ARG_NOT_USED (rem_perm_tok)

	if (!rem_id_tok ||
	    !rem_id_tok->binary_value1 ||
	    !DDS_SEQ_LENGTH (*rem_id_tok->binary_value1))
		return (AS_FAILED);

	data.tag = NULL;
	data.data = DDS_SEQ_DATA (*rem_id_tok->binary_value1);
	data.length = DDS_SEQ_LENGTH (*rem_id_tok->binary_value1);
	data.rdata = rem_key;
	data.rlength = 16;
	ret = sec_authentication_request (DDS_VALIDATE_REMOTE_ID, &data);
	if (ret || data.action == DDS_AA_REJECTED)
		return (AS_FAILED);
	else {
		*replier = data.handle;
		return (AS_OK);
	}
}

/* sd_get_id_token -- Get an IdentityToken for a local Identity handle. */

static Token_t *sd_get_id_token (const SEC_AUTH *ap, Identity_t id)
{
	Token_t			*token;
	DDS_IdentityToken	*tp;
	DDS_OctetSeq		*p;
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (ap)

	token = xmalloc (sizeof (Token_t));
	if (!token)
		return (NULL);

	data.handle = id;
	data.data = NULL;
	data.length = 0;
	ret = sec_authentication_request (DDS_GET_ID_NAME, &data);
	if (ret || !data.rlength)
		goto no_mem;

	tp = DDS_DataHolder__alloc (GMCLASSID_SECURITY_DTLS_ID_TOKEN);
	if (!tp)
		goto no_mem;

	tp->binary_value1 = p = DDS_OctetSeq__alloc ();
	if (!p)
		goto free_dh;

	ret = dds_seq_require (p, data.rlength);
	if (ret)
		goto free_dh;

	data.length = data.rlength;
	data.data = DDS_SEQ_DATA (*p);
	ret = sec_authentication_request (DDS_GET_ID_NAME, &data);
	if (ret || !data.rlength)
		goto free_dh;

	token->data = tp;
	token->encoding = PID_V_IDENTITY;
	token->integral = 0;
	token->nusers = 1;
	token->next = NULL;
	return (token);

    free_dh:
	DDS_DataHolder__free (tp);
    no_mem:
	xfree (token);
	return (NULL);
}

/* sd_release_id -- Release a previously received identity token and identity
		    credentials. */

static void sd_release_id (const SEC_AUTH *ap, Identity_t h)
{
	DDS_SecurityReqData	data;

	ARG_NOT_USED (ap)

	data.handle = h;
	sec_authentication_request (DDS_RELEASE_ID, &data);
}

/* sd_create_req -- Create a new Handshake Request message token.
		    Not needed for DTLS which does a handshake on transport
		    level with each peer. */

static DDS_HandshakeToken *sd_create_req (const SEC_AUTH            *ap,
				          Identity_t                req,
				          void                      **pdata)
{
	ARG_NOT_USED (ap)
	ARG_NOT_USED (req)
	ARG_NOT_USED (pdata)

	return (NULL);
}

/* sd_create_reply -- Validate a received Handshake Request token and create a
		      new Handshake Reply message token if it was valid.
		      Not needed for DTLS which does a handshake on transport
		      level with each peer. */

static DDS_DataHolder *sd_create_reply (const SEC_AUTH            *ap,
					DDS_HandshakeToken        *req,
					Identity_t                replier,
					Identity_t                peer,
					void                      **pdata,
					DDS_IdentityCredential    *rem_id_tok,
					DDS_PermissionsCredential *rem_perm_tok,
					DDS_IdentityCredential    **rem_id,
					DDS_PermissionsCredential **rem_perm,
					DDS_ReturnCode_t          *error)
{
	ARG_NOT_USED (ap)
	ARG_NOT_USED (req)
	ARG_NOT_USED (replier)
	ARG_NOT_USED (peer)
	ARG_NOT_USED (pdata)
	ARG_NOT_USED (rem_id_tok)
	ARG_NOT_USED (rem_perm_tok)

	*rem_id = NULL;
	*rem_perm = NULL;
	*error = DDS_RETCODE_UNSUPPORTED;

	return (NULL);
}

/* sd_create_final -- Validate a Handshake Reply message and create a new
		      Handshake Final message token if it was valid, as well
		      as a shared secret for which the handle is given in
		      *secret. Not needed for DTLS which does a handshake on
		      transport level with each peer.*/

static DDS_DataHolder *sd_create_final (const SEC_AUTH            *ap,
					DDS_HandshakeToken        *reply,
					Identity_t                req,
					Identity_t                peer,
					void                      **pdata,
					DDS_IdentityToken         *rem_id_tok,
					DDS_PermissionsToken      *rem_perm_tok,
					DDS_IdentityCredential    **rem_id,
					DDS_PermissionsCredential **rem_perm,
					SharedSecret_t            *secret,
					DDS_ReturnCode_t          *error)
{
	ARG_NOT_USED (ap)
	ARG_NOT_USED (reply)
	ARG_NOT_USED (req)
	ARG_NOT_USED (peer)
	ARG_NOT_USED (pdata)
	ARG_NOT_USED (rem_id_tok)
	ARG_NOT_USED (rem_perm_tok)
	ARG_NOT_USED (secret)

	*rem_id = NULL;
	*rem_perm = NULL;
	*secret = 0;
	*error = DDS_RETCODE_UNSUPPORTED;

	return (NULL);
}

/* sd_check_final == Validate a Handshake Final message and return the
		     associated shared secret handle if successful.
		     Not needed for DTLS which does a handshake on transport
		     level with each peer. */

static DDS_ReturnCode_t sd_check_final (const SEC_AUTH            *ap,
					DDS_HandshakeToken        *final,
					Identity_t                replier,
					Identity_t                peer,
					void                      **pdata,
					DDS_IdentityToken         *rem_id_tok,
					DDS_PermissionsToken      *rem_perm_tok,
					DDS_IdentityCredential    **rem_id,
					DDS_PermissionsCredential **rem_perm,
					SharedSecret_t            *secret)
{
	ARG_NOT_USED (ap)
	ARG_NOT_USED (final)
	ARG_NOT_USED (replier)
	ARG_NOT_USED (peer)
	ARG_NOT_USED (pdata)
	ARG_NOT_USED (rem_id_tok)
	ARG_NOT_USED (rem_perm_tok)
	ARG_NOT_USED (rem_id)
	ARG_NOT_USED (rem_perm)
	ARG_NOT_USED (secret)

	*secret = 0;

	return (DDS_RETCODE_UNSUPPORTED);
}

/* sd_free_secret -- The shared secret is no longer needed and can be disposed.
		     Not needed for DTLS which handles its own keys. */

static void sd_free_secret (const SEC_AUTH *ap, SharedSecret_t secret)
{
	ARG_NOT_USED (ap)
	ARG_NOT_USED (secret)
}

static SEC_AUTH sd_auth_dtls = {
	SECC_DTLS_UDP,

	NULL,
	GMCLASSID_SECURITY_DTLS_ID_TOKEN,
	0,
	sd_check_local,
	sd_validate_rem,
	sd_get_id_token,
	sd_release_id,

	NULL,

	NULL,
	NULL,
	NULL,
	sd_create_req,
	sd_create_reply,
	sd_create_final,
	sd_check_final,

	NULL,
	sd_free_secret
};

int sec_auth_add_dtls (void)
{
	return (sec_auth_add (&sd_auth_dtls, 0));
}

