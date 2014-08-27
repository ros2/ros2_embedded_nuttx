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

/* sp_access.c -- DDS Security Plugin - Access control plugin implementations. */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sys.h"
#include "log.h"
#include "dlist.h"
#include "error.h"
#include "md5.h"
#include "nmatch.h"
#include "openssl/x509v3.h"
#include "openssl/rsa.h"
#include "openssl/ssl.h"
#include "thread.h"
#include "strseq.h"
#include "uqos.h"
#include "sp_access_db.h"
#include "sp_data.h"
#include "sp_cert.h"
#include "sp_sys.h"
#include "nsecplug/nsecplug.h"
#include "dds_data.h"
#include "sec_data.h"

static sp_extra_authentication_check_fct extra_authentication_check = NULL;

static sp_dds_policy_content_fct policy_cb = NULL;

static sp_dds_userdata_match_fct match_cb = NULL;

static uintptr_t policy_userdata;

void sp_access_set_md5 (unsigned char       *dst,
			const unsigned char *identity, 
			size_t              length)
{
	MD5_CONTEXT	md5;

	md5_init (&md5);
	md5_update (&md5, identity, length);
	md5_final (dst, &md5);
}

struct validate_st {
	const char          *name;
	size_t              length;
	IdentityHandle_t    *id;
	const char          *method;
	const unsigned char *key;
	size_t              klength;
};

/* Locked in sp_access_validate_id */

static DDS_ReturnCode_t validate_id (MSParticipant_t *p, void *data)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;
	int tmp;
	const char *name = ((struct validate_st *) data)->name;
	size_t length = ((struct validate_st *) data)->length;
	IdentityHandle_t *handle = ((struct validate_st *) data)->id;
	const unsigned char *key = ((struct validate_st *) data)->key;
	size_t klength = ((struct validate_st *) data)->klength;

	if (p->name [0] == '\0' ||
	    (strchr (p->name, '*') && !nmatch (p->name, name, 0)) ||
	    !strcmp (p->name, name)) {
		if (p->blacklist)
			goto blacklist;
		if ((tmp = sp_access_is_already_cloned (key, klength)) == -1)
			ret = sp_access_clone_participant (p, name, length, key, klength, handle);
		else
			*handle = tmp;
		if (!ret)
			log_printf (SEC_ID, 0, 
				    "SP_ACCESS: Validate local identity for '%s' -> (matched with '%s') -> accepted \r\n", name, p->name);
		else
			log_printf (SEC_ID, 0, 
				    "SP_ACCESS: Validate local identity for '%s' -> (matched with '%s') -> error \r\n", name, p->name);

		return (ret);
	}
	return (DDS_RETCODE_BAD_PARAMETER);
 blacklist:
	log_printf (SEC_ID, 0, 
		    "SP_ACCESS: Validate local identity for '%s' -> blacklisted -> denied \r\n", name);
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* Take sp_lock lock */

