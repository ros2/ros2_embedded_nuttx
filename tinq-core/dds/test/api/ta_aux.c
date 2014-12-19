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

#include "test.h"
#include "ta_aux.h"

/* -- DDS_execution_environment -- */

static void try_ee (DDS_ExecEnv_t e, DDS_ReturnCode_t e_ret)
{
	DDS_ReturnCode_t	r;

	r = DDS_execution_environment (e);
	fail_unless (r == e_ret);
}

static void test_ee (void)
{
	v_printf ("\r\n - Set various Execution environment types.\r\n");
	try_ee (DDS_EE_C, DDS_RETCODE_OK);
	try_ee (DDS_EE_CPP, DDS_RETCODE_OK);
	try_ee (DDS_EE_JAVA, DDS_RETCODE_OK);
	try_ee (DDS_EE_CDD, DDS_RETCODE_BAD_PARAMETER);
	try_ee (-1, DDS_RETCODE_UNSUPPORTED);
	try_ee (22, DDS_RETCODE_UNSUPPORTED);
}

/* -- DDS_pool_constraints -- */

static int constraints_eq (DDS_PoolConstraints *c, DDS_PoolConstraints *e)
{
	unsigned	*cp, *ep;
	size_t		i;

	cp = (unsigned *) c;
	ep = (unsigned *) e;
	for (i = 0; i < sizeof (DDS_PoolConstraints) / sizeof (unsigned); i++)
		if (*cp++ != *ep++) {
			dbg_printf ("constraints_eq: differs at offset: %ld\r\n", (long) i);
			return (0);
		}
	return (1);
}

#if 0
static int constraints_dump (DDS_PoolConstraints *c)
{
	unsigned	*cp;
	size_t		i;

	cp = (unsigned *) c;
	printf ("\r\n{");
	for (i = 0; i < sizeof (DDS_PoolConstraints) / sizeof (unsigned); i++) {
		if (i)
			printf (", ");
		if ((i & 0x7) == 0)
			printf ("\r\n\t");
		printf ("%u", *cp++);
	}
	printf ("\r\n};\r\n");
	return (1);
}
#endif

static void try_constraints_set (DDS_PoolConstraints *c, DDS_ReturnCode_t e_ret)
{
	DDS_ReturnCode_t	r;

	r = DDS_set_pool_constraints (c);
	fail_unless (r == e_ret);
}

static void try_constraints (DDS_PoolConstraints *c,
		      unsigned            max,
		      unsigned            grow,
		      DDS_PoolConstraints *e_c,
		      DDS_ReturnCode_t    e_ret)
{
	DDS_ReturnCode_t	r;

	r = DDS_get_default_pool_constraints (c, max, grow);
	fail_unless (r == e_ret);
	if (!c)
		return;

	/* constraints_dump (c); */
	fail_unless (constraints_eq (c, e_c));
	try_constraints_set (c, DDS_RETCODE_OK);
}

