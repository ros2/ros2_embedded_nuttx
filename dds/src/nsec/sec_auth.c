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

/* sec_auth.c -- Implements the function that are defined 
                 for the authentication plugin. */

#include  <stdio.h>
#include "log.h"
#include "error.h"
#include "list.h"
#include "strseq.h"
#include "dds/dds_security.h"
#include "security.h"
#include "sec_data.h"
#include "sec_id.h"
#include "sec_log.h"
#include "sec_access.h"
#include "sec_plugin.h"
#include "sec_auth.h"
#include "sec_a_std.h"
#include "sec_a_dtls.h"
#include "sec_a_qeo.h"

#define	MAX_AUTH_PLUGINS 4

static unsigned		nauth_plugins;
static const SEC_AUTH	*auth_plugins [MAX_AUTH_PLUGINS];

typedef struct handshake_st HandshakeData_t;
struct handshake_st {
	HandshakeData_t	*next;
	HandshakeData_t	*prev;
	Handshake_t	handle;
	AuthState_t	state;
	IdentityData_t	*initiator;
	IdentityData_t	*replier;
	IdentityData_t	*peer;
	void		*pdata;
	const SEC_AUTH	*plugin;		/* Security plugin. */
	SharedSecret_t	secret;
};

typedef struct {
	HandshakeData_t	*head;
	HandshakeData_t	*tail;
} HandshakeList_t;

static HandshakeList_t	handshake_list;
static Handshake_t	handshake_handle;
static SharedSecret_t	secret_handle;

const char *as_str [] = {
	"OK",
	"FAILED",
	"PENDING_RETRY",
	"PENDING_HANDSHAKE_REQ",
	"PENDING_HANDSHAKE_MSG",
	"OK_FINAL_MSG",
	"PENDING_CHALLENGE_MSG"
};

static HandshakeData_t *get_handshake (Handshake_t handle)
{
	HandshakeData_t	*p;

	LIST_FOREACH (handshake_list, p)
		if (p->handle == handle)
			return (p);

	return (NULL);
}

static HandshakeData_t *get_handshake_from_secret (SharedSecret_t secret)
{
	HandshakeData_t	*p;

     	LIST_FOREACH (handshake_list, p)
		if (p->secret == secret)
			return (p);

	return (NULL);
}

static HandshakeData_t *add_handshake (IdentityData_t *initiator,
				   IdentityData_t *replier)
{
	HandshakeData_t	*p;

	p = xmalloc (sizeof (HandshakeData_t));
	if (!p)
		return (NULL);

	memset (p, 0, sizeof (HandshakeData_t));
	p->handle = ++handshake_handle;
	p->state = AS_FAILED;
	p->initiator = id_ref (initiator);
	p->replier = id_ref (replier);

	LIST_ADD_HEAD (handshake_list, *p);

	return (p);
}

static void remove_handshake (Handshake_t handle)
{
	HandshakeData_t		*p;

	p = get_handshake (handle);
	if (!p)
		return;

	id_release (p->initiator->id);
	id_release (p->replier->id);

	if (p->pdata)
		xfree (p->pdata);

	LIST_REMOVE (handshake_list, *p);
	xfree (p);
}

