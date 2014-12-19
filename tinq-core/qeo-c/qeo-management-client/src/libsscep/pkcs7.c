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

/* PKCS#7 routines */

#include "sscep.h"
#include "ias.h"

/*
 * Wrap data in PKCS#7 envelopes and base64-encode the result.
 * Data is PKCS#10 request in PKCSReq, or pkcs7_issuer_and_subject
 * structure in GetCertInitial and PKCS7_ISSUER_AND_SERIAL in
 * GetCert and GETCrl.
 */
int pkcs7_wrap(struct scep *s, struct sscep_ctx *ctx, struct sscep_operation_info *op_info)
{
    BIO *databio = NULL;
    BIO *encbio = NULL;
    BIO *pkcs7bio = NULL;
    BIO *memorybio = NULL;
    BIO *outbio = NULL;
    unsigned char *buffer = NULL;
    int len = 0;
    STACK_OF(X509) *recipients = NULL;
    PKCS7 *p7enc = NULL;
    PKCS7_SIGNER_INFO *si;
    STACK_OF(X509_ATTRIBUTE) *attributes;
    X509 *signercert = NULL;
    EVP_PKEY *signerkey = NULL;
    int ret = SCEP_PKISTATUS_P7;
    char *payload = NULL;
    int payload_len;

    /* Create a new sender nonce for all messages
     * XXXXXXXXXXXXXX should it be per transaction? */
    s->sender_nonce_len = 16;
    free(s->sender_nonce);/* Clean up from previous runs */
    s->sender_nonce = (char *)malloc(s->sender_nonce_len * sizeof(char));
    RAND_bytes((unsigned char *) s->sender_nonce, s->sender_nonce_len);

    /* Prepare data payload */
    switch (s->request_type) {
        case SCEP_REQUEST_PKCSREQ:
            /*
             * Set printable message type
             * We set this later as an autheticated attribute
             * "messageType".
             */
            s->request_type_str = SCEP_REQUEST_PKCSREQ_STR;

            /* Signer cert */
            signercert = s->signercert;
            signerkey = s->signerkey;

            /* Create inner PKCS#7  */
            if (ctx->verbose){
                qeo_log_i("creating inner PKCS#7");
            }

            /* Read request in memory bio */
            databio = BIO_new(BIO_s_mem());
            if (i2d_X509_REQ_bio(databio, op_info->request) <= 0) {
                qeo_log_e("error writing certificate request in bio");
                goto error;
            }
            (void)BIO_flush(databio);
            break;

        case SCEP_REQUEST_GETCERTINIT:

            /* Set printable message type */
            s->request_type_str = SCEP_REQUEST_GETCERTINIT_STR;

            /* Signer cert */
            signercert = s->signercert;
            signerkey = s->signerkey;

            /* Create inner PKCS#7  */
            if (ctx->verbose){
                qeo_log_i("creating inner PKCS#7");
            }

            /* Read data in memory bio */
            databio = BIO_new(BIO_s_mem());
            if (i2d_pkcs7_issuer_and_subject_bio(databio, s->ias_getcertinit)) {
                qeo_log_e("error writing GetCertInitial data in bio");
                goto error;
            }
            (void)BIO_flush(databio);
            break;
    }
    /* Below this is the common code for all request_type */

    /* Read in the payload */
    payload_len = BIO_get_mem_data(databio, &payload);
    if (ctx->verbose){
        qeo_log_i("data payload size: %d bytes", payload_len);
    }

    /* Create encryption certificate stack */
    if ((recipients = sk_X509_new(NULL) ) == NULL) {
        qeo_log_e("error creating certificate stack");
        goto error;
    }
    if (sk_X509_push(recipients, op_info->racert) <= 0) {
        qeo_log_e("error adding recipient encryption certificate");
        goto error;
    }

    /* Create BIO for encryption  */
    if ((encbio = BIO_new_mem_buf(payload, payload_len)) == NULL ) {
        qeo_log_e("error creating data bio");
        goto error;
    }

    /* Encrypt */
    if (!(p7enc = PKCS7_encrypt(recipients, encbio, ctx->enc_alg, PKCS7_BINARY))) {
        qeo_log_e("request payload encrypt failed");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("successfully encrypted payload");
    }

    /* Write encrypted data */
    memorybio = BIO_new(BIO_s_mem());
    if (i2d_PKCS7_bio(memorybio, p7enc) <= 0) {
        qeo_log_e("error writing encrypted data");
        goto error;
    }
    (void)BIO_flush(memorybio);
    BIO_set_flags(memorybio, BIO_FLAGS_MEM_RDONLY);
    len = BIO_get_mem_data(memorybio, &buffer);
    BIO_free(memorybio);
    memorybio=NULL;
    if (ctx->verbose){
        qeo_log_i("envelope size: %d bytes", len);
    }
    if (ctx->debug) {
        qeo_log_i("printing PEM fomatted PKCS#7");
        PEM_write_PKCS7(stdout, p7enc);
    }

    /* Create outer PKCS#7  */
    if (ctx->verbose){
        qeo_log_i("creating outer PKCS#7");
    }
    s->request_p7 = PKCS7_new();
    if (s->request_p7 == NULL ) {
        qeo_log_e("failed creating PKCS#7 for signing");
        goto error;
    }
    if (!PKCS7_set_type(s->request_p7, NID_pkcs7_signed)) {
        qeo_log_e("failed setting PKCS#7 type");
        goto error;
    }

    /* Add signer certificate  and signature */
    PKCS7_add_certificate(s->request_p7, signercert);
    if ((si = PKCS7_add_signature(s->request_p7, signercert, signerkey, ctx->sig_alg)) == NULL ) {
        qeo_log_e("error adding PKCS#7 signature");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("signature added successfully");
    }

    /* Set signed attributes */
    if (ctx->verbose){
        qeo_log_i("adding signed attributes");
    }
    attributes = sk_X509_ATTRIBUTE_new_null();
    add_attribute_string(attributes, ctx->nid_transId, s->transaction_id, ctx);
    add_attribute_string(attributes, ctx->nid_messageType, s->request_type_str, ctx);
    add_attribute_octet(attributes, ctx->nid_senderNonce, s->sender_nonce, s->sender_nonce_len, ctx);
    PKCS7_set_signed_attributes(si, attributes);
    sk_X509_ATTRIBUTE_pop_free(attributes, X509_ATTRIBUTE_free);

    /* Add contentType */
    if (!PKCS7_add_signed_attribute(si, NID_pkcs9_contentType, V_ASN1_OBJECT, OBJ_nid2obj(NID_pkcs7_data))) {
        qeo_log_e("error adding NID_pkcs9_contentType");
        goto error;
    }

    /* Create new content */
    if (!PKCS7_content_new(s->request_p7, NID_pkcs7_data)) {
        qeo_log_e("failed setting PKCS#7 content type");
        goto error;
    }

    /* Write data  */
    pkcs7bio = PKCS7_dataInit(s->request_p7, NULL );
    if (pkcs7bio == NULL ) {
        qeo_log_e("error opening bio for writing PKCS#7 data");
        goto error;
    }
    if (len != BIO_write(pkcs7bio, buffer, len)) {
        qeo_log_e("error writing PKCS#7 data");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("PKCS#7 data written successfully");
    }

    /* Finalize PKCS#7  */
    if (!PKCS7_dataFinal(s->request_p7, pkcs7bio)) {
        qeo_log_e("error finalizing outer PKCS#7");
        goto error;
    }
    if (ctx->debug) {
        qeo_log_i("printing PEM fomatted PKCS#7");
        PEM_write_PKCS7(stdout, s->request_p7);
    }

    /* base64-encode the data */
    if (ctx->verbose){
        qeo_log_i("applying base64 encoding");
    }

    /* Create base64 filtering bio */
    memorybio = BIO_new(BIO_s_mem());
    outbio = BIO_push(BIO_new(BIO_f_base64()), memorybio);

    /* Copy PKCS#7 */
    i2d_PKCS7_bio(outbio, s->request_p7);
    (void)BIO_flush(outbio);
    payload_len = BIO_get_mem_data(memorybio, &payload);
    s->request_payload = (char*) malloc(sizeof(char)*payload_len);
    if (!s->request_payload){
        goto error;
    }
    s->request_len = payload_len;
    memcpy(s->request_payload, payload, s->request_len);
    if (ctx->verbose){
        qeo_log_i("base64 encoded payload size: %d bytes", payload_len);
    }

    ret = 0;
error:
    BIO_free(databio);
    BIO_free(encbio);
    BIO_free_all(pkcs7bio);
    BIO_free(memorybio);
    BIO_free(outbio);
    if (recipients != NULL){
        sk_X509_free(recipients);/* Only free the stack, not the certificates */
    }
    PKCS7_free(p7enc);
    OPENSSL_free(buffer);
    return ret;
}

