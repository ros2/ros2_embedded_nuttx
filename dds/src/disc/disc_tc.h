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

/* disc_tc.h -- Interface to Typecode functions as used in Discovery. */

#ifndef __disc_tc_h_
#define __disc_tc_h_

#include "dds_data.h"

TypeSupport_t *tc_typesupport (unsigned char *tc, const char *name);

/* When a new type is discovered, this function must be used to create the
   typesupport in order to have a real type. */

unsigned char *tc_unique (Topic_t       *tp,
			  Endpoint_t    *ep, 
			  unsigned char *tc,
			  int           *incompatible);

/* Return unique typecode data by comparing the proposed data with the data of
   other topic endpoints.  If alternative typecode data is found, the proposed
   data is released and the existing data is reused. */

int tc_update (Endpoint_t    *ep,
	       unsigned char **ep_tc,
	       unsigned char **new_tc,
	       int           *incompatible);

/* Attempts to update the Typecode of a Discovered Reader. */

#endif /* !__disc_tc_h_ */

