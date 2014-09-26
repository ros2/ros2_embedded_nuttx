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

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include "thread.h"
#include "libx.h"
#include "tty.h"
#include "chat_msg.h"
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
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"

#define	WAITSETS		/* Set this to use the WaitSet mechanism. */
/*#define TRANSIENT_LOCAL	** Set to use Transient-local Durability. */
/*#define RELIABLE		** Set this for Reliable transfers. */
/*#define KEEP_ALL		** Set this for infinite history. */
#define HISTORY		1	/* # of samples buffered. */
#define	DISPLAY_SELF		/* Define this to display own messages. */

const char		*progname;
char			chatroom [64] = "DDS";		/* Chatroom name. */
char			user_name [64];			/* User name. */
unsigned		domain_id;			/* Domain identifier. */
int			verbose, aborting;
thread_t		rt;
#ifdef DDS_SECURITY
char                    *engine_id;		/* Engine id. */
char                    *cert_path;		/* Certificates path. */
char                    *key_path;		/* Private key path. */
char                    *realm_name;		/* Realm name. */
#endif

DDS_DomainParticipant	part;
DDS_DynamicTypeSupport	ts;
DDS_Publisher		pub;
DDS_Subscriber		sub;
DDS_Topic		topic;
DDS_TopicDescription	td;
DDS_DynamicDataWriter	dw;
DDS_DynamicDataReader	dr;

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "chat -- simple non-graphical chat demo program.\r\n");
	fprintf (stderr, "Usage: chat [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -r <chatroom>     Chatroom name (default=DDS).\r\n");
	fprintf (stderr, "   -u <name>         User name (default=.pid.$PID).\r\n");
	fprintf (stderr, "   -i <domain_id>    Domain identifier.\r\n");
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>         Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>         Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>         Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm>        The realm name.\r\n");
#endif
	fprintf (stderr, "   -v                Verbose.\r\n");
	fprintf (stderr, "   -vv               Very verbose.\r\n");
	fprintf (stderr, "   -h                Show some program help.\r\n");
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
	const char	*cp, *user, *room;
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
					INC_ARG ();
					if (!get_str (&cp, &room)) {
						printf ("chatroom name expected!\r\n");
						usage ();
					}
					memcpy (chatroom, room, strlen (room) + 1);
					break;

				case 'u':
					INC_ARG ();
					if (!get_str (&cp, &user)) {
						printf ("user name expected!\r\n");
						usage ();
					}
					memcpy (user_name, user, strlen (user) + 1);
					break;
				case 'i':
					INC_ARG ();
					get_num (&cp, &domain_id, 0, 255);
					break;
# if 0
				case 't':
					trace = 1;
					break;
# endif
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

