#ifndef RCL_RCL_RCL_H_
#define RCL_RCL_RCL_H_

typedef enum {RCL_MSG_TYPE_STRING, RCL_MSG_TYPE_INT} rcl_msg_type;

#define false 0
#define true 1

typedef struct rcl_node_t 
{
	char name[32]; // node name 
} rcl_node_t;

typedef struct rcl_publisher_t 
{
	rcl_node_t* node;
	char topic_name[32];
	rcl_msg_type msg_type;
} rcl_publisher_t;

typedef struct rcl_msg_t
{
	void* data;
	unsigned int length;
} rcl_msg_t;

typedef struct rcl_subscriber_t 
{
	rcl_node_t* node;
	char topic_name[32];
	rcl_msg_type msg_type;
	void (*callback)(rcl_msg_t* msg); // subscriber->callback = callback if there's a function void callback(rcl_msg_t* msg) defined.
} rcl_subscriber_t;

void rcl_init(void);

void rcl_create_node(rcl_node_t* node, const char* name);

void rcl_create_publisher(rcl_publisher_t* publisher, rcl_node_t* node, const char* topic_name, rcl_msg_type msg_type);

void rcl_publish(rcl_publisher_t* publisher, rcl_msg_t* msg);

void rcl_create_subscriber(rcl_subscriber_t* subscriber, rcl_node_t* node, const char* topic_name, rcl_msg_type msg_type, void (*callback)(rcl_msg_t* msg));

void rcl_set_loop_rate(unsigned int rate); // loop rate in millis

unsigned int rcl_ok();

void rcl_loop_sleep();

void rcl_ros_info(const char *fmt, ...);

#endif /* RCL_RCL_RCL_H_ */