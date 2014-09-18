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
#include <unistd.h>
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"
#include "test.h"

#define INTERACTIVE
#define	SLEEP(n)		usleep (n * 1000)

int all_done = 0;

/****************************************************************
 * DataReader Listener Method: dr_on_data_avail_t1()
 *
 * This listener method is called when data is available to
 * be read on this DataReader.
 ****************************************************************/
void dr_on_data_avail_t1 (DDS_DataReaderListener *self, DDS_DataReader dr1)
{
	static DDS_DataSeq	samples = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq samples_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_ReturnCode_t      	retval;
	DDS_SampleStateMask   	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask     	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask 	is = DDS_ANY_INSTANCE_STATE;
	Tst1Msg			s;

	/* Take any and all available samples.  The take() operation
	 * will remove the samples from the DataReader so they
	 * won't be available on subsequent read() or take() calls.
	 */
	self->cookie = 0;
	retval = DDS_DataReader_take (dr1, &samples, &samples_info,
					     DDS_LENGTH_UNLIMITED, 
					     ss, 
					     vs, 
					     is);
	if (retval == DDS_RETCODE_OK) {
		unsigned int i;

		/* iterrate through the samples */
		for ( i = 0;i < samples._length; i++) {
			Tst1Msg *smsg      = samples._buffer[i];
			DDS_SampleInfo *si = samples_info._buffer[i];

			/* If this sample does not contain valid data,
			 * it is a dispose or other non-data command,
			 * and, accessing any member from this sample 
			 * would be invalid.
			 */
			if (si->instance_state == DDS_ALIVE_INSTANCE_STATE)
				printf ("R: ");
			else if (si->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
				printf ("R: t1-Not alive - Disposed: ");
			else if (si->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
				printf ("R: t1-Not alive - No Writers: ");
			if (si->valid_data) {
				printf("t1-DATA: c1=%02x, ll=%08llx, c2=%02x:", smsg->c1, (long long) smsg->ll, smsg->c2);
				for (i = 0; i < smsg->data._length; i++)
					printf (" %02x", smsg->data._buffer [i]);
				printf("\r\n");
			}
			else {
				retval = DDS_DataReader_get_key_value (dr1, &s, si->instance_handle);
				if (retval != DDS_RETCODE_OK)
					printf ("ERROR (%s): unable to get key data\n", DDS_error(retval));
				else
					printf ("Key: c1=%02x, ll=%08llx, c2=%02x\n", s.c1, (long long) s.ll, s.c2);
			}
		}
		fflush (stdout);

		/* read() and take() always "loan" the data, we need to
		 * return it so CoreDX can release resources associated
		 * with it.  
		 */
		retval = DDS_DataReader_return_loan (dr1, &samples, &samples_info);
		if (retval != DDS_RETCODE_OK)
			printf("ERROR (%s): unable to return loan of samples\n",
								DDS_error (retval));
	}
	else
		printf ("ERROR (%s) taking samples from DataReader\n", DDS_error(retval));
}

/****************************************************************
 * Create a DataReaderListener with a pointer to the function 
 * that should be called on data_available events.  All other 
 * function pointers are NULL. (no listener method defined).
 ****************************************************************/
DDS_DataReaderListener drListener1 = 
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_data_avail_t1,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL,
	NULL
};

/****************************************************************
 * DataReader Listener Method: dr_on_data_avail_t2()
 *
 * This listener method is called when data is available to
 * be read on this DataReader.
 ****************************************************************/
void dr_on_data_avail_t2 (DDS_DataReaderListener *self, DDS_DataReader dr2)
{
	static DDS_DataSeq	samples = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq samples_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_ReturnCode_t      	retval;
	DDS_SampleStateMask   	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask     	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask 	is = DDS_ANY_INSTANCE_STATE;
	Tst2Msg 		s;

	/* Take any and all available samples.  The take() operation
	 * will remove the samples from the DataReader so they
	 * won't be available on subsequent read() or take() calls.
	 */
	self->cookie = 0;
	retval = DDS_DataReader_take (dr2, &samples, &samples_info,
					     DDS_LENGTH_UNLIMITED, 
					     ss, 
					     vs, 
					     is);
	if (retval == DDS_RETCODE_OK) {
		unsigned int i;

		/* iterrate through the samples */
		for ( i = 0;i < samples._length; i++) {
			Tst2Msg *smsg      = samples._buffer[i];
			DDS_SampleInfo *si = samples_info._buffer[i];

			/* If this sample does not contain valid data,
			 * it is a dispose or other non-data command,
			 * and, accessing any member from this sample 
			 * would be invalid.
			 */
			if (si->instance_state == DDS_ALIVE_INSTANCE_STATE)
				printf ("R: ");
			else if (si->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
				printf ("R: t2-Not alive - Disposed: ");
			else if (si->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
				printf ("R: t2-Not alive - No Writers: ");
			if (si->valid_data) {
				printf("t2-DATA: c1=%02x, w=%08x, n='%s', s=%04x:", smsg->c1, smsg->w, smsg->n, smsg->s);
				for (i = 0; i < smsg->data._length; i++)
					printf (" %02x", smsg->data._buffer [i]);
				printf ("\r\n");
			}
			else {
				retval = DDS_DataReader_get_key_value (dr2, &s, si->instance_handle);
				if (retval != DDS_RETCODE_OK)
					printf("ERROR (%s): unable to get key data\n", DDS_error(retval));
				else
					printf ("Key: c1=%02x, w=%08x, n='%s', s=%04x\n", s.c1, s.w, s.n, s.s);
			}
		}
		fflush (stdout);

		/* read() and take() always "loan" the data, we need to
		 * return it so CoreDX can release resources associated
		 * with it.  
		 */
		retval = DDS_DataReader_return_loan (dr2, &samples, &samples_info);
		if (retval != DDS_RETCODE_OK)
			printf("ERROR (%s): unable to return loan of samples\n",
								DDS_error(retval));
	}
	else
		printf("ERROR (%s) taking samples from DataReader\n", DDS_error(retval));
}

/****************************************************************
 * Create a DataReaderListener with a pointer to the function 
 * that should be called on data_available events.  All other 
 * function pointers are NULL. (no listener method defined).
 ****************************************************************/
DDS_DataReaderListener drListener2 = 
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_data_avail_t2,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL,
	NULL
};

/****************************************************************
 * DataReader Listener Method: dr_on_data_avail_t3()
 *
 * This listener method is called when data is available to
 * be read on this DataReader.
 ****************************************************************/
void dr_on_data_avail_t3 (DDS_DataReaderListener *self, DDS_DataReader dr3)
{
	static DDS_DataSeq	samples = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq samples_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_ReturnCode_t      	retval;
	DDS_SampleStateMask   	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask     	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask 	is = DDS_ANY_INSTANCE_STATE;
	Tst3Msg			s;

	/* Take any and all available samples.  The take() operation
	 * will remove the samples from the DataReader so they
	 * won't be available on subsequent read() or take() calls.
	 */
	self->cookie = 0;
	retval = DDS_DataReader_take (dr3, &samples, &samples_info,
					     DDS_LENGTH_UNLIMITED, 
					     ss, 
					     vs, 
					     is);
	if (retval == DDS_RETCODE_OK) {
		unsigned int i;

		/* iterrate through the samples */
		for ( i = 0;i < samples._length; i++) {
			Tst3Msg *smsg      = samples._buffer[i];
			DDS_SampleInfo *si = samples_info._buffer[i];

			/* If this sample does not contain valid data,
			 * it is a dispose or other non-data command,
			 * and, accessing any member from this sample 
			 * would be invalid.
			 */
			if (si->instance_state == DDS_ALIVE_INSTANCE_STATE)
				printf ("R: ");
			else if (si->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
				printf ("R: t3-Not alive - Disposed: ");
			else if (si->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
				printf ("R: t3-Not alive - No Writers: ");
			if (si->valid_data) {
				printf("t3-DATA: c1=%02x, s=%04x, l=%08x, n='%s', ll=%016llx:", smsg->c1, smsg->s, smsg->l, smsg->n, (long long) smsg->ll);
				for (i = 0; i < smsg->data._length; i++)
					printf (" %02x", smsg->data._buffer [i]);
				printf ("\r\n");
			}
			else {
				retval = DDS_DataReader_get_key_value (dr3, &s, si->instance_handle);
				if (retval != DDS_RETCODE_OK)
					printf("ERROR (%s): unable to get key data\n", DDS_error(retval));
				else
					printf ("Key: c1=%02x, s=%04x, l=%08x, n='%s', ll=%016llx\n", s.c1, s.s, s.l, s.n, (long long) s.ll);
			}
		}
		fflush (stdout);

		/* read() and take() always "loan" the data, we need to
		 * return it so CoreDX can release resources associated
		 * with it.  
		 */
		retval = DDS_DataReader_return_loan (dr3, &samples, &samples_info);
		if (retval != DDS_RETCODE_OK)
			printf("ERROR (%s): unable to return loan of samples\n",
								DDS_error(retval));
	}
	else
		printf("ERROR (%s) taking samples from DataReader\n", DDS_error(retval));
}


/****************************************************************
 * Create a DataReaderListener with a pointer to the function 
 * that should be called on data_available events.  All other 
 * function pointers are NULL. (no listener method defined).
 ****************************************************************/
DDS_DataReaderListener drListener3 = 
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_data_avail_t3,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL,
	NULL
};


/****************************************************************
 * DataReader Listener Method: dr_on_data_avail_t4()
 *
 * This listener method is called when data is available to
 * be read on this DataReader.
 ****************************************************************/
void dr_on_data_avail_t4 (DDS_DataReaderListener *self, DDS_DataReader dr4)
{
	static DDS_DataSeq	samples = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq samples_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_ReturnCode_t      	retval;
	DDS_SampleStateMask   	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask     	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask 	is = DDS_ANY_INSTANCE_STATE;
	Tst4Msg			s;

	/* Take any and all available samples.  The take() operation
	 * will remove the samples from the DataReader so they
	 * won't be available on subsequent read() or take() calls.
	 */
	self->cookie = 0;
	retval = DDS_DataReader_take (dr4, &samples, &samples_info,
					     DDS_LENGTH_UNLIMITED, 
					     ss, 
					     vs, 
					     is);
	if (retval == DDS_RETCODE_OK) {
		unsigned int i;

		/* iterrate through the samples */
		for ( i = 0;i < samples._length; i++) {
			Tst4Msg *smsg      = samples._buffer[i];
			DDS_SampleInfo *si = samples_info._buffer[i];

			/* If this sample does not contain valid data,
			 * it is a dispose or other non-data command,
			 * and, accessing any member from this sample 
			 * would be invalid.
			 */
			if (si->instance_state == DDS_ALIVE_INSTANCE_STATE)
				printf ("R: ");
			else if (si->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
				printf ("R: t4-Not alive - Disposed: ");
			else if (si->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
				printf ("R: t4-Not alive - No Writers: ");
			if (si->valid_data) {
				printf("t4-DATA: c1=%02x, n='%s', s=%04x:", smsg->c1, smsg->n, smsg->s);
				for (i = 0; i < smsg->data._length; i++)
					printf (" %02x", smsg->data._buffer [i]);
				printf ("\r\n");
			}
			else {
				s.n = NULL;
				retval = DDS_DataReader_get_key_value (dr4, &s, si->instance_handle);
				if (retval != DDS_RETCODE_OK)
					printf("ERROR (%s): unable to get key data\n", DDS_error(retval));
				else {
					printf ("Key: c1=%02x, n='%s', s=%04x\n", s.c1, s.n, s.s);
					free (s. n);
				}
			}
		}
		fflush (stdout);

		/* read() and take() always "loan" the data, we need to
		 * return it so CoreDX can release resources associated
		 * with it.  
		 */
		retval = DDS_DataReader_return_loan (dr4, &samples, &samples_info);
		if (retval != DDS_RETCODE_OK)
			printf("ERROR (%s): unable to return loan of samples\n",
								DDS_error(retval));
	}
	else
		printf("ERROR (%s) taking samples from DataReader\n", DDS_error(retval));
}

/****************************************************************
 * Create a DataReaderListener with a pointer to the function 
 * that should be called on data_available events.  All other 
 * function pointers are NULL. (no listener method defined).
 ****************************************************************/
DDS_DataReaderListener drListener4 = 
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_data_avail_t4,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL,
	NULL
};


/****************************************************************
 * main()
 *
 * Perform DDS setup activities:
 *   - create a Domain Participant
 *   - create a Subscriber
 *   - register the data types
 *   - create a Topic
 *   - create a DataReader and attach the listener created above
 * And wait for data
 ****************************************************************/

int main(void)
{
	DDS_DomainParticipant    domain;
	DDS_Subscriber           subscriber;
	DDS_Topic                topic1, topic2, topic3, topic4;
	DDS_TopicDescription	 topic_desc;
	DDS_DataReader           dr1, dr2, dr3, dr4;
	DDS_ReturnCode_t         retval;

#ifdef INTERACTIVE
	DDS_Debug_start ();
	DDS_Debug_abort_enable (&all_done);
#endif
	DDS_entity_name ("Technicolor Subscriber test");

	/* create a DDS_DomainParticipant */
	domain = DDS_DomainParticipantFactory_create_participant (0, 
						 DDS_PARTICIPANT_QOS_DEFAULT,
						 NULL,
						 0);
	if (domain == NULL) {
		printf("ERROR creating domain participant.\n");
		return -1;
	}

	/* create a DDS_Subscriber */
	subscriber = DDS_DomainParticipant_create_subscriber (domain, 
						  DDS_SUBSCRIBER_QOS_DEFAULT,
						  NULL,
						  0 );
	if (subscriber == NULL) {
		printf("ERROR creating subscriber\n");
		return -1;
	}
  
	/* Register the Tst1Msg type with the middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	retval = Tst1MsgTypeSupport_register_type( domain, NULL);
	if (retval != DDS_RETCODE_OK) {
		printf("ERROR (%s) registering type\n", DDS_error(retval));
		return -1;
	}

	/* create a DDS Topic with the Tst2Msg data type. */
	topic1 = DDS_DomainParticipant_create_topic (domain,
						"tst1Topic", 
						"Tst1Msg",
						DDS_TOPIC_QOS_DEFAULT,
						NULL,
						0);
	if (!topic1) {
		printf("ERROR creating topic\n");
		return -1;
	}

	/* create a DDS_DataReader on the topic
	 * with default QoS settings, and attach our 
	 * listener with the on_data_available callback set.
	 */
	topic_desc = DDS_DomainParticipant_lookup_topicdescription (
					domain,
					"tst1Topic");
	dr1 = DDS_Subscriber_create_datareader (subscriber,
					topic_desc,
					DDS_DATAREADER_QOS_DEFAULT,
					&drListener1, 
					DDS_DATA_AVAILABLE_STATUS);
	if (!dr1) {
		printf("ERROR creating data reader\n");
		return -1;
	}

	/* Register the Tst2Msg type with the middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	retval = Tst2MsgTypeSupport_register_type (domain, NULL);
	if (retval != DDS_RETCODE_OK) {
		printf("ERROR (%s) registering type\n", DDS_error(retval));
		return -1;
	}

	/* create a DDS Topic with the Tst2Msg data type. */
	topic2 = DDS_DomainParticipant_create_topic (domain,
						     "tst2Topic", 
						     "Tst2Msg",
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL,
						     0);
	if (!topic2) {
		printf("ERROR creating topic\n");
		return -1;
	}

	/* create a DDS_DataReader on the topic
	 * with default QoS settings, and attach our 
	 * listener with the on_data_available callback set.
	 */
	topic_desc = DDS_DomainParticipant_lookup_topicdescription (
					domain,
					"tst2Topic");
	dr2 = DDS_Subscriber_create_datareader (subscriber,
					topic_desc,
					DDS_DATAREADER_QOS_DEFAULT,
					&drListener2, 
					DDS_DATA_AVAILABLE_STATUS);
	if (!dr2) {
		printf("ERROR creating data reader\n");
		return -1;
	}

	/* Register the Tst3Msg type with the middleware. 
	 * This is required before creating a Topic with
	 * this data type. 
	 */
	retval = Tst3MsgTypeSupport_register_type (domain, NULL);
	if (retval != DDS_RETCODE_OK) {
		printf ("ERROR (%s) registering type\n", DDS_error(retval));
		return -1;
	}

	/* create a DDS Topic with the StringMsg data type. */
	topic3 = DDS_DomainParticipant_create_topic (domain, 
						     "tst3Topic", 
						     "Tst3Msg",
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL, 
						     0);
	if (!topic3) {
		printf("ERROR creating topic\n");
		return -1;
	}

	/* create a DDS_DataReader on the topic
	 * with default QoS settings, and attach our 
	 * listener with the on_data_available callback set.
	*/
	topic_desc = DDS_DomainParticipant_lookup_topicdescription (
					domain,
					"tst3Topic");
	dr3 = DDS_Subscriber_create_datareader (subscriber,
					topic_desc,
					DDS_DATAREADER_QOS_DEFAULT,
					&drListener3, 
					DDS_DATA_AVAILABLE_STATUS);
	if (!dr3) {
		printf("ERROR creating data reader\n");
		return -1;
	}

	/* Register the Tst4Msg type with the CoreDX middleware. 
	* This is required before creating a Topic with
	* this data type. 
	*/
	retval = Tst4MsgTypeSupport_register_type (domain, NULL);
	if (retval != DDS_RETCODE_OK) {
		printf ("ERROR (%s) registering type\n", DDS_error(retval));
		return -1;
	}

	/* create a DDS Topic with the Tst4Msg data type. */
	topic4 = DDS_DomainParticipant_create_topic (domain, 
						     "tst4Topic", 
						     "Tst4Msg",
						     DDS_TOPIC_QOS_DEFAULT, 
						     NULL, 
						     0);
	if (!topic4) {
		printf("ERROR creating topic\n");
		return -1;
	}

	/* create a DDS_DataReader on the topic
	 * with default QoS settings, and attach our 
	 * listener with the on_data_available callback set.
	 */
	topic_desc = DDS_DomainParticipant_lookup_topicdescription (
					domain,
					"tst4Topic");
	dr4 = DDS_Subscriber_create_datareader (subscriber,
					topic_desc,
					DDS_DATAREADER_QOS_DEFAULT,
					&drListener4, 
					DDS_DATA_AVAILABLE_STATUS);
	if (!dr4) {
		printf("ERROR creating data reader\n");
		return -1;
	}

	/* Wait forever.  When data arrives at one of our DataReaders, 
	* the appropriate dr_on_data_available method will be invoked.
	*/
	while (!all_done)
		SLEEP (1);

	/* Cleanup */
	DDS_DomainParticipant_delete_contained_entities(domain);
	DDS_DomainParticipantFactory_delete_participant(domain);

	return 0;
}						
