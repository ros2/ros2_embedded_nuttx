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
#include "crc32.h"
#include "tsm.h"

/** struct with basic types ::
struct struct2m {
	uint16_t u16;	//@ID(10) //@Key
	int16_t i16;	//@ID(20)
	uint32_t u32;	//@Key
	int32_t i32;	//@ID(50)
	uint64_t u64;
	int64_t i64;
	float fl;
	double d;
	char ch;	//@ID(5) //@Key
	sequence<octet> s;
};
*/

#define	ADD_FIELD(s,md,n,idx,i,t) md->name=n; md->index=idx; md->id=i;\
				  md->type=DDS_DynamicTypeBuilderFactory_get_primitive_type(t);\
				  fail_unless(md->type != NULL); \
				  rc = DDS_DynamicTypeBuilder_add_member (s,md); \
				  fail_unless (rc == DDS_RETCODE_OK)
#define	SET_FIELD(dd,id,type,v)	rc=DDS_DynamicData_set_##type##_value (dd,id,v); \
				fail_unless (rc == DDS_RETCODE_OK)

void set_key_annotation (DDS_DynamicTypeBuilder b,
			 const char             *name)
{
	DDS_DynamicTypeMember		dtm;
	DDS_ReturnCode_t	 	ret;
	static DDS_AnnotationDescriptor	ad = { NULL, };

	if (!b && ad.type) {
		DDS_AnnotationDescriptor__clear (&ad);
		return;
	}
	if (!ad.type) {
		ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("Key");
		fail_unless (ad.type != NULL);
	}
	dtm = DDS_DynamicTypeMember__alloc ();
	fail_unless (dtm != NULL);

	ret = DDS_DynamicTypeBuilder_get_member_by_name (b, dtm, name);
	fail_unless (ret == DDS_RETCODE_OK);

	ret = DDS_DynamicTypeMember_apply_annotation (dtm, &ad);
	fail_unless (ret == DDS_RETCODE_OK);

	DDS_DynamicTypeMember__free (dtm);
}

void set_id_annotation (DDS_DynamicTypeBuilder b,
			const char             *name,
			DDS_MemberId           id)
{
	DDS_DynamicTypeMember		dtm;
	DDS_ReturnCode_t	 	ret;
	unsigned			n;
	char				buf [12];
	static DDS_AnnotationDescriptor	ad = { NULL, };

	if (!b && ad.type) {
		DDS_AnnotationDescriptor__clear (&ad);
		return;
	}
	if (!ad.type) {
		ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("ID");
		fail_unless (ad.type != NULL);
	}
	n = snprintf (buf, sizeof (buf), "%u", id);
	fail_unless (n != 0);

	ret = DDS_AnnotationDescriptor_set_value (&ad, "value", buf);
	fail_unless (ret == DDS_RETCODE_OK);

	dtm = DDS_DynamicTypeMember__alloc ();
	fail_unless (dtm != NULL);

	ret = DDS_DynamicTypeBuilder_get_member_by_name (b, dtm, name);
	fail_unless (ret == DDS_RETCODE_OK);

	ret = DDS_DynamicTypeMember_apply_annotation (dtm, &ad);
	fail_unless (ret == DDS_RETCODE_OK);

	DDS_DynamicTypeMember__free (dtm);
}

void set_ext_annotation (DDS_DynamicTypeBuilder b,
			 const char             *ext)
{
	DDS_ReturnCode_t	 	ret;
	static DDS_AnnotationDescriptor	ad = { NULL, };

	if (!b && ad.type) {
		DDS_AnnotationDescriptor__clear (&ad);
		return;
	}
	if (!ad.type) {
		ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("Extensibility");
		fail_unless (ad.type != NULL);
	}
	ret = DDS_AnnotationDescriptor_set_value (&ad, "value", ext);
	fail_unless (ret == DDS_RETCODE_OK);

	ret = DDS_DynamicTypeBuilder_apply_annotation (b, &ad);
	fail_unless (ret == DDS_RETCODE_OK);
}

