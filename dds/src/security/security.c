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

/* security.c -- Security checks for DDS. */

#include <stdio.h>
#include "log.h"
#include "error.h"
#include "dds/dds_security.h"
#include "dds_data.h"
#include "domain.h"
#include "disc.h"
#include "security.h"

#ifdef DDS_SECURITY
#include "thread.h"
#include <openssl/crypto.h>
#include <openssl/ssl.h>

int dds_openssl_init_global = 1;

static lock_t	*ssl_mutexes;

#define	MAX_CERTIFICATES	10

Identity_t		local_identity;
Permissions_t		local_permissions;
DDS_SecurityPolicy	plugin_policy;

static DDS_SecurityPluginFct	plugin_fct;

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

void DDS_Security_set_library_init (int val)
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

static unsigned long id_function(void)
{
	return (thread_id ());
}

void DDS_Security_set_library_lock (void)
{
	int i;
	ssl_mutexes = xmalloc (CRYPTO_num_locks () * sizeof (lock_t));
	if (!ssl_mutexes)
		return;

	for (i = 0; i < CRYPTO_num_locks (); i++)
		lock_init_nr (ssl_mutexes [i], "OpenSSL");

	CRYPTO_set_id_callback (id_function);
	CRYPTO_set_locking_callback (lock_function);
}

void DDS_Security_unset_library_lock (void)
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

