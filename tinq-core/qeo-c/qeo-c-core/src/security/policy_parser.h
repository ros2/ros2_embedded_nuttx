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

                 
#ifndef POLICYPARSER_H_
#define POLICYPARSER_H_
/*#######################################################################
#                       HEADER (INCLUDE) SECTION                        #
########################################################################*/                                                                           
#include <qeo/error.h>
#include <stdbool.h>
#include <stdint.h>

 
 
/*#######################################################################
#                           TYPES SECTION                               #
########################################################################*/  
typedef struct policy_parser_s *policy_parser_hndl_t;

typedef struct {
    bool write;
    bool read;

} policy_parser_permission_t;

typedef void (*policy_parser_participant_found_cb)(policy_parser_hndl_t parser, uintptr_t *parser_cookie, const char *participant_id);
typedef void (*policy_parser_coarse_grained_rule_found_cb)(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const policy_parser_permission_t *perm);
typedef void (*policy_parser_sequence_number_found_cb)(policy_parser_hndl_t parser, uint64_t sequence_number);
typedef void (*policy_parser_fine_grained_rule_section_found_cb)(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const char *participant_id, const policy_parser_permission_t *perm);
                                                    

typedef struct {
    /* callbacks */
    policy_parser_participant_found_cb on_participant_found_cb;
    policy_parser_coarse_grained_rule_found_cb on_coarse_grained_rule_found_cb;
    policy_parser_sequence_number_found_cb on_sequence_number_found_cb;
    policy_parser_fine_grained_rule_section_found_cb on_fine_grained_rule_section_found_cb;

} policy_parser_init_cfg_t;
 
typedef struct {
    const char *buf; /* contains actual policy info, api does not copy this, assumes memory stays valid */
    uintptr_t  user_data;

} policy_parser_cfg_t;
 
/*#######################################################################
#                   PUBLIC FUNCTION DECLARATION                         #
########################################################################*/
/* Init policy parser */
qeo_retcode_t policy_parser_init(const policy_parser_init_cfg_t *cfg);

/* Destroy policy parser */
void policy_parser_destroy(void);
 
/* Construct policy parser object */
qeo_retcode_t policy_parser_construct(const policy_parser_cfg_t *cfg, policy_parser_hndl_t *parser);

/* Destruct policy parser object */
qeo_retcode_t policy_parser_destruct(policy_parser_hndl_t *parser);


uint64_t policy_parser_get_sequence_number(char *content);


/* Parse the policy file in the buffer provided at construction time. This buffer can be modified during parsing ! */
qeo_retcode_t policy_parser_run(policy_parser_hndl_t parser);

/* Retrieve user data */
qeo_retcode_t policy_parser_get_user_data(policy_parser_hndl_t parser, uintptr_t *user_data);
#endif
