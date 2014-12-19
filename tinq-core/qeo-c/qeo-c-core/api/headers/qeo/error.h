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
 * Qeo return and error codes.
 */

#ifndef QEO_ERROR_H_
#define QEO_ERROR_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Qeo return codes.
 */
typedef enum {
    QEO_OK,                    /**< success */
    QEO_EFAIL,                 /**< failure */
    QEO_ENOMEM,                /**< out of memory */
    QEO_EINVAL,                /**< invalid arguments */
    QEO_ENODATA,               /**< no more data */
    QEO_EBADSTATE,             /**< invalid state for operation */
} qeo_retcode_t;

#ifdef __cplusplus
}
#endif

#endif /* QEO_ERROR_H_ */