DDS_ReturnCode_t DDS_Security_set_policy (DDS_SecurityPolicy    policy,
					  DDS_SecurityPluginFct fct)
{
	static const char *policy_str [] = {
		"No security",
		"Local security checks",
		"Agent-based security checks"
	};

	if (policy > DDS_SECURITY_AGENT ||
	    (policy && !fct) || 
	    policy < plugin_policy)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (policy) {
		log_printf (DDS_ID, 0, "DDS: Security policy update: %s -> %s\r\n", 
					policy_str [plugin_policy], policy_str [policy]);
		plugin_policy = policy;
		plugin_fct = fct;
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_revoke_participant (DDS_DomainId_t id,
					 DDS_InstanceHandle_t part)
{
	DDS_DomainParticipant dp;

	dp = DDS_DomainParticipantFactory_lookup_participant (id);

	return (DDS_DomainParticipant_ignore_participant (dp, part));
}

/* DDS_Security_set_credentials -- Set the security policy.  This should only be
				   done once and before any credentials are
				   assigned, or any DomainParticipants are
				   created! */

DDS_ReturnCode_t DDS_Security_set_credentials (const char      *name,
					       DDS_Credentials *crp)
{
	DDS_Credentials	*credentials;
	char		*dp;
	const char	*sp;
       	unsigned	i, nbOfCert;
	size_t		n, xlength;
	X509            *cert_ori = NULL, *cert_cpy = NULL;

	if (!plugin_policy)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	if (!crp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (crp->credentialKind == DDS_FILE_BASED) {
		log_printf (SEC_ID, 0, "Security: set file based credentials\r\n");
		if (!crp->info.filenames.private_key_file ||
	            !crp->info.filenames.certificate_chain_file)
			return (DDS_RETCODE_BAD_PARAMETER);

		xlength = sizeof (DDS_Credentials) +
		          strlen (crp->info.filenames.private_key_file) +
			  strlen (crp->info.filenames.certificate_chain_file) +
			  2;
		credentials = malloc (xlength);
		if (!credentials)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		credentials->credentialKind = DDS_FILE_BASED;

		dp = (char *) (credentials + 1);
		sp = crp->info.filenames.private_key_file;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.filenames.private_key_file = dp;
		dp += n;
		sp = crp->info.filenames.certificate_chain_file;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.filenames.certificate_chain_file = dp;
	}
	else if (crp->credentialKind == DDS_ENGINE_BASED) {
		log_printf (SEC_ID, 0, "Security: set engine based credentials\r\n");
		if (!crp->info.engine.engine_id ||
		    !crp->info.engine.cert_id ||
		    !crp->info.engine.priv_key_id)
			return (DDS_RETCODE_BAD_PARAMETER);
			
		xlength = sizeof (DDS_Credentials) +
		          strlen (crp->info.engine.engine_id) +
			  strlen (crp->info.engine.cert_id) +
			  strlen (crp->info.engine.priv_key_id) +
			  3;
		credentials = malloc (xlength);
		if (!credentials)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		credentials->credentialKind = DDS_ENGINE_BASED;

		dp = (char *) (credentials + 1);
		sp = crp->info.engine.engine_id;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.engine.engine_id = dp;
		dp += n;
		sp = crp->info.engine.cert_id;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.engine.cert_id = dp;
		dp += n;
		sp = crp->info.engine.priv_key_id;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.engine.priv_key_id = dp;

		log_printf (SEC_ID, 0, "Security: credentials \n\r cert path = %s \n\r key path = %s \n\r engine_id = %s\r\n", credentials->info.engine.cert_id, credentials->info.engine.priv_key_id, credentials->info.engine.engine_id);
	} 
	else if (crp->credentialKind == DDS_SSL_BASED) {
		log_printf (SEC_ID, 0, "Security: set SSL based credentials\r\n");
		if (!crp->info.sslData.certificate_list ||
		    !crp->info.sslData.private_key)
			return (DDS_RETCODE_BAD_PARAMETER);
			
		xlength = sizeof (DDS_Credentials) +
			sizeof (crp->info.sslData.private_key) +
			sizeof (crp->info.sslData.certificate_list);
		credentials = malloc (xlength);
		if (!credentials)
			return (DDS_RETCODE_OUT_OF_RESOURCES);


		credentials->credentialKind = DDS_SSL_BASED;


		/* make a new empty stack object */
		credentials->info.sslData.certificate_list = sk_X509_new_null ();
		
		nbOfCert = sk_num ((const _STACK*) crp->info.sslData.certificate_list);
		/* copy every certificate to the new stack */
		for (i = 0; i < (unsigned) nbOfCert; i++) {
#ifdef DDS_DEBUG
			log_printf (SEC_ID, 0, "cert %d is: \r\n", i);

			BIO *dbg_out = BIO_new(BIO_s_mem());
			char dbg_out_string[256];
			int size;
			X509_NAME_print_ex (dbg_out, X509_get_subject_name (sk_X509_value (crp->info.sslData.certificate_list, i)),
					       1, XN_FLAG_MULTILINE);
			while(1) {
				size = BIO_read(dbg_out, &dbg_out_string, 255);
				if (size <= 0) {
					break;
				}
				dbg_out_string[size] = '\0';
				log_printf (SEC_ID, 0, dbg_out_string);
			}
			BIO_free(dbg_out);
			log_printf (SEC_ID, 0, "\r\n");
#endif		

			cert_ori = sk_X509_value (crp->info.sslData.certificate_list, i);
			cert_cpy = X509_dup (cert_ori);
			sk_X509_push (credentials->info.sslData.certificate_list,
				      cert_cpy);
		}

		/* There is no OpenSSL function to copy a private key
		   so just reference to it and then up the reference count */
		credentials->info.sslData.private_key = crp->info.sslData.private_key;
		credentials->info.sslData.private_key->references ++;
		log_printf (SEC_ID, 0, "Security: the private key and certificates are set.\r\n");
	}
	else {
		log_printf (SEC_ID, 0, "Security: set data based credentials\r\n");
		if (!crp->info.data.private_key.data ||
		    !crp->info.data.private_key.length ||
		    !crp->info.data.num_certificates ||
		    !crp->info.data.num_certificates > MAX_CERTIFICATES ||
		    !crp->info.data.certificates [0].data ||
		    !crp->info.data.certificates [0].length)
			return (DDS_RETCODE_BAD_PARAMETER);

		xlength = sizeof (DDS_Credentials) +
			  crp->info.data.num_certificates * sizeof (DDS_Credential) +
			  crp->info.data.private_key.length +
			  crp->info.data.certificates [0].length;
		for (i = 1; i < crp->info.data.num_certificates; i++) {
			if (!crp->info.data.certificates [i].data ||
			    !crp->info.data.certificates [i].length)
				return (DDS_RETCODE_BAD_PARAMETER);

			xlength += crp->info.data.certificates [i].length;
		}
		credentials = malloc (xlength);
		if (!credentials)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		credentials->credentialKind = DDS_DATA_BASED;
		credentials->info.data.private_key.format = crp->info.data.private_key.format;
		n = crp->info.data.private_key.length;
		credentials->info.data.private_key.length = n;
		dp = (char *) (credentials + 1);
		sp = crp->info.data.private_key.data;
		n = crp->info.data.private_key.length;
		memcpy (dp, sp, n);
		credentials->info.data.private_key.data = dp;
		dp += n;
		for (i = 0; i < crp->info.data.num_certificates; i++) {
			credentials->info.data.certificates [i].format = crp->info.data.certificates [i].format;
			n = crp->info.data.certificates [i].length;
			credentials->info.data.certificates [i].length = n;
			sp = crp->info.data.certificates [i].data;
			memcpy (dp, sp, n);
			credentials->info.data.certificates [i].data = dp;
			dp += n;
		}
	}

	/* Credentials are now stored locally in one big block, ready for
	   validation. Validate it now. */
	
	local_identity = validate_local_identity (name, credentials, xlength);
	if (!local_identity) {
		log_printf (SEC_ID, 0, "Security: ACCESS is denied\r\n");
		return (DDS_RETCODE_ACCESS_DENIED);
	}
    
	return (DDS_RETCODE_OK);
}

#define	ADJUST_PTR(p, px)	px = (void *) ((char *)(px) - (char *)(p))

/* validate_local_identity -- Validate the credentials of a local
			      DomainParticipant.  If successful, a non-zero
			      identity handle is returned. */

Identity_t validate_local_identity (const char      *name,
					  DDS_Credentials *credentials,
					  size_t          xlength)
{
	DDS_SecurityReqData	data;
	unsigned		i;
	DDS_ReturnCode_t	ret;

	data.name = name;
	data.data = credentials;
	data.length = xlength;
	if (plugin_policy == DDS_SECURITY_AGENT) {

		/* Make pointers so they're relative! */
		if (credentials->credentialKind == DDS_FILE_BASED) {
			ADJUST_PTR (credentials, credentials->info.filenames.private_key_file);
			ADJUST_PTR (credentials, credentials->info.filenames.certificate_chain_file);
		}
		else if (credentials->credentialKind == DDS_ENGINE_BASED) {
			ADJUST_PTR (credentials, credentials->info.engine.engine_id);
			ADJUST_PTR (credentials, credentials->info.engine.cert_id);
			ADJUST_PTR (credentials, credentials->info.engine.priv_key_id);
		}
		else if (credentials->credentialKind == DDS_SSL_BASED)	{
			ADJUST_PTR (credentials, credentials->info.sslData.private_key);
			ADJUST_PTR (credentials, credentials->info.sslData.certificate_list);
		}
		else {
			ADJUST_PTR (credentials, credentials->info.data.private_key.data);
			for (i = 0; i < credentials->info.data.num_certificates; i++)
				ADJUST_PTR (credentials, credentials->info.data.certificates [i].data);
		}
	}
	log_printf (SEC_ID, 0, "Security: call security plugin\r\n");
	ret = (*plugin_fct) (DDS_VALIDATE_LOCAL_ID, &data);
	if (ret || plugin_policy == DDS_SECURITY_AGENT) {
		memset (credentials, 0, xlength);
		free (credentials);
	}
	if (ret) {
		log_printf (SEC_ID, 0, "Security: The returned handle is 0 --> problem\r\n");
		return (0);
	}
	return (data.handle);
}

DDS_ReturnCode_t set_local_handle (Permissions_t  perm,
				   DDS_DomainId_t id )
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	log_printf (SEC_ID, 0, "Security: set local handle\r\n");

	if (!plugin_policy)
		return (DDS_AA_ACCEPTED);
	
	data.handle = perm;
	data.domain_id = id;
	ret = (*plugin_fct) (DDS_SET_LOCAL_HANDLE, &data);
	return (ret);
}

DDS_ReturnCode_t set_peer_handle (Permissions_t        perm,
				  DDS_InstanceHandle_t handle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	log_printf (SEC_ID, 0, "Security: set peer handle\r\n");

	if (!plugin_policy)
		return (DDS_AA_ACCEPTED);
	
	data.handle = perm;
	data.secure = handle;
	ret = (*plugin_fct) (DDS_SET_PEER_HANDLE, &data);
	return (ret);
}

/* validate_peer_identity -- Validate the identity token of a peer
			     DomainParticipant.  The resulting action type is
			     returned.  If the action is AA_CHALLENGE_NEEDED,
			     the challenge argument will be populated. */

int validate_peer_identity (Identity_t    id,
			    void	  *token,
			    unsigned char *identity,
			    size_t        identity_length,
			    unsigned char *challenge,
			    size_t        *challenge_length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	ARG_NOT_USED (id)
	ARG_NOT_USED (token)

	log_printf (SEC_ID, 0, "Security: validate peer identity\r\n");

	if (!plugin_policy) {
		if (challenge_length)
			*challenge_length = 0;
		return (DDS_AA_ACCEPTED);
	}
	if (!challenge_length)
		return (DDS_AA_REJECTED);

	data.data = identity;
	data.length = identity_length;
	data.rdata = challenge;
	data.rlength = *challenge_length;
	ret = (*plugin_fct) (DDS_VALIDATE_PEER_ID, &data);
	if (ret || data.action == DDS_AA_REJECTED) {
		*challenge_length = 0;
		return (DDS_AA_REJECTED);
	}
	if (data.action == DDS_AA_HANDSHAKE) {
		memcpy (challenge, data.data, data.rlength);
		*challenge_length = data.rlength;
	}
	else
		*challenge_length = 0;
	return (data.action);
}


struct addr2perm {
	Locator_t       *loc;
	Permissions_t	perm;
	Participant_t   *pp;
};

/* Walk through the skiplist and find the participant with the same locator */

static int peer_walk (Skiplist_t *list, void *node, void *arg)
{
	struct addr2perm *a2p = (struct addr2perm *) arg;
	Participant_t    *p, **pp = (Participant_t **) node;
	Locator_t        *to_find = a2p->loc;
	LocatorList_t    locList;

	ARG_NOT_USED (list)

	p = *pp;
	locList = p->p_sec_locs;
	
	if (locator_list_search (locList, 
				 to_find->kind,
				 to_find->address, 
				 to_find->port) != -1) {
		a2p->perm = p->p_proxy.permissions;
		a2p->pp = p;
		return (0);
	}
	return (1);
}

/* Get the premissions handle based on the locator */

static Permissions_t get_permissions_handle (struct sockaddr *addr, 
					     unsigned        id, 
					     Participant_t   **pp)
{
	struct addr2perm a2p;
	Locator_t        loc;
	Domain_t         *dp = domain_get (id, 0, NULL);
	
	if (!dp)
		return (0);

	sockaddr2loc (&loc, addr);
	a2p.loc = &loc;
	a2p.perm = 0;
	a2p.pp = NULL;
	sl_walk ( &dp->peers, peer_walk, (void*) &a2p);
	if (pp)
		*pp = a2p.pp;

	return (a2p.perm);
}

/* check_DTLS_handshake_initiator -- Do extra check check for DTLS handshake. */

int check_DTLS_handshake_initiator (struct sockaddr *addr,
				    unsigned        domain_id)
{
	/*if (!get_permissions_handle (addr, domain_id, NULL))
	  return (DDS_AA_REJECTED);*/
	
	ARG_NOT_USED (addr);
	ARG_NOT_USED (domain_id);

	return (DDS_AA_ACCEPTED);
}

/* accept_ssl_connection -- Check if a TLS/SSL connection is allowed. */

int accept_ssl_connection (SSL             *ssl,
			   struct sockaddr *addr,
			   unsigned        domain_id)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	Permissions_t		perm = 0;
	Participant_t           *pp;

	if (!plugin_policy) {
		log_printf (RTPS_ID, 0, "Can't accept: no plugin policy!\r\n");
		return (1);
	}

	/* The permissions are found based on the locator,
	   based on the permissions we can find the participant name. */
	perm = get_permissions_handle (addr, domain_id, &pp);
	if (!perm) {
		log_printf (RTPS_ID, 0, "Can't accept: invalid permissions!\r\n");
		return (DDS_AA_REJECTED);
	}
	data.handle = perm;
	data.data = ssl;
	data.rdata = addr;
	ret = (*plugin_fct) (DDS_ACCEPT_SSL_CX, &data);
	if (ret) {
		log_printf (RTPS_ID, 0, "Can't accept: plugin doesn't allow!\r\n");
		return (DDS_AA_REJECTED);
	}
	if (data.action == DDS_AA_REJECTED && pp) {
		log_printf (SEC_ID, 0, "Participant will be ignored\r\n");
		disc_ignore_participant (pp);
	}
	return (data.action);
}

/* Return an identity token from an Identity handle. */

DDS_ReturnCode_t get_identity_token (Identity_t    handle,
				     unsigned char *identity,
				     size_t        *identity_length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy) {
		*identity_length = 0;
		return (DDS_RETCODE_OK);
	}
	data.handle = handle;
	data.rdata = identity;
	data.rlength = *identity_length;
	ret = (*plugin_fct) (DDS_GET_ID_TOKEN, &data);
	if (ret || !data.rlength) {
		*identity_length = 0;
		return (ret);
	}
	*identity_length = data.rlength;
	return (DDS_RETCODE_OK);
}

