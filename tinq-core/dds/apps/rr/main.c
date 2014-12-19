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

/* main.c -- Test program to test Request/Response performance. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "thread.h"
#include "libx.h"
#include "list.h"
#include "dds/dds_dcps.h"
#include "dds/dds_debug.h"
#include "dds/dds_aux.h"

#define	DOMAIN_ID	0
#define MAX_DSIZE	0x8000		/* 32KB */

const char		*progname;
int			controller;		/* Controller. */
int			generator;		/* Generator (Tx+Rx data). */
int			loop;			/* Loop (loops Rxed data). */
unsigned		max_time = 5;		/* Test time (seconds or ~0). */
unsigned		nrequests = 4;		/* # of parallel requests. */
unsigned		data_size = 16;		/* Request data size (bytes). */
unsigned		delay = 0;		/* Delay (us) between requests*/
unsigned		history;		/* History to keep. */
int			reliable;		/* Reliable operation if set. */
int			dump;			/* Dump samples if set. */
int			verbose;		/* Verbose if set. */
int			quit;
DDS_DomainParticipant	part;
DDS_Topic		ctrl_topic;
DDS_Topic		req_topic;
DDS_Topic		resp_topic;
DDS_TopicDescription	ctrl_topic_desc;
DDS_TopicDescription	req_topic_desc;
DDS_TopicDescription	resp_topic_desc;
DDS_Subscriber		sub;
DDS_Publisher		pub;

typedef enum {
	RR_INIT,
	RR_WAIT_REPLY,
	RR_COMPLETED
} RRState_t;

typedef struct rr_context_st RR_t;
struct rr_context_st {
	RR_t			*next;
	RR_t			*prev;
	RRState_t		state;		/* Current state. */
	unsigned		inst;		/* Instance id. */
	DDS_InstanceHandle_t	h;		/* DDS instance handle. */
	unsigned		n;		/* # of successful replies. */
	double			rtt;		/* Last roundtrip time. */
};

typedef struct rr_list_st {
	RR_t	*head;
	RR_t	*tail;
}
RRList_t;

typedef enum {
	TEST_START = 1,		/* Start test (C->G). */
	TEST_DONE,		/* Test completed (G->C). */
	TEST_QUIT		/* Quit test (C->G+L). */
} Cmd_t;

typedef struct rr_cmd_st {
	uint32_t	cmd;
	uint32_t	dsize;
	uint32_t	time;
	uint32_t	delay;
} RRCmd_t;

static DDS_TypeSupport_meta rr_cmd_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 0, "Cmd", sizeof (struct rr_cmd_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "cmd",   0, offsetof (struct rr_cmd_st, cmd), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "dsize", 0, offsetof (struct rr_cmd_st, dsize), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "time", 0, offsetof (struct rr_cmd_st, time), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  0, "delay", 0, offsetof (struct rr_cmd_st, delay), 0, 0, NULL },
};

typedef struct rr_req_st {
	uint32_t	command;
	uint32_t	req_id;
	uint32_t	inst_id;
	char		req_data [MAX_DSIZE];
} RRReq_t;

