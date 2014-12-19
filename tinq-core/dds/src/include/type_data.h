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

/* type_data.h -- The following describes the Type type and those nested
		  types on which it depends. */

#ifndef __type_data_h_
#define __type_data_h_

#include <stdint.h>
#include <stddef.h>
#include "str.h"
#include "dds/dds_xtypes.h"

/* The name of some element (e.g. type, type member, module) */
#define ELEMENT_NAME_MAX_LENGTH	256

/* Every type has an ID. Those of the primitive types are pre-defined. */
typedef unsigned TypeId;

#define NO_TYPE_ID	  DDS_NO_TYPE
#define BOOLEAN_TYPE_ID	  DDS_BOOLEAN_TYPE
#define BYTE_TYPE_ID	  DDS_BYTE_TYPE
#define INT_16_TYPE_ID	  DDS_INT_16_TYPE
#define UINT_16_TYPE_ID	  DDS_UINT_16_TYPE
#define INT_32_TYPE_ID	  DDS_INT_32_TYPE
#define UINT_32_TYPE_ID	  DDS_UINT_32_TYPE
#define INT_64_TYPE_ID	  DDS_INT_64_TYPE
#define UINT_64_TYPE_ID	  DDS_UINT_64_TYPE
#define FLOAT_32_TYPE_ID  DDS_FLOAT_32_TYPE
#define FLOAT_64_TYPE_ID  DDS_FLOAT_64_TYPE
#define FLOAT_128_TYPE_ID DDS_FLOAT_128_TYPE
#define CHAR_8_TYPE_ID	  DDS_CHAR_8_TYPE
#define CHAR_32_TYPE_ID	  DDS_CHAR_32_TYPE

/* Literal value of an annotation member: either the default value in its
 * definition or the value applied in its usage.
 */
typedef struct {
	TypeId		type;
	union {
	  int		boolean_val;	/* BOOLEAN_TYPE */
	  unsigned char	byte_val;	/* BYTE_TYPE */
	  int16_t	int_16_val;	/* INT_16_TYPE */
	  uint16_t	uint_16_val;	/* UINT_16_TYPE */
	  int32_t	int_32_val;	/* INT_32_TYPE */
	  uint32_t	uint_32_val;	/* UINT_32_TYPE */
	  int64_t	int_64_val;	/* INT_64_TYPE */
	  uint64_t	uint_64_val;	/* UINT_64_TYPE */
	  float		float_32_val;	/* FLOAT_32_TYPE */
	  double	float_64_val;	/* FLOAT_64_TYPE */
	  long double	float_128_val;	/* FLOAT_128_TYPE */
	  signed char	char_val;	/* CHAR_8_TYPE */
	  wchar_t	wide_char_val;	/* CHAR_32_TYPE */
	  int32_t	enum_val;	/* ENUMERATION_TYPE */
	  String_t	*string_val;	/* STRING_TYPE */
	}		u;
} AnnotationMemberValue;

/* The assignment of a value to a member of an annotation. */
typedef struct annotation_usage_member_st {
	DDS_MemberId		member_id;	/* Id of member. */
	AnnotationMemberValue	value;		/* Value of member. */
} AnnotationUsageMember;

/* The application of an annotation to some type or type member. */
typedef struct annotation_usage_st AnnotationUsage;
struct annotation_usage_st {
	unsigned		nrefs;
	TypeId			id;
	unsigned		nmembers;
	AnnotationUsageMember	member [1];
};

typedef struct annotation_ref_st AnnotationRef;
struct annotation_ref_st {
	AnnotationUsage	*usage;
	AnnotationRef	*next;
};

typedef enum {
	FINAL,
	EXTENSIBLE,
	MUTABLE
} Extensibility_t;

#define	T_SCOPE_MAX	127		/* Max. scope depth. */
#define	T_REFS_MAX	16383		/* Max. # of type references. */

/* Base type: */
typedef struct type_st {
	unsigned	kind:5;		/* What kind of type is it. */
	unsigned	extensible:2;	/* Extensibility mode. */
	unsigned	nested:1;	/* Nested type only, not a topic. */
	unsigned	shared:1;	/* Collection elements are shared. */
	unsigned	building:1;	/* Set while building the type. */
	unsigned	extended:1;	/* Requires extended typecode. */
	unsigned	root:1;		/* Root type definition. */
	unsigned	scope:6;	/* Scope index. */
	unsigned	nrefs:14;	/* # of type references. */
	TypeId		id;		/* Unique type index in scope. */
	AnnotationRef	*annotations;	/* Applied annotations. */
	String_t	*name;		/* Name of type. */
} Type;

