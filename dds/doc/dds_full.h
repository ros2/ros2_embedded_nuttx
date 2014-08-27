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

/* dds.h -- C-language binding for the DDS framework. */

#ifndef __dds_h_
#define __dds_h_

#include "dds_types.h"

typedef DDS_DOMAINID DDS_DomainId_t;
typedef DDS_HANDLE DDS_InstanceHandle_t;

#define DDS_HANDLE_NIL	0L
#define DDS_LENGTH_UNLIMITED	-1L

typedef struct {
	DDS_BUILTIN_TOPIC_TYPE value [3];
} DDS_BuiltinTopicKey_t;

DDS_SEQUENCE (DDS_InstanceHandle_t DDS_InstanceHandleSeq);

typedef int DDS_ReturnCode_t;
typedef int DDS_QosPolicyId_t;

DDS_SEQUENCE (char *, DDS_StringSeq);

typedef struct {
	int		sec;
	unsigned	nanosec;
} DDS_Duration_t;

typedef struct {
	int		sec;
	unsigned	nanosec;
} DDS_Time_t;

typedef uint32_t DDS_StatusKind;
typedef uint32_t DDS_StatusMask;

#define DDS_DURATION_INFINITE_SEC	0x7fffffffL
#define DDS_DURATION_INFINITE_NSEC	0x7fffffffUL
#define DDS_DURATION_ZERO_SEC		0L
#define DDS_DURATION_ZERO_NSEC		0UL

#define DDS_TIME_INVALID_SEC		-1L
#define DDS_TIME_INVALID_NSEC		0xffffffffUL

#define DDS_RETCODE_OK				0L
#define DDS_RETCODE_ERROR			1L
#define DDS_RETCODE_UNSUPPORTED			2L
#define DDS_RETCODE_BAD_PARAMETER		3L
#define DDS_RETCODE_PRECONDITION_NOT_MET	4L
#define DDS_RETCODE_OUT_OF_RESOURCES		5L
#define DDS_RETCODE_NOT_ENABLED			6L
#define DDS_RETCODE_IMMUTABLE_POLICY		7L
#define DDS_RETCODE_INCONSISTENT_POLICY		8L
#define DDS_RETCODE_ALREADY_DELETED		9L
#define DDS_RETCODE_TIMEOUT			10L
#define DDS_RETCODE_NO_DATA			11L
#define DDS_RETCODE_ILLEGAL_OPERATION		12L

#define DDS_INCONSISTENT_TOPIC_STATUS		(0x00000001UL << 0)
#define DDS_OFFERED_DEADLINE_MISSED_STATUS	(0x00000001UL << 1)
#define DDS_REQUESTED_DEADLINE_MISSED_STATUS	(0x00000001UL << 2)
#define DDS_OFFERED_INCOMPATIBLE_QOS_STATUS	(0x00000001UL << 5)
#define DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS	(0x00000001UL << 6)
#define DDS_SAMPLE_LOST_STATUS			(0x00000001UL << 7)
#define DDS_SAMPLE_REJECTED_STATUS		(0x00000001UL << 8)
#define DDS_DATA_ON_READERS_STATUS		(0x00000001UL << 9)
#define DDS_DATA_AVAILABLE_STATUS		(0x00000001UL << 10)
#define DDS_LIVELINESS_LOST_STATUS		(0x00000001UL << 11)
#define DDS_LIVELINESS_CHANGED_STATUS		(0x00000001UL << 12)
#define DDS_PUBLICATION_MATCHED_STATUS		(0x00000001UL << 13)
#define DDS_SUBSCRIPTION_MATCHED_STATUS		(0x00000001UL << 14)

typedef struct {
	int		total_count;
	int		total_count_change;
} DDS_InconsistentTopicStatus;

typedef struct {
	int		total_count;
	int		total_count_change;
} DDS_SampleLostStatus;

typedef enum  {
	DDS_NOT_REJECTED,
	DDS_REJECTED_BY_INSTANCES_LIMIT,
	DDS_REJECTED_BY_SAMPLES_LIMIT,
	DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT
} DDS_SampleRejectedStatusKind;

typedef struct {
	int				total_count;
	int				total_count_change;
	DDS_SampleRejectedStatusKind	last_reason;
	DDS_InstanceHandle_t		last_instance_handle;
} DDS_SampleRejectedStatus;

typedef struct {
	int		total_count;
	int		total_count_change;
} DDS_LivelinessLostStatus;

typedef struct {
	int			alive_count;
	int			not_alive_count;
	int			alive_count_change;
	int			not_alive_count_change;
	DDS_InstanceHandle_t	last_publication_handle;
} DDS_LivelinessChangedStatus;

typedef struct {
	int			total_count;
	int			total_count_change;
	DDS_InstanceHandle_t	last_instance_handle;
} DDS_OfferedDeadlineMissedStatus;

typedef struct {
	int			total_count;
	int			total_count_change;
	DDS_InstanceHandle_t	last_instance_handle;
} DDS_RequestedDeadlineMissedStatus;

typedef struct {
	DDS_QosPolicyId_t	policy_id;
	int			count;
} DDS_QosPolicyCount;

DDS_SEQUENCE (DDS_QosPolicyCount, DDS_QosPolicyCountSeq);

typedef struct {
	int			total_count;
	int			total_count_change;
	DDS_QosPolicyId_t	last_policy_id;
	DDS_QosPolicyCountSeq	policies;
} DDS_OfferedIncompatibleQosStatus;

typedef struct {
	int			total_count;
	int			total_count_change;
	DDS_QosPolicyId_t	last_policy_id;
	DDS_QosPolicyCountSeq	policies;
} DDS_RequestedIncompatibleQosStatus;

typedef struct {
	int			total_count;
	int			total_count_change;
	int			current_count;
	int			current_count_change;
	DDS_InstanceHandle_t	last_subscription_handle;
} DDS_PublicationMatchedStatus;

typedef struct {
	int			total_count;
	int			total_count_change;
	int			current_count;
	int			current_count_change;
	DDS_InstanceHandle_t	last_publication_handle;
} DDS_SubscriptionMatchedStatus;

#define DDS_Object void *

typedef DDS_Object DDS_Listener;
typedef DDS_Object DDS_Entity;
typedef DDS_Object DDS_TopicDescription;
typedef DDS_Object DDS_Topic;
typedef DDS_Object DDS_ContentFilteredTopic;
typedef DDS_Object DDS_MultiTopic;
typedef DDS_Object DDS_DataWriter;
typedef DDS_Object DDS_DataReader;
typedef DDS_Object DDS_Subscriber;
typedef DDS_Object DDS_Publisher;

DDS_SEQUENCE (DDS_DataReader, DDS_DataReaderSeq);

 /***********************************************/
 /*   DDS Listener interface			*/
 /***********************************************/

typedef struct DDS_TopicListener {
	void (*on_inconsistent_topic) (DDS_TopicListener _o,
				       DDS_Topic the_topic, /* in (variable length) */
				       DDS_InconsistentTopicStatus *status);
} DDS_TopicListener;

typedef struct DDS_DataWriterListener
	void (*on_offered_deadline_missed) (DDS_DataWriterListener _o,
					    DDS_DataWriter writer, /* in (variable length) */
					    DDS_OfferedDeadlineMissedStatus *status);
	void (*on_publication_matched) (DDS_DataWriterListener _o,
					DDS_DataWriter writer, /* in (variable length) */
					DDS_PublicationMatchedStatus *status);
	void (*on_liveliness_lost) (DDS_DataWriterListener _o,
				    DDS_DataWriter writer, /* in (variable length) */
				    DDS_LivelinessLostStatus *status);
	void (*on_offered_incompatible_qos) (DDS_DataWriterListener _o,
					     DDS_DataWriter writer, /* in (variable length) */
					     DDS_OfferedIncompatibleQosStatus *status);
} DDS_DataWriterListener;

