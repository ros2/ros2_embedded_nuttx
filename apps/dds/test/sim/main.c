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

/* main.c -- Test program to test the DDS/DCPS functionality. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "log.h"
#include "error.h"
#include "debug.h"
#include "rtps_mux.h"
#include "dds/dds_dcps.h"
#include "test.h"

#define	HELLO_WORLD	"Hello DDS world!"
#define	DOMAIN_ID	0
#define MAX_DSIZE	0x20000		/* 128KB */
#define MSG_SIZE	100

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
#define TRACE_DATA	/* Define to trace data endpoints. */
#define EXTRA_READER	/* Define to create an extra reader in writer mode. */
#define DO_UNREGISTER	/* Define to unregister instances. */
/*#define DO_DISC_LISTEN ** Listen on discovery info. */
#define USE_SHAPE_TYPE	/* Use the Square/ShapeType topic. */

const char		*progname;
int			writer;			/* Default: reader. */
int			verbose;		/* Verbose if set. */
int			trace;			/* Trace messages if set. */
int			aborting;		/* Abort program if set. */
int			quit_done;		/* Quit when Tx/Rx done. */
unsigned		data_size;		/* Default: {"hello world", count} */
unsigned		max_size;		/* Max. data size. */
unsigned		nsamples;		/* Sample to send. */
size_t			cur_size;		/* Current sample size. */
unsigned		count = ~0;		/* Max. # of times to send. */
Timer_t			send_timer;		/* Write mode: timer for next sample. */
unsigned char		buf [MAX_DSIZE];	/* Data buffer to use. */
DDS_Topic		topic;
DDS_TopicDescription	topic_desc;
DDS_DataWriter		w;
const char *kind_str [] = {
	NULL,
	"ALIVE",
	"NOT_ALIVE_DISPOSED",
	NULL,
	"NOT_ALIVE_UNREGISTERED"
};

#ifndef DDS_DEBUG
int pause_traffic = 0;
unsigned max_events = ~0;
unsigned sleep_time = 1000;
#endif

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "dds -- test program for the DDS protocol.\r\n");
	fprintf (stderr, "Usage: dds [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -r		Act as a reader (default).\r\n");
	fprintf (stderr, "   -w		Act as a writer.\r\n");
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

/* get_num -- Get a number from the command line arguments. */

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

/* do_switches -- Command line switch decoder. */

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
				case 'r':
					writer = 0;
					break;
				case 'w':
					writer = 1;
					break;
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

typedef struct msg_data_st {
	uint32_t	counter;
	uint32_t	key;
	char		message [MSG_SIZE];
} MsgData_t;

#define	NMSGS	4

typedef struct msg_desc_st {
	unsigned	key;
	const char	*data;
	DDS_InstanceHandle_t handle;
} MsgDesc_t;

MsgDesc_t messages [NMSGS] = {
	{ 0, "Hello DDS world!", 0},
	{ 6, "Aint't this a pretty sight?", 0 },
	{ 22, "Having fun with a mighty middleware :-)", 0 },
	{ 33, "And the last one to conclude the deal!", 0 }
};

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

#ifdef USE_SHAPE_TYPE

typedef struct shape_type_st {
	char		color [128];
	int		x;
	int		y;
	int		shapesize;
} ShapeType_t;

static DDS_TypeSupport_meta shape_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "ShapeType", sizeof (struct shape_type_st), 0, 4 },
	{ CDR_TYPECODE_CSTRING,1, "color", 128, offsetof (struct shape_type_st, color), 0 },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct shape_type_st, x), 0},
	{ CDR_TYPECODE_LONG,   0, "y", 0, offsetof (struct shape_type_st, y), 0},
	{ CDR_TYPECODE_LONG,   0, "shapesize", 0, offsetof (struct shape_type_st, shapesize), 0 }
};

DDS_ReturnCode_t register_ShapeType_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;
	DDS_TypeSupport		*dds_ts;

	dds_ts = DDS_DynamicType_register (shape_tsm);
        if (!dds_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, dds_ts, "ShapeType");
	return (error);
}

