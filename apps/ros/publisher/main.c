/*
	ROS 2 Embedded Publisher Example
*/

#include "rcl.h"

int ros_main(int argc, char *argv[])
{
	/* Init the ROS Client Library */
	rcl_init();
	
	/* Create a ROS node */
	create_node();

	/* Init the dynamic types 
		TODO: abstract this code
	*/
	init_types();

	/* Create a publisher
		TODO: specify message types, topics, etc.
	*/
	create_publisher();

	int i;
	for (i=0; i<10; i++){
		publish("Hola ROS 2, ¿como estás?");
		printf("Hola ROS 2, ¿como estás %d?\n",i);
	}

}

