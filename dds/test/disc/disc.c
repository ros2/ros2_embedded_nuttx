/*
 * Copyright (c) 2014 - Qeo LLC
 *
 * The source code form of this Qeo Open Source Project component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the Clear BSD
 * License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
 * for more details.
 *
 * The Qeo Open Source Project also includes third party Open Source Software.
 * See LICENSE file for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <dds/dds_dcps.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>



#define DOMAIN_ID 0
#define MAX_PARTICIPANTS 500
#define NCREATORS 1 
#define EARLY_CREATORS 0
#define NREADERS 5
#define NEARLY_READERS 0

typedef struct msg_data_st {
	uint64_t	counter;
	uint32_t	key;
	char		message [10];
} MsgData_t;

static DDS_TypeSupport_meta msg_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT, 1, "HelloWorldData", sizeof (struct msg_data_st), 0, 3, 0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "counter", 0, offsetof (struct msg_data_st, counter), 0, 0, NULL },
	{ CDR_TYPECODE_ULONG,  1, "key", 0, offsetof (struct msg_data_st, key), 0, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 0 , "message", 10, offsetof (struct msg_data_st, message), 0, 0, NULL }
};

static DDS_TypeSupport	*dds_HelloWorld_ts;

DDS_ReturnCode_t register_HelloWorldData_type (DDS_DomainParticipant part)
{
	DDS_ReturnCode_t	error;

	dds_HelloWorld_ts = DDS_DynamicType_register (msg_data_tsm);
        if (!dds_HelloWorld_ts)
                return (DDS_RETCODE_ERROR);

	error = DDS_DomainParticipant_register_type (part, dds_HelloWorld_ts, "HelloWorldData");
	return (error);
}

pid_t	 observer_pid;
sigset_t newmask, oldmask;
int	 received_signal = 0;

static void
interrupt_handler (int sig)
{
	received_signal = sig;
}

static void
block (int times)
{
	int i;
	for (i = 0; i < times; i++) {
		/* Block until we receive a relevant signal */
/* Install signal handlers */
                if (signal(SIGINT, interrupt_handler) == SIG_ERR)
                        fprintf(stderr, "Could not install signal handler\n");
                if (signal(SIGQUIT, interrupt_handler) == SIG_ERR)
                        fprintf(stderr, "Could not install signal handler\n");

		while (!received_signal)
			sigsuspend(&oldmask);

		/* Signal received, handle */
		if (received_signal == SIGQUIT)
		{
			fprintf(stderr, "Got SIGQUIT!\n");
			exit (EXIT_FAILURE);
		}

		received_signal = 0;

		printf("Unblocked %d %d/%d\n", getpid(), i + 1, times);
	}
}


char tmp_file [] = "/tmp/disc_queues.XXXXXX";
int  msqid = -1;

void
setup_queues ()
{
	key_t key;
	mkstemp(tmp_file);

	key = ftok(tmp_file, 'A');
	msqid = msgget(key, IPC_CREAT | 0644);
	if (msqid == -1) {
		fprintf(stderr, "Error creating message queue\n");
		exit (EXIT_FAILURE);
	}
}


struct msgbuf {
	long mtype;		/* message type, must be > 0 */
	char mtext [1];		/* message data */
};


void
send_message()
{
	struct msgbuf buf;

	buf.mtype = 1;
	if (msgsnd(msqid, &buf, 0, 0) == -1) {
		fprintf(stderr, "Failed to send message\n");
		exit(EXIT_FAILURE);
	}
}

void
receive_message(int times)
{
	int	      i;
	struct msgbuf buf;

	for (i = 0; i < times; i++) {
		buf.mtype = 1;
		if (msgrcv(msqid, &buf, 0, 1, 0) == -1) {
			fprintf(stderr, "Failed to send message\n");
			exit(EXIT_FAILURE);
		}
		printf("Observer received message %d/%d\n", i + 1,times);
	}
}


/* Observer {{{ */

static const char *builtin_names [] = {
	"DCPSParticipant",
	"DCPSTopic",
	"DCPSPublication",
	"DCPSSubscription"
};

pthread_mutex_t	  part_mutex = PTHREAD_MUTEX_INITIALIZER;
int		  nparts = 0;
int		  ntopics = 0;
int		  npublications = 0;
int		  nsubscriptions = 0;
short		  participant_pids [MAX_PARTICIPANTS];
int		  discovered = 0;


