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

/* timer.c -- Implements a minimal-overhead timer handler. */

#include <stdio.h>
#include "sys.h"
#include "error.h"
#include "log.h"
#include "ctrace.h"
#include "debug.h"
#include "timer.h"

#define	MAX_TIME_DIFF	0x7fffffff	/* Max. time delta until delta is 0. */

static MEM_DESC_ST 	tmr_pool;	/* Pool of timers. */
static size_t		mem_size;	/* Total memory allocated. */

static const char *pool_names [1] = { "TIMER" };

static Timer_t 	*tmr_list;		/* Current timer list. */
static int	tmr_callback_active;	/* Active timer callback. */
static Timer_t 	*active_timer;		/* Currently active timer. */
static Timer_t	*phead, *ptail;		/* Elapsed but locked timers list. */
static lock_t	tmr_lock;		/* Timer lock. */
static lock_t	*lockp;			/* Active timer lock. */
static Ticks_t	tmr_ticks_init;		/* # of ticks at start. */
static unsigned	tmr_nactive;		/* # of active timers. */
static unsigned	tmr_ntimeouts;		/* # of timer time-outs. */
static unsigned	tmr_nbusy;		/* # of time-outs with busy lock. */
static unsigned	tmr_nstarts;		/* # of timer starts. */
static unsigned	tmr_nstops;		/* # of timer stops. */

/* tmr_pool_init -- Initialize a timer pool. */

int tmr_pool_init (const POOL_LIMITS *timers)
{
	static int	lock_ready = 0;

	/* Check if already initialized. */
	if (tmr_pool.md_addr) {	/* Was already initialized -- reset. */
		mds_reset (&tmr_pool, 1);
		tmr_list = NULL;
		return (TMR_OK);
	}
	if (!lock_ready) {
		lock_init_nr (tmr_lock, "tmr_lock");
		lock_ready = 1;
	}
	MDS_POOL_TYPE ((&tmr_pool), 0, *timers, sizeof (Timer_t));
	mem_size = mds_alloc (&tmr_pool, pool_names, 1);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		warn_printf ("rtps_init: not enough memory available!\r\n");
		return (TMR_ERR_NOMEM);
	}
	log_printf (TMR_ID, 0, "tmr_pool_init: %lu bytes allocated for pool.\r\n", (unsigned long) mem_size);
#endif
	tmr_ticks_init = sys_getticks ();
	return (TMR_OK);
}

/* tmr_pool_free -- Free the timer pool. */

void tmr_pool_free (void)
{
	tmr_list = NULL;
	mds_free (&tmr_pool, 1);
}

/* tmr_start_lock -- Start a timer that will elapse after the specified number
		     of ticks.  The function 'fct' will be called at time-out
		     with 'user' as parameter.  If the timer was already
		     running, it is restarted.  The lp argument will be used
		     (if non-NULL) to automatically take a lock before calling
		     the callback function, and automatically release the lock
		     afterwards. */

void tmr_start_lock (Timer_t   *t,
		     Ticks_t   ticks,
		     uintptr_t user,
		     TMR_FCT   fct,
		     lock_t    *lp)
{
	Ticks_t		now = sys_getticks ();
	Timer_t		*p, **pprev;

	if (t->tcbf)
		tmr_stop (t);

	lock_take (tmr_lock);
	tmr_nstarts++;
	tmr_nactive++;
	t->time = now + ticks;
	t->user = user;
	t->tcbf = fct;
	t->lockp = lp;
	for (pprev = &tmr_list, p = *pprev;
	     p && (sys_ticksdiff (now, p->time) > MAX_TIME_DIFF ||
	           sys_ticksdiff (now, t->time) > sys_ticksdiff (now, p->time));
	     pprev = &p->next, p = *pprev)
		;

	t->next = *pprev;
	*pprev = t;
	lock_release (tmr_lock);
}

/* tmr_stop -- Stop a previously started timer. */

void tmr_stop (Timer_t *t)
{
	Timer_t		*p, *prev, **pprev;

	if (!t)
		return;

	lock_take (tmr_lock);
	if (!t->tcbf)
		goto done;

	tmr_nstops++;
	t->tcbf = NULL;
	for (pprev = &tmr_list, p = *pprev;
	     p && t != p;
	     pprev = &p->next, p = *pprev)
		;

	if (!p && !phead)
		goto done;

	tmr_nactive--;
	if (t != p && phead) {
		for (prev = NULL, p = phead;
		     p && t != p;
		     prev = p, p = p->next)
			;

		if (!p)
			goto done;

		if (!p->next)
			ptail = prev;
		if (prev)
			pprev = &prev->next;
		else
			pprev = &phead;
	}
	*pprev = p->next;

    done:
	lock_release (tmr_lock);
}

