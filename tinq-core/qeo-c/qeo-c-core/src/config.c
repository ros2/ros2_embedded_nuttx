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

#include <stddef.h>
#include <string.h>

#include "qeocore/config.h"
#include <qeo/log.h>

#include "core.h"
#include "config.h"


#define QEO_PREPEND "QEO_"
#define MAX_PARAMETER_PER_GROUP 10
#define MAX_GROUP_NAME 10

/**
 * The different configuration group identities
 */
typedef enum {
    QEOCORE_GROUP_TYPE_FWD,
    QEOCORE_GROUP_TYPE_DDS,
    QEOCORE_GROUP_TYPE_COMMON,
} group_type_t;

/**
 * The different parameter typess
 */
typedef enum {
    QEOCORE_PAR_TYPE_STRING,
    QEOCORE_PAR_TYPE_NUMBER,
} parameter_type_t;

/**
 * This structures keeps all data related to a parameter
 */
typedef struct parameter_s {
    group_type_t group;   /**< The group this parameter belongs to */
    const char *name;     /**< The name of the parameter */
    parameter_type_t type;/**< The type of the parameter */
    union {
        char *str;        /**< The string value of the parameter */
        int num;          /**< The numeric value of the parameter */
    } value;
} parameter_t;

/**
 * This structure identifies a certain group of parameters
 */
typedef struct group_s {
    const char *name;        /**< The name of the group */
    int parameter_number;    /**< The number of parameters in the group */
    parameter_t *parameters; /**< The actual parameters of the group */
} group_t;


/**
 * The forwarder group specific configuration parameters.
 */
static parameter_t fwd_parameters[] = {
    {QEOCORE_GROUP_TYPE_FWD, "LOC_SRV_MIN_TIMEOUT", QEOCORE_PAR_TYPE_NUMBER, {.num = 30000}},
    {QEOCORE_GROUP_TYPE_FWD, "LOC_SRV_MAX_TIMEOUT", QEOCORE_PAR_TYPE_NUMBER, {.num = 2147483647}},
    {QEOCORE_GROUP_TYPE_FWD, "WAIT_LOCAL_FWD", QEOCORE_PAR_TYPE_NUMBER, {.num = 2000}},
    {QEOCORE_GROUP_TYPE_FWD, "DISABLE_LOCATION_SERVICE", QEOCORE_PAR_TYPE_NUMBER, {.num = 0}},
    {QEOCORE_GROUP_TYPE_FWD, "DISABLE_FORWARDING", QEOCORE_PAR_TYPE_NUMBER, {.num = 0}},
};
#define NBR_FWD_PARAMETERS  (sizeof(fwd_parameters) / sizeof(parameter_t))

/**
 * The DDS group specific configuration parameters.
 */
static parameter_t dds_parameters[] = {
    {QEOCORE_GROUP_TYPE_DDS, "NO_SECURITY", QEOCORE_PAR_TYPE_NUMBER, {.num = 0}},
    {QEOCORE_GROUP_TYPE_DDS, "DOMAIN_ID_CLOSED", QEOCORE_PAR_TYPE_NUMBER, {.num = -1}},
    {QEOCORE_GROUP_TYPE_DDS, "DOMAIN_ID_OPEN", QEOCORE_PAR_TYPE_NUMBER, {.num = -1}},
};
#define NBR_DDS_PARAMETERS  (sizeof(dds_parameters) / sizeof(parameter_t))

/**
 * The DDS group specific configuration parameters.
 */
static parameter_t qeo_parameters[] = {
    {QEOCORE_GROUP_TYPE_COMMON, "PUB_DEVICEINFO", QEOCORE_PAR_TYPE_NUMBER, {.num = 1}},
};
#define NBR_QEO_PARAMETERS  (sizeof(qeo_parameters) / sizeof(parameter_t))

/**
 * The different groups with their configurable parameters.
 */
static group_t groups[] = {
    {"FWD", NBR_FWD_PARAMETERS, fwd_parameters},
    {"DDS", NBR_DDS_PARAMETERS, dds_parameters},
    {"CMN", NBR_QEO_PARAMETERS, qeo_parameters},
};
#define NBR_GROUPS  (sizeof(groups) / sizeof(group_t))


static parameter_t *parameter_lookup(const char *name)
{
    int i = 0, j = 0;
    group_t *group = NULL;
    parameter_t *parameter = NULL;
    int found = 0;
    char group_name[MAX_GROUP_NAME + 1];
    char *parameter_p = NULL;
    int group_len = 0;

    parameter_p = strchr(name, '_');
    if (NULL != parameter_p) {
        if ((parameter_p - name) > MAX_GROUP_NAME)
            group_len = MAX_GROUP_NAME;
        else
            group_len = parameter_p - name;
        memcpy(group_name, name, group_len);
        group_name[group_len] = '\0';
        parameter_p++;
        for (i = 0; i < NBR_GROUPS; i++) {
            group = &groups[i];
            if (!strcmp(group->name, group_name)) {
                for (j = 0; j < group->parameter_number; j++) {
                    if (!strcmp(group->parameters[j].name, parameter_p)) {
                        parameter = &group->parameters[j];
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
        }
    }
    return parameter;
}

static char *parameter_check_env(const char *name)
{
    char *env_var = calloc(1, strlen(QEO_PREPEND) + strlen(name) + 1);
    char *str = NULL;

    if (NULL != env_var) {
        sprintf(env_var, "%s%s", QEO_PREPEND, name);
        str = getenv(env_var);
        free(env_var);
    }
    return str;
}

qeo_retcode_t qeocore_parameter_set(const char *name, const char *value)
{
    qeo_retcode_t rc = QEO_EINVAL;
    parameter_t *parameter = NULL;

    VALIDATE_NON_NULL(name);
    VALIDATE_NON_NULL(value);
    parameter = parameter_lookup(name);
    if (NULL != parameter) {
        rc = QEO_OK;
        switch (parameter->type) {
            case QEOCORE_PAR_TYPE_NUMBER: {
                char *endp = NULL;

                parameter->value.num = strtol(value, &endp, 10);
                if (('\0' == *value) || ('\0' != *endp)) {
                    rc = QEO_EINVAL;
                }
                break;
            }
            case QEOCORE_PAR_TYPE_STRING:
                if (NULL != parameter->value.str) {
                    free(parameter->value.str);
                }
                parameter->value.str = strdup(value);
                break;
            default:
                qeo_log_e("unsupported parameter type %d", parameter->type);
                rc = QEO_EINVAL;
                break;
        }
    }
    else {
        qeo_log_w("Invalid parameter: %s", name);
    }

    return rc;
}

const char *qeocore_parameter_get_string(const char *name)
{
    char *str = NULL;
    parameter_t *parameter = NULL;

    if (NULL != name) {
        parameter = parameter_lookup(name);
        if (NULL != parameter) {
            if (parameter->type == QEOCORE_PAR_TYPE_STRING) {
                str = parameter_check_env(name);
                if (NULL == str) {
                    str = parameter->value.str;
                }
            }
        }
    }
    return str;
}

int qeocore_parameter_get_number(const char *name)
{
    int number = -1;
    parameter_t *parameter = NULL;

    if (NULL != name) {
        parameter = parameter_lookup(name);
        if (NULL != parameter) {
            if (parameter->type == QEOCORE_PAR_TYPE_NUMBER) {
                char *str = parameter_check_env(name);
                if (NULL == str) {
                    number = parameter->value.num;
                }
                else {
                    number = atoi(str);
                }
            }
        }
    }
    return number;
}
