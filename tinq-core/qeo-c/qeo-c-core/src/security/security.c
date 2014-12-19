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

/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#ifndef DEBUG
#define NDEBUG
#endif
#if !defined(ANDROID) && !defined(TARG_OS_IOS)
#define NO_CONCURRENT_REGISTRATION
#endif
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#ifdef NO_CONCURRENT_REGISTRATION
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#endif
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <qeo/log.h>
#include "security.h"
#include "security_util.h"
#include <qeo/openssl_engine.h>
#include <qeo/mgmt_client.h>
#include <qeo/mgmt_cert_parser.h>
#include <qeo/platform.h>
#include <qeo/platform_security.h>
#include "remote_registration.h"
#include "qeocore/remote_registration.h"
#include <dds/dds_security.h>

#include "core_util.h"

/*########################################################################
#                                                                       #
#  TYPES SECTION                                             #
#                                                                       #
########################################################################*/
#define MAX_KEY_ID_LEN          128
#define MAGIC_QEO_SECURITY    0xdeadbeef

typedef enum {
    QEO_SECURITY_STOP_REASON_NONE,
    QEO_SECURITY_STOP_REASON_USER_CANCEL,
    QEO_SECURITY_STOP_REASON_REMOTE_REGISTRATION_TIMEOUT,

} qeo_security_stop_reason_t;

struct qeo_security {
#ifndef NDEBUG
    unsigned long                         magic;
#endif
    qeo_security_config                   cfg;
    pthread_mutex_t                       mutex;
    qeo_mgmt_client_ctx_t                 *mgmt_client_ctx;
    bool                                  authentication_started;
    pthread_t                             thread;

    /* related to qeo credentials */
    EVP_PKEY                              *pkey;
    STACK_OF(X509) * certificate_chain;

    /* related to registration method */
    pthread_cond_t                        reg_method_received_cond;
    qeo_platform_security_registration_method reg_method;

    /* related to remote registration */
    qeo_remote_registration_hndl_t        remote_reg;
    unsigned long                         registration_window;
    char                                  *suggested_username;
    char                                  *realm_name; 
    char                                  *unconfirmed_reg_url; /* must be confirmed by user */
    char                                  *unconfirmed_reg_otp; /* must be confirmed by user */

    /* related to confirmation */
    pthread_cond_t                        remote_reg_cred_confirmation_feedback_cond;
    bool                                  remote_reg_cred_confirmation_feedback_received;
    
    /* related to registration credentials in general */
    /* note that the condition variable below is used for both local and remote registration */
    pthread_cond_t                        reg_cred_received_cond;
    char                                  *reg_url; /* to use for scep */
    char                                  *reg_otp; /* to use for scep */ 

    qeo_security_stop_reason_t            stop_reason;

    qeo_security_state                    state;
    qeo_platform_security_state_reason    state_reason; /* reasons are only used at platform API -level */
};

/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                             #
#                                                                       #
########################################################################*/
#ifdef NO_CONCURRENT_REGISTRATION
static int              _qeo_registration_fd;
#endif
static bool             _initialized;
static ENGINE           *_engine;
static char             *_engine_id;
static int              _qeosec_objects_count;

#ifdef __USE_GNU
    /* compile with -D_GNU_SOURCE */
#ifndef NDEBUG
static pthread_mutex_t _mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
#else
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                             #
#                                                                       #
########################################################################*/
#ifndef NDEBUG
static bool is_valid_qeosec(qeo_security_hndl qeoSec);
#endif
static qeo_platform_security_state convert_internal_state_to_platform_security_state(qeo_security_state state);

/*########################################################################
#                                                                       #
#  STATIC SECTION                                             #
#                                                                       #
########################################################################*/

static void cfg_qeosec(qeo_security_hndl qeoSec, const qeo_security_config *cfg)
{
    memcpy(&qeoSec->cfg, cfg, sizeof(qeoSec->cfg));
    qeoSec->cfg.id.url = NULL; /* ignore any friendly name set in cfg */
    qeoSec->cfg.id.friendly_name = NULL; /* ignore any friendly name set in cfg */
    qeoSec->state = QEO_SECURITY_UNAUTHENTICATED;
    qeoSec->state_reason = QEO_PLATFORM_SECURITY_REASON_UNKNOWN;
    qeoSec->stop_reason = QEO_SECURITY_STOP_REASON_NONE;

#ifdef __USE_GNU
    /* compile with -D_GNU_SOURCE */
#ifndef NDEBUG
    qeoSec->mutex = (pthread_mutex_t)PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
    qeoSec->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif
#else
    qeoSec->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
#endif

    qeoSec->reg_method_received_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    qeoSec->remote_reg_cred_confirmation_feedback_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    qeoSec->reg_cred_received_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

#ifndef NDEBUG
    qeoSec->magic = MAGIC_QEO_SECURITY;
#endif
}

static void qeosec_lock(qeo_security_hndl qeoSec)
{
    assert(is_valid_qeosec(qeoSec));
    lock(&qeoSec->mutex);
}

static void qeosec_unlock(qeo_security_hndl qeoSec)
{
    unlock(&qeoSec->mutex);
}

static void api_lock(void)
{
    lock(&_mutex);
}

static void api_unlock(void)
{
    unlock(&_mutex);
}

static qeo_remote_registration_failure_reason_t convert_platform_security_reason_to_remote_registration_failure_reason(qeo_platform_security_state_reason state_reason){

    switch(state_reason){
        case QEO_PLATFORM_SECURITY_REASON_UNKNOWN:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_CANCELLED:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_CANCELLED;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_PLATFORM_FAILURE:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_PLATFORM_FAILURE;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INVALID_OTP:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_INVALID_OTP;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_INTERNAL_ERROR;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_NETWORK_FAILURE:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_NETWORK_FAILURE;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_SSL_HANDSHAKE_FAILURE:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_SSL_HANDSHAKE_FAILURE;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_RECEIVED_INVALID_CREDENTIALS:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_RECEIVED_INVALID_CREDENTIALS;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_STORE_FAILURE:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_STORE_FAILURE;
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_UNKNOWN:
            return QEO_REMOTE_REGISTRATION_FAILURE_REASON_UNKNOWN;
    }

    return QEO_REMOTE_REGISTRATION_FAILURE_REASON_UNKNOWN;
}

// from: http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
// + modifications
static char *trim_whitespace_strdup(const char *str)
{
    const char *end;

    // Trim leading space
    while(isspace(*str)){
        str++;
    }

    if(*str == 0){  // All spaces?
        return strdup(str);
    }

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace(*end)){ 
        end--;
    }

    char *dup = strndup(str, end - str + 1);
    if (dup == NULL){
        return NULL;
    }

    return dup;
}

static void update_remote_registration_locked(qeo_security_hndl qeoSec, qeo_security_state state){

    if (qeoSec->remote_reg != NULL){
        if (state == QEO_SECURITY_AUTHENTICATION_FAILURE){ 
            qeo_remote_registration_failure_reason_t rreason = convert_platform_security_reason_to_remote_registration_failure_reason(qeoSec->state_reason);
            qeo_remote_registration_update_registration_status(qeoSec->remote_reg, QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED, rreason);
        }
    }

}

static void update_authentication_state_locked(qeo_security_hndl qeoSec, qeo_security_state state)
{
    qeo_security_status_cb cb;
    qeo_platform_security_state_reason state_reason = qeoSec->state_reason;

    qeo_log_d("New state %d", state);
    qeoSec->state = state;
    cb            = qeoSec->cfg.security_status_cb;
    update_remote_registration_locked(qeoSec, state);
    qeosec_unlock(qeoSec);
    qeo_platform_security_update_state((qeo_platform_security_context_t)qeoSec, convert_internal_state_to_platform_security_state(state), state_reason );
    cb(qeoSec, state);
    qeosec_lock(qeoSec);
}

