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
/*#include "log.h"*/
#include "error.h"
#include "debug.h"
#include "rtps_mux.h"
#include "dds_dcps.h"

#include <sys/time.h>
#include <errno.h>
#define LOG_MAIN
#include <log.h>

#define	PUB_STR	"<PUB>"
#define	SUB_STR	"<SUB>"
#define	HELLO_WORLD	"Hello DDS world!"
#define	DOMAIN_ID	0
#define MAX_DSIZE	0x20000		/* 128KB */

/* #define TRACE_DISC    Define to trace discovery endpoints. */
/* #define TRACE_DATA    Define to trace data endpoints. */

char        pubSubStr[6];
const char			*progname;
int			writer;			/* Default: reader. */
int			verbose;		/* Verbose if set. */
int			trace;			/* Trace messages if set. */
int			aborting;		/* Abort program if set. */
size_t			data_size;		/* Default: {"hello world", count} */
size_t			max_size;		/* Max. data size. */
unsigned		nsamples;		/* Sample to send. */
size_t			cur_size;		/* Current sample size. */
unsigned		count = ~0;		/* Max. # of times to send. */
unsigned		sleep_time = 1000;	/* Sleep time (milliseconds). */
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
	fprintf (stderr, "		minimem size.\r\n");
	fprintf (stderr, "   -n <count>	Max. # of times to send/receive data.\r\n");
	fprintf (stderr, "   -f		Flood mode (no waiting: as fast as possible).\r\n");
	fprintf (stderr, "   -d <msec>	Max. delay to wait for responses (10..10000).\r\n");
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
	int	i;
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
	char		message [100];
} MsgData_t;

#define	NMSGS	4

