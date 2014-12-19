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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qeo/log.h>
#include "cnhelper.h"

/*#######################################################################
 #                       TYPES SECTION                                   #
 ########################################################################*/

#define MAX_CN_LENGTH 65 /*You can find this length inside a_strnid.c + null termination */

/*#######################################################################
 #                   STATIC FUNCTION DECLARATION                         #
 ########################################################################*/

/*#######################################################################
 #                       STATIC VARIABLE SECTION                         #
 ########################################################################*/

/*#######################################################################
 #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

/*#######################################################################
 #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/

X509_NAME* cnhelper_create_dn(const qeo_platform_device_info *info)
{
    X509_NAME* dn = NULL;

    do {
        dn = X509_NAME_new();
        if (dn) {
            char devicename[MAX_CN_LENGTH];

            strncpy(devicename, info->userFriendlyName, sizeof(devicename));
            if (devicename[sizeof(devicename)-1] != '\0'){
                strncpy(&devicename[sizeof(devicename)-4], "...", 4);
                qeo_log_w("Device name exceeds the maximal allowed length of <%d> characters, cutting it off to <%s>.", sizeof(devicename)-1, devicename);
            }
            //TODO: now only the friendly name is forwarded to the server, in the future this needs to be extended
            if (!X509_NAME_add_entry_by_NID(dn, NID_commonName, MBSTRING_ASC, (unsigned char* ) devicename, -1, -1, 0)) {
                X509_NAME_free(dn);
                dn = NULL;
                break;
            }
        }
        qeo_log_d("created DN for device");
    } while (0);

    return dn;
}
