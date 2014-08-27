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
#include <stdarg.h>
#include <unistd.h>
#include <dds/dds_dcps.h>
#include <dds/dds_aux.h>
#include <dds/dds_debug.h>
#include "libx.h"
#include "thread.h"

#define	DOMAIN_ID	0
#define MAX_PARALLEL   100

const char *progname;
int provider;
int verbose = 0;
unsigned iterations = 100000;
unsigned parallelism = 2;

#ifdef CTRACE_USED
enum {
	USER_REQ_TX, USER_REQ_RX,
	USER_RSP_TX, USER_RSP_RX,
	USER_WAIT
};

static const char *user_fct_str [] = {
	"TxRequest", "RxRequest",
	"TxResponse", "RxResponse",
	"TxWait"
};

#define	ctrace_printd(i,d,l)	DDS_CTrace_printd(i,d,l)

#else
#define	ctrace_printd(i,d,l)
#endif

void usage (void)
{
	fprintf (stderr, "%s [-c [-t <iter>]|-p] [-v]\n", progname);
	fprintf (stderr, "   -c          Consumer\n");
	fprintf (stderr, "   -p          Provider\n");
	fprintf (stderr, "   -v          Verbose\n");
	fprintf (stderr, "   -t <iter>   Number of requests to send\n");
	exit (1);
}

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
					provider = 0;
					break;
				case 'p':
					provider = 1;
					break;
				case 'v':
					verbose = 1;
					break;
				case 't':
					if (*cp) {
						sscanf(cp,"%u", &iterations);
						while (*cp) cp++; 
						break;	
					} else if (i+1<argc) {
						sscanf(argv[i+1],"%u", &iterations);
						i++;
						break;	
					}
					usage ();
					break;
				case 'l':
					if (*cp) {
						sscanf(cp,"%u", &parallelism);
						while (*cp) cp++; 
						break;	
					} else if (i+1<argc) {
						sscanf(argv[i+1],"%u", &parallelism);
						i++;
						break;	
					}
					printf("PARA=%d\n", parallelism);
					usage ();
					break;


				default:
					usage ();
				break;
			}
		}
	}

	return (i);
}

DDS_DomainParticipant	part;

void fini_generic()
{
	DDS_ReturnCode_t	error;
	if (part) {
		error = DDS_DomainParticipant_delete_contained_entities (part);
		if (error)
			fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)", DDS_error (error));

		if (verbose)
			printf ("DDS Entities deleted\n");

		error = DDS_DomainParticipantFactory_delete_participant (part);
		if (error)
			fatal ("DDS_DomainParticipantFactory_delete_participant () failed (%s)", DDS_error (error));
	}
}

typedef struct rr_req_st {
	uint32_t	id;
	uint32_t	command;
} Request_t;

static DDS_TypeSupport_meta rr_req_tsm [] = {
	{ CDR_TYPECODE_STRUCT,  1, "Req", sizeof (struct rr_req_st), 0, 2, 0, NULL },
	{ CDR_TYPECODE_ULONG,   1, "id",  0, offsetof (struct rr_req_st, id), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,   0, "command", 0, offsetof (struct rr_req_st, command), 0, 0, NULL },
};

typedef struct rr_resp_st {
	uint32_t	id;
} Response_t;

static DDS_TypeSupport_meta rr_resp_tsm [] = {
	{ CDR_TYPECODE_STRUCT,  1, "Resp", sizeof (struct rr_resp_st), 0, 1, 0, NULL },
	{ CDR_TYPECODE_ULONG,   1, "id", 0, offsetof (struct rr_resp_st, id), 0, 0, NULL },
};

static DDS_TypeSupport Request_ts, Response_ts;

DDS_Topic		req_topic;
DDS_Topic		resp_topic;
DDS_Subscriber		sub;
DDS_Publisher		pub;

