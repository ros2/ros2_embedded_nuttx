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

/* sec_a_qeo.c -- Implements the qeo security plugin for
		  PKI-RSA authentication security methods. */

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif
#include "log.h"
#include "error.h"
#include "dds/dds_security.h"
#include "nsecplug/nsecplug.h"
#include "sec_util.h"
#include "sec_auth.h"
#include "sec_plugin.h"
#include "sec_a_qeo.h"
#include "sec_qeo_policy.h"

typedef struct sec_qeo_data_st QeoData_t;

typedef struct nonces_qeo_st {
	unsigned char	nonce_a [NONCE_LENGTH];
	unsigned char	nonce_b [NONCE_LENGTH];
} NoncesQ_t;

typedef struct secrets_qeo_st {
	unsigned char	secret [SHARED_SECRET_LENGTH];
	unsigned char	kx_key [DERIVED_KEY_LENGTH];
	unsigned char	kx_mac_key [DERIVED_KEY_LENGTH];
} SecretsQ_t;

struct sec_qeo_data_st {
	SharedSecret_t	h;
	union {
	  NoncesQ_t	nonces;
	  SecretsQ_t	secrets;
	}		data;
	QeoData_t	*next;
};

static SharedSecret_t	handles;

static QeoData_t *secrets;

static DDS_ReturnCode_t qeo_get_kx (const SEC_AUTH *plugin,
				    SharedSecret_t secret,
				    unsigned char  *kx_key,
				    unsigned char  *kx_mac_key)
{
	QeoData_t	*p;

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
	char		buf [28];

	log_printf (SEC_ID, 0, "SEC: Unsecure %s => ", guid_prefix_str ((GuidPrefix_t *) key, buf));

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

	log_printf (SEC_ID, 0, "secure %s\r\n", guid_prefix_str ((GuidPrefix_t *) key, buf));
}

/* aps_check_local -- Check if plugin can handle a local participant for
		      negotiation.  If it is possible, the participant key
		      may be altered by the plugin code. */

