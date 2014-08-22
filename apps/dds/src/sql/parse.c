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

/* parse.c -- SQL subset command parser as used for the DDS SQL subset. */

#include <stdio.h>
#include <setjmp.h>
#include "scan.h"
#include "parse.h"
#ifdef XTYPES_USED
#include "dds/dds_tsm.h"
#include "type_data.h"
#include "xtypes.h"

#define	T_TYPECODE_SHORT	DDS_INT_16_TYPE
#define	T_TYPECODE_USHORT	DDS_UINT_16_TYPE
#define	T_TYPECODE_LONG		DDS_INT_32_TYPE
#define	T_TYPECODE_ULONG	DDS_UINT_32_TYPE
#define	T_TYPECODE_LONGLONG	DDS_INT_64_TYPE
#define	T_TYPECODE_ULONGLONG	DDS_UINT_64_TYPE
#define	T_TYPECODE_FLOAT	DDS_FLOAT_32_TYPE
#define	T_TYPECODE_DOUBLE	DDS_FLOAT_64_TYPE
#define	T_TYPECODE_LONGDOUBLE	DDS_FLOAT_128_TYPE
#define	T_TYPECODE_FIXED	DDS_NO_TYPE
#define	T_TYPECODE_BOOLEAN	DDS_BOOLEAN_TYPE
#define	T_TYPECODE_CHAR		DDS_CHAR_8_TYPE
#define	T_TYPECODE_WCHAR	DDS_CHAR_32_TYPE
#define	T_TYPECODE_OCTET	DDS_BYTE_TYPE
#define	T_TYPECODE_CSTRING	DDS_STRING_TYPE
#define	T_TYPECODE_WSTRING	DDS_STRING_TYPE
#define	T_TYPECODE_STRUCT	DDS_STRUCTURE_TYPE
#define	T_TYPECODE_UNION	DDS_UNION_TYPE
#define	T_TYPECODE_SEQUENCE	DDS_SEQUENCE_TYPE
#define	T_TYPECODE_ARRAY	DDS_ARRAY_TYPE
#define	T_TYPECODE_ENUM		DDS_ENUMERATION_TYPE
#define	T_TYPECODE_PAR		DDS_TYPEKIND_MAX

typedef DDS_TypeKind	T_TypeCode_t;
typedef Type		T_TypeSupport;

#define	typekind(t)	(t)->kind

#else

#include "cdr.h"

#define	T_TYPECODE_SHORT	CDR_TYPECODE_SHORT
#define	T_TYPECODE_USHORT	CDR_TYPECODE_USHORT
#define	T_TYPECODE_LONG		CDR_TYPECODE_LONG
#define	T_TYPECODE_ULONG	CDR_TYPECODE_ULONG
#define	T_TYPECODE_LONGLONG	CDR_TYPECODE_LONGLONG
#define	T_TYPECODE_ULONGLONG	CDR_TYPECODE_ULONGLONG
#define	T_TYPECODE_FLOAT	CDR_TYPECODE_FLOAT
#define	T_TYPECODE_DOUBLE	CDR_TYPECODE_DOUBLE
#define	T_TYPECODE_LONGDOUBLE	CDR_TYPECODE_LONGDOUBLE
#define	T_TYPECODE_FIXED	CDR_TYPECODE_FIXED
#define	T_TYPECODE_BOOLEAN	CDR_TYPECODE_BOOLEAN
#define	T_TYPECODE_CHAR		CDR_TYPECODE_CHAR
#define	T_TYPECODE_WCHAR	CDR_TYPECODE_WCHAR
#define	T_TYPECODE_OCTET	CDR_TYPECODE_OCTET
#define	T_TYPECODE_CSTRING	CDR_TYPECODE_CSTRING
#define	T_TYPECODE_WSTRING	CDR_TYPECODE_WSTRING
#define	T_TYPECODE_STRUCT	CDR_TYPECODE_STRUCT
#define	T_TYPECODE_UNION	CDR_TYPECODE_UNION
#define	T_TYPECODE_SEQUENCE	CDR_TYPECODE_SEQUENCE
#define	T_TYPECODE_ARRAY	CDR_TYPECODE_ARRAY
#define	T_TYPECODE_ENUM		CDR_TYPECODE_ENUM
#define	T_TYPECODE_PAR		CDR_TYPECODE_MAX

typedef CDR_TypeCode_t	T_TypeCode_t;
typedef CDR_TypeSupport	T_TypeSupport;

#define	typekind(t)	(t)->typecode

#endif

#define PARSE_SHOW_ERRS	/* Display parser errors. */
/*#define TOKEN_TRACE	** Display tokens. */

#define	BC_INC			64	/* Min./Incremental # of bytes in bytecode array. */


typedef enum {
	PE_OK,
	PE_SYNTAX,
	PE_INC_TYPES,
	PE_NOMEM,
	PE_UNKN_NAME,
	PE_INV_COMP,
	PE_INT_ERR
} ParseError;

typedef enum {
	PT_TOPIC,
	PT_QUERY,
	PT_FILTER
} ParseType;

typedef struct parse_ctxt_st {
	ParseType		type;
	ScanData		scan;
	unsigned char		*bc_data;
	unsigned		bc_size;
	unsigned		bc_left;
	unsigned		bc_npars;
	unsigned char		*bcp;
	const TypeSupport_t	*tc;
	jmp_buf			jbuf;
	ParseError		error;
	char			err_arg [48];
} ParseData;

typedef struct patch_node_st PatchNode;
struct patch_node_st {
	PatchNode		*next;		/* Next node in chain. */
	unsigned		inst_ofs;	/* Branch instruction offset. */
};

typedef struct patch_list_st {
	PatchNode		*head;
	PatchNode		*tail;
} PatchList;

typedef enum {
	C_EQ,
	C_NE,
	C_GT,
	C_LE,
	C_LT,
	C_GE,
	C_T,
	C_F
} Cond;

typedef enum {
	VT_U32,
	VT_I32,
	VT_U64,
	VT_I64,
	VT_DOUBLE,
	VT_LONGDOUBLE,
	VT_STRING
} ValueType;

typedef struct data_object_st {
	T_TypeCode_t	type;
	PatchList	tchain;
	PatchList	fchain;
	Cond		condition;
} DataObject;

typedef struct cond_object_st {
	PatchList	out_list;
	PatchList	else_list;
} CondObject;

const char *parse_errors [] = {
	"no error",
	"'%s' expected",
	"incompatible types",
	"out of memory",
	"unknown field/constant name '%s'",
	"invalid operator",
	"internal error"
};

static void show_error (ParseError err, const char *arg)
{
	printf ("Parse error: ");
	if (arg)
		printf (parse_errors [err], arg);
	else
		printf ("%s", parse_errors [err]);
	printf ("\r\n");
}

