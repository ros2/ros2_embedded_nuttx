#ifndef __ros2_embedded__rcl__h__
#define __ros2_embedded__rcl__h__

void rcl_init(void);
void create_node(void);

void create_publisher(void);
void publish(char* text_to_publish);

void create_subscriber(char* topic_name);
void take(void);
void wait(int non_blocking);

void delete_publisher(void);
void delete_node(void);

/* TODO: ROS types */
void init_types(void);
void delete_types(void);


#endif  // __ros2_embedded__rcl__h__