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

#include <qeo/api.h>
#include <qeocore/dyntype.h>

qeo_retcode_t qeo_enum_value_to_string(const DDS_TypeSupport_meta *enum_type,
                                       qeo_enum_value_t value,
                                       char *name,
                                       size_t sz)
{
    return qeocore_enum_value_to_string(enum_type, NULL, value, name, sz);
}

qeo_retcode_t qeo_enum_string_to_value(const DDS_TypeSupport_meta *enum_type,
                                       const char *name,
                                       qeo_enum_value_t *value)
{
    return qeocore_enum_string_to_value(enum_type, NULL, name, value);
}

