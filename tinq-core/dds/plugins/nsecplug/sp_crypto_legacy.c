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

/* sp_crypto.c -- DDS Security Plugin - Crypto plugin implementations. */

#include "sp_crypto.h"
#include "list.h"
#include <string.h>

static CryptoList_t    *crypto_list = NULL;
static LocalKeysList_t *local_keys_list = NULL;
static KxList_t        *kx_list = NULL;

/* for all the types of local CryptoHandles, 
   we use one counter, so handles won't overlap */
static CryptoHandle_t crypto_handle = 1;

/************************************/
/*   List Manipulation functions.    */
/************************************/

static void init_crypto_list (void)
{
	if (!crypto_list) {
		if (!(crypto_list = malloc (sizeof (CryptoList_t))))
			fatal_printf ("Out of memory");
		LIST_INIT (*crypto_list);
	}
}

static void init_local_key_list (void)
{
	if (!local_keys_list) {
		if (!(local_keys_list = malloc (sizeof (LocalKeysList_t))))
			fatal_printf ("Out of Memory");
		LIST_INIT (*local_keys_list);
	}
}

static void init_kx_list (void)
{
	if (!kx_list) {
		if (!(kx_list = malloc (sizeof (KxList_t))))
			fatal_printf ("Out of memory");
		LIST_INIT (*kx_list);
	}
}

static void crypto_init (void) 
{
	init_crypto_list ();
	init_local_key_list ();
	init_kx_list ();
}

static CryptoNode_t *get_crypto_node (CryptoHandle_t local,
				      CryptoHandle_t remote)
{
	CryptoNode_t *node = NULL;

	if (remote == ~0U) {
		LIST_FOREACH (*crypto_list, node) {
			if (node->local->local == local)
				return (node);
		}
	}
	else if (local == ~0U) {
		LIST_FOREACH (*crypto_list, node)
			if (node->remote == remote)
				return (node);
	}
	else {
		LIST_FOREACH (*crypto_list, node)
			if (node->local->local == local && node->remote == remote)
				return (node);
	}

	return (NULL);
}

static CryptoNode_t *get_crypto_node_from_token (CryptoTokens_t *tokens)
{
	CryptoNode_t *node = NULL;

	LIST_FOREACH (*crypto_list, node)
		if (node->local_token == tokens)
			return (node);

	return (NULL);
}

static LocalKeysNode_t *get_local_keys_node (CryptoHandle_t local)
{
	LocalKeysNode_t *node;

	LIST_FOREACH (*local_keys_list, node)
		if (node->local == local)
			return (node);

	return (NULL);
}

static KxNode_t *get_kx_node (IdentityHandle_t id_handle)
{
	KxNode_t *node;

	LIST_FOREACH (*kx_list, node)
		if (node->id_handle == id_handle)
			return (node);

	return (NULL);
}

static LocalKeysNode_t *add_local_key_node (CryptoHandle_t local,
					    PartCryptoHandle_t id)
{
	LocalKeysNode_t *node;

	crypto_init ();

	node = get_local_keys_node (local);
	if (!node) {			    
		if (!(node = calloc (1, sizeof (CryptoNode_t)))) {
			fatal_printf ("Out of Memory");
		} else {
			node->local = local;
			node->refcount = 0;
			if (!(node->parent = get_local_keys_node (id)))
				node->parent = node;
			LIST_ADD_HEAD (*local_keys_list, *node);
		}
	}
	return (node);
}


static KxNode_t *add_kx_node (SharedSecretHandle_t secret)
{
	KxNode_t *node;

	crypto_init ();	
	node = get_kx_node (secret);
	if (!node) {
		if (!(node = calloc (1, sizeof (KxNode_t))))
			fatal_printf ("Out of memory");
		else {
			node->secret = secret;
			node->refcount = 0;
			LIST_ADD_HEAD (*kx_list, *node);
		}
	}
	return (node);			
}

