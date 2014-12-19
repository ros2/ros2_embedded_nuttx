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

/*#######################################################################
 #                                                                      #
 #  HEADER (INCLUDE) SECTION                                            #
 #                                                                      #
 ###################################################################### */
#include <qeo/openssl_engine.h>
#include <stdio.h>
#include <openssl/engine.h>
#include <openssl/crypto.h>
#include <openssl/pkcs12.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <string.h>
#include <assert.h>
#include <qeo/platform.h>
#include <qeo/log.h>

/*#######################################################################
 #                                                                      #
 # TYPES and DEFINES                                                    #
 #                                                                      #
 ###################################################################### */
#define PKCS12_PATH       "/tmp/"
#define PKCS12_FILE       "truststore.p12"
#define PKCS12_PASSWD     "secret"
#define PKCS12_ENGINE_NAME "engine_pkcs12"
#define PKCS12_ENGINE_ID   "pkcs12"

#ifdef OPENSSL_ENGINE_DEBUG
#define TRACE_EXTRA(format, ...) if (qeo_logger) qeo_logger(QEO_LOG_DEBUG, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)
#else
#define TRACE_EXTRA(format, ...)
#endif



typedef enum
{
    SAFEBAG_TYPE_KEY, SAFEBAG_TYPE_CERT
} SafeBagType;

struct vector
{
    char **strings;
    unsigned int size;
    unsigned int capacity;
};

/*#######################################################################
 #                                                                      #
 #  PRIVATE DATA MEMBERS                                                #
 #                                                                      #
 ###################################################################### */

static char *_pkcs12_path = NULL;

static const ENGINE_CMD_DEFN _engine_pkcs12_cmd_defns[] = { { QEO_OPENSSL_ENGINE_CMD_LOAD_CERT_CHAIN, "LOAD_CERT_CHAIN",
    "Return certificate chain found by specified friendlyName", ENGINE_CMD_FLAG_INTERNAL }, { QEO_OPENSSL_ENGINE_CMD_SAVE_CREDENTIALS,
    "SAVE_CREDENTIALS", "Save private key with friendlyName and related certificate chain", ENGINE_CMD_FLAG_INTERNAL },
    { 0, NULL, NULL, 0 }, { QEO_OPENSSL_ENGINE_CMD_GET_FRIENDLY_NAMES, "GET_FRIENDLY_NAMES", "Retrieve all friendly names",
        ENGINE_CMD_FLAG_INTERNAL }, };

static RSA_METHOD _engine_pkcs12_rsa = { PKCS12_ENGINE_NAME, NULL, /* rsa_pub_enc */
NULL, /* rsa_pub_dec (verification) */
NULL, /* rsa_priv_enc (signing) */
NULL, /* rsa_priv_dec */
NULL, /* rsa_mod_exp */
NULL, /* bn_mod_exp */
NULL, /* init */
NULL, /* finish */
RSA_FLAG_EXT_PKEY | RSA_FLAG_NO_BLINDING, /* flags */
NULL, /* app_data */
NULL, /* rsa_sign */
NULL, /* rsa_verify */
NULL , /* rsa_keygen */
};

static int _engine_pkcs12_pkey_meth_nids[] = { 0 };
/*#######################################################################
 #                                                                      #
 #  PRIVATE FUNCTION PROTOTYPES                                         #
 #                                                                      #
 ###################################################################### */
static int init_vector(struct vector *v, unsigned int cap);
static int add_vector(struct vector *v, const char *str);
static void free_vector(struct vector *v);
static int bind_helper(ENGINE *e);
static int engine_pkcs12_destroy(ENGINE *e);
static int engine_pkcs12_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)());
static int engine_pkcs12_init(ENGINE *engine);
static int engine_pkcs12_finish(ENGINE *engine);
static int engine_pkcs12_cmd_load_cert_chain(qeo_openssl_engine_cmd_load_cert_chain_t *p);
static int engine_pkcs12_cmd_save_credentials(qeo_openssl_engine_cmd_save_credentials_t *p);
static int engine_pkcs12_cmd_get_friendly_names(qeo_openssl_engine_cmd_get_friendly_names_t *p);
static PKCS12 *read_pkcs12(const char *file);

static int write_pkcs12(PKCS12 *p12, const char *file);
static EVP_PKEY *engine_pkcs12_load_private_key(ENGINE *e,
                                                const char *key_id,
                                                UI_METHOD *ui_method,
                                                void *callback_data);
static int register_rsa_methods(void);
static int engine_pkcs12_pkey_meths(ENGINE *e, EVP_PKEY_METHOD **pmeth, const int **nids, int nid);

static X509 *fetch_and_remove_user_cert_from_chain(STACK_OF(X509) *chain, EVP_PKEY *pkey);
static int complete_certificate_chain(STACK_OF(X509) *in_chain, STACK_OF(X509) *out_chain);

