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
#include <stddef.h>

#define alignment_init(n, type) typedef struct { char pad; type name; } _alignment_struct_##n
#define alignment(type) offsetof(_alignment_struct_##type, name)
#define print_align(type) printf(#type "=%d\n",alignment(type));

alignment_init(char, char);
alignment_init(short, short);
alignment_init(int, int);
alignment_init(long_long, long long);
alignment_init(float, float);
alignment_init(double, double);
alignment_init(long_double, long double);

int main(int argc, char ** argv)
{
        print_align(char);
        print_align(short);
        print_align(int);
	print_align(long_long);
        print_align(float);
        print_align(double);
        print_align(long_double);
}