static DDS_ReturnCode_t qeo_check_local (const SEC_AUTH *ap,
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

static AuthState_t qeo_validate_remote (const SEC_AUTH       *ap,
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
	ARG_NOT_USED (replier)

	if (strcmp (rem_perm_tok->class_id, QEO_CLASSID_SECURITY_PERMISSIONS_TOKEN) ||
	    !rem_id_tok->binary_value1 ||
	    DDS_SEQ_LENGTH (*rem_id_tok->binary_value1) != 32 )
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

	/* policy versions are the same so use std method */
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

static Token_t *qeo_get_id_token (const SEC_AUTH *ap, Identity_t id)
{
	char			*p;
	DDS_IdentityToken	*token;
	Token_t			*tp;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (ap)

	tp = xmalloc (sizeof (Token_t));
	if (!tp)
		return (NULL);

	token = DDS_DataHolder__alloc (QEO_CLASSID_SECURITY_IDENTITY_TOKEN);
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

static void qeo_release_id (const SEC_AUTH *ap, Identity_t id)
{
	DDS_SecurityReqData	data;

	ARG_NOT_USED (ap)

	if (!plugin_policy) 
		return;

	data.handle = id;
	sec_authentication_request (DDS_RELEASE_ID, &data);
}

static DDS_ReturnCode_t aps_valid_id_credential (Identity_t    local,
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
	return (ret);
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

static DDS_OctetSeq *char_to_seq (unsigned char *data,
				  size_t        length)
{
	DDS_OctetSeq *seq;
	DDS_ReturnCode_t ret;

	seq = DDS_OctetSeq__alloc ();

	ret = dds_seq_require (seq, length);
	if (ret) {
		DDS_OctetSeq__free (seq);
		return (NULL);
	}

	memcpy (DDS_SEQ_DATA (*seq), data, length);
	return (seq);
}

/* aps_get_permissions_enc_data -- Get encrypted permissions credentials. */
/* Encrypt the policy file with a random secret, 
   encrypt the random secret with the public key,
   add the encrypted random secret in front of the encrypted policy file */

static DDS_OctetSeq *aps_get_permissions_enc_data (Identity_t       id,
						   DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData	data;
	DDS_OctetSeq            *seq;
	unsigned char           *policy_file;
	size_t                  policy_length, res_len = 0;
	uint32_t                rsa_enc_len, *p;
	unsigned char           *aes_enc, rsa_enc [MAX_RSA_KEY_SIZE];
	unsigned char           *result;
	unsigned char           sec [32];
	unsigned char           salt [16];
	uint32_t                counter = 0;

	memset (salt, 0, 16);

	/* generate random secret */
	if ((*error = sec_generate_random (&sec [0], 32)))
		return (NULL);

	/* use it to encrypt policy file */
	policy_file = get_policy_file (&policy_length, error);
	if (*error)
		return (NULL);

	if (!(aes_enc = xmalloc (policy_length))) {
		xfree (policy_file);
 		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
		
	if ((*error = sec_aes256_ctr_crypt (sec, 32, salt, 
					    &counter, policy_file, 
					    aes_enc, policy_length))) {
		xfree(policy_file);
		xfree (aes_enc);
		return (NULL);
	}

	/* encrypt secret with public key */
	data.handle = id;
	data.data = sec;
	data.length = 32;
	data.rdata = &rsa_enc [0];
	data.rlength = sizeof (rsa_enc);
	*error = sec_certificate_request (DDS_ENCRYPT_PUBLIC, &data);
	if (*error) {
        xfree(policy_file);
		xfree (aes_enc);
		log_printf (SEC_ID, 0, "get_permissions_enc_data: Could not rsa encrypt the secret\r\n");
		return (NULL);
	}

	/* Combine both */
	rsa_enc_len = data.rlength;
	res_len = 4 + policy_length + rsa_enc_len;
	if (!(result = xmalloc (res_len))) {
		xfree(policy_file);
		xfree (aes_enc);
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	p = (uint32_t *) &result [0];

	/* first copy the length of the encrypted secret */
	*p = ntohl (rsa_enc_len);
	/* Then copy the encrypted secret */
	memcpy (&result [4], &rsa_enc [0], rsa_enc_len);
	/* next copy the encrypted policy file */
	memcpy (&result [4 + rsa_enc_len], aes_enc, policy_length);

	seq = char_to_seq (result, res_len);
	xfree (result);
	return (seq);
}

/* aps_add_permissions_enc_property -- Add an encrypted Permissions property to a handshake token. */

static DDS_ReturnCode_t aps_add_permissions_enc_property (DDS_DataHolder *p,
							  Identity_t     id)
{
	DDS_BinaryProperty	*pp;
	DDS_ReturnCode_t	ret;
	DDS_OctetSeq            *perm;

	perm = aps_get_permissions_enc_data (id, &ret);
	if (!perm)
		return (ret);

	pp = DDS_DataHolder_add_binary_property (p, 
					  QEO_CLASSID_SECURITY_PERM_CREDENTIAL_ENC_PROPERTY);
	
	pp->value = *perm;

	if (!pp) {
		DDS_OctetSeq__free (perm);
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
	}
	else 
		ret = DDS_RETCODE_OK;

	return (ret);
}

/* aps_get_permissions_sig_data -- Get signed permissions credentials. */

static DDS_OctetSeq *aps_get_permissions_sig_data (Identity_t       id,
					   DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData	data;
	DDS_OctetSeq            *seq;
	unsigned char           *policy_file;
	size_t                  policy_length;
	unsigned char           *sign;

	if (!(sign = xmalloc (MAX_RSA_KEY_SIZE))) {
 		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}	
	
	policy_file = get_policy_file (&policy_length, error);
	if (*error)
		return (NULL);

	data.handle = id;
	data.data = policy_file;
	data.length = policy_length;
	data.rdata = sign;
	data.rlength = MAX_RSA_KEY_SIZE;
	*error = sec_certificate_request (DDS_SIGN_SHA256, &data);
	if (*error) {
		*error = DDS_RETCODE_NOT_ALLOWED_BY_SEC;
		xfree (sign);
		xfree (policy_file);
		log_printf (SEC_ID, 0, "get_permissions_sig_data: Could not sign the policy file\r\n");
		return (NULL);
	}
	
	seq = char_to_seq (sign, data.rlength);

	xfree (sign);
	xfree (policy_file);
	*error = DDS_RETCODE_OK;

	return (seq);
}

/* aps_add_permissions_sig_property -- Add a signed Permissions property to a handshake token. */

static DDS_ReturnCode_t aps_add_permissions_sig_property (DDS_DataHolder *p,
							  Identity_t     id)
{
	DDS_BinaryProperty	*pp;
	DDS_ReturnCode_t	ret;
	DDS_OctetSeq            *perm;

	perm = aps_get_permissions_sig_data (id, &ret);
	if (!perm)
		return (ret);

	pp = DDS_DataHolder_add_binary_property (p, 
					  QEO_CLASSID_SECURITY_PERM_CREDENTIAL_SIG_PROPERTY);

	pp->value = *perm;

	if (!pp) {
		DDS_OctetSeq__free (perm);
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
	}
	else
		ret = DDS_RETCODE_OK;

	return (ret);
}

static unsigned char *aps_decrypt_perm_cred (DDS_BinaryProperty *p,
					     Identity_t     req,
					     unsigned char  **buffer,
					     size_t         *len)
{
	DDS_SecurityReqData data;
	DDS_ReturnCode_t    ret;
	DDS_OctetSeq        *seq;
	uint32_t            *q, rsa_length, counter = 0;
	unsigned char       *rsa_dec = NULL, *aes_dec = NULL, *input = NULL;
	unsigned char       secret [32], salt [16];

	/* Get the rsa enc length */
	seq = &p->value;
	input = DDS_SEQ_DATA (*seq);
	q = (uint32_t *) &input [0];
	rsa_length = ntohl (*q);

	if (!(rsa_dec = xmalloc (rsa_length)))
		return (NULL);

	/* Decrypt secret with private key */
	memcpy (rsa_dec, &input [4], rsa_length);

	data.handle = req;
	data.data = rsa_dec;
	data.length = rsa_length;
	data.rdata = &secret [0];
	ret = sec_certificate_request (DDS_DECRYPT_PRIVATE, &data);
	if (ret || data.rlength != 32) {
		xfree (rsa_dec);
		return (NULL);
	}
	xfree (rsa_dec);

	/* decrypt policy file with secret */
	
	memset (&salt [0], 0, 16);

	*len = 	DDS_SEQ_LENGTH (*seq) - 4 - rsa_length;

	if (!(aes_dec = (unsigned char *) xmalloc (*len)))
		return (NULL);

	*buffer = (unsigned char *) xmalloc (*len);
	if (!*buffer) {
		xfree (aes_dec);
		return (NULL);
	}

	memcpy (aes_dec, &input [4 + rsa_length], *len);

	if ((ret = sec_aes256_ctr_crypt (&secret [0], 32, salt, &counter, aes_dec, *buffer, *len))) {
		xfree (aes_dec);
		xfree (*buffer);
		return (NULL);
	}
	return (*buffer);
}

static int aps_verify_perm_cred_sign (DDS_BinaryProperty *p,
				      Identity_t    rep,
				      unsigned char *policy_file,
				      size_t        len)
{
	return (sec_signed_sha256_data (rep, policy_file, len, DDS_SEQ_DATA (p->value), DDS_SEQ_LENGTH (p->value)));
}

/* Add you own local version number */
static DDS_LongLongSeq *qeo_add_seq_number (void)
{
	DDS_LongLongSeq *seq;
	DDS_ReturnCode_t error;
	uint64_t         pol_version;

	seq = DDS_LongLongSeq__alloc ();

	if (!seq)
		return (NULL);

	pol_version = get_policy_version (&error);
	if (error)
		return (NULL);

	dds_seq_append ((void *) seq, &pol_version);
	return (seq);
}

/* check the local and the remote policy version number 
   return > 0 if local is larger 
   return = 0 if remote is larger 
   return < 0 on error or equal policy file*/

static int qeo_validate_seq_number (DDS_DataHolder *remote)
{
	unsigned long long local_pol_version;
	unsigned long long *remote_pol_version;
	DDS_ReturnCode_t   error = DDS_RETCODE_OK;
	
	if (!remote)
		return (-1);

	remote_pol_version = (unsigned long long *) DDS_SEQ_DATA (*remote->longlongs_value);
	local_pol_version = get_policy_version (&error);

	if (error)
		return (-1);

	log_printf (SEC_ID, 0, "SEC_A_QEO: validate version number: local [%llu] remote [%llu]\r\n", 
		    local_pol_version, *remote_pol_version);

	if (local_pol_version < *remote_pol_version)
		return (0);
	else if (local_pol_version > *remote_pol_version)
		return (1);

	return (-1);
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

static DDS_HandshakeToken *qeo_request (const SEC_AUTH *ap,
				        Identity_t     req,
				        void           **pdata)
{
	DDS_HandshakeToken	*token;
	QeoData_t		*p;

	ARG_NOT_USED (req)

	p = xmalloc (sizeof (QeoData_t));
	if (!p)
		return (NULL);

	/* Create a proper Handshake Request message token. */
	token = DDS_DataHolder__alloc (ap->req_name);
	if (!token) {
		xfree (p);
		return (NULL);
	}
	if (aps_add_identity_property (token, req))
		goto mem_error;

	token->binary_value1 = sec_generate_nonce ();
	if (!token->binary_value1)
		goto mem_error;

	/* Add the policy sequence number you currently have */
	token->longlongs_value = qeo_add_seq_number ();
	if (!token->longlongs_value)
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

static DDS_DataHolder *qeo_reply (const SEC_AUTH            *ap,
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
	DDS_Property		*rem_id_p;
	DDS_HandshakeToken	*token;
	QeoData_t		*p;

	ARG_NOT_USED (rem_perm_tok)

	*rem_id = NULL;
	*rem_perm = NULL;
	*error = DDS_RETCODE_OK;
	p = xmalloc (sizeof (QeoData_t));
	if (!p) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}

	/* Verify that the type of handshake message is the same as the one we expect 
	   also check that the msg_in has the same policy */
	if (!msg_in ||
	    !msg_in->class_id ||
	    memcmp (msg_in->class_id,
		    ap->req_name,
		    strlen (ap->req_name) +1)) {
		xfree (p);
		log_printf (SEC_ID, 0, "QEO_REPLY: BAD_MESSAGE\r\n");
		*error = DDS_RETCODE_PRECONDITION_NOT_MET;
		return (NULL);
	}

	/* Validate the handshake msg_in argument! */
	if (!msg_in->string_properties ||
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

	/* Get the peer identity credentials and verify them
	   against their respective identity and permission tokens. */
	rem_id_p = DDS_DataHolder_get_property (msg_in,
				GMCLASSID_SECURITY_ID_CREDENTIAL_PROPERTY);
	if (!rem_id_p ||
	    !rem_id_p->value ||
	    /* Verify certificate */ 
	    (*error = aps_valid_id_credential (replier,
					       peer,
					       (unsigned char *) rem_id_p->value,
					       strlen (rem_id_p->value)))) {
		xfree (p);
		return (NULL);
	}
	/* check the remote id token with the sha256 of the certificate */
	if (!sec_valid_sha256 ((unsigned char *) rem_id_p->value,	
	    		       strlen (rem_id_p->value),
			       DDS_SEQ_DATA (*rem_id_tok->binary_value1))) {
		xfree (p);
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}

	/* Get the received Identity Credentials in Token format
	   so they can be reused later on in the handshake procedure.
	   the permissions credential is NULL */
	*rem_id = aps_credential (QEO_CLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
				  (unsigned char *) rem_id_p->value,
				  strlen (rem_id_p->value));
	if (!*rem_id) {
		xfree (p);
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}

	/* Create a proper Handshake Reply message token. */
	token = DDS_DataHolder__alloc (ap->reply_name);
	if (!token)
		goto mem_error;

	/* Add certificate */
	if (aps_add_identity_property (token, replier))
		goto mem_error;

	/* if the local policy file is larger, send the local policy file */
	if (qeo_validate_seq_number (msg_in) > 0) {
		log_printf (SEC_ID, 0, "SEC_A_QEO: qeo_reply: send policy file\r\n");
		if (aps_add_permissions_enc_property (token, peer) ||
		    aps_add_permissions_sig_property (token, replier))			
			goto mem_error;
	}

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

	/* Add the current policy sequence number */
	token->longlongs_value = qeo_add_seq_number ();
	if (!token->longlongs_value)
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

static int qeo_generate_master_secrets (SecretsQ_t    *sp,
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
		wrong, the function returns NULL and the error code is set. 
		QEO: check the signed and encrypted policy file and fill in 
		the access control database if valid */

static DDS_DataHolder *qeo_final (const SEC_AUTH            *ap,
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
	DDS_DataHolder	*token = NULL;
	DDS_Property	*rem_id_p;
	DDS_BinaryProperty *rem_perm_p_enc, *rem_perm_p_sig;
	QeoData_t	*p = *pdata;
	unsigned char	shared_secret [SHARED_SECRET_LENGTH];
	unsigned char	buf [NONCE_LENGTH + MAX_RSA_KEY_SIZE];
	unsigned char   *policy_file = NULL;
	size_t          len;

	ARG_NOT_USED (rem_perm_tok)

	*error = DDS_RETCODE_OK;

	/* Verify that msg_in is a reply message */
	if (!msg_in ||
	    !msg_in->class_id ||
	    memcmp (msg_in->class_id,
		    ap->reply_name,
		    strlen (ap->reply_name) +1)) {
		log_printf (SEC_ID, 0, "QEO_FINAL: BAD_MESSAGE\r\n");
		*error = DDS_RETCODE_PRECONDITION_NOT_MET;
		return (NULL);
	}	

	/* Verify DataHolder contents. */
	if (!p ||
	    !msg_in->binary_value1 ||
	    DDS_SEQ_LENGTH (*msg_in->binary_value1) != NONCE_LENGTH ||
	    memcmp (DDS_SEQ_DATA (*msg_in->binary_value1), "CHALLENGE:", 10) ||
	    !msg_in->binary_value2 ||
	    DDS_SEQ_LENGTH (*msg_in->binary_value2) < MIN_RSA_KEY_SIZE)
		goto done;

	/* Get credentials from peer and validate them. */
	rem_id_p = DDS_DataHolder_get_property (msg_in, 
				GMCLASSID_SECURITY_ID_CREDENTIAL_PROPERTY);
	if (!rem_id_p ||
	    !rem_id_p->value ||
	    (*error = aps_valid_id_credential (req,
					       peer,
					       (unsigned char *) rem_id_p->value,
					       strlen (rem_id_p->value)))) {
		log_printf (SEC_ID, 0, "qeo_final: not a valid id credential\r\n");
		goto done;
	}
	if (!sec_valid_sha256 ((unsigned char *) rem_id_p->value,   /* IdToken. */
	    		       strlen (rem_id_p->value),
			       DDS_SEQ_DATA (*rem_id_tok->binary_value1))) {
		log_printf (SEC_ID, 0, "qeo_final: not a valid id token\r\n");
		*error = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}

	/* Validate encrypted Nonce. */
	if (!sec_signed_sha256_data (peer,			/* Peer identity. */
	    			     p->data.nonces.nonce_a,	/* Original Nonce. */
				     NONCE_LENGTH,
				     DDS_SEQ_DATA (*msg_in->binary_value2),
				     DDS_SEQ_LENGTH (*msg_in->binary_value2))) {
		log_printf (SEC_ID, 0, "qeo_final: Invalid nonce encryption\r\n");
		*error = DDS_RETCODE_PRECONDITION_NOT_MET;
		return (NULL);
	}

	/* Get the received Identity and Permission Credentials in Token format
	   so they can be reused later on in the handshake procedure. */
	*rem_id = aps_credential (QEO_CLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
				  (unsigned char *) rem_id_p->value,
				  strlen (rem_id_p->value));
	if (!*rem_id) {
		xfree (p);
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	*rem_perm = aps_credential (QEO_CLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN,
				    NULL, 0);
	if (!*rem_perm) 
		goto mem_error;

	/* Get the permissions credential token that contains the policy file */

	/* if local policy file is smaller than the remote, 
	   we know we have received remote policy file */
	if (qeo_validate_seq_number (msg_in) == 0) {
		log_printf (SEC_ID, 0, "SEC_A_QEO: qeo_final: read policy file\r\n");
		/* 2 Qeo specific properties 
		   The first one is the encrypted policy file
		   The second one is the sign of the policy file */
		rem_perm_p_enc = DDS_DataHolder_get_binary_property (msg_in,
								     QEO_CLASSID_SECURITY_PERM_CREDENTIAL_ENC_PROPERTY);
		rem_perm_p_sig = DDS_DataHolder_get_binary_property (msg_in, 
								     QEO_CLASSID_SECURITY_PERM_CREDENTIAL_SIG_PROPERTY);
		/* Decrypt and verify the sign */
		if (!aps_decrypt_perm_cred (rem_perm_p_enc, req, &policy_file, &len) ||
		    !aps_verify_perm_cred_sign (rem_perm_p_sig, peer, policy_file, len)) {
			log_printf (SEC_ID, 0, "qeo_final: policy file decryption or sign failed\r\n");
			if (policy_file)
				xfree (policy_file);
			goto done;
		}
#if 0
		/* Validate the sign of the policy file, with the one in the perm tok */
		if (!sec_valid_sha256 ((unsigned char *) policy_file,
				       len,
				       DDS_SEQ_DATA (*rem_perm_tok->binary_value1))) {
			log_printf (SEC_ID, 0, "qeo_final: the sign of the policy file does not match the one in the participant message\r\n");
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
#endif

		/* Store the policy file after a timer */
		set_policy_file (policy_file, len);
	}

	/* Create final message token with shared_secret. */
	token = DDS_DataHolder__alloc (ap->final_name);
	if (!token)
		goto mem_error;

	/* if the local policy file is larger, send the local policy file */
	if (qeo_validate_seq_number (msg_in) > 0) {
		log_printf (SEC_ID, 0, "SEC_A_QEO: qeo_final: send policy file\r\n");
		if (aps_add_permissions_enc_property (token, peer) ||
		    aps_add_permissions_sig_property (token, req))
			goto mem_error;
	}

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

	/* Add the current policy sequence number */
	token->longlongs_value = qeo_add_seq_number ();
	if (!token->longlongs_value)
		goto mem_error;

#ifdef DUMP_HS
	dump_chunk ("nonce_a", p->data.nonces.nonce_a, NONCE_LENGTH);
#endif

	/* Finally generate the master secrets needed for encryption/HMAC. */
	if (qeo_generate_master_secrets (&p->data.secrets,
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
	if (p)
		xfree (p);
	return (NULL);
}

/* aps_check_final -- Validate a Handshake Final message and return the
		      associated shared secret handle if successful. If not, it
		      returns NULL. */

static DDS_ReturnCode_t qeo_check_final (const SEC_AUTH            *ap,
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
	QeoData_t	*p = *pdata;
	unsigned char	shared_secret [SHARED_SECRET_LENGTH];
	unsigned char	buf [NONCE_LENGTH + MAX_RSA_KEY_SIZE];
	DDS_BinaryProperty *rem_perm_p_enc, *rem_perm_p_sig;
	unsigned char      *policy_file;
	size_t             len;

	ARG_NOT_USED (rem_id);
	ARG_NOT_USED (rem_id_tok);
	ARG_NOT_USED (rem_perm_tok);

	/* Verify that final is indeed a final handshake */
	if (!final ||
	    !final->class_id ||
	    memcmp (final->class_id,
		    ap->final_name,
		    strlen (ap->final_name) +1)) {
		log_printf (SEC_ID, 0, "QEO_CHECK_FINAL: BAD_MESSAGE\r\n");
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}

	/* Verify DataHolder basic contents. */
	if (!p ||
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

	/* if the remote policy file is larger,
	   we know we received a policy file */
	if (qeo_validate_seq_number (final) == 0) {
		log_printf (SEC_ID, 0, "SEC_A_QEO: qeo_reply: read policy file\r\n");
		/* 2 Qeo specific properties 
		   The first one is the encrypted policy file
		   The second one is the sign of the policy file */
		rem_perm_p_enc = DDS_DataHolder_get_binary_property (final,
								     QEO_CLASSID_SECURITY_PERM_CREDENTIAL_ENC_PROPERTY);
		rem_perm_p_sig = DDS_DataHolder_get_binary_property (final, 
								     QEO_CLASSID_SECURITY_PERM_CREDENTIAL_SIG_PROPERTY);
		
		if (!aps_decrypt_perm_cred (rem_perm_p_enc, replier, &policy_file, &len) ||
		    !aps_verify_perm_cred_sign (rem_perm_p_sig, peer, policy_file, len)) {
			log_printf (SEC_ID, 0, "qeo_check_final: policy file decryption or sign failed\r\n");
			if (policy_file)
				xfree (policy_file);
			return (DDS_RETCODE_BAD_PARAMETER);
		}

		/* This does not work when the policy file was updated, 
		   but the hash in the participant message was not yet updated. 
		   The participant receives a new policy file in the reply message and returns
		   this new policy file, without changing the participant message. This might be exploited,
		   when a fake participant sends another policy file than it received */
#if 0
		/* Validate the sign of the policy file, with the one in the perm tok */
		if (!sec_valid_sha256 ((unsigned char *) policy_file,
				       len,
				       DDS_SEQ_DATA (*rem_perm_tok->binary_value1))) {
			log_printf (SEC_ID, 0, "qeo_check_final: the sign of the policy file does not match the one in the participant message\r\n");
			return (DDS_RETCODE_BAD_PARAMETER);
		}
#endif

		/* Store the policy file after a timer */
		set_policy_file (policy_file, len);
	}

	/* Check the nonce */
	if (!sec_signed_sha256_data (peer,	/* Peer identity handle. */
				     buf,	/* Our Nonce + encrypted secret. */
				     NONCE_LENGTH +
				    	DDS_SEQ_LENGTH (*final->binary_value1),
				     DDS_SEQ_DATA (*final->binary_value2),
				     DDS_SEQ_LENGTH (*final->binary_value2))) {
		log_printf (SEC_ID, 0, "qeo_check_final: {nonce,secret} signed SHA256 failure!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	*rem_perm = aps_credential (QEO_CLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN,
				    NULL, 0);
	if (!*rem_perm) 
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	/* Decrypt binary_value1 => Shared secret! */
	if (sec_decrypted_secret (replier,
				  DDS_SEQ_DATA (*final->binary_value1),
				  DDS_SEQ_LENGTH (*final->binary_value1),
				  shared_secret)) {
		log_printf (SEC_ID, 0, "qeo_check_final: secret decryption failure!\r\n");
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	/* Finally generate the master secrets needed for encryption/HMAC. */
	if (qeo_generate_master_secrets (&p->data.secrets,
					 p->data.nonces.nonce_a,
					 p->data.nonces.nonce_b,
					 shared_secret)) {
		log_printf (SEC_ID, 0, "qeo_check_final: master secrets generation failure!\r\n");
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

static void qeo_free_secret (const SEC_AUTH *ap, SharedSecret_t secret)
{
	QeoData_t	*p, *prev;

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


/* Qeo PKI-RSA authentication plugin: */
static const SEC_AUTH sec_auth_qeo = {
	SECC_DDS_SEC,

	QEO_CLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN,
	QEO_CLASSID_SECURITY_IDENTITY_TOKEN,
	32,

	qeo_check_local,
	qeo_validate_remote,
	qeo_get_id_token,
	qeo_release_id,

	NULL,

	QEO_CLASSID_SECURITY_HS_REQ_TOKEN_RSA,
	QEO_CLASSID_SECURITY_HS_REPLY_TOKEN_RSA,
	QEO_CLASSID_SECURITY_HS_FINAL_TOKEN_RSA,

	qeo_request,
	qeo_reply,
	qeo_final,
	qeo_check_final,

	qeo_get_kx,
	qeo_free_secret,
};

/* sec_auth_add_qeo -- Install the Qeo authentication/encryption plugin. */

int sec_auth_add_qeo (void)
{
	return (sec_auth_add (&sec_auth_qeo, 1));
}

