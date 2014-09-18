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

/* dds_xtypes.h -- Dynamic and Extensible types API for DDS. */

#ifndef __dds_xtypes_h_
#define __dds_xtypes_h_

#include <stdint.h>
#include "dds/dds_dcps.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define DDS_MEMBER_ID_INVALID		0x0FFFFFFF
#define DDS_UNBOUNDED_COLLECTION	0

typedef struct DDS_DynamicType_st *DDS_DynamicType;
typedef struct DDS_DynamicTypeMember_st *DDS_DynamicTypeMember;
typedef struct DDS_DynamicTypeBuilder_st *DDS_DynamicTypeBuilder;
typedef struct DDS_TypeObject_st *DDS_TypeObject;
typedef char *DDS_ObjectName;

DDS_SEQUENCE (char *, DDS_IncludePathSeq);

/* === Type Descriptor ====================================================== */

typedef enum {
	DDS_NO_TYPE,

	DDS_BOOLEAN_TYPE,
	DDS_BYTE_TYPE,
	DDS_INT_16_TYPE,
	DDS_UINT_16_TYPE,
	DDS_INT_32_TYPE,
	DDS_UINT_32_TYPE,
	DDS_INT_64_TYPE,
	DDS_UINT_64_TYPE,
	DDS_FLOAT_32_TYPE,
	DDS_FLOAT_64_TYPE,
	DDS_FLOAT_128_TYPE,
	DDS_CHAR_8_TYPE,
	DDS_CHAR_32_TYPE,

	DDS_ENUMERATION_TYPE,
	DDS_BITSET_TYPE,
	DDS_ALIAS_TYPE,

	DDS_ARRAY_TYPE,
	DDS_SEQUENCE_TYPE,
	DDS_STRING_TYPE,
	DDS_MAP_TYPE,

	DDS_UNION_TYPE,
	DDS_STRUCTURE_TYPE,
	DDS_ANNOTATION_TYPE,

	DDS_TYPEKIND_MAX
} DDS_TypeKind;

DDS_SEQUENCE (uint32_t, DDS_BoundSeq);

typedef struct {
	DDS_TypeKind kind;
	DDS_ObjectName name;
	DDS_DynamicType base_type;
	DDS_DynamicType discriminator_type;
	DDS_BoundSeq bound;
	DDS_DynamicType element_type;
	DDS_DynamicType key_element_type;
} DDS_TypeDescriptor;

DDS_EXPORT DDS_TypeDescriptor *DDS_TypeDescriptor__alloc (void);
DDS_EXPORT void DDS_TypeDescriptor__free (DDS_TypeDescriptor *desc);
DDS_EXPORT void DDS_TypeDescriptor__init (DDS_TypeDescriptor *desc);
DDS_EXPORT void DDS_TypeDescriptor__reset (DDS_TypeDescriptor *desc);
DDS_EXPORT void DDS_TypeDescriptor__clear (DDS_TypeDescriptor *desc);

DDS_EXPORT DDS_ReturnCode_t DDS_TypeDescriptor_copy_from (
	DDS_TypeDescriptor *self,
	DDS_TypeDescriptor *other
);

DDS_EXPORT int DDS_TypeDescriptor_equals (
	DDS_TypeDescriptor *self,
	DDS_TypeDescriptor *other
);

DDS_EXPORT int DDS_TypeDescriptor_is_consistent (
	DDS_TypeDescriptor *self
);

/* === Dynamic Type Builder Factory ========================================= */

DDS_EXPORT DDS_DynamicType DDS_DynamicTypeBuilderFactory_get_primitive_type (
	DDS_TypeKind kind
);

