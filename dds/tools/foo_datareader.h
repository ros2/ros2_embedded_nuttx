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

#define	fooDataReader_read(self,data,info,max,sstates,vstates,istates) \
		DDS_DataReader_read(self,data,info,max,sstates,vstates,istates)
#define	fooDataReader_take(self,data,info,max,sstates,vstates,istates) \
		DDS_DataReader_take(self,data,info,max,sstates,vstates,istates)
#define	fooDataReader_read_w_condition(self,data,info,max,cond) \
		DDS_DataReader_read_w_condition(self,data,info,max,cond)
#define	fooDataReader_take_w_condition(self,data,info,max,cond) \
		DDS_DataReader_take_w_condition(self,data,info,max,cond)
#define	fooDataReader_read_next_sample(self,data,info) \
		DDS_DataReader_read_next_sample(self,data,info)
#define	fooDataReader_take_next_sample(self,data,info) \
		DDS_DataReader_take_next_sample(self,data,info)
#define	fooDataReader_read_instance(self,data,info,max,handle,sstates,vstates,istates) \
		DDS_DataReader_read_instance(self,data,info,max,handle,sstates,vstates,istates)
#define	fooDataReader_take_instance(self,data,info,max,handle,sstates,vstates,istates) \
		DDS_DataReader_take_instance(self,data,info,max,handle,sstates,vstates,istates)
#define	fooDataReader_read_next_instance(self,data,info,max,handle,sstates,vstates,istates) \
		DDS_DataReader_read_next_instance(self,data,info,max,handle,sstates,vstates,istates)
#define	fooDataReader_take_next_instance(self,data,info,max,handle,sstates,vstates,istates) \
		DDS_DataReader_take_next_instance(self,data,info,max,handle,sstates,vstates,istates)
#define	fooDataReader_read_next_instance_w_condition(self,data,info,max,handle,cond) \
		DDS_DataReader_read_next_instance_w_condition(self,data,info,max,handle,cond)
#define	fooDataReader_take_next_instance_w_condition(self,data,info,max,handle,cond) \
		DDS_DataReader_take_next_instance_w_condition(self,data,info,max,handle,cond)
#define	fooDataReader_return_loan(self,data,info) \
		DDS_DataReader_return_loan(self,data,info)
#define	fooDataReader_get_key_value(self,data,handle)
		DDS_DataReader_get_key_value(self,data,handle)
#define	fooDataReader_lookup_instance(self,key_data) \
		DDS_DataReader_lookup_instance(self,key_data)
