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
#include <string.h>
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
#define	TEST_TIME	40
//#define TEST_TIME	3600
#endif
//#define	MINIMAL_POOLS

static const int nofConfigTopics = 4;
static const int nofStateTopics = 1;
static const int nofStatisticTopics = 1;

static const int nofConfigInstances = 1;
static const int nofStateInstances = 1;
static const int nofStatisticInstances = 1;

int mstime()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

unsigned int sleep(unsigned int seconds)
{
	if (seconds == 1)	/* Too long ... make it 1 ms :-) */
		DDS_wait (1);
#ifdef DDS_DUMP
	else if (seconds > 30) {
		DDS_wait (seconds * 700);
		printf ("<comp - memory/discovery status>\r\n");
		dbg_pool_dump ();
		dbg_disc_dump ();
		DDS_wait (seconds * 300);
	}
#endif
	else
		DDS_wait (seconds * 1000);
	return 0;
}

#ifdef MINIMAL_POOLS
void configure_minimal_pool_constraints (DDS_PoolConstraints *rp)
{
	rp->max_domains = 1;
	rp->min_subscribers = rp->min_publishers = 1;
	rp->min_local_readers = rp->max_local_readers = 12;
	rp->min_local_writers = rp->max_local_writers = 10;
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
	rp->min_local_readers = rp->max_local_readers = 12;
	rp->min_local_writers = rp->max_local_writers = 10;
	rp->min_topics = 8;
	rp->min_changes = 32;
	rp->min_instances = 24;
	rp->min_list_nodes /= 4;
	rp->min_pool_data = 32000;
	rp->min_strings = 4;
	rp->min_string_data = 256;
	rp->min_qos = 4;
}
#endif

