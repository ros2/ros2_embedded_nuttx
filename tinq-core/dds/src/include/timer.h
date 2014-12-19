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

/* timer.h -- Minimal overhead timer handler. */

#ifndef __timer_h_
#define __timer_h_

#include <timer.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include "sys.h"
#include "thread.h"
#include "pool.h"
#include "dds/dds_error.h"

#define	TMR_OK		DDS_RETCODE_OK
#define	TMR_ERR_NOMEM	DDS_RETCODE_OUT_OF_RESOURCES

typedef void (*TMR_FCT) (uintptr_t user); /* Timer callback function. */

typedef struct timer_st Timer_t;
struct timer_st {
	Timer_t		*next;	/* Next timer in list. */
	Ticks_t		time;	/* Ticks before time-out. */
	uintptr_t	user;	/* User parameter. */
	TMR_FCT		tcbf;	/* Callback function. */
	lock_t		*lockp;	/* Lock to take if non-NULL. */
	const char	*name;	/* Timer name. */
};


int tmr_pool_init (const POOL_LIMITS *timers);

/* Initialize a timer pool. */

void tmr_pool_free (void);

/* Free the timer pool. */

#define	tmr_init(t, n)	do { (t)->tcbf = NULL; (t)->name = n; } while (0)

/* Initialize a timer. */

#define tmr_start(t,ticks,user,fct)	tmr_start_lock(t,ticks,user,fct,NULL)

/* Start a timer that will elapse after the specified number of ticks.
   The function 'fct' will be called at time-out with 'user' as parameter.
   The timer must reside in the context of the caller.  If the timer was already
   running, it is restarted. */

void tmr_start_lock (Timer_t   *t,
		     Ticks_t   ticks,
		     uintptr_t user,
		     TMR_FCT   fct,
		     lock_t    *lp);

/* Same as tmr_start(), but automatically tries to take the given lock before
   calling the callback function.  If the lock is busy, i.e. it was already
   taken, the timer callback will become pending and will be retried at a later
   stage, if not removed before with tmr_stop(). */

void tmr_stop (Timer_t *t);

/* Stop a previously started timer.  If the timer is currently in its callback
   function, an implicit tmr_disable_lock(NULL) is performed. */

#define	tmr_active(t)	((t)->tcbf != NULL)

/* Returns a non-0 result if the timer is active. */

Ticks_t tmr_remain (const Timer_t *t);

/* Returns the number of ticks before time-out or MAX(unsigned) if inactive. */

void tmr_lock_enable (Timer_t *t, lock_t *lp);

/* Enable automatic take/release of the given lock on calling the timer
   callback function.  Note that locking will always be symmetrical, so
   a currently executing callback will not be affected. */

void tmr_lock_disable (Timer_t *t);

/* Disable automatic take/release of a timer lock. This function can also be
   used to disable the lock_release() of the timer lock in the currently active
   timer callback (if the timer parameter is NULL). */

unsigned tmr_manage_poll (unsigned ms);

/* Manage the timers and return a # of milliseconds to sleep before the timer
   list needs to be managed again.  This number can then be used as the timeout
   argument of a poll() function call. */

#ifndef _WIN32
struct timeval *tmr_manage_select (struct timeval *tv);
#endif
/* Same as tmr_manage_poll(), but should be used in a select() function call. */

void tmr_manage (void);

/* Can be used for explicit timer management. */

unsigned tmr_pending_ms (void);

/* Can be used to check if timers need scheduling.  If any timers have elapsed,
   0 will be returned, after which tmr_manage() should be called.
   Otherwise the # of milliseconds before the first timer will becoming active
   is returned. */

void tmr_dump (void);

/* Debug: dumps the active timer list. */

Timer_t *tmr_alloc (void);

/* Allocate a new timer. */

void tmr_free (Timer_t *t);

/* Free a timer. */

void tmr_pool_dump (size_t sizes []);

/* Display pool data and statistics. */

#endif /* !timer.h */

