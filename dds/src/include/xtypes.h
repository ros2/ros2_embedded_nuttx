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

/* xtypes.h -- Internal X-Types handling functions. */

#ifndef __xtypes_h_
#define	__xtypes_h_

#include "type_data.h"

extern const char *xt_primitive_names [],
		  *xt_idl_names [],
		  *xt_collection_names [];

int xtypes_init (void);

/* Core X-types functionality initialization. */

void xtypes_finish (void);

/* Core X-types functionality finalization. */


/* Type library operations.
   ------------------------ */

TypeLib *xt_lib_create (TypeLib *parent);

/* Create a new type library. If the parent field is non-NULL, a nested type
   library is created in order to represent nested name scopes.
   If the parent field is NULL, a new TypeDomain is created and the library will
   automatically be populated with the standard primitive types.
   When done using the library, xt_lib_release() should be called. */

void xt_lib_delete (TypeLib *lp);

/* Delete a previously created type library.  If this is the last remaining type
   library in a type domain, the type domain is automatically deleted. */

TypeLib *xt_lib_access (unsigned scope);

/* Start using the type library with the given scope. If 0 is given, the
   default type library is assumed.  When done using the library,
   xt_lib_release() should be used. */

void xt_lib_release (TypeLib *lp);

/* Indicates that the user has finished changing the type library. */

int xt_lib_lookup (TypeLib *lp, const char *name);

/* Lookup a type name in a type library.  If found, a positive library index is
   returned pointing to the entry.
   If not found, a negative index is returned that can be used (when negated)
   when creating new types. */

TypeLib *xt_lib_ptr (unsigned scope);

/* Return the type library pointer from a type scope index. */

int xt_lib_migrate (TypeLib *lp_dst,
		    int     index_dst,
		    TypeLib *lp_src,
		    TypeId  id);

/* Move a type from one type library to another within the same domain. */

Type *xt_type_ptr (unsigned scope, unsigned id);

/* Return a type pointer from a type scope and id. */

Type *xt_d2type_ptr (DynType_t *type, int builder);

/* Return a type pointer from a dynamic type. */

DynType_t *xt_dynamic_ptr (Type *type, int builder);

/* Return a new dynamic type from a type. */

void xt_type_ref (Type *tp);

/* Reference a type. */


/* Type create/delete functions.
    ---------------------------- */

Type *xt_real_type (const Type *t);

/* Get the 'real' type of a type, i.e. not an alias type but the actual type
   that is represented. */

#define xt_simple_type(k)	 ((k) <= DDS_BITSET_TYPE)

/* Check if a type is simple, i.e. can be stored directly in DynamicData. */

extern size_t xt_kind_size [];

/* Get the type size directly for simple types that are not enumerations
   or bitset types. */

size_t xt_enum_size (const Type *tp);

/* Get the type size for enumerations or bitsets. */

#define xt_simple_size(tp) (tp->kind < DDS_ENUMERATION_TYPE) ? \
			xt_kind_size [tp->kind] : xt_enum_size (tp)

/* Get the type size for any simple type, including enumerations and bitsets. */

Type *xt_primitive_type (DDS_TypeKind kind);

/* Return the primitive type corresponding with the given kind. */

Type *xt_enum_type_create (TypeLib *lp, const char *name, uint32_t bound, 
								    unsigned n);

/* Create a new Enumeration type (name, bound, n) in a type library (lp).
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned that can contain n constants that still
   need to be populated (via xt_enum_set_const()). */

DDS_ReturnCode_t xt_enum_type_const_set (Type       *tp,
				         unsigned   index,
				         const char *name,
				         int32_t    v);

/* Populate an enumeration type with an enumeration constant. */

Type *xt_bitset_type_create (TypeLib *lp, unsigned bound, unsigned n);

/* Create a new BitSet type (bound) in a type library (lp).
   Since BitSet names are inferred from the bound, they need not be specified.
   If the type already exists, the existing type is reused.  The bound must be
   in the valid range (1..64).
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned that can contain n bits that still need
   to be populated (via xt_bitset_set_bit()). */

DDS_ReturnCode_t xt_bitset_bit_set (Type       *tp,
				    unsigned   index,
				    const char *name,
				    unsigned   v);

/* Populate a bitset type with a bit value. */
 
Type *xt_alias_type_create (TypeLib *lp, const char *name, Type *base_type);

/* Create a new Alias type (name, base_type) in a type library (lp).
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned. */

Type *xt_array_type_create (TypeLib      *lp,
			    DDS_BoundSeq *bounds,
			    Type         *elem_type,
			    size_t       elem_size);

/* Create a new Array type (bounds, elem_type) in a type library (lp).
   Since Array names are inferred from their constituent parts they are unique.
   If the type already exists, the existing type is reused.
   The bounds must not be 0 and the element type (elem_type) must exist in the
   same type domain.
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned. */

Type *xt_sequence_type_create (TypeLib  *lp,
			       unsigned bound,
			       Type     *elem_type,
			       size_t   esize);

/* Create a new Sequence type (bound, elem_type) in a type library (lp).
   Since Sequence names are inferred from their constituent parts they are 
   unique.  If the type already exists, the existing type is reused.
   The element type (elem_type) must exist in the same type domain.
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned. */

Type *xt_string_type_create (TypeLib *lp, unsigned bound, DDS_TypeKind kind);

