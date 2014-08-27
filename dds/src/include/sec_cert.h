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

/* sec_cert.h -- Plugin functions for Certificate management. */

#ifndef __sec_cert_h_
#define __sec_cert_h_

#include "sec_data.h"

DDS_ReturnCode_t sec_validate_remote_certificate (unsigned char *certificate,
						  size_t        length);

/* Verify if a certificate is valid. */

DDS_ReturnCode_t sec_get_certificate (Identity_t    handle,
				      unsigned char *certificate,
				      size_t        *length);

/* Return a certificate with the specified ID. */

DDS_ReturnCode_t sec_get_CA_certificate_list (Identity_t    id_handle,
					      unsigned char *CAcertificates,
					      size_t        *length);

/* Return CA certificates from the certificate with the specified ID. */


DDS_ReturnCode_t sec_sign_with_private_key (int                 type,
					    const unsigned char *m,
					    size_t              m_len,
				            unsigned char       *sigret,
					    size_t              *siglen, 
				            Identity_t          id_handle)

/* Sign a piece of data with the private key with the specified ID. */

DDS_ReturnCode_t verify_signature (int                 type,
				   const unsigned char *m,
				   size_t              m_len, 
				   unsigned char       *sigbuf,
				   size_t              siglen, 
				   Identity_t          peer_id_handle,
				   void                *security_context);

/* Verify the signature of the peer. */

DDS_ReturnCode_t get_private_key (void *privateKey, Identity_t id_handle);

/* Return the private key. */

DDS_ReturnCode_t get_nb_of_CA_certificates (int *nb, Identity_t id_handle);

/* Return the number of CA certificates. */

#endif /* !__sec_cert_h_ */