typedef struct DDS_PublisherListener;
	void (*on_offered_deadline_missed) (DDS_PublisherListener _o,
					    DDS_DataWriter writer, /* in (variable length) */
					    DDS_OfferedDeadlineMissedStatus *status);
	void (*on_publication_matched) (DDS_PublisherListener _o,
					DDS_DataWriter writer, /* in (variable length) */
					DDS_PublicationMatchedStatus *status);
	void (*on_liveliness_lost) (DDS_PublisherListener _o,
				    DDS_DataWriter writer, /* in (variable length) */
				    DDS_LivelinessLostStatus *status);
	void (*on_offered_incompatible_qos) (DDS_PublisherListener _o,
					     DDS_DataWriter writer, /* in (variable length) */
					     DDS_OfferedIncompatibleQosStatus *status);
} DDS_PublisherListener;


typedef struct DDS_DataReaderListener {
	void (*on_subscription_matched) (DDS_DataReaderListener _o,
	         			DDS_DataReader the_reader, /* in (variable length) */
	         			DDS_SubscriptionMatchedStatus *status);
	void (*on_sample_rejected) (DDS_DataReaderListener _o,
	         		   DDS_DataReader the_reader, /* in (variable length) */
	         		   DDS_SampleRejectedStatus *status);
	void (*on_data_available) (DDS_DataReaderListener _o,
	         		  DDS_DataReader the_reader);
	void (*on_liveliness_changed) (DDS_DataReaderListener _o,
				       DDS_DataReader the_reader, /* in (variable length) */
				       DDS_LivelinessChangedStatus *status);
	void (*on_requested_incompatible_qos) (DDS_DataReaderListener _o,
					       DDS_DataReader the_reader, /* in (variable length) */
					       DDS_RequestedIncompatibleQosStatus *status);
	void (*on_sample_lost) (DDS_DataReaderListener _o,
				DDS_DataReader the_reader, /* in (variable length) */
				DDS_SampleLostStatus * status);
	void (*on_requested_deadline_missed) (DDS_DataReaderListener _o,
					      DDS_DataReader the_reader, /* in (variable length) */
					      DDS_RequestedDeadlineMissedStatus *status);
} DDS_DataReaderListener;

typedef struct DDS_SubscriberListener {
	void (*on_sample_rejected) (DDS_SubscriberListener _o,
				    DDS_DataReader the_reader, /* in (variable length) */
				    DDS_SampleRejectedStatus *status);
	void (*on_requested_incompatible_qos) (DDS_SubscriberListener _o,
					       DDS_DataReader the_reader, /* in (variable length) */
					       DDS_RequestedIncompatibleQosStatus *status);
	void (*on_sample_lost) (DDS_SubscriberListener _o,
				DDS_DataReader the_reader, /* in (variable length) */
				DDS_SampleLostStatus *status);
	void (*on_subscription_matched) (DDS_SubscriberListener _o,
					 DDS_DataReader the_reader, /* in (variable length) */
					 DDS_SubscriptionMatchedStatus *status);
	void (*on_data_available) (DDS_SubscriberListener _o,
				   DDS_DataReader the_reader);
	void (*on_liveliness_changed) (DDS_SubscriberListener _o,
				       DDS_DataReader the_reader, /* in (variable length) */
				       DDS_LivelinessChangedStatus *status);
	void (*on_data_on_readers) (DDS_SubscriberListener _o,
				    DDS_Subscriber the_subscriber);
	void (*on_requested_deadline_missed) (DDS_SubscriberListener _o,
					      DDS_DataReader the_reader, /* in (variable length) */
					      DDS_RequestedDeadlineMissedStatus *status);
} DDS_SubscriberListener;

/*
 * begin of interface DDS_DomainParticipantListener
 */
#ifndef _DDS_DomainParticipantListener_defined
#define _DDS_DomainParticipantListener_defined

typedef struct DDS_DomainParticipantListener *DDS_DomainParticipantListener;

#endif

#ifndef _proto_DDS_DomainParticipantListener_defined
#define _proto_DDS_DomainParticipantListener_defined

typedef void (*DDS_DomainParticipantListener_on_sample_rejected)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleRejectedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_requested_incompatible_qos)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedIncompatibleQosStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_offered_incompatible_qos)(
	DDS_DomainParticipantListener _o,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedIncompatibleQosStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_sample_lost)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SampleLostStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_offered_deadline_missed)(
	DDS_DomainParticipantListener _o,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_OfferedDeadlineMissedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_subscription_matched)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_SubscriptionMatchedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_publication_matched)(
	DDS_DomainParticipantListener _o,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_PublicationMatchedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_data_on_readers)(
	DDS_DomainParticipantListener _o,
	DDS_Subscriber the_subscriber
);
typedef void (*DDS_DomainParticipantListener_on_liveliness_changed)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_LivelinessChangedStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_data_available)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader
);
typedef void (*DDS_DomainParticipantListener_on_liveliness_lost)(
	DDS_DomainParticipantListener _o,
	DDS_DataWriter writer, /* in (variable length) */
	DDS_LivelinessLostStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_inconsistent_topic)(
	DDS_DomainParticipantListener _o,
	DDS_Topic the_topic, /* in (variable length) */
	DDS_InconsistentTopicStatus * status
);
typedef void (*DDS_DomainParticipantListener_on_requested_deadline_missed)(
	DDS_DomainParticipantListener _o,
	DDS_DataReader the_reader, /* in (variable length) */
	DDS_RequestedDeadlineMissedStatus * status
);
#endif
/*
 * end of interface DDS_DomainParticipantListener
 */
/*
 * begin of interface DDS_Condition
 */
#ifndef _DDS_Condition_defined
#define _DDS_Condition_defined

typedef DDS_Object DDS_Condition;

#endif

#ifndef _proto_DDS_Condition_defined
#define _proto_DDS_Condition_defined

extern bool DDS_Condition_get_trigger_value(
	DDS_Condition _o
);
#endif
/*
 * end of interface DDS_Condition
 */
#ifndef _DDS_sequence_DDS_Condition_defined
#define _DDS_sequence_DDS_Condition_defined
typedef struct {
	uint32_t _maximum;
	uint32_t _length;
	DDS_Condition * _buffer;
} DDS_sequence_DDS_Condition;
extern DDS_sequence_DDS_Condition * DDS_sequence_DDS_Condition__alloc(uint32_t nb);
extern DDS_Condition * DDS_sequence_DDS_Condition__allocbuf(uint32_t len);
#endif
typedef DDS_sequence_DDS_Condition DDS_ConditionSeq;
/*
 * begin of interface DDS_WaitSet
 */
#ifndef _DDS_WaitSet_defined
#define _DDS_WaitSet_defined

typedef DDS_Object DDS_WaitSet;

#endif

#ifndef _proto_DDS_WaitSet_defined
#define _proto_DDS_WaitSet_defined

extern DDS_ReturnCode_t DDS_WaitSet_detach_condition(
	DDS_WaitSet _o,
	DDS_Condition cond
);
extern DDS_ReturnCode_t DDS_WaitSet_wait(
	DDS_WaitSet _o,
	DDS_ConditionSeq * active_conditions, /* inout (variable length) */
	DDS_Duration_t * timeout
);
extern DDS_ReturnCode_t DDS_WaitSet_get_conditions(
	DDS_WaitSet _o,
	DDS_ConditionSeq * attached_conditions
);
extern DDS_ReturnCode_t DDS_WaitSet_attach_condition(
	DDS_WaitSet _o,
	DDS_Condition cond
);
#endif
/*
 * end of interface DDS_WaitSet
 */
