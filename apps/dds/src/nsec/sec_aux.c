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

/* sec_aux.c -- Implements the auxiliary security support functions. */

#include "dds/dds_security.h"

#ifdef DDS_SECURITY

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

/* accept_ssl_connection -- Validate a peer over an SSL connection. */

DDS_AuthAction_t accept_ssl_connection (SSL             *ssl, 
					struct sockaddr *addr, 
					unsigned        domain_id)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	PermissionsHandle_t     perm = 0;
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

/* check_DTLS_handshake_initiator -- When a Client Hello message is received,
				     it can be useful to check if the peer is
				     valid or invalid. When the peer is invalid,
				     the rest of the handshake should not be
				     performed. */

DDS_AuthAction_t check_DTLS_handshake_initiator (struct sockaddr *addr,
					         unsigned        domain_id)
{
	/*if (!get_permissions_handle (addr, domain_id, NULL))
	  return (DDS_AA_REJECTED);*/
	
	ARG_NOT_USED (addr);
	ARG_NOT_USED (domain_id);

	return (DDS_AA_ACCEPTED);
}

#endif /* DDS_SECURITY */