/* Add a new node to the token list, 
   or set parameters to a node that was created before */

static CryptoNode_t *add_crypto_node (CryptoHandle_t       local,
				      CryptoHandle_t       remote,
				      PartCryptoHandle_t   parent,
				      SharedSecretHandle_t secret)
{
	CryptoNode_t    *node;
	LocalKeysNode_t *key_node;
	KxNode_t        *kx_node;

	crypto_init ();

	/* See if the node is already created */
	node = get_crypto_node (local, remote);
	if (!node) {
		/* Create the node */
		if (!(node = calloc (1, sizeof (CryptoNode_t)))) {
			fatal_printf ("Out of Memory");
		} else {
			/* Add a local key node to it */
			key_node = add_local_key_node (local);
			key_node->refcount ++;
			node->local = key_node;
			
			/* Add a kx node to it */
			kx_node = add_kx_node (secret);
			kx_node->refcount ++;
			node->kx_node = kx_node;

			/* Add parent node to it */
			if (!(node->parent = get_crypto_node (local, parent)))
				node->parent = node;
			
			/* Add to list*/
			LIST_ADD_HEAD (*crypto_list, *node);
		}
	}
	return (node);
}


/* Remove a node from the list */

static DDS_ReturnCode_t remove_crypto_node (CryptoNode_t *node)
{
	/* TODO */

	if (!node)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* PROBLEM: WHAT TO DO WHEN THE LOCAL TOKEN IS NOT YET REMOVED ???? */
	/* SHOULD I REMOVE IT, OR DO NOTHING */

	free (node->pair_key);
	node->pair_key = NULL;
	
	if (node->local_token == NULL && node->remote_token == NULL) {
		LIST_REMOVE (*crypto_list, *node);
		free (node);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t remove_local_key_node (LocalKeysNode_t *node)
{
	/* TODO */
	if (!node)
		return (DDS_RETCODE_BAD_PARAMETER);

	node->refcount --;
	if (node->refcount == 0) {
		free (node->key);
		LIST_REMOVE (*local_keys_list, *node);
		free (node);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t remove_kx_node (KxNode_t *node)
{
	/* TODO */
	if (!node)
		return (DDS_RETCODE_BAD_PARAMETER);
	
	node->refcount--;
	
	if (node->refcount == 0) {
		LIST_REMOVE (*kx_list, *node);
		free (node);
	}
	return (DDS_RETCODE_OK);
}

void dump_kx_node (KxNode_t *node, const char *prefix)
{
	if (node) {
		dbg_printf ("%sKx Node: %d\r\n", prefix, node->id_handle);
		dbg_printf ("%s         %s\r\n", prefix, node->KxKey);
		dbg_printf ("%s         %s\r\n", prefix, node->KxMacKey);
		dbg_printf ("%s         %d\r\n", prefix, node->refcount);
	}
}

void dump_key_material (KeyMaterial_AES_CTR_HMAC *key, const char *prefix)
{
	if (key) {
		dbg_printf ("%sKey: \r\n", prefix);
		dbg_printf ("%s     %d\r\n", prefix, key->cipher_kind);
		dbg_printf ("%s     %d\r\n", prefix, key->hash_kind);
		dbg_printf ("%s     %l\r\n", prefix, key->master_key_id);
		dbg_printf ("%s     %s\r\n", prefix, key->master_key);
		dbg_printf ("%s     %s\r\n", prefix, key->init_vector);
		dbg_printf ("%s     %s\r\n", prefix, key->hmac_key_id);
	}
}

void dump_local_key_node (LocalKeysNode_t *node, const char *prefix)
{
	char new_prefix [100];

	strcpy (&new_prefix [0], prefix);
	strcat (new_prefix, "           ");

	if (node) {
		dbg_printf ("%sLocal Key: %d\r\n", prefix, node->local);
		dump_key_material (node->key, &new_prefix [0]);
		dbg_printf ("%s           %d\r\n", prefix, node->refcount);
	}
}

void dump_crypto_node (CryptoNode_t *node, const char *prefix)
{
	unsigned i;
	Token_t *token;
	char new_prefix [100];

	strcpy (&new_prefix [0], prefix);
	strcat (new_prefix, "    ");

	if (node) {
		dbg_printf ("%sCrypto node: \r\n", prefix);
		dump_local_key_node (node->local, new_prefix);
		dump_key_material (node->pair_key, new_prefix);
		if (node->local_token)
			DDS_SEQ_FOREACH_ENTRY (*node->local_token, i, token) 
				dump_token (token, new_prefix);
		if (node->remote_token)
			DDS_SEQ_FOREACH_ENTRY (*node->remote_token, i, token) 
				dump_token (token, new_prefix);
		dbg_printf ("    secret_handle: %d\r\n", node->secret);
		dump_kx_node (node->kx_node, new_prefix);
	}
}

void dump_crypto_list (void)
{
	CryptoNode_t *node;

	dbg_printf ("*** Crypto list dump ***\r\n");

	LIST_FOREACH (*crypto_list, node) {
		dump_crypto_node (node, "");
	}
}

void dump_local_key_list (void)
{
	LocalKeysNode_t *node;

	dbg_printf ("*** Local key list dump ***\r\n");

	LIST_FOREACH (*local_keys_list, node) {
		dump_local_key_node (node, "");
	}

}

void dump_kx_list (void)
{
	KxNode_t *node;

	dbg_printf ("*** Kx list dump ***\r\n");

	LIST_FOREACH (*kx_list, node) {
		dump_kx_node (node, "");
	}
}

/************************************/
/*       Crypto Key Factory.        */
/************************************/

/* Create a new KeyMaterial_AES_CTR_HMAC object
   and return a handle */

static DDS_ReturnCode_t register_local_entity (PartCryptoHandle_t  parent,
					       Property_t          *property, 
					       CryptoHandle_t      *handle)
{
	KeyMaterial_AES_CTR_HMAC *key;
	LocalKeysNode_t          *node;
	KxNode_t                 *kx_node;

	if (!parent)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* TODO: perhaps check if id and perm are really valid */
	/* TODO: perhaps store the id in the localkeysnode,
	   or use the same id for the cryptohandle (will not work, cause part (id) vs reader vs writer) */

	if (!(key = calloc (1, sizeof (KeyMaterial_AES_CTR_HMAC))))
		fatal_printf ("Out of Memory");

	/* TODO: fill the key */

	node = add_local_key_node (crypto_handle, parent);
	node->key = key;
	*handle = crypto_handle ++;
	return (DDS_RETCODE_OK);
}

/* Create a new KeyMaterial_AES_CTR_HMAC object
   and return a handle. Also create a KxKey and KxMacKey*/

static DDS_ReturnCode_t register_matched_remote (CryptoHandle_t       local,
						 PartCryptoHandle_t   remote,
						 SharedSecretHandle_t secret,
						 CHALLENGE            a,
						 CHALLENGE            b,
						 CryptoHandle_t       *handle)
{

	/* Remember the remote id, cause we must be able to link 
	   Part/reader/writer together */

	KeyMaterial_AES_CTR_HMAC *key;
	CryptoNode_t             *node;
	
	if (!(key = calloc (1, sizeof (KeyMaterial_AES_CTR_HMAC))))
		fatal_printf ("Out of memory");

	/* TODO: fill the key */
	
	node = add_crypto_node (local, crypto_handle, remote, secret);
	node->pair_key = key;
	node->secret = secret;

	/* TODO */
	strcpy (node->kx_node->KxKey, "");
	strcpy (node->kx_node->KxMacKey, "");
		
	*handle = crypto_handle ++;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_register_local_participant (IdentityHandle_t    id,
						PermissionsHandle_t perm,
						Property_t          *property, 
						PartCryptoHandle_t  *handle)
{
	KeyMaterial_AES_CTR_HMAC *key;
	LocalKeysNode_t          *node;
	KxNode_t                 *kx_node;

	if (!id)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* TODO: perhaps check if id and perm are really valid */
	/* TODO: perhaps store the id in the localkeysnode,
	   or use the same id for the cryptohandle (will not work, cause part (id) vs reader vs writer) */

	if (!(key = calloc (1, sizeof (KeyMaterial_AES_CTR_HMAC))))
		fatal_printf ("Out of Memory");

	/* TODO: fill the key */

	node = add_local_key_node (crypto_handle, 0);
	node->key = key;
	*handle = crypto_handle ++;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_register_matched_remote_part (PartCryptoHandle_t   handle,
						  IdentityHandle_t     remote,
						  PermissionsHandle_t  rem_perm,
						  SharedSecretHandle_t secret,
						  PartCryptoHandle_t   *part_handle)
{
	KeyMaterial_AES_CTR_HMAC *key;
	CryptoNode_t             *node;
	
	if (!(key = calloc (1, sizeof (KeyMaterial_AES_CTR_HMAC))))
		fatal_printf ("Out of memory");

	/* TODO: fill the key */
	
	node = add_crypto_node (local, crypto_handle, 0, secret);
	node->pair_key = key;
	node->secret = secret;

	/* TODO */
	strcpy (node->kx_node->KxKey, "");
	strcpy (node->kx_node->KxMacKey, "");
		
	*handle = crypto_handle ++;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_register_local_datawriter (PartCryptoHandle_t  id,
					       Property_t          *property,
					       DWCryptoHandle_t    *handle)
{
	return (register_local_entity (id, 
				       property, 
				       (CryptoHandle_t *) handle));
}

DDS_ReturnCode_t sp_register_matched_remote_dr (DWCryptoHandle_t     handle,
						PartCryptoHandle_t   remote,
						SharedSecretHandle_t secret,
						DRCryptoHandle_t     *dr_handle)
{
	return (register_matched_remote ((CryptoHandle_t *) handle,
					 remote,
					 secret,
					 (CryptoHandle_t *) dr_handle));
}

DDS_ReturnCode_t sp_register_local_datareader (PartCryptoHandle_t  id,
					       Property_t          *property, 
					       DRCryptoHandle_t    *handle)
{
	return (register_local_entity (id, 
				       property, 
				       (CryptoHandle_t *) handle));
}

DDS_ReturnCode_t sp_register_matched_remote_dw (DRCryptoHandle_t     handle, 
						PartCryptoHandle_t   remote,
						SharedSecretHandle_t secret,
						DWCryptoHandle_t     *dw_handle)
{
	return (register_matched_remote ((CryptoHandle_t *) handle,
					 remote,
					 secret,
					 (CryptoHandle_t *) dw_handle));
}

/* unregister both local and remote entities */

static DDS_ReturnCode_t unregister_entity (CryptoHandle_t handle)
{
	CryptoNode_t *node;
	LocalKeysNode_t *key;
	DDS_ReturnCode_t ret1, ret2;

	if (!handle)
		return (DDS_RETCODE_BAD_PARAMETER);
	
	if ((node = get_crypto_node (handle, ~0U)))
		ret1 = remove_local_key_node (node->local);
	else
		ret1 = DDS_RETCODE_ERROR;

	node = get_crypto_node (~0U, handle);
	ret2 = remove_crypto_node (node);

	if (ret1 && ret2)
		return (DDS_RETCODE_ERROR);

	return (DDS_RETCODE_OK);
}
			  
DDS_ReturnCode_t sp_unregister_participant (PartCryptoHandle_t handle)
{
	return (unregister_entity ((CryptoHandle_t *) handle));
}

DDS_ReturnCode_t sp_unregister_datawriter (DWCryptoHandle_t handle)
{
	return (unregister_entity ((CryptoHandle_t *) handle));
}

DDS_ReturnCode_t sp_unregister_datareader (DRCryptoHandle_t handle)
{
	return (unregister_entity ((CryptoHandle_t *) handle));
}

/************************************/
/*  Crypto Key Exchange Interface.  */
/************************************/

/* Create 2 CryptoTokens. One contains participantKeyMaterial.
   second contains p2pKeyMaterial */

static DDS_ReturnCode_t create_local_crypto_tokens (CryptoHandle_t local,
						    CryptoHandle_t remote,
						    CryptoTokens_t *tokens)
{
	CryptoToken_t    *token1, *token2;
	CryptoTokens_t   *tokenSeq;
	DDS_ReturnCode_t ret;
	CryptoNode_t     *node;

	if (!(node = get_crypto_node (local, remote)))
		goto error;

	tokens = malloc (sizeof (CryptoTokens_t));
	DDS_SEQ_INIT (*tokens);

	/* TODO */

	/* Encrypt with node->kx_node->KxKey and node->kx_node->KxMacKey */
	/* use node->local->key in the first token */
	
	/* The first token  contains ParticpantKeyMaterial 
	   created on call to register_local_part */
	if (!(token1 = token_init (crypto_id,
				   crypto_aes256_sha256_wid,
				   NULL,
				   NULL,
				   0,
				   NULL,
				   0)))
		goto memError;
 
	/* use node->key in the second token */

	/* Second token contains p2p KeyMaterial created on call
	   to register matched remote participant */
	if (!(token2 = token_init (crypto_id,
				   crypto_aes256_sha256_wid,
				   NULL,
				   NULL,
				   0,
				   NULL,
				   0)))
		goto memError;

	if ((ret = dds_seq_prepend (tokenSeq, token1)))
		goto error;
	
	if ((ret = dds_seq_prepend (tokenSeq, token2)))
		goto error;

	node->local_token = tokenSeq;
	tokens = tokenSeq;
	return (DDS_RETCODE_OK);
 error:
	tokens = NULL;
	return (ret);
 memError:
	tokens = NULL;
	return (DDS_RETCODE_OUT_OF_RESOURCES);

}

/* Set the crypto tokens from the remote created by
   create local crypto tokens on the remote side */

static DDS_ReturnCode_t set_remote_crypto_tokens (CryptoHandle_t local,
						  CryptoHandle_t remote, 
						  CryptoTokens_t *tokens)
{
	CryptoNode_t *node;

	if (!(node = get_crypto_node (local, remote)))
		return (DDS_RETCODE_BAD_PARAMETER);

	node->remote_token = tokens;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_create_local_part_crypto_tokens (PartCryptoHandle_t local, 
						     PartCryptoHandle_t remote, 
						     CryptoTokens_t     *tokens)
{
	return (create_local_crypto_tokens ((CryptoHandle_t) local,
					    (CryptoHandle_t) remote,
					    tokens));
}

DDS_ReturnCode_t sp_set_remote_part_crypto_tokens (PartCryptoHandle_t local,
						   PartCryptoHandle_t remote, 
						   CryptoTokens_t     *tokens)
{
	return (set_remote_crypto_tokens ((CryptoHandle_t) local,
					  (CryptoHandle_t) remote,
					  tokens));
}

DDS_ReturnCode_t sp_create_local_dw_crypto_tokens (DWCryptoHandle_t local,
						   DRCryptoHandle_t remote,
						   CryptoTokens_t   *tokens)
{
	return (create_local_crypto_tokens ((CryptoHandle_t) local,
					    (CryptoHandle_t) remote,
					    tokens));
}

DDS_ReturnCode_t sp_set_remote_dw_crypto_tokens (DRCryptoHandle_t local,
						 DWCryptoHandle_t remote,
						 CryptoTokens_t   *tokens)
{
	return (set_remote_crypto_tokens ((CryptoHandle_t) local,
					  (CryptoHandle_t) remote,
					  tokens));
}

DDS_ReturnCode_t sp_create_local_dr_crypto_tokens (DRCryptoHandle_t local,
						   DWCryptoHandle_t remote,
						   CryptoTokens_t   *tokens)
{
	return (create_local_crypto_tokens ((CryptoHandle_t) local,
					    (CryptoHandle_t) remote,
					    tokens));
}

DDS_ReturnCode_t sp_set_remote_dr_crypto_tokens (DWCryptoHandle_t local,
						 DRCryptoHandle_t remote,
						 CryptoTokens_t   *tokens)
{
	return (set_remote_crypto_tokens ((CryptoHandle_t) local,
					  (CryptoHandle_t) remote,
					  tokens));
}

DDS_ReturnCode_t sp_release_crypto_tokens (CryptoTokens_t *tokens)
{
	CryptoNode_t *node;
	CryptoToken_t *token;
	int i = 0, nbOfTokens = 2;

	if ((node = get_crypto_node_from_token (tokens))) {
		/* the tokens are sequence*/
		if (node->local_token) {
			for (i = 0; i < nbOfTokens; i++) {
				dds_seq_remove_first (node->local_token, token);
				token_free (token);
			}
			dds_seq_cleanup (node->local_token);
			free (node->local_token);
			node->local_token = NULL;
		}
	}
}

/***********************/
/*  Crypty Transform.  */
/***********************/

DDS_ReturnCode_t sec_encode_serialized_data (unsigned char    *buffer,
					     size_t           length,
					     DWCryptoHandle_t sender,
					     unsigned char    *encoded,
					     size_t           *enc_len)
{
	LocalKeysNode_t *senderNode = NULL;
	CryptoNode_t    *remote_node;

	if (!(senderNode = get_local_keys_node (sender)))
		return (DDS_RETCODE_BAD_PARAMETER);

	/* writerKeyMaterial = senderNode->key; */
}

DDS_ReturnCode_t encode_submsg (unsigned char  *submessage,
				size_t         length,
				CryptoHandle_t sender,
				CryptoHandle_t *receivers,
				unsigned       nb_of_receivers,
				unsigned char  *encoded,
				size_t         *enc_len)
{
	LocalKeysNode_t *senderNode = NULL;
	unsigned         i;
	CryptoHandle_t  *ptr;
	CryptoNode_t    *remote_node;

	ptr = receivers;
	if (!(senderNode = get_local_keys_node (sender)))
		return (DDS_RETCODE_BAD_PARAMETER);

	LIST_FOREACH (*crypto_list, remote_node) {
		for (i = 0; i < nb_of_receivers; i++) {
			if (remote_node->local->local == *ptr) {
				/* Use remote_node->pair_key to calc digest */
			}
		ptr ++;
		}
	}

}

DDS_ReturnCode_t sec_encode_dw_submsg (unsigned char    *submessage,
				       size_t           length,
				       DWCryptoHandle_t sender,
				       DRCryptoHandle_t *receivers,
				       unsigned         nb_of_receivers,
				       unsigned char    *encoded,
				       size_t           *enc_len)
{
	return (encode_submsg (submessage,
			       length,
			       (CryptoHandle_t) sender,
			       (CryptoHandle_t *) receivers,
			       nb_of_receivers,
			       encoded,
			       enc_len));
}

DDS_ReturnCode_t sec_encode_dr_submsg (unsigned char    *submessage,
				       size_t           length,
				       DRCryptoHandle_t sender,
				       DWCryptoHandle_t *receivers,
				       unsigned         nb_of_receivers,
				       unsigned char    *encoded,
				       size_t           *enc_len)
{
	return (encode_submsg (submessage,
			       length,
			       (CryptoHandle_t) sender,
			       (CryptoHandle_t *) receivers,
			       nb_of_receivers,
			       encoded,
			       enc_len));
}

DDS_ReturnCode_t sec_encode_rtps_message (unsigned char      *rtps_message,
					  size_t             length,
					  PartCryptoHandle_t sender,
					  PartCryptoHandle_t *receivers,
					  unsigned           nb_of_receivers,
					  unsigned char      *encoded,
					  size_t             *enc_len)
{
	return (encode_submsg (rtps_message,
			       length,
			       (CryptoHandle_t) sender,
			       (CryptoHandle_t *) receivers,
			       nb_of_receivers,
			       encoded,
			       enc_len));
}

DDS_ReturnCode_t sec_preprocess_secure_submsg (unsigned char              *encoded,
					       size_t                     length,
					       PartCryptoHandle_t         receiver,
					       PartCryptoHandle_t         sender,
					       DWCryptoHandle_t           dw_crypto,
					       DRCryptoHandle_t           dr_crypto,
					       DDS_SecureSubmsgCategory_t category)
{
	/* TODO */
}

DDS_ReturnCode_t sec_decode_serialized_data (unsigned char    *encoded, 
					     size_t           length,
					     DWCryptoHandle_t sender,
					     DRCryptoHandle_t receiver,
					     unsigned char    *decoded,
					     size_t           *dec_len)
{
	/* TODO */
}

DDS_ReturnCode_t sec_decode_dw_submsg (unsigned char    *encoded,
				       size_t           length,
				       DRCryptoHandle_t receiver,
				       DWCryptoHandle_t sender,
				       unsigned char    *decoded,
				       size_t           *dec_len)
{
	/* TODO */
}

DDS_ReturnCode_t sec_decode_dr_submsg (unsigned char    *encoded,
				       size_t           length,
				       DWCryptoHandle_t receiver,
				       DRCryptoHandle_t sender,
				       unsigned char    *decoded,
				       size_t           *dec_len)
{
	/* TODO */
}

DDS_ReturnCode_t sec_decode_rtps_message (unsigned char      *encoded,
					  size_t             length,
					  PartCryptoHandle_t receiver,
					  PartCryptoHandle_t sender,
					  unsigned char      *decoded,
					  size_t             *dec_len)
{
	/* TODO */
}

#if 0

/* Calculation of the session specific data */

static unsigned char *calculate_sessionSalt (unsigned char *master_session_salt,
					     unsigned char *session_id,
					     unsigned char *master_key)
{
	unsigned char *buffer;
	/* The resulting buffer size is the size of the specific digest function */
	unsigned char *result = malloc (sizeof (EVP_MAX_MD_SIZE));

	strcpy (result, "SessionSalt");
	strcat (result, master_session_salt);
	strcat (result, session_id);

	

	return (result);
}

static unsigned char *calculate_sessionKey (unsigned char *master_salt,
					    unsigned char *session_id,
					    unsigned char *master_key)
{
	unsigned char *buffer;
	unsigned char *result = malloc (sizeof (EVP_MAX_MD_SIZE));

	strcpy (result, "SessionKey");
	strcat (result, master_salt);
	strcat (result, session_id);

	HMAC (EVP_SHA1 (), master_key, strlen (master_key), buffer, buffer_len, result, result_len);

	realloc (result, result_len);

	return (result);
}

static unsigned char *calculate_sessionHMACKey (unsigned char *master_hmac_salt,
						unsigned char *session_id,
						unsigned char *master_key)
{
	unsigned char *buffer;
	unsigned char *result = malloc (sizeof (EVP_MAX_MD_SIZE));

	strcpy (result, "SessionHMACKey");
	strcat (result, master_hmac_salt);
	strcat (result, session_id);

	HMAC (EVP_SHA1 (), master_key, strlen (master_key), buffer, buffer_len, result, result_len);

	realloc (result, result_len);

	return (result);
}

#endif