DDS_EXPORT DDS_DynamicType DDS_DynamicTypeBuilderFactory_get_builtin_annotation (
	const char *name
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_type (
	DDS_TypeDescriptor *desc
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_type_copy (
	DDS_DynamicType type
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_type_w_type_object (
	DDS_TypeObject type_obj
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_string_type (
	unsigned bound
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_wstring_type (
	unsigned bound
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_sequence_type (
	DDS_DynamicType element_type,
	unsigned bound
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_array_type (
	DDS_DynamicType element_type,
	DDS_BoundSeq *bound
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_map_type (
	DDS_DynamicType key_element_type,
	DDS_DynamicType element_type,
	unsigned bound
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_bitset_type (
	unsigned bound
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_load_type_w_uri (
	const char *document_url,
	const char *type_name,
	DDS_IncludePathSeq *include_paths
);

DDS_EXPORT DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_load_type_w_document (
	const char *document,
	const char *type_name,
	DDS_IncludePathSeq *include_paths
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilderFactory_delete_type (
	void *type
);


/* === Member Descriptor ==================================================== */

DDS_SEQUENCE(int32_t, DDS_UnionCaseLabelSeq);

typedef struct {
	DDS_ObjectName name;
	DDS_MemberId id;
	DDS_DynamicType type;
	char *default_value;
	unsigned index;
	DDS_UnionCaseLabelSeq label;
	int default_label;
} DDS_MemberDescriptor;

DDS_EXPORT DDS_MemberDescriptor *DDS_MemberDescriptor__alloc (void);
DDS_EXPORT void DDS_MemberDescriptor__free (DDS_MemberDescriptor *desc);
DDS_EXPORT void DDS_MemberDescriptor__init (DDS_MemberDescriptor *desc);
DDS_EXPORT void DDS_MemberDescriptor__reset (DDS_MemberDescriptor *desc);
DDS_EXPORT void DDS_MemberDescriptor__clear (DDS_MemberDescriptor *desc);

DDS_EXPORT DDS_ReturnCode_t DDS_MemberDescriptor_copy_from (
	DDS_MemberDescriptor *self,
	DDS_MemberDescriptor *other
);

DDS_EXPORT int DDS_MemberDescriptor_equals (
	DDS_MemberDescriptor *self,
	DDS_MemberDescriptor *other
);

DDS_EXPORT int DDS_MemberDescriptor_is_consistent (
	DDS_MemberDescriptor *self
);


/* === Annotation Descriptor ================================================ */

/* Unbounded map: */
#define DDS_MAP(kname,vname,name) typedef struct { \
	kname key; vname value; } MapEntry_##kname##_##vname; \
	DDS_SEQUENCE(MapEntry_##kname##_##vname,name)

/* Bounded map (bound must be a valid non-zero number): */
#define DDS_MAP_BOUND(kname,vname,name,bound) typedef struct { \
	kname key; vname value; } MapEntry_##kname##_##vname##_##bound; \
	DDS_SEQUENCE(MapEntry_##kname##_##vname##_##bound,name)

DDS_MAP(DDS_ObjectName, DDS_ObjectName, DDS_Parameters);

DDS_EXPORT void DDS_Parameters__reset (DDS_Parameters *desc);
DDS_EXPORT void DDS_Parameters__clear (DDS_Parameters *desc);

typedef struct {
	DDS_DynamicType type;		/* Can be updated by users. */
	char		data [16];	/* -- internally used -- */
} DDS_AnnotationDescriptor;

DDS_EXPORT DDS_AnnotationDescriptor *DDS_AnnotationDescriptor__alloc (void);
DDS_EXPORT void DDS_AnnotationDescriptor__free (DDS_AnnotationDescriptor *desc);
DDS_EXPORT void DDS_AnnotationDescriptor__init (DDS_AnnotationDescriptor *desc);
DDS_EXPORT void DDS_AnnotationDescriptor__clear (DDS_AnnotationDescriptor *desc);

DDS_EXPORT DDS_ReturnCode_t DDS_AnnotationDescriptor_get_value (
	DDS_AnnotationDescriptor *self,
	DDS_ObjectName value,
	size_t max_value,
	DDS_ObjectName key
);

DDS_EXPORT DDS_ReturnCode_t DDS_AnnotationDescriptor_get_all_value (
	DDS_AnnotationDescriptor *self,
	DDS_Parameters *parameters
);

DDS_EXPORT DDS_ReturnCode_t DDS_AnnotationDescriptor_set_value (
	DDS_AnnotationDescriptor *self,
	DDS_ObjectName key,
	const char *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_AnnotationDescriptor_copy_from (
	DDS_AnnotationDescriptor *self,
	DDS_AnnotationDescriptor *other
);

DDS_EXPORT int DDS_AnnotationDescriptor_equals (
	DDS_AnnotationDescriptor *self,
	DDS_AnnotationDescriptor *other
);

DDS_EXPORT int DDS_AnnotationDescriptor_is_consistent (
	DDS_AnnotationDescriptor *self
);


/* === Dynamic Type Member ================================================== */

DDS_SEQUENCE(DDS_AnnotationDescriptor, DDS_AnnotationDescriptorSeq);

DDS_EXPORT DDS_DynamicTypeMember DDS_DynamicTypeMember__alloc (void);
DDS_EXPORT void DDS_DynamicTypeMember__free (DDS_DynamicTypeMember m);
DDS_EXPORT void DDS_DynamicTypeMember__init (DDS_DynamicTypeMember m);
DDS_EXPORT void DDS_DynamicTypeMember__clear (DDS_DynamicTypeMember m);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeMember_get_descriptor (
	DDS_DynamicTypeMember self,
	DDS_MemberDescriptor *descriptor
);

DDS_EXPORT unsigned DDS_DynamicTypeMember_get_annotation_count (
	DDS_DynamicTypeMember self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeMember_get_annotation (
	DDS_DynamicTypeMember self,
	DDS_AnnotationDescriptor *descriptor,
	unsigned index
);

DDS_EXPORT int DDS_DynamicTypeMember_equals (
	DDS_DynamicTypeMember self,
	DDS_DynamicTypeMember other
);

DDS_EXPORT DDS_MemberId DDS_DynamicTypeMember_get_id (
	DDS_DynamicTypeMember self
);

DDS_EXPORT char *DDS_DynamicTypeMember_get_name (
	DDS_DynamicTypeMember self
);


/* === Dynamic Type Builder ================================================= */

DDS_MAP(DDS_ObjectName, DDS_DynamicTypeMember, DDS_DynamicTypeMembersByName);

DDS_EXPORT void DDS_DynamicTypeMembersByName__reset (DDS_DynamicTypeMembersByName *desc);
DDS_EXPORT void DDS_DynamicTypeMembersByName__clear (DDS_DynamicTypeMembersByName *desc);

DDS_MAP(DDS_MemberId, DDS_DynamicTypeMember, DDS_DynamicTypeMembersById);

DDS_EXPORT void DDS_DynamicTypeMembersById__reset (DDS_DynamicTypeMembersById *desc);
DDS_EXPORT void DDS_DynamicTypeMembersById__clear (DDS_DynamicTypeMembersById *desc);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_descriptor (
	DDS_DynamicTypeBuilder self,
	DDS_TypeDescriptor *descriptor
);

DDS_EXPORT const char *DDS_DynamicTypeBuilder_get_name (
	DDS_DynamicTypeBuilder self
);

DDS_EXPORT DDS_TypeKind DDS_DynamicTypeBuilder_get_kind (
	DDS_DynamicTypeBuilder self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_member_by_name (
	DDS_DynamicTypeBuilder self,
	DDS_DynamicTypeMember member,
	const char *name
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_all_members_by_name (
	DDS_DynamicTypeBuilder self,
	DDS_DynamicTypeMembersByName *members
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_member (
	DDS_DynamicTypeBuilder self,
	DDS_DynamicTypeMember member,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_all_members (
	DDS_DynamicTypeBuilder self,
	DDS_DynamicTypeMembersById *members
);

DDS_EXPORT unsigned DDS_DynamicTypeBuilder_get_annotation_count (
	DDS_DynamicTypeBuilder self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_annotation (
	DDS_DynamicTypeBuilder self,
	DDS_AnnotationDescriptor *descriptor,
	unsigned index
);

DDS_EXPORT int DDS_DynamicTypeBuilder_equals (
	DDS_DynamicTypeBuilder self,
	DDS_DynamicType other
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_add_member (
	DDS_DynamicTypeBuilder self,
	DDS_MemberDescriptor *descriptor
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeBuilder_apply_annotation (
	DDS_DynamicTypeBuilder self,
	DDS_AnnotationDescriptor *descriptor
);

DDS_EXPORT DDS_DynamicType DDS_DynamicTypeBuilder_build (
	DDS_DynamicTypeBuilder self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeMember_apply_annotation (
	DDS_DynamicTypeMember self,
	DDS_AnnotationDescriptor *descriptor
);


/* === Dynamic Type ========================================================= */

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicType_get_descriptor (
	DDS_DynamicType self,
	DDS_TypeDescriptor *desc
);

DDS_EXPORT const char *DDS_DynamicType_get_name (
	DDS_DynamicType self
);

DDS_EXPORT DDS_TypeKind DDS_DynamicType_get_kind (
	DDS_DynamicType self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicType_get_member_by_name (
	DDS_DynamicType self,
	DDS_DynamicTypeMember member,
	const char *name
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicType_get_all_members_by_name (
	DDS_DynamicType self,
	DDS_DynamicTypeMembersByName *members
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicType_get_member (
	DDS_DynamicType self,
	DDS_DynamicTypeMember member,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicType_get_all_members (
	DDS_DynamicType self,
	DDS_DynamicTypeMembersById *members
);

DDS_EXPORT unsigned DDS_DynamicType_get_annotation_count (
	DDS_DynamicType self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicType_get_annotation (
	DDS_DynamicType self,
	DDS_AnnotationDescriptor *descriptor,
	unsigned index
);

DDS_EXPORT int DDS_DynamicType_equals (
	DDS_DynamicType self,
	DDS_DynamicType other
);


/* === Dynamic Type Support ================================================= */

typedef struct DDS_DynamicTypeSupport_st *DDS_DynamicTypeSupport;

DDS_EXPORT DDS_DynamicTypeSupport DDS_DynamicTypeSupport_create_type_support (
	DDS_DynamicType type
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeSupport_delete_type_support (
	DDS_DynamicTypeSupport type_support
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeSupport_register_type (
	DDS_DynamicTypeSupport self,
	DDS_DomainParticipant participant,
	const DDS_ObjectName type_signature
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicTypeSupport_unregister_type (
	DDS_DynamicTypeSupport self,
	DDS_DomainParticipant participant,
	const DDS_ObjectName type_signature
);

DDS_EXPORT DDS_ObjectName DDS_DynamicTypeSupport_get_type_name (
	DDS_DynamicTypeSupport self
);

DDS_EXPORT DDS_DynamicType DDS_DynamicTypeSupport_get_type (
	DDS_DynamicTypeSupport self
);

/* === TypeObject =========================================================== */

DDS_EXPORT DDS_TypeObject DDS_TypeObject_create_from_topic (
	DDS_DomainParticipant a_participant,
	const char *topic_name
);

DDS_EXPORT DDS_TypeObject DDS_TypeObject_create_from_key (
	DDS_DomainParticipant a_participant,
	DDS_BuiltinTopicKey_t *participant_key,
	DDS_BuiltinTopicKey_t *endpoint_key
);
	
DDS_EXPORT DDS_ReturnCode_t DDS_TypeObject_delete (
	DDS_TypeObject type_obj
);
	
/* === Dynamic Data Factory ================================================= */

typedef struct DDS_DynamicData_st *DDS_DynamicData;

DDS_EXPORT DDS_DynamicData DDS_DynamicDataFactory_create_data (
	DDS_DynamicType type
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicDataFactory_delete_data (
	DDS_DynamicData data
);

/* === Dynamic Data ========================================================= */

DDS_SEQUENCE (int32_t, DDS_Int32Seq);
DDS_SEQUENCE (uint32_t, DDS_UInt32Seq);
DDS_SEQUENCE (int16_t, DDS_Int16Seq);
DDS_SEQUENCE (uint16_t, DDS_UInt16Seq);
DDS_SEQUENCE (int64_t, DDS_Int64Seq);
DDS_SEQUENCE (uint64_t, DDS_UInt64Seq);
DDS_SEQUENCE (float, DDS_Float32Seq);
DDS_SEQUENCE (double, DDS_Float64Seq);
DDS_SEQUENCE (long double, DDS_Float128Seq);
DDS_SEQUENCE (char, DDS_CharSeq);
DDS_SEQUENCE (wchar_t, DDS_WcharSeq);
DDS_SEQUENCE (unsigned char, DDS_BooleanSeq);
#define	DDS_ByteSeq	DDS_OctetSeq	/* <- See dds_dcps.h */
/*DDS_SEQUENCE (char *, DDS_StringSeq);    <- See dds_dcps.h */
DDS_SEQUENCE (wchar_t *, DDS_WstringSeq);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_descriptor (
	DDS_DynamicData data,
	DDS_MemberDescriptor *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_descriptor (
	DDS_DynamicData data,
	DDS_MemberId id,
	DDS_MemberDescriptor *value
);

DDS_EXPORT int DDS_DynamicData_equals (
	DDS_DynamicData self,
	DDS_DynamicData other
);

DDS_EXPORT DDS_MemberId DDS_DynamicData_get_member_id_by_name (
	DDS_DynamicData self,
	DDS_ObjectName name
);

DDS_EXPORT DDS_MemberId DDS_DynamicData_get_member_id_at_index (
	DDS_DynamicData self,
	unsigned index
);

DDS_EXPORT unsigned DDS_DynamicData_get_item_count (
	DDS_DynamicData self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_clear_all_values (
	DDS_DynamicData self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_clear_nonkey_values (
	DDS_DynamicData self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_clear_value (
	DDS_DynamicData self,
	DDS_MemberId id
);

DDS_EXPORT DDS_DynamicData DDS_DynamicData_loan_value (
	DDS_DynamicData self,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_return_loaned_value (
	DDS_DynamicData self,
	DDS_DynamicData value
);

DDS_EXPORT DDS_DynamicData DDS_DynamicData_clone (
	DDS_DynamicData self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_int32_value (
	DDS_DynamicData self,
	int32_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_int32_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	int32_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_uint32_value (
	DDS_DynamicData self,
	uint32_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_uint32_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	uint32_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_int16_value (
	DDS_DynamicData self,
	int16_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_int16_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	int16_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_uint16_value (
	DDS_DynamicData self,
	uint16_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_uint16_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	uint16_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_int64_value (
	DDS_DynamicData self,
	int64_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_int64_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	int64_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_uint64_value (
	DDS_DynamicData self,
	uint64_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_uint64_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	uint64_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_float32_value (
	DDS_DynamicData self,
	float *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_float32_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	float value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_float64_value (
	DDS_DynamicData self,
	double *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_float64_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	double value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_float128_value (
	DDS_DynamicData self,
	long double *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_float128_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	long double value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_char8_value (
	DDS_DynamicData self,
	char *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_char8_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	char value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_char32_value (
	DDS_DynamicData self,
	wchar_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_char32_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	wchar_t value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_byte_value (
	DDS_DynamicData self,
	unsigned char *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_byte_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	unsigned char value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_boolean_value (
	DDS_DynamicData self,
	unsigned char *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_boolean_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	unsigned char value
);

DDS_EXPORT int DDS_DynamicData_get_string_length (
	DDS_DynamicData self,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_string_value (
	DDS_DynamicData self,
	char *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_string_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	const char *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_wstring_value (
	DDS_DynamicData self,
	wchar_t *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_wstring_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	const wchar_t *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_complex_value (
	DDS_DynamicData self,
	DDS_DynamicData *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_complex_value (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_DynamicData value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_int32_values (
	DDS_DynamicData self,
	DDS_Int32Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_int32_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_Int32Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_uint32_values (
	DDS_DynamicData self,
	DDS_UInt32Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_uint32_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_UInt32Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_int16_values (
	DDS_DynamicData self,
	DDS_Int16Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_int16_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_Int16Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_uint16_values (
	DDS_DynamicData self,
	DDS_UInt16Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_uint16_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_UInt16Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_int64_values (
	DDS_DynamicData self,
	DDS_Int64Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_int64_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_Int64Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_uint64_values (
	DDS_DynamicData self,
	DDS_UInt64Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_uint64_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_UInt64Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_float32_values (
	DDS_DynamicData self,
	DDS_Float32Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_float32_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_Float32Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_float64_values (
	DDS_DynamicData self,
	DDS_Float64Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_float64_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_Float64Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_float128_values (
	DDS_DynamicData self,
	DDS_Float128Seq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_float128_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_Float128Seq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_char8_values (
	DDS_DynamicData self,
	DDS_CharSeq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_char8_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_CharSeq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_char32_values (
	DDS_DynamicData self,
	DDS_WcharSeq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_char32_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_WcharSeq *value
);

#define	DDS_ByteSeq DDS_OctetSeq

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_byte_values (
	DDS_DynamicData self,
	DDS_ByteSeq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_byte_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_ByteSeq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_boolean_values (
	DDS_DynamicData self,
	DDS_BooleanSeq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_boolean_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_BooleanSeq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_string_values (
	DDS_DynamicData self,
	DDS_StringSeq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_string_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_StringSeq *value
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_get_wstring_values (
	DDS_DynamicData self,
	DDS_WstringSeq *value,
	DDS_MemberId id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DynamicData_set_wstring_values (
	DDS_DynamicData self,
	DDS_MemberId id,
	DDS_WstringSeq *value
);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_xtypes_h_ */

