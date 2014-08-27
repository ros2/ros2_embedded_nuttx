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

/* sp_cert.h -- Security plugin -- Certificate functions. */

#ifndef __sp_cert_h_
#define __sp_cert_h_

#include "dds/dds_error.h"
#include <stddef.h>
#include "sp_auth.h"

typedef struct sp_sec_cert_st SP_SEC_CERT;

/* init the sec library plugin.*/

typedef DDS_ReturnCode_t (*SP_SEC_VAL_CERT) (unsigned char *certificate,
					     size_t        len,
					     unsigned char *ca,
					     size_t        ca_len);
/* Validate a certificate */


typedef DDS_ReturnCode_t (*SP_SEC_SIGN_SHA) (unsigned char *key,
					     size_t        key_len,
					     unsigned char *buffer,
					     size_t        length,
					     unsigned char *sign_buf,
					     size_t        *sign_len);

typedef DDS_ReturnCode_t (*SP_SEC_VERIFY_SHA) (unsigned char *key,
					       size_t        key_len,
					       unsigned char *buffer,
					       size_t        length,
					       unsigned char *sign_buf,
					       size_t        sign_len,
					       unsigned      *result);

typedef DDS_ReturnCode_t (*SP_SEC_GET_CERT_X509) (unsigned char *pem,
						  size_t        len,
						  void          **data);

typedef DDS_ReturnCode_t (*SP_SEC_GET_CA_CERT_X509) (unsigned char *pem,
						     size_t        len,
						     void          **data);

typedef DDS_ReturnCode_t (*SP_SEC_GET_PRIV_KEY_X509) (unsigned char *pem,
						      size_t        len,
						      void          **data);

typedef DDS_ReturnCode_t (*SP_SEC_GET_NB_CA_CERT) (unsigned char *pem,
						   size_t        len,
						   int           *nb);

typedef DDS_ReturnCode_t (*SP_SEC_ENCRYPT_PUBLIC) (unsigned char *cert,
						   size_t        cert_len,
						   unsigned char *buffer,
						   size_t        buf_len,
						   unsigned char *encrypted,
						   size_t        *enc_len);

typedef DDS_ReturnCode_t (*SP_SEC_DECRYPT_PRIVATE) (unsigned char *key,
						    size_t        key_len,
						    unsigned char *buffer,
						    size_t        buf_len,
						    unsigned char *decrypted,
						    size_t        *dec_len);

typedef DDS_ReturnCode_t (*SP_SEC_GET_COMMON_NAME) (unsigned char *cert,
						    size_t        cert_len,
						    unsigned char *name,
						    size_t        *name_len);

struct sp_sec_cert_st {
	SP_SEC_VAL_CERT          validate_cert; /* Validate certificate. */
	SP_SEC_SIGN_SHA          sign_sha256;
	SP_SEC_VERIFY_SHA        verify_sha256;
	SP_SEC_GET_CERT_X509     get_cert_x509;
	SP_SEC_GET_CA_CERT_X509  get_ca_cert_x509;
	SP_SEC_GET_PRIV_KEY_X509 get_priv_key_x509;
	SP_SEC_GET_NB_CA_CERT    get_nb_ca_cert;
	SP_SEC_ENCRYPT_PUBLIC    encrypt_public;
	SP_SEC_DECRYPT_PRIVATE   decrypt_private;
	SP_SEC_GET_COMMON_NAME   get_common_name;
};

void sp_set_sp_sec_cert (SP_SEC_CERT *func);

DDS_ReturnCode_t sp_validate_certificate (IdentityHandle_t certificate_id,
					  IdentityHandle_t ca_id);

/* Validate the certificate */

DDS_ReturnCode_t sp_sign_sha256 (IdentityHandle_t id,
				 unsigned char    *buffer,
				 size_t           length,
				 unsigned char    *sign_buf,
				 size_t           *sign_len);

DDS_ReturnCode_t sp_verify_signature_sha256 (IdentityHandle_t id,
					     unsigned char    *buffer,
					     size_t           length,
					     unsigned char    *sign_buf,
					     size_t           sign_len,
					     unsigned         *result);

DDS_ReturnCode_t sp_encrypt_public (IdentityHandle_t remote,
				    unsigned char    *buffer,
				    size_t           length,
				    unsigned char    *encrypted,
				    size_t           *enc_len);

DDS_ReturnCode_t sp_decrypt_private (IdentityHandle_t local,
				     unsigned char    *buffer,
				     size_t           length,
				     unsigned char    *decrypted,
				     size_t           *dec_len);

DDS_ReturnCode_t sp_get_certificate_pem (IdentityHandle_t id_handle,
					 char             *buf,
					 size_t           *length);

DDS_ReturnCode_t sp_get_certificate_x509 (void             **certificate,
					  IdentityHandle_t id);

DDS_ReturnCode_t sp_get_CA_certificate_list_x509 (void             **CAcertificates,
						  IdentityHandle_t id);

DDS_ReturnCode_t sp_get_private_key_x509 (void             **pkey,
					  IdentityHandle_t id);

DDS_ReturnCode_t sp_get_nb_of_CA_cert (int              *nb,
				       IdentityHandle_t id);

DDS_ReturnCode_t sp_encrypt_pub_key (IdentityHandle_t id,
				     unsigned char    *buffer,
				     size_t           buf_len,
				     unsigned char    *encrypted,
				     size_t           *enc_len);

DDS_ReturnCode_t sp_decrypt_private_key (IdentityHandle_t id,
					 unsigned char    *buffer,
					 size_t           buf_len,
					 unsigned char    *decrypted,
					 size_t           *dec_len);

DDS_ReturnCode_t sp_get_common_name (unsigned char *cert,
				     size_t        cert_len,
				     unsigned char *name,
				     size_t        *name_len);


#endif /* !__sp_cert_h_ */

