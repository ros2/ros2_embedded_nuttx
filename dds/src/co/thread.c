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

/* thread.c -- Threads and locking support code. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/dds_error.h"
#include "log.h"
#include "error.h"
#include "prof.h"
#include "ctrace.h"
#include "pool.h"
#include "thread.h"

#ifdef LOCK_TRACE

/*#define LOG_LOCKS		** Define this for detailed lock logging. */
/*#define PROF_TRC_LOCKS	** Put lock actions in cyclic trace buffer
				   (profiling enabled). */
#ifdef CTRACE_USED
#define	CTRC_LOCKS		/* Put lock actions in cyclic trace buffer
				   (no profiling enabled). */
#endif

#define	MAX_LOCK_HASH	32

typedef enum {
	LS_NEW,
	LS_INIT,
	LS_TAKEN
} LSTATE;

typedef struct lock_state_st LockState_t;
struct lock_state_st {
	LockState_t	*next;
	pthread_mutex_t	*lock;
	LSTATE		state;
	const char	*file;
	unsigned	line;
	unsigned	nwaiters;
	unsigned	ntakers;
#ifdef PROFILE
	unsigned	pid;
#endif
	char		name [32];
	const char	*wname [2];
	unsigned	wline [2];
};

#ifdef CTRC_LOCKS

enum {
	LOCK_INIT,
	LOCK_TRY,
	LOCK_TAKE,
	LOCK_RELEASE,
	LOCK_DESTROY
};

static const char *thread_fct_str [] = {
	"LOCK_INIT", "LOCK_TRY", "LOCK_TAKE", "LOCK_RELEASE", "LOCK_DESTROY"
};

#endif

static LockState_t	*locks [MAX_LOCK_HASH];
static lock_t		trc_lock = PTHREAD_MUTEX_INITIALIZER;

#define	lock_hash(l)	((uintptr_t) l & (MAX_LOCK_HASH - 1))

#ifdef LOG_LOCKS
#define	ltrc_print1(s,a)	printf(/*THREAD_ID, 0, */s, a)
#else
#define	ltrc_print1(s,a)
#endif

static LockState_t *lock_lookup (pthread_mutex_t *l)
{
	LockState_t	*lp;

	for (lp = locks [lock_hash (l)]; lp; lp = lp->next)
		if (lp->lock == l)
			return (lp);

	return (NULL);
}

int trc_lock_init (pthread_mutex_t *l,
		   int             recursive,
		   const char      *name,
		   const char      *file,
		   int             line)
{
	LockState_t	*lp;
	unsigned	h;
	int		res;
#ifdef CTRACE_USED
	static int	ctrc_initialized = 0;

	if (!ctrc_initialized) {

		/* Initialize the cyclic trace. */
		ctrc_initialized = 1;
		log_fct_str [THREAD_ID] = thread_fct_str;
	}
#endif
	lock_takef (trc_lock);
	lp = lock_lookup (l);
	if (lp)
		warn_printf ("lock_init(%s) on existing lock (%s:%d)", name,
								file, line);
	else {
		lp = (LockState_t *) mm_fcts.alloc_ (sizeof (LockState_t));
		if (lp) {
			lp->lock = l;
			strncpy (lp->name, name, sizeof (lp->name) - 1);
			lp->name [sizeof (lp->name) - 1] = '\0';
			h = lock_hash (l);
			lp->next = locks [h];
			locks [h] = lp;
		}
	}
	if (lp) {
		lp->state = LS_NEW;
		lp->file = file;
		lp->line = line;
		lp->nwaiters = lp->ntakers = 0;
	}
#ifdef CTRC_LOCKS
	if (ctrace_used) {
		ctrc_begind (THREAD_ID, LOCK_INIT, &l, sizeof (l));
		ctrc_contd (&recursive, sizeof (recursive));
		if (name)
			ctrc_contd (name, strlen (name) + 1);
		ctrc_contd (file, strlen (file) + 1);
		ctrc_contd (&line, sizeof (line));
		ctrc_endd ();
	}
#endif
	lock_releasef (trc_lock);
	if (recursive)
		res = pthread_mutex_init (l, &recursive_mutex);
	else
		res = pthread_mutex_init (l, NULL);
	if (res) {
		warn_printf ("trc_lock_init: pthread_mutex_init(%s) returned error: %s", (lp) ? lp->name : NULL, strerror (res));
		return (res);
	}
	if (lp) {
		lp->state = LS_INIT;
#ifdef PROFILE
		prof_alloc (lp->name, &lp->pid);
#ifdef PROF_TRC_LOCKS
		prof_trace (lp->name, 1);
#endif
#endif
	}
	return (0);
}

