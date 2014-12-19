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

#define _GNU_SOURCE

#ifndef DEBUG
#define NDEBUG
#endif

#include <stdlib.h>
#include <errno.h>

#include <stdio.h>
#include <limits.h>

#include <string.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pwd.h>

#include <qeo/device.h>
#include <qeo/platform_security.h>
#include <qeo/log.h>
#include <qeo/util_error.h>
#include <platform_api/platform_common.h>

#include "linux_default_device_p.h"
/*#######################################################################
 # TYPES SECTION                                                         #
 ########################################################################*/


typedef enum
{
    LINUX_MANUFACTURER,
    LINUX_MODEL_NAME,
    LINUX_PRODUCT_CLASS,
    LINUX_HW_VERSION,
    LINUX_SW_VERSION,
    LINUX_USER_FRIENDLY_NAME,
    LINUX_SERIAL_NUMBER,
    LINUX_CONFIG_URL
} qeo_dev_info_linux_prop_t;

#define UUID_GENERATOR "/proc/sys/kernel/random/uuid"     
#define LINUX_STORAGE_DIR ".qeo/"                               /*The general storage dir name*/
#define LINUX_UUID_STORAGE_FILE "uuid"                          /*UUID storage path*/
#define LINUX_CACERT_PATH "/etc/ssl/certs/"                     /*CA certificates location*/

/*#######################################################################
 # STATIC FUNCTION PROTOTYPE
 ########################################################################*/
static qeo_platform_device_id char_to_struct(const char* string);
/*#######################################################################
 # STATIC VARIABLE SECTION                                               #
 ########################################################################*/

static qeo_platform_device_info _default_device_info;
static bool _init;

static char *_default_storage_path = NULL;
static char *_default_cacert_file = NULL;
static char *_default_cacert_path = NULL;

/*#######################################################################
 # STATIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/

static char* qeo_get_linux_dev_prop(qeo_dev_info_linux_prop_t prop)
{
    char* ret = "DUMMY";
    struct utsname _uname_buf;

    if (uname(&_uname_buf) < 0) {
        qeo_log_e("Uname has failed !");
        return qeo_strdup_ret(ret);
    }

    switch (prop) {
        case LINUX_MODEL_NAME:
            ret = _uname_buf.machine;
            break;
        case LINUX_PRODUCT_CLASS:
            ret = _uname_buf.sysname;
            break;
        case LINUX_SW_VERSION:
            ret = _uname_buf.release;
            break;
        case LINUX_HW_VERSION:
            ret = _uname_buf.nodename;
            break;
        case LINUX_USER_FRIENDLY_NAME:
            ret = _uname_buf.nodename;
            break;
        case LINUX_MANUFACTURER:
            break;
        case LINUX_SERIAL_NUMBER:
            break;
        case LINUX_CONFIG_URL:
            break;
    }

    return qeo_strdup_ret(ret);
}



static char* get_qeo_dir(void)
{
    char *qeo_dir = NULL;
    const struct passwd *pwd;

    do {
        qeo_dir = qeo_strdup_ret(getenv("QEO_STORAGE_DIR"));
        if (qeo_dir != NULL){
            break;
        }
        
        pwd = getpwuid(getuid());
        if (pwd == NULL || pwd->pw_dir == NULL) {
            qeo_log_e("Failed in getting the home directory");
            break;
        }

        if (asprintf(&qeo_dir, "%s/%s", pwd->pw_dir, LINUX_STORAGE_DIR) == -1){
            qeo_log_e("No mem");
            break;
        }

    } while(0);

    qeo_log_i("Qeo directory is %s", qeo_dir);

    return (char*)qeo_dir;
}

static void init_cacert_path(void)
{
    /* initialize CA certificate file (if any) */
    _default_cacert_file = qeo_strdup_ret(getenv("QEO_CACERT_FILE"));
    if (NULL != _default_cacert_file) {
        qeo_log_i("CA certificates file is %s", _default_cacert_file);
    }
    else {
        /* initialize CA certificate path (default if none) */
        _default_cacert_path = qeo_strdup_ret(getenv("QEO_CACERT_PATH"));
        if (NULL == _default_cacert_path) {
            _default_cacert_path = qeo_strdup_ret(LINUX_CACERT_PATH);
        }
        qeo_log_i("CA certificates path is %s", _default_cacert_path);
    }
}

