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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "tsmdump.h"
#include "typeobj.h"
#include "typeobj_manip.h"

static FILE *tsm_fp;

/* Aliases {{{ */
void tsm_dump_alias(AliasType * t)
{
	fprintf(tsm_fp, "%s", t->type.name);
}

/* }}}*/

/* Strings {{{ */
void tsm_dump_string_type(StringType * t)
{
	fprintf(tsm_fp, "string");
	if (t->bound != UNBOUNDED_COLLECTION) {
		fprintf(tsm_fp, "<%d>", t->bound);
	}
}

/* }}} */

void tsm_dump_type_pre(Type * t)
{
/*		       CDR_TYPECODE_FIXED,
		       CDR_TYPECODE_WSTRING,

		       */

	switch (t->kind) {
	case NO_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_UNKNOWN  ");
		break;
	case BOOLEAN_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_BOOLEAN  ");
		break;
	case BYTE_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_OCTET    ");
		break;
	case INT_16_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_SHORT    ");
		break;
	case UINT_16_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_USHORT   ");
		break;
	case INT_32_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_LONG     ");
		break;
	case UINT_32_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_ULONG    ");
		break;
	case INT_64_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_LONGLONG ");
		break;
	case UINT_64_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_ULONGLONG");
		break;
	case FLOAT_32_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_FLOAT    ");
		break;
	case FLOAT_64_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_DOUBLE   ");
		break;
	case FLOAT_128_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_LONGDOUBLE");
		break;
	case CHAR_8_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_CHAR     ");
		break;
	case CHAR_32_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_WCHAR    ");
		break;
	case ENUMERATION_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_ENUM     ");
		break;
	case BITSET_TYPE:
		fprintf(tsm_fp, "BITSET TODO           ");
		break;
	case ALIAS_TYPE:
		if ((((AliasType *) t)->base_type)->kind == STRUCTURE_TYPE)
			fprintf(tsm_fp, "CDR_TYPECODE_TYPEREF  ");
		else
			tsm_dump_type_pre(((AliasType *) t)->base_type);
		break;
	case ARRAY_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_ARRAY    ");
		break;
	case SEQUENCE_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_SEQUENCE ");
		break;
	case STRING_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_CSTRING  ");
		break;
	case MAP_TYPE:
		fprintf(tsm_fp, "MAP TODO");
		break;
	case UNION_TYPE:
		fprintf(tsm_fp, "CDR_TYPECODE_UNION    ");
		break;
	case STRUCTURE_TYPE:
		if (lookup_type_list(t->name))
			fprintf(tsm_fp, "CDR_TYPECODE_TYPEREF  ");
		else
			fprintf(tsm_fp, "CDR_TYPECODE_STRUCT   ");
		break;
	case ANNOTATION_TYPE:
		fprintf(tsm_fp, "ANNOTATION TODO       ");
		break;
	}
}

void tsm_dump_type_ref(Type * t)
{
	switch (t->kind) {
	case ALIAS_TYPE:
		if ((((AliasType *) t)->base_type)->kind == STRUCTURE_TYPE)
			fprintf(tsm_fp, "%s_tsm}", ((AliasType *) t)->base_type->name);	/*  */
		else
			tsm_dump_type_ref(((AliasType *) t)->base_type);
		break;
	case STRUCTURE_TYPE:
		if (lookup_type_list(t->name))
			fprintf(tsm_fp, "%s_tsm}", t->name);	/*  */
		else
			fprintf(tsm_fp, "NULL}");	/*  */
		break;
	case STRING_TYPE:
	case NO_TYPE:
	case BOOLEAN_TYPE:
	case BYTE_TYPE:
	case INT_16_TYPE:
	case UINT_16_TYPE:
	case INT_32_TYPE:
	case UINT_32_TYPE:
	case INT_64_TYPE:
	case UINT_64_TYPE:
	case FLOAT_32_TYPE:
	case FLOAT_64_TYPE:
	case FLOAT_128_TYPE:
	case CHAR_8_TYPE:
	case CHAR_32_TYPE:
	case ENUMERATION_TYPE:
	case BITSET_TYPE:
	case ARRAY_TYPE:
	case SEQUENCE_TYPE:
	case MAP_TYPE:
	case UNION_TYPE:
	case ANNOTATION_TYPE:
		fprintf(tsm_fp, "NULL}");	/*  */
		break;
	}
}

