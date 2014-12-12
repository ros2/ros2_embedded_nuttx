/*
   	Copyright 2014 Open Source Robotics Foundation, Inc.
	Apache License Version 2.0
		
		Coded by Víctor Mayoral Vilches.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libx.h"
#include "imu_msg.h"

// stamp
#define stamp_sec		"sec"
#define stamp_nanosec	"nanosec"
#define stamp_string 			"stamp"

#define stamp_sec_id		0
#define stamp_nanosec_id	1
#define stamp_id			1

// header		
#define header_seq		"seq"
#define header_stamp	"stamp"
#define header_frameid 	"frame_id"
#define header_string 			"header"

#define header_seq_id		0
#define header_stamp_id		1
#define header_frameid_id 	2
#define header_id 			0

// orientation
#define orientation_x	"x"
#define orientation_y	"y"
#define orientation_z	"z"
#define orientation_w	"w"
#define orientation_string		"orientation"		

#define orientation_x_id	0
#define orientation_y_id	1
#define orientation_z_id	2
#define orientation_w_id	3
#define orientation_id		1

//angular_velocity
#define angular_velocity_x		"x"
#define angular_velocity_y		"y"
#define angular_velocity_z		"z"
#define angular_velocity_string		"angular_velocity"

#define angular_velocity_x_id		0
#define angular_velocity_y_id		1
#define angular_velocity_z_id		2
#define angular_velocity_id			3

//linear_acceleration
#define linear_acceleration_x		"x"
#define linear_acceleration_y		"y"
#define linear_acceleration_z		"z"
#define linear_acceleration_string			"linear_acceleration"

#define linear_acceleration_x_id		0
#define linear_acceleration_y_id		1
#define linear_acceleration_z_id		2
#define linear_acceleration_id			5

//orientation_covariance
#define orientation_covariance_string		"orientation_covariance"
#define orientation_covariance_id	2

//angular_velocity_covariance
#define angular_velocity_covariance_string "angular_velocity_covariance"
#define angular_velocity_covariance_id 	4

//linear_acceleration_covariance
#define linear_acceleration_covariance_string "linear_acceleration_covariance"
#define linear_acceleration_covariance_id 	6

static DDS_DynamicType Imu_type;
static DDS_DynamicType s_type, header_type, time_type, orientation_type,
							angular_velocity_type, linear_acceleration_type,
							float_array_type;

/* Imu_type_new -- Create Imu_t type support data.  If errors occur, it 
		       returns NULL.  Otherwise the returned type support data
		       can be registered in any domain. */

