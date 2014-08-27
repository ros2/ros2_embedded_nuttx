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

/* sp_aux.c -- Security Plugin - Auxiliary functions. */

#include "sp_aux.h"

DDS_ReturnCode_t sp_get_domain_sec_caps (DDS_DomainId_t domain_id,
					 unsigned       *sec_caps)
{
	MSDomain_t	*p;

	p = lookup_domain (domain_id, 1);
	if (p)
		*sec_caps = p->transport;
	else
		*sec_caps = 0;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_validate_ssl_connection (void             *ssl_cx,
					     struct sockaddr  *sp,
					     DDS_AuthAction_t *action)
{
	SSL			*ssl = (SSL *) ssl_cx;
	struct sockaddr_in	*s4;
#ifdef DDS_IPV6
	struct sockaddr_in6	*s6;
	char			addrbuf [INET6_ADDRSTRLEN];
#endif

	if (sp->sa_family == AF_INET) {
		s4 = (struct sockaddr_in *) sp;
		log_printf (SEC_ID, 0, "DTLS: connection from %s:%d ->",
			     inet_ntoa (s4->sin_addr),
		             ntohs (s4->sin_port));
	}
#ifdef DDS_IPV6
	else if (sp->sa_family == AF_INET6) {
		s6 = (struct sockaddr_in6 *) sp;
		log_printf (SEC_ID, 0, "DTLS: connection from %s:%d ->",
        		     inet_ntop (AF_INET6,
					&s6->sin6_addr,
					addrbuf,
					INET6_ADDRSTRLEN),
			     ntohs (s6->sin6_port));
	}
#endif
	if (!msp_validate_realm_certificate (ssl)) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		*action = DDS_AA_REJECTED;
		return (DDS_RETCODE_OK);
	}
	
	if (SSL_get_peer_certificate (ssl)) {
		sp_log_X509(SSL_get_peer_certificate (ssl));
		log_printf (SEC_ID, 0, "\nDTLS: cipher: %s ->", SSL_CIPHER_get_name (SSL_get_current_cipher (ssl)));
		log_printf (SEC_ID, 0, "accepted.\r\n");
		*action = DDS_AA_ACCEPTED;
	}
	else {
		log_printf (SEC_ID, 0, "denied.\r\n");
		*action = DDS_AA_REJECTED;
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_get_certificate (void             *cert, 
				     IdentityHandle_t id_handle)
{
	X509		**certificate = (X509 **) cert;
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	int		ret;

	if (credentials->engine_based) {
		dataMap data;
	
		data.id = credentials->info.engine.cert_id;
		data.data = (void *) certificate;

		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);
	
		return (ENGINE_ctrl (engines[ret],
				    CMD_LOAD_CERT_CTRL, 
				     0, (void *)&data, NULL));
	}
	else if (credentials->file_based) {
		X509 *cert;
		
		bio_read (credentials->info.filenames.certificate_chain_file);
		cert = PEM_read_bio_X509 (in,NULL ,NULL, NULL);
		bio_cleanup ();
	
		if (!cert)
			fatal_printf ("No certificate found");

		*certificate = cert;
	}
	else {
		/*TODO: Go get the cert from data*/
	}

	return (DDS_RETCODE_OK); 
}

DDS_ReturnCode_t sp_get_CA_certificate_list (void             *ca_certs,
					     IdentityHandle_t id_handle)
{
	X509		**CAcertificates = (X509 **) ca_certs;
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	int		ret;
	
	if (credentials->engine_based) {
		dataMap data;
		
		data.id = credentials->info.engine.cert_id;
		data.data = (void *) CAcertificates;

	if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);
		
		return (ENGINE_ctrl (engines [ret], 
				    CMD_LOAD_CA_CERT_CTRL, 
				     0, (void *)&data, NULL));
	}
	else if (credentials->file_based) {
		/* Go get the cert from file*/
		int counter = 0;
		X509 *cert = NULL;

		bio_read (credentials->info.filenames.certificate_chain_file);		
		while ((cert = PEM_read_bio_X509 (in, NULL, NULL, NULL)) != NULL){
			/*Discard the first because it's not a CA cert*/
			if (counter != 0) {
				CAcertificates [counter - 1] = cert;
			}
			else
				X509_free (cert);
			counter++;
			
		}
		
		if (cert)
			X509_free (cert);
		bio_cleanup ();
	}
	else {
		/* Go get the cert from data*/
	}

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_get_private_key (void             **privateKey,
				     IdentityHandle_t id_handle)
{
	/*Get the right private key from the engine or whatever*/
	/*If the setcredentials has received an engine, use that engine*/
	
	/*Get the specific participant*/
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	EVP_PKEY *fetchedKey = NULL;
	int ret;
	ENGINE *e = NULL;
	
	if (credentials->engine_based) {

		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);

		e = engines[ret];
		fetchedKey = ENGINE_load_private_key (e, credentials->info.engine.priv_key_id,
						      NULL, NULL);
		*privateKey = fetchedKey;
	}
	else if (credentials->file_based) {
		/* Go get the key from file*/
		bio_read (credentials->info.filenames.private_key_file);
		fetchedKey = PEM_read_bio_PrivateKey (in, NULL, NULL, NULL);

		bio_cleanup ();

		if (!fetchedKey)
			fatal_printf ("Error retrieving the private key");
	
		*privateKey = fetchedKey;
	}
	else {
		/* Go get the certkey from data*/
	}
	
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_sign_with_private_key (int                 type, 
					   const unsigned char *m, 
					   unsigned int        m_len,
					   unsigned char       *sigret, 
					   unsigned int        *siglen, 
					   IdentityHandle_t    id_handle)
{
	void	*privateKey;
	RSA	*rsa = NULL;

	msp_get_private_key (&privateKey, id_handle);
	rsa = ((EVP_PKEY *) privateKey)->pkey.rsa;
	
	if (!RSA_sign (type, m, m_len, sigret, siglen, rsa))
		return (DDS_RETCODE_ERROR);
	
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_verify_signature (int                 type, 
				      const unsigned char *m, 
				      unsigned int        m_len, 
				      unsigned char       *sigbuf, 
				      unsigned int        siglen, 
				      void                *ssl)
{
	
	RSA *peer_rsa_pub = X509_get_pubkey (SSL_get_peer_certificate (ssl))->pkey.rsa;

	if (!RSA_verify(type, m, m_len, sigbuf, siglen, peer_rsa_pub))
		return (DDS_RETCODE_ERROR);

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sp_get_nb_of_CA_cert (int              *nb,
				       IdentityHandle_t id_handle)
{
	/*get nb of CA cert*/
	DDS_Credentials	*credentials = (id_handles [id_handle])->credentials;
	int ret;
	
	if (credentials->engine_based) {
		dataMap data;
		data.id = credentials->info.engine.cert_id;
		data.data = NULL;

		if ((ret = engineSeek (credentials->info.engine.engine_id)) == -1 )
			return (DDS_RETCODE_BAD_PARAMETER);
		
		*nb = ENGINE_ctrl (engines [ret],
				   CMD_LOAD_CA_CERT_CTRL,
				   0, (void *)&data, NULL);
	}
	else if (credentials->file_based) {
		int counter = 0;
		X509 *cert = NULL;

		bio_read (credentials->info.filenames.certificate_chain_file);
		while ((cert = PEM_read_bio_X509 (in, NULL,NULL, NULL)) != NULL){
			counter ++;
			X509_free (cert);
		}
		*nb = counter - 1;

		bio_cleanup ();
	}
	else {
		/* Go get the cert from data*/
	}
	
	return (DDS_RETCODE_OK);
}

