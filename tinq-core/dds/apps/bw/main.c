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

/* main.c -- Test program to test DDS bandwidth. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "libx.h"
#include "thread.h"
#ifdef DDS_SECURITY
#include "dds/dds_security.h"
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#else
#include "msecplug/msecplug.h"
#include "assert.h"
#include "../secplug/xmlparse.h"
#endif
#include "../security/engine_fs.h"
#endif
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"

#define	DOMAIN_ID	0
#define MAX_DSIZE	0x8000		/* 32KB */
/*#define READER_WAITSET		** Use Waitset based reader. */
#define	SLEEP_ON_EXIT			/* Sleep before exiting program. */
#define	MAX_HISTORY	128		/* Max. # of received samples. */
#define	TRACE_DELAY	4		/* Delay before tracing starts. */

const char		*progname;
int			reader;			/* Reader (consumer). */
int			writer;			/* Writer (producer). */
unsigned		max_readers = 1;	/* Max. # of readers in test. */
unsigned		max_time = 5;		/* Test time (seconds or ~0). */
unsigned		data_size = 1;		/* Sample size (bytes). */
unsigned		burst = 16;		/* # of samples in each burst.*/
unsigned		delay = 100;		/* Delay (us) between bursts. */
unsigned		history;		/* History to keep. */
int			reliable;		/* Reliable operation if set. */
int			forever;		/* Repeat when test done. */
int			histogram;		/* Display histogram. */
int			verbose;		/* Verbose if set. */
int			quit;			/* Quit program. */
#ifdef DDS_SECURITY
char                    *engine_id;		/* Engine id. */
char                    *cert_path;		/* Certificates path. */
char                    *key_path;		/* Private key path. */
char                    *realm_name;		/* Realm name. */
#endif
DDS_DomainParticipant	part;
DDS_Topic		cmd_topic;
DDS_Topic		status_topic;
DDS_Topic		data_topic;
DDS_TopicDescription	cmd_topic_desc;
DDS_TopicDescription	status_topic_desc;
DDS_TopicDescription	data_topic_desc;
DDS_Publisher		pub;
DDS_Subscriber		sub;

typedef struct history_st {
	unsigned	samples;
	unsigned	missed;
} History_t;
History_t		hist_data [MAX_HISTORY];
unsigned		hist_index;

#ifndef DDS_DEBUG
int pause_traffic = 0;
unsigned max_events = ~0;
unsigned sleep_time = 1000;
#endif

#ifdef CTRACE_USED
enum {
	USER_TX_START, USER_TX_STOP,
	USER_TX_BURST, USER_TX_DELAY,
	USER_RXD_START, USER_RXD_STOP,
	USER_RXD_MISS, USER_RXD_INC,
	USER_RXD_SAMPLES
};

static const char *user_fct_str [] = {
	"TxStart", "TxStop",
	"TxBurst", "TxDelay",
	"RxDStart", "RxDStop",
	"RxDMissed", "RxDIncorrect",
	"RxDSamples"
};

#define	ctrace_stop()		DDS_CTrace_stop()
#define	ctrace_printd(i,d,l)	DDS_CTrace_printd(i,d,l)
#else
#define	ctrace_stop()
#define	ctrace_printd(i,d,l)
#endif

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "bw -- Bandwidth test program for the DDS protocol.\r\n");
	fprintf (stderr, "Usage: bw [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -o         Act as a consumer.\r\n");
	fprintf (stderr, "   -p         Act as a producer.\r\n");
	fprintf (stderr, "   -op|po     Both producer and consumer (default).\r\n");
	fprintf (stderr, "   -n <nrdrs> # of consumers in test (default=1).\r\n");
	fprintf (stderr, "   -t <time>  Test time in seconds (default=5s).\r\n");
	fprintf (stderr, "   -s <size>  Data sample size in bytes (1..32768, default=1).\r\n");
	fprintf (stderr, "   -b <count> Max. # of samples per burst (1..1000, default=16).\r\n");
	fprintf (stderr, "   -d <usec>  Max. delay in us between data bursts (default=100us).\r\n");
	fprintf (stderr, "   -r         Reliable operation (default=Best-Effort).\r\n");
	fprintf (stderr, "   -q <hist>  # of samples to keep in history (default=ALL).\r\n");
	fprintf (stderr, "   -g         Display histogram of received samples in time.\r\n");
	fprintf (stderr, "   -f         Repeat tests forever.\r\n");
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>  Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>  Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>  Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm> The realm name.\r\n");
#endif
	fprintf (stderr, "   -v         Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv        Extra verbose: log detailed functionality.\r\n");
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

