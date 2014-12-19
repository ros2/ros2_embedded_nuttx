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
#include "defsampledump.h"
#include "idlparser_support.h"
#include "string_manip.h"

static FILE *defsample_fp;
GHashTable *sequences;

char *prefix = NULL;
int iter = 0;
void defsample_dump_print(FILE * fp, char **out, char *fmt, ...)
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
static int defsample_indent_level = 0;

static void defsample_indent()
{
	int i;

	for (i = 0; i < defsample_indent_level; i++)
		fprintf(defsample_fp, "   ");
}

/* }}} */

/* Aliases {{{ */
void defsample_dump_alias_pre(AliasType * t, FILE * fp, char **out)
{
	defsample_dump_type_pre(t->base_type, ' ', 1, fp, out);
}

void defsample_dump_alias_post(AliasType * t, FILE * fp, char **out)
{
	defsample_dump_type_post(t->base_type, 1, fp, out);
}

/* }}}*/

/* Arrays {{{ */
void defsample_dump_array_type_pre(ArrayType * t, FILE * fp, char **out)
{
	int i;

	defsample_dump_print(fp, out, "do {\n");
	for (i = 0; i < t->nbounds; i++) {
		defsample_dump_print(fp, out, "int i%d;\n", iter + i);
	}
	for (i = 0; i < t->nbounds; i++) {
		defsample_dump_print(fp, out, "for (i%d=0; i%d<%d; i%d++) {\n",
				     iter, iter, t->bound[i], iter);
		iter++;
	}
	defsample_dump_type_pre(t->collection.element_type, ' ', 1, fp, out);
}

void defsample_dump_array_type_post(ArrayType * t, int expand, FILE * fp,
				    char **out)
{
	int i;

	for (i = 0; i < t->nbounds; i++) {
		defsample_dump_print(fp, out, "[i%d]", iter - t->nbounds + i);
	}
	iter -= t->nbounds;
	defsample_dump_type_post(t->collection.element_type, 1, fp, out);
	for (i = 0; i < t->nbounds; i++) {
		defsample_dump_print(fp, out, ";\n}\n");
	}
	defsample_dump_print(fp, out, "} while(0)");
}

/* }}} */

/* Strings {{{ */
void defsample_dump_string_type_pre(StringType * t, char sep, FILE * fp,
				    char **out)
{
	if (t->bound != UNBOUNDED_COLLECTION) {
		defsample_dump_print(fp, out, prefix);
	} else {
		defsample_dump_print(fp, out, prefix);
	}
}

void defsample_dump_string_type_post(StringType * t, int expand, FILE * fp,
				     char **out)
{
	if (t->bound == UNBOUNDED_COLLECTION) {
		defsample_dump_print(fp, out, "= strdup(\"\")", t->bound);
	} else {
		defsample_dump_print(fp, out, "[0]= 0", t->bound);
	}
}

/* }}} */

/* Enums {{{ */
void defsample_dump_enum_type_post(EnumerationType * e, FILE * fp, char **out)
{
	defsample_dump_print(fp, out, "= %s", e->constant[0].name);
}

/* }}} */

void defsample_dump_union_member(UnionMember * m, FILE * fp, char **out)
{
	int i;

	for (i = 0; i < m->nlabels; i++) {
		defsample_dump_print(defsample_fp, out, "case %d:",
				     m->nlabels >
				     1 ? m->label.list[i] : m->label.value);
		if (m->nlabels - 1 != i) {
			defsample_dump_print(fp, out, "\n");
			defsample_indent();
		}
	}

	if (m->member.is_union_default == 1) {
		if (m->nlabels)
			defsample_dump_print(fp, out, "\n");
		defsample_dump_print(defsample_fp, out, "default :");
	}

	defsample_dump_member(&m->member, fp, out);

}

void defsample_dump_union_type(UnionType * u, FILE * fp, char **out)
{
	char *oldprefix = prefix;

	defsample_dump_print(fp, out, "%s.discriminant = %d;\n", prefix,
			     u->member[0].nlabels > 1
			     ? u->member[0].label.list[0] : u->member[0].
			     label.value);
	prefix = str_append3(prefix, ".", "u");
	defsample_dump_member(&u->member[0].member, fp, out);
	free(prefix);
	prefix = oldprefix;
	/* 
	   defsample_dump_type_pre(u->base_type, ' ', 1, fp, out);
	   defsample_dump_print(fp, out, ") {\n");
	   defsample_indent_level++;
	   for (i = 0; i < u->nmembers; i++) {
	   defsample_indent();
	   defsample_dump_union_member(&u->member[i], fp, out);
	   }
	   defsample_indent_level--;
	   defsample_indent();
	   defsample_dump_print(fp, out, "}"); */
}

