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

/* sec_a_std.c -- Implements the standard, i.e. default security plugin for
		  PKI-RSA and DSA-DH authentication security methods. */

#include "log.h"
#include "error.h"
#include "dds/dds_security.h"
#include "sec_util.h"
#include "sec_auth.h"
#include "sec_plugin.h"
#include "sec_p_std.h"

typedef struct sec_std_data_st StdData_t;

typedef struct nonces_st {
	unsigned char	nonce_a [NONCE_LENGTH];
	unsigned char	nonce_b [NONCE_LENGTH];
} Nonces_t;

typedef struct secrets_st {
	unsigned char	secret [SHARED_SECRET_LENGTH];
	unsigned char	kx_key [DERIVED_KEY_LENGTH];
	unsigned char	kx_mac_key [DERIVED_KEY_LENGTH];
} Secrets_t;

struct sec_std_data_st {
	SharedSecret_t	h;
	union {
	  Nonces_t	nonces;
	  Secrets_t	secrets;
	}		data;
	StdData_t	*next;
};

static SharedSecret_t	handles;

static StdData_t *secrets;

static DDS_ReturnCode_t aps_get_kx (const SEC_AUTH *plugin,
				    SharedSecret_t secret,
				    unsigned char  *kx_key,
				    unsigned char  *kx_mac_key)
{
	StdData_t	*p;

	ARG_NOT_USED (plugin)

	for (p = secrets; p; p = p->next)
		if (p->h == secret) {
			if (kx_key)
				memcpy (kx_key, p->data.secrets.kx_key, DERIVED_KEY_LENGTH);
			if (kx_mac_key)
				memcpy (kx_mac_key, p->data.secrets.kx_mac_key, DERIVED_KEY_LENGTH);

			return (DDS_RETCODE_OK);
		}

	return (DDS_RETCODE_ALREADY_DELETED);
}

static void calculate_effective_key (char          *name,
				     size_t        name_len,
				     unsigned char *key)
{
	unsigned char	candidate_key [16];
	unsigned char	hash [32];

	memcpy (candidate_key, key, 16);
	memset (key, 0, 16);

	/* Hash subject name and copy 6 bytes. */
	sec_hash_sha256 ((unsigned char *) name, name_len, hash);
	memcpy (key, hash, 6);

	/* Set the first bit to 1. */
	key [0] |= 0x01;

	/* Hash candidate part key and copy 6 bytes. */
	sec_hash_sha256 (candidate_key, 16, hash);
	memcpy (&key [6], hash, 6);

	/* Copy the 4 last corresponding bytes of the candidate key. */
	memcpy (&key [12], &candidate_key [12], 4);
}

/* aps_check_local -- Check if plugin can handle a local participant for
		      negotiation.  If it is possible, the participant key
		      may be altered by the plugin code. */

static DDS_ReturnCode_t aps_check_local (const SEC_AUTH *ap,
					 Identity_t     local,
					 unsigned char  participant_key [16])
{
	DDS_SecurityReqData	data;
	char			name [256];
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (ap)

	/* We need a credential or we can't accept the user.  Check if we have it. */
	data.handle = local;
	data.data = NULL;
	data.length = 0;
	error = sec_authentication_request (DDS_GET_ID_CREDENTIAL, &data);
	if (error || !data.rlength)
		return (DDS_RETCODE_UNSUPPORTED);

	/* Get the user name. */
	data.handle = local;
	data.data = name;
	data.length = sizeof (name);
	error = sec_authentication_request (DDS_GET_ID_NAME, &data);
	if (error || !data.rlength)
		return (DDS_RETCODE_UNSUPPORTED);

	calculate_effective_key (name, data.rlength, participant_key);

	return (DDS_RETCODE_OK);
}

/* aps_validate_remote -- A remote participant was detected having this plugin's
			  class id for its IdentityToken. Register it in the
			  lower-layer security database. */

