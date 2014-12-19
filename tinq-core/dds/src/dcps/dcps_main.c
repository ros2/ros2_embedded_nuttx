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

/* dcps_main.c -- Implements the Data-Centric Publish-Subscribe (DCPS) layer of
		  the Data Distribution Standard (DDS). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "log.h"
#include "prof.h"
#include "error.h"
#include "ctrace.h"
#include "pool.h"
#include "strseq.h"
#include "domain.h"
#include "guid.h"
#include "debug.h"
#ifdef RTPS_USED
#include "rtps.h"
#include "rtps_data.h"
#endif
#include "disc.h"
#include "parse.h"
#ifdef XTYPES_USED
#include "xcdr.h"
#include "xdata.h"
#else
#include "cdr.h"
#endif
#include "pl_cdr.h"
#include "dds/dds_types.h"
#include "dds.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "dcps_event.h"
#include "dcps_waitset.h"
#include "guard.h"
#include "dds/dds_aux.h"
#ifdef DDS_DEBUG
#include "dds/dds_debug.h"
#endif
#ifdef DDS_SECURITY
#include "security.h"
#endif
#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#include "dds/dds_dcps.h"

unsigned	dcps_entity_count;

const char *dcps_mem_names [] = {
	"SAMPLE_INFO",
	"WAITSET",
	"STATUS_COND",
	"READ_COND",
	"QUERY_COND",
	"GUARD_COND",
	"TOPIC_WAIT"
};

MEM_DESC_ST	dcps_mem_blocks [MB_END];  /* Memory used by DCPS. */
size_t		dcps_mem_size;		/* Total memory allocated. */

#ifdef PROFILE
PUB_PROF_PID (dcps_create_part)
PUB_PROF_PID (dcps_delete_part)
PUB_PROF_PID (dcps_reg_type)
PUB_PROF_PID (dcps_unreg_type)
PUB_PROF_PID (dcps_create_pub)
PUB_PROF_PID (dcps_delete_pub)
PUB_PROF_PID (dcps_create_sub)
PUB_PROF_PID (dcps_delete_sub)
PUB_PROF_PID (dcps_create_topic)
PUB_PROF_PID (dcps_delete_topic_p)
PUB_PROF_PID (dcps_create_ftopic)
PUB_PROF_PID (dcps_delete_ftopic)
PUB_PROF_PID (dcps_create_writer)
PUB_PROF_PID (dcps_delete_writer_p)
PUB_PROF_PID (dcps_create_reader)
PUB_PROF_PID (dcps_delete_reader)
PUB_PROF_PID (dcps_register)
PUB_PROF_PID (dcps_write_p)
PUB_PROF_PID (dcps_dispose_p)
PUB_PROF_PID (dcps_unregister)
PUB_PROF_PID (dcps_w_key)
PUB_PROF_PID (dcps_w_lookup)
PUB_PROF_PID (dcps_read_take1)
PUB_PROF_PID (dcps_read_take2)
PUB_PROF_PID (dcps_read_take3)
PUB_PROF_PID (dcps_return_loan_p)
PUB_PROF_PID (dcps_r_key)
PUB_PROF_PID (dcps_r_lookup)
PUB_PROF_PID (dcps_ws_wait)
#endif

#ifdef CTRACE_USED