void test_dyn_mutable1 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb2, ostb;
	DDS_DynamicType s2, ot, ost;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dd2, dds;
	DDS_ReturnCode_t rc;
	DDS_ByteSeq bseq;
	unsigned char values [] = { 0x22, 0x33, 0x4f, 0x5e, 0x6d, 0x7c, 0x8b };

	v_printf ("test_mutable1 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct1m";

	sb2 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb2 != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	ADD_FIELD (sb2, md, "i16", 0, 99, DDS_INT_16_TYPE);
	set_id_annotation (sb2, "i16", 20);

	ADD_FIELD (sb2, md, "u32", 1, 2, DDS_UINT_32_TYPE);
	set_key_annotation (sb2, "u32");

	ADD_FIELD (sb2, md, "i32", 2, 100, DDS_INT_32_TYPE);
	set_id_annotation (sb2, "i32", 50);
	ADD_FIELD (sb2, md, "u16", 0, DDS_MEMBER_ID_INVALID, DDS_UINT_16_TYPE);
	set_id_annotation (sb2, "u16", 10);
	set_key_annotation (sb2, "u16");
	ADD_FIELD (sb2, md, "u64", 5, 51, DDS_UINT_64_TYPE);
	ADD_FIELD (sb2, md, "i64", 5, DDS_MEMBER_ID_INVALID, DDS_INT_64_TYPE);
	ADD_FIELD (sb2, md, "fl",  6, 53, DDS_FLOAT_32_TYPE);
	ADD_FIELD (sb2, md, "d",   7, 54, DDS_FLOAT_64_TYPE);
	ADD_FIELD (sb2, md, "ch",  8, 55, DDS_CHAR_8_TYPE);
	set_id_annotation (sb2, "ch", 5);
	set_key_annotation (sb2, "ch");
	set_id_annotation (sb2, "fl", 4);

	ot = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_BYTE_TYPE);
	fail_unless (ot != NULL);

	ostb = DDS_DynamicTypeBuilderFactory_create_sequence_type (ot, 
							DDS_UNBOUNDED_COLLECTION);
	fail_unless (ostb != NULL);

	ost = DDS_DynamicTypeBuilder_build (ostb);
	fail_unless (ost != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (ostb);

	md->name = "s";
	md->index = 9;
	md->id = 9;
	md->type = ost;
	rc = DDS_DynamicTypeBuilder_add_member (sb2, md);
	fail_unless (rc == DDS_RETCODE_OK);

	set_ext_annotation (sb2, "MUTABLE_EXTENSIBILITY");

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

	SET_FIELD (dd, 50, int32, -5);
	SET_FIELD (dd, 10, uint16, 0xDEAD);
	SET_FIELD (dd, 20, int16, INT16_MIN);
	SET_FIELD (dd, 2, uint32, UINT32_MAX);
	SET_FIELD (dd, 52, int64, 100);
	SET_FIELD (dd, 4, float32, 0.5f);
	SET_FIELD (dd, 54, float64, 100e-5);
	SET_FIELD (dd, 5, char8, 'd');
	SET_FIELD (dd, 51, uint64, 5010000);

	dds = DDS_DynamicDataFactory_create_data (ost);
	fail_unless (dds != NULL);

	DDS_SEQ_INIT (bseq);
	dds_seq_from_array (&bseq, values, sizeof (values));
	rc = DDS_DynamicData_set_byte_values (dds, 0, &bseq);
	dds_seq_cleanup (&bseq);

	fail_unless (rc == DDS_RETCODE_OK);

	SET_FIELD (dd, 9, complex, dds);

	marshallDynamic (dd, &dd2, ts);

	DDS_DynamicDataFactory_delete_data (dd);
	DDS_DynamicDataFactory_delete_data (dds);

	DDS_DynamicTypeBuilderFactory_delete_type (s2);
	DDS_DynamicTypeBuilderFactory_delete_type (ost);

	DDS_DynamicTypeSupport_delete_type_support (ts);

	set_id_annotation (NULL, NULL, 0);
	set_key_annotation (NULL, NULL);
	set_ext_annotation (NULL, NULL);

	v_printf ("success!\r\n");
}

/** struct with a sequence of strings ::
struct struct3m {
	uint16_t u16;	//@ID(5) //@Key
	sequence<string> s;
};
*/