/* challenge_identity -- Handle an identity challenge from a peer participant.*/

DDS_ReturnCode_t challenge_identity (unsigned char *challenge,
				     size_t        challenge_length,
				     unsigned char *response,
				     size_t        *response_length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy) {
		*response_length = 0;
		return (DDS_RETCODE_OK);
	}
	data.data = challenge;
	data.length = challenge_length;
	data.rdata = response;
	data.rlength = *response_length;
	ret = (*plugin_fct) (DDS_CHALLENGE_ID, &data);
	if (ret || !data.length) {
		*response_length = 0;
		return (ret);
	}
	*response_length = data.length;
	return (DDS_RETCODE_OK);
}

/* validate_response -- Handle the response of an Identity challenge from a
			peer participant. */

int validate_response (unsigned char *response,
				    size_t        response_length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.data = response;
	data.length = response_length;
	ret = (*plugin_fct) (DDS_VALIDATE_RESPONSE, &data);
	return (ret);
}

/* validate_local_permissions -- Validate the permission credentials of a local
				 DomainParticipant.  If successful, a non-zero
				 permissions handle is returned. */

Permissions_t validate_local_permissions (DDS_DomainId_t domain_id,
					  Identity_t     handle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	log_printf (SEC_ID, 0, "Security: validate local permissions. \r\n");

	if (!plugin_policy)
		return (1);

	data.domain_id = domain_id;
	data.handle = handle;
	ret = (*plugin_fct) (DDS_VALIDATE_LOCAL_PERM, &data);
	if (ret)
		return (0);

	return (data.handle);
}