/*
 * Unwrap PKCS#7 data and decrypt if necessary
 */
int pkcs7_unwrap(struct scep *s, struct sscep_ctx *ctx, struct sscep_operation_info *op_info, char* data, int datalen)
{
    BIO *memorybio = NULL;
    BIO *outbio = NULL;
    BIO *pkcs7bio = NULL;
    int i, bytes, used;
    STACK_OF(PKCS7_SIGNER_INFO) *sk;
    PKCS7 *p7enc = NULL;
    PKCS7_SIGNER_INFO *si;
    STACK_OF(X509_ATTRIBUTE) *attribs;
    char *p = NULL;
    unsigned char buffer[1024];
    X509 *recipientcert;
    EVP_PKEY *recipientkey;
    int ret = SCEP_PKISTATUS_P7;

    /* Create new memory BIO for outer PKCS#7 */
    memorybio = BIO_new(BIO_s_mem());

    /* Read in data */
    if (ctx->verbose){
        qeo_log_i("reading outer PKCS#7");
    }
    if (BIO_write(memorybio, data, datalen) <= 0) {
        qeo_log_e("error reading PKCS#7 data");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("PKCS#7 payload size: %d bytes", datalen);
    }
    s->reply_p7 = d2i_PKCS7_bio(memorybio, NULL );
    if (s->reply_p7 == NULL ) {
        qeo_log_e("error retrieving PKCS#7 data");
        goto error;
    }
    if (ctx->debug) {
        qeo_log_i("printing PEM fomatted PKCS#7");
        PEM_write_PKCS7(stdout, s->reply_p7);
    }

    /* Make sure this is a signed PKCS#7 */
    if (!PKCS7_type_is_signed(s->reply_p7)) {
        qeo_log_e("PKCS#7 is not signed!");
        goto error;
    }

    /* Create BIO for content data */
    pkcs7bio = PKCS7_dataInit(s->reply_p7, NULL );
    if (pkcs7bio == NULL ) {
        qeo_log_e("cannot get PKCS#7 data");
        goto error;
    }

    /* Copy enveloped data from PKCS#7 */
    outbio = BIO_new(BIO_s_mem());
    used = 0;
    for (;;) {
        bytes = BIO_read(pkcs7bio, buffer, sizeof(buffer));
        used += bytes;
        if (bytes <= 0)
            break;
        BIO_write(outbio, buffer, bytes);
    }
    (void)BIO_flush(outbio);
    if (ctx->verbose){
        qeo_log_i("PKCS#7 contains %d bytes of enveloped data", used);
    }

    /* Get signer */
    sk = PKCS7_get_signer_info(s->reply_p7);
    if (sk == NULL ) {
        qeo_log_e("cannot get signer info!");
        goto error;
    }

    /* Verify signature */
    if (ctx->verbose){
        qeo_log_i("verifying signature");
    }
    si = sk_PKCS7_SIGNER_INFO_value(sk, 0);
    if (PKCS7_signatureVerify(pkcs7bio, s->reply_p7, si, op_info->racert) <= 0) {
        qeo_log_e("error verifying signature");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("signature ok");
    }

    /* Get signed attributes */
    if (ctx->verbose){
        qeo_log_i("finding signed attributes");
    }
    attribs = PKCS7_get_signed_attributes(si);
    if (attribs == NULL ) {
        qeo_log_e("no attributes found");
        goto error;
    }

    /* Transaction id */
    if ((get_signed_attribute(attribs, ctx->nid_transId, V_ASN1_PRINTABLESTRING, &p, ctx)) == 1) {
        qeo_log_e("cannot find transId");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("reply transaction id: %s", p);
    }
    if (strncmp(s->transaction_id, p, strlen(p))) {
        qeo_log_e("transaction id mismatch");
        goto error;
    }
    free(p);
    p=NULL;
    /* Message type, should be of type CertRep */
    if (get_signed_attribute(attribs, ctx->nid_messageType, V_ASN1_PRINTABLESTRING, &p, ctx) == 1) {
        qeo_log_e("cannot find messageType");
        goto error;
    }
    if (atoi(p) != 3) {
        qeo_log_e("wrong message type in reply");
        goto error;
    }
    if (ctx->verbose){
        qeo_log_i("reply message type is good");
    }

    free(p);
    p=NULL;
    /* Recipient nonces: */
    if (get_signed_attribute(attribs, ctx->nid_recipientNonce, V_ASN1_OCTET_STRING, &p, ctx) == 1) {
        qeo_log_e("cannot find recipientNonce");
        goto error;
    }
    s->reply_recipient_nonce = p;
    p = NULL;
    if (ctx->verbose) {
        qeo_log_i("recipientNonce in reply");
    }
    /*
     * Compare recipient nonce to original sender nonce
     * The draft says nothing about this, but it makes sense to me..
     * XXXXXXXXXXXXXX check
     */
    for (i = 0; i < 16; i++) {
        if (s->sender_nonce[i] != s->reply_recipient_nonce[i]) {
            if (ctx->verbose)
                qeo_log_e("corrupted nonce received");
            /* Instead of exit, break out */
            break;
        }
    }
    /* Get pkiStatus */
    if (get_signed_attribute(attribs, ctx->nid_pkiStatus, V_ASN1_PRINTABLESTRING, &p, ctx) == 1) {
        qeo_log_e("cannot find pkiStatus");
        /* This is a mandatory attribute.. */
        goto error;
    }
    switch (atoi(p)) {
        case SCEP_PKISTATUS_SUCCESS:
            qeo_log_i("pkistatus: SUCCESS");
            s->pki_status = SCEP_PKISTATUS_SUCCESS;
            break;
        case SCEP_PKISTATUS_FAILURE:
            qeo_log_i("pkistatus: FAILURE");
            s->pki_status = SCEP_PKISTATUS_FAILURE;
            break;
        case SCEP_PKISTATUS_PENDING:
            qeo_log_i("pkistatus: PENDING");
            s->pki_status = SCEP_PKISTATUS_PENDING;
            break;
        default:
            qeo_log_e("wrong pkistatus in reply");
            goto error;
    }
    free(p);
    p=NULL;

    /* Get failInfo */
    if (s->pki_status == SCEP_PKISTATUS_FAILURE) {
        if (get_signed_attribute(attribs, ctx->nid_failInfo, V_ASN1_PRINTABLESTRING, &p, ctx) == 1) {
            qeo_log_e("cannot find failInfo");
            goto error;
        }
        switch (atoi(p)) {
            case SCEP_FAILINFO_BADALG:
                s->fail_info = SCEP_FAILINFO_BADALG;
                qeo_log_i("reason: %s", SCEP_FAILINFO_BADALG_STR);
                break;
            case SCEP_FAILINFO_BADMSGCHK:
                s->fail_info = SCEP_FAILINFO_BADMSGCHK;
                qeo_log_i("reason: %s", SCEP_FAILINFO_BADMSGCHK_STR);
                break;
            case SCEP_FAILINFO_BADREQ:
                s->fail_info = SCEP_FAILINFO_BADREQ;
                qeo_log_i("reason: %s", SCEP_FAILINFO_BADREQ_STR);
                break;
            case SCEP_FAILINFO_BADTIME:
                s->fail_info = SCEP_FAILINFO_BADTIME;
                qeo_log_i("reason: %s", SCEP_FAILINFO_BADTIME_STR);
                break;
            case SCEP_FAILINFO_BADCERTID:
                s->fail_info = SCEP_FAILINFO_BADCERTID;
                qeo_log_i("reason: %s", SCEP_FAILINFO_BADCERTID_STR);
                break;
            default:
                qeo_log_e("wrong failInfo in " "reply");
                goto error;
        }
        free(p);
        p=NULL;
    }
    /* If FAILURE or PENDING, we can return */
    if (s->pki_status != SCEP_PKISTATUS_SUCCESS) {
        /* There shouldn't be any more data... */
        if (ctx->verbose && (used != 0)) {
            qeo_log_e("illegal size of payload");
        }
        return (0);
    }
    /* We got success and expect data */
    if (used == 0) {
        qeo_log_e("illegal size of payload");
        goto error;
    }

    /* Decrypt the inner PKCS#7 */
    recipientcert = s->signercert;
    recipientkey = s->signerkey;

    if (ctx->verbose){
        qeo_log_i("reading inner PKCS#7");
    }
    p7enc = d2i_PKCS7_bio(outbio, NULL );
    if (p7enc == NULL ) {
        qeo_log_e("cannot read inner PKCS#7");
        goto error;
    }
    BIO_free(outbio);/* No longer need it */
    outbio = NULL;
    if (ctx->debug) {
        qeo_log_i("printing PEM fomatted PKCS#7");
        PEM_write_PKCS7(stdout, p7enc);
    }

    /* Decrypt the data  */

    outbio = BIO_new(BIO_s_mem());
    if (ctx->verbose){
        qeo_log_i("decrypting inner PKCS#7");
    }
    if (PKCS7_decrypt(p7enc, recipientkey, recipientcert, outbio, 0) == 0) {
        qeo_log_e("error decrypting inner PKCS#7");
        goto error;
    }
    (void)BIO_flush(outbio);

    /* Write decrypted data */
    PKCS7_free(s->reply_p7);
    s->reply_p7 = d2i_PKCS7_bio(outbio, NULL );
    ret = 0;
error:
    free(p);
    BIO_free(outbio);
    BIO_free_all(pkcs7bio);
    BIO_free(memorybio);
    PKCS7_free(p7enc);
    return ret;
}

