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

/* sec_util.c -- Various utility functions for handling security tokens. */

#include "dds/dds_security.h"
#include "log.h"
#include "prof.h"
#include "error.h"
#include "sec_data.h"
#include "sec_util.h"

enum mem_block_en {
	MB_DATA_HOLDER,		/* Security Data Holder. */
	MB_PROPERTY,		/* Property. */
	MB_BIN_PROPERTY,	/* Binary Property. */
	MB_SEQ,			/* [Binary]Property/DataHolder Sequence. */

	MB_END
};

static const char *sec_mem_names [] = {
	"DATA_HOLDER",
	"PROPERTY",
	"BIN_PROPERTY",
	"HOLDER_SEQ"
};

static MEM_DESC_ST	sec_mem_blocks [MB_END];/* Memory used by Security. */
static size_t		sec_mem_size;		/* Total memory allocated. */

PROF_PID (sec_gen_rand)
PROF_PID (sec_gen_nonce)
PROF_PID (sec_h_sha256)
PROF_PID (sec_sign_sha256)
PROF_PID (sec_enc_secret)
PROF_PID (sec_dec_priv)
PROF_PID (sec_hm_sha1)
PROF_PID (sec_hm_sha256)
PROF_PID (sec_hm_sha_dbw)
PROF_PID (sec_aes128)
PROF_PID (sec_aes256)
PROF_PID (sec_aes_dbw)