static void update_authentication_state(qeo_security_hndl qeoSec, qeo_security_state state)
{
    qeosec_lock(qeoSec);
    update_authentication_state_locked(qeoSec, state);
    qeosec_unlock(qeoSec);
}

static void qeosec_set_reason_locked(qeo_security_hndl qeoSec, qeo_platform_security_state_reason state_reason){

    qeoSec->state_reason = state_reason;
}

static void qeosec_set_reason(qeo_security_hndl qeoSec, qeo_platform_security_state_reason state_reason){

    qeosec_lock(qeoSec);
    qeosec_set_reason_locked(qeoSec, state_reason);
    qeosec_unlock(qeoSec);
}

static char *generate_friendly_name(qeo_security_identity *sec_id, char *buf, size_t *buflen)
{
    *buflen = snprintf(buf, *buflen, FRIENDLY_NAME_FORMAT, sec_id->realm_id, sec_id->device_id, sec_id->user_id);
    return buf;
}

static char *realm_to_string_locked(qeo_security_identity *id, char *buf, size_t *buflen)
{
    return generate_friendly_name(id, buf, buflen);
}

static EVP_PKEY *load_private_key_locked(qeo_security_hndl qeoSec)
{
    char    buf[MAX_KEY_ID_LEN];
    size_t  len = sizeof(buf);

    if (_engine == NULL) {
        qeo_log_e("engine not initialized");
        return NULL;
    }

    return ENGINE_load_private_key(_engine, realm_to_string_locked(&qeoSec->cfg.id, buf, &len), NULL, NULL);
}

static STACK_OF(X509) * load_certificate_chain_locked(qeo_security_hndl qeoSec)
{
    char                              buf[MAX_KEY_ID_LEN];
    size_t                            len     = sizeof(buf);
    qeo_openssl_engine_cmd_load_cert_chain_t params  = {0};
    
    if (_engine == NULL) {
        qeo_log_e("engine not initialized");
        return NULL;
    }

    params.friendlyName = realm_to_string_locked(&qeoSec->cfg.id, buf, &len);
    if (ENGINE_ctrl(_engine, QEO_OPENSSL_ENGINE_CMD_LOAD_CERT_CHAIN, 0, (void *)&params, NULL) == 0) {
        qeo_log_e("Could not load certificate chain for friendly name %s", params.friendlyName);
    }

    return params.chain;
}

/* check out chapter 5 of O'Reilly - Network security with OpenSSL, in particular p.100 and p.122 */
static qeo_mgmt_client_retcode_t configure_ssl_ctx_cb(SSL_CTX *ssl_ctx, void *cookie)
{
    security_util_configure_ssl_ctx(ssl_ctx);

    return QMGMTCLIENT_OK;
}

static EVP_PKEY *generate_private_key(void)
{
    EVP_PKEY_CTX  *ctx  = NULL;
    EVP_PKEY      *pkey = NULL;

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, _engine);
    do {
        if (ctx == NULL) {
            qeo_log_e("Could not init ctx.");
            break;
        }

        if (EVP_PKEY_keygen_init(ctx) <= 0) {
            qeo_log_e("Error in EVP_PKEY_keygen_init.");
            break;
        }

        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 1024) <= 0) {
            qeo_log_e("Error in EVP_PKEY_CTX_set_rsa_keygen_bits. ");
            break;
        }
        /* Generate key */
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
            /* Error */
            qeo_log_e("Error in EVP_PKEY_keygen. ");
            break;
        }
    } while (0);

    EVP_PKEY_CTX_free(ctx);

    return pkey;
}

#ifndef NDEBUG
static bool is_valid_qeosec(qeo_security_hndl qeoSec)
{
    if (qeoSec->magic != MAGIC_QEO_SECURITY) {
        qeo_log_e("*** 1 (invalid pointer ?) ***");
        return false;
    }

    if (qeoSec->mgmt_client_ctx == NULL) {
        qeo_log_e("*** 2 ***");
        return false;
    }

    if ((qeoSec->state == QEO_SECURITY_UNAUTHENTICATED) && ((qeoSec->pkey != NULL) || (qeoSec->certificate_chain != NULL))) {
        qeo_log_e("*** 3 ***");
        return false;
    }

    if (((qeoSec->pkey == NULL) && (qeoSec->certificate_chain != NULL)) ||
        ((qeoSec->pkey != NULL) && (qeoSec->certificate_chain == NULL))) {
        qeo_log_e("*** 4 ***");
        return false;
    }

    if (!((qeoSec->state >= QEO_SECURITY_UNAUTHENTICATED) || (qeoSec->state <= QEO_SECURITY_AUTHENTICATED))) {
        qeo_log_e("*** 5 ***");
        return false;
    }

    if (!((qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_NONE) ||
           qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION ||
          (qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP))) {
        qeo_log_e("*** 6 ***");
        return false;
    }

    if (qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_NONE) {
        if (qeoSec->reg_url != NULL) {
            qeo_log_e("*** 7 ***");
            return false;
        }
    }

    if (qeoSec->state == QEO_SECURITY_AUTHENTICATED){
        if (qeoSec->cfg.id.realm_id == 0 ||
            qeoSec->cfg.id.user_id == 0 ||
            qeoSec->cfg.id.friendly_name == NULL) {
            qeo_log_e("*** 8 ***");
            return false;
        }
    }

    return true;
}
#endif

qeo_retcode_t get_realms_locked(qeo_security_identity **sec_id, unsigned int *length /* OUT */){

    qeo_openssl_engine_cmd_get_friendly_names_t get_friendly_names;
    qeo_retcode_t ret;

    do {
        if (_initialized == false) {
            qeo_log_e("Error: not initialized");
            ret = QEO_EBADSTATE;
            break;
        }

        if (ENGINE_ctrl(_engine, QEO_OPENSSL_ENGINE_CMD_GET_FRIENDLY_NAMES, 0, (void *)&get_friendly_names, NULL) == 0) {
            qeo_log_e("Could not retrieve friendly_names");
            ret = QEO_EFAIL;
            break;
        }

        *length = get_friendly_names.number_of_friendly_names;
        if (*length != 0){
            if (*length != 1) {
                qeo_log_e("currently only 1 certificate is supported");
                ret = QEO_EBADSTATE;
                break;
            }

            *sec_id = (qeo_security_identity *)malloc(sizeof(**sec_id) * *length);
            if (*sec_id == NULL) {
                *length = 0;
                qeo_log_e("out of memory");
                ret = QEO_ENOMEM;
                break;
            }

            for (int i = 0; i < *length; ++i){
                qeo_security_identity *id = *sec_id + i;
                id->friendly_name = get_friendly_names.friendly_names[i];

                // TODO: Refactor in Q3
                if(qeo_platform_get_key_value("url",&id->url) != QEO_UTIL_OK){
                    qeo_log_e("qeo_platform_get_key_value failed");
                    id->url = strdup("");
                }

                /* parse the realm_id and user_id */
                if (sscanf(get_friendly_names.friendly_names[i], FRIENDLY_NAME_FORMAT, &id->realm_id, &id->device_id ,&id->user_id) != 3){
                    qeo_log_e("Malformed user-friendly name ?? (%s)", get_friendly_names.friendly_names[i]);
                }
                qeo_log_d("Realm's friendly name is %s", get_friendly_names.friendly_names[i]);
            }

            /* We only free() the array, not the strings, because we copied them by pointer !! */
            free(get_friendly_names.friendly_names);
        } else {
            *sec_id = NULL;
            qeo_log_d("No realms found");
        }

        ret = QEO_OK;
    } while (0);

    return ret;

}

