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

#include <stdio.h>
#include "qeo_types.h"
#include "config.h"
#include "qeo_DeviceInfo.h"
#include <qeo/device.h>
#include <qeo/util_error.h>
#include <qeo/log.h>
#include <qeocore/api.h>

#include "core.h"

qeocore_writer_t* qeo_deviceinfo_publish(qeo_factory_t *factory)
{
    qeocore_writer_t *devinfo_writer = NULL;
    qeocore_type_t *t = NULL;
    qeo_retcode_t ret = QEO_EFAIL;
    qeo_log_i("Publishing deviceinfo");

    do {
        int pubInfo = qeocore_parameter_get_number("CMN_PUB_DEVICEINFO");
        if (pubInfo == -1) {
            qeo_log_w("Unable to fetch config parameter");
            /* publish anyway, don't break out */
        }

        if (pubInfo == 0) {
            qeo_log_d("Device info publishing disabled");
            ret = QEO_OK;
            break;
        }

        qeo_log_d("Device info publishing is enabled");

        // register tsm
        t = qeocore_type_register_tsm(factory, org_qeo_system_DeviceInfo_type, org_qeo_system_DeviceInfo_type->name);
        if (t == NULL) {
            qeo_log_d("qeocore_type_register_tsm failed");
            break;
        }

        //get device info
        const qeo_platform_device_info* qeo_dev_info = qeo_platform_get_device_info();
        if (qeo_dev_info == NULL) {
            qeo_log_d("qeo_platform_get_device_info failed");
            break;
        }

        // create writer
        devinfo_writer = qeocore_writer_open(factory, t, NULL, QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, NULL,
                                             NULL);
        if (NULL == devinfo_writer) {
            qeo_log_d("qeocore_writer_open failed");
            break;
        }

        // write DeviceInfo
        ret = qeocore_writer_write(devinfo_writer, qeo_dev_info);
        if (QEO_OK != ret) {
            qeo_log_d("qeocore_writer_write failed");
            break;
        }

        ret = QEO_OK;
    } while (0);
    if (NULL != t) {
        qeocore_type_free(t); /* decr. refcnt or free altogether */
    }
    if (ret != QEO_OK) {
        qeo_log_e("Writing of device info failed\r\n");
        if (NULL != devinfo_writer) {
            qeocore_writer_close(devinfo_writer);
            devinfo_writer = NULL;
        }
    }

    return devinfo_writer;
}

void qeo_deviceinfo_destruct(qeocore_writer_t *devinfo_writer)
{
    qeocore_writer_close(devinfo_writer);
}

