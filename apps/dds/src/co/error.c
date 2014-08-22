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

/* error.c -- Handles various grades of error handling. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef _WIN32
#include <time.h>
#include <io.h>
#else
#include <unistd.h>
#ifndef NO_SYSLOG
#include <syslog.h>
#include <libgen.h>
#endif
#endif
#include "sock.h"
#include "sys.h"
#include "skiplist.h"
#include "dds.h"
#include "log.h"
#include "thread.h"
#include "ctrace.h"
#include "error.h"
#ifdef DDS_DEBUG
#include "debug.h"
#endif

#ifdef LOG_ALWAYS
#define	LOG_STDIO	ACT_PRINT_STDIO |
#else
#define	LOG_STDIO
#endif

#ifdef LOG_FILE
#define	LOGF	ACT_LOG
#else
#define	LOGF	0
#endif

#ifndef LOG_ACTION
#define	LOG_ACTION	(LOG_STDIO LOGF)
#endif
#ifndef DEBUG_ACTION
#define	DEBUG_ACTION	(ACT_PRINT_STDIO | LOGF)
#endif
#ifndef WARN_ACTION
#define	WARN_ACTION	(ACT_PRINT_ERR | LOGF)
#endif
#ifndef ERROR_ACTION
#define	ERROR_ACTION	(ACT_PRINT_ERR | LOGF)
#endif
#ifndef FATAL_ACTION
#define	FATAL_ACTION	(ACT_PRINT_ERR | LOGF | ACT_EXIT)
#endif

#ifdef _WIN32
#define BASE_LOG_NAME	"tdds.log"
#else
#ifdef ANDROID
#define	BASE_LOG_NAME	"/mnt/sdcard/tdds_log_"
#include <android/log.h>
#else
#define	BASE_LOG_NAME	".tdds_log_"
#endif
#endif

/*#define USE_ABORT	** Set this to force an abort() instead of exit(). */

#ifdef JNI
#ifndef USE_ABORT
#define	USE_ABORT
#endif
#endif

typedef struct log_action_st {
	unsigned	id;
	unsigned	level;
	unsigned	actions;
} LogAction_t;

static char logname [256];
static Skiplist_t log_act_list;
static unsigned cur_actions [] = {
	LOG_ACTION, DEBUG_ACTION, WARN_ACTION, ERROR_ACTION, FATAL_ACTION
};
static HANDLE handle;
#ifdef _WIN32
static SOCKET osock;
#endif
static const char *indent_str = "\t";

int prog_nargs;
char **prog_args;
unsigned log_debug_count;

void err_prog_args (int *argc, char *argv [])
{
	prog_nargs = *argc;
	prog_args = argv;
}

/* log_key_cmp -- Compare a key to the key contents of a node. */

static int log_key_cmp (const void *np, const void *data)
{
	const LogAction_t	*p = (const LogAction_t *) np;
	const unsigned		*kp = (const unsigned *) data;

	if (p->id != kp [0])
		return ((int) p->id - (int) kp [0]);
	else
		return ((int) p->level - (int) kp [1]);
}

/* log_actions_set -- Set/replace all actions for the given logging source/
		      level. */

void log_actions_set (unsigned id, unsigned level, unsigned actions)
{
	LogAction_t	*p;
	unsigned	key [2];
	int		is_new;

	if (!sl_initialized (log_act_list))
		sl_init (&log_act_list, sizeof (LogAction_t));

	/* Lookup action node. */
	key [0] = id;
	key [1] = level;
	p = sl_insert (&log_act_list, key, &is_new, log_key_cmp);
	if (p) {
		if (is_new) {
			p->id = id;
			p->level = level;
		}
		p->actions = actions;
	}
}

/* log_actions_add -- Add actions to the given logging source/level. */

void log_actions_add (unsigned id, unsigned level, unsigned acts)
{
	LogAction_t	*p;
	unsigned	key [2];
	int		is_new;

	if (!sl_initialized (log_act_list))
		sl_init (&log_act_list, sizeof (LogAction_t));

	/* Lookup action node. */
	key [0] = id;
	key [1] = level;
	p = sl_insert (&log_act_list, key, &is_new, log_key_cmp);
	if (p) {
		if (is_new) {
			p->id = id;
			p->level = level;
			p->actions = cur_actions [EL_LOG];
		}
		p->actions |= acts;
	}
}

