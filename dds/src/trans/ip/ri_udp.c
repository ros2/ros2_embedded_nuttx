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

/* ri_udp.c -- Implements the RTPS over UDP/IPv4 of UDP/IPv6 transports. */

#include <stdio.h>
#include <errno.h>
#ifdef _WIN32
#include "win.h"
#include "Ws2IpDef.h"
#include "Ws2tcpip.h"
#include "Iphlpapi.h"
#define ERRNO	WSAGetLastError()
#else
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#define ERRNO errno
#endif
#include "prof.h"
#include "sock.h"
#include "log.h"
#include "error.h"
#include "debug.h"
#include "pool.h"
#include "db.h"
#include "rtps_mux.h"
#ifdef DDS_DYN_IP
#include "dynip.h"
#endif
#include "ri_data.h"
#include "ri_udp.h"
#ifdef DDS_SECURITY
#include "ri_dtls.h"
#endif

#define	DDS_MULTI_DISC	/* Allow multiple Discovery Locators (IPv4+IPv6). */

#ifdef DDS_ACT_LOG
#define	act_printf(s)	log_printf (RTPS_ID, 0, s)
#else
#define	act_printf(s)
#endif

static RTPS_UDP_PARS	udp_v4_pars;
#ifdef DDS_IPV6
static RTPS_UDP_PARS	udp_v6_pars;
#endif

IP_CX		*send_udpv4;
#ifdef DDS_IPV6
IP_CX		*send_udpv6;
#endif

PROF_PID (udp_send)

