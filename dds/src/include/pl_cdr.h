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

/* pl_cdr.h -- Implements the PL-CDR marshalling/unmarshalling functions. */

#ifndef __pl_cdr_h_
#define __pl_cdr_h_

#include "dds/dds_dcps.h"
#include "db.h"
#include "typecode.h"

void pl_init (void);

/* Initialize the Parameter List functions. */

void pl_cache_reset (void);

/* Parameter List cache reset function. In case of a major change in the
   system: adding and removing of readers/writers/participants/topics, the
   cache needs to be reset. */

size_t pl_marshalled_size (const void           *data,
			   const PL_TypeSupport *ts,
			   int                  key,
			   DDS_ReturnCode_t     *error);

/* Parameter List mode marshalled data size retrieval for C/C++.
   The data argument is a pointer to unmarshalled data.  This can be either a
   handle to specific builtin data, or a C/C++-structure containing the data. 
   If key is set, only the key fields are processed and the maximum sizes are
   returned. */

DDS_ReturnCode_t pl_marshall (void                 *dest,
			      const void           *data,
			      const PL_TypeSupport *ts,
			      int                  key,
			      int                  swap);

/* Parameter List mode marshalling for C/C++. The dest argument is a pointer
   to a buffer that is large enough to contain the resulting marshalled data.
   The data argument points is a pointer to marshalled data.  This can be either
   a handle to specific builtin data, or a C/C++-structure containing the data.
   If key is set, only the key fields are taken into account, and maximum sized
   fields are used.  If swap is set, the marshalled data will have its
   endianness swapped. */

DDS_ReturnCode_t pl_unmarshall (void                 *dest,
				DBW                  *data,
				const PL_TypeSupport *ts,
				int                  swap);

/* Unmarshalling for C/C++ to host data endian mode.  The dest argument is a
   buffer that will contain the marshalled data.  The data argument is the
   original marshalled data.  The swap argument specifies whether the marshalled
   data endianness is wrong, i.e. whether it needs to be swapped. */

size_t pl_unmarshalled_size (const DBW            *data,
			     const PL_TypeSupport *ts,
			     DDS_ReturnCode_t     *error,
			     int                  swap);

/* Return the C/C++ host data size of Parameter List marshalled data.  The data
   argument is the source marshalled data.  If swap is set, the marshalled data
   is in non-native byte order. */

size_t pl_key_size (DBW                   data,
		    const PL_TypeSupport  *ts,
		    int                   swap,
		    DDS_ReturnCode_t      *error);

/* Return the fields size for Parameter List mode marshalled data.  The data
   argument is the original marshalled data.  If swap is set, the original
   marshalled data endianness is wrong, i.e. it needs to be swapped. */

DDS_ReturnCode_t pl_key_fields (void                 *dest,
				DBW                  *data,
				const PL_TypeSupport *ts,
				int                  swap);

/* Key fields extraction for Parameter List mode marshalled data.  The data
   argument is the original marshalled data.  If swap is set, the original
   marshalled data endianness is wrong, i.e. it needs to be swapped. */

#endif /* !__pl_cdr_h_ */

