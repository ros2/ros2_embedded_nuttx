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

#ifndef DEBUG
#define NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include <dds/dds_xtypes.h>
#include <dds/dds_dreader.h>
#include <dds/dds_dwriter.h>
#include <dds/dds_seq.h>
#include <dds/dds_security.h>
#include <dds/dds_aux.h>
#include <nsecplug/nsecplug.h>

#include <qeo/log.h>

#include "core.h"
#include "config.h"
#include "forwarder.h"
#include "entity_store.h"
#include "user_data.h"
#include "samplesupport.h"
#include "core_util.h"
#include "security.h"
#include "security_util.h"
#include "policy.h"
#include "deviceinfo_writer.h"

/* Mask to extract entity type from creation flags. */
#define QEOCORE_EFLAG_ETYPE_MASK (QEOCORE_EFLAG_EVENT | QEOCORE_EFLAG_STATE)

/* ===[ domain participants, publishers and subscribers ]==================== */

#define DDS_ENTITY_NAME "Technicolor Qeo"
#define VALIDATE_NOT_INIT(f)    \
    if (f->flags.initialized) { \
        factory_unlock(f);      \
        return QEO_EBADSTATE;   \
    }


const qeocore_domain_id_t QEOCORE_DEFAULT_DOMAIN = 0x80;
const qeocore_domain_id_t QEOCORE_OPEN_DOMAIN = 0x01;

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/
static qeocore_writer_t *_deviceinfo_writer = NULL;
static int                _factory_allocs;
static bool               _init_security_done = false;
static qeo_factory_t     *_open_domain_factory = NULL;
static int                _ownership_strength = 0;

#ifdef __USE_GNU
/* compile with -D_GNU_SOURCE */
#ifndef NDEBUG
static const pthread_mutex_t _def_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
static const pthread_mutex_t _def_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
#else
static const pthread_mutex_t _def_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* dds domainparticipant lock */
static pthread_mutex_t _dds_dp_lock = PTHREAD_MUTEX_INITIALIZER;

static const char* _open_domain_types[] =
    {"org.qeo.system.RegistrationRequest", "org.qeo.system.RegistrationCredentials", "org.qeo.system.DeviceId"};

/* ===[ private function declarations ]==================== */
static void factory_lock(qeo_factory_t *factory);
static void factory_unlock(qeo_factory_t *factory);
#ifndef NDEBUG
static bool is_valid_factory(qeo_factory_t *factory);
#endif
static qeocore_domain_id_t get_domain_id_closed(void);
static qeocore_domain_id_t get_domain_id_open(void);
#define FACTORY_IS_SECURE(f) (f->domain_id != get_domain_id_open())
static void qeo_security_status(qeo_security_hndl   qeo_sec,
                                qeo_security_state  status);
static void on_qeo_security_authenticated(qeo_security_hndl qeo_sec,
                                          qeo_factory_t     *factory);

static qeo_factory_t *create_factory(const qeo_identity_t *id);
static void destroy_factory(qeo_factory_t *factory);
static qeo_retcode_t init_factory(qeo_factory_t                     *factory,
                          const qeocore_factory_listener_t  *listener);
static qeo_retcode_t factory_set_identity(qeo_factory_t               *factory,
                                          const qeo_security_identity *sec_id);
static qeo_retcode_t create_domain_participant(qeo_factory_t *factory);
static void free_identity(qeo_identity_t *id);
static void free_identities(qeo_identity_t  **identities,
                            unsigned int  length);
static void policy_update_cb(qeo_security_policy_hndl qeoSecPol);

static bool is_type_allowed(const qeo_factory_t *factory, const char* name);

/* ===[ private function implementations ]==================== */


static void factory_lock(qeo_factory_t *factory)
{
    lock(&factory->mutex);
}

static void factory_unlock(qeo_factory_t *factory)
{
    unlock(&factory->mutex);
}

#ifndef NDEBUG
static bool is_valid_factory(qeo_factory_t *factory)
{
    if (factory->flags.initialized) {
        if (factory->dp == NULL) {
            return false;
        }
        if (FACTORY_IS_SECURE(factory)) {
            if (factory->qeo_sec == NULL) {
                return false;
            }
            if (factory->qeo_pol == NULL) {
                return false;
            }
        }
        else {
            if (factory->qeo_sec != NULL) {
                return false;
            }
            if (factory->qeo_pol != NULL) {
                return false;
            }
        }
    }
    else {
        if (factory->dp != NULL) {
            return false;
        }
        if (factory->qeo_pol != NULL) {
            return false;
        }
    }

    return true;
}
#endif

static qeocore_domain_id_t get_domain_id_closed(void)
{
    qeocore_domain_id_t id = QEOCORE_DEFAULT_DOMAIN;
#if defined(DEBUG) || defined(qeo_c_core_COVERAGE)
    id = qeocore_parameter_get_number("DDS_DOMAIN_ID_CLOSED");
    if (id == -1) {
        //parameter not set, use default
        id = QEOCORE_DEFAULT_DOMAIN;
    }
    if (id != QEOCORE_DEFAULT_DOMAIN) {
        qeo_log_w("Using non-default domainId for closed domain: %d", id);
    }
#endif

    return id;
}

static qeocore_domain_id_t get_domain_id_open(void)
{
    qeocore_domain_id_t id = QEOCORE_OPEN_DOMAIN;
#if defined(DEBUG) || defined(qeo_c_core_COVERAGE)
    id = qeocore_parameter_get_number("DDS_DOMAIN_ID_OPEN");
    if (id == -1) {
        //parameter not set, use default
        id = QEOCORE_OPEN_DOMAIN;
    }
#endif

    return id;
}

static qeo_factory_t *create_factory(const qeo_identity_t *id)
{
    qeo_retcode_t rc        = QEO_EFAIL;
    qeo_factory_t *factory  = NULL;

    do {
        if (_ownership_strength == 0) {
            struct timeval tv;
            if (gettimeofday(&tv, NULL) != 0) {
                qeo_log_e("Can't get timeofday");
                break;
            }
            _ownership_strength = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
        }
        if (_factory_allocs == 0) {
            if (QEO_OK != entity_store_init()) {
                qeo_log_e("Failed to create entity store");
                break;
            }
            DDS_entity_name(DDS_ENTITY_NAME);
            DDS_set_generate_callback(calculate_member_id);
            DDS_parameter_set("IP_NO_MCAST", "any");
        }

        if (_init_security_done == false && id != QEO_IDENTITY_OPEN) {
            /* only needs to be done once, but doesn't hurt if you do it more */
            if ((rc = qeo_security_init()) != QEO_OK) {
                qeo_log_e("Failed to init security lib: %d", rc);
                break;
            }

            if ((rc = qeo_security_policy_init()) != QEO_OK) {
                qeo_log_e("Could not init policy handling");
                break;
            }

            _init_security_done = true;
        }

        factory = calloc(1, sizeof(qeo_factory_t));
        if (factory == NULL) {
            qeo_log_e("Could not allocate new factory");
            break;
        }
        ++_factory_allocs;

        memcpy(&factory->mutex, &_def_mutex, sizeof(factory->mutex));
        factory->cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

        if (id == QEO_IDENTITY_DEFAULT) {
            factory->domain_id = get_domain_id_closed();
        } else if (id == QEO_IDENTITY_OPEN ) {
            factory->domain_id = get_domain_id_open();
            if (factory->domain_id != QEOCORE_OPEN_DOMAIN) {
                qeo_log_w("Using non-default domainId for open domain: %d", factory->domain_id);
            }
            factory_lock(factory);
            factory->ready_to_destroy = true;
            pthread_cond_signal(&factory->cond);
            factory_unlock(factory);
        }
        /* FUTURE: store provided 'real' identity */

        assert(is_valid_factory(factory));

        rc = QEO_OK;

    } while (0);

    if (rc != QEO_OK){
        if (factory != NULL) {
            --_factory_allocs;
            free(factory);
            factory = NULL;
        }
    }

    if (_factory_allocs == 0) {
        entity_store_fini();
        DDS_set_generate_callback(NULL);
        qeo_security_destroy();
        qeo_security_policy_destroy();
    }
    return factory;
}

