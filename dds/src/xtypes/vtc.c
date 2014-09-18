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

/* vtc -- Vendor-specific typecode support functions. */

#include <stdint.h>
#include <ctype.h>
#include "sys.h"
#include "error.h"
#include "xtypes.h"
#include "xcdr.h"
#include "typecode.h"

#ifdef DDS_TYPECODE

typedef enum {
	VTC_NULL,
	VTC_SHORT,
	VTC_LONG,
	VTC_USHORT,
	VTC_ULONG,
	VTC_FLOAT,
	VTC_DOUBLE,
	VTC_BOOLEAN,
	VTC_CHAR,
	VTC_OCTET,
	VTC_STRUCT,
	VTC_UNION,
	VTC_ENUM,
	VTC_STRING,
	VTC_SEQUENCE,
	VTC_ARRAY,
	VTC_ALIAS,
	VTC_LONGLONG,
	VTC_ULONGLONG,
	VTC_LONGDOUBLE,
	VTC_WCHAR,
	VTC_WSTRING,
	VTC_VALUE,
	VTC_VALUE_PARAM
} VTCKind;

#define	ALIGN(delta, boundary) (delta) = (((delta) + 		\
				boundary - 1) & ~(boundary - 1))
#define	REQUIRE(n,len,ofs) if ((ofs) + (n) > (len)) return (0)
#define	GSWAP16(u16,vtc,len,ofs,swap)	 			\
		if (swap) { 					\
			memcswap16 (&u16, &vtc [ofs]); 		\
			*(uint16_t *) (&vtc [ofs]) = u16; 	\
		} else u16 = *((uint16_t *) (&vtc [ofs])); 	\
		(ofs) += 2
#define	GSWAP32(u32,vtc,len,ofs,swap)	 			\
		if (swap) { 					\
			memcswap32 (&u32, &vtc [ofs]); 		\
			*(uint32_t *) (&vtc [ofs]) = u32; 	\
		} else u32 = *((uint32_t *) (&vtc [ofs])); 	\
		(ofs) += 4
#define	GSTRING(sl,vtc,len,ofs,i)				\
		if (!isalpha (vtc [(ofs)++])) return (0);	\
		for (i = 1; i < slen - 1; i++,(ofs)++)		\
			if (vtc [ofs] != '_' &&			\
			    vtc [ofs] != '.' &&			\
			    !isalnum (vtc [ofs]))		\
				return (0);			\
		if (vtc [(ofs)++]) return (0)
#define	GNAME(sl,vtc,len,ofs,swap,i)				\
		ALIGN (ofs, 4);	REQUIRE (4, length, ofs);	\
		GSWAP32 (slen, vtc, length, ofs, swap);		\
		if (slen < 2) return (0);			\
		REQUIRE (slen, length, ofs);			\
		GSTRING (slen, vtc, length, ofs,i)

/* vtc_validate -- Validate transported typecode which is in vendor-specific
		   format.  If swap is set, the data needs to be swapped.
		   If something went wrong during validation, a 0 result
		   is returned.  Otherwise, the data will be in native format
		   and 1 is returned. */

int vtc_validate (const unsigned char *vtc,
		  size_t              length,
		  unsigned            *ofs,
		  int                 swap,
		  int                 ext)
{
	unsigned	i, j, c;
	size_t		pofs, mleft;
	int32_t		dindex;
	uint32_t	tc, slen, nmembers, def_index, num, d, nlabels;
	uint16_t	n, mlen, bits;

	ALIGN (*ofs, 4);
	REQUIRE (6, length, *ofs);
	GSWAP32 (tc, vtc, length, *ofs, swap);
	tc &= 0x7fffffff;
	if ((*ofs == 4 && tc != VTC_STRUCT) ||
	    tc > VTC_VALUE_PARAM)
		return (0);

	GSWAP16 (n, vtc, length, *ofs, swap);
	switch (tc) {
		case VTC_SHORT:
		case VTC_LONG:
		case VTC_USHORT:
		case VTC_ULONG:
		case VTC_FLOAT:
		case VTC_DOUBLE:
		case VTC_BOOLEAN:
		case VTC_CHAR:
		case VTC_OCTET:
		case VTC_LONGLONG:
		case VTC_ULONGLONG:
		case VTC_LONGDOUBLE:
		case VTC_WCHAR:
			if (n)
				return (0);
			break;

		case VTC_STRUCT:
			if (ext) {
				GSWAP16 (bits, vtc, length, *ofs, swap);
			}
			GNAME (slen, vtc, length, *ofs, swap, c);
			ALIGN (*ofs, 4);
			REQUIRE (4, length, *ofs);
			GSWAP32 (nmembers, vtc, length, *ofs, swap);
			if (!nmembers)
				return (0);

			for (i = 0; i < nmembers; i++) {
				REQUIRE (4, length, *ofs);
				GSWAP16 (mlen, vtc, length, *ofs, swap);
				if (mlen < 18 || *ofs + mlen > length)
					return (0);

				if (ext) {
					GSWAP16 (bits, vtc, length, *ofs, swap);
				}
				pofs = *ofs;
				GNAME (slen, vtc, length, *ofs, swap, c);
				if (vtc [(*ofs)++] > 7)
					return (0);

				ALIGN (*ofs, 2);
				REQUIRE (3, length, *ofs);
				GSWAP16 (bits, vtc, length, *ofs, swap);
				if (!ext && bits != 0xffff)
					return (0);

				if (vtc [(*ofs)++] > 1)
					return (0);

				mleft = pofs + mlen;
				if (!vtc_validate (vtc, mleft, ofs, swap, ext))
					return (0);
			}
			break;

		case VTC_UNION:
			if (ext) {
				GSWAP16 (bits, vtc, length, *ofs, swap);
			}
			GNAME (slen, vtc, length, *ofs, swap, c);
			ALIGN (*ofs, 4);
			REQUIRE (10, length, *ofs);
			GSWAP32 (def_index, vtc, length, *ofs, swap);
			dindex = (int32_t) def_index;
			if (!vtc_validate (vtc, length, ofs, swap, ext))
				return (0);

			ALIGN (*ofs, 4);
			REQUIRE (4, length, *ofs);
			GSWAP32 (nmembers, vtc, length, *ofs, swap);
			if (!nmembers || (dindex != -1 && dindex >= (int) nmembers))
				return (0);

			for (i = 0; i < nmembers; i++) {
				REQUIRE (2, length, *ofs);
				GSWAP16 (mlen, vtc, length, *ofs, swap);
				if (mlen < 18 || *ofs + mlen > length)
					return (0);

				pofs = *ofs;
				if (ext) {
					REQUIRE (4, length, *ofs);
					GSWAP32 (num, vtc, length, *ofs, swap);
				}
				GNAME (slen, vtc, length, *ofs, swap, c);
				if (vtc [(*ofs)++] > 7)
					return (0);

				ALIGN (*ofs, 4);
				REQUIRE (4, length, *ofs);
				GSWAP32 (nlabels, vtc, length, *ofs, swap);
				REQUIRE (nlabels << 2, length, *ofs);
				for (j = 0; j < nlabels; j++)
					GSWAP32 (num, vtc, length, *ofs, swap);

				mleft = pofs + mlen;
				if (!vtc_validate (vtc, mleft, ofs, swap, ext))
					return (0);
			}
			break;

		case VTC_ENUM:
			GNAME (slen, vtc, length, *ofs, swap, c);
			ALIGN (*ofs, 4);
			REQUIRE (4, length, *ofs);
			GSWAP32 (nmembers, vtc, length, *ofs, swap);
			if (!nmembers)
				return (0);

			for (i = 0; i < nmembers; i++) {
				REQUIRE (2, length, *ofs);
				GSWAP16 (mlen, vtc, length, *ofs, swap);
				if (mlen < 16 || *ofs + mlen > length)
					return (0);

				pofs = *ofs;
				GNAME (slen, vtc, length, *ofs, swap, c);
				ALIGN (*ofs, 4);
				REQUIRE (4, length, *ofs);
				GSWAP32 (num, vtc, length, *ofs, swap);
			}
			break;

		case VTC_STRING:
		case VTC_WSTRING:
			*ofs += 2;
			pofs = *ofs;
			REQUIRE (4, length, *ofs);
			ALIGN (*ofs, 4);
			GSWAP32 (slen, vtc, length, *ofs, swap);
			break;

		case VTC_SEQUENCE:
			*ofs += 2;
			pofs = *ofs;
			REQUIRE (6, length, *ofs);
			ALIGN (*ofs, 4);
			GSWAP32 (slen, vtc, length, *ofs, swap);
			if (!vtc_validate (vtc, length, ofs, swap, ext))
				return (0);

			break;

		case VTC_ARRAY:
			*ofs += 2;
			pofs = *ofs;
			REQUIRE (6, length, *ofs);
			ALIGN (*ofs, 4);
			GSWAP32 (num, vtc, length, *ofs, swap);
			if (!num)
				return (0);

			for (i = 0; i < num; i++) {
				REQUIRE (4, length, *ofs);
				GSWAP32 (d, vtc, length, *ofs, swap);

				if (!d || d > 0xffff)
					return (0);
			}
			if (!vtc_validate (vtc, length, ofs, swap, ext))
				return (0);

			break;

		default:
			return (0);
	}
	return (1);
}

