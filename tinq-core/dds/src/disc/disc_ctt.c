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

/* disc_ctt.c -- Implements the Crypto Tokens transport protocol which is used
		 to transfer plugin crypto data to peer participants. */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include "sys.h"
#include "log.h"
#include "error.h"
#include "dds.h"
#include "dcps.h"
#include "dds/dds_debug.h"
#if defined (NUTTX_RTOS)
#include "dds/dds_plugin.h"
#else
#include "dds/dds_security.h"
#endif
#ifdef DDS_SECURITY
#include "security.h"
#ifdef DDS_NATIVE_SECURITY
#include "sec_auth.h"
#include "sec_access.h"
#include "sec_crypto.h"
#include "disc_ep.h"
#include "disc_psmp.h"
#include "disc_ctt.h"
#include "disc_qeo.h"

#ifdef DDS_QEO_TYPES
static VOL_DATA_CB cb_fct = NULL;
#endif

#ifdef CTT_TRACE_MSG

#define CTT_TRACE(d,m)	ctt_trace(d,m)

#ifdef CTT_TRACE_DATA

/* ctt_dump_binary_value -- Dump an OctetSequence. */

static void ctt_dump_binary_value (unsigned i, DDS_OctetSeq *sp)
{
	log_printf (SPDP_ID, 0, "   BV%u:\r\n", i);
	log_print_region (SPDP_ID, 0, DDS_SEQ_DATA (*sp), DDS_SEQ_LENGTH (*sp), 1, 1);
}
#endif

/* ctt_trace -- Trace tx/rx of crypto tokens. */

static void ctt_trace (char dir, DDS_ParticipantVolatileSecureMessage *m)
{
#ifdef CTT_TRACE_DATA
	DDS_CryptoToken	*cp;
	unsigned	i;
#endif
	char		buf [40];

	if (dir == 'T')
		guid_prefix_str ((GuidPrefix_t *) m->destination_participant_key, buf);
	else
		guid_prefix_str ((GuidPrefix_t *) m->message_identity.source_guid, buf);
	log_printf (SPDP_ID, 0, "CTT: %c [%s].%llu - %s",
		dir,
		buf, 
		m->message_identity.sequence_number,
		m->message_class_id);
	if (strcmp (m->message_class_id, GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS)) {
		log_printf (SPDP_ID, 0, " (%s->", entity_id_str ((EntityId_t *) (m->source_endpoint_key + 12), buf));
		log_printf (SPDP_ID, 0, "%s)", entity_id_str ((EntityId_t *) (m->destination_endpoint_key + 12), buf));
	}
	log_printf (SPDP_ID, 0, "\r\n");
#ifdef CTT_TRACE_DATA
/*	if (strcmp (m->message_class_id, GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS)) {
		log_printf (SPDP_ID, 0, "       {%s -> ",
			guid_str ((GUID_t *) m->source_endpoint_key, buf));
		log_printf (SPDP_ID, 0, "%s}\r\n",
			guid_str ((GUID_t *) m->destination_endpoint_key, buf));
	} */
	for (i = 0; i < DDS_SEQ_LENGTH (m->message_data); i++) {
		cp = DDS_SEQ_ITEM_PTR (m->message_data, i);
		log_printf (SPDP_ID, 0, "  %u: %s:\r\n", i, cp->class_id);
		if (cp->binary_value1)
			ctt_dump_binary_value (1, cp->binary_value1);
		if (cp->binary_value2)
			ctt_dump_binary_value (2, cp->binary_value2);
	}
#endif
}

#else
#define	CTT_TRACE(d,m)
#endif

#ifdef CTT_CHECK
#define	CTT_ASSERT(dp)	ctt_assert(dp)
#else
#define	CTT_ASSERT(dp)
#endif

/* ctt_send -- Send crypto tokens to a peer participant/endpoint. */

