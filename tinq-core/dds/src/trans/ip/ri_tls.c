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

/* ri_tls.c -- RTPS over SSL/TLS transports. */

#if defined (DDS_SECURITY) && defined (DDS_TCP)

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
#include "ri_data.h"
#include "ri_tls.h"
#include "ri_tcp.h"
#include "ri_tcp_sock.h"
#include "dds/dds_security.h"

#ifdef DDS_TCP_NODELAY
#include <netinet/tcp.h>
#endif

#ifdef ANDROID
#include "sys/socket.h"
#endif

int	tls_available = 1;
int     server_mode = 0;

#define COOKIE_SECRET_LENGTH	16

static SSL_CTX		*tls_server_ctx;
static SSL_CTX		*tls_client_ctx;
static int		cookie_initialized;
static unsigned char	cookie_secret [COOKIE_SECRET_LENGTH];

typedef enum {
	TLS_CLIENT,
	TLS_SERVER
} TLS_CX_SIDE;

typedef struct {
	SSL		*ssl;		/* SSL context. */
	int             fd;
	TLS_SSL_STATE   state;
	int             ssl_pending_possible;
	TCP_MSG		*recv_msg;	/* Context in which we can asynchronously receive messages. */
	TCP_MSG		*send_msg;	/* Context in which we can asynchronously send messages. */
} TLS_CX;

/***********/
/* TRACING */
/***********/

#ifdef DDS_DEBUG
/*#define LOG_CERT_CHAIN_DETAILS ** trace if certificates are expired or not */
/*#define TRACE_STATE_CHANGES	** trace all the fd event state changes */
/*#define TRACE_ENTER_FUNCTION	** trace all the function calls */ 
/*#define TRACE_TLS_SPECIFIC	** trace tls specific calls */
/*#define TRACE_POLL_EVENTS	** Trace poll events */
/*#define TRACE_READ_OPS	** Trace read() calls */
/*#define TRACE_WRITE_OPS	** Trace write()/send() calls */
/*#define TRACE_SERVER		** Trace server side operations (listen, accept ...) */
/*#define TRACE_CLIENT		** Trace client side operations (connect, ...) */
#endif

#ifdef TRACE_STATE_CHANGES

static void trace_poll_states (char *state, int to, int fd, const char *func) 
{
	ARG_NOT_USED (func)
	log_printf (RTPS_ID, 0, "TLS: changing from %s to %d on [%d]\r\n", state, to, fd);
}

#define trace_state(arg1, arg2, arg3, arg4) trace_poll_states (arg1, arg2, arg3, arg4);
#else
#define trace_state(arg1, arg2, arg3, arg4)
#endif

#ifdef TRACE_ENTER_FUNCTION
#define trace_func() log_printf (RTPS_ID, 0, "=== Entering function %s ===\r\n", __FUNCTION__);
#else
#define trace_func()
#endif

#ifdef TRACE_TLS_SPECIFIC
#define trace_tls(msg, args) log_printf (RTPS_ID, 0, msg, (args))
#else
#define trace_tls(msg, args)
#endif

#ifdef TRACE_POLL_EVENTS
#define trace_poll_events(fd,events,arg) log_printf (RTPS_ID, 0, "tls-poll [%d] %s: revents=%s arg=%p\r\n", (fd), __FUNCTION__, dbg_poll_event_str ((events)), (arg))
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
static void trace_rw(const char *rw, ssize_t res,int fd, size_t sz) 
{
	int saved = ERRNO;

	log_printf (RTPS_ID, 0, "tls-%s [%d] s:%lu r:%ld", rw, fd, 
				(unsigned long) sz, (long) res);
	trace_log_errno (res, saved);
	log_printf (RTPS_ID, 0, "\r\n");
	ERRNO = saved;
}
#endif

#if defined (TRACE_SERVER) || defined (TRACE_CLIENT)
static void trace_call (const char *op, int res, int fd)
{
	int saved = ERRNO;

	log_printf (RTPS_ID, 0, "tls-%s [%d] r:%d", op, fd, res);
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


/***********/
/* OPENSSL */
/***********/

static void tls_dump_openssl_error_stack (const char *msg)
{
	unsigned long	err;
	const char	*file, *data;
	int		line, flags;
	char		buf [256];

	log_printf (RTPS_ID, 0, "%s", msg);
	while ((err = ERR_get_error_line_data (&file, &line, &data, &flags)))
		log_printf (RTPS_ID, 0, "    err %lu @ %s:%d -- %s\r\n", err, file, line,
				ERR_error_string (err, buf));
}

static int tls_verify_callback (int ok, X509_STORE_CTX *store)
{
	char	data[256];
	int	depth;
	X509	*cert;

	trace_func ();

	depth = X509_STORE_CTX_get_error_depth (store);
	cert = X509_STORE_CTX_get_current_cert (store);
#ifdef LOG_CERT_CHAIN_DETAILS
	if (cert) {
		X509_NAME_oneline (X509_get_subject_name (cert), data, sizeof (data));
		log_printf (RTPS_ID, 0, "TLS: depth %2i: subject = %s\r\n", depth, data);
		X509_NAME_oneline (X509_get_issuer_name (cert), data, sizeof (data));
		log_printf (RTPS_ID, 0, "TLS:           issuer  = %s\r\n", data);
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
			log_printf (SEC_ID, 0, "TLS: Certificate verify callback. The certificate is not yet valid, but this is allowed. \r\n");
#endif
			ok = 1;
		}
		if (err == X509_V_ERR_CERT_HAS_EXPIRED) {
			ok = 1;
#ifdef LOG_CERT_CHAIN_DETAILS
			log_printf (SEC_ID, 0, "TLS: Certificate verify callback. The certificate has expired, but this is allowed. \r\n");
#endif
		}
#endif
	}
	return (ok);
}

#if 1
static int generate_cookie (SSL *ssl, unsigned char *cookie, unsigned *cookie_len)
{
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength;
	union {
		struct sockaddr_storage ss;
		struct sockaddr_in6 s6;
		struct sockaddr_in s4;
	} peer;

	log_printf (RTPS_ID, 0, "TLS: Generate cookie.\r\n");

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
			length += sizeof (peer.s4.sin_port);
			break;

		case AF_INET6:
			length += sizeof (struct in6_addr);
			length += sizeof (peer.s4.sin_port);
			break;

		default:
			OPENSSL_assert (0);
			break;
	}
	buffer = (unsigned char*) OPENSSL_malloc (length);
	if (!buffer)
		fatal_printf ("DDS: generate_cookie (): out of memory!");

	switch (peer.ss.ss_family) {
		case AF_INET:
			memcpy (buffer, &peer.s4.sin_port, sizeof (peer.s4.sin_port));
			memcpy (buffer + sizeof (peer.s4.sin_port), &peer.s4.sin_addr, sizeof (struct in_addr));
			break;

		case AF_INET6:
			memcpy (buffer, &peer.s6.sin6_port, sizeof (peer.s6.sin6_port));
			memcpy (buffer + sizeof (peer.s6.sin6_port), &peer.s6.sin6_addr, sizeof (struct in6_addr));
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

	log_printf (RTPS_ID, 0, "TLS: Verify cookie.\r\n");

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
			length += sizeof (peer.s4.sin_port);
			break;

		case AF_INET6:
			length += sizeof (struct in6_addr);
			length += sizeof (peer.s6.sin6_port);
			break;

		default:
			OPENSSL_assert (0);
			break;
	}
	buffer = (unsigned char*) OPENSSL_malloc (length);
	if (!buffer)
		fatal_printf ("TLS: verify_cookie(): out of memory!");

	switch (peer.ss.ss_family) {
		case AF_INET:
			memcpy (buffer, &peer.s4.sin_port, sizeof (peer.s4.sin_port));
			memcpy (buffer + sizeof (peer.s4.sin_port), &peer.s4.sin_addr, sizeof (struct in_addr));
			break;

		case AF_INET6:
			memcpy (buffer, &peer.s6.sin6_port, sizeof (peer.s6.sin6_port));
			memcpy (buffer + sizeof (peer.s6.sin6_port), &peer.s6.sin6_addr, sizeof (struct in6_addr));
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
	
	return (0);
}
#endif

