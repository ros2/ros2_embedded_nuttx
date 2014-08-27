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

/* sec_crypto.c -- Implements the function that are defined for the crypto plugin. */

#include "log.h"
#include "error.h"
#include "handle.h"
#include "sec_cdata.h"
#include "sec_c_std.h"
#include "sec_crypto.h"

/*#define CRYPT_FTRACE	** Trace crypto function calls. */

#define	MAX_CRYPTO_PLUGINS 4

static unsigned		ncrypto_plugins;
static const SEC_CRYPTO	*crypto_plugins [MAX_CRYPTO_PLUGINS];

/* sec_crypto_init -- Initialize crypto data. */

int sec_crypto_init (unsigned min, unsigned max)
{
	int	ret;

	ret = crypto_data_init (min, max);
	if (ret)
		return (ret);

	sec_crypto_add_std ();
	return (DDS_RETCODE_OK);
}

/* sec_crypto_add -- Add a new crypto control plugin. */

int sec_crypto_add (const SEC_CRYPTO *cp)
{
	if (ncrypto_plugins >= MAX_CRYPTO_PLUGINS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	crypto_plugins [ncrypto_plugins] = cp;
	ncrypto_plugins++;
	return (DDS_RETCODE_OK);
}


/**************************/
/*   Crypto Key Factory.  */
/**************************/

#ifdef CRYPT_FTRACE
#define crypt_fct(s)	log_printf (SEC_ID, 0, s)
#else
#define	crypt_fct(s)
#endif

/* sec_register_local_participant -- Register a local domain Participant for
				     crypto operations. */

ParticipantCrypto_t sec_register_local_participant (Domain_t         *domain,
						    DDS_ReturnCode_t *error)
{
	const SEC_CRYPTO 	*cp;
	Token_t		 	*tp;
	ParticipantCrypto_t	h;
	unsigned	 	i;

	crypt_fct ("register_local_participant()\r\n");
	for (tp = domain->participant.p_id_tokens; tp; tp = tp->next)
		for (i = 0; i < ncrypto_plugins; i++) {
			cp = crypto_plugins [i];
			h = cp->reg_loc_part (cp, tp->data->class_id, domain, error);
			if (h)
				return (h);
		}

	return (0);
}

/* sec_register_remote_participant -- Register a discovered Participant for
				      crypto operations. */

ParticipantCrypto_t sec_register_remote_participant (ParticipantCrypto_t local,
						     Participant_t       *remote,
						     const void          *plugin,
						     SharedSecret_t      secret,
						     DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("register_remote_participant()\r\n");
	if ((dp = crypto_lookup (local)) == NULL) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		return (0);
	}
	return (dp->plugin->reg_rem_part (dp->plugin, dp, remote, 
					  (const SEC_AUTH *) plugin, secret, error));
}

/* sec_register_local_datawriter -- Register a local DataWriter for crypto
				    operations. */

DataWriterCrypto_t sec_register_local_datawriter (ParticipantCrypto_t local,
						  Writer_t            *w,
						  DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("register_local_datawriter()\r\n");
	if ((dp = crypto_lookup (local)) == NULL) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		return (0);
	}
	return (dp->plugin->reg_loc_writer (dp->plugin, dp, w, error));
}

/* sec_register_remote_datareader -- Register a discovered DataReader for crypto
				     operations. */

DataReaderCrypto_t sec_register_remote_datareader (DataWriterCrypto_t  local,
						   ParticipantCrypto_t remote,
						   DiscoveredReader_t  *dr,
						   int                 relay_only,
						   DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("register_remote_datareader()\r\n");
	if ((dp = crypto_lookup (local)) == NULL) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		return (0);
	}
	return (dp->plugin->reg_rem_reader (dp->plugin, dp, crypto_lookup (remote),
							dr, relay_only, error));
}

/* sec_register_local_datareader -- Register a local DataReader for crypto
				    operations. */

DataReaderCrypto_t sec_register_local_datareader (ParticipantCrypto_t local,
						  Reader_t            *r,
						  DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("register_local_datareader()\r\n");
	if ((dp = crypto_lookup (local)) == NULL) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		return (0);
	}
	return (dp->plugin->reg_loc_reader (dp->plugin, dp, r, error));
}

/* sec_register_remote_datawriter -- Register a discovered DataWriter for crypto
				     operations. */

