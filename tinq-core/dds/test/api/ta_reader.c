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
#include "ta_reader.h"

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
	DDS_DataReaderQos		rq, refrq;
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

	sub = DDS_DomainParticipant_create_subscriber (p, NULL, NULL, 0);
	fail_unless (sub != NULL);

	DDS_DataReaderQos__init (&refrq);
	err = dds_seq_from_array (&refrq.user_data, data, sizeof (data));
	fail_unless (err == 0);

	v_printf (" - Create datareader with specific QoS parameters.\r\n");
	dr = DDS_Subscriber_create_datareader (sub, td, &refrq, NULL, 0);
	fail_unless (dr != NULL);

	memset (&rq, 0, sizeof (rq));
	r = DDS_DataReader_get_qos (dr, &rq);
	n = dds_seq_to_array (&rq.user_data, d2, sizeof (d2));
	fail_unless (r == DDS_RETCODE_OK &&
		     DDS_SEQ_LENGTH (refrq.user_data.value) == sizeof (data));
	n = dds_seq_to_array (&rq.user_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_DataReaderQos__clear (&rq);
	r = DDS_Subscriber_delete_datareader (sub, dr);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Create datareader with default QoS parameters.\r\n");
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);
	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);

	v_printf (" - Update datareader QoS parameters.\r\n");
	r = DDS_DataReader_get_qos (dr, &rq);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_DataReaderQos__clear (&refrq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&rq, &refrq, sizeof (refrq)));
	err = dds_seq_from_array (&refrq.user_data, data, sizeof (data));
	fail_unless (err == 0);

	r = DDS_DataReader_set_qos (dr, &refrq);
	fail_unless (r == DDS_RETCODE_OK);

	delay ();
	r = DDS_DataReader_get_qos (dr, &rq);
	fail_unless (r == DDS_RETCODE_OK &&
		     DDS_SEQ_LENGTH (rq.user_data.value) == sizeof (data));
	n = dds_seq_to_array (&rq.user_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));

	DDS_DataReaderQos__clear (&refrq);
	DDS_DataReaderQos__clear (&rq);

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

static void subscription_matched (
	DDS_DataReaderListener *self,
	DDS_DataReader reader, /* in (variable length) */
	DDS_SubscriptionMatchedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (status)
}

static void sample_rejected (
	DDS_DataReaderListener *self,
	DDS_DataReader reader, /* in (variable length) */
	DDS_SampleRejectedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (status)
}

static void data_available (
	DDS_DataReaderListener *self,
	DDS_DataReader reader)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
}

static void liveliness_changed (
	DDS_DataReaderListener *self,
	DDS_DataReader reader, /* in (variable length) */
	DDS_LivelinessChangedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (status)
}

static void requested_inc_qos (
	DDS_DataReaderListener *self,
	DDS_DataReader reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (status)
}

static void sample_lost (
	DDS_DataReaderListener *self,
	DDS_DataReader reader, /* in (variable length) */
	DDS_SampleLostStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (status)
}

static void requested_deadline_missed (
	DDS_DataReaderListener *self,
	DDS_DataReader reader, /* in (variable length) */
	DDS_RequestedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (status)
}

static DDS_DataReaderListener r_listener = {
	sample_rejected,
	liveliness_changed,
	requested_deadline_missed,
	requested_inc_qos,
	data_available,
	subscription_matched,
	sample_lost,
	(void *) 0x44556677
};