int sec_init (const SEC_CONFIG *limits)
{
	/* Check if already initialized. */
	if (sec_mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (sec_mem_blocks, MB_END);
		return (DDS_RETCODE_OK);
	}

	/* Define the different pool attributes. */
	MDS_POOL_TYPE (sec_mem_blocks, MB_DATA_HOLDER,  limits->data_holders,   sizeof (DDS_DataHolder));
	MDS_POOL_TYPE (sec_mem_blocks, MB_PROPERTY,     limits->properties,     sizeof (DDS_Property));
	MDS_POOL_TYPE (sec_mem_blocks, MB_BIN_PROPERTY, limits->bin_properties, sizeof (DDS_BinaryProperty));
	MDS_POOL_TYPE (sec_mem_blocks, MB_SEQ,          limits->sequences,      sizeof (DDS_OctetSeq));

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	sec_mem_size = mds_alloc (sec_mem_blocks, sec_mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!sec_mem_size) {
		warn_printf ("sec_init: not enough memory available!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	log_printf (SEC_ID, 0, "sec_init: %lu bytes allocated for pools.\r\n",
						(unsigned long) sec_mem_size);
#endif
	PROF_INIT ("C:GenRand", sec_gen_rand);
	PROF_INIT ("C:GenNonce", sec_gen_nonce);
	PROF_INIT ("C:SHA256", sec_h_sha256);
	PROF_INIT ("C:SignSHA256", sec_sign_sha256);
	PROF_INIT ("C:EncSecret", sec_enc_secret);
	PROF_INIT ("C:DecPriv", sec_dec_priv);
	PROF_INIT ("C:HMAC_SHA1", sec_hm_sha1);
	PROF_INIT ("C:HMAC_SHA256", sec_hm_sha256);
	PROF_INIT ("C:HMAC_SHAx", sec_hm_sha_dbw);
	PROF_INIT ("C:AES128", sec_aes128);
	PROF_INIT ("C:AES256", sec_aes256);
	PROF_INIT ("C:AESx", sec_aes_dbw);
	return (DDS_RETCODE_OK);
}

void sec_final (void)
{
	mds_free (sec_mem_blocks, MB_END);
}

#ifdef DDS_DEBUG

/* sec_pool_dump -- Dump the memory usage of the security pool. */

void sec_pool_dump (size_t sizes [])
{
	print_pool_table (sec_mem_blocks, (unsigned) MB_END, sizes);
}

#endif

DDS_Property *DDS_Property__alloc (const char *key)
{
	DDS_Property	*p;

	p = mds_pool_alloc (&sec_mem_blocks [MB_PROPERTY]);
	if (p)
		DDS_Property__init (p, key);
	return (p);
}

void DDS_Property__init (DDS_Property *p, const char *key)
{
	p->key = key;
	p->value = NULL;
}

static void DDS_Property__cleanup (DDS_Property *p)
{
	if (p->value)
		xfree (p->value);
}

void DDS_Property__clear (DDS_Property *p)
{
	DDS_Property__cleanup (p);
	p->value = NULL;
}

void DDS_Property__free (DDS_Property *p)
{
	if (!p)
		return;

	DDS_Property__cleanup (p);
	mds_pool_free (&sec_mem_blocks [MB_PROPERTY], p);
}


DDS_Properties *DDS_Properties__alloc (void)
{
	DDS_Properties	*p;

	p = mds_pool_alloc (&sec_mem_blocks [MB_SEQ]);
	if (p)
		DDS_Properties__init (p);
	return (p);
}

void DDS_Properties__init (DDS_Properties *p)
{
	DDS_SEQ_INIT (*p);
}

static void DDS_Properties__cleanup (DDS_Properties *p)
{
	DDS_Property	*ep;
	unsigned	i;

	DDS_SEQ_FOREACH_ENTRY (*p, i, ep)
		DDS_Property__cleanup (ep);
	dds_seq_cleanup (p);
}

void DDS_Properties__clear (DDS_Properties *p)
{
	DDS_Properties__cleanup (p);
	DDS_Properties__init (p);
}

void DDS_Properties__free (DDS_Properties *p)
{
	if (!p)
		return;

	DDS_Properties__cleanup (p);
	mds_pool_free (&sec_mem_blocks [MB_SEQ], p);
}

DDS_BinaryProperty *DDS_BinaryProperty__alloc (const char *key)
{
	DDS_BinaryProperty	*p;

	p = mds_pool_alloc (&sec_mem_blocks [MB_BIN_PROPERTY]);
	if (p)
		DDS_BinaryProperty__init (p, key);
	return (p);
}

void DDS_BinaryProperty__init (DDS_BinaryProperty *p, const char *key)
{
	p->key = key;
	DDS_SEQ_INIT (p->value);
}

void DDS_BinaryProperty__clear (DDS_BinaryProperty *p)
{
	dds_seq_cleanup (&p->value);
}

void DDS_BinaryProperty__free (DDS_BinaryProperty *p)
{
	if (!p)
		return;

	DDS_BinaryProperty__clear (p);
	mds_pool_free (&sec_mem_blocks [MB_BIN_PROPERTY], p);
}

DDS_BinaryProperties *DDS_BinaryProperties__alloc (void)
{
	DDS_BinaryProperties	*p;

	p = mds_pool_alloc (&sec_mem_blocks [MB_SEQ]);
	if (p)
		DDS_BinaryProperties__init (p);
	return (p);
}

void DDS_BinaryProperties__init (DDS_BinaryProperties *p)
{
	DDS_SEQ_INIT (*p);
}

void DDS_BinaryProperties__clear (DDS_BinaryProperties *p)
{
	DDS_BinaryProperty	*ep;
	unsigned		i;

	DDS_SEQ_FOREACH_ENTRY (*p, i, ep)
		DDS_BinaryProperty__clear (ep);
	dds_seq_cleanup (p);
}

void DDS_BinaryProperties__free (DDS_BinaryProperties *p)
{
	if (!p)
		return;

	DDS_BinaryProperties__init (p);
	mds_pool_free (&sec_mem_blocks [MB_SEQ], p);
}

DDS_LongLongSeq *DDS_LongLongSeq__alloc (void)
{
	DDS_LongLongSeq *s;

	s = mm_fcts.alloc_ (sizeof (DDS_LongLongSeq));
	if (!s)
		return (NULL);

	DDS_LongLongSeq__init (s);
	return (s);
}

void DDS_LongLongSeq__init (DDS_LongLongSeq *s)
{
	DDS_SEQ_INIT (*s);
}

void DDS_LongLongSeq__clear (DDS_LongLongSeq *s)
{
	dds_seq_cleanup (s);
}

void DDS_LongLongSeq__free (DDS_LongLongSeq *s)
{
	if (!s)
		return;

	DDS_LongLongSeq__clear (s);
	mm_fcts.free_ (s);
}

DDS_DataHolder *DDS_DataHolder__alloc (const char *class_id)
{
	DDS_DataHolder *p;

	p = mds_pool_alloc (&sec_mem_blocks [MB_DATA_HOLDER]);
	if (!p)
		return (NULL);

	DDS_DataHolder__init (p, class_id);
	return (p);
}

void DDS_DataHolder__init (DDS_DataHolder *h, const char *class_id)
{
	memset (h, 0, sizeof (DDS_DataHolder));
	h->class_id = class_id;
}

static void DDS_DataHolder__cleanup (DDS_DataHolder *h)
{
	if (h->string_properties)
		DDS_Properties__free (h->string_properties);
	if (h->binary_properties)
		DDS_BinaryProperties__free (h->binary_properties);
	if (h->string_values)
		DDS_StringSeq__free (h->string_values);
	if (h->binary_value1)
		DDS_OctetSeq__free (h->binary_value1);
	if (h->binary_value2)
		DDS_OctetSeq__free (h->binary_value2);
	if (h->longlongs_value) {
		DDS_LongLongSeq__free (h->longlongs_value);
	}
}

void DDS_DataHolder__clear (DDS_DataHolder *h)
{
	DDS_DataHolder__cleanup (h);
	memset (&h->string_properties, 0, sizeof (DDS_DataHolder) - sizeof (char *));
}

void DDS_DataHolder__free (DDS_DataHolder *h)
{
	if (!h)
		return;

	DDS_DataHolder__cleanup (h);
	mds_pool_free (&sec_mem_blocks [MB_DATA_HOLDER], h);
}

Token_t *token_ref (Token_t *h)
{
	/*log_printf (SEC_ID, 0, "Token_ref: %p [%d]\r\n", (void *) h, h->nusers);*/
	if (!h)
		return (NULL);

	h->nusers++;
	return (h);
}

void token_unref (Token_t *tp)
{
	Token_t	*ntp;

	/*	if (tp)
		log_printf (SEC_ID, 0, "Token_unref: %p [%d]\r\n", (void *) tp, tp->nusers);
	*/
	while (tp) {
		if (tp->nusers > 1) {
			tp->nusers--;
			break;
		}
		else {
			ntp = tp->next;
			/* Allocated as one big chunk */
			if (tp->integral)
				xfree (tp->data);
			else
				DDS_DataHolder__free (tp->data);
			xfree (tp);
			tp = ntp;
		}
	}
}

/* DDS_DataHolder_add_property -- Add a normal (string) property to the
				  DataHolder's property list. */

DDS_Property *DDS_DataHolder_add_property (DDS_DataHolder *h,
					   const char     *key,
					   char           *value)
{
	DDS_Property	 *pp;
	unsigned	 i;
	int		 new_seq;

	if (h->string_properties) {
		new_seq = 0;
		DDS_SEQ_FOREACH_ENTRY (*h->string_properties, i, pp)
			if (pp->key && !strcmp (pp->key, key)) {
				if (!pp->value || pp->value == value) {
					pp->value = value;
					return (pp);
				}
				else
					return (NULL);
			}
	}
	else {
		h->string_properties = DDS_Properties__alloc ();
		if (!h->string_properties)
			return (NULL);

		new_seq = 1;
	}
	i = DDS_SEQ_LENGTH (*h->string_properties);
	if (DDS_SEQ_MAXIMUM (*h->string_properties) < i + 1 &&
	    dds_seq_require (h->string_properties, i + 1)) {
		if (new_seq) {
			DDS_Properties__free (h->string_properties);
			h->string_properties = NULL;
		}
		return (NULL);
	}
	pp = &DDS_SEQ_ITEM (*h->string_properties, i);
	DDS_Property__init (pp, key);
	pp->value = value;
	return (pp);
}

/* DDS_DataHolder_get_property -- Lookup a string property in the DataHolder's
				  property list. */

DDS_Property *DDS_DataHolder_get_property (DDS_DataHolder *h, const char *key)
{
	DDS_Property	*pp;
	unsigned	i;

	if (!h || !h->string_properties)
		return (NULL);

	DDS_SEQ_FOREACH_ENTRY (*h->string_properties, i, pp)
		if (pp->key && !strcmp (pp->key, key))
			return (pp);

	return (NULL);
}

/* DDS_DataHolder_add_binary_property -- Add a binary property to the
					 DataHolder's binary property list. */

DDS_BinaryProperty *DDS_DataHolder_add_binary_property (DDS_DataHolder *h,
							const char     *key)
{
	DDS_BinaryProperty	*pp;
	unsigned	 	i;
	int			new_seq;

	if (h->binary_properties) {
		new_seq = 0;
		DDS_SEQ_FOREACH_ENTRY (*h->binary_properties, i, pp)
			if (pp->key && !strcmp (pp->key, key))
				return ((DDS_SEQ_LENGTH (pp->value)) ? NULL
								     : pp);
	}
	else {
		h->binary_properties = DDS_BinaryProperties__alloc ();
		if (!h->binary_properties)
			return (NULL);

		new_seq = 1;
	}
	i = DDS_SEQ_LENGTH (*h->binary_properties);
	if (DDS_SEQ_MAXIMUM (*h->binary_properties) < i + 1 &&
	    dds_seq_require (h->binary_properties, i + 1)) {
		if (new_seq) {
			DDS_BinaryProperties__free (h->binary_properties);
			h->binary_properties = NULL;
		}
		return (NULL);
	}
	pp = &DDS_SEQ_ITEM (*h->binary_properties, i);
	DDS_BinaryProperty__init (pp, key);
	return (pp);
}

/* DDS_DataHolder_get_binary_property -- Lookup a binary property in the
					 DataHolder's binary property list. */

DDS_BinaryProperty *DDS_DataHolder_get_binary_property (DDS_DataHolder *h,
							const char     *key)
{
	DDS_BinaryProperty	*pp;
	unsigned		i;

	if (!h || !h->binary_properties)
		return (NULL);

	DDS_SEQ_FOREACH_ENTRY (*h->binary_properties, i, pp)
		if (pp->key && !strcmp (pp->key, key))
			return (pp);

	return (NULL);
}


DDS_DataHolderSeq *DDS_DataHolderSeq__alloc (void)
{
	DDS_DataHolderSeq	*p;

	p = mds_pool_alloc (&sec_mem_blocks [MB_SEQ]);
	if (p)
		DDS_DataHolderSeq__init (p);
	return (p);
}

void DDS_DataHolderSeq__init (DDS_DataHolderSeq *h)
{
	DDS_SEQ_INIT (*h);
}

void DDS_DataHolderSeq__clear (DDS_DataHolderSeq *h)
{
	DDS_DataHolder	*ep;
	unsigned	i;

	DDS_SEQ_FOREACH_ENTRY (*h, i, ep)
		DDS_DataHolder__cleanup (ep);
	dds_seq_cleanup (h);
}

void DDS_DataHolderSeq__free (DDS_DataHolderSeq *h)
{
	if (!h)
		return;

	DDS_DataHolderSeq__clear (h);
	mds_pool_free (&sec_mem_blocks [MB_SEQ], h);
}

/* sec_generate_random -- Generate a large random value with the given length
			  as a # of bytes. */

DDS_ReturnCode_t sec_generate_random (unsigned char *data, size_t length)
{
	DDS_SecurityReqData	sdata;
	DDS_ReturnCode_t	ret;

	prof_start (sec_gen_rand);
	sdata.length = length;
	sdata.rdata = data;
	ret = sec_crypto_request (DDS_GEN_RANDOM, &sdata);
	prof_stop (sec_gen_rand, 1);
	return (ret);
}

/* sec_generate_nonce -- Generate a NONCE and return it as an Octet Sequence. */

DDS_OctetSeq *sec_generate_nonce (void)
{
	DDS_OctetSeq		*p;
	DDS_ReturnCode_t	ret;
	DDS_SecurityReqData	sdata;

	prof_start (sec_gen_nonce);
	p = DDS_OctetSeq__alloc ();
	if (!p)
		return (NULL);

	ret = dds_seq_require (p, NONCE_LENGTH);
	if (ret) {
		DDS_OctetSeq__free (p);
		return (NULL);
	}
	sdata.rdata = DDS_SEQ_DATA (*p);
	sdata.length = NONCE_LENGTH;
	ret = sec_crypto_request (DDS_GEN_RANDOM, &sdata);
	if (ret) {
		DDS_OctetSeq__free (p);
		return (NULL);
	}
	memcpy (DDS_SEQ_DATA (*p), "CHALLENGE:", 10);
	prof_stop (sec_gen_nonce, 1);
	return (p);
}

/* sec_hash_sha256 -- Calculate a SHA256 over a linear data area. */

DDS_ReturnCode_t sec_hash_sha256 (unsigned char *sdata,
				  size_t        slength,
				  unsigned char *hash)
{
	DDS_ReturnCode_t	ret;
	DDS_SecurityReqData	data;

	prof_start (sec_h_sha256);
	data.action = DDS_CRYPT_FULL;
	data.data = sdata;
	data.length = slength;
	data.rdata = hash;
	ret = sec_crypto_request (DDS_SHA256, &data);
	prof_stop (sec_h_sha256, 1);
	return (ret);
}

/* sec_valid_sha256 -- Verify whether a SHA256 hash over a linear data area 
		       is correct. */

int sec_valid_sha256 (const unsigned char *sdata,
		      size_t              slength,
		      const unsigned char *hash)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	error;
	unsigned char		hdata [32];

	data.action = DDS_CRYPT_FULL;
	data.data = (unsigned char *) sdata;
	data.length = slength;
	data.rdata = hdata;
	error = sec_crypto_request (DDS_SHA256, &data);
	if (error)
		return (0);

	return (!memcmp (hash, hdata, 32));
}