#define	WALIGN(p,ofs,b)	while (((ofs) & (b - 1)) != 0) { 		\
				if (p) p [ofs] = 0; (ofs)++; }
#define	WUINT8(p,ofs,u8) if(p) *((uint8_t *) (&p [ofs])) = u8;		\
			   (ofs) += 1
#define	WUINT16(p,ofs,u16) if(p) *((uint16_t *) (&p [ofs])) = u16;	\
			   (ofs) += 2
#define	WUINT32(p,ofs,u32) if(p) *((uint32_t *) (&p [ofs])) = u32; 	\
			   (ofs) += 4
#define	WSTRING(p,ofs,s,l) if(p) memcpy (&p [ofs], s, l);		\
			   (ofs) += l
#define	WNAME(p,ofs,s)	WALIGN (p,ofs,4); 				\
			WUINT32 (p,ofs,strlen(s) + 1);			\
			WSTRING (p,ofs,s,strlen(s) + 1)

/* vtc_generate -- Generate typecode from an X-types type. */

static unsigned vtc_generate (unsigned char *tcp, Type *tp, unsigned *ofs, int ext)
{
	Type		*mtp, *dtp;
	StructureType	*stp;
	UnionType	*utp;
	Member		*mp;
	UnionMember	*ump;
	EnumType	*etp;
	EnumConst	*ecp;
	StringType	*ttp;
	SequenceType	*qtp;
	ArrayType	*atp;
	const char	*sp;
	unsigned	pofs, mofs, i, j;
	int		def_index;
	uint16_t	s;
	uint8_t		flags;
	VTCKind		tc, vtc [] = {
		VTC_NULL, VTC_BOOLEAN, VTC_OCTET, VTC_SHORT, VTC_USHORT,
		VTC_LONG, VTC_ULONG, VTC_LONGLONG, VTC_ULONGLONG,
		VTC_FLOAT, VTC_DOUBLE, VTC_LONGDOUBLE, VTC_CHAR, VTC_WCHAR,
		VTC_ENUM, VTC_ULONG, VTC_ALIAS, VTC_ARRAY, VTC_SEQUENCE,
		VTC_STRING, VTC_SEQUENCE, VTC_UNION, VTC_STRUCT
	};

	tc = vtc [tp->kind];
	WALIGN (tcp, *ofs, 4);
	WUINT32 (tcp, *ofs, tc | 0x80000000);
	switch (tc) {
		case VTC_SHORT:
		case VTC_LONG:
		case VTC_USHORT:
		case VTC_ULONG:
		case VTC_FLOAT:
		case VTC_DOUBLE:
		case VTC_BOOLEAN:
		case VTC_CHAR:
		case VTC_OCTET:
		case VTC_LONGLONG:
		case VTC_ULONGLONG:
		case VTC_LONGDOUBLE:
		case VTC_WCHAR:
			WUINT16 (tcp, *ofs, 0);
			break;

		case VTC_STRUCT:
			stp = (StructureType *) tp;
			sp = str_ptr (tp->name);
			if (!sp || (sp [0] == '_' && sp [1] == '_'))
				sp = "struct";
			pofs = *ofs;
			*ofs += 2; /* Leave room for total size. */
			if (ext) {
				s = (tp->extensible + 1) << NRE_EXT_S;
				WUINT16 (tcp, *ofs, s);
			}
			WNAME (tcp, *ofs, sp);
			WALIGN (tcp, *ofs, 4);
			WUINT32 (tcp, *ofs, stp->nmembers);
			for (i = 0, mp = stp->member;
			     i < stp->nmembers;
			     i++, mp++) {
				mofs = *ofs;
				*ofs += 2; /* Leave room for member size. */
				if (ext) {
					s = mp->member_id >> 16;
					WUINT16 (tcp, *ofs, s);
				}
				sp = str_ptr (mp->name);
				WNAME (tcp, *ofs, sp);
				flags = mp->is_shareable;
				if (ext) {
					if (mp->must_understand)
						flags |= 2;
					if (mp->is_optional)
						flags |= 4;
				}
				WUINT8 (tcp, *ofs, flags);
				if (ext)
					s = mp->member_id & 0xffff;
				else
					s = 0xffff;
				WALIGN (tcp, *ofs, 2);
				WUINT16 (tcp, *ofs, s);
				WUINT8 (tcp, *ofs, mp->is_key);
				WALIGN (tcp, *ofs, 4);
				mtp = xt_type_ptr (tp->scope, mp->id);
				if (!mtp || !vtc_generate (tcp, mtp, ofs, ext))
					return (0);

				s = *ofs - mofs - 2;
				WUINT16 (tcp, mofs, s);
			}
			s = *ofs - pofs - 2;
			WUINT16 (tcp, pofs, s);
			break;

		case VTC_UNION:
			utp = (UnionType *) tp;
			sp = str_ptr (tp->name);
			if (!sp || (sp [0] == '_' && sp [1] == '_'))
				sp = "union";
			pofs = *ofs;
			*ofs += 2;
			if (ext) {
				s = (tp->extensible + 1) << NRE_EXT_S;
				WUINT16 (tcp, *ofs, s);
			}
			WNAME (tcp, *ofs, sp);
			WALIGN (tcp, *ofs, 4);
			def_index = -1;
			for (i = 0, ump = utp->member;
			     i < utp->nmembers;
			     i++, ump++)
				if (ump->is_default) {
					def_index = i;
					break;
				}
			WUINT32 (tcp, *ofs, def_index);
			dtp = xt_type_ptr (tp->scope,
					   utp->member [0].member.id);
			if (!dtp || !vtc_generate (tcp, dtp, ofs, ext))
				return (0);

			WALIGN (tcp, *ofs, 4);
			WUINT32 (tcp, *ofs, utp->nmembers);
			for (i = 0, ump = utp->member;
			     i < utp->nmembers;
			     i++, ump++) {
				mofs = *ofs;
				*ofs += 2; /* Leave room for member size. */
				if (ext) {
					WUINT32 (tcp, *ofs, ump->member.member_id);
				}
				sp = str_ptr (ump->member.name);
				WNAME (tcp, *ofs, sp);
				flags = ump->member.is_shareable;
				if (ext) {
					if (ump->member.is_optional)
						flags |= 2;
					if (ump->member.must_understand)
						flags |= 4;
				}
				WUINT8 (tcp, *ofs, flags);
				WALIGN (tcp, *ofs, 4);
				WUINT32 (tcp, *ofs, ump->nlabels);
				if (ump->nlabels == 1) {
					WUINT32 (tcp, *ofs, ump->label.value);
				}
				else
					for (j = 0; j < ump->nlabels; j++) {
						WUINT32 (tcp, *ofs, ump->label.list [j]);
					}

				mtp = xt_type_ptr (tp->scope, ump->member.id);
				if (!vtc_generate (tcp, mtp, ofs, ext))
					return (0);

				s = *ofs - mofs - 2;
				WUINT16 (tcp, mofs, s);
			}
			s = *ofs - pofs - 2;
			WUINT16 (tcp, pofs, s);
			break;

		case VTC_ENUM:
			etp = (EnumType *) tp;
			sp = str_ptr (tp->name);
			if (!sp || (sp [0] == '_' && sp [1] == '_'))
				sp = "enum";
			pofs = *ofs;
			*ofs += 2;
			WNAME (tcp, *ofs, sp);
			WALIGN (tcp, *ofs, 4);
			WUINT32 (tcp, *ofs, etp->nconsts);
			for (i = 0, ecp = etp->constant;
			     i < etp->nconsts;
			     i++, ecp++) {
				mofs = *ofs;
				*ofs += 2; /* Leave room for element size. */
				sp = str_ptr (ecp->name);
				WNAME (tcp, *ofs, sp);
				WALIGN (tcp, *ofs, 4);
				WUINT32 (tcp, *ofs, ecp->value);
				s = *ofs - mofs - 2;
				WUINT16 (tcp, mofs, s);
			}
			s = *ofs - pofs - 2;
			WUINT16 (tcp, pofs, s);
			break;

		case VTC_STRING:
		case VTC_WSTRING:
			ttp = (StringType *) tp;
			pofs = *ofs;
			*ofs += 2; /* Leave room for total size. */
			WALIGN (tcp, *ofs, 4);
			WUINT32 (tcp, *ofs, ttp->bound);
			s = *ofs - pofs - 2;
			WUINT16 (tcp, pofs, s);
			break;

		case VTC_SEQUENCE:
			qtp = (SequenceType *) tp;
			pofs = *ofs;
			*ofs += 2; /* Leave room for total size. */
			WALIGN (tcp, *ofs, 4);
			WUINT32 (tcp, *ofs, qtp->bound);
			mtp = xt_type_ptr (tp->scope,
					   qtp->collection.element_type);
			if (!vtc_generate (tcp, mtp, ofs, ext))
				return (0);

			s = *ofs - pofs - 2;
			WUINT16 (tcp, pofs, s);
			break;

		case VTC_ARRAY:
			atp = (ArrayType *) tp;
			pofs = *ofs;
			*ofs += 2; /* Leave room for total size. */
			WALIGN (tcp, *ofs, 4);
			WUINT32 (tcp, *ofs, atp->nbounds);
			for (i = 0; i < atp->nbounds; i++) {
				WUINT32 (tcp, *ofs, atp->bound [i]);
			}
			mtp = xt_type_ptr (tp->scope,
					   atp->collection.element_type);
			if (!vtc_generate (tcp, mtp, ofs, ext))
				return (0);

			s = *ofs - pofs - 2;
			WUINT16 (tcp, pofs, s);
			break;

		default:
			return (0);
	}
	return (*ofs);
}

