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

/* main.c -- Test program for the Type interop functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <poll.h>
#endif
#include "tty.h"
#include "libx.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"
#include "static.h"
#include "dynamic.h"

unsigned domain_id = 9;

int main (int argc, const char *argv [])
{
	DDS_DomainParticipant	part;
	unsigned		sw, sr, dw, dr, i;
	int			error;

	DDS_entity_name ("Technicolor Interop test");

	printf ("Test 1: dynamic type first, then static.\r\n");
	printf ("----------------------------------------\r\n");

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

	printf ("DDS Domain Participant created.\r\n");

	/* Register the dynamic shape topic type. */
	register_dynamic_type (part);
	printf ("DDS dynamic Topic type registered.\r\n");

	/* Register the static topic type. */
	register_static_type (part);
	printf ("DDS static Topic type registered.\r\n");

	dw = dynamic_writer_create ("Square");
	dr = dynamic_reader_create ("Square");
	sw = static_writer_create ("Square");
	sr = static_reader_create ("Square");
	for (i = 0; i < 10; i++) {
		dynamic_writer_write (dw, "Red", i * 9 + 5, i * 11 + 5);
		static_writer_write (sw, "Yellow", i * 10, i * 12);
		usleep (100000);
	}
	dynamic_writer_delete (dw);
	dynamic_reader_delete (dr);
	static_writer_delete (sw);
	static_reader_delete (sr);

	unregister_dynamic_type ();
	unregister_static_type ();

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	printf ("DDS Participant deleted\r\n\r\n");

	printf ("Test 2: static type first, then dynamic.\r\n");
	printf ("----------------------------------------\r\n");

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

	printf ("DDS Domain Participant created.\r\n");

	/* Register the static topic type. */
	register_static_type (part);
	printf ("DDS static Topic type registered.\r\n");

	/* Register the dynamic shape topic type. */
	register_dynamic_type (part);
	printf ("DDS dynamic Topic type registered.\r\n");

	sw = static_writer_create ("Circle");
	sr = static_reader_create ("Circle");
	dw = dynamic_writer_create ("Circle");
	dr = dynamic_reader_create ("Circle");
	for (i = 0; i < 10; i++) {
		static_writer_write (sw, "Green", i * 11, i * 13);
		dynamic_writer_write (dw, "Magenta", i * 10 + 5, i * 12 + 5);
		usleep (100000);
	}
	static_writer_delete (sw);
	static_reader_delete (sr);
	dynamic_writer_delete (dw);
	dynamic_reader_delete (dr);
	unregister_static_type ();
	unregister_dynamic_type ();
	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	printf ("DDS Participant deleted\r\n\r\n");

	return (0);
}


