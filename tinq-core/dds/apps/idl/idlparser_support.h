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


void parser_data_init();
void register_alias_types_from_declarator_list(DeclaratorList * l);
Type *lookup_type(char *name);
TypeList *lookup_type_list(char *name);
TypeList *get_type_list();
void register_type(Type * t);
