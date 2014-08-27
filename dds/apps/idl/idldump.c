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
#include "idldump.h"
#include "idlparser_support.h"

/* Indentation {{{ */
static int idl_indent_level = 0;

static void idl_indent()
{
	int i;

	for (i = 0; i < idl_indent_level; i++)
		printf("   ");
}

/* }}} */

/* Aliases {{{ */
void idl_dump_alias(AliasType * t)
{
	printf("%s", t->type.name);
}

/* }}}*/

/* Arrays {{{ */
void idl_dump_array_type_pre(ArrayType * t)
{
	idl_dump_type_pre(t->collection.element_type);
}

void idl_dump_array_type_post(ArrayType * t)
{
	int i;

	for (i = 0; i < t->nbounds; i++) {
		printf("[%d]", t->bound[i]);
	}
	idl_dump_type_post(t->collection.element_type);
}

/* }}} */

/* Strings {{{ */
void idl_dump_string_type(StringType * t)
{
	printf("string");
	if (t->bound != UNBOUNDED_COLLECTION) {
		printf("<%d>", t->bound);
	}
}

/* }}} */

/* Enums {{{ */
void idl_dump_enum_const(EnumConst * c)
{
	idl_indent();
	printf("%s", c->name);
}

void idl_dump_enum_type(EnumerationType * e)
{
	int i;

	TypeList *tl = lookup_type_list(e->type.name);

	if (tl && tl->idl_dumped == 1) {
		printf("%s", e->type.name);
	} else {
		printf("enum %s {\n", e->type.name);
		idl_indent_level++;
		for (i = 0; i < e->nconsts; i++) {

			idl_dump_enum_const(&e->constant[i]);
			if (i != (e->nconsts - 1))
				printf(",\n");
			else
				printf("\n");
		}
		idl_indent_level--;
		idl_indent();
		printf("}");
		if (tl)
			tl->idl_dumped = 1;
	}
}

/* }}} */

/* Sequences {{{ */
void idl_dump_sequence_type(SequenceType * t)
{
	printf("sequence<");
	idl_dump_type_pre(t->collection.element_type);

	if (t->bound != UNBOUNDED_COLLECTION) {
		printf(", %d>", t->bound);
	} else {
		printf(">");
	}
	idl_dump_type_post(t->collection.element_type);

}

/* }}} */

/* Unions {{{ */
void idl_dump_union_member(UnionMember * m)
{
	int i;

	for (i = 0; i < m->nlabels; i++) {
		printf("case %d:",
		       m->nlabels > 1 ? m->label.list[i] : m->label.value);
		if (m->nlabels - 1 != i) {
			printf("\n");
			idl_indent();
		}
	}

	if (m->member.is_union_default == 1) {
		if (m->nlabels)
			printf("\n");
		printf("default :");
	}

	idl_dump_member(&m->member);

}

void idl_dump_union_type(UnionType * u)
{
	int i;

	printf("union %s switch(", u->type.name);
	idl_dump_type_pre(u->base_type);
	printf(") {\n");
	idl_indent_level++;
	for (i = 0; i < u->nmembers; i++) {
		idl_indent();
		idl_dump_union_member(&u->member[i]);
	}
	idl_indent_level--;
	idl_indent();
	printf("}");
}

/* }}} */

/* Structs {{{ */

void idl_dump_struct_type(StructureType * s)
{
	int i;

	TypeList *tl = lookup_type_list(s->type.name);

	if (tl && tl->idl_dumped == 1) {
		printf("%s", s->type.name);
	} else {
		printf("struct %s {\n", s->type.name);
		idl_indent_level++;
		for (i = 0; i < s->nmembers; i++) {
			idl_dump_member(&s->member[i]);
		}
		idl_indent_level--;
		idl_indent();
		printf("}");
		if (tl)
			tl->idl_dumped = 1;
	}
}

void idl_dump_member(Member * m)
{
	idl_indent();
	idl_dump_type_pre(m->type_id);
	printf(" %s", m->name);
	idl_dump_type_post(m->type_id);
	printf(";");
	if (m->is_key)
		printf(" //@Key");
	printf("\n");
}

/* }}} */

/* Simple type dumps {{{ */
void idl_dump_type_pre(Type * t)
{
	switch (t->kind) {
	case NO_TYPE:
		printf("/* */");
		break;
	case BOOLEAN_TYPE:
		printf("boolean");
		break;
	case BYTE_TYPE:
		printf("octet");
		break;
	case INT_16_TYPE:
		printf("short");
		break;
	case UINT_16_TYPE:
		printf("unsigned short");
		break;
	case INT_32_TYPE:
		printf("long");
		break;
	case UINT_32_TYPE:
		printf("unsigned long");
		break;
	case INT_64_TYPE:
		printf("long long");
		break;
	case UINT_64_TYPE:
		printf("unsigned long long");
		break;
	case FLOAT_32_TYPE:
		printf("float");
		break;
	case FLOAT_64_TYPE:
		printf("double");
		break;
	case FLOAT_128_TYPE:
		printf("long double");
		break;
	case CHAR_8_TYPE:
		printf("char");
		break;
	case CHAR_32_TYPE:
		printf("wchar");
		break;
	case ENUMERATION_TYPE:
		idl_dump_enum_type((EnumerationType *) t);
		break;
	case BITSET_TYPE:
		printf("TODO");
		break;
	case ALIAS_TYPE:
		idl_dump_alias((AliasType *) t);
		break;
	case ARRAY_TYPE:
		idl_dump_array_type_pre((ArrayType *) t);
		break;
	case SEQUENCE_TYPE:
		idl_dump_sequence_type((SequenceType *) t);
		break;
	case STRING_TYPE:
		idl_dump_string_type((StringType *) t);
		break;
	case MAP_TYPE:
		printf("map TODO");
		break;
	case UNION_TYPE:
		idl_dump_union_type((UnionType *) t);
		break;
	case STRUCTURE_TYPE:
		idl_dump_struct_type((StructureType *) t);
		break;
	case ANNOTATION_TYPE:
		printf("boolean");
		break;
	}
}

void idl_dump_type_post(Type * t)
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
		idl_dump_array_type_post((ArrayType *) t);
		break;
	case SEQUENCE_TYPE:
	case STRING_TYPE:
	case MAP_TYPE:
	case UNION_TYPE:
	case STRUCTURE_TYPE:
	case ANNOTATION_TYPE:
		break;
	}
}

/* }}} */

void idl_dump_toplevel(Type * type)
{
	switch (type->kind) {
	case STRUCTURE_TYPE:
		idl_dump_struct_type((StructureType *) type);
		printf(";\n\n");
		break;
	case ENUMERATION_TYPE:
		idl_dump_enum_type((EnumerationType *) type);
		printf(";\n\n");
		break;
	case ALIAS_TYPE:
		printf("typedef ");
		idl_dump_type_pre(((AliasType *) type)->base_type);
		printf(" %s", type->name);
		idl_dump_type_post(((AliasType *) type)->base_type);
		printf(";\n\n");
		break;
	default:
		break;
	}
}

/* vim: set foldmethod=marker foldmarker={{{,}}} formatoptions=tcqlron cinoptions=\:0,l1,t0,g0 noexpandtab tabstop=8 shiftwidth=8 textwidth=78: */
