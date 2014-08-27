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

/* rtps_ip -- TCP/UDP over IPv4/IPv6 transport medium for RTPS. */

#include <stdio.h>
#include <errno.h>
#ifdef _WIN32
#include "win.h"
#include <WS2Tcpip.h>
#include <MSWSock.h>
#include <WS2ipdef.h>
#define ERRNO	WSAGetLastError()
#else
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#define ERRNO errno
#endif
#ifdef DDS_SECURITY
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include "security.h"
#endif
#include "prof.h"
#include "vgdefs.h"
#include "sys.h"
#include "log.h"
#include "error.h"
#include "sock.h"
#include "debug.h"
#include "pool.h"
#include "db.h"
#include "ipfilter.h"
#include "rtps_mux.h"
#ifdef DDS_DYN_IP
#include "dynip.h"
#endif
#include "ri_data.h"
#include "ri_udp.h"
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#else
#include "msecplug/msecplug.h"
#endif
#include "ri_dtls.h"
#endif
#ifdef DDS_TCP
#include "ri_tcp.h"
#ifdef DDS_SECURITY
#include "ri_tls.h"
#endif
#endif
#include "rtps_ip.h"

/*#define USE_SENDMSG	** Define this to use sendmsg() i.o. sendto(). */
#if !defined (USE_SENDMSG) && defined (_WIN32)
#define USE_WSASENDTO	/* WIN32: use WSASendTo() i.o. plain sendto(). */
#endif
/*#define USE_RECVMSG	** Define this to use recvmsg() i.o. recvfrom(). */

/*#define USE_MSG_PEEK	** Optimisation to first peek in the first part
			   of the message and only afterwards receive
			   the data in a best-fit buffer.
			   Not used right now since:
			   	1. Code becomes more complex.
				2. Code is not complete yet.
				3. RTPS is not really suitable for this
				   method since the total message size
				   is impossible to guess and is not
				   indicated in the message header. */
/*#define RX_TX_BUF_ALLOC ** Define this to malloc() the Tx/Rx buffers. */

#define	MAX_RX_DBUFS	8	/* # of scattered Rx buffers per Rx. */
#define	MAX_IOVEC	64	/* Must be > MAX_RX_DBUFS! */

enum mem_block_en {
	MB_CXT,		/* Connection table. */
	MB_CX,		/* UDP connection. */

