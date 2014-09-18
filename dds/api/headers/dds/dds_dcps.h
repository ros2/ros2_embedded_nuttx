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

#ifndef DDS_DCPS_H_
#define DDS_DCPS_H_

#include <stdint.h>
#include "dds/dds_types.h"
#include "dds/dds_tsm.h"

#ifdef  __cplusplus
extern "C" {
#endif

/* === Generic ============================================================== */

typedef enum {
	DDS_INCONSISTENT_TOPIC_STATUS         = (0x00000001UL <<  0),
	DDS_OFFERED_DEADLINE_MISSED_STATUS    = (0x00000001UL <<  1),
	DDS_REQUESTED_DEADLINE_MISSED_STATUS  = (0x00000001UL <<  2),
	DDS_OFFERED_INCOMPATIBLE_QOS_STATUS   = (0x00000001UL <<  5),
	DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS = (0x00000001UL <<  6),
	DDS_SAMPLE_LOST_STATUS                = (0x00000001UL <<  7),
	DDS_SAMPLE_REJECTED_STATUS            = (0x00000001UL <<  8),
	DDS_DATA_ON_READERS_STATUS            = (0x00000001UL <<  9),
	DDS_DATA_AVAILABLE_STATUS             = (0x00000001UL << 10),
	DDS_LIVELINESS_LOST_STATUS            = (0x00000001UL << 11),
	DDS_LIVELINESS_CHANGED_STATUS         = (0x00000001UL << 12),
	DDS_PUBLICATION_MATCHED_STATUS        = (0x00000001UL << 13),
	DDS_SUBSCRIPTION_MATCHED_STATUS       = (0x00000001UL << 14)
} DDS_StatusMask;

#define	DDS_ALL_STATUS		      0x0000ffffU

typedef unsigned DDS_DomainId_t;

typedef DDS_HANDLE DDS_InstanceHandle_t;

typedef uint32_t DDS_SampleStateKind;
#define DDS_READ_SAMPLE_STATE         (0x00000001U << 0)
#define DDS_NOT_READ_SAMPLE_STATE     (0x00000001U << 1)

typedef uint32_t DDS_SampleStateMask;
#define DDS_ANY_SAMPLE_STATE           0x0000ffffU

typedef uint32_t DDS_ViewStateKind;
#define DDS_NEW_VIEW_STATE            (0x00000001U << 0)
#define DDS_NOT_NEW_VIEW_STATE        (0x00000001U << 1)

typedef uint32_t DDS_ViewStateMask;
#define DDS_ANY_VIEW_STATE             0x0000ffffU

typedef uint32_t DDS_InstanceStateKind;
#define DDS_ALIVE_INSTANCE_STATE                (0x00000001U << 0)
#define DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE   (0x00000001U << 1)
#define DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE (0x00000001U << 2)

typedef uint32_t DDS_InstanceStateMask;
#define DDS_ANY_INSTANCE_STATE         0x0000ffffU
#define DDS_NOT_ALIVE_INSTANCE_STATE   0x00000006U

/* === Generic types ======================================================== */

typedef struct {
	int sec;
	unsigned nanosec;
} DDS_Duration_t;

#define DDS_DURATION_INFINITE_SEC	0x7fffffff
#define DDS_DURATION_INFINITE_NSEC	0x7fffffffU
#define DDS_DURATION_ZERO_SEC		0L
#define DDS_DURATION_ZERO_NSEC		0UL

typedef struct {
	int sec;
	unsigned nanosec;
} DDS_Time_t;

#define DDS_TIME_INVALID_SEC		-1L
#define DDS_TIME_INVALID_NSEC		0xffffffffU

typedef struct {
	DDS_SampleStateKind sample_state;
	DDS_ViewStateKind view_state;
	DDS_InstanceStateKind instance_state;
	DDS_Time_t source_timestamp;
	DDS_InstanceHandle_t instance_handle;
	DDS_InstanceHandle_t publication_handle;
	int disposed_generation_count;
	int no_writers_generation_count;
	int sample_rank;
	int generation_rank;
	int absolute_generation_rank;
	int valid_data;
} DDS_SampleInfo;

#define	DDS_LENGTH_UNLIMITED	-1

/* === Standard DDS objects ================================================= */

typedef struct DDS_DomainParticipant_st    *DDS_DomainParticipant;
typedef struct DDS_Listener_st             *DDS_Listener;
typedef void                               *DDS_Entity;
typedef struct DDS_TopicDescription_st     *DDS_TopicDescription;
typedef struct DDS_Topic_st                *DDS_Topic;
typedef struct DDS_ContentFilteredTopic_st *DDS_ContentFilteredTopic;
typedef struct DDS_MultiTopic_st           *DDS_MultiTopic;
typedef struct DDS_DataWriter_st           *DDS_DataWriter;
typedef struct DDS_DataReader_st           *DDS_DataReader;
typedef struct DDS_Subscriber_st           *DDS_Subscriber;
typedef struct DDS_Publisher_st            *DDS_Publisher;
typedef struct DDS_WaitSet_st              *DDS_WaitSet;
typedef void                               *DDS_Condition;
typedef struct DDS_StatusCondition_st      *DDS_StatusCondition;
typedef struct DDS_ReadCondition_st        *DDS_ReadCondition;
typedef struct DDS_QueryCondition_st       *DDS_QueryCondition;
typedef struct DDS_GuardCondition_st       *DDS_GuardCondition;

DDS_SEQUENCE(unsigned char, DDS_OctetSeq);

DDS_EXPORT DDS_OctetSeq *DDS_OctetSeq__alloc (void);
DDS_EXPORT void DDS_OctetSeq__free (DDS_OctetSeq *octets);
DDS_EXPORT void DDS_OctetSeq__init (DDS_OctetSeq *octets);
DDS_EXPORT void DDS_OctetSeq__clear (DDS_OctetSeq *octets);

DDS_SEQUENCE(char*, DDS_StringSeq);

DDS_EXPORT DDS_StringSeq *DDS_StringSeq__alloc (void);
DDS_EXPORT void DDS_StringSeq__free (DDS_StringSeq *strings);
DDS_EXPORT void DDS_StringSeq__init (DDS_StringSeq *strings);
DDS_EXPORT void DDS_StringSeq__clear (DDS_StringSeq *strings);

DDS_SEQUENCE(void *, DDS_DataSeq);

DDS_EXPORT DDS_DataSeq *DDS_DataSeq__alloc (void);
DDS_EXPORT void DDS_DataSeq__free (DDS_DataSeq *data);
DDS_EXPORT void DDS_DataSeq__init (DDS_DataSeq *data);
DDS_EXPORT void DDS_DataSeq__clear (DDS_DataSeq *data);

DDS_SEQUENCE(DDS_SampleInfo*, DDS_SampleInfoSeq);

DDS_EXPORT DDS_SampleInfoSeq *DDS_SampleInfoSeq__alloc (void);
DDS_EXPORT void DDS_SampleInfoSeq__free (DDS_SampleInfoSeq *samples);
DDS_EXPORT void DDS_SampleInfoSeq__init (DDS_SampleInfoSeq *samples);
DDS_EXPORT void DDS_SampleInfoSeq__clear (DDS_SampleInfoSeq *samples);

DDS_SEQUENCE(DDS_InstanceHandle_t, DDS_InstanceHandleSeq);

DDS_EXPORT DDS_InstanceHandleSeq *DDS_InstanceHandleSeq__alloc (void);
DDS_EXPORT void DDS_InstanceHandleSeq__free (DDS_InstanceHandleSeq *handles);
DDS_EXPORT void DDS_InstanceHandleSeq__init (DDS_InstanceHandleSeq *handles);
DDS_EXPORT void DDS_InstanceHandleSeq__clear (DDS_InstanceHandleSeq *handles);

DDS_SEQUENCE(DDS_DataReader, DDS_DataReaderSeq);

DDS_EXPORT DDS_DataReaderSeq *DDS_DataReaderSeq__alloc (void);
DDS_EXPORT void DDS_DataReaderSeq__free (DDS_DataReaderSeq *readers);
DDS_EXPORT void DDS_DataReaderSeq__init (DDS_DataReaderSeq *readers);
DDS_EXPORT void DDS_DataReaderSeq__clear (DDS_DataReaderSeq *readers);

/* === Generic QoS (alphabetic) ============================================= */

/* QoS policy names: */
#define DDS_USERDATA_QOS_POLICY_NAME		"UserData"
#define DDS_DURABILITY_QOS_POLICY_NAME		"Durability"
#define DDS_PRESENTATION_QOS_POLICY_NAME	"Presentation"
#define DDS_DEADLINE_QOS_POLICY_NAME		"Deadline"
#define DDS_LATENCYBUDGET_QOS_POLICY_NAME	"LatencyBudget"
#define DDS_OWNERSHIP_QOS_POLICY_NAME		"Ownership"
#define DDS_OWNERSHIPSTRENGTH_QOS_POLICY_NAME	"OwnershipStrength"
#define DDS_LIVELINESS_QOS_POLICY_NAME		"Liveliness"
#define DDS_TIMEBASEDFILTER_QOS_POLICY_NAME	"TimeBasedFilter"
#define DDS_PARTITION_QOS_POLICY_NAME		"Partition"
#define DDS_RELIABILITY_QOS_POLICY_NAME		"Reliability"
#define DDS_DESTINATIONORDER_QOS_POLICY_NAME	"DestinationOrder"
#define DDS_HISTORY_QOS_POLICY_NAME		"History"
#define DDS_RESOURCELIMITS_QOS_POLICY_NAME	"ResourceLimits"
#define DDS_ENTITYFACTORY_QOS_POLICY_NAME	"EntityFactory"
#define DDS_WRITERDATALIFECYCLE_QOS_POLICY_NAME	"WriterDataLifecycle"
#define DDS_READERDATALIFECYCLE_QOS_POLICY_NAME	"ReaderDataLifecycle"
#define DDS_TOPICDATA_QOS_POLICY_NAME		"TopicData"
#define DDS_GROUPDATA_QOS_POLICY_NAME		"GroupData"
#define DDS_TRANSPORTPRIORITY_QOS_POLICY_NAME	"TransportPriority"
#define DDS_LIFESPAN_QOS_POLICY_NAME		"Lifespan"
#define DDS_DURABILITYSERVICE_POLICY_NAME	"DurabilityService"

/* QoS policy ids: */
typedef enum {
	DDS_INVALID_QOS_POLICY_ID,
	DDS_USERDATA_QOS_POLICY_ID,
	DDS_DURABILITY_QOS_POLICY_ID,
	DDS_PRESENTATION_QOS_POLICY_ID,
	DDS_DEADLINE_QOS_POLICY_ID,
	DDS_LATENCYBUDGET_QOS_POLICY_ID,
	DDS_OWNERSHIP_QOS_POLICY_ID,
	DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID,
	DDS_LIVELINESS_QOS_POLICY_ID,
	DDS_TIMEBASEDFILTER_QOS_POLICY_ID,
	DDS_PARTITION_QOS_POLICY_ID,
	DDS_RELIABILITY_QOS_POLICY_ID,
	DDS_DESTINATIONORDER_QOS_POLICY_ID,
	DDS_HISTORY_QOS_POLICY_ID,
	DDS_RESOURCELIMITS_QOS_POLICY_ID,
	DDS_ENTITYFACTORY_QOS_POLICY_ID,
	DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID,
	DDS_READERDATALIFECYCLE_QOS_POLICY_ID,
	DDS_TOPICDATA_QOS_POLICY_ID,
	DDS_GROUPDATA_QOS_POLICY_ID,
	DDS_TRANSPORTPRIORITY_QOS_POLICY_ID,
	DDS_LIFESPAN_QOS_POLICY_ID,
	DDS_DURABILITYSERVICE_QOS_POLICY_ID
} DDS_QOS_POLICY_ID;

typedef enum {
	DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS,
	DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS
} DDS_DestinationOrderQosPolicyKind;

typedef enum {
	DDS_VOLATILE_DURABILITY_QOS,
	DDS_TRANSIENT_LOCAL_DURABILITY_QOS,
	DDS_TRANSIENT_DURABILITY_QOS,
	DDS_PERSISTENT_DURABILITY_QOS
} DDS_DurabilityQosPolicyKind;

typedef enum {
	DDS_KEEP_LAST_HISTORY_QOS,
	DDS_KEEP_ALL_HISTORY_QOS
} DDS_HistoryQosPolicyKind;

typedef enum {
	DDS_AUTOMATIC_LIVELINESS_QOS,
	DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS,
	DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS
} DDS_LivelinessQosPolicyKind;

typedef enum {
	DDS_SHARED_OWNERSHIP_QOS,
	DDS_EXCLUSIVE_OWNERSHIP_QOS
} DDS_OwnershipQosPolicyKind;

typedef enum  {
	DDS_INSTANCE_PRESENTATION_QOS,
	DDS_TOPIC_PRESENTATION_QOS,
	DDS_GROUP_PRESENTATION_QOS
} DDS_PresentationQosPolicyAccessScopeKind;

typedef enum {
	DDS_BEST_EFFORT_RELIABILITY_QOS,
	DDS_RELIABLE_RELIABILITY_QOS
} DDS_ReliabilityQosPolicyKind;

typedef struct {
	DDS_Duration_t service_cleanup_delay;
	DDS_HistoryQosPolicyKind history_kind;
	int history_depth;
	int max_samples;
	int max_instances;
	int max_samples_per_instance;
} DDS_DurabilityServiceQosPolicy;

typedef struct {
	DDS_Duration_t period;
} DDS_DeadlineQosPolicy;

typedef struct {
	DDS_DestinationOrderQosPolicyKind kind;
} DDS_DestinationOrderQosPolicy;

typedef struct {
	DDS_DurabilityQosPolicyKind kind;
} DDS_DurabilityQosPolicy;

typedef struct {
	int autoenable_created_entities;
} DDS_EntityFactoryQosPolicy;

typedef struct {
	DDS_OctetSeq value;
} DDS_GroupDataQosPolicy;

DDS_EXPORT DDS_GroupDataQosPolicy *DDS_GroupDataQosPolicy__alloc (void);
DDS_EXPORT void DDS_GroupDataQosPolicy__free (DDS_GroupDataQosPolicy *data);
DDS_EXPORT void DDS_GroupDataQosPolicy__init (DDS_GroupDataQosPolicy *data);
DDS_EXPORT void DDS_GroupDataQosPolicy__clear (DDS_GroupDataQosPolicy *data);

typedef struct {
	DDS_HistoryQosPolicyKind kind;
	int depth;
} DDS_HistoryQosPolicy;

typedef struct {
	DDS_Duration_t duration;
} DDS_LatencyBudgetQosPolicy;

typedef struct {
	DDS_Duration_t duration;
} DDS_LifespanQosPolicy;

typedef struct {
	DDS_LivelinessQosPolicyKind kind;
	DDS_Duration_t lease_duration;
} DDS_LivelinessQosPolicy;

typedef struct {
	DDS_OwnershipQosPolicyKind kind;
} DDS_OwnershipQosPolicy;

typedef struct {
	int value;
} DDS_OwnershipStrengthQosPolicy;

typedef struct {
	DDS_StringSeq name;
} DDS_PartitionQosPolicy;

DDS_EXPORT DDS_PartitionQosPolicy *DDS_PartitionQosPolicy__alloc (void);
DDS_EXPORT void DDS_PartitionQosPolicy__free (DDS_PartitionQosPolicy *part);
DDS_EXPORT void DDS_PartitionQosPolicy__init (DDS_PartitionQosPolicy *part);
DDS_EXPORT void DDS_PartitionQosPolicy__clear (DDS_PartitionQosPolicy *part);

typedef struct {
	DDS_PresentationQosPolicyAccessScopeKind access_scope;
	int coherent_access;
	int ordered_access;
} DDS_PresentationQosPolicy;

typedef struct {
	DDS_Duration_t autopurge_nowriter_samples_delay;
	DDS_Duration_t autopurge_disposed_samples_delay;
} DDS_ReaderDataLifecycleQosPolicy;

typedef struct {
	DDS_ReliabilityQosPolicyKind kind;
	DDS_Duration_t max_blocking_time;
} DDS_ReliabilityQosPolicy;

typedef struct {
	int max_samples;
	int max_instances;
	int max_samples_per_instance;
} DDS_ResourceLimitsQosPolicy;

typedef struct {
	DDS_Duration_t minimum_separation;
} DDS_TimeBasedFilterQosPolicy;

typedef struct {
	DDS_OctetSeq value;
} DDS_TopicDataQosPolicy;

DDS_EXPORT DDS_TopicDataQosPolicy *DDS_TopicDataQosPolicy__alloc (void);
DDS_EXPORT void DDS_TopicDataQosPolicy__free (DDS_TopicDataQosPolicy *data);
DDS_EXPORT void DDS_TopicDataQosPolicy__init (DDS_TopicDataQosPolicy *data);
DDS_EXPORT void DDS_TopicDataQosPolicy__clear (DDS_TopicDataQosPolicy *data);

typedef struct {
	int value;
} DDS_TransportPriorityQosPolicy;

typedef struct {
	DDS_OctetSeq value;
} DDS_UserDataQosPolicy;

DDS_EXPORT DDS_UserDataQosPolicy *DDS_UserDataQosPolicy__alloc (void);
DDS_EXPORT void DDS_UserDataQosPolicy__free (DDS_UserDataQosPolicy *data);
DDS_EXPORT void DDS_UserDataQosPolicy__init (DDS_UserDataQosPolicy *data);
DDS_EXPORT void DDS_UserDataQosPolicy__clear (DDS_UserDataQosPolicy *data);

typedef struct {
	int autodispose_unregistered_instances;
} DDS_WriterDataLifecycleQosPolicy;

typedef int DDS_QosPolicyId_t;

DDS_EXPORT const char *DDS_qos_policy (DDS_QosPolicyId_t id);

typedef struct {
	DDS_QosPolicyId_t policy_id;
	int count;
} DDS_QosPolicyCount;

DDS_SEQUENCE(DDS_QosPolicyCount, DDS_QosPolicyCountSeq);

/* === Builtin Topics ======================================================= */

typedef struct {
	DDS_BUILTIN_TOPIC_TYPE_NATIVE value [3];
} DDS_BuiltinTopicKey_t;

typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_UserDataQosPolicy user_data;
} DDS_ParticipantBuiltinTopicData;

