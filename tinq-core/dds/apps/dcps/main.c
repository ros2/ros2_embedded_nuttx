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
#include <ctype.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
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
#include "dds/dds_debug.h"
#include "dds/dds_trans.h"
#include "dds/dds_aux.h"
#ifdef XTYPES_USED
#include "dds/dds_xtypes.h"
#endif

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
/*#define TRACE_DATA	** Define to trace data endpoints. */
/*#define NORMAL_TAKE	** Define to take items in reader. */
/*#define SAMPLE_INFO	** Define to show sample info. */
/*#define SLOW_RREAD	** Slow consumption. */
#define EXTRA_READER	/* Define to create an extra reader in writer mode. */
/*#define WRDISPATCH	** Dispatch from extra reader to extra thread. */
/*#define EXTRA_TAKE	** Define to take items in extra reader. */
/*#define DO_DISC_LISTEN * Listen on discovery info. */
#define DDL_DELAY 2	/* Delay (seconds) until Discovery Readers started. */
#define	RELIABLE	/* Use reliable transfer mode. */
#define TRANSIENT_LOCAL	/* Use TRANSIENT-LOCAL mode. */
/*#define KEEP_ALL	** Use KEEP_ALL history. */
#define	HISTORY	1	/* History depth. */
/*#define NO_KEY	** Use keyless data. */
#ifndef NO_KEY
#define DO_UNREGISTER	/* Define to unregister instances. */
/*#define UNREG_DISC	** Force unregister/dispose on discovery. */
#define LARGE_KEY	/* Use a large key (>16 bytes). */
/*#define SINGLE_INST	** One instance only. */
/*#define DUAL_INST	** Two instances: 1 direct in listener, 1 dispatched. */
#endif
#define	ADD_PID		/* Define to add the PID to data samples. */
#define DYNMSG		/* Dynamic message data. */
/*#define LIVELINESS_MODE DDS_AUTOMATIC_LIVELINESS_QOS*/
/*#define LIVELINESS_MODE DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS*/
/*#define LIVELINESS_MODE DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS*/
/*#define DEADLINE	** Define to use the Deadline QoS policy. */
/*#define LIFESPAN	** Define to use the Lifespan QoS policy. */
/*#define AUTOPURGE_NW	** Define to use the RDL-no-writers QoS policy. */
/*#define AUTOPURGE_D	** Define to use the RDL-disposed QoS policy. */
/*#define LRTBFILTER 1	** Local Reader time-based filter separation. */
/*#define NRTBFILTER 2	** Normal Reader time-based filter separation. */
/*#define TRANS_PARS	** Define this to use alternative port mappings. */
#define	BULK_DATA	/* Define this for bulk data transport. */
/* #define PART_UPD	** Define this for partition string updates. */
#define	MULTI_RUN    1	/* # of runs. */
/*#define DYN_CFG	** Dynamically define a Dynamic secure TCP server. */
#define LOG_DATA	/* Define this to add data messages to the log file. */
#ifdef LOG_DATA
#include "log.h"
#include "error.h"
#endif

#define	HELLO_WORLD	"Hello DDS world!"
#define MAX_DSIZE	0x20000		/* 128KB */
#ifdef DYNMSG
#define MSG_SIZE	0
#else
#define MSG_SIZE	MAX_DSIZE
#endif

const char		*progname;
int			writer;			/* Default: reader. */
int			verbose;		/* Verbose if set. */
int			trace;			/* Trace messages if set. */
int			aborting;		/* Abort program if set. */
int			quit_done;		/* Quit when Tx/Rx done. */
int			paused;			/* Pause program if set. */
int			extra_dp;		/* Create extra participant. */
int			no_rtps;		/* Don't use RTPS if set. */
unsigned		domain_id;		/* Domain id. */
int                     tests;
int                     file;
char                    *fname;
int                     sfile;
char                    *sname;
int			dbg_server;		/* Start a debug server. */
unsigned		dbg_port;		/* Debug server port. */
#ifdef DDS_SECURITY
char			*engine_id;		/* Engine id. */
char			*cert_path;		/* Certificates path. */
char			*key_path;		/* Private key path. */
#endif
char			*realm_name;		/* Realm name. */
#ifdef EXTRA_READER
unsigned		nreaders = 1;		/* # of local readers. */
DDS_DataReader		ldr [3];		/* Local readers. */
#endif
unsigned		data_size;		/* Default: {"hello world", count} */
unsigned		max_size;		/* Max. data size. */
unsigned		inc_size = 1;		/* Increment size. */
unsigned		nsamples;		/* Current # of samples done. */
unsigned		burst_amount = 1;	/* # of samples per burst. */
unsigned		nruns = MULTI_RUN;	/* # of runs. */
size_t			cur_size;		/* Current sample size. */
unsigned		max_samples = ~0;	/* Max. # of sends/receives before exit. */
unsigned		max_sends = ~0;		/* Max. # of sends before pause. */
unsigned		sleep_time = 1000;	/* Sleep time (1000ms = 1s). */
unsigned		start_delay;
unsigned char		buf [MAX_DSIZE];	/* Data buffer to use. */
DDS_Topic		topic;
DDS_TopicDescription	topic_desc;
DDS_DataWriter		w;
const char *kind_str [] = {
	NULL,
	"ALIVE",
	"NOT_ALIVE_DISPOSED",
	NULL,
	"NOT_ALIVE_NO_WRITERS"
};
int clientTest = 0;

typedef struct msg_data_st {
	uint64_t	counter;
#ifndef NO_KEY
#ifdef LARGE_KEY
	uint32_t	key [5];
#else
	uint32_t	key;
#endif
#endif
#ifdef DYNMSG
#define	DYNF		2
	char		*message;
#else
#define	DYNF		0
	char		message [MSG_SIZE];
#endif
} MsgData_t;

#define	MSG_HDR_SIZE	offsetof (struct msg_data_st, message)

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "Usage: dds [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -w             Act as a writer.\r\n");
	fprintf (stderr, "   -r             Act as a reader (default).\r\n");
#ifdef EXTRA_READER
	fprintf (stderr, "   -0..3          Number of local readers (default=1).\r\n");
#endif
	fprintf (stderr, "   -s <size>      Data size to write in writer mode.\r\n");
	fprintf (stderr, "                  Default it sends a 4-byte counter, followed\r\n");
	fprintf (stderr, "                  by a 'hello world' type string.\r\n");
	fprintf (stderr, "   -m <size>      Maximum data size in writer mode.\r\n");
	fprintf (stderr, "                  If set, will continuously increment the data\r\n");
	fprintf (stderr, "                  until the maximum size and restarts with the\r\n");
	fprintf (stderr, "                  minimum size.\r\n");
	fprintf (stderr, "   -a <size>      Offset to add for each size increment.\r\n");
	fprintf (stderr, "   -n <count>     Max. # of times to send/receive data.\r\n");
	fprintf (stderr, "   -b <count>     # of messages per burst.\r\n");
	fprintf (stderr, "   -q             Quit when all packets sent/received.\r\n");
	fprintf (stderr, "   -f             Flood mode (no waiting: as fast as possible).\r\n");
	fprintf (stderr, "   -d <msec>      Max. delay to wait between bursts (10..10000).\r\n");
	fprintf (stderr, "   -p             Startup in paused state.\r\n");
	fprintf (stderr, "   -t             Trace transmitted/received messages.\r\n");
	fprintf (stderr, "   -i <num>       Domain id to use.\r\n");
	fprintf (stderr, "   -u <num>       Number of runs before closing down.\r\n");
	fprintf (stderr, "   -e <name>      Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>      Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>      Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm>     The realm name.\r\n");
	fprintf (stderr, "   -x             Create secondary domain participant.\r\n");
	fprintf (stderr, "   -l             No lower layer RTPS functionality.\r\n");
	fprintf (stderr, "   -g [<num>]     Start a debug server.\r\n");
	fprintf (stderr, "   -y <filename>  Write received data to file.\r\n");
        fprintf (stderr, "   -o <delay>     Specific command only for automated tests.\r\n");
        fprintf (stderr, "                  + adds a startup delay.\r\n");
	fprintf (stderr, "   -j <filename>  Specify a security.xml file.\r\n");
	fprintf (stderr, "   -v             Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv            Extra verbose: log detailed functionality.\r\n");
	exit (1);
}

