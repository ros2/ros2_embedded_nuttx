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

/* sec_plugin.h -- Security plugin definitions for authentications, access
		   control and crypto plugin modules. */

#ifndef __sec_plugin_h_
#define	__sec_plugin_h_

#include "sec_auth.h"
#include "sec_crypto.h"

/* Secure authentication plugin methods: */

typedef struct sec_auth_st SEC_AUTH;

typedef DDS_ReturnCode_t (*SA_CHECK_LOC) (const SEC_AUTH *ap,
					  Identity_t     local,
					  unsigned char  participant_key [16]);

/* Check if plugin can handle a local participant for negotiation.  If it is
   possible, the participant key may be altered by the plugin code. */

typedef AuthState_t (*SA_VALIDATE_REM) (const SEC_AUTH       *ap,
					Identity_t           initiator,
					unsigned char        init_key [16],
					DDS_IdentityToken    *rem_id_tok,
					DDS_PermissionsToken *rem_perm_tok,
					unsigned char        rem_key [16],
					Identity_t           *replier);

/* A remote participant was detected having this plugin's class id for its
   IdentityToken. Register it in the lower-layer security database. */

typedef Token_t *(*SA_GET_ID_TOKEN) (const SEC_AUTH *ap, Identity_t id);

/* Get an IdentityToken from an Identity handle. */

typedef void (*SA_RELEASE_ID) (const SEC_AUTH *ap, Identity_t h);

/* Release a previously received identity token and credentials. */

typedef DDS_HandshakeToken *(*SA_CREATE_REQ) (const SEC_AUTH *ap,
					      Identity_t     req,
					      void           **pdata);

/* Create a new Handshake Request message token with the given parameters.
   The req parameter is the identity of the local user, the Identity and
   Permission credentials are specified for correct token creation.
   The pdata argument is a pointer to a location in which a pointer may be
   stored for reference in the handshake process. */

typedef DDS_HandshakeToken *(*SA_CREATE_REPLY) (const SEC_AUTH            *ap,
					        DDS_HandshakeToken        *req,
					        Identity_t                replier,
						Identity_t                peer,
					        void                      **pdata,
						DDS_IdentityToken         *rem_id_tok,
						DDS_PermissionsToken      *rem_perm_tok,
					        DDS_IdentityCredential    **rem_id,
					        DDS_PermissionsCredential **rem_perm,
					        DDS_ReturnCode_t          *error);

/* Validate a received Handshake Request token and create a new Handshake Reply
   message token if it was valid. The id_cred and perm_cred arguments are the
   local user's Identity and Permissions credentials. On success, the new
   Handshake Reply token is returned and rem_id and rem_perm contain the
   received Identity and Permissions credentials. If something went wrong, the
   function returns NULL and the error code will be set.*/

typedef DDS_HandshakeToken *(*SA_CREATE_FINAL) (const SEC_AUTH            *ap,
					        DDS_HandshakeToken        *reply,
					        Identity_t                req,
						Identity_t                peer,
					        void                      **pdata,
						DDS_IdentityToken         *rem_id_tok,
						DDS_PermissionsToken      *rem_perm_tok,
					        DDS_IdentityCredential    **rem_id,
					        DDS_PermissionsCredential **rem_perm,
					        SharedSecret_t            *secret,
					        DDS_ReturnCode_t          *error);

/* Validate a Handshake Reply message and create a new Handshake Final message
   token if it was valid, as well as a shared secret for which the handle is
   given in *secret.  As a side effect, *rem_id and *rem_perm will be set to
   the received peer credentials.  If something went wrong, the function
   returns NULL and the error code will be set. */

typedef DDS_ReturnCode_t (*SA_CHECK_FINAL) (const SEC_AUTH            *ap,
					    DDS_HandshakeToken        *final,
					    Identity_t                replier,
					    Identity_t                peer,
					    void                      **pdata,
					    DDS_IdentityToken         *rem_id_tok,
					    DDS_PermissionsToken      *rem_perm_tok,
					    DDS_IdentityCredential    **rem_id,
					    DDS_PermissionsCredential **rem_perm,
					    SharedSecret_t            *secret);

/* Validate a Handshake Final message and return the associated shared secret
   handle if successful. If not, it returns NULL. */

