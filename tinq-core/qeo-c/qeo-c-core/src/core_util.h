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

#ifndef CORE_UTIL_H_
#define CORE_UTIL_H_

#include <pthread.h>

#include <qeo/error.h>
#include <qeo/log.h>
#include <dds/dds_error.h>

qeo_retcode_t ddsrc_to_qeorc(DDS_ReturnCode_t ddsrc);

#if DEBUG == 1

#define qeo_log_dds_rc(ddsfn, ddsrc) if (DDS_RETCODE_OK != ddsrc) \
    qeo_log_i("%s failed with return code %d", ddsfn, ddsrc)

#else /* !DEBUG */

#define qeo_log_dds_rc(ddsfn, ddsrc)

#endif

#define qeo_log_dds_null(ddsfn, ptr) if (NULL == ptr) qeo_log_e("%s returned NULL", ddsfn)

void lock(pthread_mutex_t *mutex);
void unlock(pthread_mutex_t *mutex);

#endif /* CORE_UTIL_H_ */
