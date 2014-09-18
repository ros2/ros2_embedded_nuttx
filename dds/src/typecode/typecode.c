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

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "ctrace.h"
#include "prof.h"
#include "log.h"
#include "debug.h"
#include "pool.h"
#include "sys.h"
#include "dds.h"
#include "dds/dds_dcps.h"
#include "dds.h"
#include "cdr.h"
#ifndef CDR_ONLY
#include "pl_cdr.h"
#include "md5.h"
#include "typecode.h"
#endif

#ifdef PROFILE
PROF_PID (tc_dtype_reg)
PROF_PID (tc_dtype_free)
PROF_PID (tc_sample_free)
PROF_PID (tc_k_s_nat)
PROF_PID (tc_k_g_nat)
PROF_PID (tc_k_p_nat)
PROF_PID (tc_k_s_marsh)
PROF_PID (tc_k_g_marsh)
PROF_PID (tc_h_key)
#endif
#ifndef CDR_ONLY
static lock_t type_lock;

#define	META_LIST	/* Keep a list of registered types to prevent duplicate
			   type definitions and to allow type cleanup on exit. */
#endif
/*#define TRACE_MLIST	** Define this to trace type registrations. */

#ifdef META_LIST
static Skiplist_t meta_types;
#endif


const unsigned typecode_len [CDR_TYPECODE_MAX + 1] = {
	0, /* CDR_TYPECODE_UNKNOWN */
	2, /* CDR_TYPECODE_SHORT */
	2, /* CDR_TYPECODE_USHORT */
	4, /* CDR_TYPECODE_LONG */
	4, /* CDR_TYPECODE_ULONG */
	8, /* CDR_TYPECODE_LONGLONG */
	8, /* CDR_TYPECODE_ULONGLONG */
	4, /* CDR_TYPECODE_FLOAT */
	8, /* CDR_TYPECODE_DOUBLE */
#ifdef LONGDOUBLE
	16,/* CDR_TYPECODE_LONGDOUBLE */
#endif
	0, /* CDR_TYPECODE_FIXED */
	1, /* CDR_TYPECODE_BOOLEAN */
	1, /* CDR_TYPECODE_CHAR */
	0, /* CDR_TYPECODE_WCHAR */
	1, /* CDR_TYPECODE_OCTET */
	0, /* CDR_TYPECODE_CSTRING */
	0, /* CDR_TYPECODE_WSTRING */
	0, /* CDR_TYPECODE_STRUCT */
	0, /* CDR_TYPECODE_UNION */
	4, /* CDR_TYPECODE_ENUM */
	0, /* CDR_TYPECODE_SEQUENCE */
	0, /* CDR_TYPECODE_ARRAY */
	0, /* CDR_TYPECODE_TYPEREF */
	0, /* CDR_TYPECODE_MAX */
};


/* Dynamic type construction. */
/* -------------------------- */

/* CDR_DynamicType_alloc_struct_union -- Create a structure or union type. */