DDS_SEQUENCE(DDS_ParticipantBuiltinTopicData *,DDS_ParticipantBuiltinTopicDataSeq);
DDS_SEQUENCE(DDS_TopicBuiltinTopicData *,DDS_TopicBuiltinTopicDataSeq);
DDS_SEQUENCE(DDS_PublicationBuiltinTopicData *,DDS_PublicationBuiltinTopicDataSeq);
DDS_SEQUENCE(DDS_SubscriptionBuiltinTopicData *,DDS_SubscriptionBuiltinTopicDataSeq);

void
kick_creators(int lock)
{
	int i;

	if (lock)
		pthread_mutex_lock(&part_mutex);

	for (i = 0; i < nparts; i++) {
		printf("Kicking %d pid %d\n", i, participant_pids [i]);
		kill(participant_pids [i], SIGINT);
	}

	if (lock)
		pthread_mutex_unlock(&part_mutex);
}

#define ARG_NOT_USED(p) (void) p;

void
participant_info (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	DDS_ParticipantBuiltinTopicDataSeq rx_sample = DDS_SEQ_INITIALIZER (DDS_ParticipantBuiltinTopicData *);
	DDS_SampleInfoSeq		   rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask		   ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask		   vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask		   is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t		   error;

	ARG_NOT_USED(l)

	do {
		error = DDS_DataReader_read (dr, (DDS_DataSeq *) &rx_sample, &rx_info, 1, ss, vs, is);
		if (error == DDS_RETCODE_OK)
		{
			int state = 0;
			int i;
			if ((DDS_SEQ_ITEM(rx_info,0)->view_state & DDS_NEW_VIEW_STATE) != 0) {
				printf ("New ");
				state = 1;
			} else if (DDS_SEQ_ITEM(rx_info,0)->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				state = 2;
				printf ("Updated ");
			} else {
				printf ("Deleted ");
				state = 3;
			}

			if (DDS_SEQ_ITEM(rx_info,0)->valid_data) {
				printf("Participant %08x:%08x:%08x\n", ntohl (DDS_SEQ_ITEM(rx_sample, 0)->key.value [0]),
				       ntohl (DDS_SEQ_ITEM(rx_sample, 0)->key.value [1]),
				       ntohl (DDS_SEQ_ITEM(rx_sample, 0)->key.value [2]));

				if (state == 1) {
					int this_pid = ntohs((DDS_SEQ_ITEM(rx_sample, 0)->key.value [1] & 0xffff0000U) >> 16); 

					pthread_mutex_lock(&part_mutex);
					for (i = 0; i < MAX_PARTICIPANTS; i++)
						if (participant_pids [i] == this_pid)
							break;

					if (i < MAX_PARTICIPANTS) {
						discovered++;
						printf("Detected participant %d (%d/%d)\n", this_pid, discovered, nparts);
						fflush(stdout);

						if (discovered == nparts) {
							printf("Discovered them all, waiting until they are ready, then, kicking them all again\n");
							receive_message(NCREATORS);
							fflush(stdout);
							kick_creators(0);
						}

					}
					else {
						fprintf(stderr,"Unknown participant discovered: pid=%d\n", this_pid);
					}
					pthread_mutex_unlock(&part_mutex);
				}
			} else {
				DDS_ParticipantBuiltinTopicData tmp;
				DDS_DataReader_get_key_value (dr, &tmp, DDS_SEQ_ITEM(rx_info,0)->instance_handle);

				printf("Participant %08x:%08x:%08x\n", ntohl (tmp.key.value [0]),
				       ntohl (tmp.key.value [1]),
				       ntohl (tmp.key.value [2]));

				if (state == 3) {
					pthread_mutex_lock(&part_mutex);
					for (i = 0; i < MAX_PARTICIPANTS; i++) {
						int this_pid = ntohs((tmp.key.value [1] & 0xffff0000U) >> 16); 

						if (participant_pids [i] == this_pid)
							break;
					}
					if (i < MAX_PARTICIPANTS) {
						kill(participant_pids [i], SIGINT);
						participant_pids [i] = 0;
						nparts--;
					}
					pthread_mutex_unlock(&part_mutex);
				}
			}
			DDS_DataReader_return_loan (dr, (DDS_DataSeq *) &rx_sample, &rx_info);
		}
	} while (error == DDS_RETCODE_OK);
}