#ifdef DDS_SECURITY

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

#endif

#define	INC_ARG()	if (!*cp) { i++; cp = argv [i]; }

/* do_switches -- Command line switch decoder. */

int do_switches (int argc, const char **argv)
{
	int		i;
	const char	*cp;
#ifdef DDS_SECURITY
	const char      *arg_input;
#endif
	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'o':
					reader = 1;
					break;
				case 'p':
					writer = 1;
					break;
				case 'n':
					INC_ARG()
					if (!get_num (&cp, &max_readers, 1, 64))
						usage ();
					break;
				case 't':
					INC_ARG()
					if (!get_num (&cp, &max_time, 1, ~0))
						usage ();
					break;
				case 's':
					INC_ARG()
					if (!get_num (&cp, &data_size, 1, MAX_DSIZE))
						usage ();
					break;
				case 'b':
					INC_ARG()
					if (!get_num (&cp, &burst, 1, 1000))
						usage ();
					break;
				case 'd':
					INC_ARG()
					if (!get_num (&cp, &delay, 1, 3600000000U))
						usage ();
					break;
				case 'r':
					reliable = 1;
					break;
				case 'q':
					INC_ARG()
					if (!get_num (&cp, &history, 1, ~0))
						usage ();
					break;
				case 'f':
					forever = 1;
					break;
				case 'g':
					histogram = 1;
					break;
#ifdef DDS_SECURITY
			        case 'e':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					engine_id = malloc (strlen (arg_input) + 1);
					strcpy (engine_id, arg_input);
					break;
			        case 'c':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					cert_path = malloc (strlen (arg_input) + 1);
					strcpy (cert_path, arg_input);
					break;
			        case 'k':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					key_path = malloc (strlen (arg_input) + 1);
					strcpy (key_path, arg_input);

					break;
			        case 'z':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					realm_name = malloc (strlen (arg_input) + 1);
					strcpy (realm_name, arg_input);
					break;
#endif
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
	if (!reader && !writer)
		reader = writer = 1;

	return (i);
}

typedef enum {
	TEST_START = 1,		/* Start test. */
	TEST_STOP,		/* Stop test. */
	TEST_QUIT		/* Quit test. */
} BWCmd_t;

typedef struct bw_cmd_st {
	uint32_t	cmd;
	uint32_t	dsize;
	uint32_t	burst;
} BwCmd_t;

static DDS_TypeSupport_meta bw_cmd_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "Cmd",   sizeof (struct bw_cmd_st), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "cmd",   0, offsetof (struct bw_cmd_st, cmd), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "dsize", 0, offsetof (struct bw_cmd_st, dsize), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "burst", 0, offsetof (struct bw_cmd_st, burst), 0, 0, NULL },
};

typedef enum {
	RX_MISSED = 1,		/* Samples missed. */
	RX_INCORRECT		/* Incorrect data received. */
} BWStatus_t;

typedef struct bw_status_st {
	uint32_t	status;
	uint32_t	nsamples;
} BwStatus_t;

static DDS_TypeSupport_meta bw_status_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "Status",   sizeof (struct bw_status_st), 0, 2, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "status",   0, offsetof (struct bw_status_st, status), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "nsamples", 0, offsetof (struct bw_status_st, nsamples), 0, 0, NULL },
};

typedef struct bw_data_st {
	uint32_t	counter;
	char		value [MAX_DSIZE];
} BwData_t;

static DDS_TypeSupport_meta bw_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT,  0, "Data",    sizeof (struct bw_data_st), 0, 2, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "counter", 0, offsetof (struct bw_data_st, counter), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "value",   MAX_DSIZE, offsetof (struct bw_data_st, value), 0, 0, NULL }
};

static DDS_TypeSupport	BwCmd_ts, BwStatus_ts, BwData_ts;

DDS_ReturnCode_t register_bw_types (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	BwCmd_ts = DDS_DynamicType_register (bw_cmd_tsm);
        if (!BwCmd_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, BwCmd_ts, "BwCmd");
	if (error)
		return (error);

	BwStatus_ts = DDS_DynamicType_register (bw_status_tsm);
        if (!BwStatus_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, BwStatus_ts, "BwStatus");
	if (error)
		return (error);

	BwData_ts = DDS_DynamicType_register (bw_data_tsm);
        if (!BwData_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, BwData_ts, "BwData");
	return (error);
}

