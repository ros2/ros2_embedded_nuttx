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

/* Very basic map ::
map<long, string<50>>

will is serialized as:

struct MapEntry_Int32_String_50 {
        long key;
        string<50> value;
};
typedef sequence<MapEntry_Int32_String_50> Map_Int32_String_50;

This is used in:

struct dmap1 {
	map<long, string<50>> m;
	long long i64;
};
*/

void test_dyn_map1 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb1, mb, sb;
	DDS_DynamicType s1, s, m;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dd2, ddm;
	DDS_ReturnCode_t rc;

	v_printf ("test_dyn_map1 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	sb = DDS_DynamicTypeBuilderFactory_create_string_type (50);
	fail_unless (sb != NULL);

	s = DDS_DynamicTypeBuilder_build (sb);
	fail_unless (s != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb);

	mb = DDS_DynamicTypeBuilderFactory_create_map_type (
		DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE),
		s, DDS_UNBOUNDED_COLLECTION);
	fail_unless (mb != NULL);

	m = DDS_DynamicTypeBuilder_build (mb);
	fail_unless (m != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (mb);

	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dmap1";

	sb1 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb1 != NULL);

	md->name = "m";
	md->id = 0;
	md->type = m;
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

	DDS_TypeSupport_dump_type (0, (DDS_TypeSupport) ts, 15);

	/* 2. Create a Dynamic data item for this type. */
	ddm = DDS_DynamicDataFactory_create_data (m);
	md->name = "key";
	md->id = DDS_DynamicData_get_member_id_by_name (ddm, "alfa");
	md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
	md->default_value = "0"; 
	md->index = 0;
	md->default_label = 0;


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

void test_dyn_maps (void)
{
	test_dyn_map1 ();
}

