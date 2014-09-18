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

/* xdata.c -- X-types data factory operations. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "log.h"
#include "error.h"
#include "debug.h"
#include "db.h"
#include "type_data.h"
#include "xtypes.h"

/* The Dynamic Data in-memory format is a very compact data representation that
   takes advantage of many DDS X-types mechanisms in order to keep memory requi-
   rements for data storage as small as possible.

   Following general guidelines were used while defining this structure:

	- Simple data subelements are always stored in native format within the
	  Dynamic data structure.
	- Complex data subelements are always contained within a data element
	  in an indirect manner via a nested Dynamic Data pointer.
	- Default values are never stored.

   This results in the following concrete representations:

	- The 'simple' types, e.g. Boolean, Byte, Int16/32/64, UInt16/32/64,
	  Float32/64/128, Char8/32, Enum16/32 and BitSet8/16/32/64 are always
	  stored in native format using appropriate native alignment.
	- Non-default arrays always get a separate Dynamic Data container for
	  the array data.
	  Array elements are either Dynamic Data container references (actual
	  or NULL) or contain 'simple' native data.
	- Non-default sequences and maps always get a separate Dynamic Data
	  container for the sequence/map data.
	- Non-empty strings always get a separate Dynamic Data container for
	  the string data.
	- Unions and structures contain a field list that describes the contents
	  of each contained field.  Only non-default fields are actually stored
	  in the data container.
	- Unions can have from 0 up to 2 fields, of which the first is always
	  the discriminant value, and the second (if present) being the union
	  variant data.
	- Structures have their fields in any order, typically the order is
	  determined at field creation time.  At generation, the user specifies
	  the fields one by one. At reception time, the received data field
	  order will determine the Dynamic Data field order.
	- Any non-simple type that consists completely of default values,
	  may be referenced as a NULL Dynamic Data reference.
 */

#ifdef XD_TRACE
#define	dm_print(s)		dbg_printf(s)
#define	dm_print1(s,a)		dbg_printf(s,a)
#define	dm_print2(s,a1,a2)	dbg_printf(s,a1,a2)
#else
#define	dm_print(s)
#define	dm_print1(s,a)
#define	dm_print2(s,a1,a2)
#endif

enum mem_block_en {
	MB_DYNTYPE,	/* Dynamic Type Reference. */
	MB_DYNDREF,	/* Dynamic Data Reference. */

	MB_END
};

static const char *mem_names [] = {
	"DYNTYPE",
	"DYNDREF",
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory used for typecode. */
static size_t		mem_size;

int xd_pool_init (const POOL_LIMITS *dyntypes, const POOL_LIMITS *dyndata)
{
	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		return (DDS_RETCODE_OK);
	}
	if (!dyntypes || !dyndata)
		return (DDS_RETCODE_BAD_PARAMETER);

	MDS_POOL_TYPE (mem_blocks, MB_DYNTYPE, *dyntypes, sizeof (DynType_t));
	MDS_POOL_TYPE (mem_blocks, MB_DYNDREF, *dyndata, sizeof (DynDataRef_t));

	/* Allocate all pools in one go. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		warn_printf ("xd_pool_init: not enough memory available!\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	log_printf (XTYPES_ID, 0, "locator_pool_init: %lu bytes allocated for locators.\r\n", (unsigned long) mem_size);
#endif
	return (DDS_RETCODE_OK);
}

void xd_pool_final (void)
{
	mds_free (mem_blocks, MB_END);
}

#ifdef DDS_DEBUG

/* xd_pool_dump -- Dump the memory usage of the dynamic pool. */

void xd_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}
#endif

DynType_t *xd_dyn_type_alloc (void)
{
	dm_print ("xd_dyn_type_alloc();\r\n");
	return (mds_pool_alloc (&mem_blocks [MB_DYNTYPE]));
}

void xd_dyn_type_free (DynType_t *tp)
{
	dm_print1 ("xd_dyn_type_free(%p);\r\n", tp);
	mds_pool_free (&mem_blocks [MB_DYNTYPE], tp);
}

DynDataRef_t *xd_dyn_dref_alloc (void)
{
	dm_print ("xd_dyn_dref_alloc();\r\n");
	return (mds_pool_alloc (&mem_blocks [MB_DYNDREF]));
}

void xd_dyn_dref_free (DynDataRef_t *rp)
{
	dm_print1 ("xd_dyn_dref_free(%p);\r\n", rp);
	mds_pool_free (&mem_blocks [MB_DYNDREF], rp);
}

DDS_ReturnCode_t xd_delete_data (DynData_t *ddp)
{
	DynData_t	**nested_dp;
	CollectionType	*ctp;
	ArrayType	*atp;
	Type		*etp;
	DynDataMember_t	*fp;
	DB		*dbp;
	unsigned	n, i;

	if (!ddp)
		return (DDS_RETCODE_ALREADY_DELETED);

	dm_print2 ("xd_delete_data(%p:%u);\r\n", ddp, ddp->nrefs);
	if (ddp->nrefs-- > 1)
		return (DDS_RETCODE_OK);

	if (ddp->type->kind == DDS_UNION_TYPE ||
	    ddp->type->kind == DDS_STRUCTURE_TYPE) {
		for (i = 0, fp = ddp->fields; 
		     i < ddp->nfields && (fp->flags & DMF_PRESENT) != 0; 
		     i++, fp++)
			if ((fp->flags & DMF_DYNAMIC) != 0) {
				assert (fp->offset <= ddp->dsize - sizeof (void *));
				nested_dp = (DynData_t **) (ddp->dp + fp->offset);
				if (*nested_dp)
					xd_delete_data (*nested_dp);
			}
	}
	else if (ddp->type->kind == DDS_ARRAY_TYPE || 
		 ddp->type->kind == DDS_SEQUENCE_TYPE ||
		 ddp->type->kind == DDS_MAP_TYPE) {
		ctp = (CollectionType *) ddp->type;
		etp = xt_type_ptr (ddp->type->scope, ctp->element_type);
		if (etp && !xt_simple_type (etp->kind)) {
			if (ddp->type->kind == DDS_ARRAY_TYPE) {
				atp = (ArrayType *) ctp;
				n = atp->bound [0];
				for (i = 1; i < atp->nbounds; i++)
					n *= atp->bound [i];
			}
			else
				n = (ddp->dsize - ddp->dleft) / sizeof (DynData_t *);
			for (i = 0, nested_dp = (DynData_t **) ddp->dp;
			     i < n;
			     i++, nested_dp++) {
				if (!*nested_dp)
					continue;

				xd_delete_data (*nested_dp);
			}
		}
	}
	if (ddp->dsize && (ddp->flags & DDF_FOREIGN) == 0)
		xfree (ddp->dp);

	if ((ddp->flags & DDF_DB) != 0) {
		dbp = (DB *) (((unsigned char *) ddp) - DB_HDRSIZE);
		db_free_data (dbp);
	}
	else
		xfree (ddp);
	return (DDS_RETCODE_OK);
}

DynData_t *xd_dyn_data_alloc (Type *tp, size_t length)
{
	DynData_t	*ddp;
#ifndef FORCE_MALLOC
	DB		*dbp;
#endif
	unsigned	n = DYN_DATA_SIZE + length;

	dm_print2 ("xd_dyn_data_alloc(%p,%lu);\r\n", tp, length);
#if !defined (BIGDATA) && (STRD_SIZE == 4)
	if (length > 0xffffU) {
		warn_printf ("DDS: DynDataAlloc(): data size too large!");
		return (NULL);
	}
#endif
#ifdef FORCE_MALLOC
	dm_print1 ("{T:xd_dd_alloc(%u)}\r\n", n);
	ddp = xmalloc (n);
	if (!ddp)
		return (NULL);

	ddp->flags = 0;
	ddp->dsize = length;
#else
	dbp = db_alloc_data (n, 1);
	if (!dbp)
		return (NULL);

	ddp = (DynData_t *) dbp->data;
	dm_print2 ("DB=%p->ddp=%p\r\n", dbp, ddp);
	ddp->flags = DDF_DB;
	ddp->dsize = dbp->size - DYN_DATA_SIZE;
#endif
	ddp->dleft = ddp->dsize;
	ddp->type = tp;
	ddp->nrefs = 1;
	ddp->dp = (unsigned char *) ddp + DYN_DATA_SIZE;
	ddp->flags |= DDF_FOREIGN | DDF_CONTAINED;
	return (ddp);
}

DynData_t *xd_dyn_data_grow (DynData_t *ddp, size_t nsize)
{
	DynData_t	*nddp;
	DB		*dbp, *ndbp;
	unsigned char	*ndp;
	size_t		dp_ofs;

	dm_print2 ("xd_dyn_data_grow(%p,%lu);\r\n", ddp, nsize);
	if (ddp->dsize >= nsize)
		return (ddp);

	if (ddp->dp && (ddp->flags & DDF_CONTAINED) != 0)
		dp_ofs = ddp->dp - (unsigned char *) ddp;
	else if ((ddp->flags & DDF_FOREIGN) == 0) {
		ndp = xrealloc (ddp->dp, DYN_DATA_SIZE + nsize);
		if (!ndp)
			return (NULL);

		ddp->dp = ndp;
		ddp->dleft += nsize - ddp->dsize;
		ddp->dsize = nsize;
		return (ddp);
	}
	else
		return (NULL);

	if ((ddp->flags & DDF_DB) == 0) {
		nddp = xrealloc (ddp, dp_ofs + nsize);
		if (!nddp)
			return (NULL);

		nddp->dleft += nsize - nddp->dsize;
		nddp->dsize = nsize;
	}
	else {
		dbp = (DB *) ((unsigned char *) ddp - DB_HDRSIZE);
		dm_print2 ("ddp=%p->DB=%p\r\n", ddp, dbp);
		ndbp = db_alloc_data (dp_ofs + nsize, 1);
		if (!ndbp)
			return (NULL);

		nddp = (DynData_t *) ndbp->data;
		memcpy (nddp, ddp, dp_ofs + ddp->dsize - ddp->dleft);
		nddp->dsize = ndbp->size - dp_ofs;
		nddp->dleft += nddp->dsize - ddp->dsize;
		db_free_data (dbp);
	}
	nddp->dp = (unsigned char *) nddp + dp_ofs;
	return (nddp);
}

