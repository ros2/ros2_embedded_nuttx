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
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include <qeo/platform.h>
#include <qeo/device.h>
#include <qeo/log.h>
#include <qeo/util_error.h>
#include "limits.h"

#include <platform_api/platform_common.h>
/*#######################################################################
 #                                                                      #
 # TYPES and DEFINES                                                    #
 #                                                                      #
 ###################################################################### */
#ifndef LINE_MAX
#define LINE_MAX (256)
#endif
/*#######################################################################
 #                                                                      #
 #  PRIVATE DATA MEMBERS                                                #
 #                                                                      #
 ###################################################################### */

/*#######################################################################
 #                                                                      #
 #  PRIVATE FUNCTION PROTOTYPES                                         #
 #                                                                      #
 ###################################################################### */

/*#######################################################################
 #                                                                      #
 #  PRIVATE FUNCTIONS                                                   #
 #                                                                      #
 ###################################################################### */

void qeo_pprint_device_info(const qeo_platform_device_info* qeo_dev_info)
{
    printf("\n%-30s%40s\n", "Manufacturer:", qeo_dev_info->manufacturer);
    printf("%-30s%40s\n", "Model:", qeo_dev_info->modelName);
    printf("%-30s%40s\n", "Product Class:", qeo_dev_info->productClass);
    printf("%-30s%40s\n", "Hardware Version:", qeo_dev_info->hardwareVersion);
    printf("%-30s%40s\n", "Software Version:", qeo_dev_info->softwareVersion);
    printf("%-30s%40s\n", "User Friendly Name:", qeo_dev_info->userFriendlyName);
    printf("%-30s%40s\n", "Product Serial Number:", qeo_dev_info->serialNumber);
    printf("%-30s%40s\n", "Configuration URL:", qeo_dev_info->configURL);

    printf("%-30s%20" PRIu64 "%20" PRIu64"\n", "Qeo UUID:", qeo_dev_info->qeoDeviceId.upperId,
           qeo_dev_info->qeoDeviceId.lowerId);
}


const char *platform_security_state_to_string(qeo_platform_security_state state)
{

    switch (state) {
        case QEO_PLATFORM_SECURITY_UNAUTHENTICATED:
            return "Unauthenticated";
        case QEO_PLATFORM_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS:
            return "Trying to load stored Qeo-credentials";
        case QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY:
            return "Retrieving registration credentials - generating key in the mean while";
        case QEO_PLATFORM_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED:
            return "Key generated - still waiting for registration credentials";
        case QEO_PLATFORM_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE:
            return "Waiting for signed certificate";
        case QEO_PLATFORM_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS:
            return "Verifying loaded Qeo-credentials";
        case QEO_PLATFORM_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS:
            return "Verifying received Qeo-credentials";
        case QEO_PLATFORM_SECURITY_STORING_QEO_CREDENTIALS:
            return "Storing Qeo-credentials";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE:
            return "Authentication failure";
        case QEO_PLATFORM_SECURITY_AUTHENTICATED:
            return "Authentication successful";
    }

    return "?";
}

const char *platform_security_state_reason_to_string(qeo_platform_security_state_reason reason)
{

    switch (reason) {
        case QEO_PLATFORM_SECURITY_REASON_UNKNOWN:
            return "Unknown reason";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_CANCELLED:
            return "Cancelled";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT:
            return "Remote registration timeout";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_PLATFORM_FAILURE:
            return "Platform failure";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INVALID_OTP:
            return "Invalid OTC";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INTERNAL_ERROR:
            return "Internal error";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_NETWORK_FAILURE:
            return "Network failure";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_SSL_HANDSHAKE_FAILURE:
            return "SSL handshake failed";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_RECEIVED_INVALID_CREDENTIALS:
            return "Received invalid credentials";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_STORE_FAILURE:
            return "Store failure";
        case QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_UNKNOWN:
            return "Unknown failure reason";
    }

    return "?";
}

char* qeo_strdup_ret(const char* ret_str)
{
    char *ret = NULL;

    if (ret_str) {

        if ((ret = strdup(ret_str)) == NULL ) {
            qeo_log_e("Failed to duplicate string");
        }
    }

    return ret;
}

