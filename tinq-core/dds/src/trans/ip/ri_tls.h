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

/* ri_tls.h -- Interface to the RTPS over SSL/TLS over TCP protocol stack. */

#ifndef __ri_tls_h_
#define __ri_tls_h_

#include "ri_data.h"

extern int	tls_available;		/* Always exists: status of SSL/TLS. */

void rtps_tls_init (void);

/* Initialize the SSL/TLS support. */

void rtps_tls_finish (void);

/* Finalize the SSL/TLS support. */

#endif /* !__ri_tls_h_ */

