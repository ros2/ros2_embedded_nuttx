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


#include "sp_sys_crypto.h"
#include "sp_crypto.h"
#include "dds/dds_error.h"
#include <stddef.h>
#include "openssl/evp.h"
#include "openssl/bn.h"
#include "openssl/rand.h"
#include "openssl/hmac.h"
#include "dlist.h"
#include "error.h"
#include "dds/dds_error.h"
#include <stdlib.h>
#include <string.h>
	
/***********/
/* Hashing */
/***********/

static DDS_ReturnCode_t sp_hash (unsigned char *buffer,
				 size_t        length,
				 unsigned char *hash,
				 size_t        *hash_len,
				 const EVP_MD  *type)
{
	unsigned char *ptr;

	if (type == EVP_sha1 ()) {
		if (length < 20) {
			memcpy (hash, buffer, length);
			ptr = &hash [length - 1];
			memset (ptr, 1, 20 - length);
		} else
			memcpy (hash, buffer, 20);
		*hash_len = 20;
	}
	else if (type == EVP_sha256 ()) {
		if (length < 32) {
			memcpy (hash, buffer, length);
			ptr = &hash [length - 1];
			memset (ptr, 1, 32 - length);
		} else
			memcpy (hash,buffer, 32);
		*hash_len = 32;
	}
	return (DDS_RETCODE_OK);
}

/* Calculate sha1 hash */

static DDS_ReturnCode_t sp_sys_none_hash_sha1 (unsigned char *buffer,
					  size_t        length,
					  unsigned char *hash,
					  size_t        *hash_len)
{
	return (sp_hash (buffer,
			 length, 
			 hash, 
			 hash_len,
			 EVP_sha1 ()));
}

/* Calculate the hash for the buffer with sha256 */

static DDS_ReturnCode_t sp_sys_none_hash_sha256 (unsigned char *buffer,
					    size_t        length,
					    unsigned char *hash,
					    size_t        *hash_len)
{
	return (sp_hash (buffer,
			 length, 
			 hash, 
			 hash_len,
			 EVP_sha256 ()));
}

/* binary compare of the hashes */

static DDS_ReturnCode_t sp_sys_none_compare_hash (unsigned char *hash1,
					     size_t        len1,
					     unsigned char *hash2,
					     size_t        len2)
{
	size_t i;
	size_t c, x;
	
	if (!hash1 || !hash2)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (len1 != len2)
		return (DDS_RETCODE_ERROR);

	c = len1 / sizeof (x);
	for (i = 0; i < c; i++) {
		if (*(unsigned long *) (hash1 + (i * sizeof (x))) !=
		    *(unsigned long *) (hash2 + (i * sizeof (x))))
			return (DDS_RETCODE_ERROR);
	}
	for (i = c * sizeof (x); i < len1; i++)
		if (hash1 [i] != hash2 [i])
			return (DDS_RETCODE_ERROR);

	return (DDS_RETCODE_OK);
}

static unsigned sp_hash_begin (unsigned char *data, size_t length, const EVP_MD *type)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (length)
	ARG_NOT_USED (type)
	
	return (0);
}

static DDS_ReturnCode_t sp_hash_continue (unsigned char *data, size_t length, unsigned handle)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (length)
	ARG_NOT_USED (handle)
	
	return (DDS_RETCODE_UNSUPPORTED);
}

static DDS_ReturnCode_t sp_hash_end (unsigned char *result, unsigned handle)
{
	ARG_NOT_USED (result)
	ARG_NOT_USED (handle)

	return (DDS_RETCODE_UNSUPPORTED);
}