/* validate_peer_permissions -- Validate the permissions token of a peer
				DomainParticipant.  The resulting permissions
				handle is returned, or 0, if the particant is
				rejected. */

Permissions_t validate_peer_permissions (DDS_DomainId_t domain_id,
					 unsigned char  *permissions,
					 size_t         length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (1);

	data.domain_id = domain_id;
	data.data = permissions;
	data.length = length;
	ret = (*plugin_fct) (DDS_VALIDATE_PEER_PERM, &data);
	if (ret)
		return (0);

	return (data.handle);
}

/* check_create_participant -- Check if a local DomainParticipant can be
			       created for the domain. */

DDS_ReturnCode_t check_create_participant (Permissions_t                  perm,
					   const DDS_DomainParticipantQos *qos,
					   unsigned                       *secure)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy) {
		*secure = 0;
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.data = (void *) qos;
	ret = (*plugin_fct) (DDS_CHECK_CREATE_PARTICIPANT, &data);
	if (!ret)
		*secure = data.secure;
	return (ret);
}

/* check_create_topic -- Check if a topic may be created. */

DDS_ReturnCode_t check_create_topic (Permissions_t      permissions,
				     const char         *topic_name,
				     const DDS_TopicQos *qos)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = permissions;
	data.name = topic_name;
	data.data = (void *) qos;
	ret = (*plugin_fct) (DDS_CHECK_CREATE_TOPIC, &data);
	return (ret);
}

