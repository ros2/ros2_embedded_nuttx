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

/* dds_builtin.h -- Preregistered builtin types for simple use-cases. */

#ifndef __dds_builtin_h_
#define __dds_builtin_h_

#include "dds/dds_xtypes.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* DDS_String type.
   ----------------
  
   This key-less type can be used to send simple unbounded strings between
   peers. */

typedef struct dds_string_st {
	char	*value;
} DDS_String;

/* 1. DDS_StringTypeSupport. */

DDS_EXPORT DDS_ReturnCode_t DDS_StringTypeSupport_register_type (
	DDS_DomainParticipant part,
	const char *name
);

DDS_EXPORT const char *DDS_StringTypeSupport_get_type_name (void);


/* 2. DDS_StringDataWriter. */

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataWriter_write(
	DDS_DataWriter self,
	const char *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataWriter_write_w_timestamp(
	DDS_DataWriter self,
	const char *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataWriter_write_directed (
	DDS_DataWriter self,
	const char *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataWriter_write_w_timestamp_directed (
	DDS_DataWriter self,
	const char *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);


/* 3. DDS_StringDataReader. */

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_read(
	DDS_DataReader self,
	DDS_StringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_take(
	DDS_DataReader self,
	DDS_StringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_read_w_condition (
	DDS_DataReader self,
	DDS_StringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_take_w_condition (
	DDS_DataReader self,
	DDS_StringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_read_next_sample (
	DDS_DataReader self,
	char *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_take_next_sample (
	DDS_DataReader self,
	char *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_StringDataReader_return_loan (
	DDS_DataReader self,
	DDS_StringSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);


/* DDS_KeyedString type.
   ---------------------
 
   The DDS_KeyedString type consists of two unbounded strings where the first
   is a key field and the second is the associated value. */

typedef struct dds_keyed_string_st {
	char	*key;		/* //@Key */
	char	*value;
} DDS_KeyedString;

DDS_SEQUENCE (DDS_KeyedString, DDS_KeyedStringSeq);


/* 1. DDS_KeyedStringTypeSupport. */

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringTypeSupport_register_type (
	DDS_DomainParticipant part,
	const char *name
);

DDS_EXPORT const char *DDS_KeyedStringTypeSupport_get_type_name (void);


/* 2. DDS_KeyedStringDataWriter. */

/* 2a. Extra functions. */

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance_w_key(
	DDS_DataWriter self,
	const char *key
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_unregister_instance_w_key(
	DDS_DataWriter self,
	const char *key
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_unregister_instance_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_string_w_key(
	DDS_DataWriter self,
	const char *key,
	const char *str,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_string_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const char *str,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_key(
	DDS_DataWriter self,
	const char *key
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_get_key_value_w_key(
	DDS_DataWriter self,
	char *key_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_lookup_instance_w_key(
	DDS_DataWriter self,
	const char *key
);


/* 2b. Standard functions. */

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance_directed(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance_w_timestamp_directed(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_get_key_value(
	DDS_DataWriter self,
	DDS_KeyedString *key_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataWriter_lookup_instance(
	DDS_DataWriter self,
	const DDS_KeyedString *key_data
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_write(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_directed (
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_w_timestamp_directed (
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_directed(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_timestamp_directed(
	DDS_DataWriter self,
	const DDS_KeyedString *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);


/* 3. DDS_KeyedStringDataReader. */

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_read(
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_take(
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_read_w_condition (
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_take_w_condition (
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_read_next_sample (
	DDS_DataReader self,
	DDS_KeyedString *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_take_next_sample (
	DDS_DataReader self,
	DDS_KeyedString *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_read_instance (
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_take_instance (
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_read_next_instance (
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_take_next_instance (
	DDS_DataReader r,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_read_next_instance_w_condition (
	DDS_DataReader r,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_take_next_instance_w_condition (
	DDS_DataReader r,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_return_loan (
	DDS_DataReader self,
	DDS_KeyedStringSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedStringDataReader_get_key_value (
	DDS_DataReader self,
	DDS_KeyedString *data,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedStringDataReader_lookup_instance (
	DDS_DataReader self,
	const DDS_KeyedString *key_data
);


/* DDS_Octets type.
   ---------------
 
   The DDS_Octets type is a key-less unbounded sequence of bytes. */

typedef struct dds_bytes_st {
	DDS_ByteSeq	value;
} DDS_Bytes;

DDS_SEQUENCE (DDS_Bytes, DDS_BytesSeq);


/* 1. DDS_BytesTypeSupport. */

DDS_EXPORT DDS_ReturnCode_t DDS_BytesTypeSupport_register_type (
	DDS_DomainParticipant part,
	const char *name
);

DDS_EXPORT const char *DDS_BytesTypeSupport_get_type_name (void);


/* 2. DDS_BytesDataWriter. */

/* 2a. Extra functions. */

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataWriter_write_w_bytes(
	DDS_DataWriter self,
	const unsigned char *bytes,
	int offset,
	int length,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataWriter_write_w_bytes_w_timestamp(
	DDS_DataWriter self,
	const unsigned char *bytes,
	int offset,
	int length,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);


/* 2b. Standard functions. */

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataWriter_write(
	DDS_DataWriter self,
	const DDS_Bytes *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataWriter_write_w_timestamp(
	DDS_DataWriter self,
	const DDS_Bytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataWriter_write_directed (
	DDS_DataWriter self,
	const DDS_Bytes *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataWriter_write_w_timestamp_directed (
	DDS_DataWriter self,
	const DDS_Bytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);


/* 3. DDS_BytesDataReader. */

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_read(
	DDS_DataReader self,
	DDS_BytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_take(
	DDS_DataReader self,
	DDS_BytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_read_w_condition (
	DDS_DataReader self,
	DDS_BytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_take_w_condition (
	DDS_DataReader self,
	DDS_BytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_read_next_sample (
	DDS_DataReader self,
	DDS_Bytes *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_take_next_sample (
	DDS_DataReader self,
	DDS_Bytes *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_BytesDataReader_return_loan (
	DDS_DataReader self,
	DDS_BytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);


/* DDS_KeyedBytes type.
   ---------------------
 
   The DDS_KeyedBytes type is a keyed unbounded sequence of bytes. */

typedef struct dds_keyed_bytes_st {
	char		*key;		/* //@Key */
	DDS_ByteSeq	value;
} DDS_KeyedBytes;

DDS_SEQUENCE (DDS_KeyedBytes, DDS_KeyedBytesSeq);


/* 1. DDS_KeyedBytesTypeSupport. */

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesTypeSupport_register_type (
	DDS_DomainParticipant part,
	const char *name
);

DDS_EXPORT const char *DDS_KeyedBytesTypeSupport_get_type_name (void);


/* 2. DDS_KeyedBytesDataWriter. */

/* 2a. Extra functions. */

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance_w_key(
	DDS_DataWriter self,
	const char *key
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_unregister_instance_w_key(
	DDS_DataWriter self,
	const char *key
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_unregister_instance_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_bytes_w_key(
	DDS_DataWriter self,
	const char *key,
	const unsigned char *bytes,
	int offset,
	int length,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_bytes_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const unsigned char *bytes,
	int offset,
	int length,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_key(
	DDS_DataWriter self,
	const char *key
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_key_w_timestamp(
	DDS_DataWriter self,
	const char *key,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_get_key_value_w_key(
	DDS_DataWriter self,
	char *key_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_lookup_instance_w_key(
	DDS_DataWriter self,
	const char *key
);


/* 2b. Standard functions. */

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance_directed(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance_w_timestamp_directed(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_get_key_value(
	DDS_DataWriter self,
	DDS_KeyedBytes *key_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_lookup_instance(
	DDS_DataWriter self,
	const DDS_KeyedBytes *key_data
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_directed (
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_w_timestamp_directed (
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_timestamp(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_directed(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_timestamp_directed(
	DDS_DataWriter self,
	const DDS_KeyedBytes *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);


/* 3. DDS_KeyedBytesDataReader. */

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_read(
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_take(
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_w_condition (
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_w_condition (
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_next_sample (
	DDS_DataReader self,
	DDS_KeyedBytes *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_next_sample (
	DDS_DataReader self,
	DDS_KeyedBytes *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_instance (
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_instance (
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_next_instance (
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_next_instance (
	DDS_DataReader r,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_next_instance_w_condition (
	DDS_DataReader r,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_next_instance_w_condition (
	DDS_DataReader r,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_ReadCondition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_return_loan (
	DDS_DataReader self,
	DDS_KeyedBytesSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);

DDS_EXPORT DDS_ReturnCode_t DDS_KeyedBytesDataReader_get_key_value (
	DDS_DataReader self,
	DDS_KeyedBytes *data,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_KeyedBytesDataReader_lookup_instance (
	DDS_DataReader self,
	const DDS_KeyedBytes *key_data
);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_builtin_h_ */

