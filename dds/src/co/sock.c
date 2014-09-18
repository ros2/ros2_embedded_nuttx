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

/* sock.c -- Provides common socket control functions to manage the
	     different DDS transport protocols that use fds. */

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "win.h"
#elif defined (NUTTX_RTOS)
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
/* #include <netinet/tcp.h> */
#include <poll.h> 
#else
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#endif
#include <string.h>
#include <errno.h>
#include "pool.h"
#include "config.h"
#include "log.h"
#include "atomic.h"
#include "error.h"
#include "timer.h"
#include "dds.h"
#include "debug.h"
#include "sock.h"
#include <sys/types.h>
#include <sys/socket.h>

static int n_ready;
static lock_t sock_lock;
static lock_t poll_lock;

#ifdef _WIN32

#define INC_SIZE	16
#define MAX_SIZE	MAXIMUM_WAIT_OBJECTS

typedef struct handle_st {
	int		is_socket;
	unsigned	index;
	unsigned	events;
	const char	*name;
	HANDLE		handle;
	RHDATAFCT	hfct;
	void		*udata;
} SockHandle_t;

typedef struct socket_st {
	int		is_socket;
	unsigned	index;
	unsigned	events;
	const char	*name;
	WSAEVENT	handle;
	RSDATAFCT	sfct;
	void		*udata;
	SOCKET		socket;
} SockSocket_t;

typedef struct sock_st {
	int		is_socket;
	unsigned	index;
	unsigned	events;
	const char	*name;
	HANDLE		handle;

	/* -- remainder depends on is_socket -- */

} Sock_t;

static unsigned		nhandles;
static HANDLE		whandles [MAXIMUM_WAIT_OBJECTS];
static Sock_t		*wsock [MAXIMUM_WAIT_OBJECTS];
static SockSocket_t	(*sockets) [MAXIMUM_WAIT_OBJECTS];
static SockHandle_t	(*handles) [MAXIMUM_WAIT_OBJECTS];
static unsigned		num_socks, max_socks;
static unsigned		num_handles, max_handles;

/* sock_fd_init -- Initialize the file descriptor array. */