/* Add signed attributes */
int add_attribute_string(STACK_OF(X509_ATTRIBUTE) *attrs, int nid, char *buffer, struct sscep_ctx *ctx)
{
    ASN1_STRING *asn1_string = NULL;
    X509_ATTRIBUTE *x509_a;
    int ret = SCEP_PKISTATUS_P7;

    if (ctx->verbose){
        qeo_log_i("adding string attribute %s", OBJ_nid2sn(nid));
    }

    asn1_string = ASN1_STRING_new();
    if (ASN1_STRING_set(asn1_string, buffer, strlen(buffer)) <= 0) {
        qeo_log_e("error adding data to ASN.1 string");
        goto error;
    }
    x509_a = X509_ATTRIBUTE_create(nid, V_ASN1_PRINTABLESTRING, asn1_string);
    /* Previoux function takes ownership of the string */
    asn1_string = NULL;
    sk_X509_ATTRIBUTE_push(attrs, x509_a);

    ret = 0;
error:
    ASN1_STRING_free(asn1_string);
    return ret;

}
int add_attribute_octet(STACK_OF(X509_ATTRIBUTE) *attrs, int nid, char *buffer, int len, struct sscep_ctx *ctx)
{
    ASN1_STRING *asn1_string = NULL;
    X509_ATTRIBUTE *x509_a;
    int ret = SCEP_PKISTATUS_P7;

    if (ctx->verbose){
        qeo_log_i("adding octet attribute %s", OBJ_nid2sn(nid));
    }

    asn1_string = ASN1_STRING_new();
    if (ASN1_STRING_set(asn1_string, buffer, len) <= 0) {
        qeo_log_e("error adding data to ASN.1 string");
        goto error;
    }
    x509_a = X509_ATTRIBUTE_create(nid, V_ASN1_OCTET_STRING, asn1_string);
    /* Previoux function takes ownership of the string */
    asn1_string = NULL;
    sk_X509_ATTRIBUTE_push(attrs, x509_a);

    ret = 0;
error:
    ASN1_STRING_free(asn1_string);
    return ret;
}

