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


#include "sp_crypto.h"
#include "openssl/ossl_typ.h"
#include "openssl/bn.h"
#include "openssl/rand.h"
#include <stdint.h>

SP_SEC_CRYPTO *sp_sec_crypto = NULL;

void sp_set_sp_sec_crypto (SP_SEC_CRYPTO *func)
{
	sp_sec_crypto = func;
}

DDS_ReturnCode_t sp_crypto_initialize (void)
{
	if (sp_sec_crypto)
		return (sp_sec_crypto->init ());

	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_hash_sha1 (const unsigned char *buffer,
			       size_t              length,
			       unsigned char       *hash,
			       size_t              *hash_len)
{
	if (sp_sec_crypto)
		return (sp_sec_crypto->hash_sha1 (buffer,
						  length,
						  hash,
						  hash_len));
	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_hash_sha256 (const unsigned char *buffer,
				 size_t              length,
				 unsigned char       *hash,
				 size_t              *hash_len)
{
	if (sp_sec_crypto)
		return (sp_sec_crypto->hash_sha256 (buffer,
						    length,
						    hash,
						    hash_len));
	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_compare_hash (const unsigned char *hash1,
				  size_t              len1,
				  const unsigned char *hash2,
				  size_t              len2)
{
	if (sp_sec_crypto)
		return (sp_sec_crypto->compare_hash (hash1,
						     len1,
						     hash2,
						     len2));
	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_generate_random (size_t length, unsigned char *secret)
{
	if (sp_sec_crypto)
		return (sp_sec_crypto->generate_random (length, secret));
	
	return (DDS_RETCODE_UNSUPPORTED);
}

/* SHA functions.
   ---------------- */

DDS_ReturnCode_t sp_sha_begin (const void *data, size_t length, Hash_t type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->sha1_begin (data, length));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->sha256_begin (data, length));
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

/* SHA checksum calculation on a data area. This must be the first function
   to start the checksum. */

DDS_ReturnCode_t sp_sha_continue (const void *data, size_t length, Hash_t type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->sha1_continue (data, length));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->sha256_continue (data, length));	
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

/* SHA checksum calculation on a data area.  This function should be used for
   consecutively following data areas. */

DDS_ReturnCode_t sp_sha_end (void *checksum, Hash_t type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->sha1_end (checksum));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->sha256_end (checksum));
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_hmac_sha (const unsigned char *data,
			      size_t              data_len,
			      const unsigned char *key,
			      size_t              key_len,
			      unsigned char       *hash,
			      size_t              *hash_len,
			      Hash_t              type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->hmac_sha1 (data, data_len, key, key_len, hash, hash_len));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->hmac_sha256 (data, data_len, key, key_len, hash, hash_len));
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

/* Retrieve the resulting 20-byte SHA-1 checksum of the entire data area. */

DDS_ReturnCode_t sp_hmac_sha_begin (const unsigned char *key,
				    size_t              key_len,
				    Hash_t              type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->hmac_sha1_begin (key, key_len));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->hmac_sha256_begin (key, key_len));
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_hmac_sha_continue (const unsigned char *data,
				       size_t              length,
				       Hash_t              type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->hmac_sha1_continue (data, length));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->hmac_sha256_continue (data, length));
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t sp_hmac_sha_end (unsigned char *result, size_t *length, Hash_t type)
{
	if (sp_sec_crypto) {
		if (type == SP_HASH_SHA1)
			return (sp_sec_crypto->hmac_sha1_end (result, length));
		else if (type == SP_HASH_SHA256)
			return (sp_sec_crypto->hmac_sha256_end (result, length));
	}
	return (DDS_RETCODE_UNSUPPORTED);
}

/* AES-128/192/256 counter mode encryption/decryption.
   --------------------------------------------------- */

DDS_ReturnCode_t sp_aes_ctr_begin (const unsigned char IV [16],
				   const unsigned char *key,
				   size_t              key_bits,
				   uint32_t            counter)
{
	return (sp_sec_crypto->aes_begin (IV, key, key_bits, counter));
}

/* Create a new AES crypto handle for either encryption or decryption with the
   given key.  The key size should be either 128 or 256 bits as
   given in the key_bits parameter.  The IV argument should be
   properly initialized. If successful, a non-0 AES context handle
   is returned that can be used in the following function. */

DDS_ReturnCode_t sp_aes_ctr_update (const unsigned char *src,
				    size_t              src_len,
				    unsigned char       *dst)
{
	return (sp_sec_crypto->aes_update (src, src_len, dst));
}

/* Encrypt or decrypt the next source data region (src,length) to the next
   destination data region (dst,length) in AES Counter mode.
   This function can be called repeatedly, as long as the maximum counter
   use is respected. */
DDS_ReturnCode_t sp_aes_ctr_end (uint32_t *counter)
{
	return (sp_sec_crypto->aes_end (counter));
}

/* Cleanup an existing AES context. */

DDS_ReturnCode_t sp_aes_ctr (const unsigned char *key,
			     size_t              key_len,
			     const unsigned char IV [16],
			     uint32_t            *counter,
			     const unsigned char *data,
			     size_t              length,
			     unsigned char       *rdata)
{
	if (sp_sec_crypto)
		return (sp_sec_crypto->aes_ctr (key,
						key_len,
						IV,
						counter,
						data,
						length,
						rdata));
	
	return (DDS_RETCODE_BAD_PARAMETER);
}