DDS_DynamicTypeSupport Imu_type_new (void)
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

		/* The Imu type support is coded following the type definition in 
		the header imu_msg.h */

		/* Create time: */		
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "stamp";
		tb_time = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb_time)
			break;
		md->name = stamp_sec;
		md->id = md->index= stamp_sec_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_time, md);
		if (rc)
			break;
		md->name = stamp_nanosec;
		md->id = md->index = stamp_nanosec_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_UINT_32_TYPE);
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
		if (!tb_header)
			break;
		md->name = header_seq;
		md->id = md->index = header_seq_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_UINT_32_TYPE);
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
		if (!tb_orientation)
			break;
		md->name = orientation_x;
		md->id = md->index = orientation_x_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		md->name = orientation_y;
		md->id = md->index = orientation_y_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);		
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		md->name = orientation_z;
		md->id = md->index = orientation_z_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);		
		rc = DDS_DynamicTypeBuilder_add_member (tb_orientation, md);
		if (rc)
			break;
		md->name = orientation_w;
		md->id = md->index = orientation_w_id;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);		
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
		if (!tb_angular_velocity)
			break;
		md->name = angular_velocity_x;
		md->id = md->index = angular_velocity_x_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_angular_velocity, md);
		if (rc)
			break;
		md->name = angular_velocity_y;
		md->id = md->index = angular_velocity_y_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_angular_velocity, md);
		if (rc)
			break;
		md->name = angular_velocity_z;
		md->id = md->index = angular_velocity_z_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
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
		if (!tb_linear_acceleration)
			break;
		md->name = linear_acceleration_x;
		md->id = md->index = linear_acceleration_x_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_linear_acceleration, md);
		if (rc)
			break;
		md->name = linear_acceleration_y;
		md->id = md->index = linear_acceleration_y_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_linear_acceleration, md);
		if (rc)
			break;
		md->name = linear_acceleration_z;
		md->id = md->index = linear_acceleration_z_id;		
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
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
			DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE),
			&bounds);
		if (!tb_float_array)
			break;		
		float_array_type = DDS_DynamicTypeBuilder_build (tb_float_array);
		if (!float_array_type)
			break;

		// Create Imu
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "Imu";
		tb_imu23 = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb_imu23)
			break;
					// header
		md->name = header_string;
		md->id = md->index = header_id;		
		md->type = header_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// orientation
		md->name = orientation_string;
		md->id = md->index = orientation_id;		
		md->type = orientation_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// orientation_covariance
		md->name = orientation_covariance_string;
		md->id = md->index = orientation_covariance_id;		
		md->type = float_array_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// angular_velocity
		md->name = angular_velocity_string;
		md->id = md->index = angular_velocity_id;		
		md->type = angular_velocity_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// angular_velocity_covariance
		md->name = angular_velocity_covariance_string;
		md->id = md->index = angular_velocity_covariance_id;		
		md->type = float_array_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// linear_acceleration
		md->name = linear_acceleration_string;
		md->id = md->index = linear_acceleration_id;		
		md->type = linear_acceleration_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;
					// linear_acceleration_covariance
		md->name = linear_acceleration_covariance_string;
		md->id = md->index = linear_acceleration_covariance_id;		
		md->type = float_array_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_imu23, md);
		if (rc)
			break;

		/* Finally create the Dynamic Type t. */
		Imu_type = DDS_DynamicTypeBuilder_build (tb_imu23);
		if (!Imu_type)
			break;
		/* Create a Typesupport package from the type. */
		ts = DDS_DynamicTypeSupport_create_type_support (Imu_type);
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

/* Imu_type_free -- Release the previously created typesupport data. */

void Imu_type_free (DDS_DynamicTypeSupport ts)
{
	if (Imu_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (Imu_type);
		DDS_DynamicTypeSupport_delete_type_support (ts);
		Imu_type = NULL;
	}
}

/* Imu_register -- Register a Imu in the DataWriter. */

DDS_InstanceHandle_t Imu_register (DDS_DynamicDataWriter  dw,
				       Imu_t              *data)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;
	DDS_InstanceHandle_t h = 0;

	d = DDS_DynamicDataFactory_create_data (Imu_type);
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

/* Imu_write -- Write a message on the dynamic type writer. */