#endif

/* trace_data -- Dump data in a nicely readable format. */

void trace_data (void *data, size_t len)
{
	unsigned char	c, *dp = (unsigned char *) data;
	unsigned	i;
	char		ascii [17];

	ascii [16] = '\0';
	for (i = 0; i < len; i++) {
		if ((i & 0xf) == 0) {
			if (i)
				log_printf (LOG_DEF_ID, 0, "   %s\r\n", ascii);
			log_printf (LOG_DEF_ID, 0, "  %05u:", i);
		}
		else if ((i & 0x7) == 0)
			log_printf (LOG_DEF_ID, 0, " -");
		c = *dp++;
		ascii [i & 0xf] = (c >= ' ' && c <= '~') ? c : '.';
		log_printf (LOG_DEF_ID, 0, " %02x", c);
	}
	while ((i & 0xf) != 0) {
		log_printf (LOG_DEF_ID, 0, "   ");
		if ((i & 0x7) == 0)
			log_printf (LOG_DEF_ID, 0, "  ");
		ascii [i & 0xf] = ' ';
		i++;
	}
	log_printf (LOG_DEF_ID, 0, "   %s\r\n", ascii);
}

static void do_write (uintptr_t user)
{
	MsgData_t	data;
	MsgDesc_t	*dp;
	unsigned	op, inst_delta, index;
	int		error;
	static DDS_InstanceHandle_t h [4];

	ARG_NOT_USED (user)

	if (pause_traffic) {
		tmr_start (&send_timer, sleep_time / TMR_UNIT_MS, 0, do_write);
		return;
	}
	index = nsamples & 3;
	op = (nsamples & 0xc) >> 2;
	inst_delta = (nsamples & 0x30) >> 4;
	dp = &messages [index];
	data.key = dp->key + inst_delta;
	switch (op) {
		case 0:
			h [index] = DDS_DataWriter_register_instance (w, &data);
			if (verbose)
				printf ("DDS-W: [%2u] Registered instance.\r\n", h [index]);
		case 1:
		case 2:
		case 3:
			if (trace)
				trace_data (buf, cur_size);
			
			data.counter = nsamples;
			if (strlen (dp->data) + 1 > MSG_SIZE) {
				memcpy (data.message, dp->data, MSG_SIZE - 1);
				data.message [MSG_SIZE - 1] = '\0';
			}
			else
				strcpy (data.message, dp->data);

			error = DDS_DataWriter_write (w, &data, h [index]);
			if (error) {
				/*printf ("DDS_DataWriter_write() failed! (error=%u)\r\n", error);*/
				break;
			}
			nsamples++;
			if (verbose)
				printf ("DDS-W: [%2u] ALIVE - %2u :%6u - '%s'\r\n", 
							h [index],
							data.key,
							data.counter,
							data.message);
			if (op < 3)
				break;

#ifdef DO_UNREGISTER
			if (h [index] >= 8) {
				if (nsamples == 45)
					error = 0;
				do {
					error = DDS_DataWriter_unregister_instance (w, &data, h [index]);
					/*if (error)
						printf ("DDS_DataWriter_unregister_instance() failed! (error=%u)\r\n", error);*/
				}
				while (error);
				if (verbose)
					printf ("DDS-W: [%2u] Unregistered instance\r\n", h [index]);
			}
			else {
				do {
					error = DDS_DataWriter_dispose (w, &data, h [index]);
					/*if (error)
						printf ("DDS_DataWriter_dispose() failed! (error=%u)\r\n", error);*/
				}
				while (error);
				if (verbose)
					printf ("DDS-W: [%2u] Disposed instance\r\n", h [index]);
			}
#endif
			break;
	}
	cur_size++;
	if (cur_size > max_size)
		cur_size = data_size;
	if (!--max_events)
		pause_traffic = 1;

	tmr_start (&send_timer, sleep_time / TMR_UNIT_MS, 0, do_write);
}

