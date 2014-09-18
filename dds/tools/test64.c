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

# include <stdio.h>
# include <stddef.h>

char buf [] = "Hello";

typedef int (*FCT)(void);

typedef struct test_st {
	char		c;
	unsigned short	us;
	unsigned	u;
	unsigned long	ul;
	unsigned long long ull;
	short		s;
	int		i;
	long		l;
	long long	ll;
	float		f;
	double		d;
	long double	ld;
	char		*p;
	FCT		fct;
} Test_t;

int main (void)
{
 	Test_t	t;
	Test_t	*tp = &t;

	printf ("char               = %lu byte\r\n", sizeof (t.c));
	printf ("unsigned short     = %lu bytes\r\n", sizeof (t.us));
	printf ("unsigned           = %lu bytes\r\n", sizeof (t.u));
	printf ("unsigned long      = %lu bytes\r\n", sizeof (t.ul));
	printf ("unsigned long long = %lu bytes\r\n", sizeof (t.ull));
	printf ("short              = %lu bytes\r\n", sizeof (t.s));
	printf ("int                = %lu bytes\r\n", sizeof (t.i));
	printf ("long               = %lu bytes\r\n", sizeof (t.l));
	printf ("long long          = %lu bytes\r\n", sizeof (t.ll));
	printf ("float              = %lu bytes\r\n", sizeof (t.f));
	printf ("double             = %lu bytes\r\n", sizeof (t.d));
	printf ("long double        = %lu bytes\r\n", sizeof (t.ld));
	printf ("char *             = %lu bytes\r\n", sizeof (t.p));
	printf ("int (*)(void)      = %lu bytes\r\n", sizeof (t.fct));
	printf ("Test_t *           = %lu bytes\r\n", sizeof (tp));
	printf ("Test_t             = %lu bytes\r\n", sizeof (t));
	printf ("\r\nFor "
"struct {\r\n"
"	char		c;\r\n"
"	unsigned short	us;\r\n"
"	unsigned	u;\r\n"
"	unsigned long	ul;\r\n"
"	unsigned long long ull;\r\n"
"	short		s;\r\n"
"	int		i;\r\n"
"	long		l;\r\n"
"	long long	ll;\r\n"
"	float		f;\r\n"
"	double		d;\r\n"
"	long double	ld;\r\n"
"	char		*p;\r\n"
"	int		(*fct)(void);\r\n"
"}\r\n");
	printf ("t.c offset          = %lu bytes\r\n", offsetof (Test_t, c));
	printf ("t.us offset         = %lu bytes\r\n", offsetof (Test_t, us));
	printf ("t.u offset          = %lu bytes\r\n", offsetof (Test_t, u));
	printf ("t.ul offset         = %lu bytes\r\n", offsetof (Test_t, ul));
	printf ("t.ull offset        = %lu bytes\r\n", offsetof (Test_t, ull));
	printf ("t.s offset          = %lu bytes\r\n", offsetof (Test_t, s));
	printf ("t.i offset          = %lu bytes\r\n", offsetof (Test_t, i));
	printf ("t.l offset          = %lu bytes\r\n", offsetof (Test_t, l));
	printf ("t.ll offset         = %lu bytes\r\n", offsetof (Test_t, ll));
	printf ("t.f offset          = %lu bytes\r\n", offsetof (Test_t, f));
	printf ("t.d offset          = %lu bytes\r\n", offsetof (Test_t, d));
	printf ("t.ld offset         = %lu bytes\r\n", offsetof (Test_t, ld));
	printf ("t.p offset          = %lu bytes\r\n", offsetof (Test_t, p));
	printf ("t.fct offset        = %lu bytes\r\n", offsetof (Test_t, fct));
}

