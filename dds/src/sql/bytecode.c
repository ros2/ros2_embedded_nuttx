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

/* bytecode.c -- Implements the bytecode interpreter that are generated from Query and Filter
		 SQL statements as well the interface to the bytecode
		 interpreter that can be used to parse the statements. */

#include <stdio.h>
#ifndef _WIN32
#include <inttypes.h>
#endif
#include "error.h"
#include "prof.h"
#include "debug.h"
#include "nmatch.h"
#include "bytecode.h"

#ifdef XTYPES_USED

#include "type_data.h"
#include "xtypes.h"
#include "xcdr.h"

#define	is_struct(t)	((t)->kind == DDS_STRUCTURE_TYPE)
#define	typekind(t)	(t)->kind
#define	s_member(t,i)	(t)->member[i]
#define	CDR_TypeSupport	Type

#else

#include "cdr.h"

#define	is_struct(t)	((t)->typecode == CDR_TYPECODE_STRUCT)
#define	typekind(t)	(t)->typecode
#define	s_member(t,i)	(t)->elements [i]

#endif

#define	MAX_STACK	32	/* Max. stack depth. */
#define	MAX_INSTS	512	/* Max. # of instructions to execute. */

typedef enum {
	T_STRING,
	T_INT32,
	T_INT64,
	T_DOUBLE
#ifdef LONGDOUBLE
	, T_LDOUBLE
#endif
} Type_t;

typedef union element_un {
	uint32_t	u;
	int32_t		i;
	int64_t		l;
	double		d;
#ifdef LONGDOUBLE
	long double	o;
#endif
	const char	*sp;
	char		c2s [16];
} Element_t;

static Element_t stack [MAX_STACK];

typedef struct bc_cache_st {
	Element_t	*args;
	Type_t		*types;
	unsigned	nargs;
} BCCache_t;


/* data_container -- Update the pointer to payload data (*wp) and the type 
		     pointer (*tp), so that *wp will point to the embedded
		     container at the field offset given by *dp. If prefixed
		     is non-0, the payload data can be marshalled using the
		     appropriate typecode mechanisms (CDR/PL/XML/...), otherwise
		     it is in the native binary format. */

static int data_container (DBW                   *wp,
			   const CDR_TypeSupport **tp,
			   unsigned              field,
			   int                   prefixed,
			   unsigned              *offset)
{
#ifdef XTYPES_USED
	StructureType		*sp = (StructureType *) *tp;
#else
	CDR_TypeSupport_struct	*sp = (CDR_TypeSupport_struct *) *tp;
#endif
	const unsigned char	*cp;

	if (!is_struct (*tp))
		return (BC_ERR_UNIMPL);
	
	if (!offset)
		return (BC_ERR_INVDATA);

	cp = wp->data;
	if (prefixed) {
		unsigned        	type;
		DDS_ReturnCode_t	error;

		if (!(*offset))
			*offset = 4;

		type = (cp [0] << 8) | cp [1];
		if ((type >> 1) == MODE_CDR) 
			*offset = cdr_field_offset (cp + *offset, *offset,
						    field, *tp,
						    (type & 1) ^ ENDIAN_CPU,
						    &error);
		else if ((type >> 1) == MODE_RAW)
			*offset += s_member (sp, field).offset;
		else 
			return (BC_ERR_UNIMPL);
	}
	else
		*offset += s_member (sp, field).offset;
#ifdef XTYPES_USED
	*tp = xt_type_ptr (sp->type.scope, sp->member [field].id);
#else
	*tp = sp->elements [field].ts.ts;
#endif
	return ((*tp) ? BC_OK : BC_ERR_INVDATA);
}

/* data_discriminant -- Update the pointer to payload data (*wp) and the type
			pointer (*tp), so that *wp will point to the embedded
			container that corresponds with the given discriminant
			(d).  If prefixed is non-0, the payload data can be 
			marshalled using the appropriate typecode mechanisms
			(CDR/PL/XML/...), otherwise it is in the native binary
			format. */

static int data_discriminant (DBW                   *dp,
			      const CDR_TypeSupport **tp,
			      unsigned              field,
			      int                   prefixed,
			      unsigned              *offset)
{
	ARG_NOT_USED (dp)
	ARG_NOT_USED (tp)
	ARG_NOT_USED (field)
	ARG_NOT_USED (prefixed)
	ARG_NOT_USED (offset)
	

	/* ... TBC ... */

	return (BC_ERR_UNIMPL);
}

/* data_ptr -- Return a pointer to actual payload data within a data container
	       (wp) of the given type (tp) where the 2-byte field number within
	       the container is specified in *pc.  If prefixed is non-0, it is
	       prefixed, so the actual payload data can use any marshalling
	       mechanism (CDR/PL/XML/...), otherwise it is in the native binary
	       format. */

