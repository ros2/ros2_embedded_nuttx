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

/* main.c -- DDS Central Discovery Daemon. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include <ctype.h>
#include <signal.h>
#include <syslog.h>
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
#include "dds/dds_debug.h"
#include "dds/dds_cdd.h"
#include "dds/dds_aux.h"

#include "guid.h"	/* To get the value of MAX_DOMAIN_ID */
#include "error.h"	/* for ARG_NOT_USED */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#ifndef USE_DAEMON
#include <linux/kdev_t.h>

#define DEVNULL     "/dev/null"
#define DEVNULL_MAJ 1
#define DEVNULL_MIN 3

int daemon (int nochdir, int noclose)
{
	pid_t pid;

	/* Do the fork. */
	pid = fork ();
	if (pid < 0)
		return (-1);	/* errno contains the reason. */

	else if (pid != 0)
		exit (0);	/* Let the parent terminate. */

	/* Make ourselves a session leader (and give up controlling terminal).*/
	if (setsid () == (pid_t) -1)
		return (-1);

	/* fork() again, so we are not a session leader. */
	pid = fork ();
	if (pid < 0)
		return (-1);	/* errno contains the reason. */

	else if (pid != 0)
		exit (0);	/* Let the parent terminate, again. */

	if (!nochdir && chdir ("/"))
		return (-1);

	if (!noclose) {
		struct stat	buf;
		int		fd;

		if (stat (DEVNULL, &buf))
			return (-1);

		if (!S_ISCHR (buf.st_mode))
			return (-1);

		if (MKDEV (DEVNULL_MAJ, DEVNULL_MIN) != buf.st_rdev)
			return (-1);

		fd = open (DEVNULL, O_RDWR | O_NOCTTY);
		if (fd == -1)
			return (-1);

		if (dup2 (fd, STDIN_FILENO) != STDIN_FILENO) {
			close (fd);
			return (-1);
		}
		if (dup2 (fd, STDOUT_FILENO) != STDOUT_FILENO) {
			close (fd);
			return (-1);
		}
		if (dup2 (fd, STDERR_FILENO) != STDERR_FILENO) {
			close (fd);
			return (-1);
		}
		close (fd);
	}

	/* Control the initial permissions of newly created files */
	umask(0);

	return (0);
}
#endif

#ifndef DEF_DOMAIN_ID
#define	DEF_DOMAIN_ID	0
#endif

/*#define TRACE_DISC	** Define to trace discovery endpoints. */
#define DISC_LISTEN	/* Listen on discovery info. */

const char		*progname;
int			verbose;		/* Verbose if set. */
int			trace;			/* Trace messages if set. */
int			reset_shm;		/* Reset shared memory. */
int			force_start;		/* Startup, even if already there. */
int			aborting;		/* Abort program if set. */
unsigned		domain_id;		/* Domain id to use. */
#ifdef DISC_LISTEN
int			trace_disc;		/* Display Discovery info. */
#endif
int			daemonize = 1;		/* Daemonize cdd */
#ifdef DDS_SECURITY
char                    *engine_id;		/* Engine id. */
char                    *cert_path;		/* Certificates path. */
char                    *key_path;		/* Private key path. */
char                    *realm_name;		/* Realm name. */
#endif
const char 		*pipe_name;

static void d_printf (const char *fmt, ...)
{
	va_list	arg;

	va_start (arg, fmt);
	if (daemonize == 1)
		vsyslog (LOG_ERR, fmt, arg);
	else
		vprintf (fmt, arg);
	va_end (arg);
}

static void f_printf (const char *fmt, ...)
{
	va_list	arg;

	va_start (arg, fmt);
	if (daemonize == 1)
		vsyslog (LOG_ERR, fmt, arg);
	else {
		vprintf (fmt, arg);
		printf ("\n");
	}
	va_end (arg);
	exit (1);
}

void pipe_error (void)
{
	FILE *fp = fopen (pipe_name, "w");

	if (fp != NULL)	{
		fwrite ("FAIL", 4, 1, fp);
		fclose (fp);
	}
}

const char *kind_str [] = {
	NULL,
	"ALIVE",
	"NOT_ALIVE_DISPOSED",
	NULL,
	"NOT_ALIVE_NO_WRITERS"
};

/* usage -- Print out program usage. */

