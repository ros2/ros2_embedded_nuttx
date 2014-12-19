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

/* Misc. SCEP routines */

#include "sscep.h"
#include "ias.h"

static void clean_http_reply(struct http_reply *r){
    free(r->payload);
    memset(r, 0, sizeof(*r));
}

/**
 * Clean all temporary memory of a transaction reply response while keeping the global state.
 * @param s
 */
static void clean_transaction(struct scep *s)
{
    if (s == NULL) return;
    PKCS7_free(s->request_p7);
    s->request_p7 = NULL;
    PKCS7_free(s->reply_p7);
    s->reply_p7 = NULL;
    free(s->request_payload);
    s->request_payload = NULL;
    free(s->sender_nonce);
    s->sender_nonce = NULL;
    free(s->reply_recipient_nonce);
    s->reply_recipient_nonce = NULL;
}

/**
 * Completely free up all transaction memory.
 * @param s
 */
static void free_transaction(struct scep *s)
{
    if (s == NULL) return;
    clean_transaction(s);
    free(s->transaction_id);
    X509_free(s->signercert);
    EVP_PKEY_free(s->signerkey);
    pkcs7_issuer_and_subject_free(s->ias_getcertinit);
    PKCS7_ISSUER_AND_SERIAL_free(s->ias_getcert);
    PKCS7_ISSUER_AND_SERIAL_free(s->ias_getcrl);
    EVP_PKEY_free(s->pkey);
    memset(s, 0, sizeof(*s));
}

/*
 * Initialize a SCEP transaction
 */
static int new_transaction(struct scep *s, struct sscep_ctx *ctx, struct sscep_operation_info *op_info)
{

    /* Set the whole struct as 0 */
    memset(s, 0, sizeof(*s));

    /* Set request and reply type */
    s->request_type = SCEP_REQUEST_NONE;
    s->request_type_str = NULL;
    s->reply_type = SCEP_REPLY_NONE;
    s->pki_status = SCEP_PKISTATUS_UNSET;

    /* Set other variables */
    s->ias_getcertinit = pkcs7_issuer_and_subject_new();
    s->ias_getcert = PKCS7_ISSUER_AND_SERIAL_new();
    s->ias_getcrl = PKCS7_ISSUER_AND_SERIAL_new();

    /* Create transaction id */
    if (op_info->operation_mode == SCEP_OPERATION_ENROLL)
        s->transaction_id = key_fingerprint(op_info->request);
    else
        s->transaction_id = strdup(TRANS_ID_GETCERT);
    if (ctx->verbose) {
        qeo_log_i("transaction id: %s", s->transaction_id);
    }
    return (0);
}

/*
 * Create self signed certificate based on request subject.
 * Set also subjectAltName extension if found from request.
 */
int new_selfsigned(struct scep *s, struct sscep_ctx *ctx, struct sscep_operation_info *op_info)
{
    unsigned char *ptr;
    X509 *cert = NULL;
    X509_NAME *subject;
    ASN1_INTEGER *serial = NULL;
    int ret = SCEP_PKISTATUS_ERROR;

    /* Extract public value of the local key from request */
    ret = SCEP_PKISTATUS_SS;
    if (!(s->pkey = X509_REQ_get_pubkey(op_info->request))) {
        qeo_log_e("error getting public key from request");
        goto error;
    }

    /* Get subject, issuer and extensions */
    if (!(subject = X509_REQ_get_subject_name(op_info->request))) {
        qeo_log_e("error getting subject");
        goto error;
    }

    /* Create new certificate */
    if (!(cert = X509_new())) {
        qeo_log_e("Could not create certificate");
        goto error;
    }
    /* Set version (X509v3) */
    if (X509_set_version(cert, 2L) != 1) {
        qeo_log_e("error setting cert version");
        goto error;
    }
    /* Get serial no from transaction id */
    ptr = (unsigned char *)s->transaction_id;
    if (!(serial = c2i_ASN1_INTEGER(NULL, (const unsigned char **) &ptr, 32))) {
        qeo_log_e("error converting serial");
        goto error;
    }
    if (X509_set_serialNumber(cert, serial) != 1) {
        qeo_log_e("error setting serial");
        goto error;
    }
    /* Set subject */
    if (X509_set_subject_name(cert, subject) != 1) {
        qeo_log_e("error setting subject");
        goto error;
    }
    /* Set issuer (it's really the same as subject */
    if (X509_set_issuer_name(cert, subject) != 1) {
        qeo_log_e("error setting issuer");
        goto error;
    }
    /* Set public key */
    if (X509_set_pubkey(cert, s->pkey) != 1) {
        qeo_log_e("error setting public key");
        goto error;
    }
    /* Set duration */
    if (!(X509_gmtime_adj(X509_get_notBefore(cert), 0))) {
        qeo_log_e("error setting begin time");
        goto error;
    }
    if (!(X509_gmtime_adj(X509_get_notAfter(cert), SELFSIGNED_EXPIRE_DAYS * 24 * 60))) {
        qeo_log_e("error setting end time");
        goto error;
    }

    /* Sign certificate */
    if (!(X509_sign(cert, op_info->rsa, ctx->sig_alg))) {
        qeo_log_e("error signing certificate");
        goto error;
    }

    /* Copy the pointer and return */
    s->signercert = cert;
    cert = NULL;
    /* Increase the ref count of the key */
    CRYPTO_add(&op_info->rsa->references, 1, CRYPTO_LOCK_EVP_PKEY);
    s->signerkey = op_info->rsa;
    ret = 0;
error:
    X509_free(cert);
    ASN1_INTEGER_free(serial);
    return ret;
}

