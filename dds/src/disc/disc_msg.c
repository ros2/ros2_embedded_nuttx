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

/* disc_msg.c -- Implements the Participant message procedures for discovery. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "error.h"
#include "domain.h"
#include "dds.h"
#include "guard.h"
#include "dds/dds_tsm.h"
#include "dds/dds_dcps.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_ep.h"
#include "disc_msg.h"

#ifdef SIMPLE_DISCOVERY

static DDS_TypeSupport	dds_participant_msg_ts;

static const DDS_TypeSupport_meta dds_participant_msg_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 3, "ParticipantMessageData", sizeof (ParticipantMessageData), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ARRAY, 1, "participantGuidPrefix", 0, 0, sizeof (GuidPrefix_t), 0, NULL },
	{ CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL },
	{ CDR_TYPECODE_ARRAY, 1, "kind", 0, offsetof (ParticipantMessageData, kind), 4, 0, NULL },
	{ CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL },
	{ CDR_TYPECODE_SEQUENCE, 2, "data", 0, offsetof (ParticipantMessageData, data), 0, 0, NULL },
	{ CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL } 
};

#ifdef DISC_MSG_DUMP

static void msg_dump_data (DDS_OctetSeq *sp)
{
	unsigned		i;
	const unsigned char	*cp;

	if (!DDS_SEQ_LENGTH (*sp)) {
		log_printf (SPDP_ID, 0, "<empty>\r\n");
		return;
	}
	for (i = 0, cp = sp->_buffer; i < DDS_SEQ_LENGTH (*sp); i++) {
		if ((i & 0xf) == 0)
			log_printf (SPDP_ID, 0, "\t%04u: ", i);
		log_printf (SPDP_ID, 0, "%02x ", *cp++);
	}
	log_printf (SPDP_ID, 0, "\r\n");
}

/* msg_data_info -- A Participant Message was received from a remote participant. */

static void msg_data_info (Participant_t          *pp,
			   ParticipantMessageData *dp,
			   char                   dir,
			   InfoType_t             type)
{
	uint32_t	*lp = (uint32_t *) &dp->participantGuidPrefix;

	ARG_NOT_USED (pp)

	log_printf (SPDP_ID, 0, "MSG-%c: %08x:%08x:%08x - ", dir, ntohl (lp [0]), ntohl (lp [1]), ntohl (lp [2]));
	if ((dp->kind [0] & 0x80) != 0)
		log_printf (SPDP_ID, 0, "Vendor: %02x.%02x.%02x.%02x, ",
			dp->kind [0], dp->kind [1], dp->kind [2], dp->kind [3]);
	else if (dp->kind [0] == 0 &&
		 dp->kind [1] == 0 &&
		 dp->kind [2] == 0 &&
		 (dp->kind [3] == 1 || dp->kind [3] == 2))
		if (dp->kind [3] == 1)
			log_printf (SPDP_ID, 0, "Auto Liveliness Update, ");
		else
			log_printf (SPDP_ID, 0, "Manual Liveliness Update, ");
	else
		log_printf (SPDP_ID, 0, "Reserved: %02x.%02x.%02x.%02x, ",
			dp->kind [0], dp->kind [1], dp->kind [2], dp->kind [3]);
	switch (type) {
		case EI_NEW:
			log_printf (SPDP_ID, 0, "{New} ");
			msg_dump_data (&dp->data);
			break;
		case EI_UPDATE:
			msg_dump_data (&dp->data);
			break;
		case EI_DELETE:
			log_printf (SPDP_ID, 0, "{deleted}!\r\n");
			break;
	}
}

#endif

/* msg_data_event -- Receive a Participant Message from a remote participant. */

void msg_data_event (Reader_t *rp, NotificationType_t t, int secure)
{
	Domain_t		*dp = rp->r_subscriber->domain;
	Participant_t		*pp;
	unsigned		nchanges;
	ChangeData_t		change;
	ParticipantMessageData	*info = NULL;
#ifdef DISC_MSG_DUMP
	InfoType_t		type;
#endif
	int			error;

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
			/*error = hc_get_key (cdp, change.h, &info, 0);
			if (error)
				continue;*/

#ifdef DISC_MSG_DUMP
			type = EI_DELETE;
#endif
			hc_inst_free (rp->r_cache, change.h);
		}
		else {
#ifdef DISC_MSG_DUMP
			if (change.is_new)
				type = EI_NEW;
			else
				type = EI_UPDATE;
#endif
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
#ifdef DISC_MSG_DUMP

			/* Message from remote participant. */
			if (spdp_log)
				msg_data_info (pp, info, 'R', type);
#endif
			if (info->kind [0] == 0 &&
			    info->kind [1] == 0 &&
			    info->kind [2] == 0 &&
			    (info->kind [3] == 1 || info->kind [3] == 2)) {
				pp = participant_lookup (dp, &info->participantGuidPrefix);
				if (pp)
					liveliness_participant_event (pp, info->kind [3] != 1);
			}
			xfree (info);
		}
		hc_inst_free (rp->r_cache, change.h);
	}
	while (nchanges);
}

/* msg_init -- Initialize the message type. */

