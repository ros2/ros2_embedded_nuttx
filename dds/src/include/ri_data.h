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

/* ri_data.h -- Definition and common functions for RTPS over various IP
		protocols. */

#ifndef __ri_data_h_
#define __ri_data_h_

#include "sys.h"
#include "config.h"
#include "locator.h"
#include "ipfilter.h"
#include "rtps_mux.h"

#define MAX_TX_SIZE	0xfff0	/* 64K - 16 bytes. */
#define MAX_RX_SIZE	0xfff0	/* 64K - 16 bytes. */

#define DDS_TCP_NODELAY /* Use TCP_NODELAY flag as recommanded in the spec. */

#define	CLASSD(ipa) ((unsigned char) ipa >= 224 && (unsigned char) ipa <= 239)

#ifndef IP_CX_defined
#define IP_CX_defined
typedef struct ip_cx_st		IP_CX;
#endif

#ifndef STREAM_API_defined
#define STREAM_API_defined
typedef struct stream_fct_st	STREAM_API;	/* See: ri_tcp_sock.h */
#endif

#ifndef STREAM_CB_defined
#define STREAM_CB_defined
typedef struct stream_cb_st	STREAM_CB;	/* See: ri_tcp_sock.h */
#endif

/* IP connection statistics: */
typedef struct ip_cx_stats_st {
	ULLONG		(octets_sent);	/* # of octets txed. */
	ULLONG		(octets_rcvd);	/* # of octets rxed. */
	unsigned	packets_sent;	/* # of packets txed. */
	unsigned	packets_rcvd;	/* # of packets rxed. */
	unsigned	read_err;	/* # of read errors. */
	unsigned	write_err;	/* # of write errors. */
	unsigned	empty_read;	/* # of empty reads. */
	unsigned	too_short;	/* # of too short reads. */
	unsigned	nomem;		/* # of out-of-memory conds. */
	unsigned	nqueued;	/* # of messages queued. */
} IP_CX_STATS;

typedef enum {
	CXT_UDP,		/* Plain UDP mode (default). */
	CXT_UDP_DTLS,		/* UDP over DTLS mode. */
	CXT_TCP,		/* TCP mode. */
	CXT_TCP_TLS		/* TCP over TLS mode. */
} IP_CX_TYPE;

typedef enum {
	ICM_UNKNOWN,		/* As yet unknown. */
	ICM_ROOT,		/* Main server. */
	ICM_CONTROL,		/* Control connection. */
	ICM_DATA		/* Data connection. */
} IP_CX_MODE;

typedef enum {
	ICS_NONE,		/* No specific side (rx-endpoint). */
	ICS_SERVER,		/* Server side. */
	ICS_CLIENT		/* Client side. */
} IP_CX_SIDE;

/* Connection states: */
typedef enum {
	CXS_CLOSED,		/* No connection (or UDP). */
	CXS_LISTEN,		/* S: waiting for new connections. */
	CXS_CAUTH,		/* S: client being authenticated (DTLS/TLS). */
	CXS_CONREQ,		/* C: connection pending. */
	CXS_CONNECT,		/* C: connection initiated. */
	CXS_WRETRY,		/* C: waiting to retry connection. */
	CXS_SAUTH,		/* C: server being authenticated (optional). */
	CXS_OPEN		/* Connection established. */
} IP_CX_STATE;

/* TCP Control state. */
typedef enum {
	TCS_IDLE,		/* No connection (or UDP). */
	TCS_WCXOK,		/* Setting up connection. */
	TCS_WIBINDOK,		/* Identity Bind sent. */
	TCS_CONTROL		/* Control connection established. */
} TCP_CONTROL_STATE;

/* TCP Data state. */
typedef enum {
	TDS_IDLE,		/* No connection (or UDP). */
	TDS_WCONTROL,		/* Waiting for Control connection. */
	TDS_WPORTOK,		/* Waiting for Logical Port. */
	TDS_WCXOK,		/* Wait for successful data connection. */
	TDS_WCBINDOK,		/* Waiting for Connection Bind. */
	TDS_DATA		/* Data phase active. */
} TCP_DATA_STATE;

/* Pending, as yet unassigned TCP connection: */
#ifndef TCP_FD_defined
#define TCP_FD_defined
typedef struct tcp_fd_st TCP_FD; /* See: ri_tcp_sock.h */
#endif

#ifndef TCP_MSG_defined
#define TCP_MSG_defined
typedef struct tcp_msg_frame_st TCP_MSG; /* See: ri_tcp_sock.h */
#endif