/* check_create_writer -- Check if a datawriter may be created for the given
			  topic. */

DDS_ReturnCode_t check_create_writer (Permissions_t           permissions,
				      Topic_t                 *topic,
				      const DDS_DataWriterQos *qos,
				      const Strings_t         *partitions)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = permissions;
	data.name = str_ptr (topic->name);
	data.data = (void *) qos;
	data.rdata = (void *) partitions;
	ret = (*plugin_fct) (DDS_CHECK_CREATE_WRITER, &data);
	return (ret);
}


/* check_create_reader -- Check if a datareader may be created for the given
			  topic. */

DDS_ReturnCode_t check_create_reader (Permissions_t           permissions,
				      Topic_t                 *topic,
				      const DDS_DataReaderQos *qos,
				      const Strings_t         *partitions)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = permissions;
	data.name = str_ptr (topic->name);
	data.data = (void *) qos;
	data.rdata = (void *) partitions;
	ret = (*plugin_fct) (DDS_CHECK_CREATE_READER, &data);
	return (ret);
}

/* check_peer_participant -- Check if a peer DomainParticipant may be created
			     for the domain. */

DDS_ReturnCode_t check_peer_participant (Permissions_t perm,
					 String_t      *user_data)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = perm;
	data.data = user_data;
	ret = (*plugin_fct) (DDS_CHECK_PEER_PARTICIPANT, &data);
	return (ret);
}