static void parse_error (ParseData *pdp, ParseError err, const char *arg)
{
	unsigned	n;

	pdp->error = err;
	if (arg) {
		n = strlen (arg);
		if (n >= sizeof (pdp->err_arg)) {
			n = sizeof (pdp->err_arg) - 1;
			pdp->err_arg [sizeof (pdp->err_arg) - 1] = '\0';
		}
		memcpy (pdp->err_arg, arg, n + 1);
	}
	else
		pdp->err_arg [0] = '\0';

#ifdef PARSE_SHOW_ERRS
	show_error (err, pdp->err_arg [0] ? pdp->err_arg : NULL);
#endif
	longjmp (pdp->jbuf, err);
}

#ifdef TOKEN_TRACE

static void next_token (ScanData *sdp)
{
	sql_next_token (sdp);
	sql_dump_token (sdp);
}

#else
#define	next_token(sdp)	sql_next_token(sdp)
#endif

static void expect (ParseData *pdp, Token t)
{
	if (pdp->scan.token != t) {
		parse_error (pdp, PE_SYNTAX, sql_token_str (t));
		return;
	}
	next_token (&pdp->scan);
}

static void subject_field_spec (ParseData *pdp)
{
	expect (pdp, TK_ID);
	if (pdp->scan.token == TK_AS) {
		next_token (&pdp->scan);
		expect (pdp, TK_ID);
	}
	else if (pdp->scan.token == TK_ID)
		next_token (&pdp->scan);
}

static void aggregation (ParseData *pdp)
{
	if (pdp->scan.token == TK_ALL)
		next_token (&pdp->scan);
	else {
		subject_field_spec (pdp);
		while (pdp->scan.token == TK_COMMA) {
			next_token (&pdp->scan);
			subject_field_spec (pdp);
		}
	}
}

static void natural_join (ParseData *pdp)
{
	if (pdp->scan.token == TK_INNER) {
		next_token (&pdp->scan);
		expect (pdp, TK_NATURAL);
	}
	else {
		expect (pdp, TK_NATURAL);
		if (pdp->scan.token == TK_INNER)
			next_token (&pdp->scan);
	}
	expect (pdp, TK_JOIN);
}

static void selection (ParseData *pdp);

static void join_item (ParseData *pdp)
{
	if (pdp->scan.token == TK_LPAR) {
		selection (pdp);
		expect (pdp, TK_RPAR);
	}
	else
		selection (pdp);
}

static void selection (ParseData *pdp)
{
	if (pdp->scan.token != TK_PARAM && pdp->scan.ptype != PT_STRING) {
		pdp->error = 1;
		return;
	}
	next_token (&pdp->scan);
	if (pdp->scan.token == TK_INNER || pdp->scan.token == TK_NATURAL) {
		natural_join (pdp);
		join_item (pdp);
	}
}

static void select_from (ParseData *pdp)
{
	expect (pdp, TK_SELECT);
	aggregation (pdp);
	expect (pdp, TK_FROM);
	selection (pdp);
}

static void emitx (ParseData *pdp, unsigned char c)
{
	if (pdp->error)
		return;

	if (!pdp->bc_left) {
		if (!pdp->bc_size)
			pdp->bc_data = xmalloc (BC_INC);
		else
			pdp->bc_data = xrealloc (pdp->bc_data, pdp->bc_size + BC_INC);

		if (!pdp->bc_data) {
			parse_error (pdp, PE_NOMEM, NULL);
			return;
		}
		pdp->bcp = pdp->bc_data + pdp->bc_size;
		pdp->bc_size += BC_INC;
		pdp->bc_left = BC_INC;
	}
	*pdp->bcp++ = c;
	pdp->bc_left--;
}

#define new_list(l)	(l)->head = (l)->tail = NULL

static void append (ParseData *pdp, PatchList *l, unsigned ofs)
{
	PatchNode	*p;

	p = xmalloc (sizeof (PatchNode));
	if (!p) {
		parse_error (pdp, PE_NOMEM, NULL);
		return;
	}
	p->inst_ofs = ofs;
	p->next = NULL;
	if (l->head)
		l->tail->next = p;
	else
		l->head = p;
	l->tail = p;
}

static void backpatch (ParseData *pdp, PatchList *l, unsigned ofs)
{
	PatchNode	*p;

	for (p = l->head; p; p = p->next) {
		pdp->bc_data [p->inst_ofs + 1] = ofs >> 8;
		pdp->bc_data [p->inst_ofs + 2] = ofs & 0xff;
	}
}

static void merge_patches (PatchList *s1, PatchList *s2)
{
	if (s1->head) {
		if (s2->head) {
			s1->tail->next = s2->head;
			s1->tail = s2->tail;
		}
	}
	else
		*s1 = *s2;
}

static void free_patches (PatchList *l)
{
	PatchNode	*p, *next_p;

	for (p = l->head; p; p = next_p) {
		next_p = p->next;
		xfree (p);
	}
	l->head = l->tail = NULL;
}

#if 0	/* No real need to optimise this, SQL compilation is infrequent. */
#define emit(pdp,c)	if (!pdp->error && pdp->bc_left) { 		\
				*pdp->bcp++ = c; pdp->bc_left--;	\
			} else emitx (pdp, c)
#else
#define	emit	emitx
#endif

#define	emits(pdp,s)	emit (pdp, (s) >> 8); emit (pdp, (s) & 0xff)
#define	emitl(pdp,l)	emit (pdp, (l) >> 24); emit (pdp, (l) >> 16); \
			emit (pdp, (l) >> 8); emit (pdp, (l) & 0xff)

static void const_int (ParseData *pdp, int64_t i, T_TypeCode_t *itype)
{
	if (i >= -64 && i <= 63) {
		emit (pdp, i & 0x7f);
		*itype = T_TYPECODE_LONG;
	}
	else if (i >= 0 && i <= 255) {
		emit (pdp, O_LCBU);
		emit (pdp, (unsigned) i);
		*itype = T_TYPECODE_ULONG;
	}
	else if (i >= -128 && i < 0) {
		emit (pdp, O_LCBS);
		emit (pdp, (unsigned) i);
		*itype = T_TYPECODE_LONG;
	}
	else if (i >= 0 && i <= 0xffffU) {
		emit (pdp, O_LCSU);
		emits (pdp, (unsigned) i);
		*itype = T_TYPECODE_ULONG;
	}
	else if (i >= -32768 && i < 0) {
		emit (pdp, O_LCSS);
		emits (pdp, (unsigned) i);
		*itype = T_TYPECODE_LONG;
	}
	else if (i >= 0 && i <= 0xffffffffU) {
		emit (pdp, O_LCWU);
		emitl (pdp, (unsigned) i);
		*itype = T_TYPECODE_ULONG;
	}
	else if (i >= -2147483648LL && i < 0) {
		emit (pdp, O_LCWS);
		emitl (pdp, (unsigned) i);
		*itype = T_TYPECODE_LONG;
	}
	else {
		emit (pdp, O_LCL);
		emit (pdp, (( i) >> 0) & 0xff);
		emit (pdp, (( i) >> 8) & 0xff);
		emit (pdp, (( i) >> 16) & 0xff);
		emit (pdp, (( i) >> 24) & 0xff);
		emit (pdp, (( i) >> 32) & 0xff);
		emit (pdp, (( i) >> 40) & 0xff);
		emit (pdp, (( i) >> 48) & 0xff);
		emit (pdp, (( i) >> 56) & 0xff);
		*itype = T_TYPECODE_LONGLONG;
	}
}

