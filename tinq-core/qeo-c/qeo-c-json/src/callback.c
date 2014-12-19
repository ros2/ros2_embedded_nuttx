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
#include <qeojson/api.h>
#include <qeo/log.h>
#include <qeocore/api.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "qeo_json_p.h"
#include "json_types_util.h"

/*########################################################################
#                                                                       #
#  TYPES and DEFINES SECTION                                            #
#                                                                       #
########################################################################*/
typedef enum {
    QEOJSON_PERM_UNKNOWN,
    QEOJSON_PERM_ALLOW,
    QEOJSON_PERM_DENY
}qeojson_perm_t;
/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                           #
#                                                                       #
########################################################################*/
static qeo_retcode_t qeojson_policy_user_get_perm_by_uid(json_t * policy, int64_t uid, qeojson_perm_t *perm);
static qeo_retcode_t qeojson_policy_mv(json_t * source_policy, json_t *dest_policy);
static qeo_retcode_t qeojson_policy_add_user_by_uid(json_t * policy, int64_t uid, qeojson_perm_t perm);
static qeo_retcode_t qeojson_policy_cp_user_by_uid(json_t * source_policy, json_t * dest_policy, int64_t uid);
static qeo_policy_perm_t qeojson_reader_process_uid(qeojson_reader_t * jsonreader, int64_t uid);
static qeo_policy_perm_t qeojson_writer_process_uid(qeojson_writer_t * jsonwriter, int64_t uid);

/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                              #
#                                                                       #
########################################################################*/

/*#######################################################################
 #                                                                       #
 # STATIC FUNCTION IMPLEMENTATION                                        #
 #                                                                       #
 ########################################################################*/
static qeo_retcode_t qeojson_policy_user_get_perm_by_uid(json_t * policy, int64_t uid, qeojson_perm_t *perm)
{
    qeo_retcode_t ret = QEO_OK;
    json_t *users = NULL;
    json_t *user = NULL;
    json_t *id = NULL;
    json_t *allow = NULL;
    int i = 0;

    do
    {
        *perm = QEOJSON_PERM_UNKNOWN;
        users = json_object_get(policy,"users");
        if (!json_is_array(users)) {
            ret = QEO_EFAIL;
            qeo_log_e("json type error");
            break;
        }

        for(i=0; i < json_array_size(users);i++) {
            user = json_array_get(users,i);
            if (NULL == user) {
                ret = QEO_EFAIL;
                break;
            }

            id = json_object_get(user,"id");
            if (!json_is_integer(id)) {
                ret = QEO_EFAIL;
                qeo_log_e("json type error");
                break;
            }
            if (json_integer_value(id) == uid){
                //found uid in policy cache -> get allow

                allow = json_object_get(user,"allow");
                if (!json_is_boolean(allow)) {
                    ret = QEO_EFAIL;
                    qeo_log_e("json type error");
                    break;
                }

                if (json_is_true(allow)) {
                    *perm = QEOJSON_PERM_ALLOW;    
                }
                else{
                    *perm = QEOJSON_PERM_DENY;    
                }
                break;   
            }

        }
        if (ret != QEO_OK) {
            break;
        }

    } while(0);

    return ret;

}

static qeo_retcode_t qeojson_policy_mv(json_t * source_policy, json_t *dest_policy)
{
    qeo_retcode_t ret = QEO_EFAIL;
    json_t *users = NULL;

    do
    {
        //mv users
        users = json_object_get(source_policy,"users");
        if (!json_is_array(users)) {
            qeo_log_e("json type error");
            break;
        }

        if (0 != json_object_del(dest_policy,"users")) {
            qeo_log_e("json_object_del failed");
            break;

        }

        if ( 0 != json_object_set(dest_policy,"users",users)) {
            qeo_log_e("json_object_set failed");
            break;
        }

        if ( 0 != json_object_clear(source_policy)) {
            qeo_log_e("json_object_clear failed");
            break;
        }

        if ( 0 != json_object_set_new(source_policy,"users",json_array())) {
            qeo_log_e("json_object_set_new failed");
            break;

        }

        ret = QEO_OK;
    } while(0);

    return ret;

}

