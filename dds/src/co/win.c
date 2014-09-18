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

/* win.c -- Extra functions that are defined in Unix systems for which there is
            no equivalent in the Windows environment. */


#include <time.h>
#include "win.h"
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <iphlpapi.h>

#ifdef _WIN32

void usleep (long usec)
{
	LARGE_INTEGER	lFrequency;
	LARGE_INTEGER	lEndTime;
	LARGE_INTEGER	lCurTime;

	if (usec >= 1000) {
		Sleep (usec / 1000);	/* Don't bother with us accuracy. */
		return;
	}
	QueryPerformanceFrequency (&lFrequency);
	if (lFrequency.QuadPart) {
		QueryPerformanceCounter (&lEndTime);
		lEndTime.QuadPart += (LONGLONG) usec * lFrequency.QuadPart / 1000000;
		do {
			QueryPerformanceCounter (&lCurTime);
			Sleep(0);
		}
		while (lCurTime.QuadPart < lEndTime.QuadPart);
	}
}

static LARGE_INTEGER get_filetime_offset (void)
{
	SYSTEMTIME	s;
	FILETIME	f;
	LARGE_INTEGER	t;

	s.wYear = 1970;
	s.wMonth = 1;
	s.wDay = 1;
	s.wHour = 0;
	s.wMinute = 0;
	s.wSecond = 0;
	s.wMilliseconds = 0;
	SystemTimeToFileTime(&s, &f);
	t.QuadPart = f.dwHighDateTime;
	t.QuadPart <<= 32;
	t.QuadPart |= f.dwLowDateTime;
	return (t);
}

int clock_gettime (int X, struct timespec *tv)
{
	LARGE_INTEGER		t;
	FILETIME		f;
	double			nanoseconds;
	static LARGE_INTEGER	offset;
	static double		frequencyToNanoseconds;
	static int		initialized = 0;
	static BOOL		usePerformanceCounter = 0;

	if (!initialized) {
		LARGE_INTEGER	performanceFrequency;
		initialized = 1;
		usePerformanceCounter = QueryPerformanceFrequency (&performanceFrequency);
		if (usePerformanceCounter) {
			QueryPerformanceCounter (&offset);
			frequencyToNanoseconds = (double) performanceFrequency.QuadPart / 1000000000.;
		}
		else {
			offset = get_filetime_offset ();
			frequencyToNanoseconds = 10000.;
		}
	}
	if (usePerformanceCounter)
		QueryPerformanceCounter (&t);
	else {
		GetSystemTimeAsFileTime (&f);
		t.QuadPart = f.dwHighDateTime;
		t.QuadPart <<= 32;
		t.QuadPart |= f.dwLowDateTime;
	}

	t.QuadPart -= offset.QuadPart;
	nanoseconds = (double) t.QuadPart / frequencyToNanoseconds;
	t.QuadPart = (LONGLONG) nanoseconds;
	tv->tv_sec = (long) (t.QuadPart / 1000000000);
	tv->tv_nsec = t.QuadPart % 1000000000;
	return (0);
}

int gettimeofday (struct timeval *tv, struct timezone *tz)
{
	struct timespec ts;
	static int	tzflag;
 
	if (tv) {
		clock_gettime (0, &ts);
		tv->tv_sec = (long) ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
	}
	if (tz) {
		if (!tzflag) {
			_tzset ();
			tzflag++;
		}
		_get_timezone (&tz->tz_minuteswest);
		tz->tz_minuteswest /= 60;
		_get_daylight (&tz->tz_dsttime);
	}
	return (0);
}

#endif /* _WIN32 */