void init_generic()
{
	DDS_ReturnCode_t	error;

	DDS_entity_name ("Technicolor Request/Response test");
	part = DDS_DomainParticipantFactory_create_participant (DOMAIN_ID, NULL, NULL, 0);
	if (!part) {
		fini_generic();
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");
	}

	Request_ts = DDS_DynamicType_register (rr_req_tsm);
	if (!Request_ts)
		fatal ("Could not register dynamic type!");

	error = DDS_DomainParticipant_register_type (part, Request_ts, "Request");
	if (error)
		fatal ("Could not register type!");

	Response_ts = DDS_DynamicType_register (rr_resp_tsm);
	if (!Response_ts)
		fatal ("Could not register dynamic type!");

	error = DDS_DomainParticipant_register_type (part, Response_ts, "Response");


	req_topic = DDS_DomainParticipant_create_topic (part, "RequestTopic", "Request", NULL, NULL, 0);
	if (!req_topic) {
		fini_generic();
		fatal ("DDS_DomainParticipant_create_topic ('RequestTopic') failed!");
	}

	resp_topic = DDS_DomainParticipant_create_topic (part, "ResponseTopic", "Response", NULL, NULL, 0);
	if (!resp_topic) {
		fini_generic();
		fatal ("DDS_DomainParticipant_create_topic ('ResponseTopic') failed!");
	}

	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		fini_generic();
		fatal ("DDS_DomainParticipant_create_publisher () failed!");
	}

	/* Create a subscriber */
	sub = DDS_DomainParticipant_create_subscriber (part, 0, NULL, 0);
	if (!sub) {
		fini_generic();
		fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");
	}
}


DDS_DataReader request_reader;
DDS_DataWriter response_writer;
DDS_SEQUENCE(Request_t *,RequestSeq);

#define	BEGIN	0
#define	CONT	1
#define	END	2

void request_listener (DDS_DataReaderListener *l, const DDS_DataReader r)
{
	static RequestSeq	rx_sample = DDS_SEQ_INITIALIZER (Request_t *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error = 0;
	static Response_t	reply;
	static Request_t	request;
	unsigned		i;
	static unsigned		m = 0;

	(void) l;

	if (verbose) printf("Received (some) request!\n");
	while (!error) {
		error = DDS_DataReader_take (r, (DDS_DataSeq *) &rx_sample, &rx_info, DDS_LENGTH_UNLIMITED, ss, vs, is);
		if (verbose) printf("Took %d request(s) \n",  DDS_SEQ_LENGTH (rx_sample));
		for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++) {
			if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
				ctrace_printd (USER_REQ_RX, NULL, 0);
				m++;
				if (m >= 1000) {
					m = 0;
					printf (".");
					fflush (stdout);
				}
				if (verbose) printf("Write response for %d\n", DDS_SEQ_ITEM (rx_sample, i)->id);
				reply.id=DDS_SEQ_ITEM (rx_sample, i)->id;
#ifdef PROFILE
				if (DDS_SEQ_ITEM (rx_sample, i)->command == BEGIN)
					prof_clear (0, 0);
				else if (DDS_SEQ_ITEM (rx_sample, i)->command == END)
					prof_list ();
#endif
				ctrace_printd (USER_RSP_TX, NULL, 0);
				DDS_DataWriter_write (response_writer, &reply, DDS_HANDLE_NIL);
#ifdef CTRACE_USED
				if (DDS_SEQ_ITEM (rx_sample, i)->command == BEGIN)
					DDS_CTrace_start ();
				else if (DDS_SEQ_ITEM (rx_sample, i)->command == END) {
					DDS_CTrace_stop ();
					DDS_CTrace_save ("provider.ctrc");
				}
#endif
			}

			if (verbose) printf("State = %d\n", DDS_SEQ_ITEM (rx_info, i)->instance_state);

			if (DDS_SEQ_ITEM (rx_info, i)->instance_state != DDS_ALIVE_INSTANCE_STATE) {
				error = DDS_DataReader_get_key_value(r, &request, DDS_SEQ_ITEM (rx_info, i)->instance_handle);
				if (verbose) printf("UNREGISTER %d for %d\n", error, request.id);
				reply.id = request.id;
				DDS_DataWriter_unregister_instance(response_writer, &reply, DDS_HANDLE_NIL);
			}
		}

		if (!error) error = DDS_DataReader_return_loan(r, (DDS_DataSeq *) &rx_sample, &rx_info);
	}
}

