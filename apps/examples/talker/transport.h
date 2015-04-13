void transport_init();

void transport_create_node(const char* name);

void transport_create_publisher(const char* topic_name);

void transport_publish_string(const char* topic_name, const char* msg_string);