static int parse_pk12(PKCS12 *p12,
                      const char *pass,
                      int passlen,
                      const char *friendlyName,
                      struct vector *friendlyNames,
                      EVP_PKEY **pkey,
                      STACK_OF(X509) *ocerts);
static int parse_bags(STACK_OF(PKCS12_SAFEBAG) *bags,
                      const char *pass,
                      int passlen,
                      const char *friendlyName,
                      struct vector *friendlyNames,
                      EVP_PKEY **pkey,
                      STACK_OF(X509) *ocerts);
static int parse_bag(PKCS12_SAFEBAG *bag,
                     const char *pass,
                     int passlen,
                     const char *friendlyName,
                     struct vector *friendlyNames,
                     EVP_PKEY **pkey,
                     STACK_OF(X509) *ocerts);
static PKCS12 *pkcs12_construct(void);
int pkcs12_update(PKCS12 *p12, char *pass, const char *name, EVP_PKEY *pkey, X509 *cert, STACK_OF(X509) *ca);

static int copy_bag_attr(PKCS12_SAFEBAG *bag, EVP_PKEY *pkey, int nid);

static int set_pkcs12_path(void);
static char * get_pkcs12_passwd(void);

/*#######################################################################
 #                                                                      #
 #  PRIVATE FUNCTIONS                                                   #
 #                                                                      #
 ###################################################################### */

static int engine_pkcs12_pkey_meths(ENGINE *e, EVP_PKEY_METHOD **pmeth, const int **nids, int nid)
{
    int ret = 0;

    TRACE_EXTRA("enter");

    do {

        if (pmeth == NULL ) {
            if (nids == NULL ) {
                qeo_log_e("nids == NULL");
                break;
            }
            *nids = _engine_pkcs12_pkey_meth_nids;
            break;
        }

        *pmeth = (EVP_PKEY_METHOD *)EVP_PKEY_meth_find(nid);
        if (*pmeth == NULL ) {
            qeo_log_e("*pmeth == NULL");
            break;
        }

        ret = 1;
    } while (0);

    TRACE_EXTRA("exit");
    return ret;
}

static int bind_helper(ENGINE *e)
{
    TRACE_EXTRA("enter");
    register_rsa_methods();

    if (!ENGINE_set_id(e, PKCS12_ENGINE_ID) || !ENGINE_set_name(e, PKCS12_ENGINE_NAME)
            || !ENGINE_set_cmd_defns(e, _engine_pkcs12_cmd_defns)
            || !ENGINE_set_destroy_function(e, engine_pkcs12_destroy)
            || !ENGINE_set_init_function(e, engine_pkcs12_init) || !ENGINE_set_finish_function(e, engine_pkcs12_finish)
            || !ENGINE_set_ctrl_function(e, engine_pkcs12_ctrl)
            || !ENGINE_set_load_privkey_function(e, engine_pkcs12_load_private_key)
            || !ENGINE_set_pkey_meths(e, engine_pkcs12_pkey_meths) || !ENGINE_set_RSA(e, &_engine_pkcs12_rsa) /*||
             !ENGINE_set_load_pubkey_function (e, engine_pkcs12_load_public_key) ||
             !ENGINE_set_DSA (e, engine_pkcs12_dsa) ||
             !ENGINE_set_ECDH (e, engine_pkcs12_dh) ||
             !ENGINE_set_ECDSA (e, engine_pkcs12_dh) ||
             !ENGINE_set_DH (e, engine_pkcs12_dh) ||
             !ENGINE_set_RAND (e, engine_pkcs12_rand) ||
             !ENGINE_set_STORE (e, asn1_i2d_ex_primitiveengine_pkcs12_rand) ||
             !ENGINE_set_ciphers (e, engine_pkcs12_syphers_f) ||
             !ENGINE_set_digests (e, engine_pkcs12_digest_f) ||
             !ENGINE_set_flags (e, engine_pkcs12_flags) */) {
        return 0;
    }

    TRACE_EXTRA("exit");
    return 1;
}

/***********************************************/
/** RSA functions **/
/***********************************************/

/* Enable RSA usage through this engine */
static int register_rsa_methods(void)
{
    TRACE_EXTRA("enter");
    const RSA_METHOD *rsa_meth = RSA_PKCS1_SSLeay();

    _engine_pkcs12_rsa.rsa_pub_enc = rsa_meth->rsa_pub_enc;
    _engine_pkcs12_rsa.rsa_pub_dec = rsa_meth->rsa_pub_dec;
    _engine_pkcs12_rsa.rsa_priv_enc = rsa_meth->rsa_priv_enc;
    _engine_pkcs12_rsa.rsa_priv_dec = rsa_meth->rsa_priv_dec;
    _engine_pkcs12_rsa.rsa_mod_exp = rsa_meth->rsa_mod_exp;
    _engine_pkcs12_rsa.bn_mod_exp = rsa_meth->bn_mod_exp;
    TRACE_EXTRA("exit");

    return 1;
}