/* vtc_generate_ts -- Generate typecode from a typesupport structure. */

static unsigned vtc_generate_ts (unsigned char *tcp, const TypeSupport_t *ts)
{
	const PL_TypeSupport	*pl;
	Type			*tp;
	unsigned		s, ofs;
	uint16_t		*up;

	switch (ts->ts_prefer) {
		case MODE_CDR:
			ofs = 0;
			tp = ts->ts [MODE_CDR].cdr;
			s = vtc_generate (tcp, tp, &ofs, tp->extended);
			break;

		case MODE_V_TC:
			up = (uint16_t *) ts->ts [MODE_V_TC].tc;
			s = up [2] + 6;
			if (tcp)
				memcpy (tcp, up, s);
			break;

		case MODE_PL_CDR:
			pl = ts->ts [MODE_PL_CDR].pl;
			if (pl->builtin)
				s = 0;
			else {
				ofs = 0;
				tp = pl->xtype;
				s = vtc_generate (tcp, tp, &ofs, 1);
			}
			break;

		case MODE_XML:
		case MODE_RAW:
		case MODE_X_TC:
		default:
			s = 0;
			break;
	}
	return (s);
}

/* vtc_create -- Create vendor-specific typecode data from a previously created
		 type. */

unsigned char *vtc_create (const TypeSupport_t *ts)
{
	VTC_Header_t	*p;
	unsigned	s;
	unsigned char	*tcp;

	s = vtc_generate_ts (NULL, ts);
	if (!s || (tcp = xmalloc (s)) == NULL)
		return (NULL);

	vtc_generate_ts (tcp, ts);
	p = (VTC_Header_t *) tcp;
	p->nrefs_ext |= 1;
	return (tcp);
}

