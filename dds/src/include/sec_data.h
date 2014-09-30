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

/* sec_data.h -- Defines the common structures as used in the native DDS
		 security framework. */

#ifndef __sec_data_h_
#define	__sec_data_h_

#if !defined (NUTTX_RTOS)
#include "openssl/ssl.h"
#endif
#include "str.h"
#include "strseq.h"
#include "dds/dds_dcps.h"
#include "dds/dds_security.h"
#include "dds_data.h"

typedef struct {
	const char	*key;
	char		*value;
} DDS_Property;

DDS_Property *DDS_Property__alloc (const char *key);
void DDS_Property__init (DDS_Property *p, const char *key);
void DDS_Property__clear (DDS_Property *p);
void DDS_Property__free (DDS_Property *p);

DDS_SEQUENCE (DDS_Property, DDS_Properties);

DDS_Properties *DDS_Properties__alloc (void);
void DDS_Properties__init (DDS_Properties *p);
void DDS_Properties__clear (DDS_Properties *p);
void DDS_Properties__free (DDS_Properties *p);

typedef struct {
	const char	*key;
	DDS_OctetSeq	value;
} DDS_BinaryProperty;

DDS_BinaryProperty *DDS_BinaryProperty__alloc (const char *key);
void DDS_BinaryProperty__init (DDS_BinaryProperty *p, const char *key);
void DDS_BinaryProperty__clear (DDS_BinaryProperty *p);
void DDS_BinaryProperty__free (DDS_BinaryProperty *p);

DDS_SEQUENCE (DDS_BinaryProperty, DDS_BinaryProperties);

DDS_BinaryProperties *DDS_BinaryProperties__alloc (void);
void DDS_BinaryProperties__init (DDS_BinaryProperties *p);
void DDS_BinaryProperties__clear (DDS_BinaryProperties *p);
void DDS_BinaryProperties__free (DDS_BinaryProperties *p);

DDS_SEQUENCE (long long, DDS_LongLongSeq);

DDS_LongLongSeq *DDS_LongLongSeq__alloc (void);
void DDS_LongLongSeq__init (DDS_LongLongSeq *s);
void DDS_LongLongSeq__clear (DDS_LongLongSeq *s);
void DDS_LongLongSeq__free (DDS_LongLongSeq *s);


/* Encoding type in messages: */
#define DDS_ENC_STD	0	/* Standard encoding if 0. */
#define	DDS_ENC_VENDOR	1	/* Encoding with vendor-specific extensions.
				   Holds the PID value. */

typedef struct DDS_DataHolder_st DDS_DataHolder;
struct DDS_DataHolder_st {
	const char      	*class_id;		/* Class of token. */
	DDS_Properties    	*string_properties;	/* Property list. */
	DDS_BinaryProperties	*binary_properties;	/* Idem: binary. */
	DDS_StringSeq		*string_values;		/* Strings. */
	DDS_OctetSeq	   	*binary_value1;		/* Binary value 1. */
	DDS_OctetSeq	   	*binary_value2;		/* Binary value 2. */
	DDS_LongLongSeq		*longlongs_value;	/* List of long longs.*/
};

typedef DDS_DataHolder DDS_Token;

typedef struct DDS_TokenRef_st DDS_TokenRef;
struct DDS_TokenRef_st {
	DDS_DataHolder		*data;			/* DataHolder data. */
	int			encoding;		/* PID if != 0. */
	int			integral;		/* Data is in 1 chunk. */
	unsigned		nusers;			/* Reference count. */
	DDS_TokenRef		*next;			/* Used in token lists. */
};

DDS_DataHolder *DDS_DataHolder__alloc (const char *class_id);
void DDS_DataHolder__init (DDS_DataHolder *h, const char *class_id);
void DDS_DataHolder__clear (DDS_DataHolder *h);
void DDS_DataHolder__free (DDS_DataHolder *h);
DDS_DataHolder *DDS_DataHolder__copy (DDS_DataHolder *h);

Token_t *token_ref (Token_t *h);
void token_unref (Token_t *h);

DDS_Property *DDS_DataHolder_add_property (DDS_DataHolder *h,
					   const char     *key,
					   char           *value);

/* Add a normal (string) property to the DataHolder's property list.
   If successful, a pointer to the new property is returned, and the property
   can still be changed.  (Ex: value parameter NULL, but updated afterwards). */

DDS_Property *DDS_DataHolder_get_property (DDS_DataHolder *k, const char *key);

/* Lookup a string property in the DataHolder's property list.
   If found, a pointer to it is returned.  If not, NULL is returned. */

