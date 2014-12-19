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

/* ctrace.c -- Implements a fast cyclic trace buffer for storing real-time
	       events. */

#ifdef CTRACE_USED

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "thread.h"
#include "sys.h"
#include "libx.h"
#include "log.h"
#include "error.h"
#include "pool.h"
#include "sock.h"
#include "debug.h"
#include "ctrace.h"

#define	MAX_UNITS	4096		/* Max. # of tracing units. */
#define	MAX_SAMPLE_DATA	1024		/* Max. sample data size. */

/* Cyclic trace data unit (16-bytes): */
typedef struct ctrace_unit_st {
	Time_t		time;		/* 64-bit timestamp. */
	unsigned char	id;		/* Module Id. */
	unsigned char	index;		/* Trace point index. (0xff:extended).*/
        unsigned char	data [6];	/* Extra data. */
} CTRACE_UNIT_ST, *CTRACE_UNIT;

/* Trace status flags: */
#define	CTF_RUNNING	1		/* Tracing is active. */
#define	CTF_STOP	2		/* Stop tracing when full. */
#define	CTF_CYCLED	4		/* Trace buffer has cycled. */

static CTRACE_UNIT	first;		/* First cyclic trace unit. */
static CTRACE_UNIT	last;		/* Last cyclic trace unit. */
static CTRACE_UNIT	head;		/* Head of valid units. */
static CTRACE_UNIT	tail;		/* Tail of valid units. */
static CTRACE_UNIT	tailh;		/* First tail record of valid units. */
static unsigned		max_units;	/* Maximum # of traced units. */
static unsigned		cur_units;	/* Current # of traced units. */
static unsigned         trc_flags = 0;	/* Trace flags. */
static lock_t		trc_lock;	/* Trace buffer access busy. */
static lock_t		trc_lock_data;	/* Data buffer in use. */
static unsigned		trc_id;
static unsigned		trc_index;
static unsigned char	trc_data [MAX_SAMPLE_DATA];
static size_t		trc_length;

int ctrace_used;
int ctrace_saves;

/* ctrc_init -- Initialize the cyclic tracing buffer. */

void ctrc_init (size_t bufsize)
{
	unsigned	nunits = bufsize >> 4;

	if (ctrace_used)
		return;

	first = mm_fcts.alloc_ (nunits * sizeof (CTRACE_UNIT_ST));
	if (!first)
		return;

	last = first + nunits - 1;
	head = tail = first;
	max_units = nunits;
	cur_units = 0;
	trc_flags = CTF_STOP;
#ifdef CTRACE_START
	trc_flags |= CTF_RUNNING;
#endif
	lock_init_nr (trc_lock, "TRC_LOCK");
	lock_init_nr (trc_lock_data, "TRC_DATA");
	ctrace_used = 1;
}

void ctrc_final (void)
{
	mm_fcts.free_ (first);
	first = NULL;
}

/* ctrc_start -- Start tracing. */

void ctrc_start (void)
{
	trc_flags |= CTF_RUNNING;
}

/* ctrc_stop -- Stop tracing. */

void ctrc_stop (void)
{
	trc_flags &= ~CTF_RUNNING;
}

/* ctrc_lock -- Lock access to the trace buffer. */

#define ctrc_lock()	lock_takef(trc_lock)

/* ctrc_unlock -- Unlock trace buffer access. */

#define ctrc_unlock()	lock_releasef(trc_lock)

/* ctrc_clear -- Clear the trace buffer. */

void ctrc_clear (void)
{
	ctrc_lock ();
	head = tail = tailh = first;
	cur_units = 0;
	trc_flags &= ~CTF_CYCLED;
	ctrc_unlock ();
}

/* ctrc_mode -- Set the trace mode to either cyclic or stop when full. */

void ctrc_mode (int cycle)
{
	if (cycle)
		trc_flags &= ~CTF_STOP;
	else
		trc_flags |= CTF_STOP;
}

#define	MAX_ACTIONS	64
#define	MAX_ACT_TABLE	128

typedef struct trc_actions_st TrcActions_t;
struct trc_actions_st {
	TrcActions_t	*next;
	unsigned	id;
	unsigned	index;
	void		*data;
	size_t		length;
	CTrcAction_t	actions [16];
	unsigned	nactions;
};

