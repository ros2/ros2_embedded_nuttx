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

#define	ADD_FIELD(s,md,n,idx,i,t) md->name=n; md->index=idx; md->id=i;\
				  md->type=DDS_DynamicTypeBuilderFactory_get_primitive_type(t);\
				  fail_unless(md->type != NULL); \
				  rc = DDS_DynamicTypeBuilder_add_member (s,md); \
				  fail_unless (rc == DDS_RETCODE_OK)
#define	SET_FIELD(dd,id,type,v)	rc=DDS_DynamicData_set_##type##_value (dd,id,v); \
				fail_unless (rc == DDS_RETCODE_OK)

/** Array of basic char type ::
typedef char chararray [20];
struct dstruct1 {
	chararray char_a;
};
*/

void test_dyn_array1 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb, abc20;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicType s, ac20;
	DDS_BoundSeq bounds;
	DDS_DynamicData dd, dda, dd2;
	DDS_ReturnCode_t rc;
	unsigned i;

	v_printf ("test_dyn_array1 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);

	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct1";
	sb = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	DDS_SEQ_INIT (bounds);
	dds_seq_require (&bounds, 1);
	DDS_SEQ_LENGTH (bounds) = 1;
	DDS_SEQ_ITEM (bounds, 0) = 20;

	abc20 = DDS_DynamicTypeBuilderFactory_create_array_type (
		DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_CHAR_8_TYPE),
		&bounds);
	fail_unless (abc20 != NULL);

	dds_seq_cleanup (&bounds);

	ac20 = DDS_DynamicTypeBuilder_build (abc20);
	fail_unless (ac20 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (abc20);

	md->name = "char_a";
	md->index = md->id = 0;
	md->type = ac20;

	rc = DDS_DynamicTypeBuilder_add_member (sb, md);
	fail_unless (rc == DDS_RETCODE_OK);

	s = DDS_DynamicTypeBuilder_build (sb);
	fail_unless (s != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb);
	ts = DDS_DynamicTypeSupport_create_type_support (s);
	fail_unless (ts != NULL);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);

	/* 2. Create a Dynamic Data item for this type. */
	dd = DDS_DynamicDataFactory_create_data (s);
	fail_unless (dd != NULL);

	dda = DDS_DynamicDataFactory_create_data (ac20);
	fail_unless (dda != NULL);

	for (i = 0; i < 20; i++) {
		SET_FIELD (dda, i, char8, i + '0');
	}
	SET_FIELD (dd, 0, complex, dda);

	marshallDynamic (dd, &dd2, ts);

	DDS_DynamicDataFactory_delete_data (dd);
	DDS_DynamicDataFactory_delete_data (dda);

	DDS_DynamicTypeBuilderFactory_delete_type (ac20);
	DDS_DynamicTypeBuilderFactory_delete_type (s);
	DDS_DynamicTypeSupport_delete_type_support (ts);
	v_printf ("success!\r\n");
}

/* Array of struct ::
#define TA2_I64X	5

typedef int64_t i64array [TA2_I64X];
struct dstruct2a {
	i64array i64;
	char ch;
};

#define TA2_STR2X	7

typedef struct dstruct2a dstruct2aarray [TA2_STR2X];

struct dstruct2 {
	char c;
	dstruct2aarray str2;
	uint16_t u16;
};
*/

void test_dyn_array2 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb1, sb1a, abi64, abs1a;
	DDS_DynamicType s1, s1a, ai64, as1a;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dds, dda, ddas, dd2;
	DDS_ReturnCode_t rc;
	DDS_BoundSeq bounds;
	unsigned i, j;

	v_printf ("test_dyn_array2 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);

	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct2a";
	sb1a = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb1a != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	DDS_SEQ_INIT (bounds);
	dds_seq_require (&bounds, 1);
	DDS_SEQ_LENGTH (bounds) = 1;
	DDS_SEQ_ITEM (bounds, 0) = 5;

	abi64 = DDS_DynamicTypeBuilderFactory_create_array_type (
		DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_64_TYPE),
		&bounds);
	fail_unless (abi64 != NULL);

	ai64 = DDS_DynamicTypeBuilder_build (abi64);
	fail_unless (ai64 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (abi64);

	md->name = "i64";
	md->index = md->id = 0;
	md->type = ai64;

	rc = DDS_DynamicTypeBuilder_add_member (sb1a, md);
	fail_unless (rc == DDS_RETCODE_OK);

	ADD_FIELD (sb1a, md, "ch", 1, 1, DDS_CHAR_8_TYPE);

	s1a = DDS_DynamicTypeBuilder_build (sb1a);
	fail_unless (s1a != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb1a);

	DDS_SEQ_ITEM (bounds, 0) = 7;
	abs1a = DDS_DynamicTypeBuilderFactory_create_array_type (s1a, &bounds);
	fail_unless (abs1a != NULL);

	dds_seq_cleanup (&bounds);

	as1a = DDS_DynamicTypeBuilder_build (abs1a);
	fail_unless (as1a != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (abs1a);
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct2";
	sb1 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb1 != NULL);

	ADD_FIELD (sb1, md, "c", 0, 0, DDS_CHAR_8_TYPE);

	md->name = "str2";
	md->index = md->id = 1;
	md->type = as1a;
	rc = DDS_DynamicTypeBuilder_add_member (sb1, md);
	fail_unless (rc == DDS_RETCODE_OK);

	ADD_FIELD (sb1, md, "u16", 2, 2, DDS_UINT_16_TYPE);

	s1 = DDS_DynamicTypeBuilder_build (sb1);
	fail_unless (s1 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb1);
	ts = DDS_DynamicTypeSupport_create_type_support (s1);
	fail_unless (ts != NULL);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);

	/* 2. Create a Dynamic Data item for this type. */
	dd = DDS_DynamicDataFactory_create_data (s1);
	fail_unless (dd != NULL);

	ddas = DDS_DynamicDataFactory_create_data (as1a);
	fail_unless (ddas != NULL);

	SET_FIELD (dd, 0, char8, 'A');
	for (i = 0; i < 7; i++) {
		dds = DDS_DynamicDataFactory_create_data (s1a);
		fail_unless (dds != NULL);

		dda = DDS_DynamicDataFactory_create_data (ai64);
		fail_unless (dda != NULL);

		for (j = 0; j < 5; j++) {
			SET_FIELD (dda, j, int64, ((i + 6) * j) << i);
		}
		SET_FIELD (dds, 0, complex, dda);
		DDS_DynamicDataFactory_delete_data (dda);

		SET_FIELD (dds, 1, char8, i + 0x30);

		SET_FIELD (ddas, i, complex, dds);
		DDS_DynamicDataFactory_delete_data (dds);
	}
	SET_FIELD (dd, 1, complex, ddas);
	DDS_DynamicDataFactory_delete_data (ddas);

	SET_FIELD (dd, 2, uint16, 1366);

	marshallDynamic (dd, &dd2, ts);

	DDS_DynamicDataFactory_delete_data (dd);
	DDS_DynamicTypeBuilderFactory_delete_type (s1);
	DDS_DynamicTypeBuilderFactory_delete_type (as1a);
	DDS_DynamicTypeBuilderFactory_delete_type (s1a);
	DDS_DynamicTypeBuilderFactory_delete_type (ai64);
	DDS_DynamicTypeSupport_delete_type_support (ts);
	v_printf ("success!\r\n");
}

void test_dyn_arrays (void)
{
	test_dyn_array1 ();
	test_dyn_array2 ();
}