static DynData_t *xd_create_data (Type *tp)
{
	DynData_t	*ddp;
	unsigned	fsize;

	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
			ddp = xd_dyn_data_alloc (tp, xt_kind_size [tp->kind]);
			break;

		case DDS_ENUMERATION_TYPE:
		case DDS_BITSET_TYPE:
			ddp = xd_dyn_data_alloc (tp, xt_simple_size (tp));
			break;

		case DDS_ARRAY_TYPE: {
			ArrayType	*atp = (ArrayType *) tp;
			Type		*etp;
			size_t		n;
			unsigned	i;

			etp = xt_type_ptr (tp->scope,
					   atp->collection.element_type);
			if (!etp)
				return (NULL);

			if (!xt_simple_type (etp->kind))
				n = sizeof (DynData_t *);
			else
			    	n = atp->collection.element_size;
			for (i = 0; i < atp->nbounds; i++)
				n *= atp->bound [i];
			ddp = xd_dyn_data_alloc (tp, n);
			if (!ddp)
				return (NULL);

			ddp->dleft -= n;
			memset (ddp->dp, 0, n);
			break;
		}
		case DDS_SEQUENCE_TYPE:
		case DDS_STRING_TYPE:
		case DDS_MAP_TYPE:
			ddp = xd_dyn_data_alloc (tp, 0);
			break;

		case DDS_UNION_TYPE: {
			fsize = DYN_EXTRA_AGG_SIZE (2);
			ddp = xd_dyn_data_alloc (tp, fsize + DYN_DATA_INC);
			if (ddp) {
				ddp->nfields = 0;
				memset (ddp->fields, 0,
						sizeof (DynDataMember_t) * 2);
				ddp->dsize -= fsize;
				ddp->dleft -= fsize;
				ddp->dp += fsize;
			}
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType *stp = (StructureType *) tp;

			fsize = DYN_EXTRA_AGG_SIZE (stp->nmembers);
			ddp = xd_dyn_data_alloc (tp, fsize + DYN_DATA_INC);
			if (ddp) {
				ddp->nfields = 0;
				memset (ddp->fields, 0,
				      sizeof (DynDataMember_t) * stp->nmembers);
				ddp->dsize -= fsize;
				ddp->dleft -= fsize;
				ddp->dp += fsize;
			}
			break;
		}
		default:
			ddp = NULL;
			break;
	}
	return (ddp);
}

DDS_DynamicData DDS_DynamicDataFactory_create_data (DDS_DynamicType type)
{
	DynData_t	*ddp;
	DynDataRef_t	*ddrp;
	Type		*tp = xt_d2type_ptr ((DynType_t *) type, 0);

	if (!tp)
		return (NULL);

	ddp = xd_create_data (tp);
	if (!ddp)
		return (NULL);

	dm_print1 ("{T:new_ddr(%lu)}\r\n", sizeof (DynDataRef_t));
	ddrp = xd_dyn_dref_alloc ();
	if (!ddrp) {
		xd_delete_data (ddp);
		return (NULL);
	}
	ddrp->magic = DD_MAGIC;
	ddrp->nrefs = 1;
	ddrp->ddata = ddp;
	return ((DDS_DynamicData) ddrp);
}

DDS_ReturnCode_t DDS_DynamicDataFactory_delete_data (DDS_DynamicData data)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	DDS_ReturnCode_t rc;

	if (!drp)
		return (DDS_RETCODE_ALREADY_DELETED);

	if (drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (drp->nrefs-- > 1)
		return (DDS_RETCODE_OK);

	rc = xd_delete_data (drp->ddata);
	xd_dyn_dref_free (drp);
	return (rc);
}

DDS_ReturnCode_t DDS_DynamicData_get_descriptor (DDS_DynamicData data,
						 DDS_MemberDescriptor *value,
						 DDS_MemberId id)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (value)
	ARG_NOT_USED (id)

	/* ... TBC ... */

	return (DDS_RETCODE_UNSUPPORTED);
}

DDS_ReturnCode_t DDS_DynamicData_set_descriptor (DDS_DynamicData data,
						 DDS_MemberId id,
						 DDS_MemberDescriptor *value)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (id)
	ARG_NOT_USED (value)

	/* ... TBC ... */

	return (DDS_RETCODE_UNSUPPORTED);
}

static DynDataMember_t *member_lookup (StructureType   *stp,
				       DynDataMember_t *fp,
				       DDS_MemberId    id)
{
	unsigned	i;

	for (i = 0; i < stp->nmembers; i++, fp++) {
		if ((fp->flags & DMF_PRESENT) == 0)
			break;

		if (stp->member [fp->index].member_id == id)
			return (fp);
	}
	return (NULL);
}

static int xd_equals (DynData_t *d1p, DynData_t *d2p)
{
	Type	*tp;

	if (d1p) {
		tp = d1p->type;
		if (d2p && d2p->type != tp)
			return (0);
	}
	else if (d2p)
		tp = d2p->type;
	else
		return (1);

	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
		case DDS_ENUMERATION_TYPE:
		case DDS_BITSET_TYPE:
			return (d1p != NULL &&
			        d2p != NULL &&
				!memcmp (d1p->dp, d2p->dp, 
						xt_simple_size (tp)));

		case DDS_ARRAY_TYPE: {
			ArrayType	*atp = (ArrayType *) tp;
			DynData_t	**sdp, **ddp;
			Type		*etp;
			unsigned	n, esize, i;

			if (!d1p || !d2p)
				return (0);

			etp = xt_type_ptr (tp->scope,
					   atp->collection.element_type);
			if (!etp)
				return (0);

			if (!xt_simple_type (etp->kind))
				esize = sizeof (DynData_t *);
			else
				esize = atp->collection.element_size;
			n = atp->bound [0];
			for (i = 1; i < atp->nbounds; i++)
				n *= atp->bound [i];

			if (xt_simple_type (etp->kind))
				return (!memcmp (d1p->dp, d2p->dp, esize * n));

			sdp = (DynData_t **) d1p->dp;
			ddp = (DynData_t **) d2p->dp;
			for (i = 0; i < n; i++, sdp++, ddp++) {
				if (*sdp == NULL && *ddp == NULL)
					continue;

				if ((*sdp == NULL && *ddp != NULL) ||
				    (*sdp != NULL && *ddp == NULL))
					return (0);

				if (*sdp == *ddp)
					continue;

				if (!xd_equals (*sdp, *ddp))
					return (0);
			}
			return (1);
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			SequenceType	*stp = (SequenceType *) tp;
			DynData_t	**sdp, **ddp;
			Type		*etp;
			unsigned	esize, n, i;

			if (!d1p)
				return (d2p->dsize - d2p->dleft == 0);

			else if (!d2p)
				return (d1p->dsize - d1p->dleft == 0);
				
			if ((d1p->dsize - d1p->dleft) != (d2p->dsize - d2p->dleft))
				return (0);

			etp = xt_type_ptr (tp->scope, stp->collection.element_type);
			if (xt_simple_type (etp->kind))
				return (!memcmp (d1p->dp, d2p->dp, d1p->dsize - d1p->dleft));

			esize = sizeof (DynData_t *);
			n = d1p->dsize / esize;
			sdp = (DynData_t **) d1p->dp;
			ddp = (DynData_t **) d2p->dp;
			for (i = 0; i < n; i++, sdp++, ddp++) {
				if (*sdp == NULL && *ddp == NULL)
					continue;

				if ((*sdp == NULL && *ddp != NULL) ||
				    (*sdp != NULL && *ddp == NULL))
					return (0);

				if (*sdp == *ddp)
					continue;

				if (!xd_equals (*sdp, *ddp))
					return (0);
			}
			return (1);
		}
		case DDS_STRING_TYPE:
			if (!d1p)
				return (d2p->dsize == 1);
			else if (!d2p)
				return (d1p->dsize == 1);
			else
				return (d1p->dsize == d2p->dsize &&
				        !memcmp (d1p->dp, d2p->dp, d1p->dsize - d1p->dleft));

		case DDS_UNION_TYPE: {
			DynDataMember_t	*f1p, *f2p;
			DynData_t	**sdp, **ddp;
			unsigned	i;

			if (!d1p || !d2p)
				return (0);

			for (i = 0, f1p = d1p->fields, f2p = d2p->fields;
			     i < 2;
			     i++, f1p++, f2p++) {
				if (((f1p->flags | f2p->flags) & DMF_PRESENT) == 0)
					break;

				if (((f1p->flags ^ f2p->flags) & DMF_PRESENT) != 0)
					return (0);

				if (f1p->index != f2p->index)
					return (0);

				if ((f1p->flags & DMF_DYNAMIC) == 0) {
					if (memcmp (d1p->dp + f1p->offset,
						    d2p->dp + f2p->offset,
						    f1p->length))
						return (0);
					else
						continue;
				}
				sdp = (DynData_t **) (d1p->dp + f1p->offset);
				ddp = (DynData_t **) (d2p->dp + f2p->offset);
				if (!xd_equals (*sdp, *ddp))
					return (0);
			}
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp;
			DynDataMember_t	*f1p, *f2p;
			DynData_t	**sdp, **ddp;
			unsigned	i;

			if (!d1p || !d2p)
				return (0);

			stp = (StructureType *) d1p->type;
			for (i = 0; i < stp->nmembers; i++) {
				f1p = member_lookup (stp, d1p->fields,
						     stp->member [i].member_id);
				f2p = member_lookup (stp, d2p->fields,
						     stp->member [i].member_id);

				if (!f1p && !f2p)
					continue;

				if ((f1p && !f2p) || (!f1p && f2p))
					return (0);

				if ((f1p->flags & DMF_DYNAMIC) == 0) {
					if (memcmp (d1p->dp + f1p->offset,
						    d2p->dp + f2p->offset,
						    f1p->length)) {
						dbg_printf ("Field %u doesn't match:\r\n\t", i);
						for (i = 0; i < f1p->length; i++)
							dbg_printf ("%02x ", d1p->dp [f1p->offset + i]);
						dbg_printf ("\r\n\t");
						for (i = 0; i < f2p->length; i++)
							dbg_printf ("%02x ", d2p->dp [f2p->offset + i]);
						dbg_printf ("\r\n");
						return (0);
					}
					else
						continue;
				}
				sdp = (DynData_t **) (d1p->dp + f1p->offset);
				ddp = (DynData_t **) (d2p->dp + f2p->offset);
				if (!xd_equals (*sdp, *ddp))
					return (0);
			}
			break;
		}
		default:
			return (0);
	}
	return (1);
}

int DDS_DynamicData_equals (DDS_DynamicData self,
			    DDS_DynamicData other)
{
	DynDataRef_t	*dr1p = (DynDataRef_t *) self,
			*dr2p = (DynDataRef_t *) other;
	DynData_t	*d1p, *d2p;

	if (!dr1p || dr1p->magic != DD_MAGIC || 
	    !dr2p || dr2p->magic != DD_MAGIC)
		return (0);

	d1p = dr1p->ddata;
	d2p = dr2p->ddata;
	if (d1p == d2p)
		return (1);

	return (xd_equals (d1p, d2p));
}