static AuthState_t aps_validate_remote (const SEC_AUTH       *ap,
					Identity_t           initiator,
					unsigned char        init_key [16],
					DDS_IdentityToken    *rem_id_tok,
					DDS_PermissionsToken *rem_perm_tok,
					unsigned char        rem_key [16],
					Identity_t           *replier)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (ap)
	ARG_NOT_USED (initiator)
	ARG_NOT_USED (rem_perm_tok)
	ARG_NOT_USED (replier)

	if (!rem_id_tok->binary_value1 ||
	    DDS_SEQ_LENGTH (*rem_id_tok->binary_value1) != 32)
		return (AS_FAILED);

	data.tag = rem_id_tok->class_id;
	data.data = DDS_SEQ_DATA (*rem_id_tok->binary_value1);
	data.length = DDS_SEQ_LENGTH (*rem_id_tok->binary_value1);
	data.kdata = rem_key;
	data.klength = 16;
	error = sec_authentication_request (DDS_VALIDATE_REMOTE_ID, &data);
	if (error || data.action == DDS_AA_REJECTED)
		return (AS_FAILED);

	*replier = data.handle;
	if (data.action == DDS_AA_ACCEPTED)
		return (AS_OK);

	/* data.action == DDS_AA_HANDSHAKE */
	if (memcmp (rem_key, init_key, 12) > 0)
		return (AS_PENDING_HANDSHAKE_REQ);
	else
		return (AS_PENDING_CHALLENGE_MSG);
}

/* aps_get_identity_data -- Get Identity credentials. */

static char *aps_get_identity_data (Identity_t       id,
				    DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData	data;
	char			*dp;
	size_t			cred_len;
	DDS_ReturnCode_t	ret;

	data.handle = id;
	data.data = NULL;
	data.length = 0;
	ret = sec_authentication_request (DDS_GET_ID_CREDENTIAL, &data);
	if (ret || !data.rlength)
		goto done;

	cred_len = data.rlength;
	dp = xmalloc (cred_len + 1);
	if (!dp)
		goto done;

	data.data = dp;
	data.length = cred_len;
	ret = sec_authentication_request (DDS_GET_ID_CREDENTIAL, &data);
	if (ret)
		goto done;

	dp [cred_len] = '\0';
	return (dp);

    done:
	*error = ret;
	return (NULL);
}

/* aps_get_id_token-- Get an IdentityToken from an Identity handle. */

static Token_t *aps_get_id_token (const SEC_AUTH *ap, Identity_t id)
{
	char			*p;
	DDS_IdentityToken	*token;
	Token_t			*tp;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (ap)

	tp = xmalloc (sizeof (Token_t));
	if (!tp)
		return (NULL);

	token = DDS_DataHolder__alloc (GMCLASSID_SECURITY_IDENTITY_TOKEN);
	if (!token) {
		xfree (tp);
		return (NULL);
	}
	tp->data = token;
	tp->nusers = 1;
	tp->integral = 0;
	tp->encoding = 0;
	tp->next = NULL;

	token->binary_value1 = DDS_OctetSeq__alloc ();
	if (!token->binary_value1)
		goto free_dh;

	error = dds_seq_require (token->binary_value1, 32);
	if (error)
		goto free_dh;

	p = aps_get_identity_data (id, &error);
	if (!p)
		goto free_dh;

	log_printf (SEC_ID, 0, "Identity Credential for [%u]: %s\r\n", id, p);

	error = sec_hash_sha256 ((unsigned char *) p,
				 strlen (p),
				 DDS_SEQ_DATA (*token->binary_value1));
	xfree (p);
	if (error)
		goto free_dh;

	return (tp);

    free_dh:
	token_unref (tp);
	return (NULL);
}

/* Release previously received identity token and credentials. */

static void aps_release_id (const SEC_AUTH *ap, Identity_t id)
{
	DDS_SecurityReqData	data;

	ARG_NOT_USED (ap)

	if (!plugin_policy) 
		return;

	data.handle = id;
	sec_authentication_request (DDS_RELEASE_ID, &data);
}

