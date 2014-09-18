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

/* disc_policy_updater.c -- Implements the Policy updater message procedures for discovery. */

#ifdef DDS_QEO_TYPES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "error.h"
#include "log.h"
#include "dds_data.h"
#include "domain.h"
#include "dds.h"
#include "list.h"
#include "dds_data.h"
#include "guard.h"
#include "dds/dds_tsm.h"
#include "dds/dds_dcps.h"
#include "disc.h"
#include "disc_cfg.h"
#include "disc_spdp.h"
#include "disc_priv.h"
#include "disc_ep.h"
#include "disc_qeo.h"
#include "disc_match.h"
#include "disc_policy_updater.h"

lock_t part_lock;

typedef struct part_st Part_t;

struct part_st {
	Part_t *next;
	Part_t *prev;
	Timer_t tmr;
	Domain_t *dp;
	Participant_t *p;
};

typedef struct {
	Part_t *head;
	Part_t *tail;
} Part_List_t;

static Part_List_t list;
static int list_init = 0;

static void init_list (void)
{
	if (!list_init) {
		LIST_INIT (list);
	}
	list_init = 1;
}

static void remove_part_node (Part_t *node);

static void part_to (uintptr_t user)
{
	Part_t *node = (Part_t *) user;
	Participant_t *p = node->p;

	log_printf (DISC_ID, 0, "Policy_updater: inconsistent handshake: deploying safety net\r\n");
	disc_ignore_participant (p);
	spdp_timeout_participant (p, TICKS_PER_SEC * 2);
}

static Part_t *get_part_node_by_guid_prefix (Domain_t *dp, GuidPrefix_t guid_prefix)
{
	Part_t *node;

	LIST_FOREACH (list, node)
		if (node->dp->domain_id == dp->domain_id &&
		    node->p && 
		    !memcmp (&node->p->p_guid_prefix, &guid_prefix, sizeof (GuidPrefix_t)))
			return (node);

	return (NULL);
}

static Part_t *get_part_node (Participant_t *p)
{
	Part_t *node;

	LIST_FOREACH (list, node)
		if (node->p == p)
			return (node);

	return (NULL);
}

static Part_t *add_part_node (Domain_t *dp, Participant_t *p, unsigned timer)
{
	Part_t *node;
	char   buf [32];

	if (!(node = get_part_node (p))) {
		if (!(node = xmalloc (sizeof (Part_t)))) {
			return (NULL);
		} else {
			log_printf (DISC_ID, 0, "Policy_updater: add_part_node for %s\r\n", 
				    guid_prefix_str ((GuidPrefix_t *) &p->p_guid_prefix, buf) );
			node->dp = dp;
			node->p = p;
			tmr_init (&node->tmr, "part_node tmr");
			tmr_start_lock (&node->tmr, timer, (uintptr_t) node, part_to, &dp->lock);
			LIST_ADD_HEAD (list, *node);
		}
	}
	else
		tmr_start (&node->tmr, timer, (uintptr_t) node, part_to);
	return (node);
}

static void remove_part_node (Part_t *node)
{
	char buf [32];

	if (node) {
		log_printf (DISC_ID, 0, "Policy_updater: remove_part_node for %s\r\n", 
			    guid_prefix_str ((GuidPrefix_t *) &node->p->p_guid_prefix, buf) );
		tmr_stop (&node->tmr);
		LIST_REMOVE (list, *node);
		xfree (node);
	}
}

static POLICY_VERSION_CB cb_fct = NULL;

static DDS_TypeSupport dds_policy_updater_msg_ts;

static const DDS_TypeSupport_meta dds_policy_updater_msg_tsm [] = {
	{ CDR_TYPECODE_STRUCT, TSMFLAG_KEY|TSMFLAG_DYNAMIC, "PolicyUpdaterMessageData", sizeof (PolicyUpdaterMessageData), 0, 2, 0, NULL },
	{ CDR_TYPECODE_ARRAY, TSMFLAG_KEY, "participantGuidPrefix", 0, 0, sizeof (GuidPrefix_t), 0, NULL },
	{ CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG, 0, "version", 0, offsetof (PolicyUpdaterMessageData, version), 0, 0, NULL }
};

/* policy_updater_data_event -- Receive a Policy Message from a remote participant. */

