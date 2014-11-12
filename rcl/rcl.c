
#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include "thread.h"
#include "libx.h"
#include "tty.h"
#include "chat_msg.h"
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <apps/netutils/netlib.h>

#include "rcl.h"
#include "aux/chat_msg.h"

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
char						user_name [64];				/* User name. */
unsigned					domain_id= 0;				/* Domain identifier. */
int							verbose = 0;
thread_t					rt;

/* 
	Initizalize the ROS 2 client library for embedded

		the code basically configures the network interface
		according to the NuttX defined variables CONFIG_EXAMPLES_UDP_*.
		In order to modify them you'll need to interact with the
		NuttX kernel through "make menuconfig".
*/
void rcl_init(void) /* equivalent to init() */
{
	/* Configure the network
		this values should be defined in the NuttX configuration
	*/
	/* host address */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_IPADDR);
	netlib_sethostaddr("eth0", &addr);

	/* default router address */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_DRIPADDR);
	netlib_setdraddr("eth0", &addr);

	/* subnet mask */
	addr.s_addr = HTONL(CONFIG_EXAMPLES_UDP_NETMASK);
	netlib_setnetmask("eth0", &addr);
	
	if (verbose)	
		printf("rcl init()\n");
}

/*
	Create a ROS 2 embedded node

		Internally a ROS 2 node consists of a domain participant created
		in a specific domain.
*/
void create_node(void)
{
	if (verbose)
		printf("create_node()\n");

	/* Create a DDS entity, name hardcoded for now */
	DDS_entity_name ("ROS2Embedded-Tinq-Nuttx");
	/* Create a domain participant */
	part = DDS_DomainParticipantFactory_create_participant (domain_id, NULL, NULL, 0);
	if (!part) {
		printf ("create_node() can't create participant!\r\n");
		exit (1);
	}
	if (verbose)
		printf ("create_node() DDS Domain Participant created.\r\n");
}

/*	
	Creates a Publisher and a DataWriter within the Domain participant at the 
	topics created previously by init_types().

	- Return:
		the instantiated DataWriter

*/
void create_publisher(void)
{
	if (verbose)
		printf("create_publisher()\n");

	pub = DDS_DomainParticipant_create_publisher (part, NULL, NULL, 0);
	if (!pub) {
		printf ("create_publisher() DDS_DomainParticipant_create_publisher() failed!\r\n");
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
		printf ("create_publisher() unable to create message writer.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("create_publisher() DDS message writer created.\r\n");
}

/*
	Publish a message through the DataWritter
		TODO: generalize and get rid of ChatMsg
*/
void publish(char* text_to_publish)
{
	ChatMsg_t				m;
	char*					buf = text_to_publish;

	if (!h)
		h = ChatMsg_register (dw, &m);
	m.message = buf;	
	ChatMsg_write (dw, &m, h);
}

/*
	Create subscriber to a topic
*/
void create_subscriber(char* topic_name)
{
	/* TODO */
}

/*
	Takes the last message from the topic subscribed
*/
void take(void)
{
	/* TODO */
}

/*
	Waits for a topic to have available packages
	and returns which one of those listening topics
	have available packages
*/
void wait(int non_blocking)
{
	/* TODO */
}


/*
	Deletes the DataWritter
*/
void delete_publisher(void)
{
	DDS_Publisher_delete_datawriter (pub, dw);
	/* usleep (200000); */

	if (verbose)
		printf ("delete_publisher() DDS message writer deleted.\r\n");
}

/* 
	Deletes the domain participant and the rest of the entities that
	it contains.
*/
void delete_node(void)
{
	error = DDS_DomainParticipant_delete_contained_entities (part);
	if (verbose)
		printf ("delete_node() DDS Entities deleted (error = %u).\r\n", error);

	delete_types();	
	if (verbose)
		printf ("delete_node() types deleted.\r\n");	

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (verbose)
		printf ("delete_node() DDS Participant deleted (error = %u).\r\n", error);
}

/*
	Initialize the dynamic types to be used within RCL
		NOTE THAT THIS PROTOTYPE HARDCODES THIS INITIALIZATION USING
		DDS PRIMITIVES. IN FUTURE PROTOTYPES AN ABSTRACTION LAYER 
		SIMILAR TO THE ROS INTERFACES SHOULD BE CODED.

		Note also that ts, error, topic and td are global variables that should
		be populated.
*/
#include "aux/chat_msg.h"		
void init_types(void)
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
	if (verbose)
		printf ("DDS Topic type ('%s') registered.\r\n", "ChatMsg");

	topic = DDS_DomainParticipant_create_topic (part, "Chat", "ChatMsg", NULL, NULL, 0);
	if (!topic) {
		printf ("Can't register chat message type.\r\n");
		exit (1);
	}
	if (verbose)
		printf ("DDS ChatMsg Topic created.\r\n");

	td = DDS_DomainParticipant_lookup_topicdescription (part, "Chat");
	if (!td) {
		printf ("Can't get topicdescription.\r\n");
		exit (1);
	}	
}

/*
	Deletes the allocated dynamic types
*/
void delete_types(void)
{
	ChatMsg_type_free (ts);
}
