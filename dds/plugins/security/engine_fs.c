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

/*Includes*/
#include "engine_fs.h"
#include "dds/dds_security.h"
#include <stdio.h>
#include <openssl/engine.h>
#include <openssl/crypto.h>
#include <string.h>
#include <openssl/pem.h>
#include "error.h"

/*Function Prototypes*/

static int bind_helper (ENGINE *e);

static int engine_fs_destroy (ENGINE * e);
static int engine_fs_ctrl (ENGINE * e, int cmd, long i, void *p, void (*f) ());

int engine_fs_init (ENGINE * engine);
int engine_fs_finish (ENGINE * engine);

static EVP_PKEY *engine_fs_load_private_key (ENGINE *e, const char *key_id,
					     UI_METHOD *ui_method, void *callback_data);

static int engine_fs_load_cert_ctrl (void *p);
static int engine_fs_load_ca_cert_ctrl (void *p);

static int engine_fs_load_cert (const char *file, X509 **cert);
static int engine_fs_load_ca_cert (const char *file, X509 **cert);

static int register_rsa_methods (void);

static BIO *in;

static RSA_METHOD engine_fs_rsa = {
	FS_ENGINE_NAME,
	NULL, /* rsa_pub_enc */
	NULL, /* rsa_pub_dec (verification) */
	NULL, /* rsa_priv_enc (signing) */
	NULL, /* rsa_priv_dec */
	NULL, /* rsa_mod_exp */
	NULL, /* bn_mod_exp */
	NULL, /* init */
	NULL, /* finish */
	RSA_FLAG_EXT_PKEY | RSA_FLAG_NO_BLINDING, /* flags */
	NULL, /* app_data */
	NULL, /* rsa_sign */
	NULL, /* rsa_verify */
	NULL, /* rsa_keygen */
};

/***********************************************/
/** bio functions **/ 
/***********************************************/

static int bio_read(const char *file)
{
	in = BIO_new (BIO_s_file_internal());
	if (in == NULL) {
		printf ("BIO PROBLEM\n\r");
		return (0);
	}
	if (BIO_read_filename(in, file) <= 0) {
		printf ("READ PROBLEM\n\r");
		return (0);
	}
	return (1);
}

static void bio_cleanup (void)
{
	if (in)
		BIO_free(in);
}

/***********************************************/
/** Helper functions **/ 
/***********************************************/

static int bind_helper (ENGINE *e) {

	if (!ENGINE_set_id(e, FS_ENGINE_ID) ||
		!ENGINE_set_name (e, FS_ENGINE_NAME) ||
		!ENGINE_set_destroy_function (e, engine_fs_destroy) ||
		!ENGINE_set_finish_function (e, engine_fs_finish) ||
		!ENGINE_set_ctrl_function (e, engine_fs_ctrl) ||
		!ENGINE_set_load_privkey_function (e, engine_fs_load_private_key) ||
		!ENGINE_set_RSA (e, &engine_fs_rsa) /*||
		!ENGINE_set_load_pubkey_function (e, engine_fs_load_public_key) ||
		!ENGINE_set_init_function (e, engine_fs_init) ||
		!ENGINE_set_DSA (e, engine_fs_dsa) ||
		!ENGINE_set_ECDH (e, engine_fs_dh) ||
		!ENGINE_set_ECDSA (e, engine_fs_dh) ||
		!ENGINE_set_DH (e, engine_fs_dh) ||
		!ENGINE_set_RAND (e, engine_fs_rand) ||
		!ENGINE_set_STORE (e, asn1_i2d_ex_primitiveengine_fs_rand) ||
		!ENGINE_set_ciphers (e, engine_fs_syphers_f) ||
		!ENGINE_set_digests (e, engine_fs_digest_f) ||
		!ENGINE_set_flags (e, engine_fs_flags) ||
		!ENGINE_set_cmd_defns (e, engine_fs_cmd_defns)*/) {
		return (0);
	}
	
	if (!ENGINE_set_RSA (e, &engine_fs_rsa)
                || !register_rsa_methods ()) {
            return 0;
	}
	
	return (1);
}

/***********************************************/
/** RSA functions **/ 
/***********************************************/

static int register_rsa_methods (void) {
	
	const RSA_METHOD* rsa_meth = RSA_PKCS1_SSLeay();
	engine_fs_rsa.rsa_pub_enc = rsa_meth->rsa_pub_enc;
	engine_fs_rsa.rsa_pub_dec = rsa_meth->rsa_pub_dec;
	engine_fs_rsa.rsa_priv_dec = rsa_meth->rsa_priv_dec;
	engine_fs_rsa.rsa_mod_exp = rsa_meth->rsa_mod_exp;
	engine_fs_rsa.bn_mod_exp = rsa_meth->bn_mod_exp;

        return 1;
}

