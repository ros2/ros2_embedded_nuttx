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

/* disc_psmp.c -- Implements the Discovery Participant Stateless Message
		  protocol. */

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif
#include "sys.h"
#include "log.h"
#include "error.h"
#include "random.h"
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
#ifdef DDS_QEO_TYPES
#include "disc_qeo.h"
#endif
#include "sec_auth.h"
#include "sec_access.h"
#include "disc.h"
#include "disc_cfg.h"
#include "disc_priv.h"
#include "disc_ep.h"
#include "disc_spdp.h"
#include "disc_sedp.h"
#include "disc_psmp.h"

#ifdef SIMPLE_DISCOVERY

/*#define PSMP_TRACE_MSG	** PSMP handshake message tracing if defined. */
/*#define PSMP_RTPS_TRACE	** PSMP endpoint tracing on RTPS if defined. */
/*#define PSMP_TRACE_HS		** PSMP creation, deletion, getting, ... of Handshakes. */

int64_t		psmp_seqnr = 1;
unsigned char	psmp_unknown_key [16] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

typedef enum {
	IHSS_R_VRI,		/* Need to retry Identity Validation. */
	IHSS_R_REQ,		/* Need to retry Handshake Request. */
	IHSS_W_REQ,		/* Wait for Handshake Request. */
	IHSS_R_REPLY,		/* Need to retry Handshake Reply. */
	IHSS_W_MSG,		/* Wait for continued Handshake message. */
	IHSS_R_HS,		/* Need to retry Process Handshake. */
	IHSS_W_TO		/* Delayed for handshake cleanup. */
} IHS_STATE;

#define	MAX_VRI_RETRIES		3	/* Max. # of validation retries before giving up. */
#define	MAX_REQ_RETRIES		31	/* Max. # of request retries before giving up. */
#define	MAX_REP_RETRIES		31	/* Max. # of reply retries before giving up. */
#define	MAX_HS_RETRIES		31	/* Max. # of handshake retries before giving up. */
#define	MAX_WHS_RETRIES		8	/* Max. # of handshake invites before giving up. */

#define PSMP_V_RETRY_TO		200	/* Identity validation retry timeout (ms). */
#define PSMP_CLEANUP_TO		40000	/* Cleanup handshake context timeout (ms). */
#define PSMP_HS_REQ_TO		1000	/* Waiting before resending request message (ms). */
#define PSMP_REQ_RETRY_TO	900	/* Retry handshake request timeout (ms). (Non handshake protocol required) */
#define PSMP_REPLY_RETRY_TO	900	/* Retry handshake reply timeout (ms).  (Non handshake protocol required) */
#define PSMP_WAIT_REQ_TO	4000	/* Wait initial handshake Request (ms). */
#define PSMP_WAIT_MSG_TO	1000	/* Waiting before sending next handshake message (i.e. reply msg) (ms). */
#define PSMP_WAIT_FAILED_TO	5000	/* Time for an ignored participant to timeout and retry handshake. */

#define	PSMP_MAX_BACKOFF	3	/* Max. power for backoff. */

typedef struct psmp_handshake_st IpspHandshake_t;
struct psmp_handshake_st {
	Domain_t		*domain;	/* Source domain participant. */
	Participant_t		*peer;		/* Discovered Peer participant. */
	IHS_STATE		state;		/* Handshake state. */
	DDS_HandshakeToken	*tx_hs_token; 	/* Last sent token. */
	DDS_HandshakeToken	*rx_hs_token; 	/* Last received token. */
	Token_t			*i_token;	/* Identity Token. */
	Token_t			*p_token;	/* Permissions Token. */
	DDS_PermissionsCredential *p_creds;	/* Credentials. */
	Timer_t			timer;		/* Active Timer. */
	Handshake_t		handle;		/* Security handshake handle. */
	unsigned		retries;	/* # of retries left. */
	unsigned		backoff;	/* Backoff value. */
	int64_t			transaction;	/* Transaction number. */
	int64_t			tx_seqnr;	/* Tx handshake sequence number. */
	int64_t			last_seqnr;	/* Last handshake sequence number. */
	int64_t			last_rel;	/* Last transaction number. */
	int			initiator;	/* Handshake initiator. */
	int			rehandshake;	/* Rehandshake i.o. first time. */
	IpspHandshake_t		*next;		/* Next active handshake. */
};

static IpspHandshake_t	*handshakes;

#ifdef PSMP_TRACE_MSG

static const char *psmp_state_str [] = {
	"RetryIdVal", "RetryHSRequest", "WaitHSRequest", "RetryHSReply",
	"WaitMsg", "RetryHS", "WaitTO"
};

static char *psmp_id (IpspHandshake_t *p)
{
	static char buf [32];

	return (guid_prefix_str (&p->peer->p_guid_prefix, buf));
}

