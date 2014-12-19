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

#include <arpa/inet.h>
#include "test.h"
#include "ta_data.h"
#include "ta_part.h"

static void test_qos (void)
{
	DDS_DomainParticipant	 part;
	DDS_DomainParticipantQos qos;
	DDS_ReturnCode_t	 error;
	DDS_InstanceHandle_t	 h;
	char			 buf [] = "Folks hello!";
	char			 buf2 [] = "Hello folkskes!";

	part = DDS_DomainParticipantFactory_create_participant (
						22, NULL, NULL, 0);
	fail_unless (part != NULL);
	v_printf ("\r\n - Participant created with default QoS!\r\n");
	delay ();

	DDS_SEQ_INIT (qos.user_data.value);
	error = dds_seq_from_array (&qos.user_data.value, buf, sizeof (buf));
	fail_unless (error == DDS_RETCODE_OK);
	qos.entity_factory.autoenable_created_entities = 0;
	error = DDS_DomainParticipant_set_qos (part, &qos);
	fail_unless (error == DDS_RETCODE_OK);
	error = dds_seq_from_array (&qos.user_data.value, buf2, sizeof (buf2));
	fail_unless (error == DDS_RETCODE_OK);
	qos.entity_factory.autoenable_created_entities = 1;
	error = DDS_DomainParticipant_set_qos (part, &qos);
	fail_unless (error == DDS_RETCODE_OK);
	dds_seq_cleanup (&qos.user_data.value);
	error = DDS_DomainParticipant_get_qos (part, &qos);
	fail_unless (error == DDS_RETCODE_OK &&
		     !memcmp (qos.user_data.value._buffer, buf2, sizeof (buf2)) &&
		     qos.user_data.value._length == sizeof (buf2) &&
		     qos.entity_factory.autoenable_created_entities == 1);
	DDS_DomainParticipantQos__clear (&qos);

	/*printf ("Updated QoS\r\n");*/
	h = DDS_DomainParticipant_get_instance_handle (part);
	fail_unless (h == 1);

	h = DDS_Entity_get_instance_handle (part);
	fail_unless (h == 1);

	h = DDS_Entity_get_instance_handle (NULL);
	fail_unless (h == 0);

	v_printf (" - Participant qos updated: user data should now be 'Folks hello!'!\r\n");
	delay ();

	/*printf ("Deleting\r\n");*/
	error = DDS_DomainParticipantFactory_delete_participant (part);
	fail_unless (error == DDS_RETCODE_OK);
}

static int delta_t_ms (const DDS_Time_t *t1, const DDS_Time_t *t2)
{
	int		s;
	unsigned	ns;

	if (t1->sec == t2->sec)
		return ((t2->nanosec - t1->nanosec) / 1000000);
	else if (t1->sec < t2->sec) {
		s = t2->sec;
		ns = t2->nanosec;
		if (ns < t1->nanosec) {
			s--;
			ns += 1000000000;
		}
		return ((s - t1->sec) * 1000 + (ns - t1->nanosec) / 1000000);
	}
	else
		return (-1);
}

