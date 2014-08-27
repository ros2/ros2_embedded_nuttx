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

/* main.c -- Test program for the Shapes functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <poll.h>
#endif
#include "tty.h"
#include "random.h"
#include "libx.h"
#include "../cdr/crc32.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"
#include "typecode.h"

const char		 *progname;
unsigned		 domain_id;		/* Domain. */
DDS_DomainParticipant	 part;
int			 verbose;

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "tsm -- TSM test program.\r\n");
	fprintf (stderr, "Usage: tsm [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -i <domain>        Domain Identifier.\r\n");
	fprintf (stderr, "   -v                 Verbose.\r\n");
	fprintf (stderr, "   -h                 Display help information.\r\n");
	exit (1);
}

/* get_num -- Get a number from the command line arguments. */

int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max)
{
	const char      *cp = *cpp;

	while (isspace (*cp))
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

/* get_str -- Get a string from the command line arguments. */

int get_str (const char **cpp, const char **name)
{
	const char      *cp = *cpp;

	while (isspace (*cp))
		cp++;

	*name = cp;
	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp;

	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'i':
					INC_ARG ();
					if (!get_num (&cp, &domain_id, 0, 255)) {
						printf ("Domain number expected!\r\n");
						usage ();
					}
					break;
				case 'v':
					verbose = 1;
					break;
				case 'h':
					usage ();
					break;
				default:
					printf ("Unknown option!\r\n");
					usage ();
				break;
			}
		}
	}
	return (i);
}

#include "wifidoctor.h"

DDS_TypeSupport com_technicolor_wifidoctor_AssociatedStationStats_ts;
DDS_TypeSupport com_technicolor_wifidoctor_APStats_ts;
DDS_TypeSupport com_technicolor_wifidoctor_STAStats_ts;
DDS_TypeSupport com_technicolor_wifidoctor_RadioStats_ts;
DDS_TypeSupport com_technicolor_wifidoctor_TestRequest_ts;
DDS_TypeSupport com_technicolor_wifidoctor_TestState_ts;

DDS_ReturnCode_t register_types (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	DDS_set_generate_callback (crc32_char);

	printf ("Registering com_technicolor_wifidoctor_AssociatedStationStats: "); fflush (stdout);
	com_technicolor_wifidoctor_AssociatedStationStats_ts = DDS_DynamicType_register (com_technicolor_wifidoctor_AssociatedStationStats_type);
       	if (!com_technicolor_wifidoctor_AssociatedStationStats_ts)
               	return (DDS_RETCODE_ERROR);

	printf ("ok, type registration: "); fflush (stdout);
	error = DDS_DomainParticipant_register_type (part, 
				com_technicolor_wifidoctor_AssociatedStationStats_ts,
				"com_technicolor_wifidoctor_AssociatedStationStats");
	if (error)
		return (error);

	printf ("ok!\r\n");
	DDS_TypeSupport_dump_type (1, com_technicolor_wifidoctor_AssociatedStationStats_ts, XDF_ALL);

	printf ("Registering com_technicolor_wifidoctor_APStats: "); fflush (stdout);
	com_technicolor_wifidoctor_APStats_ts = DDS_DynamicType_register (com_technicolor_wifidoctor_APStats_type);
       	if (!com_technicolor_wifidoctor_APStats_ts)
               	return (DDS_RETCODE_ERROR);

	printf ("ok, type registration: "); fflush (stdout);
	error = DDS_DomainParticipant_register_type (part, 
				com_technicolor_wifidoctor_APStats_ts,
				"com_technicolor_wifidoctor_APStats");
	if (error)
		return (error);

	printf ("ok!\r\n");
	DDS_TypeSupport_dump_type (1, com_technicolor_wifidoctor_APStats_ts, XDF_ALL);

	printf ("Registering com_technicolor_wifidoctor_STAStats: "); fflush (stdout);
	com_technicolor_wifidoctor_STAStats_ts = DDS_DynamicType_register (com_technicolor_wifidoctor_STAStats_type);
       	if (!com_technicolor_wifidoctor_STAStats_ts)
               	return (DDS_RETCODE_ERROR);

	printf (" ok, type registration: "); fflush (stdout);
	error = DDS_DomainParticipant_register_type (part, 
				com_technicolor_wifidoctor_STAStats_ts,
				"com_technicolor_wifidoctor_STAStats");
	if (error)
		return (error);

	printf ("ok!\r\n");
	DDS_TypeSupport_dump_type (1, com_technicolor_wifidoctor_STAStats_ts, XDF_ALL);

	printf ("Registering com_technicolor_wifidoctor_RadioStats: "); fflush (stdout);
	com_technicolor_wifidoctor_RadioStats_ts = DDS_DynamicType_register (com_technicolor_wifidoctor_RadioStats_type);
       	if (!com_technicolor_wifidoctor_RadioStats_ts)
               	return (DDS_RETCODE_ERROR);

	printf (" ok, type registration: "); fflush (stdout);
	error = DDS_DomainParticipant_register_type (part, 
				com_technicolor_wifidoctor_RadioStats_ts,
				"com_technicolor_wifidoctor_RadioStats");
	if (error)
		return (error);

	printf ("ok!\r\n");
	DDS_TypeSupport_dump_type (1, com_technicolor_wifidoctor_RadioStats_ts, XDF_ALL);

	printf ("Registering com_technicolor_wifidoctor_TestRequest: "); fflush (stdout);
	com_technicolor_wifidoctor_TestRequest_ts = DDS_DynamicType_register (com_technicolor_wifidoctor_TestRequest_type);
       	if (!com_technicolor_wifidoctor_TestRequest_ts)
               	return (DDS_RETCODE_ERROR);

	printf (" ok, type registration: "); fflush (stdout);
	error = DDS_DomainParticipant_register_type (part, 
				com_technicolor_wifidoctor_TestRequest_ts,
				"com_technicolor_wifidoctor_TestRequest");
	if (error)
		return (error);

	printf ("ok!\r\n");
	DDS_TypeSupport_dump_type (1, com_technicolor_wifidoctor_TestRequest_ts, XDF_ALL);

	printf ("Registering com_technicolor_wifidoctor_TestState: "); fflush (stdout);
	com_technicolor_wifidoctor_TestState_ts = DDS_DynamicType_register (com_technicolor_wifidoctor_TestState_type);
       	if (!com_technicolor_wifidoctor_TestState_ts)
               	return (DDS_RETCODE_ERROR);

	printf (" ok, type registration: "); fflush (stdout);
	error = DDS_DomainParticipant_register_type (part, 
				com_technicolor_wifidoctor_TestState_ts,
				"com_technicolor_wifidoctor_TestState");

	printf ("ok!\r\n");
	DDS_TypeSupport_dump_type (1, com_technicolor_wifidoctor_TestState_ts, XDF_ALL);
	return (error);
}

