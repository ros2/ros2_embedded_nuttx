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
#include <stdio.h>
#include "typeobj_manip.h"
#include <string.h>

MapType *create_map_type(Type * key, Type * elem, int bound)
{
	MapType *ret = (MapType *) malloc(sizeof(MapType));
	ret->collection.type.kind = MAP_TYPE;
	ret->collection.element_type = elem;
	ret->key_element_type = key;
	ret->bound = bound;

	/* A collection type's implicit name is the concatenation of a label that
	 * identifies the type of collection (given below), the bound(s)
	 * (for bounded collections, expressed as a decimal integer), the key
	 * element type name (for maps), and the element type name, separated
	 * by underscores.  These names are all in the global namespace.

	 The collection type labels are:
	 - sequence (for type kind SEQUENCE_TYPE)
	 - string (for type kind STRING_TYPE)
	 - map (for type kind MAP_TYPE)
	 - array (for type kind ARRAY_TYPE)

	 For example, the following are all valid implicit type names:
	 - sequence_10_integer
	 - string_widecharacter
	 - sequence_10_string_15_character
	 - map_20_integer_integer
	 - array_12_8_string_64_character */

	return ret;
}

ArrayType *create_array_type(Type * element_type, int nbounds, int *bound)
{
	ArrayType *ret =
	    (ArrayType *) malloc(sizeof(ArrayType) + sizeof(Bound) * nbounds);
	ret->collection.type.kind = ARRAY_TYPE;
	ret->collection.element_type = element_type;
	memcpy(ret->bound, bound, sizeof(Bound) * nbounds);
	ret->nbounds = nbounds;
	return ret;

}

SequenceType *create_sequence_type(Type * elem, int bound)
{
	SequenceType *ret = (SequenceType *) malloc(sizeof(SequenceType));
	ret->collection.type.kind = SEQUENCE_TYPE;
	ret->collection.element_type = elem;
	ret->bound = bound;
	return ret;

}

StructureType *struct_set_members(StructureType * s, MemberList * members)
{
	StructureType *ret = s;
	MemberList *m;
	int i;
	if (members) {
		Member *omembers;
		for (m = members; m != NULL; m = m->next)
			ret->nmembers++;

		ret =
		    realloc(ret,
			    sizeof(StructureType) + (ret->nmembers -
						     1) * sizeof(Member));

		omembers = ret->member;
		i = ret->nmembers - 1;
		for (m = members; m != NULL; m = m->next) {
			memcpy(&omembers[i], m->member, sizeof(Member));
			i--;
		}
	}
	return ret;
}

StructureType *create_structure_type(char *name, Type * base,
				     MemberList * members)
{
	StructureType *ret = malloc(sizeof(StructureType));

	ret->type.name = name;
	ret->base_type = base;
	ret->type.kind = STRUCTURE_TYPE;
	ret->nmembers = 0;

	return struct_set_members(ret, members);
}

UnionType *create_union_type(char *name, Type * t, UnionMemberList * list)
{
	int nmembers = 0;
	UnionMemberList *l;
	UnionType *ret;

	for (l = list; l != NULL; l = l->next)
		nmembers++;

	ret =
	    (UnionType *) malloc(sizeof(UnionType) +
				 (nmembers >
				  0 ? nmembers - 1 : 0) * sizeof(UnionMember));

	ret->type.name = name;
	ret->type.kind = UNION_TYPE;
	ret->base_type = t;
	ret->nmembers = 0;

	for (l = list; l != NULL; l = l->next) {
		memcpy(&ret->member[nmembers - ret->nmembers - 1], l->member,
		       sizeof(UnionMember));
		ret->nmembers++;
	}

	return ret;
}

Type *create_invalid_type()
{
	Type *ret = calloc(1, sizeof(Type));
	return ret;
}

Type *get_primitive_type(TypeKind kind)
{
	Type *ret = malloc(sizeof(Type));
	ret->kind = kind;
	return ret;
}

AliasType *create_alias_type(char *name, Type * base)
{
	AliasType *ret = malloc(sizeof(AliasType));
	ret->type.kind = ALIAS_TYPE;
	ret->type.name = name;
	ret->base_type = base;
	return ret;
}

StringType *create_string_type(int bound)
{
	StringType *ret = (StringType *) malloc(sizeof(StringType));
	ret->collection.type.kind = STRING_TYPE;
	ret->bound = bound;
	return ret;
}

