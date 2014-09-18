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

/* sec_qeo_policy.h - define functions to get and set policy file and version */

#ifndef __sec_qeo_policy_h_
#define __sec_qeo_policy_h_


#include "error.h"
#include "log.h"

void set_policy_file (unsigned char *policy_file,
		      size_t        length);

unsigned char *get_policy_file (size_t *length,
				DDS_ReturnCode_t *error);

uint64_t get_policy_version (DDS_ReturnCode_t *error);

#endif
