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

#include "test.h"
#include "ta_pfact.h"

/* DDS_DomainParticipant DDS_DomainParticipantFactory_create_participant(
	DDS_DomainId_t domain_id,
	const DDS_DomainParticipantQos *qos,
	const DDS_DomainParticipantListener *a_listener,
	DDS_StatusMask mask
); */

/* DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_participant(
	DDS_DomainParticipant a_participant
); */

static void try_create (DDS_DomainId_t did,
			const DDS_DomainParticipantQos *qos,
			const DDS_DomainParticipantListener *l,
			DDS_StatusMask m)
{
	DDS_DomainParticipant	p;
	DDS_ReturnCode_t	r;

	p = DDS_DomainParticipantFactory_create_participant (did, qos, l, m);
	fail_unless (p != NULL);
	delay ();
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_create (void)
{
	DDS_DomainParticipantQos	qos;
	DDS_ReturnCode_t		r;
	char				buf [] = "Hello folks!";

	v_printf ("\r\n - Test domain participant create.\r\n");
	try_create (0, NULL, NULL, 0);
	r = DDS_DomainParticipantFactory_get_default_participant_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK &&
	             qos.user_data.value._length == 0 &&
		     qos.entity_factory.autoenable_created_entities == 1);
	v_printf (" - Test default domain participant QoS.\r\n");
	try_create (229, &qos, NULL, 0);
	r = dds_seq_from_array (&qos.user_data.value, buf, sizeof (buf));
	fail_unless (r == DDS_RETCODE_OK);
	try_create (200, &qos, NULL, 0);
	dds_seq_cleanup (&qos.user_data.value);
}

static void test_qos (void)
{
	DDS_DomainParticipantFactoryQos	qos;
	DDS_ReturnCode_t		r;

	r = DDS_DomainParticipantFactory_get_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test default/update participant factory QoS.\r\n");
	fail_unless (qos.entity_factory.autoenable_created_entities == 1);
	qos.entity_factory.autoenable_created_entities = 0;
	r = DDS_DomainParticipantFactory_set_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipantFactory_get_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK &&
		     qos.entity_factory.autoenable_created_entities == 0);
	qos.entity_factory.autoenable_created_entities = 1;
	r = DDS_DomainParticipantFactory_set_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipantFactory_get_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK &&
		     qos.entity_factory.autoenable_created_entities == 1);
}

static void test_dqos (void)
{
	DDS_DomainParticipantQos qos;
	DDS_ReturnCode_t	 r;
	char			 buf [] = "Folks hello!";

	v_printf (" - Test default contained entities QoS.\r\n");
	r = DDS_DomainParticipantFactory_get_default_participant_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK &&
	             qos.entity_factory.autoenable_created_entities == 1 &&
		     qos.user_data.value._length == 0);

	r = dds_seq_from_array (&qos.user_data.value, buf, sizeof (buf));
	fail_unless (r == DDS_RETCODE_OK);
	qos.entity_factory.autoenable_created_entities = 1;
	r = DDS_DomainParticipantFactory_set_default_participant_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);
	try_create (123, NULL, NULL, 0);
	r = DDS_DomainParticipantFactory_set_default_participant_qos (DDS_PARTICIPANT_QOS_DEFAULT);
	try_create (19, NULL, NULL, 0);
	dds_seq_cleanup (&qos.user_data.value);
}

static void test_multi (void)
{
	DDS_DomainParticipant	p, p1, p2;
	DDS_PoolConstraints	c;
	DDS_ReturnCode_t	r;

	v_printf (" - Test multiple participants in same domain.\r\n");
	r = DDS_get_default_pool_constraints (&c, 0, 0);
	fail_unless (r == DDS_RETCODE_OK);
	c.max_domains = 2;
	c.min_topics += 4;
	c.max_topics += 4;
	c.max_list_nodes *= 3;
	c.min_list_nodes *= 3;
	c.max_list_nodes *= 3;
	c.min_locators *= 3;
	c.max_locators *= 3;
	c.min_local_readers *= 2;
	c.min_local_readers += 16;
	c.max_local_readers *= 2;
	c.max_local_readers += 16;
	c.min_local_writers *= 2;
	c.min_local_writers += 16;
	c.max_local_writers *= 2;
	c.max_local_writers += 16;
	c.min_remote_readers *= 4;
	c.max_remote_readers *= 4;
	c.min_remote_writers *= 4;
	c.max_remote_writers *= 4;
	r = DDS_set_pool_constraints (&c);
	fail_unless (r == DDS_RETCODE_OK);
	p1 = DDS_DomainParticipantFactory_create_participant (20, NULL, NULL, 0);
	fail_unless (p1 != NULL);
	p2 = DDS_DomainParticipantFactory_create_participant (20, NULL, NULL, 0);
	fail_unless (p1 != p2 && p2 != NULL);
	p = DDS_DomainParticipantFactory_lookup_participant (20);
	fail_unless (p == p1 || p == p2);
	delay ();

	/*DDS_Debug_pool_dump ();
	DDS_Debug_disc_dump ();*/
	r = DDS_DomainParticipantFactory_delete_participant (p1);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipantFactory_delete_participant (p2);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_participant_factory (void)
{
	dbg_printf ("Participant Factory ");
	if (trace)
		fflush (stdout);
	test_create ();
	test_qos ();
	test_dqos ();
	test_multi ();
	dbg_printf (" - success!\r\n");
}