typedef enum {
	SSL_NONE,                /* No current error */
	SSL_ERROR,               /* Error */
	SSL_READ_WANT_READ,      /* The read wants to read */
	SSL_WRITE_WANT_READ,     /* The write wants to read */
	SSL_WRITE_WANT_WRITE,    /* The write wants to write */
	SSL_READ_WANT_WRITE      /* The read want to write */
} TLS_SSL_STATE;

/* Common connection data: */
struct ip_cx_st {
	LocatorNode_t	*locator;	/* Local address info. */
	unsigned char	dst_addr [16];	/* Destination address if has_dst_addr.*/
	uint16_t	users;		/* Use count. */
	uint16_t	dst_port;	/* Destination port if cx_mode. */
	GuidPrefix_t	dst_prefix;	/* GUID prefix if has_prefix. */
	unsigned	dst_forward;	/* Destination can forward. */
	unsigned	retries:4;	/* # of retries. */
	unsigned	cx_type:2;	/* Connection type (IP_CX_TYPE). */
	unsigned	cx_mode:2;	/* Connection mode (IP_CX_MODE). */
	unsigned	cx_side:2;	/* Side of connection (IP_CX_SIDE). */
	unsigned	cx_state:3;	/* Connection state (IP_CX_STATE). */
	unsigned	p_state:3;	/* Protocol state (*_CONTROL/DATA_STATE).*/
	unsigned	associated:1;	/* Connection association done. */
	unsigned	src_mcast:1;	/* Multicast source? */
	unsigned	redundant:1;	/* May be purged? */
	unsigned	share:1;	/* Allow fd sharing. */
	unsigned	tx_data:1;	/* Can send data on connection. */
	unsigned	rx_data:1;	/* Can receive data on connection. */
	unsigned	fd_owner:1;	/* Owns the file descriptor. */
	unsigned	trace:1;	/* Trace messages if set. */
	unsigned	has_prefix:1;	/* GUID prefix present. */
	unsigned	has_dst_addr:1;	/* Destination address present. */
	unsigned	cxbs_queued:1;	/* ConnectionBindSuccess queued. */
	unsigned	id;		/* Higher layer id. */
	unsigned	handle;		/* Connection handle. */
	int		fd;		/* File descriptor. */
	RMREF		*head;		/* Pending messages list. */
	RMREF		*tail;
	IP_CX		*next;		/* Next in connection list. */
	IP_CX		*clients;	/* Client connections. */
	IP_CX		*parent;	/* Parent connection. */
	IP_CX		*group;		/* Connection group. */
	IP_CX		*paired;	/* Paired connection. */
	TCP_FD		*pending;	/* Pending connections. */
#ifdef DDS_TCP
	long		label;		/* Connection label. */
	unsigned	data_length;	/* Data length. */
	void		*data;		/* Data. */
	uint32_t	transaction_id;	/* Transaction Id. */
	STREAM_API	*stream_fcts;	/* Lower layer interface for managing connections and transferring data. */
	STREAM_CB	*stream_cb;	/* Callbacks to be used by the installed STREAM_API implementation. */
#endif
	void		*sproto;	/* Security (DTLS/TLS) or raw TCP context. */
	Timer_t		*timer;		/* Retry timer. */
	IP_CX_STATS	stats;		/* Statistics. */
};

typedef struct ip_ctrl_st {
	RTPS_TRANSPORT	*transport;
	int		(*enable) (void);
	void		(*disable) (void);
} IP_CTRL;

typedef enum {
	IPK_UDP,
	IPK_TCP
} IP_KIND;

#define	N_IP_KIND	(((unsigned) IPK_TCP) + 1)

typedef struct ip_proto_st {
	IP_MODE		mode;		/* Current mode. */
	unsigned	num_own;	/* Current # of local IP addresses. */
	unsigned	max_own;	/* Maximum # of local IP addresses. */
	unsigned char	*own;		/* List of local IP addresses. */
	Scope_t		min_scope;	/* Minimum scope. */
	Scope_t		max_scope;	/* Minimum scope. */
	uint32_t 	mcast_if;	/* Forced via _MCAST_DEST/INTF= */
	int		wait_mc_if;	/* No route yet for mcast_if yet. */
	int		enabled;	/* If currently enabled. */
	unsigned	max_src_mc;	/* # of local IP data interfaces. */
	IpFilter_t	filters;	/* Source address filter. */
	IpFilter_t	mc_src;		/* Multicast capable sources. */
	unsigned	nprotos;	/* # of assigned protocols. */
	IP_MODE		udp_mode;	/* Current UDP mode. */
	IP_MODE		tcp_mode;	/* Current TCP mode. */
	IP_CTRL		*control [N_IP_KIND]; /* Assigned protocols. */
} IP_PROTO;

