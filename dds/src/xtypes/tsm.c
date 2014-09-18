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

/* tsm.c -- Support routines for conversion of TSM-based types to the internal
	    type representation. */

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "ctrace.h"
#include "prof.h"
#include "log.h"
#include "debug.h"
#include "pool.h"
#include "sys.h"
#include "typecode.h"
#include "xtypes.h"
#include "dds/dds_xtypes.h"
#include "dds/dds_dcps.h"
#include "tsm.h"

static uint32_t (*tsm_genid_callback)(const char *) = NULL;

static Type *tsm_create_type (TypeLib                    *lp,
			      const DDS_TypeSupport_meta **tsm,
			      unsigned                   *iflags);

/* DDS_set_generate_callback -- Assign the member_id generation callback. */

void DDS_set_generate_callback (uint32_t (*f) (const char *))
{
	tsm_genid_callback = f;
}

/* generate_member_id -- Generate a member_id of a struct/union member. */

static uint32_t generate_member_id (const DDS_TypeSupport_meta **tsm)
{
	const char	*name = (*tsm)->name;
	uint32_t	ret;

	if (!tsm_genid_callback)
		exit (1);

	ret = tsm_genid_callback (name);
	ret &= 0x0FFFFFFF;
	if (ret < 2)
		ret += 2;

	return (ret);
}

/* tsm_create_struct_union -- Create a Structure or a Union. */

Type *tsm_create_struct_union (TypeLib                    *lp,
			       const DDS_TypeSupport_meta **tsm,
			       unsigned                   *iflags)
{
	int				i, nelem, nested, key, mutable;
	Type				*tp, *tp2;
	const DDS_TypeSupport_meta	*stsm = *tsm, *tsm_ori;
	CDR_TypeCode_t			tc_ori;
	unsigned			mflags, flags = XTF_EXTENSIBLE;
	DDS_ReturnCode_t		ret;
	static unsigned			anonymous_count = 0;
	int32_t				member_id, union_label;
	char				buffer [32];
	const char			*name;

	nelem = stsm->nelem;
	if (!nelem)
		return (NULL);

	nested = (*iflags & IF_TOPLEVEL) == 0;
	if (nested) {
		snprintf (buffer, sizeof (buffer), "__%s_%u_%s",
			  (stsm->tc == CDR_TYPECODE_UNION) ? "union" : "struct",
						anonymous_count++, stsm->name);
		name = buffer;
	}
	else
		name = stsm->name;

	*iflags &= ~IF_TOPLEVEL;
	if (stsm->tc == CDR_TYPECODE_UNION) {
		tp = xt_union_type_create (lp,
					   name,
					   xt_primitive_type (DDS_INT_32_TYPE),
					   nelem,
					   stsm->size);
		if (!tp) {
			xt_lib_release (lp);
			return (NULL);
		}
		member_id = 1;
	}
	else {
		tp = xt_struct_type_create (lp, name, nelem, stsm->size);
		if (!tp) {
			xt_lib_release (lp);
			return (NULL);
		}
		member_id = 0;
	}
	mutable = stsm->flags & TSMFLAG_MUTABLE;
	for (i = 0; i < nelem; i++) {
		(*tsm)++;
		tsm_ori = *tsm;
		tc_ori = tsm_ori->tc;
		mflags = 0;
		if ((tsm_ori->flags & TSMFLAG_OPTIONAL) != 0) {
			mflags |= XMF_OPTIONAL;
			*iflags |= IF_DYNAMIC;
		}
		if ((tsm_ori->flags & TSMFLAG_SHARED) != 0) {
			mflags |= XMF_SHAREABLE;
			*iflags |= IF_DYNAMIC;
		}
		key = ((tsm_ori->flags & TSMFLAG_KEY) != 0);
		if (key)
			mflags |= XMF_KEY;

		if (tc_ori == CDR_TYPECODE_TYPEREF) {
			if (tsm_ori->tsm->tc == CDR_TYPECODE_TYPEREF) {
				xt_lib_release (lp);
				xt_type_delete (tp);
				return (NULL);
			}
			*tsm = tsm_ori->tsm;
		}
		tp2 = tsm_create_type (lp, tsm, iflags);
		if (!tp2) {
			xt_type_delete (tp);
			return (NULL);
		}
		if (mutable) {
			if ((stsm->flags & TSMFLAG_GENID) != 0)
				member_id = generate_member_id (&tsm_ori);
			else if (stsm->tc == CDR_TYPECODE_UNION &&
				 tsm_ori->nelem)
				member_id = tsm_ori->nelem;
			else if (stsm->tc == CDR_TYPECODE_STRUCT &&
				 tsm_ori->label)
				member_id = tsm_ori->label;
		}
		if (stsm->tc == CDR_TYPECODE_UNION) {
			union_label = tsm_ori->label;
			ret = xt_union_type_member_set (tp, i + 1, 1,
							&union_label,
							tsm_ori->name,
							member_id, tp2, 0,
							tsm_ori->offset);
		}
		else
			ret = xt_struct_type_member_set (tp, i,
							 tsm_ori->name,
							 member_id, 
							 tp2,
							 tsm_ori->offset);
		xt_lib_release (lp);
		if (ret) {
			xt_type_delete (tp2);
			xt_type_delete (tp);
			return (NULL);
		}
		xt_type_delete (tp2);
		xt_lib_access (lp->scope);
		if (mflags)
			xt_type_member_flags_modify (tp, i, mflags, mflags);

		if (tc_ori == CDR_TYPECODE_TYPEREF)
			*tsm = tsm_ori;

		member_id++;
	}
	if ((stsm->flags & TSMFLAG_MUTABLE) != 0)
		flags = XTF_MUTABLE;
	if ((stsm->flags & TSMFLAG_SHARED) != 0) {
		flags |= XTF_SHARED;
		*iflags |= IF_DYNAMIC;
	}
	if ((stsm->flags & TSMFLAG_DYNAMIC) != 0)
		*iflags |= IF_DYNAMIC;
	if (nested)
		flags |= XTF_NESTED;
	if (flags != XTF_EXTENSIBLE)
		xt_type_flags_modify (tp, XTF_EXT_MASK | flags, flags);

	return (tp);
}