/***********************************************/
/** CTRL functions **/
/***********************************************/

static int engine_pkcs12_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)())
{
    int ret = 0;

    TRACE_EXTRA("enter");
    TRACE_EXTRA("cmd: %d", cmd);

    switch (cmd) {
        case QEO_OPENSSL_ENGINE_CMD_LOAD_CERT_CHAIN: {
            ret = engine_pkcs12_cmd_load_cert_chain((qeo_openssl_engine_cmd_load_cert_chain_t *)p);
        }
            break;

        case QEO_OPENSSL_ENGINE_CMD_SAVE_CREDENTIALS: {
            ret = engine_pkcs12_cmd_save_credentials((qeo_openssl_engine_cmd_save_credentials_t *)p);
        }
            break;

        case QEO_OPENSSL_ENGINE_CMD_GET_FRIENDLY_NAMES: {
            ret = engine_pkcs12_cmd_get_friendly_names((qeo_openssl_engine_cmd_get_friendly_names_t *)p);
        }
            break;

        default:
            qeo_log_e("unknown cmd: %d", cmd);
    }

    TRACE_EXTRA("exit");
    return ret;
}

/***********************************************/
/** PKCS12 functions **/
/***********************************************/
static PKCS12 *read_pkcs12(const char *file)
{
    FILE *fp = NULL;
    PKCS12 *p12 = NULL;

    TRACE_EXTRA("enter");
    do {
        if (file == NULL ) {
            qeo_log_e("file == NULL %s", file);
            break;
        }

        fp = fopen(file, "rb");
        if (fp == NULL ) {
            qeo_log_d("Error opening file %s", file);
            break;
        }
        p12 = d2i_PKCS12_fp(fp, NULL );
        fclose(fp);
        if (!p12) {
            qeo_log_e("Error reading PKCS#12 from  file\n");
            break;
        }

        return p12;
    } while (0);

    TRACE_EXTRA("exit");
    return p12;
}

static int write_pkcs12(PKCS12 *p12, const char *file)
{
    FILE *fp = NULL;
    int ret = 0;

    TRACE_EXTRA("enter");

    do {

        if (file == NULL ) {
            qeo_log_e("file == NULL %s", file);
            break;
        }

        fp = fopen(file, "wb");
        if (fp == NULL ) {
            qeo_log_e("Error opening file %s", file);
            break;
        }

        i2d_PKCS12_fp(fp, p12);
        fclose(fp);

        ret = 1;
    } while (0);

    TRACE_EXTRA("exit");

    return ret;
}

static PKCS12 *pkcs12_construct(void)
{
    TRACE_EXTRA("enter");
    PKCS12 *p12;
    int nid_p7 = NID_pkcs7_data;
    TRACE_EXTRA("creating new PKCS#12");
    p12 = PKCS12_init(nid_p7);
    if (p12 == NULL ) {
        qeo_log_e("Error creating PKCS#12 file\n");
    }

    TRACE_EXTRA("exit");
    return p12;
}

static EVP_PKEY *engine_pkcs12_load_private_key(ENGINE *e,
                                                const char *key_id,
                                                UI_METHOD *ui_method,
                                                void *callback_data)
{
    EVP_PKEY *pkey = NULL;
    PKCS12 *p12 = NULL;
    STACK_OF(X509) * ocerts = NULL;

    TRACE_EXTRA("enter");

    do {
        ocerts = sk_X509_new_null();
        if (ocerts == NULL ) {
            qeo_log_e("ERR_R_MALLOC_FAILURE");
            break;
        }

        p12 = read_pkcs12(_pkcs12_path);
        if (p12 == NULL ) {
            qeo_log_e("PKCS12_R_INVALID_NULL_PKCS12_POINTER");
            break;
        }
        if (!parse_pk12(p12, get_pkcs12_passwd(), -1, key_id, NULL, &pkey, ocerts)) {
            qeo_log_e("PKCS12_R_PARSE_ERROR");
            break;
        }
    } while (0);

    if (ocerts != NULL ) {
        sk_X509_pop_free(ocerts, X509_free);
    }

    if (p12 != NULL ) {
        PKCS12_free(p12);
    }

    TRACE_EXTRA("exit");
    //Don't forget to free the private key
    return pkey;
}