void free_bw_types (void)
{
	if (BwCmd_ts) {
		DDS_DynamicType_free (BwCmd_ts);
		BwCmd_ts = NULL;
	}
	if (BwStatus_ts) {
		DDS_DynamicType_free (BwStatus_ts);
		BwStatus_ts = NULL;
	}
	if (BwData_ts) {
		DDS_DynamicType_free (BwData_ts);
		BwData_ts = NULL;
	}
}

static BwStatus_t r_stat;

void bw_on_status_avail (DDS_DataReaderListener *l, const DDS_DataReader sr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	unsigned		i;

	ARG_NOT_USED (l)

	error = DDS_DataReader_take (sr, &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
	if (error) {
		if (error != DDS_RETCODE_NO_DATA)
			printf ("Unable to read status samples: error = %u!\r\n", error);
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++) {
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			memcpy (&r_stat, DDS_SEQ_ITEM (rx_sample, i), sizeof (r_stat));
			if (verbose) {
				printf ("Status received: ");
				if (r_stat.status == RX_MISSED)
					printf ("reader missed %u samples!", r_stat.nsamples);
				else if (r_stat.status == RX_INCORRECT)
					printf ("reader got an invalid sample!");
				else
					printf ("?(%u)", r_stat.status);
				printf ("\r\n");
			}
			ctrace_stop ();
		}
	}
	DDS_DataReader_return_loan (sr, &rx_sample, &rx_info);
}

DDS_DataReaderListener sr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	bw_on_status_avail,
	NULL,
	NULL,
	NULL
};

static unsigned num_readers;

void bw_on_publication_matched (DDS_DataWriterListener *l,
				DDS_DataWriter w,
				DDS_PublicationMatchedStatus *st)
{
	ARG_NOT_USED (l)
	ARG_NOT_USED (w)

	if (verbose)
		printf ("on_publication_matched: %d of %d readers match\r\n",
				st->current_count, st->total_count);
	num_readers = st->current_count;
}

DDS_DataWriterListener dw_listener = {
	NULL,
	bw_on_publication_matched,
	NULL,
	NULL,
	NULL
};