int sock_fd_init (void)
{
	static int	initialized = 0;

	if (sockets || handles)
		return (0);

	sockets = xmalloc (sizeof (SockSocket_t) * INC_SIZE);
	if (!sockets)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	handles = xmalloc (sizeof (SockHandle_t) * INC_SIZE);
	if (!handles) {
		xfree (sockets);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	num_socks = 0;
	max_socks = INC_SIZE;
	num_handles = 0;
	max_handles = INC_SIZE;

	if (!initialized) {
		lock_init_nr (sock_lock, "sock");
		lock_init_nr (poll_lock, "poll");
		initialized = 1;
	}
	return (0);
}

/* sock_fd_final -- Finalize the poll file descriptor array. */

void sock_fd_final (void)
{
	if (!max_socks)
		return;

	xfree (sockets);
	xfree (handles);
	num_socks = max_socks = num_handles = max_handles = 0;
	sockets = NULL;
	handles = NULL;
}

/* sock_fd_add_handle -- Add a handle and associated callback function. */

int sock_fd_add_handle (HANDLE     h,
			short      events,
			RHDATAFCT  rx_fct,
			void       *udata,
			const char *name)
{
	SockHandle_t	*hp;
	void		*p;

	if (!max_handles) /* Needed for Debug init ... */
		sock_fd_init ();

	lock_take (sock_lock);
	if (num_handles == max_handles ||
	    nhandles >= MAXIMUM_WAIT_OBJECTS) {
		if (!max_handles ||
		    max_handles >= MAX_SIZE ||
		    nhandles >= MAXIMUM_WAIT_OBJECTS) {
			lock_release (sock_lock);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		max_handles += INC_SIZE;
		p = xrealloc (handles, sizeof (SockHandle_t) * max_handles);
		if (!p)
			fatal_printf ("sock_fd_add: can't realloc()!");

		handles = p;
	}

	/*printf ("handle added: fd=%d, events=%d\n", h, events);*/
	hp = &(*handles) [num_handles];
	hp->is_socket = 0;
	hp->index = nhandles;
	hp->events = events;
	hp->name = name;
	hp->handle = h;
	hp->hfct = rx_fct;
	hp->udata = udata;
	num_handles++;
	whandles [nhandles] = h;
	wsock [nhandles++] = (Sock_t *) hp;
	lock_release (sock_lock);
	return (DDS_RETCODE_OK);
}

/* sock_fd_remove_handle -- Remove a file descriptor. */

void sock_fd_remove_handle (HANDLE h)
{
	unsigned	i;
	SockHandle_t	*hp;

	lock_take (sock_lock);
	for (i = 0, hp = &(*handles) [0]; i < num_handles; i++, hp++)
		if (hp->handle == h) {
			if (hp->index + 1 < nhandles) {
				memmove (&whandles [hp->index],
					 &whandles [hp->index + 1],
					 (nhandles - i - 1) *
					 sizeof (HANDLE));
				memmove (&wsock [hp->index],
					 &wsock [hp->index + 1],
					 (nhandles - i - 1) *
					 sizeof (Sock_t *));
			}
			nhandles--;
			if (i + 1 < num_handles)
				memmove (&(*handles) [i],
					 &(*handles) [i + 1],
					 (num_handles - i - 1) *
					 sizeof (SockHandle_t));
			num_handles--;
			break;
		}
	lock_release (sock_lock);
}

/* sock_fd_add_socket -- Add a socket and associated callback function. */

int sock_fd_add_socket (SOCKET s, short events, RSDATAFCT rx_fct, void *udata, const char *name)
{
	SockSocket_t	*sp;
	void		*p;
	unsigned	e;
	WSAEVENT	ev;

	lock_take (sock_lock);
	if ((ev = WSACreateEvent ()) == WSA_INVALID_EVENT) {
		lock_release (sock_lock);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	if (num_socks == max_socks ||
	    nhandles >= MAXIMUM_WAIT_OBJECTS) {
		if (!max_socks ||
		    max_socks > MAX_SIZE ||
		    nhandles >= MAXIMUM_WAIT_OBJECTS) {
			WSACloseEvent (ev);
			lock_release (sock_lock);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		max_socks += INC_SIZE;
		p = xrealloc (sockets, sizeof (SockSocket_t) * max_socks);
		if (!p)
			fatal_printf ("sock_fd_add_socket: can't realloc()!");

		sockets = p;
	}

	/*printf ("socket added: fd=%d, events=%d\n", s, events);*/
	sp = &(*sockets) [num_socks];
	sp->is_socket = 1;
	sp->index = nhandles;
	sp->socket = s;
	sp->events = events;
	sp->name = name;
	sp->handle = ev;
	e = 0;
	if ((events & POLLIN) != 0)
		e = FD_READ;
	if ((events & POLLPRI) != 0)
		e |= FD_OOB;
	if ((events & POLLOUT) != 0)
		e |= FD_WRITE;
	if ((events & POLLHUP) != 0)
		e |= FD_CLOSE;
	if (WSAEventSelect (s, ev, e))
		fatal_printf ("sock_fd_add_socket(): WSAEventSelect() failed - error = %d", WSAGetLastError ());

	sp->sfct = rx_fct;
	sp->udata = udata;
	num_socks++;
	whandles [nhandles] = ev;
	wsock [nhandles++] = (Sock_t *) sp;
	lock_release (sock_lock);
	return (DDS_RETCODE_OK);
}

/* sock_fd_remove -- Remove a file descriptor. */

void sock_fd_remove_socket (SOCKET s)
{
	unsigned	i;
	SockSocket_t	*sp;

	lock_take (sock_lock);
	for (i = 0, sp = &(*sockets) [0]; i < num_socks; i++, sp++)
		if (sp->socket == s) {
			WSACloseEvent (sp->handle);
			if (sp->index + 1 < nhandles) {
				memmove (&whandles [sp->index],
					 &whandles [sp->index + 1],
					 (nhandles - i - 1) *
					 sizeof (HANDLE));
				memmove (&wsock [sp->index],
					 &wsock [sp->index + 1],
					 (nhandles - i - 1) *
					 sizeof (Sock_t *));
			}
			nhandles--;
			if (i + 1 < num_socks)
				memmove (&(*sockets) [i],
					 &(*sockets) [i + 1],
					 (num_socks - i - 1) *
					 sizeof (SockSocket_t));

			num_socks--;
			break;
		}
	lock_release (sock_lock);
}

/* sock_fd_valid_socket -- Check if a socket is still valid. */

int sock_fd_valid_socket (SOCKET s)
{
	unsigned	i;
	SockSocket_t	*sp;
	
	lock_take (sock_lock);
	for (i = 0, sp = &(*sockets) [0]; i < num_socks; i++, sp++)
		if (sp->socket == s) {
			lock_release (sock_lock);
			return (1);
		}

	lock_release (sock_lock);
	return (0);
}

/* sock_fd_event_socket -- Update the notified events on a socket. */

int sock_fd_event_socket (SOCKET s, short events, int set)
{
	unsigned	i;
	SockSocket_t	*sp;

	lock_take (sock_lock);
	for (i = 0, sp = &(*sockets) [0]; i < num_socks; i++, sp++)
		if (sp->socket == s) {
			if (set)
				sp->events |= events;
			else
				sp->events &= ~events;
			break;
		}
	lock_release (sock_lock);
	return (0);
}

/* sock_fd_udata_socket -- Update the notified user data on a socket. */

int sock_fd_udata_socket (SOCKET s, void *udata)
{
	unsigned	i;
	SockSocket_t	*sp;

	lock_take (sock_lock);
	for (i = 0, sp = &(*sockets) [0]; i < num_socks; i++, sp++)
		if (sp->socket == s) {
			sp->udata = udata;
			break;
		}
	lock_release (sock_lock);
	return (0);
}

/* sock_fd_schedule -- Schedule all pending event handlers. */

void sock_fd_schedule (void)
{
	Sock_t		*p;
	SockHandle_t	*hp;
	SockSocket_t	*sp;
	WSANETWORKEVENTS ev;
	unsigned	events;

	if (n_ready < 0 || n_ready >= (int) nhandles)
		return;

	lock_take (sock_lock);
	p = wsock [n_ready];
	if (p->is_socket) {
		sp = (SockSocket_t *) p;
		if (WSAEnumNetworkEvents (sp->socket, sp->handle, &ev)) {
			log_printf (LOG_DEF_ID, 0, "sock_fd_schedule: WSAEnumNetworkEvents() returned error %d\r\n", WSAGetLastError ());
			return;
		}
		events = 0;
		if ((ev.lNetworkEvents & FD_READ) != 0)
			events |= POLLRDNORM;
		if ((ev.lNetworkEvents & FD_OOB) != 0)
			events |= POLLPRI;
		if ((ev.lNetworkEvents & FD_WRITE) != 0)
			events |= POLLWRNORM;
		if ((ev.lNetworkEvents & FD_CLOSE) != 0)
			events |= POLLHUP;
		(*sp->sfct) (sp->socket, events, sp->udata);
	}
	else {
		hp = (SockHandle_t *) p;
		(*hp->hfct) (hp->handle, hp->events, hp->udata);
	}
	lock_release (sock_lock);
}

#undef errno
#define errno	WSAGetLastError()

/* sock_fd_poll -- Use poll() or select() to query the state of all file
		   descriptors. */

void sock_fd_poll (unsigned poll_time)
{
	if (!nhandles) {
		Sleep (poll_time);
		n_ready = -1;
		return;
	}

	/* Wait until at least one handle is signalled or until time-out. */
	n_ready = WaitForMultipleObjects (nhandles, whandles, 0, poll_time);
	if (n_ready >= WAIT_OBJECT_0 &&
	    n_ready <= (int) (WAIT_OBJECT_0 + (int) nhandles - 1)) {
		dds_lock_ev ();
		dds_ev_pending |= DDS_EV_IO;
		n_ready -= WAIT_OBJECT_0;
		dds_unlock_ev ();
	}
	else if (n_ready == WAIT_TIMEOUT)
		n_ready = -1;
	else if (n_ready >= WAIT_ABANDONED_0 &&
		 n_ready <= (int) (WAIT_ABANDONED_0 + (int) nhandles - 1)) {
		log_printf (LOG_DEF_ID, 0, "sock_fd_poll: WaitForMultipleObjects(): abandoned handle %d was signalled", n_ready - WAIT_ABANDONED_0);
		n_ready = -1;
	}
	else if (n_ready == WAIT_FAILED) {
		log_printf (LOG_DEF_ID, 0, "sock_fd_poll: WaitForMultipleObjects() returned an error: %d", GetLastError ());
		n_ready = -1;
	}
	else {
		log_printf (LOG_DEF_ID, 0, "sock_fd_poll: WaitForMultipleObjects() returned unknown status: %d", n_ready);
		n_ready = -1;
	}
}

#else /* if not WIN */

#ifndef FD_MAX_SIZE
#define	FD_INC_SIZE	1024
#define FD_MAX_SIZE	1024
#else
#ifndef FD_INC_SIZE
#define	FD_INC_SIZE	FD_MAX_SIZE
#endif
#endif

static void *(*ud) [FD_MAX_SIZE];
static const char *(*names) [FD_MAX_SIZE];
static RSDATAFCT (*fcts) [FD_MAX_SIZE];
static unsigned num_fds, max_fds, fd_max_size;
static struct pollfd (*fds) [FD_MAX_SIZE];


/* sock_fd_init -- Initialize the poll file descriptor array. */

int sock_fd_init (void)
{
	static int	initialized = 0;

	if (fds)
		return (0);
#if defined (NUTTX_RTOS)
	fds = malloc (sizeof (struct pollfd) * FD_INC_SIZE);
	fcts = malloc (sizeof (RSDATAFCT) * FD_INC_SIZE);
	ud = malloc (sizeof (void *) * FD_INC_SIZE);
	names = malloc (sizeof (const char *) * FD_INC_SIZE);
#else
	fds = xmalloc (sizeof (struct pollfd) * FD_INC_SIZE);
	fcts = xmalloc (sizeof (RSDATAFCT) * FD_INC_SIZE);
	ud = xmalloc (sizeof (void *) * FD_INC_SIZE);
	names = xmalloc (sizeof (const char *) * FD_INC_SIZE);
#endif	
	if (!fds || !fcts || !ud || !names)
		return (1);

	atomic_set_w (num_fds, 0);
	atomic_set_w (max_fds, FD_INC_SIZE);

	if (!initialized) {
		lock_init_nr (sock_lock, "lock");
		fd_max_size = config_get_number (DC_IP_Sockets, FD_MAX_SIZE);
		initialized = 1;
	}
	return (0);
}

/* sock_fd_final -- Finalize the poll file descriptor array. */

void sock_fd_final (void)
{
#if defined (NUTTX_RTOS)
	free (fds);
	free (fcts);
	free (ud);
	free (names);
#else	
	xfree (fds);
	xfree (fcts);
	xfree (ud);
	xfree (names);
#endif	
	atomic_set_w (num_fds, 0);
	atomic_set_w (max_fds, 0);
	fds = NULL;
}

/* sock_fd_add -- Add a file descriptor and associated callback function. */

int sock_fd_add (int fd, short events, RSDATAFCT rx_fct, void *udata, const char *name)
{
	unsigned	n;

	if (!max_fds)
		sock_fd_init ();
	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	if (n == atomic_get_w (max_fds)) {
		if (max_fds >= fd_max_size) {
			lock_release (sock_lock);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		lock_take (poll_lock);
		atomic_add_w (max_fds, FD_INC_SIZE);
#if defined (NUTTX_RTOS)		
		fds = realloc (fds, sizeof (struct pollfd) * max_fds);
		fcts = realloc (fcts, sizeof (RSDATAFCT) * max_fds);
		ud = realloc (ud, sizeof (void *) * max_fds);
#else
		fds = xrealloc (fds, sizeof (struct pollfd) * max_fds);
		fcts = xrealloc (fcts, sizeof (RSDATAFCT) * max_fds);
		ud = xrealloc (ud, sizeof (void *) * max_fds);		
#endif
		lock_release (poll_lock);
		if (!fds || !fcts || !ud)
			fatal_printf ("rtps_fd_add: can't realloc()!");
	}
	/*printf ("socket added: fd=%d, events=%d\n", fd, events);*/
	(*fds) [n].fd = fd;
	(*fds) [n].events = events;
	(*fds) [n].revents = 0;
	(*fcts) [n] = rx_fct;
	(*ud) [n] = udata;
	(*names) [n] = name;
	atomic_inc_w (num_fds);
	lock_release (sock_lock);
	return (DDS_RETCODE_OK);
}

/* sock_fd_valid -- Check if a socket is still valid. */

int sock_fd_valid (int fd)
{
	unsigned	i, n;
	
	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0; i < n; i++)
		if ((*fds) [i].fd == fd) {
			lock_release (sock_lock);
			return (1);
		}

	lock_release (sock_lock);
	return (0);
}

/* sock_fd_remove -- Remove a file descriptor. */

void sock_fd_remove (int fd)
{
	unsigned	i, n;

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0; i < n; i++)
		if ((*fds) [i].fd == fd) {
			lock_take (poll_lock);
			n = atomic_get_w (num_fds);
			if (i + 1 < n) {
				memmove (&(*fds) [i],
						&(*fds) [i + 1],
						(n - i - 1) * sizeof (struct pollfd));
				memmove (&(*fcts) [i],
						&(*fcts) [i + 1],
						(n - i - 1) * sizeof (RSDATAFCT));
				memmove (&(*ud) [i],
						&(*ud) [i + 1],
						(n - i - 1) * sizeof (void *));
				memmove (&(*names) [i],
						&(*names) [i + 1],
						(n - i - 1) * sizeof (char *));
			}
			atomic_dec_w (num_fds);
			lock_release (poll_lock);
			break;
		}
	lock_release (sock_lock);
}

/* sock_fd_event_socket -- Update the notified events on a socket. */

void sock_fd_event_socket (int fd, short events, int set)
{
	unsigned	i, n;

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0; i < n; i++)
		if ((*fds) [i].fd == fd) {
			lock_take (poll_lock);
			if (set)
				(*fds) [i].events |= events;
			else
				(*fds) [i].events &= ~events;
			lock_release (poll_lock);
			break;
		}
	lock_release (sock_lock);
}

/* sock_fd_fct_socket -- Update the notified callback function on a socket. */

void sock_fd_fct_socket (int fd, RSDATAFCT fct)
{
	unsigned	i, n;

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0; i < n; i++)
		if ((*fds) [i].fd == fd) {
			lock_take (poll_lock);
			(*fcts) [i] = fct;
			lock_release (poll_lock);
			break;
		}
	lock_release (sock_lock);
}

/* sock_fd_udata_socket -- Update the notified user data on a socket. */

void sock_fd_udata_socket (int fd, void *udata)
{
	unsigned	i, n;

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0; i < n; i++)
		if ((*fds) [i].fd == fd) {
			lock_take (poll_lock);
			(*ud) [i] = udata;
			lock_release (poll_lock);
			break;
		}
	lock_release (sock_lock);
}

