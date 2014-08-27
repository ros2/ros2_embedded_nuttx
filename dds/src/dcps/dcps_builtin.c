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

/* dcps_builtin.c -- Implements the DCPS builtin reader data access functions.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "sys.h"
#include "log.h"
#include "str.h"
#include "dds/dds_dcps.h"
#include "dds_data.h"
#include "dds.h"
#include "prof.h"
#include "dcps_priv.h"
#include "pid.h"
#include "disc.h"
#include "dcps_builtin.h"

#ifdef DCPS_BUILTIN_READERS

/* create_builtin_reader -- Create a builtin DataReader entity. */

static Endpoint_t *create_builtin_reader (Domain_t       *dp,
					  Builtin_Type_t type,
					  const char     *topic_name,
					  const char     *type_name)
{
	TopicType_t	*typep;
	Topic_t		*tp;
	Reader_t	*rp;
	DDS_TopicQos	qos_data;

	typep = type_create (dp, type_name, NULL);
	if (!typep)
		return (NULL);

	typep->flags = EF_LOCAL | EF_BUILTIN;
	typep->index = type;
	tp = topic_create (&dp->participant, NULL, topic_name, type_name, NULL);
	if (!tp)
		return (NULL);

	tp->entity.flags |= EF_ENABLED | EF_BUILTIN | EF_NOT_IGNORED;
	qos_data = qos_def_topic_qos;
	qos_data.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	qos_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	tp->qos = qos_topic_new (&qos_data);
	if (!tp->qos) {
		topic_delete (&dp->participant, tp, NULL, NULL);
		return (NULL);
	}

	rp = DDS_Subscriber_create_datareader (dp->builtin_subscriber,
					       (DDS_TopicDescription) tp,
					       DDS_DATAREADER_QOS_USE_TOPIC_QOS,
					       NULL, 0);
	if (!rp) {
		topic_delete (&dp->participant, tp, NULL, NULL);
		return (NULL);
	}
	dp->builtin_readers [type] = rp;

#ifdef RTPS_USED
	if (rtps_used)
		disc_populate_builtin (dp, type);
#endif
	return (&rp->r_ep);
}

/* delete_builtin_reader -- Delete the reader for the specific builtin Topic. */

static void delete_builtin_reader (Domain_t       *dp,
				   Builtin_Type_t type,
				   Topic_t        *tp)
{
	/* Remove the cache reference. */
	dp->builtin_readers [type] = NULL;

	/* Delete the builtin datareader. */
	DDS_Subscriber_delete_datareader ((DDS_Subscriber) dp->builtin_subscriber,
					  (DDS_DataReader) tp->readers);

	/* Delete builtin topic. */
	lock_take (tp->lock);
	topic_delete (&dp->participant, tp, NULL, NULL);
}

typedef struct builtin_reader_desc_st {
	Builtin_Type_t	type;
	const char	*topic_name;
	const char	*type_name;
} BuiltinReaderDesc_t;

static const BuiltinReaderDesc_t bi_readers [] = {
	{ BT_Participant,  "DCPSParticipant",  "ParticipantBuiltinTopicData"  },
	{ BT_Topic,        "DCPSTopic",        "TopicBuiltinTopicData"        },
	{ BT_Publication,  "DCPSPublication",  "PublicationBuiltinTopicData"  },
	{ BT_Subscription, "DCPSSubscription", "SubscriptionBuiltinTopicData" }
};

Endpoint_t *dcps_new_builtin_reader (Domain_t *dp, const char *topic_name)
{
	const BuiltinReaderDesc_t	*bip;
	unsigned			i;

	for (i = 0, bip = bi_readers;
	     i < sizeof (bi_readers) / sizeof (BuiltinReaderDesc_t);
	     i++, bip++)
		if (!strcmp (topic_name, bip->topic_name))
			return (create_builtin_reader (dp,
						       bip->type, 
						       bip->topic_name,
						       bip->type_name));
	return (NULL);
}