/* sec_sign_sha256_data -- Sign a SHA256 checksum of a linear data area using
			   the private key of the user identified with the
			   given id. */

DDS_OctetSeq *sec_sign_sha256_data (Identity_t          id,
				    const unsigned char *sdata,
				    size_t              slength)
{
	DDS_SecurityReqData	data;
	DDS_OctetSeq		*p;
	DDS_ReturnCode_t	ret;

	prof_start (sec_sign_sha256);
	p = DDS_OctetSeq__alloc ();
	if (!p)
		return (NULL);

	ret = dds_seq_require (p, MAX_RSA_KEY_SIZE);
	if (ret) {
		DDS_OctetSeq__free (p);
		return (NULL);
	}
	data.handle = id;
	data.data = (unsigned char *) sdata;
	data.length = slength;
	data.rdata = DDS_SEQ_DATA (*p);
	data.rlength = MAX_RSA_KEY_SIZE;
	ret = sec_certificate_request (DDS_SIGN_SHA256, &data);
	if (ret) {
		DDS_OctetSeq__free (p);
		return (NULL);
	}
	DDS_SEQ_LENGTH (*p) = data.rlength;
	prof_stop (sec_sign_sha256, 1);
	return (p);
}

/* sec_signed_sha256_data -- Verify whether a data area SHA256 checksum was
			     correctly signed using the public key of the user,
			     the signed data and the expected SHA256 value. */