void tsm_dump_type_size(Type * t)
{
	switch (t->kind) {
	case ALIAS_TYPE:
		if ((((AliasType *) t)->base_type)->kind == STRUCTURE_TYPE)
			fprintf(tsm_fp, "0, ");	/*  */
		else
			tsm_dump_type_size(((AliasType *) t)->base_type);
		break;
	case STRUCTURE_TYPE:
		if (lookup_type_list(t->name))
			fprintf(tsm_fp, "0, ");	/*  */
		else
			fprintf(tsm_fp, "sizeof (struct _%s_st), ", t->name);
		break;
	case STRING_TYPE:
		fprintf(tsm_fp, "%d, ", ((StringType *) t)->bound);
		break;
	case UNION_TYPE:
		fprintf(tsm_fp, "sizeof (struct _%s_st), ", t->name);
		break;
	case NO_TYPE:
	case BOOLEAN_TYPE:
	case BYTE_TYPE:
	case INT_16_TYPE:
	case UINT_16_TYPE:
	case INT_32_TYPE:
	case UINT_32_TYPE:
	case INT_64_TYPE:
	case UINT_64_TYPE:
	case FLOAT_32_TYPE:
	case FLOAT_64_TYPE:
	case FLOAT_128_TYPE:
	case CHAR_8_TYPE:
	case CHAR_32_TYPE:
	case ENUMERATION_TYPE:
	case BITSET_TYPE:
	case ARRAY_TYPE:
	case SEQUENCE_TYPE:
	case MAP_TYPE:
	case ANNOTATION_TYPE:
		fprintf(tsm_fp, "0, ");
		break;
	}
}

void tsm_dump_type_bound(Type * t)
{
	switch (t->kind) {
	case ENUMERATION_TYPE:
		fprintf(tsm_fp, "%d, ", ((EnumerationType *) t)->nconsts);
		break;
	case STRUCTURE_TYPE:
		if (lookup_type_list(t->name))
			fprintf(tsm_fp, "0, ");	/*  */
		else
			fprintf(tsm_fp, "%d, ",
				((StructureType *) t)->nmembers);
		break;
	case UNION_TYPE:
		fprintf(tsm_fp, "%d, ", ((UnionType *) t)->nmembers);
		break;
	case ARRAY_TYPE:
		fprintf(tsm_fp, "%d, ", ((ArrayType *) t)->bound[0]);
		break;
	case SEQUENCE_TYPE:
		fprintf(tsm_fp, "%d, ", ((SequenceType *) t)->bound);
		break;
	case STRING_TYPE:
	case NO_TYPE:
	case BOOLEAN_TYPE:
	case BYTE_TYPE:
	case INT_16_TYPE:
	case UINT_16_TYPE:
	case INT_32_TYPE:
	case UINT_32_TYPE:
	case INT_64_TYPE:
	case UINT_64_TYPE:
	case FLOAT_32_TYPE:
	case FLOAT_64_TYPE:
	case FLOAT_128_TYPE:
	case CHAR_8_TYPE:
	case CHAR_32_TYPE:
	case BITSET_TYPE:
	case MAP_TYPE:
	case ANNOTATION_TYPE:
		fprintf(tsm_fp, "0, ");
		break;
	case ALIAS_TYPE:
		if ((((AliasType *) t)->base_type)->kind == STRUCTURE_TYPE)
			fprintf(tsm_fp, "0, ");	/*  */
		else
			tsm_dump_type_bound(((AliasType *) t)->base_type);
		break;
	}
}

void tsm_dump_enumconst(EnumerationType * e, EnumConst * c)
{
	fprintf(tsm_fp,
		"{CDR_TYPECODE_LONG     , 0, \"%s\", 0, 0, 0, %d, NULL}",
		c->name, c->value);
}

void tsm_dump_type_post(Type * t, char *member_name);

