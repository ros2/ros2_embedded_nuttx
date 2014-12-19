/*
   	Copyright 2014 Open Source Robotics Foundation, Inc.
	Apache License Version 2.0
		
		Coded by Víctor Mayoral Vilches.
 */

/* imu_msg.h -- Message type as used in the vector3 application. */

#ifndef __imu_msg_h_
#define	__imu_msg_h_

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

typedef struct Quaternion_
{
  double x;
  double y;
  double z;
  double w;

} Quaternion_t ;

typedef struct _Vector3__st
{
    double x_;
    double y_;
    double z_;
} Vector3_t;

typedef struct Imu_
{

  Header_t header;

  Quaternion_t orientation;
  double orientation_covariance[9];

  Vector3_t angular_velocity;
  double angular_velocity_covariance[9];

  Vector3_t linear_acceleration;
  double linear_acceleration_covariance[9];

} Imu_t; // struct Imu_


/* Create a new Imu_t type.  If errors occurred, a non-0 error code is
   returned, otherwise *tp and *tsp will be set to the proper DynamicType and
   TypeSupport data. */
DDS_DynamicTypeSupport Imu_type_new (void);


/* Release the previously created TypeSupport data. */
void Imu_type_free (DDS_DynamicTypeSupport ts);


DDS_InstanceHandle_t Imu_register (DDS_DynamicDataWriter  dw,
				       Imu_t              *data);

/* Write a vector3 message on the dynamic type writer. */
DDS_ReturnCode_t Imu_write (DDS_DynamicDataWriter dw,
				Imu_t             *data,
				DDS_InstanceHandle_t  h);


/* Indicate a vector3 signal on the dynamic type writer.
   If unreg is set, the signal indicates that the writer has finished
   writing.  Otherwise it indicates the writer is cleaning up its data. */
DDS_ReturnCode_t Imu_signal (DDS_DynamicDataWriter dw,
				 DDS_InstanceHandle_t  h,
				 int                   unreg);


/* Dynamically read or take a Imu_t data item. */
DDS_ReturnCode_t Imu_read_or_take (DDS_DynamicDataReader dr,
				       Imu_t             *data,
				       DDS_SampleStateMask   ss,
				       DDS_ViewStateMask     vs,
				       DDS_InstanceStateMask is,
				       int                   take,
				       int                   *valid,
				       DDS_InstanceStateKind *kind);


/* Cleanup dynamic message data. */
void Imu_cleanup (Imu_t *data);


#endif /* __imu_msg_h_ */

