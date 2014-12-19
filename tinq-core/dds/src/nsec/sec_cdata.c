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

#include "error.h"
#include "handle.h"
#include "sec_cdata.h"

#ifdef _WIN32
#define VARSIZE	1000000
#else
#define VARSIZE
#endif

static CryptoData_t 	*(*crypto) [VARSIZE];	/* Crypto contexts. */
static void		*handles;		/* Crypto handles. */
static unsigned		min_handles;
static unsigned		cur_handles;
static unsigned		max_handles;

size_t sec_crypt_alloc;


/* crypto_data_init -- Initialize crypto data. */

int crypto_data_init (unsigned min, unsigned max)
{
	if (handles) {	/* Already initialized -- reset. */
		memset (crypto, 0, sizeof (CryptoData_t *) * cur_handles);
		handle_reset (handles);
		return (DDS_RETCODE_OK);
	}

	/* Allocate the crypto handles. */
	handles = handle_init (min);
	if (!handles) {
		warn_printf ("crypto_init: not enough memory for crypto handles!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Initialize the crypto array. */
	cur_handles = min_handles = min;
	max_handles = max;
	crypto = xmalloc ((cur_handles + 1) * sizeof (CryptoData_t *));
	if (!crypto) {
		warn_printf ("crypto_init: not enough memory for crypto table!\r\n");
		handle_final (handles);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	sec_crypt_alloc = (cur_handles + 1) * sizeof (CryptoData_t *);
	return (DDS_RETCODE_OK);
}

/* crypto_create -- Create a new crypto handle for a crypto context, and return
		    its data context. */

CryptoData_t *crypto_create (const SEC_CRYPTO *plugin,
			     size_t           data_size,
			     void             *owner,
			     int              endpoint)
{
	CryptoData_t	*dp;
	Crypto_t	h;
	unsigned	n;
	void		*nhandles;

	h = handle_alloc (handles);
	if (!h) {
		n = cur_handles + min_handles;
		if (n > max_handles)
			n = max_handles;
		n -= cur_handles;
		if (!n) {
			warn_printf ("Crypto: max. # of contexts reached (1)!");
			return (NULL);
		}
		nhandles = handle_extend (handles, n);
		if (!nhandles) {
			warn_printf ("Crypto: max. # of contexts reached (2)!");
			return (NULL);
		}
		handles = nhandles;
		h = handle_alloc (handles);
		if (!h) {
			fatal_printf ("Crypto: can't create a handle!");
			return (NULL);
		}
		crypto = xrealloc (crypto, (cur_handles + 1 + n) * sizeof (CryptoData_t *));
		if (!crypto) {
			fatal_printf ("Crypto: can't extend crypto table!");
			return (NULL);
		}
		cur_handles += n;
		sec_crypt_alloc += n * sizeof (CryptoData_t *);
	}
	dp = xmalloc (sizeof (CryptoData_t) + data_size);
	if (!dp) {
		warn_printf ("Crypto: Out of memory for crypto data!");
		handle_free (handles, h);
		return (NULL);
	}
	sec_crypt_alloc += sizeof (CryptoData_t) + data_size;
	dp->handle = h;
	dp->plugin = plugin;
	if (endpoint)
		dp->parent.endpoint = owner;
	else
		dp->parent.participant = owner;
	dp->endpoint = endpoint;
	dp->data = dp + 1;
	(*crypto) [h] = dp;
	dp->handle = h;
	return (dp);
}

/* crypto_lookup - Return the crypto context from the handle. */

CryptoData_t *crypto_lookup (Crypto_t handle)
{
	return ((handle && handle <= cur_handles) ? (*crypto) [handle] : NULL);
}

/* crypto_release -- Release a crypto context. */

void crypto_release (Crypto_t h)
{
	CryptoData_t	*dp;

	if (h < 1 || h > cur_handles || (dp = (*crypto) [h]) == NULL) {
		warn_printf ("Crypto: Invalid handle (%u)!", h);
		return;
	}
	xfree (dp);
	(*crypto) [h] = NULL;
	handle_free (handles, h);
}

