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

/* ri_tcp_sock.c -- Implement the (plain) TCP version of the STREAM_API */

#ifdef DDS_TCP

#include <errno.h>
#define ERRNO errno
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "debug.h"
#include "error.h"
#include "log.h"
#include "sock.h"
#include "ri_data.h"
#include "ri_tcp.h"
#include "ri_tcp_sock.h"

/*#define TCP_ACCEPT_DELAY_MS	1000	** Defined: used as accept() delay. */

#ifdef DDS_DEBUG
/*#define TRACE_CON_EVENTS	** Trace client connect() events */
/*#define TRACE_POLL_EVENTS	** Trace poll events */
/*#define TRACE_READ_OPS	** Trace read() calls */
/*#define TRACE_WRITE_OPS	** Trace write()/send() calls */
/*#define TRACE_SERVER		** Trace server side operations (listen, accept ...) */
/*#define TRACE_CLIENT		** Trace client side operations (connect, ...) */
#endif

typedef struct tcp_data_st {
	TCP_MSG	*recv_msg;
	TCP_MSG	*send_msg;
} TCP_DATA;

static TCP_CON_LIST_ST *tcp_cx_pending;

#ifdef TRACE_POLL_EVENTS
#define trace_poll_events(fd,events,arg) log_printf (RTPS_ID, 0, "tcp-poll [%d] %s: revents=%s arg=%p\r\n", (fd), __FUNCTION__, dbg_poll_event_str ((events)), (arg))
#else
#define trace_poll_events(fd,events,arg) ARG_NOT_USED ((fd)) ARG_NOT_USED ((events)) ARG_NOT_USED ((arg))
#endif

#if defined (TRACE_READ_OPS) || defined (TRACE_WRITE_OPS) || defined (TRACE_SERVER) || defined (TRACE_CLIENT)
static void trace_log_errno (int res, int err)
{
	if (res == -1)
		log_printf (RTPS_ID, 0, " e:%d %s", errno, strerror (err));
}
#endif

#if defined (TRACE_READ_OPS) || defined (TRACE_WRITE_OPS)
static void trace_rw (const char *rw, ssize_t res,int fd, size_t sz) 
{
	int saved = ERRNO;

	log_printf (RTPS_ID, 0, "tcp-%s [%d] s:%ld r:%ld", rw, fd, (long) sz, (long) res);
	trace_log_errno (res, saved);
	log_printf (RTPS_ID, 0, "\r\n");
	ERRNO = saved;
}
#endif

#if defined (TRACE_SERVER) || defined (TRACE_CLIENT)
static void trace_call (const char *op, int res, int fd)
{
	int saved = ERRNO;

	log_printf (RTPS_ID, 0, "tcp-%s [%d] r:%d", op, fd, res);
	trace_log_errno (res, saved);
	log_printf (RTPS_ID, 0, "\r\n");
	ERRNO = saved;
}
#endif

#ifdef TRACE_SERVER
#define trace_server(op,res,fd) trace_call ((op), (res), (fd))
#else
#define trace_server(op,res,fd)
#endif

#ifdef TRACE_CLIENT
#define trace_client(op,res,fd) trace_call ((op), (res), (fd))
#else
#define trace_client(op,res,fd)
#endif

#ifdef TRACE_CLIENT
#endif

#ifdef TRACE_READ_OPS
#define trace_read(res,fd,sz) trace_rw ("read", (res), (fd), (sz))
#else
#define trace_read(res,fd,sz)
#endif

#ifdef TRACE_WRITE_OPS
#define trace_write(res,fd,sz) trace_rw ("write", (res), (fd), (sz))
#else
#define trace_write(res,fd,sz)
#endif

static void tcp_cleanup_ctx (IP_CX *cxp)
{
	sock_fd_remove_socket (cxp->fd);
	cxp->cx_state = CXS_CLOSED;
	cxp->stream_cb->on_close (cxp); /* This fd most probably became unusable. */
}