static int rtps_udpv4_enable (void)
{
	const char	*env_str;
	unsigned	i, mc_ttl;
	int		d [4];
	struct in_addr	mc_dst;
	int		error, set;
#ifdef __APPLE__
	int		yes = 1;
#endif
	act_printf ("rtps_udpv4_enable()\r\n");

	/* Create sender socket. */
	send_udpv4 = rtps_ip_alloc ();
	if (!send_udpv4) {
		error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (error);
	}
	send_udpv4->locator = xmalloc (sizeof (LocatorNode_t));
	if (!send_udpv4->locator) {
		rtps_ip_free (send_udpv4);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (send_udpv4->locator, 0, sizeof (LocatorNode_t));
	send_udpv4->locator->users = 0;
	send_udpv4->locator->locator.kind = LOCATOR_KIND_UDPv4;
	send_udpv4->fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (send_udpv4->fd < 0) {
		perror ("rtps_udpv4_init: socket()");
		err_printf ("rtps_udpv4_init: socket() call failed - errno = %d.\r\n", ERRNO);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	send_udpv4->fd_owner = 1;

#ifdef _WIN32
#ifdef USE_RECVMSG
	if (!WSARecvMsg) {
		error = WSAIoctl (send_udpv4->fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				  &WSARecvMsg_GUID, sizeof WSARecvMsg_GUID,
				  &WSARecvMsg, sizeof WSARecvMsg,
				  &nbytes, NULL, NULL);
		if (error == SOCKET_ERROR || !WSARecvMsg)
			fatal_printf ("WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER:"
					"WSARecvMsg_GUID) returned an error: %d", WSAGetLastError());
	}
#endif /* USE_RECVMSG */
#ifdef USE_SENDMSG
#if (_WIN32_WINNT < 0x0600)
	if (!WSASendMsg) {
		error = WSAIoctl (send_udpv4->fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
				  &WSASendMsg_GUID, sizeof WSASendMsg_GUID,
				  &WSASendMsg, sizeof WSASendMsg,
				  &nbytes, NULL, NULL);
		if (error == SOCKET_ERROR || !WSASendMsg)
			fatal_printf ("WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER:"
					"WSASendMsg_GUID) returned an error: %d", WSAGetLastError());
	}
#endif /* _WIN32::pre-Vista */
#endif /* USE_SENDMSG */
#endif /* _WIN32 */

	/* Check if Multicast TTL is defined in the environment. */
	set = config_defined (DC_IP_MCastTTL);
	if (set) {
		mc_ttl = config_get_number (DC_IP_MCastTTL, 1);
		if (mc_ttl >= 1 && mc_ttl <= 255)
			log_printf (RTPS_ID, 0, "IP: TDDS_IP_MCAST_TTL=%d overrides default value (1).\r\n", mc_ttl);
		else
			set = 0;
	}

	/* Set Multicast TTL parameter. */
	if (set && 
	    (setsockopt (send_udpv4->fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *) &mc_ttl, sizeof (mc_ttl))) < 0) {
		perror ("rtps_udpv4_init: setsockopt (IP_MULTICAST_TTL)");
		warn_printf ("rtps_udpv4_init: setsockopt (IP_MULTICAST_TTL) call failed - errno = %d.\r\n", ERRNO);
	}

	/* Check if Multicast Interface override is defined. */
	set = 0;
	if (config_defined (DC_IP_MCastDest)) {
		env_str = config_get_string (DC_IP_MCastDest, NULL);
		if ((i = sscanf (env_str, "%d.%d.%d.%d", &d [0], &d [1], &d [2], &d [3])) >= 1) {
			ipv4_proto.mcast_if = (d [0] << 24) | (d [1] << 16) | (d [2] << 8) | d [3];
			mc_dst.s_addr = ntohl (ipv4_proto.mcast_if);
			log_printf (RTPS_ID, 0, "IP: TDDS_IP_MCAST_DEST=%d.%d.%d.%d overrides default routing.\r\n", d [0], d [1], d [2], d [3]);
			set = 1;
		}
	}

	/* Set Multicast TTL parameter. */
	if (set &&
	    (setsockopt (send_udpv4->fd, IPPROTO_IP, IP_MULTICAST_IF, (const char *) &mc_dst, sizeof (mc_dst))) < 0) {
		perror ("rtps_udpv4_init: setsockopt (IP_MULTICAST_IF)");
		warn_printf ("rtps_udpv4_init: setsockopt (IP_MULTICAST_IF) call failed - errno = %d.\r\n", ERRNO);
	}

	/* Setup Multicast source address filter. */
	if (config_defined (DC_IP_MCastSrc)) {
		env_str = config_get_string (DC_IP_MCastSrc, NULL);
		ipv4_proto.mc_src = ip_filter_new (env_str, IPF_MASK | IPF_DOMAIN, 0);
	}

#ifdef __APPLE__
	/* MSG_NOSIGNAL does not exist for Apple OS, but an equivalent socket option
	   is available. */
	if (setsockopt(send_udpv4->fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) < 0)
		perror("__func__: setsockopt() failed");
#endif
	return (DDS_RETCODE_OK);
}

#ifdef _WIN32
#define close closesocket
#endif

static void rtps_udpv4_disable (void)
{
	act_printf ("rtps_udpv4_disable()\r\n");

	close (send_udpv4->fd);
	xfree (send_udpv4->locator);
        rtps_ip_free (send_udpv4);
        send_udpv4 = NULL;
}

/* rtps_udpv4_init -- Initialize the UDP/IPv4 RTPS transport. */

static int rtps_udpv4_init (RMRXF    rxf,
			    MEM_DESC msg_hdr,
			    MEM_DESC msg_elem)
{
	int		error;

	act_printf ("rtps_udpv4_init()\r\n");

	if ((ip_attached & LOCATOR_KINDS_IPv4) == 0) {
		error = rtps_ipv4_init (rxf, msg_hdr, msg_elem);
		if (error)
			return (error);
	}
	return (DDS_RETCODE_OK);
}

/* rtps_udpv4_final -- Finalize the UDP/IPv4 transport protocol. */

static void rtps_udpv4_final (void)
{
	act_printf ("rtps_udpv4_final()\r\n");

	if ((ip_attached & LOCATOR_KINDS_IPv4) == 0)
		rtps_ipv4_final ();
}

/* udpv4_add_port -- Add a locator based on the IP address and port number. */

static void udpv4_add_port (LocatorList_t       *llp,
			    const unsigned char *ip,
			    unsigned            port,
			    Scope_t             scope,
			    unsigned            flags)
{
	unsigned char	addr [16];

	if (port >= 0xffffU)
		warn_printf ("UDP: can't create locator for selected DomainId and ParticipantId parameters!");

	if (llp && port < 0xffff) {
		memset (addr, 0, 12);
		memcpy (addr + 12, ip, 4);
		locator_list_add (llp, LOCATOR_KIND_UDPv4, addr, port,
				  0, scope, flags, 0);
	}
}

/* rtps_udpv4_locators_get-- Add protocol specific locators to the given locator
			     lists uc & mc, derived from the given parameters
			     (domain_id & participant_id) for the given type. */

static void rtps_udpv4_locators_get (DomainId_t    domain_id,
				     unsigned      participant_id,
				     RTPS_LOC_TYPE type,
				     LocatorList_t *uc,
				     LocatorList_t *mc,
				     LocatorList_t *dst)
{
	unsigned	i;
	unsigned char	*cp;
	Scope_t		scope;
	unsigned char	mcast_ip [4];
	unsigned	ip [4];
	const char	*env_str;
#ifdef DDS_IP_BCAST
	static unsigned char def_mcast_ip [] = { 255, 255, 255, 255 };
#else
	static unsigned char def_mcast_ip [] = { 239, 255, 0, 1 };
#endif

	memcpy (mcast_ip, def_mcast_ip, 4);
	if (config_defined (DC_IP_MCastAddr)) {
		env_str = config_get_string (DC_IP_MCastAddr, NULL);
		if (sscanf (env_str, "%u.%u.%u.%u", &ip [0], &ip [1],
						    &ip [2], &ip [3]) == 4) {
			mcast_ip [0] = ip [0];
			mcast_ip [1] = ip [1];
			mcast_ip [2] = ip [2];
			mcast_ip [3] = ip [3];
		}
	}
	switch (type) {
		case RTLT_USER:
			for (i = 0, cp = ipv4_proto.own;
			     i < ipv4_proto.num_own;
			     i++, cp += OWN_IPV4_SIZE)
				if (!ipv4_proto.filters ||
				    ip_match (ipv4_proto.filters,
				    	      domain_id,
					      cp)) {
					memcpy (&scope, cp + OWN_IPV4_SCOPE_OFS, 4);
					udpv4_add_port (uc,
						        cp,
						        udp_v4_pars.pb +
						        udp_v4_pars.dg * domain_id + 
						        udp_v4_pars.pg * participant_id +
						        udp_v4_pars.d3,
							scope,
							LOCF_DATA | LOCF_UCAST);
				}
				udpv4_add_port (mc,
					        mcast_ip,
					        udp_v4_pars.pb +
					        udp_v4_pars.dg * domain_id +
					        udp_v4_pars.d2,
						ORG_LOCAL,
						LOCF_DATA | LOCF_MCAST);
			break;
		case RTLT_SPDP_SEDP:
			for (i = 0, cp = ipv4_proto.own;
			     i < ipv4_proto.num_own;
			     i++, cp += OWN_IPV4_SIZE)
				if (!ipv4_proto.filters ||
				    ip_match (ipv4_proto.filters,
				    	      domain_id,
					      cp)) {
					memcpy (&scope, cp + OWN_IPV4_SCOPE_OFS, 4);
					udpv4_add_port (uc,
						        cp,
						        udp_v4_pars.pb +
						        udp_v4_pars.dg * domain_id +
						        udp_v4_pars.pg * participant_id +
						        udp_v4_pars.d1,
							scope,
							LOCF_META | LOCF_UCAST);
				}
			udpv4_add_port (mc,
				        mcast_ip,
				        udp_v4_pars.pb +
				        udp_v4_pars.dg * domain_id +
				        udp_v4_pars.d0,
					ORG_LOCAL,
					LOCF_META | LOCF_MCAST);
			udpv4_add_port (dst,
				        mcast_ip,
				        udp_v4_pars.pb +
				        udp_v4_pars.dg * domain_id +
				        udp_v4_pars.d0,
					ORG_LOCAL,
					LOCF_META | LOCF_MCAST);
			break;
		default:
			break;
	}
}

void rtps_udp_send (unsigned id, Locator_t *first, LocatorList_t *next, RMBUF *msgs)
{
	IP_CX		*ucp, *send_any_cx;
	RMBUF		*mp;
	RME		*mep;
	Locator_t	*lp;
	LocatorList_t	listp = NULL;
	int		nwritten, slist;
	unsigned	lflags, max_src_mc;
	struct sockaddr_in addr;
	struct sockaddr	*sa;
	size_t		ssize;
#ifdef MSG_TRACE
	unsigned char	*daddr;
#endif
#ifdef USE_SENDMSG
#ifdef _WIN32
	WSAMSG		message;
	WSABUF		*iop;
#else /* !_WIN32 */
	struct msghdr	message;
	struct iovec	*iop;
#endif /* !_WIN32 */
#else /* !USE_SENDMSG */
#ifdef USE_WSASENDTO
	WSABUF		*iop;
#else /* !USE_WSASENDTO */
	unsigned	ofs;
	unsigned	n;
#endif /* !USE_WSASENDTO */
#endif /* !USE_SENDMSG */
#if defined (USE_SENDMSG) || defined (USE_WSASENDTO)
	unsigned	i;
#elif defined DDS_IPV6
	char		buf [100];
#endif
#ifdef DDS_IPV6
	struct sockaddr_in6 addr6;
#endif

	/* Initialize message context for WSASendMsg() or sendmsg(). */
#ifdef USE_SENDMSG
#ifdef _WIN32
	message.lpBuffers = ios;
	message.Control.buf = NULL;
	message.Control.len = 0;
	message.dwFlags = 0;
#else /* !_WIN32 */
	message.msg_iov = ios;
	message.msg_control = NULL;
	message.msg_controllen = 0;
	message.msg_flags = 0;
#endif /* !_WIN32 */
#endif /* USE_SENDMSG */

	for (mp = msgs; mp; mp = mp->next) {
		lp = first;
		listp = (next && *next) ? *next : NULL;

		/* 1. Setup transmitter context for the RTPS message. */

#if defined (USE_SENDMSG) || defined (USE_WSASENDTO)

		/* Setup array of data chunks for WSASendMsg() or sendmsg(). */
		ios [0].iov_base = (char *) &mp->header;
		ios [0].iov_len = sizeof (mp->header);
		for (iop = ios + 1, mep = mp->first, i = 0; mep; iop++, mep = mep->next, i++) {
			if (i >= MAX_IOVEC)
				fatal_printf ("rtps_ip_send: too many elements in message (>%u)!", MAX_IOVEC);

			if ((mep->flags & RME_HEADER) != 0) {
				iop->iov_base = (char *) &mep->header;
				iop->iov_len = sizeof (mep->header);
				if (mep->data == mep->d)
					iop->iov_len += mep->length;
				else {
					iop++;
					iop->iov_base = mep->data;
					iop->iov_len = mep->length;
				}
			}
			else {
				iop->iov_base = mep->data;
				iop->iov_len = mep->length;
			}
		}
#ifdef USE_SENDMSG
#ifdef _WIN32
		message.dwBufferCount = iop - ios;
#else /* !_WIN32 */
		message.msg_iovlen = iop - ios;
#endif /* !_WIN32 */
#endif /* USE_SENDMSG */

#else /* !USE_SENDMSG && !USE_WSASENDTO */

		/* Copy message to transmit buffer, chunk by chunk. */
		memcpy (rtps_tx_buf, &mp->header, sizeof (mp->header));
		ofs = sizeof (mp->header);
		for (mep = mp->first; mep; mep = mep->next) {
			if ((mep->flags & RME_HEADER) != 0) {
				n = sizeof (mep->header);
				if (ofs + n > MAX_TX_SIZE)
					break;
				
				memcpy (&rtps_tx_buf [ofs], &mep->header, n);
				ofs += n;
			}
			if ((n = mep->length) != 0) {
				if (ofs + n > MAX_TX_SIZE)
					break;

				if (mep->data != mep->d && mep->db)
					db_get_data (&rtps_tx_buf [ofs], mep->db, mep->data, 0, n - mep->pad);
				else
					memcpy (&rtps_tx_buf [ofs], mep->data, n - mep->pad); 
				ofs += n;
			}
		}
		if (mep) {
			log_printf (RTPS_ID, 0, "rtps_ip_send: sendto(): packet too long (> %lu)\r\n", (unsigned long) MAX_TX_SIZE);
			continue;
		}
#endif /* !USE_SENDMSG && !USE_WSASENDTO */

		/* 2. RTPS message is ready for transmission: send it to all
		      requested destinations. */

		for (;;) { /* For each destination address. */
#ifdef DDS_IPV6
			if (lp->kind == LOCATOR_KIND_UDPv4) {
#endif
				memset (&addr, 0, sizeof (addr));
				addr.sin_family = AF_INET;
				addr.sin_port = htons (lp->port);
				memcpy (&addr.sin_addr.s_addr, lp->address + 12, 4);
				sa = (struct sockaddr *) &addr;
				ssize = sizeof (addr);
				send_any_cx = send_udpv4;
				max_src_mc = ipv4_proto.max_src_mc;
#ifdef DDS_IPV6
			}
			else /*if (locator->kind == LOCATOR_KIND_UDPv6)*/ {
				memset (&addr6, 0, sizeof (addr6));
				addr6.sin6_family = AF_INET6;
				addr6.sin6_port = htons (lp->port);
				memcpy (&addr6.sin6_addr.s6_addr, lp->address, 16);
				sa = (struct sockaddr *) &addr6;
				ssize = sizeof (addr6);
				send_any_cx = send_udpv6;
				max_src_mc = ipv6_proto.max_src_mc;
			}
#endif
			if ((lp->flags & LOCF_UCAST) != 0) {
				if (lp->handle) {
					ucp = rtps_ip_from_handle (lp->handle);
					slist = 0;
					lflags = 0;
				}
				else {
					lflags = lp->flags;
					ucp = send_any_cx;
					slist = 0;
				}
			}
			else {
				lflags = (lp->flags & ~LOCF_MCAST) | LOCF_UCAST;
				ucp = rtps_src_mcast_next (id, lp->kind, lflags, NULL);
				if (!ucp) {
					ucp = send_any_cx;
					slist = 0;
				}
				else
					slist = max_src_mc > 1;
			}
			for (;;) {	/* For each source address. */
				if (!ucp)
					break;

#if defined (USE_SENDMSG) || defined (USE_WSASENDTO)
#ifdef USE_SENDMSG
#ifdef _WIN32
				message.name = (LPSOCKADDR) sa;
				message.namelen = ssize;
				prof_start (udp_send);
				if (WSASendMsg (ucp->fd, &message, 0, &nwritten, NULL, NULL) == SOCKET_ERROR) {
#else /* !_WIN32 */
				message.msg_name = sa;
				message.msg_namelen = ssize;
				prof_start (udp_send);
				nwritten = sendmsg (ucp->fd, &message, 0/*MSG_DONTWAIT*/);
				if (nwritten < 0) {
#endif /* !_WIN32 */
#else /* USE_WSASENDTO */
				prof_start (udp_send);
				if (WSASendTo (ucp->fd, ios, iop - ios, &nwritten, 0,
					       sa, ssize, NULL, NULL)) {
#endif /* USE_WSASENDTO */
					perror ("rtps_udpv4_send: sendmsg()");
					log_printf (RTPS_ID, 0, "rtps_udpv4_send: sendmsg() returned an error: %u.\r\n", ERRNO);
				}
				prof_stop (udp_send, 1);
#else /* !USE_SENDMSG && !USE_WSASENDTO */
				prof_start (udp_send);
				nwritten = sendto (ucp->fd, rtps_tx_buf, ofs, 0, sa, ssize);
				prof_stop (udp_send, 1);
#ifdef _WIN32
				if (nwritten == SOCKET_ERROR) {
#else /* !_WIN32 */
				if (nwritten < 0) {
#endif /* !_WIN32 */
					perror ("rtps_ip_send: sendto()");
#ifdef DDS_IPV6
					if (lp->kind == LOCATOR_KIND_UDPv4)
#endif
						log_printf (RTPS_ID, 0, "rtps_ip_send: sendto(%d.%d.%d.%d:%u->%d.%d.%d.%d:%u) returned an error: %u.\r\n", 
							ucp->locator->locator.address [12],
							ucp->locator->locator.address [13],
							ucp->locator->locator.address [14],
							ucp->locator->locator.address [15],
							ucp->locator->locator.port,
							lp->address [12], lp->address [13],
							lp->address [14], lp->address [15],
							lp->port,
							ERRNO);
#ifdef DDS_IPV6
					else /* if (locator->kind == LOCATOR_KIND_UDPv6) */
						log_printf (RTPS_ID, 0, "rtps_ip_send: sendto(%s:%u) returned an error: %u.\r\n",
							inet_ntop (AF_INET6, lp->address, buf, sizeof (buf)),
							lp->port,
							ERRNO);
#endif
				}
#endif /* !USE_SENDMSG && !USE_WSASENDTO */

#ifdef MSG_TRACE
				if (ucp->trace || (msgs->element.flags & RME_TRACE) != 0) {
#ifdef DDS_IPV6
					if (lp->kind == LOCATOR_KIND_UDPv4)
#endif
						daddr = &lp->address [12];
#ifdef DDS_IPV6
					else
						daddr = lp->address;
#endif
					rtps_ip_trace (ucp->handle, 'T',
						       &ucp->locator->locator,
						       daddr, lp->port, nwritten);
				}
#endif
				ADD_ULLONG (ucp->stats.octets_sent, (unsigned) nwritten);
				ucp->stats.packets_sent++;

				if (!slist)
					break;

				ucp = rtps_src_mcast_next (id, lp->kind, lflags, ucp);
			}

			/* Stop if no list or end-of-list. */
			if (!listp)
				break;

			lp = &listp->data->locator;
			if ((lp->kind & (LOCATOR_KIND_UDPv4 |
					 LOCATOR_KIND_UDPv6)) == 0 || lp->sproto)
				break;

			/* Process next element in destination list! */
			listp = listp->next;
		}
	}
	if (next)
		*next = listp;
}

#ifdef SIMULATION

/* rtps_udpv4_send -- Send RTPS messages to the locator in simulation mode. */

static void rtps_udpv4_send (Locator_t *locator, RMBUF *msgs)
{
	IP_CX	*ucp;

	if (!msgs) {
	    	warn_printf ("rtps_udpv4_send: no messages to send?");
		return;
	}
	ucp = xmalloc (sizeof (IP_CX));
	if (!ucp) {
	    	warn_printf ("rtps_udpv4_send: no memory for container.");
		return;
	}
	memset (ucp, 0, sizeof (IP_CX));
	ucp->locator = locator;
	ucp->head = ucp->tail = msgs;
	rtps_udpv4_nqueued++;
	while (ucp->tail->next) {
		ucp->tail = ucp->tail->next;
		rtps_udpv4_nqueued++;
	}
	if (ip_tx_head)
		ip_tx_tail->next = ucp;
	else
		ip_tx_head = ucp;
	ip_tx_tail = ucp;
}

/* rtps_udpv4_send_next -- Get the next RTPS message that was transmitted. */

static int rtps_udpv4_send_next (Locator_t *lp, RMBUF **msg)
{
	RMBUF	*mp;
	IP_CX	*ucp;

	ucp = ip_tx_head;
	if (!ucp)
		return (DDS_RETCODE_ALREADY_DELETED);

	if (lp)
		*lp = *ucp->locator;
	mp = ucp->head;
	ucp->head = mp->next;
	if (!ucp->head) {
		ip_tx_head = ucp->next;
		if (!ip_tx_head)
			ip_tx_tail = NULL;
		xfree (ucp);
	}
	mp->next = NULL;
	*msg = mp;
	return (DDS_OK);
}

/* rtps_udpv4_receive_add -- Enqueue an RTPS message for simulated reception. */

static void rtps_udpv4_receive_add (const Locator_t *lp, RMBUF *msgs)
{
	unsigned	i;
	IP_CX	*ucp;
	int		wakeup = 0;
	char		buf [1];

	for (i = 0; i < nlocators; i++) {
		ucp = udpv4 [i];
		if (locator_equal (ucp->locator, lp))
			break;
	}

	/* If no such locator exists, just exit. */
	if (i >= nlocators) {
	    	warn_printf ("rtps_udpv4_receive_add: no locator");
		rtps_free_messages (msgs);
		return;
	}

	/* Add message(s) to locator. */
	if (ucp->head)
		ucp->tail->next = msgs;
	else {
		ucp->head = msgs;
		ucp->next = NULL;
		if (!ip_rx_head) {
			ip_rx_head = ucp;
			wakeup = 1;
		}
		else
			ip_rx_tail->next = ucp;
		ip_rx_tail = ucp;
	}
	ucp->tail = msgs;
	while (ucp->tail->next)
		ucp->tail = ucp->tail->next;
	if (!wakeup)
		return;

	/* Send wakeup message. */
	buf [0] = 1;
	write (pipe_fds [1], buf, 1);
}

static void rtps_udpv4_sim_event (int fd, short revents, void *arg)
{
	RMBUF		*mp;
	IP_CX	*ucp;
	char		buf [4];
	ssize_t		n;

	ARG_NOT_USED (revents)
	ARG_NOT_USED (arg)

	n = read (fd, buf, sizeof (buf));
	for (ucp = ip_rx_head; ucp; ucp = ip_rx_head) {
		ip_rx_head = ip_rx_head->next;
		mp = ucp->head;
		ucp->head = ucp->tail = NULL;
		(*rtps_rxf) (ucp->id, mp, ucp->locator);
	}
		
}

#endif /* SIMULATION */

static int rtps_udp_pars_set (LocatorKind_t kind, const void *p)
{
	const RTPS_UDP_PARS *pp = (const RTPS_UDP_PARS *) p;
	RTPS_UDP_PARS	pars;

	if (kind != LOCATOR_KIND_UDPv4 &&
	    kind != LOCATOR_KIND_UDPv6)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!p) {
		pars = rtps_udp_def_pars;
		pars.pb = config_get_number (DC_UDP_PB, pars.pb);
		pars.dg = config_get_number (DC_UDP_DG, pars.dg);
		pars.pg = config_get_number (DC_UDP_PG, pars.pg);
		pars.d0 = config_get_number (DC_UDP_D0, pars.d0);
		pars.d1 = config_get_number (DC_UDP_D1, pars.d1);
		pars.d2 = config_get_number (DC_UDP_D2, pars.d2);
		pars.d3 = config_get_number (DC_UDP_D3, pars.d3);
		pp = &pars;
	}
	else if (!pp->pb || pp->pb > 0xff00 ||
		 !pp->dg || pp->dg > 0x8000 ||
		 !pp->pg || pp->pg > 0x8000 ||
		 pp->d0 > 0x8000 ||
		 pp->d1 > 0x8000 ||
		 pp->d2 > 0x8000 ||
		 pp->d3 > 0x8000)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (kind == LOCATOR_KIND_UDPv4)
		udp_v4_pars = *pp;
#ifdef DDS_IPV6
	else if (kind == LOCATOR_KIND_UDPv6)
		udp_v6_pars = *pp;
#endif
	return (DDS_RETCODE_OK);
}

static int rtps_udp_pars_get (LocatorKind_t kind, void *p, size_t msize)
{
	if (kind != LOCATOR_KIND_UDPv4 &&
	    kind != LOCATOR_KIND_UDPv6)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!p || msize < sizeof (RTPS_UDP_PARS))
		return (DDS_RETCODE_OUT_OF_RESOURCES);
#ifdef DDS_IPV6
	if (kind == LOCATOR_KIND_UDPv4)
#endif
		memcpy (p, &udp_v4_pars, sizeof (RTPS_UDP_PARS));
#ifdef DDS_IPV6
	else /* if (kind == LOCATOR_KIND_UDPv6) */
		memcpy (p, &udp_v6_pars, sizeof (RTPS_UDP_PARS));
#endif
	return (DDS_RETCODE_OK);
}

typedef struct udp_mc_join_st {
	unsigned	flags;
	IP_CX		*uc_cxp;
	struct ip_mreq	mreq;
	const char	*abuf;
} UDP_MC_JOIN;

static void rtps_udpv4_mc_join (IP_CX *mc_cxp, void *data)
{
	UDP_MC_JOIN	*jp = (UDP_MC_JOIN *) data;
	uint32_t	mc_addr;
	int		error;

	if (mc_cxp->locator->locator.kind != LOCATOR_KIND_UDPv4 ||
	    (mc_cxp->locator->locator.flags & jp->flags) != jp->flags)
		return;

	mc_addr = (mc_cxp->locator->locator.address [12] << 24) |
	          (mc_cxp->locator->locator.address [13] << 16) |
	          (mc_cxp->locator->locator.address [14] << 8) |
	           mc_cxp->locator->locator.address [15];
	jp->mreq.imr_multiaddr.s_addr = htonl (mc_addr);
	log_printf (RTPS_ID, 0, "UDP: setsockopt (IP_ADD_MEMBERSHIP) %u.%u.%u.%u on %s\r\n",
		mc_cxp->locator->locator.address [12],
		mc_cxp->locator->locator.address [13],
		mc_cxp->locator->locator.address [14],
		mc_cxp->locator->locator.address [15],
		jp->abuf);
	error = setsockopt (jp->uc_cxp->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			    (char *) &jp->mreq, sizeof (jp->mreq));
	if (error < 0) {
		/*perror ("setsockopt (IP_ADD_MEMBERSHIP) failed");*/
		log_printf (RTPS_ID, 0, "rtps_udp_add_locator(%s): setsockopt (IP_ADD_MEMBERSHIP) for %s failed - errno = %d.",
				jp->abuf, 
				locator_str (&mc_cxp->locator->locator),
				ERRNO);
	}
}

/* rtps_add_unicast_ipv4 -- Set multicast attributes in IP source address. */

static void rtps_add_unicast_ipv4 (DomainId_t domain_id, IP_CX *cxp)
{
	const char	*env_str;
	int		mc_ttl = 1, i;
	int		d [4], set;
	uint32_t	addr;
	struct in_addr	mc_dst;
	UDP_MC_JOIN	jd;

	/* Check if we can use this Multicast source addresses are defined. */
	if (ipv4_proto.mc_src &&
	    !ip_match (ipv4_proto.mc_src,
		       domain_id,
		       &cxp->locator->locator.address [12]))
		return;

	if ((cxp->locator->locator.flags & LOCF_DATA) != 0)	/* Only count DATA! */
		ipv4_proto.max_src_mc++;

	/* Check if Multicast TTL is defined in the environment. */
	set = 0;
	if (config_defined (DC_IP_MCastTTL)) {
		i = config_get_number (DC_IP_MCastTTL, 1);
		if (i >= 1 && i <= 255) {
			mc_ttl = i;
			set = 1;
		}
	}

	/* Set Multicast TTL parameter. */
	if (set && 
	    (setsockopt (cxp->fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *) &mc_ttl, sizeof (mc_ttl))) < 0) {
		perror ("rtps_udp_add_locator: setsockopt (IP_MULTICAST_TTL)");
		warn_printf ("rtps_udp_add_locator: setsockopt (IP_MULTICAST_TTL) call failed - errno = %d.\r\n", ERRNO);
	}

	/* Check if Multicast Interface override is defined. */
	set = 0;
	if (config_defined (DC_IP_MCastDest)) {
		env_str = config_get_string (DC_IP_MCastDest, NULL);
		if ((i = sscanf (env_str, "%d.%d.%d.%d", &d [0], &d [1], &d [2], &d [3])) >= 1) {
			ipv4_proto.mcast_if = (d [0] << 24) | (d [1] << 16) | (d [2] << 8) | d [3];
			mc_dst.s_addr = htonl (ipv4_proto.mcast_if);
			if (!memcmp (&cxp->locator->locator.address [12], &mc_dst.s_addr, 4))
				cxp->src_mcast = 1;
			set = 1;
		}
		else
			cxp->src_mcast = 1;
	}
	else
		cxp->src_mcast = 1;

	/* Set Multicast Interface parameter. */
	if (set &&
	    (setsockopt (cxp->fd, IPPROTO_IP, IP_MULTICAST_IF, (const char *) &mc_dst, sizeof (mc_dst))) < 0) {
		perror ("rtps_udp_add_locator: setsockopt (IP_MULTICAST_IF)");
		warn_printf ("rtps_udp_add_locator: setsockopt (IP_MULTICAST_IF) call failed - errno = %d.\r\n", ERRNO);
	}
	if (cxp->src_mcast) {
		jd.flags = (cxp->locator->locator.flags & (LOCF_DATA | LOCF_META)) | LOCF_MCAST;
		jd.uc_cxp = cxp;
		addr = (cxp->locator->locator.address [12] << 24) |
		       (cxp->locator->locator.address [13] << 16) |
		       (cxp->locator->locator.address [14] << 8) |
	                cxp->locator->locator.address [15];
		jd.mreq.imr_interface.s_addr = htonl (addr);
		jd.abuf = locator_str (&cxp->locator->locator);
		rtps_ip_foreach (rtps_udpv4_mc_join, &jd);
	}
}

typedef struct udp_join_data_st {
	int		fd;
	struct ip_mreq	*mp;
	unsigned	flags;
	const char	*abuf;
} UDP_JOIN_DATA;

static void rtps_udp_join_intf (IP_CX *cxp, void *data)
{
	UDP_JOIN_DATA	*jp = (UDP_JOIN_DATA *) data;
	uint32_t	addr;
	/*int		error;*/

	if (cxp->locator->locator.kind != LOCATOR_KIND_UDPv4 ||
	    (cxp->locator->locator.flags & jp->flags) != jp->flags ||
	    !cxp->src_mcast)
		return;

	addr = (cxp->locator->locator.address [12] << 24) |
	       (cxp->locator->locator.address [13] << 16) |
	       (cxp->locator->locator.address [14] << 8) |
	        cxp->locator->locator.address [15];
	jp->mp->imr_interface.s_addr = htonl (addr);
	log_printf (RTPS_ID, 0, "UDP: setsockopt (IP_ADD_MEMBERSHIP) %s on %u.%u.%u.%u:%u\r\n",
		jp->abuf,
		cxp->locator->locator.address [12],
		cxp->locator->locator.address [13],
		cxp->locator->locator.address [14],
		cxp->locator->locator.address [15],
		cxp->locator->locator.port);
	setsockopt (jp->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			    (char *) jp->mp, sizeof (*jp->mp));
	/* -- don't warn if this fails:: when multiple participants are
	   -- present, this will always occur.
	if (error < 0) {
		perror ("setsockopt (IP_ADD_MEMBERSHIP) failed");
		err_printf ("rtps_udp_add_locator(%s): setsockopt (IP_ADD_MEMBERSHIP) for %u.%u.%u.%u failed - errno = %d.",
				jp->abuf,
				cxp->locator->locator.address [12],
				cxp->locator->locator.address [13],
				cxp->locator->locator.address [14],
				cxp->locator->locator.address [15],
				ERRNO);
	}*/
}

#define	BCAST_ADDR(a) (a [12] == 255 && a [13] == 255 && a [14] == 255 && a [15] == 255)

/* rtps_udpv4_add_locator -- Add a new IP locator. */

static int rtps_udpv4_add_locator (DomainId_t    domain_id,
				   LocatorNode_t *lnp,
				   unsigned      id,
				   int           serve)
{
	Locator_t	*lp = &lnp->locator;
	IP_CX		*ccxp, *cxp = rtps_ip_lookup (id, lp);
	int		flag_on = 1;
	int		error;
	int		family = 0;
	SOCKET		fd;
	struct sockaddr_in addr;
	struct sockaddr	*sa = NULL;
	size_t		ssize = 0;
	struct ip_mreq	mc_req;
	char		buf [24];
	UDP_JOIN_DATA	jd;

	if (!serve)
		return (DDS_RETCODE_OK);

	if (cxp) {
		if (cxp->redundant) {
			cxp->redundant = 0;
			for (ccxp = cxp->clients; ccxp; ccxp = ccxp->next)
				ccxp->redundant = 0;
			return (DDS_RETCODE_OK);
		}
		else {
			dbg_printf ("rtps_udpv4_add_locator (%u.%u.%u.%u:%u): already exists!\r\n",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
			return (DDS_RETCODE_PRECONDITION_NOT_MET);
		}
	}
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		log_printf (RTPS_ID, 0, "rtps_udpv4_add_locator: out of contexts!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	locator_lock (lp);
	cxp->locator = lnp;
	lnp->users++;
	locator_release (lp);
	cxp->id = id;
	snprintf (buf, sizeof (buf), "%u.%u.%u.%u:%u",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
	addr.sin_family = family = AF_INET;
	addr.sin_port = htons (lp->port);
	memset (addr.sin_zero, 0, sizeof (addr.sin_zero)); /* Mandatory on apple platforms. */
	memcpy (&addr.sin_addr.s_addr, lp->address + 12, 4);
	sa = (struct sockaddr *) &addr;
	ssize = sizeof (addr);

	/* Create the listening socket. */
	cxp->fd = fd = socket (family, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		perror ("rtps_udpv4_add_locator: socket ()");
		log_printf (RTPS_ID, 0, "rtps_udpv4_add_locator: socket () failed - errno = %d.\r\n", ERRNO);
		locator_unref (lnp);
		rtps_ip_free (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	cxp->fd_owner = 1;

	log_printf (RTPS_ID, 0, "UDP: adding %s on [%d]\r\n", buf, fd);

	/* If multicast address: allow multiple binds per host. */
	if (CLASSD (lp->address [12])) {
		log_printf (RTPS_ID, 0, "UDP: setsockopt (SO_REUSEADDR) %s\r\n", buf);
		if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
    					(char *) &flag_on, sizeof (flag_on)) < 0) {
			perror ("setsockopt (SO_REUSEADDR) failed");
			log_printf (RTPS_ID, 0, "rtps_udpv4_add_locator: setsockopt (SO_REUSEADDR) failed - errno = %d.\r\n", ERRNO);
		}
	}
#ifdef DDS_SECURITY
	else if ((lp->sproto & SECC_DTLS_UDP) != 0) {
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &flag_on, (socklen_t) sizeof (flag_on));
#if !defined (_WIN32) && defined (SO_REUSEPORT)
		setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (const void *) &flag_on, (socklen_t) sizeof (flag_on));
#endif
	}
#endif
	else if (BCAST_ADDR (lp->address)) {
		setsockopt (fd, SOL_SOCKET, SO_BROADCAST, (const void *) &flag_on, (socklen_t) sizeof (flag_on));
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &flag_on, (socklen_t) sizeof (flag_on));
#if !defined (_WIN32) && defined (SO_REUSEPORT)
		setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (const void *) &flag_on, (socklen_t) sizeof (flag_on));
#endif
	}
	else	/* Unicast source IPv4 address. */
		rtps_add_unicast_ipv4 (domain_id, cxp);

	/* Bind to the address. */
#ifdef _WIN32
	/* Windows doesn't like binding to a Multicast address ... */
	memset (&addr.sin_addr.s_addr, 0, 4);
#endif /* _WIN32 */
	if (bind (fd, sa, ssize)) {
		close (fd);
		rtps_ip_free (cxp);
		error = ERRNO;
		perror ("rtps_udpv4_add_locator: bind()");
		if (error == EADDRINUSE)
			return (DDS_RETCODE_PRECONDITION_NOT_MET);

		err_printf ("rtps_udpv4_add_locator: bind(%s) failed - errno = %d.",
				buf, error);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* If multicast address, we have to join the group via IGMP. */
	if (CLASSD (lp->address [12])) {
		memcpy (&mc_req.imr_multiaddr.s_addr, lp->address + 12, 4);
		if (ipv4_proto.mcast_if) {
			mc_req.imr_interface.s_addr = htonl (ipv4_proto.mcast_if);
			log_printf (RTPS_ID, 0, "UDP: setsockopt (IP_ADD_MEMBERSHIP) %u.%u.%u.%u:%u\r\n",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
			if (setsockopt (fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
    						(char *) &mc_req, sizeof (mc_req)) < 0) {
#ifndef DDS_NO_MCAST
				/* If there is no default route or if the ip address we
				 * specified is not yet assigned to a device, we will
				 * get ENODEV. We will block until addresses are
				 * reassigned or a route is added. */ 
				if (ERRNO == ENODEV && !config_defined (DC_IP_NoMCast)) {
					warn_printf ("No multicast route available - waiting for a route.");
					ipv4_proto.wait_mc_if = 1;
				} 
				else {
#endif
					perror ("setsockopt (IP_ADD_MEMBERSHIP) failed");
					err_printf ("rtps_udpv4_add_locator(%s): setsockopt (IP_ADD_MEMBERSHIP) failed - errno = %d.",
							buf, ERRNO);
					close (fd);
					rtps_ip_free (cxp);
					lnp->users--;
					return (DDS_RETCODE_OUT_OF_RESOURCES);
#ifndef DDS_NO_MCAST
				}
#endif
			}
		}
		else {
			jd.fd = fd;
			jd.mp = &mc_req;
			jd.flags = (lp->flags & (LOCF_DATA | LOCF_META)) | LOCF_UCAST;
			jd.abuf = buf;
			rtps_ip_foreach (rtps_udp_join_intf, &jd);
		}
	}

	/* Complete the locator context. */
	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = cxp->handle;

	/* Register file descriptor for possible data reception. */
#ifdef DDS_SECURITY
	if ((lp->sproto & SECC_DTLS_UDP) != 0)
		rtps_dtls_attach_server (cxp);
	else
#endif
	{
		cxp->cx_type = CXT_UDP;
		sock_fd_add_socket (fd, POLLIN | POLLPRI | POLLHUP | POLLNVAL,
						rtps_ip_rx_fd, cxp, "DDS.UDPv4");
	}
	return (DDS_RETCODE_OK);
}

static RTPS_TRANSPORT rtps_udpv4 = {
	LOCATOR_KIND_UDPv4,
	rtps_udpv4_init,
	rtps_udpv4_final,
	rtps_udp_pars_set,
	rtps_udp_pars_get,
	rtps_udpv4_locators_get,
	rtps_udpv4_add_locator,
	rtps_ip_rem_locator,
	rtps_ip_update,
	rtps_ip_send
};

static IP_CTRL	rtps_udpv4_control = {
	&rtps_udpv4,
	rtps_udpv4_enable,
	rtps_udpv4_disable
};

int rtps_udpv4_attach (void)
{
	int	error;

	act_printf ("rtps_udpv4_attach()\r\n");
	error = rtps_transport_add (&rtps_udpv4);
	if (error)
		return (error);

	if (!udp_v4_pars.pb && !udp_v4_pars.dg && !udp_v4_pars.pg)
		rtps_parameters_set (LOCATOR_KIND_UDPv4, NULL);

	ipv4_proto.control [IPK_UDP] = &rtps_udpv4_control;
	ipv4_proto.nprotos++;
	ip_attached |= LOCATOR_KIND_UDPv4;

	if (ipv4_proto.enabled)
		rtps_udpv4_enable ();

	return (DDS_RETCODE_OK);
}

void rtps_udpv4_detach (void)
{
	act_printf ("rtps_udpv4_detach()\r\n");

	if (!ipv4_proto.control [IPK_UDP])
		return;

	if (send_udpv4)
		rtps_udpv4_disable ();

	ipv4_proto.control [IPK_UDP] = NULL;
	ipv4_proto.nprotos--;
	ip_attached &= ~LOCATOR_KIND_UDPv4;

	rtps_transport_remove (&rtps_udpv4);
}

#ifdef DDS_IPV6

static int rtps_udpv6_enable (void)
{
	const char	*env_str;
	unsigned	i;
	int		error, set;

	act_printf ("rtps_udpv6_enable()\r\n");

	/* Create sender socket. */
	send_udpv6 = rtps_ip_alloc ();
	if (!send_udpv6) {
		error = DDS_RETCODE_OUT_OF_RESOURCES;
		return (error);
	}
	send_udpv6->locator = xmalloc (sizeof (LocatorNode_t));
	if (!send_udpv6->locator) {
		rtps_ip_free (send_udpv6);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	memset (send_udpv6->locator, 0, sizeof (LocatorNode_t));
	send_udpv6->locator->users = 0;
	send_udpv6->locator->locator.kind = LOCATOR_KIND_UDPv6;
	send_udpv6->fd = socket (PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (send_udpv6->fd < 0) {
		perror ("rtps_udpv6_init: socket()");
		err_printf ("rtps_udpv6_init: socket() call failed - errno = %d.\r\n", ERRNO);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	send_udpv6->fd_owner = 1;

	/* Check if IPv6 Multicast maximum hopcount is defined in the environment. */
	set = 0;
	if (config_defined (DC_IPv6_MCastHops)) {
		i = config_get_number (DC_IPv6_MCastHops, 0);
		if (i && i <= 255) {
			log_printf (RTPS_ID, 0, "IPv6: TDDS_IPV6_MCAST_HOPS=%d overrides default value (1).\r\n", i);
			set = 1;
		}
	}

	/* Set IPv6 Multicast TTL parameter. */
	if (set && 
	    (setsockopt (send_udpv6->fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &i, sizeof (i))) < 0) {
		perror ("rtps_udpv6_init: setsockopt (IPV6_MULTICAST_HOPS)");
		warn_printf ("rtps_udpv6_init: setsockopt (IPV6_MULTICAST_HOPS) call failed - errno = %d.\r\n", ERRNO);
	}

	/* Check if IPv6 Multicast Interface override is defined. */
	set = 0;
	if (config_defined (DC_IPv6_MCastIntf)) {
		env_str = config_get_string (DC_IPv6_MCastIntf, NULL);
		if ((i = if_nametoindex (env_str)) >= 1) {
			ipv4_proto.mcast_if = i;
			set = 1;
			log_printf (RTPS_ID, 0, "IPv6: TDDS_IPV6_MCAST_INTF=%s overrides default routing.\r\n", env_str);
		}
	}

	/* Set Multicast TTL parameter. */
	if (set &&
	    (setsockopt (send_udpv6->fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *) &i, sizeof (i))) < 0) {
		perror ("rtps_udpv6_init: setsockopt (IPV6_MULTICAST_IF)");
		warn_printf ("rtps_udpv6_init: setsockopt (IPV6_MULTICAST_IF) call failed - errno = %d.\r\n", ERRNO);
	}
	return (DDS_RETCODE_OK);
}

static void rtps_udpv6_disable (void)
{
	act_printf ("rtps_udpv6_disable()\r\n");

	close (send_udpv6->fd);
	xfree (send_udpv6->locator);
        rtps_ip_free (send_udpv6);
        send_udpv6 = NULL;
}

/* rtps_udpv6_init -- Initialize the UDPv4 RTPS transport. */

static int rtps_udpv6_init (RMRXF    rxf,
			    MEM_DESC msg_hdr,
			    MEM_DESC msg_elem)
{
	int	error;

	act_printf ("rtps_udpv6_init()\r\n");
	if ((ip_attached & LOCATOR_KINDS_IPv6) == 0) {
		error = rtps_ipv6_init (rxf, msg_hdr, msg_elem);
		if (error)
			return (error);
	}
	return (DDS_RETCODE_OK);
}

/* rtps_udpv6_final -- Finalize the UDP/IPv6 transport protocol. */

static void rtps_udpv6_final (void)
{
	act_printf ("rtps_udpv6_final()\r\n");

	if ((ip_attached & LOCATOR_KINDS_IPv6) == 0)
		rtps_ipv6_final ();
}

/* udp6_add_port -- Add a locator based on the IP address and port number. */

static void udpv6_add_port (LocatorList_t       *llp,
			    const unsigned char *ip,
			    unsigned            port,
			    uint32_t            scope_id,
			    Scope_t             scope,
			    unsigned            flags)
{
	unsigned char	addr [16];

	if (port >= 0xffffU)
		warn_printf ("UDPv6: can't create locator due to selected DomainId and ParticipantId parameters!");

	if (llp && port < 0xffff) {
		memcpy (addr, ip, 16);
		locator_list_add (llp, LOCATOR_KIND_UDPv6, addr, port,
				  scope_id, scope, flags, 0);
	}
}

/* rtps_udpv6_locators_get-- Add protocol specific locators to the given locator
			     lists uc & mc, derived from the given parameters
			     (domain_id & participant_id) for the given type. */

static void rtps_udpv6_locators_get (DomainId_t    domain_id,
				     unsigned      participant_id,
				     RTPS_LOC_TYPE type,
				     LocatorList_t *uc,
				     LocatorList_t *mc,
				     LocatorList_t *dst)
{
	unsigned	i;
	unsigned char	*cp;
	uint32_t	scope_id;
	Scope_t		scope;
	int		mcast;
	const char	*env_str;
	unsigned char	mc_addr [16];

	if (config_defined (DC_IPv6_MCastAddr)) {
		env_str = config_get_string (DC_IPv6_MCastAddr, NULL);
		if (!env_str || *env_str == '\0')
			mcast = 0;
		else if (inet_pton (AF_INET6, env_str, mc_addr) == 1)
			mcast = 1;
		else {
			log_printf (RTPS_ID, 0, "IPv6: Invalid TDDS_IPV6_GROUP syntax: IPv6 address expected.\r\n");
			mcast = 0;
		}
	}
	else {
		inet_pton (AF_INET6, "FF03::80", mc_addr);
		mcast = 1;
	}
	switch (type) {
		case RTLT_USER:
			for (i = 0, cp = ipv6_proto.own;
			     i < ipv6_proto.num_own;
			     i++, cp += OWN_IPV6_SIZE) {
				memcpy (&scope_id, cp + OWN_IPV6_SCOPE_ID_OFS, 4);
				memcpy (&scope, cp + OWN_IPV6_SCOPE_OFS, 4);
				udpv6_add_port (uc,
					        cp,
					        udp_v6_pars.pb +
					        udp_v6_pars.dg * domain_id + 
					        udp_v6_pars.pg * participant_id +
					        udp_v6_pars.d3,
						scope_id,
						scope,
						LOCF_DATA | LOCF_UCAST);
			}
			if (ipv6_proto.num_own && mcast)
				udpv6_add_port (mc,
					        mc_addr,
					        udp_v6_pars.pb +
					        udp_v6_pars.dg * domain_id +
					        udp_v6_pars.d2,
						0,
						sys_ipv6_scope (mc_addr),
						LOCF_DATA | LOCF_MCAST);
			break;
		case RTLT_SPDP_SEDP:
			for (i = 0, cp = ipv6_proto.own;
			     i < ipv6_proto.num_own;
			     i++, cp += OWN_IPV6_SIZE) {
				memcpy (&scope_id, cp + OWN_IPV6_SCOPE_ID_OFS, 4);
				memcpy (&scope, cp + OWN_IPV6_SCOPE_OFS, 4);
				udpv6_add_port (uc,
					        cp,
					        udp_v6_pars.pb +
					        udp_v6_pars.dg * domain_id +
					        udp_v6_pars.pg * participant_id +
					        udp_v6_pars.d1,
						scope_id,
						scope,
						LOCF_META | LOCF_UCAST);
			}
			if (
#ifndef DDS_MULTI_DISC
			    rtps_mux_mode == LOCATOR_KIND_UDPv6 &&
			    ipv4_proto.mode == MODE_DISABLED &&
#endif
			    ipv6_proto.num_own &&
			    mcast) {
				udpv6_add_port (mc,
					        mc_addr,
					        udp_v6_pars.pb +
					        udp_v6_pars.dg * domain_id +
					        udp_v6_pars.d0,
						0,
						sys_ipv6_scope (mc_addr),
						LOCF_META | LOCF_MCAST);
				udpv6_add_port (dst,
					        mc_addr,
					        udp_v6_pars.pb +
					        udp_v6_pars.dg * domain_id +
					        udp_v6_pars.d0,
						0,
						sys_ipv6_scope (mc_addr),
						LOCF_META | LOCF_MCAST);
			}
			break;
		default:
			break;
	}
}

#ifdef DDS_DYN_IP

typedef struct udpv6_mc_join_st {
	unsigned	flags;
	IP_CX		*uc_cxp;
	struct ipv6_mreq mreq;
	const char	*abuf;
} UDPV6_MC_JOIN;

static void rtps_udpv6_mc_join (IP_CX *mc_cxp, void *data)
{
	UDPV6_MC_JOIN	*jp = (UDPV6_MC_JOIN *) data;
	int		error;
	char		buf [100];

	if (mc_cxp->locator->locator.kind != LOCATOR_KIND_UDPv6 ||
	    (mc_cxp->locator->locator.flags & jp->flags) != jp->flags)
		return;

	memcpy (&jp->mreq.ipv6mr_multiaddr, mc_cxp->locator->locator.address, 16);

	inet_ntop (AF_INET6, mc_cxp->locator->locator.address, buf, sizeof (buf));
	log_printf (RTPS_ID, 0, "UDPv6: setsockopt (IPV6_JOIN_GROUP) %s on %s\r\n",
		buf, jp->abuf);
	error = setsockopt (jp->uc_cxp->fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			    (char *) &jp->mreq, sizeof (jp->mreq));
	if (error < 0) {
		perror ("setsockopt (IPV6_JOIN_GROUP) failed");
		err_printf ("rtps_udpv6_add_locator(%s): setsockopt (IPV6_JOIN_GROUP) %s failed - errno = %d.",
				jp->abuf, buf, ERRNO);
	}
}

#endif

/* rtps_add_unicast_ipv6 -- Set multicast attributes in IPv6 source address. */

static void rtps_add_unicast_ipv6 (DomainId_t domain_id, IP_CX *cxp)
{
	const char	*env_str;
	int		i, set;
#ifdef DDS_DYN_IP
	UDPV6_MC_JOIN	jd;
#endif

	ARG_NOT_USED (domain_id)

	if ((cxp->locator->locator.flags & LOCF_DATA) != 0)	/* Only count DATA! */
		ipv6_proto.max_src_mc++;

	/* Check if IPv6 Multicast maximum hopcount is defined in the environment. */
	set = 0;
	if (config_defined (DC_IPv6_MCastHops)) {
		i = config_get_number (DC_IPv6_MCastHops, 1);
		if (i >= 1 && i <= 255) {
			log_printf (RTPS_ID, 0, "IPv6: TDDS_IPV6_MCAST_HOPS=%d overrides default value (1).\r\n", i);
			set = 1;
		}
	}

	/* Set IPv6 Multicast TTL parameter. */
	if (set && 
	    (setsockopt (cxp->fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &i, sizeof (i))) < 0) {
		perror ("rtps_init: setsockopt (IPV6_MULTICAST_HOPS)");
		warn_printf ("rtps_init: setsockopt (IPV6_MULTICAST_HOPS) call failed - errno = %d.\r\n", ERRNO);
	}

	/* Check if IPv6 Multicast Interface override is defined. */
	set = 0;
	if (config_defined (DC_IPv6_MCastIntf)) {
		env_str = config_get_string (DC_IPv6_MCastIntf, NULL);
		if (env_str && (i = if_nametoindex (env_str)) >= 1) {
			ipv4_proto.mcast_if = i;
			log_printf (RTPS_ID, 0, "IPv6: TDDS_IPV6_MCAST_INTF=%s overrides default routing.\r\n", env_str);
			set = 1;
		}
		else
			cxp->src_mcast = 1;
	}
	else
		cxp->src_mcast = 1;

	/* Set Multicast TTL parameter. */
	if (set &&
	    (setsockopt (cxp->fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *) &i, sizeof (i))) < 0) {
		perror ("rtps_init: setsockopt (IPV6_MULTICAST_IF)");
		warn_printf ("rtps_init: setsockopt (IPV6_MULTICAST_IF) call failed - errno = %d.\r\n", ERRNO);
	}
#ifdef DDS_DYN_IP
	if (cxp->src_mcast) {
		jd.flags = (cxp->locator->locator.flags & (LOCF_DATA | LOCF_META)) | LOCF_MCAST;
		jd.uc_cxp = cxp;
		jd.abuf = locator_str (&cxp->locator->locator);
		jd.mreq.ipv6mr_interface = di_ipv6_intf (cxp->locator->locator.address);
		if (jd.mreq.ipv6mr_interface)
			rtps_ip_foreach (rtps_udpv6_mc_join, &jd);
	}
#endif
}

#ifdef DDS_DYN_IP

typedef struct udpv6_join_data_st {
	int		fd;
	struct ipv6_mreq *mp;
	unsigned	flags;
	const char	*abuf;
} UDPV6_JOIN_DATA;

static void rtps_udpv6_join_intf (IP_CX *cxp, void *data)
{
	UDPV6_JOIN_DATA	*jp = (UDPV6_JOIN_DATA *) data;
	int		error;
	char		buf [100];

	if (cxp->locator->locator.kind != LOCATOR_KIND_UDPv6 ||
	    (cxp->locator->locator.flags & jp->flags) != jp->flags ||
	    !cxp->src_mcast)
		return;

	inet_ntop (AF_INET6, cxp->locator->locator.address, buf, sizeof (buf));
	jp->mp->ipv6mr_interface = di_ipv6_intf (cxp->locator->locator.address);
	log_printf (RTPS_ID, 0, "UDPv6: setsockopt (IPV6_JOIN_GROUP) %s on %s:%u\r\n",
			jp->abuf, buf, cxp->locator->locator.port);
	error = setsockopt (jp->fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			    (char *) jp->mp, sizeof (*jp->mp));
	if (error < 0) {
		perror ("setsockopt (IPV6_JOIN_GROUP) failed");
		err_printf ("rtps_udpv6_add_locator(%s): setsockopt (IPV6_JOIN_GROUP) %s failed - errno = %d.",
				buf, jp->abuf, ERRNO);
	}
}

#endif

/* rtps_udpv6_add_locator -- Add a new UDP/IPv6 locator. */

static int rtps_udpv6_add_locator (DomainId_t    domain_id,
				   LocatorNode_t *lnp,
				   unsigned      id,
				   int           serve)
{
	Locator_t	*lp = &lnp->locator;
	IP_CX		*cxp = rtps_ip_lookup (id, lp);
	int		flag_on = 1;
	int		error;
	SOCKET		fd;
	struct sockaddr	*sa = NULL;
	size_t		ssize = 0;
	struct sockaddr_in6 addr6;
	struct ipv6_mreq mc_req6;
	char		buf [100];
#ifdef DDS_DYN_IP
	UDPV6_JOIN_DATA	jd;
#endif

	if (!serve)
		return (DDS_RETCODE_OK);

	if (cxp) {
		if (cxp->redundant) {
			cxp->redundant = 0;
			return (DDS_RETCODE_OK);
		}
		else {
			dbg_printf ("rtps_udpv6_add_locator (%s:%u): already exists!\r\n",
				inet_ntop (AF_INET6, lp->address, buf, sizeof (buf)),
				lp->port);
			return (DDS_RETCODE_PRECONDITION_NOT_MET);
		}
	}
	cxp = rtps_ip_alloc ();
	if (!cxp) {
		log_printf (RTPS_ID, 0, "rtps_udpv6_add_locator: out of contexts!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	locator_lock (lp);
	cxp->locator = lnp;
	lnp->users++;
	locator_release (lp);
	cxp->id = id;
	inet_ntop (AF_INET6, lp->address, buf, sizeof (buf));
	snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), ":%u", lp->port);
	memset (&addr6, 0, sizeof (addr6));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port = htons (lp->port);
	memcpy (&addr6.sin6_addr.s6_addr, lp->address, 16);
	addr6.sin6_scope_id = lp->scope_id;
	sa = (struct sockaddr *) &addr6;
	ssize = sizeof (addr6);

	log_printf (RTPS_ID, 0, "UDPv6: adding %s\r\n", buf);

	/* Create the listening socket. */
	cxp->fd = fd = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		perror ("rtps_udpv6_add_locator: socket ()");
		log_printf (RTPS_ID, 0, "rtps_udpv6_add_locator: socket () failed - errno = %d.\r\n", ERRNO);
		locator_unref (lnp);
		rtps_ip_free (cxp);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	cxp->fd_owner = 1;

	/* If multicast address: allow multiple binds per host. */
	if (lp->kind == LOCATOR_KIND_UDPv6 && lp->address [0] == 0xff) {
		log_printf (RTPS_ID, 0, "UDPv6: setsockopt (SO_REUSEADDR) %s\r\n", buf);
		if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
    					(char *) &flag_on, sizeof (flag_on)) < 0) {
			perror ("setsockopt (SO_REUSEADDR) failed");
			log_printf (RTPS_ID, 0, "rtps_udpv6_add_locator: setsockopt (SO_REUSEADDR) failed - errno = %d.\r\n", ERRNO);
		}
	}
#ifdef DDS_SECURITY
	else if ((lp->sproto & SECC_DTLS_UDP) != 0) {
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &flag_on, (socklen_t) sizeof (flag_on));
#if !defined (_WIN32) && defined (SO_REUSEPORT)
		setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (const void *) &flag_on, (socklen_t) sizeof (flag_on));
#endif
	}
