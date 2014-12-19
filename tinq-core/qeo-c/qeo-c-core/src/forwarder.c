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

#include <string.h>

#include <dds/dds_aux.h>

#include <qeo/log.h>
#include <qeo/mgmt_client_forwarder.h>
#include <qeo/mgmt_cert_parser.h>

#include "forwarder.h"
#include "core.h"
#include "config.h"
#include "core_util.h"
#include "security.h"
#include "security_util.h"

#include "qdm/qeo_Forwarder.h"

#define bool2str(b) (b) ? "true" : "false"

/**
 * The possible events used in the Client state machine
 */
typedef enum {
    CLIENT_EVENT_START,                 /**< Start state machine */
    CLIENT_EVENT_TIMEOUT,               /**< A timeout occurred */
    CLIENT_EVENT_LOC_SRV_DATA_RECEIVED, /**< Data from the location service has been received */
    CLIENT_EVENT_FWD_DATA_RECEIVED,     /**< Data on the QeoFwd topic received */
    CLIENT_EVENT_FWD_DATA_REMOVED,      /**< The data is removed */
} client_state_events_t;

static qeo_retcode_t fwd_locator_create(qeo_factory_t *factory);
static void fwd_locator_destroy(qeo_factory_t *factory);

static void fwd_server_timeout(uintptr_t userdata);
static qeo_retcode_t fwd_server_instance_remove(const qeo_factory_t *factory);
static qeo_retcode_t fwd_server_instance_publish(const qeo_factory_t *factory);

static void fwd_client_discovery_timeout(uintptr_t userdata);
static void fwd_client_reconfig(qeo_factory_t *factory,
                                qeo_mgmt_client_locator_t *locator,
                                int64_t device_id);
static void client_state_machine_eval_ul(qeo_factory_t *factory,
                                         client_state_events_t event,
                                         qeo_mgmt_client_locator_t *locator,
                                         int64_t device_id);
static void client_state_machine_eval(qeo_factory_t *factory,
                                      client_state_events_t event,
                                      qeo_mgmt_client_locator_t *locator,
                                      int64_t device_id);

/** This callback will be called for each forwarder in the list received from the management client. */
qeo_mgmt_client_retcode_t forwarder_cb(qeo_mgmt_client_forwarder_t* forwarder, void *cookie)
{
    qeo_factory_t             *factory = (qeo_factory_t *) cookie;
    qeo_mgmt_client_locator_t *locator = NULL;
    int                       i = 0;

    /* Get the IP address and port from the qeo_mgmt_client_forwarder_t info. */
    if ((forwarder != NULL) && (forwarder->nrOfLocators > 0)) {
        qeo_log_i("received %d locators", forwarder->nrOfLocators);
        for (i = 0, locator = forwarder->locators; i < forwarder->nrOfLocators; i++, locator++) {
            qeo_log_i("locator %d", i);
            if (locator != NULL) {
                qeo_log_i("valid locator");
                /* TODO: at the moment only one forwarder is taken into account. */
                if (factory->fwd.locator == NULL) {
                    client_state_machine_eval(factory, CLIENT_EVENT_LOC_SRV_DATA_RECEIVED, locator, forwarder->deviceID);
                }
            }
        }
        qeo_mgmt_client_free_forwarder(forwarder);
    }

    return QMGMTCLIENT_OK;
}

void result_cb(qeo_mgmt_client_retcode_t result, void *cookie)
{
    qeo_factory_t             *factory = (qeo_factory_t *) cookie;

    /* Allow other threads to wait until no request is pending anymore. */
    lock(&factory->mutex);
    factory->fwd.rqst_pending = false;
    if ((factory->fwd.timeout * 2) < qeocore_parameter_get_number("FWD_LOC_SRV_MAX_TIMEOUT")) {
        factory->fwd.timeout = factory->fwd.timeout * 2;
    } else {
        factory->fwd.timeout = qeocore_parameter_get_number("FWD_LOC_SRV_MAX_TIMEOUT");
    }
    pthread_cond_broadcast(&factory->fwd.wait_rqst_finished);
    unlock(&factory->mutex);

}

