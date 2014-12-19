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

/* sp_sys_cert.c -- Openssl specific implementation of crypto functions */

#include "error.h"
#include "log.h"
#include "sp_sys_cert.h"
#include "sp_sys.h"
#include "sp_cert.h"
#include "dds/dds_error.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/rsa.h"

/* Get the first certificate from a char pointer, 
   this must be your own certificate */

static DDS_ReturnCode_t get_first_certificate (unsigned char *data,
					       size_t        length,
					       X509          **cert)
{
	BIO  *bio;
	X509 *ptr;

	/* Create a new BIO with the certificate data */
 	if (!(bio = BIO_new_mem_buf ((void *) data, length))) 
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	/* read the first certificate */
	if (!(ptr = PEM_read_bio_X509 (bio, NULL, NULL, NULL)))
		goto error;

	BIO_free (bio);
	*cert = ptr;

#ifdef DDS_DEBUG
	sp_log_X509(ptr);
#endif

	return (DDS_RETCODE_OK);
 error:
	BIO_free (bio);
	return (DDS_RETCODE_ERROR);
}

/* Get all but the first certificate from a char pointer --> CA certificate chain */

static DDS_ReturnCode_t get_certificate_chain (unsigned char  *data,
					       size_t         length,
					       STACK_OF(X509) **cert_chain)
{
	BIO *bio;
	X509 *ptr;
	int counter = 0;
#ifdef DDS_DEBUG
#if 0
	int i;
#endif
#endif
	/* init certificate chain stack */
	if (!*cert_chain)
		*cert_chain = sk_X509_new_null ();

	/* Create a new BIO with the certificate data */
 	if (!(bio = BIO_new_mem_buf ((void *) data, length))) {
		sk_X509_free (*cert_chain);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	while ((ptr = PEM_read_bio_X509 (bio, NULL, NULL, NULL)) != NULL) {
		/*Discard the first because it's not a CA cert*/
		if (counter != 0)
			/* push cert to stack */
			sk_X509_push (*cert_chain, ptr);
		else
			X509_free (ptr);
		counter++;
	}

#ifdef DDS_DEBUG
#if 0
	for (i = 0; i < sk_X509_num (*cert_chain); i++) {
		sp_log_X509(sk_X509_value (*cert_chain, i));
	}
#endif
#endif
	BIO_free (bio);
	return (DDS_RETCODE_OK);
}

/* Get the private key from a char pointer */

static DDS_ReturnCode_t get_private_key (unsigned char *data,
					 size_t        length,
					 EVP_PKEY      **key)
{
	BIO *bio;
	char buffer [120];
	int line, flags;
	char *errData, *file;
	unsigned long code;
	EVP_PKEY *pkey = NULL;

	/* Create a new BIO with the certificate data */
 	if (!(bio = BIO_new_mem_buf ((void *) data, length)))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if ((pkey = PEM_read_bio_PrivateKey (bio, NULL, NULL, NULL)) == NULL)
		goto error;		

	*key = pkey;
	BIO_free (bio);
	return (DDS_RETCODE_OK);
 error:
	code = ERR_get_error_line_data ( (const char **) &file, &line, (const char **)  &errData, &flags);
	ERR_error_string(ERR_get_error(), buffer);
	log_printf (SEC_ID, 0, "SP_SYS_CERT: OpenSSL error: code (%lu), file (%s), line (%d)\r\n", code, file, line);
	if (errData && (flags & ERR_TXT_STRING))
		printf ("error data: %s \r\n", errData);
	BIO_free (bio);
	return (DDS_RETCODE_ERROR);
}

static DDS_ReturnCode_t get_public_key (X509     *cert,
					EVP_PKEY **key)
{
	if ((*key = X509_get_pubkey (cert)))
		return (DDS_RETCODE_OK);

	return (DDS_RETCODE_BAD_PARAMETER);
}

static int verify_callback (int ok, X509_STORE_CTX *st)
{
	X509	*cert;
	int	depth;
	char data [512];
	int	err = X509_STORE_CTX_get_error (st);
	depth = X509_STORE_CTX_get_error_depth (st);
	cert = X509_STORE_CTX_get_current_cert (st);

	ARG_NOT_USED(st)

	if (!ok) {
		ERR_load_crypto_strings ();
	
		if (cert)
			X509_NAME_oneline (X509_get_subject_name (cert), 
					   data, 
					   sizeof (data));
		else
			strcpy (data, "<Unknown>");
		err_printf ("SP_SYS_CERT: err %i @ depth %i for issuer: %s\r\n\t%s\r\n", 
			    err, depth, data, X509_verify_cert_error_string (err));
#ifdef NO_CERTIFICATE_TIME_VALIDITY_CHECK
		/* Exceptions */
		if (err == X509_V_ERR_CERT_NOT_YET_VALID) {
#ifdef LOG_CERT_CHAIN_DETAILS
			log_printf (SEC_ID, 0, "SP_SYS: Certificate verify callback. The certificate is not yet valid, but this is allowed. \r\n");
#endif
			ok = 1;
		}
		if (err == X509_V_ERR_CERT_HAS_EXPIRED) {
			ok = 1;
#ifdef LOG_CERT_CHAIN_DETAILS
			log_printf (SEC_ID, 0, "SP_SYS: Certificate verify callback. The certificate has expired, but this is allowed. \r\n");
#endif
		}
#endif
	}
	return (ok);
}

/* Verify certificate call */

static DDS_ReturnCode_t verify_certificate (X509* cert, 
					    STACK_OF(X509) *uchain, 
					    X509_STORE *store,
					    STACK_OF(X509) *tchain)
{
	int              ret, i;
	DDS_ReturnCode_t dds_ret;
	X509_STORE_CTX   *verify_ctx = NULL;

	/* TODO: add the crls */

	if (!(verify_ctx = X509_STORE_CTX_new ()))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	/* Add the untrusted cert and chain */
	if (!(X509_STORE_CTX_init (verify_ctx, store, cert, uchain))) {
		dds_ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}
	
	/* Add the trusted chain (CA) certificates */
	X509_STORE_CTX_trusted_stack (verify_ctx, tchain);

	/* Verify the untrusted certificate */
	if ((ret = X509_verify_cert (verify_ctx)) <= 0) {
		i = X509_STORE_CTX_get_error (verify_ctx);
		log_printf (SEC_ID, 0, "%s\r\n", X509_verify_cert_error_string (i));
		dds_ret = DDS_RETCODE_ERROR;
		goto cleanup;
	} else 
		log_printf (SEC_ID, 0, "Verfication result: %s\r\n", X509_verify_cert_error_string (verify_ctx->error));

#ifdef QEO_SECURITY
	/* Do a qeo specific realm check */
	if ((X509_cmp (sk_X509_value (uchain, 0), sk_X509_value (tchain, 0)))) {
		log_printf (SEC_ID, 0, "SP_SYS_CERT: not the same realm certificate --> Denied\r\n");
		dds_ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}
#endif

	dds_ret = DDS_RETCODE_OK;
 cleanup:
	X509_STORE_CTX_free (verify_ctx);
	return (dds_ret);
}

static X509_STORE *return_ca_store (void)
{
	X509_STORE *store;

	/* TODO: SUPER IMPORTANT (but should be done somewhere else) */
	OpenSSL_add_all_algorithms ();

	/* make a cert store with the verify cb */
	if (!(store = X509_STORE_new ()))
		return (NULL);
	
	X509_STORE_set_verify_cb_func (store, verify_callback);
	X509_STORE_set_depth (store, 5);

	return (store);
}

static DDS_ReturnCode_t sp_sys_validate_certificate (unsigned char *certificate,
						     size_t        len,
						     unsigned char *ca,
						     size_t        ca_len)
{
	X509 *cert;
	STACK_OF(X509) *chain = NULL;
	STACK_OF(X509) *ca_chain = NULL;
	X509_STORE *store;

	DDS_ReturnCode_t ret;

	/* get the certificate to be checked */
	if ((ret = get_first_certificate (certificate, len, &cert)))
		return (ret);

	/* get the untrusted certificate chain */
	if ((ret = get_certificate_chain (certificate, len, &chain))) {
		X509_free (cert);
		return (ret);
	}

	/* get the trusted certificate chain (CA) */
	if ((ret = get_certificate_chain (ca, ca_len, &ca_chain))) {
		X509_free (cert);
		sk_X509_pop_free (chain, X509_free);
		return (ret);
	}

	/* Get a trust store */
	if (!(store = return_ca_store ())) {
		X509_free (cert);
		sk_X509_pop_free (chain, X509_free);
		sk_X509_pop_free (ca_chain, X509_free);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* verify the untrusted certificate and chain against the trusted certificate chain */
	ret = verify_certificate (cert, chain, store, ca_chain);

	/* Cleanup */
	X509_free (cert);
	X509_STORE_free (store);
	sk_X509_pop_free (chain, X509_free);
	sk_X509_pop_free (ca_chain, X509_free);
	return (ret);
}

/*************************/
/* Signing and verifying */
/*************************/

/* Sign a buffer with sha256 */
/* The allocated size of the sign_buf, will max be the same as
   EVP_PKEY_size */

static DDS_ReturnCode_t sp_sys_sign_sha256 (unsigned char *key,
					    size_t        key_len,
					    unsigned char *buffer,
					    size_t        length,
					    unsigned char *sign_buf,
					    size_t        *sign_len)
{
	EVP_MD_CTX       *ctx;
	DDS_ReturnCode_t ret;
	EVP_PKEY         *pkey = NULL;
	unsigned         len;
	unsigned char    *tmp_sign_buf = NULL;

	if ((ret = get_private_key (key, key_len, &pkey)))
		return (ret);

	if (!(ctx = EVP_MD_CTX_create ())) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup_key;
	}
	if (!(EVP_SignInit (ctx, EVP_sha256 ()))) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}

	if (!(EVP_SignUpdate (ctx, buffer, length))) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}

	tmp_sign_buf = malloc (EVP_PKEY_size (pkey));
	if (!(EVP_SignFinal (ctx, tmp_sign_buf, &len, pkey))) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}

	if (len <= *sign_len) {
		memcpy (sign_buf, tmp_sign_buf, len);
		*sign_len = len;
	}

	
	ret = DDS_RETCODE_OK;
 cleanup:
	EVP_MD_CTX_destroy (ctx);
 cleanup_key:
	EVP_PKEY_free (pkey);
	if (tmp_sign_buf)
		free (tmp_sign_buf);
	return (ret);
}

