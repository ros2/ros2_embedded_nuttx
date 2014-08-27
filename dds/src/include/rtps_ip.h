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

/* rtps_ip -- TCP/UDP over IP (v4 and v6) transport media for RTPS. */

#ifndef __rtps_ip_
#define __rtps_ip_

#include "dds/dds_trans.h"
#include "rtps_data.h"


int rtps_ipv4_attach (unsigned max_cx, unsigned max_addr);

/* Attach the RTPS over IPv4 transport handler.
   The max_cx parameter specifies the number of IPv4 sockets that can be used.
   The max_addr parameter specifies the maximum number of local IP addresses. */

void rtps_ipv4_detach (void);

/* Detach the RTPS over IPv4 transport handler. */


int rtps_ipv6_attach (unsigned max_cx, unsigned max_addr);

/* Attach the RTPS over IPv6 transport handler.
   The max_cx parameter specifies the number of IPv6 sockets that can be used.
   The max_addr parameter specifies the maximum number of local IPv6 addresses. */

void rtps_ipv6_detach (void);

/* Detach the RTPS over IPv6 transport handler. */

void rtps_ip_dump (const char *cx, int extra);

/* Debug: dump all IP connection contexts. */

void rtps_ip_dump_queued (void);

/* Debug: dump all queued messages in IP contexts. */

void rtps_ip_pool_dump (size_t sizes []);

/* Debug: dump the IP pools. */

void rtps_dtls_dump (void);

/* Debug: dump dtls related info. */


/* Suspend/resume support: */
/* ----------------------- */

void rtps_udp_suspend (void);

/* Suspend RTPS over UDP transport mode. */

void rtps_udp_resume (void);

/* Resume RTPS over UDP transport mode. */

void rtps_tcp_suspend (void);

/* Suspend RTPS over TCP transport mode. */

void rtps_tcp_resume (void);

/* Resume RTPS over TCP transport mode. */


/* Extras for simulation: */
/* ---------------------- */

extern int rtps_ipv4_nqueued;	/* # of messages queued for transmission. */

int rtps_ipv4_send_next (Locator_t *lp, RMBUF **msg);

/* Retrieve the next message that was queued for transmission.
   If successful, a zero-result is returned, the transmit locator will be copied
   to *lp, and *msg will be set to the RTPS message that was enqueud.
   When done with the message, rtps_free_messages() should be used to free it.*/

void rtps_ipv4_receive_add (const Locator_t *lp, RMBUF *msg);

/* This function can be used to simulate that a message(msg) was received on the
   given locator. */

#endif /* !__rtps_ipv4_ */