void bw_do_writer (void)
{
	int			error;
	unsigned		i;
	DDS_DataWriter	 	cw;
	DDS_DataWriter	 	dw;
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReader		sr;
	DDS_DataReaderQos	sr_qos;
	DDS_StatusMask		sm;
	DDS_Time_t		dds_time = { 0, 0 };
	DDS_Time_t		t, nt;
	BwCmd_t			cmd;
	static BwData_t		data;
	DDS_ReturnCode_t	ret;

	/* Setup Command Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Writer. */
	cw = DDS_Publisher_create_datawriter (pub, cmd_topic, &wr_qos, NULL, 0);
	if (!cw)
		fatal ("Unable to create command writer!");

	if (verbose)
		printf ("DDS Command Writer created.\r\n");

	/* Setup Status Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &sr_qos);
	sr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	sr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Status Reader. */
	sr = DDS_Subscriber_create_datareader (sub, status_topic_desc, &sr_qos, 
					&sr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!sr)
		fatal ("Unable to create status reader!");

	if (verbose)
		printf ("DDS Status Reader created.\r\n");

	/* Setup Data Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	if (reliable) {
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
		wr_qos.reliability.max_blocking_time.sec = 1;
		wr_qos.reliability.max_blocking_time.nanosec = 0;
		wr_qos.resource_limits.max_samples = 
		wr_qos.resource_limits.max_samples_per_instance = 128;
		wr_qos.resource_limits.max_instances = 1;
	}
	if (!history || history == ~0U)
		wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	else {
		wr_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
		wr_qos.history.depth = history;
	}

	/* Create Data Writer. */
	sm = DDS_PUBLICATION_MATCHED_STATUS | DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
	dw = DDS_Publisher_create_datawriter (pub, data_topic, &wr_qos, &dw_listener, sm);
	if (!dw)
		fatal ("Unable to create data writer!");

	if (verbose)
		printf ("DDS Data Writer created.\r\n");

	/* Create data sample. */
	if (data_size > 1)
		memset (data.value, '#', data_size - 1);
	data.value [data_size - 1] = '\0';

	do {
		/* Wait until all readers are online. */
		do {
			usleep (1000);
			if (quit)
				break;
		}
		while (num_readers < max_readers);

		/* Send START command to all readers. */
		cmd.cmd = TEST_START;
		cmd.dsize = data_size;
		cmd.burst = burst;
		error = DDS_DataWriter_write (cw, &cmd, DDS_HANDLE_NIL);
		if (error)
			fatal ("Unable to write START command (%s)!", DDS_error (error));

		printf ("Test started ... ");
		fflush (stdout);

		ctrace_printd (USER_TX_START, NULL, 0);

		data.counter = 0;
		DDS_DomainParticipant_get_current_time (part, &t);
		t.sec += max_time;
		for (;;) {

			/* Send a burst of data samples. */
			ctrace_printd (USER_TX_BURST, NULL, 0);
			for (i = 0; i < burst; i++) {
				ret = DDS_DataWriter_write_w_timestamp (dw, &data, DDS_HANDLE_NIL, &dds_time);
				if (ret != DDS_RETCODE_OK) {
					if (ret != DDS_RETCODE_TIMEOUT)
						fatal ("Data write error (%s)!", DDS_error (ret));
				}
				else {
					data.counter++;
					if (verbose == 2) {
						printf ("W");
						fflush (stdout);
					}
				}
			}
			ctrace_printd (USER_TX_DELAY, NULL, 0);

			/* Inter-burst delay. */
			usleep (delay);

			/* Check if test done. */
			DDS_DomainParticipant_get_current_time (part, &nt);
			if (quit ||
			    nt.sec > t.sec ||
			    (nt.sec == t.sec && nt.nanosec >= t.nanosec))
				break;

			/* Check if reader missed samples. */
			if (r_stat.status >= RX_MISSED) {
				printf ("Stopped (reader missed samples)!\r\n");
				break;
			}
		}
		ctrace_printd (USER_TX_STOP, NULL, 0);
		if (!quit && !r_stat.status)
			printf ("done!\r\n");

		/* Send STOP command to all readers. */
		cmd.cmd = TEST_STOP;
		error = DDS_DataWriter_write (cw, &cmd, DDS_HANDLE_NIL);
		if (error)
			fatal ("Unable to write STOP command (%s)!", DDS_error (error));
	}
	while (forever && !quit);

	/* Wait for readers to sign-off. */
	do {
		usleep (1000);
		if (quit)
			break;
	}
	while (num_readers);

	usleep (10000);

	/* Finish writer/publisher data. */
	DDS_Publisher_delete_datawriter (pub, dw);
	DDS_Publisher_delete_datawriter (pub, cw);
	DDS_DomainParticipant_delete_publisher (part, pub);
}

static BwCmd_t r_cmd;
static unsigned r_samples;
static unsigned r_missed;
static unsigned r_expected;
static DDS_DataWriter r_sw;

void dump_stats (DDS_Time_t *start, DDS_Time_t *stop)
{
	double	r_bytes = (double) r_samples * (r_cmd.dsize + 4);
	double	r_secs, mbps, kbps;

	r_secs = (stop->sec + stop->nanosec / 1000000000) -
	         (start->sec + start->nanosec / 1000000000);
	mbps = r_bytes * 8 / (1000000 * r_secs);
	kbps = r_bytes / (1024 * r_secs);

	printf ("%u bytes/sample, %f secs, %u samples, %.0f bytes, %u missed\r\n"
		"  => %.0f samples/sec, %f Mbps, %f KBps\r\n",
			r_cmd.dsize, r_secs, r_samples, r_bytes, r_missed,
			r_samples / r_secs, mbps, kbps);
}

void bw_on_cmd_avail (DDS_DataReaderListener *l, const DDS_DataReader cr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	unsigned		i;

	ARG_NOT_USED (l)

	error = DDS_DataReader_take (cr, &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
	if (error) {
		if (error != DDS_RETCODE_NO_DATA)
			printf ("Unable to read commands: error = %u!\r\n", error);
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++) {
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			memcpy (&r_cmd, DDS_SEQ_ITEM (rx_sample, i), sizeof (r_cmd));
			if (verbose) {
				printf ("Command received: ");
				if (r_cmd.cmd == TEST_START)
					printf ("START");
				else if (r_cmd.cmd == TEST_STOP)
					printf ("STOP");
				else if (r_cmd.cmd == TEST_QUIT)
					printf ("QUIT");
				else
					printf ("?(%u)", r_cmd.cmd);
				printf ("\r\n");
			}
		}
	}
	DDS_DataReader_return_loan (cr, &rx_sample, &rx_info);
}