#define	MAX_LABELS	16	/* If more labels: allocate. */

#define	GUINT16(vtc,ofs) *((uint16_t *) (&vtc [ofs])); (ofs) += 2
#define	GUINT32(vtc,ofs) *((uint32_t *) (&vtc [ofs])); (ofs) += 4
#define	GSTR(s,l,vtc,ofs) ALIGN (ofs, 4); l = GUINT32(vtc,ofs); \
			  s= (char *) &vtc [ofs]; (ofs) += l

/* vtc_create_type -- Create a new local type from vendor typecode data. */

static Type *vtc_create_type (TypeLib       *lp,
			      unsigned char *vtc,
			      unsigned      *ofs,
			      int           *dynamic,
			      int           ext)
{
	Type		*tp, *etp, *disc_tp;
	char		*sp, *msp;
	unsigned	i, j, f, max_labels, member_id;
	Extensibility_t	extensible;
	uint32_t	tc, slen, mslen, nmembers, num, nlabels;
	int		ptr, key, top_level = (*ofs == 0);
	uint16_t	bits;
	int32_t		*lbp, *nlbp, *ip, def_index, labels [MAX_LABELS];
	DDS_BoundSeq	bseq;
	uint32_t	bound [9];
	char		buffer [32];
	static unsigned	anon_count = 0;

	ALIGN (*ofs, 4);
	tc = GUINT32 (vtc, *ofs);
	tc &= 0x7fffffff;
	*ofs += 2;	/* Skip type size. */
	switch (tc) {
		case VTC_SHORT:
			tp = xt_primitive_type (DDS_INT_16_TYPE);
			break;
		case VTC_LONG:
			tp = xt_primitive_type (DDS_INT_32_TYPE);
			break;
		case VTC_USHORT:
			tp = xt_primitive_type (DDS_UINT_16_TYPE);
			break;
		case VTC_ULONG:
			tp = xt_primitive_type (DDS_UINT_32_TYPE);
			break;
		case VTC_FLOAT:
			tp = xt_primitive_type (DDS_FLOAT_32_TYPE);
			break;
		case VTC_DOUBLE:
			tp = xt_primitive_type (DDS_FLOAT_64_TYPE);
			break;
		case VTC_BOOLEAN:
			tp = xt_primitive_type (DDS_BOOLEAN_TYPE);
			break;
		case VTC_CHAR:
			tp = xt_primitive_type (DDS_CHAR_8_TYPE);
			break;
		case VTC_OCTET:
			tp = xt_primitive_type (DDS_BYTE_TYPE);
			break;
		case VTC_LONGLONG:
			tp = xt_primitive_type (DDS_INT_64_TYPE);
			break;
		case VTC_ULONGLONG:
			tp = xt_primitive_type (DDS_UINT_64_TYPE);
			break;
		case VTC_LONGDOUBLE:
			tp = xt_primitive_type (DDS_FLOAT_128_TYPE);
			break;
		case VTC_WCHAR:
			tp = xt_primitive_type (DDS_CHAR_32_TYPE);
			break;

		case VTC_STRUCT:
			if (ext) {
				bits = GUINT16 (vtc, *ofs);
				extensible = ((bits & NRE_EXT_M) >> NRE_EXT_S) - 1;
			}
			else
				extensible = EXTENSIBLE;
			GSTR (sp, slen, vtc, *ofs);
			if (!top_level && !strcmp (sp, "struct")) {
				snprintf (buffer, sizeof (buffer), "__struct_%u", anon_count++);
				sp = buffer;
			}
			ALIGN (*ofs, 4);
			nmembers = GUINT32 (vtc, *ofs);
			tp = xt_struct_type_create (lp, sp, nmembers, 0);
			if (!tp)
				return (NULL);

			if (!top_level || ext) {
				f = extensible;
				if (!top_level)
					f |= XTF_NESTED;
				xt_type_flags_modify (tp, XTF_EXT_MASK | XTF_NESTED, f);
			}
			for (i = 0; i < nmembers; i++) {
				*ofs += 2;	/* Skip member size. */
				if (ext) {
					member_id = GUINT16 (vtc, *ofs);
					member_id <<= 16;
				}
				else
					member_id = i;
				GSTR (msp, mslen, vtc, *ofs);
				ptr = vtc [(*ofs)++];
				ALIGN (*ofs, 2);
				bits = GUINT16 (vtc, *ofs);
				if (ext)
					member_id |= bits;
				else if (bits != 0xffff)
					warn_printf ("vtc_create_type: unknown bits combination!");

				key = vtc [(*ofs)++];
				etp = vtc_create_type (lp, vtc, ofs, dynamic, ext);
				if (!etp) {
					xt_type_delete (tp);
					return (NULL);
				}
				if (xt_struct_type_member_set (tp, i, msp,
							       member_id, etp, 0)) {
					xt_type_delete (etp);
					xt_type_delete (tp);
					return (NULL);
				}
				xt_type_delete (etp);
				if (ptr || key) {
					f = 0;
					if ((ptr & 1) != 0) {
						f |= XMF_SHAREABLE;
						*dynamic = 1;
					}
					if (ext) {
						if ((ptr & 2) != 0)
							f |= XMF_MUST_UNDERSTAND;
						if ((ptr & 4) != 0)
							f |= XMF_OPTIONAL;
					}
					if (key)
						f |= XMF_KEY;
					xt_type_member_flags_modify (tp, i, 
								     XMF_ALL, f);
				}
			}
			break;

		case VTC_UNION:
			if (ext) {
				bits = GUINT16 (vtc, *ofs);
				extensible = ((bits & NRE_EXT_M) >> NRE_EXT_S) - 1;
			}
			else
				extensible = EXTENSIBLE;
			GSTR (sp, slen, vtc, *ofs);
			if (!top_level && !strcmp (sp, "union")) {
				snprintf (buffer, sizeof (buffer), "__union_%u", anon_count++);
				sp = buffer;
			}
			ALIGN (*ofs, 4);
			def_index = (int32_t) GUINT32 (vtc, *ofs);
			disc_tp = vtc_create_type (lp, vtc, ofs, dynamic, ext);
			if (!disc_tp)
				return (NULL);

			ALIGN (*ofs, 4);
			nmembers = GUINT32 (vtc, *ofs);
			tp = xt_union_type_create (lp, sp, disc_tp, nmembers, 0);
			if (!tp) {
				xt_type_delete (disc_tp);
				return (NULL);
			}
			xt_type_delete (disc_tp);
			if (!top_level || extensible) {
				f = extensible;
				if (!top_level)
					f |= XTF_NESTED;
				xt_type_flags_modify (tp, XTF_EXT_MASK | XTF_NESTED, f);
			}
			max_labels = 0;
			lbp = NULL;
			for (i = 0; i < nmembers; i++) {
				*ofs += 2;	/* Skip member size. */
				if (ext) {
					member_id = GUINT32 (vtc, *ofs);
				}
				else
					member_id = i + 1;
				GSTR (msp, mslen, vtc, *ofs);
				ptr = vtc [(*ofs)++];
				ALIGN (*ofs, 4);
				nlabels = GUINT32 (vtc, *ofs);
				if (nlabels > MAX_LABELS) {
					if (nlabels > max_labels) {
						if (max_labels)
							nlbp = xrealloc (lbp, 
								sizeof (int32_t) * nlabels);
						else
							nlbp = xmalloc (sizeof (int32_t) * nlabels);
						if (!nlbp) {
							xt_type_delete (tp);
							xfree (lbp);
							return (NULL);
						}
						max_labels = nlabels;
						lbp = nlbp;
					}
					ip = lbp;
				}
				else if (nlabels)
					ip = labels;
				else
					ip = NULL;

				for (j = 0; j < nlabels; j++) {
					ip [j] = (int32_t) GUINT32 (vtc, *ofs);
				}
				etp = vtc_create_type (lp, vtc, ofs, dynamic, ext);
				if (!etp)
					goto cleanup;

				if (xt_union_type_member_set (tp, i + 1, nlabels,
							  ip, msp,
							  member_id, etp, 
							  (int) i == def_index,
							  0)) {
					xt_type_delete (etp);

				    cleanup:
					xt_type_delete (tp);
					if (lbp)
						xfree (lbp);
					return (NULL);
				}
				xt_type_delete (etp);
				if (ptr) {
					f = 0;
					if ((ptr & 1) != 0) {
						f |= XMF_SHAREABLE;
						*dynamic = 1;
					}
					if (ext) {
						if ((ptr & 2) != 0)
							f |= XMF_MUST_UNDERSTAND;
						if ((ptr & 4) != 0)
							f |= XMF_OPTIONAL;
					}
					xt_type_member_flags_modify (tp, i, 
								     XMF_ALL, f);
				}
			}
			if (lbp)
				xfree (lbp);
			break;

		case VTC_ENUM:
			GSTR (sp, slen, vtc, *ofs);
			ALIGN (*ofs, 4);
			nmembers = GUINT32 (vtc, *ofs);
			if (!nmembers)
				return (0);

			tp = xt_enum_type_create (lp, sp, 32, nmembers);
			if (!tp)
				return (NULL);

			for (i = 0; i < nmembers; i++) {
				*ofs += 2;	/* Skip member size. */
				GSTR (msp, mslen, vtc, *ofs);
				ALIGN (*ofs, 4);
				num = GUINT32 (vtc, *ofs);
				if (xt_enum_type_const_set (tp, i, msp, num)) {
					xt_type_delete (tp);
					return (NULL);
				}
			}
			break;

		case VTC_STRING:
		case VTC_WSTRING:
			ALIGN (*ofs, 4);
			slen = GUINT32 (vtc, *ofs);
			tp = xt_string_type_create (lp, slen, 
				   (tc == VTC_STRING) ? DDS_CHAR_8_TYPE :
							DDS_CHAR_32_TYPE);
			if (!tp)
				return (NULL);

			if (!slen)
				*dynamic = 1;
			break;

		case VTC_SEQUENCE:
			ALIGN (*ofs, 4);
			slen = GUINT32 (vtc, *ofs);
			etp = vtc_create_type (lp, vtc, ofs, dynamic, ext);
			if (!etp)
				return (NULL);

			tp = xt_sequence_type_create (lp, slen, etp, 0);
			if (!tp) {
				xt_type_delete (etp);
				return (NULL);
			}
			*dynamic = 1;
			break;

		case VTC_ARRAY:
			ALIGN (*ofs, 4);
			num = GUINT32 (vtc, *ofs);
			if (!num || num > 9)
				return (NULL);

			for (i = 0; i < num; i++) {
				bound [i] = GUINT32 (vtc, *ofs);
			}
			etp = vtc_create_type (lp, vtc, ofs, dynamic, ext);
			if (!etp)
				return (NULL);

			DDS_SEQ_INIT (bseq);
			bseq._length = bseq._maximum = num;
			bseq._buffer = bound;
			tp = xt_array_type_create (lp, &bseq, etp, 0);
			if (!tp) {
				xt_type_delete (etp);
				return (NULL);
			}
			break;

		default:
			return (NULL);
	}
	return (tp);
}

