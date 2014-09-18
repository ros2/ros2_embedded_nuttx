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

/* prof.c -- Generic profiling support functionality. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sys.h"
#include "log.h"
#include "error.h"
#include "ctrace.h"
#include "dds/dds_error.h"
#include "prof.h"

#ifdef PROFILE

/*#define MEASURE_CACHE  ** Define this if you want to measure cache/mem performance. */

#if __i386__ || __amd64__

typedef uint64_t Cycles_t;

Cycles_t rdtsc (void)
{
	unsigned a, d;

	__asm__ volatile("rdtsc" : "=a" (a), "=d" (d));
 
	return (((Cycles_t) a) | (((Cycles_t) d) << 32));
}
#else
#if __mips__
typedef uint64_t Cycles_t;
Cycles_t rdtsc(void)
{
	   struct timespec ts;

	   clock_gettime (CLOCK_MONOTONIC, &ts);
	   return ((Cycles_t) ts.tv_sec * 1000 * 1000 * 1000 + (Cycles_t)  ts.tv_nsec);
}
#else
#include <asm/timex.h>
#define	rdtsc rdtscll
#define	Cycles_t cycles_t
#endif
#endif

#define	sys_time_hr(hr)	hr = rdtsc()

#define	MAX_PROFS	1024	/* Max. # of profile contexts. */

/* History masks from short to long. We assume that most often we get short
   profile periods, e.g. <= 16ms. */
typedef struct hist_id_st { 
	uint32_t	mask;
	const char	*name;
} HIST_ID_ST;

static const HIST_ID_ST hist_masks [] = {
	{ 0xFFFFF000, "  < 4us "},
	{ 0xFFFFC000, " < 16us "},
	{ 0xFFFF0000, " < 64us "},
	{ 0xFFFC0000, "< 256us "},
	{ 0xFFF00000, "  < 1ms "},
	{ 0xFFC00000, "  < 4ms "},
	{ 0xFF000000, " < 16ms "},
	{ 0x00000000, " > 16ms "},
};

#define QTY_HIST	(sizeof (hist_masks) / sizeof (HIST_ID_ST))

#define PROF_STARTED	1	/* Profile started. */
#define PROF_TRACE	2	/* Create a trace at start/stop. */

typedef struct prof_st {
	const char	*name;
	unsigned long	ncalls;
	unsigned long	niter;
	Cycles_t	hrt;
	Time_t		min;
	Time_t		max;
	Time_t		total;
	void		*wc_arg;		/* Worst-case argument. */
	unsigned long	flags;
	unsigned long	hist [QTY_HIST];
} PROF_ST, *PROF;

static PROF_ST		profs [MAX_PROFS];
static unsigned 	calib_delta;		/* In nanoseconds. */
static unsigned 	cycles_100ms;		/* In cycles. */
static Time_t		start_req;		/* Requested start time. */
static Time_t		stop_req;		/* Requested stop_time. */
static Time_t		duration_req;		/* Requested duration. */
static Cycles_t		start_cycles;		/* Effective profiling start. */
static Time_t		start_time;		/* Effective profiling start. */
static Time_t		prof_time;		/* Effective profiling time. */
static int 		profile_disabled = 1;

typedef struct sys_time_eng_st {	/* Engineering time. */
	unsigned long	hp;		/* High precision time */
	int		eng_factor;	/* Engineering factor: 0=s, -3=ms,
							      -6=us, -9=ns. */
} STIMENG_ST, *STIMENG;

#ifdef MEASURE_CACHE

static void measure_cache (void)
{
	int		i;
	unsigned	pid;

	static char buf1 [100000] ; /* larger than 32K cache */
	static char buf2 [100000];

	if (prof_alloc ("CACHE_10MB", &pid))
		return;

	/* First test cached: 1000 loops of 10K: first loop to get it in the
	   cache. */
	memcpy (buf2, buf1, 1000);
	prof_start (pid);
	for (i = 0; i < 1000; i++)
		memcpy(buf2, buf1, 10000);
	prof_stop (pid, 1);

	/* Second loop: 100 times 100K: not in cache. */
	memcpy (buf2, buf1, 100000); /* Be sure it is flushed. */
	prof_start (pid);
	for (i = 0; i < 100; i++)
		memcpy (buf2, buf1, 100000);
	prof_stop (pid, 1);
}