static qeo_util_retcode_t default_free_device_info(qeo_platform_device_info* qeo_dev_info)
{
    if (qeo_dev_info == NULL){
        return QEO_UTIL_OK; /* free()-style semantics */
    }

    free((char*)qeo_dev_info->manufacturer);
    free((char*)qeo_dev_info->modelName);
    free((char*)qeo_dev_info->productClass);
    free((char*)qeo_dev_info->hardwareVersion);
    free((char*)qeo_dev_info->softwareVersion);
    free((char*)qeo_dev_info->userFriendlyName);
    free((char*)qeo_dev_info->serialNumber);
    free((char*)qeo_dev_info->configURL);

    return QEO_UTIL_OK;

}


#ifdef __APPLE__
static char* get_uuid(const char* path)
{
    /* TODO: IMPROVE ME */
    int r = rand();
    char *ret = NULL;

    asprintf(&ret, "%d", r);
    return ret;

}

#else
static char* get_uuid(const char* path)
{
    if (path == NULL )
        return strdup("");
    FILE* file = NULL;

    char temp[128]; /* e9cabebc-923b-4932-a534-9578a6904bf8 */
    char* result = NULL;

    file = fopen(path, "rb");
    if (file == NULL ) {
        qeo_log_e("Could not open file: %s", path);
        return strdup("");
    }

    if (fgets(temp, sizeof(temp), file) == NULL){
        fclose(file);
        qeo_log_e("Could not get UUID");
        return strdup("");
    }
    fclose(file);
    result = strdup(temp);
    if (result == NULL ) {
        qeo_log_e("Could not allocate memory..");
        return strdup("");
    }

    return result;
}
#endif

/**
 * This function checks locally for an existing UUID file that was generated from before.
 * If such a file exists, it will use it to return a qeo_platform_device_id struct using a char_to_struct call.
 * Otherwise it will generate a new UUID and also return a qeo_platform_device_id struct using a char_to_struct.
 * */

static qeo_platform_device_id qeo_get_device_uuid(const char* platform_storage_filepath, const char* generator_path)
{
    FILE* file = NULL;
    char* result_uuid = NULL;

    do {
        file = fopen(platform_storage_filepath, "r");
        if (file != NULL ) {
            fclose(file);
            qeo_log_i("Fetching the existing UUID");
            result_uuid = get_uuid(platform_storage_filepath);
            break;
        }

        qeo_log_i("Creating a new UUID, because %s not found", platform_storage_filepath);

        result_uuid = get_uuid(generator_path);
        if (NULL == result_uuid) {
            qeo_log_e("Failed to get UUID");
            break;
        }

        FILE* fp = fopen(platform_storage_filepath, "w");

        if (fp != NULL ) {
            qeo_log_i("Writing the new UUID");
            fprintf(fp, "%s", result_uuid);
            fclose(fp);
        }
        else {
            qeo_log_e("Failed to open %s", platform_storage_filepath);
        }
    } while (0);

    qeo_platform_device_id res = char_to_struct(result_uuid);

    free(result_uuid);

    return res;
}