static DDS_TypeSupport_meta rr_req_tsm [] = {
	{ CDR_TYPECODE_STRUCT,  1, "Req", sizeof (struct rr_req_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "command", 0, offsetof (struct rr_req_st, command), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "req_id",  0, offsetof (struct rr_req_st, req_id), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,   1, "inst_id", 0, offsetof (struct rr_req_st, inst_id), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "req_data", MAX_DSIZE, offsetof (struct rr_req_st, req_data), 0, 0, NULL }
};

typedef struct rr_resp_st {
	uint32_t	req_id;
	uint32_t	inst_id;
	uint32_t	resp_id;
	char		resp_data [MAX_DSIZE];
} RRResp_t;

static DDS_TypeSupport_meta rr_resp_tsm [] = {
	{ CDR_TYPECODE_STRUCT,  1, "Resp", sizeof (struct rr_resp_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "req_id", 0, offsetof (struct rr_resp_st, req_id), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,   1, "inst_id", 0, offsetof (struct rr_resp_st, inst_id), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "resp_id", 0, offsetof (struct rr_resp_st, resp_id), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0, "resp_data", MAX_DSIZE, offsetof (struct rr_resp_st, resp_data), 0, 0, NULL }
};

static DDS_TypeSupport	RRCmd_ts, RRReq_ts, RRResp_ts;

typedef struct cmd_context_st {
	RRCmd_t		last_cmd;
	char		rxch;
} CmdContext_t;


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

#define	ctrace_printd(i,d,l)	DDS_CTrace_printd(i,d,l)
#else
#define	ctrace_printd(i,d,l)
#endif

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "rr -- Request/response test program for the DDS protocol.\r\n");
	fprintf (stderr, "Usage: rr[switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -c		Controller (controls the others).\r\n");
	fprintf (stderr, "   -g		Generator (sends requests/receives responses).\r\n");
	fprintf (stderr, "   -l		Looper (responds to requests - default).\r\n");
	fprintf (stderr, "   -n <reqs>	# of Requests to launch (default=16).\r\n");
	fprintf (stderr, "   -t <time>	Test time in seconds (default=5s).\r\n");
	fprintf (stderr, "   -s <size>	Data size in bytes (default=16).\r\n");
	fprintf (stderr, "   -d <usec>	Delay between bursts (default=0us).\r\n");
	fprintf (stderr, "   -r		Reliable operation (default=Best-Effort).\r\n");
	fprintf (stderr, "   -k <hist>	# of samples to keep in history (default=ALL).\r\n");
	fprintf (stderr, "   -i		Dump all info.\r\n");
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
				case 'c':
					controller = 1;
					break;
				case 'g':
					generator = 1;
					break;
				case 'l':
					loop = 1;
					break;
				case 't':
					INC_ARG()
					if (!get_num (&cp, &max_time, 1, ~0))
						usage ();
					break;
				case 'n':
					INC_ARG()
					if (!get_num (&cp, &nrequests, 1, 10000))
						usage ();
					break;
				case 's':
					INC_ARG()
					if (!get_num (&cp, &data_size, 1, MAX_DSIZE))
						usage ();
					break;
				case 'd':
					INC_ARG()
					if (!get_num (&cp, &delay, 0, 3600000))
						usage ();
					break;
				case 'r':
					reliable = 1;
					break;
				case 'k':
					INC_ARG()
					if (!get_num (&cp, &history, 1, ~0))
						usage ();
					break;
				case 'i':
					dump = 1;
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
	if (!controller && !generator && !loop)
		loop = 1;

	return (i);
}

DDS_ReturnCode_t register_rr_types (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	RRCmd_ts = DDS_DynamicType_register (rr_cmd_tsm);
        if (!RRCmd_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, RRCmd_ts, "RRCmd");
	if (error)
		return (error);

	RRReq_ts = DDS_DynamicType_register (rr_req_tsm);
        if (!RRReq_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, RRReq_ts, "RRReq");
	if (error)
		return (error);

	RRResp_ts = DDS_DynamicType_register (rr_resp_tsm);
        if (!RRResp_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, RRResp_ts, "RRResp");
	return (error);
}

void free_rr_types (void)
{
	if (RRCmd_ts) {
		DDS_DynamicType_free (RRCmd_ts);
		RRCmd_ts = NULL;
	}
	if (RRReq_ts) {
		DDS_DynamicType_free (RRReq_ts);
		RRReq_ts = NULL;
	}
	if (RRResp_ts) {
		DDS_DynamicType_free (RRResp_ts);
		RRResp_ts = NULL;
	}
}

static unsigned num_cmd_readers;

void rr_on_publication_matched (DDS_DataWriterListener *l,
			        DDS_DataWriter w,
			        DDS_PublicationMatchedStatus *st)
{
	ARG_NOT_USED (l)
	ARG_NOT_USED (w)

	if (verbose)
		printf ("C: %d of %d readers match\r\n",
				st->current_count, st->total_count);
	num_cmd_readers = st->current_count;
}

DDS_DataWriterListener c_cw_listener = {
	NULL,
	rr_on_publication_matched,
	NULL,
	NULL,
	NULL
};

void rr_on_cmd_avail (DDS_DataReaderListener *l, const DDS_DataReader cr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	RRCmd_t			cmd;
	CmdContext_t		*ccp = (CmdContext_t *) l->cookie;
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
			memcpy (&cmd, DDS_SEQ_ITEM (rx_sample, i), sizeof (RRCmd_t));
			if (verbose) {
				printf ("%c: Command received: ", ccp->rxch);
				if (cmd.cmd == TEST_START)
					printf ("START");
				else if (cmd.cmd == TEST_DONE)
					printf ("DONE");
				else if (cmd.cmd == TEST_QUIT)
					printf ("QUIT");
				else
					printf ("?(%u)", cmd.cmd);
				printf ("\r\n");
			}
			if (cmd.cmd == TEST_QUIT)
				quit = 1;
			ccp->last_cmd = cmd;
		}
	}
	DDS_DataReader_return_loan (cr, &rx_sample, &rx_info);
}