extern IP_PROTO	ipv4_proto;
#ifdef DDS_IPV6
extern IP_PROTO	ipv6_proto;
#endif

extern unsigned	ip_attached;
extern unsigned char *rtps_tx_buf;
extern unsigned char *rtps_rx_buf;

extern unsigned long ip_rx_fd_count;
extern unsigned long dtls_server_fd_count;
extern unsigned long dtls_rx_fd_count;

int rtps_ipv4_init (RMRXF    rxf,
		    MEM_DESC msg_hdr,
		    MEM_DESC msg_elem);

/* Initialize the IPv4-based RTPS transport mechanisms. */

void rtps_ipv4_final (void);

/* Finalize the IPv4-based RTPS tramsport mechanisms. */

int rtps_ipv6_init (RMRXF    rxf,
		    MEM_DESC msg_hdr,
		    MEM_DESC msg_elem);

/* Initialize the IPv6-based RTPS transport mechanisms. */

void rtps_ipv6_final (void);

/* Finalize the IPv6-based RTPS tramsport mechanisms. */

RMBUF *rtps_parse_buffer (IP_CX *cxp, unsigned char *buf, unsigned len);

/* Parse a (received) packet in a buffer. */

void rtps_rx_buffer (IP_CX         *cxp,
		     unsigned char *buf,
		     size_t        len,
		     unsigned char *saddr,
		     uint32_t      sport);

/* Process an RTPS message in a buffer. */

void rtps_rx_msg (IP_CX         *cxp,
		  RMBUF         *msg,
		  unsigned char *saddr,
		  uint32_t      sport);

/* Process an RTPS message in message format. */

void rtps_ip_rx_fd (SOCKET fd, short revents, void *arg);

/* Should be called whenever the file descriptor has received data ready. */

void rtps_ip_rem_locator (unsigned id, LocatorNode_t *lnp);

/* Remove a locator. */

IP_CX *rtps_ip_alloc (void);

/* Allocate a new IP connection record. */

void rtps_ip_free (IP_CX *cx);

/* Free a previously allocated IP connection. */

void rtps_ip_send (unsigned id, void *dest, int dlist, RMBUF *msgs);

/* Send RTPS messages to the given IP locator. */

IP_CX *rtps_ip_lookup (unsigned id, const Locator_t *lp);

/* Lookup the context corresponding to a locator. */

IP_CX *rtps_ip_lookup_peer (unsigned id, const Locator_t *lp);

/* Lookup the context of a peer entity corresponding to a locator. */

IP_CX *rtps_ip_lookup_port (unsigned id, LocatorKind_t kind, unsigned port);

/* Lookup the context of a peer entity corresponding to a port. */

typedef void (*IP_VISITF) (IP_CX *cxp, void *data);

/* Visit connection function signature. */

void rtps_ip_foreach (IP_VISITF fct, void *data);

/* Call the visit function for each connection with the given data. */

unsigned rtps_ip_update (LocatorKind_t kind, Domain_t *dp, int done);

/* Update the locators and returns the number of valid locators of this kind. */

unsigned rtps_ip_new_handle (IP_CX *cxp);

/* Assign a new connection handle. */

void rtps_ip_free_handle (unsigned h);

/* Free a connection handle. */

IP_CX *rtps_ip_from_handle (unsigned handle);

/* Get an IP context from a handle. */

IP_CX *rtps_src_mcast_next (unsigned      id,
			    LocatorKind_t kind,
			    unsigned char flags,
			    IP_CX         *prev);

/* Get the next IP context that can be used as a multicast source. */

void rtps_ip_trace (unsigned            handle,
		    char                dir,
		    Locator_t           *lp,
		    const unsigned char *addr,
		    unsigned            port,
		    unsigned            length);

/* Trace an IP message. */

int rtps_ip_cx_trace_mode (int handle, int on);

/* Set/clear tracing on 1 (handle > 0) or all (handle == -1) IP connections.
   If on is -1, tracing is toggled. Otherwise 0 or 1 clears or sets tracing
   explicitly.  If the connection doesn't exist, -1 is returned, otherwise
   the new mode is returned. */

unsigned rtps_ip_def_trace_mode (int on);

/* Set/clear or toggle the default tracing mode for new IP connections.
   Returns the new default tracing mode. */

void dds_ssl_init (void);

/* Security : initialize SSL. */

void dds_ssl_finish (void);

/* Security : finalize SSL. */

void rtps_ip_dump_cx (IP_CX *cxp, int extra);

/* Debug: dump a single IP connection context. */


#endif /* !__ri_data_h_ */