static void destroy_factory(qeo_factory_t *factory)
{
    qeo_security_hndl         qeo_sec = NULL;
    qeo_security_policy_hndl  qeo_pol = NULL;

    factory_lock(factory);
    assert(is_valid_factory(factory));
    qeo_sec = factory->qeo_sec;
    qeo_pol = factory->qeo_pol;

    if (qeo_sec != NULL) {
        /* to cover case where only qeocore_factory_new has been called and not qeocore_factory_init */
        while (factory->ready_to_destroy == false){
            pthread_cond_wait(&factory->cond, &factory->mutex);
        }
    }
    factory_unlock(factory);

    /* Remove the device info writer */
    /* Question : why is this removed upon factory close while this is a static variable ?
     * Should be kept on factory level, no ?
     */
    if (NULL != _deviceinfo_writer) {
        qeo_deviceinfo_destruct(_deviceinfo_writer);
        _deviceinfo_writer = NULL;
    }

    factory_lock(factory);

    if (NULL != factory->dp) {
        DDS_DomainParticipant_delete_contained_entities(factory->dp);
        lock(&_dds_dp_lock);
        DDS_DomainParticipantFactory_delete_participant(factory->dp);
        unlock(&_dds_dp_lock);
    }
    free_identity(&(factory->qeo_id));

    factory_unlock(factory);

    if (qeo_pol != NULL) {
        if (qeo_security_policy_destruct(&qeo_pol) != QEO_OK) {
            qeo_log_e("qeo_security_policy_destruct failed");
        }
    }
    if (qeo_sec != NULL) {
        if (qeo_security_destruct(&qeo_sec) != QEO_OK) {
            qeo_log_e("qeo_security_destruct failed");
        }
    }

    free(factory);

    if (--_factory_allocs == 0) {
        qeo_security_policy_destroy();
        qeo_security_destroy();
//        DDS_Security_cleanup_credentials();
        _init_security_done = false;
        entity_store_fini();
    }
}

static qeo_retcode_t init_factory(qeo_factory_t *factory, const qeocore_factory_listener_t *listener)
{
    qeo_retcode_t       rc = QEO_EFAIL;
    qeo_security_config cfg;

    factory->listener = *listener;

    if (FACTORY_IS_SECURE(factory) == false) {
        qeocore_on_factory_init_done  cb          = NULL;
        bool success = false;
        do {
            if ((rc = create_domain_participant(factory)) != QEO_OK){
                qeo_log_e("Could not create domain participant %d", rc);
                break;
            }

            success = true;
            rc = QEO_OK;
        } while (0);

        cb = factory->listener.on_factory_init_done;
        if (NULL != cb) {
            factory_unlock(factory);
            cb(factory, success);
            factory_lock(factory);
        }

    } else {

        cfg.id.realm_id         = factory->qeo_id.realm_id;
        cfg.id.user_id          = factory->qeo_id.user_id;
        cfg.id.device_id        = factory->qeo_id.device_id;
        cfg.id.url              = factory->qeo_id.url;
        cfg.security_status_cb  = qeo_security_status;
        cfg.user_data           = (void *)factory;

        do {
            if ((rc = fwd_init_pre_auth(factory)) != QEO_OK) {
                qeo_log_e("Forwarding factory initialization failed");
                break;
            }
            if ((rc = qeo_security_construct(&cfg, &factory->qeo_sec)) != QEO_OK) {
                qeo_log_e("Failed to construct security handle: %d", rc);
                break;
            }
            if ((rc = qeo_security_authenticate(factory->qeo_sec)) != QEO_OK) {
                qeo_log_e("Failed to authenticate (rc=%d)", rc);
                break;
            }

            rc = QEO_OK;
        } while (0);

        if (rc != QEO_OK) {
            factory->ready_to_destroy = true;
            pthread_cond_signal(&factory->cond);
        }
    }

    return rc;
}

int qeocore_get_num_factories()
{
    return _factory_allocs;
}

static qeo_retcode_t create_domain_participant(qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_EFAIL;

    lock(&_dds_dp_lock);
    factory->dp = DDS_DomainParticipantFactory_create_participant(factory->domain_id, NULL, NULL, 0);
    assert(factory->dp != 0);
    unlock(&_dds_dp_lock);
    qeo_log_dds_null("DDS_DomainParticipantFactory_create_participant", factory->dp);
    if (NULL != factory->dp) {
        factory->flags.initialized = 1;
        rc = QEO_OK;
    }
    return rc;
}

/* sets the qeo_identity_t based on the qeo_security_identity */
static qeo_retcode_t factory_set_identity(qeo_factory_t *factory, const qeo_security_identity *sec_id)
{
    qeo_retcode_t rc = QEO_OK;

    free_identity(&(factory->qeo_id));
    factory->qeo_id.realm_id  = sec_id->realm_id;
    factory->qeo_id.user_id   = sec_id->user_id;
    factory->qeo_id.device_id = sec_id->device_id;
    if ((factory->qeo_id.url = strdup(sec_id->url)) == NULL) {
        qeo_log_e("Out of memory");
        rc = QEO_ENOMEM;
    }

    return rc;
}

/* frees the contents of the qeo_identity_t */
static void free_identity(qeo_identity_t *id)
{
    free(id->url);
}

static void free_identities(qeo_identity_t **identities, unsigned int length)
{
    unsigned int  i;
    qeo_identity_t  *ids = *identities;

    for (i = 0; i < length; ++i) {
        free_identity(ids + i);
    }
    free(ids);
    identities = NULL;
}


/* ===[ call-backs ]==================== */