static void extra_measurements (void)
{
	static int done = 0;

	if (done)
		return;

	measure_cache ();
	done = 1;
}
#endif

#ifdef CTRACE_USED

enum {
	PROF_CTRL,
	PROF_START, PROF_END
};

static const char *prof_fct_str [] = {
	"Control", "Start", "Stop"
};

/* prof_trace -- Enable/disable tracing of profile start/stop. */

void prof_trace (char *name, int enable)
{
	PROF		pp;
	unsigned	i;

	for (i = 0, pp = profs; i < MAX_PROFS; i++, pp++) {
		if (!pp->name) 
			continue;

		if (!name || strcmp (name, pp->name) == 0) {
			if (enable)
				pp->flags |= PROF_TRACE;
			else
				pp->flags &= ~PROF_TRACE;
		}
	}
}

#endif

/* prof_init -- Initialize the profiling library. */

int prof_init (void)
{
	prof_clear (0, 0);
	prof_calibrate ();
#ifdef MEASURE_CACHE
	extra_measurements ();
#endif
#ifdef CTRACE_USED
	log_fct_str [PROF_ID] = prof_fct_str;
#endif
	sys_time_hr (start_cycles);
	return (0);
}

/* prof_alloc -- Add a new profiling context.  Returns a profile id (pid). */