DDS_ReturnCode_t Imu_write (DDS_DynamicDataWriter  dw,
                Imu_t              *data,
                DDS_InstanceHandle_t   h)
{
DDS_DynamicData d_imu, d_time, d_header,
                d_orientation, d_angular_velocity,
                d_linear_acceleration,
                d_orientation_cov, d_angular_velocity_cov,
                d_linear_acceleration_cov;

DDS_Float64Seq fseq = DDS_SEQ_INITIALIZER (double);
DDS_ReturnCode_t rc;

d_imu = DDS_DynamicDataFactory_create_data (Imu_type);
if (!d_imu)
    return (DDS_RETCODE_OUT_OF_RESOURCES);

do {

    /* According to @jvoe in #28:

    For data creation, it's slightly different. To add array data, use 
    DDS_DynamicData_set_float32_values(). To add a substruct, first create 
    the substruct data, populating all data member fields, and add the complete 
    substruct as member data to the top struct with DDS_DynamicData_set_complex_value(). 
    */

    /* Time */
    d_time = DDS_DynamicDataFactory_create_data (time_type);
    if (!d_time)
        return (DDS_RETCODE_OUT_OF_RESOURCES);      
    rc = DDS_DynamicData_set_int32_value (d_time, stamp_sec_id, data->header.stamp.sec);
    if (rc)
        break;
    rc = DDS_DynamicData_set_uint32_value (d_time, stamp_nanosec_id, data->header.stamp.nanosec);
    if (rc)
        break;

    /* Header */
    d_header = DDS_DynamicDataFactory_create_data (header_type);
    if (!d_header)
        return (DDS_RETCODE_OUT_OF_RESOURCES);      
    rc = DDS_DynamicData_set_uint32_value (d_header, header_seq_id, data->header.seq);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_header, header_stamp_id, d_time);
    if (rc)
        break;
    rc = DDS_DynamicData_set_string_value (d_header, header_frameid_id, data->header.frame_id);
    if (rc)
        break;

    /* Quaternion: orientation */
    d_orientation = DDS_DynamicDataFactory_create_data (orientation_type);
    if (!d_orientation)
        return (DDS_RETCODE_OUT_OF_RESOURCES);      
    rc = DDS_DynamicData_set_float64_value (d_orientation, orientation_x_id, data->orientation.x);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_orientation, orientation_y_id, data->orientation.y);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_orientation, orientation_z_id, data->orientation.z);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_orientation, orientation_w_id, data->orientation.w);
    if (rc)
        break;

    /* Vector3: angular_velocity */
    d_angular_velocity = DDS_DynamicDataFactory_create_data (angular_velocity_type);
    if (!d_angular_velocity)
        return (DDS_RETCODE_OUT_OF_RESOURCES);      
    rc = DDS_DynamicData_set_float64_value (d_angular_velocity, angular_velocity_x_id, data->angular_velocity.x_);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_angular_velocity, angular_velocity_y_id, data->angular_velocity.y_);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_angular_velocity, angular_velocity_z_id, data->angular_velocity.z_);
    if (rc)
        break;

    /* Vector3: linear_acceleration */
    d_linear_acceleration = DDS_DynamicDataFactory_create_data (linear_acceleration_type);
    if (!d_linear_acceleration)
        return (DDS_RETCODE_OUT_OF_RESOURCES);      
    rc = DDS_DynamicData_set_float64_value (d_linear_acceleration, linear_acceleration_x_id, data->linear_acceleration.x_);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_linear_acceleration, linear_acceleration_y_id, data->linear_acceleration.y_);
    if (rc)
        break;
    rc = DDS_DynamicData_set_float64_value (d_linear_acceleration, linear_acceleration_z_id, data->linear_acceleration.z_);
    if (rc)
        break;

    /* Imu */
    rc = DDS_DynamicData_set_complex_value (d_imu, header_id, d_header);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_imu, orientation_id, d_orientation);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_imu, angular_velocity_id, d_angular_velocity);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_imu, linear_acceleration_id, d_linear_acceleration);
    if (rc)
        break;

    d_orientation_cov = DDS_DynamicDataFactory_create_data (float_array_type);
    dds_seq_from_array (&fseq, data->orientation_covariance, 9);
    rc = DDS_DynamicData_set_float64_values (d_orientation_cov, 0/*orientation_covariance_id*/, &fseq);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_imu, orientation_covariance_id, d_orientation_cov);
    if (rc)
        break;

    d_angular_velocity_cov = DDS_DynamicDataFactory_create_data (float_array_type);
    dds_seq_from_array (&fseq, data->angular_velocity_covariance, 9);
    rc = DDS_DynamicData_set_float64_values (d_angular_velocity_cov, 0/*angular_velocity_covariance_id*/, &fseq);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_imu, angular_velocity_covariance_id, d_angular_velocity_cov);
    if (rc)
        break;

    d_linear_acceleration_cov = DDS_DynamicDataFactory_create_data (float_array_type);
    dds_seq_from_array (&fseq, data->linear_acceleration_covariance, 9);
    rc = DDS_DynamicData_set_float64_values (d_linear_acceleration_cov, 0/*linear_acceleration_covariance_id*/, &fseq);
    if (rc)
        break;
    rc = DDS_DynamicData_set_complex_value (d_imu, linear_acceleration_covariance_id, d_linear_acceleration_cov);
    if (rc)
        break;

	rc = DDS_DynamicDataWriter_write (dw, d_imu, h);

} while (0);

