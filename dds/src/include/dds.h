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

/* dds.h -- DDS-internal functionality definitions. */

#ifndef __dds_h_
#define __dds_h_

#include "thread.h"
#include "dcps.h"
#include "config.h"
#include "dds_data.h"

#ifndef MAX_SAMPLE_SIZE_NF
#define	MAX_SAMPLE_SIZE_NF	(64*1024 - 129)	/* Max. unfragmented size. */
#endif
#ifndef MAX_SAMPLE_SIZE_F
#define MAX_SAMPLE_SIZE_F	(256*1024*1024)	/* Max. 256 MBytes/samples. */
#endif
#ifndef MAX_SAMPLE_SIZE
#if defined (BIGDATA) || (WORDSIZE == 64)
#define	MAX_SAMPLE_SIZE		MAX_SAMPLE_SIZE_F
#else
#define	MAX_SAMPLE_SIZE		MAX_SAMPLE_SIZE_NF
#endif
#endif

extern int dds_nargs;
extern char **dds_args;
extern const char *dds_entity_name;
extern unsigned dds_ev_pending;
extern size_t dds_max_sample_size;		/* Maximum sample size. */
extern unsigned dds_dtrace;
#ifdef THREADS_USED
extern thread_t dds_core_thread;
#endif

void dds_pre_init (void);

/* DDS preinitialization. Can be called either from domainparticipant create or
   from typecode registration, whichever occurs first. */

int dds_init (void);

/* Main DDS initialisation. */

void dds_final (void);

/* Main DDS finalisation. */

/* DDS internal events. */
#define	DDS_EV_IO	1	/* Pending socket I/O. */
#define	DDS_EV_TMR	2	/* Timers need to be scheduled. */
#define	DDS_EV_PROXY_NE	4	/* RTPS proxy contexts pending transmit. */
#define	DDS_EV_CACHE_X	8	/* Cache tranders pending. */
#define	DDS_EV_WAITSET	0x10	/* Waitset notifications pending. */
#define	DDS_EV_NOTIFY	0x20	/* Listener calls pending. */
#define	DDS_EV_CONFIG	0x40	/* Configuration updates pending. */
#define	DDS_EV_QUIT	0x80	/* Quit event processing. */

/* Various Locking primitives are used throughout the DDS system:

	- core lock: Prevents the core worker thread from becoming active.
	- event lock: For queueing events to the core worker thread.
	- domain lock: To protect the global domain data.
	- lock per parent node, e.g. Domain participant, Topic, Publisher
	  and Subscriber.
	- locks on the various pools.
*/
  
void dds_lock_core (void);
void dds_unlock_core (void);
void dds_lock_ev (void);
void dds_unlock_ev (void);
void dds_lock_domains (void);
void dds_unlock_domains (void);

void dds_signal (unsigned event);

/* Signal a new pending event. */

/* Notification types: */
typedef enum {
	NT_INCONSISTENT_TOPIC,
	NT_OFFERED_DEADLINE_MISSED,
	NT_REQUESTED_DEADLINE_MISSED,
	NT_OFFERED_INCOMPATIBLE_QOS = 5,
	NT_REQUESTED_INCOMPATIBLE_QOS,
	NT_SAMPLE_LOST,
	NT_SAMPLE_REJECTED,
	NT_DATA_ON_READERS,
	NT_DATA_AVAILABLE,
	NT_LIVELINESS_LOST,
	NT_LIVELINESS_CHANGED,
	NT_PUBLICATION_MATCHED,
	NT_SUBSCRIPTION_MATCHED
} NotificationType_t;

typedef void (*DDSNOTFCT) (Entity_t *ep, NotificationType_t type);

/* Callback function for notification dispatch. */

/* Notification service classes. */
#define	NSC_DCPS	0	/* DCPS service class notifications. */
#define	NSC_DISC	1	/* Discovery service class notifications. */

void dds_attach_notifier (unsigned nclass, DDSNOTFCT fct);

/* Add a notifier function for notifications of the given class. */

void dds_notify (unsigned class, Entity_t *ep, unsigned status);
/*void dds_notify (unsigned nclass, Entity_t *ep, NotificationType_t type);*/

/* Notify a listener for the given status. */

int dds_purge_notifications (Entity_t *ep, DDS_StatusMask status, int not_running);

/* Remove all outstanding notifications for the entity */

void dds_wait_listener (Entity_t *ep);

/* Wait until all listeners on a certain endpoint are ended */

void dds_defer_waitset_check (void *ep, void *cond);

/* Add a deferred waitset check for the given condition on an entity. */

void dds_defer_waitset_undo (void *ep, void *cond);

/* Remove a deferred waitset check for the given condition. */

void dds_config_update (Config_t c, CFG_NOTIFY_FCT fct);

/* Schedule a configuration parameter update. */

void dds_pool_dump (size_t sizes []);

/* Dump the DDS pools. */

#endif /* !__dds_h_ */