static void const_float (ParseData *pdp, double f)
{
	union tu {
	  double	f;
	  unsigned char	c [sizeof (double)];
	} u;
	unsigned	i;

	emit (pdp, O_LCD);
	u.f = f;
	for (i = 0; i < sizeof (double); i++)
		emit (pdp, u.c [i]);
}

static void const_string (ParseData *pdp, char *s)
{
	unsigned	n;

	emit (pdp, O_LCS);
	for (n = 0; n <= strlen (s); n++)
		emit (pdp, s [n]);
}

#ifdef XTYPES_USED

static const T_TypeSupport *field_lookup (const T_TypeSupport *tp,
					  const char          *name,
					  unsigned            *offset)
{
	const StructureType	*cp = (const StructureType *) tp;
	const Member		*mp;
	unsigned		i;

	for (i = 0, mp = cp->member; i < cp->nmembers; i++, mp++)
		if (!strcmp (name, str_ptr (mp->name))) {
			*offset = i;
			return (xt_type_ptr (tp->scope, mp->id));
		}

	return (NULL);
}
	
static int enum_lookup (const T_TypeSupport *tp,
			const char          *name,
			int                 *value)
{
	const EnumType	*ep = (const EnumType *) tp;
	const EnumConst	*cp;
	unsigned	i;

	if (!tp || tp->kind != DDS_ENUMERATION_TYPE)
		return (DDS_RETCODE_ALREADY_DELETED);

	for (i = 0, cp = ep->constant; i < ep->nconsts; i++, cp++)
		if (!strcmp (name, str_ptr (cp->name))) {
			*value = cp->value;
			return (DDS_RETCODE_OK);
		}

	return (DDS_RETCODE_ALREADY_DELETED);
}

#else

static const T_TypeSupport *field_lookup (const T_TypeSupport *tp,
					  const char          *name,
					  unsigned            *offset)
{
	const CDR_TypeSupport_struct	*cp;
	unsigned			i;

	cp = (const CDR_TypeSupport_struct *) tp;
	for (i = 0; i < cp->container.numelems; i++, tp++)
		if (!strcmp (name, cp->elements [i].ts.name)) {
			*offset = i;
			return ((T_TypeSupport *) &cp->elements [i]);
		}

	return (NULL);
}

#define container_type(t) ((t) == CDR_TYPECODE_STRUCT   || (t) == CDR_TYPECODE_UNION || \
			   (t) == CDR_TYPECODE_SEQUENCE || (t) == CDR_TYPECODE_ARRAY || \
			   (t) == CDR_TYPECODE_ENUM)

static const CDR_TypeSupport *enum_cont_find (const CDR_TypeSupport_container *cp,
					      const char                      *name,
					      int                             *value)
{
	const CDR_TypeSupport_container	 *ncp;
	const CDR_TypeSupport_struct	 *sp;
	const CDR_TypeSupport_structelem *sep;
	const CDR_TypeSupport_array	 *ap;
	const CDR_TypeSupport		 *tp;
	unsigned			 i;

	if (cp->ts.typecode == CDR_TYPECODE_STRUCT ||
	    cp->ts.typecode == CDR_TYPECODE_UNION ||
	    cp->ts.typecode == CDR_TYPECODE_ENUM) {
		sp = (const CDR_TypeSupport_struct *) cp;
		for (i = 0, sep = sp->elements; i < sp->container.numelems; i++, sep++) {
			tp = &sep->ts;
			if (container_type (tp->typecode)) {
				ncp = (const CDR_TypeSupport_container *) tp->ts;
				if (enum_cont_find (ncp, name, value))
					return (tp);
			}
			else if (cp->ts.typecode == CDR_TYPECODE_ENUM &&
				 tp->typecode == CDR_TYPECODE_LONG) {
				if (!strcmp (tp->name, name)) {
					*value = sep->label;
					return (tp);
				}
			}
		}
	}
	else if (cp->ts.typecode == CDR_TYPECODE_ARRAY ||
		 cp->ts.typecode == CDR_TYPECODE_SEQUENCE) {
		ap = (CDR_TypeSupport_array *) cp;
		if (container_type (ap->el_ts.typecode)) {
			ncp = (const CDR_TypeSupport_container *) ap->el_ts.ts;
			if (enum_cont_find (ncp, name, value))
				return (&ncp->ts);
		}
	}
	return (NULL);
}

static int enum_lookup (const DDS_TypeSupport *ts,
			const char            *name,
			int                   *value)
{
	const T_TypeSupport	*tp;

	tp = ts->ts_cdr;
	if (!tp || tp->typecode != CDR_TYPECODE_STRUCT)
		return (DDS_RETCODE_ALREADY_DELETED);

	if (enum_cont_find ((CDR_TypeSupport_container *) tp, name, value))
		return (DDS_RETCODE_OK);
	else
		return (DDS_RETCODE_ALREADY_DELETED);
}

#endif