DDS_DataReaderListener cr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	bw_on_cmd_avail,
	NULL,
	NULL,
	NULL
};

void bw_read_data (const DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	unsigned		i;
	BwData_t		*sample;
	BwStatus_t		status;
#ifdef CTRACE_USED
	unsigned		n = r_samples;
#endif

	ctrace_printd (USER_RXD_START, NULL, 0);
	error = DDS_DataReader_take (dr, &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
	if (error) {
		if (error != DDS_RETCODE_NO_DATA) {
			printf ("Unable to read data samples: error = %u!\r\n", error);
			ctrace_printd (USER_RXD_SAMPLES, NULL, 0);
		}
		return;
	}
	if (!DDS_SEQ_LENGTH (rx_sample)) {
#ifdef CTRACE_USED
		n = 0;
		ctrace_printd (USER_RXD_SAMPLES, &n, sizeof (n));
#endif
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++) {
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			sample = (BwData_t *) DDS_SEQ_ITEM (rx_sample, i);
			if (sample->counter > r_expected) {
				ctrace_printd (USER_RXD_MISS, &sample->counter, sizeof (sample->counter));
				ctrace_stop ();
				r_missed += sample->counter - r_expected;
				if (hist_index < MAX_HISTORY)
					hist_data [hist_index].missed += sample->counter - r_expected;
				status.status = RX_MISSED;
				status.nsamples = sample->counter - r_expected;
				DDS_DataWriter_write (r_sw, &status, DDS_HANDLE_NIL);
#if 0
				if (verbose)
					printf ("Samples missed (%u)!\r\n", sample->counter - r_expected);
#endif
			}
			else if (sample->counter < r_expected) {
				printf ("Incorrect sample: received = %u, expected = %u!\r\n",
						sample->counter, r_expected);
				ctrace_printd (USER_RXD_INC, &sample->counter, sizeof (sample->counter));
				ctrace_stop ();
				status.status = RX_INCORRECT;
				DDS_DataWriter_write (r_sw, &status, DDS_HANDLE_NIL);
			}
			if (hist_index < MAX_HISTORY)
				hist_data [hist_index].samples++;
			r_samples++;
			r_expected = sample->counter + 1;
		}
	}
	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
#ifdef CTRACE_USED
	n = r_samples - n;
	ctrace_printd (USER_RXD_SAMPLES, &n, sizeof (n));
#endif
}

#ifndef READER_WAITSET

void bw_on_data_avail (DDS_DataReaderListener *l, const DDS_DataReader dr)
{
	ARG_NOT_USED (l)

	if (verbose == 2) {
		printf ("R");
		fflush (stdout);
	}
	bw_read_data (dr);
}

DDS_DataReaderListener dr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	bw_on_data_avail,
	NULL,
	NULL,
	NULL
};

#endif

