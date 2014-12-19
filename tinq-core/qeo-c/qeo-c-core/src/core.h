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

#ifndef CORE_H_
#define CORE_H_

#include <stdbool.h>
#include <pthread.h>

#include <dds/dds_aux.h>
#include <dds/dds_xtypes.h>
#include <dds/dds_dreader.h>

#include <qeocore/api.h>
#include <qeo/mgmt_client_forwarder.h>

#include "list.h"
#include "typesupport.h"
#include "security.h"
#include "policy.h"

#define VALIDATE_NON_NULL(p) if (NULL == (p)) return QEO_EINVAL;

/**
 * Forwarder server specific states.
 */
typedef enum {
    FWD_STATE_INIT,            /**< initialization started */
    FWD_STATE_WAIT_LOCAL,      /**< waiting for local forwarders */
    FWD_STATE_STARTING,        /**< forwarder is starting */
    FWD_STATE_ENABLED,         /**< enabled as a forwarder */
    FWD_STATE_DISABLED,        /**< disabled as a forwarder */
} forwarder_state_t;

/**
 * Forwarder client specific states.
 */
typedef enum {
    FWD_CLIENT_STATE_INIT,       /**< initialization started */
    FWD_CLIENT_STATE_WAIT,       /**< waiting for any forwarders */
    FWD_CLIENT_STATE_WAIT_READER,/**< waiting for the forwarder on the topic */
    FWD_CLIENT_STATE_READY,      /**< the client is ready to roll */
} client_state_t;

/**
 * Server specific part of forwarder struct.
 */
typedef struct qeo_fwd_server_s {
    forwarder_state_t state;
    qeocore_writer_t *writer;
} fwd_server_t;

/**
 * Client specific part of forwarder struct.
 */
typedef struct qeo_fwd_client_s {
    client_state_t state;
} fwd_client_t;

/**
 * Structure containing information for forwarding (client and forwarder).
 */
typedef struct qeo_forwarder_s {
    qeo_mgmt_client_locator_t *locator; /**< The locator (list) to be used by the client or
                                             to be registered by a forwarder. */
    int64_t device_id;                  /**< This is the device ID as known by the location service.
                                             It is by no means the Qeo DeviceId!! */
    qeocore_reader_t *reader;           /**< The reader for the QeoFwd topic. */
    DDS_Timer timer;                    /**< A general timer */
    int timeout;                        /**< The timeout value for the timer */
    pthread_cond_t wait_rqst_finished;  /**< Conditional variable used to wait untill a fwd request is finished.*/
    bool rqst_pending;                  /**< Boolean indicating whether a request towards the server
                                             to get a list of forwarders is currenctly ongoing. */
    qeocore_reader_listener_t listener;
    union {
        fwd_client_t client;
        fwd_server_t server;
    } u;
} forwarder_t;

struct qeo_factory_s {
    struct {
        unsigned initialized : 1;
        unsigned is_forwarder : 1;
    } flags;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool ready_to_destroy;
    qeocore_factory_listener_t listener;
    qeocore_domain_id_t domain_id;
    qeo_security_hndl qeo_sec;
    qeo_security_policy_hndl qeo_pol;
    qeo_identity_t qeo_id;
    DDS_DomainParticipant dp;
    uintptr_t      userdata;
    forwarder_t fwd;
};

typedef struct {
    const qeo_factory_t *factory;
    struct {
        unsigned is_writer : 1;        /**< reader or writer ? */
        unsigned is_state : 1;         /**< event or state ? */
        unsigned enabled : 1;          /**< has the entity been enabled ? */
    } flags;
    qeocore_type_t *type_info;
    DDS_Topic topic;
} entity_t;

typedef struct {
    DDS_SampleInfoSeq seq_info;
    DDS_DynamicDataSeq seq_data;
    DDS_DynamicData single_data;
    DDS_DynamicType single_type;
} dynamic_data_t;

typedef struct {
    DDS_SampleInfoSeq seq_info;
    DDS_DataSeq seq_data;
    void *single_data;
} tsm_based_data_t;

struct qeocore_data_s {
    union {
        const qeocore_reader_t *reader;
        const qeocore_writer_t *writer;
    } rw;
    struct {
        unsigned is_writer : 1;
        unsigned is_tsm_based : 1;
        unsigned is_single : 1;
        unsigned needs_return_loan : 1;
        unsigned notify_only : 1;
    } flags;
    DDS_ReturnCode_t ddsrc;
    union {
        tsm_based_data_t tsm_based;
        dynamic_data_t dynamic;
    } d;
};

