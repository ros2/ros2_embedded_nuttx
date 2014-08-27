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

DDS_InstanceHandle_t fooDataWriter_register_instance(
	DDS_DataWriter self,
	const foo *instance_data
);

DDS_InstanceHandle_t fooDataWriter_register_instance_w_timestamp(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_Time_t *timestamp
);

DDS_ReturnCode_t fooDataWriter_unregister_instance(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_ReturnCode_t fooDataWriter_unregister_instance_w_timestamp(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_ReturnCode_t fooDataWriter_unregister_instance_directed(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_ReturnCode_t fooDataWriter_unregister_instance_w_timestamp_directed(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
	DDS_InstanceHandleSeq *destinations
);

DDS_ReturnCode_t fooDataWriter_get_key_value(
	DDS_DataWriter self,
	foo *key_data,
	const DDS_InstanceHandle_t handle
);

DDS_InstanceHandle_t fooDataWriter_lookup_instance(
	DDS_DataWriter self,
	const foo *key_data
);

DDS_ReturnCode_t fooDataWriter_write(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_ReturnCode_t fooDataWriter_write_w_timestamp(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_ReturnCode_t fooDataWriter_write_directed (
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_ReturnCode_t fooDataWriter_write_w_timestamp_directed (
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_ReturnCode_t fooDataWriter_dispose(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_ReturnCode_t fooDataWriter_dispose_w_timestamp(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_ReturnCode_t fooDataWriter_dispose_directed(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_ReturnCode_t fooDataWriter_dispose_w_timestamp_directed(
	DDS_DataWriter self,
	const foo *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);
