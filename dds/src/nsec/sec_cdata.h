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

/* sec_cdata.h -- Crypto handle associated data management for crypto plugins. */

#ifndef __sec_cdata_h_
#define __sec_cdata_h_

#include "sec_plugin.h"

/* Crypto associated data: */
struct crypto_data_st {
	unsigned	 handle;	/* Generic crypto handle. */
	const SEC_CRYPTO *plugin;	/* Used by which plugin. */
	union {	
	  Participant_t	 *participant;	/* If owner is a participant. */
	  Endpoint_t	 *endpoint;	/* If owner is an endpoint. */
	}		 parent;
	int		 endpoint;	/* Non-0 if associated with endpoint. */
	void		 *data;		/* Plugin-specific data pointer. */
};

extern size_t sec_crypt_alloc;

int crypto_data_init (unsigned min, unsigned max);

/* Initialize crypto data. */

CryptoData_t *crypto_create (const SEC_CRYPTO *plugin,
			     size_t           data_size,
			     void             *owner,
			     int              endpoint);

/* Create a new crypto handle for a crypto context, and return its data
   context. */

CryptoData_t *crypto_lookup (Crypto_t handle);

/* Return the crypto context from the handle. */

void crypto_release (Crypto_t handle);

/* Release a crypto context. */

#endif /* !__sec_cdata_h_ */

