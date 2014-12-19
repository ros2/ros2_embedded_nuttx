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

/* ri_dtls.c -- RTPS over DTLS transports. */

#ifdef DDS_SECURITY

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#include "win.h"
#define ERRNO	WSAGetLastError()
#else
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#define ERRNO errno
#endif
#include "sock.h"
#include "log.h"
#include "error.h"
#include "debug.h"
#include "openssl/bio.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "openssl/rand.h"
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_data.h"
#endif
#include "rtps_ip.h"
#include "ri_data.h"
#include "ri_dtls.h"

#ifdef ANDROID
#include "sys/socket.h"
/* Type to represent a port.  */
typedef uint16_t in_port_t;
#endif

#define DTLS_IDLE_CX_TIMEOUT	25	/* When this timer expires and there are
					   no messages received or sent (independently
					   counter), we assume that the connection
					   should come to an end. [sec]. */
#define COOKIE_SECRET_LENGTH	16
#define DTLS_MAX_RETRY_COUNT	2	/* # of retries we allow for DTLS timeouts */

/* #define NO_CERTIFICATE_TIME_VALIDITY_CHECK */

typedef enum {
	DTLS_ROLE_ERR,	/* Can't determine role -- error. */
	DTLS_SERVER,	/* Server side of the DTLS connection */
	DTLS_CLIENT 	/* Client side of the DTLS connection */
} DTLS_CX_SIDE;

typedef enum {
	DTLS_SERVER_RX,	/* Server: wait for Client Hello. */
	DTLS_ACCEPT,	/* SData: Server Hello sent, wait on SSL_accept ().*/
	DTLS_CONNECT,	/* CData: Client Hello sent, wait on SSL_connect ().*/
	DTLS_DATA	/* SData/CData: active Data exchange. */
} DTLS_STATE;

typedef enum {
	SSW_IDLE,	/* Inactive, i.e. nothing to write. */
	SSW_WWRITE,	/* Wait until we can write (again). */
	SSW_WREAD	/* Wait until we can read. */
} SSW_STATE;

typedef enum {
	TMR_NOT_RUNNING,	/* Timer is not running */
	TMR_DTLS_PROTOCOL,	/* Timer is in use for handling DTLS protocol timeouts */
	TMR_IDLE_CX		/* Timer is counting messages to detect idle connections */
} TMR_STATE;

typedef enum {
	SSR_IDLE,	/* Inactive, i.e. no data ready for reading. */
	SSR_WREAD,	/* Wait until we can read again. */
	SSR_WWRITE	/* Wait until we can write. */
} SSR_STATE;

typedef struct {
	SSL		*ssl;		/* SSL context. */
	DTLS_STATE	state;		/* Current state. */
	SSW_STATE	wstate;		/* Writer state. */
	SSR_STATE	rstate;		/* Reader state. */
	TMR_STATE	tstate;		/* Identify why the timer is currently running */
	unsigned	rcvd_msg_cnt;	/* Number of received messages in last time window */
	unsigned	sent_msg_cnt;	/* Number of sent messages in the last time window */
} DTLS_CX;

static SSL_CTX		*dtls_server_ctx;
static SSL_CTX		*dtls_client_ctx;
static int		cookie_initialized;
static unsigned char	cookie_secret [COOKIE_SECRET_LENGTH];

static in_port_t	dtls_server_port;	/* UDP port to use for our DTLS server(s) */
static IP_CX		*dtls_v4_servers;
#ifdef DDS_IPV6
static IP_CX		*dtls_v6_servers;
#endif
static IP_CX		*dtls_pending_cx; /* list of connections attempts, not yet associated with their
                                             respective server IP_CX. Note that this list is a mix of
					     IPv4 and IPv6 contexts. */
unsigned long		dtls_rx_fd_count;
unsigned long		dtls_server_fd_count;

int			dtls_available = 1;

static char *dtls_cx_state_str [] = {	/* See enum IP_CX_STATE */
	"Closed", "Listen", "CAuth", "ConReq", "Connect", "WRetry", "SAuth", "Open"
};
/* Convert struct timeval * to Ticks_t. Result is rounded up with a single tick. */
#define TIMEVAL_TO_TICKS(tv) (((tv)->tv_sec) * TICKS_PER_SEC + (tv)->tv_usec / (1000 * TMR_UNIT_MS) + 1)

void rtps_dtls_init (void)
{
	/* nada */
}

void rtps_dtls_finish (void)
{
	/* nada */
}

/*#define LOG_DTLS_FSM		 ** Log DTLS usage details */
/*#define LOG_POLL_EVENTS	 ** Log poll events and state changes */
/*#define LOG_SSL_FSM		 ** Log SSL connection state changes */
/*#define LOG_CERT_CHAIN_DETAILS ** Log some details about the certificate when doing certificate verification */
/*#define LOG_CONNECTION_CHANGES ** Log any sort of changes in our own connections - can become very verbose */
/*#define LOG_SEARCH_CTX	 ** Log context searches, based on locators */

#ifdef LOG_DTLS_FSM
#define	dtls_print(s)			log_printf (RTPS_ID, 0, s)
#define	dtls_print1(s,a)		log_printf (RTPS_ID, 0, s,a)
#define	dtls_print2(s,a1,a2)		log_printf (RTPS_ID, 0, s,a1,a2)
#define	dtls_print3(s,a1,a2,a3)		log_printf (RTPS_ID, 0, s,a1,a2,a3)
#define	dtls_print4(s,a1,a2,a3,a4)	log_printf (RTPS_ID, 0, s,a1,a2,a3,a4)
#define	dtls_fflush()			log_flush (RTPS_ID, 0)

#else
#define	dtls_print(s)
#define	dtls_print1(s,a)
#define	dtls_print2(s,a1,a2)
#define	dtls_print3(s,a1,a2,a3)
#define	dtls_print4(s,a1,a2,a3,a4)
#define	dtls_fflush()
#endif

#ifdef LOG_SSL_FSM
static void rtps_dtls_info_callback (const SSL *ssl, int type, int val)
{
	const char *str;
	int w;
	BIO *bio;
	int fd;

	bio = SSL_get_rbio (ssl);
	fd = BIO_get_fd (bio, NULL);

	w = type & ~SSL_ST_MASK;

	if (w & SSL_ST_CONNECT)
		str = "SSL_connect";
	else if (w & SSL_ST_ACCEPT)
		str = "SSL_accept";
	else
		str = "undefined";

	if (type & SSL_CB_LOOP) {
		log_printf (RTPS_ID, 0, "DTLS/SSL trace (fd %d): %s:%s\r\n", fd, str, SSL_state_string_long (ssl));
	}
	else if (type & SSL_CB_ALERT) {
		str = (type & SSL_CB_READ) ? "read" : "write";
		log_printf (RTPS_ID, 0, "DTLS/SSL trace (fd %d): alert %s:%s:%s\r\n", fd, str,
				SSL_alert_type_string_long (val),
				SSL_alert_desc_string_long (val));
	}
	else if (type & SSL_CB_EXIT) {
		if (val == 0)
			log_printf (RTPS_ID, 0, "DTLS/SSL trace (fd %d): %s:failed in %s\r\n", fd, str, SSL_state_string_long (ssl));
		else if (val < 0)
			log_printf (RTPS_ID, 0, "DTLS/SSL trace (fd %d): %s:error in %s\r\n", fd, str, SSL_state_string_long (ssl));
	}
}

static void rtps_dtls_trace_openssl_fsm (SSL_CTX *ctx)
{
	SSL_CTX_set_info_callback (ctx, rtps_dtls_info_callback);
}
#else
#define rtps_dtls_trace_openssl_fsm(ctx) 
#endif

#ifdef LOG_POLL_EVENTS
static void dtls_dump_poll_events (int fd, short events)
{
	log_printf (RTPS_ID, 0, "{ fd: %d -%s }\r\n", fd, dbg_poll_event_str (events));
}

#else
#define dtls_dump_poll_events(fd, events)
#endif

#ifdef LOG_CONNECTION_CHANGES
static void trace_connection_changes (void)
{
	log_printf (RTPS_ID, 0, ">>>>>>>>>\r\n");
	rtps_dtls_dump ();
	log_printf (RTPS_ID, 0, "<<<<<<<<<\r\n");
}
#else
static void trace_connection_changes (void)
{
}
#endif

