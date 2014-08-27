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

/* bi_handler.c -- Implements built-in Discovery data readers that can optionally
                   dump received data automatically, and can also callback to the
		   user each time events occur. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "error.h"
#include "libx.h"
#include "bi_handler.h"

static const char	*names [] = {
	"DCPSParticipant",
	"DCPSTopic",
	"DCPSPublication",
	"DCPSSubscription"
};

#define	RC_BUFSIZE	80

static FILE      *outf;	/* Output file. */
static unsigned	 logm;	/* Logging mask. */
static uintptr_t user;	/* User parameter. */

static DDS_Participant_notify	pnotify;
static DDS_Topic_notify		tnotify;
static DDS_Publication_notify	wnotify;
static DDS_Subscription_notify	rnotify;

static const char *region_chunk_str (const void *p,
			             unsigned   length,
			             int        show_ofs,
			             const void *sp)
{
	char			ascii [17], *bp;
	static char		buf [RC_BUFSIZE];
	unsigned		i, left;
	const unsigned char	*dp = (const unsigned char *) p;
	unsigned char		c;

	bp = buf;
	left = RC_BUFSIZE;
	if (show_ofs)
		if (sp)
			snprintf (bp, left, "  %4ld: ", (long) (dp - (const unsigned char *) sp));
		else
			snprintf (bp, left, "  %p: ", p);
	else {
		buf [0] = '\t';
		buf [1] = '\0';
	}
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

static void print_region (FILE       *f,
			  const void *dstp,
			  unsigned   length,
			  int        show_addr,
			  int        ofs)
{
	unsigned		i;
	const unsigned char	*dp = (const unsigned char *) dstp;

	for (i = 0; i < length; i += 16) {
		fprintf (f, "%s\r\n", region_chunk_str (dp,
				(length < i + 16) ? length - i: 16, show_addr,
				(ofs) ? dstp : NULL));
		dp += 16;
	}
}

static void dump_key (FILE *f, DDS_BuiltinTopicKey_t *kp)
{
	fprintf (f, "%08x:%08x:%08x", ntohl (kp->value [0]), 
			ntohl (kp->value [1]), ntohl (kp->value [2]));
}

static void dump_user_data (FILE *f, DDS_OctetSeq *sp)
{
	unsigned	i;
	unsigned char	*p;

	if (!DDS_SEQ_LENGTH (*sp))
		fprintf (f, "<none>\r\n");
	else if (DDS_SEQ_LENGTH (*sp) < 10) {
		DDS_SEQ_FOREACH_ENTRY (*sp, i, p)
			fprintf (f, "%02x ", *p);
		fprintf (f, "\r\n");
	}
	else {
		fprintf (f, "\r\n");
		print_region (f, DDS_SEQ_ITEM_PTR (*sp, 0), DDS_SEQ_LENGTH (*sp), 0, 0);
	}
}

static void display_participant_info (FILE *f, DDS_ParticipantBuiltinTopicData *sample)
{
	fprintf (f, "\tKey                = ");
	dump_key (f, &sample->key);
	fprintf (f, "\r\n\tUser data          = ");
	dump_user_data (f, &sample->user_data.value);
}

static void participant_info (DDS_DataReaderListener *l,
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
	FILE			*f;
	DDS_BuiltinTopicKey_t	*key;

	ARG_NOT_USED (l)

	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				fprintf (stderr, "Unable to read Discovered Participant samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if ((logm & BI_PARTICIPANT_M) != 0) {
				f = outf;
				fprintf (f, "* ");
				if (info->valid_data) {
					dump_key (f, &sample->key);
					key = &sample->key;
				}
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (f, &tmp.key);
					key = &tmp.key;
				}
				fprintf (f, "  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					fprintf (f, "New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					fprintf (f, "Updated");
				else
					fprintf (f, "Deleted");
				fprintf (f, " Participant\r\n");
				if (info->valid_data)
					display_participant_info (f, sample);
			}
			if (pnotify)
				(*pnotify) (key, sample, info, user);

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

static void dump_duration (FILE *f, DDS_Duration_t *dp)
{
	if (dp->sec == DDS_DURATION_ZERO_SEC &&
	    dp->nanosec == DDS_DURATION_ZERO_NSEC)
		fprintf (f, "0s");
	else if (dp->sec == DDS_DURATION_INFINITE_SEC &&
	         dp->nanosec == DDS_DURATION_INFINITE_NSEC)
		fprintf (f, "<infinite>");
	else
		fprintf (f, "%d.%09us", dp->sec, dp->nanosec);
}

static void dump_durability (FILE *f, DDS_DurabilityQosPolicy *dp)
{
	static const char *durability_str [] = {
		"Volatile", "Transient-local", "Transient", "Persistent"
	};

	if (dp->kind <= DDS_PERSISTENT_DURABILITY_QOS)
		fprintf (f, "%s", durability_str [dp->kind]);
	else
		fprintf (f, "?(%d)", dp->kind);
}

static void dump_history (FILE *f, DDS_HistoryQosPolicyKind k, int depth)
{
	if (k == DDS_KEEP_ALL_HISTORY_QOS)
		fprintf (f, "All");
	else
		fprintf (f, "Last %d", depth);
}

static void dump_resource_limits (FILE *f,
				  int max_samples,
				  int max_inst,
				  int max_samples_per_inst)
{
	fprintf (f, "max_samples/instances/samples_per_inst=%d/%d/%d",
			max_samples, max_inst, max_samples_per_inst);
}

static void dump_durability_service (FILE *f, DDS_DurabilityServiceQosPolicy *sp)
{
	fprintf (f, "\r\n\t     Cleanup Delay = ");
	dump_duration (f, &sp->service_cleanup_delay);
	fprintf (f, "\r\n\t     History       = ");
	dump_history (f, sp->history_kind, sp->history_depth);
	fprintf (f, "\r\n\t     Limits        = ");
	dump_resource_limits (f,
			      sp->max_samples,
			      sp->max_instances,
			      sp->max_samples_per_instance);
}

static void dump_liveliness (FILE *f, DDS_LivelinessQosPolicy *lp)
{
	static const char *liveness_str [] = {
		"Automatic", "Manual_by_Participant", "Manual_by_Topic"
	};

	if (lp->kind <= DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS)
		fprintf (f, "%s", liveness_str [lp->kind]);
	else
		fprintf (f, "?(%d)", lp->kind);
	fprintf (f, ", Lease duration: ");
	dump_duration (f, &lp->lease_duration);
}

static void dump_reliability (FILE *f, DDS_ReliabilityQosPolicy *rp)
{
	if (rp->kind == DDS_BEST_EFFORT_RELIABILITY_QOS)
		fprintf (f, "Best-effort");
	else if (rp->kind == DDS_RELIABLE_RELIABILITY_QOS)
		fprintf (f, "Reliable");
	else
		fprintf (f, "?(%d)", rp->kind);
	fprintf (f, ", Max_blocking_time: ");
	dump_duration (f, &rp->max_blocking_time);
}

static void dump_destination_order (FILE *f, DDS_DestinationOrderQosPolicyKind k)
{
	if (k == DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS)
		fprintf (f, "Reception_Timestamp");
	else if (k == DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS)
		fprintf (f, "Source_Timestamp");
	else
		fprintf (f, "?(%d)", k);
}

static void dump_ownership (FILE *f, DDS_OwnershipQosPolicyKind k)
{
	if (k == DDS_SHARED_OWNERSHIP_QOS)
		fprintf (f, "Shared");
	else if (k == DDS_EXCLUSIVE_OWNERSHIP_QOS)
		fprintf (f, "Exclusive");
	else
		fprintf (f, "?(%d)", k);
}

static void display_topic_info (FILE *f, DDS_TopicBuiltinTopicData *sample)
{
	fprintf (f, "\tKey                = ");
	dump_key (f, &sample->key);
	fprintf (f, "\r\n\tName               = %s", sample->name);
	fprintf (f, "\r\n\tType Name          = %s", sample->type_name);
	fprintf (f, "\r\n\tDurability         = ");
	dump_durability (f, &sample->durability);
	fprintf (f, "\r\n\tDurability Service:");
	dump_durability_service (f, &sample->durability_service);
	fprintf (f, "\r\n\tDeadline           = ");
	dump_duration (f, &sample->deadline.period);
	fprintf (f, "\r\n\tLatency Budget     = ");
	dump_duration (f, &sample->latency_budget.duration);
	fprintf (f, "\r\n\tLiveliness         = ");
	dump_liveliness (f, &sample->liveliness);
	fprintf (f, "\r\n\tReliability        = ");
	dump_reliability (f, &sample->reliability);
	fprintf (f, "\r\n\tTransport Priority = %d", sample->transport_priority.value);
	fprintf (f, "\r\n\tLifespan           = ");
	dump_duration (f, &sample->lifespan.duration);
	fprintf (f, "\r\n\tDestination Order  = ");
	dump_destination_order (f, sample->destination_order.kind);
	fprintf (f, "\r\n\tHistory            = ");
	dump_history (f, sample->history.kind, sample->history.depth);
	fprintf (f, "\r\n\tResource Limits    = ");
	dump_resource_limits (f, sample->resource_limits.max_samples,
			      sample->resource_limits.max_instances,
			      sample->resource_limits.max_samples_per_instance);
	fprintf (f, "\r\n\tOwnership          = ");
	dump_ownership (f, sample->ownership.kind);
	fprintf (f, "\r\n\tTopic Data         = ");
	dump_user_data (f, &sample->topic_data.value);
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
	FILE			*f;

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
			if ((logm & BI_TOPIC_M) != 0) {
				f = outf;
				fprintf (f, "* ");
				if (info->valid_data)
					dump_key (f, &sample->key);
				else {
					DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
					dump_key (f, &tmp.key);
				}
				fprintf (f, "  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					fprintf (f, "New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					fprintf (f, "Updated");
				else
					fprintf (f, "Deleted");
				fprintf (f, " Topic");
				if (info->valid_data)
					fprintf (f, " (%s/%s)", sample->name, sample->type_name);
				fprintf (f, "\r\n");
				if (info->valid_data)
					display_topic_info (f, sample);
			}
			if (tnotify)
				(*tnotify) (sample, info, user);

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

static void dump_presentation (FILE *f, DDS_PresentationQosPolicy *pp)
{
	static const char *pres_str [] = {
		"Instance", "Topic", "Group"
	};

	fprintf (f, "Scope: ");
	if (pp->access_scope <= DDS_GROUP_PRESENTATION_QOS)
		fprintf (f, "%s", pres_str [pp->access_scope]);
	else
		fprintf (f, "?(%d)", pp->access_scope);
	fprintf (f, ", coherent: %d, ordered: %d", pp->coherent_access, pp->ordered_access);
}

static void dump_partition (FILE *f, DDS_PartitionQosPolicy *pp)
{
	unsigned	i;
	char		**cp;

	if (!DDS_SEQ_LENGTH (pp->name)) {
		fprintf (f, "<none>");
		return;
	}
	DDS_SEQ_FOREACH_ENTRY (pp->name, i, cp) {
		if (i)
			fprintf (f, ", ");
		fprintf (f, "%s", *cp);
	}
}

static void display_publication_info (FILE *f, DDS_PublicationBuiltinTopicData *sample)
{
	fprintf (f, "\tKey                = ");
	dump_key (f, &sample->key);
	fprintf (f, "\r\n\tParticipant Key    = ");
	dump_key (f, &sample->participant_key);
	fprintf (f, "\r\n\tTopic Name         = %s", sample->topic_name);
	fprintf (f, "\r\n\tType Name          = %s", sample->type_name);
	fprintf (f, "\r\n\tDurability         = ");
	dump_durability (f, &sample->durability);
	fprintf (f, "\r\n\tDurability Service:");
	dump_durability_service (f, &sample->durability_service);
	fprintf (f, "\r\n\tDeadline           = ");
	dump_duration (f, &sample->deadline.period);
	fprintf (f, "\r\n\tLatency Budget     = ");
	dump_duration (f, &sample->latency_budget.duration);
	fprintf (f, "\r\n\tLiveliness         = ");
	dump_liveliness (f, &sample->liveliness);
	fprintf (f, "\r\n\tReliability        = ");
	dump_reliability (f, &sample->reliability);
	fprintf (f, "\r\n\tLifespan           = ");
	dump_duration (f, &sample->lifespan.duration);
	fprintf (f, "\r\n\tUser Data          = ");
	dump_user_data (f, &sample->user_data.value);
	fprintf (f, "\tOwnership          = ");
	dump_ownership (f, sample->ownership.kind);
	fprintf (f, "\r\n\tOwnership strength = %d",
			sample->ownership_strength.value);
	fprintf (f, "\r\n\tDestination Order  = ");
	dump_destination_order (f, sample->destination_order.kind);
	fprintf (f, "\r\n\tPresentation       = ");
	dump_presentation (f, &sample->presentation);
	fprintf (f, "\r\n\tPartition          = ");
	dump_partition (f, &sample->partition);
	fprintf (f, "\r\n\tTopic Data         = ");
	dump_user_data (f, &sample->topic_data.value);
	fprintf (f, "\tGroup Data         = ");
	dump_user_data (f, &sample->group_data.value);
}

static void publication_info (DDS_DataReaderListener *l,
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
	FILE			*f;
	DDS_BuiltinTopicKey_t	*key;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				fprintf (stderr, "Unable to read Discovered Publication samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (info->valid_data)
				key = &sample->key;
			else {
				DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
				key = &tmp.key;
			}
			if ((logm & BI_PUBLICATION_M) != 0) {
				f = outf;
				fprintf (f, "* ");
				dump_key (f, key);
				fprintf (f, "  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					fprintf (f, "New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					fprintf (f, "Updated");
				else
					fprintf (f, "Deleted");
				fprintf (f, " Publication");
				if (info->valid_data)
					fprintf (f, " (%s/%s)", sample->topic_name, sample->type_name);
				fprintf (f, "\r\n");
				if (info->valid_data)
					display_publication_info (f, sample);
			}
			if (wnotify)
				(*wnotify) (key, sample, info, user);

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

static void display_subscription_info (FILE *f, DDS_SubscriptionBuiltinTopicData *sample)
{
	fprintf (f, "\tKey                = ");
	dump_key (f, &sample->key);
	fprintf (f, "\r\n\tParticipant Key    = ");
	dump_key (f, &sample->participant_key);
	fprintf (f, "\r\n\tTopic Name         = %s", sample->topic_name);
	fprintf (f, "\r\n\tType Name          = %s", sample->type_name);
	fprintf (f, "\r\n\tDurability         = ");
	dump_durability (f, &sample->durability);
	fprintf (f, "\r\n\tDeadline           = ");
	dump_duration (f, &sample->deadline.period);
	fprintf (f, "\r\n\tLatency Budget     = ");
	dump_duration (f, &sample->latency_budget.duration);
	fprintf (f, "\r\n\tLiveliness         = ");
	dump_liveliness (f, &sample->liveliness);
	fprintf (f, "\r\n\tReliability        = ");
	dump_reliability (f, &sample->reliability);
	fprintf (f, "\r\n\tOwnership          = ");
	dump_ownership (f, sample->ownership.kind);
	fprintf (f, "\r\n\tDestination Order  = ");
	dump_destination_order (f, sample->destination_order.kind);
	fprintf (f, "\r\n\tUser Data          = ");
	dump_user_data (f, &sample->user_data.value);
	fprintf (f, "\tTime based filter  = ");
	dump_duration (f, &sample->time_based_filter.minimum_separation);
	fprintf (f, "\r\n\tPresentation       = ");
	dump_presentation (f, &sample->presentation);
	fprintf (f, "\r\n\tPartition          = ");
	dump_partition (f, &sample->partition);
	fprintf (f, "\r\n\tTopic Data         = ");
	dump_user_data (f, &sample->topic_data.value);
	fprintf (f, "\tGroup Data         = ");
	dump_user_data (f, &sample->group_data.value);
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
	FILE			*f;
	DDS_BuiltinTopicKey_t	*key;

	ARG_NOT_USED (l)

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				fprintf (stderr, "Unable to read Discovered Subscription samples: error = %u!\r\n", error);
			return;
		}
		if (DDS_SEQ_LENGTH (rx_info)) {
			sample = DDS_SEQ_ITEM (rx_sample, 0);
			info = DDS_SEQ_ITEM (rx_info, 0);
			if (info->valid_data)
				key = &sample->key;
			else {
				DDS_DataReader_get_key_value (dr, &tmp, info->instance_handle);
				key = &tmp.key;
			}
			if ((logm & BI_SUBSCRIPTION_M) != 0) {
				f = outf;
				fprintf (f, "* ");
				dump_key (f, key);
				fprintf (f, "  ");
				if ((info->view_state & DDS_NEW_VIEW_STATE) != 0)
					fprintf (f, "New");
				else if (info->instance_state == DDS_ALIVE_INSTANCE_STATE)
					fprintf (f, "Updated");
				else
					fprintf (f, "Deleted");
				fprintf (f, " Subscription");
				if (info->valid_data)
					fprintf (f, " (%s/%s)", sample->topic_name, sample->type_name);
				fprintf (f, "\r\n");
				if (info->valid_data)
					display_subscription_info (f, sample);
			}
			if (rnotify)
				(*rnotify) (key, sample, info, user);

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

DDS_ReturnCode_t bi_attach (DDS_DomainParticipant   part,
			    unsigned                m,
			    DDS_Participant_notify  pnf,
			    DDS_Topic_notify        tnf,
			    DDS_Publication_notify  wnf,
			    DDS_Subscription_notify rnf,
			    uintptr_t               u)
{
	DDS_Subscriber		sub;
	DDS_DataReader		dr;
	unsigned		i;
	DDS_ReturnCode_t	ret;

	if (outf)
		outf = stdout;

	sub = DDS_DomainParticipant_get_builtin_subscriber (part);
	if (!sub) {
		fprintf (stderr, "DDS_DomainParticipant_get_builtin_subscriber() returned an error!");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	pnotify = pnf;
	tnotify = tnf;
	wnotify = wnf;
	rnotify = rnf;
	user = u;
	for (i = 0; i < sizeof (names) / sizeof (char *); i++) {
		if (((1 << i) & m) != 0) {
			dr = DDS_Subscriber_lookup_datareader (sub, names [i]);
			if (!dr) {
				fprintf (stderr, "DDS_Subscriber_lookup_datareader returned an error!");
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			}
			ret = DDS_DataReader_set_listener (dr, &builtin_listeners [i], DDS_DATA_AVAILABLE_STATUS);
			if (ret) {
				fprintf (stderr, "DDS_DataReader_set_listener returned an error (%s)!", DDS_error (ret));
				return (ret);
			}
		}
	}
	return (DDS_RETCODE_OK);
}

void bi_detach (DDS_DomainParticipant p)
{
	ARG_NOT_USED (p)

	pnotify = NULL;
	tnotify = NULL;
	wnotify = NULL;
	rnotify = NULL;
}

void bi_log (FILE *f, unsigned mask)
{
	outf = f;
	logm = mask;
}


