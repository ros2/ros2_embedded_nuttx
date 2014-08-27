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

/* sec_access.c -- Implements the function that are defined 
                   for the access plugin. */

#include <stdio.h>
#include "log.h"
#include "error.h"
#include "strseq.h"
#include "disc.h"
#include "sec_log.h"
#include "sec_perm.h"
#include "sec_access.h"
#include "sec_p_std.h"
#include "sec_p_dtls.h"
#include "sec_p_qeo.h"

#define	MAX_PERM_PLUGINS 4

static unsigned		nperm_plugins;
static const SEC_PERM	*perm_plugins [MAX_PERM_PLUGINS];

int sec_perm_init (void)
{
	nperm_plugins = 0;
#ifdef QEO_SECURITY
	sec_perm_add_qeo ();
#endif
	sec_perm_add_std ();
#ifdef DTLS_SECURITY
	sec_perm_add_dtls ();
#endif
	return (DDS_RETCODE_OK);
}

int sec_perm_add (const SEC_PERM *pp, int req_default)
{
	if (nperm_plugins >= MAX_PERM_PLUGINS)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	perm_plugins [nperm_plugins] = pp;
	if (req_default) {
		if (nperm_plugins)
			perm_plugins [nperm_plugins] = perm_plugins [0];
		perm_plugins [0] = pp;
	}
	nperm_plugins++;
	return (DDS_RETCODE_OK);
}

/* sec_validate_local_permissions -- Validate local permissions based on the
				     given permissions credentials and return
				     a corresponding permissions handle. */

Permissions_t sec_validate_local_permissions (Identity_t                id,
					      DDS_DomainId_t            domain_id,
					      DDS_PermissionsCredential *creds,
					      DDS_ReturnCode_t          *error)
{
	DDS_SecurityReqData	data;
	Permissions_t		perms;
	PermissionsData_t	*p;
	const SEC_PERM		*pp;
	unsigned		i, nplugins;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("validate_local_permissions");
	if (!plugin_policy) {
		sec_log_ret ("%u", 1);
		return (1);
	}
	data.handle = id;
	data.domain_id = domain_id;
	data.data = creds;
	data.action = 1; /* this means we have to take the lock */
	*error = sec_access_control_request (DDS_VALIDATE_LOCAL_PERM, &data);
	if (*error) {
		sec_log_ret ("%u", 0);
		return (0);
	}
	perms = data.handle;
	p = perm_create (perms, error);
	if (!p) {
		sec_log_ret ("%u", 0);
		return (0);
	}

	/* Check all Permissions plugins in priority order and store them in
	   the permissions data. */
	for (i = 0, nplugins = 0; i < nperm_plugins; i++) {
		pp = perm_plugins [i];
		if (pp->valid_loc_perm) {
			ret = (*pp->valid_loc_perm) (pp, id, perms);
			if (!ret)
				perm_add_plugin (p, pp);
				nplugins++;
		}
	}
	if (!nplugins) {
		*error = DDS_RETCODE_ALREADY_DELETED;
		perms = 0;
	}
	p->id = id;
	sec_log_ret ("0x%x", perms);
	return (perms);
}

static DDS_ReturnCode_t sec_revalidate_local_permissions (Domain_t *dp, int take_lock)
{
	PermissionsData_t	*pp;
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	error;
	unsigned		perms;

	data.handle = dp->participant.p_id;
	data.domain_id = dp->domain_id;
	data.data = NULL;
	data.action = take_lock; /* Function requires a lock. */
	error = sec_access_control_request (DDS_VALIDATE_LOCAL_PERM, &data);
	if (error) {
		sec_log_ret ("%u", error);
		return (error);
	}
	perms = data.handle;
	if (dp->participant.p_permissions == perms) /* No changes ... */
		return (DDS_RETCODE_OK);

	perm_release (dp->participant.p_permissions);
	pp = perm_lookup (perms, NULL);
	if (pp) {
		perm_ref (pp);
		dp->participant.p_permissions = perms;

		/* Only notify that you have new permissions. */
		disc_participant_rehandshake (dp, 1);
		return (DDS_RETCODE_OK);
	}
	pp = perm_create (perms, &error);
	if (!pp) {
		sec_log_ret ("%u", error);
		return (error);
	}
	dp->participant.p_permissions = perms;

	return (DDS_RETCODE_OK);
}

