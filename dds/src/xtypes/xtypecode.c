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
#include "dds/dds_dcps.h"
#include "xcdr.h"
#include "pl_cdr.h"
#include "xtypes.h"
#include "xtopic.h"
#include "builtin.h"
#include "tsm.h"
#include "md5.h"
#include "vtc.h"
#include "xdata.h"
#include "typecode.h"

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

#ifdef TRACE_MLIST
#define	ml_print(s)	log_printf(DDS_ID, 0, s)
#define	ml_print1(s,a1)	log_printf(DDS_ID, 0, s,a1)
#else
#define	ml_print(s)
#define	ml_print1(s,a1)
#endif

/*#define DUMP_BEFORE_AFTER	** Dump lib/domain info before/after create. */

#ifndef CDR_ONLY
static lock_t type_lock;

#define	META_LIST	/* Keep a list of registered types to prevent duplicate
			   type definitions and to allow type cleanup on exit. */
/*#define TRACE_MLIST	** Define this to trace type registrations. */

#else
#undef lock_init_nr
#undef lock_take
#undef lock_release
#undef lock_destroy
#define	lock_init_nr(l,name)
#define	lock_take(x)
#define	lock_release(x)
#define	lock_destroy(l)
#endif

#ifdef META_LIST
static Skiplist_t meta_types;
static Skiplist_t dyn_types;
#endif

#ifdef META_LIST
#ifdef TRACE_MLIST

static int dump_meta_type (Skiplist_t *lp, void *np, void *arg)
{
	const TypeSupport_t *ts, **tsp = (const TypeSupport_t **) np;

	ARG_NOT_USED (lp)
	ARG_NOT_USED (arg)

	ts = *tsp;
	ml_print1 ("%s ", ts->ts_name);
	return (1);
}
#endif

static int str_cmp (const void *np, const void *data)
{
	const TypeSupport_t *ts, **tsp = (const TypeSupport_t **) np;
	const char *name = (const char *) data;

	ts = *tsp;
	if (ts->ts_name)
		return (strcmp (name, ts->ts_name));
	else
		return (-1);
}

static int dyn_cmp (const void *np, const void *data)
{
	const TypeSupport_t	*ts, **tsp = (const TypeSupport_t **) np;
	const Type		*tp1, *tp2 = (const Type *) data;

	ts = *tsp;
	if (ts->ts_prefer == MODE_CDR)
		tp1 = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR)
		tp1 = ts->ts_pl->xtype;
	else
		return (0);

	return ((long)(uintptr_t) tp1 - (long)(uintptr_t) tp2);
}

#endif

/* DDS_DynamicType_register -- API function to initialize the run-time type
			       support based on the meta type support array. */