void ctt_send (Domain_t                             *dp,
	       Participant_t                        *pp,
	       Endpoint_t                           *sep,
	       Endpoint_t                           *dep,
	       DDS_ParticipantVolatileSecureMessage *msg)
{
	Writer_t			*wp;
	DDS_Time_t			time;
	DDS_InstanceHandleSeq		handles;
	DDS_InstanceHandle_t		h;
	int				error;

	CTT_ASSERT (dp);

	memcpy (msg->message_identity.source_guid, dp->participant.p_guid_prefix.prefix, 12);
	msg->message_identity.sequence_number = psmp_seqnr++;
	if (pp)
		memcpy (msg->destination_participant_key, pp->p_guid_prefix.prefix, 12);
	if (dep) {
		memcpy (msg->destination_endpoint_key, pp->p_guid_prefix.prefix, 12);
		memcpy (msg->destination_endpoint_key + 12, dep->entity_id.id, 4);
		memcpy (msg->source_endpoint_key, dp->participant.p_guid_prefix.prefix, 12);
		memcpy (msg->source_endpoint_key + 12, sep->entity_id.id, 4);
	}
	CTT_TRACE ('T', msg);
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_W];

	/* Send participant data. */
	if (pp) {
		DDS_SEQ_INIT (handles);
		h = pp->p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_R]->entity.handle;
		DDS_SEQ_LENGTH (handles) = DDS_SEQ_MAXIMUM (handles) = 1;
		DDS_SEQ_DATA (handles) = &h;
		sys_gettime ((Time_t *) &time);
		error = DDS_DataWriter_write_w_timestamp_directed (wp, msg, 0, &time, &handles);
		if (error)
			fatal_printf ("ctt_send: error sending crypto tokens message!");
	} else {
		sys_gettime ((Time_t *) &time);
		error = DDS_DataWriter_write_w_timestamp (wp, msg, 0, &time);
		if (error)
			fatal_printf ("ctt_send: error sending crypto tokens message!");

	}
	CTT_ASSERT (dp);
}

/* ctt_participant_crypto_tokens -- Handle Participant Crypto Tokens. */

static void ctt_participant_crypto_tokens (
				Domain_t                             *dp,
				DDS_ParticipantVolatileSecureMessage *info)
{
	Participant_t	*pp;

	pp = participant_lookup (dp, (GuidPrefix_t *) info->message_identity.source_guid);
	if (!pp)
		return;

	sec_set_remote_participant_tokens (dp->participant.p_crypto,
					   pp->p_crypto,
					   &info->message_data);
}

/* ctt_data_writer_crypto_tokens -- Handle DataWriter Crypto Tokens. */

static void ctt_data_writer_crypto_tokens (
				Domain_t                             *dp,
				DDS_ParticipantVolatileSecureMessage *info)
{
	Participant_t		*pp;
	Endpoint_t		*lep, *rep;
	EntityId_t		*eidp;
	DataWriterCrypto_t 	crypto;

	pp = participant_lookup (dp, (GuidPrefix_t *) info->source_endpoint_key);
	if (!pp)
		return;

	if (memcmp (dp->participant.p_guid_prefix.prefix,
		    info->destination_endpoint_key,
		    12)) {
		warn_printf ("CTT: DataWriter Token error: incorrect participant GUID.");
		return;
	}
	lep = endpoint_lookup (&dp->participant, (EntityId_t *) &info->destination_endpoint_key [12]);
	if (!lep) {
		warn_printf ("CTT: DataWriter Token error: not a local DataReader.");
		return;
	}
	rep = endpoint_lookup (pp, (EntityId_t *) &info->source_endpoint_key [12]);
	if (!rep ||
	    (crypto = rtps_peer_writer_crypto_get ((Reader_t *) lep,
					      (DiscoveredWriter_t *) rep)) == 0) {
		eidp = (EntityId_t *) (info->source_endpoint_key + 12);
		log_printf (SEC_ID, 0, "CTT: DataWriter Token remember (0x%04x)!\r\n", eidp->w);
		sec_remember_remote_datawriter_tokens (((Reader_t *) lep)->r_crypto,
						       pp->p_crypto,
						       eidp,
						       &info->message_data);
		return;
	}
	sec_set_remote_datawriter_tokens (((Reader_t *) lep)->r_crypto,
					  crypto,
					  &info->message_data);
}

/* ctt_data_reader_crypto_tokens -- Handle DataReader Crypto Tokens. */