static int aps_valid_id_credential (Identity_t    local,
				    Identity_t    peer,
				    unsigned char *cred,
				    size_t        cred_length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	data.handle = local;
	data.secure = peer;
	data.data = cred;
	data.length = cred_length;
	ret = sec_authentication_request (DDS_VERIFY_ID_CREDENTIAL, &data);
	return ((ret) ? 0 : data.action == DDS_AA_ACCEPTED);
}

/* aps_add_identity_property -- Add an Identity property to a handshake token. */

static DDS_ReturnCode_t aps_add_identity_property (DDS_DataHolder *p,
						   Identity_t     id)
{
	char			*s;
	DDS_Property		*pp;
	DDS_ReturnCode_t	ret;

	s = aps_get_identity_data (id, &ret);
	if (!s)
		return (ret);

	pp = DDS_DataHolder_add_property (p,
					  GMCLASSID_SECURITY_ID_CREDENTIAL_PROPERTY,
					  s);
	if (!pp) {
		xfree (s);
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
	}
	else
		ret = DDS_RETCODE_OK;
	return (ret);
}

/* aps_get_permissions_data -- Get permissions credentials. */

static char *aps_get_permissions_data (Identity_t       id,
				       DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData	data;
	unsigned char		*dp;
	size_t			perm_len;
	DDS_ReturnCode_t	ret;

	data.handle = id;
	data.data = NULL;
	data.kdata = NULL;
	data.length = 0;
	data.rlength = 0;
	ret = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (ret || !data.rlength)
		goto done;

	perm_len = data.rlength;
	dp = xmalloc (perm_len + 1);
	if (!dp)
		goto done;

	data.data = dp;
	data.kdata = NULL;
	data.length = perm_len;
	ret = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (ret)
		goto done;

	dp [perm_len] = '\0';
	return ((char *) dp);

    done:
	*error = ret;
	return (NULL);
}

/* aps_add_permissions_property -- Add a Permissions property to a handshake token. */

static DDS_ReturnCode_t aps_add_permissions_property (DDS_DataHolder *p,
					              Identity_t     id)
{
	char			*perm;
	DDS_Property		*pp;
	DDS_ReturnCode_t	ret;

	perm = aps_get_permissions_data (id, &ret);
	if (!perm)
		return (ret);

	pp = DDS_DataHolder_add_property (p, 
					  GMCLASSID_SECURITY_PERM_CREDENTIAL_PROPERTY,
					  perm);
	if (!pp) {
		xfree (perm);
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
	}
	else
		ret = DDS_RETCODE_OK;
	return (ret);
}

/*#define DUMP_HS*/
#ifdef DUMP_HS

static void dump_chunk (const char *s, const unsigned char *buf, size_t len)
{
	unsigned	i;

	log_printf (SEC_ID, 0, "\r\n%s: ", s);
	for (i = 0; i < len; i++) {
		if ((i & 0xf) == 0)
			log_printf (SEC_ID, 0, "\r\n\t");
		log_printf (SEC_ID, 0, "%02x ", buf [i]);
		if ((i & 0xf) == 7)
			log_printf (SEC_ID, 0, "- ");
	}
}

#endif

/* aps_request -- Create a new Handshake Request message token. */

static DDS_HandshakeToken *aps_request (const SEC_AUTH *ap,
				        Identity_t     req,
				        void           **pdata)
{
	DDS_HandshakeToken	*token;
	StdData_t		*p;

	ARG_NOT_USED (req)

	p = xmalloc (sizeof (StdData_t));
	if (!p)
		return (NULL);

	/* Create a proper Handshake Request message token. */
	token = DDS_DataHolder__alloc (ap->req_name);
	if (!token) {
		xfree (p);
		return (NULL);
	}
	if (aps_add_identity_property (token, req) ||
	    aps_add_permissions_property (token, req))
		goto mem_error;

	token->binary_value1 = sec_generate_nonce ();
	if (!token->binary_value1)
		goto mem_error;

	memcpy (p->data.nonces.nonce_a, DDS_SEQ_DATA (*token->binary_value1), NONCE_LENGTH);
#ifdef DUMP_HS
	dump_chunk ("nonce_a", p->data.nonces.nonce_a, NONCE_LENGTH);
	log_printf (SEC_ID, 0, "\r\n");
#endif
	p->h = 0;
	*pdata = p;
	return (token);

    mem_error:
	xfree (p);
	DDS_DataHolder__free (token);
	return (NULL);
}