static qeo_platform_security_state convert_internal_state_to_platform_security_state(qeo_security_state state){

    switch (state){
        case QEO_SECURITY_UNAUTHENTICATED:
            return QEO_PLATFORM_SECURITY_UNAUTHENTICATED;
        case QEO_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS:
            return QEO_PLATFORM_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS;
        case QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY:
            return QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
        case QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED:
            return QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
        case QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE:
            return QEO_PLATFORM_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
        case QEO_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS:
            return QEO_PLATFORM_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS;
        case QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS:
            return QEO_PLATFORM_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS;
        case QEO_SECURITY_STORING_QEO_CREDENTIALS:
            return QEO_PLATFORM_SECURITY_STORING_QEO_CREDENTIALS;
        case QEO_SECURITY_AUTHENTICATION_FAILURE:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE;
        case QEO_SECURITY_AUTHENTICATED:
            return QEO_PLATFORM_SECURITY_AUTHENTICATED;
    }

    return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE; /* to keep the compiler happy */
}

static bool get_ids_from_certificate( STACK_OF(X509) *certificate_chain, qeo_security_identity *sec_id)
{
    bool ret = false;
    qeo_mgmt_cert_contents contents = {0};

    do {

        if((qeo_mgmt_cert_parse(certificate_chain,&contents) !=  QCERT_OK)) {
            qeo_log_e("failed to get ids from certificate chain");
            break;
        }
        sec_id->realm_id = contents.realm;
        sec_id->device_id = contents.device;
        sec_id->user_id = contents.user;

        ret = true;

    }while(0);


    return ret;
}

static bool verify_qeo_credentials_locked(qeo_security_hndl qeoSec, EVP_PKEY *pkey, STACK_OF(X509) *certificate_chain)
{
    X509      *own_cert   = NULL;
    ASN1_TIME *notAfter   = NULL;
    qeo_security_identity sec_id = {0};
    


    /* verify our own certifcate (which is guaranteed to be the first in the stack !!!!)  */
    own_cert = sk_X509_value(certificate_chain, 0);
    /* verify key matches the cert */
    if (X509_check_private_key(own_cert, pkey) == 0) {
        qeo_log_e("Certificate does not match private key");
        return false;
    }

    /* check cryptography sanity (verify the chain..) */
    /* TODO IMPLEMENT THIS WITH X509 STORE STUFF BUT WE NEED TO GET THE ROOT CA FIRST...*/

    if (get_ids_from_certificate(certificate_chain, &sec_id) == false) {
        qeo_log_e("Could not get realm id");
        return false;
    }

    /* only check the realm if we know which realm to expect */
    if (qeoSec->cfg.id.realm_id != 0) {
        if (sec_id.realm_id != qeoSec->cfg.id.realm_id) {
            qeo_log_e("Expected keyid %" PRIx64 " != %" PRIx64, sec_id.realm_id, qeoSec->cfg.id.realm_id);
            return false;
        }
    }

    /* check date */
    notAfter  = X509_get_notAfter(own_cert);

    if(X509_cmp_current_time(notAfter) < 0) {
        qeo_log_e("Certificate expired");
        return false;

    }

    return true;
}

static bool verify_received_qeo_credentials_locked(qeo_security_hndl qeoSec, EVP_PKEY *pkey, STACK_OF(X509) *certificate_chain)
{
    update_authentication_state_locked(qeoSec, QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS);
    return verify_qeo_credentials_locked(qeoSec, pkey, certificate_chain);
}

static bool verify_loaded_qeo_credentials(qeo_security_hndl qeoSec, EVP_PKEY *pkey, STACK_OF(X509) *certificate_chain)
{
    bool retval;
    update_authentication_state(qeoSec, QEO_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS);

    qeosec_lock(qeoSec);
    retval = verify_qeo_credentials_locked(qeoSec, pkey, certificate_chain);
    qeosec_unlock(qeoSec);
    return retval;
}

static qeo_retcode_t set_otp_url_locked(qeo_security_hndl qeoSec, const char *otp, const char *url){

    qeo_retcode_t ret = QEO_EFAIL;
    qeo_util_retcode_t utilret = QEO_UTIL_EFAIL;
    char *otp_dup = NULL;
    char *url_dup = NULL;

    do {
        if (qeoSec->stop_reason != QEO_SECURITY_STOP_REASON_NONE){
            qeo_log_e("Already stopped ...");
            ret = QEO_EBADSTATE;                                                          
            break;
        }
        
        if (!(qeoSec->state == QEO_SECURITY_AUTHENTICATION_FAILURE || (qeoSec->reg_url == NULL && qeoSec->reg_otp == NULL))){
            qeo_log_e("Not valid to set registration credentials in this state");
            ret = QEO_EBADSTATE;
            break;
        }


        otp_dup = trim_whitespace_strdup(otp);
        url_dup= trim_whitespace_strdup(url);
        if (otp_dup == NULL || url_dup == NULL){
            ret = QEO_ENOMEM;
            break;
        }
        
        qeoSec->reg_otp = otp_dup;
        qeoSec->reg_url = url_dup;
        
        utilret = qeo_platform_set_key_value("url",url_dup);
        if(utilret != QEO_UTIL_OK) {
            qeo_log_e("qeo_platform_security_set_key_value failed");
            ret = QEO_EFAIL;
            break;
        }

        if (pthread_cond_signal(&qeoSec->reg_cred_received_cond) < 0) {
            qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
            ret = QEO_EFAIL;
            break;
        }

        ret = QEO_OK;

    } while (0);

    if (ret != QEO_OK){
        free(otp_dup);
        free(url_dup);
        qeoSec->reg_otp = NULL;
        qeoSec->reg_url = NULL;
    }

    return ret;
}

static void on_remote_qeo_registration_credentials(qeo_remote_registration_hndl_t remote_reg, const char *otp, const char *realm_name, const char *url){

    assert(remote_reg != NULL);
    assert(otp != NULL);
    assert(url != NULL);

    qeo_security_hndl qeoSec = NULL;
    uintptr_t user_data;
    qeo_retcode_t ret;

    do { 

        qeo_log_i("Setting remote reg , otp: %s, realm: %s, url: %s", otp, realm_name, url);
        ret = qeo_remote_registration_get_user_data(remote_reg, &user_data);
        if (ret != QEO_OK){
            qeo_log_e("Could not retrieve user data");
            break;
        }
        qeoSec = (qeo_security_hndl)user_data;

        qeosec_lock(qeoSec);
        assert(is_valid_qeosec(qeoSec) == true);
        qeoSec->realm_name = strdup(realm_name);
        qeoSec->unconfirmed_reg_otp = trim_whitespace_strdup(otp);
        qeoSec->unconfirmed_reg_url = trim_whitespace_strdup(url);
        if (qeoSec->realm_name == NULL || qeoSec->unconfirmed_reg_otp == NULL || qeoSec->unconfirmed_reg_url == NULL){
            qeo_log_e("Could not allocate memory");
            ret = QEO_ENOMEM;
            qeosec_unlock(qeoSec);
            break;
        }

        if (pthread_cond_signal(&qeoSec->reg_cred_received_cond) < 0) {
            qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
            ret = QEO_EFAIL;
            qeosec_unlock(qeoSec);
            break;
        }

        qeosec_unlock(qeoSec);

    } while (0);

    if (ret != QEO_OK){
        /* TODO: DO SOMETHING ? */
    }

}