/* tmr_lock_enable -- Enable automatic take/release of the given lock on calling
		      the timer callback function.  Note that locking will
		      always be symmetrical, so a currently executing callback
		      will not be affected. */

void tmr_lock_enable (Timer_t *t, lock_t *lp)
{
	lock_take (tmr_lock);
	if (t && t->tcbf)
		t->lockp = lp;
	lock_release (tmr_lock);
}

/* tmr_lock_disable -- Disable locking/unlocking of a timer. */

void tmr_lock_disable (Timer_t *t)
{
	lock_take (tmr_lock);
	if (t)
		t->lockp = NULL;
	else
		lockp = NULL;
	lock_release (tmr_lock);
}

/* tmr_remain -- Returns the number of ticks before time-out or MAX(unsigned) if
		 not started. */

Ticks_t tmr_remain (const Timer_t *t)
{
	Ticks_t	d, now = sys_getticks ();

	lock_take (tmr_lock);
	if (!t->tcbf) {
		d = MAX_TIME_DIFF;
		goto done;
	}
	d = sys_ticksdiff (now, t->time);
	if (d >= MAX_TIME_DIFF)
		d = 0;

    done:
	lock_release (tmr_lock);
	return (d);
}

/* tmr_manage -- Should be used to manage the timer lists. */

void tmr_manage (void)
{
	Ticks_t	now, d;
        uintptr_t user;
	Timer_t	*t;
	TMR_FCT	fct;

	lock_take (tmr_lock);
	if (tmr_callback_active) {
                lock_release (tmr_lock);
		return;
        }
	while (tmr_list) {
		now = sys_getticks ();
		/*ctrc_printf (TMR_ID, 0, "Now:%lu, tmr_list->time:%lu\r\n", now, tmr_list->time);*/
		d = sys_ticksdiff (now, tmr_list->time);
		if (d && d < MAX_TIME_DIFF)
			break;

		/* Timer elapsed! */
		t = tmr_list;
		tmr_list = t->next;
		fct = t->tcbf;
		if (!fct) {
#ifdef TRC_TIMER
			err_printf ("Timer (%s) callback function is NULL!", t->name);
#endif
			continue;
		}
		lockp = t->lockp;
                user = t->user;
                active_timer = t;
		t->tcbf = NULL;
		tmr_ntimeouts++;
		tmr_nactive--;
		lock_release (tmr_lock);
		if (lockp && !lock_try (*lockp)) {
			/*dbg_printf ("Locked timer is busy!\r\n");*/
			lock_take (tmr_lock);
			tmr_nbusy++;
			tmr_nactive++;
			if (!active_timer)	/* Just stopped! */
				continue;

	                active_timer = NULL;
			t->tcbf = fct;
			if (phead)
				ptail->next = t;
			else
				phead = t;
			t->next = NULL;
			ptail = t;
			continue;
		}
		tmr_callback_active++;
#ifdef CTRACE_USED
		ctrc_printf (TMR_ID, 0, "%s", t->name);
#endif
		(*fct) (user);
		lock_take (tmr_lock);
		if (lockp)
			lock_release (*lockp);
		tmr_callback_active--;
                active_timer = NULL;
	}
	if (phead) {	/* Lock collisions occurred! */
		ptail->next = tmr_list;
		tmr_list = phead;
		phead = NULL;
		lock_release (tmr_lock);
		thread_yield ();
	}
	else
		lock_release (tmr_lock);
}

# if 0
/* tmr_manage_delta -- Manage a timer list with the maximum timer delta. */

