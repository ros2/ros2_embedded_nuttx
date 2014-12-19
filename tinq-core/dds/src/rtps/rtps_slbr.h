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

/* rtps_slbr.h -- Defines the interface to the RTPS Best-effort Reader. */

#ifndef __rtps_slbr_h_
#define __rtps_slbr_h_

void reader_add_fragment (READER       *rp,
			  Change_t     *cp,
			  KeyHash_t    *hp,
			  DataFragSMsg *fragp);

#endif /* !__rtps_slbr_h_ */