/* log_actions_remove -- Remove actions from the given logging source/level. */

void log_actions_remove (unsigned id, unsigned level, unsigned acts)
{
	LogAction_t	*p;
	unsigned	key [2];
	int		is_new;

	if (!sl_initialized (log_act_list))
		sl_init (&log_act_list, sizeof (LogAction_t));

	/* Lookup action node. */
	key [0] = id;
	key [1] = level;
	p = sl_insert (&log_act_list, key, &is_new, log_key_cmp);
	if (p) {
		if (is_new) {
			p->id = id;
			p->level = level;
			p->actions = cur_actions [EL_LOG];
		}
		p->actions &= ~acts;
	}
}

/* log_actions_default -- Do default actions given logging source/level. */

void log_actions_default (unsigned id, unsigned level)
{
	unsigned	key [2];

	if (!sl_initialized (log_act_list))
		sl_init (&log_act_list, sizeof (LogAction_t));

	/* Delete action node. */
	key [0] = id;
	key [1] = level;
	sl_delete (&log_act_list, key, log_key_cmp);
}

/* log_logged -- Return the logging status of the given id/level. */

unsigned log_logged (unsigned id, unsigned level)
{
	unsigned	acts, key [2];
	LogAction_t	*p;

	acts = cur_actions [EL_LOG];
#ifdef LOG_USER_ALWAYS
	if (id == USER_ID)
		acts |= ACT_PRINT_STDIO;
#endif
	if (sl_initialized (log_act_list)) {
		key [0] = id;
		key [1] = level;
		p = sl_search (&log_act_list, key, log_key_cmp);
		if (p)
			acts = p->actions;
	}
	return (acts);
}

/* err_actions_set -- Set/replace all actions for the given error level. */

void err_actions_set (ErrLevel_t level, unsigned acts)
{
	if (level <= EL_FATAL)
		cur_actions [level] = acts;
}

/* err_actions_add -- Add actions to the given error level. */

void err_actions_add (ErrLevel_t level, unsigned acts)
{
	if (level <= EL_FATAL)
		cur_actions [level] |= acts;
}

/* err_actions_remove -- Remove actions from the given error level. */

void err_actions_remove (ErrLevel_t level, unsigned acts)
{
	if (level <= EL_FATAL && acts)
		cur_actions [level] &= ~acts;
}

#ifdef _WIN32
#define snprintf	sprintf_s
#define openf(f,name,m)	fopen_s(&f,name,m) == 0
#define NO_SYSLOG
#else
#define openf(f,name,m)	((f = fopen (name, m)) != NULL)
#endif

#ifndef NO_SYSLOG
static int errlevel_to_syslog [] = {
	/* EL_LOG -> LOG_NOTICE: normal, but significant, condition */
	LOG_NOTICE,
	/* EL_DEBUG -> LOG_DEBUG: debug-level message */
	LOG_DEBUG,
	/* EL_WARNING -> LOG_WARNING: warning conditions */
	LOG_WARNING,
	/* EL_ERROR -> LOG_ERR: error conditions */
	LOG_ERR,
	/* EL_FATAL -> LOG_CRIT:  critical conditions */
	LOG_CRIT
};
#endif

static void log_dir_change (Config_t c)
{
	const char	*dir;

	ARG_NOT_USED (c)

	if ((dir = config_get_string (DC_LogDir, NULL)) != NULL)
		snprintf (logname, sizeof (logname), "%s/%s%u", dir, BASE_LOG_NAME, sys_pid ());
	else
		snprintf (logname, sizeof (logname), "%s%u", BASE_LOG_NAME, sys_pid ());
}