static int int_string (DDS_TypeKind k, void *p, char *s, unsigned max)
{
	unsigned	n;

	switch (k) {
		case DDS_INT_16_TYPE:
			n = snprintf (s, max, "%d", *((int16_t *) p));
			break;
		case DDS_UINT_16_TYPE:
			n = snprintf (s, max, "%u", *((uint16_t *) p));
			break;
		case DDS_INT_32_TYPE:
			n = snprintf (s, max, "%d", *((int32_t *) p));
			break;
		case DDS_UINT_32_TYPE:
			n = snprintf (s, max, "%u", *((uint32_t *) p));
			break;
		case DDS_INT_64_TYPE:
			n = snprintf (s, max, "%lld", *((long long int *) p));
			break;
		case DDS_UINT_64_TYPE:
			n = snprintf (s, max, "%llu", *((long long unsigned *) p));
			break;
		default:
			return (0);
	}
	return (n);
}

DDS_MemberId DDS_DynamicData_get_member_id_by_name (DDS_DynamicData self,
						    DDS_ObjectName name)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self;
	DynData_t	*dp;
	unsigned	i, n;
	char		buf [16];

	if (!drp ||drp->magic != DD_MAGIC)
		return (DDS_MEMBER_ID_INVALID);

	dp = drp->ddata;
	switch (dp->type->kind) {
		case DDS_UNION_TYPE: {
			UnionType	*utp = (UnionType *) dp->type;
			UnionMember	*ump;

			for (i = 0, ump = utp->member;
			     i < utp->nmembers;
			     i++, ump++)
				if (!strcmp (name, str_ptr (ump->member.name)))
					return (ump->member.member_id);

			break;
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp = (StructureType *) dp->type;
			Member		*mp;

			for (i = 0, mp = stp->member;
			     i < stp->nmembers;
			     i++, mp++)
				if (!strcmp (name, str_ptr (mp->name)))
					return (mp->member_id);

			break;
		}
		case DDS_MAP_TYPE: {
			MapType		*mp = (MapType *) dp->type;
			Type		*ktp;
			StructureType	*stp;
			DynData_t	**p, *sp, *kp;
			const char	*s;
			int		dyn_key;

			stp = (StructureType *) xt_type_ptr (
						   mp->collection.type.scope,
						   mp->collection.element_type);
                        if (stp)
			        ktp = (Type *) xt_type_ptr (mp->collection.type.scope,
						    stp->member [0].id);
			else
				ktp = NULL;

			if (!stp || !ktp)
				return (DDS_MEMBER_ID_INVALID);

			dyn_key = xt_simple_type (ktp->kind);
			for (i = 0, p = (DynData_t **) dp->dp;
			     i < dp->dsize / sizeof (DynData_t);
			     i++, p++) {
				sp = *p;
				if (dyn_key) {
					kp = *((DynData_t **) sp->dp);
					s = (char *) kp->dp;
				}
				else {
					n = int_string (ktp->kind, sp->dp, buf, sizeof (buf));
					if (!n)
						return (DDS_MEMBER_ID_INVALID);

					s = buf;
				}
				if (!strcmp (s, name))
					return (i);
			}
			break;
		}
	}
	return (DDS_MEMBER_ID_INVALID);
}

DDS_MemberId DDS_DynamicData_get_member_id_at_index (DDS_DynamicData self,
						     unsigned index)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self;
	DynData_t	*dp;
	DynDataMember_t	*fp;

	if (!drp ||drp->magic != DD_MAGIC)
		return (DDS_MEMBER_ID_INVALID);

	dp = drp->ddata;
	if (dp->type->kind == DDS_UNION_TYPE) {
		UnionType	*utp = (UnionType *) dp->type;

		if (index < 2) {
			fp = dp->fields + index;
			if ((fp->flags & DMF_PRESENT) != 0)
				return (utp->member [fp->index].member.member_id);
		}
	}
	else if (dp->type->kind != DDS_STRUCTURE_TYPE) {
		StructureType	*stp = (StructureType *) dp->type;

		if (index < stp->nmembers) {
			fp = dp->fields + index;
			if ((fp->flags & DMF_PRESENT) != 0)
				return (stp->member [fp->index].member_id);
		}
	}
	return (DDS_MEMBER_ID_INVALID);
}

unsigned DDS_DynamicData_get_item_count (DDS_DynamicData self)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self;
	DynData_t	*dp;

	if (!drp ||drp->magic != DD_MAGIC)
		return (0);

	dp = drp->ddata;
	switch (dp->type->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
		case DDS_ENUMERATION_TYPE:
			return (1);

		case DDS_BITSET_TYPE: {
			union {
				uint64_t	u64;
				uint32_t	u32 [2];
				uint16_t	u16 [4];
				unsigned char	u8 [8];
			} u;
			unsigned	n, c, i;

			u.u64 = 0;
			n = xt_simple_size (dp->type);
			memcpy (u.u8, dp->dp, n);
			c = 0;
			for (i = 0; i < n << 3; i++)
				if ((u.u8 [i >> 3] & (1 << (i & 7))) != 0)
					c++;
			return (c);
		}
		case DDS_ARRAY_TYPE: {
			ArrayType	*atp = (ArrayType *) dp->type;
			unsigned	n, i;

			n = atp->bound [0];
			for (i = 1; i < atp->nbounds; i++)
				n *= atp->bound [i];
			return (n);
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			SequenceType	*stp = (SequenceType *) dp->type;
			Type		*etp;
			unsigned	esize;

			etp = xt_type_ptr (dp->type->scope,
					   stp->collection.element_type);
			if (!etp)
				return (0);

			if (xt_simple_type (etp->kind))
				esize = stp->collection.element_size;
			else
				esize = sizeof (DynData_t *);
			return ((dp->dsize - dp->dleft) / esize);
		}
		case DDS_STRING_TYPE:
			return (dp->dsize);

		case DDS_UNION_TYPE:
			return (2);

		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp = (StructureType *) dp->type;
			DynDataMember_t	*fp;
			unsigned	i;

			for (i = 0, fp = dp->fields;
			     i < stp->nmembers;
			     i++, fp++)
				if ((fp->flags & DMF_PRESENT) == 0)
					break;
			return (i);
		}
		default:
			return (0);
	}
	return (0);
}

typedef enum {
	CM_ALL,
	CM_NON_KEY,
	CM_ELEMENT
} ClearMode_t;

static DDS_ReturnCode_t clear_values (DynData_t    *dp,
				      ClearMode_t  mode,
				      DDS_MemberId member_id,
				      int          is_key)
{
	int	clear;

	clear = (mode == CM_ALL || (mode == CM_NON_KEY && !is_key));
	switch (dp->type->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
			if (clear)
				memset (dp->dp, 0, xt_simple_size (dp->type));
			break;

		case DDS_ENUMERATION_TYPE: {
			EnumType	*etp = (EnumType *) dp->type;

			if (clear) {
				if (etp->bound <= 8)
					*dp->dp = etp->constant [0].value;
				else if (etp->bound <= 16)
					*((short *) dp->dp) = etp->constant [0].value;
				else if (etp->bound <= 32)
					memcpy (dp->dp, &etp->constant [0].value, 4);
			}
			break;
		}
		case DDS_BITSET_TYPE: {
			union u {
				uint64_t	u64;
				uint32_t	u32 [2];
				uint16_t	u16 [4];
				unsigned char	u8 [8];
			}		*up;

			if (clear)
				memset (dp->dp, 0, xt_simple_size (dp->type));
			else if (mode == CM_ELEMENT) {
				up = (union u *) dp->dp;
				up->u8 [member_id >> 3] &= ~(1 << (member_id & 7));
			}
			break;
		}
		case DDS_ARRAY_TYPE: {
			ArrayType	*atp = (ArrayType *) dp->type;
			Type		*etp;
			DynData_t	**ddp;
			unsigned char	*xp;
			unsigned	n, i, esize;

			etp = xt_type_ptr (dp->type->scope,
					   atp->collection.element_type);
			if (!etp)
				return (DDS_RETCODE_BAD_PARAMETER);

			n = atp->bound [0];
			for (i = 1; i < atp->nbounds; i++)
				n *= atp->bound [i];
			if (!xt_simple_type (etp->kind))
				esize  = sizeof (DynData_t *);
			else
				esize = atp->collection.element_size;
			if (clear) {
				if (!xt_simple_type (etp->kind))
					for (i = 0, ddp = (DynData_t **) dp->dp;
					     i < n;
					     i++, ddp++) {
						if (!*ddp)
							continue;

						xd_delete_data (*ddp);
						*ddp = NULL;
					} 
				else
					memset (dp->dp, 0, n * esize);
			}
			else if (mode == CM_ELEMENT) {
				xp = dp->dp + esize * member_id;
				if (!xt_simple_type (etp->kind)) {
					ddp = (DynData_t **) xp;
					if (*ddp) {
						xd_delete_data (*ddp);
						*ddp = NULL;
					}
				}
				else
					memset (xp, 0, esize);
			}
			break;
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			SequenceType	*stp = (SequenceType *) dp->type;
			Type		*etp;
			DynData_t	**ddp;
			unsigned char	*xp;
			unsigned	i, esize, nelems;

			etp = xt_type_ptr (dp->type->scope,
					   stp->collection.element_type);
			if (!etp)
				return (DDS_RETCODE_BAD_PARAMETER);

			if (!xt_simple_type (etp->kind))
				esize = sizeof (DynData_t *);
			else
				esize = stp->collection.element_size;
			nelems = dp->dsize / esize;
			if (!nelems)
				break;

			if (clear) {
				if (!xt_simple_type (etp->kind))
					for (i = 0, ddp = (DynData_t **) dp->dp;
					     i < nelems;
					     i++, ddp++) {
						if (!*ddp)
							continue;

						xd_delete_data (*ddp);
					}
				if ((dp->flags & DDF_FOREIGN) == 0) {
					xfree (dp->dp);
					dp->dp = NULL;
					dp->dsize = 0;
				}
				else
					dp->dleft = dp->dsize;
			}
			else if (mode == CM_ELEMENT) {
				xp = dp->dp + esize * member_id;
				if (member_id >= nelems)
					break;

				if (!xt_simple_type (etp->kind)) {
					ddp = (DynData_t **) xp;
					if (*ddp)
						xd_delete_data (*ddp);
				}
				if (nelems == 1) {
					if ((dp->flags & DDF_FOREIGN) == 0) {
						xfree (dp->dp);
						dp->dp = NULL;
						dp->dsize = 0;
					}
					else
						dp->dleft = dp->dsize;
				}
				else {
					if (member_id < nelems - 1)
						memmove (xp, xp + esize, 
						      (nelems - member_id - 1) *
						      esize);
					dp->dp = xrealloc (dp->dp, dp->dsize - esize);
				}
			}
			break;
		}
		case DDS_STRING_TYPE:
			if (clear) {
			 	if ((dp->flags & DDF_FOREIGN) == 0) {
					xfree (dp->dp);
					dp->dp = NULL;
					dp->dsize = 0;
				}
				else
					dp->dleft = dp->dsize;
			}
			break;

		case DDS_UNION_TYPE: {
			UnionType	*utp = (UnionType *) dp->type;
			UnionMember	*ump;
			DynDataMember_t	*fp = dp->fields;

			/* If discriminant selected clear full union. */
			if (mode == CM_ALL ||
			    (mode == CM_ELEMENT && !member_id)) {
				if ((dp->flags & DDF_FOREIGN) == 0) {
					xfree (dp->dp);
					dp->dp = NULL;
					dp->dsize = 0;
				}
				else
					dp->dleft = dp->dsize;
				fp [0].flags = 0;
				fp [1].flags = 0;
				dp->nfields = 0;
				break;
			}

			/* Specific field selected! */
			ump = &utp->member [fp [1].index];
			if (mode == CM_NON_KEY ||
			    (mode == CM_ELEMENT &&
			     ump->member.member_id == member_id)) {
				memset (dp->dp + ump->member.offset, 0,
					        dp->dsize - ump->member.offset);
				dp->nfields = 1;
			}
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp = (StructureType *) dp->type;
			Member		*mp;
			DynData_t	**nested_dp;
			DynDataMember_t	*fp, *nf;
			unsigned	i, j, esize;

			if (mode == CM_ALL) {
				if ((dp->flags & DDF_FOREIGN) == 0) {
					xfree (dp->dp);
					dp->dp = NULL;
					dp->dsize = 0;
				}
				else
					dp->dleft = dp->dsize;
				dp->nfields = 0;
				break;
			}
			for (i = 0, fp = dp->fields;
			     i < stp->nmembers;
			     i++, fp++) {
				if ((fp->flags & DMF_PRESENT) == 0)
					break;

				/* If clear not needed, skip field. */
				mp = &stp->member [fp->index];
				if ((mode == CM_NON_KEY && mp->is_key) ||
				    (mode == CM_ELEMENT && 
				     mp->member_id != member_id))
					continue;

				/* Field needs clearing. */
				esize = fp->length;
				if ((fp->flags & DMF_DYNAMIC) != 0) {
					esize = sizeof (DynData_t *);
					nested_dp = (DynData_t **) (dp->dp +
								    fp->offset);
					if (*nested_dp) {
						xd_delete_data (*nested_dp);
						*nested_dp = NULL;
					}
				}
				if (mp->is_optional) {
					if (i + 1 < stp->nmembers) {
						nf = fp + 1;
						memmove (dp->dp + fp->offset,
							 dp->dp + nf->offset,
							 dp->dsize - nf->offset);
					}
					for (j = i + 1; j < stp->nmembers; j++) {
						dp->fields [j - 1] = dp->fields [j];
						dp->fields [j - 1].offset -= esize;
					}
					dp->nfields--;
					dp->fields [stp->nmembers - 1].flags = 0;
					dp->dleft += esize;
				}
				else
					memset (dp->dp + fp->offset, 0, esize);
			}
			break;
		}
		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DynamicData_clear_all_values (DDS_DynamicData self)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self;
	DynData_t	*dp;

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = drp->ddata;
	return (clear_values (dp, CM_ALL, 0, 0));
}

DDS_ReturnCode_t DDS_DynamicData_clear_nonkey_values (DDS_DynamicData self)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self;
	DynData_t	*dp;

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = drp->ddata;
	return (clear_values (dp, CM_NON_KEY, 0, 0));
}

