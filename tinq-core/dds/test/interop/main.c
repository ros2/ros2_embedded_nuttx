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
#include <ctype.h>
#include "assert.h"
#include <semaphore.h>
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_trans.h"
#include "dds/dds_aux.h"
#ifdef XTYPES_USED
#include "dds/dds_xtypes.h"
#endif

#include "tsm_types.h"

#define INTEROP

int reader = 0;
int writer = 0;
int key;
int test = 0;
int tsm_type = 0;

DDS_DomainParticipant		part;
DDS_Topic		topic;
DDS_TopicDescription	topic_desc;
DDS_StatusMask			sm;
static sem_t _sync;

DDS_Subscriber		sub;
DDS_SubscriberQos	sqos;
DDS_DataReader		dr;

DDS_Publisher		pub;
DDS_PublisherQos	pqos;
DDS_DataWriter    dw;

typedef struct msg_data_st {
	uint64_t	counter;
	uint32_t	key;
	char		message [128];
} MsgData_t;

uint32_t calculate_member_id(const char *name)
{
    uint32_t crc = crc32(0, name, strlen(name));

    crc &= 0x0FFFFFFF;
    if (crc < 2) {
        crc += 2;
    }
    return crc;
}

static DDS_TypeSupport_meta msg_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 1, "HelloWorldData", sizeof (struct msg_data_st), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "counter", 0, offsetof (struct msg_data_st, counter), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  1, "key", 0, offsetof (struct msg_data_st, key), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "message", 128, offsetof (struct msg_data_st, message), 0, 0, NULL }
};


void usage (void)
{
	printf ("========\r\n");
	printf ("= Help =\r\n");
	printf ("========\r\n\r\n");
	printf ("-t <nb> run test <nb>\r\n");
	printf ("   test 0: write one sample, unregister sample (must have -w & -k)\r\n");
	printf ("   test 1: write infinite samples, unregister each sample (must have -w & -k)\r\n");
	printf ("   test 2: write infinite samples, dispose each sample (must have -w & -k)\r\n");
	printf ("   test 3: Qeo-c interop test (must have -k)\r\n");
	printf ("-r create a reader\r\n");
	printf ("-w create a writer\r\n");
	printf ("-k <int key> set the key value\r\n");
	printf ("examples: \r\n");
	printf ("     -r: create a reader (ctrl+c to stop) \r\n");
	printf ("     -t 0 -w -k <nb>: run test 0 with a writer\r\n");
	printf ("     -t 1 -r -w -k <nb>: run test 1 with a writer and reader\r\n");
	printf ("     -t 3 -k <nb>: run interop test (automatically creates reader and writer\r\n");
}


int get_num (const char **cpp,
	     unsigned   *num, 
	     unsigned   min,
	     unsigned   max,
	     const char *name)
{
	const char	*cp = *cpp;

	while (isspace ((unsigned char) *cp))
		cp++;
	if (*cp < '0' || *cp > '9') {
		fprintf (stderr, "option %s requires a numeric argument!\r\n", name);
		return (0);
	}
	*num = (unsigned) atoi (cp);
	if (*num < min || *num > max) {
		fprintf (stderr, "option %s argument out of range (minimum=%u, maximum=%u)!\r\n", name, min, max);
		return (0);
	}
	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

int get_str (const char **cpp, const char **name)
{
	const char	*cp = *cpp;

	while (isspace (*cp))
		cp++;

	*name = cp;
	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp;
	const char      *arg_input;

	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'r':
					reader = 1;
					break;
				case 'w':
					writer = 1;
					break;
			        case 'k':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					key = atoi (arg_input);
					break;
			        case 't':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					test = atoi (arg_input);					
					break;
			        case 'n': 
					tsm_type = 1;
					break;
				default:
					usage ();
					break;
			}
		}
	}
	return (i);
}

static DDS_TypeSupport	dds_HelloWorld_ts;

DDS_ReturnCode_t register_HelloWorldData_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	dds_HelloWorld_ts = DDS_DynamicType_register (msg_data_tsm);
        if (!dds_HelloWorld_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, dds_HelloWorld_ts, "HelloWorldData");
	return (error);
}

