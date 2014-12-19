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

/* main.c -- Test program to test the DDS/DCPS functionality. */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libx.h"
#include "error.h"
#include "config.h"
#ifdef DDS_SECURITY
#include "assert.h"
#include "dds/dds_security.h"
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#else
#include "msecplug/msecplug.h"
#include "../../plugins/secplug/xmlparse.h"
#endif
#include "../../plugins/security/engine_fs.h"
#endif
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"

extern const int	forward_available;	/* from: rtps_fwd.h */

const char		*progname;
int			verbose;		/* Verbose if set. */
int			aborting;		/* Abort program if set. */
int			paused = 0;		/* Pause program if set. */
unsigned		domain_id;		/* Domain id. */
#ifdef DDS_SECURITY
char                    *engine_id;		/* Engine id. */
char                    *cert_path;		/* Path where the certificate is located. */
char                    *key_path;		/* Path where the private ket is located. */
char                    *realm_name;		/* Realm name. */
#endif
unsigned		max_events = ~0;	/* Max. # of steps. */
unsigned		sleep_time = 1000;	/* Sleep time (1000ms = 1s). */
unsigned		forwarding_mode = 15;	/* forward every kind to every kind: see dds_data.h for details */

#undef dbg_printf
#define	dbg_printf printf

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "fwd -- test program for the DDS forwarder.\r\n");
	fprintf (stderr, "Usage: fwd [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -i	<num>	Domain id to use.\r\n");
	fprintf (stderr, "   -v		Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv  	Extra verbose: log detailed functionality.\r\n");
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>  Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>  Pass the path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>  Pass the path of the private key to use.\r\n");
	fprintf (stderr, "   -z <RealmName> Pass the realm name for testing.\r\n");
#endif
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

int get_str (const char **cpp, const char **name)
{
	const char	*cp = *cpp;

	while (isspace (*cp))
		cp++;

	*name = cp;
	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	do if (!*cp) { i++; cp = argv [i]; } while (0)

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp;
#ifdef DDS_SECURITY
	const char      *arg_input;
#endif

	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'v':
					verbose = 1;
					if (*cp == 'v') {
						verbose = 2;
						cp++;
					}
					break;
				case 'i':
					INC_ARG ();
					if (!get_num (&cp, &domain_id, 0, 256))
						usage ();
					break;
#ifdef DDS_SECURITY
			        case 'e':
					INC_ARG ();
					if (!get_str (&cp, &arg_input))
						usage();
					engine_id = malloc (strlen (arg_input) + 1);
					strcpy (engine_id, arg_input);
					break;
			        case 'c':
					INC_ARG ();
					if (!get_str (&cp, &arg_input))
						usage ();
					cert_path = malloc (strlen (arg_input) + 1);
					strcpy (cert_path, arg_input);
					break;
			        case 'k':
					INC_ARG ();
					if (!get_str (&cp, &arg_input))
						usage ();
					key_path = malloc (strlen (arg_input) + 1);
					strcpy (key_path, arg_input);

					break;
			        case 'z':
					INC_ARG ();
					if (!get_str (&cp, &arg_input))
						usage ();
					realm_name = malloc (strlen (arg_input) + 1);
					strcpy (realm_name, arg_input);
					break;
#endif
				default:
					usage ();
				break;
			}
		}
	}
	return (i);
}

#ifdef DDS_SECURITY

#define fail_unless     assert

static void enable_security (void)
{
	DDS_Credentials		credentials;
	DDS_ReturnCode_t	error;

	error = DDS_SP_set_policy ();
	if (error)
		fatal ("DDS_SP_set_policy() returned error (%s)!", DDS_error (error));

#ifdef MSECPLUG_WITH_SECXML
	if (!DDS_SP_parse_xml ("security.xml"))
		fatal_printf ("MSP: no DDS security rules in 'security.xml'!\r\n");

#else
	DDS_SP_add_domain();
	if (!realm_name)
		DDS_SP_add_participant ();
	else
		DDS_SP_set_participant_access (DDS_SP_add_participant (), strcat (realm_name, "*"), 2, 0);
#endif
	if (!cert_path || !key_path)
		fatal("Error: you must provide a valid certificate path and a valid private key path\r\n");

	if (engine_id) {
		DDS_SP_init_engine (engine_id, init_engine_fs);
		credentials.credentialKind = DDS_ENGINE_BASED;
		credentials.info.engine.engine_id = engine_id;
		credentials.info.engine.cert_id = cert_path;
		credentials.info.engine.priv_key_id = key_path;
	}
	else {
		credentials.credentialKind = DDS_FILE_BASED;
		credentials.info.filenames.private_key_file = key_path;
		credentials.info.filenames.certificate_chain_file = cert_path;
	}

	error = DDS_Security_set_credentials ("Technicolor DDS Forwarder", &credentials);
}

static void cleanup_security (void)
{
	/* Cleanup security submodule. */
	DDS_SP_access_db_cleanup ();
	DDS_SP_engine_cleanup ();

	/* Cleanup malloc-ed memory. */
	if (engine_id)
		free (engine_id);
	if (cert_path)
		free (cert_path);
	if (key_path)
		free (key_path);
	if (realm_name)
		free (realm_name);
}

#endif

int main (int argc, const char *argv [])
{
	DDS_DomainParticipant		part;
	DDS_DomainParticipantFactoryQos	dfqos;
	DDS_DomainParticipantQos	dpqos;
	int				error;

	do_switches (argc, argv);

	config_set_number (DC_Forward, forwarding_mode);

	if (verbose > 1)
		DDS_Log_stdio (1);

	DDS_entity_name ("Technicolor DDS Forwarder");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif
	aborting = 0;
#ifdef DDS_DEBUG
	DDS_Debug_start ();
	DDS_Debug_abort_enable (&aborting);
	DDS_Debug_control_enable (&paused, &max_events, &sleep_time);
#endif

	DDS_RTPS_control (1);

	/* Test get_qos() fynctionality. */
	if ((error = DDS_DomainParticipantFactory_get_qos (&dfqos)) != DDS_RETCODE_OK)
		fatal ("DDS_DomainParticipantFactory_get_qos () failed (%s)!", DDS_error (error));

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
			domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

	if (verbose)
		dbg_printf ("DDS Domain Participant created.\r\n");

	/* Test get_qos() fynctionality. */
	if ((error = DDS_DomainParticipant_get_qos (part, &dpqos)) != DDS_RETCODE_OK)
		fatal ("DDS_DomainParticipant_get_qos () failed (%s)!", DDS_error (error));

	/* Keep on forwarding as long as we do not quit the application */
	while (!aborting)
		usleep (1000000);

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	if (verbose)
		dbg_printf ("DDS Participant deleted\r\n");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		cleanup_security ();
#endif
	return (0);
}

