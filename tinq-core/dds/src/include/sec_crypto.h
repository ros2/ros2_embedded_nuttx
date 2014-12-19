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

/* sec_crypto.h -- DDS Security - Crypto plugin definitions. */

#ifndef __sec_crypto_h_
#define __sec_crypto_h_

#include "sec_auth.h"
#include "sec_access.h"
#include "rtps_data.h"

typedef unsigned Crypto_t;

typedef Crypto_t ParticipantCrypto_t;
typedef Crypto_t DataWriterCrypto_t;
typedef Crypto_t DataReaderCrypto_t;

int sec_crypto_init (unsigned min_handles, unsigned max_handles);

/* Initialize crypto functionality. */


/**************************/
/*   Crypto Key Factory.  */
/**************************/

ParticipantCrypto_t sec_register_local_participant (Domain_t         *domain,
						    DDS_ReturnCode_t *error);

/* Register a local domain Participant for crypto operations. */

ParticipantCrypto_t sec_register_remote_participant (ParticipantCrypto_t local,
						     Participant_t       *remote,
						     const void          *plugin,
						     SharedSecret_t      secret,
						     DDS_ReturnCode_t    *error);

/* Register a discovered Participant for crypto operations. */

DataWriterCrypto_t sec_register_local_datawriter (ParticipantCrypto_t local,
						  Writer_t            *w,
						  DDS_ReturnCode_t    *error);

/* Register a local DataWriter for crypto operations. */

DataReaderCrypto_t sec_register_remote_datareader (DataWriterCrypto_t  local,
						   ParticipantCrypto_t remote,
						   DiscoveredReader_t  *dr,
						   int                 relay_only,
						   DDS_ReturnCode_t    *error);

/* Register a discovered DataReader for crypto operations. */

DataReaderCrypto_t sec_register_local_datareader (ParticipantCrypto_t id,
						  Reader_t            *r,
						  DDS_ReturnCode_t    *error);

/* Register a local DataReader for crypto operations. */

DataWriterCrypto_t sec_register_remote_datawriter (DataReaderCrypto_t  local, 
						   ParticipantCrypto_t remote,
						   DiscoveredWriter_t  *dw,
						   DDS_ReturnCode_t    *error);
						  
/* Register a discovered DataWriter for crypto operations. */

DDS_ReturnCode_t sec_unregister_participant (ParticipantCrypto_t handle);

/* Unregister a Participant for crypto operations. */

DDS_ReturnCode_t sec_unregister_datawriter (DataWriterCrypto_t handle);

/* Unregister a DataWriter for crypto operations. */

DDS_ReturnCode_t sec_unregister_datareader (DataReaderCrypto_t handle);

/* Unregister a DataReader for crypto operations. */


/**************************************/
/*   Crypto Key Exchange Interface.   */
/**************************************/

DDS_ReturnCode_t sec_create_local_participant_tokens (ParticipantCrypto_t local, 
						      ParticipantCrypto_t remote, 
						      DDS_CryptoTokenSeq  *tokens);

/* Create crypto tokens for the local domain Participant. */

DDS_ReturnCode_t sec_set_remote_participant_tokens (ParticipantCrypto_t local,
						    ParticipantCrypto_t remote, 
						    DDS_CryptoTokenSeq  *tokens);

/* Set the crypto tokens for a discovered Participant. */

DDS_ReturnCode_t sec_create_local_datawriter_tokens (DataWriterCrypto_t local,
						     DataReaderCrypto_t remote,
						     DDS_CryptoTokenSeq *tokens);

/* Create crypto tokens for a local DataWriter. */

DDS_ReturnCode_t sec_set_remote_datawriter_tokens (DataReaderCrypto_t local,
						   DataWriterCrypto_t remote,
						   DDS_CryptoTokenSeq *tokens);

/* Set the crypto tokens for a discovered DataWriter. */

DDS_ReturnCode_t sec_create_local_datareader_tokens (DataReaderCrypto_t local,
						     DataWriterCrypto_t remote,
						     DDS_CryptoTokenSeq *tokens);

/* Create crypto tokens for a local DataReader. */

