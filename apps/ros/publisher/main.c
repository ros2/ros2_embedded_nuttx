/*
	ROS 2 Embedded Publisher Example
*/

#include "rcl.h"

int ros_main(int argc, char *argv[])
{
	char buf[256];

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
	for (;;){
		sprintf(buf, "Hola ROS 2, ¿como estás %d?\n", i++);	
		publish(buf);
		printf(buf);
	}

}

