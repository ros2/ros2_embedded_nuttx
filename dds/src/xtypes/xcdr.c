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

/* xcdr.c -- CDR marshaller code for X-Types. */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#if defined (NUTTX_RTOS)
#define PRId64	"lld"
#define PRIu64	"llu"
#else
#include <wctype.h>
#endif

#ifdef _WIN32
#include "win.h"
#else
#include <inttypes.h>
#include <arpa/inet.h>
#endif
#include "sys.h"
#include "pool.h"
#include "prof.h"
#include "error.h"
#ifdef DDS_DEBUG
#include "debug.h"
#endif
#include "xdata.h"
#include "xcdr.h"

#ifdef _WIN32
#define INLINE
#else
#define INLINE inline
#endif

#ifdef PROFILE
PROF_PID (cdr_m_size)
PROF_PID (cdr_m)
PROF_PID (cdr_um_size)
PROF_PID (cdr_um)
PROF_PID (cdr_k_size)
PROF_PID (cdr_k_get)
PROF_PID (cdr_f_ofs)
#endif

#define	MAX_MKFDS	8	/* Default max. # of key fields. */

#define	dm_printf(s,a)

#define	XT_PID_VENDOR	0x8000	/* Vendor-specific extension flag. */
#define	XT_PID_UNDERST	0x4000	/* Must-understand flag. */
#define	XT_PID_ID_MASK	0x3fff	/* Mask for Id. */
#define	XT_PID_EXTENDED	0x3f01	/* Extended Parameter format follows. */
#define	XT_PID_LIST_END	0x3f02	/* End of Parameter list. */
#define	XT_PID_IGNORE	0x3f03	/* Ignore this Parameter. */
#define	XT_PID RESERVED	0x3f04	/* Parameters in 0x3f04..0x3fff are reserved. */

#define	ALIGN(delta, boundary)	((delta + boundary - 1) & ~(boundary - 1))
#define CDR_ALIGN(delta, boundary, do_write, out) \
	for (; delta < ALIGN (delta, boundary); delta++) \
		if (do_write) (out) [delta] = '\0';

static Type *real_type_ptr (unsigned scope, unsigned id)
{
	Type	*tp;
	TypeLib	*lp = xt_lib_ptr (scope);

	if (!lp)
		return (NULL);

	for (;;) {
		tp = lp->domain->types [id];
		if (!tp)
			return (NULL);

		if (tp->kind != DDS_ALIAS_TYPE)
			return (tp);

		id = ((AliasType *) tp)->base_type;
	}
}

typedef enum {
	CF_Native,
	CF_CDR,
	CF_CDR_Key,
	CF_CDR_PaddedKey
} CDRFormat;

#define	CF_Key(f)	((f) >= CF_CDR_Key)

typedef struct gen_info_st {
	const unsigned char	*src;
	const DynData_t		*sdp;
	CDRFormat		dformat;
	unsigned		dynamic:1;
	unsigned		key_mode:1;
	unsigned		swap:1;
	unsigned		dump:1;
	unsigned		names:1;
	unsigned		indent:8;
} GenInfo_t;

static const unsigned char zeroes [16];

static size_t cdr_generate (unsigned char *out,
			    size_t        offset,
			    const Type    *type,
			    GenInfo_t     *ip,
			    int           key,
			    int           shared);

static size_t cdr_generate_member (unsigned char         *out,
				   size_t                offset,
				   unsigned              scope,
				   const Member          *mp,
				   const DynDataMember_t *fp,
				   GenInfo_t             *ip,
				   int                   mutable,
				   int                   disc,
				   int                   all_key)
{
	const Type		*tp;
	const void		*ndata;
	uint32_t		pid;
	uint16_t		*s_len_ptr = NULL, s, sl;
	uint32_t		*l_len_ptr = NULL, l;
	const unsigned char	*prev_data;
	const DynData_t		*prev_sdp;
	int			prev_dynamic;
	int			do_write, add_header, skip_optional = 0;
	size_t			delta = offset, n;

	prev_data = ip->src;
	prev_sdp = ip->sdp;
	if ((prev_dynamic = ip->dynamic) != 0) {
		do_write = out && ip->sdp;
		if (ip->sdp && fp) {
			ndata = ip->sdp->dp + fp->offset;
			if ((fp->flags & DMF_DYNAMIC) != 0)
				ip->sdp = *((DynData_t **) ndata);
			else {
				ip->dynamic = 0;
				ip->src = ndata;
				ip->sdp = NULL;
			}
		}
		else
			ip->src = zeroes;
	}
	else if (ip->src) {
		do_write = (out != NULL);
		ndata = ip->src + mp->offset;
		if (mp->is_shareable || mp->is_optional) {
			ndata = *((const void **) ndata);
			if (ndata)
				ip->src = ndata;
			else if (!mp->is_optional)
				return (0);
			else
				skip_optional = 1;
		}
		else
			ip->src = ndata;
	}
	else
		do_write = 0;

	add_header = mutable && !skip_optional;
	if (add_header)	{ /* Add Parameter header! */
		CDR_ALIGN (delta, 4, do_write, out);
		if (mp->member_id >= XT_PID_EXTENDED) {
			if (do_write) {
				s = XT_PID_EXTENDED;
				if (mp->must_understand)
					s |= XT_PID_UNDERST;
				pid = mp->member_id;
				sl = 8;
				if (ip->swap) {
					memcswap16 (&out [delta], &s);
					memcswap16 (&out [delta + 2], &sl);
					memcswap32 (&out [delta + 4], &pid);
				}
				else {
					memcpy16 (&out [delta], &s);
					memcpy16 (&out [delta + 2], &sl);
					memcpy32 (&out [delta + 4], &pid);
				}
				l_len_ptr = (uint32_t *) &out [delta + 8];
			}
			delta += 12;
		}
		else {
			if (do_write) {
				s = mp->member_id;
				if (mp->must_understand)
					s |= XT_PID_UNDERST;
				if (ip->swap)
					memcswap16 (&out [delta], &s);
				else
					memcpy16 (&out [delta], &s);
				s_len_ptr = (uint16_t *) &out [delta + 2];
			}
			delta += 4;
		}
	}
	else if (!mutable && mp->is_optional) {
		CDR_ALIGN (delta, 4, do_write, out);
		l_len_ptr = (uint32_t *) &out [delta];
	}
	if (ip->dump && !disc) {
		if (ip->names)
			dbg_printf ("%s=", str_ptr (mp->name));
		else if (mutable)
			dbg_printf (".%s=", str_ptr (mp->name));
	}
	if (!skip_optional) {
		tp = real_type_ptr (scope, mp->id);
		n = cdr_generate (out, delta, tp, ip,
				  mp->is_key || all_key,
				  mp->is_shareable);
		if (!n)
			return (0);
	}
	else
		n = delta;
	ip->src = prev_data;
	ip->sdp = prev_sdp;
	ip->dynamic = prev_dynamic;
	if (add_header && do_write) {	/* Backpatch parameter length. */
		CDR_ALIGN (n, 4, do_write, out);
		if (mp->member_id >= XT_PID_EXTENDED) {
			l = n - delta;
			if (ip->swap)
				memcswap32 (l_len_ptr, &l);
			else
				memcpy32 (l_len_ptr, &l);
		}
		else {
			s = n - delta;
			if (ip->swap)
				memcswap16 (s_len_ptr, &s);
			else
				memcpy16 (s_len_ptr, &s);
		}
	}
	else if (!mutable && mp->is_optional) {
		l = n - delta;
		if (ip->swap)
			memcswap32 (l_len_ptr, &l);
		else
			memcpy32 (l_len_ptr, &l);
	}
	return (n);
}

static size_t cdr_generate_eol (unsigned char *out, size_t offset, int swap)
{
	int		do_write = out != NULL;
	int16_t		hdr;
	size_t		delta = offset;

	CDR_ALIGN (delta, 4, do_write, out);
	if (do_write) {
		hdr = XT_PID_LIST_END;
		if (swap)
			memcswap16 (&out [delta], &hdr);
		else
			memcpy16 (&out [delta], &hdr);
		out [delta + 2] = 0;
		out [delta + 3] = 0;
	}
	return (delta + 4);
}

static size_t cdr_generate_struct_cdr (unsigned char       *out,
				       size_t              offset,
				       const StructureType *stp,
				       GenInfo_t           *ip)
{
	const Member		*mp = NULL;
	const DynDataMember_t	*fp;
	unsigned		i, j, nf = 0;
	size_t			delta = offset, n;
	int			use_member;

	for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++) {
		if (ip->dynamic && ip->sdp) { /* Lookup member. */
			for (j = 0, fp = ip->sdp->fields;
			     j < ip->sdp->nfields && (fp->flags & DMF_PRESENT) != 0;
			     j++, fp++)
				if (fp->index == i)
					break;

			if (j >= ip->sdp->nfields)
				fp = NULL;
		}
		else
			fp = NULL;

		use_member = !ip->key_mode || mp->is_key || !stp->keyed;
		if (!use_member)
			continue;

		if (ip->dump && nf++)
			dbg_printf (", ");

		n = cdr_generate_member (out, delta, stp->type.scope, mp, fp,
				                  ip, 0, 0, !stp->keyed);
		if (!n)
			return (0);

		delta = n;
	}
	return (delta);
}

static size_t cdr_generate_mutable_unordered (unsigned char       *out,
					      size_t              offset,
					      const StructureType *stp,
					      GenInfo_t           *ip)
{
	const Member		*mp = NULL;
	const DynDataMember_t	*fp;
	unsigned		i, j, nf = 0;
	size_t			delta = offset, n;

	for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++) {
		if (ip->dynamic && ip->sdp) { /* Lookup member. */
			for (j = 0, fp = ip->sdp->fields;
			     j < ip->sdp->nfields && (fp->flags & DMF_PRESENT) != 0;
			     j++, fp++)
				if (fp->index == i)
					break;

			if (j >= ip->sdp->nfields)
				continue;
		}
		else
			fp = NULL;

		if (ip->dump && nf++)
			dbg_printf (", ");

		n = cdr_generate_member (out, delta, stp->type.scope, mp, fp,
					 ip, 1, 0, 0);
		if (!n)
			return (0);

		delta = n;
	}
	delta = cdr_generate_eol (out, delta, ip->swap);
	return (delta);
}

typedef struct mp_fdesc_st {
	unsigned	f;
	unsigned	id: 28;
	unsigned	found: 1;
	unsigned	index;
} MPFDesc_st;

static int frcmp (const void *a1, const void *a2)
{
	MPFDesc_st	*fp1 = (MPFDesc_st *) a1,
			*fp2 = (MPFDesc_st *) a2;

	return ((int) fp1->id - (int) fp2->id);
}

