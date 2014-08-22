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

/* uqos.h -- Defines common operations on QoS policies of different entities
	     such as Readers, Writers and Topics and attempts to optimize the
	     amount of storage that is required for the QoS parameters. */

#ifndef __uqos_h_
#define	__uqos_h_

#include "dds/dds_dcps.h"
#include "pool.h"
#include "str.h"
#include "strseq.h"

/* Predefined defaults for the various QoS parameters. */
extern const DDS_OctetSeq qos_data_init;
extern const DDS_DurabilityQosPolicy qos_durability_init;
extern const DDS_DurabilityServiceQosPolicy qos_durability_service_init;
extern const DDS_PresentationQosPolicy qos_presentation_init;
extern const DDS_DeadlineQosPolicy qos_deadline_init;
extern const DDS_LatencyBudgetQosPolicy qos_latency_init;
extern const DDS_OwnershipQosPolicy qos_ownership_init;
extern const DDS_OwnershipStrengthQosPolicy qos_ownership_strength_init;
extern const DDS_LivelinessQosPolicy qos_liveliness_init;
extern const DDS_TimeBasedFilterQosPolicy qos_time_based_filter_init;
extern const DDS_PartitionQosPolicy qos_partition_init;
extern const DDS_ReliabilityQosPolicy qos_reliability_init;
extern const DDS_TransportPriorityQosPolicy qos_transport_prio_init;
extern const DDS_LifespanQosPolicy qos_lifespan_init;
extern const DDS_DestinationOrderQosPolicy qos_destination_order_init;
extern const DDS_HistoryQosPolicy qos_history_init;
extern const DDS_ResourceLimitsQosPolicy qos_resource_limits_init;
extern const DDS_EntityFactoryQosPolicy qos_entity_factory_init;
extern const DDS_WriterDataLifecycleQosPolicy qos_writer_data_lifecycle_init;
extern const DDS_ReaderDataLifecycleQosPolicy qos_reader_data_lifecycle_init;

/* Predefined defaults for the various QoS parameter sets: */
extern DDS_DomainParticipantQos qos_def_participant_qos;
extern DDS_TopicQos qos_def_topic_qos;
extern DDS_PublisherQos qos_def_publisher_qos;
extern DDS_SubscriberQos qos_def_subscriber_qos;
extern DDS_DataReaderQos qos_def_reader_qos;
extern DDS_DataWriterQos qos_def_writer_qos;

/* QoS parameters validation functions: */
int qos_valid_participant_qos (const DDS_DomainParticipantQos *qos);
int qos_valid_topic_qos (const DDS_TopicQos *qos);
int qos_valid_subscriber_qos (const DDS_SubscriberQos *qos);
int qos_valid_publisher_qos (const DDS_PublisherQos *qos);
int qos_valid_reader_qos (const DDS_DataReaderQos *qos);
int qos_valid_writer_qos (const DDS_DataWriterQos *qos);

/* Unified Group QoS parameters as used in */

typedef struct group_qos_st {
	String_t		*group_data;
	Strings_t		*partition;
	unsigned		presentation_access_scope:2;
	unsigned		presentation_coherent_access:1;
	unsigned		presentation_ordered_access:1;
	unsigned		no_autoenable:1;
} GroupQos_t;

/* Unified QoS parameters structure that can be used to store various QoS
   types, such as Topic, Reader, Writer, Discovered Topic, Discovered Reader
   and Discovered Writer QoS parameters: */

