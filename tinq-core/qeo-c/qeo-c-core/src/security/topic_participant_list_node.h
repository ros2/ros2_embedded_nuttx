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

#ifndef TOPIC_PARTICIPANT_LIST_NODE_H_
#define TOPIC_PARTICIPANT_LIST_NODE_H_

/*#######################################################################
# HEADER (INCLUDE) SECTION                                               #
########################################################################*/
#include <inttypes.h>
#include <qeo/error.h>
#include <qeocore/api.h>
#include "policy_identity.h"
#include "utlist.h"
#include <nsecplug/nsecplug.h>
#include <openssl/ssl.h>


/*#######################################################################
# TYPES SECTION                                                         #
########################################################################*/
#define TOPIC_PARTICIPANT_SELECTOR_READ 0x01
#define TOPIC_PARTICIPANT_SELECTOR_WRITE 0x02
#define TOPIC_PARTICIPANT_SELECTOR_ALL (TOPIC_PARTICIPANT_SELECTOR_READ | TOPIC_PARTICIPANT_SELECTOR_WRITE)

struct topic_participant_list_node {
    qeo_policy_identity_t id;
    const char *participant_name;
    struct topic_participant_list_node *next;
};

/*########################################################################
#                       API FUNCTION SECTION                             #
########################################################################*/

#endif