static size_t cdr_generate_mutable_ordered (unsigned char       *out,
					    size_t              offset,
					    const StructureType *stp,
					    GenInfo_t           *ip)
{
	const Member		*mp = NULL;
	const DynDataMember_t	*fp;
	unsigned		i, j, nkf, nff, prev_id = 0;
	int			sort;
	size_t			delta = offset, n;
	MPFDesc_st		*fds, *fdp;
	MPFDesc_st		mfds [MAX_MKFDS];

	/* 1. Count # of key fields and point fds to an array large enough
	      to contain all key field descriptors. */
	if (!stp->keyed)
		nkf = stp->nmembers;
	else
		for (i = 0, nkf = 0, mp = stp->member; i < stp->nmembers; i++, mp++)
			if (mp->is_key)
				nkf++;
	if (nkf <= 1 && ip->sdp) /* No need to sort fields! */
		return (cdr_generate_struct_cdr (out, offset, stp, ip));

	if (nkf > MAX_MKFDS) {
		if ((fds = xmalloc (nkf * sizeof (MPFDesc_st))) == NULL)
			return (0);
	}
	else
		fds = mfds;

	/* 2. Initialize all field descriptors, and check if sorting needed. */
	memset (fds, 0, nkf * sizeof (MPFDesc_st));
	fdp = fds;
	for (i = 0, mp = stp->member, sort = 0; i < stp->nmembers; i++, mp++)
		if (mp->is_key || !stp->keyed) {
			if (mp->member_id < prev_id)
				sort = 1;
			prev_id = fdp->id = mp->member_id;
			fdp->index = i;
			fdp++;
		}

	/* 3. Check presence of key fields, and whether sorting is needed. */
	if (ip->sdp) {
		for (i = 0, nff = 0, fp = ip->sdp->fields;
		     i < stp->nmembers;
		     i++, fp++) {
			if ((fp->flags & DMF_PRESENT) == 0)
				continue;

			mp = &stp->member [fp->index];
			if (!mp->is_key && stp->keyed)
				continue;

			for (j = 0, fdp = fds; j < nkf; j++, fdp++)
				if (fdp->id == mp->member_id)
					break;

			if (j >= nkf || fdp->found) {

				/* !found || duplicate! */
				delta = 0;
				goto cleanup;
			}
			fdp->f = i;
			fdp->found = 1;
			nff++;
		}
		if (nff != nkf) {	/* Some key fields missing! */
			delta = 0;
			goto cleanup;
		}
	}

	/* 4. Sort the key fields on member id. */
	if (sort)
		qsort (fds, nkf, sizeof (MPFDesc_st), frcmp);

	/* 5. Generate each key field in member_id order. */
	for (i = 0, fdp = fds, nff = 0; i < nkf; i++, fdp++) {
		if (ip->dump && i)
			dbg_printf (", ");

		if (ip->sdp)
			fp = &ip->sdp->fields [fdp->f];
		else
			fp = NULL;
		mp = &stp->member [fdp->index];
		n = cdr_generate_member (out, delta, stp->type.scope, mp, fp,
			                      ip, 0, 0, !stp->keyed);
		if (!n) {
			delta = 0;
			goto cleanup;
		}
		delta = n;
	}

    cleanup:
	if (nkf > MAX_MKFDS)
		xfree (fds);
	return (delta);
}

static size_t cdr_generate_struct (unsigned char       *out,
				   size_t              offset,
				   const StructureType *stp,
				   GenInfo_t           *ip)
{
	if (ip->dump)
		dbg_printf ("{");

	if (stp->type.extensible != MUTABLE)
		offset = cdr_generate_struct_cdr (out, offset, stp, ip);
	else if (!ip->key_mode) /* Use user field specfication order. */
		offset = cdr_generate_mutable_unordered (out, offset, stp, ip);
	else			/* Use key fields generation order. */
		offset = cdr_generate_mutable_ordered (out, offset, stp, ip);

	if (ip->dump)
		dbg_printf ("}");

	return (offset);
}

int64_t cdr_union_label (const Type *tp, const void *data)
{
	switch (tp->kind) {
		case DDS_CHAR_8_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_BOOLEAN_TYPE:
		    get_8:
			return (*((unsigned char *) data));

		case DDS_UINT_16_TYPE:
		    get_u16:
		    	return (*((uint16_t *) data));

		case DDS_INT_16_TYPE:
		    	return (*((int16_t *) data));

		case DDS_CHAR_32_TYPE:
		case DDS_UINT_32_TYPE:
		    get_u32:
		    	return (*((uint32_t *) data));

		case DDS_INT_32_TYPE:
		    	return (*((int32_t *) data));

		case DDS_UINT_64_TYPE:
		case DDS_INT_64_TYPE:
		    	return (*((int64_t *) data));

		case DDS_ENUMERATION_TYPE: {
			EnumType *ep = (EnumType *) tp;

			if (ep->bound <= 8)
				goto get_8;
			else if (ep->bound <= 16)
				goto get_u16;
			else /*if (ep->bound <= 32)*/
				goto get_u32;
		}
	}
	return (0);
}

static size_t cdr_generate_union (unsigned char   *out,
				  size_t          offset,
				  const UnionType *utp,
				  GenInfo_t       *ip)
{
	const UnionMember	*mp, *xmp = NULL, *def_mp = NULL;
	const DynDataMember_t	*fp;
	const Type		*tp;
	unsigned		i, l;
	int32_t			label;
	size_t			delta = offset;
	int			plist = utp->type.extensible == MUTABLE &&
					!ip->key_mode;

	/* Insert the union discriminant. */
	mp = &utp->member [0];
	if ((ip->key_mode && !mp->member.is_key) || !ip->src)
		return (offset);

	if (ip->dump)
		dbg_printf ("{");

	fp = (ip->dynamic && ip->sdp) ? ip->sdp->fields : NULL;
	tp = real_type_ptr (utp->type.scope, mp->member.id);
	if (!tp)
		return (0);

	label = (int32_t) cdr_union_label (tp, ip->src);
	delta = cdr_generate_member (out, offset, utp->type.scope,
					    &mp->member, fp, ip, plist, 1, 0);
	if (!delta)
		return (0);

	if (ip->dump)
		dbg_printf (": ");

	/* Find the union member with the correct discriminant. */
	for (i = 1, mp = &utp->member [1]; i < utp->nmembers; i++, mp++) {
		if (mp->is_default)
			def_mp = mp;
		else if (mp->nlabels == 1 && mp->label.value == label) {
			xmp = mp;
			goto found_it;
		}
		else if (mp->nlabels > 1)
			for (l = 0; l < mp->nlabels; l++)
				if (mp->label.list [l] == label) {
					xmp = mp;
					goto found_it;
				}
	}
	if (def_mp)
		xmp = def_mp;
	else
		return (0);

    found_it:

	/* Add the found union member. */
	if (ip->dynamic && ip->sdp)
		fp = ip->sdp->fields + 1;
	else
		fp = NULL;
	delta = cdr_generate_member (out, delta, utp->type.scope,
				     &xmp->member, fp, ip, plist, 0, 0);
	if (!delta)
		return (0);

	if (ip->dump)
		dbg_printf ("}");

	if (plist)
		delta = cdr_generate_eol (out, delta, ip->swap);

	return (delta);
}

static size_t cdr_generate_array (unsigned char   *out,
				  size_t          offset,
				  const ArrayType *atp,
				  GenInfo_t       *ip,
				  int             kf)
{
	const Type		*etp;
	unsigned		i, nelems = atp->bound [0];
	const unsigned char	*prev_src, *sp;
	const DynData_t		*prev_sdp, **dpp = NULL;
	int			prev_dynamic;
	size_t			delta = offset;

	if (ip->key_mode && !kf)
		return (delta);

	for (i = 1; i < atp->nbounds; i++)
		nelems *= atp->bound [i];

	etp = real_type_ptr (atp->collection.type.scope,
			     atp->collection.element_type);
	if (!etp)
		return (0);

	prev_src = ip->src;
	prev_sdp = ip->sdp;
	prev_dynamic = ip->dynamic;
	if (prev_dynamic) {
		if (etp->kind <= DDS_BITSET_TYPE) {
			ip->src = prev_sdp->dp;
			ip->sdp = NULL;
			ip->dynamic = 0;
		}
		else
			dpp = (const DynData_t **) prev_sdp->dp;
	}
	if (ip->dump)
		dbg_printf ("{");

	for (i = 0; i < nelems; i++) {
		if (ip->dump && i)
			dbg_printf (", ");

		sp = ip->src;
		if (ip->dynamic)
			ip->sdp = dpp [i];
		delta = cdr_generate (out, delta, etp, ip, kf, 
				      atp->collection.type.shared);
		if (!delta)
			return (0);

		if (ip->dynamic)
			continue;

		if (atp->collection.type.shared)
			ip->src = sp + sizeof (void *);
		else
			ip->src = sp + atp->collection.element_size;
	}
	ip->src = prev_src;
	ip->sdp = prev_sdp;
	ip->dynamic = prev_dynamic;
	if (ip->dump)
		dbg_printf ("}");

	return (delta);
}

static size_t cdr_generate_seqmap (unsigned char      *out,
				   size_t             offset,
				   const SequenceType *stp,
				   GenInfo_t          *ip,
				   int                kf)
{
	const Type		*etp;
	unsigned		i;
	uint32_t		nelem;
	const unsigned char	*prev_src, *sp;
	const DynData_t		*prev_sdp, **dpp = NULL;
	int			prev_dynamic, do_write = out != NULL;
	size_t			delta = offset;

	etp = real_type_ptr (stp->collection.type.scope,
			     stp->collection.element_type);
	if (!etp)
		return (0);

	prev_src = ip->src;
	prev_sdp = ip->sdp;
	prev_dynamic = ip->dynamic;
	if (prev_dynamic) {
		if (etp->kind <= DDS_BITSET_TYPE) {
			ip->src = prev_sdp->dp;
			ip->sdp = NULL;
			ip->dynamic = 0;
			nelem = (prev_sdp->dsize - prev_sdp->dleft) /
						stp->collection.element_size;
		}
		else {
			dpp = (const DynData_t **) prev_sdp->dp;
			nelem = (prev_sdp->dsize - prev_sdp->dleft) /
							sizeof (DynData_t *);
		}
	}
	else if (ip->src) {
		nelem = ((const DDS_VoidSeq *) ip->src)->_length;
		ip->src = ((const DDS_VoidSeq *) ip->src)->_buffer;
	}
	else
		nelem = 0;

	/* Insert the number of element in the sequence */
	if (ip->key_mode && !kf)
		return (delta);

	CDR_ALIGN (delta, 4, do_write, out);
	if (out) {
		if (ip->swap)
			memcswap32 (&out [delta], &nelem);
		else
			memcpy32 (&out [delta], &nelem);
	}
	else if (ip->dump)
		dbg_printf ("{");

	delta += 4;
	if (!nelem)
		return (delta);

	for (i = 0; i < nelem; i++) {
		if (ip->dump && i)
			dbg_printf (", ");

		sp = ip->src;
		if (ip->dynamic)
			ip->sdp = dpp [i];
		delta = cdr_generate (out, delta, etp, ip, kf,
				      stp->collection.type.shared);
		if (!delta)
			return (0);

		if (ip->dynamic)
			continue;

		if (stp->collection.type.shared)
			ip->src = sp + sizeof (void *);
		else
			ip->src = sp + stp->collection.element_size;
	}
	ip->src = prev_src;
	ip->sdp = prev_sdp;
	ip->dynamic = prev_dynamic;
	if (ip->dump)
		dbg_printf ("}");

	return (delta);
}

static INLINE unsigned strlen_bound (const char *s, size_t bound)
{
	unsigned	i;

	for (i = 0; i < bound; i++)
		if (!*s++)
			break;

	/*printf ("%s -> bound = %u\r\n", s, bound);*/
	return (i);
}

/* cdr_char8 -- Dump a CHAR_8 character. */

static const char *cdr_char8 (char c)
{
	static const char	esc_b [] = "\\\a\b\f\n\r\t\v";
	static const char	esc_c [] = "\\abfnrtv0";
	static char		tbuf [5];
	unsigned		i;

	if (isprint (c)) {
		tbuf [0] = c;
		tbuf [1] = '\0';
	}
	else {
		for (i = 0; i < 9; i++)
			if (c == esc_b [i]) {
				snprintf (tbuf, sizeof (tbuf), "\\%c", esc_c [i]);
				break;
			}
		if (i >= 9)
			snprintf (tbuf, sizeof (tbuf), "\\%o", c);
	}
	return (tbuf);
}

static void cdr_dump_string (const char *data, size_t len, unsigned csize)
{
	const char	*cp;
	const wchar_t	*wcp;
	unsigned	i;

	if (!data) {
		dbg_printf ("NULL");
		return;
	}
	if (csize == 1) {
		cp = data;
		dbg_printf ("\'");
		for (i = 0; i < len - 1; i++) {
			dbg_printf ("%s", cdr_char8 (*cp));
			cp++;
		}
	}
	else {
		wcp = (const wchar_t *) data;
		dbg_printf ("L\'");
		for (i = 0; i < len - 1; i++) {
# if 0
			if (iswprint (*wcp))
				dbg_printf ("'%lc'", (wint_t) *wcp);
			else
# endif
				dbg_printf ("'\\x%u'", (unsigned) *wcp);
			wcp++;
		}
	}
	dbg_printf ("\'");
}