/* remove_from_ip_cx_list -- Remove target from headp list (connected through
			     'next' chain). */

static int remove_from_ip_cx_list (IP_CX **headp, IP_CX *target)
{
	IP_CX 	*p, *pp;

	if (!headp || !*headp)
		return -1; /* empty list: nothing to do */

	for (pp = NULL, p = *headp; p; pp = p, p = p->next) {
		if (p == target)
			break;
	}
	if (p) {
		if (pp)
			pp->next = p->next;
		else
			*headp = p->next;
		p->next = NULL;
		return 0;
	}
	return -1; /* not found */
}

static void rtps_dtls_dump_openssl_error_stack (const char *msg)
{
	unsigned long	err;
	const char	*file, *data;
	int		line, flags;
	char		buf[256];

	log_printf (RTPS_ID, 0, "%s", msg);
	while ((err = ERR_get_error_line_data (&file, &line, &data, &flags)))
		log_printf (RTPS_ID, 0, "    err %lu @ %s:%d -- %s\r\n", err, file, line,
				ERR_error_string (err, buf));
}

static void rtps_dtls_shutdown_connection (IP_CX *cxp)
{
	log_printf (RTPS_ID, 0, "DTLS(%u): Shutting down connection on [%d]\r\n", cxp->handle, cxp->fd);

	rtps_dtls_cleanup_cx (cxp);

	if (cxp->fd) {
		sock_fd_remove_socket (cxp->fd);
		close (cxp->fd);
	}
	if (cxp->handle) {
		cxp->locator->locator.handle = 0;
#if 0
		if (cxp->dst_loc) {
			cxp->dst_loc->handle = 0;
			cxp->dst_loc = NULL;
		}
#endif
		rtps_ip_free_handle (cxp->handle);
		cxp->handle = 0;
	}
	if ((cxp->parent != cxp) && cxp->locator)
		xfree (cxp->locator);

	rtps_ip_free (cxp);
	trace_connection_changes ();
}

static void rtps_dtls_timeout (uintptr_t arg);

static void rtps_dtls_start_idle_cx_timer (IP_CX *cxp)
{
	DTLS_CX	*dtls = cxp->sproto;

	dtls_print2 ("DTLS-timer for h:%u fd:%d: start 'idle connection' timer\r\n", cxp->handle, cxp->fd);
	dtls->rcvd_msg_cnt = dtls->sent_msg_cnt = 0;
	dtls->tstate = TMR_IDLE_CX;
	tmr_start (cxp->timer, DTLS_IDLE_CX_TIMEOUT * TICKS_PER_SEC, (uintptr_t) cxp, rtps_dtls_timeout);
}

static void rtps_dtls_start_protocol_timer (IP_CX *cxp)
{
	DTLS_CX		*dtls = cxp->sproto;
	struct timeval	to;

	/*
	 * Note: we reuse (abuse?) dtls->rcvd_msg_cnt to count the number recurring DTLS timeouts. We
	 * do this to limit the overall timeout before a connection attempt timeout the DTLS-way, which is
	 * in the order of 10 minutes.
	 */
	if (DTLSv1_get_timeout (dtls->ssl, &to)) {
		dtls_print2 ("DTLS-timer for h:%u fd:%d: start DTLS protocol timer\r\n", cxp->handle, cxp->fd);
		tmr_start (cxp->timer, TIMEVAL_TO_TICKS (&to), (uintptr_t) cxp, rtps_dtls_timeout);
		if (dtls->tstate == TMR_DTLS_PROTOCOL)
			dtls->rcvd_msg_cnt++;
		else {
			dtls->tstate = TMR_DTLS_PROTOCOL;
			dtls->rcvd_msg_cnt = 0;
		}
	}
}

static void rtps_dtls_timeout (uintptr_t arg)
{
	IP_CX		*cxp = (IP_CX *) arg;
	DTLS_CX		*dtls = cxp->sproto;
	int		error;

	if (!dtls)
		return;

	dtls_print3 ("DTLS-timeout on h:%u fd:%d type %d\r\n", cxp->handle, cxp->fd, dtls->tstate);
	switch (dtls->tstate) {
		case TMR_IDLE_CX:
			dtls_print2 ("    #msgs rcvd = %u -- sent = %u\r\n", dtls->rcvd_msg_cnt, dtls->sent_msg_cnt);
			if (dtls->rcvd_msg_cnt == 0) {
				rtps_dtls_shutdown_connection (cxp);
				/* !! do not access cxp anymore !! */
				return;
			}
			else
				rtps_dtls_start_idle_cx_timer (cxp);
			break;

		case TMR_DTLS_PROTOCOL:
			dtls_print ("    DTLS protocol timeout\r\n");
			switch (DTLSv1_handle_timeout (dtls->ssl)) {
				case 0:
					dtls_print ("    DTLS timer did not timeout (yet)\r\n");
					rtps_dtls_start_protocol_timer (cxp);
					break;

				case 1:
					if (dtls->rcvd_msg_cnt > DTLS_MAX_RETRY_COUNT) {
						rtps_dtls_shutdown_connection (cxp);
						return;
					}
					dtls_print ("    DTLS timer handled\r\n");
					rtps_dtls_start_protocol_timer (cxp);
					break;

				case -1:
				default:
					error = SSL_get_error (dtls->ssl, 0);
					dtls_print1 ("    DTLS_handle_timeout () failed - error = %d\r\n", error);
					rtps_dtls_dump_openssl_error_stack ("Protocol timout error");
					if (error != SSL_ERROR_NONE)
						rtps_dtls_shutdown_connection (cxp);
						/* !! do not access cxp anymore !! */
					return;
			}
			break;

		default:
			dtls_print ("    Unexpected timeout\r\n");
			break;
	}
}

/* rtps_dtls_socket -- Create a new socket and bind/connect it to the given
		       addresses. It is assumed that both addresses belong to
		       the same family. */

static int rtps_dtls_socket (struct sockaddr *bind_addr, struct sockaddr *connect_addr)
{
	const int	on = 1;
#ifdef DDS_IPV6
	const int	off = 0;
#endif
	int 		fd;
	socklen_t	len;

	fd = socket (bind_addr->sa_family, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror ("rtps_dtls_socket (): socket () failure");
		log_printf (RTPS_ID, 0, "rtps_dtls_socket: socket () failed - errno = %d.\r\n", ERRNO);
		return (-1);
	}

	if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, (socklen_t) sizeof (on))) {
		perror ("rtps_dtls_socket (): setsockopt () failure");
		log_printf (RTPS_ID, 0, "setsockopt (REUSEADDR) failed - errno = %d.\r\n", ERRNO);
		goto cleanup;
	}

#ifndef WIN32
#ifdef SO_REUSEPORT
	if (setsockopt (fd, SOL_SOCKET, SO_REUSEPORT, (const void*) &on, (socklen_t) sizeof (on))) {
		perror ("rtps_dtls_socket (): setsockopt () failure");
		log_printf (RTPS_ID, 0, "setsockopt (REUSEPORT) failed - errno = %d.\r\n", ERRNO);
		/* Don't cleanup if this fails. Some platforms don't support this
		   socket option.  Example: raspberry pi arch linux kernel 3.6.11. */

		/*goto cleanup;*/
	}
#endif
#endif

	len = sizeof (struct sockaddr_in);
#ifdef DDS_IPV6
	if (bind_addr->sa_family == AF_INET6) {
		if (setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &off, sizeof (off))) {
			perror ("rtps_dtls_socket (): setsockopt () failure");
			log_printf (RTPS_ID, 0, "setsockopt (IPV6_ONLY) failed - errno = %d.\r\n", ERRNO);
			goto cleanup;
		}
		len = sizeof (struct sockaddr_in6);
	}
#endif

	if (bind (fd, bind_addr, len)) {
		perror ("rtps_dtls_socket (): bind () failure");
		log_printf (RTPS_ID, 0, "bind () failed - errno = %d.\r\n", ERRNO);
		goto cleanup;
	}

	if (connect (fd, connect_addr, len)) {
		perror ("rtps_dtls_socket (): connect () failure");
		log_printf (RTPS_ID, 0, "connect () failed - errno = %d.\r\n", ERRNO);
		goto cleanup;
	}

	return (fd);

cleanup:
	close (fd);
	return (-1);
}

static int set_socket_nonblocking (int fd)
{
	int mode = fcntl (fd, F_GETFL, 0);
	return ((mode != -1) ? fcntl (fd, F_SETFL, mode | O_NONBLOCK) : -1);
}

