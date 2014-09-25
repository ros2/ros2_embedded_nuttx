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

/* dds.c -- Global DDS initialization code.
            Note: some tweaking of the parameters based on the actual
	          environement is probably required. */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "win.h"
#else
#include <unistd.h>
#include <poll.h>
#endif
#include <errno.h>
#include <stdarg.h>
#include "thread.h"
#include "log.h"
#include "sys.h"
#include "config.h"
#include "ctrace.h"
#include "prof.h"
#include "sys.h"
#include "libx.h"
#include "skiplist.h"
#include "error.h"
#include "str.h"
#include "sock.h"
#include "parse.h"
#include "debug.h"
#include "typecode.h"
#ifdef XTYPES_USED
#include "xtypes.h"
#include "xdata.h"
#endif
#ifdef DDS_NATIVE_SECURITY
#include "sec_data.h"
#include "sec_auth.h"
#endif
#include "rtps.h"
#include "rtps_ip.h"
#include "rtps_mux.h"
#include "dynip.h"
#include "uqos.h"
#include "disc.h"
#include "dcps.h"
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"
#include "dds.h"

#define	KB		1024

static const DDS_PoolConstraints def_pool_reqs = {
	1,		/* Supported domains. */
	2, ~0,		/* Min/max Subscribers. */
	2, ~0,		/* Min/max Publishers. */
	12, ~0,		/* Min/max Local Readers. */
	8, ~0,		/* Min/max Local Writers. */
	16, ~0,		/* Min/max Topics. */
	1, ~0,		/* Min/max Filtered Topics. */
	10, ~0,		/* Min/max Topic Types. */
	10, ~0,		/* Min/max Reader proxies. */
	10, ~0,		/* Min/max Writer proxies. */
	8, ~0,		/* Min/max Remote Participants. */
	24, ~0,		/* Min/max Remote Readers. */
	24, ~0,		/* Min/max Remote Writers. */
	32 * KB, ~0,	/* Min/max Data Buffer pool size. */
	4,		/* # of 8K receive buffers. */
	64, ~0,		/* Min/max Cache change entries. */
	32, ~0,		/* Min/max Cache instance entries. */
	32, ~0,		/* Min/max Samples used in applications. */
	2, ~0,		/* Min/max Local matching caches. */
	2, ~0,		/* Min/max Cache waiter contexts. */
	2, ~0,		/* Min/max Cache pending transfers. */
	2, ~0,		/* Min/max Time-based filters. */
	2, ~0,		/* Min/max Time-based filter instances. */
	64, 0,		/* Min/max strings. */
	4 * KB, ~0,	/* Min/max string data storage. */
	32, ~0,		/* Min/max locators. */
	16, ~0,		/* Min/max uniques QoS combinations. */
	8, ~0,		/* Min/max Lists. */
	256, ~0,	/* Min/max List nodes. */
	32, ~0,		/* Min/max active Timers. */
	1024,		/* Max. # of IP sockets. */
	16,		/* Max. # of IPv4 addresses. */
	8,		/* Max. # of IPv6 addresses. */
	2, ~0,		/* Min/max WaitSets. */
	2, ~0,		/* Min/max StatusConditions. */
	2, ~0,		/* Min/max ReadConditions. */
	2, ~0,		/* Min/max QueryConditions. */
	2, ~0,		/* Min/max GuardConditions. */
	8, ~0,		/* Min/max Notifications. */
	2, ~0,		/* Min/max TopicWaits. */
	4, ~0,		/* Min/max Guards. */
	8, ~0,		/* Min/max Dynamic Types. */
	4, ~0,		/* Min/max Dynamic Data samples. */
	4, ~0,		/* Min/max Data Holders. */
	4, ~0,		/* Min/max Properties. */
	4, ~0,		/* Min/max BinaryProperties. */
	4, ~0,		/* Min/max HolderSequences. */
	0		/* Grow factor. */
};

static DDS_PoolConstraints *dds_pool_reqs;
static DDS_PoolConstraints dds_pool_data;
static int dds_pool_data_fixed;
static DDS_ExecEnv_t dds_environment;
static int dds_environment_fixed;
static unsigned dds_purge_delay;
static int dds_purge_delay_fixed;
const char *dds_entity_name;
static int dds_entity_name_fixed;
#ifdef RTPS_USED
static int rtps_use_fixed;
#endif
#ifdef THREADS_USED
#ifdef _WIN32
static HANDLE wakeup_event;
#else
static int dds_pipe_fds [2];
#endif
thread_t dds_core_thread;
#else
static int dds_continue;
#endif
static lock_t core_lock;

static lock_t ev_lock;

static Entity_t *ev_entity_in_listener;

static cond_t ev_wait;
static int listeners_waiting = 0;

static lock_t global_lock;
static lock_t pre_init_lock = LOCK_STATIC_INIT;

typedef struct pending_notify_st PendingNotify_t;
struct pending_notify_st {
	PendingNotify_t	*next;
	Entity_t	*entity;
	unsigned	class: 1;
	unsigned	status: 31;
};

static PendingNotify_t	*notify_head;
static PendingNotify_t	*notify_tail;

#define	MAX_NOTIFIERS	2

static DDSNOTFCT dds_notifiers [MAX_NOTIFIERS];

typedef struct pending_ws_chk_st PendingWSChk_t;
struct pending_ws_chk_st {
	PendingWSChk_t	*next;
	void		*entity;
	void		*condition;
};

static PendingWSChk_t	*defer_head;
static PendingWSChk_t	*defer_tail;

typedef struct pending_cfg_change_st PendingCfgChg_t;
struct pending_cfg_change_st {
	PendingCfgChg_t	*next;
	Config_t	id;
	CFG_NOTIFY_FCT	fct;
};

static PendingCfgChg_t	*cfg_change_head;
static PendingCfgChg_t	*cfg_change_tail;

enum mem_block_en {
	MB_NOTIFICATION,	/* DDS Notification. */
	MB_WS_CHECK,		/* DDS Waitset check. */
	MB_CFG_UPD,		/* DDS Config update. */

	MB_END
};