static void dbg_dump_e_b (const Type *tp, int64_t v)
{
	unsigned	i, n, b;
	uint64_t	m;

	if (tp->kind == DDS_ENUMERATION_TYPE) {
		EnumType	*etp = (EnumType *) tp;
		EnumConst	*cp;

		for (i = 0, cp = etp->constant; i < etp->nconsts; i++, cp++)
			if (cp->value == v)
				break;

		if (i >= etp->nconsts)
			dbg_printf ("?%d", (int32_t) v);
		else
			dbg_printf ("%s", str_ptr (cp->name));
	}
	else if (tp->kind == DDS_BITSET_TYPE) {
		BitSetType	*btp = (BitSetType *) tp;
		Bit		*bp;

		dbg_printf ("%s {", str_ptr (btp->type.name));
		n = 0;
		for (b = 0, m = 1; b < 64; b++, m <<= 1)
			if ((v & m) != 0) {
				if (!n++)
					dbg_printf (",");
				for (i = 0, bp = btp->bit; i < btp->nbits; i++, bp++)
					if (bp->index == b)
						break;

				if (i >= btp->nbits)
					dbg_printf ("%u", b);
				else
					dbg_printf ("%s", str_ptr (bp->name));
			}
		dbg_printf ("}");
	}
}

/* cdr_generate -- Generate CDR or PL-CDR from any kind of type in the output
		   buffer. If dynamic is set, then the source data is a dynamic
		   DynData_t structure, or NULL if defaults are to be created.
		   If key is set, only the key fields are marshalled and
		   concatenated in the output buffer.  If msize is set in
		   combination with key, strings are padded to the max. length.
		   If key is not set, all the fields are added to the output
		   buffer. If swap is set, the content of numbers is swapped.*/

static size_t cdr_generate (unsigned char *out,
			    size_t        offset,
			    const Type    *tp,
			    GenInfo_t     *ip,
			    int           kf,
			    int           sf)
{
	size_t			delta = offset;
	int			do_write, prev_dynamic;
	const unsigned char	**ndata, *prev_src;
	const DynData_t		*prev_sdp;
	wchar_t			wc;

	prev_src = ip->src;
	prev_sdp = ip->sdp;
	prev_dynamic = ip->dynamic;
	do_write = (out != NULL);
	if (!prev_dynamic && sf) {
		ndata = (const unsigned char **) ip->src;
		ip->src = (ndata) ? *ndata : NULL;
	}
	switch (tp->kind) {
		case DDS_CHAR_8_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_BOOLEAN_TYPE:

		    out_8:
			if (!ip->key_mode || kf) {
				if (out)
					out [delta] = *ip->src;
				else if (ip->dump) {
					if (tp->kind == DDS_CHAR_8_TYPE)
						dbg_printf ("'%s'", 
						 cdr_char8 (*(char *) ip->src));
					else if (tp->kind == DDS_BYTE_TYPE)
						dbg_printf ("0x%x", *ip->src);
					else if (tp->kind == DDS_BOOLEAN_TYPE) {
						if (*ip->src)
							dbg_printf ("true");
						else
							dbg_printf ("false");
					}
					else if (tp->kind == DDS_BITSET_TYPE)
						dbg_dump_e_b (tp, *ip->src);
				}
				delta++;
			}
			break;

		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:

		    out_16:
			if (!ip->key_mode || kf) {
				CDR_ALIGN (delta, 2, do_write, out);
				if (out) {
					if (ip->swap)
						memcswap16 (&out [delta], ip->src);
					else
						memcpy16 (&out [delta], ip->src);
				}
				else if (ip->dump) {
					if (tp->kind == DDS_INT_16_TYPE)
						dbg_printf ("%d", *((int16_t *) ip->src));
					else if (tp->kind == DDS_UINT_16_TYPE)
						dbg_printf ("%uU", *((uint16_t *) ip->src));
					else
						dbg_dump_e_b (tp, *((int16_t *) ip->src));
				}
				delta += 2;
			}
			break;

		case DDS_CHAR_32_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:

		    out_32:
			if (!ip->key_mode || kf) {
				CDR_ALIGN (delta, 4, do_write, out);
				if (out) {
					if (ip->swap)
						memcswap32 (&out [delta], ip->src);
					else
						memcpy32 (&out [delta], ip->src);
				}
				else if (ip->dump) {
					if (tp->kind == DDS_FLOAT_32_TYPE)
						dbg_printf ("%f", *((float *) ip->src));
					else if (tp->kind == DDS_INT_32_TYPE)
						dbg_printf ("%d", *((int32_t *) ip->src));
					else if (tp->kind == DDS_UINT_32_TYPE)
						dbg_printf ("%uU", *((uint32_t *) ip->src));
					else if (tp->kind == DDS_CHAR_32_TYPE) {
						wc = *((wchar_t *) ip->src);
# if 0
						if (iswprint (wc))
							dbg_printf ("'%lc'", (wint_t) wc);
						else
# endif
							dbg_printf ("'\\x%u'", (unsigned) wc);
					}
					else
						dbg_dump_e_b (tp, *((int32_t *) ip->src));
				}
				delta += 4;
			}
			break;

		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:

		    out_64:
			if (!ip->key_mode || kf) {
				CDR_ALIGN (delta, 8, do_write, out);
				if (out) {
					if (ip->swap)
						memcswap64 (&out [delta], ip->src);
					else
						memcpy64 (&out [delta], ip->src);
				}
				else if (ip->dump) {
					if (tp->kind == DDS_FLOAT_64_TYPE)
						dbg_printf ("%f", *((double *) ip->src));
					else if (tp->kind == DDS_INT_64_TYPE)
						dbg_printf ("%" PRId64, *((int64_t *) ip->src));
					else if (tp->kind == DDS_UINT_64_TYPE)
						dbg_printf ("%" PRIu64 "U", *((uint64_t *) ip->src));
					else
						dbg_dump_e_b (tp, *((int64_t *) ip->src));
				}
				delta += 8;
			}
			break;

		case DDS_FLOAT_128_TYPE:
			if (!ip->key_mode || kf) {
				CDR_ALIGN (delta, 8, do_write, out);
				if (out) {
					if (ip->swap) {
						memcswap64 (&out [delta], ip->src);
						memcswap64 (&out [delta + 8], ((uint64_t *) ip->src + 1));
					}
					else {
						memcpy64 (&out [delta], ip->src);
						memcpy64 (&out [delta + 8], ((uint64_t *) ip->src + 1));
					}
				}
				else if (ip->dump)
					dbg_printf ("%Lf", *((long double *) ip->src));
				delta += 16;
			}
			break;

		case DDS_ENUMERATION_TYPE: {
			EnumType *ep = (EnumType *) tp;

			if (ep->bound <= 16)
				goto out_16;
			else /*if (ep->bound <= 32)*/
				goto out_32;
		}
		case DDS_BITSET_TYPE: {
			BitSetType *bp = (BitSetType *) tp;

			if (bp->bit_bound <= 8)
				goto out_8;
			else if (bp->bit_bound <= 16)
				goto out_16;
			else if (bp->bit_bound <= 32)
				goto out_32;
			else
				goto out_64;
		}
		case DDS_STRING_TYPE:
			if (!ip->key_mode || kf) {

				/* Bounded/unbounded string handling. */
				StringType *sp = (StringType *) tp;
				const unsigned char *ptr;
				uint32_t len;

				if (ip->dynamic) {
					if (!ip->sdp) {
						len = 0;
						ptr = NULL;
					}
					else {
						ptr = ip->sdp->dp;
						if (!ptr)
							len = 0;
						else
							len = prev_sdp->dsize - prev_sdp->dleft - 1;
					}
				}
				else if (ip->src) {
					if (sp->bound) {
						ptr = ip->src;
						len = strlen_bound ((const char *) ptr,
									    sp->bound);
					}
					else {
						ptr = *(const unsigned char **) ip->src;
						if (ptr)
							len = strlen ((const char *) ptr);
						else
							len = 0;
					}
				}
				else {
					ptr = NULL;
					len = 0;
				}
				if (!ip->dynamic || ip->sdp)
					len++;
				CDR_ALIGN (delta, 4, do_write, out);
				if (out) {
					if (ip->swap)
						memcswap32 (&out [delta], &len);
					else
						memcpy32 (&out [delta], &len);
				}
				else if (ip->dump)
					cdr_dump_string ((char *) ptr, len,
						  sp->collection.element_size);
				delta += 4;
				if (ptr) {
					if (out) {
						if (len > 1)
							memcpy (&out [delta],
								ptr, len - 1);
						out [delta + len - 1] = '\0';
					}
					delta += len;
				}
				else if (!ip->dynamic || ip->sdp) {
					if (out)
						out [delta] = '\0';
					delta++;
				}
				if (ip->key_mode &&
				    ip->dformat == CF_CDR_PaddedKey &&
				    sp->bound) {

					/* Pad to max. size in key mode. */
					if (len <= sp->bound) {
						if (out)
							memset (&out [delta], 0,
							   sp->bound - len + 1);
						delta += sp->bound - len + 1;
					}
				}
			}
			break;

		case DDS_STRUCTURE_TYPE:
			if (!ip->key_mode || kf)
				delta = cdr_generate_struct (out, offset,
						    (const StructureType *) tp, ip);
			break;
		case DDS_UNION_TYPE:
			if (!ip->key_mode || kf)
				delta = cdr_generate_union (out, offset,
						    (const UnionType *) tp, ip);
			break;
		case DDS_ARRAY_TYPE:
			if (!ip->key_mode || kf)
				delta = cdr_generate_array (out, offset,
						    (const ArrayType *) tp, ip, kf);
			break;
		case DDS_SEQUENCE_TYPE:
		case DDS_MAP_TYPE:
			if (!ip->key_mode || kf)
				delta = cdr_generate_seqmap (out, offset,
						   (const SequenceType *) tp, ip, kf);
			break;
		default:
			return (0);
	}
	ip->src = prev_src;
	ip->sdp = prev_sdp;
	ip->dynamic = prev_dynamic;
	return (delta);
}

/* cdr_marshalled_size -- CDR-marshalled data size calculation from a native
  		          data sample (data), with the destination data buffer
			  having a given alignment offset (hsize).
			  If key is set: key marshalling mode will be used.
			  If msize is set: pad strings to the maximum length. */

size_t cdr_marshalled_size (size_t           hsize,
			    const void       *data,
			    const Type       *type,
			    int              dynamic,
			    int              key,
			    int              msize,
			    DDS_ReturnCode_t *error)
{
	size_t		length;
	GenInfo_t	info;

	prof_start (cdr_m_size);
	if (!type) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	memset (&info, 0, sizeof (info));
	if (key) {
		if (msize)
			info.dformat = CF_CDR_PaddedKey;
		else
			info.dformat = CF_CDR_Key;
		info.key_mode = 1;
	}
	else
		info.dformat = CF_CDR;
	if ((info.dynamic = dynamic) != 0)
		info.sdp = (const DynData_t *) data;
	else
		info.src = data;
	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_SEQUENCE_TYPE &&
	     type->kind != DDS_ARRAY_TYPE &&
	     type->kind != DDS_UNION_TYPE) ||
	    (length = cdr_generate (NULL, hsize, type, &info, key, 0)) == 0) {
		if (error)
			*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (error)
		*error = DDS_RETCODE_OK;
	prof_stop (cdr_m_size, 1);
	return (length - hsize);
}

/* cdr_marshall -- Marshall a native data sample (data) in CDR format in the
		   given buffer (dest) with the given alignment offset (hsize).
		   If key is set: only key fields are marshalled.
		   If msize && key set: pad strings to the maximum length.
		   If swap set: reverse endianness of the marshalled data. */