static TrcActions_t	action_data [MAX_ACTIONS];
static unsigned		nactions;
static TrcActions_t	*actions_ht [MAX_ACT_TABLE];
static unsigned		nactions_ht;
static TrcActions_t	*id_actions [32];
static TrcActions_t	*trc_ap;
static unsigned long	counter [16];
static CTRACE_UNIT	mark_head;
static CTRACE_UNIT	mark_tail;
static unsigned		mark_flags;
static unsigned		mark_units;

#define	TRC_HASH(id,index)	((((id) << 4) | (index)) & 0x7f)

static TrcActions_t *ctrc_actions_lookup (unsigned   id,
				          unsigned   index,
					  const void *data,
					  size_t     length)
{
	unsigned	h;
	TrcActions_t	*ap;

	if (nactions_ht) {
		h = TRC_HASH (id, index);
		for (ap = actions_ht [h]; ap; ap = ap->next) {
			if (ap->id != id || ap->index != index)
				continue;

			if (!ap->length ||
			    (length == ap->length &&
			     !memcmp (data, ap->data, length)))
				return (ap);
		}
	}
	ap = id_actions [id];
	if (!ap)
		return (NULL);

	if (ap->length &&
	    (length == ap->length ||
	     memcmp (data, ap->data, length)))
		return (NULL);

	return (ap);
}

static void ctrc_do_actions (TrcActions_t *ap)
{
	unsigned	pc;
	CTrcAction_t	acts;

	pc = 0;
	while (pc < ap->nactions) {
		acts = ap->actions [pc++];
		if ((acts & CTA_START) != 0)
			trc_flags |= CTF_RUNNING;
		if ((acts & CTA_STOP) != 0)
			trc_flags &= CTF_RUNNING;
		if ((acts & CTA_CLEAR) != 0) {
			head = tail = tailh = first;
			cur_units = 0;
			trc_flags &= ~CTF_CYCLED;
		}
		if ((acts & CTA_MODE) != 0)
			trc_flags ^= CTF_STOP;
		if ((acts & CTA_INC) != 0)
			counter [acts & CTA_PARAM]++;
		if ((acts & CTA_DEC) != 0)
			counter [acts & CTA_PARAM]--;
		if ((acts & CTA_MARK) != 0) {
			mark_head = head;
			mark_tail = tail;
			mark_flags = trc_flags;
			mark_units = cur_units;
		}
		if ((acts & CTA_RESTORE) != 0) {
			if (mark_head) {
				head = mark_head;
				tail = mark_tail;
				trc_flags = mark_flags;
				cur_units = mark_units;
				mark_head = NULL;
			}
		}
		if ((acts & CTA_IFZ) != 0 &&
		    counter [acts & CTA_PARAM] != 0)
				pc++;
		if ((acts & CTA_IFNZ) != 0 &&
		    counter [acts & CTA_PARAM] == 0)
				pc++;
		if ((acts & CTA_GOTO) != 0)
			pc = acts & CTA_PARAM;
		if ((acts & CTA_SIGNAL) != 0)
			dbg_printf ("CTrace:Hit(%s)\r\n", ctrc_trace_name (ap->id, ap->index));
	}
}

/* ctrc_printd -- Cyclic tracing function. */

void ctrc_printd (unsigned id, unsigned index, const void *data, size_t length)
{
	unsigned		i, n;
	const unsigned char	*sp;
	unsigned char		*dp;
	TrcActions_t		*ap;

	if (nactions_ht || id_actions [id])
		ap = ctrc_actions_lookup (id, index, data, length);
	else
		ap = NULL;

	if ((trc_flags & CTF_RUNNING) == 0 &&
	    (!ap || (ap->actions [0] & CTA_START) == 0))
		return;

	ctrc_lock ();
	sys_gettime (&tail->time);
	tailh = tail;
	tail->id = id;
	tail->index = index;
	tail->data [0] = length >> 8;
	tail->data [1] = length & 0xff;
	if (data && length) {
		sp = data;
		n = length;
		if (n > 4)
			n = 4;
		for (i = 0; i < n; i++)
			tail->data [i + 2] = *sp++;
		length -= n;
		while (length) { /* More data needed: extra unit! */
			cur_units++;
			if (tail == last) {
				tail = first;
				trc_flags |= CTF_CYCLED;
			}
			else
				++tail;
			tail->index = 0xff;

			/* Copy up to 5 bytes at hrt + tid. */
			dp = (unsigned char *) &tail->time;
			n = length;
			if (n > 5)
				n = 5;
			memcpy (dp, sp, n);
			length -= n;
			sp += n;
			if (length) { /* Copy up to 6 bytes at data. */
				dp = tail->data;
				n = length;
				if (n > sizeof(tail->data))
					n = sizeof(tail->data);
				memcpy (dp, sp, n);
				length -= n;
				sp += n;
			}
		}
	}
	cur_units++;
	if ((trc_flags & CTF_STOP) != 0 && cur_units + 1 >= max_units)
		trc_flags &= ~CTF_RUNNING;
	if (tail == last) {
		tail = first;
		trc_flags |= CTF_CYCLED;
	}
	else
		++tail;
	if ((trc_flags & CTF_CYCLED) != 0) {
		head = tail;
		while (head->index == 0xff)
			if (head == last)
				head = first;
			else
				++head;
	}
	if (ap)
		ctrc_do_actions (ap);
	ctrc_unlock ();
}