static int engine_pkcs12_cmd_load_cert_chain(qeo_openssl_engine_cmd_load_cert_chain_t *p)
{
    if (p == NULL ) {
        return 0;
    }

    EVP_PKEY *pkey = NULL;
    PKCS12 *p12 = NULL;
    STACK_OF(X509) * ocerts = NULL;
    X509 *user_cert = NULL;
    int ret = 0;

    do {
        TRACE_EXTRA("enter");
        if (p == NULL ) {
            return 0;
        }

        if (p->friendlyName == NULL ) {
            qeo_log_e("friendlyName == NULL");
            return 0;
        }

        ocerts = sk_X509_new_null();
        if (ocerts == NULL ) {
            qeo_log_e("ocerts == NULL");
            break;
        }

        p12 = read_pkcs12(_pkcs12_path);
        if (p12 == NULL ) {
            qeo_log_e("p12 == NULL");
            break;
        }

        if (!parse_pk12(p12, get_pkcs12_passwd(), -1, p->friendlyName, NULL, &pkey, ocerts)) {
            qeo_log_e("p12 parse error");
            break;
        }

        if (pkey == NULL ) {
            qeo_log_e("pkey == NULL");
            break;
        }

        //Don't forget to free the chain
        p->chain = sk_X509_new_null();

        if (p->chain == NULL ) {
            qeo_log_e("p->chain == NULL");
            ret = 0;
            break;
        }

        TRACE_EXTRA("#ocerts: %d", sk_X509_num(ocerts));
        user_cert = fetch_and_remove_user_cert_from_chain(ocerts, pkey);
        if (user_cert == NULL ) {
            qeo_log_e("No user certificate found in chain");
            ret = 0;
            break;
        }
        TRACE_EXTRA("#ocerts: %d", sk_X509_num(ocerts));
        TRACE_EXTRA("#p->chain: %d", sk_X509_num(p->chain));
        if (sk_X509_push(p->chain, user_cert) == 0) {
            qeo_log_e("sk_X509_push failed");
            ret = 0;
            break;
        }

        TRACE_EXTRA("#ocerts: %d", sk_X509_num(ocerts));
        TRACE_EXTRA("#p->chain: %d", sk_X509_num(p->chain));
        if (complete_certificate_chain(ocerts, p->chain) == 0) {
            qeo_log_e("complete_certificate_chain failed");
            ret = 0;
            break;
        }
        TRACE_EXTRA("#ocerts: %d", sk_X509_num(ocerts));
        TRACE_EXTRA("#p->chain: %d", sk_X509_num(p->chain));

        ret = 1;
    } while (0);

    if (ocerts != NULL ) {
        sk_X509_pop_free(ocerts, X509_free);
    }

    EVP_PKEY_free(pkey);

    if (p12 != NULL ) {
        PKCS12_free(p12);
    }

    TRACE_EXTRA("exit");
    return ret;
}

static int init_vector(struct vector *v, unsigned int capacity)
{

    assert(capacity > 0); /* capacity == 0 causes problems when realloc'ing */
    v->strings = malloc(capacity * sizeof(char *));
    if (v->strings == NULL ) {
        return 0;
    }
    v->capacity = capacity;
    v->size = 0;

    return 1;
}

static int add_vector(struct vector *v, const char *str)
{

    if (v->capacity == v->size) {
        v->capacity *= 2;
        v->strings = realloc(v->strings, v->capacity * sizeof(char *));
        if (v->strings == NULL ) { /* strings are lost now :( --> anyway, all is probably lost now :-/ */
            return 0;
        }
    }
    v->strings[v->size++] = strdup(str);
    if (v->strings[v->size - 1] == NULL ) {
        return 0;
    }

    return 1;
}

static void free_vector(struct vector *v)
{

    int i;
    for (i = 0; i < v->size; ++i) {
        free(v->strings[i]);
    }

    free(v->strings);

}

static int engine_pkcs12_cmd_get_friendly_names(qeo_openssl_engine_cmd_get_friendly_names_t *p)
{
    if (p == NULL ) {
        return 0;
    }

    p->friendly_names = NULL;
    p->number_of_friendly_names = 0;
    int ret = 0;

    EVP_PKEY *dummyKey = NULL;
    PKCS12 *p12 = NULL;
    STACK_OF(X509) * dummyCerts = NULL;
    struct vector friendlyNames = { };

    do {
        if (init_vector(&friendlyNames, 1) == 0) {
            qeo_log_e("Could not init vector");
            break;
        }

        dummyCerts = sk_X509_new_null();
        if (dummyCerts == NULL ) {
            qeo_log_e("dummyCerts == NULL");
            break;
        }

        p12 = read_pkcs12(_pkcs12_path);
        if (p12 == NULL ) {
            ret = 1; /* not a real failure if no pkcs12 was present */
            break;
        }

        if (parse_pk12(p12, get_pkcs12_passwd(), -1, NULL, &friendlyNames, &dummyKey, dummyCerts) == 0) {
            qeo_log_e("p12 parse error");
            break;
        }

        p->friendly_names = friendlyNames.strings;
        p->number_of_friendly_names = friendlyNames.size;

        ret = 1;

    } while (0);

    EVP_PKEY_free(dummyKey);
    if (dummyCerts != NULL ) {
        sk_X509_pop_free(dummyCerts,X509_free);
    }

    if (p12 == 0 || ret == 0) { /* only free if an error occured */
        free_vector(&friendlyNames);
    }

    if (p12 != NULL ) {
        PKCS12_free(p12);
    }

    return ret;
}