static const char *mem_names [] = {
	"NOTIFICATION",
	"WS_DEFERED",
	"CFG_UPDATE"
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory blocks. */
static size_t		mem_size;		/* Total allocated. */

static int		pre_init;		/* Preinitialized state. */
static int		tmr_suspend;		/* Timer suspend mode. */

#ifdef DDS_TRACE
unsigned		dds_dtrace;		/* Default tracing mode. */
#endif
size_t			dds_max_sample_size;	/* Maximum sample size. */
#ifdef PROFILE
PROF_PID (dds_w_events)
PROF_PID (dds_w_sleep)
PROF_PID (dds_w_fd)
PROF_PID (dds_w_tmr)
PROF_PID (dds_w_cache)
PROF_PID (dds_w_waitset)
PROF_PID (dds_w_listen)
#endif
#ifdef CTRACE_USED

enum {
	DDS_EXEC_ENV,
	DDS_G_CONSTR,
	DDS_S_CONSTR,
	DDS_PROG_NAME,
	DDS_WORK_EV,
	DDS_WORK_POLL,
	DDS_WORK_CONT,
	DDS_WORK_QUIT,
	DDS_WORK_IO,
	DDS_WORK_TMR,
	DDS_WORK_PROXY,
	DDS_WORK_XFER,
	DDS_WORK_NOTIF,
	DDS_WORK_DONE,
	DDS_WAKEUP,
	DDS_SCHED,
	DDS_WAIT,
	DDS_CONTINUE,
	DDS_NOTIFY,
	DDS_DEFER_WS,
	DDS_UNDO_WS,
	DDS_WORK_WS,
	DDS_CFG_CHG,
	DDS_CFG_NOTIF
};

static const char *dds_fct_str [] = {
	"EXEC_ENV", "G_CONSTR", "S_CONSTR", "PROG_NAME",
	"W_EV", "W_POLL", "W_CONT", "W_QUIT", "W_IO", "W_TMR", "W_PROXY",
	"W_XFER", "W_NOTIF", "W_DONE", "WAKEUP", "SCHED", "WAIT", "CONTINUE",
	"NOTIFY", "DEFER_WS", "UNDO_WS", "WORK_WS", "CFG_CHG", "CFG_NOTIF"
};

#endif

DDS_ReturnCode_t DDS_execution_environment (DDS_ExecEnv_t eid)
{
	ctrc_printd (DDS_ID, DDS_EXEC_ENV, &eid, sizeof (eid));
	if ((int) eid < DDS_EE_C || (eid > DDS_EE_JAVA && eid < DDS_EE_CDD))
		return (DDS_RETCODE_UNSUPPORTED);

	if (eid == DDS_EE_CDD)
		return (DDS_RETCODE_BAD_PARAMETER);

	dds_environment = eid;
	dds_environment_fixed = 1;

	return (DDS_RETCODE_OK);
}

#define	DDS_PRE_INIT()	dds_pre_init ()

DDS_ReturnCode_t DDS_alloc_fcts_set (void *(*pmalloc) (size_t size),
				     void *(*prealloc) (void *ptr, size_t size),
				     void (*pfree) (void *ptr))
{
	if (pre_init)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	if (!pmalloc || !prealloc || !pfree)
		return (DDS_RETCODE_BAD_PARAMETER);

	mm_fcts.alloc_ = pmalloc;
	mm_fcts.realloc_ = prealloc;
	mm_fcts.free_ = pfree;
	return (DDS_RETCODE_OK);
}

#define	MAX_SET(r,n,f)	(r)->max_##n=(f==~0U)?~0U:((r)->min_##n*(100U+f))/100U

DDS_ReturnCode_t DDS_get_default_pool_constraints (DDS_PoolConstraints *reqs,
						   unsigned max_factor,
						   unsigned grow_factor)
{
	if (!reqs)
		return (DDS_RETCODE_BAD_PARAMETER);

	ctrc_printd (DDS_ID, DDS_G_CONSTR, &reqs, sizeof (reqs));
	memcpy (reqs, &def_pool_reqs, sizeof (DDS_PoolConstraints));

	MAX_SET (reqs, subscribers, max_factor);
	MAX_SET (reqs, publishers, max_factor);
	MAX_SET (reqs, local_readers, max_factor);
	MAX_SET (reqs, local_writers, max_factor);
	MAX_SET (reqs, topics, max_factor);
	MAX_SET (reqs, filtered_topics, max_factor);
	MAX_SET (reqs, topic_types, max_factor);
	MAX_SET (reqs, reader_proxies, max_factor);
	MAX_SET (reqs, writer_proxies, max_factor);
	MAX_SET (reqs, remote_participants, max_factor);
	MAX_SET (reqs, remote_readers, max_factor);
	MAX_SET (reqs, remote_writers, max_factor);
	MAX_SET (reqs, pool_data, max_factor);
	MAX_SET (reqs, changes, max_factor);
	MAX_SET (reqs, instances, max_factor);
	MAX_SET (reqs, application_samples, max_factor);
	MAX_SET (reqs, local_match, max_factor);
	MAX_SET (reqs, cache_waiters, max_factor);
	MAX_SET (reqs, cache_transfers, max_factor);
	MAX_SET (reqs, time_filters, max_factor);
	MAX_SET (reqs, time_instances, max_factor);
	MAX_SET (reqs, strings, max_factor);
	MAX_SET (reqs, string_data, max_factor);
	MAX_SET (reqs, locators, max_factor);
	MAX_SET (reqs, qos, max_factor);
	MAX_SET (reqs, lists, max_factor);
	MAX_SET (reqs, list_nodes, max_factor);
	MAX_SET (reqs, timers, max_factor);
	MAX_SET (reqs, waitsets, max_factor);
	MAX_SET (reqs, statusconditions, max_factor);
	MAX_SET (reqs, readconditions, max_factor);
	MAX_SET (reqs, queryconditions, max_factor);
	MAX_SET (reqs, guardconditions, max_factor);
	MAX_SET (reqs, notifications, max_factor);
	MAX_SET (reqs, topicwaits, max_factor);
	MAX_SET (reqs, guards, max_factor);
	MAX_SET (reqs, dyn_types, max_factor);
	MAX_SET (reqs, dyn_samples, max_factor);
	MAX_SET (reqs, data_holders, max_factor);
	MAX_SET (reqs, properties, max_factor);
	MAX_SET (reqs, binary_properties, max_factor);
	MAX_SET (reqs, holder_sequences, max_factor);

	reqs->grow_factor = grow_factor;

	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_get_pool_constraints (DDS_PoolConstraints *reqs)
{
	if (!reqs)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_PRE_INIT ();
	*reqs = dds_pool_data;
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_set_pool_constraints (const DDS_PoolConstraints *reqs)
{
	ctrc_printd (DDS_ID, DDS_S_CONSTR, &reqs, sizeof (reqs));
	if (!reqs)
		return (DDS_RETCODE_BAD_PARAMETER);

	dds_pool_data = *reqs;
	dds_pool_reqs = &dds_pool_data;
	dds_pool_data_fixed = 1;

	return (DDS_RETCODE_OK);
}

void DDS_program_name (int *argc, char *argv [])
{
	ctrc_printd (DDS_ID, DDS_PROG_NAME, argv [0], strlen (argv [0]));
	err_prog_args (argc, argv);
}

void DDS_entity_name (const char *name)
{
	dds_entity_name = name;
	dds_entity_name_fixed = 1;
}

/* DDS_RTPS_control -- Control the activity of the RTPS/Discovery layers. */

void DDS_RTPS_control (int enable)
{
#ifdef RTPS_USED
	rtps_used = enable;
	rtps_use_fixed = 1;
#else
	ARG_NOT_USED (enable)
#endif
}

#define	EXTCNT(r,f)	r->min_##f < r->max_##f

unsigned ext_cnt (unsigned min, unsigned max)
{
	if (max == ~0U)
		return (~0U);
	else if (max <= min)
		return (0);
	else
		return (max - min);
}

unsigned dds_ev_pending;
int dds_sleeping;

/* int wakeup_queued; */

void dds_lock_ev (void)
{
	lock_take (ev_lock);
}

void dds_unlock_ev (void)
{
	lock_release (ev_lock);
}

void dds_lock_core (void)
{
	lock_take (core_lock);
}

void dds_unlock_core (void)
{
	lock_release (core_lock);
}

void dds_lock_domains (void)
{
	lock_take (global_lock);
}

void dds_unlock_domains (void)
{
	lock_release (global_lock);
}

#ifdef THREADS_USED

static void dds_wakeup (void)
{
#ifndef _WIN32
	char	ch = '.';
	ssize_t	res;
#endif

	/* wakeup_queued = 1; */
#ifdef _WIN32
	/*printf ("<Wakeup>");*/
	SetEvent (wakeup_event);
#else
	res = write (dds_pipe_fds [1], &ch, 1);
	if (res != 1)
		fatal_printf ("dds_wakeup: write() failure!");
#endif
}
#endif

void dds_signal (unsigned event)
{
	unsigned	prev_ev_pending;

	lock_take (ev_lock);

	prev_ev_pending = dds_ev_pending;
	dds_ev_pending |= event;
#ifdef THREADS_USED
	if (!prev_ev_pending && dds_sleeping
	    /*&& !wakeup_queued <-- leads to too late events!!! */ )
		dds_wakeup ();
#endif
	lock_release (ev_lock);
}

void dds_notify (unsigned class, Entity_t *ep, unsigned status)
{
	PendingNotify_t	*p;
#ifdef THREADS_USED
	int wakeup = 0;
#endif

	if ((ep->flags & EF_SHUTDOWN) != 0)
		return;

	if (class >= MAX_NOTIFIERS || !dds_notifiers [class]) {
		warn_printf ("Invalid notification class.");
		return;
	}
	p = mds_pool_alloc (&mem_blocks [MB_NOTIFICATION]);
	if (!p) {
		warn_printf ("Out-of-memory for pending notification.");
		return;
	}
	p->next = NULL;
	p->entity = ep;
	p->class = class;
	p->status = status;
	ctrc_printd (DDS_ID, DDS_NOTIFY, &p->entity,
			     sizeof (p->entity) + sizeof (unsigned));
	lock_take (ev_lock);
	if (!notify_head)
		notify_head = p;
	else
		notify_tail->next = p;
	notify_tail = p;
	dds_ev_pending |= DDS_EV_NOTIFY;
#ifdef THREADS_USED
	if (dds_sleeping)
		wakeup = 1;
#endif
	lock_release (ev_lock);
#ifdef THREADS_USED
	/* Might be called too much, but not really an issue */
	if (wakeup)
		dds_wakeup ();
#endif
}

void dds_defer_waitset_check (void *e, void *cond)
{
	PendingWSChk_t	*p;
#ifdef THREADS_USED
	int wakeup = 0;
#endif

	p = mds_pool_alloc (&mem_blocks [MB_WS_CHECK]);
	if (!p) {
		warn_printf ("Out-of-memory for defered waitset.");
		return;
	}
	p->entity = e;
	p->condition = cond;
	p->next = NULL;

	ctrc_printd (DDS_ID, DDS_DEFER_WS, &cond, sizeof (cond));
	lock_take (ev_lock);
	if (!defer_head)
		defer_head = p;
	else
		defer_tail->next = p;
	defer_tail = p;
	dds_ev_pending |= DDS_EV_WAITSET;
#ifdef THREADS_USED
	if (dds_sleeping)
		wakeup = 1;
#endif
	lock_release (ev_lock);
#ifdef THREADS_USED
	/* Might be called too much, but not really an issue */
	if (wakeup)
		dds_wakeup ();
#endif
}

static void dds_waitset_checks (unsigned *events)
{
	PendingWSChk_t	*p;

	lock_take (ev_lock);
	p = defer_head;
	if (p)
		defer_head = p->next;
	if (!defer_head) {
		dds_ev_pending &= ~DDS_EV_WAITSET;
		*events &= ~DDS_EV_WAITSET;
	}
	lock_release (ev_lock);
	if (!p)
		return;

	dcps_deferred_waitset_check (p->entity, p->condition);
	mds_pool_free (&mem_blocks [MB_WS_CHECK], p);
}

void dds_defer_waitset_undo (void *e, void *cond)
{
	PendingWSChk_t	*p, *xp, *prev;

	ctrc_printd (DDS_ID, DDS_UNDO_WS, &cond, sizeof (cond));

	if (!defer_head)
		return;

	lock_take (ev_lock);
	for (prev = NULL, p = defer_head; p; )
		if (p->entity == e && p->condition == cond) {
			xp = p;
			p = p->next;
			if (prev)
				prev->next = p;
			else
				defer_head = p;
			mds_pool_free (&mem_blocks [MB_WS_CHECK], xp);
		}
		else {
			prev = p;
			p = p->next;
		}

	if (!defer_head) {
		defer_tail = NULL;
		dds_ev_pending &= ~DDS_EV_WAITSET;
	}
	lock_release (ev_lock);
}

void dds_config_update (Config_t c, CFG_NOTIFY_FCT fct)
{
	PendingCfgChg_t	*p;
#ifdef THREADS_USED
	int		wakeup = 0;

	if (!dds_core_thread || thread_id () == dds_core_thread) {
		(*fct) (c);
		return;
	}
#endif
	p = mds_pool_alloc (&mem_blocks [MB_CFG_UPD]);
	if (!p) {
		warn_printf ("Out-of-memory for scheduled configuration update.");
		return;
	}
	p->id = c;
	p->fct = fct;
	p->next = NULL;

	ctrc_printd (DDS_ID, DDS_CFG_CHG, &c, sizeof (c));
	lock_take (ev_lock);
	if (!cfg_change_head)
		cfg_change_head = p;
	else
		cfg_change_tail->next = p;
	cfg_change_tail = p;
	dds_ev_pending |= DDS_EV_CONFIG;
#ifdef THREADS_USED
	if (dds_sleeping)
		wakeup = 1;
#endif
	lock_release (ev_lock);
#ifdef THREADS_USED
	/* Might be called too much, but not really an issue */
	if (wakeup)
		dds_wakeup ();
#endif
}

static void dds_config_checks (unsigned *events)
{
	PendingCfgChg_t	*p;

	lock_take (ev_lock);
	p = cfg_change_head;
	if (p)
		cfg_change_head = p->next;
	if (!cfg_change_head) {
		dds_ev_pending &= ~DDS_EV_CONFIG;
		*events &= ~DDS_EV_CONFIG;
	}
	lock_release (ev_lock);
	if (!p)
		return;

	(*p->fct) (p->id);
	mds_pool_free (&mem_blocks [MB_CFG_UPD], p);
}

static void dds_client_notify (unsigned *events)
{
	PendingNotify_t	*p;

	lock_take (ev_lock);
	p = notify_head;
	if (p) {
		notify_head = p->next;
		ev_entity_in_listener = p->entity;
        }
	if (!notify_head) {
		dds_ev_pending &= ~DDS_EV_NOTIFY;
		*events &= ~DDS_EV_NOTIFY;
	}
	lock_release (ev_lock);
	if (!p)
		return;

	dds_notifiers [p->class] (p->entity, p->status);

	lock_take (ev_lock);
	ev_entity_in_listener = NULL;
	if (listeners_waiting == 1)
		cond_signal (ev_wait);
	else if (listeners_waiting)
		cond_signal_all (ev_wait);
	lock_release (ev_lock);

	mds_pool_free (&mem_blocks [MB_NOTIFICATION], p);
}

void dds_wait_listener (Entity_t *ep)
{
	lock_take (ev_lock);
	while (ep == ev_entity_in_listener) {
		listeners_waiting++;
		cond_wait (ev_wait, ev_lock);
		listeners_waiting--;
	}
	lock_release (ev_lock);
}

/* dds_purge_notifications -- Purge notifications for a given endpoint, and
			      with statuses matching a status mask from the
			      event queue. The not running argument can be used
			      to signal that the function need to return
			      immediately when a listener is still running.
			      This function will return zero when there was
			      still a listener running (regardless of whether
			      not_running is used or not). Otherwise it will
			      return 1. */

int dds_purge_notifications (Entity_t *ep, DDS_StatusMask status, int not_running)
{
	PendingNotify_t	*p, *prev, *next;

	lock_take (ev_lock);
	if (not_running && ep == ev_entity_in_listener) {
		lock_release (ev_lock);
		return (0);
	}

	for (p = notify_head, prev = NULL; p; p = next) {
		next = p->next;
		if (p->entity == ep && (status & (1 << p->status)) != 0) {
			if (prev)
				prev->next = p->next;
			else
				notify_head = p->next;

			if (p == notify_tail)
				notify_tail = prev;

			mds_pool_free (&mem_blocks [MB_NOTIFICATION], p);
		}
		else
			prev = p;
	}

	if (!not_running && ep == ev_entity_in_listener) {
		lock_release (ev_lock);
		return (0);
	}

	lock_release (ev_lock);
	return (1);
}

/* dds_attach_notifier -- Add a notifier function for notifications of the
			  given class. */

void dds_attach_notifier (unsigned class, DDSNOTFCT fct)
{
	if (class >= MAX_NOTIFIERS)
		return;

	dds_notifiers [class] = fct;
}

static int dds_work (unsigned max_wait_ms)
{
	unsigned	i;
	unsigned	events;
	unsigned	tmr_delay;

	for (i = 0; i < 32; ) {

		/* Get the set of events to process. */
		prof_start (dds_w_events);
		lock_take (ev_lock);
		events = dds_ev_pending;
		dds_ev_pending = 0;
		tmr_delay = (tmr_suspend) ? ~0U : tmr_pending_ms ();
		if (!tmr_delay) {
			events |= DDS_EV_TMR;
			dds_sleeping = 0;
		}
		else 
			dds_sleeping = (events == 0);

		lock_release (ev_lock);
		ctrc_printd (DDS_ID, DDS_WORK_EV, &events, sizeof (events));
		prof_stop (dds_w_events, 1);

		/* Check if there is work to do. */
		if (dds_sleeping) {

			/* If just entered: sleep, else return. */
			if (i)
				break;

			/* First entry and nothing to do - go to sleep. */
			prof_start (dds_w_sleep);
			if (tmr_delay > max_wait_ms)
				tmr_delay = max_wait_ms;
			ctrc_printd (DDS_ID, DDS_WORK_POLL, &tmr_delay, sizeof (tmr_delay));
			sock_fd_poll (tmr_delay);
			ctrc_printd (DDS_ID, DDS_WORK_CONT, NULL, 0);
			lock_take (ev_lock);
			dds_sleeping = 0;
			lock_release (ev_lock);
			i = 0;
			prof_stop (dds_w_sleep, 1);
			continue;
		}
		else
			/* Work to do: handle each event. */
			do {
				if ((events & DDS_EV_QUIT) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_QUIT, NULL, 0);
					return (1);
				}
				else if ((events & DDS_EV_TMR) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_TMR, NULL, 0);
					prof_start (dds_w_tmr);
					events &= ~DDS_EV_TMR;
					tmr_manage ();
					prof_stop (dds_w_tmr, 1);
				}
				else if ((events & DDS_EV_IO) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_IO, NULL, 0);
					prof_start (dds_w_fd);
					events &= ~DDS_EV_IO;
					sock_fd_schedule ();
					prof_stop (dds_w_fd, 1);
				}
#ifdef RTPS_USED
				else if (rtps_used && (events & DDS_EV_PROXY_NE) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_PROXY, NULL, 0);
					events &= ~DDS_EV_PROXY_NE;
					rtps_send_changes ();
				}
#endif
				else if ((events & DDS_EV_CACHE_X) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_XFER, NULL, 0);
					prof_start (dds_w_cache);
					events &= ~DDS_EV_CACHE_X;
					hc_transfer_samples ();
					prof_stop (dds_w_cache, 1);
				}
				else if ((events & DDS_EV_WAITSET) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_WS, NULL, 0);
					prof_start (dds_w_waitset);
					dds_waitset_checks (&events);
					prof_stop (dds_w_waitset, 1);
				}
				else if ((events & DDS_EV_NOTIFY) != 0) {
					ctrc_printd (DDS_ID, DDS_WORK_NOTIF, NULL, 0);
					prof_start (dds_w_listen);
					dds_client_notify (&events);
					prof_stop (dds_w_listen, 1);
				}
				else if ((events & DDS_EV_CONFIG) != 0) {
					ctrc_printd (DDS_ID, DDS_CFG_NOTIF, NULL, 0);
					dds_config_checks (&events);
				}
			}
			while (events);
		i++;
	}
	ctrc_printd (DDS_ID, DDS_WORK_DONE, NULL, 0);
	return (0);
}

