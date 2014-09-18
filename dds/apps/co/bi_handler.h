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

/* bi_handler.c -- Builtin Discovery data readers utility functions. */

#ifndef __bi_handler_c_
#define __bi_handler_c_

#include "dds/dds_dcps.h"

/* Masks for logging: */
#define	BI_PARTICIPANT_M	1
#define	BI_TOPIC_M		2
#define	BI_PUBLICATION_M	4
#define	BI_SUBSCRIPTION_M	8

#define	BI_ALL_M		0xf

/* Listener types: */
typedef void (* DDS_Participant_notify)  (DDS_BuiltinTopicKey_t            *key,
					  DDS_ParticipantBuiltinTopicData  *data,
					  DDS_SampleInfo                   *info,
					  uintptr_t                        user);
typedef void (* DDS_Topic_notify)        (DDS_TopicBuiltinTopicData        *data,
					  DDS_SampleInfo                   *info,
					  uintptr_t                        user);
typedef void (* DDS_Publication_notify)  (DDS_BuiltinTopicKey_t            *key,
					  DDS_PublicationBuiltinTopicData  *data,
					  DDS_SampleInfo                   *info,
					  uintptr_t                        user);
typedef void (* DDS_Subscription_notify) (DDS_BuiltinTopicKey_t            *key,
					  DDS_SubscriptionBuiltinTopicData *data,
					  DDS_SampleInfo                   *info,
					  uintptr_t                        user);

DDS_ReturnCode_t bi_attach (DDS_DomainParticipant   p,
			    unsigned                m,
			    DDS_Participant_notify  pnf,
			    DDS_Topic_notify        tnf,
			    DDS_Publication_notify  wnf,
			    DDS_Subscription_notify rnf,
			    uintptr_t               user);

/* Attach the builtin topic readers (m) of the given participant (p) such that
   all the activated readers will immediately start calling the given functions
   (if non-NULL) with the user parameter (user).  */

void bi_detach (DDS_DomainParticipant p);

/* Detach the application from the builtin topic readers. */

void bi_log (FILE *f, unsigned mask);

/* Log received discovery information to the given file. */

#endif /* !__bi_handler_c_ */