DDS_DynamicDataFactory_delete_data (d_imu);
return (rc);
}

#if 0
/* Imu_write -- Write a message on the dynamic type writer. */

DDS_ReturnCode_t Imu_write (DDS_DynamicDataWriter  dw,
				Imu_t              *data,
				DDS_InstanceHandle_t   h)
{
	DDS_DynamicData	d_imu, d_time, d_header,
					d_orientation, d_orientation_covariance, d_angular_velocity, d_angular_velocity_covariance,
					d_linear_acceleration, d_linear_acceleration_covariance;	
	DDS_ReturnCode_t rc;
	int i;

	d_imu = DDS_DynamicDataFactory_create_data (Imu_type);
	if (!d_imu)
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	do {

		/* According to @jvoe in #28:

		For data creation, it's slightly different. To add array data, use 
		DDS_DynamicData_set_float32_values(). To add a substruct, first create 
		the substruct data, populating all data member fields, and add the complete 
		substruct as member data to the top struct with DDS_DynamicData_set_complex_value(). 
		*/

		/* Time */
		d_time = DDS_DynamicDataFactory_create_data (time_type);
		if (!d_time)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		rc = DDS_DynamicData_set_int32_value (d_time, stamp_sec_id, data->header.stamp.sec);
		if (rc)
			break;
		rc = DDS_DynamicData_set_uint32_value (d_time, stamp_nanosec_id, data->header.stamp.nanosec);
		if (rc)
			break;

		/* Header */
		d_header = DDS_DynamicDataFactory_create_data (header_type);
		if (!d_header)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		rc = DDS_DynamicData_set_uint32_value (d_header, header_seq_id, data->header.seq);
		if (rc)
			break;
		rc = DDS_DynamicData_set_complex_value (d_header, header_stamp_id, d_time);
		if (rc)
			break;
		rc = DDS_DynamicData_set_string_value (d_header, header_frameid_id, data->header.frame_id);
		if (rc)
			break;

		/* Quaternion: orientation */
		d_orientation = DDS_DynamicDataFactory_create_data (orientation_type);
		if (!d_orientation)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		rc = DDS_DynamicData_set_float32_value (d_orientation, orientation_x_id, data->orientation.x);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_orientation, orientation_y_id, data->orientation.y);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_orientation, orientation_z_id, data->orientation.z);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_orientation, orientation_w_id, data->orientation.w);
		if (rc)
			break;

		/* Vector3: angular_velocity */
		d_angular_velocity = DDS_DynamicDataFactory_create_data (angular_velocity_type);
		if (!d_angular_velocity)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		rc = DDS_DynamicData_set_float32_value (d_angular_velocity, angular_velocity_x_id, data->angular_velocity.x_);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_angular_velocity, angular_velocity_y_id, data->angular_velocity.y_);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_angular_velocity, angular_velocity_z_id, data->angular_velocity.z_);
		if (rc)
			break;

		/* Vector3: linear_acceleration */
		d_linear_acceleration = DDS_DynamicDataFactory_create_data (linear_acceleration_type);
		if (!d_linear_acceleration)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		rc = DDS_DynamicData_set_float32_value (d_linear_acceleration, linear_acceleration_x_id, data->linear_acceleration.x_);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_linear_acceleration, linear_acceleration_y_id, data->linear_acceleration.y_);
		if (rc)
			break;
		rc = DDS_DynamicData_set_float32_value (d_linear_acceleration, linear_acceleration_z_id, data->linear_acceleration.z_);
		if (rc)
			break;

		/* Imu */
		rc = DDS_DynamicData_set_complex_value (d_imu, header_id, d_header);
		if (rc)
			break;
		rc = DDS_DynamicData_set_complex_value (d_imu, orientation_id, d_orientation);
		if (rc)
			break;
		d_orientation_covariance = DDS_DynamicDataFactory_create_data (float_array_type);
		if (!d_orientation_covariance)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		for (i = 0; i< 9; i++){
			rc = DDS_DynamicData_set_float32_value (d_orientation_covariance, i, data->orientation_covariance[i]);
			if (rc)
				break;			
		}
		rc = DDS_DynamicData_set_complex_value (d_imu, orientation_covariance_id, d_orientation_covariance);
		if (rc)
			break;
		rc = DDS_DynamicData_set_complex_value (d_imu, angular_velocity_id, d_angular_velocity);
		if (rc)
			break;
		d_angular_velocity_covariance = DDS_DynamicDataFactory_create_data (float_array_type);
		if (!d_angular_velocity_covariance)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		for (i = 0; i< 9; i++){
			rc = DDS_DynamicData_set_float32_value (d_angular_velocity_covariance, i, data->angular_velocity_covariance[i]);
			if (rc)
				break;			
		}
		rc = DDS_DynamicData_set_complex_value (d_imu, angular_velocity_covariance_id, d_angular_velocity_covariance);
		if (rc)
			break;
		rc = DDS_DynamicData_set_complex_value (d_imu, linear_acceleration_id, d_linear_acceleration);
		if (rc)
			break;
		d_linear_acceleration_covariance = DDS_DynamicDataFactory_create_data (float_array_type);
		if (!d_linear_acceleration_covariance)
			return (DDS_RETCODE_OUT_OF_RESOURCES);		
		for (i = 0; i< 9; i++){
			rc = DDS_DynamicData_set_float32_value (d_linear_acceleration_covariance, i, data->linear_acceleration_covariance[i]);
			if (rc)
				break;			
		}
		rc = DDS_DynamicData_set_complex_value (d_imu, angular_velocity_covariance_id, d_linear_acceleration_covariance);
		if (rc)
			break;

		rc = DDS_DataWriter_write (dw, d_imu, h);
	}
	while (0);

	DDS_DynamicDataFactory_delete_data (d_imu);
	return (rc);
}
#endif

