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

/* disc_psmp.h -- Interface to the Discovery Participant Stateless Message
		  protocol. */

#ifndef __disc_psmp_h_
#define __disc_psmp_h_

#include "dds_data.h"

extern int64_t psmp_seqnr;
extern unsigned char psmp_unknown_key [16];

int psmp_start (Domain_t *dp);

/* Start the Participant Stateless message protocol. */

void psmp_disable (Domain_t *dp);

/* Disable the Participant Stateless reader/writer. */

void psmp_stop (Domain_t *dp);

/* Stop the Participant Stateless reader/writer.
   On entry/exit: domain and global lock taken. */

void psmp_retry_validate (Domain_t      *dp,
			  Participant_t *pp,
			  int           restart);

/* Retry Identity validation in a short while. */

void psmp_handshake_initiate (Domain_t      *dp,
			      Participant_t *pp,
			      Token_t       *id_token,
			      Token_t       *p_token,
			      int           restart);

/* Send a Handshake Request message token. */

void psmp_handshake_wait (Domain_t      *dp,
			  Participant_t *pp,
			  Token_t       *id_token,
			  Token_t       *perm_token,
			  int           restart);

/* Wait for a Handshake Request message token. */

void psmp_delete (Domain_t *dp, Participant_t *pp);

/* Delete a handshake context corresponding to the peer participant. */

void psmp_event (Reader_t *rp, NotificationType_t t);

/* New Participant stateless message data available callback function.
   Locked on entry/exit: DP + R(rp). */

#endif /* !__disc_psmp_h_ */