#ifdef DDS_IPV6
#define	SA_LENGTH	sizeof (struct sockaddr_in6)
#else
#define	SA_LENGTH	sizeof (struct sockaddr_in)
#endif

static void rtps_dtls_connect (IP_CX *cxp)
{
	int		error;
	unsigned char	sabuf [SA_LENGTH];
	DTLS_CX		*dtls = cxp->sproto;
	int		(*action) (SSL*);
	char		side;

	if (dtls->state == DTLS_ACCEPT) {
		action = SSL_accept;
		side = 'S';
	}
	else if (dtls->state == DTLS_CONNECT) {
		action = SSL_connect;
		side = 'C';
	} else
		fatal_printf ("%s(): invalid value for dtls->state (%d)", __FUNCTION__, dtls->state);

	dtls_print4 ("DTLS(%c) h:%u fd:%d %s() - Continue establishing the DTLS connection\r\n",
			side, cxp->handle, cxp->fd, side == 'S' ? "SSL_accept" : "SSL_connect");
	dtls_fflush ();

	error = SSL_get_error (dtls->ssl, action (dtls->ssl));
	rtps_dtls_start_protocol_timer (cxp);
	switch (error) {
		case SSL_ERROR_NONE:
			if (accept_ssl_connection (dtls->ssl,
						   loc2sockaddr (
							cxp->locator->locator.kind,
							cxp->dst_port,
							cxp->dst_addr,
							sabuf),
						   cxp->id)
					== DDS_AA_ACCEPTED) {
				log_printf (RTPS_ID, 0, "DTLS.%c(%u): Connection accepted on [%d]!\r\n", side, cxp->handle, cxp->fd);
				sock_fd_event_socket (cxp->fd, POLLOUT, 1); /* start sending enqueued data, if any */
				dtls->state = DTLS_DATA;
				cxp->cx_state = CXS_OPEN;
				cxp->p_state = TDS_DATA;
				trace_connection_changes ();
				rtps_dtls_start_idle_cx_timer (cxp);
				break;
			}
			log_printf (RTPS_ID, 0, "DTLS.%c(%u): Connection refused on [%d]!\r\n", side, cxp->handle, cxp->fd);
			/* FALLTHRU */

		case SSL_ERROR_ZERO_RETURN:
			rtps_dtls_shutdown_connection (cxp);
			break;

		case SSL_ERROR_WANT_WRITE:
			sock_fd_event_socket (cxp->fd, POLLOUT, 1);
			break;

		case SSL_ERROR_WANT_READ:
			sock_fd_event_socket (cxp->fd, POLLOUT, 0);
			break;

		default:
			log_printf (RTPS_ID, 0, "%s () error# %d\r\n", (side == 'S' ? "SSL_accept" : "SSL_connect"), error);
			rtps_dtls_dump_openssl_error_stack (side == 'S' ? "DTLS(S): SSL_accept () error:" : "DTLS(C): SSL_connect () error:");
			rtps_dtls_shutdown_connection (cxp);
			break;
	}
}

static void rtps_dtls_listen (IP_CX *cxp)
{
	DTLS_CX	        *dtls = cxp->sproto;
	unsigned char	sabuf [SA_LENGTH];
	int	        error;

	dtls_print2 ("DTLS(S) h:%u fd:%d DTLSv1_listen()\r\n", cxp->handle, cxp->fd);
	cxp->cx_state = CXS_LISTEN;
	trace_connection_changes ();
	error = SSL_get_error (dtls->ssl, DTLSv1_listen (dtls->ssl, (struct sockaddr *) sabuf));
	rtps_dtls_start_protocol_timer (cxp);
	switch (error) {
		case SSL_ERROR_NONE:
			dtls->state = DTLS_ACCEPT;
			cxp->cx_state = CXS_CAUTH;
			trace_connection_changes ();

			/* This is an extra check to see if the handshake needs to be completed */
			if (check_DTLS_handshake_initiator (loc2sockaddr (cxp->locator->locator.kind,
								     cxp->dst_port,
								     cxp->dst_addr,
								     sabuf),
						       cxp->id) == DDS_AA_ACCEPTED) {
				/*
				 * We immediately try to process the second "Client Hello". Experiments showed that this
				 * avoid that we have to wait on a retransmit.
				 */
				rtps_dtls_connect (cxp);
				break;
			}
			/* Validation failed, explicit fall-through to close connection. */

		case SSL_ERROR_ZERO_RETURN:
			rtps_dtls_shutdown_connection (cxp);
			break;

		case SSL_ERROR_WANT_WRITE:
			sock_fd_event_socket (cxp->fd, POLLOUT, 1);
			break;

		case SSL_ERROR_WANT_READ:
			sock_fd_event_socket (cxp->fd, POLLOUT, 0);
			break;

		default:
			dtls_print2 ("%s () error# %d\r\n", __FUNCTION__, error);
			rtps_dtls_dump_openssl_error_stack ("DTLSv1_listen ()");
			rtps_dtls_shutdown_connection (cxp);
			break;
	}
}

/* rtps_dtls_send_msgs -- Attempt to send a single packet on a DTLS connection. */

static void rtps_dtls_send_msgs (IP_CX *cxp)
{
	RMBUF		*mp;
	RME		*mep;
	RMREF		*mrp;
	DTLS_CX		*dtls = (DTLS_CX *) cxp->sproto;
#ifdef MSG_TRACE
	unsigned char	*ap;
#endif
	size_t		len, n;
	int		error;

	do {
		dtls->wstate = SSW_IDLE;

		/* Copy message to buffer. */
		mrp = cxp->head;
		if (!mrp) {
			sock_fd_event_socket (cxp->fd, POLLOUT, 0);
			return;
		}
		mp = mrp->message;
		memcpy (rtps_tx_buf, &mp->header, sizeof (mp->header));
		len = sizeof (mp->header);
		for (mep = mp->first; mep; mep = mep->next) {
			if ((mep->flags & RME_HEADER) != 0) {
				n = sizeof (mep->header);
				if (len + n > MAX_TX_SIZE)
					break;
			
				memcpy (&rtps_tx_buf [len], &mep->header, n);
				len += n;
			}
			if ((n = mep->length) != 0) {
				if (len + n > MAX_TX_SIZE)
					break;

				if (mep->data != mep->d && mep->db)
					db_get_data (&rtps_tx_buf [len], mep->db, mep->data, 0, n - mep->pad);
				else
					memcpy (&rtps_tx_buf [len], mep->data, n - mep->pad);
				len += n;
			}
		}

		/* Transmit message. */
		len = SSL_write (dtls->ssl, rtps_tx_buf, len);
		error = SSL_get_error (dtls->ssl, len);
		rtps_dtls_start_protocol_timer (cxp);

		/* Check returned result. */
		switch (error) {
			case SSL_ERROR_NONE:
				/* Write successful. */
#ifdef MSG_TRACE
				if (cxp->trace || (mp->element.flags & RME_TRACE) != 0) {
					ap = cxp->dst_addr;
					if (cxp->locator->locator.kind == LOCATOR_KIND_UDPv4)
						ap += 12;
					rtps_ip_trace (cxp->handle, 'T',
						       &cxp->locator->locator,
						       ap, cxp->dst_port,
						       len);
				}
#endif
				++dtls->sent_msg_cnt;
				ADD_ULLONG (cxp->stats.octets_sent, (unsigned) len);
				cxp->stats.packets_sent++;
				cxp->head = mrp->next;
				mrp->next = NULL;
				rtps_unref_message (mrp);
				break;

			case SSL_ERROR_WANT_WRITE:
				dtls->wstate = SSW_WWRITE;
				sock_fd_event_socket (cxp->fd, POLLOUT, 1);
				return;

			case SSL_ERROR_WANT_READ:
				dtls->wstate = SSW_WREAD;
				sock_fd_event_socket (cxp->fd, POLLOUT, 0);
				return;

			case SSL_ERROR_ZERO_RETURN:
				rtps_dtls_shutdown_connection (cxp);
				return;

			case SSL_ERROR_SSL:
			default:
				dtls_print2 ("SSL_write () error# %d fd %d\r\n", error, cxp->fd);
				rtps_dtls_dump_openssl_error_stack ("SSL_write () error");
				rtps_dtls_shutdown_connection (cxp);
				/* cxp and dtls are freed now - do not access them anymore */
				return;
		}
	}
	while (cxp->head && dtls->wstate == SSW_IDLE);
	if (!cxp->head)
		sock_fd_event_socket (cxp->fd, POLLOUT, 0);
}