void defsample_dump_type_pre(Type * t, char sep, int expand, FILE * fp,
			     char **out)
{
	switch (t->kind) {
	case NO_TYPE:
		defsample_dump_print(fp, out, "/* */");
		break;
	case BYTE_TYPE:
	case BOOLEAN_TYPE:
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
		defsample_dump_print(fp, out, prefix);
		break;
	case STRING_TYPE:
		defsample_dump_string_type_pre((StringType *) t, sep, fp, out);
		break;
	case ALIAS_TYPE:
		defsample_dump_alias_pre((AliasType *) t, fp, out);
		break;
	case ARRAY_TYPE:
		defsample_dump_array_type_pre((ArrayType *) t, fp, out);
		break;
	case SEQUENCE_TYPE:
		defsample_dump_print(fp, out, "DDS_SEQ_INIT(%s);", prefix);
		defsample_dump_print(fp, out, "dds_seq_require(&%s,1)", prefix);
		break;
	case MAP_TYPE:
//              defsample_dump_print(fp, out, "map TODO");
		break;
	case UNION_TYPE:
		defsample_dump_union_type((UnionType *) t, fp, out);
		break;
	case STRUCTURE_TYPE:
		defsample_dump_struct_type((StructureType *) t, expand, fp,
					   out);
		break;
	case ANNOTATION_TYPE:
//              defsample_dump_print(fp, out, "boolean");
		break;
	}
}

void defsample_dump_type_post(Type * t, int expand, FILE * fp, char **out)
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
		defsample_dump_print(fp, out, "= 1");
		break;
	case ENUMERATION_TYPE:
		defsample_dump_enum_type_post((EnumerationType *) t, fp, out);
		break;
	case BITSET_TYPE:
	case ALIAS_TYPE:
		defsample_dump_alias_post((AliasType *) t, fp, out);
		break;
	case ARRAY_TYPE:
		defsample_dump_array_type_post((ArrayType *) t, expand, fp,
					       out);
		break;
	case STRING_TYPE:
		defsample_dump_string_type_post((StringType *) t, expand, fp,
						out);
		break;
	case SEQUENCE_TYPE:
	case MAP_TYPE:
	case UNION_TYPE:
	case ANNOTATION_TYPE:
		break;
	case STRUCTURE_TYPE:
		defsample_dump_struct_type_post((StructureType *) t, fp, out);
		break;
	}
}

void defsample_dump_struct_type(StructureType * s, int expand, FILE * fp,
				char **out)
{
	int i;

	TypeList *tl = lookup_type_list(s->type.name);

	if (tl && tl->defsample_dumped) {
		defsample_dump_print(fp, out, "%s_fill(&%s", s->type.name,
				     prefix);
	} else {
		for (i = 0; i < s->nmembers; i++) {
			defsample_dump_member(&s->member[i], fp, out);
		}
	}

}

void defsample_dump_struct_type_post(StructureType * s, FILE * fp, char **out)
{
	TypeList *tl = lookup_type_list(s->type.name);
	if (tl && tl->defsample_dumped)
		defsample_dump_print(fp, out, ")");
	if (tl)
		tl->defsample_dumped = 1;
}

void defsample_dump_toplevel(Type * type)
{
	switch (type->kind) {
	case STRUCTURE_TYPE:
		fprintf(defsample_fp, "void %s_fill(%s *ret)\n{\n", type->name,
			type->name);
		defsample_dump_struct_type((StructureType *) type, 1,
					   defsample_fp, NULL);
		fprintf(defsample_fp, "}\n\n");
		fprintf(defsample_fp, "%s *%s_create()\n{\n", type->name,
			type->name);
		fprintf(defsample_fp, "%s *ret=malloc(sizeof(%s));\n",
			type->name, type->name);
		fprintf(defsample_fp, "%s_fill(ret);\n", type->name);
		fprintf(defsample_fp, "return ret;\n");
		fprintf(defsample_fp, "}\n\n");
		break;
	case ENUMERATION_TYPE:
		break;
	case ALIAS_TYPE:
/*		fprintf(defsample_fp, "typedef ");
		defsample_dump_type_pre(((AliasType *) type)->base_type, ' ', 1, defsample_fp, NULL);
		fprintf(defsample_fp, " %s", type->name);
		defsample_dump_type_post(((AliasType *) type)->base_type, 1, defsample_fp, NULL);
		fprintf(defsample_fp, ";\n\n");
*/ break;
	default:
		break;
	}
}

void defsample_dump_member(Member * m, FILE * fp, char **out)
{
	char *oldprefix = prefix;
	if (!prefix)
		prefix = strdup("(*ret)");

	defsample_indent();
	prefix = str_append3(prefix, ".", m->name);
	defsample_dump_type_pre(m->type_id, ' ', 1, fp, out);
	defsample_dump_type_post(m->type_id, 1, fp, out);

	defsample_dump_print(fp, out, ";\n");

	free(prefix);
	prefix = oldprefix;

	/*switch (m->type_id->kind) {
	   case STRUCTURE_TYPE:
	   defsample_dump_struct_type((StructureType *) m->type_id, 1, defsample_fp, NULL);
	   break;
	   case ARRAY_TYPE:
	   break;
	   default:
	   fprintf(defsample_fp, "%s.%s = ", prefix, m->name);
	   fprintf(defsample_fp, "\n");
	   } */
}

void defsample_dump_set_output(FILE * fp)
{
	defsample_fp = fp;
	sequences = g_hash_table_new(g_str_hash, g_str_equal);
}

/* vim: set foldmethod=marker foldmarker={{{,}}} formatoptions=tcqlron cinoptions=\:0,l1,t0,g0 noexpandtab tabstop=8 shiftwidth=8 textwidth=78: */