static void test_enable (void)
{
	DDS_DomainParticipant		part;
	DDS_DomainParticipantFactoryQos	qos;
	DDS_Subscriber			sub;
	DDS_DomainId_t			id;
	DDS_ReturnCode_t		r;
	DDS_Time_t			t1, t2;
	int				dt;
	int				b;

	qos.entity_factory.autoenable_created_entities = 0;
	r = DDS_DomainParticipantFactory_set_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);
	part = DDS_DomainParticipantFactory_create_participant (17, NULL, NULL, 0);
	fail_unless (part != NULL);
	r = DDS_DomainParticipant_assert_liveliness (part);
	fail_unless (r == DDS_RETCODE_NOT_ENABLED);
	v_printf (" - Participant created but not enabled - no notification expected!\r\n");
	delay ();

	r = DDS_Entity_enable (part);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DomainParticipant_enable (part);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DomainParticipant_assert_liveliness (part);
	fail_unless (r == DDS_RETCODE_OK);
	sub = DDS_DomainParticipant_get_builtin_subscriber (part);
	fail_unless (sub != NULL);
	id = DDS_DomainParticipant_get_domain_id (part);
	fail_unless (id == 17);
	b = DDS_DomainParticipant_contains_entity (part, 
				DDS_Subscriber_get_instance_handle (sub));
	fail_unless (b == 1);
	b = DDS_DomainParticipant_contains_entity (part, 997);
	fail_unless (b == 0);
	r = DDS_DomainParticipant_get_current_time (part, &t1);
	fail_unless (r == DDS_RETCODE_OK);
	v_printf (" - Participant enabled - notification expected now!\r\n");
	sleep (1);

	r = DDS_DomainParticipant_get_current_time (part, &t2);
	/*printf ("t2=%d.%09us\n", t2.sec, t2.nanosec);*/
	fail_unless (r == DDS_RETCODE_OK &&
		     (dt = delta_t_ms (&t1, &t2)) > 900 && dt < 1100);
	/*printf ("dt=%ums\n", dt);*/
	r = DDS_DomainParticipant_delete_contained_entities (part);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipantFactory_delete_participant (part);
	fail_unless (r == DDS_RETCODE_OK);

	qos.entity_factory.autoenable_created_entities = 1;
	r = DDS_DomainParticipantFactory_set_qos (&qos);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_dqos (void)
{
	DDS_DomainParticipant	part;
	DDS_PublisherQos	ppq, refpq;
	DDS_SubscriberQos	psq, refsq;
	DDS_TopicQos		ptq, reftq;
	DDS_ReturnCode_t	r;
	int			err;
	size_t			i;
	static char		*buf [] = { "Hello", "World" };
	static unsigned char	data [] = { 0x00, 0x11, 0x22, 0x33, 0x44 };
	unsigned char		d2 [sizeof (data) + 1];
	unsigned		n;

	/* Create Participant. */
	part = DDS_DomainParticipantFactory_create_participant (17, NULL, NULL, 0);
	fail_unless (part != NULL);

	/* Test default Publisher QoS. */
	v_printf (" - Test default Topic/Publisher/Subscriber QoS.\r\n");
	r = DDS_DomainParticipant_get_default_publisher_qos (part, &ppq);
	DDS_PublisherQos__init (&refpq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&ppq, &refpq, sizeof (DDS_PublisherQos)));
	for (i = 0; (size_t) i < sizeof (buf) / sizeof (char *); i++) {
		err = dds_seq_append (&refpq.partition.name, &buf [i]);
		fail_unless (err == 0);
	}
	err = dds_seq_from_array (&refpq.group_data.value, data, sizeof (data));
	fail_unless (err == 0);
	r = DDS_DomainParticipant_set_default_publisher_qos (part, &refpq);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_get_default_publisher_qos (part, &ppq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (refpq.partition.name) == DDS_SEQ_LENGTH (ppq.partition.name) &&
		     DDS_SEQ_LENGTH (refpq.group_data.value) == sizeof (data));
	DDS_SEQ_FOREACH (refpq.partition.name, i)
		fail_unless (!strcmp (DDS_SEQ_ITEM (ppq.partition.name, i),
				      DDS_SEQ_ITEM (refpq.partition.name, i)));
	n = dds_seq_to_array (&ppq.group_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_PublisherQos__clear (&refpq);

	/* Test default Subscriber QoS. */
	r = DDS_DomainParticipant_get_default_subscriber_qos (part, &psq);
	DDS_SubscriberQos__init (&refsq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&psq, &refsq, sizeof (DDS_SubscriberQos)));
	for (i = 0; (size_t) i < sizeof (buf) / sizeof (char *); i++) {
		err = dds_seq_append (&refsq.partition.name, &buf [i]);
		fail_unless (err == 0);
	}
	err = dds_seq_from_array (&refsq.group_data, data, sizeof (data));
	fail_unless (err == 0);
	r = DDS_DomainParticipant_set_default_subscriber_qos (part, &refsq);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_get_default_subscriber_qos (part, &psq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (refsq.partition.name) == DDS_SEQ_LENGTH (psq.partition.name) &&
		     DDS_SEQ_LENGTH (refsq.group_data.value) == sizeof (data));
	DDS_SEQ_FOREACH (refsq.partition.name, i)
		fail_unless (!strcmp (DDS_SEQ_ITEM (psq.partition.name, i),
				      DDS_SEQ_ITEM (refsq.partition.name, i)));
	n = dds_seq_to_array (&psq.group_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_SubscriberQos__clear (&refsq);

	/* Test default Topic QoS. */
	r = DDS_DomainParticipant_get_default_topic_qos (part, &ptq);
	DDS_TopicQos__init (&reftq);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&ptq, &reftq, sizeof (DDS_TopicQos)));
	err = dds_seq_from_array (&reftq.topic_data, data, sizeof (data));
	fail_unless (err == 0);
	r = DDS_DomainParticipant_set_default_topic_qos (part, &reftq);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_get_default_topic_qos (part, &ptq);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (reftq.topic_data.value) == sizeof (data));
	n = dds_seq_to_array (&ptq.topic_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_TopicQos__clear (&reftq);

	/* Delete Participant. */
	r = DDS_DomainParticipantFactory_delete_participant (part);
	fail_unless (r == DDS_RETCODE_OK);
}

