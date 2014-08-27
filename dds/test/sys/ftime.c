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

#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

#define FTIME_STRUCT

typedef struct time_st {
	int32_t		seconds;	/* Time in seconds. */
	uint32_t	nanos;		/* Time in nanoseconds. */
} Time_t;

#ifndef FTIME_STRUCT

typedef int64_t FTime_t;	/* Fractionalized time can be used directly in
				   time calculations. */

#else /* FTIME_STRUCT		** Fractionalized time, not usable directly. */

typedef struct ftime_st {
	int32_t		seconds;	/* Time in seconds. */
	uint32_t	fraction;	/* Time in seconds / 2^32 */
} FTime_t;

#endif


/* sys_getftime -- Get the system time in seconds/fractions. */

void sys_getftime (FTime_t *time)
{
	struct timespec	ts;

	clock_gettime (CLOCK_REALTIME, &ts);

#ifdef FTIME_STRUCT
	time->seconds = (int32_t) ts.tv_sec;
	time->fraction = (uint32_t) ((((uint64_t) ts.tv_nsec) << 32ULL) / 1000000000UL);
#else
	*time = (((int64_t) ts.tv_sec) << 32ULL) |
	         ((((int64_t) ts.tv_nsec) << 32ULL) / 1000000000UL);
#endif
}

/* sys_gettime -- Get the system time in seconds/nanoseconds. */

void sys_gettime (Time_t *time)
{
	struct timespec	ts;

	clock_gettime (CLOCK_REALTIME, &ts);
	time->seconds = (int32_t) ts.tv_sec;
	time->nanos = ts.tv_nsec;
}

/* time2ftime -- Convert a timestamp in seconds/nanoseconds to seconds/
		 fractions. */

void time2ftime (Time_t *t, FTime_t *ft)
{
#ifdef FTIME_STRUCT
	ft->seconds = t->seconds;
	ft->fraction = (uint32_t) ((((uint64_t) t->nanos) << 32) / 1000000000ULL);
#else
	*ft = (((int64_t) t->seconds) << 32ULL) |
	       ((((int64_t) t->nanos) << 32ULL) / 1000000000ULL);
#endif
}

/* ftime2time -- Convert a timestamp in seconds/fractions to seconds/
		 nanoseconds. */

void ftime2time (FTime_t *ft, Time_t *t)
{
#ifdef FTIME_STRUCT
	t->seconds = ft->seconds;
	t->nanos = (((uint64_t) ft->fraction) * 1000000000ULL) >> 32;
#else
	t->seconds = *ft >> 32ULL;
	t->nanos = ((*ft & 0xffffffffULL) * 1000000000ULL) >> 32;
#endif
}

#ifdef FTIME_STRUCT
#define FTIME_SET(t,s,ns)	(t).seconds = (s); (t).fraction = ((uint64_t) (ns) << 32) / 1000000000;
#define FTIME_SEC(t)		(t).seconds
#define FTIME_FRACT(t)		(t).fraction
#else
#define FTIME_SET(t,s,ns)	t = ((int64_t) s << 32) | (((int64_t) (ns) << 32) / 1000000000);
#define FTIME_SEC(t)		(int32_t) (t >> 32)
#define FTIME_FRACT(t)		(uint32_t) (t & 0xffffffffU)
#endif

void dump_ftime (FTime_t *tp, unsigned ns)
{
	FTime_t	nt;
	Time_t	t;

#ifndef FTIME_STRUCT
	printf ("%9u %016llx | %llu (in) = ", ns, *tp, *tp);
#endif
	printf ("%d:%010us (in) -> ", FTIME_SEC (*tp), FTIME_FRACT (*tp));
	ftime2time (tp, &t);
	printf ("%us,%uns -> ", t.seconds, t.nanos);
	time2ftime (&t, &nt);
	printf ("%d:%010us (out)\r\n", FTIME_SEC (nt), FTIME_FRACT (nt));
}

int main (void)
{
	FTime_t	t;

	FTIME_SET (t, 3, 0); dump_ftime (&t, 0);
	FTIME_SET (t, 3, 1); dump_ftime (&t, 1);
	FTIME_SET (t, 3, 2); dump_ftime (&t, 2);
	FTIME_SET (t, 3, 100); dump_ftime (&t, 100);
	FTIME_SET (t, 3, 100000); dump_ftime (&t, 1000000);
	FTIME_SET (t, 3, 999000000); dump_ftime (&t, 999000000);
	FTIME_SET (t, 3, 999999999); dump_ftime (&t, 999999999);
}