static int engine_pkcs12_cmd_save_credentials(qeo_openssl_engine_cmd_save_credentials_t *p)
{
    if (p == NULL ) {
        return 0;
    }

    if (p->friendlyName == NULL ) {
        qeo_log_e("friendlyName == NULL");
        return 0;
    }

    if (p->pkey == NULL ) {
        qeo_log_e("p->pkey == NULL");
        return 0;
    }

    if (p->chain == NULL ) {
        qeo_log_e("chain == NULL");
        return 0;
    }

    int ret = 0;
    X509 *user_cert = NULL;
    PKCS12 *p12 = NULL;
    STACK_OF(X509) * sktmp = NULL;

    do {
        TRACE_EXTRA("enter");

        p12 = read_pkcs12(_pkcs12_path);
        //try to create a new pkcs12 when reading it from a file fails
        if (p12 == NULL && (p12 = pkcs12_construct()) == NULL ) {
            qeo_log_e("p12 == NULL");
            break;
        }

        /* We use a temporary STACK so we can chop and hack at it */
        sktmp = sk_X509_dup(p->chain);
        if (sktmp == NULL ) {
            qeo_log_e("sktmp == NULL");
        }

        //Fetch user certificate from sktmp based on pkey and remove user certificate from chain
        user_cert = fetch_and_remove_user_cert_from_chain(sktmp, p->pkey);
        if (user_cert == NULL ) {
            qeo_log_e("No user certificate found in chain");
            ret = 0;
            break;
        }

        ret = pkcs12_update(p12, get_pkcs12_passwd(), p->friendlyName, p->pkey, user_cert, sktmp);
        if (ret != 1) {
            qeo_log_e("pkcs12_update failed");
            break;
        }

        ret = write_pkcs12(p12, _pkcs12_path);
        if (ret != 1) {
            qeo_log_e("write_pkcs12 failed");
            break;
        }

        ret = 1;

        TRACE_EXTRA("exit");
    } while (0);

    if (sktmp != NULL ) {
        sk_X509_free(sktmp);
    }

    if (p12 != NULL ) {
        PKCS12_free(p12);
    }

    return ret;
}

static int engine_pkcs12_destroy(ENGINE *e)
{
    TRACE_EXTRA("enter");TRACE_EXTRA("exit");
    return 1;
}

static X509 *fetch_and_remove_user_cert_from_chain(STACK_OF(X509) *chain, EVP_PKEY *pkey)
{
    X509 *x = NULL;
    X509 *user_cert = NULL;
    int i = 0;

    TRACE_EXTRA("enter");
    for (i = 0; i < sk_X509_num(chain); i++) {
        x = sk_X509_value(chain, i);
        if (X509_check_private_key(x, pkey) == 1) {
            (void)sk_X509_delete(chain, i);
            user_cert = x;
            break;
        }
    }

    TRACE_EXTRA("exit");

    return user_cert;
}

static int complete_certificate_chain(STACK_OF(X509) *in_chain, STACK_OF(X509) *out_chain)
{
    X509 *x = NULL;
    X509 *cert = NULL;
    int ret = 0;
    int i = 0;

    TRACE_EXTRA("enter");

    do {
        cert = sk_X509_value(out_chain, sk_X509_num(out_chain) - 1);
        if (cert == NULL ) {
            qeo_log_e("cert == NULL");
            break;
        }
        if (X509_check_issued(cert, cert) == 0) {
            TRACE_EXTRA("found self-signed certificate");
            ret = 1;
            break;
        }

        for (i = 0; i < sk_X509_num(in_chain); i++) {
            x = sk_X509_value(in_chain, i);
            if (X509_check_issued(x, cert) == 0) {
                //found issuer cert
                TRACE_EXTRA("found issuer certificate");
                (void)sk_X509_delete(in_chain, i);
                if (sk_X509_push(out_chain, x) == 0) {
                    qeo_log_e("sk_X509_push failed");
                    break;
                }
                ret = complete_certificate_chain(in_chain, out_chain);
                break;
            }
        }
    } while (0);

    TRACE_EXTRA("exit");
    return ret;
}

