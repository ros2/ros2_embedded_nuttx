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
#include "dds/dds_security.h"
#include "sp_cred.h"

static void readFile (const char *path, unsigned char **buf)
{
	FILE *fp;
	long lSize;

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

	fclose(fp);
}

static int compare (unsigned char *first,
		    size_t        len1,
		    unsigned char *second,
		    size_t        len2)
{
	if (len1 != len2)
		return (0);
	if (!memcmp (first, second, len1))
		return (1);
	return (0);
}

/* First arg = local cert
 * Second arg = local private key */

int main (int argc, const char *argv [])
{
	unsigned char *cert_file = NULL, *key_file = NULL;
	unsigned char *cert;
	unsigned char *key;
	size_t cert_len = 0, key_len = 0;
	X509 *cert_ptr = NULL;
	STACK_OF(X509) *chain;
	EVP_PKEY *pkey = NULL;
	FILE *fp;
	DDS_Credentials credential;
	
	/* Test DDS_Credentials in DDS_SSL_BASED format */

	fp = fopen (argv [1], "rb");

	chain = sk_X509_new_null ();

	while ((cert_ptr = PEM_read_X509(fp, NULL, NULL, NULL)) != NULL) 
		sk_X509_push (chain, cert_ptr);

	fp = fopen (argv [2], "rb");

	pkey = PEM_read_PrivateKey(fp ,NULL, NULL, NULL);

	credential.credentialKind = DDS_SSL_BASED;
	credential.info.sslData.certificate_list = chain;
	credential.info.sslData.private_key = pkey;

	sp_extract_pem (&credential,
			&cert, &cert_len,
			&key, &key_len);

	readFile (argv [1], &cert_file);
	readFile (argv [2], &key_file);

	if (!compare (cert_file, strlen (cert_file),
		      cert, cert_len)) {
		printf ("compare problem \r\n");
		exit (1);
	}
	if (!compare (key_file, strlen (key_file),
		      key, key_len)) {
		printf ("compare problem \r\n");
		exit (1);
	}

	free (cert);
	free (key);

	/* Test DDS_Credentials in DDS_DATA_BASED PEM format */

	credential.credentialKind = DDS_DATA_BASED;
	credential.info.data.private_key.format = DDS_FORMAT_PEM;
	credential.info.data.private_key.data = key_file;
	credential.info.data.private_key.length = strlen (key_file);
	credential.info.data.num_certificates = 1;
	credential.info.data.certificates [0].format = DDS_FORMAT_PEM;
	credential.info.data.certificates [0].data = cert_file;
	credential.info.data.certificates [0].length = strlen (cert_file);
	
	cert_len = 0;
	key_len = 0;

	sp_extract_pem (&credential,
			&cert, &cert_len,
			&key, &key_len);

	if (!compare (cert_file, strlen (cert_file),
		      cert, cert_len)) {
		printf ("compare problem \r\n");
		exit (1);
	}
	if (!compare (key_file, strlen (key_file),
		      key, key_len)) {
		printf ("compare problem \r\n");
		exit (1);
	}

	if (sp_add_credential (1, "fake_name", cert, cert_len,
			       key, key_len))
		exit (1);

	if (!sp_get_name (1))
		exit (1);

	if (!sp_get_cert (1, &cert_len))
		exit (1);

	if (!sp_get_key (1, &key_len))
		exit (1);

	if (sp_remove_credential (1))
		exit (1);

	free (cert_file);
	free (key_file);
	free (cert);
	free (key);

	sk_X509_free (chain);

	printf ("Test succeeded\r\n");
}


