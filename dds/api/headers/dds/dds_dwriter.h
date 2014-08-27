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

/* dds_dwriter.h -- DDS Dynamic Data Writer functions. */

#ifndef __dds_dwriter_h_
#define __dds_dwriter_h_

#include "dds/dds_xtypes.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef DDS_DataWriter DDS_DynamicDataWriter;

DDS_EXPORT DDS_InstanceHandle_t DDS_DynamicDataWriter_register_instance(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DynamicDataWriter_register_instance_w_timestamp(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance_w_timestamp(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance_directed(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance_w_timestamp_directed(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_get_key_value(
	DDS_DynamicDataWriter self,
	DDS_DynamicData key_data,
	const DDS_InstanceHandle_t h
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DynamicDataWriter_lookup_instance(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData key_data
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_write(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_write_w_timestamp(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_write_directed (
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_write_w_timestamp_directed (
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_dispose(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_dispose_w_timestamp(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_dispose_directed(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataWriter_dispose_w_timestamp_directed(
	DDS_DynamicDataWriter self,
	const DDS_DynamicData instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_dwriter_h_ */