/* Verify sha256 signature */

static DDS_ReturnCode_t sp_sys_verify_signature_sha256 (unsigned char *cert,
							size_t        cert_len,
							unsigned char *buffer,
							size_t        length,
							unsigned char *sign_buf,
							size_t        sign_len,
							unsigned      *result)
{
	EVP_MD_CTX       *ctx = NULL;
	DDS_ReturnCode_t ret;
	X509             *certificate = NULL;
	EVP_PKEY         *pkey = NULL;

	*result = 0;
	
	if ((ret = get_first_certificate (cert, cert_len, &certificate)))
		return (ret);

	if ((ret = get_public_key (certificate, &pkey)))
		goto cleanup_cert;

	if (!(ctx = EVP_MD_CTX_create ())) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup_key;
	}
	if (!(EVP_VerifyInit (ctx, EVP_sha256 ()))) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}
	if (!(EVP_VerifyUpdate (ctx, buffer, length))) {
		ret = DDS_RETCODE_ERROR;
		goto cleanup;
	}
	if (!(EVP_VerifyFinal (ctx, sign_buf, sign_len, pkey))) {
		/* Bad signature */
		ret = DDS_RETCODE_NOT_ALLOWED_BY_SEC;
		goto cleanup;
	}
	*result = 1;
	ret = DDS_RETCODE_OK;
 cleanup:
	EVP_MD_CTX_destroy (ctx);
 cleanup_key:
	EVP_PKEY_free (pkey);
 cleanup_cert:
	X509_free (certificate);
	return (ret);
}