size_t cdr_marshall (void             *dest,
		     size_t           hsize,
		     const void       *data,
		     const Type       *type,
		     int              dynamic,
		     int              key,
		     int              msize,
		     int              swap,
		     DDS_ReturnCode_t *error)
{
	size_t		length;
	GenInfo_t	info;

	prof_start (cdr_m);
	if (!dest || (!key && !data) || !type) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE)) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	memset (&info, 0, sizeof (info));
	if (key) {
		if (msize)
			info.dformat = CF_CDR_PaddedKey;
		else
			info.dformat = CF_CDR_Key;
		info.key_mode = 1;
	}
	else
		info.dformat = CF_CDR;
	if ((info.dynamic = dynamic) != 0)
		info.sdp = (const DynData_t *) data;
	else
		info.src = data;
	info.swap = swap;
	length = cdr_generate ((unsigned char *) dest - hsize, hsize,
						type, &info, key, 0) - hsize;
	if (!length) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	CDR_ALIGN (length, 4, 1, (unsigned char *) dest);

	prof_stop (cdr_m, 1);
	*error = DDS_RETCODE_OK;
	return (length);
}

/* cdr_dump_native -- Dump a native data sample (data). If key is given, only
		      key fields will be displayed.  If names is set, then field
		      names will be displayed together with field data. */

DDS_ReturnCode_t cdr_dump_native (unsigned   indent,
				  const void *data,
				  const Type *type,
				  int        dynamic,
				  int        key,
				  int        names)
{
	size_t		length;
	GenInfo_t	info;

	if (!data || !type)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE))
		return (DDS_RETCODE_BAD_PARAMETER);

	memset (&info, 0, sizeof (info));
	if (key)
		info.key_mode = 1;
	if ((info.dynamic = dynamic) != 0)
		info.sdp = (const DynData_t *) data;
	else
		info.src = data;
	info.dump = 1;
	info.indent = indent;
	info.names = names;
	length = cdr_generate (NULL, 0, type, &info, key, 0);
	if (!length)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}


typedef enum {
	PA_GetNative,
	PA_GetCDRKey,
	PA_GetCDRKeyLength,
	PA_GetDynamic,
	PA_GetAuxLength,
	PA_GetOffset,
	PA_DumpData
} ParseAction;

#define	PA_CopyData(a)	((a) <= PA_GetCDRKey || (a) == PA_GetDynamic)

typedef struct parse_info_st {
	ParseAction	action;		/* Parse action. */
	CDRFormat	sformat;	/* Source format. */
	CDRFormat	dformat;	/* Destination format. */
	unsigned char	*dst;		/* Dest. data buffer (Convert). */
	size_t		dofs;		/* Dest. offset (Convert/KeyLength). */
	DynData_t	*dyn_dst;	/* Dynamic dest. context (Convert). */
	unsigned char	*aux_dst;	/* Extra dest. data buffer (Convert).*/
	size_t		aux_ofs;	/* Offset in extra dest. buffer. */
	size_t		dlength;	/* Length (DynLength). */
	unsigned	depth:8;	/* Structure depth (GetOffset). */
	unsigned	field:8;	/* Structure field id (GetOffset). */
	unsigned	indent:8;	/* Indent value while dumping. */
	unsigned	int_swap:1;	/* Swap numbers for interpretation. */
	unsigned	cnv_swap:1;	/* Swap numbers for conversion. */
	unsigned	key_mode:1;	/* Key mode. */
	unsigned	copy_data:1;	/* No references, copy all data. */
	unsigned	names:1;	/* Dump field names. */
	int64_t		*label;		/* Discriminator value while parsing. */
} ParseInfo;

static size_t pid_parse_header (const unsigned char *src,
				size_t              sofs,
				int                 swap,
				unsigned            *id,
				size_t              *length,
				int                 *understand)
{
	uint16_t	s_pid, s_pid_length;
	uint32_t	l_pid, l_pid_length;

	if (swap) {
		memcswap16 (&s_pid, &src [sofs]);
		memcswap16 (&s_pid_length, &src [sofs + 2]);
	}
	else {
		memcpy16 (&s_pid, &src [sofs]);
		memcpy16 (&s_pid_length, &src [sofs + 2]);
	}
	sofs += 4;
	*id = s_pid & XT_PID_ID_MASK;
	*understand = s_pid & XT_PID_UNDERST;
	if (*id < XT_PID_EXTENDED ||
	    *id == XT_PID_LIST_END ||
	    *id == XT_PID_IGNORE) {
		*length = s_pid_length;
		return (sofs);
	}
	else if (*id == XT_PID_EXTENDED) {
		if (s_pid_length != 8)
			return (0);

		if (swap) {
			memcswap32 (&l_pid, &src [sofs]);
			memcswap32 (&l_pid_length, &src [sofs + 4]);
		}
		else {
			memcpy32 (&l_pid, &src [sofs]);
			memcpy32 (&l_pid_length, &src [sofs + 4]);
		}
		*id = l_pid & 0xfffffff;
		*length = l_pid_length;
		return (sofs + 8);
	}
	else
		return (0);
}

static size_t cdr_parse (const unsigned char *src,
			 size_t              sofs,
			 const Type          *tp,
			 ParseInfo           *ip,
			 int                 key);

static size_t cdr_parse_string (const unsigned char *src,
			        size_t              sofs,
				const StringType    *stp,
				ParseInfo           *ip,
				int                 key)
{
	unsigned char	*dcp;
	DynData_t	*p;
	uint32_t	l;
	size_t		s;

	if (ip->sformat == CF_CDR || key) {
		sofs = ALIGN (sofs, 4);

		/* Calculate the string length. */
		if (ip->int_swap)
			memcswap32 (&l, &src [sofs]);
		else
			memcpy32 (&l, &src [sofs]);
		/*if (!l)		<== Allow 0 lengths!
			return (0); */

		/* Point source to string data. */
		sofs += 4;
	}
	else
		return (sofs);

	if (ip->key_mode && !key) {
		if (ip->sformat == CF_CDR)
			sofs += l;
		return (sofs);
	}
	if (ip->action == PA_GetCDRKey && key) {

		/* Align dofs for str. length.*/
		CDR_ALIGN (ip->dofs, 4, 1, ip->dst);
		if (ip->int_swap != ip->cnv_swap)
			memcswap32 (&ip->dst [ip->dofs], &l);
		else
			memcpy32 (&ip->dst [ip->dofs], &l);
		ip->dofs += 4;
	}
	if (!PA_CopyData (ip->action)) { /* Length calculation. */
		if (ip->action == PA_DumpData)
			cdr_dump_string ((l) ? (char *) src + sofs : NULL,
					 l, stp->collection.element_size);
		else if (ip->action == PA_GetAuxLength && !stp->bound)

			/* Unbounded string: increase total length
			   with unbounded string length. */
			ip->dlength += l;
		else if (CF_Key (ip->dformat)) {
			ip->dofs = ALIGN (ip->dofs, 4) + 4;
			if (stp->bound &&
			    ip->dformat == CF_CDR_PaddedKey)
			 	ip->dofs += stp->bound + 1;
			else
				ip->dofs += l;
		}
	}
	else if (ip->action == PA_GetDynamic) {	/* Dynamic data copy. */
		if (!l)
			ip->dyn_dst = NULL;
		else {
			s = (ip->copy_data) ? l : 0;
			/*dbg_printf ("{R:str(%lu)}\r\n", DYN_DATA_SIZE + l);*/
			ip->dyn_dst = p = xd_dyn_data_alloc (
						&stp->collection.type, s);
			if (!ip->dyn_dst)
				return (0);

			p->nrefs = 1;
			if (ip->copy_data) {
				memcpy (p->dp, (char *) &src [sofs], l);
				p->dleft -= l;
			}
			else {
				p->flags |= DDF_FOREIGN;
				p->dp = (unsigned char *) &src [sofs];
				p->dsize = l;
				p->dleft = 0;
			}
			dm_printf ("cdr_parse_string(dd=%p);\r\n", p);
		}
	}
	else if (stp->bound) {	/* Native/CDR: bounded string copy. */
		if (l > stp->bound + 1U)
			return (0);

		if (l > 1)
			memcpy (&ip->dst [ip->dofs], &src [sofs], l - 1);

		if (CF_Key (ip->dformat)) { /* Key? */
			if (ip->dformat == CF_CDR_PaddedKey &&
			    stp->bound + 1 > l - 1U) {
				memset (&ip->dst [ip->dofs + l - 1],
				        0, 
					stp->bound + 2 - l);
				ip->dofs += stp->bound + 1;
			}
			else {
				ip->dst [ip->dofs + l - 1] = '\0';
				ip->dofs += l;
			}
		}

		/* Zero out the remainder of the string if we are not 
		   working on a minimal size key (if there is a 
		   remainder). */
		else if (l <= stp->bound + 1)
			memset (&ip->dst [ip->dofs + l - 1], 0,	stp->bound + 2 - l); 

		if (ip->sformat == CF_CDR_PaddedKey)
			sofs += stp->bound - l;
	}
	else {	/* Native/CDR: unbounded string copy to destination buffer. */
		if (CF_Key (ip->dformat)) { /* Key? */
			if (l)
				memcpy (&ip->dst [ip->dofs], &src [sofs], l);
			ip->dofs += l;
		}
		else if (CF_Key (ip->sformat)) {

			/* Key data requested. */
			if (l) {
				if ((dcp = mm_fcts.alloc_ (l)) == NULL)
					return (0);
			}
			else
				dcp = NULL;
			memcpy (&ip->dst [ip->dofs], &dcp, sizeof (char *));
			if (l)
				memcpy (dcp, &src [sofs], l);
		}
		else {
			/* Copy to extra buffer space. */
			if (l) {
				dcp = ip->aux_dst + ip->aux_ofs;
				memcpy (dcp, &src [sofs], l);
			}
			else
				dcp = NULL;

			/* Set the char * to the correct value. */
			memcpy (&ip->dst [ip->dofs], &dcp, sizeof (char *));
			ip->aux_ofs += l;
		}
	}
	if (ip->sformat == CF_CDR || key)
		sofs += l;
	return (sofs);
}

static size_t cdr_field_ofs (const Type *tp, size_t ofs, size_t *esize)
{
	size_t	nofs;

	switch (tp->kind) {
		case DDS_BOOLEAN_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_CHAR_8_TYPE:
		    do_8:
			*esize = 1;
			return (ofs);

		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:
		    do_16:
			*esize = 2;
			return ((ofs + 1) & ~1);

		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_CHAR_32_TYPE:
		case DDS_FLOAT_32_TYPE:
		    do_32:
			*esize = 4;
			return ((ofs + 3) & ~3);

		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:
		    do_64:
			*esize = 8;
			return ((ofs + 7) & ~7);

		case DDS_FLOAT_128_TYPE:
			*esize = 16;
			return ((ofs + 15) & ~15);

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
		case DDS_ARRAY_TYPE: {
			const ArrayType	*atp = (const ArrayType *) tp;
			size_t		s = atp->collection.element_size;
			unsigned	i;

			if (s == 1)
				nofs = ofs;
			else if (s == 2)
				nofs = (ofs + 1) & ~1;
			else if (s == 4)
				nofs = (ofs + 3) & ~3;
			else
				nofs = (ofs + sizeof (void *) - 1) & ~(sizeof (void *) - 1);
			for (i = 0; i < atp->nbounds; i++)
				s *= atp->bound [i];
			*esize = s;
			return (nofs);
		}
		case DDS_SEQUENCE_TYPE:
		case DDS_MAP_TYPE:
			nofs = (ofs + sizeof (void *) - 1) & ~(sizeof (void *) - 1);
			*esize = sizeof (DDS_VoidSeq);
			return (nofs);

		case DDS_STRING_TYPE: {
			const StringType *sp = (const StringType *) tp;

			*esize = sp->bound;
			return (ofs);
		}
		case DDS_UNION_TYPE: {
			const UnionType	*utp = (const UnionType *) tp;

			nofs = (ofs + sizeof (void *) - 1) & ~(sizeof (void *) - 1);
			*esize = utp->size;
			return (nofs);
		}
		case DDS_STRUCTURE_TYPE: {
			const StructureType *stp = (const StructureType *) tp;

			nofs = (ofs + sizeof (void *) - 1) & ~(sizeof (void *) - 1);
			*esize = stp->size;
			return (nofs);
		}
		default:
			return (0);
	}
}