int trc_lock_try (pthread_mutex_t *l, const char *file, int line)
{
	LockState_t	*lp;
	int		res;

	lock_takef (trc_lock);
	lp = lock_lookup (l);
#ifdef LOG_LOCKS
	if (lp)
		ltrc_print1 ("{TRY(%s)", lp->name);
	else
		ltrc_print1 ("{TRY(%p)", (void *) l);
#endif
	lock_releasef (trc_lock);
	res = pthread_mutex_trylock (l);
	lock_takef (trc_lock);
#ifdef CTRC_LOCKS
	if (ctrace_used) {
		ctrc_begind (THREAD_ID, LOCK_TRY, &l, sizeof (l));
		if (lp && lp->name)
			ctrc_contd (lp->name, strlen (lp->name) + 1);
		ctrc_contd (file, strlen (file) + 1);
		ctrc_contd (&line, sizeof (line));
		ctrc_contd (&res, sizeof (res));
		ctrc_endd ();
	}
#endif
	if (res && res != EBUSY)
		warn_printf ("trc_lock_try: pthread_mutex_trylock(%s) returned error: %s",
			(lp) ? lp->name : NULL, strerror (res));
	if (lp && !res) {
		lp->state = LS_TAKEN;
		lp->file = file;
		lp->line = line;
		lp->ntakers++;
		ltrc_print1 ("{GOT(%s)}", lp->name);
	}
	else if (res && res != EBUSY)
		warn_printf ("trc_lock_try: pthread_mutex_trylock(%s) returned error: %s",
			(lp) ? lp->name : NULL, strerror (res));
	lock_releasef (trc_lock);
	ltrc_print1 (":%d}", res);
	return (res != EBUSY);
}

int trc_lock_take (pthread_mutex_t *l, const char *file, int line)
{
	LockState_t	*lp;
#ifdef PROFILE
	int		r;
#endif
	int		res;

	lock_takef (trc_lock);
	lp = lock_lookup (l);
#ifdef LOG_LOCKS
	if (lp)
		ltrc_print1 ("{TAKE(%s)}", lp->name);
	else
		ltrc_print1 ("{TAKE(%p)}", (void *) l);
#endif
	if (lp) {
		if (lp->nwaiters < 2) {
			lp->wname [lp->nwaiters] = file;
			lp->wline [lp->nwaiters] = line;
		}
		lp->nwaiters++;
	}
#ifdef PROFILE
	if (lp)
		r = prof_start (lp->pid);
#endif
#ifdef CTRC_LOCKS
	if (ctrace_used) {
		ctrc_begind (THREAD_ID, LOCK_TAKE, &l, sizeof (l));
		if (lp && lp->name)
			ctrc_contd (lp->name, strlen (lp->name) + 1);
		ctrc_contd (file, strlen (file) + 1);
		ctrc_contd (&line, sizeof (line));
		ctrc_endd ();
	}
#endif
	lock_releasef (trc_lock);
	res = lock_takef (*l);
#ifdef PROFILE
	if (lp && r == PROF_OK)
		prof_stop (lp->pid, 1);
#endif
	if (res) {
		warn_printf ("trc_lock_take: pthread_mutex_lock(%s) returned error: %s",
			(lp) ? lp->name : NULL, strerror (res));
		return (res);
	}
	lock_takef (trc_lock);
	if (lp) {
		lp->state = LS_TAKEN;
		lp->file = file;
		lp->line = line;
		lp->ntakers++;
		lp->nwaiters--;
		ltrc_print1 ("{GOT(%s)}", lp->name);
	}
#ifdef LOG_LOCKS
	else
		ltrc_print1 ("{GOT(%p)}", (void *) l);
#endif
	lock_releasef (trc_lock);
	return (0);
}