int sec_signed_sha256_data (Identity_t          peer,
			    const unsigned char *sdata,
			    size_t              slength,
			    const unsigned char *hash,
			    size_t              hlength)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	data.handle = peer;
	data.data = (unsigned char *) sdata;
	data.length = slength;
	data.rdata = (unsigned char *) hash;
	data.rlength = hlength;
	ret = sec_certificate_request (DDS_VERIFY_SHA256, &data);
	if (ret)
		return (0);
	else
		return (data.secure);
}

/* sec_encrypt_secret -- Encrypt a shared secret with a public key. */

DDS_OctetSeq *sec_encrypt_secret (Identity_t    peer,
				  unsigned char *sdata,
				  size_t        slength)
{
	DDS_SecurityReqData	data;
	DDS_OctetSeq		*p;
	DDS_ReturnCode_t	ret;
	char			buf [MAX_RSA_KEY_SIZE];

	prof_start (sec_enc_secret);
	data.handle = peer;
	data.data = sdata;
	data.length = slength;
	data.rdata = buf;
	data.rlength = sizeof (buf);
	ret = sec_certificate_request (DDS_ENCRYPT_PUBLIC, &data);
	if (ret)
		return (NULL);

	p = DDS_OctetSeq__alloc ();
	if (!p)
		return (NULL);

	if (dds_seq_require (p, data.rlength)) {
		DDS_OctetSeq__free (p);
		return (NULL);
	}
	memcpy (DDS_SEQ_DATA (*p), buf, data.rlength);
	prof_stop (sec_enc_secret, 1);
	return (p);
}

