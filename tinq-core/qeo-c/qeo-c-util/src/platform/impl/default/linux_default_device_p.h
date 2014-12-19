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

#ifndef LINUX_DEFAULT_DEVICE_P_H_
#define LINUX_DEFAULT_DEVICE_P_H_
/*#######################################################################
# HEADER (INCLUDE) SECTION                                               #
########################################################################*/
#include <qeo/device.h>

/*#######################################################################
 # TYPES SECTION                                                         #
 ########################################################################*/
/*########################################################################
#                       API FUNCTION SECTION                             #
########################################################################*/
const qeo_platform_device_info *get_default_device_info(void);
void free_default_device_info(void);
const char *get_default_device_storage_path(void);
void get_default_cacert_path(const char **ca_file, const char **ca_path);
void free_default_paths(void);
#endif