DDS_EXPORT DDS_ParticipantBuiltinTopicData *DDS_ParticipantBuiltinTopicData__alloc (void);
DDS_EXPORT void DDS_ParticipantBuiltinTopicData__free (DDS_ParticipantBuiltinTopicData *data);
DDS_EXPORT void DDS_ParticipantBuiltinTopicData__init (DDS_ParticipantBuiltinTopicData *data);
DDS_EXPORT void DDS_ParticipantBuiltinTopicData__clear (DDS_ParticipantBuiltinTopicData *data);

typedef struct {
	DDS_BuiltinTopicKey_t key;
	char *name;
	char *type_name;
	DDS_DurabilityQosPolicy durability;
	DDS_DurabilityServiceQosPolicy durability_service;
	DDS_DeadlineQosPolicy deadline;
	DDS_LatencyBudgetQosPolicy latency_budget;
	DDS_LivelinessQosPolicy liveliness;
	DDS_ReliabilityQosPolicy reliability;
	DDS_TransportPriorityQosPolicy transport_priority;
	DDS_LifespanQosPolicy lifespan;
	DDS_DestinationOrderQosPolicy destination_order;
	DDS_HistoryQosPolicy history;
	DDS_ResourceLimitsQosPolicy resource_limits;
	DDS_OwnershipQosPolicy ownership;
	DDS_TopicDataQosPolicy topic_data;
} DDS_TopicBuiltinTopicData;

