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

/* builtin.c -- Extra builtin types: String, KeyedString, Octets and KeyedOctets
		that allow easy communication without having to create your own
		types. */

#include <string.h>
#include "dds/dds_builtin.h"
#include "dds/dds_tsm.h"

int dds_builtins_used;

#ifdef DDS_BUILTINS

/* DDS_String type.
   ----------------
  
   This key-less type can be used to send simple unbounded strings between
   peers. */

static DDS_TypeSupport_meta string_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 2, "DDS_String", sizeof (struct dds_string_st), 0, 1, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 2, "value", 0, offsetof(struct dds_string_st, value), 0, 0, NULL }
};

static DDS_TypeSupport	string_ts;

static int string_builtin_init (void)
{
	string_ts = DDS_DynamicType_register (string_tsm);
	if (!string_ts)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}


/* 1. DDS_StringTypeSupport. */

DDS_ReturnCode_t DDS_StringTypeSupport_register_type (DDS_DomainParticipant p,
						      const char *name)
{
	if (!string_ts)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (DDS_DomainParticipant_register_type (p, string_ts, name));
}

const char *DDS_StringTypeSupport_get_type_name (void)
{
	return (DDS_TypeSupport_get_type_name (string_ts));
}


/* 2. DDS_StringDataWriter. */

DDS_ReturnCode_t DDS_StringDataWriter_write (DDS_DataWriter dw,
					     const char *data,
					     const DDS_InstanceHandle_t h)
{
	DDS_String	s;

	s.value = (char *) data;
	return (DDS_DataWriter_write (dw, &s, h));
}

DDS_ReturnCode_t DDS_StringDataWriter_write_w_timestamp (DDS_DataWriter dw,
							 const char *data,
							 const DDS_InstanceHandle_t h,
							 const DDS_Time_t *time)
{
	DDS_String	s;

	s.value = (char *) data;
	return (DDS_DataWriter_write_w_timestamp (dw, &s, h, time));
}

DDS_ReturnCode_t DDS_StringDataWriter_write_directed (DDS_DataWriter dw,
						      const char *data,
						      const DDS_InstanceHandle_t h,
						      DDS_InstanceHandleSeq *dests)
{
	DDS_String	s;

	s.value = (char *) data;
	return (DDS_DataWriter_write_directed (dw, &s, h, dests));
}

DDS_ReturnCode_t DDS_StringDataWriter_write_w_timestamp_directed (
						DDS_DataWriter dw,
						const char *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	DDS_String	s;

	s.value = (char *) data;
	return (DDS_DataWriter_write_w_timestamp_directed (dw, &s, h, time, dests));
}


/* 3. DDS_StringDataReader. */

DDS_ReturnCode_t DDS_StringDataReader_read (DDS_DataReader dr,
					    DDS_StringSeq *data,
					    DDS_SampleInfoSeq *info_seq,
					    unsigned max_samples,
					    DDS_SampleStateMask sample_states,
					    DDS_ViewStateMask view_states,
					    DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read (dr, (DDS_DataSeq *) data, info_seq, 
				     max_samples,
				     sample_states, view_states, inst_states));
}

DDS_ReturnCode_t DDS_StringDataReader_take (DDS_DataReader dr,
					    DDS_StringSeq *data,
					    DDS_SampleInfoSeq *info_seq,
					    unsigned max_samples,
					    DDS_SampleStateMask sample_states,
					    DDS_ViewStateMask view_states,
					    DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take (dr, (DDS_DataSeq *) data, info_seq,
				     max_samples,
				     sample_states, view_states, inst_states));
}

DDS_ReturnCode_t DDS_StringDataReader_read_w_condition (DDS_DataReader dr,
							DDS_StringSeq *data,
							DDS_SampleInfoSeq *info,
							unsigned max_samples,
							DDS_ReadCondition cond)
{
	return (DDS_DataReader_read_w_condition (dr, (DDS_DataSeq *) data, 
						 info, max_samples, cond));
}

