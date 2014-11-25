/*
   	Copyright 2014 Open Source Robotics Foundation, Inc.
	Apache License Version 2.0
		
		Coded by Víctor Mayoral Vilches.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libx.h"
#include "imu32_msg.h"

// stamp
#define stamp_sec		"sec"
#define stamp_nanosec	"nanosec"
#define stamp 			"stamp"

#define stamp_sec_id		0
#define stamp_nanosec_id	1
#define stamp_id			1

// header		
#define header_seq		"seq"
#define header_stamp	"stamp"
#define header_frameid 	"frame_id"
#define header 			"header"

#define header_seq_id		0
#define header_stamp_id		1
#define header_frameid_id 	2
#define header_id 			0

// orientation
#define orientation_x	"x"
#define orientation_y	"y"
#define orientation_z	"z"
#define orientation_w	"w"
#define orientation		"orientation"		

#define orientation_x_id	0
#define orientation_y_id	1
#define orientation_z_id	2
#define orientation_w_id	3
#define orientation_id		1

//angular_velocity
#define angular_velocity_x		"x"
#define angular_velocity_y		"y"
#define angular_velocity_z		"z"
#define angular_velocity		"angular_velocity"

#define angular_velocity_x_id		0
#define angular_velocity_y_id		1
#define angular_velocity_z_id		2
#define angular_velocity_id			3

//linear_acceleration
#define linear_acceleration_x		"x"
#define linear_acceleration_y		"y"
#define linear_acceleration_z		"z"
#define linear_acceleration			"linear_acceleration"

#define linear_acceleration_x_id		0
#define linear_acceleration_y_id		1
#define linear_acceleration_z_id		2
#define linear_acceleration_id			5

//orientation_covariance
#define orientation_covariance		"orientation_covariance"
#define orientation_covariance_id	2

//angular_velocity_covariance
#define angular_velocity_covariance "angular_velocity_covariance"
#define angular_velocity_covariance_id 	4

//linear_acceleration_covariance
#define linear_acceleration_covariance "linear_acceleration_covariance"
#define linear_acceleration_covariance_id 	6

static DDS_DynamicType Imu32_type;

/* Imu32_type_new -- Create Imu32_t type support data.  If errors occur, it 
		       returns NULL.  Otherwise the returned type support data
		       can be registered in any domain. */

