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

/* sec_compat.c -- Compatibility functions with previous generation security
		   subsystem. */

#include "dds/dds_dcps.h"
#include "dds/dds_security.h"
#include "log.h"
#include "error.h"
#include "strseq.h"
#include "dds_data.h"
#include "domain.h"
#include "disc.h"
#include "security.h"
#include "sec_data.h"
#include "sec_auth.h"
#include "sec_access.h"
#include "sec_p_dtls.h"

/********************************/
/*   Authentication functions	*/
/********************************/

#ifndef DDS_NATIVE_SECURITY

/* validate_peer_identity -- Validate the identity token of a peer
			     DomainParticipant.  The resulting action type is
			     returned.  If the action is AA_CHALLENGE_NEEDED,
			     the challenge argument will be populated and
			     *challenge_length is set.
			     The *challenge_length argument needs to be set to
			     the buffer size before calling this function. */

AuthState_t validate_peer_identity (Identity_t    id,
				    void          *token,
				    unsigned char *identity,
				    size_t        identity_length,
				    unsigned char *challenge,
				    size_t        *challenge_length)
{
	Identity_t		h;
	AuthState_t		r;
	DDS_IdentityToken	rem_idt, *list;
	DDS_OctetSeq		seq;
	DDS_ReturnCode_t	err;

	ARG_NOT_USED (token)
	ARG_NOT_USED (challenge)

	DDS_DataHolder__init (&rem_idt, DDS_DTLS_ID_TOKEN_CLASS);
	rem_idt.binary_value1 = &seq;
	DDS_SEQ_INIT (seq);
	seq._buffer = identity;
	seq._maximum = seq._length = identity_length;
	rem_idt.nusers = ~0;
	list = &rem_idt;
	r = sec_validate_remote_id (id,
				    &list,
				    NULL,
				    NULL,
				    &h, &err);
	*challenge_length = 0;
	if (err)
		return (AS_FAILED);

	return (r);
}

#endif

/* get_identity_token -- Return an identity token from an Identity handle. */

DDS_ReturnCode_t get_identity_token (Identity_t    handle,
				     unsigned char *identity,
				     size_t        *identity_length)
{
	Token_t			*tokens, *token;
	DDS_IdentityToken	*idp;
	DDS_ReturnCode_t	error;

	tokens = sec_get_identity_tokens (handle, SECC_DTLS_UDP, &error);
	if (!tokens)
		return (error);

	for (token = tokens; token; token = token->next)
		if (token->encoding) {
			idp = token->data;
			if (!idp->binary_value1 ||
			    DDS_SEQ_LENGTH (*idp->binary_value1) > *identity_length ||
			    !DDS_SEQ_LENGTH (*idp->binary_value1))
				return (DDS_RETCODE_BAD_PARAMETER);

			memcpy (identity,
				DDS_SEQ_DATA (*idp->binary_value1),
				DDS_SEQ_LENGTH (*idp->binary_value1));
			break;
		}
	sec_release_id_tokens (tokens);
	return ((token) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER);
}

/* challenge_identity -- Handle an identity challenge from a peer participant. */

DDS_ReturnCode_t challenge_identity (unsigned char *challenge,
				     size_t        challenge_length,
				     unsigned char *response,
				     size_t        *response_length)
{
	ARG_NOT_USED (challenge)
	ARG_NOT_USED (challenge_length)
	ARG_NOT_USED (response)
	ARG_NOT_USED (response_length)

	/* ... TBC ... */

	return (DDS_AA_REJECTED);
}

/* validate_response -- Handle the response of an Identity challenge from a
			peer participant. */

int validate_response (unsigned char *response,
		       size_t        response_length)
{
	ARG_NOT_USED (response)
	ARG_NOT_USED (response_length)

	/* ... TBC ... */

	return (DDS_AA_REJECTED);
}


/********************************/
/*   Access Control functions	*/
/********************************/

/* validate_local_permissions -- Validate the permission credentials of a local
				 DomainParticipant.  If successful, a non-zero
				 permissions handle is returned. */

Permissions_t validate_local_permissions (DDS_DomainId_t domain_id,
					  Identity_t     handle)
{
	DDS_ReturnCode_t	ret;

	log_printf (SEC_ID, 0, "Security: validate local permissions. \r\n");

	return (sec_validate_local_permissions (handle, domain_id, NULL, &ret));
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

	if (length != sizeof (DDS_PermissionsCredential))
		return (0);

	data.domain_id = domain_id;
	data.data = permissions;
	data.length = length;
	ret = sec_access_control_request (DDS_VALIDATE_REMOTE_PERM, &data);
	if (ret)
		return (0);

	return (data.handle);
}

/* check_create_participant -- Check if a local DomainParticipant can be created
			       for the domain.  If successful, *secure will be
			       set if the domain needs to be secure. */

DDS_ReturnCode_t check_create_participant (Permissions_t                  perm,
					   const DDS_DomainParticipantQos *qos,
					   unsigned                       *secure)
{
	DDS_ReturnCode_t ret;

	ret = sec_check_create_participant (perm, NULL, qos, secure);
	return (ret);
}

/* check_create_topic -- Check if a topic may be created. */

DDS_ReturnCode_t check_create_topic (Permissions_t       permissions,
				     const char          *topic_name,
				     const DDS_TopicQos  *qos)
{
	return (sec_check_create_topic (permissions, topic_name, NULL, qos));
}