DDS_ReturnCode_t sec_set_remote_datareader_tokens (DataWriterCrypto_t local,
						   DataReaderCrypto_t remote,
						   DDS_CryptoTokenSeq *tokens);

/* Set crypto tokens for a discovered DataReader. */

DDS_ReturnCode_t sec_remember_remote_datawriter_tokens (DataReaderCrypto_t  local,
						        ParticipantCrypto_t remote,
							EntityId_t          *eid,
							DDS_CryptoTokenSeq *tokens);

/* Remember crypto tokens for a not yet discovered DataWriter. */

DDS_ReturnCode_t sec_remember_remote_datareader_tokens (DataWriterCrypto_t  local,
						        ParticipantCrypto_t remote,
							EntityId_t          *eid,
							DDS_CryptoTokenSeq *tokens);

/* Remember crypto tokens for a not yet discovered DataWriter. */

void sec_release_tokens (DDS_CryptoTokenSeq *tokens);

/* Release previously created crypto tokens. */


/***********************************/
/*   Crypto Transform Interface.   */
/***********************************/

/* 1. Encoding operations.
   ----------------------- */

DB *sec_encode_serialized_data (DBW                *data,
				DataWriterCrypto_t sender,
				size_t             *enc_len,
				DDS_ReturnCode_t   *error);

/* Encode the serialized data submessage element. */

RME *sec_encode_datawriter_submsg (RME                *submsg,
				   DataWriterCrypto_t sender,
				   DataReaderCrypto_t receivers [],
				   unsigned           nreceivers,
				   DDS_ReturnCode_t   *error);

/* Encode a DataWriter submessage. */

RME *sec_encode_datareader_submsg (RME                *submsg,
				   DataReaderCrypto_t sender,
				   DataWriterCrypto_t receiver,
				   DDS_ReturnCode_t   *error);

/* Encode a DataReader submessage. */

RMBUF *sec_encode_rtps_message (RMBUF               *rtps_message,
				ParticipantCrypto_t sender,
				ParticipantCrypto_t receivers [],
				unsigned            nreceivers,
				DDS_ReturnCode_t    *error);

/* Encode a complete RTPS message. */


/* 2. Decoding operations.
   ----------------------- */

typedef enum {
	DATAWRITER_SUBMSG,
	DATAREADER_SUBMSG,
	INFO_SUBMSG
} SubmsgCategory_t;

DB *sec_decode_serialized_data (DBW                *encoded, 
				DataReaderCrypto_t receiver,
				DataWriterCrypto_t sender,
				size_t             *dec_len,
				size_t             *db_ofs,
				DDS_ReturnCode_t   *error);

/* Decode a serialized data submessage element. */

SubmsgCategory_t sec_preprocess_secure_submsg (RME                 *submsg,
					       ParticipantCrypto_t receiver,
					       ParticipantCrypto_t sender,
					       DataWriterCrypto_t  *dwcrypto,
					       DataReaderCrypto_t  *drcrypto,
					       DDS_ReturnCode_t    *error);

/* Preprocess a secure submessage to determine whether it is an encoded
   DataReader or DataWriter submessage.  Depending on the result, either
   *dwcrypto or *drcrypto will be set and either sec_decode_datawriter_submsg()
   or sec_decode_datareader_submsg() should be called. */

RME *sec_decode_datawriter_submsg (RME                *submsg,
				   DataReaderCrypto_t receiver,
				   DataWriterCrypto_t sender,
				   DDS_ReturnCode_t   *error);

/* Decode a DataWriter submessage. */

RME *sec_decode_datareader_submsg (RME                *submsg,
				   DataWriterCrypto_t receiver,
				   DataReaderCrypto_t sender,
				   DDS_ReturnCode_t   *error);

/* Decode a DataReader submessage. */

RMBUF *sec_decode_rtps_message (RME                 *submsg,
				ParticipantCrypto_t receiver,
				ParticipantCrypto_t sender,
				DDS_ReturnCode_t    *error);

/* Decode a complete RTPS message from a secure submessage element. */

void sec_crypto_dump (unsigned eh);

/* Dump a crypto context based on an entity handle. */

#endif /* !__sec_crypto_h_ */