static void qeo_security_status(qeo_security_hndl qeo_sec, qeo_security_state status)
{
    qeo_factory_t                 *factory  = NULL;
    qeocore_on_factory_init_done  cb        = NULL;

    qeo_security_get_user_data(qeo_sec, (void *)&factory);
    assert(factory != NULL);

    qeo_log_i("QEO security status (%p) for factory (%p): %d", qeo_sec, factory, status);

    switch (status) {
        case QEO_SECURITY_UNAUTHENTICATED:
        case QEO_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS:
        case QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY:
        case QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED:
        case QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE:
        case QEO_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS:
        case QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS:
        case QEO_SECURITY_STORING_QEO_CREDENTIALS:
            break;

        case QEO_SECURITY_AUTHENTICATION_FAILURE:
            qeo_log_e("Authentication failure");
            factory_lock(factory);
            cb = factory->listener.on_factory_init_done;
            factory_unlock(factory);
            cb(factory, false);
            break;

        case QEO_SECURITY_AUTHENTICATED:
            on_qeo_security_authenticated(qeo_sec, factory);
            break;
    }

    if (IS_FINAL_SECURITY_STATE(status)) {
        factory_lock(factory);
        factory->ready_to_destroy = true;
        pthread_cond_signal(&factory->cond);
        factory_unlock(factory);
    }
}

static void on_qeo_security_authenticated(qeo_security_hndl qeo_sec, qeo_factory_t  *factory)
{
    DDS_ReturnCode_t      ddsrc;
    qeo_retcode_t         rc    = QEO_EFAIL;
    qeo_security_identity *id   = NULL;
    EVP_PKEY              *key  = NULL;

    STACK_OF(X509) * certs = NULL;
    DDS_Credentials               creds       = { 0 };
    qeo_security_policy_config    policy_cfg  = { 0 };
    qeo_security_policy_hndl      qeo_pol     = NULL;
    qeocore_on_factory_init_done  cb          = NULL;
    bool                          success     = false;

    factory_lock(factory);
    cb = factory->listener.on_factory_init_done;
    do {

        if (QEO_OK != (rc = qeo_security_get_identity(qeo_sec, &id))) {
            qeo_log_e("Failed to get identity (rc=%d)", rc);
            break;
        }

        if (QEO_OK != (rc = factory_set_identity(factory, id))) {
            qeo_log_e("Failed to set factory's identity (rc=%d)", rc);
            break;
        }

        if (QEO_OK != (rc = qeo_security_get_credentials(qeo_sec, &key, &certs))) {
            qeo_log_e("Failed to get credentials (rc=%d)", rc);
            break;
        }

        policy_cfg.sec        = qeo_sec;
        policy_cfg.factory    = factory;
        policy_cfg.update_cb  = policy_update_cb;

        if (QEO_OK != qeo_security_policy_construct(&policy_cfg, &qeo_pol)) {
            qeo_log_e("qeo_security_policy_construct failed");
            break;
        }
        factory->qeo_pol = qeo_pol;

        /* We only need to pass references, DDS will make their own copy */
        creds.credentialKind                = DDS_SSL_BASED;
        creds.info.sslData.private_key      = key;
        creds.info.sslData.certificate_list = certs;

        ddsrc = DDS_Security_set_credentials(id->friendly_name, &creds);
        qeo_log_dds_rc("DDS_Security_set_credentials", ddsrc);
        if (DDS_RETCODE_OK != ddsrc) {
            qeo_log_e("DDS_Security_set_credentials failed: %d", ddsrc_to_qeorc(ddsrc));
            break;
        }

        /* Finally create the domain participant */
        if (QEO_OK != create_domain_participant(factory)) {
            break;
        }

        success = true;
    } while (0);
    factory_unlock(factory);

    if ((success == true) && (true == FACTORY_IS_SECURE(factory))) {
        /* Enable forwarding logic (client or server)  */
        if (QEO_OK != fwd_init_post_auth(factory)) {
            qeo_log_e("Failed to initialize forwarding logic");
            if (factory->flags.is_forwarder) {
                success = false; /* fatal */
            }
        }
    }

    if (id != NULL) {
        qeo_security_free_identity(&id);
    }

    /* Publish the device info */
    if ((success == true) && (NULL == _deviceinfo_writer)) {
        _deviceinfo_writer = qeo_deviceinfo_publish(factory);
    }

    if (NULL != cb) {
        cb(factory, success);
    }
}

static void policy_update_cb(qeo_security_policy_hndl secpol)
{
    qeo_security_policy_config cfg = { 0 };

    qeo_log_i("policy update");
    if (QEO_OK != qeo_security_policy_get_config(secpol, &cfg)) {
        qeo_log_e("qeo_security_policy_get_config failed");
    }
    else if (QEO_OK != entity_store_update_user_data(cfg.factory)) {
        qeo_log_e("Failed to update userdata");
    }
}

static bool is_type_allowed(const qeo_factory_t *factory, const char* name)
{
    if (FACTORY_IS_SECURE(factory) == true) {
        return true;
    }

    for (unsigned int i = 0; i < sizeof(_open_domain_types) / sizeof(_open_domain_types[0]); i++) {
        if (0 == strcmp(name, _open_domain_types[i])) {
            return true;
        }
    }

    qeo_log_e("Type %s not allowed for this factory", name);

    return false;
}

/* ===[ public API ]==================== */

qeo_factory_t *qeocore_factory_new(const qeo_identity_t *id)
{
    if (id != QEO_IDENTITY_DEFAULT && id != QEO_IDENTITY_OPEN) {
        return NULL;
    }

    return create_factory(id);
}

void qeocore_factory_close(qeo_factory_t *factory)
{
    if (NULL == factory) {
        return;
    }

    fwd_destroy(factory);
    destroy_factory(factory);

    if (_factory_allocs == 1 && NULL != _open_domain_factory) {
        destroy_factory(_open_domain_factory);
        _open_domain_factory = NULL;
    }
}

qeo_retcode_t qeocore_factory_init(qeo_factory_t *factory, const qeocore_factory_listener_t *listener)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_NON_NULL(factory);
    VALIDATE_NON_NULL(listener);
    VALIDATE_NON_NULL(listener->on_factory_init_done);

    factory_lock(factory);
    VALIDATE_NOT_INIT(factory);
    rc = init_factory(factory, listener);
    assert(is_valid_factory(factory));
    factory_unlock(factory);

    return rc;
}

qeo_retcode_t qeocore_factory_refresh_policy(const qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_NON_NULL(factory);
    factory_lock((qeo_factory_t *)factory);
    if (!factory->flags.initialized) {
        rc = QEO_EBADSTATE;
    }
    else {
        rc = qeo_security_policy_refresh(factory->qeo_pol);
    }
    factory_unlock((qeo_factory_t *)factory);
    return rc;
}

qeo_retcode_t qeocore_factory_set_domainid(qeo_factory_t        *factory,
                                           qeocore_domain_id_t  id)
{
    VALIDATE_NON_NULL(factory);

    factory_lock(factory);
    VALIDATE_NOT_INIT(factory);
    factory->domain_id = id;
    factory_unlock(factory);

    return QEO_OK;
}

