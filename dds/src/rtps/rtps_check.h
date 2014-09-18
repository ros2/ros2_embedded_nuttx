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

/* rtps_check.h -- Some consistency checking on RTPS internal data. */

#include "rtps_cfg.h"

#ifdef RTPS_CACHE_CHECK

void rtps_proxy_check (ENDPOINT *ep);

void rtps_cache_check (ENDPOINT *ep, const char *name);

#define	CACHE_CHECK(ep,s)	rtps_cache_check(ep, s)
#else
#define	CACHE_CHECK(ep,s)
#endif

#ifdef RTPS_RANGE_CHECK

void rtps_range_check (CCLIST *lp, const char *id);

#define	RANGE_CHECK(lp, s)	rtps_range_check (lp, s)
#else
#define	RANGE_CHECK(list, s)
#endif