/* ctrc_printf -- Same as ctrc_printd, but with extra formatted arguments. */

void ctrc_printf (unsigned id, unsigned index, const char *fmt, ...)
{
	va_list	arg;
	char	buf [128];

	if ((trc_flags & CTF_RUNNING) == 0)
		return;

	va_start (arg, fmt);
	vsprintf (buf, fmt, arg);
	va_end (arg);
	ctrc_printd (id, index, buf, strlen (buf));
}

/* ctrc_begind -- Start a long sample, consisting of a number of concatenated
		  binary data chunks. */

void ctrc_begind (unsigned id, unsigned index, const void *data, size_t length)
{
	TrcActions_t		*ap;

	if (nactions_ht || id_actions [id])
		ap = ctrc_actions_lookup (id, index, data, length);
	else
		ap = NULL;

	if ((trc_flags & CTF_RUNNING) == 0 &&
	    (!ap || (ap->actions [0] & CTA_START) == 0))
		return;

	lock_takef (trc_lock_data);
	trc_ap = ap;
	trc_id = id;
	trc_index = index;
	trc_length = 0;
	ctrc_contd (data, length);
}

/* ctrc_contd -- Continue with the next binary data chunk. */

void ctrc_contd (const void *data, size_t length)
{
	if ((trc_flags & CTF_RUNNING) == 0 &&
	    (!trc_ap || (trc_ap->actions [0] & CTA_START) == 0))
		return;

	if (trc_length + length > MAX_SAMPLE_DATA)
		length = MAX_SAMPLE_DATA - trc_length;
	if (length) {
		memcpy (&trc_data [trc_length], data, length);
		trc_length += length;
	}
}

/* ctrc_endd -- All data chunks given, store the data. */

void ctrc_endd (void)
{
	if ((trc_flags & CTF_RUNNING) == 0 &&
	    (!trc_ap || (trc_ap->actions [0] & CTA_START) == 0))
		return;

	ctrc_printd (trc_id, trc_index, trc_data, trc_length);
	if (trc_ap) {
		ctrc_do_actions (trc_ap);
		trc_ap = NULL;
	}
	lock_releasef (trc_lock_data);
}


/* ctrc_next_entry -- Return the cyclic trace entry at the given offset and
		      return the number of data bytes in the trace entry. */

int ctrc_next_entry (unsigned *ofs,
		     Time_t   *time,
		     unsigned *id,
		     unsigned *index,
		     void     *dp,
		     size_t   max)
{
	CTRACE_UNIT	tp;
	unsigned char	*data = (unsigned char *) dp;
	size_t		len, n;

	if (!ofs || *ofs >= cur_units || !time || !id || !index || !data || max < 6)
		return (-1);

	n = last - head + 1;
	if (*ofs < n)
		tp = head + *ofs;
	else
		tp = first + *ofs - n;
	(*ofs)++;
	*time = tp->time;
	*id = tp->id;
	*index = tp->index;
	len = (tp->data [0] << 8) | tp->data [1];
	memcpy (data, &tp->data [2], 4);
	max -= 4;
	data += 4;
	if (tp == last)
		tp = first;
	else
		tp++;
	while (*ofs < cur_units && tp->index == 0xff) {
		if (max) {
			n = max;
			if (n > 5)
				n = 5;
			memcpy (data, tp, n);
			max -= n;
			data += n;
			if (max) {
				n = max;
				if (n > 6)
					n = 6;
				memcpy (data, tp->data, n);
				max -= n;
				data += n;
			}
		}
		if (tp == last)
			tp = first;
		else
			tp++;
		(*ofs)++;
	}
	return (len);
}

