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

/* sp_crypto.h -- DDS Security Plugin - Crypto plugin definitions. */

#ifndef __sp_crypto_h_
#define __sp_crypto_h_

#include "dds/dds_error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
	SP_HASH_SHA1,
	SP_HASH_SHA256
} Hash_t;

typedef struct sp_sec_crypto_st SP_SEC_CRYPTO;

typedef DDS_ReturnCode_t (*SP_SEC_INIT) (void);

typedef DDS_ReturnCode_t (*SP_SEC_HASH_SHA) (const unsigned char *buffer,
					     size_t              length,
					     unsigned char       *hash,
					     size_t              *hash_len);

typedef DDS_ReturnCode_t (*SP_SEC_COMP_HASH) (const unsigned char *hash1,
					      size_t              len1,
					      const unsigned char *hash2,
					      size_t              len2);

typedef DDS_ReturnCode_t (*SP_SEC_GENERATE) (size_t length, 
					     unsigned char *data);

typedef DDS_ReturnCode_t (*SP_SEC_HASH_BEGIN) (const unsigned char *data, 
					       size_t              length);

typedef DDS_ReturnCode_t (*SP_SEC_HASH_CONTINUE) (const unsigned char *data, 
						  size_t              length);

typedef DDS_ReturnCode_t (*SP_SEC_HASH_END) (unsigned char *result);

typedef DDS_ReturnCode_t (*SP_SEC_HMAC_SHA) (const unsigned char *data,
					     size_t              data_len,
					     const unsigned char *key,
					     size_t              key_len,
					     unsigned char       *hash,
					     size_t              *hash_len);

typedef DDS_ReturnCode_t (*SP_SEC_HMAC_SHA_BEGIN) (const unsigned char *key,
						   size_t key_len);

typedef DDS_ReturnCode_t (*SP_SEC_HMAC_SHA_CONTINUE) (const unsigned char *data,
						      size_t length);

typedef DDS_ReturnCode_t (*SP_SEC_HMAC_SHA_END) (unsigned char *result,
						 size_t *length);

typedef DDS_ReturnCode_t (*SP_SEC_AES_BEGIN) (const unsigned char IV [16],
					      const unsigned char *key,
					      size_t              key_bits,
					      uint32_t            counter);

typedef DDS_ReturnCode_t (*SP_SEC_AES_UPDATE) (const unsigned char *src,
					       size_t              src_len,
					       unsigned char       *dst);

typedef DDS_ReturnCode_t (*SP_SEC_AES_END) (uint32_t *counter);

typedef DDS_ReturnCode_t (*SP_AES_CTR) (const unsigned char *key,
					size_t              key_len,
					const unsigned char IV [16],
					uint32_t            *counter,
					const unsigned char *data,
					size_t              length,
					unsigned char       *rdata);

struct sp_sec_crypto_st {
	SP_SEC_INIT              init;
	SP_SEC_HASH_SHA          hash_sha1;
	SP_SEC_HASH_SHA          hash_sha256;
	SP_SEC_COMP_HASH         compare_hash;
	SP_SEC_GENERATE          generate_random;
	SP_SEC_HASH_BEGIN        sha1_begin;
	SP_SEC_HASH_CONTINUE     sha1_continue;
	SP_SEC_HASH_END          sha1_end;
	SP_SEC_HASH_BEGIN        sha256_begin;
	SP_SEC_HASH_CONTINUE     sha256_continue;
	SP_SEC_HASH_END          sha256_end;
	SP_SEC_HMAC_SHA          hmac_sha1;
	SP_SEC_HMAC_SHA          hmac_sha256;
	SP_SEC_HMAC_SHA_BEGIN    hmac_sha1_begin;
	SP_SEC_HMAC_SHA_CONTINUE hmac_sha1_continue;
	SP_SEC_HMAC_SHA_END      hmac_sha1_end;
	SP_SEC_HMAC_SHA_BEGIN    hmac_sha256_begin;
	SP_SEC_HMAC_SHA_CONTINUE hmac_sha256_continue;
	SP_SEC_HMAC_SHA_END      hmac_sha256_end;
	SP_SEC_AES_BEGIN         aes_begin;
	SP_SEC_AES_UPDATE        aes_update;
	SP_SEC_AES_END           aes_end;
	SP_AES_CTR               aes_ctr;
	/* TODO: add more */
};

void sp_set_sp_sec_crypto (SP_SEC_CRYPTO *func);

DDS_ReturnCode_t sp_crypto_initialize (void);

/* Hashing */

DDS_ReturnCode_t sp_hash_sha1 (const unsigned char *buffer,
			       size_t              length,
			       unsigned char       *hash,
			       size_t              *hash_len);

DDS_ReturnCode_t sp_hash_sha256 (const unsigned char *buffer,
				 size_t              length,
				 unsigned char       *hash,
				 size_t              *hash_len);

DDS_ReturnCode_t sp_compare_hash (const unsigned char *hash1,
				  size_t              len1,
				  const unsigned char *hash2,
				  size_t              len2);

DDS_ReturnCode_t sp_generate_random (size_t length, unsigned char *secret);


/* SHA functions.
   ---------------- */

DDS_ReturnCode_t sp_sha_begin (const void *data, size_t length, Hash_t type);

/* SHA-1 checksum calculation on a data area. This must be the first function
   to start the checksum. */

DDS_ReturnCode_t sp_sha_continue (const void *data, size_t length, Hash_t type);

/* SHA-1 checksum calculation on a data area.  This function should be used for
   consecutively following data areas. */

DDS_ReturnCode_t sp_sha_end (void *checksum, Hash_t type);

/* Retrieve the resulting 20-byte SHA-1 checksum of the entire data area. */


/* HMAC */

DDS_ReturnCode_t sp_hmac_sha (const unsigned char *data,
			      size_t              data_len,
			      const unsigned char *key,
			      size_t              key_len,
			      unsigned char       *hash,
			      size_t              *hash_len,
			      Hash_t              type);

DDS_ReturnCode_t sp_hmac_sha_begin (const unsigned char *key,
				    size_t              key_len,
				    Hash_t              type);

DDS_ReturnCode_t sp_hmac_sha_continue (const unsigned char *data,
				       size_t              length,
				       Hash_t              type);

DDS_ReturnCode_t sp_hmac_sha_end (unsigned char *result, size_t *length, Hash_t type);


/* AES-128/192/256 counter mode encryption/decryption.
   --------------------------------------------------- */

DDS_ReturnCode_t sp_aes_ctr_begin (const unsigned char IV [16],
				   const unsigned char *key,
				   size_t              key_bits,
				   uint32_t            counter);

/* Create a new AES crypto handle for either encryption or decryption with the
   given key.  The key size should be either 128 or 256 bits as
   given in the key_bits parameter.  The IV argument should be
   properly initialized. If successful, a non-0 AES context handle
   is returned that can be used in the following function. */

DDS_ReturnCode_t sp_aes_ctr_update (const unsigned char *src,
				    size_t              src_len,
				    unsigned char       *dst);
				    
/* Encrypt or decrypt the next source data region (src,length) to the next
   destination data region (dst,length) in AES Counter mode.
   This function can be called repeatedly, as long as the maximum counter
   use is respected. */

DDS_ReturnCode_t sp_aes_ctr_end (uint32_t *counter);
/* Cleanup an existing AES context. */

DDS_ReturnCode_t sp_aes_ctr (const unsigned char *key,
			     size_t              key_bits,
			     const unsigned char IV [16],
			     uint32_t            *counter,
			     const unsigned char *data,
			     size_t              length,
			     unsigned char       *rdata);

#endif

