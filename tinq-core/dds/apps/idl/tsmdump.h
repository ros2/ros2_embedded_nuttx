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

#ifndef TSMDUMP_H
#define TSMDUMP_H
#include "typeobj.h"
void tsm_dump_struct_type(StructureType * s);
void tsm_dump_member(StructureType *, Member *);
void tsm_dump_set_output(FILE * fp);
#endif