#define DEPTH_CHECK 5

/* Fill the SSL_CTX with certificate and key */
static SSL_CTX *create_ssl_tls_ctx (TLS_CX_SIDE purpose)
{
	SSL_CTX *ctx = NULL;
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
	static int warned = 0;

	trace_func ();

	switch (purpose) 
	{
		case TLS_CLIENT:
			if ((ctx = SSL_CTX_new (TLSv1_client_method ())) == NULL)
				fatal_printf ("TLS - %s () - Failed to create new SSL context", __FUNCTION__);
			break;
		case TLS_SERVER:
			if ((ctx = SSL_CTX_new (TLSv1_server_method ())) == NULL)
				fatal_printf ("TLS - %s () - Failed to create new SSL context", __FUNCTION__);
			break;
	        default:
			fatal_printf ("TLS - %s () - invalid purpose for new SSL context", __FUNCTION__);
			break;
	}

	if (!SSL_CTX_set_cipher_list (ctx, "AES:!aNULL:!eNULL"))
		fatal_printf ("TLS %s (): failed to set cipher list", __FUNCTION__);

	SSL_CTX_set_session_cache_mode (ctx, SSL_SESS_CACHE_OFF);

	if (get_certificate (&cert, local_identity) ||
	    !SSL_CTX_use_certificate (ctx, cert)) {
		if (!warned) {
			warn_printf ("TLS: no client certificate found!");
			warned = 1;
		}
		SSL_CTX_free (ctx);
		return (NULL);
	}

	/* for the client we have to add the client cert to the trusted ca certs */
	if (purpose == TLS_CLIENT) {
		/* Add extra cert does not automatically up the reference */
		SSL_CTX_add_extra_chain_cert (ctx, cert);
		cert->references ++;
	}

	if (get_nb_of_CA_certificates (&nbOfCA, local_identity) ||
	    nbOfCA == 0)
		fatal_printf ("TLS: Did not find any trusted CA certificates");
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
	
	if (get_private_key (&privateKey, local_identity) ||
	    !SSL_CTX_use_PrivateKey (ctx, privateKey))
		fatal_printf ("TLS: no private key found!");
       
	SSL_CTX_set_verify (ctx,
			    SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			    tls_verify_callback);	
			
	/*Check the private key*/
	if (!SSL_CTX_check_private_key (ctx))
		fatal_printf ("TLS: invalid private key!");

	SSL_CTX_set_verify_depth (ctx, DEPTH_CHECK);
	SSL_CTX_set_read_ahead (ctx, 1);
	if (purpose == TLS_SERVER) {
		SSL_CTX_set_cookie_generate_cb (ctx, generate_cookie);
		SSL_CTX_set_cookie_verify_cb (ctx, verify_cookie);
	}

#ifdef DDS_NATIVE_SECURITY
	X509_free (cert);
	sk_X509_pop_free (ca_cert_list, X509_free);
	EVP_PKEY_free (privateKey);
#endif

	return (ctx);
}

/* Initialize the ssl tls server and client contexts */

static int initialize_ssl_tls_ctx (void)
{
	log_printf (RTPS_ID, 0, "TLS: Contexts initialized.\r\n");
	dds_ssl_init ();

	tls_server_ctx = create_ssl_tls_ctx (TLS_SERVER);
	if (!tls_server_ctx)
		return (1);

	tls_client_ctx = create_ssl_tls_ctx (TLS_CLIENT);
	if (!tls_client_ctx) {
		SSL_CTX_free (tls_server_ctx);
		return (1);
	}
	return (0);
}

/* Cleanup of the tls server and client contexts */
static void cleanup_ssl_tls_ctx (void)
{
	SSL_CTX_free (tls_client_ctx);
	SSL_CTX_free (tls_server_ctx);
	tls_client_ctx = tls_server_ctx = NULL;
	dds_ssl_finish ();
	log_printf (RTPS_ID, 0, "TLS: Contexts freed.\r\n");
}

/***********/
/* TLS CTX */
/***********/

/* create a tls ctx and a new ssl object */
static TLS_CX *create_tls_ctx (int fd, TLS_CX_SIDE role)
{
	TLS_CX *tls;
#ifdef __APPLE__
	int yes = 1;
#endif

	trace_func ();
	trace_tls("TLS: Create TLS context [%d].\r\n", fd);

	tls = xmalloc (sizeof (TLS_CX));
	if (!tls) {
		err_printf ("%s (): out of memory for TLS context!", __FUNCTION__);
		return (NULL);
	}
	if (role == TLS_SERVER) {
		if ((tls->ssl = SSL_new (tls_server_ctx)) == NULL) {
			err_printf ("%s (): failed to alloc SSL connection context", __FUNCTION__);
			goto fail;
		}
	}
	else {
		if ((tls->ssl = SSL_new (tls_client_ctx)) == NULL) {
			err_printf ("%s (): failed to alloc SSL connection context", __FUNCTION__);
			goto fail;
		}
	}

	tls->fd = fd;
	tls->state = SSL_NONE;
	tls->ssl_pending_possible = 0;
	tls->recv_msg = NULL;
	tls->send_msg = NULL;

#ifdef __APPLE__

	/* MSG_NOSIGNAL does not exist for Apple OS, but a equivalent socket option is available */
	if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) < 0)
		err_printf("%s: setsockopt()", __FUNCTION__);
#endif
	SSL_set_fd (tls->ssl, fd);
	return (tls);
 fail:
	xfree (tls);
	return (NULL);
}

int tls_do_connect (TCP_CON_REQ_ST *p);

void tls_cleanup_ctx (IP_CX *cxp)
{
	sock_fd_remove_socket (cxp->fd);
	cxp->stream_cb->on_close (cxp);
}

static void pollout_on (int fd)
{
	trace_state ("POLLOUT", 1, fd, __FUNCTION__);
	sock_fd_event_socket (fd, POLLOUT, 1);
}

static void pollout_off (int fd)
{
	trace_state ("POLLOUT", 0, fd, __FUNCTION__);
	sock_fd_event_socket (fd, POLLOUT, 0);
}

static void pollin_on (int fd)
{
	trace_state ("POLLIN", 1, fd, __FUNCTION__);
	sock_fd_event_socket (fd, POLLIN, 1);
}

static void pollin_off (int fd)
{
	trace_state ("POLLIN", 0, fd, __FUNCTION__);
	sock_fd_event_socket (fd, POLLIN, 0);
}

