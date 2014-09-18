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
#define	SHOW_INC
//#define USE_STDINT
#ifdef USE_STDINT
#include <stdint.h>
#endif

int main (int argc, char **argv)
{
	double		f;
	unsigned	ns, exact_f, error, ns_max_error, max_error = 0, est_f;

#ifdef USE_STDINT
	printf ("size of est_f = %lu bytes!\n", sizeof (est_f));
#endif
	for (ns = 0; ns < 1000000000; ns++) {
#ifdef USE_STDINT
		est_f = ((uint64_t) ns * (uint64_t) 0x100000000UL) / 1000000000UL;
		if (est_f < ns)
			printf ("Error: result(%u) < ns(%u)???\n", est_f, ns);
#else
		est_f = ns << 2;
		if (ns < 0x10000)
			est_f += ((ns << 16) / 222180);
		else if (ns < 0x400000)
			est_f += ((ns << 10) / 3472);
		else
			est_f += ((ns / 868) << 8);
#endif
		f = (double) ns * 4.2949672965;
		exact_f = (unsigned) f;
		if (exact_f > est_f)
			error = exact_f - est_f;
		else
			error = est_f - exact_f;
		if (error > max_error) {
			max_error = error;
			ns_max_error = ns;
#ifdef SHOW_INC
			printf ("%10u - exact:%u, estimated:%u, error:%u => max-error(%u) = %u\n", ns, exact_f, (unsigned) est_f, error, ns_max_error, max_error);
#endif
		}
#ifndef SHOW_INC
		if ((ns & 0xfff) == 0)
			printf ("%10u - exact:%u, estimated:%u, error:%u => max-error(%u) = %u\n", ns, exact_f, (unsigned) est_f, error, ns_max_error, max_error);
#endif
	}
	printf ("Max. error = %u\n", error);
	return 0;
}
