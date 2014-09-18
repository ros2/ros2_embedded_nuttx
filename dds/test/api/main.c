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

#include <ctype.h>
#include "test.h"
#include "ta_aux.h"
#include "ta_seq.h"
#include "ta_type.h"
#include "ta_pfact.h"
#include "ta_part.h"
#include "ta_topic.h"
#include "ta_pub.h"
#include "ta_sub.h"
#include "ta_writer.h"
#include "ta_reader.h"
#include "ta_data.h"

const char	*progname;
int		trace = 0;
int		verbose = 0;
int		no_rtps = 0;
int		shell = 0;
unsigned	delay_ms = 1000;
unsigned	ntest = 1;

typedef struct test_st {
	const char	*name;
	void		(*fct) (void);
	const char	*info;
} Test_t;

const Test_t tests [] = {
	{ "extra", test_aux,   "Auxiliary, e.g. uDDS-specific extensions." },
	{ "seq", test_sequences, "Sequence-related tests." },
	{ "type", test_type, "Type registration tests." },
	{ "factory", test_participant_factory, "Participant Factory tests." },
	{ "participant", test_participant, "Participant tests." },
	{ "topic", test_topic, "Topic tests." },
	{ "filter", test_contentfilteredtopic, "Content-filtered topic tests." },
	{ "multitopic", test_multitopic, "Multitopic tests." },
	{ "pub", test_publisher, "Publisher tests." },
	{ "sub", test_subscriber, "Subscriber tests." },
	{ "writer", test_writer, "DataWriter tests." },
	{ "reader", test_reader, "DataReader tests." },
	{ "data", test_data, "Complex Data exchange tests." }
};

void list_tests (void)
{
	const Test_t	*tp;
	unsigned	i;

	for (i = 0, tp = tests; i < sizeof (tests) / sizeof (Test_t); tp++, i++)
		printf ("\t%-12s\t%s\r\n", tp->name, tp->info);
}

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "%s -- test program for the DCPS API.\r\n", progname);
	fprintf (stderr, "Usage: %s [switches] {<testname>}\r\n", progname);
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -l         List all test names.\r\n");
	fprintf (stderr, "   -d <msec>  Max. delay to wait for responses (10..10000).\r\n");
	fprintf (stderr, "   -n <num>   Number of times tests need to be done.\r\n");
	fprintf (stderr, "   -r         No lower layer RTPS functionality.\r\n");
#ifdef DDS_DEBUG
	fprintf (stderr, "   -s         Debug shell mode.\r\n");
#endif
	fprintf (stderr, "   -v         Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv        Very verbose logging.\r\n");
	fprintf (stderr, "   -h         Display help info.\r\n");
	exit (1);
}

/* get_num -- Get a number from the command line arguments. */

int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max)
{
	const char	*cp = *cpp;

	while (isspace ((unsigned char) *cp))
		cp++;
	if (*cp < '0' || *cp > '9')
		return (0);

	*num = (unsigned) atoi (cp);
	if (*num < min || *num > max)
		return (0);

	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i, help;
	const char	*cp;

	progname = argv [0];
	help = 0;
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'l':
					list_tests ();
					exit (0);
					break;
				case 'd':
					INC_ARG()
					if (!get_num (&cp, &delay_ms, 10, 10000000))
						usage ();
					break;
				case 'r':
					no_rtps = 1;
					break;
#ifdef DDS_DEBUG
				case 's':
					shell = 1;
					break;
#endif
				case 'v':
					trace = 1;
					if (*cp == 'v') {
						verbose = 1;
						cp++;
					}
					break;
				case 'h':
					help = 1;
				default:
					if (!help)
						fprintf (stderr, "Unknown option!\r\n");
					usage ();
				break;
			}
		}
	}
	return (i);
}

void run_test (const Test_t *tp)
{
	(*tp->fct) ();
}

void run_all (void)
{
	const Test_t	*tp;
	unsigned	i;

	for (i = 0, tp = tests; i < sizeof (tests) / sizeof (Test_t); i++, tp++)
		run_test (tp);
}

void run_named (const char *name)
{
	const Test_t	*tp;
	unsigned	i;

	for (i = 0, tp = tests; i < sizeof (tests) / sizeof (Test_t); i++, tp++)
		if (!strcmp (name, tp->name)) {
			run_test (tp);
			return;
		}

	printf ("No such test!\r\n");
	exit (1);
}

int main (int argc, const char *argv [])
{
	int	n;

	n = do_switches (argc, argv);
#ifdef DDS_DEBUG
	if (shell)
		DDS_Debug_start ();
#endif
	DDS_entity_name ("Technicolor API Tester");
	DDS_RTPS_control (!no_rtps);
	if (n == argc)
		run_all ();
	else
		while (n < argc)
			run_named (argv [n++]);
	return (0);
}

