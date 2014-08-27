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

/* sec_p_qeo.c -- Support for Qeo DDS security.
		  Permissions handling plugin implementation. */

#include "error.h"
#include "log.h"
#include "timer.h"
#include "list.h"
#include "random.h"
#include "sec_access.h"
#include "sec_plugin.h"
#include "sec_perm.h"
#include "sec_util.h"
#include "sec_p_qeo.h"
#include "dds/dds_security.h"
#include "disc_qeo.h"
#include "sec_qeo_policy.h"

#ifdef DDS_QEO_TYPES

static lock_t pol_lock;

typedef struct policy_st Policy_t;
struct policy_st {
	Policy_t     *next;
	Policy_t     *prev;
	GuidPrefix_t guid_prefix;
	uint64_t     version;
	int          update_cnt;
};

typedef struct {
	Policy_t *head;
	Policy_t *tail;
} PolicyList_t;

PolicyList_t policy_list;
Timer_t      tmr_policy;
static int   list_init = 0;

void qeo_receive_policy_version (GuidPrefix_t guid_prefix,
				 uint64_t     version,
				 int          type);

static void init_policy_list (void)
{
	if (!list_init) {
		tmr_init (&tmr_policy, "Policy Timer");
		LIST_INIT (policy_list);
		DDS_Security_register_policy_version (qeo_receive_policy_version);
	}
	list_init = 1;
}

static Policy_t *get_policy_node (GuidPrefix_t guid_prefix)
{
	Policy_t *node;

	LIST_FOREACH (policy_list, node)
		if (!memcmp (&node->guid_prefix, &guid_prefix, sizeof (GuidPrefix_t)))
			return (node);
	return (NULL);
}

static Policy_t *add_policy_node (GuidPrefix_t guid_prefix,
				  uint64_t     version)
{
	Policy_t *node;
	
	if (!(node = get_policy_node (guid_prefix))) {
		if (!(node = malloc (sizeof (Policy_t)))) {
			return (NULL);
		} else {
			memcpy (&node->guid_prefix, &guid_prefix, sizeof (GuidPrefix_t));
			node->version = version;
			node->update_cnt = 1;
			LIST_ADD_HEAD (policy_list, *node);
		}
	}
	return (node);
}

static void remove_policy_node (Policy_t *node)
{
	if (node) {
		LIST_REMOVE (policy_list, *node);
		free (node);
	}
}

void dump_policy_version_list (void)
{
	Policy_t *node;
	int      i = 0;
	char     buf [32];

	dbg_printf ("Policy version list\r\n");
	LIST_FOREACH (policy_list, node) {
		dbg_printf ("%d. %s [%llu] [update_cnt: %d]\r\n", i, 
			    guid_prefix_str (&node->guid_prefix, buf), 
			    (unsigned long long) node->version, node->update_cnt);
		i++;
	}
}

static DDS_OctetSeq *char_to_seq (unsigned char *data,
				  size_t        length)
{
	DDS_OctetSeq *seq;
	DDS_ReturnCode_t ret;

	seq = DDS_OctetSeq__alloc ();

	ret = dds_seq_require (seq, length);
	if (ret) {
		DDS_OctetSeq__free (seq);
		return (NULL);
	}

	memcpy (DDS_SEQ_DATA (*seq), data, length);
	return (seq);
}

static void qeo_write_policy_file (void);

static void qeo_policy_update_to (uintptr_t user)
{
	ARG_NOT_USED (user)

	qeo_write_policy_file ();
}