DataWriterCrypto_t sec_register_remote_datawriter (DataReaderCrypto_t  local, 
						   ParticipantCrypto_t remote,
						   DiscoveredWriter_t  *dw,
						   DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("register_remote_datawriter()\r\n");
	if ((dp = crypto_lookup (local)) == NULL) {
		if (error)
			*error = DDS_RETCODE_ALREADY_DELETED;
		return (0);
	}
	return (dp->plugin->reg_rem_writer (dp->plugin, dp,
					crypto_lookup (remote), dw, error));
}
						  
/* sec_unregister_participant -- Unregister a Participant for crypto operations.*/

DDS_ReturnCode_t sec_unregister_participant (ParticipantCrypto_t handle)
{
	CryptoData_t	*dp;

	crypt_fct ("unregister_participant()\r\n");
	if ((dp = crypto_lookup (handle)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->unreg_part (dp->plugin, dp));
}

/* sec_unregister_datawriter -- Unregister a DataWriter for crypto operations. */

DDS_ReturnCode_t sec_unregister_datawriter (DataWriterCrypto_t handle)
{
	CryptoData_t	*dp;

	crypt_fct ("unregister_datawriter()\r\n");
	if ((dp = crypto_lookup (handle)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->unreg_writer (dp->plugin, dp));
}

/* sec_unregister_datareader -- Unregister a DataReader for crypto operations. */

DDS_ReturnCode_t sec_unregister_datareader (DataReaderCrypto_t handle)
{
	CryptoData_t	*dp;

	crypt_fct ("unregister_datareader()\r\n");
	if ((dp = crypto_lookup (handle)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->unreg_reader (dp->plugin, dp));
}


/**************************************/
/*   Crypto Key Exchange Interface.   */
/**************************************/


/* sec_create_local_participant_tokens -- Create crypto tokens for the local
					  domain Participant. */

DDS_ReturnCode_t sec_create_local_participant_tokens (ParticipantCrypto_t local, 
						      ParticipantCrypto_t remote, 
						      DDS_CryptoTokenSeq  *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("create_local_participant_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->cr_lp_tokens (dp->plugin, crypto_lookup (local),
								dp, tokens));
}

/* sec_set_remote_participant_tokens -- Set the crypto tokens for a discovered
					Participant. */

DDS_ReturnCode_t sec_set_remote_participant_tokens (ParticipantCrypto_t local,
						    ParticipantCrypto_t remote, 
						    DDS_CryptoTokenSeq  *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("set_remote_participant_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->set_rp_tokens (dp->plugin, crypto_lookup (local),
								dp, tokens));
}

/* sec_create_local_datawriter_tokens -- Create crypto tokens for a local
					 DataWriter. */

DDS_ReturnCode_t sec_create_local_datawriter_tokens (DataWriterCrypto_t local,
						     DataReaderCrypto_t remote,
						     DDS_CryptoTokenSeq *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("create_local_datawriter_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->cr_w_tokens (dp->plugin, crypto_lookup (local),
								dp, tokens));
}

/* sec_set_remote_datawriter_tokens -- Set the crypto tokens for a discovered
				       DataWriter. */

DDS_ReturnCode_t sec_set_remote_datawriter_tokens (DataReaderCrypto_t local,
						   DataWriterCrypto_t remote,
						   DDS_CryptoTokenSeq *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("set_remote_datawriter_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->set_dw_tokens (dp->plugin, crypto_lookup (local),
								dp, tokens));
}

/* sec_create_local_datareader_tokens -- Create crypto tokens for a local
					 DataReader. */

DDS_ReturnCode_t sec_create_local_datareader_tokens (DataReaderCrypto_t local,
						     DataWriterCrypto_t remote,
						     DDS_CryptoTokenSeq *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("create_local_datareader_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->cr_r_tokens (dp->plugin, crypto_lookup (local),
								dp, tokens));
}

/* sec_set_remote_datareader_tokens-- Set crypto tokens for a discovered
				      DataReader. */

DDS_ReturnCode_t sec_set_remote_datareader_tokens (DataWriterCrypto_t local,
						   DataReaderCrypto_t remote,
						   DDS_CryptoTokenSeq *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("set_remote_datareader_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->set_dr_tokens (dp->plugin, crypto_lookup (local),
								dp, tokens));
}

/* sec_remember_remote_datawriter_tokens -- Remember crypto tokens for a not yet
					    discovered DataWriter. */

DDS_ReturnCode_t sec_remember_remote_datawriter_tokens (DataReaderCrypto_t  local,
						        ParticipantCrypto_t remote,
							EntityId_t          *eid,
							DDS_CryptoTokenSeq *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("remember_remote_datawriter_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->rem_dw_tokens (dp->plugin, crypto_lookup (local), dp,
								   eid, tokens));
}