/* check_create_writer -- Check if a datawriter may be created for the given
			  topic. */

DDS_ReturnCode_t check_create_writer (Permissions_t           permissions,
				      Topic_t                 *topic,
				      const DDS_DataWriterQos *qos,
				      const Strings_t         *partitions)
{
	unsigned	secure;

	return (sec_check_create_writer (permissions, str_ptr (topic->name),
					NULL, qos, partitions, NULL, &secure));
}

/* check_create_reader -- Check if a datareader may be created for the given
			  topic. */

DDS_ReturnCode_t check_create_reader (Permissions_t           permissions,
				      Topic_t                 *topic,
				      const DDS_DataReaderQos *qos,
				      const Strings_t         *partitions)
{
	unsigned	secure;

	return (sec_check_create_reader (permissions, str_ptr (topic->name), 
					NULL, qos, partitions, NULL, &secure));
}


/* check_peer_participant -- Check if a peer DomainParticipant may be created
			     for the domain. */

DDS_ReturnCode_t check_peer_participant (Permissions_t perm,
					 String_t      *user_data)
{
	return (sec_check_remote_participant (perm, user_data));
}

/* check_peer_topic -- Check if a topic may be created. */

DDS_ReturnCode_t check_peer_topic (Permissions_t            permissions,
				   const char               *topic_name,
				   const DiscoveredTopicQos *qos)
{
	return (sec_check_remote_topic (permissions, topic_name, qos));
}

/* check_peer_writer -- Check if a datawriter may be created for the given
			topic. */

DDS_ReturnCode_t check_peer_writer (Permissions_t             permissions,
				    const char                *topic_name,
				    const DiscoveredWriterQos *qos)
{
	return (sec_check_remote_datawriter (permissions, topic_name, qos, NULL));
}

/* check_peer_reader -- Check if a datareader may be created for the given
			topic. */

DDS_ReturnCode_t check_peer_reader (Permissions_t             permissions,
				    const char                *topic_name,
				    const DiscoveredReaderQos *qos)
{
	return (sec_check_remote_datareader (permissions, topic_name, qos, NULL));
}

#ifndef DDS_NATIVE_SECURITY

/* get_permissions_token -- Return an Permissions token from a Permissions
			    handle. */

DDS_ReturnCode_t get_permissions_token (Permissions_t handle,
				        unsigned char *permissions,
					size_t        *perm_length)
{
	return (sec_get_permissions_token (handle, permissions, perm_length));
}

#endif

/***************************************/
/*   Certificate handling functions    */
/***************************************/

/* get_certificate -- Return a certificate with the specified ID. */

DDS_ReturnCode_t get_certificate (void *certificate, Identity_t handle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = handle;
	data.data = (void *) certificate;

	ret = sec_certificate_request (DDS_GET_CERT_X509, &data);
	return (ret);
}

/* get_nb_of_CA_certificates -- Return the number of CA certificates. */
 
DDS_ReturnCode_t get_nb_of_CA_certificates (int *nb, Identity_t id_handle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = id_handle;
	data.data = (void *) nb;
	
	ret = sec_certificate_request (DDS_GET_NB_CA_CERT, &data);
	return (ret);
}

/* get_CA_certificate_list -- Return a CA certificate from the certificate
			      with the specified ID. */

DDS_ReturnCode_t get_CA_certificate_list (void *CAcertificates, Identity_t id_handle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = id_handle;
	data.data = (void *) CAcertificates;
	
	ret = sec_certificate_request (DDS_GET_CA_CERT_X509, &data);
	return (ret);
}

# if 0
/* verify_signature -- Verify the signature of the peer. */

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

	ret = sec_certificate_request (DDS_VERIFY_SIGNATURE, &data);
	return (ret);
}
# endif

/* get_private_key -- Return the private key. */

DDS_ReturnCode_t get_private_key (void *privateKey, Identity_t id_handle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	data.handle = id_handle;
	data.data = (void *) privateKey;

	ret = sec_certificate_request (DDS_GET_PRIVATE_KEY_X509, &data);
	return (ret);
}


/*************************/
/*   Utility functions.	 */
/*************************/

/* get_domain_security -- Return the domain security parameters. */

uint32_t get_domain_security (DDS_DomainId_t domain)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_policy)
		return (0);

	data.domain_id = domain;
	ret = sec_aux_request (DDS_GET_DOMAIN_SEC_CAPS, &data);
	if (ret)
		return (0);

	return (data.secure);
}

/* check_DTLS_handshake_initiator -- When a Client Hello message is received,
				     check if the peer is valid. The rest of
				     the handshake is not needed if invalid. */

int check_DTLS_handshake_initiator (struct sockaddr *addr,
				    unsigned        domain_id)
{
	ARG_NOT_USED (addr);
	ARG_NOT_USED (domain_id);

	return (DDS_AA_ACCEPTED);
}

struct addr2perm {
	Locator_t           *loc;
	Permissions_t perm;
	Participant_t       *pp;
};

/* peer_walk -- Walk through the skiplist and find the participant with the
		same locator */

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

/* get_permissions_handle -- Get the permissions handle based on the locator.*/

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

/* accept_ssl_connection -- Validate a peer over an SSL connection. */

int accept_ssl_connection (SSL             *ssl, 
			   struct sockaddr *addr, 
			   unsigned        domain_id)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	Permissions_t     	perm = 0;
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
	ret = sec_aux_request (DDS_ACCEPT_SSL_CX, &data);
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