/* sec_decrypted_secret -- Decrypt a shared secret and return whether this
			   succeeded properly. */

int sec_decrypted_secret (Identity_t    replier,
			  unsigned char *sdata,
			  size_t        slength,
			  unsigned char *rdata)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	prof_start (sec_dec_priv);
	data.handle = replier;
	data.data = sdata;
	data.length = slength;
	data.rdata = rdata;
	data.rlength = SHARED_SECRET_LENGTH;
	ret = sec_certificate_request (DDS_DECRYPT_PRIVATE, &data);
	prof_stop (sec_dec_priv, 1);
	return (ret);
}

/* sec_hmac_sha1 -- Calculate a HMAC_SHA1 over message data using the given key
		    and return the HMAC data in *result (20-bytes). */

DDS_ReturnCode_t sec_hmac_sha1 (const unsigned char *key,
				size_t              key_length,
				const unsigned char *msg,
				size_t              msg_length,
				unsigned char       *result)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	prof_start (sec_hm_sha1);
	data.action = DDS_CRYPT_FULL;
	data.kdata = (unsigned char *) key;
	data.klength = key_length;
	data.data = (unsigned char *) msg;
	data.length = msg_length;
	data.rdata = result;
	ret = sec_crypto_request (DDS_HMAC_SHA1, &data);
	prof_stop (sec_hm_sha1, 1);
	return (ret);
}

