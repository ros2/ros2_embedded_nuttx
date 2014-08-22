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

/* test.h -- Defines the types as used in the code. */

#ifndef __test_h_
#define	__test_h_

#include "dds/dds_dcps.h"

typedef struct tst1_msg_st { 
	unsigned char	c1;	/* //@key */
	int64_t		ll;	/* //@key */
	unsigned char	c2;	/* //@key */
	DDS_OctetSeq	data;
} Tst1Msg;

typedef struct tst2_msg_st { 
	unsigned char	c1;	/* //@key */
	int		w;	/* //@key */
	char		n [21];	/* //@key */
	short		s;	/* //@key */
	DDS_OctetSeq	data;
} Tst2Msg;

typedef struct tst3_msg_st { 
	unsigned char	c1;	/* //@key */
	short		s;	/* //@key */
	int		l;	/* //@key */
	char		n [13];	/* //@key */
	int64_t		ll;	/* //@key */
	DDS_OctetSeq	data;
} Tst3Msg;

typedef struct tst4_msg_st { 
	unsigned char	c1;	/* //@key */
	char		*n;	/* //@key */
	short		s;	/* //@key */
	DDS_OctetSeq	data;
} Tst4Msg;

DDS_ReturnCode_t Tst1MsgTypeSupport_register_type (
			DDS_DomainParticipant domain,
			const char *type_name);

DDS_ReturnCode_t Tst2MsgTypeSupport_register_type (
			DDS_DomainParticipant domain,
			const char *type_name);

DDS_ReturnCode_t Tst3MsgTypeSupport_register_type (
			DDS_DomainParticipant domain,
			const char *type_name);

DDS_ReturnCode_t Tst4MsgTypeSupport_register_type (
			DDS_DomainParticipant domain,
			const char *type_name);

void test_unregister_types (void);

#endif /* !__test_h_ */

