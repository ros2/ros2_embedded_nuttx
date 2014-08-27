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

/* sec_c_std.c -- Support for Standard DDS security.
		  Crypto handling plugin definitions. */

#include <stdint.h>
#include "log.h"
#include "prof.h"
#include "error.h"
#include "random.h"
#include "sec_plugin.h"
#include "sec_util.h"
#include "sec_a_std.h"
#include "sec_cdata.h"
#include "sec_c_std.h"

/*#define CSTD_T_AES_TRACE	** Trace token AES encrypt/decrypt functions. */
#define	MAX_BPSESSION		2000

typedef enum {
	CK_NONE,
	CK_AES128,
	CK_AES256
} CipherKind_t;

typedef enum {
	HK_NONE,
	HK_SHA1,
	HK_SHA256
} HashKind_t;

typedef struct {
	CipherKind_t	cipher_kind;
	HashKind_t	hash_kind;
	uint32_t	master_key_id;
	unsigned char	master_key [32];
	unsigned char	initialization_vector [32];
	unsigned char	hmac_key_id [32];
} KeyMaterial_AES_CTR_HMAC;

/* Master keys are stored in different master records, depending on key size: */
typedef struct {
	uint32_t	id;
	unsigned char	master_key [16];
	unsigned char	hmac_key_id [16];
	unsigned char	init_vector [16];
} Master128_t;

typedef struct {
	uint32_t	id;
	unsigned char	master_key [32];
	unsigned char	hmac_key_id [32];
	unsigned char	init_vector [32];
} Master256_t;

typedef union {
	Master128_t	m128;
	Master256_t	m256;
} Master_t;

/* Master sizes depend on the key size as well as of the type of use: */
#define CRYPT_128_MSZ	sizeof (Master128_t)
#define	CRYPT_256_MSZ	sizeof (Master256_t)
#define	SIGN_128_MSZ	offsetof (Master128_t, init_vector)
#define	SIGN_256_MSZ	offsetof (Master256_t, init_vector)

/* Session keys are stored in different session records: */
typedef struct session_128_st {
	uint32_t	id;		/* Key id. */
	unsigned char	hmac [16];	/* HMAC key. */
	unsigned char	key [16];	/* Key value. */
	unsigned char	salt [16];	/* Salt value (Enc/Dec only). */
	uint32_t	counter;	/* Block counter (Tx only). */
} Session128_t;

typedef struct session_256_st {
	uint32_t	id;		/* Key id. */
	unsigned char	hmac [32];	/* HMAC key. */
	unsigned char	key [32];	/* Key value (Enc/Dec only). */
	unsigned char	salt [16];	/* Salt value (Enc/Dec only). */
	uint32_t	counter;	/* Block counter (Tx only). */
} Session256_t;

typedef union SessionAES_un {
	Session128_t	s128;
	Session256_t	s256;
} Session_t;

/* Session sizes depend on the type of session: */
#define	SIGN_128_SSZ	offsetof (Session128_t, key)
#define	SIGN_256_SSZ	offsetof (Session256_t, key)
#define	ENCODE_128_SZ	sizeof (Session128_t)
#define	ENCODE_256_SZ	sizeof (Session256_t)
#define	DECODE_128_SZ	(sizeof (Session128_t) - sizeof (uint32_t))
#define	DECODE_256_SZ	(sizeof (Session256_t) - sizeof (uint32_t))

/* Kx keys required for token encryption/decryption and signing. */
typedef struct kx_keys_st {
	unsigned char	kx_key [32];
	unsigned char	kx_mac_key [32];
} KxKeys_t;

/* Peer Endpoint key info. */
typedef struct ep_key_info_st {
	EntityId_t	eid;		/* Endpoint Entity Id. */
	unsigned	data_cipher:2;	/* Data Rx encryption type. */
	unsigned	data_hash:2;	/* Data Rx source sign type. */
	unsigned	sign_hash:2;	/* Data Rx destination sign type. */
	Master_t	*data_master;	/* Data Rx & source sign master keys.*/
	Session_t	*data_session;	/* Data Rx & source sign session keys.*/
	Master_t	*sign_master;	/* Data Rx dest. sign master keys. */
	Session_t	*sign_session;	/* Data Rx dest. sign session keys. */

	/* Embedded master and session keys follow. */
} EPKeyInfo_t;

/* Full crypto data set:
   Note that depending on entity type, lots of fields are optional. */
typedef struct std_crypto_st {
	unsigned	data_hash:2;	/* Signing type for data Tx/Rx. */
	unsigned	data_cipher:2;	/* Cipher type for data Tx/Rx. */
	unsigned	sign_hash:2;	/* Signing type for p2p data Tx. */
	unsigned	rem_hash:2;	/* Signing type for p2p data rx. */
	unsigned	local:1;	/* Local crypto. */
	unsigned	endpoint:1;	/* Endpoint crypto. */
	Master_t	*data_m;	/* Master keys (data sign and AES). */
	Session_t	*data_s;	/* Session keys (data sign and AES). */
	Master_t	*sign_tx_m;	/* Master keys for p2p tx signing. */
	Session_t	*sign_tx_s;	/* Session key for p2p tx signing. */
	Master_t	*sign_rx_m;	/* Master keys for p2p rx signing. */
	Session_t	*sign_rx_s;	/* Session key for p2p rx signing. */
	KxKeys_t	kx_keys;	/* KxKeys for transport (P only). */
	Session256_t	*token_tx;	/* Token encode session (P only). */
	Session256_t	*token_rx;	/* Token decode session (P only). */
	Skiplist_t	ep_keys;	/* List of peer endpoint keys. */
	size_t		tcsize;		/* Total size of stored key data. */
} StdCrypto_t;

#define	CRYPTO_SZ_DP	offsetof (StdCrypto_t, sign_tx_m)
#define	CRYPTO_SZ_P	sizeof (StdCrypto_t)
#define	CRYPTO_SZ_LEP	offsetof (StdCrypto_t, sign_tx_m)
#define	CRYPTO_SZ_REP	offsetof (StdCrypto_t, kx_keys)

#define KEY_SIZE(m)	((m == DDS_CRYPT_HMAC_SHA1 || m == DDS_CRYPT_AES128_HMAC_SHA1) ? 16 : 32)

const char *crypt_std_str [] = {
	"none", "HMAC_SHA1", "HMAC_SHA256",
	"AES128_HMAC_SHA1", "AES256_HMAC_SHA256"
};

static unsigned long	max_blocks_per_session = MAX_BPSESSION;
static unsigned		next_id;


/*************************************/
/*   1. Crypto context management.   */
/*************************************/

static void cps_mode_get (unsigned mode, CipherKind_t *cipher, HashKind_t *hash)
{
	switch (mode) {
		case DDS_CRYPT_HMAC_SHA1:
			*cipher = CK_NONE;
			*hash = HK_SHA1;
			break;
		case DDS_CRYPT_HMAC_SHA256:
			*cipher = CK_NONE;
			*hash = HK_SHA256;
			break;
		case DDS_CRYPT_AES128_HMAC_SHA1:
			*cipher = CK_AES128;
			*hash = HK_SHA1;
			break;
		case DDS_CRYPT_AES256_HMAC_SHA256:
			*cipher = CK_AES256;
			*hash = HK_SHA256;
			break;
		default:
			*cipher = *hash = 0;
			break;
	}
}

/* cps_crypto_sizes -- Return the sizes of Master and Session crypto data. */

static void cps_crypto_sizes (CipherKind_t ck,
			      HashKind_t   hk,
			      size_t       *msize,
			      size_t       *ssize,
			      int          tx)
{
	if (ck == CK_AES256) {
		*msize = CRYPT_256_MSZ;
		*ssize = (tx) ? ENCODE_256_SZ : DECODE_256_SZ;
	}
	else if (ck == CK_AES128) {
		*msize = CRYPT_128_MSZ;
		*ssize = (tx) ? ENCODE_128_SZ : DECODE_128_SZ;
	}
	else if (hk == HK_SHA256) {
		*msize = SIGN_256_MSZ;
		*ssize = SIGN_256_SSZ;
	}
	else if (hk == HK_SHA1) {
		*msize = SIGN_128_MSZ;
		*ssize = SIGN_128_SSZ;
	}
	else
		*msize = *ssize = 0;
}

/* cps_init_master_keys -- Initialize the master crypto data. */

static void cps_init_master_keys (Master_t *kp, int encrypt, int ksize)
{
	if (ksize == 32) {
		kp->m256.id = next_id++;
		sec_generate_random (kp->m256.master_key, 32);
		sec_generate_random (kp->m256.hmac_key_id, 32);
		if (encrypt)
			sec_generate_random (kp->m256.init_vector, 32);
	}
	else {
		kp->m128.id = next_id++;
		sec_generate_random (kp->m128.master_key, 16);
		sec_generate_random (kp->m128.hmac_key_id, 16);
		if (encrypt)
			sec_generate_random (kp->m128.init_vector, 16);
	}
}

/* cps_init_session -- Initialize a session context. The algorithm used is based
		       on the original OMG security presentation on 20/12/2013,
		       modified to take into account master key/HMAC sizes of 16
		       bytes as well as 32 bytes. */

