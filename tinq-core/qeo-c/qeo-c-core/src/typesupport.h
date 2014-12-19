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

#ifndef TYPESUPPORT_H_
#define TYPESUPPORT_H_

#include <dds/dds_tsm.h>
#include <dds/dds_xtypes.h>

#include <qeocore/dyntype.h>

#define TYPE_REFCNT_BITS 8
#define TYPE_REFCNT_MAX  ((1 << TYPE_REFCNT_BITS) - 1)

struct qeocore_type_s {
    const qeo_factory_t *factory;
    struct {
        unsigned refcnt : TYPE_REFCNT_BITS;
        unsigned final : 1;
        unsigned tsm_based : 1;
        unsigned registered : 1;
        unsigned keyed : 1;
    } flags;
    union {
        struct {
            const char *name;
            DDS_TypeSupport ts;
            const DDS_TypeSupport_meta *tsm;
        } tsm_based;
        struct {
            const char *name;
            DDS_DynamicTypeSupport ts;
            DDS_DynamicType dtype;
        } dynamic;
        struct {
            qeocore_typecode_t tc;
            DDS_DynamicTypeBuilder tb;
            DDS_DynamicType dtype;
        } intermediate;
    } u;
};

uint32_t calculate_member_id(const char *name);

qeo_retcode_t qeocore_type_use(qeocore_type_t *type);

#endif /* TYPESUPPORT_H_ */
