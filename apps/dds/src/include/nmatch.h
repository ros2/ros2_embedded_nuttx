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

/* nmatch.h -- Name matching function, loosely based on the posix fnmatch()
               function specification. */

#ifndef __nmatch_h_
#define __nmatch_h_

/* Flags that can be used in nmatch(): */
#define	NM_NOESCAPE	1	/* Backslashes don't quote special characters. */
#define NM_CASEFOLD	2	/* Compare without regard to case. */
#define NM_SQL		4	/* Use SQL wildcard characters. */

/* Return values: */
#define NM_MATCH	0	/* The input string matches the pattern. */
#define	NM_NOMATCH	1	/* The input string doesn't match the pattern. */

int nmatch (const char *pattern, const char *name, int flags);

/* Matches the pattern against the name, with the given flags, and returns 0 if
   the match is successful. This function is loosely based on fnmatch.3, except
   that it can not be used for filename analysis. */

#endif /* !__nmatch_h_ */