static qeo_retcode_t qeojson_policy_add_user_by_uid(json_t * policy, int64_t uid, qeojson_perm_t perm)
{
    qeo_retcode_t ret = QEO_EFAIL;
    json_t *users = NULL;
    json_t *user = NULL;

    do
    {
        users = json_object_get(policy,"users");
        if (!json_is_array(users)) {
            qeo_log_e("json type error");
            break;
        }

        //not found uid in policy cache -> add uid and  allow
        user = json_object();
        if (!json_is_object(user)) {
            qeo_log_e("json type error");
            break;
        }
        if ( 0 != json_object_set_new(user,"id",json_integer(uid))){
            qeo_log_e("json_object_set_new failed");
            break;
        }
        // default rule for new uid's == allow 
        if (0 != json_object_set_new(user,"allow",(perm == QEOJSON_PERM_ALLOW) ? json_true():json_false())){
            qeo_log_e("json_object_set_new failed");
            break;
        }
        if (0 != json_array_append(users,user)) {
            qeo_log_e("json_array_append failed");
            break;
        }
        json_decref(user);



        ret = QEO_OK;
    } while(0);

    return ret;

}
static qeo_retcode_t qeojson_policy_cp_user_by_uid(json_t * source_policy, json_t * dest_policy, int64_t uid)
{
    qeo_retcode_t ret = QEO_OK;
    json_t *src_users = NULL;
    json_t *dest_users = NULL;
    json_t *user = NULL;
    json_t *id = NULL;
    int i = 0;

    do
    {
        src_users = json_object_get(source_policy,"users");
        if (!json_is_array(src_users)) {
            ret = QEO_EFAIL;
            qeo_log_e("json type error");
            break;
        }

        dest_users = json_object_get(dest_policy,"users");
        if (!json_is_array(dest_users)) {
            ret = QEO_EFAIL;
            qeo_log_e("json type error");
            break;
        }

        for(i=0; i < json_array_size(src_users);i++) {
            user = json_array_get(src_users,i);
            if (NULL == user) {
                ret = QEO_EFAIL;
                break;
            }

            id = json_object_get(user,"id");
            if (!json_is_integer(id)) {
                ret = QEO_EFAIL;
                qeo_log_e("json type error");
                break;
            }
            if (json_integer_value(id) == uid){
                break;   
            }

        }
        if (ret != QEO_OK) {
            break;
        }
        
        if(0 != json_array_append(dest_users,user)) {
            ret = QEO_EFAIL;
            qeo_log_e("json type error");
            break;
        }

    } while(0);

    return ret;

}

static qeo_policy_perm_t qeojson_reader_process_uid(qeojson_reader_t * jsonreader, int64_t uid)
{
    qeo_policy_perm_t               perm          = QEO_POLICY_DENY;
    qeojson_perm_t qeojson_perm = QEOJSON_PERM_UNKNOWN;
    
    do {
        qeojson_policy_lock(&jsonreader->policy_mutex);
        if (QEO_OK != qeojson_policy_user_get_perm_by_uid(jsonreader->policy_cache,uid, &qeojson_perm)) {
            qeo_log_e("qeojson_policy_user_get_allowed_by_uid failed");
            break;
        }

        if (QEOJSON_PERM_UNKNOWN == qeojson_perm) {
            qeojson_perm = QEOJSON_PERM_ALLOW;
            if (QEO_OK != qeojson_policy_add_user_by_uid(jsonreader->policy_cache,uid,qeojson_perm)){
                qeo_log_e("qeojson_policy_add_user_by_uid failed");
                break;
            }
        }

        if(QEO_OK != qeojson_policy_cp_user_by_uid(jsonreader->policy_cache, jsonreader->policy,uid)){
            qeo_log_e("qeojson_policy_cp_user_by_uid failed");
            break;
        }

        if (QEOJSON_PERM_ALLOW == qeojson_perm) {
            perm = QEO_POLICY_ALLOW;
        }

    }while(0);
    qeojson_policy_unlock(&jsonreader->policy_mutex);

    return perm; 

}