static void test_listener (void)
{
	DDS_DomainParticipant		p;
	DDS_Topic			t;
	DDS_TopicDescription		td;
	DDS_Subscriber			sub;
	DDS_DataReader			dr;
	DDS_DataReaderListener		*lp;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);

	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);

	v_printf (" - Test reader listener.\r\n");
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);

	lp = DDS_DataReader_get_listener (dr);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == NULL &&
		     lp->on_liveliness_changed == NULL &&
		     lp->on_requested_deadline_missed == NULL &&
		     lp->on_requested_incompatible_qos == NULL &&
		     lp->on_data_available == NULL &&
		     lp->on_subscription_matched == NULL &&
		     lp->on_sample_lost == NULL);

	v_printf (" - Test specific reader listener updates.\r\n");
	r = DDS_DataReader_set_listener (dr, &r_listener,
			DDS_SAMPLE_REJECTED_STATUS |
			DDS_LIVELINESS_CHANGED_STATUS |
			DDS_REQUESTED_DEADLINE_MISSED_STATUS |
			DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS |
			DDS_DATA_AVAILABLE_STATUS |
			DDS_SUBSCRIPTION_MATCHED_STATUS |
			DDS_SAMPLE_LOST_STATUS);
	fail_unless (r == DDS_RETCODE_OK);

	lp = DDS_DataReader_get_listener (dr);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == sample_rejected &&
		     lp->on_liveliness_changed == liveliness_changed &&
		     lp->on_requested_deadline_missed == requested_deadline_missed &&
		     lp->on_requested_incompatible_qos == requested_inc_qos &&
		     lp->on_data_available == data_available &&
		     lp->on_subscription_matched == subscription_matched &&
		     lp->on_sample_lost == sample_lost &&
		     lp->cookie == (void *) 0x44556677);

	v_printf (" - Test default reader listener update.\r\n");
	r = DDS_DataReader_set_listener (dr, NULL, DDS_REQUESTED_DEADLINE_MISSED_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_DataReader_get_listener (dr);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == NULL &&
		     lp->on_liveliness_changed == NULL &&
		     lp->on_requested_deadline_missed == NULL &&
		     lp->on_requested_incompatible_qos == NULL &&
		     lp->on_data_available == NULL &&
		     lp->on_subscription_matched == NULL &&
		     lp->on_sample_lost == NULL);
	r = DDS_Subscriber_delete_datareader (sub, dr);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
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
	DDS_TopicDescription		td;
	DDS_Subscriber			sub, nsub;
	DDS_DataReader			dr;
	DDS_StatusCondition		sc, sc2;
	DDS_StatusMask 			sm;
	DDS_InstanceHandle_t		h, h2;
	DDS_Duration_t			w;
	DDS_LivelinessChangedStatus	lcs;
	DDS_RequestedDeadlineMissedStatus dms;
	DDS_RequestedIncompatibleQosStatus iqs;
	DDS_SampleLostStatus		sls;
	DDS_SampleRejectedStatus	srs;
	DDS_SubscriptionMatchedStatus	sms;
	DDS_InstanceHandleSeq		handles;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);

	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);

	v_printf (" - Test reader auxiliary functions.\r\n");
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);

	sc = DDS_DataReader_get_statuscondition (dr);
	fail_unless (sc != NULL);
	
	sc2 = DDS_Entity_get_statuscondition (dr);
	fail_unless (sc2 == sc);
	
	sm = DDS_DataReader_get_status_changes (dr);
	fail_unless (sm == 0);

	sm = DDS_Entity_get_status_changes (dr);
	fail_unless (sm == 0);

	h = DDS_Entity_get_instance_handle (dr);
	fail_unless (h != 0);

	h2 = DDS_DataReader_get_instance_handle (dr);
	fail_unless (h == h2);

	r = DDS_DataReader_get_liveliness_changed_status (dr, &lcs);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataReader_get_requested_deadline_missed_status (dr, &dms);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataReader_get_requested_incompatible_qos_status (dr, &iqs);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataReader_get_sample_lost_status (dr, &sls);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataReader_get_sample_rejected_status (dr, &srs);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DataReader_get_subscription_matched_status (dr, &sms);
	fail_unless (r == DDS_RETCODE_OK);

	td = DDS_DataReader_get_topicdescription (dr);
	fail_unless ((DDS_Topic) td == t);

	nsub = DDS_DataReader_get_subscriber (dr);
	fail_unless (nsub == sub);

	w.sec = 2;
	w.nanosec = 0;
	r = DDS_DataReader_wait_for_historical_data (dr, &w);
	fail_unless (r == DDS_RETCODE_OK);

	DDS_InstanceHandleSeq__init (&handles);
	r = DDS_DataReader_get_matched_publications (dr, &handles);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_InstanceHandleSeq__clear (&handles);

	r = DDS_Subscriber_delete_datareader (sub, dr);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_reader (void)
{
	dbg_printf ("Reader ");
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


