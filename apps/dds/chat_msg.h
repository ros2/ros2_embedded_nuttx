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

/* chat_msg.h -- Message type as used in the chat application. */

#ifndef __chat_msg_h_
#define	__chat_msg_h_

#include "dds/dds_dcps.h"
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"

typedef struct chat_msg_st {
	char	*chatroom;
	char	*from;
	char	*message;
} ChatMsg_t;


DDS_DynamicTypeSupport ChatMsg_type_new (void);

/* Create a new ChatMsg_t type.  If errors occurred, a non-0 error code is
   returned, otherwise *tp and *tsp will be set to the proper DynamicType and
   TypeSupport data. */

void ChatMsg_type_free (DDS_DynamicTypeSupport ts);

/* Release the previously created TypeSupport data. */

DDS_InstanceHandle_t ChatMsg_register (DDS_DynamicDataWriter  dw,
				       ChatMsg_t              *data);
/* Write a chat message on the dynamic type writer. */

DDS_ReturnCode_t ChatMsg_write (DDS_DynamicDataWriter dw,
				ChatMsg_t             *data,
				DDS_InstanceHandle_t  h);

/* Write a chat message on the dynamic type writer. */

DDS_ReturnCode_t ChatMsg_signal (DDS_DynamicDataWriter dw,
				 DDS_InstanceHandle_t  h,
				 int                   unreg);

/* Indicate a chat signal on the dynamic type writer.
   If unreg is set, the signal indicates that the writer has finished
   writing.  Otherwise it indicates the writer is cleaning up its data. */

DDS_ReturnCode_t ChatMsg_read_or_take (DDS_DynamicDataReader dr,
				       ChatMsg_t             *data,
				       DDS_SampleStateMask   ss,
				       DDS_ViewStateMask     vs,
				       DDS_InstanceStateMask is,
				       int                   take,
				       int                   *valid,
				       DDS_InstanceStateKind *kind);

/* Dynamically read or take a ChatMsg_t data item. */

void ChatMsg_cleanup (ChatMsg_t *data);

/* Cleanup dynamic message data. */

#endif /* __chat_msg_h_ */

