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

#ifndef POLICY_CACHE_H_
#define POLICY_CACHE_H_

/*#######################################################################
# HEADER (INCLUDE) SECTION                                               #
########################################################################*/
#include <inttypes.h>
#include <qeo/error.h>
#include  "policy_parser.h" /* for permission stuff */
#include "utlist.h"
#include "topic_participant_list_node.h"

/*#######################################################################
#                       TYPE SECTION                                    #
########################################################################*/
typedef struct qeo_policy_cache *qeo_policy_cache_hndl_t;

/* this callback is guaranteed to be called PER selector. */
typedef void (*qeo_policy_cache_update_topic_cb)(qeo_policy_cache_hndl_t cache, uintptr_t cookie,  const char *participant_tag, const char *topic_name, unsigned int selector, struct topic_participant_list_node * read_participant_list, struct topic_participant_list_node * write_participant_list);

/* this callback is guaranteed to be called PER participant. */
typedef void (*qeo_policy_cache_participant_cb) (qeo_policy_cache_hndl_t cache,  const char *participant);


/*########################################################################
#                       API FUNCTION SECTION                             #
########################################################################*/

/* Constructs policy cache object. This object will store the information found during top-down parsing. */
qeo_retcode_t qeo_policy_cache_construct(uintptr_t cookie, qeo_policy_cache_hndl_t *cache);

/* Retrieve cookie provided at construction time */
qeo_retcode_t qeo_policy_cache_get_cookie(qeo_policy_cache_hndl_t cache, uintptr_t *cookie);

/* Throw away all stored data from parsing - but do not destruct the cache object */
qeo_retcode_t qeo_policy_cache_reset(qeo_policy_cache_hndl_t cache);

/* Destructs the policy cache object */
qeo_retcode_t qeo_policy_cache_destruct(qeo_policy_cache_hndl_t *cache);

/* Store sequence number found during parsing */
qeo_retcode_t qeo_policy_cache_set_seq_number(qeo_policy_cache_hndl_t cache, uint64_t sequence_number);

/* Store 'participant tag' (those things found between []) */
qeo_retcode_t qeo_policy_cache_add_participant_tag(qeo_policy_cache_hndl_t cache, const char *participant_tag);

/* Retrieve number of participants */
qeo_retcode_t qeo_policy_cache_get_number_of_participants(qeo_policy_cache_hndl_t cache, unsigned int *number_of_participants);

/* Retrieve number of topics */
qeo_retcode_t qeo_policy_cache_get_number_of_topics(qeo_policy_cache_hndl_t cache, unsigned int *number_of_topics);

/* Add coarse-grained rule for particular participant and topic */
qeo_retcode_t qeo_policy_cache_add_coarse_grained_rule(qeo_policy_cache_hndl_t cache, const char *participant_tag, const char *topic_name, const policy_parser_permission_t *perms);

/* Add fine-grained rule for particular participant (between[]), topic, participant_specifier (between < >) */
qeo_retcode_t qeo_policy_cache_add_fine_grained_rule_section(qeo_policy_cache_hndl_t cache, const char *participant_tag, const char *topic_name, const policy_parser_permission_t *perms, const char *participant_specifier);

/* Call this function when parsing has ended */
qeo_retcode_t qeo_policy_cache_finalize(qeo_policy_cache_hndl_t cache);

/* Iterate over participants */
qeo_retcode_t qeo_policy_cache_get_participants(qeo_policy_cache_hndl_t cache, qeo_policy_cache_participant_cb participant_cb);

/* Iterate over topic-participant (can only be done after qeo_policy_cache_finalize() */
/* participant_id == NULL --> iterate over all participant_id */
/* topic_name == NULL --> iterate over all topics */
/* topic_name must be NULL OR fully-qualified, org::blabla::* is NOT ALLLOWED */
qeo_retcode_t qeo_policy_cache_get_topic_rules(qeo_policy_cache_hndl_t cache, uintptr_t cookie, const char *participant_id, const char *topic_name, unsigned int selector_mask, qeo_policy_cache_update_topic_cb update_cb);


#endif /* POLICY_CACHE_H_ */