int sec_auth_add (const SEC_AUTH *ap, int req_default)
{
	if (nauth_plugins >= MAX_AUTH_PLUGINS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	auth_plugins [nauth_plugins] = ap;
	if (req_default) {
		if (nauth_plugins)
			auth_plugins [nauth_plugins] = auth_plugins [0];
		auth_plugins [0] = ap;
	}
	nauth_plugins++;
	return (DDS_RETCODE_OK);
}

static sec_auth_revoke_listener_fct on_revoke_identity = NULL;

int sec_auth_init (void)
{
	LIST_INIT (handshake_list);
	handshake_handle = 0;
	secret_handle = 0;
	nauth_plugins = 0;
#ifdef QEO_SECURITY
	sec_auth_add_qeo ();
#endif
	sec_auth_add_pki_rsa ();
	sec_auth_add_dsa_dh ();
#ifdef DTLS_SECURITY
	sec_auth_add_dtls ();
#endif
	return (DDS_RETCODE_OK);
}

#define	ADJUST_PTR(p, px)	px = (void *) ((char *)(px) - (char *)(p))

/* sec_validate_local_id -- Validate local Identity Credentials. */

AuthState_t sec_validate_local_id (const char       *name,
				   unsigned char    key [16],
				   DDS_Credentials  *credentials,
				   size_t           xlength,
				   Identity_t       *local_id,
				   DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData	data;
	const SEC_AUTH		*ap;
	IdentityData_t		*p;
	unsigned		i, nplugins;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("validate_local_id");
	if (!plugin_policy) {
		*error = DDS_RETCODE_OK;
		sec_log_ret ("%s", "OK");
		return (AS_OK);
	}
	data.name = name;
	data.data = (void *) credentials;
	data.length = xlength;
	data.kdata = key;
	data.klength = 16;
	if (plugin_policy == DDS_SECURITY_AGENT) {

		/* Make pointers so they're relative! */
		if (credentials->credentialKind == DDS_FILE_BASED) {
			ADJUST_PTR (credentials, credentials->info.filenames.private_key_file);
			ADJUST_PTR (credentials, credentials->info.filenames.certificate_chain_file);
		}
		else if (credentials->credentialKind == DDS_ENGINE_BASED) {
			ADJUST_PTR (credentials, credentials->info.engine.engine_id);
			ADJUST_PTR (credentials, credentials->info.engine.cert_id);
			ADJUST_PTR (credentials, credentials->info.engine.priv_key_id);
		}
		else if (credentials->credentialKind == DDS_SSL_BASED)	{
			ADJUST_PTR (credentials, credentials->info.sslData.private_key);
			ADJUST_PTR (credentials, credentials->info.sslData.certificate_list);
		}
		else {
			ADJUST_PTR (credentials, credentials->info.data.private_key.data);
			for (i = 0; i < credentials->info.data.num_certificates; i++)
				ADJUST_PTR (credentials, credentials->info.data.certificates [i].data);
		}
	}
	log_printf (SEC_ID, 0, "Security: call security plugin\r\n");
	ret = sec_authentication_request (DDS_VALIDATE_LOCAL_ID, &data);
	*error = ret;
	if (ret) {
		*error = ret;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	*local_id = data.handle;

	/* Identity credentials have been stored properly.
	   Register all authentication plugins that can handle it. */
	p = id_create (data.handle, error);
	if (!p) {
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	for (i = 0, nplugins = 0; i < nauth_plugins; i++) {
		ap = auth_plugins [i];
		if (ap->check_local) {
			ret = (*ap->check_local) (ap, data.handle, key);
			if (!ret) {
				id_add_plugin (p, ap);
				nplugins++;
			}
		}
	}
	if (!nplugins) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	sec_log_ret ("%s", "OK");
 	return (AS_OK);
}

/* sec_get_identity_tokens -- Return the Identity Tokens that will be used in
			      Participant discovery. */

Token_t *sec_get_identity_tokens (Identity_t       id,
				  unsigned         caps,
				  DDS_ReturnCode_t *error)
{
	Token_t			*token, *list = NULL;
	const SEC_AUTH		*ap, *prev_ap = NULL;
	IdentityData_t		*p;
	unsigned		i;

	sec_log_fct ("get_identity_tokens");
	p = id_lookup (id, NULL);
	if (!p)
		return (NULL);

	for (i = 0; i < nauth_plugins; i++) {
		if ((ap = p->plugins [i]) == NULL)
			break;

		if ((caps & ap->capabilities) == 0 ||
		    !ap->get_id_token ||
		    (prev_ap && !strcmp (ap->idtoken_class,
		    			 prev_ap->idtoken_class)))
			continue;

		token = (*ap->get_id_token) (ap, id);
		if (token) {
			token->next = list;
			list = token;
		}
		prev_ap = ap;
	}
	if (!list) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%p", NULL);
		return (NULL);
	}
	sec_log_ret ("%p", (void *) list);
	return (list);
}

/* sec_set_perm_cred_and_token -- Set the permission credentials and the
				  permissions token for the given id. */

int sec_set_perm_cred_and_token (Identity_t                id,
				 DDS_PermissionsCredential *perm_creds,
				 Token_t                   *perm_token)
{
	IdentityData_t	*p;

	sec_log_fct ("set_perm_cred_and_token");
	p = id_lookup (id, NULL);
	if (!p) {
		sec_log_ret ("%d", 0);
		return (0);
	}
	p->perm_cred = perm_creds;
	p->perm_token = token_ref (perm_token);
	sec_log_ret ("%d", 1);
	return (1);
}

AuthState_t sec_validate_remote_id (Identity_t       local_id,
				    unsigned char    local_key [16],
				    unsigned         local_caps,
				    Token_t          *rem_id_tokens,
				    Token_t          *rem_perm_tokens,
				    Token_t          **id_token,
				    Token_t          **perm_token,
				    unsigned char    rem_key [16],
				    Identity_t       *rem_id,
				    DDS_ReturnCode_t *error)
{
	Token_t			*id_list, *perm_list, *token = NULL, *perm_tok;
	Token_t			*ptoken;
	IdentityData_t		*p;
	AuthState_t		r;
	const SEC_AUTH		*ap = NULL;
	unsigned		i;

	sec_log_fct ("validate_remote_id");
	*id_token = NULL;
	*perm_token = NULL;
	if (!plugin_policy) {
		*error = DDS_RETCODE_OK;
		sec_log_ret ("%s", "OK");
		return (AS_OK);
	}
	if (!rem_id_tokens || !rem_perm_tokens) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	id_list = rem_id_tokens;
	perm_list = rem_perm_tokens;

	/* Check all Identity Tokens, in plugin priority order. The highest
	   priority plugin that has a token match is selected as the preferred
	   plugin. */
	r = AS_FAILED;
	for (i = 0; i < nauth_plugins; i++) {
		ap = auth_plugins [i];
		for (token = id_list, perm_tok = perm_list;
		     token;
		     token = token->next, perm_tok = (perm_tok) ? perm_tok->next : NULL) {
			if ((ap->capabilities & local_caps) == 0)
				continue;

			if (ap->idtoken_class &&
			    !strcmp (ap->idtoken_class, token->data->class_id) &&
			    ap->valid_remote &&
			    (r = (*ap->valid_remote) (ap,
			    			      local_id, local_key,
						      token->data,
						      perm_tok->data,
						      rem_key, rem_id)) != 
							      AS_FAILED)
				break;
		}
		if (token)
			break;
	}

	/* Exit if no plugin found. */
	if (!token || r == AS_FAILED) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}

	/* Check if there is also a plugin available for the permissions tokens
	   and permissions credentials. */
	ptoken = sec_known_permissions_tokens (rem_perm_tokens, local_caps);
	if (!ptoken) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}

	/* Add plugin code to Identity. */
	*id_token = token;
	*perm_token = ptoken;
	p = id_lookup (*rem_id, NULL);
	if (p) { /* Already exists: just release cached tokens. */
		token_unref (p->id_token);
		token_unref (p->perm_token);
	}
	else {	/* Create a new identity record. */
		p = id_create (*rem_id, error);
		if (!p) {
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			sec_log_ret ("%s", "FAILED");
			return (AS_FAILED);
		}
	}
	p->plugins [0] = ap;
	p->plugins [1] = NULL;
	p->id_token = token_ref (token);
	p->perm_token = token_ref (ptoken);
	*error = DDS_RETCODE_OK;
	sec_log_ret ("%s", "OK");
	return (r);
}