static void ctt_data_reader_crypto_tokens (
				Domain_t                             *dp,
				DDS_ParticipantVolatileSecureMessage *info)
{
	Participant_t		*pp;
	Endpoint_t		*lep, *rep;
	EntityId_t		*eidp;
	DataReaderCrypto_t 	crypto;

	pp = participant_lookup (dp, (GuidPrefix_t *) info->source_endpoint_key);
	if (!pp)
		return;

	if (memcmp (dp->participant.p_guid_prefix.prefix,
		    info->destination_endpoint_key,
		    12)) {
		warn_printf ("CTT: DataReader Token error: incorrect participant GUID.");
		return;
	}
	lep = endpoint_lookup (&dp->participant, (EntityId_t *) &info->destination_endpoint_key [12]);
	if (!lep) {
		warn_printf ("CTT: DataReader Token error: not a local DataWriter.");
		return;
	}
	rep = endpoint_lookup (pp, (EntityId_t *) &info->source_endpoint_key [12]);
	if (!rep ||
	    (crypto = rtps_peer_reader_crypto_get ((Writer_t *) lep,
					      (DiscoveredReader_t *) rep)) == 0) {
		eidp = (EntityId_t *) (info->source_endpoint_key + 12);
		log_printf (SEC_ID, 0, "CTT: DataReader Token remember (0x%04x)!\r\n", eidp->w);
		sec_remember_remote_datareader_tokens (((Writer_t *) lep)->w_crypto,
						       pp->p_crypto,
						       eidp,
						       &info->message_data);
		return;
	}
	sec_set_remote_datareader_tokens (((Writer_t *) lep)->w_crypto,
					  crypto,
					  &info->message_data);
}

/* ctt_event -- New participant to participant volatile secure writer message
		data available callback function.
		Locked on entry/exit: DP + R(rp). */

void ctt_event (Reader_t *rp, NotificationType_t t)
{
	Domain_t			*dp = rp->r_subscriber->domain;
	ChangeData_t			change;
	DDS_ParticipantStatelessMessage *info = NULL;
	int				error;
#ifdef DDS_QEO_TYPES
	unsigned                        i;
	DDS_DataHolder                  *cp;
#endif

	if (t != NT_DATA_AVAILABLE)
		return;

	rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
	for (;;) {
		error = disc_get_data (rp, &change);
		if (error)
			break;

		/* Unmarshall handshake message. */
		info = change.data;
		if (!info)
			break;

		/* Drop message if not a broadcast or no matching key. */
		if (memcmp (info->destination_participant_key,
				psmp_unknown_key, GUIDPREFIX_SIZE) &&
		    memcmp (info->destination_participant_key,
		    		dp->participant.p_guid_prefix.prefix, GUIDPREFIX_SIZE))
			goto free_data;

		CTT_TRACE ('R', info);

		CTT_ASSERT (dp);

		/* Handle different message classes specifically: */
		if (!info->message_class_id)
			;
		else if (!strcmp (info->message_class_id,
				  GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS))
			ctt_participant_crypto_tokens (dp, info);
		else if (!strcmp (info->message_class_id,
				  GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS))
			ctt_data_writer_crypto_tokens (dp, info);
		else if (!strcmp (info->message_class_id,
				  GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS))
			ctt_data_reader_crypto_tokens (dp, info);
#ifdef DDS_QEO_TYPES
		else if (cb_fct) {
			for (i = 0; i < DDS_SEQ_LENGTH (info->message_data); i++) {
				cp = DDS_SEQ_ITEM_PTR (info->message_data, i);
				(*cb_fct) (cp);
			}
		}
#endif
		CTT_ASSERT (dp);

		/* Free message info. */

	    free_data:
		xfree (info);
		info = NULL;
	}
	if (info)
		xfree (info);
}

/* ctt_start -- Create and register the Crypto Token transport endpoints. */

int ctt_start (Domain_t *dp)
{
#if defined (DDS_TRACE) && defined (CTT_TRACE_RTPS)
	Writer_t	*wp;
#endif

	Reader_t	*rp;
	int		error;

	CTT_ASSERT (dp);

	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_VOL_SEC_W,
					 1, 1,
					 1, 1, 0,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_VOL_SEC_R,
					 0, 1,
					 1, 1, 0,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_R];
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("SPDP: can't register Crypto Token listener!");
		return (error);
	}
