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

/* xtypes.c -- Implements the core type library. */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "error.h"
#include "list.h"
#include "debug.h"
#include "str.h"
#include "type_data.h"
#include "dds/dds_xtypes.h"
#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#include "xtypes.h"
#include "xcdr.h"
#include "xdata.h"

#ifdef _WIN32
#define INLINE
#else
#define INLINE inline
#endif

#define	INIT_REALLOC	/* Clear unused realloced regions. */
/*#define DUMP_LIB	** Dump type library when changed. */
/*#define DUMP_ATTR	** Dump attributes when dumping struct/union members. */

#define	MIN_PRIM_ID	BOOLEAN_TYPE_ID
#define	MAX_PRIM_ID	CHAR_32_TYPE_ID

#define	primitive_type(k)	((k) >= MIN_PRIM_ID && (k) <= MAX_PRIM_ID)

#define	MAX_ARRAY_BOUNDS	8

#define ALIGN_INIT(n, type) typedef struct { char pad; type name; } _align_struct_##n
#define ALIGNMENT(type) offsetof(_align_struct_##type, name)

ALIGN_INIT(int16_t, int16_t);
ALIGN_INIT(int32_t, int32_t);
ALIGN_INIT(int64_t, int64_t);
ALIGN_INIT(float, float);
ALIGN_INIT(double, double);
ALIGN_INIT(long_double, long double);
ALIGN_INIT(ptr_t, void *);

const char *xt_primitive_names [] = {
	NULL,
	"Boolean", "Byte",
	"Int16", "UInt16", "Int32", "UInt32", "Int64", "UInt64",
	"Float32", "Float64", "Float128",
	"Char8", "Char32"
};

const char *xt_idl_names [] = {
	NULL,
	"boolean", "octet",
	"short", "unsigned short", "long", "unsigned long", "long long",
	"unsigned long long", "float", "double", "long double",
	"char", "wchar"
};

const char *xt_collection_names [] = {
	NULL,
	"boolean", "octet",
	"short", "unsignedshort", "long", "unsignedlong", "longlong",
	"unsignedlonglong", "float", "double", "longdouble",
	"character", "widecharacter"
};

static Type dt_boolean  = { 
	DDS_BOOLEAN_TYPE,   FINAL, 1, 0, 0, 0, 0, 0, 0, BOOLEAN_TYPE_ID,   0, NULL
}, dt_byte = {
	DDS_BYTE_TYPE,      FINAL, 1, 0, 0, 0, 0, 0, 0, BYTE_TYPE_ID,      0, NULL
}, dt_int16 = {
	DDS_INT_16_TYPE,    FINAL, 1, 0, 0, 0, 0, 0, 0, INT_16_TYPE_ID,    0, NULL
}, dt_uint16 = {
	DDS_UINT_16_TYPE,   FINAL, 1, 0, 0, 0, 0, 0, 0, UINT_16_TYPE_ID,   0, NULL
}, dt_int32 = {
	DDS_INT_32_TYPE,    FINAL, 1, 0, 0, 0, 0, 0, 0, INT_32_TYPE_ID,    0, NULL
}, dt_uint32 = {
	DDS_UINT_32_TYPE,   FINAL, 1, 0, 0, 0, 0, 0, 0, UINT_32_TYPE_ID,   0, NULL
}, dt_int64 = {
	DDS_INT_64_TYPE,    FINAL, 1, 0, 0, 0, 0, 0, 0, INT_64_TYPE_ID,    0, NULL
}, dt_uint64 = {
	DDS_UINT_64_TYPE,   FINAL, 1, 0, 0, 0, 0, 0, 0, UINT_64_TYPE_ID,   0, NULL
}, dt_float32 = {
	DDS_FLOAT_32_TYPE,  FINAL, 1, 0, 0, 0, 0, 0, 0, FLOAT_32_TYPE_ID,  0, NULL
}, dt_float64 = {
	DDS_FLOAT_64_TYPE,  FINAL, 1, 0, 0, 0, 0, 0, 0, FLOAT_64_TYPE_ID,  0, NULL
}, dt_float128 = {
	DDS_FLOAT_128_TYPE, FINAL, 1, 0, 0, 0, 0, 0, 0, FLOAT_128_TYPE_ID, 0, NULL
}, dt_char8 = {
	DDS_CHAR_8_TYPE,    FINAL, 1, 0, 0, 0, 0, 0, 0, CHAR_8_TYPE_ID,    0, NULL
}, dt_char32 = {
	DDS_CHAR_32_TYPE,   FINAL, 1, 0, 0, 0, 0, 0, 0, CHAR_32_TYPE_ID,   0, NULL
};

static Type *primitive_types [] = {
	NULL,
	&dt_boolean, &dt_byte,
	&dt_int16, &dt_uint16, &dt_int32, &dt_uint32, &dt_int64, &dt_uint64,
	&dt_float32, &dt_float64, &dt_float128,
	&dt_char8, &dt_char32
};

static DynType_t dtr_boolean  = { DT_MAGIC, 0, NULL, BOOLEAN_TYPE_ID },
		 dtr_byte     = { DT_MAGIC, 0, NULL, BYTE_TYPE_ID },
		 dtr_int16    = { DT_MAGIC, 0, NULL, INT_16_TYPE_ID },
		 dtr_uint16   = { DT_MAGIC, 0, NULL, UINT_16_TYPE_ID },
		 dtr_int32    = { DT_MAGIC, 0, NULL, INT_32_TYPE_ID },
		 dtr_uint32   = { DT_MAGIC, 0, NULL, UINT_32_TYPE_ID },
		 dtr_int64    = { DT_MAGIC, 0, NULL, INT_64_TYPE_ID },
		 dtr_uint64   = { DT_MAGIC, 0, NULL, UINT_64_TYPE_ID },
		 dtr_float32  = { DT_MAGIC, 0, NULL, FLOAT_32_TYPE_ID },
		 dtr_float64  = { DT_MAGIC, 0, NULL, FLOAT_64_TYPE_ID },
		 dtr_float128 = { DT_MAGIC, 0, NULL, FLOAT_128_TYPE_ID },
		 dtr_char8    = { DT_MAGIC, 0, NULL, CHAR_8_TYPE_ID },
		 dtr_char32   = { DT_MAGIC, 0, NULL, CHAR_32_TYPE_ID };

static DynType_t *primitive_dtypes [] = {
	NULL,
	&dtr_boolean, &dtr_byte,
	&dtr_int16, &dtr_uint16, &dtr_int32, &dtr_uint32, &dtr_int64, &dtr_uint64,
	&dtr_float32, &dtr_float64, &dtr_float128,
	&dtr_char8, &dtr_char32
};

#define	LIBS_INC	4
#define	IDS_INC		16

#define	N_BUILTIN_ANNOTATIONS	(unsigned) AC_Shared

static DynType_t *builtin_annotations [N_BUILTIN_ANNOTATIONS];

typedef struct type_lib_list_st {
	TypeLib		*head;
	TypeLib		*tail;
} TypeLibList;

static TypeLib		**libs;
static TypeLibList 	lib_list;
static unsigned 	next_lib;
static unsigned 	high_lib;
static unsigned 	free_libs;
static TypeDomain	*def_domain;
static TypeLib		*def_lib;
static TypeLib		*dyn_lib;
static lock_t		xt_lock;	/* Global X-types library data. */

typedef enum {
	TDKind,
	TDName,
	TDBaseType,
	TDDiscrType,
	TDBound,
	TDElement,
	TDKey
} TDFieldType;

#define TDF_KIND	(1 << TDKind)
#define TDF_NAME	(1 << TDName)
#define TDF_BASE	(1 << TDBaseType)
#define TDF_DISCR	(1 << TDDiscrType)
#define TDF_BOUND	(1 << TDBound)
#define TDF_ELEMENT	(1 << TDElement)
#define TDF_KEY		(1 << TDKey)

typedef struct td_field_st {
	TDFieldType	type;
	unsigned	offset;
} TDField;

typedef struct td_fields_st {
	unsigned	fields;
	const char	*cname;
	unsigned	min_bounds;
	unsigned	max_bounds;
} TDFields;

static TDFields td_fields [] = {
/*enum*/	{ TDF_KIND | TDF_NAME | TDF_BOUND, 
		  NULL, 0, 1 },
/*bitset*/	{ TDF_KIND | TDF_NAME | TDF_BOUND,
		  NULL, 1, 1 },
/*alias*/	{ TDF_KIND | TDF_NAME | TDF_BASE,
		  NULL, 0, 0 },
/*array*/	{ TDF_KIND | TDF_NAME | TDF_BOUND | TDF_ELEMENT,
		  "array", 1, MAX_ARRAY_BOUNDS },
/*sequence*/	{ TDF_KIND | TDF_NAME | TDF_BOUND | TDF_ELEMENT,
		  "sequence", 1, 1 },
/*string*/	{ TDF_KIND | TDF_NAME | TDF_BOUND | TDF_ELEMENT,
		  "string", 1, 1 },
/*map*/		{ TDF_KIND | TDF_NAME | TDF_BOUND | TDF_ELEMENT | TDF_KEY,
		  "map", 1, 1 },
/*union*/	{ TDF_KIND | TDF_NAME | TDF_DISCR,
		  NULL, 0, 0 },
/*struct*/	{ TDF_KIND | TDF_NAME | TDF_BASE,
		  NULL, 0, 0 },
/*annotation*/	{ TDF_KIND | TDF_NAME, 
		  NULL, 0, 0 }
};


static DDS_ReturnCode_t xt_type_unref (Type *tp);

/* ------------------------------ Type Management --------------------------- */

/* xt_domain_create -- Create a new type domain. */

static TypeDomain *xt_domain_create (void)
{
	TypeDomain	*tp;

	tp = xmalloc (sizeof (TypeDomain));
	if (!tp)
		return (NULL);

	tp->types = (Type **) xmalloc (sizeof (Type *) * IDS_INC);
	if (!tp->types) {
		xfree (tp);
		return (NULL);
	}
	memset (tp->types, 0, sizeof (Type *) * IDS_INC);
	tp->num_ids = 0;
	tp->next_id = 1;
	tp->max_ids = IDS_INC;
	lock_init_nr (tp->lock, "xtdomain");
	return (tp);
}

/* DOMAIN_TYPE -- Reference a type from a domain and a type identifier. */

#define DOMAIN_TYPE(dp,id)	(dp)->types [id]

/* xt_domain_delete -- Delete an existing type domain. */

static void xt_domain_delete (TypeDomain *dp)
{
	Type		*tp;
	unsigned	i;

	for (i = dp->next_id - 1; i; i--)
		if ((tp = DOMAIN_TYPE (dp, i)) != NULL)
			xt_type_unref (tp);

	xfree (dp->types);
	lock_destroy (dp->lock);
	xfree (dp);
}

/* xt_domain_add_type -- Add a type to an existing type domain. */

static int xt_domain_add_type (TypeDomain *dp, Type *tp)
{
	unsigned	i;
	Type		**nt;

	if (dp->next_id == dp->max_ids)	/* Buffer full. */
		if (dp->num_ids < dp->max_ids - 1)
			for (i = MAX_PRIM_ID + 1; DOMAIN_TYPE (dp, i); i++)
				;
		else {
			nt = xrealloc (dp->types, (dp->max_ids + IDS_INC) *
					       		       sizeof (Type *));
			if (!nt)
				return (DDS_RETCODE_OUT_OF_RESOURCES);

			dp->types = nt;
			dp->max_ids += IDS_INC;
			i = dp->next_id++;

			/* Following is not really needed, but makes debug
			   so much easier: */
#ifdef INIT_REALLOC
			memset (&dp->types [dp->next_id], 0, 
					sizeof (Type *) * (IDS_INC - 1));
#endif
		}
	else
		i = dp->next_id++;
	DOMAIN_TYPE (dp, i) = tp;
	tp->id = i;
	dp->num_ids++;
	return (DDS_RETCODE_OK);
}

/* xt_domain_remove_type -- Remove an existing type from a type domain. */

static void xt_domain_rem_type (TypeDomain *dp, Type *tp)
{
	Type		**nt;
	unsigned	i;

	if (!tp->id || tp->id > dp->max_ids - 1 || !DOMAIN_TYPE (dp, tp->id))
		return;

	DOMAIN_TYPE (dp, tp->id) = NULL;
	dp->num_ids--;
	if (tp->id + 1 == dp->next_id) {
		for (i = dp->next_id - 2; i; i--)
			if (DOMAIN_TYPE (dp, i)) {
				dp->next_id = i + 1;
				break;
			}
	}
	tp->id = 0;
	if (dp->next_id < dp->max_ids >> 1 && dp->max_ids >= IDS_INC * 2) {
		nt = xrealloc (dp->types, (dp->max_ids << 1) * sizeof (Type *));
		if (!nt)
			return;

		dp->types = nt;
		dp->max_ids >>= 1;
	}
}

/* xt_lib_add_id -- Add a type to a type library at the given index. */

static int xt_lib_add_id (TypeLib *lp, unsigned index, TypeId id)
{
	unsigned short	*p;

	if (lp->ntypes == lp->max_type) {
		p = xrealloc (lp->types, sizeof (unsigned short) * 
						(lp->max_type + IDS_INC));
		if (!p)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		lp->types = p;
		lp->max_type += IDS_INC;

		/* Following is not really needed, but makes debug
		   so much easier: */
#ifdef INIT_REALLOC
		memset (&lp->types [lp->ntypes], 0, 
				sizeof (unsigned short) * (IDS_INC - 1));
#endif
	}
	if (lp->ntypes > index)
		memmove (&lp->types [index + 1], &lp->types [index], 
			 (lp->ntypes - index) * sizeof (unsigned short));
	lp->types [index] = id;
	lp->ntypes++;
	return (DDS_RETCODE_OK);
}

/* xt_lib_lookup -- Lookup a type name in a type library.  If found, a
		    positive index is returned pointing to the entry.
		    If not found, a negative index is returned that can
		    be used (when negated) in xt_lib_add_id(). */

int xt_lib_lookup (TypeLib *lp, const char *n)
{
	int		d;
	int		l, h, m;
	const char	*w;

	l = 0;
	h = lp->ntypes - 1;
	while (l <= h) {
		m = l + ((h - l) >> 1);
		w = str_ptr (lp->domain->types [lp->types [m]]->name);
		d = strcmp (n, w);
		if (d < 0)
			h = --m;
		else if (d > 0)
			l = ++m;
		else
			return (m);
	}
	return (-l - 1);
}

/* xt_lib_rem_id -- Remove a type from a type library. */

static void xt_lib_rem_id (TypeLib *lp, TypeId id)
{
	int	index;
	Type	*tp;

	tp = lp->domain->types [id];
	if (!tp)
		return;

	index = xt_lib_lookup (lp, str_ptr (tp->name));
	if (index < 0)
		return;

	if ((unsigned) index < lp->ntypes - 1)
		memmove (&lp->types [index], &lp->types [index + 1],
			 (lp->ntypes - index - 1) * sizeof (unsigned short));
	lp->ntypes--;
	xt_domain_rem_type (lp->domain, tp);
#ifdef DUMP_LIB
	dbg_printf ("\r\nAfter type %s deleted:\r\n", str_ptr (tp->name));
	xt_dump_lib (lp);
	if (lp != def_lib)
		xt_dump_lib (def_lib);
#endif
}

/* xt_lib_migrate -- Move a type from one type library to another within
		     the same domain. */

int xt_lib_migrate (TypeLib *lp_dst,
		    int     index_dst,
		    TypeLib *lp_src,
		    TypeId  id)
{
	int	index_src, ret;
	Type	*tp;

	if (lp_src->domain != lp_dst->domain)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = lp_src->domain->types [id];
	if (!tp)
		return (DDS_RETCODE_ALREADY_DELETED);

	index_src = xt_lib_lookup (lp_src, str_ptr (tp->name));
	if (index_src < 0)
		return (DDS_RETCODE_ALREADY_DELETED);

	ret = xt_lib_add_id (lp_dst, index_dst, id);
	if (ret)
		return (ret);

	if ((unsigned) index_src < lp_src->ntypes - 1)
		memmove (&lp_src->types [index_src], &lp_src->types [index_src + 1],
			 (lp_src->ntypes - index_src - 1) * sizeof (unsigned short));
	lp_src->ntypes--;

	/* Update type's scope after migration. */
	tp->scope = lp_dst->scope;
	return (DDS_RETCODE_OK);
}

#ifdef DUMP_LIB

/* xt_dump_lib -- Dump a type library. */

void xt_dump_lib (TypeLib *lp)
{
	unsigned	i;
	Type		*tp;

	if (!lp)
		return;

	dbg_printf ("XTypes library #%u contains(n=%u,max=%u):\r\n\t", 
						lp->scope,
						lp->ntypes,
						lp->max_type);
	for (i = 0; i < lp->ntypes; i++) {
		if (i) {
			dbg_printf (", ");
			if ((i & 7) == 0)
				dbg_printf ("\r\n\t");
		}
		tp = lp->domain->types [lp->types [i]];
		dbg_printf ("%u:%s", lp->types [i], str_ptr (tp->name));
		if (tp->nrefs != 1)
			dbg_printf ("*%u", tp->nrefs);
	}
	dbg_printf ("\r\n");
	dbg_printf ("Lib: ");
	for (i = 0; i < lp->ntypes; i++) {
		if (i)
			dbg_printf (",");
		dbg_printf ("%u", lp->types [i]);
	}
	dbg_printf ("\r\nDomain (num=%u,next=%u,max=%u): ", 
						lp->domain->num_ids,
						lp->domain->next_id,
						lp->domain->max_ids);
	for (i = 1; i < lp->domain->max_ids; i++) {
		if (i > 1)
			dbg_printf (",");
		if (((i - 1) & 7) == 0)
			dbg_printf ("\r\n\t");
		if (lp->domain->types [i])
			dbg_printf ("%u:%p", i, (void *) lp->domain->types [i]);
	}
	dbg_printf ("\r\n");
}

#endif

/* annotation_used -- Verify whether an annotation type is referenced. */

static int annotation_used (AnnotationRef *rp, unsigned id)
{
	for (; rp; rp = rp->next)
		if (rp->usage->id == id)
			return (1);
	return (0);
}

/* type_used -- Verify whether a type is used by another in a type library. */

static int type_used (TypeLib *lp, unsigned id)
{
	Type		*tp, *xtp;
	AnnotationClass	c;
	unsigned	i, xid;
	int		is_annotation = 0;

	tp = DOMAIN_TYPE (lp->domain, id);
	if (tp->kind == DDS_ANNOTATION_TYPE) {
		c = ((AnnotationType *) tp)->bi_class;
		if (c == AC_User || c == AC_Verbatim)
			is_annotation = 1;
	}
	for (i = 0; i < lp->ntypes; i++) {
		xid = lp->types [i];
		if (xid == id)
			continue;

		xtp = DOMAIN_TYPE (lp->domain, xid);
		if (is_annotation &&
		    xtp->annotations &&
		    annotation_used (xtp->annotations, id))
			return (1);

		switch (xtp->kind) {
			case DDS_ALIAS_TYPE: {
				AliasType *atp = (AliasType *) xtp;

				if (atp->base_type == id)
					return (1);

				break;
			}
			case DDS_ARRAY_TYPE:
			case DDS_SEQUENCE_TYPE:
			case DDS_MAP_TYPE: {
				CollectionType *ctp = (CollectionType *) xtp;

				if (ctp->element_type == id)
					return (1);

				break;
			}
			case DDS_UNION_TYPE: {
				UnionType	*utp = (UnionType *) xtp;
				UnionMember	*mp;
				unsigned	m;

				for (m = 0, mp = utp->member;
				     m < utp->nmembers;
				     m++, mp++) {
					if (is_annotation &&
					    mp->member.annotations &&
					    annotation_used (mp->member.annotations, id))
						return (1);

					if (mp->member.id == id)
						return (1);
				}
				break;
			}
			case DDS_STRUCTURE_TYPE: {
				StructureType	*stp = (StructureType *) xtp;
				Member		*mp;
				unsigned	m;

				for (m = 0, mp = stp->member;
				     m < stp->nmembers;
				     m++, mp++) {
					if (is_annotation &&
					    mp->annotations &&
					    annotation_used (mp->annotations, id))
						return (1);

					if (mp->id == id)
						return (1);
				}
				break;
			}
			case DDS_ANNOTATION_TYPE: {
				AnnotationType	 *atp = (AnnotationType *) xtp;
				AnnotationMember *mp;
				unsigned	 m;

				for (m = 0, mp = atp->member;
				     m < atp->nmembers;
				     m++, mp++) {
					if (is_annotation &&
					    mp->member.annotations &&
					    annotation_used (mp->member.annotations, id))
						return (1);

					if (mp->member.id == id)
						return (1);
				}
				break;
			}
			default:
			  	break;
		}
	}
	return (0);
}

/* builtin_annotation -- Check if a type is a builtin annotation type. */

static int builtin_annotation (Type *tp)
{
	AnnotationType	*atp = (AnnotationType *) tp;

	return (tp->kind == DDS_ANNOTATION_TYPE && atp->bi_class > AC_User); 
}

/* xt_lib_delete -- Delete a previously created type library. */

void xt_lib_delete (TypeLib *lp)
{
	Type		*tp;
	unsigned	id, i, n;
# if 0
	AnnotationClass	c;

	/* Remove all annotations if this is the last library. */
	if (lp == def_lib)
		for (c = AC_ID; c <= AC_Shared; c++)
			if (builtin_annotations [c]) {
				DDS_DynamicTypeBuilderFactory_delete_type (builtin_annotations [c]);
				builtin_annotations [c] = NULL;
			}
# endif
	lock_take (lp->domain->lock);

	/* Cleanup all remaining types contained in the library and notify
	   lingering types. */
	do {
		n = 0;
		for (i = 0; i < lp->ntypes; i++) {
			id = lp->types [i];
			tp = DOMAIN_TYPE (lp->domain, id);
			if (tp->kind > MAX_PRIM_ID) {
				n++;
				if (!type_used (lp, id)) {
					if (!builtin_annotation (tp))
						warn_printf ("xt_lib_delete: lingering type: '%s'",
							     str_ptr (tp->name));
					/*else
						warn_printf ("xt_lib_delete: cleanup builtin annotation: '%s'",
							     str_ptr (tp->name));*/
					xt_type_unref (tp);
					break;
				}
			}
		}
	}
	while (n);

	/* Free all string references to primitive types. */
	for (i = 0; i < lp->ntypes; i++) {
		id = lp->types [i];
		tp = DOMAIN_TYPE (lp->domain, id);
		if (tp->kind <= MAX_PRIM_ID)
			str_unref (tp->name);
	}

	/* Remove library from set of active libraries. */
	lock_take (xt_lock);
	if (lp->scope == next_lib - 1)
		next_lib--;
	else
		free_libs++;
	if (libs) {
		libs [lp->scope] = NULL;
		LIST_REMOVE (lib_list, *lp);
	}

	/* Dispose remaining library contents, including library. */
	if (lp->types)
		xfree (lp->types);

	lock_release (lp->domain->lock);

	/* Dispose domain if last type library. */
	if (!lp->parent && lp->domain) {
		xt_domain_delete (lp->domain);
		if (lp->domain == def_domain)
			def_domain = NULL;
	}
	xfree (lp);
	if (lp == def_lib) {
		def_lib = NULL;
		if (LIST_EMPTY (lib_list)) {
			next_lib = free_libs = high_lib = 0;
			xfree (libs);
			libs = NULL;
		}
	}
	lock_release (xt_lock);
}

/* xt_lib_add -- Add a type to a type library on the given index. */

static int xt_lib_add (TypeLib *lp, Type *tp, unsigned index)
{
	TypeDomain	*tdp;
	int		ret;

	tdp = lp->domain;
	ret = xt_domain_add_type (tdp, tp);
	if (ret)
		return (ret);

	ret = xt_lib_add_id (lp, index, tp->id);
	if (ret)
		xt_domain_rem_type (tdp, tp);
	else {
		tp->scope = lp->scope;
		tp->building = 1;
#ifdef DUMP_LIB
		dbg_printf ("\r\nAfter type %s added:\r\n", str_ptr (tp->name));
		xt_dump_lib (lp);
		if (lp != def_lib)
			xt_dump_lib (def_lib);
#endif
	}
	return (ret);
}

#if 0

/* xt_lib_remove -- Remove a type from a type library. */

static void xt_lib_remove (TypeLib *lp, Type *tp)
{
	xt_lib_rem_id (lp, tp->id);
}

#endif

/* xt_lib_add_primitives -- Add the primitive types to the given type
			    library. */

static int xt_lib_add_primitives (TypeLib *lp)
{
	TypeDomain	*tdp;
	Type		*tp;
	unsigned	i;
	int		index;

	tdp = lp->domain;
	for (i = MIN_PRIM_ID; i <= MAX_PRIM_ID; i++) {
		tp = primitive_types [i];
		tp->name = str_new_cstr (xt_primitive_names [i]);
		if (!tp->name)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		tdp->types [i] = tp;
		tdp->num_ids++;
		index = xt_lib_lookup (lp, str_ptr (tp->name));
		if (xt_lib_add_id (lp, -index - 1, i))
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	tdp->next_id = i;

#ifdef DUMP_LIB
	dbg_printf ("\r\nAfter add primitives:\r\n");
	xt_dump_lib (lp);
	if (lp != def_lib)
		xt_dump_lib (def_lib);
#endif
	return (DDS_RETCODE_OK);
}

/* xt_lib_create -- Create a new type library in the given type domain.
		    The parent should be used for a nested type library. */

TypeLib *xt_lib_create (TypeLib *parent)
{
	TypeLib		*lp, *pn;
	TypeLib		**nlibs;
	unsigned	scope;

	lock_take (xt_lock);

	/* Find the next scope index if there are holes. */
	if (free_libs) {
		for (scope = 0; scope < next_lib; scope++)
			if (!libs [scope])
				break;
	}

	/* Allocate/extend existing library table if necessary. */
	else if (next_lib == high_lib) {
		if (!high_lib)
			nlibs = xmalloc (sizeof (TypeLib *) * LIBS_INC);
		else
			nlibs = xrealloc (libs, sizeof (TypeLib *) *
							(high_lib + LIBS_INC));
		if (!nlibs)
			goto error;

		libs = nlibs;
		high_lib += LIBS_INC;
		scope = next_lib;
	}
	else
		scope = next_lib;

	/* Allocate a new library and initialize its contents. */
	lp = xmalloc (sizeof (TypeLib));
	if (!lp)
		goto error;

	if (!parent) {
		lp->domain = xt_domain_create ();
		if (!lp->domain)
			goto free_lp;
	}
	else
		lp->domain = parent->domain;
	lp->parent = parent;
	lp->scope = scope;
	lp->ntypes = 0;
	lp->max_type = IDS_INC;
	lp->types = (unsigned short *) xmalloc (sizeof (unsigned short) * IDS_INC);
	if (!lp->types)
		goto types_error;

	/* Library successfully created - add to list of libraries. */
	if (scope == next_lib) {
		next_lib++;
		LIST_ADD_TAIL (lib_list, *lp);
	}
	else {
		if (scope == 1)
			LIST_ADD_HEAD (lib_list, *lp);
		else {
			pn = libs [scope - 1];
			LIST_INSERT (*lp, *pn);
		}
		free_libs--;
	}
	libs [scope] = lp;

	/* If this is the top-level library, then add the primitive types. */
	if (!parent && xt_lib_add_primitives (lp) != DDS_RETCODE_OK) {
		lock_release (xt_lock);
		xt_lib_delete (lp);
		return (NULL);
	}
	lock_release (xt_lock);
	return (lp);

    types_error:
	if (!parent)
		xt_domain_delete (lp->domain);

    free_lp:
	xfree (lp);

    error:
	lock_release (xt_lock);
	return (NULL);
}

/* xt_lib_ptr -- Convert a scope index to a type library. */

TypeLib *xt_lib_ptr (unsigned scope)
{
	TypeLib	*lp;

	lock_take (xt_lock);
	if (scope < next_lib)
		lp = libs [scope];
	else
		lp = NULL;
	lock_release (xt_lock);
	return (lp);
}

/* xt_type_ptr -- Return a type pointer from a type scope and id. */

Type *xt_type_ptr (unsigned scope, unsigned id)
{
	TypeLib	*lp;
	Type	*tp;

	lock_take (xt_lock);
	if (!id || scope >= next_lib)
		return (NULL);

	lp = libs [scope];
	if (!lp || id >= lp->domain->max_ids)
		return (NULL);

	tp = DOMAIN_TYPE (lp->domain, id);
	lock_release (xt_lock);
	return (tp);
}

/* xt_domain_ptr -- Convert a scope index to a type domain. */

static TypeDomain *xt_domain_ptr (unsigned scope)
{
	TypeLib *lp;

	lock_take (xt_lock);
	if (scope >= next_lib) {
		lock_release (xt_lock);
		return (NULL);
	}
	lp = libs [scope];
	lock_release (xt_lock);
	if (!lp)
		return (NULL);
	else
		return (lp->domain);
}

/* xt_lib_access -- Start using either the type library with the given scope.
		    If 0 is given, the default type library is assumed. When
		    done using the library, xt_lib_release() should be used. */

TypeLib *xt_lib_access (unsigned scope)
{
	TypeLib		*lp;

	lock_take (xt_lock);
	if (scope >= next_lib) {
		lock_release (xt_lock);
		return (NULL);
	}
	lp = libs [scope];
	lock_release (xt_lock);
	if (!lp)
		return (NULL);

	lock_take (lp->domain->lock);
	return (lp);
}

/* xt_lib_release -- Indicates that the user has finished changing the type
		     library. */

void xt_lib_release (TypeLib *lp)
{
	lock_release (lp->domain->lock);
}

int xtypes_init (void)
{
	unsigned	i;

	lock_init_nr (xt_lock, "xt_libs");

	LIST_INIT (lib_list);

	def_lib = xt_lib_create (NULL);
	if (!def_lib)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	xt_lib_access (def_lib->scope);

	def_domain = def_lib->domain;
	for (i = MIN_PRIM_ID; i <= MAX_PRIM_ID; i++)
		primitive_dtypes [i]->domain = def_domain;

	xt_lib_release (def_lib);

	dyn_lib = xt_lib_create (def_lib);
	if (!dyn_lib)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	return (DDS_RETCODE_OK);
}

/* xtypes_finish -- Core X-types functionality finalization. */

void xtypes_finish (void)
{
	if (dyn_lib)
		xt_lib_delete (dyn_lib);

	if (def_lib)
		xt_lib_delete (def_lib);

	lock_destroy (xt_lock);
}


/* ------------------------  TypeDescriptor operations --------------------- */

DDS_TypeDescriptor *DDS_TypeDescriptor__alloc (void)
{
	DDS_TypeDescriptor	*dp;

	dp = xmalloc (sizeof (DDS_TypeDescriptor));
	if (!dp)
		return (NULL);

	DDS_TypeDescriptor__init (dp);
	return (dp);
}

void DDS_TypeDescriptor__free (DDS_TypeDescriptor *desc)
{
	if (!desc)
		return;

	/* Don't -> DDS_TypeDescriptor__clear (desc); */
	xfree (desc);
}

void DDS_TypeDescriptor__init (DDS_TypeDescriptor *desc)
{
	if (!desc)
		return;

	memset (desc, 0, sizeof (DDS_TypeDescriptor));
	desc->bound._esize = sizeof (uint32_t);
	desc->bound._own = 1;
}

static void DDS_TypeDescriptor__cleanup (DDS_TypeDescriptor *desc)
{
	if (desc->name) {
		free (desc->name);
		desc->name = NULL;
	}
	if (desc->base_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (desc->base_type);
		desc->base_type = NULL;
	}
	if (desc->discriminator_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (desc->discriminator_type);
		desc->discriminator_type = NULL;
	}
	if (desc->element_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (desc->element_type);
		desc->element_type = NULL;
	}
	if (desc->key_element_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (desc->key_element_type);
		desc->key_element_type = NULL;
	}
}

void DDS_TypeDescriptor__reset (DDS_TypeDescriptor *desc)
{
	if (!desc)
		return;

	DDS_TypeDescriptor__cleanup (desc);
	dds_seq_reset (&desc->bound);
}

void DDS_TypeDescriptor__clear (DDS_TypeDescriptor *desc)
{
	if (!desc)
		return;

	DDS_TypeDescriptor__cleanup (desc);
	dds_seq_cleanup (&desc->bound);
}

/* xt_real_type -- Return the real type of a type, i.e. not the alias. */

Type *xt_real_type (const Type *t)
{
	const Type		*tp = t;
	const AliasType		*ap;
	const TypeDomain	*dp;

	if (!tp)
		return (NULL);

	if (tp->kind != DDS_ALIAS_TYPE)
		return ((Type *) tp);

	dp = xt_domain_ptr (t->scope);
	do {
		ap = (const AliasType *) tp;
		tp = DOMAIN_TYPE (dp, ap->base_type);
	}
	while (tp->kind == DDS_ALIAS_TYPE);
	return ((Type *) tp);
}

/* xt_d2type_ptr -- Convert a Dynamic Type pointer to a real type pointer. */

Type *xt_d2type_ptr (DynType_t *dp, int builder)
{
	if (!dp)
		return (NULL);

	if ((builder && dp->magic != DTB_MAGIC) ||
	    (!builder && dp->magic != DT_MAGIC) ||
	    !dp->id ||
	    !dp->domain ||
	    dp->id >= dp->domain->max_ids)
		return (NULL);

	return (dp->domain->types [dp->id]);
}

static INLINE int xt_valid_dtype (DynType_t *dp, int builder)
{
	if (!dp)
		return (1);

	return ((builder && dp->magic == DTB_MAGIC) ||
	        (!builder && dp->magic == DT_MAGIC));
}

static INLINE void xt_dtype_ref (void *t)
{
	DynType_t	*dp;

	if (t) {
		dp = (DynType_t *) t;
		rcl_access (dp);
		dp->nrefs++;
		rcl_done (dp);
	}
}

/* DDS_TypeDescriptor_copy_from -- Copy a TypeDescriptor. */

DDS_ReturnCode_t DDS_TypeDescriptor_copy_from (DDS_TypeDescriptor *dst,
					       DDS_TypeDescriptor *src)
{
	DDS_ReturnCode_t	ret;

	if (!src || !dst || 
	    !xt_valid_dtype ((DynType_t *) src->base_type, 0) ||
	    !xt_valid_dtype ((DynType_t *) src->discriminator_type, 0) ||
	    !xt_valid_dtype ((DynType_t *) src->element_type, 0) ||
	    !xt_valid_dtype ((DynType_t *) src->key_element_type, 0))
		return (DDS_RETCODE_BAD_PARAMETER);

	ret = dds_seq_copy (&dst->bound, &src->bound);
	if (ret)
		return (ret);

	dst->kind = src->kind;
	dst->name = strdup (src->name);
	dst->base_type = src->base_type;
	dst->discriminator_type = src->discriminator_type;
	dst->element_type = src->element_type;
	dst->key_element_type = src->key_element_type;
	if (dst->base_type)
		xt_dtype_ref (dst->base_type);
	if (dst->discriminator_type)
		xt_dtype_ref (dst->discriminator_type);
	if (dst->element_type)
		xt_dtype_ref (dst->element_type);
	if (dst->key_element_type)
		xt_dtype_ref (dst->key_element_type);
	return (DDS_RETCODE_OK);
}

static int streq (char *s1, char *s2)
{
	if (!s1)
		return (!s2);
	else if (!s2)
		return (0);
	else
		return (!strcmp (s1, s2));
}

#define dt2type(dt)	xt_real_type (xt_d2type_ptr ((DynType_t *) dt, 0))

/* DDS_TypeDescriptor_equals -- Compare two TypeDescriptors. */

int DDS_TypeDescriptor_equals (DDS_TypeDescriptor *d1, DDS_TypeDescriptor *d2)
{
	if (d1->kind != d2->kind ||
	    !streq (d1->name, d2->name) ||
	    dt2type (d1->base_type) != dt2type (d2->base_type) ||
	    dt2type (d1->discriminator_type) != dt2type (d2->discriminator_type) ||
	    !dds_seq_equal (&d1->bound, &d2->bound) ||
	    dt2type (d1->element_type) != dt2type (d2->element_type) ||
	    dt2type (d1->key_element_type) != dt2type (d2->key_element_type))
		return (0);

	return (1);
}

/* valid_discriminant_type -- Check if a type is a valid discriminant type. */

#define	valid_discriminant_type(t)	((t)->kind == DDS_ENUMERATION_TYPE || \
	  ((t)->kind >= DDS_BOOLEAN_TYPE && (t)->kind <= DDS_UINT_64_TYPE) || \
	   (t)->kind == DDS_CHAR_8_TYPE || (t)->kind == DDS_CHAR_32_TYPE)

