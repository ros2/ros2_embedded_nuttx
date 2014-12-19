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

#ifndef QEO_OPENSSL_ENGINE_ERROR_H_
#define QEO_OPENSSL_ENGINE_ERROR_H_

/**
 * Qeo internal return codes.
 */
typedef enum {
    QEO_OPENSSL_ENGINE_OK,                    /**< success */
    QEO_OPENSSL_ENGINE_EFAIL,                 /**< failure */
    QEO_OPENSSL_ENGINE_ENOMEM,                /**< out of memory */
    QEO_OPENSSL_ENGINE_EINVAL,                /**< invalid arguments */
    QEO_OPENSSL_ENGINE_ENODATA,               /**< no more data */
    QEO_OPENSSL_ENGINE_EBADSTATE,             /**< invalid state for operation */
    QEO_OPENSSL_ENGINE_ENOTSUPPORTED,         /**< particular functionality is not (yet) supported */
    QEO_OPENSSL_ENGINE_ENOTIMPLEMENTED,       /**< function is not implemented */
} qeo_openssl_engine_retcode_t;

#endif /* QEO_OPENSSL_ENGINE_ERROR_H_ */