DDS_DynamicTypeSupport Imu32_type_new (void)
{
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md = NULL;
	DDS_DynamicTypeBuilder ssb;
	DDS_DynamicTypeBuilder tb_header = NULL;
	DDS_DynamicTypeBuilder tb_time = NULL;	
	DDS_DynamicTypeBuilder tb_orientation = NULL;
	DDS_DynamicTypeBuilder tb_angular_velocity = NULL;
	DDS_DynamicTypeBuilder tb_linear_acceleration = NULL;			
	DDS_DynamicTypeBuilder tb_imu23 = NULL;
	DDS_DynamicTypeBuilder tb_float_array;
	DDS_DynamicType s_type, header_type, time_type, orientation_type,
							angular_velocity_type, linear_acceleration_type,
							float_array_type;
	DDS_BoundSeq bounds;
	DDS_DynamicTypeSupport ts = NULL;
	DDS_ReturnCode_t rc;

	desc = DDS_TypeDescriptor__alloc ();
	if (!desc)
		return (NULL);

	do {

		// MemberDescriptor to be reused on the type definitions
		md = DDS_MemberDescriptor__alloc ();
		if (!md)
			break;

		/* The Imu32 type support is coded following the type definition in 
		the header imu32_msg.h */

		/* Create time: */		
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "time";
		tb_time = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;
		md->name = stamp_sec;
		md->id = md->index= stamp_sec_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_time, md);
		if (rc)
			break;
		md->name = stamp_nanosec;
		md->id = md->index = stamp_nanosec_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_time, md);
		if (rc)
			break;
		time_type = DDS_DynamicTypeBuilder_build (tb_time);
		if (!time_type)
			break;

		/* Create String. */
		ssb = DDS_DynamicTypeBuilderFactory_create_string_type (FRAMEID_LEN);
		if (!ssb)
			break;
		s_type = DDS_DynamicTypeBuilder_build (ssb);
		if (!s_type)
			break;

		/* Create header: */
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "header";
		tb_header = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;
		md->name = header_seq;
		md->id = md->index = header_seq_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_header, md);
		if (rc)
			break;
		md->name = header_stamp;
		md->id = md->index = header_stamp_id;		
		md->type = time_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_header, md);
		if (rc)
			break;
		md->name = header_frameid;
		md->id = md->index = header_frameid_id;		
		md->type = s_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_header, md);
		if (rc)
			break;
		header_type = DDS_DynamicTypeBuilder_build (tb_header);
		if (!header_type)
			break;

		/* Create orientation: */
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "orientation";
		tb_orientation = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;
		md->name = orientation_x;
		md->id = md->index = orientation_x_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		md->name = orientation_y;
		md->id = orientation_y_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		md->index = 1;
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		md->name = orientation_z;
		md->id = orientation_z_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		md->index = 2;
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		md->name = orientation_w;
		md->id = orientation_w_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		md->index = 3;
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		orientation_type = DDS_DynamicTypeBuilder_build (tb_orientation);
		if (!orientation_type)
			break;

		/* Create angular_velocity: */
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "angular_velocity";
		tb_angular_velocity = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;
		md->name = angular_velocity_x;
		md->id = md->index = angular_velocity_x_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_angular_velocity, md);
		if (rc)
			break;
		md->name = angular_velocity_y;
		md->id = md->index = angular_velocity_y_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_angular_velocity, md);
		if (rc)
			break;
		md->name = angular_velocity_z;
		md->id = md->index = angular_velocity_z_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_angular_velocity, md);
		if (rc)
			break;
		angular_velocity_type = DDS_DynamicTypeBuilder_build (tb_angular_velocity);
		if (!angular_velocity_type)
			break;

		/* Create linear_acceleration: */
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "linear_acceleration";
		tb_linear_acceleration = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb)
			break;
		md->name = linear_acceleration_x;
		md->id = md->index = linear_acceleration_x_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_linear_acceleration, md);
		if (rc)
			break;
		md->name = linear_acceleration_y;
		md->id = md->index = linear_acceleration_y_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_linear_acceleration, md);
		if (rc)
			break;
		md->name = linear_acceleration_z;
		md->id = md->index = linear_acceleration_z_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_linear_acceleration, md);
		if (rc)
			break;
		linear_acceleration_type = DDS_DynamicTypeBuilder_build (tb_linear_acceleration);
		if (!linear_acceleration_type)
			break;

		/* Create float array type */
		DDS_SEQ_INIT (bounds);
		dds_seq_require (&bounds, 1);
		DDS_SEQ_LENGTH (bounds) = 1;
		DDS_SEQ_ITEM (bounds, 0) = 9;		
		tb_float_array = DDS_DynamicTypeBuilderFactory_create_array_type (
			DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_32_TYPE),
			&bounds);
		if (!tb_float_array)
			break;		
		float_array_type = DDS_DynamicTypeBuilder_build (tb_float_array);
		if (!float_array_type)
			break;

		// Create Imu32
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "Imu32";
		tb_imu23 = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb_aux)
			break;
					// header
		md->name = header;
		md->id = md->index = header_id;		
		md->type = header_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// orientation
		md->name = orientation;
		md->id = md->index = orientation_id;		
		md->type = orientation_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// orientation_covariance
		md->name = orientation_covariance;
		md->id = md->index = orientation_covariance_id;		
		md->type = float_array_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// angular_velocity
		md->name = angular_velocity;
		md->id = md->index = angular_velocity_id;		
		md->type = angular_velocity_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// angular_velocity_covariance
		md->name = angular_velocity_covariance;
		md->id = md->index = angular_velocity_covariance_id;		
		md->type = float_array_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// linear_acceleration
		md->name = linear_acceleration;
		md->id = md->index = linear_acceleration_id;		
		md->type = linear_acceleration_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// linear_acceleration_covariance
		md->name = linear_acceleration_covariance;
		md->id = md->index = linear_acceleration_covariance_id;		
		md->type = float_array_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;

		/* Finally create the Dynamic Type t. */
		Imu32_type = DDS_DynamicTypeBuilder_build (tb_imu23);
		if (!Imu32_type)
			break;
		/* Create a Typesupport package from the type. */
		ts = DDS_DynamicTypeSupport_create_type_support (Imu32_type);
	}
	while (0);

	if (md)
		DDS_MemberDescriptor__free (md);
	if (tb_header)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_header);
	if (tb_time)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_time);
	if (tb_orientation)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_orientation);
	if (tb_angular_velocity)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_angular_velocity);
	if (tb_linear_acceleration)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_linear_acceleration);
	if (tb_imu23)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_imu23);
	if (tb_float_array)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_float_array);		

	if (desc)
		DDS_TypeDescriptor__free (desc);
	return (ts);
}