void
topic_info (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	DDS_TopicBuiltinTopicDataSeq rx_sample = DDS_SEQ_INITIALIZER (DDS_TopicBuiltinTopicData *);
	DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED(l)

        printf("Topic\n");
	/*dbg_printf ("do_read: got notification!\r\n");*/
        do {
                error = DDS_DataReader_read (dr, (DDS_DataSeq *) &rx_sample, &rx_info, 1, ss, vs, is);
		if (error == DDS_RETCODE_OK)
		{
                        if ((DDS_SEQ_ITEM(rx_info,0)->view_state & DDS_NEW_VIEW_STATE) != 0) {
                                printf ("New Topic\n");
                                ntopics++;

                                
                        } else if (DDS_SEQ_ITEM(rx_info,0)->instance_state == DDS_ALIVE_INSTANCE_STATE) {
                                ntopics++;
				printf ("Updated Topic (%d/%d)\n", ntopics, NCREATORS);
                                if (ntopics == NCREATORS * NREADERS) {
                                        printf("Discovered all topics, wait until creators are ready, then kick them\n");
                                        /*receive_message(NCREATORS);
                                        fflush(stdout);
                                        kick_creators(0); */
                                }
                        } else {
                                printf ("Deleted Topic\n");
                        }
                        DDS_DataReader_return_loan (dr, (DDS_DataSeq *) &rx_sample, &rx_info);
                }
                else if (error != DDS_RETCODE_NO_DATA)
                {
                        printf("Error reading data from builtin topic\n");
                }
        }
	while (error == DDS_RETCODE_OK); 
}

void
publication_info (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	DDS_PublicationBuiltinTopicDataSeq rx_sample = DDS_SEQ_INITIALIZER (DDS_PublicationBuiltinTopicData *);
	DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED(l)
	/*dbg_printf ("do_read: got notification!\r\n");*/
        do {
                error = DDS_DataReader_read (dr, (DDS_DataSeq *) &rx_sample, &rx_info, 1, ss, vs, is);
		if (error == DDS_RETCODE_OK)
		{
                        if ((DDS_SEQ_ITEM(rx_info,0)->view_state & DDS_NEW_VIEW_STATE) != 0) {
                                npublications++;
                                printf ("New Publication (%d/%d)\n",npublications, NCREATORS);

                                if (npublications == NCREATORS) {
                                        printf("Discovered all publications, wait until creators are ready, then kick them\n");
                                        receive_message(NCREATORS);
                                        fflush(stdout);
                                        kick_creators(0);
                                }
                        } else if (DDS_SEQ_ITEM(rx_info,0)->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				printf ("Updated Publication\n");
                        } else {
                                printf ("Deleted Publication\n");
                        }
                        DDS_DataReader_return_loan (dr, (DDS_DataSeq *) &rx_sample, &rx_info);
                }
                else if (error != DDS_RETCODE_NO_DATA)
                {
                        printf("Error reading data from builtin publication info: %d\n", error);
                }
        }
	while (error == DDS_RETCODE_OK);
}

void
subscription_info (DDS_DataReaderListener *l, DDS_DataReader dr)
{
	DDS_SubscriptionBuiltinTopicDataSeq rx_sample = DDS_SEQ_INITIALIZER (DDS_SubscriptionBuiltinTopicData *);
	DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_ReturnCode_t	error;

	ARG_NOT_USED(l)

	/*dbg_printf ("do_read: got notification!\r\n");*/
        do {
                error = DDS_DataReader_read (dr, (DDS_DataSeq *) &rx_sample, &rx_info, 1, ss, vs, is);
		if (error == DDS_RETCODE_OK)
		{
                        if ((DDS_SEQ_ITEM(rx_info,0)->view_state & DDS_NEW_VIEW_STATE) != 0) {
                                nsubscriptions++;
                                printf ("New Subscription (%d/%d)\n",nsubscriptions, NCREATORS * NREADERS);

                                if (nsubscriptions == NCREATORS * NREADERS) {
                                        printf("Discovered all subscriptions, wait until creators are ready, then kick them\n");
                                        receive_message(NCREATORS);
                                        fflush(stdout);
                                        kick_creators(0);
                                }
                        } else if (DDS_SEQ_ITEM(rx_info,0)->instance_state == DDS_ALIVE_INSTANCE_STATE) {
				printf ("Updated Subscription\n");
                        } else {
                                printf ("Deleted Subscription\n");
                        }
                        DDS_DataReader_return_loan (dr, (DDS_DataSeq *) &rx_sample, &rx_info);
                }
                else if (error != DDS_RETCODE_NO_DATA)
                {
                        printf("Error reading data from builtin subscription info: %d\n", error);
                }
        }
	while (error == DDS_RETCODE_OK);
}

