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

/* main.c -- Test program to test DDS latency. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "thread.h"
#include "libx.h"
#ifdef DDS_SECURITY
#include "dds/dds_security.h"
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#else
#include "msecplug/msecplug.h"
#include "assert.h"
#include "../../plugins/secplug/xmlparse.h"
#endif
#include "../../plugins/security/engine_fs.h"
#endif
#include "dds/dds_dcps.h"
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"

#define	DOMAIN_ID	0
#define MAX_DSIZE	0x8000		/* 32KB */

const char		*progname;
int			controller;		/* Controller. */
int			generator;		/* Generator (Tx+Rx data). */
int			loop;			/* Loop (loops Rxed data). */
unsigned		nsamples = 50;		/* # of samples. */
unsigned		data_size = 4;		/* Sample size (bytes). */
unsigned		delay = 100;		/* Delay (ms) between samples. */
unsigned		history;		/* History to keep. */
int			reliable;		/* Reliable operation if set. */
int			dump;			/* Dump samples if set. */
int			verbose;		/* Verbose if set. */
int			quit;
#ifdef DDS_SECURITY
char                    *engine_id;		/* Engine id. */
char                    *cert_path;		/* Certificates path. */
char                    *key_path;		/* Private key path. */
char                    *realm_name;		/* Realm name. */
#endif
DDS_DomainParticipant	part;
DDS_Topic		cmd_topic;
DDS_Topic		data_tx_topic;
DDS_Topic		data_rx_topic;
DDS_TopicDescription	cmd_topic_desc;
DDS_TopicDescription	data_tx_topic_desc;
DDS_TopicDescription	data_rx_topic_desc;
DDS_Subscriber		sub;
DDS_Publisher		pub;
double			*results;

#ifndef DDS_DEBUG
int pause_traffic = 0;
unsigned max_events = ~0;
unsigned sleep_time = 1000;
#endif

#ifdef CTRACE_USED
enum {
	USER_C_START, USER_C_DONE,
	USER_G_START, USER_G_DONE,
	USER_G_SEND, USER_G_RECEIVED,
	USER_LOOPED
};

static const char *user_fct_str [] = {
	"Ctrl-Start", "Ctrl-Done",
	"TxStart", "TxDone",
	"TxSample", "RxSample",
	"LoopedSample"
};

#define	ctrace_printd(i,l,d)	DDS_CTrace_printd(i,l,d)
#else
#define	ctrace_printd(i,l,d)
#endif

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "latency -- Latency test program for the DDS protocol.\r\n");
	fprintf (stderr, "Usage: latency [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -o		Controller (controls the others).\r\n");
	fprintf (stderr, "   -g		Generator (sends/receives samples).\r\n");
	fprintf (stderr, "   -l		Looper (loops samples - default).\r\n");
	fprintf (stderr, "   -n <smpls>	# of samples to take (default=50).\r\n");
	fprintf (stderr, "   -s <size>	Data sample size in bytes (default=1).\r\n");
	fprintf (stderr, "   -d <msec>	Delay between each sample (default=100ms).\r\n");
	fprintf (stderr, "   -r		Reliable operation (default=Best-Effort).\r\n");
	fprintf (stderr, "   -q <hist>	# of samples to keep in history (default=ALL).\r\n");
	fprintf (stderr, "   -i		Dump all latency sample info.\r\n");
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>  Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>  Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>  Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm> The realm name.\r\n");
#endif
	fprintf (stderr, "   -v		Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv  	Extra verbose: log detailed functionality.\r\n");
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
					controller = 1;
					break;
				case 'g':
					generator = 1;
					break;
				case 'l':
					loop = 1;
					break;
				case 'n':
					INC_ARG()
					if (!get_num (&cp, &nsamples, 1, 10000))
						usage ();
					break;
				case 's':
					INC_ARG()
					if (!get_num (&cp, &data_size, 1, MAX_DSIZE))
						usage ();
					break;
				case 'd':
					INC_ARG()
					if (!get_num (&cp, &delay, 1, 3600000))
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
				case 'i':
					dump = 1;
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
	if (!controller && !generator && !loop)
		loop = 1;

	return (i);
}