/* Write/send (the next part of) a msg (fragment). */
static void tcp_write_message_fragment (IP_CX *cxp)
{
	ssize_t		n;
	unsigned char	*sp;
	TCP_DATA	*dp;

	if ((dp = cxp->sproto) == NULL) {
		warn_printf ("tcp_write_message_fragment: no TCP context!");
		return;
	}
	if (!dp->send_msg) {
		warn_printf ("tcp_write_message_fragment: no send_msg context!");
#ifdef TRACE_POLL_EVENTS
			log_printf (RTPS_ID, 0, "TLS: POLLOUT = 0 [%d]\r\n", cxp->fd);
#endif
			sock_fd_event_socket (cxp->fd, POLLOUT, 0);
		return;
	}
	sp = dp->send_msg->buffer + dp->send_msg->used;
	while (dp->send_msg->used < dp->send_msg->size) {

#ifdef __APPLE__
		/* Apple does not support MSG_NOSIGNAL, therefore we set the equivalent Apple-specific 
		   socket option SO_NOSIGPIPE. */
		n = send (cxp->fd, sp, dp->send_msg->size - dp->send_msg->used, 0);
#else
		/* We're using send() here iso write() so that we can indicate to the kernel *not* to send
		   a SIGPIPE in case the other already closed this connection. A return code of -1 and ERRNO
		   to EPIPE is given instead */
		n = send (cxp->fd, sp, dp->send_msg->size - dp->send_msg->used, MSG_NOSIGNAL);
#endif
		trace_write (n, cxp->fd, dp->send_msg->size - dp->send_msg->used);
#ifdef MSG_TRACE
		if (cxp->trace)
			rtps_ip_trace (cxp->handle, 'T',
				       &cxp->locator->locator,
				       cxp->dst_addr, cxp->dst_port,
				       dp->send_msg->size - dp->send_msg->used);
#endif
		if (n < 0) {
			if (ERRNO == EINTR)
				continue; /* immediately try again */

			if ((ERRNO == EAGAIN) || (ERRNO == EWOULDBLOCK))
				return; /* Wait for poll() to indicate that we can write another chunk. */

			perror ("tcp_write_message_fragment");
			log_printf (RTPS_ID, 0, "%s: error sending data on [%d] (%s).\r\n",
					__FUNCTION__, cxp->fd, strerror (ERRNO));
			xfree (dp->send_msg->buffer);
			xfree (dp->send_msg);
			dp->send_msg = NULL;
			tcp_cleanup_ctx (cxp);
			return;
		}
		dp->send_msg->used += n;
		sp += n;
	}

	xfree (dp->send_msg->buffer);
	xfree (dp->send_msg);
	dp->send_msg = NULL;
#ifdef TRACE_POLL_EVENTS
	log_printf (RTPS_ID, 0, "TLS: POLLOUT = 0 [%d]\r\n", cxp->fd);
#endif
	sock_fd_event_socket (cxp->fd, POLLOUT, 0);

	if (cxp->stream_cb->on_write_completed)
		cxp->stream_cb->on_write_completed (cxp);
	else if (cxp->paired &&
		 cxp->paired->fd == cxp->fd &&
		 cxp->paired->stream_cb->on_write_completed)
		cxp->paired->stream_cb->on_write_completed (cxp);
}

/* Receive (the next part of) a msg (fragment).
   A message is complete when msg->read == msg->size; the data is then available in msg->buffer.
   It is assumed that fd is set to nonblocking beforehand.
   Returns:
     0: success (the read(s) went well, msg not necessarilly complete).
    -1: Some problem with read (see ERRNO for details). Connection should most
        probably be closed. */
static int tcp_receive_message_fragment (int fd, TCP_MSG *msg)
{
	unsigned char	*dp;
	ssize_t		n;
	uint32_t	l;

	if (!msg->size) {
		/* Start of a new message. Expect at least a message header */
		msg->used = 0;
		msg->size = sizeof (CtrlHeader);
	}
	if (msg->used < sizeof (CtrlHeader))
		/* (Still) reading the msg header */
		dp = (unsigned char*) &msg->header + msg->used;
	else
		/* Continue reading 'til the end of the message */
		dp = msg->buffer + msg->used;
	
    continue_reading:

	while (msg->used < msg->size) {
		n = read (fd, dp, msg->size - msg->used);
		trace_read (n, fd, msg->size - msg->used);
		if (n < 0) {
			if (ERRNO == EINTR)
				goto continue_reading;
			if ((ERRNO == EAGAIN) || (ERRNO == EWOULDBLOCK))
				return (0); /* try again later */
			perror ("tcp_receive_message_fragment()");
			log_printf (RTPS_ID, 0, "TCP: error reading from connection [%d] (%s)!\r\n", fd, strerror (ERRNO));
			return (-1);
		}
		else if (n == 0)
			return (-1); /* end-of-stream encountered */

		msg->used += n;
		dp += n;
	}
	if (!msg->buffer) {
		/* Just received a CtrlHeader - see what else is needed? */
		if (ctrl_protocol_valid (&msg->header))
			msg->size += msg->header.length;
		else if (protocol_valid (&msg->header)) {
			memcpy (&l, &msg->header.msg_kind, sizeof (l));
			msg->size += l;
		}
		else {
			/* Data not recognized as either a RTPS nor a RPSC message */
			msg->size = msg->used = 0;
			return (-1);
		}
		msg->buffer = xmalloc (msg->size);
		if (!msg->buffer) {
			msg->size = msg->used = 0;
			return (-1);
		}
		memcpy (msg->buffer, &msg->header, sizeof (CtrlHeader));
		dp = msg->buffer + msg->used;
		goto continue_reading;
	}
	return (0);
}

static void tcp_receive_message (IP_CX *cxp)
{
	TCP_MSG		*recv_msg;
	TCP_DATA	*dp;

	if ((dp = cxp->sproto) == NULL) {
		warn_printf ("tcp_receive_message: no TCP context!");
		return;
	}
	if (!dp->recv_msg) {

		/* Prepare for receiving messages */
		dp->recv_msg = xmalloc (sizeof (TCP_MSG));
		if (!dp->recv_msg) {
			tcp_cleanup_ctx (cxp);
			return;
		}
		dp->recv_msg->size = dp->recv_msg->used = 0;
		dp->recv_msg->buffer = NULL;
	}

	if (tcp_receive_message_fragment (cxp->fd, dp->recv_msg) == -1) {
		tcp_cleanup_ctx (cxp);
		return;
	}
	if (dp->recv_msg->used == dp->recv_msg->size) {
		recv_msg = dp->recv_msg;
		dp->recv_msg = NULL;
		cxp->stream_cb->on_new_message (cxp, recv_msg->buffer, recv_msg->size);
		xfree (recv_msg->buffer);
		xfree (recv_msg);
	}
}