const char *dcps_fct_str [DCPS_NTF_D_OR_IND + 1] = {
	"DynamicType_Register", "DynamicType_Free", "SampleFree",

	"Entity_get_statuscondition", "Entity_get_status_changes",
	"Entity_enable", "Entity_get_instance_handle",

	"WaitSet__alloc", "WaitSet__free",
	"WaitSet_attach_condition", "WaitSet_detach_condition",
	"WaitSet_wait", "WaitSet_get_conditions",

	"GuardCondition_alloc", "GuardCondition_free",
	"GuardCondition_get_trigger_value", "GuardCondition_set_trigger_value",

	"StatusCondition_get_trigger_value",
	"StatusCondition_set_enabled_statuses",
	"StatusCondition_get_enabled_statuses",
	"StatusCondition_get_entity",

	"ReadCondition_get_trigger_value", "ReadCondition_get_datareader",
	"ReadCondition_get_view_state_mask",
	"ReadCondition_get_instance_state_mask",
	"ReadCondition_get_sample_state_mask",

	"QueryCondition_get_trigger_value", "QueryCondition_get_datareader",
	"QueryCondition_get_query_expression",
	"QueryCondition_get_query_parameters",
	"QueryCondition_set_query_parameters",
	"QueryCondition_get_view_state_mask",
	"QueryCondition_get_instance_state_mask",
	"QueryCondition_get_sample_state_mask",

	"DomainParticipantFactory_create_participant",
	"DomainParticipantFactory_delete_participant",
	"DomainParticipantFactory_lookup_participant",
	"DomainParticipantFactory_set_default_participant_qos",
	"DomainParticipantFactory_get_default_participant_qos",
	"DomainParticipantFactory_get_qos", "DomainParticipantFactory_set_qos",

	"DomainParticipant_register_type", "DomainParticipant_unregister_type",
	"DomainParticipant_delete_typesupport",
	"DomainParticipant_get_qos", "DomainParticipant_set_qos",
	"DomainParticipant_get_listener", "DomainParticipant_set_listener",
	"DomainParticipant_get_statuscondition",
	"DomainParticipant_get_status_changes",
	"DomainParticipant_enable", "DomainParticipant_get_instance_handle",
	"DomainParticipant_create_publisher",
	"DomainParticipant_delete_publisher",
	"DomainParticipant_create_subscriber",
	"DomainParticipant_delete_subscriber",
	"DomainParticipant_create_topic", "DomainParticipant_delete_topic",
	"DomainParticipant_create_contentfilteredtopic",
	"DomainParticipant_delete_contentfilteredtopic",
	"DomainParticipant_create_multitopic",
	"DomainParticipant_delete_multitopic",
	"DomainParticipant_find_topic",
	"DomainParticipant_lookup_topicdescription",
	"DomainParticipant_get_builtin_subscriber",
	"DomainParticipant_ignore_participant",
	"DomainParticipant_ignore_topic",
	"DomainParticipant_ignore_publication",
	"DomainParticipant_ignore_subscription",
	"DomainParticipant_get_domain_id",
	"DomainParticipant_delete_contained_entities",
	"DomainParticipant_assert_liveliness",
	"DomainParticipant_set_default_publisher_qos",
	"DomainParticipant_get_default_publisher_qos",
	"DomainParticipant_set_default_subscriber_qos",
	"DomainParticipant_get_default_subscriber_qos",
	"DomainParticipant_set_default_topic_qos",
	"DomainParticipant_get_default_topic_qos",
	"DomainParticipant_get_discovered_participants",
	"DomainParticipant_get_discovered_participant_data",
	"DomainParticipant_get_discovered_topics",
	"DomainParticipant_get_discovered_topic_data",
	"DomainParticipant_contains_entity",
	"DomainParticipant_get_current_time",

	"TopicDescription_get_participant",
	"TopicDescription_get_type_name",
	"TopicDescription_get_name", 

	"Topic_get_qos", "Topic_set_qos",
	"Topic_get_listener", "Topic_set_listener",
	"Topic_get_statuscondition", "Topic_get_status_changes",
	"Topic_enable",	"Topic_get_instance_handle",
	"Topic_get_participant", "Topic_get_type_name", "Topic_get_name",
	"Topic_get_inconsistent_topic_status",

	"ContentFilteredTopic_get_related_topic",
	"ContentFilteredTopic_get_expression_parameters",
	"ContentFilteredTopic_set_expression_parameters",
	"ContentFilteredTopic_get_filter_expression",
	"ContentFilteredTopic_get_participant",
	"ContentFilteredTopic_get_type_name",
	"ContentFilteredTopic_get_name",

	"Publisher_get_qos", "Publisher_set_qos",
	"Publisher_get_listener", "Publisher_set_listener",
	"Publisher_get_statuscondition", "Publisher_get_status_changes",
	"Publisher_enable", "Publisher_get_instance_handle",
	"Publisher_create_datawriter", "Publisher_delete_datawriter",
	"Publisher_lookup_datawriter",
	"Publisher_wait_for_acknowledgments",
	"Publisher_get_participant", "Publisher_delete_contained_entities",
	"Publisher_set_default_datawriter_qos",
	"Publisher_get_default_datawriter_qos",
	"Publisher_copy_from_topic_qos",
	"Publisher_suspend_publications",
	"Publisher_resume_publications",
	"Publisher_begin_coherent_changes",
	"Publisher_end_coherent_changes",

	"DataWriter_get_qos", "DataWriter_set_qos",
	"DataWriter_get_listener", "DataWriter_set_listener",
	"DataWriter_get_statuscondition", "DataWriter_get_status_changes",
	"DataWriter_enable", "DataWriter_get_instance_handle",
	"DataWriter_register_instance", "DataWriter_register_instance_w_timestamp",
	"DataWriter_unregister_instance",
	"DataWriter_unregister_instance_w_timestamp",
	"DataWriter_unregister_instance_directed",
	"DataWriter_unregister_instance_w_timestamp_directed",
	"DataWriter_get_key_value", "DataWriter_lookup_instance",
	"DataWriter_write", "DataWriter_write_w_timestamp",
	"DataWriter_write_directed", "DataWriter_write_w_timestamp_directed",
	"DataWriter_dispose", "DataWriter_dispose_w_timestamp",
	"DataWriter_dispose_directed", "DataWriter_dispose_w_timestamp_directed",
	"DataWriter_wait_for_acknowledgments",
	"DataWriter_get_liveliness_lost_status",
	"DataWriter_get_offered_deadline_missed_status",
	"DataWriter_get_offered_incompatible_qos_status",
	"DataWriter_get_publication_matched_status",
	"DataWriter_get_topic", "DataWriter_get_publisher",
	"DataWriter_get_matched_subscription_data",
	"DataWriter_get_matched_subscriptions", "DataWriter_get_reply_subscriptions",

	"Subscriber_get_qos", "Subscriber_set_qos",
	"Subscriber_get_listener", "Subscriber_set_listener",
	"Subscriber_get_statuscondition", "Subscriber_get_status_changes",
	"Subscriber_enable", "Subscriber_get_instance_handle",
	"Subscriber_create_datareader", "Subscriber_delete_datareader",
	"Subscriber_lookup_datareader",
	"Subscriber_get_datareaders",
	"Subscriber_notify_datareaders",
	"Subscriber_get_participant", "Subscriber_delete_contained_entities",
	"Subscriber_set_default_datareader_qos",
	"Subscriber_get_default_datareader_qos",
	"Subscriber_copy_from_topic_qos",
	"Subscriber_begin_access",
	"Subscriber_end_access",

	"DataReader_get_qos", "DataReader_set_qos",
	"DataReader_get_listener", "DataReader_set_listener",
	"DataReader_get_statuscondition", "DataReader_get_status_changes",
	"DataReader_enable", "DataReader_get_instance_handle",
	"DataReader_read", "DataReader_take", 
	"DataReader_read_w_condition", "DataReader_take_w_condition", 
	"DataReader_read_next_sample", "DataReader_take_next_sample",
	"DataReader_read_instance", "DataReader_take_instance",
	"DataReader_read_next_instance", "DataReader_take_next_instance",
	"DataReader_read_next_instance_w_condition",
	"DataReader_take_next_instance_w_condition",
	"DataReader_return_loan",
	"DataReader_get_key_value", "DataReader_lookup_instance",
	"DataReader_create_readcondition", "DataReader_create_querycondition",
	"DataReader_delete_readcondition",
	"DataReader_get_liveliness_changed_status",
	"DataReader_get_requested_deadline_missed_status",
	"DataReader_get_requested_incompatible_qos_status",
	"DataReader_get_sample_lost_status",
	"DataReader_get_sample_rejected_status",
	"DataReader_get_subscription_matched_status",
	"DataReader_get_topicdescription",
	"DataReader_get_subscriber",
	"DataReader_delete_contained_entities",
	"DataReader_wait_for_historical_data",
	"DataReader_get_matched_publication_data",
	"DataReader_get_matched_publications",

	"notify_data_avail", "notify_data_avail_ind", "notify_data_on_readers_ind"
};
#endif