int msg_init (void)
{
	dds_participant_msg_ts = DDS_DynamicType_register (dds_participant_msg_tsm);
	if (!dds_participant_msg_ts) {
		fatal_printf ("Can't register ParticipantMessageData type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

/* msg_final -- Finalize the message type. */

void msg_final (void)
{
	DDS_DynamicType_free (dds_participant_msg_ts);
}

/* msg_start -- Start the Participant message reader/writer.
		On entry/exit: no locks used. */

int msg_start (Domain_t *dp)
{
	Reader_t	*rp;
	TopicType_t	*tp;
	int		error;

	error = DDS_DomainParticipant_register_type ((DDS_DomainParticipant) dp,
						     dds_participant_msg_ts,
						     "ParticipantMessageData");
	if (error) {
		warn_printf ("disc_start: can't register ParticipantMessageData type!");
		return (error);
	}
	if (lock_take (dp->lock)) {
		warn_printf ("disc_start: domain lock error (2)");
		return (DDS_RETCODE_ERROR);
	}
	tp = type_lookup (dp, "ParticipantMessageData");
	if (tp)
		tp->flags |= EF_BUILTIN;
	lock_release (dp->lock);

	/* Create builtin Participant Message Reader. */
	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_R,
					 0, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Attach to builtin Participant Message Reader. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_MSG_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("msg_start: can't register Message Reader!");
		return (error);
	}

	/* Create builtin Participant Message Writer. */
	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_W,
					 1, 1,
					 1, 0, 1,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

	if (NATIVE_SECURITY (dp)) {

		/* Create builtin Participant Secure Message Reader. */
		error = create_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_SEC_R,
						 0, 1,
						 1, 0, 1,
						 NULL,
						 dp->participant.p_meta_ucast,
						 dp->participant.p_meta_mcast,
						 NULL);
		if (error)
			return (error);

		/* Attach to builtin Participant Secure Message Reader. */
		rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_MSG_SEC_R];
		error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
		if (error) {
			fatal_printf ("msg_start: can't register secure Message Reader!");
			return (error);
		}

		/* Create builtin Participant Secure Message Writer. */
		error = create_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_SEC_W,
						 1, 1,
						 1, 0, 1,
						 NULL,
						 dp->participant.p_meta_ucast,
						 dp->participant.p_meta_mcast,
						 NULL);
		if (error)
			return (error);
	}
#endif
	return (DDS_RETCODE_OK);
}

/* msg_send_liveliness -- Send a liveliness update via the message writer. */

int msg_send_liveliness (Domain_t *dp, unsigned kind)
{
	ParticipantMessageData	msgd;
	Writer_t		*wp;
	DDS_Time_t		time;
	int			error;

	if (!domain_ptr (dp, 1, (DDS_ReturnCode_t *) &error))
		return (error);

	msgd.participantGuidPrefix = dp->participant.p_guid_prefix;
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_MSG_W];
	lock_release (dp->lock);

	msgd.kind [0] = msgd.kind [1] = msgd.kind [2] = 0;
	if (kind == 0)	/* Automatic mode. */
		msgd.kind [3] = 1;
	else if (kind == 1) /* Manual mode. */
		msgd.kind [3] = 2;
	else
		msgd.kind [3] = 0;
	msgd.data._length = msgd.data._maximum = 0;
	msgd.data._esize = 1;
	msgd.data._own = 1;
	msgd.data._buffer = NULL;
	sys_gettime ((Time_t *) &time);
	error = DDS_DataWriter_write_w_timestamp (wp, &msgd, 0, &time);

#ifdef DISC_MSG_DUMP

	/* Message to remote participant. */
	if (spdp_log)
		msg_data_info (&dp->participant, &msgd, 'T', 0);
#endif
	return (error);
}

/* msg_disable -- Disable the Participant message reader/writer.
		  On entry/exit: domain and global lock taken. */

void msg_disable (Domain_t *dp)
{
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_R);
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_W);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		disable_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_SEC_R);
		disable_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_SEC_W);
	}
#endif
}

/* msg_stop -- Stop the Participant message reader/writer.
	       On entry/exit: domain and global lock taken. */

void msg_stop (Domain_t *dp)
{
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_R);
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_W);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		delete_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_SEC_R);
		delete_builtin_endpoint (dp, EPB_PARTICIPANT_MSG_SEC_W);
	}
#endif
	DDS_DomainParticipant_unregister_type ((DDS_DomainParticipant) dp,
						     dds_participant_msg_ts,
						     "ParticipantMessageData");
}

/* msg_connect -- Connect the messaging endpoints to the peer participant. */

void msg_connect (Domain_t *dp, Participant_t *rpp)
{
	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_R)) != 0)
		connect_builtin (dp, EPB_PARTICIPANT_MSG_W, rpp, EPB_PARTICIPANT_MSG_R);
	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_W)) != 0)
		connect_builtin (dp, EPB_PARTICIPANT_MSG_R, rpp, EPB_PARTICIPANT_MSG_W);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_SEC_R)) != 0)
			connect_builtin (dp, EPB_PARTICIPANT_MSG_SEC_W, rpp, EPB_PARTICIPANT_MSG_SEC_R);
		if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_SEC_W)) != 0)
			connect_builtin (dp, EPB_PARTICIPANT_MSG_SEC_R, rpp, EPB_PARTICIPANT_MSG_SEC_W);
	}
#endif
}

/* msg_disconnect -- Disconnect the messaging endpoints from the peer. */

void msg_disconnect (Domain_t *dp, Participant_t *rpp)
{
	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_R)) != 0)
		disconnect_builtin (dp, EPB_PARTICIPANT_MSG_W, rpp, EPB_PARTICIPANT_MSG_R);
	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_W)) != 0)
		disconnect_builtin (dp, EPB_PARTICIPANT_MSG_R, rpp, EPB_PARTICIPANT_MSG_W);
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (NATIVE_SECURITY (dp)) {
		if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_SEC_R)) != 0)
			disconnect_builtin (dp, EPB_PARTICIPANT_MSG_SEC_W, rpp, EPB_PARTICIPANT_MSG_SEC_R);
		if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_MSG_SEC_W)) != 0)
			disconnect_builtin (dp, EPB_PARTICIPANT_MSG_SEC_R, rpp, EPB_PARTICIPANT_MSG_SEC_W);
	}
#endif
}

#endif /* SIMPLE_DISCOVERY */