static size_t cdr_parse_member (const unsigned char *src, 
				size_t              sofs,
				unsigned            scope,
				const Member        *mp,
				ParseInfo           *ip,
				size_t              plen,
				int                 mutable,
				int                 key)
{
	const Type	*tp;
	ParseAction	action = ip->action;
	unsigned char	*prev_dst = NULL, **pp = NULL;
	DynData_t	*dp = NULL, *ndp;
	DynDataMember_t	*fp = NULL;
	size_t		prev_dofs = 0, n, nofs, esize;

	tp = real_type_ptr (scope, mp->id);
	if (!tp)
		return (0);

	if (action == PA_GetNative) {
		prev_dst = ip->dst;
		prev_dofs = ip->dofs;
		if (mp->is_optional || mp->is_shareable) {
			pp = (unsigned char **) &ip->dst [ip->dofs + mp->offset];
			if (mp->is_optional && !plen) {
				*pp = NULL;
				return (sofs);
			}
			if (CF_Key (ip->sformat) && !*pp)
				return (0);

			else if (!CF_Key (ip->sformat)) {
				ip->aux_ofs = cdr_field_ofs (tp, ip->aux_ofs, &esize);
				*pp = ip->aux_dst + ip->aux_ofs;
				ip->aux_ofs += esize;
			}
			ip->dst = *pp;
		}
		else
			ip->dst += ip->dofs + mp->offset;
		ip->dofs = 0;
	}
	else if (action == PA_GetDynamic) {
		dp = ip->dyn_dst;
		fp = &dp->fields [dp->nfields];
		if (xt_simple_type (tp->kind)) {
			fp->length = (tp->kind <= DDS_CHAR_32_TYPE) ?
					xt_kind_size [tp->kind] :
					xt_simple_size (tp);
			fp->flags = 0;
		}
		else {
			fp->length = sizeof (DynData_t *);
			fp->flags = DMF_DYNAMIC;
		}
		nofs = dp->dsize - dp->dleft;
		nofs = ALIGN (nofs, fp->length);
		ip->dofs = fp->offset = nofs;
		if (nofs + fp->length > dp->dsize) {
			/*dbg_printf ("{R:struct_data%c(%u)}\r\n", (dp->dsize) ? '+' : '=', dp->dsize + DYN_DATA_INC);*/
			ndp = xd_dyn_data_grow (dp, dp->dsize + DYN_DATA_INC);
			if (!ndp)
				return (0);

			dp = ip->dyn_dst = ndp;
			fp = &dp->fields [dp->nfields];
		}
		dp->dleft = dp->dsize - nofs;
		ip->dst = dp->dp;
		if (dp->type->kind == DDS_STRUCTURE_TYPE)
			fp->index = mp - ((const StructureType *) dp->type)->member;
		else
			fp->index = (UnionMember *) mp - 
					     ((const UnionType *) dp->type)->member;
	}
	else if (action == PA_DumpData && (!ip->key_mode || key)) {
		if (ip->names)
			dbg_printf ("%s=", str_ptr (mp->name));
		else if (mutable)
			dbg_printf (".%s=", str_ptr (mp->name));
	}
	else if (action == PA_GetAuxLength &&
		 (mp->is_optional || mp->is_shareable) &&
		 plen) {
		ip->dlength = cdr_field_ofs (tp, ip->dlength, &esize);
		ip->dlength += esize;
	}
	n = cdr_parse (src, sofs, tp, ip, key);
	if (!n)
		return (0);

	if (action == PA_GetNative) {
		ip->dst = prev_dst;
		ip->dofs = prev_dofs;
	}
	else if (action == PA_GetDynamic) {
		if (!xt_simple_type (tp->kind))
			memcpy (dp->dp + fp->offset, &ip->dyn_dst, 
							sizeof (DynData_t *));
		dp->dleft -= fp->length;
		fp->flags |= DMF_PRESENT;
		dp->nfields++;
		ip->dyn_dst = dp;
	}
	if (plen) {
		sofs += plen;
		if (sofs < n)
			return (0);
	}
	else
		sofs = n;
	return (sofs);
}

static size_t cdr_parse_cdr_unordered (const unsigned char *src,
				       size_t              sofs,
				       const StructureType *stp,
				       ParseInfo           *ip,
				       int                 key)
{
	const Member		*mp;
	unsigned		i, id, nf = 0;
	size_t			plen, n;
	int			understand, key_member;

	for (i = 0, mp = stp->member;
	     i < stp->nmembers;
	     i++, mp++) {

		key_member = key && (mp->is_key || !stp->keyed);
		if (CF_Key (ip->sformat) && !key_member)
			continue;

		if (mp->is_optional) { /* Parse Parameter header. */
			n = pid_parse_header (src, sofs, ip->int_swap, 
					      &id, &plen, &understand);
			if (!n || id != mp->member_id)
				return (0);

			sofs = n;
		}
		else
			plen = 0;

		if (ip->action == PA_DumpData &&
		    (!ip->key_mode || key_member) &&
		    nf++)
			dbg_printf (", ");
		else if (ip->action == PA_GetOffset &&
		         ip->field == i)
			return (sofs);

		sofs = cdr_parse_member (src, sofs, stp->type.scope, 
				      mp, ip, plen, 0, key_member);
		if (!sofs)
			break;
	}
	return (sofs);
}

typedef struct mo_fdesc_st {
#if defined (BIGDATA) || (WORDSIZE == 64)
	unsigned	dofs;
	unsigned	length;
#else
	unsigned short	dofs;
	unsigned short	length;
#endif
	unsigned	id: 28;
	unsigned	found: 1;
	unsigned	index;
} MOFDesc_st;

static int fdcmp (const void *a1, const void *a2)
{
	MOFDesc_st	*fp1 = (MOFDesc_st *) a1,
			*fp2 = (MOFDesc_st *) a2;

	return ((int) fp1->id - (int) fp2->id);
}

static size_t cdr_parse_cdr_ordered (const unsigned char *src,
				     size_t              sofs,
				     const StructureType *stp,
				     ParseInfo           *ip,
				     int                 key)
{
	const Member		*mp;
	unsigned		i, prev_id, nkf;
	int			sort;
	MOFDesc_st		*fds, *fp;
	MOFDesc_st		mfds [MAX_MKFDS];

	/* 1. Count # of key fields and check if sorting is needed. */
	sort = 0;
	prev_id = 0;
	nkf = 0;
	if (key)
		for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++)
			if (mp->is_key || !stp->keyed) {
				nkf++;
				if (mp->member_id < prev_id)
					sort = 1;
				prev_id = mp->member_id;
			}

	if (nkf <= 1 || !sort)
		return (cdr_parse_cdr_unordered (src, sofs, stp, ip, key));

	/* 2. Point fds to an array large enough to contain all key field
	      descriptors, and initialize it properly. */
	if (nkf > MAX_MKFDS) {
		fds = xmalloc (nkf * sizeof (MOFDesc_st));
		if (!fds) {
			sofs = 0;
			return (sofs);
		}
	}
	else
		fds = mfds;
	memset (fds, 0, nkf * sizeof (MOFDesc_st));
	for (i = 0, mp = stp->member, fp = fds; i < stp->nmembers; i++, mp++)
		if (mp->is_key || !stp->keyed) {
			fp->id = mp->member_id;
			fp->index = i;
			fp++;
		}

	/* 3. Sort the key fields on member id. */
	if (sort)
		qsort (fds, nkf, sizeof (MOFDesc_st), fdcmp);

	/* 5. Parse each field in sorted key field order. */
	for (i = 0, fp = fds; i < nkf; i++, fp++) {
		if (ip->action == PA_DumpData && i)
			dbg_printf (", ");

		mp = &stp->member [fp->index];
		sofs = cdr_parse_member (src, sofs, stp->type.scope, 
						      mp, ip, 0, 0, 1);
		if (!sofs)
			break;
	}
	if (nkf > MAX_MKFDS) {
	    xfree (fds);
	}
	return (sofs);
}

static size_t cdr_parse_mutable_unordered (const unsigned char *src,
					   size_t              sofs,
					   const StructureType *stp,
					   ParseInfo           *ip,
					   int                 key)
{
	const Member		*mp;
	unsigned char		**pp;
	unsigned		i, id, nf = 0;
	size_t			plen, n;
	int			understand, key_field;

	/* Clear optional fields. */
	if (stp->optional && ip->action == PA_GetNative)
		for (i = 0, mp = stp->member;
		     i < stp->nmembers;
		     i++, mp++)
			if (mp->is_optional) {
				pp = (unsigned char **) &ip->dst [ip->dofs + mp->offset];
				*pp = NULL;
			}

	/* Parse all fields. */
	for (;;) {
		n = pid_parse_header (src, sofs, ip->int_swap,
					    &id, &plen, &understand);
		if (!n)
			return (0);

		sofs = n;
		if (id == XT_PID_LIST_END)
			return (sofs);

		for (i = 0, mp = stp->member;
		     i < stp->nmembers;
		     i++, mp++) {
			if (mp->member_id != id)
				continue;

			key_field = key && (mp->is_key || !stp->keyed);
			if (ip->action == PA_DumpData &&
			    (!ip->key_mode || key_field) && 
			    nf++)
				dbg_printf (", ");
			else if (ip->action == PA_GetOffset &&
				 ip->field == i)
				return (sofs);

			n = cdr_parse_member (src, sofs,
					      stp->type.scope, mp, ip,
					      plen, 1, key_field);
			if (!n)
				return (0);

			sofs = n;
			break;
		}
		if (i == stp->nmembers) {
			if (understand)
				return (0); /* Not found and must understand! */
			else
				sofs += plen; /* skip to next header */
                }
	}
	return (sofs);
}

static size_t cdr_parse_mutable_ordered (const unsigned char *src, 
					 size_t              sofs,
					 const StructureType *stp,
					 ParseInfo           *ip,
					 int                 key)
{
	const Member		*mp;
	unsigned		i, j, id, prev_id, nff, nkf;
	size_t			plen, n;
	int			understand, sort;
	MOFDesc_st		*fds, *fp;
	MOFDesc_st		mfds [MAX_MKFDS];

	/* 1. Count # of key fields and point fds to an array large enough
	      to contain all key field descriptors. */
	sort = 0;
	prev_id = 0;
	nkf = 0;
	if (key)
		for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++)
			if (mp->is_key || !stp->keyed) {
				nkf++;
				if (mp->member_id < prev_id)
					sort = 1;
				prev_id = mp->member_id;
			}

	if (nkf <= 1)
		return (cdr_parse_mutable_unordered (src, sofs, stp, ip, key));

	if (nkf > MAX_MKFDS) {
		fds = xmalloc (nkf * sizeof (MOFDesc_st));
		if (!fds) {
			sofs = 0;
			return (sofs);
		}
	}
	else
		fds = mfds;

	/* 2. Initialize all field descriptors. */
	memset (fds, 0, nkf * sizeof (MOFDesc_st));
	fp = fds;
	for (i = 0, mp = stp->member; i < stp->nmembers; i++, mp++)
		if (mp->is_key || !stp->keyed) {
			fp->id = mp->member_id;
			fp->index = i;
			fp++;
		}

	/* 3. Parse headers only, completing each field descriptor. */
	for (nff = 0;;) {
		n = pid_parse_header (src, sofs, ip->int_swap,
					    &id, &plen, &understand);
		if (!n) {
			sofs = 0;
			goto cleanup;
		}
		sofs = n;
		if (id == XT_PID_LIST_END)
			break;

		for (i = 0, mp = stp->member;
		     i < stp->nmembers;
		     i++, mp++) {
			if ((!mp->is_key && stp->keyed) ||
			    mp->member_id != id)
				continue;

			for (j = 0, fp = fds; j < nkf; j++, fp++)
				if (fp->id == id)
					break;

			if (j >= nkf || fp->found) {

				/* !Found or Duplicate! */
				sofs = 0;
				goto cleanup;
			}
			fp->dofs = sofs;
			fp->length = plen;
			fp->found = 1;
			nff++;
		}
		if (i == stp->nmembers && understand) {

			/* Not found and must understand! */
			sofs = 0;
			goto cleanup;
		}
		sofs += plen;
	}
	if (nff != nkf) {	/* Some key fields missing! */
		sofs = 0;
		goto cleanup;
	}

	/* 4. Sort the key fields on member id. */
	if (sort)
		qsort (fds, nkf, sizeof (MOFDesc_st), fdcmp);

	/* 5. Parse each field in sorted key field order. */
	for (i = 0, fp = fds; i < nkf; i++, fp++) {
		if (ip->action == PA_DumpData && i)
			dbg_printf (", ");

		mp = &stp->member [fp->index];
		n = cdr_parse_member (src, fp->dofs,
				      stp->type.scope, mp, ip,
				      fp->length, 1, 1);
		if (!n) {
			sofs = 0;
			goto cleanup;
		}
	}

    cleanup:
    	if (nkf > MAX_MKFDS)
		xfree (fds);
	return (sofs);
}

