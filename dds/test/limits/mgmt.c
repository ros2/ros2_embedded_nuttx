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
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dds/dds_dcps.h>
#include <dds/dds_debug.h>
#include <dds/dds_aux.h>

#include "userTypeSupport.h"

#ifndef TEST_TIME
#define	TEST_TIME	80
//#define TEST_TIME	3600
#endif
#define	MINIMAL_POOLS
//#define TRACE_DATA

#ifdef TRACE_DATA
#include "rtps.h"
#endif

static const int nofConfigTopics = /*8*/500 + /*1*/48 * 4;
static const int nofStateTopics = /*2*/100 + /*1*/48 * 1;
static const int nofStatisticTopics = /*2*/100 + /*1*/48 * 1;

static const int nofConfigInstances = 1;
static const int nofStateInstances = 1;
static const int nofStatisticInstances = 1;

int mstime()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

#ifndef PTHREADS_USED

unsigned int sleep(unsigned int seconds)
{
	if (seconds == 1)	/* Too long ... make it 1 ms :-) */
		DDS_wait (1);
#ifdef DDS_DUMP
	else if (seconds > 30) {
		DDS_wait (seconds * 700);
		printf ("<mgmt - memory/discovery status>\r\n");
		dbg_pool_dump ();
		dbg_disc_dump ();
		DDS_wait (seconds * 300);
	}
#endif
	else
		DDS_wait (seconds * 1000);
	return 0;
}
#endif

#ifdef MINIMAL_POOLS
void configure_minimal_pool_constraints (DDS_PoolConstraints *rp)
{
	rp->max_domains = 1;
	rp->min_subscribers = rp->min_publishers = 1;
	rp->min_local_readers = rp->max_local_readers = 1000;
	rp->min_local_writers = rp->max_local_writers = 700;
	rp->min_topics = 1;
	rp->min_topic_types = 1;
	rp->min_reader_proxies = rp->min_writer_proxies = 1;
	rp->min_remote_participants = 1;
	rp->min_remote_readers = rp->min_remote_writers = 1;
	rp->min_pool_data = 16000;
	rp->min_changes = 1;
	rp->min_instances = 1;
	rp->min_application_samples = 1;
	rp->min_strings = 1;
	rp->min_string_data = 256;
	rp->min_locators = 1;
	rp->min_qos = 1;
	rp->min_list_nodes = 1;
	/* rp->min_timers = 1; */
}
#else
void configure_optimal_pool_constraints (DDS_PoolConstraints *rp)
{
	rp->min_local_readers = rp->max_local_readers = 1000;
	rp->min_local_writers = rp->max_local_writers = 700;
	rp->min_topics = 1000;
	rp->min_changes = 1700;
	rp->min_instances = 1700;
	rp->min_list_nodes *= 26;
	rp->min_pool_data = 64000;
	rp->min_strings = 1000;
	rp->min_string_data = 25000;
	rp->min_locators = 10;
	rp->min_qos = 4;
}
#endif

static int nofConfigSamples = 0;

void dr_on_config_data_available(DDS_DataReaderListener *self, DDS_DataReader dataReader)
{
	acceptance_high_end_ConfigPtrSeq samples;
	DDS_SampleInfoSeq sampleInfos;
	DDS_ReturnCode_t retCode;
	int i;

	DDS_SEQ_INIT(samples);
	DDS_SEQ_INIT(sampleInfos);

	retCode = DDS_DataReader_take(dataReader, (DDS_DataSeq *)&samples, &sampleInfos, -1, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
	if (retCode == DDS_RETCODE_NO_DATA) {
		return;
	}
	else if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error taking sample (%s).\r\n", DDS_error(retCode));
		return;
	}

	for (i = 0; i < DDS_SEQ_LENGTH (samples); i++)
		if (DDS_SEQ_ITEM (sampleInfos, i)->valid_data)
			nofConfigSamples++;

	retCode = DDS_DataReader_return_loan(dataReader, (DDS_DataSeq *)&samples, &sampleInfos);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error returning loan (%s).\r\n", DDS_error(retCode));
		return;
	}
}

DDS_DataReaderListener configDataReaderListener =
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_config_data_available,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL
};

static int nofStateSamples = 0;

void dr_on_state_data_available(DDS_DataReaderListener *self, DDS_DataReader dataReader)
{
	acceptance_high_end_StatePtrSeq samples;
	DDS_SampleInfoSeq sampleInfos;
	DDS_ReturnCode_t retCode;
	int i;

	DDS_SEQ_INIT(samples);
	DDS_SEQ_INIT(sampleInfos);

	retCode = DDS_DataReader_take(dataReader, (DDS_DataSeq *)&samples, &sampleInfos, -1, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
	if (retCode == DDS_RETCODE_NO_DATA) {
		return;
	}
	else if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error taking sample (%s).\r\n", DDS_error(retCode));
		return;
	}

	for (i = 0; i < DDS_SEQ_LENGTH (samples); i++)
		if (DDS_SEQ_ITEM (sampleInfos, i)->valid_data)
			nofStateSamples++;

	retCode = DDS_DataReader_return_loan(dataReader, (DDS_DataSeq *)&samples, &sampleInfos);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error returning loan (%s).\r\n", DDS_error(retCode));
		return;
	}
}