/* rtps_dtls_receive -- Attempt to receive packets from a DTLS connection. */

static void rtps_dtls_receive (IP_CX *cxp)
{
	DTLS_CX			*dtls = (DTLS_CX *) cxp->sproto;
	ssize_t			nread;
	int			error;

	do {
		dtls->rstate = SSR_IDLE;
		nread = SSL_read (dtls->ssl, rtps_rx_buf, MAX_RX_SIZE);
		error = SSL_get_error (dtls->ssl, nread);
		rtps_dtls_start_protocol_timer (cxp);
		switch (error) {
			case SSL_ERROR_NONE:
				++dtls->rcvd_msg_cnt;
				rtps_rx_buffer (cxp, rtps_rx_buf, nread, cxp->dst_addr, cxp->dst_port);
				break;

			case SSL_ERROR_WANT_READ:
				dtls->rstate = SSR_WREAD;
				sock_fd_event_socket (cxp->fd, POLLOUT, 0);
				break;

			case SSL_ERROR_WANT_WRITE:
				dtls->rstate = SSR_WWRITE;
				sock_fd_event_socket (cxp->fd, POLLOUT, 1);
				break;

			case SSL_ERROR_ZERO_RETURN:
				rtps_dtls_shutdown_connection (cxp);
				return;

			case SSL_ERROR_SSL:
			default:
				dtls_print2 ("SSL_read () error# %d fd %d\r\n", error, cxp->fd);
				rtps_dtls_dump_openssl_error_stack ("SSL_read () error");
				rtps_dtls_shutdown_connection (cxp);
				/* cxp and dtls are freed now - do not access them anymore */
				return;
		}
	}
	while (SSL_pending (dtls->ssl) && dtls->rstate != SSR_WREAD);
}

/* rtps_dtls_complete_pending_cx -- Fill in the last details of a pending
				    connection. The end result is that pcxp is
				    removed from the list of pending connections
				    and added to the list of clients of a server
				    IP_CX. */

static void rtps_dtls_complete_pending_cx (IP_CX *pcxp, int fd)
{
	IP_CX 		*scx;
	char		sa_buf [SA_LENGTH];
	socklen_t	sa_len = SA_LENGTH;
	struct sockaddr *sap;

	sap = (struct sockaddr *) sa_buf;
	getsockname (fd, sap, &sa_len);
	if (sap->sa_family == AF_INET) {
		struct sockaddr_in *sa = (struct sockaddr_in *) sa_buf;

		for (scx = dtls_v4_servers; scx; scx = scx->next) {
			if (!memcmp(&scx->locator->locator.address [12], &sa->sin_addr, 4)
				&& (scx->locator->locator.port == ntohs(sa->sin_port)))
				break;
		}
	}
#ifdef DDS_IPV6
	else if (sap->sa_family == AF_INET6) {
		struct sockaddr_in6 *sa = (struct sockaddr_in6 *) sa_buf;

		for (scx = dtls_v6_servers; scx; scx = scx->next) {
			if (!memcmp(scx->locator->locator.address, &sa->sin6_addr, 16)
				&& (scx->locator->locator.port == ntohs(sa->sin6_port)))
				break;
		}
	}
#endif
	else
		scx = NULL;

	if (!scx)
		return;


	memcpy (pcxp->locator->locator.address, scx->locator->locator.address, 16);
	pcxp->parent = scx;
	pcxp->id = scx->id;

	/* remove pcxp from pending list !! */
	if (remove_from_ip_cx_list (&dtls_pending_cx, pcxp)) {
		dtls_print1 ("DTLS(%u): Failed to find back connection in list of pending connections\r\n", pcxp->handle);
	}

	pcxp->next = scx->clients;
	scx->clients = pcxp;
}

/* rtps_dtls_rx_fd -- Function that should be called whenever the file 
		      descriptor has DTLS receive data ready from a client. */

static void rtps_dtls_rx_fd (SOCKET fd, short revents, void *arg)
{
	IP_CX		*cxp = arg;
	DTLS_CX		*dtls = cxp->sproto;

	ARG_NOT_USED (fd)
	
	if (!dtls)
		return;

	dtls_dump_poll_events (fd, revents);
	if (revents & (POLLERR | POLLNVAL | POLLHUP)) {
		log_printf (RTPS_ID, 0, "DTLS: poll () returned error for [%d]. Connection closed.", fd);
		rtps_dtls_shutdown_connection (cxp);
		return;
	}

	dtls_rx_fd_count++;
	if (!cxp->parent) {
		rtps_dtls_complete_pending_cx (cxp, fd);
	}
	if (dtls->state == DTLS_SERVER_RX) {
		rtps_dtls_listen (cxp);
	}
	else if (dtls->state == DTLS_ACCEPT || dtls->state == DTLS_CONNECT) {
		rtps_dtls_connect (cxp);
	}
	else if (dtls->state == DTLS_DATA) {

		/* In DATA state. */
		if (((revents & POLLIN) != 0 && dtls->wstate != SSW_WREAD) ||
				(dtls->rstate == SSR_WWRITE && (revents & POLLOUT) != 0)) {
			rtps_dtls_receive (cxp);
		}
		else if (((revents & POLLOUT) != 0 && dtls->rstate != SSR_WWRITE) ||
				(dtls->wstate == SSW_WREAD && (revents & POLLIN) != 0)) {
			rtps_dtls_send_msgs (cxp);
		}
#ifdef LOG_DTLS_FSM
		else {
			dtls_print ("DTLS: nothing to do!\r\n");
			dtls_fflush ();
		}
#endif
	}
}

static int dtls_verify_callback (int ok, X509_STORE_CTX *store)
{
	char	data[256];
	int	depth;
	X509	*cert;

	depth = X509_STORE_CTX_get_error_depth (store);
	cert = X509_STORE_CTX_get_current_cert (store);
#ifdef LOG_CERT_CHAIN_DETAILS
	if (cert) {
		X509_NAME_oneline (X509_get_subject_name (cert), data, sizeof (data));
		log_printf (RTPS_ID, 0, "DTLS: depth %2i: subject = %s\r\n", depth, data);
		X509_NAME_oneline (X509_get_issuer_name (cert), data, sizeof (data));
		log_printf (RTPS_ID, 0, "DTLS:           issuer  = %s\r\n", data);
	}
#endif

	if (!ok) {
		int	err = X509_STORE_CTX_get_error (store);

		if (cert)
			X509_NAME_oneline (X509_get_subject_name (cert), data, sizeof (data));
		else
			strcpy (data, "<Unknown>");
		err_printf ("err %i @ depth %i for issuer: %s\r\n\t%s\r\n", err, depth, data, X509_verify_cert_error_string (err));
#ifdef NO_CERTIFICATE_TIME_VALIDITY_CHECK
		/* Exceptions */
		if (err == X509_V_ERR_CERT_NOT_YET_VALID) {
#ifdef LOG_CERT_CHAIN_DETAILS
			log_printf (SEC_ID, 0, "DTLS: Certificate verify callback. The certificate is not yet valid, but this is allowed. \r\n");
#endif
			ok = 1;
		}
		if (err == X509_V_ERR_CERT_HAS_EXPIRED) {
			ok = 1;
#ifdef LOG_CERT_CHAIN_DETAILS
			log_printf (SEC_ID, 0, "DTLS: Certificate verify callback. The certificate has expired, but this is allowed. \r\n");
#endif
		}
#endif
	}
	return (ok);
}