static void p_sample_rejected (
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleRejectedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void p_requested_inc_qos (
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static void p_offered_inc_qos (
	DDS_DomainParticipantListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (status)
}

static void p_sample_lost (
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleLostStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (the_reader)
	ARG_NOT_USED (status)
}

static DDS_DomainParticipantListener participant_listener = {
	p_sample_rejected,
	p_requested_inc_qos,
	p_offered_inc_qos,
	p_sample_lost,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static void test_listener (void)
{
	DDS_DomainParticipant		p;
	DDS_DomainParticipantListener	*lp;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	v_printf (" - Test Participant Listeners.\r\n");
	lp = DDS_DomainParticipant_get_listener (p);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == NULL &&
		     lp->on_requested_incompatible_qos == NULL &&
		     lp->on_offered_incompatible_qos == NULL &&
		     lp->on_sample_lost == NULL);
	fail_unless (lp->on_offered_deadline_missed == NULL &&
		     lp->on_subscription_matched == NULL &&
		     lp->on_publication_matched == NULL &&
		     lp->on_data_on_readers == NULL &&
		     lp->on_liveliness_changed == NULL &&
		     lp->on_data_available == NULL &&
		     lp->on_liveliness_lost == NULL &&
		     lp->on_inconsistent_topic == NULL &&
		     lp->on_requested_deadline_missed == NULL);
	r = DDS_DomainParticipant_set_listener (p, &participant_listener,
				DDS_INCONSISTENT_TOPIC_STATUS | 
				DDS_OFFERED_DEADLINE_MISSED_STATUS |
				DDS_REQUESTED_DEADLINE_MISSED_STATUS |
				DDS_OFFERED_INCOMPATIBLE_QOS_STATUS |
				DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS |
				DDS_SAMPLE_LOST_STATUS |
				DDS_SAMPLE_REJECTED_STATUS |
				DDS_DATA_ON_READERS_STATUS |
				DDS_DATA_AVAILABLE_STATUS |
				DDS_LIVELINESS_LOST_STATUS |
				DDS_LIVELINESS_CHANGED_STATUS |
				DDS_PUBLICATION_MATCHED_STATUS |
				DDS_SUBSCRIPTION_MATCHED_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_DomainParticipant_get_listener (p);
	fail_unless (lp != NULL &&
		     lp->on_sample_rejected == p_sample_rejected &&
		     lp->on_requested_incompatible_qos == p_requested_inc_qos &&
		     lp->on_offered_incompatible_qos == p_offered_inc_qos &&
		     lp->on_sample_lost == p_sample_lost);
	r = DDS_DomainParticipant_set_listener (p, NULL, DDS_INCONSISTENT_TOPIC_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_DomainParticipant_get_listener (p);
	fail_unless (lp != NULL && lp->on_sample_rejected == NULL);

	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_aux (void)
{
	DDS_DomainParticipant		p;
	DDS_StatusCondition		sc, sc2;
	DDS_StatusMask 			sm;
	DDS_ReturnCode_t		r;
	unsigned long			fh = 7 << 13;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	v_printf (" - Test extra Participant functions.\r\n");
	sc = DDS_DomainParticipant_get_statuscondition (p);
	fail_unless (sc != NULL);

	sc2 = DDS_Entity_get_statuscondition (p);
	fail_unless (sc != NULL && sc == sc2);

	sm = DDS_DomainParticipant_get_status_changes (p);
	fail_unless (sm == 0);

	sm = DDS_Entity_get_status_changes (p);
	fail_unless (sm == 0);

	sm = DDS_Entity_get_status_changes (NULL);
	fail_unless (sm == 0);

	sm = DDS_Entity_get_status_changes (&fh);
	fail_unless (sm == 0);

	r = DDS_Entity_enable (&fh);
	fail_unless (r == DDS_RETCODE_BAD_PARAMETER);

	sc = DDS_Entity_get_statuscondition (&fh);
	fail_unless (sc == 0);

	fh |= 8;
	sm = DDS_Entity_get_status_changes (&fh);
	fail_unless (sm == 0);

	r = DDS_Entity_enable (&fh);
	fail_unless (r == DDS_RETCODE_BAD_PARAMETER);

	sc = DDS_Entity_get_statuscondition (&fh);
	fail_unless (sc == 0);


	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static int seq_default (void *seq, size_t s)
{
	DDS_OctetSeq	*osp = (DDS_OctetSeq *) seq;

	if (osp->_maximum != 0 ||
	    osp->_length != 0 ||
	    osp->_esize != s ||
	    !osp->_own ||
	    osp->_buffer)
		return (0);

	return (1);
}

static void test_xqos (void)
{
	DDS_DomainParticipant	p;
	DDS_UserDataQosPolicy	*udp;
	DDS_GroupDataQosPolicy	*gdp;
	DDS_TopicDataQosPolicy	*tdp;
	DDS_PartitionQosPolicy	*pdp;
	DDS_DomainParticipantQos *dp;
	DDS_TopicQos		*tp;
	DDS_SubscriberQos	*sp;
	DDS_PublisherQos	*pp;
	DDS_DataReaderQos	*rp;
	DDS_DataWriterQos	*wp;
	DDS_ReturnCode_t	r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	v_printf (" - Test QoS support: UserData.\r\n");
	udp = DDS_UserDataQosPolicy__alloc ();
	assert (udp != NULL && seq_default (&udp->value, 1));
	DDS_UserDataQosPolicy__init (udp);
	assert (seq_default (&udp->value, 1));
	DDS_UserDataQosPolicy__clear (udp);
	assert (seq_default (&udp->value, 1));
	DDS_UserDataQosPolicy__free (udp);

	v_printf (" - Test QoS support: GroupData.\r\n");
	gdp = DDS_GroupDataQosPolicy__alloc ();
	assert (gdp != NULL && seq_default (&gdp->value, 1));
	DDS_GroupDataQosPolicy__init (gdp);
	assert (seq_default (&gdp->value, 1));
	DDS_GroupDataQosPolicy__clear (gdp);
	assert (seq_default (&gdp->value, 1));
	DDS_GroupDataQosPolicy__free (gdp);

	v_printf (" - Test QoS support: TopicData.\r\n");
	tdp = DDS_TopicDataQosPolicy__alloc ();
	assert (tdp != NULL && seq_default (&tdp->value, 1));
	DDS_TopicDataQosPolicy__init (tdp);
	assert (seq_default (&tdp->value, 1));
	DDS_TopicDataQosPolicy__clear (tdp);
	assert (seq_default (&tdp->value, 1));
	DDS_TopicDataQosPolicy__free (tdp);

	v_printf (" - Test QoS support: Partition.\r\n");
	pdp = DDS_PartitionQosPolicy__alloc ();
	assert (pdp != NULL && seq_default (&pdp->name, sizeof (char *)));
	DDS_PartitionQosPolicy__init (pdp);
	assert (seq_default (&pdp->name, sizeof (char *)));
	DDS_PartitionQosPolicy__clear (pdp);
	assert (seq_default (&pdp->name, sizeof (char *)));
	DDS_PartitionQosPolicy__free (pdp);

	v_printf (" - Test QoS support: DomainParticipant.\r\n");
	dp = DDS_DomainParticipantQos__alloc ();
	assert (dp != NULL && seq_default (&dp->user_data.value, 1));
	DDS_DomainParticipantQos__init (dp);
	assert (seq_default (&dp->user_data.value, 1));
	DDS_DomainParticipantQos__clear (dp);
	assert (seq_default (&dp->user_data.value, 1));
	DDS_DomainParticipantQos__free (dp);

	v_printf (" - Test QoS support: Topic.\r\n");
	tp = DDS_TopicQos__alloc ();
	assert (tp != NULL && seq_default (&tp->topic_data.value, 1));
	DDS_TopicQos__init (tp);
	assert (seq_default (&tp->topic_data.value, 1));
	DDS_TopicQos__clear (tp);
	assert (seq_default (&tp->topic_data.value, 1));
	DDS_TopicQos__free (tp);

	v_printf (" - Test QoS support: Subscriber.\r\n");
	sp = DDS_SubscriberQos__alloc ();
	assert (sp != NULL &&
		seq_default (&sp->group_data.value, 1) &&
		seq_default (&sp->partition.name, sizeof (char *)));
	DDS_SubscriberQos__init (sp);
	assert (seq_default (&sp->group_data.value, 1) &&
		seq_default (&sp->partition.name, sizeof (char *)));
	DDS_SubscriberQos__clear (sp);
	assert (seq_default (&sp->group_data.value, 1) &&
		seq_default (&sp->partition.name, sizeof (char *)));
	DDS_SubscriberQos__free (sp);

	v_printf (" - Test QoS support: Publisher.\r\n");
	pp = DDS_PublisherQos__alloc ();
	assert (pp != NULL &&
		seq_default (&pp->group_data.value, 1) &&
		seq_default (&pp->partition.name, sizeof (char *)));
	DDS_PublisherQos__init (pp);
	assert (seq_default (&pp->group_data.value, 1) &&
		seq_default (&pp->partition.name, sizeof (char *)));
	DDS_PublisherQos__clear (pp);
	assert (seq_default (&pp->group_data.value, 1) &&
		seq_default (&pp->partition.name, sizeof (char *)));
	DDS_PublisherQos__free (pp);

	v_printf (" - Test QoS support: DataReader.\r\n");
	rp = DDS_DataReaderQos__alloc ();
	assert (rp != NULL && seq_default (&rp->user_data.value, 1));
	DDS_DataReaderQos__init (rp);
	assert (seq_default (&rp->user_data.value, 1));
	DDS_DataReaderQos__clear (rp);
	assert (seq_default (&rp->user_data.value, 1));
	DDS_DataReaderQos__free (rp);

	v_printf (" - Test QoS support: DataWriter.\r\n");
	wp = DDS_DataWriterQos__alloc ();
	assert (wp != NULL && seq_default (&wp->user_data.value, 1));
	DDS_DataWriterQos__init (wp);
	assert (seq_default (&wp->user_data.value, 1));
	DDS_DataWriterQos__clear (wp);
	assert (seq_default (&wp->user_data.value, 1));
	DDS_DataWriterQos__free (wp);

	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_builtin (void)
{
	DDS_DomainParticipant	p;
	DDS_Subscriber		sub;
	DDS_DataReader		dr;
	DDS_ReturnCode_t	r;
	unsigned		i;
	static const char	*names [] = {
		"DCPSParticipant",
		"DCPSTopic",
		"DCPSPublication",
		"DCPSSubscription"
	};

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	v_printf (" - Test builtin topics.\r\n");
	sub = DDS_DomainParticipant_get_builtin_subscriber (p);
	fail_unless (sub != NULL);

	for (i = 0; i < sizeof (names) / sizeof (char *); i++) {
		dr = DDS_Subscriber_lookup_datareader (sub, names [i]);
		fail_unless (dr != NULL);
	}
	delay ();

	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_disc (void)
{
	DDS_DomainParticipant	p;
	DDS_ReturnCode_t	r;
	DDS_InstanceHandleSeq	*handles;
	DDS_InstanceHandle_t	*hp;
	DDS_ParticipantBuiltinTopicData *pdata;
	DDS_TopicBuiltinTopicData *tdata;
	unsigned		i;

	v_printf (" - Test Discovery access functions.\r\n");
	p = data_prologue ();
	data_xtopic_add ();
	handles = DDS_InstanceHandleSeq__alloc ();
	fail_unless (handles != NULL);
	r = DDS_DomainParticipant_get_discovered_participants (p, handles);
	fail_unless (r == DDS_RETCODE_OK);
	pdata = DDS_ParticipantBuiltinTopicData__alloc ();
	fail_unless (pdata != NULL);
	DDS_SEQ_FOREACH_ENTRY (*handles, i, hp) {
		r = DDS_DomainParticipant_get_discovered_participant_data (p, pdata, *hp);
		fail_unless (r == DDS_RETCODE_OK);
		v_printf (" - Discovered participant: %08x:%08x:%08x {%u}\r\n",
			ntohl (pdata->key.value [0]),
			ntohl (pdata->key.value [1]),
			ntohl (pdata->key.value [2]),
			*hp);
		DDS_ParticipantBuiltinTopicData__clear (pdata);
	}
	DDS_ParticipantBuiltinTopicData__free (pdata);
	DDS_InstanceHandleSeq__clear (handles);
	r = DDS_DomainParticipant_get_discovered_topics (p, handles);
	fail_unless (r == DDS_RETCODE_OK);
	tdata = DDS_TopicBuiltinTopicData__alloc ();
	fail_unless (tdata != NULL);
	DDS_SEQ_FOREACH_ENTRY (*handles, i, hp) {
		r = DDS_DomainParticipant_get_discovered_topic_data (p, tdata, *hp);
		fail_unless (r == DDS_RETCODE_OK);
		v_printf (" - Discovered topic: %08x:%08x:%08x {%u} - %s/%s\r\n",
			ntohl (tdata->key.value [0]),
			ntohl (tdata->key.value [1]),
			ntohl (tdata->key.value [2]),
			*hp,
			tdata->name,
			tdata->type_name);
		DDS_TopicBuiltinTopicData__clear (tdata);
	}
	DDS_TopicBuiltinTopicData__free (tdata);
	DDS_InstanceHandleSeq__free (handles);
	data_xtopic_remove ();
	data_epilogue ();
}

void test_participant (void)
{
	dbg_printf ("Participant ");
	if (trace)
		fflush (stdout);
	test_qos ();
	test_enable ();
	test_dqos ();
	test_listener ();
	test_aux ();
	test_xqos ();
	test_builtin ();
	test_disc ();
	dbg_printf (" - success!\r\n");
}

