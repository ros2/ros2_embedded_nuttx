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

#include <stdio.h>
#include <stdlib.h>
#include <openssl/crypto.h>

/* This is not clean, but the most easy way to test static functions */
#include "../../apps/nsecplug/sp_sys_cert.c"

unsigned char *local_cert = NULL;
unsigned char *remote_cert = NULL;
unsigned char *local_private_key = NULL;
unsigned char *bad_cert = NULL;
size_t length;

#if 0
static void readFile (const char *path, unsigned char **buf)
{
	FILE *fp;
	long lSize;

	printf ("Read the file\r\n");

	fp = fopen ( path , "rb" );
	if( !fp ) perror (path),exit(1);

	fseek ( fp , 0L , SEEK_END);
	lSize = ftell( fp );
	rewind ( fp );

	/* allocate memory for entire content */
	if (*buf)
		free ((void*) *buf);
	*buf = calloc ( 1, lSize+1 );
	if( !*buf ) fclose(fp),fputs("memory alloc fails",stderr),exit(1);

	/* copy the file into the buffer */
	if( 1!=fread( (void *) *buf , lSize, 1 , fp) )
		fclose(fp),free(buf),fputs("entire read fails",stderr),exit(1);

	/* do your work here, buffer is a string contains the whole text */
	length = (size_t) lSize;
	fclose(fp);
}
#endif 

static void print_hex (unsigned char *bs, unsigned int n)
{
	unsigned int i;
	
	for (i = 0; i < n; i++)
		printf ("%02x", bs [i]);

	printf ("\r\n");
}

static void readBIO (const char *path, unsigned char **buf)
{
	BIO *bio;
	int size = 5000;

	bio = BIO_new_file(path, "r");

	*buf = malloc (size);
	
	BIO_read (bio, *buf, size);

	BIO_free (bio);
}

/* First arg = local cert
 * Second arg = local private key
 * Third arg = remote cert
 * Fourth arg = bad remote cert */