static Type *tsm_create_enum (TypeLib                    *lp,
			      const DDS_TypeSupport_meta **tsm)
{
	int			i, nelem = (*tsm)->nelem;
	Type			*tp;
	DDS_ReturnCode_t	ret;
	static unsigned		anonymous_count = 0;
	char			buffer [32];
	unsigned		flags = XTF_NESTED;

	if (!nelem) {
		xt_lib_release (lp);
		return (NULL);
	}
	snprintf (buffer, sizeof (buffer), "__enum_%u_%s",
					anonymous_count++, (*tsm)->name);
	tp = xt_enum_type_create (lp, buffer, 32, nelem);
	if (!tp) {
		xt_lib_release (lp);
		return (NULL);
	}
	if (((*tsm)->flags & TSMFLAG_MUTABLE) != 0)
		flags |= XTF_MUTABLE;
	for (i = 0; i < nelem; i++) {
		(*tsm)++;

		ret = xt_enum_type_const_set (tp, i, (*tsm)->name, (*tsm)->label);
		if (ret) {
			xt_lib_release (lp);
			xt_type_delete (tp);
			return (NULL);
		}
	}
	xt_type_flags_modify (tp, XTF_EXT_MASK, flags);
	return (tp);
}

static size_t element_size (const DDS_TypeSupport_meta *tsm)
{
	static const size_t prim_size [] = {
		0, 2, 2, 4, 4, 8, 8, 4, 8, 
#ifdef LONGDOUBLE
		16,
#endif
		0, 1, 1, 4, 1 };

	if (tsm->size)
		return (tsm->size);

	else if (tsm->tc <= CDR_TYPECODE_OCTET)
		return (prim_size [tsm->tc]);

	else if (tsm->tc == CDR_TYPECODE_SEQUENCE)
		return (sizeof (DDS_VoidSeq));

	else if (tsm->tc == CDR_TYPECODE_CSTRING) {
		if (tsm->size)
			return (tsm->size);
		else
			return (sizeof (char *));
	}
	else if (tsm->tc == CDR_TYPECODE_WSTRING) {
		if (tsm->size)
			return (tsm->size * sizeof (wchar_t));
		else
			return (sizeof (wchar_t *));
	}
	else 
		return (~0);
}

static Type *tsm_create_sequence (TypeLib                    *lp,
				  const DDS_TypeSupport_meta **tsm,
				  unsigned                   *iflags)
{
	size_t			size, nelem = (*tsm)->nelem, esize;
	Type			*tp, *element_type;
	uint32_t		bound;
	int 			shared, keys, fksize, dkeys, dynamic;

	bound = (nelem) ? nelem : DDS_UNBOUNDED_COLLECTION;
	(*tsm)++;
	esize = element_size (*tsm);
		
	shared = (((*tsm)->flags & TSMFLAG_SHARED) != 0);
	element_type = tsm_create_type (lp, tsm, iflags);
	if (!element_type)
		return (NULL);

	tp = xt_sequence_type_create (lp, bound, element_type, esize);
	xt_lib_release (lp);
	xt_type_delete (element_type);
	if (!tp)
		return (NULL);

	xt_lib_access (lp->scope);
	if (shared)
		xt_type_flags_modify (element_type, XTF_SHARED, XTF_SHARED);
	xt_type_finalize (tp, &size, &keys, &fksize, &dkeys, &dynamic);
	return (tp);
}