static size_t cdr_parse_struct (const unsigned char *src, 
				size_t              sofs,
				const StructureType *stp,
				ParseInfo           *ip,
				int                 key)
{
	DynData_t		*p;
	size_t			n;

	if (ip->action == PA_GetDynamic) {
		n = DYN_EXTRA_AGG_SIZE (stp->nmembers);
		/*dbg_printf ("{R:new_struct(%lu)}\r\n", n + DYN_DATA_SIZE + DYN_DATA_INC);*/
		ip->dyn_dst = p = xd_dyn_data_alloc (&stp->type, n + DYN_DATA_INC);
		if (!p)
			return (0);

		p->nfields = 0;
		memset (p->fields, 0, sizeof (DynDataMember_t) * stp->nmembers);
		p->dsize -= n;
		p->dleft -= n;
		p->dp += n;
		dm_printf ("cdr_parse_struct(dd=%p);\r\n", p);
	}
	else if (ip->action == PA_DumpData)
		dbg_printf ("{");

	if (stp->type.extensible != MUTABLE)
		sofs = cdr_parse_cdr_unordered (src, sofs, stp, ip, key);
	else if (CF_Key (ip->sformat))
		sofs = cdr_parse_cdr_ordered (src, sofs, stp, ip, key);
	else if (!CF_Key (ip->dformat))
		sofs = cdr_parse_mutable_unordered (src, sofs, stp, ip, key);
	else
		sofs = cdr_parse_mutable_ordered (src, sofs, stp, ip, key);

	if (ip->action == PA_DumpData)
		dbg_printf ("}");
	return (sofs);
}

#define	valid_discriminant(t)	(((t) >= DDS_BOOLEAN_TYPE && 	\
				  (t) <= DDS_UINT_64_TYPE) ||	\
				 ((t) >= DDS_CHAR_8_TYPE && 	\
				  (t) <= DDS_ENUMERATION_TYPE))

static size_t cdr_parse_union (const unsigned char *src,
			       size_t              sofs,
			       const UnionType     *utp,
			       ParseInfo           *ip,
			       int                 key)
{
	const Type		*tp;
	const UnionMember	*mp, *def_mp;
	void			**pp;
	unsigned		i, j, id;
	int			got_disc = 0, got_id = 0, understand;
	size_t			plen, n;
	int64_t			label;
	DynData_t		*p;

	if (ip->action == PA_GetDynamic) {
		n = DYN_EXTRA_AGG_SIZE (2);
		/*dbg_printf ("{R:new_union(%lu)}\r\n", DYN_DATA_SIZE + n + DYN_DATA_INC);*/
		ip->dyn_dst = p = xd_dyn_data_alloc (&utp->type, n + DYN_DATA_INC);
		if (!p)
			return (0);

		p->nfields = 0;
		memset (p->fields, 0, sizeof (DynDataMember_t) * 2);
		p->dsize -= n;
		p->dleft -= n;
		p->dp += n;
		dm_printf ("cdr_parse_union(dd=%p);\r\n", p);
	}
	else if (ip->action == PA_DumpData)
		dbg_printf ("{");
	if (ip->sformat == CF_CDR && utp->type.extensible == MUTABLE)
		for (;;) {
			n = pid_parse_header (src, sofs, ip->int_swap,
						       &id, &plen, &understand);
			if (!n)
				return (0);

			sofs = n;
			if (id == XT_PID_LIST_END)
				return (sofs);

			for (i = 0, mp = utp->member;
			     i < utp->nmembers;
			     i++, mp++) {
				if (mp->member.member_id != id)
					continue;

				if (!got_disc) {
					if (i)
						return (0);

					got_disc = 1;
					tp = real_type_ptr (utp->type.scope,
							    mp->member.id);

					if (!tp || !valid_discriminant (tp->kind))
						return (0);

					ip->label = &label;
					n = cdr_parse (src, sofs, tp, ip,
						       key && mp->member.is_key);
					if (!n)
						return (0);

					if (ip->action == PA_DumpData)
						dbg_printf (": ");

					ip->label = NULL;
					break;
				}
				else if (got_id)
					return (0);
				else
					got_id = 1;

				if (!plen) {
					if (ip->action == PA_GetNative) {
						pp = (void **) &ip->dst [ip->dofs];
						*pp = NULL;
					}
					break;
				}
				if (!mp->is_default) {
					if (mp->nlabels == 1) {
						if (mp->label.value != label)
							return (0); /* Unknown label! */
					}
					else {
						for (j = 0; j < mp->nlabels; j++)
							if (mp->label.list [j] == label)
								break;

						if (j == mp->nlabels)
							return (0); /* Unknown label! */
					}
				}
				n = cdr_parse_member (src, sofs,
				     		      utp->type.scope, &mp->member,
						      ip, plen, 1, key && mp->member.is_key);
				if (!n)
					return (0);

				break;
			}
			if (i == utp->nmembers && understand)
				return (0); /* Not found and must understand! */
		}
	else {
		/* Parse the discriminant. */
		mp = &utp->member [0];
		tp = real_type_ptr (utp->type.scope, mp->member.id);
		if (!tp || !valid_discriminant (tp->kind))
			return (0);

		ip->label = &label;
		n = cdr_parse (src, sofs, tp, ip, 
			       key && utp->member [0].member.is_key);
		if (!n)
			return (0);

		if (ip->action == PA_DumpData)
			dbg_printf (": ");
		sofs = n;

		/* Lookup label in list of union labels and select member. */
		def_mp = NULL;
		for (i = 0, mp = utp->member;
		     i < utp->nmembers;
		     i++, mp++) {
			if (mp->is_default) {
				def_mp = mp;
				continue;
			}
			else if (mp->nlabels == 1 &&
				 mp->label.value == label)
				break;

			else if (mp->nlabels > 1) {
				for (j = 0; j < mp->nlabels; j++)
					if (mp->label.list [j] == label)
						goto found_it;

				if (j == mp->nlabels)
					return (0);
			}
		}
		if (i == utp->nmembers) {
			if (!def_mp)
				return (0);

			mp = def_mp;
		}

	    found_it:

		/* We found the correct union member -- parse its type. */
		if (mp->member.is_optional) { /* Parse Parameter header. */
			n = pid_parse_header (src, sofs, ip->int_swap, &id,
						     &plen, &understand);
			if (!n || id != mp->member.member_id)
				return (0);

			sofs = n;
		}
		else
			plen = 0;

		sofs = cdr_parse_member (src, sofs, utp->type.scope,
					 &mp->member, ip, plen, 0,
					 key && mp->member.is_key);
		if (!sofs)
			return (0);
	}
	if (ip->action == PA_DumpData)
		dbg_printf ("}");
	return (sofs);
}

static size_t cdr_parse_array (const unsigned char *src,
			       size_t              sofs,
			       const ArrayType     *atp,
			       ParseInfo           *ip,
			       int                 key)
{
	unsigned	i, n, s, ts, nelems = atp->bound [0];
	Type		*tp;
	DynData_t	*p;
	size_t		dofs;

	for (i = 1; i < atp->nbounds; i++)
		nelems *= atp->bound [i];

	tp = real_type_ptr (atp->collection.type.scope,
			    atp->collection.element_type);
	if (!tp)
		return (0);

	if (ip->action == PA_GetDynamic) {
		if (xt_simple_type (tp->kind))
			s = atp->collection.element_size;
		else
			s = sizeof (DynData_t *);
		ts = s * nelems;
		/*dbg_printf ("{R:new_array(%lu)}\r\n", DYN_DATA_SIZE + ts);*/
		ip->dyn_dst = p = xd_dyn_data_alloc (&atp->collection.type, ts);
		if (!p)
			return (0);

		p->dleft -= ts;
		ip->dst = p->dp;
		ip->dofs = 0;

		dm_printf ("cdr_parse_array(dd=%p);\r\n", p);
	}
	else {
		s = atp->collection.element_size;
		p = NULL;
	}
	if (ip->action == PA_DumpData)
		dbg_printf ("{");
	for (i = 0, dofs = ip->dofs; i < nelems; i++, dofs += s) {
		if (ip->action == PA_DumpData && i)
			dbg_printf (", ");
		ip->dofs = dofs;
		n = cdr_parse (src, sofs, tp, ip, key);
		if (!n)
			return (0);

		if (ip->action == PA_GetDynamic &&
		    !xt_simple_type (tp->kind))
			memcpy (p->dp + dofs, &ip->dyn_dst, sizeof (DynData_t *));
		sofs = n;
	}
	if (ip->action == PA_GetDynamic)
		ip->dyn_dst = p;
	else if (ip->action == PA_DumpData)
		dbg_printf ("}");
	return (sofs);
}

static size_t cdr_parse_seqmap (const unsigned char *src,
			        size_t              sofs,
			        const SequenceType  *stp,
			        ParseInfo           *ip,
				int                 key)
{
	DDS_VoidSeq	*seq = NULL;
	Type		*tp;
	DynData_t	*p;
	unsigned char	*dp;
	uint32_t	nelem;
	unsigned	i, n, dofs, s, ts;
	size_t		esize;
	
	tp = real_type_ptr (stp->collection.type.scope,
			    stp->collection.element_type);
	if (!tp)
		return (0);

	sofs = (sofs + 3) & ~3;
	if (ip->int_swap)
		memcswap32 (&nelem, &src [sofs]);
	else
		memcpy32 (&nelem, &src [sofs]);
	sofs += 4;
	if (stp->bound && nelem > stp->bound)
		return (0);

	if (!PA_CopyData (ip->action)) {
		s = stp->collection.element_size;
		if (ip->action == PA_DumpData)
			dbg_printf ("{");
		else if (ip->action == PA_GetAuxLength) {
			ip->dlength = cdr_field_ofs (tp, ip->dlength, &esize);
			ip->dlength += nelem * s;
		}
		else if (CF_Key (ip->dformat) && (!ip->key_mode || key))
			ip->dofs = ALIGN (ip->dofs, 4) + 4;
		p = NULL;
	}
	else if (ip->action == PA_GetDynamic) {
		if (xt_simple_type (tp->kind))
			s = stp->collection.element_size;
		else
			s = sizeof (DynData_t *);
		ts = s * nelem;
		/*dbg_printf ("{R:new_seq(%lu)}\r\n", DYN_DATA_SIZE + ts);*/
		p = xd_dyn_data_alloc (&stp->collection.type, ts);
		if (!p)
			return (0);

		p->dleft -= ts;
		ip->dst = p->dp;
		ip->dofs = 0;

		dm_printf ("cdr_parse_sequence(dd=%p);\r\n", p);
	}
	else {
		s = stp->collection.element_size;
		if (!ip->key_mode || key) {
			if (CF_Key (ip->dformat)) { /* Key? */

				/* Align dofs for str. length.*/
				CDR_ALIGN (ip->dofs, 4, 1, ip->dst);
				if (ip->int_swap != ip->cnv_swap)
					memcswap32 (&ip->dst [ip->dofs], &nelem);
				else
					memcpy32 (&ip->dst [ip->dofs], &nelem);
				ip->dofs += 4;
			}
			else {
				seq = (DDS_VoidSeq *) (ip->dst + ip->dofs);
				if (CF_Key (ip->sformat)) {

					/* Key data requested. */
					if (nelem &&
					    (dp = mm_fcts.alloc_ (nelem * 
					       stp->collection.element_size)) == NULL)
						return (0);
					else
						dp = NULL;
					memcpy (&seq->_buffer, &dp, sizeof (unsigned char *));
				}
				else {
					ip->aux_ofs = cdr_field_ofs (tp, ip->aux_ofs, &esize);
					seq->_buffer = ip->aux_dst + ip->aux_ofs;
					ip->aux_ofs += nelem * stp->collection.element_size;
				}
				seq->_length = seq->_maximum = nelem;
				seq->_esize = stp->collection.element_size;
				seq->_own = 1;
			}
		}
		p = NULL;
	}
	for (i = 0, dofs = 0; i < nelem; i++, dofs += s) {
		if (ip->action <= PA_GetCDRKey && (!ip->key_mode || key)) {
			if (!CF_Key (ip->dformat)) { /* Key? */
				ip->dst = seq->_buffer;
				ip->dofs = dofs;
			}
		}
		else if (ip->action == PA_DumpData) {
			if (i)
				dbg_printf (", ");
		}
		n = cdr_parse (src, sofs, tp, ip, key);
		if (!n)
			return (0);

		if (ip->action == PA_GetDynamic) {
			if (xt_simple_type (tp->kind))
				ip->dofs += s;
			else
				memcpy (p->dp + dofs, &ip->dyn_dst, sizeof (DynData_t *));
		}
		sofs = n;
	}
	if (ip->action == PA_GetDynamic)
		ip->dyn_dst = p;
	if (ip->action == PA_DumpData)
		dbg_printf ("}");
	return (sofs);
}