static qeo_util_retcode_t on_remote_registration_credentials_feedback_cb(qeo_platform_security_context_t context, bool ok){

    qeo_util_retcode_t ret = QEO_UTIL_EFAIL;
    qeo_security_hndl qeoSec = (qeo_security_hndl)context;

    qeosec_lock(qeoSec);
    do {
        qeoSec->remote_reg_cred_confirmation_feedback_received = true;

        if (qeo_remote_registration_enable_using_new_registration_credentials(qeoSec->remote_reg) != QEO_OK) {
                ret = QEO_UTIL_EINVAL;
        }

        if (ok == true){
            if (set_otp_url_locked(qeoSec, qeoSec->unconfirmed_reg_otp, qeoSec->unconfirmed_reg_url) != QEO_OK){
                ret = QEO_UTIL_EFAIL;
                break;
            }
        }

        if (pthread_cond_signal(&qeoSec->remote_reg_cred_confirmation_feedback_cond) < 0) {
            qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
            ret = QEO_UTIL_EFAIL;
            break;
        }

        ret = QEO_UTIL_OK;
    } while (0);

    free(qeoSec->unconfirmed_reg_otp);
    free(qeoSec->unconfirmed_reg_url);
    qeoSec->unconfirmed_reg_url = NULL;
    qeoSec->unconfirmed_reg_otp = NULL;

    qeosec_unlock(qeoSec);

    return ret;
}


/* this function is a call-back which will be called from outside */
static qeo_util_retcode_t set_registration_credentials(qeo_platform_security_context_t context, const qeo_platform_security_registration_credentials *reg_cred)
{
    qeo_util_retcode_t                         ret           = QEO_UTIL_OK;
    qeo_security_hndl qeoSec = (qeo_security_hndl)context;

    if ((qeoSec == NULL) || (reg_cred == NULL)) {
        return QEO_UTIL_EINVAL;
    }

    /* input checking */
    switch (reg_cred->reg_method) {
        case QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP:
            if (reg_cred->u.otp.url == NULL) {
                qeo_log_e("No URL specified");
                return QEO_UTIL_EINVAL;
            }
            if (reg_cred->u.otp.otp == NULL) {
                qeo_log_e("No OTP specified");
                return QEO_UTIL_EINVAL;
            }
            break;
        case QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION:
            break;
        case QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_NONE:
            qeo_log_e("Does not make sense to set registration credentials without a method");
            return QEO_UTIL_EINVAL;
    }

    api_lock();
    if (_initialized == false) {
        ret = QEO_UTIL_EBADSTATE;
        api_unlock();
        return ret;
    }

    qeosec_lock(qeoSec);

    do {
        qeoSec->reg_method = reg_cred->reg_method;
        if (pthread_cond_signal(&qeoSec->reg_method_received_cond) < 0) {
            qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
            ret = QEO_UTIL_EFAIL;
        }

        switch (reg_cred->reg_method){
            case QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP:
                if (set_otp_url_locked(qeoSec, reg_cred->u.otp.otp, reg_cred->u.otp.url) != QEO_OK){
                    ret = QEO_UTIL_EFAIL;
                    break;
                }
                break;
            case QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION:
                qeoSec->registration_window = reg_cred->u.remote_registration.registration_window;
                qeoSec->suggested_username = strdup(reg_cred->u.remote_registration.suggested_username);
                /* strdup() failure is here not critical */
                
                break;
            case QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_NONE:
                assert(false);
        }

        ret = QEO_UTIL_OK;
    } while (0);

    qeosec_unlock(qeoSec);
    api_unlock();

    return ret;
}

static qeo_util_retcode_t retrieving_registration_credentials_stop(qeo_security_hndl qeoSec, qeo_security_stop_reason_t stop_reason){

    qeo_util_retcode_t                         ret           = QEO_UTIL_OK;

    if (qeoSec == NULL)  {
        return QEO_UTIL_EINVAL;
    }

    api_lock();
    if (_initialized == false) {
        ret = QEO_UTIL_EBADSTATE;
        api_unlock();
        return ret;
    }

    qeosec_lock(qeoSec);
    qeoSec->stop_reason = stop_reason;

    if (qeoSec->remote_reg != NULL) {
        if (qeo_remote_registration_enable_using_new_registration_credentials(qeoSec->remote_reg) != QEO_OK) {
            ret = QEO_UTIL_EINVAL;
        }
    }

    if (pthread_cond_signal(&qeoSec->reg_method_received_cond) < 0) {
        qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
        ret = QEO_UTIL_EFAIL;
    }

    if (pthread_cond_signal(&qeoSec->reg_cred_received_cond) < 0) {
        qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
        ret = QEO_UTIL_EFAIL;
    }

    if (pthread_cond_signal(&qeoSec->remote_reg_cred_confirmation_feedback_cond) < 0) {
        qeo_log_e("Failure while sending condition signal(%s)", strerror(errno));
        ret = QEO_UTIL_EFAIL;
    }

    qeosec_unlock(qeoSec);
    api_unlock();

    return ret;
}

static qeo_util_retcode_t retrieving_registration_credentials_user_cancel(qeo_platform_security_context_t cookie)
{
    qeo_security_hndl qeoSec = (qeo_security_hndl)cookie;
    return retrieving_registration_credentials_stop(qeoSec, QEO_SECURITY_STOP_REASON_USER_CANCEL);
}

static qeo_platform_security_state_reason qeo_client_ret_to_state_reason(qeo_mgmt_client_retcode_t ret){

    switch (ret){
        case QMGMTCLIENT_OK:
            return QEO_PLATFORM_SECURITY_REASON_UNKNOWN;
        case QMGMTCLIENT_EFAIL:
        case QMGMTCLIENT_ENOTALLOWED:
        case QMGMTCLIENT_EINVAL:
        case QMGMTCLIENT_EMEM:
        case QMGMTCLIENT_EBADREQUEST:
        case QMGMTCLIENT_EBADSERVICE:
        case QMGMTCLIENT_EHTTP:
        case QMGMTCLIENT_EBADREPLY:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
        case QMGMTCLIENT_EOTP:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INVALID_OTP;
        case QMGMTCLIENT_ECONNECT:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_NETWORK_FAILURE;
        case QMGMTCLIENT_ESSL:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_SSL_HANDSHAKE_FAILURE;

    }
    return QEO_PLATFORM_SECURITY_REASON_UNKNOWN;

}

static void on_remote_registration_timeout(qeo_remote_registration_hndl_t remote_reg){

    uintptr_t user_data;
    qeo_remote_registration_get_user_data(remote_reg, &user_data);
    retrieving_registration_credentials_stop((qeo_security_hndl)user_data, QEO_SECURITY_STOP_REASON_REMOTE_REGISTRATION_TIMEOUT);

}

static qeo_retcode_t start_remote_registration_locked(qeo_security_hndl qeoSec, EVP_PKEY *pkey){

    const qeo_remote_registration_init_cfg_t init_cfg = {
        .on_qeo_registration_credentials = on_remote_qeo_registration_credentials,
        .on_qeo_registration_timeout = on_remote_registration_timeout
    };

    const qeo_remote_registration_cfg_t cfg = {
        .registration_window = qeoSec->registration_window,
        .suggested_username = qeoSec->suggested_username,
        .pkey = pkey,
        .user_data = (uintptr_t)qeoSec
    };

    qeo_retcode_t ret = QEO_EFAIL;
    qeo_remote_registration_hndl_t remote_reg;
    
    do {
        if (qeo_remote_registration_init(&init_cfg) != QEO_OK){
            qeo_log_e("Could not init remote registration");
            ret = QEO_EFAIL;
            break;
        }

        if ((ret = qeo_remote_registration_construct(&cfg, &qeoSec->remote_reg)) != QEO_OK){
            qeo_log_e("Could not construct remote registration object");
            break;
        }

        
        remote_reg = qeoSec->remote_reg;
        if ((ret = qeo_remote_registration_start(remote_reg)) != QEO_OK){
            qeo_log_e("Could not start remote registration object");
            qeosec_lock(qeoSec);
            break;
        }

        ret = QEO_OK;

    } while (0);
    
    if (ret != QEO_OK){
        qeo_remote_registration_destruct(&qeoSec->remote_reg);
    }
    
    return ret;
}