static TLS_CX *get_tls_ctx (IP_CX *cxp) 
{
	TLS_CX *tls = (TLS_CX *) cxp->sproto;
	
	trace_func ();
	if (!tls) {
		if (cxp->paired && cxp->fd == cxp->paired->fd) {
			tls = cxp->paired->sproto;
			if (!tls) {
				log_printf (RTPS_ID, 0, "TLS: no valid tls ctx found for [%d] via shared", cxp->fd);
				tls = NULL;
			}
		}
		else {
			/* this could happen on the ctrl channel when a write fails while also reading */
			log_printf (RTPS_ID, 0, "TLS: no valid tls ctx found for [%d]", cxp->fd);
			tls = NULL;
		}
	}

#if 0
	if (cxp->fd_owner) {
		if (cxp->sproto != tls)
			warn_printf ("TLS: the tls context is not attached to the owner");
	} else if (cxp->paired && cxp->paired->fd_owner) {
		if (cxp->paired->sproto != tls)
			warn_printf ("TLS: the tls context is not attached to the owner");
	} else
		warn_printf ("TLS: There is no fd owner");
#endif

	return (tls);
}

static void handle_pollout (IP_CX *cxp)
{
	TLS_CX *tls = get_tls_ctx (cxp);

	if (!tls)
		return;

	/* If WANT_WRITE then set POLLOUT = 1 and POLLIN = 0 */
	if (tls->state == SSL_WRITE_WANT_WRITE || 
	    tls->state == SSL_READ_WANT_WRITE) {
#ifdef TRACE_STATE_CHANGES
		log_printf (RTPS_ID, 0, "TLS: POLLOUT = 1, POLLIN = 0\r\n");
#endif
		pollout_on (cxp->fd);
		pollin_off (cxp->fd);
	}
	/* If WANT_READ then set POLLOUT = 0 and POLLIN = 1 */
	else if (tls->state == SSL_READ_WANT_READ ||
		 tls->state == SSL_WRITE_WANT_READ) {
#ifdef TRACE_STATE_CHANGES
		log_printf (RTPS_ID, 0, "TLS: POLLOUT = 0, POLLIN = 1\r\n");
#endif
		pollout_off (cxp->fd);
		pollin_on (cxp->fd);
	}
	/* Error case then remove socket */
	else if (tls->state == SSL_ERROR) {
#ifdef TRACE_STATE_CHANGES
		log_printf (RTPS_ID, 0, "TLS: sock_fd_remove_sock [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif
		sock_fd_remove_socket (cxp->fd);
	}
	/* Default POLLOUT = 0 and POLLIN = 1 */
	else {
#ifdef TRACE_STATE_CHANGES
		log_printf (RTPS_ID, 0, "TLS: POLLOUT = 0, POLLIN = 1\r\n");
#endif
		pollout_off (cxp->fd);
		pollin_on (cxp->fd);
	}
}

static void tls_receive_message (IP_CX *cxp);

/* Write/send (the next part of) a msg (fragment). */
static void tls_write_msg_fragment (IP_CX *cxp)
{
	ssize_t		n;
	unsigned char	*sp;
	TLS_CX          *tls = get_tls_ctx (cxp);
	int             len, error;

	trace_func ();
	
	if (!tls)
		return;

	trace_tls ("TLS: completing a pending write for fd [%d].\r\n", cxp->fd);
	sp = tls->send_msg->buffer + tls->send_msg->used;
	while (tls->send_msg->used < tls->send_msg->size) {
		len = tls->send_msg->size - tls->send_msg->used;
		n = SSL_write (tls->ssl, sp, len);
		if (n > 0) {
			tls->send_msg->used += n;
			sp += n;
		}
		error = SSL_get_error (tls->ssl, n);
		trace_write (n, cxp->fd, len);
		/* log_printf (RTPS_ID, 0, "in %s\r\n", __FUNCTION__); */

		switch (error) {
		case SSL_ERROR_NONE:
			/* Write successful. */
#ifdef MSG_TRACE
			if (cxp->trace)
				rtps_ip_trace (cxp->handle, 'T',
					       &cxp->locator->locator,
					       cxp->dst_addr, cxp->dst_port,
					       len);
#endif
			trace_tls ("SSL_write: Error none for fd [%d]\r\n", cxp->fd);
			break;
		case SSL_ERROR_WANT_WRITE:
			trace_tls ("SSL_write: ERROR want write for fd [%d]\r\n", cxp->fd);
			tls->state = SSL_WRITE_WANT_WRITE;
			handle_pollout (cxp);
			return;
		case SSL_ERROR_WANT_READ:
			trace_tls ("SSL_write: ERROR want read for fd [%d]\r\n", cxp->fd);
			tls->state = SSL_WRITE_WANT_READ;
			handle_pollout (cxp);
			return;
		case SSL_ERROR_ZERO_RETURN:
			trace_tls ("SSL_write: ERROR zero return for fd [%d]\r\n",cxp->fd);
			handle_pollout (cxp);
			tls_cleanup_ctx (cxp);
			return;
		case SSL_ERROR_SSL:
		default:
			trace_tls ("SSL_write: ERROR for fd [%d]\r\n", cxp->fd);
			tls_dump_openssl_error_stack ("SSL_write () error");
			xfree (tls->send_msg->buffer);
			xfree (tls->send_msg);
			tls->send_msg = NULL;
			tls->state = SSL_ERROR;
			handle_pollout (cxp);
			tls_cleanup_ctx (cxp);
			return;
		}
	}

	xfree (tls->send_msg->buffer);
	xfree (tls->send_msg);
	tls->send_msg = NULL;

	/* Change own state to none */
	tls->state = SSL_NONE;
	handle_pollout (cxp);

	/* If we wanted to read during prev write, retry */
	if (tls->ssl_pending_possible) {
		tls->ssl_pending_possible = 0;
		tls_receive_message (cxp);
	}

	if (cxp->stream_cb->on_write_completed)
		cxp->stream_cb->on_write_completed (cxp);
	else if (cxp->paired &&	
		 cxp->paired->fd == cxp->fd &&
		 cxp->paired->stream_cb->on_write_completed)
		cxp->paired->stream_cb->on_write_completed (cxp->paired);
}


/* Receive (the next part of) a msg (fragment).
   A message is complete when msg->read == msg->size; the data is then available in msg->buffer.
   It is assumed that fd is set to nonblocking beforehand.
   Returns:
     0: success (the read(s) went well, msg not necessarilly complete).
    -1: Some problem with read (see ERRNO for details). Connection should most
        probably be closed.
    -2: The context is already deleted somewhere else, no double deletion. */
static int tls_receive_message_fragment (IP_CX *cxp, TCP_MSG *msg)
{
	unsigned char	*dp;
	ssize_t		n;
	uint32_t	l;
	int error;
	TLS_CX *tls = get_tls_ctx (cxp);

	trace_func ();

	if (!tls && cxp->fd)
		return (-1);

	if (!tls && !cxp->fd)
		return (-2);

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
		n = SSL_read (tls->ssl, dp, msg->size - msg->used);
		if (n > 0) {
			msg->used += n;
			dp += n;
		}			
		error = SSL_get_error (tls->ssl, n);
		trace_read (n, cxp->fd, msg->size - msg->used);
		switch (error) {
		case SSL_ERROR_NONE:
			trace_tls ("SSL_read: Error none for fd [%d]\r\n", cxp->fd);
			break;
		case SSL_ERROR_ZERO_RETURN:
			trace_tls ("SSL_read: Error zero return for fd [%d]\r\n", cxp->fd);
			goto error;
		case SSL_ERROR_WANT_WRITE:
			trace_tls ("SSL_read: Error want write for fd [%d]\r\n", cxp->fd);
			tls->state = SSL_READ_WANT_WRITE;
			handle_pollout (cxp);
			return (0);
		case SSL_ERROR_WANT_READ:
			trace_tls ("SSL_read: Error want read for fd [%d]\r\n", cxp->fd);
			tls->state = SSL_READ_WANT_READ;
			handle_pollout (cxp);
			return (0);
		default:
			trace_tls ("SSL_read: Error for fd [%d]\r\n", cxp->fd);
			tls_dump_openssl_error_stack ("TLS(C): SSL_read () error:");
			goto error;
		}
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
			goto error;
		}
		msg->buffer = xmalloc (msg->size);
		if (!msg->buffer) {
			msg->size = msg->used = 0;
			goto error;
		}
		memcpy (msg->buffer, &msg->header, sizeof (CtrlHeader));
		dp = msg->buffer + msg->used;
		goto continue_reading;
	}

	if (SSL_pending (tls->ssl))
		log_printf (RTPS_ID, 0, "TLS: SSL_pending says there are %d more data bytes to read.\r\n", SSL_pending (tls->ssl));

	/* Change own state to none */
	tls->state = SSL_NONE;
	handle_pollout (cxp);
	return (0);

 error:
	tls->state = SSL_ERROR;
	handle_pollout (cxp);
	return (-1);

}