void usage (void)
{
	fprintf (stderr, "cdd -- Central Discovery Daemon for the DDS protocol.\r\n");
	fprintf (stderr, "Usage: cdd [switches]\r\n");
	fprintf (stderr, "\r\n");
	fprintf (stderr, "Switches:\r\n");
	fprintf (stderr, "   -i <domain> Domain Id (default: %u).\r\n", DEF_DOMAIN_ID);
	fprintf (stderr, "   -f          Continue if daemon is already running.\r\n");
	fprintf (stderr, "   -r          Reset shared memory area.\r\n");
#ifdef DISC_LISTEN
	fprintf (stderr, "   -d          Display discovery info.\r\n");
#endif
#ifdef DDS_SECURITY
	fprintf (stderr, "   -e <name>   Pass the name of the engine.\r\n");
	fprintf (stderr, "   -c <path>   Path of the certificate to use.\r\n");
	fprintf (stderr, "   -k <path>   Path of the private key to use.\r\n");
	fprintf (stderr, "   -z <realm>  The realm name.\r\n");
#endif
	fprintf (stderr, "   -D          Don't daemonize.\r\n");
	fprintf (stderr, "   -p <name>   Pipe to notify interested parties when fully started\r\n");
	fprintf (stderr, "   -v          Verbose: log overall functionality\r\n");
	fprintf (stderr, "   -vv         Extra verbose: log detailed functionality.\r\n");
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

int do_switches (int argc, char **argv)
{
	int		i;
	const char	*cp;
#ifdef DDS_SECURITY
	const char      *arg_input;
#endif
	DDS_program_name (&argc, argv);

	progname = argv [0];
	domain_id = DEF_DOMAIN_ID;
	for (i = 1; i < argc; i++) {
		cp = argv [i];
		if (*cp++ != '-')
			break;

		while (*cp) {
			switch (*cp++) {
				case 'i':
					INC_ARG()
					if (!get_num (&cp, &domain_id, 0, MAX_DOMAIN_ID))
						usage ();
					break;
				case 'f':
					force_start = 1;
					break;
				case 'r':
					reset_shm = 1;
					break;
#ifdef DISC_LISTEN
				case 'd':
					trace_disc = 1;
					break;
#endif
				case 'D':
					daemonize = 0;
					break;
				case 'p':
					INC_ARG()
					pipe_name = cp;
					while (*cp)
						cp++;
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

#ifdef DISC_LISTEN

static const char	*names [] = {
	"DCPSParticipant",
	"DCPSTopic",
	"DCPSPublication",
	"DCPSSubscription"
};

void dump_key (DDS_BuiltinTopicKey_t *kp)
{
	d_printf ("%08x:%08x:%08x", ntohl (kp->value [0]), 
			ntohl (kp->value [1]), ntohl (kp->value [2]));
}

#define	RC_BUFSIZE	80

static const char *region_chunk_str (const void *p,
				     unsigned	length)
{
	char			ascii [17], *bp;
	static char		buf [RC_BUFSIZE];
	unsigned		i, left;
	const unsigned char	*dp = (const unsigned char *) p;
	unsigned char		c;

	bp = buf;
	left = RC_BUFSIZE;
	buf [0] = '\t';
	buf [1] = '\0';
	bp = &buf [strlen (buf)];
	left = RC_BUFSIZE - strlen (buf) - 1;
	for (i = 0; i < length; i++) {
		c = *dp++;
		ascii [i] = (c >= ' ' && c <= '~') ? c : '.';
		if (i == 8) {
			snprintf (bp, left, "- ");
			bp += 2;
			left -= 2;
		}
		snprintf (bp, left, "%02x ", c);
		bp += 3;
		left -= 3;
	}
	ascii [i] = '\0';
	while (i < 16) {
		if (i == 8) {
			snprintf (bp, left, "  ");
			bp += 2;
			left -= 2;
		}
		snprintf (bp, left, "   ");
		bp += 3;
		left -= 3;
		i++;
	}
	snprintf (bp, left, "  %s", ascii);
	return (buf);
}

void dump_user_data (DDS_OctetSeq *sp)
{
	unsigned	i;
	unsigned char	*p;

	if (!DDS_SEQ_LENGTH (*sp))
		d_printf ("<none>\r\n");
	else if (DDS_SEQ_LENGTH (*sp) < 10) {
		DDS_SEQ_FOREACH_ENTRY (*sp, i, p)
			d_printf ("%02x ", *p);
		d_printf ("\r\n");
	}
	else {
		const unsigned char *dp = DDS_SEQ_ITEM_PTR (*sp, 0);
		d_printf ("\r\n");
		for (i = 0; i < DDS_SEQ_LENGTH (*sp); i += 16) {
			d_printf ("%s\r\n", region_chunk_str (dp, (DDS_SEQ_LENGTH (*sp) < i + 16) ? DDS_SEQ_LENGTH (*sp) - i : 16));
			dp += 16;
		}
	}
}

void display_participant_info (DDS_ParticipantBuiltinTopicData *sample)
{
	d_printf ("\tKey                = ");
	dump_key (&sample->key);
	d_printf ("\r\n\tUser data          = ");
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
				d_printf ("Unable to read Discovered Participant samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				d_printf ("* ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				d_printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					d_printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					d_printf ("Updated");
				else
					d_printf ("Deleted");
				d_printf (" Participant\r\n");
				if (info->valid_data)
					display_participant_info (sample);
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*d_printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

void dump_duration (DDS_Duration_t *dp)
{
	if (dp->sec == DDS_DURATION_ZERO_SEC &&
	    dp->nanosec == DDS_DURATION_ZERO_NSEC)
		d_printf ("0s");
	else if (dp->sec == DDS_DURATION_INFINITE_SEC &&
	         dp->nanosec == DDS_DURATION_INFINITE_NSEC)
		d_printf ("<infinite>");
	else
		d_printf ("%d.%09us", dp->sec, dp->nanosec);
}

void dump_durability (DDS_DurabilityQosPolicy *dp)
{
	static const char *durability_str [] = {
		"Volatile", "Transient-local", "Transient", "Persistent"
	};

	if (dp->kind <= DDS_PERSISTENT_DURABILITY_QOS)
		d_printf ("%s", durability_str [dp->kind]);
	else
		d_printf ("?(%d)", dp->kind);
}

void dump_history (DDS_HistoryQosPolicyKind k, int depth)
{
	if (k == DDS_KEEP_ALL_HISTORY_QOS)
		d_printf ("All");
	else
		d_printf ("Last %d", depth);
}

void dump_resource_limits (int max_samples, int max_inst, int max_samples_per_inst)
{
	d_printf ("max_samples/instances/samples_per_inst=%d/%d/%d",
			max_samples, max_inst, max_samples_per_inst);
}

void dump_durability_service (DDS_DurabilityServiceQosPolicy *sp)
{
	d_printf ("\r\n\t     Cleanup Delay = ");
	dump_duration (&sp->service_cleanup_delay);
	d_printf ("\r\n\t     History       = ");
	dump_history (sp->history_kind, sp->history_depth);
	d_printf ("\r\n\t     Limits        = ");
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
		d_printf ("%s", liveness_str [lp->kind]);
	else
		d_printf ("?(%d)", lp->kind);
	d_printf (", Lease duration: ");
	dump_duration (&lp->lease_duration);
}

void dump_reliability (DDS_ReliabilityQosPolicy *rp)
{
	if (rp->kind == DDS_BEST_EFFORT_RELIABILITY_QOS)
		d_printf ("Best-effort");
	else if (rp->kind == DDS_RELIABLE_RELIABILITY_QOS)
		d_printf ("Reliable");
	else
		d_printf ("?(%d)", rp->kind);
	d_printf (", Max_blocking_time: ");
	dump_duration (&rp->max_blocking_time);
}

void dump_destination_order (DDS_DestinationOrderQosPolicyKind k)
{
	if (k == DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS)
		d_printf ("Reception_Timestamp");
	else if (k == DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS)
		d_printf ("Source_Timestamp");
	else
		d_printf ("?(%d)", k);
}

void dump_ownership (DDS_OwnershipQosPolicyKind k)
{
	if (k == DDS_SHARED_OWNERSHIP_QOS)
		d_printf ("Shared");
	else if (k == DDS_EXCLUSIVE_OWNERSHIP_QOS)
		d_printf ("Exclusive");
	else
		d_printf ("?(%d)", k);
}

void display_topic_info (DDS_TopicBuiltinTopicData *sample)
{
	d_printf ("\tKey                = ");
	dump_key (&sample->key);
	d_printf ("\r\n\tName               = %s", sample->name);
	d_printf ("\r\n\tType Name          = %s", sample->type_name);
	d_printf ("\r\n\tDurability         = ");
	dump_durability (&sample->durability);
	d_printf ("\r\n\tDurability Service:");
	dump_durability_service (&sample->durability_service);
	d_printf ("\r\n\tDeadline           = ");
	dump_duration (&sample->deadline.period);
	d_printf ("\r\n\tLatency Budget     = ");
	dump_duration (&sample->latency_budget.duration);
	d_printf ("\r\n\tLiveliness         = ");
	dump_liveliness (&sample->liveliness);
	d_printf ("\r\n\tReliability        = ");
	dump_reliability (&sample->reliability);
	d_printf ("\r\n\tTransport Priority = %d", sample->transport_priority.value);
	d_printf ("\r\n\tLifespan           = ");
	dump_duration (&sample->lifespan.duration);
	d_printf ("\r\n\tDestination Order  = ");
	dump_destination_order (sample->destination_order.kind);
	d_printf ("\r\n\tHistory            = ");
	dump_history (sample->history.kind, sample->history.depth);
	d_printf ("\r\n\tResource Limits    = ");
	dump_resource_limits (sample->resource_limits.max_samples,
			      sample->resource_limits.max_instances,
			      sample->resource_limits.max_samples_per_instance);
	d_printf ("\r\n\tOwnership          = ");
	dump_ownership (sample->ownership.kind);
	d_printf ("\r\n\tTopic Data         = ");
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

	/*d_printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				d_printf ("Unable to read Discovered Topic samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				d_printf ("* ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				d_printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					d_printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					d_printf ("Updated");
				else
					d_printf ("Deleted");
				d_printf (" Topic");
				if (info->valid_data)
					d_printf (" (%s/%s)", sample->name, sample->type_name);
				d_printf ("\r\n");
				if (info->valid_data)
					display_topic_info (sample);
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*d_printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

void dump_presentation (DDS_PresentationQosPolicy *pp)
{
	static const char *pres_str [] = {
		"Instance", "Topic", "Group"
	};

	d_printf ("Scope: ");
	if (pp->access_scope <= DDS_GROUP_PRESENTATION_QOS)
		d_printf ("%s", pres_str [pp->access_scope]);
	else
		d_printf ("?(%d)", pp->access_scope);
	d_printf (", coherent: %d, ordered: %d", pp->coherent_access, pp->ordered_access);
}

void dump_partition (DDS_PartitionQosPolicy *pp)
{
	unsigned	i;
	char		**cp;

	if (!DDS_SEQ_LENGTH (pp->name)) {
		d_printf ("<none>");
		return;
	}
	DDS_SEQ_FOREACH_ENTRY (pp->name, i, cp) {
		if (i)
			d_printf (", ");
		d_printf ("%s", *cp);
	}
}

void display_publication_info (DDS_PublicationBuiltinTopicData *sample)
{
	d_printf ("\tKey                = ");
	dump_key (&sample->key);
	d_printf ("\r\n\tParticipant Key    = ");
	dump_key (&sample->participant_key);
	d_printf ("\r\n\tTopic Name         = %s", sample->topic_name);
	d_printf ("\r\n\tType Name          = %s", sample->type_name);
	d_printf ("\r\n\tDurability         = ");
	dump_durability (&sample->durability);
	d_printf ("\r\n\tDurability Service:");
	dump_durability_service (&sample->durability_service);
	d_printf ("\r\n\tDeadline           = ");
	dump_duration (&sample->deadline.period);
	d_printf ("\r\n\tLatency Budget     = ");
	dump_duration (&sample->latency_budget.duration);
	d_printf ("\r\n\tLiveliness         = ");
	dump_liveliness (&sample->liveliness);
	d_printf ("\r\n\tReliability        = ");
	dump_reliability (&sample->reliability);
	d_printf ("\r\n\tLifespan           = ");
	dump_duration (&sample->lifespan.duration);
	d_printf ("\r\n\tUser Data          = ");
	dump_user_data (&sample->user_data.value);
	d_printf ("\tOwnership          = ");
	dump_ownership (sample->ownership.kind);
	d_printf ("\r\n\tOwnership strength = %d",
			sample->ownership_strength.value);
	d_printf ("\r\n\tDestination Order  = ");
	dump_destination_order (sample->destination_order.kind);
	d_printf ("\r\n\tPresentation       = ");
	dump_presentation (&sample->presentation);
	d_printf ("\r\n\tPartition          = ");
	dump_partition (&sample->partition);
	d_printf ("\r\n\tTopic Data         = ");
	dump_user_data (&sample->topic_data.value);
	d_printf ("\tGroup Data         = ");
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

	/*d_printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				d_printf ("Unable to read Discovered Publication samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				d_printf ("* ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				d_printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					d_printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					d_printf ("Updated");
				else
					d_printf ("Deleted");
				d_printf (" Publication");
				if (info->valid_data)
					d_printf (" (%s/%s)", sample->topic_name, sample->type_name);
				d_printf ("\r\n");
				if (info->valid_data)
					display_publication_info (sample);
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*d_printf ("do_read: all read!\r\n");*/
			return;
		}
	}
}

void display_subscription_info (DDS_SubscriptionBuiltinTopicData *sample)
{
	d_printf ("\tKey                = ");
	dump_key (&sample->key);
	d_printf ("\r\n\tParticipant Key    = ");
	dump_key (&sample->participant_key);
	d_printf ("\r\n\tTopic Name         = %s", sample->topic_name);
	d_printf ("\r\n\tType Name          = %s", sample->type_name);
	d_printf ("\r\n\tDurability         = ");
	dump_durability (&sample->durability);
	d_printf ("\r\n\tDeadline           = ");
	dump_duration (&sample->deadline.period);
	d_printf ("\r\n\tLatency Budget     = ");
	dump_duration (&sample->latency_budget.duration);
	d_printf ("\r\n\tLiveliness         = ");
	dump_liveliness (&sample->liveliness);
	d_printf ("\r\n\tReliability        = ");
	dump_reliability (&sample->reliability);
	d_printf ("\r\n\tOwnership          = ");
	dump_ownership (sample->ownership.kind);
	d_printf ("\r\n\tDestination Order  = ");
	dump_destination_order (sample->destination_order.kind);
	d_printf ("\r\n\tUser Data          = ");
	dump_user_data (&sample->user_data.value);
	d_printf ("\tTime based filter  = ");
	dump_duration (&sample->time_based_filter.minimum_separation);
	d_printf ("\r\n\tPresentation       = ");
	dump_presentation (&sample->presentation);
	d_printf ("\r\n\tPartition          = ");
	dump_partition (&sample->partition);
	d_printf ("\r\n\tTopic Data         = ");
	dump_user_data (&sample->topic_data.value);
	d_printf ("\tGroup Data         = ");
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

	/*d_printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				d_printf ("Unable to read Discovered Subscription samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (verbose) {
				d_printf ("* ");
				if (info->valid_data)
					dump_key (&sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (&tmp.key);
				}
				d_printf ("  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					d_printf ("New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					d_printf ("Updated");
				else
					d_printf ("Deleted");
				d_printf (" Subscription");
				if (info->valid_data)
					d_printf (" (%s/%s)", sample->topic_name, sample->type_name);
				d_printf ("\r\n");
				if (info->valid_data)
					display_subscription_info (sample);
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
		}
		else {
			/*d_printf ("do_read: all read!\r\n");*/
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
		f_printf ("DDS_DomainParticipant_get_builtin_subscriber() returned an error!\r\n");

	if (verbose)
		d_printf ("DDS Builtin Subscriber found.\r\n");

	for (i = 0; i < sizeof (names) / sizeof (char *); i++) {
		dr = DDS_Subscriber_lookup_datareader (sub, names [i]);
		if (!dr)
			f_printf ("DDS_Subscriber_lookup_datareader (%s) returned an error!\r\n", names [i]);

		ret = DDS_DataReader_set_listener (dr, &builtin_listeners [i], DDS_DATA_AVAILABLE_STATUS);
		if (ret)
			f_printf ("DDS_DataReader_set_listener(%s) returned an error!\r\n", names [i]);

		if (verbose)
			d_printf ("DDS Discovery Reader created (%s).\r\n", names [i]);
	}
}

#endif


void closedown (int i)
{
	ARG_NOT_USED (i)
#ifdef CLEAN_SHUTDOWN
	int			error;

	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (error)
		f_printf ("DDS_DomainParticipant_delete_contained_entities () failed: error = %d", error);

	if (verbose)
		d_printf ("DDS Entities deleted\r\n");

#if 0
	aborting = 0;
	do {
		DDS_wait (sleep_time);
	}
	while (!aborting);
#endif

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error)
		f_printf ("DDS_DomainParticipantFactory_delete_participant () failed: error = %d", error);

	if (verbose)
		d_printf ("DDS Participant deleted\r\n");

#endif
	DDS_CDD_unregister ();
	exit (0);
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
		fatal_printf ("DDS_SP_set_policy() returned error (%s)!", DDS_error (error));

#ifdef MSECPLUG_WITH_SECXML
	if (DDS_SP_parse_xml ("security.xml"))
		fatal_printf ("SP: no DDS security rules in 'security.xml'!\r\n");
#else
	DDS_SP_add_domain();
	if (!realm_name)
		DDS_SP_add_participant ();
	else 
		DDS_SP_set_participant_access (DDS_SP_add_participant (), strcat(realm_name, "*"), 2, 0);
#endif
	if (!cert_path || !key_path)
		fatal_printf ("Error: you must provide a valid certificate path and a valid private key path\r\n");

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

	error = DDS_Security_set_credentials ("Technicolor CDD Daemon", &credentials);
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

int main (int argc, char **argv)
{
	DDS_DomainParticipant	part;
# if 0
	DDS_PoolConstraints	reqs;
# endif
	FILE 			*fp;
	pid_t			cdd_pid;

	do_switches (argc, argv);


	DDS_CDD_init (daemonize, verbose);
	DDS_execution_environment (DDS_EE_CDD);
# if 0
	DDS_get_default_pool_constraints (&reqs, ~0, 100);
	reqs.max_rx_buffers = 16;
	reqs.min_local_readers = 10;
	reqs.min_local_writers = 8;
	reqs.min_changes = 64;
	reqs.min_instances = 48;
	DDS_set_pool_constraints (&reqs);
# endif
	DDS_entity_name ("Technicolor CDD Daemon");

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		enable_security ();
#endif
#ifdef DDS_DEBUG
	if (!daemonize) {
		DDS_Debug_start ();
		DDS_Debug_abort_enable (&aborting);
	}
#endif
	printf ("TDDS Central Discovery Daemon - (c) Technicolor, 2012\r\n");

	if (daemonize) {
		daemon(0, 0);
		if (pipe_name)
			atexit(pipe_error);
	}

#ifdef TRACE_DISC
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif
	/* Reset shared memory if requested. */
	if (reset_shm) {
		d_printf ("Resetting Technicolor DDS shared memory area.\r\n");
		DDS_CDD_reset_shm ();
	}

	signal (SIGINT, closedown);
	signal (SIGHUP, closedown);
	signal (SIGTERM, closedown);

	DDS_CDD_register (domain_id, getpid (), force_start);

	/* Create a domain participant. */
	part = DDS_DomainParticipantFactory_create_participant (
						domain_id, NULL, NULL, 0);
	if (!part)
		f_printf ("DDS_DomainParticipantFactory_create_participant () failed!\r\n");

	cdd_pid = DDS_CDD_pid ();
	if (cdd_pid == 0)
		f_printf ("Too many Central Discovery daemons running - exiting");

	if (cdd_pid != getpid ())
		f_printf ("Central Discovery already started (pid: %u) - exiting", cdd_pid);

#ifdef TRACE_DISC
	rtps_dtrace_set (0);
#endif
	if (verbose)
		d_printf ("DDS Domain Participant created.\r\n");

#ifdef DISC_LISTEN
	if (trace_disc)
		start_disc_readers (part);
#endif

	if (verbose)
		d_printf ("Announcing Central Discovery presence for domain %u\r\n", domain_id);

	if (pipe_name) {
		fp = fopen (pipe_name, "w");
		if (fp != NULL) {
			fprintf (fp, "%d", cdd_pid);
			fclose (fp);
		}
	}
	for (; !aborting; )
		DDS_wait (1000);

	closedown (0);

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		cleanup_security ();
#endif
	return (0);
}