/*
 * begin of interface DDS_GuardCondition
 */
#ifndef _DDS_GuardCondition_defined
#define _DDS_GuardCondition_defined

typedef DDS_Object DDS_GuardCondition;

#endif

#ifndef _proto_DDS_GuardCondition_defined
#define _proto_DDS_GuardCondition_defined

extern bool DDS_GuardCondition_get_trigger_value(
	DDS_GuardCondition _o
);
extern DDS_ReturnCode_t DDS_GuardCondition_set_trigger_value(
	DDS_GuardCondition _o,
	bool value
);
#endif
/*
 * end of interface DDS_GuardCondition
 */
/*
 * begin of interface DDS_StatusCondition
 */
#ifndef _DDS_StatusCondition_defined
#define _DDS_StatusCondition_defined

typedef DDS_Object DDS_StatusCondition;

#endif

#ifndef _proto_DDS_StatusCondition_defined
#define _proto_DDS_StatusCondition_defined

extern bool DDS_StatusCondition_get_trigger_value(
	DDS_StatusCondition _o
);
extern DDS_ReturnCode_t DDS_StatusCondition_set_enabled_statuses(
	DDS_StatusCondition _o,
	DDS_StatusMask mask
);
extern DDS_Entity DDS_StatusCondition_get_entity(
	DDS_StatusCondition _o
);
extern DDS_StatusMask DDS_StatusCondition_get_enabled_statuses(
	DDS_StatusCondition _o
);
#endif
/*
 * end of interface DDS_StatusCondition
 */
typedef uint32_t DDS_SampleStateKind;
#define DDS_READ_SAMPLE_STATE	(0x00000001UL << 0)
#define DDS_NOT_READ_SAMPLE_STATE	(0x00000001UL << 1)
typedef uint32_t DDS_SampleStateMask;
#define DDS_ANY_SAMPLE_STATE	0x0000ffffUL
typedef uint32_t DDS_ViewStateKind;
#define DDS_NEW_VIEW_STATE	(0x00000001UL << 0)
#define DDS_NOT_NEW_VIEW_STATE	(0x00000001UL << 1)
typedef uint32_t DDS_ViewStateMask;
#define DDS_ANY_VIEW_STATE	0x0000ffffUL
typedef uint32_t DDS_InstanceStateKind;
#define DDS_ALIVE_INSTANCE_STATE	(0x00000001UL << 0)
#define DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE	(0x00000001UL << 1)
#define DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE	(0x00000001UL << 2)
typedef uint32_t DDS_InstanceStateMask;
#define DDS_ANY_INSTANCE_STATE	0x0000ffffUL
#define DDS_NOT_ALIVE_INSTANCE_STATE	0x00000006UL
/*
 * begin of interface DDS_ReadCondition
 */
#ifndef _DDS_ReadCondition_defined
#define _DDS_ReadCondition_defined

typedef DDS_Object DDS_ReadCondition;

#endif

#ifndef _proto_DDS_ReadCondition_defined
#define _proto_DDS_ReadCondition_defined

extern bool DDS_ReadCondition_get_trigger_value(
	DDS_ReadCondition _o
);
extern DDS_DataReader DDS_ReadCondition_get_datareader(
	DDS_ReadCondition _o
);
extern DDS_ViewStateMask DDS_ReadCondition_get_view_state_mask(
	DDS_ReadCondition _o
);
extern DDS_InstanceStateMask DDS_ReadCondition_get_instance_state_mask(
	DDS_ReadCondition _o
);
extern DDS_SampleStateMask DDS_ReadCondition_get_sample_state_mask(
	DDS_ReadCondition _o
);
#endif
/*
 * end of interface DDS_ReadCondition
 */
/*
 * begin of interface DDS_QueryCondition
 */
#ifndef _DDS_QueryCondition_defined
#define _DDS_QueryCondition_defined

typedef DDS_Object DDS_QueryCondition;

#endif

#ifndef _proto_DDS_QueryCondition_defined
#define _proto_DDS_QueryCondition_defined

extern bool DDS_QueryCondition_get_trigger_value(
	DDS_QueryCondition _o
);
extern DDS_ReturnCode_t DDS_QueryCondition_set_query_parameters(
	DDS_QueryCondition _o,
	DDS_StringSeq * query_parameters
);
extern DDS_ReturnCode_t DDS_QueryCondition_get_query_parameters(
	DDS_QueryCondition _o,
	DDS_StringSeq * query_parameters
);
extern DDS_string DDS_QueryCondition_get_query_expression(
	DDS_QueryCondition _o
);
extern DDS_ViewStateMask DDS_QueryCondition_get_view_state_mask(
	DDS_QueryCondition _o
);
extern DDS_DataReader DDS_QueryCondition_get_datareader(
	DDS_QueryCondition _o
);
extern DDS_SampleStateMask DDS_QueryCondition_get_sample_state_mask(
	DDS_QueryCondition _o
);
extern DDS_InstanceStateMask DDS_QueryCondition_get_instance_state_mask(
	DDS_QueryCondition _o
);
#endif
/*
 * end of interface DDS_QueryCondition
 */