static void identifier (ParseData *pdp, T_TypeCode_t *idtype, int enum_ok)
{
	const T_TypeSupport	*tcp, *ntcp;
	unsigned		ofs;
	int			v;

	tcp = pdp->tc->ts_cdr;
	for (;;) {
		ntcp = field_lookup (tcp, pdp->scan.ident, &ofs);
		if (!ntcp) {
			if (!enum_ok ||
			    enum_lookup (
#ifdef XTYPES_USED
					 tcp,
#else
			    		 pdp->tc, 
#endif
					 pdp->scan.ident, &v))
				parse_error (pdp, PE_UNKN_NAME, pdp->scan.ident);
			else
				const_int (pdp, v, idtype);
			return;
		}
		tcp = ntcp;
		if (typekind (tcp) == T_TYPECODE_STRUCT) {
			emit (pdp, O_CREF);
			emits (pdp, ofs);
			next_token (&pdp->scan);
			expect (pdp, TK_DOT);
			if (pdp->scan.token == TK_ID) {
#ifndef XTYPES_USED
				tcp = tcp->ts;
#endif
				continue;
			}
			pdp->error = 1;
			parse_error (pdp, PE_SYNTAX, "field/struct name");
			return;
		}
		if (typekind (tcp) == T_TYPECODE_UNION ||
		    typekind (tcp) == T_TYPECODE_SEQUENCE ||
		    typekind (tcp) == T_TYPECODE_ARRAY) {
			parse_error (pdp, PE_INC_TYPES, NULL);
			return;
		}
		switch (typekind (tcp)) {
			case T_TYPECODE_SHORT:
				emit (pdp, O_LDSS);
				break;
			case T_TYPECODE_USHORT:
				emit (pdp, O_LDSU);
				break;
			case T_TYPECODE_LONG:
			case T_TYPECODE_ENUM:
				emit (pdp, O_LDWS);
				break;
			case T_TYPECODE_WCHAR:
			case T_TYPECODE_ULONG:
				emit (pdp, O_LDWU);
				break;
			case T_TYPECODE_LONGLONG:
			case T_TYPECODE_ULONGLONG:
				emit (pdp, O_LDL);
				break;
			case T_TYPECODE_FLOAT:
				emit (pdp, O_LDF);
				break;
			case T_TYPECODE_DOUBLE:
				emit (pdp, O_LDD);
				break;
#ifdef LONGDOUBLE
			case T_TYPECODE_LONGDOUBLE:
				emit (pdp, O_LDO);
				break;
#endif
			case T_TYPECODE_BOOLEAN:
			case T_TYPECODE_OCTET:
				emit (pdp, O_LDBU);
				break;
			case T_TYPECODE_CHAR:
				emit (pdp, O_LDBS);
				break;
			case T_TYPECODE_CSTRING:
#ifndef XTYPES_USED
			case T_TYPECODE_WSTRING:
#endif
				emit (pdp, O_LDS);
				break;
			default:
				parse_error (pdp, PE_INC_TYPES, NULL);
				return;
		}
		*idtype = typekind (tcp);
		emits (pdp, ofs);
		return;
	}
}

static void parameter (ParseData *pdp, T_TypeCode_t *type)
{
	switch (pdp->scan.ptype) {
		case PT_INT:
			const_int (pdp, pdp->scan.integer, type);
			break;
		case PT_CHAR:
			const_int (pdp, (unsigned) pdp->scan.character, type);
			*type = T_TYPECODE_CHAR;
			break;
		case PT_FLOAT:
			const_float (pdp, pdp->scan.float_num);
			*type = T_TYPECODE_DOUBLE;
			break;
		case PT_STRING:
			const_string (pdp, pdp->scan.string);
			*type = T_TYPECODE_CSTRING;
			break;
		case PT_PARAM:
			emit (pdp, O_LPS);
			emit (pdp, pdp->scan.par_index);
			*type = T_TYPECODE_PAR;
			if (pdp->scan.par_index >= pdp->bc_npars)
				pdp->bc_npars = pdp->scan.par_index + 1;
			break;
		default:
			pdp->error = 1;
			parse_error (pdp, PE_INC_TYPES, NULL);
			break;
	}
}

static void id_or_parameter (ParseData *pdp, T_TypeCode_t *type)
{
	if (pdp->scan.token == TK_ID)
		identifier (pdp, type, 1);
	else if (pdp->scan.token == TK_PARAM)
		parameter (pdp, type);
	else {
		expect (pdp, TK_ID);
		return;
	}
}

#define is_string(t)	((t) == T_TYPECODE_CSTRING || (t) == T_TYPECODE_WSTRING) 
#define is_char(t)	((t) == T_TYPECODE_CHAR || (t) == T_TYPECODE_WCHAR) 

static void check_types (ParseData *pdp, T_TypeCode_t t1, T_TypeCode_t t2)
{
	if (t1 == T_TYPECODE_PAR || t2 == T_TYPECODE_PAR)
		return;

	if ((is_string(t1) && is_char (t2))  || (is_string(t2) && is_char (t1))) 
		return;

	if (is_string (t1) != is_string (t2))
		parse_error (pdp, PE_INC_TYPES, NULL);
}

static unsigned type_size (ParseData *pdp, T_TypeCode_t t)
{
	switch (t) {
		case T_TYPECODE_OCTET:
		case T_TYPECODE_BOOLEAN:
		case T_TYPECODE_CHAR:
			/*return (1);*/	/* -> casted implicitly to int32! */
		case T_TYPECODE_USHORT:
		case T_TYPECODE_SHORT:
			/*return (2);*/	/* -> casted implicitly to int32! */

		case T_TYPECODE_ULONG:
		case T_TYPECODE_LONG:
		case T_TYPECODE_ENUM:
			return (4);
		case T_TYPECODE_ULONGLONG:
		case T_TYPECODE_LONGLONG:
			return (8);
		case T_TYPECODE_FLOAT:
			/*return (10);*/ /* -> casted implicitly to double! */
		case T_TYPECODE_DOUBLE:
			return (12);
#ifdef LONGDOUBLE
		case T_TYPECODE_LONGDOUBLE:
			return (16);
#endif
		case T_TYPECODE_CSTRING:
		case T_TYPECODE_PAR:
			return (0);
		default:
			parse_error (pdp, PE_INC_TYPES, NULL);
			break;
	}
	return (0);
}

static void emit_compare (ParseData *pdp, T_TypeCode_t t)
{
	switch (t) {
		case T_TYPECODE_OCTET:
		case T_TYPECODE_BOOLEAN:
		case T_TYPECODE_USHORT:
		case T_TYPECODE_ULONG:
			emit (pdp, O_CMPWU);
			break;
		case T_TYPECODE_CHAR:
		case T_TYPECODE_WCHAR:
		case T_TYPECODE_SHORT:
		case T_TYPECODE_LONG:
		case T_TYPECODE_ENUM:
			emit (pdp, O_CMPWS);
			break;
		case T_TYPECODE_ULONGLONG:
			emit (pdp, O_CMPLU);
			break;
		case T_TYPECODE_LONGLONG:
			emit (pdp, O_CMPLS);
			break;
		case T_TYPECODE_FLOAT:
		case T_TYPECODE_DOUBLE:
			emit (pdp, O_CMPD);
			break;
#ifdef LONGDOUBLE
		case T_TYPECODE_LONGDOUBLE:
			emit (pdp, O_CMPO);
			break;
#endif
		case T_TYPECODE_CSTRING:
#ifndef XTYPES_USED
		case T_TYPECODE_WSTRING:
#endif
			emit (pdp, O_CMPS);
			break;
		default:
			parse_error (pdp, PE_INC_TYPES, NULL);
			break;
	}
}