#endif
	else
		/* Unicast source IPv6 address. */
		rtps_add_unicast_ipv6 (domain_id, cxp);

	/* Bind to the address. */
	/*log_printf (RTPS_ID, 0, "rtps_udp_add_locator: bind(%s)\r\n", buf);*/
	if (bind (fd, sa, ssize)) {
		close (fd);
		rtps_ip_free (cxp);
		error = ERRNO;
		perror ("rtps_udpv6_add_locator: bind()");
		if (error == EADDRINUSE)
			return (DDS_RETCODE_PRECONDITION_NOT_MET);

		err_printf ("rtps_udpv6_add_locator: bind(%s) failed - errno = %d.",
				buf, error);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* If multicast address, we have to join the group via MLD. */
	if (lp->address [0] == 0xff) {
		log_printf (RTPS_ID, 0, "UDPv6: setsockopt (IPV6_JOIN_GROUP) %s:%u\r\n",
							buf, lp->port);
		mc_req6.ipv6mr_multiaddr = addr6.sin6_addr;
		if (ipv4_proto.mcast_if) {
			mc_req6.ipv6mr_interface = ipv6_proto.mcast_if;
#ifndef DDS_DYN_IP
		}
		else {
			mc_req6.ipv6mr_interface = 0;
#endif
			if (setsockopt (fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, 
					(char *) &mc_req6, sizeof (mc_req6)) < 0) {
				perror ("setsockopt (IPV6_JOIN_GROUP) failed");
				err_printf ("rtps_udpv6_add_locator(%s): setsockopt (IPV6_JOIN_GROUP) failed - errno = %d.",
							buf, ERRNO);
				close (fd);
				rtps_ip_free (cxp);
				lnp->users--;
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			}
#ifdef DDS_DYN_IP
		}
		else {
			jd.fd = fd;
			jd.mp = &mc_req6;
			jd.flags = (lp->flags & (LOCF_DATA | LOCF_META)) | LOCF_UCAST;
			jd.abuf = buf;
			rtps_ip_foreach (rtps_udpv6_join_intf, &jd);
#endif
		}
	}

	/* Complete the locator context. */
	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = cxp->handle;

	/* Register file descriptor for possible data reception. */
#ifdef DDS_SECURITY
	if ((lp->sproto & SECC_DTLS_UDP) != 0)
		rtps_dtls_attach_server (cxp);
	else
#endif
	{
		cxp->cx_type = CXT_UDP;
		sock_fd_add_socket (fd, POLLIN | POLLPRI | POLLHUP | POLLNVAL,
						rtps_ip_rx_fd, cxp, "DDS.UDPv6");
	}
	return (DDS_RETCODE_OK);
}

