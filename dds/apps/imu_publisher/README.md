# DDS IMU example

This example is meant to be a prove of concept of the interoperability between Tinq and other DDS implementations (refer to https://github.com/ros2/examples for an imu implementation within OpenSplice and Connext)



The code uses the idl application (located at `../idl`) to generate source files from an IDL message type `Vector.idl`:

```

module simple_msgs
{

module dds_
{




struct Vector3_
{

  double x_;
  double y_;
  double z_;

};  // struct Vector3_

//#pragma keylist Vector3_

};  // module dds_

};  // module simple_msgs

```

The commands used for these purpose are:

```bash
cd ..
cd idl
./idlparser -o out.c -t examples/Vector3.idl
```

