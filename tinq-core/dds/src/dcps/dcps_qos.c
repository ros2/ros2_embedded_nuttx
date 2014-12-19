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

/* dcps_qos.c -- Implements the DCPS Qos methods. */

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
#include "pid.h"
#include "dcps_builtin.h"

void DDS_UserDataQosPolicy__init (DDS_UserDataQosPolicy *data)
{
	DDS_SEQ_INIT (data->value);
}

void DDS_UserDataQosPolicy__clear (DDS_UserDataQosPolicy *data)
{
	dds_seq_cleanup (&data->value);
}

DDS_UserDataQosPolicy *DDS_UserDataQosPolicy__alloc (void)
{
	DDS_UserDataQosPolicy	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_UserDataQosPolicy));
	if (!p)
		return (NULL);

	DDS_UserDataQosPolicy__init (p);
	return (p);
}

void DDS_UserDataQosPolicy__free (DDS_UserDataQosPolicy *data)
{
	if (!data)
		return;

	DDS_UserDataQosPolicy__clear (data);
	mm_fcts.free_ (data);
}


void DDS_GroupDataQosPolicy__init (DDS_GroupDataQosPolicy *data)
{
	DDS_SEQ_INIT (data->value);
}

void DDS_GroupDataQosPolicy__clear (DDS_GroupDataQosPolicy *data)
{
	dds_seq_cleanup (&data->value);
}

DDS_GroupDataQosPolicy *DDS_GroupDataQosPolicy__alloc (void)
{
	DDS_GroupDataQosPolicy	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_GroupDataQosPolicy));
	if (!p)
		return (NULL);

	DDS_GroupDataQosPolicy__init (p);
	return (p);
}

void DDS_GroupDataQosPolicy__free (DDS_GroupDataQosPolicy *data)
{
	if (!data)
		return;

	DDS_GroupDataQosPolicy__clear (data);
	mm_fcts.free_ (data);
}


void DDS_TopicDataQosPolicy__init (DDS_TopicDataQosPolicy *data)
{
	DDS_SEQ_INIT (data->value);
}

void DDS_TopicDataQosPolicy__clear (DDS_TopicDataQosPolicy *data)
{
	dds_seq_cleanup (&data->value);
}

DDS_TopicDataQosPolicy *DDS_TopicDataQosPolicy__alloc (void)
{
	DDS_TopicDataQosPolicy	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_TopicDataQosPolicy));
	if (!p)
		return (NULL);

	DDS_TopicDataQosPolicy__init (p);
	return (p);
}

void DDS_TopicDataQosPolicy__free (DDS_TopicDataQosPolicy *data)
{
	if (!data)
		return;

	DDS_TopicDataQosPolicy__clear (data);
	mm_fcts.free_ (data);
}


void DDS_PartitionQosPolicy__init (DDS_PartitionQosPolicy *part)
{
	DDS_SEQ_INIT (part->name);
}

void DDS_PartitionQosPolicy__clear (DDS_PartitionQosPolicy *part)
{
	dds_seq_cleanup (&part->name);
}

DDS_PartitionQosPolicy *DDS_PartitionQosPolicy__alloc (void)
{
	DDS_PartitionQosPolicy	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_PartitionQosPolicy));
	if (!p)
		return (NULL);

	DDS_PartitionQosPolicy__init (p);
	return (p);
}

void DDS_PartitionQosPolicy__free (DDS_PartitionQosPolicy *part)
{
	if (!part)
		return;

	DDS_PartitionQosPolicy__clear (part);
	mm_fcts.free_ (part);
}


void DDS_DomainParticipantQos__init (DDS_DomainParticipantQos *qos)
{
	memcpy (qos, &qos_def_participant_qos, sizeof (DDS_DomainParticipantQos));
}

void DDS_DomainParticipantQos__clear (DDS_DomainParticipantQos *qos)
{
	dds_seq_cleanup (&qos->user_data.value);
}

DDS_DomainParticipantQos *DDS_DomainParticipantQos__alloc (void)
{
	DDS_DomainParticipantQos	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_DomainParticipantQos));
	if (!p)
		return (NULL);

	DDS_DomainParticipantQos__init (p);
	return (p);
}