typedef void (*SA_FREE_SECRET) (const SEC_AUTH *ap, SharedSecret_t secret);

/* The shared secret is no longer needed and can be disposed. */

typedef DDS_ReturnCode_t (*SA_GET_KX) (const SEC_AUTH *ap,
				       SharedSecret_t secret,
				       unsigned char  *kx_key,
				       unsigned char  *kx_mac_key);

/* Get the encryption parameters, based on the shared secret. */

struct sec_auth_st {

	/*Identity credentials/token handling. */
	unsigned	capabilities;	/* Capabilities mask. */
	const char	*id_class;	/* Identity class. */
	const char	*idtoken_class;	/* Identity Token class. */
	size_t		idtoken_size;	/* Size of Identity tokens. */
	SA_CHECK_LOC	check_local;	/* Local identity check. */
	SA_VALIDATE_REM	valid_remote;	/* Remote Identity validation. */
	SA_GET_ID_TOKEN	get_id_token;	/* Get Identity token. */
	SA_RELEASE_ID	release_id;	/* Release an Identity handle. */

	/* Alternative plugins list to handle this Identity token type. */
	const SEC_AUTH	*next;		/* Next in plugins list. */

	/* Handshake support data/functions. */
	const char	*req_name;	/* Name of Handshake Request tokens. */
	const char	*reply_name;	/* Name of Handshake Reply tokens. */
	const char	*final_name;	/* Name of Handshake Final tokens. */
	SA_CREATE_REQ	create_req;	/* New Handshake Request message. */
	SA_CREATE_REPLY	create_reply;	/* Handshake Request -> Reply message. */
	SA_CREATE_FINAL	create_final;	/* Handshake Reply -> Final, secret. */
	SA_CHECK_FINAL	check_final;	/* Validate Handshake Final -> secret. */

	/* Shared secret support functions. */
	SA_GET_KX       get_kx;         /* Get encryption parameters. */
	SA_FREE_SECRET	free_secret;	/* Release a no longer used secret. */
};

int sec_auth_add (const SEC_AUTH *ap, int req_default);

/* Add a new authentication plugin.  If req_default is set, this plugin will be
   used as the default mechanism for newly requested authentications. */


/* Secure access control plugin methods: */

typedef struct sec_perm_st SEC_PERM;

typedef DDS_ReturnCode_t (*SP_VALID_L_PERM) (const SEC_PERM *pp,
					     Identity_t     local,
					     Permissions_t  perms);

/* Check if a local identity and permissions handle is supported. */

typedef Permissions_t (*SP_VALID_R_PERM) (const SEC_PERM            *pp,
					  Identity_t                local,
					  Identity_t                remote,
					  DDS_PermissionsToken      *token,
					  DDS_PermissionsCredential *cred);

/* Get a permissions handle for the given token. */

typedef Token_t *(*SP_GET_P_TOKEN) (const SEC_PERM *pp,
				    Permissions_t  perm);

/* Get a PermissionsToken from a permissions handle. */

typedef void (*SP_REL_P_TOKEN) (const SEC_PERM *pp, Permissions_t h);

/* Release a previously received PermissionsToken. */

typedef DDS_PermissionsCredential *(*SP_GET_PC_TOKEN) (const SEC_PERM *pp,
						       Permissions_t  id);

/* Get a PermissionsCredential token from a permissions handle. */

typedef void (*SP_REL_PC_TOKEN) (const SEC_PERM            *pp, 
				 DDS_PermissionsCredential *creds);

/* Release previously received Permissions Credentials. */

struct sec_perm_st {
	unsigned	capabilities;	/* Capabilities mask. */
	const char	*perm_class;	/* Permissions class. */
	const char	*ptoken_class;	/* Permissions Token class. */
	size_t		ptoken_size;	/* Size of Permissions Tokens. */
	SP_VALID_L_PERM	valid_loc_perm;	/* Check a local permissions handle. */
	SP_VALID_R_PERM	valid_rem_perm;	/* Check a remote Permissions token. */
	SP_GET_P_TOKEN	get_perm_token;	/* Get Permissions Token. */
	SP_REL_P_TOKEN	rel_perm_token;	/* Release Permissions. */
	SP_GET_PC_TOKEN	get_perm_cred;	/* Get Permission Credentials. */
	SP_REL_PC_TOKEN	rel_perm_cred;	/* Release Permission Credentials. */
};