/* Member of an aggregation type: */
typedef struct member_st {
	unsigned	is_key:1;
	unsigned	is_optional:1;
	unsigned	is_shareable:1;
	unsigned	must_understand:1;
	DDS_MemberId	member_id:28;
	TypeId		id;
	AnnotationRef	*annotations;
	String_t	*name;
	size_t		offset;
} Member;

typedef struct structure_type_st {
	Type		type;
	size_t		size;
	TypeId		base_type;
	unsigned	nmembers:26;
	unsigned	keyed:1;	/* Some member fields have keys. */
	unsigned	optional:1;	/* Some members are optional. */
	unsigned	keys:1;		/* Final type: contains keys. */
	unsigned	fksize:1;	/* Final type: fixed key size. */
	unsigned	dkeys:1;	/* Final type: dynamic keys. */
	unsigned	dynamic:1;	/* Final type: dynamic size. */
	Member		member [1];
} StructureType;

/* Member of a union type: */
typedef struct union_member_st {
	Member		member;
	unsigned	is_default:1;
	unsigned	nlabels:31;
	union {
	  int32_t	value;
	  int32_t	*list;
	}		label;
} UnionMember;

typedef struct union_type_st {
	Type		type;
	size_t		size;
	unsigned	nmembers:26;
	unsigned	keyed:1;	/* Some member fields have keys. */
	unsigned	keys:1;		/* Final type: contains keys. */
	unsigned	fksize:1;	/* Final type: fixed key size. */
	unsigned	dkeys:1;	/* Final type: dynamic keys. */
	unsigned	dynamic:1;	/* Final type: dynamic size. */
	UnionMember	member [1];
} UnionType;

typedef struct annotation_member_st {
	Member			member;
	AnnotationMemberValue	default_value;
} AnnotationMember;

/* Builtin annotations: */
typedef enum {
	AC_User,		/* User-defined. */
	AC_ID,			/* @ID(value) */
	AC_Optional,		/* @Optional(value:true) */
	AC_Key,			/* @Key(value:true) */
	AC_Bitbound,		/* @Bitbound(value:32) */
	AC_Value,		/* @Value(value) */
	AC_BitSet,		/* @BitSet */
	AC_Nested,		/* @Nested(value:true) */
	AC_Extensibility,	/* @Extensibility(value) where value can be: 
				     (FINAL|EXTENSIBLE|MUTABLE)_EXTENSIBILITY */
	AC_MustUnderstand,	/* @MustUnderstand(value:true) */
	AC_Verbatim,		/* @Verbatim(
					language:"*",
					placement:"before-declaration",
					text) */
	AC_Shared		/* @Shared(value:true) */
} AnnotationClass;

typedef struct annotation_type_st {
	Type		type;
	AnnotationClass	bi_class;
	unsigned	nmembers;
	AnnotationMember member [1];
} AnnotationType;

/* Alias: */
typedef struct alias_type_st {
	Type		type;
	TypeId		base_type;
} AliasType;

/* Collections: */
typedef uint32_t Bound;	/* Bound of a collection type. */

#define UNBOUNDED_COLLECTION	0

/* Base type for collection types: */
typedef struct collection_type_st {
	Type		type;
	TypeId		element_type;
	size_t		element_size;
} CollectionType;

/* Array: */
typedef struct array_type_st {
	CollectionType	collection;
	unsigned	nbounds;
	Bound		bound [1];
} ArrayType;

/* Sequence: */
typedef struct sequence_type_st {
	CollectionType	collection;
	Bound		bound;
} SequenceType;

/* Map: */
typedef SequenceType MapType;

/* String: */
typedef struct string_type_st {
	CollectionType	collection;
	Bound		bound;
} StringType;

/* Bitset: */
typedef struct bit_st {	/* Individual bit in a bit set. */
	uint32_t	index;
	String_t	*name;
} Bit;

typedef struct bitset_type_st {
	Type		type;
	unsigned short	bit_bound;
	unsigned short	nbits;
	Bit		bit [2];
} BitSetType;

/* Enumeration: */
typedef struct enum_const_st {
	int32_t		value;
	String_t	*name;
} EnumConst;

/* Enumeration type: */
typedef struct enum_type_st {
	Type		type;		/* Base type. */
	unsigned short	bound;		/* # of bits for each constant. */
	unsigned short	nconsts;	/* # of enumeration constants. */
	EnumConst	constant [2];	/* Enumeration constants. */
} EnumType;