static DDS_ReturnCode_t cps_init_session (HashKind_t     hash,
					  CipherKind_t   cipher,
					  const Master_t *mp,
					  Session_t      *sp,
					  uint32_t       id,
					  int            tx)
{
	Session128_t	*ssp;
	Session256_t	*lsp;
	unsigned char	data [50];
	unsigned char	h [32];
	DDS_ReturnCode_t ret;

	memcpy (data, "SessionHMACKey", 14);
	if (hash == HK_SHA1) {
		memcpy (data + 14, mp->m128.hmac_key_id, 16);
		data [30] = id >> 24;
		data [31] = (id >> 16) & 0xff;
		data [32] = (id >> 8) & 0xff;
		data [33] = id & 0xff;
		ssp = &sp->s128;
		ssp->id = id;
		ret = sec_hmac_sha1 (mp->m128.master_key, 16, data, 34, h);
		if (ret)
			return (ret);

		memcpy (ssp->hmac, h, 16);
		if (cipher) {
			if (tx)
				ssp->counter = 0;

			memcpy (data, "SessionKey", 9);
			memcpy (data + 9, mp->m128.init_vector, 16);
			data [25] = id >> 24;
			data [26] = (id >> 16) & 0xff;
			data [27] = (id >> 8) & 0xff;
			data [28] = id & 0xff;
			data [29] = 1;
			ret = sec_hmac_sha1 (mp->m128.master_key, 16, data, 30, h);
			if (ret)
				return (ret);

			memcpy (ssp->key, h, 16);
			memcpy (data, "SessionSalt", 10);
			memcpy (data + 10, mp->m128.init_vector, 16);
			data [26] = id >> 24;
			data [27] = (id >> 16) & 0xff;
			data [28] = (id >> 8) & 0xff;
			data [29] = id & 0xff;
			data [30] = 0;
			ret = sec_hmac_sha1 (mp->m128.master_key, 16, data, 31, h);
			memcpy (ssp->salt, h, 16);
		}
	}
	else /* hash == HK_SHA256 */ {
		memcpy (data + 14, mp->m256.hmac_key_id, 32);
		data [46] = id >> 24;
		data [47] = (id >> 16) & 0xff;
		data [48] = (id >> 8) & 0xff;
		data [49] = id & 0xff;
		lsp = &sp->s256;
		lsp->id = id;
		ret = sec_hmac_sha256 (mp->m256.master_key, 32, data, 50, lsp->hmac);
		if (ret)
			return (ret);

		if (cipher) {
			if (tx)
				lsp->counter = 0;

			memcpy (data, "SessionKey", 9);
			memcpy (data + 9, mp->m256.init_vector, 32);
			data [41] = id >> 24;
			data [42] = (id >> 16) & 0xff;
			data [43] = (id >> 8) & 0xff;
			data [44] = id & 0xff;
			data [45] = 1;
			ret = sec_hmac_sha256 (mp->m256.master_key, 32, data, 46, lsp->key);
			if (ret)
				return (ret);

			memcpy (data, "SessionSalt", 10);
			memcpy (data + 10, mp->m256.init_vector, 32);
			data [42] = id >> 24;
			data [43] = (id >> 16) & 0xff;
			data [44] = (id >> 8) & 0xff;
			data [45] = id & 0xff;
			data [46] = 0;
			ret = sec_hmac_sha256 (mp->m256.master_key, 32, data, 47, h);
			if (!ret)
				memcpy (lsp->salt, h, 16);
		}
	}
	return (ret);
}

/* cps_register_loc_part -- Register the local domain participant. */

static ParticipantCrypto_t cps_register_loc_part (const SEC_CRYPTO *cp,
						  const char       *idtoken_name,
						  Domain_t         *domain,
						  DDS_ReturnCode_t *error)
{
	CryptoData_t	*cdp;
	StdCrypto_t	*sp;
	unsigned char	*xp;
	HashKind_t	hash;
	CipherKind_t	cipher;
	size_t		size, msize, ssize;

	if (strcmp (idtoken_name, GMCLASSID_SECURITY_IDENTITY_TOKEN))
		return (0);

	/* Reseed the Id counter. */
	next_id += fastrand ();

	size = CRYPTO_SZ_DP;
	cps_mode_get (domain->rtps_protected, &cipher, &hash);
	cps_crypto_sizes (cipher, hash, &msize, &ssize, 1);
	if (!msize)
		size -= sizeof (Master_t *) + sizeof (Session_t *);
	else
		size += msize + ssize;
	cdp = crypto_create (cp, size, &domain->participant, 0);
	if (!cdp) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (0);
	}
	sp = (StdCrypto_t *) cdp->data;
	memset (sp, 0, size);
	sp->data_hash = hash;
	sp->data_cipher = cipher;
	sp->sign_hash = 0;
	sp->rem_hash = 0;
	sp->local = 1;
	sp->endpoint = 0;
	if (hash) {
		xp = ((unsigned char *) sp) + CRYPTO_SZ_DP;
		sp->data_m = (Master_t *) xp;
		xp += msize;
		cps_init_master_keys (sp->data_m, cipher, (hash == HK_SHA256) ? 32 : 16);
		sp->data_s = (Session_t *) xp;
		*error = cps_init_session (hash, cipher, sp->data_m, sp->data_s, next_id++, 1);
		if (*error) {
			crypto_release (cdp->handle);
			return (0);
		}
	}
	*error = DDS_RETCODE_OK;
	return (cdp->handle);
}

/* cps_register_rem_part -- Register a discovered Participant for crypto ops. */

static ParticipantCrypto_t cps_register_rem_part (const SEC_CRYPTO *cp,
						  CryptoData_t     *lcp,
						  Participant_t    *remote,
						  const SEC_AUTH   *plugin,
						  SharedSecret_t   secret,
						  DDS_ReturnCode_t *error)
{
	CryptoData_t	*cdp;
	StdCrypto_t	*sp;
	Domain_t	*dp;
	unsigned char	*xp;
	HashKind_t	hash;
	CipherKind_t	cipher;
	size_t		size, msize, ssize;
	Master256_t	token_m;

	ARG_NOT_USED (lcp)

	dp = remote->p_domain;
	size = CRYPTO_SZ_P;
	cps_mode_get (dp->rtps_protected, &cipher, &hash);
	if (hash == HK_SHA1) {
		msize = SIGN_128_MSZ;
		ssize = SIGN_128_SSZ;
	}
	else if (hash == HK_SHA256) {
		msize = SIGN_256_MSZ;
		ssize = SIGN_256_SSZ;
	}
	else
		msize = ssize = 0;
	size += msize + ssize + ENCODE_256_SZ + DECODE_256_SZ;
	cdp = crypto_create (cp, size, remote, 0);
	if (!cdp) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (0);
	}
	sp = (StdCrypto_t *) cdp->data;
	memset (sp, 0, size);
	sp->data_hash = 0;
	sp->data_cipher = 0;
	sp->sign_hash = hash;
	sp->rem_hash = 0;
	sp->local = 0;
	sp->endpoint = 0;
	xp = ((unsigned char *) sp) + CRYPTO_SZ_P;
	if (hash) {

		/* Initialize Tx signing master data. */
		sp->sign_tx_m = (Master_t *) xp;
		xp += msize;
		cps_init_master_keys (sp->sign_tx_m, 0, (hash == HK_SHA256) ? 32 : 16);

		/* Initialize Tx signing session data. */
		sp->sign_tx_s = (Session_t *) xp;
		xp += ssize;
		cps_init_session (hash, CK_NONE, sp->sign_tx_m, sp->sign_tx_s, next_id++, 1);
	}

	/* Initialize the KxKeys. */
	plugin->get_kx (plugin, secret, sp->kx_keys.kx_key, sp->kx_keys.kx_mac_key);

	/* Initialize Token Master structure. */
	token_m.id = 0;
	memcpy (token_m.master_key, sp->kx_keys.kx_key, 32);
	memcpy (token_m.hmac_key_id, sp->kx_keys.kx_mac_key, 32);
	memset (token_m.init_vector, 0, 32);

	/* Initialize Token Tx/Rx encryption sessions. */
	sp->token_tx = (Session256_t *) xp;
	xp += ENCODE_256_SZ;
	cps_init_session (HK_SHA256, CK_AES256,
			  (Master_t *) &token_m,
			  (Session_t *) sp->token_tx,
			  next_id++, 1);
	sp->token_rx = (Session256_t *) xp;
	memset (sp->token_rx, 0, DECODE_256_SZ);
	sl_init (&sp->ep_keys, sizeof (EPKeyInfo_t *));
	sp->tcsize = 0;
	return (cdp->handle);
}

/* cps_register_local_endpoint -- Register a local endpoint for std crypto. */

static Crypto_t cps_register_local_endpoint (const SEC_CRYPTO *cp,
					     CryptoData_t     *part,
					     LocalEndpoint_t  *lep,
					     DDS_ReturnCode_t *error)
{
	CryptoData_t	*cdp;
	StdCrypto_t	*sp;
	unsigned char	*xp;
	HashKind_t	hash;
	CipherKind_t	cipher;
	size_t		size, msize, ssize;

	ARG_NOT_USED (part)

	/* Reseed the Id counter. */
	next_id += fastrand ();

	size = CRYPTO_SZ_LEP;
	cps_mode_get (lep->crypto_type, &cipher, &hash);
	cps_crypto_sizes (cipher, hash, &msize, &ssize, 1);
	if (msize)
		size += msize + ssize;
	else
		size -= sizeof (Master_t *) - sizeof (Session_t *);
	cdp = crypto_create (cp, size, &lep->ep, 1);
	if (!cdp) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (0);
	}
	sp = (StdCrypto_t *) cdp->data;
	memset (sp, 0, size);
	sp->data_hash = hash;
	sp->data_cipher = cipher;
	sp->sign_hash = 0;
	sp->rem_hash = 0;
	sp->local = 1;
	sp->endpoint = 1;
	if (hash) {
		xp = ((unsigned char *) sp) + CRYPTO_SZ_LEP;
		sp->data_m = (Master_t *) xp;
		xp += msize;
		cps_init_master_keys (sp->data_m, cipher, (hash == HK_SHA256) ? 32 : 16);
		sp->data_s = (Session_t *) xp;
		*error = cps_init_session (hash, cipher, sp->data_m, sp->data_s, next_id++, 1);
		if (*error) {
			crypto_release (cdp->handle);
			return (0);
		}
	}
	*error = DDS_RETCODE_OK;
	return (cdp->handle);
}

/* cps_register_local_writer -- Register a local DataWriter for std crypto. */

static DataWriterCrypto_t cps_register_local_writer (const SEC_CRYPTO *cp,
					             CryptoData_t     *part,
						     Writer_t         *w,
						     DDS_ReturnCode_t *error)
{
	return (cps_register_local_endpoint (cp, part, &w->w_lep, error));
}