qeo_retcode_t qeocore_factory_get_domainid(qeo_factory_t        *factory,
                                           qeocore_domain_id_t  *id)
{
    VALIDATE_NON_NULL(factory);
    VALIDATE_NON_NULL(id);

    factory_lock(factory);
    *id = factory->domain_id;
    factory_unlock(factory);

    return QEO_OK;
}

int64_t qeocore_factory_get_realm_id(qeo_factory_t *factory)
{
    int64_t id = -1;

    if (NULL != factory) {
        factory_lock(factory);
        id = factory->qeo_id.realm_id;
        factory_unlock(factory);
    }
    return id;
}

int64_t qeocore_factory_get_user_id(qeo_factory_t *factory)
{
    int64_t id = -1;

    if (NULL != factory) {
        factory_lock(factory);
        id = factory->qeo_id.user_id;
        factory_unlock(factory);
    }
    return id;
}

const char * qeocore_factory_get_realm_url(qeo_factory_t *factory)
{
    const char * url = NULL;

    if (NULL != factory) {
        factory_lock(factory);
        url = factory->qeo_id.url;
        factory_unlock(factory);
    }
    return url;
}

qeo_retcode_t qeocore_factory_set_intf(qeo_factory_t  *factory,
                                       const char     *interfaces)
{
    DDS_ReturnCode_t ddsrc;

    VALIDATE_NON_NULL(factory);
    VALIDATE_NON_NULL(interfaces);

    factory_lock(factory);
    VALIDATE_NOT_INIT(factory);
    factory_unlock(factory);
    ddsrc = DDS_parameter_set("IP_INTF", interfaces);
    qeo_log_dds_rc("DDS_parameter_set", ddsrc);

    return ddsrc_to_qeorc(ddsrc);
}

qeo_retcode_t qeocore_factory_set_tcp_server(qeo_factory_t  *factory,
                                             const char     *tcp_server)
{
    qeo_retcode_t rc;

    VALIDATE_NON_NULL(factory);
    VALIDATE_NON_NULL(tcp_server);

    factory_lock(factory);
    rc = core_factory_set_tcp_server_no_lock(factory, tcp_server);
    factory_unlock(factory);

    return rc;
}

qeo_retcode_t qeocore_factory_set_local_tcp_port(qeo_factory_t *factory,
                                                 const char *local_port)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;

    VALIDATE_NON_NULL(local_port);
    if (NULL == getenv("TDDS_TCP_PORT")) {
        /* set DDS TCP_SERVER to value from SMS if not overridden */
        qeo_log_i("Set TCP port to %s", local_port);
        ddsrc = DDS_parameter_set("TCP_PORT", local_port);
        qeo_log_dds_rc("DDS_parameter_set TCP_PORT", ddsrc);
    }
    return ddsrc_to_qeorc(ddsrc);
}

qeo_retcode_t core_factory_set_tcp_server_no_lock(qeo_factory_t  *factory,
                                                  const char     *tcp_server)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;

    VALIDATE_NON_NULL(tcp_server);
    if (NULL == getenv("TDDS_TCP_SERVER")) {
        /* set DDS TCP_SERVER to value from SMS if not overridden */
        qeo_log_i("Set TCP server to %s", tcp_server);
        ddsrc = DDS_parameter_set("TCP_SERVER", tcp_server);
        qeo_log_dds_rc("DDS_parameter_set TCP_SERVER", ddsrc);
    }

    return ddsrc_to_qeorc(ddsrc);
}

qeo_retcode_t qeocore_factory_set_user_data(qeo_factory_t *factory,
                                            uintptr_t     userdata)
{
    VALIDATE_NON_NULL(factory);

    factory_lock(factory);
    factory->userdata = userdata;
    factory_unlock(factory);
    return QEO_OK;
}

qeo_retcode_t qeocore_factory_get_user_data(qeo_factory_t *factory,
                                            uintptr_t     *userdata)
{
    VALIDATE_NON_NULL(factory);

    factory_lock(factory);
    *userdata = factory->userdata;
    factory_unlock(factory);

    return QEO_OK;
}

qeo_retcode_t qeocore_get_identities(qeo_identity_t **identities, unsigned int *length)
{
    qeo_security_identity *sec_ids  = NULL;
    qeo_retcode_t         ret       = QEO_OK;
    unsigned int          i         = 0;

    VALIDATE_NON_NULL(identities);
    VALIDATE_NON_NULL(length);

    do {
        if ((ret = qeo_security_get_realms(&sec_ids, length)) != QEO_OK) {
            qeo_log_e("Could not get realms");
            break;
        }

        if (length == 0) {
            qeo_log_i("No existing identities found");
            break;
        }

        *identities = (qeo_identity_t *)malloc(sizeof(**identities) * (*length));
        if (NULL == *identities) {
            qeo_log_e("Out of memory");
            ret = QEO_ENOMEM;
            break;
        }
        for (i = 0; i < *length; i++) {
            qeo_identity_t *id = *identities + i;
            id->realm_id  = sec_ids[i].realm_id;
            id->user_id   = sec_ids[i].user_id;
            id->device_id = sec_ids[i].device_id;
            if ((id->url = strdup(sec_ids[i].url)) == NULL) {
                qeo_log_e("Out of memory");
                ret = QEO_ENOMEM;
                free_identities(identities, i + 1);
                break;
            }
        }

        qeo_log_i("Found %d identities", *length);

        qeo_security_free_realms(&sec_ids, *length);
    } while (0);

    return ret;
}

qeo_retcode_t qeocore_free_identities(qeo_identity_t **identities, unsigned int length)
{
    VALIDATE_NON_NULL(identities);
    free_identities(identities, length);

    return QEO_OK;
}

/* ===[ readers and writers ]================================================ */

static qeo_retcode_t init_topic(const qeo_factory_t *factory,
                                entity_t            *entity,
                                const char          *type_name,
                                const char          *topic_name)
{
    qeo_retcode_t rc = QEO_OK;

    entity->topic = DDS_DomainParticipant_create_topic(factory->dp, topic_name, type_name, NULL, NULL, 0);
    if (NULL == entity->topic) {
        qeo_log_e("DDS_DomainParticipant_create_topic failed for:%s", topic_name);
        rc = QEO_EFAIL;
    }
    else {
        rc = qeocore_type_use(entity->type_info); /* incr refcnt */
    }
    return rc;
}

static void release_topic(const qeo_factory_t *factory,
                          entity_t            *entity)
{
    if (NULL != entity->topic) {
        DDS_DomainParticipant_delete_topic(factory->dp, entity->topic);
        qeocore_type_free((qeocore_type_t *)entity->type_info); /* decr refcnt */
        entity->topic = NULL;
    }
}

static void dwqos_from_type(DDS_Publisher     pub,
                            DDS_DataWriterQos *qos,
                            bool is_state)
{
    DDS_Publisher_get_default_datawriter_qos(pub, qos);
    if (!is_state) {
        qos->history.kind     = DDS_KEEP_ALL_HISTORY_QOS;
        qos->history.depth    = DDS_LENGTH_UNLIMITED;
        qos->reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        qos->durability.kind  = DDS_VOLATILE_DURABILITY_QOS;
    }
    else {
        qos->history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
        qos->history.depth    = 1;
        qos->reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        qos->durability.kind  = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
        qos->ownership.kind   = DDS_EXCLUSIVE_OWNERSHIP_QOS;
        qos->ownership_strength.value = _ownership_strength;
    }
}