int trc_lock_release (pthread_mutex_t *l, const char *file, int line)
{
	LockState_t	*lp;
	int		res;

	lock_takef (trc_lock);
	lp = lock_lookup (l);
	if (lp) {
		if (lp->state != LS_TAKEN)
			warn_printf ("lock_release(%s) on free lock (%s:%d)",
							lp->name, file, line);
		ltrc_print1 ("{FREE(%s)", lp->name);
	}
#ifdef LOG_LOCKS
	else
		ltrc_print1 ("{FREE(%p)", (void *) l);
#endif
	if (lp) {
		if (!--lp->ntakers)
			lp->state = LS_INIT;
		lp->file = file;
		lp->line = line;
	}
#ifdef CTRC_LOCKS
	if (ctrace_used) {
		ctrc_begind (THREAD_ID, LOCK_RELEASE, &l, sizeof (l));
		if (lp && lp->name)
			ctrc_contd (lp->name, strlen (lp->name) + 1);
		ctrc_contd (file, strlen (file) + 1);
		ctrc_contd (&line, sizeof (line));
		ctrc_endd ();
	}
#endif
	res = lock_releasef (*l);
	if (res)
		warn_printf ("trc_lock_release: pthread_mutex_unlock(%s) returned error: %s",
			(lp) ? lp->name : NULL, strerror (res));
	ltrc_print1 ("%c}", '!');
	lock_releasef (trc_lock);
	return (res);
}

int trc_lock_destroy (pthread_mutex_t *l, const char *file, int line)
{
	LockState_t	*lp, *prev_lp;
	unsigned	h;
	int		res;

	ARG_NOT_USED (file)
	ARG_NOT_USED (line)

	lock_takef (trc_lock);
	h = lock_hash (l);
	for (prev_lp = NULL, lp = locks [h];
	     lp && lp->lock != l;
	     prev_lp = lp, lp = lp->next)
		;

	if (lp) {
		ltrc_print1 ("{Destroy(%s)", lp->name);
		if (prev_lp)
			prev_lp->next = lp->next;
		else
			locks [h] = lp->next;
	}
#ifdef LOG_LOCKS
	else
		ltrc_print1 ("{Destroy(%p)", (void *) l);
#endif
	ltrc_print1 ("%c}", '!');
#ifdef CTRC_LOCKS
	if (ctrace_used) {
		ctrc_begind (THREAD_ID, LOCK_DESTROY, &l, sizeof (l));
		if (lp && lp->name)
			ctrc_contd (lp->name, strlen (lp->name) + 1);
		ctrc_contd (file, strlen (file) + 1);
		ctrc_contd (&line, sizeof (line));
		ctrc_endd ();
	}
#endif
	res = pthread_mutex_destroy (l);
	if (res)
		warn_printf ("trc_lock_destroy: pthread_mutex_destroy(%s) returned error: %s",
			(lp) ? lp->name : NULL, strerror (res));

	if (lp)
		mm_fcts.free_ (lp);
	lock_releasef (trc_lock);
	return (res);
}

int trc_lock_required (pthread_mutex_t *l, const char *file, int line)
{
	LockState_t	*lp;

	lock_takef (trc_lock);
	lp = lock_lookup (l);
#ifdef LOG_LOCKS
	if (lp)
		ltrc_print1 ("{REQD(%s)}", lp->name);
	else
		ltrc_print1 ("{REQD(%p)}", (void *) l);
#endif
	if (lp && lp->state != LS_TAKEN)
		fatal_printf ("trc_lock_required (at %s:%u): lock(%s) not yet taken!", file, line, lp->name);

	lock_releasef (trc_lock);
	return (0);
}

void trc_lock_info (void)
{
	unsigned	h, i;
	LockState_t	*lp;

	/*lock_takef (trc_lock);*/
	for (h = 0; h < MAX_LOCK_HASH; h++)
		for (lp = locks [h]; lp; lp = lp->next) {
			if (lp->state == LS_TAKEN) {
				dbg_printf ("%s: taken (at %s:%d), %u waiters!\r\n", 
						lp->name, lp->file, lp->line, lp->nwaiters);
				if (lp->nwaiters)
					for (i = 0; i < lp->nwaiters; i++)
						dbg_printf ("\t%s:%d\r\n", 
							lp->wname [i], lp->wline [i]);
			}
			/*else
				dbg_printf ("%s: free", lp->name);*/
		}
	/*dbg_printf ("\r\n");*/
	/*lock_releasef (trc_lock);*/
}