static qeo_platform_security_state_reason stop_reason_to_platform_security_reason(qeo_security_stop_reason_t stop_reason){

    switch(stop_reason){
        case QEO_SECURITY_STOP_REASON_NONE:
            return QEO_PLATFORM_SECURITY_REASON_UNKNOWN;
        case QEO_SECURITY_STOP_REASON_USER_CANCEL:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_CANCELLED;
        case QEO_SECURITY_STOP_REASON_REMOTE_REGISTRATION_TIMEOUT:
            return QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT;
    }
    return QEO_PLATFORM_SECURITY_REASON_UNKNOWN;

}

static bool wait_for_registration_method(qeo_security_hndl qeoSec, EVP_PKEY **pkey, qeo_platform_security_state_reason *reason){

    pthread_t key_gen_thread;
    qeo_util_retcode_t               qeoutilret = QEO_UTIL_OK;
    bool retval = false;

    /* generate keypair */
    /* as this can take up to a few seconds, it is a good idea do this while we are waiting for the user to enter the OTP */
    if (pthread_create(&key_gen_thread, NULL, (void *(*)(void *))generate_private_key, NULL) != 0){
        qeo_log_e("Could not start keygenthread");
        *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
        return retval;
    }
    qeocore_remote_registration_init_cond();
    qeoutilret = qeo_platform_security_registration_credentials_needed((qeo_platform_security_context_t)qeoSec, 
            set_registration_credentials,
            retrieving_registration_credentials_user_cancel);
    qeosec_lock(qeoSec);

    if (pthread_join(key_gen_thread, (void **)(void *)pkey) != 0){
        qeo_log_e("Could not join keygenthread");
        *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
        qeosec_unlock(qeoSec);
        return retval;
    }
    //pass keypair to remote registration anyway so it can still use it (eg android)
    qeocore_remote_registration_set_key(*pkey);

    if (qeoutilret != QEO_UTIL_OK) {
        qeo_log_e("Not able to request registration method/credentials.");
        *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_PLATFORM_FAILURE;
        qeosec_unlock(qeoSec);
        return retval;
    }

    if (*pkey == NULL) {
        qeo_log_e("Could not construct keypair");
        *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
        qeosec_unlock(qeoSec);
        return retval;
    }
    update_authentication_state_locked(qeoSec, QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED);

    do {
        while (qeoSec->stop_reason == QEO_SECURITY_STOP_REASON_NONE && qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_NONE) {
            qeo_log_d("Waiting until we get registration method...");
            if (pthread_cond_wait(&qeoSec->reg_method_received_cond, &qeoSec->mutex) < 0) {
                qeo_log_e("Error while waiting for condition variable(%s)", strerror(errno));
            }
        }

        if (qeoSec->stop_reason != QEO_SECURITY_STOP_REASON_NONE){
            qeo_log_d("Retrieving registration method was stopped (%d)", qeoSec->stop_reason);
            *reason = stop_reason_to_platform_security_reason(qeoSec->stop_reason);
            break;
        }

        if (qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION){
            if ((start_remote_registration_locked(qeoSec, *pkey)) != QEO_OK){
                qeo_log_e("Could not start remote registration");
                *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
                break;
            }
        }

        retval = true;
    } while (0);
    qeosec_unlock(qeoSec);                               

    return retval;
}

static bool wait_for_remote_registration_credentials_locked(qeo_security_hndl qeoSec, EVP_PKEY *pkey, qeo_platform_security_state_reason *reason){

    qeo_remote_registration_failure_reason_t failure_reason = convert_platform_security_reason_to_remote_registration_failure_reason(*reason);
    qeo_util_retcode_t qeoutilret;

    do {
        /* go to unregistered state */
        if (qeo_remote_registration_update_registration_status(qeoSec->remote_reg, 
                                                               QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED, 
                                                               failure_reason) != QEO_OK){
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }

        while (qeoSec->stop_reason == QEO_SECURITY_STOP_REASON_NONE && 
                qeoSec->reg_otp == NULL /* we could still get local OTP */ && 
                qeoSec->unconfirmed_reg_otp == NULL){
            qeo_log_d("Waiting until we get remote registration credentials...");
            if (pthread_cond_wait(&qeoSec->reg_cred_received_cond, &qeoSec->mutex) < 0) {
                qeo_log_e("Error while waiting for condition variable(%s)", strerror(errno));
            }
        }

        if (qeoSec->stop_reason != QEO_SECURITY_STOP_REASON_NONE){
            *reason = stop_reason_to_platform_security_reason(qeoSec->stop_reason);
            return false;
        }

        if (qeoSec->reg_otp != NULL){
            return true;
        }
        assert(qeoSec->unconfirmed_reg_otp != NULL);

        /* go to registering state */
        if (qeo_remote_registration_update_registration_status(qeoSec->remote_reg, 
                                                               QEO_REMOTE_REGISTRATION_STATUS_REGISTERING, 
                                                               failure_reason) != QEO_OK){
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }

        /* ask for confirmation */
        const qeo_platform_security_remote_registration_credentials_t rrcred = {
            .realm_name = strdup(qeoSec->realm_name),
            .url = strdup(qeoSec->unconfirmed_reg_url),
        };

        if (rrcred.realm_name == NULL || rrcred.url == NULL){
            free((char *)rrcred.realm_name);
            free((char *)rrcred.url);
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            return false;
        }

        qeoSec->remote_reg_cred_confirmation_feedback_received = false;
        qeosec_unlock(qeoSec);
        qeoutilret = qeo_platform_security_remote_registration_confirmation_needed((qeo_platform_security_context_t)qeoSec,
                &rrcred,
                on_remote_registration_credentials_feedback_cb);

        free((char *)rrcred.realm_name);
        free((char *)rrcred.url);

        if (qeoutilret != QEO_UTIL_OK){
            qeo_log_e("could not ask for confirmation");
            return false;
        }

        /* block until any confirmation */
        qeosec_lock(qeoSec);
        while (qeoSec->remote_reg_cred_confirmation_feedback_received == false){
            qeo_log_d("Waiting until we get confirmation feedback...");
            if (pthread_cond_wait(&qeoSec->remote_reg_cred_confirmation_feedback_cond, &qeoSec->mutex) < 0) {
                qeo_log_e("Error while waiting for condition variable(%s)", strerror(errno));
            }
        }
        free(qeoSec->realm_name);
        qeoSec->realm_name = NULL;

        if (qeoSec->stop_reason != QEO_SECURITY_STOP_REASON_NONE){
            *reason = stop_reason_to_platform_security_reason(qeoSec->stop_reason);
            return false;
        }

        assert(qeoSec->remote_reg_cred_confirmation_feedback_received != false);
        if (qeoSec->reg_otp == NULL){ /* this is how we find out whether we got positive or negative feedback */
            qeo_log_d("We have to ask the management app for new registration credentials");
            failure_reason = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NEGATIVE_CONFIRMATION;
        }

    } while (qeoSec->reg_otp == NULL);

    return true;

}

static bool wait_for_registration_credentials(qeo_security_hndl qeoSec, EVP_PKEY *pkey, qeo_platform_security_state_reason *reason){

    bool retval = false;

    qeosec_lock(qeoSec);
    do {
        if (qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION){
            if (wait_for_remote_registration_credentials_locked(qeoSec, pkey, reason) == false){
                break;
            }
        }

        while (qeoSec->stop_reason == QEO_SECURITY_STOP_REASON_NONE && qeoSec->reg_otp == NULL) {
            qeo_log_d("Waiting until we get registration credentials...");
            if (pthread_cond_wait(&qeoSec->reg_cred_received_cond, &qeoSec->mutex) < 0) {
                qeo_log_e("Error while waiting for condition variable(%s)", strerror(errno));
            }
        }

        if (qeoSec->stop_reason != QEO_SECURITY_STOP_REASON_NONE){
            qeo_log_d("Retrieving registration credentials was stopped");
            *reason = stop_reason_to_platform_security_reason(qeoSec->stop_reason);
            break;
        }

        retval = true;

    } while (0);
    qeosec_unlock(qeoSec);                               

    return retval;
}

