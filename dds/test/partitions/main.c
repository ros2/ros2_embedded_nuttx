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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "dds/dds_dcps.h"
#include "dds/dds_aux.h"
#include "dds/dds_error.h"

DDS_DomainParticipant	 participant;
DDS_Publisher		 publisher;
DDS_Subscriber		 subscriber;
DDS_DataWriter           writer;
DDS_DataReader           reader;
DDS_Topic                topic;
DDS_TopicDescription     topic_desc;
const char               *topic_name = "partitionTest";
const char               *topic_type = "PartitionTestType";
int                      quit = 0;
DDS_TypeSupport          type;

typedef struct test_type_st {
	int		x;
} TestType_t;

typedef struct test_st {
	int        x;
	TestType_t type;
	DDS_HANDLE h;
} Test;

Test                     my_test_data;

void on_inc_qos (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	printf ("========================================= invalid Qos ");
}

void on_data_read (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	printf ("====================================================================================================== Data available ");
}

const static DDS_DataReaderListener r_listener = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	on_inc_qos,		/* Requested incompatible QoS. */
	on_data_read,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

void createTopic (void)
{
	topic = DDS_DomainParticipant_create_topic (participant, topic_name, topic_type,
						    NULL, NULL, 0);

	/* Create Topic. */
	if (!topic)
		fatal ("DDS_DomainParticipant_create_topic () failed!(%d)\r\n", __LINE__);

	printf ("DDS Topic (%s) created.\r\n(%d)\r\n", topic_type, __LINE__);

	topic_desc = DDS_DomainParticipant_lookup_topicdescription (participant, topic_name);

}

void createPublisher (void)
{
	if (!publisher) {	/* Create a publisher. */
		publisher = DDS_DomainParticipant_create_publisher (participant, NULL, NULL, 0);
		if (!publisher)
			fatal ("DDS_DomainParticipant_create_publisher () failed!(%d)\r\n", __LINE__);
		
		printf ("DDS Publisher created.(%d)\r\n", __LINE__);
	}
}

