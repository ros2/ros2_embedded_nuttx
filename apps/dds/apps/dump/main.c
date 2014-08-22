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

/* main.c -- Program to dump DDS traffic. */

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
#include <poll.h>
#include "libx.h"
#include "list.h"
#include "error.h"
#include "nmatch.h"
#include "tty.h"
#include "thread.h"
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
#include "dds/dds_xtypes.h"
#include "dds/dds_dreader.h"
#include "../co/bi_handler.h"

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
/*#define TRACE_DATA	** Define to trace data endpoints. */
/*#define DO_DISC_LISTEN * Listen on discovery info. */
#define	RELIABLE	/* Use reliable transfer mode. */
#define TRANSIENT_LOCAL	/* Use TRANSIENT-LOCAL mode. */
/*#define KEEP_ALL	** Use KEEP_ALL history. */
#define	HISTORY	1	/* History depth. */

typedef struct endpoint_st Endpoint;
typedef struct topic_st Topic;

struct endpoint_st {
	Endpoint			*next;
	Endpoint			*prev;
	Endpoint			*link;
	Topic				*topic;
	int				writer;
	DDS_BuiltinTopicKey_t		key;
	DDS_PublicationBuiltinTopicData data;
};

typedef struct endpoint_list {
	Endpoint	*head;
	Endpoint	*tail;
} EndpointList;

struct topic_st {
	Topic				*next;
	Topic				*prev;
	Endpoint			*readers;
	Endpoint			*writers;
	char				*topic_name;
	char				*type_name;
	int				active;
	unsigned long			ndata;
	unsigned long			ndispose;
	unsigned long			nnowriter;
	DDS_Topic			topic;
	DDS_Subscriber			sub;
	DDS_DynamicDataReader		reader;
	DDS_DynamicType			dtype;
	DDS_DynamicTypeSupport		ts;
};

typedef struct topic_list {
	Topic		*head;
	Topic		*tail;
} TopicList;

const char		*progname;
int			verbose;		/* Verbose if set. */
int			trace;			/* Trace discovery if set. */
int			aborting;		/* Abort program if set. */
int			quit_done;		/* Quit when Tx/Rx done. */
int			paused;			/* Pause program if set. */
unsigned		domain_id;		/* Domain id. */
unsigned		max_events = ~0;
unsigned		sleep_time = 100;
TopicList		topics;
EndpointList		endpoints;
lock_t			topic_lock;
char			auto_filter [64];
#ifdef DDS_SECURITY
char                    *engine_id;		/* Engine id. */
char                    *cert_path;		/* Certificates path. */
char                    *key_path;		/* Private key path. */
char                    *realm_name;		/* Realm name. */
#endif

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "ddsdump -- DDS protocol dump utility.\r\n");
	fprintf (stderr, "Usage: ddsdump [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -p                Startup in paused state.\r\n");
	fprintf (stderr, "   -d                Trace discovered data.\r\n");
	fprintf (stderr, "   -i	<num>          Domain id to use.\r\n");
	fprintf (stderr, "   -a <topics_spec>  Topics to be monitored.\r\n");
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>         Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>         Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>         Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm>        The realm name.\r\n");
#endif
	fprintf (stderr, "   -v                Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv               Extra verbose: log detailed functionality.\r\n");
	exit (1);
}

/* get_num -- Get a number from the command line arguments. */

int get_num (const char **cpp, unsigned *num, unsigned min, unsigned max)
{
	const char	*cp = *cpp;

	while (isspace ((unsigned char) *cp))
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

/* get_str -- Get a string from the command line arguments. */

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

	progname = argv [0];
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'p':
					paused = 1;
					break;
				case 'd':
					trace = 1;
					break;
				case 'i':
					INC_ARG()
					if (!get_num (&cp, &domain_id, 0, 256))
						usage ();
					break;
				case 'a':
					INC_ARG()
					get_str (&cp, &arg_input);
					strcpy (auto_filter, arg_input);
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
	return (i);
}

#include "typecode.h"

