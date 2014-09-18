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

/* MDDRIVER.C - test driver for MD2, MD4 and MD5 */

/* Copyright (C) 1990-2, RSA Data Security, Inc.
   Created 1990. All rights reserved.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software. */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "md5.h"

/* Length of test block, number of test blocks.
 */
#define TEST_BLOCK_LEN 1000
#define TEST_BLOCK_COUNT 1000000

/* Prints a message digest in hexadecimal. */

static void MDPrint (const unsigned char digest [16])
{
	unsigned	i;

	for (i = 0; i < 16; i++)
		printf ("%02x", digest [i]);
}

/* Digests a string and prints the result. */

static void MDString (const char *string)
{
	MD5_CONTEXT	context;
	unsigned char	digest [16];
	unsigned	len = strlen (string);

	md5_init (&context);
	md5_update (&context, string, len);
	md5_final (digest, &context);

	printf ("MD5 (\"%s\") = ", string);
	MDPrint (digest);
	printf ("\n");
}

/* Measures the time to digest TEST_BLOCK_COUNT TEST_BLOCK_LEN-byte blocks. */

static void MDTimeTrial (void)
{
	MD5_CONTEXT	context;
	time_t		endTime, startTime;
	unsigned char	block [TEST_BLOCK_LEN], digest [16];
	unsigned	i;

	printf ("MD5 time trial. Digesting %d %d-byte blocks ...", 
					TEST_BLOCK_COUNT, TEST_BLOCK_LEN);

	/* Initialize block */
	for (i = 0; i < TEST_BLOCK_LEN; i++)
		block [i] = (unsigned char)(i & 0xff);

	/* Start timer */
	time (&startTime);

	/* Digest blocks */
	md5_init (&context);
	for (i = 0; i < TEST_BLOCK_COUNT; i++)
		md5_update (&context, block, TEST_BLOCK_LEN);
	md5_final (digest, &context);

	/* Stop timer */
	time (&endTime);

	printf (" done\n");
	printf ("Digest = ");
	MDPrint (digest);
	printf ("\nTime = %ld seconds\n", (long) (endTime - startTime));
	printf ("Speed = %ld bytes/second\n",
		(long) TEST_BLOCK_LEN * (long) TEST_BLOCK_COUNT / (endTime - startTime));
}

/* Digests a reference suite of strings and prints the results. */

static void MDTestSuite (void)
{
	printf ("MD5 test suite:\n");

	MDString ("");
	MDString ("a");
	MDString ("abc");
	MDString ("message digest");
	MDString ("abcdefghijklmnopqrstuvwxyz");
	MDString ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	MDString ("1234567890123456789012345678901234567890"
		  "1234567890123456789012345678901234567890");
}

/* Digests a file and prints the result. */

static void MDFile (const char *filename)
{
	FILE		*file;
	MD5_CONTEXT	context;
	int		len;
	unsigned char	buffer [1024], digest [16];

	if ((file = fopen (filename, "rb")) == NULL)
		printf ("%s can't be opened\n", filename);

	else {
		md5_init (&context);
		while (len = fread (buffer, 1, 1024, file))
			md5_update (&context, buffer, len);
		md5_final (digest, &context);

		fclose (file);

		printf ("MD5 (%s) = ", filename);
		MDPrint (digest);
		printf ("\n");
 	}
}

/* Digests the standard input and prints the result. */

static void MDFilter (void)
{
	MD5_CONTEXT	context;
	int		len;
	unsigned char	buffer [16], digest [16];

	md5_init (&context);
	while (len = fread (buffer, 1, 16, stdin))
		md5_update (&context, buffer, len);
	md5_final (digest, &context);

	MDPrint (digest);
	printf ("\n");
}

/* Main driver.

	Arguments (may be any combination):
	  -sstring - digests string
	  -t       - runs time trial
	  -x       - runs test script
	  filename - digests file
	  (none)   - digests standard input
 */
int main (int argc, char *argv [])
{
	int i;

	if (argc > 1)
		for (i = 1; i < argc; i++)
			if (argv [i][0] == '-' && argv [i][1] == 's')
				MDString (argv[i] + 2);
			else if (strcmp (argv [i], "-t") == 0)
				MDTimeTrial ();
			else if (strcmp (argv [i], "-x") == 0)
				MDTestSuite ();
			else
				MDFile (argv[i]);
	else
		MDFilter ();

	return (0);
}

