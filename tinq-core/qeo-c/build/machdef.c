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

int 
main(int argc, char ** argv)
{
	int endian=0x01000000;
	printf("E_SIZEOF_LONG = %zu\n", sizeof(long));
	printf("E_ENDIAN = %s\n", (((char *) &endian)[0]==1)?"BIG":"LITTLE");
}