DDS_TypeSupport DDS_DynamicType_register (const DDS_TypeSupport_meta *tc)
{
	DDS_TypeSupport dds_ts;
	PL_TypeSupport	*plp;
	Type		*tp, *otp;
	StructureType	*stp;
	UnionType	*utp;
	TypeLib		*lp, *def_lp;
#ifdef META_LIST
	TypeSupport_t	**np;
	int		is_new;
#endif
	const DDS_TypeSupport_meta *mp = tc;
	size_t		size;
	int		keys, fksize, dkeys, equal, dynamic;
	int		oindex;
	unsigned	oid;
#ifndef CDR_ONLY
	unsigned	count_1, count_2;
	size_t		bytes_1, bytes_2;
#endif
	unsigned	iflags, tflags;
	DDS_ReturnCode_t error;

	prof_start (tc_dtype_reg);
	lock_take (type_lock);
	ctrc_printd (DCPS_ID, 0, (tc && tc->name) ? tc->name : NULL,
                                 (tc && tc->name) ? strlen (tc->name) : 0);

	if (!tc ||
	    (tc->tc != CDR_TYPECODE_STRUCT &&
	     tc->tc != CDR_TYPECODE_UNION && 
	     tc->tc != CDR_TYPECODE_TYPE)) {
		lock_release (type_lock);
		return (NULL);
	}

#ifdef META_LIST
	ml_print1 ("DDS_DynamicType_register (%s);\r\n", mp->name);
	np = sl_insert (&meta_types, tc->name, &is_new, str_cmp);
	if (!np) {
		lock_release (type_lock);
		return (NULL);	/* No memory! */ 
	}
	if (!is_new) {
		ml_print (" => Exists!\r\n");
		dds_ts = *np;
		dds_ts->ts_users++;
		lock_release (type_lock);
		prof_stop (tc_dtype_reg, 1);
		return (dds_ts);
	}
#endif
#ifndef CDR_ONLY
	pool_get_malloc_count (&count_1, &bytes_1);
#endif
#ifdef DUMP_BEFORE_AFTER
	dbg_printf ("\r\nBefore DDS_DynamicType_register(%s) - ", mp->name);
	xt_type_dump (0, NULL);
#endif
	lp = xt_lib_access (0);
	if (!lp) {
		lock_release (type_lock);
		return (NULL); 
	}
	def_lp = lp;
	iflags = IF_TOPLEVEL;
	keys = tc->flags & TSMFLAG_KEY;
	oindex = xt_lib_lookup (lp, tc->name);
	if (oindex >= 0) {	/* Already exists! */
		lp = xt_lib_create (def_lp);
		if (!lp) {
			xt_lib_release (def_lp);
			lock_release (type_lock);
			return (NULL);
		}
	}
	if (tc->tc == CDR_TYPECODE_STRUCT || tc->tc == CDR_TYPECODE_UNION) {
		tp = tsm_create_struct_union (lp, &tc, &iflags);
		if (!tp) {
			xt_lib_release (lp);
			lock_release (type_lock);
			return (NULL); 
		}
		xt_type_finalize (tp,
				  &size,
				  (keys) ? &keys : NULL,
				  &fksize,
				  &dkeys,
				  &dynamic);
		tp->root = 1;
		if (tp->kind == DDS_STRUCTURE_TYPE) {
			stp = (StructureType *) tp;
			stp->keys = (keys != 0);
			stp->fksize = fksize;
			stp->dkeys = dkeys;
			stp->dynamic = dynamic;
		}
		else {
			utp = (UnionType *) tp;
			utp->keys = (keys != 0);
			utp->fksize = fksize;
			utp->dkeys = dkeys;
			utp->dynamic = dynamic;
		}
	}
	else
		tp = tsm_create_typedef (lp, tc, &iflags);

	/* If it already existed, verify equality! */
	if (oindex >= 0) {
		oid = def_lp->types [oindex];
		otp = def_lp->domain->types [oid];
		equal = xt_type_equal (tp, otp);
		xt_lib_release (def_lp);
		xt_type_delete (tp);
		xt_lib_delete (lp);
		if (!equal) {
			lock_release (type_lock);
			return (NULL);
		}
		tp = otp;
		lp = def_lp;
		rcl_access (tp);
		tp->nrefs++;
		rcl_done (tp);
	}
	else
		xt_lib_release (lp);
#ifdef DUMP_BEFORE_AFTER
	dbg_printf ("After DDS_DynamicType_register(%s) - ", mp->name);
	xt_type_dump (0, NULL);
	/*xt_dump_lib (def_lib);*/
#endif
	xt_type_flags_get (tp, &tflags);
	if ((tflags & XTF_EXT_MASK) == XTF_MUTABLE) {
		dds_ts = xmalloc (sizeof (TypeSupport_t) +
				  sizeof (PL_TypeSupport));
		if (dds_ts) {
			plp = (PL_TypeSupport *) (dds_ts + 1);
			plp->builtin = 0;
			plp->type = BT_None;
			plp->xtype = tp;
		}
	}
	else {
		dds_ts = xmalloc (sizeof (TypeSupport_t));
		plp = NULL;
	}
	if (!dds_ts) {
		xt_type_delete (tp);
		lock_release (type_lock);
		return (NULL);
	}
	memset (dds_ts, 0, sizeof (TypeSupport_t));
	dds_ts->ts_name = mp->name;
	dds_ts->ts_keys = mp->flags & TSMFLAG_KEY;
	dds_ts->ts_dynamic = (iflags & IF_DYNAMIC) != 0;
#ifdef DDS_TYPECODE
	dds_ts->ts_origin = TSO_Meta;
#endif
	dds_ts->ts_length = size;
	if (dds_ts->ts_keys)
		dds_ts->ts_fksize = fksize;
	if (plp) {
		plp->builtin = 0;
		plp->type = BT_None;
		plp->xtype = tp;
		dds_ts->ts_prefer = MODE_PL_CDR;
		dds_ts->ts_pl = plp;
	}
	else {
		dds_ts->ts_prefer = MODE_CDR;
		dds_ts->ts_cdr = tp;
	}
	if (dds_ts->ts_keys && !dkeys)
		dds_ts->ts_mkeysize = cdr_marshalled_size (4, NULL, tp, 0, 1, 1, &error);
	dds_ts->ts_meta = mp;
	dds_ts->ts_users = 1;
#ifndef CDR_ONLY
	pool_get_malloc_count (&count_2, &bytes_2);
	log_printf (DDS_ID, 0, "DDS: DynamicType_register(%s): %u blocks, %lu bytes\r\n",
				mp->name,
				count_2 - count_1,
				(unsigned long) (bytes_2 - bytes_1));
#endif
#ifdef META_LIST
	*np = dds_ts;
#ifdef TRACE_MLIST
	ml_print ("After sl_insert(): ");
	sl_walk (&meta_types, dump_meta_type, NULL);
	ml_print ("\r\n");
#endif
#endif
	lock_release (type_lock);
	prof_stop (tc_dtype_reg, 1);
	return (dds_ts);
}

/* DDS_DynamicType_free -- Free the resources associated with a type. */

void DDS_DynamicType_free (DDS_TypeSupport ts)
{
	TS_MODE		mode;
#ifdef META_LIST
	TypeSupport_t **np;
#endif
	PL_TypeSupport	*pl;
#ifdef DUMP_BEFORE_AFTER
	char		s [64];
#endif

	prof_start (tc_dtype_free);
	ctrc_printd (DCPS_ID, 1, &ts, sizeof (ts));
	if (!ts)
		return;	/* Bad parameter! */

	lock_take (type_lock);

#ifdef META_LIST
	ml_print1 ("DDS_DynamicType_free (%s);\r\n", ts->ts_name);
	np = sl_search (&meta_types, ts->ts_name, str_cmp);
	if (!np || *np != ts) {
		ml_print (" ==> Not found!\r\n");
		lock_release (type_lock);
		return;	/* Not found! */
	}
#endif
	if (--ts->ts_users) {
		ml_print (" ==> still users!\r\n");
		lock_release (type_lock);
		return;	/* Done: usecount decremented. */
	}

#ifdef META_LIST
	sl_delete (&meta_types, ts->ts_name, str_cmp);
#ifdef TRACE_MLIST
	ml_print ("After sl_delete(): ");
	sl_walk (&meta_types, dump_meta_type, NULL);
	ml_print ("\r\n");
#endif
#endif
#ifdef DUMP_BEFORE_AFTER
	strcpy (s, ts->ts_name);
	dbg_printf ("\r\nBefore DDS_DynamicType_free(%s) - ", s);
	xt_type_dump (0, NULL);
#endif
	for (mode = MODE_CDR; mode <= MODE_XML; mode++)
		if (ts->ts [mode].cdr)
			switch (mode) {
				case MODE_CDR:
					xt_type_delete (ts->ts_cdr);
					break;

				case MODE_PL_CDR:
					pl = (PL_TypeSupport *) ts->ts_pl;
					if (!pl->builtin)
						xt_type_delete (pl->xtype);
					break;

				default:
			  		break;
			}
#ifdef DUMP_BEFORE_AFTER
	dbg_printf ("\r\nAfter DDS_DynamicType_free(%s) - ", s);
	xt_type_dump (0, NULL);
#endif
	xfree (ts);
	lock_release (type_lock);
	prof_stop (tc_dtype_free, 1);
}