DDS_EXPORT DDS_TopicBuiltinTopicData *DDS_TopicBuiltinTopicData__alloc (void);
DDS_EXPORT void DDS_TopicBuiltinTopicData__free (DDS_TopicBuiltinTopicData *data);
DDS_EXPORT void DDS_TopicBuiltinTopicData__init (DDS_TopicBuiltinTopicData *data);
DDS_EXPORT void DDS_TopicBuiltinTopicData__clear (DDS_TopicBuiltinTopicData *data);

typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_BuiltinTopicKey_t participant_key;
	char *topic_name;
	char *type_name;
	DDS_DurabilityQosPolicy durability;
	DDS_DurabilityServiceQosPolicy durability_service;
	DDS_DeadlineQosPolicy deadline;
	DDS_LatencyBudgetQosPolicy latency_budget;
	DDS_LivelinessQosPolicy liveliness;
	DDS_ReliabilityQosPolicy reliability;
	DDS_LifespanQosPolicy lifespan;
	DDS_UserDataQosPolicy user_data;
	DDS_OwnershipQosPolicy ownership;
	DDS_OwnershipStrengthQosPolicy ownership_strength;
	DDS_DestinationOrderQosPolicy destination_order;
	DDS_PresentationQosPolicy presentation;
	DDS_PartitionQosPolicy partition;
	DDS_TopicDataQosPolicy topic_data;
	DDS_GroupDataQosPolicy group_data;
} DDS_PublicationBuiltinTopicData;

DDS_EXPORT DDS_PublicationBuiltinTopicData *DDS_PublicationBuiltinTopicData__alloc (void);
DDS_EXPORT void DDS_PublicationBuiltinTopicData__free (DDS_PublicationBuiltinTopicData *data);
DDS_EXPORT void DDS_PublicationBuiltinTopicData__init (DDS_PublicationBuiltinTopicData *data);
DDS_EXPORT void DDS_PublicationBuiltinTopicData__clear (DDS_PublicationBuiltinTopicData *data);

typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_BuiltinTopicKey_t participant_key;
	char *topic_name;
	char *type_name;
	DDS_DurabilityQosPolicy durability;
	DDS_DeadlineQosPolicy deadline;
	DDS_LatencyBudgetQosPolicy latency_budget;
	DDS_LivelinessQosPolicy liveliness;
	DDS_ReliabilityQosPolicy reliability;
	DDS_OwnershipQosPolicy ownership;
	DDS_DestinationOrderQosPolicy destination_order;
	DDS_UserDataQosPolicy user_data;
	DDS_TimeBasedFilterQosPolicy time_based_filter;
	DDS_PresentationQosPolicy presentation;
	DDS_PartitionQosPolicy partition;
	DDS_TopicDataQosPolicy topic_data;
	DDS_GroupDataQosPolicy group_data;
} DDS_SubscriptionBuiltinTopicData;

DDS_EXPORT DDS_SubscriptionBuiltinTopicData *DDS_SubscriptionBuiltinTopicData__alloc (void);
DDS_EXPORT void DDS_SubscriptionBuiltinTopicData__free (DDS_SubscriptionBuiltinTopicData *data);
DDS_EXPORT void DDS_SubscriptionBuiltinTopicData__init (DDS_SubscriptionBuiltinTopicData *data);
DDS_EXPORT void DDS_SubscriptionBuiltinTopicData__clear (DDS_SubscriptionBuiltinTopicData *data);

/* === DomainParticipantFactory ============================================= */