void dds_sec_local_perm_update (int take_lock)
{
	Domain_t	*dp;
	unsigned	i;
	DDS_ReturnCode_t error;

	log_printf (SEC_ID, 0, "sec_local_permissions_update\r\n");

	if (!domains_used ())
		return;

	for (i = 1; i <= MAX_DOMAINS; i++) {
		dp = domain_get (i, 1, &error);
		if (!dp || !dp->security || !dp->participant.p_permissions) {
			if (dp)
				lock_release (dp->lock);
			continue;
		}
		sec_revalidate_local_permissions (dp, take_lock);
		lock_release (dp->lock);
	}
}

static DDS_ReturnCode_t sec_revalidate_permissions (Domain_t *dp, int notify_only)
{
	PermissionsData_t	*pp;
	const SEC_PERM		*plp;
	unsigned		caps, perms, i, nplugins;
	DDS_ReturnCode_t	error;

	perms = dp->participant.p_permissions;
	pp = perm_lookup (perms, NULL);
	if (!pp) {
		warn_printf ("sec revalidate permissions could not find permissions! Unexpected behaviour, Maybe sec_revalidate_local_permissions was not yet called\r\n");
		return (DDS_RETCODE_ERROR);
	}

	/* Check all Permissions plugins in priority order and store them in
	   the permissions data. */
	for (i = 0, nplugins = 0; i < nperm_plugins; i++) {
		plp = perm_plugins [i];
		if (plp->valid_loc_perm) {
			error = (*plp->valid_loc_perm) (plp, dp->participant.p_id, perms);
			if (!error)
				perm_add_plugin (pp, plp);
				nplugins++;
		}
	}
	if (!nplugins) {
		warn_printf ("DDS: no valid access control plugins!");
		error = DDS_RETCODE_ALREADY_DELETED;
		perms = 0;
	}
	caps = (dp->participant.p_sec_caps & 0xffff) | 
	       (dp->participant.p_sec_caps >> 16);
	dp->participant.p_p_tokens = sec_get_permissions_tokens (perms, caps);
	if (!dp->participant.p_p_tokens) {
		warn_printf ("DDS: can't derive permissions token!");
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	disc_participant_rehandshake (dp, notify_only);

	return (DDS_RETCODE_OK);
}

static void sec_permissions_update (uintptr_t user)
{
	Domain_t	*dp;
	unsigned	i;
	DDS_ReturnCode_t error;

	for (i = 1; i <= MAX_DOMAINS; i++) {
		dp = domain_get (i, 1, &error);
		if (!dp || !dp->security || !dp->participant.p_permissions) {
			if (dp)
				lock_release (dp->lock);
			continue;
		}
		sec_revalidate_permissions (dp, user);
		lock_release (dp->lock);
	}
}

static Timer_t sec_perm_timer;

void DDS_Security_permissions_changed (void)
{
	log_printf (SEC_ID, 0, "DDS_Security_permissions_changed()\r\n");
	if (domains_used ())
		tmr_start (&sec_perm_timer, 0, 0, sec_permissions_update);
}

void DDS_Security_permissions_notify (void)
{
	if (domains_used ())
		tmr_start (&sec_perm_timer, 0, 1, sec_permissions_update);
}

/* sec_known_permissions_tokens -- Verify Permissions Tokens.  We need at least
				   one of the received tokens to match an
				   existing plugin. If not found, we can't
				   validate the message properly. */

Token_t *sec_known_permissions_tokens (Token_t *perm_list, unsigned caps)
{
	Token_t			*ptoken = NULL, *list;
	const SEC_PERM		*pp;
	unsigned		i;

	sec_log_fct ("known_permissions_tokens");
	if (!perm_list) {
		sec_log_ret ("%p", NULL);
		return (NULL);
	}
	for (list = perm_list, i = 0; i < nperm_plugins; i++) {
		pp = perm_plugins [i];
		if ((pp->capabilities & caps) == 0)
			continue;

		for (ptoken = list; ptoken; ptoken = ptoken->next)
			if (!strcmp (pp->ptoken_class, ptoken->data->class_id))
				break;

		if (ptoken)
			break;
	}
	sec_log_ret ("%p", (void *) ptoken);
	return (ptoken);
}

static sec_revoke_listener_fct on_revoke_permissions = NULL;

DDS_ReturnCode_t sec_check_create_participant (
				Permissions_t                  perm,
				const Property_t               *properties,
				const DDS_DomainParticipantQos *qos,
				unsigned                       *secure)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_create_participant");
	if (!plugin_policy) {
		*secure = 0;
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.data = (void *) qos;
	data.rdata = (void *) properties;
	ret = sec_access_control_request (DDS_CHECK_CREATE_PARTICIPANT, &data);
	if (!ret)
		*secure = data.secure;
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_create_writer (Permissions_t           perm,
					  const char              *topic_name,
					  const Property_t        *properties,
					  const DDS_DataWriterQos *qos,
					  const Strings_t         *partitions,
					  const char              *tag,
					  unsigned                *secure)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_create_writer");
	if (!plugin_policy) {
		*secure = 0;
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.name = topic_name;
	data.rdata = (void *) properties;
	data.data = (void *) qos;
	data.strings = partitions;
	data.tag = tag;
	data.secure = 0;
	ret = sec_access_control_request (DDS_CHECK_CREATE_WRITER, &data);
	if (!ret)
		*secure = data.secure;
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_create_reader (Permissions_t           perm,
					  const char              *topic_name,
					  const Property_t        *properties,
					  const DDS_DataReaderQos *qos,
					  const Strings_t         *partitions,
					  const char              *tag,
					  unsigned                *secure)
{

	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_create_reader");
	if (!plugin_policy) {
		*secure = 0;
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.name = topic_name;
	data.rdata = (void *) properties;
	data.data = (void *) qos;
	data.strings = partitions;
	data.tag = tag;
	data.secure = 0;
	ret = sec_access_control_request (DDS_CHECK_CREATE_READER, &data);
	if (!ret)
		*secure = data.secure;
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_create_topic (Permissions_t       perm,
					 const char          *topic_name,
					 const Property_t    *properties,
					 const DDS_TopicQos  *qos)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_create_topic");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.name = topic_name;
	data.rdata = (void *) properties;
	data.data = (void *) qos;
	ret = sec_access_control_request (DDS_CHECK_CREATE_TOPIC, &data);
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_local_register_instance (Permissions_t       perm,
				 	            Writer_t            *writer,
						    const unsigned char *key)
{
	ARG_NOT_USED (perm)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (key)

	sec_log_fct ("check_local_register_instance");

	/* ... TBC ... */

	sec_log_ret ("%d", 0);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sec_check_local_dispose_instance (Permissions_t       perm,
						   Writer_t            *writer,
						   const unsigned char *key)
{
	ARG_NOT_USED (perm)
	ARG_NOT_USED (writer)
	ARG_NOT_USED (key)

	sec_log_fct ("check_local_dispose_instance");

	/* ... TBC ... */

	sec_log_ret ("%d", 0);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sec_check_remote_participant (Permissions_t perm,
					       String_t      *udata)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_remote_participant");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.data = (void *) udata;
	ret = sec_access_control_request (DDS_CHECK_REMOTE_PARTICIPANT, &data);
	sec_log_ret ("%d", 0);
	return (ret);
}

DDS_ReturnCode_t sec_check_remote_datawriter (Permissions_t             perm,
					      const char                *topic,
					      const DiscoveredWriterQos *qos,
					      const char                *tag)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_remote_participant");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.data = (void *) qos;
	data.name = topic;
	data.tag = tag;
	ret = sec_access_control_request (DDS_CHECK_REMOTE_WRITER, &data);
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_remote_datareader (Permissions_t             perm,
					      const char                *topic,
					      const DiscoveredReaderQos *qos,
					      const char                *tag)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_remote_datareader");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.data = (void *) qos;
	data.name = topic;
	data.tag = tag;
	ret = sec_access_control_request (DDS_CHECK_REMOTE_READER, &data);
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_remote_topic (Permissions_t            perm,
					 const char               *topic,
					 const DiscoveredTopicQos *qos)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_remote_topic");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = perm;
	data.data = (void *) qos;
	data.name = topic;
	ret = sec_access_control_request (DDS_CHECK_REMOTE_TOPIC, &data);
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_local_writer_match (Permissions_t    lperm,
					       Permissions_t    rperm,
					       const Writer_t   *writer,
					       const Endpoint_t *r)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_local_datawriter_match");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = lperm;
	data.domain_id = rperm;
	data.name = str_ptr (writer->w_topic->name);
	data.tag = str_ptr (r->topic->name);
	data.data = (void *) writer->w_qos->qos.user_data;
	data.rdata = (void *) r->qos->qos.user_data;
	ret = sec_access_control_request (DDS_CHECK_LOCAL_WRITER_MATCH, &data);
	sec_log_ret ("%d", ret);
	return (ret);
}

DDS_ReturnCode_t sec_check_local_reader_match (Permissions_t    lperm,
					       Permissions_t    rperm,
					       const Reader_t   *reader,
					       const Endpoint_t *w)
{
	DDS_SecurityReqData	data;
	DDS_ReturnCode_t	ret;

	sec_log_fct ("check_local_datareader_match");
	if (!plugin_policy) {
		sec_log_ret ("%d", 0);
		return (DDS_RETCODE_OK);
	}
	data.handle = lperm;
	data.domain_id = rperm;
	data.name = str_ptr (reader->r_topic->name);
	data.tag = str_ptr (w->topic->name);
	data.data = (void *) reader->r_qos->qos.user_data;
	data.rdata = (void *) w->qos->qos.user_data;
	ret = sec_access_control_request (DDS_CHECK_LOCAL_READER_MATCH, &data);
	return (ret);
}

DDS_ReturnCode_t sec_check_remote_register_instance (Permissions_t            perm,
						     const Reader_t           *reader,
						     const DiscoveredWriter_t *dw,
						     const unsigned char      *key)
{
	ARG_NOT_USED (perm)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (dw)
	ARG_NOT_USED (key)

	sec_log_fct ("check_remote_register_instance");

	/* ... TBC ... */

	sec_log_ret ("%d", 0);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t sec_check_remote_dispose_instance (Permissions_t            perm,
						    const Reader_t           *reader,
						    const DiscoveredWriter_t *dw,
						    const unsigned char      *key)
{
	ARG_NOT_USED (perm)
	ARG_NOT_USED (reader)
	ARG_NOT_USED (dw)
	ARG_NOT_USED (key)

	sec_log_fct ("check_remote_dispose_instance");

	/* ... TBC ... */

	sec_log_ret ("%d", 0);
	return (DDS_RETCODE_OK);
}

Token_t *sec_get_permissions_tokens (Permissions_t perm, unsigned caps)
{
	Token_t			*token, *list = NULL;
	PermissionsData_t	*pp;
	const SEC_PERM		*plugin;
	unsigned		i;

	sec_log_fct ("get_permissions_tokens");
	pp = perm_lookup (perm, NULL);
	if (!pp) {
		sec_log_ret ("%p", NULL);
		return (NULL);
	}
	for (i = 0; i < MAX_P_PLUGINS; i++) {
		if ((plugin = pp->plugins [i]) == NULL)
			break;

		if (!plugin->get_perm_token ||
		    (plugin->capabilities & caps) == 0)
			continue;

		token = (*plugin->get_perm_token) (plugin, perm);
		if (token) {
			token->next = list;
			list = token;
		}
	}
	if (!list) {
		sec_log_ret ("%p", NULL);
		return (NULL);
	}
	sec_log_ret ("%p", (void *) pp->perm_tokens);
	return (list);
}

Permissions_t sec_validate_remote_permissions (Identity_t                local,
					       Identity_t                rem,
					       unsigned                  caps,
					       DDS_PermissionsToken      *token,
					       DDS_PermissionsCredential *cred,
					       DDS_ReturnCode_t          *error)
{
	Permissions_t	perm;
	const SEC_PERM	*plugin;
	unsigned	i;

	sec_log_fct ("validate_remote_permissions");
	for (i = 0; i < nperm_plugins; i++)
		if ((plugin = perm_plugins [i]) != NULL &&
		    (plugin->capabilities & caps) != 0 &&
		    plugin->valid_rem_perm &&
		    ((!cred && !plugin->perm_class) ||
		     (cred && plugin->perm_class &&
		      !strcmp (plugin->perm_class, cred->class_id))) &&
		    ((!token && !plugin->ptoken_class) ||
		     (token && plugin->ptoken_class &&
		      !strcmp (plugin->ptoken_class, token->class_id)))) {
			perm = ((*plugin->valid_rem_perm) (plugin, local, rem,
							   token, cred));
			if (perm) {
				*error = DDS_RETCODE_OK;
				sec_log_ret ("0x%x", perm);
				return (perm);
			}
		}

	*error = DDS_RETCODE_ALREADY_DELETED;
	sec_log_ret ("%d", 0);
	return (0);
}

void sec_release_permissions (Permissions_t perm)
{
	PermissionsData_t	*pdp;
	DDS_SecurityReqData	data;

	sec_log_fct ("release_permissions");
	pdp = perm_lookup (perm, NULL);
	if (pdp) {
		if (pdp->nusers == 1) {
			data.handle = pdp->perm;
			sec_access_control_request (DDS_RELEASE_PERM, &data);
		}
		perm_unref (&pdp);
	}
	sec_log_retv ();
}
 
DDS_ReturnCode_t sec_set_revoke_listener (sec_revoke_listener_fct fct)
{
	sec_log_fct ("set_revoke_listener");
	if (!fct) {
		sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	on_revoke_permissions = fct;
	sec_log_ret ("%d", DDS_RETCODE_OK);
	return (DDS_RETCODE_OK);
}
