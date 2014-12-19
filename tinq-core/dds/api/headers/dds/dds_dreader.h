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

/* dds_dreader.h -- Defines the Dynamic Data Reader interface. */

#ifndef __dds_dreader_h_
#define __dds_dreader_h_

#include "dds/dds_xtypes.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef DDS_DataReader DDS_DynamicDataReader;

DDS_SEQUENCE (DDS_DynamicData, DDS_DynamicDataSeq);

DDS_EXPORT DDS_DynamicDataSeq *DDS_DynamicDataSeq__alloc (void);
DDS_EXPORT void DDS_DynamicDataSeq__free (DDS_DynamicDataSeq *dyndata);
DDS_EXPORT void DDS_DynamicDataSeq__init (DDS_DynamicDataSeq *dyndata);
DDS_EXPORT void DDS_DynamicDataSeq__clear (DDS_DynamicDataSeq *dyndata);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_read(
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_take(
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_read_w_condition (
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_take_w_condition (
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_read_next_sample (
	DDS_DynamicDataReader self,
	DDS_DynamicData *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_take_next_sample (
	DDS_DynamicDataReader self,
	DDS_DynamicData *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_read_instance (
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_take_instance (
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_read_next_instance (
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_take_next_instance (
	DDS_DynamicDataReader r,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_read_next_instance_w_condition (
	DDS_DynamicDataReader r,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_take_next_instance_w_condition (
	DDS_DynamicDataReader r,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_return_loan (
	DDS_DynamicDataReader self,
	DDS_DynamicDataSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataReader_get_key_value (
	DDS_DynamicDataReader self,
	DDS_DynamicData data,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DynamicDataReader_lookup_instance (
	DDS_DynamicDataReader self,
	const DDS_DynamicData key_data
);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_dreader_h_ */