#define DDS_USERDATA_QOS_POLICY_NAME	"UserData"
#define DDS_DURABILITY_QOS_POLICY_NAME	"Durability"
#define DDS_PRESENTATION_QOS_POLICY_NAME	"Presentation"
#define DDS_DEADLINE_QOS_POLICY_NAME	"Deadline"
#define DDS_LATENCYBUDGET_QOS_POLICY_NAME	"LatencyBudget"
#define DDS_OWNERSHIP_QOS_POLICY_NAME	"Ownership"
#define DDS_OWNERSHIPSTRENGTH_QOS_POLICY_NAME	"OwnershipStrength"
#define DDS_LIVELINESS_QOS_POLICY_NAME	"Liveliness"
#define DDS_TIMEBASEDFILTER_QOS_POLICY_NAME	"TimeBasedFilter"
#define DDS_PARTITION_QOS_POLICY_NAME	"Partition"
#define DDS_RELIABILITY_QOS_POLICY_NAME	"Reliability"
#define DDS_DESTINATIONORDER_QOS_POLICY_NAME	"DestinationOrder"
#define DDS_HISTORY_QOS_POLICY_NAME	"History"
#define DDS_RESOURCELIMITS_QOS_POLICY_NAME	"ResourceLimits"
#define DDS_ENTITYFACTORY_QOS_POLICY_NAME	"EntityFactory"
#define DDS_WRITERDATALIFECYCLE_QOS_POLICY_NAME	"WriterDataLifecycle"
#define DDS_READERDATALIFECYCLE_QOS_POLICY_NAME	"ReaderDataLifecycle"
#define DDS_TOPICDATA_QOS_POLICY_NAME	"TopicData"
#define DDS_GROUPDATA_QOS_POLICY_NAME	"TransportPriority"
#define DDS_LIFESPAN_QOS_POLICY_NAME	"Lifespan"
#define DDS_DURABILITYSERVICE_POLICY_NAME	"DurabilityService"
#define DDS_INVALID_QOS_POLICY_ID	0L
#define DDS_USERDATA_QOS_POLICY_ID	1L
#define DDS_DURABILITY_QOS_POLICY_ID	2L
#define DDS_PRESENTATION_QOS_POLICY_ID	3L
#define DDS_DEADLINE_QOS_POLICY_ID	4L
#define DDS_LATENCYBUDGET_QOS_POLICY_ID	5L
#define DDS_OWNERSHIP_QOS_POLICY_ID	6L
#define DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID	7L
#define DDS_LIVELINESS_QOS_POLICY_ID	8L
#define DDS_TIMEBASEDFILTER_QOS_POLICY_ID	9L
#define DDS_PARTITION_QOS_POLICY_ID	10L
#define DDS_RELIABILITY_QOS_POLICY_ID	11L
#define DDS_DESTINATIONORDER_QOS_POLICY_ID	12L
#define DDS_HISTORY_QOS_POLICY_ID	13L
#define DDS_RESOURCELIMITS_QOS_POLICY_ID	14L
#define DDS_ENTITYFACTORY_QOS_POLICY_ID	15L
#define DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID	16L
#define DDS_READERDATALIFECYCLE_QOS_POLICY_ID	17L
#define DDS_TOPICDATA_QOS_POLICY_ID	18L
#define DDS_GROUPDATA_QOS_POLICY_ID	19L
#define DDS_TRANSPORTPRIORITY_QOS_POLICY_ID	20L
#define DDS_LIFESPAN_QOS_POLICY_ID	21L
#define DDS_DURABILITYSERVICE_QOS_POLICY_ID	22L
#ifndef _DDS_sequence_octet_defined
#define _DDS_sequence_octet_defined
typedef struct {
	uint32_t _maximum;
	uint32_t _length;
	uint8_t * _buffer;
} DDS_sequence_octet;
extern DDS_sequence_octet * DDS_sequence_octet__alloc(uint32_t nb);
extern uint8_t * DDS_sequence_octet__allocbuf(uint32_t len);
#endif
typedef struct {
	DDS_sequence_octet value;
} DDS_UserDataQosPolicy;
extern DDS_UserDataQosPolicy * DDS_UserDataQosPolicy__alloc(uint32_t nb);
typedef struct {
	DDS_sequence_octet value;
} DDS_TopicDataQosPolicy;
extern DDS_TopicDataQosPolicy * DDS_TopicDataQosPolicy__alloc(uint32_t nb);
typedef struct {
	DDS_sequence_octet value;
} DDS_GroupDataQosPolicy;
extern DDS_GroupDataQosPolicy * DDS_GroupDataQosPolicy__alloc(uint32_t nb);
typedef struct {
	int32_t value;
} DDS_TransportPriorityQosPolicy;
typedef struct {
	DDS_Duration_t duration;
} DDS_LifespanQosPolicy;
/* enum DDS_DurabilityQosPolicyKind */
#define DDS_DurabilityQosPolicyKind uint32_t
#define DDS_VOLATILE_DURABILITY_QOS	0
#define DDS_TRANSIENT_LOCAL_DURABILITY_QOS	1
#define DDS_TRANSIENT_DURABILITY_QOS	2
#define DDS_PERSISTENT_DURABILITY_QOS	3

typedef struct {
	DDS_DurabilityQosPolicyKind kind;
} DDS_DurabilityQosPolicy;
/* enum DDS_PresentationQosPolicyAccessScopeKind */
#define DDS_PresentationQosPolicyAccessScopeKind uint32_t
#define DDS_INSTANCE_PRESENTATION_QOS	0
#define DDS_TOPIC_PRESENTATION_QOS	1
#define DDS_GROUP_PRESENTATION_QOS	2

typedef struct {
	DDS_PresentationQosPolicyAccessScopeKind access_scope;
	bool coherent_access;
	bool ordered_access;
} DDS_PresentationQosPolicy;
typedef struct {
	DDS_Duration_t period;
} DDS_DeadlineQosPolicy;
typedef struct {
	DDS_Duration_t duration;
} DDS_LatencyBudgetQosPolicy;
/* enum DDS_OwnershipQosPolicyKind */
#define DDS_OwnershipQosPolicyKind uint32_t
#define DDS_SHARED_OWNERSHIP_QOS	0
#define DDS_EXCLUSIVE_OWNERSHIP_QOS	1

typedef struct {
	DDS_OwnershipQosPolicyKind kind;
} DDS_OwnershipQosPolicy;
typedef struct {
	int32_t value;
} DDS_OwnershipStrengthQosPolicy;
/* enum DDS_LivelinessQosPolicyKind */
#define DDS_LivelinessQosPolicyKind uint32_t
#define DDS_AUTOMATIC_LIVELINESS_QOS	0
#define DDS_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS	1
#define DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS	2

typedef struct {
	DDS_LivelinessQosPolicyKind kind;
	DDS_Duration_t lease_duration;
} DDS_LivelinessQosPolicy;
typedef struct {
	DDS_Duration_t minimum_separation;
} DDS_TimeBasedFilterQosPolicy;
typedef struct {
	DDS_StringSeq name;
} DDS_PartitionQosPolicy;
extern DDS_PartitionQosPolicy * DDS_PartitionQosPolicy__alloc(uint32_t nb);
/* enum DDS_ReliabilityQosPolicyKind */
#define DDS_ReliabilityQosPolicyKind uint32_t
#define DDS_BEST_EFFORT_RELIABILITY_QOS	0
#define DDS_RELIABLE_RELIABILITY_QOS	1

typedef struct {
	DDS_ReliabilityQosPolicyKind kind;
	DDS_Duration_t max_blocking_time;
} DDS_ReliabilityQosPolicy;
/* enum DDS_DestinationOrderQosPolicyKind */
#define DDS_DestinationOrderQosPolicyKind uint32_t
#define DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS	0
#define DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS	1

typedef struct {
	DDS_DestinationOrderQosPolicyKind kind;
} DDS_DestinationOrderQosPolicy;
/* enum DDS_HistoryQosPolicyKind */
#define DDS_HistoryQosPolicyKind uint32_t
#define DDS_KEEP_LAST_HISTORY_QOS	0
#define DDS_KEEP_ALL_HISTORY_QOS	1