DDS_ReturnCode_t DDS_StringDataReader_take_w_condition (DDS_DataReader dr,
							DDS_StringSeq *data,
							DDS_SampleInfoSeq *info,
							unsigned max_samples,
							DDS_ReadCondition cond)
{
	return (DDS_DataReader_take_w_condition (dr, (DDS_DataSeq *) data,
						 info, max_samples, cond));
}

DDS_ReturnCode_t DDS_StringDataReader_read_next_sample (DDS_DataReader dr,
							char *data,
							DDS_SampleInfo *info)
{
	DDS_String	s;

	if (!data)
		return (DDS_RETCODE_BAD_PARAMETER);

	s.value = data;
	return (DDS_DataReader_read_next_sample (dr, &s, info));
}

DDS_ReturnCode_t DDS_StringDataReader_take_next_sample (DDS_DataReader dr,
							char *data,
							DDS_SampleInfo *info)
{
	DDS_String	s;

	if (!data)
		return (DDS_RETCODE_BAD_PARAMETER);

	s.value = data;
	return (DDS_DataReader_take_next_sample (dr, &s, info));
}

DDS_ReturnCode_t DDS_StringDataReader_return_loan (DDS_DataReader dr,
						   DDS_StringSeq *data,
						   DDS_SampleInfoSeq *info)
{
	return (DDS_DataReader_return_loan (dr, (DDS_DataSeq *) data, info));
}


/* DDS_KeyedString type.
   ---------------------
 
   The DDS_KeyedString type consists of two unbounded strings where the first
   is a key field and the second is the associated value. */

static DDS_TypeSupport_meta keyed_string_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 3, "DDS_KeyedString", sizeof (struct dds_keyed_string_st), 0, 2, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 3, "key", 0, offsetof (struct dds_keyed_string_st, key), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 2, "value", 0, offsetof (struct dds_keyed_string_st, value), 0, 0, NULL }
};

static DDS_TypeSupport	keyed_string_ts;

static int keyed_string_builtin_init (void)
{
	keyed_string_ts = DDS_DynamicType_register (keyed_string_tsm);
	if (!keyed_string_ts)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}


/* 1. DDS_KeyedStringTypeSupport. */

DDS_ReturnCode_t DDS_KeyedStringTypeSupport_register_type (
						DDS_DomainParticipant part,
						const char *name)
{
	if (!keyed_string_ts)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (DDS_DomainParticipant_register_type (part, keyed_string_ts, name));
}

const char *DDS_KeyedStringTypeSupport_get_type_name (void)
{
	return (DDS_TypeSupport_get_type_name (keyed_string_ts));
}


/* 2. DDS_KeyedStringDataWriter. */

