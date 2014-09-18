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

/* parse.h -- SQL subset command parser as used for the DDS SQL subset. */

#ifndef __parse_h_
#define __parse_h_

#include "bytecode.h"

void sql_parse_init (void);

/* Initialize the parser. */

int sql_parse_topic (const char *s);

/* Parse a multitopic specification SQL statement. */

int sql_parse_query (const TypeSupport_t *ts,
		     const char          *s,
		     BCProgram           *mprog,
		     BCProgram           *oprog);

/* Parse a query specification SQL statement.
   Two bytecode programs will be returned.  The first can be used to check if a
   match occurs (returns True of False, depending on the condition result).
   The second bytecode can be used to compare two matching samples in order to
   get all the appropriate samples in the requested order. */

int sql_parse_filter (const TypeSupport_t *ts, const char *s, BCProgram *prog);

/* Parse a filter specification SQL statement. */

#endif /* !__parse_h_ */