DDS_ReturnCode_t DDS_DynamicData_clear_value (DDS_DynamicData self,
					      DDS_MemberId id)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self;
	DynData_t	*dp;

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = drp->ddata;
	return (clear_values (dp, CM_ELEMENT, id, 0));
}

DDS_DynamicData DDS_DynamicData_loan_value (DDS_DynamicData self,
					    DDS_MemberId id)
{
	DynDataRef_t	*ddrp, *drp = (DynDataRef_t *) self;
	DynData_t	*dp, **edp, *ep;
	Type		*etp;
	DynDataMember_t	*fp;
	Member		*mp;
	unsigned	l;

	if (!drp || drp->magic != DD_MAGIC)
		return (NULL);

	dp = drp->ddata;
	if ((dp->flags & DDF_LOANED) != 0)
		return (NULL);

	if (dp->type->kind == DDS_UNION_TYPE) {
		UnionType	*utp = (UnionType *) dp->type;

		fp = &dp->fields [1];
		if ((fp->flags & DMF_PRESENT) == 0)
			return (NULL);

		mp = &utp->member [fp->index].member;
		if (mp->member_id != id)
			return (NULL);
	}
	else if (dp->type->kind == DDS_STRUCTURE_TYPE) {
		StructureType	*stp = (StructureType *) dp->type;
		unsigned	i;

		for (i = 0, fp = dp->fields; i < stp->nmembers; i++, fp++) {
			if ((fp->flags & DMF_PRESENT) == 0)
				return (NULL);

			mp = &stp->member [fp->index];
			if (mp->member_id == id)
				break;
		}
		if (i >= stp->nmembers)
			return (NULL);
	}
	else
		return (NULL);

	etp = xt_type_ptr (dp->type->scope, mp->id);
	if (!etp)
		return (NULL);

	if (!xt_simple_type (etp->kind)) {	/* Reference original data. */
		edp = (DynData_t **) (dp->dp + fp->offset);
		ep = *edp;
		ep->nrefs++;
	}
	else {	/* Make a copy of the data. */
		l = xt_simple_size (etp);
		ep = xd_dyn_data_alloc (etp, l);
		if (!ep)
			return (NULL);

		memcpy (ep->dp, dp->dp + fp->offset, l);
	}

	dm_print1 ("{T:new_ddr(%lu)}\r\n", sizeof (DynDataRef_t));
	ddrp = xd_dyn_dref_alloc ();
	if (!ddrp) {
		xd_delete_data (ep);
		return (NULL);
	}
	ddrp->magic = DD_MAGIC;
	ddrp->nrefs = 1;
	ddrp->ddata = ep;
	dp->flags |= DDF_LOANED;
	return ((DDS_DynamicData) ddrp);
}

