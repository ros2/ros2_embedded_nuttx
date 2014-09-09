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

/* chat_msg.h -- Chat message type handling. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libx.h"
#include "chat_msg.h"

#define	USE_MUTABLE

#ifdef USE_MUTABLE
#define	M_0	"chatbox"
#define	M_1	"from"
#define	M_2	"message"
#define CBOX_ID	43995547
#define FROM_ID	237207046
#define MSG_ID	60019451
#else
#if !defined (ALT1) && !defined (ALT2)
#define M_0	"chatbox"
#define M_1	"from"
#define M_2	"message"
#define CBOX_ID	0
#define FROM_ID	1
#define MSG_ID	2
#elif defined (ALT1)
#define M_0	"chatbox"
#define M_2	"from"
#define M_1	"message"
#define CBOX_ID	0
#define FROM_ID	2
#define MSG_ID	1
#else
#define M_1	"chatbox"
#define M_2	"from"
#define M_0	"message"
#define CBOX_ID	1
#define FROM_ID	2
#define MSG_ID	0
#endif
#endif

static DDS_DynamicType ChatMsg_type;

#ifdef USE_MUTABLE

void set_key_annotation (DDS_DynamicTypeBuilder b,
			 const char             *name)
{
	DDS_DynamicTypeMember		dtm;
	DDS_ReturnCode_t	 	ret;
	static DDS_AnnotationDescriptor	ad = { NULL, {0,} };

	if (!b && !name) {
		DDS_AnnotationDescriptor__clear (&ad);
		return;
	}
	if (!ad.type) {
		ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("Key");
		if (!ad.type)
			fatal ("set_key: can't get builtin annotation!");
	}
	dtm = DDS_DynamicTypeMember__alloc ();
	if (!dtm)
		fatal ("set_key: can't allocate dynamic type member!");

	ret = DDS_DynamicTypeBuilder_get_member_by_name (b, dtm, name);
	if (ret)
		fatal ("set_key: can't get member!");

	ret = DDS_DynamicTypeMember_apply_annotation (dtm, &ad);
	if (ret)
		fatal ("set_key: can't apply annotation!");

	DDS_DynamicTypeMember__free (dtm);
}

void set_id_annotation (DDS_DynamicTypeBuilder b,
			const char             *name,
			DDS_MemberId           id)
{
	DDS_DynamicTypeMember		dtm;
	DDS_ReturnCode_t	 	ret;
	unsigned			n;
	char				buf [12];
	static DDS_AnnotationDescriptor	ad = { NULL, {0,} };

	if (!b && !name) {
		DDS_AnnotationDescriptor__clear (&ad);
		return;
	}
	if (!ad.type) {
		ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("ID");
		if (!ad.type)
			fatal ("set_id: can't get builtin annotation!");
	}
	n = snprintf (buf, sizeof (buf), "%u", id);
	if (!n)
		fatal ("set_id: can't create id value!");

	ret = DDS_AnnotationDescriptor_set_value (&ad, "value", buf);
	if (ret)
		fatal ("set_id: can't set id value in annotation!");

	dtm = DDS_DynamicTypeMember__alloc ();
	if (!dtm)
		fatal ("set_id: can't allocate dynamic type member!");

	ret = DDS_DynamicTypeBuilder_get_member_by_name (b, dtm, name);
	if (ret)
		fatal ("set_id: can't get member!");

	ret = DDS_DynamicTypeMember_apply_annotation (dtm, &ad);
	if (ret)
		fatal ("set_id: can't apply annotation!");

	DDS_DynamicTypeMember__free (dtm);
}

void set_ext_annotation (DDS_DynamicTypeBuilder b,
			 const char             *ext)
{
	DDS_ReturnCode_t	 	ret;
	static DDS_AnnotationDescriptor	ad = { NULL, {0,} };

	if (!b && !ext) {
		DDS_AnnotationDescriptor__clear (&ad);
		return;
	}
	if (!ad.type) {
		ad.type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("Extensibility");
		if (!ad.type)
			fatal ("set_ext: can't get builtin annotation!");
	}
	ret = DDS_AnnotationDescriptor_set_value (&ad, "value", ext);
	if (ret)
		fatal ("set_ext: can't set value!");

	ret = DDS_DynamicTypeBuilder_apply_annotation (b, &ad);
	if (ret)
		fatal ("set_id: can't apply annotation!");
}

#endif

/* ChatMsg_type_new -- Create ChatMsg_t type support data.  If errors occur, it 
		       returns NULL.  Otherwise the returned type support data
		       can be registered in any domain. */