typedef struct {
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_DomainParticipantFactoryQos;

/* === DomainParticipant ==================================================== */

typedef struct {
	DDS_UserDataQosPolicy user_data;
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_DomainParticipantQos;

#define DDS_PARTICIPANT_QOS_DEFAULT NULL

DDS_EXPORT DDS_DomainParticipantQos *DDS_DomainParticipantQos__alloc (void);
DDS_EXPORT void DDS_DomainParticipantQos__free (DDS_DomainParticipantQos *qos);
DDS_EXPORT void DDS_DomainParticipantQos__init (DDS_DomainParticipantQos *qos);
DDS_EXPORT void DDS_DomainParticipantQos__clear (DDS_DomainParticipantQos *qos);

/* === DomainParticipantListener ============================================ */

typedef struct {
	int total_count;
	int total_count_change;
} DDS_InconsistentTopicStatus;

typedef enum {
	DDS_NOT_REJECTED,
	DDS_REJECTED_BY_INSTANCES_LIMIT,
	DDS_REJECTED_BY_SAMPLES_LIMIT,
	DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT
} DDS_SampleRejectedStatusKind;

typedef struct {
	int total_count;
	int total_count_change;
	DDS_SampleRejectedStatusKind last_reason;
	DDS_InstanceHandle_t last_instance_handle;
} DDS_SampleRejectedStatus;

typedef struct {
	int total_count;
	int total_count_change;
	DDS_QosPolicyId_t last_policy_id;
	DDS_QosPolicyCountSeq policies;
} DDS_IncompatibleQosStatus;

typedef DDS_IncompatibleQosStatus DDS_RequestedIncompatibleQosStatus;
typedef DDS_IncompatibleQosStatus DDS_OfferedIncompatibleQosStatus;

typedef struct {
	int total_count;
	int total_count_change;
} DDS_SampleLostStatus;

typedef struct {
	int total_count;
	int total_count_change;
	DDS_InstanceHandle_t last_instance_handle;
} DDS_OfferedDeadlineMissedStatus;

typedef struct {
	int total_count;
	int total_count_change;
	DDS_InstanceHandle_t last_instance_handle;
} DDS_RequestedDeadlineMissedStatus;

typedef struct {
	int total_count;
	int total_count_change;
	int current_count;
	int current_count_change;
	DDS_InstanceHandle_t last_publication_handle;
} DDS_SubscriptionMatchedStatus;

typedef struct {
	int total_count;
	int total_count_change;
	int current_count;
	int current_count_change;
	DDS_InstanceHandle_t last_subscription_handle;
} DDS_PublicationMatchedStatus;

typedef struct {
	int total_count;
	int total_count_change;
} DDS_LivelinessLostStatus;

typedef struct {
	int alive_count;
	int not_alive_count;
	int alive_count_change;
	int not_alive_count_change;
	DDS_InstanceHandle_t last_publication_handle;
} DDS_LivelinessChangedStatus;

typedef struct DDS_DomainParticipantListener DDS_DomainParticipantListener;

typedef void (*DDS_DomainParticipantListener_on_sample_rejected)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleRejectedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_requested_incompatible_qos)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_offered_incompatible_qos)(
	DDS_DomainParticipantListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_sample_lost)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleLostStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_offered_deadline_missed)(
	DDS_DomainParticipantListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedDeadlineMissedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_subscription_matched)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SubscriptionMatchedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_publication_matched)(
	DDS_DomainParticipantListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_PublicationMatchedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_data_on_readers)(
	DDS_DomainParticipantListener *self,
	DDS_Subscriber the_subscriber
);
typedef void (*DDS_DomainParticipantListener_on_liveliness_changed)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_LivelinessChangedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_data_available)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader
);
typedef void (*DDS_DomainParticipantListener_on_liveliness_lost)(
	DDS_DomainParticipantListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_LivelinessLostStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_inconsistent_topic)(
	DDS_DomainParticipantListener *self,
	DDS_Topic the_topic, /* in (variable length) */
	DDS_InconsistentTopicStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_requested_deadline_missed)(
	DDS_DomainParticipantListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedDeadlineMissedStatus * status
);

struct DDS_DomainParticipantListener {
	DDS_DomainParticipantListener_on_sample_rejected on_sample_rejected;
	DDS_DomainParticipantListener_on_requested_incompatible_qos on_requested_incompatible_qos;
	DDS_DomainParticipantListener_on_offered_incompatible_qos on_offered_incompatible_qos;
	DDS_DomainParticipantListener_on_sample_lost on_sample_lost;
	DDS_DomainParticipantListener_on_offered_deadline_missed on_offered_deadline_missed;
	DDS_DomainParticipantListener_on_subscription_matched on_subscription_matched;
	DDS_DomainParticipantListener_on_publication_matched on_publication_matched;
	DDS_DomainParticipantListener_on_data_on_readers on_data_on_readers;
	DDS_DomainParticipantListener_on_liveliness_changed on_liveliness_changed;
	DDS_DomainParticipantListener_on_data_available on_data_available;
	DDS_DomainParticipantListener_on_liveliness_lost on_liveliness_lost;
	DDS_DomainParticipantListener_on_inconsistent_topic on_inconsistent_topic;
	DDS_DomainParticipantListener_on_requested_deadline_missed on_requested_deadline_missed;
};

/* === Topic ================================================================ */

typedef struct {
	DDS_TopicDataQosPolicy topic_data;
	DDS_DurabilityQosPolicy durability;
	DDS_DurabilityServiceQosPolicy durability_service;
	DDS_DeadlineQosPolicy deadline;
	DDS_LatencyBudgetQosPolicy latency_budget;
	DDS_LivelinessQosPolicy liveliness;
	DDS_ReliabilityQosPolicy reliability;
	DDS_DestinationOrderQosPolicy destination_order;
	DDS_HistoryQosPolicy history;
	DDS_ResourceLimitsQosPolicy resource_limits;
	DDS_TransportPriorityQosPolicy transport_priority;
	DDS_LifespanQosPolicy lifespan;
	DDS_OwnershipQosPolicy ownership;
} DDS_TopicQos;

#define DDS_TOPIC_QOS_DEFAULT NULL

DDS_EXPORT DDS_TopicQos *DDS_TopicQos__alloc (void);
DDS_EXPORT void DDS_TopicQos__free (DDS_TopicQos *qos);
DDS_EXPORT void DDS_TopicQos__init (DDS_TopicQos *qos);
DDS_EXPORT void DDS_TopicQos__clear (DDS_TopicQos *qos);

/* === TopicListener ======================================================== */

typedef struct DDS_TopicListener DDS_TopicListener;

typedef void (*DDS_TopicListener_on_inconsistent_topic) (
	DDS_TopicListener *self,
	DDS_Topic the_topic, /* in (variable length) */
	DDS_InconsistentTopicStatus *status
);

struct DDS_TopicListener {
	DDS_TopicListener_on_inconsistent_topic on_inconsistent_topic;
};

/* === Subscriber =========================================================== */

typedef struct {
	DDS_PresentationQosPolicy presentation;
	DDS_PartitionQosPolicy partition;
	DDS_GroupDataQosPolicy group_data;
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_SubscriberQos;

#define DDS_SUBSCRIBER_QOS_DEFAULT NULL

DDS_EXPORT DDS_SubscriberQos *DDS_SubscriberQos__alloc (void);
DDS_EXPORT void DDS_SubscriberQos__free (DDS_SubscriberQos *qos);
DDS_EXPORT void DDS_SubscriberQos__init (DDS_SubscriberQos *qos);
DDS_EXPORT void DDS_SubscriberQos__clear (DDS_SubscriberQos *qos);

/* === SubscriberListener -------============================================ */

typedef struct DDS_SubscriberListener DDS_SubscriberListener;

typedef	void (*DDS_SubscriberListener_on_sample_rejected) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleRejectedStatus *status
);
typedef	void (*DDS_SubscriberListener_on_requested_incompatible_qos) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus *status
);
typedef	void (*DDS_SubscriberListener_on_sample_lost) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleLostStatus *status
);
typedef	void (*DDS_SubscriberListener_on_subscription_matched) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SubscriptionMatchedStatus *status
);
typedef	void (*DDS_SubscriberListener_on_data_available) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader
);
typedef	void (*DDS_SubscriberListener_on_liveliness_changed) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_LivelinessChangedStatus *status
);
typedef	void (*DDS_SubscriberListener_on_data_on_readers) (
	DDS_SubscriberListener *self,
	DDS_Subscriber the_subscriber
);
typedef	void (*DDS_SubscriberListener_on_requested_deadline_missed) (
	DDS_SubscriberListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedDeadlineMissedStatus *status
);