#define psmp_state_i(p,v) log_printf (SPDP_ID, 0, "PSMP: E [%s] - state = %s\r\n", psmp_id (p), psmp_state_str [v]); p->state = v
#define	psmp_state(p,v)	log_printf (SPDP_ID, 0, "PSMP: E [%s] - %s -> %s\r\n", psmp_id (p), psmp_state_str [p->state], psmp_state_str [v]); p->state = v
#else
#define	psmp_state_i(p,v) p->state = v
#define	psmp_state(p,v)	p->state = v
#endif

/* psmp_handshake_clear -- Cleanup a handshake context. */

static void psmp_handshake_clear (IpspHandshake_t *p)
{
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_clear: %p\r\n", (void *) p);
#endif
	tmr_stop (&p->timer);
	if (p->tx_hs_token) {
		DDS_DataHolder__free (p->tx_hs_token);
		p->tx_hs_token = NULL;
	}
	if (p->i_token) {
		token_unref (p->i_token);
		p->i_token = NULL;
	}
	if (p->p_token) {
		token_unref (p->p_token);
		p->p_token = NULL;
	}
	if (p->handle) {
		sec_release_handshake (p->handle);
		p->handle = 0;
	}
}

/* psmp_handshake_get -- Get a new handshake context. */

static IpspHandshake_t *psmp_handshake_get (Domain_t      *dp,
					    Participant_t *pp,
					    int           create,
					    int           reset)
{
	IpspHandshake_t	*p;

	for (p = handshakes; p; p = p->next)
		if (p->domain == dp && p->peer == pp) {
			if (reset)
				psmp_handshake_clear (p);
#ifdef PSMP_TRACE_HS
			log_printf (SEC_ID, 0, "psmp_handshake_get: %p found\r\n", (void *) p);
#endif
			break;
		}

	if (!p && create) {
		p = xmalloc (sizeof (IpspHandshake_t));
		if (!p)
			return (NULL);
#ifdef PSMP_TRACE_HS
		log_printf (SEC_ID, 0, "psmp_handshake_get: %p create\r\n", (void *) p);
#endif
		p->state = IHSS_W_REQ;
		p->domain = dp;
		p->peer = pp;
		p->tx_hs_token = NULL;
		p->rx_hs_token = NULL;
		p->i_token = NULL;
		p->p_token = NULL;
		p->p_creds = NULL;
		tmr_init (&p->timer, "AuthHS");
		p->handle = 0;
		p->retries = 0;
		p->backoff = 0;
		p->transaction = 0;
		p->tx_seqnr = 0;
		p->last_seqnr = 0;
		p->last_rel = 0;
		p->initiator = 0;
		p->next = handshakes;
		handshakes = p;
	}
	return (p);
}

/* psmp_handshake_free -- Free a specific handshake context. */

static void psmp_handshake_free (IpspHandshake_t *p)
{
	IpspHandshake_t	*xp, *prev;
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_free: %p\r\n", (void *) p);
#endif
	psmp_handshake_clear (p);
	for (xp = handshakes, prev = NULL; xp; prev = xp, xp = xp->next)
		if (xp == p) {
			if (prev)
				prev->next = p->next;
			else
				handshakes = p->next;
			xfree (p);
			break;
		}
}

/* psmp_handshake_free_all -- Free all handshake contexts in a domain or the
			      handshake with a specific peer participant. */

static void psmp_handshake_free_all (Domain_t *dp, Participant_t *pp)
{
	IpspHandshake_t	*xp, *prev, *p;

	for (xp = handshakes, prev = NULL; xp; )
		if (xp->domain == dp && (!pp || xp->peer == pp)) {
			p = xp;
			xp = xp->next;
			if (prev)
				prev->next = xp;
			else
				handshakes = xp;
			psmp_handshake_clear (p);
			xfree (p);
		}
		else {
			prev = xp;
			xp = xp->next;
		}
}

typedef enum {
	IHSE_TOKEN,
	IHSE_TO
} IHS_EVENT;

#define	IHS_NEVENTS	((unsigned) IHSE_TO + 1)
#define	IHS_NSTATES	((unsigned) IHSS_W_TO + 1)

typedef void (*IHSFCT) (IpspHandshake_t *p);

static void psmp_timeout (uintptr_t user);

/* psmp_handshake_fail -- Handshake failure - abort authentication. */

static void psmp_handshake_fail (IpspHandshake_t *p, int may_retry)
{
	Participant_t	*pp;
	char		buf [32];

	pp = p->peer;
	pp->p_auth_state = AS_FAILED;
	log_printf (SPDP_ID, 0, "PSMP: Cleanup handshake && ignore participant %s!\r\n", 
		    guid_prefix_str (&pp->p_guid_prefix, buf));
	disc_ignore_participant (pp);
	if (may_retry)
		spdp_timeout_participant (pp, PSMP_WAIT_FAILED_TO / TMR_UNIT_MS);
}