DDS_ReturnCode_t DDS_DynamicData_return_loaned_value (DDS_DynamicData self,
						      DDS_DynamicData value)
{
	DynDataRef_t	*drp = (DynDataRef_t *) self,
			*drv = (DynDataRef_t *) value;
	DynData_t	*dp;

	if (!drp || drp->magic != DD_MAGIC || !drv || drv->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = drp->ddata;
	if ((dp->flags & DDF_LOANED) == 0)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	DDS_DynamicDataFactory_delete_data (value);
	dp->flags &= ~DDF_LOANED;
	return (DDS_RETCODE_OK);
}

DDS_DynamicData DDS_DynamicData_clone (DDS_DynamicData self)
{
	DynDataRef_t	*dp = (DynDataRef_t *) self;

	if (!dp)
		return (NULL);

	dp->nrefs++;
	return ((DDS_DynamicData) self);
}

# if 0
static int check_multiple (int           create,
			   unsigned      *length,
			   unsigned char *p, 
			   Type          *etp)
{
	if (create && *length != 1)
		return (0);
}
# endif

static unsigned char *dyn_value_ptr (DynData_t        **dpp,
				     DDS_MemberId     id,
				     Type             **type,
				     int              create,
				     int              multiple,
				     unsigned         *length,
				     int              *optional,
				     DDS_ReturnCode_t *error)
{
	DynData_t	*dp = *dpp, *ndp;

	*error = DDS_RETCODE_OK;
	*type = dp->type;
	if (xt_simple_type (dp->type->kind)) {
		if (create && multiple && *length != 1) {
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
		else if (multiple)
			*length = 1;
	}
	switch (dp->type->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
			if (id != DDS_MEMBER_ID_INVALID) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			return (dp->dp);

		case DDS_ENUMERATION_TYPE: {
			EnumType	*ep = (EnumType *) dp->type;

			if (id != DDS_MEMBER_ID_INVALID) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			if (ep->bound <= 8)
				*type = xt_primitive_type (DDS_BYTE_TYPE);
			else if (ep->bound <= 16)
				*type = xt_primitive_type (DDS_INT_16_TYPE);
			else
				*type = xt_primitive_type (DDS_INT_32_TYPE);
			return (dp->dp);
		}
		case DDS_BITSET_TYPE: {
			BitSetType	*bp = (BitSetType *) dp->type;

			if (id >= bp->bit_bound) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			if (bp->bit_bound <= 8)
				*type = xt_primitive_type (DDS_BYTE_TYPE);
			else if (bp->bit_bound <= 16)
				*type = xt_primitive_type (DDS_UINT_16_TYPE);
			else if (bp->bit_bound <= 32)
				*type = xt_primitive_type (DDS_UINT_32_TYPE);
			else
				*type = xt_primitive_type (DDS_UINT_64_TYPE);
			return (dp->dp);
		}
		case DDS_ARRAY_TYPE: {
			ArrayType	*atp = (ArrayType *) dp->type;
			Type		*etp;
			unsigned	n, i;

			n = atp->bound [0];
			for (i = 1; i < atp->nbounds; i++)
				n *= atp->bound [i];
			if (id >= n) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			etp = xt_type_ptr (dp->type->scope,
					  atp->collection.element_type);
			*type = etp;
			if (!etp) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			if (multiple) {
				if (create && *length > n - id) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					return (NULL);
				}
				else if (!create)
					*length = n - id;
			}
			if (xt_simple_type (etp->kind))
				return (dp->dp +
					id * atp->collection.element_size);
			else
				return (dp->dp + id * sizeof (DynData_t *));
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			SequenceType	*stp = (SequenceType *) dp->type;
			Type		*etp;
			unsigned	esize, max, nelems, increment;

			etp = xt_type_ptr (dp->type->scope,
					   stp->collection.element_type);
			if (!etp) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			if (xt_simple_type (etp->kind))
				esize = stp->collection.element_size;
			else
				esize = sizeof (DynData_t *);
			max = dp->dsize / esize;
			if (multiple) {
				if (create)
					nelems = *length;
				else
					nelems = (dp->dsize - dp->dleft) / esize;
			}
			else
				nelems = 1;
			if (id > max) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			else if (id + nelems > max) {
				if (!create) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					return (NULL);
				}
				increment = id + nelems - max;
				if ((stp->bound && max + increment > stp->bound)
#if !defined (BIGDATA)
				    || (max + increment) * esize > 65535
#endif
				) {
					*error = DDS_RETCODE_OUT_OF_RESOURCES;
					return (NULL);
				}
				dm_print2 ("{T:seq_data%c(%u)}\r\n", (dp->dsize) ? '+' : '=', dp->dsize + esize * increment);
				ndp = xd_dyn_data_grow (dp, esize * (max + increment));
				if (!ndp) {
					*error = DDS_RETCODE_OUT_OF_RESOURCES;
					return (NULL);
				}
				dp = *dpp = ndp;
				max += increment;
			}
			if (multiple)
				*length = nelems - id;
			*type = etp;
			if (create) {
				dp->dleft -= esize * nelems;
				memset (dp->dp + id * esize, 0, esize * nelems);
			}
			return (dp->dp + id * esize);
		}
		case DDS_STRING_TYPE:
			if (id != DDS_MEMBER_ID_INVALID) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			return (dp->dp);

		case DDS_UNION_TYPE: {
			UnionType	*utp = (UnionType *) dp->type;
			Type		*etp;
			UnionMember	*ump;
			unsigned	i;
			unsigned char	*p;

			if (create && !id) {
				etp = xt_type_ptr (dp->type->scope,
						   utp->member [0].member.id);
				*type = etp;
				if (multiple && *length > 1) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					return (NULL);
				}
				return ((unsigned char *) &dp->dp);
			}
			if ((!id && (dp->fields [0].flags & DMF_PRESENT) == 0) ||
			    ( id && (dp->fields [1].flags & DMF_PRESENT) == 0))
				ump = NULL;
			else {
				i = (id != 0);
				ump = &utp->member [dp->fields [i].index];
				if (ump->member.member_id != id)
					ump = NULL;
			}
			if (!ump) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			etp = xt_type_ptr (dp->type->scope, ump->member.id);
			*type = etp;
			if (!etp) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			p = dp->dp + dp->fields [i].offset;
# if 0
			if (multiple &&
			    !check_multiple (create, length, p, etp))
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
# endif
			return (p);
		}
		case DDS_STRUCTURE_TYPE: {
			StructureType	*stp = (StructureType *) dp->type;
			Type		*etp;
			Member		*mp = NULL;
			DynDataMember_t	*fp;
			unsigned	i, esize, f, ofs;

			for (f = 0, fp = dp->fields;
			     f < stp->nmembers;
			     f++, fp++) {
				if ((fp->flags & DMF_PRESENT) == 0)
					break;

				mp = &stp->member [fp->index];
				if (mp->member_id == id)
					break;
			}
			if (f >= stp->nmembers) {
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			if ((fp->flags & DMF_PRESENT) == 0) {
				for (i = 0, mp = stp->member;
				     i < stp->nmembers;
				     i++, mp++)
					if (mp->member_id == id)
						break;

				if (i >= stp->nmembers) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					return (NULL);
				}
				etp = xt_type_ptr (dp->type->scope, mp->id);
				*type = etp;
				if (!etp) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					return (NULL);
				}
				if (!create) {
					if (optional)
						*optional = mp->is_optional;
					*error = DDS_RETCODE_NO_DATA;
					return (NULL);
				}
				fp->flags = DMF_PRESENT;
				if (!xt_simple_type (etp->kind)) {
					esize = sizeof (DynData_t *);
					fp->flags |= DMF_DYNAMIC;
				}
				else
					esize = xt_simple_size (etp);
# if 0
				if (multiple &&
				    !check_multiple (create, length, p, etp)) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					fp->flags = 0;
					return (NULL);
				}
# endif
				ofs = dp->dsize - dp->dleft;
				fp->offset = (ofs + esize - 1) & ~(esize - 1);
				dp->dleft -= (fp->offset - ofs);
				if (dp->dleft < esize) {
					dm_print2 ("{T:struct_data%c(%u)}\r\n", (dp->dsize) ? '+' : '=', dp->dsize + esize);
					ndp = xd_dyn_data_grow (dp, dp->dsize + DYN_DATA_INC);
					if (!ndp) {
						*error = DDS_RETCODE_OUT_OF_RESOURCES;
						fp->flags = 0;
						return (NULL);
					}
					dp = *dpp = ndp;
					fp = &dp->fields [f];
				}
				dp->dleft -= esize;
				memset (dp->dp + fp->offset, 0, esize);
				fp->length = esize;
				fp->index = i;
				dp->nfields++;
			}
			else {
				etp = xt_type_ptr (dp->type->scope, mp->id);
				*type = etp;
				if (!etp) {
					*error = DDS_RETCODE_BAD_PARAMETER;
					return (NULL);
				}
			}
			return (dp->dp + fp->offset);
		}
		default:
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
	}
	*error = DDS_RETCODE_BAD_PARAMETER;
	return (NULL);
}

#define	MBOOL	(1 << (DDS_BOOLEAN_TYPE - 1))
#define	MBYTE	(1 << (DDS_BYTE_TYPE - 1))
#define	MI16	(1 << (DDS_INT_16_TYPE - 1))
#define	MUI16	(1 << (DDS_UINT_16_TYPE - 1))
#define	MI32	(1 << (DDS_INT_32_TYPE - 1))
#define	MUI32	(1 << (DDS_UINT_32_TYPE - 1))
#define	MI64	(1 << (DDS_INT_64_TYPE - 1))
#define	MUI64	(1 << (DDS_UINT_64_TYPE - 1))
#define MF32	(1 << (DDS_FLOAT_32_TYPE - 1))
#define MF64	(1 << (DDS_FLOAT_64_TYPE - 1))
#define	MF128	(1 << (DDS_FLOAT_128_TYPE - 1))
#define	MC8	(1 << (DDS_CHAR_8_TYPE - 1))
#define	MC32	(1 << (DDS_CHAR_32_TYPE - 1))

static const unsigned valid_copy [DDS_CHAR_32_TYPE] = {
/*Boolean*/	MBOOL | MI16 | MI32 | MI64 | MUI16 | MUI32 | MUI64 | MF32 | MF64 | MF128,
/*Byte*/	MBOOL | MBYTE | MI16 | MI32 | MI64 | MUI16 | MUI32 | MUI64 | MF32 | MF64 | MF128,
/*Int16*/	MI16 | MI32 | MI64 | MF32 | MF64 | MF128,
/*UInt16*/	MUI16 | MI32 | MI64 | MUI32 | MUI64 | MF32 | MF64 | MF128,
/*Int32*/	MI32 | MI64 | MF64 | MF128,
/*UInt32*/	MUI32 | MI64 | MUI64 | MF64 | MF128,
/*Int64*/	MI64 | MF128,
/*UInt64*/	MUI64 | MF128,
/*Float32*/	MF32 | MF64 | MF128,
/*Float64*/	MF64 | MF128,
/*Float128*/	MF128,
/*Char8*/	MC8 | MC32 | MI16 | MI32 | MI64 | MF32 | MF64 | MF128,
/*Char32*/	MC32 | MI32 | MI64 | MF32 | MF64 | MF128
};

#define	is_float(k)	((k) >= DDS_FLOAT_32_TYPE && (k) <= DDS_FLOAT_128_TYPE)

static DDS_ReturnCode_t copy_value (void         *dst,
				    DDS_TypeKind dkind,
				    const void   *src,
				    DDS_TypeKind skind)
{
	unsigned	smask;
	unsigned char	u8;
	signed char	i8;
	uint16_t	u16;
	int16_t		i16;
	int32_t		i32;
	uint32_t	u32;
	int64_t		i64;
	uint64_t	u64;
	float		f32;
	double		f64;
	long double	f128;
	unsigned	dsize, ssize;
	
	if (skind > DDS_CHAR_32_TYPE || skind < DDS_BOOLEAN_TYPE)
		return (DDS_RETCODE_BAD_PARAMETER);

	smask = valid_copy [skind - 1];
	if (((1 << (dkind - 1)) & smask) == 0)
		return (DDS_RETCODE_BAD_PARAMETER);

	dsize = xt_kind_size [dkind];
	ssize = xt_kind_size [skind];
	if (dsize == ssize &&
	    is_float (skind) == is_float (dkind))
		memcpy (dst, src, ssize);
	else {
		switch (skind) {
			case DDS_BOOLEAN_TYPE:
			case DDS_BYTE_TYPE:
				memcpy (&u8, src, 1);
				break;
			case DDS_CHAR_8_TYPE:
				memcpy (&i8, src, 1);
				if (dkind == DDS_INT_16_TYPE) {
					i16 = i8;
					memcpy (dst, &i16, 2);
				}
				else {
					i32 = i8;
					goto convert_i16;
				}
				break;
			case DDS_INT_16_TYPE:
				memcpy (&i16, src, 2);
				i32 = i16;

			    convert_i16:
				if (dkind == DDS_INT_32_TYPE ||
				    dkind == DDS_CHAR_32_TYPE)
					memcpy (dst, &i32, 4);
				else
					goto convert_i32;
				break;
			case DDS_UINT_16_TYPE:
				memcpy (&u16, src, 2);
				u32 = u16;
				if (dkind == DDS_INT_32_TYPE ||
				    dkind == DDS_UINT_32_TYPE)
					memcpy (dst, &u32, 4);
				else
					goto convert_u32;
				break;
			case DDS_INT_32_TYPE:
			case DDS_CHAR_32_TYPE:
				memcpy (&i32, src, 4);

			    convert_i32:
				if (dkind == DDS_INT_64_TYPE) {
					i64 = i32;
					memcpy (dst, &i64, 8);
				}
				else if (dkind == DDS_FLOAT_32_TYPE) {
					f32 = (float) i32;
					memcpy (dst, &f32, 4);
				}
				else if (dkind == DDS_FLOAT_64_TYPE) {
					f64 = (double) i32;
					memcpy (dst, &f64, 8);
				}
				else {
					f128 = i32;
					memcpy (dst, &f128, sizeof(long double));
				}
				break;
			case DDS_UINT_32_TYPE:
				memcpy (&u32, src, 4);

			    convert_u32:
				if (dkind == DDS_INT_64_TYPE ||
				    dkind == DDS_UINT_64_TYPE) {
					u64 = u32;
					memcpy (dst, &u64, 8);
				}
				else if (dkind == DDS_FLOAT_64_TYPE) {
					f64 = u32;
					memcpy (dst, &f64, 8);
				}
				else {
					f128 = u32;
					memcpy (dst, &f128, sizeof(long double));
				}
				break;
			case DDS_INT_64_TYPE:
				memcpy (&i64, src, 8);
				f128 = (long double) i64;
				goto copy_f128;
			case DDS_UINT_64_TYPE:
				memcpy (&u64, src, 8);
				f128 = (long double) u64;
				goto copy_f128;
			case DDS_FLOAT_32_TYPE:
				memcpy (&f32, src, 4);
				if (dkind == DDS_FLOAT_64_TYPE) {
					f64 = f32;
					memcpy (dst, &f64, 8);
				}
				else {
					f128 = f32;
					goto copy_f128;
				}
				break;
			case DDS_FLOAT_64_TYPE:
				memcpy (&f64, src, 8);
				f128 = f64;

			    copy_f128:
				memcpy (dst, &f128, sizeof(long double));
				break;
			default:
				return (DDS_RETCODE_BAD_PARAMETER);
		}
	}
	return (DDS_RETCODE_OK);
}

