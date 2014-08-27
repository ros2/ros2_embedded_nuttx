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

/* dcps_waitset.h -- DCPS Waitset functions. */

#ifndef __dcps_waitset_h_
#define __dcps_waitset_h_

void dcps_waitset_wakeup (void *ep, Condition_t *cp, lock_t *no_lock);

#endif /* !__dcps_waitset_h_ */