#define	ALIGN(delta, boundary)	(((delta) + (boundary) - 1) & ~((boundary) - 1))

typedef struct {
	int prefixed; /* */
	int string;
	unsigned align;
	unsigned char buffer[16];
	unsigned offset;
} data_ptr_type;

static const unsigned char *data_ptr (DBW                   *wp,
				      const CDR_TypeSupport *tp,
				      unsigned              field,
				      data_ptr_type         *type)
{
#ifdef XTYPES_USED
	StructureType		*sp = (StructureType *) tp;
#else
	CDR_TypeSupport_struct	*sp = (CDR_TypeSupport_struct *) tp;
#endif
	unsigned        	prefix;
	unsigned		n, left, ofs;
	const unsigned char	*cp;
	const DB		*bp;
	int 			swap = 0;
	DDS_ReturnCode_t	error;

	if (!is_struct (tp))
		return (NULL);

	cp = wp->data;
	ofs = type->offset;
	
	if (type->prefixed) {
		if (!ofs)
			ofs = 4;

		prefix = ((cp [0] << 8) | cp [1]) >> 1;
		if (prefix == MODE_CDR || prefix == MODE_PL_CDR) {
			swap = (cp [1] & 1) ^ ENDIAN_CPU;
			ofs = cdr_field_offset (cp + ofs, ofs, field, tp,
								swap, &error);
			ofs = ALIGN (ofs, type->align > 8 ? 8 : type->align);
			if (type->string)
				ofs += 4;
		}
		else if (prefix == MODE_RAW)
			ofs += s_member (sp, field).offset;
		else
			return (NULL);
	}
	else
		ofs += s_member (sp, field).offset;

	n = ofs;
	bp = wp->dbp;
	left = wp->left;
	while (n >= left) {
		bp = bp->next;
		n -= left;
		left = bp->size;
		cp = bp->data;
	}
	cp += n;

	if (swap && !type->string) {
		switch (type->align) {
			case 2:
				memcswap16 (type->buffer, cp);
				return (type->buffer);
			case 4:
				memcswap32 (type->buffer, cp);
				return (type->buffer);
			case 8:
				memcswap64 (type->buffer, cp);
				return (type->buffer);
			case 16:

			default:
				break;
		}
	}
	return (cp);
}

/* bc_cache_init -- Initialize a cache argument. */

void bc_cache_init (void *cache)
{
	BCCache_t	**cachep = (BCCache_t **) cache;

	*cachep = NULL;
}


/* bc_cache_flush -- Flush the cache contents.  This function should be called
		     whenever either the bytecode program or its arguments are
		     updated. */

void bc_cache_flush (void *cache)
{
	BCCache_t	*cp, **cachep = (BCCache_t **) cache;

	if ((cp = *cachep) == NULL)
		return;

	if (cp->nargs) {
		xfree (cp->args);
		xfree (cp->types);
	}
	xfree (cp);
	*cachep = NULL;
}

/* bc_cache_add -- Add an entry to the cache. */

static void bc_cache_add (BCCache_t *cp, unsigned par, Type_t t, Element_t *ep)
{
	unsigned	i;

	if (!cp)
		return;

	if (par >= cp->nargs) {
		if (!cp->nargs) {
			cp->args = xmalloc (sizeof (Element_t) * (par + 1));
			if (cp->args) {
				cp->types = xmalloc (sizeof (Type_t) * (par + 1));
				if (!cp->types) {
					xfree (cp->args);
					return;
				}
				else
					cp->nargs = par + 1;
			}
			else 
				return;

			for (i = 0; i < cp->nargs; i++)
				cp->types [i] = T_STRING;
		}
		else {
			cp->args = xrealloc (cp->args, sizeof (Element_t) * (par + 1));
			if (!cp->args) {
				xfree (cp->types);
				cp->nargs = 0;
				return;
			}
			else {
				cp->types = xrealloc (cp->types, sizeof (Type_t) * (par + 1));
				if (!cp->types) {
					xfree (cp->args);
					cp->nargs = 0;
					return;
				}
			}
			for (i = cp->nargs; i <= par; i++)
				cp->types [i] = T_STRING;
			cp->nargs = par + 1;
		}
	}
	cp->args [par] = *ep;
	cp->types [par] = t;
}

/* bc_cache_reset -- Reset the cache contents since the parameters were changed. */

void bc_cache_reset (void *cache)
{
	BCCache_t	*cp = (BCCache_t *) cache;
	unsigned	i;

	if (!cp)
		return;

	for (i = 0; i < cp->nargs; i++)
		cp->types [i] = T_STRING;
}

#ifdef _WIN32
#define strtoll(p, e, b) _strtoi64(p, e, b)
#endif

PROF_PID (bc_prog_pid)

#ifdef ANDROID
#define strtold strtod
#endif