static RTPS_TRANSPORT rtps_udpv6 = {
	LOCATOR_KIND_UDPv6,
	rtps_udpv6_init,
	rtps_udpv6_final,
	rtps_udp_pars_set,
	rtps_udp_pars_get,
	rtps_udpv6_locators_get,
	rtps_udpv6_add_locator,
	rtps_ip_rem_locator,
	rtps_ip_update,
	rtps_ip_send
};

static IP_CTRL	rtps_udpv6_control = {
	&rtps_udpv6,
	rtps_udpv6_enable,
	rtps_udpv6_disable
};

int rtps_udpv6_attach (void)
{
	int	error;

	act_printf ("rtps_udpv6_attach()\r\n");
	error = rtps_transport_add (&rtps_udpv6);
	if (error)
		return (error);

	if (!udp_v6_pars.pb && !udp_v6_pars.dg && !udp_v6_pars.pg)
		rtps_parameters_set (LOCATOR_KIND_UDPv6, NULL);

	ipv6_proto.control [IPK_UDP] = &rtps_udpv6_control;
	ipv6_proto.nprotos++;
	ip_attached |= LOCATOR_KIND_UDPv6;

	if (ipv6_proto.enabled)
		rtps_udpv6_enable ();

	return (DDS_RETCODE_OK);
}

void rtps_udpv6_detach (void)
{
	act_printf ("rtps_udpv6_detach()\r\n");

	if (!ipv6_proto.control [IPK_UDP])
		return;

	if (send_udpv6)
		rtps_udpv6_disable ();

	ipv6_proto.control [IPK_UDP] = NULL;
	ipv6_proto.nprotos--;
	ip_attached &= ~LOCATOR_KIND_UDPv6;

	rtps_transport_remove (&rtps_udpv6);
}

#endif

