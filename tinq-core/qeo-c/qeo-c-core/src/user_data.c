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
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/

#include "core.h"
#include "core_util.h"
#include "user_data.h"
#include "policy.h"
#include "dds/dds_security.h"

/*#######################################################################
#                       TYPES SECTION                                   #
########################################################################*/

typedef struct {
    struct {
        unsigned is_writer : 1;
        unsigned changed : 1;
    } flags;
    union {
        const entity_t *entity;
        const qeocore_reader_t *reader;
        const qeocore_writer_t *writer;
    } e;
    DDS_OctetSeq *user_data;
    qeo_retcode_t rc;
} fine_grained_user_data_t;

/*#######################################################################
#                   STATIC FUNCTION DECLARATION                         #
########################################################################*/

/*#######################################################################
#                       STATIC VARIABLE SECTION                         #
########################################################################*/

static unsigned _user_data_cb_set = 0;

/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/* Call the on_policy_update callbacks for the specific entity. */
static qeo_policy_perm_t call_on_update(fine_grained_user_data_t *data,
                                        qeo_policy_identity_t    *id)
{
    qeo_policy_perm_t perm = QEO_POLICY_ALLOW;

    if (data->flags.is_writer) {
        if (NULL != data->e.writer->listener.on_policy_update) {
            perm = data->e.writer->listener.on_policy_update(data->e.writer, id, data->e.writer->listener.userdata);
        }
    }
    else {
        if (NULL != data->e.reader->listener.on_policy_update) {
            perm = data->e.reader->listener.on_policy_update(data->e.reader, id, data->e.reader->listener.userdata);
        }
    }
    return perm;
}

/* Callback i called for each fine grained topic rule. */
static void get_fine_grained_rules_cb(qeo_security_policy_hndl policy,
                                      uintptr_t cookie,
                                      const char *topic_name,
                                      unsigned int selector,
                                      struct topic_participant_list_node *read_participant_list,
                                      struct topic_participant_list_node *write_participant_list)
{
    fine_grained_user_data_t *data = (fine_grained_user_data_t *)cookie;
    struct topic_participant_list_node *list = NULL;
    struct topic_participant_list_node *item = NULL;
    qeo_security_identity *own_id;
    char own_id_str[255] = "";
    char *user_data = NULL;
    size_t length = 0;

    if (QEO_OK == data->rc) {

        do {
            /* Build the userdata in the following way:
             * own_id-id1,id2,id3,
             * The list of id's behind the '-' is the list of users that have been denied access */
            if (qeo_security_get_identity(data->e.entity->factory->qeo_sec, &own_id) == QEO_OK) {
                sprintf(own_id_str, "%" PRId64 "-", own_id->user_id);
                length = strlen(own_id_str) + 1;
                user_data = calloc(length, sizeof(char));
                if (user_data == NULL) {
                    data->rc = QEO_ENOMEM;
                    break;
                }
                strcpy(user_data, own_id_str);

                if (selector == TOPIC_PARTICIPANT_SELECTOR_WRITE) {
                    list = read_participant_list;
                }
                else {
                    list = write_participant_list;
                }

                LL_FOREACH(list, item) {
                    qeo_policy_perm_t perm = QEO_POLICY_ALLOW;

                    perm = call_on_update(data, &item->id);
                    if (QEO_POLICY_DENY == perm) {
                        char id[255] = "";

                        sprintf(id, "%" PRId64 ",", item->id.user_id);
                        length = strlen(user_data) + strlen(id) + 1;
                        user_data = realloc(user_data, length);
                        if (user_data == NULL) {
                            data->rc = QEO_ENOMEM;
                            break;
                        }
                        strcat(user_data, id);
                    }
                }

                if (data->rc == QEO_OK) {
                    /* Check if user data has changed and set the new user data into the userdata sequence */
                    if ((DDS_SEQ_LENGTH(*data->user_data) > 0 && strcmp((char *)DDS_SEQ_DATA(*data->user_data), user_data)) ||
                            (DDS_SEQ_LENGTH(*data->user_data) == 0)){
                        DDS_OctetSeq__clear(data->user_data);
                        data->rc = ddsrc_to_qeorc(dds_seq_from_array(data->user_data, user_data, strlen(user_data) + 1));
                        data->flags.changed = 1;
                    }
                }
            }
        } while (0);

        qeo_security_free_identity(&own_id);
        if (user_data != NULL) {
            free(user_data);
        }
    }
}

static qeo_retcode_t update_user_data_seq(fine_grained_user_data_t *data)
{
    qeo_retcode_t rc = QEO_OK;
    unsigned int mask = (data->flags.is_writer ? TOPIC_PARTICIPANT_SELECTOR_WRITE : TOPIC_PARTICIPANT_SELECTOR_READ);

    /* insert current user data (done in get_fine_grained_rules_cb) */
    rc = qeo_security_policy_get_fine_grained_rules(data->e.entity->factory->qeo_pol, (uintptr_t)data,
                                                    DDS_Topic_get_name(data->e.entity->topic), mask,
                                                    get_fine_grained_rules_cb);
    if (QEO_OK == rc) {
        rc = data->rc;
    }

    /* signal end-of-list */
    call_on_update(data, NULL);

    return rc;
}