int main(int argc, char* argv[])
{
	int startTime = mstime();
	DDS_PoolConstraints reqs;
	DDS_DomainParticipant    domainParticipant;
	DDS_Publisher    publisher;
	DDS_Subscriber    subscriber;
	DDS_ReturnCode_t retCode;
	char *configTopicName[nofConfigTopics];
	DDS_Topic configTopic[nofConfigTopics];
	DDS_Topic stateTopic[nofStateTopics];
	DDS_Topic statisticTopic[nofStatisticTopics];
	DDS_DataWriterQos dataWriterQos;
	DDS_DataWriter    configDataWriter[nofConfigTopics];
	DDS_DataWriter    stateDataWriter[nofStateTopics];
	DDS_DataWriter    statisticDataWriter[nofStatisticTopics];
	DDS_DataReaderQos dataReaderQos;
	DDS_DataReader    configDataReader[nofConfigTopics];
	acceptance_high_end_Config config;
	acceptance_high_end_State state;
	acceptance_high_end_Statistic statistic;
	DDS_InstanceHandle_t configInstance[nofConfigTopics][nofConfigInstances];
	DDS_InstanceHandle_t stateInstance[nofStateTopics][nofStateInstances];
	DDS_InstanceHandle_t statisticInstance[nofStatisticTopics][nofStatisticInstances];
	int i = 0;
	int j = 0;

	int firstConfigTopic = (argc > 1) ? atoi(argv[1]) : 0;
	int firstStateTopic = (argc > 2) ? atoi(argv[2]) : 0;
	int firstStatisticTopic = (argc > 3) ? atoi(argv[3]) : 0;
	int stagedLoading = (argc > 4) ? atoi(argv[4]) : 0;
	printf("[0] Config topics start at %d...\r\n", firstConfigTopic);
	printf("[0] State topics start at %d...\r\n", firstStateTopic);
	printf("[0] Statistic topics start at %d...\r\n", firstStatisticTopic);
	printf("[0] Staged loading %s.\r\n", stagedLoading ? "on" : "off");

	if (stagedLoading) {
		char file_name[24];
		sprintf(file_name, "/tmp/acceptStageLoad%d", firstConfigTopic);
		struct stat buf;
		if (stat(file_name, &buf)) {
			printf ("[%d] Waiting for %s\r\n", mstime() - startTime, file_name);
			do {
				sleep(1);
			}
			while (stat(file_name, &buf));
			printf ("[%d] Got %s!\r\n", mstime() - startTime, file_name);
		}
	}

        DDS_program_name (&argc, argv);
	DDS_entity_name ("Technicolor Limits Component");
	DDS_get_default_pool_constraints(&reqs, ~0, 100);
#ifdef MINIMAL_POOLS
	configure_minimal_pool_constraints (&reqs);
#else
	configure_optimal_pool_constraints (&reqs);
#endif
	DDS_set_pool_constraints(&reqs);

#ifdef DDS_DEBUG
	DDS_Debug_start ();
#endif

	/* Create domain participant... */
	domainParticipant = DDS_DomainParticipantFactory_create_participant(0, NULL, NULL, 0);
	if (!domainParticipant) {
		fprintf(stderr, "Error creating domain participant.\r\n");
		return -1;
	}
	printf("[%d] Created domain participant.\r\n", mstime() - startTime);
	sleep(1);

	/* Create publisher... */
	publisher = DDS_DomainParticipant_create_publisher(domainParticipant, NULL, NULL, 0);
	if (!publisher) {
		fprintf(stderr, "Error creating publisher.\r\n");
		return -1;
	}
	printf("[%d] Created publisher.\r\n", mstime() - startTime);
	sleep(1);

	/* Create subscriber... */
	subscriber = DDS_DomainParticipant_create_subscriber(domainParticipant, NULL, NULL, 0);
	if (!subscriber) {
		fprintf(stderr, "Error creating subscriber.\r\n");
		return -1;
	}
	printf("[%d] Created subscriber.\r\n", mstime() - startTime);
	sleep(1);

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
	sleep(1);

	/* Create topics... */
	for (i = 0; i < nofConfigTopics; i++) {
		char topicName[32];
		sprintf(topicName, "ConfigTopic%d", firstConfigTopic + i);
		configTopicName[i] = strdup (topicName);
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
		statisticTopic[i] = DDS_DomainParticipant_create_topic(domainParticipant, topicName, acceptance_high_end_StatisticTypeSupport_get_type_name(), NULL, NULL, 0);
		if (!statisticTopic[i]) {
			fprintf(stderr, "Error creating topic.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d statistic topics.\r\n", mstime() - startTime, nofStatisticTopics);
	sleep(1);

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
	DDS_Publisher_get_default_datawriter_qos(publisher, &dataWriterQos);
	dataWriterQos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dataWriterQos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	for (i = 0; i < nofStateTopics; i++) {
		stateDataWriter[i] = DDS_Publisher_create_datawriter(publisher, stateTopic[i], &dataWriterQos, NULL, 0);
		if (!stateDataWriter[i]) {
			fprintf(stderr, "Error creating data writer.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d state data writers.\r\n", mstime() - startTime, nofStateTopics);
	DDS_Publisher_get_default_datawriter_qos(publisher, &dataWriterQos);
	for (i = 0; i < nofStatisticTopics; i++) {
		statisticDataWriter[i] = DDS_Publisher_create_datawriter(publisher, statisticTopic[i], &dataWriterQos, NULL, 0);
		if (!statisticDataWriter[i]) {
			fprintf(stderr, "Error creating data writer.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d statistic data writers.\r\n", mstime() - startTime, nofStatisticTopics);
	sleep(1);

	/* Create data readers... */
	DDS_Subscriber_get_default_datareader_qos(subscriber, &dataReaderQos);
	dataReaderQos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
	dataReaderQos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	for (i = 0; i < nofConfigTopics; i++) {
		configDataReader[i] = DDS_Subscriber_create_datareader(subscriber, DDS_DomainParticipant_lookup_topicdescription(domainParticipant, configTopicName[i]), &dataReaderQos, NULL, 0);
		if (!configDataReader[i]) {
			fprintf(stderr, "Error creating data reader.\r\n");
			return -1;
		}
	}
	printf("[%d] Created %d config data readers.\r\n", mstime() - startTime, nofConfigTopics);
	sleep(1);

	/* Register instances... */
	for (i = 0; i < nofConfigTopics; i++) {
		for (j = 0; j < nofConfigInstances; j++) {
			config.key = j;
			configInstance[i][j] = DDS_DataWriter_register_instance(configDataWriter[i], &config);
			if (!configInstance[i][j]) {
				fprintf(stderr, "Error registering instance.\r\n");
				break;
			}
		}
	}
	printf("[%d] Registered %d config instances.\r\n", mstime() - startTime, nofConfigTopics * nofConfigInstances);
	for (i = 0; i < nofStateTopics; i++) {
		for (j = 0; j < nofStateInstances; j++) {
			state.key = j;
			stateInstance[i][j] = DDS_DataWriter_register_instance(stateDataWriter[i], &state);
			if (!stateInstance[i][j]) {
				fprintf(stderr, "Error registering instance.\r\n");
				break;
			}
		}
	}
	printf("[%d] Registered %d state instances.\r\n", mstime() - startTime, nofStatisticTopics * nofStatisticInstances);
	for (i = 0; i < nofStatisticTopics; i++) {
		for (j = 0; j < nofStatisticInstances; j++) {
			statistic.key = j;
			statisticInstance[i][j] = DDS_DataWriter_register_instance(statisticDataWriter[i], &statistic);
			if (!statisticInstance[i][j]) {
				fprintf(stderr, "Error registering instance.\r\n");
				break;
			}
		}
	}
	printf("[%d] Registered %d statistic instances.\r\n", mstime() - startTime, nofStatisticTopics * nofStatisticInstances);
	sleep(1);

	/* Publish samples... */
	for (i = 0; i < nofConfigTopics; i++) {
		for (j = 0; j < nofConfigInstances; j++) {
			config.key = j;
			retCode = DDS_DataWriter_write(configDataWriter[i], &config, configInstance[i][j]);
			if (retCode != DDS_RETCODE_OK) {
				fprintf(stderr, "Error publishing sample (%s).\r\n", DDS_error(retCode));
				break;
			}
		}
	}
	printf("[%d] Published %d config samples.\r\n", mstime() - startTime, nofConfigTopics * nofConfigInstances);
	for (i = 0; i < nofStateTopics; i++) {
		for (j = 0; j < nofStateInstances; j++) {
			state.key = j;
			retCode = DDS_DataWriter_write(stateDataWriter[i], &state, stateInstance[i][j]);
			if (retCode != DDS_RETCODE_OK) {
				fprintf(stderr, "Error publishing sample (%s).\r\n", DDS_error(retCode));
				break;
			}
		}
	}
	printf("[%d] Published %d state samples.\r\n", mstime() - startTime, nofStateTopics * nofStateInstances);
	for (i = 0; i < nofStatisticTopics; i++) {
		for (j = 0; j < nofStatisticInstances; j++) {
			statistic.key = j;
			retCode = DDS_DataWriter_write(statisticDataWriter[i], &statistic, statisticInstance[i][j]);
			if (retCode != DDS_RETCODE_OK) {
				fprintf(stderr, "Error publishing sample (%s).\r\n", DDS_error(retCode));
				break;
			}
		}
	}
	printf("[%d] Published %d statistic samples.\r\n", mstime() - startTime, nofStatisticTopics * nofStatisticInstances);
	sleep(1);

	if (stagedLoading) {
		char file_name[24];
		sprintf(file_name, "/tmp/acceptStageLoad%d", firstConfigTopic + nofConfigTopics);
		FILE *f = fopen(file_name, "w");
		fclose(f);
	}

//	dbg_printf ("Zzzzzzz ... \r\n");
	sleep(TEST_TIME);
//	dbg_printf ("... o?\r\n");

	/* Delete contained entities... */
//	printf (" -- deleting contained entities.\r\n");
	retCode = DDS_DomainParticipant_delete_contained_entities(domainParticipant);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error deleting contained entities.\r\n");
		return -1;
	}

	for (i = 0; i < nofConfigTopics; i++)
		free (configTopicName[i]);

//	printf (" -- contained entities deleted!\r\n");
//	sleep(TEST_TIME);

	/* Delete domain participants... */
	retCode = DDS_DomainParticipantFactory_delete_participant(domainParticipant);
	if (retCode != DDS_RETCODE_OK) {
		fprintf(stderr, "Error deleting domain participant.\r\n");
		return -1;
	}

	printf ("[%d] comp completed successfully.\r\n", mstime() - startTime); 
	return 0;
}