/********************************************************/
/** PKCS12 parse functions based on openSSL p12_kiss.c **/
/********************************************************/
static int parse_pk12(PKCS12 *p12,
                      const char *pass,
                      int passlen,
                      const char *friendlyName,
                      struct vector *friendlyNames,
                      EVP_PKEY **pkey,
                      STACK_OF(X509) *ocerts)
{
    STACK_OF(PKCS7) * asafes;
    STACK_OF(PKCS12_SAFEBAG) * bags;
    int i, bagnid;
    PKCS7 *p7;

    TRACE_EXTRA("enter");

    if (!(asafes = PKCS12_unpack_authsafes(p12))) {
        return 0;
    }

    for (i = 0; i < sk_PKCS7_num(asafes); i++) {
        p7 = sk_PKCS7_value(asafes, i);
        bagnid = OBJ_obj2nid(p7->type);
        if (bagnid == NID_pkcs7_data) {
            bags = PKCS12_unpack_p7data(p7);
        }
        else if (bagnid == NID_pkcs7_encrypted) {
            bags = PKCS12_unpack_p7encdata(p7, pass, passlen);
        }
        else {
            continue;
        }
        if (!bags) {
            sk_PKCS7_pop_free(asafes, PKCS7_free);
            return 0;
        }
        if (!parse_bags(bags, pass, passlen, friendlyName, friendlyNames, pkey, ocerts)) {
            sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
            sk_PKCS7_pop_free(asafes, PKCS7_free);
            return 0;
        }
        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
    }
    sk_PKCS7_pop_free(asafes, PKCS7_free);TRACE_EXTRA("exit");
    return 1;
}

static int parse_bags(STACK_OF(PKCS12_SAFEBAG) *bags,
                      const char *pass,
                      int passlen,
                      const char *friendlyName,
                      struct vector *friendlyNames,
                      EVP_PKEY **pkey,
                      STACK_OF(X509) *ocerts)
{
    int i;

    for (i = 0; i < sk_PKCS12_SAFEBAG_num(bags); i++) {
        PKCS12_SAFEBAG *bag = sk_PKCS12_SAFEBAG_value(bags, i);
        if (!parse_bag(bag, pass, passlen, friendlyName, friendlyNames, pkey, ocerts)) {
            continue;
        }
    }
    return 1;
}

static int parse_bag(PKCS12_SAFEBAG *bag,
                     const char *pass,
                     int passlen,
                     const char *friendlyName,
                     struct vector *friendlyNames,
                     EVP_PKEY **pkey,
                     STACK_OF(X509) *ocerts)
{
    int rc = 1;
    PKCS8_PRIV_KEY_INFO *p8;
    X509 *x509;
    ASN1_TYPE *attrib;
    ASN1_BMPSTRING *fname = NULL;
    ASN1_OCTET_STRING *lkid = NULL;
    char *bagFriendlyName = NULL;

    if (bag == NULL ) {
        qeo_log_e("bag == NULL");
        return 0;
    }

    do {
        bagFriendlyName = PKCS12_get_friendlyname(bag);
        if (bagFriendlyName == NULL ) {
            //Friendly name can be NULL
            TRACE_EXTRA("bagFriendlyName == NULL");
        }

        if ((attrib = PKCS12_get_attr(bag, NID_friendlyName))) {
            fname = attrib->value.bmpstring;
        }

        if ((attrib = PKCS12_get_attr(bag, NID_localKeyID))) {
            lkid = attrib->value.octet_string;
        }

        if (M_PKCS12_bag_type(bag) == NID_pkcs8ShroudedKeyBag && friendlyNames != NULL && bagFriendlyName != NULL ) {
            add_vector(friendlyNames, bagFriendlyName);
            break;
        }

        switch (M_PKCS12_bag_type(bag)) {
            case NID_keyBag:
                TRACE_EXTRA("NID_keyBag");

                if ((bagFriendlyName == NULL )|| (strcmp(bagFriendlyName, friendlyName) != 0)){
                    qeo_log_w("KeyBag name mismatch: %s != %s", bagFriendlyName, friendlyName);
                    rc = 0;
                    break;
                }

                TRACE_EXTRA("bagFriendlyName(%s) == friendlyName(%s)", bagFriendlyName, friendlyName);

                if (!pkey || *pkey) {
                    break;
                }

                if (!(*pkey = EVP_PKCS82PKEY(bag->value.keybag))) {
                    rc = 0;
                    break;
                }
                break;

            case NID_pkcs8ShroudedKeyBag:
                TRACE_EXTRA("NID_pkcs8ShroudedKeyBag");

                if ((bagFriendlyName == NULL) || (strcmp(bagFriendlyName, friendlyName) != 0)) {
                    qeo_log_w("ShroudedKeyBag name mismatch: %s != %s", bagFriendlyName, friendlyName);
                    rc = 0;
                    break;
                }

                TRACE_EXTRA("bagFriendlyName(%s) == friendlyName(%s)", bagFriendlyName, friendlyName);

                if (!pkey || *pkey) {
                    break;
                }

                if (!(p8 = PKCS12_decrypt_skey(bag, pass, passlen))) {
                    rc = 0;
                    break;
                }
                *pkey = EVP_PKCS82PKEY(p8);
                PKCS8_PRIV_KEY_INFO_free(p8);
                if (!(*pkey)) {
                    rc = 0;
                    break;
                }
                break;

            case NID_certBag:
                TRACE_EXTRA("NID_certBag");
                if (M_PKCS12_cert_bag_type(bag) != NID_x509Certificate) {
                    break;
                }
                if (!(x509 = PKCS12_certbag2x509(bag))) {
                    rc = 0;
                    break;
                }
                if (lkid && !X509_keyid_set1(x509, lkid->data, lkid->length)) {
                    X509_free(x509);
                    rc = 0;
                    break;
                }
                if (fname) {
                    int len, r;
                    unsigned char *data;
                    len = ASN1_STRING_to_UTF8(&data, fname);
                    if (len > 0) {
                        r = X509_alias_set1(x509, data, len);
                        OPENSSL_free(data);
                        if (!r) {
                            X509_free(x509);
                            rc = 0;
                            break;
                        }
                    }
                }

                if (!sk_X509_push(ocerts, x509)) {
                    X509_free(x509);
                    rc = 0;
                    break;
                }

            break;

            case NID_safeContentsBag:
                TRACE_EXTRA("NID_safeContentsBag");
                rc = parse_bags(bag->value.safes, pass, passlen, friendlyName, friendlyNames,
                        pkey, ocerts);

            break;

            default:

            break;
        }
    } while(0);

    free(bagFriendlyName);

    return rc;
}