struct DDS_SubscriberListener {
	DDS_SubscriberListener_on_sample_rejected on_sample_rejected;
	DDS_SubscriberListener_on_requested_incompatible_qos on_requested_incompatible_qos;
	DDS_SubscriberListener_on_sample_lost on_sample_lost;
	DDS_SubscriberListener_on_subscription_matched on_subscription_matched;
	DDS_SubscriberListener_on_data_available on_data_available;
	DDS_SubscriberListener_on_liveliness_changed on_liveliness_changed;
	DDS_SubscriberListener_on_data_on_readers on_data_on_readers;
	DDS_SubscriberListener_on_requested_deadline_missed on_requested_deadline_missed;
};

/* === Publisher ============================================================ */

typedef struct {
	DDS_PresentationQosPolicy presentation;
	DDS_PartitionQosPolicy partition;
	DDS_GroupDataQosPolicy group_data;
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_PublisherQos;

#define DDS_PUBLISHER_QOS_DEFAULT NULL

DDS_EXPORT DDS_PublisherQos *DDS_PublisherQos__alloc (void);
DDS_EXPORT void DDS_PublisherQos__free (DDS_PublisherQos *qos);
DDS_EXPORT void DDS_PublisherQos__init (DDS_PublisherQos *qos);
DDS_EXPORT void DDS_PublisherQos__clear (DDS_PublisherQos *qos);

/* === PublisherListener ==================================================== */

typedef struct DDS_PublisherListener DDS_PublisherListener;

typedef void (*DDS_PublisherListener_on_offered_deadline_missed) (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedDeadlineMissedStatus *status
);
typedef void (*DDS_PublisherListener_on_publication_matched) (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_PublicationMatchedStatus *status
);
typedef void (*DDS_PublisherListener_on_liveliness_lost) (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_LivelinessLostStatus *status
);
typedef void (*DDS_PublisherListener_on_offered_incompatible_qos) (
	DDS_PublisherListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus *status
);

struct DDS_PublisherListener {
	DDS_PublisherListener_on_offered_deadline_missed on_offered_deadline_missed;
	DDS_PublisherListener_on_publication_matched on_publication_matched;
	DDS_PublisherListener_on_liveliness_lost on_liveliness_lost;
	DDS_PublisherListener_on_offered_incompatible_qos on_offered_incompatible_qos;
};

/* === DataReader =========================================================== */

typedef struct {
	DDS_DurabilityQosPolicy durability;
	DDS_DeadlineQosPolicy deadline;
	DDS_LatencyBudgetQosPolicy latency_budget;
	DDS_LivelinessQosPolicy liveliness;
	DDS_ReliabilityQosPolicy reliability;
	DDS_DestinationOrderQosPolicy destination_order;
	DDS_HistoryQosPolicy history;
	DDS_ResourceLimitsQosPolicy resource_limits;
	DDS_UserDataQosPolicy user_data;
	DDS_OwnershipQosPolicy ownership;
	DDS_TimeBasedFilterQosPolicy time_based_filter;
	DDS_ReaderDataLifecycleQosPolicy reader_data_lifecycle;
} DDS_DataReaderQos;

#define DDS_DATAREADER_QOS_DEFAULT NULL
#define DDS_DATAREADER_QOS_USE_TOPIC_QOS ((DDS_DataReaderQos *) 1UL)

DDS_EXPORT DDS_DataReaderQos *DDS_DataReaderQos__alloc (void);
DDS_EXPORT void DDS_DataReaderQos__free (DDS_DataReaderQos *qos);
DDS_EXPORT void DDS_DataReaderQos__init (DDS_DataReaderQos *qos);
DDS_EXPORT void DDS_DataReaderQos__clear (DDS_DataReaderQos *qos);

/* === DataReaderListener =================================================== */

typedef struct DDS_DataReaderListener DDS_DataReaderListener;

typedef void (*DDS_DataReaderListener_on_subscription_matched)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SubscriptionMatchedStatus *status
);

typedef void (*DDS_DataReaderListener_on_sample_rejected)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleRejectedStatus *status
);

typedef void (*DDS_DataReaderListener_on_data_available)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader
);

typedef void (*DDS_DataReaderListener_on_liveliness_changed)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_LivelinessChangedStatus *status
);

typedef void (*DDS_DataReaderListener_on_requested_incompatible_qos)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus *status
);

typedef void (*DDS_DataReaderListener_on_sample_lost)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleLostStatus *status
);

typedef void (*DDS_DataReaderListener_on_requested_deadline_missed)(
	DDS_DataReaderListener *self,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedDeadlineMissedStatus *status
);

struct DDS_DataReaderListener {
	DDS_DataReaderListener_on_sample_rejected on_sample_rejected;
	DDS_DataReaderListener_on_liveliness_changed on_liveliness_changed;
	DDS_DataReaderListener_on_requested_deadline_missed on_requested_deadline_missed;
	DDS_DataReaderListener_on_requested_incompatible_qos on_requested_incompatible_qos;
	DDS_DataReaderListener_on_data_available on_data_available;
	DDS_DataReaderListener_on_subscription_matched on_subscription_matched;
	DDS_DataReaderListener_on_sample_lost on_sample_lost;
	void *cookie;
};

/* === DataWriter =========================================================== */

typedef struct {
	DDS_DurabilityQosPolicy durability;
	DDS_DurabilityServiceQosPolicy durability_service;
	DDS_DeadlineQosPolicy deadline;
	DDS_LatencyBudgetQosPolicy latency_budget;
	DDS_LivelinessQosPolicy liveliness;
	DDS_ReliabilityQosPolicy reliability;
	DDS_DestinationOrderQosPolicy destination_order;
	DDS_HistoryQosPolicy history;
	DDS_ResourceLimitsQosPolicy resource_limits;
	DDS_TransportPriorityQosPolicy transport_priority;
	DDS_LifespanQosPolicy lifespan;
	DDS_UserDataQosPolicy user_data;
	DDS_OwnershipQosPolicy ownership;
	DDS_OwnershipStrengthQosPolicy ownership_strength;
	DDS_WriterDataLifecycleQosPolicy writer_data_lifecycle;
} DDS_DataWriterQos;

#define DDS_DATAWRITER_QOS_DEFAULT NULL
#define DDS_DATAWRITER_QOS_USE_TOPIC_QOS ((DDS_DataWriterQos *) 1UL)

DDS_EXPORT DDS_DataWriterQos *DDS_DataWriterQos__alloc (void);
DDS_EXPORT void DDS_DataWriterQos__free (DDS_DataWriterQos *qos);
DDS_EXPORT void DDS_DataWriterQos__init (DDS_DataWriterQos *qos);
DDS_EXPORT void DDS_DataWriterQos__clear (DDS_DataWriterQos *qos);

/* === DataWriterListener =================================================== */

typedef struct DDS_DataWriterListener DDS_DataWriterListener;

typedef void (*DDS_DataWriterListener_on_offered_deadline_missed) (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedDeadlineMissedStatus *status
);
typedef void (*DDS_DataWriterListener_on_publication_matched) (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_PublicationMatchedStatus *status
);
typedef void (*DDS_DataWriterListener_on_liveliness_lost) (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_LivelinessLostStatus *status
);
typedef void (*DDS_DataWriterListener_on_offered_incompatible_qos) (
	DDS_DataWriterListener *self,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus *status
);

struct DDS_DataWriterListener {
	DDS_DataWriterListener_on_offered_deadline_missed on_offered_deadline_missed;
	DDS_DataWriterListener_on_publication_matched on_publication_matched;
	DDS_DataWriterListener_on_liveliness_lost on_liveliness_lost;
	DDS_DataWriterListener_on_offered_incompatible_qos on_offered_incompatible_qos;
	void *cookie;
};

/* === Entity methods ====================================================== */

DDS_EXPORT DDS_StatusCondition DDS_Entity_get_statuscondition(
	DDS_Entity self
);