AuthState_t sec_begin_handshake_req (Identity_t         initiator,
				     Identity_t         replier,
				     Handshake_t        *handle,
				     DDS_HandshakeToken **msg_token,
				     DDS_ReturnCode_t   *error)
{
	DDS_HandshakeToken	*token;
	IdentityData_t		*init_p, *reply_p;
	HandshakeData_t		*handshake;
	const SEC_AUTH		*ap;

	*msg_token = NULL;

	sec_log_fct ("begin_handshake_req");
	init_p = id_lookup (initiator, NULL);
	reply_p = id_lookup (replier, NULL);
	ap = init_p->plugins [0];
	if (!ap) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	handshake = add_handshake (init_p, reply_p);
	if (!handshake) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	handshake->peer = reply_p;
	handshake->plugin = ap;
	token = (*ap->create_req) (ap, initiator, &handshake->pdata);
	if (!token) {
		remove_handshake (handshake->handle);
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	handshake->state = AS_PENDING_HANDSHAKE_MSG;

	*handle = handshake->handle;
	*msg_token = token;

	sec_log_ret ("%s", as_str [handshake->state]);
	return (handshake->state);
}

AuthState_t sec_begin_handshake_reply (DDS_HandshakeToken *msg_in,
				       Identity_t         initiator,
				       Identity_t         replier,
				       Handshake_t        *handle,
				       DDS_HandshakeToken **msg_out,
				       DDS_ReturnCode_t   *error)
{
	HandshakeData_t		*handshake;
	IdentityData_t		*init_p, *reply_p;
	DDS_HandshakeToken	*token;
	const SEC_AUTH		*ap = NULL;
	unsigned		i;

	sec_log_fct ("begin_handshake_reply");
	init_p = id_lookup (initiator, NULL);
	reply_p = id_lookup (replier, NULL);
	*msg_out = NULL;
	for (i = 0; i < nauth_plugins; i++) {
		ap = auth_plugins [i];
		if (ap->req_name &&
		    !strcmp (ap->req_name, msg_in->class_id) &&
		    ap->create_reply)
			break;
	}
	if (!ap || i >= nauth_plugins) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%s", "FAILED");
		return (AS_PENDING_HANDSHAKE_REQ);
	}
	handshake = add_handshake (init_p, reply_p);
	if (!handshake) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	handshake->peer = init_p;
	token = (*ap->create_reply) (ap,
				     msg_in,
				     replier,
				     initiator,
				     &handshake->pdata,
				     init_p->id_token->data,
				     init_p->perm_token->data,
				     &init_p->id_creds,
				     &init_p->perm_cred,
				     error);
	if (!token) {
		remove_handshake (handshake->handle);
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}

	handshake->state = AS_PENDING_HANDSHAKE_MSG;
	handshake->plugin = ap;

	*handle = handshake->handle;
	*msg_out = token;

	sec_log_ret ("%s", as_str [handshake->state]);
	return (handshake->state);
}