/* Returns the type kind that can be provided to the copy_value function for
   copying this type. */
static DDS_TypeKind copy_kind(const Type *tp)
{
	if (tp->kind == DDS_ENUMERATION_TYPE) {
		EnumType *etp = (EnumType *) tp;

		if (etp->bound <= 8)
			return DDS_BYTE_TYPE;
		else if (etp->bound <= 16)
			return DDS_INT_16_TYPE;
		else
			return DDS_INT_32_TYPE;
	}
	else if (tp->kind == DDS_BITSET_TYPE) {
		BitSetType  *btp = (BitSetType *) tp;

		if (btp->bit_bound <= 8)
			return DDS_BYTE_TYPE;
		else if (btp->bit_bound <= 16)
			return DDS_UINT_16_TYPE;
		else if (btp->bit_bound <= 32)
			return DDS_UINT_32_TYPE;
		else
			return DDS_UINT_64_TYPE;
	}
	return tp->kind;
}

static DDS_ReturnCode_t get_value (DDS_DynamicData d,
				   DDS_MemberId    id,
				   void            *v,
				   DDS_TypeKind    kind)
{
	DynDataRef_t		*dr = (DynDataRef_t *) d;
	unsigned char		*sp;
	Type			*stype;
	int			optional;
	DynData_t		*str;
	DDS_TypeKind		skind;
	DDS_ReturnCode_t	error;

	if (!v || !d || dr->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	optional = 0;
	sp = dyn_value_ptr (&dr->ddata, id, &stype, 0, 0, NULL, &optional, &error);
	if (!sp && (error != DDS_RETCODE_NO_DATA || optional))
		return (error);

	skind = copy_kind (stype);
	if (kind == DDS_STRING_TYPE) {
		if (skind != DDS_STRING_TYPE)
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!sp)
			*((char *) v) = '\0';
		else {
			str = *((DynData_t **) sp);
			if (!str)
				*((char *) v) = '\0';
			else
				memcpy (v, str->dp,
					strlen ((char *) (str->dp)) + 1);
		}
		error = DDS_RETCODE_OK;
	}
	else if (sp)
		error = copy_value (v, kind, sp, skind);
	else {
		memset (v, 0, xt_kind_size [kind]);
		error = DDS_RETCODE_OK;
	}
	return (error);
}

static DynData_t *new_str_value (const char *s, Type *tp)
{
	StringType	*stp = (StringType *) tp;
	DynData_t	*sp;
	size_t		l;

	l = strlen (s) + 1;
	if (stp->bound && l > stp->bound)
		l = stp->bound;
	dm_print1 ("{T:str(%lu)}\r\n", DYN_DATA_SIZE + l);
	sp = xd_dyn_data_alloc (tp, l);
	if (!sp)
		return (NULL);

	memcpy (sp->dp, s, l);
	sp->dleft -= l;
	dm_print1 ("new_str_value(dd=%p);\r\n", sp);
	return (sp);
}

static DynData_t *clone_data (DynData_t *dp)
{
	DynData_t	*ndp;

	dp->nrefs--;
	ndp = xd_dyn_data_alloc (dp->type, 
				 DYN_DATA_SIZE + dp->dsize - dp->dleft);
	if (!ndp)
		return (NULL);

	dp->type->nrefs++;
	ndp->nrefs = 1;
	switch (dp->type->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
		case DDS_ENUMERATION_TYPE:
		case DDS_BITSET_TYPE:
			memcpy (ndp->dp, dp->dp, xt_simple_size (dp->type));
			break;

		case DDS_ARRAY_TYPE: {
			ArrayType	*atp = (ArrayType *) dp->type;
			DynData_t	**sdp, **ddp;
			Type		*etp;
			size_t		n;
			unsigned	i;

			etp = xt_type_ptr (dp->type->scope,
					   atp->collection.element_type);
			if (!etp) {
				xd_delete_data (ndp);
				return (NULL);
			}
			n = atp->bound [0];
			for (i = 1; i < atp->nbounds; i++)
				n *= atp->bound [i];

			if (xt_simple_type (etp->kind)) {
				memcpy (ndp->dp, dp->dp, dp->dsize - dp->dleft);
				break;
			}
			sdp = (DynData_t **) dp->dp;
			ddp = (DynData_t **) ndp->dp;
			for (i = 0; i < n; i++) {
				if (!*sdp)
					*ddp = NULL;
				else {
					*ddp = *sdp;
					(*sdp)->nrefs++;
				}
				sdp++;
				ddp++;
			}
			break;
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			SequenceType	*stp = (SequenceType *) dp->type;
			DynData_t	**sdp, **ddp;
			Type		*etp;
			size_t		n;
			unsigned	i;

			etp = xt_type_ptr (dp->type->scope,
					   stp->collection.element_type);
			if (!etp) {
				xd_delete_data (ndp);
				return (NULL);
			}
			if (xt_simple_type (etp->kind)) {
				memcpy (ndp->dp, dp->dp, dp->dsize - dp->dleft);
				break;
			}
			sdp = (DynData_t **) dp->dp;
			ddp = (DynData_t **) ndp->dp;
			n = (dp->dsize - dp->dleft) / sizeof (DynData_t *);
			for (i = 0; i < n; i++) {
				if (!*sdp)
					*ddp = NULL;
				else {
					*ddp = *sdp;
					(*sdp)->nrefs++;
				}
				sdp++;
				ddp++;
			}
			break;
		}
		case DDS_STRING_TYPE:
			memcpy (ndp->dp, dp->dp, dp->dsize - dp->dleft);
			break;

		case DDS_UNION_TYPE:
		case DDS_STRUCTURE_TYPE: {
			DynDataMember_t	*f1p, *f2p;
			DynData_t	**ddp;
			unsigned	i;

			for (i = 0, f1p = ndp->fields, f2p = dp->fields;
			     i < dp->nfields;
			     i++, f1p++, f2p++) {
				if ((f2p->flags & DMF_PRESENT) == 0)
					break;

				f1p->flags = f2p->flags;
				memcpy (ndp->dp + f1p->offset,
					 dp->dp + f2p->offset,
					 f2p->length);

				if ((f2p->flags & DMF_DYNAMIC) == 0)
					continue;

				ddp = (DynData_t **) (dp->dp + f2p->offset);
				(*ddp)->nrefs++;
			}
			ndp->nfields = dp->nfields;
			break;
		}
		default:
			xd_delete_data (ndp);
			return (NULL);
	}
	return (ndp);
}

