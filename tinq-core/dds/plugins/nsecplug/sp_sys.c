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

/* sp_sys.c -- System specific functions -- OpenSSL-specific variant. */

#include "error.h"
#include "pool.h"
#include "thread.h"
#include "sp_sys.h"
#include "dds/dds_security.h"
#include <openssl/crypto.h>
#include <openssl/ssl.h>

int dds_openssl_init_global = 1;

static lock_t	*ssl_mutexes;

void DDS_SP_init_library (void)
{
	if (!dds_openssl_init_global)
		return;

	DDS_Security_set_library_lock ();
	OpenSSL_add_ssl_algorithms ();

#ifdef DDS_DEBUG
	SSL_load_error_strings ();
#endif
	
	dds_openssl_init_global = 0;
}

void sp_sys_set_library_init (int val)
{
	dds_openssl_init_global = val;
}

static void lock_function (int mode, int n, const char *file, int line)
{
	ARG_NOT_USED (file)
	ARG_NOT_USED (line)

	if ((mode & CRYPTO_LOCK) != 0)
		lock_take (ssl_mutexes [n]);
	else
		lock_release (ssl_mutexes [n]);
}

static void id_function(CRYPTO_THREADID *openssl_id)
{
	pthread_t id = pthread_self();
	CRYPTO_THREADID_set_pointer(openssl_id, (void *)id);
}

void sp_sys_set_library_lock (void)
{
	int i;

	ssl_mutexes = xmalloc (CRYPTO_num_locks () * sizeof (lock_t));
	if (!ssl_mutexes)
		return;

	for (i = 0; i < CRYPTO_num_locks (); i++)
		lock_init_nr (ssl_mutexes [i], "OpenSSL");

	CRYPTO_THREADID_set_callback (id_function);
	CRYPTO_set_locking_callback (lock_function);
}

void sp_sys_unset_library_lock (void)
{
	int i;

	if (!ssl_mutexes)
		return;

	CRYPTO_set_id_callback (NULL);
	CRYPTO_set_locking_callback (NULL);

	for (i = 0; i < CRYPTO_num_locks (); i++)
		lock_destroy (ssl_mutexes [i]);

	xfree (ssl_mutexes);
	ssl_mutexes = NULL;
}

#ifdef DDS_DEBUG
void sp_log_X509(X509* cert)
{
	BIO *dbg_out = BIO_new(BIO_s_mem());
	char dbg_out_string[256];
	int size;
	X509_NAME_print_ex(dbg_out, X509_get_subject_name(cert), 1, XN_FLAG_MULTILINE);
	while(1) {
		size = BIO_read(dbg_out, &dbg_out_string, 255);
		if (size <= 0) {
			break;
		}
		dbg_out_string[size] = '\0';
		log_printf (SEC_ID, 0, "%s", dbg_out_string);
	}
	log_printf (SEC_ID, 0, "\r\n");
	BIO_free(dbg_out);
}
#endif

