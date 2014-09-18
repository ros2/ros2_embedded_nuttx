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


#include "sp_cert.h"
#include "sp_cred.h"
#include "nsecplug/nsecplug.h"
#include "log.h"
#include "error.h"

#define SP_CERT_LOG

SP_SEC_CERT *sp_sec_cert = NULL;

void sp_set_sp_sec_cert (SP_SEC_CERT *func) 
{
	sp_sec_cert = func;
}

/* Handle is a sec lib specific handle to allow for faster processing */

DDS_ReturnCode_t sp_validate_certificate (IdentityHandle_t certificate_id,
					  IdentityHandle_t ca_id)
{
	unsigned char          *cert1, *cert2;
	size_t                 cert1_len, cert2_len;
	DDS_ReturnCode_t       ret;

#ifdef SP_CERT_LOG
	log_printf (SEC_ID, 0, "SP_CERT: Validating certificate [%d] [%d]\r\n", certificate_id, ca_id);
#endif
	/* Get the correct certificate stuff from DDS_Credentials */

	if (!(cert1 = sp_get_cert (certificate_id, &cert1_len))) {
#ifdef SP_CERT_LOG
	log_printf (SEC_ID, 0, "SP_CERT: Could not get certificate for [%d]\r\n", certificate_id);
#endif
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!(cert2 = sp_get_cert (ca_id, &cert2_len))) {
#ifdef SP_CERT_LOG
	log_printf (SEC_ID, 0, "SP_CERT: Could not get certificate for [%d]\r\n", ca_id);
#endif
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (sp_sec_cert) {
		ret = sp_sec_cert->validate_cert (cert1, cert1_len,
					    cert2, cert2_len);
#ifdef SP_CERT_LOG
		log_printf (SEC_ID, 0, "SP_CERT: Certificate validation of [%d] against ca [%d] returned %d\r\n", certificate_id, ca_id, ret);
#endif
	}
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_sign_sha256 (IdentityHandle_t id,
				 unsigned char    *buffer,
				 size_t           length,
				 unsigned char    *sign_buf,
				 size_t           *sign_len)
{
	unsigned char    *key;
	size_t           key_len;
	DDS_ReturnCode_t ret;

	if (!(key = sp_get_key (id, &key_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->sign_sha256 (key,
						key_len,
						buffer,
						length,
						sign_buf,
						sign_len);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_verify_signature_sha256 (IdentityHandle_t id,
					     unsigned char    *buffer,
					     size_t           length,
					     unsigned char    *sign_buf,
					     size_t           sign_len,
					     unsigned         *result)
{
	/* We must pass the certificate along, to get the public key from it
	   to verify the signature. */
	
	unsigned char    *cert;
	size_t           cert_len;
	DDS_ReturnCode_t ret;

	if (!(cert = sp_get_cert (id, &cert_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->verify_sha256 (cert,
						  cert_len,
						  buffer,
						  length,
						  sign_buf,
						  sign_len,
						  result);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_get_certificate_pem (IdentityHandle_t id,
					 char             *buf,
					 size_t           *length)
{
	if (!(buf = (char *) sp_get_cert (id, length)))
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_get_certificate_x509 (void             **certificate,
					  IdentityHandle_t id)
{
	unsigned char    *cert;
	size_t           cert_len;
	DDS_ReturnCode_t ret;

	if (!(cert = sp_get_cert (id, &cert_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->get_cert_x509 (cert,
						  cert_len,
						  certificate);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_get_CA_certificate_list_x509 (void             **CAcertificates,
						  IdentityHandle_t id)
{
	unsigned char    *cert;
	size_t           cert_len;
	DDS_ReturnCode_t ret;

	if (!(cert = sp_get_cert (id, &cert_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->get_ca_cert_x509 (cert,
						     cert_len,
						     CAcertificates);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_get_private_key_x509 (void             **pkey,
					  IdentityHandle_t id)
{
	unsigned char    *key;
	size_t           key_len;
	DDS_ReturnCode_t ret;

	if (!(key = sp_get_key (id, &key_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->get_priv_key_x509 (key,
						      key_len,
						      pkey);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_get_nb_of_CA_cert (int              *nb,
				       IdentityHandle_t id)
{
	unsigned char    *cert;
	size_t           cert_len;
	DDS_ReturnCode_t ret;

	if (!(cert = sp_get_cert (id, &cert_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->get_nb_ca_cert (cert, cert_len, nb);
	else
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;

	return (ret);
}

DDS_ReturnCode_t sp_encrypt_public (IdentityHandle_t remote,
				    unsigned char    *buffer,
				    size_t           length,
				    unsigned char    *encrypted,
				    size_t           *enc_len)
{
	unsigned char    *cert;
	size_t           cert_len;
	DDS_ReturnCode_t ret;
	
	if (!(cert = sp_get_cert (remote, &cert_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->encrypt_public (cert,
						   cert_len,
						   buffer,
						   length,
						   encrypted,
						   enc_len);
	else
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
	return (ret);
}

DDS_ReturnCode_t sp_decrypt_private (IdentityHandle_t local,
				     unsigned char    *buffer,
				     size_t           length,
				     unsigned char    *decrypted,
				     size_t           *dec_len)
{
	unsigned char    *key;
	size_t           key_len;
	DDS_ReturnCode_t ret;

	if (!(key = sp_get_key (local, &key_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->decrypt_private (key,
						    key_len,
						    buffer,
						    length,
						    decrypted,
						    dec_len);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_encrypt_pub_key (IdentityHandle_t id,
				     unsigned char    *buffer,
				     size_t           buf_len,
				     unsigned char    *encrypted,
				     size_t           *enc_len)
{
	unsigned char    *cert;
	size_t           cert_len;
	DDS_ReturnCode_t ret;

	if (!(cert = sp_get_cert (id, &cert_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->encrypt_public (cert,
						   cert_len,
						   buffer,
						   buf_len,
						   encrypted,
						   enc_len);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_decrypt_private_key (IdentityHandle_t id,
					 unsigned char    *buffer,
					 size_t           buf_len,
					 unsigned char    *decrypted,
					 size_t           *dec_len)
{
	unsigned char    *key;
	size_t           key_len;
	DDS_ReturnCode_t ret;

	if (!(key = sp_get_key (id, &key_len)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (sp_sec_cert)
		ret = sp_sec_cert->decrypt_private (key,
						    key_len,
						    buffer,
						    buf_len,
						    decrypted,
						    dec_len);
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}

DDS_ReturnCode_t sp_get_common_name (unsigned char *cert,
				     size_t        cert_len,
				     unsigned char *name,
				     size_t        *name_len)
{
	DDS_ReturnCode_t ret;

	if (sp_sec_cert) {
		ret = sp_sec_cert->get_common_name (cert,
						    cert_len,
						    name,
						    name_len);
#ifdef SP_CERT_LOG
		log_printf (SEC_ID, 0, "SP_CERT: getting the common name returned %d\r\n", ret);
#endif
	}
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (ret);
}