DDS_DynamicTypeSupport ChatMsg_type_new (void)
{
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md = NULL;
	DDS_DynamicTypeBuilder bstr = NULL, tb = NULL;
	DDS_DynamicType str = NULL;
	DDS_DynamicTypeSupport ts = NULL;
	DDS_ReturnCode_t rc;

	desc = DDS_TypeDescriptor__alloc ();
	if (!desc)
		return (NULL);

	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "ChatMsg";

	do {
		tb = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;

		md = DDS_MemberDescriptor__alloc ();
		if (!md)
			break;

		bstr = DDS_DynamicTypeBuilderFactory_create_string_type (0);
		if (!bstr)
			break;

		str = DDS_DynamicTypeBuilder_build (bstr);
		if (!str)
			break;

		/* Add structure members: */
		md->name = M_0;
		md->id = 0;
		md->type = str;
		md->index = 0;
		rc = DDS_DynamicTypeBuilder_add_member (tb, md);
		if (rc)
			break;

#ifdef USE_MUTABLE
		set_id_annotation (tb, M_0, CBOX_ID);
		set_key_annotation (tb, M_0);
#endif
		md->name = M_1;
		md->id = 1;
		md->type = str;
		md->index = 1;
		rc = DDS_DynamicTypeBuilder_add_member (tb, md);
		if (rc)
			break;

#ifdef USE_MUTABLE
		set_id_annotation (tb, M_1, FROM_ID);
		set_key_annotation (tb, M_1);
#endif

		md->name = M_2;
		md->id = 2;
		md->type = str;
		md->index = 2;
		rc = DDS_DynamicTypeBuilder_add_member (tb, md);
		if (rc)
			break;

#ifdef USE_MUTABLE
		set_id_annotation (tb, M_2, MSG_ID);
		set_ext_annotation (tb, "MUTABLE_EXTENSIBILITY");
#endif

		/* Finally create the Dynamic Type t. */
		ChatMsg_type = DDS_DynamicTypeBuilder_build (tb);
		if (!ChatMsg_type)
			break;

		/* Create a Typesupport package from the type. */
		ts = DDS_DynamicTypeSupport_create_type_support (ChatMsg_type);
	}
	while (0);

	if (md)
		DDS_MemberDescriptor__free (md);
	if (str)
		DDS_DynamicTypeBuilderFactory_delete_type (str);
	if (bstr)
		DDS_DynamicTypeBuilderFactory_delete_type (bstr);
	if (tb)
		DDS_DynamicTypeBuilderFactory_delete_type (tb);
	if (desc)
		DDS_TypeDescriptor__free (desc);

#ifdef USE_MUTABLE
	set_id_annotation (NULL, NULL, 0);
	set_key_annotation (NULL, NULL);
	set_ext_annotation (NULL, NULL);
#endif
	return (ts);
}

/* ChatMsg_type_free -- Release the previously created typesupport data. */

void ChatMsg_type_free (DDS_DynamicTypeSupport ts)
{
	if (ChatMsg_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (ChatMsg_type);
		DDS_DynamicTypeSupport_delete_type_support (ts);
		ChatMsg_type = NULL;
	}
}

/* ChatMsg_register -- Write a chat message on the dynamic type writer. */

DDS_InstanceHandle_t ChatMsg_register (DDS_DynamicDataWriter  dw,
				       ChatMsg_t              *data)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;
	DDS_InstanceHandle_t h = 0;

	d = DDS_DynamicDataFactory_create_data (ChatMsg_type);
	if (!d)
		return (0);

	do {
		rc = DDS_DynamicData_set_string_value (d, CBOX_ID, data->chatroom);
		if (rc)
			break;

		rc = DDS_DynamicData_set_string_value (d, FROM_ID, data->from);
		if (rc)
			break;

		h = DDS_DynamicDataWriter_register_instance (dw, d);
	}
	while (0);

	DDS_DynamicDataFactory_delete_data (d);
	return (h);
}

/* ChatMsg_write -- Write a chat message on the dynamic type writer. */