/* Create a new String type (bound, elem_type) in a type library (lp).
   Since String names are inferred from their constituent parts, they are
   unique.  If the type already exists, the existing type is reused.
   The element kind (kind) can only be DDS_CHAR_8_TYPE or DDS_CHAR_32_TYPE.
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned. */

Type *xt_map_type_create (TypeLib  *lp,
			  unsigned bound,
			  Type     *key,
			  Type     *elem);

/* Create a new Map type (bound, key, elem) in a type library (lp).
   Since Map names are inferred from their constituent parts, they are
   unique.  If the type already exists, the existing type is reused.
   There are limits as to what key and element types (key, elem) can be used
   (refer to the X-Types specification.
   If not successful, NULL is returned.  Otherwise the type is created and a
   non-NULL type pointer is returned. */

Type *xt_union_type_create (TypeLib    *lp,
			    const char *name,
			    Type       *disc,
			    unsigned   n,
			    size_t     size);

/* Create a new Union type (name, disc, n) in a type library (lp).
   If not successful, NULL is returned.  Otherwise the type is created
   as being a union with n + 1 elements, of which the discriminator element
   is the first, and the other elements still to be populated (via
   xt_union_member_set()). */

DDS_ReturnCode_t xt_union_type_member_set (Type         *up,
					   unsigned     index,
					   unsigned     nlabels,
					   int32_t      *labels,
					   const char   *name,
					   DDS_MemberId id,
					   Type         *tp,
					   int          def,
					   size_t       offset);

/* Populate a Union type with member parameters. */

Type *xt_struct_type_create (TypeLib    *lp,
			     const char *name,
			     unsigned   n,
			     size_t     size);

/* Create a new Structure type (name, n) in a type library (lp).
   If not successful, NULL is returned.  Otherwise the type is created
   as being a struct with n elements that still need to be populated (via
   xt_struct_type_member_set ()). */

DDS_ReturnCode_t xt_struct_type_member_set (Type         *sp,
					    unsigned     index,
					    const char   *name,
					    DDS_MemberId id,
					    Type         *tp,
					    size_t       offset);

/* Populate a Structure type with member parameters. */

Type *xt_annotation_type_create (TypeLib *lp, const char *name, unsigned n);

/* Create a new Annotation type (name, n) in a type library (lp).
   If not successful, NULL is returned.  Otherwise the type is created
   as being an annotation with n elements that still need to be populated (via
   xt_annotation_type_member_set ()). */

DDS_ReturnCode_t xt_annotation_type_member_set (Type         *sp,
					        unsigned     index,
					        const char   *name,
					        DDS_MemberId id,
					        Type         *tp,
						const char   *def_val);

/* Populate an Annotation type with member parameters. */

/* Type flags: */
#define	XTF_EXT_MASK	3	/* Extensibility mode mask. */
#define	XTF_FINAL	0	/* Final extensibility. */
#define	XTF_EXTENSIBLE	1	/* Extensible extensibility. */
#define	XTF_MUTABLE	2	/* Mutable extensibility. */
#define	XTF_NESTED	4	/* Nested. */
#define	XTF_SHARED	8	/* Shared. */

DDS_ReturnCode_t xt_type_flags_modify (Type *tp, unsigned mask, unsigned flags);

/* Update specific type flags. */

DDS_ReturnCode_t xt_type_flags_get (Type *tp, unsigned *flags);

/* Get the type flags. */

/* Member flags: */
#define	XMF_KEY			1	/* Is a key member. */
#define	XMF_OPTIONAL		2	/* Optional member. */
#define	XMF_SHAREABLE		4	/* Shareable. */
#define	XMF_MUST_UNDERSTAND	8	/* Must understand flag. */

#define	XMF_ALL			0xf

DDS_ReturnCode_t xt_type_member_flags_modify (Type     *tp,
					      unsigned index,
					      unsigned mask,
					      unsigned flags);

/* Update specific member flags. */

DDS_ReturnCode_t xt_type_member_flags_get (Type     *tp,
					   unsigned index,
					   unsigned *flags);

/* Get member flags. */

DDS_ReturnCode_t xt_type_delete (Type *tp);

/* Delete any previously created type. */

DDS_ReturnCode_t xt_type_finalize (Type   *tp,
				   size_t *size,
				   int    *keys,
				   int    *fksize,
				   int    *dkeys,
				   int    *dynamic);

/* Finalize a type by calculating its size and its member offsets. */

int xt_type_equal (Type *tp1, Type *tp2);

/* Compare two types for equality. */

void xt_data_free (const Type *tp, void *sample_data, int ptr);

/* Free the contents of a data sample in order to properly cleanup the dynamic
   data members.  If ptr is specified, the sample_data itself is a pointer to a
   data pointer instead of to the actual data. */

int xt_data_copy (const Type *tp,
		  void       *dst_data,
		  const void *src_data,
		  size_t     size,
		  int        ptr,
		  int        shared);

/* Copy the contents of a sample to another sample. */

int xt_data_equal (const Type *tp,
		   const void *data1,
		   const void *data2,
		   size_t     size,
		   int        ptr,
		   int        shared);

/* Compare two data samples for equality. */

void xt_dump_type (unsigned indent, Type *tp, unsigned flags);

/* Dump a type in an IDL-like notation. */

void xt_dump_lib (TypeLib *lp);

/* Dump a type library. */

void xt_type_dump (unsigned scope, const char *name, unsigned flags);

/* Dump one or more types with the given scope.  If name is empty, all types
   will be shown.  Otherwise only the named one is shown. */

#endif /* !__xtypes_h_ */