static DDS_DataReaderListener builtin_listeners [] = { {
							       NULL,	/* Sample rejected. */
							       NULL,	/* Liveliness changed. */
							       NULL,	/* Requested Deadline missed. */
							       NULL,	/* Requested incompatible QoS. */
							       participant_info,/* Data available. */
							       NULL,	/* Subscription matched. */
							       NULL,	/* Sample lost. */
							       NULL	/* Cookie */
						       }, {
							       NULL,	/* Sample rejected. */
							       NULL,	/* Liveliness changed. */
							       NULL,	/* Requested Deadline missed. */
							       NULL,	/* Requested incompatible QoS. */
							       topic_info,	/* Data available. */
							       NULL,	/* Subscription matched. */
							       NULL,	/* Sample lost. */
							       NULL	/* Cookie */
						       }, {
							       NULL,	/* Sample rejected. */
							       NULL,	/* Liveliness changed. */
							       NULL,	/* Requested Deadline missed. */
							       NULL,	/* Requested incompatible QoS. */
							       publication_info,/* Data available. */
							       NULL,	/* Subscription matched. */
							       NULL,	/* Sample lost. */
							       NULL	/* Cookie */
						       }, {
							       NULL,	/* Sample rejected. */
							       NULL,	/* Liveliness changed. */
							       NULL,	/* Requested Deadline missed. */
							       NULL,	/* Requested incompatible QoS. */
							       subscription_info,	/* Data available. */
							       NULL,	/* Subscription matched. */
							       NULL,	/* Sample lost. */
							       NULL	/* Cookie */
						       } };

void observer ()
{
	DDS_DomainParticipant participant;
	DDS_Subscriber	      builtin_subscriber;
	DDS_DataReader	      dr;
	DDS_ReturnCode_t      error;
        DDS_Topic             topic;
        DDS_Publisher publisher;
        DDS_DataWriter writer;
        DDS_Subscriber subscriber = NULL;
        DDS_DataReader reader;
	unsigned int          i;

	receive_message (EARLY_CREATORS);

	participant = DDS_DomainParticipantFactory_create_participant (DOMAIN_ID, NULL, NULL, 0);
	if (!participant) {
		fprintf (stderr, "Could not create domain participant\n");
		exit (EXIT_FAILURE);
	}

	printf("Observer participant created\n");

	builtin_subscriber = DDS_DomainParticipant_get_builtin_subscriber (participant);
	if (!builtin_subscriber) {
		fprintf (stderr, "Could not get builtin subscriber\n");
		exit (EXIT_FAILURE);
	}

	for (i = 0; i < sizeof (builtin_names) / sizeof (char *); i++) {
		dr = DDS_Subscriber_lookup_datareader (builtin_subscriber, builtin_names [i]);
		if (!dr)
			fprintf (stderr, "DDS_Subscriber_lookup_datareader (%s) returned an error!\n", builtin_names [i]);

		error = DDS_DataReader_set_listener (dr, &builtin_listeners [i], DDS_DATA_AVAILABLE_STATUS);
		if (error)
			fprintf (stderr, "Could not set data reader listener for builtin reader %s.\nError Code = %d\n", builtin_names [i], error);
	}

        error = register_HelloWorldData_type (participant);
        if (error) {
                fprintf (stderr, "Could not register type\n");
		exit (EXIT_FAILURE);
        }

        /* Create a topic */    
        topic = DDS_DomainParticipant_create_topic (participant, "HelloWorld0", "HelloWorldData", NULL, NULL, 0);


        publisher = DDS_DomainParticipant_create_publisher (participant, NULL, NULL, 0);

	if (!publisher) {
		fprintf (stderr, "Could not create publisher\n");
		exit (EXIT_FAILURE);
	}
	
        writer = DDS_Publisher_create_datawriter (publisher, topic, NULL, NULL, 0);

        if (!writer) {
		fprintf (stderr, "Could not create data writer\n");
		exit (EXIT_FAILURE);
	}

        subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0);

	if (!subscriber) {
		fprintf (stderr, "Could not create subscriber\n");
		exit (EXIT_FAILURE);
	}

        reader = DDS_Subscriber_create_datareader (subscriber, topic, NULL, NULL, 0);

        if (!reader) {
                fprintf (stderr, "Could not create data reader\n");
                exit (EXIT_FAILURE);
        }


	kick_creators(1);

	for (i = 0; i < NCREATORS; i++) {
		printf("=============================================> %d\n",wait(NULL));
	}

	error = DDS_DomainParticipant_delete_contained_entities (participant);

	if (error) {
		fprintf (stderr, "Could not delete contained entities.\nError code = %d\n", error);
		exit (EXIT_FAILURE);
	}

	error = DDS_DomainParticipantFactory_delete_participant (participant);
	if (error) {
		fprintf (stderr, "Could not delete domain participant.\nError code = %d\n", error);
		exit (EXIT_FAILURE);
	}

	msgctl(msqid, IPC_RMID, NULL);
	unlink(tmp_file);

	exit(EXIT_SUCCESS);
}
/* }}} */