static void drqos_from_type(DDS_Subscriber    sub,
                            DDS_DataReaderQos *qos,
                            bool is_state)
{
    DDS_Subscriber_get_default_datareader_qos(sub, qos);
    if (!is_state) {
        qos->history.kind     = DDS_KEEP_ALL_HISTORY_QOS;
        qos->history.depth    = DDS_LENGTH_UNLIMITED;
        qos->reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        qos->durability.kind  = DDS_VOLATILE_DURABILITY_QOS;
    }
    else {
        qos->history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
        qos->history.depth    = 1;
        qos->reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        qos->durability.kind  = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
        qos->ownership.kind   = DDS_EXCLUSIVE_OWNERSHIP_QOS;
    }
}

// TODO clean up double code : read_or_take_tsm vs. read_or_take_dynamic
static void read_or_take_tsm(const qeocore_reader_t *reader,
                             const qeocore_filter_t *filter,
                             qeocore_data_t         *data,
                             tsm_based_data_t       *tsmdata,
                             int                    take)
{
    DDS_InstanceHandle_t  ih      = (NULL == filter ? DDS_HANDLE_NIL : filter->instance_handle);
    DDS_SampleInfo        *pinfo  = NULL;

    while (1) {
        if (take) {
            data->ddsrc = DDS_DataReader_take_next_instance(reader->dr, &tsmdata->seq_data, &tsmdata->seq_info,
                                                            1, ih, DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                                            DDS_ANY_INSTANCE_STATE);
        }
        else {
            data->ddsrc = DDS_DataReader_read_next_instance(reader->dr, &tsmdata->seq_data, &tsmdata->seq_info,
                                                            1, ih, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                                            DDS_ANY_INSTANCE_STATE);
        }
        if (DDS_RETCODE_OK != data->ddsrc) {
            if (DDS_RETCODE_NO_DATA != data->ddsrc) {
                qeo_log_dds_rc(take ? "DDS_DataReader_take_next_instance" :
                               "DDS_DataReader_read_next_instance", data->ddsrc);
            }
            break; /* errors and no more data */
        }

        pinfo = DDS_SEQ_ITEM(tsmdata->seq_info, 0);
        // TODO check valid_data before states and then check states
        if (DDS_ALIVE_INSTANCE_STATE != pinfo->instance_state) {
            if (reader->flags.ignore_dispose) {
                ih = pinfo->instance_handle;
                DDS_DataReader_return_loan(reader->dr, &tsmdata->seq_data, &tsmdata->seq_info);
                /* take sample to clean up cache */
                DDS_DataReader_take_instance(reader->dr, &tsmdata->seq_data, &tsmdata->seq_info, DDS_LENGTH_UNLIMITED,
                                             ih, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                             DDS_ANY_INSTANCE_STATE);
                DDS_DataReader_return_loan(reader->dr, &tsmdata->seq_data, &tsmdata->seq_info);
                continue; /* with next sample */
            }
            else {
                /* instance disposal */
                data->flags.is_single = 1;
                tsmdata->single_data  = calloc(1, reader->entity.type_info->u.tsm_based.tsm->size);
                if (NULL != tsmdata->single_data) {
                    data->ddsrc = DDS_DataReader_get_key_value(reader->dr, tsmdata->single_data, pinfo->instance_handle);
                    qeo_log_dds_rc("DDS_DataReader_get_key_value", data->ddsrc);
                }
            }
        }
        else if (1 == pinfo->valid_data) {
            data->flags.is_single = 0;
        }
        data->flags.needs_return_loan = 1;
        break; /* valid data */
    }
}

static void read_or_take_dynamic(const qeocore_reader_t *reader,
                                 const qeocore_filter_t *filter,
                                 qeocore_data_t         *data,
                                 dynamic_data_t         *dyndata,
                                 int                    take)
{
    DDS_InstanceHandle_t  ih      = (NULL == filter ? DDS_HANDLE_NIL : filter->instance_handle);
    DDS_SampleInfo        *pinfo  = NULL;

    while (1) {
        if (take) {
            data->ddsrc = DDS_DynamicDataReader_take_next_instance(reader->dr, &dyndata->seq_data, &dyndata->seq_info,
                                                                   1, ih, DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                                                   DDS_ANY_INSTANCE_STATE);
        }
        else {
            data->ddsrc = DDS_DynamicDataReader_read_next_instance(reader->dr, &dyndata->seq_data, &dyndata->seq_info,
                                                                   1, ih, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                                                   DDS_ANY_INSTANCE_STATE);
        }
        if (DDS_RETCODE_OK != data->ddsrc) {
            if (DDS_RETCODE_NO_DATA != data->ddsrc) {
                qeo_log_dds_rc(take ? "DDS_DynamicDataReader_take_next_instance" :
                               "DDS_DynamicDataReader_read_next_instance", data->ddsrc);
            }
            break; /* errors and no more data */
        }

        pinfo = DDS_SEQ_ITEM(dyndata->seq_info, 0);
        // TODO check valid_data before states and then check states
        if (DDS_ALIVE_INSTANCE_STATE != pinfo->instance_state) {
            if (reader->flags.ignore_dispose) {
                ih = pinfo->instance_handle;
                DDS_DynamicDataReader_return_loan(reader->dr, &dyndata->seq_data, &dyndata->seq_info);
                /* take sample to clean up cache */
                DDS_DynamicDataReader_take_instance(reader->dr, &dyndata->seq_data, &dyndata->seq_info, DDS_LENGTH_UNLIMITED,
                                                    ih, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                                    DDS_ANY_INSTANCE_STATE);
                DDS_DynamicDataReader_return_loan(reader->dr, &dyndata->seq_data, &dyndata->seq_info);
                continue; /* with next sample */
            }
            else {
                /* instance disposal */
                data->flags.is_single = 1;
                dyndata->single_data  = DDS_DynamicDataFactory_create_data(reader->entity.type_info->u.dynamic.dtype);
                qeo_log_dds_null("DDS_DynamicDataFactory_create_data", dyndata->single_data);
                if (NULL != dyndata->single_data) {
                    data->ddsrc = DDS_DynamicDataReader_get_key_value(reader->dr, dyndata->single_data, pinfo->instance_handle);
                    qeo_log_dds_rc("DDS_DynamicDataReader_get_key_value", data->ddsrc);
                }
            }
        }
        else if (1 == pinfo->valid_data) {
            data->flags.is_single = 0;
        }
        data->flags.needs_return_loan = 1;
        break; /* valid data */
    }
}