typedef struct uni_qos_t {
	String_t			*user_data;
	String_t			*topic_data;
	String_t			*group_data;
	Strings_t			*partition;
	unsigned			durability_kind:2;
	unsigned			presentation_access_scope:2;
	unsigned			presentation_coherent_access:1;
	unsigned			presentation_ordered_access:1;
	unsigned			ownership_kind:1;
	unsigned			liveliness_kind:2;
	unsigned			reliability_kind:1;
	unsigned			destination_order_kind:1;
	unsigned			history_kind:1;
	unsigned			no_autodispose:1;
#ifdef DURABILITY_SERVICE
	unsigned			ds_history_kind:1;
	DDS_Duration_t			ds_cleanup_delay;
	int				ds_history_depth;
	DDS_ResourceLimitsQosPolicy	ds_limits;
#endif
	DDS_DeadlineQosPolicy		deadline;
	DDS_LatencyBudgetQosPolicy	latency_budget;
	DDS_OwnershipStrengthQosPolicy	ownership_strength;
	DDS_Duration_t			liveliness_lease_duration;
	DDS_Duration_t			reliability_max_blocking_time;
	DDS_TransportPriorityQosPolicy	transport_priority;
	DDS_LifespanQosPolicy		lifespan;
	int				history_depth;
	DDS_ResourceLimitsQosPolicy	resource_limits;
} UniQos_t;

typedef struct qos_data_st {
	unsigned	users;
	UniQos_t	qos;
} Qos_t;


int qos_pool_init (const POOL_LIMITS *qosrefs, const POOL_LIMITS *qosdata);

/* Setup a pool of QoS entries for the respective minimum/extra amount of Qos
   nodes and node references. */

void qos_pool_free (void);

/* Free all QoS pools. */

#define	qos_ptr(rp)	&rp->qos

/* Convert a (Qos_t *) to a (UniQos_t *). */

Qos_t *qos_add (const UniQos_t *qp);

/* Add a QoS data set to the QoS database and return a reference pointer. */

void qos_free (Qos_t *rp);

/* Release allocated QoS data. */


void qos_init_user_data (DDS_UserDataQosPolicy *qp);

/* Initialize QoS User data. */

String_t *qos_octets2str (const DDS_OctetSeq *sp);

/* Copy an Octet sequence to a String. */

void qos_str2octets (const String_t *s, DDS_OctetSeq *sp);

/* Copy a String to an Octet sequence. */


/* Publisher QoS.
   -------------- */

int qos_publisher_new (GroupQos_t *gp, const DDS_PublisherQos *qp);

/* Create Publisher QoS parameters from the given data in *qp. */

int qos_publisher_update (GroupQos_t *gp, const DDS_PublisherQos *qp);

/* Update a Publisher QoS reference with new values. If updating is not
   possible, an error value is returned. */

void qos_publisher_get (GroupQos_t *gp, DDS_PublisherQos *qp);

/* Get Publisher QoS data. */

void qos_publisher_free (GroupQos_t *gp);

/* Release the allocated Publisher QoS data. */


/* Subscriber QoS.
   -------------- */

int qos_subscriber_new (GroupQos_t *gp, const DDS_SubscriberQos *qp);

/* Create Subscriber QoS parameters from the given data in *qp. */

int qos_subscriber_update (GroupQos_t *gp, const DDS_SubscriberQos *qp);

/* Update a Subscriber QoS reference with new values. If updating is not
   possible, an error value is returned. */

void qos_subscriber_get (GroupQos_t *gp, DDS_SubscriberQos *qp);

/* Get Subscriber QoS data. */

void qos_subscriber_free (GroupQos_t *gp);

/* Release the allocated Subscriber QoS data. */


/* Topic QoS.
   ---------- */

Qos_t *qos_topic_new (const DDS_TopicQos *qp);

/* Get a new Topic QoS reference. */

int qos_topic_update (Qos_t **rp, const DDS_TopicQos *qp);

/* Update a Topic QoS reference with new values. If updating is not possible,
   a NULL-value is returned. */

void qos_topic_get (Qos_t *rp, DDS_TopicQos *qp);

/* Get Topic QoS data. */

#define	qos_topic_free(rp)	qos_free(rp)

/* Release the allocated Topic QoS data. */


/* Writer QoS.
   ----------- */

Qos_t *qos_writer_new (const DDS_DataWriterQos *wp);