/* get_num -- Get a number from the command line arguments. */

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
				case 'r':
					writer = 0;
					break;
				case 'w':
					writer = 1;
					break;
				case '0':
					nreaders = 0;
					break;
				case '1':
					nreaders = 1;
					break;
				case '2':
					nreaders = 2;
					break;
				case '3':
					nreaders = 3;
					break;
				case 's':
					INC_ARG()
					if (!get_num (&cp, &data_size, 16 + MSG_HDR_SIZE, MAX_DSIZE, "-s"))
						usage ();
					break;
				case 'm':
					INC_ARG()
					if (!data_size ||
					    !get_num (&cp, &max_size, data_size + 1, MAX_DSIZE, "-m"))
						usage ();
					break;
				case 'a':
					INC_ARG()
					if (!data_size || !max_size ||
					    !get_num (&cp, &inc_size, 1, max_size - data_size, "-a"))
						usage ();
					break;
				case 'n':
					INC_ARG()
					if (!get_num (&cp, &max_samples, 0, ~0, "-n"))
						usage ();
					break;
				case 'b':
					INC_ARG()
					if (!get_num (&cp, &burst_amount, 1, 100, "-b"))
						usage ();
					break;
				case 'f':
					sleep_time = 0;
					break;
				case 'd':
					INC_ARG()
					if (!get_num (&cp, &sleep_time, 10, 10000, "-d"))
						usage ();
					break;
				case 'p':
					paused = 1;
					break;
				case 'q':
					quit_done = 1;
					break;
				case 't':
					trace = 1;
					break;
				case 'u':
					INC_ARG()
					if (!get_num (&cp, &nruns, 1, 1000, "-u"))
						usage ();
					break;
				case 'v':
					verbose = 1;
					if (*cp == 'v') {
						verbose = 2;
						cp++;
					}
					break;
				case 'l':
					no_rtps = 1;
					break;
				case 'i':
					INC_ARG ()
					if (!get_num (&cp, &domain_id, 0, 256, "-i"))
						usage ();
					break;
				case 'g':
					if (!*cp &&
					    argv [i + 1] &&
					    *argv [i + 1] >= '0' &&
					    *argv [i + 1] <= '9') {
						INC_ARG()
						if (!get_num (&cp, &dbg_port, 1024, 65535, "-g"))
							usage ();
					}
					dbg_server = 1;
					break;
#ifdef DDS_SECURITY
			        case 'e':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage();
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
				case 'x':
					extra_dp = 1;
					break;
				case 'h':
					fprintf (stderr, "dds -- test program for the DDS protocol.\r\n");
					usage ();
					break;
			        case 'y':
					file = 1;
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage();
					fname = malloc (strlen (arg_input) + 1);
					strcpy (fname, arg_input);
					break;
			        case 'o':
					INC_ARG ()
					if (!get_num (&cp, &start_delay, 0, 20000, "-o"))
						usage ();
					tests = 1;
					break;
			        case 'j':
					INC_ARG ()
					if (!get_str (&cp, &arg_input))
						usage ();
					sname = malloc (strlen (arg_input) + 1);
					strcpy (sname, arg_input);
					sfile = 1;
					break;
				default:
					usage ();
					break;
			}
		}
	}
	return (i);
}

#define	NMSGS	4

typedef struct msg_desc_st {
#ifndef NO_KEY
#ifdef LARGE_KEY
	unsigned	key [5];
#else
	unsigned	key;
#endif
#endif
	const char	*data;
	DDS_InstanceHandle_t handle;
} MsgDesc_t;

#ifndef NO_KEY
#ifdef LARGE_KEY
#define	_KS	{
#define	_KE	,}
#else
#define	_KS
#define	_KE
#endif
#define	_(x,y)	x,
#define	NF	3
#else
#define	_(x,y)
#define	NF	2
#endif

MsgDesc_t messages [NMSGS] = {
	{ _(_KS  0 _KE,) "Hello DDS world!", 0},
	{ _(_KS  6 _KE,) "Aint't this a pretty sight?", 0 },
	{ _(_KS 22 _KE,) "Having fun with a mighty middleware :-)", 0 },
	{ _(_KS 33 _KE,) "And the last one to conclude the deal!", 0 }
};

#ifndef NO_KEY
#define KEYF 1
#else
#define KEYF 0
#endif
static DDS_TypeSupport_meta msg_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT, DYNF|KEYF, "HelloWorldData", sizeof (struct msg_data_st), 0, NF, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "counter", 0, offsetof (struct msg_data_st, counter), 0, 0, NULL },
#ifndef NO_KEY
#ifdef LARGE_KEY
	{ CDR_TYPECODE_ARRAY,  1, "key", sizeof (unsigned [5]), offsetof (struct msg_data_st, key), 5, 0, NULL },
	{ CDR_TYPECODE_ULONG,  1, NULL, 0, 0, 0, 0, NULL },
#else
	{ CDR_TYPECODE_ULONG,  1, "key", 0, offsetof (struct msg_data_st, key), 0, 0, NULL },
#endif
#endif
	{ CDR_TYPECODE_CSTRING, DYNF, "message", MSG_SIZE, offsetof (struct msg_data_st, message), 0, 0, NULL }
};

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

#if 0
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
				printf ("   %s\r\n", ascii);
			printf ("  %05u:", i);
		}
		else if ((i & 0x7) == 0)
			printf (" -");
		c = *dp++;
		ascii [i & 0xf] = (c >= ' ' && c <= '~') ? c : '.';
		printf (" %02x", c);
	}
	while ((i & 0xf) != 0) {
		printf ("   ");
		if ((i & 0x7) == 0)
			printf ("  ");
		ascii [i & 0xf] = ' ';
		i++;
	}
	printf ("   %s\r\n", ascii);
}
#endif

static void do_write (void)
{
	MsgData_t	data;
	MsgDesc_t	*dp;
	unsigned	op, index, i, c;
#ifndef NO_KEY
	unsigned	inst_delta;
#endif
	int		error;
	char		*dbuf = NULL;
	static DDS_InstanceHandle_t h [4];
	char		buffer [256];

	if (paused)
		return;

	for (i = 0; i < burst_amount; i++) {
#ifdef SINGLE_INST
		index = 0;
		op = 1;
#elif defined (DUAL_INST)
		index = nsamples & 1;
		op = 1;
#else
		index = nsamples & 3;
		op = (nsamples & 0xc) >> 2;
#endif
		dp = &messages [index];
#ifndef NO_KEY
#ifdef DUAL_INST
		inst_delta = nsamples & 1;
#else
		inst_delta = (nsamples & 0x30) >> 4;
#endif
#ifdef LARGE_KEY
#ifdef SINGLE_INST
		data.key [0] = dp->key [0];
#else
		data.key [0] = dp->key [0] + inst_delta;
#endif
#ifdef ADD_PID
		data.key [0] += getpid () & 0xf;
#endif
		data.key [1] = getpid ();
		data.key [2] = data.key [3] = data.key [4] = 0;
#else
#ifdef SINGLE_INST
		data.key = dp->key;
#else
		data.key = dp->key + inst_delta;
#endif
#ifdef ADD_PID
		data.key += getpid () & 0xf;
#endif
#endif
#endif

#ifndef DYNMSG
		dbuf = data.message;
#endif
		switch (op) {
			case 0:
#ifndef NO_KEY
				h [index] = DDS_DataWriter_register_instance (w, &data);
				if (verbose)
					printf ("DDS-W: [%2u] Registered instance.\r\n", h [index]);
#endif
			case 1:
			case 2:
			case 3:
				/*if (trace)
					trace_data (buf, cur_size);*/
			
				data.counter = nsamples;
				if (cur_size) {
#ifdef DYNMSG
					data.message = dbuf = malloc (cur_size - MSG_HDR_SIZE);
					if (!data.message) {
						printf ("DDS-W: No memory for dynamic sample data!");
						exit (1);
					}
#endif
					sprintf (dbuf, "%9lu bytes ", (unsigned long) cur_size);
					memcpy (dbuf + 16, buf, cur_size - MSG_HDR_SIZE - 16);
					dbuf [cur_size - MSG_HDR_SIZE - 1] = '\0';
				}
				else {
#ifdef DYNMSG
					data.message = (char *) dp->data;
#else
					if (strlen (dp->data) + 1 > MSG_SIZE) {
						memcpy (data.message, dp->data, MSG_SIZE - 1);
						data.message [MSG_SIZE - 1] = '\0';
					}
					else
						strcpy (data.message, dp->data);
#endif
				}
				if (verbose || trace) {
					c = snprintf (buffer, 255, "DDS-W: [%2u] ALIVE - %2u :%6llu - ", 
								h [index],
#ifndef NO_KEY
#ifdef LARGE_KEY
								data.key [0],
#else
								data.key,
#endif
#else
								0,
#endif
								(unsigned long long) data.counter);
					if (trace || !cur_size)
						snprintf (buffer + c, 255 - c, "'%s'\r\n", data.message);
					else
						snprintf (buffer + c, 255 - c, "%9lu bytes\r\n", (unsigned long) cur_size);
#ifdef LOG_DATA
					log_printf (USER_ID, 0, "%s", buffer);
#else
					fprintf (stdout, "%s", buffer);
#endif
				}
				do {
					error = DDS_DataWriter_write (w, &data, h [index]);
					/*if (error) {
						printf ("DDS_DataWriter_write() failed! (error=%u)\r\n", error);
						break;
					}*/
				}
				while (error);
#ifdef DYNMSG
				if (cur_size)
					free (dbuf);
#endif
				nsamples++;
				if (op < 3)
					break;

#ifdef DO_UNREGISTER
				if (h [index] >= 8) {
					if (nsamples == 45)
						error = 0;
					if (verbose) {
						snprintf (buffer, 255, "DDS-W: [%2u] Unregister instance\r\n", h [index]);
#ifdef LOG_DATA
						log_printf (USER_ID, 0, "%s", buffer);
#else
						fprintf (stdout, "%s", buffer);
#endif
					}
					do {
						error = DDS_DataWriter_unregister_instance (w, &data, h [index]);
						/*if (error) {
							printf ("DDS_DataWriter_unregister_instance() failed! (error=%u)\r\n", error);
							break;
						}*/
					}
					while (error);
				}
				else {
					if (verbose) {
						snprintf (buffer, 255, "DDS-W: [%2u] Dispose instance\r\n", h [index]);
#ifdef LOG_DATA
						log_printf (USER_ID, 0, "%s", buffer);
#else
						fprintf (stdout, "%s", buffer);
#endif
					}
					do {
						error = DDS_DataWriter_dispose (w, &data, h [index]);
						/*if (error) {
							printf ("DDS_DataWriter_dispose() failed! (error=%u)\r\n", error);
							break;
						}*/
					}
					while (error);
				}
#endif
				break;
		}
		if (cur_size < max_size && cur_size + inc_size > max_size)
			cur_size = max_size;
		else {
			cur_size += inc_size;
			if (cur_size > max_size)
				cur_size = data_size;
		}
		if (!--max_sends) {
			paused = 1;
			break;
		}
	}
}