qeo_mgmt_client_retcode_t ssl_ctx_cb(SSL_CTX *ctx, void *cookie)
{
    qeo_mgmt_client_retcode_t client_ret  = QMGMTCLIENT_EFAIL;
    qeo_retcode_t             qeo_ret     = QEO_OK;
    qeo_security_hndl         qeo_sec   = (qeo_security_hndl)cookie;
    EVP_PKEY                  *key        = NULL;

    STACK_OF(X509) * certs = NULL;
    X509  *user_cert  = NULL;
    X509  *cert       = NULL;
    int   i           = 0;

    do {
        if (qeo_sec == NULL) {
            qeo_log_e("qeoSecPol == NULL");
            break;
        }

        qeo_ret = qeo_security_get_credentials(qeo_sec, &key, &certs);
        if (qeo_ret != QEO_OK) {
            qeo_log_e("failed to get credentials");
            break;
        }

        if (sk_X509_num(certs) <= 1) {
            qeo_log_e("not enough certificates in chain");
            break;
        }

        user_cert = sk_X509_value(certs, 0);
        if (user_cert == NULL) {
            qeo_log_e("user_cert == NULL");
            break;
        }

        if (!SSL_CTX_use_certificate(ctx, user_cert)) {
            qeo_log_e("SSL_CTX_use_certificate failed");
            break;
        }
        if (!SSL_CTX_use_PrivateKey(ctx, key)) {
            qeo_log_e("SSL_CTX_use_PrivateKey failed");
            break;
        }

        if (!SSL_CTX_check_private_key(ctx)) {
            qeo_log_e("SSL_CTX_check_private_key failed");
            break;
        }

        security_util_configure_ssl_ctx(ctx);
        for (i = 1; i < sk_X509_num(certs); i++) {
            qeo_log_i("add cert: %d", i);
            cert = sk_X509_value(certs, i);
            if (cert == NULL) {
                qeo_log_e("cert == NULL");
                break;
            }

            if (!X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx), cert)) {
                dump_openssl_error_stack("X509_STORE_add_cert failed");
                break;
            }
        }

        client_ret = QMGMTCLIENT_OK;
    } while (0);

    return client_ret;
}

/* ===[ locator API ]==================== */

static qeo_retcode_t fwd_locator_create(qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_OK;

    if (NULL == factory->fwd.locator) {
        factory->fwd.locator = calloc(1, sizeof(qeo_mgmt_client_locator_t));
    }
    if (NULL == factory->fwd.locator) {
        rc = QEO_ENOMEM;
    }

    return rc;
}

static void fwd_locator_destroy(qeo_factory_t *factory)
{
    if (factory->fwd.locator != NULL) {
        if (factory->fwd.locator->address != NULL) {
            free(factory->fwd.locator->address);
        }
        free(factory->fwd.locator);
        factory->fwd.locator = NULL;
    }
}

static qeo_retcode_t fwd_locator_update(qeo_factory_t *factory, qeo_mgmt_client_locator_t *locator)
{
    qeo_retcode_t rc = QEO_OK;

    if (NULL != factory->fwd.locator->address) {
        free(factory->fwd.locator->address);
    }
    factory->fwd.locator->type = locator->type;
    if (-1 != locator->port) {
        factory->fwd.locator->port = locator->port;
    }
    factory->fwd.locator->address = strdup(locator->address);
    if (NULL == factory->fwd.locator->address) {
        rc = QEO_ENOMEM;
    }

    return rc;
}

static qeo_retcode_t fwd_server_register(qeo_factory_t *factory)
{
    qeo_retcode_t             rc = QEO_OK;

    if (!qeocore_parameter_get_number("FWD_DISABLE_LOCATION_SERVICE")) {
        qeo_security_hndl         qeo_sec = factory->qeo_sec;
        qeo_mgmt_client_retcode_t mgmt_rc = QMGMTCLIENT_EFAIL;
        qeo_mgmt_client_ctx_t     *mgmt_client_ctx = NULL;
        qeo_mgmt_client_locator_t locator={QMGMT_LOCATORTYPE_TCPV4, factory->fwd.locator->address, factory->fwd.locator->port};
        int                       nrOfLocators = 1;

        do {
            if ((rc = qeo_security_get_mgmt_client_ctx(qeo_sec, &mgmt_client_ctx)) != QEO_OK) {
                qeo_log_e("register_forwarder get security mgmt client failed (rc=%d)", rc);
                break;
            }
            /* Now register the forwarder. */
            qeo_log_i("register the forwarder with locator address %s:port %d\n", locator.address, locator.port);
            if ((mgmt_rc = qeo_mgmt_client_register_forwarder(mgmt_client_ctx,
                                                              factory->qeo_id.url,
                                                              &locator,
                                                              nrOfLocators,
                                                              ssl_ctx_cb,
                                                              qeo_sec)) != QMGMTCLIENT_OK) {
                qeo_log_e("register forwarder failed (rc=%d)", mgmt_rc);
                rc = QEO_EFAIL;
                break;
            }

        } while (0);
    }
    return rc;
}