#ifdef EXTRA_READER

void do_read (DDS_DataReaderListener *l,
	      DDS_DataReader         dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	MsgData_t		*sample;
	DDS_ReturnCode_t	error;
	unsigned		n;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			printf ("Unable to read samples!\r\n");
			return;
		}
		if (rx_info.length) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
					n = sample->counter;
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

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

static DDS_DataReaderListener r_listener = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	do_read,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

#endif

void dcps_do_writer (DDS_DomainParticipant part)
{
	DDS_Publisher		pub;
	char			c;
	int			error;
	unsigned		i;
	DDS_DataWriterQos 	wr_qos;
#ifdef EXTRA_READER
	DDS_DataReader		dr;
	DDS_DataReaderQos	rd_qos;
	DDS_Subscriber		sub;
#endif

	if (!data_size) {
		data_size = sizeof (unsigned long) + sizeof (HELLO_WORLD);
		strcpy ((char *) &buf [sizeof (unsigned long)], HELLO_WORLD);
	}
	else {
		if (!max_size)
			max_size = data_size;
		for (i = sizeof (unsigned long), c = ' ' + 1; i < max_size; i++) {
			buf [i] = c++;
			if (c > '~')
				c = ' ' + 1;
		}
	}

	/* Create a publisher. */
	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		fatal_printf ("DDS_DomainParticipant_create_publisher () failed!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Publisher created.\r\n");

	/* Setup writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a Data Writer. */
	w = DDS_Publisher_create_datawriter (pub, topic, &wr_qos, NULL, 0);
	if (!w) {
		fatal_printf ("Unable to create a writer \r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Writer created.\r\n");

#ifdef EXTRA_READER
	sub = DDS_DomainParticipant_create_subscriber (part, NULL, NULL, 0); 
	if (!sub) {
		fatal_printf ("DDS_DomainParticipant_create_subscriber () returned an error!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create a datareader. */
	dr = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, &r_listener, 0);
	if (!dr) {
		fatal_printf ("DDS_DomainParticipant_create_datareader () returned an error!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Reader created.\r\n");

#endif
	cur_size = data_size;
	nsamples = 0;
	tmr_start (&send_timer, sleep_time / TMR_UNIT_MS, 0, do_write);
	do {
		simulate_reader (nsamples, sleep_time);
	}
	while (nsamples < count && !aborting);

#ifdef EXTRA_READER
	error = DDS_Subscriber_delete_datareader (sub, dr);
	if (error)
		fatal_printf ("DDS_Subscriber_delete_datareader() failed! (error=%u)", error);

	if (verbose)
		printf ("DDS Reader deleted.\r\n");
#endif

	error = DDS_Publisher_delete_datawriter (pub, w);
	if (error)
		fatal_printf ("DDS_Publisher_delete_datawriter() failed! (error=%u)", error);

	if (verbose)
		printf ("DDS Writer deleted.\r\n");
}

void data_read (DDS_DataReaderListener *l,
	        DDS_DataReader         dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	MsgData_t		*sample;
	DDS_ReturnCode_t	error;
	unsigned		n, nchanges;

	ARG_NOT_USED (l)

	error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
	if (error) {
		printf ("Unable to read samples!\r\n");
		return;
	}
	if (rx_info.length) {
		sample = DDS_SEQ_ITEM (rx_sample, 0);
		info = DDS_SEQ_ITEM (rx_info, 0);
		nchanges = 1;
		if (verbose) {
			if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				n = sample->counter;
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
		/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
			trace_data (bufp, data_size);*/
		DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		nsamples += nchanges;
	}
}

static DDS_DataReaderListener rd_listener = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	data_read,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

void dcps_do_reader (DDS_DomainParticipant part)
{
	DDS_Subscriber		sub;
	DDS_DataReader		dr;
	DDS_DataReaderQos	rd_qos;
	DDS_ReturnCode_t	error;

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

#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a datareader. */
	dr = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, &rd_listener, 0);
	if (!dr) {
		fatal_printf ("DDS_DomainParticipant_create_datareader () returned an error!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Reader created.\r\n");

	do {
		simulate_writer (nsamples, sleep_time);
	}
	while (nsamples < count && !aborting);
	error = DDS_Subscriber_delete_datareader (sub, dr);
	if (error)
		fatal_printf ("DDS_Subscriber_delete_datareader() failed! (error=%u)", error);

	if (verbose)
		printf ("DDS Reader deleted.\r\n");
}

#ifdef DO_DISC_LISTEN

static const char	*names [] = {
	"DCPSParticipant",
	"DCPSTopic",
	"DCPSPublication",
	"DCPSSubscription"
};

void dump_key (DDS_BuiltinTopicKey_t *kp)
{
	printf ("%08x:%08x:%08x", kp->value [0], kp->value [1], kp->value [2]);
}

void participant_info (DDS_DataReaderListener *l,
		       DDS_DataReader         dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	DDS_ParticipantBuiltinTopicData tmp;
	DDS_ParticipantBuiltinTopicData *sample;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			printf ("Unable to read samples!\r\n");
			return;
		}
		if (rx_info.length) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("Discovery: ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					printf ("Updated");
				else
					printf ("Deleted");
				printf (" Participant\r\n");
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

void topic_info (DDS_DataReaderListener *l,
		 DDS_DataReader         dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	DDS_TopicBuiltinTopicData tmp;
	DDS_TopicBuiltinTopicData *sample;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			printf ("Unable to read samples!\r\n");
			return;
		}
		if (rx_info.length) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("Discovery: ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					printf ("Updated");
				else
					printf ("Deleted");
				printf (" Topic");
				if (info->valid_data)
					printf (" (%s/%s)", sample->name, sample->type_name);
				printf ("\r\n");
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

void publication_info (DDS_DataReaderListener *l,
		       DDS_DataReader         dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	DDS_PublicationBuiltinTopicData tmp;
	DDS_PublicationBuiltinTopicData *sample;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			printf ("Unable to read samples!\r\n");
			return;
		}
		if (rx_info.length) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("Discovery: ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					printf ("Updated");
				else
					printf ("Deleted");
				printf (" Publication");
				if (info->valid_data)
					printf (" (%s/%s)", sample->topic_name, sample->type_name);
				printf ("\r\n");
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

void subscription_info (DDS_DataReaderListener *l,
		        DDS_DataReader         dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	DDS_SubscriptionBuiltinTopicData tmp;
	DDS_SubscriptionBuiltinTopicData *sample;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			printf ("Unable to read samples!\r\n");
			return;
		}
		if (rx_info.length) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("Discovery: ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					printf ("Updated");
				else
					printf ("Deleted");
				printf (" Subscription");
				if (info->valid_data)
					printf (" (%s/%s)", sample->topic_name, sample->type_name);
				printf ("\r\n");
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

static DDS_DataReaderListener builtin_listeners [] = {{
	NULL,			/* Sample rejected. */
	NULL,			/* Liveliness changed. */
	NULL,			/* Requested Deadline missed. */
	NULL,			/* Requested incompatible QoS. */
	participant_info,	/* Data available. */
	NULL,			/* Subscription matched. */
	NULL,			/* Sample lost. */
	NULL			/* Cookie */
}, {
	NULL,			/* Sample rejected. */
	NULL,			/* Liveliness changed. */
	NULL,			/* Requested Deadline missed. */
	NULL,			/* Requested incompatible QoS. */
	topic_info,		/* Data available. */
	NULL,			/* Subscription matched. */
	NULL,			/* Sample lost. */
	NULL			/* Cookie */
}, {
	NULL,			/* Sample rejected. */
	NULL,			/* Liveliness changed. */
	NULL,			/* Requested Deadline missed. */
	NULL,			/* Requested incompatible QoS. */
	publication_info,	/* Data available. */
	NULL,			/* Subscription matched. */
	NULL,			/* Sample lost. */
	NULL			/* Cookie */
}, {
	NULL,			/* Sample rejected. */
	NULL,			/* Liveliness changed. */
	NULL,			/* Requested Deadline missed. */
	NULL,			/* Requested incompatible QoS. */
	subscription_info,	/* Data available. */
	NULL,			/* Subscription matched. */
	NULL,			/* Sample lost. */
	NULL			/* Cookie */
}};

void start_disc_readers (DDS_DomainParticipant part)
{
	DDS_Subscriber		sub;
	DDS_DataReader		dr;
	unsigned		i;
	DDS_ReturnCode_t	ret;

	sub = DDS_DomainParticipant_get_builtin_subscriber (part);
	if (!sub)
		fatal_printf ("DDS_DomainParticipant_get_builtin_subscriber() returned an error!\r\n");

	if (verbose)
		printf ("DDS Builtin Subscriber found.\r\n");

	for (i = 0; i < sizeof (names) / sizeof (char *); i++) {
		dr = DDS_Subscriber_lookup_datareader (sub, names [i]);
		if (!dr)
			fatal_printf ("DDS_Subscriber_lookup_datareader(%s) returned an error!\r\n", names [i]);

		ret = DDS_DataReader_set_listener (dr, &builtin_listeners [i], 0);
		if (ret)
			fatal_printf ("DDS_DataReader_set_listener(%s) returned an error!\r\n", names [i]);

		if (verbose)
			printf ("DDS Discovery Reader created(%s).\r\n", names [i]);
	}
}

#endif

int main (int argc, const char *argv [])
{
	DDS_DomainParticipant	part;
	DDS_PoolConstraints	reqs;
	int			error, arg_index;

	arg_index = do_switches (argc, argv);

	if (verbose > 1)
		err_actions_add (EL_LOG, ACT_PRINT_STDIO);
	if (trace)
		rtps_trace = 1;

	DDS_get_default_pool_constraints (&reqs, ~0, 0);
	reqs.max_rx_buffers = 16;
	reqs.min_local_readers = reqs.max_local_readers = 5;
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

#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						DOMAIN_ID, NULL, NULL, 0);
	if (!part)
		fatal_printf ("DDS_DomainParticipantFactory_create_participant () failed!\r\n");

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

#ifdef DO_DISC_LISTEN
	start_disc_readers (part);
#endif

#ifdef USE_SHAPE_TYPE
	if (writer) { /* Register the message topic type. */
#endif
		error = register_HelloWorldData_type (part);
		if (error) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal_printf ("DDS_DomainParticipant_register_type ('%s') failed!\r\n", "HelloWorldData");
		}
		if (verbose)
			printf ("DDS Topic type ('%s') registered.\r\n", "HelloWorldData");
		/* Create a topic */
		topic = DDS_DomainParticipant_create_topic (part, "HelloWorld", "HelloWorldData", NULL, NULL, 0);
#ifdef USE_SHAPE_TYPE
	}
	else { /* Register the shape topic type. */
		error = register_ShapeType_type (part);
		if (error) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal_printf ("DDS_DomainParticipant_register_type ('%s') failed!\r\n", "ShapeType");
		}
		if (verbose)
			printf ("DDS Topic type ('%s') registered.\r\n", "ShapeType");
		/* Create a topic */
		topic = DDS_DomainParticipant_create_topic (part, "Square", "ShapeType", NULL, NULL, 0);
	}
#endif

	if (!topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal_printf ("DDS_DomainParticipant_create_topic () failed!\r\n");
	}
	if (verbose)
		printf ("DDS Topic created.\r\n");

	/* Create Topic Description. */
	topic_desc = DDS_TopicDescription_from_Topic (topic);
	if (!topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal_printf ("Unable to create topic description!\r\n");
	}

	/* Start either a reader or a writer depending on program options. */
	if (writer)
		dcps_do_writer (part);
	else
		dcps_do_reader (part);

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

	return (0);
}
