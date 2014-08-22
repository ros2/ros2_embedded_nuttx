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

static void test_qos (int enabled)
{
	DDS_DomainParticipantFactoryQos	qos;
	DDS_DomainParticipant		p;
	DDS_Publisher			pub;
	DDS_Subscriber			sub;
	DDS_DataWriter			dw;
	DDS_DataReader			dr;
	DDS_Topic			t;
	DDS_TopicDescription		td;
	DDS_SubscriberQos		psq, refsq;
	DDS_ReturnCode_t		r;
	static char			*buf [] = { "Hello", "World" };
	static unsigned char		data [] = { 0x77, 0x88, 0xaa, 0xbb, 0xcc };
	unsigned char			d2 [sizeof (data) + 1];
	unsigned			n;
	size_t				i;
	int				err;

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

	DDS_SubscriberQos__init (&refsq);
	for (i = 0; (size_t) i < sizeof (buf) / sizeof (char *); i++) {
		err = dds_seq_append (&refsq.partition.name, &buf [i]);
		fail_unless (err == 0);
	}
	err = dds_seq_from_array (&refsq.group_data, data, sizeof (data));
	fail_unless (err == 0);
	v_printf (" - Create subscriber with specific QoS parameters.\r\n");
	sub = DDS_DomainParticipant_create_subscriber (p, &refsq, NULL, 0);
	fail_unless (sub != NULL);
	memset (&psq, 0, sizeof (psq));
	r = DDS_Subscriber_get_qos (sub, &psq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (refsq.partition.name) == DDS_SEQ_LENGTH (psq.partition.name) &&
		     DDS_SEQ_LENGTH (refsq.group_data.value) == sizeof (data));
	DDS_SEQ_FOREACH (refsq.partition.name, i)
		fail_unless (!strcmp (DDS_SEQ_ITEM (psq.partition.name, i),
				      DDS_SEQ_ITEM (refsq.partition.name, i)));
	n = dds_seq_to_array (&psq.group_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_SubscriberQos__clear (&psq);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Create topic/publisher/writer/subscriber/reader entities.\r\n");
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);
	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);
	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);

	v_printf (" - Update subscriber QoS.\r\n");
	r = DDS_Subscriber_get_qos (sub, &psq);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_SubscriberQos__clear (&refsq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&psq, &refsq, sizeof (refsq)));
	for (i = 0; (size_t) i < sizeof (buf) / sizeof (char *); i++) {
		err = dds_seq_append (&refsq.partition.name, &buf [i]);
		fail_unless (err == 0);
	}
	err = dds_seq_from_array (&refsq.group_data, data, sizeof (data));
	fail_unless (err == 0);

	r = DDS_Subscriber_set_qos (sub, &refsq);
	fail_unless (r == DDS_RETCODE_OK);

	delay ();
	r = DDS_Subscriber_get_qos (sub, &psq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (refsq.partition.name) == DDS_SEQ_LENGTH (psq.partition.name) &&
		     DDS_SEQ_LENGTH (psq.group_data.value) == sizeof (data));
	DDS_SEQ_FOREACH (psq.partition.name, i)
		fail_unless (!strcmp (DDS_SEQ_ITEM (psq.partition.name, i),
				      DDS_SEQ_ITEM (refsq.partition.name, i)));
	n = dds_seq_to_array (&psq.group_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_SubscriberQos__clear (&refsq);
	DDS_SubscriberQos__clear (&psq);

	delay ();
	if (!enabled) {
		v_printf (" - Enable child entities.\r\n");
		r = DDS_DomainParticipant_enable (p);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Topic_enable (t);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Publisher_enable (pub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_Subscriber_enable (sub);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataWriter_enable (dw);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_enable (dr);
		fail_unless (r == DDS_RETCODE_OK);
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
}

static void sample_rejected (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleRejectedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void requested_incompatible_qos (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void sample_lost (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleLostStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void subscription_matched (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SubscriptionMatchedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void data_available (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
}

static void liveliness_changed (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_LivelinessChangedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void data_on_readers (
	DDS_SubscriberListener *self,
	DDS_Subscriber the_subscriber)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_subscriber)
}

static void requested_deadline_missed (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static DDS_SubscriberListener sub_listener = {
	sample_rejected,
	requested_incompatible_qos,
	sample_lost,
	subscription_matched,
	data_available,
	liveliness_changed,
	data_on_readers,
	requested_deadline_missed
};

static void test_listener (void)
{
	DDS_DomainParticipant		p;
	DDS_Subscriber			sub;
	DDS_SubscriberListener		*lp;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	v_printf (" - Test subscriber listener.\r\n");
	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);
	lp = DDS_Subscriber_get_listener (sub);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == NULL && 
		     lp->on_requested_incompatible_qos == NULL &&
		     lp->on_sample_lost == NULL &&
		     lp->on_subscription_matched == NULL &&
		     lp->on_data_available == NULL &&
		     lp->on_liveliness_changed == NULL &&
		     lp->on_data_on_readers == NULL &&
		     lp->on_requested_deadline_missed == NULL);
	r = DDS_Subscriber_set_listener (sub, &sub_listener,
			DDS_SAMPLE_REJECTED_STATUS |
			DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS |
			DDS_SAMPLE_LOST_STATUS |
			DDS_SUBSCRIPTION_MATCHED_STATUS |
			DDS_DATA_AVAILABLE_STATUS |
			DDS_LIVELINESS_CHANGED_STATUS |
			DDS_DATA_ON_READERS_STATUS |
			DDS_REQUESTED_DEADLINE_MISSED_STATUS);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test specific subscriber listener updates.\r\n");
	lp = DDS_Subscriber_get_listener (sub);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == sample_rejected && 
		     lp->on_requested_incompatible_qos == requested_incompatible_qos &&
		     lp->on_sample_lost == sample_lost &&
		     lp->on_subscription_matched == subscription_matched &&
		     lp->on_data_available == data_available &&
		     lp->on_liveliness_changed == liveliness_changed &&
		     lp->on_data_on_readers == data_on_readers &&
		     lp->on_requested_deadline_missed == requested_deadline_missed);

	v_printf (" - Test default subscriber listener update.\r\n");
	r = DDS_Subscriber_set_listener (sub, NULL, DDS_REQUESTED_DEADLINE_MISSED_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_Subscriber_get_listener (sub);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == NULL && 
		     lp->on_requested_incompatible_qos == NULL &&
		     lp->on_sample_lost == NULL &&
		     lp->on_subscription_matched == NULL &&
		     lp->on_data_available == NULL &&
		     lp->on_liveliness_changed == NULL &&
		     lp->on_data_on_readers == NULL &&
		     lp->on_requested_deadline_missed == NULL);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_aux (void)
{
	DDS_DomainParticipant		np, p;
	DDS_Subscriber			sub;
	DDS_Topic			t;
	DDS_TopicDescription		td;
	DDS_DataReader			dr, dr0, dr1;
	DDS_StatusCondition		sc, sc2;
	DDS_StatusMask			sm;
	DDS_InstanceHandle_t		h, h2;
	DDS_DataReaderQos		drq;
	DDS_ReturnCode_t		r;

	v_printf (" - Test auxiliary subscriber functions.\r\n");
	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);

	sc = DDS_Subscriber_get_statuscondition (sub);
	fail_unless (sc != NULL);
	sc2 = DDS_Entity_get_statuscondition (sub);
	fail_unless (sc2 != NULL);
	
	sm = DDS_Subscriber_get_status_changes (sub);
	fail_unless (sm == 0);
	sm = DDS_Entity_get_status_changes (sub);
	fail_unless (sm == 0);
	/*dbg_printf ("(mask=%u)", sm);*/
	h = DDS_Subscriber_get_instance_handle (sub);
	fail_unless (h != 0);
	h2 = DDS_Entity_get_instance_handle (sub);
	fail_unless (h == h2);
	np = DDS_Subscriber_get_participant (sub);
	fail_unless (np == p);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);

	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);
	dr0 = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr0 != NULL);

	dr = DDS_Subscriber_lookup_datareader (sub, "HelloWorld");
	fail_unless (dr == dr0);

	dr1 = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr1 != NULL);

	delay ();

	r = DDS_Subscriber_delete_contained_entities (sub);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test default child QoS of subscriber entities\r\n");
	r = DDS_Subscriber_get_default_datareader_qos (sub, &drq);
	fail_unless (r == DDS_RETCODE_OK);

	drq.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
	r = DDS_Subscriber_set_default_datareader_qos (sub, &drq);
	fail_unless (r == DDS_RETCODE_OK);

	dr1 = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr1 != NULL);

	delay ();

	v_printf (" - Test coherency (not implemented).\r\n");
	r = DDS_Subscriber_begin_access (sub);
	fail_unless (r == DDS_RETCODE_UNSUPPORTED);

	r = DDS_Subscriber_end_access (sub);
	fail_unless (r == DDS_RETCODE_UNSUPPORTED);

	r = DDS_Subscriber_copy_from_topic_qos (sub, &drq, NULL);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_Subscriber_notify_datareaders (sub);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_PRECONDITION_NOT_MET);
	r = DDS_Subscriber_delete_datareader (sub, dr1);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DomainParticipant_delete_topic (p, t);	
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);

	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_subscriber (void)
{
	dbg_printf ("Subscriber ");
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



