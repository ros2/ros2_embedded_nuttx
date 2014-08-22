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
#include <string.h>
#include <unistd.h>
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "test.h"

/*#define INTERACTIVE*/

int all_done = 0;

#define CHECK(r, name, fct)	if(((r) = fct) != 0) { printf ("ERROR %d in " name "\r\n", r); return (1); }
#ifdef INTERACTIVE
#define	SLEEP(n)		wait_ready(n); if (all_done) goto cleanup

int		paused;
unsigned	nsteps;

void wait_ready (unsigned delay)
{
	usleep (delay);
	if (!nsteps) {
		paused = 1;
		do {
			sleep (delay);
		}
		while (!all_done && paused);
	}
	nsteps--;
}

#else
#define	SLEEP(n)		usleep (n * 10000)
#endif

int main (void)
{
	DDS_ReturnCode_t       ret;
	DDS_DomainParticipant  domain;
	DDS_Publisher          publisher;
	DDS_Topic              topic1;
	DDS_DataWriter         dw1;
	Tst1Msg                t1Msg;
	DDS_Topic              topic2;
	DDS_DataWriter         dw2;
	Tst2Msg                t2Msg;
	DDS_Topic              topic3;
	DDS_DataWriter         dw3;
	Tst3Msg                t3Msg;
	DDS_Topic              topic4;
	DDS_DataWriter         dw4;
	Tst4Msg                t4Msg;
	int                    i;
	DDS_InstanceHandle_t   h;
	unsigned char          buffer [128];

	all_done = 0;
#ifdef INTERACTIVE
	paused = 1;
	nsteps = 0;
	dds_dbg_init ();
	dds_dbg_abort (&all_done);
	dds_dbg_control (&paused, &nsteps, NULL);
#endif
	DDS_entity_name ("Technicolor Publisher test");

	/* create a DDS_DomainParticipant */
	domain = DDS_DomainParticipantFactory_create_participant (0, 
								  DDS_PARTICIPANT_QOS_DEFAULT, 
								  NULL, 
								  0);
	if (domain == NULL) {
		printf("ERROR creating domain participant.\n");
		return -1;
	}

	/* Create a DDS Publisher with the default QoS settings */
	publisher = DDS_DomainParticipant_create_publisher (domain, 
							    DDS_PUBLISHER_QOS_DEFAULT, 
							    NULL, 
							    0);
	if (publisher == NULL) {
		printf("ERROR creating publisher.\n");
		return -1;
	}
  
	/* Register the Tst1Msg type with the CoreDX middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	if (Tst1MsgTypeSupport_register_type (domain, NULL) != DDS_RETCODE_OK) {
		printf("ERROR registering type\n");
		return -1;
	}
  
	/* create a DDS Topic with the Tst1Msg data type. */
	topic1 = DDS_DomainParticipant_create_topic (domain, 
						     "tst1Topic", 
						     "Tst1Msg", 
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL, 
						     0);
	if (topic1 == NULL) {
		printf("ERROR creating topic.\n");
		return -1;
	}

	/* Create a DDS DataWriter on the hello topic, 
	 * with default QoS settings and no listeners.
	 */
	dw1 = DDS_Publisher_create_datawriter (publisher,
					       topic1, 
					       DDS_DATAWRITER_QOS_DEFAULT, 
					       NULL, 
					       0);
	if (dw1 == NULL) {
		printf("ERROR creating data writer\n");
		return -1;
	}

	/* Wait for at least one subscriber to appear 
	*/
	printf("Waiting for a subscriber... ");
	fflush (stdout);
	while (!all_done) {
		DDS_InstanceHandleSeq dr_handles;

		DDS_DataWriter_get_matched_subscriptions(dw1, &dr_handles);
		if (dr_handles._length > 0) {
			dds_seq_cleanup (&dr_handles);
			printf ("%u subscribers found.\n", dr_handles._length);
			break;
		}
		else {
			SLEEP(1);
			if (all_done)
				printf ("done\r\n");
		}
	}

	/* Setup t1Msg. */
	t1Msg.c1 = 0xaa;
	t1Msg.ll = 0x0123456789abcdefULL;
	t1Msg.c2 = 0xbb;
	DDS_SEQ_INIT (t1Msg.data);
	t1Msg.data._buffer = buffer;
	for (i = 0; i < 100; i++)
		buffer [i] = ' ' + i;
	t1Msg.data._length = t1Msg.data._maximum = 100;
	printf ("W: t1-Write(aa/0123456789abcdef/bb - 100 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw1, &t1Msg, DDS_HANDLE_NIL));
	SLEEP (1);
	t1Msg.data._length = 64;
	printf ("W: t1-Write(aa/0123456789abcdef/bb - 64 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw1, &t1Msg, DDS_HANDLE_NIL));
	SLEEP (1);
	h = DDS_DataWriter_lookup_instance (dw1, &t1Msg);
	if (h == DDS_HANDLE_NIL) {
		printf ("ERROR retrieving handle\n");
		fflush (stdout);
		return -1;
	}
	printf ("W: t1-Lookup(aa/0123456789abcdef/bb) => <h> successful\r\n");
	SLEEP (1);
	printf ("W: t1-Dispose(<h>)\r\n");
	CHECK (ret, "dispose", DDS_DataWriter_dispose (dw1, &t1Msg, h));
	SLEEP (1);
	printf ("W: t1-Unregister(<h>)\r\n");
	CHECK (ret, "unregister", DDS_DataWriter_unregister_instance (dw1, &t1Msg, h));\
	printf ("-----------------  t1 done  ------------------\r\n");
	SLEEP (1);
	/*dds_cache_dump (dw1);*/

	/* Register the Tst2Msg type with the CoreDX middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	if (Tst2MsgTypeSupport_register_type (domain, NULL) != DDS_RETCODE_OK) {
		printf("ERROR registering type\n");
		return -1;
	}
  
	/* create a DDS Topic with the Tst1Msg data type. */
	topic2 = DDS_DomainParticipant_create_topic (domain, 
						     "tst2Topic", 
						     "Tst2Msg", 
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL, 
						     0);
	if (topic2 == NULL) {
		printf("ERROR creating topic.\n");
		return -1;
	}

	/* Create a DDS DataWriter on the hello topic, 
	 * with default QoS settings and no listeners.
	 */
	dw2 = DDS_Publisher_create_datawriter (publisher,
					       topic2, 
					       DDS_DATAWRITER_QOS_DEFAULT, 
					       NULL, 
					       0);
	if (dw2 == NULL) {
		printf("ERROR creating data writer\n");
		return -1;
	}

	/* Setup t2Msg. */
	t2Msg.c1 = 0x99;
	t2Msg.w = 0x76543210U;
	strcpy (t2Msg.n, "Hello.");
	t2Msg.s = 0x4455;
	DDS_SEQ_INIT (t2Msg.data);
	t2Msg.data._buffer = buffer;
	for (i = 0; i < 110; i++)
		buffer [i] = ' ' + i;
	t2Msg.data._length = t2Msg.data._maximum = 90;
	printf ("W: t2-Register(99/76543210/'Hello.'/4455 => <h>\r\n");
	h = DDS_DataWriter_register_instance (dw2, &t2Msg);
	if (h == DDS_HANDLE_NIL) {
		printf("ERROR registering instance\n");
		return -1;
	}
	SLEEP (1);
	printf ("W: t2-Write(<h> - 90 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw2, &t2Msg, h));
	SLEEP (1);
	t2Msg.data._length = 64;
	printf ("W: t2-Write(99/76543210/'Hello.'/4455 - 64 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw2, &t2Msg, DDS_HANDLE_NIL));
	SLEEP (1);
	h = DDS_DataWriter_lookup_instance (dw2, &t2Msg);
	if (h == DDS_HANDLE_NIL) {
		printf ("ERROR retrieving handle\n");
		fflush (stdout);
		return -1;
	}
	printf ("W: t2-Lookup(99/76543210/'Hello.'/4455 => <h> successful\r\n");
	SLEEP (1);
	printf ("W: t2-Unregister(<h>)\r\n");
	CHECK (ret, "unregister", DDS_DataWriter_unregister_instance (dw2, &t2Msg, h));
	printf ("-----------------  t2 done  ------------------\r\n");
	SLEEP (1);

	/* Register the Tst3Msg type with the CoreDX middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	if (Tst3MsgTypeSupport_register_type( domain, NULL) != DDS_RETCODE_OK) {
		printf("ERROR registering type\n");
		return -1;
	}
  
	/* create a DDS Topic with the Tst3Msg data type. */
	topic3 = DDS_DomainParticipant_create_topic (domain, 
						     "tst3Topic", 
						     "Tst3Msg", 
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL, 
						     0);
	if (topic3 == NULL) {
		printf("ERROR creating topic.\n");
		return -1;
	}

	/* Create a DDS DataWriter on the topic, 
	 * with default QoS settings and no listeners.
	 */
	dw3 = DDS_Publisher_create_datawriter (publisher,
					       topic3, 
					       DDS_DATAWRITER_QOS_DEFAULT, 
					       NULL, 
					       0);
	if (dw3 == NULL) {
		printf("ERROR creating data writer\n");
		return -1;
	}

	/* Setup t3Msg. */
	t3Msg.c1 = 0x77;
	t3Msg.s = 0x4567U;
	t3Msg.l = 0x76543210U;
	strcpy (t3Msg.n, "Bye Bye");
	t3Msg.ll = 0x4455223366770011LL;
	DDS_SEQ_INIT (t3Msg.data);
	t3Msg.data._buffer = buffer;
	for (i = 0; i < 112; i++)
		buffer [i] = ' ' + i;
	t3Msg.data._length = t3Msg.data._maximum = 92;
	h = DDS_DataWriter_register_instance (dw3, &t3Msg);
	if (h == DDS_HANDLE_NIL) {
		printf("ERROR registering instance\n");
		return -1;
	}
	SLEEP (1);
	printf ("W: t3-Write(77/4567/76543210/'Bye Bye'/4455223366770011 - 92 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw3, &t3Msg, h));
	SLEEP (1);
	t3Msg.data._length = 66;
	printf ("W: t3-Write(77/4567/76543210/'Bye Bye'/4455223366770011 - 66 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw3, &t3Msg, DDS_HANDLE_NIL));
	SLEEP (1);
	printf ("W: t3-Dispose(<h>)\r\n");
	CHECK (ret, "dispose", DDS_DataWriter_dispose (dw3, &t3Msg, h));
	SLEEP (1);
	h = DDS_DataWriter_lookup_instance (dw3, &t3Msg);
	if (h == DDS_HANDLE_NIL) {
		printf ("ERROR retrieving handle <h> - as expected?\n");
		fflush (stdout);
	}
	t3Msg.data._length = 67;
	printf ("W: t3-Write(77/4567/76543210/'Bye Bye'/4455223366770011 - 67 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw3, &t3Msg, h));
	SLEEP (1);
	printf ("W: t3-Dispose(<h>)\r\n");
	CHECK (ret, "dispose", DDS_DataWriter_dispose (dw3, &t3Msg, h));
	SLEEP (1);
	t3Msg.data._length = 69;
	printf ("W: t3-Write(77/4567/76543210/'Bye Bye'/4455223366770011 - 69 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw3, &t3Msg, h));
	SLEEP (1);
	printf ("W: t3-Unregister(<h>)\r\n");
	CHECK (ret, "unregister", DDS_DataWriter_unregister_instance (dw3, &t3Msg, h));
	printf ("-----------------  t3 done  ------------------\r\n");
	SLEEP (1);

	/* Register the Tst4Msg type with the CoreDX middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	if (Tst4MsgTypeSupport_register_type (domain, NULL) != DDS_RETCODE_OK) {
		printf("ERROR registering type\n");
		return -1;
	}
  
	/* create a DDS Topic with the Tst4Msg data type. */
	topic4 = DDS_DomainParticipant_create_topic (domain, 
						     "tst4Topic", 
						     "Tst4Msg", 
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL, 
						     0);
	if (topic4 == NULL) {
		printf("ERROR creating topic.\n");
		return -1;
	}

	/* Create a DDS DataWriter on the topic, 
	 * with default QoS settings and no listeners.
	 */
	dw4 = DDS_Publisher_create_datawriter (publisher,
					       topic4, 
					       DDS_DATAWRITER_QOS_DEFAULT, 
					       NULL, 
					       0);
	if (dw4 == NULL) {
		printf("ERROR creating data writer\n");
		return -1;
	}

	/* Setup t4Msg. */
	t4Msg.c1 = 0x33;
	t4Msg.n = "Ho";
	t4Msg.s = 0x789a;
	DDS_SEQ_INIT (t4Msg.data);
	t4Msg.data._buffer = buffer;
	for (i = 0; i < 55; i++)
		buffer [i] = ' ' + i;
	t4Msg.data._length = t4Msg.data._maximum = 55;
	h = DDS_DataWriter_register_instance (dw4, &t4Msg);
	if (h == DDS_HANDLE_NIL) {
		printf("ERROR registering instance\n");
		return -1;
	}
	SLEEP (1);
	printf ("W: t4-Write(33/'Ho'/789a - 55 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw4, &t4Msg, h));
	SLEEP (1);
	t4Msg.data._length = 53;
	printf ("W: t4-Write(33/'Ho'/789a - 53 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw4, &t4Msg, DDS_HANDLE_NIL));
	SLEEP (1);
	h = DDS_DataWriter_lookup_instance (dw4, &t4Msg);
	if (h == DDS_HANDLE_NIL) {
		printf ("ERROR retrieving handle\n");
		fflush (stdout);
		return -1;
	}
	printf ("W: t4-Unregister(<h>)\r\n");
	CHECK (ret, "unregister", DDS_DataWriter_unregister_instance (dw4, &t4Msg, h));
	SLEEP (1);

	/* Setup t4Msg. */
	t4Msg.c1 = 0x34;
	t4Msg.n = "Ho ho ho .... ";
	t4Msg.s = 0x7aac;
	DDS_SEQ_INIT (t4Msg.data);
	t4Msg.data._buffer = buffer;
	for (i = 0; i < 52; i++)
		buffer [i] = ' ' + i;
	t4Msg.data._length = t4Msg.data._maximum = 52;
	h = DDS_DataWriter_register_instance (dw4, &t4Msg);
	if (h == DDS_HANDLE_NIL) {
		printf("ERROR registering instance\n");
		return -1;
	}
	SLEEP (1);
	printf ("W: t4-Write(34/'Ho ho ho .... '/7aac - 52 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw4, &t4Msg, h));
	SLEEP(1);
	t4Msg.data._length = 51;
	printf ("W: t4-Write(34/'Ho ho ho .... '/7aac - 51 bytes)\r\n");
	CHECK (ret, "write", DDS_DataWriter_write (dw4, &t4Msg, DDS_HANDLE_NIL));
	SLEEP(1);
	h = DDS_DataWriter_lookup_instance (dw4, &t4Msg);
	if (h == DDS_HANDLE_NIL) {
		printf ("ERROR retrieving handle\n");
		fflush (stdout);
		return -1;
	}
	printf ("W: t4-Unregister(<h>)\r\n");
	CHECK (ret, "unregister", DDS_DataWriter_unregister_instance (dw4, &t4Msg, h));
	printf ("-----------------  t4 done  ------------------\r\n");
	SLEEP (1);

	/* Cleanup */
#ifdef INTERACTIVE
    cleanup:
#endif
	DDS_DomainParticipant_delete_contained_entities(domain);
	DDS_DomainParticipantFactory_delete_participant(domain);

	/*test_unregister_types ();*/


	return 0;
}