static DDS_ReturnCode_t sp_sys_get_x509_certificate (unsigned char *pem,
						     size_t        length,
						     void          **cert)
{
	return (get_first_certificate (pem, length, (X509 **) cert));
}

static DDS_ReturnCode_t sp_sys_get_x509_ca_certificate (unsigned char *pem,
							size_t        length,
							void          **ca)
{
	return (get_certificate_chain (pem, length, (STACK_OF(X509) **) ca));
}

static DDS_ReturnCode_t sp_sys_get_private_key_x509 (unsigned char *pem,
						     size_t        length,
						     void          **pkey)
{
	return (get_private_key (pem, length, (EVP_PKEY **) pkey));
}

static DDS_ReturnCode_t sp_sys_get_nb_of_ca_cert (unsigned char *pem,
						  size_t        length,
						  int           *nb)
{
	BIO *bio;
	X509 *ptr;
	int counter = 0;

	/* Create a new BIO with the certificate data */
 	if (!(bio = BIO_new_mem_buf ((void *) pem, length)))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	while ((ptr = PEM_read_bio_X509 (bio, NULL, NULL, NULL)) != NULL) {
		X509_free (ptr);
		counter++;
	}

	*nb = counter;
	BIO_free (bio);
	return (DDS_RETCODE_OK);
}

/*****************************/
/* Encrypting and decrypting */
/*****************************/

/* encrypt a buffer with the public key */

