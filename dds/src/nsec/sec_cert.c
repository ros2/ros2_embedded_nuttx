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

/* sec_cert.c -- Implements the access to the plugin for certificate management. */

#include "dds/dds_security.c"

#ifdef DDS_SECURITY

/* get_certificate -- get the certificate for this particular identity handle*/

DDS_ReturnCode_t get_certificate (void             *certificate,
				  IdentityHandle_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	data.handle = identityHandle;
	data.data = (void *) certificate;
	if (!plugin_fct)
		return (DDS_RETCODE_UNSUPPORTED);

	return (sec_certificate_request (DDS_GET_CERT, &data));
}

/* get_nb_of_CA_certificates -- to get the number of CA certificates */

DDS_ReturnCode_t get_nb_of_CA_certificates (int              *nb, 
					    IdentityHandle_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_fct)
		return (DDS_RETCODE_UNSUPPORTED);

	data.handle = identityHandle;
	data.data = (void *) nb;

	return (sec_certificate_request (DDS_GET_NB_CA_CERT, &data));
}

/* get_CA_certificate_list -- to get all the trusted CA certificates */

DDS_ReturnCode_t get_CA_certificate_list (void             *CAcertificates, 
					  IdentityHandle_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	if (!plugin_fct) {
		return (DDS_RETCODE_UNSUPPORTED);

	data.handle = identityHandle;
	data.data = (void *) CAcertificates;
	
	return (sec_certificate_request (DDS_GET_CA_CERT, &data));
}

/* sign_with_private_key -- sign m with the private key from a particular identity */

DDS_ReturnCode_t sign_with_private_key (int                 type, 
				        const unsigned char *m, 
				        size_t              m_len,
				        unsigned char       *sigret, 
				        size_t              *siglen, 
				        IdentityHandle_t    id_handle)
{

	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	if (!plugin_fct) {
		return (DDS_RETCODE_UNSUPPORTED);

	data.handle = id_handle;
	data.length = (size_t) m_len;
	data.data = (void *) m;
	data.rdata = (void *) sigret;
	data.rlength = *siglen;
	data.secure = type;
	
	ret = sec_certificate_request (DDS_SIGN_WITH_PRIVATE_KEY, &data);
	if (ret) {
		*siglen = -1;
		return (ret);
	}
	*siglen = data.rlength;
	return (ret);
}

/* verify_signature -- verify a signature */

DDS_ReturnCode_t verify_signature (int                 type, 
				   const unsigned char *m, 
				   size_t              m_len, 
				   unsigned char       *sigbuf, 
				   size_t              siglen, 
				   IdentityHandle_t    peer_id_handle, 
				   void                *security_context)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	if (!plugin_fct) {
		return (DDS_RETCODE_UNSUPPORTED);

	data.handle = peer_id_handle;
	data.length = (size_t) m_len;
	data.data = (void *) security_context;
	data.rdata = (void *) sigbuf;
	data.rlength = siglen;
	data.name = (const char *) m;
	data.secure = type;

	return (sec_certificate_request (DDS_VERIFY_SIGNATURE, &data));
}

/* get_private_key -- get the private key from a certain identity */

DDS_ReturnCode_t get_private_key (void             *privateKey, 
				  IdentityHandle_t identityHandle)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;
	
	if (!plugin_fct) {
		return (DDS_RETCODE_UNSUPPORTED);

	data.handle = identityHandle;
	data.data = (void *) privateKey;

	return (sec_certificate_request (DDS_GET_PRIVATE_KEY, &data));
}

#endif /* DDS_SECURITY */