static void tcp_socket_activity (SOCKET fd, short revents, void *arg)
{
	IP_CX		*cxp = (IP_CX *) arg;
	int		err, r;
	socklen_t	sz;
	int		handle;

	trace_poll_events (fd, revents, arg);
# if 0
	if (!cxp->fd_owner)
		log_printf (RTPS_ID, 0, "Socket is not the fd_owner [%d] cxp:%p(%d) paired:%p(%d) \r\n",
				fd,
				(void *) cxp, cxp->fd_owner,
				(void *) cxp->paired, cxp->paired ? cxp->paired->fd_owner : 99999);
# endif
	if ((revents & (POLLERR | POLLNVAL)) != 0) {
		sz = sizeof (err);
		r = getsockopt(cxp->fd, SOL_SOCKET, SO_ERROR, &err, &sz);
		if ((r == -1) || err)  {
			log_printf (RTPS_ID, 0, "POLLERR | POLLNVAL [%d]: %d %s\r\n", cxp->fd, err, strerror (err));
			tcp_cleanup_ctx (cxp);
			return;
		}
	}
	if ((revents & POLLHUP) != 0) {
		tcp_cleanup_ctx (cxp);
		return;
	}
	if ((revents & POLLOUT) != 0) {
		handle = cxp->handle;
		tcp_write_message_fragment (cxp);
		/* It is possible that the above call ended up in cxp being cleaned up. We can verify this by varifying
		   on the handle of that context. If the handle is still valid, we're in good shape and it is safe
		   to continue. */
		if (!rtps_ip_from_handle (handle)) {
			log_printf (RTPS_ID, 0, "POLLOUT [%d]: cxp %p h:%d was cleaned up\r\n", fd, (void *) cxp, handle);
			return;
		}
	}
	if ((revents & POLLIN) != 0) {
		tcp_receive_message (cxp);
		return;
	}
}

#ifdef TRACE_CON_EVENTS
#define	trc_con1(s,a)		log_printf (RTPS_ID, 0, s, a)
#define	trc_con2(s,a1,a2)	log_printf (RTPS_ID, 0, s, a1, a2)
#else
#define	trc_con1(s,a)
#define	trc_con2(s,a1,a2)
#endif

/* tcp_clear_pending_connect -- Remove the current pending connection and return
				the next. */

TCP_CON_REQ_ST *tcp_clear_pending_connect (TCP_CON_REQ_ST *p)
{
	TCP_CON_LIST_ST	*xp, *prev;

	trc_con1 ("tcp_clear_pending_connect(cp=%p) ", (void *) p);
	if (!p->next) {
		for (prev = NULL, xp = tcp_cx_pending;
		     xp;
		     prev = xp, xp = xp->next)
			if (xp == p->head)
				break;

		if (!xp)
			fatal_printf ("tcp_clear_pending: head of list not found!\r\n");

		if (prev)
			prev->next = xp->next;
		else
			tcp_cx_pending = xp->next;
		xfree (xp);
		xfree (p);
		p = NULL;
	}
	else {
		p = p->next;
		xfree (p->head->reqs);
		p->head->reqs = p;
	}
	trc_con1 ("-> next cp=%p\r\n", (void *) p);
	return (p);
}

static void tcp_wait_connect_complete (SOCKET fd, short revents, void *arg);

/* tcp_do_connect -- Connect to the given connection request record. */

