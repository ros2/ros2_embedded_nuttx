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

/* sys.h -- System-specific functions that are needed by RTPS for constructing
            things like locators, GUIDs, etc. */

#ifndef __sys_h_
#define	__sys_h_

#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
#if defined (_M_X64) || defined (_M_IA64)
#define __WORDSIZE 64
#else
#define __WORDSIZE 32
#endif
#endif
#ifdef USE_BYTESWAP
#ifndef _WIN32
#include <byteswap.h>	/* !!non-standard GNU extension!! */
#endif
#endif

/*#define FTIME_STRUCT	** Use a struct to hold a fractional time. */

/* Some macros for 64-bit numbers on 32-bit systems: */

#if defined (__WORDSIZE) && (__WORDSIZE == 64)

#define	WORDSIZE	64
#define	WORDALIGN	8

/* Unsigned 32-bit statistics counter. */
typedef unsigned ULONG;

/* Unsigned 64-bit declarations, addition, subtraction, increment & decrement:*/
#define	STATIC_ULLONG(name)	static unsigned long name
#define	ULLONG(name)		unsigned long name
#define CLR_ULLONG(name)	name = 0
#define	ADD_ULLONG(name,n)	name += n
#define	SUB_ULLONG(name,n)	name -= n
#define	INC_ULLONG(name)	name++
#define	DEC_ULLONG(name)	name--
#define	ADD2_ULLONG(name,n)	name += n

#define	DBG_PRINT_ULLONG(name)	dbg_printf ("%lu", name)

/* Signed 64-bit declarations, addition, subtraction, increment & decrement:*/
#define	STATIC_LLONG(name)	static long name
#define	LLONG(name)		long name
#define CLR_LLONG(name)		name = 0
#define	ADD_LLONG(name,n)	name += n
#define	SUB_LLONG(name,n)	name -= n
#define	INC_LLONG(name)		name++
#define	DEC_LLONG(name)		name--
#define ADD2_LLONG(name,n)	name += n
#define	GT_ULLONG(n1, n2)	(n1 > n2)
#define	ASS_ULLONG(n1,n2)	n1 = n2

#define	DBG_PRINT_LLONG(name)	dbg_printf ("%ld", name)

#else /* Try to avoid long long type -- too compiler-specific. */

#define	WORDSIZE	32
#define	WORDALIGN	4

/* Unsigned 32-bit statistics counter. */
typedef unsigned long ULONG;

/* Unsigned 64-bit declarations, addition, subtraction, increment & decrement:*/
#define	STATIC_ULLONG(name)	static unsigned long name##_h; \
				static unsigned long name##_l
#define	ULLONG(name)		unsigned long name##_h; \
				unsigned long name##_l
#define CLR_ULLONG(name)	name##_l = 0; name##_h = 0
#define ADD_ULLONG(name,n)	name##_l += n; if (name##_l < n) name##_h++
#define SUB_ULLONG(name,n)	if (name##_l < n) name##_h--; name##_l -= n
#define	INC_ULLONG(name)	name##_l++; if (!name##_l) name##_h++
#define	DEC_ULLONG(name)	if (!name##_l) name##_h--; name##_l--
#define ADD2_ULLONG(name,n)	name##_l += n##_l; name##_h += n##_h; if (name##_l < n##_l) name##_h++
#define	GT_ULLONG(n1,n2)	(n1##_h > n2##_h || (n1##_h==n2##_h && n1##_l > n2##_l))
#define	ASS_ULLONG(n1,n2)	n1##_h = n2##_h; n1##_l = n2##_l

#define	DBG_PRINT_ULLONG(name)	dbg_print_dulong (name##_h, name##_l, 10)

/* Signed 64-bit declarations, addition, subtraction, increment & decrement:*/
#define	STATIC_LLONG(name)	static long name##_h; \
				static unsigned long name##_l
#define	LLONG(name)		long name##_h; \
				unsigned long name##_l
#define CLR_LLONG		CLR_ULLONG
#define ADD_LLONG		ADD_ULLONG
#define SUB_LLONG		SUB_ULLONG
#define INC_LLONG		INC_ULLONG
#define DEC_LLONG		DEC_ULLONG
#define ADD2_LLONG		ADD2_LLONG

#define	DBG_PRINT_LLONG(name)	dbg_print_dulong (name##_h, name##_l, 10)

