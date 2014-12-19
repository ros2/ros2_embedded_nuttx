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

#include <arpa/inet.h>
#include "test.h"
#include "ta_part.h"
#include "ta_disc.h"

const char *builtin_names [] = {
	"DCPSParticipant",
	"DCPSTopic",
	"DCPSPublication",
	"DCPSSubscription"
};
DDS_InstanceHandle_t last_d0 [4];

static void dump_key (DDS_BuiltinTopicKey_t *kp)
{
	printf ("%08x:%08x:%08x", ntohl (kp->value [0]), 
			ntohl (kp->value [1]), ntohl (kp->value [2]));
}

#define	RC_BUFSIZE	80

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

static void dump_region (const void *dstp, unsigned length)
{
	unsigned		i;
	const unsigned char	*dp = (const unsigned char *) dstp;

	for (i = 0; i < length; i += 16) {
		printf ("%s\r\n",
			region_chunk_str (dp,
			   		  (length < i + 16) ? length - i : 16,
			   		  0, NULL));
		dp += 16;
	}
}

static void dump_user_data (DDS_OctetSeq *sp)
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
		dump_region (DDS_SEQ_ITEM_PTR (*sp, 0), DDS_SEQ_LENGTH (*sp));
	}
}

static void display_participant_info (DDS_ParticipantBuiltinTopicData *sample)
{
	printf ("\tKey                = ");
	dump_key (&sample->key);
	printf ("\r\n\tUser data          = ");
	dump_user_data (&sample->user_data.value);
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
			if (!l->cookie)
				last_d0 [Participant] = info->instance_handle;
			if (verbose) {
				printf ("(%lu) * ", (unsigned long)(uintptr_t) l->cookie);
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

static void dump_duration (DDS_Duration_t *dp)
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

static void dump_durability (DDS_DurabilityQosPolicy *dp)
{
	static const char *durability_str [] = {
		"Volatile", "Transient-local", "Transient", "Persistent"
	};

	if (dp->kind <= DDS_PERSISTENT_DURABILITY_QOS)
		printf ("%s", durability_str [dp->kind]);
	else
		printf ("?(%d)", dp->kind);
}

static void dump_history (DDS_HistoryQosPolicyKind k, int depth)
{
	if (k == DDS_KEEP_ALL_HISTORY_QOS)
		printf ("All");
	else
		printf ("Last %d", depth);
}

static void dump_resource_limits (int max_samples, int max_inst, int max_samples_per_inst)
{
	printf ("max_samples/instances/samples_per_inst=%d/%d/%d",
			max_samples, max_inst, max_samples_per_inst);
}

static void dump_durability_service (DDS_DurabilityServiceQosPolicy *sp)
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

static void dump_liveliness (DDS_LivelinessQosPolicy *lp)
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

static void dump_reliability (DDS_ReliabilityQosPolicy *rp)
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

static void dump_destination_order (DDS_DestinationOrderQosPolicyKind k)
{
	if (k == DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS)
		printf ("Reception_Timestamp");
	else if (k == DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS)
		printf ("Source_Timestamp");
	else
		printf ("?(%d)", k);
}

static void dump_ownership (DDS_OwnershipQosPolicyKind k)
{
	if (k == DDS_SHARED_OWNERSHIP_QOS)
		printf ("Shared");
	else if (k == DDS_EXCLUSIVE_OWNERSHIP_QOS)
		printf ("Exclusive");
	else
		printf ("?(%d)", k);
}

static void display_topic_info (DDS_TopicBuiltinTopicData *sample)
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

static void topic_info (DDS_DataReaderListener *l,
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
			if (!l->cookie)
				last_d0 [Topic] = info->instance_handle;
			if (verbose) {
				printf ("(%lu) * ", (unsigned long)(uintptr_t) l->cookie);
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

static void dump_presentation (DDS_PresentationQosPolicy *pp)
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

static void dump_partition (DDS_PartitionQosPolicy *pp)
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

static void display_publication_info (DDS_PublicationBuiltinTopicData *sample)
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
			if (!l->cookie)
				last_d0 [Pub] = info->instance_handle;
			if (verbose) {
				printf ("(%lu) * ", (unsigned long)(uintptr_t) l->cookie);
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

static void display_subscription_info (DDS_SubscriptionBuiltinTopicData *sample)
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

static void subscription_info (DDS_DataReaderListener *l,
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
			if (!l->cookie)
				last_d0 [Sub] = info->instance_handle;
			if (verbose) {
				printf ("(%lu) * ", (unsigned long)(uintptr_t) l->cookie);
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

DDS_DataReaderListener_on_data_available builtin_data_avail [] = {
	participant_info,
	topic_info,
	publication_info,
	subscription_info
};