/* 2a. Extra functions. */

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance_w_key (
							DDS_DataWriter dw,
							const char *key)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_register_instance (dw, &ks));
}

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance_w_key_w_timestamp(
							DDS_DataWriter dw,
							const char *key,
							const DDS_Time_t *time)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_register_instance_w_timestamp (dw, &ks, time));
}

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_unregister_instance_w_key(
							DDS_DataWriter dw,
							const char *key)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_unregister_instance (dw, &ks, 0));
}

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_unregister_instance_w_key_w_timestamp(
							DDS_DataWriter dw,
							const char *key,
							const DDS_Time_t *time)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_unregister_instance_w_timestamp (dw, &ks, 0, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_string_w_key(DDS_DataWriter dw,
							      const char *key,
							      const char *str,
							      const DDS_InstanceHandle_t h)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	ks.value = (char *) str;
	return (DDS_DataWriter_write (dw, &ks, h));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_string_w_key_w_timestamp(
							DDS_DataWriter dw,
							const char *key,
							const char *str,
							const DDS_InstanceHandle_t h,
							const DDS_Time_t *time)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	ks.value = (char *) str;
	return (DDS_DataWriter_write_w_timestamp (dw, &ks, h, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_key (DDS_DataWriter dw,
							  const char *key)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_dispose (dw, &ks, 0));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_key_w_timestamp (
							DDS_DataWriter dw,
							const char *key,
							const DDS_Time_t *time)
{
	DDS_KeyedString	ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_dispose_w_timestamp (dw, &ks, 0, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_get_key_value_w_key (
							DDS_DataWriter dw,
							char *key_data,
							const DDS_InstanceHandle_t h)
{
	DDS_KeyedString	ks;
	DDS_ReturnCode_t r;

	if (!key_data)
		return (DDS_RETCODE_BAD_PARAMETER);

	r = DDS_DataWriter_get_key_value (dw, &ks, h);
	if (r)
		return (r);

	strcpy (key_data, ks.key);
	return (DDS_RETCODE_OK);
}

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_lookup_instance_w_key (
							DDS_DataWriter dw,
							const char *key)
{
	DDS_KeyedString ks;

	ks.key = (char *) key;
	return (DDS_DataWriter_lookup_instance (dw, &ks));
}


/* 2b. Standard functions. */

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance (
						DDS_DataWriter dw,
						const DDS_KeyedString *data)
{
	return (DDS_DataWriter_register_instance (dw, data));
}

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_register_instance_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_register_instance_w_timestamp (dw, data, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_unregister_instance (dw, data, h));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_unregister_instance_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance_directed (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_unregister_instance_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_unregister_instance_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_unregister_instance_w_timestamp_directed (dw, 
							data, h, time, dests));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_get_key_value (
						DDS_DataWriter dw,
						DDS_KeyedString *key_data,
						const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_get_key_value (dw, key_data, h));
}

DDS_InstanceHandle_t DDS_KeyedStringDataWriter_lookup_instance (
						DDS_DataWriter dw,
						const DDS_KeyedString *key_data)
{
	return (DDS_DataWriter_lookup_instance (dw, key_data));
}


DDS_ReturnCode_t DDS_KeyedStringDataWriter_write (DDS_DataWriter dw,
						  const DDS_KeyedString *data,
						  const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_write (dw, data, h));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_write_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_directed (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_write_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_write_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_write_w_timestamp_directed (dw, data, h, time, dests));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose (DDS_DataWriter dw,
						    const DDS_KeyedString *data,
						    const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_dispose (dw, data, h));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_dispose_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_directed (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_dispose_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_KeyedStringDataWriter_dispose_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_KeyedString *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_dispose_w_timestamp_directed (dw, data, h, time, dests));
}


/* 3. DDS_KeyedStringDataReader. */

DDS_ReturnCode_t DDS_KeyedStringDataReader_read (DDS_DataReader dr,
						 DDS_KeyedStringSeq *data,
						 DDS_SampleInfoSeq *info,
					 	 unsigned max_samples,
						 DDS_SampleStateMask sample_states,
						 DDS_ViewStateMask view_states,
						 DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read (dr, (DDS_DataSeq *) data, info, max_samples,
				 sample_states, view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_take (DDS_DataReader dr,
						 DDS_KeyedStringSeq *data,
						 DDS_SampleInfoSeq *info,
					 	 unsigned max_samples,
						 DDS_SampleStateMask sample_states,
						 DDS_ViewStateMask view_states,
						 DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take (dr, (DDS_DataSeq *) data, info, max_samples,
				 sample_states, view_states, inst_states));
}


DDS_ReturnCode_t DDS_KeyedStringDataReader_read_w_condition (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_read_w_condition (dr, (DDS_DataSeq *) data, info, 
							max_samples, cond));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_take_w_condition (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_take_w_condition (dr, (DDS_DataSeq *) data, info, 
							max_samples, cond));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_read_next_sample (
						DDS_DataReader dr,
						DDS_KeyedString *data,
						DDS_SampleInfo *info)
{
	return (DDS_DataReader_read_next_sample (dr, data, info));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_take_next_sample (
						DDS_DataReader dr,
						DDS_KeyedString *data,
						DDS_SampleInfo *info)
{
	return (DDS_DataReader_read_next_sample (dr, data, info));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_read_instance (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read_instance (dr, (DDS_DataSeq *) data, info,
					      max_samples, h, sample_states,
					      view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_take_instance (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take_instance (dr, (DDS_DataSeq *) data, info,
					      max_samples, h, sample_states,
					      view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_read_next_instance (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read_next_instance (dr, (DDS_DataSeq *) data,
						   info, max_samples,
						   h, sample_states, 
						   view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_take_next_instance (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take_next_instance (dr, (DDS_DataSeq *) data, 
						   info, max_samples,
						   h, sample_states,
						   view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_read_next_instance_w_condition (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_read_next_instance_w_condition (dr, 
						(DDS_DataSeq *) data, info,
						max_samples, h, cond));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_take_next_instance_w_condition (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_take_next_instance_w_condition (dr,
						(DDS_DataSeq *) data, info,
						max_samples, h, cond));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_return_loan (
						DDS_DataReader dr,
						DDS_KeyedStringSeq *data,
						DDS_SampleInfoSeq *info)
{
	return (DDS_DataReader_return_loan (dr, (DDS_DataSeq *) data, info));
}

DDS_ReturnCode_t DDS_KeyedStringDataReader_get_key_value (
						DDS_DataReader dr,
						DDS_KeyedString *data,
						DDS_InstanceHandle_t h)
{
	return (DDS_DataReader_get_key_value (dr, data, h));
}

DDS_InstanceHandle_t DDS_KeyedStringDataReader_lookup_instance (
						DDS_DataReader dr,
						const DDS_KeyedString *key_data)
{
	return (DDS_DataReader_lookup_instance (dr, key_data));
}


/* DDS_Bytes type.
   ---------------
 
   The DDS_Bytes type is a key-less unbounded sequence of bytes. */

static DDS_TypeSupport_meta bytes_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 2, "DDS_Bytes", sizeof (struct dds_bytes_st), 0, 1, 0, NULL },
	{ CDR_TYPECODE_SEQUENCE, 2, "value", 0, offsetof (struct dds_bytes_st, value), 0, 0, NULL },
	{ CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL }
};

static DDS_TypeSupport	bytes_ts;

static int bytes_builtin_init (void)
{
	bytes_ts = DDS_DynamicType_register (bytes_tsm);
	if (!bytes_ts)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}

/* 1. DDS_BytesTypeSupport. */

DDS_ReturnCode_t DDS_BytesTypeSupport_register_type (
						DDS_DomainParticipant part,
						const char *name)
{
	if (!bytes_ts)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (DDS_DomainParticipant_register_type (part, bytes_ts, name));
}

const char *DDS_BytesTypeSupport_get_type_name (void)
{
	return (DDS_TypeSupport_get_type_name (bytes_ts));
}


/* 2. DDS_BytesDataWriter. */

/* 2a. Extra functions. */

DDS_ReturnCode_t DDS_BytesDataWriter_write_w_bytes (DDS_DataWriter dw,
						    const unsigned char *bytes,
						    int offset,
						    int length,
						    const DDS_InstanceHandle_t h)
{
	DDS_Bytes	bs;

	bs.value._maximum = bs.value._length = length;
	bs.value._esize = 1;
	bs.value._own = 1;
	bs.value._buffer = (unsigned char *) bytes + offset;
	return (DDS_DataWriter_write (dw, &bs, h));
}

DDS_ReturnCode_t DDS_BytesDataWriter_write_w_bytes_w_timestamp (
						DDS_DataWriter dw,
						const unsigned char *bytes,
						int offset,
						int length,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	DDS_Bytes	bs;

	bs.value._maximum = bs.value._length = length;
	bs.value._esize = 1;
	bs.value._own = 1;
	bs.value._buffer = (unsigned char *) bytes + offset;
	return (DDS_DataWriter_write_w_timestamp (dw, &bs, h, time));
}


/* 2b. Standard functions. */

DDS_ReturnCode_t DDS_BytesDataWriter_write (DDS_DataWriter dw,
					    const DDS_Bytes *data,
					    const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_write (dw, data, h));
}

DDS_ReturnCode_t DDS_BytesDataWriter_write_w_timestamp (DDS_DataWriter dw,
							const DDS_Bytes *data,
							const DDS_InstanceHandle_t h,
							const DDS_Time_t *time)
{
	return (DDS_DataWriter_write_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_BytesDataWriter_write_directed (DDS_DataWriter dw,
						     const DDS_Bytes *data,
						     const DDS_InstanceHandle_t h,
						     DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_write_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_BytesDataWriter_write_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_Bytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_write_w_timestamp_directed (dw,
					(DDS_DataSeq *) data, h, time, dests));
}


/* 3. DDS_BytesDataReader. */

DDS_ReturnCode_t DDS_BytesDataReader_read (DDS_DataReader dr,
					   DDS_BytesSeq *data,
					   DDS_SampleInfoSeq *info_seq,
					   unsigned max_samples,
					   DDS_SampleStateMask sample_states,
					   DDS_ViewStateMask view_states,
					   DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read (dr, (DDS_DataSeq *) data, info_seq,
				     max_samples, sample_states,
				     view_states, inst_states));
}

DDS_ReturnCode_t DDS_BytesDataReader_take (DDS_DataReader dr,
					   DDS_BytesSeq *data,
					   DDS_SampleInfoSeq *info_seq,
					   unsigned max_samples,
					   DDS_SampleStateMask sample_states,
					   DDS_ViewStateMask view_states,
					   DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take (dr, (DDS_DataSeq *) data, info_seq,
				     max_samples, sample_states,
				     view_states, inst_states));
}

DDS_ReturnCode_t DDS_BytesDataReader_read_w_condition (DDS_DataReader dr,
						       DDS_BytesSeq *data,
						       DDS_SampleInfoSeq *info,
						       unsigned max_samples,
						       DDS_ReadCondition cond)
{
	return (DDS_DataReader_read_w_condition (dr, (DDS_DataSeq *) data, info, 
						 max_samples, cond));
}

DDS_ReturnCode_t DDS_BytesDataReader_take_w_condition (DDS_DataReader dr,
						       DDS_BytesSeq *data,
						       DDS_SampleInfoSeq *info,
						       unsigned max_samples,
						       DDS_ReadCondition cond)
{
	return (DDS_DataReader_take_w_condition (dr, (DDS_DataSeq *) data, info, 
						 max_samples, cond));
}

DDS_ReturnCode_t DDS_BytesDataReader_read_next_sample (DDS_DataReader dr,
						       DDS_Bytes *data,
						       DDS_SampleInfo *info)
{
	return (DDS_DataReader_read_next_sample (dr, data, info));
}

DDS_ReturnCode_t DDS_BytesDataReader_take_next_sample (DDS_DataReader dr,
						       DDS_Bytes *data,
						       DDS_SampleInfo *info)
{
	return (DDS_DataReader_take_next_sample (dr, data, info));
}

DDS_ReturnCode_t DDS_BytesDataReader_return_loan (DDS_DataReader dr,
						  DDS_BytesSeq *data,
						  DDS_SampleInfoSeq *info)
{
	return (DDS_DataReader_return_loan (dr, (DDS_DataSeq *) data, info));
}


/* DDS_KeyedBytes type.
   ---------------------
 
   The DDS_KeyedBytes type is a keyed unbounded sequence of bytes. */

static DDS_TypeSupport_meta keyed_bytes_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 2, "DDS_KeyedBytes", sizeof (struct dds_keyed_bytes_st), 0, 2, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 3, "key", 0, offsetof (struct dds_keyed_bytes_st, key), 0, 0, NULL },
	{ CDR_TYPECODE_SEQUENCE, 2, "value", 0, offsetof (struct dds_keyed_bytes_st, value), 0, 0, NULL },
	{ CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL }
};

static DDS_TypeSupport	keyed_bytes_ts;

static int keyed_bytes_builtin_init (void)
{
	keyed_bytes_ts = DDS_DynamicType_register (keyed_bytes_tsm);
	if (!keyed_bytes_ts)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}


/* 1. DDS_KeyedBytesTypeSupport. */

DDS_ReturnCode_t DDS_KeyedBytesTypeSupport_register_type (
						DDS_DomainParticipant part,
						const char *name)
{
	if (!keyed_bytes_ts)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (DDS_DomainParticipant_register_type (part, keyed_bytes_ts, name));
}

const char *DDS_KeyedBytesTypeSupport_get_type_name (void)
{
	return (DDS_TypeSupport_get_type_name (keyed_bytes_ts));
}


/* 2. DDS_KeyedBytesDataWriter. */

/* 2a. Extra functions. */

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance_w_key (
							DDS_DataWriter dw,
							const char *key)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_register_instance (dw, &kb));
}

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance_w_key_w_timestamp(
							DDS_DataWriter dw,
							const char *key,
							const DDS_Time_t *time)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_register_instance_w_timestamp (dw, &kb, time));
}

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_unregister_instance_w_key (
							DDS_DataWriter dw,
							const char *key)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_unregister_instance (dw, &kb, 0));
}

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_unregister_instance_w_key_w_timestamp(
							DDS_DataWriter dw,
							const char *key,
							const DDS_Time_t *time)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_unregister_instance_w_timestamp (dw, &kb, 0, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_bytes_w_key (DDS_DataWriter dw,
							     const char *key,
							     const unsigned char *bytes,
							     int offset,
							     int length,
							     const DDS_InstanceHandle_t h)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	kb.value._length = kb.value._maximum = length;
	kb.value._esize = 1;
	kb.value._own = 1;
	kb.value._buffer = (unsigned char *) bytes + offset;
	return (DDS_DataWriter_write (dw, &kb, h));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_bytes_w_key_w_timestamp(
							DDS_DataWriter dw,
							const char *key,
							const unsigned char *bytes,
							int offset,
							int length,
							const DDS_InstanceHandle_t h,
							const DDS_Time_t *time)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	kb.value._length = kb.value._maximum = length;
	kb.value._esize = 1;
	kb.value._own = 1;
	kb.value._buffer = (unsigned char *) bytes + offset;
	return (DDS_DataWriter_write_w_timestamp (dw, &kb, h, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_key (DDS_DataWriter dw,
							 const char *key)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_dispose (dw, &kb, 0));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_key_w_timestamp (
							DDS_DataWriter dw,
							const char *key,
							const DDS_Time_t *time)
{
	DDS_KeyedBytes	kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_dispose_w_timestamp (dw, &kb, 0, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_get_key_value_w_key (
							DDS_DataWriter dw,
							char *key_data,
							const DDS_InstanceHandle_t h)
{
	DDS_KeyedBytes	kb;
	DDS_ReturnCode_t r;

	if (!key_data)
		return (DDS_RETCODE_BAD_PARAMETER);

	r = DDS_DataWriter_get_key_value (dw, &kb, h);
	if (r)
		return (r);

	strcpy (key_data, kb.key);
	return (DDS_RETCODE_OK);
}

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_lookup_instance_w_key (
							DDS_DataWriter dw,
							const char *key)
{
	DDS_KeyedBytes kb;

	kb.key = (char *) key;
	return (DDS_DataWriter_lookup_instance (dw, &kb));
}


/* 2b. Standard functions. */

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data)
{
	return (DDS_DataWriter_register_instance (dw, data));
}

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_register_instance_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_register_instance_w_timestamp (dw, data, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_unregister_instance (dw, data, h));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_unregister_instance_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance_directed (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_unregister_instance_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_unregister_instance_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_unregister_instance_w_timestamp_directed (dw, 
							data, h, time, dests));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_get_key_value (
						DDS_DataWriter dw,
						DDS_KeyedBytes *key_data,
						const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_get_key_value (dw, key_data, h));
}

DDS_InstanceHandle_t DDS_KeyedBytesDataWriter_lookup_instance (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *key_data)
{
	return (DDS_DataWriter_lookup_instance (dw, key_data));
}


DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write (DDS_DataWriter dw,
						  const DDS_KeyedBytes *data,
						  const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_write (dw, data, h));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_write_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_directed (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_write_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_write_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_write_w_timestamp_directed (dw, data, h, time, dests));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose (DDS_DataWriter dw,
						    const DDS_KeyedBytes *data,
						    const DDS_InstanceHandle_t h)
{
	return (DDS_DataWriter_dispose (dw, data, h));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_timestamp (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	return (DDS_DataWriter_dispose_w_timestamp (dw, data, h, time));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_directed (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_dispose_directed (dw, data, h, dests));
}

DDS_ReturnCode_t DDS_KeyedBytesDataWriter_dispose_w_timestamp_directed (
						DDS_DataWriter dw,
						const DDS_KeyedBytes *data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	return (DDS_DataWriter_dispose_w_timestamp_directed (dw, data, h, time, dests));
}


/* 3. DDS_KeyedBytesDataReader. */

DDS_ReturnCode_t DDS_KeyedBytesDataReader_read (DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
					 	unsigned max_samples,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read (dr, (DDS_DataSeq *) data, info,
				     max_samples, sample_states,
				     view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_take (DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
					 	unsigned max_samples,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take (dr, (DDS_DataSeq *) data, info,
				     max_samples, sample_states,
				     view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_w_condition (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_read_w_condition (dr, (DDS_DataSeq *) data, info, 
						 max_samples, cond));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_w_condition (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_take_w_condition (dr, (DDS_DataSeq *) data, info, 
						 max_samples, cond));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_next_sample (
						DDS_DataReader dr,
						DDS_KeyedBytes *data,
						DDS_SampleInfo *info)
{
	return (DDS_DataReader_read_next_sample (dr, data, info));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_next_sample (
						DDS_DataReader dr,
						DDS_KeyedBytes *data,
						DDS_SampleInfo *info)
{
	return (DDS_DataReader_read_next_sample (dr, data, info));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_instance (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read_instance (dr, (DDS_DataSeq *) data, info,
					      max_samples, h, sample_states,
					      view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_instance (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take_instance (dr, (DDS_DataSeq *) data, info,
					      max_samples, h, sample_states,
					      view_states, inst_states));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_next_instance (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_read_next_instance (dr, (DDS_DataSeq *) data,
						   info, max_samples, h,
						   sample_states, view_states,
						   inst_states));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_next_instance (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	return (DDS_DataReader_take_next_instance (dr, (DDS_DataSeq *) data,
						   info, max_samples, h,
						   sample_states, view_states,
						   inst_states));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_read_next_instance_w_condition (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_read_next_instance_w_condition (dr, 
						(DDS_DataSeq *) data, info,
						max_samples, h, cond));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_take_next_instance_w_condition (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info,
						unsigned max_samples,
						DDS_InstanceHandle_t h,
						DDS_ReadCondition cond)
{
	return (DDS_DataReader_take_next_instance_w_condition (dr,
						(DDS_DataSeq *) data, info,
						max_samples, h, cond));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_return_loan (
						DDS_DataReader dr,
						DDS_KeyedBytesSeq *data,
						DDS_SampleInfoSeq *info)
{
	return (DDS_DataReader_return_loan (dr, (DDS_DataSeq *) data, info));
}

DDS_ReturnCode_t DDS_KeyedBytesDataReader_get_key_value (
						DDS_DataReader dr,
						DDS_KeyedBytes *data,
						DDS_InstanceHandle_t h)
{
	return (DDS_DataReader_get_key_value (dr, data, h));
}

DDS_InstanceHandle_t DDS_KeyedBytesDataReader_lookup_instance (
						DDS_DataReader dr,
						const DDS_KeyedBytes *key_data)
{
	return (DDS_DataReader_lookup_instance (dr, key_data));
}

int builtin_init (void)
{
	DDS_ReturnCode_t	rc;

	rc = string_builtin_init ();
	if (rc)
		return (rc);

	rc = keyed_string_builtin_init ();
	if (rc)
		return (rc);

	rc = bytes_builtin_init ();
	if (rc)
		return (rc);

	rc = keyed_bytes_builtin_init ();
	if (rc)
		return (rc);

	dds_builtins_used = 1;
	return (DDS_RETCODE_OK);
}

#endif /* DDS_BUILTINS */