static int generate_cookie (SSL *ssl, unsigned char *cookie, unsigned *cookie_len)
{
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength;
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in6 s6;
		struct sockaddr_in s4;
	} peer;

	log_printf (RTPS_ID, 0, "dtls_generate_cookie () ...\r\n");

	/* Initialize a random secret */
	if (!cookie_initialized) {
		if (!RAND_bytes (cookie_secret, COOKIE_SECRET_LENGTH)) {
			fatal_printf ("error setting random cookie secret\n");
			return (0);
		}
		cookie_initialized = 1;
	}

	/* Read peer information */
	(void) BIO_dgram_get_peer (SSL_get_rbio (ssl), &peer);

	/* Create buffer with peer's address and port */
	length = 0;
	switch (peer.ss.ss_family) {
		case AF_INET:
			length += sizeof (struct in_addr);
			break;

		case AF_INET6:
			length += sizeof (struct in6_addr);
			break;

		default:
			OPENSSL_assert (0);
			break;
	}
	length += sizeof (in_port_t);
	buffer = (unsigned char*) OPENSSL_malloc (length);
	if (!buffer)
		fatal_printf ("DDS: generate_cookie (): out of memory!");

	switch (peer.ss.ss_family) {
		case AF_INET:
			memcpy (buffer, &peer.s4.sin_port, sizeof (in_port_t));
			memcpy (buffer + sizeof (in_port_t), &peer.s4.sin_addr, sizeof (struct in_addr));
			break;

		case AF_INET6:
			memcpy (buffer, &peer.s6.sin6_port, sizeof (in_port_t));
			memcpy (buffer + sizeof (in_port_t), &peer.s6.sin6_addr, sizeof (struct in6_addr));
			break;

		default:
			OPENSSL_assert (0);
			break;
	}

	/* Calculate HMAC of buffer using the secret */
	/*sign_with_private_key (NID_sha1, buffer, length,
				 &result [0], &resultlength, local_identity); */	
	HMAC (EVP_sha1 (),
	      (const void *) cookie_secret, COOKIE_SECRET_LENGTH,
	      (const unsigned char*) buffer, length,
	      result, &resultlength);
	
	OPENSSL_free (buffer);
	
	memcpy (cookie, result, resultlength);
	*cookie_len = resultlength;

	return (1);
}

