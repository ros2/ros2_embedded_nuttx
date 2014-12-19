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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include "cdump.h"
#include "idlparser_support.h"

static FILE *c_fp;
GHashTable *sequences;

void c_dump_print(FILE * fp, char **out, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (out) {
		char *tmp;
		vasprintf(&tmp, fmt, ap);

		if (fp)
			fprintf(fp, tmp);

		if (*out) {
			*out = realloc(*out, strlen(*out) + strlen(tmp) + 1);
			strcpy((*out) + strlen(*out), tmp);
			free(tmp);
		} else {
			*out = tmp;
		}
	} else if (fp) {
		vfprintf(fp, fmt, ap);
	}
	va_end(ap);
}

/* Indentation {{{ */
static int c_indent_level = 0;

static void c_indent()
{
	int i;

	for (i = 0; i < c_indent_level; i++)
		fprintf(c_fp, "   ");
}

/* }}} */

/* Aliases {{{ */
void c_dump_alias(AliasType * t, FILE * fp, char **out)
{
	c_dump_print(fp, out, "%s", t->type.name);
}

/* }}}*/

/* Arrays {{{ */
void c_dump_array_type_pre(ArrayType * t, FILE * fp, char **out)
{
	c_dump_type_pre(t->collection.element_type, ' ', 1, fp, out);
}

void c_dump_array_type_post(ArrayType * t, int expand, FILE * fp, char **out)
{
	int i;

	for (i = 0; i < t->nbounds; i++) {
		if (expand == 1)
			c_dump_print(fp, out, "[%d]", t->bound[i]);
		else
			c_dump_print(fp, out, "_%d", t->bound[i]);
	}
	c_dump_type_post(t->collection.element_type, 1, fp, out);
}

/* }}} */

/* Strings {{{ */
void c_dump_string_type_pre(StringType * t, char sep, FILE * fp, char **out)
{
	if (t->bound != UNBOUNDED_COLLECTION) {
		c_dump_print(fp, out, "char");
	} else {
		if (sep == ' ')
			c_dump_print(fp, out, "char *");
		else
			c_dump_print(fp, out, "char%cpointer", sep);
	}
}

void c_dump_string_type_post(StringType * t, int expand, FILE * fp, char **out)
{
	if (t->bound != UNBOUNDED_COLLECTION) {
		if (expand == 1)
			c_dump_print(fp, out, "[%d]", t->bound);
		else
			c_dump_print(fp, out, "_%d", t->bound);
	}
}

/* }}} */

/* Enums {{{ */
void c_dump_enum_const(EnumConst * c, FILE * fp, char **out)
{
	c_indent();
	c_dump_print(fp, out, "%s", c->name);
}

void c_dump_enum_type(EnumerationType * e, FILE * fp, char **out)
{
	int i;

	TypeList *tl = lookup_type_list(e->type.name);

	if (tl && tl->c_dumped == 1) {
		c_dump_print(fp, out, "enum %s", e->type.name);
	} else {
		c_dump_print(fp, out, "enum %s {\n", e->type.name);
		c_indent_level++;
		for (i = 0; i < e->nconsts; i++) {

			c_dump_enum_const(&e->constant[i], fp, out);
			if (i != (e->nconsts - 1))
				c_dump_print(fp, out, ",\n");
			else
				c_dump_print(fp, out, "\n");
		}
		c_indent_level--;
		c_indent();
		c_dump_print(fp, out, "}");
		if (tl)
			tl->c_dumped = 1;
	}
}

/* }}} */

void c_dump_union_member(UnionMember * m, FILE * fp, char **out)
{
	c_dump_member(&m->member, fp, out);

}

void c_dump_union_type(UnionType * u, FILE * fp, char **out)
{
	int i;

	c_dump_print(fp, out, "struct _%s_st {\n", u->type.name);
	c_indent_level++;
	c_indent();
	c_dump_type_pre(u->base_type, ' ', 1, fp, out);
	c_dump_print(fp, out, " discriminant;\n");
	c_indent();
	c_dump_print(fp, out, "union %s {\n", u->type.name);
	c_indent_level++;
	for (i = 0; i < u->nmembers; i++) {
		c_indent();
		c_dump_union_member(&u->member[i], fp, out);
	}
	c_indent_level--;
	c_indent();
	c_dump_print(fp, out, "} u;\n");
	c_indent_level--;
	c_indent();
	c_dump_print(fp, out, "}");
}

