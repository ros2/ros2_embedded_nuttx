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

#ifndef __ta_type_h_
#define	__ta_type_h_

#include <stdint.h>
#include "dds/dds_dcps.h"

#define	TYPE_NAME "HelloWorldData"

typedef struct msg_data_st {
	uint64_t	counter;
	unsigned	key [5];	/* @Key */
	char		message [200];
} MsgData_t;

extern DDS_TypeSupport dds_HelloWorld_ts;

extern DDS_ReturnCode_t new_HelloWorldData_type (void);

extern void free_HelloWorldData_type (void);

extern DDS_ReturnCode_t attach_HelloWorldData_type (DDS_DomainParticipant par);

extern DDS_ReturnCode_t detach_HelloWorldData_type (DDS_DomainParticipant par);

extern DDS_ReturnCode_t register_HelloWorldData_type (DDS_DomainParticipant part);

extern void unregister_HelloWorldData_type (DDS_DomainParticipant part);

extern void test_type (void);

#endif /* !__ta_type_h_ */