void test_dyn_mutable2 (void)
{
	DDS_DynamicTypeSupport ts;
	DDS_DynamicTypeBuilder sb2, stb, sstb;
	DDS_DynamicType s2, st, sst;
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md;
	DDS_DynamicData dd, dd2, dds;
	DDS_ReturnCode_t rc;
	DDS_StringSeq ss;
	unsigned n;
	char *strings [] = {
		"Hi there", "blabla", "\0", NULL, 
		"got here", "not yet done", "", "num8",
		"9", "10", "11", "12",
		"13", "14", "15", "16",
		"17", "18", "19", "done :)"
	};

	v_printf ("test_mutable2 - ");

	/* 1. Create the type. */
	desc = DDS_TypeDescriptor__alloc ();
	fail_unless (desc != NULL);
	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "dstruct2m";

	sb2 = DDS_DynamicTypeBuilderFactory_create_type (desc);
	fail_unless (sb2 != NULL);

	md = DDS_MemberDescriptor__alloc ();
	fail_unless (md != NULL);

	ADD_FIELD (sb2, md, "u16", 0, 5, DDS_UINT_16_TYPE);
	set_key_annotation (sb2, "u16");

	stb = DDS_DynamicTypeBuilderFactory_create_string_type (DDS_UNBOUNDED_COLLECTION);
	fail_unless (stb != NULL);

	st = DDS_DynamicTypeBuilder_build (stb);
	fail_unless (st != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (stb);

	sstb = DDS_DynamicTypeBuilderFactory_create_sequence_type (st, 
							DDS_UNBOUNDED_COLLECTION);
	fail_unless (sstb != NULL);

	sst = DDS_DynamicTypeBuilder_build (sstb);
	fail_unless (sst != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sstb);

	md->name = "s";
	md->index = 1;
	md->id = 6;
	md->type = sst;
	rc = DDS_DynamicTypeBuilder_add_member (sb2, md);
	fail_unless (rc == DDS_RETCODE_OK);

	set_ext_annotation (sb2, "MUTABLE_EXTENSIBILITY");

	s2 = DDS_DynamicTypeBuilder_build (sb2);
	fail_unless (s2 != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (sb2);

	ts = DDS_DynamicTypeSupport_create_type_support (s2);
	fail_unless (ts != NULL);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);

	/* 2. Create a Dynamic data item for this type. */

	DDS_SEQ_INIT (ss);
	dds_seq_from_array (&ss, strings, sizeof (strings) / sizeof (char *));

	for (n = 0; n <= DDS_SEQ_MAXIMUM (ss); n++) {

		dd = DDS_DynamicDataFactory_create_data (s2);
		fail_unless (dd != NULL);

		SET_FIELD (dd, 5, uint16, n);

		dds = DDS_DynamicDataFactory_create_data (sst);
		fail_unless (dds != NULL);

		DDS_SEQ_LENGTH (ss) = n;
		rc = DDS_DynamicData_set_string_values (dds, 0, &ss);

		fail_unless (rc == DDS_RETCODE_OK);

		SET_FIELD (dd, 6, complex, dds);

		marshallDynamic (dd, &dd2, ts);

		DDS_DynamicDataFactory_delete_data (dd);
		DDS_DynamicDataFactory_delete_data (dds);
		if (n > 2) {
			n += 3;
			if (n == 18)
				n++;
		}
	}
	dds_seq_cleanup (&ss);

	DDS_DynamicTypeBuilderFactory_delete_type (s2);
	DDS_DynamicTypeBuilderFactory_delete_type (sst);
	DDS_DynamicTypeBuilderFactory_delete_type (st);

	DDS_DynamicTypeSupport_delete_type_support (ts);

	set_key_annotation (NULL, NULL);
	set_ext_annotation (NULL, NULL);

	v_printf ("success!\r\n");
}

struct struct2m {
	uint16_t u16;	//@ID(10) //@Key
	int16_t i16;	//@ID(20)
	uint32_t u32;	//@Key
	int32_t i32;	//@ID(50)
	uint64_t u64;
	int64_t i64;
	float fl;
	double d;
	char ch;	//@ID(5) //@Key
	DDS_OctetSeq s;
};

static DDS_TypeSupport_meta tsm2[] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_MUTABLE | TSMFLAG_DYNAMIC | TSMFLAG_KEY | TSMFLAG_GENID, "struct2m", sizeof(struct struct2m), 0, 10, },
	{ CDR_TYPECODE_USHORT, TSMFLAG_KEY, "u16", 0, offsetof(struct struct2m, u16), 0, 10, },
	{ CDR_TYPECODE_SHORT, 0, "i16", 0, offsetof(struct struct2m, i16), 0, 20, },
	{ CDR_TYPECODE_ULONG, TSMFLAG_KEY, "u32", 0, offsetof(struct struct2m, u32), 0, 2, },
	{ CDR_TYPECODE_LONG, 0, "i32", 0, offsetof(struct struct2m, i32), 0, 50, },
	{ CDR_TYPECODE_ULONGLONG, 0, "u64", 0, offsetof(struct struct2m, u64), 0, 51, },
	{ CDR_TYPECODE_LONGLONG, 0, "i64", 0, offsetof(struct struct2m, i64), 0, 52, },
	{ CDR_TYPECODE_FLOAT, 0, "fl", 0, offsetof(struct struct2m, fl), 0, 4, },
	{ CDR_TYPECODE_DOUBLE, 0, "d", 0, offsetof(struct struct2m, d), 0, 54, },
	{ CDR_TYPECODE_CHAR, TSMFLAG_KEY, "ch", 0, offsetof(struct struct2m, ch), 0, 5, },
	{ CDR_TYPECODE_SEQUENCE, TSMFLAG_DYNAMIC, "s", 0, offsetof (struct struct2m, s), 0, 9, },
	{ CDR_TYPECODE_OCTET, 0, "octet", 0, }
	
};

