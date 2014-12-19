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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright (c) Jarkko Turkulainen 2003. All rights reserved. 
 *   See the 'sscep License' chapter in the file COPYRIGHT for copyright notice
 *   and original licensing information.
 */


#ifndef SSCEP_H_
#define SSCEP_H_

#include "conf.h"
#include "sscep_api.h"
#include <qeo/log.h>

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h> 
#include <errno.h> 
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <openssl/asn1_mac.h>

/* Global defines */

#define	VERSION	"20081211"


/* SCEP MIME headers */
#define MIME_GETCA	"application/x-x509-ca-cert"
#define MIME_GETCA_RA	"application/x-x509-ca-ra-cert"
/* Entrust VPN connector uses different MIME types */
#define MIME_PKI	"x-pki-message"
#define MIME_GETCA_RA_ENTRUST	"application/x-x509-ra-ca-certs"

/* SCEP reply types based on MIME headers */
#define	SCEP_MIME_GETCA		1
#define	SCEP_MIME_GETCA_RA	3
#define	SCEP_MIME_PKI		5

/* SCEP request types */
#define	SCEP_REQUEST_NONE		0
#define	SCEP_REQUEST_PKCSREQ		19
#define	SCEP_REQUEST_PKCSREQ_STR	"19"
#define	SCEP_REQUEST_GETCERTINIT	20
#define	SCEP_REQUEST_GETCERTINIT_STR	"20"
#define	SCEP_REQUEST_GETCERT		21
#define	SCEP_REQUEST_GETCERT_STR	"21"
#define	SCEP_REQUEST_GETCRL		22
#define	SCEP_REQUEST_GETCRL_STR		"22"

/* SCEP reply types */
#define	SCEP_REPLY_NONE		0
#define	SCEP_REPLY_CERTREP	3
#define	SCEP_REPLY_CERTREP_STR	"3"


/* SSCEP return values (not in SCEP draft) */
#define SCEP_PKISTATUS_BADALG		70 /* BADALG failInfo */
#define SCEP_PKISTATUS_BADMSGCHK	71 /* BADMSGCHK failInfo */
#define SCEP_PKISTATUS_BADREQ		72 /* BADREQ failInfo */
#define SCEP_PKISTATUS_BADTIME		73 /* BADTIME failInfo */
#define SCEP_PKISTATUS_BADCERTID	74 /* BADCERTID failInfo */
#define SCEP_PKISTATUS_TIMEOUT		89 /* Network timeout */
#define SCEP_PKISTATUS_SS		91 /* Error generating selfsigned */
#define SCEP_PKISTATUS_FILE		93 /* Error in file handling */
#define SCEP_PKISTATUS_NET		95 /* Network sending message */
#define SCEP_PKISTATUS_P7		97 /* Error in pkcs7 routines */
#define SCEP_PKISTATUS_UNSET		99 /* Unset pkiStatus */

/* SCEP failInfo values */
#define SCEP_FAILINFO_BADALG		0
#define SCEP_FAILINFO_BADALG_STR	\
	"Unrecognized or unsupported algorithm ident"
#define SCEP_FAILINFO_BADMSGCHK		1
#define SCEP_FAILINFO_BADMSGCHK_STR	\
	"Integrity check failed"
#define SCEP_FAILINFO_BADREQ		2
#define SCEP_FAILINFO_BADREQ_STR	\
	"Transaction not permitted or supported" 
#define SCEP_FAILINFO_BADTIME		3
#define SCEP_FAILINFO_BADTIME_STR	\
	"Message time field was not sufficiently close to the system time"
#define SCEP_FAILINFO_BADCERTID		4
#define SCEP_FAILINFO_BADCERTID_STR 	\
	"No certificate could be identified matching"

/* End of Global defines */


/* Structures */

/* GETCertInital data structure */

typedef struct {
	X509_NAME *issuer;
	X509_NAME *subject;
} pkcs7_issuer_and_subject;

/* HTTP reply structure */
struct http_reply {

	/* SCEP reply type */
	int type;

	/* Status */
	long status;

	/* Payload */
	char *payload;

	/* Payload size */
	int bytes;
};

/* SCEP transaction structure */
struct scep {

	/* SCEP message types */
	int request_type;
	char *request_type_str;
	int reply_type;

	/* SCEP message status */
	int pki_status;
	int fail_info;

	/* SCEP transaction attributes */
	char *transaction_id;
	char *sender_nonce;
	int sender_nonce_len;
	char *reply_recipient_nonce;
	int recipient_nonce_len;

	/* Certificates */
	X509 *signercert;
	EVP_PKEY *signerkey;

	EVP_PKEY *pkey;

	/* Request */
	PKCS7 *request_p7;
	char *request_payload;
	int request_len;
	pkcs7_issuer_and_subject *ias_getcertinit;
	PKCS7_ISSUER_AND_SERIAL *ias_getcert;
	PKCS7_ISSUER_AND_SERIAL *ias_getcrl;

	/* Reply */
	PKCS7 *reply_p7;
};

struct sscep_ctx {
    int nid_messageType;
    int nid_pkiStatus;
    int nid_failInfo;
    int nid_senderNonce;
    int nid_recipientNonce;
    int nid_transId;
    int nid_extensionReq;
    int verbose;
    int debug;
    EVP_MD *fp_alg;
    EVP_MD *sig_alg;
    EVP_CIPHER *enc_alg;
};
/* End of structures */


/* Functions */
/* Create self-signed certificate */
int new_selfsigned(struct scep *, struct sscep_ctx *ctx, struct sscep_operation_info *op_info);

/* Get key fingerprint */
char * key_fingerprint(X509_REQ *);

/* PKCS#7 encode message */
int pkcs7_wrap(struct scep *, struct sscep_ctx *, struct sscep_operation_info *);

/* PKCS#7 decode message */
int pkcs7_unwrap(struct scep *s, struct sscep_ctx *ctx, struct sscep_operation_info *op_info, char* data, int datalen);

/* Add signed string attribute */
int add_attribute_string(STACK_OF(X509_ATTRIBUTE) *, int, char *, struct sscep_ctx *);

/* Add signed octet attribute */
int add_attribute_octet(STACK_OF(X509_ATTRIBUTE) *, int, char *, int, struct sscep_ctx *);

/* Find signed attributes */
int get_signed_attribute(STACK_OF(X509_ATTRIBUTE) *, int, int, char **, struct sscep_ctx *);
int get_attribute(STACK_OF(X509_ATTRIBUTE) *, int, ASN1_TYPE **, struct sscep_ctx *);

/* URL-endcode */
char *url_encode (char *, ssize_t);

/* Retrieve the ca cert */
/* Send HTTP message */
int send_msg (struct http_reply *, char *url, int operation, CURL *cctx, int verbose);
int retrieve_local_cert(struct scep *s, cert_cb cb, void *cookie, struct sscep_ctx *, struct sscep_operation_info *op_info);
int retrieve_ca_ra(struct http_reply *s, cert_cb cb, void* cookie, struct sscep_ctx *);

#endif