/* check_peer_topic -- Check if a topic may be created. */

DDS_ReturnCode_t check_peer_topic (Permissions_t            permissions,
				   const char               *topic_name,
				   const DiscoveredTopicQos *qos)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = permissions;
	data.name = topic_name;
	data.data = (void *) qos;
	ret = (*plugin_fct) (DDS_CHECK_PEER_TOPIC, &data);
	return (ret);
}

/* check_peer_writer -- Check if a datawriter may be created for the given
			topic. */

DDS_ReturnCode_t check_peer_writer (Permissions_t             permissions,
				    const char                *topic_name,
				    const DiscoveredWriterQos *qos)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = permissions;
	data.name = topic_name;
	data.data = (void *) qos;
	ret = (*plugin_fct) (DDS_CHECK_PEER_WRITER, &data);
	return (ret);
}

/* check_peer_reader -- Check if a datareader may be created for the given 
			topic. */

DDS_ReturnCode_t check_peer_reader (Permissions_t             permissions,
				    const char                *topic_name,
				    const DiscoveredReaderQos *qos)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (DDS_RETCODE_OK);

	data.handle = permissions;
	data.name = topic_name;
	data.data = (void *) qos;
	ret = (*plugin_fct) (DDS_CHECK_PEER_READER, &data);
	return (ret);
}

/* Return an Permissions token from a Permissions handle. */