#if defined (DDS_TRACE) && defined (CTT_TRACE_RTPS)
	rtps_trace_set (&rp->r_ep, DDS_TRACE_ALL);
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_W];
	rtps_trace_set (&wp->w_ep, DDS_TRACE_ALL);
#endif
	CTT_ASSERT (dp);

	return (DDS_RETCODE_OK);
}

/* ctt_disable -- Disable the Crypto Token transport endpoints. */

void ctt_disable (Domain_t *dp)
{
	CTT_ASSERT (dp);

	/* Disable builtin endpoints. */
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_VOL_SEC_W);
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_VOL_SEC_R);

	CTT_ASSERT (dp);
}

/* ctt_stop -- Stop the Crypto Token transport reader/writer.
	       On entry/exit: domain and global lock taken. */

void ctt_stop (Domain_t *dp)
{
	CTT_ASSERT (dp);

	/* Delete builtin endpoints. */
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_VOL_SEC_W);
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_VOL_SEC_R);

	CTT_ASSERT (dp);
}

/* ctt_connect -- Connect the Crypto Token transport endpoints to the peer. */

void ctt_connect (Domain_t *dp, Participant_t *rpp)
{
	CTT_ASSERT (dp);

	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_VOL_SEC_W)) != 0)
		connect_builtin (dp, EPB_PARTICIPANT_VOL_SEC_R, rpp, EPB_PARTICIPANT_VOL_SEC_W);
	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_VOL_SEC_R)) != 0)
		connect_builtin (dp, EPB_PARTICIPANT_VOL_SEC_W, rpp, EPB_PARTICIPANT_VOL_SEC_R);

	CTT_ASSERT (dp);
}

/* ctt_disconnect -- Disconnect the Crypto Token transport endpoints from the peer. */

void ctt_disconnect (Domain_t *dp, Participant_t *rpp)
{
	CTT_ASSERT (dp);

	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_VOL_SEC_W)) != 0)
		disconnect_builtin (dp, EPB_PARTICIPANT_VOL_SEC_R, rpp, EPB_PARTICIPANT_VOL_SEC_W);
	if ((rpp->p_builtins & (1 << EPB_PARTICIPANT_VOL_SEC_R)) != 0)
		disconnect_builtin (dp, EPB_PARTICIPANT_VOL_SEC_W, rpp, EPB_PARTICIPANT_VOL_SEC_R);

	CTT_ASSERT (dp);
}

#ifdef DDS_QEO_TYPES

/* DDS_Security_write_volatile_data -- a public function to use the volatile writer
                                       to write data other than crypto tokens */

void DDS_Security_write_volatile_data (Domain_t       *dp,
				       DDS_DataHolder *dh)
{
	DDS_ParticipantVolatileSecureMessage msg;
	DDS_ReturnCode_t error;

	memset (&msg, 0, sizeof (msg));

	msg.message_class_id = GMCLASSID_SECURITY_VOL_DATA;

	DDS_SEQ_INIT (msg.message_data);
	error = dds_seq_require (&msg.message_data, 1);
	if (error)
		return;

	DDS_SEQ_ITEM (msg.message_data, 0) = *dh;

	ctt_send (dp, NULL, NULL, NULL, &msg);
}

void DDS_Security_register_volatile_data (VOL_DATA_CB fct)
{
	if (!cb_fct)
		cb_fct = fct;
}
#endif

#ifdef DDS_DEBUG

/* ctt_assert -- Check the validity of the volatile secure reader and writer. */

void ctt_assert (Domain_t *dp)
{
	Reader_t	*rp;
	Writer_t	*wp;

	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_R];
	lock_take (rp->r_lock);
	assert (rtps_endpoint_assert (&rp->r_lep) == DDS_RETCODE_OK);
	lock_release (rp->r_lock);
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_VOL_SEC_W];
	lock_take (wp->w_lock);
	assert (rtps_endpoint_assert (&wp->w_lep) == DDS_RETCODE_OK);
	lock_release (wp->w_lock);
}

#endif
#endif
#endif