static CDR_TypeSupport *CDR_DynamicType_alloc_struct_union (CDR_TypeCode_t   tc,
                                                            const char       *name,
					                    unsigned         numelems,
						            unsigned         length,
						            int              key,
						            DDS_ReturnCode_t *retp)
{
	CDR_TypeSupport_struct *new;

	if (!numelems) {
		*retp = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	new = (CDR_TypeSupport_struct *) xmalloc (sizeof (CDR_TypeSupport_struct) +
	               (numelems - 1) * sizeof (CDR_TypeSupport_structelem));
	if (!new) {
		*retp = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	new->container.ts.typecode = tc;
	new->container.ts.key = key;
	new->container.ts.refcnt = 0;
	new->container.ts.marker = 0;
	new->container.ts.length = length;
	new->container.ts.name = name;
	new->container.ts.ts = NULL;
	new->container.numelems = numelems;
	memset (new->elements, 0, numelems * sizeof (CDR_TypeSupport_structelem));
	*retp = DDS_RETCODE_OK;
	return ((CDR_TypeSupport *) new);
}

/* CDR_DynamicType_alloc_enum -- Create an enumeration type. */

static CDR_TypeSupport *CDR_DynamicType_alloc_enum (const char       *name,
					            unsigned         numelems,
						    int              key,
						    DDS_ReturnCode_t *retp)
{
	CDR_TypeSupport_struct *new;

	if (!numelems) {
		*retp = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	new = (CDR_TypeSupport_struct *) xmalloc (sizeof (CDR_TypeSupport_struct) +
	               (numelems - 1) * sizeof (CDR_TypeSupport_structelem));
	if (!new) {
		*retp = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	new->container.ts.typecode = CDR_TYPECODE_ENUM;
	new->container.ts.key = key;
	new->container.ts.refcnt = 0;
	new->container.ts.marker = 0;
	new->container.ts.length = sizeof (int32_t);
	new->container.ts.name = name;
	new->container.ts.ts = NULL;
	new->container.numelems = numelems;
	memset (new->elements, 0, numelems * sizeof (CDR_TypeSupport_structelem));
	*retp = DDS_RETCODE_OK;
	return ((CDR_TypeSupport *) new);
}

/* CDR_DynamicType_alloc_array -- Create an Array or Sequence type. */

static CDR_TypeSupport *CDR_DynamicType_alloc_array (CDR_TypeCode_t   containertc,
						     const char       *name,
						     unsigned         numelems,
						     CDR_TypeCode_t   tc,
						     void             *data,
						     int              key,
						     int              *dyn_keys,
						     int              *fksize,
						     DDS_ReturnCode_t *retp)
{
	CDR_TypeSupport_array	*new;
	unsigned		el_length, el_key_len = 0;
	CDR_TypeSupport		*ts_elem = (CDR_TypeSupport *) data;

	if ((!numelems && containertc == CDR_TYPECODE_ARRAY) ||
	    (!typecode_len [tc] && !ts_elem)) {
		*retp = DDS_RETCODE_BAD_PARAMETER;
		return (NULL);
	}
	if (tc == CDR_TYPECODE_CSTRING) {

		/* The size of the the cstring in the array is either the string
		   data in case of a bounded string or the size of a char pointer
		   in case of unbounded strings. */
		el_length = *((size_t *) data);
		if (!el_length) {
			el_length = sizeof (char *);
			if (key)
				*dyn_keys = 1;
		}
		if (key) {
			el_key_len += (sizeof (uint32_t) + el_length + 3) & ~3;
			*fksize = 0;
		}
	}
	else {
		if (ts_elem)
			el_length = ts_elem->length;
		else
			el_length = typecode_len [tc];
		if (key)
			el_key_len = el_length;
	}
	new = xmalloc (sizeof (CDR_TypeSupport_array));
	if (!new) {
		*retp = DDS_RETCODE_OUT_OF_RESOURCES;
		return (NULL);
	}
	new->container.ts.typecode = containertc;
	new->container.ts.key = key;
	new->container.ts.refcnt = 0;
	new->container.ts.marker = 0;
	if (containertc == CDR_TYPECODE_ARRAY)
		new->container.ts.length = el_length * numelems;
	else {
		new->container.ts.length = sizeof (DDS_VoidSeq);
		if (key) {
			*dyn_keys = 1;
			*fksize = 0;
		}
	}
	new->container.ts.name = name;
	new->container.ts.ts = NULL;
	new->container.numelems = numelems;
	new->el_ts.typecode = tc;
	new->el_ts.name = NULL;

	if (!ts_elem)
		new->el_ts.length = el_length;
	else if (tc == CDR_TYPECODE_CSTRING) {

		/* To indicate if the string is bounded or unbounded, set the size
		   mentioned in the metadata support struct in the support struct.
		   This is 0 for an unbounded string. */
		new->el_ts.ts = NULL;
		new->el_ts.length = *((size_t *) data);
	}
	else {
		new->el_ts.ts = ts_elem;
		new->el_ts.ts->refcnt++;
	}
	new->el_ts.key = key;
	new->el_ts.refcnt = 0;
	new->el_ts.marker = 0;
	*retp = DDS_RETCODE_OK;
	return ((CDR_TypeSupport *) new);
}

/* CDR_DynamicType_set_field -- Assign the type for a structure field. */

static DDS_ReturnCode_t CDR_DynamicType_set_field (const char      *name,
						   CDR_TypeSupport *ts,
						   unsigned        ix,
						   unsigned        offset,
						   CDR_TypeCode_t  tc,
						   void            *data,
						   int             key,
						   int 		   label)
{
	size_t			elen;
	CDR_TypeSupport_struct	*ts_struct = (CDR_TypeSupport_struct *) ts;
	CDR_TypeSupport_struct	*ts_bis;

	if (!ts ||
	    tc >= CDR_TYPECODE_MAX ||
	    offset >= ts->length ||
	    (ts->typecode != CDR_TYPECODE_STRUCT && ts->typecode != CDR_TYPECODE_UNION) ||
	    ix >= ts_struct->container.numelems)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (tc) {
		case CDR_TYPECODE_CSTRING:
			/* In case of an unbounded string: 0. */
			elen = (data) ? *((size_t *) data) : 0;
			break;

		case CDR_TYPECODE_STRUCT:
		case CDR_TYPECODE_ARRAY:
		case CDR_TYPECODE_SEQUENCE:
		case CDR_TYPECODE_UNION:
		case CDR_TYPECODE_ENUM:
			if (!data)
				return (DDS_RETCODE_BAD_PARAMETER);

			ts_bis = (CDR_TypeSupport_struct *) data;
			elen = ts_bis->container.ts.length;
			break;

		default:
			elen = typecode_len [tc];
			if (!elen)
				return (DDS_RETCODE_BAD_PARAMETER);
			break;
	}
	if (elen + offset > ts->length)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (tc == CDR_TYPECODE_STRUCT || tc == CDR_TYPECODE_SEQUENCE ||
	    tc == CDR_TYPECODE_ARRAY || tc == CDR_TYPECODE_UNION ||
	    tc == CDR_TYPECODE_ENUM) {
		ts_struct->elements [ix].ts.ts = data;
		((CDR_TypeSupport_struct *) data)->container.ts.refcnt++;
                if (tc == CDR_TYPECODE_ENUM)
                        ts_struct->elements [ix].ts.length = 4;
	}
	else
		ts_struct->elements [ix].ts.length = elen;
	ts_struct->elements [ix].ts.typecode = tc;
	ts_struct->elements [ix].ts.key = key;
	ts_struct->elements [ix].ts.name = name;
	ts_struct->elements [ix].offset = offset;
	ts_struct->elements [ix].label = label;
	return (DDS_RETCODE_OK);
}

static void CDR_DynamicType_free (CDR_TypeSupport *ts);

static CDR_TypeSupport *CDR_DynamicType_register_enum (const DDS_TypeSupport_meta **tsm)
{
	CDR_TypeSupport		*ts = NULL;
	CDR_TypeSupport_struct	*ts_struct;
	unsigned		i, nelem = (*tsm)->nelem;
	unsigned		key = (*tsm)->flags & TSMFLAG_KEY;
	const char		*name = (*tsm)->name;
	DDS_ReturnCode_t	dds_rc = DDS_RETCODE_ERROR;

	ts = CDR_DynamicType_alloc_enum (name, nelem, key, &dds_rc);
	if (dds_rc != DDS_RETCODE_OK)
		return (NULL);

	ts_struct = (CDR_TypeSupport_struct *) ts;
	for (i = 0; i < nelem; i++) {
		(*tsm)++;
		ts_struct->elements [i].ts.typecode = (*tsm)->tc;
		ts_struct->elements [i].ts.key = 0;
		ts_struct->elements [i].ts.name = (*tsm)->name;
		ts_struct->elements [i].offset = 0;
		ts_struct->elements [i].label = (*tsm)->label;
	}
	return (ts);
}

static CDR_TypeSupport *CDR_DynamicType_register_struct (const DDS_TypeSupport_meta **tsm,
							 int *dynamic,
						         int *dyn_keys,
							 int *fksize,
							 int parent_key,
							 int forced_key);

static CDR_TypeSupport *CDR_DynamicType_register_array (const DDS_TypeSupport_meta **tsm,
							int *dynamic,
						        int *dyn_keys,
							int *fksize,
							int parent_key,
							int forced_key)
{
	DDS_ReturnCode_t		dds_rc = DDS_RETCODE_ERROR;
	CDR_TypeCode_t			tc = (*tsm)->tc,
					tc_elem_ori = ((*tsm) + 1)->tc,
					tc_elem;
	const DDS_TypeSupport_meta	*tsm_save = (*tsm);
	int				key = ((tsm_save->flags & TSMFLAG_KEY) || forced_key) && parent_key;
	CDR_TypeSupport			*ts;
	void 				*ts2 = NULL;
	int 				mkey;
	int 				mforced_key = forced_key;

	if (tc_elem_ori == CDR_TYPECODE_TYPEREF) {
		mkey = key && (((*tsm)->flags & TSMFLAG_KEY) || forced_key);
		(*tsm) = ((*tsm)+1)->tsm;
		if ((*tsm)->tc == CDR_TYPECODE_TYPEREF)
			return (NULL);
		if (mkey && (!((*tsm)->flags & TSMFLAG_KEY)))
			mforced_key = 1;
	}
	else
		(*tsm)++;
	if (tc == CDR_TYPECODE_SEQUENCE) {
		*dynamic = 1;
		if (key) {
			*dyn_keys = 1;
			*fksize = 0;
		}
	}
	tc_elem = (*tsm)->tc;
	if (tc_elem == CDR_TYPECODE_ARRAY || tc_elem == CDR_TYPECODE_SEQUENCE) {
		ts2 = CDR_DynamicType_register_array(tsm, dynamic, dyn_keys, fksize, key, mforced_key);
		if (!ts2)
			return (NULL);
	}
	else if (tc_elem == CDR_TYPECODE_STRUCT || tc_elem == CDR_TYPECODE_UNION) {
		ts2 = CDR_DynamicType_register_struct (tsm, dynamic, dyn_keys,
						fksize, key, mforced_key);
		if (!ts2)
			return (NULL);
	}
	else if (tc_elem == CDR_TYPECODE_CSTRING) {
		ts2 = (void *)&(*tsm)->size;

		/* If the size is 0 we're handling an unbounded
		   string => set dynamic. */
		if (*(size_t *) ts2 == 0) {
			*dynamic = 1;
			if (key) {
				*dyn_keys = 1;
				*fksize = 0;
			}
		}
		else if (key)
			*fksize = 0;
	}
	ts = CDR_DynamicType_alloc_array (tsm_save->tc,
					  tsm_save->name,
					  tsm_save->nelem,
					  tc_elem, ts2, key,
					  dyn_keys,
					  fksize,
					  &dds_rc);
	if (dds_rc != DDS_RETCODE_OK) {
		if (tc_elem == CDR_TYPECODE_STRUCT || tc_elem == CDR_TYPECODE_UNION ||
		    tc_elem == CDR_TYPECODE_ARRAY || tc_elem == CDR_TYPECODE_SEQUENCE)
			CDR_DynamicType_free (ts2);
		return (NULL);
	}
	if (tc_elem_ori == CDR_TYPECODE_TYPEREF)
		*tsm = tsm_save + 1;
	return (ts);
}

static CDR_TypeSupport *CDR_DynamicType_register_struct (const DDS_TypeSupport_meta **tsm,
							 int *dynamic,
						         int *dyn_keys,
							 int *fksize,
							 int parent_key,
							 int forced_key)
{
	DDS_ReturnCode_t	dds_rc = DDS_RETCODE_ERROR;
	CDR_TypeSupport		*ts = NULL;
	unsigned		i, nelem = (*tsm)->nelem;
	unsigned		key = (((*tsm)->flags & TSMFLAG_KEY) || forced_key) && parent_key;
	const char		*name = (*tsm)->name;


	ts = CDR_DynamicType_alloc_struct_union ((*tsm)->tc, name, nelem,
						 (*tsm)->size, key, &dds_rc);
	if (dds_rc != DDS_RETCODE_OK)
		return (NULL);

	if (forced_key && ((*tsm)->flags & TSMFLAG_KEY))
		forced_key = 0;

	for (i = 0; i < nelem; i++) {
		CDR_TypeCode_t			tc, tc_ori;
		unsigned			offset;
		int				label;
		const DDS_TypeSupport_meta	*tsm_ori;
		int 				mkey, mforced_key;

		(*tsm)++;
		tc = (*tsm)->tc;
		offset = (*tsm)->offset;
		name = (*tsm)->name;
		mkey = key && (((*tsm)->flags & TSMFLAG_KEY) || forced_key);
		mforced_key = forced_key;
		label = (*tsm)->label;
		tsm_ori = *tsm;
		tc_ori = tc;
		if (tc_ori == CDR_TYPECODE_TYPEREF) {
			if ((*tsm)->tsm->tc == CDR_TYPECODE_TYPEREF)
				goto free_ts;

			*tsm = (*tsm)->tsm;
			tc = (*tsm)->tc;
			if (mkey && (!((*tsm)->flags & TSMFLAG_KEY)))
				mforced_key = 1;
		}

		if (tc == CDR_TYPECODE_STRUCT || tc == CDR_TYPECODE_UNION) {
			CDR_TypeSupport *ts2;

			ts2 = CDR_DynamicType_register_struct (tsm, dynamic,
							       dyn_keys, fksize, mkey, mforced_key);
			if (!ts2)
				goto free_ts;

			dds_rc = CDR_DynamicType_set_field (name, ts, i, offset,
			                                    tc, ts2, mkey, label);
			if (dds_rc != DDS_RETCODE_OK)
				CDR_DynamicType_free (ts2);
		}
		else if (tc == CDR_TYPECODE_ARRAY || tc == CDR_TYPECODE_SEQUENCE) {
			CDR_TypeSupport	*ts2;

			ts2 = CDR_DynamicType_register_array (tsm, 
							      dynamic, dyn_keys, fksize, mkey, mforced_key);
			if (!ts2)
				goto free_ts;

			CDR_DynamicType_set_field (name, ts, i, offset,
						   tc, ts2, mkey, label);
		}
		else if (tc == CDR_TYPECODE_CSTRING) {

			/* If this is an unbounded string the type support size
			   will be set to 0, and the dynamic flag will be set.*/
			if (!(*tsm)->size) {
				*dynamic = 1;
				if (mkey) {
					*dyn_keys = 1;
					*fksize = 0;
				}
			}
			else if (mkey)
				*fksize = 0;

			dds_rc = CDR_DynamicType_set_field (name, ts, i, offset,
							    CDR_TYPECODE_CSTRING,
							    (void *)&((*tsm)->size),
							    mkey, label);
		}
		else if (tc == CDR_TYPECODE_ENUM) {
			CDR_TypeSupport *ts2;

			ts2 = CDR_DynamicType_register_enum (tsm);
			if (!ts2)
				goto free_ts;

			dds_rc = CDR_DynamicType_set_field (name, ts, i, offset,
			                                    tc, ts2, mkey, label);
			if (dds_rc != DDS_RETCODE_OK)
				CDR_DynamicType_free (ts2);
		}
		else
			dds_rc = CDR_DynamicType_set_field (name, ts, i, offset,
							    tc, NULL, mkey, label);
		if (dds_rc != DDS_RETCODE_OK)
			goto free_ts;

		if (tc_ori == CDR_TYPECODE_TYPEREF)
			*tsm = tsm_ori;
	}
	return (ts);

    free_ts:
	CDR_DynamicType_free (ts);
	return (NULL);
}

#ifdef TRACE_MLIST
#define	ml_print(s)	log_printf(DDS_ID, 0, s)
#define	ml_print1(s,a1)	log_printf(DDS_ID, 0, s,a1)
#else
#define	ml_print(s)
#define	ml_print1(s,a1)
#endif

#ifdef META_LIST
#ifdef TRACE_MLIST

static int dump_meta_type (Skiplist_t *lp, void *np, void *arg)
{
	const DDS_TypeSupport *ts, **tsp = (const DDS_TypeSupport **) np;

	ARG_NOT_USED (lp)
	ARG_NOT_USED (arg)

	ts = *tsp;
	ml_print1 ("%s ", ts->ts_name);
	return (1);
}
#endif

static int meta_cmp (const void *np, const void *data)
{
	const DDS_TypeSupport	   *ts, **tsp = (const DDS_TypeSupport **) np;
	const DDS_TypeSupport_meta *meta_ts = (const DDS_TypeSupport_meta *) data;

	ts = *tsp;
	return ((long)(uintptr_t) meta_ts - (long)(uintptr_t) ts->ts_meta);
}

#endif

/* DDS_DynamicType_register -- API function to initialize the run-time
			       type support based on the meta type support
			       array. */

DDS_TypeSupport *DDS_DynamicType_register (const DDS_TypeSupport_meta *tc)
{
	CDR_TypeSupport *ts;
	DDS_TypeSupport *dds_ts;
#ifdef META_LIST
	DDS_TypeSupport **np;
	int		is_new;
#endif
	const DDS_TypeSupport_meta *mp = tc;
	size_t		size;
	int		dynamic = 0, dyn_keys = 0, fksize = 1;
#ifndef CDR_ONLY
	unsigned	count_1, count_2;
	unsigned long	bytes_1, bytes_2;
#endif
	DDS_ReturnCode_t error;

	dds_pre_init ();

	prof_start (tc_dtype_reg);
#ifndef CDR_ONLY
	lock_take (type_lock);
#endif
	ctrc_printd (DCPS_ID, 0, (tc && tc->name) ? tc->name : NULL,
                                 (tc && tc->name) ? strlen (tc->name) : 0);

	if (!tc || tc->tc != CDR_TYPECODE_STRUCT) {
#ifndef CDR_ONLY
		lock_release (type_lock);
#endif
		return (NULL);
	}

#ifdef META_LIST
	ml_print1 ("DDS_DynamicType_register (%s);\r\n", mp->name);
	np = sl_insert (&meta_types, tc, &is_new, meta_cmp);
	if (!np) {
#ifndef CDR_ONLY
		lock_release (type_lock);
#endif
		return (NULL);	/* No memory! */ 
	}

	if (!is_new) {
		ml_print (" => Exists!\r\n");
		dds_ts = *np;
		dds_ts->ts_users++;
#ifndef CDR_ONLY
		lock_release (type_lock);
#endif
		prof_stop (tc_dtype_reg, 1);
		return (dds_ts);
	}
#endif
#ifndef CDR_ONLY
	pool_get_malloc_count (&count_1, &bytes_1);
#endif
	size = tc->size;
	ts = CDR_DynamicType_register_struct (&tc, &dynamic, &dyn_keys,
						     &fksize, 1, 0);
	if (!ts) {
		DDS_TypeSupport rm;
		rm.ts_meta = tc;
		*np=&rm;
#ifdef META_LIST
		sl_delete (&meta_types, tc, meta_cmp);
#endif
#ifndef CDR_ONLY
		lock_release (type_lock);
#endif
		return (NULL); 
	}

	ts->refcnt++;
	dds_ts = xmalloc (sizeof (DDS_TypeSupport));
	if (!dds_ts) {
		CDR_DynamicType_free (ts);
#ifndef CDR_ONLY
		lock_release (type_lock);
#endif
		return (NULL);
	}
	memset (dds_ts, 0, sizeof (DDS_TypeSupport));
	dds_ts->ts_name = mp->name;
	dds_ts->ts_prefer = MODE_CDR;
	dds_ts->ts_keys = mp->flags & TSMFLAG_KEY;
	dds_ts->ts_dynamic = dynamic;
	dds_ts->ts_length = size;
	dds_ts->ts_cdr = ts;
	dds_ts->ts_fksize = fksize;
	if (dyn_keys)
		dds_ts->ts_mkeysize = 0;
	else
		dds_ts->ts_mkeysize = cdr_marshalled_size (4, NULL, ts, 1, 1, &error);
	dds_ts->ts_meta = mp;
	dds_ts->ts_users = 1;
#ifndef CDR_ONLY
	pool_get_malloc_count (&count_2, &bytes_2);
	log_printf (DDS_ID, 0, "DDS: DynamicType_register(%s): %u blocks, %lu bytes\r\n",
				mp->name,
				count_2 - count_1,
				bytes_2 - bytes_1);
#endif
#ifdef META_LIST
	*np = dds_ts;
#ifdef TRACE_MLIST
	ml_print ("After sl_insert(): ");
	sl_walk (&meta_types, dump_meta_type, NULL);
	ml_print ("\r\n");
#endif
#endif
#ifndef CDR_ONLY
	lock_release (type_lock);
#endif
	prof_stop (tc_dtype_reg, 1);
	return (dds_ts);
}

/* CDR_DynamicType_free -- Free the resources associated with a type. */

static void CDR_DynamicType_free (CDR_TypeSupport *ts)
{
	ctrc_printd (DCPS_ID, 1, ts, sizeof (ts));

	if (ts->refcnt > 1) {
		ts->refcnt--;
		prof_stop (tc_dtype_free, 1);
		return;
	}
	if (cdr_is_struct (ts->typecode)) {
		CDR_TypeSupport_struct *ts_s = (CDR_TypeSupport_struct *) ts;
		CDR_TypeSupport_structelem *ep;
		unsigned i;

		for (i = 0, ep = ts_s->elements;
		     i < ts_s->container.numelems;
		     i++, ep++)
			if (cdr_is_container (ep->ts.typecode))
				CDR_DynamicType_free (ep->ts.ts);
	}
	else if (cdr_is_array (ts->typecode)) {
		CDR_TypeSupport_array *ts_a = (CDR_TypeSupport_array *) ts;

		if (cdr_is_container (ts_a->el_ts.typecode))
			CDR_DynamicType_free (ts_a->el_ts.ts);
	}
	xfree (ts);
}

#ifndef CDR_ONLY
/* DDS__SampleFree -- Free the dynamically allocated memory used up by a sample.
                      Only use this if the allocated memory isn't consecutive. */

static void DDS__SampleFree (void *sample, const CDR_TypeSupport *ts,
			     void (*free)(void *p))
{
	unsigned i;
	void	 *cp;

	if (ts->typecode == CDR_TYPECODE_STRUCT || ts->typecode == CDR_TYPECODE_UNION) {
		CDR_TypeSupport_struct *ts_s = (CDR_TypeSupport_struct *) ts;
		CDR_TypeSupport_structelem *ep;

		for (i = 0, ep = ts_s->elements;
		     i < ts_s->container.numelems;
		     i++, ep++)
			if (cdr_is_container (ep->ts.typecode))
				DDS__SampleFree ((char *)sample + ep->offset,
						 ep->ts.ts, free);
	}
	else if (ts->typecode == CDR_TYPECODE_ARRAY) {
		CDR_TypeSupport_array *ts_a = (CDR_TypeSupport_array *) ts;

		if (!cdr_is_container (ts_a->el_ts.typecode))
			return;

		for (i = 0; i < ts_a->container.numelems; i++)
			DDS__SampleFree ((char *) sample + i * ts_a->el_ts.ts->length,
					 ts_a->el_ts.ts, free);
	}
	else if (ts->typecode == CDR_TYPECODE_SEQUENCE) {
		DDS_VoidSeq *voidseq = (DDS_VoidSeq *)sample;
		CDR_TypeSupport_array *ts_a = (CDR_TypeSupport_array *) ts;

		if (!cdr_is_container (ts_a->el_ts.typecode))
			goto free_sequence;

		DDS_SEQ_FOREACH_ENTRY (*voidseq, i, cp)
			DDS__SampleFree (cp, ts_a->el_ts.ts, free);

	    free_sequence:
		free (voidseq->_buffer);
	}
}

/* DDS_SampleFree -- Free the memory used up by a sample, using the type support
                     metadata. Only use this if the allocated memory isn't
                     consecutive. */

void DDS_SampleFree (void *sample, const DDS_TypeSupport *ts, int full, void (*free)(void *p))
{
	ctrc_printd (DCPS_ID, 2, sample, sizeof (sample));
	prof_start (tc_sample_free);

	if (ts->ts_dynamic)
		DDS__SampleFree (sample, ts->ts_cdr, free);
	if (full)
		free (sample);

	prof_stop (tc_sample_free, 1);
}
#endif

/* DDS_DynamicType_free -- Free the resources associated with a type. */

void DDS_DynamicType_free (DDS_TypeSupport *ts)
{
	TS_MODE		mode;
#ifdef META_LIST
	DDS_TypeSupport **np;
#endif

	prof_start (tc_dtype_free);

	if (!ts)
		return;	/* Bad parameter! */

#ifndef CDR_ONLY
	lock_take (type_lock);
#endif

#ifdef META_LIST
	ml_print1 ("DDS_DynamicType_free (%s);\r\n", ts->ts_name);
	np = sl_search (&meta_types, ts->ts_meta, meta_cmp);
	if (!np || *np != ts) {
		ml_print (" ==> Not found!\r\n");
		lock_release (type_lock);
		return;	/* Not found! */
	}
#endif
	if (--ts->ts_users) {
		ml_print (" ==> still users!\r\n");
#ifndef CDR_ONLY
		lock_release (type_lock);
#endif
		return;	/* Done: usecount decremented. */
	}

#ifdef META_LIST
	sl_delete (&meta_types, ts->ts_meta, meta_cmp);
#ifdef TRACE_MLIST
	ml_print ("After sl_delete(): ");
	sl_walk (&meta_types, dump_meta_type, NULL);
	ml_print ("\r\n");
#endif
#endif
	for (mode = MODE_CDR; mode <= MODE_XML; mode++)
		if (ts->ts [mode].cdr)
			switch (mode) {
				case MODE_CDR:
					CDR_DynamicType_free (ts->ts_cdr);
					break;

				default:
			  		break;
			}
	xfree (ts);
#ifndef CDR_ONLY
	lock_release (type_lock);
#endif
	prof_stop (tc_dtype_free, 1);
}


#ifdef DDS_DEBUG

/* DDS_DynamicType_dump_type -- Dump typecode data for visual analysis. */

void DDS_TypeSupport_dump_type (unsigned              indent, 
				const DDS_TypeSupport *ts,
				unsigned              flags)
{
	static const char *mode_str [] = {
		"CDR", "PL_CDR", "XML", "RAW"
	};

	if (!ts)
		return;

	if ((flags & XDF_TS_HEADER) != 0) {
		dbg_print_indent (indent, NULL);
		dbg_printf ("Preferred: %s, dynamic: %u, length: %u, keys: %u",
			mode_str [ts->ts_prefer],
			ts->ts_dynamic, ts->ts_length, ts->ts_keys);
		if (ts->ts_keys)
			dbg_printf (", mkeysize: %u", ts->ts_mkeysize);
		dbg_printf ("\r\n");
	}
	switch (ts->ts_prefer) {
		case MODE_CDR:
			cdr_dump_type (indent, (CDR_TypeSupport *) ts->ts_cdr);
			break;
		default:
			printf ("unknown mode: %u\r\n", ts->ts_prefer);
			break;
	}
}

/* DDS_DynamicType_dump_data -- Dump a data sample. */

void DDS_TypeSupport_dump_data (unsigned              indent,
				const DDS_TypeSupport *ts,
				const void            *data,
				int                   native,
				int                   dynamic,
				int                   field_names)
{
	ARG_NOT_USED (dynamic)
	ARG_NOT_USED (field_names)

	if (!ts || native)
		return;

	switch (ts->ts_prefer) {
		case MODE_CDR:
			cdr_dump_data (indent, (CDR_TypeSupport *) ts->ts_cdr, data);
			break;
		default:
			printf ("unknown mode: %u\r\n", ts->ts_prefer);
			break;
	}
}

#endif

#ifdef META_LIST
	
/* dds_cleanup_type -- Cleanup a registered type. */

int dds_cleanup_type (Skiplist_t *list, void *node, void *args)
{
	DDS_TypeSupport	   *ts, **tsp = (DDS_TypeSupport **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (args)

	ts = *tsp;
	/*log_printf (DDS_ID, 0, " %s", ts->ts_name);*/
	DDS_DynamicType_free (ts);
	return (1);
}
#endif

/* dds_typesupport_final -- Cleanup Typesupport code. */

void dds_typesupport_final (void)
{
#ifdef META_LIST
	while (meta_types.length) {
		/*log_printf (DDS_ID, 0, "Cleanup registered types:");*/
		sl_walk (&meta_types, dds_cleanup_type, NULL);
		/*log_printf (DDS_ID, 0, "\r\n");*/
	}
#endif
#ifndef CDR_ONLY
	lock_destroy (type_lock);
#endif
}

/* dds_typesupport_init -- Initialize Typesupport code. */

int dds_typesupport_init (void)
{
#ifndef CDR_ONLY
	lock_init_nr (type_lock, "type");
#endif
#ifdef META_LIST
	sl_init (&meta_types, sizeof (DDS_TypeSupport *));
#endif
	cdr_init ();
	pl_init ();

	PROF_INIT ("T:DTypeReg", tc_dtype_reg);
	PROF_INIT ("T:DTypFree", tc_dtype_free);
	PROF_INIT ("T:SampFree", tc_sample_free);
	PROF_INIT ("T:KSizeNat", tc_k_s_nat);
	PROF_INIT ("T:KGetNat", tc_k_g_nat);
	PROF_INIT ("T:KToNat", tc_k_p_nat);
	PROF_INIT ("T:KSizeMar", tc_k_s_marsh);
	PROF_INIT ("T:KGetMar", tc_k_g_marsh);
	PROF_INIT ("T:HashKey", tc_h_key);

	return (DDS_RETCODE_OK);
}

/* Data marshalling and unmarshalling. */
/* ----------------------------------- */

/* DDS_MarshalledDataSize -- Return the buffer size for marshalled data. */

size_t DDS_MarshalledDataSize (const void            *sample,
			       int                   dynamic,
			       const DDS_TypeSupport *ts,
			       DDS_ReturnCode_t      *ret)
{
	size_t	length;

	ARG_NOT_USED (dynamic)

	if (ret)
		*ret = DDS_RETCODE_BAD_PARAMETER;
	if (!ts || !sample)
		return (0);

	switch (ts->ts_prefer) {
		case MODE_CDR:
			length = cdr_marshalled_size (4, sample, ts->ts_cdr, 0, 0, ret);
			if (!length)
				return (0);
			break;
#ifndef CDR_ONLY
		case MODE_PL_CDR:
			length = pl_marshalled_size (sample, ts->ts_pl, 0, ret);
			if (!length)
				return (0);
			break;
#endif
		case MODE_XML:
		default:
			return (0);
	}
	if (ret)
		*ret = DDS_RETCODE_OK;
	return (length + 4);
}

/* DDS_MarshallData -- Marshall payload data using the proper type support coding
		       for either local (in-device.*/

DDS_ReturnCode_t DDS_MarshallData (void                  *buffer,
			           const void            *data,
				   int                   dynamic,
			           const DDS_TypeSupport *ts)
{
	unsigned char	*dp = buffer;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (dynamic)

	if (!buffer || !data || !ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (ts->ts_prefer) {
		case MODE_CDR:
			if (!ts->ts_cdr)
				return (DDS_RETCODE_BAD_PARAMETER);

			ret = cdr_marshall (dp + 4, 4, data, ts->ts_cdr, 0, 0, 0);
			if (ret)
				return (ret);

			dp [0] = 0;
			dp [1] = (MODE_CDR << 1) | ENDIAN_CPU;
			break;
#ifndef CDR_ONLY
		case MODE_PL_CDR:
			if (!ts->ts_pl)
				return (DDS_RETCODE_BAD_PARAMETER);

			ret = pl_marshall (dp + 4, data, ts->ts_pl, 0, 0);
			dp [0] = 0;
			dp [1] = (MODE_PL_CDR << 1) | ENDIAN_CPU;
			break;
#endif
		case MODE_XML:
			return (DDS_RETCODE_UNSUPPORTED);

		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	dp [2] = dp [3] = 0;
	return (DDS_RETCODE_OK);
}

/* DDS_UnmarshalledDataSize -- Return the buffer size needed for marshalled data. */

size_t DDS_UnmarshalledDataSize (DBW                   data,
			         const DDS_TypeSupport *ts,
			         DDS_ReturnCode_t      *error)
{
	unsigned char	*dp;
	unsigned	type;

	if (!ts || data.length < 8) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (!ts->ts_dynamic)
		return (ts->ts_length);

	dp = DBW_PTR (data);
	type = dp [0] << 8 | dp [1];
	DBW_INC (data, 4);
	if ((type >> 1) == MODE_CDR) {
		int swap = (type & 1) ^ ENDIAN_CPU;

		if (ts->ts_cdr)
			return (cdr_unmarshalled_size (data.data, 4, ts->ts_cdr,
							       0, 0, swap, error));
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
#ifndef CDR_ONLY
	else if ((type >> 1) == MODE_PL_CDR) {
		int swap = (type & 1) ^ ENDIAN_CPU;

		if (ts->ts_pl)
			return (pl_unmarshalled_size (&data, ts->ts_pl, error,
									swap));
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
#endif
	else if (error)
		*error = DDS_RETCODE_UNSUPPORTED;

	return 0;
}

/* DDS_UnmarshallData -- Unmarshall payload data using the proper type support
		         package. */

DDS_ReturnCode_t DDS_UnmarshallData (void                  *buffer,
				     DBW                   *data,
				     const DDS_TypeSupport *ts)
{
	int		 swap;
	unsigned char	 *dp;
	unsigned	 type;
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	dp = data->data;
	type = dp [0] << 8 | dp [1];
	swap = (type & 1) ^ ENDIAN_CPU;
	DBW_INC (*data, 4);
	if (DBW_REMAIN (*data) <= 4)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((type >> 1) == MODE_CDR) {
		if (ts->ts_cdr)
			ret = cdr_unmarshall (buffer, DBW_PTR (*data), 4,
					      ts->ts_cdr, 0, 0, swap);
		else
			ret = DDS_RETCODE_UNSUPPORTED;
	}
#ifndef CDR_ONLY
	else if ((type >> 1) == MODE_PL_CDR) {
		if (ts->ts_pl)
			ret = pl_unmarshall (buffer, data, ts->ts_pl, swap);
		else
			ret = DDS_RETCODE_UNSUPPORTED;
	}
#endif
	else if ((type >> 1) == MODE_XML)
		ret = DDS_RETCODE_UNSUPPORTED;
	else
		ret = DDS_RETCODE_BAD_PARAMETER;
	return (ret);
}

/* DDS_KeySizeFromNativeData -- Returns the total size of the key fields from
				a native data sample. */

size_t DDS_KeySizeFromNativeData (const unsigned char   *data,
				  int                   dynamic,
				  const DDS_TypeSupport *ts,
				  DDS_ReturnCode_t      *error)
{
	size_t	l;

	ARG_NOT_USED (dynamic)

	prof_start (tc_k_s_nat);

	if (!ts || !ts->ts_keys) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (ts->ts_fksize && ts->ts_mkeysize) {
		if (error)
			*error = DDS_RETCODE_OK;

		prof_stop (tc_k_s_nat, 1);
		return (ts->ts_mkeysize);
	}
	switch (ts->ts_prefer) {
		case MODE_CDR:
			l = cdr_marshalled_size (4, data, ts->ts_cdr, 1, 0, error);
			prof_stop (tc_k_s_nat, 1);
			return (l);
#ifndef CDR_ONLY
		case MODE_PL_CDR:
			l = pl_marshalled_size (data, ts->ts_pl, 1, error);
			prof_stop (tc_k_s_nat, 1);
			return (l);
#endif
		case MODE_XML:
			if (error)
				*error = DDS_RETCODE_UNSUPPORTED;
			break;
		default:
			if (error)
				*error = DDS_RETCODE_BAD_PARAMETER;
			break;
	}
	return (0);
}

/* DDS_KeyFromNativeData -- Extract the key fields from a non-marshalled native
			    data sample (data). */

DDS_ReturnCode_t DDS_KeyFromNativeData (unsigned char         *key,
					const void            *data,
					int                   dynamic,
					const DDS_TypeSupport *ts)
{
	int			swap, msize;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (dynamic)

	prof_start (tc_k_g_nat);

	if (!ts || !ts->ts_keys || !data || !key)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (ts->ts_prefer) {
		case MODE_CDR:
			if (ts->ts_mkeysize && ts->ts_mkeysize <= 16) {
				swap = ENDIAN_CPU ^ ENDIAN_BIG;
				msize = 1;
			}
			else {
				swap = 0;
				msize = 0;
			}
			ret = cdr_marshall (key, 4, data, ts->ts_cdr, 1, msize, swap);
			prof_stop (tc_k_g_nat, 1);
			return (ret);
#ifndef CDR_ONLY
		case MODE_PL_CDR:
			ret = pl_marshall (key, data, ts->ts_pl, 1, 0);
			prof_stop (tc_k_g_nat, 1);
			return (ret);

#endif
		case MODE_XML:
		default:
			return (DDS_RETCODE_UNSUPPORTED);
	}
}

/* DDS_KeyToNativeData -- Copy the key fields to a native data sample. */

DDS_ReturnCode_t DDS_KeyToNativeData (void                  *data,
				      int                   dynamic,
				      const void            *key,
				      const DDS_TypeSupport *ts)
{
	DDS_ReturnCode_t ret;
	int		 swap, msize;

	ARG_NOT_USED (dynamic)

	prof_start (tc_k_p_nat);
	switch (ts->ts_prefer) {
		case MODE_CDR:
			if (ts->ts_mkeysize && ts->ts_mkeysize <= 16) {
				swap = ENDIAN_CPU ^ ENDIAN_BIG;
				msize = 1;
			}
			else {
				swap = 0;
				msize = 0;
			}
			ret = cdr_unmarshall (data, key, 4, ts->ts_cdr, 1,
								msize, swap);
			prof_stop (tc_k_p_nat, 1);
			break;
		default:
			ret = DDS_RETCODE_UNSUPPORTED;
			break;
	}
	return (ret);
}

/* DDS_KeySizeFromMarshalled -- Returns the total size of the key fields
				in a marshalled data sample. */

size_t DDS_KeySizeFromMarshalled (DBW                   data,
				  const DDS_TypeSupport *ts,
				  int                   key,
				  DDS_ReturnCode_t      *error)
{
	unsigned char	*dp;
	unsigned	type;
	int		swap;
	size_t		l;

	prof_start (tc_k_s_marsh);
	if (!ts || !ts->ts_keys) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (ts->ts_fksize && ts->ts_mkeysize) {
		if (error)
			*error = DDS_RETCODE_OK;
		prof_stop (tc_k_s_marsh, 1);
		return (ts->ts_mkeysize);
	}
	dp = DBW_PTR (data);
	type = dp [0] << 8 | dp [1];
	DBW_INC (data, 4);
	swap = (type & 1) ^ ENDIAN_CPU;
	if ((type >> 1) == MODE_CDR) {
		if (ts->ts_cdr) {
			l = cdr_key_size (data.data, 4, ts->ts_cdr, key, 0, swap, error);
			prof_stop (tc_k_s_marsh, 1);
			return (l);
		}
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
#ifndef CDR_ONLY
	else if ((type >> 1) == MODE_PL_CDR) {
		if (ts->ts_pl) {
			l = pl_key_size (data, ts->ts_pl, swap, error);
			prof_stop (tc_k_s_marsh, 1);
			return (l);
		}
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
#endif
	else if ((type >> 1) == MODE_RAW)
		if (ts->ts_cdr) {
			l = cdr_marshalled_size (4, data.data, ts->ts_cdr, 1, 0, error);
			prof_stop (tc_k_s_marsh, 1);
			return (l);
		}
		else
			*error = DDS_RETCODE_UNSUPPORTED;
	else if (error)
		*error = DDS_RETCODE_UNSUPPORTED;

	return (0);
}

/* DDS_KeyFromMarshalled -- Extract the key fields from marshalled key/data. */

DDS_ReturnCode_t DDS_KeyFromMarshalled (unsigned char         *dst,
					DBW                   data,
					const DDS_TypeSupport *ts,
					int                   key)
{
	unsigned char	*dp;
	unsigned	type;
	int		swap, iswap, msize;
	DDS_ReturnCode_t ret;

	prof_start (tc_k_g_marsh);
	if (!ts || !ts->ts_keys || data.length < 8)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = DBW_PTR (data);
	type = dp [0] << 8 | dp [1];
	DBW_INC (data, 4);
	iswap = (type & 1) ^ ENDIAN_CPU;
	if ((type >> 1) == MODE_CDR) {
		if (ts->ts_cdr) {
			if (ts->ts_mkeysize && ts->ts_mkeysize <= 16) {
				swap = (type & 1) ^ ENDIAN_BIG;
				ret = cdr_key_fields (dst, 4, data.data, 4, 
					       ts->ts_cdr, key, 1, swap, iswap);
			}
			else {
				swap = (type & 1) ^ ENDIAN_CPU;
				ret = cdr_key_fields (dst, 4, data.data, 4,
					       ts->ts_cdr, key, 0, swap, iswap);
			}
			prof_stop (tc_k_g_marsh, 1);
			return (ret);
		}
		else
			return (DDS_RETCODE_UNSUPPORTED);
	}
#ifndef CDR_ONLY
	else if ((type >> 1) == MODE_PL_CDR) {
		if (ts->ts_pl) {
			swap = (type & 1) ^ ENDIAN_CPU;
			ret = pl_key_fields (dst, &data, ts->ts_pl, swap);
			prof_stop (tc_k_g_marsh, 1);
			return (ret);
		}
		else
			return (DDS_RETCODE_UNSUPPORTED);
	}
#endif
	else if ((type >> 1) == MODE_RAW) {
		if (ts->ts_cdr) {
			if (ts->ts_mkeysize && ts->ts_mkeysize <= 16) {
				swap = ENDIAN_CPU ^ ENDIAN_BIG;
				msize = 1;
			}
			else {
				swap = 0;
				msize = 0;
			}
			ret = cdr_marshall (dst, 4, data.data, ts->ts_cdr, 1, msize, swap);
			prof_stop (tc_k_g_marsh, 1);
			return (ret);
		}
		else
			return (DDS_RETCODE_UNSUPPORTED);
	}
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

#ifndef CDR_ONLY

#define	KEY_HSIZE	4	/* Assume same alignment for the MD5 checksum
				   data as for transported key fields. */

/* DDS_HashFromKey -- Calculate the hash value from a key. */

DDS_ReturnCode_t DDS_HashFromKey (unsigned char         hash [16],
				  const unsigned char   *key,
				  size_t                key_size,
				  const DDS_TypeSupport *ts)
{
	MD5_CONTEXT		mdc;
	unsigned char		*dp;
	unsigned		i;
	int			free_key = 0, swap = ENDIAN_CPU ^ ENDIAN_BIG;
	size_t			n;
#ifdef MAX_KEY_BUFFER
	unsigned char		key_buffer [MAX_KEY_BUFFER];
#endif

	DDS_ReturnCode_t	ret;

	prof_start (tc_h_key);
	if (ts->ts_mkeysize && ts->ts_mkeysize <= 16) {
		memcpy (hash, key, ts->ts_mkeysize);
		for (i = ts->ts_mkeysize; i < 16; i++)
			hash [i] = 0;

		prof_stop (tc_h_key, 1);
		return (DDS_RETCODE_OK);
	}
	dp = NULL;
	if (!ts->ts_mkeysize || ts->ts_mkeysize == key_size) {
		if (!swap)
			dp = (unsigned char *) key;
		n = key_size;
	}
	else if (ts->ts_mkeysize)
		n = ts->ts_mkeysize;
	else {
		n = cdr_key_size (key, 4, ts->ts_cdr, 1, 1, swap, &ret);
		if (!n)
			return (ret);
	}
	if (!dp) {
#ifdef MAX_KEY_BUFFER
		if (n <= MAX_KEY_BUFFER)
			dp = key_buffer;
		else {
#endif
			dp = xmalloc (n);
			if (!dp)
				return (DDS_RETCODE_OUT_OF_RESOURCES);

			free_key = 1;
#ifdef MAX_KEY_BUFFER
		}
#endif
		ret = cdr_key_fields (dp, KEY_HSIZE, key, 4, ts->ts_cdr,
								1, 1, swap, 0);
		if (ret) {
#ifdef MAX_KEY_BUFFER
			if (n > MAX_KEY_BUFFER)
#endif
				xfree (dp);
			return (ret);
		}
	}
	md5_init (&mdc);
	md5_update (&mdc, dp, n);
	md5_final (hash, &mdc);

	if (free_key)
		xfree (dp);

	prof_stop (tc_h_key, 1);
	return (DDS_RETCODE_OK);
}

#endif