struct qeocore_reader_s {
    entity_t entity; // keep first
    DDS_Subscriber sub;
    DDS_DataReader dr;
    struct {
        unsigned ignore_dispose : 1;
        unsigned notify_only : 1;
        unsigned close_requested : 1;
    } flags;
    qeocore_reader_listener_t listener;
};

struct qeocore_writer_s {
    entity_t entity; // keep first
    DDS_Publisher pub;
    DDS_DataWriter dw;
    qeocore_writer_listener_t listener;
};

static inline DDS_SampleInfo *info_from_data(const qeocore_data_t *data)
{
    if (data->flags.is_tsm_based) {
        return (DDS_SampleInfo *)DDS_SEQ_ITEM(data->d.tsm_based.seq_info, 0);
    }
    else {
        return (DDS_SampleInfo *)DDS_SEQ_ITEM(data->d.dynamic.seq_info, 0);
    }
}


static inline void *sample_from_data(const qeocore_data_t *data)
{
    void *p = NULL;

    if (data->flags.is_tsm_based) {
        const tsm_based_data_t *tsmdata = &data->d.tsm_based;

        p = (data->flags.is_single ? tsmdata->single_data : DDS_SEQ_ITEM(tsmdata->seq_data, 0));
    }
    else {
        const dynamic_data_t *dyndata = &data->d.dynamic;

        p = (data->flags.is_single ? dyndata->single_data : DDS_SEQ_ITEM(dyndata->seq_data, 0));
    }
    return p;
}

static inline DDS_DynamicType dtype_from_data(const qeocore_data_t *data,
                                              const dynamic_data_t *dyndata)
{
    DDS_DynamicType dtype = NULL;

    if (NULL != dyndata->single_type) {
        dtype = dyndata->single_type;
    }
    else if (data->flags.is_writer) {
        dtype = data->rw.writer->entity.type_info->u.dynamic.dtype;
    }
    else {
        dtype = data->rw.reader->entity.type_info->u.dynamic.dtype;
    }
    return dtype;
}

qeo_retcode_t core_register_type(const qeo_factory_t *factory,
                                 DDS_DynamicTypeSupport dts,
                                 DDS_TypeSupport ts,
                                 const char *name);
qeo_retcode_t core_unregister_type(const qeo_factory_t *factory,
                                   DDS_DynamicTypeSupport dts,
                                   DDS_TypeSupport ts,
                                   const char *name);

void core_data_clean(qeocore_data_t *data);
qeo_retcode_t core_data_init(qeocore_data_t *data,
                             const qeocore_reader_t *reader,
                             const qeocore_writer_t *writer);
qeocore_data_t *core_data_alloc(const qeocore_reader_t *reader,
                            const qeocore_writer_t *writer);

qeocore_reader_t *core_create_reader(const qeo_factory_t *factory,
                                     qeocore_type_t *type,
                                     const char *topic_name,
                                     int flags,
                                     const qeocore_reader_listener_t *listener,
                                     qeo_retcode_t *prc);
qeo_retcode_t core_enable_reader(qeocore_reader_t *reader);

/**
 * Free all resources associated with the reader.
 *
 * \param[in] reader the reader to be deleted, non-\c NULL
 * \param[in] lock take factory lock (or not)
 */
void core_delete_reader(qeocore_reader_t *reader,
                        bool lock);

qeo_retcode_t core_read_or_take(const qeocore_reader_t *reader,
                                const qeocore_filter_t *filter,
                                qeocore_data_t *data,
                                int take);

qeocore_writer_t *core_create_writer(const qeo_factory_t *factory,
                                     qeocore_type_t *type,
                                     const char *topic_name,
                                     int flags,
                                     const qeocore_writer_listener_t *listener,
                                     qeo_retcode_t *prc);
qeo_retcode_t core_enable_writer(qeocore_writer_t *writer);

/**
 * Free all resources associated with the writer.
 *
 * \param[in] writer the writer to be deleted, non-\c NULL
 * \param[in] lock take factory lock (or not)
 */
void core_delete_writer(qeocore_writer_t *writer,
                        bool lock);

qeo_retcode_t core_write(const qeocore_writer_t *writer,
                         const qeocore_data_t *data,
                         const void *sample);

qeo_retcode_t core_remove(const qeocore_writer_t *writer,
                          const qeocore_data_t *data,
                          const void *sample);

qeo_factory_t *core_get_open_domain_factory();

qeo_retcode_t core_factory_set_tcp_server_no_lock(qeo_factory_t  *factory,
                                                  const char     *tcp_server);

#endif /* CORE_H_ */
