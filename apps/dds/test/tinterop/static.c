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

/* static.c -- Static shapes type handler. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/dds_tsm.h"
#include "static.h"

/*#define USE_TAKE		** Use take() i.o. read() to get items. */
#define	RELIABLE		/* Use Reliable mode i.o. Best-Effort. */
#define TRANSIENT_LOCAL 	/* Define to use Transient-Local Durability QoS.*/
/*#define KEEP_ALL		** Use KEEP_ALL history. */
#define HISTORY 	1	/* History depth */
#define	EXCLUSIVE	0	/* Exclusive mode? */
#define	STRENGTH	2	/* Exclusive strength. */

typedef struct shape_type_st {
	char		color [128];
	int		x;
	int		y;
	int		shapesize;
} ShapeType_t;

static DDS_TypeSupport_meta shape_tsm [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "ShapeType", sizeof (struct shape_type_st), 0, 4, 0, NULL },
	{ CDR_TYPECODE_CSTRING,TSMFLAG_KEY, "color", 128, offsetof (struct shape_type_st, color), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "x", 0, offsetof (struct shape_type_st, x), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "y", 0, offsetof (struct shape_type_st, y), 0, 0, NULL },
	{ CDR_TYPECODE_LONG,   0, "shapesize", 0, offsetof (struct shape_type_st, shapesize), 0, 0, NULL }
};

#define	MAX_TOPICS	4
#define	MAX_READERS	8
#define	MAX_WRITERS	8

typedef struct {
	char			*name;
	DDS_Topic		t;
	DDS_TopicDescription	desc;
	unsigned		nreaders;
	unsigned		nwriters;
} Topic_t;

static Topic_t		topics [MAX_TOPICS];
static unsigned		ntopics;

typedef struct {
	Topic_t		*t;
	DDS_DataReader	dr;
} Reader_t;

static Reader_t		readers [MAX_READERS];
static unsigned		nreaders;

typedef struct {
	Topic_t		*t;
	DDS_DataWriter	dw;
} Writer_t;

static Writer_t		writers [MAX_READERS];
static unsigned		nwriters;

static DDS_DomainParticipant part;
static DDS_TypeSupport	shape_ts;
static DDS_Subscriber	sub;
static DDS_Publisher	pub;

extern void fatal (const char *s);

/* Fatal error function. */

/* register_static_type -- Register a static shapes type. */

void register_static_type (DDS_DomainParticipant dpart)
{
	DDS_ReturnCode_t	error;

	shape_ts = DDS_DynamicType_register (shape_tsm);
       	if (!shape_ts)
               	fatal ("Can't create static type!");

	error = DDS_DomainParticipant_register_type (dpart, shape_ts, "ShapeType");
	if (error != DDS_RETCODE_OK)
               	fatal ("Can't register static type in participant!");

	part = dpart;
}

/* topic_get -- Get a new topic reference with the given name. */

static Topic_t *topic_get (const char *topic_name)
{
	unsigned	h;
	Topic_t		*tp = NULL;

	for (h = 0; h < MAX_READERS; h++)
		if (topics [h].name && !strcmp (topics [h].name, topic_name))
			return (&topics [h]);
	
	if (ntopics == MAX_TOPICS)
		fatal ("Too many different topics");

	for (h = 0; h < MAX_READERS; h++)
		if (!topics [h].t)
			break;

	tp = &topics [h];
	tp->name = strdup (topic_name);
	tp->t = DDS_DomainParticipant_create_topic (part, tp->name, "ShapeType", NULL, NULL, 0);
	if (!tp->t)
		fatal ("Can't create static topic!");

	printf ("DDS Static Topic (%s) created.\r\n", topic_name);
	tp->desc = DDS_DomainParticipant_lookup_topicdescription (part, topic_name);
	if (!tp->desc)
		fatal ("Can't create topicdescription!");

	tp->nreaders = tp->nwriters = 0;
	ntopics++;
	return (tp);
}

static void topic_free (Topic_t *tp, int writer)
{
	if (writer)
		tp->nwriters--;
	else
		tp->nreaders--;

	if (tp->nreaders || tp->nwriters)
		return;

	DDS_DomainParticipant_delete_topic (part, tp->t);
	tp->t = NULL;
	tp->desc = NULL;
	free (tp->name);
	tp->name = NULL;
	ntopics--;
}

/* static_writer_create -- Create a static shapes writer. */

unsigned static_writer_create (const char *topic_name)
{
	DDS_DataWriterQos 	wr_qos;
	Topic_t			*tp;
	Writer_t		*wp;
	unsigned		h;

	if (nwriters >= MAX_WRITERS)
		fatal ("Too many static data writers!");

	for (h = 0; h < MAX_WRITERS; h++)
		if (!writers [h].dw)
			break;

	wp = &writers [h];
	tp = topic_get (topic_name);

	if (!pub) {
		pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0); 
		if (!pub)
			fatal ("DDS_DomainParticipant_create_publisher () returned an error!");

		printf ("DDS Static Publisher created.\r\n");
	}

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
	if (EXCLUSIVE) {
		wr_qos.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
		wr_qos.ownership_strength.value = STRENGTH;
	}
