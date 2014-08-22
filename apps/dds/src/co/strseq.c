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

#include "dds/dds_error.h"
#include "str.h"
#include "strseq.h"

void strings_reset (Strings_t *s)
{
	unsigned	i;
	String_t	**sp;

	if (s && s->_length) {
		for (i = 0, sp = s->_buffer; i < s->_length; i++, sp++)
			if (*sp)
				str_unref (*sp);
		s->_length = 0;
	}
}

/* strings_delete -- Delete a string sequence. */

void strings_delete (Strings_t *s)
{
	if (!s)
		return;

	strings_reset (s);
	if (s->_buffer)
		xfree (s->_buffer);
	xfree (s);
}

/* strings_append_cstr -- Add a constant string to a string sequence. */

int strings_append_cstr (Strings_t *s, const char *cp)
{
	String_t	*p;

	if (!cp) {
		if (dds_seq_append (s, (String_t *) &cp))
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	else {
		p = str_new_cstr (cp);
		if (!p)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		if (dds_seq_append (s, &p)) {
			str_unref (p);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
	}
	return (DDS_RETCODE_OK);
}