void *bw_do_reader (void *args)
{
	DDS_DataReader		cr;
	DDS_DataReader		dr;
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriterQos 	wr_qos;
	DDS_Time_t		t, nt;
	unsigned		i;
#ifdef READER_WAITSET
	DDS_WaitSet		ws;
	DDS_ReadCondition	rc;
	DDS_ConditionSeq	conds = DDS_SEQ_INITIALIZER (DDS_Condition);
	DDS_Duration_t		to;
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
#endif
#ifdef CTRACE_USED
#ifdef CTRACE_DELAY
	int			trace_started = 0;
#endif
#endif
	ARG_NOT_USED (args)

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	cr = DDS_Subscriber_create_datareader (sub, cmd_topic_desc, &rd_qos, 
					&cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!");

	if (verbose)
		printf ("DDS Command Reader created.\r\n");

	/* Setup Status Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Status Writer. */
	r_sw = DDS_Publisher_create_datawriter (pub, status_topic, &wr_qos, NULL, 0);
	if (!r_sw)
		fatal ("Unable to create status writer!");

	if (verbose)
		printf ("DDS Status Writer created.\r\n");

	/* Setup Data Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	if (reliable)
		rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	if (!history || history == ~0U)
		rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	else {
		rd_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
		rd_qos.history.depth = history;
	}

	/* Create Data Reader. */
#ifdef READER_WAITSET
	dr = DDS_Subscriber_create_datareader (sub, data_topic_desc, &rd_qos, NULL, 0);
	if (!dr)
		fatal ("Unable to create data reader!");

	if (verbose)
		printf ("DDS Data Reader created.\r\n");

	ws = DDS_WaitSet__alloc ();
	if (!ws)
		fatal ("Unable to allocate a WaitSet!");

	if (verbose)
		printf ("DDS Waitset allocated.\r\n");

	rc = DDS_DataReader_create_readcondition (dr, ss, vs, is);
	if (!rc)
		fatal ("DDS_DataReader_create_readcondition () returned an error!");

	if (verbose)
		printf ("DDS Readcondition created.\r\n");

	DDS_WaitSet_attach_condition (ws, rc);
#else
	dr = DDS_Subscriber_create_datareader (sub, data_topic_desc, &rd_qos, 
					&dr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!dr)
		fatal ("Unable to create data reader!");

	if (verbose)
		printf ("DDS Data Reader created.\r\n");

#endif
#ifdef CTRACE_USED
#ifdef CTRACE_DELAY
	trace_started = 0;
#endif
#endif
	do {
		if (verbose) {
			printf ("Awaiting command.\r\n");
		}
		/* Setup work parameters. */
		r_cmd.cmd = 0;
		r_samples = 0;
		r_missed = 0;
		r_expected = 0;
		memset (hist_data, 0, sizeof (hist_data));

		/* Wait for start command. */
		do {
			usleep (1000);
			if (quit)
				break;
		}
		while (r_cmd.cmd != TEST_START);
		/*if (verbose) {
			printf ("reading samples ... ");
			fflush (stdout);
		}*/
		DDS_DomainParticipant_get_current_time (part, &t);
		do {
#ifdef READER_WAITSET
			to.sec = 0;
			to.nanosec = 10000000;
			if ((error = DDS_WaitSet_wait (ws, &conds, &to)) != DDS_RETCODE_TIMEOUT)
				bw_read_data (dr);
#else
			usleep (10000);
#endif
			DDS_DomainParticipant_get_current_time (part, &nt);
			hist_index = nt.sec - t.sec;
			if (nt.nanosec < t.nanosec)
				hist_index--;
#ifdef CTRACE_DELAY
#ifdef CTRACE_USED
			if (!trace_started && hist_index >= CTRACE_DELAY) {
				trace_started = 1;
				DDS_CTrace_start ();
			}
#endif
#endif
			if (quit)
				break;
		}
		while (r_cmd.cmd != TEST_STOP);
		DDS_DomainParticipant_get_current_time (part, &nt);
		dump_stats (&t, &nt);
		if (histogram) {
			printf ("Time  Samples    Missed.\r\n");
			for (i = 0; i < hist_index; i++)
				printf ("%3i: %8u  %8u\r\n", i,
						hist_data [i].samples,
						hist_data [i].missed);
		}
	}
	while (forever);

#ifdef READER_WAITSET
	dds_seq_cleanup (&conds);
#endif

	DDS_Subscriber_delete_datareader (sub, dr);
	DDS_Subscriber_delete_datareader (sub, cr);
	DDS_DomainParticipant_delete_subscriber (part, sub);

	return (NULL);
}

#ifdef DDS_SECURITY

#define fail_unless     assert

static void enable_security (void)
{
	DDS_Credentials		credentials;
	DDS_ReturnCode_t	error;
#ifdef MSECPLUG_WITH_SECXML
	/*int dhandle, thandle;*/
#endif
	error = DDS_SP_set_policy ();
	if (error)
		fatal ("DDS_SP_set_policy() returned error (%s)!", DDS_error (error));

#ifdef MSECPLUG_WITH_SECXML
	if (DDS_SP_parse_xml ("security.xml"))
		fatal ("SP: no DDS security rules in 'security.xml'!\r\n");
#else
	DDS_SP_add_domain();
	if (!realm_name)
		DDS_SP_add_participant ();
	else 
		DDS_SP_set_participant_access (DDS_SP_add_participant (), strcat(realm_name, "*"), 2, 0);
#endif
	if (!cert_path || !key_path)
		fatal ("Error: you must provide a valid certificate path and a valid private key path\r\n");

	if (engine_id) {
		DDS_SP_init_engine (engine_id, init_engine_fs);
		credentials.credentialKind = DDS_ENGINE_BASED;
		credentials.info.engine.engine_id = engine_id;
		credentials.info.engine.cert_id = cert_path;
		credentials.info.engine.priv_key_id = key_path;
	}
	else {
		credentials.credentialKind = DDS_FILE_BASED;
		credentials.info.filenames.private_key_file = key_path;
		credentials.info.filenames.certificate_chain_file = cert_path;
	}

	error = DDS_Security_set_credentials ("Technicolor Bandwidth Tester", &credentials);
}

static void cleanup_security (void)
{
	/* Cleanup security submodule. */
	DDS_SP_access_db_cleanup ();
	DDS_SP_engine_cleanup ();

	/* Cleanup malloc-ed memory. */
	if (engine_id)
		free (engine_id);
	if (cert_path)
		free (cert_path);
	if (key_path)
		free (key_path);
	if (realm_name)
		free (realm_name);
}

#endif

int main (int argc, const char *argv [])
{
	DDS_PoolConstraints	reqs;
	thread_t		rt;
	int			error;
	void			*st;

	do_switches (argc, argv);

	if (verbose > 1)
		DDS_Log_stdio (1);

	DDS_get_default_pool_constraints (&reqs, ~0, 100);
	reqs.max_rx_buffers = 16;
	reqs.min_local_readers = 10;
	reqs.min_local_writers = 8;
	reqs.min_changes = 64;
	reqs.min_instances = 48;
	DDS_set_pool_constraints (&reqs);
	DDS_entity_name ("Technicolor Bandwidth Tester");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif
#ifdef DDS_DEBUG
	if (isatty (STDIN_FILENO)) {
		DDS_Debug_start ();
		DDS_Debug_abort_enable (&quit);
	}
#endif

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						DOMAIN_ID, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

#ifdef CTRACE_USED
	DDS_Log_strings (user_fct_str);
	DDS_CTrace_mode (1);	/* Cyclic trace mode. */
#endif

	/* Register the message topic type. */
	error = register_bw_types (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_register_type () failed (%s)!", DDS_error (error));
	}
	if (verbose)
		printf ("DDS Topic types registered.\r\n");

	/* Create command topic. */
	cmd_topic = DDS_DomainParticipant_create_topic (part, "BWC", "BwCmd",
									NULL, NULL, 0);
	if (!cmd_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('BWC') failed!");
	}
	if (verbose)
		printf ("DDS BWC Topic created.\r\n");

	/* Create Topic Description. */
	cmd_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "BWC");
	if (!cmd_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'BWC'!");
	}

	/* Create status topic. */
	status_topic = DDS_DomainParticipant_create_topic (part, "BWS", "BwStatus",
									NULL, NULL, 0);
	if (!status_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('BWS') failed!");
	}
	if (verbose)
		printf ("DDS BWS Topic created.\r\n");

	/* Create Topic Description. */
	status_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "BWS");
	if (!status_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'BWS'!");
	}

	/* Create data topic. */
	data_topic = DDS_DomainParticipant_create_topic (part, "BWD", "BwData",
									NULL, NULL, 0);
	if (!data_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('BWD') failed!");
	}
	if (verbose)
		printf ("DDS BWD Topic created.\r\n");

	/* Create Topic Description. */
	data_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "BWD");
	if (!data_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'BWD'!");
	}

	/* Create a publisher. */
	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		fatal ("DDS_DomainParticipant_create_publisher () failed!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Publisher created.\r\n");

	/* Create a subscriber */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

#ifdef CTRACE_USED
#ifdef CTRACE_DELAY
	DDS_CTrace_stop ();
	DDS_CTrace_clear ();
#endif
#endif

	/* Start either a reader or a writer or both, depending on program options. */
	if (reader && !writer)
		bw_do_reader (NULL);
	else if (reader && writer) {
		thread_create (rt, bw_do_reader, part);
		bw_do_writer ();
		st = NULL;
		thread_wait (rt, st);
	}
	else if (writer)
		bw_do_writer ();

#ifdef DDS_DEBUG
#ifdef SLEEP_ON_EXIT
	sleep (100);
#endif
#endif
	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	free_bw_types ();
	if (verbose)
		printf ("DDS Types deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Participant deleted\r\n");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		cleanup_security ();
#endif
	return (0);
}

