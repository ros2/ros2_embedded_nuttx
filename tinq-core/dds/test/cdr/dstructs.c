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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pool.h"
#include "dds/dds_xtypes.h"
#include "typecode.h"
#include "xcdr.h"
#include "test.h"

/* Very basic struct ::
struct struct1 {
	int32_t a;
	int64_t b;
};
*/

void test_dyn_struct1 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb1;
	DDS_DynamicType s1;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dd2;
	DDS_ReturnCode_t rc;

	v_printf ("test_dyn_struct1 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct1";

	sb1 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb1 != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	md->name = "a";
	md->id = 0;
	md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
	md->index = 0;
	fail_unless (md->type != NULL);

	rc = DDS_DynamicTypeBuilder_add_member (sb1, md);
	fail_unless (rc == DDS_RETCODE_OK);

	md->name = "b";
	md->id = 1;
	md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_64_TYPE);
	md->index = 1;
	fail_unless (md->type != NULL);

	rc = DDS_DynamicTypeBuilder_add_member (sb1, md);
	fail_unless (rc == DDS_RETCODE_OK);

	s1 = DDS_DynamicTypeBuilder_build (sb1);
	fail_unless (s1 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb1);

	ts = DDS_DynamicTypeSupport_create_type_support (s1);
	fail_unless (ts != NULL);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);

	/* 2. Create a Dynamic data item for this type. */
	dd = DDS_DynamicDataFactory_create_data (s1);
	fail_unless (dd != NULL);

	rc = DDS_DynamicData_set_int32_value (dd, 0, 0xCAFEBABE);
	fail_unless (rc == DDS_RETCODE_OK);

	rc = DDS_DynamicData_set_int64_value (dd, 1, -1);
	fail_unless (rc == DDS_RETCODE_OK);

	marshallDynamic (dd, &dd2, ts);

	DDS_DynamicDataFactory_delete_data (dd);
	DDS_DynamicTypeBuilderFactory_delete_type (s1);
	DDS_DynamicTypeSupport_delete_type_support (ts);

	v_printf ("success!\r\n");
}

/** struct with basic types ::
struct struct2 {
	uint16_t u16;
	int16_t i16;
	uint32_t u32;
	int32_t i32;
	uint64_t u64;
	int64_t i64;
	float fl;
	double d;
	char ch;
};
*/

#define	ADD_FIELD(s,md,n,idx,i,t) md->name=n; md->index=idx; md->id=i;\
				  md->type=DDS_DynamicTypeBuilderFactory_get_primitive_type(t);\
				  fail_unless(md->type != NULL); \
				  rc = DDS_DynamicTypeBuilder_add_member (s,md); \
				  fail_unless (rc == DDS_RETCODE_OK)
#define	SET_FIELD(dd,id,type,v)	rc=DDS_DynamicData_set_##type##_value (dd,id,v); \
				fail_unless (rc == DDS_RETCODE_OK)

void test_dyn_struct2 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb2;
	DDS_DynamicType s2;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dd2;
	DDS_ReturnCode_t rc;

	v_printf ("test_dyn_struct2 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct2";

	sb2 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb2 != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	ADD_FIELD (sb2, md, "u16", 0, 0, DDS_UINT_16_TYPE);
	ADD_FIELD (sb2, md, "i16", 1, 1, DDS_INT_16_TYPE);
	ADD_FIELD (sb2, md, "u32", 2, 2, DDS_UINT_32_TYPE);
	ADD_FIELD (sb2, md, "i32", 3, 3, DDS_INT_32_TYPE);
	ADD_FIELD (sb2, md, "u64", 4, 4, DDS_UINT_64_TYPE);
	ADD_FIELD (sb2, md, "i64", 5, 5, DDS_INT_64_TYPE);
	ADD_FIELD (sb2, md, "fl",  6, 6, DDS_FLOAT_32_TYPE);
	ADD_FIELD (sb2, md, "d",   7, 7, DDS_FLOAT_64_TYPE);
	ADD_FIELD (sb2, md, "ch",  8, 8, DDS_CHAR_8_TYPE);

	s2 = DDS_DynamicTypeBuilder_build (sb2);
	fail_unless (s2 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb2);

	ts = DDS_DynamicTypeSupport_create_type_support (s2);
	fail_unless (ts != NULL);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);

	/* 2. Create a Dynamic data item for this type. */
	dd = DDS_DynamicDataFactory_create_data (s2);
	fail_unless (dd != NULL);

	SET_FIELD (dd, 0, uint16, 0xDEAD);
	SET_FIELD (dd, 1, int16, INT16_MIN);
	SET_FIELD (dd, 2, uint32, UINT32_MAX);
	SET_FIELD (dd, 3, int32, -5);
	SET_FIELD (dd, 4, uint64, 5010000);
	SET_FIELD (dd, 5, int64, 100);
	SET_FIELD (dd, 6, float32, 0.5f);
	SET_FIELD (dd, 7, float64, 100e-5);
	SET_FIELD (dd, 8, char8, 'd');

	marshallDynamic (dd, &dd2, ts);

	DDS_DynamicDataFactory_delete_data (dd);

	DDS_DynamicTypeBuilderFactory_delete_type (s2);

	DDS_DynamicTypeSupport_delete_type_support (ts);
	v_printf ("success!\r\n");
}

