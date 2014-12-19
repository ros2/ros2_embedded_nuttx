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

#include "error.h"
#include "log.h"
#include "dds/dds_plugin.h"
#include "timer.h"
#include "sec_data.h"

typedef struct policy_ctx_st Policy_Ctx;

struct policy_ctx_st {
	unsigned char *policy_file;
	size_t        length;
	Timer_t       tmr;
};

static void remove_policy_ctx_node (Policy_Ctx *node)
{
	if (node) {
		xfree (node->policy_file);
		tmr_stop (&node->tmr);
		xfree (node);
	}
}

static void policy_ctx_to (uintptr_t user)
{
	Policy_Ctx *node = (Policy_Ctx *) user;
	DDS_ReturnCode_t ret;
	DDS_SecurityReqData data;

	if (node) {
		data.handle = 0;
		data.data = node->policy_file;
		data.length = node->length;
		ret = sec_access_control_request (DDS_SET_PERM_CRED, &data);
		if (ret)
			log_printf (SEC_ID, 0, "Something went wrong while writing the policy file\r\n");
		remove_policy_ctx_node (node);
	}
}

void set_policy_file (unsigned char *policy_file,
		      size_t        length)
{
	Policy_Ctx *node;

	if (!(node = xmalloc (sizeof (Policy_Ctx))))
		return;
	else {
		node->policy_file = policy_file;
		node->length = length;
		tmr_init (&node->tmr, "policy file tmr");
		tmr_start (&node->tmr, TICKS_PER_SEC, (uintptr_t) node, policy_ctx_to);
	}
	return ;
}

unsigned char *get_policy_file (size_t *length,
				DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData data;
	unsigned char       *dp;
	size_t              perm_len;

	data.handle = 0;
	data.data = NULL;
	data.kdata = NULL;
	data.length = 0;
	data.rlength = 0;
	*error = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (*error || !data.rlength)
		return (NULL);

	perm_len = data.rlength;
	dp = xmalloc (perm_len);
	if (!dp)		return (NULL);

	data.data = dp;
	data.kdata = NULL;
	data.length = perm_len;
	data.rlength = perm_len;
	*error = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (*error) {
		xfree (dp);
		return (NULL);
	}
	*length = perm_len;
	return (dp);
}

uint64_t get_policy_version (DDS_ReturnCode_t *error)
{
	DDS_SecurityReqData data;
	uint64_t            version;

	data.handle = 0;
	data.data = NULL;
	data.kdata = (void *) &version;
	data.length = 0;
	*error = sec_access_control_request (DDS_GET_PERM_CRED, &data);
	if (*error)
		return (0);

	return (version);
}
