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

#include "typeobj.h"

/* Real type constructors */
AliasType *create_alias_type(char *, Type *);
ArrayType *create_array_type(Type *, int, int *);
EnumerationType *create_enumeration_type(char *, EnumConstList *);
MapType *create_map_type(Type *, Type *, int);
SequenceType *create_sequence_type(Type *, int);
StringType *create_string_type(int);
StructureType *create_structure_type(char *, Type *, MemberList *);
UnionType *create_union_type(char *, Type *, UnionMemberList *);

Type *create_invalid_type();
Type *get_primitive_type(TypeKind kind);

/* Utility constructors */
DeclaratorList *create_array_declarator(char *name, ArraySize * sizes);
DeclaratorList *create_simple_declarator(char *name);
EnumConst *create_enum_const(char *id);
EnumConstList *create_enum_const_list(EnumConst * c);
UnionMember *create_union_member(Type * t, DeclaratorList * decl);
UnionMemberList *create_union_member_list(UnionMember *);
MemberList *create_member_list(Member * c);
AnnotationUsage *create_annotation_usage(char *name, void *members);
LabelList *create_label_list(uint32_t);
LabelList *create_label_list_default();
ArraySize *create_array_size(int i);

DeclaratorList *declarator_list_prepend(DeclaratorList * l1,
					DeclaratorList * l2);
DeclaratorList *declarator_list_set_type(DeclaratorList * l, Type * t);
DeclaratorList *declarator_list_add_annotation(DeclaratorList * in,
					       AnnotationUsage * anno);
EnumConstList *enum_const_list_prepend(EnumConstList * list, EnumConst * c);
MemberList *member_list_from_declarator_list(Type * type,
					     DeclaratorList * decl);
MemberList *member_list_join(MemberList * l1, MemberList * l2);
StructureType *struct_set_members(StructureType * s, MemberList * members);
UnionMember *union_member_add_labels(UnionMember *, LabelList *);
UnionMemberList *union_member_list_prepend(UnionMemberList *, UnionMember *);
LabelList *label_list_prepend(LabelList *, LabelList *);
ArraySize *array_size_add(ArraySize * in, int i);
uint32_t literal_to_uint32(Literal * l);

int member_contains_keys(Member * m);
int structure_contains_keys(StructureType * s);
