#include <stdarg.h>
#include <string.h>
#include "rcl.h"
#include "os.h"
#include "transport.h"

unsigned int m_loop_rate = 1000; // 1 Hz

void rcl_init(void)
{
	transport_init();
}

void rcl_create_node(rcl_node_t* node, const char* name)
{
	//char * strcpy ( char * destination, const char * source );
	strcpy(node->name, name);
	transport_create_node(name);
}

void rcl_create_publisher(rcl_publisher_t* publisher, rcl_node_t* node, const char* topic_name, rcl_msg_type msg_type)
{
	publisher->node = node;
	strcpy(publisher->topic_name, topic_name);
	publisher->msg_type = msg_type;
	transport_create_publisher(topic_name);
}

void rcl_publish(rcl_publisher_t* publisher, rcl_msg_t* msg)
{
	switch(publisher->msg_type)
	{
		case RCL_MSG_TYPE_STRING: 
			;
			const char* msg_string = (const char*)msg->data;
			transport_publish_string(publisher->topic_name, msg_string);
		break;
		default:
		break;
	}
}

void rcl_create_subscriber(rcl_subscriber_t* subscriber, rcl_node_t* node, const char* topic_name, rcl_msg_type msg_type, void (*callback)(rcl_msg_t* msg))
{
	subscriber->node = node;
	strcpy(subscriber->topic_name, topic_name);
	subscriber->msg_type = msg_type;
	subscriber->callback = callback;
}

void rcl_set_loop_rate(unsigned int rate)
{
	m_loop_rate = rate;
}

unsigned int rcl_ok()
{
	return true;
}

void rcl_loop_sleep()
{
	os_sleep((unsigned int)(1000000.0f / (float)m_loop_rate));
}

void rcl_ros_info(const char *fmt, ...)
{
	//os_printf();
	//va_start(args, fmt);
	//vprintf(fmt, args);
	//va_end(args);
}