/* sock_fd_schedule -- Schedule all pending event handlers. */

void sock_fd_schedule (void)
{
	struct pollfd	*iop;
	unsigned	i, n;
	RHDATAFCT	fct;
	int		fd;
	void		*user;
	short		events;

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0, iop = *fds; i < n; i++, iop++)
		if (iop->revents) {
			fct = (*fcts) [i];
			fd = iop->fd;
			events = iop->revents;
			user = (*ud) [i];
			iop->revents = 0;
			lock_release (sock_lock);
			(*fct) (fd, events, user);
			lock_take (sock_lock);
			n = atomic_get_w (num_fds);
		}
	lock_release (sock_lock);
}

/* sock_fd_poll -- Use poll() or select() to query the state of all file
		   descriptors. */

void sock_fd_poll (unsigned poll_time)
{
	struct pollfd	*iop;
	unsigned	i, n;

	lock_take (poll_lock);
	n = atomic_get_w (num_fds);
	/*printf ("*"); fflush (stdout);*/
	n_ready = poll (*fds, n, poll_time);
	lock_release (poll_lock);
	if (n_ready < 0) {
		log_printf (LOG_DEF_ID, 0, "sock_fd_poll: poll() returned error: %s\r\n", strerror (errno));
		return;
	}
	else if (!n_ready) {
		/* avoid starvation of other threads waiting on poll_lock.
		 * These other threads always hold sock_lock. so locking and unlocking
		 * sock_lock here, gives the proper synchronization.
		 * In case no one is waiting, this is a waste of time, but without a
		 * rewrite this is not solvable.
		 */
		lock_take (sock_lock);
		lock_release (sock_lock);
		return;
	}

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0, iop = *fds; i < n; i++, iop++) {
		if (iop->revents) {
			/*dbg_printf ("sock: %u %d=0x%04x->0x%04x\r\n", i, iop->fd, iop->events, iop->revents);*/
			dds_lock_ev ();
			dds_ev_pending |= DDS_EV_IO;
			dds_unlock_ev ();
			break;
		}
	}
	lock_release (sock_lock);
}