/* Write a policy file, but only when one other state requires this */
static void qeo_write_policy_file (void)
{
	unsigned char    *policy_file = NULL;
	size_t           len = 0;
	uint64_t         version;
	unsigned int     i = 0;
	DDS_ReturnCode_t error;
	Domain_t         *dp;
	DDS_DataHolder   *msg = DDS_DataHolder__alloc (QEO_CLASSID_SECURITY_POLICY);
	Policy_t         *node;

	init_policy_list ();

	version = get_policy_version (&error);

	LIST_FOREACH (policy_list, node) {
		if (node && node->version != version)
			goto write;
	}
	log_printf (SEC_ID, 0, "SEC_P_QEO: All qeo policy files were the same, so not written\r\n");
	return;

 write:
	policy_file = get_policy_file (&len, &error);
	msg->binary_value1 = char_to_seq (policy_file, len);

	while ((dp = domain_next (&i, &error))) {
		if (dp->security) {
			DDS_Security_write_volatile_data (dp, msg);
			log_printf (SEC_ID, 0, "SEC_P_QEO: Written qeo policy file\r\n");
		}
	}

	/* Wait some time before sending your policy file again */
	tmr_start (&tmr_policy, (fastrandn (TICKS_PER_SEC * 3) + TICKS_PER_SEC), 0, qeo_policy_update_to);
}

/* receive a policy file from the volatile secure reader */
void qeo_receive_policy_file (DDS_DataHolder *dh)
{
	DDS_OctetSeq  *seq;
	size_t        len;
	unsigned char *policy_file;

	log_printf (SEC_ID, 0, "SEC_P_QEO: qeo_receive_policy_file\r\n");

	seq = dh->binary_value1;

	len = DDS_SEQ_LENGTH (*seq);

	policy_file = xmalloc (len);
	if (!policy_file)
		return;

	/* write policy file to qeo after a timer */
	set_policy_file (policy_file, len);
}

void DDS_Security_qeo_write_policy_version (void)
{
	uint64_t version;
	DDS_ReturnCode_t error;
	Domain_t *dp;
	unsigned i = 0;

	if (!domains_used ())
		return;

	init_policy_list ();

	log_printf (SEC_ID, 0, "SEC_P_QEO: qeo_write_policy_version\r\n");

	version = get_policy_version (&error);
	while ((dp = domain_next (&i, &error))) {
		if (dp->security) {
			DDS_Security_write_policy_version (dp, version);
			log_printf (SEC_ID, 0, "SEC_P_QEO: updated qeo policy version state to [%llu]\r\n", (unsigned long long) version);
		}
	}

	/* Wait some time before effectively sending your updated policy file */
	tmr_start (&tmr_policy, fastrandn (TICKS_PER_SEC * 2), 0, qeo_policy_update_to);
}

void qeo_receive_policy_version (GuidPrefix_t guid_prefix,
				 uint64_t     version,
				 int          type)
{
	char buf [32];
	Policy_t *node;

     	log_printf (SEC_ID, 0, "SEC_P_QEO: received a new policy state [%llu] [type:%d] from %s \r\n", (unsigned long long) version, type, guid_prefix_str ((GuidPrefix_t *) &guid_prefix, buf));

	if (type == 2) /* DELETE */ {
		lock_take  (pol_lock);
		remove_policy_node (get_policy_node (guid_prefix));
		lock_release (pol_lock);
	}
	else if (type == 1) /* UPDATE */ {
		lock_take (pol_lock);
		node = add_policy_node (guid_prefix, version);
		node->update_cnt ++;
		node->version = version;
		lock_release (pol_lock);
	}
	else if (type == 0) /* NEW */ {
		lock_take (pol_lock);
		add_policy_node (guid_prefix, version);
		lock_release (pol_lock);
	}
}

#endif

/* sp_valid_local_perms -- Check a local permissions handle to verify that
			   a token can be produced for it. */

static DDS_ReturnCode_t sp_valid_local_perms (const SEC_PERM *pp,
					      Identity_t     id,
					      Permissions_t  perms)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	return ((perms) ? DDS_RETCODE_OK : DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* sp_check_rem_perms -- Get a permissions handle for a given token. */

static Permissions_t sp_check_rem_perms (const SEC_PERM            *pp,
					 Identity_t                local,
					 Identity_t                remote,
					 DDS_PermissionsToken      *token,
					 DDS_PermissionsCredential *cred)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	ARG_NOT_USED (pp)
	ARG_NOT_USED (token)

	data.handle = remote;
	data.secure = local;
	data.data = DDS_SEQ_DATA (*cred->binary_value1);
	data.length = DDS_SEQ_LENGTH (*cred->binary_value1);
	data.name = cred->class_id;
	ret = sec_access_control_request (DDS_VALIDATE_REMOTE_PERM, &data);
	if (ret)
		return (0);

	return (data.handle);
}