static void do_actions (ErrLevel_t level, 
			unsigned   act_flags,
			const char *name,
			const char *args,
			int        nl)
{
	FILE		*f;
	struct timeval	tv;
#ifdef _WIN32
	struct tm	tm_data, *tm = &tm_data;
#else
	struct tm	*tm;
#endif
	char		tmbuf [40];
	int		n;
#ifndef NO_SYSLOG
	static int	syslog_init_needed = 1;
#endif

# if 0
#ifdef CTRACE_USED
	if (!ctrace_saves) {
		if (name)
			ctrc_printf (INFO_ID, 0, "%s: %s", name, args);
		else
			ctrc_printf (INFO_ID, 0, "%s", args);
	}
#endif
# endif
	if ((act_flags & ACT_PRINT_STDIO) != 0) {
		if (name && (act_flags & ACT_PREF_STDIO) != 0) {
			if (nl)
				printf ("\r\n");
			if (prog_nargs)
				printf ("%s: ", prog_args [0]);
			printf ("%s: ", name);
		}
		printf ("%s", args);
		if (nl)
			printf ("\r\n");
	}
#if defined (DDS_DEBUG) && !defined (CDR_ONLY)
	if (level != EL_DEBUG && log_debug_count)
		debug_log (args, nl);
#endif
	if ((act_flags & ACT_PRINT_ERR) != 0) {
#ifdef ANDROID
		__android_log_print(ANDROID_LOG_INFO, "QeoDDS", "%s: %s", name, args);
#else
		if (name && (act_flags & ACT_PREF_ERR) != 0) {
			if (prog_nargs)
				fprintf (stderr, "%s: ", prog_args [0]);
			fprintf (stderr, "%s: ", name);
		}
		fprintf (stderr, "%s", args);
		if (nl)
			fprintf (stderr, "\r\n");
#endif
	}
#ifndef NO_SYSLOG
	if ((act_flags & ACT_PRINT_SYS) != 0) {
		if (syslog_init_needed) {
			if (prog_nargs) {
				openlog (prog_args[0], LOG_CONS, LOG_DAEMON);
			}
			else
				openlog ("_dds", LOG_PID|LOG_CONS, LOG_DAEMON); 
			syslog_init_needed = 0;
		}

		syslog (errlevel_to_syslog [level], "%s", args);
	}
#endif
	if ((act_flags & ACT_PRINT_FILE) != 0) {
#ifdef _WIN32
		WriteFile (handle, args, strlen (args), &n, NULL);
#else
		n = write (handle, args, strlen (args));
#endif
		if (nl)
#ifdef _WIN32
			WriteFile (handle, "\r\n", 2, &n, NULL);
#else
			n = write (handle, "\r\n", 2);
#endif
		(void) n;
	}
#ifdef _WIN32
	if ((act_flags & ACT_PRINT_SK) != 0) {
		n = send (osock, args, strlen (args), 0);
		if (nl)
			n = send (osock, "\r\n", 2, 0);
		(void) n;
	}
#endif
	if ((act_flags & ACT_LOG) != 0) {
		if (logname [0] == '\0')
			config_notify (DC_LogDir, log_dir_change);

		if (openf (f, logname, "a")) {
			gettimeofday (&tv, NULL);
#ifdef _WIN32
			_localtime32_s (&tm_data, &tv.tv_sec);
#else
			tm = localtime (&tv.tv_sec);
			if (!tm) {
#ifdef LOG_DATE
				fprintf (f, "\?\?\?\?-\?\?\?-\?\? ");
#endif
				fprintf (f, "\?\?:\?\?:\?\?.\?\?\? ");
			}
			else
#endif
			{
				tmbuf [0] = '\0';
#ifdef LOG_DATE
				strftime (tmbuf, sizeof (tmbuf), "%Y-%m-%d ", tm);
#endif
				strftime (&tmbuf [strlen (tmbuf)],
						sizeof (tmbuf) - strlen (tmbuf),
						"%H:%M:%S.", tm);
				snprintf (&tmbuf [strlen (tmbuf)],
					  sizeof (tmbuf) - strlen (tmbuf),
					  "%03lu ", (unsigned long) tv.tv_usec / 1000);
				fprintf (f, "%s", tmbuf);
			}
			if (name) {
				if (prog_nargs)
					fprintf (f, "%s: ", prog_args [0]);
				fprintf (f, "%s: ", name);
			}
			fprintf (f, "%s", args);
			fprintf (f, "\r\n");
			fclose (f);
		}
	}
	if ((act_flags & ACT_EXIT) != 0) {
		printf ("Exiting program!\r\n");
#ifdef USE_ABORT
		abort ();
#else
		exit (1);
#endif
	}
}