void send_cmd (DDS_DataWriter cw, Cmd_t type, unsigned dsize, unsigned time, unsigned delay)
{
	RRCmd_t	c;

	c.cmd = type;
	c.dsize = dsize;
	c.time = time;
	c.delay = delay;
	DDS_DataWriter_write (cw, &c, DDS_HANDLE_NIL);
}

void rr_do_controller (void)
{
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriter	 	cw;
	DDS_DataReader	 	cr;
	DDS_StatusMask		sm;
	static CmdContext_t	c_cmd;
	static DDS_DataReaderListener c_cr_listener = {
		NULL,
		NULL,
		NULL,
		NULL,
		rr_on_cmd_avail,
		NULL,
		NULL,
		(void *) &c_cmd
	};

	/* Setup Command Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Writer. */
	sm = DDS_PUBLICATION_MATCHED_STATUS | DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
	cw = DDS_Publisher_create_datawriter (pub, ctrl_topic, &wr_qos, &c_cw_listener, sm);
	if (!cw)
		fatal ("Unable to create command writer!\r\n");

	if (verbose)
		printf ("C: DDS Command Writer created.\r\n");

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	memset (&c_cmd.last_cmd, 0, sizeof (c_cmd.last_cmd));
	c_cmd.rxch = 'C';
	cr = DDS_Subscriber_create_datareader (sub, ctrl_topic_desc, &rd_qos, 
					&c_cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!\r\n");

	if (verbose)
		printf ("C: DDS Command Reader created.\r\n");

	/* Wait for all entities to become active. */
	while (num_cmd_readers < 3 && !quit)
		usleep (100000);

	if (quit)
		return;

	if (verbose)
		printf ("C: Send Start command.\r\n");

	ctrace_printd (USER_C_START, NULL, 0);
	send_cmd (cw, TEST_START, data_size, max_time, delay);

	while (c_cmd.last_cmd.cmd != TEST_DONE && !quit) {
		if (verbose == 2) {
			printf (".");
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

static RRCmd_t rr_l_cmd;

DDS_DataReaderListener rr_cr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	rr_on_cmd_avail,
	NULL,
	NULL,
	(void *) &rr_l_cmd
};

DDS_DataWriter	rr_dw;

void rr_on_l_data_avail (DDS_DataReaderListener *l, const DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	unsigned		i;
	RRReq_t			*sample;
	static RRResp_t		reply;

	ARG_NOT_USED (l)

	if (verbose == 2) {
		printf ("L");
		fflush (stdout);
	}
	error = DDS_DataReader_take (dr, &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
	if (error) {
		if (error != DDS_RETCODE_NO_DATA)
			printf ("Unable to read samples: error = %u!\r\n", error);
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++)
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			if (verbose == 2) {
				printf ("L");
				fflush (stdout);
			}
			sample = (RRReq_t *) DDS_SEQ_ITEM (rx_sample, i);
			reply.req_id = sample->req_id;
			reply.inst_id = sample->inst_id;
			reply.resp_id = (uint32_t) (uintptr_t) dr;
			memcpy (reply.resp_data, sample->req_data, data_size);
			DDS_DataWriter_write_w_timestamp (rr_dw, &reply, DDS_HANDLE_NIL,
					&DDS_SEQ_ITEM (rx_info, i)->source_timestamp);
			ctrace_printd (USER_LOOPED, NULL, 0);
		}

	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
}

DDS_DataReaderListener rr_l_dr_listener = {
	NULL,
	NULL,
	NULL,
	NULL,
	rr_on_l_data_avail,
	NULL,
	NULL,
	NULL
};

void *rr_do_loop (void *args)
{
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriterQos	wr_qos;
	DDS_DataReader	 	cr;
	DDS_DataReader		dr;
	static CmdContext_t	l_cmd;
	static DDS_DataReaderListener l_cr_listener = {
		NULL,
		NULL,
		NULL,
		NULL,
		rr_on_cmd_avail,
		NULL,
		NULL,
		(void *) &l_cmd
	};

	ARG_NOT_USED (args)

	/* Create Data Reader. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	if (reliable)
		rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dr = DDS_Subscriber_create_datareader (sub, req_topic_desc, &rd_qos, 
					&rr_l_dr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!dr)
		fatal ("Unable to create data reader!\r\n");

	if (verbose)
		printf ("L: DDS Data Reader created.\r\n");

	/* Create Data Writer. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	if (reliable)
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	rr_dw = DDS_Publisher_create_datawriter (pub, resp_topic, &wr_qos, NULL, 0);
	if (!rr_dw)
		fatal ("Unable to create data writer!\r\n");

	if (verbose)
		printf ("L: DDS Data Writer created.\r\n");

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	memset (&l_cmd.last_cmd, 0, sizeof (l_cmd.last_cmd));
	l_cmd.rxch = 'L';
	cr = DDS_Subscriber_create_datareader (sub, ctrl_topic_desc, &rd_qos, 
					&l_cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!\r\n");

	if (verbose)
		printf ("L: DDS Command Reader created.\r\n");

	while (!quit)
		sleep (1);

	DDS_Subscriber_delete_datareader (sub, cr);
	DDS_Subscriber_delete_datareader (sub, dr);
	DDS_Publisher_delete_datawriter (pub, rr_dw);

	return (NULL);
}

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

void *rr_do_generator (void *args)
{
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriter	 	cw;
	DDS_DataReader	 	cr;
	DDS_DataWriter	 	dw;
	DDS_DataReader	 	dr;
	DDS_DataSeq		rx_sample = DDS_SEQ_INITIALIZER (void *);
	DDS_SampleInfoSeq 	rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;
	DDS_SampleInfo		*info;
	DDS_WaitSet		ws;
	DDS_ReadCondition	rc;
	DDS_ConditionSeq	conds = DDS_SEQ_INITIALIZER (DDS_Condition);
	DDS_Duration_t		to;
	unsigned		i;
	DDS_Time_t		t, nt;
	double			total, min, max, avg;
	double			stime, rtime;
	unsigned		total_reqs, total_resps, nsamples;
	static RRReq_t		req;
	RRResp_t		*sample;
	RR_t			*context_data, *cp;
	RR_t			*(*context_ptrs) [];
	RRList_t		idle_list;		/* List of idle contexts. */
	RRList_t		active_list;		/* List of active contexts. */
	static volatile CmdContext_t g_cmd;
	static DDS_DataReaderListener g_cr_listener = {
		NULL,
		NULL,
		NULL,
		NULL,
		rr_on_cmd_avail,
		NULL,
		NULL,
		(void *) &g_cmd
	};

	ARG_NOT_USED (args)

	context_data = malloc (sizeof (RR_t) * nrequests);
	context_ptrs = malloc (sizeof (RR_t *) * nrequests);
	if (!context_data || !context_ptrs)
		fatal ("Out of memory for context data!");

	LIST_INIT (idle_list);
	LIST_INIT (active_list);
	for (i = 0, cp = context_data; i < nrequests; i++, cp++) {
		cp->state = RR_INIT;
		cp->inst = i;
		cp->h = 0;
		cp->n = 0;
		cp->rtt = 0.0;
		(*context_ptrs) [i] = cp;
		LIST_ADD_TAIL (idle_list, *cp);
	}

	/* Create Data Writer. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	if (reliable)
		wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dw = DDS_Publisher_create_datawriter (pub, req_topic, &wr_qos, NULL, 0);
	if (!dw)
		fatal ("Unable to create data writer!\r\n");

	if (verbose)
		printf ("G: DDS Data Writer created.\r\n");

	/* Create Data Reader. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	if (reliable)
		rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dr = DDS_Subscriber_create_datareader (sub, resp_topic_desc, 
					       &rd_qos, 
					       NULL,
					       0);
	if (!dr)
		fatal ("Unable to create data reader!\r\n");

	if (verbose)
		printf ("G: DDS Data Reader created.\r\n");

	ws = DDS_WaitSet__alloc ();
	if (!ws)
		fatal ("Unable to allocate a WaitSet!");

	if (verbose)
		printf ("G: DDS Waitset allocated.\r\n");

	rc = DDS_DataReader_create_readcondition (dr, ss, vs, is);
	if (!rc)
		fatal ("DDS_DataReader_create_readcondition () returned an error!\r\n");

	if (verbose)
		printf ("G: DDS Readcondition created.\r\n");

	DDS_WaitSet_attach_condition (ws, rc);

	/* Setup Command Writer QoS parameters. */
	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Writer. */
	cw = DDS_Publisher_create_datawriter (pub, ctrl_topic, &wr_qos, NULL, 0);
	if (!cw)
		fatal ("Unable to create command writer!\r\n");

	if (verbose)
		printf ("G: DDS Command Writer created.\r\n");

	/* Setup Command Reader QoS parameters. */
	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

	/* Create Command Reader. */
	memset ((char *) &g_cmd.last_cmd, 0, sizeof (g_cmd.last_cmd));
	g_cmd.rxch = 'G';
	cr = DDS_Subscriber_create_datareader (sub, ctrl_topic_desc, &rd_qos, 
					&g_cr_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!cr)
		fatal ("Unable to create command reader!\r\n");

	if (verbose)
		printf ("G: DDS Command Reader created.\r\n");

	while (g_cmd.last_cmd.cmd != TEST_START && !quit) {
		if (verbose == 2) {
			printf ("GC?");
			fflush (stdout);
		}
		usleep (10000);
	}
	printf ("Test started ... ");
	fflush (stdout);

	ctrace_printd (USER_G_START, NULL, 0);

	memset (req.req_data, '#', g_cmd.last_cmd.dsize);
	req.req_data [g_cmd.last_cmd.dsize] = '\0';

	DDS_DomainParticipant_get_current_time (part, &t);
	t.sec += g_cmd.last_cmd.time;

	/* Send Requests and wait for responses until done. */
	while (!quit) {

		/* Send as many requests as we have contexts. */
		while (LIST_NONEMPTY (idle_list)) {
			cp = LIST_HEAD (idle_list);
			LIST_REMOVE (idle_list, *cp);
			cp->state = RR_WAIT_REPLY;
			req.command = 22;
			req.req_id = (uint32_t) (uintptr_t) dw;
			req.inst_id = cp->inst;
			cp->h = DDS_DataWriter_register_instance (dw, &req);
			if (!cp->h)
				fatal ("Couldn't allocate a new instance!");

			LIST_ADD_TAIL (active_list, *cp);

			ctrace_printd (USER_G_SEND, NULL, 0);
			if (DDS_DataWriter_write (dw, &req, cp->h))
				fatal ("Couldn't write request!");

			if (verbose == 2) {
				printf ("T");
				fflush (stdout);
			}
		}

		/* Can't send more -- wait for some replies. */
		to.sec = 0;
		to.nanosec = 200000000;
		error = DDS_WaitSet_wait (ws, &conds, &to);

		/* Check if test done. */
		DDS_DomainParticipant_get_current_time (part, &nt);
		if (quit ||
		    nt.sec > t.sec ||
		    (nt.sec == t.sec && nt.nanosec >= t.nanosec)) {
			printf ("done!\r\n");
			break;
		}
		if (error == DDS_RETCODE_TIMEOUT)
			continue;

		/* Got some replies -- process them. */
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, nrequests, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read samples: error = %u!\r\n", error);
			break;
		}
		ctrace_printd (USER_G_RECEIVED, NULL, 0);
		rtime = nt.sec + nt.nanosec / (double) 1000000000;
		for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++) {
			if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
				info = DDS_SEQ_ITEM (rx_info, i);
				sample = (RRResp_t *) DDS_SEQ_ITEM (rx_sample, i);
				if (sample->inst_id >= nrequests ||
				    (cp = (*context_ptrs) [sample->inst_id]) == NULL ||
				    cp->state != RR_WAIT_REPLY)
					fatal ("Incorrect reply received!");

				/* Update Request context. */
				cp->state = RR_COMPLETED;
				DDS_DataWriter_unregister_instance (dw, NULL, cp->h);
				cp->n++;

				/* Calculate roundtrip time. */
				stime = info->source_timestamp.sec + 
				        info->source_timestamp.nanosec / (double) 1000000000;
				if (cp->n == 1)
					cp->rtt = (rtime - stime) * 1000000;
				else
					cp->rtt = (cp->rtt * 3.0 + 
						   (rtime - stime) * 1000000) / 4.0;
				LIST_REMOVE (active_list, *cp);
				LIST_ADD_TAIL (idle_list, *cp);

				if (verbose == 2) {
					printf ("R");
					fflush (stdout);
				}
			}
		}
		DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		if (delay) {
			ctrace_printd (USER_TX_DELAY, NULL, 0);

			/* Inter-burst delay. */
			usleep (delay);
		}
	}
	ctrace_printd (USER_G_DONE, NULL, 0);
	send_cmd (cw, TEST_DONE, 0, 0, 0);

	total_reqs = total_resps = 0;
	total = 0;
	min = 10000000;
	max = 0;
	nsamples = 0;
	for (i = 0, cp = context_data; i < nrequests; i++, cp++) {
		total_reqs += cp->n;
		if (cp->state == RR_WAIT_REPLY)
			total_reqs++;
		total_resps += cp->n;
		if (cp->n) {
			total += cp->rtt;
			if (cp->rtt < min)
				min = cp->rtt;
			if (cp->rtt > max)
				max = cp->rtt;
			nsamples++;
			if (dump)
				printf ("%5u: %12.4fus.\r\n", cp->inst, cp->rtt);
		}
	}
	avg = total / nsamples;
	printf ("%u RRs, %u RRs/s, %u bytes/sample, %4.4fus min, %4.4fus max, %4.4fus avg\r\n",
				total_resps, total_resps / g_cmd.last_cmd.time, 
				g_cmd.last_cmd.dsize, min, max, avg);
	free (context_data);
	free (context_ptrs);

	dds_seq_cleanup (&conds);
	dds_seq_cleanup (&rx_sample);
	dds_seq_cleanup (&rx_info);

	DDS_Publisher_delete_datawriter (pub, cw);
	DDS_Subscriber_delete_datareader (sub, cr);
	DDS_Subscriber_delete_datareader (sub, dr);
	DDS_Publisher_delete_datawriter (pub, rr_dw);

	return (NULL);
}

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
	DDS_entity_name ("Technicolor Request/Response timing");

