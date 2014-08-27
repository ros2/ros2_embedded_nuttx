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

DDS_SEQUENCE(foo *, fooSeq);

DDS_ReturnCode_t fooDataReader_read(
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_ReturnCode_t fooDataReader_take(
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_ReturnCode_t fooDataReader_read_w_condition (
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_ReturnCode_t fooDataReader_take_w_condition (
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_ReturnCode_t fooDataReader_read_next_sample (
	DDS_DataReader self,
	foo *data_value,
	DDS_SampleInfo *sample_info
);

DDS_ReturnCode_t fooDataReader_take_next_sample (
	DDS_DataReader self,
	foo *data_value,
	DDS_SampleInfo *sample_info
);

DDS_ReturnCode_t fooDataReader_read_instance (
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_ReturnCode_t fooDataReader_take_instance (
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_ReturnCode_t fooDataReader_read_next_instance (
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_ReturnCode_t fooDataReader_take_next_instance (
	DDS_DataReader r,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_ReturnCode_t fooDataReader_read_next_instance_w_condition (
	DDS_DataReader r,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_ReturnCode_t fooDataReader_take_next_instance_w_condition (
	DDS_DataReader r,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_ReturnCode_t fooDataReader_return_loan (
	DDS_DataReader self,
	fooSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);

DDS_ReturnCode_t fooDataReader_get_key_value (
	DDS_DataReader self,
	foo *data,
	DDS_InstanceHandle_t handle
);

DDS_InstanceHandle_t fooDataReader_lookup_instance (
	DDS_DataReader self,
	const foo *key_data
);