static unsigned relop_set = (1 << TK_EQ) | (1 << TK_GT) | (1 << TK_GE) |
			    (1 << TK_LT) | (1 << TK_LE) | (1 << TK_NE) |
			    (1 << TK_LIKE);

#define cur_pc(d)		(d)->bc_size - (d)->bc_left
#define	set_at_pc(d,ofs,x)	(d)->bc_data [ofs] = x
#define	get_at_pc(d,ofs)	(d)->bc_data [ofs]

static void ins_before (ParseData *pdp, unsigned ofs, unsigned char x)
{
	unsigned	i;

	emit (pdp, O_NOP);
	for (i = cur_pc (pdp); i > ofs; i--)
		set_at_pc (pdp, i, get_at_pc (pdp, i - 1));
	set_at_pc (pdp, ofs, x);
}

static unsigned char convertn (T_TypeCode_t t1, T_TypeCode_t t2)
{
	unsigned char	cc = O_NOP;

	switch (t1) {
		case T_TYPECODE_BOOLEAN:
		case T_TYPECODE_OCTET:
		case T_TYPECODE_USHORT:
		case T_TYPECODE_ULONG:
			if (t2 == T_TYPECODE_LONGLONG || t2 == T_TYPECODE_ULONGLONG)
				cc = O_WU2L;
			else if (t2 == T_TYPECODE_FLOAT || t2 == T_TYPECODE_DOUBLE)
				cc = O_WU2D;
#ifdef LONGDOUBLE
			else if (t2 == T_TYPECODE_LONGDOUBLE)
				cc = O_WU2O;
#endif
			break;
		case T_TYPECODE_CHAR:
		case T_TYPECODE_WCHAR:
		case T_TYPECODE_SHORT:
		case T_TYPECODE_LONG:
			if (t2 == T_TYPECODE_LONGLONG || t2 == T_TYPECODE_ULONGLONG)
				cc = O_WS2L;
			else if (t2 == T_TYPECODE_FLOAT || t2 == T_TYPECODE_DOUBLE)
				cc = O_WS2D;
#ifdef LONGDOUBLE
			else if (t2 == T_TYPECODE_LONGDOUBLE)
				cc = O_WS2O;
#endif
			break;
		case T_TYPECODE_ULONGLONG:
			if (t2 == T_TYPECODE_FLOAT || t2 == T_TYPECODE_DOUBLE)
				cc = O_LU2D;
#ifdef LONGDOUBLE
			else if (t2 == T_TYPECODE_LONGDOUBLE)
				cc = O_LU2O;
#endif
			break;
		case T_TYPECODE_LONGLONG:
			if (t2 == T_TYPECODE_FLOAT || t2 == T_TYPECODE_DOUBLE)
				cc = O_LS2D;
#ifdef LONGDOUBLE
			else if (t2 == T_TYPECODE_LONGDOUBLE)
				cc = O_LS2O;
			break;
		case T_TYPECODE_FLOAT:
		case T_TYPECODE_DOUBLE:
			if (t2 == T_TYPECODE_LONGDOUBLE)
				cc = O_D2O;
#endif
			break;
		default:
			break;
	}
	return (cc);
}

/* Why do we need this? Is it supported?
   
static unsigned char converts (T_TypeCode_t t)
{
	unsigned char	cc = O_NOP;

	switch (t) {
		case T_TYPECODE_BOOLEAN:
		case T_TYPECODE_OCTET:
		case T_TYPECODE_USHORT:
		case T_TYPECODE_ULONG:
		case T_TYPECODE_CHAR:
		case T_TYPECODE_WCHAR:
		case T_TYPECODE_SHORT:
		case T_TYPECODE_LONG:
			cc = O_S2W;
			break;
		case T_TYPECODE_ULONGLONG:
		case T_TYPECODE_LONGLONG:
			cc = O_S2L;
			break;
		case T_TYPECODE_FLOAT:
		case T_TYPECODE_DOUBLE:
			cc = O_S2D;
			break;
#ifdef LONGDOUBLE
		case T_TYPECODE_LONGDOUBLE:
			cc = O_S2O;
			break;
#endif
		default:
	  		break;
	}
	return (cc);
} */

static unsigned char lpar_type (T_TypeCode_t t)
{
	static const unsigned char lp_code [] = {
#ifdef XTYPES_USED
		O_NOP,
/*BOOLEAN*/	O_LPBU,
/*OCTET*/	O_LPBU,
/*SHORT*/	O_LPSS,
/*USHORT*/	O_LPSU,
/*LONG*/	O_LPWS,
/*ULONG*/	O_LPWU,
/*LONGLONG*/	O_LPL,
/*ULONGLONG*/	O_LPL,
/*FLOAT*/	O_LPF,
/*DOUBLE*/	O_LPD,
/*LONGDOUBLE*/	O_LPO,
/*CHAR*/	O_LPBS,
/*WCHAR*/	O_LPWS,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP,
/*CSTRING*/	O_LPS,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP
#else
		O_NOP,
/*SHORT*/	O_LPSS,
/*USHORT*/	O_LPSU,
/*LONG*/	O_LPWS,
/*ULONG*/	O_LPWU,
/*LONGLONG*/	O_LPL,
/*ULONGLONG*/	O_LPL,
/*FLOAT*/	O_LPF,
/*DOUBLE*/	O_LPD,
#ifdef LONGDOUBLE
/*LONGDOUBLE*/	O_LPO,
#endif
/*FIXED*/	O_NOP,
/*BOOLEAN*/	O_LPBU,
/*CHAR*/	O_LPBS,
/*WCHAR*/	O_LPWS,
/*OCTET*/	O_LPBU,
/*CSTRING*/	O_LPS,
/*WSTRING*/	O_LPS,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP,
		O_NOP
#endif
	};

	return (lp_code [t]);
}