/* sec_hmac_sha256 -- Calculate a HMAC_SHA256 over message data using the given
		      key and return the HMAC data in *result (32-bytes). */

DDS_ReturnCode_t sec_hmac_sha256 (const unsigned char *key,
				  size_t              key_length,
				  const unsigned char *msg,
				  size_t              msg_length,
				  unsigned char       *result)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	prof_start (sec_hm_sha256);
	data.action = DDS_CRYPT_FULL;
	data.kdata = (unsigned char *) key;
	data.klength = key_length;
	data.data = (unsigned char *) msg;
	data.length = msg_length;
	data.rdata = result;
	ret = sec_crypto_request (DDS_HMAC_SHA256, &data);
	prof_stop (sec_hm_sha256, 1);
	return (ret);
}

/* sec_hmac_sha_dbw -- Calculate a HMAC_SHA1 or HMAC_SHA256 over message data
		       using the given key and return the HMAC data in *result.*/

DDS_ReturnCode_t sec_hmac_sha_dbw (const unsigned char *key,
				    size_t              key_length,
				    const DBW           *msg,
				    unsigned char       *result,
				    int                 sha_kind)
{
	DDS_SecurityReqData	data;
	const DB		*dbp;
	size_t			len, clen;
	int			ret;

	prof_start (sec_hm_sha_dbw);
	data.action = DDS_CRYPT_BEGIN;
	data.kdata = (unsigned char *) key;
	data.klength = key_length;
	data.data = (unsigned char *) msg->data;
	data.length = clen = msg->left;
	data.rdata = result;
	for (len = msg->length, dbp = msg->dbp; len; ) {
		ret = sec_crypto_request (sha_kind, &data);
		if (ret)
			return (ret);

		if (len <= clen)
			break;

		len -= clen;
		if (len <= clen)
			data.action = DDS_CRYPT_END;
		else
			data.action = DDS_CRYPT_UPDATE;

		dbp = dbp->next;
		data.data = (unsigned char *) dbp->data;
		if (dbp->size > len)
			clen = len;
		else
			clen = dbp->size;
		data.length = clen;
	}
	prof_stop (sec_hm_sha_dbw, 1);
	return (DDS_RETCODE_OK);
}