/* Imu_signal -- Indicate a vector3 signal on the dynamic type writer. */

DDS_ReturnCode_t Imu_signal (DDS_DynamicDataWriter  dw,
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

static DDS_ReturnCode_t get_datatype (DDS_DynamicData d, Imu_t *s)
{
	DDS_ReturnCode_t rc;
	DDS_DynamicData	d_imu, d_time, d_header,
					d_orientation, d_angular_velocity,
					d_linear_acceleration, d_floats;	

	DDS_Float64Seq fseq = DDS_SEQ_INITIALIZER (double);

	/* Header */
	rc = DDS_DynamicData_get_complex_value(d, &d_header, header_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_uint32_value (d_header, &s->header.seq, header_seq_id);
	if (rc)
		return (rc);
		/* stamp */
	rc = DDS_DynamicData_get_complex_value(d_header, &d_time, stamp_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_int32_value (d_time, &s->header.stamp.sec, stamp_sec_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_uint32_value (d_time, &s->header.stamp.nanosec, stamp_nanosec_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_string_value (d_header, s->header.frame_id, header_frameid_id);
	if (rc)
		return (rc);

	/* orientation */
	rc = DDS_DynamicData_get_complex_value(d, &d_orientation, orientation_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_orientation, &s->orientation.x, orientation_x_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_orientation, &s->orientation.y, orientation_y_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_orientation, &s->orientation.z, orientation_z_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_orientation, &s->orientation.w, orientation_w_id);
	if (rc)
		return (rc);

	/* orientation_covariance */
	rc = DDS_DynamicData_get_complex_value (d, &d_floats, orientation_covariance_id);
	if (rc)
	    return (rc);
	/* Clean way to get the complete array data via a sequence, but double memcpy(): */
	rc = DDS_DynamicData_get_float64_values (d_floats, &fseq, 0);
	if (rc)
	    return (rc);
	/* You can already use the sequence data via DDS_SEQ_ITEM(fseq, i) to access an
	   individual element, or copy everything to the s->orientation_covariance member: */
	dds_seq_to_array (&fseq, s->orientation_covariance, sizeof (s->orientation_covariance) / sizeof (double));

	/* angular_velocity */
	rc = DDS_DynamicData_get_complex_value(d, &d_angular_velocity, angular_velocity_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_angular_velocity, &s->angular_velocity.x_, angular_velocity_x_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_angular_velocity, &s->angular_velocity.y_, angular_velocity_y_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_angular_velocity, &s->angular_velocity.z_, angular_velocity_z_id);
	if (rc)
		return (rc);

	/* angular_velocity_covariance */
	rc = DDS_DynamicData_get_complex_value (d, &d_floats, angular_velocity_covariance_id);
	if (rc)
	    return (rc);
	/* Clean way to get the complete array data via a sequence, but double memcpy(): */
	rc = DDS_DynamicData_get_float64_values (d_floats, &fseq, 0);
	if (rc)
	    return (rc);
	/* You can already use the sequence data via DDS_SEQ_ITEM(fseq, i) to access an
	   individual element, or copy everything to the s->orientation_covariance member: */
	dds_seq_to_array (&fseq, s->angular_velocity_covariance, sizeof (s->angular_velocity_covariance) / sizeof (double));


	/* linear_acceleration */
	rc = DDS_DynamicData_get_complex_value(d, &d_linear_acceleration, linear_acceleration_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_linear_acceleration, &s->linear_acceleration.x_, linear_acceleration_x_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_linear_acceleration, &s->linear_acceleration.y_, linear_acceleration_y_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_linear_acceleration, &s->linear_acceleration.z_, linear_acceleration_z_id);
	if (rc)
		return (rc);

	/* linear_acceleration_covariance */
	rc = DDS_DynamicData_get_complex_value (d, &d_floats, linear_acceleration_covariance_id);
	if (rc)
	    return (rc);
	/* Clean way to get the complete array data via a sequence, but double memcpy(): */
	rc = DDS_DynamicData_get_float64_values (d_floats, &fseq, 0);
	if (rc)
	    return (rc);
	/* You can already use the sequence data via DDS_SEQ_ITEM(fseq, i) to access an
	   individual element, or copy everything to the s->orientation_covariance member: */
	dds_seq_to_array (&fseq, s->linear_acceleration_covariance, sizeof (s->linear_acceleration_covariance) / sizeof (double));


	return (DDS_RETCODE_OK);
}

/* Imu_read -- Dynamically read a Imu_t data item. */

DDS_ReturnCode_t Imu_read_or_take (DDS_DynamicDataReader dr,
				       Imu_t             *data,
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
			d = DDS_DynamicDataFactory_create_data (Imu_type);
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
		rc = get_datatype (d, data);
		if (rc)
			break;

	}
	while (0);

	DDS_DynamicDataReader_return_loan (dr, &rx_sample, &rx_info);
	return (DDS_RETCODE_OK);
}

/* Imu_cleanup -- Cleanup dynamic message data. */

void Imu_cleanup (Imu_t *data)
{
/*		free (&data->header.stamp);
		//data->header.stamp = NULL;
		free (&data->header);
		//data->header = NULL;
		free (&data->orientation);
		//data->orientation = NULL;
		free (&data->angular_velocity);
		//data->angular_velocity = NULL;
		free (&data->linear_acceleration);
		//data->linear_acceleration = NULL;
*/}


