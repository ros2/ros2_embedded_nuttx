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

/* sec_tagging.h -- DDS Security - Tagging plugin definitions. */

#ifndef __sec_tagging_h_
#define __sec_tagging_h_

DDS_ReturnCode_t sec_add_data_tag (const char *tag);

DDS_ReturnCode_t sec_get_data_tag (InstanceHandle_t pub_handle,
				   char             *tag);

#endif