typedef enum {
	TEST_START = 1,		/* Start test (C->G). */
	TEST_DONE,		/* Test completed (G->C). */
	TEST_QUIT		/* Quit test (C->G+L). */
} Cmd_t;

typedef struct l_cmd_st {
	uint32_t	cmd;
	uint32_t	dsize;
	uint32_t	delay;
} LCmd_t;

static DDS_TypeSupport_meta l_cmd_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "Cmd",   sizeof (struct l_cmd_st), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "cmd",   0, offsetof (struct l_cmd_st, cmd), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "dsize", 0, offsetof (struct l_cmd_st, dsize), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "delay", 0, offsetof (struct l_cmd_st, delay), 0, 0, NULL },
};

typedef struct l_data_st {
	uint32_t	counter;
	char		value [MAX_DSIZE];
} LData_t;

static DDS_TypeSupport_meta l_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT,  0, "Data",    sizeof (struct l_data_st), 0, 2, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "counter", 0, offsetof (struct l_data_st, counter), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "value",   MAX_DSIZE, offsetof (struct l_data_st, value), 0, 0, NULL }
};

static DDS_TypeSupport	LCmd_ts, LData_ts;

DDS_ReturnCode_t register_latency_types (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	LCmd_ts = DDS_DynamicType_register (l_cmd_tsm);
        if (!LCmd_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, LCmd_ts, "LCmd");
	if (error)
		return (error);

	LData_ts = DDS_DynamicType_register (l_data_tsm);
        if (!LData_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, LData_ts, "LData");
	return (error);
}

void free_latency_types (void)
{
	if (LCmd_ts) {
		DDS_DynamicType_free (LCmd_ts);
		LCmd_ts = NULL;
	}
	if (LData_ts) {
		DDS_DynamicType_free (LData_ts);
		LData_ts = NULL;
	}
}

static unsigned num_cmd_readers;

void l_on_publication_matched (DDS_DataWriterListener *l,
			       DDS_DataWriter w,
			       DDS_PublicationMatchedStatus *st)
{
	ARG_NOT_USED (l)
	ARG_NOT_USED (w)

	if (verbose)
		printf ("on_publication_matched: %d of %d readers match\r\n",
				st->current_count, st->total_count);
	num_cmd_readers = st->current_count;
}

DDS_DataWriterListener c_cw_listener = {
	NULL,
	l_on_publication_matched,
	NULL,
	NULL,
	NULL
};

void l_on_cmd_avail (DDS_DataReaderListener *l, const DDS_DataReader cr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	LCmd_t			*cmd = (LCmd_t *) l->cookie;
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
			memcpy (cmd, DDS_SEQ_ITEM (rx_sample, i), sizeof (LCmd_t));
			if (verbose) {
				printf ("Command received: ");
				if (cmd->cmd == TEST_START)
					printf ("START");
				else if (cmd->cmd == TEST_DONE)
					printf ("DONE");
				else if (cmd->cmd == TEST_QUIT)
					printf ("QUIT");
				else
					printf ("?(%u)", cmd->cmd);
				printf ("\r\n");
			}
			if (cmd->cmd == TEST_QUIT)
				quit = 1;
		}
	}
	DDS_DataReader_return_loan (cr, &rx_sample, &rx_info);
}

static LCmd_t l_c_cmd;

DDS_DataReaderListener c_cr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	l_on_cmd_avail,
	NULL,
	NULL,
	(void *) &l_c_cmd
};

void send_cmd (DDS_DataWriter cw, Cmd_t type, unsigned dsize, unsigned delay)
{
	LCmd_t	c;

	c.cmd = type;
	c.dsize = dsize;
	c.delay = delay;
	DDS_DataWriter_write (cw, &c, DDS_HANDLE_NIL);
}