/* Set a type reference to an actual type in the given meta. */

void DDS_DynamicType_set_type (DDS_TypeSupport_meta *tc,
			       unsigned offset,
			       DDS_TypeSupport type)
{
	Type *tp;

	if (type->ts_prefer == MODE_CDR)
		tp = type->ts_cdr;
	else if (type->ts_prefer == MODE_PL_CDR && !type->ts_pl->builtin)
		tp = type->ts_pl->xtype;
	else
		return;

	tsm_typeref_set_type (tc, offset, tp);
}

/* DDS_TypeSupport_data_free -- Free the memory used up by a data sample, using
				the type support metadata. Only use this if the
				allocated memory isn't consecutive. */

void DDS_TypeSupport_data_free (DDS_TypeSupport ts, void *data, int full)
{
	ctrc_printd (DCPS_ID, 2, data, sizeof (data));
	prof_start (tc_sample_free);

	if (ts->ts_dynamic) {
		if (ts->ts_prefer == MODE_CDR)
			xt_data_free (ts->ts_cdr, data, 0);
		else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
			xt_data_free (ts->ts_pl->xtype, data, 0);
	}
	if (full)
		mm_fcts.free_ (data);

	prof_stop (tc_sample_free, 1);
}

/* DDS_TypeSupport_data_copy -- Copy the contents of a sample to another sample. */

void *DDS_TypeSupport_data_copy (DDS_TypeSupport ts, const void *data)
{
	StructureType	*stp;
	Type		*tp;
	void		*dst;

	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
		tp = ts->ts_pl->xtype;
	else
		return (NULL);

	if (tp->kind != DDS_STRUCTURE_TYPE)
		return (NULL);

	stp = (StructureType *) tp;
	dst = mm_fcts.alloc_ (stp->size);
	if (!dst)
		return (NULL);

	if (xt_data_copy (tp, dst, data, stp->size, 0, 0)) {
		mm_fcts.free_ (dst);
		return (NULL);
	}
	return (dst);
}

/* DDS_TypeSupport_data_equals -- Compare two data samples for equality. */

int DDS_TypeSupport_data_equals (DDS_TypeSupport ts,
				 const void *data,
				 const void *other)
{
	StructureType	*stp;
	Type		*tp;

	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
		tp = ts->ts_pl->xtype;
	else
		return (0);

	if (tp->kind != DDS_STRUCTURE_TYPE)
		return (0);

	stp = (StructureType *) tp;
	return (xt_data_equal (tp, data, other, stp->size, 0, 0));
}

#ifdef DDS_DEBUG

/* DDS_TypeSupport_dump_type -- Dump typecode data for visual analysis. */

void DDS_TypeSupport_dump_type (unsigned            indent,
				const TypeSupport_t *ts,
				unsigned            flags)
{
	static const char *mode_str [] = {
		"CDR", "PL_CDR", "XML", "RAW",
		"Vendor-specific Typecode",
		"X-Types Typecode"
	};
	static const char *bi_types [] = {
		"Participant", "Publication", "Subscription", "Topic"
	};
#ifdef DDS_TYPECODE
	TypeLib		*lp;
	TypeSupport_t	*nts;
#endif

	if (!ts)
		return;

	if ((flags & XDF_TS_HEADER) != 0) {
		dbg_print_indent (indent, NULL);
		dbg_printf ("Encoding: %s", mode_str [ts->ts_prefer]);
#ifdef DDS_TYPECODE
		if (ts->ts_prefer < MODE_V_TC) {
#endif
			dbg_printf (", dynamic: %u, length: %lu, keys: %u",
				ts->ts_dynamic,
				(unsigned long) ts->ts_length,
				ts->ts_keys);
			if (ts->ts_keys)
				dbg_printf (", mkeysize: %lu", 
				(unsigned long) ts->ts_mkeysize);
#ifdef DDS_TYPECODE
		}
#endif
		dbg_printf ("\r\n");
	}
	if (ts->ts_prefer == MODE_CDR)
		xt_dump_type (indent, (Type *) ts->ts_cdr, flags);
	else if (ts->ts_prefer == MODE_PL_CDR) {
		PL_TypeSupport *pl = (PL_TypeSupport *) ts->ts_pl;

		if (pl && pl->builtin && pl->type <= BT_Topic)
			dbg_printf ("Builtin %s typecode.\r\n",
							bi_types [pl->type]);
		else if (pl && !pl->builtin && pl->xtype)
			xt_dump_type (indent, pl->xtype, flags);
		else
			dbg_printf ("Unknown PL-CDR type!\r\n");
	}
#ifdef DDS_TYPECODE
	else if (ts->ts_prefer == MODE_V_TC) {
		lp = xt_lib_create (NULL);
		if (!lp)
			return;

		nts = vtc_type (lp, (unsigned char *) ts->ts_vtc);
		if (!nts)
			dbg_printf ("Failed converting typecode to real type!\r\n");
		else {
			DDS_TypeSupport_dump_type (indent, nts, flags & ~XDF_TS_HEADER);
			xt_type_delete (nts->ts_cdr);
			xfree (nts);
			xt_lib_delete (lp);
		}
	}
#endif
}

/* DDS_TypeSupport_dump_data -- Dump a data sample. */