static uint32_t generate_member_id (const char* name)
{
	uint32_t ret;

	ret = crc32_char (name);

	ret &= 0x0FFFFFFF;
	if (ret < 2) {
		ret += 2;
	}
	return (ret);
}

void test_mutable_generate_id (void)
{
	DDS_TypeSupport ts;
	DDS_DynamicTypeBuilder ostb;
	DDS_DynamicType s2, ot, ost;
	DDS_DynamicData dd, dd2, dds;
	DDS_ReturnCode_t rc;
	DDS_ByteSeq bseq;
	unsigned char values [] = { 0x22, 0x33, 0x4f, 0x5e, 0x6d, 0x7c, 0x8b };

	DDS_set_generate_callback (crc32_char);
	
	v_printf ("test_mutable3 - ");
	ts = DDS_DynamicType_register (tsm2);
	fail_unless (NULL != ts);

	s2 =  DDS_DynamicTypeSupport_get_type ((DDS_DynamicTypeSupport) ts);

	/* 2. Create a Dynamic data item for this type. */
	dd = DDS_DynamicDataFactory_create_data (s2);
	fail_unless (dd != NULL);

	ot = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_BYTE_TYPE);
	fail_unless (ot != NULL);

	ostb = DDS_DynamicTypeBuilderFactory_create_sequence_type (ot, 
							DDS_UNBOUNDED_COLLECTION);
	fail_unless (ostb != NULL);

	ost = DDS_DynamicTypeBuilder_build (ostb);
	fail_unless (ost != NULL);

	DDS_DynamicTypeBuilderFactory_delete_type (ostb);

	SET_FIELD (dd, generate_member_id ("u16"), uint16, 0xDEAD);
	SET_FIELD (dd, generate_member_id ("i16"), int16, INT16_MIN);
	SET_FIELD (dd, generate_member_id ("u32"), uint32, UINT32_MAX);
	SET_FIELD (dd, generate_member_id ("i32"), int32, -5);
	SET_FIELD (dd, generate_member_id ("u64"), uint64, 5010000);
	SET_FIELD (dd, generate_member_id ("i64"), int64, 100);
	SET_FIELD (dd, generate_member_id ("fl"), float32, 0.5f);
	SET_FIELD (dd, generate_member_id ("d"), float64, 100e-5);
	SET_FIELD (dd, generate_member_id ("ch"), char8, 'd');

	dds = DDS_DynamicDataFactory_create_data (ost);
	fail_unless (dds != NULL);

	DDS_SEQ_INIT (bseq);
	dds_seq_from_array (&bseq, values, sizeof (values));
	rc = DDS_DynamicData_set_byte_values (dds, 0, &bseq);
	dds_seq_cleanup (&bseq);

	fail_unless (rc == DDS_RETCODE_OK);

	SET_FIELD (dd, generate_member_id ("s"), complex, dds);

	marshallDynamic (dd, &dd2, (DDS_DynamicTypeSupport) ts);

	DDS_DynamicDataFactory_delete_data (dd);
	DDS_DynamicDataFactory_delete_data (dds);

	DDS_DynamicTypeBuilderFactory_delete_type (s2);
	DDS_DynamicTypeBuilderFactory_delete_type (ost);

	DDS_DynamicTypeSupport_delete_type_support ((DDS_DynamicTypeSupport) ts);
	v_printf ("success!\r\n");
}

void test_dyn_mutable (void)
{
	test_dyn_mutable1 ();
	test_dyn_mutable2 ();
	test_mutable_generate_id ();
}
