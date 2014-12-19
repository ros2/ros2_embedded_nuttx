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


#include "sp_access_populate.h"
#include "sp_data.h"
#include "sp_access.h"
#include "sp_xml.h"

/* Domain functions */

DomainHandle_t DDS_SP_add_domain (void)
{
	DomainHandle_t handle;

	if (sp_access_add_domain (&handle))
		return (handle);

	return (0);
}

DDS_ReturnCode_t DDS_SP_remove_domain (DomainHandle_t domain_handle)
{
	MSDomain_t       *d;
	unsigned         i, ntopics, npartitions;
	DDS_ReturnCode_t ret;

	/* log_printf (SEC_ID, 0, "MSP: Remove domain\r\n"); */

	if (!(d = sp_access_get_domain (domain_handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	ntopics = d->ntopics;
	for (i = 0; i < ntopics; i++)
		if ((ret = DDS_SP_remove_topic (0, domain_handle,
						LIST_HEAD (d->topics)->index)))
			return (ret);

	npartitions = d->npartitions;
	for (i = 0; i < npartitions; i++)
		if ((ret = DDS_SP_remove_partition (0, domain_handle, 
						    LIST_HEAD (d->partitions)->index)))
			return (ret);

	return (sp_access_remove_domain (domain_handle));
}

DDS_ReturnCode_t DDS_SP_set_domain_access (DomainHandle_t domain_handle,
					   DDS_DomainId_t domain_id,
					   MSAccess_t     access,
					   int            exclusive,
					   int            controlled,
					   int            msg_encrypt,
					   uint32_t       transport,
					   int            blacklist)
{
	MSDomain_t     *d;
	
	/* log_printf (SEC_ID, 0, "MSP: Set domain access\r\n"); */

	if (!(d = sp_access_get_domain (domain_handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	d->domain_id = domain_id;
	d->access = access;
	d->exclusive = exclusive;
	d->controlled = controlled;
	d->msg_encrypt = msg_encrypt;
	d->transport = transport;
	d->blacklist = blacklist;
	d->refreshed = 1;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_domain_access (DomainHandle_t domain_handle,
					   DDS_DomainId_t *domain_id,
					   MSAccess_t     *access,
					   int            *exclusive,
					   int            *controlled,
					   int            *msg_encrypt,
					   uint32_t       *transport,
					   int            *blacklist)
{
	MSDomain_t *d;

	if (!(d = sp_access_get_domain (domain_handle)))
	    return (DDS_RETCODE_BAD_PARAMETER);

	*domain_id = d->domain_id;
	*access = d->access;
	*exclusive = d->exclusive;
	*controlled = d->controlled;
	*msg_encrypt = d->msg_encrypt;
	*transport = d->transport;
	*blacklist = d->blacklist;

	return (DDS_RETCODE_OK);
}

int DDS_SP_get_domain_handle (DDS_DomainId_t domain_id)
{
	return ((int) sp_access_get_domain_handle (domain_id));
}

/* Participant functions */

ParticipantHandle_t DDS_SP_add_participant (void)
{
	ParticipantHandle_t handle;

	if (sp_access_add_participant (&handle))
		return (handle);

	return (0);
}

DDS_ReturnCode_t DDS_SP_remove_participant (ParticipantHandle_t participant_handle) 
{
	MSParticipant_t  *p;
	unsigned         i, ntopics, npartitions;
	DDS_ReturnCode_t ret;
	
	if (!(p = sp_access_get_participant (participant_handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	ntopics = p->ntopics;
	for (i = 0; i < ntopics; i++) 
		if ((ret = DDS_SP_remove_topic (participant_handle, 0, 
					     LIST_HEAD (p->topics)->topic.index)))
			return (ret);

	npartitions = p->npartitions;
	for (i = 0;  i < npartitions; i++)
		if ((ret = DDS_SP_remove_partition (participant_handle, 0, 
						 LIST_HEAD (p->partitions)->partition.index)))
			return (ret);

	return (sp_access_remove_participant (participant_handle));
}

DDS_ReturnCode_t DDS_SP_set_participant_access (ParticipantHandle_t participant_handle, 
						char                *new_name, 
						MSAccess_t          access,
						int                 blacklist)
{
	MSParticipant_t     *p;
	
	/* log_printf (SEC_ID, 0, "MSP: Set participant access\r\n"); */

	if (!(p = sp_access_get_participant (participant_handle)))
		return (DDS_RETCODE_BAD_PARAMETER);
	
	memcpy (p->name, new_name, strlen (new_name) + 1);
	p->access = access;
	p->blacklist = blacklist;
	p->refreshed = 1;
	p->input_number = input_counter ++;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_participant_access (ParticipantHandle_t participant_handle,
						char                *name, 
						MSAccess_t          *access,
						int                 *blacklist)
{	
	MSParticipant_t *p;

	if (!(p = sp_access_get_participant (participant_handle)))
		return (DDS_RETCODE_BAD_PARAMETER);

	strcpy (name, p->name);
	*access = p->access;
	*blacklist = p->blacklist;

	return (DDS_RETCODE_OK);
}

int DDS_SP_get_participant_handle (char *name)
{
	return (sp_access_get_participant_handle (name));
}

/* Topic functions */

TopicHandle_t DDS_SP_add_topic (ParticipantHandle_t participant_handle, 
				DomainHandle_t      domain_handle)
{

	TopicHandle_t handle;

	if (participant_handle > 0) {
		if (sp_access_add_topic (&handle, participant_handle, LIST_PARTICIPANT))
			return (handle);
	}
	else if (domain_handle > 0) {
		if (sp_access_add_topic (&handle, domain_handle, LIST_DOMAIN))
			return (handle);
	}
		
	return (-1);
}

DDS_ReturnCode_t DDS_SP_remove_topic (ParticipantHandle_t participant_handle,
				      DomainHandle_t      domain_handle,
				      TopicHandle_t       topic_id)
{
	/* log_printf (SEC_ID, 0, "MSP: Remove topic access\r\n"); */

	if (participant_handle > 0) {
		return (sp_access_remove_topic (topic_id, participant_handle, LIST_PARTICIPANT));
	} 
	else if (domain_handle > 0) {
		return (sp_access_remove_topic (topic_id, domain_handle, LIST_DOMAIN));
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_set_topic_access (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  TopicHandle_t       topic_id, 
					  char                *name,
					  MSMode_t            mode,
					  int                 controlled,
					  int                 disc_enc,
					  int                 submsg_enc,
					  int                 payload_enc,
					  DDS_DomainId_t      id,
					  int                 blacklist)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	/* log_printf (SEC_ID, 0, "MSP: Set topic access\r\n"); */
	
	/* We can add an extra check to see if the topic rule already exists */
	/* if (DDS_SP_get_topic_handle (participant_handle, domain_handle, name) != -1)
	   return (DDS_RETCODE_BAD_PARAMETER);
	*/

	if (participant_handle > 0) {
		if (!(tp = (MSUTopic_t *) sp_access_get_topic (topic_id, 
							       participant_handle, 
							       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);

		tp->id = id;
		if (name) {
			if (!(tp->topic.name = malloc (strlen ((char *) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (tp->topic.name, name);
		} else 
			tp->topic.name = NULL;
				
		tp->topic.mode = mode;
		tp->topic.controlled = controlled;
		tp->topic.disc_enc = disc_enc;
		tp->topic.submsg_enc = (submsg_enc) != 0;
		tp->topic.payload_enc = (payload_enc) != 0;
		if (submsg_enc)
			tp->topic.crypto_mode = submsg_enc;
		else if (payload_enc)
			tp->topic.crypto_mode = payload_enc;
		else
			tp->topic.crypto_mode = 0;
		tp->topic.blacklist = blacklist;
		tp->topic.refreshed = 1;
		tp->topic.fine_topic = NULL;
		tp->topic.fine_app_topic = NULL;
		
		/* when a topic for a participant is changed, we must update the participant as refreshed */
		if ((p = sp_access_get_participant (participant_handle)))
		    p->refreshed = 1;
	}
	else if (domain_handle > 0) {
		if (!(dtp = (MSTopic_t *) sp_access_get_topic (topic_id, domain_handle, LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (name) {
			if (!(dtp->name = malloc (strlen ((char *) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (dtp->name, name);
		} else
			dtp->name = NULL;
		
		dtp->mode = mode;
		dtp->controlled = controlled;
		dtp->disc_enc = disc_enc;
		dtp->submsg_enc = (submsg_enc != 0);
		dtp->payload_enc = (payload_enc != 0);
		if (submsg_enc)
			dtp->crypto_mode = submsg_enc;
		else if (payload_enc)
			dtp->crypto_mode = payload_enc;
		else
			dtp->crypto_mode = 0;
		dtp->blacklist = blacklist;
		dtp->refreshed = 1;
		dtp->fine_topic = NULL;
		dtp->fine_app_topic = NULL;

		/* when a topic for a domain is changed, we must update the domain as refreshed */
		if ((d = sp_access_get_domain (domain_handle)))
			d->refreshed = 1;
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_topic_access (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  TopicHandle_t       topic_id, 
					  char                *name,
					  MSMode_t            *mode,
					  int                 *controlled,
					  int                 *disc_enc,
					  int                 *submsg_enc,
					  int                 *payload_enc,
					  DDS_DomainId_t      *id,
					  int                 *blacklist)
{
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	if (participant_handle > 0) {
		
		if (!(tp = (MSUTopic_t *) sp_access_get_topic (topic_id, 
							       participant_handle, 
							       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);

 		*id = tp->id;
		if (tp->topic.name)
			strcpy (name, tp->topic.name);
		else
			strcpy (name, "*");
		*mode = tp->topic.mode;
		*controlled = tp->topic.controlled;
		*disc_enc = tp->topic.disc_enc;
		*submsg_enc = (tp->topic.submsg_enc) ? tp->topic.crypto_mode : 0;
		*payload_enc = (tp->topic.payload_enc) ? tp->topic.crypto_mode : 0;
		*blacklist = tp->topic.blacklist;
	}
	else if (domain_handle > 0) {
		if (!(dtp = (MSTopic_t *) sp_access_get_topic (topic_id, domain_handle, LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (dtp->name)
			strcpy (name, dtp->name);
		else
			strcpy (name, "*");
		*mode = dtp->mode;
		*controlled = dtp->controlled;
		*disc_enc = dtp->disc_enc;
		*submsg_enc = (dtp->submsg_enc) ? dtp->crypto_mode : 0;
		*payload_enc = (dtp->payload_enc) ? dtp->crypto_mode : 0;
		*blacklist = dtp->blacklist;
	}
	return (DDS_RETCODE_OK);
}

int DDS_SP_get_topic_handle (ParticipantHandle_t participant_handle,
			     DomainHandle_t      domain_handle,
			     char                *name,
			     MSMode_t            mode)
{
	/* log_printf (SEC_ID, 0, "MSP: Get topic handle\r\n"); */

	if (participant_handle > 0) {
		return (sp_access_get_topic_handle (name, mode, participant_handle, LIST_PARTICIPANT));
	}
	else if (domain_handle > 0) {
		return (sp_access_get_topic_handle (name, mode, domain_handle, LIST_DOMAIN));
	}
	return (-1);
}

DDS_ReturnCode_t DDS_SP_set_fine_grained_topic (ParticipantHandle_t participant_handle,
						DomainHandle_t      domain_handle,
						TopicHandle_t       handle,
						ParticipantHandle_t read [MAX_ID_HANDLES],
						int                 nb_read,
						ParticipantHandle_t write [MAX_ID_HANDLES],
						int                 nb_write)
{
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;
	int             i;

	if (participant_handle > 0) {
		if (!(tp = (MSUTopic_t *) sp_access_get_topic (handle, 
							       participant_handle, 
							       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		if (!tp->topic.fine_topic)
			if (!(tp->topic.fine_topic = malloc (sizeof (MSFTopic_t))))
				return (DDS_RETCODE_OUT_OF_RESOURCES);

		/* TO BE CHECKED */
		memset (tp->topic.fine_topic->read, 0, sizeof (tp->topic.fine_topic->read));
		memset (tp->topic.fine_topic->write, 0, sizeof (tp->topic.fine_topic->read));
		for (i = 0; i < nb_read; i++)
			tp->topic.fine_topic->read [i] = read [i];
		for (i = 0; i < nb_write; i++)
			tp->topic.fine_topic->write [i] = write [i];
	}
	else if (domain_handle > 0) {
		if (!(dtp = (MSTopic_t *) sp_access_get_topic (handle, domain_handle, LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!dtp->fine_topic)
			if (!(dtp->fine_topic = malloc (sizeof (MSFTopic_t))))
				return (DDS_RETCODE_OUT_OF_RESOURCES);

		/* TO BE CHECKED */
		memset (dtp->fine_topic->read, 0, sizeof (dtp->fine_topic->read));
		memset (dtp->fine_topic->write, 0, sizeof (dtp->fine_topic->write));
		for (i = 0; i < nb_read; i++)
			dtp->fine_topic->read [i] = read [i];
		for (i = 0; i < nb_write; i++)
			dtp->fine_topic->write [i] = write [i];
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_remove_fine_grained_topic (ParticipantHandle_t participant_handle,
						   DomainHandle_t      domain_handle,
						   TopicHandle_t       handle)
{
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	if (participant_handle > 0) {
		if (!(tp = (MSUTopic_t *) sp_access_get_topic (handle, 
							       participant_handle, 
							       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		if (tp->topic.fine_topic)
			free (tp->topic.fine_topic);
		tp->topic.fine_topic = NULL;
	}
	else if (domain_handle > 0) {
		if (!(dtp = (MSTopic_t *) sp_access_get_topic (handle, domain_handle, LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (dtp->fine_topic)
			free (dtp->fine_topic);
		dtp->fine_topic = NULL;
	}
	return (DDS_RETCODE_OK);
}
						    
DDS_ReturnCode_t DDS_SP_set_fine_grained_app_topic (ParticipantHandle_t participant_handle,
						    DomainHandle_t      domain_handle,
						    TopicHandle_t       handle,
						    ParticipantHandle_t read [MAX_ID_HANDLES],
						    int                 nb_read,
						    ParticipantHandle_t write [MAX_ID_HANDLES],
						    int                 nb_write)
{
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;
	int             i;

	if (participant_handle > 0) {
		if (!(tp = (MSUTopic_t *) sp_access_get_topic (handle, 
							       participant_handle, 
							       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);
		if (!tp->topic.fine_app_topic)
			if (!(tp->topic.fine_app_topic = malloc (sizeof (MSFTopic_t))))
				return (DDS_RETCODE_OUT_OF_RESOURCES);

		/* TO BE CHECKED */
		memset (tp->topic.fine_app_topic->read, 0, sizeof (tp->topic.fine_app_topic->read));
		memset (tp->topic.fine_app_topic->write, 0, sizeof (tp->topic.fine_app_topic->write));
		for (i = 0; i < nb_read; i++)
			tp->topic.fine_app_topic->read [i] = read [i];
		for (i = 0; i < nb_write; i++)
			tp->topic.fine_app_topic->write [i] = write [i];
	}
	else if (domain_handle > 0) {
		if (!(dtp = (MSTopic_t *) sp_access_get_topic (handle, domain_handle, LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (!dtp->fine_app_topic)
			if (!(dtp->fine_app_topic = malloc (sizeof (MSFTopic_t))))
				return (DDS_RETCODE_OUT_OF_RESOURCES);

		/* TO BE CHECKED */
		memset (dtp->fine_app_topic->read, 0, sizeof (dtp->fine_app_topic->read));
		memset (dtp->fine_app_topic->write, 0, sizeof (dtp->fine_app_topic->write));
		for (i = 0; i < nb_read; i++)
			dtp->fine_app_topic->read [i] = read [i];
		for (i = 0; i < nb_write; i++)
			dtp->fine_app_topic->write [i] = write [i];
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_remove_fine_grained_app_topic (ParticipantHandle_t participant_handle,
						       DomainHandle_t      domain_handle,
						       TopicHandle_t       handle)
{
	MSUTopic_t	*tp = NULL;
	MSTopic_t       *dtp = NULL;

	if (participant_handle > 0) {
		if (!(tp = (MSUTopic_t *) sp_access_get_topic (handle, 
							       participant_handle, 
							       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);
		
		if (tp->topic.fine_app_topic)
			free (tp->topic.fine_app_topic);
		tp->topic.fine_app_topic = NULL;
	}
	else if (domain_handle > 0) {
		if (!(dtp = (MSTopic_t *) sp_access_get_topic (handle, domain_handle, LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (dtp->fine_app_topic)
			free (dtp->fine_app_topic);
		dtp->fine_app_topic = NULL;
	}
	return (DDS_RETCODE_OK);
}

/* Partition functions */

PartitionHandle_t DDS_SP_add_partition (ParticipantHandle_t participant_handle, 
					DomainHandle_t      domain_handle)
{
	PartitionHandle_t handle;
	/* log_printf (SEC_ID, 0, "MSP: Add default 'allow all' policy for partition\r\n"); */

	if (participant_handle > 0) {
		if (sp_access_add_partition (&handle, participant_handle, LIST_PARTICIPANT))
			return (handle);
	}
	else if (domain_handle > 0) {
		if (sp_access_add_partition (&handle, domain_handle, LIST_DOMAIN))
			return (handle);
	}
	return (-1);
}

DDS_ReturnCode_t DDS_SP_remove_partition (ParticipantHandle_t participant_handle,
					  DomainHandle_t      domain_handle,
					  PartitionHandle_t   partition_id)
{
	/* log_printf (SEC_ID, 0, "MSP: Remove partition access\r\n"); */

	if (participant_handle > 0) {
		return (sp_access_remove_partition (partition_id, participant_handle, LIST_PARTICIPANT));
	} 

	else if (domain_handle > 0) {
		return (sp_access_remove_partition (partition_id, domain_handle, LIST_DOMAIN));
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_set_partition_access (ParticipantHandle_t participant_handle,
					      DomainHandle_t      domain_handle,
					      PartitionHandle_t   partition_id,
					      char                *name,
					      MSMode_t            mode,
					      DDS_DomainId_t      id,
					      int                 blacklist)
{
	MSParticipant_t	*p = NULL;
	MSDomain_t	*d = NULL;
	MSUPartition_t	*pp = NULL;
	MSPartition_t	*dp = NULL;

	if (participant_handle > 0) {
		if (!(pp = (MSUPartition_t *) sp_access_get_partition (partition_id, 
								       participant_handle, 
								       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);

		pp->id = id;
		
		if (name) {
			if (!(pp->partition.name = malloc (strlen ((char *) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (pp->partition.name, name);
		} else
			pp->partition.name = NULL;

		pp->partition.mode = mode;
		pp->partition.blacklist = blacklist;
		pp->partition.refreshed = 1;

		/* refresh participant as well */
		if ((p = sp_access_get_participant (participant_handle)))
			p->refreshed = 1;

	}
	else if (domain_handle > 0) {
		if (!(dp = (MSPartition_t *) sp_access_get_partition (partition_id, 
								       domain_handle, 
								       LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (name) {
			if (!(dp->name = malloc (strlen ((char*) name) + 1)))
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			strcpy (dp->name, name);
		} else
			dp->name = NULL;

		dp->mode = mode;
		dp->blacklist = blacklist;
		dp->refreshed = 1;

		if ((d = sp_access_get_domain (domain_handle)))
			d->refreshed = 1;
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_SP_get_partition_access (ParticipantHandle_t participant_handle,
					      DomainHandle_t      domain_handle,
					      PartitionHandle_t   partition_id,
					      char                *name,
					      MSMode_t            *mode,
					      DDS_DomainId_t      *id,
					      int                 *blacklist)
{
	MSUPartition_t	*pp = NULL;
	MSPartition_t	*dpp = NULL;

	if (participant_handle > 0) {
		if (!(pp = (MSUPartition_t *) sp_access_get_partition (partition_id, 
								       participant_handle, 
								       LIST_PARTICIPANT)))
			return (DDS_RETCODE_BAD_PARAMETER);

		*id = pp->id;
		if (pp->partition.name)
			strcpy (name, pp->partition.name);
		else
			strcpy (name, "*");
		*mode = pp->partition.mode;
		*blacklist = pp->partition.blacklist;
	}
	else if (domain_handle > 0) {
		if (!(dpp = (MSPartition_t *) sp_access_get_partition (partition_id, 
								       domain_handle, 
								       LIST_DOMAIN)))
			return (DDS_RETCODE_BAD_PARAMETER);

		if (dpp->name)
			strcpy (name, dpp->name);
		else
			strcpy (name, "*");
		*mode = dpp->mode;
		*blacklist = dpp->blacklist;
	}
	return (DDS_RETCODE_OK);
}

int DDS_SP_get_partition_handle (ParticipantHandle_t participant_handle,
				 DomainHandle_t      domain_handle,
				 char                *name,
				 MSMode_t            mode)
{
	if (name)
		if (!strcmp (name, "*"))
			name = NULL;

	if (participant_handle > 0) {
		return (sp_access_get_partition_handle (name, mode, participant_handle, LIST_PARTICIPANT));
	}
	else if (domain_handle > 0) {
		return (sp_access_get_partition_handle (name, mode, domain_handle, LIST_DOMAIN));
	}
	return (-1);
}

/* DB functions */

/* Start every db update with this function */
DDS_ReturnCode_t DDS_SP_update_start (void)
{
	return (sp_access_update_start ());
}

/* This should be called when every change to the database is done */
DDS_ReturnCode_t DDS_SP_update_done (void)
{
	return (sp_access_update_done ());
}

DDS_ReturnCode_t DDS_SP_access_db_cleanup (void)
{
	return (sp_access_db_cleanup ());
}

#ifdef MSECPLUG_WITH_SECXML
DDS_ReturnCode_t DDS_SP_parse_xml (char *path)
{
	parse_xml (path);
	return (DDS_RETCODE_OK);
}
#endif

void DDS_SP_set_extra_authentication_check (sp_extra_authentication_check_fct f)
{
	sp_set_extra_authentication_check (f);
}