/* Imu32_type_free -- Release the previously created typesupport data. */

void Imu32_type_free (DDS_DynamicTypeSupport ts)
{
	if (Imu32_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (Imu32_type);
		DDS_DynamicTypeSupport_delete_type_support (ts);
		Imu32_type = NULL;
	}
}

/* Imu32_register -- Register a Imu32 in the DataWriter. */

DDS_InstanceHandle_t Imu32_register (DDS_DynamicDataWriter  dw,
				       Imu32_t              *data)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;
	DDS_InstanceHandle_t h = 0;

	d = DDS_DynamicDataFactory_create_data (Imu32_type);
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

/* Imu32_write -- Write a message on the dynamic type writer. */

DDS_ReturnCode_t Imu32_write (DDS_DynamicDataWriter  dw,
				Imu32_t              *data,
				DDS_InstanceHandle_t   h)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;

	d = DDS_DynamicDataFactory_create_data (Imu32_type);
	if (!d)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	do {
		rc = DDS_DynamicData_set_float32_value (d, X_ID, data->x_);
		if (rc)
			break;

		rc = DDS_DynamicData_set_float32_value (d, Y_ID, data->y_);
		if (rc)
			break;

		rc = DDS_DynamicData_set_float32_value (d, Z_ID, data->z_);
		if (rc)
			break;

		rc = DDS_DynamicDataWriter_write (dw, d, h);
	}
	while (0);

	DDS_DynamicDataFactory_delete_data (d);
	return (rc);
}

/* Imu32_signal -- Indicate a vector3 signal on the dynamic type writer. */

DDS_ReturnCode_t Imu32_signal (DDS_DynamicDataWriter  dw,
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

static DDS_ReturnCode_t get_datatype (DDS_DynamicData d, double *s, DDS_MemberId id)
{
	DDS_ReturnCode_t rc;

	rc = DDS_DynamicData_get_float32_value (d, s, id);
	if (rc)
		return (rc);
	return (DDS_RETCODE_OK);
}

/* Imu32_read -- Dynamically read a Imu32_t data item. */

DDS_ReturnCode_t Imu32_read_or_take (DDS_DynamicDataReader dr,
				       Imu32_t             *data,
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
			d = DDS_DynamicDataFactory_create_data (Imu32_type);
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
		rc = get_datatype (d, &data->x_, X_ID);
		if (rc)
			break;

		rc = get_datatype (d, &data->y_, Y_ID);
		if (rc)
			break;

		rc = get_datatype (d, &data->z_, Z_ID);
		if (rc)
			break;

	}
	while (0);

	DDS_DynamicDataReader_return_loan (dr, &rx_sample, &rx_info);
	return (DDS_RETCODE_OK);
}

/* Imu32_cleanup -- Cleanup dynamic message data. */

void Imu32_cleanup (Imu32_t *data)
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


