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

/* disc_policy_updater.h -- Interface to the Participant message procedures. */

#ifndef __disc_policy_updater_h_
#define __disc_policy_updater_h_

#include "dds_data.h"

#ifdef DDS_QEO_TYPES

/* Participant Message data format: */
typedef struct policy_updater_msg_data_st {
	GuidPrefix_t	participantGuidPrefix;
	uint64_t        version;
} PolicyUpdaterMessageData;

int policy_updater_init (void);

/* Initialize the message type. */

void policy_updater_final (void);

/* Finalize the message type. */

int policy_updater_start (Domain_t *dp);

/* Start the Policy updater message reader/writer. On entry/exit: no locks used. */

void policy_updater_disable (Domain_t *dp);

/* Disable the Policy updater message reader/writer.
   On entry/exit: domain and global lock taken. */

void policy_updater_stop (Domain_t *dp);

/* Stop the Policy updater message reader/writer.
   On entry/exit: domain and global lock taken. */

void policy_updater_connect (Domain_t *dp, Participant_t *rpp);

/* Connect the messaging endpoints to a peer participant. */

void policy_updater_disconnect (Domain_t *dp, Participant_t *rpp);

/* Disconnect the messaging endpoints from a peer participant. */

int policy_updater_write_policy_version (Domain_t *dp, uint64_t version);

/* Send a policy update via the message writer. */

void policy_updater_data_event (Reader_t *rp, NotificationType_t t, int secure);

/* Receive a policy Message from a remote participant. */
#endif 
#endif /* !__disc_policy_updater_h_ */