static qeo_policy_perm_t qeojson_writer_process_uid(qeojson_writer_t * jsonwriter, int64_t uid)
{
    qeo_policy_perm_t               perm          = QEO_POLICY_DENY;
    qeojson_perm_t qeojson_perm = QEOJSON_PERM_UNKNOWN;
    
    do {
        qeojson_policy_lock(&jsonwriter->policy_mutex);
        if (QEO_OK != qeojson_policy_user_get_perm_by_uid(jsonwriter->policy_cache,uid, &qeojson_perm)) {
            qeo_log_e("qeojson_policy_user_get_allowed_by_uid failed");
            break;
        }

        if (QEOJSON_PERM_UNKNOWN == qeojson_perm) {
            qeojson_perm = QEOJSON_PERM_ALLOW;
            if (QEO_OK != qeojson_policy_add_user_by_uid(jsonwriter->policy_cache,uid,qeojson_perm)){
                qeo_log_e("qeojson_policy_add_user_by_uid failed");
                break;
            }
        }

        if(QEO_OK != qeojson_policy_cp_user_by_uid(jsonwriter->policy_cache, jsonwriter->policy,uid)){
            qeo_log_e("qeojson_policy_cp_user_by_uid failed");
            break;
        }

        if (QEOJSON_PERM_ALLOW == qeojson_perm) {
            perm = QEO_POLICY_ALLOW;
        }

    }while(0);
    qeojson_policy_unlock(&jsonwriter->policy_mutex);

    return perm; 

}
/* ===[ C specific callback dispatching ]==================================== */

static void call_on_update(const qeocore_reader_t                 *reader,
                           const json_reader_dispatcher_cookie_t  *dc)
{
    switch (dc->etype) {
        case QEO_ETYPE_STATE_UPDATE:
            if (NULL != dc->listener.state.on_update) {
                dc->listener.state.on_update((const qeo_json_state_reader_t *)dc->jsonreader,
                                             dc->userdata);
            }
            break;

        default:
            break;
    }
}

static void call_on_no_more_data(const qeocore_reader_t                 *reader,
                                 const json_reader_dispatcher_cookie_t  *dc)
{
    switch (dc->etype) {
        case QEO_ETYPE_EVENT_DATA:
            if (NULL != dc->listener.event.on_no_more_data) {
                dc->listener.event.on_no_more_data((const qeo_json_event_reader_t *)dc->jsonreader, dc->userdata);
            }
            break;

        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_no_more_data) {
                dc->listener.state_change.on_no_more_data((const qeo_json_state_change_reader_t *)dc->jsonreader, dc->userdata);
            }
            break;

        default:
            break;
    }
}

static void call_on_data(const qeocore_reader_t                 *reader,
                         const qeocore_data_t                   *data,
                         const json_reader_dispatcher_cookie_t  *dc)
{
    char* datastr = NULL;
    switch (dc->etype) {
        case QEO_ETYPE_EVENT_DATA:
            if (NULL != dc->listener.event.on_data) {
                datastr = json_from_data(dc->typedesc, data);
                dc->listener.event.on_data((const qeo_json_event_reader_t *)dc->jsonreader,
                                           datastr,
                                           dc->userdata);
            }
            break;

        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_data) {
                datastr = json_from_data(dc->typedesc, data);
                dc->listener.state_change.on_data((const qeo_json_state_change_reader_t *)dc->jsonreader,
                                                  datastr,
                                                  dc->userdata);
            }
            break;

        default:
            break;
    }
    free(datastr);
}

static void call_on_remove(const qeocore_reader_t                 *reader,
                           const qeocore_data_t                   *data,
                           const json_reader_dispatcher_cookie_t  *dc)
{
    char* datastr = NULL;
    switch (dc->etype) {
        case QEO_ETYPE_STATE_DATA:
            if (NULL != dc->listener.state_change.on_remove) {
                datastr = json_from_data(dc->typedesc, data);
                dc->listener.state_change.on_remove((const qeo_json_state_change_reader_t *)dc->jsonreader,
                                                    datastr, dc->userdata);
            }
            break;

        default:
            break;
    }
    free(datastr);
}
void qeojson_policy_lock(pthread_mutex_t *mutex)
{
#ifndef NDEBUG
    char  buf[64];
    int   retval = pthread_mutex_lock(mutex);
    if (retval != 0) {
        qeo_log_e("Error locking mutex: %s", strerror_r(retval, buf, sizeof(buf)));
    }
    assert(retval == 0);
#else
    pthread_mutex_lock(mutex);
#endif


}