DDS_ReturnCode_t get_permissions_token (Permissions_t handle,
				        unsigned char *permissions,
					size_t        *perm_length)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy) {
		*perm_length = 0;
		return (DDS_RETCODE_OK);
	}
	data.handle = handle;
	data.rdata = permissions;
	data.rlength = *perm_length;
	ret = (*plugin_fct) (DDS_GET_PERM_TOKEN, &data);
	if (ret || !data.rlength) {
		*perm_length = 0;
		return (ret);
	}
	*perm_length = data.rlength;
	return (DDS_RETCODE_OK);
}

uint32_t get_domain_security (DDS_DomainId_t domain)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (0);

	data.domain_id = domain;
	ret = (*plugin_fct) (DDS_GET_DOMAIN_SEC_CAPS, &data);
	if (ret)
		return (0);

	return (data.secure);
}

/* get_certificate -- get the certificate for this particular identity handle*/

DDS_ReturnCode_t get_certificate (void       *certificate,
				  Identity_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = identityHandle;
	data.data = (void *) certificate;
	
	if (plugin_fct)
		ret = (*plugin_fct) (DDS_GET_CERT, &data);
	else
		ret = DDS_RETCODE_UNSUPPORTED;

	return (ret);
}

/* get_nb_of_CA_certificates -- to get the number of CA certificates */

DDS_ReturnCode_t get_nb_of_CA_certificates (int       *nb, 
					   Identity_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = identityHandle;
	data.data = (void *) nb;
	
	if (plugin_fct)
		ret = (*plugin_fct) (DDS_GET_NB_CA_CERT, &data);
	else
		ret = DDS_RETCODE_UNSUPPORTED;

	return (ret);
}

/* get_CA_certificate_list -- to get all the trusted CA certificates */

DDS_ReturnCode_t get_CA_certificate_list (void       *CAcertificates, 
					  Identity_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = identityHandle;
	data.data = (void *) CAcertificates;
	
	if (plugin_fct)
		ret = (*plugin_fct) (DDS_GET_CA_CERT, &data);
	else
		ret = DDS_RETCODE_UNSUPPORTED;

	return (ret);
}

/* sign_with_private_key -- sign m with the private key from a particular identity */

DDS_ReturnCode_t sign_with_private_key(int                 type, 
				       const unsigned char *m, 
				       size_t              m_len,
				       unsigned char       *sigret, 
				       size_t              *siglen, 
				       Identity_t          id_handle)
{

	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = id_handle;
	data.length = (size_t) m_len;
	data.data = (void *) m;
	data.rdata = (void *) sigret;
	data.rlength = *siglen;
	data.secure = type;
	
	if (plugin_fct) {
		ret = (*plugin_fct) (DDS_SIGN_WITH_PRIVATE_KEY, &data);
		if (ret) {
			*siglen = -1;
			return (ret);
		}
		*siglen = data.rlength;
	}
	else
		ret = DDS_RETCODE_UNSUPPORTED;

	return (ret);
}

/* verify_signature -- verify a signature */

DDS_ReturnCode_t verify_signature (int                 type, 
				   const unsigned char *m, 
				   size_t              m_len, 
				   unsigned char       *sigbuf, 
				   size_t              siglen, 
				   Identity_t          peer_id_handle, 
				   void                *security_context)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	data.handle = peer_id_handle;
	data.length = (size_t) m_len;
	data.data = (void *) security_context;
	data.rdata = (void *) sigbuf;
	data.rlength = siglen;
	data.name = (const char *) m;
	data.secure = type;

	if (plugin_fct)
		ret = (*plugin_fct) (DDS_VERIFY_SIGNATURE, &data);
	else
		ret = DDS_RETCODE_UNSUPPORTED;

	return (ret);
}

/* get_private_key -- get the private key from a certain identity */

DDS_ReturnCode_t get_private_key (void       *privateKey, 
				  Identity_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = identityHandle;
	data.data = (void *) privateKey;

	if (plugin_fct)
		ret = (*plugin_fct) (DDS_GET_PRIVATE_KEY, &data);
	else
		ret = DDS_RETCODE_UNSUPPORTED;
	
	return (ret);
}

#endif /* DDS_SECURITY */