/* ctrc_dump_entry -- Dump a cyclic trace entry. */

static void ctrc_dump_entry (Time_t        *tp,
			     unsigned      id,
			     unsigned      index,
			     unsigned char *dp,
			     size_t        len)
{
	unsigned	i;

	dbg_printf ("%010d.%03u,%03u - %8s:", tp->seconds, 
					tp->nanos / 1000000,
					(tp->nanos / 1000) % 1000,
					log_id_str [id]);
	if (log_fct_str [id])
		dbg_printf ("%s\t", log_fct_str [id][index]);
	else
		dbg_printf ("?(%u)\t", index);
	if (len <= 4) {
		for (i = 0; i < len; i++)
			dbg_printf ("%02x", *dp++);
		dbg_printf ("\r\n");
	}
	else {
		dbg_printf ("\r\n");
		dbg_print_region (dp, len, 0, 0);
	}
}

/* ctrc_dump -- Dump the contents of the cyclic trace. */

void ctrc_dump (void)
{
	unsigned	ofs;
	int		n;
	unsigned	start_secs = 0;
	Time_t		time;
	unsigned	i;
	unsigned	id;
	unsigned	index;
	unsigned char	buf [128];

	ctrc_lock ();
	ofs = 0;
	for (ofs = 0, i = 0; ; i++) {
		n = ctrc_next_entry (&ofs, &time, &id, &index, buf, sizeof (buf));
		if (n < 0)
			break;

		if (!i) {
			start_secs = time.seconds;
			time.seconds = 0;
		}
		else
			time.seconds -= start_secs;
		ctrc_dump_entry (&time, id, index, buf, n);
	}
	ctrc_unlock ();
}

/* ctrc_info -- Display cyclic trace info. */

void ctrc_info (void)
{
	ctrc_lock ();
	dbg_printf ("Trace is ");
	if ((trc_flags & CTF_RUNNING) != 0)
		dbg_printf ("running");
	else
		dbg_printf ("inactive");
	dbg_printf (", mode: ");
	if ((trc_flags & CTF_STOP) != 0)
		dbg_printf ("stop when full");
	else {
		dbg_printf ("cyclic");
		if ((trc_flags & CTF_CYCLED) != 0)
			dbg_printf (" (cycled)");
	}
	dbg_printf ("\r\n");
	dbg_printf ("Contains %u records of %u.\r\n", cur_units, max_units);
	if (cur_units)
		dbg_printf ("Total duration is %d seconds.\r\n", tailh->time.seconds - head->time.seconds);
	ctrc_unlock ();
}

/* ctrc_save -- Save a cyclic trace buffer to a file. */

void ctrc_save (const char *filename)
{
	HANDLE	h;

	if ((h = creat (filename, 0644)) < 0) {
		dbg_printf ("File create error (%d)!\r\n", h);
		return;
	}
	ctrace_saves = 1;
	dbg_redirect (h);
	ctrc_info ();
	ctrc_dump ();
	dbg_redirect (0);
	ctrace_saves = 0;
	close (h);
}

/* ctrc_actions_add -- Do a number of cyclic trace actions based on the id,
		       index and data given. */

int ctrc_actions_add (unsigned     position,
		      unsigned     id,
		      unsigned     index,
		      void         *data,
		      size_t       length,
		      CTrcAction_t *actions,
		      unsigned     nacts)
{
	TrcActions_t	*ap;
	unsigned	h;

	if (nactions >= MAX_ACTIONS ||
	    !nacts || nacts >= 16 ||
	    id > 32 ||
	    (index == ~0U && id_actions [id] != NULL))
		return (DDS_RETCODE_OUT_OF_RESOURCES);

	if (position >= nactions)
		ap = &action_data [nactions];
	else {
		memmove (&action_data [position + 1],
			 &action_data [position],
			 (nactions - position) * sizeof (TrcActions_t));
		ap = &action_data [position];
	}
	ap->id = id;
	ap->index = index;
	if (length && data) {
		ap->data = malloc (length);
		if (!ap->data)
			fatal_printf ("ctrc_actions_add: out of memory for match data!");

		ap->length = length;
		memcpy (ap->data, data, length);
	}
	else {
		ap->data = NULL;
		ap->length = 0;
	}
	memcpy (ap->actions, actions, nacts * sizeof (CTrcAction_t));
	ap->nactions = nacts;
	if (index == ~0U) {
		id_actions [id] = ap;
		ap->next = NULL;
	}
	else {
		h = TRC_HASH (id, index);
		ap->next = actions_ht [h];
		actions_ht [h] = ap;
		nactions_ht++;
	}
	nactions++;
	return (DDS_RETCODE_OK);
}