#endif /* !__WORDSIZE == 64 */

/* Copy while swapping functions: */
#ifdef USE_BYTESWAP
#ifndef _WIN32
#define	memcswap16(d,s)	STMT_BEG uint16_t u = bswap_16 (*(uint16_t *)(s)); memcpy16 (d, &u); STMT_END
#define	memcswap32(d,s)	STMT_BEG uint32_t u = bswap_32 (*(uint32_t *)(s)); memcpy32 (d, &u); STMT_END
#define	memcswap64(d,s)	STMT_BEG uint64_t u = bswap_64 (*(uint64_t *)(s)); memcpy64 (d, &u); STMT_END
#else
#define	memcswap16(d,s)	STMT_BEG uint16_t u = _byteswap_ushort (*(uint16_t *)(s)); memcpy16 (d, &u); } STMT_END
#define	memcswap32(d,s)	STMT_BEG uint32_t u = _byteswap_ulong  (*(uint32_t *)(s)); memcpy32 (d, &u); } STMT_END
#define	memcswap64(d,s)	STMT_BEG uint64_t u = _byteswap_uint64 (*(uint64_t *)(s)); memcpy64 (d, &u); } STMT_END
#endif
#else
#define memcswap16(d,s)	STMT_BEG ((char *)d)[0]=((char *)s)[1]; ((char *)d)[1]=((char *)s)[0]; STMT_END
#define memcswap32(d,s)	STMT_BEG ((char *)d)[0]=((char *)s)[3]; ((char *)d)[1]=((char *)s)[2]; \
				 ((char *)d)[2]=((char *)s)[1]; ((char *)d)[3]=((char *)s)[0]; STMT_END
#define memcswap64(d,s)	STMT_BEG ((char *)d)[0]=((char *)s)[7]; ((char *)d)[1]=((char *)s)[6]; \
				 ((char *)d)[2]=((char *)s)[5]; ((char *)d)[3]=((char *)s)[4]; \
				 ((char *)d)[4]=((char *)s)[3]; ((char *)d)[5]=((char *)s)[2]; \
				 ((char *)d)[6]=((char *)s)[1]; ((char *)d)[7]=((char *)s)[0]; STMT_END
#endif

/* Copy while not swapping functions: */
#ifdef ALIGNED_WORDS
#define memcpy16(d,s)	*(uint16_t *)(d) = *(uint16_t *)(s)
#define memcpy32(d,s)	*(uint32_t *)(d) = *(uint32_t *)(s)
#define memcpy64(d,s)	*(uint64_t *)(d) = *(uint64_t *)(s)
#else
#define memcpy16(d,s)	memcpy (d,s,2)
#define memcpy32(d,s)	memcpy (d,s,4)
#define memcpy64(d,s)	memcpy (d,s,8)
#endif

#ifdef _WIN32
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#elif defined (__APPLE__) || defined (NetBSD) || defined (__FreeBSD__) || defined (__OpenBSD__)
#ifdef __FreeBSD__
#include <sys/param.h>
#endif
#include <machine/endian.h>
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN    BIG_ENDIAN
#define __BYTE_ORDER    BYTE_ORDER
#elif defined (NUTTX_RTOS)
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#define CLOCK_MONOTONIC	0

#else /* Linux */
#define NETLINK_SUPPORT
#include <endian.h>
#endif
#define	ENDIAN_BIG	0
#define	ENDIAN_LITTLE	1

#ifndef __BYTE_ORDER
# error __BYTE_ORDER not defined!
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define	ENDIAN_CPU	ENDIAN_LITTLE
#elif __BYTE_ORDER == __BIG_ENDIAN
#define	ENDIAN_CPU	ENDIAN_BIG
#else
#define	ENDIAN_CPU	0
#endif

int sys_init (void);

/* System initialisation as needed on some (not all) platforms. */

void sys_final (void);

/* System finalisation. */

char *sys_username (char *buf, size_t length);

/* User name. */

char *sys_hostname (char *buf, size_t length);

/* Fully qualified host name. */

char *sys_osname (char *buf, size_t length);

/* Operating System name. */

char *sys_osrelease (char *buf, size_t length);

/* Operating System release. */

char *sys_getcwd (char *buf, size_t length);

/* Current work directory. */

unsigned sys_uid (void);

