#include "rcl.h"
#include "asw.h"

RUNNABLE talker(void* params)
{
  rcl_init(); // Initialize ROS client library 

  rcl_node_t node;
  rcl_create_node(&node, "talker"); // Register node with the name 'talker' in RCL.
  rcl_publisher_t chatter_pub;
  rcl_create_publisher(&chatter_pub, &node, "chatter", RCL_MSG_TYPE_STRING); // Tell the master that we are going to be publishing a message of type simple_msgs/String on the topic chatter. Buffer length 7 (to do later!).

  rcl_set_loop_rate(5); // Set the period (here 50 Hz) for specifying how long loop_rate.sleep() will sleep in the loop later.

  rcl_msg_t msg; // instantiate the message object
  int count = 0;

  while (rcl_ok()) {
    char data[10];
    sprintf(data, "%d", ++count);
    msg.data = (void*)data;
    msg.length = strlen(data);
    rcl_ros_info("Publishing: '%s'", msg.data);
    rcl_publish(&chatter_pub, &msg); // broadcast the message to anyone who is connected
    //rcl_spin_some(&node); // Not necessary here, but if we were to add a subscriber, the callback wouldn't be called without this.
    rcl_loop_sleep(); // Sleep for the previosly specified period
  }
}