void do_chat (DDS_DataWriter dw)
{
	ChatMsg_t		m;
	DDS_InstanceHandle_t	h;
	char			buf [256];

	tty_init ();
	DDS_Handle_attach (tty_stdin,
			   POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			   tty_input,
			   NULL);

	printf ("Welcome to the ROS 2.0 DDS chatroom.\r\n");
	printf ("Anything you type will be sent to all chatroom attendees.\r\n");
	printf ("      (Please write messages of less than 255 characters).\r\n");	
	printf ("Type '!help' for chatroom options.\r\n");
	m.chatroom = chatroom;
	m.from = user_name;
	h = 0;
	while (!aborting) {
#if defined (NUTTX_RTOS)
		fgets(buf, 256, stdin);
#else
		tty_gets (sizeof (buf), buf, 0, 1);
#endif
		if (buf [0] == '!') {
			if (!strcmp (buf + 1, "quit") ||
			    (buf [1] == 'q' && buf [2] == '\0')) {
				aborting = 1;
				break;
			}
			else if (!strcmp (buf + 1, "list"))
				printf ("Attendees:\r\n\t%s\r\n", user_name);
			else if (!memcmp (buf + 1, "user", 4)) {
				if (h) {
					ChatMsg_signal (dw, h, 1);
					h = 0;
				}
				strcpy (user_name, buf + 6);
				printf ("You are now: %s\r\n", user_name);
			}
			else if (!memcmp (buf + 1, "room", 4)) {
				if (h) {
					ChatMsg_signal (dw, h, 1);
					h = 0;
				}
				strcpy (chatroom, buf + 6);
				printf ("Switched to chatroom: %s\r\n", chatroom);
			}
			else if (!strcmp (buf + 1, "info"))
				printf ("Chatroom: %s, Username: %s\r\n", 
							chatroom, user_name);
			else if (!strcmp (buf + 1, "busy"))
				ChatMsg_signal (dw, h, 0);
			else if (!strcmp (buf + 1, "away")) {
				if (h) {
					ChatMsg_signal (dw, h, 1);
					h = 0;
				}
			}
			else if (!strcmp (buf + 1, "help") ||
				 (buf [1] == 'h' && buf [2] == '\0') ||
				 (buf [1] == '?' && buf [2] == '\0')) {
				printf ("Commands:\r\n");
				printf ("    !room <room_name>   -> set the chatroom name.\r\n");
				printf ("    !user <user_name>   -> set the user name.\r\n");
				printf ("    !list               -> list the attendees.\r\n");
				printf ("    !info               -> show chatroom and user.\r\n");
				printf ("    !busy               -> momentarily not involved.\r\n");
				printf ("    !away               -> gone away.\r\n");
				printf ("    !help or !h or !?   -> Show this info.\r\n");
				printf ("    !quit or !q         -> Quit the chatroom.\r\n");
				printf ("    !!<command>         -> DDS debug command.\r\n");
				/* printf ("    !$<command>         -> Shell command.\r\n"); */
			}
			else if (buf [1] == '!')
				DDS_Debug_command (buf + 2);
			/* else if (buf [1] == '$')
				system (buf + 2); */
			else
				printf ("?%s\r\n", buf + 1);
			continue;
		}
		if (!h)
			h = ChatMsg_register (dw, &m);
		m.message = buf;
		ChatMsg_write (dw, &m, h);
	}
}