void setPublisherQos (const char *partitionString)
{
	DDS_PublisherQos	pqos;
        DDS_ReturnCode_t error;

	if ((error = DDS_Publisher_get_qos (publisher, &pqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Publisher_get_qos () failed (%s)!", DDS_error (error));

	dds_seq_reset (&pqos.partition.name);
	dds_seq_append (&pqos.partition.name, &partitionString);

	DDS_Publisher_set_qos (publisher, &pqos);
	dds_seq_cleanup (&pqos.partition.name);
}

void createDW (void)
{
	DDS_DataWriterQos 	wr_qos;
	Test                    data;

	DDS_Publisher_get_default_datawriter_qos (publisher, &wr_qos);
	
	writer = DDS_Publisher_create_datawriter (publisher, topic, &wr_qos, NULL, 0);
	if (!writer) {
		fatal ("Unable to create writer(%d)\r\n", __LINE__);
		DDS_DomainParticipantFactory_delete_participant (participant);
	}
	printf ("DDS Writer created.(%d)\r\n", __LINE__);
	data.x = 5;
	my_test_data.h = DDS_DataWriter_register_instance (writer, &data);

}

void writeSomething (void)
{
	TestType_t data;
	data.x = 5;
	DDS_DataWriter_write (writer, &data, my_test_data.h );
}

void createSubscriber (void)
{
	if (!subscriber) {
		subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0); 
		if (!subscriber)
			fatal ("DDS_DomainParticipant_create_subscriber () returned an error!(%d)\r\n", __LINE__);
		printf ("DDS Subscriber created.(%d)\r\n", __LINE__);
	}
}

void setSubscriberQos (const char *partitionString)
{
	DDS_SubscriberQos	sqos;
	DDS_ReturnCode_t        error;

	if ((error = DDS_Subscriber_get_qos (subscriber, &sqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Subscriber_get_qos () failed (%s)!", DDS_error (error));

	dds_seq_reset (&sqos.partition.name);
	dds_seq_append (&sqos.partition.name, &partitionString);
	
	DDS_Subscriber_set_qos (subscriber, &sqos);
	
	dds_seq_cleanup (&sqos.partition.name);
}

void createDR (void)
{
	DDS_DataReaderQos dr_qos;

	DDS_Subscriber_get_default_datareader_qos (subscriber, &dr_qos);

	reader = DDS_Subscriber_create_datareader (subscriber, topic_desc, &dr_qos, &r_listener, 0);
	if (!reader)
		fatal ("DDS_DomainParticipant_create_datareader () returned an error!(%d)\r\n", __LINE__);

	printf ("DDS Reader created.(%d)\r\n", __LINE__);
}

static DDS_TypeSupport_meta test_tsm [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "PartitionType", sizeof (struct test_type_st), 0, 1, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct test_type_st, x), 0, 0, NULL }
};

void register_my_type (void)
{
	DDS_ReturnCode_t	error;

	type = DDS_DynamicType_register (test_tsm);
        if (!type)
		fatal_printf ("Error registering type!");

	error = DDS_DomainParticipant_register_type (participant, type, topic_type);
}

void end_program (int signum)
{
	quit = 1;
}

/* Argv [1] --> r | w
   argv [2] --> domain id */

int main (int argc, const char *argv [])
{
	DDS_PoolConstraints	reqs;
	DDS_ReturnCode_t        error;
	unsigned counterUp = 0;
	unsigned counterDown = ~0;
	unsigned random = 0;
	char str [25];
/*	const char *zero = "Zero";
	const char *one = "One";
	const char *two = "Two";
	const char *three = "Three";
	const char *four = "Four";
	const char *five = "Five";
	const char *six = "Six";
	const char *seven = "Seven";
	const char *eight = "Eight";
	const char *nine = "Nine";
*/
	const char *all [10] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};

	/*true random number*/
	srand(time(NULL));
		
	DDS_Log_stdio (1);
	/*if (trace)
		rtps_trace = 1;*/

	DDS_get_default_pool_constraints (&reqs, ~0, 100);
/*	reqs.max_rx_buffers = 32;
	reqs.min_local_readers = reqs.max_local_readers = 9;
	reqs.min_local_writers = reqs.max_local_writers = 7;
	reqs.min_changes = 64;
	reqs.min_instances = 48;*/
	DDS_set_pool_constraints (&reqs);
	DDS_entity_name ("Partition Test Program");
#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a domain participant. */
	participant = DDS_DomainParticipantFactory_create_participant (atoi (argv [2]), NULL, NULL, 0);
	if (!participant)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	printf ("DDS Domain Participant created.\r\n");


	/* DO THE TEST */

	register_my_type ();
	createTopic ();

	signal (SIGINT, end_program);

	if (!strcmp (argv [1], "w" )) {
		createPublisher ();
		createDW ();
	}else {
		createSubscriber ();
		createDR ();
	}

	while (1) {
		random = (rand() % 3);
		if (!strcmp (argv [1], "w" )) {
			/* snprintf(str, 25, "%d", counterUp ++);
			setPublisherQos ( (const char *) &str [0]); */
			setPublisherQos ( all [random]);
			writeSomething ();
		}
		else {
			/* snprintf(str, 25, "%d", counterDown --);
			setSubscriberQos ( (const char *) &str [0]); */
			setSubscriberQos ( all [random]);
		}
		if (quit)
			break;

		/* sleep (1); */
	}

	if (!strcmp (argv [1], "w" )) {
		error = DDS_Publisher_delete_datawriter (publisher, writer);
		if (error)
			fatal ("DDS_Publisher_delete_datawriter() failed (%s)!", DDS_error (error));
		
		dbg_printf ("DDS Writer deleted.\r\n");
		
		error = DDS_DomainParticipant_delete_publisher (participant, publisher);
		if (error)
			fatal ("DDS_Domainpartiticipant_delete_publisher() failed (%s)!", DDS_error (error));
		
		dbg_printf ("DDS Publisher deleted.\r\n");
	} else {
		error = DDS_Subscriber_delete_datareader (subscriber, reader);
		if (error)
			fatal ("DDS_Subscriber_delete_datareader() failed (%s)!", DDS_error (error));
		
		dbg_printf ("DDS Reader deleted.\r\n");

		error = DDS_DomainParticipant_delete_subscriber (participant, subscriber);
		if (error)
			fatal ("DDS_Domainpartiticipant_delete_subscriber() failed (%s)!", DDS_error (error));
		
		dbg_printf ("DDS Subscriber deleted.\r\n");
	}

	error = DDS_DomainParticipant_delete_topic (participant, topic);
	if (error)
		fatal ("DDS_DomainParticipant_delete_topic () failed (%s)!", DDS_error (error));
	
	dbg_printf ("DDS Topic deleted.\r\n");

	DDS_DomainParticipant_unregister_type (participant, type, "PartitionType");


	error = DDS_DomainParticipant_delete_typesupport (participant, type);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	dbg_printf ("DDS Topic Type deleted.\r\n");

	error = DDS_DomainParticipant_delete_contained_entities (participant);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (participant);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	printf ("DDS Participant deleted\r\n");

	return (0);

}