AuthState_t sec_process_handshake (DDS_HandshakeToken *msg_in,
				   Handshake_t        handle,
				   DDS_HandshakeToken **msg_out,
				   DDS_ReturnCode_t   *error)
{
	DDS_HandshakeToken	*token;
	HandshakeData_t		*handshake;
	IdentityData_t		*init_p;
	IdentityData_t		*reply_p;
	const SEC_AUTH		*ap;

	*msg_out = NULL;

	sec_log_fct ("process_handshake");
	if (!msg_in ||
	    !msg_in->class_id ||
	    !msg_in->binary_value1) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	handshake = get_handshake (handle);
	if (!handshake) {
		*error = DDS_RETCODE_PRECONDITION_NOT_MET;
		sec_log_ret ("%s", "FAILED");
		return (AS_FAILED);
	}
	init_p = handshake->initiator;
	reply_p = handshake->replier;
	ap = handshake->plugin;
	if (!strcmp (msg_in->class_id, ap->reply_name)) {

		/* Validate the reply token and create a final token if ok. */
		token = (*ap->create_final) (ap,
					     msg_in,
					     init_p->id,
					     reply_p->id,
					     &handshake->pdata,
					     reply_p->id_token->data,
					     reply_p->perm_token->data,
					     &reply_p->id_creds,
					     &reply_p->perm_cred,
					     &handshake->secret,
					     error);
		if (!token) {
			handshake->state = AS_FAILED;
			sec_log_ret ("%s", "FAILED");
			return (AS_FAILED);
		}
		*msg_out = token;
		handshake->state = AS_OK_FINAL_MSG;
	}
	else if (!strcmp (msg_in->class_id, ap->final_name)) {

		/* Check if valid final message token. */
		*error = (*ap->check_final) (ap,
					     msg_in,
					     reply_p->id,
					     init_p->id,
					     &handshake->pdata,
					     init_p->id_token->data,
					     init_p->perm_token->data,
					     &init_p->id_creds,
					     &init_p->perm_cred,
					     &handshake->secret);
		if (*error)
			handshake->state = AS_FAILED;
		else
			handshake->state = AS_OK;
	}
	else {
		*error = DDS_RETCODE_BAD_PARAMETER;
	}
	sec_log_ret ("%s", as_str [handshake->state]);
	return (handshake->state);
}

