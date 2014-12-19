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

#include <assert.h>
#include <string.h>

#include <qeo/error.h>
#include "core_util.h"

/* ===[ return code mapping DDS -> Qeo ]===================================== */

/**
 * Map DDS return codes to Qeo return codes.
 */
qeo_retcode_t ddsrc_to_qeorc(DDS_ReturnCode_t ddsrc)
{
    switch(ddsrc) {
        case DDS_RETCODE_OK:
            return QEO_OK;
        case DDS_RETCODE_ERROR:
        case DDS_RETCODE_UNSUPPORTED:
        case DDS_RETCODE_IMMUTABLE_POLICY:
        case DDS_RETCODE_INCONSISTENT_POLICY:
        case DDS_RETCODE_TIMEOUT:
        case DDS_RETCODE_ILLEGAL_OPERATION:
        case DDS_RETCODE_ACCESS_DENIED:
            return QEO_EFAIL;
        case DDS_RETCODE_BAD_PARAMETER:
            return QEO_EINVAL;
        case DDS_RETCODE_PRECONDITION_NOT_MET:
        case DDS_RETCODE_NOT_ENABLED:
        case DDS_RETCODE_ALREADY_DELETED:
            return QEO_EBADSTATE;
        case DDS_RETCODE_OUT_OF_RESOURCES:
            return QEO_ENOMEM;
        case DDS_RETCODE_NO_DATA:
            return QEO_ENODATA;
    }

    return QEO_EFAIL;
}

void lock(pthread_mutex_t *mutex)
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

void unlock(pthread_mutex_t *mutex)
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
