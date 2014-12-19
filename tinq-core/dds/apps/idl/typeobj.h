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

/* typeobj.h -- The following describes the TypeObject type and those nested
		types on which it depends. */  
    
#ifndef __typeobj_h_
#define __typeobj_h_
    
#include <stdint.h>
#include <wchar.h>
typedef void *DDS_DynamicType;
typedef char *DDS_ObjectName;
typedef int DDS_MemberId;

/*All of the kinds of types that exist in the type system. */ 
    typedef enum { NO_TYPE, /* Sentinel indicating "null" value. */ 
	    BOOLEAN_TYPE, BYTE_TYPE, INT_16_TYPE, UINT_16_TYPE, INT_32_TYPE,
	UINT_32_TYPE, INT_64_TYPE, UINT_64_TYPE, FLOAT_32_TYPE,
	FLOAT_64_TYPE, FLOAT_128_TYPE, CHAR_8_TYPE, CHAR_32_TYPE,
	ENUMERATION_TYPE, BITSET_TYPE, ALIAS_TYPE, ARRAY_TYPE,
	SEQUENCE_TYPE, STRING_TYPE, MAP_TYPE, UNION_TYPE,
	STRUCTURE_TYPE, ANNOTATION_TYPE 
} TypeKind;

/* The name of some element (e.g. type, type member, module) */ 
#define ELEMENT_NAME_MAX_LENGTH	256
    
/* Every type has an ID. Those of the primitive types are pre-defined. */ 
typedef struct type_st *TypeId;

#define NO_TYPE_ID	  NO_TYPE;
#define BOOLEAN_TYPE_ID	  BOOLEAN_TYPE;
#define BYTE_TYPE_ID	  BYTE_TYPE;
#define INT_16_TYPE_ID	  INT_16_TYPE;
#define UINT_16_TYPE_ID	  UINT_16_TYPE;
#define INT_32_TYPE_ID	  INT_32_TYPE;
#define UINT_32_TYPE_ID	  UINT_32_TYPE;
#define INT_64_TYPE_ID	  INT_64_TYPE;
#define UINT_64_TYPE_ID	  UINT_64_TYPE;
#define FLOAT_32_TYPE_ID  FLOAT_32_TYPE;
#define FLOAT_64_TYPE_ID  FLOAT_64_TYPE;
#define FLOAT_128_TYPE_ID FLOAT_128_TYPE;
#define CHAR_8_TYPE_ID	  CHAR_8_TYPE;
#define CHAR_32_TYPE_ID	  CHAR_32_TYPE;
    
/* Literal value of an annotation member: either the default value in its
 * definition or the value applied in its usage.
 */ 
    typedef struct {
	TypeKind kind;
	union {
		int boolean_val;	/*BOOLEAN_TYPE */
		unsigned char byte_val;	/* BYTE_TYPE */
		int16_t int_16_val;	/*INT_16_TYPE */
		uint16_t uint_16_val;	/* UINT_16_TYPE */
		int32_t int_32_val;	/* INT_32_TYPE */
		uint32_t uint_32_val;	/* UINT_32_TYPE */
		int64_t int_64_val;	/* INT_64_TYPE */
		uint64_t uint_64_val;	/* UINT_64_TYPE */
		float float_32_val;	/* FLOAT_32_TYPE */
		double float_64_val;	/* FLOAT_64_TYPE */
		long double float_128_val;	/* FLOAT_128_TYPE */
		signed char char_val;	/* CHAR_8_TYPE */
		wchar_t wide_char_val;	/* CHAR_32_TYPE */
		int32_t enum_val;	/* ENUMERATION_TYPE */
		wchar_t * string_val;	/* STRING_TYPE */
	} u;
} AnnotationMemberValue;

/* The assignment of a value to a member of an annotation. */ 
    typedef struct annotation_usage_member_st {
	DDS_MemberId member_id;	/* Id of member. */
	AnnotationMemberValue value;	/* Value of member. */
} AnnotationUsageMember;

/* The application of an annotation to some type or type member. */ 
typedef struct annotation_usage_st AnnotationUsage;
struct annotation_usage_st {
	AnnotationUsage * next;
	TypeId type_id;
	unsigned nmembers;
	AnnotationUsageMember member[1];
};

/* Type base class: */ 
    typedef struct type_st {
	TypeKind kind;
	unsigned is_final:1;
	unsigned is_mutable:1;
	unsigned is_nested:1;
	TypeId type_id;
	DDS_ObjectName name;
	AnnotationUsage * annotations;
} Type;

/* Aggregations: */ 
    
/* Member of an aggregation type: */ 
    typedef struct member_st {
	unsigned is_key:1;
	unsigned is_optional:1;
	unsigned is_shareable:1;
	unsigned is_union_default:1;
	DDS_MemberId member_id;
	TypeId type_id;
	DDS_ObjectName name;
	AnnotationUsage * annotations;
} Member;
typedef struct structure_type_st {
	Type type;
	TypeId base_type;
	unsigned nmembers;
	Member member[1];
} StructureType;

/* Member of a union type: */ 
    typedef struct union_member_st {
	Member member;
	unsigned nlabels;
	union {
		int32_t value;
		int32_t * list;
	} label;
} UnionMember;
typedef struct union_type_st {
	Type type;
	TypeId base_type;
	unsigned nmembers;
	UnionMember member[1];
} UnionType;
typedef struct annotation_member_st {
	Member member;
	AnnotationMemberValue default_value;
} AnnotationMember;
typedef struct annotation_type_st {
	Type type;
	TypeId base_type;
	unsigned nmembers;
	AnnotationMember member[1];
} AnnotationType;