void read_data (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	static DDS_DynamicDataSeq drx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	DDS_ReturnCode_t	error;
	DDS_DynamicData		dd;
	Topic			*tp;
	int			secure = (cert_path != NULL);

	tp = (Topic *) l->cookie;
	for (;;) {
		error = DDS_DynamicDataReader_take (dr, &drx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				printf ("Unable to read samples: error = %s!\r\n", DDS_error (error));
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (info->valid_data) {
				dd = DDS_SEQ_ITEM (drx_sample, 0);
				if (!dd)
					fatal ("Empty dynamic sample!");

				tp->ndata++;
				if (!paused) {
					dbg_printf ("%s: ", tp->topic_name);
					DDS_Debug_dump_dynamic (0, tp->ts, dd, 0, secure, 1);
					dbg_printf ("\r\n");
				}
			}
			else if (info->instance_handle && info->instance_state != DDS_ALIVE_INSTANCE_STATE) {
				if (info->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
					tp->ndispose++;
				else if (info->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
					tp->nnowriter++;
				if (!paused) {
					dd = DDS_DynamicDataFactory_create_data (tp->dtype);
					DDS_DynamicDataReader_get_key_value (dr, dd, info->instance_handle);
					dbg_printf ("%s: ", tp->topic_name);
					DDS_Debug_dump_dynamic (0, tp->ts, dd, 1, secure, 1);
					if (info->instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
						dbg_printf (": Not alive - disposed.");
					else if (info->instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
						dbg_printf (": Not alive - no writers.");
					dbg_printf ("\r\n");
				}
			}
			DDS_DynamicDataReader_return_loan (dr, &drx_sample, &rx_info);
		}
		else
			return;
	}
}

static DDS_DataReaderListener listener = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_data,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

void start_reader (DDS_DomainParticipant part, Topic *tp)
{
	Endpoint		*wp;
	DDS_DynamicTypeBuilder	tb;
	DDS_TypeObject		tobj;
	DDS_TopicDescription	td;
	DDS_DataReaderQos	rqos;
	static DDS_Subscriber	sub = NULL;

	wp = tp->writers;
	if (!wp)
		return;

	printf ("* Monitor (%s/%s)\r\n", tp->topic_name, tp->type_name);
	if (!tp->ts) {
		tobj = DDS_TypeObject_create_from_key (part,
						       &wp->data.participant_key,
						       &wp->data.key);
		if (!tobj) {
			printf ("No type information available for %s!\r\n", tp->type_name);
			return;
		}
		tb = DDS_DynamicTypeBuilderFactory_create_type_w_type_object (tobj);
		if (!tb)
			fatal ("Can't get Type from Type object for %s!", tp->type_name);

		DDS_TypeObject_delete (tobj);

		tp->dtype = DDS_DynamicTypeBuilder_build (tb);
		if (!tp->dtype)
			fatal ("Can't get build Type from Type builder for %s!", tp->type_name);

		DDS_DynamicTypeBuilderFactory_delete_type (tb);

		tp->ts = DDS_DynamicTypeSupport_create_type_support (tp->dtype);
		if (!tp->ts)
			fatal ("Can't get TypeSupport from Type for %s!", tp->type_name);

		if (DDS_DynamicTypeSupport_register_type (tp->ts, part, tp->type_name))
			fatal ("Can't register TypeSupport in domain for %s!", tp->topic_name);
	}
	if (!sub) {
		sub = DDS_DomainParticipant_create_subscriber (part, NULL, NULL, 0); 
		if (!sub)
			fatal ("Can't create subscriber!");
	}
	tp->topic = DDS_DomainParticipant_create_topic (part,
							tp->topic_name,
							tp->type_name,
							NULL,
							NULL,
							0);
	if (!tp->topic)
		fatal ("Can't create topic!");

	td = DDS_DomainParticipant_lookup_topicdescription (part, tp->topic_name);
	if (!td)
		fatal ("Can't create topic description!");

	DDS_Subscriber_get_default_datareader_qos (sub, &rqos);
	if (wp->data.durability.kind >= DDS_TRANSIENT_LOCAL_DURABILITY_QOS)
		rqos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	rqos.reliability = wp->data.reliability;
	listener.cookie = tp;
	tp->sub = sub;
	tp->reader = DDS_Subscriber_create_datareader (sub, td, &rqos,
						       &listener,
						       DDS_DATA_AVAILABLE_STATUS);
	tp->active = 1;
}

void stop_reader (DDS_DomainParticipant part, Topic *tp)
{
	tp->active = 0;
	printf ("* Stop monitoring (%s/%s)\r\n", tp->topic_name, tp->type_name);
	DDS_Subscriber_delete_datareader (tp->sub, tp->reader);
	DDS_DomainParticipant_delete_topic (part, tp->topic);
	DDS_DynamicTypeBuilderFactory_delete_type (tp->dtype);
}

void cleanup_reader (DDS_DomainParticipant part, Topic *tp)
{
	DDS_DynamicTypeSupport_unregister_type (tp->ts, part, tp->type_name);
	DDS_DynamicTypeSupport_delete_type_support (tp->ts);
}

Topic *t_lookup (const char *topic_name)
{
	Topic	*p;

	LIST_FOREACH (topics, p)
		if (!strcmp (p->topic_name, topic_name))
			return (p);

	return (NULL);
}

unsigned nendpoints (Endpoint *ep)
{
	unsigned	n = 0;

	while (ep) {
		n++;
		ep = ep->link;
	}
	return (n);
}

Endpoint *ep_lookup (DDS_BuiltinTopicKey_t *key)
{
	Endpoint	*p;

	LIST_FOREACH (endpoints, p)
		if (!memcmp (key, &p->key, sizeof (p->key)))
			return (p);

	return (NULL);
}

Endpoint *ep_create (DDS_DomainParticipant           part,
		     DDS_BuiltinTopicKey_t           *key,
		     const char                      *topic_name,
		     const char                      *type_name,
		     DDS_PublicationBuiltinTopicData *data,
		     int                             writer)
{
	Topic		*tp;
	Endpoint	*ep;

	printf ("* New %s (%s/%s)\r\n", (writer) ? "writer" : "reader", 
						topic_name, type_name);
	ep = ep_lookup (key);
	if (ep) {
		if (writer) {
			ep->data = *data;
			tp = ep->topic;
			ep->data.topic_name = tp->topic_name;
			ep->data.type_name = tp->type_name;
		}
		return (ep);
	}
	ep = malloc (sizeof (Endpoint));
	if (!ep) {
		fprintf (stderr, "Not enough memory to create endpoint!\r\n");
		return (NULL);
	}
	ep->topic = tp = t_lookup (topic_name);
	if (!tp) {
		ep->link = NULL;
		tp = malloc (sizeof (Topic));
		if (!tp) {
			fprintf (stderr, "Not enough memory to create topic!\r\n");
			free (ep);
			return (NULL);
		}
		if (writer) {
			tp->writers = ep;
			tp->readers = NULL;
		}
		else {
			tp->readers = ep;
			tp->writers = NULL;
		}
		tp->topic_name = strdup (topic_name);
		tp->type_name = strdup (type_name);
		tp->active = 0;
		tp->topic = NULL;
		tp->dtype = NULL;
		tp->sub = NULL;
		tp->ts = NULL;
		tp->ndata = tp->ndispose = tp->nnowriter = 0;
		ep->topic = tp;
		LIST_ADD_TAIL (topics, *tp);
	}
	else if (writer) {
		ep->link = tp->writers;
		tp->writers = ep;
	}
	else {
		ep->link = tp->readers;
		tp->readers = ep;
	}
	ep->writer = writer;
	ep->key = *key;
	if (ep->writer) {
		ep->data = *data;
		ep->data.topic_name = tp->topic_name;
		ep->data.type_name = tp->type_name;
	}
	LIST_ADD_TAIL (endpoints, *ep);

	/* Endpoint successfully added -- check if we need a reader. */
	if (writer &&
	    !ep->link &&
	    auto_filter [0] &&
	    !nmatch (auto_filter, tp->topic_name, NM_CASEFOLD))
		start_reader (part, tp);
	return (ep);
}

void ep_delete (DDS_DomainParticipant part, DDS_BuiltinTopicKey_t *key)
{
	Topic		*tp;
	Endpoint	*ep, *xep, *prev;

	ep = ep_lookup (key);
	if (!ep) {
		printf ("Already deleted?\r\n");
		return;
	}
	printf ("* Delete %s (%s/%s)\r\n", (ep->writer) ? "writer" : "reader",
				ep->topic->topic_name, ep->topic->type_name);
	if (ep->writer)
		xep = ep->topic->writers;
	else
		xep = ep->topic->readers;

	/* Remove from topic endpoints list. */
	for (prev = NULL; xep; prev = xep, xep = xep->link)
		if (xep == ep) {

			/* Found it! */
			tp = ep->topic;
			if (!prev)
				if (ep->writer)
					tp->writers = ep->link;
				else
					tp->readers = ep->link;
			else
				prev->next = ep->link;

			/* If last writer and active reader: stop.  */
			if (ep->writer && !tp->writers && tp->active)
				stop_reader (part, tp);

			/* If no more readers nor writers, free topic. */
			if (!tp->readers && !tp->writers) {
				if (tp->ts)
					cleanup_reader (part, tp);
				free (tp->topic_name);
				free (tp->type_name);
				LIST_REMOVE (topics, *tp);
				free (tp);
			}
			break;
		}

	/* Free endpoint. */
	LIST_REMOVE (endpoints, *ep);
	free (ep);
}

void t_dump (void)
{
	Topic	*tp;

	if (LIST_EMPTY (topics)) {
		printf ("No topics discovered.\r\n");
		return;
	}
	printf ("Active   #rd    #wr   #msgs   #disp   #no_w   Topic\r\n");
	printf ("------   ---    ---   -----   -----   -----   -----\r\n");
	lock_take (topic_lock);
	LIST_FOREACH (topics, tp) {
		if (tp->active)
			printf ("   *  ");
		else
			printf ("      ");
		printf ("%6u %6u %7lu %7lu %7lu   %s/%s\r\n", 
					     nendpoints (tp->writers),
					     nendpoints (tp->readers),
					     tp->ndata,
					     tp->ndispose,
					     tp->nnowriter,
					     tp->topic_name,
					     tp->type_name);
	}
	lock_release (topic_lock);
}

void t_auto (const char *cmd)
{
	char	spec [192];

	skip_blanks (&cmd);
	skip_string (&cmd, spec);
	if (!spec [0])
		if (auto_filter [0])
			printf ("No filter setup yet.\r\n");
		else
			printf ("Auto filter: '%s'\r\n", auto_filter);
	else {
		strcpy (auto_filter, spec);
		printf ("Auto filter updated to: '%s'\r\n", auto_filter);
	}
}

void t_monitor (const char *cmd, DDS_DomainParticipant part)
{
	Topic	*tp;
	char	spec [192];

	skip_blanks (&cmd);
	skip_string (&cmd, spec);
	if (spec [0]) {
		lock_take (topic_lock);
		LIST_FOREACH (topics, tp)
			if (!tp->active &&
			    tp->writers &&
			    !nmatch (spec, tp->topic_name, NM_CASEFOLD))
				start_reader (part, tp);
		lock_release (topic_lock);
	}
}

void t_ignore (const char *cmd, DDS_DomainParticipant part)
{
	Topic	*tp;
	char	spec [192];

	skip_blanks (&cmd);
	skip_string (&cmd, spec);
	if (spec [0]) {
		lock_take (topic_lock);
		LIST_FOREACH (topics, tp)
			if (tp->active &&
			    !nmatch (spec, tp->topic_name, NM_CASEFOLD))
				stop_reader (part, tp);
		lock_release (topic_lock);
	}
}

void t_trace (void)
{
	if (trace) {
		printf ("Discovery tracing disabled.\r\n");
		trace = 0;
		bi_log (stdout, 0);
	}
	else {
		printf ("Discovery tracing enabled.\r\n");
		trace = 1;
		bi_log (stdout, BI_ALL_M);
	}
}

void dds_dump (DDS_DomainParticipant part)
{
	char		buf [256], cmd [64];
	const char	*sp;

	ARG_NOT_USED (part)

	LIST_INIT (topics);
	LIST_INIT (endpoints);

	tty_init ();
	DDS_Handle_attach (tty_stdin,
			   POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			   tty_input,
			   NULL);

	printf ("Technicolor DDS Dump program.\r\n");
	printf ("Type 'help' for a list ofcommands.\r\n");
	while (!aborting) {
		printf (">");
		fflush (stdout);
		tty_gets (sizeof (buf), buf, 0, 0);
		sp = buf;
		skip_blanks (&sp);
		skip_string (&sp, cmd);
		if (!memcmp (cmd, "help", 1)) {
			printf ("Following commands are available:\r\n");
			printf ("\tlist                  List all discovered domain topics.\r\n");
			printf ("\tauto <name_spec>      Automatically monitor new matching topics.\r\n");
			printf ("\tmonitor <name_spec>   Start monitoring matching topic(s).\r\n");
			printf ("\tignore <name_spec>    Stop monitoring matching topic(s).\r\n");
			printf ("\ttrace                 Toggle tracing of discovery traffic.\r\n");
#ifndef DDS_DEBUG
			printf ("\tpause                 Pause monitoring.\r\n");
			printf ("\tresume                Resume monitoring.\r\n");
			printf ("\tquit                  Quit the program.\r\n");
#else
			DDS_Debug_help ();
#endif
		}
		else if (!memcmp (cmd, "list", 2))
			t_dump ();
		else if (!memcmp (cmd, "auto", 2))
			t_auto (sp);
		else if (!memcmp (cmd, "monitor", 2))
			t_monitor (sp, part);
		else if (!memcmp (cmd, "ignore", 2))
			t_ignore (sp, part);
		else if (!memcmp (cmd, "trace", 2))
			t_trace ();			
#ifndef DDS_DEBUG
		else if (!memcmp (cmd, "pause", 1))
			paused = 1;
		else if (!memcmp (cmd, "resume", 1))
			paused = 0;
		else if (!memcmp (cmd, "quit", 1))
			aborting = 1;
		else
			printf ("?%s\r\n", buf);
#else
		else
			DDS_Debug_command (buf);
#endif
	}
}

void participant_update (DDS_BuiltinTopicKey_t           *key,
			 DDS_ParticipantBuiltinTopicData *data,
			 DDS_SampleInfo                  *info,
			 uintptr_t                       user)
{
	ARG_NOT_USED (key)
	ARG_NOT_USED (data)
	ARG_NOT_USED (info)
	ARG_NOT_USED (user)
}

void topic_update (DDS_TopicBuiltinTopicData *data,
		   DDS_SampleInfo            *info,
		   uintptr_t                 user)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (info)
	ARG_NOT_USED (user)
}

void publisher_update (DDS_BuiltinTopicKey_t           *key,
		       DDS_PublicationBuiltinTopicData *data,
		       DDS_SampleInfo                  *info,
		       uintptr_t                       user)
{
	DDS_DomainParticipant part = (DDS_DomainParticipant) user;

	lock_take (topic_lock);
	if ((info->view_state & DDS_NEW_VIEW_STATE) != 0 /*new*/ ||
	    info->instance_state == DDS_ALIVE_INSTANCE_STATE /*update*/)
		ep_create (part, key, data->topic_name, data->type_name, data, 1);
	else /*delete*/
		ep_delete (part, key);
	lock_release (topic_lock);
}

void subscriber_update (DDS_BuiltinTopicKey_t            *key,
			DDS_SubscriptionBuiltinTopicData *data,
			DDS_SampleInfo                   *info,
			uintptr_t                        user)
{
	DDS_DomainParticipant part = (DDS_DomainParticipant) user;

	lock_take (topic_lock);
	if ((info->view_state & DDS_NEW_VIEW_STATE) != 0 /*new*/ ||
	    info->instance_state == DDS_ALIVE_INSTANCE_STATE /*update*/)
		ep_create (part, key, data->topic_name, data->type_name, NULL, 0);
	else /*delete*/
		ep_delete (part, key);
	lock_release (topic_lock);
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

	error = DDS_Security_set_credentials ("Technicolor DDS dump", &credentials);
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
	DDS_DomainParticipant	part;
	DDS_ReturnCode_t	error;

	do_switches (argc, argv);
	if (verbose > 1)
		DDS_Log_stdio (1);

	DDS_entity_name ("Technicolor DDS dump");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif
#ifdef DDS_DEBUG
	DDS_Debug_abort_enable (&aborting);
	DDS_Debug_control_enable (&paused, &max_events, &sleep_time);
#endif

#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	lock_init_nr (topic_lock, "dump_topics");

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!part)
		fatal ("DDS_DomainParticipantFactory_create_participant () failed!");

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

	if (trace)
		bi_log (stdout, BI_ALL_M);

	error = bi_attach (part,
			   BI_ALL_M,
			   participant_update,
			   topic_update,
			   publisher_update,
			   subscriber_update,
			   (uintptr_t) part);
	if (error)
		fatal ("Couldn't start the discovery readers.");

	if (verbose)
		printf ("DDS Discovery readers created.\r\n");

	/* Start either a reader or a writer depending on program options. */
	dds_dump (part);

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		fatal ("DDS_DomainParticipant_delete_contained_entities () failed (%s)!", DDS_error (error));

	if (verbose)
		printf ("DDS Entities deleted\r\n");

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