/* valid_map_key -- Check if a type is a valid map key type. */

#define	valid_map_key(t) (((t)->id >= INT_16_TYPE_ID && \
			   (t)->id <= UINT_64_TYPE_ID) || \
			  t->kind == DDS_STRING_TYPE)

/* STR_INC -- Increment a running string pointer. */

#define	STR_INC(n, buf, len)	if (n >= len) return (0); buf += n; len -= n

/* collection_name -- Generate a valid collection name based on the given
		      bounds, key type (for maps) and element type. */

static int collection_name (char       *buf,
			    size_t     len,
			    const char *prefix,
			    uint32_t   *bounds,
			    unsigned   nbounds,
			    Type       *kp,
			    Type       *ep)
{
	TypeDomain	*dp;
	StringType	*sp;
	ArrayType	*ap;
	SequenceType	*qp;
	StructureType	*stp;
	MapType		*mp;
	unsigned	n, i;
	char		*p, buf2 [80];

	p = buf;
	n = snprintf (buf, len, "%s_", prefix);
	STR_INC (n, buf, len);
	if (*bounds != 0)
		for (i = 0; i < nbounds; i++) {
			n = snprintf (buf, len, "%u_", bounds [i]);
			STR_INC (n, buf, len);
		}

	dp = xt_domain_ptr (ep->scope);
	if (kp) {
		if (kp->id >= INT_16_TYPE_ID && kp->id <= UINT_64_TYPE_ID) {
			n = snprintf (buf, len, "%s_", str_ptr (kp->name));
			STR_INC (n, buf, len);
		}
		else if (kp->kind == DDS_STRING_TYPE) {
			sp = (StringType *) kp;
			if (!collection_name (buf2, sizeof (buf2), "string",
			                      &sp->bound, 1, NULL,
					      DOMAIN_TYPE (dp,
					      	sp->collection.element_type)))
				return (0);

			n = snprintf (buf, len, "%s_", buf2);
			STR_INC (n, buf, len);
		}
		else
			return (0);
	}

	if (primitive_type (ep->id))
		n = snprintf (buf, len, "%s", xt_collection_names [ep->id]);
	else if (ep->kind == DDS_ARRAY_TYPE) {
		ap = (ArrayType *) ep;
		if (!collection_name (buf2, sizeof (buf2), "array",
				      ap->bound, ap->nbounds, NULL,
				      DOMAIN_TYPE (dp,
				      		ap->collection.element_type)))
			return (0);

		n = snprintf (buf, len, "%s", buf2);
	}
	else if (ep->kind == DDS_SEQUENCE_TYPE) {
		qp = (SequenceType *) ep;
		if (!collection_name (buf2, sizeof (buf2), "sequence",
				      &qp->bound, 1, NULL,
				      DOMAIN_TYPE (dp,
				      		qp->collection.element_type)))
			return (0);

		n = snprintf (buf, len, "%s", buf2);
	}
	else if (ep->kind == DDS_STRING_TYPE) {
		sp = (StringType *) ep;
		if (!collection_name (buf2, sizeof (buf2), "string",
				      &sp->bound, 1, NULL,
				      DOMAIN_TYPE (dp, 
				      		sp->collection.element_type)))
			return (0);

		n = snprintf (buf, len, "%s", buf2);
	}
	else if (ep->kind == DDS_MAP_TYPE) {
		mp = (MapType *) ep;
		stp = (StructureType *) DOMAIN_TYPE (dp,
						   mp->collection.element_type);
		if (!collection_name (buf2, sizeof (buf2), "map",
				      &mp->bound, 1,
				      DOMAIN_TYPE (dp, stp->member [0].id),
				      DOMAIN_TYPE (dp, stp->member [1].id)))
			return (0);

		n = snprintf (buf, len, "%s", buf2);
	}
	else
		n = snprintf (buf, len, "%s", str_ptr (ep->name));
	STR_INC (n, buf, len);
	return (buf - p);
}

static int valid_name (const char *name)
{
	unsigned	i;
	int		valid;
	char		c;

	for (i = 0; *name; i++, name++) {
		c = *name;
		if (c >= 'a' && c <= 'z')
			valid = 1;
		else if (c >= 'A' && c <= 'Z')
			valid = 1;
		else if (c >= '0' && c <= '9')
			valid = i;
		else if (c == '_' || c == '/' || c == '.' || c == ':')
			valid = 1;
		else if (c == '\\')
			valid = name [1] != '\0';
		else
			valid = 0;
		if (i > 255 || !valid)
			return (0);
	}
	return (1);
}

/* DDS_TypeDescriptor_is_consistent -- Check if a TypeDescriptor is correct. */

int DDS_TypeDescriptor_is_consistent (DDS_TypeDescriptor *td)
{
	TDFields	*fp;
	Type		*bp, *ep, *dp, *kp;
	unsigned	b, i, n;
	char		buf [128];

	if (td->kind < DDS_ENUMERATION_TYPE ||
	    td->kind > DDS_ANNOTATION_TYPE ||
	    !td->name ||
	    !valid_name (td->name))
		return (0);

	fp = &td_fields [td->kind - DDS_ENUMERATION_TYPE];

	/* Check base_type field. */
	if ((fp->fields & TDF_BASE) != 0) {
		bp = xt_real_type (xt_d2type_ptr ((DynType_t *) td->base_type, 0));
		if (td->kind == DDS_ALIAS_TYPE) {
			if (!bp)
				return (0);
		}
		else /*if (td->kind == DDS_STRUCT_TYPE)*/
			if (bp && bp->kind != DDS_STRUCTURE_TYPE)
				return (0);
	}
	else if (td->base_type)
		return (0);

	/* Check discriminator_type field. */
	if ((fp->fields & TDF_DISCR) != 0) {
		dp = xt_real_type (xt_d2type_ptr ((DynType_t *) td->discriminator_type, 0));
		if (!dp || !valid_discriminant_type (dp))
			return (0);
	}
	else if (td->discriminator_type)
		return (0);

	/* Check bound field. */
	if ((fp->fields & TDF_BOUND) != 0) {
		if (DDS_SEQ_LENGTH (td->bound) < fp->min_bounds ||
		    DDS_SEQ_LENGTH (td->bound) > fp->max_bounds)
			return (0);

		if (td->kind == DDS_ENUMERATION_TYPE) {
			if (DDS_SEQ_LENGTH (td->bound) == 1) {
				b = DDS_SEQ_ITEM (td->bound, 0);
				if (!b || b > 32)
					return (0);
			}
		}
		else if (td->kind == DDS_BITSET_TYPE) {
			b = DDS_SEQ_ITEM (td->bound, 0);
			if (!b || b > 64)
				return (0);
		}
		else if (td->kind == DDS_ARRAY_TYPE) {
			for (i = 0; i < DDS_SEQ_LENGTH (td->bound); i++)
				if (!DDS_SEQ_ITEM (td->bound, i))
					return (0);
		}
	}
	else if (DDS_SEQ_LENGTH (td->bound))
		return (0);

	/* Check element_type field. */
	if ((fp->fields & TDF_ELEMENT) != 0) {
		ep = xt_real_type (xt_d2type_ptr ((DynType_t *) td->element_type, 0));
		if (!ep)
			return (0);

		if (td->kind == DDS_STRING_TYPE) {
			if (ep->kind != DDS_CHAR_8_TYPE &&
			    ep->kind != DDS_CHAR_32_TYPE)
				return (0);
		}
		else if (td->kind == DDS_BITSET_TYPE) {
			if (ep->kind != DDS_BOOLEAN_TYPE)
				return (0);
		}
	}
	else if (td->element_type)
		return (0);
	else
		ep = NULL;

	/* Check key_element_type field. */
	if ((fp->fields & TDF_KEY) != 0) {
		kp = xt_real_type (xt_d2type_ptr ((DynType_t *) td->key_element_type, 0));
		if (!kp || !valid_map_key (kp))
			return (0);
	}
	else if (td->key_element_type)
		return (0);
	else
		kp = NULL;

	/* Extra name check for collection types. */
	if (fp->cname) { /* Check fully qualified collection name. */
		if (ep) {
			n = collection_name (buf, sizeof (buf), fp->cname,
					td->bound._buffer, td->bound._length,
					kp, ep);
			if (!n || strcmp (td->name, buf))
				return (0);
		}
		else
			return (0);
	}
	return (1);
}

/* ------------------------  Type creation operations --------------------- */

/* xt_primitive_type -- Return the primitive type corresponding with the given
			kind. */

Type *xt_primitive_type (DDS_TypeKind kind)
{
	Type	*tp;

	if (primitive_type (kind)) {
		tp = primitive_types [kind];
		return (tp);
	}
	else
		return (NULL);
}

DynType_t *xt_dynamic_ptr (Type *tp, int builder)
{
	DynType_t	*dp;

	if (!tp)
		return (NULL);

	dp = xd_dyn_type_alloc ();
	if (!dp) {
		xt_type_delete (tp);
		return (NULL);
	}
	dp->magic = (builder) ? DTB_MAGIC : DT_MAGIC;
	dp->domain = xt_domain_ptr (tp->scope);
	if (!dp->domain) {
		xt_type_delete (tp);
		xd_dyn_type_free (dp);
		return (NULL);
	}
	dp->id = tp->id;
	dp->nrefs = 1;
	return (dp);
}

/* DDS_DynamicTypeFactory_get_primitive_type -- Return a primitive type. */

DDS_DynamicType DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_TypeKind k)
{
	if (k >= MIN_PRIM_ID && k <= MAX_PRIM_ID)
		return ((DDS_DynamicType) primitive_dtypes [k]);
	else
		return (NULL);
}

/* create_enum -- Create a new Enumeration type. */

static Type *create_enum (TypeLib    *lp,
			  const char *name,
			  uint32_t   bound,
			  int        index,
			  unsigned   n)
{
	EnumType	*ep;
	size_t		s;

	s = sizeof (EnumType);
	if (n > 2)
		s += (n - 2) * sizeof (EnumConst);
	ep = xmalloc (s);
	if (!ep)
		return (NULL);

	ep->type.kind = DDS_ENUMERATION_TYPE;
	ep->type.extensible = FINAL;
	ep->type.nested = 1;
	ep->type.shared = 0;
	ep->type.building = 1;
	ep->type.extended = (bound != 32);
	ep->type.root = 0;
	ep->type.nrefs = 1;
	ep->type.name = str_new_cstr (name);
	if (!ep->type.name) {
		xfree (ep);
		return (NULL);
	}
	ep->type.annotations = 0;
	ep->bound = bound;
	ep->nconsts = n;
	if (n)
		memset (ep->constant, 0, n * sizeof (EnumConst));
	if (xt_lib_add (lp, &ep->type, index)) {
		str_unref (ep->type.name);
		xfree (ep);
		return (NULL);
	}
	return (&ep->type);
}

/* xt_enum_type_create -- Create a new enumeration type. */

Type *xt_enum_type_create (TypeLib *lp, const char *name, uint32_t bound, unsigned n)
{
	int	index;

	if (!lp || !name || !bound || !n)
		return (NULL);

	index = xt_lib_lookup (lp, name);
	if (index >= 0)
		return (NULL);

	return (create_enum (lp, name, bound, -index - 1, n));
}

/* create_bitset-- Create a new Bitset type. */

static Type *create_bitset (TypeLib    *lp,
			    const char *name,
			    uint32_t   bound,
			    int        index,
			    unsigned   n)
{
	BitSetType	*bp;
	size_t		s;

	s = sizeof (BitSetType);
	if (n > 2)
		s += (n - 2) * sizeof (Bit);
	bp = xmalloc (s);
	if (!bp)
		return (NULL);

	bp->type.kind = DDS_BITSET_TYPE;
	bp->type.extensible = FINAL;
	bp->type.nested = 1;
	bp->type.shared = 0;
	bp->type.building = 1;
	bp->type.extended = 1;
	bp->type.root = 0;
	bp->type.nrefs = 1;
	bp->type.name = str_new_cstr (name);
	if (!bp->type.name) {
		xfree (bp);
		return (NULL);
	}
	bp->type.annotations = 0;
	bp->bit_bound = bound;
	bp->nbits = 0;
	if (xt_lib_add (lp, &bp->type, index)) {
		str_unref (bp->type.name);
		xfree (bp);
		return (NULL);
	}
	return (&bp->type);
}

/* xt_bitset_type_create -- Create a new BitSet type. */

Type *xt_bitset_type_create (TypeLib *lp, uint32_t bound, unsigned n)
{
	int	index;
	char	name [64];

	if (!lp || !bound || !n)
		return (NULL);

	sprintf (name, "bitset_%u", bound);
	index = xt_lib_lookup (lp, name);
	if (index >= 0)
		return (NULL);

	return (create_bitset (lp, name, bound, -index - 1, n));
}

/* xt_type_ref -- Reference a type. */

void xt_type_ref (Type *tp)
{
	if (!primitive_type (tp->kind)) {
		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
	}
}

/* create_alias -- Create a new Alias type. */

static Type *create_alias (TypeLib    *lp,
			   const char *name,
			   Type       *base_type,
			   int        index)
{
	AliasType	*ap;
	Type		*bp;

	ap = xmalloc (sizeof (AliasType));
	if (!ap)
		return (NULL);

	bp = base_type;
	ap->type.kind = DDS_ALIAS_TYPE;
	ap->type.extensible = bp->extensible;
	ap->type.nested = bp->nested;
	ap->type.shared = 0;
	ap->type.building = 1;
	ap->type.extended = 0;
	ap->type.root = 0;
	ap->type.nrefs = 1;
	ap->type.name = str_new_cstr (name);
	if (!ap->type.name) {
		xfree (ap);
		return (NULL);
	}
	ap->type.annotations = 0;
	ap->base_type = bp->id;
	if (xt_lib_add (lp, &ap->type, index)) {
		str_unref (ap->type.name);
		xfree (ap);
		return (NULL);
	}
	xt_type_ref (bp);
	return (&ap->type);
}

/* xt_alias_type_create -- Create an alias type. */

Type *xt_alias_type_create (TypeLib *lp, const char *name, Type *base_type)
{
	Type	*tp;
	int	index;

	if (!lp || !name || !base_type)
		return (NULL);

	index = xt_lib_lookup (lp, name);
	if (index >= 0)
		return (NULL);

	tp = create_alias (lp, name, base_type, -index - 1);
	if (tp)
		tp->building = 0;
	return (tp);
}

/* create_array -- Create an Array type. */

static Type *create_array (TypeLib      *lp,
			   const char   *name, 
			   DDS_BoundSeq *bound,
			   Type         *element_type,
			   size_t       esize,
			   int          index)
{
	ArrayType	*ap;
	unsigned	i;

	ap = xmalloc (sizeof (ArrayType) + 
		      (DDS_SEQ_LENGTH (*bound) - 1) * sizeof (uint32_t));
	if (!ap)
		return (NULL);

	ap->collection.type.kind = DDS_ARRAY_TYPE;
	ap->collection.type.extensible = FINAL;
	ap->collection.type.nested = 1;
	ap->collection.type.shared = 0;
	ap->collection.type.extended = 0;
	ap->collection.type.root = 0;
	ap->collection.type.nrefs = 1;
	ap->collection.type.name = str_new_cstr (name);
	if (!ap->collection.type.name) {
		xfree (ap);
		return (NULL);
	}
	ap->collection.type.annotations = 0;
	ap->collection.element_type = element_type->id;
	ap->collection.element_size = esize;
	ap->nbounds = DDS_SEQ_LENGTH (*bound);
	for (i = 0; i < ap->nbounds; i++)
		ap->bound [i] = DDS_SEQ_ITEM (*bound, i);

	if (xt_lib_add (lp, &ap->collection.type, index)) {
		str_unref (ap->collection.type.name);
		xfree (ap);
		return (NULL);
	}
	xt_type_ref (element_type);
	return (&ap->collection.type);
}

/* create_sequence -- Create a Sequence type. */

static Type *create_sequence (TypeLib    *lp,
			      const char *name,
			      uint32_t   bound,
			      Type       *element_type,
			      size_t     esize,
			      int        index)
{
	SequenceType	*sp;

	sp = xmalloc (sizeof (SequenceType));
	if (!sp)
		return (NULL);

	sp->collection.type.kind = DDS_SEQUENCE_TYPE;
	sp->collection.type.extensible = MUTABLE;
	sp->collection.type.nested = 1;
	sp->collection.type.shared = 0;
	sp->collection.type.extended = 0;
	sp->collection.type.root = 0;
	sp->collection.type.nrefs = 1;
	sp->collection.type.name = str_new_cstr (name);
	if (!sp->collection.type.name) {
		xfree (sp);
		return (NULL);
	}
	sp->collection.type.annotations = 0;
	sp->collection.element_type = element_type->id;
	sp->collection.element_size = esize;
	sp->bound = bound;

	if (xt_lib_add (lp, &sp->collection.type, index)) {
		str_unref (sp->collection.type.name);
		xfree (sp);
		return (NULL);
	}
	xt_type_ref (element_type);
	return (&sp->collection.type);
}

/* create_string -- Create a String type. */

static Type *create_string (TypeLib    *lp,
			    const char *name,
			    uint32_t   bound,
			    Type       *element_type,
			    int        index)
{
	StringType	*sp;

	sp = xmalloc (sizeof (SequenceType));
	if (!sp)
		return (NULL);

	sp->collection.type.kind = DDS_STRING_TYPE;
	sp->collection.type.extensible = MUTABLE;
	sp->collection.type.nested = 1;
	sp->collection.type.shared = 0;
	sp->collection.type.extended = 0;
	sp->collection.type.root = 0;
	sp->collection.type.nrefs = 1;
	sp->collection.type.name = str_new_cstr (name);
	if (!sp->collection.type.name) {
		xfree (sp);
		return (NULL);
	}
	sp->collection.type.annotations = 0;
	sp->collection.element_type = element_type->id;
	sp->collection.element_size = (element_type->id == DDS_CHAR_8_TYPE) ? 1 : 4;
	sp->bound = bound;

	if (xt_lib_add (lp, &sp->collection.type, index)) {
		str_unref (sp->collection.type.name);
		xfree (sp);
		return (NULL);
	}
	xt_type_ref (element_type);
	return (&sp->collection.type);
}

/* create_map -- Create a Map type. */

static Type *create_map (TypeLib    *lp,
			 const char *name,
			 uint32_t   bound,
			 Type       *key_type,
			 Type       *element_type,
			 int        index)
{
	MapType		 *mp;
	Type	 	 *sp;
	unsigned	 n;
	size_t		 ssize;
	DDS_ReturnCode_t ret;
	char		 buf [64];

	n = snprintf (buf, sizeof (buf), "MapEntry_%s_%s",
		      str_ptr (key_type->name), str_ptr (element_type->name));
	if (!n)
		return (NULL);

	if (bound) {
		n = snprintf (buf + n, sizeof (buf) - n, "_%u", bound);
		if (!n)
			return (NULL);
	}
	sp = xt_struct_type_create (lp, buf, 2, 0);
	if (!sp)
		return (NULL);

	ret = xt_struct_type_member_set (sp, 0, "key", 0, key_type, 0);
	if (ret)
		goto struct_err;

	ret = xt_struct_type_member_set (sp, 1, "value", 1, element_type, 0);
	if (ret)
		goto struct_err;

	ret = xt_type_finalize (sp, &ssize, NULL, NULL, NULL, NULL);
	if (ret)
		goto struct_err;

	mp = xmalloc (sizeof (MapType));
	if (!mp)
		goto struct_err;

	mp->collection.type.kind = DDS_MAP_TYPE;
	mp->collection.type.extensible = MUTABLE;
	mp->collection.type.nested = 1;
	mp->collection.type.shared = 0;
	mp->collection.type.extended = 0;
	mp->collection.type.root = 0;
	mp->collection.type.nrefs = 1;
	mp->collection.type.name = str_new_cstr (name);
	if (!mp->collection.type.name)
		goto map_err;

	mp->collection.type.annotations = 0;
	mp->collection.element_type = sp->id;
	mp->collection.element_size = ssize;
	mp->bound = bound;

	if (xt_lib_add (lp, &mp->collection.type, index)) {
		str_unref (mp->collection.type.name);
		goto map_err;
	}
	xt_type_ref (element_type);
	xt_type_ref (key_type);
	return (&mp->collection.type);

    map_err:
    	xfree (mp);
    struct_err:
	xt_type_unref (sp);
	return (NULL);
}

/* create_union -- Create a Union type. */

static Type *create_union (TypeLib    *lp,
			   const char *name,
			   Type       *disc_type,
			   int        index,
			   unsigned   n,
			   size_t     size)
{
	UnionType	*up;
	size_t		s;

	s = sizeof (UnionType);
	if (n)
		s += (n + 1) * sizeof (UnionMember);
	up = xmalloc (s);
	if (!up)
		return (NULL);

	up->type.kind = DDS_UNION_TYPE;
	up->type.extensible = EXTENSIBLE;
	up->type.nested = 0;
	up->type.shared = 0;
	up->type.extended = 0;
	up->type.root = 0;
	up->type.nrefs = 1;
	up->type.name = str_new_cstr (name);
	if (!up->type.name) {
		xfree (up);
		return (NULL);
	}
	up->type.annotations = 0;
	up->size = size;
	up->nmembers = n + 1;
	up->keyed = 0;
	memset (up->member, 0, (n + 1) * sizeof (UnionMember));
	if (xt_lib_add (lp, &up->type, index))
		goto error;

	if (xt_union_type_member_set (&up->type, 0, 0, NULL, "discriminator", 0, disc_type, 0, 0))
		goto error;

	return (&up->type);

    error:
	str_unref (up->type.name);
	xfree (up);
	return (NULL);
}

/* xt_union_type_create -- Create a new Union type (name, disc_type) in a type
		           library (lp).  If not successful, NULL is returned. 
		           Otherwise the type is created as being a union with a
		           n + 1 elements, the first is the discriminator
			   element. */

Type *xt_union_type_create (TypeLib    *lp,
			    const char *name,
			    Type       *disc,
			    unsigned   n,
			    size_t     size)
{
	int	index;

	if (!lp || !name || !disc)
		return (NULL);

	index = xt_lib_lookup (lp, name);
	if (index >= 0)
		return (NULL);

	return (create_union (lp, name, disc, -index - 1, n, size));
}

static Type *create_struct (TypeLib       *lp,
			    const char    *name,
			    int           index,
			    unsigned      n,
			    size_t        size,
			    Type          *base)
{
	StructureType	*sp;
	size_t		s;

	if (base && base->kind != DDS_STRUCTURE_TYPE)
		return (NULL);

	s = sizeof (StructureType);
	if (n > 1)
		s += (n - 1) * sizeof (Member);

	sp = xmalloc (s);
	if (!sp)
		return (NULL);

	sp->type.kind = DDS_STRUCTURE_TYPE;
	sp->type.extensible = EXTENSIBLE;
	sp->type.nested = 0;
	sp->type.shared = 0;
	sp->type.extended = (base != NULL);
	sp->type.root = 0;
	sp->type.nrefs = 1;
	sp->type.name = str_new_cstr (name);
	if (!sp->type.name) {
		xfree (sp);
		return (NULL);
	}
	sp->type.annotations = 0;
	sp->base_type = (base) ? base->id : 0;
	sp->size = size;
	sp->nmembers = n;
	sp->keyed = 0;
	sp->optional = 0;
	if (n)
		memset (sp->member, 0, n * sizeof (Member));
	if (xt_lib_add (lp, &sp->type, index)) {
		str_unref (sp->type.name);
		xfree (sp);
		return (NULL);
	}
	return (&sp->type);
}

/* xt_struct_type_create -- Create a new Structure type (name) in a type
		            library (lp).  If not successful, NULL is returned.
		            Otherwise the type is created as being an empty
		            Structure type. */

Type *xt_struct_type_create (TypeLib    *lp,
			     const char *name,
			     unsigned   n,
			     size_t     size)
{
	int	index;

	if (!lp || !name)
		return (NULL);

	index = xt_lib_lookup (lp, name);
	if (index >= 0)
		return (NULL);

	return (create_struct (lp, name, -index - 1, n, size, NULL));
}

static Type *create_annotation (TypeLib *lp, const char *name,
							  int index, unsigned n)
{
	AnnotationType	*ap;
	size_t		s;

	s = sizeof (AnnotationType);
	if (n > 1)
		s += (n - 1) * sizeof (AnnotationMember);

	ap = xmalloc (s);
	if (!ap)
		return (NULL);

	ap->type.kind = DDS_ANNOTATION_TYPE;
	ap->type.extensible = EXTENSIBLE;
	ap->type.nested = 0;
	ap->type.shared = 0;
	ap->type.extended = 1;
	ap->type.root = 0;
	ap->type.nrefs = 1;
	ap->type.name = str_new_cstr (name);
	if (!ap->type.name) {
		xfree (ap);
		return (NULL);
	}
	ap->type.annotations = 0;
	ap->bi_class = AC_User;
	ap->nmembers = n;
	if (n)
		memset (ap->member, 0, n * sizeof (AnnotationMember));
	if (xt_lib_add (lp, &ap->type, index)) {
		str_unref (ap->type.name);
		xfree (ap);
		return (NULL);
	}
	return (&ap->type);
}

/* xt_annotation_type_create -- Create a new Annotation type (name, n) in a type
				library (lp). If not successful, NULL is
				returned.  Otherwise the type is created as
				being an annotation with n elements that still
				need to be populated. */

Type *xt_annotation_type_create (TypeLib *lp, const char *name, unsigned n)
{
	int	index;

	if (!lp || !name)
		return (NULL);

	index = xt_lib_lookup (lp, name);
	if (index >= 0)
		return (NULL);

	return (create_annotation (lp, name, -index - 1, n));
}

