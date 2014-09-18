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

/* dds_plugin.h -- External plugin API definitions.
		   Note that there are 2 incompatible versions of the plugin
		   API, depending on the DDS_NATIVE_SECURITY define.
		   If set, DDS native security will be enabled, requiring a 
		   more complex plugin API. */

#ifndef __dds_plugin_h_
#define __dds_plugin_h_

#include "dds/dds_dcps.h"

/* Authentication actions: */
#define DDS_AA_REJECTED		0
#define	DDS_AA_ACCEPTED		1
#define	DDS_AA_HANDSHAKE	2

/* Security plugin function parameters: */
typedef struct {
	int			action;
	unsigned		handle;
	size_t			length;
	size_t			rlength;
	DDS_DomainId_t		domain_id;
	void			*data;
	void			*rdata;
	const char		*name;
	const void		*strings;
#ifdef DDS_NATIVE_SECURITY
	void			*kdata;
	size_t			klength;
	const char		*tag;
#endif
	unsigned		secure;
} DDS_SecurityReqData;

#ifdef DDS_NATIVE_SECURITY

typedef enum {
	DDS_SC_INIT,		/* Initialization. */
	DDS_SC_AUTH,		/* Authentication. */
	DDS_SC_ACCESS,		/* Access Control. */
	DDS_SC_CERTS,		/* Certificate management. */
	DDS_SC_CRYPTO,		/* Crypto primitives. */
	DDS_SC_AUX		/* Auxilary functions. */
} DDS_SecurityClass;

/* Authentication security plugin function codes: */
typedef enum {
	DDS_VALIDATE_LOCAL_ID,		/* data, length -> handle. */
	DDS_VALIDATE_REMOTE_ID,		/* tag, data, length -> action. */
	DDS_GET_ID_NAME,		/* handle, data, length -> rlength. */
	DDS_GET_ID_CREDENTIAL,		/* handle, data, length -> rlength. */
	DDS_VERIFY_ID_CREDENTIAL,	/* handle, secure, data, length. */
	DDS_RELEASE_ID			/* handle. */
} DDS_AuthenticationRequest;

/* Secure field attributes in Access Control functions
   (DDS_CHECK_CREATE_PARTICIPANT and DDS_CHECK_CREATE_READER/WRITER). */
#define	DDS_SECA_LEVEL			0x0000ffff	/* Security level. */
#define	DDS_SECA_ACCESS_PROTECTED	0x00010000	/* Use access control.*/
#define	DDS_SECA_RTPS_PROTECTED		0x00020000	/* Encrypt RTPS msgs. */
#define	DDS_SECA_DISC_PROTECTED		0x00020000	/* Encrypt Discovery. */
#define	DDS_SECA_SUBMSG_PROTECTED	0x00040000	/* Encrypt Submessage.*/
#define	DDS_SECA_PAYLOAD_PROTECTED	0x00080000	/* Encrypt payload. */
#define	DDS_SECA_ENCRYPTION		0xfff00000	/* Encryption mode. */
#define	DDS_SECA_ENCRYPTION_SHIFT	20

/* Access Control security plugin function codes: */
typedef enum {
	DDS_VALIDATE_LOCAL_PERM,	/* domain_id, handle -> handle. */
	DDS_VALIDATE_REMOTE_PERM,	/* data, length -> handle. */
	DDS_CHECK_CREATE_PARTICIPANT,	/* domain_id, handle, data -> secure. */
	DDS_CHECK_CREATE_WRITER,	/* handle, name, data, rdata -> secure. */
	DDS_CHECK_CREATE_READER,	/* handle, name, data, rdata -> secure. */
	DDS_CHECK_CREATE_TOPIC,		/* handle, name, data. */
	DDS_CHECK_LOCAL_REGISTER,	/* handle, entity, data, length. */
	DDS_CHECK_LOCAL_DISPOSE,	/* handle, entity, data, length. */
	DDS_CHECK_REMOTE_PARTICIPANT,	/* domain_id, handle, data. */
	DDS_CHECK_REMOTE_READER,	/* handle, name, data. */
	DDS_CHECK_REMOTE_WRITER,	/* handle, name, data */
	DDS_CHECK_REMOTE_TOPIC,		/* handle, name, data. */
	DDS_CHECK_REMOTE_REGISTER,	/* handle, entity, data, length. */
	DDS_CHECK_REMOTE_DISPOSE,	/* handle, entity, data, length. */
	DDS_CHECK_LOCAL_WRITER_MATCH,   /* handle, domain_id, name, tag -> secure */
	DDS_CHECK_LOCAL_READER_MATCH,   /* handle, domain_id, name, tag -> secure */
	DDS_GET_PERM_CRED,		/* handle, data, length -> rlength. */
	DDS_SET_PERM_CRED,              /* handle, data, length. */
	DDS_RELEASE_PERM		/* handle. */
} DDS_AccessControlRequest;