void unregister_types (DDS_DomainParticipant part)
{
	printf ("unregistering com_technicolor_wifidoctor_AssociatedStationStats: "); fflush (stdout);
	DDS_DomainParticipant_unregister_type (part, com_technicolor_wifidoctor_AssociatedStationStats_ts, "com_technicolor_wifidoctor_AssociatedStationStats");
	printf ("ok!\r\nunregistering com_technicolor_wifidoctor_APStats: "); fflush (stdout);
	DDS_DomainParticipant_unregister_type (part, com_technicolor_wifidoctor_APStats_ts, "com_technicolor_wifidoctor_APStats");
	printf ("ok!\r\nunregistering com_technicolor_wifidoctor_STAStats: "); fflush (stdout);
	DDS_DomainParticipant_unregister_type (part, com_technicolor_wifidoctor_STAStats_ts, "com_technicolor_wifidoctor_STAStats");
	printf ("ok!\r\nunregistering com_technicolor_wifidoctor_RadioStats: "); fflush (stdout);
	DDS_DomainParticipant_unregister_type (part, com_technicolor_wifidoctor_RadioStats_ts, "com_technicolor_wifidoctor_RadioStats");
	printf ("ok!\r\nunregistering com_technicolor_wifidoctor_TestRequest: "); fflush (stdout);
	DDS_DomainParticipant_unregister_type (part, com_technicolor_wifidoctor_TestRequest_ts, "com_technicolor_wifidoctor_TestRequest");
	printf ("ok!\r\nunregistering com_technicolor_wifidoctor_TestState: "); fflush (stdout);
	DDS_DomainParticipant_unregister_type (part, com_technicolor_wifidoctor_TestState_ts, "com_technicolor_wifidoctor_TestState");
	printf ("ok!\r\n");
}

int main (int argc, const char *argv [])
{
	int			error;

	do_switches (argc, argv);

	DDS_entity_name ("TSM");

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");
	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

	/* Register the topic types. */
	error = register_types (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_register_type ('HelloWorldData') failed (%s)!", DDS_error (error));
	}
	if (verbose)
		printf ("DDS Topic types registered.\r\n");

	DDS_wait (100);

	unregister_types (part);

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Participant deleted\r\n");

	return (0);
}

