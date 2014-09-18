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

#include "error.h"
#include "sp_sys_cert_none.h"
#include "sp_cert.h"
#include "sp_sys.h"
#include "dds/dds_error.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "openssl/x509_vfy.h"
#include "openssl/evp.h"
#include "openssl/err.h"

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
	fprintf (stdout, "\r\n======= First certificate ======\r\n");
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
	int i;
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
	for (i = 0; i < sk_X509_num (*cert_chain); i++) {
		fprintf (stdout, "\r\n======= Certificate chain ======\r\n");
		sp_log_X509(sk_X509_value (*cert_chain, i));
	}
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
	printf("OpenSSL error: code (%lu), file (%s), line (%d)\r\n", code, file, line);
	if (errData && (flags & ERR_TXT_STRING))
		printf ("error data: %s \r\n", errData);
	BIO_free (bio);
	return (DDS_RETCODE_ERROR);
}

static DDS_ReturnCode_t sp_sys_none_validate_certificate (unsigned char *certificate,
						     size_t        len,
						     unsigned char *ca,
						     size_t        ca_len)
{
	ARG_NOT_USED (certificate)
	ARG_NOT_USED (len)
	ARG_NOT_USED (ca)
	ARG_NOT_USED (ca_len)

	return (DDS_RETCODE_OK);
}

/*************************/
/* Signing and verifying */
/*************************/

/* Sign a buffer with sha256 */
/* The allocated size of the sign_buf, will max be the same as
   EVP_PKEY_size */

static DDS_ReturnCode_t sp_sys_none_sign_sha256 (unsigned char *key,
					    size_t        key_len,
					    unsigned char *buffer,
					    size_t        length,
					    unsigned char *sign_buf,
					    size_t        *sign_len)
{
	ARG_NOT_USED (key)
	ARG_NOT_USED (key_len)

	if (length <= *sign_len)
		memcpy (sign_buf, buffer, length);
	else
		memcpy (sign_buf, buffer, *sign_len);
	
	return (DDS_RETCODE_OK);
}

/* Verify sha256 signature */

static DDS_ReturnCode_t sp_sys_none_verify_signature_sha256 (unsigned char *cert,
							     size_t        cert_len,
							     unsigned char *buffer,
							     size_t        length,
							     unsigned char *sign_buf,
							     size_t        sign_len,
							     unsigned      *result)
{
	ARG_NOT_USED (cert)
	ARG_NOT_USED (cert_len)

	if (length <= sign_len) {
		if (!memcmp (sign_buf, buffer, length)) {
			*result = 1;
			return (DDS_RETCODE_OK);
		}
	}
	else {
		if (!memcmp (sign_buf, buffer, sign_len)) {
			*result = 1;
			return (DDS_RETCODE_OK);
		}
	}
	*result = 0;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_none_get_x509_certificate (unsigned char *pem,
						     size_t        length,
						     void          **cert)
{
	return (get_first_certificate (pem, length, (X509 **) cert));
}

static DDS_ReturnCode_t sp_sys_none_get_x509_ca_certificate (unsigned char *pem,
							size_t        length,
							void          **ca)
{
	return (get_certificate_chain (pem, length, (STACK_OF(X509) **) ca));
}

static DDS_ReturnCode_t sp_sys_none_get_private_key_x509 (unsigned char *pem,
						     size_t        length,
						     void          **pkey)
{
	return (get_private_key (pem, length, (EVP_PKEY **) pkey));
}

static DDS_ReturnCode_t sp_sys_none_get_nb_of_ca_cert (unsigned char *pem,
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
	ARG_NOT_USED (pkey)

	if (length <= *enc_len)
		memcpy (encrypted, buffer, length);
	else
		memcpy (encrypted, buffer, *enc_len);

	return (DDS_RETCODE_OK);
}

/* Decrypt the buffer with the private key */

static DDS_ReturnCode_t decrypt_private_key (EVP_PKEY      *pkey,
					     unsigned char *buffer,
					     size_t        length,
					     unsigned char *decrypted,
					     size_t        *dec_len)
{
	ARG_NOT_USED (pkey)

	if (length <= *dec_len)
		memcpy (decrypted, buffer, length);
	else
		memcpy (decrypted, buffer, *dec_len);

	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_none_encrypt_public (unsigned char *cert,
					       size_t        cert_len,
					       unsigned char *buffer,
					       size_t        buf_len,
					       unsigned char *encrypted,
					       size_t        *enc_len)
{
	EVP_PKEY *pkey = NULL;

	ARG_NOT_USED (cert)
	ARG_NOT_USED (cert_len)

	return (encrypt_pub_key (pkey, buffer, buf_len, encrypted, enc_len));
}

static DDS_ReturnCode_t sp_sys_none_decrypt_private (unsigned char *key,
						size_t        key_len,
						unsigned char *buffer,
						size_t        buf_len,
						unsigned char *decrypted,
						size_t        *dec_len)
{
	EVP_PKEY *pkey = NULL;

	ARG_NOT_USED (key)
	ARG_NOT_USED (key_len)

	return (decrypt_private_key (pkey, buffer, buf_len, decrypted, dec_len));
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

	X509_NAME_get_text_by_NID (X509_get_subject_name (certificate), NID_commonName, (char *) name, *name_len);
	return (DDS_RETCODE_OK);
}

static SP_SEC_CERT sp_sec_cert = {
	sp_sys_none_validate_certificate,
	sp_sys_none_sign_sha256,
	sp_sys_none_verify_signature_sha256,
	sp_sys_none_get_x509_certificate,
	sp_sys_none_get_x509_ca_certificate,
	sp_sys_none_get_private_key_x509,
	sp_sys_none_get_nb_of_ca_cert,
	sp_sys_none_encrypt_public,
	sp_sys_none_decrypt_private,
	sp_sys_get_common_name
};

void sp_sec_cert_none_add (void)
{
	sp_set_sp_sec_cert (&sp_sec_cert);
}
