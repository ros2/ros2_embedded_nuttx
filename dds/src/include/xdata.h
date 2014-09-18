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

/* xdata.h -- Defines some functions for dynamic data handling. */

#ifndef __xdata_h_
#define __xdata_h_

#include "type_data.h"

int xd_pool_init (const POOL_LIMITS *dtypes, const POOL_LIMITS *ddata);

/* Initialize the Dynamic Data pools. */

void xd_pool_final (void);

/* Flush the Dynamic Data pools. */

void xd_pool_dump (size_t sizes []);

/* Dump the pools statistics. */

DynType_t *xd_dyn_type_alloc (void);

/* Create a new Dynamic Type reference. */

void xd_dyn_type_free (DynType_t *tp);

/* Free a previously allocated Dynamic Type reference. */

DynDataRef_t *xd_dyn_dref_alloc (void);

/* Allocate a new Dynamic Data reference. */

void xd_dyn_dref_free (DynDataRef_t *rp);

/* Free a previously allocated Dynamic Data reference. */

DynData_t *xd_dyn_data_alloc (const Type *tp, size_t size);

/* Allocate a type-specific Dynamic Data container with the given number of
   data bytes for data storage.  On return, all fields will be properly
   initialized and dp will point to the contained data region, with dsize and
   dleft indicating the data size and the number of free data bytes
   respectively. */

DynData_t *xd_dyn_data_grow (DynData_t *ddp, size_t nsize);

/* Increase the contained data size of a Dynamic Data container. */

DDS_ReturnCode_t xd_delete_data (DynData_t *ddp);

/* Delete the data contained in a Dynamic Data reference. */

void xd_dump (unsigned indent, const DynData_t *ddp);

/* Debug: dump a Dynamic Data structure and all its constituents. */

#endif /* !__xdata_h_ */