static void read_or_take(const qeocore_reader_t *reader,
                         const qeocore_filter_t *filter,
                         qeocore_data_t         *data,
                         int                    take)
{
    if (reader->entity.type_info->flags.tsm_based) {
        read_or_take_tsm(reader, filter, data, &data->d.tsm_based, take);
    }
    else {
        read_or_take_dynamic(reader, filter, data, &data->d.dynamic, take);
    }
}

/**
 * \pre \a reader and \a data are non-\c NULL
 */
qeo_retcode_t core_read_or_take(const qeocore_reader_t  *reader,
                                const qeocore_filter_t  *filter,
                                qeocore_data_t          *data,
                                int                     take)
{
    read_or_take(reader, filter, data, take);
    return ddsrc_to_qeorc(data->ddsrc);
}

static void on_data_available(DDS_DataReaderListener  *self,
                              DDS_DataReader          dr)
{
    qeocore_reader_t  *reader = (qeocore_reader_t *)self->cookie;
    qeocore_data_t    data    = { { 0 } };

    data.rw.reader = reader;
    data.flags.is_tsm_based = reader->entity.type_info->flags.tsm_based;
    if (reader->flags.notify_only) {
        /* no looping if we only need to notify */
        qeocore_data_reset(&data);
        data.flags.notify_only = 1;
        reader->listener.on_data(reader, &data, reader->listener.userdata);
    }
    else {
        while (!reader->flags.close_requested && (DDS_RETCODE_NO_DATA != data.ddsrc)) {
            qeocore_data_reset(&data);
            read_or_take(reader, NULL, &data, 1);
            reader->listener.on_data(reader, &data, reader->listener.userdata);
        }
    }
    core_data_clean(&data);
}

/* ===[ entity construction and destruction ]================================ */

qeo_retcode_t core_register_type(const qeo_factory_t    *factory,
                                 DDS_DynamicTypeSupport dts,
                                 DDS_TypeSupport        ts,
                                 const char             *name)
{
    qeo_retcode_t rc = QEO_OK;
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_BAD_PARAMETER;

    factory_lock((qeo_factory_t*)factory);
    do {
        if (false == is_type_allowed(factory, name)) {
            rc = QEO_EINVAL;
            break;
        }
        if (NULL != dts) {
            ddsrc = DDS_DynamicTypeSupport_register_type(dts, factory->dp, (const DDS_ObjectName)name);
            qeo_log_dds_rc("DDS_DynamicTypeSupport_register_type", ddsrc);
        }
        else if (NULL != ts) {
            ddsrc = DDS_DomainParticipant_register_type(factory->dp, ts, (const DDS_ObjectName)name);
            qeo_log_dds_rc("DDS_DomainParticipant_register_type", ddsrc);
        }
        rc = ddsrc_to_qeorc(ddsrc);
    } while(0);
    factory_unlock((qeo_factory_t*)factory);

    return rc;
}

qeo_retcode_t core_unregister_type(const qeo_factory_t    *factory,
                                   DDS_DynamicTypeSupport dts,
                                   DDS_TypeSupport        ts,
                                   const char             *name)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_BAD_PARAMETER;

    // TODO: see note above
    //factory_lock((qeo_factory_t*)factory);
    if (NULL != dts) {
        ddsrc = DDS_DynamicTypeSupport_unregister_type(dts, factory->dp, (const DDS_ObjectName)name);
        qeo_log_dds_rc("DDS_DynamicTypeSupport_unregister_type", ddsrc);
    }
    else if (NULL != ts) {
        ddsrc = DDS_DomainParticipant_unregister_type(factory->dp, ts, (const DDS_ObjectName)name);
        qeo_log_dds_rc("DDS_DomainParticipant_unregister_type", ddsrc);
    }
    //factory_unlock((qeo_factory_t*)factory);
    return ddsrc_to_qeorc(ddsrc);
}

static DDS_Publisher create_publisher(const qeo_factory_t *factory)
{
    DDS_Publisher     pub = NULL;
    DDS_PublisherQos  qos;

    if (FACTORY_IS_SECURE(factory)) {
        DDS_DomainParticipant_get_default_publisher_qos(factory->dp, &qos);
        qos.entity_factory.autoenable_created_entities = 0;
        pub = DDS_DomainParticipant_create_publisher(factory->dp, &qos, NULL, 0);
        qeo_log_dds_null("DDS_DomainParticipant_create_publisher", pub);
    } else {
        pub = DDS_DomainParticipant_create_publisher(factory->dp, NULL, NULL, 0);
    }

    return pub;
}

static qeo_retcode_t core_create_dw(const qeo_factory_t  *factory,
                                    qeocore_writer_t     *writer,
                                    const char           *type_name,
                                    const char           *topic_name,
                                    int flags)
{
    qeo_retcode_t rc = QEO_EFAIL;

    writer->pub = create_publisher(factory);
    if (NULL != writer->pub) {
        rc = init_topic(factory, &writer->entity, type_name, topic_name);
        if (QEO_OK == rc) {
            DDS_DataWriterQos qos = { { 0 } };

            dwqos_from_type(writer->pub, &qos, writer->entity.flags.is_state ? true : false);
            writer->dw = DDS_Publisher_create_datawriter(writer->pub, writer->entity.topic, &qos, NULL, 0);
            qeo_log_dds_null("DDS_Publisher_create_datawriter", writer->dw);
            if (NULL == writer->dw) {
                rc = QEO_EFAIL;
            }
            else if (flags & QEOCORE_EFLAG_ENABLE) {
                rc = core_enable_writer(writer);
            }
        }
    }
    return rc;
}

qeocore_writer_t *core_create_writer(const qeo_factory_t   *factory,
                                     qeocore_type_t  *type,
                                     const char            *topic_name,
                                     int flags,
                                     const qeocore_writer_listener_t *listener,
                                     qeo_retcode_t         *prc)
{
    qeocore_writer_t  *writer = NULL;
    qeo_retcode_t     rc      = QEO_EINVAL;

    writer = (qeocore_writer_t *)calloc(1, sizeof(qeocore_writer_t));
    if (NULL == writer) {
        rc = QEO_ENOMEM;
    }
    else {
        const char *type_name;

        writer->entity.factory          = factory;
        writer->entity.flags.is_writer  = 1;
        writer->entity.flags.is_state   = (flags & QEOCORE_EFLAG_STATE ? 1 : 0);
        writer->entity.type_info        = type;
        if (NULL != listener) {
            writer->listener = *listener;
        }
        type_name = type->flags.tsm_based ? type->u.tsm_based.name : type->u.dynamic.name;
        if (NULL == topic_name) {
            topic_name = type_name;
        }
        rc = core_create_dw(factory, writer, type_name, topic_name, flags);
        if (QEO_OK != rc) {
            core_delete_writer(writer, false);
            writer = NULL;
        }
    }
    if (NULL != prc) {
        *prc = rc;
    }
    return writer;
}