#if defined (NUTTX_RTOS)
				/* In our architecture and with arm-none-eabi compiler double and long 
				double have the same size thereby srtold and strtod can be used interchangeably */
#define strtold strtod
#endif


/* bc_interpret -- Interpret a bytecode program (progcode, proglen), with the
		   given parameter list (pars, npars) over a data sample (data,
		   prefixed) of the specified type (ts). 
		   If successful, BC_OK (=0) is returned and *result will be set
		   to the return code of the bytecode program. */

int bc_interpret (const BCProgram     *program,
	          const Strings_t     *pars,
		  void                *cache,
	          DBW                 *data,
		  DBW                 *data2,
	          int                 prefixed,
	          const TypeSupport_t *dts,
	          int                 *result)
{
	const unsigned char	*pc, *dp;
	const char		*sp;
	unsigned char		par;
	unsigned short		*usp;
	short			*ssp;
	unsigned		*uwp, inst;
	int			*swp;
	float			f;
	DBW			*cdp;
	data_ptr_type           type;
	const CDR_TypeSupport	*ctp;
	BCCache_t		*cp, **cachep = (BCCache_t **) cache;
	unsigned		tos, n, npars;
	int			cnest, r, loc, i, rc = BC_OK;


	if (dts->ts_prefer != MODE_CDR)
		return (BC_ERR_UNIMPL);

	prof_start (bc_prog_pid);
	if (!cachep)
		cp = NULL;
	else if (!*cachep) {
		cp = *cachep = xmalloc (sizeof (BCCache_t));
		if (cp)
			cp->nargs = 0;
	}
	else
		cp = *cachep;
	pc = program->start;
	tos = 0;
	r = 0;
	cnest = 0;
	cdp = data;
	type.offset = 0;
	type.prefixed = prefixed;
	type.string = 0;
	ctp = dts->ts_cdr;
	if (!pars)
		npars = 0;
	else
		npars = DDS_SEQ_LENGTH (*pars);
	for (n = 0; n < MAX_INSTS; n++) {
		if (tos >= MAX_STACK) {
			rc = BC_ERR_STKOVFL;
			break;
		}
		if (pc < program->buffer || pc >= program->buffer + program->length) {
			rc = BC_ERR_INVADDR;
			break;
		}

		/* Load constant (embedded): */
		inst = *pc++;
		if (inst < 0x80) {
			i = inst;
			if (i >= 0x40)
				i -= 128;
			stack [tos++].i = i;
			continue;
		}
		switch (inst) {

			/* Load constant (follows opcode): */
			case O_LCBU:
				stack [tos++].u = *pc++;
				break;
			case O_LCBS:
				stack [tos++].i = (char) *pc++;
				break;
			case O_LCSU:
				stack [tos++].u = (pc [0] << 8) | pc [1];
				pc += 2;
				break;
			case O_LCSS:
				stack [tos++].i = (short) ((pc [0] << 8) | pc [1]);
				pc += 2;
				break;
			case O_LCWU:
				stack [tos++].u = (pc [0] << 24) | (pc [1] << 16) | (pc [2] << 8) | pc [3];
				pc += 4;
				break;
			case O_LCWS:
				stack [tos++].i = (int) ((pc [0] << 24) | (pc [1] << 16) | (pc [2] << 8) | pc [3]);
				pc += 4;
				break;
			case O_LCL:
				memcpy (&stack [tos++].l, pc, sizeof (int64_t));
				pc += 8;
				break;
			case O_LCF:
				memcpy (&f, pc, sizeof (float));
				stack [tos++].d = f;
				pc += sizeof (float);
				break;
			case O_LCD:
				memcpy (&stack [tos++].d, pc, sizeof (double));
				pc += sizeof (double);
				break;
#ifdef LONGDOUBLE
			case O_LCO:
				memcpy (&stack [tos++].o, pc, sizeof (long double));
				pc += sizeof (long double);
				break;
#endif
			case O_LCS:
				stack [tos].sp = (char *) pc;
				pc += strlen (stack [tos++].sp) + 1;
				break;

			/* Load payload data: */
			case O_LDBU:
				n = (pc [0] << 8) | pc [1];
				type.align=1;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				stack [tos++].u = *dp;
				if (cnest) {

				    creset:
					cdp = data;
					ctp = dts->ts_cdr;
					cnest = 0;
					type.offset = 0;
				}
				break;
			case O_LDBS:
				n = (pc [0] << 8) | pc [1];
				type.align=1;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				stack [tos++].i = (char) *dp;
				if (cnest)
					goto creset;
				break;
			case O_LDSU:
				n = (pc [0] << 8) | pc [1];
				type.align=2;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				usp = (unsigned short *) dp;
				stack [tos++].u = *usp;
				if (cnest)
					goto creset;
				break;
			case O_LDSS:
				n = (pc [0] << 8) | pc [1];
				type.align=2;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				ssp = (short *) dp;
				stack [tos++].i = *ssp;
				if (cnest)
					goto creset;
				break;
			case O_LDWU:
				n = (pc [0] << 8) | pc [1];
				type.align=4;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				uwp = (unsigned *) dp;
				stack [tos++].u = *uwp;
				if (cnest)
					goto creset;
				break;
			case O_LDWS:
				n = (pc [0] << 8) | pc [1];
				type.align=4;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				swp = (int *) dp;
				stack [tos++].i = *swp;
				if (cnest)
					goto creset;
				break;
			case O_LDL:
				n = (pc [0] << 8) | pc [1];
				type.align=8;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				memcpy (&stack [tos++].l, dp, sizeof (int64_t));
				if (cnest)
					goto creset;
				break;
			case O_LDF:
				n = (pc [0] << 8) | pc [1];
				type.align=4;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				memcpy (&f, dp, sizeof (float));
				stack [tos++].d = f;
				if (cnest)
					goto creset;
				break;
			case O_LDD:
				n = (pc [0] << 8) | pc [1];
				type.align=8;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				memcpy (&stack [tos++].d, dp, sizeof (double));
				if (cnest)
					goto creset;
				break;
#ifdef LONGDOUBLE
			case O_LDO:
				n = (pc [0] << 8) | pc [1];
				type.align=16;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				memcpy (&stack [tos++].o, dp, sizeof (long double));
				if (cnest)
					goto creset;
				break;
#endif
			case O_LDS:
				n = (pc [0] << 8) | pc [1];
				type.align=4;
				type.string=1;
				if ((dp = data_ptr (cdp, ctp, n, &type)) == NULL) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				type.string=0;
				pc += 2;
				stack [tos++].sp = (char *) dp;
				if (cnest)
					goto creset;
				break;

			/* Load parameter data. */
			case O_LPBU:
			case O_LPSU:
			case O_LPWU:
				par = *pc++;
				if (par >= npars) {
					rc = BC_ERR_INVPAR;
					goto done;
				}
				if (cp && cp->nargs > par && cp->types [par] == T_INT32)
					stack [tos] = cp->args [par];
				else {
					sp = str_ptr (DDS_SEQ_ITEM (*pars, par));
					stack [tos].u = strtol (sp, NULL, 10);
					bc_cache_add (cp, par, T_INT32, &stack [tos]);
				}
				tos++;
				break;
			case O_LPBS:
			case O_LPSS:
			case O_LPWS:
				par = *pc++;
				if (par >= npars) {
					rc = BC_ERR_INVPAR;
					goto done;
				}
				if (cp && cp->nargs > par && cp->types [par] == T_INT32)
					stack [tos] = cp->args [par];
				else {
					sp = str_ptr (DDS_SEQ_ITEM (*pars, par));
					stack [tos].i = strtol (sp, NULL, 10);
					bc_cache_add (cp, par, T_INT32, &stack [tos]);
				}
				tos++;
				break;
			case O_LPL:
				par = *pc++;
				if (par >= npars) {
					rc = BC_ERR_INVPAR;
					goto done;
				}
				if (cp && cp->nargs > par && cp->types [par] == T_INT64)
					stack [tos] = cp->args [par];
				else {
					sp = str_ptr (DDS_SEQ_ITEM (*pars, par));
					stack [tos].l = strtoll (sp, NULL, 10);
					bc_cache_add (cp, par, T_INT64, &stack [tos]);
				}
				tos++;
				break;
			case O_LPF:
			case O_LPD:
				par = *pc++;
				if (par >= npars) {
					rc = BC_ERR_INVPAR;
					goto done;
				}
				if (cp && cp->nargs > par && cp->types [par] == T_DOUBLE)
					stack [tos] = cp->args [par];
				else {
					sp = str_ptr (DDS_SEQ_ITEM (*pars, par));
					stack [tos].d = strtod (sp, NULL);
					bc_cache_add (cp, par, T_DOUBLE, &stack [tos]);
				}
				tos++;
				break;
#ifdef LONGDOUBLE
			case O_LPO:
				par = *pc++;
				if (par >= npars) {
					rc = BC_ERR_INVPAR;
					goto done;
				}
				if (cp && cp->nargs > par && cp->types [par] == T_LDOUBLE)
					stack [tos] = cp->args [par];
				else {
					sp = str_ptr (DDS_SEQ_ITEM (*pars, par));
					stack [tos].o = strtold (sp, NULL);
					bc_cache_add (cp, par, T_LDOUBLE, &stack [tos]);
				}
				tos++;
				break;
#endif
			case O_LPS:
				par = *pc++;
				if (par >= npars) {
					rc = BC_ERR_INVPAR;
					goto done;
				}
				stack [tos++].sp = str_ptr (DDS_SEQ_ITEM (*pars, par));
				break;

			/* Comparisons. */
			case O_CMPWU:
				tos -= 2;
				if (stack [tos].u == stack [tos + 1].u)
					r = 0;
				else if (stack [tos].u > stack [tos + 1].u)
					r = 1;
				else
					r = -1;
				break;
			case O_CMPWS:
				tos -= 2;
				if (stack [tos].i == stack [tos + 1].i)
					r = 0;
				else if (stack [tos].i > stack [tos + 1].i)
					r = 1;
				else
					r = -1;
				break;
			case O_CMPLU:
				tos -= 2;
				if (stack [tos].l == stack [tos + 1].l)
					r = 0;
				else if ((uint64_t) stack [tos].l > (uint64_t) stack [tos + 1].l)
					r = 1;
				else
					r = -1;
				break;
			case O_CMPLS:
				tos -= 2;
				if (stack [tos].l == stack [tos + 1].l)
					r = 0;
				else if ((int64_t) stack [tos].l > (int64_t) stack [tos + 1].l)
					r = 1;
				else
					r = -1;
				break;
			case O_CMPD:
				tos -= 2;
				if (stack [tos].d > stack [tos + 1].d)
					r = 1;
				else if (stack [tos].d == stack [tos + 1].d)
					r = 0;
				else
					r = -1;
				break;
#ifdef LONGDOUBLE
			case O_CMPO:
				tos -= 2;
				if (stack [tos].o > stack [tos + 1].o)
					r = 1;
				else if (stack [tos].o == stack [tos + 1].o)
					r = 0;
				else
					r = -1;
				break;
#endif
			case O_CMPS:
				tos -= 2;
				r = strcmp (stack [tos].sp, stack [tos + 1].sp);
				break;
			case O_BTWWU:
				tos -= 2;
				if (stack [tos - 1].u >= stack [tos].u && 
				    stack [tos - 1].u <= stack [tos + 1].u)
					stack [tos - 1].d = 1;
				else
					stack [tos - 1].d = 0;
				break;
			case O_BTWWS:
				tos -= 2;
				if (stack [tos - 1].i >= stack [tos].i && 
				    stack [tos - 1].i <= stack [tos + 1].i)
					stack [tos - 1].d = 1;
				else
					stack [tos - 1].d = 0;
				break;
			case O_BTWLU:
				tos -= 2;
				if ((uint64_t) stack [tos - 1].l >= (uint64_t) stack [tos].l && 
				    (uint64_t) stack [tos - 1].l <= (uint64_t) stack [tos + 1].l)
					stack [tos - 1].d = 1;
				else
					stack [tos - 1].d = 0;
				break;
			case O_BTWLS:
				tos -= 2;
				if ((int64_t) stack [tos - 1].l >= (int64_t) stack [tos].l && 
				    (int64_t) stack [tos - 1].l <= (int64_t) stack [tos + 1].l)
					stack [tos - 1].d = 1;
				else
					stack [tos - 1].d = 0;
				break;
			case O_BTWD:
				tos -= 2;
				if (stack [tos - 1].d >= stack [tos].d && 
				    stack [tos - 1].d <= stack [tos + 1].d)
					stack [tos - 1].d = 1;
				else
					stack [tos - 1].d = 0;
				break;
#ifdef LONGDOUBLE
			case O_BTWO:
				tos -= 2;
				if (stack [tos - 1].o >= stack [tos].o && 
				    stack [tos - 1].o <= stack [tos + 1].o)
					stack [tos - 1].d = 1;
				else
					stack [tos - 1].d = 0;
				break;
#endif
			case O_LIKE:
				tos -= 2;
				if (nmatch (stack [tos + 1].sp, stack [tos].sp, NM_SQL))
					stack [tos++].d = 0;
				else
					stack [tos++].d = 1;
				break;

			/* Branches: */
			case O_BEQ:
				if (!r) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BNE:
				if (r) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BGT:
				if (r > 0) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BLE:
				if (r <= 0) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BLT:
				if (r < 0) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BGE:
				if (r >= 0) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BT:
				if (stack [--tos].d) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;
			case O_BF:
				if (!stack [--tos].d) {
					loc = (pc [0] << 8) | pc [1];
					pc = program->buffer + loc;
				}
				else
					pc += 2;
				break;

			/* Conversions. */
			case O_WU2L:
				stack [tos - 1].l = stack [tos - 1].u;
				break;
			case O_WS2L:
				stack [tos - 1].l = stack [tos - 1].i;
				break;
			case O_WU2D:
				stack [tos - 1].d = stack [tos - 1].u;
				break;
			case O_WS2D:
				stack [tos - 1].d = stack [tos - 1].i;
				break;
#ifdef LONGDOUBLE
			case O_WU2O:
				stack [tos - 1].o = stack [tos - 1].u;
				break;
			case O_WS2O:
				stack [tos - 1].o = stack [tos - 1].i;
				break;
#endif
			case O_LU2D:
				stack [tos - 1].d = (double)(uint64_t) stack [tos - 1].l;
				break;
			case O_LS2D:
				stack [tos - 1].d = (double) stack [tos - 1].l;
				break;
#ifdef LONGDOUBLE
			case O_LU2O:
				stack [tos - 1].o = (long double) (uint64_t) stack [tos - 1].l;
				break;
			case O_LS2O:
				stack [tos - 1].o = (long double) stack [tos - 1].l;
				break;
			case O_D2O:
				stack [tos - 1].o = stack [tos - 1].d;
				break;
#endif
			case O_S2W:
				stack [tos - 1].i = strtol (stack [tos - 1].sp, NULL, 10);
				break;
			case O_S2L:
				stack [tos - 1].l = strtoll (stack [tos - 1].sp, NULL, 10);
				break;
			case O_S2D:
				stack [tos - 1].d = strtod (stack [tos - 1].sp, NULL);
				break;
#ifdef LONGDOUBLE
			case O_S2O:
				stack [tos - 1].o = strtold (stack [tos - 1].sp, NULL);
				break;
#endif
			case O_C2S:
				stack [tos - 1].c2s [8] = (char) stack [tos - 1].u;
				stack [tos - 1].c2s [9] = '\0';
				stack [tos - 1].sp = &stack [tos - 1].c2s [8];
				break;

			/* Various. */
			case O_CREF:
				n = (pc [0] << 8) | pc [1];
				if (data_container (cdp, &ctp, n, prefixed, &type.offset)) {
					rc = BC_ERR_INVDATA;
					goto done;
				}
				pc += 2;
				cnest++;
				break;
			case O_DISC:
				n = (pc [0] << 8) | pc [1];
				if ((r = data_discriminant (cdp, &ctp,
							   n, prefixed, &type.offset)) != 0) {
					if (r == BC_ERR_NFOUND) {
						rc = BC_OK;
						*result = 0;
						goto done;
					}
				}
				pc += 4;
				break;
			case O_TREF:
				/* Only for Multitopic: topic name. */
				rc = BC_ERR_UNIMPL;
				goto done;
			case O_FCR:
				/* Only for Multitopic: container reference. */
				rc = BC_ERR_UNIMPL;
				goto done;
			case O_FOFS:
				/* Only for Multitopic: field reference. */
				rc = BC_ERR_UNIMPL;
				goto done;
			case O_DS0:
				cdp = data;
				type.offset = 0;
				break;
			case O_DS1:
				cdp = data2;
				type.offset = 0;
				break;

			/* Returns. */
			case O_RET:
				*result = stack [--tos].i;
				rc = BC_OK;
				goto done;
			case O_RETC:
				*result = r;
				rc = BC_OK;
				goto done;
			case O_RETT:
				*result = 1;
				rc = BC_OK;
				goto done;
			case O_RETF:
				*result = 0;
				rc = BC_OK;
				goto done;
			case O_CRFF:
				if (!stack [--tos].d) {
					*result = 0;
					rc = BC_OK;
					goto done;
				}
				break;
			case O_CRFT:
				if (stack [--tos].d) {
					*result = 0;
					rc = BC_OK;
					goto done;
				}
				break;
			case O_CRTF:
				if (!stack [--tos].d) {
					*result = 1;
					rc = BC_OK;
					goto done;
				}
				break;
			case O_CRTT:
				if (stack [--tos].d) {
					*result = 1;
					rc = BC_OK;
					goto done;
				}
				break;
			case O_CRNE:
				if (r) {
					*result = r;
					rc = BC_OK;
					goto done;
				}
				break;


			/* Unknown opcode! */
			default:
				rc = BC_ERR_INVOP;
				goto done;
		}
	}
	if (n >= MAX_INSTS)
		rc = BC_ERR_INFLOOP;

    done:
    	prof_stop_wclog (bc_prog_pid, 1, program->start);

	return (rc);
}

