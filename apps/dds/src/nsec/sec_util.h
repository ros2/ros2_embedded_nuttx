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

/* sec_util.h -- Utility functions for security/crypto. */

#ifndef __sec_util_h_
#define __sec_util_h_

#include "sec_id.h"

#define SHARED_SECRET_LENGTH	32	/* 256-bit strong random number. */
#define	DERIVED_KEY_LENGTH	32	/* 256-bit derived key length. */
#define NONCE_LENGTH		128	/* Challenge length. */

#define	MIN_RSA_KEY_SIZE	64	/* No support for keys < 512 bits. */
#define	MAX_RSA_KEY_SIZE	512	/* Allows keys of up to 4096 bits. */


DDS_ReturnCode_t sec_generate_random (unsigned char *data, size_t length);

/* Generate a large random value with the given length as a # of bytes. */

DDS_OctetSeq *sec_generate_nonce (void);

/* Generate a NONCE and return it as an Octet Sequence. */

DDS_ReturnCode_t sec_hash_sha256 (unsigned char *sdata,
				  size_t        slength,
				  unsigned char *hash);

/* Calculate a SHA256 over a linear data area. */

int sec_valid_sha256 (const unsigned char *sdata,
		      size_t              slength,
		      const unsigned char *hash);

/* Verify whether the SHA256 hash over a linear data area is correct. */

DDS_OctetSeq *sec_sign_sha256_data (Identity_t          id,
				    const unsigned char *sdata,
				    size_t              slength);

/* Sign a SHA256 checksum of a linear data area using the private key of the
   user identified with the given id. */

int sec_signed_sha256_data (Identity_t          peer,
			    const unsigned char *sdata,
			    size_t              slength,
			    const unsigned char *hash,
			    size_t              hlength);

/* Verify whether a data area SHA256 checksum was correctly signed using the
   public key of the user, the signed data and the expected SHA256 value. */

DDS_OctetSeq *sec_encrypt_secret (Identity_t    peer,
				  unsigned char *sdata,
				  size_t        slength);

/* Encrypt a shared secret with the public key of the peer. */

int sec_decrypted_secret (Identity_t    replier,
			  unsigned char *sdata,
			  size_t        slength,
			  unsigned char *rdata);

/* Decrypt a shared secret and return whether this succeeded properly. */

DDS_ReturnCode_t sec_hmac_sha1 (const unsigned char *key,
				size_t              key_length,
				const unsigned char *msg,
				size_t              msg_length,
				unsigned char       *result);

/* Calculate a HMAC_SHA1 over message data using the given key and return
   the HMAC data in *result (20 bytes). */

DDS_ReturnCode_t sec_hmac_sha256 (const unsigned char *key,
				  size_t              key_length,
				  const unsigned char *msg,
				  size_t              msg_length,
				  unsigned char       *result);

/* Calculate a HMAC_SHA256 over message data using the given key and return
   the HMAC data in *result (32 bytes). */

DDS_ReturnCode_t sec_hmac_sha1_dbw (const unsigned char *key,
				    size_t              key_length,
				    const DBW           *msg,
				    unsigned char       *result);

/* Calculate a HMAC_SHA1 over message data using the given key and return
   the HMAC data in *result (20 bytes). */

DDS_ReturnCode_t sec_hmac_sha256_dbw (const unsigned char *key,
				      size_t              key_length,
				      const DBW           *msg,
				      unsigned char       *result);

/* Calculate a HMAC_SHA256 over message data using the given key and return
   the HMAC data in *result (32 bytes). */

DDS_ReturnCode_t sec_aes128_ctr_crypt (const unsigned char *key,
				       size_t              key_length,
				       const unsigned char *salt,
				       uint32_t            *counter,
				       const unsigned char *msg,
				       unsigned char       *result,
				       size_t              length);

/* Do AES128 counter mode enc/decryption on a data block (msg) and return the
   result in *result using key, salt and counter parameters.  The length of both
   will be the same (length). The salt length should be 16 bytes. */

DDS_ReturnCode_t sec_aes256_ctr_crypt (const unsigned char *key,
				       size_t              key_length,
				       const unsigned char *salt,
				       uint32_t            *counter,
				       const unsigned char *msg,
				       unsigned char       *result,
				       size_t              length);

/* Do AES256 counter mode enc/decryption on a data block (msg) and return the
   result in *result using key, salt and counter parameters.  The length of both
   will be the same (length). The salt length should be 16-bytes. */

DDS_ReturnCode_t sec_aes128_ctr_crypt_dbw (const unsigned char *key,
					   size_t              key_length,
					   const unsigned char *salt,
					   uint32_t            *counter,
					   const DBW           *msg,
					   unsigned char       *result);

/* Do AES128 counter mode en/de-cryption on a data block (msg) and return the
   result in *result using key, salt and counter parameters.  The length of both
   will be the same (length). The salt length should be 16 bytes. */

DDS_ReturnCode_t sec_aes256_ctr_crypt_dbw (const unsigned char *key,
					   size_t              key_length,
					   const unsigned char *salt,
					   uint32_t            *counter,
					   const DBW           *msg,
					   unsigned char       *result);

/* Do AES256 counter mode en/de-cryption on a data block (msg) and return the
   result in *result using key, salt and counter parameters.  The length of both
   will be the same (length). The salt length should be 16 bytes. */

#endif /* !__sec_util_h_ */

