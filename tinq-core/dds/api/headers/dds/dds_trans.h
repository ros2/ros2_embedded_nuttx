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

/* dds_trans -- DDS transport mapping parameters. */

#ifndef __dds_trans_
#define	__dds_trans_

#include "dds/dds_error.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* Transport types. */
typedef enum {
	LOCATOR_KIND_INVALID   = -1,
	LOCATOR_KIND_RESERVED  = 0,
	LOCATOR_KIND_UDPv4     = 1,	/* RTPS/UDPv4. */
	LOCATOR_KIND_UDPv6     = 2,	/* RTPS/UDPv6. */
	LOCATOR_KIND_TCPv4     = 4,	/* RTPS/TCPv4. */
	LOCATOR_KIND_TCPv6     = 8	/* RTPS/TCPv6. */
} LocatorKind_t;

/* Default Port creation parameters for specific UDP locators: */
#define	RTPS_UDP_PB	7400	/* Port Base number. */
#define	RTPS_UDP_DG	250	/* DomainId Gain. */
#define	RTPS_UDP_PG	2	/* ParticipantId Gain. */
#define	RTPS_UDP_D0	0
#define	RTPS_UDP_D1	10
#define	RTPS_UDP_D2	1
#define	RTPS_UDP_D3	11

typedef struct rtps_port_pars_st {
	unsigned	pb;		/* Port Base. */
	unsigned	dg;		/* Domain Id gain. */
	unsigned	pg;		/* Participant Id gain. */
	unsigned	d0;
	unsigned	d1;
	unsigned	d2;
	unsigned	d3;
} RTPS_PORT_PARS;

typedef RTPS_PORT_PARS RTPS_UDP_PARS;

#define	RTPS_TCP_SPORT	7400	/* Server port base number. */
#define	RTPS_TCP_NPORTS	3	/* Range of server ports. */

#define	RTPS_MAX_TCP_SERVERS 3	/* # of remote TCP servers. */

typedef union ip_spec_st {
	const char	*name;		/* Domain name. */
	uint32_t	ipa_v4;		/* IPv4 address. */
	unsigned char	ipa_v6 [16];	/* IPv6 address. */
} IP_Spec_t;

typedef struct rtps_tcp_rserver_st {
	int		ipv6;		/* IPv6 server if set. */
	int		name;		/* Domain name specified. */
	int		secure;		/* Secure connection if set. */
	IP_Spec_t	addr;		/* Outer IP address. */
	unsigned	port;		/* Port number of server. */
} RTPS_TCP_RSERV;

typedef struct rtps_tcp_pars_st {
	int		enabled;	/* State of TCP transport. */
	RTPS_PORT_PARS	port_pars;	/* Port parameters. */
	unsigned	sport_s;	/* Secure server port. */
	unsigned	sport_ns;	/* Unsecure server port. */
	unsigned	oport;		/* Public port (if server). */
	int		oname;		/* Outer IP is a domain name. */
	IP_Spec_t	oaddr;		/* Optional outer name (if server). */
	int		private_ok;	/* Use private addresses (if server). */
	unsigned	nrservers;	/* # of remote TCP servers. */
	RTPS_TCP_RSERV	rservers [RTPS_MAX_TCP_SERVERS];/* Remote TCP servers.*/
} RTPS_TCP_PARS;

DDS_EXPORT int DDS_Transport_parameters_set (LocatorKind_t kind, void *pars);

/* Set transport-specific parameters.

   Note: 1. Should be called before creating the first domain participant.
         2. Parameters pointer must stay valid until domain participant is
	    created. */

DDS_EXPORT int DDS_Transport_parameters_get (LocatorKind_t kind, void *pars);

/* Get transport-specific parameters. */

#ifdef __cplusplus
}
#endif

#endif /* !__dds_trans_ */

