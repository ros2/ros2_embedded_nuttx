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

/**
 * Test program to validate the wait set functionality within uDDS
 */

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#include <dds/dds_dcps.h>
#include <debug.h>

/* Definitions */
#define	DOMAIN_ID	0
#define MSG_SIZE	100
#define MAX_DSIZE	0x20000		/* 128KB */

/* Globals */
int			verbose = 1;		/* Verbose if set. */
DDS_Topic		topic;
DDS_TopicDescription	topic_desc;
unsigned		count = ~0;		/* Max. # of times to send. */
int			aborting;		/* Abort program if set. */
const char		*progname;
unsigned		data_size;		/* Default: {"hello world", count} */
unsigned		max_size;		/* Max. data size. */
int			quit_done;		/* Quit when Tx/Rx done. */
int			trace;			/* Trace messages if set. */

const char *kind_str [] = {
	NULL,
	"ALIVE",
	"NOT_ALIVE_DISPOSED",
	NULL,
	"NOT_ALIVE_UNREGISTERED"
};

#ifndef DDS_DEBUG
unsigned max_events = ~0;
unsigned sleep_time = 1000;
int pause_traffic = 0;
#endif


/**
 * Prints the formatted message and aborts execution
 */
void fatal_printf (const char *format , ... ) {
	va_list arglist;
	va_start( arglist, format );
	vprintf( format, arglist );
	va_end( arglist );
	printf("\r\n");
	exit (1);
}

/*********** The HelloWorld data type ***************/
typedef struct msg_data_st {
	uint32_t	counter;
	uint32_t	key;
	char		message [MSG_SIZE];
} MsgData_t;

static DDS_TypeSupport_meta msg_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "HelloWorldData", sizeof (struct msg_data_st), 0, 3 },
	{ CDR_TYPECODE_ULONG,  0, "counter", 0, offsetof (struct msg_data_st, counter), 0},
	{ CDR_TYPECODE_ULONG,  1, "key", 0, offsetof (struct msg_data_st, key), 0 },
	{ CDR_TYPECODE_CSTRING,0, "message", MSG_SIZE, offsetof (struct msg_data_st, message), 0 }
};

DDS_ReturnCode_t register_HelloWorldData_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;
	DDS_TypeSupport		*dds_ts;

	dds_ts = DDS_DynamicType_register (msg_data_tsm);
        if (!dds_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, dds_ts, "HelloWorldData");
	return (error);
}


/**
 * usage -- Print out program usage.
 */
void usage (void)
{
	fprintf (stderr, "dds -- test program for the DDS protocol.\r\n");
	fprintf (stderr, "Usage: dds [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -s <size>	Data size to write in writer mode.\r\n");
	fprintf (stderr, "		Default it sends a 4-byte counter, followed\r\n");
	fprintf (stderr, "		by a 'hello world' string.\r\n");
	fprintf (stderr, "   -m <size>	Maximum data size in writer mode.\r\n");
	fprintf (stderr, "		If set, will continuously increment the data\r\n");
	fprintf (stderr, "		until the maximum size and restarts with the\r\n");
	fprintf (stderr, "		minimum size.\r\n");
	fprintf (stderr, "   -n <count>	Max. # of times to send/receive data.\r\n");
	fprintf (stderr, "   -q         Quit when all packets sent/received.\r\n");
	fprintf (stderr, "   -f		Flood mode (no waiting: as fast as possible).\r\n");
	fprintf (stderr, "   -d <msec>	Max. delay to wait for responses (10..10000).\r\n");
	fprintf (stderr, "   -p         Startup in paused state.\r\n");
	fprintf (stderr, "   -t		Trace transmitted/received messages.\r\n");
	fprintf (stderr, "   -v		Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv	Extra verbose: log detailed functionality.\r\n");
	exit (1);
}


/**
 * get_num -- Get a number from the command line arguments.
 */
int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max)
{
	const char	*cp = *cpp;

	while (isspace (*cp))
		cp++;
	if (*cp < '0' || *cp > '9')
		return (0);

	*num = (unsigned) atoi (cp);
	if (*num < min || *num > max)
		return (0);

	while (*cp)
		cp++;

	*cpp = cp;
	return (1);
}

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/**
 *  do_switches -- Command line switch decoder.
 */
int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp;

	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 's':
					INC_ARG()
					if (!get_num (&cp, &data_size, 1, MAX_DSIZE))
						usage ();
					break;
				case 'm':
					INC_ARG()
					if (!data_size ||
					    !get_num (&cp, &max_size, data_size + 1, MAX_DSIZE))
						usage ();
					break;
				case 'n':
					INC_ARG()
					if (!get_num (&cp, &count, 0, ~0))
						usage ();
					break;
				case 'f':
					sleep_time = 0;
					break;
				case 'd':
					INC_ARG()
					if (!get_num (&cp, &sleep_time, 10, 10000))
						usage ();
					break;
				case 'p':
					pause_traffic = 1;
					break;
				case 'q':
					quit_done = 1;
					break;
				case 't':
					trace = 1;
					break;
				case 'v':
					verbose = 1;
					if (*cp == 'v') {
						verbose = 2;
						cp++;
					}
					break;
				default:
					usage ();
				break;
			}
		}
	}
	return (i);
}