/* User id. */

unsigned sys_hostid (void);

/* Host id. */

unsigned sys_pid (void);

/* Process id. */

#define	OWN_IPV4_SIZE		8	/* Size of an IPv4 address record. */
#define	OWN_IPV4_SCOPE_OFS	4	/* Offset for scope of address. */

typedef enum {
	UNKNOWN_SCOPE,	/* Scope type not set. */
	NODE_LOCAL,	/* For Loopback. */
	LINK_LOCAL,	/* On link only. */
	SITE_LOCAL,	/* Site-local. */
	ORG_LOCAL,	/* Organisation-local. */
	GLOBAL_SCOPE	/* Globally accessible. */
} Scope_t;

Scope_t sys_ipv4_scope (const unsigned char *addr);

/* Retrieve the scope of an IPv4 address. */

const char *sys_scope_str (Scope_t scope);

/* Return a string representing a scope. */

unsigned sys_own_ipv4_addr (unsigned char *addr,
			    size_t        max,
			    Scope_t       min_scope,
			    Scope_t       max_scope);

/* Get a list of local IPv4 addresses in addr [].  The total size of the address
   array (in bytes) should be specified in max.  The function returns the number
   of addresses stored in the array, each entry occupying OWN_IPV4_SIZE bytes,
   a 4-byte IP address followed by the scope type (4 bytes) of the address.
   The minimum and maximum scopes will be used for restricting the set of usable
   addresses. */

Scope_t sys_ipv6_scope (const unsigned char *addr);

/* Retrieve the scope of an IPv6 address. */

#define	OWN_IPV6_SIZE		24	/* Size of an IPv6 address record. */ 
#define	OWN_IPV6_SCOPE_ID_OFS	16	/* Offset for scope_id of address. */
#define	OWN_IPV6_SCOPE_OFS	20	/* Offset for scope of address. */

unsigned sys_own_ipv6_addr (unsigned char *addr,
			    size_t        max,
			    Scope_t       min_scope,
			    Scope_t       max_scope);

/* Get a list of local IPv6 addresses in addr [].  The total size of the address
   array (in bytes) should be specified in max.  The function returns the number
   of addresses stored in the array, with each address occupying OWN_IPV6_SIZE
   bytes in the address buffer (16-bytes IPv6 address, 4-byte scope type and
   4-byte scope_id. */

const char *sys_getenv (const char *var_name);

/* Get the value of the environment variable with the given name.  If it doesn't
   exist, the NULL value will be returned. */

#ifdef __APPLE__

#include <time.h>

#define CLOCK_MONOTONIC	0
#define CLOCK_REALTIME	1

int clock_gettime (int clk_id, struct timespec *t);

/* clock_gettime is not implemented on OSX, simulate it using gettimeofday(). */

#endif

#define	TMR_UNIT_MS	10		/* Timer tick value in milliseconds. */
#define	TICKS_PER_SEC	(1000/TMR_UNIT_MS)	       /* # of ticks/second. */

typedef unsigned long Ticks_t;

Ticks_t sys_getticks (void);

/* Get the system time in 10ms ticks since startup. */

#define sys_ticksdiff(old, new)	((new >= old) ? (new - old) : (~0UL - old + new + 1UL))

/* Get the difference between two tick values as a number of ticks. */

#ifndef FTIME_STRUCT

typedef int64_t FTime_t;	/* Fractionalized time can be used directly in
				   time calculations. */

#define FTIME_SET(ft,s,ns) (ft) = ((int64_t) (s) << 32) | (((int64_t) (ns) << 32) / 1000000000)
#define	FTIME_SETF(ft,s,f) (ft) = ((int64_t) (s) << 32) | (f)
#define	FTIME_SETT(ft,t)   (ft) = (((int64_t) (t) / TICKS_PER_SEC) << 32) | (((uint64_t) TMR_UNIT_MS << 32) / 1000)
#define	FTIME_CLR(ft)	   (ft) = 0ULL;

#define FTIME_SEC(t)	(int32_t) ((t) >> 32)
#define FTIME_FRACT(t)	(uint32_t) ((t) & 0xffffffffU)
#define	FTIME_NSEC(t)	(unsigned) ((((t) & 0xffffffff) * 1000000000) >> 32)

