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

/* secplug.h -- Security plugin for Authentication and Access Control. */

#ifndef __secplug_h_
#define __secplug_h_

DDS_ReturnCode_t sp_set_policy (void);

/* Set Authentication, Access Control and Auxiliary security policies to this
   plugin set. */

DDS_ReturnCode_t sp_set_crypto (void);

/* Set Crypto security functions to this plugin. */

#endif /* !__secplug_h_ */

