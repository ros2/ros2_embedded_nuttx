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

/* thread.h -- Abstracts the various threading APIs. */

#ifndef __thread_h_
#define	__thread_h_

void thread_init (void);

#ifdef _WIN32

#include "win.h"

#define	lock_t			HANDLE

#define LOCK_STATIC_INIT	CreateMutex (NULL, 0, /*s*/NULL)
#define	lock_init_r(l,s)	((l = CreateMutex (NULL, 0, /*s*/NULL)) == NULL)
#define	lock_init_nr(l,s)	((l = CreateMutex (NULL, 0, /*s*/NULL)) == NULL)
#define	lock_try(l)		(WaitForSingleObject (l, 0) != WAIT_TIMEOUT)
#define	lock_take(l)		WaitForSingleObject (l,INFINITE)
#define	lock_release(l)		ReleaseMutex (l)
#define	lock_takef		lock_take
#define	lock_releasef		lock_release
#define lock_destroy(l)		CloseHandle (l)
#define	lock_required(l)

#define	thread_t		HANDLE
#define thread_result_t		void

#define thread_create(t,f,args)	t = (HANDLE) _beginthread(f,0,args)
#define	thread_exit(n)		ExitThread(n)

#define thread_yield()		Sleep(0)
#define	thread_id()		GetCurrentThread()
#define	thread_wait(t,st)	WaitForSingleObject(t, INFINITE)
#define thread_return(p)	_endthread()

/* Condition variables can not be defined in a simple manner on Windows since
   they were only implemented from Windows Vista onwards and are thus not avail-
   able on Windows XP, which is by far a more popular platform.  We therefor
   need to implement this synchronisation feature on top of the other synchron-
   isation primitives. */
typedef struct condition_variable_st {
	long	waiters;	/* Number of waiting threads. */
	lock_t	waiters_lock;	/* Serialize access to the waiters count. */
	HANDLE	sema;		/* Queue up threads waiting for the condition.*/
	HANDLE	waiters_done;	/* Auto reset event to wait for the semaphore.*/
	size_t	was_broadcast;	/* Keeps track if we were broadcasting or just
				   signaling. */
} cond_t;

int dds_cond_init (cond_t *cond);
int dds_cond_wait(cond_t *cond, HANDLE mutex);
int dds_cond_timedwait (cond_t *cond, HANDLE mutex, const struct timespec *abstime);
int dds_cond_signal (cond_t *cond);
int dds_cond_broadcast (cond_t *cond);
int dds_cond_destroy (cond_t *cond);

#define	cond_init(c)		dds_cond_init (&c)
#define	cond_wait(c,m)		dds_cond_wait (&c, m)
#define	cond_wait_to(c,m,at)	dds_cond_timedwait (&c, m, &at)
#define	cond_signal(c)		dds_cond_signal (&c)
#define	cond_signal_all(c)	dds_cond_broadcast (&c)
#define cond_destroy(c)		dds_cond_destroy (&c)

#define THREADS_USED

#elif defined (PTHREADS_USED)

#include <pthread.h>
#include <errno.h>

#define	lock_t			pthread_mutex_t
#define LOCK_STATIC_INIT	PTHREAD_MUTEX_INITIALIZER

extern pthread_mutexattr_t	recursive_mutex;

#ifdef LOCK_TRACE

int trc_lock_init (pthread_mutex_t *l, int rec, const char *name, const char *file, int line);
int trc_lock_try (pthread_mutex_t *l, const char *file, int line);
int trc_lock_take (pthread_mutex_t *l, const char *file, int line);
int trc_lock_release (pthread_mutex_t *l, const char *file, int line);
int trc_lock_destroy (pthread_mutex_t *l, const char *file, int line);
int trc_lock_required (pthread_mutex_t *l, const char *file, int line);
void trc_lock_info (void);

#define	lock_init_r(l,s)	trc_lock_init(&l, 1, s, __FILE__, __LINE__)
#define	lock_init_nr(l,s)	trc_lock_init(&l, 0, s, __FILE__, __LINE__)
#define	lock_try(l)		trc_lock_try(&l, __FILE__, __LINE__)
#define	lock_take(l)		trc_lock_take(&l, __FILE__, __LINE__)
#define	lock_release(l)		trc_lock_release(&l, __FILE__, __LINE__)
#define	lock_takef(l)		pthread_mutex_lock(&l)
#define	lock_releasef(l)	pthread_mutex_unlock(&l)
#define	lock_destroy(l)		trc_lock_destroy(&l, __FILE__, __LINE__)
#define	lock_required(l)	trc_lock_required(&l, __FILE__, __LINE__)

#else

/* NuttX interface match */

#define	lock_init_r(l,s)	pthread_mutex_init(&l,&recursive_mutex)
#define	lock_init_nr(l,s)	pthread_mutex_init(&l,NULL)
#define	lock_try(l)		(pthread_mutex_trylock(&l) != EBUSY)
#define	lock_take(l)		pthread_mutex_lock(&l)
#define	lock_release(l)		pthread_mutex_unlock(&l)
#define	lock_takef		lock_take
#define	lock_releasef		lock_release
#define lock_destroy(l)		pthread_mutex_destroy(&l)
#define	lock_required(l)
#endif

#define	thread_t		pthread_t
#define thread_result_t		void *

#define thread_create(t,f,args)	pthread_create(&t,NULL,f,args)
#define	thread_exit(n)		pthread_exit(n)
#define	thread_yield()		sched_yield()
#define	thread_id()		pthread_self()
#define	thread_wait(t,st)	pthread_join(t,st)
#define thread_return(p)	return (p)

#define cond_t			pthread_cond_t

#define	cond_init(c)		pthread_cond_init(&c,NULL)
#define	cond_wait(c,m)		pthread_cond_wait(&c, &m)
#define	cond_wait_to(c,m,at)	pthread_cond_timedwait(&c,&m,&at)
#define	cond_signal(c)		pthread_cond_signal(&c)
#define	cond_signal_all(c)	pthread_cond_broadcast(&c)
#define cond_destroy(c)		pthread_cond_destroy(&c)

#define	THREADS_USED

#elif defined (NO_LOCKS)

#define	lock_t			int
#define LOCK_STATIC_INIT	0

#define	lock_init_r(l,s)
#define	lock_init_nr(l,s)
#define	lock_try(l)
#define	lock_take(l)		0
#define	lock_release(l)
#define	lock_takef
#define	lock_releasef
#define lock_destroy(l)
#define lock_required(l)

#define	cond_t			int

#else /* No PTHEADS, neither NO_LOCKS */

#define	lock_t			int

#define	lock_init_r(l,s)	l = 0
#define	lock_init_nr(l,s)	l = 0
#define	lock_try(l)		(!l) ? l=1, 1 : 0
#define	lock_take(l)		(l) ? do { DDS_wait(1000); } while (l), l = 1 : l = 1
#define	lock_release(l)		l = 0
#define	lock_takef		lock_take
#define	lock_releasef		lock_release
#define lock_destroy(l)		l = -1
#define lock_required(l)

#define cond_t			int

#define cond_init(c)		c = 0
#define cond_wait(c,m)		while (!c) DDS_wait(1000); m = 1
#define cond_wait_to(c,m,at)	if (!c) DDS_wait(at); m = 1
#define cond_signal(c)		c = 1
#define cond_signal_all(c)	c = 1
#define cond_destroy(c)		c = -1
#endif

void rcl_access (void *p);

/* Take a refcount lock of the given struct. */

void rcl_done (void *p);

/* Release a refcount lock. */

#endif /* !__thread_h_ */