#define	get_su(su,pc,l)	(su) = (pc [0] << 8U) | pc [1]; pc += 2; l -= 2
#define	get_ss(ss,pc,l)	(ss) = (short) (pc [0] << 8U) | pc [1]; pc += 2; l -= 2
#define	get_wu(wu,pc,l)	(wu) = (pc [0] << 24U) | (pc [1] << 16U) | \
			       (pc [2] << 8U)  | pc [3]; pc += 4; l -= 4
#define	get_ws(ws,pc,l)	(ws) = (pc [0] << 24) | (pc [1] << 16) | \
			       (pc [2] << 8)  | pc [3]; pc += 4; l -= 4
#define	get_ls(ls,pc,l)	memcpy (&ls, pc, sizeof (int64_t)); pc += 8; l -= 8
#define	get_f(f,pc,l)	memcpy (&f, pc, sizeof (f)); pc += sizeof (float); l -= sizeof (float)
#define	get_d(d,pc,l)	memcpy (&d, pc, sizeof (d)); pc += sizeof (double); l -= sizeof (double)
#define	get_o(o,pc,l)	memcpy (&o, pc, sizeof (o)); pc += sizeof (long double); l -= sizeof (long double)

#ifdef DDS_DEBUG

void bc_dump (unsigned indent, const BCProgram *prog)
{
	const unsigned char	*pc = prog->buffer, *start;
	unsigned char		op;
	uint16_t		su;
	int16_t			ss;
	uint32_t		wu;
	int32_t			ws;
	int64_t			ls;
	float			f;
	double			d;
#ifdef LONGDOUBLE
	long double		o;
#endif
	unsigned		length = prog->length;

	start = pc;
	while (length) {
		dbg_print_indent (indent + 1, NULL);
		printf ("  %04lu\t\t", (unsigned long) (pc - start));
		op = *pc++;
		length--;
		if (op < 0x80) {
			ws = op;
			if (ws >= 0x40)
				ws -= 128;
			printf ("LCI\t%d\r\n", ws);
			continue;
		}
		switch (op) {

			/* Load constants: */
			case O_LCBU:
				printf ("LCBU\t%u", *pc++);
				length--;
				break;
			case O_LCBS:
				printf ("LCBS\t%d", (char) *pc++);
				length--;
				break;
			case O_LCSU:
				get_su (su, pc, length);
				printf ("LCSU\t%u", su);
				break;
			case O_LCSS:
				get_ss (ss, pc, length);
				printf ("LCSS\t%d", ss);
				break;
			case O_LCWU:
				get_wu (wu, pc, length);
				printf ("LCWU\t%u", wu);
				break;
			case O_LCWS:
				get_ws (ws, pc, length);
				printf ("LCWS\t%u", ws);
				break;
			case O_LCL:
				get_ls (ls, pc, length);
				printf ("LCL\t%" PRIu64, ls);
				break;
			case O_LCF:
				get_f (f, pc, length);
				printf ("LCF\t%f", f);
				break;
			case O_LCD:
				get_d (d, pc, length);
				printf ("LCD\t%f", d);
				break;
#ifdef LONGDOUBLE
			case O_LCO:
				get_o (o, pc, length);
				printf ("LCO\t%Lf", o);
				break;
#endif
			case O_LCS:
				printf ("LCS\t%s", pc);
				su = strlen ((char *) pc);
				pc += su + 1;
				length -= su + 1;
				break;

			/* Load payload data: */
			case O_LDBU:
				get_su (su, pc, length);
				printf ("LDBU\t%u", su);
				break;
			case O_LDBS:
				get_su (su, pc, length);
				printf ("LDBS\t%u", su);
				break;
			case O_LDSU:
				get_su (su, pc, length);
				printf ("LDSU\t%u", su);
				break;
			case O_LDSS:
				get_su (su, pc, length);
				printf ("LDSS\t%u", su);
				break;
			case O_LDWU:
				get_su (su, pc, length);
				printf ("LDWU\t%u", su);
				break;
			case O_LDWS:
				get_su (su, pc, length);
				printf ("LDWS\t%u", su);
				break;
			case O_LDL:
				get_su (su, pc, length);
				printf ("LDL\t%u", su);
				break;
			case O_LDF:
				get_su (su, pc, length);
				printf ("LDF\t%u", su);
				break;
			case O_LDD:
				get_su (su, pc, length);
				printf ("LDD\t%u", su);
				break;
#ifdef LONGDOUBLE
			case O_LDO:
				get_su (su, pc, length);
				printf ("LDO\t%u", su);
				break;
#endif
			case O_LDS:
				get_su (su, pc, length);
				printf ("LDS\t%u", su);
				break;

			/* Load parameter data: */
			case O_LPBU:
				printf ("LPBU\t%u", *pc++);
				length--;
				break;
			case O_LPBS:
				printf ("LPBS\t%u", *pc++);
				length--;
				break;
			case O_LPSU:
				printf ("LPSU\t%u", *pc++);
				length--;
				break;
			case O_LPSS:
				printf ("LPSS\t%u", *pc++);
				length--;
				break;
			case O_LPWU:
				printf ("LPWU\t%u", *pc++);
				length--;
				break;
			case O_LPWS:
				printf ("LPWS\t%u", *pc++);
				length--;
				break;
			case O_LPL:
				printf ("LPL\t%u", *pc++);
				length--;
				break;
			case O_LPF:
				printf ("LPF\t%u", *pc++);
				length--;
				break;
			case O_LPD:
				printf ("LPD\t%u", *pc++);
				length--;
				break;
			case O_LPO:
				printf ("LPO\t%u", *pc++);
				length--;
				break;
			case O_LPS:
				printf ("LPS\t%u", *pc++);
				length--;
				break;
			
			/* Comparisons: */
			case O_CMPWU:
				printf ("CMPWU");
				break;
			case O_CMPWS:
				printf ("CMPWS");
				break;
			case O_CMPLU:
				printf ("CMPLU");
				break;
			case O_CMPLS:
				printf ("CMPLS");
				break;
			case O_CMPD:
				printf ("CMPD");
				break;
#ifdef LONGDOUBLE
			case O_CMPO:
				printf ("CMPO");
				break;
#endif
			case O_CMPS:
				printf ("CMPS");
				break;
			case O_BTWWU:
				printf ("BTWWU");
				break;
			case O_BTWWS:
				printf ("BTWWS");
				break;
			case O_BTWLU:
				printf ("BTWLU");
				break;
			case O_BTWLS:
				printf ("BTWLS");
				break;
			case O_BTWD:
				printf ("BTWD");
				break;
#ifdef LONGDOUBLE
			case O_BTWO:
				printf ("BTWO");
				break;
#endif
			case O_LIKE:
				printf ("LIKE");
				break;

			/* Branches: */
			case O_BEQ:
				get_su (su, pc, length);
				printf ("BEQ\t%u", su);
				break;
			case O_BNE:
				get_su (su, pc, length);
				printf ("BNE\t%u", su);
				break;
			case O_BGT:
				get_su (su, pc, length);
				printf ("BGT\t%u", su);
				break;
			case O_BLE:
				get_su (su, pc, length);
				printf ("BLE\t%u", su);
				break;
			case O_BLT:
				get_su (su, pc, length);
				printf ("BLT\t%u", su);
				break;
			case O_BGE:
				get_su (su, pc, length);
				printf ("BGE\t%u", su);
				break;
			case O_BT:
				get_su (su, pc, length);
				printf ("BT\t%u", su);
				break;
			case O_BF:
				get_su (su, pc, length);
				printf ("BF\t%u", su);
				break;

			/* Conversions: */
			case O_WU2L:
				printf ("WU2L");
				break;
			case O_WS2L:
				printf ("WS2L");
				break;
			case O_WU2D:
				printf ("WU2D");
				break;
			case O_WS2D:
				printf ("WS2D");
				break;
			case O_WU2O:
				printf ("WU2O");
				break;
			case O_WS2O:
				printf ("WS2O");
				break;
			case O_LU2D:
				printf ("LU2D");
				break;
			case O_LS2D:
				printf ("LS2D");
				break;
			case O_LU2O:
				printf ("LU2O");
				break;
			case O_LS2O:
				printf ("LS2O");
				break;
			case O_D2O:
				printf ("D2O");
				break;
			case O_S2W:
				printf ("S2W");
				break;
			case O_S2L:
				printf ("S2L");
				break;
			case O_S2D:
				printf ("S2D");
				break;
			case O_S2O:
				printf ("S2O");
				break;
			case O_C2S:
				printf ("C2S");
				break;

			/* Various: */
			case O_CREF:
				get_su (su, pc, length);
				printf ("CREF\t%u", su);
				break;
			case O_DISC:
				get_wu (ws, pc, length);
				printf ("DISC\t%d", ws);
				break;
			case O_TREF:
				printf ("TREF\t`%s'", pc);
				su = strlen ((char *) pc);
				pc += su + 1;
				length -= su + 1;
				break;
			case O_FCR:
				get_su (su, pc, length);
				printf ("FCR\t%u", su);
				break;
			case O_FOFS:
				get_su (su, pc, length);
				printf ("FOFS\t%u", su);
				break;
			case O_DS0:
				printf ("DS0");
				break;
			case O_DS1:
				printf ("DS1");
				break;

			/* Returns: */
			case O_RET:
				printf ("RET");
				break;
			case O_RETC:
				printf ("RETC");
				break;
			case O_RETT:
				printf ("RETT");
				break;
			case O_RETF:
				printf ("RETF");
				break;
			case O_CRFF:
				printf ("CRFF");
				break;
			case O_CRFT:
				printf ("CRFT");
				break;
			case O_CRTF:
				printf ("CRTF");
				break;
			case O_CRTT:
				printf ("CRTT");
				break;
			case O_CRNE:
				printf ("CRNE");
				break;

			/* Not a valid opcode! */
			default:
				printf ("?(%u)", op);
				break;

		}
		printf ("\r\n");
	}
}

#endif

int bc_init (void)
{
	PROF_INIT ("BC-PROG", bc_prog_pid);
	return (BC_OK);
}