static Type *tsm_create_array (TypeLib                    *lp,
			       const DDS_TypeSupport_meta **tsm,
			       unsigned                   *iflags)
{
	size_t			size, nelem = (*tsm)->nelem, esize;
	Type			*tp, *element_type;
	uint32_t		bound [9];
	unsigned		nbounds;
	int 			shared, keys, fksize, dkeys, dynamic;
	DDS_BoundSeq		bseq;

	if (!nelem) {
		xt_lib_release (lp);
		return (NULL);
	}
	for (nbounds = 0;;) {
		bound [nbounds++] = nelem;
		(*tsm)++;
		if ((*tsm)->tc != CDR_TYPECODE_ARRAY)
			break;

		if (nbounds == 9) {
			xt_lib_release (lp);
			return (NULL);
		}
		nelem = (*tsm)->nelem;
	}
	esize = element_size (*tsm);
	shared = (((*tsm)->flags & TSMFLAG_SHARED) != 0);
	element_type = tsm_create_type (lp, tsm, iflags);
	if (!element_type)
		return (NULL);

	DDS_SEQ_INIT (bseq);
	bseq._length = bseq._maximum = nbounds;
	bseq._buffer = bound;
	tp = xt_array_type_create (lp, &bseq, element_type, esize);
	xt_lib_release (lp);
	xt_type_delete (element_type);
	if (!tp)
		return (NULL);

	xt_lib_access (lp->scope);
	if (shared)
		xt_type_flags_modify (element_type, XTF_SHARED, XTF_SHARED);
	xt_type_finalize (tp, &size, &keys, &fksize, &dkeys, &dynamic);
	return (tp);
}

static Type *tsm_create_type (TypeLib                    *lp,
			      const DDS_TypeSupport_meta **tsm,
			      unsigned                   *iflags)
{
	Type			   *tp;
	unsigned		   bound;
	const DDS_TypeSupport_meta *tref_tsm;

	switch ((*tsm)->tc) {

		/* Primitive types: */
		case CDR_TYPECODE_SHORT:
			tp = xt_primitive_type (DDS_INT_16_TYPE);
			break;
		case CDR_TYPECODE_USHORT:
			tp = xt_primitive_type (DDS_UINT_16_TYPE);
			break;
		case CDR_TYPECODE_LONG:
			tp = xt_primitive_type (DDS_INT_32_TYPE);
			break;
		case CDR_TYPECODE_ULONG:
			tp = xt_primitive_type (DDS_UINT_32_TYPE);
			break;
		case CDR_TYPECODE_LONGLONG:
			tp = xt_primitive_type (DDS_INT_64_TYPE);
			break;
		case CDR_TYPECODE_ULONGLONG:
			tp = xt_primitive_type (DDS_UINT_64_TYPE);
			break;
		case CDR_TYPECODE_FLOAT:
			tp = xt_primitive_type (DDS_FLOAT_32_TYPE);
			break;
		case CDR_TYPECODE_DOUBLE:
			tp = xt_primitive_type (DDS_FLOAT_64_TYPE);
			break;
		case CDR_TYPECODE_LONGDOUBLE:
			tp = xt_primitive_type (DDS_FLOAT_128_TYPE);
			break;
		case CDR_TYPECODE_BOOLEAN:
			tp = xt_primitive_type (DDS_BOOLEAN_TYPE);
			break;
		case CDR_TYPECODE_CHAR:
			tp = xt_primitive_type (DDS_CHAR_8_TYPE);
			break;
		case CDR_TYPECODE_WCHAR:
			tp = xt_primitive_type (DDS_CHAR_32_TYPE);
			break;
		case CDR_TYPECODE_OCTET:
			tp = xt_primitive_type (DDS_BYTE_TYPE);
			break;

		/* X-Types doesn't support the FIXED type: */
		case CDR_TYPECODE_FIXED:
			warn_printf ("tsm_create_type: FIXED not supported on DDS-XTYPES!");
			xt_lib_release (lp);
			return (NULL);

		/* String types: */
		case CDR_TYPECODE_CSTRING:
		case CDR_TYPECODE_WSTRING:
			if ((*tsm)->size > 1)
				bound = (*tsm)->size - 1;
			else if (!(*tsm)->size)
				bound = 0;
			else
				return (NULL);

			if ((*tsm)->tc == CDR_TYPECODE_CSTRING)
				tp = (Type *) xt_string_type_create (lp, bound, DDS_CHAR_8_TYPE);
			else
				tp = (Type *) xt_string_type_create (lp, bound, DDS_CHAR_32_TYPE);

			if (!tp)
				xt_lib_release (lp);

			if ((*tsm)->size == 0)
				*iflags |= IF_DYNAMIC;
			break;

		/* Structure and Union types: */
		case CDR_TYPECODE_STRUCT:
		case CDR_TYPECODE_UNION:
			tp = tsm_create_struct_union (lp, tsm, iflags);
			break;

		/* Enumeration types are translated to X-Types enums now. */
		case CDR_TYPECODE_ENUM:
			tp = tsm_create_enum (lp, tsm);
			break;

		/* Sequences: */
		case CDR_TYPECODE_SEQUENCE:
			tp = tsm_create_sequence (lp, tsm, iflags);
			*iflags |= IF_DYNAMIC;
			break;

		/* Arrays: */
		case CDR_TYPECODE_ARRAY:
			tp = tsm_create_array (lp, tsm, iflags);
			break;

		/* TypeRef: */
		case CDR_TYPECODE_TYPEREF:
			tref_tsm = *tsm;
			if (tref_tsm->tsm->tc == CDR_TYPECODE_TYPEREF) {
				xt_lib_release (lp);
				return (NULL);
			}
			*tsm = tref_tsm->tsm;
			tp = tsm_create_type (lp, tsm, iflags);
			*tsm = tref_tsm;
			break;

		/* Type: */
		case CDR_TYPECODE_TYPE:
			tp = (Type *) (*tsm)->tsm;
			xt_type_ref (tp);
			break;

		/* Anything else: */
		default:
			warn_printf ("tsm_create_type: Unknown or unsupported type: %d", (*tsm)->tc);
			xt_lib_release (lp);
			return (NULL);
	}
	return (tp);
}

