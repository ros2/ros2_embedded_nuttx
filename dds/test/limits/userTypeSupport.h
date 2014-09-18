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

#ifndef _USER_TYPESUPPORT_H
#define _USER_TYPESUPPORT_H

#include <dds/dds_dcps.h>

typedef struct {
	int key;
	int data[3];
} acceptance_high_end_Config;

DDS_SEQUENCE(acceptance_high_end_Config *, acceptance_high_end_ConfigPtrSeq);

DDS_ReturnCode_t acceptance_high_end_ConfigTypeSupport_register_type(DDS_DomainParticipant domain, const char *type_name);
const char *acceptance_high_end_ConfigTypeSupport_get_type_name(void);

typedef struct {
	int key;
	int data[3];
} acceptance_high_end_State;

DDS_SEQUENCE(acceptance_high_end_State *, acceptance_high_end_StatePtrSeq);

DDS_ReturnCode_t acceptance_high_end_StateTypeSupport_register_type(DDS_DomainParticipant domain, const char *type_name);
const char *acceptance_high_end_StateTypeSupport_get_type_name(void);

typedef struct {
	int key;
	int data[3];
} acceptance_high_end_Statistic;

DDS_SEQUENCE(acceptance_high_end_Statistic *, acceptance_high_end_StatisticPtrSeq);

DDS_ReturnCode_t acceptance_high_end_StatisticTypeSupport_register_type(DDS_DomainParticipant domain, const char *type_name);
const char *acceptance_high_end_StatisticTypeSupport_get_type_name(void);

#endif