SharedSecret_t sec_get_shared_secret (Handshake_t handle)
{
	HandshakeData_t		*handshake;
	SharedSecret_t	h;

	sec_log_fct ("get_shared_secret");
	handshake = get_handshake (handle);
	h = (handshake) ? handshake->secret : 0;
	sec_log_ret ("%d", h);
	return (h);
}

const void *sec_get_auth_plugin (Handshake_t handle)
{
	HandshakeData_t		*handshake;

	sec_log_fct ("get_shared_secret");
	handshake = get_handshake (handle);
	return (!handshake ? NULL : handshake->plugin);
}

DDS_PermissionsCredential *sec_get_peer_perm_cred (Handshake_t handle)
{
	HandshakeData_t		  *handshake;
	DDS_PermissionsCredential *cp;

	sec_log_fct ("get_peer_perm_cred");
	handshake = get_handshake (handle);
	if (!handshake || !handshake->peer)
		cp = NULL;
	else
		cp = handshake->peer->perm_cred;
	sec_log_ret ("%p", (void *) cp);
	return (cp);
}

void sec_release_peer_perm_cred (DDS_PermissionsCredential *perms)
{
	sec_log_fct ("release_peer_perm_cred");
	DDS_DataHolder__free (perms);
	sec_log_retv ();
}

DDS_ReturnCode_t sec_set_auth_listener (sec_auth_revoke_listener_fct fct)
{
	DDS_ReturnCode_t	ret;

	sec_log_fct ("set_auth_listener");
	if (!fct)
		ret = DDS_RETCODE_BAD_PARAMETER;
	else {
		on_revoke_identity = fct;
		ret = DDS_RETCODE_OK;
	}
	sec_log_ret ("%d", ret);
	return (ret);
}

void sec_release_id_tokens (Token_t *list)
{
	sec_log_fct ("release_id_tokens");
	token_unref (list);
	sec_log_retv ();
}

void sec_release_handshake (Handshake_t handshake)
{
	sec_log_fct ("release_handshake");
	sec_release_shared_secret ((get_handshake (handshake))->secret);
	remove_handshake (handshake);
	sec_log_retv ();
}

void sec_release_identity (Identity_t id)
{
	IdentityData_t		*idp;
	DDS_SecurityReqData	data;

	sec_log_fct ("release_identity");
	idp = id_lookup (id, NULL);
	if (idp) {
		if (idp->nusers == 1) {
			data.handle = idp->id;
			sec_authentication_request (DDS_RELEASE_ID, &data);
		}
		id_unref (&idp);
	}
	sec_log_retv ();
}

void sec_release_shared_secret (SharedSecret_t secret)
{
	HandshakeData_t 	*handshake;
	
	sec_log_fct ("release_shared_secret");

	/* Clear the handle, so that the shared secret can no longer be requested */
	handshake = get_handshake_from_secret (secret);
	if (!handshake) {
		sec_log_retv ();
		return;
	}
	if (handshake->plugin && handshake->plugin->free_secret)
		(*handshake->plugin->free_secret) (handshake->plugin, secret);

	handshake->secret = 0;
	sec_log_retv ();
}

