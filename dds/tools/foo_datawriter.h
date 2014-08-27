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

#define	fooDataWriter_register_instance(self,data)	\
		DDS_DataWriter_register_instance(self,data)
		
#define	fooDataWriter_register_instance_w_timestamp(self,data,time)	\
		DDS_DataWriter_register_instance_w_timestamp(self,data,time)

#define fooDataWriter_unregister_instance(self,data,handle)	\
		DDS_DataWriter_unregister_instance(self,data,handle)

#define fooDataWriter_unregister_instance_w_timestamp(self,data,handle,time) \
		DDS_DataWriter_unregister_instance_w_timestamp(self,data,handle,time)

#define fooDataWriter_unregister_instance_directed(self,data,handle,dsts) \
		DDS_DataWriter_unregister_instance_directed(self,data,handle,dsts)

#define fooDataWriter_unregister_instance_w_timestamp_directed(self,data,handle,time,dsts) \
		DDS_DataWriter_unregister_instance_w_timestamp_directed(self,data,handle,time,dsts)

#define	fooDataWriter_get_key_value(self,data,handle) \
		DDS_DataWriter_get_key_value(self,data,handle)

#define	fooDataWriter_lookup_instance(self,key_data) \
		DDS_DataWriter_lookup_instance(self,key_data)

#define	fooDataWriter_write(self,data,handle) \
		DDS_DataWriter_write(self,data,handle)

#define	fooDataWriter_write_w_timestamp(self,data,handle,time) \
		DDS_DataWriter_write_w_timestamp(self,data,handle,time)

#define	fooDataWriter_write_directed(self,data,handle,dsts) \
		DDS_DataWriter_write_directed(self,data,handle,dsts)

#define	fooDataWriter_write_w_timestamp_directed(self,data,handle,time,dsts) \
		DDS_DataWriter_write_w_timestamp_directed(self,data,handle,time,dsts)

#define	fooDataWriter_dispose(self,data,handle)	\
		DDS_DataWriter_dispose(self,data,handle)

#define	fooDataWriter_dispose_w_timestamp(self,data,handle,time) \
		DDS_DataWriter_dispose_w_timestamp(self,data,handle,time)

#define	fooDataWriter_dispose_directed(self,data,handle,dsts) \
		DDS_DataWriter_dispose_directed(self,data,handle,dsts)

#define	fooDataWriter_dispose_w_timestamp_directed(self,data,handle,time,dsts) \
		DDS_DataWriter_dispose_w_timestamp_directed(self,data,handle,time,dsts)