/* vtc_type -- Create a local type representation from vendor-specific typecode
	       data. */

TypeSupport_t *vtc_type (TypeLib *lp, unsigned char *vtc)
{
	Type		*tp;
	TypeSupport_t	*dds_ts;
	unsigned	ofs;
	char		*name, *cp;
	uint32_t	name_len;
	size_t		size;
	int		keys, fksize, dkeys, dynamic;
	uint16_t	ext;
	DDS_ReturnCode_t error;

	ofs = 0;
	dynamic = 0;
	memcpy (&ext, vtc + 6, 2);
	tp = vtc_create_type (lp, vtc, &ofs, &dynamic, (ext >> NRE_EXT_S) != 0);
	if (!tp)
		return (NULL);

	xt_type_finalize (tp, &size, &keys, &fksize, &dkeys, NULL);
	ofs = 8;
	GSTR (name, name_len, vtc, ofs);
	dds_ts = xmalloc (sizeof (TypeSupport_t) + name_len);
	if (!dds_ts) {
		xt_type_delete (tp);
		return (NULL);
	}
	memset (dds_ts, 0, sizeof (TypeSupport_t));
	cp = (char *) (dds_ts + 1);
	memcpy (cp, name, name_len);
	dds_ts->ts_name = cp;
	dds_ts->ts_keys = keys;
	dds_ts->ts_dynamic = dynamic;
	dds_ts->ts_origin = TSO_Typecode;
	dds_ts->ts_length = size;
	if (dds_ts->ts_keys)
		dds_ts->ts_fksize = fksize;
	dds_ts->ts_prefer = MODE_CDR;
	dds_ts->ts_cdr = tp;
	if (keys && !dkeys)
		dds_ts->ts_mkeysize = cdr_marshalled_size (4, NULL, tp, 0, 
							   1, 1, &error);
	dds_ts->ts_users = 1;
	return (dds_ts);
}