static int verify_cookie (SSL *ssl, unsigned char *cookie, unsigned int cookie_len)
{
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength;
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in6 s6;
		struct sockaddr_in s4;
	} peer;

	log_printf (RTPS_ID, 0, "dtls_verify_cookie () ...\r\n");

	/* If secret isn't initialized yet, the cookie can't be valid */
	if (!cookie_initialized)
		return (0);

	/* Read peer information */
	(void) BIO_dgram_get_peer (SSL_get_rbio (ssl), &peer);

	/* Create buffer with peer's address and port */
	length = 0;
	switch (peer.ss.ss_family) {
		case AF_INET:
			length += sizeof (struct in_addr);
			break;

		case AF_INET6:
			length += sizeof (struct in6_addr);
			break;

		default:
			OPENSSL_assert (0);
			break;
	}
	length += sizeof (in_port_t);
	buffer = (unsigned char*) OPENSSL_malloc (length);
	if (!buffer)
		fatal_printf ("DDS: generate_cookie (): out of memory!");

	switch (peer.ss.ss_family) {
		case AF_INET:
			memcpy (buffer, &peer.s4.sin_port, sizeof (in_port_t));
			memcpy (buffer + sizeof (in_port_t), &peer.s4.sin_addr, sizeof (struct in_addr));
			break;

		case AF_INET6:
			memcpy (buffer, &peer.s6.sin6_port, sizeof (in_port_t));
			memcpy (buffer + sizeof (in_port_t), &peer.s6.sin6_addr, sizeof (struct in6_addr));
			break;

		default:
			OPENSSL_assert (0);
			break;
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC (EVP_sha1 (),
	      (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
	      (const unsigned char*) buffer, length,
	      result, &resultlength);
	OPENSSL_free (buffer);
       
	if (cookie_len == resultlength && memcmp (result, cookie, resultlength) == 0)
		return (1);
	
	/* if (verify_signature (NID_sha1, buffer, length,
				 cookie, cookie_len, local_identity, ssl))
		return (1); */

	return (0);
}

#define DEPTH_CHECK		5

static SSL_CTX *rtps_dtls_create_dtls_ctx (DTLS_CX_SIDE purpose)
{
	const SSL_METHOD	*meth = NULL;
	SSL_CTX			*ctx;

	X509 *cert;
#ifdef DDS_NATIVE_SECURITY
	STACK_OF(X509) *ca_cert_list = NULL;
#else
	X509 *ca_cert_list[10];
#endif
	X509 *ca_cert_ptr = NULL;
	int nbOfCA;
	int j;
	EVP_PKEY *privateKey;
	/* load the certificates through the msecplug */
		
	switch (purpose)
	{
		case DTLS_CLIENT:
			meth = DTLSv1_client_method ();
			break;

		case DTLS_SERVER:
			meth = DTLSv1_server_method ();
			break;

		default:
			fatal_printf ("DTLS - %s () - invalid purpose for new SSL context", __FUNCTION__);
			break;
	}
	if ((ctx = SSL_CTX_new (meth)) == NULL)
		fatal_printf ("DTLS - %s () - Failed to create new SSL context", __FUNCTION__);

	if (!SSL_CTX_set_cipher_list (ctx, "AES:!aNULL:!eNULL"))
		fatal_printf ("DTLS %s (): failed to set cipher list", __FUNCTION__);

	SSL_CTX_set_session_cache_mode (ctx, SSL_SESS_CACHE_OFF);

	/* Put SSL_MODE_RELEASE_BUFFERS to decrease memory usage of DTLS
	   connections. */
	SSL_CTX_set_mode (ctx, SSL_MODE_RELEASE_BUFFERS);

	/* Using SSL_OP_NO_COMPRESSION should also decrease memory usage. */
	SSL_CTX_set_options ( ctx, SSL_OP_NO_COMPRESSION);

	get_certificate (&cert, local_identity);
	if (!SSL_CTX_use_certificate (ctx, cert))
		fatal_printf ("DTLS: no client cert found!");

	/* for the client we have to add the client cert to the trusted ca certs */
	if (purpose == DTLS_CLIENT) {
		/* Add extra cert does not automatically up the reference */
		SSL_CTX_add_extra_chain_cert (ctx, cert);
		cert->references ++;
	}

	get_nb_of_CA_certificates (&nbOfCA, local_identity);
			
	if (nbOfCA == 0)
		fatal_printf("DTLS: Did not find any trusted CA certificates");

#ifdef DDS_NATIVE_SECURITY
	get_CA_certificate_list (&ca_cert_list, local_identity);
#else
	get_CA_certificate_list (&ca_cert_list[0], local_identity);
#endif
			
	for (j = 0; j < nbOfCA ; j++) {
#ifdef DDS_NATIVE_SECURITY
		ca_cert_ptr = sk_X509_value (ca_cert_list, j);
#else
		ca_cert_ptr = ca_cert_list [j];
#endif
		X509_STORE_add_cert (SSL_CTX_get_cert_store (ctx), ca_cert_ptr);
	}
	
	get_private_key (&privateKey, local_identity);
	if (!SSL_CTX_use_PrivateKey (ctx, privateKey))
		fatal_printf ("DTLS: no private key found!");
       
	SSL_CTX_set_verify (ctx,
			    SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			    dtls_verify_callback);	
			
	/*Check the private key*/
	if (!SSL_CTX_check_private_key (ctx))
		fatal_printf ("DTLS: invalid private key!");

	SSL_CTX_set_verify_depth (ctx, DEPTH_CHECK);
	SSL_CTX_set_read_ahead (ctx, 1);
	
	if (purpose == DTLS_SERVER) {
		SSL_CTX_set_cookie_generate_cb (ctx, generate_cookie);
		SSL_CTX_set_cookie_verify_cb (ctx, verify_cookie);
	}

	rtps_dtls_trace_openssl_fsm (ctx);

#ifdef DDS_NATIVE_SECURITY
	X509_free (cert);
	sk_X509_pop_free (ca_cert_list, X509_free);
	EVP_PKEY_free (privateKey);
#endif
	return (ctx);
}

static void rtps_dtls_ctx_init (void)
{
	log_printf (RTPS_ID, 0, "DTLS: Contexts initialized\r\n");
	dds_ssl_init ();

	dtls_server_ctx = rtps_dtls_create_dtls_ctx (DTLS_SERVER);
	dtls_client_ctx = rtps_dtls_create_dtls_ctx (DTLS_CLIENT);
}

static void rtps_dtls_ctx_finish (void)
{
	SSL_CTX_free (dtls_client_ctx);
	SSL_CTX_free (dtls_server_ctx);
	dtls_client_ctx = dtls_server_ctx = NULL;

	dds_ssl_finish ();
	log_printf (RTPS_ID, 0, "DTLS: Contexts freed\r\n");
}

#if 0
static void rtps_udp_remove_dtls_server (IP_CX *cxp)
{
	dtls_print2 ("DTLS(%u): Removing DTLS.S on [%d].\r\n", cxp->handle, cxp->fd);
	sock_fd_remove_socket (cxp->fd);
}
#endif

static DTLS_CX* rtps_dtls_new_dtls_cx (IP_CX *cxp, DTLS_CX_SIDE side)
{
	DTLS_CX		*dtls;
	BIO		*bio;
	char		sa_buf[SA_LENGTH];
	socklen_t	sa_len = SA_LENGTH;

	dtls = xmalloc (sizeof (DTLS_CX));
	if (!dtls) {
		err_printf ("%s (): out of memory for DTLS context!", __FUNCTION__);
		return (NULL);
	}
	if ((dtls->ssl = SSL_new ((side == DTLS_SERVER) ? dtls_server_ctx : dtls_client_ctx)) == NULL) {
		err_printf ("%s (): failed to alloc SSL connection context", __FUNCTION__);
		goto exit_failure;
	}
	if ((bio = BIO_new_dgram (cxp->fd, BIO_NOCLOSE)) == NULL) {
		err_printf ("%s (): failed to alloc BIO", __FUNCTION__);
		goto exit_failure2;
	}
	getpeername (cxp->fd, (struct sockaddr *) sa_buf, &sa_len);
	(void) BIO_ctrl_set_connected (bio, 0, (struct sockaddr *) sa_buf);
	SSL_set_bio (dtls->ssl, bio, bio);

	dtls->state = (side == DTLS_SERVER ? DTLS_SERVER_RX : DTLS_CONNECT);
	dtls->wstate = SSW_IDLE;
	dtls->rstate = SSR_IDLE;
	dtls->rcvd_msg_cnt = dtls->sent_msg_cnt = 0;

	return (dtls);

    exit_failure2:
	SSL_free (dtls->ssl);
    exit_failure:
	xfree (dtls);

	return (NULL);
}

/* rtps_dtls_new_ip_cx -- Allocate and prepare the context which represents a
			  connection locator: contains address/port of remote
			  party. */

static IP_CX *rtps_dtls_new_ip_cx (unsigned id, Locator_t *locator, DTLS_CX_SIDE side)
{
	IP_CX			*cxp;
	struct sockaddr_in	our_addr;	/* Client IP address. */
	struct sockaddr_in	remote_addr;	/* Server IP address. */
#ifdef DDS_IPV6
	struct sockaddr_in6	our_addr_v6;	/* Client IPv6 address. */
	struct sockaddr_in6	remote_addr_v6;	/* Server IPv6 address. */
	char			buf[128];
#endif
	struct sockaddr		*our_sa;
	struct sockaddr		*remote_sa;

	cxp = rtps_ip_alloc ();
	if (!cxp) {
		err_printf ("%s (): out of IP connection contexts!", __FUNCTION__);
		return (NULL);
	}
	cxp->locator = xmalloc (sizeof (LocatorNode_t));
	if (!cxp->locator) {
		err_printf ("%s (): out of memory for locator context!", __FUNCTION__);
		goto nomem_loc;
	}

	/* Set our address parameters */
	memset (cxp->locator, 0, sizeof (LocatorNode_t));
	cxp->locator->users = 0;
	cxp->locator->locator.kind = locator->kind;

	/* cxp->locator->locator.address is implicitly kept at all zeroes, representing INADDR_ANY
	 * (or its IPv6 equivalent). It will be filled in as soon as the first message on this
	 * connection is received. */
	cxp->locator->locator.port = dtls_server_port;
	cxp->locator->locator.flags = LOCF_DATA | LOCF_META | LOCF_UCAST | LOCF_SECURE;
	if (side == DTLS_SERVER)
		cxp->locator->locator.flags |= LOCF_SERVER;
	cxp->locator->locator.sproto = SECC_DTLS_UDP;

	/* Set remote address parameters */
	memcpy (cxp->dst_addr, locator->address, 16);
	cxp->dst_port = locator->port;
	cxp->has_dst_addr = 1;
	cxp->associated = 1;
	cxp->id = id;
	cxp->cx_type = CXT_UDP_DTLS;
	cxp->cx_mode = ICM_DATA;
	cxp->cx_side = (side == DTLS_SERVER) ? ICS_SERVER : ICS_CLIENT;
	cxp->cx_state = CXS_CLOSED;
	cxp->p_state = TDS_WCXOK;

	if ((cxp->timer = tmr_alloc ()) == NULL) {
		err_printf ("%s (): out of memory for timer!", __FUNCTION__);
		goto nomem_timer;
	}
	tmr_init (cxp->timer, (side == DTLS_SERVER) ? "DTLS.H" : "DTLS.C");

	if (locator->kind == LOCATOR_KIND_UDPv4) {
		/* Setup our socket address */
		memset (&our_addr, 0, sizeof (our_addr));
		our_addr.sin_family = AF_INET;
		our_addr.sin_port = htons (dtls_server_port);
		our_addr.sin_addr.s_addr = INADDR_ANY;
		our_sa = (struct sockaddr *) &our_addr;

		/* Setup remote socket address. */
		memset (&remote_addr, 0, sizeof (remote_addr));
		remote_addr.sin_family = AF_INET;
		remote_addr.sin_port = htons (cxp->dst_port);
		remote_addr.sin_addr.s_addr = ntohl ((cxp->dst_addr [12] << 24) |
					             (cxp->dst_addr [13] << 16) |
					             (cxp->dst_addr [14] << 8) |
					              cxp->dst_addr [15]);
		remote_sa = (struct sockaddr *) &remote_addr;
	}
#ifdef DDS_IPV6
	else if (locator->kind == LOCATOR_KIND_UDPv6) {

		/* Setup client socket address. */
		memset (&our_addr_v6, 0, sizeof (our_addr_v6));
		our_addr_v6.sin6_family = AF_INET6;
		our_addr_v6.sin6_port = htons (dtls_server_port);
		our_addr_v6.sin6_addr = in6addr_any;
		our_sa = (struct sockaddr *) &our_addr_v6;

		/* Setup server socket address. */
		memset (&remote_addr_v6, 0, sizeof (remote_addr_v6));
		remote_addr_v6.sin6_family = AF_INET6;
		remote_addr_v6.sin6_port = htons (cxp->dst_port);
		memcpy (&remote_addr_v6.sin6_addr, cxp->dst_addr, 16);
		remote_sa = (struct sockaddr *) &remote_addr_v6;

		log_printf (RTPS_ID, 0, "%s:%u)\r\n", 
				inet_ntop (AF_INET6, &cxp->dst_addr, buf, sizeof (buf)),
				ntohs (cxp->dst_port));
	}
#endif
	else {
		warn_printf ("%s (): unknown address family!", __FUNCTION__);
		goto invalid_address_family;
	}

	cxp->fd = rtps_dtls_socket (our_sa, remote_sa);
	if (cxp->fd < 0) {
		goto no_socket;
	}
	if (set_socket_nonblocking (cxp->fd))
		warn_printf ("%s (): can't set fd to non-blocking!", __FUNCTION__);
	cxp->fd_owner = 1;

	rtps_ip_new_handle (cxp);
	cxp->locator->locator.handle = locator->handle = cxp->handle;

	cxp->next = dtls_pending_cx;
	dtls_pending_cx = cxp;

	return (cxp);

    no_socket:
    invalid_address_family:
	tmr_free (cxp->timer);
    nomem_timer:
	xfree (cxp->locator);
    nomem_loc:
	rtps_ip_free (cxp);

	return (NULL);
}

/* locator_cmp -- Determine the 'smallest' locator, based on their address and
		  port. Return <0, ==0, >0 whether a < b, a == b, a > b
		 respectively. */

static int locator_cmp (Locator_t *a, Locator_t *b)
{
	int	r;

	r = memcmp (a->address, b->address, sizeof (a->address));
	if (!r)
		r = a->port - b->port;

	return (r);
}

/* dtls_locator -- Check if a locator is a secure DTLS locator. */

#define	dtls_locator(lp) (((lp)->kind & (LOCATOR_KIND_UDPv4 |		\
					 LOCATOR_KIND_UDPv6)) != 0 &&	\
			  ((lp)->sproto & SECC_DTLS_UDP) != 0)