#endif /* LOCK_TRACE */

#ifdef PTHREADS_USED

pthread_mutexattr_t	recursive_mutex;

#ifdef __APPLE__
#define PTHREAD_MUTEX_RECURSIVE_NP	PTHREAD_MUTEX_RECURSIVE
#endif

#if defined (NUTTX_RTOS)
#include <pthread.h>
#define PTHREAD_MUTEX_RECURSIVE_NP	PTHREAD_MUTEX_RECURSIVE
#endif

static lock_t		rclock;

void thread_init (void)
{
	pthread_mutexattr_init (&recursive_mutex);
	pthread_mutexattr_settype (&recursive_mutex, PTHREAD_MUTEX_RECURSIVE_NP);
	lock_init_nr (rclock, "rclock");
}

void rcl_access (void *p)
{
	ARG_NOT_USED (p);
	//lock_take (rclock);
}

void rcl_done (void *p)
{
	ARG_NOT_USED (p);
	//lock_release (rclock);
}

#else

void rcl_access (void *p) {}
void rcl_done (void *p) {}

#endif /* PTHREADS_USED */

#ifdef _WIN32

#define sema_init(sema,n)	((sema = CreateSemaphore (NULL, n, n, NULL)) == NULL)
#define sema_take(sema)		WaitForSingleObject (sema, INFINITE)
#define sema_release(sema)	ReleaseSemaphore (sema, 1, NULL)
#define sema_destroy(sema)	CloseHandle (sema)

#define ev_init(ev)		((ev = CreateEvent (NULL, 0, 0, NULL)) == NULL)
#define ev_wait(ev)		WaitForSingleObject (ev, INFINITE)
#define ev_signal(ev)		SetEvent (ev)
#define ev_destroy(ev)		CloseHandle (ev)

static lock_t		rclock;

int dds_cond_broadcast (cond_t *cv)
{
	int	have_waiters, res;

	/* Ensure that waiters and was_broadcast are consistent. */
	lock_take (cv->waiters_lock);
	have_waiters = 0;
	if (cv->waiters > 0) {
		cv->was_broadcast = 1;
		have_waiters = 1;
	}
	lock_release (cv->waiters_lock);
	res = DDS_RETCODE_OK;
	if (have_waiters) {

		/* Wake up all the waiters. */
		if (!sema_release (cv->sema))
			res = DDS_RETCODE_ERROR;

		/* Wait for all the awakened threads to acquire their part of
		   the counting semaphore. */
		else if (!ev_wait (cv->waiters_done))
		        res = DDS_RETCODE_ERROR;

		cv->was_broadcast = 0;
	}
	return (res);
}

int dds_cond_destroy (cond_t *cv)
{
	ev_destroy (cv->waiters_done);
	lock_destroy (cv->waiters_lock);
	sema_destroy (cv->sema);
	return (DDS_RETCODE_OK);
}

int dds_cond_init (cond_t *cv)
{
	int	res;

	cv->waiters = 0;
	cv->was_broadcast = 0;

	res = DDS_RETCODE_OK;
	if (sema_init (cv->sema, 1))
		res = 1;
	else if (lock_init_nr (cv->waiters_lock, NULL))
		res = 1;
	else if (ev_init (cv->waiters_done))
		res = 1;
	return (res);
}

int dds_cond_signal (cond_t *cv)
{
	int	have_waiters;

	/* If there aren't any waiters, then this is a no-op.  Note that
	   this function *must* be called with the <external_mutex> held
	   since otherwise there is a race condition that can lead to the
	   lost wakeup bug...  This is needed to ensure that the waiters
	   value is not in an inconsistent internal state while being
	   updated by another thread. */
	lock_take (cv->waiters_lock);
	have_waiters = cv->waiters > 0;
	lock_release (cv->waiters_lock);

	if (have_waiters)
		return (sema_release (cv->sema));
	else
		return (DDS_RETCODE_OK); /* No-op */
}

