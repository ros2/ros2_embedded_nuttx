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
#include <string.h>
#include "string_manip.h"

char *str_append_free_2(char *a, char *b)
{
	char *ret = malloc(strlen(a) + strlen(b) + 1);
	strcpy(ret, a);
	strcpy(ret + strlen(a), b);
	free(b);
	return a;
}

char *str_append3_free_13(char *a, char *b, char *c)
{
	char *ret = realloc(a, strlen(a) + strlen(b) + strlen(c) + 1);
	strcpy(ret + strlen(ret), b);
	strcpy(ret + strlen(ret), c);
	free(c);
	return ret;
}

char *str_append3_free_1(char *a, char *b, char *c)
{
	char *ret = realloc(a, strlen(a) + strlen(b) + strlen(c) + 1);
	strcpy(ret + strlen(ret), b);
	strcpy(ret + strlen(ret), c);
	return ret;
}

char *str_append3(char *a, char *b, char *c)
{
	char *ret = malloc(strlen(a) + strlen(b) + strlen(c) + 1);
	strcpy(ret, a);
	strcpy(ret + strlen(ret), b);
	strcpy(ret + strlen(ret), c);
	return ret;
}