static void test_constr (void)
{
	DDS_ReturnCode_t	r;
	DDS_PoolConstraints	c;
	DDS_PoolConstraints	e1 = {
		1, 2, 2, 2, 2, 12, 12, 8, 8,
		16, 16, 1, 1, 10, 10,
		10, 10, 10, 10,
		8, 8, 24, 24, 24, 24,
		32768, 32768, 4, 64, 64, 32, 32, 
		32, 32, 2, 2, 2, 2, 2, 2, 
		2, 2, 2, 2, 64, 64, 4096, 4096, 
		32, 32, 16, 16, 8, 8, 256, 256, 
		32, 32, 1024, 16, 8, 2, 2, 2, 2, 
		2, 2, 2, 2, 2, 2, 8, 8, 
		2, 2, 4, 4, 8, 8, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4,
		0
	};
	DDS_PoolConstraints	e2 = { 
		1, 2, 3, 2, 3, 12, 18, 8, 12,
		16, 24, 1, 1, 10, 15,
		10, 15, 10, 15,
		8, 12, 24, 36, 24, 36,
		32768, 49152, 4, 64, 96, 32, 48, 
		32, 48, 2, 3, 2, 3, 2, 3, 
		2, 3, 2, 3, 64, 96, 4096, 6144, 
		32, 48, 16, 24, 8, 12, 256, 384, 
		32, 48, 1024, 16, 8, 2, 3, 2, 3, 
		2, 3, 2, 3, 2, 3, 8, 12, 
		2, 3, 4, 6, 8, 12, 4, 6,
		4, 6, 4, 6, 4, 6, 4, 6,
		200
	};
	DDS_PoolConstraints	e3 = {
		1, 2, 5, 2, 5, 12, 30, 8, 20,
		16, 40, 1, 2, 10, 25,
		10, 25, 10, 25,
		8, 20, 24, 60, 24, 60,
		32768, 81920, 4, 64, 160, 32, 80, 
		32, 80, 2, 5, 2, 5, 2, 5, 
		2, 5, 2, 5, 64, 160, 4096, 10240, 
		32, 80, 16, 40, 8, 20, 256, 640, 
		32, 80, 1024, 16, 8, 2, 5, 2, 5, 
		2, 5, 2, 5, 2, 5, 8, 20, 
		2, 5, 4, 10, 8, 20, 4, 10,
		4, 10, 4, 10, 4, 10, 4, 10,
		0
	};
	DDS_PoolConstraints	e4 = {
		1, 2, ~0, 2, ~0, 12, ~0, 8, ~0,
		16, ~0, 1, ~0, 10, ~0,
		10, ~0, 10, ~0,
		8, ~0, 24, ~0, 24, ~0,
		32768, ~0, 4, 64, ~0, 32, ~0, 
		32, ~0, 2, ~0, 2, ~0, 2, ~0, 
		2, ~0, 2, ~0, 64, ~0, 4096, ~0, 
		32, ~0, 16, ~0, 8, ~0, 256, ~0, 
		32, ~0, 1024, 16, 8, 2, ~0, 2, ~0, 
		2, ~0, 2, ~0, 2, ~0, 8, ~0, 
		2, ~0, 4, ~0, 8, ~0, 4, ~0,
		4, ~0, 4, ~0, 4, ~0, 4, ~0,
		20
	};

	v_printf (" - Test various constraint settings.\r\n");	
	try_constraints (&c, 0, 0, &e1, DDS_RETCODE_OK);
	try_constraints (&c, 50, 200, &e2, DDS_RETCODE_OK);
	try_constraints (&c, 150, 0, &e3, DDS_RETCODE_OK);
	try_constraints (&c, ~0, 20, &e4, DDS_RETCODE_OK);
	r = DDS_get_default_pool_constraints (&c, 0, 0);
	fail_unless (r == DDS_RETCODE_OK);
	try_constraints_set (&c, DDS_RETCODE_OK);
}

static void test_name (void)
{
	static char	*args [] = { "test", "t2" };
	int		i;

	v_printf (" - Test program name.\r\n");
	i = 2;
	DDS_program_name (&i, args);
	i = 1;
	DDS_program_name (&i, &args [1]);
}

static void test_sched (void)
{
	v_printf (" - Test schedule/wait/continue.\r\n");	
	DDS_schedule (0);
	DDS_schedule (10);
	DDS_wait (0);
	DDS_wait (50);
	DDS_continue ();
}

static void test_err (void)
{
	int i;
	const char *dds_errors [] = {
		"no error",
		"Generic unspecified error",
		"Unsupported operation",
		"Invalid parameter value",
		"Precondition not met",
		"Not enough memory",
		"Not enabled yet",
		"Immutable policy",
		"Inconsistent policy",
		"Object not found",
		"Timeout occurred",
		"No data available",
		"Illegal operation"
	};

	v_printf (" - Test error strings.\r\n");	
	for (i = -1; i <= DDS_RETCODE_ILLEGAL_OPERATION + 2; i++)
		if (i >= 0 && i <= DDS_RETCODE_ILLEGAL_OPERATION)
			fail_unless (!strcmp (DDS_error (i), dds_errors [i]));
		else
			fail_unless (!strcmp (DDS_error (i), "unknown"));
}

void test_aux (void)
{
	dbg_printf ("Auxiliary functions ");
	if (trace)
		fflush (stdout);
	test_ee ();
	test_constr ();
	test_name ();
	test_sched ();
	test_err ();
	dbg_printf (" - success!\r\n");
}