#define STRUCT3A_MESSAGE_LEN	100

/** struct of structs ::
struct dstruct3a {
	char message[STRUCT3A_MESSAGE_LEN];
	uint32_t i;
};
struct dstruct3b {
	float fl;
};
struct dstruct3 {
	struct dstruct3a s_3a;
	struct dstruct3b s_3b;
};
*/

void test_dyn_struct3 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb3, sb3a, sb3b, ssb;
	DDS_DynamicType s3, s3a, s3b, ss;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dda, ddb, dd2;
	DDS_ReturnCode_t rc;

	v_printf ("test_dyn_struct3 - ");

	/* 1. Create the type. */
	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);

	/* Create s3a. */
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct3a";
	sb3a = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb3a != NULL);

	/* Create String. */
	ssb = DDS_DynamicTypeBuilderFactory_create_string_type (STRUCT3A_MESSAGE_LEN);
	fail_unless (ssb != NULL);

	ss = DDS_DynamicTypeBuilder_build (ssb);
	fail_unless (ss != NULL);

	/* Add string field. */
	md->name = "message";
	md->index = md->id = 0;
	md->type = ss;
	fail_unless (md->type != NULL);

	rc = DDS_DynamicTypeBuilder_add_member (sb3a, md);
	fail_unless (rc == DDS_RETCODE_OK);

	ADD_FIELD (sb3a, md, "i", 1, 1, DDS_UINT_32_TYPE);

	s3a = DDS_DynamicTypeBuilder_build (sb3a);
	fail_unless (s3a != NULL);

	/* Create s3b. */
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct3b";
	sb3b = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb3b != NULL);

	ADD_FIELD (sb3b, md, "fl", 0, 0, DDS_FLOAT_32_TYPE);

	s3b = DDS_DynamicTypeBuilder_build (sb3b);
	fail_unless (s3b != NULL);

	/* Create s3. */
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct3";

	sb3 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb3 != NULL);

	md->name = "s_3a";
	md->index = md->id = 0;
	md->type = s3a;
	rc = DDS_DynamicTypeBuilder_add_member (sb3, md);
	fail_unless (rc == DDS_RETCODE_OK);

	md->name = "s_3b";
	md->index = md->id = 1;
	md->type = s3b;
	rc = DDS_DynamicTypeBuilder_add_member (sb3, md);
	fail_unless (rc == DDS_RETCODE_OK);

	s3 = DDS_DynamicTypeBuilder_build (sb3);
	fail_unless (s3 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (ssb);
	DDS_DynamicTypeBuilderFactory_delete_type (sb3a);
	DDS_DynamicTypeBuilderFactory_delete_type (sb3b);
	DDS_DynamicTypeBuilderFactory_delete_type (sb3);

	ts = DDS_DynamicTypeSupport_create_type_support (s3);
	fail_unless (ts != NULL);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);

	/* 2. Create a Dynamic data item for this type. */
	dda = DDS_DynamicDataFactory_create_data (s3a);
	fail_unless (dda != NULL);

	SET_FIELD (dda, 0, string, "Testing 1,2,3");
	SET_FIELD (dda, 1, uint32, 25);

	ddb = DDS_DynamicDataFactory_create_data (s3b);
	fail_unless (ddb != NULL);

	SET_FIELD (ddb, 0, float32, 0.3e9);

	dd = DDS_DynamicDataFactory_create_data (s3);
	fail_unless (dd != NULL);

	SET_FIELD (dd, 0, complex, dda);
	SET_FIELD (dd, 1, complex, ddb);

	marshallDynamic (dd, &dd2, ts);

	DDS_DynamicDataFactory_delete_data (dda);
	DDS_DynamicDataFactory_delete_data (ddb);
	DDS_DynamicDataFactory_delete_data (dd);

	DDS_DynamicTypeBuilderFactory_delete_type (ss);
	DDS_DynamicTypeBuilderFactory_delete_type (s3);
	DDS_DynamicTypeBuilderFactory_delete_type (s3a);
	DDS_DynamicTypeBuilderFactory_delete_type (s3b);

	DDS_DynamicTypeSupport_delete_type_support (ts);
	v_printf ("success!\r\n");
}


void test_dyn_structs (void)
{
	test_dyn_struct1 ();
	test_dyn_struct2 ();
	test_dyn_struct3 ();
}