/*
 * Calculate transaction id.
 * Return pointer to ascii presentation of the hash.
 */
char *
key_fingerprint(X509_REQ *req)
{
    char *ret = NULL, *str = NULL;
    EVP_PKEY *pkey = NULL;
    unsigned char *data, md[MD5_DIGEST_LENGTH];
    int c, len;
    BIO *bio = NULL;
    MD5_CTX ctx;

    do {
        /* Assign space for ASCII presentation of the digest */
        str = (char *)malloc(2 * MD5_DIGEST_LENGTH + 1);
        if (str == NULL ) {
            break;
        }


        /* Create new memory bio for reading the public key */
        bio = BIO_new(BIO_s_mem());
        if (bio == NULL ) {
            break;
        }
        ret = str;
        pkey = X509_REQ_get_pubkey(req);
        i2d_PUBKEY_bio(bio, pkey);

        len = BIO_get_mem_data(bio, &data);

        /* Calculate MD5 hash: */
        MD5_Init(&ctx);
        MD5_Update(&ctx, data, len);
        MD5_Final(md, &ctx);

        /* Copy as ASCII string and return: */
        for (c = 0; c < MD5_DIGEST_LENGTH; c++, str += 2) {
            sprintf(str, "%02X", md[c]);
        }
        *(str) = '\0';
        str = NULL; /* don't free it */
    } while (0);

    EVP_PKEY_free(pkey);
    BIO_free_all(bio);
    free(str);
    return ret;
}

static int get_cacert_with_cb(cert_cb cb, void *cookie, struct sscep_ctx *ctx, struct sscep_operation_info *op_info)
{
    char url_string[16384];
    struct http_reply reply;
    BIO* bp = NULL;
    X509* cert;
    int ret = SCEP_PKISTATUS_FAILURE;

    memset(&reply, 0, sizeof(reply));
    do {

        /* Forge the HTTP message */
        snprintf(url_string, sizeof(url_string), "%s?operation=GetCACert&message=%s", op_info->url,
                 op_info->identifier);
        /*
         * Send http message.
         * Response is written to http_response struct "reply".
         */
        if ((ret = send_msg(&reply, url_string, op_info->operation_mode, op_info->curl_ctx, ctx->verbose)) != SCEP_PKISTATUS_SUCCESS) {
            qeo_log_e("error while sending message");
            break;
        }
        if (reply.payload == NULL ) {
            qeo_log_e("no data inside reply");
            break;
        }
        qeo_log_i("valid response from server");
        if (reply.type == SCEP_MIME_GETCA_RA) {
            /* XXXXXXXXXXXXXXXXXXXXX chain not verified */
            ret = retrieve_ca_ra(&reply, cb, cookie, ctx);
            break;
        }
        else if (reply.type == SCEP_MIME_GETCA) {
            /* Read payload as DER X.509 object: */
            bp = BIO_new_mem_buf(reply.payload, reply.bytes);

            cert = d2i_X509_bio(bp, NULL );
            if (cert) {
                cb(cert, cookie);
            }
            else {
                break;
            }
        }
        else {
            break;
        }
        ret = SCEP_PKISTATUS_SUCCESS;
    } while (0);

    free(reply.payload);
    BIO_free(bp);
    return ret;
}

/*
 * Initialize SCEP
 */