void tsm_dump_unionmember(UnionType * u, UnionMember * m, char *parent_name)
{
	int i;

	if (m->member.is_union_default) {
		fprintf(stderr, "Tsm does not support defaults in union\n");
		exit(0);
	}

	for (i = 0; i < m->nlabels; i++) {
		fprintf(tsm_fp, "{");
		tsm_dump_type_pre(m->member.type_id);
		fprintf(tsm_fp, ", ");
		fprintf(tsm_fp, "0, ");
		fprintf(tsm_fp, "\"%s\", ", m->member.name);
		tsm_dump_type_size(m->member.type_id);
		fprintf(tsm_fp, "offsetof(struct _%s_st,u), ", u->type.name);
		tsm_dump_type_bound(m->member.type_id);
		fprintf(tsm_fp, "%d, ", m->nlabels > 1 ? m->label.list[i] : m->label.value);	/*  */
		fprintf(tsm_fp, "NULL}");	/*  */
		if (i != m->nlabels - 1)
			fprintf(tsm_fp, ",\n");
		tsm_dump_type_post(m->member.type_id, m->member.name);
	}
}

void tsm_dump_collection_type(Type * t)
{
	fprintf(tsm_fp, "{");
	tsm_dump_type_pre(t);
	fprintf(tsm_fp, ", ");
	fprintf(tsm_fp, "0, ");
	fprintf(tsm_fp, "\"no name\", ");
	tsm_dump_type_size(t);
	fprintf(tsm_fp, "0, ");
	tsm_dump_type_bound(t);
	fprintf(tsm_fp, "0, ");	/*  */
	tsm_dump_type_ref(t);
	tsm_dump_type_post(t, "no name");
}

void tsm_dump_type_post(Type * t, char *member_name)
{
	StructureType *s;
	SequenceType *se;
	EnumerationType *e;
	UnionType *u;
	ArrayType *a;
	int i;

	switch (t->kind) {
	case STRUCTURE_TYPE:
		if (!lookup_type_list(t->name)) {
			s = (StructureType *) t;
			if (s->nmembers)
				fprintf(tsm_fp, ",\n");
			else
				fprintf(tsm_fp, "\n");
			for (i = 0; i < s->nmembers; i++) {
				tsm_dump_member(s, &s->member[i]);
				if (i != s->nmembers - 1)
					fprintf(tsm_fp, ",\n");
			}
		}
		break;
	case ENUMERATION_TYPE:
		e = (EnumerationType *) t;
		if (e->nconsts)
			fprintf(tsm_fp, ",\n");
		else
			fprintf(tsm_fp, "\n");
		for (i = 0; i < e->nconsts; i++) {
			tsm_dump_enumconst(e, &e->constant[i]);
			if (i != e->nconsts - 1)
				fprintf(tsm_fp, ",\n");
		}
		break;
	case UNION_TYPE:
		u = (UnionType *) t;
		if (u->nmembers)
			fprintf(tsm_fp, ",\n");
		else
			fprintf(tsm_fp, "\n");
		for (i = 0; i < u->nmembers; i++) {
			tsm_dump_unionmember(u, &u->member[i], member_name);
			if (i != u->nmembers - 1)
				fprintf(tsm_fp, ",\n");
		}
		break;
	case ARRAY_TYPE:
		a = (ArrayType *) t;
		fprintf(tsm_fp, ",\n");
		for (i = 1; i < a->nbounds; i++)
			fprintf(tsm_fp,
				"{CDR_TYPECODE_ARRAY    , 0 ,\"noname\", 0, 0, %d, 0, NULL},\n",
				a->bound[i]);
		tsm_dump_collection_type(a->collection.element_type);
		break;
	case SEQUENCE_TYPE:
		se = (SequenceType *) t;
		fprintf(tsm_fp, ",\n");
		tsm_dump_collection_type(se->collection.element_type);
		break;
	default:
		break;
	}
}

int tsm_struct_has_keys(StructureType * s);

