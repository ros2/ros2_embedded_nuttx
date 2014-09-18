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

/* sock.h -- Provides a set of socket and handle manipulation functions to make
	     it easier to handle I/O on these entities.  In practice, the user
	     simply registers it entities, specifying callback functions that
	     will be called when there is work to be done.
	     The sock_fd_poll() will be called regularly, and will set the
	     DDS_EV_IO flag when I/O activity is detected.  If so, the
	     sock_fd_schedule() function must be called as soon as possible,
	     which will call the appropriate callback functions. */

#ifndef __sock_h_
#define __sock_h_

#ifdef _WIN32
#include "win.h"
#else
#define SOCKET	int
#define HANDLE	int
#endif

typedef void (* RHDATAFCT) (HANDLE h, short revents, void *udata);
typedef void (* RSDATAFCT) (SOCKET fd, short revents, void *udata);

int sock_fd_init (void);

/* Initialize the socket I/O handler. */

void sock_fd_final (void);

/* Finalize the poll file descriptor array. */

#ifdef _WIN32

/* Windows makes a clear difference between Sockets and Handles to non-socket
   objects.  In fact, it is not even possible to use the well-known Unix way
   of combining Sockets and File descriptors in a select/poll wait loop.
   We therefor are forced to use the more low-level WaitForMultipleObjects()
   function, but this requires the difference to be visible at API level. */

int sock_fd_add_handle (HANDLE     h,
			short      events,
			RHDATAFCT  fct,
			void       *udata,
			const char *name);

/* Add a file descriptor and associated callback function and user parameter. */

void sock_fd_remove_handle (HANDLE h);

/* Remove a file descriptor. */

int sock_fd_add_socket (SOCKET     s,
			short      events,
			RSDATAFCT  fct,
			void       *udata,
			const char *name);

/* Add a socket and associated callback function and user parameter. */

void sock_fd_remove_socket (SOCKET s);

/* Remove a socket. */

int sock_fd_valid_socket (SOCKET s);

/* Check if a fd is (still) in the socket descriptors */ 

int sock_fd_event_socket (SOCKET s, short events, int set);

/* Update the notified events on a socket. */

void sock_fd_fct_socket (int fd, RSDATAFCT fct);

/* Update the notified callback function on a socket. */

void sock_fd_udata_socket (SOCKET s, void *udata);

/* Update the notified user data on a socket. */

#else

int sock_fd_add (int fd, short events, RHDATAFCT fct, void *udata, const char *name);

/* Add a file descriptor and associated callback function and user parameter. */

void sock_fd_remove (int fd);

/* Remove a file descriptor. */

int sock_fd_valid (int fd);

/* Check if a fd is (still) in the socket descriptors */ 

void sock_fd_event (int fd, short events, int set);

/* Update the notified events on a socket. */

void sock_fd_fct (int fd, RSDATAFCT fct);

/* Update the notified user data of a socket. */

void sock_fd_udata (int fd, void *udata);

/* Update the notified user data of a socket. */

#define sock_fd_add_handle	sock_fd_add
#define sock_fd_remove_handle	sock_fd_remove
#define sock_fd_add_socket	sock_fd_add
#define sock_fd_remove_socket	sock_fd_remove
#define	sock_fd_valid_socket	sock_fd_valid
#define	sock_fd_event_socket	sock_fd_event
#define	sock_fd_fct_socket	sock_fd_fct
#define	sock_fd_udata_socket	sock_fd_udata

#endif

void sock_fd_poll (unsigned poll_time);

/* Use poll() or select() to query the state of all socket/handle descriptors.
   The poll_time argument gives the max. time to wait in milliseconds. */

void sock_fd_schedule (void);

/* Schedule all pending event handlers. */

int sock_set_socket_nonblocking (int fd);

/* Set the socket with fd in non blocking mode. */

int sock_set_tcp_nodelay (int fd);

/* set socket tcp option TCP_NODELAY. */

void sock_fd_dump (void);

/* Debug only: dump all file descriptor contexts. */

#if defined (NUTTX_RTOS)
#define TCP_NODELAY     1       /* don't delay send to coalesce packets */
#endif

#endif /* !__sock_h_ */