qeo_retcode_t core_enable_writer(qeocore_writer_t *writer)
{
    qeo_retcode_t rc = QEO_OK;

    if (FACTORY_IS_SECURE(writer->entity.factory)) {
        rc = writer_user_data_update(writer);
    }
    if (QEO_OK == rc) {
        DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;

        ddsrc = DDS_DataWriter_enable(writer->dw);
        qeo_log_dds_rc("DDS_DataWriter_enable", ddsrc);
        rc = ddsrc_to_qeorc(ddsrc);
    }
    if (QEO_OK == rc) {
        writer->entity.flags.enabled = 1;
    }
    return rc;
}

void core_delete_writer(qeocore_writer_t  *writer,
                        bool              lock)
{
    if (NULL != writer->pub) {
        if (NULL != writer->dw) {
            DDS_Publisher_delete_datawriter(writer->pub, writer->dw);
        }
        release_topic(writer->entity.factory, &writer->entity);
        DDS_Publisher_delete_contained_entities(writer->pub);
        if (lock) {
            factory_lock((qeo_factory_t *)writer->entity.factory);
        }
        DDS_DomainParticipant_delete_publisher(writer->entity.factory->dp, writer->pub);
        if (lock) {
            factory_unlock((qeo_factory_t *)writer->entity.factory);
        }
    }
    free(writer);
}

static DDS_Subscriber create_subscriber(const qeo_factory_t *factory)
{
    DDS_Subscriber    sub = NULL;
    DDS_SubscriberQos qos;

    if (FACTORY_IS_SECURE(factory)) {
        DDS_DomainParticipant_get_default_subscriber_qos(factory->dp, &qos);
        qos.entity_factory.autoenable_created_entities = 0;
        sub = DDS_DomainParticipant_create_subscriber(factory->dp, &qos, NULL, 0);
        qeo_log_dds_null("DDS_DomainParticipant_create_subscriber", sub);
    }
    else {
        sub = DDS_DomainParticipant_create_subscriber(factory->dp, NULL, NULL, 0);
    }
    return sub;
}

static qeo_retcode_t core_create_dr(const qeo_factory_t *factory,
                                    qeocore_reader_t    *reader,
                                    const char          *type_name,
                                    const char          *topic_name,
                                    int flags)
{
    qeo_retcode_t rc = QEO_EFAIL;

    reader->sub = create_subscriber(factory);
    if (NULL != reader->sub) {
        rc = init_topic(factory, &reader->entity, type_name, topic_name);
        if (QEO_OK == rc) {
            DDS_DataReaderQos qos = { { 0 } };

            drqos_from_type(reader->sub, &qos, reader->entity.flags.is_state ? true : false);
            reader->dr = DDS_Subscriber_create_datareader(reader->sub,
                                                          DDS_Topic_get_topicdescription(reader->entity.topic),
                                                          &qos, NULL, 0);
            qeo_log_dds_null("DDS_Topic_get_topicdescription", reader->dr);
            if (NULL == reader->dr) {
                rc = QEO_EFAIL;
            }
            else {
                DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;

                rc = QEO_OK;
                if (NULL != reader->listener.on_data) {
                    DDS_DataReaderListener *l = DDS_DataReader_get_listener(reader->dr);

                    ddsrc = DDS_RETCODE_BAD_PARAMETER;
                    qeo_log_dds_null("DDS_DataReader_get_listener", l);
                    if (NULL != l) {
                        DDS_DataReaderListener drl  = *l;
                        drl.on_data_available       = on_data_available;
                        drl.cookie                  = (void *)reader;
                        ddsrc = DDS_DataReader_set_listener(reader->dr, &drl, DDS_DATA_AVAILABLE_STATUS);
                        qeo_log_dds_rc("DDS_DataReader_set_listener", ddsrc);
                    }
                    rc = ddsrc_to_qeorc(ddsrc);
                }
                if ((QEO_OK == rc) && (flags & QEOCORE_EFLAG_ENABLE)) {
                    rc = core_enable_reader(reader);
                }
            }
        }
    }
    return rc;
}

qeocore_reader_t *core_create_reader(const qeo_factory_t         *factory,
                                     qeocore_type_t        *type,
                                     const char                  *topic_name,
                                     int flags,
                                     const qeocore_reader_listener_t *listener,
                                     qeo_retcode_t               *prc)
{
    qeocore_reader_t  *reader = NULL;
    qeo_retcode_t     rc      = QEO_EINVAL;

    reader = (qeocore_reader_t *)calloc(1, sizeof(qeocore_reader_t));
    if (NULL == reader) {
        rc = QEO_ENOMEM;
    }
    else {
        const char  *type_name;

        reader->entity.factory          = factory;
        reader->entity.flags.is_writer  = 0;
        reader->entity.flags.is_state   = (flags & QEOCORE_EFLAG_STATE ? 1 : 0);
        reader->entity.type_info        = type;
        if (NULL != listener) {
            reader->listener = *listener;
        }
        if ((flags & QEOCORE_EFLAG_ETYPE_MASK) != QEOCORE_EFLAG_STATE_DATA) {
            reader->flags.ignore_dispose = 1;
        }
        if ((flags & QEOCORE_EFLAG_ETYPE_MASK) == QEOCORE_EFLAG_STATE_UPDATE) {
            reader->flags.notify_only = 1;
        }
        type_name = type->flags.tsm_based ? type->u.tsm_based.name : type->u.dynamic.name;
        if (NULL == topic_name) {
            topic_name = type_name;
        }
        rc = core_create_dr(factory, reader, type_name, topic_name, flags);
        if (QEO_OK != rc) {
            core_delete_reader(reader, false);
            reader = NULL;
        }
    }
    if (NULL != prc) {
        *prc = rc;
    }
    return reader;
}

qeo_retcode_t core_enable_reader(qeocore_reader_t *reader)
{
    qeo_retcode_t rc = QEO_OK;

    if (FACTORY_IS_SECURE(reader->entity.factory)) {
        rc = reader_user_data_update(reader);
    }
    if (QEO_OK == rc) {
        DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;

        ddsrc = DDS_DataReader_enable(reader->dr);
        qeo_log_dds_rc("DDS_DataReader_enable", ddsrc);
        rc = ddsrc_to_qeorc(ddsrc);
    }
    if (QEO_OK == rc) {
        reader->entity.flags.enabled = 1;
    }
    return rc;
}

static void core_delete_dr(qeocore_reader_t *reader)
{
    struct timespec   ts = { 0, 10 * 1000 * 1000 /* 10ms*/ };
    DDS_ReturnCode_t  ddsrc;

    /* make sure we break out of the data available loop */
    reader->flags.close_requested = 1;
    /* now disable the listener for this reader (if any) because an ongoing callback
     * will trigger a DDS_RETCODE_PRECONDITION_NOT_MET return code */
    DDS_DataReader_set_listener(reader->dr, NULL, 0);
    /* now try deleting the reader */
    while (1) {
        ddsrc = DDS_Subscriber_delete_datareader(reader->sub, reader->dr);
        if (DDS_RETCODE_PRECONDITION_NOT_MET != ddsrc) {
            reader->dr = NULL;
            break;
        }
        qeo_log_dds_rc("DDS_Subscriber_delete_datareader", ddsrc);
        nanosleep(&ts, NULL);
    }
}

