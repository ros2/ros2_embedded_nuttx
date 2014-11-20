/*
   	Copyright 2014 Open Source Robotics Foundation, Inc.
	Apache License Version 2.0
		
		Coded by Víctor Mayoral Vilches.
 */

/* vector3_msg.h -- Message type as used in the vector3 application. */

#ifndef __vector3_msg_h_
#define	__vector3_msg_h_

#include "dds/dds_dcps.h"
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"

typedef struct _Vector3__st
{
    double x_;
    double y_;
    double z_;
} Vector3_t;

/* Create a new Vector3_t type.  If errors occurred, a non-0 error code is
   returned, otherwise *tp and *tsp will be set to the proper DynamicType and
   TypeSupport data. */
DDS_DynamicTypeSupport Vector3_type_new (void);


/* Release the previously created TypeSupport data. */
void Vector3_type_free (DDS_DynamicTypeSupport ts);


DDS_InstanceHandle_t Vector3_register (DDS_DynamicDataWriter  dw,
				       Vector3_t              *data);

/* Write a vector3 message on the dynamic type writer. */
DDS_ReturnCode_t Vector3_write (DDS_DynamicDataWriter dw,
				Vector3_t             *data,
				DDS_InstanceHandle_t  h);


/* Indicate a vector3 signal on the dynamic type writer.
   If unreg is set, the signal indicates that the writer has finished
   writing.  Otherwise it indicates the writer is cleaning up its data. */
DDS_ReturnCode_t Vector3_signal (DDS_DynamicDataWriter dw,
				 DDS_InstanceHandle_t  h,
				 int                   unreg);


/* Dynamically read or take a Vector3_t data item. */
DDS_ReturnCode_t Vector3_read_or_take (DDS_DynamicDataReader dr,
				       Vector3_t             *data,
				       DDS_SampleStateMask   ss,
				       DDS_ViewStateMask     vs,
				       DDS_InstanceStateMask is,
				       int                   take,
				       int                   *valid,
				       DDS_InstanceStateKind *kind);


/* Cleanup dynamic message data. */
void Vector3_cleanup (Vector3_t *data);


#endif /* __vector3_msg_h_ */

