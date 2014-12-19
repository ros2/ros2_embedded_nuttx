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

#ifndef QEOCORE_IDENTITY_H_
#define QEOCORE_IDENTITY_H_
#include <inttypes.h>
#include <qeo/types.h>
#include <qeo/error.h>

struct qeo_identity_s {
    int64_t  realm_id;
    int64_t  device_id;
    int64_t  user_id;
    /* URL of the QEO registration server */
    char     *url;
};

qeo_retcode_t qeocore_get_identities(qeo_identity_t **identities, unsigned int *length);

qeo_retcode_t qeocore_free_identities(qeo_identity_t **identities, unsigned int length);

#endif