/* tsm_substitute_typeref -- Substitute TYPEREF fields that refer to the
			     ref_tsm field into TYPE fields that refer to the
			     real type. */ 
 
void tsm_typeref_set_type (DDS_TypeSupport_meta       *tsm,
			   unsigned                   n,
			   const Type                 *type)
{
	tsm += n;
	if (tsm->tc == CDR_TYPECODE_TYPEREF ||
	    tsm->tc == CDR_TYPECODE_TYPE) {
		tsm->tsm = (const DDS_TypeSupport_meta *) type;
		tsm->tc = CDR_TYPECODE_TYPE;
	}
}

/* tsm_create_typedef -- Create a type that is a Typedef to an existing
			 Structure or Union type or to another Typedef. */

Type *tsm_create_typedef (TypeLib                    *lp,
			  const DDS_TypeSupport_meta *tsm,
			  unsigned                   *info_flags)
{
	Type		*tp, *btp, *xtp;
	AliasType	*atp;
	StructureType	*stp;
	UnionType	*utp;

	if (!tsm ||
	    tsm->tc != CDR_TYPECODE_TYPE ||
	    !tsm->tsm)
		return (NULL);

	btp = xtp = (Type *) tsm->tsm;
	while (xtp->kind == DDS_ALIAS_TYPE) {
		atp = (AliasType *) xtp;
		xtp = xt_type_ptr (xtp->scope, atp->base_type);
		if (!xtp)
			return (NULL);
	}
	if (xtp->kind != DDS_STRUCTURE_TYPE &&
	    xtp->kind != DDS_UNION_TYPE)
		return (NULL);

	tp = xt_alias_type_create (lp, tsm->name, btp);
	if (!tp)
		return (NULL);

	if (xtp->kind == DDS_STRUCTURE_TYPE) {
		stp = (StructureType *) xtp;
		*info_flags = (stp->dynamic) ? IF_DYNAMIC : 0;
	}
	else {
		utp = (UnionType *) xtp;
		*info_flags = (utp->dynamic) ? IF_DYNAMIC : 0;
	}
	return (tp);
}

/* DDS_SampleFree -- Free the memory used up by a data sample, using the type
 		     support metadata. Only use this if the allocated memory
		     isn't consecutive. */

void DDS_SampleFree (void *sample,
		     const DDS_TypeSupport ts,
		     int full,
		     void (*free)(void *p))
{
	ARG_NOT_USED (free)

	DDS_TypeSupport_data_free (ts, sample, full);
}

/* DDS_SampleCopy -- Make a copy of a data sample. */

void *DDS_SampleCopy (const void *sample, const DDS_TypeSupport ts)
{
	return (DDS_TypeSupport_data_copy (ts, sample));
}

/* DDS_SampleEquals -- Check if two samples have an identical content and return
 		       a non-0 result if so. */

int DDS_SampleEquals (const void *sample,
		      const void *other_sample,
		      const DDS_TypeSupport ts)
{
	return (DDS_TypeSupport_data_equals (ts, sample, other_sample));
}