static Ticks_t tmr_manage_delta (Ticks_t d)
{
	Timer_t		*t;
	TMR_FCT		fct;
	Ticks_t		now, max;
	unsigned	d_max;

	lock_take (tmr_lock);
 	now = sys_getticks ();
	max = now + d;
	for (;;) {
		d = sys_ticksdiff (now, tmr_list->time);
		if (d && d < MAX_TIME_DIFF) {
			d_max = sys_ticksdiff (now, max);
			if (d > d_max)
				d = d_max;
			break;
		}

		/* Timer elapsed! */
		/*ctrc_printf (TMR_ID, 0, "Now:%lu, tmr_list->time:%lu\r\n", now, tmr_list->time);*/
		t = tmr_list;
		tmr_list = t->next;
		fct = t->tcbf;
		if (fct) {
			t->tcbf = NULL;
			tmr_callback_active++;
			lock_release (tmr_lock);
			(*fct) (t->user);
			lock_take (tmr_lock);
			tmr_callback_active--;
		}
#ifdef TRC_TIMER
		else
			err_printf ("Timer (%s) callback function is NULL!", t->name);
#endif
	 	now = sys_getticks ();
		if (!tmr_list) {
			d = sys_ticksdiff (now, max);
			break;
		}
	}
	lock_release (tmr_lock);
	return (d);
}

/* tmr_manage_poll -- Manage the timers and return a # of milliseconds to sleep
		      before the timer list needs to be managed again. This
		      number can then be used as the timeout argument of a poll()
		      function call. */

unsigned tmr_manage_poll (unsigned ms)
{
	if (!tmr_list)
		return (ms);

	return (tmr_manage_delta (ms / TMR_UNIT_MS) * TMR_UNIT_MS);
}

#ifndef _WIN32

/* tmr_manage_select -- Same as tmr_sleep_poll(), but this one should be used in
		        a select() function call. */

struct timeval *tmr_manage_select (struct timeval *tv)
{
	Ticks_t	delta;

	if (tmr_callback_active || !tmr_list)
		return (tv);

	delta = tmr_manage_delta (tv->tv_sec * 100 + tv->tv_usec / 10000);
	tv->tv_sec = delta / 100;
	tv->tv_usec = (delta - tv->tv_sec * 100) * 10000;
	return (tv);
}

#endif
# endif

unsigned tmr_pending_ms (void)
{
	Ticks_t		now, d;

	lock_take (tmr_lock);
	if (tmr_list) {
		now = sys_getticks ();
		d = sys_ticksdiff (now, tmr_list->time);
		if (d > MAX_TIME_DIFF) /* Already past time-out! */
			d = 0;
		else
			d *= TMR_UNIT_MS;
	}
	else
		d = MAX_TIME_DIFF;
	lock_release (tmr_lock);
	return (d);
}

/* tmr_alloc -- Allocate a new timer. */

Timer_t *tmr_alloc (void)
{
	Timer_t		*tp;

	tp = mds_pool_alloc (&tmr_pool);
	if (!tp)
		return (NULL);

	tp->tcbf = NULL;
	tp->name = NULL;
	return (tp);
}

/* tmr_free -- Free a timer. */

void tmr_free (Timer_t *t)
{
	mds_pool_free (&tmr_pool, t);
}


#ifdef DDS_DEBUG

/* tmr_dump -- Debug: dumps the active timer list. */

void tmr_dump (void)
{
	Timer_t	*p;
	Ticks_t	delta, now = sys_getticks ();

	lock_take (tmr_lock);
	delta = now - tmr_ticks_init;
	dbg_printf ("DDS running for %lu.%lus\r\n", delta / TICKS_PER_SEC,
						  delta % TICKS_PER_SEC);
	dbg_printf ("# of timers started/stopped/active = %u/%u/%u\r\n",
			tmr_nstarts, tmr_nstops, tmr_nactive);
	dbg_printf ("# of timeouts = %u, busy-locks = %u\r\n",
			tmr_ntimeouts, tmr_nbusy);
	lock_release (tmr_lock);
	/*lock_take (tmr_lock);*/
	dbg_printf ("Now = %lu, Timers: ", now);
	if (!tmr_list)
		dbg_printf ("none\r\n");
	else {
		for (p = tmr_list; p; p = p->next) {
			dbg_printf ("\r\n\t%p: time = %lu (", (void *) p, p->time);
			dbg_print_timer (p);
			dbg_printf ("), user = %lu, tcbf = 0x%lx", (unsigned long) p->user, (unsigned long) (uintptr_t) p->tcbf);
			dbg_printf (", name = \'%s\'", p->name);
		}
		dbg_printf ("\r\n");
	}
	/*lock_release (tmr_lock);*/
}

/* tmr_pool_dump -- Display some pool statistics. */

void tmr_pool_dump (size_t sizes [])
{
	print_pool_table (&tmr_pool, 1, sizes);
}

#endif /* !DDS_DEBUG */