const char *dds_errors [] = {
	"no error",
	"Generic unspecified error",
	"Unsupported operation",
	"Invalid parameter value",
	"Precondition not met",
	"Not enough memory",
	"Not enabled yet",
	"Immutable policy",
	"Inconsistent policy",
	"Object not found",
	"Timeout occurred",
	"No data available",
	"Illegal operation",
	"Not allowed by security"
};

const char *DDS_error (DDS_ReturnCode_t e)
{
	if (e <= DDS_RETCODE_ILLEGAL_OPERATION)
		return (dds_errors [e]);
	else
		return ("unknown");
}

StatusCondition_t *dcps_new_status_condition (void)
{
	StatusCondition_t	*scp;

	scp = mds_pool_alloc (&dcps_mem_blocks [MB_STATUS_COND]);
	if (!scp)
		return (NULL);

	scp->c.waitset = NULL;
	scp->c.class = CC_STATUS;
	scp->c.next = NULL;
	scp->c.e_next = NULL;
	scp->c.deferred = 0;
	scp->entity = NULL;
	scp->enabled = 0;
	return (scp);
}

void dcps_delete_status_condition (StatusCondition_t *cp)
{
	if (cp) {
		if (cp->c.waitset)
			DDS_WaitSet_detach_condition ((DDS_WaitSet) cp->c.waitset,
						      &cp->c);
		mds_pool_free (&dcps_mem_blocks [MB_STATUS_COND], cp);
	}
}