/* Certificate handling and asymetric crypto plugin function codes: */
typedef enum {
	DDS_GET_CERT_X509,		/* handle, data. */
	DDS_GET_CA_CERT_X509,		/* handle, data. */
	DDS_GET_PRIVATE_KEY_X509,	/* handle, data. */
	DDS_GET_CERT_PEM,		/* handle, data, length -> rlength. */
	DDS_GET_NB_CA_CERT,		/* handle -> data. */
	DDS_VALIDATE_CERT,		/* handle, secure, data, length. */
	DDS_ENCRYPT_PUBLIC,		/* handle, data, length, rdata, rlength -> rlength. */
	DDS_DECRYPT_PRIVATE,		/* handle, data, length, rdata, rlength -> rlength. */
	DDS_SIGN_SHA256,		/* Generate a signed SHA256 result. */
	DDS_VERIFY_SHA256		/* Validate a signed SHA256 result. */
} DDS_CertificateRequest;

typedef enum {
	DDS_CRYPT_UPDATE,		/* Interior chunk, after CRYPT_BEGIN. */
	DDS_CRYPT_BEGIN,		/* First chunk of a number of chunks. */
	DDS_CRYPT_END,			/* Last chunk of a number of chunks. */
	DDS_CRYPT_FULL			/* Complete message. */
} DDS_CRYPT_MODE;

/* Symmetric crypto security plugin function codes: */
typedef enum {
	DDS_GEN_RANDOM,			/* Generate a random number (length bytes). */
	DDS_SHA1,			/* SHA1 checksum: action,data,length,rdata */
	DDS_SHA256,			/* SHA256 checksum: action,data,length,rdata. */
	DDS_AES128_CTR,			/* AES128-CTR enc/decrypt: action,kdata,klength,tag,secure,data,length,rdata */
	DDS_AES256_CTR,			/* AES256-CTR enc/decrypt: action,kdata,klength,tag,secure,data,length,rdata */
	DDS_HMAC_SHA1,                  /* HMAC-SHA1 hash: action,kdata,klength,data,length,rdata,rlength. */
	DDS_HMAC_SHA256                 /* HMAC-SHA256 hash: action,kdata,klength,data,length,rdata,rlength. */
} DDS_CryptoRequest;

/* TLS/DTLS security plugin function codes: */
typedef enum {
	DDS_SET_LIBRARY_INIT,		/* secure */
	DDS_SET_LIBRARY_LOCK,		/* - */
	DDS_UNSET_LIBRARY_LOCK,		/* - */
	DDS_GET_DOMAIN_SEC_CAPS,	/* domain_id -> secure. */
	DDS_ACCEPT_SSL_CX		/* data, rdata -> action. */
} DDS_SecuritySupportRequest;

typedef DDS_ReturnCode_t (*DDS_SecurityPluginFct) (
	DDS_SecurityClass   c,
	int                 code,
	DDS_SecurityReqData *data
);

#else /* Only Transport level security. */

/* Security plugin function types: */
typedef enum {
	
	/* Encryption: */
	DDS_GET_PRIVATE_KEY,            /* handle -> data. */
	DDS_SIGN_WITH_PRIVATE_KEY,      /* handle, lenght, data, secure -> rdata, rlength. */
	DDS_GET_NB_CA_CERT,             /* handle -> data. */
	DDS_VERIFY_SIGNATURE,           /* handle, length, data, rdata, rlength, name, secure. */

	/* Authentication: */
	DDS_VALIDATE_LOCAL_ID,		/* data, length -> handle. */
	DDS_SET_LOCAL_HANDLE,
	DDS_VALIDATE_PEER_ID,		/* data, length -> action, rdata, rlength. */
	DDS_SET_PEER_HANDLE,
	DDS_ACCEPT_SSL_CX,		/* data, rdata -> action. */
	DDS_GET_ID_TOKEN,		/* handle -> rdata, rlength. */
	DDS_CHALLENGE_ID,		/* data, length -> rdata, rlength. */
	DDS_VALIDATE_RESPONSE,		/* data, length -> action. */
	DDS_GET_CERT,                   /* handle -> data. */
	DDS_GET_CA_CERT,                /* handle -> data. */
	
	/* Access Control: */
	DDS_VALIDATE_LOCAL_PERM,	/* handle -> handle. */
	DDS_VALIDATE_PEER_PERM,		/* data, length -> handle. */
	DDS_CHECK_CREATE_PARTICIPANT,	/* domain_id, handle, data -> secure. */
	DDS_CHECK_CREATE_TOPIC,		/* handle, name, data. */
	DDS_CHECK_CREATE_WRITER,	/* handle, name, data, rdata. */
	DDS_CHECK_CREATE_READER,	/* handle, name, data, rdata. */
	DDS_CHECK_PEER_PARTICIPANT,	/* domain_id, handle, data. */
	DDS_CHECK_PEER_TOPIC,		/* handle, name, data. */
	DDS_CHECK_PEER_WRITER,		/* handle, name, data */
	DDS_CHECK_PEER_READER,		/* handle, name, data. */
	DDS_GET_PERM_TOKEN,		/* handle -> rdata, rlength. */

	/* Domain parameters: */
	DDS_GET_DOMAIN_SEC_CAPS		/* domain_id -> secure. */
} DDS_SecurityRequest;

typedef DDS_ReturnCode_t (*DDS_SecurityPluginFct) (
	DDS_SecurityRequest code,
	DDS_SecurityReqData *data
);

#endif

#endif /* !__dds_plugin_h_ */