void dr_rejected (DDS_DataReaderListener  *self,
		 DDS_DataReader           reader,
		 DDS_SampleRejectedStatus *status)
{
	static const char *reason_str [] = {
		"none", "Instance", "Samples", "Samples_per_instance"
	};

	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)

	printf ("DDS-R: Sample Rejected - T=%d, TC=%d - %s - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			reason_str [status->last_reason],
			status->last_instance_handle);
}

void dr_liveliness (DDS_DataReaderListener      *self,
		    DDS_DataReader              reader,
		    DDS_LivelinessChangedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)

	printf ("DDS-R: Liveliness Changed - A=%d, NA=%d, AC=%d, NAC=%d, {%u}\r\n",
			status->alive_count,
			status->not_alive_count,
			status->alive_count_change,
			status->not_alive_count_change,
			status->last_publication_handle);
}

void dr_deadline (DDS_DataReaderListener            *self,
		  DDS_DataReader                    reader,
		  DDS_RequestedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)

	printf ("DDS-R: Requested Deadline Missed - T=%d, TC=%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->last_instance_handle);
}

void dbg_print_policies (DDS_QosPolicyCountSeq *seq)
{
	DDS_QosPolicyCount	*p;
	unsigned		i;

	if (DDS_SEQ_LENGTH (*seq)) {
		DDS_SEQ_FOREACH_ENTRY (*seq, i, p) {
			if (i)
				printf (", ");
			else
				printf ("\t");
			printf ("%s:%d", DDS_qos_policy (p->policy_id), p->count);
		}
		printf ("\r\n");
	}
}

void dr_inc_qos (DDS_DataReaderListener             *self,
		 DDS_DataReader                     reader,
		 DDS_RequestedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)

	printf ("DDS-R: Requested Incompatible QoS - T=%d, TC=%d, L:%s\r\n",
			status->total_count,
			status->total_count_change,
			DDS_qos_policy (status->last_policy_id));
	dbg_print_policies (&status->policies);
}

void dr_smatched (DDS_DataReaderListener        *self,
		  DDS_DataReader                reader,
		  DDS_SubscriptionMatchedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)

	printf ("DDS-R: Subscription Matched - T:%d, TC:%d, C:%d, CC:%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->current_count,
			status->current_count_change,
			status->last_publication_handle);
}

void dr_lost (DDS_DataReaderListener *self,
	      DDS_DataReader         reader,
	      DDS_SampleLostStatus   *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (reader)

	printf ("DDS-R: Sample Lost - T:%d, TC:%d\r\n",
			status->total_count,
			status->total_count_change);
}

#ifdef EXTRA_READER
#ifdef WRDISPATCH
static cond_t lr_cond;
static lock_t lr_lock;
static int lr_ready = 0;
#endif

void write_to_file (char *buffer)
{
	FILE *f = fopen (fname, "a");

	if (f == NULL) {
		printf ("Error opening file!\r\n");
		exit(1);
	}

	/* write buffer */
	fprintf (f, "%s", buffer);
	fclose (f);
}

void lr_data_avail_inst (DDS_DataReaderListener *l,
	                 DDS_DataReader         dr,
			 DDS_InstanceHandle_t   h)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	MsgData_t		*sample;
	DDS_ReturnCode_t	error;
	unsigned		key0;
	char                    buffer [256];
	int                     c;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		if (!h)
#ifdef EXTRA_TAKE
			error = DDS_DataReader_take (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#else
			error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#endif
		else
			error = DDS_DataReader_take_instance (dr, &rx_sample, &rx_info, 1, h, ss, vs, is);

		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				printf ("    Unable to read samples: error = %u!\r\n", error);
#ifdef WRDISPATCH
			printf ("    No more data ...\r\n");
#endif
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
#ifdef SAMPLE_INFO
				printf ("SampleInfo: ss:%u,vs:%u,is:%u,dg:%u,nwg:%u,sr:%u,gr:%u,agr:%u\r\n", 
						info->sample_state,
						info->view_state,
						info->instance_state,
						info->disposed_generation_count,
						info->no_writers_generation_count,
						info->sample_rank,
						info->generation_rank,
						info->absolute_generation_rank);
#endif
				if (info->valid_data) {

				/*instance_state == DDS_ALIVE_INSTANCE_STATE) {*/
#ifndef NO_KEY
#ifdef LARGE_KEY
					key0 = sample->key [0];
#else
					key0 = sample->key;
#endif
#else
					key0 = 0;
#endif

					c = snprintf (buffer, 255, "DDS-R: [%2u] ALIVE - %2u :%6llu - ", 
						      info->instance_handle,
						      key0,
						      (unsigned long long) sample->counter);
					if (trace || sample->message [10] != 'b')
						snprintf (buffer + c, 255 - c, "'%s'\r\n", sample->message);
					else
						snprintf (buffer + c, 255 - c, "%9lu bytes.\r\n", (unsigned long) strlen (sample->message) + MSG_HDR_SIZE + 1);
					if (file)
						write_to_file (buffer);
#ifdef LOG_DATA
					log_printf (USER_ID, 0, "%s", buffer);
#else
					fprintf (stdout, "%s", buffer);
#endif
				}
				else if (info->instance_state != DDS_ALIVE_INSTANCE_STATE) {
					snprintf (buffer, 255, "DDS-R: [%2u] %s\r\n",
						  info->instance_handle,
						  kind_str [info->instance_state]);
#ifdef LOG_DATA
					log_printf (USER_ID, 0, "%s", buffer);
#else
					fprintf (stdout, "%s", buffer);
#endif
				}
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
#ifdef SLOW_RREAD
			if (key0 == 0) {
				printf ("    Sleep a bit ... \r\n");
				sleep (5);
				printf ("    Wakeup ... \r\n");
			}
#endif
		}
		else {
			printf ("    do_read: all read!\r\n");
			return;
		}
	}
}

#ifdef WRDISPATCH