sscep_ctx_t sscep_init(int verbose, int debug)
{
    struct sscep_ctx *ctx = (struct sscep_ctx*)calloc(1, sizeof(struct sscep_ctx));

    if (ctx == NULL ) {
        goto err;
    }
    ctx->verbose = verbose;
    ctx->debug = debug;
    ctx->sig_alg = (EVP_MD *)EVP_sha256();
    ctx->fp_alg = (EVP_MD *)EVP_sha256();
    ctx->enc_alg = (EVP_CIPHER *)EVP_des_cbc();

    /* Create OpenSSL NIDs */
    ctx->nid_messageType = OBJ_create("2.16.840.1.113733.1.9.2", "messageType", "messageType");
    if (ctx->nid_messageType == 0) {
        goto err;
    }
    ctx->nid_pkiStatus = OBJ_create("2.16.840.1.113733.1.9.3", "pkiStatus", "pkiStatus");
    if (ctx->nid_pkiStatus == 0) {
        goto err;
    }
    ctx->nid_failInfo = OBJ_create("2.16.840.1.113733.1.9.4", "failInfo", "failInfo");
    if (ctx->nid_failInfo == 0) {
        goto err;
    }
    ctx->nid_senderNonce = OBJ_create("2.16.840.1.113733.1.9.5", "senderNonce", "senderNonce");
    if (ctx->nid_senderNonce == 0) {
        goto err;
    }
    ctx->nid_recipientNonce = OBJ_create("2.16.840.1.113733.1.9.6", "recipientNonce", "recipientNonce");
    if (ctx->nid_recipientNonce == 0) {
        goto err;
    }
    ctx->nid_transId = OBJ_create("2.16.840.1.113733.1.9.7", "transId", "transId");
    if (ctx->nid_transId == 0) {
        goto err;
    }
    ctx->nid_extensionReq = OBJ_create("2.16.840.1.113733.1.9.8", "extensionReq", "extensionReq");
    if (ctx->nid_extensionReq == 0) {
        goto err;
    }

    return ctx;

    err: qeo_log_e("cannot init sscep ctx");
    free(ctx);
    return NULL ;

}