void init_provider(void)
{
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriterQos	wr_qos;
	DDS_TopicDescription	topic_desc;

	static DDS_DataReaderListener listener = {
		NULL,
		NULL,
		NULL,
		NULL,
		request_listener,
		NULL,
		NULL,
		NULL	
	};

	init_generic();




	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
	wr_qos.history.depth    = 1;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	wr_qos.latency_budget.duration.sec = DDS_DURATION_ZERO_SEC; 
	wr_qos.latency_budget.duration.nanosec = DDS_DURATION_ZERO_NSEC;
	response_writer = DDS_Publisher_create_datawriter (pub, resp_topic, &wr_qos, NULL, 0);

	if (!response_writer)
		fatal ("Could not create writer!");



	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);

	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	rd_qos.latency_budget.duration.sec = DDS_DURATION_ZERO_SEC; 
	rd_qos.latency_budget.duration.nanosec = DDS_DURATION_ZERO_NSEC;
	rd_qos.history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
	rd_qos.history.depth    = 1;
/*	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.sec = 0;
	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.nanosec = 1000;
*/
	topic_desc = DDS_DomainParticipant_lookup_topicdescription(part, DDS_Topic_get_name(req_topic));
	request_reader = DDS_Subscriber_create_datareader (sub, topic_desc, &rd_qos, &listener, DDS_DATA_AVAILABLE_STATUS);

	if (!request_reader)
		fatal ("Could not create reader!");

}

void fini_provider(void)
{
	fini_generic();
}

DDS_DataReader response_reader;
DDS_DataWriter request_writer;
pthread_mutex_t reply_mutex;
pthread_cond_t reply_kick;

void response_listener (DDS_DataReaderListener *l, const DDS_DataReader r)
{
	(void) l;
	(void) r;
	pthread_mutex_lock(&reply_mutex);
	pthread_cond_signal(&reply_kick);
	pthread_mutex_unlock(&reply_mutex);
}

void init_consumer(void)
{
	DDS_DataReaderQos	rd_qos;
	DDS_DataWriterQos	wr_qos;
	static DDS_DataReaderListener listener = {
		NULL,
		NULL,
		NULL,
		NULL,
		response_listener,
		NULL,
		NULL,
		NULL	
	};


	init_generic();


	pthread_mutex_init(&reply_mutex, NULL);
	pthread_cond_init (&reply_kick, NULL);



	DDS_Subscriber_get_default_datareader_qos (sub, &rd_qos);
	rd_qos.history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
	rd_qos.history.depth    = 1;
	rd_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	rd_qos.latency_budget.duration.sec = DDS_DURATION_ZERO_SEC; 
	rd_qos.latency_budget.duration.nanosec = DDS_DURATION_ZERO_NSEC;
	/*rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.sec = 0;
	rd_qos.reader_data_lifecycle.autopurge_nowriter_samples_delay.nanosec = 1000; */
	response_reader = DDS_Subscriber_create_datareader (sub, DDS_Topic_get_topicdescription(resp_topic), &rd_qos, &listener, DDS_DATA_AVAILABLE_STATUS);

	if (!response_reader)
		fatal ("Could not create reader!");

	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
	wr_qos.history.kind     = DDS_KEEP_LAST_HISTORY_QOS;
	wr_qos.history.depth    = 1;
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	wr_qos.latency_budget.duration.sec = DDS_DURATION_ZERO_SEC; 
	wr_qos.latency_budget.duration.nanosec = DDS_DURATION_ZERO_NSEC;
	request_writer = DDS_Publisher_create_datawriter (pub, req_topic, &wr_qos, NULL, 0);
	
	if (!request_writer)
		fatal ("Could not create writer!");

}

void fini_consumer(void)
{
	fini_generic();
}
DDS_SEQUENCE(Response_t *,ResponseSeq);


