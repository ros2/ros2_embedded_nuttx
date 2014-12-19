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
#include "dds/dds_error.h"
#include "sp_sys_cert.h"
#include "sp_sys_crypto.h"
#include "openssl/pem.h"
#include "sp_auth.h"
#include <string.h>

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

/* First arg = local cert
 * Second arg = local private key
 * Third arg = remote cert */

int main (int argc, const char *argv [])
{
	unsigned char *cert_file = NULL;
	X509 *cert_ptr = NULL;
	STACK_OF(X509) *chain;
	EVP_PKEY *pkey = NULL;
	FILE *fp;
	DDS_Credentials credential;
	unsigned local_id, remote_id;
	int validation;
	DDS_ReturnCode_t ret;
	char name [32], cred [5096];
	int len;

	/* init functions */

	sp_sec_cert_add ();
	sp_sec_crypto_add ();

	fp = fopen (argv [1], "rb");

	chain = sk_X509_new_null ();

	while ((cert_ptr = PEM_read_X509(fp, NULL, NULL, NULL)) != NULL) 
		sk_X509_push (chain, cert_ptr);


	fp = fopen (argv [2], "rb");

	pkey = PEM_read_PrivateKey(fp ,NULL, NULL, NULL);

	credential.credentialKind = DDS_SSL_BASED;
	credential.info.sslData.certificate_list = chain;
	credential.info.sslData.private_key = pkey;

	DDS_SP_parse_xml ("security.xml");

	readFile (argv [3], &cert_file);

	if ((ret = sp_validate_local_id ("Test", &credential, &local_id, &validation)))
		exit (ret);

	if ((ret = sp_validate_remote_id ("DDS:Auth:X.509-PEM-SHA256", "tmp id", 5 ,&validation, &remote_id)))
		exit (ret);

	if ((ret = sp_verify_remote_credentials (local_id, remote_id, 
						 cert_file, strlen ((const char *) cert_file),
						 &validation)))
		exit (ret);

	if ((ret = sp_auth_get_name (local_id, &name [0], 32, &len)))
		exit (ret);

	if (strcmp (name, "Test"))
		exit (1);

	if ((ret = sp_auth_get_id_credential (local_id, &cred [0], 5096, &len)))
		exit (ret);

	if ((ret = sp_release_identity (local_id)))
		exit (ret);
	
	if ((ret = sp_release_identity (remote_id)))
		exit (ret);

	free (cert_file);
	EVP_PKEY_free (pkey);

	sk_X509_free (chain);

	printf ("\r\ntest succeeded\r\n");
	return (0);
}