static int tcp_do_connect (TCP_CON_REQ_ST *p)
{
	TCP_CON_LIST_ST		*hp;
	TCP_DATA		*dp;
	struct sockaddr_in	sa_v4;
#ifdef DDS_IPV6
	struct sockaddr_in6	sa_v6;
#endif
	struct sockaddr		*sa;
	socklen_t		len;
	unsigned		family;
	short			events;
	int			fd, r, err;
#ifdef __APPLE__
	int			yes = 1;
#endif

	trc_con1 ("tcp_do_connect(cp=%p);\r\n", (void *) p);
	do {
		/* No connect() in progress currently! */
		hp = p->head;
		if ((hp->locator.kind & LOCATOR_KINDS_IPv4) != 0) {
			sa_v4.sin_family = family = AF_INET;
			sa_v4.sin_port = htons (hp->locator.port);
			sa = (struct sockaddr *) &sa_v4;
			len = sizeof (sa_v4);
			sa_v4.sin_addr.s_addr = htonl ((hp->locator.address [12] << 24) |
						       (hp->locator.address [13] << 16) |
						       (hp->locator.address [14] << 8) |
						        hp->locator.address [15]);
		}
#ifdef DDS_IPV6
		else if ((hp->locator.kind & LOCATOR_KINDS_IPv6) != 0) {
			sa_v6.sin6_family = family = AF_INET6;
			memcpy (sa_v6.sin6_addr.s6_addr, hp->locator.address, 16);
			sa_v6.sin6_port = htons (hp->locator.port);
			sa = (struct sockaddr *) &sa_v6;
			len = sizeof (sa_v6);
		}
#endif
		else {
			log_printf (RTPS_ID, 0, "tcp_do_connect: invalid locator kind!\r\n");
			return (-2);
		}
		dp = xmalloc (sizeof (TCP_DATA));
		if (!dp) {
			warn_printf ("tcp_connect: out of memory for TCP context!");
			return (-2);
		}
		dp->send_msg = dp->recv_msg = NULL;

		fd = socket (family, SOCK_STREAM, IPPROTO_TCP);
		trace_client ("socket", fd, -1);
		if (fd < 0) {
			xfree (dp);
			perror ("tcp_do_connect: socket()");
			log_printf (RTPS_ID, 0, "tcp_do_connect: socket() failed - errno = %d.\r\n", ERRNO);
			return (-2);
		}
		p->cxp->fd = fd;
		p->cxp->fd_owner = 1;
		p->cxp->sproto = dp;

#ifdef __APPLE__
		/* MSG_NOSIGNAL does not exist for Apple OS, but a equivalent socket option is available */
		if (setsockopt (fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof (yes)) < 0)
			perror ("tcp_do_connect: setsockopt()");
#endif
#ifdef DDS_TCP_NODELAY
		sock_set_tcp_nodelay (fd);
#endif
		sock_set_socket_nonblocking (fd);
		events = POLLIN | POLLPRI | POLLHUP | POLLNVAL;
		for (;;) {
			r = connect (fd, sa, len);
			trace_client ("connect", r, fd);
			if (r == -1) {
				err = ERRNO;
				if (err == EINTR)
					continue;

				if (err != EINPROGRESS) {
					perror ("tcp_do_connect: connect()");
					log_printf (RTPS_ID, 0, "tcp_do_connect: connect() failed - errno = %d.\r\n", err);
					close (fd);
					trace_server ("close", r, fd);
					p->cxp->cx_state = CXS_WRETRY;
					p->cxp->fd = 0;
					p->cxp->fd_owner = 0;
				}
				else {
					log_printf (RTPS_ID, 0, "TCP: connecting to server [%d] ...\r\n", fd);
					p->cxp->cx_state = CXS_CONNECT;
					sock_fd_add_socket (fd, events | POLLOUT, tcp_wait_connect_complete, p, "DDS.TCP-C");
					return (-1);
				}
			}
			else {
				log_printf (RTPS_ID, 0, "TCP: connected to server [%d]\r\n", fd);
				p->cxp->cx_state = CXS_OPEN;
				sock_fd_add_socket (fd, events, tcp_socket_activity, p->cxp, "DDS.TCP-C");
			}
			break;
		}
		p = tcp_clear_pending_connect (p);
	}
	while (p);
	return (r);
}

static void tcp_wait_connect_complete (SOCKET fd, short revents, void *arg)
{
	TCP_CON_REQ_ST	*p = (TCP_CON_REQ_ST *) arg;
	IP_CX		*cxp = p->cxp;
	socklen_t	s;
	int		err, r;
	socklen_t	sz;

	trace_poll_events (fd, revents, arg);

	trc_con2 ("tcp_wait_connect_complete(cp=%p, cxp=%p);\r\n", (void *) p, (void *) cxp);
	p = tcp_clear_pending_connect (p);
	do {
		if ((revents & (POLLERR | POLLNVAL)) != 0) {
			sz = sizeof (err);
			r = getsockopt (cxp->fd, SOL_SOCKET, SO_ERROR, &err, &sz);
			if (r == -1 || err)  {
				log_printf (RTPS_ID, 0, "POLLERR | POLLNVAL [%d]: %d %s\r\n", cxp->fd, err, strerror (err));
				tcp_cleanup_ctx (cxp);
				break;
			}
		}
		if ((revents & POLLHUP) != 0) {
			tcp_cleanup_ctx (cxp);
			break;
		}
		if ((revents & POLLOUT) != 0) {
			s = sizeof (err);
			r = getsockopt (cxp->fd, SOL_SOCKET, SO_ERROR, &err, &s);
			if (r || err) {
				if (r)
					perror ("cc_control: getsockopt(SOL_SOCKET/SO_ERROR)");
				tcp_cleanup_ctx (cxp);
				break;
			}
		}
		log_printf (RTPS_ID, 0, "TCP: server connection established [%d]\r\n", cxp->fd);
		cxp->cx_state = CXS_OPEN;
		sock_fd_remove_socket (cxp->fd);
		sock_fd_add_socket (cxp->fd,
				    POLLIN | POLLPRI | POLLHUP | POLLNVAL,
				    tcp_socket_activity,
				    cxp, "DDS.TCP-H");
		cxp->stream_cb->on_connected (cxp);
	}
	while (0);
	if (p)
		tcp_do_connect (p);
}