static bool perform_registration(qeo_security_hndl qeoSec, const qeo_platform_device_info *device_info, EVP_PKEY *pkey, qeo_platform_security_state_reason *reason){

    STACK_OF(X509) * certificate_chain = NULL;
    X509*            device_cert = NULL;
    qeo_openssl_engine_cmd_save_credentials_t save_params = {0};
    bool                        retval = false;
    char key_id[MAX_KEY_ID_LEN];
    qeo_security_identity sec_id = {0};
    size_t        keyid_len = sizeof(key_id);
    qeo_mgmt_client_retcode_t    client_ret;

    
    qeosec_lock(qeoSec);
    do {
        update_authentication_state_locked(qeoSec, QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE);
        certificate_chain = sk_X509_new_null();
        if (certificate_chain == NULL) {
            qeo_log_e("Could not allocate certificate chain!");
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }


        client_ret = qeo_mgmt_client_enroll_device(qeoSec->mgmt_client_ctx, qeoSec->reg_url, pkey, qeoSec->reg_otp, device_info, configure_ssl_ctx_cb, qeoSec, certificate_chain);
        if (client_ret != QMGMTCLIENT_OK) {
            qeo_log_e("Could not enroll device !");
            *reason = qeo_client_ret_to_state_reason(client_ret);
            break;
        }

        qeo_log_i("Received qeo credentials !");

        if (verify_received_qeo_credentials_locked(qeoSec, pkey, certificate_chain) == false) {
            qeo_log_e("Received credentials are invalid!");
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_RECEIVED_INVALID_CREDENTIALS;
            break;
        }

        device_cert = sk_X509_value(certificate_chain, 0);

        if (get_ids_from_certificate(certificate_chain, &sec_id) == false) {
            qeo_log_e("Could not get realm id");
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }

        generate_friendly_name(&sec_id, key_id, &keyid_len);

        /* received certificate probably does not contain friendly name yet. */
        if (X509_keyid_set1(device_cert, (unsigned char *)key_id, keyid_len) == 0) {
            qeo_log_e("Could not set realm as friendly name");
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }

        /* store qeo-credentials */
        update_authentication_state_locked(qeoSec, QEO_SECURITY_STORING_QEO_CREDENTIALS);
        save_params.friendlyName  = key_id;
        save_params.pkey          = pkey;
        save_params.chain         = certificate_chain;
        if (ENGINE_ctrl(_engine, QEO_OPENSSL_ENGINE_CMD_SAVE_CREDENTIALS, 0, (void *)&save_params, NULL) == 0) {
            qeo_log_e("Could not store qeo-credentials!");
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_STORE_FAILURE;
            break;
        }

        qeoSec->pkey = pkey;
        qeoSec->certificate_chain = certificate_chain;
        if ((sec_id.friendly_name = strdup(key_id)) == NULL){
            qeo_log_e("Could not store friendly name");
            *reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }
        qeoSec->cfg.id = sec_id;

        retval =  true;

    } while (0);

    free(qeoSec->reg_otp);
    free(qeoSec->reg_url);
    qeoSec->reg_otp = NULL;
    qeoSec->reg_url = NULL;

    qeosec_unlock(qeoSec);

    if (retval == false){
        if (certificate_chain != NULL) {
            sk_X509_free(certificate_chain);
        }
    }

    return retval;
}

static bool can_retry_registration(qeo_security_hndl qeoSec){

    bool retval = false;
    qeosec_lock(qeoSec);
    if (qeoSec->stop_reason == QEO_SECURITY_STOP_REASON_NONE &&
        qeoSec->reg_method == QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION){
        retval = true;
    }
    qeosec_unlock(qeoSec);

    return retval;
}

static bool register_qeo(qeo_security_hndl qeoSec)
{
    const qeo_platform_device_info *device_info = NULL;
    bool registration_result = false;
    qeo_platform_security_state_reason reason = QEO_PLATFORM_SECURITY_REASON_UNKNOWN;
    EVP_PKEY                    *pkey       = NULL;
    bool retval = false;


    update_authentication_state(qeoSec, QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY);

    do {
        if ((device_info = qeo_platform_get_device_info()) == NULL) {
            qeo_log_e("Could not retrieve device info");
            reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR;
            break;
        }

        if (wait_for_registration_method(qeoSec, &pkey, &reason) == false){
            qeo_log_e("Waiting for registration method failed");
            break;
        }

        if (wait_for_registration_credentials(qeoSec, pkey, &reason) == false){
            qeo_log_e("Waiting for registration credentials failed");
            break;
        }

        registration_result = perform_registration(qeoSec, device_info, pkey, &reason);

        while (can_retry_registration(qeoSec) == true && registration_result == false){
            if (wait_for_registration_credentials(qeoSec, pkey, &reason) == false){
                qeo_log_e("Waiting for registration credentials failed");
                break;
            }

            registration_result = perform_registration(qeoSec, device_info, pkey, &reason);
        }


        if (registration_result == true){
            retval = true;
        }

    } while (0);

    if (retval == false) {
        EVP_PKEY_free(pkey);
    }

    qeosec_set_reason(qeoSec, reason);
    qeosec_lock(qeoSec);
    /* no more need for remote registration object if there was one */
    qeo_remote_registration_destruct(&qeoSec->remote_reg);
    qeosec_unlock(qeoSec);

    return retval;
}

static bool clear_qeo_credentials(qeo_security_hndl qeoSec)
{
    /* ask the engine to throw away security stuff */

    return true;
}

#ifdef NO_CONCURRENT_REGISTRATION
static bool take_qeo_registration_lock(void){


    qeo_log_d("Taking lock");
    if (flock (_qeo_registration_fd, LOCK_EX) == -1){
        qeo_log_e("flock() failed: %s", strerror(errno));
        return false;
    }
    qeo_log_d("Lock taken");

    return true;

}

static bool release_qeo_registration_lock(void){

    qeo_log_d("Releasing lock");
    if (flock (_qeo_registration_fd, LOCK_UN) == -1){
        qeo_log_e("flock() failed: %s", strerror(errno));
        return false;
    }
    qeo_log_d("Lock released");

    return true;

}
#endif

static bool has_identity_locked(qeo_security_hndl qeoSec){

    if (qeoSec->cfg.id.realm_id == 0){
        return false;
    }

    return true;

}