EnumConst *create_enum_const(char *id)
{
	EnumConst *ret = (EnumConst *) malloc(sizeof(EnumConst));
	ret->name = id;
	ret->value = ~0U;
	return ret;
}

EnumConstList *create_enum_const_list(EnumConst * c)
{
	EnumConstList *ret = malloc(sizeof(EnumConstList));
	ret->enumconst = c;
	ret->next = NULL;

	return ret;
}

EnumConstList *enum_const_list_prepend(EnumConstList * list, EnumConst * c)
{
	EnumConstList *ret = malloc(sizeof(EnumConstList));
	ret->enumconst = c;
	ret->next = list;
	return ret;
}

MemberList *create_member_list(Member * c)
{
	MemberList *ret = malloc(sizeof(MemberList));
	ret->member = c;
	ret->next = NULL;
	return ret;
}

MemberList *member_list_join(MemberList * l1, MemberList * l2)
{
	MemberList *i = l1;

	while (i->next)
		i = i->next;
	i->next = l2;
	return l1;
}

UnionMember *create_union_member(Type * t, DeclaratorList * decl)
{
	UnionMember *ret = (UnionMember *) malloc(sizeof(UnionMember));
	ret->nlabels = 0;
	ret->member.is_union_default = 0;

	if (decl->nsizes)
		ret->member.type_id =
		    (Type *) create_array_type(t, decl->nsizes,
					       decl->array_sizes);
	else
		ret->member.type_id = t;

	ret->member.name = decl->name;
	return ret;
}

UnionMember *union_member_add_labels(UnionMember * m, LabelList * l)
{
	UnionMember *ret = m;
	LabelList *i;
	int j;
	for (i = l; i != NULL; i = i->next) {
		if (i->is_default != 1)
			ret->nlabels++;
		else
			ret->member.is_union_default = 1;
	}

	if (ret->nlabels > 1) {
		ret->label.list =
		    (int32_t *) malloc(ret->nlabels * sizeof(int32_t));
		j = 0;
		for (i = l; i != NULL; i = i->next)
			if (i->is_default != 1)
				ret->label.list[j++] = i->label;
	} else if (ret->nlabels == 1) {
		for (i = l; i != NULL; i = i->next) {
			if (i->is_default != 1)
				ret->label.value = i->label;
		}
	}

	return m;
}

UnionMemberList *create_union_member_list(UnionMember * m)
{
	UnionMemberList *ret = (UnionMemberList *)
	    malloc(sizeof(UnionMemberList));

	ret->member = m;
	ret->next = NULL;
	return ret;
}

UnionMemberList *union_member_list_prepend(UnionMemberList * l, UnionMember * m)
{
	UnionMemberList *ret = (UnionMemberList *)
	    malloc(sizeof(UnionMemberList));

	ret->member = m;
	ret->next = l;

	return ret;
}

EnumerationType *create_enumeration_type(char *name, EnumConstList * list)
{
	EnumerationType *ret;
	EnumConstList *i;
	EnumConst *oconst;
	int nconsts = 0;
	int j;
	int id = 0;

	for (i = list; i != NULL; i = i->next)
		nconsts++;

	ret =
	    (EnumerationType *) malloc(sizeof(EnumerationType) +
				       (((nconsts - 1) >
					 0) ? (nconsts -
					       1) : 0) * sizeof(EnumConst));
	ret->type.kind = ENUMERATION_TYPE;
	ret->type.name = name;
	ret->nconsts = nconsts;
	oconst = ret->constant;

	j = 0;
	for (i = list; i != NULL; i = i->next) {
		if (i->enumconst->value == ~0U)
			i->enumconst->value = id;
		else
			id = i->enumconst->value;

		id++;
		memcpy(&oconst[j++], i->enumconst, sizeof(EnumConst));
	}

	return ret;
}

DeclaratorList *declarator_list_prepend(DeclaratorList * l1,
					DeclaratorList * l2)
{
	DeclaratorList *i = l1;
	while (i->next)
		i = i->next;
	i->next = l2;
	return l1;
}

DeclaratorList *declarator_list_set_type(DeclaratorList * l, Type * t)
{
	DeclaratorList *i;

	for (i = l; i != NULL; i = i->next)
		i->type = t;
	return l;
}

