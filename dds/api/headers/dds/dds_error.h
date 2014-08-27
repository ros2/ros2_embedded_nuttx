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

/* dds_error.h -- Defines the error codes as used in the DDS subsystem. */

#ifndef __dds_error_h_
#define __dds_error_h_

#ifdef _WIN32
#ifdef DDS_LIBRARY
#define DDS_EXPORT	__declspec(dllexport)
#else
#define DDS_EXPORT	__declspec(dllimport)
#endif
#else
#define DDS_EXPORT
#define	HANDLE		int
#endif

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
	DDS_RETCODE_OK,
	DDS_RETCODE_ERROR,
	DDS_RETCODE_UNSUPPORTED,
	DDS_RETCODE_BAD_PARAMETER,
	DDS_RETCODE_PRECONDITION_NOT_MET,
	DDS_RETCODE_OUT_OF_RESOURCES,
	DDS_RETCODE_NOT_ENABLED,
	DDS_RETCODE_IMMUTABLE_POLICY,
	DDS_RETCODE_INCONSISTENT_POLICY,
	DDS_RETCODE_ALREADY_DELETED,
	DDS_RETCODE_TIMEOUT,
	DDS_RETCODE_NO_DATA,
	DDS_RETCODE_ILLEGAL_OPERATION,
	DDS_RETCODE_NOT_ALLOWED_BY_SEC
} DDS_ReturnCode_t;

/* For backwards compatibility: */
#define	DDS_RETCODE_ACCESS_DENIED DDS_RETCODE_NOT_ALLOWED_BY_SEC

/* Return a readable error string from a DDS error code. */
DDS_EXPORT const char *DDS_error (DDS_ReturnCode_t e);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_error_h_ */