#ifdef PSMP_TRACE_MSG

#define	PSMP_TRACE(d,m)	psmp_trace(d,m)

static void psmp_trace (char dir, DDS_ParticipantStatelessMessage *m)
{
	DDS_HandshakeToken	*hs;
	char			buf [32];

	log_printf (SPDP_ID, 0, "PSMP: %c [%s -> ",
		dir,
		guid_prefix_str ((GuidPrefix_t *) m->message_identity.source_guid, buf));
	log_printf (SPDP_ID, 0, "%s].%lld.%lld - %s",
		guid_prefix_str ((GuidPrefix_t *) m->destination_participant_key, buf),
		m->message_identity.sequence_number,
		m->related_message_identity.sequence_number,
		m->message_class_id);
	if (!strcmp (m->message_class_id, GMCLASSID_SECURITY_AUTH_HANDSHAKE)) {
		hs = DDS_SEQ_ITEM_PTR (m->message_data, 0);
		log_printf (SPDP_ID, 0, "/%s", hs->class_id);
	}
	log_printf (SPDP_ID, 0, "\r\n");
}

#else
#define	PSMP_TRACE(d,m)
#endif

static void psmp_handshake_send (IpspHandshake_t *p)
{
	Writer_t			*wp;
	DDS_Time_t			time;
	DDS_ParticipantStatelessMessage	msg;
	DDS_InstanceHandleSeq		handles;
	DDS_InstanceHandle_t		h;
	int				error;

	memset (&msg, 0, sizeof (msg));
	msg.message_class_id = GMCLASSID_SECURITY_AUTH_HANDSHAKE;
	DDS_SEQ_INIT_PTR (msg.message_data, *p->tx_hs_token);
	memcpy (msg.message_identity.source_guid,
		p->domain->participant.p_guid_prefix.prefix,
		GUIDPREFIX_SIZE);
	msg.message_identity.sequence_number = p->tx_seqnr;
	msg.related_message_identity.sequence_number = p->transaction;
	memcpy (msg.destination_participant_key,
		p->peer->p_guid_prefix.prefix,
		GUIDPREFIX_SIZE);
	PSMP_TRACE ('T', &msg);
	wp = (Writer_t *) p->domain->participant.p_builtin_ep [EPB_PARTICIPANT_SL_W];

	/* Send participant data. */
	DDS_SEQ_INIT (handles);
	h = p->peer->p_handle;
	DDS_SEQ_LENGTH (handles) = DDS_SEQ_MAXIMUM (handles) = 1;
	DDS_SEQ_DATA (handles) = &h;
	sys_gettime ((Time_t *) &time);
	error = DDS_DataWriter_write_w_timestamp_directed (wp, &msg, 0, &time, &handles);
	if (error)
		fatal_printf ("psmp_handshake_send: error sending handshake message!");
}

/* psmp_handshake_ok -- Handshake is done, continue remainder of
			authentication. */

static int psmp_handshake_ok (IpspHandshake_t *p, int immediate, int send_final)
{
	Permissions_t		perm;
	unsigned		caps;
	DDS_ReturnCode_t	error;
	char                    buf [32];

	if (send_final)
		psmp_handshake_send (p);

# if 0
	p->peer->p_shared_secret = sec_get_shared_secret (p->handle);
	if (!p->peer->p_shared_secret) {
		log_printf (SPDP_ID, 0, "PSMP: invalid shared secret!\r\n");
		psmp_handshake_fail (p);
		return (0);
	}
# endif
	caps = (p->domain->participant.p_sec_caps & 0xffff) |
	       (p->domain->participant.p_sec_caps >> 16);
	if (p->domain->access_protected) {
		p->p_creds = sec_get_peer_perm_cred (p->handle);
		if (!p->p_creds) {
			log_printf (SPDP_ID, 0, "PSMP: no permissions credentials!\r\n");
			psmp_handshake_fail (p, 1);
			return (0);
		}
		perm = sec_validate_remote_permissions (p->domain->participant.p_id,
							p->peer->p_id,
							caps,
							p->p_token->data,
							p->p_creds,
							&error);
		if (check_peer_participant (perm, p->peer->p_user_data)) {
			log_printf (SPDP_ID, 0, "PSMP: remote permissions not accepted!\r\n");
			psmp_handshake_fail (p, 1);
			return (0);
		}
	}
	else
		perm = 0;
	log_printf (SPDP_ID, 0, "PSMP: authorized participant! %s\r\n", guid_prefix_str (&p->peer->p_guid_prefix, buf));
	p->peer->p_permissions = perm;
	p->peer->p_auth_state = AS_OK;
	if (p->rehandshake)
		DDS_Security_topics_reevaluate (p->domain, p->peer->p_handle, "*");
	else
		spdp_remote_participant_enable (p->domain, p->peer, p->handle);
#ifdef DDS_QEO_TYPES
	policy_updater_participant_start_timer (p->domain, p->peer, ((PSMP_CLEANUP_TO + 5000) / TMR_UNIT_MS));
#endif
	if (immediate)
		psmp_handshake_free (p);
	else {
		psmp_state (p, IHSS_W_TO);
		tmr_start_lock (&p->timer,
				PSMP_CLEANUP_TO / TMR_UNIT_MS,
				(uintptr_t) p,
				psmp_timeout,
				&p->domain->lock);
	}
	return (1);
}