Type *xt_create_type (TypeLib *lp, DDS_TypeDescriptor *desc)
{
	Type	*tp;
	int	index;

	if (!desc ||
	    !DDS_TypeDescriptor_is_consistent (desc) ||
	    (lp = xt_lib_access ((lp) ? lp->scope : 0)) == NULL)
		return (NULL);

	if ((index = xt_lib_lookup (lp, desc->name)) >= 0) {
		xt_lib_release (lp);
		return (NULL);
	}
	index = -index - 1;
	switch (desc->kind) {
		case DDS_ENUMERATION_TYPE:
			tp = create_enum (lp, desc->name,
					  !DDS_SEQ_LENGTH (desc->bound) ? 32 :
					  	DDS_SEQ_ITEM (desc->bound, 0),
					  index, 0);
			break;
		case DDS_BITSET_TYPE:
			tp = create_bitset(lp, desc->name,
					    DDS_SEQ_ITEM (desc->bound, 0),
					    index, 0);
			break;
		case DDS_ALIAS_TYPE:
			tp = create_alias (lp, desc->name, 
					   xt_d2type_ptr ((DynType_t *) desc->base_type, 0),
					   index);
			break;
		case DDS_ARRAY_TYPE:
			tp = create_array (lp, desc->name, &desc->bound,
					   xt_d2type_ptr ((DynType_t *) desc->element_type, 0),
					   0, index);
			break;
		case DDS_SEQUENCE_TYPE:
			tp = create_sequence (lp, desc->name,
					      DDS_SEQ_ITEM (desc->bound, 0),
					      xt_d2type_ptr ((DynType_t *) desc->element_type, 0),
					      0, index);
			break;
		case DDS_STRING_TYPE:
			tp = create_string (lp, desc->name,
					    DDS_SEQ_ITEM (desc->bound, 0),
					    xt_d2type_ptr ((DynType_t *) desc->element_type, 0),
					    index);
			break;
		case DDS_MAP_TYPE:
			tp = create_map (lp, desc->name,
					 DDS_SEQ_ITEM (desc->bound, 0),
					 xt_d2type_ptr ((DynType_t *) desc->key_element_type, 0),
					 xt_d2type_ptr ((DynType_t *) desc->element_type, 0),
					 index);
			break;
		case DDS_UNION_TYPE:
			tp = create_union (lp, desc->name,
					   xt_d2type_ptr ((DynType_t *) desc->discriminator_type, 0),
					   index, 0, 0);
			break;
		case DDS_STRUCTURE_TYPE:
			tp = create_struct (lp, desc->name, index, 0, 0,
					    xt_d2type_ptr ((DynType_t *) desc->base_type, 0));
			break;
		case DDS_ANNOTATION_TYPE:
			tp = create_annotation (lp, desc->name, index, 0);
			break;
		default:
			tp = NULL;
			break;
	}
	xt_lib_release (lp);
	return (tp);
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_type (
						       DDS_TypeDescriptor *desc)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (
					xt_create_type (dyn_lib, desc), 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_type_copy (
							DDS_DynamicType type)
{
	Type		*tp;
	DynType_t	*ntp;

	tp = xt_d2type_ptr ((DynType_t *) type, 0);
	if (!tp)
		return (NULL);

	ntp = xt_dynamic_ptr (tp, 1);
	if (!ntp)
		return (NULL);

	xt_type_ref (tp);
	return ((DDS_DynamicTypeBuilder) ntp);
}

#ifdef DDS_TYPECODE

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_type_w_type_object (
						       DDS_TypeObject tobj)
{
	TypeObject_t	*top = (TypeObject_t *) tobj;
	PL_TypeSupport	*pl;
	Type		*tp;
	DynType_t	*dtp;
	unsigned char	*vtc;

	if (!top || top->magic != TO_MAGIC)
		return (NULL);

	if (!top->vtc && !top->ts)
		return (NULL);

	if (!top->ts || top->ts->ts_prefer > MODE_PL_CDR) {
		if (top->vtc)
			vtc = top->vtc;
		else if (top->ts && top->ts->ts_prefer == MODE_V_TC)
			vtc = (unsigned char *) top->ts->ts_vtc;
		else
			return (NULL);	/* Not enough info. */

		if (top->ts)
			DDS_TypeSupport_delete (top->ts);

		top->ts = vtc_type (def_lib, vtc);
		if (!top->ts)
			return (NULL);
	}
	if (top->ts->ts_prefer == MODE_CDR)
		tp = top->ts->ts_cdr;
	else {
		pl = top->ts->ts_pl;
		if (pl->builtin)
			return (NULL);

		tp = pl->xtype;
	}
	dtp = xt_dynamic_ptr (tp, 1);
	if (dtp)
		xt_type_ref (tp);

	return ((DDS_DynamicTypeBuilder) dtp);
}

#endif

Type *xt_string_type_create (TypeLib *lp, unsigned bound, DDS_TypeKind kind)
{
	Type			*tp, *ep;
	unsigned		id;
	int			index;
	uint32_t		b;
	char			name [32];

	b = bound;
	if (kind == DDS_CHAR_8_TYPE)
		ep = &dt_char8;
	else if (kind == DDS_CHAR_32_TYPE)
		ep = &dt_char32;
	else
		return (NULL);

	if (!collection_name (name, sizeof (name), "string", &b, 1, NULL, ep))
		return (NULL);

	if ((index = xt_lib_lookup (lp, name)) >= 0) {
		id = lp->types [index];
		tp = lp->domain->types [id];
		if (tp->nrefs == T_REFS_MAX)
			return (NULL);

		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
		return (tp);
	}
	return (create_string (lp, name, b, ep, -index - 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_string_type (unsigned bound)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (xt_string_type_create (dyn_lib,
							  bound,
							  DDS_CHAR_8_TYPE), 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_wstring_type (unsigned bound)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (xt_string_type_create (dyn_lib,
							  bound,
							  DDS_CHAR_32_TYPE), 1));
}

/* xt_sequence_type_create -- Create a new Sequence type (bound, elem_type) in a
			      type library (lp).  If the type already exists,
			      the existing type is reused. */

Type *xt_sequence_type_create (TypeLib  *lp,
			       unsigned bound,
			       Type     *ep,
			       size_t   esize)
{
	Type			*tp;
	unsigned		id;
	int			index;
	uint32_t		b;
	char			name [80];

	if (!lp || !ep)
		return (NULL);

	b = bound;
	if (!collection_name (name, sizeof (name), "sequence", &b, 1, NULL, ep))
		return (NULL);

	if ((index = xt_lib_lookup (lp, name)) >= 0) {
		id = lp->types [index];
		tp = lp->domain->types [id];
		if (tp->nrefs == T_REFS_MAX)
			return (NULL);

		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
		return (tp);
	}
	return (create_sequence (lp, name, b, ep, esize, -index - 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_sequence_type (
						DDS_DynamicType element_type,
						unsigned bound)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (xt_sequence_type_create (dyn_lib, 
					bound, 
					xt_d2type_ptr ((DynType_t *) element_type, 0), 0), 1));
}

/* xt_array_type_create -- Create a new Array type (bounds, elem_type) in a type
			   library (lp). */

Type *xt_array_type_create (TypeLib      *lp,
			    DDS_BoundSeq *bounds,
			    Type         *ep,
			    size_t       esize)
{
	Type			*tp;
	unsigned		i, id;
	int			index;
	char			name [80];

	if (!lp ||
	    !ep ||
	    DDS_SEQ_LENGTH (*bounds) < 1 ||
	    DDS_SEQ_LENGTH (*bounds) > MAX_ARRAY_BOUNDS)
		return (NULL);

	for (i = 0; i < DDS_SEQ_LENGTH (*bounds); i++)
		if (!DDS_SEQ_ITEM (*bounds, i))
			return (NULL);

	if (!collection_name (name, sizeof (name), "array",
			      bounds->_buffer, bounds->_length, NULL, ep))
		return (NULL);

	if ((index = xt_lib_lookup (lp, name)) >= 0) {
		id = lp->types [index];
		tp = lp->domain->types [id];
		if (tp->nrefs == T_REFS_MAX)
			return (NULL);

		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
		return (tp);
	}
	return (create_array (lp, name, bounds, ep, esize, -index - 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_array_type (
						DDS_DynamicType element_type,
						DDS_BoundSeq *bound)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (xt_array_type_create (dyn_lib, bound,
					xt_d2type_ptr ((DynType_t *) element_type, 0), 0), 1));
}

Type *xt_map_type_create (TypeLib  *lp,
			  unsigned bound,
			  Type     *kp,
			  Type     *ep)
{
	Type			*tp;
	unsigned		id;
	int			index;
	uint32_t		b;
	char			name [80];

	if (!kp || !ep)
		return (NULL);

	b = bound;
	if (!collection_name (name, sizeof (name), "map", &b, 1, kp, ep))
		return (NULL);

	if ((index = xt_lib_lookup (lp, name)) >= 0) {
		id = lp->types [index];
		tp = lp->domain->types [id];
		if (tp->nrefs == T_REFS_MAX)
			return (NULL);

		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
		return (tp);
	}
	return (create_map (lp, name, b, kp, ep, -index - 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_map_type (
					DDS_DynamicType key_element_type,
					DDS_DynamicType element_type,
					unsigned bound)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (xt_map_type_create (dyn_lib, bound,
					    xt_d2type_ptr ((DynType_t *) key_element_type, 0),
					    xt_d2type_ptr ((DynType_t *) element_type, 0)),
					    1));
}

Type *xt_create_bitset_type (TypeLib *lp, unsigned bound)
{
	Type			*tp;
	unsigned		id;
	int			index;
	uint32_t		b;
	char			name [32];

	if (!lp)
		return (NULL);

	b = bound;
	if (b < 1 || b > 64)
		return (NULL);

	snprintf (name, sizeof (name), "bitset_%u", bound);
	if ((index = xt_lib_lookup (lp, name)) >= 0) {
		id = lp->types [index];
		tp = lp->domain->types [id];
		if (tp->nrefs == T_REFS_MAX)
			return (NULL);

		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
		return (tp);
	}
	return (create_bitset (lp, name, b, -index - 1, 0));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_create_bitset_type (unsigned bound)
{
	return ((DDS_DynamicTypeBuilder) xt_dynamic_ptr (xt_create_bitset_type (dyn_lib, bound), 1));
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_load_type_w_uri (
					const char *document_url,
					const char *type_name,
					DDS_IncludePathSeq *include_paths)
{
	ARG_NOT_USED (document_url)
	ARG_NOT_USED (type_name)
	ARG_NOT_USED (include_paths)

	/* ... TBC ... */

	return (NULL);
}

DDS_DynamicTypeBuilder DDS_DynamicTypeBuilderFactory_load_type_w_document (
					const char *document,
					const char *type_name,
					DDS_IncludePathSeq *include_paths)
{
	ARG_NOT_USED (document)
	ARG_NOT_USED (type_name)
	ARG_NOT_USED (include_paths)

	/* ... TBC ... */

	return (NULL);
}


/* ------------------------  Type deletion operations --------------------- */

static void delete_enum (EnumType *ep)
{
	unsigned	i;

	for (i = 0; i < ep->nconsts; i++)
		str_unref (ep->constant [i].name);
}

static void delete_bitset (BitSetType *bp)
{
	unsigned	i;

	for (i = 0; i < bp->nbits; i++)
		str_unref (bp->bit [i].name);
}

static void delete_id (unsigned scope, unsigned id)
{
	Type	*tp;

	tp = xt_type_ptr (scope, id);
	if (!tp)
		return;

	xt_type_unref (tp);
}

static void delete_alias (AliasType *ap)
{
	delete_id (ap->type.scope, ap->base_type);
	ap->base_type = 0;
}

static void delete_collection (CollectionType *cp)
{
	delete_id (cp->type.scope, cp->element_type);
	cp->element_type = 0;
}

static void delete_map (MapType *mp)
{
	delete_id (mp->collection.type.scope, mp->collection.element_type);
	mp->collection.element_type = 0;
}

static int string_type (TypeDomain *dp, TypeId t)
{
	Type	*tp = DOMAIN_TYPE (dp, t);

	return (tp->kind == DDS_STRING_TYPE);
}

static void xt_delete_annotation_usage (TypeDomain *dp, AnnotationUsage *ap)
{
	AnnotationUsageMember	*aup;
	Type			*atp;
	unsigned		i;

	rcl_access (ap);
	if (ap->nrefs-- > 1) {
		rcl_done (ap);
		return;
	}
	rcl_done (ap);
	atp = DOMAIN_TYPE (dp, ap->id);
	for (i = 0, aup = ap->member; i < ap->nmembers; i++, aup++)
		if (string_type (dp, aup->value.type) &&
		    aup->value.u.string_val)
			str_unref (aup->value.u.string_val);
	ap->nmembers = 0;
	xt_type_unref (atp);
	xfree (ap);
}

static void delete_member (unsigned scope, Member *mp)
{
	AnnotationRef	*rp;
	TypeDomain	*dp;

	dp = libs [scope]->domain;
	while ((rp = mp->annotations) != 0) {
		mp->annotations = rp->next;
		xt_delete_annotation_usage (dp, rp->usage);
		xfree (rp);
	}
	delete_id (scope, mp->id);
	str_unref (mp->name);
}

static void delete_union (UnionType *up)
{
	UnionMember	*ump;
	unsigned	i;

	for (i = 0, ump = up->member; i < up->nmembers; i++, ump++) {
		if (ump->nlabels > 1)
			xfree (ump->label.list);
		delete_member (up->type.scope, &ump->member);
	}
	up->nmembers = 0;
}

static void delete_struct (StructureType *sp)
{
	Member		*smp;
	unsigned	i;

	for (i = 0, smp = sp->member; i < sp->nmembers; i++, smp++)
		delete_member (sp->type.scope, smp);
	sp->nmembers = 0;
}

static void delete_value (unsigned scope, AnnotationMemberValue *amp)
{
	Type	*tp;

	tp = xt_type_ptr (scope, amp->type);
	if (tp && tp->kind == DDS_STRING_TYPE)
		str_unref (amp->u.string_val);
}

static void delete_annotation (AnnotationType *ap)
{
	AnnotationMember	*amp;
	unsigned		i;

	for (i = 0, amp = ap->member; i < ap->nmembers; i++, amp++) {
		if (!primitive_type (amp->default_value.type))
			delete_value (ap->type.scope, &amp->default_value);
		delete_member (ap->type.scope, &amp->member);
	}
	ap->nmembers = 0;
}

DDS_ReturnCode_t delete_type (TypeLib *lp, Type *tp)
{
	AnnotationRef	*rp;

	while ((rp = tp->annotations) != NULL) {
		tp->annotations = rp->next;
		xt_delete_annotation_usage (lp->domain, rp->usage);
		xfree (rp);
	}
	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE:
			delete_enum ((EnumType *) tp);
			break;
		case DDS_BITSET_TYPE:
			delete_bitset ((BitSetType *) tp);
			break;
		case DDS_ALIAS_TYPE:
			delete_alias ((AliasType *) tp);
			break;
		case DDS_ARRAY_TYPE:
		case DDS_SEQUENCE_TYPE:
		case DDS_STRING_TYPE:
			delete_collection ((CollectionType *) tp);
			break;
		case DDS_MAP_TYPE:
			delete_map ((MapType *) tp);
			break;
		case DDS_UNION_TYPE:
			delete_union ((UnionType *) tp);
			break;
		case DDS_STRUCTURE_TYPE:
			delete_struct ((StructureType *) tp);
			break;
		case DDS_ANNOTATION_TYPE:
			delete_annotation ((AnnotationType *) tp);
			break;
		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	xt_lib_rem_id (lp, tp->id);
	str_unref (tp->name);
	xfree (tp);
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t xt_type_unref (Type *tp)
{
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	rcl_access (tp);
	if (primitive_type (tp->kind) || tp->nrefs-- > 1) {
		rcl_done (tp);
		return (DDS_RETCODE_OK);
	}
	rcl_done (tp);
	return (delete_type (libs [tp->scope], tp));
}

DDS_ReturnCode_t xt_type_delete (Type *tp)
{
	DDS_ReturnCode_t	ret;
	TypeLib			*lp;

	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	rcl_access (tp);
	if (primitive_type (tp->kind) || tp->nrefs-- > 1) {
		rcl_done (tp);
		return (DDS_RETCODE_OK);
	}
	rcl_done (tp);
	lp = xt_lib_access (tp->scope);
	if (!lp)
		return (DDS_RETCODE_ALREADY_DELETED);

	ret = delete_type (lp, tp);
	xt_lib_release (lp);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicTypeBuilderFactory_delete_type (void *type)
{
	DynType_t		*dp = (DynType_t *) type;
	Type			*tp;
	AnnotationType		*atp;
	DDS_ReturnCode_t	ret;

	if (!dp || (dp->magic != DT_MAGIC && dp->magic != DTB_MAGIC))
		return (DDS_RETCODE_BAD_PARAMETER);

	rcl_access (dp);
	if (dp->nrefs-- > 1) {
		rcl_done (dp);
		return (DDS_RETCODE_OK);
	}
	rcl_done (dp);
	tp = xt_d2type_ptr (dp, dp->magic == DTB_MAGIC);
	if (tp->kind == DDS_ANNOTATION_TYPE && tp->nrefs == 1) {
		atp = (AnnotationType *) tp;
		if (atp->bi_class != AC_User) {
			builtin_annotations [atp->bi_class] = NULL;
			atp->bi_class = AC_User;
		}
	}
	ret = xt_type_delete (tp);
	dp->magic = 0;
	xd_dyn_type_free (dp);
	return (ret);
}

typedef struct annotation_class_lookup_st {
	const char	*name;
	AnnotationClass	c;
} AnnotationClassLookup_t;

static AnnotationClassLookup_t classes [] = {
	{ "BitBound",		AC_Bitbound	  },
	{ "BitSet",		AC_BitSet	  },
	{ "Extensibility",	AC_Extensibility  },
	{ "ID",			AC_ID		  },
	{ "Key",		AC_Key		  },
	{ "MustUnderstand",	AC_MustUnderstand },
	{ "Nested",		AC_Nested	  },
	{ "Optional",		AC_Optional	  },
	{ "Shared",		AC_Shared	  },
	{ "Value",		AC_Value	  },
	{ "Verbatim",		AC_Verbatim	  }
};

static unsigned class_names [] = { ~0, 3, 7, 4, 0, 9, 1, 6, 2, 5, 10, 8 };

static AnnotationClass annotation_lookup (const char *name)
{
	AnnotationClassLookup_t	*lp;
	unsigned		min, max, m;
	int			d;

	min = 0;
	max = sizeof (classes) / sizeof (AnnotationClassLookup_t) - 1;
	while (max >= min) {
		m = (min + max) >> 1;
		lp = &classes [m];
		d = strcmp (lp->name, name);
		if (d < 0)
			min = m + 1;
		else if (d > 0)
			max = m - 1;
		else
			return (lp->c);
	}
	return (AC_User);
}

static DDS_ReturnCode_t add_value_member (DDS_DynamicTypeBuilder b,
					  DDS_TypeKind           kind,
					  const char             *def)
{
	DDS_MemberDescriptor	md;
	DDS_ReturnCode_t	error;

	DDS_MemberDescriptor__init (&md);
	md.name = "value";
	md.type = DDS_DynamicTypeBuilderFactory_get_primitive_type (kind);
	md.default_value = (char *) def;
	error = DDS_DynamicTypeBuilder_add_member (b, &md);
	return (error);
}

static DDS_DynamicType get_extensibility_type (void)
{
	Type	*t;

	t = xt_enum_type_create (def_lib, "ExtensibilityKind", 32, 3);
	if (!t)
		return (NULL);

	xt_enum_type_const_set (t, 0, "FINAL_EXTENSIBILITY", 0);
	xt_enum_type_const_set (t, 1, "EXTENSIBLE_EXTENSIBILITY", 1);
	xt_enum_type_const_set (t, 2, "MUTABLE_EXTENSIBILITY", 2);
	t->building = 0;
	return ((DDS_DynamicType) xt_dynamic_ptr (t, 0));
}

static DDS_ReturnCode_t add_extensibility_member (DDS_DynamicTypeBuilder ab)
{
	DDS_MemberDescriptor	md;
	DDS_ReturnCode_t	error;

	DDS_MemberDescriptor__init (&md);
	md.name = "value";
	md.type = get_extensibility_type ();
	if (!md.type)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	md.default_value = NULL;
	error = DDS_DynamicTypeBuilder_add_member (ab, &md);
	DDS_DynamicTypeBuilderFactory_delete_type (md.type);
	return (error);
}

static DDS_ReturnCode_t add_verbatim_members (DDS_DynamicTypeBuilder ab)
{
	DDS_MemberDescriptor	md;
	DDS_DynamicTypeBuilder	sb1, sb2, sb3;
	DDS_DynamicType		s1, s2, s3;
	DDS_ReturnCode_t	error;

	DDS_MemberDescriptor__init (&md);
	sb1 = DDS_DynamicTypeBuilderFactory_create_string_type (32);
	if (!sb1)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	s1 = DDS_DynamicTypeBuilder_build (sb1);
	DDS_DynamicTypeBuilderFactory_delete_type (sb1);
	if (!s1)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	md.name = "language";
	md.type = s1;
	md.default_value = "*";
	error = DDS_DynamicTypeBuilder_add_member (ab, &md);
	if (error)
		goto err1;

	sb2 = DDS_DynamicTypeBuilderFactory_create_string_type (128);
	if (!sb2)
		goto err1;

	s2 = DDS_DynamicTypeBuilder_build (sb2);
	DDS_DynamicTypeBuilderFactory_delete_type (sb2);
	if (!s1)
		goto err1;

	md.name = "placement";
	md.type = s2;
	md.default_value = "before-declaration";
	error = DDS_DynamicTypeBuilder_add_member (ab, &md);
	if (error)
		goto err2;

	sb3 = DDS_DynamicTypeBuilderFactory_create_string_type (0);
	if (!sb3)
		goto err2;

	s3 = DDS_DynamicTypeBuilder_build (sb3);
	DDS_DynamicTypeBuilderFactory_delete_type (sb3);
	if (!s3)
		goto err2;

	md.name = "text";
	md.type = s3;
	md.default_value = NULL;
	error = DDS_DynamicTypeBuilder_add_member (ab, &md);
	if (error) {
		DDS_DynamicTypeBuilderFactory_delete_type (s3);
		goto err2;
	}
	return (DDS_RETCODE_OK);

    err2:
	DDS_DynamicTypeBuilderFactory_delete_type (sb2);
    err1:
	DDS_DynamicTypeBuilderFactory_delete_type (s1);
	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

DDS_DynamicType DDS_DynamicTypeBuilderFactory_get_builtin_annotation (const char *name)
{
	DDS_TypeDescriptor	desc;
	DDS_DynamicTypeBuilder	ab;
	DynType_t		*ad;
	AnnotationType		*atp;
	AnnotationClass		ac;
	DDS_ReturnCode_t	rc;

	if (!name)
		return (NULL);

	ac = annotation_lookup (name);
	if (ac == AC_User)
		return (NULL);

	if ((ad = builtin_annotations [ac]) != NULL) {
		rcl_access (ad);
		ad->nrefs++;
		rcl_done (ad);
		return ((DDS_DynamicType) ad);
	}
	DDS_TypeDescriptor__init (&desc);
	desc.kind = DDS_ANNOTATION_TYPE;
	desc.name = (char *) name;
	ab = DDS_DynamicTypeBuilderFactory_create_type (&desc);
	if (!ab)
		return (NULL);

	if (ac != AC_BitSet) {
		switch (ac) {
			case AC_ID:
			case AC_Value:
				rc = add_value_member (ab, DDS_UINT_32_TYPE, NULL);
				break;
			case AC_Optional:
			case AC_Key:
			case AC_Nested:
			case AC_MustUnderstand:
				rc = add_value_member (ab, DDS_BOOLEAN_TYPE, "true");
				break;
			case AC_Bitbound:
				rc = add_value_member (ab, DDS_UINT_16_TYPE, "32");
				break;
			case AC_Extensibility:
				rc = add_extensibility_member (ab);
				break;
			case AC_Verbatim:
				rc = add_verbatim_members (ab);
				break;
			default:
				rc = DDS_RETCODE_BAD_PARAMETER;
				break;
		}
		if (rc) {
			DDS_DynamicTypeBuilderFactory_delete_type (ab);
			return (NULL);
		}
	}
	builtin_annotations [ac] = ad = (DynType_t *) DDS_DynamicTypeBuilder_build (ab);
	if (!ad) {
		DDS_DynamicTypeBuilderFactory_delete_type (ab);
		return (NULL);
	}
	atp = (AnnotationType *) DOMAIN_TYPE (ad->domain, ad->id);
	atp->bi_class = ac;
	DDS_DynamicTypeBuilderFactory_delete_type (ab);
	return ((DDS_DynamicType) ad);
}

/* -----------------------  MemberDescriptor operations -------------------- */

DDS_MemberDescriptor *DDS_MemberDescriptor__alloc (void)
{
	DDS_MemberDescriptor	*dp;

	dp = xmalloc (sizeof (DDS_MemberDescriptor));
	if (!dp)
		return (NULL);

	DDS_MemberDescriptor__init (dp);
	return (dp);
}

void DDS_MemberDescriptor__free (DDS_MemberDescriptor *desc)
{
	if (!desc)
		return;

	/* Don't -> DDS_MemberDescriptor__clear (desc); */
	xfree (desc);
}

void DDS_MemberDescriptor__init (DDS_MemberDescriptor *desc)
{
	memset (desc, 0, sizeof (DDS_MemberDescriptor));
	desc->label._esize = sizeof (long);
	desc->label._own = 1;
}

static void DDS_MemberDescriptor__cleanup (DDS_MemberDescriptor *desc)
{
	if (desc->name) {
		free (desc->name);
		desc->name = NULL;
	}
	if (desc->type) {
		DDS_DynamicTypeBuilderFactory_delete_type (desc->type);
		desc->type = NULL;
	}
	if (desc->default_value) {
		free (desc->default_value);
		desc->default_value = NULL;
	}
}

void DDS_MemberDescriptor__reset (DDS_MemberDescriptor *desc)
{
	if (!desc)
		return;

	DDS_MemberDescriptor__cleanup (desc);
	dds_seq_reset (&desc->label);
}

void DDS_MemberDescriptor__clear (DDS_MemberDescriptor *desc)
{
	if (!desc)
		return;

	DDS_MemberDescriptor__cleanup (desc);
	dds_seq_cleanup (&desc->label);
}

DDS_ReturnCode_t DDS_MemberDescriptor_copy_from (DDS_MemberDescriptor *dst,
						 DDS_MemberDescriptor *src)
{
	DDS_ReturnCode_t	ret;

	if (!src || !dst || !xt_valid_dtype ((DynType_t *) src->type, 0))
		return (DDS_RETCODE_BAD_PARAMETER);

	ret = dds_seq_copy (&dst->label, &src->label);
	if (ret)
		return (ret);

	dst->name = src->name;
	dst->id = src->id;
	dst->type = src->type;
	if (dst->type)
		xt_dtype_ref (dst->type);
	dst->default_value = strdup (src->default_value);
	dst->index = src->index;
	dst->default_label = src->default_label;
	return (DDS_RETCODE_OK);
}

int DDS_MemberDescriptor_equals (DDS_MemberDescriptor *d1,
				 DDS_MemberDescriptor *d2)
{
	if (!streq (d1->name, d2->name) ||
	    d1->id != d2->id ||
	    dt2type (d1->type) != dt2type (d2->type) ||
	    !streq (d1->default_value, d2->default_value) ||
	    d1->index != d2->index ||
	    !dds_seq_equal (&d1->label, &d2->label) ||
	    d1->default_label != d2->default_label)
		return (0);

	return (1);
}

static int scan_int (const char *s, uint64_t min, uint64_t max, uint64_t *value)
{
	unsigned	n, c, base;
	uint64_t	max_div, max_mod, v = 0;

	while (*s == ' ')
		s++;

	if (*s == '0') {
		s++;
		if (*s == 'x' || *s == 'X') {
			s++;
			base = 16;
		}
		else if (*s)
			base = 8;
		else {
			*value = 0;
			return (min > 0);
		}
	}
	else
		base = 10;

	n = 0;
	max_div = max / base;
	max_mod = max % base;
	for (;;) {
		c = *s;
		if (!c)
			break;

		else if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'a' && c <= 'f')
			c = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			c = c - 'A' + 10;
		else
			break;

		s++;
		if (c > base - 1 ||
		    v > max_div ||
		    (v == max_div && c > max_mod))
			return (1);

		v = v * base + c;
		n++;
	}
	while (*s == ' ')
		s++;

	if (*s || !n)
		return (1);

	*value = v;
	return (v < min || v > max);
}

static int scan_float (const char *s, long double *value)
{
	return (sscanf (s, "%Lf", value));
}

static const uint64_t	ilimits [] = {
	0, 1, 0xff, 0x7fff, 0xffff, 0x7fffffff, 0xffffffff,
	0x7fffffffffffffffULL, 0xffffffffffffffffULL
};
static const int isigned [] = {
	0, 0, 0, 1, 0, 1, 0, 1, 0
};

/* valid_value -- Check whether a string value is valid for the given type.
		  if ivalue or fvalue are given, these will be set to the
		  converted value. */

int valid_value (Type        *tp,
		 const char  *v,
		 uint64_t    *ivalue,
		 long double *fvalue,
		 int         *is_signed)
{
	EnumType	*etp;
	uint64_t	max, ival;
	long double	fval;

	while (isspace (*v))
		v++;
	if ((tp->kind >= DDS_BYTE_TYPE && 
	     tp->kind <= DDS_UINT_64_TYPE) ||
	    tp->kind == DDS_ENUMERATION_TYPE) {
		if (tp->kind == DDS_ENUMERATION_TYPE) {
			etp = (EnumType *) tp;
			max = (1 << (etp->bound - 1)) - 1;
			if (*v == '-') {
				if (is_signed)
					*is_signed = 1;
				max++;
			}
			v++;
		}
		else {
			max = ilimits [tp->kind];
			if (tp->kind < DDS_UINT_64_TYPE &&
			    isigned [tp->kind] &&
			    *v == '-') {
				v++;
				if (is_signed)
					*is_signed = 1;
				max++;
			}
			else {
				if (is_signed)
					*is_signed = 0;
				if (*v == '+')
					v++;
			}
		}
		if (scan_int (v, 0, max, &ival))
			return (0);

		else if (ivalue)
			*ivalue = ival;
	}
	else if (tp->kind >= DDS_FLOAT_32_TYPE &&
		 tp->kind <= DDS_FLOAT_128_TYPE) {
		if (!scan_float (v, &fval))
			return (0);

		else if (fvalue)
			*fvalue = fval;
	}
	else if (tp->kind != DDS_CHAR_8_TYPE &&
		 tp->kind != DDS_CHAR_32_TYPE &&
	         tp->kind != DDS_STRING_TYPE)
		return (0);

	return (1);
}

int member_descriptor_is_consistent (DDS_MemberDescriptor *md,
				     uint64_t             *ivalue,
				     long double          *fvalue,
				     int                  *is_signed)
{
	Type	*tp;

	if (!md || !md->name || !valid_name (md->name) || !md->type)
		return (0);

	if (md->default_value) {
		tp = dt2type (md->type);
		if (!tp)
			return (0);

		if (!valid_value (tp, md->default_value, ivalue, fvalue, is_signed))
			return (0);
	}
	return (1);
}

int DDS_MemberDescriptor_is_consistent (DDS_MemberDescriptor *md)
{
	return (member_descriptor_is_consistent (md, NULL, NULL, NULL));
}


/* --------------------- AnnotationDescriptor operations ------------------- */

typedef struct annotation_desc_st {
	DynType_t	*type;		/* Dynamic ref. to Annotation Type. */
	AnnotationClass	ac;		/* Annotation Class. */
	int		value;		/* Annotation Value. */
	AnnotationUsage	*aup;		/* Annotation Usage ref. */
} AnnotationDesc;

static DDS_ReturnCode_t xt_annotation_descriptor_expand (AnnotationDesc *dp,
							 int init_values)
{
	TypeDomain		*tdp;
	AnnotationUsage		*ap;
	AnnotationUsageMember	*aup;
	AnnotationType		*tp;
	AnnotationMember	*amp;
	unsigned		i;

	tp = (AnnotationType *) xt_d2type_ptr (dp->type, 0);
	if (!tp || tp->type.kind != DDS_ANNOTATION_TYPE)
		return (DDS_RETCODE_BAD_PARAMETER);

	tdp = dp->type->domain;
	if (dp->aup)
		xt_delete_annotation_usage (tdp, dp->aup);

	dp->aup = NULL;
	dp->ac = tp->bi_class;
	if (dp->ac != AC_User && tp->bi_class != AC_Verbatim)
		dp->value = tp->member [0].default_value.u.boolean_val;
	else {
		ap = xmalloc (sizeof (AnnotationUsage) +
			      sizeof (AnnotationUsageMember) * (tp->nmembers - 1));
		if (!ap)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		dp->aup = ap;
		ap->nrefs = 1;
		ap->id = tp->type.id;
		rcl_access (tp);
		tp->type.nrefs++;
		rcl_done (tp);
		ap->nmembers = tp->nmembers;
		memset (ap->member, 0, sizeof (AnnotationUsageMember) * ap->nmembers);
		if (init_values) {
			for (i = 0, aup = ap->member, amp = tp->member;
			     i < tp->nmembers;
			     i++, aup++, amp++) {
				aup->member_id = amp->member.member_id;
				aup->value = amp->default_value;
				if (string_type (tdp, aup->value.type) &&
				    aup->value.u.string_val)
					str_ref (aup->value.u.string_val);
			}
		}
	}
	return (DDS_RETCODE_OK);
}

DDS_AnnotationDescriptor *DDS_AnnotationDescriptor__alloc (void)
{
	AnnotationDesc	*dp;

	dp = xmalloc (sizeof (AnnotationDesc));
	if (!dp)
		return (NULL);

	DDS_AnnotationDescriptor__init ((DDS_AnnotationDescriptor *) dp);
	return ((DDS_AnnotationDescriptor *) dp);
}

void DDS_AnnotationDescriptor__free (DDS_AnnotationDescriptor *dp)
{
	if (!dp)
		return;

	DDS_AnnotationDescriptor__clear (dp);
	xfree (dp);
}

void DDS_AnnotationDescriptor__init (DDS_AnnotationDescriptor *desc)
{
	AnnotationDesc	*dp = (AnnotationDesc *) desc;

	if (!desc)
		return;

	dp->type = NULL;
	dp->ac = AC_User;
	dp->value = 0;
	dp->aup = NULL;
}

void DDS_AnnotationDescriptor__clear (DDS_AnnotationDescriptor *desc)
{
	AnnotationDesc	*dp = (AnnotationDesc *) desc;
	AnnotationType	*tp;

	if (!desc || !dp->type)
		return;

	tp = (AnnotationType *) xt_d2type_ptr (dp->type, 0);
	if (!tp || tp->type.kind != DDS_ANNOTATION_TYPE) {
		dp->type = NULL;
		dp->aup = NULL;
		return;
	}
	if (dp->aup) {
		xt_delete_annotation_usage (dp->type->domain, dp->aup);
		dp->aup = NULL;
	}
	dp->ac = AC_User;
	DDS_DynamicTypeBuilderFactory_delete_type (dp->type);
	dp->type = NULL;
}

static void DDS_Parameters__cleanup (DDS_Parameters *desc)
{
	unsigned i;
	MapEntry_DDS_ObjectName_DDS_ObjectName *mep;

	DDS_SEQ_FOREACH_ENTRY (*desc, i, mep) {
		if (mep->key) {
			free (mep->key);
			mep->key = NULL;
		}
		if (mep->value) {
			free (mep->value);
			mep->value = NULL;
		}
	}
}

void DDS_Parameters__reset (DDS_Parameters *desc)
{
	if (!desc)
		return;

	if (DDS_SEQ_LENGTH (*desc))
		DDS_Parameters__cleanup (desc);
	dds_seq_reset (desc);
}

void DDS_Parameters__clear (DDS_Parameters *desc)
{
	if (!desc)
		return;

	if (DDS_SEQ_LENGTH (*desc))
		DDS_Parameters__cleanup (desc);
	dds_seq_cleanup (desc);
}

static DDS_ReturnCode_t ad_get_value (TypeDomain            *dp,
				      AnnotationMemberValue *vp,
				      char                  *buf,
				      size_t                max)
{
	Type		*tp = xt_real_type (DOMAIN_TYPE (dp, vp->type));
	EnumType	*etp;
	EnumConst	*ecp;
	/*StringType	*stp;*/
	size_t		n;
	unsigned	i;

	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
			n = snprintf (buf, max, "%s", 
				  vp->u.boolean_val ? "true" : "false");
			break;
		case DDS_BYTE_TYPE:
			n = snprintf (buf, max, "0x%02x", vp->u.byte_val);
			break;
		case DDS_INT_16_TYPE:
			n = snprintf (buf, max, "%d", vp->u.int_16_val);
			break;
		case DDS_UINT_16_TYPE:
			n = snprintf (buf, max, "%u", vp->u.uint_16_val);
			break;
		case DDS_INT_32_TYPE:
			n = snprintf (buf, max, "%d", vp->u.int_32_val);
			break;
		case DDS_UINT_32_TYPE:
			n = snprintf (buf, max, "%u", vp->u.uint_32_val);
			break;
		case DDS_INT_64_TYPE:
			n = snprintf (buf, max, "%lld", (long long int) vp->u.int_64_val);
			break;
		case DDS_UINT_64_TYPE:
			n = snprintf (buf, max, "%llu", (long long unsigned) vp->u.uint_64_val);
			break;
		case DDS_FLOAT_32_TYPE:
			n = snprintf (buf, max, "%f", vp->u.float_32_val);
			break;
		case DDS_FLOAT_64_TYPE:
			n = snprintf (buf, max, "%f", vp->u.float_64_val);
			break;
		case DDS_FLOAT_128_TYPE:
			n = snprintf (buf, max, "%Lf", vp->u.float_128_val);
			break;
		case DDS_CHAR_8_TYPE:
			if (isprint (vp->u.char_val))
				n = snprintf (buf, max, "%c", vp->u.char_val);
			else
				n = snprintf (buf, max, "\\0%3o", vp->u.char_val);
			break;
		case DDS_CHAR_32_TYPE:
			/* WARNS: n = snprintf (buf, max, "%lc", vp->u.wide_char_val);*/
			n = snprintf (buf, max, "\\x%x", (unsigned) vp->u.wide_char_val);
			break;
		case DDS_ENUMERATION_TYPE:
			etp = (EnumType *) tp;
			n = ~0;
			for (i = 0, ecp = etp->constant; i < etp->nconsts; i++, ecp++)
				if (ecp->value == vp->u.enum_val) {
					n = snprintf (buf, max, "%s", str_ptr (ecp->name));
					break;
				}
			if (i >= etp->nconsts)
				n = max;
			break;
		case DDS_STRING_TYPE:
			/*stp = (StringType *) tp;
			if (stp->collection.element_type == DDS_CHAR_8_TYPE)*/
				n = snprintf (buf, max, "%s", str_ptr (vp->u.string_val));
			/*else
				n = snprintf (buf, max, "%ls", str_ptr (vp->u.string_val));*/
			break;
		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	return ((n == max) ? DDS_RETCODE_OUT_OF_RESOURCES : DDS_RETCODE_OK);
}

static DDS_ReturnCode_t ad_set_value (TypeDomain            *dp,
				      AnnotationMemberValue *vp,
				      const char            *buf)
{
	Type		*tp = DOMAIN_TYPE (dp, vp->type);
	EnumType	*etp;
	EnumConst	*ecp;
	/*StringType	*stp;*/
	uint64_t	uival, max;
	int64_t		ival;
	long double	fval;
	int		invert;
	unsigned	i;

	if (!tp->kind)
		return (DDS_RETCODE_BAD_PARAMETER);

	else if (tp->kind == DDS_BOOLEAN_TYPE) {
		if (!strcmp (buf, "false"))
			vp->u.boolean_val = 0;
		else if (!strcmp (buf, "true"))
			vp->u.boolean_val = 1;
		else
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else if (tp->kind <= DDS_UINT_64_TYPE) {
		max = ilimits [tp->kind];
		if (isigned [tp->kind] && *buf == '-' ) {
			invert = 1;
			buf++;
			max++;
		}
		else {
			invert = 0;
			if (*buf == '+')
				buf++;
		}
		if (scan_int (buf, 0, ilimits [tp->kind], &uival))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (isigned [tp->kind]) {
			if (invert)
				ival = -(int64_t) uival;
			else
				ival = (int64_t) uival;
			if (tp->kind == DDS_INT_16_TYPE)
				vp->u.int_16_val = (int16_t) ival;
			else if (tp->kind == DDS_INT_32_TYPE)
				vp->u.int_32_val = (int32_t) ival;
			else /*if (tp->kind == DDS_INT_64_TYPE)*/
				vp->u.int_64_val = ival;
		}
		else if (tp->kind == DDS_UINT_16_TYPE)
			vp->u.uint_16_val = (uint16_t) uival;
		else if  (tp->kind == DDS_UINT_16_TYPE)
			vp->u.uint_32_val = (uint32_t) uival;
		else /* if (tp->kind == DDS_UINT_64_TYPE) */
			vp->u.uint_64_val = uival;
	}
	else if (tp->kind <= DDS_FLOAT_128_TYPE) {
		if (!scan_float (buf, &fval))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (tp->kind == DDS_FLOAT_32_TYPE)
			vp->u.float_32_val = (float) fval;
		else if (tp->kind == DDS_FLOAT_64_TYPE)
			vp->u.float_64_val = (double) fval;
		else /*if (tp->kind == DDS_FLOAT_128_TYPE)*/
			vp->u.float_128_val = fval;
	}
	else if	(tp->kind == DDS_CHAR_8_TYPE) {
		vp->u.char_val = buf [0];
		if (buf [1])
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else if (tp->kind == DDS_CHAR_32_TYPE) {
#ifndef ANDROID
#if !defined (NUTTX_RTOS)
		if (mbtowc (&vp->u.wide_char_val, buf, strlen (buf)) < 0)
#endif
#endif
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else if (tp->kind == DDS_ENUMERATION_TYPE) {
		etp = (EnumType *) tp;
		for (i = 0, ecp = etp->constant; i < etp->nconsts; i++, ecp++)
			if (!strcmp (str_ptr (ecp->name), buf)) {
				vp->u.enum_val = ecp->value;
				break;
			}
		if (i == etp->nconsts)
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else if (tp->kind == DDS_STRING_TYPE) {
		/*stp = (StringType *) tp;*/
		if (!buf) {
			str_unref (vp->u.string_val);
			vp->u.string_val = NULL;
		}
		else {
			if (strcmp (buf, str_ptr (vp->u.string_val))) {
				str_unref (vp->u.string_val);
				vp->u.string_val = str_new_cstr (buf);
			}
		}
	}
	else
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static int ad_eq_value (TypeDomain *dp,
			AnnotationMemberValue *v1p,
			AnnotationMemberValue *v2p)
{
	Type		*tp = xt_real_type (DOMAIN_TYPE (dp, v1p->type));

	if (!tp)
		return (0);

	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
			return (v1p->u.boolean_val == v2p->u.boolean_val);
		case DDS_BYTE_TYPE:
			return (v1p->u.byte_val == v2p->u.byte_val);
		case DDS_INT_16_TYPE:
			return (v1p->u.int_16_val == v2p->u.int_16_val);
		case DDS_UINT_16_TYPE:
			return (v1p->u.uint_16_val == v2p->u.uint_16_val);
		case DDS_INT_32_TYPE:
			return (v1p->u.int_32_val == v2p->u.int_32_val);
		case DDS_UINT_32_TYPE:
			return (v1p->u.uint_32_val == v2p->u.uint_32_val);
		case DDS_INT_64_TYPE:
			return (v1p->u.int_64_val == v2p->u.int_64_val);
		case DDS_UINT_64_TYPE:
			return (v1p->u.uint_64_val == v2p->u.uint_64_val);
		case DDS_FLOAT_32_TYPE:
			return (v1p->u.float_32_val == v2p->u.float_32_val);
		case DDS_FLOAT_64_TYPE:
			return (v1p->u.float_64_val == v2p->u.float_64_val);
		case DDS_FLOAT_128_TYPE:
			return (v1p->u.float_128_val == v2p->u.float_128_val);
		case DDS_CHAR_8_TYPE:
			return (v1p->u.char_val == v2p->u.char_val);
		case DDS_CHAR_32_TYPE:
			return (v1p->u.wide_char_val == v2p->u.wide_char_val);
		case DDS_ENUMERATION_TYPE:
			return (v1p->u.enum_val == v2p->u.enum_val);
		case DDS_STRING_TYPE:
			return (v1p->u.string_val == v2p->u.string_val);
		default:
		  	break;
	}
	return (1);
}

DDS_ReturnCode_t DDS_AnnotationDescriptor_get_value (DDS_AnnotationDescriptor *d,
					 	     DDS_ObjectName value,
						     size_t max_value,
						     DDS_ObjectName key)
{
	AnnotationDesc		*dp = (AnnotationDesc *) d;
	AnnotationType		*tp;
	AnnotationMember	*amp;
	AnnotationMemberValue	val;
	unsigned		i;
	DDS_ReturnCode_t	ret;

	if (!dp || !dp->type || !value || !key)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = (AnnotationType *) xt_d2type_ptr (dp->type, 0);
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((dp->ac == AC_User && !dp->aup) || 
	    (dp->aup->id != tp->type.id)) {
		ret = xt_annotation_descriptor_expand (dp, 1);
		if (ret)
			return (ret);
	}
	if (dp->aup) {
		for (i = 0, amp = tp->member; i < tp->nmembers; i++, amp++)
			if (!strcmp (key, str_ptr (amp->member.name)))
				return (ad_get_value (dp->type->domain,
						      &dp->aup->member [i].value,
						      value, max_value));
	}
	else if (!strcmp (key, str_ptr (tp->member [0].member.name))) {
		val.type = tp->member [0].default_value.type;
		val.u.boolean_val = dp->value;
		return (ad_get_value (dp->type->domain, &val, value, max_value));
	}
	return (DDS_RETCODE_NO_DATA);
}

DDS_ReturnCode_t DDS_AnnotationDescriptor_set_value (DDS_AnnotationDescriptor *d,
						     DDS_ObjectName key,
						     const char *value)
{
	AnnotationDesc		*dp = (AnnotationDesc *) d;
	AnnotationType		*tp;
	AnnotationMember	*amp;
	AnnotationMemberValue	val;
	unsigned		i;
	DDS_ReturnCode_t	ret;

	if (!dp || !dp->type || !key)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = (AnnotationType *) xt_d2type_ptr (dp->type, 0);
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((dp->ac == AC_User && !dp->aup) || 
	    (dp->aup && dp->aup->id != tp->type.id)) {
		ret = xt_annotation_descriptor_expand (dp, 1);
		if (ret)
			return (ret);
	}
	if (dp->aup) {
		for (i = 0, amp = tp->member; i < tp->nmembers; i++, amp++)
			if (!strcmp (key, str_ptr (amp->member.name)))
				return (ad_set_value (dp->type->domain,
					   &dp->aup->member [i].value, value));
	}
	else if (!strcmp (key, str_ptr (tp->member [0].member.name))) {
		val.type = tp->member [0].member.id;
		ret = ad_set_value (dp->type->domain, &val, value);
		if (ret)
			return (ret);

		dp->value = val.u.boolean_val;
		return (DDS_RETCODE_OK);
	}
	return (DDS_RETCODE_BAD_PARAMETER);
}

DDS_ReturnCode_t DDS_AnnotationDescriptor_get_all_value (
						DDS_AnnotationDescriptor *d,
						DDS_Parameters *pars)
{
	TypeDomain		*tdp;
	AnnotationDesc		*dp = (AnnotationDesc *) d;
	AnnotationType		*atp;
	AnnotationMember	*amp;
	AnnotationMemberValue	val, *vp;
	Type			*tp;
	unsigned		i;
	DDS_ReturnCode_t	ret;
	char			buf [80];
	MapEntry_DDS_ObjectName_DDS_ObjectName x, *xp;

	if (!d || !pars)
		return (DDS_RETCODE_BAD_PARAMETER);

	atp = (AnnotationType *) xt_d2type_ptr (dp->type, 0);
	if (!atp)
		return (DDS_RETCODE_BAD_PARAMETER);

	tdp = dp->type->domain;
	if ((dp->ac == AC_User && !dp->aup) || 
	    (dp->aup && dp->aup->id != atp->type.id)) {
		ret = xt_annotation_descriptor_expand (dp, 1);
		if (ret)
			return (ret);
	}
	if (pars->_buffer)
		xfree (pars->_buffer);

	DDS_SEQ_INIT (*pars);

	for (i = 0, amp = atp->member; i < atp->nmembers; i++, amp++) {
		x.key = strdup (str_ptr (amp->member.name));
		x.value = NULL;
		if (!x.key)
			goto cleanup;

		if (dp->aup)
			vp = &dp->aup->member [i].value;
		else {
			val.type = atp->member [0].default_value.type;
			val.u.boolean_val = dp->value;
			vp = &val;
		}
		tp = xt_real_type (DOMAIN_TYPE (tdp, vp->type));
		if (!tp)
			goto cleanup;

		if (tp->kind == DDS_STRING_TYPE) {
			if (vp->u.string_val) {
				x.value = strdup (str_ptr (vp->u.string_val));
				if (!x.value)
					goto cleanup;
			}
		}
		else {
			ret = ad_get_value (tdp, vp, buf, sizeof (buf));
			if (ret)
				goto cleanup;

			if (buf [0]) {
				x.value = strdup (buf);
				if (!x.value)
					goto cleanup;
			}
		}
		ret = dds_seq_append (pars, &x);
		if (ret)
			goto cleanup;
	}
	return (DDS_RETCODE_OK);

    cleanup:
	if (x.key)
		free (x.key);
	if (x.value)
		free (x.value);
	for (i = 0, xp = pars->_buffer; 
	     i < pars->_length;
	     i++, xp++) {
		free (xp->key);
		if (xp->value)
			free (xp->value);
	}
	dds_seq_cleanup (pars);
	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

DDS_ReturnCode_t DDS_AnnotationDescriptor_copy_from (DDS_AnnotationDescriptor *dst,
						     DDS_AnnotationDescriptor *src)
{
	AnnotationDesc		*sdp = (AnnotationDesc *) src;
	AnnotationDesc		*ddp = (AnnotationDesc *) dst;
	AnnotationUsageMember	*dmp, *smp;
	AnnotationType		*tp;
	unsigned		i;
	DDS_ReturnCode_t	ret;

	if (!dst || !src)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = (AnnotationType *) dt2type (src->type);
	if (!tp || tp->type.kind != DDS_ANNOTATION_TYPE)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (ddp->type || ddp->ac || ddp->aup)
		DDS_AnnotationDescriptor__clear (dst);

	ddp->type = sdp->type;
	if (sdp->ac != AC_User || sdp->aup) {
		ret = xt_annotation_descriptor_expand (ddp, 0);
		if (ret) {
			ddp->type = NULL;
			return (ret);
		}
	}
	if (sdp->ac != AC_User && sdp->ac != AC_Verbatim)
		ddp->value = sdp->value;
	else
		for (i = 0, dmp = ddp->aup->member, smp = sdp->aup->member;
		     i < tp->nmembers;
		     i++, dmp++, smp++) {
			dmp->member_id = smp->member_id;
			dmp->value = smp->value;
			if (string_type (sdp->type->domain, smp->value.type) &&
			    smp->value.u.string_val)
				str_ref (smp->value.u.string_val);
		}
	return (DDS_RETCODE_OK);
}

static int xt_annotation_usage_equal (AnnotationType *tp,
				      AnnotationUsage *u1p,
				      AnnotationUsage *u2p)
{
	AnnotationUsageMember	*d1mp, *d2mp;
	unsigned		i;

	for (i = 0, d1mp = u1p->member, d2mp = u2p->member;
	     i < tp->nmembers;
	     i++, d1mp++, d2mp++) {
		if (d1mp->member_id != d2mp->member_id)
			return (0);

		if (!ad_eq_value (xt_domain_ptr (tp->type.scope), 
				  &d1mp->value, &d2mp->value))
			return (0);
	}
	return (1);
}

int DDS_AnnotationDescriptor_equals (DDS_AnnotationDescriptor *d1,
				     DDS_AnnotationDescriptor *d2)
{
	AnnotationDesc		*d1p = (AnnotationDesc *) d1;
	AnnotationDesc		*d2p = (AnnotationDesc *) d2;
	Type			*tp;
	DDS_ReturnCode_t	ret;

	if (!d1)
		return (!d2);
	else if (!d2)
		return (0);

	tp = dt2type (d1->type);
	if (!tp ||
	    tp->kind != DDS_ANNOTATION_TYPE ||
	    tp != dt2type (d2->type))
		return (0);

	if ((d1p->ac == AC_User && !d1p->aup) &&
	    (d2p->ac == AC_User && !d2p->aup))
		return (1);

	if (d1p->ac == AC_User && !d1p->aup) {
		ret = xt_annotation_descriptor_expand (d1p, 1);
		if (ret)
			return (0);
	}
	if (d2p->ac == AC_User && !d2p->aup) {
		ret = xt_annotation_descriptor_expand (d2p, 1);
		if (ret)
			return (0);
	}
	if (d1p->ac != AC_User && d1p->ac != AC_Verbatim)
		return (d1p->ac == d2p->ac && d1p->value == d2p->value);

	else if (!xt_annotation_usage_equal ((AnnotationType *) tp,
							d1p->aup, d2p->aup))
		return (0);

	return (1);
}

int DDS_AnnotationDescriptor_is_consistent (DDS_AnnotationDescriptor *d)
{
	AnnotationDesc		*dp = (AnnotationDesc *) d;
	AnnotationUsage		*aup;
	AnnotationType		*ap;
	AnnotationUsageMember	*aump;
	AnnotationMember	*amp;
	unsigned		i;

	if (!d)
		return (0);

	if (dp->type == NULL)
		return (dp->aup == NULL);

	ap = (AnnotationType *) xt_d2type_ptr (dp->type, 0);
	if (ap->type.kind != DDS_ANNOTATION_TYPE)
		return (0);

	if (dp->ac == AC_User && !dp->aup)
		return (1);

	if (dp->ac != AC_User && dp->ac != AC_Verbatim)
		return (1);

	aup = dp->aup;
	if (aup->nrefs == 0 ||
	    aup->id != ap->type.id ||
	    aup->nmembers != ap->nmembers)
		return (0);

	for (i = 0, amp = ap->member, aump = aup->member;
	     i < ap->nmembers;
	     i++, amp++, aump++)
		if (aump->member_id != amp->member.member_id ||
		    aump->value.type != amp->default_value.type)
			return (0);

	return (1);
}

/* === Dynamic Type Member ================================================== */

/* Internal type definition: */

#define	DTM_MAGIC	0xDA1C0123

typedef struct DDS_DynamicTypeMember_st {
	uint32_t		magic;		/* Valid DynamicTypeMember. */
	Type			*type;		/* Type of parent. */
	void			*member;	/* Member pointer. */
} DynamicTypeMember_t;

static DynamicTypeMember_t *dynamic_member_ptr (DDS_DynamicTypeMember desc)
{
	DynamicTypeMember_t	*mp = (DynamicTypeMember_t *) desc;

	if (!mp || mp->magic != DTM_MAGIC || !mp->type)
		return (NULL);
	else
		return (mp);
}

DDS_DynamicTypeMember DDS_DynamicTypeMember__alloc (void)
{
	DynamicTypeMember_t	*dp;

	dp = xmalloc (sizeof (DynamicTypeMember_t));
	if (!dp)
		return (NULL);

	DDS_DynamicTypeMember__init ((DDS_DynamicTypeMember) dp);
	return ((DDS_DynamicTypeMember) dp);
}

void DDS_DynamicTypeMember__free (DDS_DynamicTypeMember desc)
{
	if (!desc)
		return;

	DDS_DynamicTypeMember__clear (desc);
	xfree (desc);
}

void DDS_DynamicTypeMember__init (DDS_DynamicTypeMember desc)
{
	DynamicTypeMember_t	*dp = (DynamicTypeMember_t *) desc;

	dp->magic = DTM_MAGIC;
	dp->type = NULL;
	dp->member = NULL;
}

void DDS_DynamicTypeMember__clear (DDS_DynamicTypeMember desc)
{
	DynamicTypeMember_t	*dp = (DynamicTypeMember_t *) desc;

	dp->type = NULL;
	dp->member = NULL;
}

static DDS_ReturnCode_t member_set_enum (DDS_MemberDescriptor *mp,
					 EnumType             *tp,
					 EnumConst            *cp)
{
	mp->name = strdup (str_ptr (cp->name));
	if (!mp->name)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->id = DDS_MEMBER_ID_INVALID;
	mp->default_value = strdup (str_ptr (cp->name));
	if (!mp->default_value)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->index = cp - tp->constant;
	dds_seq_reset (&mp->label);
	mp->default_label = 0;
	if (tp->bound == 16)
		mp->type = (DDS_DynamicType) &dtr_int16;
	else
		mp->type = (DDS_DynamicType) &dtr_int32;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t member_set_bitset (DDS_MemberDescriptor *mp,
					   BitSetType           *tp,
					   Bit                  *bp)
{
	mp->name = strdup (str_ptr (bp->name));
	if (!mp->name)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->id = DDS_MEMBER_ID_INVALID;
	mp->default_value = strdup (str_ptr (bp->name));
	if (!mp->default_value)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->index = bp - tp->bit;
	dds_seq_reset (&mp->label);
	mp->default_label = 0;
	if (tp->bit_bound == 8)
		mp->type = (DDS_DynamicType) &dtr_byte;
	else if (tp->bit_bound == 16)
		mp->type = (DDS_DynamicType) &dtr_uint16;
	else if (tp->bit_bound == 32)
		mp->type = (DDS_DynamicType) &dtr_uint32;
	else /*if (tp->bit_bound == 64)*/
		mp->type = (DDS_DynamicType) &dtr_uint64;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t member_set_annotation (DDS_MemberDescriptor *mp,
					       AnnotationType       *tp,
					       AnnotationMember     *ap,
					       Type                 **etp)
{
	TypeDomain		*tdp;
	DDS_ReturnCode_t	ret;
	Type			*vtp;
	char			buf [64];

	mp->name = strdup (str_ptr (ap->member.name));
	if (!mp->name)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->id = ap->member.member_id;
	tdp = xt_domain_ptr (tp->type.scope);
	vtp = xt_real_type (DOMAIN_TYPE (tdp, ap->default_value.type));
	if (!vtp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (vtp->kind == DDS_STRING_TYPE)
		mp->default_value = strdup (str_ptr (ap->default_value.u.string_val));
	else {
		ret = ad_get_value (tdp, &ap->default_value, buf, sizeof (buf));
		if (ret)
			return (ret);

		mp->default_value = strdup (buf);
	}
	if (!mp->default_value)
		ret = DDS_RETCODE_OUT_OF_RESOURCES;

	mp->index = ap - tp->member;
	dds_seq_reset (&mp->label);
	mp->default_label = 0;
	*etp = xt_type_ptr (tp->type.scope, ap->member.id);
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t member_set_union (DDS_MemberDescriptor *mp,
					  UnionType            *tp,
					  UnionMember          *up,
					  Type                 **etp)
{
	unsigned		i;
	DDS_ReturnCode_t	ret = DDS_RETCODE_OK;

	mp->name = strdup (str_ptr (up->member.name));
	if (!mp->name)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->id = up->member.member_id;
	mp->default_value = NULL;
	mp->index = up - tp->member;
	dds_seq_reset (&mp->label);
	if (up->nlabels) {
		if (up->nlabels == 1)
			ret = dds_seq_append (&mp->label, &up->label.value);
		else
			for (i = 0; i < up->nlabels; i++) {
				ret = dds_seq_append (&mp->label, &up->label.list [i]);
				if (ret)
					break;
			}
		if (ret)
			return (ret);
	}
	mp->default_label = up->is_default;
	*etp = xt_type_ptr (tp->type.scope, up->member.id);
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t member_set_struct (DDS_MemberDescriptor *mp,
					   StructureType        *tp,
					   Member               *sp,
					   Type                 **etp)
{
	mp->name = strdup (str_ptr (sp->name));
	if (!mp->name)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	mp->id = sp->member_id;
	mp->default_value = NULL;
	mp->index = sp - tp->member;
	dds_seq_reset (&mp->label);
	mp->default_label = 0;
	*etp = xt_type_ptr (tp->type.scope, sp->id);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DynamicTypeMember_get_descriptor (DDS_DynamicTypeMember desc,
					               DDS_MemberDescriptor  *mdp)
{
	DynamicTypeMember_t	*mp = dynamic_member_ptr (desc);
	Type			*etp = NULL;
	DDS_ReturnCode_t	ret;

	if (!mp || !mdp)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_MemberDescriptor__reset (mdp);
	switch (mp->type->kind) {
		case DDS_ENUMERATION_TYPE:
			ret = member_set_enum (mdp, (EnumType *) mp->type,
						    (EnumConst *) mp->member);
			break;
		case DDS_BITSET_TYPE:
			ret = member_set_bitset (mdp, (BitSetType *) mp->type,
						      (Bit *) mp->member);
			break;
		case DDS_ANNOTATION_TYPE:
			ret = member_set_annotation (mdp, (AnnotationType *) mp->type,
							  (AnnotationMember *) mp->member,
							  &etp);
			break;
		case DDS_UNION_TYPE:
			ret = member_set_union (mdp, (UnionType *) mp->type,
						     (UnionMember *) mp->member,
						     &etp);
			break;
		case DDS_STRUCTURE_TYPE:
			ret = member_set_struct (mdp, (StructureType *) mp->type,
						      (Member *) mp->member,
						      &etp);
			break;
		default:
			ret = DDS_RETCODE_BAD_PARAMETER;
			break;
	}
	if (!ret && etp) {
		if (etp->nrefs < T_REFS_MAX) {
			mdp->type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (mdp->type) {
				xt_type_ref(etp);
			}
			else
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
		}
		else
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
	}
	if (ret) {
		DDS_MemberDescriptor__clear (mdp);
		return (ret);
	}
	return (DDS_RETCODE_OK);
}

static UnionMember *union_member_lookup (UnionType  *up,
					 const char *name,
					 unsigned   id)
{
	UnionMember	*ump;
	unsigned	i;

	if (id == DDS_MEMBER_ID_INVALID) {
		for (i = 1, ump = &up->member [1]; i < up->nmembers; i++, ump++)
			if (!strcmp (str_ptr (ump->member.name), name))
				return (ump);
	}
	else
		for (i = 1, ump = &up->member [1]; i < up->nmembers; i++, ump++)
			if (ump->member.member_id == id) {
				if (name && 
				    strcmp (str_ptr (ump->member.name), name))
					return (NULL);

				return (ump);
			}
	return (NULL);
}

static Member *struct_member_lookup (StructureType *sp,
				     const char    *name,
				     unsigned      id)
{
	Member		*mp;
	unsigned	i;

	if (id == DDS_MEMBER_ID_INVALID) {
		for (i = 0, mp = sp->member; i < sp->nmembers; i++, mp++)
			if (!strcmp (str_ptr (mp->name), name))
				return (mp);
	}
	else
		for (i = 0, mp = sp->member; i < sp->nmembers; i++, mp++)
			if (mp->member_id == id) {
				if (name && 
				    strcmp (str_ptr (mp->name), name))
					return (NULL);

				return (mp);
			}
	return (NULL);
}

static unsigned xt_annotation_count (AnnotationRef *ap)
{
	unsigned	n = 0;

	while (ap) {
		n++;
		ap = ap->next;
	}
	return (n);
}

# if 0
static DDS_ReturnCode_t xt_annotation_get (TypeDomain     *tdp,
					   AnnotationRef  *ap,
					   AnnotationDesc *dp,
					   unsigned       index)
{
	while (ap) {
		if (!index) {
			dp->type = xd_dyn_type_alloc ();
			dp->type->magic = DT_MAGIC;
			dp->type->nrefs = 1;
			dp->type->domain = tdp;
			dp->type->id = ap->usage->id;
			DOMAIN_TYPE (tdp, ap->usage->id)->nrefs++;
			dp->aup = ap->usage;
			return (DDS_RETCODE_OK);
		}
		index--;
		ap = ap->next;
	}
	return (DDS_RETCODE_NO_DATA);
}
# endif

static int valid_id_annotation (Type            *tp,
				Member          *mp,
				AnnotationUsage *aup,
				int             value,
				unsigned        *id)
{
	StructureType	*stp;
	Member		*smp;
	UnionType	*utp;
	UnionMember	*ump;
	AnnotationType	*atp;
	AnnotationMember *amp;
	unsigned	i;

	if (!aup)
		*id = (unsigned) value;
	else if (aup->nmembers != 1 ||
	         aup->member [0].member_id ||
		 aup->member [0].value.type != DDS_UINT_32_TYPE)
		return (0);
	else
		*id = aup->member [0].value.u.uint_32_val;
	if (*id > 0x0fffffff)
		return (0);

	if (tp->kind == DDS_STRUCTURE_TYPE) {
		stp = (StructureType *) tp;
		for (i = 0, smp = stp->member; i < stp->nmembers; i++, smp++)
			if (smp->member_id == *id)
				return (0);
	}
	else if (tp->kind == DDS_UNION_TYPE) {
		utp = (UnionType *) tp;
		if (mp == &utp->member [0].member && *id != 0)
			return (0);

		for (i = 0, ump = utp->member; i < utp->nmembers; i++, ump++)
			if (ump->member.member_id == *id)
				return (0);
	}
	else if (tp->kind == DDS_ANNOTATION_TYPE) {
		atp = (AnnotationType *) tp;
		for (i = 0, amp = atp->member; i < atp->nmembers; i++, amp++)
			if (amp->member.member_id == *id)
				return (0);
	}
	else
		return (0);

	return (1);
}

static int valid_bool_annotation (AnnotationUsage *aup, int value, int *b)
{
	if (!aup) {
		*b = (value != 0);
		return (1);
	}
	if (aup->nmembers != 1 ||
	    aup->member [0].member_id ||
	    aup->member [0].value.type != BOOLEAN_TYPE_ID)
		return (0);

	*b = aup->member [0].value.u.boolean_val;
	return (1);
}

static int set_bitbound (Type *tp, AnnotationUsage *aup, int value)
{
	BitSetType	*btp;
	Bit		*bp;
	EnumType	*etp;
	EnumConst	*cp;
	int32_t		min, max;
	unsigned	i, n;

	if (!aup)
		n = value;
	else if (aup->nmembers != 1 ||
		 aup->member [0].member_id ||
		 aup->member [0].value.type != DDS_UINT_32_TYPE ||
		 (tp->kind != DDS_ENUMERATION_TYPE && tp->kind != DDS_BITSET_TYPE))
		return (0);
	else
		n = aup->member [0].value.u.uint_32_val;

	if (n == 0 || n > 64)
		return (0);

	if (tp->kind == DDS_ENUMERATION_TYPE) {
		etp = (EnumType *) tp;
		max = (1 << (n - 1)) - 1;
		min = -max - 1;
		for (i = 0, cp = etp->constant; i < etp->nconsts; i++, cp++)
			if (cp->value < min || cp->value > max)
				return (0);

		etp->bound = n;
		if (n != 32)
			tp->extended = 1;
	}
	else if (tp->kind == DDS_BITSET_TYPE) {
		btp = (BitSetType *) tp;
		for (i = 0, bp = btp->bit; i < btp->nbits; i++, bp++)
			if (bp->index >= (uint32_t) n)
				return (0);

		btp->bit_bound = n;
	}
	else
		return (0);

	return (1);
}

static int set_value (Type *tp, AnnotationUsage *aup, int value, unsigned index)
{
	BitSetType	*btp;
	EnumType	*etp;
	unsigned	n;

	if (!aup)
		n = (unsigned) value;
	else if (aup->nmembers != 1 ||
	         aup->member [0].member_id ||
	         aup->member [0].value.type != DDS_UINT_32_TYPE ||
	         (tp->kind != DDS_ENUMERATION_TYPE && tp->kind != DDS_BITSET_TYPE))
		return (0);
	else
		n = aup->member [0].value.u.uint_32_val;

	if (tp->kind == DDS_ENUMERATION_TYPE) {
		etp = (EnumType *) tp;
		etp->constant [index].value = n;
		tp->extended = 1;
	}
	else if (tp->kind == DDS_BITSET_TYPE) {
		btp = (BitSetType *) tp;
		btp->bit [index].index = n;
	}
	else
		return (0);

	return (1);
}

static int valid_extensibility_value (Type            *tp, 
				      AnnotationUsage *aup,
				      int             value,
				      Extensibility_t *ext)
{
	EnumType	*etp;
	Extensibility_t	v;

	if (!aup)
		v = (Extensibility_t) value;
	else if (aup->nmembers != 1 ||
		 aup->member [0].member_id)
		return (0);
	else {
		tp = xt_real_type (xt_type_ptr (tp->scope, aup->member [0].value.type));
		if (!tp)
			return (0);

		etp = (EnumType *) tp;
		if (!etp ||
		    etp->type.kind != DDS_ENUMERATION_TYPE ||
		    strcmp (str_ptr (etp->type.name), "ExtensibilityKind") ||
		    etp->nconsts != 3 ||
		    etp->constant [0].value != 0 ||
		    strcmp (str_ptr (etp->constant [0].name), "FINAL_EXTENSIBILITY") ||
		    etp->constant [1].value != 1 ||
		    strcmp (str_ptr (etp->constant [1].name), "EXTENSIBLE_EXTENSIBILITY") ||
		    etp->constant [2].value != 2 ||
		    strcmp (str_ptr (etp->constant [2].name), "MUTABLE_EXTENSIBILITY"))
			return (0);

		v = (Extensibility_t) aup->member [0].value.u.enum_val;
	}
	if (v > 2)
		return (0);

	*ext = v;
	return (1);
}

static int valid_verbatim_value (AnnotationUsage *aup)
{
	if (!aup || aup->nmembers != 3)
		return (0);

	return (1);
}

static DDS_ReturnCode_t xt_annotation_add (AnnotationRef  **ap,
					   AnnotationDesc *dp,
					   Type           *tp,
					   Member         *mp,
					   unsigned       index)
{
	AnnotationRef	*rp;
	AnnotationType	*atp;
	AnnotationClass	ac;

	if (dp->ac == AC_User) {
		atp = (AnnotationType *) xt_real_type (xt_type_ptr (tp->scope, dp->aup->id));
		if (!atp)
			return (DDS_RETCODE_BAD_PARAMETER);

		ac = annotation_lookup (str_ptr (atp->type.name));
	}
	else
		ac = dp->ac;
	switch (ac) {
		case AC_ID: {
			unsigned	id;

			if (!mp || !valid_id_annotation (tp, mp, dp->aup, dp->value, &id))
				return (DDS_RETCODE_BAD_PARAMETER);

			if (id != mp->member_id) {
				mp->member_id = id;
				tp->extended = 1;
			}
			return (DDS_RETCODE_OK);
		}
		case AC_Optional: {
			int	b;

			if (!valid_bool_annotation (dp->aup, dp->value, &b) || !mp)
				return (DDS_RETCODE_BAD_PARAMETER);

			mp->is_optional = b;
			tp->extended = 1;
			return (DDS_RETCODE_OK);
		}
		case AC_Key: {
			int	b;

			if (!valid_bool_annotation (dp->aup, dp->value, &b) || !mp)
				return (DDS_RETCODE_BAD_PARAMETER);

			mp->is_key = b;
			return (DDS_RETCODE_OK);
		}
		case AC_Bitbound:
			if (mp || !set_bitbound (tp, dp->aup, dp->value))
				return (DDS_RETCODE_BAD_PARAMETER);

			return (DDS_RETCODE_OK);

		case AC_Value:
			if (!mp || !set_value (tp, dp->aup, dp->value, index))
				return (DDS_RETCODE_BAD_PARAMETER);

			return (DDS_RETCODE_OK);

		case AC_BitSet: {
			BitSetType	*btp;
			Bit		*bp;
			int		b, i;

			if (mp ||
			    (tp->kind != DDS_BITSET_TYPE && 
			     tp->kind != DDS_ENUMERATION_TYPE) ||
			    !valid_bool_annotation (dp->aup, dp->value, &b))
				return (DDS_RETCODE_BAD_PARAMETER);

			btp = (BitSetType *) tp;
			if (b) {
				for (i = 0, bp = btp->bit; 
				     i < btp->nbits;
				     i++, bp++)
					if (bp->index >= btp->bit_bound)
						return (DDS_RETCODE_BAD_PARAMETER);

				tp->kind = DDS_BITSET_TYPE;
			}
			else
				if (btp->bit_bound > 32)
					return (DDS_RETCODE_BAD_PARAMETER);

				tp->kind = DDS_ENUMERATION_TYPE;
			tp->extended = 1;
			return (DDS_RETCODE_OK);
		}
		case AC_Nested: {
			int	b;

			if (mp || !valid_bool_annotation (dp->aup, dp->value, &b))
				return (DDS_RETCODE_BAD_PARAMETER);

			tp->nested = b;
			return (DDS_RETCODE_OK);
		}
		case AC_Extensibility: {
			Extensibility_t	e;

			if (mp || !valid_extensibility_value (tp, dp->aup, dp->value, &e))
				return (DDS_RETCODE_BAD_PARAMETER);

			tp->extensible = e;
			if (tp->extensible == MUTABLE)
				tp->extended = 1;
			return (DDS_RETCODE_OK);
		}
		case AC_MustUnderstand: {
			int	b;

			if (!mp || !valid_bool_annotation (dp->aup, dp->value, &b))
				return (DDS_RETCODE_BAD_PARAMETER);

			mp->must_understand = b;
			tp->extended = 1;
			return (DDS_RETCODE_OK);
		}
		case AC_Verbatim:
			if (mp || !valid_verbatim_value (dp->aup))
				return (DDS_RETCODE_BAD_PARAMETER);

			break; /* Fallthrough for actual application! */

		case AC_Shared: {
			int	b;

			if (!valid_bool_annotation (dp->aup, dp->value, &b))
				return (DDS_RETCODE_BAD_PARAMETER);

			if (mp)
				mp->is_shareable = b;
			else if (tp->kind == DDS_ARRAY_TYPE ||
			         tp->kind == DDS_SEQUENCE_TYPE ||
				 tp->kind == DDS_MAP_TYPE)
				tp->shared = b;
			else
				return (DDS_RETCODE_BAD_PARAMETER);

			return (DDS_RETCODE_OK);
		}
		case AC_User:
			break;
	}
	rp = xmalloc (sizeof (AnnotationRef));
	if (!rp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	rp->usage = dp->aup;
	rcl_access (rp->usage);
	rp->usage->nrefs++;
	rcl_done (rp->usage);
	rp->next = *ap;
	*ap = rp;
	return (DDS_RETCODE_OK);
}

typedef struct annotation_info_st {
	AnnotationClass	ac;
	int		value;
	AnnotationUsage	*usage;
} AnnotationInfo_t;

static void add_annotation_info (unsigned         index,
				 AnnotationInfo_t *ip,
				 unsigned         max,
				 AnnotationClass  ac,
				 int              value,
				 AnnotationUsage  *usage)
{
	AnnotationInfo_t	*p;

	if (index >= max)
		return;

	p = ip + index;
	p->ac = ac;
	p->value = value;
	p->usage = usage;
}

static unsigned xt_member_annotations (Member           *mp,
				       AnnotationInfo_t *ip,
				       unsigned         max,
				       unsigned         index,
				       size_t           msize)
{
	AnnotationRef	*rp;
	Member		*prev_mp;
	unsigned	n = 0;

	if (mp->is_key) {
		if (ip)
			add_annotation_info (0, ip, max, AC_Key, 1, NULL);
		n++;
	}
	if (mp->is_optional) {
		if (ip)
			add_annotation_info (n, ip, max, AC_Optional, 1, NULL);
		n++;
	}
	if (mp->is_shareable) {
		if (ip)
			add_annotation_info (n, ip, max, AC_Shared, 1, NULL);
		n++;
	}
	if (mp->must_understand) {
		if (ip)
			add_annotation_info (n, ip, max, AC_MustUnderstand, 1, NULL);
		n++;
	}
	prev_mp = (index) ? (Member *) (((unsigned char *) mp) - msize) : NULL;
	if ((!prev_mp && mp->member_id) ||
	    (prev_mp && mp->member_id != prev_mp->member_id + 1)) {
		if (ip)
			add_annotation_info (n, ip, max, AC_ID, mp->member_id, NULL);
		n++;
	}
	for (rp = mp->annotations; rp; rp = rp->next) {
		if (ip && n < max)
			add_annotation_info (n, ip, max, AC_User, 0, rp->usage);
		n++;
	}
	return (n);
}

#define	MAX_ANNOTATION_INFO	8

static DDS_ReturnCode_t xt_member_annotation_get (TypeDomain     *tdp,
						  Member         *mp,
						  unsigned       midx,
						  AnnotationDesc *dp,
						  unsigned       index,
						  size_t         msize)
{
	AnnotationInfo_t	*ip, ai [MAX_ANNOTATION_INFO];
	unsigned		n;
	Type			*tp;

	n = xt_member_annotations (mp, ai, MAX_ANNOTATION_INFO, midx, msize);
	if (index >= n)
		return (DDS_RETCODE_NO_DATA);

	if (index >= MAX_ANNOTATION_INFO)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	ip = &ai [index];
	if (ip->ac == AC_User || ip->ac == AC_Verbatim) {
		dp->type = xd_dyn_type_alloc ();
		if (!dp->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		dp->type->magic = DT_MAGIC;
		dp->type->nrefs = 1;
		dp->type->domain = tdp;
		dp->type->id = ip->usage->id;
		dp->aup = ip->usage;
		tp = DOMAIN_TYPE (tdp, ip->usage->id);
		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
	}
	else {
		if (!builtin_annotations [ip->ac]) {
			dp->type = (DynType_t *) DDS_DynamicTypeBuilderFactory_get_builtin_annotation (
					   classes [class_names [ip->ac]].name);
			if (!dp->type)
				return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else {
			dp->type = builtin_annotations [ip->ac];
			if (dp->type->nrefs == T_REFS_MAX)
				return (DDS_RETCODE_OUT_OF_RESOURCES);

			rcl_access (dp->type);
			dp->type->nrefs++;
			rcl_done (dp->type);
		}
		dp->ac = ip->ac;
		dp->value = ip->value;
		dp->aup = NULL;
	}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t xt_value_annotation_get (unsigned       value,
						 AnnotationDesc *dp)
{
	dp->type = builtin_annotations [AC_Value];
	dp->ac = AC_Value;
	dp->value = value;
	dp->aup = NULL;
	if (dp->type->nrefs == T_REFS_MAX)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	rcl_access (dp->type);
	dp->type->nrefs++;
	rcl_done (dp->type);
	return (DDS_RETCODE_OK);
}

unsigned DDS_DynamicTypeMember_get_annotation_count (DDS_DynamicTypeMember desc)
{
	DynamicTypeMember_t	*mp = dynamic_member_ptr (desc);
	Type			*tp;
	unsigned		n;

	if (!mp)
		return (0);

	tp = mp->type;
	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE: {
			EnumType	*etp = (EnumType *) mp->type;
			EnumConst	*ecp = (EnumConst *) mp->member;

			n = 0;
			if (ecp == etp->constant) {
				if (ecp->value)
					n = 1;
			}
			else if (ecp->value > (ecp - 1)->value + 1)
				n = 1;
			return (n);
		}
		case DDS_BITSET_TYPE: {
			BitSetType	*btp = (BitSetType *) mp->type;
			Bit		*bp = (Bit *) mp->member;

			n = 0;
			if (bp == btp->bit) {
				if (bp->index)
					n = 1;
			}
			else if (bp->index > (bp - 1)->index + 1)
				n = 1;
			return (n);
		}
		case DDS_ANNOTATION_TYPE:
			return (0);

		case DDS_UNION_TYPE: {
			UnionType	*utp = (UnionType *) mp->type;
			UnionMember	*ump = (UnionMember *) mp->member;

			return (xt_member_annotations (&ump->member, NULL, 0,
				     ump - utp->member, sizeof (UnionMember)));
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp = (StructureType *) mp->type;
			Member		*smp = (Member *) mp->member;

			return (xt_member_annotations (smp, NULL, 0,
				     smp - stp->member, sizeof (Member)));
		}
		default:
			break;
	}
	return (0);
}

DDS_ReturnCode_t DDS_DynamicTypeMember_get_annotation (
					DDS_DynamicTypeMember    desc,
					DDS_AnnotationDescriptor *d,
					unsigned                 index)
{
	DynamicTypeMember_t	*mp = dynamic_member_ptr (desc);
	Type			*tp;
	AnnotationDesc		*dp;
	unsigned		n;

	if (!mp || !d)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = mp->type;
	dp = (AnnotationDesc *) d;
	if (dp->type || dp->aup)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE: {
			EnumType	*etp = (EnumType *) mp->type;
			EnumConst	*ecp = (EnumConst *) mp->member;

			n = 0;
			if (ecp == etp->constant) {
				if (ecp->value)
					n = 1;
			}
			else if (ecp->value > (ecp - 1)->value + 1)
				n = 1;
			if (!n)
				return (DDS_RETCODE_NO_DATA);

			return (xt_value_annotation_get (ecp->value, dp));
		}
		case DDS_BITSET_TYPE: {
			BitSetType	*btp = (BitSetType *) mp->type;
			Bit		*bp = (Bit *) mp->member;

			n = 0;
			if (bp == btp->bit) {
				if (bp->index)
					n = 1;
			}
			else if (bp->index > (bp - 1)->index + 1)
				n = 1;
			if (!n)
				return (DDS_RETCODE_NO_DATA);

			return (xt_value_annotation_get (bp->index, dp));
		}
		case DDS_ANNOTATION_TYPE:
			return (DDS_RETCODE_BAD_PARAMETER);

		case DDS_UNION_TYPE: {
			UnionType	*utp = (UnionType *) mp->type;
			UnionMember	*ump = (UnionMember *) mp->member;

			return (xt_member_annotation_get (
						xt_domain_ptr (tp->scope),
						&ump->member, ump - utp->member,
						dp, index, sizeof (UnionMember)));
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp = (StructureType *) mp->type;
			Member		*smp = mp->member;

			return (xt_member_annotation_get (
						xt_domain_ptr (tp->scope),
						smp, smp - stp->member,
						dp, index, sizeof (Member)));
		}
		default:
			break;
	}
	return (DDS_RETCODE_BAD_PARAMETER);	
}

int DDS_DynamicTypeMember_equals (DDS_DynamicTypeMember m1p,
				  DDS_DynamicTypeMember m2p)
{
	DynamicTypeMember_t	*p1 = (DynamicTypeMember_t *) m1p,
				*p2 = (DynamicTypeMember_t *) m2p;

	if (!p1 || !p2 || p1->magic != DTM_MAGIC || p2->magic != DTM_MAGIC)
		return (0);

	if (!p1->type || p1->type != p2->type || p1->member != p2->member)
		return (0);

	return (1);
}

static DDS_ReturnCode_t xt_enum_set_value (EnumType *etp,
					   unsigned index,
					   int32_t  value)
{
	EnumConst	*cp;
	unsigned	i;

	for (i = 0, cp = etp->constant; i < etp->nconsts; i++, cp++)
		if (i != index && cp->value == value)
			return (DDS_RETCODE_BAD_PARAMETER);

	etp->constant [index].value = value;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t xt_bitset_set_value (BitSetType *btp,
					     unsigned   index,
					     uint32_t   value)
{
	Bit		*bp;
	unsigned	i;

	if (value >= btp->bit_bound)
		return (DDS_RETCODE_BAD_PARAMETER);

	for (i = 0, bp = btp->bit; i < btp->nbits; i++, bp++)
		if (i != index && bp->index == value)
			return (DDS_RETCODE_BAD_PARAMETER);

	btp->bit [index].index = value;
	return (DDS_RETCODE_OK);
}

static void xt_key_update (Type *tp)
{
	StructureType	*stp;
	UnionType	*utp;
	Member		*mp;
	unsigned	i;
	int		has_keys = 0;

	if (tp->kind == DDS_UNION_TYPE) {
		utp = (UnionType *) tp;
		utp->keyed = utp->member [0].member.is_key;
	}
	else if (tp->kind == DDS_STRUCTURE_TYPE) {
		stp = (StructureType *) tp;
		for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++)
			if (mp->is_key) {
				has_keys = 1;
				break;
			}
		stp->keyed = has_keys;
	}
}

DDS_ReturnCode_t DDS_DynamicTypeMember_apply_annotation (
						DDS_DynamicTypeMember desc,
						DDS_AnnotationDescriptor *d)
{
	DynamicTypeMember_t	*mp = dynamic_member_ptr (desc);
	Type			*tp;
	AnnotationDesc		*dp;
	DDS_ReturnCode_t	ret;

	if (!mp || !d)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = mp->type;
	dp = (AnnotationDesc *) d;
	if (!dp->type)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dp->ac == AC_User && !dp->aup) {
		ret = xt_annotation_descriptor_expand (dp, 1);
		if (ret)
			return (ret);
	}
	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE: {
			EnumType	*etp = (EnumType *) tp;
			EnumConst	*cp = (EnumConst *) mp->member;

			if (dp->ac != AC_Value)
				return (DDS_RETCODE_BAD_PARAMETER);

			return (xt_enum_set_value (etp, cp - etp->constant,
								dp->value));
		}
		case DDS_BITSET_TYPE: {
			BitSetType	*btp = (BitSetType *) tp;
			Bit		*bp = (Bit *) mp->member;

			if (dp->ac != AC_Value)
				return (DDS_RETCODE_BAD_PARAMETER);

			return (xt_bitset_set_value (btp, bp - btp->bit,
								dp->value));
		}
		case DDS_ANNOTATION_TYPE:
			return (DDS_RETCODE_BAD_PARAMETER);

		case DDS_UNION_TYPE: {
			UnionMember	*ump = (UnionMember *) mp->member;
			
			ret = xt_annotation_add (&ump->member.annotations, dp,
							   tp, &ump->member, 
					     ump - ((UnionType *) tp)->member);
			if (!ret && dp->ac == AC_Key)
				xt_key_update (tp);
			return (ret);
		}
		case DDS_STRUCTURE_TYPE: {
			Member		*smp = (Member *) mp->member;

			ret = xt_annotation_add (&smp->annotations, dp, tp,
				    smp, smp - ((StructureType *) tp)->member);
			if (!ret && dp->ac == AC_Key)
				xt_key_update (tp);
			return (ret);
		}
		default:
			break;
	}
	return (DDS_RETCODE_BAD_PARAMETER);
}

DDS_MemberId DDS_DynamicTypeMember_get_id (DDS_DynamicTypeMember desc)
{
	DynamicTypeMember_t	*mp = dynamic_member_ptr (desc);
	Type			*tp;
	DDS_MemberId		id;

	if (!mp)
		return (DDS_MEMBER_ID_INVALID);

	tp = mp->type;
	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE: {
			EnumConst	*ecp = (EnumConst *) mp->member;

			id = ecp->value;
			break;
		}
		case DDS_BITSET_TYPE: {
			Bit		*bp = (Bit *) mp->member;

			id = bp->index;
			break;
		}
		case DDS_ANNOTATION_TYPE: {
			AnnotationMember *amp = (AnnotationMember *) mp->member;

			id = amp->member.member_id;
			break;
		}
		case DDS_UNION_TYPE: {
			UnionMember	*ump = (UnionMember *) mp->member;

			id = ump->member.member_id;
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			Member		*smp = (Member *) mp->member;

			id = smp->member_id;
			break;
		}
		default:
			id = DDS_MEMBER_ID_INVALID;
			break;
	}
	return (id);
}

char *DDS_DynamicTypeMember_get_name (DDS_DynamicTypeMember desc)
{
	DynamicTypeMember_t	*mp = dynamic_member_ptr (desc);
	String_t		*sp;

	if (!mp)
		return (NULL);

	switch (mp->type->kind) {
		case DDS_ENUMERATION_TYPE: {
			EnumConst	*ecp = (EnumConst *) mp->member;

			sp = ecp->name;
			break;
		}
		case DDS_BITSET_TYPE: {
			Bit		*bp = (Bit *) mp->member;

			sp = bp->name;
			break;
		}
		case DDS_ANNOTATION_TYPE: {
			AnnotationMember *amp = (AnnotationMember *) mp->member;

			sp = amp->member.name;
			break;
		}
		case DDS_UNION_TYPE: {
			UnionMember	*ump = (UnionMember *) mp->member;

			sp = ump->member.name;
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			Member		*smp = (Member *) mp->member;

			sp = smp->name;
			break;
		}
		default:
			return (NULL);
	}
	return (strdup (str_ptr (sp)));
}

/* ----------------------- Common Dynamic Type operations ------------------ */

static DDS_ReturnCode_t xt_get_descriptor (Type *tp, DDS_TypeDescriptor *desc)
{
	StructureType	*stp;
	UnionType	*utp;
	UnionMember	*ump;
	SequenceType	*sp;
	ArrayType	*rp;
	AliasType	*ltp;
	Type		*etp;
	DDS_ReturnCode_t rc;

	if (!tp || !desc)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_TypeDescriptor__reset (desc);
	desc->kind = tp->kind;
	desc->name = strdup (str_ptr (tp->name));
	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_ENUMERATION_TYPE:
			return (DDS_RETCODE_OK);

		case DDS_BITSET_TYPE:
			etp = xt_primitive_type (DDS_BOOLEAN_TYPE);
			desc->element_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (!desc->element_type || etp->nrefs == T_REFS_MAX)
				goto nomem;

			rcl_access (etp);
			etp->nrefs++;
			rcl_done (etp);
			return (DDS_RETCODE_OK);

		case DDS_ALIAS_TYPE:
			ltp = (AliasType *) tp;
			etp = xt_type_ptr (tp->scope, ltp->base_type);
			desc->base_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (!desc->base_type || etp->nrefs == T_REFS_MAX)
				goto nomem;

			rcl_access (etp);
			etp->nrefs++;
			rcl_done (etp);
			return (DDS_RETCODE_OK);

		case DDS_ARRAY_TYPE:
			rp = (ArrayType *) tp;
			rc = dds_seq_from_array (&desc->bound, rp->bound, rp->nbounds);
			if (rc)
				return (rc);

			etp = xt_type_ptr (tp->scope, rp->collection.element_type);
			desc->element_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (!desc->element_type || etp->nrefs == T_REFS_MAX) {
				dds_seq_cleanup (&desc->bound);
				goto nomem;
			}
			xt_type_ref (etp);
			return (DDS_RETCODE_OK);

		case DDS_SEQUENCE_TYPE:
		case DDS_STRING_TYPE:
		case DDS_MAP_TYPE:
			sp = (SequenceType *) tp;
			rc = dds_seq_from_array (&desc->bound, &sp->bound, 1);
			if (rc)
				return (rc);

			if (tp->kind == DDS_MAP_TYPE) {
				stp = (StructureType *) xt_type_ptr (tp->scope,
						   sp->collection.element_type);
				etp = xt_type_ptr (tp->scope, stp->member [1].id);
			}
			else {
				stp = NULL;
				etp = xt_type_ptr (tp->scope,
						   sp->collection.element_type);
			}
			if (!etp)
				return (DDS_RETCODE_BAD_PARAMETER);

			desc->element_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (!desc->element_type || etp->nrefs == T_REFS_MAX) {
				dds_seq_cleanup (&desc->bound);
				goto nomem;
			}
			xt_type_ref (etp);
			if (tp->kind != DDS_MAP_TYPE)
				return (DDS_RETCODE_OK);

			etp = xt_type_ptr (tp->scope, stp->member [0].id);
			if (!etp)
				return (DDS_RETCODE_BAD_PARAMETER);

			desc->key_element_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (!desc->key_element_type || etp->nrefs == T_REFS_MAX) {
				dds_seq_cleanup (&desc->bound);
				DDS_DynamicTypeBuilderFactory_delete_type (desc->element_type);
			}
			xt_type_ref (etp);
			return (DDS_RETCODE_OK);

		case DDS_UNION_TYPE:
			utp = (UnionType *) tp;
			if (!utp->nmembers)
				return (DDS_RETCODE_BAD_PARAMETER);

			ump = utp->member;
			etp = xt_type_ptr (tp->scope, ump->member.id);
			desc->discriminator_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
			if (!desc->discriminator_type || etp->nrefs == T_REFS_MAX)
				goto nomem;

			rcl_access (etp);
			etp->nrefs++;
			rcl_done (etp);
			return (DDS_RETCODE_OK);

		case DDS_STRUCTURE_TYPE:
			stp = (StructureType *) tp;
			if (stp->base_type) {
				etp = xt_type_ptr (tp->scope, stp->base_type);
				desc->base_type = (DDS_DynamicType) xt_dynamic_ptr (etp, 0);
				if (!desc->base_type || etp->nrefs == T_REFS_MAX)
					goto nomem;

				rcl_access (etp);
				etp->nrefs++;
				rcl_done (etp);
			}
			return (DDS_RETCODE_OK);

		case DDS_ANNOTATION_TYPE:
			return (DDS_RETCODE_OK);

		default:
			break;
	}
	return (DDS_RETCODE_UNSUPPORTED);

    nomem:
    	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

const char *xt_get_name (Type *tp)
{
	return ((tp) ? (DDS_ObjectName) strdup (str_ptr (tp->name)) : NULL);
}

DDS_TypeKind xt_get_kind (Type *tp)
{
	return ((tp) ? tp->kind : DDS_NO_TYPE);
}


static void DDS_DynamicTypeMembersByName__clean (DDS_DynamicTypeMembersByName *desc)
{
	MapEntry_DDS_ObjectName_DDS_DynamicTypeMember *p;
	unsigned i;
	
	DDS_SEQ_FOREACH_ENTRY (*desc, i, p) {
		if (p->key) {
			free (p->key);
			p->key = NULL;
		}
		if (p->value) {
			DDS_DynamicTypeMember__free (p->value);
			p->value = NULL;
		}
	}
}

static void DDS_DynamicTypeMembersById__clean (DDS_DynamicTypeMembersById *desc)
{
	MapEntry_DDS_MemberId_DDS_DynamicTypeMember *p;
	unsigned i;

	DDS_SEQ_FOREACH_ENTRY (*desc, i, p) {
		if (p->value) {
			DDS_DynamicTypeMember__free (p->value);
			p->value = NULL;
		}
	}
}

void DDS_DynamicTypeMembersByName__reset (DDS_DynamicTypeMembersByName *desc)
{
	DDS_DynamicTypeMembersByName__clean (desc);
	dds_seq_reset (desc);
}

void DDS_DynamicTypeMembersByName__clear (DDS_DynamicTypeMembersByName *desc)
{
	DDS_DynamicTypeMembersByName__clean (desc);
	dds_seq_cleanup (desc);
}

void DDS_DynamicTypeMembersById__reset (DDS_DynamicTypeMembersById *desc)
{
	DDS_DynamicTypeMembersById__clean (desc);
	dds_seq_reset (desc);
}

void DDS_DynamicTypeMembersById__clear (DDS_DynamicTypeMembersById *desc)
{
	DDS_DynamicTypeMembersById__clean (desc);
	dds_seq_cleanup (desc);
	/* ... TODO ... */
}

static EnumConst *enum_member_lookup (EnumType *ep, const char *name, unsigned id)
{
	EnumConst	*ecp;
	unsigned	i;

	if (id == DDS_MEMBER_ID_INVALID)
		for (i = 0, ecp = ep->constant; i < ep->nconsts; i++, ecp++)
			if (!strcmp (str_ptr (ecp->name), name))
				return (ecp);
 
 	return (NULL);
}

static Bit *bitset_member_lookup (BitSetType *bp, const char *name, unsigned id)
{
	Bit		*bbp;
	unsigned	i;

	if (id == DDS_MEMBER_ID_INVALID)
		for (i = 0, bbp = bp->bit; i < bp->nbits; i++, bbp++)
			if (!strcmp (str_ptr (bbp->name), name))
				return (bbp);

 	return (NULL);
}

static AnnotationMember *annotation_member_lookup (AnnotationType *ap,
						   const char     *name,
						   unsigned       id)
{
	AnnotationMember	*amp;
	unsigned		i;

	if (id == DDS_MEMBER_ID_INVALID) {
		for (i = 0, amp = ap->member; i < ap->nmembers; i++, amp++)
			if (!strcmp (str_ptr (amp->member.name), name))
				return (amp);
	}
	else
		for (i = 0, amp = ap->member; i < ap->nmembers; i++, amp++)
			if (amp->member.member_id == id) {
				if (name && 
				    strcmp (str_ptr (amp->member.name), name))
					return (NULL);

				return (amp);
			}
	return (NULL);
}

static DDS_ReturnCode_t xt_get_member_by_key (DynType_t           *dp,
					      int                 builder,
					      DynamicTypeMember_t *m,
					      const char          *name,
					      unsigned            id)
{
	Type			*tp;

	if (!dp || !m || m->magic != DTM_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	m->type = NULL;
	tp = xt_real_type (xt_d2type_ptr (dp, builder));
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE:
			m->member = enum_member_lookup ((EnumType *) tp, name, id);
			break;
		case DDS_BITSET_TYPE:
			m->member = bitset_member_lookup ((BitSetType *) tp, name, id);
			break;
		case DDS_ANNOTATION_TYPE:
			m->member = annotation_member_lookup ((AnnotationType *) tp, name, id);
			break;
		case DDS_UNION_TYPE:
			m->member = union_member_lookup ((UnionType *) tp, name, id);
			break;
		case DDS_STRUCTURE_TYPE:
			m->member = struct_member_lookup ((StructureType *) tp, name, id);
			break;
		default:
			m->member = NULL;
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!m->member)
		return (DDS_RETCODE_NO_DATA);

	m->type = tp;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t xt_get_all_members_by_key (DynType_t *sdp,
					    int builder,
					    DDS_DynamicTypeMembersByName *members_name,
					    DDS_DynamicTypeMembersById *members_id)
{
	Type			*tp;
	EnumConst		*emp = NULL;
	Bit			*bmp = NULL;
	AnnotationMember	*amp = NULL;
	UnionMember		*ump = NULL;
	Member			*smp = NULL;
	DDS_MemberId		id;
	const char		*name;
	unsigned		i, n;
	DDS_ReturnCode_t	ret;
	MapEntry_DDS_ObjectName_DDS_DynamicTypeMember *mep_name, m_name;
	MapEntry_DDS_MemberId_DDS_DynamicTypeMember *mep_id, m_id;
	DynamicTypeMember_t	mval, *mp;

	if (!sdp || (!members_name && !members_id))
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = xt_real_type (xt_d2type_ptr (sdp, builder));
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE: {
			EnumType *etp = (EnumType *) tp;

			n = etp->nconsts;
			emp = etp->constant;
			break;
		}
		case DDS_BITSET_TYPE: {
			BitSetType *btp = (BitSetType *) tp;

			n = btp->nbits;
			bmp = btp->bit;
			break;
		}
		case DDS_ANNOTATION_TYPE: {
			AnnotationType *atp = (AnnotationType *) tp;

			n = atp->nmembers;
			amp = atp->member;
			break;
		}
		case DDS_UNION_TYPE: {
			UnionType *utp = (UnionType *) tp;

			n = utp->nmembers - 1;
			ump = utp->member;
			ump++;
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType *stp = (StructureType *) tp;

			n = stp->nmembers;
			smp = stp->member;
			break;
		}
		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	for (i = 0; i < n; i++) {
		if (members_name) {
			if (DDS_SEQ_LENGTH (*members_name) <= i) {
				m_name.key = NULL;
				m_name.value = NULL;
				mep_name = &m_name;
				memset (&mval, 0, sizeof (mval));
				mp = &mval;
			}
			else {
				mep_name = &DDS_SEQ_ITEM (*members_name, i);
				if (mep_name->key ) {
					free (mep_name->key);
					mep_name->key = NULL;
				}
				if (mep_name->value)
					mp = (DynamicTypeMember_t *) mep_name->value;
				else {
					memset (&mval, 0, sizeof (mval));
					mp = &mval;
				}
			}
			mep_id = NULL;
		}
		else {
			if (DDS_SEQ_LENGTH (*members_id) <= i) {
				m_id.key = DDS_MEMBER_ID_INVALID;
				m_id.value = NULL;
				mep_id = &m_id;
				memset (&mval, 0, sizeof (mval));
				mp = &mval;
			}
			else {
				mep_id = &DDS_SEQ_ITEM (*members_id, i);
				if (mep_id->value)
					mp = (DynamicTypeMember_t *) mep_id->value;
				else {
					memset (&mval, 0, sizeof (mval));
					mp = &mval;
				}
			}
			mep_name = NULL;
		}
		mp->magic = DTM_MAGIC;
		mp->type = tp;
		switch (tp->kind) {
			case DDS_ENUMERATION_TYPE:
				id = emp->value;
				name = str_ptr (emp->name);
				mp->member = emp++;
				break;
			case DDS_BITSET_TYPE:
				id = bmp->index;
				name = str_ptr (bmp->name);
				mp->member = bmp++;
				break;
			case DDS_ANNOTATION_TYPE:
				id = amp->member.member_id;
				name = str_ptr (amp->member.name);
				mp->member = amp++;
				break;
			case DDS_UNION_TYPE:
				id = ump->member.member_id;
				name = str_ptr (ump->member.name);
				mp->member = ump++;
				break;
			case DDS_STRUCTURE_TYPE:
				id = smp->member_id;
				name = str_ptr (smp->name);
				mp->member = smp++;
				break;
			default:
				name = NULL;
				id = DDS_MEMBER_ID_INVALID;
				mp->member = NULL;
				break;
		}
		if (mp == &mval) {
			mp = xmalloc (sizeof (DynamicTypeMember_t));
			if (!mp) {
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto fail;
			}
			memcpy (mp, &mval, sizeof (mval));
		}
		if (members_name) {
			mep_name->key = strdup (name);
			mep_name->value = (DDS_DynamicTypeMember) mp;
		}
		else {
			mep_id->key = id;
			mep_id->value = (DDS_DynamicTypeMember) mp;
		}
		if (members_name &&
		    DDS_SEQ_LENGTH (*members_name) <= i &&
		    mep_name)
			ret = dds_seq_append (members_name, mep_name);
		else if (members_id &&
		         DDS_SEQ_LENGTH (*members_id) <= i &&
			 mep_id)
			ret = dds_seq_append (members_id, mep_id);
		else
			continue;

		if (ret)
			goto fail;
	}
	if (members_name) {
		for (; i < DDS_SEQ_LENGTH (*members_name); i++) {
			mep_name = &DDS_SEQ_ITEM (*members_name, i);
			if (mep_name->key)
				free (mep_name->key);
			if (mep_name->value) {
				DDS_DynamicTypeMember__free (mep_name->value);
				mep_name->value = NULL;
			}
		}
		DDS_SEQ_LENGTH (*members_name) = n;
	}
	else if (members_id) {
		for (; i < DDS_SEQ_LENGTH (*members_id); i++) {
			mep_id = &DDS_SEQ_ITEM (*members_id, i);
			if (mep_id->value) {
				DDS_DynamicTypeMember__free (mep_id->value);
				mep_id->value = NULL;
			}
		}
		DDS_SEQ_LENGTH (*members_id) = n;
	}
	return (DDS_RETCODE_OK);

    fail:
    	if (members_name) {
		for (i = 0; i < DDS_SEQ_LENGTH (*members_name); i++) {
			mep_name = &DDS_SEQ_ITEM (*members_name, i);
			if (mep_name->key)
				free (mep_name->key);
			if (mep_name->value) {
				DDS_DynamicTypeMember__free (mep_name->value);
				mep_name->value = NULL;
			}
		}
		dds_seq_reset (members_name);
	}
	else {
		for (i = 0; i < DDS_SEQ_LENGTH (*members_id); i++) {
			mep_id = &DDS_SEQ_ITEM (*members_id, i);
			if (mep_id->value) {
				DDS_DynamicTypeMember__free (mep_id->value);
				mep_id->value = NULL;
			}
		}
		dds_seq_reset (members_id);
	}
	DDS_DynamicTypeMember__clear ((DDS_DynamicTypeMember) &mval);
	return (ret);
}

static unsigned xt_type_annotations (Type             *tp,
				     AnnotationInfo_t *ip,
				     unsigned         max)
{
	AnnotationRef	*rp;
	EnumType	*etp = (EnumType *) tp;
	BitSetType	*btp = (BitSetType *) tp;
	unsigned	n = 0;

	if (!primitive_type (tp->id) &&
	    tp->kind != DDS_STRING_TYPE &&
	    tp->kind != DDS_ARRAY_TYPE &&
	    tp->kind != DDS_SEQUENCE_TYPE &&
	    tp->kind != DDS_MAP_TYPE) {
		if (tp->extensible != EXTENSIBLE) {
			if (ip)
				add_annotation_info (0, ip, max, 
					AC_Extensibility, tp->extensible, NULL);
			n++;
		}
		if (tp->nested) {
			if (ip)
				add_annotation_info (n, ip, max,
					AC_Nested, 1, NULL);
			n++;
		}
	}
	if ((tp->kind == DDS_ARRAY_TYPE || 
	     tp->kind == DDS_SEQUENCE_TYPE || 
	     tp->kind == DDS_MAP_TYPE) &&
	    tp->shared) {
		if (ip)
			add_annotation_info (n, ip, max, AC_Shared, 1, NULL);
		n++;
	}
	if (tp->kind == DDS_BITSET_TYPE) {
		if (ip)
			add_annotation_info (n, ip, max, AC_BitSet, 1, NULL);
		n++;
		if (btp->bit_bound != 32) {
			if (ip)
				add_annotation_info (n, ip, max, AC_Bitbound, btp->bit_bound, NULL);
			n++;
		}
	}
	if (tp->kind == DDS_ENUMERATION_TYPE && etp->bound != 32) {
		if (ip)
			add_annotation_info (n, ip, max, AC_Bitbound, etp->bound, NULL);
		n++;
	}
	for (rp = tp->annotations; rp; rp = rp->next) {
		if (ip && n < max)
			add_annotation_info (n, ip, max, AC_User, 0, rp->usage);
		n++;
	}
	return (n);
}

static unsigned xt_type_annotation_count (DynType_t *type, int builder)
{
	Type		*tp;

	if (!type)
		return (0);

	tp = xt_real_type (xt_d2type_ptr (type, builder));
	if (!tp)
		return (0);

	return (xt_type_annotations (tp, NULL, 0));
}

static DDS_ReturnCode_t xt_type_annotation_get (DynType_t      *type,
					        int            builder,
					        AnnotationDesc *dp,
					        unsigned       index)
{
	AnnotationInfo_t	*ip, ai [MAX_ANNOTATION_INFO];
	unsigned		n;
	Type			*tp;
	TypeDomain		*tdp;

	if (!type || !dp)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = xt_real_type (xt_d2type_ptr (type, builder));
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dp->type || dp->aup)
		return (DDS_RETCODE_BAD_PARAMETER);

	n = xt_type_annotations (tp, ai, MAX_ANNOTATION_INFO);
	if (index >= n)
		return (DDS_RETCODE_NO_DATA);

	if (index >= MAX_ANNOTATION_INFO)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	ip = &ai [index];
	tdp = xt_domain_ptr (tp->scope);
	if (ip->ac == AC_User || ip->ac == AC_Verbatim) {
		dp->type = xd_dyn_type_alloc ();
		if (!dp->type)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		dp->type->magic = DT_MAGIC;
		dp->type->nrefs = 1;
		dp->type->domain = tdp;
		dp->type->id = ip->usage->id;
		dp->aup = ip->usage;
		tp = DOMAIN_TYPE (tdp, ip->usage->id);
		if (tp->nrefs == T_REFS_MAX) {
			xd_dyn_type_free (dp->type);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
	}
	else {
		if (!builtin_annotations [ip->ac]) {
			dp->type = (DynType_t *) DDS_DynamicTypeBuilderFactory_get_builtin_annotation (
					   classes [class_names [ip->ac]].name);
			if (!dp->type)
				return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else {
			dp->type = builtin_annotations [ip->ac];
			if (dp->type->nrefs == T_REFS_MAX)
				return (DDS_RETCODE_OUT_OF_RESOURCES);

			rcl_access (dp->type);
			dp->type->nrefs++;
			rcl_access (dp->type);
		}
		dp->ac = ip->ac;
		dp->value = ip->value;
		dp->aup = NULL;
	}
	return (DDS_RETCODE_OK);
}

static int enum_equal (EnumType *t1p, EnumType *t2p)
{
	EnumConst	*c1p, *c2p;
	unsigned	i;

	if (t1p->bound != t2p->bound ||
	    t1p->nconsts != t2p->nconsts)
		return (0);

	for (i = 0, c1p = t1p->constant, c2p = t2p->constant;
	     i < t1p->nconsts;
	     i++, c1p++, c2p++)
		if (c1p->value != c2p->value ||
		    strcmp (str_ptr (c1p->name), str_ptr (c2p->name)))
			return (0);

	return (1);
}

static int bitset_equal (BitSetType *t1p, BitSetType *t2p)
{
	Bit		*b1p, *b2p;
	unsigned	i;

	if (t1p->bit_bound != t2p->bit_bound ||
	    t1p->nbits != t2p->nbits)
		return (0);

	for (i = 0, b1p = t1p->bit, b2p = t2p->bit;
	     i < t1p->nbits;
	     i++, b1p++, b2p++)
		if (b1p->index != b2p->index ||
		    strcmp (str_ptr (b1p->name), str_ptr (b2p->name)))
			return (0);

	return (1);
}

static int xt_annotations_equal (TypeDomain *d1p, AnnotationRef *a1p,
				 TypeDomain *d2p, AnnotationRef *a2p)
{
	AnnotationRef	*r1p, *r2p;
	Type		*t1p, *t2p;
	unsigned	n1, n2;

	if (!a1p || !a2p)
		return (a1p == a2p);

	n1 = xt_annotation_count (a1p);
	n2 = xt_annotation_count (a2p);
	if (n1 != n2)
		return (0);

	for (r1p = a1p; r1p; r1p = r1p->next) {
		t1p = xt_real_type (DOMAIN_TYPE (d1p, r1p->usage->id));
		for (r2p = a2p; r2p; r2p = r2p->next) {
			t2p = xt_real_type (DOMAIN_TYPE (d2p, r2p->usage->id));
			if (xt_type_equal (t1p, t2p) &&
			    xt_annotation_usage_equal ((AnnotationType *) t1p,
			    				r1p->usage, r2p->usage))
				break;
		}
		if (!r2p)
			return (0);
	}
	return (1);
}

static int member_equal (TypeDomain *d1p, Member *m1p,
			  TypeDomain *d2p, Member *m2p)
{
	Type	*t1p, *t2p;

	t1p = xt_real_type (DOMAIN_TYPE (d1p, m1p->id));
	t2p = xt_real_type (DOMAIN_TYPE (d2p, m2p->id));
	if (!t1p || !t2p)
		return (0);

	if (m1p->member_id != m2p->member_id ||
	    strcmp (str_ptr (m1p->name), str_ptr (m2p->name)) ||
	    m1p->is_key != m2p->is_key ||
	    m1p->is_optional != m2p->is_optional ||
	    m1p->is_shareable != m2p->is_shareable ||
	    !xt_type_equal (t1p, t2p) ||
	    !xt_annotations_equal (d1p, t1p->annotations,
	    			   d2p, t2p->annotations))
		return (0);

	return (1);
}

static int union_equal (UnionType *t1p, UnionType *t2p)
{
	UnionMember	*m1p, *m2p;
	TypeDomain	*d1p, *d2p;
	unsigned	i;

	if (t1p->nmembers != t2p->nmembers)
		return (0);

	d1p = xt_domain_ptr (t1p->type.scope);
	d2p = xt_domain_ptr (t2p->type.scope);
	for (i = 0, m1p = t1p->member, m2p = t2p->member;
	     i < t1p->nmembers;
	     i++, m1p++, m2p++)
		if (!member_equal (d1p, &m1p->member, d2p, &m2p->member) ||
		    m1p->is_default != m2p->is_default ||
		    m1p->nlabels != m2p->nlabels ||
		    (m1p->nlabels == 1 &&
		     m1p->label.value != m2p->label.value) ||
		    (m1p->nlabels > 1 &&
		     memcmp (m1p->label.list, m2p->label.list, 
		     	     m1p->nlabels * sizeof (int32_t))))
			return (0);

	return (1);
}

static int struct_equal (StructureType *t1p, StructureType *t2p)
{
	Member		*m1p, *m2p;
	TypeDomain	*d1p, *d2p;
	unsigned	i, j;

	if (t1p->nmembers != t2p->nmembers)
		return (0);

	d1p = xt_domain_ptr (t1p->type.scope);
	d2p = xt_domain_ptr (t2p->type.scope);
	if (t1p->type.extensible == MUTABLE)

		/* Flexible ordering of members is allowed. */
		for (i = 0, m1p = t1p->member; i < t1p->nmembers; i++, m1p++) {

			/* Search for member with same id in t2p. */
			for (j = 0, m2p = t2p->member; j < t2p->nmembers; j++, m2p++)
				if (m1p->member_id == m2p->member_id)
					break;

			if (j < t2p->nmembers) {
				if (!member_equal (d1p, m1p, d2p, m2p))
					return (0);
			}
			else
				return (0);
		}

	else /* Needs strict ordering of members! */
		for (i = 0, m1p = t1p->member, m2p = t2p->member;
		     i < t1p->nmembers;
		     i++, t1p++, t2p++)
			if (!member_equal (d1p, m1p, d2p, m2p))
				return (0);

	return (1);
}

static int annotation_equal (AnnotationType *t1p, AnnotationType *t2p)
{
	AnnotationMember *m1p, *m2p;
	TypeDomain	 *d1p, *d2p;
	unsigned	 i;

	if (t1p->nmembers != t2p->nmembers)
		return (0);

	d1p = xt_domain_ptr (t1p->type.scope);
	d2p = xt_domain_ptr (t2p->type.scope);
	for (i = 0, m1p = t1p->member, m2p = t2p->member;
	     i < t1p->nmembers;
	     i++, m1p++, m2p++)
		if (!member_equal (d1p, &m1p->member, d2p, &m2p->member) ||
		    !ad_eq_value (d1p, &m1p->default_value, &m2p->default_value))
			return (0);

	return (1);
}

int xt_type_equal (Type *t1p, Type *t2p)
{
	TypeDomain	*d1p, *d2p;

	if (t1p == t2p)
		return (1);

	d1p = xt_domain_ptr (t1p->scope);
	d2p = xt_domain_ptr (t2p->scope);
	/* TODO: nested and name test needs to be checked */ 
	if (t1p->kind != t2p->kind ||
	    (!t1p->nested && !t2p->nested && strcmp (str_ptr (t1p->name), str_ptr (t2p->name))) ||
	    t1p->extensible != t2p->extensible ||
	    !xt_annotations_equal (d1p, t1p->annotations,
	    			   d2p, t2p->annotations))
		return (0);

	switch (t1p->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_CHAR_32_TYPE:
			return (1);

		case DDS_ENUMERATION_TYPE:
			return (enum_equal ((EnumType *) t1p,
					     (EnumType *) t2p));
		case DDS_BITSET_TYPE:
			return (bitset_equal ((BitSetType *) t1p,
					       (BitSetType *) t2p));
		case DDS_ARRAY_TYPE:
		case DDS_SEQUENCE_TYPE:
		case DDS_STRING_TYPE:
		case DDS_MAP_TYPE:
			return (1);	/* Collections have a unique name! */

		case DDS_UNION_TYPE:
			return (union_equal ((UnionType *) t1p,
					      (UnionType *) t2p));
		case DDS_STRUCTURE_TYPE:
			return (struct_equal ((StructureType *) t1p,
					       (StructureType *) t2p));
		case DDS_ANNOTATION_TYPE:
			return (annotation_equal ((AnnotationType *) t1p,
						   (AnnotationType *) t2p));

		default:
			break;
	}
	return (0);
}

int xt_equals (DynType_t *self, int builder, DynType_t *other)
{
	Type	*t1p, *t2p;

	if (!self || !other)
		return (0);

	t1p = xt_real_type (xt_d2type_ptr (self, builder));
	t2p = xt_real_type (xt_d2type_ptr (other, 0));

	return (xt_type_equal (t1p, t2p));
}

/* ---------------------- Dynamic Type Builder operations ------------------ */

DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_descriptor (DDS_DynamicTypeBuilder t,
							DDS_TypeDescriptor *desc)
{
	return (xt_get_descriptor (xt_d2type_ptr ((DynType_t *) t, 1), desc));
}

const char *DDS_DynamicTypeBuilder_get_name (DDS_DynamicTypeBuilder type)
{
	return (xt_get_name (xt_d2type_ptr ((DynType_t *) type, 1)));
}

DDS_TypeKind DDS_DynamicTypeBuilder_get_kind (DDS_DynamicTypeBuilder type)
{
	return (xt_get_kind (xt_d2type_ptr ((DynType_t *) type, 1)));
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_member_by_name (
						DDS_DynamicTypeBuilder type,
						DDS_DynamicTypeMember mp,
						const char *name)
{
	return (xt_get_member_by_key ((DynType_t *) type, 1,
				      (DynamicTypeMember_t *) mp, 
				      name, DDS_MEMBER_ID_INVALID));
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_all_members_by_name (
					DDS_DynamicTypeBuilder type,
					DDS_DynamicTypeMembersByName *members)
{
	DDS_SEQ_INIT (*members);
	return (xt_get_all_members_by_key ((DynType_t *) type, 1, members, NULL));
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_member (DDS_DynamicTypeBuilder type,
					            DDS_DynamicTypeMember  mp,
					            DDS_MemberId           id)
{
	return (xt_get_member_by_key ((DynType_t *) type, 1,
				      (DynamicTypeMember_t *) mp,
				      NULL, id));
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_all_members (
					DDS_DynamicTypeBuilder     type,
					DDS_DynamicTypeMembersById *members)
{
	DDS_SEQ_INIT (*members);
	return (xt_get_all_members_by_key ((DynType_t *) type, 1, NULL, members));
}

unsigned DDS_DynamicTypeBuilder_get_annotation_count (DDS_DynamicTypeBuilder type)
{
	return (xt_type_annotation_count ((DynType_t *) type, 1));
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_get_annotation (
						DDS_DynamicTypeBuilder   type,
						DDS_AnnotationDescriptor *d,
						unsigned                 index)
{
	return (xt_type_annotation_get ((DynType_t *) type, 1,
					(AnnotationDesc *) d, index));
}

int DDS_DynamicTypeBuilder_equals (DDS_DynamicTypeBuilder self,
				   DDS_DynamicType        other)
{
	return (xt_equals ((DynType_t *) self, 1, (DynType_t *) other));
}

static void type_id_update (Type *tp)
{
	TypeLib		*lp;

	if (tp->scope > high_lib)
		return;

	lp = libs [tp->scope];
	if (!lp)
		return;

	if (tp->id && tp->id < lp->domain->max_ids)
		lp->domain->types [tp->id] = tp;
}

DDS_ReturnCode_t xt_enum_type_const_set (Type *tp, unsigned index, 
						const char *name, int32_t v)
{
	EnumType	*ep = (EnumType *) tp;
	EnumConst	*ecp;
	String_t	*s;
	unsigned	i;

	if (!tp || tp->kind != DDS_ENUMERATION_TYPE || !name ||
	    index >= ep->nconsts)
		return (DDS_RETCODE_BAD_PARAMETER);

	for (i = 0, ecp = ep->constant; i < ep->nconsts; i++, ecp++)
		if (ecp->name && 
		    (ecp->value == v || !strcmp (str_ptr (ecp->name), name)))
			return (DDS_RETCODE_BAD_PARAMETER);

	s = str_new_cstr (name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	ecp = &ep->constant [index];
	ecp->name = s;
	ecp->value = v;
	if ((unsigned) v != index)
		tp->extended = 1;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t enum_add_member (EnumType *ep,
					 DDS_MemberDescriptor *dp)
{
	Type		*etp;
	unsigned	i;
	uint64_t	val;
	int32_t		v;
	EnumType	*xtp;
	EnumConst	*ecp;
	String_t	*s;
	int		sign;

	if (!dp->name ||
	    dp->id != DDS_MEMBER_ID_INVALID ||
	    !dp->type ||
	    DDS_SEQ_LENGTH (dp->label) ||
	    dp->default_label)
		return (DDS_RETCODE_BAD_PARAMETER);

	etp = dt2type (dp->type);
	if (!etp ||
	    (etp->kind < DDS_INT_16_TYPE ||
	     etp->kind > DDS_UINT_32_TYPE))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dp->default_value) {
		if (!valid_value (&ep->type, dp->default_value, &val, NULL, &sign) ||
		    val > ilimits [etp->kind])
			return (DDS_RETCODE_BAD_PARAMETER);

		if (sign) {
			if (!isigned [etp->kind])
				return (DDS_RETCODE_BAD_PARAMETER);

			v = - (int32_t) val;
		}
		else
			v =  (int32_t) val;
	}
	else if (ep->nconsts)
		v = ep->constant [ep->nconsts - 1].value + 1;
	else
		v = 0;
	for (i = 0, ecp = ep->constant; i < ep->nconsts; i++, ecp++)
		if (ecp->value == v || !strcmp (str_ptr (ecp->name), dp->name))
			return (DDS_RETCODE_BAD_PARAMETER);

	s = str_new_cstr (dp->name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (ep->nconsts >= 2) {
		xtp = xrealloc (ep, sizeof (EnumType) +
				(ep->nconsts - 1) * sizeof (EnumConst));
		if (!xtp) {
			str_unref (s);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		if (xtp != ep) {
			ep = xtp;
			type_id_update (&ep->type);
		}
	}
	i = (dp->index > ep->nconsts) ? ep->nconsts : dp->index;
	ecp = &ep->constant [i];
	if (i < ep->nconsts)
		memmove (ecp + 1, ecp, (ep->nconsts - i) * sizeof (EnumConst));
	ep->nconsts++;
	ecp->value = v;
	ecp->name = s;
	ep->type.extended = (ep->bound == 32);
	if (!ep->type.extended)
		for (i = 0, ecp = ep->constant; i < ep->nconsts; i++, ecp++)
			if ((unsigned) ecp->value != i) {
				ep->type.extended = 1;
				break;
			}
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t bitset_add_member (BitSetType *bp,
					   DDS_MemberDescriptor *dp)
{
	Type		*etp;
	unsigned	i;
	uint64_t	val;
	int32_t		v;
	BitSetType	*xtp;
	Bit		*bcp;
	String_t	*s;

	if (!dp->name ||
	    dp->id != DDS_MEMBER_ID_INVALID ||
	    !dp->type ||
	    DDS_SEQ_LENGTH (dp->label) ||
	    dp->default_label)
		return (DDS_RETCODE_BAD_PARAMETER);

	etp = dt2type (dp->type);
	if (!etp || etp->kind != DDS_BOOLEAN_TYPE)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dp->default_value) {
		if (scan_int (dp->default_value, 0, bp->bit_bound - 1, &val))
			return (DDS_RETCODE_BAD_PARAMETER);

		v = (int32_t) val;
	}
	else if (bp->nbits == 1 || bp->nbits == 2)
		v = bp->bit [bp->nbits - 1].index + 1;
	else
		v = 0;
	for (i = 0, bcp = bp->bit; i < bp->nbits; i++, bcp++)
		if (bcp->index == (unsigned) v ||
		    !strcmp (str_ptr (bcp->name), dp->name))
			return (DDS_RETCODE_BAD_PARAMETER);

	s = str_new_cstr (dp->name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (bp->nbits >= 2) {
		xtp = xrealloc (bp, sizeof (BitSetType) +
					bp->nbits * sizeof (Bit));
		if (!xtp) {
			str_unref (s);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		if (xtp != bp) {
			bp = xtp;
			type_id_update (&bp->type);
		}
	}
	i = (dp->index > bp->nbits) ? bp->nbits : dp->index;
	bcp = &bp->bit [i];
	if (i < bp->nbits)
		memmove (bcp + 1, bcp, (bp->nbits - i) * sizeof (Bit));
	bp->nbits++;
	bcp->index = v;
	bcp->name = s;
	if (i != (unsigned) v)
		bp->type.extended = 1;
	return (DDS_RETCODE_OK);
}

/* Populate a bitset type with a bit value. */

DDS_ReturnCode_t xt_bitset_bit_set (Type *p, unsigned index,
						   const char *name, unsigned v)
{
	BitSetType	*bp = (BitSetType *) p;
	unsigned	i;
	Bit		*bcp;
	String_t	*s;

	if (!p || p->kind != DDS_BITSET_TYPE || !name ||
	    index >= bp->nbits)
		return (DDS_RETCODE_BAD_PARAMETER);

	for (i = 0, bcp = bp->bit; i < bp->nbits; i++, bcp++)
		if (bcp->name &&
		    (bcp->index == v || !strcmp (str_ptr (bcp->name), name)))
			return (DDS_RETCODE_BAD_PARAMETER);

	s = str_new_cstr (name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	bcp = &bp->bit [index];
	bcp->index = v;
	bcp->name = s;
	return (DDS_RETCODE_OK);
}
 
static int label_match (int32_t *s1, unsigned n1, int32_t *s2, unsigned n2)
{
	unsigned n, m;

	for (n = 0; n < n1; n++)
		for (m = 0; m < n2; m++)
			if (s1 [n] == s2 [m])
				return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t union_add_member (UnionType             *up,
					  DDS_UnionCaseLabelSeq *labels,
					  const char            *name,
					  unsigned              id,
					  unsigned              index,
					  Type                  *tp,
					  int                   def)
{
	UnionMember	*ump;
	UnionType	*xtp;
	unsigned	next_id, i;
	int32_t		*list;
	String_t	*s;

	if (!name ||
	    !tp || 
	    (!def && (!labels || !DDS_SEQ_LENGTH (*labels))))
		return (DDS_RETCODE_BAD_PARAMETER);

	next_id = 0;
	for (i = 0, ump = up->member; i < up->nmembers; i++, ump++) {
		if ((id != DDS_MEMBER_ID_INVALID &&
		     ump->member.member_id == id) ||
		    !strcmp (str_ptr (ump->member.name), name))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (labels && ump->nlabels &&
		    label_match (labels->_buffer, labels->_length,
				 (ump->nlabels == 1) ? &ump->label.value :
					 		       ump->label.list,
				  ump->nlabels))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (id == DDS_MEMBER_ID_INVALID && ump->member.member_id >= next_id)
			next_id = ump->member.member_id + 1;
	}
	if (id == DDS_MEMBER_ID_INVALID) {
		if (next_id >= DDS_MEMBER_ID_INVALID)
			return (DDS_RETCODE_BAD_PARAMETER);

		id = next_id;
	}
	s = str_new_cstr (name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (labels && labels->_length > 1) {
		list = xmalloc (labels->_length * sizeof (int32_t));
		if (!list) {
			str_unref (s);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		memcpy (list, labels->_buffer, 
				labels->_length * sizeof (int32_t));
	}
	else
		list = NULL;
	if (up->nmembers >= 1) {
		xtp = xrealloc (up, sizeof (UnionType) +
				up->nmembers * sizeof (UnionMember));
		if (!xtp) {
			str_unref (s);
			if (list)
				xfree (list);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		if (xtp != up) {
			up = xtp;
			type_id_update (&up->type);
		}
	}
	i = (index > up->nmembers) ? up->nmembers : index;
	ump = &up->member [i];
	if (i < up->nmembers)
		memmove (ump + 1, ump,
				(up->nmembers - i) * sizeof (UnionMember));
	up->nmembers++;
	ump->member.is_key = 0;
	ump->member.is_optional = 0;
	ump->member.is_shareable = 0;
	ump->member.must_understand = 0;
	ump->member.member_id = id;
	ump->member.annotations = NULL;
	ump->member.name = s;
	ump->member.offset = 0;	/* Should be calculated? */
	ump->is_default = def;
	ump->nlabels = (labels) ? labels->_length : 0;
	if (ump->nlabels == 1)
		ump->label.value = labels->_buffer [0];
	else if (ump->nlabels > 1)
		ump->label.list = list;
	xt_type_ref (tp);
	if (tp->extended)
		up->type.extended = 1;
	else if (!up->type.extended)
		for (i = 0, ump = up->member; i < up->nmembers; i++, ump++)
			if (ump->member.member_id != i)
				up->type.extended = 1;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t xt_union_type_member_set (Type         *p,
				           unsigned     index,
					   unsigned     nlabels,
					   int32_t      *labels,
					   const char   *name,
					   DDS_MemberId id,
					   Type         *tp,
					   int          def,
					   size_t       offset)
{
	UnionType	*up = (UnionType *) p;
	UnionMember	*ump;
	unsigned	i;
	int32_t		*list;
	String_t	*s;

	if (!p || p->kind != DDS_UNION_TYPE || !name || !tp || index >= up->nmembers ||
	    (index && !def && (!labels || !nlabels)))
		return (DDS_RETCODE_BAD_PARAMETER);

	for (i = 0, ump = up->member; i < up->nmembers; i++, ump++) {
		if (ump->member.name &&
		    (ump->member.id == id ||
		     ump->member.member_id == id ||
		     !strcmp (str_ptr (ump->member.name), name)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (nlabels && ump->nlabels &&
		    label_match (labels, nlabels,
				 (ump->nlabels == 1) ? &ump->label.value :
					 		       ump->label.list,
				  ump->nlabels))
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	s = str_new_cstr (name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (nlabels > 1) {
		list = xmalloc (nlabels * sizeof (int32_t));
		if (!list) {
			str_unref (s);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		memcpy (list, labels, nlabels * sizeof (int32_t));
	}
	else
		list = NULL;
	ump = &up->member [index];
	ump->member.member_id = id;
	ump->member.id = tp->id;
	ump->member.name = s;
	ump->member.offset = offset;
	ump->is_default = def;
	ump->nlabels = nlabels;
	if (ump->nlabels == 1)
		ump->label.value = *labels;
	else if (ump->nlabels > 1)
		ump->label.list = list;
	xt_type_ref (tp);
	if (tp->extended)
		up->type.extended = 1;
	else if (!up->type.extended)
		for (i = 0, ump = up->member; i < up->nmembers; i++, ump++)
			if (ump->member.name && ump->member.member_id != i)
				up->type.extended = 1;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t struct_add_member (StructureType *sp,
					   DDS_MemberDescriptor *dp)
{
	Member		*smp;
	Type		*etp;
	StructureType	*xtp;
	unsigned	next_id, i;
	String_t	*s;

	if (!dp->name ||
	    !dp->type ||
	    DDS_SEQ_LENGTH (dp->label) ||
	    dp->default_value ||
	    dp->default_label)
		return (DDS_RETCODE_BAD_PARAMETER);

	etp = xt_d2type_ptr ((DynType_t *) dp->type, 0);
	if (!etp)
		return (DDS_RETCODE_BAD_PARAMETER);

	next_id = 0;
	for (i = 0, smp = sp->member; i < sp->nmembers; i++, smp++) {
		if ((dp->id != DDS_MEMBER_ID_INVALID &&
		     smp->member_id == dp->id) ||
		    !strcmp (str_ptr (smp->name), dp->name))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (dp->id == DDS_MEMBER_ID_INVALID &&
		    smp->member_id >= next_id)
			next_id = smp->member_id + 1;
	}
	if (dp->id == DDS_MEMBER_ID_INVALID) {
		if (next_id >= DDS_MEMBER_ID_INVALID)
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else
		next_id = dp->id;
	s = str_new_cstr (dp->name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (sp->nmembers >= 1) {
		xtp = xrealloc (sp, sizeof (StructureType) +
				sp->nmembers * sizeof (Member));
		if (!xtp) {
			str_unref (s);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		if (xtp != sp) {
			sp = xtp;
			type_id_update (&sp->type);
		}
	}
	i = (dp->index > sp->nmembers) ? sp->nmembers : dp->index;
	smp = &sp->member [i];
	if (i < sp->nmembers)
		memmove (smp + 1, smp,
				(sp->nmembers - i) * sizeof (Member));
	sp->nmembers++;
	smp->is_key = 0;
	smp->is_optional = 0;
	smp->is_shareable = 0;
	smp->must_understand = 0;
	smp->member_id = next_id;
	smp->id = etp->id;
	smp->annotations = NULL;
	smp->name = s;
	smp->offset = 0;
	xt_type_ref (etp);
	if (etp->extended)
		sp->type.extended = 1;
	else if (!sp->type.extended)
		for (i = 0, smp = sp->member; i < sp->nmembers; i++, smp++)
			if (smp->member_id != i)
				sp->type.extended = 1;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t xt_struct_type_member_set (Type          *p,
					    unsigned      index,
					    const char    *name,
					    DDS_MemberId  id,
					    Type          *tp,
					    size_t        offset)
{
	StructureType	*sp = (StructureType *) p;
	Member		*smp;
	unsigned	i;
	String_t	*s;

	if (!p || p->kind != DDS_STRUCTURE_TYPE || !name || !tp ||
	    index >= sp->nmembers)
		return (DDS_RETCODE_BAD_PARAMETER);

	for (i = 0, smp = sp->member; i < sp->nmembers; i++, smp++)
		if (smp->name &&
		    (id == smp->member_id ||
		    !strcmp (str_ptr (smp->name), name)))
			return (DDS_RETCODE_BAD_PARAMETER);

	s = str_new_cstr (name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	smp = &sp->member [index];
	smp->member_id = id;
	smp->id = tp->id;
	smp->annotations = NULL;
	smp->name = s;
	smp->offset = offset;
	xt_type_ref (tp);
	if (tp->extended)
		sp->type.extended = 1;
	else if (!sp->type.extended)
		for (i = 0, smp = sp->member; i < sp->nmembers; i++, smp++)
			if (smp->name && smp->member_id != i)
				sp->type.extended = 1;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t annotation_add_member (AnnotationType *ap,
					       DDS_MemberDescriptor *dp)
{
	DynType_t		*dtp;
	AnnotationMember	*amp;
	AnnotationType		*xtp;
	Type			*etp;
	unsigned		next_id = 0, i;
	String_t		*s;
	DDS_ReturnCode_t	ret;

	if (!dp->name ||
	    !dp->type || 
	    DDS_SEQ_LENGTH (dp->label) ||
	    dp->default_label)
		return (DDS_RETCODE_BAD_PARAMETER);

	dtp = (DynType_t *) dp->type;
	etp = xt_d2type_ptr (dtp, 0);
	if (!etp)
		return (DDS_RETCODE_BAD_PARAMETER);

	next_id = 0;
	for (i = 0, amp = ap->member; i < ap->nmembers; i++, amp++) {
		if ((dp->id != DDS_MEMBER_ID_INVALID &&
		     amp->member.member_id == dp->id) ||
		    !strcmp (str_ptr (amp->member.name), dp->name))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (dp->id == DDS_MEMBER_ID_INVALID &&
		    amp->member.member_id >= next_id)
			next_id = amp->member.member_id + 1;
	}
	if (dp->id == DDS_MEMBER_ID_INVALID) {
		if (next_id >= DDS_MEMBER_ID_INVALID)
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else
		next_id = dp->id;
	s = str_new_cstr (dp->name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (ap->nmembers >= 1) {
		xtp = xrealloc (ap, sizeof (AnnotationType) +
				ap->nmembers * sizeof (AnnotationMember));
		if (!xtp) {
			str_unref (s);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		if (xtp != ap) {
			ap = xtp;
			type_id_update (&ap->type);
		}
	}
	i = (dp->index > ap->nmembers) ? ap->nmembers : dp->index;
	amp = &ap->member [i];
	if (i < ap->nmembers)
		memmove (amp + 1, amp,
				(ap->nmembers - i) * sizeof (AnnotationMember));
	ap->nmembers++;
	amp->member.is_key = 0;
	amp->member.is_optional = 0;
	amp->member.is_shareable = 0;
	amp->member.member_id = next_id;
	amp->member.id = etp->id;
	amp->member.annotations = NULL;
	amp->member.name = s;
	amp->member.offset = 0;
	amp->default_value.type = etp->id;
	if (dp->default_value)
		ret = ad_set_value (dtp->domain,
				    &amp->default_value,
				    dp->default_value);
	else {
		memset (&amp->default_value.u, 0, sizeof (amp->default_value.u));
		ret = DDS_RETCODE_OK;
	}
	xt_type_ref (etp);
	return (ret);
}

DDS_ReturnCode_t xt_annotation_type_member_set (Type           *p,
					        unsigned       index,
					        const char     *name,
					        DDS_MemberId   id,
					        Type           *tp,
						const char     *def_val)
{
	AnnotationType		*ap = (AnnotationType *) p;
	AnnotationMember	*amp;
	unsigned		i;
	String_t		*s;
	TypeDomain		*dp;
	DDS_ReturnCode_t	ret;

	if (!p || p->kind != DDS_ANNOTATION_TYPE || !name || !tp ||
	    index >= ap->nmembers || !def_val)
		return (DDS_RETCODE_BAD_PARAMETER);

	for (i = 0, amp = ap->member; i < ap->nmembers; i++, amp++)
		if (amp->member.name &&
		    (amp->member.member_id == id ||
		     !strcmp (str_ptr (amp->member.name), name)))
			return (DDS_RETCODE_BAD_PARAMETER);

	s = str_new_cstr (name);
	if (!s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	amp = &ap->member [index];
	amp->member.is_key = 0;
	amp->member.is_optional = 0;
	amp->member.is_shareable = 0;
	amp->member.must_understand = 0;
	amp->member.member_id = id;
	amp->member.name = s;
	amp->default_value.type = tp->id;
	if (def_val) {
		dp = libs [tp->scope]->domain;
		ret = ad_set_value (dp, &amp->default_value, def_val);
	}
	else {
		memset (&amp->default_value.u, 0, sizeof (amp->default_value.u));
		ret = DDS_RETCODE_OK;
	}
	xt_type_ref (tp);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_add_member (DDS_DynamicTypeBuilder self,
					            DDS_MemberDescriptor *desc)
{
	DynType_t		*dtp = (DynType_t *) self;
	DynType_t		*etp;
	Type			*tp;
	DDS_ReturnCode_t	ret;

	if (!self || !desc || dtp->magic != DTB_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = xt_real_type (DOMAIN_TYPE (dtp->domain, dtp->id));
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	etp = (DynType_t *) desc->type;
	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE:
			ret = enum_add_member ((EnumType *) tp, desc);
			break;
		case DDS_BITSET_TYPE:
			ret = bitset_add_member ((BitSetType *) tp, desc);
			break;
		case DDS_UNION_TYPE:
			ret = union_add_member ((UnionType *) tp,
						&desc->label,
						desc->name,
						desc->id,
						desc->index,
						DOMAIN_TYPE (etp->domain, etp->id),
						desc->default_label);
			break;
		case DDS_STRUCTURE_TYPE:
			ret = struct_add_member ((StructureType *) tp, desc);
			break;
		case DDS_ANNOTATION_TYPE:
			ret = annotation_add_member ((AnnotationType *) tp, desc);
			break;
		default:
			ret = DDS_RETCODE_PRECONDITION_NOT_MET;
			break;
	}
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicTypeBuilder_apply_annotation (
					DDS_DynamicTypeBuilder self,
					DDS_AnnotationDescriptor *d)
{
	Type			*tp;
	AnnotationDesc		*dp;
	DDS_ReturnCode_t	ret;

	if (!self || !d)
		return (DDS_RETCODE_BAD_PARAMETER);

	tp = xt_d2type_ptr ((DynType_t *) self, 1);
	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = (AnnotationDesc *) d;
	if (!dp->type)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dp->ac == AC_User && !dp->aup) {
		ret = xt_annotation_descriptor_expand (dp, 1);
		if (ret)
			return (ret);
	}
	return (xt_annotation_add (&tp->annotations, dp, tp, NULL, 0));
}

#define	FF_DYNAMIC	1	/* Dynamic data. */
#define	FF_KEYS		2	/* Contains key fields. */
#define	FF_DYN_KEYS	4	/* Dynamic key fields. */
#define	FF_FKSIZE	8	/* Fixed key size. */

#define	KM_PARENT_KEY	1	/* Parent has key attribute. */
#define	KM_TOP		2	/* Top-level. */

#define	ALIGNM1(x)	(ALIGNMENT(x) - 1)

static size_t xt_finalize (size_t   iofs,
			   size_t   *fofs,
			   unsigned *min_align,
			   Type     *tp,
			   unsigned *flags,
			   int      key_mode);

static size_t xt_finalize_pointer (size_t   iofs,
				   size_t   *fofs,
				   unsigned *min_align)
{
	size_t	s, a;

	a = ALIGNMENT (ptr_t);
	iofs = (iofs + ALIGNM1 (ptr_t)) & ~ALIGNM1 (ptr_t);
	s = sizeof (void *);
	if (a > *min_align)
		*min_align = a;
	*fofs = iofs;
	return (s);
}

static size_t xt_finalize_array (size_t   iofs,
				 size_t   *fofs,
				 unsigned *min_align,
				 Type     *tp,
				 unsigned *flags,
				 int      key_mode)
{
	ArrayType	*atp = (ArrayType *) tp;
	Type		*etp;
	size_t		s;
	unsigned	nelem, i;

	nelem = atp->bound [0];
	for (i = 1; i < atp->nbounds; i++)
		nelem *= atp->bound [i];
	etp = xt_type_ptr (tp->scope, atp->collection.element_type);
	if (!etp)
		return (0);

	s = xt_finalize (iofs, fofs, min_align, etp, flags, key_mode);
	if (!s)
		return (0);

	atp->collection.element_size = s;
	s *= nelem;
	return (s);
}

static size_t xt_finalize_seqmap (size_t   iofs,
				  size_t   *fofs,
				  unsigned *min_align,
				  Type     *tp,
				  unsigned *flags,
				  int      key_mode)
{
	SequenceType	*stp = (SequenceType *) tp;
	Type		*etp;
	size_t		s, a;
	size_t		eofs = 0;

	a = ALIGNM1 (ptr_t);
	iofs = (iofs + ALIGNM1 (ptr_t)) & ~ALIGNM1 (ptr_t);
	etp = xt_type_ptr (tp->scope, stp->collection.element_type);
	if (!etp)
		return (0);

	if (a > *min_align)
		*min_align = a;
	*fofs = iofs;

	s = xt_finalize (iofs, &eofs, min_align, etp, flags, key_mode);
	if (!s)
		return (0);

	stp->collection.element_size = s;
	*flags |= FF_DYNAMIC;
	if (key_mode) {
		*flags |= FF_DYN_KEYS;
		*flags &= ~FF_FKSIZE;
	}
	if (!stp->collection.element_size)
		return (0);

	return (sizeof (DDS_VoidSeq));
}

static size_t xt_finalize_string (size_t   iofs,
				  size_t   *fofs,
				  unsigned *min_align,
				  Type     *tp)
{
	StringType	*stp = (StringType *) tp;
	Type		*etp;
	size_t		s, a;

	etp = xt_type_ptr (tp->scope, stp->collection.element_type);
	if (!etp)
		return (0);

	if (etp->kind == DDS_CHAR_8_TYPE)
		s = a = 1;
	else {
	    	a = ALIGNMENT (int32_t);
		iofs = (iofs + ALIGNM1 (int32_t)) & ~ALIGNM1 (int32_t);
		s = sizeof (int32_t);
	}
	if (a > *min_align)
		*min_align = a;
	*fofs = iofs;
	stp->collection.element_size = s;
	if (stp->bound)
		return (s * (stp->bound + 1));
	else
		return (sizeof (void *));
}

static size_t xt_finalize_union (size_t   iofs,
				 size_t   *fofs,
				 unsigned *outer_align,
				 Type     *tp,
				 unsigned *flags,
				 int      key_mode)
{
	UnionType	*utp = (UnionType *) tp;
	Type		*ftp;
	size_t		ds, s, dsize, o, offset;
	UnionMember	*mp;
	unsigned	i, min_align;
	int		dstring;

	min_align = 0;
	utp->size = 0;
	if (!utp->nmembers)
		return (0);

	ftp = xt_type_ptr (utp->type.scope, utp->member [0].member.id);
	if (!ftp)
		return (0);

	ds = xt_finalize (0, &utp->member [0].member.offset, &min_align, ftp, 
							       flags, key_mode);
	iofs = ((iofs + min_align - 1) & ~(min_align - 1));
	offset = ds;
	dsize = 0;
	for (i = 1, mp = &utp->member [1]; i < utp->nmembers; i++, mp++) {
		ftp = xt_type_ptr (utp->type.scope, mp->member.id);
		dstring = (ftp->kind == DDS_STRING_TYPE && !((StringType *) ftp)->bound);
		if (mp->member.is_optional || mp->member.is_shareable || dstring) {
			s = xt_finalize_pointer (iofs + ds, &o, &min_align);
			*flags |= FF_DYNAMIC;
			if (key_mode && mp->member.is_key && dstring)
				*flags |= FF_DYN_KEYS;
		}
		else
			s = xt_finalize (iofs + ds, &o, &min_align, ftp, 
							       flags, key_mode);
		if (s > dsize)
			dsize = s;
		if (o > offset)
			offset = o;
	}
	for (i = 1, mp = &utp->member [1]; i < utp->nmembers; i++, mp++)
		mp->member.offset = offset;

	*fofs = (iofs + min_align - 1) & ~(min_align - 1);
	utp->size = (dsize + offset + min_align - 1) & ~(min_align - 1);
	if (min_align > *outer_align)
		*outer_align = min_align;
	return (utp->size);
}

static size_t xt_finalize_struct (size_t   iofs,
				  size_t   *fofs,
				  unsigned *outer_align,
				  Type     *tp,
				  unsigned *flags,
				  unsigned key_mode)
{
	StructureType	*stp = (StructureType *) tp, *base_tp;
	Type		*ftp;
	unsigned	min_align, i;
	Member		*mp;
	size_t		s;
	int		dstring;

	if (stp->base_type) {
		base_tp = (StructureType *) xt_type_ptr (stp->type.scope, 
							 stp->base_type);
		if (!base_tp)
			return (0);

		stp->size = base_tp->size;
	}
	else
		stp->size = 0;
	min_align = 0;
	if (!stp->nmembers)
		return (0);

	for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++) {
		ftp = xt_type_ptr (stp->type.scope, mp->id);
		if (!ftp)
			return (0);

		dstring = (ftp->kind == DDS_STRING_TYPE && 
			   !((StringType *) ftp)->bound);
		if (mp->is_optional || mp->is_shareable || dstring) {
			s = xt_finalize_pointer (stp->size, &mp->offset, &min_align);
			*flags |= FF_DYNAMIC;
			if (key_mode && mp->is_key && dstring)
				*flags |= FF_DYN_KEYS;
		}
		else
			s = xt_finalize (stp->size, &mp->offset, &min_align, ftp,
				   flags, (mp->is_key || !stp->keyed) && key_mode);
		if ((mp->is_key || !stp->keyed) && key_mode) {
			*flags |= FF_KEYS;
			if (ftp->kind == DDS_STRING_TYPE)
				*flags &= ~FF_FKSIZE;
		}
		stp->size = mp->offset + s;
	}
	*fofs = (iofs + min_align - 1) & ~(min_align - 1);
	stp->size = (stp->size + min_align - 1) & ~(min_align - 1);
	if (min_align > *outer_align)
		*outer_align = min_align;
	return (stp->size);
}

static size_t xt_finalize (size_t   iofs,
			   size_t   *fofs,
			   unsigned *min_align,
			   Type     *tp,
			   unsigned *flags,
			   int      key_mode)
{
	size_t	s, a;

	tp->building = 0;
	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		    do_8:
			s = a = 1;
			break;
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		    do_16:
		    	a = ALIGNMENT (int16_t);
			iofs = (iofs + ALIGNM1 (int16_t)) & ~ALIGNM1 (int16_t);
			s = sizeof (int16_t);
			break;
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		    do_32:
		    	a = ALIGNMENT (int32_t);
			iofs = (iofs + ALIGNM1 (int32_t)) & ~ALIGNM1 (int32_t);
			s = sizeof (int32_t);
			break;
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		    do_64:
		    	a = ALIGNMENT (int64_t);
			iofs = (iofs + ALIGNM1 (int64_t)) & ~ALIGNM1 (int64_t);
			s = sizeof (int64_t);
			break;
		case DDS_FLOAT_32_TYPE:
		    	a = ALIGNMENT (float);
			iofs = (iofs + ALIGNM1 (float)) & ~ALIGNM1 (float);
			s = sizeof (float);
			break;
		case DDS_FLOAT_64_TYPE:
		    	a = ALIGNMENT (double);
			iofs = (iofs + ALIGNM1 (double)) & ~ALIGNM1 (double);
			s = sizeof (double);
			break;
		case DDS_FLOAT_128_TYPE:
		    	a = ALIGNMENT (long_double);
			iofs = (iofs + ALIGNM1 (long_double)) & ~ALIGNM1 (long_double);
			s = sizeof (long double);
			break;
		case DDS_ENUMERATION_TYPE: {
			EnumType *ep = (EnumType *) tp;

			if (ep->bound <= 8)
				goto do_8;
			else if (ep->bound <= 16)
				goto do_16;
			else /*if (ep->bound <= 32)*/
				goto do_32;
		}
		case DDS_BITSET_TYPE: {
			BitSetType *bp = (BitSetType *) tp;

			if (bp->bit_bound <= 8)
				goto do_8;
			else if (bp->bit_bound <= 16)
				goto do_16;
			else if (bp->bit_bound <= 32)
				goto do_32;
			else
				goto do_64;
		}
		case DDS_ARRAY_TYPE:
			return (xt_finalize_array (iofs, fofs, min_align, tp, flags, key_mode));

		case DDS_SEQUENCE_TYPE:
		case DDS_MAP_TYPE:
			return (xt_finalize_seqmap (iofs, fofs, min_align, tp, flags, key_mode));

		case DDS_STRING_TYPE:
			return (xt_finalize_string (iofs, fofs, min_align, tp));

		case DDS_UNION_TYPE:
			return (xt_finalize_union (iofs, fofs, min_align, tp, flags, key_mode));

		case DDS_STRUCTURE_TYPE:
			return (xt_finalize_struct (iofs, fofs, min_align, tp, flags, key_mode));

		default:
			s = a = 0;
			break;
	}
	if (a > *min_align)
		*min_align = a;
	*fofs = iofs;
	return (s);
}

/* xt_type_finalize -- Finalize a type by calculating its size and its member
		       offsets. */

DDS_ReturnCode_t xt_type_finalize (Type   *tp,
				   size_t *ssize,
				   int    *keys,
				   int    *fkeysize,
				   int    *dkeys,
				   int    *dynamic)
{
	size_t		s;
	size_t		ofs = 0;
	unsigned	oalign = 0;
	unsigned	flags, kmode;

	if (!tp)
		return (DDS_RETCODE_BAD_PARAMETER);

	flags = FF_FKSIZE;
	kmode = KM_TOP;
	if (keys)
		kmode = KM_PARENT_KEY;

	s = xt_finalize (0, &ofs, &oalign, tp, &flags, kmode);
	if (!s)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (ssize)
		*ssize = s;
	if (keys)
		*keys = (flags & FF_KEYS) != 0;
	if (fkeysize)
		*fkeysize = (flags & FF_FKSIZE) != 0;
	if (dkeys)
		*dkeys = (flags & FF_DYN_KEYS) != 0;
	if (dynamic)
		*dynamic = (flags & FF_DYNAMIC) != 0;
	return (DDS_RETCODE_OK);
}

static void xt_type_freeze (Type *tp)
{
	unsigned	i;

	if (!tp)
		return;

	tp->building = 0;
	if (tp->kind < DDS_ALIAS_TYPE)
		return;

	else if (tp->kind == DDS_ALIAS_TYPE) {
		AliasType	*atp = (AliasType *) tp;

		xt_type_freeze (xt_type_ptr (tp->scope, atp->base_type));
	}
	else if (tp->kind >= DDS_ARRAY_TYPE && tp->kind <= DDS_MAP_TYPE) {
		CollectionType	*ctp = (CollectionType *) tp;

		xt_type_freeze (xt_type_ptr (tp->scope, ctp->element_type));
	}
	else if (tp->kind == DDS_UNION_TYPE) {
		UnionType	*utp = (UnionType *) tp;
		UnionMember	*ump;

		for (i = 0, ump = utp->member; i < utp->nmembers; i++, ump++)
			xt_type_freeze (xt_type_ptr (tp->scope, ump->member.id));
	}
	else if (tp->kind == DDS_STRUCTURE_TYPE) {
		StructureType	*stp = (StructureType *) tp;
		Member		*mp;

		/* No need to freeze base type - already frozen! */
		for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++)
			xt_type_freeze (xt_type_ptr (tp->scope, mp->id));
	}
	else if (tp->kind == DDS_ANNOTATION_TYPE) {
		AnnotationType	*atp = (AnnotationType *) tp;
		AnnotationMember *amp;

		for (i = 0, amp = atp->member; i < atp->nmembers; i++, amp++)
			xt_type_freeze (xt_type_ptr (tp->scope, amp->member.id));
	}
}

DDS_DynamicType DDS_DynamicTypeBuilder_build (DDS_DynamicTypeBuilder self)
{
	DynType_t	*dtp, *ndtp;
	Type		*tp, *otp;
	StructureType	*stp;
	UnionType	*utp;
	size_t		size;
	unsigned	oid;
	int		oindex;
	int		keys;
	int		fksize;
	int		dkeys;
	int		dynamic;
	DDS_ReturnCode_t rc;

	if (!self)
		return (NULL);

	dtp = (DynType_t *) self;
	if (dtp->magic != DTB_MAGIC)
		return (NULL);

	tp = xt_d2type_ptr (dtp, 1);
	ndtp = xd_dyn_type_alloc ();
	if (!ndtp)
		return (NULL);

	ndtp->magic = DT_MAGIC;
	ndtp->domain = dtp->domain;
	ndtp->nrefs = 1;

	/* Check if already exists. */
	xt_lib_access (0);
	oindex = xt_lib_lookup (def_lib, str_ptr (tp->name));
	if (oindex >= 0) {

		/* Type already exists in type library - check if equal! */
		oid = def_lib->types [oindex];
		otp = def_lib->domain->types [oid];
		if (otp->nrefs == T_REFS_MAX) {
			xd_dyn_type_free (ndtp);
			ndtp = NULL;
		}
		else if (xt_type_equal (tp, otp)) {
			ndtp->id = oid;
			ndtp->domain = def_lib->domain;
			rcl_access (otp);
			otp->nrefs++;
			rcl_done (otp);
		}
		else {
			xd_dyn_type_free (ndtp);
			ndtp = NULL;
		}
		xt_lib_release (def_lib);
#ifdef DUMP_LIB
		dbg_printf ("\r\nAfter _build() and duplicate detected:\r\n");
		xt_dump_lib (dyn_lib);
		xt_dump_lib (def_lib);
#endif
		return (ndtp);
	}
	else {
		/* New type -- migrate it to the default library. */
		oindex = -oindex - 1;
		if (xt_lib_migrate (def_lib, oindex, dyn_lib, tp->id)) {
			xd_dyn_type_free (ndtp);
			xt_lib_release (def_lib);
			return (NULL);
		}
#ifdef DUMP_LIB
		dbg_printf ("\r\nAfter _build(), i.e. xt_lib_migrate():\r\n");
		xt_dump_lib (dyn_lib);
		xt_dump_lib (def_lib);
#endif
	}
	ndtp->id = tp->id;
	rcl_access (tp);
	tp->nrefs++;
	rcl_done (tp);
	tp->root = 1;
	keys = 0;
	if (tp->kind != DDS_ANNOTATION_TYPE) {
		if (tp->kind == DDS_STRUCTURE_TYPE) {
			stp = (StructureType *) tp;
			if (stp->keyed)
				keys = 1;
		}
		else if (tp->kind == DDS_UNION_TYPE) {
			utp = (UnionType *) tp;
			if (utp->keyed)
				keys = 1;
		}
		rc = xt_type_finalize (tp,
				       &size,
				       (keys) ? &keys : NULL,
				       &fksize,
				       &dkeys,
				       &dynamic);
		if (rc) {
			xd_dyn_type_free (ndtp);
			xt_lib_release (def_lib);
			return (NULL);
		}
	}
	if (tp->kind == DDS_STRUCTURE_TYPE) {
		stp = (StructureType *) tp;
		stp->keys = (keys != 0);
		stp->fksize = fksize;
		stp->dkeys = dkeys;
		stp->dynamic = dynamic;
	}
	else if (tp->kind == DDS_UNION_TYPE) {
		utp = (UnionType *) tp;
		utp->keys = (keys != 0);
		utp->fksize = fksize;
		utp->dkeys = dkeys;
		utp->dynamic = dynamic;
	}
	xt_type_freeze (tp);
	xt_lib_release (def_lib);
#ifdef DUMP_LIB
	dbg_printf ("\r\nAfter _build():\r\n");
	xt_dump_lib (dyn_lib);
	xt_dump_lib (def_lib);
#endif
	return ((DDS_DynamicType) ndtp);
}

/* xt_type_flags_modify -- Update specific type flags. */

DDS_ReturnCode_t xt_type_flags_modify (Type *tp, unsigned mask, unsigned flags)
{
	if (!tp || !mask)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((mask & XTF_EXT_MASK) == XTF_EXT_MASK)
		tp->extensible = flags & XTF_EXT_MASK;
	if ((mask & XTF_NESTED) != 0)
		tp->nested = (flags & XTF_NESTED) != 0;
	if ((mask & XTF_SHARED) != 0)
		tp->shared = (flags & XTF_SHARED) != 0;
	return (DDS_RETCODE_OK);
}

/* xt_type_flags_get -- Get the type flags. */

DDS_ReturnCode_t xt_type_flags_get (Type *tp, unsigned *flags)
{
	if (!tp || !flags)
		return (DDS_RETCODE_BAD_PARAMETER);

	*flags = tp->extensible;
	if (tp->nested)
		*flags |= XTF_NESTED;
	if (tp->shared)
		*flags |= XTF_SHARED;
	return (DDS_RETCODE_OK);
}

static Member *xt_type_member (Type *tp, unsigned index)
{
	UnionType	*utp;
	StructureType	*stp;
	AnnotationType	*atp;

	if (!tp)
		return (NULL);

	else if (tp->kind == DDS_UNION_TYPE) {
		utp = (UnionType *) tp;
		return ((index >= utp->nmembers) ? NULL : &utp->member [index].member);
	}
	else if (tp->kind == DDS_STRUCTURE_TYPE) {
		stp = (StructureType *) tp;
		return ((index >= stp->nmembers) ? NULL : &stp->member [index]);
	}
	else if (tp->kind == DDS_ANNOTATION_TYPE) {
		atp = (AnnotationType *) tp;
		return ((index >= atp->nmembers) ? NULL : &atp->member [index].member);
	}
	else
		return (NULL);
}


/* --------------------------- Dynamic Type operations --------------------- */

DDS_ReturnCode_t DDS_DynamicType_get_descriptor (DDS_DynamicType t,
						 DDS_TypeDescriptor *desc)
{
	return (xt_get_descriptor (xt_d2type_ptr ((DynType_t *) t, 0), desc));
}

const char *DDS_DynamicType_get_name (DDS_DynamicType type)
{
	return (xt_get_name (xt_d2type_ptr ((DynType_t *) type, 0)));
}

DDS_TypeKind DDS_DynamicType_get_kind (DDS_DynamicType type)
{
	return (xt_get_kind (xt_d2type_ptr ((DynType_t *) type, 0)));
}

DDS_ReturnCode_t DDS_DynamicType_get_member_by_name (DDS_DynamicType type,
						     DDS_DynamicTypeMember mp,
						     const char *name)
{
	return (xt_get_member_by_key ((DynType_t *) type, 0,
				      (DynamicTypeMember_t *) mp,
				      name, DDS_MEMBER_ID_INVALID));
}

DDS_ReturnCode_t DDS_DynamicType_get_all_members_by_name (
					DDS_DynamicType type,
					DDS_DynamicTypeMembersByName *members)
{
	DDS_SEQ_INIT (*members);
	return (xt_get_all_members_by_key ((DynType_t *) type, 0, members, NULL));
}

DDS_ReturnCode_t DDS_DynamicType_get_member (DDS_DynamicType       type,
					     DDS_DynamicTypeMember mp,
					     DDS_MemberId          id)
{
	return (xt_get_member_by_key ((DynType_t *) type, 0,
				      (DynamicTypeMember_t *) mp,
				      NULL, id));
}

DDS_ReturnCode_t DDS_DynamicType_get_all_members (DDS_DynamicType type,
						  DDS_DynamicTypeMembersById *members)
{
	DDS_SEQ_INIT (*members);
	return (xt_get_all_members_by_key ((DynType_t *) type, 0, NULL, members));
}

unsigned DDS_DynamicType_get_annotation_count (DDS_DynamicType type)
{
	return (xt_type_annotation_count ((DynType_t *) type, 0));
}

DDS_ReturnCode_t DDS_DynamicType_get_annotation (DDS_DynamicType type,
						 DDS_AnnotationDescriptor *d,
						 unsigned index)
{
	return (xt_type_annotation_get ((DynType_t *) type, 0,
					(AnnotationDesc *) d, index));
}

int DDS_DynamicType_equals (DDS_DynamicType self, DDS_DynamicType other)
{
	return (xt_equals ((DynType_t *) self, 0, (DynType_t *) other));
}

/* xt_type_member_flags_modify -- Update specific member flags. */

DDS_ReturnCode_t xt_type_member_flags_modify (Type     *tp,
					      unsigned index,
					      unsigned mask,
					      unsigned flags)
{
	StructureType	*stp;
	Member		*mp = xt_type_member (tp, index);

	if (!mp || !mask)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((mask & XMF_KEY) != 0) {
		mp->is_key = (flags & XMF_KEY) != 0;
		xt_key_update (tp);
	}
	if ((mask & XMF_OPTIONAL) != 0) {
		mp->is_optional = (flags & XMF_OPTIONAL) != 0;
		if (mp->is_optional && tp->kind == DDS_STRUCTURE_TYPE) {
			stp = (StructureType *) tp;
			stp->optional = 1;
		}
	}
	if ((mask & XMF_SHAREABLE) != 0)
		mp->is_shareable = (flags & XMF_SHAREABLE) != 0;
	return (DDS_RETCODE_OK);
}

/* xt_type_member_flags_get -- Get member flags. */

DDS_ReturnCode_t xt_type_member_flags_get (Type     *tp,
					   unsigned index,
					   unsigned *flags)
{
	Member	*mp = xt_type_member (tp, index);

	if (!mp || !flags)
		return (DDS_RETCODE_BAD_PARAMETER);

	*flags = 0;
	if (mp->is_key)
		*flags |= XMF_KEY;
	if (mp->is_optional)
		*flags |= XMF_OPTIONAL;
	if (mp->is_shareable)
		*flags |= XMF_SHAREABLE;
	return (DDS_RETCODE_OK);
}

size_t xt_kind_size [] = {
	0, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 16, 1, 4
};

size_t xt_enum_size (const Type *tp)
{
	EnumType	*etp;
	BitSetType	*btp;

	if (tp->kind == DDS_ENUMERATION_TYPE) {
		etp = (EnumType *) tp;
		return ((etp->bound <= 8) ? 1 : 
			(etp->bound <= 16) ? 2 : 
			(etp->bound <= 32) ? 4 : 0);
	}
	else if (tp->kind == DDS_BITSET_TYPE) {
		btp = (BitSetType *) tp;
		return ((btp->bit_bound <= 8) ? 1 :
			(btp->bit_bound <= 16) ? 2 :
			(btp->bit_bound <= 32) ? 4 : 8);
	}
	else
		return (0);
}

#if !defined (CDR_ONLY) && defined (DDS_TYPECODE)

static TypeObject_t *topic_type_obj_create (Topic_t *tp, TypeObject_t *top)
{
	if (!top) {
		top = xmalloc (sizeof (TypeObject_t));
		if (!top)
			return (NULL);
	}
	top->magic = TO_MAGIC;
	top->vtc = NULL;
	top->ts = tp->type->type_support;
	top->ts->ts_users++;
	return (top);
}

static TypeObject_t *ep_type_obj_create (Endpoint_t *ep, TypeObject_t *top)
{
	DiscoveredReader_t	*drp;
	DiscoveredWriter_t	*dwp;
	VTC_Header_t		*p;

	if (!top) {
		top = xmalloc (sizeof (TypeObject_t));
		if (!top)
			return (NULL);
	}
	top->magic = TO_MAGIC;
	if (!entity_discovered (ep->entity.flags)) {
		top->vtc = NULL;
		top->ts = ep->topic->type->type_support;
		top->ts->ts_users++;
	}
	else {
		if (entity_writer (entity_type (&ep->entity))) {
			dwp = (DiscoveredWriter_t *) ep;
			top->vtc = dwp->dw_tc;
		}
		else {
			drp = (DiscoveredReader_t *) ep;
			top->vtc = drp->dr_tc;
		}
		if (!top->vtc || top->vtc == TC_IS_TS) {
			if (ep->topic->type && ep->topic->type->type_support)
				topic_type_obj_create (ep->topic, top);
			else {
				xfree (top);
				top = NULL;
			}
		}
		else {
			p = (VTC_Header_t *) top->vtc;
			p->nrefs_ext++;
			top->ts = NULL;
		}
	}
	return (top);
}

DDS_TypeObject DDS_TypeObject_create_from_topic (DDS_DomainParticipant p,
						 const char *name)
{
	TypeObject_t	*top;
	Domain_t	*dp;
	Topic_t		*tp;
	DDS_ReturnCode_t ret;

	if ((dp = domain_ptr (p, 1, &ret)) == NULL || ret)
		return (NULL);

	tp = topic_lookup (&dp->participant, name);
	if (!tp) {
		lock_release (dp->lock);
		return (NULL);
	}
	lock_take (tp->lock);
	lock_release (dp->lock);
	if (tp->writers)
		top = ep_type_obj_create (tp->writers, NULL);
	else if (tp->readers)
		top = ep_type_obj_create (tp->readers, NULL);
	else if (tp->type && tp->type->type_support)
		top = topic_type_obj_create (tp, NULL);
	else
		top = NULL;
	lock_release (tp->lock);
	return (top);
}

DDS_TypeObject DDS_TypeObject_create_from_key (DDS_DomainParticipant p,
					       DDS_BuiltinTopicKey_t *pkey,
					       DDS_BuiltinTopicKey_t *key)
{
	TypeObject_t	*top;
	Domain_t	*dp;
	Participant_t	*pp;
	Endpoint_t	*ep;
	DDS_ReturnCode_t ret;

	if ((dp = domain_ptr (p, 1, &ret)) == NULL || ret)
		return (NULL);

	pp = participant_lookup (dp, (GuidPrefix_t *) pkey);
	if (!pp) {
		lock_release (dp->lock);
		return (NULL);
	}
	ep = endpoint_lookup (pp, (EntityId_t *) &key->value [2]);
	if (ep)
		top = ep_type_obj_create (ep, NULL);
	else
		top = NULL;
	lock_release (dp->lock);
	return (top);
}
	
DDS_ReturnCode_t DDS_TypeObject_delete (DDS_TypeObject tobj)
{
	TypeObject_t	*top = (TypeObject_t *) tobj;

	if (!top || top->magic != TO_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (top->vtc)
		vtc_free (top->vtc);
	if (top->ts)
		DDS_TypeSupport_delete (top->ts);
	top->magic = 0;
	xfree (top);
	return (DDS_RETCODE_OK);
}

#endif	

void xt_data_free (const Type *tp, void *sample_data, int ptr)
{
	const Type	*ntp;
	void		*data;
	unsigned	i;

	if (ptr) {
		data = *((void **) sample_data);
		if (!data)
			return;
	}
	else
		data = sample_data;

	switch (tp->kind) {
		case DDS_ALIAS_TYPE: {
			const AliasType	*ap = (const AliasType *) tp;

			ntp = xt_type_ptr (ap->type.scope, ap->base_type);
			xt_data_free (ntp, data, 0);
			break;
		}
		case DDS_ARRAY_TYPE: {
			const ArrayType	*atp = (const ArrayType *) tp;
			unsigned char	*dp;
			unsigned	n;

			ntp = xt_type_ptr (tp->scope,
					   atp->collection.element_type);
			if (!ntp || xt_simple_type (ntp->kind) || tp->shared)
				break;

			for (i = 1, n = atp->bound [0]; i < atp->nbounds; i++)
				n *= atp->bound [i];
			for (i = 0, dp = (unsigned char *) data;
			     i < n;
			     i++, dp += atp->collection.element_size)
				xt_data_free (ntp, dp, tp->shared);
			break;
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			const SequenceType	*stp = (const SequenceType *) tp;
			DDS_VoidSeq		*seq = (DDS_VoidSeq *) data;
			unsigned char		*dp;

			ntp = xt_type_ptr (tp->scope,
					   stp->collection.element_type);
			if (!ntp || !seq->_own)
				break;

			if (seq->_length && 
			    seq->_buffer &&
			    (!xt_simple_type (ntp->kind) || tp->shared))
				for (i = 0, dp = (unsigned char *) seq->_buffer;
				     i < seq->_length;
				     i++, dp += stp->collection.element_size)
					xt_data_free (ntp, dp, tp->shared);

			dds_seq_cleanup (seq);
			break;
		}
		case DDS_STRING_TYPE: {
			const StringType	*stp = (const StringType *) tp;
			char			**spp = (char **) data;

			if (!stp->bound && *spp)
				mm_fcts.free_ (*spp);
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			const StructureType	*stp = (const StructureType *) tp;
			const Member		*mp;
			unsigned char		*dp = (unsigned char *) data;

			for (i = 0, mp = stp->member;
			     i < stp->nmembers;
			     i++, mp++) {
				ntp = xt_type_ptr (tp->scope, mp->id);
				if (!ntp)
					continue;

				xt_data_free (ntp,
					      dp + mp->offset, 
					      mp->is_optional || mp->is_shareable);
			}
			break;
		}
		case DDS_UNION_TYPE: {
			const UnionType		*utp = (const UnionType *) tp;
			const UnionMember	*ump = utp->member, *def_mp;
			unsigned char		*dp = (unsigned char *) data;
			int32_t			label;
			unsigned		j;
			int			found = 0;

			ntp = xt_type_ptr (tp->scope, ump->member.id);
                        if (!ntp) 
                                break;
			label = (int32_t) cdr_union_label (ntp, dp);
			for (i = 1, ++ump, def_mp = NULL;
			     i < utp->nmembers;
			     i++, ump++)
				if (ump->is_default)
					def_mp = ump;
				else if (ump->nlabels == 1) {
					if (ump->label.value == label) {
						found = 1;
						break;
					}
				}
				else {
					for (j = 0; j < ump->nlabels; j++)
						if (ump->label.list [j] == label) {
							found = 1;
							break;
						}
					if (found)
						break;
				}

			if (found || def_mp) {
				if (!found)
					ump = def_mp;

				ntp = xt_type_ptr (tp->scope, ump->member.id);
				if (!ntp) 
                                        break;
                                xt_data_free (ntp, dp + ump->member.offset,
					      ump->member.is_optional ||
					      ump->member.is_shareable);
			}
			break;
		}
		default:
			break;
	}
	if (ptr)
		mm_fcts.free_ (data);
}

int xt_data_copy (const Type *tp,
		  void       *dst_data,
		  const void *src_data,
		  size_t     size,
		  int        ptr,
		  int        shared)
{
	const Type	*ntp;
	const void	*sdata;
	void		*ddata;
	unsigned	i;
	int		error = DDS_RETCODE_OK;

	if (ptr) {
		sdata = *((const void **) src_data);
		if (!sdata) {
			*((void **) dst_data) = NULL;
			return (DDS_RETCODE_OK);
		}
		if (shared) {
			*((const void **) dst_data) = sdata;
			return (DDS_RETCODE_OK);
		}
		ddata = mm_fcts.alloc_ (size);
		*((void **) dst_data) = ddata;
		if (!ddata)
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	else {
		sdata = src_data;
		ddata = dst_data;
	}
	switch (tp->kind) {
		case DDS_ALIAS_TYPE: {
			const AliasType	*ap = (const AliasType *) tp;

			ntp = xt_type_ptr (ap->type.scope, ap->base_type);
			error = xt_data_copy (ntp, dst_data, src_data, size, ptr, shared);
			break;
		}
		case DDS_ARRAY_TYPE: {
			const ArrayType		*atp = (const ArrayType *) tp;
			const unsigned char	*sp;
			unsigned char		*dp;
			unsigned		n;

			ntp = xt_type_ptr (tp->scope,
					   atp->collection.element_type);
			if (!ntp)
				return (DDS_RETCODE_BAD_PARAMETER);

			for (i = 1, n = atp->bound [0]; i < atp->nbounds; i++)
				n *= atp->bound [i];
			if (xt_simple_type (ntp->kind) || tp->shared) {
				memcpy (ddata, sdata, atp->collection.element_size * n);
				return (DDS_RETCODE_OK);
			}
			for (i = 0, sp = (const unsigned char *) sdata,
				    dp = (unsigned char *) ddata;
			     i < n;
			     i++, sp += atp->collection.element_size,
				  dp += atp->collection.element_size)
				if (!error)
					error = xt_data_copy (ntp, dp, sp,
					                      atp->collection.element_size,
							      0, 0);
				else
					memset (dp, 0, atp->collection.element_size);
			if (!error)
				return (0);

			break;
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			const SequenceType	*stp = (const SequenceType *) tp;
			const DDS_VoidSeq	*sseq = (const DDS_VoidSeq *) sdata;
			DDS_VoidSeq		*dseq = (DDS_VoidSeq *) ddata;
			const unsigned char	*sp;
			unsigned char		*dp;

			ntp = xt_type_ptr (tp->scope,
					   stp->collection.element_type);
			if (!ntp ||
			    sseq->_esize != stp->collection.element_size)
				return (DDS_RETCODE_BAD_PARAMETER);

			dseq->_maximum = dseq->_length = 0;
			dseq->_esize = sseq->_esize;
			dseq->_own = 1;
			dseq->_buffer = NULL;
			if (!sseq->_length || !sseq->_buffer)
				return (DDS_RETCODE_OK);

			if (dds_seq_require (dseq, sseq->_length))
				return (DDS_RETCODE_OUT_OF_RESOURCES);

			if (xt_simple_type (ntp->kind) || tp->shared) {
				memcpy (dseq->_buffer, sseq->_buffer, sseq->_esize * sseq->_length);
				return (DDS_RETCODE_OK);
			}
			for (i = 0, sp = (const unsigned char *) sseq->_buffer,
				    dp = (unsigned char *) dseq->_buffer;
			     i < sseq->_length;
			     i++, sp += sseq->_esize, dp += dseq->_esize)
				if (!error)
					error = xt_data_copy (ntp, dp, sp,
							      sseq->_esize,
							      0, 0);
				else
					memset (dp, 0, dseq->_esize);

			if (!error)
				return (0);

			break;
		}
		case DDS_STRING_TYPE: {
			const StringType	*stp = (const StringType *) tp;
			const char		**spp = (const char **) sdata;
			char			**dpp = (char **) ddata;
			unsigned		n;

			if (!stp->bound)
				if (*spp) {
					n = strlen (*spp) + 1;
					*dpp = mm_fcts.alloc_ (n);
					if (!*dpp) {
						error = DDS_RETCODE_OUT_OF_RESOURCES;
						break;
					}
					memcpy (*dpp, *spp, n);
				}
				else
					*dpp = NULL;
			else {
				n = strlen (sdata) + 1;
				if (n >= stp->bound)
					memcpy (ddata, sdata, stp->bound + 1);
				else {
					memcpy (ddata, sdata, n);
					memset ((unsigned char *) ddata + n, 0, 
						stp->bound - n + 1);
				}
			}
			return (DDS_RETCODE_OK);
		}
		case DDS_STRUCTURE_TYPE: {
			const StructureType	*stp = (const StructureType *) tp;
			const Member		*mp;
			const unsigned char	*sp = (const unsigned char *) sdata;
			unsigned char		*dp = (unsigned char *) ddata;

			memset (dp, 0, stp->size);
			for (i = 0, mp = stp->member;
			     i < stp->nmembers;
			     i++, mp++) {
				ntp = xt_type_ptr (tp->scope, mp->id);
				if (!ntp)
					return (DDS_RETCODE_BAD_PARAMETER);

				if (xt_simple_type (ntp->kind)) {
					memcpy (dp + mp->offset, sp + mp->offset, xt_simple_size (ntp));
					continue;
				}
				else if (mp->is_shareable) {
					memcpy (dp + mp->offset, sp + mp->offset, sizeof (void *));
					continue;
				}
				if (!error)
					error = xt_data_copy (ntp,
							      dp + mp->offset,
							      sp + mp->offset,
							      0,
							      mp->is_optional || mp->is_shareable,
							      mp->is_shareable);
			}
			if (!error)
				return (0);

			break;
		}
		case DDS_UNION_TYPE: {
			const UnionType		*utp = (const UnionType *) tp;
			const UnionMember	*ump = utp->member, *def_mp;
			const unsigned char	*sp = (const unsigned char *) sdata;
			unsigned char		*dp = (unsigned char *) ddata;
			int32_t			label;
			unsigned		j;
			int			found = 0;
			int			error = DDS_RETCODE_OK;

			ntp = xt_type_ptr (tp->scope, ump->member.id);
			if (!ntp)
				return (DDS_RETCODE_BAD_PARAMETER);

			label = (int32_t) cdr_union_label (ntp, sp);
			for (i = 1, ++ump, def_mp = NULL;
			     i < utp->nmembers;
			     i++, ump++)
				if (ump->is_default)
					def_mp = ump;
				else if (ump->nlabels == 1) {
					if (ump->label.value == label) {
						found = 1;
						break;
					}
				}
				else {
					for (j = 0; j < ump->nlabels; j++)
						if (ump->label.list [j] == label) {
							found = 1;
							break;
						}
					if (found)
						break;
				}

			memcpy (dp, sp, xt_simple_size (ntp));
			if (found || def_mp) {
				if (!found)
					ump = def_mp;
				if (!ump)
					break;

				ntp = xt_type_ptr (tp->scope, ump->member.id);
				if (!ntp)
					return (DDS_RETCODE_BAD_PARAMETER);

				if (xt_simple_type (ntp->kind))
					memcpy (dp + ump->member.offset, sp + ump->member.offset, xt_simple_size (ntp));
				else if (ump->member.is_shareable)
					memcpy (dp + ump->member.offset, sp + ump->member.offset, sizeof (void *));
				else
					error = xt_data_copy (ntp,
							      dp + ump->member.offset,
							      sp + ump->member.offset,
							      0,
							      ump->member.is_optional ||
							      ump->member.is_shareable,
							      ump->member.is_shareable);
			}
			if (!error)
				return (0);

			break;
		}
		default:
			break;
	}
	if (error)
		xt_data_free (tp, dst_data, ptr);
	return (error);
}

int xt_data_equal (const Type *tp,
		   const void *src_data1,
		   const void *src_data2,
		   size_t     size,
		   int        ptr,
		   int        shared)
{
	const Type	*ntp;
	const void	*data1, *data2;
	unsigned	i;

	if (ptr) {
		data1 = *((const void **) src_data1);
		data2 = *((const void **) src_data2);
		if (!data1)
			return (!data2);

		if (shared && data1 == data2)
			return (1);
	}
	else {
		data1 = src_data1;
		data2 = src_data2;
	}
	switch (tp->kind) {
		case DDS_ALIAS_TYPE: {
			const AliasType	*ap = (const AliasType *) tp;

			ntp = xt_type_ptr (ap->type.scope, ap->base_type);
			return (xt_data_equal (ntp, src_data1, src_data2, size, ptr, shared));
		}
		case DDS_ARRAY_TYPE: {
			const ArrayType		*atp = (const ArrayType *) tp;
			const unsigned char	*p1, *p2;
			unsigned		n;

			ntp = xt_type_ptr (tp->scope,
					   atp->collection.element_type);
			if (!ntp)
				return (0);

			if (xt_simple_type (ntp->kind) || tp->shared)
				return (!memcmp (data1, data2, size));

			for (i = 0, n = 0; i < atp->nbounds; i++)
				n *= atp->bound [i];
			for (i = 0, p1 = (const unsigned char *) data1,
				    p2 = (const unsigned char *) data2;
			     i < n;
			     i++, p1 += atp->collection.element_size,
				  p2 += atp->collection.element_size)
				if (!xt_data_equal (ntp, p1, p2,
					            atp->collection.element_size,
						    tp->shared, tp->shared))
					return (0);

			return (1);
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			const SequenceType	*stp = (const SequenceType *) tp;
			const DDS_VoidSeq	*seq1 = (const DDS_VoidSeq *) data1;
			const DDS_VoidSeq	*seq2 = (const DDS_VoidSeq *) data2;
			const unsigned char	*p1, *p2;

			ntp = xt_type_ptr (tp->scope,
					   stp->collection.element_type);
			if (!ntp)
				return (0);

			if (seq1->_length != seq2->_length ||
			    seq1->_esize != stp->collection.element_size ||
			    seq2->_esize != stp->collection.element_size ||
			    seq1->_maximum < seq1->_length ||
			    seq2->_maximum < seq2->_maximum)
				return (0);

			if (!seq1->_length)
				return (1);

			if (!seq1->_buffer || !seq2->_buffer)
				return (0);

			if (xt_simple_type (ntp->kind) || tp->shared) {
				if (!memcmp (seq1->_buffer, seq2->_buffer, seq2->_esize * seq2->_length))
					return (1);
			}
			for (i = 0, p1 = (const unsigned char *) seq1->_buffer,
				    p2 = (const unsigned char *) seq2->_buffer;
			     i < seq1->_length;
			     i++, p1 += seq1->_esize, p2 += seq2->_esize)
				if (!xt_data_equal (ntp, p1, p2,
						    stp->collection.element_size,
						    tp->shared, tp->shared))
					return (0);

			return (1);
		}
		case DDS_STRING_TYPE: {
			const StringType	*stp = (const StringType *) tp;
			const char		**pp1 = (const char **) data1;
			const char		**pp2 = (const char **) data2;

			if (!stp->bound)
				if (*pp1 && *pp2)
					return (!strcmp (*pp1, *pp2));
				else
					return (*pp1 == *pp2);
			else
				return (!strcmp (data1, data2));
		}
		case DDS_STRUCTURE_TYPE: {
			const StructureType	*stp = (const StructureType *) tp;
			const Member		*mp;
			const unsigned char	*p2 = (const unsigned char *) data2;
			const unsigned char	*p1 = (const unsigned char *) data1;

			for (i = 0, mp = stp->member;
			     i < stp->nmembers;
			     i++, mp++) {
				ntp = xt_type_ptr (tp->scope, mp->id);
				if (!ntp)
					continue;

				if (xt_simple_type (ntp->kind)) {
					if (memcmp (p1 + mp->offset,
						    p2 + mp->offset,
						    xt_simple_size (ntp)))
						return (0);
					else
						continue;
				}
				if (mp->is_shareable &&
				    !memcmp (p1 + mp->offset,
					     p2 + mp->offset,
					     sizeof (void *)))
					continue;

				if (!xt_data_equal (ntp,
						    p1 + mp->offset,
						    p2 + mp->offset,
						    0,
						    mp->is_optional ||
						    mp->is_shareable,
						    mp->is_shareable))
					return (0);
			}
			return (1);
		}
		case DDS_UNION_TYPE: {
			const UnionType		*utp = (const UnionType *) tp;
			const UnionMember	*ump = utp->member, *def_mp;
			const unsigned char	*p1 = (const unsigned char *) data1;
			const unsigned char	*p2 = (const unsigned char *) data2;
			int32_t			label1, label2;
			unsigned		j;
			int			found = 0;

			ntp = xt_type_ptr (tp->scope, ump->member.id);
			if (!ntp)
				return (0);

			label1 = (int32_t) cdr_union_label (ntp, p1);
			label2 = (int32_t) cdr_union_label (ntp, p2);
			if (label1 != label2)
				return (0);

			for (i = 1, ++ump, def_mp = NULL;
			     i < utp->nmembers;
			     i++, ump++)
				if (ump->is_default)
					def_mp = ump;
				else if (ump->nlabels == 1) {
					if (ump->label.value == label1) {
						found = 1;
						break;
					}
				}
				else {
					for (j = 0; j < ump->nlabels; j++)
						if (ump->label.list [j] == label1) {
							found = 1;
							break;
						}
					if (found)
						break;
				}

			if (!found && !def_mp)
				return (1);

			if (!found)
				ump = def_mp;
			ntp = xt_type_ptr (tp->scope, ump->member.id);
			if (!ntp)
				return (0);

			if (xt_simple_type (ntp->kind))
				return (!memcmp (p1 + ump->member.offset,
						 p2 + ump->member.offset,
						 xt_simple_size (ntp)));

			return (xt_data_equal (ntp,
					       p1 + ump->member.offset,
					       p2 + ump->member.offset,
					       0,
					       ump->member.is_optional ||
					       ump->member.is_shareable,
					       ump->member.is_shareable));
		}
		default:
			break;
	}
	return (DDS_RETCODE_OK);
}

#ifdef DDS_DEBUG

static void xt_dump (unsigned indent, Type *tp, unsigned flags);

static void xt_dump_enum (unsigned indent, EnumType *tp)
{
	unsigned	i;
	int32_t		prev;
	EnumConst	*cp;

	if (tp->bound != 32) {
		dbg_printf ("@BitBound(%u)\r\n", tp->bound);
		dbg_print_indent (indent, NULL);
	}
	dbg_printf ("enum %s {\r\n", str_ptr (tp->type.name));
	indent++;
	for (i = 0, cp = tp->constant; i < tp->nconsts; i++, cp++) {
		dbg_print_indent (indent, NULL);
		if ((!i && cp->value) || (i && cp->value != prev + 1)) {
			dbg_printf ("@Value(%u)\r\n", cp->value);
			dbg_print_indent (indent, NULL);
		}
		prev = cp->value;
		dbg_printf ("%s", str_ptr (cp->name));
		if (i + 1 < tp->nconsts)
			dbg_printf (",");
		dbg_printf ("\r\n");
	}
	indent--;
	dbg_print_indent (indent, NULL);
	dbg_printf ("}");
}

static void xt_dump_bitset (unsigned indent, BitSetType *tp)
{
	unsigned	i;
	uint32_t	prev;
	Bit		*bp;

	dbg_printf ("@BitSet ");
	if (tp->bit_bound != 32) {
		dbg_printf ("@BitBound(%u)\r\n", tp->bit_bound);
		dbg_print_indent (indent, NULL);
	}
	dbg_printf ("enum %s {\r\n", str_ptr (tp->type.name));
	indent++;
	for (i = 0, bp = tp->bit; i < tp->nbits; i++, bp++) {
		dbg_print_indent (indent, NULL);
		if ((!i && bp->index) || (i && bp->index != prev + 1)) {
			dbg_printf ("@Value(%u)\r\n", bp->index);
			dbg_print_indent (indent, NULL);
		}
		prev = bp->index;
		dbg_printf ("%s", str_ptr (bp->name));
		if (i + 1 < tp->nbits)
			dbg_printf (",");
		dbg_printf ("\r\n");
	}
	indent--;
	dbg_print_indent (indent, NULL);
	dbg_printf ("}");
}

static void xt_dump_alias (unsigned indent, AliasType *tp, int def, unsigned flags)
{
	Type	*bp;

	dbg_printf ("typedef ");
	bp = xt_type_ptr (tp->type.scope, tp->base_type);
	if (def) {
		if (bp->kind == DDS_ALIAS_TYPE)
			dbg_printf ("%s", str_ptr (bp->name));
		else
			xt_dump (indent + 1, bp, flags);
	}
	else
		dbg_printf ("%s", str_ptr (bp->name));
	dbg_printf (" %s", str_ptr (tp->type.name));
}

static void xt_dump_array (unsigned indent, ArrayType *tp, int post, unsigned flags)
{
	Type		*ep;
	unsigned	i;

	if (post) {
		if (tp->collection.type.shared)
			dbg_printf (" @Shared ");
		for (i = 0; i < tp->nbounds; i++)
			dbg_printf ("[%u]", tp->bound [i]);
		if ((flags & XDF_ESIZE) != 0)
			dbg_printf (" /*ESize:%lu*/", (unsigned long) tp->collection.element_size);
	}
	else {
		ep = xt_type_ptr (tp->collection.type.scope,
				  tp->collection.element_type);
		if (ep->root)
			dbg_printf ("%s", str_ptr (ep->name));
		else
			xt_dump (indent, ep, flags);
	}
}

static void xt_dump_sequence (unsigned indent, SequenceType *tp, unsigned flags)
{
	Type	*ep;

	ep = xt_type_ptr (tp->collection.type.scope,
			  tp->collection.element_type);
	dbg_printf ("sequence<");
	if (tp->collection.type.shared)
		dbg_printf ("@Shared ");
	if (ep->root)
		dbg_printf ("%s", str_ptr (ep->name));
	else
		xt_dump (indent, ep, flags);
	dbg_printf (">");
	if (tp->bound)
		dbg_printf ("<%u>", tp->bound);
	if ((flags & XDF_ESIZE) != 0)
		dbg_printf (" /*ESize:%lu*/", (unsigned long) tp->collection.element_size);
}

static void xt_dump_string (StringType *tp, unsigned flags)
{
	if (tp->collection.element_type == DDS_CHAR_8_TYPE)
		dbg_printf ("string");
	else
		dbg_printf ("wstring");
	if (tp->bound)
		dbg_printf ("<%u>", tp->bound);
	if ((flags & XDF_ESIZE) != 0)
		dbg_printf (" /*ESize:%lu*/", (unsigned long) tp->collection.element_size);
}

static void xt_dump_map (unsigned indent, MapType *tp, unsigned flags)
{
	Type		*ep, *kp;
	StructureType	*sp;

	sp = (StructureType *) xt_type_ptr (tp->collection.type.scope,
					    tp->collection.element_type);
	dbg_printf ("map<");
	kp = xt_type_ptr (tp->collection.type.scope, sp->member [0].id);
	ep = xt_type_ptr (tp->collection.type.scope, sp->member [1].id);
	if (kp->root)
		dbg_printf ("%s", str_ptr (kp->name));
	else
		xt_dump (indent, kp, flags);
	dbg_printf (", ");
	if (tp->collection.type.shared)
		dbg_printf ("@Shared ");
	if (ep->root)
		dbg_printf ("%s", str_ptr (ep->name));
	else
		xt_dump (indent + 1, ep, flags);
	if (tp->bound)
		dbg_printf (", %u", tp->bound);
	dbg_printf (">");
	if ((flags & XDF_ESIZE) != 0)
		dbg_printf (" /*ESize:%lu*/", (unsigned long) tp->collection.element_size);
}

static const char *ext_str [] = {
	"FINAL_EXTENSIBILITY",
	"EXTENSIBLE_EXTENSIBILITY",
	"MUTABLE_EXTENSIBILITY"
};

static void xt_dump_union (unsigned indent, UnionType *tp, unsigned flags)
{
	UnionMember	*mp;
	Type		*dp, *xtp;
	unsigned	i, j, prev;

	if (tp->type.extensible != EXTENSIBLE)
		dbg_printf ("@Extensibility(%s) ", ext_str [tp->type.extensible]);
	if (tp->type.nested)
		dbg_printf ("@Nested ");
	dbg_printf ("union %s ", str_ptr (tp->type.name));
	if ((flags & XDF_SIZE) != 0)
		dbg_printf ("/*Size:%lu*/ ", (unsigned long) tp->size);

	dbg_printf ("switch (");
	dp = xt_type_ptr (tp->type.scope, tp->member [0].member.id);
	xt_dump (indent, dp, flags);
	dbg_printf (") {\r\n");
	indent++;
	prev = 0;
	for (i = 1, mp = &tp->member [1]; i < tp->nmembers; i++, mp++) {
		if ((flags & XDF_OFFSET) != 0)
			dbg_printf ("/*%lu*/", (unsigned long) mp->member.offset);

		dbg_print_indent (indent - 1, NULL);
		if (mp->is_default)
			dbg_printf ("    default:\r\n");
		else if (mp->nlabels == 1)
			dbg_printf ("    case %d:\r\n", mp->label.value);
		else
			for (j = 0; j < mp->nlabels; j++) {
				dbg_printf ("    case %d:\r\n", mp->label.list [j]);
				if (j + 1 < mp->nlabels)
					dbg_print_indent (indent - 1, NULL);
			}
		dbg_print_indent (indent, NULL);
		xtp = xt_type_ptr (tp->type.scope, mp->member.id);
		if (xtp->root)
			dbg_printf ("%s", str_ptr (xtp->name));
		else
			xt_dump (indent, xtp, flags);
		dbg_printf (" %s", str_ptr (mp->member.name));
		if (xtp->kind == DDS_ARRAY_TYPE)
			xt_dump_array (indent, (ArrayType *) xtp, 1, flags);
		dbg_printf (";");
		if (mp->member.is_key)
			dbg_printf ("  //@Key");
		if ((i == 1 && mp->member.member_id != 1) || 
		    mp->member.member_id != prev + 1)
			dbg_printf ("  //@ID(%u)", mp->member.member_id);
		prev = mp->member.member_id;
		if (mp->member.is_optional)
			dbg_printf ("  //@Optional");
		if (mp->member.is_shareable)
			dbg_printf ("  //@Shared");
		dbg_printf ("\r\n");
	}
	indent--;
	dbg_print_indent (indent, NULL);
	dbg_printf ("}");
}

static void xt_dump_struct (unsigned indent, StructureType *tp, unsigned flags)
{
	StructureType	*base_tp;
	Member		*mp;
	Type		*xtp;
	unsigned	i, prev;

	if (tp->type.extensible != EXTENSIBLE)
		dbg_printf ("@Extensibility(%s) ", ext_str [tp->type.extensible]);
	if (tp->type.nested)
		dbg_printf ("@Nested ");
	dbg_printf ("struct %s", str_ptr (tp->type.name));
	if (tp->base_type) {
		base_tp = (StructureType *) xt_type_ptr (tp->type.scope, tp->base_type);
		dbg_printf (":%s", str_ptr (base_tp->type.name));
	}
	if ((flags & XDF_SIZE) != 0)
		dbg_printf (" /*Size:%lu*/", (unsigned long) tp->size);
	dbg_printf (" {\r\n");
	indent++;
	for (i = 0, mp = tp->member; i < tp->nmembers; i++, mp++) {
		if ((flags & XDF_OFFSET) != 0)
			dbg_printf ("/*%lu*/", (unsigned long) mp->offset);
		dbg_print_indent (indent, NULL);
		xtp = xt_type_ptr (tp->type.scope, mp->id);
		if (xtp->root)
			dbg_printf ("%s", str_ptr (xtp->name));
		else
			xt_dump (indent, xtp, flags);
		dbg_printf (" %s", str_ptr (mp->name));
		if (xtp->kind == DDS_ARRAY_TYPE)
			xt_dump_array (indent, (ArrayType *) xtp, 1, flags);
		dbg_printf (";");
		if (mp->is_key)
			dbg_printf ("  //@Key");
		if ((i == 0 && mp->member_id != 0) || 
		    (i && mp->member_id != prev + 1))
			dbg_printf ("  //@ID(%u)", mp->member_id);
		if (mp->is_optional)
			dbg_printf ("  //@Optional");
		if (mp->is_shareable)
			dbg_printf ("  //@Shared");
		prev = mp->member_id;
		dbg_printf ("\r\n");
	}
	indent--;
	dbg_print_indent (indent, NULL);
	dbg_printf ("}");
}

static void xt_dump_annotation (unsigned indent, AnnotationType *tp, unsigned flags)
{
	TypeLib			*lp;
	AnnotationMember	*mp;
	Type			*xtp;
	unsigned		i;
	char			buf [80];

	dbg_printf ("@Annotation\r\n");
	dbg_print_indent (indent, NULL);
	dbg_printf ("local interface %s {\r\n", str_ptr (tp->type.name));
	indent++;
	for (i = 0, mp = tp->member; i < tp->nmembers; i++, mp++) {
		dbg_print_indent (indent, NULL);
		dbg_printf ("attribute ");
		xtp = xt_type_ptr (tp->type.scope, mp->member.id);
		/*xt_dump (indent + 1, xtp);*/
		dbg_printf ("%s %s", str_ptr (xtp->name), str_ptr (mp->member.name));
		if (xtp->kind == DDS_ARRAY_TYPE)
			xt_dump_array (indent, (ArrayType *) xtp, 1, flags);
		if (mp->default_value.type) {
			dbg_printf (" default ");
			if (mp->default_value.type == DDS_STRING_TYPE)
				dbg_printf ("\"%s\"",
				       str_ptr (mp->default_value.u.string_val));
			else if (mp->default_value.type == DDS_CHAR_8_TYPE)
				dbg_printf ("\'%c\'", mp->default_value.u.char_val);
			else {
				lp = xt_lib_ptr (tp->type.scope);
				ad_get_value (lp->domain, &mp->default_value,
						buf, sizeof (buf));
				dbg_printf ("%s", buf);
			}
		}
		dbg_printf (";\r\n");
	}
	indent--;
	dbg_print_indent (indent, NULL);
	dbg_printf ("}");
}

/* xt_dump -- Dump a type in IDL-like notation. */

static void xt_dump (unsigned indent, Type *tp, unsigned flags)
{
	if (tp->kind >= DDS_TYPEKIND_MAX) {
		dbg_printf ("?kind=%u?", tp->kind);
		return;
	}
	switch (tp->kind) {
		case DDS_ENUMERATION_TYPE:
			xt_dump_enum (indent, (EnumType *) tp);
			break;
		case DDS_BITSET_TYPE:
			xt_dump_bitset (indent, (BitSetType *) tp);
			break;
		case DDS_ALIAS_TYPE:
			xt_dump_alias (indent, (AliasType *) tp, 0, flags);
			break;
		case DDS_ARRAY_TYPE:
			xt_dump_array (indent, (ArrayType *) tp, 0, flags);
			break;
		case DDS_SEQUENCE_TYPE:
			xt_dump_sequence (indent, (SequenceType *) tp, flags);
			break;
		case DDS_STRING_TYPE:
			xt_dump_string ((StringType *) tp, flags);
			break;
		case DDS_MAP_TYPE:
			xt_dump_map (indent, (MapType *) tp, flags);
			break;
		case DDS_UNION_TYPE:
			xt_dump_union (indent, (UnionType *) tp, flags);
			break;
		case DDS_STRUCTURE_TYPE:
			xt_dump_struct (indent, (StructureType *) tp, flags);
			break;
		case DDS_ANNOTATION_TYPE:
			xt_dump_annotation (indent, (AnnotationType *) tp, flags);
			break;
		default:
			dbg_printf ("%s", xt_idl_names [tp->kind]);
			break;
	}
}

/* xt_dump_type -- Dump a type in IDL-like notation. */

void xt_dump_type (unsigned indent, Type *tp, unsigned flags)
{
	dbg_print_indent (indent, NULL);
	if (tp->kind >= DDS_ARRAY_TYPE && tp->kind <= DDS_MAP_TYPE)
		dbg_printf ("typedef ");

	xt_dump (indent, tp, flags);
	if (tp->kind >= DDS_ARRAY_TYPE && tp->kind <= DDS_MAP_TYPE) {
		dbg_printf (" %s", str_ptr (tp->name));
		if (tp->kind == DDS_ARRAY_TYPE)
			xt_dump_array (indent, (ArrayType *) tp, 1, flags);
	}
	dbg_printf (";");
	if (tp->building)
		dbg_printf (" /* !under construction! */");
	dbg_printf ("\r\n");
}

static int cmp_us (const void *a1, const void *a2)
{
	unsigned short	*s1 = (unsigned short *) a1;
	unsigned short	*s2 = (unsigned short *) a2;

	return ((int) *s1 - (int) *s2);
}

void xt_type_dump (unsigned scope, const char *name, unsigned flags)
{
	TypeLib		*lp;
	Type		*tp;
	int		id;
	unsigned	i;
	unsigned short	*sp;

	if (scope >= next_lib || (lp = libs [scope]) == NULL) {
		dbg_printf ("No such type library!\r\n");
		return;
	}
#ifdef DUMP_LIB
	xt_dump_lib (lp);
	if (lp != def_lib)
		xt_dump_lib (def_lib);
#endif
	if (name && name [0]) {
		id = xt_lib_lookup (lp, name);
		if (id < 0) {
			dbg_printf ("No such type!\r\n");
			return;
		}
		tp = xt_type_ptr (scope, lp->types [id]);
		dbg_printf ("%u.%u*%u:", scope, id, tp->nrefs);
		xt_dump_type (1, tp, flags);
	}
	else {
		sp = xmalloc (sizeof (unsigned short) * lp->ntypes);
		if (!sp) {
			dbg_printf ("Not enough memory!\r\n");
			return;
		}
		memcpy (sp, lp->types, lp->ntypes * sizeof (unsigned short));
		qsort (sp, lp->ntypes, sizeof (unsigned short), cmp_us);
		for (i = 0; i < lp->ntypes; i++) {
			id = sp [i];
			tp = xt_type_ptr (scope, id);
			dbg_printf ("%u.%u*%u:", scope, id, tp->nrefs);
			xt_dump_type (1, tp, flags);
		}
		xfree (sp);
	}
}

#endif