DDS_ReturnCode_t ChatMsg_write (DDS_DynamicDataWriter  dw,
				ChatMsg_t              *data,
				DDS_InstanceHandle_t   h)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;

	d = DDS_DynamicDataFactory_create_data (ChatMsg_type);
	if (!d)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	do {
		rc = DDS_DynamicData_set_string_value (d, CBOX_ID, data->chatroom);
		if (rc)
			break;

		rc = DDS_DynamicData_set_string_value (d, FROM_ID, data->from);
		if (rc)
			break;

		rc = DDS_DynamicData_set_string_value (d, MSG_ID, data->message);
		if (rc)
			break;

		rc = DDS_DynamicDataWriter_write (dw, d, h);
	}
	while (0);

	DDS_DynamicDataFactory_delete_data (d);
	return (rc);
}

/* ChatMsg_signal -- Indicate a chat signal on the dynamic type writer. */

DDS_ReturnCode_t ChatMsg_signal (DDS_DynamicDataWriter  dw,
				 DDS_InstanceHandle_t   h,
				 int                    unreg)
{
	DDS_ReturnCode_t rc;

	if (unreg)
		rc = DDS_DynamicDataWriter_unregister_instance (dw, NULL, h);
	else
		rc = DDS_DynamicDataWriter_dispose (dw, NULL, h);
	return (rc);
}

static DDS_ReturnCode_t get_string (DDS_DynamicData d, char **s, DDS_MemberId id)
{
	DDS_ReturnCode_t rc;
	ssize_t		 size;

	size = DDS_DynamicData_get_string_length (d, id);
	if (size < 0)
		return (DDS_RETCODE_BAD_PARAMETER);

	*s = malloc (size + 1);
	if (!*s)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	rc = DDS_DynamicData_get_string_value (d, *s, id);
	if (rc)
		return (rc);

	return (DDS_RETCODE_OK);
}

/* ChatMsg_read -- Dynamically read a ChatMsg_t data item. */

DDS_ReturnCode_t ChatMsg_read_or_take (DDS_DynamicDataReader dr,
				       ChatMsg_t             *data,
				       DDS_SampleStateMask   ss,
				       DDS_ViewStateMask     vs,
				       DDS_InstanceStateMask is,
				       int                   take,
				       int                   *valid,
				       DDS_InstanceStateKind *kind)
{
	DDS_DynamicData		d;
	DDS_ReturnCode_t	rc;
	DDS_DynamicDataSeq	rx_sample;
	DDS_SampleInfoSeq	rx_info;
	DDS_SampleInfo		*info;

	DDS_SEQ_INIT (rx_sample);
	DDS_SEQ_INIT (rx_info);

	if (take)
		rc = DDS_DynamicDataReader_take (dr, &rx_sample, &rx_info, 1, ss, vs, is);
	else
		rc = DDS_DynamicDataReader_read (dr, &rx_sample, &rx_info, 1, ss, vs, is);
	do {
		if (rc && rc == DDS_RETCODE_NO_DATA)
			break;

		else if (rc)
			return (rc);

		if (!DDS_SEQ_LENGTH (rx_info)) {
			rc = DDS_RETCODE_NO_DATA;
			break;
		}
		info = DDS_SEQ_ITEM (rx_info, 0);
		*valid = info->valid_data;
		*kind = info->instance_state;
		if (!info->valid_data) {
			d = DDS_DynamicDataFactory_create_data (ChatMsg_type);
			rc = DDS_DynamicDataReader_get_key_value (dr, d, info->instance_handle);
			if (rc)
				fatal ("Can't get key value of instance!");
		}
		else {
			d = DDS_SEQ_ITEM (rx_sample, 0);
			if (!d) {
				rc = DDS_RETCODE_NO_DATA;
				break;
			}
		}

		/* Valid dynamic data sample received: parse the member fields. */
		rc = get_string (d, &data->chatroom, CBOX_ID);
		if (rc)
			break;

		rc = get_string (d, &data->from, FROM_ID);
		if (rc)
			break;

		if (info->valid_data)
			rc = get_string (d, &data->message, MSG_ID);

	}
	while (0);

	DDS_DynamicDataReader_return_loan (dr, &rx_sample, &rx_info);
	return (DDS_RETCODE_OK);
}

/* ChatMsg_cleanup -- Cleanup dynamic message data. */

void ChatMsg_cleanup (ChatMsg_t *data)
{
	if (data->chatroom) {
		free (data->chatroom);
		data->chatroom = NULL;
	}
	if (data->from) {
		free (data->from);
		data->from = NULL;
	}
	if (data->message) {
		free (data->message);
		data->message = NULL;
	}
}


