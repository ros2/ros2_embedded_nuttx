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

/* disc_msg.h -- Interface to the Participant message procedures. */

#ifndef __disc_msg_h_
#define __disc_msg_h_

#include "dds_data.h"

/* Participant Message data format: */
typedef struct participant_msg_data_st {
	GuidPrefix_t	participantGuidPrefix;
	unsigned char	kind [4];
	DDS_OctetSeq	data;
} ParticipantMessageData;

int msg_init (void);

/* Initialize the message type. */

void msg_final (void);

/* Finalize the message type. */

int msg_start (Domain_t *dp);

/* Start the Participant message reader/writer. On entry/exit: no locks used. */

void msg_disable (Domain_t *dp);

/* Disable the Participant message reader/writer.
   On entry/exit: domain and global lock taken. */

void msg_stop (Domain_t *dp);

/* Stop the Participant message reader/writer.
   On entry/exit: domain and global lock taken. */

void msg_connect (Domain_t *dp, Participant_t *rpp);

/* Connect the messaging endpoints to a peer participant. */

void msg_disconnect (Domain_t *dp, Participant_t *rpp);

/* Disconnect the messaging endpoints from a peer participant. */

int msg_send_liveliness (Domain_t *dp, unsigned kind);

/* Send a liveliness update via the message writer. */

void msg_data_event (Reader_t *rp, NotificationType_t t, int secure);

/* Receive a Participant Message from a remote participant. */

#endif /* !__disc_msg_h_ */