int sscep_perform(sscep_ctx_t ctx, sscep_operation_info_t op_info, cert_cb cb, void *cookie)
{
    char url_string[16384], *p;/* TODO check if this is a problem */
    struct http_reply reply;
    int c;
    struct scep scep_t;
    int count = 1;
    int ret = SCEP_PKISTATUS_ERROR;
    ASN1_INTEGER *asnint = NULL;

    memset(&reply, 0, sizeof(reply));
    memset(&scep_t, 0, sizeof(scep_t));

    if (!ctx || !op_info || !op_info->url || !op_info->curl_ctx)
        goto error;

    /*
     * Switch to operation specific code
     */
    switch (op_info->operation_mode) {
        case SCEP_OPERATION_GETCA:
            if (ctx->verbose){
                qeo_log_i("SCEP_OPERATION_GETCA");
            }

            ret = get_cacert_with_cb(cb, cookie, ctx, op_info);
            goto exit;

        case SCEP_OPERATION_ENROLL:

            /*
             * Create a new SCEP transaction and self-signed
             * certificate based on cert request
             */
            if (ctx->verbose){
                qeo_log_i("new transaction");
            }
            new_transaction(&scep_t, ctx, op_info);
            if (op_info->operation_mode != SCEP_OPERATION_ENROLL)
                goto not_enroll;
            if (ctx->verbose){
                qeo_log_i("generating selfsigned certificate");
            }
            new_selfsigned(&scep_t, ctx, op_info);

            /* Write issuer name and subject (GetCertInitial): */
            if (!X509_NAME_set(&scep_t.ias_getcertinit->subject, X509_REQ_get_subject_name(op_info->request))) {
                qeo_log_e("error getting subject for GetCertInitial");
                ret = SCEP_PKISTATUS_BADCERTID;
                goto error;
            }

            not_enroll: if (!X509_NAME_set(&scep_t.ias_getcertinit->issuer, X509_get_issuer_name(op_info->racert))) {
                qeo_log_e("error getting issuer for GetCertInitial");
                ret = SCEP_PKISTATUS_BADCERTID;
                goto error;
            }
            /* Write issuer name and serial (GETC{ert,rl}): */
            X509_NAME_set(&scep_t.ias_getcert->issuer, scep_t.ias_getcertinit->issuer);
            X509_NAME_set(&scep_t.ias_getcrl->issuer, scep_t.ias_getcertinit->issuer);
            asnint = X509_get_serialNumber(op_info->racert);
            if (!asnint) {
                qeo_log_e("error getting serial for GetCertInitial");
                ret = SCEP_PKISTATUS_ERROR;
                goto error;
            }
            ASN1_INTEGER_set(scep_t.ias_getcrl->serial, ASN1_INTEGER_get(asnint));
            break;
        default:
            goto error;
    }
    switch (op_info->operation_mode) {
        case SCEP_OPERATION_ENROLL:
            if (ctx->verbose){
                qeo_log_i("SCEP_OPERATION_ENROLL");
            }
            qeo_log_i("sending certificate request");
            scep_t.request_type = SCEP_REQUEST_PKCSREQ;
            break;
    }

    /* Enter polling loop */
    while (scep_t.pki_status != SCEP_PKISTATUS_SUCCESS) {
        clean_transaction(&scep_t);
        /* create payload */
        if (pkcs7_wrap(&scep_t, ctx, op_info) == 1){
            ret = SCEP_PKISTATUS_FAILURE;
            goto error;
        }

        /* URL-encode */
        p = url_encode(scep_t.request_payload, scep_t.request_len);
        if (p == NULL ) {
            ret = SCEP_PKISTATUS_FAILURE;
            goto error;
        }

        /* Forge the HTTP message */
        snprintf(url_string, sizeof(url_string), "%s?operation=PKIOperation&message=%s", op_info->url, p);
        free(p);

        if (ctx->verbose){
            qeo_log_d("scep msg: %s", url_string);
        }

        /* send http */
        clean_http_reply(&reply);
        if ((c = send_msg(&reply, url_string, op_info->operation_mode, op_info->curl_ctx, ctx->verbose)) != SCEP_PKISTATUS_SUCCESS) {
            qeo_log_e("error while sending message");
            ret = c;
            goto error;
        }

        /* Verisign Onsite returns strange reply...
         * XXXXXXXXXXXXXXXXXXX */
        if ((reply.status == 200) && (reply.payload == NULL )) {
            /*
             scep_t.pki_status = SCEP_PKISTATUS_PENDING;
             break;
             */
            ret = SCEP_PKISTATUS_ERROR;
            goto error;
        }

        qeo_log_i("valid response from server");

        /* Check payload */
        pkcs7_unwrap(&scep_t, ctx, op_info, reply.payload, reply.bytes);
        ret = scep_t.pki_status;

        switch (scep_t.pki_status) {
            case SCEP_PKISTATUS_SUCCESS:
                break;
            case SCEP_PKISTATUS_PENDING:
                /* Check time limits */
                if (((POLL_TIME * count) >= MAX_POLL_TIME) || (count > MAX_POLL_COUNT)) {
                    ret = SCEP_PKISTATUS_TIMEOUT;
                    goto error;
                }
                scep_t.request_type = SCEP_REQUEST_GETCERTINIT;

                /* Wait for poll interval */
                if (ctx->verbose){
                    qeo_log_i("waiting for %d secs", POLL_TIME);
                }
                sleep(POLL_TIME);
                qeo_log_i("requesting certificate (#%d)", count);

                /* Add counter */
                count++;
                break;

            case SCEP_PKISTATUS_FAILURE:

                /* Handle failure */
                switch (scep_t.fail_info) {
                    case SCEP_FAILINFO_BADALG:
                        ret = SCEP_PKISTATUS_BADALG;
                        goto error;
                    case SCEP_FAILINFO_BADMSGCHK:
                        ret = SCEP_PKISTATUS_BADMSGCHK;
                        goto error;
                    case SCEP_FAILINFO_BADREQ:
                        ret = SCEP_PKISTATUS_BADREQ;
                        goto error;
                    case SCEP_FAILINFO_BADTIME:
                        ret = SCEP_PKISTATUS_BADTIME;
                        goto error;
                    case SCEP_FAILINFO_BADCERTID:
                        ret = SCEP_PKISTATUS_BADCERTID;
                        goto error;
                        /* Shouldn't be there... */
                    default:
                        ret = SCEP_PKISTATUS_ERROR;
                        goto error;
                }
            default:
                qeo_log_e("unknown pkiStatus");
                ret = SCEP_PKISTATUS_ERROR;
                goto error;
        }
    }

    /* We got SUCCESS, analyze the reply */
    switch (scep_t.request_type) {

        /* Local certificate */
        case SCEP_REQUEST_PKCSREQ:
            if (retrieve_local_cert(&scep_t, cb, cookie, ctx, op_info) != SCEP_PKISTATUS_SUCCESS) {
                ret = SCEP_PKISTATUS_FAILURE;
                goto error;
            }
            break;
    }
    exit: error:
    clean_http_reply(&reply);
    free_transaction(&scep_t);
    return ret;
}

void sscep_cleanup(sscep_ctx_t ctx)
{
    //TODO check what to cleanup
    //OBJ_cleanup(); From a library, you cannot do this because this might impact the user of the library.
    free(ctx);
}

