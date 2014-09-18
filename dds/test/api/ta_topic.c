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
#include "ta_topic.h"

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
	DDS_TopicQos			tqos, reftqos;
	DDS_ReturnCode_t		r;
	static unsigned char		data [] = { 0x55, 0x44, 0x22, 0x33, 0x44, 0x77 };
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

	v_printf (" - Test topic create ('hello').\r\n");
	DDS_TopicQos__init (&reftqos);
	reftqos.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
	t = DDS_DomainParticipant_create_topic (p, "hello", TYPE_NAME, &reftqos, NULL, 0);
	fail_unless (t != NULL);
	v_printf (" - Test default topic QoS is correct.\r\n");
	r = DDS_Topic_get_qos (t, &tqos);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&tqos, &reftqos, sizeof (tqos)));
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test topic recreate with different name ('HelloWorld').\r\n");
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	r = DDS_Topic_get_qos (t, &tqos);
	DDS_TopicQos__init (&reftqos);
	fail_unless (r == DDS_RETCODE_OK && !memcmp (&tqos, &reftqos, sizeof (tqos)));

	v_printf (" - Test topic users (Publisher/Writer/Subscriber/Reader).\r\n");
	pub = DDS_DomainParticipant_create_publisher (p, DDS_PUBLISHER_QOS_DEFAULT, NULL, 0);
	fail_unless (pub != NULL);
	dw = DDS_Publisher_create_datawriter (pub, t, DDS_DATAWRITER_QOS_DEFAULT, NULL, 0);
	fail_unless (dw != NULL);
	sub = DDS_DomainParticipant_create_subscriber (p, DDS_SUBSCRIBER_QOS_DEFAULT, NULL, 0);
	fail_unless (sub != NULL);
	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);
	dr = DDS_Subscriber_create_datareader (sub, td, DDS_DATAREADER_QOS_DEFAULT, NULL, 0);
	fail_unless (dr != NULL);
	err = dds_seq_from_array (&reftqos.topic_data, data, sizeof (data));
	fail_unless (err == 0);

	delay ();
	v_printf (" - Test update topic QoS.\r\n");
	r = DDS_Topic_set_qos (t, &reftqos);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_Topic_get_qos (t, &tqos);
	fail_unless (r == DDS_RETCODE_OK);
	n = dds_seq_to_array (&reftqos.topic_data, d2, sizeof (d2));
	fail_unless (n == sizeof (data) && !memcmp (data, d2, sizeof (data)));
	DDS_TopicQos__clear (&reftqos);
	DDS_TopicQos__clear (&tqos);

	delay ();
	if (!enabled) {
		v_printf (" - Finally enable child entities.\r\n");
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
		delay ();
	}
	v_printf (" - Delete all child entities.\r\n");
	r = DDS_Publisher_delete_datawriter (pub, dw);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_Subscriber_delete_datareader (sub, dr);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_publisher (p, pub);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_subscriber (p, sub);
	fail_unless (r == DDS_RETCODE_OK);
	v_printf (" - Delete remaining entities.\r\n");
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void topic_on_inconsistent_topic (
	DDS_TopicListener *self,
	DDS_Topic topic, /* in (variable length) */
	DDS_InconsistentTopicStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (topic)
	ARG_NOT_USED (status)
}

static DDS_TopicListener topic_listener = {
	topic_on_inconsistent_topic
};

