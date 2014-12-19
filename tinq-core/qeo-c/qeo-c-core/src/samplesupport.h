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

#ifndef SAMPLE_SUPPORT_H_
#define SAMPLE_SUPPORT_H_

#include <dds/dds_xtypes.h>

#include <qeocore/api.h>

#define SS_FLAG_NONE              0
#define SS_FLAG_KEY_ONLY          (1 << 0)
#define SS_FLAG_FROM_DYNDATA      (1 << 1)

qeocore_data_t *data_alloc(DDS_DynamicType type,
                           int prep_data);

qeo_retcode_t data_get_member(const qeocore_data_t *data,
                              qeocore_member_id_t id,
                              void *value);

qeo_retcode_t data_set_member(qeocore_data_t *data,
                              qeocore_member_id_t id,
                              const void *value);

qeo_retcode_t sequence_member_accessor(qeocore_data_t *data,
                                       int index,
                                       qeocore_data_t **value,
                                       int get);

#endif /* SAMPLE_SUPPORT_H_ */
