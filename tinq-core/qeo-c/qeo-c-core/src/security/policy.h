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

#ifndef POLICY_H_
#define POLICY_H_


/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#include <qeo/error.h>
#include <qeo/types.h>
#include <inttypes.h>

#include "security.h"
#include "topic_participant_list_node.h"

/*########################################################################
#                                                                       #
#  TYPES SECTION                                                        #
#                                                                       #
########################################################################*/
typedef struct qeo_security_policy *qeo_security_policy_hndl;

typedef void (*qeo_security_policy_update_cb)(qeo_security_policy_hndl qeoSecPol);
typedef void (*qeo_security_policy_update_fine_grained_rules_cb)(qeo_security_policy_hndl qeoSecPol,uintptr_t cookie ,const char *topic_name, unsigned int selector, struct topic_participant_list_node *read_participant_list, struct topic_participant_list_node *write_participant_list);

typedef struct {
    qeo_security_hndl  sec;
    qeo_factory_t *factory;
    qeo_security_policy_update_cb update_cb;

} qeo_security_policy_config;
/*########################################################################
#                                                                       #
#  API FUNCTION SECTION                                                 #
#                                                                       #
########################################################################*/
/* Init qeo_security_policy module */
qeo_retcode_t qeo_security_policy_init(void);

/* De-init qeo_security_policy module */
qeo_retcode_t qeo_security_policy_destroy(void);

/* Construct new policy object */
qeo_retcode_t qeo_security_policy_construct(const qeo_security_policy_config *cfg, qeo_security_policy_hndl *qeoSecPol);

/* Retrieve policy configuration */
qeo_retcode_t qeo_security_policy_get_config(qeo_security_policy_hndl qeoSecPol, qeo_security_policy_config *cfg);

/* Get sequence number of currently enforced policy file */
qeo_retcode_t qeo_security_policy_get_sequence_number(qeo_security_policy_hndl qeoSecPol, uint64_t *sequence_number);

/* Destruct policy object */
qeo_retcode_t qeo_security_policy_destruct(qeo_security_policy_hndl *qeoSecPol);

/* Start policy redistribution */
qeo_retcode_t qeo_security_policy_start_redistribution(qeo_security_policy_hndl qeoSecPol);

/* Force refresh (get new policy from backend if present) */
qeo_retcode_t qeo_security_policy_refresh(qeo_security_policy_hndl qeoSecPol);

/* Get partition strings */
qeo_retcode_t qeo_security_policy_get_fine_grained_rules(qeo_security_policy_hndl qeoSecPol, uintptr_t cookie, const char *topic_name, unsigned int selector_mask, qeo_security_policy_update_fine_grained_rules_cb update_cb);

#endif /* POLICY_H_ */
