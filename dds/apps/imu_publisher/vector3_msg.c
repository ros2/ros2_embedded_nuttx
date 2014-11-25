/*
   	Copyright 2014 Open Source Robotics Foundation, Inc.
	Apache License Version 2.0
		
		Coded by Víctor Mayoral Vilches.
 */

/* vector3_msg.h -- Vector3 message type handling. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libx.h"
#include "vector3_msg.h"

#define M_0	"x"
#define M_1	"y"
#define M_2	"z"
#define X_ID	0
#define Y_ID	1
#define Z_ID	2

static DDS_DynamicType Vector3_type;

/* Vector3_type_new -- Create Vector3_t type support data.  If errors occur, it 
		       returns NULL.  Otherwise the returned type support data
		       can be registered in any domain. */

DDS_DynamicTypeSupport Vector3_type_new (void)
{
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md = NULL;
	DDS_DynamicTypeBuilder tb = NULL;
	DDS_DynamicTypeSupport ts = NULL;
	DDS_ReturnCode_t rc;

	desc = DDS_TypeDescriptor__alloc ();
	if (!desc)
		return (NULL);

	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "Vector3";

	do {
		tb = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;

		md = DDS_MemberDescriptor__alloc ();
		if (!md)
			break;

		/* Add structure members: */
		md->name = M_0;
		md->id = 0;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		md->index = 0;
		rc = DDS_DynamicTypeBuilder_add_member (tb, md);
		if (rc)
			break;

		md->name = M_1;
		md->id = 1;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		md->index = 1;
		rc = DDS_DynamicTypeBuilder_add_member (tb, md);
		if (rc)
			break;

		md->name = M_2;
		md->id = 2;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		md->index = 2;
		rc = DDS_DynamicTypeBuilder_add_member (tb, md);
		if (rc)
			break;

		/* Finally create the Dynamic Type t. */
		Vector3_type = DDS_DynamicTypeBuilder_build (tb);
		if (!Vector3_type)
			break;

		/* Create a Typesupport package from the type. */
		ts = DDS_DynamicTypeSupport_create_type_support (Vector3_type);
	}
	while (0);

	if (md)
		DDS_MemberDescriptor__free (md);
	if (tb)
		DDS_DynamicTypeBuilderFactory_delete_type (tb);
	if (desc)
		DDS_TypeDescriptor__free (desc);
	return (ts);
}

/* Vector3_type_free -- Release the previously created typesupport data. */

void Vector3_type_free (DDS_DynamicTypeSupport ts)
{
	if (Vector3_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (Vector3_type);
		DDS_DynamicTypeSupport_delete_type_support (ts);
		Vector3_type = NULL;
	}
}

/* Vector3_register -- Register a Vector3 in the DataWriter. */

DDS_InstanceHandle_t Vector3_register (DDS_DynamicDataWriter  dw,
				       Vector3_t              *data)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;
	DDS_InstanceHandle_t h = 0;

	d = DDS_DynamicDataFactory_create_data (Vector3_type);
	if (!d)
		return (0);

	do {
		/* Not necessary to register

		rc = DDS_DynamicData_set_string_value (d, X_ID, data->x_);
		if (rc)
			break;

		rc = DDS_DynamicData_set_string_value (d, Y_ID, data->y_);
		if (rc)
			break;

		rc = DDS_DynamicData_set_string_value (d, Z_ID, data->z_);
		if (rc)
			break;
		*/

		h = DDS_DynamicDataWriter_register_instance (dw, d);
	}
	while (0);
	DDS_DynamicDataFactory_delete_data (d);
	return (h);
}

/* Vector3_write -- Write a message on the dynamic type writer. */

DDS_ReturnCode_t Vector3_write (DDS_DynamicDataWriter  dw,
				Vector3_t              *data,
				DDS_InstanceHandle_t   h)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;

	d = DDS_DynamicDataFactory_create_data (Vector3_type);
	if (!d)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	do {
		rc = DDS_DynamicData_set_float64_value (d, X_ID, data->x_);
		if (rc)
			break;

		rc = DDS_DynamicData_set_float64_value (d, Y_ID, data->y_);
		if (rc)
			break;

		rc = DDS_DynamicData_set_float64_value (d, Z_ID, data->z_);
		if (rc)
			break;

		rc = DDS_DynamicDataWriter_write (dw, d, h);
	}
	while (0);

	DDS_DynamicDataFactory_delete_data (d);
	return (rc);
}

/* Vector3_signal -- Indicate a vector3 signal on the dynamic type writer. */

DDS_ReturnCode_t Vector3_signal (DDS_DynamicDataWriter  dw,
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

static DDS_ReturnCode_t get_double (DDS_DynamicData d, double *s, DDS_MemberId id)
{
	DDS_ReturnCode_t rc;

	rc = DDS_DynamicData_get_float64_value (d, s, id);
	if (rc)
		return (rc);
	return (DDS_RETCODE_OK);
}

/* Vector3_read -- Dynamically read a Vector3_t data item. */

DDS_ReturnCode_t Vector3_read_or_take (DDS_DynamicDataReader dr,
				       Vector3_t             *data,
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
			d = DDS_DynamicDataFactory_create_data (Vector3_type);
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
		rc = get_double (d, &data->x_, X_ID);
		if (rc)
			break;

		rc = get_double (d, &data->y_, Y_ID);
		if (rc)
			break;

		rc = get_double (d, &data->z_, Z_ID);
		if (rc)
			break;

	}
	while (0);

	DDS_DynamicDataReader_return_loan (dr, &rx_sample, &rx_info);
	return (DDS_RETCODE_OK);
}

/* Vector3_cleanup -- Cleanup dynamic message data. */

void Vector3_cleanup (Vector3_t *data)
{
// Not necessary with primitive types
#if 0
	if (data->x_) {
		free (data->x_);
		data->x_ = NULL;
	}
	if (data->y_) {
		free (data->y_);
		data->y_ = NULL;
	}
	if (data->z_) {
		free (data->z_);
		data->z_ = NULL;
	}
#endif
}