static DDS_ReturnCode_t sp_sys_none_sha1_begin (unsigned char *data, size_t length, unsigned *handle)
{
	if ((*handle = sp_hash_begin (data, length, EVP_sha1 ())))
		return (DDS_RETCODE_OK);

	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

static DDS_ReturnCode_t sp_sys_none_sha1_continue (unsigned char *data, size_t length, unsigned handle)
{
	return (sp_hash_continue (data, length, handle));
}

static DDS_ReturnCode_t sp_sys_none_sha1_end (unsigned char *result, unsigned handle)
{
	return (sp_hash_end (result, handle));
}

static DDS_ReturnCode_t sp_sys_none_sha256_begin (unsigned char *data, size_t length, unsigned *handle)
{
	if ((*handle = sp_hash_begin (data, length, EVP_sha256 ())))
		return (DDS_RETCODE_OK);

	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

static DDS_ReturnCode_t sp_sys_none_sha256_continue (unsigned char *data, size_t length, unsigned handle)
{
	return (sp_hash_continue (data, length, handle));
}

static DDS_ReturnCode_t sp_sys_none_sha256_end (unsigned char *result, unsigned handle)
{
	return (sp_hash_end (result, handle));
}

/**********/
/* RANDOM */
/**********/

/* random number will be length bits */

static DDS_ReturnCode_t sp_sys_none_generate_random (size_t length, unsigned char *secret)
{
	BIGNUM bn;
	
	if (!RAND_load_file ("/dev/urandom", 1024))
		return (DDS_RETCODE_ERROR);

	/* PROBLEM ON WINDOWS SINCE IT DOES NOT HAVE A /dev/random */

	BN_init (&bn);
	BN_rand (&bn, length << 3, 1, 1);
	BN_bn2bin (&bn, secret);
	BN_free (&bn);

	return (DDS_RETCODE_OK);
}

/********/
/* HMAC */
/********/


/* Calculate the HMAC with sha1 */

DDS_ReturnCode_t sp_sys_none_hmac_sha1 (unsigned char *data,
				   size_t        data_len,
				   unsigned char *key,
				   size_t        key_len,
				   unsigned char *hash,
				   size_t        *hash_len)
{
	if (!data ||
	    !data_len ||
	    !key ||
	    !key_len ||
	    !hash)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!HMAC (EVP_sha1 (), key, key_len, data, data_len, hash, hash_len))
		return (DDS_RETCODE_ERROR);

	return (DDS_RETCODE_OK);
}

/* Calculate HMAC with sha256 */

DDS_ReturnCode_t sp_sys_none_hmac_sha256 (unsigned char *data,
				     size_t        data_len,
				     unsigned char *key,
				     size_t        key_len,
				     unsigned char *hash,
				     size_t        *hash_len)
{
	if (!data ||
	    !data_len ||
	    !key ||
	    !key_len ||
	    !hash)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!HMAC (EVP_sha256 (), key, key_len, data, data_len, hash, hash_len))
		return (DDS_RETCODE_ERROR);

	return (DDS_RETCODE_OK);
}


static unsigned sp_hmac_begin (unsigned char *key, size_t key_len,
			       const EVP_MD *type)
{
	ARG_NOT_USED (key)
	ARG_NOT_USED (key_len)
	ARG_NOT_USED (type)

	return (0);
}

static DDS_ReturnCode_t sp_hmac_continue (unsigned char *data, size_t length, unsigned handle)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (length)
	ARG_NOT_USED (handle)

	return (DDS_RETCODE_UNSUPPORTED);
}

static DDS_ReturnCode_t sp_hmac_end (unsigned char *result, size_t *length, unsigned handle)
{
	ARG_NOT_USED (result)
	ARG_NOT_USED (length)
	ARG_NOT_USED (handle)

		return (DDS_RETCODE_UNSUPPORTED);
}