static void tls_receive_message (IP_CX *cxp)
{
	TCP_MSG *recv_msg = NULL;
	int	r;
	TLS_CX  *tls = get_tls_ctx (cxp);

	trace_func ();

	if (!tls)
		return;

try_next:
	tls->ssl_pending_possible = 0;
	if (!tls->recv_msg) {

		/* Prepare for receiving messages */
		tls->recv_msg = xmalloc (sizeof (TCP_MSG));
		if (!tls->recv_msg) {
			tls_cleanup_ctx (cxp);
			return;
		}
		tls->recv_msg->size = tls->recv_msg->used = 0;
		tls->recv_msg->buffer = NULL;
	}
	
	r = tls_receive_message_fragment (cxp, tls->recv_msg);
	if (r == -1) {
		tls_cleanup_ctx (cxp);
		return;
	} else if (r == -2) {
		log_printf (RTPS_ID, 0, "The context was already deleted, don't delete it again\r\n");
		return;
	}

	else if (tls->recv_msg->used == tls->recv_msg->size) {
		recv_msg = tls->recv_msg;
		tls->recv_msg = NULL;
		if (cxp->stream_cb->on_new_message)
			r = cxp->stream_cb->on_new_message (cxp,
							    recv_msg->buffer,
							    recv_msg->size);
		else
			r = cxp->paired->stream_cb->on_new_message (cxp,
							    recv_msg->buffer,
							    recv_msg->size);

		xfree (recv_msg->buffer);
		xfree (recv_msg);

		/* If we're not done, check whether another message is present
		   in the SSL buffers. */
		if (r && 
		    (tls->state == SSL_WRITE_WANT_WRITE ||
		     tls->state == SSL_WRITE_WANT_READ)) {
			tls->ssl_pending_possible = 1;
			return;
		}
		if (r)
			goto try_next;
	}
}

static void tls_socket_activity (SOCKET fd, short revents, void *arg)
{
	IP_CX		*cxp = (IP_CX *) arg;
	int		err, r;
	socklen_t	sz;
	TLS_CX          *tls = get_tls_ctx (cxp);

	trace_func ();
	trace_poll_events (fd, revents, arg);

	if (fd != cxp->fd)
		fatal_printf ("TLS: tls_socket_activity: fd != cxp->fd");

	if (!tls)
		return;

	if ((revents & (POLLERR | POLLNVAL)) != 0) {
		sz = sizeof (err);
		r = getsockopt(cxp->fd, SOL_SOCKET, SO_ERROR, &err, &sz);
		if ((r == -1) || err)  {
			log_printf (RTPS_ID, 0, "POLLERR | POLLNVAL [%d]: %d %s\r\n", cxp->fd, err, strerror (err));
			tls_cleanup_ctx (cxp);
			return;
		}
	}

	if ((revents & POLLHUP) != 0) {
		tls_cleanup_ctx (cxp);
		return;
	}
	if ((revents & POLLOUT) != 0) {
		if (tls->state == SSL_WRITE_WANT_WRITE)
			tls_write_msg_fragment (cxp);
		else if (tls->state == SSL_READ_WANT_WRITE)
			tls_receive_message (cxp);
		else
			handle_pollout (cxp);
		return;
	}
	if ((revents & POLLIN) != 0) {
		if (tls->state == SSL_WRITE_WANT_READ)
			tls_write_msg_fragment (cxp);
		else if (tls->state == SSL_READ_WANT_READ)
			tls_receive_message (cxp);
		else if (tls->state == SSL_NONE)
			tls_receive_message (cxp);
		else
			handle_pollout (cxp);
	}
}

