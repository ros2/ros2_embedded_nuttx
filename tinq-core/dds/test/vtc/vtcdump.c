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

/* vtcdump.c -- Dumps a vendor-specific typecode data file and dumps it in readable type format. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "pool.h"
#include "xtypes.h"
#include "xdata.h"
#include "vtc.h"

void rcl_access (void *p)
{
}

void rcl_done (void *p)
{
}

void vtcdump (const char *filename)
{
	int		fd;
	ssize_t		n;
	size_t		len;
	int		swap, res;
	unsigned	ofs;
	unsigned char	hdr [4];
	unsigned char	*vtc;
	union {
		unsigned char	b [2];
		uint16_t	s;
	}		u;
	DDS_TypeSupport	ts;
	TypeLib		*def_lib;

	fd = open (filename, O_RDONLY);
	if (fd < 0) {
		perror ("open");
		exit (1);
	}
	n = read (fd, hdr, 4);
	if (n < 0) {
		perror ("read");
		exit (1);
	}
	if (n < 4) {
		fprintf (stderr, "File too small!\n");
		exit (1);
	}
	u.s = 0x1234;
	if (hdr [0] == 0x80 && hdr [1] == 0x04) {
		swap = (u.b [0] != 0x12);
		len = hdr [2] << 8 | hdr [3];
	}
	else if (hdr [1] == 0x80 && hdr [0] == 0x04) {
		swap = (u.b [0] == 0x12);
		len = hdr [3] << 8 | hdr [2];
	}
	else {
		fprintf (stderr, "Invalid file header -- should start with proper PID field!\n");
		exit (1);
	}
	printf ("Validating typecode (%lu bytes, %sswapped) ", (unsigned long) len, (swap == 0) ? "not " : "");
	fflush (stdout);
	vtc = malloc (len);
	n = read (fd, vtc, len);
	if (n != len) {
		if (n < 0)
			perror ("read");
		exit (1);
	}
	close (fd);
	ofs = 0;
	res = vtc_validate (vtc, len, &ofs, swap, 0);
	if (!res) {
		printf ("-- invalid: aborting.\n");
		return;
	}
	printf ("-- valid.\n");
	printf ("Converting back to real type ");
	fflush (stdout);
	def_lib = xt_lib_create (NULL);
	ts = vtc_type (def_lib, vtc);
	if (!ts) {
		printf ("-- failed : aborting.\n");
		return;
	}
	printf ("-- done.\n");
	xt_dump_type (0, (Type *) ts->ts_cdr, 0);
	xt_type_delete (ts->ts_cdr);
	xfree (ts);
	xt_lib_delete (def_lib);
}

int main (int argc, const char *argv [])
{
	unsigned	i;

        const POOL_LIMITS str = {
                2, ~0, 0
        }, refs = {
                2, ~0, 0
        }, dtypes = {
                2, ~0, 0
        }, ddata = {
                2, ~0, 0
        };

        str_pool_init (&str, &refs, 20000, 1);
	xtypes_init ();
        xd_pool_init (&dtypes, &ddata);

	if (argc < 2) {
		printf ("%s: filename(s) expected.\n", argv [0]);
		return (1);
	}
	for (i = 1; i < argc; i++)
		vtcdump (argv [i]);

	return (0);
}

