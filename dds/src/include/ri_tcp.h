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

/* ri_tcp.h -- Interface to the RTPS over TCP/IPv4 and over TCP/IPv6 protocol.*/

#ifndef __ri_tcp_h_
#define __ri_tcp_h_

#include "ri_data.h"

#define	TCP_MAX_CLIENTS		48	/* Max. # of TCP clients. */

extern int	tcp_available;		/* Always exists: status of TCP. */

/* Following variables/functions are only available if -DDDS_TCP */

extern RTPS_TCP_PARS	tcp_v4_pars;
#ifdef DDS_IPV6
extern RTPS_TCP_PARS	tcp_v6_pars;
#endif
extern IP_CX		*tcpv4_server;
#ifdef DDS_IPV6
extern IP_CX		*tcpv6_server;
#endif
extern IP_CX		*tcp_client [TCP_MAX_CLIENTS];

int rtps_tcpv4_attach (void);

/* Attach the TCPv4 protocol in order to send RTPS over TCPv4 messages. */

void rtps_tcpv4_detach (void);

/* Detach the previously attached TCPv4 protocol. */


int rtps_tcpv6_attach (void);

/* Attach the TCPv6 protocol for sending RTPS over TCPv6 messages. */

void rtps_tcpv6_detach (void);

/* Detach the previously attached TCPv6 protocol. */


void rtps_tcp_send (unsigned id, Locator_t *lp, LocatorList_t *next, RMBUF *msgs);

/* Send the given messages (msgs) on the TCP locator (lp). The locator list
   (next) gives more destinations to send to after the first if non-NULL.
   If there are no more destinations, and the messages are no longer needed,
   rtps_free_messages () should be called. */

void rtps_tcp_cleanup_cx (IP_CX *cxp);

/* Release all resources related to the given connection (and potential paired
   and client connections. */

int rtps_tcp_peer (unsigned     handle,
		   DomainId_t   domain_id,
		   unsigned     pid,
		   GuidPrefix_t *prefix);

/* Get the neighbour GUID prefix for the given domain id and participant id. */

LocatorList_t rtps_tcp_secure_servers (LocatorList_t uclocs);

/* Return all secure TCP server addresses private and/or public, based on the
   given unicast locators. */

void rtps_tcp_add_mcast_locator (Domain_t *dp);

/* Add the predefined TCP Meta Multicast locator. */

void rtps_tcp_addr_update_start (unsigned family);

/* Start updating addresses for the given address family. */

void rtps_tcp_addr_update_end (unsigned family);

/* Done updating addresses for the given address family. */

#define	ctrl_protocol_valid(p)	!memcmp (p, rpsc_protocol_id, sizeof (ProtocolId_t))
extern ProtocolId_t		rpsc_protocol_id;

/* Protocol header sequence for control messages */


/* Pending connect() serialization data structures and utility functions. */

typedef struct tcp_con_list_st TCP_CON_LIST_ST;
typedef struct tcp_con_req_st TCP_CON_REQ_ST;

typedef int (* TCP_CON_FCT) (TCP_CON_REQ_ST *req);

struct tcp_con_list_st {
	Locator_t	locator;
	TCP_CON_FCT	fct;
	TCP_CON_REQ_ST	*reqs;
	TCP_CON_LIST_ST	*next;
};

struct tcp_con_req_st {
	TCP_CON_LIST_ST	*head;
	IP_CX		*cxp;
	TCP_CON_REQ_ST	*next;
};

int tcp_connect_enqueue (IP_CX *cxp, unsigned port, TCP_CON_FCT fct);

/* Request a new [TLS/]TCP connection setup to a given destination. */

TCP_CON_REQ_ST *tcp_clear_pending_connect (TCP_CON_REQ_ST *p);

/* Remove the current pending connection and return the next. */

TCP_CON_REQ_ST *tcp_pending_connect_remove (IP_CX *cxp);

#endif /* !__ri_tcp_h_ */