static qeo_retcode_t fwd_server_unregister(qeo_factory_t *factory)
{
    qeo_retcode_t             rc = QEO_OK;

    if (!qeocore_parameter_get_number("FWD_DISABLE_LOCATION_SERVICE")) {
        qeo_security_hndl         qeo_sec = factory->qeo_sec;
        qeo_mgmt_client_retcode_t mgmt_rc = QMGMTCLIENT_EFAIL;
        qeo_mgmt_client_ctx_t     *mgmt_client_ctx = NULL;
        int                       nrOfLocators = 0;


        do {
            if (QEO_OK != (rc = qeo_security_get_mgmt_client_ctx(qeo_sec, &mgmt_client_ctx))) {
                qeo_log_e("unregister_forwarder get security mgmt client failed (rc=%d)", rc);
                break;
            }

            /* Now register the forwarder. */
            qeo_log_i("unregister the forwarder");
            if ((mgmt_rc = qeo_mgmt_client_register_forwarder(mgmt_client_ctx,
                                                              factory->qeo_id.url,
                                                              NULL,
                                                              nrOfLocators,
                                                              ssl_ctx_cb,
                                                              qeo_sec)) != QMGMTCLIENT_OK) {
                qeo_log_e("unregister forwarder failed (rc=%d)", mgmt_rc);
                rc = QEO_EFAIL;
                break;
            }

        } while (0);
    }
    return rc;
}

void fwd_destroy(qeo_factory_t *factory)
{
    lock(&factory->mutex);
    if (true == factory->fwd.rqst_pending) {
        qeo_security_hndl         qeo_sec = factory->qeo_sec;
        qeo_mgmt_client_ctx_t     *mgmt_client_ctx = NULL;

        if (qeo_security_get_mgmt_client_ctx(qeo_sec, &mgmt_client_ctx) == QEO_OK) {
            qeo_mgmt_client_ctx_stop(mgmt_client_ctx);
        }

        /* Wait untill the fwd request is finished before continuing. */
        qeo_log_i("waiting for the fwd request to finish.");
        pthread_cond_wait(&factory->fwd.wait_rqst_finished, &factory->mutex);
    }
    unlock(&factory->mutex);
    if (factory->flags.is_forwarder) {
        fwd_server_unregister(factory);
        if (FWD_STATE_ENABLED == factory->fwd.u.server.state) {
            if (QEO_OK != fwd_server_instance_remove(factory)) {
                qeo_log_e("failed to remove instance from forwarder topic");
            }
        }
        if (NULL != factory->fwd.u.server.writer) {
            qeocore_writer_close(factory->fwd.u.server.writer);
        }
    }
    if (NULL != factory->fwd.reader) {
        qeocore_reader_close(factory->fwd.reader);
    }
    if (NULL != factory->fwd.timer) {
        DDS_Timer_delete(factory->fwd.timer);
    }
    pthread_cond_destroy(&factory->fwd.wait_rqst_finished);
    fwd_locator_destroy(factory);
}

static qeo_retcode_t fwd_get_list(qeo_factory_t *factory)
{
    qeo_retcode_t             rc = QEO_OK;

    if (!qeocore_parameter_get_number("FWD_DISABLE_LOCATION_SERVICE")) {
        qeo_security_hndl         qeo_sec = factory->qeo_sec;
        qeo_mgmt_client_retcode_t mgmt_rc = QMGMTCLIENT_EFAIL;
        qeo_mgmt_client_ctx_t     *mgmt_client_ctx = NULL;

        do {
            if ((rc = qeo_security_get_mgmt_client_ctx(qeo_sec, &mgmt_client_ctx)) != QEO_OK) {
                qeo_log_e("get_forwarders get security mgmt client failed (rc=%d)", rc);
                break;
            }
            /* Factory is already locked when calling this function. */
            if (true == factory->fwd.rqst_pending) {
                /* Just break, we will retry later. */
                qeo_log_i("no need to send request (previous fwd request still ongoing)");
                break;
            }

            /* Now get the list of forwarders. */
            factory->fwd.rqst_pending = true;
            mgmt_rc = qeo_mgmt_client_get_forwarders(mgmt_client_ctx, factory->qeo_id.url, forwarder_cb, result_cb,
                                                     factory, ssl_ctx_cb, qeo_sec);
            if (mgmt_rc == QMGMTCLIENT_OK) {
                qeo_log_d("get_forwarders succeeded");
            } else {
                factory->fwd.rqst_pending = false; /* result callback will not be called. */
                if ((mgmt_rc == QMGMTCLIENT_ESSL) || (mgmt_rc == QMGMTCLIENT_ENOTALLOWED)) {
                    qeo_log_e("get_forwarders failed (rc=%d), aborting", mgmt_rc);
                    rc = QEO_EFAIL;
                    break;
                }
                else {
                    qeo_log_e("get_forwarders failed (rc=%d), ignoring", mgmt_rc);
                }
            }

        } while (0);
    }
    return rc;
}

