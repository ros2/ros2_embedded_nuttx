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
#include <stdlib.h>
#include "nmatch.h"

static void usage (void)
{
	printf ("Usage: nmatch [<options>] <pattern> <name>\r\n");
	printf ("Where <options> = '-'<option>{<option>}\r\n");
	printf ("         -e  No backslash to quote special characters.\r\n");
	printf ("         -c  Compare without regard for case.\r\n");
	printf ("         -s  Use SQL regular expressions ('%%' and '_' i.o. '*' and '?').\r\n");
	exit (1);
}

int main (int argc, const char *argv[])
{
	const char	*name, *pattern, *s;
	unsigned	i;
	int		flags = 0, r;

	if (argc == 4)
		if (argv [1][0] == '-') {
			pattern = argv [2];
			name = argv [3];
			s = argv [1];
			while (*++s != '\0')
				if (*s == 'e')
					flags |= NM_NOESCAPE;
				else if (*s == 'c')
					flags |= NM_CASEFOLD;
				else if (*s == 's')
					flags |= NM_SQL;
				else
					usage ();
		}
		else
			usage ();
	else if (argc == 3) {
		pattern = argv [1];
		name = argv [2];
	}
	else
		usage ();

	printf ("NoEscape:%u, CaseFold:%u, SQL:%u, Pattern:'%s', Name:'%s'  ->  ",
			(flags & NM_NOESCAPE) != 0,
			(flags & NM_CASEFOLD) != 0,
			(flags & NM_SQL) != 0,
			pattern,
			name);
	r = nmatch (pattern, name, flags);
	if (r == NM_MATCH)
		printf ("Match!\r\n");
	else
		printf ("No match\n");
}