/*************************************************************************************/
/** PKCS12 update function based on PKCS12_create found in openSSL p12_crt.c **/
/*************************************************************************************/
int pkcs12_update(PKCS12 *p12, char *pass, const char *name, EVP_PKEY *pkey, X509 *cert, STACK_OF(X509) *ca)
{
    STACK_OF(PKCS7) * safes = NULL;
    STACK_OF(PKCS12_SAFEBAG) * bags = NULL;
    PKCS12_SAFEBAG *bag = NULL;
    int i;
    unsigned char keyid[EVP_MAX_MD_SIZE];
    unsigned int keyidlen = 0;
    /* Set defaults */
    int nid_cert = NID_pbe_WithSHA1And40BitRC2_CBC;
    int nid_key = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
    int iter = PKCS12_DEFAULT_ITER;
    int mac_iter = 1;
    int keytype = 0;
    int ret = 1;

    TRACE_EXTRA("enter");

    if (pkey == NULL ) {
        qeo_log_e("pkey == NULL");
        return 0;
    }

    if (cert == NULL ) {
        qeo_log_e("pkey == NULL");
        return 0;
    }

    do {
        //unpack existing autosafes
        if (!(safes = PKCS12_unpack_authsafes(p12))) {
            TRACE_EXTRA("PKCS12_unpack_authsafes: no safes found");

        }
        if (!X509_check_private_key(cert, pkey)) {
            qeo_log_e("X509_check_private_key failed");
            ret = 0;
            break;
        }
        X509_digest(cert, EVP_sha1(), keyid, &keyidlen);

        /****CERTIFICATES***/
        bag = PKCS12_add_cert(&bags, cert);
        if (name && !PKCS12_add_friendlyname(bag, name, -1)) {
            qeo_log_e("PKCS12_add_friendlyname failed");
            ret = 0;
            break;
        }
        if (keyidlen && !PKCS12_add_localkeyid(bag, keyid, keyidlen)) {
            qeo_log_e("PKCS12_add_localkeyid failed");
            ret = 0;
            break;
        }

        /* Add all other certificates */
        for (i = 0; i < sk_X509_num(ca); i++) {
            if (!PKCS12_add_cert(&bags, sk_X509_value(ca, i) )) {
                qeo_log_e("PKCS12_add_cert failed");
                ret = 0;
                break;
            }
        }

        if (ret == 0) {
            break;
        }

        if (bags && !PKCS12_add_safe(&safes, bags, nid_cert, iter, pass)) {
            qeo_log_e("PKCS12_add_safe failed");
            ret = 0;
            break;
        }

        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
        bags = NULL;

        /****PRIVATE KEY***/
        if (pkey) {

            bag = PKCS12_add_key(&bags, pkey, keytype, iter, nid_key, pass);
            if (!bag) {
                qeo_log_e("PKCS12_add_key failed");
                ret = 0;
                break;
            }

            if (!copy_bag_attr(bag, pkey, NID_ms_csp_name)) {
                qeo_log_e("copy_bag_attr failed");
                ret = 0;
                break;
            }

            if (!copy_bag_attr(bag, pkey, NID_LocalKeySet)) {
                qeo_log_e("copy_bag_attr failed");
                ret = 0;
                break;
            }

            if (name && !PKCS12_add_friendlyname(bag, name, -1)) {
                qeo_log_e("PKCS12_add_friendlyname failed");
                ret = 0;
                break;
            }

            if (keyidlen && !PKCS12_add_localkeyid(bag, keyid, keyidlen)) {
                qeo_log_e("PKCS12_add_friendlyname failed");
                ret = 0;
                break;
            }
        }

        if (bags && !PKCS12_add_safe(&safes, bags, -1, 0, NULL )) {
            qeo_log_e("PKCS12_add_safe failed");
            ret = 0;
            break;
        }

        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
        bags = NULL;

        if (!PKCS12_pack_authsafes(p12, safes)) {
            qeo_log_e("PKCS12_pack_authsafes failed");
            ret = 0;
            break;
        }

        sk_PKCS7_pop_free(safes, PKCS7_free);
        safes = NULL;

        if ((mac_iter != -1) && !PKCS12_set_mac(p12, pass, -1, NULL, 0, mac_iter, NULL )) {
            qeo_log_e("PKCS12_set_mac failed");
            ret = 0;
            break;
        }

    } while (0);

    if (safes) {
        sk_PKCS7_pop_free(safes, PKCS7_free);
    }

    if (bags) {
        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
    }

    TRACE_EXTRA("exit");
    return ret;
}