/* ===[ Forwarder state machine ]=========================================== */

static qeo_retcode_t fwd_server_start_forwarding(qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_EFAIL;
    char buf[64];

    snprintf(buf, sizeof(buf), "%s:%d", factory->fwd.locator->address, factory->fwd.locator->port);
    if (DDS_RETCODE_OK != DDS_parameter_set("TCP_PUBLIC", buf)) {
        qeo_log_e("failed to set public IP address");
    }
    else
    {
        rc = fwd_server_instance_publish(factory);
        if (QEO_OK != rc) {
            qeo_log_e("failed to publish instance on forwarder topic");
        }
        else {
            rc = fwd_server_register(factory);
            if (QEO_OK != rc) {
                qeo_log_e("failed to register at location service");
            }
        }
    }
    return rc;
}

#ifdef DEBUG
static char *server_state_to_str(forwarder_state_t state)
{
    switch (state) {
        case FWD_STATE_INIT:
            return "FWD_STATE_INIT";
        case FWD_STATE_WAIT_LOCAL:
            return "FWD_STATE_WAIT_LOCAL";
        case FWD_STATE_STARTING:
            return "FWD_STATE_STARTING";
        case FWD_STATE_ENABLED:
            return "FWD_STATE_ENABLED";
        case FWD_STATE_DISABLED:
            return "FWD_STATE_DISABLED";
        default:
            return "unknown state";
    }
}
#endif

/**
 * \pre This should be called with the factory lock taken.
 *
 * param[in] num_local Number of local forwarders discovered, -1 if not counted
 * param[in] timeout   True if a timeout occurred
 */
static qeo_retcode_t fwd_server_state_machine_eval_ul(qeo_factory_t *factory,
                                                      int num_local,
                                                      bool timeout,
                                                      bool public_ip_available)
{
    qeo_retcode_t rc = QEO_OK;

    qeo_log_i("old state %s : local fwd count = %d, timeout = %s, public ip available = %s",
              server_state_to_str(factory->fwd.u.server.state), num_local, bool2str(timeout), bool2str(public_ip_available));
    switch (factory->fwd.u.server.state) {
        case FWD_STATE_INIT: {
            /* done with initialization */
            int timeout = qeocore_parameter_get_number("FWD_WAIT_LOCAL_FWD");

            rc = ddsrc_to_qeorc(DDS_Timer_start(factory->fwd.timer, timeout, (uintptr_t)factory,
                                                fwd_server_timeout));
            if (QEO_OK != rc) {
                qeo_log_e("failed to start forwarder timer");
                break;
            }
            rc = qeocore_reader_enable(factory->fwd.reader);
            if (QEO_OK != rc) {
                qeo_log_e("failed to enable forwarder reader");
                break;
            }
            factory->fwd.u.server.state = FWD_STATE_WAIT_LOCAL;
            break;
        }
        case FWD_STATE_WAIT_LOCAL:
            /* done waiting for local forwarder */
            if (timeout) {
                /* no local forwarders found, try start forwarding ourselves */
                factory->fwd.u.server.state = FWD_STATE_STARTING;
                unlock(&factory->mutex);
                factory->listener.on_fwdfactory_get_public_locator(factory);
                lock(&factory->mutex);
            }
            else if (num_local > 0) {
                /* local forwarder(s) available stop timer */
                DDS_Timer_stop(factory->fwd.timer);
                factory->fwd.u.server.state = FWD_STATE_DISABLED;
            }
            break;
        case FWD_STATE_STARTING:
            /* done waiting for publicly available locator */
            if (num_local > 0) {
                factory->fwd.u.server.state = FWD_STATE_DISABLED;
            }
            else if (public_ip_available) {
                if (QEO_OK == fwd_server_start_forwarding(factory)) {
                    factory->fwd.u.server.state = FWD_STATE_ENABLED;
                }
                else {
                    /* failed to start */
                    factory->fwd.u.server.state = FWD_STATE_DISABLED;
                }
            }
            break;
        case FWD_STATE_ENABLED:
            if (public_ip_available) {
                if (QEO_OK == fwd_server_start_forwarding(factory)) {
                    factory->fwd.u.server.state = FWD_STATE_ENABLED;
                }
                else {
                    /* failed to start */
                    factory->fwd.u.server.state = FWD_STATE_DISABLED;
                }
            }
            break;
        case FWD_STATE_DISABLED:
            if (0 == num_local) {
                /* no local forwarders anymore, try start forwarding ourselves */
                factory->fwd.u.server.state = FWD_STATE_STARTING;
                unlock(&factory->mutex);
                factory->listener.on_fwdfactory_get_public_locator(factory);
                lock(&factory->mutex);
            }
            break;
    }
    qeo_log_i("new state %s", server_state_to_str(factory->fwd.u.server.state));
    return rc;
}

