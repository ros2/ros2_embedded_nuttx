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

#include <pthread.h>
#include <arpa/inet.h>
#include "test.h"
#include "ta_type.h"
#include "ta_disc.h"

#define	DOMAIN_ID		7	/* Domain Id to use. */
#define	MAX_PARTICIPANTS	3	/* # of participants. */
#define	NDR			2	/* # of DataReaders. */
#define	NDW			2	/* # of DataWriters. */
#define	NINST			2	/* # of instances per DataWriter. */

#define	lock_t			pthread_mutex_t
#define	lock_init_nr(l,s)	pthread_mutex_init(&l,NULL)
#define	lock_take(l)		pthread_mutex_lock(&l)
#define	lock_release(l)		pthread_mutex_unlock(&l)

typedef struct dreader_st {
	DDS_DataReader		dr;
	DDS_DataReaderListener	l;
} DREADER;

typedef struct dwriter_st {
	DDS_DataWriter		dw;
	DDS_DataWriterListener	l;
	DDS_InstanceHandle_t	h [NINST];
} DWRITER;

typedef struct ddata_st {
	int			id;
	DDS_DomainParticipant	p;
	DDS_Subscriber		bi_sub;
	DDS_Subscriber		d_sub;
	DDS_Publisher		d_pub;
	DDS_Topic		dt;
	DDS_TopicDescription	dtd;
	DREADER			bi_r [NBUILTINS];
	DREADER			d_r [NDR];
	DWRITER			d_w [NDW];
} DDATA;

static DDATA	*d [MAX_PARTICIPANTS];
static lock_t	plock;
const char *kind_str [] = {
	NULL,
	"ALIVE",
	"NOT_ALIVE_DISPOSED",
	NULL,
	"NOT_ALIVE_NO_WRITERS"
};

static void v_print_cookie (int reader, void *cookie)
{
	unsigned	participant, num;

	participant = ((uintptr_t) cookie) >> 16;
	num = ((uintptr_t) cookie) & 0xff;
	v_printf ("(%u) %c%u     - ", participant, (reader) ? 'R' : 'W', num);
}

void dr_rejected (DDS_DataReaderListener   *self,
		  DDS_DataReader           reader,
		  DDS_SampleRejectedStatus *status)
{
	static const char *reason_str [] = {
		"none", "Instance", "Samples", "Samples_per_instance"
	};

	ARG_NOT_USED (reader)

	lock_take (plock);
	v_print_cookie (1, self->cookie);
	v_printf ("Sample Rejected - T=%d, TC=%d - %s - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			reason_str [status->last_reason],
			status->last_instance_handle);
	lock_release (plock);
}

void dr_liveliness (DDS_DataReaderListener      *self,
		    DDS_DataReader              reader,
		    DDS_LivelinessChangedStatus *status)
{
	ARG_NOT_USED (reader)

	lock_take (plock);
	v_print_cookie (1, self->cookie);
	v_printf ("Liveliness Changed - A=%d, NA=%d, AC=%d, NAC=%d, {%u}\r\n",
			status->alive_count,
			status->not_alive_count,
			status->alive_count_change,
			status->not_alive_count_change,
			status->last_publication_handle);
	lock_release (plock);
}