/**
 * \param[in] reader the reader to be closed, non-\c NULL
 */
void core_delete_reader(qeocore_reader_t  *reader,
                        bool              lock)
{
    if (NULL != reader->sub) {
        if (NULL != reader->dr) {
            core_delete_dr(reader);
            release_topic(reader->entity.factory, &reader->entity);
            DDS_Subscriber_delete_contained_entities(reader->sub);
            if (lock) {
                factory_lock((qeo_factory_t *)reader->entity.factory);
            }
            DDS_DomainParticipant_delete_subscriber(reader->entity.factory->dp, reader->sub);
            if (lock) {
                factory_unlock((qeo_factory_t *)reader->entity.factory);
            }
        }
    }
    free(reader);
}

/* ===[ miscellaneous public functions ]===================================== */

static void data_clean_dynamic(qeocore_data_t *data,
                               dynamic_data_t *dyndata)
{
    if (data->flags.needs_return_loan) {
        DDS_DynamicDataReader_return_loan(data->rw.reader->dr, &dyndata->seq_data, &dyndata->seq_info);
    }
    if (data->flags.is_single) {
        DDS_DynamicDataFactory_delete_data(dyndata->single_data);
    }
    if (NULL != dyndata->single_type) {
        DDS_DynamicTypeBuilderFactory_delete_type(dyndata->single_type);
    }
}

static void data_clean_tsm(qeocore_data_t   *data,
                           tsm_based_data_t *tsmdata)
{
    if (data->flags.needs_return_loan) {
        DDS_DataReader_return_loan(data->rw.reader->dr, &tsmdata->seq_data, &tsmdata->seq_info);
    }
    if (data->flags.is_single) {
        DDS_SampleFree(tsmdata->single_data, data->rw.reader->entity.type_info->u.tsm_based.ts, 1, free);
    }
}

void core_data_clean(qeocore_data_t *data)
{
    /* release */
    if (data->flags.is_tsm_based) {
        data_clean_tsm(data, &data->d.tsm_based);
    }
    else {
        data_clean_dynamic(data, &data->d.dynamic);
    }
    /* reset */
    data->flags.is_single         = 0;
    data->flags.needs_return_loan = 0;
    data->ddsrc = DDS_RETCODE_OK;
    memset(&data->d, 0, sizeof(data->d));
}

qeo_retcode_t core_data_init(qeocore_data_t         *data,
                             const qeocore_reader_t *reader,
                             const qeocore_writer_t *writer)
{
    qeo_retcode_t         rc = QEO_OK;
    const qeocore_type_t  *type_info;

    if (NULL != writer) {
        data->flags.is_writer = 1;
        data->rw.writer       = writer;
        data->flags.is_single = 1;
        type_info             = writer->entity.type_info;
    }
    else if (NULL != reader) {
        data->flags.is_writer = 0;
        data->rw.reader       = reader;
        data->flags.is_single = 0;
        type_info             = reader->entity.type_info;
    }
    else {
        rc = QEO_EINVAL;
    }
    if (QEO_OK == rc) {
        data->flags.is_tsm_based = type_info->flags.tsm_based;
        if (data->flags.is_tsm_based) {
            tsm_based_data_t *tsmdata = &data->d.tsm_based;

            if (data->flags.is_single) {
                tsmdata->single_data = calloc(1, type_info->u.tsm_based.tsm->size);
                qeo_log_dds_null("calloc", tsmdata->single_data);
                if (NULL == tsmdata->single_data) {
                    rc = QEO_ENOMEM;
                }
            }
            else {
                DDS_SEQ_INIT(tsmdata->seq_data);
                DDS_SEQ_INIT(tsmdata->seq_info);
            }
        }
        else {
            dynamic_data_t *dyndata = &data->d.dynamic;

            if (data->flags.is_single) {
                dyndata->single_data = DDS_DynamicDataFactory_create_data(dtype_from_data(data, dyndata));
                qeo_log_dds_null("DDS_DynamicDataFactory_create_data", dyndata->single_data);
                if (NULL == dyndata->single_data) {
                    rc = QEO_ENOMEM;
                }
            }
            else {
                DDS_SEQ_INIT(dyndata->seq_data);
                DDS_SEQ_INIT(dyndata->seq_info);
            }
        }
    }
    return rc;
}

qeocore_data_t *core_data_alloc(const qeocore_reader_t  *reader,
                                const qeocore_writer_t  *writer)
{
    qeocore_data_t *data = NULL;

    data = calloc(1, sizeof(qeocore_data_t));
    if (NULL != data) {
        if (QEO_OK != core_data_init(data, reader, writer)) {
            free(data);
            data = NULL;
        }
    }
    return data;
}

qeo_retcode_t core_write(const qeocore_writer_t *writer,
                         const qeocore_data_t   *data,
                         const void             *sample)
{
    DDS_ReturnCode_t  ddsrc = DDS_RETCODE_OK;
    qeo_retcode_t     rc    = QEO_OK;

    if (NULL == data) {
        ddsrc = DDS_DataWriter_write(writer->dw, sample, DDS_HANDLE_NIL);
        qeo_log_dds_rc("DDS_DataWriter_write", ddsrc);
    }
    else {
        ddsrc = DDS_DynamicDataWriter_write(writer->dw, data->d.dynamic.single_data, DDS_HANDLE_NIL);
        qeo_log_dds_rc("DDS_DynamicDataWriter_write", ddsrc);
    }
    return QEO_OK == rc ? ddsrc_to_qeorc(ddsrc) : rc;
}

qeo_retcode_t core_remove(const qeocore_writer_t  *writer,
                          const qeocore_data_t    *data,
                          const void              *sample)
{
    DDS_ReturnCode_t  ddsrc = DDS_RETCODE_OK;
    qeo_retcode_t     rc    = QEO_OK;

    if (NULL == data) {
        ddsrc = DDS_DataWriter_unregister_instance(writer->dw, sample, DDS_HANDLE_NIL);
        qeo_log_dds_rc("DDS_DataWriter_unregister_instance", ddsrc);
    }
    else {
        ddsrc = DDS_DynamicDataWriter_unregister_instance(writer->dw, data->d.dynamic.single_data, DDS_HANDLE_NIL);
        qeo_log_dds_rc("DDS_DynamicDataWriter_unregister_instance", ddsrc);
    }
    return QEO_OK == rc ? ddsrc_to_qeorc(ddsrc) : rc;
}

qeo_factory_t *core_get_open_domain_factory(void)
{
    qeocore_factory_listener_t listener = { NULL };

    do {
        if (_open_domain_factory == NULL) {
            _open_domain_factory = create_factory(QEO_IDENTITY_OPEN);
            if (_open_domain_factory == NULL) {
                qeo_log_e("Failed to create internal open domain factory");
                break;
            }
        }

        factory_lock(_open_domain_factory);
        init_factory(_open_domain_factory, &listener);
        assert(is_valid_factory(_open_domain_factory));
        factory_unlock(_open_domain_factory);
    } while(0);

    return _open_domain_factory;
}