static void fwd_server_state_machine_eval(qeo_factory_t *factory,
                                          int num_local,
                                          bool timeout)
{
    lock(&factory->mutex);
    fwd_server_state_machine_eval_ul(factory, num_local, timeout, false);
    unlock(&factory->mutex);
}

/* ===[ Forwarder topic handling ]========================================== */

static qeo_retcode_t fwd_server_instance_remove(const qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_EFAIL;
    org_qeo_system_Forwarder_t fwd_data = {};

    fwd_data.deviceId = factory->qeo_id.device_id;
    rc = qeocore_writer_remove(factory->fwd.u.server.writer, &fwd_data);
    return rc;
}

static qeo_retcode_t fwd_server_instance_publish(const qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_OK;
    org_qeo_system_Forwarder_t fwd_data = {
        .locator =  DDS_SEQ_INITIALIZER(org_qeo_system_ForwarderLocator_t)
    };

    fwd_data.deviceId = factory->qeo_id.device_id;
    /* add locator if available */
    if (QMGMT_LOCATORTYPE_UNKNOWN != factory->fwd.locator->type) {
        org_qeo_system_ForwarderLocator_t locator_data;

        locator_data.type = factory->fwd.locator->type;
        locator_data.port = factory->fwd.locator->port;
        locator_data.address = factory->fwd.locator->address;
        rc = ddsrc_to_qeorc(dds_seq_append(&fwd_data.locator, &locator_data));
    }
    if (QEO_OK == rc) {
        rc = qeocore_writer_write(factory->fwd.u.server.writer, &fwd_data);
    }
    dds_seq_cleanup(&fwd_data.locator);
    return rc;
}

static int fwd_server_count_instances(const qeocore_reader_t *reader)
{
    qeo_retcode_t rc = QEO_OK;
    qeocore_filter_t filter = { 0 };
    qeocore_data_t *data;
    int cnt = 0;

    data = qeocore_reader_data_new(reader);
    if (NULL != data) {
        filter.instance_handle = DDS_HANDLE_NIL;
        while (1) {
            rc = qeocore_reader_read(reader, &filter, data);
            if (QEO_OK == rc) {
                filter.instance_handle = qeocore_data_get_instance_handle(data);
                cnt++;
#ifdef DEBUG
                {
                    const org_qeo_system_Forwarder_t *fwd = qeocore_data_get_data(data);

                    qeo_log_d("forwarder %d : id=%" PRIx64 " -> %s", cnt, fwd->deviceId,
                              (fwd->deviceId == reader->entity.factory->qeo_id.device_id ? "self" : "other"));
                }
#endif
                qeocore_data_reset(data);
                continue;
            }
            else if (QEO_ENODATA == rc) {
                rc = QEO_OK;
            }
            /* QEO_ENODATA or error */
            break;
        }
        qeocore_data_free(data);
    }
    if (QEO_OK == rc) {
        qeo_log_i("found %d local forwarders", cnt);
    }
    else {
        qeo_log_e("failed to read all local forwarders");
    }
    return cnt;
}

static void fwd_server_on_data(const qeocore_reader_t *reader,
                                const qeocore_data_t *data,
                                uintptr_t userdata)
{
    /* count number of local forwarder instances */
    if (QEOCORE_NOTIFY == qeocore_data_get_status(data)) {
        qeo_factory_t *factory = (qeo_factory_t *)reader->entity.factory;
        int cnt = fwd_server_count_instances(reader);

        fwd_server_state_machine_eval(factory, cnt, false);
    }
}

