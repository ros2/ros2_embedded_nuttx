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
#include "ta_writer.h"

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
	DDS_DataWriterQos		wq, refwq;
	DDS_ReturnCode_t		r;
	static unsigned char		data [] = { 0x77, 0x88, 0xaa, 0xbb, 0xcc, 0xdd };
	unsigned char			d2 [sizeof (data) + 1];
	unsigned			n;
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

	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);

	pub = DDS_DomainParticipant_create_publisher (p, NULL, NULL, 0);
	fail_unless (pub != NULL);

	DDS_DataWriterQos__init (&refwq);
	err = dds_seq_from_array (&refwq.user_data, data, sizeof (data));
	fail_unless (err == 0);

	v_printf (" - Create datawriter with specific QoS parameters.\r\n");
	dw = DDS_Publisher_create_datawriter (pub, t, &refwq, NULL, 0);
	fail_unless (dw != NULL);

	memset (&wq, 0, sizeof (wq));
	r = DDS_DataWriter_get_qos (dw, &wq);
	n = dds_seq_to_array (&wq.user_data, d2, sizeof (d2));
	fail_unless (r == DDS_RETCODE_OK &&
		     DDS_SEQ_LENGTH (refwq.user_data.value) == sizeof (data));
	n = dds_seq_to_array (&wq.user_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_DataWriterQos__clear (&wq);
	r = DDS_Publisher_delete_datawriter (pub, dw);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Create datawriter with default QoS parameters.\r\n");
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);
	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);

	v_printf (" - Update datawriter QoS parameters.\r\n");
	r = DDS_DataWriter_get_qos (dw, &wq);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_DataWriterQos__clear (&refwq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&wq, &refwq, sizeof (refwq)));
	err = dds_seq_from_array (&refwq.user_data, data, sizeof (data));
	fail_unless (err == 0);

	r = DDS_DataWriter_set_qos (dw, &refwq);
	fail_unless (r == DDS_RETCODE_OK);

	delay ();
	r = DDS_DataWriter_get_qos (dw, &wq);
	fail_unless (r == DDS_RETCODE_OK &&
		     DDS_SEQ_LENGTH (wq.user_data.value) == sizeof (data));
	n = dds_seq_to_array (&wq.user_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_DataWriterQos__clear (&refwq);
	DDS_DataWriterQos__clear (&wq);

	delay ();
	if (!enabled) {
		v_printf (" - Enable all entities.\r\n");
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

static void offered_deadline_missed (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void publication_matched (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_PublicationMatchedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void liveliness_lost (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_LivelinessLostStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void offered_inc_qos (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static DDS_DataWriterListener w_listener = {
	offered_deadline_missed,
	publication_matched,
	liveliness_lost,
	offered_inc_qos,
	(void *) 0x1234fedc
};

static void test_listener (void)
{
	DDS_DomainParticipant		p;
	DDS_Topic			t;
	DDS_Publisher			pub;
	DDS_DataWriter			dw;
	DDS_DataWriterListener		*lp;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);

	v_printf (" - Test writer listener.\r\n");
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);

	lp = DDS_DataWriter_get_listener (dw);
	fail_unless (lp != NULL &&
		     lp->on_offered_deadline_missed == NULL && 
		     lp->on_publication_matched == NULL &&
		     lp->on_liveliness_lost == NULL &&
		     lp->on_offered_incompatible_qos == NULL);

	v_printf (" - Test specific writer listener updates.\r\n");
	r = DDS_DataWriter_set_listener (dw, &w_listener,
			DDS_OFFERED_DEADLINE_MISSED_STATUS |
			DDS_PUBLICATION_MATCHED_STATUS |
			DDS_LIVELINESS_LOST_STATUS |
			DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
	fail_unless (r == DDS_RETCODE_OK);

	lp = DDS_DataWriter_get_listener (dw);
	fail_unless (lp != NULL &&
		     lp->on_offered_deadline_missed == offered_deadline_missed &&
		     lp->on_publication_matched == publication_matched &&
		     lp->on_liveliness_lost == liveliness_lost &&
		     lp->on_offered_incompatible_qos == offered_inc_qos &&
		     lp->cookie == (void *) 0x1234fedc);

	v_printf (" - Test default writer listener update.\r\n");
	r = DDS_DataWriter_set_listener (dw, NULL, DDS_OFFERED_DEADLINE_MISSED_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_DataWriter_get_listener (dw);
	fail_unless (lp != NULL &&
		     lp->on_offered_deadline_missed == NULL &&
		     lp->on_publication_matched == NULL &&
		     lp->on_liveliness_lost == NULL &&
		     lp->on_offered_incompatible_qos == NULL);
	r = DDS_Publisher_delete_datawriter (pub, dw);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_aux (void)
{
	DDS_DomainParticipant		p;
	DDS_Topic			t;
	DDS_Publisher			pub, npub;
	DDS_DataWriter			dw;
	DDS_StatusCondition		sc, sc2;
	DDS_StatusMask 			sm;
	DDS_InstanceHandle_t		h, h2;
	DDS_Duration_t			w;
	DDS_LivelinessLostStatus	lls;
	DDS_OfferedDeadlineMissedStatus	dms;
	DDS_OfferedIncompatibleQosStatus iqs;
	DDS_PublicationMatchedStatus	pms;
	DDS_InstanceHandleSeq		handles;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);

	v_printf (" - Test writer auxiliary functions.\r\n");
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);

	sc = DDS_DataWriter_get_statuscondition (dw);
	fail_unless (sc != NULL);
	sc2 = DDS_Entity_get_statuscondition (dw);
	fail_unless (sc2 == sc);
	
	sm = DDS_DataWriter_get_status_changes (dw);
	fail_unless (sm == 0);
	sm = DDS_Entity_get_status_changes (dw);
	fail_unless (sm == 0);

	h = DDS_DataWriter_get_instance_handle (dw);
	fail_unless (h != 0);
	h2 = DDS_Entity_get_instance_handle (dw);
	fail_unless (h2 == h);

	w.sec = 1;
	w.nanosec = 0;
	r = DDS_DataWriter_wait_for_acknowledgments (dw, &w);
	fail_unless (r == DDS_RETCODE_OK);
	
	r = DDS_DataWriter_get_liveliness_lost_status (dw, &lls);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataWriter_get_offered_deadline_missed_status (dw, &dms);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataWriter_get_offered_incompatible_qos_status (dw, &iqs);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataWriter_get_publication_matched_status (dw, &pms);
	fail_unless (r == DDS_RETCODE_OK);
	
	npub = DDS_DataWriter_get_publisher (dw);
	fail_unless (npub == pub);

	r = DDS_DataWriter_assert_liveliness (dw);
	fail_unless (r == DDS_RETCODE_OK);

	DDS_InstanceHandleSeq__init (&handles);
	r = DDS_DataWriter_get_matched_subscriptions (dw, &handles);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_InstanceHandleSeq__clear (&handles);

	r = DDS_Publisher_delete_datawriter (pub, dw);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_writer (void)
{
	dbg_printf ("Writer ");
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