static size_t cdr_parse (const unsigned char *src,
			 size_t              sofs,
			 const Type          *tp,
			 ParseInfo           *ip,
			 int                 key)
{
	uint16_t	s;
	uint32_t	l;
	uint64_t	ll;
	float		f;
	double		d;
	long double	ld;

	switch (tp->kind) {
		case DDS_CHAR_8_TYPE:
		case DDS_BYTE_TYPE:
		case DDS_BOOLEAN_TYPE:

		    get_8:
			if (!ip->key_mode || key) {
				if (PA_CopyData (ip->action))
					ip->dst [ip->dofs] = src [sofs];
				else if (ip->action == PA_DumpData) {
					if (tp->kind == DDS_CHAR_8_TYPE)
						dbg_printf ("'%s'", 
						 cdr_char8 ((char) src [sofs]));
					else if (tp->kind == DDS_BYTE_TYPE)
						dbg_printf ("0x%x", src [sofs]);
					else if (tp->kind == DDS_BOOLEAN_TYPE) {
						if (src [sofs])
							dbg_printf ("true");
						else
							dbg_printf ("false");
					}
					else if (tp->kind == DDS_BITSET_TYPE)
						dbg_dump_e_b (tp, src [sofs]);
				}
				if (CF_Key (ip->dformat))
					ip->dofs++;
				if (ip->label) {
					*ip->label = src [sofs];
					ip->label = NULL;
				}
			}
			if (ip->sformat == CF_CDR || key)
				sofs++;
			break;

		case DDS_INT_16_TYPE:
		case DDS_UINT_16_TYPE:

		    get_16:
			if (ip->sformat == CF_CDR || key)
				sofs = ALIGN (sofs, 2);

			if (!ip->key_mode || key) {
				if (ip->action == PA_GetCDRKeyLength)
					ip->dofs = ALIGN (ip->dofs, 2) + 2;
				else if (PA_CopyData (ip->action)) {
					if (CF_Key (ip->dformat))
						CDR_ALIGN (ip->dofs, 2, 1, ip->dst);
					if (ip->cnv_swap)
						memcswap16 (&ip->dst [ip->dofs], &src [sofs]);
					else
						memcpy16 (&ip->dst [ip->dofs], &src [sofs]);
					if (CF_Key (ip->dformat))
						ip->dofs += 2;
				}
				if (ip->label || ip->action == PA_DumpData) {
					if (ip->int_swap)
						memcswap16 (&s, &src [sofs]);
					else
						memcpy16 (&s, &src [sofs]);
					if (tp->kind == DDS_INT_16_TYPE) {
						if (ip->label)
							*ip->label = (short) s;
						if (ip->action == PA_DumpData)
							dbg_printf ("%d", (short) s);
					}
					else if (tp->kind == DDS_UINT_16_TYPE) {
						if (ip->label)
							*ip->label = s;
						if (ip->action == PA_DumpData)
							dbg_printf ("%uU", s);
					}
					else {
						if (ip->label)
							*ip->label = s;
						if (ip->action == PA_DumpData)
							dbg_dump_e_b (tp, (short) s);
					}
					ip->label = NULL;
				}
			}
			if (ip->sformat == CF_CDR || key)
				sofs += 2;
			break;

		case DDS_CHAR_32_TYPE:
		case DDS_INT_32_TYPE:
		case DDS_UINT_32_TYPE:
		case DDS_FLOAT_32_TYPE:

		    get_32:
			if (ip->sformat == CF_CDR || key)
				sofs = ALIGN (sofs, 4);

			if (!ip->key_mode || key) {
				if (ip->action == PA_GetCDRKeyLength)
					ip->dofs = ALIGN (ip->dofs, 4) + 4;
				else if (PA_CopyData (ip->action)) {
					if (CF_Key (ip->dformat))
						CDR_ALIGN (ip->dofs, 4, 1, ip->dst);
					if (ip->cnv_swap)
						memcswap32 (&ip->dst [ip->dofs], &src [sofs]);
					else
						memcpy32 (&ip->dst [ip->dofs], &src [sofs]);
					if (CF_Key (ip->dformat))
						ip->dofs += 4;
				}
				if (ip->label || ip->action == PA_DumpData) {
					if (tp->kind == DDS_FLOAT_32_TYPE) {
						if (ip->label)
							return (0);

						if (ip->int_swap)
							memcswap32 (&f, &src [sofs]);
						else
							memcpy32 (&f, &src [sofs]);
						dbg_printf ("%f", f);
					}
					else {
						if (ip->int_swap)
							memcswap32 (&l, &src [sofs]);
						else
							memcpy32 (&l, &src [sofs]);
						if (ip->label) {
							if (tp->kind == DDS_UINT_32_TYPE)
								*ip->label = l;
							else
								*ip->label = (int32_t) l;
						}
						if (ip->action == PA_DumpData) {
							if (tp->kind == DDS_INT_32_TYPE)
								dbg_printf ("%d", (int32_t) l);
							else if (tp->kind == DDS_UINT_32_TYPE)
								dbg_printf ("%uU", l);
							else if (tp->kind == DDS_CHAR_32_TYPE)
# if 0
								if (iswprint ((wchar_t) l))
									dbg_printf ("'%lc'", l);
								else
# endif
									dbg_printf ("'\\x%u'", l);
							else
								dbg_dump_e_b (tp, (int32_t) l);
						}
					}
					ip->label = NULL;
				}
			}
			if (ip->sformat == CF_CDR || key)
				sofs += 4;
			break;

		case DDS_INT_64_TYPE:
		case DDS_UINT_64_TYPE:
		case DDS_FLOAT_64_TYPE:

		    get_64:
			if (ip->sformat == CF_CDR || key)
				sofs = ALIGN (sofs, 8);

			if (!ip->key_mode || key) {
				if (ip->action == PA_GetCDRKeyLength)
					ip->dofs = ALIGN (ip->dofs, 8) + 8;
				else if (PA_CopyData (ip->action)) {
					if (CF_Key (ip->dformat))
						CDR_ALIGN (ip->dofs, 8, 1, ip->dst);
					if (ip->cnv_swap)
						memcswap64 (&ip->dst [ip->dofs], &src [sofs]);
					else
						memcpy64 (&ip->dst [ip->dofs], &src [sofs]);
					if (CF_Key (ip->dformat))
						ip->dofs += 8;
				}
				if (ip->label || ip->action == PA_DumpData) {
					if (tp->kind == DDS_FLOAT_64_TYPE) {
						if (ip->label)
							return (0);

						if (ip->int_swap)
							memcswap64 (&d, &src [sofs]);
						else
							memcpy64 (&d, &src [sofs]);
						dbg_printf ("%f", d);
					}
					else {
						if (ip->int_swap)
							memcswap64 (&ll, &src [sofs]);
						else
							memcpy64 (&ll, &src [sofs]);
						if (ip->label)
							*ip->label = ll;
						if (ip->action == PA_DumpData) {
							if (tp->kind == DDS_INT_64_TYPE)
								dbg_printf ("%" PRId64, (int64_t) ll);
							else if (tp->kind == DDS_UINT_64_TYPE)
								dbg_printf ("%" PRIu64 "U", ll);
							else
								dbg_dump_e_b (tp, ll);
						}
					}
					ip->label = NULL;
				}
			}
			if (ip->sformat == CF_CDR || key)
				sofs += 8;
			break;

		case DDS_FLOAT_128_TYPE:
			if (ip->sformat == CF_CDR || key)
				sofs = ALIGN (sofs, 8);

			if (!ip->key_mode || key) {
				if (ip->action == PA_GetCDRKeyLength)
					ip->dofs = ALIGN (ip->dofs, 8) + 16;
				else if (PA_CopyData (ip->action)) {
					if (CF_Key (ip->dformat))
						CDR_ALIGN (ip->dofs, 8, 1, ip->dst);
					if (ip->cnv_swap) {
						memcswap64 (&ip->dst [ip->dofs], &src [sofs + 8]);
						memcswap64 (&ip->dst [ip->dofs + 8], &src [sofs]);
					}
					else {
						memcpy64 (&ip->dst [ip->dofs], &src [sofs]);
						memcpy64 (&ip->dst [ip->dofs + 8], &src [sofs + 8]);
					}
					if (CF_Key (ip->dformat))
						ip->dofs += 16;
				}
				else if (ip->action == PA_DumpData) {
					if (ip->int_swap) {
						memcswap64 (((char *) &ld) + 8, &src [sofs]);
						memcswap64 (&ld, &src [sofs + 8]);
					}
					else {
						memcpy64 (&ld, &src [sofs]);
						memcpy64 (((char *) &ld) + 8, &src [sofs + 8]);
					}
					dbg_printf ("%Lf", ld);
				}
			}
			if (ip->sformat == CF_CDR || key)
				sofs += 16;
			break;

		case DDS_ENUMERATION_TYPE: {
			EnumType *ep = (EnumType *) tp;

			if (ep->bound <= 16)
				goto get_16;
			else /*if (ep->bound <= 32)*/
				goto get_32;
		}
		case DDS_BITSET_TYPE: {
			BitSetType *bp = (BitSetType *) tp;

			if (bp->bit_bound <= 8)
				goto get_8;
			else if (bp->bit_bound <= 16)
				goto get_16;
			else if (bp->bit_bound <= 32)
				goto get_32;
			else
				goto get_64;
		}
		case DDS_ARRAY_TYPE:
			sofs = cdr_parse_array (src, sofs, (const ArrayType *) tp, ip, key);
			break;
		case DDS_SEQUENCE_TYPE:
		case DDS_MAP_TYPE:
			sofs = cdr_parse_seqmap (src, sofs, (const SequenceType *) tp, ip, key);
			break;
		case DDS_STRING_TYPE:
			sofs = cdr_parse_string (src, sofs, (const StringType *) tp, ip, key);
			break;
		case DDS_UNION_TYPE:
			sofs = cdr_parse_union (src, sofs, (const UnionType *) tp, ip, key);
			break;
		case DDS_STRUCTURE_TYPE:
			sofs = cdr_parse_struct (src, sofs, (const StructureType *) tp, ip, key);
			break;
		default:
			return (0);
	}
	return (sofs);
}