static void fwd_server_timeout(uintptr_t userdata)
{
    fwd_server_state_machine_eval((qeo_factory_t *)userdata, -1, true);
}

static void fwd_client_on_data(const qeocore_reader_t *reader,
                               const qeocore_data_t *data,
                               uintptr_t userdata)
{
    qeo_factory_t *factory = (qeo_factory_t *)userdata;
    qeocore_data_status_t status;
    org_qeo_system_Forwarder_t *fwd_data;
    org_qeo_system_ForwarderLocator_t fwd_locator;
    qeo_mgmt_client_locator_t locator;

    status = qeocore_data_get_status(data);
    if (QEOCORE_DATA == status) {
        fwd_data = (org_qeo_system_Forwarder_t *)qeocore_data_get_data(data);
        /* check locator */
        if ((NULL != fwd_data) && (DDS_SEQ_LENGTH(fwd_data->locator) > 0)) {
            fwd_locator = DDS_SEQ_ITEM(fwd_data->locator, 0);
            locator.type = fwd_locator.type;
            locator.port = fwd_locator.port;
            locator.address = fwd_locator.address;
            client_state_machine_eval(factory, CLIENT_EVENT_FWD_DATA_RECEIVED, &locator, fwd_data->deviceId);
        }
    }
    else if (QEOCORE_REMOVE == status) {
        fwd_data = (org_qeo_system_Forwarder_t *)qeocore_data_get_data(data);
        if (NULL != fwd_data) {
            client_state_machine_eval(factory, CLIENT_EVENT_FWD_DATA_REMOVED, NULL, fwd_data->deviceId);
        }
    }
}

/* ===[ Forwarder factory 'public' API ]==================================== */

qeo_retcode_t fwd_init_pre_auth(qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_OK;

    factory->flags.is_forwarder = (NULL != factory->listener.on_fwdfactory_get_public_locator ? 1 : 0);
    do {
        pthread_cond_init(&factory->fwd.wait_rqst_finished, NULL);
        factory->fwd.timer = DDS_Timer_create("qeofwd");
        if (NULL == factory->fwd.timer) {
            rc = QEO_ENOMEM;
            qeo_log_e("failed to create forwarder timer");
            break;
        }
        if (factory->flags.is_forwarder) {
            /* enable forwarder logic */
            rc = ddsrc_to_qeorc(DDS_parameter_set("FORWARD", "15"));
            if (QEO_OK != rc) {
                qeo_log_e("failed to enable FORWARD");
                break;
            }
        }
    } while (0);
    return rc;
}

qeo_retcode_t fwd_init_post_auth(qeo_factory_t *factory)
{
    qeo_retcode_t rc = QEO_EFAIL;
    qeocore_type_t *type = NULL;

    /* this will eventually also take the factory lock, so postponing our lock */
    type = qeocore_type_register_tsm(factory, org_qeo_system_Forwarder_type, org_qeo_system_Forwarder_type->name);
    if (NULL != type) {
        lock(&factory->mutex);
        if (factory->flags.is_forwarder) {
            factory->fwd.listener.on_data = fwd_server_on_data;
            factory->fwd.reader = qeocore_reader_open(factory, type, org_qeo_system_Forwarder_type->name,
                                                      QEOCORE_EFLAG_STATE_UPDATE, &factory->fwd.listener, NULL);
            if (NULL != factory->fwd.reader) {
                factory->fwd.u.server.writer = qeocore_writer_open(factory, type, org_qeo_system_Forwarder_type->name,
                                                                   QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE,
                                                                   NULL, NULL);
                if (NULL != factory->fwd.u.server.writer) {
                    factory->fwd.u.server.state = FWD_STATE_INIT;
                    rc = fwd_server_state_machine_eval_ul(factory, -1, false, false);
                }
            }
        } else {
            factory->fwd.listener.on_data = fwd_client_on_data;
            factory->fwd.listener.userdata = (uintptr_t)factory;
            factory->fwd.reader = qeocore_reader_open(factory, type, org_qeo_system_Forwarder_type->name,
                                                      QEOCORE_EFLAG_STATE_DATA, &factory->fwd.listener, NULL);
            /* note: reader construction failure is not fatal */
            if (qeocore_parameter_get_number("FWD_DISABLE_FORWARDING")) {
                qeo_log_i("Disable forwarding");
                factory->fwd.u.client.state = FWD_CLIENT_STATE_READY;
            }
            else {
                factory->fwd.u.client.state = FWD_CLIENT_STATE_INIT;
                client_state_machine_eval_ul(factory, CLIENT_EVENT_START, NULL, -1);
            }
            rc = QEO_OK;
        }
        unlock(&factory->mutex);
        qeocore_type_free(type);
    }
    return rc;
}