int sec_perm_add (const SEC_PERM *pp, int req_default);

/* Add a new access control plugin. If req_default is set, this plugin will be
   used as the default mechanism for access control. */

#ifdef DDS_QEO_TYPES
void dump_policy_version_list (void);
#endif


/* Crypto plugin methods: */

/* Crypto context: */
typedef struct crypto_data_st CryptoData_t;
typedef struct sec_crypto_st SEC_CRYPTO;

typedef ParticipantCrypto_t (*SP_REG_L_PART) (const SEC_CRYPTO *cp,
					      const char       *idtoken_name,
					      Domain_t         *domain,
					      DDS_ReturnCode_t *error);

/* Register the local domain participant. */

typedef ParticipantCrypto_t (*SP_REG_R_PART) (const SEC_CRYPTO *cp,
					      CryptoData_t     *local_p,
					      Participant_t    *remote_p,
					      const SEC_AUTH   *plugin,
					      SharedSecret_t   secret,
					      DDS_ReturnCode_t *error);

/* Register a discovered Participant for crypto operations. */

typedef DataWriterCrypto_t (*SP_REG_L_WRITER) (const SEC_CRYPTO *cp,
					       CryptoData_t     *local_p,
					       Writer_t         *w,
					       DDS_ReturnCode_t *error);

/* Register a local DataWriter for crypto operations. */

typedef DataReaderCrypto_t (*SP_REG_R_READER) (const SEC_CRYPTO   *cp,
					       CryptoData_t       *local_w,
					       CryptoData_t       *remote_p,
					       DiscoveredReader_t *dr,
					       int                relay_only,
					       DDS_ReturnCode_t   *error);

/* Register a discovered DataReader for crypto operations. */

typedef DataReaderCrypto_t (*SP_REG_L_READER) (const SEC_CRYPTO *cp,
					       CryptoData_t     *local_p,
					       Reader_t         *r,
					       DDS_ReturnCode_t *error);

/* Register a local DataReader for crypto operations. */

typedef DataWriterCrypto_t (*SP_REG_R_WRITER) (const SEC_CRYPTO   *cp,
					       CryptoData_t       *local_r,
					       CryptoData_t       *remote_p,
					       DiscoveredWriter_t *dw,
					       DDS_ReturnCode_t   *error);

/* Register a discovered DataWriter for crypto operations. */

typedef DDS_ReturnCode_t (*SP_UNREG_PART) (const SEC_CRYPTO *cp,
					   CryptoData_t     *part);

/* Unregister a Participant for crypto operations. */

typedef DDS_ReturnCode_t (*SP_UNREG_WRITER) (const SEC_CRYPTO *cp,
					     CryptoData_t     *w);

/* Unregister a DataWriter for crypto operations. */

typedef DDS_ReturnCode_t (*SP_UNREG_READER) (const SEC_CRYPTO *cp,
					     CryptoData_t     *r);

/* Unregister a DataReader for crypto operations. */


/* Crypto key exchange methods: */

typedef DDS_ReturnCode_t (*SP_CR_LP_TOKEN) (const SEC_CRYPTO   *cp,
					    CryptoData_t       *local_p,
					    CryptoData_t       *remote_p,
					    DDS_CryptoTokenSeq *tokens);

/* Create local Participant crypto tokens. */

typedef DDS_ReturnCode_t (*SP_SET_RP_TOKEN) (const SEC_CRYPTO   *cp,
					     CryptoData_t       *local_p,
					     CryptoData_t       *remote_p,
					     DDS_CryptoTokenSeq *tokens);

/* Set peer Participant crypto tokens. */

typedef DDS_ReturnCode_t (*SP_CR_LW_TOKEN) (const SEC_CRYPTO   *cp,
					    CryptoData_t       *local_w,
					    CryptoData_t       *remote_r,
					    DDS_CryptoTokenSeq *tokens);

/* Create local DataWriter crypto tokens. */

