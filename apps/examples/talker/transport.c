#include "transport.h"
#include <stdio.h>
#include "tty.h"
#include <arpa/inet.h>
#include "chat_msg.h"

//#include <nuttx/config.h>
//#include <stdlib.h>
//#include <string.h>
//#include <ctype.h>
//#include <unistd.h>
//#include <poll.h>
//#include "thread.h"
//#include "libx.h"
//#include "dds/dds_aux.h"
//#include "dds/dds_debug.h"
//#include <netinet/in.h>
//#include <apps/netutils/netlib.h>


#define HISTORY		1	/* # of samples buffered. */

DDS_DomainParticipant		part;
DDS_DynamicTypeSupport		ts;
DDS_Publisher				pub;
DDS_Subscriber				sub;
DDS_Topic					topic;
DDS_TopicDescription		td;
DDS_DynamicDataWriter		dw;
DDS_DynamicDataReader		dr;
DDS_DataWriterQos 			wr_qos;
DDS_DataReaderQos			rd_qos;
DDS_ReturnCode_t			error;
DDS_InstanceHandle_t		h;

struct in_addr 				addr;


const char					*progname;
char						chatroom [64] = "DDS";		/* Chatroom name. */
char						user_name [64] = "username";				/* User name. */
unsigned					domain_id= 0;				/* Domain identifier. */
int							verbose = 0;
thread_t					rt;
ChatMsg_t m;

void transport_init()
{
	
	/* Configure the network */
	/* Set up our host address */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_IPADDR);
	netlib_sethostaddr("eth0", &addr);

	/* Set up the default router address */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_DRIPADDR);
	netlib_setdraddr("eth0", &addr);

	/* Setup the subnet mask */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_NETMASK);
	netlib_setnetmask("eth0", &addr);


}

void transport_create_node(const char* name)
{
	/* Create a DDS entity, name hardcoded for now */
	//DDS_entity_name ("Technicolor Chatroom");
	DDS_entity_name (name);

	part = DDS_DomainParticipantFactory_create_participant (domain_id, NULL, NULL, 0);
	if (!part) {
		printf ("Can't create participant!\r\n");
		exit (1);
	}


}


void transport_create_publisher(const char* topic_name)
{
	ts = ChatMsg_type_new ();
	if (!ts) {
		printf ("Can't create chat message type!\r\n");
		exit (1);
	}
	error = DDS_DynamicTypeSupport_register_type (ts, part, "ChatMsg");
	if (error) {
		printf ("Can't register chat message type.\r\n");
		exit (1);
	}
	
	//topic = DDS_DomainParticipant_create_topic (part, "Chat", "ChatMsg", NULL, NULL, 0);
	topic = DDS_DomainParticipant_create_topic (part, topic_name, "ChatMsg", NULL, NULL, 0);
	if (!topic) {
		printf ("Can't register chat message type.\r\n");
		exit (1);
	}

	//td = DDS_DomainParticipant_lookup_topicdescription (part, "Chat");
	td = DDS_DomainParticipant_lookup_topicdescription (part, topic_name);
	if (!td) {
		printf ("Can't get topicdescription.\r\n");
		exit (1);
	}
	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		printf ("DDS_DomainParticipant_create_publisher () failed!\r\n");
		exit (1);
	}

	DDS_Publisher_get_default_datawriter_qos (pub, &wr_qos);
#ifdef TRANSIENT_LOCAL
	wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
#endif
#ifdef RELIABLE
	wr_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
#endif
#ifdef KEEP_ALL
	wr_qos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
	wr_qos.history.depth = DDS_LENGTH_UNLIMITED;
	wr_qos.resource_limits.max_samples_per_instance = HISTORY;
	wr_qos.resource_limits.max_instances = HISTORY * 10;
	wr_qos.resource_limits.max_samples = HISTORY * 4;
#else
	wr_qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
	wr_qos.history.depth = HISTORY;
#endif
	/* Create a Data Writer. */
	dw = DDS_Publisher_create_datawriter (pub, topic, &wr_qos, NULL, 0);
	if (!dw) {
		printf ("Unable to create chat message writer.\r\n");
		exit (1);
	}

	m.chatroom = chatroom;
	m.from = user_name;
}

void transport_publish_string(const char* topic_name, const char* msg_string)
{
	//printf("Publishing %s to topic %s.\n", msg_string, topic_name);

	h = ChatMsg_register (dw, &m);
	m.message = msg_string;
	ChatMsg_write (dw, &m, h);
}