/* Get a new Writer QoS reference. */

int qos_writer_update (Qos_t **wp, const DDS_DataWriterQos *qp);

/* Update a Writer QoS reference with new writer values. If updating is not
   possible, a NULL-value is returned. */

void qos_writer_get (Qos_t *rp, DDS_DataWriterQos *qp);

/* Get a Writer QoS pointer. */

#define	qos_writer_free(rp)	qos_free(rp)

/* Release the allocated Writer QoS data. */

void qos_init_writer_data_lifecycle (DDS_WriterDataLifecycleQosPolicy *qp);

/* Initialise writer data lifecycle QoS. */


/* Reader QoS.
   ----------- */

Qos_t *qos_reader_new (const DDS_DataReaderQos *qp);

/* Get a new Reader QoS reference. */

int qos_reader_update (Qos_t **rp, const DDS_DataReaderQos *qp);

/* Update a Reader Qos reference with new values. If updating is not possible,
   an NULL-value is returned. */

void qos_reader_get (Qos_t *rp, DDS_DataReaderQos *qp);

/* Get a Reader QoS pointer. */

#define	qos_reader_free(rp)	qos_free(rp)

/* Release the allocated Reader QoS data. */

void qos_init_time_based_filter (DDS_TimeBasedFilterQosPolicy *qp);

/* Initialise time-based-filter QoS. */

void qos_init_reader_data_lifecycle (DDS_ReaderDataLifecycleQosPolicy *qp);

/* Initialise reader data lifecycle QoS. */


/* Discovered Topic QoS.
   --------------------- */

typedef struct discovered_topic_qos_st {
	DDS_DurabilityQosPolicy 	durability;
	DDS_DurabilityServiceQosPolicy 	durability_service;
	DDS_DeadlineQosPolicy		deadline;
	DDS_LatencyBudgetQosPolicy 	latency_budget;
	DDS_LivelinessQosPolicy		liveliness;
	DDS_ReliabilityQosPolicy 	reliability;
	DDS_TransportPriorityQosPolicy	transport_priority;
	DDS_LifespanQosPolicy		lifespan;
	DDS_DestinationOrderQosPolicy	destination_order;
	DDS_HistoryQosPolicy		history;
	DDS_ResourceLimitsQosPolicy	resource_limits;
	DDS_OwnershipQosPolicy		ownership;
	String_t			*topic_data;
} DiscoveredTopicQos;

extern DiscoveredTopicQos qos_def_disc_topic_qos;

Qos_t *qos_disc_topic_new (DiscoveredTopicQos *qp);

/* Get a new discovered Topic QoS reference. */

int qos_disc_topic_update (Qos_t **rp, DiscoveredTopicQos *qp);

/* Update a Topic QoS reference with new values.  If updating was not possible,
   an NULL-value is returned. */

void qos_disc_topic_get (Qos_t *rp, DiscoveredTopicQos *qp);

/* Get Topic QoS data. */

#define	qos_disc_topic_free(rp)	qos_free(rp)

/* Release the allocated Topic QoS data. */


/* Discovered Writer QoS.
   ---------------------- */

typedef struct discovered_writer_qos_st {
	DDS_DurabilityQosPolicy		durability;
	DDS_DurabilityServiceQosPolicy	durability_service;
	DDS_DeadlineQosPolicy		deadline;
	DDS_LatencyBudgetQosPolicy	latency_budget;
	DDS_LivelinessQosPolicy		liveliness;
	DDS_ReliabilityQosPolicy	reliability;
	DDS_LifespanQosPolicy		lifespan;
	String_t			*user_data;
	DDS_OwnershipQosPolicy		ownership;
	DDS_OwnershipStrengthQosPolicy	ownership_strength;
	DDS_DestinationOrderQosPolicy	destination_order;
	DDS_PresentationQosPolicy	presentation;
	Strings_t			*partition;
	String_t			*topic_data;
	String_t			*group_data;
} DiscoveredWriterQos;

