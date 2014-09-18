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

#ifndef IDLDUMP_H
#define IDLDUMP_H
#include "typeobj.h"
void idl_dump_string_type(StringType * t);
void idl_dump_array_type_pre(ArrayType * t);
void idl_dump_array_type_post(ArrayType * t);
void idl_dump_type_pre(Type * t);
void idl_dump_type_post(Type * t);
void idl_dump_struct_type(StructureType * s);
void idl_dump_member(Member * m);
void idl_dump_toplevel(Type * type);
#endif
