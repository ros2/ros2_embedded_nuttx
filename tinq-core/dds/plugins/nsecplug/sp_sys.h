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

/* sp_sys.h -- System specific library init. */

#ifndef __sp_sys_h_
#define __sp_sys_h_

#ifdef DDS_DEBUG
#include <openssl/x509v3.h>
#include "log.h"
#endif

void sp_sys_set_library_init (int val);
void sp_sys_set_library_lock (void);
void sp_sys_unset_library_lock (void);

#ifdef DDS_DEBUG
void sp_log_X509(X509* cert);
#endif

#endif