void dbg_redirect (HANDLE outf)
{
	if (outf) {
		handle = outf;
		cur_actions [EL_DEBUG] |= ACT_PRINT_FILE;
		cur_actions [EL_DEBUG] &= ~ACT_PRINT_STDIO;
	}
	else {
		handle = 0;
		cur_actions [EL_DEBUG] &= ~ACT_PRINT_FILE;
		cur_actions [EL_DEBUG] |= ACT_PRINT_STDIO;
	}
}

#ifdef _WIN32

void dbg_redirect_sk (SOCKET out_sk)
{
	if (out_sk) {
		osock = out_sk;
		cur_actions [EL_DEBUG] |= ACT_PRINT_SK;
		cur_actions [EL_DEBUG] &= ~ACT_PRINT_STDIO;
	}
	else {
		osock = 0;
		cur_actions [EL_DEBUG] &= ~ACT_PRINT_SK;
		cur_actions [EL_DEBUG] |= ACT_PRINT_STDIO;
	}
}
#endif

#define	MAX_BUFFER	1024

static char	log_buffer [MAX_BUFFER];
static char	dbg_buffer [MAX_BUFFER];
static unsigned	log_length;
static unsigned	dbg_length;
static lock_t	log_lock;
static lock_t	dbg_lock;
#ifndef CDR_ONLY
static cond_t   log_cond;
static cond_t   dbg_cond;
static thread_t log_thread;
static thread_t dbg_thread;
#endif

static void buffer_output (ErrLevel_t level,
			   unsigned   acts,
			   char       *sbuf,
			   char       *dbuf,
			   unsigned   *lp)
{
	char	*cp, *start;

	for (cp = sbuf, start = sbuf;; )
		if (*cp == '\r') {
			*cp++ = '\0';
			memcpy (&dbuf [*lp], start, cp - start + 1);
			*lp += cp - start;
 			do_actions (level, acts, NULL, dbuf, 1);
			*lp = 0;
			dbuf [0] = '\0';
			if (*cp == '\n')
				cp++;
			start = cp;
		}
		else if (!*cp) {
			if (cp != start) {
				if (*lp + cp - start + 1 > MAX_BUFFER) {
					do_actions (level, acts, NULL, dbuf, 1);
					*lp = 0;
					dbuf [0] = '\0';
				}
				memcpy (&dbuf [*lp], start, cp - start + 1);
				*lp += cp - start;
			}
			break;
		}
		else
			cp++;
}

void dbg_printf (const char *fmt, ...)
{
	va_list		arg;
	char		sbuf [256];
#ifndef CDR_ONLY
	static int	init_needed = 1;

	if (init_needed) {
		lock_init_r (dbg_lock, "dbg");
		cond_init (dbg_cond);
		init_needed = 0;
	}
#endif
	va_start (arg, fmt);
	vsnprintf (sbuf, sizeof (sbuf), fmt, arg);
	va_end (arg);

#ifndef CDR_ONLY
	lock_takef (dbg_lock);

	/* Verify that the buffer is not in use by another thread. If it is, wait
	 * until it signals it is done with it. */
	while (dbg_thread && dbg_thread != thread_id ())
		cond_wait (dbg_cond, dbg_lock); 

	dbg_thread = thread_id ();
#endif
	buffer_output (EL_DEBUG, cur_actions [EL_DEBUG], sbuf, dbg_buffer, &dbg_length);
#ifndef CDR_ONLY
	if (dbg_length == 0) {
		dbg_thread = 0;
		cond_signal_all (dbg_cond);
	}
	lock_releasef (dbg_lock);
#endif
}

#define	RC_BUFSIZE	128