/* cdr_unmarshalled_size -- Get the unmarshalled, i.e. native data size from
			    CDR-encoded source data (data) with the given
			    header alignment (hsize).
			    If key set: marshalled data is a key list.
			    If key && msize: string key fields are padded.
			    If swap set: reverse endianness. */

size_t cdr_unmarshalled_size (const void       *data,
			      size_t           hsize,
			      const Type       *type,
			      int              key,
			      int              msize,
			      int              swap,
			      size_t           size,
			      DDS_ReturnCode_t *error)
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	ParseInfo		info;
	DDS_ReturnCode_t	err = DDS_RETCODE_BAD_PARAMETER;

	prof_start (cdr_um_size);

	if (!data || !type) {
		if (error)
			*error = err;
		return (0);
	}
	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE)) {
		if (error)
			*error = err;
		return (0);
	}
	memset (&info, 0, sizeof (info));
	info.action = PA_GetAuxLength;
	info.sformat = (key) ? ((msize) ? CF_CDR_PaddedKey 
				        : CF_CDR_Key) 
	                     : CF_CDR;
	info.dformat = CF_Native;
	info.dofs = hsize;
	info.int_swap = info.cnv_swap = swap;
	info.key_mode = key;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
		if ((!size || size >= stp->size) &&
		    cdr_parse_struct (src - hsize, hsize, stp, &info, 1)) {
			err = DDS_RETCODE_OK;
			info.dlength += (size) ? size : stp->size;
		}
	}
	else {
		utp = (const UnionType *) type;
		if ((!size || size >= utp->size) &&
		    cdr_parse_union (src - hsize, hsize, utp, &info, 1)) {
			err = DDS_RETCODE_OK;
			info.dlength += (size) ? size : utp->size;
		}
	}
	if (error)
		*error = err;

	prof_stop (cdr_um_size, 1);
	return (info.dlength);
}

/* cdr_unmarshall -- Get unmarshalled data from CDR-encoded source data (data)
		     with the given alignment (hsize) into a destination buffer
		     (dest).
		     If key set: marshalled data is a key list.
		     If key && msize: string key fields are padded.
		     If swap set: reverse endianness for correct byte order. */

DDS_ReturnCode_t cdr_unmarshall (void       *dest,
				 const void *data,
			         size_t     hsize,
				 const Type *type,
				 int        key,
			         int        msize,
				 int        swap,
				 size_t     size)
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	ParseInfo		info;

	prof_start (cdr_um);

	if (!dest || !data || !type)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE))
		return (DDS_RETCODE_BAD_PARAMETER);

	memset (&info, 0, sizeof (info));
	info.action = PA_GetNative;
	info.sformat = (key) ? ((msize) ? CF_CDR_PaddedKey 
				        : CF_CDR_Key) 
	                     : CF_CDR;
	info.dformat = CF_Native;
	info.dst = dest;
	info.int_swap = info.cnv_swap = swap;
	info.key_mode = key;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
		if (size && size < stp->size)
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!size)
			size = stp->size;

		info.aux_dst = (unsigned char *) dest + size;
		if (!cdr_parse_struct (src - hsize, hsize, stp, &info, 1))
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else {
		utp = (const UnionType *) type;
		if (size && size < utp->size)
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!size)
			size = utp->size;

		info.aux_dst = (unsigned char *) dest + size;
		if (!cdr_parse_union (src - hsize, hsize, utp, &info, 1))
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	prof_stop (cdr_um, 1);
	return (DDS_RETCODE_OK);
}

/* cdr_dynamic_data -- Get a dynamic data container from CDR-encoded marshalled
		       data (data) which has a given alignment offset (hsize).
		       If successful, a Dynamic Data container is returned
		       containing the data description, which can be queried
		       via the DDS_DynamicData_* primitives. */

DynData_t *cdr_dynamic_data (const void *data,
			     size_t     hsize,
			     const Type *type,
			     int        key,
			     int        copy,
			     int        swap)
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	ParseInfo		info;

	prof_start (cdr_um);

	if (!data || !type)
		return (NULL);

	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE))
		return (NULL);

	memset (&info, 0, sizeof (info));
	info.action = PA_GetDynamic;
	info.sformat = (key) ? CF_CDR_Key : CF_CDR;
	info.dformat = CF_Native;
	info.int_swap = info.cnv_swap = swap;
	info.key_mode = key;
	info.copy_data = copy;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
		if (!cdr_parse_struct (src - hsize, hsize, stp, &info, 1))
			return (NULL);
	}
	else {
		utp = (const UnionType *) type;
		if (!cdr_parse_union (src - hsize, hsize, utp, &info, 1))
			return (NULL);
	}
	prof_stop (cdr_um, 1);
	return (info.dyn_dst);
}

/* cdr_key_size -- Get the size of a set of CDR-encoded concatenated key fields
		   from marshalled data (data) with given alignment (hsize).
		   If key set: marshalled data contains unpadded key fields.
		   If key && msize: resulting key list must have strings padded.
		   If swap: reverse endianness for correct byte order. */

size_t cdr_key_size (size_t           dhsize,	/* Dst: data offset.          */
		     const void       *data,	/* Src: data pointer.         */
		     size_t           hsize,	/* Src: data offset.          */
		     const Type       *type,	/* Type info.                 */
		     int              key,	/* Src: key fields only.      */
		     int              msize,	/* Dst: max-sized fields.     */
		     int              swap,	/* Swap reqd between src/dst. */
		     int              iswap,	/* Src: non-cpu endianness.   */
		     DDS_ReturnCode_t *error)	/* Resulting error code.      */
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	ParseInfo		info;
	DDS_ReturnCode_t 	err = DDS_RETCODE_BAD_PARAMETER;

	prof_start (cdr_k_size);

	if (!data || !type) {
		info.dofs = 0;
		goto out;
	}
	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE)) {
		info.dofs = 0;
		goto out;
	}
	memset (&info, 0, sizeof (info));
	info.action = PA_GetCDRKeyLength;
	info.sformat = (key) ? CF_CDR_Key : CF_CDR;
	info.dformat = (msize) ? CF_CDR_PaddedKey : CF_CDR_Key;
	info.dofs = dhsize;
	info.cnv_swap = swap;
	info.int_swap = iswap;
	info.key_mode = 1;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
		if (cdr_parse_struct (src - hsize, hsize, stp, &info, 1)) {
			err = DDS_RETCODE_OK;
			info.dofs -= dhsize;
		}
		else
			info.dofs = 0;
	}
	else {
		utp = (const UnionType *) type;
		if (cdr_parse_union (src - hsize, hsize, utp, &info, 1)) {
			err = DDS_RETCODE_OK;
			info.dofs -= dhsize;
		}
		else
			info.dofs = 0;
	}

    out:
	if (error)
		*error = err;

	prof_stop (cdr_k_size, 1);
	return (info.dofs);
}

/* cdr_key_fields -- Get CDR-encoded key fields from CDR-encoded data (data)
		     with given alignment offset (hsize) in the buffer (dest).
		     If key set: marshalled data contains unpadded key fields.
		     If key && msize: resulting key list must pad strings
		     If swap: reverse endianness for correct byte order. */

DDS_ReturnCode_t cdr_key_fields (void       *dest,	/* Dst: data pointer. */
				 size_t     dhsize,	/* Dst: data offset.  */
				 const void *data,	/* Src: data pointer. */
				 size_t     hsize,	/* Src: data offset.  */
				 const Type *type,	/* Type info.         */
				 int        key,	/* Src: key data only.*/
				 int        msize,	/* Dst: max-size reqd.*/
				 int        swap,	/* Swap src->dst.     */
				 int        iswap)	/* Src: non-cpu end.  */
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	ParseInfo		info;

	prof_start (cdr_k_get);

	if (!dest || !data || !type)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE))
		return (DDS_RETCODE_BAD_PARAMETER);

	memset (&info, 0, sizeof (info));
	info.action = PA_GetCDRKey;
	info.sformat = (key) ? CF_CDR_Key : CF_CDR;
	info.dformat = (msize) ? CF_CDR_PaddedKey : CF_CDR_Key;
	info.dst = (unsigned char *) dest - dhsize;
	info.dofs = dhsize;
	info.cnv_swap = swap;
	info.int_swap = iswap;
	info.key_mode = 1;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
		if (!cdr_parse_struct (src - hsize, hsize, stp, &info, 1))
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else {
		utp = (const UnionType *) type;
		if (!cdr_parse_union (src - hsize, hsize, utp, &info, 1))
			return (DDS_RETCODE_BAD_PARAMETER);
	}

	prof_stop (cdr_k_get, 1);
	return (DDS_RETCODE_OK);
}

/* cdr_field_offset -- Get the field offset of a structure field (field = index)
		       from CDR-encoded data (data) with the specified alignment
		       offset (hsize).
		       If swap: the marshalled data has non-native byte order.*/

size_t cdr_field_offset (const void       *data,
		         size_t           hsize,
		         unsigned         field,
		         const Type       *type,
			 int              swap,
		         DDS_ReturnCode_t *error)
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	size_t			offset;
	ParseInfo		info;

	prof_start (cdr_f_ofs);

	if (!data || !type) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE)) {
		*error = DDS_RETCODE_BAD_PARAMETER;
		return (0);
	}
	memset (&info, 0, sizeof (info));
	info.action = PA_GetOffset;
	info.sformat = CF_CDR;
	info.dformat = CF_Native;
	info.dofs = hsize;
	info.field = field;
	info.int_swap = info.cnv_swap = swap;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
	 	offset = cdr_parse_struct (src - hsize, hsize, stp, &info, 1);
	}
	else {
		utp = (const UnionType *) type;
	 	offset = cdr_parse_union (src - hsize, hsize, utp, &info, 1);
	}
	if (!offset)
		*error = DDS_RETCODE_BAD_PARAMETER;
	else
		*error = DDS_RETCODE_OK;
	prof_stop (cdr_f_ofs, 1);
	return (offset);
}

#ifdef DDS_DEBUG

/* cdr_dump_cdr -- Dump a CDR-encoded data sample (data) with the specified
		   alignment offset (hsize).  The indent parameter specifies
		   the number of initial tabs when dumping. */

DDS_ReturnCode_t cdr_dump_cdr (unsigned   indent,
			       const void *data,
			       size_t     hsize,
			       const Type *type,
			       int        key,
			       int        msize,
			       int        swap,
			       int        names)
{
	const StructureType	*stp;
	const UnionType		*utp;
	const unsigned char	*src = (const unsigned char *) data;
	ParseInfo		info;

	if (!data || !type)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (type->kind == DDS_ALIAS_TYPE)
		type = xt_real_type (type);
	if (!type ||
	    (type->kind != DDS_STRUCTURE_TYPE &&
	     type->kind != DDS_UNION_TYPE))
		return (DDS_RETCODE_BAD_PARAMETER);

	memset (&info, 0, sizeof (info));
	info.action = PA_DumpData;
	info.sformat = (key) ? ((msize) ? CF_CDR_PaddedKey
					: CF_CDR_Key)
			     : CF_CDR;
	info.indent = indent;
	info.int_swap = swap;
	info.key_mode = key;
	info.names = names;
	if (type->kind == DDS_STRUCTURE_TYPE) {
		stp = (const StructureType *) type;
		if (!cdr_parse_struct (src - hsize, hsize, stp, &info, 1))
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	else {
		utp = (const UnionType *) type;
		if (!cdr_parse_union (src - hsize, hsize, utp, &info, 1))
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

#endif

void cdr_init (void)
{
	PROF_INIT ("C:MSize", cdr_m_size);
	PROF_INIT ("C:Marshall", cdr_m);
	PROF_INIT ("C:UMSize", cdr_um_size);
	PROF_INIT ("C:UMarshal", cdr_um);
	PROF_INIT ("C:KeySize", cdr_k_size);
	PROF_INIT ("C:KeyGet", cdr_k_get);
	PROF_INIT ("C:FieldOfs", cdr_f_ofs);
}

