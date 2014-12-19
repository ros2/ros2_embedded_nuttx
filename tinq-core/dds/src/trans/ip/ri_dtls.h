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

/* ri_dtls.h -- Interface to the RTPS over DTLS/UDP protocol stack. */

#ifndef __ri_dtls_h_
#define __ri_dtls_h_

#include "ri_data.h"

extern int	dtls_available;		/* Always exists: status of DTLS. */

/* Following variables/functions are only available if -DDDS_SECURITY */

void rtps_dtls_init (void);

void rtps_dtls_finish (void);

void rtps_dtls_send (unsigned id, Locator_t *lp, LocatorList_t *next, RMBUF *msgs);

/* Send RTPS messages to the secure locator (lp). */

void rtps_dtls_attach_server (IP_CX *cxp);

/* Attach a DTLS server to the IP_CX. */

void rtps_dtls_cleanup_cx (IP_CX *cxp);

/* Cleanup the DTLS specific bits of IP_CX */

void rtps_dtls_final (void);

/* Finalize the DTLS subsystem. All related resources will be freed */

#endif /* !__ri_dtls_h_ */