void do_consumer_request(void)
{
	static ResponseSeq	 	rx_sample = DDS_SEQ_INITIALIZER (Response_t *);
	static DDS_SampleInfoSeq 	rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	static Request_t		req;
	DDS_SampleStateMask		ss = DDS_ANY_SAMPLE_STATE;
	DDS_ViewStateMask		vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask		is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t		take_error = DDS_RETCODE_NO_DATA;
	DDS_ReturnCode_t		error = DDS_RETCODE_NO_DATA;
	DDS_InstanceHandle_t		h;
	unsigned 				oreq [MAX_PARALLEL];
	int 				noreq = 0;
	int 				last_id=0;
	unsigned i, j, k, m = 0;

	memset(oreq, parallelism, sizeof(int));

	pthread_mutex_lock(&reply_mutex);

	for (i=0; i<(iterations/parallelism); i++)
	{
		for (j=0; j<parallelism; j++) {
			ctrace_printd (USER_REQ_TX, NULL, 0);
			req.id = ++last_id;
			oreq[noreq] = req.id;
			noreq ++;
			if (!i && !j)
				req.command = BEGIN;
			else if (i == iterations / parallelism - 1 && j == parallelism - 1) {
				req.command = END;
#ifdef CTRACE_USED
				DDS_CTrace_stop ();
				DDS_CTrace_save ("consumer.ctrc");
#endif
			}
			else
				req.command = CONT;
			if (DDS_DataWriter_write (request_writer, &req, DDS_HANDLE_NIL))
				fatal ("Couldn't write request!");
			if (verbose) printf("Wrote request %d\n", last_id);
			m++;
			if (m >= 1000) {
				m = 0;
				printf (".");
				fflush (stdout);
			}
		}
		take_error=0;

		pthread_cond_wait(&reply_kick, &reply_mutex);


		do 
		{
			ctrace_printd (USER_WAIT, NULL, 0);
			if (take_error == DDS_RETCODE_NO_DATA) pthread_cond_wait(&reply_kick, &reply_mutex);
			take_error = DDS_DataReader_take (response_reader, (DDS_DataSeq *) &rx_sample, &rx_info, 1, ss, vs, is);
			for (j = 0; j < DDS_SEQ_LENGTH (rx_sample); j++) {
				if (DDS_SEQ_ITEM (rx_info, j)->valid_data) {
					ctrace_printd (USER_RSP_RX, NULL, 0);
					if (verbose) printf("Valid Sample %d: %d\n", j,  DDS_SEQ_ITEM (rx_sample, j)->id);
					req.id=DDS_SEQ_ITEM (rx_sample, j)->id;

					for (k=0; k<parallelism; k++)
						if (req.id == oreq[k]) {
							if (verbose) 
								printf("Received response for %d/%d\n", DDS_SEQ_ITEM (rx_sample, j)->id, oreq[k]);
							oreq[k]=0; 
							noreq --;
						}
					h= DDS_DataWriter_lookup_instance(request_writer, &req);
					error = DDS_DataWriter_unregister_instance (request_writer, &req, h);
					if (verbose) printf("Unreg = %d\n", error);
				}
				else {
					if (verbose) printf("Invalid Sample %d\n", j);
				}
			}
			DDS_DataReader_return_loan(response_reader, (DDS_DataSeq *) &rx_sample, &rx_info);
		} while (noreq > 0);
	}
	pthread_mutex_unlock(&reply_mutex);
}

void handle_provider_requests(void)
{
	while(1) {
		sleep(1);
		if (verbose) printf("Provider\n");
	}
}
	
int 
main(int argc, const char ** argv)
{
	do_switches (argc, argv);

#ifdef CTRACE_USED
	log_fct_str [USER_ID] = user_fct_str;
	DDS_CTrace_mode (1);	/* Cyclic trace mode. */
#endif
	if (provider) {
		init_provider();
#ifdef CTRACE_USED
		DDS_CTrace_stop ();
#endif
		handle_provider_requests();
		fini_provider();
	} else {
		init_consumer();
		do_consumer_request();
		fini_consumer();
	}
#ifdef PROFILE
	prof_list ();
#endif

	exit(EXIT_SUCCESS);
}
