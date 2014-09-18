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

#include "dds/dds_dcps.h"
#include "db.h"
#include "typecode.h"

size_t pl_marshalled_size (const void           *data,
			   const PL_TypeSupport *ts,
			   int                  key,
			   DDS_ReturnCode_t     *error)
{
	return (0);
}

DDS_ReturnCode_t pl_marshall (void                 *dest,
			      const void           *data,
			      const PL_TypeSupport *ts,
			      int                  key,
			      int                  swap)
{
	return (0);
}

size_t pl_unmarshalled_size (const DBW            *data,
			     const PL_TypeSupport *ts,
			     DDS_ReturnCode_t     *error,
			     int                  swap)
{
	return (0);
}

DDS_ReturnCode_t pl_unmarshall (void                 *dest,
				DBW                  *data,
				const PL_TypeSupport *ts,
				int                  swap)
{
	return (0);
}

size_t pl_key_size (DBW                   data,
		    const PL_TypeSupport  *ts,
		    int                   swap,
		    DDS_ReturnCode_t      *error)
{
	return (0);
}

DDS_ReturnCode_t pl_key_fields (void                 *dest,
				DBW                  *data,
				const PL_TypeSupport *ts,
				int                  swap)
{
	return (0);
}

