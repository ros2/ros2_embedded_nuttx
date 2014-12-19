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

/* dcps_dpfact.c -- DomainParticipantFactory methods. */

#include <stdio.h>
#include "log.h"
#include "prof.h"
#include "ctrace.h"
#include "config.h"
#include "dds/dds_dcps.h"
#include "dds_data.h"
#include "dds.h"
#include "rtps.h"
#include "domain.h"
#include "disc.h"
#include "dcps_priv.h"
#include "dcps_builtin.h"
#include "dcps_dpfact.h"
#include "error.h"
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_data.h"
#include "sec_auth.h"
#include "sec_access.h"
#endif
#endif

static unsigned nparticipants;
static int autoenable_created_entities = 1;

DDS_DomainParticipantQos dcps_def_participant_qos = {
/* TopicData */
	{ DDS_SEQ_INITIALIZER (unsigned char) },
/* EntityFactory */
	{ 1 }
};

DDS_DomainParticipant DDS_DomainParticipantFactory_create_participant (
			DDS_DomainId_t			    domain,
			const DDS_DomainParticipantQos	    *qos,
			const DDS_DomainParticipantListener *listener,
			DDS_StatusMask			    mask)
{
	Domain_t	*dp;
	unsigned	part_id;
	GuidPrefix_t	prefix;
#ifdef DDS_SECURITY
	unsigned	secure;
	uint32_t	sec_caps;
	Permissions_t	permissions;
#ifdef DDS_NATIVE_SECURITY
	DDS_ReturnCode_t ret;
#else
	unsigned char	buffer [128];
	size_t		length;
#endif
#endif

	if (!nparticipants) {
#ifdef CTRACE_USED
		log_fct_str [DCPS_ID] = dcps_fct_str;
#endif
		dds_init ();
	}

	ctrc_begind (DCPS_ID, DCPS_DPF_C_PART, &domain, sizeof (domain));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	prof_start (dcps_create_part);

	if (qos == DDS_PARTICIPANT_QOS_DEFAULT)
		qos = &dcps_def_participant_qos;
	else if (!qos_valid_participant_qos (qos))
		return (NULL);

	part_id = guid_new_participant (&prefix, domain);
	if (part_id == ILLEGAL_PID)
		return (NULL);

#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	if (!local_identity)
		authenticate_participant (&prefix);
#endif

	/* Check if security policy allows us to join the domain! */
	permissions = validate_local_permissions (domain, local_identity);
	if (check_create_participant (permissions, qos, &secure)) {
		log_printf (DCPS_ID, 0, "DDS: create_participant(%u): secure domain - access denied!\r\n", domain);
		return (NULL);
	}
	else if (secure) {
		log_printf (DCPS_ID, 0, "DDS: create_participant(%u): secure domain - access granted!\r\n", domain);
		sec_caps = get_domain_security (domain);
	}
	else
		sec_caps = 0;
#endif
	dp = domain_create (domain);
	if (!dp)
		return (NULL);

	dp->participant_id = part_id;
	dp->participant.p_guid_prefix = prefix;
	dp->autoenable = qos->entity_factory.autoenable_created_entities;

#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	log_printf (DCPS_ID, 0, "DDS: domain security mode = 0x%x\r\n", secure);
	dp->security = secure & DDS_SECA_LEVEL;
	dp->access_protected = (secure & DDS_SECA_ACCESS_PROTECTED) != 0;
	if ((secure & DDS_SECA_RTPS_PROTECTED) != 0)
		dp->rtps_protected = secure >> DDS_SECA_ENCRYPTION_SHIFT;
	else
		dp->rtps_protected = 0;

#else
	dp->security = secure;
#endif
	dp->participant.p_permissions = permissions;
	dp->participant.p_sec_caps = sec_caps;
	dp->participant.p_sec_locs = NULL;
	dp->participant.p_id_tokens = dp->participant.p_p_tokens = NULL;
	if (secure) {
#ifdef DDS_NATIVE_SECURITY
		dp->participant.p_id = local_identity;
		dp->participant.p_id_tokens = sec_get_identity_tokens (local_identity,
							 (sec_caps & 0xffff) | 
							    (sec_caps >> 16),
							 &ret);
		if (!dp->participant.p_id_tokens)
			warn_printf ("DDS: can't derive identity tokens!");
		dp->participant.p_p_tokens = sec_get_permissions_tokens (permissions,
							   (sec_caps & 0xffff) | 
							    (sec_caps >> 16));
		if (!dp->participant.p_p_tokens)
			warn_printf ("DDS: can't derive permissions token!");
#else
		length = sizeof (buffer);
		if (get_identity_token (local_identity, buffer, &length))
			warn_printf ("DDS: can't derive identity token!");
		else {
			dp->participant.p_id_tokens = str_new ((char *) buffer, length, length, 0);
			if (!dp->participant.p_id_tokens)
				warn_printf ("DDS: out-of-memory for identity token!");
		}
		length = sizeof (buffer);
		if (get_permissions_token (permissions, buffer, &length))
			warn_printf ("DDS: can't derive permissions token!");
		else {
			dp->participant.p_p_tokens = str_new ((char *) buffer, length, length, 0);
			if (!dp->participant.p_p_tokens)
				warn_printf ("DDS: out-of-memory for permissions token!");
		}
#endif
	}
#endif
#ifdef DDS_FORWARD
	dp->participant.p_forward = config_get_number (DC_Forward, 0);;
#endif
	dp->participant.p_user_data = qos_octets2str (&qos->user_data.value);
	if (dds_entity_name)
		dp->participant.p_entity_name = str_new_cstr (dds_entity_name);
	else
		dp->participant.p_entity_name = NULL;
	if (listener)
		dp->listener = *listener;
	else
		memset (&dp->listener, 0, sizeof (dp->listener));
	dp->mask = mask;
	dp->def_topic_qos = qos_def_topic_qos;
	dp->def_publisher_qos = qos_def_publisher_qos;
	dp->def_subscriber_qos = qos_def_subscriber_qos;
	
	nparticipants++;

	lock_release (dp->lock);

	if (autoenable_created_entities)
		DDS_DomainParticipant_enable (dp);

	prof_stop (dcps_create_part, 1);
	return (dp);
}

DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_participant (DDS_DomainParticipant dp)
{
	Condition_t			*cp;
	DDS_ReturnCode_t		ret;

	ctrc_printd (DCPS_ID, DCPS_DPF_D_PART, &dp, sizeof (dp));
	prof_start (dcps_delete_part);
	if (!domain_ptr (dp, 0, &ret))
		return (ret);

	domain_close (dp->index);
        lock_take (dp->lock);

	if (dp->publishers.head || dp->subscribers.head) {
		lock_release (dp->lock);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}

#ifdef DCPS_BUILTIN_READERS
	dcps_delete_builtin_readers (dp);
#endif
#ifdef RTPS_USED

	if ((dp->participant.p_flags & EF_ENABLED) != 0 && rtps_used)

		/* Delete RTPS-specific participant data (automatically deletes
		   Discovery participant data as well). */
		rtps_participant_delete (dp);
#endif

	/* Delete Status Condition if it was created. */
	if (dp->condition) {
		cp = (Condition_t *) dp->condition;
		if (cp->deferred)
			dds_defer_waitset_undo (dp, dp->condition);
		dcps_delete_status_condition ((StatusCondition_t *) dp->condition);
		dp->condition = NULL;
	}

#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	token_unref (dp->participant.p_id_tokens);
	token_unref (dp->participant.p_p_tokens);
#else
	if (dp->participant.p_id_tokens)
		str_unref (dp->participant.p_id_tokens);
	if (dp->participant.p_p_tokens)
		str_unref (dp->participant.p_p_tokens);
#endif
	dp->participant.p_id_tokens = NULL;
	dp->participant.p_p_tokens = NULL;
#endif

	/* Delete participant QoS data. */
	if (dp->participant.p_user_data) {
		str_unref (dp->participant.p_user_data);
		dp->participant.p_user_data = NULL;
	}

	/* Delete entity name. */
	if (dp->participant.p_entity_name) {
		str_unref (dp->participant.p_entity_name);
		dp->participant.p_entity_name = NULL;
	}

	/* Remove domain from list of valid domains. */
	domain_detach (dp);

	/* Remove relays from domain. */
	if (dp->nr_relays) {
		xfree (dp->relays);
		dp->relays = NULL;
	}
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	if (dp->participant.p_id)
		sec_release_identity (dp->participant.p_id);
	if (dp->participant.p_permissions)
		sec_release_permissions (dp->participant.p_permissions);
#endif
#endif
	/* Finally delete the generic participant. */
	domain_delete (dp);

	prof_stop (dcps_delete_part, 1);

	if (!--nparticipants) {
		dds_final ();
#ifdef PROFILE
		prof_list();
#endif
	}
	return (DDS_RETCODE_OK);
}

DDS_DomainParticipant DDS_DomainParticipantFactory_lookup_participant (DDS_DomainId_t domain)
{
	DDS_DomainParticipant	p;

	ctrc_printd (DCPS_ID, DCPS_DPF_L_PART, &domain, sizeof (domain));
	dds_lock_domains ();
	p = domain_lookup (domain);
	dds_unlock_domains ();
	return (p);
}

DDS_ReturnCode_t DDS_DomainParticipantFactory_get_default_participant_qos (DDS_DomainParticipantQos *qos)
{
	ctrc_printd (DCPS_ID, DCPS_DPF_G_DP_QOS, &qos, sizeof (qos));
	if (qos)
		*qos = dcps_def_participant_qos;

	return (qos ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER);
}

DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos (DDS_DomainParticipantQos *qos)
{
	ctrc_printd (DCPS_ID, DCPS_DPF_S_DP_QOS, &qos, sizeof (qos));
	if (qos == DDS_PARTICIPANT_QOS_DEFAULT)
		dcps_def_participant_qos = qos_def_participant_qos;
	else if (qos_valid_participant_qos (qos))
		dcps_def_participant_qos = *qos;
	else
		return (DDS_RETCODE_INCONSISTENT_POLICY);

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DomainParticipantFactory_get_qos (DDS_DomainParticipantFactoryQos *qos)
{
	ctrc_printd (DCPS_ID, DCPS_DPF_G_QOS, &qos, sizeof (qos));
	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	qos->entity_factory.autoenable_created_entities = autoenable_created_entities;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos (DDS_DomainParticipantFactoryQos *qos)
{
	ctrc_printd (DCPS_ID, DCPS_DPF_S_QOS, &qos, sizeof (qos));
	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	autoenable_created_entities = qos->entity_factory.autoenable_created_entities;
	return (DDS_RETCODE_OK);
}