static void test_listener (void)
{
	DDS_DomainParticipant		p;
	DDS_Topic			t;
	DDS_TopicListener		*lp;
	DDS_ReturnCode_t		r;

	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);

	v_printf (" - Test topic listener.\r\n");
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	lp = DDS_Topic_get_listener (t);
	fail_unless (lp != NULL && lp->on_inconsistent_topic == NULL);

	v_printf (" - Test topic listener updates.\r\n");
	r = DDS_Topic_set_listener (t, &topic_listener, DDS_INCONSISTENT_TOPIC_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_Topic_get_listener (t);
	fail_unless (lp != NULL && lp->on_inconsistent_topic == topic_on_inconsistent_topic);
	r = DDS_Topic_set_listener (t, NULL, DDS_INCONSISTENT_TOPIC_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	lp = DDS_Topic_get_listener (t);
	fail_unless (lp != NULL && lp->on_inconsistent_topic == NULL);

	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_aux (void)
{
	DDS_DomainParticipant		np, p;
	DDS_Topic			t;
	DDS_TopicDescription		td;
	DDS_InconsistentTopicStatus	tstat;
	DDS_StatusCondition		sc, sc2;
	DDS_StatusMask			sm;
	DDS_InstanceHandle_t		h, h2;
	DDS_ReturnCode_t		r;
	const char			*sp;

	v_printf (" - Test auxiliary topic functions.\r\n");
	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless (td != NULL);
	sc = DDS_Topic_get_statuscondition (t);
	fail_unless (sc != NULL);
	sc2 = DDS_Entity_get_statuscondition (t);
	fail_unless (sc == sc2);
	sm = DDS_Topic_get_status_changes (t);
	fail_unless (sm == 0);
	sm = DDS_Entity_get_status_changes (t);
	fail_unless (sm == 0);
	/*dbg_printf ("(mask=%u)", sm);*/
	h = DDS_Entity_get_instance_handle (t);
	fail_unless (h != 0);
	h2 = DDS_Topic_get_instance_handle (t);
	fail_unless (h2 == h);
	np = DDS_Topic_get_participant (t);
	fail_unless (np == p);
	sp = DDS_Topic_get_type_name (t);
	fail_unless (!strcmp (sp, TYPE_NAME));
	sp = DDS_Topic_get_name (t);
	fail_unless (!strcmp (sp, "HelloWorld"));
	r = DDS_Topic_get_inconsistent_topic_status (t, &tstat);
	fail_unless (r == DDS_RETCODE_OK);

	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

static void test_desc (void)
{
	DDS_DomainParticipant		p, np;
	DDS_Topic			t;
	DDS_TopicDescription		td;
	DDS_Duration_t			d;
	DDS_ReturnCode_t		r;
	const char			*name;

	v_printf (" - Test TopicDescription functions.\r\n");
	p = DDS_DomainParticipantFactory_create_participant (3, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);

	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);

	d.sec = 0;
	d.nanosec = 1000000;
	t = DDS_DomainParticipant_find_topic (p, "HelloWorld", &d);
	fail_unless (t == NULL);

	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	np = DDS_TopicDescription_get_participant ((DDS_TopicDescription) t);
	fail_unless (np == p);

	name = DDS_TopicDescription_get_type_name ((DDS_TopicDescription) t);
	fail_unless (!strcmp (name, TYPE_NAME));

	name = DDS_TopicDescription_get_name ((DDS_TopicDescription) t);
	fail_unless (!strcmp (name, "HelloWorld"));

	td = DDS_DomainParticipant_lookup_topicdescription (p, "HelloWorld");
	fail_unless ((DDS_Topic) td == t);

	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_topic (void)
{
	dbg_printf ("Topic ");
	if (trace)
		fflush (stdout);
	if (verbose)
		printf ("\r\n");
	test_qos (0);
	test_qos (1);
	test_listener ();
	test_aux ();
	test_desc ();
	dbg_printf (" - success!\r\n");
}

void test_ftopic (void)
{
	DDS_DomainParticipant		np, p;
	DDS_Topic			t, xt;
	DDS_ContentFilteredTopic	ft;
	static const char		*expression = "counter > %0 and counter < %1";
	DDS_StringSeq			parameters, ftpars;
	DDS_ReturnCode_t		r;
	unsigned			i;
	const char			*sp;
	char				spars [2][20];

	v_printf (" - Create content-filtered topic.\r\n");
	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);

	snprintf (spars [0], sizeof (spars [0]), "%u", 10);
	snprintf (spars [1], sizeof (spars [1]), "%u", 15);
	DDS_SEQ_INIT (parameters);
	for (i = 0; i < 2; i++) {
		sp = spars [i];
		dds_seq_append (&parameters, &sp);
	}
	ft = DDS_DomainParticipant_create_contentfilteredtopic (p, "HWFilter", t,
							expression, &parameters);
	fail_unless (ft != NULL);
	DDS_StringSeq__init (&ftpars);
	r = DDS_ContentFilteredTopic_get_expression_parameters (ft, &ftpars);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (ftpars) == 2 &&
		     !strcmp (DDS_SEQ_ITEM (ftpars, 0), "10") &&
		     !strcmp (DDS_SEQ_ITEM (ftpars, 1), "15"));
	DDS_StringSeq__clear (&ftpars);
	v_printf (" - Update content-filter parameters.\r\n");
	DDS_SEQ_ITEM_SET (parameters, 1, "19");
	r = DDS_ContentFilteredTopic_set_expression_parameters (ft, &parameters);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_ContentFilteredTopic_get_expression_parameters (ft, &ftpars);
	fail_unless (r == DDS_RETCODE_OK &&
	             DDS_SEQ_LENGTH (ftpars) == 2 &&
		     !strcmp (DDS_SEQ_ITEM (ftpars, 0), "10") &&
		     !strcmp (DDS_SEQ_ITEM (ftpars, 1), "19"));
	DDS_StringSeq__clear (&ftpars);
	DDS_StringSeq__clear (&parameters);
	v_printf (" - Test auxiliary content-filter functions.\r\n");
	sp = DDS_ContentFilteredTopic_get_filter_expression (ft);
	fail_unless (!strcmp (sp, expression));
	xt = DDS_ContentFilteredTopic_get_related_topic (ft);
	fail_unless (xt == t);
	np = DDS_ContentFilteredTopic_get_participant (ft);
	fail_unless (np == p);
	sp = DDS_ContentFilteredTopic_get_type_name (ft);
	fail_unless (!strcmp (sp, TYPE_NAME));
	sp = DDS_ContentFilteredTopic_get_name (ft);
	fail_unless (!strcmp (sp, "HWFilter"));

	v_printf (" - Test proper delete of content-filter.\r\n");
	r = DDS_DomainParticipant_delete_contentfilteredtopic (p, ft);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_contentfilteredtopic (void)
{
	dbg_printf ("ContentFilteredTopic ");
	if (trace)
		fflush (stdout);
	if (verbose)
		printf ("\r\n");
	test_ftopic ();
	dbg_printf (" - success!\r\n");
}

static void test_mtopic (void)
{
	DDS_DomainParticipant		p;
	DDS_Topic			t;
	DDS_MultiTopic			mt;
	DDS_ReturnCode_t		r;

	v_printf (" - Test Multitopic (not implemented).\r\n");
	p = DDS_DomainParticipantFactory_create_participant (0, DDS_PARTICIPANT_QOS_DEFAULT, NULL, 0);
	fail_unless (p != NULL);
	r = register_HelloWorldData_type (p);
	fail_unless (r == DDS_RETCODE_OK);
	t = DDS_DomainParticipant_create_topic (p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
	fail_unless (t != NULL);
	mt = DDS_DomainParticipant_create_multitopic (p, "HelloWorldMulti", TYPE_NAME,
		"SELECT key, counter FROM `HelloWorld' WHERE counter > 200;",
		NULL);
	fail_unless (mt == NULL);	/* Unsupported! */

	r = DDS_DomainParticipant_delete_multitopic (p, mt);
	fail_unless (r == DDS_RETCODE_UNSUPPORTED);
	r = DDS_DomainParticipant_delete_topic (p, t);
	fail_unless (r == DDS_RETCODE_OK);
	unregister_HelloWorldData_type (p);
	r = DDS_DomainParticipantFactory_delete_participant (p);
	fail_unless (r == DDS_RETCODE_OK);
}

void test_multitopic (void)
{
	dbg_printf ("MultiTopic ");
	if (trace)
		fflush (stdout);
	if (verbose)
		printf ("\r\n");
	test_mtopic ();
	dbg_printf (" - success!\r\n");
}

