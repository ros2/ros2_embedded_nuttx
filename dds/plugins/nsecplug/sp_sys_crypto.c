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


#include <stdlib.h>
#include <string.h>
#include "thread.h"
#include "sp_sys_crypto.h"
#include "sp_crypto.h"
#include "dds/dds_error.h"
#include <stddef.h>
#include "openssl/evp.h"
#include "openssl/bn.h"
#include "openssl/rand.h"
#include "openssl/hmac.h"
#include "dlist.h"
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

EVP_MD_CTX       *hash_ctx = NULL;
lock_t           hash_lock;
HMAC_CTX         hmac_ctx;
lock_t           hmac_lock;
EVP_CIPHER_CTX   *aes_enc_ctx = NULL;
lock_t           aes_lock;
size_t           total_length;
uint32_t         ctr;

DDS_ReturnCode_t sp_sys_crypto_init (void)
{
	/* initialize all the locks */
	lock_init_nr (hash_lock, "sp_hash");
	lock_init_nr (hmac_lock, "sp_hmac");
	lock_init_nr (aes_lock, "sp_aes");

	return (DDS_RETCODE_OK);
}

/***********/
/* Hashing */
/***********/

static DDS_ReturnCode_t sp_hash (const unsigned char *buffer,
				 size_t              length,
				 unsigned char       *hash,
				 size_t              *hash_len,
				 const EVP_MD        *type)
{
	EVP_MD_CTX ctx;
	unsigned   len;
	
	if (!buffer ||
	    !length ||
	    !hash)
		return (DDS_RETCODE_BAD_PARAMETER);
	
	lock_take (hash_lock);

	EVP_MD_CTX_init (&ctx);
	if (!(EVP_DigestInit (&ctx, type)))
		goto error;

	if (!(EVP_DigestUpdate (&ctx, buffer, length)))
		goto error;

	if (!(EVP_DigestFinal (&ctx, hash, &len)))
		goto error;

	EVP_MD_CTX_cleanup (&ctx);
	*hash_len = len;
	lock_release (hash_lock);
	return (DDS_RETCODE_OK);

 error:
	lock_release (hash_lock);
	return (DDS_RETCODE_ERROR);
}

/* Calculate sha1 hash */

static DDS_ReturnCode_t sp_sys_hash_sha1 (const unsigned char *buffer,
					  size_t              length,
					  unsigned char       *hash,
					  size_t              *hash_len)
{
	return (sp_hash (buffer,
			 length, 
			 hash, 
			 hash_len,
			 EVP_sha1 ()));
}

/* Calculate the hash for the buffer with sha256 */

static DDS_ReturnCode_t sp_sys_hash_sha256 (const unsigned char *buffer,
					    size_t              length,
					    unsigned char       *hash,
					    size_t              *hash_len)
{
	return (sp_hash (buffer,
			 length, 
			 hash, 
			 hash_len,
			 EVP_sha256 ()));
}

/* binary compare of the hashes */

static DDS_ReturnCode_t sp_sys_compare_hash (const unsigned char *hash1,
					     size_t              len1,
					     const unsigned char *hash2,
					     size_t              len2)
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