static void tcp_pending_free (TCP_FD *pp)
{
	TCP_FD	*xpp, *prev_pp;

	for (prev_pp = NULL, xpp = pp->parent->pending;
	     xpp;
	     prev_pp = xpp, xpp = xpp->next)
		if (xpp == pp) {
			if (prev_pp)
				prev_pp->next = pp->next;
			else
				pp->parent->pending = pp->next;
			xfree (pp);
			break;
		}
}

static void tcp_close_pending_connection (TCP_FD *pp)
{
	int	r;

	if (tmr_active (&pp->timer))
		tmr_stop (&pp->timer);

	sock_fd_remove_socket (pp->fd);
	r = close (pp->fd);
	trace_server ("close", r, pp->fd);
	if (r)
		perror ("tcp_close_pending_connection: close()");
	pp->fd = 0;
	tcp_pending_free (pp);
}

static void tcp_pending_timeout (uintptr_t user)
{
	TCP_FD	*pp = (TCP_FD *) user;

	log_printf (RTPS_ID, 0, "tcp_pending_timeout: connection close!\r\n");
	tcp_close_pending_connection (pp);
}

static void tcp_pending_first_message (SOCKET fd, short revents, void *arg)
{
	TCP_FD		*pp = (TCP_FD *) arg;
	IP_CX		*cxp;
	TCP_DATA	*dp;
	int		err, r;
	socklen_t	sz;

	trace_poll_events (fd, revents, arg);

	if ((revents & (POLLERR | POLLNVAL)) != 0) {
		sz = sizeof (err);
		r = getsockopt (pp->fd, SOL_SOCKET, SO_ERROR, &err, &sz);
		if ((r == -1) || err)  {
			log_printf (RTPS_ID, 0, "POLLERR | POLLNVAL [%d]: %d %s\r\n", pp->fd, err, strerror (err));
			tcp_close_pending_connection (pp);
			return;
		}
	}

	/* Check if connection unexpectedly closed. */
	if ((revents & POLLHUP) != 0) {
		log_printf (RTPS_ID, 0, "TCP(Sp): connection error!\r\n");
		tcp_close_pending_connection (pp);
		return;
	}

	if (!pp->sproto) {
		dp = xmalloc (sizeof (TCP_DATA));
		if (!dp) {
			log_printf (RTPS_ID, 0, "tcp_pending_first_message: out of memory for TCP context on [%d]\r\n", pp->fd);
			tcp_close_pending_connection (pp);
			return;
		}
		dp->recv_msg = xmalloc (sizeof (TCP_MSG));
		if (!dp->recv_msg) {
			xfree (dp);
			log_printf (RTPS_ID, 0, "tcp_pending_first_message: out of memory for TCP context on [%d]\r\n", pp->fd);
			tcp_close_pending_connection (pp);
			return;
		}
		dp->recv_msg->msg = NULL;
		dp->recv_msg->buffer = NULL;
		dp->recv_msg->size = dp->recv_msg->used = 0;
		dp->send_msg = NULL;
		pp->sproto = dp;
	}
	else
		dp = (TCP_DATA *) pp->sproto;

	if (tcp_receive_message_fragment (pp->fd, dp->recv_msg) == -1) {
		tcp_close_pending_connection (pp);
		return;
	}
	if (dp->recv_msg->used != dp->recv_msg->size)
		return; /* message (still) incomplete */

	cxp = pp->parent->stream_cb->on_new_connection (pp, dp->recv_msg->buffer, dp->recv_msg->size);
	xfree (dp->recv_msg->buffer);
	dp->recv_msg->buffer = NULL;
	dp->recv_msg->size = dp->recv_msg->used = 0;
	if (!cxp) {
		/* pending connection could not be 'promoted' to an IP_CX */
		tcp_close_pending_connection (pp);
		return;
	}

	/* Note: it is assumed that the returned cxp is usable for this layer, iow that required
	   callbacks are filled in. Failure to do so will result in crashes. */
	sock_fd_udata_socket (cxp->fd, cxp);
	sock_fd_fct_socket (cxp->fd,
			    tcp_socket_activity);

	/*	sock_fd_remove_socket (pp->fd);
	sock_fd_add_socket (cxp->fd,
			    POLLIN | POLLPRI | POLLHUP | POLLNVAL | POLLOUT,
			    tcp_socket_activity,
			    cxp,
			    "DDS.TCP");
	*/
	tmr_stop (&pp->timer);
	tcp_pending_free (pp);
}