void c_dump_type_pre(Type * t, char sep, int expand, FILE * fp, char **out)
{
	switch (t->kind) {
	case NO_TYPE:
		c_dump_print(fp, out, "/* */");
		break;
	case BYTE_TYPE:
	case BOOLEAN_TYPE:
		c_dump_print(fp, out, "unsigned%cchar", sep);
		break;
	case INT_16_TYPE:
		c_dump_print(fp, out, "short");
		break;
	case UINT_16_TYPE:
		c_dump_print(fp, out, "unsigned%cshort", sep);
		break;
	case INT_32_TYPE:
		c_dump_print(fp, out, "int");
		break;
	case UINT_32_TYPE:
		c_dump_print(fp, out, "unsigned%cint", sep);
		break;
	case INT_64_TYPE:
		c_dump_print(fp, out, "long%clong", sep);
		break;
	case UINT_64_TYPE:
		c_dump_print(fp, out, "unsigned%clong%clong", sep, sep);
		break;
	case FLOAT_32_TYPE:
		c_dump_print(fp, out, "float");
		break;
	case FLOAT_64_TYPE:
		c_dump_print(fp, out, "double");
		break;
	case FLOAT_128_TYPE:
		c_dump_print(fp, out, "long%cdouble", sep);
		break;
	case CHAR_8_TYPE:
		c_dump_print(fp, out, "char");
		break;
	case CHAR_32_TYPE:
		c_dump_print(fp, out, "wchar");
		break;
	case ENUMERATION_TYPE:
		c_dump_enum_type((EnumerationType *) t, fp, out);
		break;
	case BITSET_TYPE:
		c_dump_print(fp, out, "TODO");
		break;
	case ALIAS_TYPE:
		c_dump_alias((AliasType *) t, fp, out);
		break;
	case ARRAY_TYPE:
		c_dump_array_type_pre((ArrayType *) t, fp, out);
		break;
	case SEQUENCE_TYPE:
		c_dump_type_pre(((SequenceType *)
				 t)->collection.element_type, '_',
				expand == 1 ? 2 : expand, fp, out);
		c_dump_type_post(((SequenceType *)
				  t)->collection.element_type,
				 expand == 1 ? 2 : expand, fp, out);
		c_dump_print(fp, out, "_seq");
		break;
	case STRING_TYPE:
		c_dump_string_type_pre((StringType *) t, sep, fp, out);
		break;
	case MAP_TYPE:
		c_dump_print(fp, out, "map TODO");
		break;
	case UNION_TYPE:
		c_dump_union_type((UnionType *) t, fp, out);
		break;
	case STRUCTURE_TYPE:
		c_dump_struct_type((StructureType *) t, expand, fp, out);
		break;
	case ANNOTATION_TYPE:
		c_dump_print(fp, out, "boolean");
		break;
	}
}

void c_dump_type_post(Type * t, int expand, FILE * fp, char **out)
{
	switch (t->kind) {
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
	case ALIAS_TYPE:
		break;
	case ARRAY_TYPE:
		c_dump_array_type_post((ArrayType *) t, expand, fp, out);
		break;
	case STRING_TYPE:
		c_dump_string_type_post((StringType *) t, expand, fp, out);
		break;
	case SEQUENCE_TYPE:
	case MAP_TYPE:
	case UNION_TYPE:
	case STRUCTURE_TYPE:
	case ANNOTATION_TYPE:
		break;
	}
}

void c_dump_struct_type(StructureType * s, int expand, FILE * fp, char **out)
{
	int i;

	TypeList *tl = lookup_type_list(s->type.name);

	if (expand == 2) {
		c_dump_print(fp, out, "%s_t", s->type.name);
	} else if (!expand || (tl && tl->c_dumped == 1)) {
		c_dump_print(fp, out, "struct _%s_st", s->type.name);
	} else {

		c_dump_print(fp, out, "struct _%s_st {\n", s->type.name);
		c_indent_level++;
		for (i = 0; i < s->nmembers; i++) {
			c_dump_member(&s->member[i], fp, out);
		}
		c_indent_level--;
		c_indent();
		c_dump_print(fp, out, "}");
		if (tl)
			tl->c_dumped = 1;
	}
}

