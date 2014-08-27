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

/* sp_aux.h -- Security Plugin - Auxiliary functions. */

#ifndef __sp_aux_h_
#define __sp_aux_h_

DDS_ReturnCode_t sp_get_domain_sec_caps (DDS_DomainId_t domain_id,
					 unsigned       *sec_caps);

/* Get domain security capabilities. */

DDS_ReturnCode_t sp_validate_ssl_connection (void             *ssl_cx,
					     struct sockaddr  *sp,
					     DDS_AuthAction_t *action);

#endif /* !__sp_aux_h_ */