void DDS_DomainParticipantQos__free (DDS_DomainParticipantQos *qos)
{
	if (!qos)
		return;

	DDS_DomainParticipantQos__clear (qos);
	mm_fcts.free_ (qos);
}


void DDS_TopicQos__init (DDS_TopicQos *qos)
{
	memcpy (qos, &qos_def_topic_qos, sizeof (DDS_TopicQos));
}

void DDS_TopicQos__clear (DDS_TopicQos *qos)
{
	dds_seq_cleanup (&qos->topic_data.value);
}

DDS_TopicQos *DDS_TopicQos__alloc (void)
{
	DDS_TopicQos	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_TopicQos));
	if (!p)
		return (NULL);

	DDS_TopicQos__init (p);
	return (p);
}

void DDS_TopicQos__free (DDS_TopicQos *qos)
{
	if (!qos)
		return;

	DDS_TopicQos__clear (qos);
	mm_fcts.free_ (qos);
}


void DDS_SubscriberQos__init (DDS_SubscriberQos *qos)
{
	memcpy (qos, &qos_def_subscriber_qos, sizeof (DDS_SubscriberQos));
}

void DDS_SubscriberQos__clear (DDS_SubscriberQos *qos)
{
	DDS_PartitionQosPolicy__clear (&qos->partition);
	dds_seq_cleanup (&qos->group_data.value);
}

DDS_SubscriberQos *DDS_SubscriberQos__alloc (void)
{
	DDS_SubscriberQos	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_SubscriberQos));
	if (!p)
		return (NULL);

	DDS_SubscriberQos__init (p);
	return (p);
}

void DDS_SubscriberQos__free (DDS_SubscriberQos *qos)
{
	if (!qos)
		return;

	DDS_SubscriberQos__clear (qos);
	mm_fcts.free_ (qos);
}


void DDS_PublisherQos__init (DDS_PublisherQos *qos)
{
	memcpy (qos, &qos_def_publisher_qos, sizeof (DDS_PublisherQos));
}

void DDS_PublisherQos__clear (DDS_PublisherQos *qos)
{
	DDS_PartitionQosPolicy__clear (&qos->partition);
	dds_seq_cleanup (&qos->group_data.value);
}

DDS_PublisherQos *DDS_PublisherQos__alloc (void)
{
	DDS_PublisherQos	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_PublisherQos));
	if (!p)
		return (NULL);

	DDS_PublisherQos__init (p);
	return (p);
}

void DDS_PublisherQos__free (DDS_PublisherQos *qos)
{
	if (!qos)
		return;

	DDS_PublisherQos__clear (qos);
	mm_fcts.free_ (qos);
}


void DDS_DataReaderQos__init (DDS_DataReaderQos *qos)
{
	memcpy (qos, &qos_def_reader_qos, sizeof (DDS_DataReaderQos));
}

void DDS_DataReaderQos__clear (DDS_DataReaderQos *qos)
{
	dds_seq_cleanup (&qos->user_data.value);
}

DDS_DataReaderQos *DDS_DataReaderQos__alloc (void)
{
	DDS_DataReaderQos	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_DataReaderQos));
	if (!p)
		return (NULL);

	DDS_DataReaderQos__init (p);
	return (p);
}

void DDS_DataReaderQos__free (DDS_DataReaderQos *qos)
{
	if (!qos)
		return;

	DDS_DataReaderQos__clear (qos);
	mm_fcts.free_ (qos);
}


void DDS_DataWriterQos__init (DDS_DataWriterQos *qos)
{
	memcpy (qos, &qos_def_writer_qos, sizeof (DDS_DataWriterQos));
}

void DDS_DataWriterQos__clear (DDS_DataWriterQos *qos)
{
	dds_seq_cleanup (&qos->user_data.value);
}

DDS_DataWriterQos *DDS_DataWriterQos__alloc (void)
{
	DDS_DataWriterQos	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_DataWriterQos));
	if (!p)
		return (NULL);

	DDS_DataWriterQos__init (p);
	return (p);
}

void DDS_DataWriterQos__free (DDS_DataWriterQos *qos)
{
	if (!qos)
		return;

	DDS_DataWriterQos__clear (qos);
	mm_fcts.free_ (qos);
}