static const char *region_chunk_str (const void *p,
			             unsigned   length,
			             int        show_ofs,
			             const void *sp)
{
	char			ascii [17], *bp;
	static char		buf [RC_BUFSIZE];
	unsigned		i, left;
	const unsigned char	*dp = (const unsigned char *) p;
	unsigned char		c;

	bp = buf;
	left = RC_BUFSIZE;
	if (show_ofs)
		if (sp)
			snprintf (bp, left, "  %4ld: ", (long) (dp - (const unsigned char *) sp));
		else
			snprintf (bp, left, "  %p: ", p);
	else {
		buf [0] = '\t';
		buf [1] = '\0';
	}
	bp = &buf [strlen (buf)];
	left = RC_BUFSIZE - strlen (buf) - 1;
	for (i = 0; i < length; i++) {
		c = *dp++;
		ascii [i] = (c >= ' ' && c <= '~') ? c : '.';
		if (i == 8) {
			snprintf (bp, left, "- ");
			bp += 2;
			left -= 2;
		}
		snprintf (bp, left, "%02x ", c);
		bp += 3;
		left -= 3;
	}
	ascii [i] = '\0';
	while (i < 16) {
		if (i == 8) {
			snprintf (bp, left, "  ");
			bp += 2;
			left -= 2;
		}
		snprintf (bp, left, "   ");
		bp += 3;
		left -= 3;
		i++;
	}
	snprintf (bp, left, "  %s", ascii);
	return (buf);
}

void dbg_print_region (const void *dstp, size_t length, int show_addr, int ofs)
{
	size_t			i;
	const unsigned char	*dp = (const unsigned char *) dstp;

	for (i = 0; i < length; i += 16) {
		dbg_printf ("%s\r\n", region_chunk_str (dp,
				(length < i + 16) ? length - i: 16, show_addr,
				(ofs) ? dstp : NULL));
		dp += 16;
	}
}

void dbg_print_time (const FTime_t *t)
{
	FTime_t	rel = *t;

	FTIME_SUB (rel, sys_startup_time);
	dbg_printf ("%d.%09us", FTIME_SEC (rel), FTIME_NSEC (rel));
}

/* dbg_indent -- Set the indent string (e.g. <TAB> or ' '*n). */

void dbg_indent (const char *indstr)
{
	indent_str = indstr;
}

/* dbg_print_indent -- Indent with TABs for the given level and display
		       '<name>: ' if name is non-NULL. */

void dbg_print_indent (unsigned peer, const char *name)
{
	unsigned	i;

	for (i = 0; i < (unsigned) peer; i++)
		dbg_printf ("%s", indent_str);
	if (name && *name)
		dbg_printf ("%s: ", name);
}

void dbg_flush (void)
{
#ifndef CDR_ONLY
	lock_takef (dbg_lock);
	if (dbg_thread != thread_id()) {
		lock_releasef (dbg_lock);
		return;
	}
#endif
	do_actions (EL_DEBUG, cur_actions [EL_DEBUG], NULL, dbg_buffer, 0);
	if (!handle)
		fflush (stdout);

	dbg_length = 0;
	dbg_buffer [0] = '\0';

#ifndef CDR_ONLY
	dbg_thread = 0;
	cond_signal_all (dbg_cond);
	lock_releasef (dbg_lock);
#endif
}


void log_printf (unsigned id, unsigned level, const char *fmt, ...)
{
	va_list		arg;
	unsigned	acts;
	char		sbuf [256];
#ifndef CDR_ONLY
	static int	init_needed = 1;

	if (init_needed) {
		lock_init_r (log_lock, "log");
		cond_init (log_cond);
		init_needed = 0;
	}
#endif
	acts = log_logged (id, level);
	if (!acts)
		return;

	va_start (arg, fmt);
	vsnprintf (sbuf, sizeof (sbuf), fmt, arg);
	va_end (arg);

#ifndef CDR_ONLY
	lock_takef (log_lock);

	/* Verify that the buffer is not in use by another thread. If it is wait
	 * until it signals it is done with it. */
	while (log_thread && log_thread != thread_id())
		cond_wait (log_cond, log_lock); 

	log_thread = thread_id ();
#endif
	buffer_output (EL_LOG, acts, sbuf, log_buffer, &log_length);

#ifndef CDR_ONLY
	if (log_length == 0) {
		log_thread = 0;
		cond_signal_all (log_cond);
	}
	
	lock_releasef (log_lock);
#endif
}