/* ctrc_actions_remove -- Remove an action at the given position. */

void ctrc_actions_remove (unsigned position)
{
	ARG_NOT_USED (position)

	/* ... TBD ... */
}

/* ctrc_actions_remove_all -- Remove all installed actions. */

void ctrc_actions_remove_all (void)
{
	TrcActions_t	*ap;
	unsigned	i;

	for (i = 0, ap = action_data; i < nactions; i++, ap++)
		if (ap->length && ap->data)
			free (ap->data);

	nactions = 0;
	nactions_ht = 0;
	memset (actions_ht, 0, sizeof (actions_ht));
	memset (id_actions, 0, sizeof (id_actions));
	memset (counter, 0, sizeof (counter));
}

/* ctrc_actions_get -- Get the actions at the given action index. */

int ctrc_actions_get (unsigned     position,
		      unsigned     *id,
		      unsigned     *index,
		      void         **data,
		      size_t       *length,
		      CTrcAction_t **actions,
		      unsigned     *nacts)
{
	TrcActions_t	*ap;

	if (position >= nactions)
		return (DDS_RETCODE_NO_DATA);

	ap = &action_data [position];
	if (id)
		*id = ap->id;
	if (index)
		*index = ap->index;
	if (data)
		*data = ap->data;
	if (length)
		*length = ap->length;
	if (actions)
		*actions = ap->actions;
	if (nacts)
		*nacts = ap->nactions;
	return (DDS_RETCODE_OK);
}

/* ctrc_trace_name -- Display a tracepoint id or a tracepoint id:index. */

const char *ctrc_trace_name (unsigned id, unsigned index)
{
	static char	buf [80];

	if (id > LOG_MAX_ID || !log_id_str [id])
		return (NULL);

	if (index == ~0U)
		snprintf (buf, sizeof (buf), "%s", log_id_str [id]);
	else if (!log_fct_str [id][index])
		snprintf (buf, sizeof (buf), "%s:?%u", log_id_str [id], index);
	else
		snprintf (buf, sizeof (buf), "%s:%s", log_id_str [id], log_fct_str [id][index]);
	return (buf);
}

/* ctrc_trace_match -- Convert a trace name to an id and index pair. */

int ctrc_trace_match (const char *name, unsigned *id, unsigned *index)
{
	unsigned	i, n;
	const char	**spp, *np;
	static char	buf [80];

	np = strchr (name, ':');
	if (np)
		n = np - name;
	else
		n = strlen (name);
	if (name [0] == '#') {
		memcpy (buf, name + 1, n - 1);
		buf [n - 1] = '\0';
		*id = atoi (buf);
	}
	else {
		for (i = 0; i <= LOG_MAX_ID; i++) {
			if (!log_id_str [i])
				continue;

			if (!astrncmp (name, log_id_str [i], n)) {
				*id = i;
				break;
			}
		}
		if (i > LOG_MAX_ID)
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (!np) {
		*index = ~0;
		return (DDS_RETCODE_OK);
	}
	np++;
	if (*np == '#')
		*index = atoi (np + 1);
	else {
		for (i = 0, spp = log_fct_str [*id]; *spp; i++, spp++)
			if (!astrcmp (np, *spp)) {
				*index = i;
				break;
			}
		if (!*spp)
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

/* ctrc_counters_get -- Get all the counter values. */

void ctrc_counters_get (unsigned long counters [16])
{
	memcpy (counters, counter, sizeof (counter));
}

/* ctrc_counters_clear -- Clear all counters. */

void ctrc_counters_clear (void)
{
	memset (counter, 0, sizeof (counter));
}


#else
int ctrace_used = 0;
#endif