/* cps_register_local_reader -- Register a local DataReader for std crypto. */

static DataReaderCrypto_t cps_register_local_reader (const SEC_CRYPTO *cp,
						     CryptoData_t     *part,
						     Reader_t         *r,
						     DDS_ReturnCode_t *error)
{
	return (cps_register_local_endpoint (cp, part, &r->r_lep, error));
}

/* entity_id_cmp -- Compare a node key with an entity id. */

static int entity_id_cmp (const void *np, const void *data)
{
	const EPKeyInfo_t	**epp = (const EPKeyInfo_t **) np;
	const unsigned char	*eid = (const unsigned char *) data;

	return (memcmp ((*epp)->eid.id, eid, sizeof (EntityId_t)));
}

/* cps_disc_endpoint_set_keys -- Set discovered endpoint keys to received key info. */

static void cps_disc_endpoint_set_keys (StdCrypto_t *sp, EPKeyInfo_t *kip)
{
	sp->data_cipher = kip->data_cipher;
	sp->data_hash = kip->data_hash;
	sp->sign_hash = kip->sign_hash;
	sp->data_m = kip->data_master;
	sp->data_s = kip->data_session;
	sp->sign_rx_m = kip->sign_master;
	sp->sign_rx_s = kip->sign_session;
}

/* cps_register_disc_endpoint -- Register a discovered endpoint for std crypto.*/

static Crypto_t cps_register_disc_endpoint (const SEC_CRYPTO *cp,
					    CryptoData_t     *local_ep,
					    CryptoData_t     *remote_p,
					    Endpoint_t       *ep,
					    DDS_ReturnCode_t *error)
{
	CryptoData_t	*cdp;
	StdCrypto_t	*sp, *pp;
	LocalEndpoint_t	*lep;
	unsigned char	*xp;
	HashKind_t	hash;
	CipherKind_t	cipher;
	size_t		size, msize, ssize;
	EPKeyInfo_t	**epp;

	size = CRYPTO_SZ_REP;
	lep = (LocalEndpoint_t *) local_ep->parent.endpoint;
	cps_mode_get (lep->crypto_type, &cipher, &hash);
	if (hash == HK_SHA1) {
		msize = SIGN_128_MSZ;
		ssize = SIGN_128_SSZ;
	}
	else if (hash == HK_SHA256) {
		msize = SIGN_256_MSZ;
		ssize = SIGN_256_SSZ;
	}
	else {
		msize = ssize = 0;
	}
	cdp = crypto_create (cp, size + msize + ssize, ep, 1);
	if (!cdp) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (0);
	}
	sp = (StdCrypto_t *) cdp->data;
	memset (sp, 0, size);
	sp->data_hash = 0;
	sp->data_cipher = 0;
	sp->sign_hash = hash;
	sp->rem_hash = 0;
	sp->local = 0;
	sp->endpoint = 1;
	xp = ((unsigned char *) sp) + CRYPTO_SZ_REP;
	if (hash) {
		/* Initialize Tx signing master data. */
		sp->sign_tx_m = (Master_t *) xp;
		xp += msize;
		cps_init_master_keys (sp->sign_tx_m, 0, (hash == HK_SHA256) ? 32 : 16);

		/* Initialize Tx signing session data. */
		sp->sign_tx_s = (Session_t *) xp;
		xp += ssize;
		cps_init_session (hash, CK_NONE, sp->sign_tx_m, sp->sign_tx_s, next_id++, 1);
	}

	/* If we already received peer crypto tokens, apply them now. */
	pp = (StdCrypto_t *) remote_p->data;
	epp = sl_search (&pp->ep_keys, ep->entity_id.id, entity_id_cmp);
	if (epp)
		cps_disc_endpoint_set_keys (sp, *epp);

	return (cdp->handle);
}

/* cps_register_disc_writer-- Register a discovered DataWriter for std crypto. */

static DataWriterCrypto_t cps_register_disc_writer (const SEC_CRYPTO   *cp,
						    CryptoData_t       *local_r,
						    CryptoData_t       *remote_p,
						    DiscoveredWriter_t *dw,
						    DDS_ReturnCode_t   *error)
{
	ARG_NOT_USED (cp)

	return (cps_register_disc_endpoint (cp, local_r, remote_p,
							&dw->dw_ep, error));
}

/* cps_register_disc_reader -- Register a discovered DataReader for std crypto. */

static DataReaderCrypto_t cps_register_disc_reader (const SEC_CRYPTO   *cp,
						    CryptoData_t       *local_w,
						    CryptoData_t       *remote_p,
						    DiscoveredReader_t *dr,
						    int                relay_only,
						    DDS_ReturnCode_t   *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (relay_only)

	return (cps_register_disc_endpoint (cp, local_w, remote_p, 
							&dr->dr_ep, error));
}

/* cps_unregister_participant -- Unregister a Participant for crypto operations.*/

static DDS_ReturnCode_t cps_unregister_participant (const SEC_CRYPTO *cp,
						    CryptoData_t     *part)
{
	StdCrypto_t	*sp;
	EPKeyInfo_t	**kpp, *kip;

	ARG_NOT_USED (cp)

	sp = (StdCrypto_t *) part->data;
	if (!sp->local) {
		while (sl_length (&sp->ep_keys)) {
			kpp = (EPKeyInfo_t **) sl_head (&sp->ep_keys);
			if (!kpp || !*kpp)
				break;

			kip = *kpp;
			sl_delete (&sp->ep_keys, kip->eid.id, entity_id_cmp);
			xfree (kip);
		}
	}
	crypto_release (part->handle);
	return (DDS_RETCODE_OK);
}

/* cps_unregister_writer -- Unregister a DataWriter for crypto operations. */

static DDS_ReturnCode_t cps_unregister_writer (const SEC_CRYPTO *cp,
					       CryptoData_t     *w)
{
	ARG_NOT_USED (cp)

	crypto_release (w->handle);
	return (DDS_RETCODE_OK);
}

/* cps_unregister_reader -- Unregister a DataReader for crypto operations. */

static DDS_ReturnCode_t cps_unregister_reader (const SEC_CRYPTO *cp,
					       CryptoData_t     *r)
{
	ARG_NOT_USED (cp)

	crypto_release (r->handle);
	return (DDS_RETCODE_OK);
}


/*****************************************/
/*   2. Crypto key exchange functions.	 */
/*****************************************/

#if defined (CSTD_T_AES_TRACE)

static void dump_array (const char *name, const unsigned char *p, size_t length)
{
	unsigned	i;

	log_printf (SEC_ID, 0, "%s: ", name);
	for (i = 0; i < length; i++)
		log_printf (SEC_ID, 0, "%02x", p [i]);
	log_printf (SEC_ID, 0, "\r\n");
}

#endif

#define	ADD_4(buf,ofs,x)	*((uint32_t *) &buf [ofs]) = (x); (ofs) += 4
#define	ADD_N(buf,ofs,p,l)	memcpy (&buf [ofs], p, l); (ofs) += (l)
#define	SET_N(buf,ofs,l,v)	memset (&buf [ofs], v, l); (ofs) += (l)