/* Creators {{{ */

void
kick_observer()
{
	kill(observer_pid, SIGINT);
}

int early_creator = 0;
int creator_id = 1;

void creator ()
{
	DDS_DomainParticipant	participant;
        DDS_Publisher		publisher;
        DDS_DataWriter		writer;
        DDS_Subscriber		subscriber = NULL;
        DDS_DataReader		reader[NREADERS];
        DDS_DataReader		dreader;
	DDS_ReturnCode_t	error;
	DDS_Topic		dtopic;
        DDS_Topic		topic[NREADERS];
	int early_readers = 0;
        int i;

        /*close(1);
        sprintf(outname,"stdout.%d", creator_id);
        open(outname,O_CREAT|O_TRUNC|O_WRONLY, 0666);
        close(2);
        sprintf(outname,"stderr.%d", creator_id);
        open(outname,O_CREAT|O_TRUNC|O_WRONLY, 0666); */


	if (!early_creator) {
		printf("Creator %d started, blocks at entry (late creator)\n", creator_id);
		block(1);
	} else {
		printf("Creator %d started, creating domain participant (early creator)\n", creator_id);
        }

	/* Create the participant */
	participant = DDS_DomainParticipantFactory_create_participant (DOMAIN_ID, NULL, NULL, 0);
	if (!participant) {
		fprintf (stderr, "Could not create domain participant\n");
		exit (EXIT_FAILURE);
	}

	error = register_HelloWorldData_type (participant);
        if (error) {
                fprintf (stderr, "Could not register type\n");
		exit (EXIT_FAILURE);
        }
	
	dtopic = DDS_DomainParticipant_create_topic (participant, "DeletedData", "HelloWorldData", NULL, NULL, 0);

	/* Create some topics */    
        for (i=0; i< NREADERS; i++)
        {
                char name[30];
                sprintf(name, "HelloWorld%d",i);
                topic[i] = DDS_DomainParticipant_create_topic (participant, name, "HelloWorldData", NULL, NULL, 0);
        }


	if (early_creator) {
		early_readers = NEARLY_READERS;
		subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0);
		if (!subscriber) {
			fprintf (stderr, "Could not create subscriber\n");
			exit (EXIT_FAILURE);
		}
		

		for (i=0; i< NEARLY_READERS; i++)
		{
			dreader = DDS_Subscriber_create_datareader (subscriber, dtopic, NULL, NULL, 0);
			DDS_Subscriber_delete_datareader (subscriber, dreader);
			reader[i] = DDS_Subscriber_create_datareader (subscriber, topic[i], NULL, NULL, 0);

			if (!reader[i]) {
				fprintf (stderr, "Could not create data reader\n");
				exit (EXIT_FAILURE);
			}
		}

		printf("Creator %d ready, kicking observer (early creator)\n", creator_id);
		send_message();
		block(1);
	}

	
	printf("Creator %d started, domain participant and early readers ready, waiting for signal to continue\n", creator_id);
	send_message();
	block(1);
	printf("Creator %d got signal to continue....\n", creator_id);

       
       
        publisher = DDS_DomainParticipant_create_publisher (participant, NULL, NULL, 0);

	if (!publisher) {
		fprintf (stderr, "Could not create publisher\n");
		exit (EXIT_FAILURE);
	}
	
        writer = DDS_Publisher_create_datawriter (publisher, topic[0], NULL, NULL, 0);

        if (!writer) {
		fprintf (stderr, "Could not create data writer\n");
		exit (EXIT_FAILURE);
	}

	if (!early_creator) {
		subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0);

		if (!subscriber) {
			fprintf (stderr, "Could not create subscriber\n");
			exit (EXIT_FAILURE);
		}
	}

        for (i=early_readers; i< NREADERS; i++) {
                reader[i] = DDS_Subscriber_create_datareader (subscriber, topic[i], NULL, NULL, 0);

                if (!reader[i]) {
                        fprintf (stderr, "Could not create data reader\n");
                        exit (EXIT_FAILURE);
                }
        }
        
	

	send_message();
        block(1);
	send_message();
        block(1); 
	