typedef struct msg_desc_st {
	unsigned	key;
	const char	*data;
	InstanceHandle_t handle;
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
	{ CDR_TYPECODE_CSTRING,0, "message", 100, offsetof (struct msg_data_st, message), 0 }
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

static void do_write (unsigned long user)
{
	MsgData_t	data;
	MsgDesc_t	*dp;
	unsigned	op, inst_delta, index;
	int		error;
	static DDS_InstanceHandle_t h [4];
	
	int retcode;
	long tv_hours = 0, tv_min = 0, tv_sec = 0, tv_msec = 0, tv_usec = 0;
	long tv_days_mod = 0, tv_hours_mod = 0, tv_min_mod = 0; 
	struct timeval timev;

	ARG_NOT_USED (user)

	tmr_start (&send_timer, sleep_time / TMR_UNIT, 0, do_write);

	if (pause_traffic)
		return;

	/*index = 0; //nsamples & 3;
	op = (index == 0)? 0:1;//(nsamples & 0xc) >> 2;
	inst_delta = 0; //(nsamples & 0x30) >> 4;
	dp = &messages [index]; for single instance */
	
	index = nsamples & 3;
	op = (nsamples & 0xc) >> 2;
	inst_delta = (nsamples & 0x30) >> 4;
	dp = &messages [index];
	data.key = dp->key + inst_delta;
	switch (op) {
		case 0:	
			h [index] = DDS_DataWriter_register_instance (w, &data);
			if (verbose)
				log_info("%s DDS                   : [%ld] Registered instance.", pubSubStr, h [index]);
		case 1:
		case 2:
		case 3:
		/*
		if (trace)
			trace_data (buf, cur_size);
		 */
			
		data.counter = nsamples++;
		strcpy (data.message, dp->data);
		error = DDS_DataWriter_write (w, &data, h[index]);
		if (error)
			log_error("%s DDS_DataWriter_write() failed! (error=%u)", pubSubStr, error);
		if (verbose) {
			memset(&timev, 0, sizeof(struct timeval));
			retcode = gettimeofday(&timev, NULL);
			if (retcode < 0)
		        log_error("%s gettimeofday() failed! (errno=%u)", pubSubStr, errno);
			tv_days_mod  = timev.tv_sec % 86400;
			tv_hours     = tv_days_mod / 3600;
			tv_hours_mod = tv_days_mod % 3600;
			tv_min       = tv_hours_mod / 60;
			tv_min_mod   = tv_hours_mod % 60;
			tv_sec       = tv_min_mod;
			tv_msec      = timev.tv_usec / 1000;
			tv_usec      = timev.tv_usec % 1000;
			//			log_info("%s DDS [%.2ld:%.2ld:%.2ld %.3ld:%.3ld]: [%ld] %2u - '%s'",pubSubStr, 
			log_info("%s DDS [%.2ld:%.2ld:%.2ld %.3ld:%.3ld]: [%ld] ALIVE - %2u :%6u - '%s'",pubSubStr, 
					tv_hours,
					tv_min,
					tv_sec,
					tv_msec,
					tv_usec,
					h[index],
					data.key,
					data.counter,
					data.message);
		}
		if (op < 3)
			break;

		DDS_DataWriter_unregister_instance (w, &data, h [index]);
		if (verbose)
			log_info("%s DDS                   : [%ld] Unregistered instance.", pubSubStr, h [index]);
		break;
	}
	cur_size++;
	if (cur_size > max_size)
		cur_size = data_size;
	DDS_schedule (1);
	/*if (!--max_events)
		pause_traffic = 1;*/
}

void dcps_do_writer (DDS_DomainParticipant part)
{
	DDS_Publisher	pub;
	char		c;
	int		error;
	unsigned	i;
	DDS_DataWriterQos w_qos;

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
		log_critical("%s DDS_DomainParticipant_create_publisher () failed!", pubSubStr);
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		log_info("%s DDS Publisher created.", pubSubStr);

	/* Setup writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &w_qos);
	w_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	w_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a Data Writer. */
	w = DDS_Publisher_create_datawriter (pub, topic, &w_qos, NULL, 0);
	if (!w) {
		log_critical("%s Unable to create a writer !", pubSubStr);
		DDS_DomainParticipantFactory_delete_participant (part);
	}

#ifdef TRACE_DATA
	rtps_dtrace_set (0);
#endif
	if (verbose)
		log_info("%s DDS Writer created.", pubSubStr);

	cur_size = data_size;
	nsamples = 0;
	tmr_start (&send_timer, sleep_time / TMR_UNIT, 0, do_write);
	do {
		DDS_wait (sleep_time);
	}
	while (nsamples < count && !aborting);

	error = DDS_Publisher_delete_datawriter (pub, w);
	if (error)
		log_critical("%s DDS_Publisher_delete_datawriter() failed! (error=%u)", pubSubStr, error);

	if (verbose)
		log_info("%s DDS Writer deleted.", pubSubStr);
}

void dcps_do_reader (DDS_DomainParticipant part)
{
	DDS_Subscriber		sub;
	DDS_DataReader		dr;
	DDS_DataReaderQos	r_qos;
	DDS_DataSeq		rx_sample = DDS_SEQ_INITIALIZER (void *);
	DDS_SampleInfoSeq	rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	DDS_SampleInfo		*info;
	MsgData_t		*sample;
	unsigned		i, n, nchanges;

	int retcode;
	long tv_hours = 0, tv_min = 0, tv_sec = 0, tv_msec = 0, tv_usec = 0;
	long tv_days_mod = 0, tv_hours_mod = 0, tv_min_mod = 0; 
	struct timeval timev;
	memset(&timev, 0, sizeof(struct timeval));
	
	/* Create a subscriber */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		log_critical("%s DDS_DomainParticipant_create_subscriber () returned an error!", pubSubStr);
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		log_info("%s DDS Subscriber created.", pubSubStr);

	/* Setup reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &r_qos);
	r_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	r_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a datareader. */
	dr = DDS_Subscriber_create_datareader (sub, topic_desc, &r_qos, NULL, 0);
	if (!dr) {
		log_critical("%s DDS_DomainParticipant_create_datareader () returned an error!", pubSubStr);
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		log_info("%s DDS Reader created.", pubSubStr);

#ifdef TRACE_DATA
	rtps_dtrace_set (0);
#endif

	for (i = 0; i < count && !aborting; ) {
		error = DDS_DataReader_take (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			log_error("%s Unable to read samples!", pubSubStr);
			break;
		}
		if (verbose) {
			memset(&timev, 0, sizeof(struct timeval));
			retcode = gettimeofday(&timev, NULL);
			if (retcode < 0)
				log_error("%s gettimeofday() failed! (errno=%u)", pubSubStr, errno);
			tv_days_mod  = timev.tv_sec % 86400;
			tv_hours     = tv_days_mod / 3600;
			tv_hours_mod = tv_days_mod % 3600;
			tv_min       = tv_hours_mod / 60;
			tv_min_mod   = tv_hours_mod % 60;
			tv_sec       = tv_min_mod;
			tv_msec      = timev.tv_usec / 1000;
			tv_usec      = timev.tv_usec % 1000;
		}		
		if (rx_info.length) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			nchanges = 1;
			if (verbose) {
				if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
					n = sample->counter;			
					//					log_info("%s DDS [%.2ld:%.2ld:%.2ld %.3ld:%.3ld]: [%ld] %2u - '%s'", pubSubStr,
					log_info("%s DDS [%.2ld:%.2ld:%.2ld %.3ld:%.3ld]: [%ld] ALIVE - %2u :%6u - '%s'", pubSubStr,
							tv_hours,
							tv_min,
							tv_sec,
							tv_msec,
							tv_usec,
							info->instance_handle,
							sample->key,
							sample->counter,
							sample->message);
				}
				else
					log_info("%s DDS [%.2ld:%.2ld:%.2ld %.3ld:%.3ld]: [%ld] %s", pubSubStr,
							tv_hours,
							tv_min,
							tv_sec,
							tv_msec,
							tv_usec,
							info->instance_handle,
							kind_str [info->instance_state]);
			}
			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
			i += nchanges;
		}
		rtps_poll (500);
	}
	error = DDS_Subscriber_delete_datareader (sub, dr);
	if (error)
		log_critical("%s DDS_Subscriber_delete_datareader() failed! (error=%u)", error);
}