static DDS_ReturnCode_t cps_make_token (DDS_CryptoToken *token,
					CipherKind_t    cipher,
					HashKind_t      hash,
					Master_t        *master,
					Session256_t    *encrypt)
{
	DDS_OctetSeq	*p;
	unsigned char	*dp;
	unsigned char	buffer [128];
	size_t		len, dlen;
	DDS_ReturnCode_t error;

	memset (token, 0, sizeof (DDS_CryptoToken));
	token->class_id = GMCLASSID_SECURITY_AES_CTR_HMAC;
	len = 0;

	/* 1. Create cleartext CDR-encoded KeyMaterial.
	   - - - - - - - - - - - - - - - - - - - - - - */

	/* Don't know if preamble is needed - lets assume so: */
	buffer [0] = 0;
	buffer [1] = (MODE_CDR << 1) | ENDIAN_CPU;
	buffer [2] = 0;
	buffer [3] = 0;
	len += 4;

	/* Add the actual token data. */
	ADD_4 (buffer, len, cipher);
	ADD_4 (buffer, len, hash);
	if (hash) {
		if (hash == HK_SHA256) {
			ADD_4 (buffer, len, master->m256.id);
			ADD_4 (buffer, len, 32);
			ADD_N (buffer, len, master->m256.master_key, 32);
			if (cipher) {
				ADD_4 (buffer, len, 32);
				ADD_N (buffer, len, master->m256.init_vector, 32);
			}
			else {
				ADD_4 (buffer, len, 0);
			}
			ADD_4 (buffer, len, 32);
			ADD_N (buffer, len, master->m256.hmac_key_id, 32);
		}
		else {
			ADD_4 (buffer, len, master->m128.id);
			ADD_4 (buffer, len, 16);
			ADD_N (buffer, len, master->m128.master_key, 16);
			if (cipher) {
				ADD_4 (buffer, len, 16);
				ADD_N (buffer, len, master->m128.init_vector, 16);
			}
			else {
				ADD_4 (buffer, len, 0);
			}
			ADD_4 (buffer, len, 16);
			ADD_N (buffer, len, master->m128.hmac_key_id, 16);
		}
	}
	else {
		ADD_4 (buffer, len, 0);	/* Id */
		ADD_4 (buffer, len, 0);	/* Master key. */
		ADD_4 (buffer, len, 0);	/* Init Vector. */
		ADD_4 (buffer, len, 0);	/* HMAC key id. */
	}


	/* 2. Encrypt CDR-encoded KeyMaterial.
	   - - - - - - - - - - - - - - - - - - */

	/* Allocate a new Octet sequence. */
	p = DDS_OctetSeq__alloc ();
	if (!p)
		goto no_mem;

	token->binary_value1 = p;
	error = dds_seq_require (p, len + 32);
	if (error)
		goto cleanup;

	dp = DDS_SEQ_DATA (*p);
	dlen = 0;

	/* Add secure CryptoTransformIdentifier. */
	ADD_4 (dp, dlen, 0x201);	/* AES256_HMAC_SHA256 */
	ADD_4 (dp, dlen, encrypt->id * 9812345);
	ADD_4 (dp, dlen, encrypt->id);

	/* Add session id/counter info. */
	ADD_4 (dp, dlen, encrypt->id);
	ADD_4 (dp, dlen, encrypt->counter);

	/* Add length of encrypted data. */
	ADD_4 (dp, dlen, len);

#ifdef CSTD_T_AES_TRACE
	log_printf (SEC_ID, 0, "Before encrypt: counter=0x%x\r\n", encrypt->counter);
	dump_array ("key", encrypt->key, 32);
	dump_array ("salt", encrypt->salt, 16);
	dump_array ("data", buffer, len);
#endif

	/* Add encrypted token data. */
	error = sec_aes256_ctr_crypt (encrypt->key,
				      32,
				      encrypt->salt,
				      &encrypt->counter,
				      buffer,
				      dp + dlen,
				      len);
	if (error)
		goto cleanup;

#ifdef CSTD_T_AES_TRACE
	log_printf (SEC_ID, 0, "After encrypt: counter=0x%x\r\n", encrypt->counter);
	dump_array ("key", encrypt->key, 32);
	dump_array ("salt", encrypt->salt, 16);
#endif
	dlen += len;

	/* Add empty common digest. */
	ADD_4 (dp, dlen, 0);

	/* Add empty additional digests. */
	ADD_4 (dp, dlen, 0);


	/* 3. Create HMAC of binary_value1 and set it as binary_value2. 
	   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	p = DDS_OctetSeq__alloc ();
	if (!p)
		goto no_mem;

	token->binary_value2 = p;
	error = dds_seq_require (p, 32);
	if (error)
		goto cleanup;

	error = sec_hmac_sha256 (encrypt->hmac,
				 32,
				 dp,
				 len + 32,
				 DDS_SEQ_DATA (*p));
	if (error)
		goto cleanup;

	return (DDS_RETCODE_OK);

    no_mem:
	error = DDS_RETCODE_OUT_OF_RESOURCES;

    cleanup:
	if (token->binary_value1) {
		DDS_OctetSeq__free (token->binary_value1);
		token->binary_value1 = NULL;
	}
	if (token->binary_value2) {
		DDS_OctetSeq__free (token->binary_value2);
		token->binary_value2 = NULL;
	}
	return (error);
}

static DDS_ReturnCode_t cps_get4 (const unsigned char *bp,
				  unsigned            ofs,
				  unsigned            max,
				  int                 swap,
				  uint32_t            *v)
{
	union {
		unsigned char d [4];
		uint32_t      v;
	} u;

	if (ofs + 4 > max)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (swap) {
		u.d [0] = bp [ofs + 3];
		u.d [1] = bp [ofs + 2];
		u.d [2] = bp [ofs + 1];
		u.d [3] = bp [ofs];
	}
	else {
		u.d [0] = bp [ofs];
		u.d [1] = bp [ofs + 1];
		u.d [2] = bp [ofs + 2];
		u.d [3] = bp [ofs + 3];
	}
	*v = u.v;
	return (DDS_RETCODE_OK);
}

#define	GET_4(bp,l,m,swap,v)	 if (cps_get4 (bp, l, m, swap, &v)) return (DDS_RETCODE_BAD_PARAMETER); l += 4

/* cps_get_token -- Get crypto key material from a crypto token. */

static DDS_ReturnCode_t cps_get_token (KeyMaterial_AES_CTR_HMAC *kmp,
				       DDS_CryptoToken          *token,
				       KxKeys_t                 *kkp,
				       Session256_t             *decrypt)
{
	unsigned char	*dp;
	uint32_t	id1, id2, id3, crypto_type, tlen, h, c, counter;
	unsigned	len, max;
	int		swap;
	unsigned char	buffer [128];
	Master256_t	master;
	DDS_ReturnCode_t error;

	dp = DDS_SEQ_DATA (*token->binary_value1);
	max = DDS_SEQ_LENGTH (*token->binary_value1);
	len = 0;

	/* Check if swapped, based on CryptoTransformIdentifier data. */
	GET_4 (dp, len, max, 0, crypto_type);
	if (crypto_type == 0x201)
		swap = 0;
	else if (crypto_type == 0x01020000UL)
		swap = 1;
	else
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/* Get CryptoTransformIdentifier data and verify it. */
	GET_4 (dp, len, max, swap, id1);
	GET_4 (dp, len, max, swap, id2);
	GET_4 (dp, len, max, swap, id3);
	GET_4 (dp, len, max, swap, counter);

	if (id1 != id2 * 9812345 || id2 != id3)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/* Update session data if peer has updated its session. */
	if (decrypt->id != id3) {

		/* Session update needed! */
		master.id = 0;
		memcpy (master.master_key, kkp->kx_key, 32);
		memcpy (master.hmac_key_id, kkp->kx_mac_key, 32);
		memset (master.init_vector, 0, 32);
		cps_init_session (HK_SHA256, CK_AES256,
				  (Master_t *) &master,
				  (Session_t *) decrypt, id3, 0);
	}

	/* Get length of encrypted token data. */
	GET_4 (dp, len, max, swap, tlen);
	if (tlen + 32 != max)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/* Verify HMAC of received data. */
	error = sec_hmac_sha256 (decrypt->hmac, 32, dp, max, buffer);
	if (error ||
	    DDS_SEQ_LENGTH (*token->binary_value2) != 32 ||
	    memcmp (buffer, DDS_SEQ_DATA (*token->binary_value2), 32))
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef CSTD_T_AES_TRACE
	log_printf (SEC_ID, 0, "Before decrypt: counter=0x%x\r\n", counter);
	dump_array ("key", decrypt->key, 32);
	dump_array ("salt", decrypt->salt, 16);
#endif
	error = sec_aes256_ctr_crypt (decrypt->key, 32, decrypt->salt, &counter,
						dp + len, buffer, tlen);
	if (error)
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef CSTD_T_AES_TRACE
	log_printf (SEC_ID, 0, "After decrypt: counter=0x%x\r\n", counter);
	dump_array ("key", decrypt->key, 32);
	dump_array ("salt", decrypt->salt, 16);
	dump_array ("data", buffer, tlen);
#endif

	/* Check the digests. */
	len += tlen;
	GET_4 (dp, len, max, swap, id2);
	GET_4 (dp, len, max, swap, id3);
	if (id2 || id3)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/* Extended CDR encoded data is in buffer now. */
	/* Check preamble. */
	if (buffer [0] ||
	    (buffer [1] != 0 && buffer [1] != 1) ||
	    buffer [2] ||
	    buffer [3])
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	swap = (buffer [1] != ENDIAN_CPU);
	len = 4;
	max = tlen;

	/* Get cipher/hash. */
	GET_4 (buffer, len, max, swap, c);
	GET_4 (buffer, len, max, swap, h);

	/* Check valid cipher/hash combinations: */
	if (c > CK_AES256 || h > HK_SHA256 || (c && h != c))
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	kmp->cipher_kind = c;
	kmp->hash_kind = h;

	/* Get Master Key. */
	GET_4 (buffer, len, max, swap, id1);
	kmp->master_key_id = id1;
	GET_4 (buffer, len, max, swap, tlen);
	if (h) {
		id2 = tlen;
		if (tlen != 16 && tlen != 32)
			return (DDS_RETCODE_BAD_PARAMETER);

		memcpy (kmp->master_key, buffer + len, tlen);
		len += tlen;
	}
	else if (tlen)
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Initialization Vector. */
	GET_4 (buffer, len, max, swap, tlen);
	if (c) {
		if (tlen != id2)
			return (DDS_RETCODE_BAD_PARAMETER);

		memcpy (kmp->initialization_vector, buffer + len, tlen);
		len += tlen;
	}

	/* Get HMAC key. */
	GET_4 (buffer, len, max, swap, tlen);
	if (h) {
		if (tlen != id2)
			return (DDS_RETCODE_BAD_PARAMETER);

		memcpy (kmp->hmac_key_id, buffer + len, tlen);
	}
	return (DDS_RETCODE_OK);
}

/* cps_create_loc_part_tokens -- Create local Participant crypto tokens. */

static DDS_ReturnCode_t cps_create_loc_part_tokens (const SEC_CRYPTO   *cp,
						    CryptoData_t       *local_p,
						    CryptoData_t       *remote_p,
						    DDS_CryptoTokenSeq *tokens)
{
	StdCrypto_t	*lp, *rp;
	DDS_ReturnCode_t error;

	ARG_NOT_USED (cp)

	DDS_SEQ_INIT (*tokens);
	error = dds_seq_require (tokens, 2);
	if (error)
		return (error);

	lp = (StdCrypto_t *) local_p->data;
	rp = (StdCrypto_t *) remote_p->data;
	error = cps_make_token (DDS_SEQ_ITEM_PTR (*tokens, 0),
			        lp->data_cipher,
			        lp->data_hash,
			        (lp->data_hash) ? lp->data_m : NULL,
			        rp->token_tx);
	if (error) {
		DDS_DataHolderSeq__clear (tokens);
		return (error);
	}
	error = cps_make_token (DDS_SEQ_ITEM_PTR (*tokens, 1),
			        rp->sign_hash,
			        CK_NONE,
			        rp->sign_tx_m,
			        rp->token_tx);
	if (error)
		DDS_DataHolderSeq__clear (tokens);

	return (error);
}

/* cps_set_master -- Set master crypto data from key material. */

static void cps_set_master (Master_t *mp, KeyMaterial_AES_CTR_HMAC *m)
{
	if (m->hash_kind == HK_SHA256) {
		mp->m256.id = m->master_key_id;
		memcpy (mp->m256.master_key, m->master_key, 32);
		memcpy (mp->m256.hmac_key_id, m->hmac_key_id, 32);
		if (m->cipher_kind)
			memcpy (mp->m256.init_vector, m->initialization_vector, 32);
	}
	else {
		mp->m128.id = m->master_key_id;
		memcpy (mp->m128.master_key, m->master_key, 16);
		memcpy (mp->m128.hmac_key_id, m->hmac_key_id, 16);
		if (m->cipher_kind)
			memcpy (mp->m128.init_vector, m->initialization_vector, 16);
	}
}

/* cps_new_master -- Add master and session crypto data for a peer rx. */

static size_t cps_new_master (Master_t                 **mpp,
			      Session_t                **spp,
			      KeyMaterial_AES_CTR_HMAC *m)
{
	Master_t	*mp;
	Session_t	*sp;
	size_t		msize, ssize;
	unsigned char	*p;

	cps_crypto_sizes (m->cipher_kind, m->hash_kind, &msize, &ssize, 0);
	if (!msize) {
		*mpp = NULL;
		*spp = NULL;
		return (0);
	}
	p = xmalloc (msize + ssize);
	if (!p)
		return (0);

	*mpp = mp = (Master_t *) p;
	*spp = sp = (Session_t *) (p + msize);
	memset (sp, 0, ssize);
	cps_set_master (mp, m);
	return (msize + ssize);
}

/* cps_parse_tokens -- Parse a set of tokens and return resulting keymaterial. */

static DDS_ReturnCode_t cps_parse_tokens (DDS_CryptoTokenSeq       *tokens,
					  KeyMaterial_AES_CTR_HMAC *t1,
					  KeyMaterial_AES_CTR_HMAC *t2,
				          KxKeys_t                 *kkp,
				          Session256_t             *decrypt)
{
	DDS_ReturnCode_t	error;

	if (DDS_SEQ_LENGTH (*tokens) < 2)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	error = cps_get_token (t1,
			       DDS_SEQ_ITEM_PTR (*tokens, 0),
			       kkp,
			       decrypt);
	if (error) {
		warn_printf ("Remote token #0 error: %s!", DDS_error (error));
		return (error);
	}
	error = cps_get_token (t2,
			       DDS_SEQ_ITEM_PTR (*tokens, 1),
			       kkp,
			       decrypt);
	if (error) {
		warn_printf ("Remote token #1 error: %s!", DDS_error (error));
		return (error);
	}
	if (t1->cipher_kind && t2->cipher_kind)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	return (DDS_RETCODE_OK);
}

/* cps_set_rem_part_tokens -- Set peer Participant crypto tokens. */

static DDS_ReturnCode_t cps_set_rem_part_tokens (const SEC_CRYPTO   *cp,
						 CryptoData_t       *local_p,
						 CryptoData_t       *remote_p,
						 DDS_CryptoTokenSeq *tokens)
{
	KeyMaterial_AES_CTR_HMAC t1, t2;
	StdCrypto_t		 *sp;
	size_t			 s;
	DDS_ReturnCode_t	 error;

	ARG_NOT_USED (cp)
	ARG_NOT_USED (local_p)

	sp = (StdCrypto_t *) remote_p->data;
	error = cps_parse_tokens (tokens, &t1, &t2, &sp->kx_keys, sp->token_rx);
	if (error)
		return (error);

	if (!t1.cipher_kind && t2.cipher_kind) {
		s = cps_new_master (&sp->data_m, &sp->data_s, &t2);
		if (!s)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		sp->tcsize += s;
		sec_crypt_alloc += s;
		sp->data_hash = t2.hash_kind;
		sp->data_cipher = t2.cipher_kind;
		s = cps_new_master (&sp->sign_rx_m, &sp->sign_rx_s, &t1);
		if (!s) {
			sp->sign_hash = t1.hash_kind;
			error = DDS_RETCODE_OUT_OF_RESOURCES;
		}
		else
			error = DDS_RETCODE_OK;
		sp->tcsize += s;
		sec_crypt_alloc += s;
	}
	else if (t1.cipher_kind && !t2.cipher_kind) {
		s = cps_new_master (&sp->data_m, &sp->data_s, &t1);
		if (!s)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		sp->tcsize += s;
		sec_crypt_alloc += s;
		sp->data_hash = t1.hash_kind;
		sp->data_cipher = t1.cipher_kind;
		s = cps_new_master (&sp->sign_rx_m, &sp->sign_rx_s, &t2);
		if (!s) {
			sp->sign_hash = t2.hash_kind;
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else
			error = DDS_RETCODE_OK;
		sp->tcsize += s;
		sec_crypt_alloc += s;
	}
	else
		error = DDS_RETCODE_PRECONDITION_NOT_MET;
	return (error);
}

/* cps_create_local_ep_tokens -- Create local endpoint crypto tokens. */

static DDS_ReturnCode_t cps_create_local_ep_tokens (CryptoData_t       *lcp,
						    CryptoData_t       *rcp,
						    DDS_CryptoTokenSeq *tokens)
{
	StdCrypto_t	 *lp, *rp, *pp;
	CryptoData_t	 *pcp;
	Participant_t	 *rpp;
	DDS_ReturnCode_t error;

	rpp = rcp->parent.endpoint->u.participant;
	if (!rpp)
		return (DDS_RETCODE_ALREADY_DELETED);

	pcp = crypto_lookup (rpp->p_crypto);
	if (!pcp)
		return (DDS_RETCODE_ALREADY_DELETED);

	DDS_SEQ_INIT (*tokens);
	error = dds_seq_require (tokens, 2);
	if (error)
		return (error);

	lp = (StdCrypto_t *) lcp->data;
	rp = (StdCrypto_t *) rcp->data;
	pp = (StdCrypto_t *) pcp->data;

	error = cps_make_token (DDS_SEQ_ITEM_PTR (*tokens, 0),
			        lp->data_cipher,
			        lp->data_hash,
			        lp->data_m,
			        pp->token_tx);
	if (error) {
		DDS_DataHolderSeq__clear (tokens);
		return (error);
	}
	error = cps_make_token (DDS_SEQ_ITEM_PTR (*tokens, 1),
			        CK_NONE,
			        rp->sign_hash,
			        rp->sign_tx_m,
			        pp->token_tx);
	if (error)
		DDS_DataHolderSeq__clear (tokens);

	return (error);
}

/* cps_create_writer_tokens -- Create local DataWriter crypto tokens. */

static DDS_ReturnCode_t cps_create_writer_tokens (const SEC_CRYPTO   *cp,
						  CryptoData_t       *local_w,
						  CryptoData_t       *remote_r,
						  DDS_CryptoTokenSeq *tokens)
{
	ARG_NOT_USED (cp)

	return (cps_create_local_ep_tokens (local_w, remote_r, tokens));
}

/* cps_create_reader_tokens -- Create local DataReader crypto tokens. */

static DDS_ReturnCode_t cps_create_reader_tokens (const SEC_CRYPTO   *cp,
						  CryptoData_t       *local_r,
						  CryptoData_t       *remote_w,
						  DDS_CryptoTokenSeq *tokens)
{
	ARG_NOT_USED (cp)

	return (cps_create_local_ep_tokens (local_r, remote_w, tokens));
}

/* cps_add_ep_keys -- Save crypto keys at the peer participant. */

static EPKeyInfo_t *cps_add_ep_keys (StdCrypto_t              *pp,
				     EntityId_t               *eidp,
				     KeyMaterial_AES_CTR_HMAC *dkm,
				     KeyMaterial_AES_CTR_HMAC *skm)
{
	EPKeyInfo_t	**kpp, *kip = NULL, tmp;
	unsigned char	*xp;
	size_t		dm_size, ds_size, sm_size, ss_size, s;
	int		is_new;

	kpp = sl_insert (&pp->ep_keys, eidp->id, &is_new, entity_id_cmp);
	if (!kpp) {
		warn_printf ("Security: can't store Endpoint key info!");
		return (NULL);
	}
	if (!is_new) {
		kip = *kpp;
		if (kip->data_cipher != dkm->cipher_kind ||
		    kip->data_hash != dkm->hash_kind ||
		    kip->sign_hash != skm->hash_kind) {
			*kpp = NULL;
			xfree (kip);
			is_new = 1;
		}
	}
	if (is_new) {
		cps_crypto_sizes (dkm->cipher_kind, dkm->hash_kind,
							&dm_size, &ds_size, 0);
		cps_crypto_sizes (CK_NONE, skm->hash_kind,
							&sm_size, &ss_size, 0);
		s = sizeof (EPKeyInfo_t) + dm_size + ds_size + sm_size + ss_size;
		kip = xmalloc (s);
		if (!kip) {
			tmp.eid = *eidp;
			*kpp = &tmp;
			sl_delete (&pp->ep_keys, eidp->id, entity_id_cmp);
			warn_printf ("Security: out of memory storing peer crypto material!");
			return (NULL);
		}
		*kpp = kip;
		pp->tcsize += s;
		sec_crypt_alloc += s;
		xp = (unsigned char *) kip + sizeof (EPKeyInfo_t);
		kip->eid = *eidp;
		kip->data_cipher = dkm->cipher_kind;
		kip->data_hash = dkm->hash_kind;
		kip->sign_hash = skm->hash_kind;

		kip->data_master = (Master_t *) xp;
		xp += dm_size;
		kip->data_session = (Session_t *) xp;
		memset (xp, 0, ds_size);
		xp += ds_size;
		kip->sign_master = (Master_t *) xp;
		xp += sm_size;
		kip->sign_session = (Session_t *) xp;
		memset (xp, 0, ss_size);
	}
	cps_set_master (kip->data_master, dkm);
	cps_set_master (kip->sign_master, skm);
	return (kip);
}

/* cps_set_rem_ep_tokens -- Set peer endpoint crypto tokens. */

static DDS_ReturnCode_t cps_set_rem_ep_tokens (CryptoData_t       *lcp,
					       CryptoData_t       *rcp,
					       DDS_CryptoTokenSeq *tokens)
{
	KeyMaterial_AES_CTR_HMAC t1, t2;
	CryptoData_t		 *pcp;
	StdCrypto_t		 *rp, *pp;
	EPKeyInfo_t		 *kip;
	EntityId_t		 *eidp;
	DDS_ReturnCode_t	 error;

	ARG_NOT_USED (lcp)

	rp = (StdCrypto_t *) rcp->data;
	pcp = crypto_lookup (rcp->parent.endpoint->u.participant->p_crypto);
	if (!pcp)
		return (DDS_RETCODE_ALREADY_DELETED);

	pp = (StdCrypto_t *) pcp->data;
	error = cps_parse_tokens (tokens, &t1, &t2, &pp->kx_keys, pp->token_rx);
	if (error)
		return (error);

	eidp = &rcp->parent.endpoint->entity_id;
	if (!t1.cipher_kind && t2.cipher_kind)
		kip = cps_add_ep_keys (pp, eidp, &t2, &t1);
	else
		kip = cps_add_ep_keys (pp, eidp, &t1, &t2);

	if (!kip)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	cps_disc_endpoint_set_keys (rp, kip);
	return (DDS_RETCODE_OK);
}

/* cps_set_disc_writer_tokens -- Set the crypto tokens for a discovered writer.*/

static DDS_ReturnCode_t cps_set_disc_writer_tokens (const SEC_CRYPTO   *cp,
						    CryptoData_t       *local,
						    CryptoData_t       *remote,
						    DDS_CryptoTokenSeq *tokens)
{
	ARG_NOT_USED (cp)

	return (cps_set_rem_ep_tokens (local, remote, tokens));
}

/* cps_set_disc_reader_tokens -- Set the crypto tokens for a discovered reader.*/

static DDS_ReturnCode_t cps_set_disc_reader_tokens (const SEC_CRYPTO   *cp,
						    CryptoData_t       *local,
						    CryptoData_t       *remote,
						    DDS_CryptoTokenSeq *tokens)
{
	ARG_NOT_USED (cp)

	return (cps_set_rem_ep_tokens (local, remote, tokens));
}

/* cps_remember_rem_ep_tokens -- Remember peer endpoint crypto material. */

static DDS_ReturnCode_t cps_remember_rem_ep_tokens (CryptoData_t *lcp,
						    CryptoData_t *pcp,
						    EntityId_t   *eid,
						    DDS_CryptoTokenSeq *tokens)
{
	KeyMaterial_AES_CTR_HMAC t1, t2;
	StdCrypto_t		 *pp;
	EPKeyInfo_t		 *kip;
	DDS_ReturnCode_t	 error;

	ARG_NOT_USED (lcp)

	pp = (StdCrypto_t *) pcp->data;
	error = cps_parse_tokens (tokens, &t1, &t2, &pp->kx_keys, pp->token_rx);
	if (error)
		return (error);

	if (!t1.cipher_kind && t2.cipher_kind)
		kip = cps_add_ep_keys (pp, eid, &t2, &t1);
	else
		kip = cps_add_ep_keys (pp, eid, &t1, &t2);

	if (!kip)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}

/* cps_remember_disc_writer_tokens -- Remember crypto tokens for a not yet
				      discovered DataWriter. */

static DDS_ReturnCode_t cps_remember_disc_writer_tokens (const SEC_CRYPTO    *cp,
							 CryptoData_t        *local_r,
						         CryptoData_t        *rem_p,
							 EntityId_t          *eid,
							 DDS_CryptoTokenSeq *tokens)
{
	ARG_NOT_USED (cp)

	return (cps_remember_rem_ep_tokens (local_r, rem_p, eid, tokens));
}

/* cps_remember_disc_reader_tokens -- Remember crypto tokens for a not yet
				      discovered DataReader. */

static DDS_ReturnCode_t cps_remember_disc_reader_tokens (const SEC_CRYPTO    *cp,
							 CryptoData_t        *local_w,
						         CryptoData_t        *rem_p,
							 EntityId_t          *eid,
							 DDS_CryptoTokenSeq *tokens)
{
	ARG_NOT_USED (cp)

	return (cps_remember_rem_ep_tokens (local_w, rem_p, eid, tokens));
}

/*************************************/
/*   3. Crypto encoding functions.   */
/*************************************/

/* cps_encrypt_payload -- Encode payload data and return the encrypted data. */

static DB *cps_encrypt_payload (const SEC_CRYPTO *cp,
			        DBW              *data,
			        CryptoData_t     *sender,
			        size_t           *enc_len,
			        DDS_ReturnCode_t *error)
{
	StdCrypto_t	*scp;
	Session128_t	*d128 = NULL;
	Session256_t	*d256 = NULL;
	DB		*dbp;
	unsigned char	*dp;
	size_t		len, dlen;
	unsigned	id, type, c;
	int		ret;

	ARG_NOT_USED (cp)

	scp = (StdCrypto_t *) sender->data;
	len = ((data->length + 3) & ~3) + 32;
	if (scp->data_hash == HK_SHA1)
		len += 20;
	else
		len += 32;
	dbp = db_alloc_data (len, 1);
	if (!dbp) {
		*error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	dlen = 0;
	dp = dbp->data;
	if (scp->data_cipher == CK_AES256) {
		type = 0x201;
		d256 = (Session256_t *) scp->data_s;
		if (!d256->id || d256->counter >= max_blocks_per_session) {
			ret = cps_init_session (scp->data_hash,
						scp->data_cipher,
						scp->data_m,
						scp->data_s,
						next_id++,
						1);
			if (ret) {
				*error = ret;
				db_free_data (dbp);
				return (NULL);
			}
		}
		id = d256->id;
		c = d256->counter;
	}
	else if (scp->data_cipher == CK_AES128) {
		type = 0x200;
		d128 = (Session128_t *) scp->data_s;
		if (!d128->id || d128->counter >= max_blocks_per_session) {
			ret = cps_init_session (scp->data_hash,
						scp->data_cipher,
						scp->data_m,
						scp->data_s,
						next_id++,
						1);
			if (ret) {
				*error = ret;
				db_free_data (dbp);
				return (NULL);
			}
		}
		id = d128->id;
		c = d128->counter;
	}
	else if (scp->data_hash == HK_SHA256) {
		type = 0x101;
		d256 = (Session256_t *) scp->data_s;
		id = d256->id;
		c = 0;
	}
	else if (scp->data_hash == HK_SHA1) {
		type = 0x100;
		d128 = (Session128_t *) scp->data_s;
		id = d128->id;
		c = 0;
	}
	else {
		db_free_data (dbp);
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}

	/* Add secure CryptoTransformIdentifier. */
	ADD_4 (dp, dlen, type);
	ADD_4 (dp, dlen, id * 9812345);
	ADD_4 (dp, dlen, id);

	/* Add session id/counter info. */
	ADD_4 (dp, dlen, id);
	ADD_4 (dp, dlen, c);

	/* Add length of encrypted/embedded data. */
	ADD_4 (dp, dlen, data->length);

	/* Add either encrypted data (AES128 or AES256) or vanilla data. */
	if (scp->data_cipher == CK_AES256) {
		if (data->length <= data->left)
			ret = sec_aes256_ctr_crypt (d256->key,
						    32,
						    d256->salt,
						    &d256->counter,
						    data->data,
						    dp + dlen,
						    data->length);
		else
			ret = sec_aes256_ctr_crypt_dbw (d256->key,
							32,
							d256->salt,
							&d256->counter,
							data,
							dp + len);
		if (ret) {
			db_free_data (dbp);
			*error = ret;
			return (NULL);
		}
	}
	else if (scp->data_cipher == CK_AES128) {
		if (data->length <= data->left)
			ret = sec_aes128_ctr_crypt (d128->key,
						    16,
						    d128->salt,
						    &d128->counter,
						    data->data,
						    dp + dlen,
						    data->length);
		else
			ret = sec_aes128_ctr_crypt_dbw (d128->key,
							16,
							d128->salt,
							&d128->counter,
							data,
							dp + len);
		if (ret) {
			db_free_data (dbp);
			*error = ret;
			return (NULL);
		}
	}
	else
		db_get_data (dp + dlen, data->dbp, data->data, 0, data->length);

	dlen += data->length;

	/* Add common digest. */
	while ((dlen & 3) != 0)	/* Align up. */
		dp [dlen++] = 0;
	if (scp->data_hash == HK_SHA1) {
		ADD_4 (dp, dlen, 20);
		ret = sec_hmac_sha1 (d128->hmac,
				     16,
				     dp,
				     data->length + 24,
				     dp + dlen);
		if (ret) {
			db_free_data (dbp);
			*error = ret;
			return (NULL);
		}
		dlen += 20;
	}
	else if (scp->data_hash == HK_SHA256) {
		ADD_4 (dp, dlen, 32);
		ret = sec_hmac_sha256 (d256->hmac,
				       32,
				       dp,
				       data->length + 24,
				       dp + dlen);
		if (ret) {
			db_free_data (dbp);
			*error = ret;
			return (NULL);
		}
		dlen += 32;
	}

	/* Add empty additional digests. */
	ADD_4 (dp, dlen, 0);
	*enc_len = dlen;
	*error = DDS_RETCODE_OK;
	return (dbp);
}

/* cps_encrypt_writer_submsg -- Encode a DataWriter submessage and return the
				secure submessage. */

static RME *cps_encrypt_writer_submsg (const SEC_CRYPTO *cp,
				       RME              *submsg,
				       CryptoData_t     *sender,
				       CryptoData_t     *receivers [],
				       unsigned         nreceivers,
				       DDS_ReturnCode_t *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (submsg)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (receivers)
	ARG_NOT_USED (nreceivers)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (NULL);
}

/* cps_encrypt_reader_submsg -- Encode a DataReader submessage and return the
				secure submessage. */

static RME *cps_encrypt_reader_submsg (const SEC_CRYPTO *cp,
				       RME              *submsg,
				       CryptoData_t     *sender,
				       CryptoData_t     *receiver,
				       DDS_ReturnCode_t *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (submsg)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (receiver)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (NULL);
}

/* cps_encrypt_message -- Encode a complete RTPS message and return a new
			  message with encrypted contents. */

static RMBUF *cps_encrypt_message (const SEC_CRYPTO *cp,
				   RMBUF            *rtps_message,
				   CryptoData_t     *sender,
				   CryptoData_t     *receivers [],
				   unsigned         nreceivers,
				   DDS_ReturnCode_t *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (rtps_message)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (receivers)
	ARG_NOT_USED (nreceivers)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (NULL);
}


/*************************************/
/*   4. Crypto decoding functions.   */
/*************************************/

/* cps_get_data -- Get a linearized data chunk from received message data. */

static const unsigned char *cps_get_data (DBW *data, size_t len, unsigned char *buf)
{
	const unsigned char	*dp;
	unsigned char		*dbuf;
	const DB		*rdp;
	size_t			left, dlen, n;

	left = data->left;
	if (left >= len)
		return (data->data);

	/* Data region is fragmented, copy data, fragment by fragment to buf. */
	dbuf = buf;
	dlen = len;
	dp = data->data;
	rdp = data->dbp;
	for (;;) {
		if (dlen > left)
			n = left;
		else
			n = len;
		memcpy (dbuf, dp, n);
		dbuf += n;
		dlen -= n;
		if (!dlen)
			break;

		rdp = rdp->next;
		dp = rdp->data;
		left = rdp->size;
	}
	return (buf);
}

#define	GETP_4(bp,l,m,swap,v,e)	 if (cps_get4 (bp, l, m, swap, &v)) { *e = DDS_RETCODE_BAD_PARAMETER; return (NULL); } l += 4

/* cps_decrypt_payload -- Decrypt a serialized data submessage element. */

static DB *cps_decrypt_payload (const SEC_CRYPTO *cp,
				DBW              *encoded,
				CryptoData_t     *receiver,
				CryptoData_t     *sender,
				size_t           *dec_len,
				size_t           *db_ofs,
				DDS_ReturnCode_t *error)
{
	StdCrypto_t		*scp;
	Session128_t		*d128;
	Session256_t		*d256;
	DB			*ndb;
	const unsigned char	*dp;
	size_t			len, hlen, max;
	uint32_t		crypto_type, exp_type, x1, x2, session_id, counter, dlen, xlen;
	int			swap, ret;
	unsigned char		buffer [36];
	unsigned char		hash [32];

	ARG_NOT_USED (cp)
	ARG_NOT_USED (receiver)

	scp = (StdCrypto_t *) sender->data;
	if (!scp || encoded->length < 33) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}

	/* Get linearized crypto header. */
	dp = cps_get_data (encoded, 24, buffer);

	/* Verify expected crypto type. */
	len = 0;
	max = encoded->length;
	GETP_4 (dp, len, max, 0, crypto_type, error);
	if (crypto_type <= 0x201)
		swap = 0;
	else {
		swap = 1;
		len = 0;
		GETP_4 (dp, len, max, swap, crypto_type, error);
	}
	if (scp->data_cipher == CK_AES256) {
		exp_type = 0x201;
		hlen = 32;
	}
	else if (scp->data_cipher == CK_AES128) {
		exp_type = 0x200;
		hlen = 20;
	}
	else if (scp->data_hash == HK_SHA256) {
		exp_type = 0x101;
		hlen = 32;
	}
	else if (scp->data_hash == HK_SHA1) {
		exp_type = 0x100;
		hlen = 20;
	}
	else {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (crypto_type != exp_type) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}

	/* Get the rest of the header. */
	GETP_4 (dp, len, max, swap, x1, error);
	GETP_4 (dp, len, max, swap, x2, error);
	GETP_4 (dp, len, max, swap, session_id, error);
	GETP_4 (dp, len, max, swap, counter, error);
	GETP_4 (dp, len, max, swap, dlen, error);

	/* Check if data length is correct. */
	if (dlen + 24 + 8 + hlen > encoded->length) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	*dec_len = dlen;

	/* Get hash of header + data. */
	if (scp->data_hash == HK_SHA1) {
		d128 = (Session128_t *) scp->data_s;
		d256 = NULL;
		if (d128->id != session_id)
			cps_init_session (HK_SHA1, scp->data_cipher, 
				scp->data_m, scp->data_s, session_id, 0);
		if (encoded->left >= dlen + 24)
			ret = sec_hmac_sha1 (d128->hmac, 16,
					     encoded->data, dlen + 24, hash);
		else {
			xlen = encoded->length;
			encoded->length = dlen + 24;
			ret = sec_hmac_sha1_dbw (d128->hmac, 16,
						 encoded, hash);
			encoded->length = xlen;
		}
	}
	else {
		d256 = (Session256_t *) scp->data_s;
		d128 = NULL;
		if (d256->id != session_id)
			cps_init_session (HK_SHA256, scp->data_cipher, 
				scp->data_m, scp->data_s, session_id, 0);
		if (encoded->left >= dlen + 24)
			ret = sec_hmac_sha256 (d256->hmac, 16,
					     encoded->data, dlen + 24, hash);
		else {
			xlen = encoded->length;
			encoded->length = dlen + 24;
			ret = sec_hmac_sha256_dbw (d256->hmac, 16,
						 encoded, hash);
			encoded->length = xlen;
		}
	}
	if (ret) {
		*error = ret;
		return (NULL);
	}

	/* Point walker to start of encrypted/signed data area. */
	DBW_INC (*encoded, 24);

	/* Get new buffer for decrypted data. */
	if (scp->data_cipher) {
		ndb = db_alloc_data (dlen, 1);
		if (!ndb) {
			   *error = DDS_RETCODE_OUT_OF_RESOURCES;
			   return (NULL);
		}
		*db_ofs = 0;
	}
	else {
		ndb = (DB *) encoded->dbp;
		*db_ofs = encoded->data - ndb->data;
	}

	/* Decrypt data if it's encrypted. */
	if (scp->data_cipher == CK_AES256) {
		if (dlen <= encoded->left)
			ret = sec_aes256_ctr_crypt (d256->key,
						    32,
						    d256->salt,
						    &counter,
						    encoded->data,
						    ndb->data,
						    dlen);
		else {
			xlen = encoded->length;
			encoded->length = dlen;
			ret = sec_aes256_ctr_crypt_dbw (d256->key,
							32,
							d256->salt,
							&counter,
							encoded,
							ndb->data);
			encoded->length = xlen;
		}
	}
	else if (scp->data_cipher == CK_AES128) {
		if (dlen <= encoded->left)
			ret = sec_aes128_ctr_crypt (d128->key,
						    16,
						    d128->salt,
						    &counter,
						    encoded->data,
						    ndb->data,
						    dlen);
		else {
			xlen = encoded->length;
			encoded->length = dlen;
			ret = sec_aes128_ctr_crypt_dbw (d128->key,
							16,
							d128->salt,
							&counter,
							encoded,
							ndb->data);
			encoded->length = xlen;
		}
	}
	if (ret) {
		*error = ret;
		return (NULL);
	}

	dlen = (dlen + 3) & ~3;	/* Align up. */
	DBW_INC (*encoded, dlen);

	/* Verify authenticity of message via HMAC. */
	dp = cps_get_data (encoded, 4 + hlen, buffer);
	len = 0;
	max = 4 + hlen;
	GETP_4 (dp, len, max, swap, xlen, error);
	if (xlen != hlen || memcmp (dp + 4, hash, hlen)) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	return (ndb);
}

/* cps_preprocess_submsg -- Preprocess a secure submessage to determine whether
			    it is an encoded DataReader or DataWriter submessage.
			    Depending on the result, either *dwcrypto or
			    *drcrypto will be set and one of the following
			    decrypt functions should be called. */

static SubmsgCategory_t cps_preprocess_submsg (const SEC_CRYPTO   *cp,
					       RME                *submsg,
					       CryptoData_t       *receiver,
					       CryptoData_t       *sender,
					       DataWriterCrypto_t *dwcrypto,
					       DataReaderCrypto_t *drcrypto,
					       DDS_ReturnCode_t   *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (submsg)
	ARG_NOT_USED (receiver)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (dwcrypto)
	ARG_NOT_USED (drcrypto)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (0);
}

/* cps_decrypt_writer_submsg -- Decrypt a DataWriter submessage. */

static RME *cps_decrypt_writer_submsg (const SEC_CRYPTO *cp,
				       RME              *submsg,
				       CryptoData_t     *receiver,
				       CryptoData_t     *sender,
				       DDS_ReturnCode_t *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (submsg)
	ARG_NOT_USED (receiver)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (NULL);
}

/* cps_decrypt_reader_submsg -- Decrypt a DataReader submessage. */

static RME *cps_decrypt_reader_submsg (const SEC_CRYPTO *cp,
				       RME              *submsg,
				       CryptoData_t     *receiver,
				       CryptoData_t     *sender,
				       DDS_ReturnCode_t *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (submsg)
	ARG_NOT_USED (receiver)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (NULL);
}

/* cps_decrypt_message -- Decrypt a complete RTPS message from a secure
			  submessage element. */

static RMBUF *cps_decrypt_message (const SEC_CRYPTO *cp,
				   RME              *submsg,
				   CryptoData_t     *receiver,
				   CryptoData_t     *sender,
				   DDS_ReturnCode_t *error)
{
	ARG_NOT_USED (cp)
	ARG_NOT_USED (submsg)
	ARG_NOT_USED (receiver)
	ARG_NOT_USED (sender)
	ARG_NOT_USED (error)

	/* ... TBC ... */

	*error = DDS_RETCODE_UNSUPPORTED;
	return (NULL);
}

#ifdef DDS_DEBUG

static void dump_master (Master_t *mp, int full, size_t length)
{
	unsigned	i;

	if (length == 32) {
		dbg_printf ("\tM.Id:   %u,\r\n\tM.Key:  ", mp->m256.id);
		for (i = 0; i < length; i++)
			dbg_printf ("%02x", mp->m256.master_key [i]);
		if (full) {
			dbg_printf ("\r\n\tM.IV:   ");
			for (i = 0; i < length; i++)
				dbg_printf ("%02x", mp->m256.init_vector [i]);
		}
		dbg_printf ("\r\n\tM.HMAC: ");
		for (i = 0; i < 32; i++)
			dbg_printf ("%02x", mp->m256.hmac_key_id [i]);
	}
	else {
		dbg_printf ("\tM.Id:   %u,\r\n\tM.Key:  ", mp->m128.id);
		for (i = 0; i < length; i++)
			dbg_printf ("%02x", mp->m128.master_key [i]);
		if (full) {
			dbg_printf ("\r\n\tM.IV:   ");
			for (i = 0; i < length; i++)
				dbg_printf ("%02x", mp->m128.init_vector [i]);
		}
		dbg_printf ("\r\n\tM.HMAC: ");
		for (i = 0; i < 16; i++)
			dbg_printf ("%02x", mp->m128.hmac_key_id [i]);
	}
	dbg_printf ("\r\n");
}

static void dump_session128 (Session128_t *sp, int full, int tx)
{
	unsigned	i;

	dbg_printf ("\tS.Id:   %u\r\n\t", sp->id);
	if (full) {
		dbg_printf ("S.Key:  ");
		for (i = 0; i < 16; i++)
			dbg_printf ("%02x", sp->key [i]);
		dbg_printf ("\r\n\tS.Salt: ");
		for (i = 0; i < 16; i++)
			dbg_printf ("%02x", sp->salt [i]);
		dbg_printf ("\r\n\t");
	}
	dbg_printf ("S.HMAC: ");
	for (i = 0; i < 16; i++)
		dbg_printf ("%02x", sp->hmac [i]);
	dbg_printf ("\r\n");
	if (tx && full)
		dbg_printf ("\tS.Count:%u\r\n", sp->counter);
}

static void dump_session256 (Session256_t *sp, int full, int tx)
{
	unsigned	i;

	dbg_printf ("\tS.Id:   %u\r\n\t", sp->id);
	if (full) {
		dbg_printf ("S.Key:  ");
		for (i = 0; i < 32; i++)
			dbg_printf ("%02x", sp->key [i]);
		dbg_printf ("\r\n\tS.Salt: ");
		for (i = 0; i < 16; i++)
			dbg_printf ("%02x", sp->salt [i]);
		dbg_printf ("\r\n\t");
	}
	dbg_printf ("S.HMAC: ");
	for (i = 0; i < 32; i++)
		dbg_printf ("%02x", sp->hmac [i]);
	dbg_printf ("\r\n");
	if (tx && full)
		dbg_printf ("\tS.Count:%u\r\n", sp->counter);
}

static void cps_dump_crypto (const char *name,
			     int        tx,
			     unsigned   hash,
			     unsigned   cipher,
			     Master_t   *mp,
			     Session_t  *sp)
{
	if (!hash) {
		dbg_printf ("%s: No crypto enabled.\r\n", name);
		return;
	}
	dbg_printf ("%s: ", name);
	if (cipher == CK_AES256) {
		dbg_printf ("AES256/HMAC-SHA256 encrypted/signed data.\r\n");
		dump_master (mp, 1, 32);
		dump_session256 ((Session256_t *) sp, 1, tx);
	}
	else if (cipher == CK_AES128) {
		dbg_printf ("AES128/HMAC-SHA1 encrypted/signed data.\r\n");
		dump_master (mp, 1, 16);
		dump_session128 ((Session128_t *) sp, 1, tx);
	}
	else if (hash == HK_SHA256) {
		dbg_printf ("HMAC-SHA256 signed data.\r\n");
		dump_master (mp, 0, 32);
		dump_session256 ((Session256_t *) sp, 0, tx);
	}
	else if (hash == HK_SHA1) {
		dbg_printf ("HMAC-SHA1 signed data.\r\n");
		dump_master (mp, 0, 16);
		dump_session128 ((Session128_t *) sp, 0, tx);
	}
}

static void cps_dump_source (CryptoData_t *dp)
{
	StdCrypto_t	*sp = (StdCrypto_t *) dp->data;

	if (sp->data_hash)
		cps_dump_crypto ("TxData", 1, sp->data_hash, sp->data_cipher, sp->data_m, sp->data_s);
	else
		dbg_printf ("Txdata: Crypto not enabled.\r\n");
}

static int cps_visit_kip (Skiplist_t *list, void *node, void *arg)
{
	EPKeyInfo_t	**kpp = (EPKeyInfo_t **) node, *kip;
	char		buf [16];

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	kip = *kpp;
	dbg_printf (" [%s]:\r\n", entity_id_str (&kip->eid, buf));
	cps_dump_crypto ("  Data", 0, kip->data_hash, kip->data_cipher,
					kip->data_master, kip->data_session);
	cps_dump_crypto ("  Sign", 0, kip->sign_hash, CK_NONE,
					kip->sign_master, kip->sign_session);
	return (1);
}

static void cps_dump_participant (CryptoData_t *dp)
{
	StdCrypto_t	*sp = (StdCrypto_t *) dp->data;
	unsigned	i;

	if (sp->data_hash)
		cps_dump_crypto ("RxData", 0, sp->data_hash, sp->data_cipher, sp->data_m, sp->data_s);
	else
		dbg_printf ("RxData: Crypto not enabled.\r\n");
	if (sp->sign_hash)
		cps_dump_crypto ("TxSign", 1, sp->sign_hash, CK_NONE, sp->sign_tx_m, sp->sign_tx_s);
	else
		dbg_printf ("TxSign: Hash not enabled.\r\n");
	if (sp->rem_hash)
		cps_dump_crypto ("RxSign", 0, sp->rem_hash, CK_NONE, sp->sign_rx_m, sp->sign_rx_s);
	else
		dbg_printf ("RxSign: Hash not enabled.\r\n");
	dbg_printf ("KxKeys:\tM.Key:  ");
	for (i = 0; i < 32; i++)
		dbg_printf ("%02x", sp->kx_keys.kx_key [i]);
	dbg_printf ("\r\n\tM.Mac:  ");
	for (i = 0; i < 32; i++)
		dbg_printf ("%02x", sp->kx_keys.kx_mac_key [i]);
	dbg_printf ("\r\n");
	dbg_printf ("TToken: AES256/HMAC-SHA256 encrypted/signed data.\r\n");
	dump_session256 (sp->token_tx, 1, 1);
	dbg_printf ("RToken: AES256/HMAC-SHA256 encrypted/signed data.\r\n");
	dump_session256 (sp->token_rx, 1, 0);
	dbg_printf ("RxKeys:\r\n");
	if (sl_length (&sp->ep_keys))
		sl_walk (&sp->ep_keys, cps_visit_kip, NULL);
	dbg_printf ("Keysize: %lu bytes.\r\n", (unsigned long) sp->tcsize);
}

static void cps_dump_discovered_endpoint (CryptoData_t *dp)
{
	StdCrypto_t	*sp = (StdCrypto_t *) dp->data;

	if (sp->data_hash)
		cps_dump_crypto ("RxData", 0, sp->data_hash, sp->data_cipher, sp->data_m, sp->data_s);
	else
		dbg_printf ("RxData: Crypto not enabled.\r\n");
	if (sp->sign_hash)
		cps_dump_crypto ("TxSign", 1, sp->sign_hash, CK_NONE, sp->sign_tx_m, sp->sign_tx_s);
	else
		dbg_printf ("TxSign: Hash not enabled.\r\n");
	if (sp->rem_hash)
		cps_dump_crypto ("RxSign", 0, sp->rem_hash, CK_NONE, sp->sign_rx_m, sp->sign_rx_s);
	else
		dbg_printf ("RxSign: Hash not enabled.\r\n");
}

/* cps_dump -- Dump cryptographic material. */

static void cps_dump (const SEC_CRYPTO *cp,
		      CryptoData_t     *dp,
		      Entity_t         *p)
{
	ARG_NOT_USED (cp)

	if (entity_type (p) == ET_PARTICIPANT)
		if ((entity_flags (p) & EF_LOCAL) != 0) {
			cps_dump_source (dp);
			dbg_printf ("Total crypto data: %lu bytes.\r\n", (unsigned long) sec_crypt_alloc);
		}
		else
			cps_dump_participant (dp);
	else
		if ((entity_flags (p) & EF_LOCAL) != 0)
			cps_dump_source (dp);
		else
			cps_dump_discovered_endpoint (dp);
}

#endif

/* Crypto plugin data: */
struct sec_crypto_st sec_crypto_aes_ctr_hmac = {
	GMCLASSID_SECURITY_AES_CTR_HMAC_PLUGIN,
	GMCLASSID_SECURITY_AES_CTR_HMAC,

	/* Supported crypto transformation kind ids: */
	{ 0x00000100,			/* HMAC_SHA1 */
	  0x00000101,			/* HMAC_SHA256 */
	  0x00000200,			/* AES128_HMAC_SHA1 */
	  0x00000201 },			/* AES256_HMAC_SHA256 */

	/* Crypto key factory methods: */
	cps_register_loc_part,
	cps_register_rem_part,
	cps_register_local_writer,
	cps_register_local_reader,
	cps_register_disc_writer,
	cps_register_disc_reader,
	cps_unregister_participant,
	cps_unregister_writer,
	cps_unregister_reader,

	/* Crypto key exchange methods: */
	cps_create_loc_part_tokens,
	cps_set_rem_part_tokens,
	cps_create_writer_tokens,
	cps_create_reader_tokens,
	cps_set_disc_writer_tokens,
	cps_set_disc_reader_tokens,
	cps_remember_disc_writer_tokens,
	cps_remember_disc_reader_tokens,

	/* Crypto encryption methods: */
	cps_encrypt_payload,
	cps_encrypt_writer_submsg,
	cps_encrypt_reader_submsg,
	cps_encrypt_message,

	/* Crypto decryption methods: */
	cps_decrypt_payload,
	cps_preprocess_submsg,
	cps_decrypt_writer_submsg,
	cps_decrypt_reader_submsg,
	cps_decrypt_message,

	/* Crypto debug methods: */
#ifdef DDS_DEBUG
	cps_dump
#else
	NULL
#endif
};

int sec_crypto_add_std (void)
{
	return (sec_crypto_add (&sec_crypto_aes_ctr_hmac));
}