typedef DDS_ReturnCode_t (*SP_CR_LR_TOKEN) (const SEC_CRYPTO *cp,
					    CryptoData_t     *local_r,
					    CryptoData_t     *remote_w,
					    DDS_CryptoTokenSeq *tokens);

/* Create local DataReader crypto tokens. */

typedef DDS_ReturnCode_t (*SP_SET_DW_TOKEN) (const SEC_CRYPTO *cp,
					     CryptoData_t     *local_r,
					     CryptoData_t     *remote_w,
					     DDS_CryptoTokenSeq *tokens);

/* Set the crypto tokens for a discovered DataWriter. */

typedef DDS_ReturnCode_t (*SP_SET_DR_TOKEN) (const SEC_CRYPTO *cp,
					     CryptoData_t     *local_w,
					     CryptoData_t     *remote_r,
					     DDS_CryptoTokenSeq *tokens);

/* Set the crypto tokens for a discovered DataReader. */

typedef DDS_ReturnCode_t (*SP_REM_DW_TOKEN) (const SEC_CRYPTO   *cp,
					     CryptoData_t       *local_r,
					     CryptoData_t       *remote_p,
					     EntityId_t         *eid,
					     DDS_CryptoTokenSeq *tokens);

/* Remember the crypto tokens for a discovered DataWriter. */

typedef DDS_ReturnCode_t (*SP_REM_DR_TOKEN) (const SEC_CRYPTO   *cp,
					     CryptoData_t       *local_w,
					     CryptoData_t       *remote_p,
					     EntityId_t         *eid,
					     DDS_CryptoTokenSeq *tokens);

/* Remember the crypto tokens for a discovered DataReader. */


/* Encoding methods: */

typedef DB *(*SP_ENC_PAYLOAD) (const SEC_CRYPTO *cp,
			       DBW              *data,
			       CryptoData_t     *sender,
			       size_t           *enc_len,
			       DDS_ReturnCode_t *error);

/* Encode payload data and return the encrypted data. */

typedef RME *(*SP_ENC_DW_SMSG) (const SEC_CRYPTO *cp,
				RME              *submsg,
				CryptoData_t     *sender,
				CryptoData_t     *receivers [],
				unsigned         nreceivers,
				DDS_ReturnCode_t *error);

/* Encode a DataWriter submessage and return the secure submessage. */

typedef RME *(*SP_ENC_DR_SMSG) (const SEC_CRYPTO *cp,
				RME              *submsg,
				CryptoData_t     *sender,
				CryptoData_t     *receiver,
				DDS_ReturnCode_t *error);

/* Encode a DataReader submessage and return the secure submessage. */

typedef RMBUF *(*SP_ENC_MESSAGE) (const SEC_CRYPTO *cp,
				  RMBUF            *rtps_message,
				  CryptoData_t     *sender,
				  CryptoData_t     *receivers [],
				  unsigned         nreceivers,
				  DDS_ReturnCode_t *error);

/* Encode a complete RTPS message and return a new message with encrypted
   contents. */


/* Decoding methods: */

typedef DB *(*SP_DEC_PAYLOAD) (const SEC_CRYPTO *cp,
			       DBW              *encoded,
			       CryptoData_t     *receiver,
			       CryptoData_t     *sender,
			       size_t           *dec_len,
			       size_t           *db_ofs,
			       DDS_ReturnCode_t *error);

/* Decode a serialized data submessage element. */

typedef SubmsgCategory_t (*SP_PREP_SUBMSG) (const SEC_CRYPTO   *cp,
					    RME                *submsg,
					    CryptoData_t       *receiver,
					    CryptoData_t       *sender,
					    DataWriterCrypto_t *dwcrypto,
					    DataReaderCrypto_t *drcrypto,
					    DDS_ReturnCode_t   *error);

/* Preprocess a secure submessage to determine whether it is an encoded
   DataReader or DataWriter submessage.  Depending on the result, either
   *dwcrypto or *drcrypto will be set and either sec_decode_datawriter_submsg()
   or sec_decode_datareader_submsg() should be called. */

typedef RME *(*SP_DEC_DW_SMSG) (const SEC_CRYPTO *cp,
				RME              *submsg,
				CryptoData_t     *receiver,
				CryptoData_t     *sender,
				DDS_ReturnCode_t *error);

