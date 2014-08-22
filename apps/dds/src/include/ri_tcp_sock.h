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

/* ri_tcp_sock.h -- Abstraction layer between the protocol running on top of a (TCP) stream
                    and the actual transport. This allows to implement multiple variations
		    of the transport (open, encrypted, ...). */

#include <stdint.h>
#include <stdlib.h>

#include "timer.h"

#ifndef RI_TCP_SOCK_H
#define RI_TCP_SOCK_H

#ifndef STREAM_API_defined
#define STREAM_API_defined
typedef struct stream_fct_st	STREAM_API;
#endif

#ifndef STREAM_CB_defined
#define STREAM_CB_defined
typedef struct stream_cb_st	STREAM_CB;
#endif

#ifndef TCP_MSG_defined
#define TCP_MSG_defined
typedef struct tcp_msg_frame_st TCP_MSG;
#endif

#ifndef TCP_FD_defined
#define TCP_FD_defined
typedef struct tcp_fd_st	TCP_FD;	/* See: ri_tcp_sock.h */
#endif

#ifndef IP_CX_defined
#define IP_CX_defined
typedef struct ip_cx_st		IP_CX; /* See: ri_data.h */
#endif

typedef unsigned short ControlMsgKind_t;
typedef unsigned char TransactionId_t [12];

typedef struct ctrl_header_st {
	ProtocolId_t		protocol;
	uint16_t		version;
	VendorId_t		vendor_id;
	TransactionId_t		transaction;
	ControlMsgKind_t	msg_kind;
	unsigned short		length;
} CtrlHeader;

struct tcp_msg_frame_st {
	unsigned char	*msg;		/* Original message data buffer. */
	unsigned char	*buffer;	/* Ptr to remaining buffer data. */
	size_t		size;		/* Remaining message size. */
	size_t		used;		/* Number of bytes already read/written for this message. */
	CtrlHeader	header;		/* (read side only) Temporary location to store the header of a
	                                   message. Avoid the need to realloc buffer. */
};

/* Functions which must be implemented by a STREAM_API implementation. */

typedef int (* SA_start_server_fct) (IP_CX *scxp);

/* Start a new server, using data provided in scxp:
       scxp->locator->locator.kind
       scxp->locator->locator.port
       
   Preconditions/Assumptions:
     scxp->fd == 0 (no fd yet for this connection)
     scxp->stream_cb.on_new_connection set; the others are not used
   Return values:
     0: success
       scxp->fd set to resulting sockfd
       scxp->fd_owner set
       scxp->cx_state set to CXS_LISTEN
       Socket  (fd) is set to non-blocking. When this fails, a warning is printed, but it is not seen as a hard error.
    -1: error (+ logmessage on exact cause of the problem). */

typedef void (* SA_stop_server_fct) (IP_CX *scxp);

/* Stop listening on this socket.
   Preconditions:
     scxp has previously been provided to the corresponding start_server() function.
   */

typedef int (* SA_connect_fct) (IP_CX *cxp, unsigned port);

/* Setup a new connection to the given server. Upon success, new messages will be delivered using
   the SA_on_new_message_cb.
   Return values:
     0: success
    -1: error (+ logmessage on exact cause of the problem). */

typedef void (* SA_disconnect_fct) (IP_CX *cxp);

/* Close the connection (if fd_owner). This should be called for both server side as for client side. */

typedef enum write_return_codes_e {
	WRITE_OK,	/* The message has been written. Ready to write another one. */
	WRITE_PENDING,	/* The message has only been partly written. The data will be buffered and the
	                   on_write_completed callback will be issued when the message has been completely
			   written. */
	WRITE_BUSY,	/* A previous pending write  has not yet been completed. The provided data is *not*
	                   buffered in this case. */
	WRITE_ERROR,	/* An error occured or an other precondition is not met. The provided data is *not*
	                   buffered in this case. More details about the exact issue can be found in ERRNO.
			   It is up to the caller to decide what to do with this context (close it, retry,
			   ...). */
	WRITE_FATAL	/* No use continuing. Caller should close/cleanup contexts. */
} WR_RC;