/* thread implementation */
/* this implementation is not safe for pthread_cancel() --> it may leak resources */
static bool authenticate(qeo_security_hndl qeoSec)
{
    EVP_PKEY *pkey = NULL;

    STACK_OF(X509) * certificate_chain = NULL;
    bool retval = false;
    char key_id[MAX_KEY_ID_LEN];
    size_t        keyid_len = sizeof(key_id);
    qeo_security_identity *identities = NULL;
    unsigned int number_of_identities;
    qeo_retcode_t ret;

    qeo_log_i("Authentication started");
    
    
#ifdef NO_CONCURRENT_REGISTRATION
    take_qeo_registration_lock();
#endif

    qeosec_lock(qeoSec);
    if (has_identity_locked(qeoSec) == false){
        /* let's see if there is an identity we can use */ 
        if ((ret = get_realms_locked(&identities, &number_of_identities)) == QEO_OK) {
            if (number_of_identities > 0) {
                memcpy(&qeoSec->cfg.id, &identities[0], sizeof(qeoSec->cfg.id));
            }
            qeo_security_free_realms(&identities, number_of_identities);
        }
        else {
            qeo_log_e("Get QEO identities failed");

            qeosec_unlock(qeoSec);
            update_authentication_state(qeoSec, QEO_SECURITY_AUTHENTICATION_FAILURE);
#ifdef NO_CONCURRENT_REGISTRATION
            release_qeo_registration_lock();
#endif
            return false;
        }

    }

    qeo_log_i("We will use realm_id %" PRIx64 ", user_id %"PRIx64, qeoSec->cfg.id.realm_id, qeoSec->cfg.id.user_id);
    if (qeoSec->cfg.id.realm_id != 0) {
        update_authentication_state_locked(qeoSec, QEO_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS);
        pkey = load_private_key_locked(qeoSec);
        certificate_chain = load_certificate_chain_locked(qeoSec);

        if (pkey == NULL) {
            qeo_log_w("No private key loaded");
        }
        if (certificate_chain == NULL) {
            qeo_log_w("No certificate chain loaded");
        }
    }
    qeosec_unlock(qeoSec);

    if ((pkey == NULL) && (certificate_chain == NULL)) {
        /* no stored credentials exist or can be loaded, we have to register first */
        qeo_log_d("no stored credentials exist or can be loaded, we have to register first");
        retval = register_qeo(qeoSec);
    }
    else {
        /* we loaded the stored credentials, let's verify them */
        if ((pkey != NULL) && (certificate_chain != NULL) && (verify_loaded_qeo_credentials(qeoSec, pkey, certificate_chain) == true)) {
            /* copy the credentials into qeoSec (resource allocation was done in the engine, eventually it is our responsbility to free it, when we destruct the qeoSec) */
            qeosec_lock(qeoSec);
            generate_friendly_name(&qeoSec->cfg.id, key_id, &keyid_len);
            qeoSec->pkey = pkey;
            qeoSec->certificate_chain = certificate_chain;
            if (get_ids_from_certificate(certificate_chain, &qeoSec->cfg.id) == false) {
                qeo_log_e("Could not get realm id");
                retval = false;
            }
            else if ((qeoSec->cfg.id.friendly_name = strdup(key_id)) == NULL){
                qeo_log_e("Could not store friendly name");
                qeosec_set_reason_locked(qeoSec, QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR);
                retval = false;
            } else {
                retval = true;
            }

            qeosec_unlock(qeoSec);
        }
        else {
            /* no discrimination at all WHY credentials are invalid. Not needed now */
            qeo_log_e("Credentials invalid !");
            EVP_PKEY_free(pkey);
            if (certificate_chain != NULL) {
                sk_X509_pop_free(certificate_chain, X509_free);
            }

            /* if the stored qeo-credentials are invalid, we better reset them ? */
            clear_qeo_credentials(qeoSec);

            retval = register_qeo(qeoSec);
        }
    }

    if (retval == true) {
        update_authentication_state(qeoSec, QEO_SECURITY_AUTHENTICATED);
    }
    else {
        update_authentication_state(qeoSec, QEO_SECURITY_AUTHENTICATION_FAILURE);
    }

#ifdef NO_CONCURRENT_REGISTRATION
    release_qeo_registration_lock();
#endif

    return retval;
}

static void free_qeosec(qeo_security_hndl qeoSec)
{
    qeo_remote_registration_destruct(&qeoSec->remote_reg);
    EVP_PKEY_free(qeoSec->pkey);
    if (qeoSec->certificate_chain != NULL) {
        sk_X509_pop_free(qeoSec->certificate_chain, X509_free);
    }

    free(qeoSec->reg_url);
    free(qeoSec->reg_otp);
    free(qeoSec->cfg.id.friendly_name);
    free(qeoSec->suggested_username);
    free(qeoSec->realm_name);
    free(qeoSec->unconfirmed_reg_otp);
    free(qeoSec->unconfirmed_reg_url);

    qeo_mgmt_client_clean(qeoSec->mgmt_client_ctx);

    free(qeoSec);
}

/* should be called in qeosec_locked state */
static qeo_retcode_t start_authentication(qeo_security_hndl qeoSec)
{
    if (qeoSec->thread != 0){
        qeo_log_e("Authentication already started !");
        return QEO_EBADSTATE;
    }

    if (pthread_create(&qeoSec->thread, NULL, (void *(*)(void *))authenticate, qeoSec) != 0) {
        qeo_log_e("Could not create thread");
        return QEO_EFAIL;
    }

    return QEO_OK;
}

static bool init_openSSL(void)
{
    if (qeo_openssl_engine_init() != QEO_OPENSSL_ENGINE_OK){
        qeo_log_e("Could not init engine");
        return false;
    }

    DDS_Security_set_library_init (0);
    DDS_Security_set_library_lock ();

    OpenSSL_add_all_algorithms();
    SSL_library_init();

    return true;
}

static ENGINE *init_engine(const char *id)
{
    ENGINE *engine = NULL;

    do {
        engine = ENGINE_by_id(id);
        if (engine == NULL) {
            qeo_log_e("Could not find engine %s", id);
            break;
        }

        if (!ENGINE_init(engine)) {
            qeo_log_e("Could not init engine %s", id);
            break;
        }
        qeo_log_d("created openssl engine");
    } while (0);

    return engine;
}

static void destroy_engine(ENGINE *e)
{
    if (e != NULL) {
        qeo_log_d("destroy openssl engine");
        ENGINE_finish(e);
        ENGINE_free(e);
        ENGINE_cleanup();
    }
}

static qeo_retcode_t qeo_security_init_locked(void)
{
    qeo_retcode_t ret = QEO_OK;

    do {
        if (_initialized == false) {
            if (init_openSSL() == false){
                qeo_log_e("Could not init openssl");
                ret = QEO_EFAIL;
                break;
            }

            if (qeo_openssl_engine_get_engine_id(&_engine_id) != QEO_OPENSSL_ENGINE_OK){
                qeo_log_e("Could not get engine id");
                ret = QEO_EFAIL;
                break;
            }

            _engine = init_engine(_engine_id);
            if (_engine == NULL) {
                qeo_log_e("Could not init engine");
                ret = QEO_EFAIL;
                break;
            }


#ifdef NO_CONCURRENT_REGISTRATION
            char path[32];
/* if you are wondering why this is not in the same directory as the credentials:  
 * flock() methods will not work reliably on NFS
 * mounted filesystems. It is quite likely the qeo storage directory IS in a
 * NFS mounted filesystem (for every Qeo developer this is by default the case).
 * Therefore we construct a path in /tmp, assuming every system at least has a writable
 * /tmp directory
 */
            snprintf(path, sizeof(path), "/tmp/.qeo_reg_%u.lock", getuid());
            _qeo_registration_fd = creat(path, 0700);
            if (_qeo_registration_fd < 0){
                qeo_log_e("Could not create file (%s)", strerror(errno));
                break;
            }
#endif

        }

        _initialized  = true;
        ret           = QEO_OK;
    } while (0);

    if (ret != QEO_OK){
        free(_engine_id);
    }

    return ret;
}

/* borrowed from openssl engine tests */
static void openSSL_rough_cleanup(void)
{
    // Some clean-up to avoid memory leaks : see http://www.openssl.org/support/faq.html#PROG13
    ERR_remove_state(getpid());
    CONF_modules_unload(0);

    DDS_Security_unset_library_lock ();

    EVP_cleanup();
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
}

