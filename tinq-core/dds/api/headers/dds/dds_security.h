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

/* dds_security.h -- DDS Security interface. */

#ifndef __dds_security_h_
#define	__dds_security_h_

#include "dds/dds_dcps.h"
#include "dds/dds_plugin.h"
#if !defined(NUTTX_RTOS)
#include "openssl/safestack.h"
#include "openssl/ossl_typ.h"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
	DDS_CRYPT_NONE,
	DDS_CRYPT_HMAC_SHA1,
	DDS_CRYPT_HMAC_SHA256,
	DDS_CRYPT_AES128_HMAC_SHA1,
	DDS_CRYPT_AES256_HMAC_SHA256
} DDS_CRYPT_STD_TYPE;

typedef enum {
	DDS_FORMAT_PEM,		/* PEM format. */
	DDS_FORMAT_X509,	/* X.509 format. */
	DDS_FORMAT_RSA,		/* RSA format. */
	DDS_FORMAT_ASN1		/* ASN.1 format. */
} DDS_CredentialsFormat;

typedef struct {
	DDS_CredentialsFormat	format;		/* Format of credentials data. */
	const void		*data;
	size_t			length;
} DDS_Credential;

typedef struct {
	DDS_Credential	private_key;		/* Private key. */
	unsigned	num_certificates;
	DDS_Credential	certificates [1];	/* Certificates or chains. */
} DDS_DataCredentials;

typedef struct {
	const char	*private_key_file;
	const char	*certificate_chain_file;
} DDS_FileCredentials;

typedef struct {
	const char	*engine_id;
	const char	*cert_id;
	const char	*priv_key_id;
} DDS_SecurityEngine;

typedef struct {
	STACK_OF(X509) *certificate_list;
	EVP_PKEY *private_key;
} DDS_SSLDataCredentials;
	
typedef enum {
	DDS_DATA_BASED,
	DDS_FILE_BASED,
	DDS_ENGINE_BASED,
	DDS_SSL_BASED
} DDS_CredentialKind;

typedef struct {
	DDS_CredentialKind       credentialKind;
	union {
	  DDS_DataCredentials	 data;
	  DDS_FileCredentials	 filenames;
	  DDS_SecurityEngine	 engine;
	  DDS_SSLDataCredentials sslData;
	}			 info;
#ifdef DDS_NATIVE_SECURITY
	DDS_Credential		 permissions;
#endif
} DDS_Credentials;

/* Copy a credentials structure and return it. The size parameter will be
   set to the total size of the data block.  All pointers in the resulting
   data will point to locations within the data block.
   If there is a problem in the source credentials data or an other
   error happens, the function returns NULL and *error will be set. */
DDS_Credentials *DDS_Credentials__copy (DDS_Credentials  *crp,
					size_t           *size,
					DDS_ReturnCode_t *error);

/* Free a credentials structure. */
void DDS_Credentials__free (DDS_Credentials *crp);

/* Set user credentials.
   This function is mandatory if access to secure domains is needed.
   It is allowed to set the credentials more than once, since they are used
   only when a new DomainParticipant is created.  Changes do not have an effect
   on existing domains.
   Note that the credentials information will not be stored locally, but is
   instead delivered to the security agent for safekeeping.  Only a reference
   is stored in DDS. */
DDS_EXPORT DDS_ReturnCode_t DDS_Security_set_credentials (
	const char      *name,		/* Obsolete since superfluous in v2! */
	DDS_Credentials *credentials
);

#ifdef DDS_NATIVE_SECURITY

/* Cleanup user credentials. */
DDS_EXPORT void DDS_Security_cleanup_credentials (void);

#endif

typedef enum {
	DDS_SECURITY_UNSPECIFIED,	/* Unset security policy. */
	DDS_SECURITY_LOCAL,		/* Local, i.e. via callback functions.*/
	DDS_SECURITY_AGENT		/* Remote via secure channel. */
} DDS_SecurityPolicy;

/* Set the security policy.  This should be done only once and before any
   credentials are assigned or any DomainParticipants are created! */
DDS_EXPORT DDS_ReturnCode_t DDS_Security_set_policy (
	DDS_SecurityPolicy policy,
	DDS_SecurityPluginFct plugin
);

#ifdef DDS_NATIVE_SECURITY

/* Set the crypto plugin library. */
DDS_EXPORT DDS_ReturnCode_t DDS_Security_set_crypto (
	DDS_SecurityPluginFct plugin
);

#endif

/* Either have DDS do the initialization of the security library or tell DDS not
   to do it and do it yourself.  If value is 0, DDS does not initialize it. */
DDS_EXPORT void DDS_Security_set_library_init (
	int value
);

/* Let DDS do the locking of the security library. */
DDS_EXPORT void DDS_Security_set_library_lock (void);

/* Unset the locking mechanism of the security library. */
DDS_EXPORT void DDS_Security_unset_library_lock (void);

#ifdef DDS_NATIVE_SECURITY

/* Revoke a participant, i.e. refuse all further communication with it. */
DDS_EXPORT DDS_ReturnCode_t DDS_Security_revoke_participant (
	DDS_DomainId_t id,
	DDS_InstanceHandle_t part
);

/* Indicates to DDS security that the local permissions credential was changed
   probably resulting in a rehandshake with existing authorized peers. */
DDS_EXPORT void DDS_Security_permissions_changed (void);

/* same as DDS_Security_permissions_changed (),
   but without a forced rehandshake. */
DDS_EXPORT void DDS_Security_permissions_notify (void);

/* Indicates to DDS security that specific topic permissions were modified,
   resulting in possible changes in topic matching.
   The first argument must be a valid DomainParticipant. The handle parameter
   is an optional peer participant handle or 0.  The topic_name parameter can
   contain wildcards and must not be NULL. */
DDS_EXPORT void DDS_Security_topics_reevaluate (
	DDS_DomainParticipant part,
	DDS_InstanceHandle_t handle,
	const char *topic_name
);

DDS_EXPORT void DDS_Security_qeo_write_policy_version (void);

/* QEO specific call to let the qeo plugin know there is a new policy version */

/* Logging support functions: */

#define	DDS_TRACE_LEVEL		(0x00000001U << 0)
#define	DDS_DEBUG_LEVEL		(0x00000001U << 1)
#define	DDS_INFO_LEVEL		(0x00000001U << 2)
#define	DDS_NOTICE_LEVEL	(0x00000001U << 3)
#define	DDS_WARNING_LEVEL	(0x00000001U << 4)
#define	DDS_ERROR_LEVEL		(0x00000001U << 5)
#define	DDS_SEVERE_LEVEL	(0x00000001U << 6)
#define	DDS_FATAL_LEVEL		(0x00000001U << 7)

typedef struct {
	unsigned	log_level;
	const char	*log_file;
	int		distribute;
} DDS_LogOptions;

/* Configure logging mode and destination. */
DDS_EXPORT int DDS_Security_set_log_options (
	DDS_LogOptions *options
);

/* Security logging function. */
DDS_EXPORT void DDS_Security_log (
	unsigned   log_level,
	const char *string,
	const char *category
);

/* Logging enabling. */
DDS_EXPORT void DDS_Security_enable_logging (void);

/* Add a data tag to a DataWriter: */
DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_add_data_tag (
	DDS_DataWriter w,
	const char *data_tag
);

/* Get a data tag from a remote DataWriter: */
DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_data_tag (
	DDS_DataReader r,
	char *data_tag,
	DDS_InstanceHandle_t publication_handle
);

#endif /* DDS_NATIVE_SECURITY */

#ifdef  __cplusplus
}
#endif

#endif /* __dds_security_h_ */

