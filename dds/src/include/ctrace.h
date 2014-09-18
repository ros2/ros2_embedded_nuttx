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

/* ctrace.h -- Interface to the cyclic trace functionality. */

#ifndef __ctrace_h_
#define	__ctrace_h_

#ifndef __GNUC__
#define	__attribute__(x)	/*NOTHING*/
#endif

extern int ctrace_used;
extern int ctrace_saves;

#ifdef CTRACE_USED

void ctrc_init (size_t bufsize);

/* Initialize the cyclic tracing buffer. */

void ctrc_final (void);

/* Cleanup the cyclic tracing buffer. */


void ctrc_start (void);

/* Start tracing. */

void ctrc_stop (void);

/* Stop tracing. */

void ctrc_clear (void);

/* Clear the trace buffer. */

void ctrc_mode (int cyclic);

/* Set the trace mode to either cyclic (cyclic=1) or stop on full (cyclic=0). */


void ctrc_printd (unsigned id, unsigned index, const void *data, size_t length);

/* Reentrant cyclic tracing function.  The id is the module identifier as
   defined in log.h.  The index parameter is a module specific index.
   The ptr/length arguments can be used to add extra binary data. */

void ctrc_printf (unsigned id, unsigned index, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

/* Same as ctrc_printd, but with extra formatted arguments. */

void ctrc_begind (unsigned id, unsigned index, const void *data, size_t length);

/* Start a long sample, consisting of a number of binary data chunks. */

void ctrc_contd (const void *data, size_t length);

/* Continue with the next binary data chunk. */

void ctrc_endd (void);

/* All data chunks given, store the data. */


void ctrc_info (void);

/* Display cyclic trace info. */

void ctrc_dump (void);

/* Dump the contents of the cyclic trace. */

void ctrc_save (const char *filename);

/* Save the contents of the cyclic trace to the given file. */

/* Cyclic trace actions: */
#define	CTA_PARAM	0x000f	/* Counter/action number. */
#define CTA_START	0x0010	/* Enable tracing from now on. */
#define	CTA_STOP	0x0020	/* Stop tracing. */
#define CTA_CLEAR	0x0040	/* Clear the trace. */
#define	CTA_MODE	0x0080	/* Toggle the tracing mode. */
#define	CTA_INC		0x0100	/* Increment counter. */
#define	CTA_DEC		0x0200	/* Decrement counter. */
#define	CTA_MARK	0x0400	/* Mark the current trace context. */
#define CTA_RESTORE	0x0800	/* Restore trace context to the mark point. */
#define	CTA_IFZ		0x1000	/* Only perform next action if counter == 0. */
#define	CTA_IFNZ	0x2000	/* Only perform next action if counter != 0. */
#define	CTA_GOTO	0x4000	/* Go to the given action index. */
#define	CTA_SIGNAL	0x8000	/* Signal user. */

typedef unsigned short CTrcAction_t;

int ctrc_actions_add (unsigned     position,
		      unsigned     id,
		      unsigned     index,
		      void         *data,
		      size_t       length,
		      CTrcAction_t *actions,
		      unsigned     nactions);

/* Do a number of cyclic trace actions based on the id, index and data given.
   The position is the index where to add the the actions.  If the action index
   is an existing index, the action will be inserted between that one and the
   previous action.  If it is larger than the highest action index, it will
   become the next last one.
   If index == ~0, any index for the given id will trigger the actions.
   If data == NULL and length == 0, the data parameter will not be checked.
   Several actions may be combined together, as shown above. */

void ctrc_actions_remove (unsigned position);

/* Remove an action at the given position. */

void ctrc_actions_remove_all (void);

/* Remove all installed actions. */

int ctrc_actions_get (unsigned     position,
		      unsigned     *id,
		      unsigned     *index,
		      void         **data,
		      size_t       *length,
		      CTrcAction_t **actions,
		      unsigned     *nactions);

/* Get the actions at the given action index.  If no actions are defined, a
   non-0 error code wil be returned. */

const char *ctrc_trace_name (unsigned id, unsigned index);

/* Display a tracepoint id or id:index. */

int ctrc_trace_match (const char *name, unsigned *id, unsigned *index);

/* Convert a trace name to an id and index pair. */

void ctrc_counters_get (unsigned long counters [16]);

/* Get all the counter values. */

void ctrc_counters_clear (void);

/* Clear all counters. */

#else

#define	ctrc_init(n)
#define	ctrc_start()
#define	ctrc_stop()
#define	ctrc_mode(n)
#define	ctrc_printd(id,idx,d,l)
#define	ctrc_begind(id,idx,d,l)
#define	ctrc_contd(d,l)
#define	ctrc_endd()

#endif


#endif /* !__ctrace_h_ */