static void tls_ssl_connect (SOCKET fd, short revents, void *arg)
{
	IP_CX *cxp = (IP_CX *) arg;
	TLS_CX *tls = (TLS_CX *) cxp->sproto;
	int		error;

	trace_func ();
	trace_tls ("SSL_connect: ssl connect for fd [%d]\r\n", fd);
	log_printf (RTPS_ID, 0, "%p\r\n", (void *) cxp);
	trace_poll_events (fd, revents, arg);
	 
	if ((revents & POLLERR) != 0 || (revents & POLLHUP) != 0) {
		log_printf (RTPS_ID, 0, "tls_ssl_connect received POLLERR | POLLHUP for fd [%d]\r\n", fd);
		tls_cleanup_ctx (cxp);
	}
	else if ((revents & POLLOUT) != 0 || (revents & POLLIN) != 0) {
		
		/* Create a tls context */
		if (!tls) {
			if ((tls = create_tls_ctx (fd, TLS_CLIENT)) == NULL)
				return;
			cxp->sproto = tls;
		}

		error = SSL_get_error (tls->ssl, SSL_connect (tls->ssl));
		switch (error) {
		case SSL_ERROR_NONE:
			trace_tls ("SSL_connect: Connect completed for fd [%d]\r\n", fd);
			cxp->cx_state = CXS_OPEN;
			cxp->stream_cb->on_connected (cxp);

			/* Don't change the events */
			/* update the userdata function */
#ifdef TRACE_STATE_CHANGES
			log_printf (RTPS_ID, 0, "TLS: sock_fd_udata_socket [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif
			sock_fd_udata_socket (cxp->fd, cxp);
			/* update the callback function */
			log_printf (RTPS_ID, 0, "TLS: Set activity(1) -- [%d], owner=%d, cxp=%p\r\n", cxp->fd, cxp->fd_owner, (void *) cxp);
#ifdef TRACE_STATE_CHANGES
			log_printf (RTPS_ID, 0, "TLS: sock_fd_fct_socket [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif
			sock_fd_fct_socket (cxp->fd,
					    tls_socket_activity);
			break;
		case SSL_ERROR_ZERO_RETURN:
			trace_tls ("SSL_connect: Error zero return for fd [%d]\r\n", fd);
			break;
		case SSL_ERROR_WANT_WRITE:
			trace_tls ("SSL_connect: Error want write for fd [%d]\r\n", fd);
			trace_state ("POLLOUT", 1, cxp->fd, __FUNCTION__);
			sock_fd_event_socket (cxp->fd, POLLOUT, 1);
			break;
		case SSL_ERROR_WANT_READ:
			trace_tls ("SSL_connect: Error want read for fd [%d]\r\n", fd);
			trace_state ("POLLOUT", 0, cxp->fd, __FUNCTION__);
			sock_fd_event_socket (cxp->fd, POLLOUT, 0);
			break;
		default:
			trace_tls ("SSL_connect: Default error [%d]\r\n", fd);
			tls_dump_openssl_error_stack ("TLS(C): SSL_connect () error:");
			tls_cleanup_ctx (cxp);
			break;
		}
	}
	/* TODO catch other revents */
}

static void tls_wait_connect_complete (SOCKET fd, short revents, void *arg)
{
	TCP_CON_REQ_ST	*p = (TCP_CON_REQ_ST *) arg;
	IP_CX		*cxp = p->cxp;
	socklen_t	s;
	int		err, r;
	socklen_t	sz;

	trace_func ();
	trace_poll_events (fd, revents, arg);

	p = tcp_clear_pending_connect (p);
	do {
		if ((revents & (POLLERR | POLLNVAL)) != 0) {
			sz = sizeof (err);
			r = getsockopt(cxp->fd, SOL_SOCKET, SO_ERROR, &err, &sz);
			if ((r == -1) || err)  {
				log_printf (RTPS_ID, 0, "POLLERR | POLLNVAL [%d]: %d %s\r\n", cxp->fd, err, strerror (err));
				tls_cleanup_ctx (cxp);
				break;
			}
		}
		if ((revents & POLLHUP) != 0) {
			tls_cleanup_ctx (cxp);
			break;
		}
		if ((revents & POLLOUT) != 0) {
			s = sizeof (err);
			r = getsockopt (cxp->fd, SOL_SOCKET, SO_ERROR, &err, &s);
			if (r || err) {
				warn_printf ("cc_control: getsockopt(SOL_SOCKET/SO_ERROR)");
				perror ("cc_control: getsockopt(SOL_SOCKET/SO_ERROR)");
				tls_cleanup_ctx (cxp);
				break;
			}
			trace_tls ("Connect completed, trying SSL_connect for fd [%d].\r\n", fd);

			/* Don't change the events */
			/* update the userdata function */
#ifdef TRACE_STATE_CHANGES
			log_printf (RTPS_ID, 0, "TLS: sock_fd_udata_socket [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif
			sock_fd_udata_socket (cxp->fd, cxp);
			/* update the callback function */
#ifdef TRACE_STATE_CHANGES
			log_printf (RTPS_ID, 0, "TLS: sock_fd_fct_socket [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif
			sock_fd_fct_socket (cxp->fd,
					    tls_ssl_connect);

			tls_ssl_connect (cxp->fd, POLLOUT, cxp);
		}
	}
	while (0);
	if (p)
		tls_do_connect (p);
}

/********************/
/* CONNECTION SETUP */
/********************/

static void tls_pending_free (TCP_FD *pp)
{
	TCP_FD	*xpp, *prev_pp;

	trace_func ();
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

static void tls_close_pending_connection (TCP_FD *pp)
{
	TLS_CX *tls = (TLS_CX *) pp->sproto;
	int err;
#ifdef TRACE_TLS_SPECIFIC
	int mode;
#endif
#ifdef TRACE_SERVER
	int	r;
#endif
	trace_func ();
	if (tmr_active (&pp->timer))
		tmr_stop (&pp->timer);

	sock_fd_remove_socket (pp->fd);

	if (tls) {
		if (tls->ssl) {
			if (server_mode) {
				err = SSL_shutdown (tls->ssl);
				if (err == 0) {
					shutdown (pp->fd, 1);
					err = SSL_shutdown (tls->ssl);
				}
				switch (err) {
				case 1: 
					break;
				case 0:
				case -1:
				default:
#ifdef TRACE_TLS_SPECIFIC
					log_printf (RTPS_ID, 0, "TLS: socket [%d] not closed gracefully", pp->fd);
#endif
					break;
				}
				SSL_free (tls->ssl);
			} else {
				err = SSL_shutdown (tls->ssl);
				switch (err) {
				case 1: 
					break;
				case 0:
				case -1:
				default:
#ifdef TRACE_TLS_SPECIFIC
					log_printf (RTPS_ID, 0, "TLS: socket [%d] not closed gracefully", pp->fd);
#endif
					break;
				}

				SSL_free (tls->ssl);
			}
		}
		xfree (tls);
	}

#ifdef TRACE_SERVER
	r = close (pp->fd);
	trace_server ("close", r, pp->fd);
#else
	close (pp->fd);
#endif
	pp->fd = 0;
	tls_pending_free (pp);
}

static void tls_pending_timeout (uintptr_t user)
{
	TCP_FD	*pp = (TCP_FD *) user;

	log_printf (RTPS_ID, 0, "tls_pending_timeout: connection close!\r\n");
	tls_close_pending_connection (pp);
}

static void tls_pending_first_message (SOCKET fd, short revents, void *arg)
{
	TCP_FD	*pp = (TCP_FD *) arg;
	IP_CX		*cxp = NULL;
	IP_CX tmp;

	trace_func ();
	trace_poll_events (fd, revents, arg);

	/* Check if connection unexpectedly closed. */
	if ((revents & POLLHUP) != 0) {
		trace_tls ("TLS(Sp): connection error for [%d]!\r\n", fd);
		tls_close_pending_connection (pp);
		return;
	}

	/* we need a tmp CX_IP to keep track of the correct TLS_CX */
	tmp.sproto = pp->sproto;
	tmp.fd = pp->fd;
	tmp.paired = NULL;
	tmp.fd_owner = 1;
	if (tls_receive_message_fragment (&tmp, &pp->recv_msg) == -1) {
		tls_close_pending_connection (pp);
		return;
	}
	if (pp->recv_msg.used != pp->recv_msg.size)
		return; /* message (still) incomplete */

	cxp = pp->parent->stream_cb->on_new_connection (pp, pp->recv_msg.buffer, pp->recv_msg.size);
	xfree (pp->recv_msg.buffer);
	pp->recv_msg.buffer = NULL;
	pp->recv_msg.size = pp->recv_msg.used = 0;
	if (!cxp) {
		/* pending connection could not be 'promoted' to an IP_CX */
		tls_close_pending_connection (pp);
		return;
	}
	cxp->sproto = pp->sproto;
	cxp->cx_state = CXS_OPEN;
		
	/* Don't change the events */
	/* update the userdata function */
#ifdef TRACE_STATE_CHANGES
	log_printf (RTPS_ID, 0, "TLS: sock_fd_udata_socket [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif	
	sock_fd_udata_socket (cxp->fd, cxp);
	/* update the callback function */
	log_printf (RTPS_ID, 0, "TLS: Set activity(2) -- [%d], owner=%d\r\n", cxp->fd, cxp->fd_owner);
#ifdef TRACE_STATE_CHANGES
	log_printf (RTPS_ID, 0, "TLS: sock_fd_fct_socket [%d] (%d)\r\n", cxp->fd, cxp->handle);
#endif
	sock_fd_fct_socket (cxp->fd,
			    tls_socket_activity);
		
	tmr_stop (&pp->timer);
	tls_pending_free (pp);
}

static void tls_ssl_accept (SOCKET fd, short revents, void *arg)
{
	TCP_FD	*pp = (TCP_FD *) arg;
	TLS_CX  *tls = pp->sproto;
	int error;

	trace_func ();
	trace_poll_events (fd, revents, arg);
	trace_tls ("SSL_accept: ssl accept for fd [%d]\r\n", fd);

	if ((revents & POLLOUT) != 0 || (revents & POLLIN) != 0) {

		error = SSL_accept (tls->ssl);
		error = SSL_get_error (tls->ssl, error);
		switch (error) {
		case SSL_ERROR_NONE:
			/* Accept successful. */
#ifdef TRACE_MSGS
			if (rtps_trace && (mp->element.flags & RME_TRACE) != 0) {
				trace_msg ('T', loc2sockaddr (
							      pp->parent->locator->locator.kind,
							      pp->parent->dst_port,
							      pp->parent->dst_addr,
							      sabuf), len);
			}
#endif			
			trace_tls ("SSL_accept: Completed for fd [%d]\r\n", fd);
			pp->parent->cx_state = CXS_OPEN;
			/* Don't change the events */
			/* update the userdata function */
			sock_fd_udata_socket (fd, pp);
			/* update the callback function */
			sock_fd_fct_socket (fd,
					    tls_pending_first_message);
			
			break;
		case SSL_ERROR_WANT_WRITE:
			trace_tls ("SSL_accept: Error want write for fd [%d]\r\n", fd);
			/* SSL_accept wants to write, so it needs to wait for a timeslot
			   that the socket can write data out */
			trace_state ("POLLOUT", 1, pp->fd, __FUNCTION__);
			sock_fd_event_socket (fd, POLLOUT, 1);
			break;
		case SSL_ERROR_WANT_READ:
			trace_tls ("SSL_accept: Error want read for fd [%d]\r\n", fd);
			/* SSL_accept wants to read, so it needs to wait for a timeslot
			   that the socket has data available to read */
			trace_state ("POLLOUT", 0, pp->fd, __FUNCTION__);
			sock_fd_event_socket (fd, POLLOUT, 0);
			break;
		case SSL_ERROR_ZERO_RETURN:
			trace_tls ("SSL_accept: Error zero return for fd [%d]\r\n", fd);
			break;
		case SSL_ERROR_SYSCALL:
			trace_tls ("SSL_accept: Error syscall for fd [%d] --> ", fd);
			if (ERRNO == 0)
				trace_tls ("No syscall error for fd [%d]\r\n", fd);
			else {
				trace_tls ("Unknown error for fd [%d]\r\n", fd);
			}
			break;
		default:
			trace_tls ("SSL_accept: Error other for fd [%d]\r\n", fd);
			tls_dump_openssl_error_stack ("SSL_accept () default\r\n");

			/* The accept has not yet succeeded, so quietly shutdown ssl connection */
			SSL_set_quiet_shutdown (tls->ssl, 1);
			tls_close_pending_connection (pp);
		}
	}
}

static void tls_server_accept (SOCKET fd, short revents, void *arg)
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
	int			r, err;
#ifdef TRACE_TLS_SPECIFIC
	int                     mode;
#endif
#ifdef TRACE_SERVER
	int			ofcmode;
#endif
	TLS_CX			*tls;

	trace_func ();
	trace_poll_events (fd, revents, arg);

	log_printf (RTPS_ID, 0, "TLS: server accept on [%d].\r\n", fd);

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
		warn_printf ("TLS(S): unknown address family!");
		return;
	}

	scxp->cx_state = CXS_LISTEN;
	log_printf (RTPS_ID, 0, "TLS: The socket [%d] is now listening\r\n", scxp->fd);
	r = accept (fd, caddr, &size);
	trace_server ("accept", r, fd);
	if (r < 0) {
		perror ("tls_server_accept: accept()");
		log_printf (RTPS_ID, 0, "tls_server_accept: accept() failed - errno = %d.\r\n", ERRNO);
		return;
	}

#ifdef DDS_TCP_NODELAY
	sock_set_tcp_nodelay (r);
#endif

	/* Create a new tls context */
	if ((tls = create_tls_ctx (r, TLS_SERVER)) == NULL) {
		close (r);
		return;
	}

	/* Create a new pending TCP connection. */
	pp = xmalloc (sizeof (TCP_FD));
	if (!pp) {
		if (tls) {
			if (tls->ssl) {
				if (server_mode) {
					err = SSL_shutdown (tls->ssl);
					if (err == 0) {
						shutdown (r, 1);
						err = SSL_shutdown (tls->ssl);
					}
					switch (err) {
					case 1: 
						break;
					case 0:
					case -1:
					default:
#ifdef TRACE_TLS_SPECIFIC
						log_printf (RTPS_ID, 0, "TLS: socket [%d] not closed gracefully", pp->fd);
#endif
						break;
					}
					SSL_free (tls->ssl);
				} else {
					err = SSL_shutdown (tls->ssl);
					switch (r) {
					case 1: 
						break;
					case 0:
					case -1:
					default:
#ifdef TRACE_TLS_SPECIFIC
						log_printf (RTPS_ID, 0, "TLS: socket [%d] not closed gracefully", pp->fd);
#endif
						break;
					}
					SSL_free (tls->ssl);
				}
			}
			xfree (tls);
		}
#ifdef TRACE_SERVER
		ofcmode = close (r); /* bad reuse of variable in error case */
		trace_server ("close", ofcmode, r);
#else
		close (r);
#endif
		log_printf (RTPS_ID, 0, "TLS(S): allocation failure!\r\n");
		return;
	}
	pp->sproto = tls;
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
		log_printf (RTPS_ID, 0, "TLS(S): unsupported connection family!\r\n");
		return;
	}
	tmr_init (&pp->timer, "TLS-Pending");
	pp->parent = scxp;
	pp->next = scxp->pending;
	scxp->pending = pp;
	pp->recv_msg.used = pp->recv_msg.size = 0;
	pp->recv_msg.buffer = NULL;
	tmr_start (&pp->timer, TICKS_PER_SEC * 2, (uintptr_t) pp, tls_pending_timeout);

	/* Set socket as non-blocking. */
	sock_set_socket_nonblocking (pp->fd);
#ifdef TRACE_STATE_CHANGES
	log_printf (RTPS_ID, 0, "TLS: sock_fd_add_sock [%d] \r\n", pp->fd);
#endif
	sock_fd_add_socket (pp->fd,
			    POLLIN | POLLPRI | POLLHUP | POLLNVAL /*| POLLOUT*/,
			    tls_ssl_accept,
			    pp,
			    "DDS.TLS-A");
	tls_ssl_accept (pp->fd, POLLIN, pp);
}

/**************/
/* STREAM API */
/**************/

/* Start the tls server based on the ip context */

int tls_start_server (IP_CX *scxp)
{
	struct sockaddr_in	sa_v4;
#ifdef DDS_IPV6
	struct sockaddr_in6	sa_v6;
#endif
	struct sockaddr		*sa;
	size_t			size;
	int			fd, r;
	unsigned		family;

	trace_func ();

	/* initialize ssl context */
	if (!tls_server_ctx && initialize_ssl_tls_ctx ())
		return (-1);

	server_mode = 1;

#ifdef DDS_IPV6
	if (scxp->locator->locator.kind == LOCATOR_KIND_TCPv4) {
#endif
		sa_v4.sin_family = family = AF_INET;
		memset (&sa_v4.sin_addr, 0, 4);
		sa_v4.sin_port = htons (scxp->locator->locator.port);
		sa = (struct sockaddr *) &sa_v4;
		size = sizeof (sa_v4);
		log_printf (RTPS_ID, 0, "TCP: Starting secure TCPv4 server on port %d.\r\n", scxp->locator->locator.port);
#ifdef DDS_IPV6
	}
	else {
		sa_v6.sin6_family = family = AF_INET6;
		memset (&sa_v6.sin6_addr, 0, 16);
		sa_v6.sin6_port = htons (scxp->locator->locator.port);
		sa = (struct sockaddr *) &sa_v6;
		size = sizeof (sa_v6);
		log_printf (RTPS_ID, 0, "TCP: Starting secure TCPv6 server on port %d.\r\n", scxp->locator->locator.port);
	}
#endif

	fd = socket (family, SOCK_STREAM, IPPROTO_TCP);
	log_printf (RTPS_ID, 0, "TCP: Secure listening socket created on [%d].\r\n", fd);
	trace_server ("socket", fd, -1);
	if (fd < 0) {
		perror ("TLS: socket()");
		log_printf (RTPS_ID, 0, "TLS: socket() failed - errno = %d.\r\n", ERRNO);
		return (-1);
	}
	
	r = bind (fd, sa, size);
	trace_server ("bind", r, fd);
	if (r) {
		perror ("TLS: bind()");
		log_printf (RTPS_ID, 0, "TLS: bind() failed - errno = %d.\r\n", ERRNO);
		r = close (fd);
		trace_server ("close", r, fd);
		return (-1);
	}

	r = listen (fd, 32);
	trace_server ("listen", r, fd);
	if (r) {
		perror ("TLS: listen()");
		log_printf (RTPS_ID, 0, "TLS: listen() failed - errno = %d.\r\n", ERRNO);
		r = close (fd);
		trace_server ("close", r, fd);
		return (-1);
	}

#ifdef DDS_TCP_NODELAY
	/* Set TCP_NODELAY flag */
	sock_set_tcp_nodelay (fd);
#endif	

	/* Set socket as non-blocking. */
	sock_set_socket_nonblocking (fd);

	/* Secure locator stuff */
	locator_lock (&scxp->locator->locator);
	scxp->locator->locator.flags = LOCF_SECURE | LOCF_SERVER;
	scxp->locator->locator.handle = scxp->handle;
	locator_release (&scxp->locator->locator);

	scxp->cx_type = CXT_TCP_TLS;
	scxp->cx_mode = ICM_ROOT;
	scxp->fd = fd;
	scxp->fd_owner = 1;
	log_printf (RTPS_ID, 0, "TCP: The secure socket [%d] is now listening.\r\n", fd);
	scxp->cx_state = CXS_LISTEN;
#ifdef TRACE_STATE_CHANGES
	log_printf (RTPS_ID, 0, "TLS: sock_fd_add_sock [%d] (%d)\r\n", scxp->fd, scxp->handle);
#endif
	sock_fd_add_socket (scxp->fd,
			    POLLIN | POLLPRI | POLLHUP | POLLNVAL,
			    tls_server_accept,
			    scxp,
			    "DDS.TLS-S");

	log_printf (RTPS_ID, 0, "TCP: Secure server started.\r\n");

	return (0);
}

/* Stop a previously started server */

void tls_stop_server (IP_CX *scxp)
{
	trace_func ();
	log_printf (RTPS_ID, 0, "TCP: Stopping secure server on [%d]\r\n", scxp->fd);

#ifdef TRACE_STATE_CHANGES
	log_printf (RTPS_ID, 0, "TLS: sock_fd_remove_sock [%d] (%d)\r\n", scxp->fd, scxp->handle);
#endif
	sock_fd_remove_socket (scxp->fd);
	if (close (scxp->fd))
		warn_printf ("TCP: Could not close [%d]", scxp->fd);

	scxp->fd = 0;
	scxp->fd_owner = 0;
	log_printf (RTPS_ID, 0, "TCP: Secure server stopped.\r\n");
	cleanup_ssl_tls_ctx ();
}

int tls_do_connect (TCP_CON_REQ_ST *p)
{
	TCP_CON_LIST_ST		*hp;
	struct sockaddr_in	sa_v4;
#ifdef DDS_IPV6
	struct sockaddr_in6	sa_v6;
#endif
	struct sockaddr		*sa;
	socklen_t		len;
	unsigned		family;
	int			fd, r, err;
	short			events;

	trace_func ();
	if (!tls_client_ctx) {
		if (initialize_ssl_tls_ctx ())
    		return (-2);
	}
	do {
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
			memcpy (&sa_v6.sin6_addr, hp->locator.address, 16);
			sa_v6.sin6_port = htons (hp->locator.port);
			sa = (struct sockaddr *) &sa_v6;
			len = sizeof (sa_v6);
		}
#endif
		else
			return (-2);

		fd = socket (family, SOCK_STREAM, IPPROTO_TCP);
		trace_client ("socket", fd, -1);
		if (fd < 0) {
			perror ("tls_connect: socket()");
			log_printf (RTPS_ID, 0, "tls_connect: socket() failed - errno = %d.\r\n", ERRNO);
			return (-2);
		}
		log_printf (RTPS_ID, 0, "TLS: created a new socket on [%d] (%p).\r\n", fd, (void *) p->cxp);
		p->cxp->fd = fd;
		p->cxp->fd_owner = 1;

#ifdef DDS_TCP_NODELAY
		/* Set TCP_NODELAY flag */
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
					perror ("tls_connect: connect()");
					log_printf (RTPS_ID, 0, "tls_connect: connect() failed - errno = %d.\r\n", ERRNO);
					close (fd);
					p->cxp->fd = 0;
					p->cxp->fd_owner = 0;
					trace_server ("close", r, fd);
				}
				else {
					log_printf (RTPS_ID, 0, "TLS: connecting to server [%d] ...\r\n", fd);
					p->cxp->cx_state = CXS_CONNECT;
					sock_fd_add_socket (fd, events | POLLOUT, tls_wait_connect_complete, p, "DDS.TLS-C");
					return (-1);
				}
			}
			else {
				log_printf (RTPS_ID, 0, "TLS: Connected to server on [%d]\r\n", fd);
				p->cxp->cx_state = CXS_SAUTH;
				sock_fd_add_socket (fd, events, tls_ssl_connect, p->cxp, "DDS.TLS-C");
				tls_ssl_connect (fd, events, p->cxp);
			}
			break;
		}
		p = tcp_clear_pending_connect (p);
	}
	while (p);
	return (r);
}

