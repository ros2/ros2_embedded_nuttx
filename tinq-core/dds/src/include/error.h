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

/* error.h -- Various grades of error handling support. */

#ifndef __error_h_
#define	__error_h_

#include <stdio.h>
#include <stdint.h>
#include "sock.h"
#include "sys.h"

#define	ARG_NOT_USED(p)	(void) p;

#define	ACT_IGNORE	0	/* No actions. */
#define	ACT_PRINT_STDIO	1	/* Output message to stdout. */
#define	ACT_PRINT_ERR	2	/* Output message to stderr. */ 
#define ACT_PRINT_FILE	4	/* Output message to redirect file. */
#ifdef _WIN32
#define ACT_PRINT_SK	8	/* Output message to redirect socket. */
#else
#define	ACT_PRINT_SYS	8	/* Output message to syslog. */
#endif
#define ACT_PREF_STDIO	16	/* Add non-log prefix on stdout. */
#define ACT_PREF_ERR	32	/* Add non-log prefix on stderr. */
#define	ACT_LOG		64	/* Add message to log file. */
#define	ACT_EXIT	128	/* Exit program immediately. */	

void err_prog_args (int *argc, char *argv []);

/* Set the program name and arguments for successive error logging. */

typedef enum {
	EL_LOG,		/* Informational message. */
	EL_DEBUG,	/* Debug shell message. */
	EL_WARNING,	/* Warning message. */
	EL_ERROR,	/* Recoverable error. */
	EL_FATAL	/* Fatal error. */
} ErrLevel_t;

extern unsigned log_debug_count;

/* Logging/tracing level action management: */
/* - - - - - - - - - - - - - - - - - - - -  */

void log_actions_set (unsigned id, unsigned level, unsigned actions);

/* Set/replace all actions for the given logging source/level. */

void log_actions_add (unsigned id, unsigned level, unsigned actions);

/* Add actions to the given logging source/level. */

void log_actions_remove (unsigned id, unsigned level, unsigned actions);

/* Remove actions from the given logging source/level. */

void log_actions_default (unsigned id, unsigned level);

/* Do default actions for the given logging source/level. */


/* Error level action management: */
/* - - - - - - - - - - - - - - -  */

void err_actions_set (ErrLevel_t level, unsigned actions);

/* Set/replace all actions for the given error level. */

void err_actions_add (ErrLevel_t level, unsigned actions);

/* Add actions to the given error level. */

void err_actions_remove (ErrLevel_t level, unsigned actions);

/* Remove actions from the given error level. */


/* Level-specific notification functions: */
/* - - - - - - - - - - - - - - - - - - -  */

#ifndef __GNUC__
#define	__attribute__(x)	/*NOTHING*/
#endif

void log_printf (unsigned id, unsigned level, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

/* Log some informational message. */

void log_print_region (unsigned   id,
		       unsigned   level,
		       const void *p,
		       size_t     length,
		       int        show_addr,
		       int        offset);

/* Log the contents of a memory region {*p,length}.  If show_addr is set,
   each line is prepended with either the memory address (offset=0) or the data
   offset (offset=1). */

void log_flush (unsigned id, unsigned level);

/* Signal completion of a logged string.  Necessary if logged output doesn't
   contain an end-of-line. */

unsigned log_logged (unsigned id, unsigned level);

/* Return the logging status of the given id/level. */


void dbg_printf (const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

/* Display a debug shell request or response message. */

void dbg_print_region (const void *p,
		       size_t     length,
		       int        show_addr,
		       int        offset);

/* Dump the contents of a memory region {*p,length}.  If show_addr is set,
   each line is prepended with either the memory address (offset=0) or the data
   offset (offset=1). */

void dbg_print_time (const FTime_t *t);

/* Dump a timestamp. */

void dbg_indent (const char *indent_str);

/* Set the indent string (e.g. <TAB> or ' '*n). */

void dbg_print_indent (unsigned level, const char *name);

/* Indent with the indent string for the given level and display '<name> = '
   if name is non-NULL. */

void dbg_flush (void);

/* Signal completion of a debug shell string.  Necessary if shell output doesn't
   contain an end-of-line. */


void warn_printf (const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

/* Warning message. */

void err_printf (const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

/* Recoverable error message. */

#ifdef _WIN32
__declspec(noreturn)
#endif
void fatal_printf (const char *fmt, ...)
	__attribute__((noreturn, format(printf, 1, 2)));

/* Non-recoverable error message. */

void dbg_redirect (HANDLE h);

/* Redirect debug output to the given file descriptor if non-NULL.
   Otherwise, the output is reverted back to the standard output. */

#ifdef _WIN32

void dbg_redirect_sk (SOCKET out_sock);

/* Redirect debug output to the given socket if non-0.
   Otherwise, the output is reverted back to the standard output. */

#endif

/* Extra functionality: */
/* - - - - - - - - - -  */

char *dul2str (uint32_t high, uint32_t low, char *buf, unsigned base);

/* Convert a 64-bit number based on two 32-bit numbers to a string. */

void dbg_print_dulong (uint32_t high, uint32_t low, unsigned base);

/* Display a 64-bit number based on two 32-bit numbers. */

#endif /* !__error_h_ */