void policy_updater_data_event (Reader_t *rp, NotificationType_t t, int secure)
{
	Domain_t		*dp = rp->r_subscriber->domain;
	ChangeData_t		change;
	Participant_t           *pp;
	Part_t                  *node;
	PolicyUpdaterMessageData *info = NULL;
	PolicyUpdaterMessageData info2;
	InfoType_t		type;
	int			error;
	unsigned		nchanges;

	ARG_NOT_USED (secure)

	if (t != NT_DATA_AVAILABLE)
		return;

	rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
	do {
		nchanges = 1;
		/*dtrc_print0 ("PMSG: get samples");*/
		error = disc_get_data (rp, &change);
		if (error) {
			/*dtrc_print0 ("- none\r\n");*/
			break;
		}
		/*dtrc_print1 ("- valid(%u)\r\n", change.kind);*/
		if (change.kind != ALIVE) {
			error = hc_get_key (rp->r_cache, change.h, &info2, 0);
			if (error) {
				warn_printf ("policy_updater_event: can't get key on dispose!");
				continue;
			}
			type = EI_DELETE;

			/* Fall back mechanism for inconsistent handshake is no longer needed */
			log_printf (DISC_ID, 0, "Policy updater data event !ALIVE\r\n");
			lock_take (part_lock);
			node = get_part_node_by_guid_prefix (dp, info2.participantGuidPrefix);
			remove_part_node (node);
			lock_release (part_lock);

			/* call callback function with right parameters */
			if (cb_fct)
				(*cb_fct) (info2.participantGuidPrefix, 0, (int) type);
			hc_inst_free (rp->r_cache, change.h);
			continue;
		}
		else {
			if (change.is_new)
				type = EI_NEW;
			else
				type = EI_UPDATE;
			info = change.data;
		}
		pp = entity_participant (change.writer);
		if (!pp ||				/* Not found. */
		    pp == &dp->participant ||		/* Own sent info. */
		    entity_ignored (pp->p_flags)) {	/* Ignored. */
			hc_inst_free (rp->r_cache, change.h);
			continue;	/* Filter out unneeded info. */
		}

		/* If it's a liveliness indication, then propagate it. */ 
		if (info) {

			/* Fall back mechanism for inconsistent handshake is no longer needed */
			log_printf (DISC_ID, 0, "Policy updater data event ALIVE\r\n");
			lock_take (part_lock);
			node = get_part_node_by_guid_prefix (dp, info->participantGuidPrefix);
			remove_part_node (node);
			lock_release (part_lock);

			/* call callback function with right parameters */
			if (cb_fct)
				(*cb_fct) (info->participantGuidPrefix, info->version, (int) type);
			xfree (info);
		}
		/* hc_inst_free (rp->r_cache, change.h); */
	}
	while (nchanges);
}

/* policy_updater_init -- Initialize the policy updater type. */

