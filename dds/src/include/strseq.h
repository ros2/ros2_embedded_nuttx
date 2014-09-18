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

/* strseq.h -- Defines the Strings sequence type as used internally in DDS. */

#ifndef __strseq_h_
#define	__strseq_h_

#include "str.h"
#include "dds/dds_seq.h"

DDS_SEQUENCE (String_t *, Strings_t);

void strings_delete (Strings_t *s);

/* Delete a sequence of strings. */

void strings_reset (Strings_t *s);

/* Reset a sequence of strings so that it contains no elements. */

int strings_append_cstr (Strings_t *s, const char *cp);

/* Add a constant string to a string sequence. */

#endif /* !__strseq_h_ */