void dcps_delete_builtin_readers (DDS_DomainParticipant dp)
{
	const BuiltinReaderDesc_t	*bip;
	Topic_t				*tp;
	unsigned			i;

	/* Delete builtin topic readers. */
	for (i = 0, bip = bi_readers;
	     i < sizeof (bi_readers) / sizeof (BuiltinReaderDesc_t);
	     i++, bip++)
		if ((tp = topic_lookup (&dp->participant, bip->topic_name)) != NULL)
			delete_builtin_reader (dp, bip->type, tp);
}

static int oseq_set (DDS_OctetSeq *osp, String_t *sp, unsigned char *bp)
{
	osp->_esize = 1;
	osp->_own = 0;
	if (sp) {
		osp->_maximum = osp->_length = str_len (sp);
		if (bp)
			osp->_buffer = bp;
		else
			osp->_buffer = xmalloc (osp->_length);
		if (!osp->_buffer)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		memcpy (osp->_buffer, str_ptr (sp), osp->_length);
	}
	else {
		osp->_maximum = osp->_length = 0;
		osp->_buffer = NULL;
	}
	return (DDS_RETCODE_OK);
}

static int get_builtin_key (Domain_t       *dp,
			    void           *data,
			    Builtin_Type_t type,
			    DDS_HANDLE     h)
{
	int	error;

	error = hc_get_key (dp->builtin_readers [type]->r_cache, h, data, 0);
	return (error);
}

int dcps_get_builtin_participant_data (DDS_ParticipantBuiltinTopicData *dp,
				       Participant_t                   *pp)
{
	int	error;

	memcpy (dp->key.value, &pp->p_guid_prefix, sizeof (DDS_BuiltinTopicKey_t));
#if 0
	error = get_builtin_key (pp->p_domain,
				 dp->key.value,
				 BT_Participant,
				 entity_handle (pp->p_flags));
	if (error)
		return (error);
#endif
	error = oseq_set (&dp->user_data.value, pp->p_user_data, NULL);
	return (error);
}


#define	ALIGN		sizeof (void *)
#define ROUND_LEN(n)	(n) = (((n) + ALIGN - 1) & ~(ALIGN - 1))
#define ROUND_PTR(p)	(p) = (void *)(((uintptr_t) (p) + ALIGN - 1) & ~(ALIGN - 1))