DDS_ReturnCode_t sp_access_validate_local_id (const char       *name,
					      size_t           length,
					      unsigned char    *key,
					      size_t           klength,
					      IdentityHandle_t *handle)
{
	DDS_ReturnCode_t ret;
	struct validate_st data;

	data.name = name;
	data.length = length;
	data.id = handle;
	data.key = key;
	data.klength = klength;

	lock_take (sp_lock);
	if ((ret = sp_access_participant_walk (validate_id,
					       (void *) &data)) == DDS_RETCODE_BAD_PARAMETER) {
		log_printf (SEC_ID, 0, "SP_ACCESS: Validate local identity for '%s' -> denied\r\n", name);
		lock_release (sp_lock);
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
	lock_release (sp_lock);
	return (ret);
}

/* Lock taken in sp_access_validate_remote_id */

static DDS_ReturnCode_t validate_remote_id (MSParticipant_t *p, void *data)
{
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;
	MSParticipant_t *np;
	const unsigned char *identity = (const unsigned char *) ((struct validate_st *) data)->name;
	IdentityHandle_t *handle = ((struct validate_st *) data)->id;
	size_t length = ((struct validate_st *) data)->length;	
	const char *method = ((struct validate_st *) data)->method;
	const unsigned char *key = (const unsigned char *) ((struct validate_st *) data)->key;
	size_t klength = ((struct validate_st *) data)->klength;

	if (method) {
		if (p->key_length == klength &&
		    !memcmp (p->key, key, klength))
			goto found;

		return (DDS_RETCODE_BAD_PARAMETER);
	found:
		*handle = p->handle;
		return (DDS_RETCODE_OK);
	}

	if (p->name [0] == '\0' ||
	    (strchr (p->name, '*') &&
	     !nmatch (p->name, (char *) identity, 0)) ||
	    !strcmp (p->name, (char *) identity)) {
		
		if (p->blacklist)
			goto blacklist;
		if ((*handle = sp_access_is_already_cloned (key, klength)) == (unsigned) -1)
			ret = sp_access_clone_participant (p, 
							   (char *) identity, 
							   length - 1,
							   key, klength,
							   handle);
		if (!ret) {
			if (!(np = sp_access_get_participant ((ParticipantHandle_t ) *handle)))
				fatal_printf ("Should not be able to happen\r\n");
			if (!np->key_length) {
				sp_access_set_md5 (np->key, identity, length - 1);
				np->key_length = 16;
			}
			log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> cloned and accepted\r\n");
		}
		else
			log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> error\r\n");
		return (ret);
	}
	else {
		
		if (p->blacklist)
			goto blacklist;
		
		/* Found it. */
		if (!p->key_length) {
			sp_access_set_md5 (p->key, identity, length - 1);
			p->key_length = 16;
		}
		*handle = p->handle;
		log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}
	return (DDS_RETCODE_BAD_PARAMETER);

 blacklist:
	log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> blacklisted -> denied\r\n");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* Take sp_lock lock */

DDS_ReturnCode_t sp_access_validate_remote_id (const char          *method,
					       const unsigned char *identity,
					       size_t              length,
					       const unsigned char *key,
					       size_t              klength,
					       int                 *validation,
					       IdentityHandle_t    *handle)
{
	struct validate_st data;
	DDS_ReturnCode_t   ret = DDS_RETCODE_OK;

	data.name = (const char *) identity;
	data.id = handle;
	data.length = length;
	data.method = method;
	data.key = key;
	data.klength = klength;

	if (method) {
		if (!strcmp (method, GMCLASSID_SECURITY_IDENTITY_TOKEN) || 
		    !strcmp (method, QEO_CLASSID_SECURITY_IDENTITY_TOKEN)) {
			*validation = DDS_AA_HANDSHAKE;
			goto continue_validation;
		}
		else 
			*validation = DDS_AA_REJECTED;

		log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> denied\r\n");
		return (DDS_RETCODE_OK);
	}
	*validation = DDS_AA_ACCEPTED;	

 continue_validation:
	lock_take (sp_lock);
	if ((ret = sp_access_participant_walk (validate_remote_id, (void *) &data))) {
		if (!method) {
			/* failed */
			*validation = DDS_AA_REJECTED;
			ret = DDS_RETCODE_OK;
			log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> denied\r\n");
		}
		else {
			ret = sp_access_add_unchecked_participant (identity, length, key, klength, handle);
			log_printf (SEC_ID, 0, "SP_ACCESS: adding a temporary unauthenticated participant [%d]\r\n",  *handle);
		}
	}
	else
		/* Found a participant that was already authorized */
		*validation = DDS_AA_ACCEPTED;

	lock_release (sp_lock);
	log_printf (SEC_ID, 0, "SP_ACCESS: Validate remote identity -> accepted\r\n");
	return (ret);
}

/* A temp workaround for QEO only to make sure the matching of the common name
   and the rules in the database succeeds */
#ifdef COMMON_NAME_ALTERATION
void alter_common_name (char *name,
			char *new_name)
{
	char *c;

	/* Copy realm id with prefix */
	c = strtok (name," ");
	strcpy (new_name, "<rid:");
	strcpy (&new_name [strlen (new_name)], c);
	
	/* Copy device id with prefix */
	c = strtok (NULL, " ");
	strcpy (&new_name [strlen (new_name)], "><did:");
	strcpy (&new_name [strlen (new_name)], c);

	/* Copy user id with prefix */
	c = strtok (NULL, " ");
	strcpy (&new_name [strlen (new_name)], "><uid:");
	strcpy (&new_name [strlen (new_name)], c);
	strcpy (&new_name [strlen (new_name)], ">");

	log_printf (SEC_ID, 0, "The common name has been changed to %s\r\n", new_name);
}
#endif

/* Take sp_lock lock */

DDS_ReturnCode_t sp_access_add_common_name (IdentityHandle_t remote,
					    unsigned char    *cert,
					    size_t           cert_len)
{
	unsigned char    name [512];
	size_t           name_len = 512;
#ifdef COMMON_NAME_ALTERATION
	char             new_name [512];
#endif
	MSParticipant_t  *p;
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	if ((ret = sp_get_common_name (cert, cert_len, &name [0], &name_len)))
		return (ret);

#ifdef COMMON_NAME_ALTERATION
	if (name_len) {
		memset (new_name, 0, 512);
		alter_common_name ((char *) name, new_name);
	}
#endif

	/* Change participant name, so take the lock */
	lock_take (sp_lock);

	if (!(p = sp_access_get_participant (remote))) {
		log_printf (SEC_ID, 0, "SP_ACCESS: could not find participant [%d] to add the common name\r\n", remote);
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto end;
	}
	if (name_len) {
#ifdef COMMON_NAME_ALTERATION
		strcpy (p->name,(const char *) new_name);
#else
		strcpy (p->name, (const char *) name);
#endif
		log_printf (SEC_ID, 0, "SP_ACCESS: changed the name of participant [%d] to %s\r\n", p->handle, p->name);
	}
	else 
		ret = DDS_RETCODE_BAD_PARAMETER;

 end:
	lock_release (sp_lock);
	return (ret);
}

/* This function is to check the remote participant after the authentication has succeeded.
   It checks the db for participants that match, and adds them as a clone 
   already locked */

static DDS_ReturnCode_t check_remote (MSParticipant_t *p, void *data)
{
	MSParticipant_t	 *pp;
	const unsigned char *identity = (const unsigned char *) ((struct validate_st *) data)->name;
	IdentityHandle_t *handle = ((struct validate_st *) data)->id;

	/* It's the previously added temp part */
	if (p->handle == *handle)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (p->name [0] == '\0' ||
	    (strchr (p->name, '*') &&
	     !nmatch (p->name, (char *) identity, 0))) {
		
		if (p->blacklist)
			goto blacklist;

		if (p->cloned)
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!(pp = sp_access_get_participant (*handle)))
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			pp->cloned = p;

		log_printf (SEC_ID, 0, "wildcard match (%s) -> accepted", p->name);
		return (DDS_RETCODE_OK);
	}
	else if (!strcmp (p->name, (char *) identity)) {
		
		if (p->blacklist)
			goto blacklist;
		
		if (p->cloned)
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!(pp = sp_access_get_participant (*handle)))
			return (DDS_RETCODE_BAD_PARAMETER);
		else
			pp->cloned = p;
				
		log_printf (SEC_ID, 0, "exact match (%s) -> accepted", p->name);
		return (DDS_RETCODE_OK);
	}
	else
		return (DDS_RETCODE_BAD_PARAMETER);

 blacklist:
	log_printf (SEC_ID, 0, "denied");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* already locked with sp_lock */

static DDS_ReturnCode_t verify_remote (IdentityHandle_t handle, unsigned *action)
						      
{
	struct validate_st data;
	DDS_ReturnCode_t   ret;
	MSParticipant_t    *p;

	log_printf (SEC_ID, 0, "SP_ACCESS: Verify remote for '%d' -> ", handle);
	if (!(p = sp_access_get_participant (handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	data.name = p->name;
	data.id = &handle;
	data.length = strlen (p->name);

	if ((ret = sp_access_participant_walk (check_remote, (void *) &data))) {
		*action = DDS_AA_REJECTED;
		log_printf (SEC_ID, 0, "\r\n");
		return (DDS_RETCODE_OK);
	}
	log_printf (SEC_ID, 0, "\r\n");
	*action = DDS_AA_ACCEPTED;
	return (DDS_RETCODE_OK);
}

/* Already locked */

static int allow_access (MSDomain_t *dp, MSParticipant_t *pp)
{
	MSUTopic_t	*tp;
	MSAccess_t	access;
	
	if (!dp->access)
		return (1);

	if (pp->cloned)
		pp = pp->cloned;

	access = pp->access;

	if (access < dp->access)
		return (0);

	if (!dp->exclusive)
		return (1);

	LIST_FOREACH (pp->topics, tp) {
		if (!tp->topic.mode)
			continue;

		if (tp->id == ~0U || tp->id == dp->domain_id)
			return (1);
	}
	return (0);
}

/* Already locked */

static DDS_ReturnCode_t sp_perm_pars (PermissionsHandle_t perm,
				      MSDomain_t          **p,
				      MSParticipant_t     **pp)
{
	unsigned	dh, ih;

	dh = perm >> 16;
	ih = perm & 0xffff;
	if (!dh || 
	    !ih || 
	    (*p = sp_access_get_domain (dh)) == NULL ||
	    (*pp = sp_access_get_participant_by_perm (ih)) == NULL)
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);

	return (DDS_RETCODE_OK);
}

/* Lookup functions 
   Already locked */

static MSTopic_t *lookup_topic (MSDomain_t      *dp,
				MSParticipant_t *pp,
				const char      *topic_name)
{
	MSUTopic_t	*up;
	MSTopic_t	*tp;
	
	if (pp->cloned)
		pp = pp->cloned;

	LIST_FOREACH (pp->topics, up) {
		if (up->id != ~0U &&
		    up->id != dp->domain_id)
			continue;

		if (!up->topic.name) {
			log_printf (SEC_ID, 0, " Participant Topic rule for \'%s\' (matched with *)\r\n", topic_name);
			if (!up->topic.blacklist)
				return (&up->topic);
			else
				goto blacklist;
		}
		else if (strchr (up->topic.name, '*')) {
			if (!nmatch (up->topic.name, topic_name, 0)) {
				log_printf (SEC_ID, 0, " Participant Topic rule for \'%s\' (matched with %s)\r\n", topic_name, up->topic.name);
				if (!up->topic.blacklist)
					return (&up->topic);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (up->topic.name, topic_name)) {
			log_printf (SEC_ID, 0, " Participant Topic rule for \'%s' (exact match)\r\n", topic_name);
			if (!up->topic.blacklist)
				return (&up->topic);
			else
				goto blacklist;
		}
	}

	/* No match in participant topic ruleset, check domain topics. */

	LIST_FOREACH (dp->topics, tp) {
		if (!tp->name) {
			log_printf (SEC_ID, 0, "Domain Topic rule for \'%s\' (matched with *)\r\n", topic_name);
			if (!tp->blacklist)
				return (tp);
			else
				goto blacklist;
		}

		else if (strchr (tp->name, '*')) {
			if (!nmatch (tp->name, topic_name, 0)) {
				log_printf (SEC_ID, 0, " Domain Topic rule for \'%s\' (matched with %s)\r\n", topic_name, tp->name);
				if (!tp->blacklist)
					return (tp);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (tp->name, topic_name)) {
			log_printf (SEC_ID, 0, " Domain Topic rule for \'%s\' (exact match)\r\n", topic_name);
			if (!tp->blacklist)
				return (tp);
			else
				goto blacklist;
		}
	}
	log_printf (SEC_ID, 0, " No Topic rule found\r\n");
       	return (NULL);

 blacklist:
	log_printf (SEC_ID, 0, " (Blacklisted) \r\n");
	return (NULL);
}

/* Already locked */

static MSPartition_t *lookup_partition (MSDomain_t      *dp,
					MSParticipant_t *pp,
					const char      *partition_name,
					MSMode_t        mode)
{
	MSUPartition_t	*up;
	MSPartition_t	*tp;
	
	if (pp->cloned)
		pp = pp->cloned;

	LIST_FOREACH (pp->partitions, up) {
		if (up->id != ~0U &&
		    up->id != dp->domain_id)
			continue;

		if ((up->partition.mode & mode) == 0)
			continue;
		
		if (!up->partition.name) {
			log_printf (SEC_ID, 0, " Participant Partition rule for \'%s\' (matched with *)\r\n", partition_name);
			if (!up->partition.blacklist)
				return (&up->partition);
			else
				goto blacklist;
		}

		else if (strchr (up->partition.name, '*')) {
			if (!nmatch (up->partition.name, partition_name, 0)) {
				log_printf (SEC_ID, 0, " Participant Partition rule for \'%s\' (matched with %s)\r\n", partition_name, up->partition.name);
				if (!up->partition.blacklist)
					return (&up->partition);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (up->partition.name, partition_name)) {
			log_printf (SEC_ID, 0, " Participant Partition rule for \'%s\' (exact match)\r\n", partition_name);
			if (!up->partition.blacklist)
				return (&up->partition);
			else
				goto blacklist;
		}
	}

	/* No match in participant partition ruleset, check domain partitions. */
	LIST_FOREACH (dp->partitions, tp) {
		if ((tp->mode & mode) == 0)
			continue;
		
		if (!tp->name)
			if (!tp->blacklist) {
				log_printf (SEC_ID, 0, " Domain Partition rule for \'%s\' (matched with *)\r\n", partition_name);
				return (tp);
			}
			else
				goto blacklist;

		else if (strchr (tp->name, '*')) {
			if (!nmatch (tp->name, partition_name, 0)) {
				log_printf (SEC_ID, 0, " Domain Partition rule for \'%s\' (matched with %s)\r\n", partition_name, tp->name);
				if (!tp->blacklist)
					return (tp);
				else
					goto blacklist;
			}
		}
		else if (!strcmp (tp->name, partition_name)) {
			log_printf (SEC_ID, 0, " Domain Partition rule for \'%s\' (exact match)\r\n", partition_name);
			if (!tp->blacklist)
				return (tp);
			else
				goto blacklist;
		}
	}
	log_printf (SEC_ID, 0, " No Partition rule found for \'%s\'\r\n", partition_name);
	return (NULL);
 blacklist:
	log_printf (SEC_ID, 0, " (Blacklisted) \r\n");
	return (NULL);
}

/* Local checks */

/* This function is locked on entry with sp_lock */
/* Already locked */

DDS_ReturnCode_t sp_validate_local_perm (DDS_DomainId_t   domain_id,
					 IdentityHandle_t *handle)
{
	MSDomain_t	*p;
	MSParticipant_t	*pp;

	log_printf (SEC_ID, 0, "SP_ACCESS: Validate local permissions -> ");

	p = sp_access_lookup_domain (domain_id, 1);
	
	if (!p)
		log_printf (SEC_ID, 0, "SP: not p");
	if (!*handle)
		log_printf (SEC_ID, 0, "SP: not *handle");

	if (!p) {
		log_printf (SEC_ID, 0, ", domain not found -> accepted as unsecure domain.\r\n");
		*handle = 1;
		return (DDS_RETCODE_OK);
	}
    	if (!p || !*handle) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
	pp = sp_access_get_participant (*handle);
	if (allow_access (p, pp)) {
		/* If the permissions were updated, use the updated permissions */
		pp->permissions_handle = (pp->updated_perm_handle) ? 
			pp->updated_perm_handle : pp->permissions_handle;
		pp->updated_perm_handle = 0;
		*handle = (p->handle << 16) | pp->permissions_handle;
		pp->permissions = *handle;
		log_printf (SEC_ID, 0, "accepted.\r\n");
		return (DDS_RETCODE_OK);
	}
	else {
		*handle = 0;
		pp->permissions = *handle;
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_validate_peer_perm (IdentityHandle_t    local,
					IdentityHandle_t    remote,
					const char          *classid,
					unsigned char       *permissions,
					size_t              length,
					PermissionsHandle_t *handle)
{
	unsigned d_handle, action;
	MSDomain_t *d;
	MSParticipant_t	*local_p, *remote_p, *pp;
	DDS_ReturnCode_t ret;

	if (!(local_p = sp_access_get_participant (local)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!(remote_p = sp_access_get_participant (remote)))
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Get the domain handle from the local permissions handle */
	d_handle = local_p->permissions >> 16;
	if (!(d = sp_access_get_domain (d_handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (strstr (classid, "DTLS")) {

		log_printf (SEC_ID, 0, "SP_ACCESS: Validate peer permissions -> ");
		if (!d) {
			log_printf (SEC_ID, 0, ", domain not found -> accepted as unsecure domain.\r\n");
			*handle = 1;
			remote_p->permissions = *handle;
			return (DDS_RETCODE_OK);
		}
		pp = sp_access_lookup_participant_by_perm (permissions, length);
		if (pp != remote_p)
			goto end;
		if (allow_access (d, remote_p)) {
			/* If the permissions were updated, use the updated permissions */
			remote_p->permissions_handle = (remote_p->updated_perm_handle) ? 
				remote_p->updated_perm_handle : remote_p->permissions_handle;
			remote_p->updated_perm_handle = 0;
			*handle = (d_handle << 16) | remote_p->permissions_handle;
			remote_p->permissions = *handle;
			log_printf (SEC_ID, 0, "accepted.\r\n");
			return (DDS_RETCODE_OK);
		}
	}
	else {
		/* Add all kinds of checks we want */
		if ((ret = verify_remote (remote, &action)) || action != DDS_AA_ACCEPTED)
			goto end;
		/* If the permissions were updated, use the updated permissions */
		remote_p->permissions_handle = (remote_p->updated_perm_handle) ? 
			remote_p->updated_perm_handle : remote_p->permissions_handle;
		remote_p->updated_perm_handle = 0;
		*handle = d_handle << 16 | remote_p->permissions_handle;
		remote_p->permissions = *handle;
		return (DDS_RETCODE_OK);
	}
 end:
	*handle = 0;
	log_printf (SEC_ID, 0, "denied.\r\n");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_create_participant (PermissionsHandle_t            perm,
					      const DDS_DomainParticipantQos *qos,
					      unsigned                       *secure)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (qos)

	log_printf (SEC_ID, 0, "SP_ACCESS: Check if local participant may be created -> ");

	if (perm == 1) {
		*secure = 0;
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}
	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "denied.\r\n");
		return (ret);
	}
	*secure = dp->access & DDS_SECA_LEVEL;
	if (dp->controlled)
		*secure |= DDS_SECA_ACCESS_PROTECTED;
	if (dp->msg_encrypt) {
		*secure |= DDS_SECA_RTPS_PROTECTED;
		*secure |= (dp->msg_encrypt << DDS_SECA_ENCRYPTION_SHIFT);
	}
	log_printf (SEC_ID, 0, "accepted.\r\n");
	return (DDS_RETCODE_OK);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_create_topic (PermissionsHandle_t perm,
					const char          *topic_name,
					const DDS_TopicQos  *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (qos)
	
	log_printf (SEC_ID, 0, "SP_ACCESS: Topic('%s') create request by local participant -> ", topic_name);

	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}

	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_CREATE) == 0) {
		log_printf (SEC_ID, 0, "--> denied.\r\n");
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_create_writer (PermissionsHandle_t     perm,
					 const char              *topic_name,
					 const DDS_DataWriterQos *qos,
					 const Strings_t         *partitions,
					 unsigned                *secure)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	unsigned         i;
	String_t         **name_ptr;
	const char       *part_ptr;

	ARG_NOT_USED (qos)

	log_printf (SEC_ID, 0, "SP_ACCESS: DataWriter('%s') create request by local participant -> ", topic_name);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}
	
	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_WRITE) == 0) 
		goto denied;
	
	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {
			/* The partition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (sp_access_has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip,(char*) part_ptr, TA_WRITE);
			if (!pp || (pp->mode & TA_WRITE) == 0)
				goto denied;
		}
	}
	*secure = 0;
	if (tp->controlled)
		*secure |= DDS_SECA_ACCESS_PROTECTED;
	if (tp->disc_enc)
		*secure |= DDS_SECA_DISC_PROTECTED;
	if (tp->submsg_enc)
		*secure |= DDS_SECA_SUBMSG_PROTECTED;
	if (tp->payload_enc)
		*secure |= DDS_SECA_PAYLOAD_PROTECTED;
	if (tp->submsg_enc || tp->payload_enc)
		*secure |= (tp->crypto_mode << DDS_SECA_ENCRYPTION_SHIFT);
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_create_reader (PermissionsHandle_t     perm,
					 const char              *topic_name,
					 const DDS_DataWriterQos *qos,
					 const Strings_t         *partitions,
					 unsigned                *secure)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	unsigned         i;
	String_t **name_ptr;
	const char *part_ptr;
		
	ARG_NOT_USED (qos);

	log_printf (SEC_ID, 0, "SP_ACCESS: DataReader('%s') create request by local participant -> ", topic_name);

	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}

	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_READ) == 0)
		goto denied;

	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {

			/* The partition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (sp_access_has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip, (char*) part_ptr, TA_READ);
			if (!pp || (pp->mode & TA_READ) == 0)
				goto denied;
		}
	}
	*secure = 0;
	if (tp->controlled)
		*secure |= DDS_SECA_ACCESS_PROTECTED;
	if (tp->disc_enc)
		*secure |= DDS_SECA_DISC_PROTECTED;
	if (tp->submsg_enc)
		*secure |= DDS_SECA_SUBMSG_PROTECTED;
	if (tp->payload_enc)
		*secure |= DDS_SECA_PAYLOAD_PROTECTED;
	if (tp->submsg_enc || tp->payload_enc)
		*secure |= (tp->crypto_mode << DDS_SECA_ENCRYPTION_SHIFT);
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_peer_participant (PermissionsHandle_t perm,
					    String_t            *user_data)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (user_data)
	
	log_printf (SEC_ID, 0, "SP_ACCESS: Check if peer participant is allowed [%d] -> ", perm);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}
	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_peer_topic (PermissionsHandle_t      perm,
				      const char               *topic_name,
				      const DiscoveredTopicQos *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (qos)
	
	log_printf (SEC_ID, 0, "SP_ACCESS: Topic('%s') create request by peer participant -> ", topic_name);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}

	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_CREATE) == 0) {
		log_printf (SEC_ID, 0, "--> denied.\r\n");
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_peer_writer (PermissionsHandle_t      perm,
				       const char               *topic_name,
				       const DiscoveredWriterQos *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	Strings_t       *partitions;
	String_t        **name_ptr;
	const char      *part_ptr;
	unsigned        i;

	partitions = qos->partition;
	
	log_printf (SEC_ID, 0, "SP_ACCESS: DataWriter('%s') create request by peer participant -> ", topic_name);

	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}

	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_WRITE) == 0)
		goto denied;
	
	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {
			/* The parition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (sp_access_has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip, (char*) part_ptr, TA_WRITE);
			if (!pp || (pp->mode & TA_WRITE) == 0)
				goto denied;
		}
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_check_peer_reader (PermissionsHandle_t      perm,
					 const char               *topic_name,
					 const DiscoveredReaderQos *qos)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	MSTopic_t	*tp;
	MSPartition_t   *pp;
	DDS_ReturnCode_t ret;
	Strings_t       *partitions;
	String_t        **name_ptr;
	const char      *part_ptr;
	unsigned        i;

	log_printf (SEC_ID, 0, "SP_ACCESS: DataReader('%s') create request by peer participant -> ", topic_name);
	
	if (perm == 1) {
		log_printf (SEC_ID, 0, "unsecure domain -> accepted\r\n");
		return (DDS_RETCODE_OK);
	}
	
	partitions = qos->partition;
	
	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret) {
		log_printf (SEC_ID, 0, "error.\r\n");
		return (ret);
	}
	tp = lookup_topic (dp, ip, topic_name);
	if (!tp || (tp->mode & TA_READ) == 0)
		goto denied;

	if (partitions) {
		DDS_SEQ_FOREACH_ENTRY(*partitions, i, name_ptr) {
			/* The partition string cannot have wildcards */
			part_ptr = str_ptr(*name_ptr);
			if (sp_access_has_wildcards (part_ptr))
				goto denied;

			pp = lookup_partition (dp, ip, (char*) part_ptr, TA_READ);
			if (!pp || (pp->mode & TA_READ) == 0)
				goto denied;
		}
	}
	log_printf (SEC_ID, 0, "--> accepted.\r\n");
	return (DDS_RETCODE_OK);
 denied:
	log_printf (SEC_ID, 0, "--> denied.\r\n");
	return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
}


/* Permissions */

DDS_ReturnCode_t sp_get_perm_token (PermissionsHandle_t handle,
				    unsigned char       *permissions,
				    size_t              *perm_length)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;

	ret = sp_perm_pars (handle, &dp, &ip);
	if (ret)
		return (ret);

	if (*perm_length < ip->key_length)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (ip->key_length)
		memcpy (permissions, ip->key, ip->key_length);
	*perm_length = ip->key_length;
	return (DDS_RETCODE_OK);
}

void DDS_SP_set_policy_cb (sp_dds_policy_content_fct cb, uintptr_t userdata)
{
	policy_cb = cb;
	policy_userdata = userdata;
	log_printf(SEC_ID, 0, "SP_ACCESS: Set callback to be called when policy string is received or send\r\n");
}

void DDS_SP_set_userdata_match_cb (sp_dds_userdata_match_fct cb)
{
	match_cb = cb;
	log_printf (SEC_ID, 0, "SP_ACCESS: Set callback to call when userdata matching is required\r\n");
}

/* Not yet locked */

DDS_ReturnCode_t sp_access_get_permissions (PermissionsHandle_t handle,
					    unsigned char       *data,
					    size_t              max,
					    size_t              *length,
					    uint64_t            *version)
{
	DDS_ReturnCode_t ret;
	size_t	len;
	unsigned char *policy_file = NULL;
	size_t        policy_length = 0;
	static const char policy_data [] = 
		"Permissions as based on QEO policy file data!\r\n"
		"Should really be changed to a real DDS permissions-based mechanism "
		"using an XML-file based signed permissions document.\r\n"
		"The DDS Security implementation supports this.\r\n";

	ARG_NOT_USED (handle)

	/* in case of qeo policy file */
	if (policy_cb) {
		if (version)
			return (policy_cb (policy_userdata, version, NULL, 0, 0));

		if (*length != 0) {
			if (!(policy_file = (unsigned char *) xmalloc (*length)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		if ((ret = policy_cb (policy_userdata, NULL, (void *) policy_file, &policy_length, 0))) {
			if (policy_file)
				xfree (policy_file);
			return (ret);
		}		
		if (max && max < policy_length) {
			if (policy_file)
				xfree (policy_file);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		*length = policy_length;
		if (!max || !data) {
			if (policy_file)
				xfree (policy_file);
			return (DDS_RETCODE_OK);
		}
		memcpy (data, policy_file, policy_length);
		if (policy_file)
			xfree (policy_file);
		return (DDS_RETCODE_OK);
	}
	
	if (version) {
		*version = 0;
		return (DDS_RETCODE_OK);
	}

	len = strlen (policy_data);
	if (max && max < len)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	*length = len;
	if (!max || !data)
		return (DDS_RETCODE_OK);

	memcpy (data, policy_data, len);
	return (DDS_RETCODE_OK);
}

/* Not yet locked */

DDS_ReturnCode_t sp_access_set_permissions (PermissionsHandle_t handle,
					    unsigned char       *data,
					    size_t              length)
{
	ARG_NOT_USED (handle)

	/* Return the qeo policy file to qeo */
	if (policy_cb)
		return (policy_cb (policy_userdata, NULL, (char *) data, &length, 1));

	return (DDS_RETCODE_OK);
}

#if 0
/* The most important part in this matching is the way the rules are build
   when a rule states:
   [uid:1]
       tmp=r<uid:1;uid:2>w<uid:1;uid:3>

   that means that uid 1 for topic tmp
   can only READ FROM uid:1 and uid:2
   and WRITE TO uid:1 and uid:3
*/

static char *get_uid_from_name (char *name, char *uid, int maxsize)
{
	char *p;
	int i = 0;

	memset (uid, 0, maxsize);
	strcpy (uid, "uid:");

	/* locate last occurance of > */
	if (!(p = strrchr (name, '>')))
		return (p);
	while (memcmp (p, ":", 1)) {
		p--;
		i++;
	}
	/* Copy the uid */
	memcpy (&uid [4], ++p, --i);
	return (uid);
}

/* uid1: the uid of the one that made the rule
   uid2: the uid of the one that needs to be checked */

static DDS_ReturnCode_t check_topic_write_access (const char *topic, char *uid1, char *uid2)
{
	char *s, *p = NULL, *q = NULL;

	/* s is the current rule string for the current topic */
	/* there can be for one uid only one topic string */

	/* TODO: fetch s based on topic and uid1 */

	while ((p = strstr (s, uid2))) {
		q = p;
		while (q != s) {
			if (!memcmp (q, "w", 1))
				/* okay */
				return (DDS_RETCODE_OK);
			else if (!memcmp (q, "r", 1))
				/* not okay */
				break;
			else
				q--;
		}
	}

	return (DDS_RETCODE_ACCESS_DENIED);
}

/* uid1: the uid of the one that made the rule
   uid2: the uid of the one that needs to be checked */

static DDS_ReturnCode_t check_topic_read_access (const char *topic, char *uid1, char *uid2)
{
	char *s, *p = NULL, *q = NULL;

	/* s is the current rule string for the current topic */
	/* there can be for one uid only one topic string */

	/* TODO: fetch s based on topic and uid1 */

	while ((p = strstr (s, uid2))) {
		q = p;
		while (q != s) {
			if (!memcmp (q, "r", 1))
				/* okay */
				return (DDS_RETCODE_OK);
			else if (!memcmp (q, "w", 1))
				/* not okay */
				break;
			else
				q--;
		}
	}
	return (DDS_RETCODE_ACCESS_DENIED);
}
#endif

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_access_check_local_datawriter_match (Permissions_t local_w,
							 Permissions_t remote_r,
							 const char    *tag_w,
							 const char    *tag_r,
							 String_t      *w_userdata,
							 String_t      *r_userdata)
{
	MSDomain_t	 *local_dp, *remote_dp;
	MSParticipant_t	 *local_ip, *remote_ip;
	MSTopic_t       *tp;
	DDS_ReturnCode_t ret;
	int              i;

	ret = sp_perm_pars (local_w, &local_dp, &local_ip);
	if (ret)
		return (ret);
	
	ret = sp_perm_pars (remote_r, &remote_dp, &remote_ip);
	if (ret)
		return (ret);

	if (!(tp = lookup_topic (local_dp, local_ip, tag_w)))
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Check if the local may write to the remote */
	/* The app fine grained is always a subset of the fine grained
	   so if there is an app defined fine grained rule, we only 
	   have to check this one 
	   TO BE CHECKED: is the subset rule checked here or somewhere else?
	*/
	/* when a fine grained app is there, a fine grained should be there as well */
	if (tp->fine_app_topic && tp->fine_topic) {
		for (i = 0; i < MAX_ID_HANDLES; i++) {
			if (tp->fine_app_topic->write [i] == remote_ip->cloned->handle)
				/* eureka but is it allowed by the fine_grained? */
				goto fine1;
			else if (tp->fine_app_topic->write [i] == 0) {
				/* Not allowed */
				log_printf (SEC_ID, 0, "SP: local_datawriter_match: fine app topic rule does not allow (%d) to write to (%d)\r\n", 
					    local_ip->handle, remote_ip->cloned->handle);
				return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
			}
		}
	} else {
	fine1: 
		if (tp->fine_topic) {
			for (i = 0; i < MAX_ID_HANDLES; i++) {
				if (tp->fine_topic->write [i] == remote_ip->cloned->handle)
					/* eureka */
					goto next;
				else if (tp->fine_topic->write [i] == 0) {
					log_printf (SEC_ID, 0, "SP: local_datawriter_match: fine topic rule does not allow (%d) to write to (%d)\r\n", 
						    local_ip->handle, remote_ip->cloned->handle);
					/* Not allowed */
					return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
				}
			}
		}
	}

 next:
	/* Check if the remote may read from the local */
    if (!(tp = lookup_topic (remote_dp, remote_ip, tag_r)))
        return (DDS_RETCODE_BAD_PARAMETER);

	/* The app fine grained is always a subset of the fine grained
	   so if there is an app defined fine grained rule, we only 
	   have to check this one */
	if (tp->fine_app_topic && tp->fine_topic) {
		for (i = 0; i < MAX_ID_HANDLES; i++) {
			if (tp->fine_app_topic->read [i] == local_ip->cloned->handle)
				/* eureka but is it allowed by the fine_grained */
				goto fine2;
			else if (tp->fine_app_topic->read [i] == 0) {
				/* Not allowed */
				log_printf (SEC_ID, 0, "SP: local_datawriter_match: fine app topic rule does not allow (%d) to read from (%d)\r\n", 
					    remote_ip->handle, local_ip->cloned->handle);
				return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
			}
		}
	} else {
	fine2:
		if (tp->fine_topic) {
			for (i = 0; i < MAX_ID_HANDLES; i++) {
				if (tp->fine_topic->read [i] == local_ip->cloned->handle)
					/* eureka */
					goto success;
				else if (tp->fine_topic->read [i] == 0) {
					/* Not allowed */
					log_printf (SEC_ID, 0, "SP: local_datawriter_match: fine topic rule does not allow (%d) to read from (%d)\r\n", 
						    remote_ip->handle, local_ip->cloned->handle);
					return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
				}
			}
		}
		goto success;
	}
 success:
	if (r_userdata || w_userdata) {
		/* check if there is a callback */
		if (match_cb) {
			return (match_cb (tag_r, r_userdata ? str_ptr (r_userdata) : NULL, 
					  w_userdata ? str_ptr (w_userdata) : NULL));
		} else
			warn_printf ("SP_ACCESS: there is userdata available for matching, but no matching cb function\r\n");
	}
	return (DDS_RETCODE_OK);	
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_access_check_local_datareader_match (Permissions_t local_r,
							 Permissions_t remote_w,
							 const char    *tag_r,
							 const char    *tag_w,
							 String_t      *r_userdata,
							 String_t      *w_userdata)
{
	MSDomain_t	*local_dp, *remote_dp;
	MSParticipant_t	*local_ip, *remote_ip;
	MSTopic_t       *tp;
	DDS_ReturnCode_t ret;
	int              i;

	ret = sp_perm_pars (local_r, &local_dp, &local_ip);
	if (ret)
		return (ret);
	
	ret = sp_perm_pars (remote_w, &remote_dp, &remote_ip);
	if (ret)
		return (ret);

	if (!(tp = lookup_topic (local_dp, local_ip, tag_r)))
		return (DDS_RETCODE_BAD_PARAMETER);

	/* Check if the local may read from the remote */
	/* The app fine grained is always a subset of the fine grained
	   so if there is an app defined fine grained rule, we only 
	   have to check this one */
	if (tp->fine_app_topic && tp->fine_topic) {
		for (i = 0; i < MAX_ID_HANDLES; i++) {
			if (tp->fine_app_topic->read [i] == remote_ip->cloned->handle)
				/* eureka */
				goto fine1;
			else if (tp->fine_app_topic->read [i] == 0) {
				/* Not allowed */
				log_printf (SEC_ID, 0, "SP: local_datareader_match: fine app topic rule does not allow (%d) to read from (%d)\r\n", 
					    local_ip->handle, remote_ip->cloned->handle);
				return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
			}
		}
	} else {
	fine1:
		if (tp->fine_topic) {
			for (i = 0; i < MAX_ID_HANDLES; i++) {
				if (tp->fine_topic->read [i] == remote_ip->cloned->handle)
					/* eureka */
					goto next;
				else if (tp->fine_topic->read [i] == 0) {
					/* Not allowed */
					log_printf (SEC_ID, 0, "SP: local_datareader_match: fine topic rule does not allow (%d) to read from (%d)\r\n", 
						    local_ip->handle, remote_ip->cloned->handle);
					return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
				}
			}
		}
	}
 next:
	/* Check if the remote may write to the local */
    if (!(tp = lookup_topic (remote_dp, remote_ip, tag_w)))
        return (DDS_RETCODE_BAD_PARAMETER);

	/* The app fine grained is always a subset of the fine grained
	   so if there is an app defined fine grained rule, we only 
	   have to check this one */
	if (tp->fine_app_topic && tp->fine_topic) {
		for (i = 0; i < MAX_ID_HANDLES; i++) {
			if (tp->fine_app_topic->write [i] == local_ip->cloned->handle)
				/* eureka */
				goto fine2;
			else if (tp->fine_app_topic->write [i] == 0) {
				/* Not allowed */
				log_printf (SEC_ID, 0, "SP: local_datareader_match: fine app topic rule does not allow (%d) to write to (%d)\r\n", 
					    remote_ip->handle, local_ip->cloned->handle);
				return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
			}
		}
	} else {
	fine2:
		if (tp->fine_topic) {
			for (i = 0; i < MAX_ID_HANDLES; i++) {
				if (tp->fine_topic->write [i] == local_ip->cloned->handle)
					/* eureka */
					goto success;
				else if (tp->fine_topic->write [i] == 0) {
					/* Not allowed */
					log_printf (SEC_ID, 0, "SP: local_datareader_match: fine topic rule does not allow (%d) to write to (%d)\r\n", 
						    remote_ip->handle, local_ip->cloned->handle);
					return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
				}
			}
		}
	}
 success:
	if (r_userdata || w_userdata) {
		/* check if there is a callback */
		if (match_cb) {
			return (match_cb (tag_w, r_userdata ? str_ptr (r_userdata) : NULL,
					  w_userdata ? str_ptr (w_userdata) : NULL));
		} else
			warn_printf ("SP_ACCESS: there is userdata available for matching, but no matching cb function\r\n");
	}
	return (DDS_RETCODE_OK);	
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_get_domain_sec_caps (DDS_DomainId_t domain_id,
					 unsigned       *sec_caps)
{
	MSDomain_t	*p;

	p = sp_access_lookup_domain (domain_id, 1);
	if (p)
		*sec_caps = p->transport;
	else
		*sec_caps = 0;

	return (DDS_RETCODE_OK);
}

void sp_set_extra_authentication_check (sp_extra_authentication_check_fct f)
{
	extra_authentication_check = f;
}

/* This function is locked on entry with sp_lock 
   Already locked */

DDS_ReturnCode_t sp_validate_ssl_connection (PermissionsHandle_t perm,
					     SSL                 *ssl,
					     struct sockaddr     *sp,
					     int                 *action)
{
	MSDomain_t	*dp;
	MSParticipant_t	*ip;
	DDS_ReturnCode_t ret;
	X509             *peer_cert;

	struct sockaddr_in	*s4;
#ifdef DDS_IPV6
	struct sockaddr_in6	*s6;
	char			addrbuf [INET6_ADDRSTRLEN];
#endif

	if (sp->sa_family == AF_INET) {
		s4 = (struct sockaddr_in *) sp;
		log_printf (SEC_ID, 0, "DTLS: connection from %s:%d ->",
			     inet_ntoa (s4->sin_addr),
		             ntohs (s4->sin_port));
	}
#ifdef DDS_IPV6
	else if (sp->sa_family == AF_INET6) {
		s6 = (struct sockaddr_in6 *) sp;
		log_printf (SEC_ID, 0, "DTLS: connection from %s:%d ->",
        		     inet_ntop (AF_INET6,
					&s6->sin6_addr,
					addrbuf,
					INET6_ADDRSTRLEN),
			     ntohs (s6->sin6_port));
	}
#endif
	ret = sp_perm_pars (perm, &dp, &ip);
	if (ret)
		goto denied;

	/* The check to see if peer user is who he says he is, needs to be performed in qeo */
	
	if (extra_authentication_check != NULL)
		if (extra_authentication_check ((void *) ssl, ip->name) != DDS_RETCODE_OK) {
			log_printf (SEC_ID, 0, "Extra Authentication check -> Denied.\r\n");
			*action = DDS_AA_REJECTED;
			return (DDS_RETCODE_OK);
		}
		
	if ((peer_cert = SSL_get_peer_certificate (ssl))) {
#ifdef DDS_DEBUG
		sp_log_X509(peer_cert);
#endif
		log_printf (SEC_ID, 0, "\nDTLS: cipher: %s ->", SSL_CIPHER_get_name (SSL_get_current_cipher (ssl)));
		log_printf (SEC_ID, 0, "accepted.\r\n");
		*action = DDS_AA_ACCEPTED;
		X509_free (peer_cert);
	}
	else 
		goto denied;
	return (DDS_RETCODE_OK);

 denied:
	log_printf (SEC_ID, 0, "denied.\r\n");
	*action = DDS_AA_REJECTED;
	return (DDS_RETCODE_OK);
	
}
