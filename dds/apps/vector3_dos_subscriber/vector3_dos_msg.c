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
#include "vector3_dos_msg.h"

#define M_0_uno	"x"
#define M_1_uno	"y"
#define M_2_uno	"z"

#define X_ID_uno	0
#define Y_ID_uno	1
#define Z_ID_uno	2

#define uno_id	0
#define dos_id	1

static DDS_DynamicType Vector3_dos_type, Vector3_type;

/* Vector3_dos_type_new -- Create Vector3_dos_t type support data.  If errors occur, it 
		       returns NULL.  Otherwise the returned type support data
		       can be registered in any domain. */

DDS_DynamicTypeSupport Vector3_dos_type_new (void)
{
	DDS_TypeDescriptor *desc;
	DDS_MemberDescriptor *md = NULL;
	DDS_DynamicTypeBuilder tb_uno = NULL;
	DDS_DynamicTypeBuilder tb_dos = NULL;
	DDS_DynamicTypeBuilder tb_vector = NULL;	
	DDS_DynamicTypeSupport ts = NULL;
	DDS_ReturnCode_t rc;

	desc = DDS_TypeDescriptor__alloc ();
	if (!desc)
		return (NULL);

	do {
		md = DDS_MemberDescriptor__alloc ();
		if (!md)
			break;

		/* Vector3 uno: */
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "Vector3";
		tb_uno = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb_uno)
			break;

		md->name =  M_0_uno;
		md->id = md->index = X_ID_uno;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);		
		rc = DDS_DynamicTypeBuilder_add_member (tb_uno, md);
		if (rc)
			break;

		md->name = M_1_uno;
		md->id = md->index = Y_ID_uno;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);
		rc = DDS_DynamicTypeBuilder_add_member (tb_uno, md);
		if (rc)
			break;

		md->name = M_2_uno;
		md->id = md->index = Z_ID_uno;
		md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_FLOAT_64_TYPE);		
		rc = DDS_DynamicTypeBuilder_add_member (tb_uno, md);
		if (rc)
			break;

		Vector3_type = DDS_DynamicTypeBuilder_build (tb_uno);
		if (!Vector3_type)
			break;


		/* Vector3_dos: */
		desc->kind = DDS_STRUCTURE_TYPE;
		desc->name = "Vector3_dos";
		tb_vector = DDS_DynamicTypeBuilderFactory_create_type (desc);
		if (!tb_vector)
			break;

		md->name = "uno";
		md->id = md->index = 0;		
		md->type = Vector3_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_vector, md);
		if (rc)
			break;		

		md->name = "dos";
		md->id = md->index = 1;		
		md->type = Vector3_type;
		rc = DDS_DynamicTypeBuilder_add_member (tb_vector, md);
		if (rc)
			break;		

		/* Finally create the Dynamic Type t. */
		Vector3_dos_type = DDS_DynamicTypeBuilder_build (tb_vector);
		if (!Vector3_dos_type)
			break;

		/* Create a Typesupport package from the type. */
		ts = DDS_DynamicTypeSupport_create_type_support (Vector3_dos_type);
	}
	while (0);

	if (md)
		DDS_MemberDescriptor__free (md);
	if (tb_uno)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_uno);
	if (tb_vector)
		DDS_DynamicTypeBuilderFactory_delete_type (tb_vector);
	if (desc)
		DDS_TypeDescriptor__free (desc);
	return (ts);
}


/* Vector3_dos_type_free -- Release the previously created typesupport data. */

void Vector3_dos_type_free (DDS_DynamicTypeSupport ts)
{
	if (Vector3_dos_type) {
		DDS_DynamicTypeBuilderFactory_delete_type (Vector3_dos_type);
		DDS_DynamicTypeSupport_delete_type_support (ts);
		Vector3_dos_type = NULL;
	}
}

/* Vector3_dos_register -- Register a Vector3_dos in the DataWriter. */