int main (int argc, const char *argv [])
{	
	DDS_DomainParticipant	part;
	int			error, arg_index;

	arg_index = do_switches (argc, argv);

	if (writer)	{
		strncpy(pubSubStr, PUB_STR, 5);
	} else 	{
		strncpy(pubSubStr, SUB_STR, 5);
	}
	
/*	printf("writer=%d, count=%d, verbose=%d\n\r", writer, count, verbose);*/
    log_info("%s >> Start main program for US 403 upon technicolor DDS.", pubSubStr);

	if (verbose > 1)
		err_actions_add (EL_LOG, ACT_PRINT_STDIO);
	if (trace)
		rtps_trace = 1;

	DDS_init ();

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
        log_critical("%s DDS_DomainParticipantFactory_create_participant () failed!", pubSubStr);

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	if (verbose)
	    log_info("%s DDS Domain Participant created.", pubSubStr);

	/* Register the message topic type. */
	error = register_HelloWorldData_type (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		log_critical("DDS_DomainParticipant_register_type ('%s') failed!", "HelloWorldData", pubSubStr);
	}
	if (verbose)
		log_info("%s DDS Topic type ('%s') registered.", pubSubStr, "HelloWorldData");

	/* Create a topic */
	topic = DDS_DomainParticipant_create_topic (part, "HelloWorld", "HelloWorldData",
									NULL, NULL, 0);
	if (!topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		log_critical("%s DDS_DomainParticipant_create_topic ('HelloWorld') failed!", pubSubStr);
	}
	if (verbose)
		log_info("%s DDS Topic created.", pubSubStr);

	/* Create Topic Description. */
	topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "HelloWorld");
	if (!topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		log_critical("%s Unable to create topic description for 'HelloWorld'!", pubSubStr);
	}

	/* Start either a reader or a writer depending on program options. */
	if (writer)
		dcps_do_writer (part);
	else
		dcps_do_reader (part);

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		log_critical("%s DDS_DomainParticipantFactory_delete_participant () failed: error = %d", pubSubStr, error);

	if (verbose)
		log_info("%s DDS Participant deleted.", pubSubStr);

    log_info("%s << End main program for US 403 upon technicolor DDS.", pubSubStr);

	return (0);
}