#ifdef DDS_DEBUG
	DDS_Debug_start ();
	DDS_Debug_abort_enable (&quit);
#endif

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						DOMAIN_ID, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!\r\n");

	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

#ifdef CTRACE_USED
	log_fct_str [USER_ID] = user_fct_str;
	/*ctrc_mode (1); ** Cyclic trace mode. */
#endif

	/* Register the topic type. */
	error = register_rr_types (part);
	if (error) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_register_type () failed!\r\n");
	}
	if (verbose)
		printf ("DDS Topic types registered.\r\n");

	/* Create command topic. */
	ctrl_topic = DDS_DomainParticipant_create_topic (part, "RRC", "RRCmd",
									NULL, NULL, 0);
	if (!ctrl_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('RRC') failed!\r\n");
	}
	if (verbose)
		printf ("DDS RRC Topic created.\r\n");

	/* Create Topic Description. */
	ctrl_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "RRC");
	if (!ctrl_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'RRC'!\r\n");
	}

	/* Create Request topic. */
	req_topic = DDS_DomainParticipant_create_topic (part, "RRR", "RRReq",
									NULL, NULL, 0);
	if (!req_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('RRR') failed!\r\n");
	}
	if (verbose)
		printf ("DDS RRR Topic created.\r\n");

	/* Create Request Topic Description. */
	req_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "RRR");
	if (!req_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'RRR'!\r\n");
	}

	/* Create Response topic. */
	resp_topic = DDS_DomainParticipant_create_topic (part, "RRL", "RRResp",
									NULL, NULL, 0);
	if (!resp_topic) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("DDS_DomainParticipant_create_topic ('RRL') failed!\r\n");
	}
	if (verbose)
		printf ("DDS RRL Topic created.\r\n");

	/* Create Response Topic Description. */
	resp_topic_desc = DDS_DomainParticipant_lookup_topicdescription (part, "RRL");
	if (!resp_topic_desc) {
		DDS_DomainParticipantFactory_delete_participant (part);
		fatal ("Unable to create topic description for 'RRL'!\r\n");
	}

	/* Create a publisher. */
	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		fatal ("DDS_DomainParticipant_create_publisher () failed!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Publisher created.\r\n");

	/* Create a subscriber */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		fatal ("DDS_DomainParticipant_create_subscriber () returned an error!\r\n");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

	/* Start either a reader or a writer or both, depending on program options. */
	if (loop) {
		if (controller || generator)
			thread_create (lt, rr_do_loop, part);
		else
			rr_do_loop (NULL);
	}
	if (generator) {
		if (controller || loop)
			thread_create (gt, rr_do_generator, part);
		else
			rr_do_generator (NULL);
	}
	if (controller)
		rr_do_controller ();

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
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed: error = %d", error);

	if (verbose)
		printf ("DDS Entities deleted\r\n");

	free_rr_types ();
	if (verbose)
		printf ("DDS Types deleted\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		fatal ("DDS_DomainParticipantFactory_delete_participant () failed: error = %d", error);

	if (verbose)
		printf ("DDS Participant deleted\r\n");

	return (0);
}