typedef struct {
	DDS_HistoryQosPolicyKind kind;
	int32_t depth;
} DDS_HistoryQosPolicy;
typedef struct {
	int32_t max_samples;
	int32_t max_instances;
	int32_t max_samples_per_instance;
} DDS_ResourceLimitsQosPolicy;
typedef struct {
	bool autoenable_created_entities;
} DDS_EntityFactoryQosPolicy;
typedef struct {
	bool autodispose_unregistered_instances;
} DDS_WriterDataLifecycleQosPolicy;
typedef struct {
	DDS_Duration_t autopurge_nowriter_samples_delay;
	DDS_Duration_t autopurge_disposed_samples_delay;
} DDS_ReaderDataLifecycleQosPolicy;
typedef struct {
	DDS_Duration_t service_cleanup_delay;
	DDS_HistoryQosPolicyKind history_kind;
	int32_t history_depth;
	int32_t max_samples;
	int32_t max_instances;
	int32_t max_samples_per_instance;
} DDS_DurabilityServiceQosPolicy;
typedef struct {
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_DomainParticipantFactoryQos;
typedef struct {
	DDS_UserDataQosPolicy user_data;
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_DomainParticipantQos;
extern DDS_DomainParticipantQos * DDS_DomainParticipantQos__alloc(uint32_t nb);
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
extern DDS_TopicQos * DDS_TopicQos__alloc(uint32_t nb);
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
extern DDS_DataWriterQos * DDS_DataWriterQos__alloc(uint32_t nb);
typedef struct {
	DDS_PresentationQosPolicy presentation;
	DDS_PartitionQosPolicy partition;
	DDS_GroupDataQosPolicy group_data;
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_PublisherQos;
extern DDS_PublisherQos * DDS_PublisherQos__alloc(uint32_t nb);
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
extern DDS_DataReaderQos * DDS_DataReaderQos__alloc(uint32_t nb);
typedef struct {
	DDS_PresentationQosPolicy presentation;
	DDS_PartitionQosPolicy partition;
	DDS_GroupDataQosPolicy group_data;
	DDS_EntityFactoryQosPolicy entity_factory;
} DDS_SubscriberQos;
extern DDS_SubscriberQos * DDS_SubscriberQos__alloc(uint32_t nb);
typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_UserDataQosPolicy user_data;
} DDS_ParticipantBuiltinTopicData;
extern DDS_ParticipantBuiltinTopicData * DDS_ParticipantBuiltinTopicData__alloc(uint32_t nb);
typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_string name;
	DDS_string type_name;
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
extern DDS_TopicBuiltinTopicData * DDS_TopicBuiltinTopicData__alloc(uint32_t nb);
typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_BuiltinTopicKey_t participant_key;
	DDS_string topic_name;
	DDS_string type_name;
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
extern DDS_PublicationBuiltinTopicData * DDS_PublicationBuiltinTopicData__alloc(uint32_t nb);
typedef struct {
	DDS_BuiltinTopicKey_t key;
	DDS_BuiltinTopicKey_t participant_key;
	DDS_string topic_name;
	DDS_string type_name;
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
extern DDS_SubscriptionBuiltinTopicData * DDS_SubscriptionBuiltinTopicData__alloc(uint32_t nb);
/*
 * begin of interface DDS_Entity
 */
#ifndef _DDS_Entity_defined
#define _DDS_Entity_defined

//typedef DDS_Object DDS_Entity;

#endif

#ifndef _proto_DDS_Entity_defined
#define _proto_DDS_Entity_defined

extern DDS_StatusMask DDS_Entity_get_status_changes(
	DDS_Entity _o
);
extern DDS_ReturnCode_t DDS_Entity_enable(
	DDS_Entity _o
);
extern DDS_StatusCondition DDS_Entity_get_statuscondition(
	DDS_Entity _o
);
extern DDS_InstanceHandle_t DDS_Entity_get_instance_handle(
	DDS_Entity _o
);
#endif
/*
 * end of interface DDS_Entity
 */
/*
 * begin of interface DDS_DomainParticipant
 */
#ifndef _DDS_DomainParticipant_defined
#define _DDS_DomainParticipant_defined

typedef struct DDS_DomainParticipant *DDS_DomainParticipant;

#endif

#ifndef _proto_DDS_DomainParticipant_defined
#define _proto_DDS_DomainParticipant_defined

extern DDS_ReturnCode_t DDS_DomainParticipant_ignore_participant(
	DDS_DomainParticipant _o,
	DDS_InstanceHandle_t handle
);
extern DDS_ReturnCode_t DDS_DomainParticipant_set_default_topic_qos(
	DDS_DomainParticipant _o,
	DDS_TopicQos * qos
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_qos(
	DDS_DomainParticipant _o,
	DDS_DomainParticipantQos * qos
);
extern DDS_ReturnCode_t DDS_DomainParticipant_delete_contentfilteredtopic(
	DDS_DomainParticipant _o,
	DDS_ContentFilteredTopic a_contentfilteredtopic
);
extern DDS_Publisher DDS_DomainParticipant_create_publisher(
	DDS_DomainParticipant _o,
	DDS_PublisherQos * qos, /* in (variable length) */
	DDS_PublisherListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_Subscriber DDS_DomainParticipant_create_subscriber(
	DDS_DomainParticipant _o,
	DDS_SubscriberQos * qos, /* in (variable length) */
	DDS_SubscriberListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_ReturnCode_t DDS_DomainParticipant_set_default_subscriber_qos(
	DDS_DomainParticipant _o,
	DDS_SubscriberQos * qos
);
extern bool DDS_DomainParticipant_contains_entity(
	DDS_DomainParticipant _o,
	DDS_InstanceHandle_t a_handle
);
extern DDS_ReturnCode_t DDS_DomainParticipant_ignore_subscription(
	DDS_DomainParticipant _o,
	DDS_InstanceHandle_t handle
);
extern DDS_ReturnCode_t DDS_DomainParticipant_set_qos(
	DDS_DomainParticipant _o,
	DDS_DomainParticipantQos * qos
);
extern DDS_InstanceHandle_t DDS_DomainParticipant_get_instance_handle(
	DDS_DomainParticipant _o
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_current_time(
	DDS_DomainParticipant _o,
	DDS_Time_t * current_time
);
extern DDS_StatusCondition DDS_DomainParticipant_get_statuscondition(
	DDS_DomainParticipant _o
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_topics(
	DDS_DomainParticipant _o,
	DDS_InstanceHandleSeq * topic_handles
);
extern DDS_Subscriber DDS_DomainParticipant_get_builtin_subscriber(
	DDS_DomainParticipant _o
);
extern DDS_Topic DDS_DomainParticipant_find_topic(
	DDS_DomainParticipant _o,
	DDS_string topic_name, /* in (variable length) */
	DDS_Duration_t * timeout
);
extern DDS_ReturnCode_t DDS_DomainParticipant_ignore_topic(
	DDS_DomainParticipant _o,
	DDS_InstanceHandle_t handle
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_participants(
	DDS_DomainParticipant _o,
	DDS_InstanceHandleSeq * participant_handles
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_topic_data(
	DDS_DomainParticipant _o,
	DDS_TopicBuiltinTopicData * topic_data, /* inout (variable length) */
	DDS_InstanceHandle_t topic_handle
);
extern DDS_ReturnCode_t DDS_DomainParticipant_set_default_publisher_qos(
	DDS_DomainParticipant _o,
	DDS_PublisherQos * qos
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_discovered_participant_data(
	DDS_DomainParticipant _o,
	DDS_ParticipantBuiltinTopicData * participant_data, /* inout (variable length) */
	DDS_InstanceHandle_t participant_handle
);
extern DDS_DomainId_t DDS_DomainParticipant_get_domain_id(
	DDS_DomainParticipant _o
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_default_topic_qos(
	DDS_DomainParticipant _o,
	DDS_TopicQos * qos
);
extern DDS_StatusMask DDS_DomainParticipant_get_status_changes(
	DDS_DomainParticipant _o
);
extern DDS_MultiTopic DDS_DomainParticipant_create_multitopic(
	DDS_DomainParticipant _o,
	DDS_string name, /* in (variable length) */
	DDS_string type_name, /* in (variable length) */
	DDS_string subscription_expression, /* in (variable length) */
	DDS_StringSeq * expression_parameters
);
extern DDS_ReturnCode_t DDS_DomainParticipant_enable(
	DDS_DomainParticipant _o
);
extern DDS_ReturnCode_t DDS_DomainParticipant_delete_multitopic(
	DDS_DomainParticipant _o,
	DDS_MultiTopic a_multitopic
);
extern DDS_ReturnCode_t DDS_DomainParticipant_delete_contained_entities(
	DDS_DomainParticipant _o
);
extern DDS_Topic DDS_DomainParticipant_create_topic(
	DDS_DomainParticipant _o,
	DDS_string topic_name, /* in (variable length) */
	DDS_string type_name, /* in (variable length) */
	DDS_TopicQos * qos, /* in (variable length) */
	DDS_TopicListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_ReturnCode_t DDS_DomainParticipant_ignore_publication(
	DDS_DomainParticipant _o,
	DDS_InstanceHandle_t handle
);
extern DDS_ContentFilteredTopic DDS_DomainParticipant_create_contentfilteredtopic(
	DDS_DomainParticipant _o,
	DDS_string name, /* in (variable length) */
	DDS_Topic related_topic, /* in (variable length) */
	DDS_string filter_expression, /* in (variable length) */
	DDS_StringSeq * expression_parameters
);
extern DDS_DomainParticipantListener DDS_DomainParticipant_get_listener(
	DDS_DomainParticipant _o
);
extern DDS_ReturnCode_t DDS_DomainParticipant_assert_liveliness(
	DDS_DomainParticipant _o
);
extern DDS_TopicDescription DDS_DomainParticipant_lookup_topicdescription(
	DDS_DomainParticipant _o,
	DDS_string name
);
extern DDS_ReturnCode_t DDS_DomainParticipant_delete_publisher(
	DDS_DomainParticipant _o,
	DDS_Publisher p
);
extern DDS_ReturnCode_t DDS_DomainParticipant_delete_subscriber(
	DDS_DomainParticipant _o,
	DDS_Subscriber s
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_default_publisher_qos(
	DDS_DomainParticipant _o,
	DDS_PublisherQos * qos
);
extern DDS_ReturnCode_t DDS_DomainParticipant_delete_topic(
	DDS_DomainParticipant _o,
	DDS_Topic a_topic
);
extern DDS_ReturnCode_t DDS_DomainParticipant_set_listener(
	DDS_DomainParticipant _o,
	DDS_DomainParticipantListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_ReturnCode_t DDS_DomainParticipant_get_default_subscriber_qos(
	DDS_DomainParticipant _o,
	DDS_SubscriberQos * qos
);
#endif
/*
 * end of interface DDS_DomainParticipant
 */
/*
 * begin of interface DDS_DomainParticipantFactory
 */
#ifndef _DDS_DomainParticipantFactory_defined
#define _DDS_DomainParticipantFactory_defined

typedef DDS_Object DDS_DomainParticipantFactory;

#endif

#ifndef _proto_DDS_DomainParticipantFactory_defined
#define _proto_DDS_DomainParticipantFactory_defined

extern DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos(
	DDS_DomainParticipantFactory _o,
	DDS_DomainParticipantQos * qos
);
extern DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_participant(
	DDS_DomainParticipantFactory _o,
	DDS_DomainParticipant a_participant
);
extern DDS_ReturnCode_t DDS_DomainParticipantFactory_get_qos(
	DDS_DomainParticipantFactory _o,
	DDS_DomainParticipantFactoryQos * qos
);
extern DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos(
	DDS_DomainParticipantFactory _o,
	DDS_DomainParticipantFactoryQos * qos
);
extern DDS_DomainParticipant DDS_DomainParticipantFactory_lookup_participant(
	DDS_DomainParticipantFactory _o,
	DDS_DomainId_t domain_id
);
extern DDS_ReturnCode_t DDS_DomainParticipantFactory_get_default_participant_qos(
	DDS_DomainParticipantFactory _o,
	DDS_DomainParticipantQos * qos
);
extern DDS_DomainParticipant DDS_DomainParticipantFactory_create_participant(
	DDS_DomainParticipantFactory _o,
	DDS_DomainId_t domain_id, /* in (fixed length) */
	DDS_DomainParticipantQos * qos, /* in (variable length) */
	DDS_DomainParticipantListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
#endif
/*
 * end of interface DDS_DomainParticipantFactory
 */
/*
 * begin of interface DDS_TypeSupport
 */
#ifndef _DDS_TypeSupport_defined
#define _DDS_TypeSupport_defined

typedef DDS_Object DDS_TypeSupport;

#endif

/*
 * end of interface DDS_TypeSupport
 */
/*
 * begin of interface DDS_TopicDescription
 */
#ifndef _DDS_TopicDescription_defined
#define _DDS_TopicDescription_defined

//typedef DDS_Object DDS_TopicDescription;

#endif

#ifndef _proto_DDS_TopicDescription_defined
#define _proto_DDS_TopicDescription_defined

extern DDS_DomainParticipant DDS_TopicDescription_get_participant(
	DDS_TopicDescription _o
);
extern DDS_string DDS_TopicDescription_get_name(
	DDS_TopicDescription _o
);
extern DDS_string DDS_TopicDescription_get_type_name(
	DDS_TopicDescription _o
);
#endif
/*
 * end of interface DDS_TopicDescription
 */
/*
 * begin of interface DDS_Topic
 */
#ifndef _DDS_Topic_defined
#define _DDS_Topic_defined

//typedef DDS_Object DDS_Topic;

#endif

#ifndef _proto_DDS_Topic_defined
#define _proto_DDS_Topic_defined

extern DDS_DomainParticipant DDS_Topic_get_participant(
	DDS_Topic _o
);
extern DDS_StatusMask DDS_Topic_get_status_changes(
	DDS_Topic _o
);
extern DDS_ReturnCode_t DDS_Topic_enable(
	DDS_Topic _o
);
extern DDS_ReturnCode_t DDS_Topic_set_qos(
	DDS_Topic _o,
	DDS_TopicQos * qos
);
extern DDS_InstanceHandle_t DDS_Topic_get_instance_handle(
	DDS_Topic _o
);
extern DDS_string DDS_Topic_get_type_name(
	DDS_Topic _o
);
extern DDS_TopicListener DDS_Topic_get_listener(
	DDS_Topic _o
);
extern DDS_StatusCondition DDS_Topic_get_statuscondition(
	DDS_Topic _o
);
extern DDS_ReturnCode_t DDS_Topic_get_qos(
	DDS_Topic _o,
	DDS_TopicQos * qos
);
extern DDS_ReturnCode_t DDS_Topic_get_inconsistent_topic_status(
	DDS_Topic _o,
	DDS_InconsistentTopicStatus * a_status
);
extern DDS_ReturnCode_t DDS_Topic_set_listener(
	DDS_Topic _o,
	DDS_TopicListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_string DDS_Topic_get_name(
	DDS_Topic _o
);
#endif
/*
 * end of interface DDS_Topic
 */
/*
 * begin of interface DDS_ContentFilteredTopic
 */
#ifndef _DDS_ContentFilteredTopic_defined
#define _DDS_ContentFilteredTopic_defined

//typedef DDS_Object DDS_ContentFilteredTopic;

#endif

#ifndef _proto_DDS_ContentFilteredTopic_defined
#define _proto_DDS_ContentFilteredTopic_defined

extern DDS_DomainParticipant DDS_ContentFilteredTopic_get_participant(
	DDS_ContentFilteredTopic _o
);
extern DDS_Topic DDS_ContentFilteredTopic_get_related_topic(
	DDS_ContentFilteredTopic _o
);
extern DDS_ReturnCode_t DDS_ContentFilteredTopic_get_expression_parameters(
	DDS_ContentFilteredTopic _o,
	DDS_StringSeq * expression_parameters
);
extern DDS_string DDS_ContentFilteredTopic_get_filter_expression(
	DDS_ContentFilteredTopic _o
);
extern DDS_ReturnCode_t DDS_ContentFilteredTopic_set_expression_parameters(
	DDS_ContentFilteredTopic _o,
	DDS_StringSeq * expression_parameters
);
extern DDS_string DDS_ContentFilteredTopic_get_type_name(
	DDS_ContentFilteredTopic _o
);
extern DDS_string DDS_ContentFilteredTopic_get_name(
	DDS_ContentFilteredTopic _o
);
#endif
/*
 * end of interface DDS_ContentFilteredTopic
 */
/*
 * begin of interface DDS_MultiTopic
 */
#ifndef _DDS_MultiTopic_defined
#define _DDS_MultiTopic_defined

//typedef DDS_Object DDS_MultiTopic;

#endif

#ifndef _proto_DDS_MultiTopic_defined
#define _proto_DDS_MultiTopic_defined

extern DDS_string DDS_MultiTopic_get_subscription_expression(
	DDS_MultiTopic _o
);
extern DDS_DomainParticipant DDS_MultiTopic_get_participant(
	DDS_MultiTopic _o
);
extern DDS_ReturnCode_t DDS_MultiTopic_get_expression_parameters(
	DDS_MultiTopic _o,
	DDS_StringSeq * expression_parameters
);
extern DDS_ReturnCode_t DDS_MultiTopic_set_expression_parameters(
	DDS_MultiTopic _o,
	DDS_StringSeq * expression_parameters
);
extern DDS_string DDS_MultiTopic_get_type_name(
	DDS_MultiTopic _o
);
extern DDS_string DDS_MultiTopic_get_name(
	DDS_MultiTopic _o
);
#endif
/*
 * end of interface DDS_MultiTopic
 */
/*
 * begin of interface DDS_Publisher
 */
#ifndef _DDS_Publisher_defined
#define _DDS_Publisher_defined

//typedef DDS_Object DDS_Publisher;

#endif

#ifndef _proto_DDS_Publisher_defined
#define _proto_DDS_Publisher_defined

extern DDS_ReturnCode_t DDS_Publisher_begin_coherent_changes(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_get_qos(
	DDS_Publisher _o,
	DDS_PublisherQos * qos
);
extern DDS_ReturnCode_t DDS_Publisher_get_default_datawriter_qos(
	DDS_Publisher _o,
	DDS_DataWriterQos * qos
);
extern DDS_DomainParticipant DDS_Publisher_get_participant(
	DDS_Publisher _o
);
extern DDS_StatusMask DDS_Publisher_get_status_changes(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_enable(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_delete_contained_entities(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos(
	DDS_Publisher _o,
	DDS_DataWriterQos * qos
);
extern DDS_ReturnCode_t DDS_Publisher_set_qos(
	DDS_Publisher _o,
	DDS_PublisherQos * qos
);
extern DDS_ReturnCode_t DDS_Publisher_end_coherent_changes(
	DDS_Publisher _o
);
extern DDS_InstanceHandle_t DDS_Publisher_get_instance_handle(
	DDS_Publisher _o
);
extern DDS_PublisherListener DDS_Publisher_get_listener(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_resume_publications(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_copy_from_topic_qos(
	DDS_Publisher _o,
	DDS_DataWriterQos * a_datawriter_qos, /* inout (variable length) */
	DDS_TopicQos * a_topic_qos
);
extern DDS_ReturnCode_t DDS_Publisher_wait_for_acknowledgments(
	DDS_Publisher _o,
	DDS_Duration_t * max_wait
);
extern DDS_DataWriter DDS_Publisher_lookup_datawriter(
	DDS_Publisher _o,
	DDS_string topic_name
);
extern DDS_StatusCondition DDS_Publisher_get_statuscondition(
	DDS_Publisher _o
);
extern DDS_ReturnCode_t DDS_Publisher_suspend_publications(
	DDS_Publisher _o
);
extern DDS_DataWriter DDS_Publisher_create_datawriter(
	DDS_Publisher _o,
	DDS_Topic a_topic, /* in (variable length) */
	DDS_DataWriterQos * qos, /* in (variable length) */
	DDS_DataWriterListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_ReturnCode_t DDS_Publisher_delete_datawriter(
	DDS_Publisher _o,
	DDS_DataWriter a_datawriter
);
extern DDS_ReturnCode_t DDS_Publisher_set_listener(
	DDS_Publisher _o,
	DDS_PublisherListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
#endif
/*
 * end of interface DDS_Publisher
 */
/*
 * begin of interface DDS_DataWriter
 */
#ifndef _DDS_DataWriter_defined
#define _DDS_DataWriter_defined

//typedef DDS_Object DDS_DataWriter;

#endif

#ifndef _proto_DDS_DataWriter_defined
#define _proto_DDS_DataWriter_defined

extern DDS_ReturnCode_t DDS_DataWriter_get_matched_subscriptions(
	DDS_DataWriter _o,
	DDS_InstanceHandleSeq * subscription_handles
);
extern DDS_ReturnCode_t DDS_DataWriter_get_offered_incompatible_qos_status(
	DDS_DataWriter _o,
	DDS_OfferedIncompatibleQosStatus * status
);
extern DDS_Topic DDS_DataWriter_get_topic(
	DDS_DataWriter _o
);
extern DDS_Publisher DDS_DataWriter_get_publisher(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_get_matched_subscription_data(
	DDS_DataWriter _o,
	DDS_SubscriptionBuiltinTopicData * subscription_data, /* inout (variable length) */
	DDS_InstanceHandle_t subscription_handle
);
extern DDS_ReturnCode_t DDS_DataWriter_get_qos(
	DDS_DataWriter _o,
	DDS_DataWriterQos * qos
);
extern DDS_StatusMask DDS_DataWriter_get_status_changes(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_enable(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_get_publication_matched_status(
	DDS_DataWriter _o,
	DDS_PublicationMatchedStatus * status
);
extern DDS_ReturnCode_t DDS_DataWriter_set_qos(
	DDS_DataWriter _o,
	DDS_DataWriterQos * qos
);
extern DDS_InstanceHandle_t DDS_DataWriter_get_instance_handle(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_get_liveliness_lost_status(
	DDS_DataWriter _o,
	DDS_LivelinessLostStatus * status
);
extern DDS_DataWriterListener DDS_DataWriter_get_listener(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_assert_liveliness(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_get_offered_deadline_missed_status(
	DDS_DataWriter _o,
	DDS_OfferedDeadlineMissedStatus * status
);
extern DDS_StatusCondition DDS_DataWriter_get_statuscondition(
	DDS_DataWriter _o
);
extern DDS_ReturnCode_t DDS_DataWriter_wait_for_acknowledgments(
	DDS_DataWriter _o,
	DDS_Duration_t * max_wait
);
extern DDS_ReturnCode_t DDS_DataWriter_set_listener(
	DDS_DataWriter _o,
	DDS_DataWriterListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
#endif
/*
 * end of interface DDS_DataWriter
 */
/*
 * begin of interface DDS_Subscriber
 */
#ifndef _DDS_Subscriber_defined
#define _DDS_Subscriber_defined

//typedef DDS_Object DDS_Subscriber;

#endif

#ifndef _proto_DDS_Subscriber_defined
#define _proto_DDS_Subscriber_defined

extern DDS_DataReader DDS_Subscriber_create_datareader(
	DDS_Subscriber _o,
	DDS_TopicDescription a_topic, /* in (variable length) */
	DDS_DataReaderQos * qos, /* in (variable length) */
	DDS_DataReaderListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_DataReader DDS_Subscriber_lookup_datareader(
	DDS_Subscriber _o,
	DDS_string topic_name
);
extern DDS_ReturnCode_t DDS_Subscriber_end_access(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_get_default_datareader_qos(
	DDS_Subscriber _o,
	DDS_DataReaderQos * qos
);
extern DDS_ReturnCode_t DDS_Subscriber_delete_datareader(
	DDS_Subscriber _o,
	DDS_DataReader a_datareader
);
extern DDS_ReturnCode_t DDS_Subscriber_get_datareaders(
	DDS_Subscriber _o,
	DDS_DataReaderSeq * readers, /* inout (variable length) */
	DDS_SampleStateMask sample_states, /* in (fixed length) */
	DDS_ViewStateMask view_states, /* in (fixed length) */
	DDS_InstanceStateMask instance_states
);
extern DDS_ReturnCode_t DDS_Subscriber_get_qos(
	DDS_Subscriber _o,
	DDS_SubscriberQos * qos
);
extern DDS_StatusMask DDS_Subscriber_get_status_changes(
	DDS_Subscriber _o
);
extern DDS_DomainParticipant DDS_Subscriber_get_participant(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_enable(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_delete_contained_entities(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_set_qos(
	DDS_Subscriber _o,
	DDS_SubscriberQos * qos
);
extern DDS_InstanceHandle_t DDS_Subscriber_get_instance_handle(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_notify_datareaders(
	DDS_Subscriber _o
);
extern DDS_SubscriberListener DDS_Subscriber_get_listener(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_copy_from_topic_qos(
	DDS_Subscriber _o,
	DDS_DataReaderQos * a_datareader_qos, /* inout (variable length) */
	DDS_TopicQos * a_topic_qos
);
extern DDS_ReturnCode_t DDS_Subscriber_begin_access(
	DDS_Subscriber _o
);
extern DDS_StatusCondition DDS_Subscriber_get_statuscondition(
	DDS_Subscriber _o
);
extern DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos(
	DDS_Subscriber _o,
	DDS_DataReaderQos * qos
);
extern DDS_ReturnCode_t DDS_Subscriber_set_listener(
	DDS_Subscriber _o,
	DDS_SubscriberListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
#endif
/*
 * end of interface DDS_Subscriber
 */
/*
 * begin of interface DDS_DataReader
 */
#ifndef _DDS_DataReader_defined
#define _DDS_DataReader_defined

//typedef DDS_Object DDS_DataReader;

#endif

#ifndef _proto_DDS_DataReader_defined
#define _proto_DDS_DataReader_defined

extern DDS_ReturnCode_t DDS_DataReader_get_matched_publication_data(
	DDS_DataReader _o,
	DDS_PublicationBuiltinTopicData * publication_data, /* inout (variable length) */
	DDS_InstanceHandle_t publication_handle
);
extern DDS_Subscriber DDS_DataReader_get_subscriber(
	DDS_DataReader _o
);
extern DDS_ReturnCode_t DDS_DataReader_get_liveliness_changed_status(
	DDS_DataReader _o,
	DDS_LivelinessChangedStatus * status
);
extern DDS_QueryCondition DDS_DataReader_create_querycondition(
	DDS_DataReader _o,
	DDS_SampleStateMask sample_states, /* in (fixed length) */
	DDS_ViewStateMask view_states, /* in (fixed length) */
	DDS_InstanceStateMask instance_states, /* in (fixed length) */
	DDS_string query_expression, /* in (variable length) */
	DDS_StringSeq * query_parameters
);
extern DDS_ReadCondition DDS_DataReader_create_readcondition(
	DDS_DataReader _o,
	DDS_SampleStateMask sample_states, /* in (fixed length) */
	DDS_ViewStateMask view_states, /* in (fixed length) */
	DDS_InstanceStateMask instance_states
);
extern DDS_ReturnCode_t DDS_DataReader_get_requested_incompatible_qos_status(
	DDS_DataReader _o,
	DDS_RequestedIncompatibleQosStatus * status
);
extern DDS_ReturnCode_t DDS_DataReader_get_qos(
	DDS_DataReader _o,
	DDS_DataReaderQos * qos
);
extern DDS_ReturnCode_t DDS_DataReader_wait_for_historical_data(
	DDS_DataReader _o,
	DDS_Duration_t * max_wait
);
extern DDS_ReturnCode_t DDS_DataReader_get_subscription_matched_status(
	DDS_DataReader _o,
	DDS_SubscriptionMatchedStatus * status
);
extern DDS_StatusMask DDS_DataReader_get_status_changes(
	DDS_DataReader _o
);
extern DDS_ReturnCode_t DDS_DataReader_enable(
	DDS_DataReader _o
);
extern DDS_ReturnCode_t DDS_DataReader_delete_contained_entities(
	DDS_DataReader _o
);
extern DDS_ReturnCode_t DDS_DataReader_get_requested_deadline_missed_status(
	DDS_DataReader _o,
	DDS_RequestedDeadlineMissedStatus * status
);
extern DDS_ReturnCode_t DDS_DataReader_set_qos(
	DDS_DataReader _o,
	DDS_DataReaderQos * qos
);
extern DDS_InstanceHandle_t DDS_DataReader_get_instance_handle(
	DDS_DataReader _o
);
extern DDS_ReturnCode_t DDS_DataReader_get_sample_rejected_status(
	DDS_DataReader _o,
	DDS_SampleRejectedStatus * status
);
extern DDS_ReturnCode_t DDS_DataReader_delete_readcondition(
	DDS_DataReader _o,
	DDS_ReadCondition a_condition
);
extern DDS_DataReaderListener DDS_DataReader_get_listener(
	DDS_DataReader _o
);
extern DDS_StatusCondition DDS_DataReader_get_statuscondition(
	DDS_DataReader _o
);
extern DDS_TopicDescription DDS_DataReader_get_topicdescription(
	DDS_DataReader _o
);
extern DDS_ReturnCode_t DDS_DataReader_get_matched_publications(
	DDS_DataReader _o,
	DDS_InstanceHandleSeq * publication_handles
);
extern DDS_ReturnCode_t DDS_DataReader_set_listener(
	DDS_DataReader _o,
	DDS_DataReaderListener a_listener, /* in (variable length) */
	DDS_StatusMask mask
);
extern DDS_ReturnCode_t DDS_DataReader_get_sample_lost_status(
	DDS_DataReader _o,
	DDS_SampleLostStatus * status
);
#endif
/*
 * end of interface DDS_DataReader
 */
typedef struct {
	DDS_SampleStateKind sample_state;
	DDS_ViewStateKind view_state;
	DDS_InstanceStateKind instance_state;
	DDS_Time_t source_timestamp;
	DDS_InstanceHandle_t instance_handle;
	DDS_InstanceHandle_t publication_handle;
	int32_t disposed_generation_count;
	int32_t no_writers_generation_count;
	int32_t sample_rank;
	int32_t generation_rank;
	int32_t absolute_generation_rank;
	bool valid_data;
} DDS_SampleInfo;
#ifndef _DDS_sequence_DDS_SampleInfo_defined
#define _DDS_sequence_DDS_SampleInfo_defined
typedef struct {
	uint32_t _maximum;
	uint32_t _length;
	DDS_SampleInfo * _buffer;
} DDS_sequence_DDS_SampleInfo;
extern DDS_sequence_DDS_SampleInfo * DDS_sequence_DDS_SampleInfo__alloc(uint32_t nb);
extern DDS_SampleInfo * DDS_sequence_DDS_SampleInfo__allocbuf(uint32_t len);
#endif
typedef DDS_sequence_DDS_SampleInfo DDS_SampleInfoSeq;

/* === HAND CRAFTED EXTRAS ================================================== */

#define DDS_PARTICIPANT_QOS_DEFAULT NULL
#define DDS_PUBLISHER_QOS_DEFAULT   NULL
#define DDS_SUBSCRIBER_QOS_DEFAULT  NULL

DDS_DomainParticipant DDS_DomainParticipantFactory_get_instance(void);

#endif /* !__dds_h_ */

