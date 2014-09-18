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

#include "sp_auth.h"
/*#include "sp_access.h"*/
/*#include "sp_aux.h"*/
#include "sp_cert.h"
#include "sp_crypto.h"
#include "sp_sys_cert.h"
#include "sp_sys_crypto.h"
#include "sp_sys_cert_none.h"
#include "sp_sys_crypto_none.h"
#include "sp_access.h"
#include "sp_sys.h"
#include "sp_xml.h"
#include "dds/dds_plugin.h"
#include "dds/dds_security.h"
#include "dds/dds_error.h"
#include "error.h"
#include "log.h"

static DDS_ReturnCode_t sp_policy_init (void)
{
	/* Add the openssl plugin for certificates */
#ifdef DDS_SP_SIMULATE_SIGN
	log_printf (SEC_ID, 0, "SEC: Define DDS_SP_SIMULATE_SIGN is set. Certificate validation and signing will be bypassed\r\n");
	sp_sec_cert_none_add ();
	sp_sec_crypto_none_add ();
#else
	sp_sec_cert_add ();
	/* Add the openssl plugin for crypto */
	sp_sec_crypto_add ();
#endif

	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_auth_req (DDS_AuthenticationRequest code,
				     DDS_SecurityReqData       *data)
{
	DDS_ReturnCode_t	rc;

	switch (code) {
	case DDS_VALIDATE_LOCAL_ID:
		rc = sp_validate_local_id (data->name,
					   data->data,
					   data->kdata,
					   data->klength,
					   &data->handle,
					   &data->action);
		break;
	case DDS_VALIDATE_REMOTE_ID:
		rc = sp_validate_remote_id (data->tag, 
					    data->data, 
					    data->length,
					    data->kdata,
					    data->klength,
					    &data->action, 
					    &data->handle);
					
		break;
	case DDS_GET_ID_NAME:
		rc = sp_auth_get_name (data->handle,
				       data->data,
				       data->length,
				       &data->rlength);
		break;
	case DDS_GET_ID_CREDENTIAL:
		rc = sp_auth_get_id_credential (data->handle, 
						data->data, 
						data->length,
						&data->rlength);
		break;
	case DDS_RELEASE_ID:
		rc = sp_release_identity (data->handle);
		break;
	case DDS_VERIFY_ID_CREDENTIAL:
		rc = sp_verify_remote_credentials (data->handle,
						   data->secure,
						   data->data,
						   data->length,
						   &data->action);
		break;
	default:
		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	}
	return (rc);
}

static DDS_ReturnCode_t sp_access_req (DDS_AccessControlRequest code,
				       DDS_SecurityReqData      *data)
{
	DDS_ReturnCode_t	rc;

	switch (code) {
	case DDS_VALIDATE_LOCAL_PERM:
		/* if action is set, take the lock */
		if (data->action) {
			lock_take (sp_lock);
		}
		rc = sp_validate_local_perm (data->domain_id,
					     &data->handle);
		if (data->action) {
			lock_release (sp_lock);
		}
		break;
	case DDS_VALIDATE_REMOTE_PERM:
		lock_take (sp_lock);
		rc = sp_validate_peer_perm (data->secure,
					    data->handle,
					    data->name,
					    data->data,
					    data->length,
					    &data->handle);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_CREATE_PARTICIPANT:
		lock_take (sp_lock);
		rc = sp_check_create_participant (data->handle,
						  data->data,
						  &data->secure);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_CREATE_WRITER:
		lock_take (sp_lock);
		rc = sp_check_create_writer (data->handle,
					     data->name,
					     data->data,
					     data->rdata,
					     &data->secure);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_CREATE_READER:
		lock_take (sp_lock);
		rc = sp_check_create_reader (data->handle,
					     data->name,
					     data->data,
					     data->rdata,
					     &data->secure);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_CREATE_TOPIC:
		lock_take (sp_lock);
		rc = sp_check_create_topic (data->handle,
					    data->name,
					    data->data);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_LOCAL_REGISTER:

		/* ... TBC ... */

		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	case DDS_CHECK_LOCAL_DISPOSE:

		/* ... TBC ... */

		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	case DDS_CHECK_REMOTE_PARTICIPANT:
		lock_take (sp_lock);
		rc = sp_check_peer_participant (data->handle,
						data->data);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_REMOTE_READER:
		lock_take (sp_lock);
		rc = sp_check_peer_reader (data->handle,
					   data->name,
					   data->data);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_REMOTE_WRITER:
		lock_take (sp_lock);
		rc = sp_check_peer_writer (data->handle,
					   data->name,
					   data->data);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_REMOTE_TOPIC:
		lock_take (sp_lock);
		rc = sp_check_peer_topic (data->handle,
					  data->name,
					  data->data);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_REMOTE_REGISTER:

		/* ... TBC ... */

		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	case DDS_CHECK_REMOTE_DISPOSE:

		/* ... TBC ... */

		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	case DDS_GET_PERM_CRED:
		rc = sp_access_get_permissions (data->handle,
						data->data,
						data->length,
						&data->rlength,
						(uint64_t *) data->kdata);
		break;
	case DDS_SET_PERM_CRED:
		rc = sp_access_set_permissions (data->handle,
						data->data,
						data->length);
		break;
	case DDS_CHECK_LOCAL_WRITER_MATCH:
		lock_take (sp_lock);
		rc = sp_access_check_local_datawriter_match (data->handle,
							     data->domain_id,
							     data->name,
							     data->tag,
							     data->data,
							     data->rdata);
		lock_release (sp_lock);
		break;
	case DDS_CHECK_LOCAL_READER_MATCH:
		lock_take (sp_lock);
		rc = sp_access_check_local_datareader_match (data->handle,
							     data->domain_id,
							     data->name,
							     data->name,
							     data->data,
							     data->rdata);
		lock_release (sp_lock);
		break;
	default:
		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	}
	return (rc);
}

static DDS_ReturnCode_t sp_aux_req (DDS_SecuritySupportRequest code,
				    DDS_SecurityReqData        *data)
{
	DDS_ReturnCode_t	r = DDS_RETCODE_UNSUPPORTED;

	lock_take (sp_lock);

	switch (code) {
		case DDS_SET_LIBRARY_INIT:
			sp_sys_set_library_init (data->secure);
			break;
		case DDS_SET_LIBRARY_LOCK:
			sp_sys_set_library_lock ();
			break;
		case DDS_UNSET_LIBRARY_LOCK:
			sp_sys_unset_library_lock ();
			break;
		case DDS_GET_DOMAIN_SEC_CAPS:
			r = sp_get_domain_sec_caps (data->domain_id, &data->secure);
			break;
		case DDS_ACCEPT_SSL_CX:
			r = sp_validate_ssl_connection (data->handle, data->data, data->rdata,
							&data->action);
			break;
		default:
			fatal_printf ("Crypto: function not supported!");
			r = DDS_RETCODE_UNSUPPORTED;
	}
	lock_release (sp_lock);
	return (r);
}

static DDS_ReturnCode_t sp_aac_plugin (DDS_SecurityClass   c,
				       int                 code,
				       DDS_SecurityReqData *data)
{
	if (c == DDS_SC_INIT) {
		sp_policy_init ();
		return (DDS_RETCODE_OK);
	}
	else if (c == DDS_SC_AUTH) {
		if (code < 0 || code > DDS_RELEASE_ID)
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			return (sp_auth_req ((DDS_AuthenticationRequest) code, data));
	}
	else if (c == DDS_SC_ACCESS) {
		if (code < 0 || code > DDS_RELEASE_PERM)
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			return (sp_access_req ((DDS_AccessControlRequest) code, data));
	}
	else if (c == DDS_SC_AUX) {
		if (code < 0 || code > DDS_ACCEPT_SSL_CX)
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			return (sp_aux_req ((DDS_SecuritySupportRequest) code, data));
	}
	else
		return (DDS_RETCODE_BAD_PARAMETER);
}

/* sp_set_policy -- Set security policy to this code. */

DDS_ReturnCode_t sp_set_policy (void)
{
	return (DDS_Security_set_policy (DDS_SECURITY_LOCAL, sp_aac_plugin));
}

static DDS_ReturnCode_t sp_cert_req (DDS_CertificateRequest code,
				     DDS_SecurityReqData    *data)
{
	DDS_ReturnCode_t	rc;

	switch (code) {
	case DDS_VALIDATE_CERT:
		rc = sp_verify_remote_credentials (data->handle,
						   (unsigned) data->secure,
						   data->data,
						   data->length,
						   &data->action);
		break;
	case DDS_SIGN_SHA256:
		rc = sp_sign_sha256 (data->handle,
				     data->data,
				     data->length,
				     data->rdata,
				     &data->rlength);
		break;
	case DDS_VERIFY_SHA256:
		rc = sp_verify_signature_sha256 (data->handle,
						 data->data,
						 data->length,
						 data->rdata,
						 data->rlength,
						 &data->secure);
		break;
	case DDS_GET_CERT_X509:
		rc = sp_get_certificate_x509 (data->data,
					      data->handle);
		break;
	case DDS_GET_CA_CERT_X509:
		rc = sp_get_CA_certificate_list_x509 (data->data,
						      data->handle);
		break;
	case DDS_GET_PRIVATE_KEY_X509:
		rc = sp_get_private_key_x509 (data->data,
					      data->handle);
		break;
	case DDS_GET_CERT_PEM:
		rc = sp_get_certificate_pem (data->handle,
					     data->data,
					     &data->rlength);
		break;
	case DDS_GET_NB_CA_CERT:
		rc = sp_get_nb_of_CA_cert ((int *) data->data,
					   data->handle);
		break;
	case DDS_ENCRYPT_PUBLIC:
		rc = sp_encrypt_pub_key (data->handle,
					 data->data,
					 data->length,
					 data->rdata,
					 &data->rlength);
		break;
	case DDS_DECRYPT_PRIVATE:
		rc = sp_decrypt_private_key (data->handle,
					     data->data,
					     data->length,
					     data->rdata,
					     &data->rlength);
		break;
#if 0
	case DDS_VERIFY_SIGNATURE:
		rc = sp_verify_signature (data->name);
		break;
	case DDS_CHECK_SUBJECT_NAME:
		rc = sp_check_subject_name (data->name,
					    data->handle);
#endif
		break;
	default:
		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	}
	return (rc);
}

static DDS_ReturnCode_t sp_crypto_req (DDS_CryptoRequest   code,
				       DDS_SecurityReqData *data)
{
	DDS_ReturnCode_t	rc;
	uint32_t                counter;

	switch (code) {
	case DDS_GEN_RANDOM: 
		rc = sp_generate_random (data->length, data->rdata);
		break;
	case DDS_SHA1:
		switch (data->action) {
			case DDS_CRYPT_FULL:
				data->rlength = 20;
				rc = sp_hash_sha1 (data->data,
						   data->length,
						   data->rdata,
						   &data->rlength);
				break;
			case DDS_CRYPT_BEGIN:
				rc = sp_sha_begin (data->data,
						   data->length,
						   SP_HASH_SHA1);
				break;
			case DDS_CRYPT_UPDATE:
				rc = sp_sha_continue (data->data,
						      data->length,
						      SP_HASH_SHA1);
				break;
			case DDS_CRYPT_END:
				rc = sp_sha_continue (data->data,
						      data->length,
						      SP_HASH_SHA1);
				if (!rc)
					rc = sp_sha_end (data->rdata,
							 SP_HASH_SHA1);
				break;
			default:
				rc = DDS_RETCODE_UNSUPPORTED;
				break;
		}
		break;
	case DDS_SHA256:
		switch (data->action) {
			case DDS_CRYPT_FULL:
				data->rlength = 32;
				rc = sp_hash_sha256 (data->data,
						     data->length,
						     data->rdata,
						     &data->rlength);
				break;
			case DDS_CRYPT_BEGIN:
				rc = sp_sha_begin (data->data,
						   data->length,
						   SP_HASH_SHA256);
				break;
			case DDS_CRYPT_UPDATE:
				rc = sp_sha_continue (data->data,
						      data->length,
						      SP_HASH_SHA256);
				break;
			case DDS_CRYPT_END:
				rc = sp_sha_continue (data->data,
						      data->length,
						      SP_HASH_SHA256);
				if (!rc)
					rc = sp_sha_end (data->rdata,
							 SP_HASH_SHA1);
				break;
			default:
				rc = DDS_RETCODE_UNSUPPORTED;
				break;
		}
		break;
	case DDS_AES128_CTR:
		switch (data->action) {
			case DDS_CRYPT_FULL:
				counter = data->secure;
				rc = sp_aes_ctr (data->kdata,
						 data->klength << 3,
						 (unsigned char *) data->tag,
						 &counter,
						 data->data,
						 data->length,
						 data->rdata);
				data->secure = counter;
				break;
			case DDS_CRYPT_BEGIN:
				rc = sp_aes_ctr_begin ((unsigned char *) data->tag,
						       data->kdata,
						       data->klength << 3,
						       data->secure);
				if (rc)
					return (rc);

				/*FALLTHRU*/

			case DDS_CRYPT_UPDATE:
				rc = sp_aes_ctr_update (data->data,
							data->length,
							data->rdata);
				break;
			case DDS_CRYPT_END:
				rc = sp_aes_ctr_update (data->data,
							data->length,
							data->rdata);
				if (rc)
					return (rc);

				rc = sp_aes_ctr_end (&counter);
				data->secure = counter;
				break;
			default:
				rc = DDS_RETCODE_UNSUPPORTED;
				break;
		}
		break;
	case DDS_AES256_CTR:
		switch (data->action) {
			case DDS_CRYPT_FULL:
				counter = data->secure;
				rc = sp_aes_ctr (data->kdata,
						 data->klength << 3,
						 (unsigned char *) data->tag,
						 &counter,
						 data->data,
						 data->length,
						 data->rdata);
				data->secure = counter;
				break;
			case DDS_CRYPT_BEGIN:
				rc = sp_aes_ctr_begin ((unsigned char *) data->tag,
						       data->kdata,
						       data->klength << 3,
						       data->secure);
				if (rc)
					return (rc);

				/*FALLTHRU*/

			case DDS_CRYPT_UPDATE:
				rc = sp_aes_ctr_update (data->data,
							data->length,
							data->rdata);
				break;
			case DDS_CRYPT_END:
				rc = sp_aes_ctr_update (data->data,
							data->length,
							data->rdata);
				if (rc)
					return (rc);

				rc = sp_aes_ctr_end (&counter);
				data->secure = counter;
				break;
			default:
				rc = DDS_RETCODE_UNSUPPORTED;
				break;
		}
		break;
	case DDS_HMAC_SHA1:
		switch (data->action) {
			case DDS_CRYPT_FULL:
				data->rlength = 20;
				rc = sp_hmac_sha (data->data,
						  data->length,
						  (unsigned char *) data->kdata,
						  (size_t) data->klength,
						  (unsigned char *) data->rdata,
						  &data->rlength,
						  SP_HASH_SHA1);
				break;
			case DDS_CRYPT_BEGIN:
				rc = sp_hmac_sha_begin ((unsigned char *) data->kdata,
							(size_t) data->klength,
							SP_HASH_SHA1);
				if (rc)
					return (rc);

				/*FALLTHRU*/
			case DDS_CRYPT_UPDATE:
				rc = sp_hmac_sha_continue (data->data,
							   data->length,
							   SP_HASH_SHA1);
				break;
			case DDS_CRYPT_END:
				rc = sp_hmac_sha_continue (data->data,
							   data->length,
							   SP_HASH_SHA1);
				if (rc)
					return (rc);

				data->rlength = 20;
				rc = sp_hmac_sha_end (data->rdata,
						      &data->rlength,
						      SP_HASH_SHA1);
				break;
			default:
				rc = DDS_RETCODE_UNSUPPORTED;
				break;
		}
		break;
	case DDS_HMAC_SHA256:
		switch (data->action) {
			case DDS_CRYPT_FULL:
				data->rlength = 32;
				rc = sp_hmac_sha (data->data,
						  data->length,
						  (unsigned char *) data->kdata,
						  (size_t) data->klength,
						  (unsigned char *) data->rdata,
						  &data->rlength,
						  SP_HASH_SHA256);
				break;
			case DDS_CRYPT_BEGIN:
				rc = sp_hmac_sha_begin ((unsigned char *) data->kdata,
							(size_t) data->klength,
							SP_HASH_SHA256);
				if (rc)
					return (rc);

				/*FALLTHRU*/
			case DDS_CRYPT_UPDATE:
				rc = sp_hmac_sha_continue (data->data,
							   data->length,
							   SP_HASH_SHA256);
				break;
			case DDS_CRYPT_END:
				rc = sp_hmac_sha_continue (data->data,
							   data->length,
							   SP_HASH_SHA256);
				if (rc)
					return (rc);

				data->rlength = 32;
				rc = sp_hmac_sha_end (data->rdata,
						      &data->rlength,
						      SP_HASH_SHA256);
				break;
			default:
				rc = DDS_RETCODE_UNSUPPORTED;
				break;
		}
		break;
	default:
		rc = DDS_RETCODE_UNSUPPORTED;
		break;
	}
	return (rc);
}

static DDS_ReturnCode_t sp_crypto_init (void)
{
	return (sp_crypto_initialize ());
}

static DDS_ReturnCode_t sp_crypto_plugin (DDS_SecurityClass   c,
					  int                 code,
					  DDS_SecurityReqData *data)
{
	if (c == DDS_SC_INIT) {
		return (sp_crypto_init ());
	}
	else if (c == DDS_SC_CERTS) {
		if (code < 0 || code > DDS_VERIFY_SHA256)
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			return (sp_cert_req ((DDS_CertificateRequest) code, data));
	}
	else if (c == DDS_SC_CRYPTO) {
		if (code < 0 || code > DDS_HMAC_SHA256)
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			return (sp_crypto_req ((DDS_CertificateRequest) code, data));
	}
	else
		return (DDS_RETCODE_BAD_PARAMETER);
}

DDS_ReturnCode_t sp_set_crypto (void)
{
	return (DDS_Security_set_crypto (sp_crypto_plugin));
}

void init_engine_fs (void)
{
	/* Legacy --> should be removed asap */
}			

DDS_ReturnCode_t DDS_SP_init_engine (const char *engine_id, 
				     void* (*engine_constructor_callback)(void)) 
{
	ARG_NOT_USED (engine_id)
	ARG_NOT_USED (engine_constructor_callback)

	/* Legacy --> should be removed asap */

	return (DDS_RETCODE_OK);
}

void DDS_SP_engine_cleanup (void)
{
	/* Legacy --> should be removed asap */
}

DDS_ReturnCode_t DDS_SP_set_policy (void)
{
	DDS_ReturnCode_t r;

	r = sp_set_policy ();
	if (r)
		return (r);

	r = sp_set_crypto ();
	return (r);
}