DDS_BinaryProperty *DDS_DataHolder_add_binary_property (DDS_DataHolder *h,
							const char     *key);

/* Add a binary property to the DataHolder's binary property list.
   If successful, a pointer to the new property is returned, and the property
   data can be changed. */

DDS_BinaryProperty *DDS_DataHolder_get_binary_property (DDS_DataHolder *k,
							const char *key);

/* Lookup a binary property in the DataHolder's binary property list.
   If found, a pointer to it is returned.  If not, NULL is returned. */

DDS_SEQUENCE (DDS_DataHolder, DDS_DataHolderSeq);

DDS_DataHolderSeq *DDS_DataHolderSeq__alloc (void);
void DDS_DataHolderSeq__free (DDS_DataHolderSeq *h);
void DDS_DataHolderSeq__init (DDS_DataHolderSeq *h);
void DDS_DataHolderSeq__clear (DDS_DataHolderSeq *h);

typedef DDS_DataHolder DDS_Credential_;

typedef DDS_Credential_ DDS_IdentityCredential;
typedef DDS_Credential_ DDS_PermissionsCredential;

/*typedef DDS_DataHolder DDS_Token;*/

typedef DDS_Token DDS_IdentityToken;
typedef DDS_Token DDS_PermissionsToken;
typedef DDS_Token DDS_CryptoToken;

typedef DDS_DataHolderSeq DDS_TokenSeq;

typedef DDS_TokenSeq DDS_CryptoTokenSeq;

typedef DDS_CryptoTokenSeq DDS_ParticipantCryptoTokenSeq;
typedef DDS_CryptoTokenSeq DDS_DataWriterCryptoTokenSeq;
typedef DDS_CryptoTokenSeq DDS_DataReaderCryptoTokenSeq;

typedef DDS_Token DDS_HandshakeToken;

typedef DDS_HandshakeToken DDS_HandshakeRequestToken;
typedef DDS_HandshakeToken DDS_HandshakeReplyToken;
typedef DDS_HandshakeToken DDS_HandshakeFinalToken;

#define GMCLASSID_SECURITY_ID_CREDENTIAL_PROPERTY	"dds.sec.identity"
#define GMCLASSID_SECURITY_PERM_CREDENTIAL_PROPERTY	"dds.sec.permissions"
#define GMCLASSID_SECURITY_AUTH_HANDSHAKE		"dds.sec.auth"
#define GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS	"dds.sec.participant_crypto_tokens"
#define GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS	"dds.sec.writer_crypto_tokens"
#define GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS	"dds.sec.reader_crypto_tokens"
#define GMCLASSID_SECURITY_VOL_DATA                     "dds.sec.volatile_data"

#define	GMCLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN "DDS:Access:PKI-Signed-XML-Permissions"
#define	GMCLASSID_SECURITY_PERMISSIONS_TOKEN 		"DDS:Access:PKI-Signed-XML-Permissions-SHA256"
#define	GMCLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN	"DDS:Auth:X.509-PEM"
#define	GMCLASSID_SECURITY_IDENTITY_TOKEN		"DDS:Auth:X.509-PEM-SHA256"

#define	GMCLASSID_SECURITY_HS_REQ_TOKEN_DH		"DDS:Auth:ChallengeReq:PKI-DH"
#define	GMCLASSID_SECURITY_HS_REQ_TOKEN_RSA		"DDS:Auth:ChallengeReq:PKI-RSA"
#define	GMCLASSID_SECURITY_HS_REPLY_TOKEN_DH		"DDS:Auth:ChallengeRep:PKI-DH"
#define	GMCLASSID_SECURITY_HS_REPLY_TOKEN_RSA		"DDS:Auth:ChallengeRep:PKI-RSA"
#define	GMCLASSID_SECURITY_HS_FINAL_TOKEN_DH		"DDS:Auth:ChallengeFin:PKI-DH"
#define	GMCLASSID_SECURITY_HS_FINAL_TOKEN_RSA		"DDS:Auth:ChallengeFin:PKI-RSA"

#define	GMCLASSID_SECURITY_DTLS_ID_TOKEN		"Auth:DTLS-Name"
#define	GMCLASSID_SECURITY_DTLS_PERM_TOKEN		"Access:DTLS-Name-MD5"

#define	GMCLASSID_SECURITY_AES_CTR_HMAC_PLUGIN		"DDS:Crypto:AES-CTR-HMAC-RSA/DSA-DH"
#define	GMCLASSID_SECURITY_AES_CTR_HMAC			"DDS:Crypto:AES-CTR-HMAC"

