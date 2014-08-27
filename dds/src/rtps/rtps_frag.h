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

/* rtps_frag.h -- Defines functions for handling fragmentation contexts. */

#ifndef __rtps_frag_h_
#define __rtps_frag_h_

#include "rtps_priv.h"

FragInfo_t *rfraginfo_create (CCREF        *refp,
			      DataFragSMsg *fragp,
			      unsigned     max_frags);

void rfraginfo_delete (CCREF *refp);

FragInfo_t *rfraginfo_update (CCREF *refp, DataFragSMsg *fragp);

void mark_fragment (FragInfo_t *fip, DataFragSMsg *fragp, Change_t *cp);

#endif /* !__rtps_frag_h_ */