void c_dump_contained_sequences(Type * t, FILE * fp)
{
	int i;
	StructureType *s;
	Type *e;
	switch (t->kind) {
	case STRUCTURE_TYPE:
		s = (StructureType *) t;
		for (i = 0; i < s->nmembers; i++) {
			c_dump_contained_sequences(s->member[i].type_id, fp);
		}

		break;
	case SEQUENCE_TYPE:
		e = ((SequenceType *) t)->collection.element_type;
		char *seq_name = NULL;

		if ((e->kind == ARRAY_TYPE)
		    || ((e->kind == STRING_TYPE)
			&& (((StringType *) e)->bound != UNBOUNDED_COLLECTION))) {
			fprintf(fp, "typedef ");
			c_dump_type_pre((((SequenceType *) t)->collection.
					 element_type), ' ', 1, fp, NULL);
			fprintf(fp, " ");
			c_dump_type_pre((((SequenceType *) t)->collection.
					 element_type), ' ', 0, fp, NULL);
			c_dump_type_post((((SequenceType *) t)->collection.
					  element_type), 0, fp, NULL);
			c_dump_type_post((((SequenceType *) t)->collection.
					  element_type), 1, fp, NULL);
			fprintf(fp, ";\n");
		}

		c_dump_type_pre((((SequenceType *) t)->collection.element_type),
				'_', 2, NULL, &seq_name);
		c_dump_type_post((((SequenceType *) t)->collection.
				  element_type), 2, NULL, &seq_name);

		if (!g_hash_table_lookup(sequences, seq_name)) {
			g_hash_table_insert(sequences, seq_name, seq_name);

			fprintf(fp, "DDS_SEQUENCE(");
			c_dump_type_pre((((SequenceType *) t)->collection.
					 element_type), ' ', 0, fp, NULL);
			c_dump_type_post((((SequenceType *) t)->collection.
					  element_type), 0, NULL, NULL);

			fprintf(fp, ", ");
			fprintf(fp, "%s_seq);\n", seq_name);
		}
		break;
	case ALIAS_TYPE:
		c_dump_contained_sequences(((AliasType *) t)->base_type, fp);
		break;
	default:
		break;
	}
}

void c_dump_toplevel(Type * type)
{
	c_dump_contained_sequences(type, c_fp);
	switch (type->kind) {
	case STRUCTURE_TYPE:
		fprintf(c_fp, "typedef ");
		c_dump_struct_type((StructureType *) type, 1, c_fp, NULL);
		fprintf(c_fp, " %s;\n\n", type->name);
		break;
	case ENUMERATION_TYPE:
		break;
	case ALIAS_TYPE:
		fprintf(c_fp, "typedef ");
		c_dump_type_pre(((AliasType *) type)->base_type, ' ', 1, c_fp,
				NULL);
		fprintf(c_fp, " %s", type->name);
		c_dump_type_post(((AliasType *) type)->base_type, 1, c_fp,
				 NULL);
		fprintf(c_fp, ";\n\n");
		break;
	default:
		break;
	}
}

void c_dump_member(Member * m, FILE * fp, char **out)
{
	c_indent();
	c_dump_type_pre(m->type_id, ' ', 1, fp, out);
	fprintf(c_fp, " %s", m->name);
	c_dump_type_post(m->type_id, 1, fp, out);
	fprintf(c_fp, ";\n");
}

void c_dump_set_output(FILE * fp)
{
	c_fp = fp;
	sequences = g_hash_table_new(g_str_hash, g_str_equal);
}

/* vim: set foldmethod=marker foldmarker={{{,}}} formatoptions=tcqlron cinoptions=\:0,l1,t0,g0 noexpandtab tabstop=8 shiftwidth=8 textwidth=78: */