int tsm_member_has_dyndata(Member * m)
{
	switch (m->type_id->kind) {
	case STRING_TYPE:
		if (((StringType *) m->type_id)->bound == UNBOUNDED_COLLECTION) {
			return 1;
		}
		break;
	case SEQUENCE_TYPE:
		if (((SequenceType *) m->type_id)->bound ==
		    UNBOUNDED_COLLECTION) {
			return 1;
		}
		break;
	default:
		break;
	}
	if (m->type_id->kind == STRUCTURE_TYPE) {
		int i;
		StructureType *s = (StructureType *) m->type_id;
		for (i = 0; i < s->nmembers; i++) {
			if (tsm_member_has_dyndata(&s->member[i])) {
				return 1;
			}
		}
	}
	return 0;
}

void tsm_dump_member(StructureType * s, Member * m)
{
	int has_keys = member_contains_keys(m);
	int has_dyn = tsm_member_has_dyndata(m);
	fprintf(tsm_fp, "{");
	tsm_dump_type_pre(m->type_id);
	fprintf(tsm_fp, ", ");
	if (has_keys && has_dyn)
		fprintf(tsm_fp, "TSMFLAG_KEY|TSMFLAG_DYNAMIC, ");	/* Print attributes here */
	else if (has_keys)
		fprintf(tsm_fp, "TSMFLAG_KEY, ");	/* Print attributes here */
	else if (has_dyn)
		fprintf(tsm_fp, "TSMFLAG_DYNAMIC, ");	/* Print attributes here */
	else
		fprintf(tsm_fp, "0, ");	/* Print attributes here */

	fprintf(tsm_fp, "\"%s\", ", m->name);
	tsm_dump_type_size(m->type_id);
	fprintf(tsm_fp, "offsetof (struct _%s_st, %s), ", s->type.name,
		m->name);
	tsm_dump_type_bound(m->type_id);
	fprintf(tsm_fp, "0, ");	/*  */
	tsm_dump_type_ref(m->type_id);
	tsm_dump_type_post(m->type_id, m->name);
}

void tsm_dump_struct_type(StructureType * s)
{
	int i;
	int struct_has_keys = structure_contains_keys(s);
	int struct_has_dyndata = 0;
	for (i = 0; i < s->nmembers; i++) {
		if (tsm_member_has_dyndata(&s->member[i])) {
			struct_has_dyndata = 1;
			break;
		}
	}

	fprintf(tsm_fp, "static DDS_TypeSupport_meta %s_tsm [] = {\n",
		s->type.name);
	fprintf(tsm_fp, "{CDR_TYPECODE_STRUCT   , ");
	if (struct_has_keys && struct_has_dyndata)
		fprintf(tsm_fp, "TSMFLAG_KEY|TSMFLAG_DYNAMIC, ");	/* Print attributes here */
	else if (struct_has_keys)
		fprintf(tsm_fp, "TSMFLAG_KEY, ");	/* Print attributes here */
	else if (struct_has_dyndata)
		fprintf(tsm_fp, "TSMFLAG_DYNAMIC, ");	/* Print attributes here */
	else
		fprintf(tsm_fp, "0, ");	/* Print attributes here */
	fprintf(tsm_fp, "\"%s\", ", s->type.name);
	fprintf(tsm_fp, "sizeof (struct _%s_st), ", s->type.name);
	fprintf(tsm_fp, "0, ");	/*  */
	fprintf(tsm_fp, "%d, ", s->nmembers);
	fprintf(tsm_fp, "0, ");	/*  */
	fprintf(tsm_fp, "NULL}");	/*  */
	if (s->nmembers)
		fprintf(tsm_fp, ",\n");
	else
		fprintf(tsm_fp, "\n");
	for (i = 0; i < s->nmembers; i++) {
		tsm_dump_member(s, &s->member[i]);
		if (i != s->nmembers - 1)
			fprintf(tsm_fp, ",\n");
	}
	fprintf(tsm_fp, "\n};\n");
}

void tsm_dump_set_output(FILE * fp)
{
	tsm_fp = fp;
}

/* vim: set foldmethod=marker foldmarker={{{,}}} formatoptions=tcqlron cinoptions=\:0,l1,t0,g0 noexpandtab tabstop=8 shiftwidth=8 textwidth=78: */