DDS_InstanceHandle_t Vector3_dos_register (DDS_DynamicDataWriter  dw,
				       Vector3_dos_t              *data)
{
	DDS_DynamicData	d;
	DDS_ReturnCode_t rc;
	DDS_InstanceHandle_t h = 0;

	d = DDS_DynamicDataFactory_create_data (Vector3_dos_type);
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

/* Vector3_dos_write -- Write a message on the dynamic type writer. */

DDS_ReturnCode_t Vector3_dos_write (DDS_DynamicDataWriter  dw,
				Vector3_dos_t              *data,
				DDS_InstanceHandle_t   h)
{
	DDS_DynamicData	d_uno;
	DDS_DynamicData	d_dos;
	DDS_DynamicData	d_vector3_dos;
	DDS_ReturnCode_t rc;

	do {

	    /* Vector3: uno */
	    d_uno = DDS_DynamicDataFactory_create_data (Vector3_type);
	    if (!d_uno)
	        return (DDS_RETCODE_OUT_OF_RESOURCES);      
	    rc = DDS_DynamicData_set_float64_value (d_uno, X_ID_uno, data->uno.x_);
	    if (rc)
	        break;
	    rc = DDS_DynamicData_set_float64_value (d_uno, Y_ID_uno, data->uno.y_);
	    if (rc)
	        break;
	    rc = DDS_DynamicData_set_float64_value (d_uno, Z_ID_uno, data->uno.z_);
	    if (rc)
	        break;

	    /* Vector3: dos */
	    d_dos = DDS_DynamicDataFactory_create_data (Vector3_type);
	    if (!d_dos)
	        return (DDS_RETCODE_OUT_OF_RESOURCES);      
	    rc = DDS_DynamicData_set_float64_value (d_dos, X_ID_uno, data->dos.x_);
	    if (rc)
	        break;
	    rc = DDS_DynamicData_set_float64_value (d_dos, Y_ID_uno, data->dos.y_);
	    if (rc)
	        break;
	    rc = DDS_DynamicData_set_float64_value (d_dos, Z_ID_uno, data->dos.z_);
	    if (rc)
	        break;

	    /* _Vector3_dos_st */
		d_vector3_dos = DDS_DynamicDataFactory_create_data (Vector3_dos_type);
		if (!d_vector3_dos)
		    return (DDS_RETCODE_OUT_OF_RESOURCES);
	    rc = DDS_DynamicData_set_complex_value (d_vector3_dos, uno_id, d_uno);
	    if (rc)
	        break;
	    rc = DDS_DynamicData_set_complex_value (d_vector3_dos, dos_id, d_dos);
	    if (rc)
	        break;

	    /* Write everything over the network */
		rc = DDS_DynamicDataWriter_write (dw, d_vector3_dos, h);
	}
	while (0);

	DDS_DynamicDataFactory_delete_data (d_vector3_dos);
	return (rc);
}

/* Vector3_dos_signal -- Indicate a vector3 signal on the dynamic type writer. */
DDS_ReturnCode_t Vector3_dos_signal (DDS_DynamicDataWriter  dw,
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

static DDS_ReturnCode_t get_datatype (DDS_DynamicData d, Vector3_dos_t *s)
{
	DDS_ReturnCode_t rc;
	DDS_DynamicData	d_uno;
	DDS_DynamicData	d_dos;
	DDS_DynamicData	d_vector3_dos;

	/* Vector3: uno */
	rc = DDS_DynamicData_get_complex_value(d, &d_uno, uno_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_uno, &s->uno.x_, X_ID_uno);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_uno, &s->uno.y_, Y_ID_uno);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_uno, &s->uno.z_, Z_ID_uno);
	if (rc)
		return (rc);

	/* Vector3: dos */
	rc = DDS_DynamicData_get_complex_value(d, &d_dos, dos_id);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_dos, &s->dos.x_, X_ID_uno);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_dos, &s->dos.y_, Y_ID_uno);
	if (rc)
		return (rc);
	rc = DDS_DynamicData_get_float64_value (d_dos, &s->dos.z_, Z_ID_uno);
	if (rc)
		return (rc);

	return (DDS_RETCODE_OK);
}

/* Vector3_dos_read -- Dynamically read a Vector3_dos_t data item. */

DDS_ReturnCode_t Vector3_dos_read_or_take (DDS_DynamicDataReader dr,
				       Vector3_dos_t             *data,
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
			d = DDS_DynamicDataFactory_create_data (Vector3_dos_type);
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

/* Vector3_dos_cleanup -- Cleanup dynamic message data. */

void Vector3_dos_cleanup (Vector3_dos_t *data)
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