/* vtc_free -- Free typecode data. */

void vtc_free (unsigned char *vtc)
{
	VTC_Header_t	*p = (VTC_Header_t *) vtc;

	if ((p->nrefs_ext & NRE_NREFS) <= 1)
		xfree (p);
	else
		p->nrefs_ext--;
}

/* vtc_delete -- Delete typesupport data that was created from typecode. */

void vtc_delete (TypeSupport_t *ts)
{
	if (ts->ts_origin != TSO_Typecode)
		return;

	switch (ts->ts_prefer) {
		case MODE_V_TC:
			vtc_free ((unsigned char *) ts->ts_vtc);
			ts->ts_vtc = NULL;
			break;
		case MODE_CDR:
			xt_type_delete (ts->ts_cdr);
			ts->ts_cdr = NULL;
			break;
		default:
			warn_printf ("vtc_delete: incorrect type encoding!");
			return;
	}
	xfree (ts);
}

/* label_match -- Check if there is a label match between an existing union
		  member (*ump) and the union member we're validating (*ip, 
		  n). */

static int label_match (UnionMember *ump, int32_t *ip, unsigned n)
{
	unsigned	i, j;

	for (i = 0; i < n; i++, ip++) {
		if (ump->nlabels == 1) {
			if (ump->label.value == *ip)
				return (1);
		}
		else
			for (j = 0; j < ump->nlabels; j++)
				if (ump->label.list [j] == *ip)
					return (1);
	}
	return (0);
}

/* vtc_equal_type -- Compare a real local type with vendor typecode for either
		     equality (strict == 1) or compatibility (strict == 0). */