#define	FTIME_ADD(t,d)	(t) += (d)
#define	FTIME_SUB(t,d)	(t) -= (d)
#define	FTIME_LT(t1,t2)	(t1) < (t2)
#define	FTIME_EQ(t1,t2)	(t1) == (t2)
#define	FTIME_GT(t1,t2)	(t1) > (t2)
#define	FTIME_ZERO(t)	(!t)
#define	FTIME_TICKS(t)	(Ticks_t) ((t) / (0x100000000ULL / TICKS_PER_SEC))

#else /* FTIME_STRUCT		** Fractionalized time, not usable directly. */

typedef struct ftime_st {
	int32_t		seconds;	/* Time in seconds. */
	uint32_t	fraction;	/* Time in seconds / 2^32 */
} FTime_t;

#define FTIME_SET(ft,s,ns) (ft).seconds = (s); (ft).fraction = ((uint64_t) (ns) << 32) / 1000000000
#define	FTIME_SETF(ft,s,f) (ft).seconds = (s); (ft).fraction = (f)
#define	FTIME_SETT(ft,t)   (ft).seconds = (t) * TICKS_PER_SEC; (ft).fraction = ((uint64_t) TMR_UNIT_MS << 32) / 1000
#define	FTIME_CLR(ft)	   (ft).seconds = 0; (ft).fraction = 0U;

#define FTIME_SEC(t)	(t).seconds
#define FTIME_FRACT(t)	(t).fraction
#define	FTIME_NSEC(t)	(unsigned) (((uint64_t)((t).fraction) * 1000000000) >> 32)

#define	FTIME_ADD(t,d)	(t).fraction += (d).fraction; \
			(t).seconds += (d).seconds;   \
			if ((t).fraction < (d).fraction) (t).seconds++
#define	FTIME_SUB(t,d)	if ((t).fraction < (d).fraction) (t).seconds--; \
			(t).seconds -= (d).seconds; \
			(t).fraction -= (d).fraction
#define	FTIME_LT(t1,t2)	((t1).seconds < (t2).seconds || \
			 ((t1).seconds == (t2).seconds && \
			  (t1).fraction < (t2).fraction))
#define	FTIME_EQ(t1,t2)	((t1).seconds == (t2).seconds && \
			 (t1).fraction == (t2).fraction)
#define	FTIME_GT(t1,t2)	((t1).seconds > (t2).seconds || \
			 ((t1).seconds == (t2).seconds && \
			  (t1).fraction > (t2).fraction))
#define	FTIME_ZERO(t)	(!(t).seconds && !(t).fraction)
#define	FTIME_TICKS(t)	((t).seconds * TICKS_PER_SEC + \
			 (t).fraction / (0xffffffffU / TICKS_PER_SEC))

#endif

extern FTime_t	sys_startup_time;	/* Time at startup. */
extern Ticks_t	sys_ticks_last;		/* Last retrieved system ticks value.
					   Since the timer handler as well as
					   several other sources update this
					   regularly it should be quite accurate
					   and can be used for low-cost ageing
					   purposes. */

typedef struct time_st {
	int32_t		seconds;	/* Time in seconds. */
	uint32_t	nanos;		/* Time in nanoseconds. */
} Time_t;

#define	TIME_ZERO	{ 0, 0 }
#define	TIME_INVALID	{ -1, ~0 }
#define	TIME_INFINITE	{ 0x7fffffff, ~0 }

#define	TIME_LT(t1,t2)	((t1).seconds < (t2).seconds || \
		((t1).seconds == (t2).seconds && (t1).nanos < (t2).nanos))

/* Compare two timestamps for the '<' condition. */

#define	TIME_GT(t1,t2)	((t1).seconds > (t2).seconds || \
		((t1).seconds == (t2).seconds && (t1).nanos > (t2).nanos))

/* Compare two timestamps for the '>' condition. */

void sys_getftime (FTime_t *time);

/* Get the system time in seconds/fractions. */

void sys_gettime (Time_t *time);

/* Get the system time in seconds/nanoseconds. */

void time2ftime (Time_t *t, FTime_t *ft);

/* Convert a timestamp in seconds/nanoseconds to seconds/fractions. */

void ftime2time (FTime_t *ft, Time_t *t);

/* Convert a timestamp in seconds/fractions to seconds/nanoseconds. */

#endif /* !__sys_h_ */