/* Alias: */ 
    typedef struct alias_type_st {
	Type type;
	TypeId base_type;
} AliasType;

/* Collections: */ 
typedef uint32_t Bound;		/* Bound of a collection type. */

#define UNBOUNDED_COLLECTION	0
    
/* Base type for collection types: */ 
    typedef struct collection_type_st {
	Type type;
	TypeId element_type;
	int element_shared;
} CollectionType;

/* Array: */ 
    typedef struct array_type_st {
	CollectionType collection;
	unsigned nbounds;
	Bound bound[1];
} ArrayType;

/* Map: */ 
    typedef struct map_type_st {
	CollectionType collection;
	TypeId key_element_type;
	Bound bound;
} MapType;

/* Sequence: */ 
    typedef struct sequence_type_st {
	CollectionType collection;
	Bound bound;
} SequenceType;

/* String: */ 
    typedef struct string_type_st {
	CollectionType collection;
	Bound bound;
} StringType;

/* Bitset: */ 
    typedef struct bit_st {	/* Individual bit in a bit set. */
	unsigned index;
	DDS_ObjectName name;
} Bit;
typedef struct bitset_type_st {
	Type type;
	Bound bit_bound;
	unsigned nbits;
	Bit bit[1];
} BitSetType;

/* Enumeration: */ 
    typedef struct enum_const_st {
	uint32_t value;
	DDS_ObjectName name;
} EnumConst;

/* Enumeration type: */ 
    typedef struct enumeration_type_st {
	Type type;
	Bound bit_bound;
	unsigned nconsts;
	EnumConst constant[1];
} EnumerationType;

/* Module: */ 
typedef struct module_st Module;

/* Type library: */ 
/* All of the kinds of definitions that can exist in a type library. */ 
typedef enum { ALIAS_TYPE_ELEMENT = ALIAS_TYPE, ANNOTATION_TYPE_ELEMENT =
	    ANNOTATION_TYPE, ARRAY_TYPE_ELEMENT =
	    ARRAY_TYPE, BITSET_TYPE_ELEMENT =
	    BITSET_TYPE, ENUMERATION_TYPE_ELEMENT =
	    ENUMERATION_TYPE, MAP_TYPE_ELEMENT =
	    MAP_TYPE, SEQUENCE_TYPE_ELEMENT =
	    SEQUENCE_TYPE, STRING_TYPE_ELEMENT =
	    STRING_TYPE, STRUCTURE_TYPE_ELEMENT =
	    STRUCTURE_TYPE, UNION_TYPE_ELEMENT = UNION_TYPE, MODULE_ELEMENT 
} TypeLibElemKind;

/* Element that can appear in a type library or module: a type or module. */ 
    typedef struct type_library_element_st {
	TypeLibElemKind kind;
	union {
		AliasType * alias;	/*ALIAS_TYPE_ELEMENT */
		AnnotationType * annotation;	/*ANNOTATION_TYPE_ELEMENT */
		ArrayType * array_type;	/*ARRAY_TYPE_ELEMENT */
		BitSetType * bitset;	/*BITSET_TYPE_ELEMENT */
		EnumerationType * enumeration;	/*ENUMERATION_TYPE_ELEMENT */
		MapType * map;	/*MAP_TYPE_ELEMENT */
		SequenceType * sequence;	/*SEQUENCE_TYPE_ELEMENT */
		StringType * string;	/*STRING_TYPE_ELEMENT */
		StructureType * structure;	/*STRUCTURE_TYPE_ELEMENT */
		UnionType * union_type;	/*UNION_TYPE_ELEMENT */
		Module * module;	/*MODULE_ELEMENT */
	} u;
} TypeLibElem;
typedef struct type_library_st {
	unsigned nelements;	/* # of elements. */
	TypeLibElem element[1];	/* Variable # of elements. */
} TypeLibrary;
struct module_st {
	DDS_ObjectName name;
	TypeLibrary library;
};
typedef struct type_object_st {
	TypeId the_type;	/* Type id. (optional) */
	TypeLibrary library;
} TypeObject;
typedef struct _UnionMemberList  {
	UnionMember * member;
	struct _UnionMemberList *next;
} UnionMemberList;
typedef struct _MemberList  {
	Member * member;
	struct _MemberList *next;
} MemberList;
typedef struct _EnumConstList  {
	EnumConst * enumconst;
	struct _EnumConstList *next;
} EnumConstList;
typedef struct _DeclaratorList  {
	char *name;
	int nsizes;
	int *array_sizes;
	struct _DeclaratorList *next;
	AnnotationUsage * anno;
	Type * type;
} DeclaratorList;
typedef struct _DeclaratorList TypeDeclaratorList;
typedef struct  {
	int nsizes;
	int *array_sizes;
} ArraySize;
typedef enum 
    { INTEGER_KIND, STRING_KIND, WSTRING_KIND, CHAR_KIND, WCHAR_KIND,
	FIXED_KIND, FLOAT_KIND, BOOLEAN_KIND 
} LiteralKind;
typedef struct {
	LiteralKind kind;
	char *string;
} Literal;
typedef struct _LabelList  {
	int is_default;
	uint32_t label;
	struct _LabelList *next;
} LabelList;
typedef struct _TypeList  {
	Type * type;
	struct _TypeList *next;
	int idl_dumped;
	int c_dumped;
	int defsample_dumped;
} TypeList;

#endif	/* __typeobj_h_ */