/* dtls_connection_role -- Uniquely determines whether we should play client or 
			   server in the connection to the given peer locator.
			   We compare the order of our locator and their
			   locator to determine the role. */

static DTLS_CX_SIDE dtls_connection_role (Locator_t *lp, LocatorList_t *next)
{
	IP_CX		*scx;
	LocatorRef_t	*rp;
	LocatorNode_t	*np;
	Locator_t	*our_loc = NULL, *their_loc = NULL;

	/* Determine their reference (ie 'smallest') locator. */
	their_loc = lp;
	if (lp->flags & LOCF_FCLIENT)
		return (DTLS_CLIENT);

	if (next)
		foreach_locator (*next, rp, np) {
			if (!dtls_locator (&np->locator))
				break;

			if (np->locator.flags & LOCF_FCLIENT)
				return (DTLS_CLIENT);

			if (locator_cmp (&np->locator, their_loc) < 0)
				their_loc = &np->locator;
		}

	/* Determine our reference (ie 'smallest') locator. */
	if (lp->kind == LOCATOR_KIND_UDPv4)
		if (!dtls_v4_servers)
			return (DTLS_ROLE_ERR);
		else
			scx = dtls_v4_servers;
#ifdef DDS_IPV6
	else if (lp->kind == LOCATOR_KIND_UDPv6)
		if (!dtls_v6_servers)
			return (DTLS_ROLE_ERR);
		else
			scx = dtls_v6_servers;
#endif
	else
		return (DTLS_ROLE_ERR);

	if (!scx)
		return (DTLS_ROLE_ERR);

	our_loc = &scx->locator->locator;
	for (scx = scx->next; scx; scx = scx->next)
		if (locator_cmp (&scx->locator->locator, our_loc) < 0)
			our_loc = &scx->locator->locator;

	return (locator_cmp (our_loc, their_loc) < 0 ? DTLS_SERVER : DTLS_CLIENT);
}

/* rtps_dtls_setup -- Setup connectivity with a new DTLS-based participant. */

static IP_CX *rtps_dtls_setup (unsigned id, Locator_t *locator, DTLS_CX_SIDE role)
{
	IP_CX	*cxp;

	/*
	 * No matter what role we play in this connection attempt, in a next
	 * connection attempt, we will play the role of (DTLS) client. See
	 * dtls_connection_role ().
	 */
	locator->flags |= LOCF_FCLIENT;

	cxp = rtps_dtls_new_ip_cx (id, locator, role);
	if (!cxp)
		return (NULL);

	cxp->sproto = rtps_dtls_new_dtls_cx (cxp, role);
	if (!cxp->sproto)
		goto exit_failure;

	sock_fd_add_socket (cxp->fd, POLLIN | POLLPRI | POLLOUT, rtps_dtls_rx_fd, cxp, 
				(role == DTLS_SERVER) ? "DDS.DTLS-S" : "DDS.DTLS-C");
	rtps_dtls_start_idle_cx_timer (cxp);
	log_printf (RTPS_ID, 0, "DTLS(%u): new connection on [%d] (%s) ",
		    cxp->handle, cxp->fd, role == DTLS_SERVER ? "server" : role == DTLS_CLIENT ? "client" : "<invalid>");
#ifdef DDS_DEBUG
	log_printf (RTPS_ID, 0, "%s", locator_str (locator));
#endif
	log_printf (RTPS_ID, 0, "\r\n");
	trace_connection_changes ();
	return (cxp);

    exit_failure:
	rtps_dtls_shutdown_connection (cxp);
	return (NULL);
}

/* rtps_dtls_search_cx -- Search for an existing IP_CX, based on the given
			  locator. */

static IP_CX *rtps_dtls_search_cx (unsigned id, Locator_t *lp)
{
	IP_CX	*cxp = NULL;

#ifdef LOG_SEARCH_CTX
	log_printf (RTPS_ID, 0, "DTLS: search ctx for locator ");
#ifdef DDS_DEBUG
	log_printf (RTPS_ID, 0, "%s", locator_str (lp));
#endif
	log_printf (RTPS_ID, 0, " (%u):\r\n\t", lp->handle);
#endif
#if 0
	if (lp->handle) {
		cxp = rtps_ip_from_handle (lp->handle);
#ifdef LOG_SEARCH_CTX
		if (cxp)
			log_printf (RTPS_ID, 0, "found via locator handle.");
#endif
	}
#endif
	if (!cxp && ((cxp = rtps_ip_lookup_peer (id, lp)) != NULL)) {
		lp->handle = cxp->handle;
#ifdef LOG_SEARCH_CTX
		log_printf (RTPS_ID, 0, "found via peer.");
	}
	else if (!cxp) {
		log_printf (RTPS_ID, 0, "not found");
#endif
	}
#ifdef LOG_SEARCH_CTX
	log_printf (RTPS_ID, 0, "(%p)\r\n", (void *) cxp);
#endif
	return (cxp); /* Note: can still be NULL! */
}

/* rtps_dtls_enqueue_msgs -- Enqueue msgs to a IP_CX.  Returns 1 on success; 0
			     when no related IP_CX was found. */

static void rtps_dtls_enqueue_msgs (IP_CX *cxp, RMBUF *msgs)
{
	RMREF	*rp;
	RMBUF	*mp;

	for (mp = msgs; mp; mp = mp->next) {
		rp = rtps_ref_message (mp);
		if (!rp)
			return;

		if (cxp->head)
			cxp->tail->next = rp;
		else
			cxp->head = rp;
		cxp->tail = rp;
	}
}

/* rtps_dtls_server_rx_fd -- Function that attracts all traffic for which we do not (yet?)
			     have a dedicated socket available. A new context is
			     created as needed. */

static void rtps_dtls_server_rx_fd (SOCKET fd, short revents, void *arg)
{
	char			buf [256];
	IP_CX			*client_cxp, *cxp = (IP_CX *) arg;
	Locator_t		loc;
	struct sockaddr_storage sa;
	socklen_t		sa_len = sizeof (sa);

	ARG_NOT_USED (revents)

	dtls_dump_poll_events (fd, revents);
	dtls_server_fd_count++;

	/*
	 * The message content will be ignored. The src address of the packet is used to setup
	 * a new connection. The DTLS retransmit mechanisms will make sure that subsequent
	 * messages will arrive at the new context.
	 */
	if (recvfrom (fd, buf, sizeof (buf), MSG_DONTWAIT, (struct sockaddr *)&sa, &sa_len) == -1)
		return;

	if (!sockaddr2loc (&loc, (struct sockaddr *) &sa))
		return;

	loc.handle = 0;
	loc.flags = 0;
	if ((client_cxp = rtps_dtls_search_cx (cxp->id, &loc)) != NULL) {
		log_printf (RTPS_ID, 0, "DTLS(%u): ignoring message on [%d], state:%s\r\n",
				client_cxp->handle, client_cxp->fd,
				dtls_cx_state_str [client_cxp->cx_state]);
		return; /* a subsequent message will be received by its true destination - ignore this one */
	}

	log_printf (RTPS_ID, 0, "DTLS: Creating new DTLS.H from server socket [%d]\r\n", fd);
	rtps_dtls_setup (cxp->id, &loc, DTLS_SERVER);

	return;
}

static void rtps_udp_add_dtls_server (IP_CX *cxp)
{
	if (!dtls_client_ctx)
		rtps_dtls_ctx_init ();

	if (set_socket_nonblocking (cxp->fd))
		warn_printf ("rtps_udp_add_dtls_server: can't set non-blocking!");

	sock_fd_add_socket (cxp->fd, POLLIN | POLLPRI, rtps_dtls_server_rx_fd, cxp, "DDS.DTLS-S");

	log_printf (RTPS_ID, 0, "DTLS: Server started on [%d].\r\n", cxp->fd);
}

/* rtps_dtls_send -- Send messages to the given set of secure locators. */