void free_HelloWorldData_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	if (!dds_HelloWorld_ts)
		return;

	error = DDS_DomainParticipant_unregister_type (part, dds_HelloWorld_ts, "HelloWorldData");
	if (error) {
		printf ("DDS_DomainParticipant_unregister_type() failed! (error=%u)\r\n", error);
		return;
	}
	DDS_DynamicType_free (dds_HelloWorld_ts);
	dds_HelloWorld_ts = NULL;
}

static DDS_TypeSupport dds_TestType_ts;

DDS_ReturnCode_t register_TestType_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	dds_TestType_ts = DDS_DynamicType_register (_tsm_types);
        if (!dds_TestType_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, dds_TestType_ts, _tsm_types->name);
	return (error);
}

void free_TestType_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	if (!dds_TestType_ts)
		return;

	error = DDS_DomainParticipant_unregister_type (part, dds_TestType_ts, _tsm_types->name);
	if (error) {
		printf ("DDS_DomainParticipant_unregister_type() failed! (error=%u)\r\n", error);
		return;
	}
	DDS_DynamicType_free (dds_TestType_ts);
	dds_TestType_ts = NULL;
}

const char *kind_str [] = {
	NULL,
	"ALIVE",
	"NOT_ALIVE_DISPOSED",
	NULL,
	"NOT_ALIVE_NO_WRITERS"
};

void dr_listener_data_available (DDS_DataReaderListener *list,
				 DDS_DataReader         dr)
{
	MsgData_t *data;
	types_t *datax;
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_ReturnCode_t error;
	DDS_SampleInfo *info;

	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);

	error = DDS_DataReader_take_next_instance(dr, &rx_sample, &rx_info, 1, 
						  DDS_HANDLE_NIL, 
						  DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
						  DDS_ANY_INSTANCE_STATE);

	printf ("dds-interop: Take next instance.\r\n");
	
	if (error)
		printf ("Error taking next instance\r\n");


	/* === print sample === */
  
	if (DDS_SEQ_LENGTH (rx_info)) {
		data = DDS_SEQ_ITEM (rx_sample, 0);
		datax = DDS_SEQ_ITEM (rx_sample, 0);
		info = DDS_SEQ_ITEM (rx_info, 0);
		if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
			if (tsm_type == 0)
				printf ("DDS-R: [%2u] ALIVE - %2u :%6llu - %s\r\n", 
					info->instance_handle,
					data->key,
					(unsigned long long) data->counter,
					data->message);
			else
				printf ("DDS-R: [%2u] ALIVE - %2u -  %s\r\n", 
					info->instance_handle, datax->i8, datax->string);
			if (test == 3)
				sem_post (&_sync);
		}
		else if (info->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE) {
			if (tsm_type == 0)
				printf ("DDS-R: The sample %s with key (%d) has been disposed.\r\n", data->message,  data->key);
			else 
				printf ("DDS-R: The sample has been disposed.\r\n", datax->string);
			if (test == 3)
				sem_post (&_sync);
		}
		else if (info->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) {
			printf ("DDS-R: There are no more writers.\r\n");
			if (test == 3)
				sem_post (&_sync);
		}
		else
			printf ("DDS-R: Unknown state.\r\n");
	}
	
	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);

}