void dr_data_avail (DDS_DataReaderListener *l,
	            DDS_DataReader         dr)
{
#if defined (SINGLE_INST) || defined (DUAL_INST)
	unsigned	i, prev_ready;
	MsgData_t	sample;
	DDS_InstanceHandle_t h;

	ARG_NOT_USED (l)

	printf ("Listener!\r\n");

#ifdef LARGE_KEY
	sample.key [0] = messages [0].key [0];
	sample.key [1] = sample.key [2] = sample.key [3] = sample.key [4] = 0;
#else
	sample.key = messages [0].key;
#endif
	h = DDS_DataReader_lookup_instance (dr, &sample);
	if (h) {
		messages [0].handle = h;
		printf ("(L) Dispatched instance ...\r\n");
		for (i = 0; i < nreaders; i++)
			if (ldr [i] == dr)
				break;

		if (i == nreaders)
			return;

		printf ("(L) Take lock ... \r\n");
		lock_take (lr_lock);
		prev_ready = lr_ready;
		lr_ready |= (1 << i);
		if (!prev_ready) {
			printf ("(L) Got data: send signal!\r\n");

			cond_signal (lr_cond);
		}
		lock_release (lr_lock);
		printf ("(L) Release lock ... \r\n");
	}
#ifdef LARGE_KEY
	sample.key [0] = messages [1].key [0] + 1;
	sample.key [1] = sample.key [2] = sample.key [3] = sample.key [4] = 0;
#else
	sample.key = messages [1].key + 1;
#endif
	h = DDS_DataReader_lookup_instance (dr, &sample);
	if (h) {
		messages [1].handle = h;
		printf ("(L) In-listener instance ...\r\n");
		lr_data_avail_inst (l, dr, h);
	}
#else
	lr_data_avail_inst (l, dr, 0);
#endif
	printf ("(L) Done\r\n");
}

#endif

void lr_data_avail (DDS_DataReaderListener *l,
	            DDS_DataReader         dr)
{
	lr_data_avail_inst (l, dr, 0);
}

static DDS_DataReaderListener r_listener = {
	dr_rejected,	/* Sample rejected. */
	dr_liveliness,	/* Liveliness changed. */
	dr_deadline,	/* Requested Deadline missed. */
	dr_inc_qos,	/* Requested incompatible QoS. */
#ifdef WRDISPATCH
	dr_data_avail,	/* Data available. */
#else
	lr_data_avail,	/* Data available. */
#endif
	dr_smatched,	/* Subscription matched. */
	dr_lost,	/* Sample lost. */
	NULL		/* Cookie */
};

#ifdef WRDISPATCH

thread_t	lr_thread;

thread_result_t lr_consumer (void *arg)
{
	unsigned	i;

	ARG_NOT_USED (arg)

	for (; !aborting; ) {
		lock_take (lr_lock);
		if (!lr_ready) {
			printf ("(C) Np more data: wait for signal!\r\n");
			cond_wait (lr_cond, lr_lock);
			printf ("(C) Got signal.\r\n");
		}
		lock_release (lr_lock);
		do {
			for (i = 0; i < nreaders; i++)
				if ((lr_ready & (1 << i)) != 0) {
					printf ("(C) Take lock.\r\n");
					lock_take (lr_lock);
					lr_ready &= ~(1 << i);
					lock_release (lr_lock);
					printf ("(C) Release lock.\r\n");
					printf ("(C) Dispatched data!\r\n");
					lr_data_avail_inst (&r_listener, ldr [i], messages [0].handle);
					printf ("(C) Data processed\r\n");
				}
		}
		while (lr_ready);
	}
	thread_return (NULL);
}

#endif
#endif

void dw_deadline (DDS_DataWriterListener          *self,
		  DDS_DataWriter                  writer,
		  DDS_OfferedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)

	printf ("DDS-W: Offered Deadline Missed - T=%d, TC=%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->last_instance_handle);
}

void dw_pmatched (DDS_DataWriterListener       *self,
		  DDS_DataWriter               writer,
		  DDS_PublicationMatchedStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)

	printf ("DDS-W: Publication Matched - T:%d, TC:%d, C:%d, CC:%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->current_count,
			status->current_count_change,
			status->last_subscription_handle);
}

void dw_liveliness (DDS_DataWriterListener   *self,
		    DDS_DataWriter           writer,
		    DDS_LivelinessLostStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)

	printf ("DDS-W: Liveliness Lost - T:%d, TC:%d\r\n",
			status->total_count,
			status->total_count_change);
}

void dw_inc_qos (DDS_DataWriterListener           *self,
		 DDS_DataWriter                   writer,
		 DDS_OfferedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (writer)

	printf ("DDS-W: Offered Incompatible QoS - T=%d, TC=%d, L:%s\r\n",
			status->total_count,
			status->total_count_change,
			DDS_qos_policy (status->last_policy_id));
	dbg_print_policies (&status->policies);
}

static DDS_DataWriterListener w_listener = {
	dw_deadline,	/* Offered Deadline missed. */
	dw_pmatched,	/* Publication matched. */
	dw_liveliness,	/* Liveliness lost. */
	dw_inc_qos,	/* Offered Incompatible QoS. */
	NULL
};