int dds_cond_wait (cond_t *cv, HANDLE mutex)
{
	int	res, last_waiter;

	/* Prevent race conditions on the waiters count. */
	lock_take (cv->waiters_lock);
	cv->waiters++;
	lock_release (cv->waiters_lock);

	res = DDS_RETCODE_OK;

	/* We keep the lock held just long enough to increment the count of
	   waiters by one.  Note that we can't keep it held across the call
	   sema_wait() since that will deadlock other calls to cond_signal(). */
	if (lock_release (mutex))
		return (DDS_RETCODE_ERROR);

	/* Wait to be awakened by a cond_signal() or cond_signal_all(). */
	res = sema_take (cv->sema);

	/* Reacquire lock to avoid race conditions on the waiters count. */
	lock_take (cv->waiters_lock);

	/* We're ready to return, so there's one less waiter. */
	cv->waiters--;
	last_waiter = cv->was_broadcast && cv->waiters == 0;

	/* Release the lock so that other collaborating threads can make
	   progress. */
	lock_release (cv->waiters_lock);

	if (res)
		; /* Bad things happened, so let's just return. */

	/* If we're the last waiter thread during this particular broadcast
	   then let all the other threads proceed. */
	if (last_waiter)
		ev_signal (cv->waiters_done);

	/* We must always regain the external_mutex, even when errors
	   occur because that's the guarantee that we give to our callers. */
	lock_take (mutex);

	return (res);
}

int dds_cond_timedwait (cond_t *cv, HANDLE mutex, const struct timespec *time)
{
	int		res, msec_timeout, last_waiter;
	struct timespec	now;

	/* Handle the easy case first. */
	if (!time)
		return (dds_cond_wait (cv, mutex));

	/* Prevent race conditions on the waiters count. */
	lock_take (cv->waiters_lock);
	cv->waiters++;
	lock_release (cv->waiters_lock);

	res = DDS_RETCODE_OK;

	if (time->tv_sec == 0 && time->tv_nsec == 0)
		msec_timeout = 0; /* Do a "poll." */
	else {
		/* Note that we must convert between absolute time (which is
		   passed as a parameter) and relative time (which is what
		   WaitForSingleObject() expects). */
		clock_gettime (0, &now);
		if (now.tv_sec > time->tv_sec ||
		    (now.tv_sec == time->tv_sec && now.tv_nsec > time->tv_nsec))
			msec_timeout = 0;
		else
			msec_timeout = (int) ((time->tv_sec - now.tv_sec) * 1000 +
			                      (time->tv_nsec - now.tv_nsec) / 1000000);
	}

	/* We keep the lock held just long enough to increment the count
	   of waiters by one.  Note that we can't keep it held across
	   the call to WaitForSingleObject since that will deadlock
	   other calls to cond_signal(). */
	lock_release (mutex);

	/* Wait to be awakened by a cond_signal() or cond_signal_all(). */
	res = WaitForSingleObject (cv->sema, msec_timeout);

	/* Reacquire lock to avoid race conditions. */
	lock_take (cv->waiters_lock);
	cv->waiters--;
	last_waiter = cv->was_broadcast && cv->waiters == 0;
	lock_release (cv->waiters_lock);

	if (res != WAIT_OBJECT_0) {
		if (res == WAIT_TIMEOUT)
			res = DDS_RETCODE_TIMEOUT;
		else
			res = DDS_RETCODE_ERROR;
	}
	else
		res = DDS_RETCODE_OK;

	if (last_waiter)

		/* Release the signaler/broadcaster if we're the last waiter. */
		ev_signal (cv->waiters_done);

	/* We must always regain the external mutex, even when errors
	   occur because that's the guarantee that we give to our callers. */
	lock_take (mutex);

	return (res);
}

void rcl_access (void *p)
{
	ARG_NOT_USED (p);
	lock_take (rclock);
}

void rcl_done (void *p)
{
	ARG_NOT_USED (p);
	lock_release (rclock);
}

void thread_init (void)
{
	lock_init_nr (rclock, "RCLock");
}

#endif /* _WIN32 */