/* aps_credential -- Create a credential token from a handshake message
		     credential. */

static DDS_Credential_ *aps_credential (const char    *name,
					unsigned char *credential,
					size_t        credential_length)
{
	DDS_Credential_		*token;
	DDS_OctetSeq		*p;
	DDS_ReturnCode_t	ret;

	token = DDS_DataHolder__alloc (name);
	if (!token)
		return (NULL);

	token->binary_value1 = p = DDS_OctetSeq__alloc ();
	if (!p) {
		DDS_DataHolder__free (token);
		return (NULL);
	}
	ret = dds_seq_require (p, credential_length);
	if (ret) {
		DDS_DataHolder__free (token);
		return (NULL);
	}
	memcpy (DDS_SEQ_DATA (*p), credential, credential_length);
	return (token);
}

/* aps_reply -- Validate a Handshake Request message and create a new 
		Handshake Reply message token if valid. */

static DDS_DataHolder *aps_reply (const SEC_AUTH            *ap,
				  DDS_DataHolder            *msg_in,
				  Identity_t                replier,
				  Identity_t                peer,
				  void                      **pdata,
				  DDS_IdentityToken         *rem_id_tok,
				  DDS_PermissionsToken      *rem_perm_tok,
				  DDS_IdentityCredential    **rem_id,
				  DDS_PermissionsCredential **rem_perm,
				  DDS_ReturnCode_t          *error)
{
	DDS_Property		*rem_id_p, *rem_perm_p;
	DDS_HandshakeToken	*token;
	StdData_t		*p;

	*rem_id = NULL;
	*rem_perm = NULL;
	p = xmalloc (sizeof (StdData_t));
	if (!p) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}

	/* Validate the handshake msg_in argument! */
	if (!msg_in ||
	    !msg_in->class_id ||
	    memcmp (msg_in->class_id,
		    ap->req_name,
		    strlen (ap->req_name) + 1) ||
	    !msg_in->string_properties ||
	    !msg_in->binary_value1 ||
	    DDS_SEQ_LENGTH (*msg_in->binary_value1) != NONCE_LENGTH ||
	    memcmp (DDS_SEQ_DATA (*msg_in->binary_value1), "CHALLENGE:", 10)) {
		xfree (p);
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	memcpy (p->data.nonces.nonce_a,
		DDS_SEQ_DATA (*msg_in->binary_value1),
		NONCE_LENGTH);

#ifdef DUMP_HS
	dump_chunk ("nonce_a", p->data.nonces.nonce_a, NONCE_LENGTH);
	log_printf (SEC_ID, 0, "\r\n");
#endif

	/* Get the peer identity and permission credentials and verify them
	   against their respective identity and permission tokens. */
	rem_id_p = DDS_DataHolder_get_property (msg_in,
				GMCLASSID_SECURITY_ID_CREDENTIAL_PROPERTY);
	rem_perm_p = DDS_DataHolder_get_property (msg_in,
				GMCLASSID_SECURITY_PERM_CREDENTIAL_PROPERTY);
	if (!rem_id_p ||
	    !rem_id_p->value ||
	    !aps_valid_id_credential (replier,
	    			      peer,
	    			      (unsigned char *) rem_id_p->value,
				      strlen (rem_id_p->value)) ||
	    !rem_perm_p ||
	    !rem_perm_p->value ||
	    !sec_valid_sha256 ((unsigned char *) rem_id_p->value,	/* Data */
	    		       strlen (rem_id_p->value),
			       DDS_SEQ_DATA (*rem_id_tok->binary_value1)) ||
	    !sec_valid_sha256 ((unsigned char *) rem_perm_p->value,
	    		       strlen (rem_perm_p->value),
			       DDS_SEQ_DATA (*rem_perm_tok->binary_value1))) {
		xfree (p);
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}

	/* Get the received Identity and Permission Credentials in Token format
	   so they can be reused later on in the handshake procedure. */
	*rem_id = aps_credential (GMCLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
				  (unsigned char *) rem_id_p->value,
				  strlen (rem_id_p->value));
	if (!*rem_id) {
		xfree (p);
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	*rem_perm = aps_credential (GMCLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN,
				    (unsigned char *) rem_perm_p->value,
				    strlen (rem_perm_p->value));
	if (!*rem_perm) {
		token = NULL;
		goto mem_error;
	}

	/* Create a proper Handshake Reply message token. */
	token = DDS_DataHolder__alloc (ap->reply_name);
	if (!token)
		goto mem_error;

	if (aps_add_identity_property (token, replier) ||
	    aps_add_permissions_property (token, replier))
		goto mem_error;

	/* Add NONCE to binary value 1. */
	token->binary_value1 = sec_generate_nonce ();
	if (!token->binary_value1)
		goto mem_error;

	memcpy (p->data.nonces.nonce_b,
		DDS_SEQ_DATA (*token->binary_value1),
		NONCE_LENGTH);

	/* Set binary value 2 to the signed SHA256 result of msg_in->binary_value1. */
	token->binary_value2 = sec_sign_sha256_data (replier,
						     DDS_SEQ_DATA (*msg_in->binary_value1),
						     DDS_SEQ_LENGTH (*msg_in->binary_value1));
	if (!token->binary_value2)
		goto mem_error;

	p->h = 0;
	*pdata = p;
	return (token);

    mem_error:
	xfree (p);
	*pdata = NULL;
	*error = DDS_RETCODE_OUT_OF_RESOURCES;
	if (*rem_id)
		DDS_DataHolder__free (*rem_id);
	if (*rem_perm)
		DDS_DataHolder__free (*rem_perm);
	if (token)
		DDS_DataHolder__free (token);
	return (NULL);
}

static int aps_generate_master_secrets (Secrets_t     *sp,
				        unsigned char *nonce_a,
					unsigned char *nonce_b,
				        unsigned char *key)
{
	unsigned char	data [272];
	int		ret;

	/* Get nonce data in data buffer. */
	memcpy (data, nonce_a, NONCE_LENGTH);
	memcpy (&data [NONCE_LENGTH], nonce_b, NONCE_LENGTH);

	/* Remember shared key. */
	memcpy (sp->secret, key, SHARED_SECRET_LENGTH);

	/* Calculate derived master encryption/decryption key. */
	memcpy (&data [NONCE_LENGTH * 2], "key exchange key", 16);
	ret = sec_hmac_sha256 (key,
			       SHARED_SECRET_LENGTH,
			       data,
			       sizeof (data),
			       sp->kx_key);
	if (ret)
		return (ret);

	/* Calculate derived master HMAC key. */
	memcpy (&data [NONCE_LENGTH * 2], "key exchange mac", 16);
	ret = sec_hmac_sha256 (key,
			       SHARED_SECRET_LENGTH,
			       data,
			       sizeof (data),
			       sp->kx_mac_key);
	return (ret);
}

/* aps_final -- Validate a Handshake Reply message and create a new Handshake
		Final message token if it was valid, as well as a shared
		secret of which the handle is in *secret.  If something went
		wrong, the function returns NULL and the error code is set. */

static DDS_DataHolder *aps_final (const SEC_AUTH            *ap,
				  DDS_HandshakeToken        *msg_in,
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
	DDS_DataHolder	*token;
	DDS_Property	*rem_id_p, *rem_perm_p;
	StdData_t	*p = *pdata;
	unsigned char	shared_secret [SHARED_SECRET_LENGTH];
	unsigned char	buf [NONCE_LENGTH + MAX_RSA_KEY_SIZE];

	/* Verify DataHolder contents. */
	if (!p ||
	    memcmp (msg_in->class_id,
		    ap->reply_name,
		    strlen (ap->reply_name) + 1) ||
	    DDS_SEQ_LENGTH (*msg_in->binary_value1) != NONCE_LENGTH ||
	    memcmp (DDS_SEQ_DATA (*msg_in->binary_value1), "CHALLENGE:", 10) ||
	    !msg_in->binary_value2 ||
	    DDS_SEQ_LENGTH (*msg_in->binary_value2) < MIN_RSA_KEY_SIZE)
		goto done;

	/* Get credentials from peer and validate them. */
	rem_id_p = DDS_DataHolder_get_property (msg_in, 
				GMCLASSID_SECURITY_ID_CREDENTIAL_PROPERTY);
	rem_perm_p = DDS_DataHolder_get_property (msg_in,
				GMCLASSID_SECURITY_PERM_CREDENTIAL_PROPERTY);
	if (!rem_id_p ||
	    !rem_id_p->value ||
	    !aps_valid_id_credential (req,
	    			      peer,
	    			      (unsigned char *) rem_id_p->value,
				      strlen (rem_id_p->value)) ||
	    !rem_perm_p ||
	    !rem_perm_p->value ||
	    !sec_valid_sha256 ((unsigned char *) rem_id_p->value,   /* IdToken. */
	    		       strlen (rem_id_p->value),
			       DDS_SEQ_DATA (*rem_id_tok->binary_value1)) ||
	    !sec_valid_sha256 ((unsigned char *) rem_perm_p->value, /* PermToken. */
	    		       strlen (rem_perm_p->value),
			       DDS_SEQ_DATA (*rem_perm_tok->binary_value1)))
		goto done;

	/* Validate encrypted Nonce. */
	if (!sec_signed_sha256_data (peer,			/* Peer identity. */
	    			     p->data.nonces.nonce_a,	/* Original Nonce. */
				     NONCE_LENGTH,
				     DDS_SEQ_DATA (*msg_in->binary_value2),
				     DDS_SEQ_LENGTH (*msg_in->binary_value2)))
		goto done;

	/* Get the received Identity and Permission Credentials in Token format
	   so they can be reused later on in the handshake procedure. */
	*rem_id = aps_credential (GMCLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
				  (unsigned char *) rem_id_p->value,
				  strlen (rem_id_p->value));
	if (!*rem_id) {
		xfree (p);
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	*rem_perm = aps_credential (GMCLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN,
				    (unsigned char *) rem_perm_p->value,
				    strlen (rem_perm_p->value));
	if (!*rem_perm) {
		token = NULL;
		goto mem_error;
	}

	/* Create final message token with shared_secret. */
	token = DDS_DataHolder__alloc (ap->final_name);
	if (!token)
		goto mem_error;

	/* Generate a shared secret and sign it with public key of peer. */
	if (sec_generate_random (shared_secret, sizeof (shared_secret)))
		goto mem_error;

	token->binary_value1 = sec_encrypt_secret (peer, shared_secret,
						   sizeof (shared_secret));
	if (!token->binary_value1)
		goto mem_error;

	memcpy (buf, DDS_SEQ_DATA (*msg_in->binary_value1), NONCE_LENGTH);
	memcpy (buf + NONCE_LENGTH,
		DDS_SEQ_DATA (*token->binary_value1),
		DDS_SEQ_LENGTH (*token->binary_value1));
	token->binary_value2 = sec_sign_sha256_data (req, buf, 
					NONCE_LENGTH +
					DDS_SEQ_LENGTH (*token->binary_value1));
	if (!token->binary_value2)
		goto mem_error;

#ifdef DUMP_HS
	dump_chunk ("nonce_a", p->data.nonces.nonce_a, NONCE_LENGTH);
#endif

	/* Finally generate the master secrets needed for encryption/HMAC. */
	if (aps_generate_master_secrets (&p->data.secrets,
					 p->data.nonces.nonce_a,
					 DDS_SEQ_DATA (*msg_in->binary_value1),
					 shared_secret))
		goto mem_error;

#ifdef DUMP_HS
	dump_chunk ("Shared secret", shared_secret, sizeof (shared_secret));
	dump_chunk ("Encrypted secret", DDS_SEQ_DATA (*token->binary_value1),
					DDS_SEQ_LENGTH (*token->binary_value1));
	dump_chunk ("buffer", buf, NONCE_LENGTH + DDS_SEQ_LENGTH (*token->binary_value1));
	dump_chunk ("buffer sign", DDS_SEQ_DATA (*token->binary_value2),
				   DDS_SEQ_LENGTH (*token->binary_value2));
	dump_chunk ("kx_key", p->data.secrets.kx_key, DERIVED_KEY_LENGTH);
	dump_chunk ("kx_mac_key", p->data.secrets.kx_mac_key, DERIVED_KEY_LENGTH);
	log_printf (SEC_ID, 0, "\r\n");
#endif
	p->h = ++handles;
	*secret = p->h;
	*pdata = NULL;
	p->next = secrets;
	secrets = p;
	return (token);

    mem_error:
	*error = DDS_RETCODE_OUT_OF_RESOURCES;
	if (*rem_id)
		DDS_DataHolder__free (*rem_id);
	if (*rem_perm)
		DDS_DataHolder__free (*rem_perm);
	if (token)
		DDS_DataHolder__free (token);

    done:
	*pdata = NULL;
	xfree (p);
	return (NULL);
}

/* aps_check_final -- Validate a Handshake Final message and return the
		      associated shared secret handle if successful. If not, it
		      returns NULL. */

static DDS_ReturnCode_t aps_check_final (const SEC_AUTH            *ap,
					 DDS_HandshakeToken        *final,
					 Identity_t                replier,
					 Identity_t                peer,
					 void                      **pdata,
					 DDS_IdentityToken         *rem_id_tok,
					 DDS_PermissionsToken      *rem_perm_tok,
					 DDS_IdentityCredential    **rem_id,
					 DDS_PermissionsCredential **rem_perm,
					 SharedSecret_t      *secret)
{
	StdData_t	*p = *pdata;
	unsigned char	shared_secret [SHARED_SECRET_LENGTH];
	unsigned char	buf [NONCE_LENGTH + MAX_RSA_KEY_SIZE];

	ARG_NOT_USED (rem_id_tok)
	ARG_NOT_USED (rem_perm_tok)
	ARG_NOT_USED (rem_id)
	ARG_NOT_USED (rem_perm)

	/* Verify DataHolder basic contents. */
	if (!p ||
	    memcmp (final->class_id,
		    ap->final_name,
		    strlen (ap->final_name) + 1) ||
	    !final->binary_value1 ||
	    DDS_SEQ_LENGTH (*final->binary_value1) < MIN_RSA_KEY_SIZE ||
	    !final->binary_value2 ||
	    DDS_SEQ_LENGTH (*final->binary_value2) < MIN_RSA_KEY_SIZE) {
		log_printf (SEC_ID, 0, "aps_check_final: basic contents validation failure!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	/* Validate binary_value2 contents. */
	memcpy (buf, p->data.nonces.nonce_b, NONCE_LENGTH);
	memcpy (buf + NONCE_LENGTH,
		DDS_SEQ_DATA (*final->binary_value1),
		DDS_SEQ_LENGTH (*final->binary_value1));

#ifdef DUMP_HS
	dump_chunk ("Encrypted secret", DDS_SEQ_DATA (*final->binary_value1),
					DDS_SEQ_LENGTH (*final->binary_value1));
	dump_chunk ("buffer", buf, NONCE_LENGTH + DDS_SEQ_LENGTH (*final->binary_value1));
	dump_chunk ("buffer sign", DDS_SEQ_DATA (*final->binary_value2),
				   DDS_SEQ_LENGTH (*final->binary_value2));
	log_printf (SEC_ID, 0, "\r\n");
#endif
	if (!sec_signed_sha256_data (peer,	/* Peer identity handle. */
				     buf,	/* Our Nonce + encrypted secret. */
				     NONCE_LENGTH +
				    	DDS_SEQ_LENGTH (*final->binary_value1),
				     DDS_SEQ_DATA (*final->binary_value2),
				     DDS_SEQ_LENGTH (*final->binary_value2))) {
		log_printf (SEC_ID, 0, "aps_check_final: {nonce,secret} signed SHA256 failure!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	/* Decrypt binary_value1 => Shared secret! */
	if (sec_decrypted_secret (replier,
				  DDS_SEQ_DATA (*final->binary_value1),
				  DDS_SEQ_LENGTH (*final->binary_value1),
				  shared_secret)) {
		log_printf (SEC_ID, 0, "aps_check_final: secret decryption failure!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	/* Finally generate the master secrets needed for encryption/HMAC. */
	if (aps_generate_master_secrets (&p->data.secrets,
					 p->data.nonces.nonce_a,
					 p->data.nonces.nonce_b,
					 shared_secret)) {
		log_printf (SEC_ID, 0, "aps_check_final: master secrets generation failure!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

#ifdef DUMP_HS
	dump_chunk ("Shared secret", shared_secret, sizeof (shared_secret));
	dump_chunk ("kx_key", p->data.secrets.kx_key, DERIVED_KEY_LENGTH);
	dump_chunk ("kx_mac_key", p->data.secrets.kx_mac_key, DERIVED_KEY_LENGTH);
	log_printf (SEC_ID, 0, "\r\n");
#endif
	p->h = ++handles;
	*secret = p->h;
	*pdata = NULL;
	p->next = secrets;
	secrets = p;
	return (DDS_RETCODE_OK);
}

/* aps_free_secret -- The shared secret is no longer needed and can be freed. */

static void aps_free_secret (const SEC_AUTH *ap, SharedSecret_t secret)
{
	StdData_t	*p, *prev;

	ARG_NOT_USED (ap)

	for (prev = NULL, p = secrets; p; prev = p, p = p->next)
		if (p->h == secret) {
			if (prev)
				prev->next = p->next;
			else
				secrets = p->next;
			xfree (p);
			return;
		}
}


static const SEC_AUTH sec_auth_dsa_dh;

/* PKI-RSA authentication plugin: */
static const SEC_AUTH sec_auth_pki_rsa = {
	SECC_DDS_SEC,

	GMCLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
	GMCLASSID_SECURITY_IDENTITY_TOKEN,
	32,

	aps_check_local,
	aps_validate_remote,
	aps_get_id_token,
	aps_release_id,

	&sec_auth_dsa_dh,

	GMCLASSID_SECURITY_HS_REQ_TOKEN_RSA,
	GMCLASSID_SECURITY_HS_REPLY_TOKEN_RSA,
	GMCLASSID_SECURITY_HS_FINAL_TOKEN_RSA,

	aps_request,
	aps_reply,
	aps_final,
	aps_check_final,

	aps_get_kx,
	aps_free_secret,
};

/* PKI-DH authentication plugin: */
static const SEC_AUTH sec_auth_dsa_dh = {
	SECC_DDS_SEC,

	GMCLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
	GMCLASSID_SECURITY_IDENTITY_TOKEN,
	32,

	aps_check_local,
	aps_validate_remote,
	aps_get_id_token,
	aps_release_id,

	&sec_auth_pki_rsa,

	GMCLASSID_SECURITY_HS_REQ_TOKEN_DH,
	GMCLASSID_SECURITY_HS_REPLY_TOKEN_DH,
	GMCLASSID_SECURITY_HS_FINAL_TOKEN_DH,

	aps_request,
	aps_reply,
	aps_final,
	aps_check_final,

	aps_get_kx,
	aps_free_secret,
};

/* sec_auth_add_pki_rsa =- Install the PKI-RSA authentication/encryption plugin. */

int sec_auth_add_pki_rsa (void)
{
	return (sec_auth_add (&sec_auth_pki_rsa, 0));
}


/* sec_auth_add_dsa_dh -- Install the DSA-DH authentication/encryption plugin. */

int sec_auth_add_dsa_dh (void)
{
	return (sec_auth_add (&sec_auth_dsa_dh, 0));
}

