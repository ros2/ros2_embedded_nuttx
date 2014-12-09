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
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include "thread.h"
#include "libx.h"
#include "tty.h"
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
#include "dds/dds_dcps.h"
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"

#include "imu_msg.h"

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
thread_t		rt2;
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


void do_dds_shell (DDS_DataWriter dw)
{
	Imu_t		m;
	DDS_InstanceHandle_t	h;
	char			buf [256];

	tty_init ();
	DDS_Handle_attach (tty_stdin,
			   POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			   tty_input,
			   NULL);

#if 0
	printf ("Welcome to the Technicolor chatroom.\r\n");
	printf ("Anything you type will be sent to all chatroom attendees.\r\n");
	printf ("Type '!help' for chatroom options.\r\n");
	m.chatroom = chatroom;
	m.from = user_name;
#endif

	h = 0;
	while (!aborting) {
		tty_gets (sizeof (buf), buf, 0, 1);
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
					Imu_signal (dw, h, 1);
					h = 0;
				}
				strcpy (user_name, buf + 6);
				printf ("You are now: %s\r\n", user_name);
			}
			else if (!strcmp (buf + 1, "busy"))
				Imu_signal (dw, h, 0);
			else if (!strcmp (buf + 1, "away")) {
				if (h) {
					Imu_signal (dw, h, 1);
					h = 0;
				}
			}
			else if (!strcmp (buf + 1, "help") ||
				 (buf [1] == 'h' && buf [2] == '\0') ||
				 (buf [1] == '?' && buf [2] == '\0')) {
				printf ("Commands:\r\n");
				printf ("    !list               -> list the attendees.\r\n");
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
		} else {
#if 0			
			if (!h)
				h = Imu_register (dw, &m);
			m.message = buf;
			Imu_write (dw, &m, h);			
#endif
			printf ("Commands:\r\n");
			printf ("    !list               -> list the attendees.\r\n");
			printf ("    !busy               -> momentarily not involved.\r\n");
			printf ("    !away               -> gone away.\r\n");
			printf ("    !help or !h or !?   -> Show this info.\r\n");
			printf ("    !quit or !q         -> Quit the chatroom.\r\n");
			printf ("    !!<command>         -> DDS debug command.\r\n");			
		}
	}
}

static void *dds_send_imu (void *args)
{
	Imu_t				m;
	DDS_InstanceHandle_t	h;
	int i = 0;

	h = 0;
	for (;;){
		sleep (1); // sleep 0.5 seconds

		m.orientation.x = 10;
		m.orientation.y = 10;
		m.orientation.z = 10;
		m.orientation.w = 10;

		m.orientation_covariance[0] = 0;
		m.orientation_covariance[1] = 1;
		m.orientation_covariance[2] = 2;
		m.orientation_covariance[3] = 3;
		m.orientation_covariance[4] = 4;
		m.orientation_covariance[5] = 5;
		m.orientation_covariance[6] = 6;
		m.orientation_covariance[7] = 7;
		m.orientation_covariance[8] = 8;

		m.linear_acceleration.x_ = 1 + i;
		m.linear_acceleration.y_ = 2 + i;
		m.linear_acceleration.z_ = 3 + i;
#if 0
		/* According to https://github.com/brunodebus/tinq-core/issues/7#issuecomment-63740498:
			the Imu shouldn't be registered if it doesn't contain a @key attribute
		*/		
		if (!h)
			h = Imu_register (dw, &m);
#endif
		Imu_write (dw, &m, h);
		printf("IMU message sent\n");
		i++;
	}
}