/* sec_hmac_sha1_dbw -- Calculate a HMAC_SHA1 over message data using the given
			key and return the HMAC data in *result (20 bytes). */

DDS_ReturnCode_t sec_hmac_sha1_dbw (const unsigned char *key,
				    size_t              key_length,
				    const DBW           *msg,
				    unsigned char       *result)
{
	if (msg->length <= msg->left)
		return (sec_hmac_sha1 (key, key_length, msg->data, msg->length, result));
	else
		return (sec_hmac_sha_dbw (key, key_length, msg, result, DDS_HMAC_SHA1));
}

/* sec_hmac_sha256_dbw -- Calculate a HMAC_SHA256 over message data using the
			  given key and return the HMAC data in *result (32
			  bytes). */

DDS_ReturnCode_t sec_hmac_sha256_dbw (const unsigned char *key,
				      size_t              key_length,
				      const DBW           *msg,
				      unsigned char       *result)
{
	if (msg->length <= msg->left)
		return (sec_hmac_sha256 (key, key_length, msg->data, msg->length, result));
	else
		return (sec_hmac_sha_dbw (key, key_length, msg, result, DDS_HMAC_SHA256));
}

/* sec_aes128_ctr_crypt -- Do AES128 counter mode en/de-cryption on a data block
			   and return the result using key, salt and counter. */

DDS_ReturnCode_t sec_aes128_ctr_crypt (const unsigned char *key,
				       size_t              key_length,
				       const unsigned char *salt,
				       uint32_t            *counter,
				       const unsigned char *msg,
				       unsigned char       *result,
				       size_t              length)
{
	DDS_SecurityReqData	data;
	int			ret;

	prof_start (sec_aes128);
	data.action = DDS_CRYPT_FULL;
	data.kdata = (unsigned char *) key;
	data.klength = key_length;
	data.tag = (const char *) salt;
	data.secure = *counter;
	data.data = (unsigned char *) msg;
	data.rdata = result;
	data.length = length;
	ret = sec_crypto_request (DDS_AES128_CTR, &data);
	if (!ret)
		*counter = data.secure;
	prof_stop (sec_aes128, 1);
	return (ret);
}

/* sec_aes256_ctr_crypt -- Do AES256 counter mode en/de-cryption on a data block
			   and return the result using key, salt and counter. */