int dcps_get_builtin_topic_data (DDS_TopicBuiltinTopicData *dp,
				 Topic_t                   *tp,
				 int                       bi_reader)
{
	size_t		size;
	char		*xp;
	UniQos_t	*uqp;
	KeyHash_t	hash;
	int		error;

	size = str_len (tp->name) + 1;
	size += str_len (tp->type->type_name) + 1;
	if (tp->qos->qos.topic_data) {
		ROUND_LEN (size);
		size += str_len (tp->qos->qos.topic_data);
	}
	xp = xmalloc (size);
	if (!xp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	dp->name = xp;
	if (bi_reader) { /* Key already in reader cache! */
		error = get_builtin_key (tp->domain, dp->key.value,
					 BT_Topic, tp->entity.handle);
		if (error)
			goto key_error;
	}
	else { /* (Re)calculate key value. */
		topic_key_from_name (str_ptr (tp->name),
				     str_len (tp->name) - 1,
				     str_ptr (tp->type->type_name),
				     str_len (tp->type->type_name) - 1,
				     &hash);
		memcpy (dp->key.value, &hash, sizeof (DDS_BuiltinTopicKey_t));
	}
	memcpy (xp, str_ptr (tp->name), str_len (tp->name));
	xp += str_len (tp->name);
	dp->type_name = xp;
	memcpy (xp, str_ptr (tp->type->type_name), str_len (tp->type->type_name));
	xp += str_len (tp->type->type_name);
	uqp = &tp->qos->qos;
	dp->durability.kind = uqp->durability_kind;
#ifdef DURABILITY_SERVICE
	dp->durability_service.service_cleanup_delay = uqp->ds_cleanup_delay;
	dp->durability_service.history_kind = uqp->ds_history_kind;
	dp->durability_service.history_depth = uqp->ds_history_depth;
	dp->durability_service.max_samples = uqp->ds_limits.max_samples;
	dp->durability_service.max_instances = uqp->ds_limits.max_instances;
	dp->durability_service.max_samples_per_instance = uqp->ds_limits.max_samples_per_instance;
#else
	dp->durability_service = qos_def_writer_qos.durability_service;
#endif
	dp->deadline = uqp->deadline;
	dp->latency_budget = uqp->latency_budget;
	dp->liveliness.kind = uqp->liveliness_kind;
	dp->liveliness.lease_duration = uqp->liveliness_lease_duration;
	dp->reliability.kind = uqp->reliability_kind;
	dp->reliability.max_blocking_time = uqp->reliability_max_blocking_time;
	dp->transport_priority = uqp->transport_priority;
	dp->lifespan = uqp->lifespan;
	dp->destination_order.kind = uqp->destination_order_kind;
	dp->history.kind = uqp->history_kind;
	dp->history.depth = uqp->history_depth;
	dp->resource_limits = uqp->resource_limits;
	dp->ownership.kind = uqp->ownership_kind;
	if (uqp->topic_data)
		ROUND_PTR (xp);
	oseq_set (&dp->topic_data.value, uqp->topic_data, (unsigned char *) xp);
	return (DDS_RETCODE_OK);

    key_error:
	xfree (dp->name);
	return (error);
}

static size_t partition_size (Strings_t *partition)
{
	size_t		s;
	unsigned	i;
	String_t	*sp;

	if (!partition)
		return (0);

	s = DDS_SEQ_LENGTH (*partition) * sizeof (char *);
	for (i = 0; i < DDS_SEQ_LENGTH (*partition); i++) {
		sp = DDS_SEQ_ITEM (*partition, i);
		if (sp)
			s += str_len (sp) + 1;
		else
			s += 1;
	}
	return (s);
}

static unsigned partition_set (DDS_StringSeq *ssp,
			       Strings_t     *pp,
			       unsigned char *dst)
{
	String_t	*sp;
	unsigned char	*start = dst;
	unsigned	i, len;

	ssp->_esize = sizeof (char *);
	if (pp) {
		ssp->_maximum = ssp->_length = pp->_length;
		ssp->_buffer = (char **) dst;
		dst += pp->_length * sizeof (char *);
		for (i = 0; i < DDS_SEQ_LENGTH (*pp); i++) {
			sp = DDS_SEQ_ITEM (*pp, i);
			if (sp) {
				len = str_len (sp);
				memcpy (dst, str_ptr (sp), len);
			}
			else {
				len = 1;
				*dst = '\0';
			}
			DDS_SEQ_ITEM_SET (*ssp, i, (char *) dst);
			dst += len;
		}
	}
	else
		ssp->_maximum = ssp->_length = 0;
	return (dst - start);
}

int dcps_get_builtin_publication_data (DDS_PublicationBuiltinTopicData *dp,
				       DiscoveredWriter_t              *dwp)
{
	size_t		size, psize;
	char		*xp;
	UniQos_t	*uqp;

	size = str_len (dwp->dw_topic->name);
	size += str_len (dwp->dw_topic->type->type_name);
	if (dwp->dw_qos->qos.user_data) {
		ROUND_LEN (size);
		size += str_len (dwp->dw_qos->qos.user_data);
	}
	if (dwp->dw_qos->qos.partition) {
		ROUND_LEN (size);
		psize = partition_size (dwp->dw_qos->qos.partition);
		size += psize;
	}
	if (dwp->dw_qos->qos.topic_data) {
		ROUND_LEN (size);
		size += str_len (dwp->dw_qos->qos.topic_data);
	}
	if (dwp->dw_qos->qos.group_data) {
		ROUND_LEN (size);
		size += str_len (dwp->dw_qos->qos.group_data);
	}
	xp = xmalloc (size);
	if (!xp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	dp->topic_name = xp;
	dp->key.value [0] = dwp->dw_participant->p_guid_prefix.w [0];
	dp->key.value [1] = dwp->dw_participant->p_guid_prefix.w [1];
	dp->key.value [2] = dwp->dw_entity_id.w;
	memcpy (&dp->participant_key,
		&dwp->dw_participant->p_guid_prefix,
		sizeof (DDS_BuiltinTopicKey_t));
	memcpy (xp, str_ptr (dwp->dw_topic->name),
						str_len (dwp->dw_topic->name));
	xp += str_len (dwp->dw_topic->name);
	dp->type_name = xp;
	memcpy (xp, str_ptr (dwp->dw_topic->type->type_name),
				     str_len (dwp->dw_topic->type->type_name));
	xp += str_len (dwp->dw_topic->type->type_name);
	uqp = &dwp->dw_qos->qos;
	dp->durability.kind = uqp->durability_kind;
#ifdef DURABILITY_SERVICE
	dp->durability_service.service_cleanup_delay = uqp->ds_cleanup_delay;
	dp->durability_service.history_kind = uqp->ds_history_kind;
	dp->durability_service.history_depth = uqp->ds_history_depth;
	dp->durability_service.max_samples = uqp->ds_limits.max_samples;
	dp->durability_service.max_instances = uqp->ds_limits.max_instances;
	dp->durability_service.max_samples_per_instance = uqp->ds_limits.max_samples_per_instance;
#else
	dp->durability_service = qos_def_writer_qos.durability_service;
#endif
	dp->deadline = uqp->deadline;
	dp->latency_budget = uqp->latency_budget;
	dp->liveliness.kind = uqp->liveliness_kind;
	dp->liveliness.lease_duration = uqp->liveliness_lease_duration;
	dp->reliability.kind = uqp->reliability_kind;
	dp->reliability.max_blocking_time = uqp->reliability_max_blocking_time;
	dp->lifespan = uqp->lifespan;
	if (uqp->user_data)
		ROUND_PTR (xp);
	oseq_set (&dp->user_data.value, uqp->user_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->user_data.value);
	dp->ownership.kind = uqp->ownership_kind;
	dp->ownership_strength = uqp->ownership_strength;
	dp->destination_order.kind = uqp->destination_order_kind;
	dp->presentation.access_scope = uqp->presentation_access_scope;
	dp->presentation.coherent_access = uqp->presentation_coherent_access;
	dp->presentation.ordered_access = uqp->presentation_ordered_access;
	if (uqp->partition)
		ROUND_PTR (xp);
	xp += partition_set (&dp->partition.name, uqp->partition, (unsigned char *) xp);
	if (uqp->topic_data)
		ROUND_PTR (xp);
	oseq_set (&dp->topic_data.value, uqp->topic_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->topic_data.value);
	if (uqp->group_data)
		ROUND_PTR (xp);
	oseq_set (&dp->group_data.value, uqp->group_data, (unsigned char *) xp);
	return (DDS_RETCODE_OK);
}

int dcps_get_local_publication_data (DDS_PublicationBuiltinTopicData *dp,
				     Writer_t                        *wp)
{
	size_t		size, psize;
	char		*xp;
	UniQos_t	*uqp;

	size = str_len (wp->w_topic->name);
	size += str_len (wp->w_topic->type->type_name);
	if (wp->w_qos->qos.user_data) {
		ROUND_LEN (size);
		size += str_len (wp->w_qos->qos.user_data);
	}
	if (wp->w_publisher->qos.partition) {
		ROUND_LEN (size);
		psize = partition_size (wp->w_publisher->qos.partition);
		size += psize;
	}
	if (wp->w_topic->qos->qos.topic_data) {
		ROUND_LEN (size);
		size += str_len (wp->w_topic->qos->qos.topic_data);
	}
	if (wp->w_publisher->qos.group_data) {
		ROUND_LEN (size);
		size += str_len (wp->w_publisher->qos.group_data);
	}
	xp = xmalloc (size);
	if (!xp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	dp->topic_name = xp;
	dp->key.value [0] = wp->w_publisher->domain->participant.p_guid_prefix.w [0];
	dp->key.value [1] = wp->w_publisher->domain->participant.p_guid_prefix.w [1];
	dp->key.value [2] = wp->w_entity_id.w;
	memcpy (&dp->participant_key,
		&wp->w_publisher->domain->participant.p_guid_prefix,
		sizeof (DDS_BuiltinTopicKey_t));
	memcpy (xp, str_ptr (wp->w_topic->name),
						str_len (wp->w_topic->name));
	xp += str_len (wp->w_topic->name);
	dp->type_name = xp;
	memcpy (xp, str_ptr (wp->w_topic->type->type_name),
				     str_len (wp->w_topic->type->type_name));
	xp += str_len (wp->w_topic->type->type_name);
	uqp = &wp->w_qos->qos;
	dp->durability.kind = uqp->durability_kind;
#ifdef DURABILITY_SERVICE
	dp->durability_service.service_cleanup_delay = uqp->ds_cleanup_delay;
	dp->durability_service.history_kind = uqp->ds_history_kind;
	dp->durability_service.history_depth = uqp->ds_history_depth;
	dp->durability_service.max_samples = uqp->ds_limits.max_samples;
	dp->durability_service.max_instances = uqp->ds_limits.max_instances;
	dp->durability_service.max_samples_per_instance = uqp->ds_limits.max_samples_per_instance;
#else
	dp->durability_service = qos_def_writer_qos.durability_service;
#endif
	dp->deadline = uqp->deadline;
	dp->latency_budget = uqp->latency_budget;
	dp->liveliness.kind = uqp->liveliness_kind;
	dp->liveliness.lease_duration = uqp->liveliness_lease_duration;
	dp->reliability.kind = uqp->reliability_kind;
	dp->reliability.max_blocking_time = uqp->reliability_max_blocking_time;
	dp->lifespan = uqp->lifespan;
	if (uqp->user_data)
		ROUND_PTR (xp);
	oseq_set (&dp->user_data.value, uqp->user_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->user_data.value);
	dp->ownership.kind = uqp->ownership_kind;
	dp->ownership_strength = uqp->ownership_strength;
	dp->destination_order.kind = uqp->destination_order_kind;
	dp->presentation.access_scope = uqp->presentation_access_scope;
	dp->presentation.coherent_access = uqp->presentation_coherent_access;
	dp->presentation.ordered_access = uqp->presentation_ordered_access;
	if (uqp->partition)
		ROUND_PTR (xp);
	xp += partition_set (&dp->partition.name, wp->w_publisher->qos.partition, (unsigned char *) xp);
	if (wp->w_topic->qos->qos.topic_data)
		ROUND_PTR (xp);
	oseq_set (&dp->topic_data.value, wp->w_topic->qos->qos.topic_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->topic_data.value);
	if (wp->w_publisher->qos.group_data)
		ROUND_PTR (xp);
	oseq_set (&dp->group_data.value, wp->w_publisher->qos.group_data, (unsigned char *) xp);
	return (DDS_RETCODE_OK);
}

int dcps_get_builtin_subscription_data (DDS_SubscriptionBuiltinTopicData *dp,
					DiscoveredReader_t               *drp)
{
	size_t		size, psize;
	char		*xp;
	UniQos_t	*uqp;

	size = str_len (drp->dr_topic->name);
	size += str_len (drp->dr_topic->type->type_name);
	if (drp->dr_qos->qos.user_data) {
		ROUND_LEN (size);
		size += str_len (drp->dr_qos->qos.user_data);
	}
	if (drp->dr_qos->qos.partition) {
		ROUND_LEN (size);
		psize = partition_size (drp->dr_qos->qos.partition);
		size += psize;
	}
	if (drp->dr_qos->qos.topic_data) {
		ROUND_LEN (size);
		size += str_len (drp->dr_qos->qos.topic_data);
	}
	if (drp->dr_qos->qos.group_data) {
		ROUND_LEN (size);
		size += str_len (drp->dr_qos->qos.group_data);
	}
	xp = xmalloc (size);
	if (!xp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	dp->topic_name = xp;
	dp->key.value [0] = drp->dr_participant->p_guid_prefix.w [0];
	dp->key.value [1] = drp->dr_participant->p_guid_prefix.w [1];
	dp->key.value [2] = drp->dr_entity_id.w;
	memcpy (&dp->participant_key,
		&drp->dr_participant->p_guid_prefix,
		sizeof (DDS_BuiltinTopicKey_t));
	memcpy (xp, str_ptr (drp->dr_topic->name),
				str_len (drp->dr_topic->name));
	xp += str_len (drp->dr_topic->name);
	dp->type_name = xp;
	memcpy (xp, str_ptr (drp->dr_topic->type->type_name),
				str_len (drp->dr_topic->type->type_name));
	xp += str_len (drp->dr_topic->type->type_name);
	uqp = &drp->dr_qos->qos;
	dp->durability.kind = uqp->durability_kind;
	dp->deadline = uqp->deadline;
	dp->latency_budget = uqp->latency_budget;
	dp->liveliness.kind = uqp->liveliness_kind;
	dp->liveliness.lease_duration = uqp->liveliness_lease_duration;
	dp->reliability.kind = uqp->reliability_kind;
	dp->reliability.max_blocking_time = uqp->reliability_max_blocking_time;
	dp->ownership.kind = uqp->ownership_kind;
	dp->destination_order.kind = uqp->destination_order_kind;
	if (uqp->user_data)
		ROUND_PTR (xp);
	oseq_set (&dp->user_data.value, uqp->user_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->user_data.value);
	dp->time_based_filter = drp->dr_time_based_filter;
	dp->presentation.access_scope = uqp->presentation_access_scope;
	dp->presentation.coherent_access = uqp->presentation_coherent_access;
	dp->presentation.ordered_access = uqp->presentation_ordered_access;
	if (uqp->partition)
		ROUND_PTR (xp);
	xp += partition_set (&dp->partition.name, uqp->partition, (unsigned char *) xp);
	if (uqp->topic_data)
		ROUND_PTR (xp);
	oseq_set (&dp->topic_data.value, uqp->topic_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->topic_data.value);
	if (uqp->group_data)
		ROUND_PTR (xp);
	oseq_set (&dp->group_data.value, uqp->group_data, (unsigned char *) xp);
	return (DDS_RETCODE_OK);
}

int dcps_get_local_subscription_data (DDS_SubscriptionBuiltinTopicData *dp,
				      Reader_t                         *rp)
{
	size_t		size, psize;
	char		*xp;
	UniQos_t	*uqp;

	size = str_len (rp->r_topic->name);
	size += str_len (rp->r_topic->type->type_name);
	if (rp->r_qos->qos.user_data) {
		ROUND_LEN (size);
		size += str_len (rp->r_qos->qos.user_data);
	}
	if (rp->r_subscriber->qos.partition) {
		ROUND_LEN (size);
		psize = partition_size (rp->r_subscriber->qos.partition);
		size += psize;
	}
	if (rp->r_topic->qos->qos.topic_data) {
		ROUND_LEN (size);
		size += str_len (rp->r_topic->qos->qos.topic_data);
	}
	if (rp->r_subscriber->qos.group_data) {
		ROUND_LEN (size);
		size += str_len (rp->r_subscriber->qos.group_data);
	}
	xp = xmalloc (size);
	if (!xp)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	dp->topic_name = xp;
	dp->key.value [0] = rp->r_subscriber->domain->participant.p_guid_prefix.w [0];
	dp->key.value [1] = rp->r_subscriber->domain->participant.p_guid_prefix.w [1];
	dp->key.value [2] = rp->r_entity_id.w;
	memcpy (&dp->participant_key,
		&rp->r_subscriber->domain->participant.p_guid_prefix,
		sizeof (DDS_BuiltinTopicKey_t));
	memcpy (xp, str_ptr (rp->r_topic->name),
				str_len (rp->r_topic->name));
	xp += str_len (rp->r_topic->name);
	dp->type_name = xp;
	memcpy (xp, str_ptr (rp->r_topic->type->type_name),
				str_len (rp->r_topic->type->type_name));
	xp += str_len (rp->r_topic->type->type_name);
	uqp = &rp->r_qos->qos;
	dp->durability.kind = uqp->durability_kind;
	dp->deadline = uqp->deadline;
	dp->latency_budget = uqp->latency_budget;
	dp->liveliness.kind = uqp->liveliness_kind;
	dp->liveliness.lease_duration = uqp->liveliness_lease_duration;
	dp->reliability.kind = uqp->reliability_kind;
	dp->reliability.max_blocking_time = uqp->reliability_max_blocking_time;
	dp->ownership.kind = uqp->ownership_kind;
	dp->destination_order.kind = uqp->destination_order_kind;
	if (uqp->user_data)
		ROUND_PTR (xp);
	oseq_set (&dp->user_data.value, uqp->user_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->user_data.value);
	dp->time_based_filter = rp->r_time_based_filter;
	dp->presentation.access_scope = uqp->presentation_access_scope;
	dp->presentation.coherent_access = uqp->presentation_coherent_access;
	dp->presentation.ordered_access = uqp->presentation_ordered_access;
	if (uqp->partition)
		ROUND_PTR (xp);
	xp += partition_set (&dp->partition.name, rp->r_subscriber->qos.partition, (unsigned char *) xp);
	if (rp->r_topic->qos->qos.topic_data)
		ROUND_PTR (xp);
	oseq_set (&dp->topic_data.value, rp->r_topic->qos->qos.topic_data, (unsigned char *) xp);
	xp += DDS_SEQ_LENGTH (dp->topic_data.value);
	if (rp->r_subscriber->qos.group_data)
		ROUND_PTR (xp);
	oseq_set (&dp->group_data.value, rp->r_subscriber->qos.group_data, (unsigned char *) xp);
	return (DDS_RETCODE_OK);
}

static void *read_builtin_participant_data (Change_t *cp)
{
	Entity_t			*ep;
	DDS_ParticipantBuiltinTopicData	*dp;

	ep = entity_ptr (cp->c_writer);
	if (!ep)
		return (NULL);
	if (ep->type != ET_PARTICIPANT ||
	    (ep->flags & EF_REMOTE) == 0)
		return (NULL);

	dp = xmalloc (sizeof (DDS_ParticipantBuiltinTopicData));
	if (!dp)
		return (NULL);

	if (dcps_get_builtin_participant_data (dp, (Participant_t *) ep)) {
		xfree (dp);
		return (NULL);
	}
	return (dp);
}

static void free_builtin_participant_data (void *p)
{
	DDS_ParticipantBuiltinTopicData	*dp = (DDS_ParticipantBuiltinTopicData *) p;

	if (dp->user_data.value._length)
		xfree (dp->user_data.value._buffer);
	xfree (dp);
}

static void *read_builtin_publication_data (Change_t *cp)
{
	Entity_t			*ep;
	DDS_PublicationBuiltinTopicData	*dp;

	ep = entity_ptr (cp->c_writer);
	if (!ep)
		return (NULL);
	if (ep->type != ET_WRITER ||
	    (ep->flags & EF_REMOTE) == 0)
		return (NULL);

	dp = xmalloc (sizeof (DDS_PublicationBuiltinTopicData));
	if (!dp)
		return (NULL);

	if (dcps_get_builtin_publication_data (dp, (DiscoveredWriter_t *) ep)) {
		xfree (dp);
		return (NULL);
	}
	return (dp);
}

static void free_builtin_publication_data (void *p)
{
	DDS_PublicationBuiltinTopicData	*dp = (DDS_PublicationBuiltinTopicData *) p;

	xfree (dp->topic_name);
	xfree (dp);
}

void DDS_PublicationBuiltinTopicData__init (DDS_PublicationBuiltinTopicData *data)
{
	memset (data, 0, sizeof (DDS_PublicationBuiltinTopicData));
	DDS_SEQ_INIT (data->user_data.value);
	DDS_SEQ_INIT (data->partition.name);
	DDS_SEQ_INIT (data->topic_data.value);
	DDS_SEQ_INIT (data->group_data.value);
}

void DDS_PublicationBuiltinTopicData__clear (DDS_PublicationBuiltinTopicData *data)
{
	if (data->topic_name)
		xfree (data->topic_name);

	DDS_PublicationBuiltinTopicData__init (data);
}

DDS_PublicationBuiltinTopicData *DDS_PublicationBuiltinTopicData__alloc (void)
{
	DDS_PublicationBuiltinTopicData	*p;

	p = xmalloc (sizeof (DDS_PublicationBuiltinTopicData));
	if (!p)
		return (NULL);

	DDS_PublicationBuiltinTopicData__init (p);
	return (p);
}

void DDS_PublicationBuiltinTopicData__free (DDS_PublicationBuiltinTopicData *data)
{
	if (!data)
		return;

	DDS_PublicationBuiltinTopicData__clear (data);
	xfree (data);
}

static void *read_builtin_subscription_data (Change_t *cp)
{
	Entity_t			 *ep;
	DDS_SubscriptionBuiltinTopicData *dp;

	ep = entity_ptr (cp->c_writer);
	if (!ep)
		return (NULL);
	if (ep->type != ET_READER ||
	    (ep->flags & EF_REMOTE) == 0)
		return (NULL);

	dp = xmalloc (sizeof (DDS_SubscriptionBuiltinTopicData));
	if (!dp)
		return (NULL);

	if (dcps_get_builtin_subscription_data (dp, (DiscoveredReader_t *) ep)) {
		xfree (dp);
		return (NULL);
	}
	return (dp);
}

static void free_builtin_subscription_data (void *p)
{
	DDS_SubscriptionBuiltinTopicData *dp = (DDS_SubscriptionBuiltinTopicData *) p;

	xfree (dp->topic_name);
	xfree (dp);
}

void DDS_SubscriptionBuiltinTopicData__init (DDS_SubscriptionBuiltinTopicData *data)
{
	memset (data, 0, sizeof (DDS_SubscriptionBuiltinTopicData));
	DDS_SEQ_INIT (data->user_data.value);
	DDS_SEQ_INIT (data->partition.name);
	DDS_SEQ_INIT (data->topic_data.value);
	DDS_SEQ_INIT (data->group_data.value);
}

void DDS_SubscriptionBuiltinTopicData__clear (DDS_SubscriptionBuiltinTopicData *data)
{
	if (data->topic_name)
		xfree (data->topic_name);

	DDS_SubscriptionBuiltinTopicData__init (data);
}

DDS_SubscriptionBuiltinTopicData *DDS_SubscriptionBuiltinTopicData__alloc (void)
{
	DDS_SubscriptionBuiltinTopicData	*p;

	p = xmalloc (sizeof (DDS_SubscriptionBuiltinTopicData));
	if (!p)
		return (NULL);

	DDS_SubscriptionBuiltinTopicData__init (p);
	return (p);
}

void DDS_SubscriptionBuiltinTopicData__free (DDS_SubscriptionBuiltinTopicData *data)
{
	if (!data)
		return;

	DDS_SubscriptionBuiltinTopicData__clear (data);
	xfree (data);
}

static void *read_builtin_topic_data (Change_t *cp)
{
	Entity_t			*ep;
	DDS_TopicBuiltinTopicData	*dp;

	ep = entity_ptr (cp->c_writer);
	if (!ep)
		return (NULL);
	if (ep->type != ET_TOPIC ||
	    (ep->flags & EF_REMOTE) == 0)
		return (NULL);

	dp = xmalloc (sizeof (DDS_TopicBuiltinTopicData));
	if (!dp)
		return (NULL);

	if (dcps_get_builtin_topic_data (dp, (Topic_t *) ep, 1)) {
		xfree (dp);
		return (NULL);
	}
	return (dp);
}

static void free_builtin_topic_data (void *p)
{
	DDS_TopicBuiltinTopicData	*dp = (DDS_TopicBuiltinTopicData *) p;

	xfree (dp->name);
	xfree (dp);
}

void *dcps_read_builtin_data (Reader_t *rp, Change_t *cp)
{
	void	*ret;

	switch (rp->r_topic->type->index) {
		case BT_Participant:
			ret = read_builtin_participant_data (cp);
			break;
		case BT_Publication:
			ret = read_builtin_publication_data (cp);
			break;
		case BT_Subscription:
			ret = read_builtin_subscription_data (cp);
			break;
		case BT_Topic:
			ret = read_builtin_topic_data (cp);
			break;
		default:
			ret = NULL;
			break;
	}
	return (ret);
}

void dcps_free_builtin_data (Reader_t *rp, void *dp)
{
	switch (rp->r_topic->type->index) {
		case BT_Participant:
			free_builtin_participant_data (dp);
			break;
		case BT_Publication:
			free_builtin_publication_data (dp);
			break;
		case BT_Subscription:
			free_builtin_subscription_data (dp);
			break;
		case BT_Topic:
			free_builtin_topic_data (dp);
			break;
		default:
			break;
	}
}

#endif

