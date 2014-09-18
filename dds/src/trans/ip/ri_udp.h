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

/* ri_udp.h -- Interface to the RTPS over UDP/IPv4 and over UDP/IPv6 protocol.*/

#ifndef __ri_udp_h_
#define __ri_udp_h_

#include "ri_data.h"

extern IP_CX	*send_udpv4;
#ifdef DDS_IPV6
extern IP_CX	*send_udpv6;
#endif

int rtps_udpv4_attach (void);

/* Attach the UDPv4 protocol in order to send RTPS over UDPv4 messages. */

void rtps_udpv4_detach (void);

/* Detach the previously attached UDPv4 protocol. */


int rtps_udpv6_attach (void);

/* Attach the UDPv6 protocol for sending RTPS over UDPv6 messages. */

void rtps_udpv6_detach (void);

/* Detach the previously attached UDPv6 protocol. */

void rtps_udp_send (unsigned id, Locator_t *lp, LocatorList_t *next, RMBUF *msgs);

/* Send the given messages (msgs) on the locator (lp).  The locator list (next)
   gives more destinations to send to after the first.  This function is free
   to send to the locators in next if it can, but it should then update *next to
   properly indicate this.  If there are no more destinations left, the msgs
   list should be cleaned up using rtps_free_messages(). */

#endif /* !__ri_udp_h_ */

