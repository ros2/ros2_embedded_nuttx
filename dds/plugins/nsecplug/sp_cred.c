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

#include "sp_cred.h"
#include "list.h"
#include "error.h"
#include "openssl/ssl.h"
#include "openssl/bio.h"
#include "log.h"

#define SP_CRED_LOG

typedef struct pluginCred_st PluginCred_t;
struct pluginCred_st {
	PluginCred_t     *next;
	PluginCred_t     *prev;
	IdentityHandle_t id;
	char             *name;
	unsigned char    *certificate;
	size_t           cert_len;
	unsigned char    *key;
	size_t           key_len;
};

typedef struct {
	PluginCred_t *head;
	PluginCred_t *tail;
} PluginCredList_t;

PluginCredList_t list;
int              list_init = 0;

static void init_plugincred_list (void)
{
	if (!list_init) {
#ifdef SP_CRED_LOG
	log_printf (SEC_ID, 0, "SP_CRED: Credential list initialized\r\n");
#endif
		LIST_INIT (list);
	}
	list_init = 1;
}

static PluginCred_t *get_plugincred_node (IdentityHandle_t id)
{
	PluginCred_t *node;

	init_plugincred_list ();

	LIST_FOREACH (list, node)
		if (node->id == id)
			return (node);
	return (NULL);
}

static PluginCred_t *add_plugincred_node (IdentityHandle_t id)
{
	PluginCred_t *node;
	
	init_plugincred_list ();

	if (!(node = get_plugincred_node (id))) {
		if (!(node = malloc (sizeof (PluginCred_t)))) {
			exit (1);
		} else {
			node->id = id;
			node->name = NULL;
			node->certificate = NULL;
			node->cert_len = 0;
			node->key = NULL;
			node->key_len = 0;
			LIST_ADD_HEAD (list, *node);
		}
	}
#ifdef SP_CRED_LOG
	log_printf (SEC_ID, 0, "SP_CRED: Add node (%p) [%d] to credential list\r\n",(void *) node, id);
#endif
	return (node);
}

static void remove_plugincred_node (PluginCred_t *node)
{

#ifdef SP_CRED_LOG
	log_printf (SEC_ID, 0, "SP_CRED: remove node (%p)[%d] from credential list\r\n",(void *) node, node->id);
#endif
	if (node) {
		if (node->name)
			free (node->name);
		if (node->certificate)
			free (node->certificate);
		if (node->key)
			free (node->key);
		LIST_REMOVE (list, *node);
		free (node);
	}
}

/* Add a credential to the database */

DDS_ReturnCode_t sp_add_credential (IdentityHandle_t id,
				    char             *name,
				    unsigned char    *cert,
				    size_t           cert_len,
				    unsigned char    *key,
				    size_t           key_len)
{
	PluginCred_t *node;
	
	if (!cert)
		return (DDS_RETCODE_BAD_PARAMETER);

	node = add_plugincred_node (id);
	if (name) {
		node->name = malloc (sizeof (unsigned char) * strlen (name) + 1);
		strcpy ((char *) node->name, (const char *) name);
	}
	node->certificate = malloc (sizeof (unsigned char) * cert_len + 2);
	strcpy ((char *) node->certificate, (const char *) cert);
	node->cert_len = cert_len;
	if (key) {
		node->key = malloc (sizeof (unsigned char) * key_len + 2); 
		strcpy ( (char *) node->key,(const char *) key);
		node->key_len = key_len;
	} 

	return (DDS_RETCODE_OK);
}

/* Get a credential from the database */

char *sp_get_name (IdentityHandle_t id)
{
	PluginCred_t *node;

	if ((node = get_plugincred_node (id))) {
		return (node->name);
	}
	return (NULL);
}

unsigned char *sp_get_cert (IdentityHandle_t id,
			    size_t           *len)
{
	PluginCred_t *node;

	if ((node = get_plugincred_node (id))) {
		*len = node->cert_len + 1; 
		return (node->certificate);
	}
	*len = 0;
	return (NULL);
}

unsigned char *sp_get_key (IdentityHandle_t id,
			   size_t           *len)
{
	PluginCred_t *node;

	if ((node = get_plugincred_node (id))) {
		*len = node->key_len; 
		return (node->key);
	}
	*len = 0;
	return (NULL);
}

/* remove a credential from the database */