int tls_connect (IP_CX *cxp, unsigned port)
{
	log_printf (RTPS_ID, 0, "Put (%p) in the connect queue\r\n", (void *) cxp);

	return (tcp_connect_enqueue (cxp, port, tls_do_connect));
}

void tls_disconnect (IP_CX *cxp)
{
	TCP_CON_REQ_ST	*next_p = NULL;
	TLS_CX		*tls;
	int		r;
#ifdef TRACE_TLS_SPECIFIC
	int             mode;
#endif
	trace_func ();

	log_printf (RTPS_ID, 0, "TLS: disconnect [%d] (%d) (owner:%d)\r\n", cxp->fd, cxp->handle, cxp->fd_owner);

	if (cxp->fd_owner)
		sock_fd_remove_socket (cxp->fd);
	if (cxp->cx_state == CXS_CONREQ || cxp->cx_state == CXS_CONNECT) {
		next_p = tcp_pending_connect_remove (cxp);
		if (cxp->cx_state == CXS_CONREQ) {
			if (next_p)
				tls_do_connect (next_p);
			return;
		}
	}

	if ((tls = (TLS_CX *) cxp->sproto) != NULL) {
		cxp->sproto = NULL;
		if (tls->ssl) {

			if (server_mode) {
				r = SSL_shutdown (tls->ssl);
				if (r == 0) {
					shutdown (cxp->fd, 1);
					r = SSL_shutdown (tls->ssl);
				}
				switch (r) {
				case 1: 
					break;
				case 0:
				case -1:
				default:
#ifdef TRACE_TLS_SPECIFIC
					log_printf (RTPS_ID, 0, "TLS: socket [%d] not closed gracefully\r\n", cxp->fd);
#endif
					break;
				}
				SSL_free (tls->ssl);
			} else {
				r = SSL_shutdown (tls->ssl);
				switch (r) {
				case 1: 
					break;
				case 0:
				case -1:
				default:
#ifdef TRACE_TLS_SPECIFIC
					log_printf (RTPS_ID, 0, "TLS: socket [%d] not closed gracefully\r\n", cxp->fd);
#endif
					break;
				}
				SSL_free (tls->ssl);
			}

			/* Cleanup the send and recv buffers */
			if (tls->recv_msg) {
				if (tls->recv_msg->buffer) {
					xfree (tls->recv_msg->buffer);
					tls->recv_msg->buffer = NULL;
				}
				xfree (tls->recv_msg);
				tls->recv_msg = NULL;
			}
			if (tls->send_msg) {
				if (tls->send_msg->buffer) {
					xfree (tls->send_msg->buffer);
					tls->send_msg->buffer = NULL;
				}
				xfree (tls->send_msg);
				tls->send_msg = NULL;
			}
		}
		xfree (tls);
	}
	do {
		r = close (cxp->fd);
		trace_server ("close", r, cxp->fd);
	}
	while (r == -1 && ERRNO == EINTR);

	log_printf (RTPS_ID, 0, "TLS: The socket [%d] (%d) is now closed\r\n", cxp->fd, cxp->handle);

	/*if (cxp->timer && tmr_active (cxp->timer))
	  tmr_stop (cxp->timer);*/

	cxp->fd = 0;
	cxp->fd_owner = 0;
	
	if (next_p)
		tls_do_connect (next_p);
}