qeo_retcode_t fwd_server_reconfig(qeo_factory_t *factory,
                                  const char *ip_address,
                                  int port)
{
    qeo_retcode_t rc = QEO_OK;

    VALIDATE_NON_NULL(factory);
    if (!factory->flags.is_forwarder) {
        return QEO_EINVAL;
    }
    VALIDATE_NON_NULL(ip_address);
    if (!strcmp("", ip_address)) {
        return QEO_EINVAL;
    }
    lock(&factory->mutex);
    if (QEO_OK == fwd_locator_create(factory)) {
        qeo_mgmt_client_locator_t locator = {QMGMT_LOCATORTYPE_TCPV4, (char *)ip_address, port};

        if (QEO_OK == fwd_locator_update(factory, &locator)) {
            fwd_server_state_machine_eval_ul(factory, -1, false, true);
        }
    }
    unlock(&factory->mutex);
    return rc;
}

/* ===[ Client helper functions ]=========================================== */

static char *client_state_to_str(client_state_t state)
{
    switch (state) {
        case FWD_CLIENT_STATE_INIT:
            return "FWD_CLIENT_STATE_INIT";
        case FWD_CLIENT_STATE_WAIT:
            return "FWD_CLIENT_STATE_WAIT";
        case FWD_CLIENT_STATE_WAIT_READER:
            return "FWD_CLIENT_STATE_WAIT_READER";
        case FWD_CLIENT_STATE_READY:
            return "FWD_CLIENT_STATE_READY";
        default:
            return "unknown state";
    }
}

static char *client_event_to_str(client_state_events_t event)
{
    switch (event) {
        case CLIENT_EVENT_START:
            return "CLIENT_EVENT_START";
        case CLIENT_EVENT_TIMEOUT:
            return "CLIENT_EVENT_TIMEOUT";
        case CLIENT_EVENT_LOC_SRV_DATA_RECEIVED:
            return "CLIENT_EVENT_LOC_SRV_DATA_RECEIVED";
        case CLIENT_EVENT_FWD_DATA_RECEIVED:
            return "CLIENT_EVENT_FWD_DATA_RECEIVED";
        case CLIENT_EVENT_FWD_DATA_REMOVED:
            return "CLIENT_EVENT_FWD_DATA_REMOVED";
        default:
            return "unknown event";
    }
}

static void fwd_client_reconfig(qeo_factory_t *factory,
                                qeo_mgmt_client_locator_t *locator,
                                int64_t device_id)
{
    qeo_retcode_t rc = QEO_OK;

    /* Configure the locator in the factory */
    if (QEO_OK == fwd_locator_create(factory)) {
        if (QEO_OK == fwd_locator_update(factory, locator)) {
            char *tcp_server = calloc(1, strlen(locator->address) + 12);

            if (NULL != tcp_server) {
                snprintf(tcp_server, strlen(locator->address) + 12, "%s:%d", locator->address, locator->port);
                qeo_log_i("use forwarder %s", tcp_server);
                if ((rc = core_factory_set_tcp_server_no_lock(factory, tcp_server)) != QEO_OK) {
                    qeo_log_e("set tcp server failed (rc=%d)", rc);
                }
                free(tcp_server);
            }
        }
    }
    /* Configure the device ID in the factory */
    factory->fwd.device_id = device_id;
}

static qeo_retcode_t client_start_timer(qeo_factory_t *factory, bool reset)
{
    qeo_retcode_t rc = QEO_OK;

    if (reset) {
        factory->fwd.timeout = qeocore_parameter_get_number("FWD_LOC_SRV_MIN_TIMEOUT");
    }
    qeo_log_i("retry contacting location service after %ds", factory->fwd.timeout/1000);
    rc = ddsrc_to_qeorc(DDS_Timer_start(factory->fwd.timer, factory->fwd.timeout, (uintptr_t)factory,
                                        fwd_client_discovery_timeout));
    return rc;
}