/* Return the own_id from the userdata and also set the remainder pointer. */
static char *user_data_get_id(char *userdata, char **remainder, char delimiter)
{
    char *id = NULL;

    id = userdata;
    *remainder = strchr(userdata, delimiter);
    if (*remainder != NULL) {
        if (strlen(*remainder) > 1) {
            **remainder = '\0';
            (*remainder)++;
        }
        else {
            **remainder = '\0';
        }
    }

    return id;
}

/* Check if id is found in userdata. If so, return true. Otherwise return false. */
static bool user_data_check_ignore(char *id, char *userdata, char delimiter)
{
    bool found = false;
    char *ignore = NULL;
    size_t length_ignore = 0;

    while (!found) {
        ignore = strchr(userdata, delimiter);
        if (ignore != NULL) {
            length_ignore = strlen(ignore);
            *ignore = '\0';
            if (!strcmp(userdata, id)) {
                found = true;
            }
            if (length_ignore > 1) {
                ignore++;
            }
            else {
                break;
            }
            userdata = ignore;
        }
        else {
            break;
        }
    }

    return found;
}

DDS_ReturnCode_t user_data_match_cb (const char *topic_name, const char *r_userdata, const char *w_userdata)
{
    char *r_copy = NULL;
    char *w_copy = NULL;
    char *r_own_id = NULL;
    char *w_own_id = NULL;
    char *r_next = NULL;
    char *w_next = NULL;
    DDS_ReturnCode_t ret = DDS_RETCODE_ACCESS_DENIED;

    qeo_log_d("Topic: %s, reader user data: %s, writer user data: %s", topic_name, r_userdata, w_userdata);

    do {
        if (r_userdata == NULL || w_userdata == NULL) {
            qeo_log_e("Invalid reader and/or writer userdata");
            break;
        }
        r_copy = strdup(r_userdata);
        w_copy = strdup(w_userdata);
        if (r_copy == NULL || w_copy == NULL) {
            qeo_log_e("copying reader and/or writer userdata failed");
            break;
        }
        r_own_id = user_data_get_id(r_copy, &r_next, '-');
        w_own_id = user_data_get_id(w_copy, &w_next, '-');

        /* check if the writers own_id is found in the readers userdata.
         * access will be denied if it is found. */
        if (w_own_id != NULL && r_next != NULL) {
            if (user_data_check_ignore(w_own_id, r_next, ',')) {
                qeo_log_d("Access denied");
                break;
            }
        }
        /* check if the readers own_id is found in the writers userdata.
         * access will be denied if it is found. */
        if (r_own_id != NULL && w_next != NULL) {
            if (user_data_check_ignore(r_own_id, w_next, ',')) {
                qeo_log_d("Access denied");
                break;
            }
        }
        qeo_log_d("Access granted");
        ret = DDS_RETCODE_OK;
    } while (0);

    if (r_copy) {
        free(r_copy);
    }
    if (w_copy) {
        free(w_copy);
    }
    return ret;
}

static void user_data_set_cb()
{
    if (_user_data_cb_set) {
        return;
    }

    DDS_SP_set_userdata_match_cb(user_data_match_cb);
    _user_data_cb_set = 1;
}

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

qeo_retcode_t reader_user_data_update(const qeocore_reader_t *reader)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;
    qeo_retcode_t rc = QEO_OK;
    DDS_DataReaderQos qos;

    user_data_set_cb();

    ddsrc = DDS_DataReader_get_qos(reader->dr, &qos);
    qeo_log_dds_rc("DDS_Subscriber_get_qos", ddsrc);
    rc = ddsrc_to_qeorc(ddsrc);
    if (QEO_OK == rc) {
        fine_grained_user_data_t data;

        data.flags.is_writer = 0;
        data.flags.changed = 0;
        data.e.reader = reader;
        data.user_data = &qos.user_data.value;
        data.rc = QEO_OK;
        rc = update_user_data_seq(&data);
        if (QEO_OK == rc && data.flags.changed) {
            ddsrc = DDS_DataReader_set_qos(reader->dr, &qos);
            dds_seq_cleanup(data.user_data);
            qeo_log_dds_rc("DDS_DataReader_set_qos", ddsrc);
            rc = ddsrc_to_qeorc(ddsrc);
        }
    }
    return rc;
}

qeo_retcode_t writer_user_data_update(const qeocore_writer_t *writer)
{
    DDS_ReturnCode_t ddsrc = DDS_RETCODE_OK;
    qeo_retcode_t rc = QEO_OK;
    DDS_DataWriterQos qos;

    user_data_set_cb();

    ddsrc = DDS_DataWriter_get_qos(writer->dw, &qos);
    qeo_log_dds_rc("DDS_DataWriter_get_qos", ddsrc);
    rc = ddsrc_to_qeorc(ddsrc);
    if (QEO_OK == rc) {
        fine_grained_user_data_t data;

        data.flags.is_writer = 1;
        data.flags.changed = 0;
        data.e.writer = writer;
        data.user_data = &qos.user_data.value;
        data.rc = QEO_OK;
        rc = update_user_data_seq(&data);
        if (QEO_OK == rc && data.flags.changed) {
            ddsrc = DDS_DataWriter_set_qos(writer->dw, &qos);
            dds_seq_cleanup(data.user_data);
            qeo_log_dds_rc("DDS_DataWriter_set_qos", ddsrc);
            rc = ddsrc_to_qeorc(ddsrc);
        }
    }
    return rc;
}