static WR_RC tls_enqueue (IP_CX         *cxp,
			  unsigned char *msg,
			  unsigned char *sp,
			  size_t        left)
{
	TCP_MSG		**send_msg, *p;
	TLS_CX          *tls = get_tls_ctx (cxp);

	/* Save message in current context -- fd owner or not! */
	send_msg = &tls->send_msg;

	/* Make arrangements for fragmented write. */
	p = *send_msg = xmalloc (sizeof (TCP_MSG));
	if (!p) {
		xfree (msg);
		return (WRITE_ERROR);
	}
	p->msg = msg;
	p->buffer = sp;
	p->size = left;
	p->used = 0;
	return (WRITE_PENDING);
}

WR_RC tls_write_msg (IP_CX *cxp, unsigned char *msg, size_t len)
{
	TLS_CX          *tls = get_tls_ctx (cxp);
	ssize_t		n;
	size_t		left;
	unsigned char	*sp = msg;
	int             error;
	WR_RC		rc;

	trace_func ();

	if (!tls) {
		xfree (msg);
		if (cxp && cxp->fd) {
			sock_fd_remove_socket (cxp->fd);
		}
		return (WRITE_FATAL);
	}
	if (tls->send_msg) {
		xfree (msg);
		return (WRITE_BUSY);
	}
	if (cxp->cx_state != CXS_OPEN) {
		trace_tls ("SSL_write: Socket [%d] is not open.\r\n", cxp->fd);
		xfree (msg);
		return (WRITE_BUSY);
	}
	left = len;
	while (left) {
		/* We're using send() here iso write() so that we can indicate to the kernel *not* to send
		   a SIGPIPE in case the other already closed this connection. A return code of -1 and ERRNO
		   to EPIPE is given instead */

		n = SSL_write (tls->ssl, sp, left);
		if (n > 0) {
			left -= n;
			sp += n;
		}
		error = SSL_get_error (tls->ssl, n);
		trace_write (n, cxp->fd, left);
		/*log_printf (RTPS_ID, 0, "in %s\r\n", __FUNCTION__);*/

		switch (error) {
			case SSL_ERROR_NONE:
				/* Write successful. */
#ifdef MSG_TRACE
				if (cxp->trace)
					rtps_ip_trace (cxp->handle, 'T',
						       &cxp->locator->locator,
						       cxp->dst_addr, cxp->dst_port,
						       len);
#endif
				trace_tls ("SSL_write: Error none for [%d]\r\n", cxp->fd);
				break;
		        case SSL_ERROR_WANT_WRITE:
				trace_tls ("SSL_write: ERROR want write for [%d]\r\n", cxp->fd);
				rc = tls_enqueue (cxp, msg, sp, left);
				if (rc == WRITE_PENDING) {
					tls->state = SSL_WRITE_WANT_WRITE;
					handle_pollout (cxp);
				}
				return (rc);
		        case SSL_ERROR_WANT_READ:
				/* This can happen when a renegotiation of handshake happens
				   So we can't ignore this */
				trace_tls ("SSL_write: ERROR want read for [%d]\r\n", cxp->fd);
				rc = tls_enqueue (cxp, msg, sp, left);
				if (rc == WRITE_PENDING) {
					tls->state = SSL_WRITE_WANT_READ;
					handle_pollout (cxp);
				}
				return (rc);
			case SSL_ERROR_ZERO_RETURN:
				trace_tls ("SSL_write: ERROR zero return for [%d]\r\n",cxp-> fd);
				goto error;
		        case SSL_ERROR_SSL:
			default:
				trace_tls ("SSL_write: Error for [%d]\r\n", cxp->fd);
				tls_dump_openssl_error_stack ("SSL_write () error");
				goto error;
		}
	}

	/* Change own state to none */
	tls->state = SSL_NONE;
	handle_pollout (cxp);
	xfree (msg);
	return (WRITE_OK);

    error:
	xfree (msg);
	tls->state = SSL_ERROR;
	handle_pollout (cxp);
	return (WRITE_FATAL);
}

/* Initialize the SSL/TLS support. */

void rtps_tls_init (void)
{
	log_printf (RTPS_ID, 0, "TLS: Initialization.\r\n");
	initialize_ssl_tls_ctx ();
}

/* Finalize the SSL/TLS support. */

void rtps_tls_finish (void)
{
	log_printf (RTPS_ID, 0, "TLS: Cleanup.\r\n");
	cleanup_ssl_tls_ctx ();
}

STREAM_API tls_functions = {
	tls_start_server,
	tls_stop_server,
	tls_connect,
	tls_disconnect,
	tls_write_msg
};

#else

int tls_available = 0;

#endif

