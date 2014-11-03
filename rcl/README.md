rcl for embedded
=================

The ROS Client Library for embedded devices has been built using the following building blocks:

```
----------------------------------
|               rcl               |
----------------------------------
|            DDS (Tinq)           |  DDS Layer
----------------------------------
|          RTOS (NuttX)           |  RTOS Layer
__________________________________

|        Hardware (STM32F4)       |  Hardware Layer
----------------------------------
```

For now, thew implementation focuses in the configuration described above using the NuttX RTOS and Tinq's DDS. 

Ideally, `rcl` will me modified to support a different set of RTOS/DDS options.