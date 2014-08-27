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

/* dcps_entity.c -- Implements the DCPS Entity methods. */

#include "sys.h"
#include "log.h"
#include "ctrace.h"
#include "prof.h"
#include "str.h"
#include "dds/dds_dcps.h"
#include "dds_data.h"
#include "dds.h"
#include "dcps_priv.h"
#include "dcps_entity.h"

DDS_StatusMask DDS_Entity_get_status_changes (DDS_Entity e)
{
	Entity_t	*ep = (Entity_t *) e;
	DDS_StatusMask	m;

	ctrc_printd (DCPS_ID, DCPS_E_G_SCH, &e, sizeof (e));
	if (!ep || (ep->flags & EF_LOCAL) == 0)
		return (0);

	switch (ep->type) {
		case ET_PARTICIPANT:
			m = DDS_DomainParticipant_get_status_changes ((DDS_DomainParticipant) e);
			break;
		case ET_TOPIC:
			m = DDS_Topic_get_status_changes ((DDS_Topic) e);
			break;
		case ET_PUBLISHER:
			m = DDS_Publisher_get_status_changes ((DDS_Publisher) e);
			break;
		case ET_WRITER:
			m = DDS_DataWriter_get_status_changes ((DDS_DataWriter) e);
			break;
		case ET_SUBSCRIBER:
			m = DDS_Subscriber_get_status_changes ((DDS_Subscriber) e);
			break;
		case ET_READER:
			m = DDS_DataReader_get_status_changes ((DDS_DataReader) e);
			break;
		default:
			m = 0;
			break;
	}
	return (m);
}

DDS_ReturnCode_t DDS_Entity_enable (DDS_Entity e)
{
	Entity_t	*ep = (Entity_t *) e;

	ctrc_printd (DCPS_ID, DCPS_E_ENABLE, &e, sizeof (e));
	if (!ep || (ep->flags & EF_LOCAL) == 0)
		return (DDS_RETCODE_BAD_PARAMETER);

	switch (ep->type) {
		case ET_PARTICIPANT:
			DDS_DomainParticipant_enable ((DDS_DomainParticipant) e);
			break;
		case ET_TOPIC:
			DDS_Topic_enable ((DDS_Topic) e);
			break;
		case ET_PUBLISHER:
			DDS_Publisher_enable ((DDS_Publisher) e);
			break;
		case ET_SUBSCRIBER:
			DDS_Subscriber_enable ((DDS_Subscriber) e);
			break;
		case ET_WRITER:
			DDS_DataWriter_enable ((DDS_DataWriter) e);
			break;
		case ET_READER:
			DDS_DataReader_enable ((DDS_DataReader) e);
			break;
		default:
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

DDS_StatusCondition DDS_Entity_get_statuscondition (DDS_Entity e)
{
	Entity_t		*ep = (Entity_t *) e;
	DDS_StatusCondition	sc;

	ctrc_printd (DCPS_ID, DCPS_E_G_SCOND, &e, sizeof (e));
	if (!ep || (ep->flags & EF_LOCAL) == 0)
		return (0);

	switch (ep->type) {
		case ET_PARTICIPANT:
			sc = DDS_DomainParticipant_get_statuscondition ((DDS_DomainParticipant) e);
			break;
		case ET_TOPIC:
			sc = DDS_Topic_get_statuscondition ((DDS_Topic) e);
			break;
		case ET_PUBLISHER:
			sc = DDS_Publisher_get_statuscondition ((DDS_Publisher) e);
			break;
		case ET_SUBSCRIBER:
			sc = DDS_Subscriber_get_statuscondition ((DDS_Subscriber) e);
			break;
		case ET_WRITER:
			sc = DDS_DataWriter_get_statuscondition ((DDS_DataWriter) e);
			break;
		case ET_READER:
			sc = DDS_DataReader_get_statuscondition ((DDS_DataReader) e);
			break;
		default:
			sc = NULL;
			break;
	}
	return (sc);
}

DDS_InstanceHandle_t DDS_Entity_get_instance_handle (DDS_Entity e)
{
	Entity_t	*ep = (Entity_t *) e;

	ctrc_printd (DCPS_ID, DCPS_E_G_HANDLE, &e, sizeof (e));
	if (!ep)
		return (0);
	else
		return (ep->handle);
}