DDS_ReturnCode_t sp_remove_credential (IdentityHandle_t id)
{
	PluginCred_t *node;

	if ((node = get_plugincred_node (id)))
		remove_plugincred_node (node);
	else
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_extract_pem (DDS_Credentials *cred,
				 unsigned char  **cert,
				 size_t         *cert_len,
				 unsigned char  **key,
				 size_t         *key_len)
{
	BIO *cert_in, *key_in;
	int i;
	FILE *fp;
	long lSize;

	if (cred->credentialKind == DDS_ENGINE_BASED) {
		/* Not implemented */
	}
	else if (cred->credentialKind == DDS_FILE_BASED) {
		/* Read the certificate pem file */
		fp = fopen ( cred->info.filenames.certificate_chain_file , "rb" );
		if( !fp ) perror ( cred->info.filenames.certificate_chain_file),exit(1);
		
		fseek ( fp , 0L , SEEK_END);
		lSize = ftell( fp );
		rewind ( fp );
		
		*cert = calloc ( 1, lSize+1 );
		if( !*cert ) fclose(fp),fputs("memory alloc fails",stderr),exit(1);
		
		if( 1!=fread( (void *) *cert , lSize, 1 , fp) )
			fclose(fp),free(*cert),fputs("entire read fails",stderr),exit(1);
		
		*cert_len = (size_t) lSize;
		fclose(fp);

		/* Read private key pem file */
		fp = fopen (cred->info.filenames.private_key_file, "rb" );
		if (!fp) {
			perror (cred->info.filenames.private_key_file);
			exit(1);
		}
		
		fseek ( fp , 0L , SEEK_END);
		lSize = ftell( fp );
		rewind ( fp );
		
		*key = calloc ( 1, lSize+1 );
		if( !*key ) fclose(fp),fputs("memory alloc fails",stderr),exit(1);
		
		if( 1!=fread( (void *) *key , lSize, 1 , fp) )
			fclose(fp),free(*key),fputs("entire read fails",stderr),exit(1);
		
		*key_len = (size_t) lSize;
		fclose(fp);

	}
	else if (cred->credentialKind == DDS_SSL_BASED) {
		cert_in = BIO_new (BIO_s_mem ());
		key_in = BIO_new (BIO_s_mem ());
		
		/* Write the X509 to a PEM format in a BIO */
		for (i = 0; i < sk_X509_num (cred->info.sslData.certificate_list); i++)
			PEM_write_bio_X509(cert_in, sk_X509_value (cred->info.sslData.certificate_list, i));

		*cert = malloc (sizeof (unsigned char) * cert_in->num_write + 1);
		
		/* read from the BIO into a char * */
		*cert_len = BIO_read (cert_in, *cert, cert_in->num_write);
		memset (&(*cert) [(int) *cert_len], '\0', sizeof (char));

		/* Write the X509 to a PEM format in a BIO */
		PEM_write_bio_PrivateKey(key_in, cred->info.sslData.private_key, NULL, NULL, 0, 0, NULL);
		*key = malloc (sizeof (unsigned char) * key_in->num_write + 1);
		
		/* read from the BIO into a char * */
		*key_len = BIO_read (key_in, *key, key_in->num_write);
		memset (&(*key) [(int) *key_len], '\0', sizeof (char));
		BIO_free (cert_in);
		BIO_free (key_in);
	}
	else if (cred->credentialKind == DDS_DATA_BASED) {
		if (cred->info.data.private_key.format == DDS_FORMAT_PEM) {
			*key = malloc (sizeof (unsigned char) * cred->info.data.private_key.length + 1);
			strcpy ((char *) *key, (char *) cred->info.data.private_key.data);
			*key_len = cred->info.data.private_key.length;
		}
		else {
			/* Not implemented */
		}
		/* This is okay for the current useage, but not for the intended useage */
		for (i = 0; i < (int) cred->info.data.num_certificates ; i++) {
			if (cred->info.data.certificates [i].format == DDS_FORMAT_PEM) {
				*cert = malloc (sizeof (unsigned char) * cred->info.data.certificates [i].length + 1);
				strcpy ((char *) *cert, (char *) cred->info.data.certificates [i].data);
				*cert_len = cred->info.data.certificates [i].length;
			}
		}
	}
	else {
		/*TODO: Go get the cert from data*/
	}

	return (DDS_RETCODE_OK); 
}