#if 0	
	printf("Creator %d updating subscriptions....\n", creator_id);

	for (i=0; i< NREADERS; i++) {
		DDS_DataReaderQos 	qos;
		memset(&qos, 0, sizeof(DDS_DataReaderQos));
		DDS_Subscriber_get_default_datareader_qos (publisher, &qos);
		DDS_DataReader_get_qos(reader[i], &qos);
		qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
		qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
		qos.history.kind = DDS_KEEP_LAST_HISTORY_QOS;
		qos.history.depth = 5;

		error = DDS_DataReader_set_qos(reader[i], &qos);
		if (error) {
			fprintf (stderr, "Creator %d could not set qos params.\nError code = %d\n", creator_id, error);
			exit (EXIT_FAILURE);
			
		}
	}

	send_message();
        block(1); 
#endif

	error = DDS_DomainParticipant_delete_contained_entities (participant);

	if (error) {
		fprintf (stderr, "Creator %d could not delete contained entities.\nError code = %d\n", creator_id, error);
		exit (EXIT_FAILURE);
	}

	/* Destroy participant */
	error = DDS_DomainParticipantFactory_delete_participant (participant);
	if (error) {
		fprintf (stderr, "Creator %d could not delete domain participant.\nError code = %d\n", creator_id, error);
		exit (EXIT_FAILURE);
	}
	printf("Creator %d deleted participant....\n", creator_id);

	block(1);

	printf("Creator %d got final signal.\n", creator_id);

	/* Restore signals */
	sigprocmask (SIG_UNBLOCK, &newmask, NULL);

	exit(EXIT_SUCCESS);
}

/* }}} */

#if 0
#ifdef PROFILE
extern void _start (void), etext (void);
void  monstartup(u_long *lowpc, u_long *highpc);
#endif
#endif

int
main (int argc, char **argv)
{
	pid_t pid;

	ARG_NOT_USED(argc)
	ARG_NOT_USED(argv)

	sigfillset(&oldmask);
	sigdelset(&oldmask, SIGQUIT);
	sigdelset(&oldmask, SIGINT);
	sigprocmask (SIG_BLOCK, &oldmask, NULL);
	sigemptyset(&newmask);
	sigaddset(&newmask, SIGQUIT);
	sigaddset(&newmask, SIGINT);

	/* Block our signals for now */
	sigprocmask (SIG_BLOCK, &newmask, &oldmask);

	/* Install signal handlers */
	if (signal(SIGINT, interrupt_handler) == SIG_ERR)
		fprintf(stderr, "Could not install signal handler\n");
	if (signal(SIGQUIT, interrupt_handler) == SIG_ERR)
		fprintf(stderr, "Could not install signal handler\n");

	setup_queues();

	observer_pid = getpid();

	for (creator_id = 1; creator_id <= NCREATORS; creator_id++) {
		fflush(stdout);
		if ((pid = fork()) != 0) {
			if (pid < 0) {
				fprintf(stderr, "Failed to fork\n");
				exit(0);
			}
			printf("pid %d created for creator %d\n", pid, creator_id);
			/* Store pid */
			pthread_mutex_lock(&part_mutex);
			participant_pids [nparts++] = pid;
			pthread_mutex_unlock(&part_mutex);
		}
		else {
                        char output[200];
                        sprintf(output,"GCOV_PREFIX=/tmp/gcov/%d",creator_id);
                        putenv(output);
#if 0
#ifdef PROFILE
                        monstartup ((u_long *) &_start, (u_long *) &etext);
#endif
#endif
                        
			if (creator_id <= EARLY_CREATORS)
				early_creator = 1;
			else
				early_creator = 0;
			creator();
			return EXIT_FAILURE;
		}
	}

	observer();
	return EXIT_FAILURE;
}

/* vim: set foldmethod=marker foldmarker={{{,}}}: */