extern DiscoveredWriterQos qos_def_disc_writer_qos;


void qos_disc_writer_set (UniQos_t *up, DiscoveredWriterQos *qp);

/* Get the Discovered Writer QoS settings in the unified format. */

void qos_disc_writer_restore (DiscoveredWriterQos *qp, UniQos_t *up);

/* Restore Discovered Writer QoS dataset after qos_disc_writer_set. */

int qos_disc_writer_update (Qos_t **rp, DiscoveredWriterQos *qp);

/* Update a Writer Qos reference with new values. If updating is not possible,
   0 is returned. */

void qos_disc_writer_get (Qos_t *rp, DiscoveredWriterQos *qp);

/* Get a Writer QoS pointer. */

#define	qos_disc_writer_free(rp) qos_free(rp)

/* Release the allocated Writer QoS data. */


void qos_init_writer_data_lifecycle (DDS_WriterDataLifecycleQosPolicy *qp);

/* Initialise writer data lifecycle QoS. */


/* Discovered Reader QoS.
   ---------------------- */

typedef struct discovered_reader_qos_st {
	DDS_DurabilityQosPolicy		durability;
	DDS_DeadlineQosPolicy		deadline;
	DDS_LatencyBudgetQosPolicy	latency_budget;
	DDS_LivelinessQosPolicy		liveliness;
	DDS_ReliabilityQosPolicy	reliability;
	DDS_OwnershipQosPolicy		ownership;
	DDS_DestinationOrderQosPolicy	destination_order;
	String_t			*user_data;
	DDS_TimeBasedFilterQosPolicy	time_based_filter;
	DDS_PresentationQosPolicy	presentation;
	Strings_t			*partition;
	String_t			*topic_data;
	String_t			*group_data;
} DiscoveredReaderQos;

extern DiscoveredReaderQos qos_def_disc_reader_qos;


void qos_disc_reader_set (UniQos_t *up, DiscoveredReaderQos *qp);

/* Get the Discovered Reader QoS settings in the unified format. */

void qos_disc_reader_restore (DiscoveredReaderQos *qp, UniQos_t *up);

/* Restore Discovered Reader QoS dataset after qos_disc_reader_set. */

int qos_disc_reader_update (Qos_t **rp, DiscoveredReaderQos *qp);

/* Update a Reader Qos reference with new values. If updating is not possible,
   a NULL-value is returned. */

void qos_disc_reader_get (Qos_t *rp, DiscoveredReaderQos *qp);

/* Get a Reader QoS pointer. */

#define	qos_disc_reader_free(rp) qos_free(rp)

/* Release the allocated Reader QoS data. */


void qos_init_time_based_filter (DDS_TimeBasedFilterQosPolicy *qp);

/* Initialise time-based-filter QoS. */

void qos_init_reader_data_lifecycle (DDS_ReaderDataLifecycleQosPolicy *qp);

/* Initialise reader data lifecycle QoS. */


/* QoS matching.
   ------------- */

int qos_match (const UniQos_t *wp, const GroupQos_t *wgp,
	       const UniQos_t *rp, const GroupQos_t *rgp,
	       DDS_QOS_POLICY_ID *qid);

/* Returns a non-0 result if the Writer/Reader QoS values are compatible.
   If either wgp or rgp is significant, parent data needs to be taken from
   that dataset.  If log is set and matching isn't successful, this will
   be logged. */

int qos_same_partition (Strings_t *wp, Strings_t *rp);

/* Returns a non-0 result if Writer and Reader are in the same partition. */


/* Trace/debug support.
   -------------------- */

void qos_dump (void);

/* Dump all QoS data records. */

void qos_entity_dump (void *ep);

/* Dump Entity QoS parameters. */

void qos_pool_dump (size_t sizes []);

/* Dump all QoS pool statistics. */

#endif	/* !__uqos_h_ */