void rtps_dtls_send (unsigned id, Locator_t *lp, LocatorList_t *listp, RMBUF *msgs)
{
	IP_CX		*ucp;
	DTLS_CX		*dtls;
	DTLS_CX_SIDE	role;	/* Role if we need a new connection(s) */

	/* Get the connection role, i.e. whether we are client or server. */
	if ((lp->flags & LOCF_FCLIENT) != 0)
		role = DTLS_CLIENT;
	else {
		role = dtls_connection_role (lp, listp);
		if (role == DTLS_ROLE_ERR) {
			log_printf (RTPS_ID, 0, "DTLS: Could not determine role -- message dropped!\r\n");
			rtps_free_messages (msgs);
			return;
		}
	}

	/* Send all messages to each destination locator. */
	for (;;) {
		ucp = rtps_dtls_search_cx (id, lp);
		if (!ucp)
			ucp = rtps_dtls_setup (id, lp, role);
		if (ucp) {

			/* Enqueue messages into context. */
			rtps_dtls_enqueue_msgs (ucp, msgs);

			/* Try to send messages immediately if ready. */
			dtls = ucp->sproto;
			if (dtls &&
			    dtls->state == DTLS_DATA &&
			    dtls->wstate == SSW_IDLE)
				rtps_dtls_send_msgs (ucp);
		}

		/* If multiple destination locators, take the next one. */
		if (listp && *listp) {
			lp = &(*listp)->data->locator;
			if (!dtls_locator (lp))
				break;

			*listp = (*listp)->next;
		}
		else
			break;
	}
}

void rtps_dtls_attach_server (IP_CX *cxp)
{
	locator_lock (&cxp->locator->locator);
	cxp->locator->locator.flags |= LOCF_SERVER;
	locator_release (&cxp->locator->locator);
	cxp->cx_type = CXT_UDP_DTLS;
	cxp->cx_mode = ICM_ROOT;
	cxp->cx_side = ICS_SERVER;
	cxp->cx_state = CXS_CLOSED;
	cxp->p_state = TDS_WCXOK;
	trace_connection_changes ();
	rtps_udp_add_dtls_server (cxp);
	cxp->parent = cxp; /* parent pointing to itself means we're dealing with a DTLS.S */
	if (cxp->locator->locator.kind == LOCATOR_KIND_UDPv4) {
		cxp->next = dtls_v4_servers;
		dtls_v4_servers = cxp;
	}
#ifdef DDS_IPV6
	else if (cxp->locator->locator.kind == LOCATOR_KIND_UDPv6) {
		cxp->next = dtls_v6_servers;
		dtls_v6_servers = cxp;
	}
#endif
	if (dtls_server_port != cxp->locator->locator.port) {
		log_printf (RTPS_ID, 0, "DTLS: Changing server port from %d to %d\r\n", dtls_server_port, cxp->locator->locator.port);
		dtls_server_port = cxp->locator->locator.port;
	}
}

void rtps_dtls_detach_server (IP_CX *cxp)
{
	ARG_NOT_USED (cxp);
	/* TODO - inverse of rtps_dtls_attach_server () above
	 * 1- close all client connections
	 * 2- remove dtls server from list of servers
	 * 3- set locator to non-secure
	 * 4- ....
	 */
}

void rtps_dtls_cleanup_cx (IP_CX *cxp)
{
	DTLS_CX	*dtls = cxp->sproto;

	IP_CX *scx, **sl;

	scx = cxp->parent;
	if (scx == cxp) { /* DTLS.S : cleanup all connections derived from or linked to this connection */
		sl = &dtls_v4_servers;
#ifdef DDS_IPV6
		if (cxp->locator->locator.kind == LOCATOR_KIND_UDPv6)
			sl = &dtls_v6_servers;
#endif
		log_printf (RTPS_ID, 0, "DTLS(%u): cleaning up server on [%d]\r\n", cxp->handle, cxp->fd);
		if (remove_from_ip_cx_list (sl, scx))
			log_printf (RTPS_ID, 0, "%s(): could not find dtls server %p in list of dtls servers\r\n", __FUNCTION__, (void *) cxp);

		while (scx->clients)
			rtps_dtls_shutdown_connection (scx->clients);
		return;
	}
	else if (scx) { /* DTLS.{C,H} : remove this connection from the list of sibling connections */
		log_printf (RTPS_ID, 0, "DTLS(%u): cleaning up connection on [%d]\r\n", cxp->handle, cxp->fd);
		if (remove_from_ip_cx_list (&scx->clients, cxp)) {
			dtls_print ("DTLS: Failed to find back connection in list of sibling connections\r\n");
		}
	}
	else { /* DTLS.{C,H}: remove this connection from list of pending connections */
		log_printf (RTPS_ID, 0, "DTLS(%u): cleaning up pending connection on [%d]\r\n", cxp->handle, cxp->fd);
		if (remove_from_ip_cx_list (&dtls_pending_cx, cxp)) {
			dtls_print ("DTLS: Failed to find back connection in list of pending connections\r\n");
		}
	}

	/*
	 * Be nice to the other side and shutdown the DTLS connection over the wire.
	 * However, we do not care whether we successfully sent the "close notify"
	 * over the wire. In the end, the other side will see that we are not sending
	 * them any messages anymore and it will close its side of the connection in 
	 * a similar fashion.
	 */
	if (dtls) {
		int error;
		socklen_t err_len = sizeof (error);
		if (getsockopt (cxp->fd, SOL_SOCKET, SO_ERROR, &error, &err_len) && (error != 0)) {
			dtls_print2 ("DTLS(%u): Not sending shutdown on [%d] because socket is in error state.\r\n", cxp->handle, cxp->fd);
		}
		else {
			dtls_print2 ("DTLS(%u): Sending shutdown on [%d].\r\n", cxp->handle, cxp->fd);
			SSL_shutdown (dtls->ssl);
		}
		SSL_free (dtls->ssl); /* this also frees the used BIO */
		xfree (dtls);
	}

	/* Free all pending messages */
	rtps_unref_messages (cxp->head);
	cxp->head = cxp->tail = NULL;

	if (cxp->timer) {
		tmr_stop (cxp->timer);
		tmr_free (cxp->timer);
	}
}

void rtps_dtls_final (void)
{
	while (dtls_pending_cx)
		rtps_dtls_shutdown_connection (dtls_pending_cx);
	while (dtls_v4_servers)
		rtps_dtls_shutdown_connection (dtls_v4_servers);
#ifdef DDS_IPV6
	while (dtls_v6_servers)
		rtps_dtls_shutdown_connection (dtls_v6_servers);
#endif
	rtps_dtls_ctx_finish ();
}

/*
 * Dumping data and statistics related to our DTLS servers and connections
 */

static void rtps_dtls_dump_cx_list (IP_CX *head)
{
	IP_CX		*cxp;
	Locator_t	loc;
	const char	*buf;

	loc.sproto = SECC_DTLS_UDP;
	if (head)
		for (cxp = head; cxp; cxp = cxp->next) {
			loc.kind = cxp->locator->locator.kind;
			loc.port = cxp->dst_port;
			loc.flags = LOCF_SECURE | ((cxp->cx_side == ICS_SERVER) ? LOCF_SERVER : 0);
			memcpy (loc.address, cxp->dst_addr, sizeof (loc.address));
			buf = locator_str (&loc);
			log_printf (RTPS_ID, 0, "\tDTLS.%c(%u) to %s [%d] state: %s p:%p\r\n",
					(cxp->cx_side == ICS_SERVER) ? 'H' : 'C',
					cxp->handle,
					buf,
					cxp->fd,
					dtls_cx_state_str [cxp->cx_state],
					(void *) cxp);
		}
	else
		log_printf (RTPS_ID, 0, "\t<No connections>\r\n");
}

static void rtps_dtls_dump_sx_list (IP_CX *head)
{
	IP_CX		*sxp;
	const char	*buf;

	if (head)
		for (sxp = head; sxp; sxp = sxp->next) {
			buf = locator_str (&sxp->locator->locator);
			log_printf (RTPS_ID, 0, "    DTLS.S(%u) %s [%d] p:%p\r\n", sxp->handle, buf, sxp->fd, (void *) sxp);
			rtps_dtls_dump_cx_list (sxp->clients);
		}
	else
		log_printf (RTPS_ID, 0, "    <None>\r\n");
}

void rtps_dtls_dump (void)
{
	dbg_printf ("DTLS connection attempts:\r\n");
	rtps_dtls_dump_cx_list (dtls_pending_cx);

	dbg_printf ("DTLS v4 Servers:\r\n");
	rtps_dtls_dump_sx_list (dtls_v4_servers);

#ifdef DDS_IPV6
	dbg_printf ("DTLS v6 Servers:\r\n");
	rtps_dtls_dump_sx_list (dtls_v6_servers);
#endif
}

#else
int	dtls_available = 0;
#endif