/* Decode a DataWriter submessage. */

typedef RME *(*SP_DEC_DR_SMSG) (const SEC_CRYPTO *cp,
				RME              *submsg,
				CryptoData_t     *receiver,
				CryptoData_t     *sender,
				DDS_ReturnCode_t *error);

/* Decode a DataReader submessage. */

typedef RMBUF *(*SP_DEC_MESSAGE) (const SEC_CRYPTO *cp,
				  RME              *submsg,
				  CryptoData_t     *receiver,
				  CryptoData_t     *sender,
				  DDS_ReturnCode_t *error);

/* Decode a complete RTPS message from a secure submessage element. */

typedef void (*SP_DUMP_CRYPTO) (const SEC_CRYPTO *cp,
				CryptoData_t     *data,
				Entity_t         *ep);

/* Display the crypto context of an entity. */

#define	MAX_C_KINDS	4	/* # of crypto methods supported (i.e. the
				   Transformation Kind Ids as a native encoded
				   32-bit number). */

/* Crypto plugin datastructure: */
struct sec_crypto_st {
	const char	*plugin;	/* Plugin name. */
	const char	*ctoken_class;	/* Crypto Token class. */

	/* Supported crypto Transformation Kind Ids: */
	const uint32_t	kind_ids [MAX_C_KINDS];

	/* Key factory. */
	SP_REG_L_PART	reg_loc_part;	/* Register a local DomainParticipant. */
	SP_REG_R_PART	reg_rem_part;	/* Register a peer Participant. */
	SP_REG_L_WRITER	reg_loc_writer;	/* Register a local DataWriter. */
	SP_REG_L_READER	reg_loc_reader;	/* Register a local DataReader. */
	SP_REG_R_WRITER	reg_rem_writer;	/* Register a remote DataWriter. */
	SP_REG_R_READER	reg_rem_reader;	/* Register a remote DataReader. */
	SP_UNREG_PART	unreg_part;	/* Unregister a participant. */
	SP_UNREG_WRITER	unreg_writer;	/* Unregister a DataWriter. */
	SP_UNREG_READER	unreg_reader;	/* Unregister a DataReader. */

	/* Key exchange. */
	SP_CR_LP_TOKEN	cr_lp_tokens;	/* Create local Participant tokens. */
	SP_SET_RP_TOKEN	set_rp_tokens;	/* Set peer Participant tokens. */
	SP_CR_LW_TOKEN	cr_w_tokens;	/* Create local DataWriter tokens. */
	SP_CR_LR_TOKEN	cr_r_tokens;	/* Create local DataReader tokens. */
	SP_SET_DW_TOKEN	set_dw_tokens;	/* Set remote DataWriter tokens. */
	SP_SET_DR_TOKEN	set_dr_tokens;	/* Set remote DataReader tokens. */
	SP_REM_DW_TOKEN	rem_dw_tokens;	/* Remember remote DataWriter tokens. */
	SP_REM_DR_TOKEN	rem_dr_tokens;	/* Remember remote DataReader tokens. */

	/* Encoding. */
	SP_ENC_PAYLOAD	enc_payload;	/* Encode payload data. */
	SP_ENC_DW_SMSG	enc_dw_submsg;	/* Encode a DataWriter submessage. */
	SP_ENC_DR_SMSG	enc_dr_submsg;	/* Encode a DataReader submessage. */
	SP_ENC_MESSAGE	enc_message;	/* Encode a complete RTPS message. */

	/* Decoding. */
	SP_DEC_PAYLOAD	dec_payload;	/* Decode payload data. */
	SP_PREP_SUBMSG	preproc_submsg;	/* Preprocess a submessage. */
	SP_DEC_DW_SMSG	dec_dw_submsg;	/* Decode a DataWriter submessage. */
	SP_DEC_DR_SMSG	dec_dr_submsg;	/* Decode a DataReader submessage. */
	SP_DEC_MESSAGE	dec_message;	/* Decode a complete RTPS message. */

	/* Debug. */
	SP_DUMP_CRYPTO	dump;		/* Display crypto information. */
};

int sec_crypto_add (const SEC_CRYPTO *cp);

/* Add a new crypto control plugin. */

#endif /* !__sec_plugin_h_ */

