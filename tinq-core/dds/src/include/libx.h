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

/* libx.h -- Extra functions, not a part of DDS as such, but often used by
	     applications. */

#ifndef __libx_
#define	__libx_

#ifndef ARG_NOT_USED
#define	ARG_NOT_USED(p)	(void) p;
#endif

int astrcmp (const char *s1, const char *s2);

/* Compare two strings for equality but do it case independent. */

int astrncmp (const char *s1, const char *s2, size_t n);

/* Compare two strings for equality assuming a fixed length n but do it in a
   case independent way. */

#ifndef __GNUC__
#define	__attribute__(x)	/*NOTHING*/
#endif
#ifdef _WIN32
__declspec(noreturn)
#endif
void fatal (const char *fmt, ...)
	__attribute__((noreturn, format(printf, 1, 2)));

/* Non-recoverable error message. */

void skip_blanks (const char **args);

/* Set the pointer to the next non-whitespace character. */

void skip_string (const char **cmd, char *buf);

/* Collect characters from *cmd in *buf, incrementing the *cmd pointer. */

int slist_match (const char *list, const char *name, char sep);

/* Check if a string is present in a list of strings, separated by the given
   separator character. */

#endif /* !__libx_ */

