/*
   	Copyright 2014 Open Source Robotics Foundation, Inc.
	Apache License Version 2.0
		
		Coded by Víctor Mayoral Vilches.
 */

/* imu32_msg.h -- Message type as used in the vector3 application. */

#ifndef __vector3_msg_h_
#define	__vector3_msg_h_

#include "dds/dds_dcps.h"
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"

#define FRAMEID_LEN	100

typedef struct Time_
{
  int32_t sec;
  uint32_t nanosec;
} Time_t; // struct Time_


typedef struct Header_
{
  uint32_t seq;
  Time_t stamp;
  char frame_id[FRAMEID_LEN];
} Header_t;

typedef struct Quaternion32_
{
  float x;
  float y;
  float z;
  float w;

} Quaternion32_t ;

typedef struct _Vector3__st
{
    float x_;
    float y_;
    float z_;
} Vector3_t;

typedef struct Imu32_
{

  Header_t header;

  Quaternion32_t orientation;
  float orientation_covariance[9];

  Vector3_t angular_velocity;
  float angular_velocity_covariance[9];

  Vector3_t linear_acceleration;
  float linear_acceleration_covariance[9];

} Imu32_t; // struct Imu32_


/* Create a new Imu32_t type.  If errors occurred, a non-0 error code is
   returned, otherwise *tp and *tsp will be set to the proper DynamicType and
   TypeSupport data. */
DDS_DynamicTypeSupport Imu32_type_new (void);


/* Release the previously created TypeSupport data. */
void Imu32_type_free (DDS_DynamicTypeSupport ts);


DDS_InstanceHandle_t Imu32_register (DDS_DynamicDataWriter  dw,
				       Imu32_t              *data);

/* Write a vector3 message on the dynamic type writer. */
DDS_ReturnCode_t Imu32_write (DDS_DynamicDataWriter dw,
				Imu32_t             *data,
				DDS_InstanceHandle_t  h);


/* Indicate a vector3 signal on the dynamic type writer.
   If unreg is set, the signal indicates that the writer has finished
   writing.  Otherwise it indicates the writer is cleaning up its data. */
DDS_ReturnCode_t Imu32_signal (DDS_DynamicDataWriter dw,
				 DDS_InstanceHandle_t  h,
				 int                   unreg);


/* Dynamically read or take a Imu32_t data item. */
DDS_ReturnCode_t Imu32_read_or_take (DDS_DynamicDataReader dr,
				       Imu32_t             *data,
				       DDS_SampleStateMask   ss,
				       DDS_ViewStateMask     vs,
				       DDS_InstanceStateMask is,
				       int                   take,
				       int                   *valid,
				       DDS_InstanceStateKind *kind);


/* Cleanup dynamic message data. */
void Imu32_cleanup (Imu32_t *data);


#endif /* __imu32_msg_h_ */