static void tcp_server_accept (SOCKET fd, short revents, void *arg)
{
	IP_CX			*scxp = (IP_CX *) arg;
	TCP_FD			*pp;
	struct sockaddr_in	peer_addr;
#ifdef DDS_IPV6
	struct sockaddr_in6	peer_addr_v6;
#endif
	struct sockaddr		*caddr;
	socklen_t		size;
	uint32_t		a4;
	int			r;

	trace_poll_events (fd, revents, arg);

	memset (&scxp->dst_addr, 0, sizeof (scxp->dst_addr));
	if (scxp->locator->locator.kind == LOCATOR_KIND_TCPv4) {
		peer_addr.sin_family = AF_INET;
		caddr = (struct sockaddr *) &peer_addr;
		size = sizeof (struct sockaddr_in);
	}
#ifdef DDS_IPV6
	else if (scxp->locator->locator.kind == LOCATOR_KIND_TCPv6) {
		peer_addr_v6.sin6_family = AF_INET6;
		caddr = (struct sockaddr *) &peer_addr_v6;
		size = sizeof (struct sockaddr_in6);
	}
#endif
	else {
		warn_printf ("TCP(S): unknown address family!");
		return;
	}
#ifdef TCP_ACCEPT_DELAY_MS
	usleep (TCP_ACCEPT_DELAY_MS * 1000);
#endif
	r = accept (fd, caddr, &size);
	trace_server ("accept", r, fd);
	if (r < 0) {
		perror ("tcp_server_accept: accept()");
		log_printf (RTPS_ID, 0, "tcp_server_accept: accept() failed - errno = %d.\r\n", ERRNO);
		return;
	}

#ifdef DDS_TCP_NODELAY
	sock_set_tcp_nodelay (r);
#endif

	/* Create a new pending TCP connection. */
	pp = xmalloc (sizeof (TCP_FD));
	if (!pp) {
		close (r); /* bad reuse of variable in error case */
		trace_server ("close", 0, r);
		log_printf (RTPS_ID, 0, "TCP(S): allocation failure!\r\n");
		return;
	}
	pp->fd = r;
	if (scxp->locator->locator.kind == LOCATOR_KIND_TCPv4) {
		a4 = ntohl (peer_addr.sin_addr.s_addr);
		memset (pp->dst_addr, 0, 12);
		pp->dst_addr [12] = a4 >> 24;
		pp->dst_addr [13] = (a4 >> 16) & 0xff;
		pp->dst_addr [14] = (a4 >> 8) & 0xff;
		pp->dst_addr [15] = a4 & 0xff;
		pp->dst_port = ntohs (peer_addr.sin_port);
	}
#ifdef DDS_IPV6
	else if (scxp->locator->locator.kind == LOCATOR_KIND_TCPv6) {
		memcpy (&pp->dst_addr, &peer_addr_v6.sin6_addr, 16);
		pp->dst_port = ntohs (peer_addr_v6.sin6_port);
	}
#endif
	else {
		r = close (pp->fd);
		trace_server ("close", r, pp->fd);
		xfree (pp);
		log_printf (RTPS_ID, 0, "TCP(S): unsupported connection family!\r\n");
		return;
	}
	tmr_init (&pp->timer, "TCP-Pending");
	pp->parent = scxp;
	pp->next = scxp->pending;
	scxp->pending = pp;
	pp->sproto = NULL;
	tmr_start (&pp->timer, TICKS_PER_SEC * 2, (uintptr_t) pp, tcp_pending_timeout);

	/* Set socket as non-blocking. */
	sock_set_socket_nonblocking (pp->fd);
	sock_fd_add_socket (pp->fd,
			    POLLIN | POLLPRI | POLLHUP | POLLNVAL,
			    tcp_pending_first_message,
			    pp,
			    "DDS.TCP-A");
	log_printf (RTPS_ID, 0, "TCP: new pending connection on [%d]!\r\n", pp->fd);
}

/* TCP implementation of STREAM_API */

static int tcp_start_server (IP_CX *scxp)
{
	struct sockaddr_in	sa_v4;
#ifdef DDS_IPV6
	struct sockaddr_in6	sa_v6;
#endif
	struct sockaddr		*sa;
	size_t			size;
	int			fd, r;
	unsigned		family;

#ifdef DDS_IPV6
	if (scxp->locator->locator.kind == LOCATOR_KIND_TCPv4) {
#endif
		sa_v4.sin_family = family = AF_INET;
		memset (&sa_v4.sin_addr, 0, 4);
		sa_v4.sin_port = htons (scxp->locator->locator.port);
		sa = (struct sockaddr *) &sa_v4;
		size = sizeof (sa_v4);
		log_printf (RTPS_ID, 0, "TCP: Starting TCPv4 server on port %d.\r\n", scxp->locator->locator.port);
#ifdef DDS_IPV6
	}
	else {
		sa_v6.sin6_family = family = AF_INET6;
		memset (&sa_v6.sin6_addr, 0, 16);
		sa_v6.sin6_port = htons (scxp->locator->locator.port);
		sa = (struct sockaddr *) &sa_v6;
		size = sizeof (sa_v6);
		log_printf (RTPS_ID, 0, "TCP: Starting TCPv6 server on port %d.\r\n", scxp->locator->locator.port);
	}
#endif

	fd = socket (family, SOCK_STREAM, IPPROTO_TCP);
	trace_server ("socket", fd, -1);
	if (fd < 0) {
		perror ("tcp_start_server: socket()");
		log_printf (RTPS_ID, 0, "tcp_start_server: socket() failed - errno = %d.\r\n", ERRNO);
		return (-1);
	}

#ifdef DDS_TCP_NODELAY
	sock_set_tcp_nodelay (fd);
#endif

	r = bind (fd, sa, size);
	trace_server ("bind", r, fd);
	if (r) {
		perror ("tcp_start_server: bind()");
		log_printf (RTPS_ID, 0, "tcp_start_server: bind() failed - errno = %d.\r\n", ERRNO);
		r = close (fd);
		trace_server ("close", r, fd);
		return (-1);
	}
	r = listen (fd, 32);
	trace_server ("listen", r, fd);
	if (r) {
		perror ("tcp_start_server: listen()");
		log_printf (RTPS_ID, 0, "tcp_start_server: listen() failed - errno = %d.\r\n", ERRNO);
		r = close (fd);
		trace_server ("close", r, fd);
		return (-1);
	}

	/* Set socket as non-blocking. */
	sock_set_socket_nonblocking (scxp->fd);

	scxp->fd = fd;
	scxp->fd_owner = 1;
	scxp->cx_state = CXS_LISTEN;
	sock_fd_add_socket (scxp->fd,
			    POLLIN | POLLPRI | POLLHUP | POLLNVAL,
			    tcp_server_accept,
			    scxp, "DDS.TCP-S");

	log_printf (RTPS_ID, 0, "TCP: Server started.\r\n");
	return (0);
}