typedef WR_RC (* SA_write_msg_fct) (IP_CX *cxp, unsigned char *msg, size_t len);

/* Write a message on the stream. This method may never call the on_close() callback directly. When an
   error occurs, that must be reported with WRITE_ERROR. The caller then has to decide what to be done with
   the connection.
   Note:
   Calling this function with len == 0 will return the current status:
       WRITE_OK: currently not writing anything
       WRITE_BUSY: a fragmented write is ongoing */

struct stream_fct_st {
	SA_start_server_fct	start_server;	/* Start a TCP server (listen) and prepare to accept new connections.*/
	SA_stop_server_fct	stop_server;	/* Stop a TCP server. */
	SA_connect_fct		connect;	/* Setup a new connection to a server. */
	SA_disconnect_fct	disconnect;	/* Close a connection. */
	SA_write_msg_fct	write_msg;	/* Write a new message on this connection */
};

/* Known STREAM_API implementations */

extern STREAM_API tcp_functions;	/* Plain/open streams */
#ifdef DDS_SECURITY
extern STREAM_API tls_functions;	/* Stream, encrypted using TLS (openssl) */
#endif

/* Callbacks a STREAM_API implementation must use to inform the caller of certain events. */

/* Pending TCP server connections.
   These are created directly after accept() in order to be able to receive
   initial control messages.  This is a preliminary stage connection since we
   can't actually determine yet what the purpose of the connection is.  It could be used
   either as a control connection or as a data connection or as both depending
   on the type of control message that are received. */

struct tcp_fd_st {
	int		fd;		/* Socket. */
	unsigned char	dst_addr [16];	/* Destination address. */
	uint32_t	dst_port;	/* Destination port. */
	Timer_t		timer;		/* Recognition timer. */
	TCP_FD		*next;		/* Next in list. */
	IP_CX		*parent;	/* Server connection. */
	TCP_MSG		recv_msg;	/* Support receiving of partial messages. */
	void            *sproto;        /* TLS_CX context */
};

typedef IP_CX *(* SA_on_new_connection_cb) (TCP_FD *pp, unsigned char* msg, size_t size);

/* 'Promote' a pending context to a full blown IP_CX and deliver the first message on that connection
    The contained file descriptor has been set to non-blocking.
    Return value:
      a valid IP_CX ptr: The pending part of this connection will be cleaned up and the IP_CX object will
                         be prepared for receiving (more) messages.
      NULL: The pending context will be cleaned up and the socket will be closed by the STREAM_API implementation.*/

typedef void (* SA_on_connected_cb) (IP_CX *cxp);

/* A pending connect() is successfully completed. */

typedef void (* SA_on_write_completed_cb) (IP_CX *cxp);

/* A pending write for cxp has just been finished. The user can choose to ignore this fact (either by not setting the
   callback or returning immediately in the callback implementation). It is allowed to start sending a new message
   from within the callback. */

typedef int (* SA_on_new_message_cb) (IP_CX *cxp, unsigned char *msg, size_t size);

/* A new TCP message has been arrived for this context. The data behind msg will
   be freed immediately after the callback finishes. If one needs the data
   thereafter, a private copy should be made of it.
   The callback function should return a non-0 value if processing should continue
   afterwards. */

typedef void (* SA_on_close_cb) (IP_CX *cxp);

/* A close or other fatal error has been detected on the associated fd. There is no data cleaned up (yet). The user
   of the STREAM_API should call the disconnect function as appropriate. */

struct stream_cb_st {
	SA_on_new_connection_cb		on_new_connection;	/* (server only) The first message on a new connection has been received. */
	SA_on_connected_cb		on_connected;		/* (client only) The connect() is successful. */
	SA_on_write_completed_cb	on_write_completed;	/* (optional) A pending write has now been completed. */
	SA_on_new_message_cb		on_new_message;		/* A new message has been received. */
	SA_on_close_cb			on_close;		/* The remote side closed this connection. */
};

#endif

