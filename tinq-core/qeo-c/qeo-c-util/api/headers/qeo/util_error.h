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

/** \file
 * Qeo internal return and error codes.
 */

#ifndef QEO_UTIL_ERROR_H_
#define QEO_UTIL_ERROR_H_

/**
 * Qeo internal return codes.
 */
typedef enum {
    QEO_UTIL_OK,                    /**< success */
    QEO_UTIL_EFAIL,                 /**< failure */
    QEO_UTIL_ENOMEM,                /**< out of memory */
    QEO_UTIL_EINVAL,                /**< invalid arguments */
    QEO_UTIL_ENODATA,               /**< no more data */
    QEO_UTIL_EBADSTATE,             /**< invalid state for operation */
    QEO_UTIL_ENOTSUPPORTED,         /**< particular functionality is not (yet) supported */
    QEO_UTIL_ENOTIMPLEMENTED,       /**< function is not implemented */
} qeo_util_retcode_t;

#endif /* QEO_UTIL_ERROR_H_ */
