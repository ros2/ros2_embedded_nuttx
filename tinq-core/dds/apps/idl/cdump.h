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

#ifndef CDUMP_H
#define CDUMP_H
#include "typeobj.h"
void c_dump_string_type(StringType * t);
void c_dump_array_type_pre(ArrayType *, FILE *, char **);
void c_dump_array_type_post(ArrayType * t, int expand, FILE *, char **out);
void c_dump_type_pre(Type * t, char sep, int expand, FILE *, char **ret);
void c_dump_type_post(Type * t, int expand, FILE *, char **ret);
void c_dump_struct_type(StructureType * s, int expand, FILE *, char **out);
void c_dump_member(Member *, FILE *, char **);
void c_dump_set_output(FILE * fp);
void c_dump_toplevel(Type * type);
#endif
