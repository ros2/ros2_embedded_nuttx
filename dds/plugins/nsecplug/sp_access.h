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

/* sp_access.h -- DDS Security Plugin - Access control plugin definitions. */

#ifndef __sp_access_h_
#define __sp_access_h_

#include "strseq.h"
#include "uqos.h"
#include "dlist.h"
#include "sp_access_db.h"
#include "nsecplug/nsecplug.h"
#include <sys/socket.h>

void sp_access_set_md5 (unsigned char       *dst,
			const unsigned char *identity, 
			size_t              length);

DDS_ReturnCode_t sp_access_validate_local_id (const char          *name,
					      size_t              length,
					      const unsigned char *key,
					      size_t              klength,
					      IdentityHandle_t    *handle);

DDS_ReturnCode_t sp_access_validate_remote_id (const char          *method,
					       const unsigned char *identity,
					       size_t              length,
					       const unsigned char *key,
					       size_t              klength,
					       int                 *validation,
					       IdentityHandle_t    *id);

DDS_ReturnCode_t sp_access_add_common_name (IdentityHandle_t remote,
					    unsigned char    *cert,
					    size_t           cert_len);

/* Local checks */

DDS_ReturnCode_t sp_validate_local_perm (DDS_DomainId_t   domain_id,
					 IdentityHandle_t *handle);

DDS_ReturnCode_t sp_validate_peer_perm (IdentityHandle_t    local,
					IdentityHandle_t    remote,
					const char          *classid,
					unsigned char       *permissions,
					size_t              length,
					PermissionsHandle_t *handle);

DDS_ReturnCode_t sp_check_create_participant (PermissionsHandle_t            perm,
					      const DDS_DomainParticipantQos *qos,
					      unsigned                       *secure);

DDS_ReturnCode_t sp_check_create_writer (PermissionsHandle_t     perm,
					 const char              *topic_name,
					 const DDS_DataWriterQos *qos,
					 const Strings_t         *partitions,
					 unsigned                *secure);

DDS_ReturnCode_t sp_check_create_reader (PermissionsHandle_t     perm,
					 const char              *topic_name,
					 const DDS_DataWriterQos *qos,
					 const Strings_t         *partitions,
					 unsigned                *secure);

DDS_ReturnCode_t sp_check_create_topic (PermissionsHandle_t perm,
					const char          *topic_name,
					const DDS_TopicQos  *qos);

#if 0
DDS_ReturnCode_t sp_check_local_register_instance (PermissionsHandle_t perm,
						   DDS_DataWriter      *writer,
						   const char          *key);

DDS_ReturnCode_t sp_check_local_dispose_instance (PermissionsHandle_t perm,
						  DDS_DataWriter      *writer,
						  const char          *key);
#endif

/* Remote checks */

DDS_ReturnCode_t sp_check_peer_participant (PermissionsHandle_t perm,
					    String_t            *user_data);

DDS_ReturnCode_t sp_check_peer_writer (PermissionsHandle_t      perm,
				       const char               *topic_name,
				       const DiscoveredTopicQos *qos);

DDS_ReturnCode_t sp_check_peer_reader (PermissionsHandle_t      perm,
				       const char               *topic_name,
				       const DiscoveredTopicQos *qos);

DDS_ReturnCode_t sp_check_peer_topic (PermissionsHandle_t      perm,
				      const char               *topic_name,
				      const DiscoveredTopicQos *qos);

#if 0
DDS_ReturnCode_t sp_check_remote_register_instance (PermissionsHandle_t perm,
						    DDS_DataReader      *reader,
						    InstanceHandle_t    pub_handle,
						    const char          *key,
						    InstanceHandle_t    handle);

DDS_ReturnCode_t sp_check_remote_dispose_instance (PermissionsHandle_t  perm,
						   DDS_DataReader       *reader,
						   InstanceHandle_t     pub_handle,
						   const char           *key);
#endif


DDS_ReturnCode_t sp_access_get_permissions (PermissionsHandle_t handle,
					    unsigned char       *data,
					    size_t              max,
					    size_t              *length,
					    uint64_t            *version);

DDS_ReturnCode_t sp_access_set_permissions (PermissionsHandle_t handle,
					    unsigned char       *data,
					    size_t              length);

DDS_ReturnCode_t sp_access_check_local_datawriter_match (PermissionsHandle_t local_w,
							 PermissionsHandle_t remote_r,
							 const char          *tag_w,
							 const char          *tag_r,
							 String_t            *w_userdata,
							 String_t            *r_userdata);

DDS_ReturnCode_t sp_access_check_local_datareader_match (PermissionsHandle_t local_r,
							 PermissionsHandle_t remote_w,
							 const char          *tag_r,
							 const char          *tag_w,
							 String_t            *r_userdata,
							 String_t            *w_userdata);

DDS_ReturnCode_t sp_get_domain_sec_caps (DDS_DomainId_t domain_id,
					 unsigned       *sec_caps);

void sp_set_extra_authentication_check (sp_extra_authentication_check_fct f);

DDS_ReturnCode_t sp_validate_ssl_connection (PermissionsHandle_t perm,
					     SSL                 *ssl,
					     struct sockaddr     *sp,
					     int                 *action);

#endif /* !__sp_access_h_ */

