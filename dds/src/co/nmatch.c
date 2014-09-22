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

/* nmatch.c -- Name matching functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>

#include "nmatch.h"

/* nmatch -- Matches the pattern against the name, with the given flags, and
	     returns 0 if the match is successful. This function is loosely
	     based on fnmatch.3, except that it can not be used for filename
	     analysis. */

int nmatch (const char *pattern, const char *name, int flags)
{
	int		esc, r, fflags = 0;
	char		*fnpattern, ch;
	char		*sp;

	if (!name || !pattern)
		return (NM_NOMATCH);

	if ((flags & NM_NOESCAPE) != 0)
		fflags |= FNM_NOESCAPE;
	if ((flags & NM_CASEFOLD) != 0)
		fflags |= (1 << 4)/*FNM_CASEFOLD*/;
	if ((flags & NM_SQL) != 0) {
		fnpattern = strdup (pattern);
		esc = 0;
		for (sp = fnpattern; *sp; sp++) {
			ch = *sp;
			if (ch == '\\' && (flags & NM_NOESCAPE) == 0 && !esc) {
				esc = 1;
				continue;
			}
			if (!esc) {
				if (ch == '%')
					*sp = '*';
				else if (ch == '_')
					*sp = '?';
			}
			else
				esc = 0;
		}
		/*printf ("  new pattern => %s\n", fnpattern);*/
	}
#if defined (NUTTX_RTOS)
#include "nuttx/regex.h"
	r = match(pattern, name);
#else
	r = fnmatch (((flags & NM_SQL) != 0) ? fnpattern : pattern, name, fflags);
#endif
	if (!r)
		return (NM_MATCH);
	else
		return (NM_NOMATCH);
}