static DDS_ReturnCode_t sp_sys_none_hmac_sha1_begin (unsigned char *key, size_t key_len,
						     unsigned *handle)
{
	if ((*handle = sp_hmac_begin (key, key_len, EVP_sha1 ())))
		return (DDS_RETCODE_OK);

	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

static DDS_ReturnCode_t sp_sys_none_hmac_sha1_continue (unsigned char *data, size_t length, unsigned handle)
{
	return (sp_hmac_continue (data, length, handle));
}

static DDS_ReturnCode_t sp_sys_none_hmac_sha1_end (unsigned char *result, size_t *length, unsigned handle)
{
	return (sp_hmac_end (result, length, handle));
}

static DDS_ReturnCode_t sp_sys_none_hmac_sha256_begin (unsigned char *key, size_t key_len,
						       unsigned *handle)
{
	if ((*handle = sp_hmac_begin (key, key_len, EVP_sha256 ())))
		return (DDS_RETCODE_OK);

	return (DDS_RETCODE_OUT_OF_RESOURCES);

}

static DDS_ReturnCode_t sp_sys_none_hmac_sha256_continue (unsigned char *data, size_t length, unsigned handle)
{
	return (sp_hmac_continue (data, length, handle));
}

static DDS_ReturnCode_t sp_sys_none_hmac_sha256_end (unsigned char *result, size_t *length, unsigned handle)
{
	return (sp_hmac_end (result, length, handle));
}

/*******/
/* AES */
/*******/

static DDS_ReturnCode_t sp_sys_aes_ctr_encrypt_begin (unsigned      *handle,
						      unsigned char IV [32],
						      unsigned char *key,
						      size_t        key_bits)
{
	ARG_NOT_USED (IV)
	ARG_NOT_USED (key)
	ARG_NOT_USED (key_bits)

	*handle = 1;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_aes_ctr_encrypt_update (unsigned      handle,
						       unsigned char *src,
						       size_t        src_len,
						       unsigned char *dst,
						       size_t        *length)
{
	if (handle == 1 && *length >= src_len)
		memcpy (dst, src, src_len);
	else
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_aes_ctr_encrypt_end (unsigned      handle, 
						    unsigned char *dst,
						    size_t        *length)
{
	ARG_NOT_USED (dst)
	ARG_NOT_USED (length)
	
	if (handle != 1)
		return (DDS_RETCODE_BAD_PARAMETER);
	*length = 0;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_aes_ctr_decrypt_begin (unsigned      *handle,
						      unsigned char IV [32],
						      unsigned char *key,
						      size_t        key_bits)
{
	return (sp_sys_aes_ctr_encrypt_begin (handle, IV, key, key_bits));
}

static DDS_ReturnCode_t sp_sys_aes_ctr_decrypt_update (unsigned      handle,
						       unsigned char *src,
						       size_t        src_len,
						       unsigned char *dst,
						       size_t        *length)
{
	return (sp_sys_aes_ctr_encrypt_update (handle, src, src_len, dst, length));
}

static DDS_ReturnCode_t sp_sys_aes_ctr_decrypt_end (unsigned      handle, 
						    unsigned char *dst,
						    size_t        *length)
{
	return (sp_sys_aes_ctr_encrypt_end (handle, dst, length));
}

static SP_SEC_CRYPTO sp_sec_crypto = {
	sp_sys_none_hash_sha1,
	sp_sys_none_hash_sha256,
	sp_sys_none_compare_hash,
	sp_sys_none_generate_random,
	sp_sys_none_sha1_begin,
	sp_sys_none_sha1_continue,
	sp_sys_none_sha1_end,
	sp_sys_none_sha256_begin,
	sp_sys_none_sha256_continue,
	sp_sys_none_sha256_end,
	sp_sys_none_hmac_sha1,
	sp_sys_none_hmac_sha256,
	sp_sys_none_hmac_sha1_begin,
	sp_sys_none_hmac_sha1_continue,
	sp_sys_none_hmac_sha1_end,
	sp_sys_none_hmac_sha256_begin,
	sp_sys_none_hmac_sha256_continue,
	sp_sys_none_hmac_sha256_end,
	sp_sys_aes_ctr_encrypt_begin,
	sp_sys_aes_ctr_encrypt_update,
	sp_sys_aes_ctr_encrypt_end,
	sp_sys_aes_ctr_decrypt_begin,
	sp_sys_aes_ctr_decrypt_update,
	sp_sys_aes_ctr_decrypt_end
};

void sp_sec_crypto_none_add (void)
{
	sp_set_sp_sec_crypto (&sp_sec_crypto);
}
