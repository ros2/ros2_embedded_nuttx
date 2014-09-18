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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dds/dds_dcps.h>

#include "userTypeSupport.h"

const char* acceptance_high_end_ConfigTypeSupport_get_type_name(void)
{
	return "Config";
}

DDS_ReturnCode_t acceptance_high_end_ConfigTypeSupport_register_type (DDS_DomainParticipant domain, const char * type_name)
{
	DDS_TypeSupport ts;
	const static DDS_TypeSupport_meta ts_meta[] = {
		{.tc = CDR_TYPECODE_STRUCT, .flags = 1, .name = "Config", .size = sizeof(acceptance_high_end_Config), .nelem = 2},
		{.tc = CDR_TYPECODE_LONG, .flags = 1, .name = "key", .offset = 0},
		{.tc = CDR_TYPECODE_ARRAY, .flags = 0, .name = "data", .nelem = 3, .offset = offsetof(acceptance_high_end_Config, data)},
		{.tc = CDR_TYPECODE_SHORT}
	};

	if (!type_name)
		type_name = acceptance_high_end_ConfigTypeSupport_get_type_name();

	ts = DDS_DynamicType_register(ts_meta);
	if (!ts)
		return (DDS_RETCODE_ERROR);

	return (DDS_DomainParticipant_register_type (domain, ts, type_name));
}

const char* acceptance_high_end_StateTypeSupport_get_type_name (void)
{
	return "State";
}

DDS_ReturnCode_t acceptance_high_end_StateTypeSupport_register_type (DDS_DomainParticipant domain, const char * type_name)
{
	DDS_TypeSupport ts;
	const static DDS_TypeSupport_meta ts_meta[] = {
		{.tc = CDR_TYPECODE_STRUCT, .flags = 1, .name = "State", .size = sizeof(acceptance_high_end_State), .nelem = 2},
		{.tc = CDR_TYPECODE_LONG, .flags = 1, .name = "key", .offset = 0},
		{.tc = CDR_TYPECODE_ARRAY, .flags = 0, .name = "data", .nelem = 3, .offset = offsetof(acceptance_high_end_State, data)},
		{.tc = CDR_TYPECODE_SHORT}
	};


	if (!type_name)
		type_name = acceptance_high_end_StateTypeSupport_get_type_name();

	ts = DDS_DynamicType_register(ts_meta);
	if (!ts)
		return (DDS_RETCODE_ERROR);

	return (DDS_DomainParticipant_register_type (domain, ts, type_name));
}

const char* acceptance_high_end_StatisticTypeSupport_get_type_name (void)
{
	return "Statistic";
}

DDS_ReturnCode_t acceptance_high_end_StatisticTypeSupport_register_type(DDS_DomainParticipant domain, const char * type_name)
{
	DDS_TypeSupport ts;
	const static DDS_TypeSupport_meta ts_meta[] = {
		{.tc = CDR_TYPECODE_STRUCT, .flags = 1, .name = "Statistic", .size = sizeof(acceptance_high_end_Statistic), .nelem = 2},
		{.tc = CDR_TYPECODE_LONG, .flags = 1, .name = "key", .offset = 0},
		{.tc = CDR_TYPECODE_ARRAY, .flags = 0, .name = "data", .nelem = 3, .offset = offsetof(acceptance_high_end_Statistic, data)},
		{.tc = CDR_TYPECODE_SHORT}
	};

	if (!type_name)
		type_name = acceptance_high_end_StatisticTypeSupport_get_type_name();

	ts = DDS_DynamicType_register(ts_meta);
	if (!ts)
		return (DDS_RETCODE_ERROR);

	return (DDS_DomainParticipant_register_type (domain, ts, type_name));
}