static int vtc_equal_type (Type                *tp,
			   const unsigned char *vtc,
			   unsigned            *ofs,
			   int                 strict,
			   int                 *same,
			   int                 ext)
{
	Type		*etp, *mtp;
	StructureType	*stp;
	Member		*mp;
	UnionType	*utp;
	UnionMember	*ump, *def_ump;
	EnumType	*ntp;
	EnumConst	*ecp;
	StringType	*itp;
	SequenceType	*qtp;
	ArrayType	*atp;
	char		*sp, *msp;
	unsigned	i, j, mofs, m, member_id;
	uint32_t	tc, slen, mslen, nmembers, def_index, num, nlabels,
			max_labels, next_ofs, matched_members, mu_members;
	int		ptr, key, rc, found;
	uint16_t	msize;
	uint16_t	bits;
	int32_t		*lbp, *nlbp, *ip, label, labels [MAX_LABELS];
	uint32_t	dim;

	ALIGN (*ofs, 4);
	tc = GUINT32 (vtc, *ofs);
	tc &= 0x7fffffff;
	*ofs += 2;	/* Skip type size. */
	switch (tc) {
		case VTC_SHORT:
			rc = (tp->kind == DDS_INT_16_TYPE);
			break;
		case VTC_LONG:
			rc = (tp->kind == DDS_INT_32_TYPE);
			break;
		case VTC_USHORT:
			rc = (tp->kind == DDS_UINT_16_TYPE);
			break;
		case VTC_ULONG:
			rc = (tp->kind == DDS_UINT_32_TYPE);
			break;
		case VTC_FLOAT:
			rc = (tp->kind == DDS_FLOAT_32_TYPE);
			break;
		case VTC_DOUBLE:
			rc = (tp->kind == DDS_FLOAT_64_TYPE);
			break;
		case VTC_BOOLEAN:
			rc = (tp->kind == DDS_BOOLEAN_TYPE);
			break;
		case VTC_CHAR:
			rc = (tp->kind == DDS_CHAR_8_TYPE);
			break;
		case VTC_OCTET:
			rc = (tp->kind == DDS_BYTE_TYPE);
			break;
		case VTC_LONGLONG:
			rc = (tp->kind == DDS_INT_64_TYPE);
			break;
		case VTC_ULONGLONG:
			rc = (tp->kind == DDS_UINT_64_TYPE);
			break;
		case VTC_LONGDOUBLE:
			rc = (tp->kind == DDS_FLOAT_128_TYPE);
			break;
		case VTC_WCHAR:
			rc = (tp->kind == DDS_CHAR_32_TYPE);
			break;

		case VTC_STRUCT:
			if (tp->kind != DDS_STRUCTURE_TYPE)
				return (0);

			stp = (StructureType *) tp;
			if (ext) {
				bits = GUINT16 (vtc, *ofs);
			}
			GSTR (sp, slen, vtc, *ofs);
			ALIGN (*ofs, 4);
			nmembers = GUINT32 (vtc, *ofs);
			if (strict && (stp->nmembers != nmembers ||
				strcmp (str_ptr (stp->type.name), sp)))
				return (0);

			matched_members = 0;
			mu_members = 0;
			for (j = 0, mp = stp->member;
			     j < stp->nmembers;
			     j++, mp++)
				if (mp->must_understand || mp->is_key)
					mu_members++;

			for (i = 0; i < nmembers; i++) {
				next_ofs = *ofs + 2;
				next_ofs += GUINT16 (vtc, *ofs);
				ALIGN (*ofs, 2);
				if (ext) {
					member_id = GUINT16 (vtc, *ofs);
					member_id <<= 16;
					ALIGN (*ofs, 4);
				}
				else
					member_id = i;
				GSTR (msp, mslen, vtc, *ofs);
				ptr = vtc [(*ofs)++];
				ALIGN (*ofs, 2);
				bits = GUINT16 (vtc, *ofs);
				if (ext)
					member_id |= bits;
				else if (bits != 0xffff)
					warn_printf ("vtc_create_type: unknown bits combination!");

				key = vtc [(*ofs)++];
				for (j = 0, mp = stp->member;
				     j < stp->nmembers;
				     j++, mp++)
					if (mp->member_id == member_id) {
						mtp = xt_real_type (
							xt_type_ptr (tp->scope,
								     mp->id));
						if (mp->is_key != key ||
						    mp->is_shareable != ((ptr & 1) != 0) ||
						    mp->must_understand != ((ptr & 2) != 0) ||
						    mp->is_optional != ((ptr & 4) != 0) ||
						    strcmp (str_ptr (mp->name), msp) ||
						    !vtc_equal_type (mtp, vtc, ofs, strict, same, ext))
							return (0);

						else {
							if (mp->must_understand ||
							    mp->is_key)
								mu_members --;
							matched_members++;
							break;
						}
					}

				if (j == stp->nmembers) {
					if (same)
						*same = 0;
					if ((ptr & 2) != 0)
						return (0);
				}
				*ofs = next_ofs;
			}
			if (nmembers != stp->nmembers && same)
				*same = 0;

			if (matched_members == 0 || mu_members > 0)
				return (0);

			rc = 1;
			break;

		case VTC_UNION:
			if (tp->kind != DDS_UNION_TYPE)
				return (0);

			utp = (UnionType *) tp;
			if (ext) {
				bits = GUINT16 (vtc, *ofs);
			}
			GSTR (sp, slen, vtc, *ofs);
			ALIGN (*ofs, 4);
			def_index = GUINT32 (vtc, *ofs);
			ALIGN (*ofs, 4);
			nmembers = GUINT32 (vtc, *ofs);
			ump = utp->member;
			mtp = xt_real_type (xt_type_ptr (tp->scope, ump->member.id));
			if (utp->nmembers != nmembers) {
				if (strict)
					return (0);

				else if (same)
					*same = 0;
			}
			if (strcmp (str_ptr (utp->type.name), sp) ||
			    !vtc_equal_type (mtp, vtc, ofs, strict, same, ext))
				return (0);

			max_labels = 0;
			lbp = NULL;
			for (i = 0; i < nmembers; i++) { /* For each member. */
				msize = GUINT16 (vtc, *ofs);
				mofs = *ofs;
				ALIGN (*ofs, 4);
				if (ext) {
					member_id = GUINT32 (vtc, *ofs);
				}
				else
					member_id = i;
				GSTR (msp, mslen, vtc, *ofs);
				ptr = vtc [(*ofs)++];
				ALIGN (*ofs, 4);
				nlabels = GUINT32 (vtc, *ofs);
				if (strict) {
					ump++;
					if (ump->nlabels != nlabels ||
					    ump->member.member_id != (i + 1) ||
					    strcmp (str_ptr (ump->member.name), msp))
						return (0);

					ip = NULL;
				}
				else if (nlabels > MAX_LABELS) {
					if (nlabels > max_labels) {
						if (max_labels)
							nlbp = xrealloc (lbp, 
								sizeof (int32_t) * nlabels);
						else
							nlbp = xmalloc (sizeof (int32_t) * nlabels);
						if (!nlbp) {
							if (lbp)
								xfree (lbp);
							return (0);
						}
						max_labels = nlabels;
						lbp = nlbp;
					}
					ip = lbp;
				}
				else if (nlabels)
					ip = labels;
				else
					ip = NULL;

				if (nlabels == 1) {
					label = (int32_t) GUINT32 (vtc, *ofs);
					if (!strict)
						ip [0] = label;
					else if (ump->label.value != label)
						return (0);
				}
				else
					for (j = 0; j < nlabels; j++) {
						label = (int32_t) GUINT32 (vtc, *ofs);
						if (!strict)
							ip [j] = label;
						else if (ump->label.list [j] != label)
							return (0);
					}
				if (strict) { /* Must be identical! */
					mtp = xt_real_type (xt_type_ptr (tp->scope, ump->member.id));
					if (ump->is_default != (def_index == member_id) ||
					    ptr != ump->member.is_shareable ||
					    !vtc_equal_type (mtp, vtc, ofs, 1, NULL, ext))
						return (0);
				}
				else {	/* Compatible if we don't find the
					   member in our local type, or
					   if we see it exactly once. */
					found = 0;
					def_ump = NULL;
					for (m = 0, ump = utp->member;
					     m < utp->nmembers;
					     m++, ump++) {
						if (ump->is_default)
							def_ump = ump;
						if (ump->nlabels && nlabels &&
						    label_match (ump, ip, nlabels)) {
							mtp = xt_real_type (xt_type_ptr (tp->scope, 
											 ump->member.id));
							if (found ||
							    strcmp (str_ptr (ump->member.name), msp) ||
							    ptr != ump->member.is_shareable ||
							    !vtc_equal_type (mtp, vtc, ofs, 0, same, ext)) {
								if (lbp)
									xfree (lbp);
								return (0);
							}
							found = 1;
						}
					}
					if (!found && def_ump) {
						mtp = xt_real_type (xt_type_ptr (tp->scope, 
										 def_ump->member.id));
						if (strcmp (str_ptr (def_ump->member.name), msp) ||
						    ptr != def_ump->member.is_shareable ||
						    !vtc_equal_type (mtp, vtc, ofs, 0, same, ext)) {
							if (lbp)
								xfree (lbp);
							return (0);
						}
					}
					if (same && *same &&
					    (!found || 
					     ump != &utp->member [i] ||
					     ump->nlabels != nlabels ||
					     (nlabels &&
					      !memcmp (ip, ump->label.list, 
					      	 nlabels * sizeof (int32_t)))))
						*same = 0;
				}

				/* In all cases, skip past the element. */
				*ofs = mofs + msize;
			}
			if (lbp)
				xfree (lbp);
			rc = 1;
			break;

		case VTC_ENUM:
			if (tp->kind != DDS_ENUMERATION_TYPE)
				return (0);

			ntp = (EnumType *) tp;
			GSTR (sp, slen, vtc, *ofs);
			ALIGN (*ofs, 4);
			nmembers = GUINT32 (vtc, *ofs);
			if (ntp->nconsts != nmembers ||
			    ntp->bound != 32 ||
			    (!tp->nested && strcmp (str_ptr (tp->name), sp)))
				return (0);

			for (i = 0, ecp = ntp->constant; i < nmembers; i++, ecp++) {
				*ofs += 2;	/* Skip member size. */
				ALIGN (*ofs, 4);
				GSTR (msp, mslen, vtc, *ofs);
				ALIGN (*ofs, 4);
				label = GUINT32 (vtc, *ofs);
				if (ecp->value != label ||
				    strcmp (str_ptr (ecp->name), msp))
					return (0);
			}
			rc = 1;
			break;

		case VTC_STRING:
		case VTC_WSTRING:
			if (tp->kind != DDS_STRING_TYPE)
				return (0);

			itp = (StringType *) tp;
			*ofs += 2;	/* Skip type size. */
			ALIGN (*ofs, 4);
			slen = GUINT32 (vtc, *ofs);
			if (itp->bound != slen ||
			    (tc == VTC_STRING && 
			     itp->collection.element_type != DDS_CHAR_8_TYPE) ||
			    (tc == VTC_WSTRING && 
			     itp->collection.element_type != DDS_CHAR_32_TYPE))
				return (0);

			rc = 1;
			break;

		case VTC_SEQUENCE:
			if (tp->kind != DDS_SEQUENCE_TYPE)
				return (0);

			qtp = (SequenceType *) tp;
			*ofs += 2;	/* Skip type size. */
			ALIGN (*ofs, 4);
			slen = GUINT32 (vtc, *ofs);
			etp = xt_real_type (xt_type_ptr (tp->scope, 
					        qtp->collection.element_type));
			if (qtp->bound != slen ||
			    !vtc_equal_type (etp, vtc, ofs, strict, same, ext))
				return (0);

			rc = 1;
			break;

		case VTC_ARRAY:
			if (tp->kind != DDS_ARRAY_TYPE)
				return (0);

			atp = (ArrayType *) tp;
			*ofs += 2;	/* Skip type size. */
			ALIGN (*ofs, 4);
			num = GUINT32 (vtc, *ofs);
			if (atp->nbounds != num)
				return (0);

			for (i = 0; i < num; i++) {
				dim = GUINT32 (vtc, *ofs);
				if (atp->bound [i] != dim)
					return (0);
			}
			etp = xt_real_type (xt_type_ptr (tp->scope, 
					        atp->collection.element_type));
			if (!vtc_equal_type (etp, vtc, ofs, strict, same, ext))
				return (0);

			rc = 1;
			break;

		default:
			rc = 0;
			break;
	}
	return (rc);
}