void DDS_TypeSupport_dump_data (unsigned            indent,
				const TypeSupport_t *ts,
				const void          *data,
				int                 native,
				int                 dynamic,
				int                 names)
{
	const unsigned char	*dp = (const unsigned char *) data;
	unsigned		type;
	int			swap;

	if (!ts)
		return;

	if (native) {
		type = ts->ts_prefer;
		swap = 0;
	}
	else {
		type = dp [0] << 8 | dp [1];
		swap = (type & 1) ^ ENDIAN_CPU;
		type >>= 1;
		dp += 4;
	}
	if (type == MODE_CDR) {
		if (native)
			cdr_dump_native (indent, dp, ts->ts_cdr, dynamic, 0, names);
		else
			cdr_dump_cdr (indent, dp, 4, ts->ts_cdr, 0, 0, swap, names);
		return;
	}
	else if (type == MODE_PL_CDR) {
		PL_TypeSupport *pl = (PL_TypeSupport *) ts->ts_pl;

		if (pl && !pl->builtin && pl->xtype) {
			if (native)
				cdr_dump_native (indent, dp, pl->xtype, dynamic, 0, names);
			else
				cdr_dump_cdr (indent, dp, 4, pl->xtype, 0, 0, swap, names);
			return;
		}
	}
	else if (type == MODE_RAW) {
		cdr_dump_native (indent, dp + 4, ts->ts_cdr, 0, 0, names);
		return;
	}
	dbg_printf ("unknown mode: %u\r\n", type);
}

void DDS_TypeSupport_dump_key (unsigned            indent,
			       const TypeSupport_t *ts,
			       const void          *key,
			       int                 native,
			       int                 dynamic,
			       int                 secure,
			       int                 names)
{
	const unsigned char	*dp = (const unsigned char *) key;
	PL_TypeSupport		*pl;
	Type			*tp;
	int			swap, msize;
	char			buffer [32];

	if (!ts || !ts->ts_keys)
		return;

	if (ts->ts_prefer == MODE_CDR) {
		tp = ts->ts_cdr;
		pl = NULL;
	}
	else if (ts->ts_prefer == MODE_PL_CDR) {
		pl = (PL_TypeSupport *) ts->ts_pl;
		if (pl->builtin)
			tp = NULL;
		else
			tp = pl->xtype;
	}
	else {
		tp = NULL;
		pl = NULL;
	}
	if (tp) {
		if (native)
			cdr_dump_native (indent, dp, tp, dynamic, 1, names);
		else {
			if (ts->ts_mkeysize && ts->ts_mkeysize <= 16 && !secure) {
				swap = ENDIAN_CPU ^ ENDIAN_BIG;
				msize = 1;
			}
			else {
				swap = 0;
				msize = 0;
			}
			cdr_dump_cdr (indent, dp, 4, tp, 1, msize, swap, names);
		}
	}
	else if (pl) {
		if (pl->type == BT_Participant)
			dbg_printf ("{guid_prefix=%s}", guid_prefix_str ((GuidPrefix_t *) key, buffer));
		else
			dbg_printf ("{guid=%s}", guid_str ((GUID_t *) key, buffer));
	}
	else
		dbg_printf ("unknown mode: %u", ts->ts_prefer);
}

#endif

/* dds_typesupport_init -- Initialize Typesupport code. */