void log_print_region (unsigned   id,
		       unsigned   level,
		       const void *dstp,
		       size_t     length,
		       int        show_addr,
		       int        offset)
{
	size_t			i;
	const unsigned char	*dp = (const unsigned char *) dstp;

	for (i = 0; i < length; i += 16) {
		log_printf (id, level, "%s\r\n",
			    region_chunk_str (dp, (length < i + 16) ? length - i : 16,
			    		show_addr, (offset) ? dstp : NULL));
		dp += 16;
	}
}

void log_flush (unsigned id, unsigned level)
{
	unsigned	acts;

#ifndef CDR_ONLY
	lock_takef (log_lock);
	if (log_thread != thread_id()) {
		lock_releasef (log_lock);
		return;
	}
#endif
	acts = log_logged (id, level);
	if (acts)
		do_actions (EL_LOG, acts, NULL, log_buffer, 0);

	log_buffer [0] = '\0';
	log_length = 0;
#ifndef CDR_ONLY
	log_thread = 0;
	cond_signal_all (log_cond);
	lock_releasef (log_lock);
#endif
}


void warn_printf (const char *fmt, ...)
{
	va_list	arg;
	char	sbuf [256];

	va_start (arg, fmt);
	vsnprintf (sbuf, sizeof (sbuf), fmt, arg);
	va_end (arg);
	do_actions (EL_WARNING, cur_actions [EL_WARNING], "Warning", sbuf, 1);
}

void err_printf (const char *fmt, ...)
{
	va_list	arg;
	char	sbuf [256];

	va_start (arg, fmt);
	vsnprintf (sbuf, sizeof (sbuf), fmt, arg);
	va_end (arg);
	do_actions (EL_ERROR, cur_actions [EL_ERROR], "Error", sbuf, 1);
}

void fatal_printf (const char *fmt, ...)
{
	va_list	arg;
	char	sbuf [256];

	va_start (arg, fmt);
	vsnprintf (sbuf, sizeof (sbuf), fmt, arg);
	va_end (arg);
	do_actions (EL_FATAL, cur_actions [EL_FATAL], "Fatal", sbuf, 1);
#ifdef USE_ABORT
	abort();
#else
	exit (1);
#endif
}

static void ula2una (uint32_t ul [2], unsigned char nyb [16])
{
	unsigned	j, off, s;
	uint32_t	m;

	for (j = 0, off = 0; j < 2; j++)
		for (m = 0xf0000000U, s = 28; m; m >>= 4, s -= 4)
			nyb [off++] = (ul [j] & m) >> s;
}

static void unadivmod (unsigned char	nyb [],
		       unsigned		n,
		       unsigned char	n2 [],
		       unsigned		*rem)
{
	unsigned	j, x, r = 0;

	for (j = 0; j < 16; j++) {
		x = nyb [j] + (r << 4);
		n2 [j] = x / n;
		r = x % n;
	}
	*rem = r;
}

static int unanzero (unsigned char nyb [])
{
	unsigned	i;

	for (i = 0; i < 16; i++)
		if (nyb [i])
			return (1);
	return (0);
}

#define	hexchar(r) (r > 9) ? 'a' + (r) - 10: '0' + (r)

static void una2str (unsigned char una [], char *buf, unsigned *pos, unsigned base)
{
	unsigned	rem;
	unsigned char	una2 [16];

	unadivmod (una, base, una2, &rem);
	if (unanzero (una2))
		una2str (una2, buf, pos, base);
	buf [(*pos)++] = hexchar (rem);
}

char *dul2str (uint32_t high, uint32_t low, char *buf, unsigned base)
{
	unsigned	pos = 0;
	uint32_t	ula [2];
	unsigned char	una [16];

	if (base < 2 || base > 16)
		return (NULL);

	ula [0] = high;
	ula [1] = low;
	ula2una (ula, una);
	una2str (una, buf, &pos, base);
	buf [pos] = '\0';
	return (buf);
}

void dbg_print_dulong (uint32_t high, uint32_t low, unsigned base)
{
	char	buf [32];

	dbg_printf ("%s", dul2str (high, low, buf, base));
}