void dcps_do_writer (DDS_DomainParticipant part)
{
	DDS_Publisher		pub;
	DDS_PublisherQos	pqos;
	char			c;
	int			error;
	unsigned		i;
	DDS_DataWriterQos 	wr_qos, dwqos;
#ifdef EXTRA_READER
	DDS_DataReaderQos	rd_qos;
	DDS_Subscriber		sub;
#ifdef PART_UPD
	DDS_SubscriberQos	sqos;
#endif
#endif
	DDS_StatusMask		sm;
#ifdef PART_UPD
	char			*s0 = "Hi", *s1 = "Folks", *s2 = "cya", *s3 = "bye";
#endif

	if (data_size) {
		if (!max_size)
			max_size = data_size;
		for (i = 0, c = ' ' + 1; i < max_size; i++) {
			buf [i] = c++;
			if (c > '~')
				c = ' ' + 1;
		}
	}

	/* Create a publisher. */
	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		fatal ("DDS_DomainParticipant_create_publisher () failed!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Publisher created.\r\n");

	/* Test get_qos() fynctionality. */
	if ((error = DDS_Publisher_get_qos (pub, &pqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Publisher_get_qos () failed (%s)!", DDS_error (error));

	/* Setup writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
#ifdef TRANSIENT_LOCAL
	wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
#ifdef RELIABLE
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#endif
#ifdef KEEP_ALL
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.history.depth = DDS_LENGTH_UNLIMITED;
	wr_qos.resource_limits.max_samples_per_instance = HISTORY;
	wr_qos.resource_limits.max_instances = HISTORY * 10;
	wr_qos.resource_limits.max_samples = HISTORY * 4;
#else
	wr_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
	wr_qos.history.depth = HISTORY;
#endif
#ifdef LIVELINESS_MODE
	wr_qos.liveliness.kind = LIVELINESS_MODE;
	wr_qos.liveliness.lease_duration.sec = 1;
	wr_qos.liveliness.lease_duration.nanosec = 0;
#endif
#ifdef DEADLINE
	wr_qos.deadline.period.sec = 30;
	wr_qos.deadline.period.nanosec = 0;
#endif
#ifdef LIFESPAN
	wr_qos.lifespan.duration.sec = 60;
	wr_qos.lifespan.duration.nanosec = 0;
#endif
#ifdef TRACE_DATA
	DDS_Trace_defaults_set (DDS_TRACE_ALL);
#endif
	/* Create a Data Writer. */
	sm = DDS_OFFERED_DEADLINE_MISSED_STATUS |
	     DDS_OFFERED_INCOMPATIBLE_QOS_STATUS |
	     DDS_LIVELINESS_LOST_STATUS |
	     DDS_PUBLICATION_MATCHED_STATUS;
	w = DDS_Publisher_create_datawriter (pub, topic, &wr_qos, &w_listener, sm);
	if (!w) {
		fatal ("Unable to create a writer \r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Writer created.\r\n");

#ifdef UNREG_DISC
	usleep (5000000);
	for (i = 0; i < 3; i++) {
		usleep (500000);
		DDS_Publisher_delete_datawriter (pub, w);
		usleep (500000);
		w = DDS_Publisher_create_datawriter (pub, topic, &wr_qos, &w_listener, sm);
		if (!w) {
			fatal ("Unable to create a writer (2) \r\n");
			DDS_DomainParticipantFactory_delete_participant (part);
		}
		if (verbose)
			printf ("DDS Writer created (2).\r\n");
	}
#endif

	/* Test get_qos() fynctionality. */
	if ((error = DDS_DataWriter_get_qos (w, &dwqos)) != DDS_RETCODE_OK)
		fatal ("DDS_DataWriter_get_qos () failed (%s)!", DDS_error (error));

	/*trc_lock_info ();*/

#ifdef EXTRA_READER
	sub = DDS_DomainParticipant_create_subscriber (part, NULL, NULL, 0); 
	if (!sub) {
		fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

#ifdef PART_UPD

	/* Test get_qos() fynctionality. */
	if ((error = DDS_Subscriber_get_qos (sub, &sqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Subscriber_get_qos () failed (%s)!", DDS_error (error));
#endif

	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
#ifdef TRANSIENT_LOCAL
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
#ifdef RELIABLE
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#endif
#ifdef KEEP_ALL
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.history.depth = DDS_LENGTH_UNLIMITED;
	rd_qos.resource_limits.max_samples_per_instance = HISTORY;
	rd_qos.resource_limits.max_instances = HISTORY * 10;
	rd_qos.resource_limits.max_samples = HISTORY * 4;
#else
	rd_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
	rd_qos.history.depth = HISTORY;
#endif
#ifdef LIVELINESS_MODE
	rd_qos.liveliness.kind = LIVELINESS_MODE;
	rd_qos.liveliness.lease_duration.sec = 9;
	rd_qos.liveliness.lease_duration.nanosec = 0;
#endif
#ifdef DEADLINE
	rd_qos.deadline.period.sec = 40;
	rd_qos.deadline.period.nanosec = 0;
#endif
#ifdef AUTOPURGE_NW
	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.sec = 60;
	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.nanosec = 0;
#endif
#ifdef AUTOPURGE_D
	rd_qos.reader_data_lifecycle.autopurge_disposed_samples_delay.sec = 65;
	rd_qos.reader_data_lifecycle.autopurge_disposed_samples_delay.nanosec = 0;
#endif
#ifdef LRTBFILTER
	rd_qos.time_based_filter.minimum_separation.sec = LRTBFILTER;
	rd_qos.time_based_filter.minimum_separation.nanosec = 0;
#endif

	/* Create a datareader. */
	sm = DDS_SAMPLE_REJECTED_STATUS |
	     DDS_LIVELINESS_CHANGED_STATUS | 
	     DDS_REQUESTED_DEADLINE_MISSED_STATUS | 
	     DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS |
	     DDS_DATA_AVAILABLE_STATUS |
	     DDS_SUBSCRIPTION_MATCHED_STATUS |
	     DDS_SAMPLE_LOST_STATUS;

#ifdef WRDISPATCH
	thread_create (lr_thread, lr_consumer, 0);
#endif
	for (i = 0; i < nreaders; i++) {
		ldr [i] = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, &r_listener, sm);
		if (!ldr [i])
			fatal ("DDS_DomainParticipant_create_datareader () returned an error!");

		if (verbose)
			printf ("DDS Extra Reader created.\r\n");

		/* Test get/set_qos() functionality. */
		if ((error = DDS_DataReader_get_qos (ldr [i], &rd_qos)) != DDS_RETCODE_OK)
			fatal ("DDS_DataReader_get_qos () failed (%s)!", DDS_error (error));

		/* Test set_qos() functionality. */
		if ((error = DDS_DataReader_set_qos (ldr [i], &rd_qos)) != DDS_RETCODE_OK)
			fatal ("DDS_DataReader_set_qos () failed (%s)!", DDS_error (error));
	}
#endif
	cur_size = data_size;
	nsamples = 0;
	i = 0;
	
	if (tests)
		DDS_wait (start_delay);
	do {
		do_write ();
		DDS_wait (sleep_time);
		i++;
#ifdef DYN_CFG
		if (i == 5) {
			if (verbose)
				printf ("Set TCP_SEC_SERVER parameter to 'localhost:7300'!\r\n");
			DDS_parameter_set ("TCP_SEC_SERVER", "localhost:7300");
		}
#endif
#ifdef PART_UPD
		if (i == 5) {
			if (verbose)
				printf ("DDS Publisher QoS update to: %s, %s\r\n", s0, s1);
			dds_seq_reset (&pqos.partition.name);
			dds_seq_append (&pqos.partition.name, &s0);
			dds_seq_append (&pqos.partition.name, &s1);
			if ((error = DDS_Publisher_set_qos (pub, &pqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Publisher_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&pqos.partition.name);
		}
		else if (i == 15) {
			if (verbose)
				printf ("DDS Publisher QoS update to: %s, %s\r\n", s1, s0);
			dds_seq_reset (&pqos.partition.name);
			dds_seq_append (&pqos.partition.name, &s1);
			dds_seq_append (&pqos.partition.name, &s0);
			if ((error = DDS_Publisher_set_qos (pub, &pqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Publisher_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&pqos.partition.name);
		}
		else if (i == 25) {
			if (verbose)
				printf ("DDS Publisher QoS update to: empty\r\n");
			dds_seq_reset (&pqos.partition.name);
			if ((error = DDS_Publisher_set_qos (pub, &pqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Publisher_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&pqos.partition.name);
		}
#ifdef EXTRA_READER
		if (i == 10) {
			if (verbose)
				printf ("DDS Subscriber QoS update to: %s, %s\r\n", s1, s0);
			dds_seq_reset (&sqos.partition.name);
			dds_seq_append (&sqos.partition.name, &s1);
			dds_seq_append (&sqos.partition.name, &s0);
			if ((error = DDS_Subscriber_set_qos (sub, &sqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Subscriber_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&sqos.partition.name);
		}
		else if (i == 20) {
			if (verbose)
				printf ("DDS Subscriber QoS update to: %s, %s\r\n", s2, s3);
			dds_seq_reset (&sqos.partition.name);
			dds_seq_append (&sqos.partition.name, &s2);
			dds_seq_append (&sqos.partition.name, &s3);
			if ((error = DDS_Subscriber_set_qos (sub, &sqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Subscriber_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&sqos.partition.name);
		}
		else if (i == 30) {
			if (verbose)
				printf ("DDS Subscriber QoS update to: empty\r\n");
			dds_seq_reset (&sqos.partition.name);
			if ((error = DDS_Subscriber_set_qos (sub, &sqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Subscriber_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&sqos.partition.name);
		}
#endif
#endif
	}
	while (nsamples < max_samples && !aborting);
	if (nsamples >= max_samples)
		DDS_wait (2);

	if (tests)
		DDS_wait (start_delay);

#ifdef EXTRA_READER
	for (i = 0; i < nreaders; i++) {
		DDS_DataReader_set_listener (ldr [i], NULL, 0);
		error = DDS_Subscriber_delete_datareader (sub, ldr [i]);
		if (error)
			fatal ("DDS_Subscriber_delete_datareader() failed (%s)!", DDS_error (error));

		if (verbose)
			printf ("DDS Extra Reader deleted.\r\n");
	}
#endif
	DDS_DataWriter_set_listener (w, NULL, 0);
	error = DDS_Publisher_delete_datawriter (pub, w);
	if (error)
		fatal ("DDS_Publisher_delete_datawriter() failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Writer deleted.\r\n");
}

#ifdef DO_DISC_LISTEN
void start_disc_readers (DDS_DomainParticipant part);
#endif

static DDS_DataReaderListener dr_listener = {
	dr_rejected,	/* Sample rejected. */
	dr_liveliness,	/* Liveliness changed. */
	dr_deadline,	/* Requested Deadline missed. */
	dr_inc_qos,	/* Requested incompatible QoS. */
	NULL,		/* Data available. */
	dr_smatched,	/* Subscription matched. */
	dr_lost,	/* Sample lost. */
	NULL		/* Cookie */
};

void dcps_do_reader (DDS_DomainParticipant part)
{
	DDS_Subscriber		sub;
	DDS_SubscriberQos	sqos;
	DDS_DataReader		dr;
	DDS_DataReaderQos	rd_qos, drqos;
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	DDS_SampleInfo		*info;
	DDS_WaitSet		ws;
	DDS_ReadCondition	rc;
	DDS_ConditionSeq	conds = DDS_SEQ_INITIALIZER (DDS_Condition);
	DDS_Duration_t		to;
	DDS_StatusMask		sm;
	MsgData_t		*sample;
	unsigned		i, nchanges;
	char                    buffer [256];
#ifdef PART_UPD
	char			*s0 = "Hi", *s1 = "Folks", *s2 = "cya", *s3 = "bye";
#endif

	/* Create a subscriber */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

	/* Test get_qos() fynctionality. */
	if ((error = DDS_Subscriber_get_qos (sub, &sqos)) != DDS_RETCODE_OK)
		fatal ("DDS_Subscriber_get_qos () failed (%s)!", DDS_error (error));

	/* Setup reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
#ifdef TRANSIENT_LOCAL
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
#ifdef RELIABLE
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#endif
#ifdef KEEP_ALL
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.history.depth = DDS_LENGTH_UNLIMITED;
	rd_qos.resource_limits.max_samples_per_instance = HISTORY;
	rd_qos.resource_limits.max_instances = HISTORY * 10;
	rd_qos.resource_limits.max_samples = HISTORY * 4;
#else
	rd_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
	rd_qos.history.depth = HISTORY;
#endif
#ifdef LIVELINESS_MODE
	rd_qos.liveliness.kind = LIVELINESS_MODE;
	rd_qos.liveliness.lease_duration.sec = 12;
	rd_qos.liveliness.lease_duration.nanosec = 0;
#endif
#ifdef DEADLINE
	rd_qos.deadline.period.sec = 50;
	rd_qos.deadline.period.nanosec = 0;
#endif
#ifdef AUTOPURGE_NW
	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.sec = 70;
	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.nanosec = 0;
#endif
#ifdef AUTOPURGE_D
	rd_qos.reader_data_lifecycle.autopurge_disposed_samples_delay.sec = 75;
	rd_qos.reader_data_lifecycle.autopurge_disposed_samples_delay.nanosec = 0;
#endif
#ifdef NRTBFILTER
	rd_qos.time_based_filter.minimum_separation.sec = NRTBFILTER;
	rd_qos.time_based_filter.minimum_separation.nanosec = 0;
#endif

#ifdef TRACE_DATA
	DDS_Trace_defaults_set (DDS_TRACE_ALL);
#endif
	/* Create a datareader. */
	sm = DDS_SAMPLE_REJECTED_STATUS |
	     DDS_LIVELINESS_CHANGED_STATUS | 
	     DDS_REQUESTED_DEADLINE_MISSED_STATUS | 
	     DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS |
	     DDS_SUBSCRIPTION_MATCHED_STATUS |
	     DDS_SAMPLE_LOST_STATUS;
	dr = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, &dr_listener, sm);
	if (!dr)
		fatal ("DDS_DomainParticipant_create_datareader () returned an error!");

	if (verbose)
		printf ("DDS Reader created.\r\n");

	/* Test get_qos() functionality. */
	if ((error = DDS_DataReader_get_qos (dr, &drqos)) != DDS_RETCODE_OK)
		fatal ("DDS_DataReader_get_qos () failed (%s)!", DDS_error (error));

	/* Test set_qos() functionality. */
	if ((error = DDS_DataReader_set_qos (dr, &drqos)) != DDS_RETCODE_OK)
		fatal ("DDS_DataReader_set_qos () failed (%s)!", DDS_error (error));

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
	for (i = 0; i < max_samples && !aborting; i++) {

#ifdef PART_UPD
		if (i == 18) {
			if (verbose)
				printf ("DDS Subscriber QoS updated to: %s, %s\r\n", s2, s3);
			dds_seq_reset (&sqos.partition.name);
			dds_seq_append (&sqos.partition.name, &s2);
			dds_seq_append (&sqos.partition.name, &s3);
			if ((error = DDS_Subscriber_set_qos (sub, &sqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Subscriber_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&sqos.partition.name);
		}
		else if (i == 28) {
			if (verbose)
				printf ("DDS Subscriber QoS updated to: %s, %s\r\n", s1, s0);
			dds_seq_reset (&sqos.partition.name);
			dds_seq_append (&sqos.partition.name, &s1);
			dds_seq_append (&sqos.partition.name, &s0);
			if ((error = DDS_Subscriber_set_qos (sub, &sqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Subscriber_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&sqos.partition.name);
		}
		else if (i == 38) {
			if (verbose)
				printf ("DDS Subscriber QoS updated to: empty\r\n");
			dds_seq_reset (&sqos.partition.name);
			if ((error = DDS_Subscriber_set_qos (sub, &sqos)) != DDS_RETCODE_OK)
				fatal ("DDS_Subscriber_set_qos () failed (%s)!", DDS_error (error));

			dds_seq_cleanup (&sqos.partition.name);
		}
#endif
#if defined (DO_DISC_LISTEN) && defined (DDL_DELAY)
		if (i == DDL_DELAY) {
			printf ("Starting Discovery readers.\r\n");
			start_disc_readers (part);
		}
#endif
		to.sec = 1;
		to.nanosec = 0;
		if (paused) {
			usleep (1000000);
			continue;
		}
		if ((error = DDS_WaitSet_wait (ws, &conds, &to)) == DDS_RETCODE_TIMEOUT) {
			/*printf ("WaitSet - Time-out!\r\n");*/
			continue;
		}

#ifdef NORMAL_TAKE
		error = DDS_DataReader_take (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#else
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#endif
		/*printf ("WaitSet:read/take() result=%s!\r\n", DDS_error (error));*/
		if (error) {
			if (error == DDS_RETCODE_NO_DATA)
				continue;

			printf ("Unable to read samples: error = %u!\r\n", error);
			break;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			nchanges = 1;
#ifdef SAMPLE_INFO
			printf ("SampleInfo: ss:%u,vs:%u,is:%u,dg:%u,nwg:%u,sr:%u,gr:%u,agr:%u\r\n", 
					info->sample_state,
					info->view_state,
					info->instance_state,
					info->disposed_generation_count,
					info->no_writers_generation_count,
					info->sample_rank,
					info->generation_rank,
					info->absolute_generation_rank);
#endif
			if (verbose) {
				if (info->valid_data) {
					snprintf (buffer, 255, "DDS-R: [%2u] ALIVE - %2u :%6llu - '%s'\r\n", 
						  info->instance_handle,
#ifndef NO_KEY
#ifdef LARGE_KEY
						  sample->key [0],
#else
						  sample->key,
#endif
#else
						  0,
#endif
						  (unsigned long long) sample->counter,
						  sample->message);
					if (file)
						write_to_file (buffer);
#ifdef LOG_DATA
					log_printf (USER_ID, 0, "%s", buffer);
#else
					fprintf (stdout, "%s", buffer);
#endif
				}
				else if (info->instance_state != DDS_ALIVE_INSTANCE_STATE) {
					snprintf (buffer, 255, "DDS-R: [%2u] %s\r\n",
							info->instance_handle,
							kind_str [info->instance_state]);
#ifdef LOG_DATA
					log_printf (USER_ID, 0, "%s", buffer);
#else
					fprintf (stdout, "%s", buffer);
#endif
				}
			}
			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
#ifdef SLOW_RREAD
			DDS_wait (sleep_time);
#endif
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
			i += nchanges;
		}
		else
			printf ("WaitSet:read/take() - no info!\r\n");
		dds_seq_reset (&conds);
	}
	if (tests) 
		DDS_wait (start_delay);

	error = DDS_DataReader_delete_contained_entities (dr);
	if (error)
		fatal ("DDS_DataReader_delete_contained_entities() failed (%s)!", DDS_error (error));

	DDS_DataReader_set_listener (dr, NULL, 0);
	error = DDS_Subscriber_delete_datareader (sub, dr);
	if (error)
		fatal ("DDS_Subscriber_delete_datareader() failed (%s)!", DDS_error (error));

	DDS_WaitSet__free (ws);
	dds_seq_cleanup (&rx_sample);
	dds_seq_cleanup (&rx_info);
	dds_seq_cleanup (&conds);
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
	printf ("%08x:%08x:%08x", ntohl (kp->value [0]), 
			ntohl (kp->value [1]), ntohl (kp->value [2]));
}

void dump_user_data (DDS_OctetSeq *sp)
{
	unsigned	i;
	unsigned char	*p;

	if (!DDS_SEQ_LENGTH (*sp))
		printf ("<none>\r\n");
	else if (DDS_SEQ_LENGTH (*sp) < 10) {
		DDS_SEQ_FOREACH_ENTRY (*sp, i, p)
			printf ("%02x ", *p);
		printf ("\r\n");
	}
	else {
		printf ("\r\n");
		dbg_print_region (DDS_SEQ_ITEM_PTR (*sp, 0), DDS_SEQ_LENGTH (*sp), 0, 0);
	}
}

void display_participant_info (DDS_ParticipantBuiltinTopicData *sample)
{
	printf ("\tKey                = ");
	dump_key (&sample->key);
	printf ("\r\n\tUser data          = ");
	dump_user_data (&sample->user_data.value);
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
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read Discovered Participant samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("* ");
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
				if (info->valid_data)
					display_participant_info (sample);
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

void dump_duration (DDS_Duration_t *dp)
{
	if (dp->sec == DDS_DURATION_ZERO_SEC &&
	    dp->nanosec == DDS_DURATION_ZERO_NSEC)
		printf ("0s");
	else if (dp->sec == DDS_DURATION_INFINITE_SEC &&
	         dp->nanosec == DDS_DURATION_INFINITE_NSEC)
		printf ("<infinite>");
	else
		printf ("%d.%09us", dp->sec, dp->nanosec);
}

void dump_durability (DDS_DurabilityQosPolicy *dp)
{
	static const char *durability_str [] = {
		"Volatile", "Transient-local", "Transient", "Persistent"
	};

	if (dp->kind <= DDS_PERSISTENT_DURABILITY_QOS)
		printf ("%s", durability_str [dp->kind]);
	else
		printf ("?(%d)", dp->kind);
}

void dump_history (DDS_HistoryQosPolicyKind k, int depth)
{
	if (k == DDS_KEEP_ALL_HISTORY_QOS)
		printf ("All");
	else
		printf ("Last %d", depth);
}

void dump_resource_limits (int max_samples, int max_inst, int max_samples_per_inst)
{
	printf ("max_samples/instances/samples_per_inst=%d/%d/%d",
			max_samples, max_inst, max_samples_per_inst);
}

void dump_durability_service (DDS_DurabilityServiceQosPolicy *sp)
{
	printf ("\r\n\t     Cleanup Delay = ");
	dump_duration (&sp->service_cleanup_delay);
	printf ("\r\n\t     History       = ");
	dump_history (sp->history_kind, sp->history_depth);
	printf ("\r\n\t     Limits        = ");
	dump_resource_limits (sp->max_samples,
			      sp->max_instances,
			      sp->max_samples_per_instance);
}

void dump_liveliness (DDS_LivelinessQosPolicy *lp)
{
	static const char *liveness_str [] = {
		"Automatic", "Manual_by_Participant", "Manual_by_Topic"
	};

	if (lp->kind <= DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS)
		printf ("%s", liveness_str [lp->kind]);
	else
		printf ("?(%d)", lp->kind);
	printf (", Lease duration: ");
	dump_duration (&lp->lease_duration);
}

void dump_reliability (DDS_ReliabilityQosPolicy *rp)
{
	if (rp->kind == DDS_BEST_EFFORT_RELIABILITY_QOS)
		printf ("Best-effort");
	else if (rp->kind == DDS_RELIABLE_RELIABILITY_QOS)
		printf ("Reliable");
	else
		printf ("?(%d)", rp->kind);
	printf (", Max_blocking_time: ");
	dump_duration (&rp->max_blocking_time);
}

void dump_destination_order (DDS_DestinationOrderQosPolicyKind k)
{
	if (k == DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS)
		printf ("Reception_Timestamp");
	else if (k == DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS)
		printf ("Source_Timestamp");
	else
		printf ("?(%d)", k);
}

void dump_ownership (DDS_OwnershipQosPolicyKind k)
{
	if (k == DDS_SHARED_OWNERSHIP_QOS)
		printf ("Shared");
	else if (k == DDS_EXCLUSIVE_OWNERSHIP_QOS)
		printf ("Exclusive");
	else
		printf ("?(%d)", k);
}

void display_topic_info (DDS_TopicBuiltinTopicData *sample)
{
	printf ("\tKey                = ");
	dump_key (&sample->key);
	printf ("\r\n\tName               = %s", sample->name);
	printf ("\r\n\tType Name          = %s", sample->type_name);
	printf ("\r\n\tDurability         = ");
	dump_durability (&sample->durability);
	printf ("\r\n\tDurability Service:");
	dump_durability_service (&sample->durability_service);
	printf ("\r\n\tDeadline           = ");
	dump_duration (&sample->deadline.period);
	printf ("\r\n\tLatency Budget     = ");
	dump_duration (&sample->latency_budget.duration);
	printf ("\r\n\tLiveliness         = ");
	dump_liveliness (&sample->liveliness);
	printf ("\r\n\tReliability        = ");
	dump_reliability (&sample->reliability);
	printf ("\r\n\tTransport Priority = %d", sample->transport_priority.value);
	printf ("\r\n\tLifespan           = ");
	dump_duration (&sample->lifespan.duration);
	printf ("\r\n\tDestination Order  = ");
	dump_destination_order (sample->destination_order.kind);
	printf ("\r\n\tHistory            = ");
	dump_history (sample->history.kind, sample->history.depth);
	printf ("\r\n\tResource Limits    = ");
	dump_resource_limits (sample->resource_limits.max_samples,
			      sample->resource_limits.max_instances,
			      sample->resource_limits.max_samples_per_instance);
	printf ("\r\n\tOwnership          = ");
	dump_ownership (sample->ownership.kind);
	printf ("\r\n\tTopic Data         = ");
	dump_user_data (&sample->topic_data.value);
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
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read Discovered Topic samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("* ");
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
				if (info->valid_data)
					display_topic_info (sample);
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

void dump_presentation (DDS_PresentationQosPolicy *pp)
{
	static const char *pres_str [] = {
		"Instance", "Topic", "Group"
	};

	printf ("Scope: ");
	if (pp->access_scope <= DDS_GROUP_PRESENTATION_QOS)
		printf ("%s", pres_str [pp->access_scope]);
	else
		printf ("?(%d)", pp->access_scope);
	printf (", coherent: %d, ordered: %d", pp->coherent_access, pp->ordered_access);
}

void dump_partition (DDS_PartitionQosPolicy *pp)
{
	unsigned	i;
	char		**cp;

	if (!DDS_SEQ_LENGTH (pp->name)) {
		printf ("<none>");
		return;
	}
	DDS_SEQ_FOREACH_ENTRY (pp->name, i, cp) {
		if (i)
			printf (", ");
		printf ("%s", *cp);
	}
}

void display_publication_info (DDS_PublicationBuiltinTopicData *sample)
{
	printf ("\tKey                = ");
	dump_key (&sample->key);
	printf ("\r\n\tParticipant Key    = ");
	dump_key (&sample->participant_key);
	printf ("\r\n\tTopic Name         = %s", sample->topic_name);
	printf ("\r\n\tType Name          = %s", sample->type_name);
	printf ("\r\n\tDurability         = ");
	dump_durability (&sample->durability);
	printf ("\r\n\tDurability Service:");
	dump_durability_service (&sample->durability_service);
	printf ("\r\n\tDeadline           = ");
	dump_duration (&sample->deadline.period);
	printf ("\r\n\tLatency Budget     = ");
	dump_duration (&sample->latency_budget.duration);
	printf ("\r\n\tLiveliness         = ");
	dump_liveliness (&sample->liveliness);
	printf ("\r\n\tReliability        = ");
	dump_reliability (&sample->reliability);
	printf ("\r\n\tLifespan           = ");
	dump_duration (&sample->lifespan.duration);
	printf ("\r\n\tUser Data          = ");
	dump_user_data (&sample->user_data.value);
	printf ("\tOwnership          = ");
	dump_ownership (sample->ownership.kind);
	printf ("\r\n\tOwnership strength = %d",
			sample->ownership_strength.value);
	printf ("\r\n\tDestination Order  = ");
	dump_destination_order (sample->destination_order.kind);
	printf ("\r\n\tPresentation       = ");
	dump_presentation (&sample->presentation);
	printf ("\r\n\tPartition          = ");
	dump_partition (&sample->partition);
	printf ("\r\n\tTopic Data         = ");
	dump_user_data (&sample->topic_data.value);
	printf ("\tGroup Data         = ");
	dump_user_data (&sample->group_data.value);
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
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read Discovered Publication samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("* ");
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
				if (info->valid_data)
					display_publication_info (sample);
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

void display_subscription_info (DDS_SubscriptionBuiltinTopicData *sample)
{
	printf ("\tKey                = ");
	dump_key (&sample->key);
	printf ("\r\n\tParticipant Key    = ");
	dump_key (&sample->participant_key);
	printf ("\r\n\tTopic Name         = %s", sample->topic_name);
	printf ("\r\n\tType Name          = %s", sample->type_name);
	printf ("\r\n\tDurability         = ");
	dump_durability (&sample->durability);
	printf ("\r\n\tDeadline           = ");
	dump_duration (&sample->deadline.period);
	printf ("\r\n\tLatency Budget     = ");
	dump_duration (&sample->latency_budget.duration);
	printf ("\r\n\tLiveliness         = ");
	dump_liveliness (&sample->liveliness);
	printf ("\r\n\tReliability        = ");
	dump_reliability (&sample->reliability);
	printf ("\r\n\tOwnership          = ");
	dump_ownership (sample->ownership.kind);
	printf ("\r\n\tDestination Order  = ");
	dump_destination_order (sample->destination_order.kind);
	printf ("\r\n\tUser Data          = ");
	dump_user_data (&sample->user_data.value);
	printf ("\tTime based filter  = ");
	dump_duration (&sample->time_based_filter.minimum_separation);
	printf ("\r\n\tPresentation       = ");
	dump_presentation (&sample->presentation);
	printf ("\r\n\tPartition          = ");
	dump_partition (&sample->partition);
	printf ("\r\n\tTopic Data         = ");
	dump_user_data (&sample->topic_data.value);
	printf ("\tGroup Data         = ");
	dump_user_data (&sample->group_data.value);
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
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read Discovered Subscription samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				printf ("* ");
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
				if (info->valid_data)
					display_subscription_info (sample);
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
		fatal ("DDS_DomainParticipant_get_builtin_subscriber() returned an error!");

	if (verbose)
		printf ("DDS Builtin Subscriber found.\r\n");

	for (i = 0; i < sizeof (names) / sizeof (char *); i++) {
		dr = DDS_Subscriber_lookup_datareader (sub, names [i]);
		if (!dr)
			fatal ("DDS_Subscriber_lookup_datareader returned an error!");

		ret = DDS_DataReader_set_listener (dr, &builtin_listeners [i], DDS_DATA_AVAILABLE_STATUS);
		if (ret)
			fatal ("DDS_DataReader_set_listener returned an error (%s)!", DDS_error (ret));

		if (verbose)
			printf ("DDS Discovery Reader created (%s).\r\n", names [i]);

		switch (i) {
			case 0:
				participant_info (NULL, dr);
				break;
			case 1:
				topic_info (NULL, dr);
				break;
			case 2:
				publication_info (NULL, dr);
				break;
			case 3:
				subscription_info (NULL, dr);
				break;
			default:
				break;
		}
	}
}

#endif

void t_inconsistent (DDS_TopicListener           *self,
		     DDS_Topic                   topic,
		     DDS_InconsistentTopicStatus *status)
{
	ARG_NOT_USED (self)
	ARG_NOT_USED (topic)

	printf ("DDS-T: Inconsistent Topic - T=%d, TC=%d\r\n",
			status->total_count, status->total_count_change);
}

DDS_TopicListener t_listener = {
	t_inconsistent
};


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
	if (sfile) {
		if (DDS_SP_parse_xml (sname))
			fatal ("SP: no DDS security rules in '%s'!\r\n", sname);
	}
	else 
		if (DDS_SP_parse_xml ("security.xml"))
			fatal ("SP: no DDS security rules in 'security.xml'!\r\n");

	/* some tests */
	/*	dhandle = DDS_SP_get_domain_handle (~0);
	fail_unless (dhandle != -1);
	thandle = DDS_SP_get_topic_handle (0, dhandle, "*");
	fail_unless (thandle != -1);
	thandle = DDS_SP_get_partition_handle (0, dhandle, "*");
	fail_unless (thandle != -1);

	dhandle = DDS_SP_get_participant_handle ("DCPS Test program");
	fail_unless (dhandle != -1);
	thandle = DDS_SP_get_topic_handle (dhandle, 0, "HelloWorld");
	fail_unless (thandle != -1);*/
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

	error = DDS_Security_set_credentials ("DCPS Test program", &credentials);
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

#ifdef DDS_NATIVE_SECURITY
	DDS_Security_cleanup_credentials ();
#endif
}

#endif

int main (int argc, const char *argv [])
{
	DDS_DomainParticipant		part, part2;
	DDS_PoolConstraints		reqs;
	DDS_StatusMask			sm;
	DDS_DomainParticipantFactoryQos	dfqos;
	DDS_DomainParticipantQos	dpqos;
	DDS_TopicQos			tqos;
#ifdef TRANS_PARS
	RTPS_UDPV4_PARS			udp_pars = {
		3010, 200, 5, 0, 10, 1, 11
	};
#endif
	int				error;

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
	DDS_entity_name ("Technicolor DCPS Tester");

#ifdef DDS_DEBUG
	DDS_Debug_start ();
	DDS_Debug_abort_enable (&aborting);
	DDS_Debug_control_enable (&paused, &max_sends, &sleep_time);
	if (dbg_server)
		DDS_Debug_server_start (2, dbg_port);
#endif
	for (; nruns; nruns--) {
		aborting = 0;

#ifdef DDS_SECURITY
		if (cert_path || key_path || engine_id)
			enable_security ();
#endif
#ifdef RTPS_USED
#ifdef TRACE_DISC
		DDS_Trace_defaults_set (DDS_TRACE_ALL);
#endif
#endif
		DDS_RTPS_control (!no_rtps);

		/* Test get_qos() fynctionality. */
		if ((error = DDS_DomainParticipantFactory_get_qos (&dfqos)) != DDS_RETCODE_OK)
			fatal ("DDS_DomainParticipantFactory_get_qos () failed (%s)!", DDS_error (error));

#ifdef TRANS_PARS

		/* Change some transport parameters. */
		DDS_Transport_parameters (LOCATOR_KIND_UDPv4, &udp_pars);
#endif
		if (extra_dp) {	/* Create an open domain in addition to the secure domain. */
			part2 = DDS_DomainParticipantFactory_create_participant (
						(domain_id) ? 0 : 1, NULL, NULL, 0);
			if (!part2)
				fatal ("Secondary DDS_DomainParticipantFactory_create_participant () failed!");

			if (verbose)
				printf ("Secondary DDS Domain Participant created.\r\n");
		}

		/* Create a domain participant. */
		part = DDS_DomainParticipantFactory_create_participant (
							domain_id, NULL, NULL, 0);
		if (!part)
			fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

#ifdef TRACE_DISC
		DDS_Trace_defaults_set (DDS_TRACE_NONE);
#endif
		if (verbose)
			printf ("DDS Domain Participant created.\r\n");

		/* Test get_qos() fynctionality. */
		if ((error = DDS_DomainParticipant_get_qos (part, &dpqos)) != DDS_RETCODE_OK)
			fatal ("DDS_DomainParticipant_get_qos () failed (%s)!", DDS_error (error));

#if defined (DO_DISC_LISTEN) && !defined (DDL_DELAY)
		start_disc_readers (part);
#endif

		/* Register the message topic type. */
		error = register_HelloWorldData_type (part);
		if (error) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("DDS_DomainParticipant_register_type ('HelloWorldData') failed (%s)!", DDS_error (error));
		}
		if (verbose)
			printf ("DDS Topic type ('%s') registered.\r\n", "HelloWorldData");

		/* Create a topic */
		sm = DDS_INCONSISTENT_TOPIC_STATUS;
		topic = DDS_DomainParticipant_create_topic (part, "HelloWorld", "HelloWorldData",
								NULL, &t_listener, sm);
		if (!topic) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("DDS_DomainParticipant_create_topic ('HelloWorld') failed!");
		}
		if (verbose)
			printf ("DDS Topic created.\r\n");

		/* Test get_qos() fynctionality. */
		if ((error = DDS_Topic_get_qos (topic, &tqos)) != DDS_RETCODE_OK)
			fatal ("DDS_Topic_get_qos () failed (%s)!", DDS_error (error));

		/* Check Topic QoS functionality working. */

		/* Create Topic Description. */
		topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "HelloWorld");
		if (!topic_desc) {
			DDS_DomainParticipantFactory_delete_participant (part);
			fatal ("Unable to create topic description for 'HelloWorld'!");
		}

		/* Start either a reader or a writer depending on program options. */
		if (writer) 
			dcps_do_writer (part);
		else
			dcps_do_reader (part);

		if (tests) {
			DDS_Debug_command ("sdisca");
			DDS_wait (start_delay);
		}

		/* Delete the topic. */
		error = DDS_DomainParticipant_delete_topic (part, topic);
		if (error)
			fatal ("DDS_DomainParticipant_delete_topic () failed (%s)!", DDS_error (error));

		if (verbose)
			printf ("DDS Topic deleted.\r\n");

		free_HelloWorldData_type (part);

		if (extra_dp) {
			error = DDS_DomainParticipantFactory_delete_participant (part2);
			if (error)
				fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

			if (verbose)
				printf ("Secondary DDS Participant deleted\r\n");
		}

		error = DDS_DomainParticipant_delete_contained_entities (part);
		if (error)
			fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

		if (verbose)
			printf ("DDS Entities deleted\r\n");

# if 0
		aborting = 0;
		do {
			DDS_wait (sleep_time);
		}
		while (!aborting);
# endif
		
		error = DDS_DomainParticipantFactory_delete_participant (part);
		if (error)
			fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)!", DDS_error (error));

		if (verbose)
			printf ("DDS Participant deleted\r\n");

#ifdef DDS_SECURITY
		if (cert_path || key_path || engine_id)
			cleanup_security ();
#endif
		if (nruns > 1) {
			DDS_wait (1000);
			printf ("\r\n --- Dumping some data ---\r\n\r\n");
			DDS_Debug_command ("spool");
			DDS_Debug_command ("sstr");
			DDS_Debug_command ("sloc");
			DDS_Debug_command ("sfd");
			DDS_Debug_command ("scxa");
			DDS_wait (1000);
			printf ("\r\n --- Restarting ---\r\n\r\n");
		}
	}
	return (0);
}