static void client_state_machine_eval_ul(qeo_factory_t *factory,
                                         client_state_events_t event,
                                         qeo_mgmt_client_locator_t *locator,
                                         int64_t device_id)
{
    qeo_log_i("received event %s in state %s", client_event_to_str(event),
              client_state_to_str(factory->fwd.u.client.state));
    switch (factory->fwd.u.client.state) {
        case FWD_CLIENT_STATE_INIT:
            switch (event) {
                case CLIENT_EVENT_START:
                    factory->fwd.device_id = -1;
                    if (QEO_OK == client_start_timer(factory, 1)) {
                        factory->fwd.u.client.state = FWD_CLIENT_STATE_WAIT;
                    }
                    if (QEO_OK != qeocore_reader_enable(factory->fwd.reader)) {
                        qeo_log_e("error enabling forwarder topic reader");
                    }
                    if (QEO_OK != fwd_get_list(factory)) {
                        qeo_log_e("error requesting list of forwarders");
                    }
                    break;
                default:
                    qeo_log_e("unexpected event %s in state %s", client_event_to_str(event),
                              client_state_to_str(factory->fwd.u.client.state));
                    break;
            }
            break;
        case FWD_CLIENT_STATE_WAIT:
            switch (event) {
                case CLIENT_EVENT_TIMEOUT:
                    if (QEO_OK == client_start_timer(factory, 0)) {
                        factory->fwd.u.client.state = FWD_CLIENT_STATE_WAIT;
                    }
                    if (QEO_OK != fwd_get_list(factory)) {
                        qeo_log_e("error requesting list of forwarders");
                    }
                    break;
                case CLIENT_EVENT_LOC_SRV_DATA_RECEIVED:
                    fwd_client_reconfig(factory, locator, device_id);
                    factory->fwd.u.client.state = FWD_CLIENT_STATE_WAIT_READER;
                    break;
                case CLIENT_EVENT_FWD_DATA_RECEIVED:
                    fwd_client_reconfig(factory, locator, device_id);
                    DDS_Timer_stop(factory->fwd.timer);
                    factory->fwd.u.client.state = FWD_CLIENT_STATE_READY;
                    break;
                default:
                    qeo_log_e("unexpected event %s in state %s", client_event_to_str(event),
                              client_state_to_str(factory->fwd.u.client.state));
                    break;
            }
            break;
        case FWD_CLIENT_STATE_WAIT_READER:
            switch (event) {
                case CLIENT_EVENT_TIMEOUT:
                    if (QEO_OK == client_start_timer(factory, 0)) {
                        factory->fwd.u.client.state = FWD_CLIENT_STATE_WAIT;
                    }
                    fwd_locator_destroy(factory);
                    factory->fwd.device_id = -1;
                    if (QEO_OK != fwd_get_list(factory)) {
                        qeo_log_e("error requesting list of forwarders");
                    }
                    break;
                case CLIENT_EVENT_FWD_DATA_RECEIVED:
                    DDS_Timer_stop(factory->fwd.timer);
                    factory->fwd.u.client.state = FWD_CLIENT_STATE_READY;
                    if (factory->fwd.device_id != device_id) {
                        fwd_client_reconfig(factory, locator, device_id);
                    }
                    break;
                default:
                    qeo_log_e("unexpected event %s in state %s", client_event_to_str(event),
                              client_state_to_str(factory->fwd.u.client.state));
                    break;
            }
            break;
        case FWD_CLIENT_STATE_READY:
            switch (event) {
                case CLIENT_EVENT_FWD_DATA_REMOVED:
                    if (QEO_OK == client_start_timer(factory, 1)) {
                        factory->fwd.u.client.state = FWD_CLIENT_STATE_WAIT;
                    }
                    fwd_locator_destroy(factory);
                    factory->fwd.device_id = -1;
                    if (QEO_OK != fwd_get_list(factory)) {
                        qeo_log_e("error requesting list of forwarders");
                    }
                    break;
                default:
                    qeo_log_e("unexpected event %s in state %s", client_event_to_str(event),
                              client_state_to_str(factory->fwd.u.client.state));
                    break;
            }
            break;
    }
    qeo_log_i("new state %s", client_state_to_str(factory->fwd.u.client.state));
}

static void client_state_machine_eval(qeo_factory_t *factory,
                                      client_state_events_t event,
                                      qeo_mgmt_client_locator_t *locator,
                                      int64_t device_id)
{
    lock(&factory->mutex);
    client_state_machine_eval_ul(factory, event, locator, device_id);
    unlock(&factory->mutex);
}

static void fwd_client_discovery_timeout(uintptr_t userdata)
{
    qeo_factory_t *factory = (qeo_factory_t *)userdata;

    client_state_machine_eval(factory, CLIENT_EVENT_TIMEOUT, NULL, -1);
}
