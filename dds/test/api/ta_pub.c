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
#include "ta_type.h"
#include "ta_pub.h"

extern int no_rtps;

static void test_qos (int enabled)
{
	DDS_PoolConstraints		c;
	DDS_DomainParticipantFactoryQos	qos;
	DDS_DomainParticipant		p, rem_p = NULL;
	DDS_Publisher			pub;
	DDS_Subscriber			sub, rem_sub = NULL;
	DDS_DataWriter			dw;
	DDS_DataReader			dr, rem_dr = NULL;
	DDS_Topic			t, rem_t = NULL;
	DDS_TopicDescription		td, rem_td = NULL;
	DDS_PublisherQos		ppq, refpq;
	DDS_ReturnCode_t		r;
	static char			*buf [] = { "Hello", "World" };
	static unsigned char		data [] = { 0x77, 0x88, 0xaa, 0xbb, 0xcc };
	unsigned char			d2 [sizeof (data) + 1];
	unsigned			n;
	size_t				i;
	int				err;

	r = DDS_get_default_pool_constraints (&c, 5000, 2000);
	c.max_domains = 3;
	c.min_list_nodes *= 5;
	c.max_list_nodes *= 5;
	c.min_locators *= 5;
	c.max_locators *= 5;
	c.min_local_readers *= 4;
	c.max_local_readers *= 4;
	c.min_local_writers *= 4;
	c.max_local_writers *= 4;
	c.min_remote_readers *= 4;
	c.max_remote_readers *= 4;
	c.min_remote_writers *= 4;
	c.max_remote_writers *= 4;
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_set_pool_constraints (&c);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Set factory QoS to %sabled.\r\n", (enabled) ? "en" : "dis");
	r = DDS_DomainParticipantFactory_get_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);
	qos.entity_factory.autoenable_created_entities = enabled;
	r = DDS_DomainParticipantFactory_set_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);

	/* Create remote participant. */
	if (!no_rtps) {
		rem_p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
		fail_unless (rem_p != NULL);
		r = register_HelloWorldData_type (rem_p);
		fail_unless (r == DDS_RETCODE_OK);
	}

	DDS_PublisherQos__init (&refpq);
	for (i = 0; (size_t) i < sizeof (buf) / sizeof (char *); i++) {
		err = dds_seq_append (&refpq.partition.name, &buf [i]);
		fail_unless (err == 0);
	}
	err = dds_seq_from_array (&refpq.group_data, data, sizeof (data));
	fail_unless (err == 0);
	v_printf (" - Create publisher with specific QoS parameters.\r\n");
	pub = DDS_DomainParticipant_create_publisher (p, &refpq, NULL, 0);
	fail_unless (pub != NULL);
	fail_unless (DDS_DomainParticipant_contains_entity (p, DDS_Entity_get_instance_handle (pub)));
	memset (&ppq, 0, sizeof (ppq));
	r = DDS_Publisher_get_qos (pub, &ppq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (refpq.partition.name) == DDS_SEQ_LENGTH (ppq.partition.name) &&
		     DDS_SEQ_LENGTH (refpq.group_data.value) == sizeof (data));
	DDS_SEQ_FOREACH (refpq.partition.name, i)
		fail_unless (!strcmp (DDS_SEQ_ITEM (ppq.partition.name, i),
				      DDS_SEQ_ITEM (refpq.partition.name, i)));
	n = dds_seq_to_array (&ppq.group_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_PublisherQos__clear (&ppq);
	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Create topic/publisher/writer/subscriber/reader entities.\r\n");
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);
	fail_unless (DDS_DomainParticipant_contains_entity (p, DDS_Entity_get_instance_handle (t)));
	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);
	fail_unless (DDS_DomainParticipant_contains_entity (p, DDS_Entity_get_instance_handle (dw)));
	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);
	fail_unless (DDS_DomainParticipant_contains_entity (p, DDS_Entity_get_instance_handle (sub)));
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);
	fail_unless (DDS_DomainParticipant_contains_entity (p, DDS_Entity_get_instance_handle (dr)));

	/* Create remote subscriber/topic/reader. */
	if (!no_rtps) {
		rem_t = DDS_DomainParticipant_create_topic (rem_p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
		fail_unless (rem_t != NULL);
		rem_td = DDS_DomainParticipant_lookup_topicdescription (rem_p, "HelloWorld");
		fail_unless (rem_td != NULL);
		rem_sub = DDS_DomainParticipant_create_subscriber (rem_p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
		fail_unless (rem_sub != NULL);
		fail_unless (DDS_DomainParticipant_contains_entity (rem_p, DDS_Entity_get_instance_handle (rem_sub)));
		rem_dr = DDS_Subscriber_create_datareader (rem_sub, rem_td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
		fail_unless (rem_dr != NULL);
		fail_unless (DDS_DomainParticipant_contains_entity (rem_p, DDS_Entity_get_instance_handle (rem_dr)));
		delay ();
	}

	v_printf (" - Update publisher QoS.\r\n");
	r = DDS_Publisher_get_qos (pub, &ppq);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_PublisherQos__clear (&refpq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&ppq, &refpq, sizeof (refpq)));
	for (i = 0; (size_t) i < sizeof (buf) / sizeof (char *); i++) {
		err = dds_seq_append (&refpq.partition.name, &buf [i]);
		fail_unless (err == 0);
	}
	err = dds_seq_from_array (&refpq.group_data, data, sizeof (data));
	fail_unless (err == 0);

	r = DDS_Publisher_set_qos (pub, &refpq);
	fail_unless (r == DDS_RETCODE_OK);

	delay ();
	r = DDS_Publisher_get_qos (pub, &ppq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (refpq.partition.name) == DDS_SEQ_LENGTH (ppq.partition.name) &&
		     DDS_SEQ_LENGTH (ppq.group_data.value) == sizeof (data));
	DDS_SEQ_FOREACH (ppq.partition.name, i)
		fail_unless (!strcmp (DDS_SEQ_ITEM (ppq.partition.name, i),
				      DDS_SEQ_ITEM (refpq.partition.name, i)));
	n = dds_seq_to_array (&ppq.group_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_PublisherQos__clear (&ppq);
	DDS_PublisherQos__clear (&refpq);

	delay ();
	if (!enabled) {
		v_printf (" - Enable all entities.\r\n");
		r = DDS_Entity_enable (p);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DomainParticipant_enable (p);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Entity_enable (t);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Topic_enable (t);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Entity_enable (pub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Publisher_enable (pub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Entity_enable (sub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Subscriber_enable (sub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Entity_enable (dw);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataWriter_enable (dw);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Entity_enable (dr);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_enable (dr);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Entity_enable (NULL);
		fail_unless (r == DDS_RETCODE_BAD_PARAMETER);
		r = DDS_Entity_enable (p);

		/* Enable remote participant/subscriber/topic/reader. */
		if (!no_rtps) {
			r = DDS_Entity_enable (rem_p);
			fail_unless (r == DDS_RETCODE_OK);
			r = DDS_Entity_enable (rem_t);
			fail_unless (r == DDS_RETCODE_OK);
			r = DDS_Entity_enable (rem_sub);
			fail_unless (r == DDS_RETCODE_OK);
			r = DDS_Entity_enable (rem_dr);
			fail_unless (r == DDS_RETCODE_OK);
		}
		sleep (1);
	}
	v_printf (" - Delete child entities.\r\n");
	r = DDS_Publisher_delete_datawriter (pub, dw);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_Subscriber_delete_datareader (sub, dr);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);

	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);

	/* Delete remote participant/subscriber/topic/reader. */
	if (!no_rtps) {
		r = DDS_Subscriber_delete_datareader (rem_sub, rem_dr);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DomainParticipant_delete_subscriber (rem_p, rem_sub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DomainParticipant_delete_topic (rem_p, rem_t);
		fail_unless (r == DDS_RETCODE_OK);
		unregister_HelloWorldData_type (rem_p);
		r = DDS_DomainParticipantFactory_delete_participant (rem_p);
		fail_unless (r == DDS_RETCODE_OK);
	}
}

static void offered_deadline_missed (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void publication_matched (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_PublicationMatchedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void liveliness_lost (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_LivelinessLostStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void offered_incompatible_qos (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static DDS_PublisherListener pub_listener = {
	offered_deadline_missed,
	publication_matched,
	liveliness_lost,
	offered_incompatible_qos
};

static void test_listener (void)
{
	DDS_DomainParticipant		p;
	DDS_Publisher			pub;
	DDS_PublisherListener		*lp;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	v_printf (" - Test publisher listener.\r\n");
	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);
	lp = DDS_Publisher_get_listener (pub);
	fail_unless (lp != NULL &&
		     lp->on_offered_deadline_missed == NULL && 
		     lp->on_publication_matched == NULL &&
		     lp->on_liveliness_lost == NULL &&
		     lp->on_offered_incompatible_qos == NULL);
	r = DDS_Publisher_set_listener (pub, &pub_listener,
			DDS_OFFERED_DEADLINE_MISSED_STATUS |
			DDS_PUBLICATION_MATCHED_STATUS |
			DDS_LIVELINESS_LOST_STATUS |
			DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test specific publisher listener updates.\r\n");
	lp = DDS_Publisher_get_listener (pub);
	fail_unless (lp != NULL &&
		lp->on_offered_deadline_missed == offered_deadline_missed &&
		lp->on_publication_matched == publication_matched &&
		lp->on_liveliness_lost == liveliness_lost &&
		lp->on_offered_incompatible_qos == offered_incompatible_qos);

	v_printf (" - Test default publisher listener update.\r\n");
	r = DDS_Publisher_set_listener (pub, NULL, DDS_OFFERED_DEADLINE_MISSED_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_Publisher_get_listener (pub);
	fail_unless (lp != NULL &&
		lp->on_offered_deadline_missed == NULL &&
		lp->on_publication_matched == NULL &&
		lp->on_liveliness_lost == NULL &&
		lp->on_offered_incompatible_qos == NULL);

	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_aux (void)
{
	DDS_DomainParticipant		np, p;
	DDS_Publisher			pub;
	DDS_Topic			t;
	DDS_DataWriter			dw, dw0, dw1;
	DDS_StatusCondition		sc, sc2;
	DDS_StatusMask			sm;
	DDS_InstanceHandle_t		h, h2;
	DDS_Duration_t			w;
	DDS_DataWriterQos		dwq;
	DDS_ReturnCode_t		r;

	v_printf (" - Test auxiliary publisher functions.\r\n");
	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);

	sc = DDS_Publisher_get_statuscondition (pub);
	fail_unless (sc != NULL);
	sc2 = DDS_Entity_get_statuscondition (pub);
	fail_unless (sc2 == sc);
	sc2 = DDS_Entity_get_statuscondition (NULL);
	fail_unless (sc2 == 0);
	sm = DDS_Publisher_get_status_changes (pub);
	fail_unless (sm == 0);
	sm = DDS_Entity_get_status_changes (pub);
	fail_unless (sm == 0);
	/*dbg_printf ("(mask=%u)", sm);*/
	h = DDS_Entity_get_instance_handle (pub);
	fail_unless (h != 0);
	h2 = DDS_Publisher_get_instance_handle (pub);
	fail_unless (h == h2);
	h2 = DDS_Publisher_get_instance_handle (0);
	fail_unless (h2 == 0);
	np = DDS_Publisher_get_participant (pub);
	fail_unless (np == p);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);

	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	dw0 = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw0 != NULL);

	dw = DDS_Publisher_lookup_datawriter (pub, "HelloWorld");
	fail_unless (dw == dw0);

	dw1 = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw1 != NULL);

	w.sec = 1;
	w.nanosec = 0;
	r = DDS_Publisher_wait_for_acknowledgments (pub, &w);
	fail_unless (r == DDS_RETCODE_OK);

	delay ();

	r = DDS_Publisher_delete_contained_entities (pub);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_Publisher_get_default_datawriter_qos (pub, &dwq);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test default child QoS of publisher entities\r\n");
	dwq.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
	dwq.ownership_strength.value = 12;
	r = DDS_Publisher_set_default_datawriter_qos (pub, &dwq);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test suspend/resume publications.\r\n");
	r = DDS_Publisher_suspend_publications (pub);
	dw0 = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw0 != NULL);

	dwq.ownership_strength.value = 15;
	r = DDS_Publisher_set_default_datawriter_qos (pub, &dwq);
	fail_unless (r == DDS_RETCODE_OK);

	dw1 = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw1 != NULL);
	r = DDS_Publisher_resume_publications (pub);

	delay ();

	r = DDS_Publisher_copy_from_topic_qos (pub, &dwq, NULL);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test coherency (not implemented).\r\n");
	r = DDS_Publisher_begin_coherent_changes (pub);
	fail_unless (r == DDS_RETCODE_UNSUPPORTED);
	r = DDS_Publisher_end_coherent_changes (pub);
	fail_unless (r == DDS_RETCODE_UNSUPPORTED);

	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_PRECONDITION_NOT_MET);
	r = DDS_Publisher_delete_datawriter (pub, dw0);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_Publisher_delete_datawriter (pub, dw1);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DomainParticipant_delete_topic (p, t);	
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);

	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_publisher (void)
{
	dbg_printf ("Publisher ");
	if (trace)
		fflush (stdout);
	if (verbose)
		printf ("\r\n");
	test_qos (0);
	test_qos (1);
	test_listener ();
	test_aux ();
	dbg_printf (" - success!\r\n");
}