static void tcp_stop_server (IP_CX *scxp)
{
	ARG_NOT_USED (scxp);
}

TCP_CON_REQ_ST *tcp_pending_connect_remove (IP_CX *cxp)
{
	TCP_CON_LIST_ST	*prev, *xp;
	TCP_CON_REQ_ST	*pprev, *p, *next_req = NULL;

	trc_con1 ("tcp_pending_connect_remove(%p);\r\n", (void *) cxp);
	for (prev = NULL, xp = tcp_cx_pending;
	     xp;
	     prev = xp, xp = xp->next)
		for (pprev = NULL, p = xp->reqs;
		     p;
		     pprev = p, p = p->next)
			if (p->cxp == cxp) {
				if (!pprev && !p->next) {
					if (prev)
						prev->next = xp->next;
					else
						tcp_cx_pending = xp->next;
					xfree (xp);
				}
				else if (!pprev) {
					xp->reqs = p->next;
					next_req = xp->reqs;
				}
				else
					pprev->next = p->next;
				xfree (p);
				return (next_req);
			}

	return (NULL);
}

/* tcp_connect_enqueue -- Request a TCP connection setup to a destination. */

int tcp_connect_enqueue (IP_CX *cxp, unsigned port, TCP_CON_FCT fct)
{
	TCP_CON_LIST_ST		*pcp, *new_pcp;
	TCP_CON_REQ_ST		*rp, **rpp;
	unsigned char		*ap;
	Locator_t		l;
	int			r;

	/* Find a matching connect() address record in the existing pending
	   addresses list. */
	if (cxp->parent)
		ap = cxp->parent->dst_addr;
	else
		ap = cxp->dst_addr;
	l.kind = cxp->locator->locator.kind;
	l.port = port;
	memcpy (l.address, ap, 16);
	for (pcp = tcp_cx_pending; pcp; pcp = pcp->next)
		if (pcp->fct == fct &&
		    pcp->locator.kind == l.kind &&
		    pcp->locator.port == l.port &&
		    !memcmp (pcp->locator.address, l.address, 16))
			break;

	/* If not found, create a new pending connect() address node. */
	if (!pcp) {
		new_pcp = xmalloc (sizeof (TCP_CON_LIST_ST));
		if (!new_pcp) {
			log_printf (RTPS_ID, 0, "tcp_connect_enqueue: not enough memory for pending connection data!\r\n");
			return (-2);
		}
		trc_con1 ("tcp_connect_enqueue(): new address node (%p)\r\n", (void *) new_pcp);
		pcp = new_pcp;
		pcp->locator = l;
		pcp->fct = fct;
		pcp->reqs = NULL;
	}
	else {
		trc_con1 ("tcp_connect_enqueue(): address node exists (%p)\r\n", (void *) pcp);
		new_pcp = NULL;
		log_printf (RTPS_ID, 0, "tcp_connect_enqueue(): connect() already pending -- queueing request!\r\n");
	}

	/* Append the new pending connect() data to the existing/new address
	   node. */
	rp = xmalloc (sizeof (TCP_CON_REQ_ST));
	if (!rp) {
		log_printf (RTPS_ID, 0, "tcp_connect_enqueue: not enough memory for pending connection data!\r\n");
		if (new_pcp)
			xfree (new_pcp);
		return (-2);
	}
	trc_con1 ("tcp_connect_enqueue(): new connect node (%p)!\r\n", (void *) rp);
	rp->head = pcp;
	rp->cxp = cxp;
	cxp->cx_state = CXS_CONREQ;
	for (rpp = &pcp->reqs; *rpp; rpp = &(*rpp)->next)
		;

	*rpp = rp;
	rp->next = NULL;

	if (!new_pcp)
		return (-1); /* Existing pending address node: we're done. */

	/* New address node becomes head of list. */
	pcp->next = tcp_cx_pending;
	tcp_cx_pending = pcp;

	/* Do the actual connect() now. */
	r = (*fct) (rp);
	if (r == -2)
		tcp_pending_connect_remove (cxp);
	return (r);
}

