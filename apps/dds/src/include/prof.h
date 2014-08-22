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

/* prof.h -- Generic profiling support functionality. */

#ifndef __prof_h_
#define	__prof_h_

#ifndef PROFILE

#define	PROF_PID(v)
#define	PROF_ITER(n)
#define	PROF_INC(n)
#define	PROF_INIT(name, pid)

#define prof_init(name,pid) 
#define prof_start(pid)
#define prof_stop(pid,iters)
#define prof_stop_wclog(pid, iters, wcarg)
#define prof_diff(pid,iters,start)

#else

#define PROF_PID(v)     static unsigned v = ~0U;
#define PUB_PROF_PID(v) unsigned v = ~0U;
#define	EXT_PROF_PID(v)	extern unsigned v;
#define PROF_INIT(name,pid)  do {if (pid == ~0U) prof_alloc(name, &(pid)); } while(0)
#define PROF_ITER(n)   unsigned n = 0;
#define PROF_INC(n)    n++;

int prof_init (void);

/* Initialize the profiling library. */

int prof_alloc (const char *name, unsigned *pid);

/* Add a new profiling context.  The profile id (pid) is returned on success
   (returns 0).  If no more profiling contexts are available, a non-0 error
   code is returned. */

void prof_free (unsigned pid);

/* Free a profiling context. */

void prof_trace (char *name, int enable);

/* Enable/disable tracing of profile start/stop. */

#define	PROF_OK		0	/* Profile started. */
#define PROF_ERR_NFOUND	1	/* No such profile id. */
#define	PROF_ERR_INACT	2	/* Profiling has stopped. */
#define	PROF_ERR_BUSY	3	/* Profile already started. */

int prof_start (unsigned pid);

/* Start profile timing for the given context.  A number of error codes may be
   returned (see above: PROF_*). */

#define prof_stop(pid, iters)   prof_stop_wclog (pid, iters, 0)

/* Stop profile timing that was started by prof_start ().
   It also returns the number of microseconds that the profile took.
   This can be used to for instance stop a trace when you encounter
   something that took to much time... */

unsigned prof_stop_wclog (unsigned pid, unsigned iters, void *wcarg);

/* Same as prof_stop, except that you can add a "worst case" argument.
   this argument is kept if we have a new duration record. */

void prof_list (void);

/* Display all profiling contexts. */

void prof_clear (unsigned long delay_ms, unsigned long duration_ms);

/* Clear and restart all profiling counters.
   If delay_ms is given, it specifies that profiling will only start after
   that number of milliseconds.  If duration_ms is given, profiling will stop
   after that number of milliseconds. */

void prof_calibrate (void);

/* Calibrate profiling delays. */

#endif
#endif /* __nprof_h_ */