/* Type domain: */
typedef struct type_domain_st {
	Type		**types;	/* Sparse Id to type pointer table. */
	unsigned	num_ids;	/* # of valid ids. */
	unsigned 	next_id;	/* Next Id to assign. */
	unsigned	max_ids;	/* Maximum Ids that can be assigned. */
	lock_t		lock;		/* Type domain lock. */
} TypeDomain;

/* Type list: */
typedef struct type_lib_st TypeLib;
struct type_lib_st {
	TypeLib		*next;		/* Linked list node. */
	TypeLib		*prev;
	TypeLib		*parent;	/* Parent Type Library. */
	TypeDomain	*domain;	/* Global Type domain. */
	unsigned	scope;		/* Scope id. */
	unsigned	ntypes;		/* # of types defined in scope. */
	unsigned	max_type;	/* Maximum # of types. */
	unsigned short	*types;		/* Type Id list sorted by name. */
};

/* Module type: */
typedef struct module_st {
	Type		type;		/* Module is a type. */
	TypeLib		*types;		/* All types defined within. */
} Module;

#define	DTB_MAGIC	0xd1e7e6a5	/* Type Builder magic number. */
#define	DT_MAGIC	0xd1e7e6a6	/* Dynamic Type magic number. */

/* Dynamic Type handling container: */
typedef struct DDS_DynamicType_st {
	unsigned	magic;		/* For verification purposes. */
	unsigned	nrefs;		/* # of references to dynamic type. */
	TypeDomain	*domain;	/* Owner domain. */
	unsigned	id;		/* Type handle. */
} DynType_t;

/* Dynamic Data handling: */
typedef struct dyn_data_st DynData_t;
typedef struct dyn_data_member_st DynDataMember_t;

#define	DMF_PRESENT	1	/* Field is present if set. */
#define	DMF_DYNAMIC	2	/* Field is a Dynamic Data reference. */

struct dyn_data_member_st {
	unsigned short	flags;		/* Member flags. */
	unsigned short	index;		/* Field index in struct/union. */
#if defined (BIGDATA) || (STRD_SIZE == 8)
	size_t		offset;		/* Field offset in data area. */
	size_t		length;		/* Field length in data area. */
#else
	unsigned short	offset;		/* Field offset in data area. */
	unsigned short	length;		/* Field length in data area. */
#endif
};

#if defined (BIGDATA) || (STRD_SIZE == 8)
#define	DYN_DATA_INC	32	/* Extra allocation step. */
#else
#define	DYN_DATA_INC	16	/* Extra allocation step. */
#endif

#define	DDF_LOANED	1	/* Data was loaned. */
#define	DDF_FOREIGN	2	/* Actual data storage is foreign (no free!). */
#define	DDF_CONTAINED	4	/* Data container is embedded in the struct. */
#define	DDF_DB		8	/* The DynData_t is embedded in a DB. */

struct dyn_data_st {
	Type		*type;		/* Data Type. */
	unsigned char	*dp;		/* Pointer to actual data storage. */
	unsigned short	flags;		/* Dynamic data flags. */
	unsigned short	nrefs;		/* # of dynamic data references. */
#if defined (BIGDATA) || (STRD_SIZE == 8)
	size_t		dsize;		/* Dynamic data size (if allocated). */
	size_t		dleft;		/* Dynamic data size remaining. */
#else
	unsigned short	dsize;		/* Dynamic data size (if allocated). */
	unsigned short	dleft;		/* Dynamic data size remaining. */
#endif
	/* Optional field descriptors for Unions and Structures. */
	unsigned	nfields;	/* Current # of fields. */
	DynDataMember_t	fields [1];	/* Subfield list (if significant). */
};

#define	DD_MAGIC	0xd1e7e6a7	/* Dynamic Data magic number. */

typedef struct DDS_DynamicData_st {
	unsigned	magic;		/* For verification purposes. */
	unsigned	nrefs;		/* # of Dynamic Data references. */
	DynData_t	*ddata;		/* Actual Dynamic Data. */
} DynDataRef_t;

#define	DYN_DATA_SIZE		(offsetof (DynData_t, nfields))
#define	DYN_EXTRA_AGG_SIZE(nf)	(sizeof (DynData_t) - DYN_DATA_SIZE + \
				(nf - 1) * sizeof (DynDataMember_t))

#define	TO_MAGIC	0xd1e7e6a8	/* Type object magic number. */

typedef struct DDS_TypeObject_st {
	unsigned	magic;		/* For verification purposes. */
	unsigned char	*vtc;		/* Vendor typecode if != NULL. */
	DDS_TypeSupport	ts;		/* Common TypeSupport. */
} TypeObject_t;

#endif /* __type_data_h_ */