/* sp_get_perm_token -- Get a PermissionsToken from a permissions handle. */

static Token_t *sp_get_perm_token (const SEC_PERM *pp, Permissions_t id)
{
	Token_t			*tp;
	unsigned char		*p;
	DDS_PermissionsToken	*token;
	size_t			cred_len;
	DDS_ReturnCode_t	error;
	uint64_t                policy_version;

	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	tp = xmalloc (sizeof (Token_t));
	if (!tp)
		return (NULL);

	token = DDS_DataHolder__alloc (QEO_CLASSID_SECURITY_PERMISSIONS_TOKEN);
	if (!token) {
		xfree (tp);
		return (NULL);
	}
	/* policy version number */
	token->longlongs_value = DDS_LongLongSeq__alloc ();
	if (!token->longlongs_value)
		goto free_dh;

	/* Sha256 */
	token->binary_value1 = DDS_OctetSeq__alloc ();
	if (!token->binary_value1)
		goto free_dh;

	error = dds_seq_require (token->binary_value1, 32);
	if (error)
		goto free_dh;

	p = get_policy_file (&cred_len, &error);
	if (error)
		goto free_p;

	/* SHA256 of signed qeo policy file */
	error = sec_hash_sha256 ((unsigned char *) p,
				 cred_len,
				 DDS_SEQ_DATA (*token->binary_value1));
	xfree (p);
	if (error)
		goto free_dh;

	/* policy version number */
	policy_version = get_policy_version (&error);
	if (error)
		goto free_dh;

	dds_seq_append ((void *) token->longlongs_value, &policy_version);

	tp->data = token;
	tp->nusers = 1;
	tp->encoding = 0;
	tp->integral = 0;
	tp->next = NULL;
	return (tp);

    free_p:
    	xfree (p);
    free_dh:
	DDS_DataHolder__free (token);
	xfree (tp);
	return (NULL);
}

/* sp_free_perm_token -- Release a previously received PermissionsToken. */

static void sp_free_perm_token (const SEC_PERM *pp, Permissions_t h)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (h)

	/* ... TBC ... */
}

/* sp_get_perm_creds -- Get a PermissionsCredential token from a permissions
			handle. */

static DDS_PermissionsCredential *sp_get_perm_creds (const SEC_PERM *pp,
					             Permissions_t  id)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (id)

	/* ... TBC ... */

	return (NULL);
}

/* sp_free_perm_creds -- Release previously received Permissions Credentials.*/

static void sp_free_perm_creds (const SEC_PERM            *pp, 
			        DDS_PermissionsCredential *creds)
{
	ARG_NOT_USED (pp)
	ARG_NOT_USED (creds)

	/* ... TBC ... */
}

/* Qeo DDS security permissions handling plugin: */

static SEC_PERM sp_perm_qeo = {
	SECC_DDS_SEC,

	QEO_CLASSID_SECURITY_PERMISSIONS_CREDENTIAL_TOKEN,
	QEO_CLASSID_SECURITY_PERMISSIONS_TOKEN,
	32,

	sp_valid_local_perms,
	sp_check_rem_perms,
	sp_get_perm_token,
	sp_free_perm_token,
	sp_get_perm_creds,
	sp_free_perm_creds
};

int sec_perm_add_qeo (void)
{
#ifdef DDS_QEO_TYPES
	init_policy_list ();
	lock_init_nr (pol_lock, "Pol lock");
#endif
	return (sec_perm_add (&sp_perm_qeo, 1));
}