#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Create a Data Writer. */
	wp->dw = DDS_Publisher_create_datawriter (pub, tp->t, &wr_qos, NULL, 0);
	if (!wp->dw) {
		fatal ("Unable to create writer");
		DDS_DomainParticipantFactory_delete_participant (part);
	}
	printf ("DDS Static Writer (%s) created.\r\n", tp->name);

	tp->nwriters++;
	nwriters++;
	wp->t = tp;
	return (h);
}

static void static_listen (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	ShapeType_t		*sample;
	ShapeType_t		data;
	DDS_ReturnCode_t	error;
	unsigned		i;

	for (i = 0; i < MAX_READERS; i++)
		if (readers [i].dr == dr)
			break;

	if (i >= MAX_READERS)
		fatal ("static listener called with invalid DataReader argument!");

	for (;;) {
#ifdef USE_TAKE
			error = DDS_DataReader_take (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#else
			error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
#endif
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read samples: error = %s!\r\n", DDS_error (error));
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (info->valid_data)
				sample = DDS_SEQ_ITEM (rx_sample, 0);
			else
				sample = NULL;

			printf ("{%s}: ", readers [i].t->name);
			if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				printf ("Color=%s, X=%d, Y=%d", sample->color, sample->x, sample->y);
				if (info->view_state == DDS_NEW_VIEW_STATE)
					printf (" [NEW]");
			}
			else {
				DDS_DataReader_lookup_instance (dr, &data);
				printf ("Color=%s", data.color);
				if (info->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
					printf (" [NO_WRITERS]");
				else
					printf (" [DISPOSED]");
			} 
			printf ("\r\n");
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else
			break;
	}
}

static DDS_DataReaderListener static_listener = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	static_listen,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

/* static_reader_create -- Create a static shapes reader. */

unsigned static_reader_create (const char *topic_name)
{
	DDS_DataReaderQos	rd_qos;
	Topic_t			*tp;
	Reader_t		*rp;
	unsigned		h;

	if (nreaders >= MAX_READERS)
		fatal ("Too many static data readers!");

	for (h = 0; h < MAX_READERS; h++)
		if (!readers [h].dr)
			break;

	rp = &readers [h];
	tp = topic_get (topic_name);
	
	if (!sub) {
		sub = DDS_DomainParticipant_create_subscriber (part, NULL, NULL, 0); 
		if (!sub)
			fatal ("DDS_DomainParticipant_create_subscriber () returned an error!");

		printf ("DDS Static Subscriber created.\r\n");
	}
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
	if (EXCLUSIVE)
		rd_qos.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;

	/* Create a reader. */
#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	rp->dr = DDS_Subscriber_create_datareader (sub, tp->desc, &rd_qos, &static_listener, DDS_DATA_AVAILABLE_STATUS);
	if (!rp->dr)
		fatal ("DDS_DomainParticipant_create_datareader () returned an error!");

	printf ("DDS Static Reader (%s) created.\r\n", tp->name);
	tp->nreaders++;
	nreaders++;
	rp->t = tp;
	return (h);
}

/* static_writer_delete -- Delete a static shapes writer. */

void static_writer_delete (unsigned h)
{
	Writer_t	*wp;

	if (h >= MAX_WRITERS || !writers [h].dw || !writers [h].t)
		fatal ("static_writer_delete: Invalid writer handle!");

	wp = &writers [h];
	DDS_Publisher_delete_datawriter (pub, wp->dw);
	topic_free (wp->t, 1);
	wp->t = NULL;
	wp->dw = NULL;
	nwriters--;
	if (!nwriters) {
		DDS_DomainParticipant_delete_publisher (part, pub);
		pub = NULL;
	}
}

void static_writer_write (unsigned h, const char *color, unsigned x, unsigned y)
{
	Writer_t	*wp;
	ShapeType_t	sample;

	if (h >= MAX_WRITERS || !writers [h].dw || !writers [h].t)
		fatal ("static_writer_writer: Invalid writer handle!");

	wp = &writers [h];
	strcpy (sample.color, color);
	sample.x = x;
	sample.y = y;
	sample.shapesize = 30;
	DDS_DataWriter_write (wp->dw, &sample, 0);
}

/* static_reader_delete -- Delete a static shapes reader. */

void static_reader_delete (unsigned h)
{
	Reader_t	*rp;

	if (h >= MAX_READERS || !readers [h].dr || !readers [h].t)
		fatal ("static_reader_delete: Invalid reader handle!");

	rp = &readers [h];
	DDS_Subscriber_delete_datareader (sub, rp->dr);
	topic_free (rp->t, 0);
	rp->t = NULL;
	rp->dr = NULL;
	nreaders--;
	if (!nreaders) {
		DDS_DomainParticipant_delete_subscriber (part, sub); 
		sub = NULL;
	}
}

/* unregister_static_type -- Unregister and delete the static type. */

void unregister_static_type (void)
{
	DDS_DomainParticipant_unregister_type (part, shape_ts, "ShapeType");
	DDS_DynamicType_free (shape_ts);
}

