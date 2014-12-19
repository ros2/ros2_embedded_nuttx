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

#ifndef TEST_DYN_TYPES_H_
#define TEST_DYN_TYPES_H_

#include <stdint.h>
#include <stddef.h>

#include <qeocore/api.h>

/* dynamic version of TSM defined in tsm_types */
typedef enum {
    M_STRING, M_OTHER, M_I8, M_I16, M_I32, M_I64, M_F32, M_BOOL, M_A8, M_E, M_AE, M_INNER_I32, M_INNER
} types_members_t;
extern qeocore_member_id_t _member_id[];
qeocore_type_t *types_get(const DDS_TypeSupport_meta *tsm);

extern const char *_TC2STR[];

extern qeocore_member_id_t _id_id;
extern qeocore_member_id_t _inner_id;
extern qeocore_member_id_t _num_id;
extern qeocore_member_id_t _sequence_id;

qeocore_type_t *nested_type_get(int id_is_keyed,
                            int inner_is_keyed,
                            int num_is_keyed);

qeocore_type_t *seq_type_get(qeo_factory_t *factory,
                         qeocore_typecode_t typecode,
                         int is_keyed,
                         int reg_struct);

#endif /* TEST_TYPES_H_ */