/* vtc_equal -- Compare two typecode types for equality. */

int vtc_equal (const unsigned char *vtc1, const unsigned char *vtc2)
{
	uint16_t	len1, len2;

	if (vtc1 == vtc2)
		return (1);

	memcpy (&len1, vtc1 + 4, 2);
	memcpy (&len2, vtc2 + 4, 2);
	return (len1 == len2 &&
	        !memcmp (vtc1 + 8, vtc2 + 8, len1 - 2));
}

/* vtc_identical -- Verify if the given types are identical and return a
		    non-0 result if so. */

int vtc_identical (const TypeSupport_t *ts, const unsigned char *vtc)
{
	Type 		*tp;
	unsigned	ofs;
	uint16_t	ext;

	if (!ts)
		return (1);

	if (ts->ts_prefer == MODE_V_TC) {
		if ((VTC_Header_t *) vtc == ts->ts_vtc)
			return (1);

		return (vtc_equal ((unsigned char *) ts->ts_vtc, vtc));
	}
	if (ts->ts_prefer == MODE_CDR)
		tp = ts->ts_cdr;
	else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
		tp = ts->ts_pl->xtype;
	else
		return (0);

	ofs = 0;
	memcpy (&ext, vtc + 6, 2);
	return (vtc_equal_type (tp, vtc, &ofs, 1, NULL, (ext >> NRE_EXT_S) != 0));
}

/* vtc_compatible -- Verify if the given types are compatible and return a
		     non-0 result if so. */

int vtc_compatible (TypeSupport_t *ts, const unsigned char *vtc, int *same)
{
	TypeLib		*lp;
	Type 		*tp;
	unsigned	ofs;
	TypeSupport_t	*nts;
	int		res;
	uint16_t	ext;

	if (!ts)
		return (1);

	if (same)
		*same = 1;

	if (ts->ts_prefer == MODE_V_TC) {
		if ((VTC_Header_t *) vtc == ts->ts_vtc)
			return (1);

		if (vtc_equal ((unsigned char *) ts->ts_vtc, vtc))
			return (1);

		/* Typecode differs, but might still be compatible ...
		   Since we don't support direct typecode compatibility checks,
		   we need to convert the existing typecode to a real type
		   before continuing with relaxed type compatibility checks. */
		lp = xt_lib_create (NULL);
		if (!lp)
			return (0);

		nts = vtc_type (lp, (unsigned char *) ts->ts_vtc);
		if (!nts) {
			if (same)
				*same = 0;
			xt_lib_delete (lp);
			return (0);
		}
		ofs = 0;
		memcpy (&ext, vtc + 6, 2);
		res = vtc_equal_type (nts->ts_cdr, vtc, &ofs, 0, same, 
							(ext >> NRE_EXT_S) != 0);
		vtc_delete (nts);
		xt_lib_delete (lp);
	}
	else {
		if (ts->ts_prefer == MODE_CDR)
			tp = ts->ts_cdr;
		else if (ts->ts_prefer == MODE_PL_CDR && !ts->ts_pl->builtin)
			tp = ts->ts_pl->xtype;
		else
			return (0);

		ofs = 0;
		memcpy (&ext, vtc + 6, 2);
		res = vtc_equal_type (tp, vtc, &ofs, 0, same, (ext >> NRE_EXT_S) != 0);
	}
	return (res);
}

#endif