static void comp_predicate (ParseData    *pdp,
			    T_TypeCode_t t1,
			    unsigned     t1_ofs,
			    DataObject   *op)
{
	Token		cmp;
	unsigned	t2_ofs, ts1, ts2;
	T_TypeCode_t	t, t2;
	
	cmp = pdp->scan.token;
	next_token (&pdp->scan);
	t2_ofs = cur_pc (pdp);

	id_or_parameter (pdp, &t2);
	next_token (&pdp->scan);
	if (pdp->error)
		return;

	check_types (pdp, t1, t2);
	ts1 = type_size (pdp, t1);
	ts2 = type_size (pdp, t2);
	if (t1 == T_TYPECODE_PAR) {
		if (t2 == T_TYPECODE_PAR || is_string (t2))
			t = T_TYPECODE_CSTRING;
		else {
			set_at_pc (pdp, t1_ofs, lpar_type (t2));
			t = t2;
		}
	}
	else if (t2 == T_TYPECODE_PAR) {
		set_at_pc (pdp, t2_ofs, lpar_type (t1));
		t = t1;
	}
	else if (is_string(t2) && is_char(t1)) {
		t = t2;
		ins_before (pdp, t2_ofs, O_C2S);
	}
	else if (is_string(t1) && is_char(t2)) {
		t = t1;
		emit (pdp, O_C2S);
	} 
	else if (ts1 > ts2) {
		t = t1;
		emit (pdp, convertn (t2, t1));
	}
	else if (ts1 < ts2) {
		t = t2;
		ins_before (pdp, t2_ofs, convertn (t1, t2));
	}
	else
		t = t1;

	if (cmp == TK_LIKE) {
		if (!is_string (t1) || !is_string (t2))
			parse_error (pdp, PE_INC_TYPES, NULL);

		emit (pdp, O_LIKE);
	}
	else
		emit_compare (pdp, t);

	new_list (&op->fchain);
	new_list (&op->tchain);
	switch (cmp) {
		case TK_EQ:
			op->condition = C_NE;
			break;
		case TK_NE:
			op->condition = C_EQ;
			break;
		case TK_GT:
			op->condition = C_LE;
			break;
		case TK_GE:
			op->condition = C_LT;
			break;
		case TK_LT:
			op->condition = C_GE;
			break;
		case TK_LE:
			op->condition = C_GT;
			break;
		case TK_LIKE:
			op->condition = C_F;
			break;
		default:
			parse_error (pdp, PE_INV_COMP, NULL);
	  		break;
	}
}

static void between_predicate (ParseData    *pdp,
			       T_TypeCode_t t,
			       unsigned     t_ofs,
			       DataObject   *op)
{
	T_TypeCode_t	tr1, tr2;
	unsigned	ts, ts1, ts2, t1_ofs, t2_ofs;
	int		negate;

	if (pdp->scan.token == TK_NOT) {
                next_token (&pdp->scan);
		expect (pdp, TK_BETWEEN);
		negate = 1;
	}
	else {
		next_token (&pdp->scan);
		negate = 0;
	}
	ts = type_size (pdp, t);
	t1_ofs = cur_pc (pdp);
	id_or_parameter (pdp, &tr1);
	check_types (pdp, t, tr1);
	ts1 = type_size (pdp, tr1);
	if ((tr1 == T_TYPECODE_PAR) && (t == T_TYPECODE_PAR)) {
                t = T_TYPECODE_CSTRING;
                set_at_pc (pdp, t_ofs, lpar_type (t));
	}
	else if (tr1 == T_TYPECODE_PAR) {
		set_at_pc (pdp, t1_ofs, lpar_type (t));
	}
        /* Cannot happen, as check_types prevents this 
	else if (t == T_TYPECODE_CSTRING && tr1 != T_TYPECODE_CSTRING) {
		ins_before (pdp, t1_ofs, converts (tr1));
	}
	else if (tr1 == T_TYPECODE_CSTRING && t != T_TYPECODE_CSTRING) {
		t = tr1;
		emit (pdp, converts (t));
	} */
	else if (ts > ts1) {
		emit (pdp, convertn (tr1, t));
	}
	else if (ts < ts1) {
		ins_before (pdp, t1_ofs, convertn (t, tr1));
		t = tr1;
	}

	next_token (&pdp->scan);
	expect (pdp, TK_AND);
	t2_ofs = cur_pc (pdp);
	id_or_parameter (pdp, &tr2);
	ts2 = type_size (pdp, tr2);
	check_types (pdp, t, tr2);
	next_token (&pdp->scan);

	if (tr2 == T_TYPECODE_PAR) {
		set_at_pc (pdp, t2_ofs, lpar_type (t));
	}
	else if (ts > ts2) {
		emit (pdp, convertn (tr2, t));
	}
	switch (t) {
		case T_TYPECODE_OCTET:
		case T_TYPECODE_BOOLEAN:
		case T_TYPECODE_USHORT:
		case T_TYPECODE_ULONG:
			emit (pdp, O_BTWWU);
			break;
		case T_TYPECODE_CHAR:
		case T_TYPECODE_WCHAR:
		case T_TYPECODE_SHORT:
		case T_TYPECODE_LONG:
			emit (pdp, O_BTWWS);
			break;
		case T_TYPECODE_ULONGLONG:
			emit (pdp, O_BTWLU);
			break;
		case T_TYPECODE_LONGLONG:
			emit (pdp, O_BTWLS);
			break;
		case T_TYPECODE_FLOAT:
		case T_TYPECODE_DOUBLE:
			emit (pdp, O_BTWD);
			break;
#ifdef LONGDOUBLE
		case T_TYPECODE_LONGDOUBLE:
			emit (pdp, O_BTWO);
			break;
#endif
		default:
			parse_error (pdp, PE_INC_TYPES, NULL);
			break;
	}

	new_list (&op->fchain);
	new_list (&op->tchain);
	if (negate)
		op->condition = C_T;
	else
		op->condition = C_F;
}

static void condition (ParseData *pdp, DataObject *op);

#define	swap(x,y,tmp)	(tmp) = (x); (x) = (y); (y) = (tmp)

static Cond complement (ParseData *pdp, Cond c)
{
	Cond	nc = -1;

	switch (c) {
		case C_EQ:
			nc = C_NE;
			break;
		case C_NE:
			nc = C_EQ;
			break;
		case C_GT:
			nc = C_LE;
			break;
		case C_LE:
			nc = C_GT;
			break;
		case C_LT:
			nc = C_GE;
			break;
		case C_GE:
			nc = C_LT;
			break;
		case C_F:
			nc = C_T;
			break;
		case C_T:
			nc = C_F;
			break;
		default:
			parse_error (pdp, PE_INV_COMP, NULL);
			break;
	}
	return (nc);
}

static void factor (ParseData *pdp, DataObject *op)
{
	T_TypeCode_t	t1;
	unsigned	t1_ofs;
	PatchNode	*p;

	if (pdp->scan.token == TK_NOT) {
		next_token (&pdp->scan);
		factor (pdp, op);
		swap (op->tchain.head, op->fchain.head, p);
		swap (op->tchain.tail, op->fchain.tail, p);
		op->condition = complement (pdp, op->condition);
	}
	else if (pdp->scan.token == TK_LPAR) {
		next_token (&pdp->scan);
		condition (pdp, op);
		expect (pdp, TK_RPAR);
	}
	else {
		t1_ofs = cur_pc (pdp);
		id_or_parameter (pdp, &t1);
		next_token (&pdp->scan);
		if (pdp->scan.token == TK_BETWEEN || pdp->scan.token == TK_NOT)
			between_predicate (pdp, t1, t1_ofs, op);
		else if (((1 << pdp->scan.token) & relop_set) != 0)
			comp_predicate (pdp, t1, t1_ofs, op);
		else
			expect (pdp, TK_EQ);
	}
}