int prof_alloc (const char *name, unsigned *pid)
{
	PROF		pp;
	unsigned	i;

	/* Check if name exists: if so re-use same pid. */
	for (i = 0, pp = profs; i < MAX_PROFS; i++, pp++)
		if (pp->name && (strcmp (pp->name, name) == 0)) {
			*pid = i;
			return (0);
		}

	/* Get an empty item. */
	for (i = 0, pp = profs; i < MAX_PROFS; i++, pp++)
		if (!pp->name) {
			*pid = i;
			pp->name = name;
			pp->min.nanos = ~0;
			pp->min.seconds = 0x7fffffff;
			pp->max.nanos = 0;
			pp->max.seconds = 0;
			pp->flags = 0/*PROF_TRACE*/;
			return (0);
		}
	*pid = ~0;
	warn_printf ("prof_alloc(%s): out of resources!\r\n", name);
	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

/* prof_free -- Remove a profiling context. */

void prof_free (unsigned pid)
{
	PROF	pp;

	if (pid >= MAX_PROFS)
		return;

	pp = &profs [pid];
	pp->name = NULL;
}

/* prof_start -- Start profile timing for the given context. */

int prof_start (unsigned pid)
{
	PROF	pp;

	if (pid >= MAX_PROFS)
		return (PROF_ERR_NFOUND);

	pp = &profs [pid];

	if (profile_disabled) {

		/* Do we have a delayed start? */
		if (!start_req.seconds && !start_req.nanos)
			return (PROF_ERR_INACT);

		/* Yes: get time and check if profile may start */
		sys_gettime (&start_time);
		if (!TIME_LT (start_time, start_req)) {
			profile_disabled = 0;
			start_req.seconds = start_req.nanos = 0;
			if (duration_req.seconds || duration_req.nanos) {
				stop_req = start_time;
				stop_req.seconds += duration_req.seconds;
				stop_req.nanos += duration_req.nanos;
				if (stop_req.nanos > 1000000000) {
					stop_req.seconds++;
					stop_req.nanos -= 1000000000;
				}
			}
			else {
				stop_req.seconds = 0;
				stop_req.nanos = 0;
			}
#ifdef CTRACE_USED
			ctrc_printf (PROF_ID, PROF_CTRL, "After delay - start profile.");
#endif
			sys_time_hr (start_cycles);
		}
		else
			return (PROF_ERR_INACT);
	}
	if ((pp->flags & PROF_STARTED) != 0)
		return (PROF_ERR_BUSY);

	pp->flags |= PROF_STARTED;
#ifdef CTRACE_USED
	if ((pp->flags & PROF_TRACE) != 0)
		ctrc_printd (PROF_ID, PROF_START, &pid, sizeof (pid));
#endif
	sys_time_hr (pp->hrt);
	return (PROF_OK);
}

static void sys_time_diff (Cycles_t prev, Cycles_t *new, Time_t *diff)
{
	uint64_t	delta;

	sys_time_hr (*new);
	delta = *new - prev - calib_delta;
	delta = (delta * 100000000) / cycles_100ms;
	diff->seconds = delta / 1000000000;
	diff->nanos = delta % 1000000000;
	if (diff->seconds || diff->nanos > calib_delta) {
		if (diff->nanos < calib_delta) {
			diff->seconds--;
			diff->nanos = diff->nanos + 1000000000 - calib_delta;
		}
		else
			diff->nanos -= calib_delta;
	}
	else
		diff->nanos = 0;
}

unsigned prof_stop_wclog (unsigned pid, unsigned divider, void *wcarg)
{
	PROF		pp;
	Cycles_t	nt;
	Time_t		t, ct;
	unsigned	i;
	const HIST_ID_ST *histp; 

	if (!divider || pid >= MAX_PROFS || profile_disabled)
		return (0);

	pp = &profs [pid];

	/* Start was not done (tracing disabled, or invalid sequence of calls)
	   or start done, but trace is now disabled: do not trace! */
	if (profile_disabled || (pp->flags & PROF_STARTED) == 0) 
		return (0);

	sys_time_diff (pp->hrt, &nt, &t);
	pp->flags &= ~PROF_STARTED;

#ifdef CTRACE_USED
	if ((pp->flags & PROF_TRACE) != 0)
		ctrc_printd (PROF_ID, PROF_END, &pid, sizeof (pid));
#endif

	/* Check if we have to measure only some duration. */
	if (stop_req.seconds || stop_req.nanos) {
		sys_gettime (&ct);
		if (!TIME_LT (ct, stop_req)) {
			sys_time_diff (start_cycles, &nt, &prof_time);
			stop_req.seconds = 0;
			stop_req.nanos = 0;
			profile_disabled = 1;
		}
	}

	/* Prevent overflow: we stop gathering totals if more than 4000000000 samples. */
	if (divider < (0xFFFFFFFF - pp->niter)) {
		pp->ncalls++;
		pp->niter += divider;
		pp->total.nanos   += t.nanos;
		pp->total.seconds += t.seconds;
		if (pp->total.nanos >= 1000000000) {
			pp->total.nanos -= 1000000000;
			pp->total.seconds++;
		}
	}

	/* Quick min/max call. */
	if (TIME_LT (t, pp->min))
		pp->min = t;
	if (TIME_GT (t, pp->max)) {
		pp->max = t;
		pp->wc_arg = wcarg;
	}

	/* Prevent overflow: stop gathering histogram if more than 4000000000 samples. */
	if (divider < (0xFFFFFFFF - pp->niter)) {
		if (t.seconds)
			pp->hist [QTY_HIST - 1] += divider;
		else {
			/* Do histogram. */
			for (i = 0, histp = hist_masks; i < QTY_HIST; i++, histp++)
				if (!(t.nanos & histp->mask))
					break;

			pp->hist [i] += divider;
		}
	}
	return (t.nanos);
}

static void large_number_print (unsigned   number,
				const char *units,
				int        smalldisp)
{
	const char 	scale [] =" KMG";
	int		idx = 0;

	if (!smalldisp) {
		while (number >= 1000000) {
			number /=1000;
			idx++;
		}
	
		if (units) 
			if (!idx) 
				dbg_printf ("%7u%s ", number, units);
			else
				dbg_printf ("%6u%c%s ", number, scale [idx], units);
		else 
			if (!idx) 
				dbg_printf ("%7u ", number);
			else
				dbg_printf ("%6u%c ", number, scale [idx]);
	}
	else {
		while (number >= 10000) {
			number /=1000;
			idx++;
		}
		if (units) 
			if (!idx) 
				dbg_printf ("%4u%s ", number, units);
			else
				dbg_printf ("%3u%c%s ", number, scale [idx], units);
		else 
			if (!idx) 
				dbg_printf ("%4u ", number);
			else
				dbg_printf ("%3u%c ", number, scale [idx]);

	}
}

static void timer_print (Time_t *time)
{
	if (time->seconds >= 360000)
		dbg_printf ("%5uh ", time->seconds / 3600);
	else if (time->seconds >=10000)
		dbg_printf ("%5um ", time->seconds / 60);
	else if (time->seconds >= 10)
		dbg_printf ("%5us ", time->seconds);
	else if (time->seconds || time->nanos >= 10000000)
		dbg_printf ("%4ums ", (time->seconds * 1000) + (time->nanos / 1000000));
	else if (time->nanos >= 10000)
		dbg_printf ("%4uus ", time->nanos / 1000);
	else
		dbg_printf ("%4uns ", time->nanos);
}

static void time_to_engtime (STIMENG_ST *engtime, Time_t *time)
{
	/* In case we have enough seconds, just return seconds. */
	if (time->seconds >= 10000) {
		engtime->hp  = time->seconds; /* in sec */
		engtime->eng_factor = 0;
	}
	else if (time->seconds >= 1000) {
		engtime->hp   = (time->seconds * 1000) + 
				 (time->nanos / 1000000); /* in ms */
		engtime->eng_factor = -3;
	}
	else if (time->seconds >= 4) {
		engtime->hp   = (time->seconds * 1000000) + 
				 (time->nanos / 1000); /* in us */
		engtime->eng_factor = -6;
	}
	else {
		engtime->hp  = (time->seconds * 1000000000) + 
			        time->nanos; /* in ns */
		engtime->eng_factor = -9;
	}
}

static void engtime_to_time (Time_t *time, STIMENG_ST *engtime)
{
	static const unsigned factors [] = {
		1U,  10U , 100U, 1000U, 10000U, 100000U, 1000000U,
		10000000U, 100000000U, 1000000000U
	};
	unsigned factor;
	unsigned tmptime;

	/* We support min -9 max 0. */
	tmptime = engtime->hp;
	while (engtime->eng_factor < -9) {
		engtime->eng_factor++;
		tmptime /= 10;
	}
	while (engtime->eng_factor > 0) {
		engtime->eng_factor--;
		tmptime *= 10;
	}

	factor = factors [0 - engtime->eng_factor];

	time->seconds =  tmptime / factor;
	time->nanos   = (tmptime % factor) * (1000000000 / factor);
}

static void percentage_calc_and_print (Time_t *part, Time_t *tot)
{
	STIMENG_ST	num;  
	STIMENG_ST	div; 
	unsigned	result;
	unsigned	digits;

	/* Try to have large number on both sizes. */ 
	time_to_engtime (&num, part);
	time_to_engtime (&div, tot);

	/* Step 1: we are only interested in difference of factors (eg ms/ms = s/s)
	   and we assume that divider has a larger or eq. factor than number, as it
	   should be larger! */
	num.eng_factor -= div.eng_factor;
	div.eng_factor  = 0;

	/* We go for 4 digits: 99.99 percent
	   so we have to multiply the upper by 10000 or divide divider by 10000.
	   We start with the number factor. */
	digits = 0; 
	while (num.eng_factor < 0 && digits < 4) {
		num.eng_factor++; /* Number * 10 = factor++ */
		digits++;
	}

	/* If num is still to small for 4 digits: percentage is zero. */
	if (num.eng_factor) {
		dbg_printf ("00.00 ");
		return;
	}

	/* For the rest of the digits, we try to higher the number first. */
	while (digits < 4 && num.hp < 400000000) {
		digits++;
		num.hp *= 10;
	}

	/* If still some digits left, decrease divider. */
	while (digits < 4) {
		div.hp /= 10;
		digits++;
	}

	/* And the result is... */
	if (div.hp)
		result = num.hp / div.hp;
	else
		result = ~0; /* infinite ;-) */

	/* Now print: */
	if (result > 9999) 
		dbg_printf ("99.99 ");
	else
		dbg_printf ("%2u.%02u ", result / 100U, result % 100U);
}

static void average_calc_and_print (Time_t *time, unsigned long qty)
{
	STIMENG_ST	engtime;  
	Time_t		avg;

	/* Get high resolution time number. */
	time_to_engtime (&engtime, time);

	/* If time = 0, or qty = 0, just print 0. */
	if (!qty || !engtime.hp) {
		avg.nanos   = 0;
		avg.seconds = 0;
	}
	else {	
		/* Try to get a as high as possible number, to be accurate... */
		while (engtime.hp < 400000000U) {
			engtime.hp  *= 10;
			engtime.eng_factor--;
		}
	
		/* Now do the division. */
		engtime.hp /= qty;
	}

	/* Transform to time struct. */
	engtime_to_time (&avg, &engtime);
	timer_print (&avg);
}

static void frequency_calc_and_print (Time_t *tot, unsigned long qty)
{
	STIMENG_ST	engtime;
	unsigned	hz = 0;

	if (!qty) {
		dbg_printf ("    0Hz ");
		return;
	}
	time_to_engtime (&engtime, tot);

	/* As we divide, the factor reverse eg /ms = KHz. */
	engtime.eng_factor = -engtime.eng_factor;

	/* If time is too large compared with qty, try to use lower factor. */
	while (engtime.hp && ((hz = qty / engtime.hp) < 10)) {
		if (qty < 4000000)
			qty *= 1000;
		else
			engtime.hp /= 1000;
		engtime.eng_factor -= 3;
	}

	if (!engtime.hp) 
		dbg_printf ("  ???Hz "); 
	else if (engtime.eng_factor == 9)
		dbg_printf ("%4uGHz ", hz);
	else if (engtime.eng_factor == 6)
		dbg_printf ("%4uMHz ", hz);
	else if (engtime.eng_factor == 3)
		dbg_printf ("%4uKHz ", hz);
	else if (engtime.eng_factor == 0)
		dbg_printf (" %4uHz ", hz);
	else if (engtime.eng_factor == -3)
		dbg_printf ("%4umHz ", hz);
	else if (engtime.eng_factor < 0)
		dbg_printf ("    0Hz ");
	else
		dbg_printf ("  ???Hz "); 
}

static unsigned	sorted [MAX_PROFS];

static int cmp_fct (const void *p1, const void *p2)
{
	unsigned	i1 = (unsigned *) p1 - sorted;
	unsigned	i2 = (unsigned *) p2 - sorted;
	PROF		pp1, pp2;
	int		diff;

	pp1 = &profs [sorted [i1]];
	pp2 = &profs [sorted [i2]];

	if (!pp1->name || !pp1->ncalls)
		if (!pp2->name || !pp2->ncalls)
			diff = 0;
		else
			diff = -1;
	else if (!pp2->name || !pp2->ncalls)
		diff = 1;

	else if (pp1->total.seconds == pp2->total.seconds) {
		if (pp1->total.nanos == pp2->total.nanos)
			diff = 0;
		else if (pp1->total.nanos > pp2->total.nanos)
			diff = 1;
		else
			diff = -1;
	}
	else if (pp1->total.seconds > pp2->total.seconds)
		diff = 1;
	else
		diff = -1;
	return (-diff);
}

void prof_list (void)
{
	PROF		pp;
	PROF_ST		p;
	unsigned	i, j, l;
	Cycles_t	new;
	Time_t		diff;
	const HIST_ID_ST *histp; 
	int		old_prof_dis_flag;
	int		valid_data = 0;

	old_prof_dis_flag = profile_disabled;
	profile_disabled = 1;

	/* If we have stopped, set correct stop time. */
	if (prof_time.seconds || prof_time.nanos)
		diff = prof_time;
	else
		sys_time_diff (start_cycles, &new, &diff);

	memset (sorted, 0, sizeof (sorted));
	for (i = 0; i < MAX_PROFS; i++)
		sorted [i] = i;
	qsort (sorted, MAX_PROFS, sizeof (unsigned), cmp_fct);
	for (i = 0, l = 0; i < MAX_PROFS; i++) {
		pp = &profs [sorted [i]];
		if (!pp->name || !pp->ncalls)
			continue;

		j = strlen (pp->name);
		if (j > l)
			l = j;
	}
	if (l < 10)
		l = 10;
	dbg_printf ("Name      ");
	for (i = 10; i < l; i++)
		dbg_printf (" ");
	dbg_printf ("   calls   iters    freq  total  perc    min    avg    max worstcase\r\n");
	dbg_printf ("----      ");
	for (i = 10; i < l; i++)
		dbg_printf (" ");
	dbg_printf ("   -----   -----    ----  -----  ----    ---    ---    --- ---------\r\n");
	for (i = 0; i < MAX_PROFS; i++) {
		pp = &profs [sorted [i]];
		if (!pp->name || !pp->ncalls)
			continue;

		p = *pp;
		valid_data = 1;
		dbg_printf ("%*s ", -l, p.name);
		if (p.ncalls) {
			large_number_print (p.ncalls, 0, 0);
			large_number_print (p.niter,  0, 0);
			frequency_calc_and_print (&diff, p.niter);
			timer_print (&p.total);
			percentage_calc_and_print (&p.total, &diff);
			timer_print (&p.min);
			average_calc_and_print (&p.total, p.niter);
			timer_print (&p.max);
			if (p.wc_arg)
				dbg_printf ("%p ", p.wc_arg);
		}
		else
			dbg_printf ("%7lu %7lu ", p.ncalls, p.niter);
		dbg_printf ("\r\n");
	}

	/* Now print histogram: if some data. */
	if (valid_data) {
		dbg_printf ("\r\nName       ");
		for (i = 10; i < l; i++)
			dbg_printf (" ");
		for (i = 0, histp = hist_masks; i < QTY_HIST; i++, histp++)
			dbg_printf ("%s", histp->name);

		dbg_printf ("\r\n----       ");
		for (i = 10; i < l; i++)
			dbg_printf (" ");
		dbg_printf ("------- ------- ------- ------- ------- ------- ------- -------\r\n") ;
		for (i = 0; i < MAX_PROFS; i++) {
			pp = &profs [sorted [i]];
			if (!pp->name || !pp->ncalls)
				continue;

			dbg_printf ("%*s ", -l, pp->name);
			for (j = 0; j < QTY_HIST; j++)
				large_number_print (pp->hist [j], 0, 0);
			dbg_printf ("\r\n");
		}
	}
	else 
		dbg_printf ("\r\nNo samples gathered\r\n\r\n");

	profile_disabled = old_prof_dis_flag;
	dbg_printf ("Profiling period =  ");
	timer_print (&diff);
	dbg_printf ("\r\n");
}

/* prof_clear -- Clear all profiles. */

void prof_clear (unsigned long delay_ms, unsigned long duration_ms)
{
	PROF		pp;
	unsigned	i,j;

	profile_disabled  = 1;
	if (duration_ms) {
		duration_req.seconds = duration_ms / 1000;
		duration_req.nanos = duration_ms % 1000;
	}
	else {
		duration_req.seconds = 0;
		duration_req.nanos = 0;
	}
	for (i = 0, pp = profs; i < MAX_PROFS; i++, pp++) {
		if (!pp->name)
			continue;

		pp->ncalls = pp->niter = pp->max.seconds = pp->max.nanos = 
			pp->total.nanos = pp->total.seconds = 0;
		pp->flags &= ~PROF_STARTED;
		pp->min.nanos = ~0;
		pp->min.seconds = 0x7fffffff;
		for (j = 0; j < QTY_HIST; j++)
			pp->hist [j] = 0;
	}

	/* If delayed start, don't re-enable. */
	if (!delay_ms) {
		sys_time_hr (start_cycles);
		start_req.seconds = 0;
		start_req.nanos = 0;
		if (duration_ms) {
			stop_req = start_time;
			stop_req.seconds += duration_req.seconds;
			stop_req.nanos += duration_req.nanos;
			if (stop_req.nanos > 1000000000) {
				stop_req.seconds++;
				stop_req.nanos -= 1000000000;
			}
		}
		else {
			stop_req.seconds = 0;
			stop_req.nanos = 0;
		}
		profile_disabled = 0;
	}
	else {
		sys_gettime (&start_req);
		start_req.seconds += delay_ms / 1000;
		start_req.nanos += delay_ms % 1000;
		if (start_req.nanos > 1000000000) {
			start_req.seconds++;
			start_req.nanos -= 1000000000;
		}
	}
}

/* prof_calibrate -- Calibrate profiling delays. */

void prof_calibrate (void)
{
	PROF		pp;
	Time_t		start;
	Time_t		t;
	Cycles_t	start_c;
	Cycles_t	stop_c;
	unsigned	i, index;

	if (prof_alloc ("PROF_CALI", &index))
		return;

	for (i = 0; i < 10; i++) {
		do {
			sys_gettime (&start);
		}
		while (start.nanos > 1000000);
		sys_time_hr (start_c);
		do {
			sys_gettime (&t);
		}
		while (t.nanos - start.nanos < 100000000);
		sys_time_hr (stop_c);
		if (stop_c > start_c) {
			cycles_100ms = stop_c - start_c;
			break;
		}
	}
	calib_delta = 0;
	for (i = 0; i < 4; i++) {
		prof_start (index);
		prof_stop (index, 1);
	}
	pp = &profs [index];
	calib_delta = pp->min.nanos;
	log_printf (PROF_ID, 0, "Profiling calibrated : %u cycles/100ms, delta = %uns.\r\n",
				cycles_100ms, calib_delta);
}

#else
int	prof_no_warnings;
#endif