/* Find signed attributes */
int get_signed_attribute(STACK_OF(X509_ATTRIBUTE) *attribs, int nid, int type, char **buffer, struct sscep_ctx *ctx)
{
    int rc;
    ASN1_TYPE *asn1_type;
    unsigned int len;

    /* Find attribute */
    rc = get_attribute(attribs, nid, &asn1_type, ctx);
    if (rc == 1) {
        if (ctx->verbose)
            qeo_log_e("error finding attribute");
        goto error;
    }
    if (ASN1_TYPE_get(asn1_type) != type) {
        qeo_log_e("wrong ASN.1 type");
        goto error;
    }

    /* Copy data */
    len = ASN1_STRING_length(asn1_type->value.asn1_string);
    if (len <= 0) {
        goto error;
    }
    else if (ctx->verbose){
        qeo_log_i("allocating %d bytes for attribute", len);
    }
    if (type == V_ASN1_PRINTABLESTRING) {
        *buffer = (char *)malloc(len + 1);
    }
    else {
        *buffer = (char *)malloc(len);
    }
    if (*buffer == NULL ) {
        qeo_log_e("cannot malloc space for attribute");
        goto error;
    }
    memcpy(*buffer, ASN1_STRING_data(asn1_type->value.asn1_string), len);

    /* Add null terminator if it's a PrintableString */
    if (type == V_ASN1_PRINTABLESTRING) {
        (*buffer)[len] = 0;
    }

    return (0);
    error:
        return 1;

}
int get_attribute(STACK_OF(X509_ATTRIBUTE) *attribs, int required_nid, ASN1_TYPE **asn1_type, struct sscep_ctx *ctx)
{
    int i;
    ASN1_OBJECT *asn1_obj = NULL;
    X509_ATTRIBUTE *x509_attrib = NULL;

    if (ctx->verbose){
        qeo_log_i("finding attribute %s", OBJ_nid2sn(required_nid));
    }
    *asn1_type = NULL;
    asn1_obj = OBJ_nid2obj(required_nid);
    if (asn1_obj == NULL ) {
        qeo_log_e("error creating ASN.1 object");
        goto error;
    }
    /* Find attribute */
    for (i = 0; i < sk_X509_ATTRIBUTE_num(attribs); i++) {
        x509_attrib = sk_X509_ATTRIBUTE_value(attribs, i);
        if (OBJ_cmp(x509_attrib->object, asn1_obj) == 0) {
            if ((x509_attrib->value.set) && (sk_ASN1_TYPE_num(x509_attrib->value.set) != 0)) {
                if (*asn1_type != NULL ) {
                    qeo_log_e("no value found");
                    goto error;
                }
                *asn1_type = sk_ASN1_TYPE_value(x509_attrib->value.set, 0);
            }
        }
    }

    if (*asn1_type == NULL )
        goto error;
    return (0);
    error:
        return 1;

}