Strings_t *dcps_new_str_pars (DDS_StringSeq *pars, int *error)
{
	Strings_t	*ssp;
	const char	*cp;
	unsigned	i;

	if (!pars || !DDS_SEQ_LENGTH (*pars)) {
		*error = DDS_RETCODE_OK;
		return (NULL);
	}
	ssp = xmalloc (sizeof (Strings_t));
	if (!ssp)
		goto pars_failed;

	DDS_SEQ_INIT (*ssp);
	for (i = 0; i < DDS_SEQ_LENGTH (*pars); i++) {
		cp = DDS_SEQ_ITEM (*pars, i);
		if (strings_append_cstr (ssp, cp)) {
			strings_delete (ssp);
			goto pars_failed;
		}
	}
	*error = DDS_RETCODE_OK;
	return (ssp);

     pars_failed:
	*error = DDS_RETCODE_OUT_OF_RESOURCES;
	return (NULL);
}


int dcps_update_str_pars (Strings_t **sp, DDS_StringSeq *pars)
{
	char			*cp;
	unsigned		i;
	int			rc;

	if (!pars) {
		if (*sp) {
			strings_delete (*sp);
			*sp = NULL;
		}
		return (DDS_RETCODE_OK);
	}
	else if (!*sp) {
		*sp = dcps_new_str_pars (pars, &rc);
		return (rc);
	}
	if ((rc = dds_seq_require (*sp, DDS_SEQ_LENGTH (*pars))) != DDS_RETCODE_OK)
		return (rc);

	strings_reset (*sp);
	for (i = 0; i < DDS_SEQ_LENGTH (*pars); i++) {
		cp = DDS_SEQ_ITEM (*pars, i);
		if (strings_append_cstr (*sp, cp))
			return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	return (DDS_RETCODE_OK);
}

int dcps_get_str_pars (DDS_StringSeq *pars, Strings_t *sp)
{
	unsigned	i;
	int		error;

	if (!sp)
		DDS_SEQ_LENGTH (*pars) = 0;
	else {
		if ((error = dds_seq_require (pars, DDS_SEQ_LENGTH (*sp))) != DDS_RETCODE_OK)
			return (error);

		for (i = 0; i < DDS_SEQ_LENGTH (*sp); i++)
			DDS_SEQ_ITEM_SET (*pars, i, (char *) str_ptr (DDS_SEQ_ITEM (*sp, i)));
	}
	return (DDS_RETCODE_OK);
}

unsigned dcps_skip_mask (DDS_SampleStateMask   sample_states,
			 DDS_ViewStateMask     view_states,
			 DDS_InstanceStateMask instance_states)
{
	unsigned	mask = 0;

	if ((sample_states & DDS_READ_SAMPLE_STATE) != 0)
		mask |= SKM_READ;
	if ((sample_states & DDS_NOT_READ_SAMPLE_STATE) != 0)
		mask |= SKM_NOT_READ;
	if ((view_states & DDS_NEW_VIEW_STATE) != 0)
		mask |= SKM_NEW_VIEW;
	if ((view_states & DDS_NOT_NEW_VIEW_STATE) != 0)
		mask |= SKM_OLD_VIEW;
	if ((instance_states & DDS_ALIVE_INSTANCE_STATE) != 0)
		mask |= SKM_ALIVE;
	if ((instance_states & DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE) != 0)
		mask |= SKM_DISPOSED;
	if ((instance_states & DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) != 0)
		mask |= SKM_NO_WRITERS;
	return (~mask & SKM_ALL);
}


/**************************/
/*   Overall management	  */
/**************************/

static int dcps_init_memory (const DCPS_CONFIG *cp)
{
	/* Check if already initialized. */
	if (dcps_mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (dcps_mem_blocks, MB_END);
		return (DCPS_OK);
	}

	/* Define the different pool attributes. */
	MDS_POOL_TYPE (dcps_mem_blocks, MB_SAMPLE_INFO, cp->sampleinfos, sizeof (DDS_SampleInfo));
	MDS_POOL_TYPE (dcps_mem_blocks, MB_WAITSET, cp->waitsets, sizeof (WaitSet_t));
	MDS_POOL_TYPE (dcps_mem_blocks, MB_STATUS_COND, cp->statusconds, sizeof (StatusCondition_t));
	MDS_POOL_TYPE (dcps_mem_blocks, MB_READ_COND, cp->readconds, sizeof (ReadCondition_t));
	MDS_POOL_TYPE (dcps_mem_blocks, MB_QUERY_COND, cp->queryconds, sizeof (QueryCondition_t));
	MDS_POOL_TYPE (dcps_mem_blocks, MB_GUARD_COND, cp->guardconds,  sizeof (GuardCondition_t));
	MDS_POOL_TYPE (dcps_mem_blocks, MB_TOPIC_WAIT, cp->topicwaits, sizeof (TopicWait_t));

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	dcps_mem_size = mds_alloc (dcps_mem_blocks, dcps_mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!dcps_mem_size) {
		warn_printf ("dcps_init: not enough memory available!\r\n");
		return (DCPS_ERR_NOMEM);
	}
	log_printf (DCPS_ID, 0, "dcps_init: %lu bytes allocated for pools.\r\n", (unsigned long) dcps_mem_size);
#endif
	return (DCPS_OK);
}

DDS_ReturnCode_t dcps_init (const DCPS_CONFIG *cp)
{
	dcps_init_memory (cp);

	PROF_INIT ("D:CPart", dcps_create_part);
	PROF_INIT ("D:DPart", dcps_delete_part);
	PROF_INIT ("D:RType", dcps_reg_type);
	PROF_INIT ("D:UType", dcps_unreg_type);
	PROF_INIT ("D:CPub", dcps_create_pub);
	PROF_INIT ("D:DPub", dcps_delete_pub);
	PROF_INIT ("D:CSub", dcps_create_sub);
	PROF_INIT ("D:DSub", dcps_delete_sub);
	PROF_INIT ("D:CTopic", dcps_create_topic);
	PROF_INIT ("D:DTopic", dcps_delete_topic_p);
	PROF_INIT ("D:CFTopic", dcps_create_ftopic);
	PROF_INIT ("D:DFTopic", dcps_delete_ftopic);
	PROF_INIT ("D:CWriter", dcps_create_writer);
	PROF_INIT ("D:DWriter", dcps_delete_writer_p);
	PROF_INIT ("D:CReader", dcps_create_reader);
	PROF_INIT ("D:DReader", dcps_delete_reader);
	PROF_INIT ("D:Register", dcps_register);
	PROF_INIT ("D:Write", dcps_write_p);
	PROF_INIT ("D:Dispose", dcps_dispose_p);
	PROF_INIT ("D:Unreg", dcps_unregister);
	PROF_INIT ("D:WKey", dcps_w_key);
	PROF_INIT ("D:WLookup", dcps_w_lookup);
	PROF_INIT ("D:RdTake1", dcps_read_take1);
	PROF_INIT ("D:RdTake2", dcps_read_take2);
	PROF_INIT ("D:RdTake3", dcps_read_take3);
	PROF_INIT ("D:RetLoan", dcps_return_loan_p);
	PROF_INIT ("D:RKey", dcps_r_key);
	PROF_INIT ("D:RLookup", dcps_r_lookup);
	PROF_INIT ("D:WWait", dcps_ws_wait);
#ifdef RTPS_USED
	if (rtps_used)
		disc_register (dcps_notify_match,
			       dcps_notify_unmatch,
			       dcps_notify_done);
#endif
	dds_attach_notifier (NSC_DCPS, dcps_notify_listener);
	return (DDS_RETCODE_OK);
}

void dcps_final (void)
{
#if 0
	Domain_t		*dp;
	DDS_DomainParticipant	p;
	unsigned		i = 0;
#endif

#ifdef RTPS_USED
	if (rtps_used)
		disc_register (NULL, NULL, NULL);
#endif
	dds_attach_notifier (NSC_DCPS, NULL);
#if 0
	while ((dp = domain_next (&i, NULL)) != NULL) {
		p = (DDS_DomainParticipant) (uintptr_t) dp->index;
		DDS_DomainParticipant_delete_contained_entities (p);
		DDS_DomainParticipantFactory_delete_participant (p);
	}
#endif
	mds_free (dcps_mem_blocks, MB_END);
}