static DDS_ReturnCode_t encrypt_pub_key (EVP_PKEY         *pkey,
					 unsigned char    *buffer,
					 size_t           length,
					 unsigned char    *encrypted,
					 size_t           *enc_len)
{
	char           err [512];
	RSA            *rsa;
	int            res;
	
	if (!buffer ||
	    !length ||
	    !encrypted)
		return (DDS_RETCODE_BAD_PARAMETER);

	rsa = EVP_PKEY_get1_RSA (pkey);
	if (RSA_size (rsa) > (int) *enc_len) {
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if ((res = RSA_public_encrypt (length, buffer, 
				       encrypted, rsa, 
				       RSA_PKCS1_OAEP_PADDING)) == -1)
		goto error;

	*enc_len = (size_t) res;
	RSA_free (rsa);
	return (DDS_RETCODE_OK);

 error:
	RSA_free (rsa);
	ERR_load_crypto_strings ();
	while ((strcmp(ERR_error_string (ERR_get_error(), err), 
		       "error:00000000:lib(0):func(0):reason(0)")))
		log_printf (SEC_ID, 0, "Error encrypting message: %s\r\n", err);
	return (DDS_RETCODE_ERROR);
}

/* Decrypt the buffer with the private key */

static DDS_ReturnCode_t decrypt_private_key (EVP_PKEY      *pkey,
					     unsigned char *buffer,
					     size_t        length,
					     unsigned char *decrypted,
					     size_t        *dec_len)
{
	char             err [512];
	int              res;
	RSA              *rsa;

	if (!buffer ||
	    !length ||
	    !pkey ||
	    !decrypted)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* THIS WORKS */
	rsa = EVP_PKEY_get1_RSA (pkey);
	if ((res = RSA_private_decrypt (length, buffer, 
					decrypted, 
					rsa,
					RSA_PKCS1_OAEP_PADDING)) == -1)
		goto error;

	*dec_len = (size_t) res;
	RSA_free (rsa);
	return (DDS_RETCODE_OK);

 error:
	RSA_free (rsa);
	ERR_load_crypto_strings ();
	while ((strcmp(ERR_error_string (ERR_get_error(), err), 
		       "error:00000000:lib(0):func(0):reason(0)")))
		log_printf (SEC_ID, 0, "Error decrypting message: %s\r\n", err);
	return (DDS_RETCODE_ERROR);
}

static DDS_ReturnCode_t sp_sys_encrypt_public (unsigned char *cert,
					       size_t        cert_len,
					       unsigned char *buffer,
					       size_t        buf_len,
					       unsigned char *encrypted,
					       size_t        *enc_len)
{
	EVP_PKEY *pkey;
	X509     *certificate;
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	if ((ret = get_first_certificate (cert, cert_len, &certificate)))
		return (ret);

	if ((ret = get_public_key (certificate, &pkey))) {
		X509_free (certificate);
		return (ret);
	}

	ret = encrypt_pub_key (pkey, buffer, buf_len, encrypted, enc_len);
	X509_free (certificate);
	EVP_PKEY_free (pkey);
	return (ret);
}

static DDS_ReturnCode_t sp_sys_decrypt_private (unsigned char *key,
						size_t        key_len,
						unsigned char *buffer,
						size_t        buf_len,
						unsigned char *decrypted,
						size_t        *dec_len)
{
	EVP_PKEY *pkey;
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	if ((ret = get_private_key (key, key_len, &pkey)))
		return (ret);

	ret = decrypt_private_key (pkey, buffer, buf_len, decrypted, dec_len);
	EVP_PKEY_free (pkey);
	return (ret);
}

static DDS_ReturnCode_t sp_sys_get_common_name (unsigned char *cert,
						size_t        cert_len,
						unsigned char *name,
						size_t        *name_len)
{
	X509 *certificate;
	DDS_ReturnCode_t ret;
	
	if ((ret = get_first_certificate (cert, cert_len, &certificate)))
		return (ret);

	X509_NAME_get_text_by_NID (X509_get_subject_name (certificate), 
				   NID_commonName, (char *) name, *name_len);
	X509_free (certificate);
	return (DDS_RETCODE_OK);
}

static SP_SEC_CERT sp_sec_cert = {
	sp_sys_validate_certificate,
	sp_sys_sign_sha256,
	sp_sys_verify_signature_sha256,
	sp_sys_get_x509_certificate,
	sp_sys_get_x509_ca_certificate,
	sp_sys_get_private_key_x509,
	sp_sys_get_nb_of_ca_cert,
	sp_sys_encrypt_public,
	sp_sys_decrypt_private,
	sp_sys_get_common_name
};

void sp_sec_cert_add (void)
{
	sp_set_sp_sec_cert (&sp_sec_cert);
}