void l_do_controller (void)
{
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriter	 	cw;
	DDS_DataReader	 	cr;
	DDS_StatusMask		sm;

	/* Setup Command Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Writer. */
	sm = DDS_PUBLICATION_MATCHED_STATUS | DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
	cw = DDS_Publisher_create_datawriter (pub, cmd_topic, &wr_qos, &c_cw_listener, sm);
	if (!cw)
		fatal ("Unable to create command writer!");

	if (verbose)
		printf ("C: DDS Command Writer created.\r\n");

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	cr = DDS_Subscriber_create_datareader (sub, cmd_topic_desc, &rd_qos, 
					&c_cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!");

	if (verbose)
		printf ("C: DDS Command Reader created.\r\n");

	/* Wait for all entities to become active. */
	while (num_cmd_readers < 3 && !quit)
		usleep (100000);

	if (quit)
		return;

	if (verbose) {
		printf ("Ctrl: start ... ");
		fflush (stdout);
	}
	ctrace_printd (USER_C_START, NULL, 0);
	send_cmd (cw, TEST_START, data_size, delay);

	while (l_c_cmd.cmd != TEST_DONE && !quit) {
		if (verbose == 2) {
			printf ("CD?");
			fflush (stdout);
		}
		usleep (500000);
	}
	ctrace_printd (USER_C_DONE, NULL, 0);
	if (verbose)
		printf ("done!\r\n");

	DDS_Subscriber_delete_datareader (sub, cr);
	DDS_Publisher_delete_datawriter (pub, cw);
}

static LCmd_t l_l_cmd;

DDS_DataReaderListener l_cr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	l_on_cmd_avail,
	NULL,
	NULL,
	(void *) &l_l_cmd
};

DDS_DataWriter	l_dw;

void l_on_l_data_avail (DDS_DataReaderListener *l, const DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	unsigned		i;
	LData_t			*sample;

	ARG_NOT_USED (l)

	if (verbose == 2) {
		printf ("L");
		fflush (stdout);
	}
	error = DDS_DataReader_take (dr, &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
	if (error) {
		if (error != DDS_RETCODE_NO_DATA)
			printf ("Unable to read data samples: error = %u!\r\n", error);
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++)
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			if (verbose == 2) {
				printf ("L");
				fflush (stdout);
			}
			sample = (LData_t *) DDS_SEQ_ITEM (rx_sample, i);
			DDS_DataWriter_write_w_timestamp (l_dw, sample, DDS_HANDLE_NIL,
						&DDS_SEQ_ITEM (rx_info, i)->source_timestamp);
			ctrace_printd (USER_LOOPED, NULL, 0);
		}

	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
}

DDS_DataReaderListener l_l_dr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	l_on_l_data_avail,
	NULL,
	NULL,
	NULL
};