int policy_updater_init (void)
{
	init_list ();
	lock_init_nr (part_lock, "Pol Updater Liveliness Lock");
	dds_policy_updater_msg_ts = DDS_DynamicType_register (dds_policy_updater_msg_tsm);
	if (!dds_policy_updater_msg_ts) {
		fatal_printf ("Can't register PolicyUpdaterMessageData type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

/* msg_final -- Finalize the message type. */

void policy_updater_final (void)
{
	DDS_DynamicType_free (dds_policy_updater_msg_ts);
}

static uint64_t get_policy_version (DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData data;
	uint64_t            version;

	data.handle = 0;
	data.data = NULL;
	data.kdata = (void *) &version;
	data.length = 0;
	*error = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (*error)
		return (0);

	return (version);
}

/* policy_updater_start -- Start the Policy updater message reader/writer.
		           On entry/exit: no locks used. */

int policy_updater_start (Domain_t *dp)
{
	Reader_t	*rp;
	TopicType_t	*tp;
	int		error;

	error = DDS_DomainParticipant_register_type ((DDS_DomainParticipant) dp,
						     dds_policy_updater_msg_ts,
						     "PolicyUpdaterMessageData");
	if (error) {
		warn_printf ("disc_start: can't register PolicyUpdaterMessageData type!");
		return (error);
	}
	if (lock_take (dp->lock)) {
		warn_printf ("disc_start: domain lock error (2)");
		return (DDS_RETCODE_ERROR);
	}
	tp = type_lookup (dp, "PolicyUpdaterMessageData");
	if (tp)
		tp->flags |= EF_BUILTIN;
	lock_release (dp->lock);

	/* Create builtin Policy Updater Message Reader. */
	error = create_builtin_endpoint (dp, EPB_POLICY_UPDATER_SEC_R,
					 0, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Attach to builtin Policy Updater Message Reader. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_POLICY_UPDATER_SEC_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("msg_start: can't register Message Reader!");
		return (error);
	}

	/* Create builtin Policy Updater Message Writer. */
	error = create_builtin_endpoint (dp, EPB_POLICY_UPDATER_SEC_W,
					 1, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	DDS_Security_write_policy_version (dp, get_policy_version ((DDS_ReturnCode_t *) &error));

	return (DDS_RETCODE_OK);
}

static void policy_updater_cleanup (Domain_t *dp, Participant_t *p)
{
	Part_t	*node;

	ARG_NOT_USED (dp)
	log_printf (DISC_ID, 0, "Policy updater cleanup\r\n");
	lock_take (part_lock);
	node = get_part_node (p);
	if (node)
		remove_part_node (node);
	lock_release (part_lock);
}

/* policy_updater_disable -- Disable the Policy updater message reader/writer.
                             On entry/exit: domain and global lock taken. */

void policy_updater_disable (Domain_t *dp)
{
	disable_builtin_endpoint (dp, EPB_POLICY_UPDATER_SEC_R);
	disable_builtin_endpoint (dp, EPB_POLICY_UPDATER_SEC_W);
}

/* policy_updater_stop -- Stop the Policy updater message reader/writer.
	                  On entry/exit: domain and global lock taken. */

void policy_updater_stop (Domain_t *dp)
{
	delete_builtin_endpoint (dp, EPB_POLICY_UPDATER_SEC_R);
	delete_builtin_endpoint (dp, EPB_POLICY_UPDATER_SEC_W);
	DDS_DomainParticipant_unregister_type ((DDS_DomainParticipant) dp,
						     dds_policy_updater_msg_ts,
						     "PolicyUpdaterMessageData");
}

/* policy_updater_connect -- Connect the messaging endpoints to the peer participant. */

void policy_updater_connect (Domain_t *dp, Participant_t *rpp)
{
	if ((rpp->p_builtins & (1 << EPB_POLICY_UPDATER_SEC_R)) != 0)
		connect_builtin (dp, EPB_POLICY_UPDATER_SEC_W, rpp, EPB_POLICY_UPDATER_SEC_R);
	if ((rpp->p_builtins & (1 << EPB_POLICY_UPDATER_SEC_W)) != 0)
		connect_builtin (dp, EPB_POLICY_UPDATER_SEC_R, rpp, EPB_POLICY_UPDATER_SEC_W);
}

/* policy_updater_disconnect -- Disconnect the messaging endpoints from the peer. */

void policy_updater_disconnect (Domain_t *dp, Participant_t *rpp)
{
	if ((rpp->p_builtins & (1 << EPB_POLICY_UPDATER_SEC_R)) != 0)
		disconnect_builtin (dp, EPB_POLICY_UPDATER_SEC_W, rpp, EPB_POLICY_UPDATER_SEC_R);
	if ((rpp->p_builtins & (1 << EPB_POLICY_UPDATER_SEC_W)) != 0)
		disconnect_builtin (dp, EPB_POLICY_UPDATER_SEC_R, rpp, EPB_POLICY_UPDATER_SEC_W);
	policy_updater_cleanup (dp, rpp);
}

/* policy_updater_write_policy_version -- Send a version update via the message writer. */

int DDS_Security_write_policy_version  (Domain_t *dp, uint64_t version)
{
	PolicyUpdaterMessageData	msgd;
	Writer_t		*wp;
	DDS_Time_t		time;
	int			error;

	if (!domain_ptr (dp, 1, (DDS_ReturnCode_t *) &error))
		return (error);

	msgd.participantGuidPrefix = dp->participant.p_guid_prefix;
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_POLICY_UPDATER_SEC_W];
	lock_release (dp->lock);

	msgd.version = version;
	sys_gettime ((Time_t *) &time);
	error = DDS_DataWriter_write_w_timestamp (wp, &msgd, 0, &time);

	return (error);
}

void DDS_Security_register_policy_version (POLICY_VERSION_CB fct)
{
	if (!cb_fct)
		cb_fct = fct;
}

/* This list will make sure we can recover from the state that one of the participants
   is authenticated, while the other is in a failed state */

void policy_updater_participant_start_timer (Domain_t *dp, Participant_t *p, unsigned timer)
{
	lock_take (part_lock);
	add_part_node (dp, p, timer);
	lock_release (part_lock);
}

#endif