/* tcp_connect -- Do a connect() to the destination specified by the
		  connection/port. */

static int tcp_connect (IP_CX *cxp, unsigned port)
{
	trc_con2 ("tcp_connect(cxp=%p, port=%u);\r\n", (void *) cxp, port);
	return (tcp_connect_enqueue (cxp, port, tcp_do_connect));
}

static void tcp_disconnect (IP_CX *cxp)
{
	TCP_CON_REQ_ST	*next_p = NULL;
	TCP_DATA	*dp;
	int		r;

	trc_con1 ("tcp_disconnect(%p);\r\n", (void *) cxp);
	if (cxp->cx_state == CXS_CONREQ || cxp->cx_state == CXS_CONNECT)
		next_p = tcp_pending_connect_remove (cxp);

	if (cxp->fd_owner) {
		sock_fd_remove_socket (cxp->fd);
		do {
			r = close (cxp->fd);
			trace_server ("close", r, cxp->fd);
		}
		while (r == -1 && ERRNO == EINTR);
		cxp->fd = 0;
		cxp->fd_owner = 0;
		dp = cxp->sproto;
		cxp->sproto = NULL;
		if (dp) {
			if (dp->recv_msg) {
				if (dp->recv_msg->buffer) {
					xfree (dp->recv_msg->buffer);
					dp->recv_msg->buffer = NULL;
				}
				xfree (dp->recv_msg);
				dp->recv_msg = NULL;
			}
			if (dp->send_msg) {
				if (dp->send_msg->buffer) {
					xfree (dp->send_msg->buffer);
					dp->send_msg->buffer = NULL;
				}
				xfree (dp->send_msg);
				dp->send_msg = NULL;
			}
			if (!cxp->paired || cxp->paired->fd != cxp->fd)
				xfree (dp);
		}
	}
	if (next_p)
		tcp_do_connect (next_p);
}

static WR_RC tcp_write_msg (IP_CX *cxp, unsigned char *msg, size_t len)
{
	ssize_t		n;
	size_t		left;
	unsigned char	*sp = msg;
	TCP_DATA	*dp = cxp->sproto;
	TCP_MSG		**send_msg;
	int		err;

	if (!dp) {
		dp = xmalloc (sizeof (TCP_DATA));
		if (!dp) {
			log_printf (RTPS_ID, 0, "tcp_write_msg: out of memory for TCP context on [%d]\r\n", cxp->fd);
			return (WRITE_FATAL);
		}
		dp->recv_msg = dp->send_msg = NULL;
		cxp->sproto = dp;
	}
	else if (dp->send_msg) 
		return (WRITE_BUSY);

	left = len;
	while (left) {
#ifdef __APPLE__
		/* Apple does not support MSG_NOSIGNAL, therefore we set the equivalent Apple-specific 
		   socket option SO_NOSIGPIPE. */
		n = send (cxp->fd, sp, left, 0);
#else
		/* We're using send() here iso write() so that we can indicate to the kernel *not* to send
		   a SIGPIPE in case the other already closed this connection. A return code of -1 and ERRNO
		   to EPIPE is given instead */
		n = send (cxp->fd, sp, left, MSG_NOSIGNAL);
#endif
		trace_write (n, cxp->fd, left);
#ifdef MSG_TRACE
		if (cxp->trace)
			rtps_ip_trace (cxp->handle, 'T',
				       &cxp->locator->locator,
				       cxp->dst_addr, cxp->dst_port,
				       left);
#endif
		if (n < 0) {
			err = ERRNO;
			if (err == EINTR)
				continue; /* immediately try again */

			if (err == EAGAIN || err == EWOULDBLOCK) {
				/* Make sure to attach the message to the context owning the fd. */
				send_msg = &dp->send_msg;
				/* Make arrangements for fragmented write */
				*send_msg = xmalloc (sizeof (TCP_MSG));
				if (!*send_msg)
					return (WRITE_ERROR);

				(*send_msg)->buffer = xmalloc (left);
				if (!(*send_msg)->buffer) {
					xfree (*send_msg);
					*send_msg = NULL;
					return (WRITE_ERROR);
				}
				memcpy ((*send_msg)->buffer, sp, left);
				(*send_msg)->size = left;
				(*send_msg)->used = 0;
#ifdef TRACE_POLL_EVENTS
				log_printf (RTPS_ID, 0, "TLS: POLLOUT = 1 [%d]\r\n", cxp->fd);
#endif
				sock_fd_event_socket (cxp->fd, POLLOUT, 1);
				return (WRITE_PENDING);
			}
			perror ("tcp_write_msg");
			log_printf (RTPS_ID, 0, "%s: error sending data on [%d] (%s).\r\n",
					__FUNCTION__, cxp->fd, strerror (ERRNO));
			return (WRITE_ERROR);
		}
		left -= n;
		sp += n;
	}
	return (WRITE_OK);
}

STREAM_API tcp_functions = {
	tcp_start_server,
	tcp_stop_server,
	tcp_connect,
	tcp_disconnect,
	tcp_write_msg
};

#else

int avoid_emtpy_translation_unit_ri_tcp_sock_c;

#endif