/* sec_remember_remote_datareader_tokens -- Remember crypto tokens for a not yet
					    discovered DataWriter. */

DDS_ReturnCode_t sec_remember_remote_datareader_tokens (DataWriterCrypto_t  local,
						        ParticipantCrypto_t remote,
							EntityId_t          *eid,
							DDS_CryptoTokenSeq *tokens)
{
	CryptoData_t	*dp;

	crypt_fct ("remember_remote_datareader_tokens()\r\n");
	if ((dp = crypto_lookup (remote)) == NULL)
		return (DDS_RETCODE_ALREADY_DELETED);

	return (dp->plugin->rem_dr_tokens (dp->plugin, crypto_lookup (local), dp,
								   eid, tokens));
}

/* sec_release_tokens -- Release previously created crypto tokens. */

void sec_release_tokens (DDS_CryptoTokenSeq *tokens)
{
	DDS_DataHolderSeq__clear (tokens);
}


/***********************************/
/*   Crypto Transform Interface.   */
/***********************************/

/* 1. Encoding operations.
   ----------------------- */

/* sec_encode_serialized_data -- Encode the serialized data submessage element.*/

DB *sec_encode_serialized_data (DBW                *data,
				DataWriterCrypto_t sender,
				size_t             *enc_len,
				DDS_ReturnCode_t   *error)
{
	CryptoData_t	*dp;

	/*crypt_fct ("encode_serialized_data()\r\n");*/
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	return (dp->plugin->enc_payload (dp->plugin, data, dp, enc_len, error));
}

#define	MAX_CRYPTO_NRX	32

/* sec_encode_datawriter_submsg -- Encode a DataWriter submessage. */

RME *sec_encode_datawriter_submsg (RME                *submsg,
				   DataWriterCrypto_t sender,
				   DataReaderCrypto_t receivers [],
				   unsigned           nreceivers,
				   DDS_ReturnCode_t   *error)
{
	CryptoData_t	*dp, *rx_dp [MAX_CRYPTO_NRX], **rxp;
	RME		*nsubmsg;
	unsigned	i;

	crypt_fct ("encode_datawriter_submsg()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	if (nreceivers <= MAX_CRYPTO_NRX) {
		rxp = xmalloc (nreceivers * sizeof (CryptoData_t *));
		if (!rxp) {
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
	}
	else
		rxp = rx_dp;
	for (i = 0; i < nreceivers; i++) {
		rxp [i] = crypto_lookup (receivers [i]);
		if (!rxp [i]) {
			if (nreceivers > MAX_CRYPTO_NRX)
				xfree (rxp);
			*error = DDS_RETCODE_ALREADY_DELETED;
			return (NULL);
		}
	}
	nsubmsg = dp->plugin->enc_dw_submsg (dp->plugin, submsg, dp, rxp,
							nreceivers, error);
	if (nreceivers > MAX_CRYPTO_NRX)
		xfree (rxp);
	return (nsubmsg);
}

/* sec_encode_datareader_submsg -- Encode a DataReader submessage. */

RME *sec_encode_datareader_submsg (RME                *submsg,
				   DataReaderCrypto_t sender,
				   DataWriterCrypto_t receiver,
				   DDS_ReturnCode_t   *error)
{
	CryptoData_t	*dp;

	crypt_fct ("encode_datareader_submsg()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	return (dp->plugin->enc_dr_submsg (dp->plugin, submsg, dp, 
					crypto_lookup (receiver), error));
}

/* sec_encode_rtps_message -- Encode a complete RTPS message. */