static int copy_bag_attr(PKCS12_SAFEBAG *bag, EVP_PKEY *pkey, int nid)
{
    TRACE_EXTRA("enter");
    int idx;
    X509_ATTRIBUTE *attr;
    idx = EVP_PKEY_get_attr_by_NID(pkey, nid, -1);
    if (idx < 0) {
        return 1;
    }
    attr = EVP_PKEY_get_attr(pkey, idx);
    if (!X509at_add1_attr(&bag->attrib, attr)) {
        return 0;
    }TRACE_EXTRA("exit");
    return 1;
}

/*create a new pkcs12 engine*/
static ENGINE *engine_pkcs12(void)
{
    ENGINE *e = NULL;

    TRACE_EXTRA("enter");

    e = ENGINE_new();
    if (e == NULL ) {
        qeo_log_e("Could not allocate new engine");
        return NULL ;
    }
    if (!bind_helper(e)) {
        qeo_log_e("failed to configure engine");
        ENGINE_free(e);
        return NULL ;
    }

    TRACE_EXTRA("exit");
    return e;
}

static int engine_pkcs12_init(ENGINE *e)
{
    TRACE_EXTRA("enter");
    if (set_pkcs12_path() == 0) {
        return 0;
    }TRACE_EXTRA("exit");

    return 1;
}

/*"Destructor"*/
static int engine_pkcs12_finish(ENGINE *e)
{
    TRACE_EXTRA("enter");
    free(_pkcs12_path);
    _pkcs12_path = NULL;
    TRACE_EXTRA("exit");
    return 1;
}
static int set_pkcs12_path(void)
{
    if (qeo_platform_get_device_storage_path(PKCS12_FILE, &_pkcs12_path) != QEO_UTIL_OK) {
        return 0;
    }

    return 1;
}

static char * get_pkcs12_passwd(void)
{
    return PKCS12_PASSWD;
}

/*#######################################################################
 #                                                                      #
 #  PUBLIC FUNCTIONS                                                    #
 #                                                                      #
 ###################################################################### */
/*add the engine to the engine arraylist*/
qeo_openssl_engine_retcode_t qeo_openssl_engine_init(void)
{
    ENGINE *e = NULL;

    TRACE_EXTRA("enter");

    e = engine_pkcs12();
    if (!e) {
        return QEO_OPENSSL_ENGINE_EFAIL;
    }
    ENGINE_add(e);
    ENGINE_free(e);
    ERR_clear_error();
    TRACE_EXTRA("exit");
    return QEO_OPENSSL_ENGINE_OK;
}

void qeo_openssl_engine_destroy(void){

    /* TODO */

}

qeo_openssl_engine_retcode_t qeo_openssl_engine_get_engine_id(char **engine_id){

    if (engine_id == NULL){
        return QEO_OPENSSL_ENGINE_EINVAL;
    }

    *engine_id = strdup(PKCS12_ENGINE_ID);
    if (*engine_id == NULL){
        return QEO_OPENSSL_ENGINE_ENOMEM;
    }

    return QEO_OPENSSL_ENGINE_OK;

}