DDS_DataReaderListener stateDataReaderListener =
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_state_data_available,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL
};

static int nofStatisticSamples = 0;

void dr_on_statistic_data_available(DDS_DataReaderListener *self, DDS_DataReader dataReader)
{
	acceptance_high_end_StatisticPtrSeq samples;
	DDS_SampleInfoSeq sampleInfos;
	DDS_ReturnCode_t retCode;
	int i;

	DDS_SEQ_INIT(samples);
	DDS_SEQ_INIT(sampleInfos);

	retCode = DDS_DataReader_take(dataReader, (DDS_DataSeq *)&samples, &sampleInfos, -1, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
	if (retCode == DDS_RETCODE_NO_DATA) {
		return;
	}
	else if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error taking sample (%s).\r\n", DDS_error(retCode));
		return;
	}

	for (i = 0; i < DDS_SEQ_LENGTH (samples); i++)
		if (DDS_SEQ_ITEM (sampleInfos, i)->valid_data)
			nofConfigSamples++;

	retCode = DDS_DataReader_return_loan(dataReader, (DDS_DataSeq *)&samples, &sampleInfos);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error returning loan (%s).\r\n", DDS_error(retCode));
		return;
	}
}

DDS_DataReaderListener statisticDataReaderListener =
{
	/* .on_requested_deadline_missed  */ NULL,
	/* .on_requested_incompatible_qos */ NULL,
	/* .on_sample_rejected            */ NULL,
	/* .on_liveliness_changed         */ NULL,
	/* .on_data_available             */ dr_on_statistic_data_available,
	/* .on_subscription_matched       */ NULL,
	/* .on_sample_lost                */ NULL
};