#ifdef THREADS_USED

static DDS_exit_cb thread_exit_cb = NULL;

void DDS_atexit(DDS_exit_cb cb)
{
	thread_exit_cb = cb;
}

static thread_result_t dds_core (void *arg)
{
	ARG_NOT_USED (arg)

	log_printf (DDS_ID, 0, "DDS: core thread running.\r\n");
	for (;;) {
		if (dds_work (2000)) {
			if (thread_exit_cb)
				thread_exit_cb ();

			thread_exit (0);
		}
	}
	thread_return (NULL);
}

static void dds_wakeup_event (HANDLE h, short revents, void *arg)
{
#ifndef _WIN32
	char		ch;
	int		n;
#else
	ARG_NOT_USED (h)
#endif
	ARG_NOT_USED (revents)
	ARG_NOT_USED (arg)

	ctrc_printd (DDS_ID, DDS_WAKEUP, NULL, 0);

#ifndef _WIN32
	/* wakeup_queued = 0; */
	n = read (h, &ch, 1);
	(void) n;
#endif
}

/* dds_init_threads -- Called to create all necessary synchronisation data and
		       worker threads needed for DDS processing. */

static int dds_init_threads (void)
{
#ifdef DUMP_PRIO
	int			policy, error;
	struct sched_param	param;
#endif
	HANDLE			wakeup_handle;

	thread_init ();
	lock_init_nr (core_lock, "core");
	lock_init_nr (ev_lock, "ev");
	cond_init (ev_wait);
	lock_init_r (global_lock, "domain");

#ifdef _WIN32
	if ((wakeup_event = CreateEventA (NULL, 0, FALSE, "DDS_Wakeup")) == NULL) {
		err_printf ("dds_init_threads: CreateEvent() call failed - error = %u.\r\n", GetLastError);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	wakeup_handle = wakeup_event;
#else
	if (pipe (dds_pipe_fds) == -1) {
		perror ("dds_init_threads: pipe()");
		err_printf ("dds_init_threads: pipe() call failed - errno = %d.\r\n", errno);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
	wakeup_handle = dds_pipe_fds [0];
#endif
	sock_fd_add_handle (wakeup_handle,
			    POLLIN | POLLPRI | POLLHUP | POLLNVAL,
			    dds_wakeup_event,
			    0,
			    "DDS.wakeup");

	thread_create (dds_core_thread, dds_core, 0);
	log_printf (DDS_ID, 0, "DDS: core thread created.\r\n");
#if DUMP_PRIO
	error = pthread_getschedparam (dds_core_thread, &policy, &param);
	if (!error) {
		printf ("Scheduling policy = ");
		if (policy == SCHED_FIFO)
			printf ("SCHED_FIFO");
		else if (policy == SCHED_RR)
			printf ("SCHED_RR");
		else if (policy == SCHED_OTHER)
			printf ("SCHED_OTHER");
		else
			printf ("???");
		printf (", priority = %u\r\n", param.sched_priority);
	}
#endif
	return (DDS_RETCODE_OK);
}

/* dds_finalize_threads -- Finalize all worker threads. */

static void dds_finalize_threads (void)
{
	log_printf (DDS_ID, 0, "DDS: stopping core thread.\r\n");
	dds_signal (DDS_EV_QUIT);
	thread_wait (dds_core_thread, NULL);
	memset (&dds_core_thread, 0, sizeof (thread_t));
	log_printf (DDS_ID, 0, "DDS: core thread exited.\r\n");

        lock_destroy (global_lock);
        cond_destroy (ev_wait);
        lock_destroy (ev_lock);
        lock_destroy (core_lock);

#ifndef _WIN32
	sock_fd_remove_handle (dds_pipe_fds [0]);
	close (dds_pipe_fds [0]);
	close (dds_pipe_fds [1]);
#endif
}

#endif

#ifdef CTRACE_ATEXIT
#ifdef CTRACE_USED
static void dumpcrtc (void)
{
	unsigned	pid = sys_pid ();
	char		output [100];

	sprintf (output, "/tmp/ctrace.%d", pid);
	ctrc_save (output);
}
#endif
#endif

void dds_post_final (void)
{
	lock_take (pre_init_lock);
	if (!pre_init) {
		lock_release (pre_init_lock);
		return;
	}

#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
	if (local_identity)
		sec_release_identity (local_identity);
#endif
#endif
	sock_fd_final ();
	sys_final ();
	dds_typesupport_final ();
	str_pool_free ();
	sl_pool_free ();
	pool_final_stats ();
	config_flush ();

#ifdef CTRACE_ATEXIT
	dumpcrtc ();
#endif
	pool_post_final ();

	pre_init = 0;
	lock_release (pre_init_lock);
}

/* dds_pool_data_init -- Set dds_pool_data to correct defaults. */

static void dds_pool_data_init (void)
{
	DDS_PoolConstraints *reqs;

	reqs = &dds_pool_data;
	*reqs = def_pool_reqs;
	reqs->max_domains = config_get_number (DC_Pool_Domains, reqs->max_domains);
	config_get_range (DC_Pool_Subscribers, &reqs->min_subscribers, &reqs->max_subscribers);
	config_get_range (DC_Pool_Publishers, &reqs->min_publishers, &reqs->max_publishers);
	config_get_range (DC_Pool_Readers, &reqs->min_local_readers, &reqs->max_local_readers);
	config_get_range (DC_Pool_Writers, &reqs->min_local_writers, &reqs->max_local_writers);
	config_get_range (DC_Pool_Topics, &reqs->min_topics, &reqs->max_topics);
	config_get_range (DC_Pool_FilteredTopics, &reqs->min_filtered_topics, &reqs->max_filtered_topics);
	config_get_range (DC_Pool_TopicTypes, &reqs->min_topic_types, &reqs->max_topic_types);
	config_get_range (DC_Pool_ReaderProxies, &reqs->min_reader_proxies, &reqs->max_reader_proxies);
	config_get_range (DC_Pool_WriterProxies, &reqs->min_writer_proxies, &reqs->max_writer_proxies);
	config_get_range (DC_Pool_DiscParticipants, &reqs->min_remote_participants, &reqs->max_remote_participants);
	config_get_range (DC_Pool_DiscReaders, &reqs->min_remote_readers, &reqs->max_remote_readers);
	config_get_range (DC_Pool_DiscWriters, &reqs->min_remote_writers, &reqs->max_remote_writers);
	config_get_range (DC_Pool_PoolData, &reqs->min_pool_data, &reqs->max_pool_data);
	reqs->max_rx_buffers = config_get_number (DC_Pool_RxBuffers, reqs->max_rx_buffers);
	config_get_range (DC_Pool_Changes, &reqs->min_changes, &reqs->max_changes);
	config_get_range (DC_Pool_Instances, &reqs->min_instances, &reqs->max_instances);
	config_get_range (DC_Pool_Samples, &reqs->min_application_samples, &reqs->max_application_samples);
	config_get_range (DC_Pool_LocalMatch, &reqs->min_local_match, &reqs->max_local_match);
	config_get_range (DC_Pool_CacheWait, &reqs->min_cache_waiters, &reqs->max_cache_waiters);
	config_get_range (DC_Pool_CacheXfer, &reqs->min_cache_transfers, &reqs->max_cache_transfers);
	config_get_range (DC_Pool_TimeFilters, &reqs->min_time_filters, &reqs->max_time_filters);
	config_get_range (DC_Pool_TimeInsts, &reqs->min_time_instances, &reqs->max_time_instances);
	config_get_range (DC_Pool_Strings, &reqs->min_strings, &reqs->max_strings);
	config_get_range (DC_Pool_StringData, &reqs->min_string_data, &reqs->max_string_data);
	config_get_range (DC_Pool_Locators, &reqs->min_locators, &reqs->max_locators);
	config_get_range (DC_Pool_QoS, &reqs->min_qos, &reqs->max_qos);
	config_get_range (DC_Pool_Lists, &reqs->min_lists, &reqs->max_lists);
	config_get_range (DC_Pool_ListNodes, &reqs->min_list_nodes, &reqs->max_list_nodes);
	config_get_range (DC_Pool_Timers, &reqs->min_timers, &reqs->max_timers);
	config_get_range (DC_Pool_WaitSets, &reqs->min_waitsets, &reqs->max_waitsets);
	config_get_range (DC_Pool_StatusConds, &reqs->min_statusconditions, &reqs->max_statusconditions);
	config_get_range (DC_Pool_ReadConds, &reqs->min_readconditions, &reqs->max_readconditions);
	config_get_range (DC_Pool_QueryConds, &reqs->min_queryconditions, &reqs->max_queryconditions);
	config_get_range (DC_Pool_GuardConds, &reqs->min_guardconditions, &reqs->max_guardconditions);
	config_get_range (DC_Pool_Notifications, &reqs->min_notifications, &reqs->max_notifications);
	config_get_range (DC_Pool_TopicWaits, &reqs->min_topicwaits, &reqs->max_topicwaits);
	config_get_range (DC_Pool_Guards, &reqs->min_guards, &reqs->max_guards);
	config_get_range (DC_Pool_DynTypes, &reqs->min_dyn_types, &reqs->max_dyn_types);
	config_get_range (DC_Pool_DynSamples, &reqs->min_dyn_samples, &reqs->max_dyn_samples);
	reqs->grow_factor = config_get_number (DC_Pool_Growth, reqs->grow_factor);
	dds_pool_reqs = &dds_pool_data;
}

/* dds_pre_init -- Initialize the DDS permanent context data (survives restarts
		   of domainparticipants). */

void dds_pre_init (void)
{
	int		error;
	unsigned	min_endpoints, max_endpoints;
	DDS_PoolConstraints *reqs;
	POOL_LIMITS	sl_pools, sl_nodes, str, refs;
#ifdef XTYPES_USED
	POOL_LIMITS	dtypes, ddata;
#endif

	lock_take (pre_init_lock);
	if (pre_init) {
		lock_release (pre_init_lock);
		return;
	}

	pool_pre_init ();

	config_load ();

	if (!dds_pool_data_fixed)
		dds_pool_data_init ();
	reqs = &dds_pool_data;

	if (!dds_environment_fixed)
		dds_environment = config_get_number (DC_Environment, DDS_EE_C);
	if (!dds_purge_delay_fixed)
		dds_purge_delay = config_get_number (DC_PurgeDelay, 50000);
	if (!dds_entity_name_fixed)
		dds_entity_name = config_get_string (DC_Name, NULL);
	dds_max_sample_size = config_get_number (DC_SampleSize, MAX_SAMPLE_SIZE);
#ifdef RTPS_USED
	if (!rtps_use_fixed)
		rtps_used = config_get_mode (DC_RTPS_Mode, rtps_used);
#endif
#ifdef DDS_TRACE
	dds_dtrace = config_get_number (DC_RTPS_DefTrace, DDS_TRACE_NONE);
#endif
#ifdef CTRACE_USED
	/* Initialize the cyclic trace. */
	ctrc_init (0x800000);
	log_fct_str [DDS_ID] = dds_fct_str;
#endif

#ifdef PROFILE
	prof_init ();
#endif
	sys_init ();

	pool_init_stats ();

	pool_limits_set (sl_pools, reqs->min_lists, reqs->max_lists, reqs->grow_factor);
	pool_limits_set (sl_nodes, reqs->min_list_nodes, reqs->max_list_nodes, reqs->grow_factor);
	error = sl_pool_init (&sl_pools, &sl_nodes, sizeof (void *));
	if (error) {
		lock_release (pre_init_lock);
		fatal_printf ("sl_pool_init() failed: error = %d", error);
	}

	log_printf (DDS_ID, 0, "List pools created.\r\n");

	min_endpoints = reqs->min_topics + reqs->min_topic_types +
			reqs->min_remote_readers + reqs->min_remote_writers;
	max_endpoints = (reqs->max_topics == ~0U || reqs->max_topic_types == ~0U ||
			 reqs->max_remote_readers == ~0U || reqs->max_remote_writers == ~0U) ?
			 	~0UL :
			reqs->max_topics + reqs->max_topic_types +
			reqs->max_remote_readers + reqs->max_remote_writers;
	pool_limits_set (str, reqs->min_strings, reqs->max_strings, reqs->grow_factor);
	pool_limits_set (refs, min_endpoints, max_endpoints, reqs->grow_factor);
	error = str_pool_init (&str, &refs, reqs->min_string_data,
			       (reqs->min_string_data < reqs->max_string_data) ? 1 : 0);
	if (error) {
		lock_release (pre_init_lock);
		fatal_printf ("str_pool_init() failed: error = %d", error);
	}

	log_printf (DDS_ID, 0, "String pool initialized.\r\n");

#ifdef XTYPES_USED
	pool_limits_set (dtypes, reqs->min_dyn_types,
				 reqs->max_dyn_types,
				 reqs->grow_factor);
	pool_limits_set (ddata, reqs->min_dyn_samples,
				reqs->max_dyn_samples,
				reqs->grow_factor);
	error = xd_pool_init (&dtypes, &ddata);
	if (error) {
		lock_release (pre_init_lock);
		fatal_printf ("xd_pool_init() failed: error = %d", error);
	}

	log_printf (DDS_ID, 0, "Typecode pools created.\r\n");
#endif

	dds_typesupport_init ();
	log_printf (DDS_ID, 0, "Typesupport initialized.\r\n");

	error = sock_fd_init ();
	if (error) {
		lock_release (pre_init_lock);
		fatal_printf ("sock_fd_init() failed: error = %d", error);
	}

	log_printf (DDS_ID, 0, "Socket handler initialized.\r\n");

	bc_init ();
	sql_parse_init ();

	atexit (dds_post_final);
	
	pre_init = 1;
	lock_release (pre_init_lock);
}

static Timer_t	shm_timer;

static void dds_assert_shm_liveness (uintptr_t user)
{
	Domain_t        *dp;
	unsigned        i = 0;

	ARG_NOT_USED (user)

	while ((dp = domain_next (&i, NULL)) != NULL)
		guid_restate_participant (dp->participant_id);

	tmr_start (&shm_timer, TICKS_PER_SEC, 0, dds_assert_shm_liveness);
}

/* dds_init -- Main DDS initialisation. */

int dds_init (void)
{
	int		error;
	DB_POOL		pools [8], *pp;
	DomainCfg_t	domain_cfg;
	DCPS_CONFIG	dcps_cfg;
#ifdef RTPS_USED
	RTPS_CONFIG 	rtps_cfg;
#endif
	CACHE_CONFIG	cache_cfg;
#ifdef DDS_NATIVE_SECURITY
	SEC_CONFIG	sec_cfg;
#endif
	DDS_PoolConstraints *reqs;
	unsigned	i, min_endpoints, max_endpoints, size;
	GuidPrefix_t	*gp;
	POOL_LIMITS	loc_refs, locators,
			tmr_pool, qos_refs, qos_data, notif_data, def_ws_data;

	DDS_PRE_INIT ();
	reqs = dds_pool_reqs;

	/* Calculate data pool sizes. */
	for (i = 0, pp = &pools [7], size = 64; i < 8; i++, pp--, size <<= 1) {
		pp->msgdata.reserved = reqs->min_pool_data >> (4 + 6 + i);
		if (!pp->msgdata.reserved)
			pp->msgdata.reserved = 1;
		pp->msgdata.extra = (reqs->min_pool_data == reqs->max_pool_data) ? 0 : ~0;
		pp->msgdata.grow = pool_grow_amount (pp->msgdata.reserved,
						pp->msgdata.extra, reqs->grow_factor);
		pp->maxmsgdata = size;
	}
#ifdef USE_RECVMSG
	pools [0].msgdata.reserved += reqs->max_rx_buffers;
#endif

	pool_limits_set (tmr_pool, reqs->min_timers, reqs->max_timers, reqs->grow_factor);
	error = tmr_pool_init (&tmr_pool);
	if (error)
		fatal_printf ("tmr_pool_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "Timer pool initialized.\r\n");

	error = db_pool_init (sizeof (pools) / sizeof (DB_POOL), pools);
	if (error)
		fatal_printf ("db_pool_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "Data buffer pools created.\r\n");

	error = guid_init (dds_environment);
	if (error)
		fatal_printf ("guid_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "Unique GUID prefix created: ");
	gp = guid_local ();
	for (i = 0; i < 12; i++) {
		if (i && (i & 0x3) == 0)
			log_printf (DDS_ID, 0, ":");
		log_printf (DDS_ID, 0, "%02x", gp->prefix [i]);
	}
	log_printf (DDS_ID, 0, "\r\n");

	min_endpoints = reqs->min_topics + reqs->min_topic_types +
			reqs->min_remote_readers + reqs->min_remote_writers;
	max_endpoints = (reqs->max_topics == ~0U || reqs->max_topic_types == ~0U ||
			 reqs->max_remote_readers == ~0U || reqs->max_remote_writers == ~0U) ?
			 	~0UL :
			reqs->max_topics + reqs->max_topic_types +
			reqs->max_remote_readers + reqs->max_remote_writers;
	pool_limits_set (loc_refs, min_endpoints, max_endpoints, reqs->grow_factor);
	pool_limits_set (locators, reqs->min_locators, reqs->max_locators, reqs->grow_factor);
	error = locator_pool_init (&loc_refs, &locators);
	if (error)
		fatal_printf ("locator_pool_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "Locator pools created.\r\n");

	min_endpoints = reqs->min_local_readers + reqs->min_local_writers + 6;
	max_endpoints = (reqs->max_local_readers == ~0U || reqs->max_local_writers == ~0U) ?
				~0U : reqs->max_local_readers +
				      reqs->max_local_writers + 6;
	pool_limits_set (cache_cfg.cache, min_endpoints, max_endpoints,
							     reqs->grow_factor);
	pool_limits_set (cache_cfg.instance, reqs->min_instances,
					     reqs->max_instances, reqs->grow_factor);
	pool_limits_set (cache_cfg.change, reqs->min_changes,
					   reqs->max_changes, reqs->grow_factor);
	pool_limits_set (cache_cfg.ccrefs, reqs->min_changes * 2,
		 (reqs->max_changes == ~0U) ? ~0U : reqs->max_changes * 2,
		 reqs->grow_factor);
	pool_limits_set (cache_cfg.crefs, reqs->min_local_match,
				   reqs->max_local_match, reqs->grow_factor);
	pool_limits_set (cache_cfg.cwaits, reqs->min_cache_waiters,
				   reqs->max_cache_waiters, reqs->grow_factor);
	pool_limits_set (cache_cfg.cxfers, reqs->min_cache_transfers,
				   reqs->max_cache_transfers, reqs->grow_factor);
	pool_limits_set (cache_cfg.xflists, reqs->min_local_match,
				   reqs->max_local_match, reqs->grow_factor);
	pool_limits_set (cache_cfg.filters, reqs->min_time_filters,
				   reqs->max_time_filters, reqs->grow_factor);
	pool_limits_set (cache_cfg.finsts, reqs->min_time_instances,
				   reqs->max_time_instances, reqs->grow_factor);
	error = hc_pool_init (&cache_cfg);
	if (error)
		fatal_printf ("hc_pool_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "History cache pools created.\r\n");

	domain_cfg.ndomains = reqs->max_domains;
	pool_limits_set (domain_cfg.dparticipants, reqs->min_remote_participants,
				reqs->max_remote_participants, reqs->grow_factor);
	pool_limits_set (domain_cfg.types, reqs->min_topic_types,
					reqs->max_topic_types, reqs->grow_factor);
	pool_limits_set (domain_cfg.topics, reqs->min_topics, reqs->max_topics,
							reqs->grow_factor);
	pool_limits_set (domain_cfg.filter_topics, reqs->min_filtered_topics,
				reqs->max_filtered_topics, reqs->grow_factor);
	pool_limits_set (domain_cfg.publishers, reqs->min_publishers,
				      reqs->max_publishers, reqs->grow_factor);
	pool_limits_set (domain_cfg.subscribers, reqs->min_subscribers,
				      reqs->max_subscribers, reqs->grow_factor);
	pool_limits_set (domain_cfg.writers, reqs->min_local_writers,
				    reqs->max_local_writers, reqs->grow_factor);
	pool_limits_set (domain_cfg.readers, reqs->min_local_readers,
				    reqs->max_local_readers, reqs->grow_factor);
	pool_limits_set (domain_cfg.dwriters, reqs->min_remote_writers,
				   reqs->max_remote_writers, reqs->grow_factor);
	pool_limits_set (domain_cfg.dreaders, reqs->min_remote_readers,
				   reqs->max_remote_readers, reqs->grow_factor);
	pool_limits_set (domain_cfg.guards, reqs->min_guards, reqs->max_guards,
							     reqs->grow_factor);
	pool_limits_set (domain_cfg.prefixes, 8, reqs->max_remote_participants, 
							     reqs->grow_factor);
	error = domain_pool_init (&domain_cfg);
	if (error)
		fatal_printf ("domain_pool_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "Domain pools created.\r\n");

#ifdef RTPS_USED
	if (rtps_used) {
		pool_limits_set (rtps_cfg.readers, reqs->min_local_readers + 3,
				reqs->max_local_readers, reqs->grow_factor);
		pool_limits_set (rtps_cfg.writers, reqs->min_local_writers + 3,
				reqs->max_local_writers, reqs->grow_factor);
		pool_limits_set (rtps_cfg.rreaders, reqs->min_reader_proxies,
				reqs->max_reader_proxies, reqs->grow_factor);
		pool_limits_set (rtps_cfg.rwriters, reqs->min_writer_proxies,
				reqs->max_writer_proxies, reqs->grow_factor);
		pool_limits_set (rtps_cfg.ccrefs, reqs->min_changes, reqs->max_changes,
				reqs->grow_factor);
		pool_limits_set (rtps_cfg.messages, 32, ~0U, reqs->grow_factor);
		pool_limits_set (rtps_cfg.msgelements, 64, ~0U, reqs->grow_factor);
		pool_limits_set (rtps_cfg.msgrefs, 16, ~0U, reqs->grow_factor);
		error = rtps_init (&rtps_cfg);
		if (error)
			fatal_printf ("rtps_init() failed: error = %d", error);

		log_printf (DDS_ID, 0, "RTPS Initialised.\r\n");

		error = rtps_ipv4_attach (reqs->max_ip_sockets, reqs->max_ipv4_addresses);
		if (error)
			fatal_printf ("rtps_ipv4_attach() failed: error = %d", error);

		log_printf (DDS_ID, 0, "RTPS over IPv4 Initialised.\r\n");
#ifdef DDS_IPV6
		error = rtps_ipv6_attach (reqs->max_ip_sockets, reqs->max_ipv6_addresses);
		if (error)
			fatal_printf ("rtps_ipv6_attach() failed: error = %d", error);

		log_printf (DDS_ID, 0, "RTPS over IPv6 Initialised.\r\n");
#endif
	}
#endif

	pool_limits_set (qos_refs, reqs->min_local_readers + reqs->min_local_writers +
				   reqs->min_remote_readers + reqs->min_remote_writers +
				   reqs->min_topics, ~0U, reqs->grow_factor);
	pool_limits_set (qos_data, reqs->min_qos, reqs->max_qos, reqs->grow_factor);
	error = qos_pool_init (&qos_refs, &qos_data);
	if (error)
		fatal_printf ("qos_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "QoS pools initialized.\r\n");

	error = disc_init ();
	if (error)
		fatal_printf ("disc_init() failed: error = %d", error);

	log_printf (DDS_ID, 0, "Discovery initialized.\r\n");

	pool_limits_set (dcps_cfg.sampleinfos, reqs->min_application_samples,
				reqs->max_application_samples, reqs->grow_factor);
	pool_limits_set (dcps_cfg.waitsets, reqs->min_waitsets,
				reqs->max_waitsets, reqs->grow_factor);
	pool_limits_set (dcps_cfg.statusconds, reqs->min_statusconditions,
				reqs->max_statusconditions, reqs->grow_factor);
	pool_limits_set (dcps_cfg.readconds, reqs->min_readconditions,
				reqs->max_readconditions, reqs->grow_factor);
	pool_limits_set (dcps_cfg.queryconds, reqs->min_queryconditions,
				reqs->max_queryconditions, reqs->grow_factor);
	pool_limits_set (dcps_cfg.guardconds, reqs->min_guardconditions,
				reqs->max_guardconditions, reqs->grow_factor);
	pool_limits_set (dcps_cfg.topicwaits, reqs->min_topicwaits,
				reqs->max_topicwaits, reqs->grow_factor);
	error = dcps_init (&dcps_cfg);
	if (error)
		fatal_printf ("dcps_init() failed: error = %d", error);

#ifdef DDS_NATIVE_SECURITY
	pool_limits_set (sec_cfg.data_holders, reqs->min_data_holders,
				reqs->max_data_holders, reqs->grow_factor);
	pool_limits_set (sec_cfg.properties, reqs->min_properties,
				reqs->max_properties, reqs->grow_factor);
	pool_limits_set (sec_cfg.bin_properties, reqs->min_binary_properties,
				reqs->max_binary_properties, reqs->grow_factor);
	pool_limits_set (sec_cfg.sequences, reqs->min_holder_sequences,
				reqs->max_holder_sequences, reqs->grow_factor);
	error = sec_init (&sec_cfg);
	if (error)
		fatal_printf ("seq_init() failed: error = %d", error);
#endif
	pool_limits_set (notif_data, reqs->min_notifications,
				reqs->max_notifications, reqs->grow_factor);
	MDS_POOL_TYPE (mem_blocks, MB_NOTIFICATION, notif_data, sizeof (PendingNotify_t));
	pool_limits_set (def_ws_data, 1, ~0U, reqs->grow_factor);
	MDS_POOL_TYPE (mem_blocks, MB_WS_CHECK, def_ws_data, sizeof (PendingWSChk_t));
	MDS_POOL_TYPE (mem_blocks, MB_CFG_UPD, def_ws_data, sizeof (PendingCfgChg_t));
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size)
		fatal_printf ("Notification pool create failed!");
#endif
	log_printf (DDS_ID, 0, "DCPS Initialised.\r\n");

	PROF_INIT ("W:Events", dds_w_events);
	PROF_INIT ("W:Sleep", dds_w_sleep);
	PROF_INIT ("W:FD", dds_w_fd);
	PROF_INIT ("W:Timer", dds_w_tmr);
	PROF_INIT ("W:Cache", dds_w_cache);
	PROF_INIT ("W:Waitset", dds_w_waitset);
	PROF_INIT ("W:Listen", dds_w_listen);

	/* Initialize threading support. */
#ifdef THREADS_USED
	dds_init_threads ();
#endif

	tmr_init (&shm_timer, "SharedMemory");
	tmr_start (&shm_timer, TICKS_PER_SEC, 0, dds_assert_shm_liveness);

	return (DDS_RETCODE_OK);
}

/* dds_final -- Main DDS finalisation. */

void dds_final (void)
{
	tmr_stop (&shm_timer);

#ifdef THREADS_USED
	dds_finalize_threads ();
#endif
	mds_free (mem_blocks, MB_END);
	dcps_final ();
	disc_final ();
	qos_pool_free ();
#ifdef RTPS_USED
	if (rtps_used) {
		rtps_ipv4_detach ();
#ifdef DDS_IPV6
		rtps_ipv6_detach ();
#endif
		rtps_final ();
	}
#endif
	domain_pool_final ();
	hc_pool_free ();
	locator_pool_free ();
	guid_final ();
	db_pool_free ();
	tmr_pool_free ();
#ifdef DDS_NATIVE_SECURITY
	sec_final ();
#endif
}

#ifndef THREADS_USED

void DDS_schedule (unsigned ms)
{
	ctrc_printd (DDS_ID, DDS_SCHED, NULL, 0);
	if (!dds_listener_state)
		dds_work (ms);
}

void DDS_wait (unsigned ms)
{
	Ticks_t		d, now, end_time;	/* *10ms */

	if (dds_listener_state)
		return;

	ctrc_printd (DDS_ID, DDS_WAIT, NULL, 0);
	dds_continue = 0;
	now = sys_getticks ();
	end_time = now + ms / TMR_UNIT_MS;
	for (;;) {
		d = end_time - now;
		if (d >= 0x7fffffffUL)
			break;

		dds_work (d * TMR_UNIT_MS);

		if (dds_continue)
			break;

		now = sys_getticks ();
	}
}

void DDS_continue (void)
{
	ctrc_printd (DDS_ID, DDS_CONTINUE, NULL, 0);
	dds_continue = 1;
}

#else

void DDS_schedule (unsigned ms)
{
	ARG_NOT_USED (ms)
}

void DDS_wait (unsigned ms)
{
	usleep (ms * 1000);
}

void DDS_continue (void)
{
}

#endif

void DDS_Activities_suspend (DDS_Activity suspend)
{
	if ((suspend & DDS_TIMER_ACTIVITY) != 0) {
		tmr_suspend = 1;
		disc_suspend ();
	}
	if ((suspend & DDS_UDP_ACTIVITY) != 0)
		rtps_udp_suspend ();
#ifdef DDS_TCP
	if ((suspend & DDS_TCP_ACTIVITY) != 0)
		rtps_tcp_suspend ();
#endif
#ifdef DDS_DEBUG
	if ((suspend & DDS_DEBUG_ACTIVITY) != 0)
		debug_suspend ();
#endif
}

void DDS_Activities_resume (DDS_Activity resume)
{
	if ((resume & DDS_TIMER_ACTIVITY) != 0) {
		disc_resume ();
		tmr_suspend = 0;
	}
	if ((resume & DDS_UDP_ACTIVITY) != 0)
		rtps_udp_resume ();
#ifdef DDS_TCP
	if ((resume & DDS_TCP_ACTIVITY) != 0)
		rtps_tcp_resume ();
#endif
#ifdef DDS_DEBUG
	if ((resume & DDS_DEBUG_ACTIVITY) != 0)
		debug_resume ();
#endif
}

#ifdef DDS_DEBUG

/* dds_pool_dump -- Display some pool statistics. */

void dds_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

#endif
#ifdef RTPS_USED

/* DDS_Transport_parameters -- Set transport-specific parameters. */

int DDS_Transport_parameters (LocatorKind_t kind, void *pars)
{
	DDS_PRE_INIT ();
	return (rtps_parameters_set (kind, pars));
}

#endif

/* DDS_get_purge_delay -- Get the *_delete_contained_entities() delay in
			  microseconds. Default setting is 50us. */

unsigned DDS_get_purge_delay (void)
{
	DDS_PRE_INIT ();
	return (dds_purge_delay);
}

/* DDS_set_purge_delay -- Set the *_delete_contained_entities() delay in
			  microseconds. */

void DDS_set_purge_delay (unsigned us)
{
	dds_purge_delay = us;
	dds_purge_delay_fixed = 1;
}

/* DDS_configuration_set -- Set the configuration filename for the DDS
			    parameters. Needs to be done before calling any
			    of the other DDS functions. */

void DDS_configuration_set (const char *filename)
{
	if (filename)
		strcpy (cfg_filename, filename);
	else
		cfg_filename [0] = '\0';
}

/* DDS_parameter_set -- Update/set one of the DDS parameters. */

DDS_ReturnCode_t DDS_parameter_set (const char *name, const char *value)
{
	DDS_ReturnCode_t	ret;

	ret = config_parameter_set (name, value);
	return (ret);
}

/* DDS_parameter_unset -- Unset one of the DDS parameters. */

DDS_ReturnCode_t DDS_parameter_unset (const char *name)
{
	DDS_ReturnCode_t	ret;

	ret = config_parameter_unset (name);
	return (ret);
}


/* DDS_parameter_get -- Retrieve one of the DDS parameters. */

const char *DDS_parameter_get (const char *name, char buffer [], size_t size)
{
	return (config_parameter_get (name, buffer, size));
}

#define UTMR_MAGIC	0x234123ff	/* User timer magic word. */

typedef struct DDS_Timer_st {
	unsigned	magic;
	Timer_t		timer;
} UTimer_t;

/* DDS_Timer_create -- Create a new user timer. */

DDS_Timer DDS_Timer_create (const char *name)
{
	UTimer_t	*t;

	DDS_PRE_INIT ();
	t = xmalloc (sizeof (UTimer_t));
	if (!t)
		return (NULL);

	t->magic = UTMR_MAGIC;
	tmr_init (&t->timer, strdup (name));
	return (t);
}

DDS_ReturnCode_t DDS_Timer_start (DDS_Timer t,
				  unsigned ms,
				  uintptr_t user,
				  void (*fct) (uintptr_t))
{
	unsigned	ticks;

	if (!t || t->magic != UTMR_MAGIC || !ms)
		return (DDS_RETCODE_BAD_PARAMETER);

	ticks = ms / TMR_UNIT_MS;
	if (!ticks)
		ticks = 1;
	tmr_start (&t->timer, ticks, user, fct);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Timer_stop (DDS_Timer t)
{
	if (!t || t->magic != UTMR_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	tmr_stop (&t->timer);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Timer_delete (DDS_Timer t)
{
	if (!t || t->magic != UTMR_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	tmr_stop (&t->timer);
	t->magic = 0;
	free ((char *) t->timer.name);
	xfree (t);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_Handle_attach (HANDLE h,
				    short poll_ev,
				    void (*fct) (HANDLE h,
						 short poll_ev,
						 void *udata),
				    void *udata)
{
	DDS_PRE_INIT ();
	if (!poll_ev || !fct)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (sock_fd_add_handle (h, poll_ev, fct, udata, "USER"));
}

void DDS_Handle_detach (HANDLE h)
{
	sock_fd_remove_handle (h);
}

void DDS_Log_stdio (int enable)
{
	DDS_PRE_INIT ();
	if (enable)
		err_actions_add (EL_LOG, ACT_PRINT_STDIO);
	else
		err_actions_remove (EL_LOG, ACT_PRINT_STDIO);
}

void DDS_Log_strings (const char *fcts [])
{
	log_fct_str [USER_ID] = fcts;
}

DDS_ReturnCode_t DDS_Debug_start (void)
{
	DDS_PRE_INIT ();
#ifdef DDS_DEBUG
	debug_start ();
	return (DDS_RETCODE_OK);
#else
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_start_fct (HANDLE inh,
			     void (*fct) (HANDLE fd, short events, void *udata))
{
#ifdef DDS_DEBUG
	debug_start_fct (inh, fct);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (inh)
	ARG_NOT_USED (fct)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_input (HANDLE fd, short events, void *udata)
{
#ifdef DDS_DEBUG
	debug_input (fd, events, udata);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (fd)
	ARG_NOT_USED (events)
	ARG_NOT_USED (udata)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_server_start (unsigned       nclients,
					 unsigned short port)
{
	DDS_PRE_INIT ();
#ifdef DDS_DEBUG
	if (port == DDS_DEBUG_PORT_DEFAULT)
		port = DDS_DEBUG_PORT_OFS + dds_participant_id;

	return (debug_server_start (nclients, port));
#else
	ARG_NOT_USED (nclients)
	ARG_NOT_USED (port)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_abort_enable (int *abort_program)
{
#ifdef DDS_DEBUG
	debug_abort_enable (abort_program);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (abort_program)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_control_enable (int *pause_cmd,
					   unsigned *nsteps,
					   unsigned *delay)
{
#ifdef DDS_DEBUG
	debug_control_enable (pause_cmd, nsteps, delay);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (pause_cmd)
	ARG_NOT_USED (nsteps)
	ARG_NOT_USED (delay)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_menu_enable (int *menu)
{
#ifdef DDS_DEBUG
	debug_menu_enable (menu);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (menu)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Debug_command (const char *buf)
{
#ifdef DDS_DEBUG
	debug_command (buf);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (buf)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

void DDS_Debug_help (void)
{
#ifdef DDS_DEBUG
	debug_help ();
#endif
}

void DDS_Debug_cache_dump (void *ep)
{
#ifdef DDS_DEBUG
	debug_cache_dump (ep);
#else
	ARG_NOT_USED (ep)
#endif
}

void DDS_Debug_proxy_dump (void *ep)
{
#ifdef DDS_DEBUG
	debug_proxy_dump (ep);
#else
	ARG_NOT_USED (ep)
#endif
}

void DDS_Debug_topic_dump (unsigned domain, const char *name)
{
#ifdef DDS_DEBUG
	debug_topic_dump (domain, name);
#else
	ARG_NOT_USED (domain)
	ARG_NOT_USED (name)
#endif
}

void DDS_Debug_pool_dump (int wide)
{
#ifdef DDS_DEBUG
	debug_pool_dump (wide);
#else
	ARG_NOT_USED (wide)
#endif
}

void DDS_Debug_disc_dump (void)
{
#ifdef DDS_DEBUG
	debug_disc_dump ();
#endif
}

void DDS_Debug_type_dump (unsigned scope, const char *name)
{
#if defined (DDS_DEBUG) && defined (XTYPES_USED)
	debug_type_dump (scope, name);
#else
	ARG_NOT_USED (scope)
	ARG_NOT_USED (name)
#endif
}

void DDS_Debug_dump_static (unsigned indent,
			    DDS_TypeSupport ts,
			    void *data,
			    int key_only,
			    int secure,
			    int field_names)
{
#if defined (DDS_DEBUG) && defined (XTYPES_USED)
	if (!ts || !data)
		return;

	if (key_only)
		DDS_TypeSupport_dump_key (indent, ts, data, 1, 0, secure, field_names);
	else
		DDS_TypeSupport_dump_data (indent, ts, data, 1, 0, field_names);
#else
	ARG_NOT_USED (indent)
	ARG_NOT_USED (ts)
	ARG_NOT_USED (data)
	ARG_NOT_USED (key_only)
	ARG_NOT_USED (secure)
	ARG_NOT_USED (field_names)
#endif
}

void DDS_Debug_dump_dynamic (unsigned indent,
			     DDS_DynamicTypeSupport ts,
			     DDS_DynamicData data,
			     int key_only,
			     int secure,
			     int field_names)
{
#if defined (DDS_DEBUG) && defined (XTYPES_USED)
	DynDataRef_t	*ddr = (DynDataRef_t *) data;

	if (!ts || !data)
		return;

	if (key_only)
		DDS_TypeSupport_dump_key (indent, (TypeSupport_t *) ts, ddr->ddata, 1, 1, secure, field_names);
	else
		DDS_TypeSupport_dump_data (indent, (TypeSupport_t *) ts, ddr->ddata, 1, 1, field_names);
#else
	ARG_NOT_USED (indent)
	ARG_NOT_USED (ts)
	ARG_NOT_USED (data)
	ARG_NOT_USED (key_only)
	ARG_NOT_USED (secure)
	ARG_NOT_USED (field_names)
#endif
}


DDS_ReturnCode_t DDS_Trace_set (DDS_Entity entity, unsigned mode)
{
	Entity_t	*ep = (Entity_t *) entity;

	if (!ep ||
	    !entity_local (ep->flags) ||
	    (entity_type (ep) != ET_WRITER &&
	     entity_type (ep) != ET_READER))
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef RTPS_TRACE
	return (rtps_trace_set ((Endpoint_t *) ep, mode));
#else
	ARG_NOT_USED (entity)
	ARG_NOT_USED (mode)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

DDS_ReturnCode_t DDS_Trace_get (DDS_Entity entity, unsigned *mode)
{
	Entity_t	*ep = (Entity_t *) entity;

	if (!ep ||
	    !entity_local (ep->flags) ||
	    (entity_type (ep) != ET_WRITER &&
	     entity_type (ep) != ET_READER))
		return (DDS_RETCODE_BAD_PARAMETER);

#ifdef RTPS_TRACE
	return (rtps_trace_get ((Endpoint_t *) ep, mode));
#else
	ARG_NOT_USED (entity)
	ARG_NOT_USED (mode)
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

void DDS_Trace_defaults_set (unsigned mode)
{
#ifdef RTPS_TRACE
	DDS_PRE_INIT ();
	if (mode != DDS_TRACE_MODE_TOGGLE)
		dds_dtrace = mode;
	else if (dds_dtrace)
		dds_dtrace = 0;
	else
		dds_dtrace = DDS_TRACE_ALL;
#else
	ARG_NOT_USED (mode)
#endif
}

void DDS_Trace_defaults_get (unsigned *mode)
{
#ifdef RTPS_TRACE
	DDS_PRE_INIT ();
	if (mode)
		*mode = dds_dtrace;
#else
	ARG_NOT_USED (mode)
#endif
}


DDS_ReturnCode_t DDS_CTrace_start (void)
{
#ifdef CTRACE_USED
	ctrc_start ();
	return (DDS_RETCODE_OK);
#else
	return (DDS_RETCODE_UNSUPPORTED);
#endif
}


void DDS_CTrace_stop (void)
{
#ifdef CTRACE_USED
	ctrc_start ();
#endif
}

void DDS_CTrace_clear (void)
{
#ifdef CTRACE_USED
	ctrc_clear ();
#endif
}

DDS_ReturnCode_t DDS_CTrace_mode (int cyclic)
{
#ifdef CTRACE_USED
	ctrc_mode (cyclic);
	return (DDS_RETCODE_OK);
#else
	ARG_NOT_USED (cyclic)

	return (DDS_RETCODE_UNSUPPORTED);
#endif
}

void DDS_CTrace_printd (unsigned index, const void *data, size_t length)
{
#ifdef CTRACE_USED
	ctrc_printd (USER_ID, index, data, length);
#else
	ARG_NOT_USED (index)
	ARG_NOT_USED (data)
	ARG_NOT_USED (length)
#endif
}

void DDS_CTrace_printf (unsigned index, const char *fmt, ...)
{
#ifdef CTRACE_USED
	va_list	arg;
	char	buf [128];

	va_start (arg, fmt);
	vsprintf (buf, fmt, arg);
	va_end (arg);
	ctrc_printd (USER_ID, index, buf, strlen (buf));
#else
	ARG_NOT_USED (index)
	ARG_NOT_USED (fmt)
#endif
}

void DDS_CTrace_begind (unsigned index, const void *data, size_t length)
{
#ifdef CTRACE_USED
	ctrc_begind (USER_ID, index, data, length);
#else
	ARG_NOT_USED (index)
	ARG_NOT_USED (data)
	ARG_NOT_USED (length)
#endif
}

void DDS_CTrace_contd (const void *data, size_t length)
{
#ifdef CTRACE_USED
	ctrc_contd (data, length);
#else
	ARG_NOT_USED (data)
	ARG_NOT_USED (length)
#endif
}

void DDS_CTrace_endd (void)
{
#ifdef CTRACE_USED
	ctrc_endd ();
#endif
}