void qeojson_policy_unlock(pthread_mutex_t *mutex)
{
#ifndef NDEBUG
    char  buf[64];
    int   retval = pthread_mutex_unlock(mutex);
    if (retval != 0) {
        qeo_log_e("Error unlocking mutex: %s", strerror_r(retval, buf, sizeof(buf)));
    }
    assert(retval == 0);
#else
    pthread_mutex_unlock(mutex);
#endif
}

qeo_retcode_t qeojson_policy_update(json_t * policy, const char *json_policy)
{
    qeo_retcode_t ret   = QEO_OK;
    json_t *src_policy = NULL;
    json_t *src_users = NULL;
    json_t *src_user = NULL;
    json_t *src_id = NULL;
    json_t *dst_users = NULL;
    json_t *dst_user = NULL;
    json_t *allow = NULL;
    json_t *dst_id = NULL;
    int i=0;
    int j=0;

    do
    {
        src_policy = json_loads(json_policy, 0, NULL);
        if(NULL == policy) {
            qeo_log_e("json_loads failed");
            break;
        }

        src_users = json_object_get(src_policy,"users");
        if (!json_is_array(src_users)) {
            ret = QEO_EFAIL;
            qeo_log_e("json type error");
            break;
        }

        dst_users = json_object_get(policy,"users");
        if (!json_is_array(dst_users)) {
            ret = QEO_EFAIL;
            qeo_log_e("json type error");
            break;
        }

        for(i=0; i < json_array_size(src_users);i++) {
            src_user = json_array_get(src_users,i);
            if (NULL == src_user) {
                ret = QEO_EFAIL;
                break;
            }

            src_id = json_object_get(src_user,"id");
            if (!json_is_integer(src_id)) {
                ret = QEO_EFAIL;
                qeo_log_e("json type error");
                break;
            }

            allow = json_object_get(src_user,"allow");
            if (!json_is_boolean(allow)) {
                ret = QEO_EFAIL;
                qeo_log_e("json type error");
                break;
            }

            for(j=0; j < json_array_size(dst_users);j++) {
                dst_user = json_array_get(dst_users,i);
                if (NULL == dst_user) {
                    ret = QEO_EFAIL;
                    break;
                }

                dst_id = json_object_get(dst_user,"id");
                if (!json_is_integer(dst_id)) {
                    ret = QEO_EFAIL;
                    qeo_log_e("json type error");
                    break;
                }

                if (json_integer_value(src_id) ==json_integer_value(dst_id) ){
                    if( 0 != json_object_set(dst_user, "id",src_id)){
                        ret = QEO_EFAIL;
                        qeo_log_e("json_object_set_failed");
                        break;

                    }

                    if( 0 != json_object_set(dst_user, "allow",allow)){
                        ret = QEO_EFAIL;
                        qeo_log_e("json_object_set_failed");
                        break;

                    }

                    break;
                }

            }
            if(ret != QEO_OK) {
                break;
            }

        }
        if (ret != QEO_OK) {
            break;
        }

    } while (0);

    if (NULL != src_policy) {
        json_decref(src_policy);
    }

    return ret;
}

void qeojson_core_callback_dispatcher(const qeocore_reader_t  *reader,
                              const qeocore_data_t    *data,
                              uintptr_t               userdata)
{
    json_reader_dispatcher_cookie_t *dc = (json_reader_dispatcher_cookie_t *)userdata;

    switch (qeocore_data_get_status(data)) {
        case QEOCORE_NOTIFY:
            call_on_update(reader, dc);
            break;

        case QEOCORE_DATA:
            call_on_data(reader, data, dc);
            break;

        case QEOCORE_NO_MORE_DATA:
            call_on_no_more_data(reader, dc);
            break;

        case QEOCORE_REMOVE:
            call_on_remove(reader, data, dc);
            break;

        case QEOCORE_ERROR:
            qeo_log_e("no callback called due to prior error");
            break;
    }
}