#define	QEO_CLASSID_SECURITY_IDENTITY_CREDENTIAL_TOKEN	"Qeo:Auth:X.509-PEM"
#define	QEO_CLASSID_SECURITY_IDENTITY_TOKEN		"Qeo:Auth:X.509-PEM-SHA256"

#define QEO_CLASSID_SECURITY_PERM_CREDENTIAL_ENC_PROPERTY "org.qeo.enc.permissions"
#define QEO_CLASSID_SECURITY_PERM_CREDENTIAL_SIG_PROPERTY "org.qeo.sig.permissions"

#define	QEO_CLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN "Qeo:Access:PKI-Signed-Policy-Enc"
#define	QEO_CLASSID_SECURITY_PERMISSIONS_TOKEN 		  "Qeo:Access:PKI-Signed-Policy-SHA256"

#define	QEO_CLASSID_SECURITY_HS_REQ_TOKEN_RSA		"Qeo:Auth:ChallengeReq:PKI-RSA"
#define	QEO_CLASSID_SECURITY_HS_REPLY_TOKEN_RSA		"Qeo:Auth:ChallengeRep:PKI-RSA"
#define	QEO_CLASSID_SECURITY_HS_FINAL_TOKEN_RSA		"Qeo:Auth:ChallengeFin:PKI-RSA"

#define QEO_CLASSID_SECURITY_POLICY "Qeo:Access:Policy-File"

typedef unsigned char DDS_BuiltinKey_t [16];

typedef struct {
	unsigned char	source_guid [16];
	long long	sequence_number;
} DDS_MessageIdentity;

typedef struct {
	DDS_MessageIdentity	message_identity;
	DDS_MessageIdentity	related_message_identity;
	unsigned char		destination_participant_key [16];
	unsigned char		destination_endpoint_key [16];
	unsigned char		source_endpoint_key [16];
	char			*message_class_id;
	DDS_DataHolderSeq	message_data;
} DDS_ParticipantGenericMessage;

typedef DDS_ParticipantGenericMessage DDS_ParticipantStatelessMessage;
typedef DDS_ParticipantGenericMessage DDS_ParticipantVolatileSecureMessage;
typedef DDS_ParticipantGenericMessage DDS_PolicyMessage;

extern DDS_TypeSupport	Property_ts;
extern DDS_TypeSupport	BinaryProperty_ts;
extern DDS_TypeSupport	DataHolder_ts;
extern DDS_TypeSupport	MessageIdentity_ts;
extern DDS_TypeSupport	ParticipantGenericMessage_ts;
extern DDS_TypeSupport	ParticipantStatelessMessage_ts;
extern DDS_TypeSupport	ParticipantVolatileSecureMessage_ts;

typedef struct DDS_LogInfo_st {
	unsigned char	source_guid [16];
	unsigned	log_level;
	char		*message;
	char		*category;
} DDS_LogInfo;

extern DDS_SecurityPolicy	plugin_policy;
extern Identity_t		local_identity;

typedef struct sec_config_st {
	POOL_LIMITS	data_holders;
	POOL_LIMITS	properties;
	POOL_LIMITS	bin_properties;
	POOL_LIMITS	sequences;
} SEC_CONFIG;

int sec_init (const SEC_CONFIG *limits);

/* Security submodule initialization. */

void sec_final (void);

/* Security submodule closedown. */

DDS_ReturnCode_t sec_register_types (void);

/* Create the Typesupport types that are needed for transporting security
   data. */

void sec_unregister_types (void);

/* Remove the previously created Typesupport types for security data. */

DDS_ReturnCode_t sec_authentication_request (unsigned r, DDS_SecurityReqData *d);

/* Call the authentication plugin for the given request function (r) and
   request data (d). */

DDS_ReturnCode_t sec_access_control_request (unsigned r, DDS_SecurityReqData *d);

/* Call the access control plugin for the given request function (r) and
   request data (d). */

DDS_ReturnCode_t sec_certificate_request (unsigned r, DDS_SecurityReqData *d);

/* Call the certificate plugin for certificate access with the given request
   function (r) and request data (d). */

DDS_ReturnCode_t sec_aux_request (unsigned r, DDS_SecurityReqData *d);

/* Call the auxiliary functions plugin for the given request function (r) and
   request data (d). */

DDS_ReturnCode_t sec_crypto_request (unsigned r, DDS_SecurityReqData *d);

/* Call the crypto functions plugin for the given request function (r) and 
   request data (d). */

void sec_pool_dump (size_t sizes []);

/* Dump the security pools. */

void sec_cache_dump (void);

/* Dump the security caches. */

#endif /* !__sec_data_h_ */

