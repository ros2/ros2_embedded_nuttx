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

/* disc_qeo.h -- Qeo security discovery specific methods. */

#ifndef __disc_qeo_h_
#define __disc_qeo_h_

#ifdef DDS_QEO_TYPES

#include "sec_data.h"

void DDS_Security_set_policy_version (uint64_t version);

typedef void (*VOL_DATA_CB) (DDS_DataHolder *dh);

/* Write dataholder data on a secure volatile topic */
void DDS_Security_write_volatile_data (Domain_t       *dp,
				       DDS_DataHolder *dh);

/* Register a callback to receive data on a secure volatile topic */
void DDS_Security_register_volatile_data (VOL_DATA_CB fct);

typedef void (*POLICY_VERSION_CB) (GuidPrefix_t guid_prefix, uint64_t version, int type);

/* Write policy version number data on state topic */
int DDS_Security_write_policy_version (Domain_t *dp,
				       uint64_t version);

/* Register a callback to receive data on a policy version state topic */
void DDS_Security_register_policy_version (POLICY_VERSION_CB fct);

/* Install the Qeo authentication/encryption plugin code. */

void policy_updater_participant_start_timer (Domain_t *dp, Participant_t *p, unsigned timer);

/* Start an "inconsistent handshake" safety net, 
   this will start a timer that will wait for data on the policy updater.
   if the timer ran out and no policy version from the peer was received,
   we are in an "incosistent handshake" and have to restart the handshake with the peer */

#endif

#endif