static qeo_platform_device_id char_to_struct(const char* string)
{
    char uuid_parsed[34];
    char upperId[17];
    char lowerId[17];
    uint64_t upper = 0;
    uint64_t lower = 0;

    qeo_platform_device_id qeoDeviceId;

    if (string == NULL ) {
        qeo_log_e("Called with NULL pointer argument");
        qeoDeviceId.upperId = upper;
        qeoDeviceId.lowerId = lower;
        return qeoDeviceId;
    }

    /* Remove all '-' characters */
    const char *src = string;
    char *dst = uuid_parsed;
    while ((*dst++ = *src++)) {
        if (*src == '-') {
            src++;
        }
    }

    /* Split in 2 and convert from HEX to uint64_t */
    snprintf(upperId, sizeof(upperId), "%s", uuid_parsed);
    snprintf(lowerId, sizeof(lowerId), "%s", uuid_parsed + 16);
    upper = strtoull(upperId, NULL, 16);
    lower = strtoull(lowerId, NULL, 16);

    qeoDeviceId.upperId = upper;
    qeoDeviceId.lowerId = lower;

    return qeoDeviceId;
}

/*#######################################################################
 # PUBLIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/
/**
 * This function assigns to each of the passed in qeo_platform_device_info struct constituents the respective
 * information fetched somehow, mainly with qeo_get_linux_dev_prop.
 * */
const qeo_platform_device_info *get_default_device_info(void)
{
    char* path = NULL;

    if (_init == true) {
        return &_default_device_info;
    }

    if ((asprintf(&path, "%s/%s", get_default_device_storage_path(), LINUX_UUID_STORAGE_FILE) < 0)
            || (path == NULL )) {

        qeo_log_e("Failed to allocate memory for the UUID storage path string !");

        return NULL;

    }
    _default_device_info.qeoDeviceId = qeo_get_device_uuid(path, UUID_GENERATOR);

    _default_device_info.manufacturer = qeo_get_linux_dev_prop(LINUX_MANUFACTURER);
    _default_device_info.modelName = qeo_get_linux_dev_prop(LINUX_MODEL_NAME);
    _default_device_info.productClass = qeo_get_linux_dev_prop(LINUX_PRODUCT_CLASS);
    _default_device_info.hardwareVersion = qeo_get_linux_dev_prop(LINUX_HW_VERSION);
    _default_device_info.softwareVersion = qeo_get_linux_dev_prop(LINUX_SW_VERSION);
    _default_device_info.userFriendlyName = qeo_get_linux_dev_prop(LINUX_USER_FRIENDLY_NAME);
    _default_device_info.serialNumber = qeo_get_linux_dev_prop(LINUX_SERIAL_NUMBER);
    _default_device_info.configURL = qeo_get_linux_dev_prop(LINUX_CONFIG_URL);

    _init = true;

    free(path);

    return &_default_device_info;
}

void free_default_device_info(void){

    default_free_device_info(&_default_device_info);
}

/**
 * This function returns the platform specific storage location as a path.
 * If the path does not exist, it will create one with 0700 mode.
 * The caller is responsible for freeing the string returned in *path.
 * */
const char *get_default_device_storage_path(void)
{
    if (NULL != _default_storage_path) {
        return _default_storage_path;
    }

    do {
        _default_storage_path = get_qeo_dir();
        if (_default_storage_path == NULL){
            qeo_log_e("Could not get qeo storage dir");
            break;
        }

        if (access(_default_storage_path, R_OK | W_OK) != 0) {
            qeo_log_i("Creating storage directory (%s)", strerror(errno));
            if (mkdir(_default_storage_path, 0700) != 0) {
                qeo_log_e("Failed to create storage directory %s", _default_storage_path);
                break;
            }
        }

    } while(0);

    return _default_storage_path;
}

void get_default_cacert_path(const char **ca_file,
                             const char **ca_path)
{
    init_cacert_path();
    *ca_file = _default_cacert_file;
    *ca_path = _default_cacert_path;
}

void free_default_paths(void){

    free(_default_storage_path);
    _default_storage_path = NULL;
    free(_default_cacert_file);
    _default_cacert_file = NULL;
    free(_default_cacert_path);
    _default_cacert_path = NULL;
}