	MB_END
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory blocks used by driver. */
static size_t		mem_size;		/* Total memory allocated. */

static const char *mem_names [] = {
	"IP_CXT",
	"IP_CX",
	"IP_IPT"
};

#ifdef DDS_ACT_LOG
#define	act_printf(s)	log_printf (RTPS_ID, 0, s)
#define	act_print1(s,a)	log_printf (RTPS_ID, 0, s, a)
#else
#define	act_printf(s)
#define	act_print1(s,a)
#endif

static unsigned		max_ip_cx;

static RMRXF		rtps_rxf;
static MEM_DESC 	mhdr_pool, melem_pool;
static IP_CX		**ip;
#ifdef SIMULATION
static IP_CX		*ip_tx_head;
static IP_CX		*ip_tx_tail;
static IP_CX		*ip_rx_head;
static IP_CX		*ip_rx_tail;
static int		pipe_fds [2];
#endif
static unsigned		nlocators;
static int		maxlocator;
#ifdef DDS_SECURITY
static unsigned		dds_ssl_inits;
#endif
#ifdef MSG_TRACE
static unsigned		rtps_ip_dtrace;
#endif

unsigned		ip_attached;
unsigned long		ip_rx_fd_count;

#if defined (USE_SENDMSG) || defined (USE_RECVMSG) || defined (USE_WSASENDTO)
#ifdef _WIN32
static WSABUF		ios [MAX_IOVEC];
#else /* !_WIN32 */
static struct iovec	ios [MAX_IOVEC];
#endif /* !_WIN32 */
#endif /* USE_SENDMSG || USE_RECVMSG || USE_WSASENDTO */
#ifdef USE_SENDMSG
#if defined (_WIN32) && (_WIN32_WINNT < 0x0600)
LPFN_WSASENDMSG		WSASendMsg;
#endif /* _WIN32::pre_Vista */
#else /* !USE_SENDMSG */
#ifdef RX_TX_BUF_ALLOC
unsigned char		*rtps_tx_buf;
#else
static unsigned char	tx_buf_data [MAX_TX_SIZE];
unsigned char		*rtps_tx_buf = tx_buf_data;
#endif
#endif /* !USE_SENDMSG */
#ifdef USE_RECVMSG
static DB		*rx_db_head;
static DB		*rx_db_tail;
static unsigned		rx_db_count;
#ifdef _WIN32
LPFN_WSARECVMSG		WSARecvMsg;
#endif
#else /* !USE_RECVMSG */
#ifdef RX_TX_BUF_ALLOC
unsigned char		*rtps_rx_buf;
#else
static unsigned char	rx_buf_data [MAX_RX_SIZE];
unsigned char		*rtps_rx_buf = rx_buf_data;
#endif
#endif /* !USE_RECVMSG */
#ifdef SIMULATION
int			rtps_ipv4_nqueued;

static void rtps_ipv4_sim_event (int fd, short revents, void *arg);

#else /* !SIMULATION */

/* Global statistics/filters. */
static unsigned long	rtps_ip_nomem_hdr;

#endif /* !SIMULATION */

IP_PROTO		ipv4_proto;
#ifdef DDS_IPV6
IP_PROTO		ipv6_proto;
#endif

/* rtps_scope -- Get IP scope settings. */

static void rtps_scope (Config_t c, Scope_t *min, Scope_t *max, Scope_t dmin)
{
	unsigned	min_s, max_s;

	if (config_defined (c)) {
		config_get_range (c, &min_s, &max_s);
		if (min_s >= NODE_LOCAL && max_s <= GLOBAL_SCOPE) {
			*min = min_s;
			*max = max_s;
		}
	}
	*min = dmin;
	*max = GLOBAL_SCOPE;
}

#ifdef DDS_DYN_IP

static int rtps_ipv4_addr_notify (void)
{
	IP_KIND		i;
	unsigned	n;
	Domain_t	*dp;
	int		ret2, ret = DDS_RETCODE_OK;

	/*dbg_printf ("IPv4 addresses from dyn_ip\r\n:");
	dbg_print_region (ipv4_proto.own, ipv4_proto.num_own * OWN_IPV4_SIZE, 0, 0);*/

	if (!ipv4_proto.num_own && ipv4_proto.enabled) { /* Last IPv4 address gone! */
		for (i = IPK_UDP; i <= IPK_TCP; i++)
			if (ipv4_proto.control [i])
				ipv4_proto.control [i]->disable ();
		ipv4_proto.enabled = 0;
	}
	else if (ipv4_proto.num_own && !ipv4_proto.enabled) { /* First IPv4 address. */
		for (i = IPK_UDP; i <= IPK_TCP; i++)
			if (ipv4_proto.control [i])
				ipv4_proto.control [i]->enable ();
		ipv4_proto.enabled = 1;
	}
	n = 0;
#ifdef DDS_TCP
	rtps_tcp_addr_update_start (AF_INET);
#endif
	while ((dp = domain_next (&n, NULL)) != NULL) {
		ret2 = rtps_participant_update (dp);
		if (ret2)
			ret = ret2;
	}

#ifdef DDS_TCP
	rtps_tcp_addr_update_end (AF_INET);
#endif
	
	return (ret);
}

#ifdef DDS_IPV6

static int rtps_ipv6_addr_notify (void)
{
	IP_KIND		i;
	unsigned	n;
	Domain_t	*dp;
	int		ret2, ret = DDS_RETCODE_OK;

	/*dbg_printf ("!!! IPv6 address changes !!!\r\n");

	dbg_printf ("IPv6 addresses from dyn_ip:\r\n");
	dbg_print_region (ipv6_proto.own, ipv6_proto.num_own * OWN_IPV6_SIZE, 0, 0);*/

	if (!ipv6_proto.num_own && ipv6_proto.enabled) { /* Last IPv4 address gone! */
		for (i = IPK_UDP; i <= IPK_TCP; i++)
			if (ipv6_proto.control [i])
				ipv6_proto.control [i]->disable ();
		ipv6_proto.enabled = 0;
	}
	else if (ipv6_proto.num_own && !ipv6_proto.enabled) { /* First IPv4 address. */
		for (i = IPK_UDP; i <= IPK_TCP; i++)
			if (ipv6_proto.control [i])
				ipv6_proto.control [i]->enable ();
		ipv6_proto.enabled = 1;
	}
	n = 0;
#ifdef DDS_TCP
	rtps_tcp_addr_update_start (AF_INET);
#endif
	while ((dp = domain_next (&n, NULL)) != NULL) {
		ret2 = rtps_participant_update (dp);
		if (ret2)
			ret = ret2;
	}
#ifdef DDS_TCP
	rtps_tcp_addr_update_end (AF_INET6);
#endif

	return (ret);
}

#endif
#endif

#ifdef DDS_IPV6

static void rtps_update_mux_mode (void)
{
	LocatorKind_t	old_mux_mode = rtps_mux_mode;

	if (ipv6_proto.mode > ipv4_proto.mode) {
#ifdef DDS_TCP
		if (ipv6_proto.udp_mode >= MODE_ENABLED)
#endif
			rtps_mux_mode = LOCATOR_KIND_UDPv6;
#ifdef DDS_TCP
		else
			rtps_mux_mode = LOCATOR_KIND_TCPv6;
#endif
	}
	else if (ipv4_proto.mode != MODE_DISABLED) {
#ifdef DDS_TCP
		if (ipv4_proto.udp_mode >= MODE_ENABLED)
#endif
			rtps_mux_mode = LOCATOR_KIND_UDPv4;
#ifdef DDS_TCP
		else
			rtps_mux_mode = LOCATOR_KIND_TCPv4;
#endif
	}
	if (rtps_mux_mode != old_mux_mode) {
		if ((rtps_mux_mode & LOCATOR_KINDS_IPv4) != 0)
			log_printf (RTPS_ID, 0, "IP: preferred mode set to IPv4!\r\n");
		else
			log_printf (RTPS_ID, 0, "IP: preferred mode set to IPv6!\r\n");
	}
}

#endif

static int rtps_udp_suspended;
#ifdef DDS_TCP
static int rtps_tcp_suspended;
#endif

static void rtps_udp_mode_change (Config_t c)
{
	IP_MODE	old_v4_mode;
#ifdef DDS_IPV6
	IP_MODE	old_v6_mode;
#endif

	act_printf ("UDP mode change");
/*	if (!ip_attached) {
		act_printf (" -- not attached!\r\n");
		return;
	}*/
	old_v4_mode = ipv4_proto.udp_mode;
#ifdef DDS_IPV6
	old_v6_mode = ipv6_proto.udp_mode;
#endif
	if (rtps_udp_suspended)
		ipv4_proto.udp_mode = MODE_DISABLED;
	else
		ipv4_proto.udp_mode = config_get_mode (c, MODE_ENABLED);
	act_print1 (": UDP mode=%d", ipv4_proto.udp_mode);
	if (old_v4_mode == ipv4_proto.udp_mode)
#ifdef DDS_IPV6
		if (old_v6_mode == ipv6_proto.udp_mode)
#endif
	{
			act_printf (" -- no change\r\n");
			return;
	}
#ifdef DDS_IPV6
	ipv6_proto.udp_mode = ipv4_proto.udp_mode;
	rtps_update_mux_mode ();
#endif
	act_print1 (", Mux mode=%d\r\n", rtps_mux_mode);
	if (ipv4_proto.mode != MODE_DISABLED) {
		if (old_v4_mode == MODE_DISABLED && ipv4_proto.udp_mode != MODE_DISABLED) {
			act_printf ("UDP attach\r\n");
			rtps_udpv4_attach ();
		}
		else if (old_v4_mode != MODE_DISABLED && ipv4_proto.udp_mode == MODE_DISABLED) {
			act_printf ("UDP detach\r\n");
			rtps_udpv4_detach ();
		}
	}
#ifdef DDS_IPV6
	if (ipv6_proto.mode != MODE_DISABLED) {
		if (old_v6_mode == MODE_DISABLED && ipv6_proto.udp_mode != MODE_DISABLED) {
			act_printf ("UDPv6 attach\r\n");
			rtps_udpv6_attach ();
		}
		else if (old_v6_mode != MODE_DISABLED && ipv6_proto.udp_mode == MODE_DISABLED) {
			act_printf ("UDPv6 detach\r\n");
			rtps_udpv6_detach ();
		}
	}
#endif
#ifdef DDS_DYN_IP
	rtps_ipv4_addr_notify ();
#ifdef DDS_IPV6
	rtps_ipv6_addr_notify ();
#endif
#endif
}

void rtps_udp_suspend (void)
{
	act_printf ("UDP suspend");
	if (rtps_udp_suspended) {
		act_printf (" -- already suspended\r\n");
		return;
	}
	rtps_udp_suspended = 1;
	config_notify (DC_UDP_Mode, NULL);
	act_printf (" -- trigger config change!\r\n");
	rtps_udp_mode_change (DC_UDP_Mode);
}

void rtps_udp_resume (void)
{
	act_printf ("UDP resume");
	if (!rtps_udp_suspended) {
		act_printf (" -- already resumed\r\n");
		return;
	}
	rtps_udp_suspended = 0;
	act_printf (" -- trigger config change!\r\n");
	config_notify (DC_UDP_Mode, rtps_udp_mode_change);
}

#ifdef DDS_TCP

static void rtps_tcp_mode_change (Config_t c)
{
	IP_MODE	old_v4_mode;
#ifdef DDS_IPV6
	IP_MODE	old_v6_mode;
#endif
	act_printf ("TCP mode change");
/*	if (!ip_attached) {
		act_printf (RTPS_ID, 0, " -- not attached!\r\n");
		return;
	}*/
	old_v4_mode = ipv4_proto.tcp_mode;
#ifdef DDS_IPV6
	old_v6_mode = ipv6_proto.tcp_mode;
#endif
	if (rtps_tcp_suspended)
		ipv4_proto.tcp_mode = MODE_DISABLED;
	else
		ipv4_proto.tcp_mode = config_get_mode (c, MODE_ENABLED);
	act_print1 (": TCP mode=%d", ipv4_proto.tcp_mode);
	if (old_v4_mode == ipv4_proto.tcp_mode)
#ifdef DDS_IPV6
		if (old_v6_mode == ipv6_proto.tcp_mode)
#endif
	{
			act_printf (" -- no change\r\n");
			return;
	}
#ifdef DDS_IPV6
	ipv6_proto.tcp_mode = ipv4_proto.tcp_mode;
	rtps_update_mux_mode ();
#endif
	act_print1 (", Mux mode=%d\r\n", rtps_mux_mode);
	if (ipv4_proto.mode != MODE_DISABLED) {
		if (old_v4_mode == MODE_DISABLED && ipv4_proto.tcp_mode != MODE_DISABLED) {
			act_printf ("TCP attach\r\n");
			rtps_tcpv4_attach ();
		}
		else if (old_v4_mode != MODE_DISABLED && ipv4_proto.tcp_mode == MODE_DISABLED) {
			act_printf ("TCP detach\r\n");
			rtps_tcpv4_detach ();
		}
	}
#if 0	/* Not used for now ... */
	if (ipv6_proto.mode != MODE_DISABLED) {
		if (old_v6_mode == MODE_DISABLED && ipv6_proto.tcp_mode != MODE_DISABLED) {
			act_printf ("TCPv6 attach\r\n");
			rtps_tcpv6_attach ();
		}
		else if (old_v6_mode != MODE_DISABLED && ipv6_proto.tcp_mode == MODE_DISABLED) {
			act_printf ("TCPv6 detach\r\n");
			rtps_tcpv6_detach ();
		}
	}
#endif
#ifdef DDS_DYN_IP
	rtps_ipv4_addr_notify ();
#ifdef DDS_IPV6
	rtps_ipv6_addr_notify ();
#endif
#endif
}

void rtps_tcp_suspend (void)
{
	act_printf ("TCP suspend");
	if (rtps_tcp_suspended) {
		act_printf (" -- already suspended\r\n");
		return;
	}
	rtps_tcp_suspended = 1;
	config_notify (DC_TCP_Mode, NULL);
	act_printf (" -- trigger config change!\r\n");
	rtps_tcp_mode_change (DC_TCP_Mode);
}

void rtps_tcp_resume (void)
{
	act_printf ("TCP resume");
	if (!rtps_tcp_suspended) {
		act_printf (" -- already resumed\r\n");
		return;
	}
	rtps_tcp_suspended = 0;
	act_printf (" -- trigger config change!\r\n");
	config_notify (DC_TCP_Mode, rtps_tcp_mode_change);
}

#endif

static int rtps_ip_init (RMRXF    rxf,
			 MEM_DESC msg_hdr,
			 MEM_DESC msg_elem)
{
	POOL_LIMITS	limits;

	act_printf ("rtps_ip_init()\r\n");
	if (mem_blocks [0].md_addr)	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
	else {
		MDS_BLOCK_TYPE (mem_blocks, MB_CXT, sizeof (IP_CX *) * max_ip_cx);
		limits.reserved = max_ip_cx + 1;
		limits.extra = limits.grow = 0;
		MDS_POOL_TYPE (mem_blocks, MB_CX, limits, sizeof (IP_CX));
		mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
		if (!mem_size) {
			warn_printf ("rtps_ip_init: not enough memory for IP contexts.\r\n");
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		log_printf (RTPS_ID, 0, "rtps_ip_init: %lu bytes allocated for IP data.\r\n", (unsigned long) mem_size);
#endif
	}
	rtps_rxf = rxf;
	mhdr_pool = msg_hdr;
	melem_pool = msg_elem;
	nlocators = 0;
	maxlocator = -1;

	/* Setup runtime dynamic data. */
	ip = (IP_CX **) mds_block (&mem_blocks [MB_CXT]);

#ifdef RX_TX_BUF_ALLOC
	rtps_tx_buf = xmalloc (MAX_TX_SIZE);
	if (!rtps_tx_buf) {
		warn_printf ("rtps_ip_init: not enough memory for IP transmit buffer!");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	rtps_rx_buf = xmalloc (MAX_RX_SIZE);
	if (!rtps_rx_buf) {
		warn_printf ("rtps_ip_init: not enough memory for IP receive buffer!");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
#endif

#ifdef DDS_DYN_IP
	if (di_init ())
		warn_printf ("rtps_ip_init: couldn't start the dynamic IP address service!");
#endif
	config_notify (DC_UDP_Mode, rtps_udp_mode_change);
#ifdef DDS_TCP
	config_notify (DC_TCP_Mode, rtps_tcp_mode_change);
#endif
	return (DDS_RETCODE_OK);
}

unsigned rtps_ip_update (LocatorKind_t kind, Domain_t *dp, int done)
{
	IP_CX		**tp, *cxp;
	unsigned	i/*, kinds*/;
	unsigned	present = 0;

/*	if (kind == LOCATOR_KIND_UDPv4 || kind == LOCATOR_KIND_TCPv4)
		kinds = LOCATOR_KINDS_IPv4;
#ifdef DDS_IPV6
	else if (kind == LOCATOR_KIND_UDPv6 || kind == LOCATOR_KIND_TCPv6)
		kinds = LOCATOR_KINDS_IPv6;
#endif
	else
		return (0);*/

	/*log_printf (RTPS_ID, 0, "IP update (domain=%p, kind=%u, done=%d);\r\n", (void *) dp, kind, done);*/
	if (!nlocators)
		return (0);

	for (i = 0, tp = ip; (int) i <= maxlocator; i++, tp++) {
		cxp = *tp;
		if (!cxp ||
		    (dp && cxp->id != dp->index) ||
		    cxp->locator->locator.kind != kind)
			continue;

		if (!done) {
			cxp->redundant = 1;
			present++;
		}
		else if (cxp->redundant)
			rtps_ip_rem_locator (cxp->id, cxp->locator);
		else
			present++;
	}
	return (present);
}

/* rtps_ip_final -- Finalize the IP transport subsystems for RTPS. */

static void rtps_ip_final (void)
{
	IP_CX		*cxp;
	int		i;
#ifdef USE_RECVMSG
	DB		*dbp;

	while ((dbp = rx_db_head) != NULL) {
		rx_db_head = dbp->next;
		rx_db_count--;
		mds_pool_free (mdata_pool, dbp);
	}
#endif

	act_printf ("rtps_ip_final()\r\n");

#ifdef DDS_SECURITY
	rtps_dtls_final ();
#endif

	if (nlocators)
		for (i = maxlocator; i >= 0; i--) {
			cxp = ip [i];
			if (cxp) {
				log_printf (RTPS_ID, 0, "IP: lingering locator: %s\r\n", locator_str (&cxp->locator->locator));
				rtps_ip_rem_locator (cxp->id, cxp->locator);
			}
		}
	mds_free (mem_blocks, MB_END);
#ifdef RX_TX_BUF_ALLOC
	xfree (rtps_rx_buf);
	xfree (rtps_tx_buf);
	rtps_rx_buf = rtps_tx_buf = NULL;
#endif
#ifdef DDS_DYN_IP
	di_final ();
#endif
}

/* rtps_ipv4_init -- Initialize the IPv4-based RTPS transport mechanisms. */

int rtps_ipv4_init (RMRXF    rxf,
		    MEM_DESC msg_hdr,
		    MEM_DESC msg_elem)
{
#ifndef SIMULATION
	unsigned	i;
#ifdef _WIN32
	GUID		WSARecvMsg_GUID = WSAID_WSARECVMSG;
#if defined (USE_RECVMSG) || (defined (USE_SENDMSG) && (_WIN32_WINNT < 0x0600))
	DWORD		nbytes;
#endif
#endif
#endif
	const char	*env_str;
	int		error;

	act_printf ("rtps_ipv4_init()\r\n");
	if (!ip_attached) {
		error = rtps_ip_init (rxf, msg_hdr, msg_elem);
		if (error)
			return (error);
	}
	rtps_scope (DC_IP_Scope, &ipv4_proto.min_scope, &ipv4_proto.max_scope, LINK_LOCAL);
	log_printf (RTPS_ID, 0, "IP: scope = %s", sys_scope_str (ipv4_proto.min_scope));
	if (ipv4_proto.min_scope < ipv4_proto.max_scope)
		log_printf (RTPS_ID, 0, "..%s", sys_scope_str (ipv4_proto.max_scope));
	log_printf (RTPS_ID, 0, "\r\n");
	ipv4_proto.own = xmalloc (OWN_IPV4_SIZE * ipv4_proto.max_own);
	if (!ipv4_proto.own)
		fatal_printf ("rtps_ipv4_init: not enough memory for IPv4 addresses.\r\n");

	/* Get Address and Network filter specs if they are defined. */
	ipv4_proto.filters = NULL;
	if (config_defined (DC_IP_Address)) {
		env_str = config_get_string (DC_IP_Address, NULL);
		ipv4_proto.filters = ip_filter_new (env_str, IPF_DOMAIN, 0);
	}
	if (config_defined (DC_IP_Network)) {
		env_str = config_get_string (DC_IP_Network, NULL);
		if (ipv4_proto.filters)
			ipv4_proto.filters = ip_filter_add (ipv4_proto.filters,
					       env_str, IPF_DOMAIN | IPF_MASK);
		else
			ipv4_proto.filters = ip_filter_new (env_str,
						     IPF_DOMAIN | IPF_MASK, 0);
	}

#ifndef SIMULATION
	ipv4_proto.enabled = 0;
	env_str = config_get_string (DC_IP_Address, NULL);
	if (!env_str || strcmp ("127.0.0.1", env_str) != 0) {
		ipv4_proto.num_own = sys_own_ipv4_addr (ipv4_proto.own, OWN_IPV4_SIZE * ipv4_proto.max_own,
						        ipv4_proto.min_scope, ipv4_proto.max_scope);
		/*dbg_printf ("IPv4 addresses from sys:\r\n");
		dbg_print_region (ipv4_proto.own, ipv4_proto.num_own * OWN_IPV4_SIZE,
				  0, 0);*/
		for (i = 0; i < ipv4_proto.num_own; i++)
			if (ipv4_proto.filters &&
			    !ip_match (ipv4_proto.filters, IPF_DOMAIN_ANY,
			    			&ipv4_proto.own [i * OWN_IPV4_SIZE])) {
				if (i + 1 < ipv4_proto.num_own)
					memmove (&ipv4_proto.own [i * OWN_IPV4_SIZE],
						 &ipv4_proto.own [(i + 1) * OWN_IPV4_SIZE],
						 (ipv4_proto.num_own - i - 1) * OWN_IPV4_SIZE);
				i--;
				ipv4_proto.num_own--;
			}
		ipv4_proto.enabled = (ipv4_proto.num_own != 0);
#ifdef DDS_DYN_IP
		if (di_attach (AF_INET,
			       ipv4_proto.own,
			       &ipv4_proto.num_own,
			       ipv4_proto.max_own,
			       ipv4_proto.min_scope,
			       ipv4_proto.max_scope,
			       rtps_ipv4_addr_notify))
			warn_printf ("rtps_ip_init: couldn't start the dynamic IP address service!");
#endif
	}
	else {
		ipv4_proto.own [0] = 127; 
		ipv4_proto.own [1] = 0;
		ipv4_proto.own [2] = 0;   
		ipv4_proto.own [3] = 1;
		ipv4_proto.num_own = 1;
		ipv4_proto.enabled = 1;
	}
#else /* SIMULATION */
	/* Create event pipe. */
	if (pipe (pipe_fds) == -1) {
		perror ("rtps_ipv4_init: pipe()");
		err_printf ("rtps_ipv4_init: pipe() call failed - errno = %d.\r\n", errno);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	sock_fd_add (pipe_fds [0], POLLIN | POLLPRI | POLLHUP | POLLNVAL, rtps_udpv4_sim_event, 0);
#endif /* SIMULATION */

	return (DDS_RETCODE_OK);
}

void rtps_ipv4_final (void)
{
#ifdef DDS_DYN_IP
	const char	*env_str;
#endif

	act_printf ("rtps_ipv4_final()\r\n");
#ifdef DDS_DYN_IP
	env_str = config_get_string (DC_IP_Address, NULL);
	if (!env_str || strcmp ("127.0.0.1", env_str) != 0)
		di_detach (AF_INET);
#endif
	xfree (ipv4_proto.own);
	ipv4_proto.own = NULL;
	ipv4_proto.num_own = 0;
	if (!ip_attached)
		rtps_ip_final ();
}

#ifdef DDS_IPV6

/* rtps_ipv6_init -- Initialize the IPv6-based RTPS transport mechanisms. */

int rtps_ipv6_init (RMRXF    rxf,
		    MEM_DESC msg_hdr,
		    MEM_DESC msg_elem)
{
	int	error;

	if (!ip_attached) {
		error = rtps_ip_init (rxf, msg_hdr, msg_elem);
		if (error)
			return (error);
	}
	rtps_scope (DC_IPv6_Scope, &ipv6_proto.min_scope, &ipv6_proto.max_scope, SITE_LOCAL);
	log_printf (RTPS_ID, 0, "IPv6: scope = %s", sys_scope_str (ipv6_proto.min_scope));
	if (ipv6_proto.min_scope < ipv6_proto.max_scope)
		log_printf (RTPS_ID, 0, "..%s", sys_scope_str (ipv6_proto.max_scope));
	log_printf (RTPS_ID, 0, "\r\n");

	ipv6_proto.own = xmalloc (OWN_IPV6_SIZE * ipv6_proto.max_own);
	if (!ipv6_proto.own)
		fatal_printf ("rtps_ipv6_init: not enough memory for IPv6 addresses.\r\n");

#ifndef SIMULATION
	ipv6_proto.num_own = sys_own_ipv6_addr (ipv6_proto.own, OWN_IPV6_SIZE * ipv6_proto.max_own,
					        ipv6_proto.min_scope, ipv6_proto.max_scope);
	/*dbg_printf ("IPv6 addresses from sys\r\n:");
	dbg_print_region (ipv6_proto.own, ipv6_proto.num_own * OWN_IPV6_SIZE, 0, 0);*/
	ipv6_proto.enabled = (ipv6_proto.num_own != 0);
#ifdef DDS_DYN_IP
	if (di_attach (AF_INET6,
		       ipv6_proto.own,
		       &ipv6_proto.num_own,
		       ipv6_proto.max_own,
		       ipv6_proto.min_scope,
		       ipv6_proto.max_scope,
		       rtps_ipv6_addr_notify))
		warn_printf ("rtps_ip_init: couldn't start the dynamic IP address service!");
#endif
#endif /* SIMULATION */

	return (DDS_RETCODE_OK);
}

void rtps_ipv6_final (void)
{
	act_printf ("rtps_ipv6_final()\r\n");
#ifdef DDS_DYN_IP
	di_detach (AF_INET6);
#endif
	xfree (ipv6_proto.own);
	ipv6_proto.own = NULL;
	ipv6_proto.num_own = 0;
	if (!ip_attached)
		rtps_ip_final ();
}

#endif /* DDS_IPV6 */

/* rtps_ip_new_handle -- Get a new connection handle. */

unsigned rtps_ip_new_handle (IP_CX *cxp)
{
	unsigned	h;

	if (!nlocators) {
		h = 0;
		maxlocator = 0;
	}
	else if (nlocators - 1 == (unsigned) maxlocator) {
		h = nlocators;
		maxlocator = h;
	}
	else
		for (h = 0; h < (unsigned) maxlocator; h++)
			if (!ip [h])
				break;

	nlocators++;
	ip [h++] = cxp;
	cxp->handle = h;
	return (h);
}

void rtps_ip_free_handle (unsigned h)
{
	int	i;

	if (!h)
		return;

	/* Remove handle from all existing locators. */
	locators_remove_handle (h);

	h--;
	ip [h] = NULL;
	if (!--nlocators)
		maxlocator = -1;
	else if (h == (unsigned) maxlocator) {
		for (i = h - 1; ; i--)
			if (ip [i])
				break;

		maxlocator = i;
	}
}

/* rtps_ip_alloc -- Allocate a new IP connection record. */

IP_CX *rtps_ip_alloc (void)
{
	IP_CX	*cxp;

	cxp = mds_pool_alloc (&mem_blocks [MB_CX]);
	if (cxp)
		memset (cxp, 0, sizeof (IP_CX));

#ifdef MSG_TRACE
	if (cxp)
	    cxp->trace = rtps_ip_dtrace;
#endif
	return (cxp);
}

/* rtps_ip_free -- Free a previously allocated IP connection. */

void rtps_ip_free (IP_CX *cx)
{
	mds_pool_free (&mem_blocks [MB_CX], cx);
}

/* rtps_ip_lookup -- Lookup the context corresponding to a locator. */

IP_CX *rtps_ip_lookup (unsigned id, const Locator_t *lp)
{
	IP_CX		*cxp, **tp;
	unsigned	i;

	if (!nlocators)
		return (NULL);

	for (i = 0, tp = ip; (int) i <= maxlocator; i++, tp++)
		if ((cxp = *tp) != NULL &&
		    (id == ~0U || id == cxp->id) &&
		    locator_equal (lp, &cxp->locator->locator))
			return (cxp);
	return (NULL);
}

/* rtps_ip_lookup_peer -- Lookup the context of a peer locator. */

IP_CX *rtps_ip_lookup_peer (unsigned id, const Locator_t *lp)
{
	IP_CX		*cxp, **tp;
	unsigned	i;

	if (!nlocators)
		return (NULL);

	for (i = 0, tp = ip; (int) i <= maxlocator; i++, tp++)
		if ((cxp = *tp) != NULL &&
		    (id == ~0U || id == cxp->id) &&
		    cxp->has_dst_addr &&
		    cxp->locator->locator.kind == lp->kind &&
		    cxp->dst_port == lp->port &&
		    !memcmp (cxp->dst_addr, lp->address, 16))
			return (cxp);

	return (NULL);
}

/* rtps_ip_lookup_port -- Lookup a context based on a port number. */

IP_CX *rtps_ip_lookup_port (unsigned id, LocatorKind_t kind, unsigned port)
{
	IP_CX		*cxp, **tp;
	unsigned	i;

	if (!nlocators)
		return (NULL);

	for (i = 0, tp = ip; (int) i <= maxlocator; i++, tp++) {
		if ((cxp = *tp) != NULL &&
		    !cxp->associated &&
		    (id == ~0U || id == cxp->id) &&
		    cxp->locator->locator.kind == kind &&
		    cxp->locator->locator.port == port)
			return (cxp);
	}

	return (NULL);
}

/* rtps_ip_from_handle -- Get an IP context from a handle. */

IP_CX *rtps_ip_from_handle (unsigned handle)
{
	IP_CX	*cxp;

	if (!handle || !nlocators)
		return (NULL);

	if ((int) handle > maxlocator + 1)
		return (NULL);

	cxp = ip [handle - 1];
	return ((cxp && cxp->handle == handle) ? cxp : NULL);
}

/* rtps_ip_foreach -- Call the visit function for each connection. */

void rtps_ip_foreach (IP_VISITF fct, void *data)
{
	IP_CX		**tp;
	unsigned	i;

	if (!nlocators)
		return;

	for (i = 0, tp = ip; (int) i <= maxlocator; i++, tp++)
		if (*tp)
			(*fct) (*tp, data);
}

#ifdef MSG_TRACE

#ifndef MAX_TRC_LEN
#define	MAX_TRC_LEN	64
#endif

#ifdef _WIN32
#define iov_len		len
#define iov_base	buf
#endif

#if defined (USE_SENDMSG) || defined (USE_WSASENDTO)
#define USE_TX_IOS
#endif

void rtps_ip_trace (unsigned            handle,
		    char                dir,
		    Locator_t           *lp,
		    const unsigned char *addr,
		    unsigned            port,
		    unsigned            length)
{
#if defined (USE_TX_IOS) || defined (USE_RECVMSG)
	int		vec;
#endif
	unsigned	i, left;
	unsigned char	*cp, c;
	char		ascii [17];
	char		buf [128];

#ifdef DDS_IPV6
	if ((lp->kind & LOCATOR_KINDS_IPv4) != 0) {
#endif
		snprintf (buf, sizeof (buf), "%u.%u.%u.%u:%u %s %u.%u.%u.%u:%u", 
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15], lp->port,
				(dir == 'R') ? "<-" : "->",
				(addr) ? addr [0] : 0,
				(addr) ? addr [1] : 0,
				(addr) ? addr [2] : 0,
				(addr) ? addr [3] : 0, port);
#ifdef DDS_IPV6
	}
	else /*if ((kind & LOCATOR_KINDS_IPv6) != 0)*/ {
		char ipv6_l [INET6_ADDRSTRLEN + 1];
		char ipv6_r [INET6_ADDRSTRLEN + 1];

		inet_ntop (AF_INET6, lp->address, ipv6_l, sizeof (ipv6_l));
		inet_ntop (AF_INET6, addr, ipv6_r, sizeof (ipv6_r));
		snprintf (buf, sizeof (buf), "%s:%u %s %s:%u",
				ipv6_l, lp->port,
				(dir == 'R') ? "<-" : "->",
				ipv6_r, port);
	}
#endif
	log_printf (RTPS_ID, 0, "(%u) %c -%5u: %s\r\n          ", handle, dir, length, buf);
	ascii [16] = '\0';
	cp = NULL;	/* Some compilers warn - cp used before init! */
	for (i = 0, left = 0
#if defined (USE_TX_IOS) || defined (USE_RECVMSG)
	     , vec = -1
#endif
	     ; i < length;
	     i++) {
		if (!left) {
#if defined (USE_TX_IOS) && defined (USE_RECVMSG)
			left = ios [++vec].iov_len;
			cp = ios [vec].iov_base;
#elif defined (USE_TX_IOS) || defined (USE_RECVMSG)
			if (dir == 'T') {

#ifdef USE_TX_IOS
				left = ios [++vec].iov_len;
				cp = ios [vec].iov_base;
#else
				left = length;
				cp = rtps_tx_buf;
#endif
			}
			else {
#ifdef USE_RECVMSG
				left = ios [++vec].iov_len;
				cp = ios [vec].iov_base;
#else
				left = length;
				cp = rtps_rx_buf;
#endif
			}
#else
			left = length;
			cp = (dir == 'T') ? rtps_tx_buf : rtps_rx_buf;
#endif
		}
		c = *cp++;
		left--;
		if (i && (i & 0xf) == 0)
			log_printf (RTPS_ID, 0, "   %s\r\n          ", ascii);
		log_printf (RTPS_ID, 0, "%02x ", c);
		ascii [i & 0xf] = (c >= ' ' && c <= '~') ? c : '.';
		if (i + 1 >= MAX_TRC_LEN)
			break;
	}
	ascii [(i & 0xf) + 1] = '\0';
	i++;
	while ((i & 0xf) != 0) {
		log_printf (RTPS_ID, 0, "   ");
		i++;
	}
	log_printf (RTPS_ID, 0, "   %s", ascii);
	if (i >= MAX_TRC_LEN)
		log_printf (RTPS_ID, 0, " ..");
	log_printf (RTPS_ID, 0, "\r\n");
}
#endif

#ifdef _WIN32
#define close closesocket
#endif

/* rtps_ip_cleanup_cx -- Cleanup a connection. */

static void rtps_ip_cleanup_cx (IP_CX *cxp)
{
	if (cxp->fd_owner) {
		sock_fd_remove_socket (cxp->fd);
		close (cxp->fd);
		cxp->fd_owner = 0;
	}
	rtps_ip_free_handle (cxp->handle);
	if (!cxp->locator->users)
		xfree (cxp->locator);
	else {
		cxp->locator->locator.handle = 0;
		locator_unref (cxp->locator);
	}
	mds_pool_free (&mem_blocks [MB_CX], cxp);
}

#ifndef SIMULATION

#if defined (DDS_TCP) || defined (DDS_SECURITY)

/* rtps_parse_buffer -- Parse a (received) packet in a buffer. */

RMBUF *rtps_parse_buffer (IP_CX *cxp, unsigned char *buf, unsigned len)
{
	RMBUF		*mp;
	RME		*dst_smbp;
	SubmsgHeader	*src_smp;
	size_t		ofs, remain, src_rem, smsg_size;
	DB		*bp;
	int		swap;
	int		mode, eid_ofs;
	EntityId_t	eid;
	unsigned short	slen;

	mp = mds_pool_alloc (mhdr_pool);
	if (!mp) {
		cxp->stats.nomem++;
		return (NULL);
	}
	mp->next = NULL;
	mp->prio = 0;
	mp->users = 1;
	ADD_ULLONG (cxp->stats.octets_rcvd, (unsigned) len);
	cxp->stats.packets_rcvd++;
	if (len < (ssize_t) (sizeof (MsgHeader) + sizeof (SubmsgHeader))) {
		cxp->stats.too_short++;
		mds_pool_free (mhdr_pool, mp);
		return (NULL);
	}

	/* Get the RTPS message header. */
	memcpy ((char *) &mp->header, buf, sizeof (MsgHeader));
	mp->size = ofs = sizeof (MsgHeader);

	remain = len - sizeof (MsgHeader);
	mp->first = NULL;

	/* Add subelements. */
	src_smp = (SubmsgHeader *) &buf [ofs];
	src_rem = remain;
	dst_smbp = &mp->element;

	/* Haven't got the mode yet. */
	mode = -1;

	do {	/* Add a new subelement structure to the message. */
		if (mp->first) { /* Need to allocate a new one. */
			dst_smbp = mds_pool_alloc (melem_pool);
			if (!dst_smbp) {
				cxp->stats.nomem++;
				rtps_free_messages (mp);
				return (NULL);
			}
			mp->last->next = dst_smbp;
			dst_smbp->flags = RME_HEADER;
		}
		else { /* Use the subelement that is already present. */
			mp->first = dst_smbp;
			dst_smbp->flags = RME_CONTAINED | RME_HEADER;
		}
		dst_smbp->pad = 0;
		dst_smbp->db = NULL;
		mp->last = dst_smbp;
		dst_smbp->next = NULL;

		/* Verify/set subelement header and data lengths. */
		/*if (VALGRIND_CHECK_MEM_IS_DEFINED (src_smp, 4) != 0)
			fprintf (stderr, "<<src_smp is *not* defined!>>\r\n");*/

		swap = (src_smp->flags & SMF_ENDIAN) ^ ENDIAN_CPU;
		/*if (VALGRIND_CHECK_VALUE_IS_DEFINED (swap) != 0) {
			printf ("<<swap is *not* defined!>>\r\n");
			printf ("src_smp = %p", src_smp);
			dump_vec (ios, nread);
		}*/
		if (swap) {
			dst_smbp->flags |= RME_SWAP;
			memcswap16 (&slen, &src_smp->length);
		}
		else
			memcpy16 (&slen, &src_smp->length);
		/*if (VALGRIND_CHECK_VALUE_IS_DEFINED (slen)) {
			printf ("<<slen is *not* defined!>>\r\n");
			printf ("src_smp = %p", src_smp);
			dump_vec (ios, nread);
		}*/
		smsg_size = sizeof (SubmsgHeader) + slen;
		if (smsg_size > remain) {
			dst_smbp->length = 0;
			rtps_free_messages (mp);
			return (NULL);
		}
		if (!src_smp->length &&
		    src_smp->id != ST_PAD &&
		    src_smp->id != ST_INFO_TS)
			smsg_size = remain;

		/* Copy submessage header (and preferably data if possible) to
		   the subelement. */
		remain -= smsg_size;
		mp->size += smsg_size;
		if (smsg_size <= MAX_ELEMENT_DATA + sizeof (SubmsgHeader)) {
			dst_smbp->data = dst_smbp->d;
			dst_smbp->length = smsg_size - sizeof (SubmsgHeader);
				memcpy (&dst_smbp->header, src_smp, smsg_size);
				src_smp = (SubmsgHeader *) ((unsigned long) src_smp + smsg_size);
				src_rem -= smsg_size;
			dst_smbp->db = NULL;
		}
		else {
			/* Copy submessage header and skip it in the source buffer. */
			memcpy (&dst_smbp->header, src_smp, sizeof (SubmsgHeader));
			smsg_size -= sizeof (SubmsgHeader);
			src_rem -= sizeof (SubmsgHeader);
				src_smp++;

			if ((bp = db_alloc_data (smsg_size, 1)) == NULL) {
				dst_smbp->length = 0;
				cxp->stats.nomem++;
				rtps_free_messages (mp);
				return (NULL);
			}
			dst_smbp->db = bp;
			dst_smbp->data = bp->data;
			dst_smbp->length = smsg_size;
			memcpy (bp->data, src_smp, smsg_size);
			if (remain) {	/* More subelements? */
				src_smp = (SubmsgHeader *) ((unsigned long) src_smp + smsg_size);
				src_rem -= smsg_size;
			}
		}
		dst_smbp->header.length = slen;

		/* Set mode if not yet done. */
		if (mode < 0 && dst_smbp->header.id <= MAX_SEID_OFS) {
			eid_ofs = rtps_seid_offsets [dst_smbp->header.id];
			if (eid_ofs >= 0) {
				eid.w = *(uint32_t *) (dst_smbp->data + eid_ofs);
				mode = (eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MAJOR) == 
						ENTITY_KIND_USER;
			}
		}
	}
	while (remain);
	if (mode > 0)
		mp->element.flags |= RME_USER;
	mp->next = NULL;
	return (mp);
}

/* rtps_rx_buffer -- Process a received packet in a buffer. */

void rtps_rx_buffer (IP_CX         *cxp,
		     unsigned char *buf,
		     size_t        len,
		     unsigned char *saddr,
		     uint32_t      sport/*,
		     unsigned      sintf*/)
{
	RMBUF		*mp;
#ifdef MSG_TRACE
	unsigned char	*ap;
#endif
	static Locator_t raddr = {
		LOCATOR_KIND_INVALID,
		0,
		{ 0, 0, 0, 0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		UNKNOWN_SCOPE,
		0,
		0,
		0,
		0
	};

#ifdef MSG_TRACE
	if (cxp->trace) {
		if (saddr)
			ap = (cxp->locator->locator.kind & LOCATOR_KINDS_IPv4) ? saddr + 12 : saddr;
		else
			ap = NULL;
		rtps_ip_trace (cxp->handle, 'R',
			       &cxp->locator->locator,
			       ap, sport, len);
	}
#endif
	mp = rtps_parse_buffer (cxp, buf, len);
	if (!mp)
		return;

	raddr.kind = cxp->locator->locator.kind;
	raddr.flags = cxp->locator->locator.flags;
	if (saddr) {
		memcpy (&raddr.address, saddr, 16);
		raddr.port = sport;
		raddr.handle = cxp->handle;
	}
	else {
		memcpy (&raddr.address, &loc_addr_invalid, 16);
		raddr.port = 0;
		raddr.handle = 0;
	}

	/*dbg_printf ("locator: %s\r\n", locator_str (&raddr));*/
	(*rtps_rxf) (cxp->id, mp, &raddr);
}

/* rtps_rx_msg -- Process a received message that is already converted to the
		  RMBUF format. */

void rtps_rx_msg (IP_CX         *cxp,
		  RMBUF         *msg,
		  unsigned char *saddr,
		  uint32_t      sport)
{
	static Locator_t raddr = {
		LOCATOR_KIND_UDPv4,
		0,
		{ 0, 0, 0, 0, 0, 0, 0, 0,
		  0, 0, 0, 0, 0, 0, 0, 0 },
		0,
		UNKNOWN_SCOPE,
		0,
		0,
		0,
		0
	};

#ifdef MSG_TRACE
	if (cxp->trace || (msg->element.flags & RME_TRACE) != 0)
		rtps_ip_trace (cxp->handle, 'R',
			       &cxp->locator->locator,
			       saddr, sport, msg->size);
#endif
	raddr.kind = cxp->locator->locator.kind;
	raddr.flags = cxp->locator->locator.flags;
	if (saddr) {
		memcpy (&raddr.address, saddr, 16);
		raddr.port = sport;
		raddr.handle = cxp->handle;
	}
	else {
		memcpy (&raddr.address, &loc_addr_invalid, 16);
		raddr.port = 0;
		raddr.handle = 0;
	}

	/*dbg_printf ("locator: %s\r\n", locator_str (&raddr));*/
	(*rtps_rxf) (cxp->id, msg, &raddr);
}

#endif

#ifdef DDS_SECURITY

static int ssl_sock_send (BIO *b, const char *in, int inl)
{
	/* See BIO_socket of the details */
	int ret;

	errno = 0;
#ifdef __APPLE__
	ret = send (b->num, in, inl, 0);
#else
	ret = send (b->num, in, inl, MSG_NOSIGNAL);
#endif
	if (ret <= 0) {
		if (BIO_sock_should_retry (ret))
			BIO_set_retry_write (b);
	}
	return (ret);
}

void dds_ssl_init (void)
{
	BIO_METHOD	*bm;

	if (dds_ssl_inits++)
		return;

	DDS_SP_init_library ();

	/* initialize openssl locking mechanism */

	/* Doing a write() on an already closed TCP socket leads to a SIGPIPE
	   which, by default, terminates your application. Because we're a
	   library, we should not alter the signal disposition of SIGPIPE (or
	   any other signal for that matter) to SIG_IGN. The solution
	   implemented in (plain) TCP is to use send(..., MSG_NOSIGNAL).

	   Unfortunately, openssl also uses write() at the lowest layer and has
	   no direct option whatsoever to avoid the resulting signals. What we
	   do here is we override the write function of the socket BIO with our
	   own implementation, which uses send(). */
 	bm = BIO_s_socket();
	bm->bwrite = ssl_sock_send;

	rtps_dtls_init ();
}

void dds_ssl_finish (void)
{
	if (--dds_ssl_inits)
		return;

	rtps_dtls_finish ();
}

#endif /* DDS_SECURITY */

IP_CX *rtps_src_mcast_next (unsigned      id,
			    LocatorKind_t kind,
			    unsigned char flags,
			    IP_CX         *prev)
{
	IP_CX		*cxp, **tp;
	int		h;

	if (prev)
		h = prev->handle;
	else
		h = 0;
	for (tp = &ip [h]; h <= maxlocator; h++, tp++)
		if ((cxp = *tp) != NULL &&
		    cxp->id == id &&
		    cxp->locator->locator.kind == kind &&
		    (cxp->locator->locator.flags & flags) == flags &&
		    cxp->src_mcast)
			return (cxp);

	return (NULL);
}

/* rtps_ip_send -- Send RTPS messages to the given IP locator. */

void rtps_ip_send (unsigned id, void *dest, int dlist, RMBUF *msgs)
{
	Locator_t	*lp;
	LocatorList_t	listp;

	if (!dlist) {
		lp = (Locator_t *) dest;
		dest = NULL;
	}
	else {
		listp = *((LocatorList_t *) dest);
		lp = &listp->data->locator;
		*(LocatorList_t *) dest = listp = listp->next;
	}

#ifdef DDS_SECURITY
	if (lp->sproto) {
#ifdef DDS_TCP
		if ((lp->sproto & SECC_DTLS_UDP) != 0)
#endif
			rtps_dtls_send (id, lp, (LocatorList_t *) dest, msgs);
#ifdef DDS_TCP
		else
			rtps_tcp_send (id, lp, (LocatorList_t *) dest, msgs);
#endif
	}
	else
#endif
#ifdef DDS_TCP
	     if (lp->kind == LOCATOR_KIND_TCPv4 || 
	         lp->kind == LOCATOR_KIND_TCPv6)
		rtps_tcp_send (id, lp, (LocatorList_t *) dest, msgs);
	else
#endif
		rtps_udp_send (id, lp, (LocatorList_t *) dest, msgs);
}

#ifdef VALGRIND_USED
#if 0
static void dump_vec (struct iovec ios [], unsigned nread)
{
	unsigned	i, n, remain = nread;

	printf (", nread = %u, MsgHeader = %u bytes, SubmsgHeader = %u bytes.\r\n", nread, sizeof (MsgHeader), sizeof (SubmsgHeader));
	printf ("\t{");
	for (i = 0; i < MAX_IOVEC; i++) {
		if (i)
			printf (", ");
		printf ("%p:%u", ios [i].iov_base, ios [i].iov_len);
	}
	printf ("}\r\n");
	for (i = 0; i < MAX_IOVEC; i++) {
		if (ios [i].iov_len > remain)
			n = remain;
		else
			n = ios [i].iov_len;
		if (!n)
			break;

		dbg_print_region (ios [i].iov_base, n, 1);

		remain -= n;
		if (!remain)
			break;
	}
}
#endif
#endif /* USE_SENDMSG */

#ifdef _WIN32
#define ssize_t	SSIZE_T
#endif

/* rtps_ip_rx_fd -- Function that should be called whenever the file 
		    descriptor has receive data ready. */

void rtps_ip_rx_fd (SOCKET fd, short revents, void *arg)
{
	ssize_t		nread;
	RMBUF		*mp;
	RME		*dst_smbp;
	SubmsgHeader	*src_smp;
	unsigned	ofs, remain, src_rem, smsg_size;
	IP_CX		*cxp = arg;
	struct sockaddr_in addr;
#ifdef DDS_IPV6
	struct sockaddr_in6 addr6;
#endif
	struct sockaddr	*sa;
	socklen_t	ssize;
	DB		*bp;
	int		mode, eid_ofs;
	EntityId_t	eid;
	int		swap;
	unsigned short	slen;
#ifdef MSG_TRACE
	unsigned char	*ap;
#endif
#ifdef USE_RECVMSG
	unsigned	vec;
#ifdef _WIN32
	WSAMSG		message;
#else
	struct msghdr	message;
#endif
	DB		*bufs [MAX_RX_DBUFS + 1];
#endif
	Locator_t	raddr;

	ARG_NOT_USED (revents)

	/*log_printf (RTPS_ID, 0, "rtps_ip_rx_fd: events = 0x%x\r\n", revents);*/
	ip_rx_fd_count++;
	mp = mds_pool_alloc (mhdr_pool);
	if (!mp) {
		rtps_ip_nomem_hdr++;
		cxp->stats.nomem++;
		return;
	}
	mp->next = NULL;
	mp->prio = 0;
	mp->users = 1;
	raddr.kind = cxp->locator->locator.kind;
#ifdef DDS_IPV6
	if ((raddr.kind & LOCATOR_KINDS_IPv4) != 0) {
#endif
		sa = (struct sockaddr *) &addr;
		ssize = sizeof (addr);
#ifdef DDS_IPV6
	}
	else {
		sa = (struct sockaddr *) &addr6;
		ssize = sizeof (addr6);
	}
#endif
#ifdef USE_RECVMSG

	/* Prepare a scatter-gather vector consisting of:
		a. 1 * RTPS message header.
		b. up to 8 large data buffers (~64K data approximately).
	 */
	ios [0].iov_base = (char *) &mp->header;
	ios [0].iov_len = sizeof (mp->header);

	/* Prepare the argument of rcvmsg(). */
#ifdef _WIN32
	message.name = (LPSOCKADDR) sa;
	message.namelen = ssize;
	message.lpBuffers = ios;
	message.Control.buf = NULL;
	message.Control.len = 0;
	message.dwFlags = 0;
#else
	message.msg_name = sa;
	message.msg_namelen = ssize;
	message.msg_iov = ios;
	message.msg_control = NULL;
	message.msg_controllen = 0;
	message.msg_flags = 0;
#endif

	/* Replenish Rx buffer pool. */
	while (rx_db_count < MAX_RX_DBUFS) {
		bp = db_alloc_rx ();
		if (!bp)
			break;

		VG_DEFINED (bp, DB_HDRSIZE);
		if (rx_db_head)
			rx_db_tail->next = bp;
		else
			rx_db_head = bp;
		rx_db_tail = bp;
		bp->next = NULL;
		rx_db_count++;
	}
#endif
#ifdef USE_MSG_PEEK

	/* Prepare the rest of the receive buffer vector. */
	ios [1].iov_base = &mp->element.header;
	ios [1].iov_len = sizeof (SubmsgHeader) + MAX_ELEMENT_DATA;
	message.msg_iovlen = vec = 2;

	/* Receive the message. */
	nread = recvmsg (fd, &message, MSG_WAITALL | MSG_PEEK);

	/* Check received message data. */
	log_printf (RTPS_ID, 0, "rtps_ip_rx_fd: rcvmsg(MSG_PEEK) -> %u bytes.\r\n", nread);
	if (nread > 0 && (message.flags & MSG_TRUNC) != 0) {

		/* Determine # of bytes that still needs to be read
		   by scanning what we received so far. */
		rx_extra = 0;
		src_smp = (SubmsgHeader *) ios [1].iov_base;
		for (left = ios [1].iov_len; left; ) {
			left -= sizeof (SubmsgHeader);
			if (!src_smp->length)
				break;

			else if (src_smp->length >= left) {
				rx_extra = src_smp->length - left;
				break;
			}
			left -= src_smp->length;
			src_smp = (SubmsgHeader *) ((unsigned char *) src_smp +
					sizeof (SubmsgHeader) + src_smp->length);
		}
		if (!rx_extra) { /* Provide huge buffer since we are
				    oblivious as to the # of bytes needed. */
			for (vec = 1, bp = rx_db_head;
			     vec <= MAX_RX_DBUFS;
			     bp = bp->next, vec++) {
				if (!bp)
					break;

				ios [vec].iov_base = bp->data;
				ios [vec].iov_len = bp->size;
				bufs [vec] = bp;
				bufs [vec]->size = ios [vec].iov_len;
				bufs [vec]->nrefs = 0;
			}
			nread = recvmsg (fd, &message, MSG_WAITALL);
		}
		else {	/* Allocate a best-fit data buffer. */
			rmb = rtps_alloc_data (rx_extra + ios [1].iov_len);
			if (!rmb) {
				/* Not enough buffers! */
				mds_pool_free (mhdr_pool, mp);
				cxp->nomem++;
				return;
			}
			VG_DEFINED (rmb, DB_HDRSIZE);
			vec = 1;
			for (bp = rmb; bp; bp = bp->next, vec++) {
				ios [vec].iov_base = bp->data;
				ios [vec].iov_len = bp->size;
				bufs [vec] = bp;
			}
			nread = recvmsg (fd, &message, MSG_WAITALL);
		}
	}
	else
		nread = recvmsg (fd, &message, MSG_WAITALL);
#else /* !USE_MSG_PEEK */
#ifdef USE_RECVMSG
	/* Prepare the rest of the receive buffer vector. */
	for (vec = 1, bp = rx_db_head;
	     vec <= MAX_RX_DBUFS;
	     bp = bp->next, vec++) {
		if (!bp)
			break;

		ios [vec].iov_base = bp->data;
		ios [vec].iov_len = bp->size;
		if (bp->size != 8192)
			printf ("Invalid buffer size (%u)!!!\r\n", bp->size);
		bufs [vec] = bp;
		bufs [vec]->size = (unsigned) ios [vec].iov_len;
		bufs [vec]->nrefs = 0;
	}
	if (vec < 2) {
		warn_printf ("Lack of Rx buffers!!!\r\n");
		return;
	}
#ifdef _WIN32
	message.dwBufferCount = vec;
	if (WSARecvMsg (fd, &message, &nread, NULL, NULL) == SOCKET_ERROR) {
#else /* !_WIN32 */
	message.msg_iovlen = vec;
	nread = recvmsg (fd, &message, MSG_WAITALL);

	/* Check reception result. */
	if (nread < 0) {
#endif /* !_WIN32 */
#else /* !USE_RECVMSG */
	nread = recvfrom (fd, (char *) rtps_rx_buf, MAX_RX_SIZE, 0, sa, &ssize);
#ifdef _WIN32
	if (nread == SOCKET_ERROR) {
#else /* !_WIN32 */
	if (nread < 0) {
#endif /* !_WIN32 */
#endif /* !USE_RECVMSG */
#endif /* !USE_MSG_PEEK */
		if (sock_fd_valid_socket (fd)) {
			perror ("rtps_ip_rx_fd: rcvmsg()");
			log_printf (RTPS_ID, 0, "rtps_ip_rx_fd: rcvmsg() returned an error - errno = %d.\r\n", ERRNO);
			cxp->stats.read_err++;
		}
		mds_pool_free (mhdr_pool, mp);
		return;
	}
	if (!nread) {
		cxp->stats.empty_read++;
		mds_pool_free (mhdr_pool, mp);
		return;	/* Nothing received? */
	}
#ifdef DDS_IPV6
	if ((raddr.kind & LOCATOR_KINDS_IPv4) != 0) {
#endif
		raddr.port = ntohs (addr.sin_port);
		memset (raddr.address, 0, 12);
		memcpy (&raddr.address [12], &addr.sin_addr.s_addr, 4);
#ifdef MSG_TRACE
		ap = &raddr.address [12];
#endif
#ifdef DDS_IPV6
	}
	else {
		raddr.port = ntohs (addr6.sin6_port);
		memcpy (&raddr.address, &addr6.sin6_addr.s6_addr, 16);
#ifdef MSG_TRACE
		ap = raddr.address;
#endif
	}
#endif
#ifdef MSG_TRACE
	if (cxp->trace)
		rtps_ip_trace (cxp->handle, 'R',
			       &cxp->locator->locator,
			       ap, raddr.port, nread);
#endif
	ADD_ULLONG (cxp->stats.octets_rcvd, (unsigned) nread);
	cxp->stats.packets_rcvd++;
	if (nread < (ssize_t) (sizeof (MsgHeader) + sizeof (SubmsgHeader))) {
		cxp->stats.too_short++;
		mds_pool_free (mhdr_pool, mp);
		return;
	}

#ifndef USE_RECVMSG
	/* Get the RTPS message header. */
	memcpy ((char *) &mp->header, rtps_rx_buf, sizeof (MsgHeader));
	mp->size = ofs = sizeof (MsgHeader);
#endif

/*#define DISABLE_RECEIVE */
#ifdef DISABLE_RECEIVE
	mds_pool_free (mhdr_pool, mp);
#else /* !DISABLE_RECEIVE */
	remain = nread - sizeof (MsgHeader);
	mp->first = NULL;

	/* Add subelements. */
#ifdef USE_RECVMSG
	vec = 1;
	src_smp = (SubmsgHeader *) ios [vec].iov_base;
	src_rem = ios [vec].iov_len;
#else
	src_smp = (SubmsgHeader *) &rtps_rx_buf [ofs];
	src_rem = remain;
#endif
	dst_smbp = &mp->element;

	/* Haven't got the mode yet. */
	mode = -1;

	do {	/* Add a new subelement structure to the message. */
		if (mp->first) { /* Need to allocate a new one. */
			dst_smbp = mds_pool_alloc (melem_pool);
			if (!dst_smbp) {
				cxp->stats.nomem++;
				rtps_free_messages (mp);
				return;
			}
			mp->last->next = dst_smbp;
			dst_smbp->pad = 0;
			dst_smbp->flags = RME_HEADER;
		}
		else { /* Use the subelement that is already present. */
			mp->first = dst_smbp;
			dst_smbp->pad = 0;
			dst_smbp->flags = RME_CONTAINED | RME_HEADER;
		}
		dst_smbp->db = NULL;
		mp->last = dst_smbp;
		dst_smbp->next = NULL;

		/* Verify/set subelement header and data lengths. */
		/*if (VALGRIND_CHECK_MEM_IS_DEFINED (src_smp, 4) != 0)
			fprintf (stderr, "<<src_smp is *not* defined!>>\r\n");*/

		swap = (src_smp->flags & SMF_ENDIAN) ^ ENDIAN_CPU;
		/*if (VALGRIND_CHECK_VALUE_IS_DEFINED (swap) != 0) {
			printf ("<<swap is *not* defined!>>\r\n");
			printf ("src_smp = %p", src_smp);
			dump_vec (ios, nread);
		}*/
		if (swap) {
			dst_smbp->flags |= RME_SWAP;
			memcswap16 (&slen, &src_smp->length);
		}
		else
			memcpy16 (&slen, &src_smp->length);
		/*if (VALGRIND_CHECK_VALUE_IS_DEFINED (slen)) {
			printf ("<<slen is *not* defined!>>\r\n");
			printf ("src_smp = %p", src_smp);
			dump_vec (ios, nread);
		}*/
		smsg_size = sizeof (SubmsgHeader) + slen;
		if (smsg_size > remain) {
			dst_smbp->length = 0;
			rtps_free_messages (mp);
			return;
		}
		if (!src_smp->length &&
		    src_smp->id != ST_PAD &&
		    src_smp->id != ST_INFO_TS)
			smsg_size = remain;

		/* Copy submessage header (and preferably data if possible) to
		   the subelement. */
		remain -= smsg_size;
		mp->size += smsg_size;
#ifdef USE_RECVMSG
		ofs += smsg_size;
#endif
		if (smsg_size <= MAX_ELEMENT_DATA + sizeof (SubmsgHeader)) {
			dst_smbp->data = dst_smbp->d;
			dst_smbp->length = smsg_size - sizeof (SubmsgHeader);
#ifdef USE_RECVMSG
			if (smsg_size < src_rem) {
#endif
				memcpy (&dst_smbp->header, src_smp, smsg_size);
				src_smp = (SubmsgHeader *) ((unsigned long) src_smp + smsg_size);
				src_rem -= smsg_size;
#ifdef USE_RECVMSG
			}
			else {
				/* End of datablock reached, go to next block.
				   Two copies are needed now. */
				memcpy (&dst_smbp->header, src_smp, src_rem);
				smsg_size -= src_rem;
				ofs = src_rem;
				vec++;
				src_smp = (SubmsgHeader *) ios [vec].iov_base;
				src_rem = ios [vec].iov_len;
				if (smsg_size) {
					memcpy ((unsigned char *) &dst_smbp->header + ofs,
						src_smp, smsg_size);
					src_smp = (SubmsgHeader *) ((unsigned long) src_smp + smsg_size);
					src_rem -= smsg_size;
				}
			}
#endif
			dst_smbp->db = NULL;
		}
		else {
			/* Copy submessage header and skip it in the source buffer. */
			memcpy (&dst_smbp->header, src_smp, sizeof (SubmsgHeader));
			smsg_size -= sizeof (SubmsgHeader);
			src_rem -= sizeof (SubmsgHeader);
#ifdef USE_RECVMSG
			if (!src_rem) {
				vec++;
				src_smp = (SubmsgHeader *) ios [vec].iov_base;
				src_rem = ios [vec].iov_len;
			}
			else
#endif
				src_smp++;

#ifdef USE_RECVMSG
			/* Point the subelement data region to the DB block. */ 
			dst_smbp->data = (unsigned char *) src_smp;
			dst_smbp->length = smsg_size;
			dst_smbp->db = bufs [vec];

			/* Detach data buffer if not yet done! */
			if (!dst_smbp->db->nrefs++) {
				rx_db_head = rx_db_head->next;
				rx_db_count--;
			}

			/* Skip past the actual data area. */
			while (smsg_size >= src_rem) {
				smsg_size -= src_rem;
				vec++;
				src_smp = (SubmsgHeader *) ios [vec].iov_base;
				src_rem = ios [vec].iov_len;
				bp = bufs [vec];
				if (smsg_size && bp && !bp->nrefs++) {
					rx_db_head = rx_db_head->next;
					rx_db_count--;
				}
			}
#else
			if ((bp = db_alloc_data (smsg_size, 1)) == NULL) {
				dst_smbp->length = 0;
				cxp->stats.nomem++;
				rtps_free_messages (mp);
				return;
			}
			dst_smbp->db = bp;
			dst_smbp->data = bp->data;
			dst_smbp->length = smsg_size;
			memcpy (bp->data, src_smp, smsg_size);
#endif /* USE_RECVMSG */
			if (remain) {	/* More subelements? */
				src_smp = (SubmsgHeader *) ((unsigned long) src_smp + smsg_size);
				src_rem -= smsg_size;
			}
		}
		dst_smbp->header.length = slen;

		/* Set mode if not yet done. */
		if (mode < 0 && dst_smbp->header.id <= MAX_SEID_OFS) {
			eid_ofs = rtps_seid_offsets [dst_smbp->header.id];
			if (eid_ofs >= 0) {
				eid.w = *(uint32_t *) (dst_smbp->data + eid_ofs);
				mode = (eid.id [ENTITY_KIND_INDEX] & ENTITY_KIND_MAJOR) == 
						ENTITY_KIND_USER;
			}
		}
	}
	while (remain);
	mp->next = NULL;

	raddr.intf = 0;
	raddr.scope_id = 0;
	raddr.scope = UNKNOWN_SCOPE;
	raddr.flags = cxp->locator->locator.flags;
	if ((raddr.flags & LOCF_MCAST) != 0) {
		raddr.flags &= ~LOCF_MCAST;
		raddr.flags |= LOCF_UCAST;
	}
	raddr.handle = cxp->handle;
	raddr.sproto = 0;
	if (mode > 0)
		mp->element.flags |= RME_USER;
	(*rtps_rxf) (cxp->id, mp, &raddr);
#endif /* !DISABLE_RECEIVE */
}
#endif 

/* rtps_ip_rem_locator -- Remove a UDP locator. */

void rtps_ip_rem_locator (unsigned id, LocatorNode_t *lnp)
{
	Locator_t	*lp = &lnp->locator;
	IP_CX		*cxp;
	char		buf [100];

	if ((lp->kind & LOCATOR_KINDS_IPv4) != 0) {
		snprintf (buf, sizeof (buf), "%u.%u.%u.%u:%u",
				lp->address [12], lp->address [13],
				lp->address [14], lp->address [15],
				lp->port);
	}
#ifdef DDS_IPV6
	else {
		inet_ntop (AF_INET6, lp->address, buf, sizeof (buf));
		snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), ":%u", lp->port);
	}
#endif
	cxp = rtps_ip_lookup (id, lp);
	if (cxp) {
		log_printf (RTPS_ID, 0, "IP: removing %s (%d)\r\n", buf, cxp->handle);
#ifdef DDS_SECURITY
		if (cxp->locator->locator.sproto) {
			if ((cxp->locator->locator.sproto & SECC_DTLS_UDP) != 0)
				rtps_dtls_cleanup_cx (cxp);
		}
#endif
#ifdef DDS_TCP
		if ((lp->kind & LOCATOR_KINDS_TCP) != 0)
			rtps_tcp_cleanup_cx (cxp);
		else
#endif
			rtps_ip_cleanup_cx (cxp);
	}
}

#ifdef DDS_DEBUG

void rtps_ip_dump_cx (IP_CX *cxp, int extra)
{
	char		*bp, buf [100];
	size_t		n, rem;
#ifdef DDS_IPV6
	unsigned	family;
#endif
	unsigned	sec, ms;
#ifdef DDS_TCP
	char		c;
	const char	*s;
	static const char *cx_state_str [] = {
		"Closed", "Listening", "Authenticating(C)", "ConnectReq",
		"Connecting", "Retrying", "Authenticating(S)", "Established"
	};
	static const char *tcp_control_state_str [] = {
		"Idle", "WCxOk", "WIBindOk", "Control"
	};
	static const char *tcp_data_state_str [] = {
		"Idle", "WControl", "WPortOk", "WCxOk", "WCBindOk", "Data"
	};
#endif

	bp = buf;
	rem = sizeof (buf);
	if (extra) {
		n = snprintf (bp, rem, "(%u) ", cxp->handle);
		bp += n;
		rem -= n;
	}
#ifdef DDS_TCP
	if (cxp->cx_type == CXT_TCP || cxp->cx_type == CXT_TCP_TLS) {
#ifdef DDS_SECURITY
		if (cxp->cx_type == CXT_TCP_TLS)
			n = snprintf (bp, rem, "TLS");
		else
#endif /* DDS_SECURITY */
			n = snprintf (bp, rem, "TCP");
		bp += n;
		rem -= n;
		switch (cxp->cx_mode) {
			case ICM_ROOT:
				c = 'S';
				break;
			case ICM_CONTROL:
				if (cxp->cx_side == ICS_SERVER)
					c = 'H';
				else
					c = 'C';
				break;
			case ICM_DATA:
				c = 'D';
				break;
			default:
				c = '?';
				break;
		}
		n = snprintf (bp, rem, ".%c", c);
	}
	else
#endif /* DDS_TCP */
	     {
#ifdef DDS_SECURITY
		if (cxp->cx_type == CXT_UDP_DTLS)
			n = snprintf (bp, rem, "DTLS.%c",
				(cxp->cx_side == ICS_SERVER) ? 
				cxp->sproto ? 'H' : 'S' : 'C');
		else
#endif
			n = snprintf (bp, rem, "UDP");
	}
	bp += n;
	rem -= n;
	do {
		*bp++ = ' ';
		rem--;
	}
	while (rem > sizeof (buf) - 8);
	*bp = '\0';
	if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv4) != 0) {
#ifdef DDS_IPV6
		family = AF_INET;
#endif
		n = snprintf (bp, rem, "%u.%u.%u.%u",
						cxp->locator->locator.address [12], 
						cxp->locator->locator.address [13],
						cxp->locator->locator.address [14],
						cxp->locator->locator.address [15]);
		rem -= n;
	}
#ifdef DDS_IPV6
	else if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv6) != 0) {
		family = AF_INET6;
		inet_ntop (AF_INET6, cxp->locator->locator.address, bp, rem);
		n = strlen (bp);
	}
#endif
	else {
		n = snprintf (bp, rem, "<Kind = %u?>", cxp->locator->locator.kind);
#ifdef DDS_IPV6
		family = 0;
#endif
	}
	bp += n;
	rem -= n;
	if (rem) {
		n = snprintf (bp, rem, ":%u", cxp->locator->locator.port);
		bp += n;
		rem -= n;
	}
	if (rem && extra && cxp->locator->locator.handle != cxp->handle)
		snprintf (bp, rem, "(!%u)", cxp->locator->locator.handle);
	dbg_printf ("  %-20s %cfd:%d%c ", buf, 
			(cxp->fd_owner) ? ' ' : '(', 
			cxp->fd, 
			(cxp->fd_owner) ? ' ' : ')');
#ifdef DDS_IPV6
	if ((cxp->locator->locator.kind & LOCATOR_KINDS_IPv4) == 0)
		dbg_printf ("\r\n\t");
#endif
	dbg_printf ("    id:%u  ", cxp->id);
	if (cxp->dst_forward)
		dbg_printf ("rfwd:%u  ", cxp->dst_forward);
	if ((cxp->locator->locator.flags & LOCF_DATA) != 0)
		dbg_printf (" USER");
	if ((cxp->locator->locator.flags & LOCF_META) != 0)
		dbg_printf (" META");
	if ((cxp->locator->locator.flags & LOCF_UCAST) != 0)
		dbg_printf (" UCAST");
	if ((cxp->locator->locator.flags & LOCF_MCAST) != 0)
		dbg_printf (" MCAST");
#ifdef DDS_SECURITY
	if ((cxp->locator->locator.flags & LOCF_SECURE) != 0)
		dbg_printf (" SECURE");
#endif
	if ((cxp->locator->locator.flags & LOCF_SERVER) != 0)
		dbg_printf (" SERVER");
	if ((cxp->locator->locator.flags & LOCF_FCLIENT) != 0)
		dbg_printf (" FCLIENT");
	if (cxp->src_mcast)
		dbg_printf (" SRC_MCAST");
	if (cxp->tx_data)
		dbg_printf (" TX");
	if (cxp->rx_data)
		dbg_printf (" RX");
#ifdef MSG_TRACE
	if (cxp->trace)
		dbg_printf (" TRACE");
#endif
	if (cxp->redundant)
		dbg_printf (" CLEANUP");
	if (cxp->cx_type) {
		dbg_printf ("\r\n\t  ");
		if (cxp->cx_side)
			dbg_printf ("  mode:%s", (cxp->cx_side == ICS_SERVER) ? "server" : "client");
#ifdef DDS_TCP
		if (cxp->cx_type != CXT_UDP && cxp->cx_mode >= ICM_CONTROL) {
			dbg_printf ("  state:%s", cx_state_str [cxp->cx_state]);
			if (cxp->cx_type == CXT_TCP ||
			    cxp->cx_type == CXT_TCP_TLS) {
				if (cxp->cx_mode != ICM_DATA) {
					c = 'C';
					s = tcp_control_state_str [cxp->p_state];
				}
				else {
					c = 'D';
					s = tcp_data_state_str [cxp->p_state];
				}
				dbg_printf ("/%c:%s", c, s);
			}
		}
#endif /* DDS_TCP */
		if (cxp->has_dst_addr || cxp->has_prefix) {
#ifdef DDS_TCP
			if ((cxp->cx_type == CXT_TCP || 
			     cxp->cx_type == CXT_TCP_TLS) &&
			    cxp->has_prefix)
				dbg_printf ("\r\n\t  ");
#endif /* DDS_TCP */
			if (cxp->has_dst_addr) {
				dbg_printf ("  dest:");
#ifdef DDS_IPV6
				if (family == AF_INET)
#endif
					dbg_printf ("%u.%u.%u.%u:%u", 
						cxp->dst_addr [12],
						cxp->dst_addr [13],
						cxp->dst_addr [14],
						cxp->dst_addr [15],
						cxp->dst_port);
#ifdef DDS_IPV6
				else if (family == AF_INET6)
					dbg_printf ("%s:%u",
						inet_ntop (AF_INET6, cxp->dst_addr, bp, rem),
						cxp->dst_port);
#endif
				if (cxp->associated)
					dbg_printf ("*");
			}
#ifdef DDS_TCP
			if (cxp->has_prefix)
				dbg_printf ("  prefix:%s",
					guid_prefix_str (&cxp->dst_prefix, buf));
#endif /* DDS_TCP */
		}
		if (cxp->timer && tmr_active (cxp->timer)) {
			n = tmr_remain (cxp->timer);
			sec = n / TICKS_PER_SEC;
			ms = (n % TICKS_PER_SEC) * TMR_UNIT_MS;
			dbg_printf ("  T:%u.%02us", sec, ms / 10);
		}
	}
	dbg_printf ("\r\n\t    r.errors:%u  w.errors:%u  empty:%u  "
		    "too_short:%u  no buffers:%u\r\n",
			cxp->stats.read_err, cxp->stats.write_err, cxp->stats.empty_read,
			cxp->stats.too_short, cxp->stats.nomem);
	dbg_printf ("\t    octets Tx/Rx:");
	DBG_PRINT_ULLONG (cxp->stats.octets_sent);
	dbg_printf ("/");
	DBG_PRINT_ULLONG (cxp->stats.octets_rcvd);
	dbg_printf ("  packets Tx/Rx:%u/%u",
			cxp->stats.packets_sent, cxp->stats.packets_rcvd);
	if (cxp->stats.nqueued)
		dbg_printf ("  queued:%u", cxp->stats.nqueued);
	dbg_printf ("\r\n");
	if (extra) {
		dbg_printf ("\t    self:%p", (void *) cxp);
		if (cxp->parent)
			dbg_printf ("  parent:%p", (void *) cxp->parent);
		if (cxp->clients)
			dbg_printf ("  clients:%p", (void *) cxp->clients);
		if (cxp->next) {
			if (cxp->cx_mode != ICM_DATA)
				dbg_printf ("  next:%p", (void *) cxp->next);
			else
				dbg_printf ("  next:(%u)", cxp->next->handle);
		}
		if (cxp->group)
			dbg_printf ("  group:(%u)", cxp->group->handle);
		if (cxp->paired)
			dbg_printf ("  paired:(%u)", cxp->paired->handle);
		dbg_printf ("\r\n");
	}
}

void rtps_ip_dump_queued (void)
{
	unsigned	i;

	dbg_printf ("Queued Locators:\r\n");
	if (nlocators)
		for (i = 0; i <= (unsigned) maxlocator; i++)
			if (ip [i] && ip [i]->stats.nqueued)
				rtps_ip_dump_cx (ip [i], 1);

}

void rtps_ip_dump (const char *buf, int extra)
{
	unsigned	i;
#ifdef DDS_TCP
	unsigned	nclients;
	IP_CX		*ccxp;
#endif

	if (buf && *buf >= '0' && *buf <= '9') {
		i = atoi (buf);
		if (i &&
		    nlocators &&
		    i - 1 <= (unsigned) maxlocator &&
		    ip [i - 1])
			rtps_ip_dump_cx (ip [i - 1], extra);
		else
			dbg_printf ("No such connection!\r\n");
		return;
	}
	dbg_printf ("# of IP receive events   = %lu\r\n", ip_rx_fd_count);
#ifdef DDS_SECURITY
	dbg_printf ("# of DTLS server events  = %lu\r\n", dtls_server_fd_count);
	dbg_printf ("# of DTLS receive events = %lu\r\n", dtls_rx_fd_count);
#endif
#ifndef SIMULATION
	if (send_udpv4) {
		dbg_printf ("Sending UDP socket:\r\n");
		rtps_ip_dump_cx (send_udpv4, extra);
	}
#ifdef DDS_IPV6
	if (send_udpv6) {
		dbg_printf ("Sending UDPv6 socket:\r\n");
		rtps_ip_dump_cx (send_udpv6, extra);
	}
#endif
#endif
#ifdef DDS_TCP
	nclients = 0;
	if (tcpv4_server) {
		dbg_printf ("TCP server:\r\n");
		rtps_ip_dump_cx (tcpv4_server, extra);
		if (tcpv4_server->clients) {
			dbg_printf ("TCP control:\r\n");
			for (ccxp = tcpv4_server->clients; ccxp; ccxp = ccxp->next) {
				rtps_ip_dump_cx (ccxp, extra);
				nclients++;
			}
		}
	}
#ifdef DDS_IPV6
	if (tcpv6_server) {
		dbg_printf ("TCPv6 server:\r\n");
		rtps_ip_dump_cx (tcpv6_server, extra);
		if (tcpv6_server->clients) {
			if (!nclients)
				dbg_printf ("TCP control:\r\n");
			for (ccxp = tcpv6_server->clients; ccxp; ccxp = ccxp->next) {
				rtps_ip_dump_cx (ccxp, extra);
				nclients++;
			}
		}
	}
#endif
	for (i = 0; i < 3 && tcp_client [i]; i++) {
		dbg_printf ("TCP client:\r\n");
		rtps_ip_dump_cx (tcp_client [i], extra);
	}
#endif
	dbg_printf ("Locators:\r\n");
	if (nlocators)
		for (i = 0; i <= (unsigned) maxlocator; i++)
			if (ip [i])
				rtps_ip_dump_cx (ip [i], extra);
}

void rtps_ip_pool_dump (size_t sizes [PPT_SIZES])
{
	print_pool_table (mem_blocks, MB_END, sizes);
}

#endif /* DDS_DEBUG */

int rtps_ipv4_attach (unsigned max_cx, unsigned max_addr)
{
	int	error = DDS_RETCODE_OK;

	act_printf ("IPv4_attach()\r\n");
	ipv4_proto.mode = config_get_mode (DC_IP_Mode, MODE_PREFERRED);
	if (ipv4_proto.mode > MODE_PREFERRED)
		ipv4_proto.mode = MODE_PREFERRED;
	if (ipv4_proto.mode == MODE_DISABLED ||
	    (ip_attached & LOCATOR_KINDS_IPv4) != 0) {
		act_printf (" - already attached!\r\n");
		return (DDS_RETCODE_OK);
	}
	ipv4_proto.udp_mode = config_get_mode (DC_UDP_Mode, MODE_ENABLED);
	act_print1 (": UDP mode=%d", ipv4_proto.udp_mode);
#ifdef DDS_TCP
	ipv4_proto.tcp_mode = config_get_mode (DC_TCP_Mode, MODE_ENABLED);
	act_print1 (", TCP mode=%d", ipv4_proto.tcp_mode);
	if (ipv4_proto.udp_mode != MODE_DISABLED)
#endif
		rtps_mux_mode = LOCATOR_KIND_UDPv4;
#ifdef DDS_TCP
	else
		rtps_mux_mode = LOCATOR_KIND_TCPv4;
#endif
	act_print1 (", Mux mode=%d\r\n", rtps_mux_mode);

	if (!ip_attached)
		max_ip_cx = max_cx;
	ipv4_proto.max_own = max_addr;

	if (ipv4_proto.udp_mode != MODE_DISABLED) {
		act_printf ("Attach UDP\r\n");
		error = rtps_udpv4_attach ();
	}
#ifdef DDS_TCP
	if (ipv4_proto.tcp_mode != MODE_DISABLED) {
		act_printf ("Attach TCP\r\n");
		error = rtps_tcpv4_attach ();
	}
#endif
	/* for TCP:
	ip_attached |= LOCATOR_KIND_TCPv4;
	error = rtps_transport_add (&rtps_tcpv4);
	if (error)
		ip_attached &= ~LOCATOR_KIND_TCPv4;
	 */
	return (error);
}

/* rtps_ipv4_detach -- Detach the RTPS over IPv4 transport handlers. */

void rtps_ipv4_detach (void)
{
	act_printf ("IPv4_detach()\r\n");
	if (ipv4_proto.mode == MODE_DISABLED ||
	    (ip_attached & LOCATOR_KINDS_IPv4) == 0) {
		act_printf (" - already detached!\r\n");
		return;
	}
#ifdef DDS_TCP
	if ((ip_attached & LOCATOR_KIND_TCPv4) != 0) {
		act_printf ("Detach TCPv4\r\n");
		rtps_tcpv4_detach ();
	}
#endif
	if ((ip_attached & LOCATOR_KIND_UDPv4) != 0) {
		act_printf ("Detach UDPv4\r\n");
		rtps_udpv4_detach ();
	}
	if (ipv4_proto.filters) {
		ip_filter_free (ipv4_proto.filters);
		ipv4_proto.filters = NULL;
	}
	if (ipv4_proto.mc_src) {
		ip_filter_free (ipv4_proto.mc_src);
		ipv4_proto.mc_src = NULL;
	}
	if (ipv4_proto.own) {
		xfree (ipv4_proto.own);
		ipv4_proto.own = NULL;
	}
	ipv4_proto.max_src_mc = 0;
	if (!ip_attached)
		rtps_ip_final ();
}

#ifdef DDS_IPV6

/* rtps_ipv6_attach -- Attach the RTPS over IPv6 transport handlers. */

int rtps_ipv6_attach (unsigned max_cx, unsigned max_addr)
{
	int	error;

	act_printf ("IPv6_attach()\r\n");
	ipv6_proto.mode = config_get_mode (DC_IPv6_Mode, MODE_ENABLED);
	if (ipv6_proto.mode > MODE_PREFERRED)
		ipv6_proto.mode = MODE_ENABLED;

	ipv6_proto.udp_mode = config_get_mode (DC_UDP_Mode, MODE_ENABLED);
	act_print1 (": UDPv6 mode=%d", ipv6_proto.udp_mode);
#ifdef DDS_TCP
	ipv6_proto.tcp_mode = config_get_mode (DC_TCP_Mode, MODE_ENABLED);
	act_print1 (", TCPv6 mode=%d", ipv6_proto.tcp_mode);
#endif
	rtps_update_mux_mode ();
	act_print1 (", Mux mode=%d\r\n", rtps_mux_mode);

	if (ipv6_proto.mode == MODE_DISABLED ||
	    (ip_attached & LOCATOR_KINDS_IPv6) != 0)
		return (DDS_RETCODE_OK);

	if (!ip_attached)
		max_ip_cx = max_cx;
	ipv6_proto.max_own = max_addr;

	if (ipv6_proto.udp_mode != MODE_DISABLED) {
		act_printf ("Attach UDPv6\r\n");
		error = rtps_udpv6_attach ();
	}
	else
		error = DDS_RETCODE_OK;
	return (error);
}

/* rtps_ipv6_detach -- Detach the RTPS over IPv6 transport handlers. */

void rtps_ipv6_detach (void)
{
	act_printf ("IPv6_detach()\r\n");
	if (ipv6_proto.mode == MODE_DISABLED ||
	    (ip_attached & LOCATOR_KINDS_IPv6) == 0) {
		act_printf (" - already detached!\r\n");
		return;
	}
	if ((ip_attached & LOCATOR_KIND_UDPv6) != 0) {
		act_printf ("Detach UDPv6\r\n");
		rtps_udpv6_detach ();
	}
	if (ipv6_proto.own) {
		xfree (ipv6_proto.own);
		ipv6_proto.own = NULL;
	}
	if (!ip_attached)
		rtps_ip_final ();
}

#endif
#ifdef MSG_TRACE

/* rtps_ip_cx_trace_mode -- Set/clear tracing on 1 (handle > 0) or all
			    IP connections.  If on == -1, tracing is toggled.
			    Otherwise tracing is set to the given argument.
			    If the connection doesn't exist, -1 is returned,
			    otherwise the new mode is returned. */

int rtps_ip_cx_trace_mode (int handle, int on)
{
	IP_CX		*cxp;
	unsigned	h;
	int		active = 0;

	if (handle < 0) {
		for (h = 0; (int) h <= maxlocator; h++)
			if ((cxp = ip [h]) != NULL) {
				if (on < 0)
					cxp->trace = !cxp->trace;
				else
					cxp->trace = (on != 0);
				if (cxp->trace)
					active = 1;
			}

	}
	else if (handle > 0) {
		if (handle - 1 > maxlocator ||
		    (cxp = ip [handle - 1]) == NULL)
			return (-1);

		if (on < 0)
			cxp->trace = !cxp->trace;
		else
			cxp->trace = (on != 0);
		if (cxp->trace)
			active = 1;
	}
	return (active);
}

/* rtps_ip_def_trace_mode -- Set/clear or toggle the default tracing mode for
			     new IP connections.  Returns the new default
			     tracing mode. */

unsigned rtps_ip_def_trace_mode (int on)
{
	if (on < 0)
		rtps_ip_dtrace = !rtps_ip_dtrace;
	else
		rtps_ip_dtrace = (on != 0);
	return (rtps_ip_dtrace);
}

#endif