#ifdef DDS_DEBUG

/* sock_fd_dump -- Dump all file descriptor contexts. */

void sock_fd_dump (void)
{
	unsigned	i, n;

	lock_take (sock_lock);
	n = atomic_get_w (num_fds);
	for (i = 0; i < n; i++) {
		dbg_printf ("%d: [%d] %s {%s} -> ", i, (*fds) [i].fd, (*names) [i], dbg_poll_event_str ((*fds) [i].events));
		dbg_printf ("{%s} ", dbg_poll_event_str ((*fds) [i].events));
		dbg_printf ("Rxfct=0x%lx, U=%p\r\n", (unsigned long) ((*fcts) [i]), (*ud) [i]);
	}
	lock_release (sock_lock);
}

#endif

/* sock_set_socket_nonblocking - Set the socket with fd in non blocking mode. */

int sock_set_socket_nonblocking (int fd)
{
	int ofcmode = fcntl (fd, F_GETFL, 0);

	ofcmode |= O_NONBLOCK;
	if (fcntl (fd, F_SETFL, ofcmode)) {
		perror ("set_socket_nonblocking: fcntl(NONBLOCK)");
		warn_printf ("set_socket_nonblocking: can't set non-blocking!");
		return (0);
	}
	return (1);
}

/* sock_set_tcp_nodelay - set socket tcp option TCP_NODELAY. */

int sock_set_tcp_nodelay (int fd)
{
	int one = 1;

	if (setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one))) {
		perror ("set_tcp_nodelay (): setsockopt () failure");
		warn_printf ("setsockopt (TCP_NODELAY) failed - errno = %d.\r\n", errno);
		return (0);
	}
	return (1);
}

#endif 
/* OS */
