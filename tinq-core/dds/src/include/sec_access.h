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

/* sec_access.h -- DDS Security - Access Control plugin definitions. */

#ifndef __sec_access_h_
#define __sec_access_h_

#include "dds/dds_security.h"
#include "dds_data.h"
#include "sec_data.h"
#include "guid.h"

int sec_perm_init (void);

/* Initialize the access control plugin code. */

Permissions_t sec_validate_local_permissions (Identity_t                id,
					      DDS_DomainId_t            domain_id,
					      DDS_PermissionsCredential *creds,
					      DDS_ReturnCode_t          *error);

/* Validate local permissions based on the given permissions credentials and 
   return a corresponding permissions handle if successful.  If an error
   occurs, a 0 handle is returned and *error will be set an error code. */

void dds_sec_local_perm_update (int take_lock);

/* Call this when your local permissions have changed in the policy database,
   this will update only the local permissions. take_lock will tell the function
   to either take the lock or not take the lock if it is already taken */

Token_t *sec_known_permissions_tokens (Token_t *list, unsigned caps);

/* Check whether a Permissions Token can be handled by an access control
   plugin.  If so, the first valid token will be taken from the list and
   returned. */

Permissions_t sec_validate_remote_permissions (Identity_t                local,
					       Identity_t                rem,
					       unsigned                  caps,
					       DDS_PermissionsToken      *token,
					       DDS_PermissionsCredential *cred,
					       DDS_ReturnCode_t          *error);

/* Validate the remote permissions, based on the identity of the peer and the
   Permission Token/Credential. */

DDS_ReturnCode_t sec_check_create_participant (Permissions_t                  perm,
					       const Property_t               *properties,
					       const DDS_DomainParticipantQos *qos,
					       unsigned                       *secure);

DDS_ReturnCode_t sec_check_create_writer (Permissions_t           perm,
					  const char              *topic_name,
					  const Property_t        *properties,
					  const DDS_DataWriterQos *qos,
				          const Strings_t         *partitions,
					  const char              *tag,
					  unsigned                *secure);

DDS_ReturnCode_t sec_check_create_reader (Permissions_t           perm,
					  const char              *topic_name,
					  const Property_t        *properties,
					  const DDS_DataReaderQos *qos,
				          const Strings_t         *partitions,
					  const char              *tag,
					  unsigned                *secure);

DDS_ReturnCode_t sec_check_create_topic (Permissions_t       perm,
					 const char          *topic_name,
					 const Property_t    *properties,
					 const DDS_TopicQos  *qos);

DDS_ReturnCode_t sec_check_local_register_instance (Permissions_t       perm,
						    Writer_t            *writer,
						    const unsigned char *key);

DDS_ReturnCode_t sec_check_local_dispose_instance (Permissions_t       perm,
						   Writer_t            *writer,
						   const unsigned char *key);

DDS_ReturnCode_t sec_check_remote_participant (Permissions_t perm,
					       String_t      *data);

DDS_ReturnCode_t sec_check_remote_datawriter (Permissions_t             perm,
					      const char                *topic,
					      const DiscoveredWriterQos *qos,
					      const char                *tag);

DDS_ReturnCode_t sec_check_remote_datareader (Permissions_t             perm,
					      const char                *topic,
					      const DiscoveredReaderQos *qos,
					      const char                *tag);

DDS_ReturnCode_t sec_check_remote_topic (Permissions_t            perm,
					 const char               *topic,
					 const DiscoveredTopicQos *qos);

DDS_ReturnCode_t sec_check_local_writer_match (Permissions_t    lperm,
					       Permissions_t    rperm,
					       const Writer_t   *writer,
					       const Endpoint_t *r);

DDS_ReturnCode_t sec_check_local_reader_match (Permissions_t    lperm,
					       Permissions_t    rperm,
					       const Reader_t   *reader,
					       const Endpoint_t *w);

DDS_ReturnCode_t sec_check_remote_register_instance (Permissions_t            perm,
						     const Reader_t           *reader,
						     const DiscoveredWriter_t *dw,
						     const unsigned char      *key);

DDS_ReturnCode_t sec_check_remote_dispose_instance (Permissions_t            perm,
						    const Reader_t           *reader,
						    const DiscoveredWriter_t *dw,
						    const unsigned char      *key);

void sec_release_permissions (Permissions_t perm);

Token_t *sec_get_permissions_tokens (Permissions_t perm, unsigned caps);

DDS_ReturnCode_t sec_release_perm_tokens (DDS_PermissionsToken *tokens);

typedef DDS_ReturnCode_t (*sec_revoke_listener_fct) (Permissions_t perm);

DDS_ReturnCode_t sec_set_revoke_listener (sec_revoke_listener_fct fct);

#endif /* !__sec_access_h_ */