MemberList *member_list_from_declarator_list(Type * type, DeclaratorList * decl)
{
	MemberList *ret = NULL;
	MemberList *t = NULL;
	Member *m;
	DeclaratorList *i;
	for (i = decl; i != NULL; i = i->next) {
		m = malloc(sizeof(Member));

		if (i->nsizes)
			m->type_id =
			    (Type *) create_array_type(type, i->nsizes,
						       i->array_sizes);
		else
			m->type_id = type;

		if (decl->anno)
			m->is_key = 1;
		else
			m->is_key = 0;

		m->name = i->name;
		t = malloc(sizeof(MemberList));
		t->next = ret;
		t->member = m;
		ret = t;
	}
	return ret;
}

ArraySize *create_array_size(int i)
{
	ArraySize *ret = malloc(sizeof(ArraySize));
	ret->nsizes = 1;
	ret->array_sizes = malloc(sizeof(int));
	ret->array_sizes[0] = i;
	return ret;
}

ArraySize *array_size_add(ArraySize * in, int i)
{
	in->array_sizes =
	    realloc(in->array_sizes, sizeof(int) * (in->nsizes + 1));
	in->array_sizes[in->nsizes] = i;
	in->nsizes++;
	return in;
}

DeclaratorList *create_array_declarator(char *name, ArraySize * sizes)
{
	DeclaratorList *ret = malloc(sizeof(DeclaratorList));
	ret->name = name;
	ret->nsizes = sizes->nsizes;
	ret->array_sizes = sizes->array_sizes;
	ret->next = NULL;
	ret->anno = NULL;
	return ret;
}

DeclaratorList *create_simple_declarator(char *name)
{
	DeclaratorList *ret = malloc(sizeof(DeclaratorList));
	ret->name = name;
	ret->nsizes = 0;
	ret->array_sizes = NULL;
	ret->next = NULL;
	ret->anno = NULL;
	return ret;
}

Type AnnotationKeyType;

AnnotationUsage *create_annotation_usage(char *name, void *members)
{
	AnnotationUsage *ret = malloc(sizeof(AnnotationUsage));

	if (strcmp(name, "Key") == 0) {
		ret->type_id = &AnnotationKeyType;
	} else {
		printf("Implement real annotations\n");
		exit(0);
	}

	if (members) {
		printf("Implement members in annotations\n");
		exit(0);
	} else {
		ret->nmembers = 0;
	}

	return ret;
}

DeclaratorList *declarator_list_add_annotation(DeclaratorList * in,
					       AnnotationUsage * anno)
{
	if (anno) {
		if (anno->type_id != &AnnotationKeyType) {
			printf("Implement real annotations\n");
			exit(0);
		}

		in->anno = anno;
	}
	return in;
}

LabelList *create_label_list(uint32_t l)
{
	LabelList *ret = (LabelList *) malloc(sizeof(LabelList));
	ret->is_default = 0;
	ret->label = l;
	ret->next = NULL;
	return ret;
}

LabelList *label_list_prepend(LabelList * l1, LabelList * l2)
{

	l2->next = l1;

	return l2;
}

LabelList *create_label_list_default()
{
	LabelList *ret = (LabelList *) malloc(sizeof(LabelList));
	ret->is_default = 1;
	ret->label = 0;
	ret->next = NULL;
	return ret;
}

uint32_t literal_to_uint32(Literal * l)
{
	uint32_t ret = 0;

	switch (l->kind) {
	case INTEGER_KIND:
	case STRING_KIND:
	case WSTRING_KIND:
	case WCHAR_KIND:
	case FIXED_KIND:
	case FLOAT_KIND:
	case BOOLEAN_KIND:
		break;
	case CHAR_KIND:
		ret = (*(l->string + 1));
		break;
	}

	return ret;
}

int member_contains_keys(Member * m)
{
	if (m->is_key)
		return 1;

	if ((m->type_id->kind == STRUCTURE_TYPE)
	    && (!lookup_type_list(m->type_id->name)))
		return structure_contains_keys((StructureType *) m->type_id);

	return 0;
}

int structure_contains_keys(StructureType * s)
{
	int i;

	for (i = 0; i < s->nmembers; i++)
		if (member_contains_keys(&s->member[i]))
			return 1;

	return 0;
}

/* vim: set foldmethod=marker foldmarker={{{,}}} formatoptions=tcqlron cinoptions=\:0,l1,t0,g0 noexpandtab tabstop=8 shiftwidth=8 textwidth=78: */