int main (int argc, const char *argv [])
{
	unsigned char sign [512];
	size_t sign_len = 512;
	unsigned char name [256];
	size_t        name_len = 256;
	unsigned char encrypted [1024], decrypted [1024];
	size_t enc_len = 1024, dec_len;
	unsigned char *msg = "This is teststring with a very long long long ling lign lgin lgnisdlfkljdsn gl;ksjhdakjs gkljshadgk jsadglkjs hadkgj hsakljghsakjghlaskjgdh";
	unsigned char *badmsg = "This is a bad teststring ...";
	unsigned char data [256];
	size_t data_len;
	int i;
	unsigned result;

	const unsigned char secret [32] = {"qwertyuiopasdfghjklzxcvbnmqwerty"};

	X509 *mycert = NULL;
	STACK_OF(X509) *ca = NULL;
	EVP_PKEY *pkey = NULL;
	int nb;

	printf ("Using ssl version %s\r\n", SSLeay_version(SSLEAY_VERSION));	

	/*DDS_Log_stdio (1);*/

	/* Load the certificates and keys in buffer */
	readBIO (argv [1], &local_cert);
	printf ("Local certificate is \r\n %s", local_cert);

	readBIO (argv [3], &remote_cert);
	printf ("Remote certificate is \r\n %s", remote_cert);

	readBIO (argv [2], &local_private_key);
	printf ("Local private key is \r\n %s", local_private_key);

	readBIO (argv [4], &bad_cert);
	printf ("Bad certificate is \r\n %s", bad_cert);

	/* Validate the local certificate */
      	if (sp_sys_validate_certificate (local_cert, strlen (local_cert), 
					 local_cert, strlen (local_cert))) {
		printf ("Problem validating local certificate");
		exit (1);
	}
	printf ("Validating local certificate succeeded\r\n");

	/* Validate the remote certificate */
	if (sp_sys_validate_certificate (remote_cert, strlen (remote_cert),
					 local_cert, strlen (local_cert))) {
		printf ("PROBLEM validating remote cert\r\n");
		exit (1);
	}
	printf ("Validating remote certificate succeeded\r\n");

	/* Validate bad certificate */
	if (!sp_sys_validate_certificate (bad_cert, strlen (bad_cert),
					 local_cert, strlen (local_cert))) {
		printf ("PROBLEM: Validating a bad certificate succeeded\r\n");
		exit (1);
	}
	printf ("Validating bad certificate succeeded\r\n");


	/* Sign a string with private key */
	if (sp_sys_sign_sha256 (local_private_key, strlen (local_private_key), 
				msg, strlen ((char *) msg), sign, &sign_len)) {
		printf ("PROBLEM Signing\r\n");
		exit (1);
	} else {
		printf ("Signing succeeded, signed msg is: ");
		print_hex (&sign [0], sign_len);
	}
	
	/* Verify signature with public key */
	if (sp_sys_verify_signature_sha256 (local_cert, strlen ((char *) local_cert), 
					    msg, strlen ((char *) msg), &sign [0], sign_len, &result)) {
		printf ("PROBLEM verifying signature\r\n");
		exit (1);
	} else
		printf ("signature verified correctly\r\n");
	
	/* Verification that should fail */
	if (sp_sys_verify_signature_sha256 (local_cert, strlen ((char *) local_cert), 
					    badmsg, strlen ((char *) msg), &sign [0],
					    sign_len, &result))
		printf ("The signatures don't match, this fail is to be expected\r\n");
	else {
		printf ("PROBLEM Verifying succeeded when it should not\r\n");
		exit (1);
	}

	/* Verify encryption with public key. */
	if (sp_sys_encrypt_public (local_cert, strlen ((char *) local_cert),
				   secret, 32,
				   &encrypted, &enc_len)) {
		printf ("PROBLEM Encrypting with public key\r\n");
		exit (1);
	}
	else 
		printf ("Public key encryption succeeded\r\n");

	/* Verify decryption with private key. */
	if (sp_sys_decrypt_private (local_private_key, strlen ((char *) local_private_key), 
				    encrypted, enc_len, &decrypted, &dec_len)) {
		printf ("PROBLEM Decrypting with private key\r\n");
		exit (1);
	}
	if (dec_len != 32) {
		printf ("PROBLEM Decrypting with private key (incorrect length)\r\n");
		exit (1);
	}
	if (!memcmp (&secret, &decrypted, 32))
		printf ("Decryption with private key successful\r\n");
	else {
		printf ("PROBLEM: no match after encryption and decryption\r\n");
		exit (1);
	}

	/* Test X.509 certificate functions. */
	if (!sp_sys_get_x509_certificate (local_cert, strlen ((const char *)local_cert), &mycert))
		printf ("X509 Certificate fetched from pem file\r\n");
	else {
		printf ("PROBLEM fetching certificate from pem\r\n");
		exit (1);
	}

	if (!sp_sys_get_x509_ca_certificate (local_cert, strlen ((const char *)local_cert), &ca))
		printf ("X509 CA Certificate fetched from pem file\r\n");
	else {
		printf ("PROBLEM fetching certificate from pem\r\n");
		exit (1);
	}

	if (!sp_sys_get_private_key_x509 (local_private_key, strlen ((const char *)local_private_key), &pkey))
		printf ("EVP_PKEY fetched from pem file\r\n");
	else {
		printf ("PROBLEM fetching EVP_PKEY from pem\r\n");
		exit (1);
	}

	if (!sp_sys_get_nb_of_ca_cert (local_cert, strlen ((const char *)local_cert), &nb))
		printf ("nb of CA Certificate fetched from pem file\r\n");
	else {
		printf ("PROBLEM fetching nb of ca certificates from pem\r\n");
		exit (1);
	}

	if (!sp_sys_encrypt_public (local_cert, strlen (local_cert),
				    secret, 32, 
				    &encrypted, &enc_len))
		printf ("Public key encyption succeedded: %s\r\n", encrypted);

	else
		exit (1);

	if (!sp_sys_decrypt_private (local_private_key, strlen (local_private_key),
				     encrypted, enc_len, 
				     &decrypted, &dec_len))
		printf ("Private key decryption succeedded: %s\r\n", decrypted);
	else
		exit (1);

	if (!memcmp (&secret, &decrypted, 32))
		printf ("The data after Encryption and decryption matches\r\n");
	else {
		printf ("No match after encryption and decryption\r\n");
		exit (1);
	}

	if (!sp_sys_get_common_name (local_cert, strlen (local_cert),
				     &name, &name_len))
		printf ("\r\nretrieved common name %s\r\n", name);

	if (!strcmp (name, "1a5235b470334aa8 14d3 5283"))
		printf ("common name matches\r\n");

	free (local_cert);
	free (remote_cert);
	free (local_private_key);
	free (bad_cert);

	printf ("Test succeeded\r\n");
}

