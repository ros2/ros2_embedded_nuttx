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
#include "ipfilter.h"

int main (int argc, const char *argv [])
{
	int		i, j;
	unsigned	d;
	IpFilter_t	f;
	unsigned	ip [4];
	unsigned char	ipa [4];

	for (i = 1; i < argc; i++) {
		f = ip_filter_new (argv [i], 3, 1);
		if (!f) {
			printf ("Invalid filter spec: %s\n", argv [i]);
			break;
		}
		ip_filter_dump (f);
		i++;
		if (i >= argc - 1) {
			printf ("Missing <domain> <address>!\n");
			return (1);
		}
		d = atoi (argv [i]);
		i++;
		sscanf (argv [i], "%d.%d.%d.%d", &ip [0], &ip [1], &ip [2], &ip [3]);
		for (j = 0; j < 4; j++) {
			if (ip [j] > 255)
				printf ("Incorrect IP address!\n");

			ipa [j] = ip [j];
		}
		i++;
		if (ip_match (f, d, ipa))
			printf (" -> match!\n");
		else
			printf (" -> no match\n");
		ip_filter_free (f);
	}
	return (0);
}