static void gen_condition (ParseData *pdp, Cond c)
{
	switch (c) {
		case C_EQ:
			emit (pdp, O_BEQ);
			break;
		case C_NE:
			emit (pdp, O_BNE);
			break;
		case C_GT:
			emit (pdp, O_BGT);
			break;
		case C_LE:
			emit (pdp, O_BLE);
			break;
		case C_LT:
			emit (pdp, O_BLT);
			break;
		case C_GE:
			emit (pdp, O_BGE);
			break;
		case C_F:
			emit (pdp, O_BF);
			break;
		case C_T:
			emit (pdp, O_BT);
			break;
		default:
			parse_error (pdp, PE_INT_ERR, "invalid stored condition");
			break;
	}
	emits (pdp, 0);
}

static void bool_op (DataObject *op, DataObject *np)
{
	merge_patches (&op->tchain, &np->tchain);
	merge_patches (&op->fchain, &np->fchain);
	op->condition = np->condition;
}

static void term (ParseData *pdp, DataObject *op)
{
	DataObject	ndp;

	factor (pdp, op);
	while (pdp->scan.token == TK_AND) {
		append (pdp, &op->fchain, cur_pc (pdp));
		gen_condition (pdp, op->condition);
		backpatch (pdp, &op->tchain, cur_pc (pdp));
		free_patches (&op->tchain);
		next_token (&pdp->scan);
		factor (pdp, &ndp);
		bool_op (op, &ndp);
	}
}

static void condition (ParseData *pdp, DataObject *op)
{
	DataObject	ndp;

	term (pdp, op);
	while (pdp->scan.token == TK_OR) {
		append (pdp, &op->tchain, cur_pc (pdp));
		gen_condition (pdp, complement (pdp, op->condition));
		backpatch (pdp, &op->fchain, cur_pc (pdp));
		free_patches (&op->fchain);
		next_token (&pdp->scan);
		term (pdp, &ndp);
		bool_op (op, &ndp);
	}
}

int sql_parse_topic (const char *s)
{
	ParseData	data;
	DataObject	dobj;
	char		buffer [80];

	if (!s)
		return (1);

	data.type = PT_TOPIC;
	data.bc_data = NULL;
	data.bc_size = data.bc_left = 0;
	data.error = PE_OK;
	data.scan.buffer = buffer;
	data.scan.buf_size = sizeof (buffer);
	data.scan.dyn_buf = 0;
	if (!setjmp (data.jbuf)) {
		sql_start_scan (s, &data.scan);
		next_token (&data.scan);
		select_from (&data);
		if (data.scan.token == TK_WHERE) {
			next_token (&data.scan);
			condition (&data, &dobj);
		}
		expect (&data, TK_SEMI);
		expect (&data, TK_EOL);
	}
	if (data.scan.dyn_buf)
		xfree (data.scan.buffer);
	return (data.error);
}

static void patch_out (ParseData *pdp, DataObject *op)
{
	append (pdp, &op->fchain, cur_pc (pdp));
	gen_condition (pdp, op->condition);
	backpatch (pdp, &op->tchain, cur_pc (pdp));
	free_patches (&op->tchain);
	emit (pdp, O_RETT);
	backpatch (pdp, &op->fchain, cur_pc (pdp));
	free_patches (&op->fchain);
	emit (pdp, O_RETF);
}

void sql_parse_init (void)
{
	sql_init_scan ();
}

int sql_parse_query (const TypeSupport_t *ts,
		     const char          *s,
		     BCProgram           *mprog,
		     BCProgram           *oprog)
{
	ParseData	data;
	DataObject	dobj;
	T_TypeCode_t	type;
	unsigned	ofs, nofs, i;
	char		buffer [80];

	if (!ts || !s || !mprog || !oprog) {
		if (mprog)
			mprog->length = 0;
		if (oprog)
			oprog->length = 0;
		return (1);
	}
	data.type = PT_QUERY;
	data.bc_data = NULL;
	data.bc_size = data.bc_left = 0;
	data.bc_npars = 0;
	data.error = 0;
	data.tc = ts;
	data.scan.buffer = buffer;
	data.scan.buf_size = sizeof (buffer);
	data.scan.dyn_buf = 0;
	if (!setjmp (data.jbuf)) {
		sql_start_scan (s, &data.scan);
		next_token (&data.scan);
		if (data.scan.token != TK_ORDER && data.scan.token != TK_EOL) {
			condition (&data, &dobj);
			patch_out (&data, &dobj);
		}
		if (!data.error) {
			mprog->start = mprog->buffer = data.bc_data;
			mprog->length = data.bc_size - data.bc_left;
			mprog->npars = data.bc_npars;
		}
		data.bc_data = NULL;
		data.bc_size = data.bc_left = 0;
		if (data.scan.token == TK_ORDER) {
			next_token (&data.scan);
			expect (&data, TK_BY);
			if (data.scan.token != TK_ID)
				parse_error (&data, PE_SYNTAX, "Identifier");
			else {
				emit (&data, O_DS0);
				ofs = cur_pc (&data);
				identifier (&data, &type, 0);
				expect (&data, TK_ID);
				nofs = cur_pc (&data);
				emit (&data, O_DS1);
				for (i = 0; i < nofs - ofs; i++)
					emit (&data, data.bc_data [ofs + i]);
				emit_compare (&data, type);
				while (data.scan.token == TK_COMMA) {
					emit (&data, O_CRNE);
					next_token (&data.scan);
					emit (&data, O_DS0);
					ofs = cur_pc (&data);
					identifier (&data, &type, 0);
					expect (&data, TK_ID);
					nofs = cur_pc (&data);
					emit (&data, O_DS1);
					for (i = 0; i < nofs - ofs; i++)
						emit (&data, data.bc_data [ofs + i]);
					emit_compare (&data, type);
				}
				emit (&data, O_RETC);
			}
		}
		expect (&data, TK_EOL);
	}
	if (!data.error) {
		oprog->start = oprog->buffer = data.bc_data;
		oprog->length = cur_pc (&data);
	}
	if (data.scan.dyn_buf)
		xfree (data.scan.buffer);
	return (data.error);
}