void *l_do_loop (void *args)
{
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriterQos	wr_qos;
	DDS_DataReader	 	cr;
	DDS_DataReader		dr;

	ARG_NOT_USED (args)

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	cr = DDS_Subscriber_create_datareader (sub, cmd_topic_desc, &rd_qos, 
					&l_cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!");

	if (verbose)
		printf ("L: DDS Command Reader created.\r\n");

	/* Create Data Reader. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	if (reliable)
		rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dr = DDS_Subscriber_create_datareader (sub, data_tx_topic_desc, &rd_qos, 
					&l_l_dr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!dr)
		fatal ("Unable to create data reader!");

	if (verbose)
		printf ("L: DDS Data Reader created.\r\n");

	/* Create Data Writer. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	if (reliable)
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	l_dw = DDS_Publisher_create_datawriter (pub, data_rx_topic, &wr_qos, NULL, 0);
	if (!l_dw)
		fatal ("Unable to create data writer!");

	if (verbose)
		printf ("L: DDS Data Writer created.\r\n");

	while (!quit)
		sleep (1);

	DDS_Subscriber_delete_datareader (sub, cr);
	DDS_Subscriber_delete_datareader (sub, dr);
	DDS_Publisher_delete_datawriter (pub, l_dw);

	return (NULL);
}

static LCmd_t l_g_cmd;

DDS_DataReaderListener g_cr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	l_on_cmd_avail,
	NULL,
	NULL,
	(void *) &l_g_cmd
};

unsigned sample_count;

void l_rx_on_data_avail (DDS_DataReaderListener *l, const DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	unsigned		i;
	DDS_SampleInfo		*info;
	DDS_Time_t		t;
	double			stime, rtime;
	double			rtt, latency;

	ARG_NOT_USED (l)

	DDS_DomainParticipant_get_current_time (part, &t);
	rtime = t.sec + t.nanosec / (double) 1000000000;
	if (verbose == 2) {
		printf ("R");
		fflush (stdout);
	}
	error = DDS_DataReader_take (dr, &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
	if (error) {
		if (error != DDS_RETCODE_NO_DATA)
			printf ("Unable to read data samples: error = %u!\r\n", error);
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++) {
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			info = DDS_SEQ_ITEM (rx_info, i);
			stime = info->source_timestamp.sec + 
			        info->source_timestamp.nanosec / (double) 1000000000;
			rtt = rtime - stime;
			latency = rtt / 2.0;
			if (sample_count < nsamples)
				results [sample_count++] = latency * 1000000;

			ctrace_printd (USER_G_RECEIVED, NULL, 0);
		}
	}
	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
}

DDS_DataReaderListener l_rx_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	l_rx_on_data_avail,
	NULL,
	NULL,
	NULL
};

int dcmpf (const void *v1p, const void *v2p)
{
	const double	*d1p, *d2p;
	double		d;

	d1p = (const double *) v1p;
	d2p = (const double *) v2p;
	d = *d1p - *d2p;
	if (d < 0)
		return (-1);
	else if (d > 0)
		return (1);
	else
		return (0);
}

void *l_do_generator (void *args)
{
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriter	 	cw;
	DDS_DataReader	 	cr;
	DDS_DataWriter	 	dw;
	DDS_DataReader	 	dr;
	unsigned		i;
	double			total, min, max, avg, stdev;
	static LData_t		data;

	ARG_NOT_USED (args)

	/* Setup Command Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Writer. */
	cw = DDS_Publisher_create_datawriter (pub, cmd_topic, &wr_qos, NULL, 0);
	if (!cw)
		fatal ("Unable to create command writer!");

	if (verbose)
		printf ("G: DDS Command Writer created.\r\n");

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	cr = DDS_Subscriber_create_datareader (sub, cmd_topic_desc, &rd_qos, 
					&g_cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!");

	if (verbose)
		printf ("G: DDS Command Reader created.\r\n");

	/* Create Data Writer. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	if (reliable)
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dw = DDS_Publisher_create_datawriter (pub, data_tx_topic, &wr_qos, NULL, 0);
	if (!cw)
		fatal ("Unable to create data writer!");

	if (verbose)
		printf ("G: DDS Data Writer created.\r\n");

	/* Create Data Reader. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	if (reliable)
		rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dr = DDS_Subscriber_create_datareader (sub, data_rx_topic_desc, 
					       &rd_qos, 
					       &l_rx_listener,
					       DDS_DATA_AVAILABLE_STATUS);
	if (!dr)
		fatal ("Unable to create data reader!");

	if (verbose)
		printf ("G: DDS Data Reader created.\r\n");

	sample_count = 0;
	results = malloc (sizeof (double) * nsamples);

	while (l_g_cmd.cmd != TEST_START && !quit) {
		if (verbose == 2) {
			printf ("GC?");
			fflush (stdout);
		}
		usleep (100000);
	}
	ctrace_printd (USER_G_START, NULL, 0);

	memset (data.value, '#', l_g_cmd.dsize);
	data.value [l_g_cmd.dsize] = '\0';

	for (i = 0; i < nsamples; i++) {
		data.counter = i;
		if (verbose == 2) {
			printf ("W");
			fflush (stdout);
		}
		ctrace_printd (USER_G_SEND, NULL, 0);
		DDS_DataWriter_write (dw, &data, DDS_HANDLE_NIL);
		if (quit)
			break;

		usleep (l_g_cmd.delay * 1000);
	}
	ctrace_printd (USER_G_DONE, NULL, 0);

	send_cmd (cw, TEST_DONE, 0, 0);

	total = 0;
	min = 10000000;
	max = 0;
	qsort (results, nsamples, sizeof (double), dcmpf);
	for (i = 10; i < nsamples - 10; i++) {	/* Drop 20 extreme samples.*/
		total += results [i];
		if (results [i] < min)
			min = results [i];
		if (results [i] > max)
			max = results [i];
		if (dump)
			printf ("%5u: %12.4fus.\r\n", i, results [i]);
	}
	avg = total / (nsamples - 20);
	stdev = (results [nsamples / 2 - 1] + results [nsamples / 2]) / 2;
	printf ("%u bytes/sample, %u samples, %4.4fus min, %4.4fus max, %4.4fus avg, %4.4fus median\r\n",
				l_g_cmd.dsize, nsamples, min, max, avg, stdev);
	free (results);

	while (!quit)
		usleep (100000);

	DDS_Publisher_delete_datawriter (pub, cw);
	DDS_Subscriber_delete_datareader (sub, cr);
	DDS_Subscriber_delete_datareader (sub, dr);
	DDS_Publisher_delete_datawriter (pub, l_dw);

	return (NULL);
}

#ifdef DDS_SECURITY

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
		DDS_SP_set_participant_access (DDS_SP_add_participant (), strcat (realm_name, "*"), 2, 0);
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
	error = DDS_Security_set_credentials ("Technicolor Latency tester", &credentials);
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
	thread_t		lt, gt;
	int			error;

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
	DDS_entity_name ("Technicolor Latency tester");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif
#ifdef DDS_DEBUG
        DDS_Debug_start ();
        DDS_Debug_abort_enable (&quit);
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
	/*DDS_CTrace_mode (1); ** Cyclic trace mode. */
#endif

	/* Register the message topic type. */
	error = register_latency_types (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_register_type () failed!");
	}
	if (verbose)
		printf ("DDS Topic types registered.\r\n");

	/* Create command topic. */
	cmd_topic = DDS_DomainParticipant_create_topic (part, "LC", "LCmd",
									NULL, NULL, 0);
	if (!cmd_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('LC') failed!");
	}
	if (verbose)
		printf ("DDS LC Topic created.\r\n");

	/* Create Topic Description. */
	cmd_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "LC");
	if (!cmd_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'LC'!");
	}

	/* Create data Tx topic. */
	data_tx_topic = DDS_DomainParticipant_create_topic (part, "LDT", "LData",
									NULL, NULL, 0);
	if (!data_tx_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('LDT') failed!");
	}
	if (verbose)
		printf ("DDS LDT Topic created.\r\n");

	/* Create data Tx Topic Description. */
	data_tx_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "LDT");
	if (!data_tx_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'LDT'!");
	}

	/* Create data Rx topic. */
	data_rx_topic = DDS_DomainParticipant_create_topic (part, "LDR", "LData",
									NULL, NULL, 0);
	if (!data_rx_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('LDR') failed!");
	}
	if (verbose)
		printf ("DDS LDR Topic created.\r\n");

	/* Create data Rx Topic Description. */
	data_rx_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "LDR");
	if (!data_rx_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'LDR'!");
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

	/* Start either a reader or a writer or both, depending on program options. */
	if (loop) {
		if (controller || generator)
			thread_create (lt, l_do_loop, part);
		else
			l_do_loop (NULL);
	}
	if (generator) {
		if (controller || loop)
			thread_create (gt, l_do_generator, part);
		else
			l_do_generator (NULL);
	}
	if (controller)
		l_do_controller ();

#ifdef DDS_DEBUG
	sleep (100);
#endif

	quit = 1;
	if (loop && (controller || generator))
		thread_wait (lt, NULL);
	if (generator && (controller || loop))
		thread_wait (gt, NULL);

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	free_latency_types ();
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