qeo_policy_perm_t qeojson_core_reader_policy_update_callback_dispatcher(const qeocore_reader_t      *reader,
                                                                const qeo_policy_identity_t *identity,
                                                                uintptr_t                   userdata)
{
    json_reader_dispatcher_cookie_t *dc           = (json_reader_dispatcher_cookie_t *)userdata;
    const char                      *json_policy  = NULL;
    qeo_policy_perm_t               perm          = QEO_POLICY_DENY;
    int64_t uid = 0;

    if (identity == NULL) {

        qeojson_policy_lock(&dc->jsonreader->policy_mutex);
        json_policy= json_dumps(dc->jsonreader->policy, JSON_INDENT(4));
        qeojson_policy_unlock(&dc->jsonreader->policy_mutex);

        if (QEO_OK != qeojson_policy_mv(dc->jsonreader->policy, dc->jsonreader->policy_cache)){
            qeo_log_e("qeojson_policy_mv failed");
            return QEO_POLICY_ALLOW;
        }

        switch (dc->etype) {
            case QEO_ETYPE_EVENT_DATA:
                if (NULL != dc->listener.event.on_policy_update) {
                    dc->listener.event.on_policy_update((qeo_json_event_reader_t *)dc->jsonreader, json_policy, dc->userdata);
                }
                break;

            case QEO_ETYPE_STATE_DATA:
                if (NULL != dc->listener.state_change.on_policy_update) {
                    dc->listener.state_change.on_policy_update((qeo_json_state_change_reader_t *)dc->jsonreader, json_policy,
                            dc->userdata);
                }
                break;

            case QEO_ETYPE_STATE_UPDATE:
                if (NULL != dc->listener.state.on_policy_update) {
                    dc->listener.state.on_policy_update((qeo_json_state_reader_t *)dc->jsonreader, json_policy, dc->userdata);
                }
                break;

            default:
                break;
        }

        free((void *)json_policy);
    }
    else {

        do 
        {
            uid = qeo_policy_identity_get_uid(identity);
            if (uid == 0) {
                qeo_log_e("qeo_policy_identity_get_uid failed");
                break;
            }

            perm =qeojson_reader_process_uid(dc->jsonreader,uid);

        } while(0);


    }
    qeo_log_i("uid: %" PRId64 " perm: %s",uid, (perm == QEO_POLICY_ALLOW) ? "allowed":"denied");

    return perm;
}

qeo_policy_perm_t qeojson_core_writer_policy_update_callback_dispatcher(const qeocore_writer_t      *writer,
                                                                const qeo_policy_identity_t *identity,
                                                                uintptr_t                   userdata)
{
    json_writer_dispatcher_cookie_t *dc           = (json_writer_dispatcher_cookie_t *)userdata;
    const char                      *json_policy  = NULL;
    qeo_policy_perm_t               perm          = QEO_POLICY_ALLOW;
    int64_t uid = 0;

    if (identity == NULL) {

        qeojson_policy_lock(&dc->jsonwriter->policy_mutex);
        json_policy= json_dumps(dc->jsonwriter->policy, JSON_INDENT(4));
        qeojson_policy_unlock(&dc->jsonwriter->policy_mutex);

        if (QEO_OK != qeojson_policy_mv(dc->jsonwriter->policy, dc->jsonwriter->policy_cache)){
            qeo_log_e("qeojson_policy_mv failed");
            return QEO_POLICY_ALLOW;
        }

        switch (dc->etype) {
            case QEO_ETYPE_EVENT_DATA:
                if (NULL != dc->listener.event.on_policy_update) {
                    dc->listener.event.on_policy_update((qeo_json_event_writer_t *)dc->jsonwriter, json_policy, dc->userdata);
                }
                break;

            case QEO_ETYPE_STATE_DATA:
                if (NULL != dc->listener.state.on_policy_update) {
                    dc->listener.state.on_policy_update((qeo_json_state_writer_t *)dc->jsonwriter, json_policy, dc->userdata);
                }
                break;

            default:
                break;
        }
        free((void *)json_policy);
    }
    else {

        do 
        {
            uid = qeo_policy_identity_get_uid(identity);
            if (uid == 0) {
                qeo_log_e("qeo_policy_identity_get_uid failed");
                break;
            }

            perm =qeojson_writer_process_uid(dc->jsonwriter,uid);

        } while(0);


    }

    qeo_log_i("uid: %" PRId64 " perm: %s",uid, (perm == QEO_POLICY_ALLOW) ? "allowed":"denied");

    return perm;
}