static void make_state_reader ()
{
	DDS_DataReaderQos	rd_qos;
	DDS_ReturnCode_t error;

	printf ("Create Reader\r\n");

	/* === Create a subscriber === */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}

	/* Test get_qos() fynctionality. */
	if ((error = DDS_Subscriber_get_qos (sub, &sqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Subscriber_get_qos () failed (%s)!", DDS_error (error));

	/* Setup reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);

	/* === create the reader === */
	
	rd_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
        rd_qos.history.depth = 1;
        rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        rd_qos.durability.kind  = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
        rd_qos.ownership.kind   = DDS_EXCLUSIVE_OWNERSHIP_QOS;

	dr = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, NULL, sm);
	if (!dr)
		fatal ("DDS_DomainParticipant_create_datareader () returned an error!");

	DDS_DataReaderListener *l = DDS_DataReader_get_listener(dr);
	if (NULL != l) {
		l->on_data_available  = dr_listener_data_available;
		error = DDS_DataReader_set_listener(dr, l, DDS_DATA_AVAILABLE_STATUS);
	}
	error = DDS_DataReader_enable(dr);
			
	/* === end program === */

	if (test == 3)
		return;

	if (!writer)
		while (1)
			sleep (1);
}

static void remove_state_reader ()
{
	DDS_ReturnCode_t error;

	error = DDS_DataReader_delete_contained_entities (dr);
	if (error)
		fatal ("DDS_DataReader_delete_contained_entities() failed (%s)!", DDS_error (error));

	DDS_DataReader_set_listener (dr, NULL, 0);
	error = DDS_Subscriber_delete_datareader (sub, dr);
	if (error)
		fatal ("DDS_Subscriber_delete_datareader() failed (%s)!", DDS_error (error));

}

static void make_state_writer (int id)
{
	DDS_DataWriterQos wr_qos;
	DDS_ReturnCode_t error;
	static DDS_InstanceHandle_t handle;
	MsgData_t data;
	

	printf ("Create Writer\r\n");

	/* === create publisher === */

	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		fatal ("DDS_DomainParticipant_create_publisher () failed!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}

	/* Test get_qos() fynctionality. */
	if ((error = DDS_Publisher_get_qos (pub, &pqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Publisher_get_qos () failed (%s)!", DDS_error (error));

	/* Setup writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);

	/* === create the writer === */

	wr_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
        wr_qos.history.depth = 1;
        wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
        wr_qos.durability.kind  = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
        wr_qos.ownership.kind   = DDS_EXCLUSIVE_OWNERSHIP_QOS;

	dw = DDS_Publisher_create_datawriter (pub, topic, &wr_qos, NULL, sm);
	if (!dw) {
		fatal ("Unable to create a writer \r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}

	/* === While true, add sample, remove sample === */
	if (tsm_type == 0) {
		data.key = id;
		data.counter = 100 + id;
		strcpy (data.message, "Hi folks!");
		
		if (test == 3) {
			printf ("Register instance\r\n");
			DDS_DataWriter_register_instance (dw, &data);
			printf ("Write data\r\n");
			if ((error = DDS_DataWriter_write (dw, &data, DDS_HANDLE_NIL)))
				printf ("Error writing data\r\n");
			
			sleep (5);
			
			sem_wait (&_sync);
			sem_wait (&_sync);
			
			printf ("Unregister instance\r\n");
			if ((error = DDS_DataWriter_unregister_instance(dw, &data, DDS_HANDLE_NIL)))
				printf ("Error unregistering instance\r\n");
			
			sleep (5);
			
			sem_wait (&_sync);
			sem_wait (&_sync);
			
			return;
		}
	} else {
		if (test == 3) {
			_types1.i8 = key;
			printf ("Register instance\r\n");
			DDS_DataWriter_register_instance (dw, &_types1);
			printf ("Write data\r\n");
			if ((error = DDS_DataWriter_write (dw, &_types1, DDS_HANDLE_NIL)))
				printf ("Error writing data\r\n");
			
			sleep (5);
			
			sem_wait (&_sync);
			sem_wait (&_sync);
			
			printf ("Unregister instance\r\n");
			if ((error = DDS_DataWriter_unregister_instance(dw, &_types1, DDS_HANDLE_NIL)))
				printf ("Error unregistering instance\r\n");
			
			sleep (5);
			
			sem_wait (&_sync);
			sem_wait (&_sync);
			
			return;
		}
	}

	if (test == 0) {
		printf ("Register instance\r\n");
		DDS_DataWriter_register_instance (dw, &data);
		printf ("Write data\r\n");
		if ((error = DDS_DataWriter_write (dw, &data, DDS_HANDLE_NIL)))
			printf ("Error writing data\r\n");
		
		sleep (2);
		printf ("Unregister instance\r\n");
		if ((error = DDS_DataWriter_unregister_instance(dw, &data, DDS_HANDLE_NIL)))
			printf ("Error unregistering instance\r\n");
		
		sleep (2);
		if (reader)
			remove_state_reader ();
	} else if (test >= 1) {
		while (1) {
			printf ("Register instance\r\n");
			handle = DDS_DataWriter_register_instance (dw, &data);
			printf ("Write data\r\n");
			if ((error = DDS_DataWriter_write (dw, &data, handle)))
				printf ("Error writing data\r\n");
			sleep (1);
			if (test == 1) {
				printf ("Unregister instance\r\n");
				if ((error = DDS_DataWriter_unregister_instance(dw, &data, handle)))
					printf ("Error unregistering instance\r\n");
			} else {
				printf ("Dispose instance\r\n");
				if ((error = DDS_DataWriter_dispose (dw, &data, handle)))
					printf ("Error disposing instance\r\n");
			}
			sleep (1);
		}
	}
}
 
 static void remove_state_writer ()
 {
	DDS_ReturnCode_t error;

	DDS_DataWriter_set_listener (dw, NULL, 0);
	if ((error = DDS_Publisher_delete_datawriter (pub, dw)))
		fatal ("DDS_Publisher_delete_datawriter() failed (%s)!", DDS_error (error));

	if ((error = DDS_Publisher_delete_contained_entities (pub)))
		fatal ("DDS_Publisher_delete_contained_entities () failed (%s)!", DDS_error (error));
}
 
int main (int argc, char *argv [])
{
	int domain_id = 128;
	DDS_ReturnCode_t error;

	do_switches (argc, argv);

#ifdef DDS_DEBUG
	DDS_Debug_start ();
#endif
	
	DDS_set_generate_callback (calculate_member_id);
		
	sem_init(&_sync, 0, 0);

	part = DDS_DomainParticipantFactory_create_participant (domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

	if (tsm_type == 0) {
		error = register_HelloWorldData_type (part);
		if (error) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("DDS_DomainParticipant_register_type ('HelloWorldData') failed (%s)!", DDS_error (error));
		}

		sm = DDS_INCONSISTENT_TOPIC_STATUS;
		topic = DDS_DomainParticipant_create_topic (part, "HelloWorld", "HelloWorldData",
							    NULL, NULL, sm);
		if (!topic) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("DDS_DomainParticipant_create_topic ('HelloWorld') failed!");
		}
		
		topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "HelloWorld");
		if (!topic_desc) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("Unable to create topic description for 'HelloWorld'!");
		}
	} else {
		_tsm_types[0].flags |= TSMFLAG_KEY;
		error = register_TestType_type (part);
		if (error) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("DDS_DomainParticipant_register_type ('HelloWorldData') failed (%s)!", DDS_error (error));
		}

		sm = DDS_INCONSISTENT_TOPIC_STATUS;
		topic = DDS_DomainParticipant_create_topic (part, _tsm_types->name, _tsm_types->name,
							    NULL, NULL, sm);
		if (!topic) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("DDS_DomainParticipant_create_topic ('%s') failed!", _tsm_types->name);
		}
		
		topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, _tsm_types->name);
		if (!topic_desc) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("Unable to create topic description for '%s'!", _tsm_types->name);
		}
	}

	if (test == 3) {
		/* interop test */
		make_state_reader ();
		make_state_writer (key);
	} else {	
		if (reader)
			make_state_reader ();
		if (writer)
			make_state_writer (key);
	}
	
	if (reader)
		remove_state_reader ();
	if (writer)
		remove_state_writer ();

	if (test == 3) {
		remove_state_reader ();
		remove_state_writer ();
	}

	error = DDS_DomainParticipant_delete_topic (part, topic);
	if (error)
		fatal ("DDS_DomainParticipant_delete_topic () failed (%s)!", DDS_error (error));

	free_HelloWorldData_type (part);
	
	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	return (0);

}