static DDS_ReturnCode_t sp_hash_begin (const unsigned char *data,
				       size_t              length,
				       const EVP_MD        *type)
{
	lock_take (hash_lock);
	if (!(hash_ctx = EVP_MD_CTX_create ()))	{
		lock_release (hash_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	if ((EVP_DigestInit_ex (hash_ctx, type, NULL)) <= 0) {
		EVP_MD_CTX_destroy (hash_ctx);
		lock_release (hash_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	if ((EVP_DigestUpdate (hash_ctx, data, length)) <= 0) {
		EVP_MD_CTX_destroy (hash_ctx);
		lock_release (hash_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_hash_continue (const unsigned char *data,
					  size_t              length)
{
	if ((EVP_DigestUpdate (hash_ctx, data, length)) <= 0) {
		EVP_MD_CTX_destroy (hash_ctx);
		lock_release (hash_lock);
		return (DDS_RETCODE_ERROR);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_hash_end (unsigned char *result)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	if ((EVP_DigestFinal_ex (hash_ctx, result, NULL)))
		ret = DDS_RETCODE_ERROR;

	EVP_MD_CTX_destroy (hash_ctx);
	lock_release (hash_lock);
	return (ret);
}

static DDS_ReturnCode_t sp_sys_sha1_begin (const unsigned char *data,
					   size_t              length)
{
	return (sp_hash_begin (data, length, EVP_sha1 ()));
}

static DDS_ReturnCode_t sp_sys_sha1_continue (const unsigned char *data,
					      size_t              length)
{
	return (sp_hash_continue (data, length));
}

static DDS_ReturnCode_t sp_sys_sha1_end (unsigned char *result)
{
	return (sp_hash_end (result));
}

static DDS_ReturnCode_t sp_sys_sha256_begin (const unsigned char *data,
					     size_t              length)
{
	return (sp_hash_begin (data, length, EVP_sha256 ()));
}

static DDS_ReturnCode_t sp_sys_sha256_continue (const unsigned char *data,
						size_t              length)
{
	return (sp_hash_continue (data, length));
}

static DDS_ReturnCode_t sp_sys_sha256_end (unsigned char *result)
{
	return (sp_hash_end (result));
}


/**********/
/* RANDOM */
/**********/

/* random number will be length bits */

static DDS_ReturnCode_t sp_sys_generate_random (size_t length, unsigned char *secret)
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

DDS_ReturnCode_t sp_sys_hmac_sha1 (const unsigned char *data,
				   size_t              data_len,
				   const unsigned char *key,
				   size_t              key_len,
				   unsigned char       *hash,
				   size_t              *hash_len)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;
	
	if (!data ||
	    !data_len ||
	    !key ||
	    !key_len ||
	    !hash)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (hmac_lock);
	if (!HMAC (EVP_sha1 (), key, key_len, data, data_len, hash, (unsigned int *) hash_len))
		ret = DDS_RETCODE_ERROR;

	lock_release (hmac_lock);
	return (ret);
}

/* Calculate HMAC with sha256 */

DDS_ReturnCode_t sp_sys_hmac_sha256 (const unsigned char *data,
				     size_t              data_len,
				     const unsigned char *key,
				     size_t              key_len,
				     unsigned char       *hash,
				     size_t              *hash_len)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	if (!data ||
	    !data_len ||
	    !key ||
	    !key_len ||
	    !hash)
		return (DDS_RETCODE_BAD_PARAMETER);

        lock_take (hmac_lock);
	if (!HMAC (EVP_sha256 (), key, key_len, data, data_len, hash, (unsigned int *) hash_len))
		ret = DDS_RETCODE_ERROR;

	lock_release (hmac_lock);
	return (ret);
}


static DDS_ReturnCode_t sp_hmac_begin (const unsigned char *key,
				       size_t              key_len,
				       const EVP_MD        *type)
{
	lock_take (hmac_lock);

	HMAC_CTX_init (&hmac_ctx);

	if ((HMAC_Init_ex (&hmac_ctx, key, key_len, type, NULL)) <= 0) {
		HMAC_CTX_cleanup (&hmac_ctx);
		lock_release (hmac_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_hmac_continue (const unsigned char *data,
					  size_t              length)
{
	if ((HMAC_Update (&hmac_ctx, data, length)) <= 0) {
		HMAC_CTX_cleanup (&hmac_ctx);
		lock_release (hmac_lock);
		return (DDS_RETCODE_ERROR);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_hmac_end (unsigned char *result, size_t *length)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	if ((HMAC_Final (&hmac_ctx, result, (unsigned int *) length)) <= 0)
		ret = DDS_RETCODE_ERROR;

	HMAC_CTX_cleanup (&hmac_ctx);
	lock_release (hmac_lock);
	return (ret);
}

static DDS_ReturnCode_t sp_sys_hmac_sha1_begin (const unsigned char *key,
						size_t              key_len)
{
	return (sp_hmac_begin (key, key_len, EVP_sha1 ()));
}

static DDS_ReturnCode_t sp_sys_hmac_sha1_continue (const unsigned char *data,
						   size_t              length)
{
	return (sp_hmac_continue (data, length));
}

static DDS_ReturnCode_t sp_sys_hmac_sha1_end (unsigned char *result, size_t *length)
{
	return (sp_hmac_end (result, length));
}

static DDS_ReturnCode_t sp_sys_hmac_sha256_begin (const unsigned char *key,
						  size_t              key_len)
{
	return (sp_hmac_begin (key, key_len, EVP_sha256 ()));
}

static DDS_ReturnCode_t sp_sys_hmac_sha256_continue (const unsigned char *data,
						     size_t              length)
{
	return (sp_hmac_continue (data, length));
}

static DDS_ReturnCode_t sp_sys_hmac_sha256_end (unsigned char *result, size_t *length)
{
	return (sp_hmac_end (result, length));
}

static DDS_ReturnCode_t sp_sys_aes_ctr_begin (const unsigned char IV [16],
					      const unsigned char *key,
					      size_t              key_bits,
					      uint32_t            counter)
{
	const EVP_CIPHER *cipher;
	uint32_t *p, x, ori;
	int i = 11;
	unsigned char tmp_IV [16];

	lock_take (aes_lock);
	
	if (!(aes_enc_ctx = EVP_CIPHER_CTX_new ())) {
		lock_release (aes_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	if (key_bits == 128)
		cipher = (const EVP_CIPHER *) EVP_aes_128_ctr ();
	else if (key_bits == 256)
		cipher = (const EVP_CIPHER *) EVP_aes_256_ctr ();
	else {
		EVP_CIPHER_CTX_free (aes_enc_ctx);
		lock_release (aes_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}

	memcpy (tmp_IV, IV, 16);
	p = (uint32_t *) &tmp_IV [12];
	x = ori = ntohl (*p); 
	x += counter;
	*p = ntohl (x);

	/* overflow */
	if (ori > x)
		while (tmp_IV [i]++)
			if (--i)
				break;
	total_length = 0;
	ctr = counter;

	if (EVP_EncryptInit_ex (aes_enc_ctx, cipher, NULL, key, tmp_IV) <= 0) {
		EVP_CIPHER_CTX_free (aes_enc_ctx);
		lock_release (aes_lock);
		return (DDS_RETCODE_ERROR);
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_aes_ctr_update (const unsigned char *src,
					       size_t              src_len,
					       unsigned char       *dst)
{
	int	len;

	if ((EVP_EncryptUpdate (aes_enc_ctx, dst, &len, src, src_len)) <= 0) {
		EVP_CIPHER_CTX_free (aes_enc_ctx);
		lock_release (aes_lock);
		return (DDS_RETCODE_ERROR);
	}
	total_length += len;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t sp_sys_aes_ctr_end (uint32_t *counter)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;
	unsigned char    data [512]; /* is 512 enough ??? */
	int              len;

	if ((EVP_EncryptFinal_ex (aes_enc_ctx, data, &len)) <= 0)
		ret = DDS_RETCODE_ERROR;

	/* The new counter is the initial counter +
	   number of encrypted blocks divided by 16 */
	*counter = ctr + ((total_length + 15) >> 4);
	EVP_CIPHER_CTX_free (aes_enc_ctx);
	lock_release (aes_lock);
	return (ret);
}

static DDS_ReturnCode_t sp_sys_aes_ctr (const unsigned char *key,
					size_t              key_len,
					const unsigned char IV [16],
					uint32_t            *counter,
					const unsigned char *data,
					size_t              length,
					unsigned char       *rdata)
{
	DDS_ReturnCode_t ret;

	if ((ret = sp_sys_aes_ctr_begin (IV, key, key_len, *counter)))
		return (ret);

	if ((ret = sp_sys_aes_ctr_update (data, length, rdata)))
		return (ret);

	return (sp_sys_aes_ctr_end (counter));
}

static SP_SEC_CRYPTO sp_sec_crypto = {
	sp_sys_crypto_init,
	sp_sys_hash_sha1,
	sp_sys_hash_sha256,
	sp_sys_compare_hash,
	sp_sys_generate_random,
	sp_sys_sha1_begin,
	sp_sys_sha1_continue,
	sp_sys_sha1_end,
	sp_sys_sha256_begin,
	sp_sys_sha256_continue,
	sp_sys_sha256_end,
	sp_sys_hmac_sha1,
	sp_sys_hmac_sha256,
	sp_sys_hmac_sha1_begin,
	sp_sys_hmac_sha1_continue,
	sp_sys_hmac_sha1_end,
	sp_sys_hmac_sha256_begin,
	sp_sys_hmac_sha256_continue,
	sp_sys_hmac_sha256_end,
	sp_sys_aes_ctr_begin,
	sp_sys_aes_ctr_update,
	sp_sys_aes_ctr_end,
	sp_sys_aes_ctr
};

void sp_sec_crypto_add (void)
{
	sp_set_sp_sec_crypto (&sp_sec_crypto);
}