DDS_EXPORT DDS_StatusMask DDS_Entity_get_status_changes(
	DDS_Entity self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Entity_enable(
	DDS_Entity self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_Entity_get_instance_handle(
	DDS_Entity self
);

/* === WaitSet ============================================================== */

DDS_SEQUENCE (DDS_Condition, DDS_ConditionSeq);

DDS_EXPORT DDS_ConditionSeq *DDS_ConditionSeq__alloc (void);
DDS_EXPORT void DDS_ConditionSeq__free (DDS_ConditionSeq *conditions);
DDS_EXPORT void DDS_ConditionSeq__init (DDS_ConditionSeq *conditions);
DDS_EXPORT void DDS_ConditionSeq__clear (DDS_ConditionSeq *conditions);

DDS_EXPORT DDS_WaitSet DDS_WaitSet__alloc (void);

DDS_EXPORT void DDS_WaitSet__free (
	DDS_WaitSet ws
);

DDS_EXPORT DDS_ReturnCode_t DDS_WaitSet_attach_condition (
	DDS_WaitSet ws,
	DDS_Condition c
);

DDS_EXPORT DDS_ReturnCode_t DDS_WaitSet_detach_condition (
	DDS_WaitSet ws,
	DDS_Condition c
);

DDS_EXPORT DDS_ReturnCode_t DDS_WaitSet_wait (
	DDS_WaitSet ws,
	DDS_ConditionSeq *conditions,
	DDS_Duration_t *timeout
);

DDS_EXPORT DDS_ReturnCode_t DDS_WaitSet_get_conditions (
	DDS_WaitSet ws,
	DDS_ConditionSeq *conditions
);

/* === Conditions =========================================================== */

DDS_EXPORT int DDS_Condition_get_trigger_value (
	DDS_Condition c
);


DDS_EXPORT DDS_GuardCondition DDS_GuardCondition__alloc (void);

DDS_EXPORT void DDS_GuardCondition__free(
	DDS_GuardCondition gc
);

DDS_EXPORT int DDS_GuardCondition_get_trigger_value(
	DDS_GuardCondition gc
);

DDS_EXPORT DDS_ReturnCode_t DDS_GuardCondition_set_trigger_value(
	DDS_GuardCondition gc,
	int value
);


DDS_EXPORT int DDS_StatusCondition_get_trigger_value(
	DDS_StatusCondition self
);

DDS_EXPORT DDS_ReturnCode_t DDS_StatusCondition_set_enabled_statuses(
	DDS_StatusCondition self,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusMask DDS_StatusCondition_get_enabled_statuses(
	DDS_StatusCondition self
);

DDS_EXPORT DDS_Entity DDS_StatusCondition_get_entity(
	DDS_StatusCondition self
);


DDS_EXPORT int DDS_ReadCondition_get_trigger_value(
	DDS_ReadCondition self
);

DDS_EXPORT DDS_DataReader DDS_ReadCondition_get_datareader(
	DDS_ReadCondition self
);

DDS_EXPORT DDS_ViewStateMask DDS_ReadCondition_get_view_state_mask(
	DDS_ReadCondition self
);

DDS_EXPORT DDS_InstanceStateMask DDS_ReadCondition_get_instance_state_mask(
	DDS_ReadCondition self
);

DDS_EXPORT DDS_SampleStateMask DDS_ReadCondition_get_sample_state_mask(
	DDS_ReadCondition self
);


DDS_EXPORT int DDS_QueryCondition_get_trigger_value(
	DDS_QueryCondition self
);

DDS_EXPORT DDS_ReturnCode_t DDS_QueryCondition_set_query_parameters(
	DDS_QueryCondition self,
	DDS_StringSeq *query_parameters
);

DDS_EXPORT DDS_ReturnCode_t DDS_QueryCondition_get_query_parameters(
	DDS_QueryCondition self,
	DDS_StringSeq *query_parameters
);

DDS_EXPORT const char *DDS_QueryCondition_get_query_expression(
	DDS_QueryCondition self
);

DDS_EXPORT DDS_ViewStateMask DDS_QueryCondition_get_view_state_mask(
	DDS_QueryCondition self
);

DDS_EXPORT DDS_DataReader DDS_QueryCondition_get_datareader(
	DDS_QueryCondition self
);

DDS_EXPORT DDS_SampleStateMask DDS_QueryCondition_get_sample_state_mask(
	DDS_QueryCondition self
);

DDS_EXPORT DDS_InstanceStateMask DDS_QueryCondition_get_instance_state_mask(
	DDS_QueryCondition self
);

/* === DomainParticipantFactory methods ===================================== */

DDS_EXPORT DDS_DomainParticipant DDS_DomainParticipantFactory_create_participant(
	DDS_DomainId_t domain_id,
	const DDS_DomainParticipantQos *qos,
	const DDS_DomainParticipantListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_participant(
	DDS_DomainParticipant a_participant
);

DDS_EXPORT DDS_DomainParticipant DDS_DomainParticipantFactory_lookup_participant(
	DDS_DomainId_t domain_id
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos(
	DDS_DomainParticipantQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipantFactory_get_default_participant_qos(
	DDS_DomainParticipantQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipantFactory_get_qos(
	DDS_DomainParticipantFactoryQos * qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos(
	DDS_DomainParticipantFactoryQos * qos
);

/* === TypeSupport methods ================================================== */

DDS_EXPORT const char *DDS_TypeSupport_get_type_name (
	const DDS_TypeSupport ts
);

DDS_EXPORT void DDS_TypeSupport_data_free (
	const DDS_TypeSupport ts,
	void *data,
	int full
);

DDS_EXPORT void *DDS_TypeSupport_data_copy (
	const DDS_TypeSupport ts,
	const void *data
);

DDS_EXPORT int DDS_TypeSupport_data_equals (
	DDS_TypeSupport ts,
	const void *data,
	const void *other
);

/* === DomainParticipant methods ============================================ */

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_register_type(
	DDS_DomainParticipant self,
	DDS_TypeSupport ts,
	const char *type_name
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_unregister_type(
	DDS_DomainParticipant self,
	DDS_TypeSupport ts,
	const char *type_name
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_typesupport(
	DDS_DomainParticipant self,
	DDS_TypeSupport ts
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_qos(
	DDS_DomainParticipant self,
	DDS_DomainParticipantQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_set_qos(
	DDS_DomainParticipant self,
	DDS_DomainParticipantQos *qos
);

DDS_EXPORT DDS_DomainParticipantListener *DDS_DomainParticipant_get_listener(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_set_listener(
	DDS_DomainParticipant self,
	DDS_DomainParticipantListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusCondition DDS_DomainParticipant_get_statuscondition(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_StatusMask DDS_DomainParticipant_get_status_changes(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_enable(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DomainParticipant_get_instance_handle(
	DDS_DomainParticipant a_participant
);

DDS_EXPORT DDS_Publisher DDS_DomainParticipant_create_publisher(
	DDS_DomainParticipant self,
	const DDS_PublisherQos *qos,
	const DDS_PublisherListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_publisher(
	DDS_DomainParticipant self,
	DDS_Publisher a_publisher
);

DDS_EXPORT DDS_Subscriber DDS_DomainParticipant_create_subscriber(
	DDS_DomainParticipant self,
	const DDS_SubscriberQos *qos,
	const DDS_SubscriberListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_subscriber(
	DDS_DomainParticipant self,
	DDS_Subscriber a_subscriber
);

DDS_EXPORT DDS_Topic DDS_DomainParticipant_create_topic(
	DDS_DomainParticipant self,
	const char *topic_name,
	const char *type_name,
	const DDS_TopicQos *qos,
	const DDS_TopicListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_topic(
	DDS_DomainParticipant self,
	DDS_Topic a_topic
);

DDS_EXPORT DDS_ContentFilteredTopic DDS_DomainParticipant_create_contentfilteredtopic(
	DDS_DomainParticipant self,
	const char *name,
	DDS_Topic related_topic,
	const char *filter_expression,
	DDS_StringSeq *expression_parameters
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_contentfilteredtopic(
	DDS_DomainParticipant self,
	DDS_ContentFilteredTopic contentfilteredtopic
);

DDS_EXPORT DDS_MultiTopic DDS_DomainParticipant_create_multitopic(
	DDS_DomainParticipant self,
	const char *name,
	const char *type_name,
	const char *subscription_expression,
	DDS_StringSeq *expression_parameters
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_multitopic(
	DDS_DomainParticipant self,
	DDS_MultiTopic topic
);

DDS_EXPORT DDS_Topic DDS_DomainParticipant_find_topic(
	DDS_DomainParticipant self,
	const char *topic_name,
	DDS_Duration_t *timeout
);

DDS_EXPORT DDS_TopicDescription DDS_DomainParticipant_lookup_topicdescription(
	DDS_DomainParticipant self,
	const char *topic_name
);

DDS_EXPORT DDS_Subscriber DDS_DomainParticipant_get_builtin_subscriber(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_ignore_participant(
	DDS_DomainParticipant self,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_ignore_topic(
	DDS_DomainParticipant self,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_ignore_publication(
	DDS_DomainParticipant self,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_ignore_subscription(
	DDS_DomainParticipant self,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_DomainId_t DDS_DomainParticipant_get_domain_id(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_delete_contained_entities(
	DDS_DomainParticipant a_participant
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_assert_liveliness(
	DDS_DomainParticipant self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_set_default_publisher_qos(
	DDS_DomainParticipant self,
	DDS_PublisherQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_default_publisher_qos(
	DDS_DomainParticipant self,
	DDS_PublisherQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_set_default_subscriber_qos(
	DDS_DomainParticipant self,
	DDS_SubscriberQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_default_subscriber_qos(
	DDS_DomainParticipant self,
	DDS_SubscriberQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_set_default_topic_qos(
	DDS_DomainParticipant self,
	DDS_TopicQos * qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_default_topic_qos(
	DDS_DomainParticipant self,
	DDS_TopicQos * qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_participants(
	DDS_DomainParticipant self,
	DDS_InstanceHandleSeq *participant_handles
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_participant_data(
	DDS_DomainParticipant self,
	DDS_ParticipantBuiltinTopicData *participant_data,
	DDS_InstanceHandle_t participant_handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_topics(
	DDS_DomainParticipant self,
	DDS_InstanceHandleSeq *topic_handles
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_topic_data(
	DDS_DomainParticipant self,
	DDS_TopicBuiltinTopicData *topic_data,
	DDS_InstanceHandle_t topic_handle
);

DDS_EXPORT int DDS_DomainParticipant_contains_entity(
	DDS_DomainParticipant _o,
	DDS_InstanceHandle_t a_handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DomainParticipant_get_current_time(
	DDS_DomainParticipant self,
	DDS_Time_t *current_time
);

/* === Topic Description methods ============================================ */

DDS_EXPORT DDS_DomainParticipant DDS_TopicDescription_get_participant(
	DDS_TopicDescription self
);

DDS_EXPORT const char *DDS_TopicDescription_get_type_name(
	DDS_TopicDescription self
);

DDS_EXPORT const char *DDS_TopicDescription_get_name(
	DDS_TopicDescription self
);

/* === Topic methods ======================================================== */

DDS_EXPORT DDS_ReturnCode_t DDS_Topic_get_qos(
	DDS_Topic self,
	DDS_TopicQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Topic_set_qos(
	DDS_Topic self,
	DDS_TopicQos *qos
);

DDS_EXPORT DDS_TopicListener *DDS_Topic_get_listener(
	DDS_Topic self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Topic_set_listener(
	DDS_Topic self,
	DDS_TopicListener *listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusCondition DDS_Topic_get_statuscondition(
	DDS_Topic self
);

DDS_EXPORT DDS_StatusMask DDS_Topic_get_status_changes(
	DDS_Topic self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Topic_enable(
	DDS_Topic self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_Topic_get_instance_handle(
	DDS_Topic self
);

DDS_EXPORT DDS_DomainParticipant DDS_Topic_get_participant(
	DDS_Topic self
);

DDS_EXPORT const char *DDS_Topic_get_type_name(
	DDS_Topic self
);

DDS_EXPORT const char *DDS_Topic_get_name(
	DDS_Topic self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Topic_get_inconsistent_topic_status(
	DDS_Topic self,
	DDS_InconsistentTopicStatus *a_status
);

DDS_EXPORT DDS_TopicDescription DDS_Topic_get_topicdescription (
	DDS_Topic self
);

/* === ContentFilteredTopic methods ========================================= */

DDS_EXPORT DDS_Topic DDS_ContentFilteredTopic_get_related_topic(
	DDS_ContentFilteredTopic self
);

DDS_EXPORT DDS_ReturnCode_t DDS_ContentFilteredTopic_get_expression_parameters(
	DDS_ContentFilteredTopic self,
	DDS_StringSeq * expression_parameters
);

DDS_EXPORT DDS_ReturnCode_t DDS_ContentFilteredTopic_set_expression_parameters(
	DDS_ContentFilteredTopic self,
	DDS_StringSeq *expression_parameters
);

DDS_EXPORT const char *DDS_ContentFilteredTopic_get_filter_expression(
	DDS_ContentFilteredTopic self
);

DDS_EXPORT DDS_DomainParticipant DDS_ContentFilteredTopic_get_participant(
	DDS_ContentFilteredTopic self
);

DDS_EXPORT const char *DDS_ContentFilteredTopic_get_type_name(
	DDS_ContentFilteredTopic self
);

DDS_EXPORT const char *DDS_ContentFilteredTopic_get_name(
	DDS_ContentFilteredTopic self
);

DDS_EXPORT DDS_TopicDescription DDS_ContentFilteredTopic_get_topicdescription (
	DDS_ContentFilteredTopic self
);

/* === Publisher methods ==================================================== */

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_get_qos(
	DDS_Publisher self,
	DDS_PublisherQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_set_qos(
	DDS_Publisher self,
	DDS_PublisherQos *qos
);

DDS_EXPORT DDS_PublisherListener *DDS_Publisher_get_listener(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_set_listener(
	DDS_Publisher self,
	DDS_PublisherListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusCondition DDS_Publisher_get_statuscondition(
	DDS_Publisher self
);

DDS_EXPORT DDS_StatusMask DDS_Publisher_get_status_changes(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_enable(
	DDS_Publisher self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_Publisher_get_instance_handle(
	DDS_Publisher self
);

DDS_EXPORT DDS_DataWriter DDS_Publisher_create_datawriter(
	DDS_Publisher self,
	DDS_Topic a_topic,
	const DDS_DataWriterQos *qos,
	const DDS_DataWriterListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_delete_datawriter(
	DDS_Publisher self,
	DDS_DataWriter a_datawriter
);

DDS_EXPORT DDS_DataWriter DDS_Publisher_lookup_datawriter(
	DDS_Publisher self,
	const char *topic_name
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_suspend_publications(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_resume_publications(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_begin_coherent_changes(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_end_coherent_changes(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_wait_for_acknowledgments(
	DDS_Publisher self,
	DDS_Duration_t *max_wait
);

DDS_EXPORT DDS_DomainParticipant DDS_Publisher_get_participant(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_delete_contained_entities(
	DDS_Publisher self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos(
	DDS_Publisher self,
	DDS_DataWriterQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_get_default_datawriter_qos(
	DDS_Publisher self,
	DDS_DataWriterQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Publisher_copy_from_topic_qos(
	DDS_Publisher self,
	DDS_DataWriterQos *datawriter_qos,
	DDS_TopicQos *topic_qos
);

/* === DataWriter methods =================================================== */

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_qos(
	DDS_DataWriter self,
	DDS_DataWriterQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_set_qos(
	DDS_DataWriter self,
	DDS_DataWriterQos *qos
);

DDS_EXPORT DDS_DataWriterListener *DDS_DataWriter_get_listener(
	DDS_DataWriter self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_set_listener(
	DDS_DataWriter self,
	DDS_DataWriterListener *listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusCondition DDS_DataWriter_get_statuscondition(
	DDS_DataWriter self
);

DDS_EXPORT DDS_StatusMask DDS_DataWriter_get_status_changes(
	DDS_DataWriter self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_enable(
	DDS_DataWriter self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DataWriter_get_instance_handle(
	DDS_DataWriter self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DataWriter_register_instance(
	DDS_DataWriter self,
	const void *instance_data
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DataWriter_register_instance_w_timestamp(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_unregister_instance(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_unregister_instance_w_timestamp(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_unregister_instance_directed(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_unregister_instance_w_timestamp_directed(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_key_value(
	DDS_DataWriter self,
	void *key_data,
	const DDS_InstanceHandle_t h
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DataWriter_lookup_instance(
	DDS_DataWriter self,
	const void *key_data
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_write(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_write_w_timestamp(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_write_directed (
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_write_w_timestamp_directed (
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_dispose(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_dispose_w_timestamp(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_dispose_directed(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_dispose_w_timestamp_directed(
	DDS_DataWriter self,
	const void *instance_data,
	const DDS_InstanceHandle_t handle,
	const DDS_Time_t *timestamp,
	DDS_InstanceHandleSeq *destinations
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_wait_for_acknowledgments(
	DDS_DataWriter self,
	const DDS_Duration_t *max_wait
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_liveliness_lost_status(
	DDS_DataWriter self,
	DDS_LivelinessLostStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_offered_deadline_missed_status(
	DDS_DataWriter self,
	DDS_OfferedDeadlineMissedStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_offered_incompatible_qos_status(
	DDS_DataWriter self,
	DDS_OfferedIncompatibleQosStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_publication_matched_status(
	DDS_DataWriter self,
	DDS_PublicationMatchedStatus *status
);

DDS_EXPORT DDS_Topic DDS_DataWriter_get_topic(
        DDS_DataWriter self
);

DDS_EXPORT DDS_Publisher DDS_DataWriter_get_publisher(
	DDS_DataWriter self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_assert_liveliness (
	DDS_DataWriter self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_matched_subscription_data(
	DDS_DataWriter self,
	DDS_SubscriptionBuiltinTopicData *subscription_data,
	DDS_InstanceHandle_t subscription_handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_matched_subscriptions(
	DDS_DataWriter self,
	DDS_InstanceHandleSeq *subscription_handles
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataWriter_get_reply_subscriptions(
	DDS_DataWriter self,
	DDS_InstanceHandle_t publication_handle,
	DDS_InstanceHandleSeq *subscription_handles
);

/* === Subscriber methods =================================================== */

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_get_qos(
	DDS_Subscriber self,
	DDS_SubscriberQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_set_qos(
	DDS_Subscriber self,
	DDS_SubscriberQos *qos
);

DDS_EXPORT DDS_SubscriberListener *DDS_Subscriber_get_listener(
	DDS_Subscriber self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_set_listener(
	DDS_Subscriber self,
	DDS_SubscriberListener *listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusCondition DDS_Subscriber_get_statuscondition(
	DDS_Subscriber self
);

DDS_EXPORT DDS_StatusMask DDS_Subscriber_get_status_changes(
	DDS_Subscriber self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_enable(
	DDS_Subscriber self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_Subscriber_get_instance_handle(
	DDS_Subscriber self
);

DDS_EXPORT DDS_DataReader DDS_Subscriber_create_datareader(
	DDS_Subscriber self,
	DDS_TopicDescription a_topic,
	const DDS_DataReaderQos *qos,
	const DDS_DataReaderListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_delete_datareader(
	DDS_Subscriber self,
	DDS_DataReader a_datareader
);

DDS_EXPORT DDS_DataReader DDS_Subscriber_lookup_datareader(
	DDS_Subscriber self,
	const char *topic_name
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_begin_access (
	DDS_Subscriber self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_end_access (
	DDS_Subscriber self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_get_datareaders(
	DDS_Subscriber self,
	DDS_DataReaderSeq *readers,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_notify_datareaders(
	DDS_Subscriber self
);

DDS_EXPORT DDS_DomainParticipant DDS_Subscriber_get_participant(
	DDS_Subscriber self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_delete_contained_entities(
	DDS_Subscriber self
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos(
	DDS_Subscriber self,
	DDS_DataReaderQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_get_default_datareader_qos(
	DDS_Subscriber self,
	DDS_DataReaderQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_Subscriber_copy_from_topic_qos(
	DDS_Subscriber self,
	DDS_DataReaderQos *datareader_qos,
	DDS_TopicQos *topic_qos
);

/* === DataReader methods =================================================== */

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_qos(
	DDS_DataReader self,
	DDS_DataReaderQos *qos
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_set_qos(
	DDS_DataReader self,
	DDS_DataReaderQos *qos
);

DDS_EXPORT DDS_DataReaderListener *DDS_DataReader_get_listener(
	DDS_DataReader self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_set_listener(
	DDS_DataReader self,
	const DDS_DataReaderListener *a_listener,
	DDS_StatusMask mask
);

DDS_EXPORT DDS_StatusCondition DDS_DataReader_get_statuscondition(
	DDS_DataReader self
);

DDS_EXPORT DDS_StatusMask DDS_DataReader_get_status_changes(
	DDS_DataReader self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_enable(
	DDS_DataReader self
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DataReader_get_instance_handle(
	DDS_DataReader self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_read(
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_take(
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_read_w_condition (
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_Condition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_take_w_condition (
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_Condition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_read_next_sample (
	DDS_DataReader self,
	void *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_take_next_sample (
	DDS_DataReader self,
	void *data_value,
	DDS_SampleInfo *sample_info
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_read_instance (
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_take_instance (
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_read_next_instance (
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_take_next_instance (
	DDS_DataReader r,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_read_next_instance_w_condition (
	DDS_DataReader r,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_Condition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_take_next_instance_w_condition (
	DDS_DataReader r,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq,
	unsigned max_samples,
	DDS_InstanceHandle_t handle,
	DDS_Condition condition
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_return_loan (
	DDS_DataReader self,
	DDS_DataSeq *received_data,
	DDS_SampleInfoSeq *info_seq
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_key_value (
	DDS_DataReader self,
	void *data,
	DDS_InstanceHandle_t handle
);

DDS_EXPORT DDS_InstanceHandle_t DDS_DataReader_lookup_instance (
	DDS_DataReader self,
	const void *key_data
);

DDS_EXPORT DDS_ReadCondition DDS_DataReader_create_readcondition (
	DDS_DataReader self,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states
);

DDS_EXPORT DDS_QueryCondition DDS_DataReader_create_querycondition (
	DDS_DataReader self,
	DDS_SampleStateMask sample_states,
	DDS_ViewStateMask view_states,
	DDS_InstanceStateMask instance_states,
	const char *query_expression,
	DDS_StringSeq *query_parameters
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_delete_readcondition (
	DDS_DataReader self,
	DDS_Condition cond
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_liveliness_changed_status(
	DDS_DataReader self,
	DDS_LivelinessChangedStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_requested_deadline_missed_status(
	DDS_DataReader self,
	DDS_RequestedDeadlineMissedStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_requested_incompatible_qos_status(
	DDS_DataReader self,
	DDS_RequestedIncompatibleQosStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_sample_lost_status(
	DDS_DataReader self,
	DDS_SampleLostStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_sample_rejected_status(
	DDS_DataReader self,
	DDS_SampleRejectedStatus *status
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_subscription_matched_status(
	DDS_DataReader self,
	DDS_SubscriptionMatchedStatus *status
);

DDS_EXPORT DDS_TopicDescription DDS_DataReader_get_topicdescription (
	DDS_DataReader self
);

DDS_EXPORT DDS_Subscriber DDS_DataReader_get_subscriber (
	DDS_DataReader self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_delete_contained_entities(
	DDS_DataReader self
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_wait_for_historical_data(
	DDS_DataReader self,
	DDS_Duration_t *max_wait
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_matched_publication_data(
	DDS_DataReader self,
	DDS_PublicationBuiltinTopicData *publication_data,
	DDS_InstanceHandle_t publication_handle
);

DDS_EXPORT DDS_ReturnCode_t DDS_DataReader_get_matched_publications(
	DDS_DataReader self,
	DDS_InstanceHandleSeq *publication_handles
);

#ifdef  __cplusplus
}
#endif

#endif /* DDS_DCPS_H_ */