RMBUF *sec_encode_rtps_message (RMBUF               *rtps_message,
				ParticipantCrypto_t sender,
				ParticipantCrypto_t receivers [],
				unsigned            nreceivers,
				DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp, *rx_dp [MAX_CRYPTO_NRX], **rxp;
	RMBUF		*nmsg;
	unsigned	i;

	crypt_fct ("encode_rtps_message()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	if (nreceivers <= MAX_CRYPTO_NRX) {
		rxp = xmalloc (nreceivers * sizeof (CryptoData_t *));
		if (!rxp) {
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
	}
	else
		rxp = rx_dp;
	for (i = 0; i < nreceivers; i++) {
		rxp [i] = crypto_lookup (receivers [i]);
		if (!rxp [i]) {
			if (nreceivers > MAX_CRYPTO_NRX)
				xfree (rxp);
			*error = DDS_RETCODE_ALREADY_DELETED;
			return (NULL);
		}
	}
	nmsg = dp->plugin->enc_message (dp->plugin, rtps_message, dp, rxp,
							nreceivers, error);
	if (nreceivers > MAX_CRYPTO_NRX)
		xfree (rxp);
	return (nmsg);
}


/* 2. Decoding operations.
   ----------------------- */

/* sec_decode_serialized_data -- Decode a serialized data submessage element. */

DB *sec_decode_serialized_data (DBW                *encoded, 
				DataReaderCrypto_t receiver,
				DataWriterCrypto_t sender,
				size_t             *dec_len,
				size_t             *ofs,
				DDS_ReturnCode_t   *error)
{
	CryptoData_t	*dp;

	/*crypt_fct ("decode_serialized_data()\r\n");*/
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	return (dp->plugin->dec_payload (dp->plugin, encoded,
				crypto_lookup (receiver), dp, dec_len, ofs, error));
}

/* sec_preprocess_secure_submsg -- Preprocess a secure submessage to determine
				   whether it is an encoded DataReader or
				   DataWriter submessage.  Depending on the
				   result, both *dw_crypto or *dr_crypto will
				   be set. */

SubmsgCategory_t sec_preprocess_secure_submsg (RME                 *submsg,
					       ParticipantCrypto_t receiver,
					       ParticipantCrypto_t sender,
					       DataWriterCrypto_t  *dwcrypto,
					       DataReaderCrypto_t  *drcrypto,
					       DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("preprocess_secure_submsg()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (-1);
	}
	return (dp->plugin->preproc_submsg (dp->plugin, submsg,
						crypto_lookup (receiver), dp,
						dwcrypto, drcrypto, error));
}

/* sec_decode_datawriter_submsg -- Decode a DataWriter submessage. */

RME *sec_decode_datawriter_submsg (RME                *submsg,
				   DataReaderCrypto_t receiver,
				   DataWriterCrypto_t sender,
				   DDS_ReturnCode_t   *error)
{
	CryptoData_t	*dp;

	crypt_fct ("decode_datawriter_submsg()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	return (dp->plugin->dec_dw_submsg (dp->plugin, submsg,
					crypto_lookup (receiver), dp, error));
}

/* sec_decode_datareader_submsg -- Decode a DataReader submessage. */

RME *sec_decode_datareader_submsg (RME                *submsg,
				   DataWriterCrypto_t receiver,
				   DataReaderCrypto_t sender,
				   DDS_ReturnCode_t   *error)
{
	CryptoData_t	*dp;

	crypt_fct ("decode_datareader_submsg()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	return (dp->plugin->dec_dr_submsg (dp->plugin, submsg,
					crypto_lookup (receiver), dp, error));
}

/* sec_decode_rtps_message -- Decode a complete RTPS message from a secure
			      submessage element. */

RMBUF *sec_decode_rtps_message (RME                 *submsg,
				ParticipantCrypto_t receiver,
				ParticipantCrypto_t sender,
				DDS_ReturnCode_t    *error)
{
	CryptoData_t	*dp;

	crypt_fct ("decode_rtps_message()\r\n");
	if ((dp = crypto_lookup (sender)) == NULL) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		return (NULL);
	}
	return (dp->plugin->dec_message (dp->plugin, submsg,
					crypto_lookup (receiver), dp, error));
}

#ifdef DDS_DEBUG

/* sec_crypto_dump -- Dump a crypto context based on an entity handle. */

void sec_crypto_dump (unsigned handle)
{
	Entity_t	*p = entity_ptr (handle);
	Participant_t	*pp;
	LocalEndpoint_t	*ep;
	CryptoData_t	*dp;
	unsigned	ch;

	if (!p) {
		dbg_printf ("Entity not found!\r\n");
		return;
	}
	switch (entity_type (p)) {
		case ET_PARTICIPANT:
			pp = (Participant_t *) p;
			ch = pp->p_crypto;
			break;
		case ET_WRITER:
		case ET_READER:
			if ((p->flags & EF_LOCAL) != 0) {
				ep = (LocalEndpoint_t *) p;
				ch = ep->crypto;
			}
			else if (entity_type (p) == ET_WRITER)
				ch = rtps_peer_writer_crypto_get (NULL, 
							(DiscoveredWriter_t *) p);
			else
				ch = rtps_peer_reader_crypto_get (NULL, 
							(DiscoveredReader_t *) p);
			break;
		default:
			dbg_printf ("Invalid entity type!\r\n");
			return;
	}
	if (!ch) {
		dbg_printf ("No associated crypto context.\r\n");
		return;
	}
	dp = crypto_lookup (ch);
	if (!dp) {
		dbg_printf ("Crypto context deleted.\r\n");
		return;
	}
	dp->plugin->dump (dp->plugin, dp, p);
}

#endif

