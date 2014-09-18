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

/* sec_auth.h -- DDS Security - Authentication plugin definitions. */

#ifndef __sec_auth_h_
#define __sec_auth_h_

#include "dds/dds_security.h"
#include "sec_data.h"

typedef unsigned Handshake_t;
typedef unsigned SharedSecret_t;

int sec_auth_init (void);

/* Initialize the authentication plugin code. */

AuthState_t sec_validate_local_id (const char       *name,	/* Optional! */
				   unsigned char    key [16],
				   DDS_Credentials  *credentials,
				   size_t           xlength,
				   Identity_t       *local_id,
				   DDS_ReturnCode_t *error);

/* Validate local Identity Credentials. */

Token_t *sec_get_identity_tokens (Identity_t       id,
				  unsigned         caps,
				  DDS_ReturnCode_t *error);

/* Get the Identity Token that will be used in Participant discovery. */

int sec_set_perm_cred_and_token (Identity_t                id,
				 DDS_PermissionsCredential *perm_creds,
				 Token_t                   *perm_token);

/* Set the permission credentials and the permissions token for the given id. */

AuthState_t sec_validate_remote_id (Identity_t           local_id,
				    unsigned char        local_key [16],
				    unsigned             local_caps,
				    Token_t              *rem_id_tokens,
				    Token_t              *rem_perm_tokens,
				    Token_t              **id_token,
				    Token_t              **perm_token,
				    unsigned char        rem_key [16],
				    Identity_t           *rem_id,
				    DDS_ReturnCode_t     *error);

/* Validate a remote Identity Token. */

AuthState_t sec_begin_handshake_req (Identity_t         initiator,
				     Identity_t         replier,
				     Handshake_t        *handshake,
				     DDS_HandshakeToken **msg_token,
				     DDS_ReturnCode_t   *error);

/* Get a new Handshake Request message token. */

AuthState_t sec_begin_handshake_reply (DDS_HandshakeToken *msg_in,
				       Identity_t         initiator,
				       Identity_t         replier,
				       Handshake_t        *handshake,
				       DDS_HandshakeToken **msg_out,
				       DDS_ReturnCode_t   *error);

/* Send a Handshake Reply message token to a received Handshake Request. */

AuthState_t sec_process_handshake (DDS_HandshakeToken *msg_in,
				   Handshake_t        handshake,
				   DDS_HandshakeToken **msg_out,
				   DDS_ReturnCode_t   *error);

/* Process the continuation of a handshake, i.e. either this is due to a
   Handshake Reply or due to a Handshake Final message being received. */

SharedSecret_t sec_get_shared_secret (Handshake_t handshake);

/* Return a shared secret handle from a successful handshake. */

const void *sec_get_auth_plugin (Handshake_t handle);

/* Return the plugin that corresponds to the handshake handle. */

DDS_PermissionsCredential *sec_get_peer_perm_cred (Handshake_t handshake);

/* Return peer permissions credential data. */

typedef DDS_ReturnCode_t (*sec_auth_revoke_listener_fct) (Identity_t id);

/* Revoked identity listener function. */

DDS_ReturnCode_t sec_set_auth_listener (sec_auth_revoke_listener_fct fct);

/* On revoke identity listener for DDS */

void sec_release_id_tokens (Token_t *id_tokens);

/* Release Identity tokens. */

void sec_release_peer_perm_cred (DDS_PermissionsCredential *perms);

/* Release the peer permissions credential that was returned in
   sec_get_peer_perm_cred (). */

void sec_release_handshake (Handshake_t handshake);

/* Release a handshake context. */

void sec_release_identity (Identity_t id);

/* Release an Identity. */

void sec_release_shared_secret (SharedSecret_t secret);

/* Release a shared secret. */

#endif /* !__p_auth_h_ */