/*########################################################################
#                                                                       #
#  PUBLIC FUNCTION SECTION                                             #
#                                                                       #
########################################################################*/
qeo_retcode_t qeo_security_init(void)
{
    qeo_retcode_t ret = QEO_OK;

    api_lock();
    ret = qeo_security_init_locked();
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_destroy(void)
{
    qeo_retcode_t ret = QEO_OK;

    api_lock();

    if (_initialized == true) {
        do {
            if (_qeosec_objects_count > 0) {
                qeo_log_e("We still have %d objects --> destruct them first", _qeosec_objects_count);
                ret = QEO_EBADSTATE;
                break;
            }

            qeo_remote_registration_destroy();
            destroy_engine(_engine);
            _engine = NULL;
            free(_engine_id);
            openSSL_rough_cleanup();

#ifdef NO_CONCURRENT_REGISTRATION
            close(_qeo_registration_fd);
#endif
            
            _initialized = false;
        } while (0);
    }

    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_construct(const qeo_security_config *cfg, qeo_security_hndl *qeoSec)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if ((cfg == NULL) || (qeoSec == NULL)) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            if ((ret = qeo_security_init_locked()) != QEO_OK) {
                qeo_log_e("Security initialization failed");
                break;
            }
        }

        *qeoSec = calloc(1, sizeof(**qeoSec));
        if (*qeoSec == NULL) {
            ret = QEO_ENOMEM;
            break;
        }

        cfg_qeosec(*qeoSec, cfg);

        (*qeoSec)->mgmt_client_ctx = qeo_mgmt_client_init();
        if ((*qeoSec)->mgmt_client_ctx == NULL) {
            free(*qeoSec);
            *qeoSec = NULL;
            ret = QEO_EFAIL;
            break;
        }

        assert(is_valid_qeosec(*qeoSec));
        ++_qeosec_objects_count;

        ret = QEO_OK;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_authenticate(qeo_security_hndl qeoSec)
{
    qeo_retcode_t ret;

    if (qeoSec == NULL) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            if ((ret = qeo_security_init_locked()) != QEO_OK) {
                qeo_log_e("Security initialization failed");
                break;
            }
        }
        qeosec_lock(qeoSec);
        ret = start_authentication(qeoSec);
        qeosec_unlock(qeoSec);
    } while (0);
    api_unlock();

    return ret;
}

/*
   in any case, this function should process the LAST reference to this object,
   no more references to this object should be dangling around !
 */
qeo_retcode_t qeo_security_destruct(qeo_security_hndl *_qeoSec)
{
    qeo_retcode_t     ret     = QEO_OK;
    qeo_security_hndl qeoSec  = NULL;
    pthread_t thread = 0;

    if (_qeoSec == NULL) {
        return QEO_OK;
    }
    qeoSec = *_qeoSec;
    if (qeoSec == NULL) {
        return QEO_OK;
    }

    api_lock();
    if (_initialized == false) {
        api_unlock();
        return QEO_EBADSTATE;
    }

    qeosec_lock(qeoSec);
    assert(_qeosec_objects_count > 0);
    do {
        /* stop only in initial or final state */
        if (!(qeoSec->state == QEO_SECURITY_UNAUTHENTICATED || qeoSec->state == QEO_SECURITY_AUTHENTICATED || qeoSec->state == QEO_SECURITY_AUTHENTICATION_FAILURE)) {
            qeo_log_e("Authentication ongoing; cannot destruct qeoSec in state (%d)", qeoSec->state);
            // TODO: mark this object as to be destructed and clean up on its own once authentication is stopped
            ret = QEO_EBADSTATE;
            break;
        }
        thread = qeoSec->thread;
        qeosec_unlock(qeoSec);

        if (thread != 0){
            if (pthread_join(thread, NULL) < 0){
                qeo_log_e("Could not join");
            }
        }

        *_qeoSec = NULL;
        --_qeosec_objects_count;

        api_unlock();
        free_qeosec(qeoSec);

        ret = QEO_OK;
    } while (0);
    if (ret != QEO_OK) {
        qeosec_unlock(qeoSec);
        api_unlock();
    }
    return ret;
}

qeo_retcode_t qeo_security_get_credentials(qeo_security_hndl qeoSec, EVP_PKEY **key, STACK_OF(X509) **certs)
{
    qeo_retcode_t ret = QEO_OK;

    api_lock();
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }

        if ((key == NULL) || (certs == NULL)) {
            ret = QEO_EINVAL;
            break;
        }

        qeosec_lock(qeoSec);
        if (qeoSec->state != QEO_SECURITY_AUTHENTICATED) {
            qeo_log_e("Not authenticated");
            ret = QEO_EBADSTATE;
            qeosec_unlock(qeoSec);
            break;
        }

        *key    = qeoSec->pkey;
        *certs  = qeoSec->certificate_chain;


        qeosec_unlock(qeoSec);

        ret = QEO_OK;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_get_realms(qeo_security_identity **sec_id, unsigned int *length /* OUT */)
{
    qeo_retcode_t ret = QEO_OK;

    if (sec_id == NULL || length == NULL){
        return QEO_EINVAL;
    }

    api_lock();
    ret = get_realms_locked(sec_id, length);

    api_unlock();
    return ret;
}

qeo_retcode_t qeo_security_get_identity(qeo_security_hndl qeoSec, qeo_security_identity **id){

    qeo_retcode_t     ret     = QEO_OK;
    if (id == NULL || qeoSec == NULL){
        return QEO_EINVAL;
    }

    api_lock();
    if (_initialized == false) {
        api_unlock();
        return QEO_EBADSTATE;
    }

    qeosec_lock(qeoSec);
    do {

        if (qeoSec->state != QEO_SECURITY_AUTHENTICATED){
            ret = QEO_EBADSTATE;
            break;
        }

        *id = (qeo_security_identity *) malloc(sizeof(qeo_security_identity));
        if (NULL == *id) {
            ret = QEO_ENOMEM;
            break;
        }

        (*id)->realm_id = qeoSec->cfg.id.realm_id; 
        (*id)->device_id = qeoSec->cfg.id.device_id; 
        (*id)->user_id = qeoSec->cfg.id.user_id; 
        (*id)->friendly_name = strdup(qeoSec->cfg.id.friendly_name);
        if(qeo_platform_get_key_value("url",&(*id)->url) != QEO_UTIL_OK){
            qeo_log_e("qeo_platform_get_key_value failed");
            (*id)->url = strdup("");
        }
        ret = QEO_OK;
        
    } while(0);

    qeosec_unlock(qeoSec);
    api_unlock();

    return ret;

}
qeo_retcode_t qeo_security_free_identity(qeo_security_identity **id)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if (id == NULL){
        return QEO_EINVAL;
    }

    if (*id == NULL){
        return QEO_EINVAL;
    }

    do {
        free((*id)->url);
        free((*id)->friendly_name);
        free(*id);
        *id=NULL; 
        ret = QEO_OK;
    }while(0);


    return ret;

}

qeo_retcode_t qeo_security_free_realms(qeo_security_identity **id, unsigned int len){

    qeo_retcode_t ret = QEO_EFAIL;
    int i = 0;

    if (id == NULL){
        return QEO_EINVAL;
    }

    do 
    {
        qeo_security_identity *tmp_id = NULL;

        for (i = 0; i< len; ++i){
            tmp_id = id[i];
            ret =  qeo_security_free_identity(&tmp_id);
            if (ret != QEO_OK) {
                qeo_log_e("qeo_security_free_identity failed");
            }
        }

        id = NULL;
        ret = QEO_OK;

    }while(0);

    return ret;
}

qeo_retcode_t qeo_security_get_user_data(qeo_security_hndl qeoSec,
                                         void              *user_data)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if (qeoSec == NULL) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }

        if (user_data == NULL) {
            ret = QEO_EINVAL;
            break;
        }

        qeosec_lock(qeoSec);
        *(void **)user_data = qeoSec->cfg.user_data;
        qeosec_unlock(qeoSec);

        ret = QEO_OK;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_get_mgmt_client_ctx(qeo_security_hndl qeoSec,
                                               qeo_mgmt_client_ctx_t **ctx)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if (qeoSec == NULL) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }

        if (ctx == NULL) {
            ret = QEO_EINVAL;
            break;
        }

        qeosec_lock(qeoSec);
        *ctx = qeoSec->mgmt_client_ctx;
        qeosec_unlock(qeoSec);

        ret = QEO_OK;
    } while (0);
    api_unlock();

    return ret;
}
