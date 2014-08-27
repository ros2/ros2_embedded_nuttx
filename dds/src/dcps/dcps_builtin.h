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

/* dcps_builtin.h -- Defines the builtin data structure access functions. */

#ifndef __dcps_builtin_h_
#define __dcps_builtin_h_

Endpoint_t *dcps_new_builtin_reader (Domain_t *dp, const char *topic_name);

void dcps_delete_builtin_readers (DDS_DomainParticipant dp);

int dcps_get_builtin_participant_data (DDS_ParticipantBuiltinTopicData *dp,
				       Participant_t                   *pp);

int dcps_get_builtin_topic_data (DDS_TopicBuiltinTopicData *dp,
				 Topic_t                   *tp,
				 int                       bi_reader);

int dcps_get_builtin_publication_data (DDS_PublicationBuiltinTopicData *dp,
				       DiscoveredWriter_t              *dwp);

int dcps_get_local_publication_data (DDS_PublicationBuiltinTopicData *dp,
				     Writer_t                        *wp);

int dcps_get_builtin_subscription_data (DDS_SubscriptionBuiltinTopicData *dp,
					DiscoveredReader_t               *drp);

int dcps_get_local_subscription_data (DDS_SubscriptionBuiltinTopicData *dp,
				      Reader_t                         *rp);

void *dcps_read_builtin_data (Reader_t *rp, Change_t *cp);

void dcps_free_builtin_data (Reader_t *rp, void *dp);

#endif /* !__dcps_builtin_h_ */