int sql_parse_filter (const TypeSupport_t *ts, const char *s, BCProgram *prog)
{
	ParseData	data;
	DataObject	dobj;
	char		buffer [80];

	if (!ts || !s || !prog) {
		if (prog)
			prog->length = 0;
		return (1);
	}
	data.type = PT_FILTER;
	data.bc_data = NULL;
	data.bc_size = data.bc_left = 0;
	data.bc_npars = 0;
	data.error = 0;
	data.tc = ts;
	data.scan.buffer = buffer;
	data.scan.buf_size = sizeof (buffer);
	data.scan.dyn_buf = 0;
	if (!setjmp (data.jbuf)) {
		sql_start_scan (s, &data.scan);
		next_token (&data.scan);
		condition (&data, &dobj);
		patch_out (&data, &dobj);
		expect (&data, TK_EOL);
	}
	if (!data.error) {
		prog->start = prog->buffer = data.bc_data;
		prog->length = cur_pc (&data);
		prog->npars = data.bc_npars;
	}
	if (data.scan.dyn_buf)
 		xfree (data.scan.buffer);
	return (data.error);
}

#ifdef PARSE_DEBUG

struct simple_st {
	uint16_t	u16;
	int16_t		i16;
	uint32_t	u32;
	int32_t		i32;
	uint64_t	u64;
	int64_t		i64;
	float		fl;
	double		d;
	char		*sp;
	char		ch;
};

enum discr_en {
	d_ch = 4,
	d_num = 22,
	d_fnum = 555
};

union info_un {
	char		ch;
	uint32_t	num;
	float		fnum;
};

DDS_UNION (union info_un, enum discr_en, info_u);

enum object {
	bike,
	car,
	plane,
	blimp = -1
};

struct s_st {
	struct simple_st simple;
	char		 *name;
	int32_t		 x, y;
	int32_t		 height;
	enum object	 obj;
	info_u		 info;
} s;

static DDS_TypeSupport_meta s_tsm [] = {
	{ CDR_TYPECODE_STRUCT,   2, "s", sizeof (struct s_st), 0, 7, 0 },
	{ CDR_TYPECODE_CSTRING,  0, "name",   0, offsetof (struct s_st, name), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "x",      0, offsetof (struct s_st, x), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "y",      0, offsetof (struct s_st, y), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "height", 0, offsetof (struct s_st, height), 0, 0 },
	{ CDR_TYPECODE_STRUCT,   0, "simple", sizeof (struct simple_st), 0, 10, 0 },
	{ CDR_TYPECODE_CHAR,     0, "ch",     0, offsetof (struct simple_st, ch), 0, 0 },
	{ CDR_TYPECODE_USHORT,   0, "u16",    0, offsetof (struct simple_st, u16), 0, 0 },
	{ CDR_TYPECODE_SHORT,    0, "i16",    0, offsetof (struct simple_st, i16), 0, 0 },
	{ CDR_TYPECODE_ULONG,    0, "u32",    0, offsetof (struct simple_st, u32), 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "i32",    0, offsetof (struct simple_st, i32), 0, 0 },
	{ CDR_TYPECODE_ULONGLONG,0, "u64",    0, offsetof (struct simple_st, u64), 0, 0 },
	{ CDR_TYPECODE_LONGLONG, 0, "i64",    0, offsetof (struct simple_st, i64), 0, 0 },
	{ CDR_TYPECODE_FLOAT,    0, "fl",     0, offsetof (struct simple_st, fl), 0, 0 },
	{ CDR_TYPECODE_DOUBLE,   0, "d",      0, offsetof (struct simple_st, d), 0, 0 },
	{ CDR_TYPECODE_CSTRING,  0, "sp",     0, offsetof (struct simple_st, sp), 0, 0 },
	{ CDR_TYPECODE_ENUM,     0, "obj",    0, offsetof (struct s_st, obj), 4, 0 },
	{ CDR_TYPECODE_LONG,     0, "bike",   0, 0, 0, 0 },
	{ CDR_TYPECODE_LONG,     0, "car",    0, 0, 0, 1 },
	{ CDR_TYPECODE_LONG,     0, "plane",  0, 0, 0, 2 },
	{ CDR_TYPECODE_LONG,     0, "blimp",  0, 0, 0, -1 },
	{ CDR_TYPECODE_UNION,    2, "info",   sizeof (info_u), offsetof (struct s_st, info), 3, 0 },
	{ CDR_TYPECODE_CHAR,     2, "ch",     0, offsetof (info_u, u), 0, d_ch },
	{ CDR_TYPECODE_ULONG,	 2, "num",    0, offsetof (info_u, u), 0, d_num },
	{ CDR_TYPECODE_FLOAT,    2, "fnum",   0, offsetof (info_u, u), 0, d_fnum }
};

void dbg_print_indent (unsigned level, const char *name)
{
	while (level) {
		printf ("  ");
		level--;
	}
	if (name)
		printf ("%s:", name);
}

int main (void)
{
	DDS_TypeSupport	*ts;
	BCProgram	mbc, obc;
	int		err, i;

	const char	*queries [] = {
		/*"height < 1000 AND x <23.23 ORDER BY name, x",*/
		"(height < 1000 OR simple.i16 <> 345) AND x <23.23 ORDER BY name, x",
	};
	const char	*filters [] = {
		"obj <> blimp OR height BETWEEN 10 AND %1 OR x >= %0 AND y < %2",
		/*"height BETWEEN 10 AND %1 OR x >= %0 AND y < %2"*/
	};

	/* Create the type we're gonna use. */
	ts = DDS_DynamicType_register (s_tsm);
	if (!ts) {
		printf ("Type not properly registered!\r\n");
		exit (1);
	}
	DDS_TypeSupport_dump (0, ts);

	/* Init scanner. */
	sql_init_scan ();

#if 0
	/* Topic definition parsing -- forget it for now. */
	sql_parse_topic ("SELECT flight_name, x, y, z AS height "
		         "FROM `Location' NATURAL JOIN 'FlightPlan' "
		         "WHERE height < 1000 AND x <23;");
#endif

	/* Parse a simple query. */
	for (i = 0; i < sizeof (queries) / sizeof (char *); i++) {
		printf ("Parse query: \"%s\"\r\n", queries [i]);
		err = sql_parse_query (ts, queries [i], &mbc, &obc);
		if (!err) {
			printf ("Success! Resulting match bytecode program:\r\n");
			bc_dump (0, &mbc);
			if (obc.length) {
				printf ("Resulting sample order bytecode program:\r\n");
				bc_dump (0, &obc);
			}
		}
	}

	/* Parse a filter statement. */
	for (i = 0; i < sizeof (filters) / sizeof (char *); i++) {
		printf ("Parse filter: \"%s\"\r\n", filters [i]);
		err = sql_parse_filter (ts, filters [i], &mbc);
		if (!err) {
			printf ("Success! Resulting match bytecode program:\r\n");
			bc_dump (0, &mbc);
		}
	}
	return (0);
}

#endif