/**
 * DDS reader handling
 */
void dcps_do_reader_waitset (DDS_DomainParticipant part)
{
	DDS_Subscriber		sub;
	DDS_DataReader		dr;
	DDS_DataReaderQos	rd_qos;
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;

	DDS_WaitSet		ws;
	DDS_Condition		c;
	DDS_Duration_t		duration;
	DDS_ConditionSeq        c_seq = DDS_SEQ_INITIALIZER(DDS_Condition);
	unsigned		i, nchanges, j;
	DDS_SampleInfo		*info;
	MsgData_t		*sample;
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);

	/* Create a subscriber */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		fatal_printf ("DDS_DomainParticipant_create_subscriber () returned an error!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

	/* Setup reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create a datareader. */
	dr = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, NULL, 0);
	if (!dr) {
		fatal_printf ("DDS_DomainParticipant_create_datareader () returned an error!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Reader created.\r\n");

	c = DDS_DataReader_create_readcondition ( dr, ss, vs, is);

	ws = DDS_WaitSet__alloc();
	error =  DDS_WaitSet_attach_condition (ws, c);
	if (error)
		fatal_printf ("DDS_WaitSet_attach_condition() failed! (error=%u)", error);

	 duration.sec = 60;
	 duration.nanosec = 0;

	for (i = 0, j = 0; i < count && !aborting; j++) {
		/* Wait for the conditions to occur. */
		error = DDS_WaitSet_wait (ws, &c_seq, &duration);

		if (error == DDS_OK) {
			/* Resetting the length of the conditions sequence to 0. */
			c_seq.length = 0;

			/* A condition has been triggered. Read one sample. */
			error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
			if (error) {
				printf ("Unable to read samples!\r\n");
				break;
			}
			if (rx_info.length) {
				sample = DDS_SEQ_ITEM (rx_sample, 0);
				info = DDS_SEQ_ITEM (rx_info, 0);
				nchanges = 1;
				if (verbose) {
					if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
						printf ("DDS-R: [%2u] ALIVE - %2u :%6u - '%s'\r\n",
								info->instance_handle,
								sample->key,
								sample->counter,
								sample->message);
					}
					else
						printf ("DDS-R: [%2u] %s\r\n",
								info->instance_handle,
								kind_str [info->instance_state]);
				}
				DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
				i += nchanges;
			}
		}
		else if (error == DDS_ERR_TIMEOUT)
			printf("Timeout.\r\n");
		else
			fatal_printf ("DDS_WaitSet_wait() failed! (error=%u)", error);
	}

	error =  DDS_DataReader_delete_readcondition (dr, c);
	if (error)
		fatal_printf ("DDS_DataReader_delete_readcondition() failed! (error=%u)", error);

	DDS_WaitSet__free(ws);

	error = DDS_Subscriber_delete_datareader (sub, dr);
	if (error)
		fatal_printf ("DDS_Subscriber_delete_datareader() failed! (error=%u)", error);

	if (verbose)
		printf ("DDS Reader deleted.\r\n");
}


int main (int argc, const char *argv []) {
	DDS_DomainParticipant	part;
	DDS_PoolConstraints	reqs;
	int error = 1, arg_index;

	if (verbose)
		printf("Wait set validation...\n");

	arg_index = do_switches (argc, argv);

	DDS_get_default_pool_constraints (&reqs, 0, 0);
	reqs.min_local_readers = reqs.max_local_readers = 4;
	reqs.min_local_writers = reqs.max_local_writers = 7;
	reqs.min_changes = 64;
	reqs.min_instances = 48;
	DDS_set_pool_constraints (&reqs);

#ifdef DDS_DEBUG
	if (isatty (STDIN_FILENO)) {
		debug_init ();
		debug_abort (&aborting);
	}
#endif

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						DOMAIN_ID, NULL, NULL, 0);
	if (!part)
		fatal_printf ("DDS_DomainParticipantFactory_create_participant () failed!\r\n");

	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

	/* Register the message topic type. */
	error = register_HelloWorldData_type (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal_printf ("DDS_DomainParticipant_register_type ('%s') failed!\r\n", "HelloWorldData");
	}
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "HelloWorldData");

	/* Create a topic */
	topic = DDS_DomainParticipant_create_topic (part, "HelloWorld", "HelloWorldData",
									NULL, NULL, 0);
	if (!topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal_printf ("DDS_DomainParticipant_create_topic ('HelloWorld') failed!\r\n");
	}
	if (verbose)
		printf ("DDS Topic created.\r\n");

	/* Create Topic Description. */
	topic_desc = TopicDescription_from_Topic (topic);
	if (!topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal_printf ("Unable to create topic description for 'HelloWorld'!\r\n");
	}

	/** Do Wait Set validation in the reader handling **/
	dcps_do_reader_waitset(part);
	/*dcps_do_reader_listener(part);*/

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal_printf ("DDS_DomainParticipant_delete_contained_entities () failed: error = %d", error);

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal_printf ("DDS_DomainParticipantFactory_delete_participant () failed: error = %d", error);

	if (verbose)
		printf ("DDS Participant deleted\r\n");

	return 0;
}