void read_msg (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	Imu_t		msg;
	DDS_InstanceStateKind	kind;
	int			valid;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (l)

	memset (&msg, 0, sizeof (msg));
	ret = Imu_read_or_take (dr, &msg, DDS_NOT_READ_SAMPLE_STATE, 
					      DDS_ANY_VIEW_STATE,
					      DDS_ANY_INSTANCE_STATE, 1,
					      &valid, &kind);
	if (ret == DDS_RETCODE_OK)
		do {
#if 0
/*  Use a from field within the message to avoid to read the messages that this
	node sent.
*/
#ifndef DISPLAY_SELF
			if (!strcmp (msg.from, user_name) &&
			    !strcmp (msg.chatroom, chatroom))
				break;
#endif
#endif
			if (valid){
				printf ("Imu ROS message received:\n");

				printf("	header.seq=%d\n", msg.header.seq);
				printf("	header.stamp.sec=%d\n", msg.header.stamp.sec);			
				printf("	header.stamp.nanosec=%d\n", msg.header.stamp.nanosec);			
				printf("	header.frame_id=%d\n", msg.header.frame_id);
				printf(" 	orientation.x=%f\n", msg.orientation.x);			
				printf(" 	orientation.y=%f\n", msg.orientation.y);			
				printf(" 	orientation.z=%f\n", msg.orientation.z);			
				printf(" 	orientation.w=%f\n", msg.orientation.w);			
				printf(" 	orientation_covariance=%f %f %f %f %f %f %f %f %f\n", msg.orientation_covariance[0], 
					msg.orientation_covariance[1], msg.orientation_covariance[2], msg.orientation_covariance[3],
					msg.orientation_covariance[4], msg.orientation_covariance[5], msg.orientation_covariance[6],
					msg.orientation_covariance[7], msg.orientation_covariance[8]);
				printf("    angular_velocity_x=%f\n", msg.angular_velocity.x_);
				printf("    angular_velocity_y=%f\n", msg.angular_velocity.y_);
				printf("    angular_velocity_z=%f\n", msg.angular_velocity.z_);
				printf(" 	angular_velocity_covariance=%f %f %f %f %f %f %f %f %f\n", msg.angular_velocity_covariance[0], 
					msg.angular_velocity_covariance[1], msg.angular_velocity_covariance[2], msg.angular_velocity_covariance[3],
					msg.angular_velocity_covariance[4], msg.angular_velocity_covariance[5], msg.angular_velocity_covariance[6],
					msg.angular_velocity_covariance[7], msg.angular_velocity_covariance[8]);
				printf("    linear_acceleration_x=%f\n", msg.linear_acceleration.x_);
				printf("    linear_acceleration_y=%f\n", msg.linear_acceleration.y_);
				printf("    linear_acceleration_z=%f\n", msg.linear_acceleration.z_);
				printf(" 	linear_acceleration_covariance=%f %f %f %f %f %f %f %f %f\n", msg.linear_acceleration_covariance[0], 
					msg.linear_acceleration_covariance[1], msg.linear_acceleration_covariance[2], msg.linear_acceleration_covariance[3],
					msg.linear_acceleration_covariance[4], msg.linear_acceleration_covariance[5], msg.linear_acceleration_covariance[6],
					msg.linear_acceleration_covariance[7], msg.linear_acceleration_covariance[8]);

			}
			else if (kind == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
				printf ("DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE!\r\n");
			else if (kind == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
				printf ("DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE!\r\n");
		}
		while (0);

	Imu_cleanup (&msg);
}

#ifdef WAITSETS

static void *imu_reader (void *args)
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

static void start_imu_reader (DDS_DynamicDataReader dr)
{
	thread_create (rt, imu_reader, dr);
}

static void stop_imu_reader (DDS_DynamicDataReader dr)
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

int main (int argc, const char **argv)
{
	DDS_DataWriterQos 	wr_qos;
	DDS_DataReaderQos	rd_qos;
	DDS_ReturnCode_t	error;

	sprintf (user_name, ".pid.%u", getpid ());
	DDS_entity_name ("Technicolor Chatroom");

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

	ts = Imu_type_new ();
	if (!ts) {
		printf ("Can't create Imu message type!\r\n");
		exit (1);
	}
	error = DDS_DynamicTypeSupport_register_type (ts, part, "simple_msgs::dds_::Imu_");
	if (error) {
		printf ("Can't register Imu message type.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "simple_msgs::dds_::Imu_");

	topic = DDS_DomainParticipant_create_topic (part, "imu", "simple_msgs::dds_::Imu_", NULL, NULL, 0);
	if (!topic) {
		printf ("Can't register Imu message type.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Imu Topic created.\r\n");

	td = DDS_DomainParticipant_lookup_topicdescription (part, "imu");
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
		printf ("Unable to create Imu message writer.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS Imu message writer created.\r\n");

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
		printf ("DDS Vector message reader created.\r\n");

#ifdef WAITSETS
	start_imu_reader (dr);
#endif

	// publisher thread
	thread_create (rt2, dds_send_imu, dr);	

	// DDS Debug shell thread
	do_dds_shell (dw);

#ifdef WAITSETS
	stop_imu_reader (dr);
#endif
	DDS_Publisher_delete_datawriter (pub, dw);
	usleep (200000);
	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (verbose)
		printf ("DDS Entities deleted (error = %u).\r\n", error);

	Imu_type_free (ts);
	if (verbose)
		printf ("Imu Type deleted.\r\n");

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (verbose)
		printf ("DDS Participant deleted (error = %u).\r\n", error);

#ifdef DDS_SECURITY
	if (cert_path || key_path || engine_id)
		cleanup_security ();
#endif
	return (0);
}