void dr_deadline (DDS_DataReaderListener            *self,
		  DDS_DataReader                    reader,
		  DDS_RequestedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (reader)

	lock_take (plock);
	v_print_cookie (1, self->cookie);
	v_printf ("Requested Deadline Missed - T=%d, TC=%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->last_instance_handle);
	lock_release (plock);
}

void v_print_policies (DDS_QosPolicyCountSeq *seq)
{
	DDS_QosPolicyCount	*p;
	unsigned		i;

	if (DDS_SEQ_LENGTH (*seq)) {
		DDS_SEQ_FOREACH_ENTRY (*seq, i, p) {
			if (i) {
				v_printf (", ");
			}
			else {
				v_printf ("\t");
			}
			v_printf ("%s:%d", DDS_qos_policy (p->policy_id), p->count);
		}
		v_printf ("\r\n");
	}
}

void dr_inc_qos (DDS_DataReaderListener             *self,
		 DDS_DataReader                     reader,
		 DDS_RequestedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (reader)

	lock_take (plock);
	v_print_cookie (1, self->cookie);
	v_printf ("Requested Incompatible QoS - T=%d, TC=%d, L:%s\r\n",
			status->total_count,
			status->total_count_change,
			DDS_qos_policy (status->last_policy_id));
	v_print_policies (&status->policies);
	lock_release (plock);
}

void dr_smatched (DDS_DataReaderListener        *self,
		  DDS_DataReader                reader,
		  DDS_SubscriptionMatchedStatus *status)
{
	ARG_NOT_USED (reader)

	lock_take (plock);
	v_print_cookie (1, self->cookie);
	v_printf ("Subscription Matched - T:%d, TC:%d, C:%d, CC:%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->current_count,
			status->current_count_change,
			status->last_publication_handle);
	lock_release (plock);
}

void dr_lost (DDS_DataReaderListener *self,
	      DDS_DataReader         reader,
	      DDS_SampleLostStatus   *status)
{
	ARG_NOT_USED (reader)

	lock_take (plock);
	v_print_cookie (1, self->cookie);
	v_printf ("Sample Lost - T:%d, TC:%d\r\n",
			status->total_count,
			status->total_count_change);
	lock_release (plock);
}

void dr_data_avail (DDS_DataReaderListener *l,
	            DDS_DataReader         dr)
{
	DDS_DataSeq		*rx_samples;
	DDS_SampleInfoSeq 	*rx_infos;
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		*info;
	MsgData_t		*sample;
	DDS_ReturnCode_t	error;
	unsigned		participant, reader;

	ARG_NOT_USED (l)
	rx_samples = DDS_DataSeq__alloc ();
	rx_infos = DDS_SampleInfoSeq__alloc ();
	fail_unless (rx_samples && rx_infos);

	/*printf ("do_read: got notification!\r\n");*/
	for (;;) {
		error = DDS_DataReader_read (dr, rx_samples, rx_infos, 1, ss, vs, is);
		if (error) {
			if (error != DDS_RETCODE_NO_DATA)
				v_printf ("Unable to read samples: error = %u!\r\n", error);

			break;
		}
		if (DDS_SEQ_LENGTH (*rx_infos)) {
			lock_take (plock);
			sample = DDS_SEQ_ITEM (*rx_samples, 0);
			info = DDS_SEQ_ITEM (*rx_infos, 0);
			if (verbose) {
				participant = ((uintptr_t) l->cookie) >> 16;
				reader = ((uintptr_t) l->cookie) & 0xff;
				v_printf ("(%u) R%u [%u] - ", participant,
						reader, info->instance_handle);
				if (info->instance_state == DDS_ALIVE_INSTANCE_STATE) {
					v_printf ("'%s'\r\n", sample->message);
				}
				else
					v_printf ("%s\r\n", kind_str [info->instance_state]);
			}

			/*if (trace && info->instance_state == DDS_ALIVE_INSTANCE_STATE)
				trace_data (bufp, data_size);*/
			lock_release (plock);
			DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
		}
		else {
			v_printf ("do_read: all read!\r\n");
			break;
		}
	}
	DDS_DataSeq__free (rx_samples);
	DDS_SampleInfoSeq__free (rx_infos);
}

static DDS_DataReaderListener dr_listener = {
	dr_rejected,	/* Sample rejected. */
	dr_liveliness,	/* Liveliness changed. */
	dr_deadline,	/* Requested Deadline missed. */
	dr_inc_qos,	/* Requested incompatible QoS. */
	dr_data_avail,	/* Data available. */
	dr_smatched,	/* Subscription matched. */
	dr_lost,	/* Sample lost. */
	NULL		/* Cookie */
};

void dw_deadline (DDS_DataWriterListener          *self,
		  DDS_DataWriter                  writer,
		  DDS_OfferedDeadlineMissedStatus *status)
{
	ARG_NOT_USED (writer)

	lock_take (plock);
	v_print_cookie (0, self->cookie);
	v_printf ("Offered Deadline Missed - T=%d, TC=%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->last_instance_handle);
	lock_release (plock);
}

void dw_pmatched (DDS_DataWriterListener       *self,
		  DDS_DataWriter               writer,
		  DDS_PublicationMatchedStatus *status)
{
	ARG_NOT_USED (writer)

	lock_take (plock);
	v_print_cookie (0, self->cookie);
	v_printf ("Publication Matched - T:%d, TC:%d, C:%d, CC:%d - {%u}\r\n",
			status->total_count,
			status->total_count_change,
			status->current_count,
			status->current_count_change,
			status->last_subscription_handle);
	lock_release (plock);
}

void dw_liveliness (DDS_DataWriterListener   *self,
		    DDS_DataWriter           writer,
		    DDS_LivelinessLostStatus *status)
{
	ARG_NOT_USED (writer)

	lock_take (plock);
	v_print_cookie (0, self->cookie);
	v_printf ("Liveliness Lost - T:%d, TC:%d\r\n",
			status->total_count,
			status->total_count_change);
	lock_release (plock);
}

void dw_inc_qos (DDS_DataWriterListener           *self,
		 DDS_DataWriter                   writer,
		 DDS_OfferedIncompatibleQosStatus *status)
{
	ARG_NOT_USED (writer)

	lock_take (plock);
	v_print_cookie (0, self->cookie);
	v_printf ("Offered Incompatible QoS - T=%d, TC=%d, L:%s\r\n",
			status->total_count,
			status->total_count_change,
			DDS_qos_policy (status->last_policy_id));
	v_print_policies (&status->policies);
	lock_release (plock);
}

static DDS_DataWriterListener dw_listener = {
	dw_deadline,	/* Offered Deadline missed. */
	dw_pmatched,	/* Publication matched. */
	dw_liveliness,	/* Liveliness lost. */
	dw_inc_qos,	/* Offered Incompatible QoS. */
	NULL
};

DDS_DomainParticipant data_prologue (void)
{
	DDS_PoolConstraints	c;
	DDS_ReturnCode_t	r;
	DDATA			*dp;
	DREADER			*rp;
	Builtin_t		i;
	DDS_StatusMask		m;
	unsigned		n;

	v_printf (" - Updating pool constraints for %u participants.\r\n", MAX_PARTICIPANTS);
	r = DDS_get_default_pool_constraints (&c, 5000, 2000);
	c.max_domains = 3;
	c.min_list_nodes *= 5;
	c.max_list_nodes *= 5;
	c.min_locators *= 5;
	c.max_locators *= 5;
	c.min_local_readers *= 4;
	c.max_local_readers *= 4;
	c.min_local_writers *= 4;
	c.max_local_writers *= 4;
	c.min_remote_readers *= 4;
	c.max_remote_readers *= 4;
	c.min_remote_writers *= 4;
	c.max_remote_writers *= 4;
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_set_pool_constraints (&c);
	fail_unless (r == DDS_RETCODE_OK);

	for (n = 0; n < MAX_PARTICIPANTS; n++) {
		d [n] = dp = malloc (sizeof (DDATA));
		fail_unless (dp != NULL);
		dp->id = n;
		dp->p = DDS_DomainParticipantFactory_create_participant (DOMAIN_ID, NULL, NULL, 0);
		fail_unless (dp->p != NULL);
		if (!n) {
			r = new_HelloWorldData_type ();
			fail_unless (r == DDS_RETCODE_OK);
		}
		dp->bi_sub = DDS_DomainParticipant_get_builtin_subscriber (dp->p);
		fail_unless (dp->bi_sub != NULL);
		for (i = 0, rp = dp->bi_r; i < NBUILTINS; rp++, i++) {
			rp->dr = DDS_Subscriber_lookup_datareader (dp->bi_sub, builtin_names [i]);
			fail_unless (rp->dr != NULL);
			memset (&rp->l, 0, sizeof (rp->l));
			rp->l.on_data_available = builtin_data_avail [i];
			rp->l.cookie = (void *) (uintptr_t) n;
			r = DDS_DataReader_set_listener (rp->dr, &rp->l, DDS_DATA_AVAILABLE_STATUS);
			fail_unless (r == DDS_RETCODE_OK);
		}
		r = attach_HelloWorldData_type (dp->p);
		fail_unless (r == DDS_RETCODE_OK);

		dp->dt = DDS_DomainParticipant_create_topic (dp->p, "HelloWorld", TYPE_NAME, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
		fail_unless (dp->dt != NULL);

		dp->dtd = DDS_DomainParticipant_lookup_topicdescription (dp->p, "HelloWorld");
		fail_unless (dp->dtd != NULL);

		dp->d_pub = DDS_DomainParticipant_create_publisher (dp->p, NULL, NULL, 0);
		fail_unless (dp->d_pub != NULL);

		dp->d_sub = DDS_DomainParticipant_create_subscriber (dp->p, NULL, NULL, 0);
		fail_unless (dp->d_sub != NULL);

		for (i = 0; i < NDW; i++) {
			dp->d_w [i].l = dw_listener;
			m = DDS_OFFERED_DEADLINE_MISSED_STATUS |
			    DDS_PUBLICATION_MATCHED_STATUS |
			    DDS_LIVELINESS_LOST_STATUS |
			    DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
			dp->d_w [i].l.cookie = (void *)(uintptr_t)(n << 16 | i);
			dp->d_w [i].dw = DDS_Publisher_create_datawriter (dp->d_pub, dp->dt, NULL, &dp->d_w [i].l, m);
			fail_unless (dp->d_w [i].dw != NULL);
		}
		for (i = 0; i < NDR; i++) {
			dp->d_r [i].l = dr_listener;
			m = DDS_SAMPLE_REJECTED_STATUS |
			    DDS_LIVELINESS_CHANGED_STATUS |
			    DDS_REQUESTED_DEADLINE_MISSED_STATUS |
			    DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS |
			    DDS_SUBSCRIPTION_MATCHED_STATUS |
			    DDS_SAMPLE_LOST_STATUS;
			if (!n)
				dp->d_r [i].l.on_data_available = NULL;
			else
				m |= DDS_DATA_AVAILABLE_STATUS;
			dp->d_r [i].l.cookie = (void *)(uintptr_t)(n << 16 | i);
			dp->d_r [i].dr = DDS_Subscriber_create_datareader (dp->d_sub, dp->dtd, NULL, &dp->d_r [i].l, m);
			fail_unless (dp->d_r [i].dr != NULL);
		}
	}
	sleep (1);
	return (d [0]->p);
}

typedef enum {
	WO_REGISTER,
	WO_WRITE,
	WO_WRITE_DIRECTED,
	WO_DISPOSE,
	WO_DISPOSE_DIRECTED,
	WO_UNREGISTER,
	WO_UNREGISTER_DIRECTED
} WriterOp_t;

static void data_w_op (DDS_DataWriter        w,
		       MsgData_t             *data,
		       DDS_InstanceHandle_t  *h,
		       unsigned              i,
		       DDS_InstanceHandleSeq *dests,
		       WriterOp_t            op,
		       unsigned              c)
{
	DDS_Time_t		time;
	DDS_ReturnCode_t	r;
	static const char 	*str [] = {
		"Hello folks ... welcome to TDDS!",
		"This is another data sample.",
		"And the last one!"
	};

	switch (op) {
		case WO_REGISTER:
			v_printf ("Register handle.\r\n");
			if (!i)
				h [i] = DDS_DataWriter_register_instance (w, data);
			else {
				r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
				fail_unless (r == DDS_RETCODE_OK);
				h [i] = DDS_DataWriter_register_instance_w_timestamp (w, data, &time);
			}
			fail_unless (h [i] != 0);
			break;
		case WO_WRITE:
		case WO_WRITE_DIRECTED:
			data->counter++;
			strcpy (data->message, (char *) str [c]);
			v_printf ("'%s'%s\r\n", data->message, (op != WO_WRITE) ? " Directed" : "");
			if (op == WO_WRITE)
				if (!c)
					r = DDS_DataWriter_write (w, data, h [i]);
				else {
					r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
					fail_unless (r == DDS_RETCODE_OK);
					r = DDS_DataWriter_write_w_timestamp (w, data, h [i], &time);
				}
			else {
				if (!c)
					r = DDS_DataWriter_write_directed (w, data, h [i], dests);
				else {
					r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
					fail_unless (r == DDS_RETCODE_OK);
					r = DDS_DataWriter_write_w_timestamp_directed (w, data, h [i], &time, dests);
				}
			}
			fail_unless (r == DDS_RETCODE_OK);
			break;
		case WO_DISPOSE:
		case WO_DISPOSE_DIRECTED:
			v_printf ("Dispose%s\r\n", (op != WO_DISPOSE) ? " Directed" : "");
			if (op == WO_DISPOSE)
				if (!c)
					r = DDS_DataWriter_dispose (w, data, h [i]);
				else {
					r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
					fail_unless (r == DDS_RETCODE_OK);
					r = DDS_DataWriter_dispose_w_timestamp (w, data, h [i], &time);
				}
			else {
				if (!c)
					r = DDS_DataWriter_dispose_directed (w, data, h [i], dests);
				else {
					r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
					fail_unless (r == DDS_RETCODE_OK);
					r = DDS_DataWriter_dispose_w_timestamp_directed (w, data, h [i], &time, dests);
				}
			}
			fail_unless (r == DDS_RETCODE_OK);
			break;
		case WO_UNREGISTER:
		case WO_UNREGISTER_DIRECTED:
			v_printf ("Unregister%s\r\n", (op != WO_UNREGISTER) ? " Directed" : "");
			if (op == WO_UNREGISTER)
				if (!c)
					r = DDS_DataWriter_unregister_instance (w, data, h [i]);
				else {
					r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
					fail_unless (r == DDS_RETCODE_OK);
					r = DDS_DataWriter_unregister_instance_w_timestamp (w, data, h [i], &time);
				}
			else {
				if (!c)
					r = DDS_DataWriter_unregister_instance_directed (w, data, h [i], dests);
				else {
					r = DDS_DomainParticipant_get_current_time (d [0]->p, &time);
					fail_unless (r == DDS_RETCODE_OK);
					r = DDS_DataWriter_unregister_instance_w_timestamp_directed (w, data, h [i], &time, dests);
				}
			}
			fail_unless (r == DDS_RETCODE_OK);
			h [i] = 0;
			break;
	}
}

static void test_data_w (WriterOp_t op, unsigned c)
{
	DDATA			*dp;
	static MsgData_t 	md;
	unsigned		n, i, x, cx;
	DDS_DataReader		dr;
	DDS_InstanceHandle_t	h;
	DDS_ReturnCode_t	r;
	DDS_InstanceHandleSeq	handles, *sp;

	DDS_InstanceHandleSeq__init (&handles);
	for (n = 0; n < MAX_PARTICIPANTS; n++) {
		dp = d [n];
		for (i = 0; i < NDW; i++)
			for (x = 0; x < NINST; x++) {
				md.key [0] = n;
				md.key [1] = i;
				md.key [2] = x + 0x20;
				md.key [3] = x + 0x30;
				md.key [4] = x + 0x40;
				for (cx = 0; cx < c; cx++) {
					lock_take (plock);
					v_printf ("(%u) W%u [%u] - ", n, i, x);
					if (op == WO_WRITE_DIRECTED ||
					    op == WO_DISPOSE_DIRECTED ||
					    op == WO_UNREGISTER_DIRECTED) {
						dr = dp->d_r [NDR - 1].dr;
						h = DDS_DataReader_get_instance_handle (dr);
						fail_unless (h != 0);
						r = dds_seq_append (&handles, &h);
						fail_unless (r == DDS_RETCODE_OK);
						sp = &handles;
					}
					else
						sp = NULL;
					data_w_op (dp->d_w [i].dw, &md,
						   dp->d_w [i].h, x, sp,
						   op, i);
					lock_release (plock);
					if (sp)
						DDS_InstanceHandleSeq__clear (sp);
				}
			}
	}
}

extern int no_rtps;

static void test_data_r (DDS_DataReader dr)
{
	DDS_DataSeq		*rx_samples;
	DDS_SampleInfoSeq 	*rx_infos;
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_SampleInfo		si;
	MsgData_t		sd;
	DDS_ReturnCode_t	r;
	unsigned		i;

	rx_samples = DDS_DataSeq__alloc ();
	rx_infos = DDS_SampleInfoSeq__alloc ();
	fail_unless (rx_samples && rx_infos);

	for (i = 0; i < 7; i++) {
		if (i > 2 && no_rtps) 
			break;
		switch (i) {
			case 0:
				v_printf (" - Test DDS_DataReader_take(!read,n=2)\r\n");
				r = DDS_DataReader_take (dr, rx_samples, rx_infos, 2, ss, vs, is);
				fail_unless (r == DDS_RETCODE_OK);
				DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			case 1:
				v_printf (" - Test DDS_DataReader_read_next_sample(!read)\r\n");
				r = DDS_DataReader_read_next_sample (dr, &sd, &si);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			case 2:
				v_printf (" - Test DDS_DataReader_take_next_sample(!read)\r\n");
				r = DDS_DataReader_take_next_sample (dr, &sd, &si);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			case 3: 
				v_printf (" - Test DDS_DataReader_read_instance(#9,any,n=1)\r\n");
				ss = DDS_ANY_SAMPLE_STATE;
				r = DDS_DataReader_read_instance (dr, rx_samples, rx_infos, 1, 9, ss, vs, is);
				fail_unless (r == DDS_RETCODE_OK);
				r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			case 4:
				v_printf (" - Test DDS_DataReader_take_instance(#10,any,n=1)\r\n");
				r = DDS_DataReader_take_instance (dr, rx_samples, rx_infos, 1, 10, ss, vs, is);
				fail_unless (r == DDS_RETCODE_OK);
				r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			case 5:
				v_printf (" - Test DDS_DataReader_read_next_instance(#10,any,n=1)\r\n");
				r = DDS_DataReader_read_next_instance (dr, rx_samples, rx_infos, 1, 10, ss, vs, is);
				fail_unless (r == DDS_RETCODE_OK);
				r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			case 6:
				v_printf (" - Test DDS_DataReader_take_next_instance(#11,any,n=1)\r\n");
				r = DDS_DataReader_take_next_instance (dr, rx_samples, rx_infos, 1, 11, ss, vs, is);
				fail_unless (r == DDS_RETCODE_OK);
				r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
				fail_unless (r == DDS_RETCODE_OK);
				break;
			default:
		  		break;
		}
	}

	DDS_DataSeq__free (rx_samples);
	DDS_SampleInfoSeq__free (rx_infos);
}

void test_data_cond (DDS_DataReader dr)
{
	DDS_DataSeq		*rx_samples;
	DDS_SampleInfoSeq 	*rx_infos;
	DDS_QueryCondition	qc;
	DDS_ReturnCode_t	r;

	rx_samples = DDS_DataSeq__alloc ();
	rx_infos = DDS_SampleInfoSeq__alloc ();
	fail_unless (rx_samples && rx_infos);

	qc = DDS_DataReader_create_querycondition (dr,
			DDS_ANY_SAMPLE_STATE,
			DDS_ANY_VIEW_STATE,
			DDS_ANY_INSTANCE_STATE,
			"counter > 3 and message like `%TDDS%'",
			NULL);
	fail_unless (qc != NULL);
	if (!no_rtps) {
		r = DDS_DataReader_read_w_condition (dr, rx_samples, rx_infos, 1, qc);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_read_next_instance_w_condition (dr, rx_samples, 
				rx_infos, 1, 8, qc);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_take_w_condition (dr, rx_samples, rx_infos, 2, qc);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_take_next_instance_w_condition (dr, rx_samples, 
				rx_infos, 1, 6, qc);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DataReader_return_loan (dr, rx_samples, rx_infos);
	}
	DDS_DataSeq__free (rx_samples);
	DDS_SampleInfoSeq__free (rx_infos);
}

void data_epilogue (void)
{
	DDATA			*dp;
	DDS_ReturnCode_t	r;
	unsigned		n;

	for (n = 0; n < MAX_PARTICIPANTS; n++) {
		dp = d [n];
		r = DDS_DomainParticipant_delete_contained_entities (dp->p);
		fail_unless (r == DDS_RETCODE_OK);
		r = DDS_DomainParticipantFactory_delete_participant (dp->p);
		fail_unless (r == DDS_RETCODE_OK);
		free (dp);
		d [n] = NULL;
	}
}

#define ARRAY_SIZE 5

struct struct3 {
	char *char_ptr_array [ARRAY_SIZE];
};

static DDS_TypeSupport_meta tsm3 [] = {
	{ CDR_TYPECODE_STRUCT, 0, "struct3", sizeof (struct struct3), 0, 1, 0, NULL },
	{ CDR_TYPECODE_ARRAY, 0, "char_ptr_array", sizeof (char * [ARRAY_SIZE]), offsetof (struct struct3, char_ptr_array), ARRAY_SIZE, 0, NULL },
	{ CDR_TYPECODE_CSTRING, 2, NULL, 0, 0, 0, 0, NULL }
};

static DDS_TypeSupport ts3;

void data_xtopic_add (void)
{
	DDS_ReturnCode_t	r;
	static int		i = 0;

	ts3 = DDS_DynamicType_register (tsm3);
	fail_unless (ts3 != NULL);

	r = DDS_DomainParticipant_register_type (d [MAX_PARTICIPANTS - 1]->p,
							ts3, "StrArray");
	fail_unless (r == DDS_RETCODE_OK);

	i++;
}

void data_xtopic_remove (void)
{
	DDS_DynamicType_free (ts3);
}

void test_ignore (void)
{
	if (last_d0 [Pub])
		DDS_DomainParticipant_ignore_publication (d [0]->p, last_d0 [Pub]);
	if (last_d0 [Sub])
		DDS_DomainParticipant_ignore_subscription (d [0]->p, last_d0 [Sub]);
	if (last_d0 [Topic])
		DDS_DomainParticipant_ignore_topic (d [0]->p, last_d0 [Topic]);
	if (last_d0 [Participant])
		DDS_DomainParticipant_ignore_participant (d [0]->p, last_d0 [Participant]);
}

void test_data (void)
{
	DDS_SampleStateMask	ss = DDS_NOT_READ_SAMPLE_STATE;
	DDS_ViewStateMask	vs = DDS_ANY_VIEW_STATE;
	DDS_InstanceStateMask	is = DDS_ANY_INSTANCE_STATE;
	DDS_InstanceStateMask	m;
	DDS_DataReaderSeq	*readers;
	DDS_DataReader		*rp;
	DDS_DataWriter		dw;
	DDS_DataReader		dr;
	DDS_InstanceHandleSeq	*handles;
	DDS_InstanceHandle_t	h, *hp;
	DDS_Topic		t;
	DDS_WaitSet		ws;
	DDS_ReadCondition	rc [NDR];
	DDS_GuardCondition	gc;
	DDS_StatusCondition	sc [6];
	DDS_QueryCondition	qc;
	DDS_StatusMask		sm;
	DDS_Entity		e;
	DDS_ConditionSeq	*conds, *xconds;
	DDS_Duration_t		to;
	DDS_SubscriptionBuiltinTopicData *sdata;
	DDS_PublicationBuiltinTopicData *pdata;
	DDS_StringSeq		*ssp;
	DDS_ReturnCode_t	r;
	MsgData_t		mdata;
	unsigned		i, j = 0;
	int			triggered;
	const char		*expr, *par;
	static const char	qc_expr [] = "counter > %0";

	dbg_printf ("Data ");
	if (trace)
		fflush (stdout);

	lock_init_nr (plock, "Mutex");

	data_prologue ();
	test_data_w (WO_REGISTER, 1);
	test_data_w (WO_WRITE, 1);

	sleep (1);

	v_printf (" - Test WaitSet\r\n");
	conds = DDS_ConditionSeq__alloc ();
	fail_unless (conds != NULL);

	ws = DDS_WaitSet__alloc ();
	fail_unless (ws != NULL);

	for (i = 0; i < NDR; i++) {
		rc [i] = DDS_DataReader_create_readcondition (d [0]->d_r [i].dr, ss, vs, is);
		fail_unless (rc [i] != NULL);

		r = DDS_WaitSet_attach_condition (ws, rc [i]);
		fail_unless (r == DDS_RETCODE_OK);
	}
	xconds = DDS_ConditionSeq__alloc ();
	r = DDS_WaitSet_get_conditions (ws, xconds);
	fail_unless (r == DDS_RETCODE_OK &&
		     DDS_SEQ_LENGTH (*xconds) == NDR);

	DDS_ConditionSeq__clear (xconds);
	to.sec = 1;
	to.nanosec = 0;
	r = DDS_WaitSet_wait (ws, xconds, &to);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_SEQ_FOREACH (*xconds, i) {
		for (j = 0; j < NDR; j++)
			if (rc [j] == DDS_SEQ_ITEM (*xconds, i))
				break;

		fail_unless (j < NDR);
		m = DDS_DataReader_get_status_changes (d [0]->d_r [j].dr);
		fail_unless ((m & DDS_DATA_AVAILABLE_STATUS) != 0);
		v_printf ("!R%u ", j);
		dr_data_avail (&d [0]->d_r [j].l, d [0]->d_r [j].dr);
		v_printf ("\r\n");
	}

	v_printf (" - Test extra Data transfers\r\n");

	/*fail_unless (r = DDS_RETCODE_OK);*/

	test_data_w (WO_WRITE, 2);
	test_data_w (WO_WRITE_DIRECTED, 1);
	test_data_w (WO_DISPOSE, 1);
	test_data_w (WO_WRITE, 2);
	test_data_w (WO_DISPOSE_DIRECTED, 1);

	v_printf (" - Test instance data\r\n");
	r = DDS_DataWriter_get_key_value (d [0]->d_w [1].dw, &mdata, d [0]->d_w [1].h [0]);
	fail_unless (r == DDS_RETCODE_OK &&
		     mdata.key [0] == 0 && mdata.key [1] == 1 &&
		     mdata.key [2] == 0x20 && mdata.key [3] == 0x30 && 
		     mdata.key [4] == 0x40);
	h = DDS_DataWriter_lookup_instance (d [0]->d_w [1].dw, &mdata);
	fail_unless (h == d [0]->d_w [1].h [0]);
	test_data_w (WO_UNREGISTER, 1);
	test_data_w (WO_WRITE, 2);
	test_data_w (WO_UNREGISTER_DIRECTED, 1);
	readers = DDS_DataReaderSeq__alloc ();
	fail_unless (readers != NULL);
	to.sec = 1;
	to.nanosec = 0;
	DDS_ConditionSeq__clear (xconds);
	r = DDS_WaitSet_wait (ws, xconds, &to);
	fail_unless (r == DDS_RETCODE_OK);
	usleep (1000);
	DDS_SEQ_FOREACH (*xconds, i) {
		for (j = 0; j < NDR; j++)
			if (rc [j] == DDS_SEQ_ITEM (*xconds, i))
				break;

		fail_unless (j < NDR);
		m = DDS_DataReader_get_status_changes (d [0]->d_r [j].dr);
		fail_unless ((m & DDS_DATA_AVAILABLE_STATUS) != 0);
		test_data_r (d [0]->d_r [j].dr);
	}
	DDS_ConditionSeq__clear (xconds);
	usleep (1000);
	v_printf (" - Test get_datareaders():");
	r = DDS_Subscriber_get_datareaders (d [0]->d_sub, readers, ss, vs, is);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_SEQ_FOREACH_ENTRY (*readers, i, rp) {
		h = DDS_DataReader_get_instance_handle (*rp);
		v_printf (" {%u}", h);
	}
	v_printf ("\r\n");
	DDS_DataReaderSeq__free (readers);

	v_printf (" - Test instance/key conversion functions.\r\n");
	r = DDS_DataReader_get_key_value (d [0]->d_r [j].dr, &mdata, 200);
	fail_unless (r == DDS_RETCODE_ALREADY_DELETED);
	if (!no_rtps) {
		r = DDS_DataReader_get_key_value (d [0]->d_r [j].dr, &mdata, 6);
		fail_unless (r == DDS_RETCODE_OK);
		h = DDS_DataReader_lookup_instance (d [0]->d_r [j].dr, &mdata);
		fail_unless (h == 6);
	}

	v_printf (" - Test read/take_*w_condition.\r\n");
	test_data_cond (d [0]->d_r [0].dr);

	v_printf (" - Test auxiliary functions\r\n");
	t = DDS_DataWriter_get_topic (d [1]->d_w [1].dw);
	fail_unless (t == d [1]->dt);

	handles = DDS_InstanceHandleSeq__alloc ();
	fail_unless (handles != NULL);
	dw = d [0]->d_w [NDW - 1].dw;
	r = DDS_DataWriter_get_matched_subscriptions (dw, handles);
	fail_unless (r == DDS_RETCODE_OK);
	sdata = DDS_SubscriptionBuiltinTopicData__alloc ();
	fail_unless (sdata != NULL);
	DDS_SEQ_FOREACH_ENTRY (*handles, i, hp) {
		r = DDS_DataWriter_get_matched_subscription_data (dw, sdata, *hp);
		fail_unless (r == DDS_RETCODE_OK);
		v_printf (" --> W%u matches %08x:%08x:%08x {%u} %s/%s\r\n",
			NDW - 1,
			ntohl (sdata->key.value [0]),
			ntohl (sdata->key.value [1]),
			ntohl (sdata->key.value [2]),
			*hp,
			sdata->topic_name,
			sdata->type_name);
		DDS_SubscriptionBuiltinTopicData__clear (sdata);
	}
	DDS_SubscriptionBuiltinTopicData__free (sdata);
	DDS_InstanceHandleSeq__clear (handles);

	dr = d [0]->d_r [NDR - 1].dr;
	r = DDS_DataReader_get_matched_publications (dr, handles);
	fail_unless (r == DDS_RETCODE_OK);
	pdata = DDS_PublicationBuiltinTopicData__alloc ();
	fail_unless (sdata != NULL);
	DDS_SEQ_FOREACH_ENTRY (*handles, i, hp) {
		r = DDS_DataReader_get_matched_publication_data (dr, pdata, *hp);
		fail_unless (r == DDS_RETCODE_OK);
		v_printf (" --> R%u matches %08x:%08x:%08x {%u} %s/%s\r\n",
			NDR - 1,
			ntohl (pdata->key.value [0]),
			ntohl (pdata->key.value [1]),
			ntohl (pdata->key.value [2]),
			*hp,
			pdata->topic_name,
			pdata->type_name);
		DDS_PublicationBuiltinTopicData__clear (pdata);
	}
	DDS_PublicationBuiltinTopicData__free (pdata);
	DDS_InstanceHandleSeq__free (handles);
	DDS_ConditionSeq__free (conds);

	usleep (10000);
	v_printf (" - ReadCondition operations.\r\n");
	dr = DDS_ReadCondition_get_datareader (rc [0]);
	fail_unless (dr == d [0]->d_r [0].dr);
	vs = DDS_ReadCondition_get_view_state_mask (rc [0]);
	fail_unless (vs == DDS_ANY_VIEW_STATE);
	is = DDS_ReadCondition_get_instance_state_mask (rc [0]);
	fail_unless (is == DDS_ANY_INSTANCE_STATE);
	ss = DDS_ReadCondition_get_sample_state_mask (rc [0]);
	fail_unless (ss == DDS_NOT_READ_SAMPLE_STATE);
	triggered = DDS_ReadCondition_get_trigger_value (rc [0]);
	/*fail_unless (triggered == 0);*/

	v_printf (" - GuardCondition operations.\r\n");
	gc = DDS_GuardCondition__alloc ();
	fail_unless (gc != NULL);
	triggered = DDS_GuardCondition_get_trigger_value (gc);
	fail_unless (triggered == 0);
	r = DDS_GuardCondition_set_trigger_value (gc, 1);
	fail_unless (r == DDS_RETCODE_OK);
	triggered = DDS_GuardCondition_get_trigger_value (gc);
	fail_unless (triggered != 0);

	v_printf (" - StatusCondition operations.\r\n");
	sc [0] = DDS_DomainParticipant_get_statuscondition (d [0]->p);
	fail_unless (sc [0] != NULL);
	sc [1] = DDS_Topic_get_statuscondition (d [0]->dt);
	fail_unless (sc [1] != NULL);
	sc [2] = DDS_Publisher_get_statuscondition (d [0]->d_pub);
	fail_unless (sc [2] != NULL);
	sc [3] = DDS_Subscriber_get_statuscondition (d [0]->d_sub);
	fail_unless (sc [3] != NULL);
	sc [4] = DDS_DataWriter_get_statuscondition (d [0]->d_w [0].dw);
	fail_unless (sc [4] != NULL);
	sc [5] = DDS_DataReader_get_statuscondition (d [0]->d_r [0].dr);
	fail_unless (sc [5] != NULL);
	r = DDS_StatusCondition_set_enabled_statuses (sc [1], DDS_INCONSISTENT_TOPIC_STATUS);
	fail_unless (r == DDS_RETCODE_OK);
	sm = DDS_StatusCondition_get_enabled_statuses (sc [1]);
	fail_unless (sm == DDS_INCONSISTENT_TOPIC_STATUS);
	e = DDS_StatusCondition_get_entity (sc [1]);
	fail_unless (e == (DDS_Entity) d [0]->dt);
	triggered = DDS_StatusCondition_get_trigger_value (sc [1]);
	/*fail_unless (triggered == 0);*/

	v_printf (" - QueryCondition operations.\r\n");
	ssp = DDS_StringSeq__alloc ();
	fail_unless (ssp != NULL);
	par = "4";
	r = dds_seq_append (ssp, &par);
	fail_unless (r == DDS_RETCODE_OK);
	qc = DDS_DataReader_create_querycondition (d [0]->d_r [0].dr,
			DDS_ANY_SAMPLE_STATE,
			DDS_ANY_VIEW_STATE,
			DDS_ANY_INSTANCE_STATE,
			qc_expr,
			ssp);
	fail_unless (qc != NULL);
	triggered = DDS_QueryCondition_get_trigger_value (qc);
	/*fail_unless (triggered != 0);*/
	par = "200";
	r = dds_seq_replace (ssp, 0, &par);
	fail_unless (r == DDS_RETCODE_OK);
	r = DDS_QueryCondition_set_query_parameters (qc, ssp);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_StringSeq__clear (ssp);
	r = DDS_QueryCondition_get_query_parameters (qc, ssp);
	fail_unless (r == DDS_RETCODE_OK && 
		     DDS_SEQ_LENGTH (*ssp) == 1 &&
		     !strcmp (DDS_SEQ_ITEM (*ssp, 0), "200"));
	expr = DDS_QueryCondition_get_query_expression (qc);
	fail_unless (expr && !strcmp (qc_expr, expr));
	vs = DDS_QueryCondition_get_view_state_mask (qc);
	fail_unless (vs == DDS_ANY_VIEW_STATE);
	dr = DDS_QueryCondition_get_datareader (qc);
	fail_unless (dr == d [0]->d_r [0].dr);
	ss = DDS_QueryCondition_get_sample_state_mask (qc);
	fail_unless (ss == DDS_ANY_SAMPLE_STATE);
	is = DDS_QueryCondition_get_instance_state_mask (qc);
	fail_unless (is == DDS_ANY_INSTANCE_STATE);
	triggered = DDS_QueryCondition_get_trigger_value (qc);
	/*fail_unless (triggered == 0);*/
	DDS_StringSeq__free (ssp);

	test_ignore ();

	/* Cleanup. */
	for (i = 0; i < NDR; i++) {
		r = DDS_WaitSet_detach_condition (ws, rc [i]);
		fail_unless (r == DDS_RETCODE_OK);

		r = DDS_DataReader_delete_readcondition (d [0]->d_r [i].dr, rc [i]);
		fail_unless (r == DDS_RETCODE_OK);
	}
	DDS_GuardCondition__free (gc);
	r= DDS_DataReader_delete_readcondition (d [0]->d_r [0].dr, (DDS_ReadCondition) qc);
	fail_unless (r == DDS_RETCODE_OK);
	DDS_WaitSet__free (ws);
	DDS_ConditionSeq__free (xconds);
#ifdef DDS_DEBUG
	/*sleep (60);*/
#endif
	data_epilogue ();
	dbg_printf (" - success!\r\n");
}