DDS_ReturnCode_t sec_aes256_ctr_crypt (const unsigned char *key,
				       size_t              key_length,
				       const unsigned char *salt,
				       uint32_t            *counter,
				       const unsigned char *msg,
				       unsigned char       *result,
				       size_t              length)
{
	DDS_SecurityReqData	data;
	int			ret;

	prof_start (sec_aes256);
	data.action = DDS_CRYPT_FULL;
	data.kdata = (unsigned char *) key;
	data.klength = key_length;
	data.tag = (char *) salt;
	data.secure = *counter;
	data.data = (unsigned char *) msg;
	data.rdata = result;
	data.length = length;
	ret = sec_crypto_request (DDS_AES256_CTR, &data);
	if (!ret)
		*counter = data.secure;
	prof_stop (sec_aes256, 1);
	return (ret);
}

/* sec_aes_ctr_crypt -- Do any AES counter mode en/de-cryption on a data
			block (msg) and return the result using key, salt
			and counter parameters. */

static DDS_ReturnCode_t sec_aes_ctr_crypt (const unsigned char *key,
				           size_t              key_length,
					   const unsigned char *salt,
					   uint32_t            *counter,
					   const DBW           *msg,
					   unsigned char       *result,
					   int                 aes_kind)
{
	DDS_SecurityReqData	data;
	const DB		*dbp;
	size_t			len, clen;
	int			ret;

	prof_start (sec_aes_dbw);
	data.action = DDS_CRYPT_BEGIN;
	data.kdata = (unsigned char *) key;
	data.klength = key_length;
	data.tag = (char *) salt;
	data.secure = *counter;
	data.data = (unsigned char *) msg->data;
	data.length = clen = msg->left;
	data.rdata = result;
	for (len = msg->length, dbp = msg->dbp; len; ) {
		ret = sec_crypto_request (aes_kind, &data);
		if (ret)
			return (ret);

		if (len <= clen)
			break;

		data.rdata = (unsigned char *) data.rdata + clen;
		len -= clen;
		if (len <= clen)
			data.action = DDS_CRYPT_END;
		else
			data.action = DDS_CRYPT_UPDATE;

		dbp = dbp->next;
		data.data = (unsigned char *) dbp->data;
		if (dbp->size > len)
			clen = len;
		else
			clen = dbp->size;
		data.length = clen;
	}
	*counter = data.secure;
	prof_stop (sec_aes_dbw, 1);
	return (DDS_RETCODE_OK);
}

/* sec_aes128_ctr_crypt_dbw -- Do AES128 counter mode en/de-cryption on a data
			       block (msg) and return the result using key, salt
			       and counter parameters. */

DDS_ReturnCode_t sec_aes128_ctr_crypt_dbw (const unsigned char *key,
				           size_t              key_length,
					   const unsigned char *salt,
					   uint32_t            *counter,
					   const DBW           *msg,
					   unsigned char       *result)
{
	if (msg->length <= msg->left)
		return (sec_aes128_ctr_crypt (key, key_length, salt, counter, msg->data, result, msg->length));
	else
		return (sec_aes_ctr_crypt (key, key_length, salt, counter, msg, result, DDS_AES128_CTR));
}

/* sec_aes256_ctr_crypt_dbw -- Do AES256 counter mode en/de-cryption on a data
			       block (msg) and return the result using key, salt
			       and counter parameters. */

DDS_ReturnCode_t sec_aes256_ctr_crypt_dbw (const unsigned char *key,
				           size_t              key_length,
					   const unsigned char *salt,
					   uint32_t            *counter,
					   const DBW           *msg,
					   unsigned char       *result)
{
	if (msg->length <= msg->left)
		return (sec_aes256_ctr_crypt (key, key_length, salt, counter, msg->data, result, msg->length));
	else
		return (sec_aes_ctr_crypt (key, key_length, salt, counter, msg, result, DDS_AES256_CTR));
}

