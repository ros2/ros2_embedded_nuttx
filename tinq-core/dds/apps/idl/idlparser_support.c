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

#include <glib.h>
#include <stdlib.h>
#include "typeobj.h"
#include "typeobj_manip.h"
GHashTable *id_hash;

TypeList *first;
TypeList *last;

void register_type(Type * t)
{
	TypeList *add = malloc(sizeof(TypeList));
	add->type = t;
	add->next = NULL;
	add->idl_dumped = 0;
	add->c_dumped = 0;
	if (last) {
		last->next = add;
		last = add;
	} else
		first = last = add;
	g_hash_table_insert(id_hash, t->name, add);
	//printf("Registering type %s for %p\n", t->name, t);
}

TypeList *get_type_list()
{
	return first;
}

void parser_data_init()
{
	id_hash = g_hash_table_new(g_str_hash, g_str_equal);
}

void register_alias_types_from_declarator_list(DeclaratorList * l)
{
	DeclaratorList *i;

	for (i = l; i != NULL; i = i->next) {
		Type *base = l->type;
		Type *final;

		if (l->nsizes)
			final =
			    (Type *) create_array_type(base, l->nsizes,
						       l->array_sizes);
		else
			final = base;

		AliasType *alias = create_alias_type(l->name, final);
		register_type((Type *) alias);
	}
}

Type *lookup_type(char *name)
{
	TypeList *tl = (TypeList *) g_hash_table_lookup(id_hash, name);
	if (tl)
		return tl->type;
	else
		return NULL;
}

TypeList *lookup_type_list(char *name)
{
	return ((TypeList *) g_hash_table_lookup(id_hash, name));
}