static void psmp_handshake_request (IpspHandshake_t *p)
{
	DDS_ReturnCode_t	error;

#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_request: %p\r\n", (void *) p);
#endif
	p->tx_seqnr = psmp_seqnr++;
	p->peer->p_auth_state = sec_begin_handshake_req (p->domain->participant.p_id,
							 p->peer->p_id,
							 &p->handle,
							 &p->tx_hs_token,
							 &error);
	switch (p->peer->p_auth_state) {
		case AS_OK:
			psmp_handshake_ok (p, 0, 0);
			break;
		case AS_PENDING_HANDSHAKE_MSG:
			p->transaction = p->tx_seqnr;
			psmp_handshake_send (p);
			psmp_state (p, IHSS_W_MSG);
			p->retries = MAX_REQ_RETRIES;
			p->backoff = 0;
			tmr_start_lock (&p->timer,
					PSMP_HS_REQ_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_OK_FINAL_MSG:
			psmp_handshake_ok (p, 0, 1);
			break;
		case AS_PENDING_RETRY:
			if (p->state != IHSS_R_REQ) {
				psmp_state (p, IHSS_R_REQ);
				p->retries = MAX_REQ_RETRIES;
				p->backoff = 0;
			}
			tmr_start_lock (&p->timer,
					PSMP_REQ_RETRY_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_FAILED:
		default:
			log_printf (SPDP_ID, 0, "PSMP: Begin handshake failure!\r\n");
			psmp_handshake_fail (p, 1);
			break;
	}
}

static void psmp_handshake_reply (IpspHandshake_t *p)
{
	DDS_ReturnCode_t	error;
	int                     retry = 1;

#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_reply: %p\r\n", (void *) p);
#endif
	p->tx_seqnr = psmp_seqnr++;
	p->peer->p_auth_state = sec_begin_handshake_reply (p->rx_hs_token,
							   p->peer->p_id,
							   p->domain->participant.p_id,
							   &p->handle,
							   &p->tx_hs_token,
							   &error);
	switch (p->peer->p_auth_state) {
		case AS_OK:
			psmp_handshake_ok (p, 0, 0);
			break;
		case AS_PENDING_HANDSHAKE_MSG:
			p->transaction = p->last_rel;
			psmp_state (p, IHSS_W_MSG);
			p->retries = MAX_HS_RETRIES;
			p->backoff = 0;
			psmp_handshake_send (p);
			tmr_start_lock (&p->timer,
					PSMP_WAIT_MSG_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_OK_FINAL_MSG:
			psmp_state (p, IHSS_W_TO);
			psmp_handshake_ok (p, 0, 1);
			break;
		case AS_PENDING_RETRY:
			if (p->state != IHSS_R_REPLY) {
				psmp_state (p, IHSS_R_REPLY);
				p->retries = MAX_REP_RETRIES;
				p->backoff = 0;
			}
			tmr_start_lock (&p->timer,
					PSMP_REPLY_RETRY_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_PENDING_HANDSHAKE_REQ:
			/* Ignore Request for whatever reason. */
			break;
		case AS_FAILED:
			if (error == DDS_RETCODE_NOT_ALLOWED_BY_SEC) {
				log_printf (DISC_ID, 0, 
					    "psmp_handshake_reply: Failed without a forced retry\r\n");
				retry = 0;
			}
		default:
			tmr_stop (&p->timer);
			log_printf (SPDP_ID, 0, "PSMP: Begin handshake reply failure!\r\n");
			psmp_handshake_fail (p, retry);
			break;
	}
}

static void psmp_handshake_process (IpspHandshake_t *p)
{
	DDS_ReturnCode_t	error;
	DDS_HandshakeToken	*tmp_token = NULL;
	int                     retry = 1;

#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_process: %p \r\n", (void *) p);
#endif
	p->peer->p_auth_state = sec_process_handshake (p->rx_hs_token,
						       p->handle,
						       &tmp_token,
						       &error);
	if (tmp_token) {
		if (p->tx_hs_token)
			DDS_DataHolder__free (p->tx_hs_token);

		p->tx_hs_token = tmp_token;
		p->tx_seqnr = psmp_seqnr++;
	}
	switch (p->peer->p_auth_state) {
		case AS_OK:
			tmr_stop (&p->timer);
#ifdef DDS_QEO_TYPES
			policy_updater_participant_start_timer (p->domain, p->peer, (5000 / TMR_UNIT_MS));
#endif
			psmp_handshake_ok (p, 1, 0);
			break;
		case AS_PENDING_HANDSHAKE_MSG:
			psmp_state (p, IHSS_W_MSG);
			p->retries = MAX_HS_RETRIES;
			p->backoff = 0;
			psmp_handshake_send (p);
			tmr_start_lock (&p->timer,
					PSMP_WAIT_MSG_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_OK_FINAL_MSG:
			psmp_state (p, IHSS_W_TO);
			psmp_handshake_ok (p, 0, 1);
			break;
		case AS_PENDING_RETRY:
			if (p->state != IHSS_R_REPLY) {
				psmp_state (p, IHSS_R_REPLY);
				p->retries = MAX_REP_RETRIES;
				p->backoff = 0;
			}
			tmr_start_lock (&p->timer,
					PSMP_REPLY_RETRY_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_FAILED:
			if (error == DDS_RETCODE_NOT_ALLOWED_BY_SEC) {
				log_printf (DISC_ID, 0, 
					    "psmp_handshake_process: Failed without a forced retry\r\n");
				retry = 0;
			}
		default:
			log_printf (SPDP_ID, 0, "PSMP: Process handshake failure!\r\n");
			psmp_handshake_fail (p, retry);
			break;
	}
}

#define	WHANDSHAKE_STR	"WaitHandshake"

static DDS_HandshakeToken *psmp_hs_init_token (void)
{
	DDS_HandshakeToken *token;

	token = DDS_DataHolder__alloc (WHANDSHAKE_STR);
	return (token);
}

static void rvri_to (IpspHandshake_t *p)
{
	Token_t			*id_token;
	Token_t			*perm_token;
	AuthState_t		state;
	unsigned		caps;
	DDS_ReturnCode_t	error;

	caps = (p->domain->participant.p_sec_caps & 0xffff) |
	       (p->domain->participant.p_sec_caps >> 16);
	state = sec_validate_remote_id (p->peer->p_id,
					p->domain->participant.p_guid_prefix.prefix,
					caps,
					p->peer->p_id_tokens,
					p->peer->p_p_tokens,
					&id_token,
					&perm_token,
				        p->peer->p_guid_prefix.prefix,
				        &p->peer->p_id,
				        &error);
	if (state != AS_PENDING_RETRY) {
		if (p->i_token) {
			log_printf (SEC_ID, 0, "rvri_to: unref %p\r\n", (void *) p->i_token);
			token_unref (p->i_token);
		}
		if (p->p_token)
			token_unref (p->p_token);
		p->i_token = token_ref (id_token);
		p->p_token = token_ref (perm_token);
	}
	else {
		log_printf (SEC_ID, 0, "rvri_to: unref 2 %p\r\n", (void *) id_token);
		token_unref (id_token);
		token_unref (perm_token);
	}
	p->peer->p_auth_state = state;
	switch (state) {
		case AS_OK:
			psmp_handshake_ok (p, 1, 0);
			break;
		case AS_PENDING_RETRY:
			if (!--p->retries) {
				log_printf (SPDP_ID, 0, "PSMP: Too many retries!\r\n");
				psmp_handshake_fail (p, 1);
			}
			else
				tmr_start_lock (&p->timer,
						PSMP_V_RETRY_TO / TMR_UNIT_MS,
						(uintptr_t) p,
						psmp_timeout,
						&p->domain->lock);
			break;
		case AS_PENDING_HANDSHAKE_REQ:
			psmp_handshake_request (p);
			break;
		case AS_PENDING_CHALLENGE_MSG:
			psmp_state (p, IHSS_W_REQ);
			p->tx_hs_token = psmp_hs_init_token ();
			p->retries = MAX_WHS_RETRIES;
			p->backoff = 0;
			p->tx_seqnr = psmp_seqnr++;
			psmp_handshake_send (p);
			tmr_start_lock (&p->timer,
					PSMP_WAIT_REQ_TO / TMR_UNIT_MS,
					(uintptr_t) p,
					psmp_timeout,
					&p->domain->lock);
			break;
		case AS_FAILED:
		default:
			/* Just ignore participant. */
			log_printf (SPDP_ID, 0, "PSMP: Remote Identity validation failure!\r\n");
			psmp_handshake_fail (p, 1);
			break;
	}
}

static void rreq_to (IpspHandshake_t *p)
{
	if (!--p->retries) {
		log_printf (SPDP_ID, 0, "PSMP: Too many retries failure!\r\n");
		psmp_handshake_fail (p, 1);
	}
	else
		psmp_handshake_request (p);
}

static void wreq_token (IpspHandshake_t *p)
{
	psmp_handshake_reply (p);
}

static void wreq_to (IpspHandshake_t *p)
{
	unsigned	n;

	if (!--p->retries) {
		log_printf (SPDP_ID, 0, "PSMP: Remote Request time-out!\r\n");
		psmp_handshake_fail (p, 1);
	}
	else {
		psmp_handshake_send (p);
		if (p->backoff < PSMP_MAX_BACKOFF)
			p->backoff++;
		n = 1 << (fastrandn (p->backoff + 1));
		tmr_start_lock (&p->timer,
				(PSMP_WAIT_REQ_TO / TMR_UNIT_MS) * n,
				(uintptr_t) p,
				psmp_timeout,
				&p->domain->lock);
	}
}

static void rrep_to (IpspHandshake_t *p)
{
	if (!--p->retries) {
		log_printf (SPDP_ID, 0, "PSMP: Too many retries failure!\r\n");
		psmp_handshake_fail (p, 1);
	}
	else
		psmp_handshake_reply (p);
}

static void wmsg_token (IpspHandshake_t *p)
{
	if (p->last_rel != p->transaction) {
		if (p->initiator || p->last_rel < p->transaction) {
#ifdef PSMP_TRACE_HS
			log_printf (SPDP_ID, 0, "wmsg_token: incorrect transaction.\r\n");
#endif
			return;
		}

		/* Restart handshake again! */
		psmp_state (p, IHSS_W_REQ);
		psmp_handshake_reply (p);
	}
	else
		psmp_handshake_process (p);
}

static void wto_token (IpspHandshake_t *p)
{
	psmp_handshake_send (p);
	tmr_start_lock (&p->timer,
			PSMP_CLEANUP_TO / TMR_UNIT_MS,
			(uintptr_t) p,
			psmp_timeout,
			&p->domain->lock);
#ifdef DDS_QEO_TYPES
	policy_updater_participant_start_timer (p->domain, p->peer, ((PSMP_CLEANUP_TO + 5000) / TMR_UNIT_MS));
#endif
}

static void wmsg_to (IpspHandshake_t *p)
{
	unsigned	n;

	if (!--p->retries) {
		log_printf (SPDP_ID, 0, "PSMP: Too many retries failure!\r\n");
		psmp_handshake_fail (p, 1);
	}
	else {
		if (!p->initiator)
			p->tx_seqnr = psmp_seqnr++;

		psmp_handshake_send (p);
		if (p->backoff < PSMP_MAX_BACKOFF)
			p->backoff++;
		n = 1 << (fastrandn (p->backoff + 1));
		tmr_start_lock (&p->timer,
				(PSMP_WAIT_MSG_TO / TMR_UNIT_MS) * n,
				(uintptr_t) p,
				psmp_timeout,
				&p->domain->lock);
	}
}

static void rhs_to (IpspHandshake_t *p)
{
	if (!--p->retries) {
		log_printf (SPDP_ID, 0, "PSMP: Too many retries failure!\r\n");
		psmp_handshake_fail (p, 1);
	}
	else
		psmp_handshake_process (p);
}

static void wto_to (IpspHandshake_t *p)
{
	psmp_handshake_free (p);
}

static IHSFCT ihsp_fsm [IHS_NSTATES][IHS_NEVENTS] = {
		  /*TOKEN*/	/*TO*/
/*R_VRI  */	{ NULL,		rvri_to },
/*R_REQ  */	{ NULL,		rreq_to },
/*W_REQ  */	{ wreq_token,	wreq_to },
/*R_REPLY*/	{ NULL,		rrep_to },
/*W_MSG  */	{ wmsg_token,	wmsg_to },
/*R_HS   */	{ NULL,		rhs_to  },
/*W_TO   */	{ wto_token,	wto_to  }
};

/* psmp_timeout -- Various timeout handlers. */

static void psmp_timeout (uintptr_t user)
{
	IpspHandshake_t	*p = (IpspHandshake_t *) user;
	IHSFCT		fct;


	if (!p)
		return;

	if (p->state != IHSS_W_TO)
		log_printf (SPDP_ID, 0, "PSMP: handshake time-out - retry!\r\n");

	fct = ihsp_fsm [p->state][IHSE_TO];
	if (fct)
		(*fct) (p);
}

/* psmp_retry_validate -- Retry Identity validation in a short while. */

void psmp_retry_validate (Domain_t      *dp,
			  Participant_t *pp,
			  int           rehandshake)
{
	IpspHandshake_t	*p;

	p = psmp_handshake_get (dp, pp, 0, 1);
	if (!p) {
		pp->p_auth_state = AS_FAILED;
		return;
	}
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_retry_validate: %p\r\n", (void *) p);
#endif
	psmp_state (p, IHSS_R_VRI);
	p->retries = MAX_VRI_RETRIES;
	p->backoff = 0;
	p->rehandshake = rehandshake;
	tmr_start_lock (&p->timer,
			PSMP_V_RETRY_TO / TMR_UNIT_MS,
			(uintptr_t) p,
			psmp_timeout,
			&dp->lock);
}

/* psmp_handshake_initiate -- Send a Handshake Request message token. */

void psmp_handshake_initiate (Domain_t      *dp,
			      Participant_t *pp,
			      Token_t       *id_token,
			      Token_t       *p_token,
			      int           rehandshake)
{
	IpspHandshake_t	*p;
	Writer_t	*wp;

	log_printf (SPDP_ID, 0, "PSMP: initiate handshake!\r\n");
	p = psmp_handshake_get (dp, pp, 1, 1);
	if (!p) {
		pp->p_auth_state = AS_FAILED;
		return;
	}
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_initiate: %p\r\n", (void *) p);
#endif
	/* Resend participant info. */
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	rtps_stateless_resend (wp);

	/* Send handshake. */
	psmp_state_i (p, IHSS_R_REQ);
	p->i_token = token_ref (id_token);
	p->p_token = token_ref (p_token);
	p->retries = MAX_REQ_RETRIES;
	p->backoff = 0;
	p->rehandshake = rehandshake;
	p->initiator = 1;
	psmp_handshake_request (p);
}

/* psmp_handshake_wait -- Wait for a Handshake Request message token. */

void psmp_handshake_wait (Domain_t      *dp,
			  Participant_t *pp,
			  Token_t       *id_token,
			  Token_t       *perm_token,
			  int           rehandshake)
{
	IpspHandshake_t	*p;
	Writer_t	*wp;

	log_printf (SPDP_ID, 0, "PSMP: wait for handshake!\r\n");

	/* Resend participant info. */
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_W];
	rtps_stateless_resend (wp);

	p = psmp_handshake_get (dp, pp, 1, 1);
	if (!p) {
		pp->p_auth_state = AS_FAILED;
		return;
	}
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_wait: %p\r\n", (void *) p);
#endif
	p->i_token = token_ref (id_token);
	p->p_token = token_ref (perm_token);
	p->rehandshake = rehandshake;
	p->initiator = 0;
	psmp_state_i (p, IHSS_W_REQ);
	if (!rehandshake) {
		p->tx_hs_token = psmp_hs_init_token ();
		p->retries = MAX_WHS_RETRIES;
		p->backoff = 0;
		p->tx_seqnr = psmp_seqnr++;
		psmp_handshake_send (p);
		tmr_start_lock (&p->timer,
			PSMP_WAIT_REQ_TO / TMR_UNIT_MS,
			(uintptr_t) p,
			psmp_timeout,
			&dp->lock);
	}
}

/* psmp_delete -- Delete a handshake context corresponding to a participant. */

void psmp_delete (Domain_t *dp, Participant_t *pp)
{
	IpspHandshake_t	*p;

	p = psmp_handshake_get (dp, pp, 0, 0);
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_delete: %p\r\n", (void *) p);
#endif
	if (p)
		psmp_handshake_free (p);
}

/* psmp_handshake_token -- A new handshake message token was received. */

static void psmp_handshake_token (Domain_t                        *dp,
				  DDS_ParticipantStatelessMessage *info)
{
	IpspHandshake_t	*p;
	Participant_t	*pp;
	IHSFCT		fct;
	char		buf [32];

	/*log_printf (SEC_ID, 0, "PSMP: handshake token\r\n");*/

	/* Check if message elements are correct. */
	if (memcmp (info->destination_endpoint_key, psmp_unknown_key, 16) ||
	    memcmp (info->source_endpoint_key, psmp_unknown_key, 16) ||
	    DDS_SEQ_LENGTH (info->message_data) != 1)
		return;

	/* Lookup the participant. */
	pp = participant_lookup (dp,
			  (GuidPrefix_t *) info->message_identity.source_guid);
	if (!pp) {
		log_printf (SEC_ID, 0, "psmp_handshake_token: participant lookup failure (%s)\r\n",
			    guid_prefix_str ((GuidPrefix_t *) info->message_identity.source_guid, buf));
		return;
	}


	/* Lookup the handshake context. */
	p = psmp_handshake_get (dp, pp, 0, 0);
	if (!strcmp (info->message_class_id, WHANDSHAKE_STR)) {
		if (pp->p_auth_state == AS_OK || pp->p_auth_state == AS_OK_FINAL_MSG) {
			log_printf (SEC_ID, 0, "psmp_handshake_token: restarting handshake (%s)\r\n",
			    guid_prefix_str ((GuidPrefix_t *) info->message_identity.source_guid, buf));

			psmp_handshake_initiate (dp, pp, pp->p_id_tokens, pp->p_p_tokens, 1);
		}
		return;
	}
	if (!p) {
#ifdef PSMP_TRACE_HS
		log_printf (SEC_ID, 0, "psmp_handshake_token: handshake get failure (%s)\r\n",
				    guid_prefix_str (&pp->p_guid_prefix, buf));
#endif
		return;
	}
#ifdef PSMP_TRACE_HS
	log_printf (SEC_ID, 0, "psmp_handshake_token: %p\r\n", (void *) p);
#endif

	/* Validate sequence number. */
	if (info->message_identity.sequence_number <= p->last_seqnr) {
#ifdef PSMP_TRACE_HS
		log_printf (SEC_ID, 0, "psmp_handshake_token: %p - duplicate message.\r\n", (void *) p);
#endif
		return;
	}

	/* Remember sequence numbers. */
	p->last_seqnr = info->message_identity.sequence_number;
	p->last_rel = info->related_message_identity.sequence_number;

	/* Process the Handshake Message Token. */
	fct = ihsp_fsm [p->state][IHSE_TOKEN];
	if (fct) {
		p->rx_hs_token = DDS_SEQ_ITEM_PTR (info->message_data, 0);

		/* Initialize non-marshalled token fields. */
		(*fct) (p);
	}
}

/* psmp_event -- New Participant stateless message data available callback
		 function. Locked on entry/exit: DP + R(rp). */

void psmp_event (Reader_t *rp, NotificationType_t t)
{
	Domain_t			*dp = rp->r_subscriber->domain;
	ChangeData_t			change;
	DDS_ParticipantStatelessMessage *info = NULL;
	int				error;

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

		PSMP_TRACE ('R', info);

		/* Handle different message classes specifically: */
		if (!info->message_class_id)
			;
		else if (!strcmp (info->message_class_id,
				  GMCLASSID_SECURITY_AUTH_HANDSHAKE))
			psmp_handshake_token (dp, info);

		/* Free message info. */

	    free_data:
		xfree (info);
		info = NULL;
	}
	if (info)
		xfree (info);
}

/* psmp_start -- Start the Participant Stateless message protocol. */

int psmp_start (Domain_t *dp)
{
#if defined (DDS_TRACE) && defined (PSMP_RTPS_TRACE)
	Writer_t		*wp;
#endif
	Reader_t		*rp;
	DDS_ReturnCode_t	error;

	if (sedp_log)
		log_printf (SEDP_ID, 0, "PSMP: starting Participant Stateless message protocol for domain #%u.\r\n", dp->domain_id);

	/* Create builtin participant Stateless Writer. */
	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_SL_W,
					 1, 0,
					 0, 0, 0,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 dp->dst_locs);
	if (error)
		return (error);

#if defined (DDS_TRACE) && defined (PSMP_RTPS_TRACE)
	wp = (Writer_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_W];
	rtps_trace_set (&wp->w_ep, DDS_TRACE_ALL);
#endif

	/* Create builtin Participant Stateless Reader. */
	error = create_builtin_endpoint (dp, EPB_PARTICIPANT_SL_R,
					 0, 0,
					 0, 0, 0,
					 NULL,
					 dp->participant.p_meta_ucast,
					 dp->participant.p_meta_mcast,
					 NULL);
	if (error)
		return (error);

	/* Enable read events for Participant Stateless Reader. */
	rp = (Reader_t *) dp->participant.p_builtin_ep [EPB_PARTICIPANT_SL_R];
#if defined (DDS_TRACE) && defined (PSMP_RTPS_TRACE)
	rtps_trace_set (&rp->r_ep, DDS_TRACE_ALL);
#endif
	error = hc_request_notification (rp->r_cache, disc_data_available, (uintptr_t) rp);
	if (error) {
		fatal_printf ("SPDP: can't register Participant Stateless Reader events!");
		return (error);
	}
	return (DDS_RETCODE_OK);
}

/* psmp_disable -- Disable the Participant Stateless reader/writer. */

void psmp_disable (Domain_t *dp)
{
	if (sedp_log)
		log_printf (SEDP_ID, 0, "PSMP: disabling Participant Stateless message protocol for domain #%u.\r\n", dp->domain_id);

	/* Disable Participant Stateless builtin endpoints. */
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_SL_W);
	disable_builtin_endpoint (dp, EPB_PARTICIPANT_SL_R);

	psmp_handshake_free_all (dp, NULL);
}

/* psmp_stop -- Stop the Participant Stateless reader/writer.
	        On entry/exit: domain and global lock taken. */

void psmp_stop (Domain_t *dp)
{
	if (sedp_log)
		log_printf (SEDP_ID, 0, "PSMP: disabling Participant Stateless message protocol for domain #%u.\r\n", dp->domain_id);

	/* Delete Participant Stateless builtin endpoints. */
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_SL_W);
	delete_builtin_endpoint (dp, EPB_PARTICIPANT_SL_R);
}

#endif
#endif
#endif