/***********************************************/
/** CTRL functions **/ 
/***********************************************/
    
static int engine_fs_ctrl (ENGINE * e, int cmd, long i, void *p, void (*f) ())
{
	ARG_NOT_USED (e)
	ARG_NOT_USED (i)
	ARG_NOT_USED (f)
	
	switch (cmd) {
	        case CMD_LOAD_CERT_CTRL:
			return engine_fs_load_cert_ctrl (p);
			break;
	        case CMD_LOAD_CA_CERT_CTRL:
			return engine_fs_load_ca_cert_ctrl (p);
			break;		
                default:
			break;
	}
	return 0;
}

/***********************************************/
/** Key functions **/ 
/***********************************************/
static EVP_PKEY *engine_fs_load_private_key (ENGINE *e, const char *key_id,
					     UI_METHOD *ui_method, void *callback_data)
{
	EVP_PKEY *pk = NULL;

	ARG_NOT_USED (e)
	ARG_NOT_USED (ui_method)
	ARG_NOT_USED (callback_data)

	bio_read (key_id);
	
	pk = PEM_read_bio_PrivateKey (in, NULL, NULL, NULL);
	
	bio_cleanup ();

	if (!pk)
		fatal_printf ("Error retrieving the private key");
		
	return pk;
}

/***********************************************/
/** Cert functions **/ 
/***********************************************/

static int engine_fs_load_ca_cert_ctrl (void *p)
{
	return engine_fs_load_ca_cert (((dataMap *)p)->id, (X509 **) ((dataMap *)p)->data);
}

static int engine_fs_load_cert_ctrl (void *p)
{
	return engine_fs_load_cert (((dataMap *)p)->id , (X509 **)((dataMap *)p)->data);
}

static int engine_fs_load_ca_cert (const char *file, X509 *CAcertificates [10]){
	
	int counter = 0;
	X509 *cert = NULL;

	bio_read (file);
	if (CAcertificates != NULL) {

		X509_free (cert);
		while ((cert = PEM_read_bio_X509 (in, NULL, NULL, NULL)) != NULL) {
			/*Discard the first because it's not a CA cert*/
			if (counter != 0) 
				CAcertificates [counter - 1] = cert;
			
			else
				X509_free (cert);
			counter++;
		}
		if (cert)
			X509_free (cert);
	}
	else
	{
		/* We count the number of CA certificates */
		while ((cert = PEM_read_bio_X509 (in, NULL,NULL, NULL)) != NULL) {
			counter ++;
			X509_free (cert);
		}
	}
	
	bio_cleanup ();
	
	return (counter - 1);
}

/* Get the certificate provided by a file path */

static int engine_fs_load_cert (const char *file, X509 **cert)
{
	X509 *tmpCert;
	
	bio_read(file);
	tmpCert = PEM_read_bio_X509 (in,NULL ,NULL, NULL);
	bio_cleanup();
	
	if (!tmpCert)
		fatal_printf ("No certificate found");

	*cert = tmpCert;
	return (1);
}

/***********************************************/
/** Init functions **/ 
/***********************************************/

/*create a new android fs engine*/
void *init_engine_fs (void) 
{
	ENGINE *ret = ENGINE_new ();

	if (!ret)
		return (NULL);
	if (!bind_helper (ret))
	{
		ENGINE_free (ret);
		return (NULL);
	}
	return (ret);
}

/*add the engine to the engine arraylist*/
void engine_fs_load (void)
{

	ENGINE *e_android_fs = init_engine_fs ();
	
	if (!e_android_fs) return;
	ENGINE_add (e_android_fs);
	ENGINE_free (e_android_fs);
	ERR_clear_error ();
}

/*"Destructor"*/
int engine_fs_finish (ENGINE * e)
{
	ARG_NOT_USED (e)
	return (1);
}

/*not used for the moment*/
static int engine_fs_destroy (ENGINE * e)
{
	ARG_NOT_USED (e)
	return (1);
}

#if 0

/***********************************************/
/** Bind functions **/ 
/***********************************************/

/* This stuff is needed if this ENGINE is being compiled into a self-contained
 * shared-library. */
static int bind_fn (ENGINE * e, const char *id)
{
	if (id && (strcmp (id, FS_ENGINE_ID) != 0)) {
		fprintf (stderr, "bad engine id\n");
		return (0);
	}
	if (!bind_helper (e)) {
		fprintf (stderr, "bind failed\n");
		return (0);
	}
	return (1);
}

IMPLEMENT_DYNAMIC_CHECK_FN ()
IMPLEMENT_DYNAMIC_BIND_FN (bind_fn)

#endif