int dds_typesupport_init (void)
{
	DDS_ReturnCode_t ret;

	lock_init_nr (type_lock, "type");
#ifdef META_LIST
	sl_init (&meta_types, sizeof (TypeSupport_t *));
	sl_init (&dyn_types, sizeof (TypeSupport_t *));
#endif
	ret = xtypes_init ();
	if (ret)
		return (ret);

#ifndef CDR_ONLY
	pl_init ();
	xtopic_init ();

#ifdef DDS_BUILTINS
	builtin_init ();
#endif
#endif

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

#ifdef META_LIST
	
/* dds_cleanup_static_type -- Cleanup a registered static type. */

int dds_cleanup_static_type (Skiplist_t *list, void *node, void *args)
{
	TypeSupport_t	   *ts, **tsp = (TypeSupport_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (args)

	ts = *tsp;
	/*log_printf (DDS_ID, 0, " %s", ts->ts_name);*/
	DDS_DynamicType_free (ts);
	return (1);
}

/* dds_cleanup_dynamic_type -- Cleanup a registered dynamic type. */

int dds_cleanup_dynamic_type (Skiplist_t *list, void *node, void *args)
{
	DDS_DynamicTypeSupport   ts, *tsp = (DDS_DynamicTypeSupport *) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (args)

	ts = *tsp;
	/*log_printf (DDS_ID, 0, " %s", ts->ts_name);*/
	DDS_DynamicTypeSupport_delete_type_support (ts);
	return (1);
}
#endif

/* dds_typesupport_final -- Cleanup Typesupport code. */

void dds_typesupport_final (void)
{
#ifdef META_LIST
	if (meta_types.length) {
		/*log_printf (DDS_ID, 0, "Cleanup static types:");*/
		sl_walk (&meta_types, dds_cleanup_static_type, NULL);
		/*log_printf (DDS_ID, 0, "\r\n");*/
	}
	if (dyn_types.length) {
		/*log_printf (DDS_ID, 0, "Cleanup dynamic types:");*/
		sl_walk (&dyn_types, dds_cleanup_dynamic_type, NULL);
		/*log_printf (DDS_ID, 0, "\r\n");*/
	}
#endif
#ifdef XTYPES_USED
	xd_pool_final ();
#endif
	xtypes_finish ();
	lock_destroy (type_lock);
}

/* DDS_DynamicTypeSupport_create_type_support -- Register a fully dynamic type. */

DDS_DynamicTypeSupport DDS_DynamicTypeSupport_create_type_support (DDS_DynamicType t)
{
	TypeSupport_t	*dds_ts;
	PL_TypeSupport	*plp;
	DynType_t	*dtp = (DynType_t *) t;
	StructureType	*stp;
	size_t		s;
	DDS_ReturnCode_t error;
#ifdef META_LIST
	TypeSupport_t	**np;
	int		is_new;
#endif
#ifndef CDR_ONLY
	unsigned	count_1, count_2;
	size_t		bytes_1, bytes_2;
#endif
	prof_start (tc_dtype_reg);
	lock_take (type_lock);

	stp = (StructureType *) xt_d2type_ptr (dtp, 0);
	if (!stp) {
		lock_release (type_lock);
		return (NULL);
	}

#ifdef META_LIST
	ml_print1 ("DDS_DynamicTypeSupport_create_type_support (%s);\r\n", str_ptr (stp->type.name));
	np = sl_insert (&dyn_types, stp, &is_new, dyn_cmp);
	if (!np) {
		lock_release (type_lock);
		return (NULL);	/* No memory! */ 
	}
	if (!is_new) {
		ml_print (" => Exists!\r\n");
		dds_ts = *np;
		dds_ts->ts_users++;
		lock_release (type_lock);
		prof_stop (tc_dtype_reg, 1);
		return ((DDS_DynamicTypeSupport) dds_ts);
	}
#endif
#ifndef CDR_ONLY
	pool_get_malloc_count (&count_1, &bytes_1);
#endif
	s = sizeof (TypeSupport_t);
	if (stp->type.extensible == MUTABLE)
		s += sizeof (PL_TypeSupport);
	dds_ts = xmalloc (s);
	if (!dds_ts) {
		DDS_DynamicTypeBuilderFactory_delete_type (t);
		lock_release (type_lock);
		return (NULL);
	}
	memset (dds_ts, 0, sizeof (TypeSupport_t));
	dds_ts->ts_name = str_ptr (stp->type.name);
	dds_ts->ts_keys = stp->keys;
	dds_ts->ts_fksize = stp->fksize;
	dds_ts->ts_dynamic = 1;
#ifdef DDS_TYPECODE
	dds_ts->ts_origin = TSO_Dynamic;
#endif
	if (stp->keys && !stp->dkeys)
		dds_ts->ts_mkeysize = cdr_marshalled_size (4, NULL, &stp->type, 0, 1, 1, &error);
	else
		dds_ts->ts_mkeysize = 0;
	dds_ts->ts_length = stp->size;
	if (stp->type.extensible == MUTABLE) {
		dds_ts->ts_prefer = MODE_PL_CDR;
		plp = (PL_TypeSupport *) (dds_ts + 1);
		dds_ts->ts_pl = plp;
		plp->builtin = 0;
		plp->type = BT_None;
		plp->xtype = &stp->type;
	}
	else {
		dds_ts->ts_prefer = MODE_CDR;
		dds_ts->ts_cdr = &stp->type;
	}
	dds_ts->ts_meta = NULL;
	dds_ts->ts_users = 1;
	stp->type.nrefs++;
#ifndef CDR_ONLY
	pool_get_malloc_count (&count_2, &bytes_2);
	log_printf (DDS_ID, 0, "DDS: DynamicTypeSupport_create_type_support(%s): %u blocks, %lu bytes\r\n",
				str_ptr (stp->type.name),
				count_2 - count_1,
				(unsigned long) (bytes_2 - bytes_1));
#endif
#ifdef META_LIST
	*np = dds_ts;
#ifdef TRACE_MLIST
	ml_print ("After sl_insert(): ");
	sl_walk (&dyn_types, dump_meta_type, NULL);
	ml_print ("\r\n");
#endif
#endif
	lock_release (type_lock);
	prof_stop (tc_dtype_reg, 1);
	return ((DDS_DynamicTypeSupport) dds_ts);
}

/* DDS_TypeSupport_get_type_name -- Get the type name of a dynamic type. */

DDS_ObjectName DDS_DynamicTypeSupport_get_type_name (DDS_DynamicTypeSupport self)
{
	TypeSupport_t	*ts = (TypeSupport_t *) self;

	if (!ts
#ifdef DDS_TYPECODE
	    || ts->ts_origin != TSO_Dynamic
#endif
	                                   )
		return (NULL);

	return ((char *) ts->ts_name);
}

DDS_DynamicType DDS_DynamicTypeSupport_get_type (DDS_DynamicTypeSupport self)
{
	TypeSupport_t	*ts = (TypeSupport_t *) self;
	Type		*tp;

	if (!ts)
		return (NULL);

	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
		tp = ts->ts_pl->xtype;
	else
		return (NULL);

	tp->nrefs++;
	return ((DDS_DynamicType) xt_dynamic_ptr (tp, 0));
}

static DDS_ReturnCode_t DDS_DynamicTypeSupport_free (DDS_DynamicTypeSupport ts)
{
	TypeSupport_t	*tsp = (TypeSupport_t *) ts;
#ifdef META_LIST
	TypeSupport_t	**np;
#endif
	PL_TypeSupport	*pl;
	StructureType	*stp;
#ifdef DUMP_BEFORE_AFTER
	char		s [64];
#endif

	prof_start (tc_dtype_free);

	if (!ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (type_lock);

	if (tsp->ts_prefer == MODE_CDR)
		stp = (StructureType *) tsp->ts_cdr;
	else if (tsp->ts_prefer == MODE_PL_CDR) {
		pl = (PL_TypeSupport *) tsp->ts_pl;
		if (!pl->builtin)
			stp = (StructureType *) pl->xtype;
		else
			stp = NULL;
	}
	else
		stp = NULL;
	if (!stp) {
		lock_release (type_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}

#ifdef META_LIST
	ml_print1 ("DDS_DynamicTypeSupport_free (%s);\r\n", ts->ts_name);
	np = sl_search (&dyn_types, stp, dyn_cmp);
	if (!np || *np != (TypeSupport_t *) ts) {
		ml_print (" ==> Not found!\r\n");
		lock_release (type_lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
#endif
	if (--tsp->ts_users) {
		ml_print (" ==> still users!\r\n");
		lock_release (type_lock);
		return (DDS_RETCODE_OK); /* Done: usecount decremented. */
	}

#ifdef META_LIST
	sl_delete (&dyn_types, stp, dyn_cmp);
#ifdef TRACE_MLIST
	ml_print ("After sl_delete(): ");
	sl_walk (&dyn_types, dump_meta_type, NULL);
	ml_print ("\r\n");
#endif
#endif
#ifdef DUMP_BEFORE_AFTER
	strcpy (s, tsp->ts_name);
	dbg_printf ("\r\nBefore xt_type_detete(%s) - ", s);
	xt_type_dump (0, NULL);
#endif
	xt_type_delete (&stp->type);

#ifdef DUMP_BEFORE_AFTER
	dbg_printf ("\r\nAfter xt_type_detete(%s) - ", s);
	xt_type_dump (0, NULL);
#endif
	xfree (ts);
	lock_release (type_lock);
	prof_stop (tc_dtype_free, 1);
	return (DDS_RETCODE_OK);
}

/* DDS_TypeSupport_delete -- Delete a previously constructed type. */

DDS_ReturnCode_t DDS_TypeSupport_delete (TypeSupport_t *ts)
{
	if (!ts)
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef DDS_TYPECODE
	switch (ts->ts_origin) {
		case TSO_Meta:
#endif
			DDS_DynamicType_free (ts);
#ifdef DDS_TYPECODE
			break;
		case TSO_Dynamic:
			DDS_DynamicTypeSupport_free ((DDS_DynamicTypeSupport) ts);
			break;
		case TSO_Typecode:
			vtc_delete (ts);
			break;
		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
#endif
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DynamicTypeSupport_delete_type_support (DDS_DynamicTypeSupport ts)
{
	return (DDS_TypeSupport_delete ((TypeSupport_t *) ts));
}


/* Data marshalling and unmarshalling. */
/* ----------------------------------- */

/* DDS_MarshalledDataSize -- Return the buffer size for marshalled data. */

size_t DDS_MarshalledDataSize (const void          *sample,
			       int                 dynamic,
			       const TypeSupport_t *ts,
			       DDS_ReturnCode_t    *ret)
{
	size_t	length;

	if (ret)
		*ret = DDS_RETCODE_BAD_PARAMETER;
	if (!ts || !sample)
		return (0);

	if (ts->ts_prefer == MODE_CDR)
		length = cdr_marshalled_size (4, sample, ts->ts_cdr,
					       dynamic, 0, 0, ret);
	else if (ts->ts_prefer == MODE_PL_CDR)
#ifndef CDR_ONLY
		if (ts->ts_pl->builtin)
			length = pl_marshalled_size (sample, ts->ts_pl, 0, ret);
		else
#endif
			length = cdr_marshalled_size (4, sample,
						       ts->ts_pl->xtype,
						       dynamic, 0, 0, ret);
	else /* No XML for now. */
		return (0);

	if (!length)
		return (0);

	if (ret)
		*ret = DDS_RETCODE_OK;
	return (length + 4);
}

/* DDS_MarshallData -- Marshall payload data using the proper type support coding
		       for either local (in-device.*/

DDS_ReturnCode_t DDS_MarshallData (void                *buffer,
			           const void          *data,
				   int                 dynamic,
			           const TypeSupport_t *ts)
{
	unsigned char	*dp = buffer;
	size_t		length;
	DDS_ReturnCode_t ret;

	if (!buffer || !data ||
	    !ts || ts->ts_prefer > MODE_XML || !ts->ts [ts->ts_prefer].cdr)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (ts->ts_prefer == MODE_CDR) {
		length = cdr_marshall (dp + 4, 4, data, ts->ts_cdr,
							dynamic, 0, 0, 0, &ret);
		if (!length)
			return (ret);

		dp [0] = 0;
		dp [1] = (MODE_CDR << 1) | ENDIAN_CPU;
	}
	else if (ts->ts_prefer == MODE_PL_CDR) {
#ifndef CDR_ONLY
		if (ts->ts_pl->builtin)
			ret = pl_marshall (dp + 4, data, ts->ts_pl, 0, 0);
		else
#endif
			length = cdr_marshall (dp + 4, 4, data,
					     ts->ts_pl->xtype, dynamic,
					     0, 0, 0, &ret);
		dp [0] = 0;
		dp [1] = (MODE_PL_CDR << 1) | ENDIAN_CPU;
	}
	else /* No XML for now. */
		return (DDS_RETCODE_BAD_PARAMETER);

	dp [2] = dp [3] = 0;
	return (DDS_RETCODE_OK);
}

/* DDS_UnmarshalledDataSize -- Return the buffer size needed for marshalled data. */

size_t DDS_UnmarshalledDataSize (DBW                 data,
			         const TypeSupport_t *ts,
			         DDS_ReturnCode_t    *error)
{
	const unsigned char	*dp;
	unsigned		type;
	int			swap;

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
	swap = (type & 1) ^ ENDIAN_CPU;
	if ((type >> 1) == MODE_CDR) {
		if (ts->ts_cdr)
			return (cdr_unmarshalled_size (data.data, 4, ts->ts_cdr,
						       0, 0, swap, 0, error));
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
	else if ((type >> 1) == MODE_PL_CDR) {
		if (!ts->ts_pl) {
			if (error)
				*error = DDS_RETCODE_UNSUPPORTED;
		}
#ifndef CDR_ONLY
		else if (ts->ts_pl->builtin)
			return (pl_unmarshalled_size (&data, ts->ts_pl, error,
									swap));
#endif
		else
			return (cdr_unmarshalled_size (data.data, 4,
						       ts->ts_pl->xtype,
						       0, 0, swap, 0, error));
	}
	else if (error)
		*error = DDS_RETCODE_UNSUPPORTED;

	return (0);
}

/* DDS_UnmarshallData -- Unmarshall payload data using the proper type support
		         package. */

DDS_ReturnCode_t DDS_UnmarshallData (void                *buffer,
				     DBW                 *data,
				     const TypeSupport_t *ts)
{
	int		 	swap;
	const unsigned char	*dp;
	unsigned		type;
	DDS_ReturnCode_t	ret = DDS_RETCODE_OK;

	dp = data->data;
	type = dp [0] << 8 | dp [1];
	swap = (type & 1) ^ ENDIAN_CPU;
	DBW_INC (*data, 4);
	if (DBW_REMAIN (*data) <= 4)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((type >> 1) == MODE_CDR) {
		if (ts->ts_cdr)
			ret = cdr_unmarshall (buffer, DBW_PTR (*data), 4,
					      ts->ts_cdr, 0, 0, swap, 0);
		else
			ret = DDS_RETCODE_UNSUPPORTED;
	}
	else if ((type >> 1) == MODE_PL_CDR) {
		if (!ts->ts_pl)
			ret = DDS_RETCODE_UNSUPPORTED;
#ifndef CDR_ONLY
		else if (ts->ts_pl->builtin)
			ret = pl_unmarshall (buffer, data, ts->ts_pl, swap);
#endif
		else
			ret = cdr_unmarshall (buffer, DBW_PTR (*data), 4,
					      ts->ts_pl->xtype, 0, 0, swap, 0);
	}
	else if ((type >> 1) == MODE_XML)
		ret = DDS_RETCODE_UNSUPPORTED;
	else
		ret = DDS_RETCODE_BAD_PARAMETER;
	return (ret);
}

/* DDS_KeySizeFromNativeData -- Returns the total size of the key fields from
				a native data sample. */

size_t DDS_KeySizeFromNativeData (const unsigned char *data,
				  int                 dynamic,
				  const TypeSupport_t *ts,
				  DDS_ReturnCode_t    *error)
{
	size_t	l;

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
	if (ts->ts_prefer == MODE_CDR) {
		l = cdr_marshalled_size (4, data, ts->ts_cdr, 
							dynamic, 1, 0, error);
		prof_stop (tc_k_s_nat, 1);
		return (l);
	}
	else if (ts->ts_prefer == MODE_PL_CDR) {
#ifndef CDR_ONLY
		if (ts->ts_pl->builtin)
			l = pl_marshalled_size (data, ts->ts_pl, 1, error);
		else
#endif
			l = cdr_marshalled_size (4, data, ts->ts_pl->xtype, 
							dynamic, 1, 0, error);
		prof_stop (tc_k_s_nat, 1);
		return (l);
	}
	else if (ts->ts_prefer == MODE_XML) {
		if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
	else if (error)
		*error = DDS_RETCODE_BAD_PARAMETER;
	return (0);
}

/* DDS_KeyFromNativeData -- Extract the key fields from a non-marshalled native
			    data sample (data). */

DDS_ReturnCode_t DDS_KeyFromNativeData (unsigned char       *key,
					const void          *data,
					int                 dynamic,
					int                 secure,
					const TypeSupport_t *ts)
{
	int			swap, msize;
	size_t			length;
	DDS_ReturnCode_t	ret;

	prof_start (tc_k_g_nat);

	if (!ts || !ts->ts_keys || !data || !key)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (ts->ts_mkeysize && ts->ts_mkeysize <= 16 && !secure) {
		swap = ENDIAN_CPU ^ ENDIAN_BIG;
		msize = 1;
	}
	else {
		swap = 0;
		msize = 0;
	}
	if (ts->ts_prefer == MODE_CDR) {
		length = cdr_marshall (key, 4, data, ts->ts_cdr, 
						dynamic, 1, msize, swap, &ret);
		prof_stop (tc_k_g_nat, 1);
		return ((length) ? DDS_RETCODE_OK : ret);
	}
	else if (ts->ts_prefer == MODE_PL_CDR) {
#ifndef CDR_ONLY
		if (ts->ts_pl->builtin)
			ret = pl_marshall (key, data, ts->ts_pl, 1, 0);
		else {
#endif
			length = cdr_marshall (key, 4, data, ts->ts_pl->xtype, 
						dynamic, 1, msize, swap, &ret);
			if (length)
				ret = DDS_RETCODE_OK;
#ifndef CDR_ONLY
		}
#endif
		prof_stop (tc_k_g_nat, 1);
		return (ret);
	}
 	return (DDS_RETCODE_UNSUPPORTED);
}

/* DDS_KeyToNativeData -- Copy the key fields to a native data sample. */

DDS_ReturnCode_t DDS_KeyToNativeData (void                *data,
				      int                 dynamic,
				      int                 secure,
				      const void          *key,
				      const TypeSupport_t *ts)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;
	DynDataRef_t	 *ddrp;
	int		 swap, msize;

	prof_start (tc_k_p_nat);
	if (!data || !key || !ts)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (dynamic) {
		ddrp = (DynDataRef_t *) data;
		if (ddrp->magic != DD_MAGIC)
			return (DDS_RETCODE_BAD_PARAMETER);

		xd_delete_data (ddrp->ddata);
	}
	else
		ddrp = NULL;
	if (ts->ts_mkeysize && ts->ts_mkeysize <= 16 && !secure) {
		swap = ENDIAN_CPU ^ ENDIAN_BIG;
		msize = 1;
	}
	else {
		swap = 0;
		msize = 0;
	}
	if (ts->ts_prefer == MODE_CDR) {
		if (dynamic)
			ddrp->ddata = cdr_dynamic_data (key, 4,
							ts->ts_cdr, 1, 1, swap);
		else
			ret = cdr_unmarshall (data, key, 4,
						 ts->ts_cdr, 1, msize, swap, 0);
		prof_stop (tc_k_p_nat, 1);
	}
	else if (ts->ts_prefer == MODE_PL_CDR) {
#ifndef CDR_ONLY
		if (!ts->ts_pl->builtin)
#endif
			if (dynamic)
				ddrp->ddata = cdr_dynamic_data (key, 4,
						  ts->ts_pl->xtype, 1, 1, swap);
			else
				ret = cdr_unmarshall (data, key, 4,
					     ts->ts_pl->xtype, 1, msize, swap, 0);
#ifndef CDR_ONLY
		else
			memcpy (data, key, (ts->ts_pl->type == BT_Participant) ? 12 : 16);
#endif
	}
	else
		ret = DDS_RETCODE_UNSUPPORTED;
	return (ret);
}

/* DDS_KeySizeFromMarshalled -- Returns the total size of the key fields
				in a marshalled data sample. */

size_t DDS_KeySizeFromMarshalled (DBW                 data,
				  const TypeSupport_t *ts,
				  int                 key,
				  DDS_ReturnCode_t    *error)
{
	const unsigned char	*dp;
	const Type		*tp;
	unsigned		type;
	int			swap;
	size_t			l;

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
	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
		tp = ts->ts_pl->xtype;
	else
		tp = NULL;
	if ((type >> 1) == MODE_CDR) {
		if (tp) {
			l = cdr_key_size (4, data.data, 4, tp, key, 0,
							     swap, swap, error);
			prof_stop (tc_k_s_marsh, 1);
			return (l);
		}
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
	else if ((type >> 1) == MODE_PL_CDR) {
		if (tp) {
			l = cdr_key_size (4, data.data, 4, tp, key, 0,
					  swap, swap, error);
			prof_stop (tc_k_s_marsh, 1);
			return (l);
		}
#ifndef CDR_ONLY
		else if (ts->ts_pl && ts->ts_pl->builtin) {
			l = pl_key_size (data, ts->ts_pl, swap, error);
			prof_stop (tc_k_s_marsh, 1);
			return (l);
		}
#endif
		else if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}
	else if ((type >> 1) == MODE_RAW)
		if (ts->ts_cdr) {
			l = cdr_marshalled_size (4, data.data, ts->ts_cdr, 0,
								  1, 0, error);
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

DDS_ReturnCode_t DDS_KeyFromMarshalled (unsigned char       *dst,
					DBW                 data,
					const TypeSupport_t *ts,
					int                 key,
					int                 secure)
{
	const unsigned char	*dp;
	const Type		*tp;
	unsigned		type;
	int			swap, iswap, msize;
	size_t			length;
	DDS_ReturnCode_t	ret;

	prof_start (tc_k_g_marsh);
	if (!ts || !ts->ts_keys || data.length < 8)
		return (DDS_RETCODE_BAD_PARAMETER);

	dp = DBW_PTR (data);
	type = dp [0] << 8 | dp [1];
	DBW_INC (data, 4);
	iswap = (type & 1) ^ ENDIAN_CPU;
	if (ts->ts_mkeysize && ts->ts_mkeysize <= 16 && !secure) {
		swap = (type & 1) ^ ENDIAN_BIG;
		msize = 1;
	}
	else {
		swap = iswap;
		msize = 0;
	}
	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
		tp = ts->ts_pl->xtype;
	else
		tp = NULL;
	if ((type >> 1) == MODE_CDR) {
		if (tp) {
			ret = cdr_key_fields (dst, 4, data.data, 4, tp,
						       key, msize, swap, iswap);
			prof_stop (tc_k_g_marsh, 1);
			return (ret);
		}
		else if (ts->ts_pl && ts->ts_pl->builtin) {
			memcpy (dst, data.data, ts->ts_mkeysize);
			return (DDS_RETCODE_OK);
		}
		else
			return (DDS_RETCODE_UNSUPPORTED);
	}
	else if ((type >> 1) == MODE_PL_CDR) {
		if (tp) {
			ret = cdr_key_fields (dst, 4, data.data, 4, tp,
						       key, msize, swap, iswap);
			prof_stop (tc_k_g_marsh, 1);
			return (ret);
		}
#ifndef CDR_ONLY
		else if (ts->ts_pl && ts->ts_pl->builtin) {
			swap = (type & 1) ^ ENDIAN_CPU;
			ret = pl_key_fields (dst, &data, ts->ts_pl, swap);
			prof_stop (tc_k_g_marsh, 1);
			return (ret);
		}
#endif
		else
			return (DDS_RETCODE_UNSUPPORTED);
	}
	else if ((type >> 1) == MODE_RAW) {
		if (ts->ts_cdr) {
			if (ts->ts_mkeysize && (ts->ts_mkeysize > 16 || secure))
				swap = 0;
			else
				swap = ENDIAN_CPU ^ ENDIAN_BIG;
			length = cdr_marshall (dst, 4, data.data, ts->ts_cdr, 0,
								1, msize, swap, &ret);
			prof_stop (tc_k_g_marsh, 1);
			return ((length) ? DDS_RETCODE_OK : ret);
		}
		else
			return (DDS_RETCODE_UNSUPPORTED);
	}
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

#define	KEY_HSIZE	4	/* Assume same alignment for the MD5 checksum
				   data as for transported key fields. */

/* DDS_HashFromKey -- Calculate the hash value from a key. */

DDS_ReturnCode_t DDS_HashFromKey (unsigned char       hash [16],
				  const unsigned char *key,
				  size_t              key_size,
				  int                 secure,
				  const TypeSupport_t *ts)
{
	MD5_CONTEXT		mdc;
	unsigned char		*dp;
	unsigned		i;
	int			free_key = 0, swap = ENDIAN_CPU ^ ENDIAN_BIG;
	size_t			n;
	Type			*tp;
	DDS_ReturnCode_t	ret;
#ifdef MAX_KEY_BUFFER
	unsigned char		key_buffer [MAX_KEY_BUFFER];
#endif

	prof_start (tc_h_key);
	if (ts->ts_mkeysize && ts->ts_mkeysize <= 16 && !secure) {
		memcpy (hash, key, ts->ts_mkeysize);
		for (i = ts->ts_mkeysize; i < 16; i++)
			hash [i] = 0;

		prof_stop (tc_h_key, 1);
		return (DDS_RETCODE_OK);
	}
	dp = NULL;
	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR)
		if (ts->ts_pl->builtin)
			tp = NULL;
		else
			tp = ts->ts_pl->xtype;
	else
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!tp) {
		n = key_size;
		dp = (unsigned char *) key;
	}
	else if (!ts->ts_mkeysize || ts->ts_mkeysize == key_size) {
		if (!swap)
			dp = (unsigned char *) key;
#if (KEY_HSIZE == 4)
		n = key_size;
#else
		n = cdr_key_size (KEY_HSIZE, key, 4, tp, 1, 1, swap, 0, &ret);
#endif
	}
	else if (ts->ts_mkeysize)
		n = ts->ts_mkeysize;
	else {
		n = cdr_key_size (KEY_HSIZE, key, 4, tp, 1, 1, swap, 0, &ret);
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
		ret = cdr_key_fields (dp, KEY_HSIZE, key, 4, tp, 1, 1, swap, 0);
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