static DDS_ReturnCode_t set_value (DDS_DynamicData d,
				   DDS_MemberId    id,
				   const void      *v,
				   DDS_TypeKind    kind)
{
	DynDataRef_t		*dr = (DynDataRef_t *) d;
	unsigned char		*dp;
	Type			*dtype;
	DynData_t		*str;
	DDS_TypeKind		dkind;
	DDS_ReturnCode_t	error;

	if (!v || !d || dr->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dr->ddata->nrefs > 1) { /* Multiple data references, needs copy. */
		dr->ddata = clone_data (dr->ddata);
		if (!dr->ddata)
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	dp = dyn_value_ptr (&dr->ddata, id, &dtype, 1, 0, NULL, NULL, &error);
	if (!dp)
		return (error);

	dkind = copy_kind (dtype);
	if (kind == DDS_STRING_TYPE) {
		if (dkind != DDS_STRING_TYPE)
			return (DDS_RETCODE_BAD_PARAMETER);

		memcpy (&str, dp, sizeof (DynData_t *));
		if (str)
			xd_delete_data (str);

		if (v) {
			str = new_str_value (v, dtype);
			if (!str)
				return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else
			str = NULL;
		memcpy (dp, &str, sizeof (DynData_t *));
		error = DDS_RETCODE_OK;
	}
	else
		error = copy_value (dp, dkind, v, kind);
	return (error);
}

static DDS_ReturnCode_t get_values (DDS_DynamicData d,
				    DDS_MemberId    id,
				    void            *v,
				    DDS_TypeKind    kind)
{
	DynDataRef_t		*dr = (DynDataRef_t *) d;
	DDS_VoidSeq		*seqp;
	Type			*stype;
	DynData_t		*str;
	unsigned char		*sp, *dp;
	DDS_TypeKind		skind;
	unsigned		i, n, ssize, dsize;
	int			optional;
	DDS_ReturnCode_t	error;

	if (!v || !d || dr->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	optional = 0;
	sp = dyn_value_ptr (&dr->ddata, id, &stype, 0, 1, &n, &optional, &error);
	if (!sp && (error != DDS_RETCODE_NO_DATA || optional))
		return (error);

	skind = copy_kind (stype);
	seqp = (DDS_VoidSeq *) v;
	if (!n) {
		seqp->_length = 0;
		return (DDS_RETCODE_OK);
	}
	error = dds_seq_require (seqp, n);
	if (error)
		return (error);

	if (kind == DDS_STRING_TYPE) {
		ssize = sizeof (DynData_t *);
		dsize = sizeof (char *);
	}
	else {
		ssize = xt_kind_size [skind];
		dsize = xt_kind_size [kind];
	}
	for (i = 0, dp = seqp->_buffer;
	     i < n;
	     i++, dp += dsize) {
		if (kind == DDS_STRING_TYPE) {
			if (skind != DDS_STRING_TYPE)
				return (DDS_RETCODE_BAD_PARAMETER);

			if (!sp) {
				*((char **) dp) = NULL;
				continue;
			}
			memcpy (&str, sp, sizeof (DynData_t *));
			if (str)
				*((char **) dp) = (char *) str->dp;
			else
				*((char **) dp) = NULL;
		}
		else if (!sp) {
			memset (dp, 0, dsize);
			continue;
		}
		else {
			error = copy_value (dp, kind, sp, skind);
			if (error)
				return (error);
		}
		sp += ssize;
	}
	seqp->_length = n;
	return (DDS_RETCODE_OK);
}

static DDS_ReturnCode_t set_values (DDS_DynamicData d,
				    DDS_MemberId    id,
				    void            *v,
				    DDS_TypeKind    kind)
{
	DynDataRef_t		*dr = (DynDataRef_t *) d;
	DDS_VoidSeq		*seqp;
	Type			*dtype;
	DynData_t		*str;
	unsigned char		*dp, *sp;
	char			*s;
	DDS_TypeKind		dkind;
	DDS_ReturnCode_t	error;
	unsigned		ssize, dsize, i;

	if (!v || !d || dr->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	seqp = (DDS_VoidSeq *) v;
	if (dr->ddata->nrefs > 1) { /* Multiple data references, need to copy. */
		dr->ddata = clone_data (dr->ddata);
		if (!dr->ddata)
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	dp = dyn_value_ptr (&dr->ddata, id, &dtype, 1, 1, &seqp->_length, NULL, &error);
	if (!dp)
		return (error);

	dkind = copy_kind (dtype);
	if (kind == DDS_STRING_TYPE) {
		dsize = sizeof (DynData_t *);
		ssize = sizeof (char *);
	}
	else {
		ssize = xt_kind_size [kind];
		dsize = xt_kind_size [dkind];
	}
	for (i = 0, sp = seqp->_buffer;
	     i < seqp->_length;
	     i++, sp += ssize, dp += dsize) {
		if (kind == DDS_STRING_TYPE) {
			if (dkind != DDS_STRING_TYPE)
				return (DDS_RETCODE_BAD_PARAMETER);

			memcpy (&str, dp, sizeof (DynData_t *));
			if (str)
				xd_delete_data (str);

			memcpy (&s, sp, sizeof (char *));
			if (s) {
				str = new_str_value (s, dtype);
				if (!str)
					return (DDS_RETCODE_OUT_OF_RESOURCES);
			}
			else
				str = NULL;
			memcpy (dp, &str, sizeof (DynData_t *));
			error = DDS_RETCODE_OK;
		}
		else {
			error = copy_value (dp, dkind, sp, kind);
			if (error)
				return (error);
		}
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DynamicData_get_int32_value (DDS_DynamicData self,
						  int32_t *value,
						  DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_INT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_int32_value (DDS_DynamicData self,
						  DDS_MemberId id,
						  int32_t value)
{
	return (set_value (self, id, &value, DDS_INT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_uint32_value (DDS_DynamicData self,
						   uint32_t *value,
						   DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_UINT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_uint32_value (DDS_DynamicData self,
						   DDS_MemberId id,
						   uint32_t value)
{
	return (set_value (self, id, &value, DDS_UINT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_int16_value (DDS_DynamicData self,
						  int16_t *value,
						  DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_INT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_int16_value (DDS_DynamicData self,
						  DDS_MemberId id,
						  int16_t value)
{
	return (set_value (self, id, &value, DDS_INT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_uint16_value (DDS_DynamicData self,
						   uint16_t *value,
						   DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_UINT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_uint16_value (DDS_DynamicData self,
						   DDS_MemberId id,
						   uint16_t value)
{
	return (set_value (self, id, &value, DDS_UINT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_int64_value (DDS_DynamicData self,
						  int64_t *value,
						  DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_INT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_int64_value (DDS_DynamicData self,
						  DDS_MemberId id,
						  int64_t value)
{
	return (set_value (self, id, &value, DDS_INT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_uint64_value (DDS_DynamicData self,
						   uint64_t *value,
						   DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_UINT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_uint64_value (DDS_DynamicData self,
						   DDS_MemberId id,
						   uint64_t value)
{
	return (set_value (self, id, &value, DDS_UINT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_float32_value (DDS_DynamicData self,
						    float *value,
						    DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_FLOAT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_float32_value (DDS_DynamicData self,
						    DDS_MemberId id,
						    float value)
{
	return (set_value (self, id, &value, DDS_FLOAT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_float64_value (DDS_DynamicData self,
						    double *value,
						    DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_FLOAT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_float64_value (DDS_DynamicData self,
						    DDS_MemberId id,
						    double value)
{
	return (set_value (self, id, &value, DDS_FLOAT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_float128_value (DDS_DynamicData self,
						     long double *value,
						     DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_FLOAT_128_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_float128_value (DDS_DynamicData self,
						     DDS_MemberId id,
						     long double value)
{
	return (set_value (self, id, &value, DDS_FLOAT_128_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_char8_value (DDS_DynamicData self,
						  char *value,
						  DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_CHAR_8_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_char8_value (DDS_DynamicData self,
						  DDS_MemberId id,
						  char value)
{
	return (set_value (self, id, &value, DDS_CHAR_8_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_char32_value (DDS_DynamicData self,
						   wchar_t *value,
						   DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_CHAR_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_char32_value (DDS_DynamicData self,
						   DDS_MemberId id,
						   wchar_t value)
{
	return (set_value (self, id, &value, DDS_CHAR_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_byte_value (DDS_DynamicData self,
						 unsigned char *value,
						 DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_BYTE_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_byte_value (DDS_DynamicData self,
						 DDS_MemberId id,
						 unsigned char value)
{
	return (set_value (self, id, &value, DDS_BYTE_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_boolean_value (DDS_DynamicData self,
						    unsigned char *value,
						    DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_BOOLEAN_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_boolean_value (DDS_DynamicData self,
						    DDS_MemberId id,
						    unsigned char value)
{
	return (set_value (self, id, &value, DDS_BOOLEAN_TYPE));
}

int DDS_DynamicData_get_string_length (DDS_DynamicData d,
				       DDS_MemberId id)
{
	DynDataRef_t		*dr = (DynDataRef_t *) d;
	unsigned char		*sp;
	Type			*stype;
	int			optional;
	DynData_t		*str;
	DDS_ReturnCode_t	error;

	if (!d || dr->magic != DD_MAGIC)
		return (-1);

	optional = 0;
	sp = dyn_value_ptr (&dr->ddata, id, &stype, 0, 0, NULL, &optional, &error);
	if (!sp) {
		if (error == DDS_RETCODE_NO_DATA && !optional)
			return (0);
		else
			return (-1);
	}
	if (stype->kind != DDS_STRING_TYPE)
		return (-1);

	str = *((DynData_t **) sp);
	if (!str)
		return (0);
	else
		return (strlen ((char *) (str->dp)));
}

DDS_ReturnCode_t DDS_DynamicData_get_string_value (DDS_DynamicData self,
						   char *value,
						   DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_string_value (DDS_DynamicData self,
						   DDS_MemberId id,
						   const char *value)
{
	return (set_value (self, id, (const void *) value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_wstring_value (DDS_DynamicData self,
						    wchar_t *value,
						    DDS_MemberId id)
{
	return (get_value (self, id, value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_wstring_value (DDS_DynamicData self,
						    DDS_MemberId id,
						    const wchar_t *value)
{
	return (set_value (self, id, &value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_complex_value (DDS_DynamicData self,
						    DDS_DynamicData *value,
						    DDS_MemberId id)
{
	DynDataRef_t	 *drp, *erp = (DynDataRef_t *) self;
	DynData_t	 *etp, **sp, *vp;
	Type	 	 *stype;
	DDS_TypeKind	 skind;
	unsigned	 n;
	DDS_ReturnCode_t error;

	if (!self || !value || erp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sp = (DynData_t **) dyn_value_ptr (&erp->ddata, id, &stype, 0, 0, NULL, NULL, &error);
	if (!sp)
		return (error);

	skind = stype->kind;
	if (xt_simple_type (skind)) {
		n = xt_kind_size [skind];
		etp = xd_dyn_data_alloc (xt_primitive_type (skind), n);
		if (!etp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		memcpy (etp->dp, sp, n);
		vp = (DynData_t *) etp;
	}
	else {
		vp = (DynData_t *) *sp;
		(*sp)->nrefs++;
	}
	dm_print1 ("{T:new_ddr(%lu)}\r\n", sizeof (DynDataRef_t));
	drp = xd_dyn_dref_alloc ();
	if (!drp) {
		xd_delete_data (vp);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	drp->magic = DD_MAGIC;
	drp->nrefs = 1;
	drp->ddata = vp;
	*value = (DDS_DynamicData) drp;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DynamicData_set_complex_value (DDS_DynamicData self,
						    DDS_MemberId id,
						    DDS_DynamicData value)
{
	DynDataRef_t	 *drp = (DynDataRef_t *) self,
			 *vrp = (DynDataRef_t *) value;
	DynData_t	 *etp, **dp;
	Type	 	 *dtype;
	void		 *p;
	DDS_TypeKind	 dkind;
	DDS_ReturnCode_t error;

	if (!self || !value || drp->magic != DD_MAGIC || vrp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (drp->ddata->nrefs > 1) { /* Multiple data references, need to copy. */
		drp->ddata = clone_data (drp->ddata);
		if (!drp->ddata)
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	p = (void *) dyn_value_ptr (&drp->ddata, id, &dtype, 1, 0, NULL, NULL, &error);
	if (!p)
		return (error);

	dkind = dtype->kind;
	etp = vrp->ddata;
	if (xt_simple_type (etp->type->kind) && xt_simple_type (dkind))
		error = copy_value (p, dkind, etp->dp, etp->type->kind);
	else if	(!xt_simple_type (etp->type->kind) && !xt_simple_type (dkind)) {
		dp = (DynData_t **) p;
		if (*dp)
			xd_delete_data (*dp);

		*dp = etp;
		etp->nrefs++;
		error = DDS_RETCODE_OK;
	}
	else
		error = DDS_RETCODE_BAD_PARAMETER;

	return (error);
}

DDS_ReturnCode_t DDS_DynamicData_get_int32_values (DDS_DynamicData self,
						   DDS_Int32Seq *value,
						   DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_INT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_int32_values (DDS_DynamicData self,
						   DDS_MemberId id,
						   DDS_Int32Seq *value)
{
	return (set_values (self, id, value, DDS_INT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_uint32_values (DDS_DynamicData self,
						    DDS_UInt32Seq *value,
						    DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_UINT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_uint32_values (DDS_DynamicData self,
						    DDS_MemberId id,
						    DDS_UInt32Seq *value)
{
	return (set_values (self, id, value, DDS_UINT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_int16_values (DDS_DynamicData self,
						   DDS_Int16Seq *value,
						   DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_INT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_int16_values (DDS_DynamicData self,
						   DDS_MemberId id,
						   DDS_Int16Seq *value)
{
	return (set_values (self, id, value, DDS_INT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_uint16_values (DDS_DynamicData self,
						    DDS_UInt16Seq *value,
						    DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_UINT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_uint16_values (DDS_DynamicData self,
						    DDS_MemberId id,
						    DDS_UInt16Seq *value)
{
	return (set_values (self, id, value, DDS_UINT_16_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_int64_values (DDS_DynamicData self,
						   DDS_Int64Seq *value,
						   DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_INT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_int64_values (DDS_DynamicData self,
						   DDS_MemberId id,
						   DDS_Int64Seq *value)
{
	return (set_values (self, id, value, DDS_INT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_uint64_values (DDS_DynamicData self,
						    DDS_UInt64Seq *value,
						    DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_UINT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_uint64_values (DDS_DynamicData self,
						    DDS_MemberId id,
						    DDS_UInt64Seq *value)
{
	return (set_values (self, id, value, DDS_UINT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_float32_values (DDS_DynamicData self,
						     DDS_Float32Seq *value,
						     DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_FLOAT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_float32_values (DDS_DynamicData self,
						     DDS_MemberId id,
						     DDS_Float32Seq *value)
{
	return (set_values (self, id, value, DDS_FLOAT_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_float64_values (DDS_DynamicData self,
						     DDS_Float64Seq *value,
						     DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_FLOAT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_float64_values (DDS_DynamicData self,
						     DDS_MemberId id,
						     DDS_Float64Seq *value)
{
	return (set_values (self, id, value, DDS_FLOAT_64_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_float128_values (DDS_DynamicData self,
						      DDS_Float128Seq *value,
						      DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_FLOAT_128_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_float128_values (DDS_DynamicData self,
						      DDS_MemberId id,
						      DDS_Float128Seq *value)
{
	return (set_values (self, id, value, DDS_FLOAT_128_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_char8_values (DDS_DynamicData self,
						   DDS_CharSeq *value,
						   DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_CHAR_8_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_char8_values (DDS_DynamicData self,
						   DDS_MemberId id,
						   DDS_CharSeq *value)
{
	return (set_values (self, id, value, DDS_CHAR_8_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_char32_values (DDS_DynamicData self,
						    DDS_WcharSeq *value,
						    DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_CHAR_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_char32_values (DDS_DynamicData self,
						    DDS_MemberId id,
						    DDS_WcharSeq *value)
{
	return (set_values (self, id, value, DDS_CHAR_32_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_byte_values (DDS_DynamicData self,
						  DDS_ByteSeq *value,
						  DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_BYTE_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_byte_values (DDS_DynamicData self,
						  DDS_MemberId id,
						  DDS_ByteSeq *value)
{
	return (set_values (self, id, value, DDS_BYTE_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_boolean_values (DDS_DynamicData self,
						     DDS_BooleanSeq *value,
						     DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_BOOLEAN_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_boolean_values (DDS_DynamicData self,
						     DDS_MemberId id,
						     DDS_BooleanSeq *value)
{
	return (set_values (self, id, value, DDS_BOOLEAN_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_string_values (DDS_DynamicData self,
						    DDS_StringSeq *value,
						    DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_string_values (DDS_DynamicData self,
						    DDS_MemberId id,
						    DDS_StringSeq *value)
{
	return (set_values (self, id, value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_get_wstring_values (DDS_DynamicData self,
						     DDS_WstringSeq *value,
						     DDS_MemberId id)
{
	return (get_values (self, id, value, DDS_STRING_TYPE));
}

DDS_ReturnCode_t DDS_DynamicData_set_wstring_values (DDS_DynamicData self,
						     DDS_MemberId id,
						     DDS_WstringSeq *value)
{
	return (set_values (self, id, value, DDS_STRING_TYPE));
}

#ifdef DDS_DEBUG

static void dump_indent (unsigned indent)
{
	unsigned	i;

	for (i = 0; i < indent; i++)
		dbg_printf ("  ");
}

static void dump_xdata (const unsigned char *p, unsigned len)
{
	unsigned	n;

	dbg_printf ("0x");
	for (n = 0; n < len; n++)
		dbg_printf ("%02x", p [n]);
}

void xd_dump (unsigned indent, const DynData_t *ddp)
{
	dump_indent (indent);
	if (!ddp) {
		dbg_printf ("<null>\r\n");
		return;
	}
	dbg_printf ("%p:{type=%p,dp=%p,flags=%u,nrefs=%u,dsize=%lu,dleft=%lu,",
			(void *) ddp, (void *) ddp->type, ddp->dp,
			ddp->flags, ddp->nrefs,
			(unsigned long) ddp->dsize, (unsigned long) ddp->dleft);
	switch (ddp->type->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		case DDS_FLOAT_128_TYPE:
			dbg_printf ("%s:kind_size=%lu", xt_primitive_names [ddp->type->kind], (unsigned long) xt_kind_size [ddp->type->kind]);
			dump_xdata (ddp->dp, xt_kind_size [ddp->type->kind]);
			break;
		case DDS_ENUMERATION_TYPE:
		case DDS_BITSET_TYPE:
			if (ddp->type->kind == DDS_ENUMERATION_TYPE)
				dbg_printf ("enum");
			else
				dbg_printf ("bitset");
			dbg_printf (":simple_size=%lu", (unsigned long) (xt_simple_size (ddp->type)));
			dump_xdata (ddp->dp, xt_simple_size (ddp->type));
			break;
		case DDS_ARRAY_TYPE: {
			const ArrayType		*atp = (const ArrayType *) ddp->type;
			const Type		*etp;
			const DynData_t		*ep;
			const unsigned char	*p;
			size_t			n;
			unsigned		i, b;

			etp = xt_type_ptr (ddp->type->scope,
					   atp->collection.element_type);
			if (!xt_simple_type (etp->kind))
				n = sizeof (DynData_t *);
			else
			    	n = atp->collection.element_size;
			for (i = 0, b = 1; i < atp->nbounds; i++)
				b *= atp->bound [i];
			dbg_printf ("Array[%u]:\r\n", b);
			for (p = ddp->dp, i = 0; i < b; i++, p += n)
				if (!xt_simple_type (etp->kind)) {
					memcpy (&ep, p, n);
					xd_dump (indent + 1, ep);
				}
				else {
					if (i)
						dbg_printf (",");
					else
						dump_indent (indent + 1);
					dump_xdata (p, n);
				}

			if (xt_simple_type (etp->kind))
				dbg_printf ("\r\n");
			dump_indent (indent);
			break;
		}
		case DDS_MAP_TYPE:
		case DDS_SEQUENCE_TYPE: {
			const SequenceType	*stp = (const SequenceType *) ddp->type;
			const DynData_t		*ep;
			const unsigned char	*p;
			const Type		*etp;
			size_t			n, es;
			unsigned		i;

			if (ddp->type->kind == DDS_MAP_TYPE)
				dbg_printf ("Map<");
			else
				dbg_printf ("Sequence<");
			etp = xt_type_ptr (ddp->type->scope,
					   stp->collection.element_type);
			if (!xt_simple_type (etp->kind))
				es = sizeof (DynData_t *);
			else
				es = stp->collection.element_size;
			n = (ddp->dsize - ddp->dleft) / es;
			dbg_printf ("%lu>", (unsigned long) n);
			if (!n)
				break;

			dbg_printf (":\r\n");
			for (p = ddp->dp, i = 0; i < n; i++, p += es)
				if (!xt_simple_type (etp->kind)) {
					memcpy (&ep, p, es);
					xd_dump (indent + 1, ep);
				}
				else {
					if (i)
						dbg_printf (",");
					else
						dump_indent (indent + 1);
					dump_xdata (p, es);
				}

			if (xt_simple_type (etp->kind))
				dbg_printf ("\r\n");
			dump_indent (indent);
			break;
		}
		case DDS_STRING_TYPE: {
			const char	*cp;
			unsigned	i, n;
			int		in_str;

			dbg_printf ("String:");
			n = ddp->dsize - ddp->dleft;
			if (!n)
				break;

			dbg_printf ("\r\n");
			dump_indent (indent + 1);
			in_str = 0;
			for (i = 0, cp = (char *) ddp->dp; i < n; i++, cp++)
				if (*cp >= ' ' && *cp <= '~') {
					if (!in_str) {
						if (i)
							dbg_printf (", ");
						dbg_printf ("\"");
						in_str = 1;
					}
					dbg_printf ("%c", *cp);
				}
				else {
					if (in_str) {
						dbg_printf ("\"");
						in_str = 0;
					}
					if (i)
						dbg_printf (", ");
					dbg_printf ("0x%02x", *cp);
				}
			if (in_str)
				dbg_printf ("\"");
			dbg_printf ("\r\n");
			dump_indent (indent);
			break;
		}
		case DDS_STRUCTURE_TYPE: {
			const StructureType	*stp;
			const Member		*mp;
			const DynDataMember_t	*fp;
			const DynData_t		**fdp;
			unsigned		i;

			dbg_printf ("Struct:");
			stp = (const StructureType *) ddp->type;
			dbg_printf ("\r\n");
			for (i = 0, fp = ddp->fields; i < ddp->nfields; i++, fp++) {
				if ((fp->flags & DMF_PRESENT) == 0)
					break;

				mp = &stp->member [fp->index];
				dump_indent (indent + 1);
				dbg_printf ("%s:\t", str_ptr (mp->name));
				if ((fp->flags & DMF_DYNAMIC) == 0) {
					dump_xdata (ddp->dp + fp->offset, fp->length);
					dbg_printf ("\r\n");
				}
				else {
					dbg_printf ("\r\n");
					fdp = (const DynData_t **) (ddp->dp + fp->offset);
					xd_dump (indent + 2, *fdp);
				}
			}
			dump_indent (indent);
			break;
		}
		case DDS_UNION_TYPE: {
			const UnionType		*utp;
			const UnionMember	*mp;
			const DynDataMember_t	*fp;
			const DynData_t		**fdp;
			unsigned		i;

			dbg_printf ("Union: ");
			utp = (const UnionType *) ddp->type;
			dbg_printf ("\r\n");
			for (i = 0, fp = ddp->fields; i < ddp->nfields; i++, fp++) {
				if ((fp->flags & DMF_PRESENT) == 0)
					break;

				mp = &utp->member [fp->index];
				dump_indent (indent + 1);
				dbg_printf ("%s:\t", str_ptr (mp->member.name));
				if ((fp->flags & DMF_DYNAMIC) == 0) {
					dump_xdata (ddp->dp + fp->offset, fp->length);
					dbg_printf ("\r\n");
				}
				else {
					dbg_printf ("\r\n");
					fdp = (const DynData_t **) (ddp->dp + fp->offset);
					xd_dump (indent + 2, *fdp);
				}
			}
			dump_indent (indent);
			break;
		}
		default:
			break;
	}
	dbg_printf ("}\r\n");
}

#endif