int main(int argc, char* argv[])
{
	int startTime = mstime();
	DDS_PoolConstraints reqs;
	DDS_DomainParticipant    domainParticipant;
	DDS_Publisher    publisher;
	DDS_Subscriber    subscriber;
	DDS_ReturnCode_t retCode;
	char *configTopicName[nofConfigTopics];
	char *stateTopicName[nofStateTopics];
	char *statisticTopicName[nofStatisticTopics];
	DDS_Topic configTopic[nofConfigTopics];
	DDS_Topic stateTopic[nofStateTopics];
	DDS_Topic statisticTopic[nofStatisticTopics];
	DDS_DataWriterQos dataWriterQos;
	DDS_DataWriter    configDataWriter[nofConfigTopics];
	DDS_DataReaderQos dataReaderQos;
	DDS_DataReader    configDataReader[nofConfigTopics];
	DDS_DataReader    stateDataReader[nofStateTopics];
	DDS_DataReader    statisticDataReader[nofStatisticTopics];
#if 0
	acceptance_high_end_Config config;
	acceptance_high_end_State state;
	acceptance_high_end_Statistic statistic;
	DDS_InstanceHandle_t configInstance[nofConfigTopics][nofConfigInstances];
	DDS_InstanceHandle_t stateInstance[nofStateTopics][nofStateInstances];
	DDS_InstanceHandle_t statisticInstance[nofStatisticTopics][nofStatisticInstances];
#endif
	int i = 0;
	/*int j = 0;*/

	int firstConfigTopic = (argc > 1) ? atoi(argv[1]) : 0;
	int firstStateTopic = (argc > 2) ? atoi(argv[2]) : 0;
	int firstStatisticTopic = (argc > 3) ? atoi(argv[3]) : 0;
	int stagedLoading = (argc > 4) ? atoi(argv[4]) : 0;
	printf("[0] Config topics start at %d...\r\n", firstConfigTopic);
	printf("[0] State topics start at %d...\r\n", firstStateTopic);
	printf("[0] Statistic topics start at %d...\r\n", firstStatisticTopic);
	printf("[0] Staged loading %s.\r\n", stagedLoading ? "on" : "off");

        DDS_program_name (&argc, argv);
	DDS_entity_name ("Technicolor Limits Manager");
	DDS_get_default_pool_constraints(&reqs, ~0, 100);
#ifdef MINIMAL_POOLS
	configure_minimal_pool_constraints (&reqs);
#else
	configure_optimal_pool_constraints (&reqs);
#endif
	DDS_set_pool_constraints (&reqs);

#ifdef DDS_DEBUG
	DDS_Debug_start ();
#endif
#ifdef TRACE_DATA
	rtps_dtrace_set (DRTRC_TRACE_ALL);
#endif

	/* Create domain participant... */
	domainParticipant = DDS_DomainParticipantFactory_create_participant(0, NULL, NULL, 0);
	if (!domainParticipant) {
		fprintf(stderr, "Error creating domain participant.\r\n");
		return -1;
	}
	printf("[%d] Created domain participant.\r\n", mstime() - startTime);
	// sleep(1);

	/* Create publisher... */
	publisher = DDS_DomainParticipant_create_publisher(domainParticipant, NULL, NULL, 0);
	if (!publisher) {
		fprintf(stderr, "Error creating publisher.\r\n");
		return -1;
	}
	printf("[%d] Created publisher.\r\n", mstime() - startTime);
	// sleep(1);

	/* Create subscriber... */
	subscriber = DDS_DomainParticipant_create_subscriber(domainParticipant, NULL, NULL, 0);
	if (!subscriber) {
		fprintf(stderr, "Error creating subscriber.\r\n");
		return -1;
	}
	printf("[%d] Created subscriber.\r\n", mstime() - startTime);
	// sleep(1);

	/* Register types... */
	retCode = acceptance_high_end_ConfigTypeSupport_register_type(domainParticipant, NULL);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error registering type (%s).\r\n", DDS_error(retCode));
		return -1;
	}
	printf("[%d] Registered config type.\r\n", mstime() - startTime);
	retCode = acceptance_high_end_StateTypeSupport_register_type(domainParticipant, NULL);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error registering type (%s).\r\n", DDS_error(retCode));
		return -1;
	}
	printf("[%d] Registered state type.\r\n", mstime() - startTime);
	retCode = acceptance_high_end_StatisticTypeSupport_register_type(domainParticipant, NULL);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error registering type (%s).\r\n", DDS_error(retCode));
		return -1;
	}
	printf("[%d] Registered statistic type.\r\n", mstime() - startTime);
	// sleep(1);

	/* Create topics... */
	for (i = 0; i < nofConfigTopics; i++) {
		char topicName[32];
		sprintf(topicName, "ConfigTopic%d", firstConfigTopic + i);
		configTopicName[i] = topicName;
		configTopic[i] = DDS_DomainParticipant_create_topic(domainParticipant, topicName, acceptance_high_end_ConfigTypeSupport_get_type_name(), NULL, NULL, 0);
		if (!configTopic[i]) {
			fprintf(stderr, "Error creating topic.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d config topics.\r\n", mstime() - startTime, nofConfigTopics);
	for (i = 0; i < nofStateTopics; i++) {
		char topicName[32];
		sprintf(topicName, "StateTopic%d", firstStateTopic + i);
		stateTopicName[i] = topicName;
		stateTopic[i] = DDS_DomainParticipant_create_topic(domainParticipant, topicName, acceptance_high_end_StateTypeSupport_get_type_name(), NULL, NULL, 0);
		if (!stateTopic[i]) {
			fprintf(stderr, "Error creating topic.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d state topics.\r\n", mstime() - startTime, nofStateTopics);
	for (i = 0; i < nofStatisticTopics; i++) {
		char topicName[32];
		sprintf(topicName, "StatisticTopic%d", firstStatisticTopic + i);
		statisticTopicName[i] = topicName;
		statisticTopic[i] = DDS_DomainParticipant_create_topic(domainParticipant, topicName, acceptance_high_end_StatisticTypeSupport_get_type_name(), NULL, NULL, 0);
		if (!statisticTopic[i]) {
			fprintf(stderr, "Error creating topic.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d statistic topics.\r\n", mstime() - startTime, nofStatisticTopics);
	// sleep(1);

	/* Create data writers... */
	DDS_Publisher_get_default_datawriter_qos(publisher, &dataWriterQos);
	dataWriterQos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dataWriterQos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	for (i = 0; i < nofConfigTopics; i++) {
		configDataWriter[i] = DDS_Publisher_create_datawriter(publisher, configTopic[i], &dataWriterQos, NULL, 0);
		if (!configDataWriter[i]) {
			fprintf(stderr, "Error creating data writer.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d config data writers.\r\n", mstime() - startTime, nofConfigTopics);
	// sleep(1);

	/* Create data readers... */
	DDS_Subscriber_get_default_datareader_qos(subscriber, &dataReaderQos);
	dataReaderQos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dataReaderQos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	for (i = 0; i < nofConfigTopics; i++) {
		configDataReader[i] = DDS_Subscriber_create_datareader(subscriber, DDS_DomainParticipant_lookup_topicdescription(domainParticipant, configTopicName[i]), &dataReaderQos, &configDataReaderListener, DDS_DATA_AVAILABLE_STATUS);
		if (!configDataReader[i]) {
			fprintf(stderr, "Error creating data reader.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d config data readers.\r\n", mstime() - startTime, nofConfigTopics);
	DDS_Subscriber_get_default_datareader_qos(subscriber, &dataReaderQos);
	dataReaderQos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dataReaderQos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	for (i = 0; i < nofStateTopics; i++) {
		stateDataReader[i] = DDS_Subscriber_create_datareader(subscriber, DDS_DomainParticipant_lookup_topicdescription(domainParticipant, stateTopicName[i]), &dataReaderQos, &stateDataReaderListener, DDS_DATA_AVAILABLE_STATUS);
		if (!stateDataReader[i]) {
			fprintf(stderr, "Error creating data reader.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d state data readers.\r\n", mstime() - startTime, nofStateTopics);
	DDS_Subscriber_get_default_datareader_qos(subscriber, &dataReaderQos);
	for (i = 0; i < nofStatisticTopics; i++) {
		statisticDataReader[i] = DDS_Subscriber_create_datareader(subscriber, DDS_DomainParticipant_lookup_topicdescription(domainParticipant, statisticTopicName[i]), &dataReaderQos, &statisticDataReaderListener, DDS_DATA_AVAILABLE_STATUS);
		if (!statisticDataReader[i]) {
			fprintf(stderr, "Error creating data reader.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d statistic data readers.\r\n", mstime() - startTime, nofStatisticTopics);
	// sleep(1);

	/* Lookup instances... */
	/*for (i = 0; i < nofConfigTopics; i++) {
		for (j = 0; j < nofConfigInstances; j++) {
			config.key = j;
			configInstance[i][j] = acceptance_high_end_ConfigDataReader_lookup_instance(configDataReader[i], &config);
			if (!configInstance[i][j]) {
				fprintf(stderr, "Error looking up instance.\r\n");
				break;
			}
		}
	}
	printf("[%d] Looked up %d config instances.\r\n", mstime() - startTime, nofConfigTopics * nofConfigInstances);
	for (i = 0; i < nofStateTopics; i++) {
		for (j = 0; j < nofStateInstances; j++) {
			state.key = j;
			stateInstance[i][j] = acceptance_high_end_StateDataReader_lookup_instance(stateDataReader[i], &state);
			if (!stateInstance[i][j]) {
				fprintf(stderr, "Error looking up instance.\r\n");
				break;
			}
		}
	}
	printf("[%d] Looked up %d state instances.\r\n", mstime() - startTime, nofStatisticTopics * nofStatisticInstances);
	for (i = 0; i < nofStatisticTopics; i++) {
		for (j = 0; j < nofStatisticInstances; j++) {
			statistic.key = j;
			statisticInstance[i][j] = acceptance_high_end_StatisticDataReader_lookup_instance(statisticDataReader[i], &statistic);
			if (!statisticInstance[i][j]) {
				fprintf(stderr, "Error looking up instance.\r\n");
				break;
			}
		}
	}
	printf("[%d] Looked up %d statistic instances.\r\n", mstime() - startTime, nofStatisticTopics * nofStatisticInstances);*/
	// sleep(1);

	if (stagedLoading) {
		char file_name[24];
		sprintf(file_name, "/tmp/acceptStageLoad0");
		FILE *f = fopen(file_name, "w");
		fclose(f);
	}

	int waitTime = mstime();
	int _nofConfigSamples = 0;
	int _nofStateSamples = 0;
	int _nofStatisticSamples = 0;
	while ((mstime() - waitTime) < (TEST_TIME * 1000)) {
		if ((nofConfigSamples > _nofConfigSamples) || (nofStateSamples > _nofStateSamples) || (nofStatisticSamples > _nofStatisticSamples)) {
			printf("[%d] Received %d config samples.\r\n", mstime() - startTime, nofConfigSamples);
			printf("[%d] Received %d state samples.\r\n", mstime() - startTime, nofStateSamples);
			printf("[%d] Received %d statistic samples.\r\n", mstime() - startTime, nofStatisticSamples);
			_nofConfigSamples = nofConfigSamples;
			_nofStateSamples = nofStateSamples;
			_nofStatisticSamples = nofStatisticSamples;
		}
		sleep(1);
	}

	/* Delete contained entities... */
//	printf (" -- Deleting contained entities.\r\n");
	retCode = DDS_DomainParticipant_delete_contained_entities(domainParticipant);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error deleting contained entities.\r\n");
		return -1;
	}

//	printf ("contained entities deleted!\r\n");

	/* Delete domain participants... */
	retCode = DDS_DomainParticipantFactory_delete_participant(domainParticipant);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error deleting domain participant.\r\n");
		return -1;
	}

	printf ("[%d] mgmt completed successfully.\r\n", mstime() - startTime);
	return 0;
}

