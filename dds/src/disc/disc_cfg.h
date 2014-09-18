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

/* disc_cfg.h -- Configuration of discovery module. */

#ifndef __disc_cfg_h_
#define	__disc_cfg_h_

/*#define TOPIC_DISCOVERY	** Optional Topic Discovery. */
/*#define DISC_MSG_DUMP		** Dump Participant messages. */
/*#define DISC_EV_TRACE		** Trace Discovery events. */
/*#define PSMP_TRACE_MSG	** P2P participant stateless message trace. */
#define CTT_TRACE_MSG		/* Secure Token transport message trace. */
/*#define CTT_TRACE_DATA	** Secure Token transport message contents trace. */
#define	CTT_TRACE_RTPS		/* Trace Secure Token transport on RTPS level. */

#if !defined (SIMULATION) && !defined (STATIC_DISCOVERY)
#define	SIMPLE_DISCOVERY
#endif

#define	LEASE_DELTA		10	/* # of seconds extra for lease time-out. */

/* Discovery endpoints crypto type: */
#ifndef DISC_SUBMSG_PROT
#define DISC_SUBMSG_PROT	0
#endif
#ifndef DISC_PAYLOAD_PROT
#define DISC_PAYLOAD_PROT	1
#endif
#ifndef DISC_CRYPTO_TYPE
#define DISC_CRYPTO_TYPE	DDS_CRYPT_AES128_HMAC_SHA1
#endif

#endif /* !__disc_cfg_h_ */