void read_msg (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	ChatMsg_t		msg;
	DDS_InstanceStateKind	kind;
	int			valid;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (l)

	memset (&msg, 0, sizeof (msg));
	ret = ChatMsg_read_or_take (dr, &msg, DDS_NOT_READ_SAMPLE_STATE, 
					      DDS_ANY_VIEW_STATE,
					      DDS_ANY_INSTANCE_STATE, 1,
					      &valid, &kind);
	if (ret == DDS_RETCODE_OK)
		do {
#ifndef DISPLAY_SELF
			if (!strcmp (msg.from, user_name) &&
			    !strcmp (msg.chatroom, chatroom))
				break;
#endif
			if (valid)
				printf ("%s: %s\r\n", msg.from, msg.message);
			else if (kind == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
				printf ("%s is busy!\r\n", msg.from);
			else if (kind == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
				printf ("%s has left!\r\n", msg.from);
		}
		while (0);

	ChatMsg_cleanup (&msg);
}

#ifdef WAITSETS

static void *chat_reader (void *args)
{
	DDS_DynamicDataReader	dr;
	DDS_WaitSet		ws;
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReadCondition	rc;
	DDS_ConditionSeq	conds = DDS_SEQ_INITIALIZER (DDS_Condition);
	DDS_Duration_t		to;
	DDS_ReturnCode_t	ret;

	dr = args;

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

	ret = DDS_WaitSet_attach_condition (ws, rc);
	if (ret)
		fatal ("Unable to attach condition to a WaitSet!");

	while (!aborting) {
		to.sec = 0;
		to.nanosec = 200000000;	/* Timeout after 200ms. */
		ret = DDS_WaitSet_wait (ws, &conds, &to);
		if (ret == DDS_RETCODE_TIMEOUT)
			continue;

		read_msg (NULL, dr);
	}
	ret = DDS_WaitSet_detach_condition (ws, rc);
	if (ret)
		fatal ("Unable to detach condition from WaitSet (%s)!", DDS_error (ret));

	DDS_WaitSet__free (ws);

	return (NULL);
}

static void start_chat_reader (DDS_DynamicDataReader dr)
{
	thread_create (rt, chat_reader, dr);
}

static void stop_chat_reader (DDS_DynamicDataReader dr)
{
	ARG_NOT_USED (dr)

	thread_wait (rt, NULL);
}

#else

static DDS_DataReaderListener msg_listener = {
	NULL,		/* Sample rejected. */
	NULL,		/* Liveliness changed. */
	NULL,		/* Requested Deadline missed. */
	NULL,		/* Requested incompatible QoS. */
	read_msg,	/* Data available. */
	NULL,		/* Subscription matched. */
	NULL,		/* Sample lost. */
	NULL		/* Cookie */
};

#endif
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

	error = DDS_Security_set_credentials ("Technicolor Chatroom", &credentials);
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


#if defined (NUTTX_RTOS)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <apps/netutils/netlib.h>
#endif

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int dds_chat_main(int argc, char *argv[])
#endif
{
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReaderQos	rd_qos;
	DDS_ReturnCode_t	error;	
	struct in_addr addr;

	/* Configure the network */
	/* Set up our host address */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_IPADDR);
	netlib_sethostaddr("eth0", &addr);

	/* Set up the default router address */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_DRIPADDR);
	netlib_setdraddr("eth0", &addr);

	/* Setup the subnet mask */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_NETMASK);
	netlib_setnetmask("eth0", &addr);

  	/* Start the application */
  	printf("Network configured, starting DDS chat:\n");

	sprintf (user_name, ".pid.%u", getpid ());
	do_switches (argc, argv);
	if (verbose > 1)
		DDS_Log_stdio (1);

	DDS_entity_name ("ROS 2.0 embedded");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif
	part = DDS_DomainParticipantFactory_create_participant (domain_id, NULL, NULL, 0);
	if (!part) {
		printf ("Can't create participant!\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Domain Participant created.\r\n");

	ts = ChatMsg_type_new ();
	if (!ts) {
		printf ("Can't create chat message type!\r\n");
		exit (1);
	}
	error = DDS_DynamicTypeSupport_register_type (ts, part, "ChatMsg");
	if (error) {
		printf ("Can't register chat message type.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "ChatMsg");

	topic = DDS_DomainParticipant_create_topic (part, "Chat", "ChatMsg", NULL, NULL, 0);
	if (!topic) {
		printf ("Can't register chat message type.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS ChatMsg Topic created.\r\n");

	td = DDS_DomainParticipant_lookup_topicdescription (part, "Chat");
	if (!td) {
		printf ("Can't get topicdescription.\r\n");
		exit (1);
	}
	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		printf ("DDS_DomainParticipant_create_publisher () failed!\r\n");
		exit (1);
	}
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
	/* Create a Data Writer. */
	dw = DDS_Publisher_create_datawriter (pub, topic, &wr_qos, NULL, 0);
	if (!dw) {
		printf ("Unable to create chat message writer.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Chat message writer created.\r\n");

	sub = DDS_DomainParticipant_create_subscriber (part, NULL, NULL, 0); 
	if (!sub) {
		printf ("DDS_DomainParticipant_create_subscriber () returned an error!\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Subscriber created.\r\n");

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
	dr = DDS_Subscriber_create_datareader (sub, td, &rd_qos,
#ifndef WAITSETS
			 &msg_listener, DDS_DATA_AVAILABLE_STATUS);
#else
			 NULL, 0);
#endif
	if (!dr) {
		printf ("DDS_DomainParticipant_create_datareader () returned an error!\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Chat message reader created.\r\n");

#ifdef WAITSETS
	start_chat_reader (dr);
#endif
	do_chat (dw);

#ifdef WAITSETS
	stop_chat_reader (dr);
#endif
	DDS_Publisher_delete_datawriter (pub, dw);
	usleep (200000);
	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (verbose)
		printf ("DDS Entities deleted (error = %u).\r\n", error);

	ChatMsg_type_free (ts);
	if (verbose)
		printf ("Chat Type deleted.\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (verbose)
		printf ("DDS Participant deleted (error = %u).\r\n", error);

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		cleanup_security ();
#endif
	return (0);
}

