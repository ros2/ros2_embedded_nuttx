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

/* cmdline.c -- Implements a command line handler with support for line editing
		that keeps a history of previously entered commands which can be
		retrieved easily for re-editing.
		Supports cursor moves (left/right/home/end/word-left/word-right),
		character delete (backspace/delete) and both insert and
		overwrite modes.
		Previous commands in the command history can be selected using
		vertical cursor moves (up/down/pgup/pgdn). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "error.h"
#include "cmdline.h"

#define	CTRLA	0x01
#define	CTRLC	0x03
#define	CTRLD	0x04
#define	CTRLE	0x05
#define	CTRLH	0x08
#define	CTRLR	0x12

#define BEL	0x07
#define BS	0x08
#define	LF	0x0a
#define CR	0x0d
#define	ESC	0x1b
#define DEL	0x7f

#define	LINE_INC		8	/* Line buffer size increment. */
#define	MAX_HIST_LINES		32	/* Max. # of history lines. */
#define	MAX_CMD_LENGTH		256	/* Max. length of a command. */

#ifdef UNIX_TERM
#define	BACKSPACE	DEL
#else
#define	BACKSPACE	BS
#endif

typedef struct cmd_st Cmd_t;
struct cmd_st {
	Cmd_t		*next;		/* Next history line. */
	Cmd_t		*prev;		/* Previous history line. */
	char		*buffer;	/* Line buffer. */
	unsigned	size;		/* Line buffer size. */
};

typedef enum {
	ES_Normal,
	ES_Esc,
	ES_Esc_LSq,
	ES_Esc_LSq_Num,
	ES_Esc_LSq_Num_Semi,
	ES_Esc_LSq_Num_Semi_5,
	ES_Esc_O,
	ES_Esc_O1,
	ES_Esc_O1_Semi,
	ES_Esc_O1_Semi_5
} EscState_t;

struct cmd_line_st {
	Cmd_t		*head;		/* First history line. */
	Cmd_t		*tail;		/* Last history line. */
	Cmd_t		*line;		/* Current history line. */
	unsigned	pos;		/* Current line position. */
	unsigned	length;		/* Current line length. */
	int		overwrite;	/* Overwrite mode. */
	int		last_acc;	/* Last line is was entered. */
	EscState_t	state;		/* Character processing state. */
	unsigned	number;		/* When parsing an escape sequence. */
};

/* cl_new -- Create a new command line context. */

CmdLine_t *cl_new (void)
{
	CmdLine_t	*p;

	p = malloc (sizeof (CmdLine_t));
	if (!p)
		return (NULL);

	memset (p, 0, sizeof (CmdLine_t));
	return (p);
}

/* line_add_tail -- Add a command to the end of the history. */

static CmdLineStatus_t line_add_tail (CmdLine_t *p,
				      char      *buffer,
				      unsigned  size)
{
	Cmd_t	*lp;

	lp = malloc (sizeof (Cmd_t));
	if (!lp)
		return (CLS_DONE_ERROR);

	lp->buffer = malloc (size);
	if (!lp->buffer) {
		free (lp);
		return (CLS_DONE_ERROR);
	}
	lp->next = NULL;
	lp->prev = p->tail;
	if (!p->head)
		p->head = lp;
	else
		p->tail->next = lp;
	p->tail = lp;
	lp->size = size;
	memcpy (lp->buffer, buffer, size);
	return (CLS_DONE_OK);
}

#ifdef _WIN32
#define snprintf	sprintf_s
#define openf(f,name,m)	fopen_s(&f,name,m) == 0
#else
#define openf(f,name,m)	((f = fopen (name, m)) != NULL)
#endif

/* cl_load -- Load the command history with the contents of a file. */

void cl_load (CmdLine_t *p, const char *filename)
{
	FILE		*f;
	unsigned	n;
	char		buffer [MAX_CMD_LENGTH];

	if (openf (f, filename, "r")) {
		while (fgets (buffer, sizeof (buffer), f)) {
			n = strlen (buffer);
			if (n) {
				if (buffer [n-1] == '\n')
					buffer [--n] = '\0';

			}
			if (line_add_tail (p, buffer, n + 1) != CLS_DONE_OK)
				break;
		}
		fclose (f);
	}
}

/* cl_save -- Save the current command history in a file. */

void cl_save (CmdLine_t *clp, const char *filename)
{
	FILE	*f;
	Cmd_t	*p;

	if (openf (f, filename, "w")) {
		for (p = clp->head; p; p = p->next)
			if (p->buffer [0] != 'q' && p->buffer [0] != 'Q')
				fprintf (f, "%s\n", p->buffer);

		fclose (f);
	}
}

/* cl_delete -- Delete a complete command line context. */

void cl_delete (CmdLine_t *hp)
{
	Cmd_t	*p, *next_p;

	for (p = hp->head; p; p = next_p) {
		next_p = p->next;
		free (p->buffer);
		free (p);
	}
	hp->head = hp->tail = hp->line = NULL;
	free (hp);
}

/* cl_home -- Move to the start of the current command line. */

static void cl_home (CmdLine_t *p)
{
	while (p->pos) {
		dbg_printf ("%c", BS);
		p->pos--;
	}
	dbg_flush ();
}

/* cl_end -- Move to the end of the current command line. */

static void cl_end (CmdLine_t *p)
{
	while (p->pos < p->length) {
		dbg_printf ("%c", p->line->buffer [p->pos]);
		p->pos++;
	}
	dbg_flush ();
}

/* line_delete_tail -- Delete the last history command. */

static void line_delete_tail (CmdLine_t *p)
{
	Cmd_t	*lp;

	if (!p->head)
		return;

	lp = p->tail;
	if (lp->buffer && lp->size)
		free (lp->buffer);

	p->tail = lp->prev;
	free (lp);
	if (!p->tail)
		p->head = NULL;
	else
		p->last_acc = 1;
}

/* cl_eol -- An end-of-line character was typed. */

static void cl_eol (CmdLine_t *p)
{
	dbg_printf ("\r\n");
	if (p->length) {
		if (p->line != p->tail && !p->last_acc)
			line_delete_tail (p);
		if (p->line != p->tail &&
		    strcmp (p->line->buffer, p->tail->buffer))
			line_add_tail (p, p->line->buffer, p->line->size);
		p->last_acc = 1;
	}
	else
		line_delete_tail (p);
	p->length = p->pos = 0;
	p->line = NULL;
}

/* cl_clear -- Clear the current command. */

static void cl_clear (CmdLine_t *p)
{
	unsigned	i;

	if (!p->length)
		return;

	if (p->pos < p->length)
		cl_end (p);

	for (i = 0; i < p->length; i++)
		dbg_printf ("%c %c", BS, BS);
	dbg_flush ();
	p->line->size = p->pos = p->length = 0;
}

/* cl_redraw -- Redraw the current command. */

static void cl_redraw (CmdLine_t *p)
{
	unsigned	i;

	for (i = 0; i < p->pos; i++)
		dbg_printf ("%c", BS);
	for (i = 0; i < p->length; i++)
		dbg_printf ("%c", p->line->buffer [i]);
	for (i = 0; i < p->length - p->pos; i++)
		dbg_printf ("%c", BS);
	dbg_flush ();
}

/* cl_move -- Move the cursor to the end of the selected history command. */

static void cl_move (CmdLine_t *p, Cmd_t *lp)
{
	unsigned	nlength, old_pos, i;

	nlength = (lp->buffer) ? strlen (lp->buffer) : 0;
	if (p->length > nlength) {
		old_pos = p->pos;
		while (p->pos < p->length) {
			dbg_printf (" ");
			p->pos++;
		}
		while (p->pos > old_pos) {
			dbg_printf ("%c", BS);
			p->pos--;
		}
		while (p->pos > nlength) {
			dbg_printf ("%c %c", BS, BS);
			p->pos--;
		}
	}
	while (p->pos) {
		dbg_printf ("%c", BS);
		p->pos--;
	}
	dbg_flush ();
	p->length = p->pos = nlength;
	for (i = 0; i < p->length; i++)
		dbg_printf ("%c", lp->buffer [i]);
	dbg_flush ();
	p->line = lp;
}

/* cl_up -- Move the cursor to the end of the previous history command. */

static void cl_up (CmdLine_t *p)
{
	Cmd_t	*lp;

	lp = p->line->prev;
	if (!lp) {
		dbg_printf ("%c", BEL);
		return;
	}
	cl_move (p, lp);
}

/* cl_down -- Move the cursor to the end of the next history command. */

static void cl_down (CmdLine_t *p)
{
	Cmd_t	*lp;

	lp = p->line->next;
	if (!lp) {
		dbg_printf ("%c", BEL);
		return;
	}
	cl_move (p, lp);
}

/* cl_pgup -- Move the cursor to the end of the first history command. */

static void cl_pgup (CmdLine_t *p)
{
	Cmd_t	*lp;

	lp = p->head;
	if (p->line && lp && p->line != lp)
		cl_move (p, lp);
	else
		dbg_printf ("%c", BEL);
}

/* cl_pgdn -- Move the cursor to the end of the last history command. */

static void cl_pgdn (CmdLine_t *p)
{
	Cmd_t	*lp;

	lp = p->tail;
	if (p->line && lp && p->line != lp)
		cl_move (p, lp);
	else
		dbg_printf ("%c", BEL);
}

/* cl_right -- Move the cursor to the left. */

static void cl_left (CmdLine_t *p)
{
	if (!p->pos)
		dbg_printf ("%c", BEL);
	else {
		dbg_printf ("%c", BS);
		p->pos--;
	}
	dbg_flush ();
}

/* cl_right -- Move the cursor to the right. */

static void cl_right (CmdLine_t *p)
{
	if (p->pos == p->length)
		dbg_printf ("%c", BEL);
	else
		dbg_printf ("%c", p->line->buffer [p->pos++]);
	dbg_flush ();
}

/* move_cursor -- Move the cursor in the given direction until either a
		  word character or another character is reached. */

static void move_cursor (CmdLine_t *p, int dir, int wordch)
{
	char	ch;

	for (;;) {
		ch = p->line->buffer [p->pos];
		if ((isalnum (ch) || ch == '_') == (wordch != 0))
			break;

		if (dir < 0) {
			if (!p->pos)
				break;

			cl_left (p);
		}
		else {
			if (p->pos == p->length)
				break;

			cl_right (p);
		}
	}
}

/* cl_word_right -- Do a word left movement. */

static void cl_word_left (CmdLine_t *p)
{
	move_cursor (p, -1, 1);
	move_cursor (p, -1, 0);
}

/* cl_word_right -- Do a word right movement. */

static void cl_word_right (CmdLine_t *p)
{
	move_cursor (p, 1, 1);
	move_cursor (p, 1, 0);
	move_cursor (p, 1, 1);
}

/* cl_backspace -- Process the backspace character. */

static void cl_backspace (CmdLine_t *p)
{
	unsigned	i;

	if (!p->pos)
		dbg_printf ("%c", BEL);
	else {
		p->pos--;
		p->length--;
		dbg_printf ("%c", BS);
		for (i = p->pos; i < p->length; i++) {
			p->line->buffer [i] = p->line->buffer [i + 1];
			dbg_printf ("%c", p->line->buffer [i]);
		}
		dbg_printf (" ");
		for (i = p->pos; i <= p->length; i++)
			dbg_printf ("%c", BS);
		dbg_flush ();
		p->line->buffer [p->length] = '\0';
	}
}

/* cl_control -- Process one of the defined control characters. */

static int cl_control (CmdLine_t *p, char ch)
{
	int	done = 1;

	if (ch == CTRLA)
		cl_home (p);
	else if (ch == CTRLC)
		cl_clear (p);
	else if (ch == CTRLE)
		cl_end (p);
	else if (ch == CTRLH)
		cl_backspace (p);
	else if (ch == CTRLR)
		cl_redraw (p);
	else
		done = 0;
	return (done);
}

/* cl_insert -- Toggle insert/overwrite mode. */

static void cl_insert (CmdLine_t *p)
{
	p->overwrite = !p->overwrite;
}

/* cl_erase -- Erase the character on the current cursor position. */

static void cl_erase (CmdLine_t *p)
{
	unsigned	i;

	if (p->pos < p->length) {
		for (i = p->pos; i < p->length - 1; i++) {
			p->line->buffer [i] = p->line->buffer [i + 1];
			dbg_printf ("%c", p->line->buffer [i]);
		}
		dbg_printf (" ");
		for (i = p->pos; i < p->length; i++)
			dbg_printf ("%c", BS);
		dbg_flush ();
		p->length--;
		p->line->buffer [p->length] = '\0';
	}
}

/* cl_visible -- A visible character is typed. */

static CmdLineStatus_t cl_visible (CmdLine_t *p, char ch)
{
	char		*xp;
	unsigned	i;

	if (!p->overwrite) {
		for (i = p->length; i > p->pos; i--)
			p->line->buffer [i] = p->line->buffer [i - 1];
		p->length++;
	}
	if (p->line->size <= p->pos + 1) {
		xp = realloc (p->line->buffer, p->line->size + LINE_INC);
		if (!xp) {
			dbg_printf ("%c", BEL);
			return (CLS_DONE_ERROR);
		}
		p->line->size += LINE_INC;
		p->line->buffer = xp;
	}
	p->line->buffer [p->pos++] = ch;
	dbg_printf ("%c", ch);
	if (p->pos > p->length) {
		p->length++;
		p->line->buffer [p->length] = '\0';
	}
	if (!p->overwrite && p->pos < p->length) {
		for (i = p->pos; i < p->length; i++)
			dbg_printf ("%c", p->line->buffer [i]);
		for (i = p->pos; i < p->length; i++)
			dbg_printf ("%c", BS);
	}
	dbg_flush ();
	return (CLS_DONE_OK);
}

/* cl_empty_line -- Add an empty line to the end of the history. */

static int cl_empty_line (CmdLine_t *p)
{
	p->line = malloc (sizeof (Cmd_t));
	if (!p->line)
		return (CLS_DONE_ERROR);
	p->line->buffer = malloc (LINE_INC);
	if (!p->line->buffer) {
		free (p->line);
		p->line = NULL;
		return (CLS_DONE_ERROR);
	}

	p->line->prev = p->tail;
	p->line->next = NULL;
	if (!p->head)
		p->head = p->line;
	else
		p->tail->next = p->line;
	p->tail = p->line;
	p->line->buffer [0] = '\0';
	p->line->size = LINE_INC;
	p->last_acc = 0;
	return (CLS_DONE_OK);
}

/* fct1 -- <Esc>'[' followed by 'A'..'E' terminal character. */

static void fct1 (CmdLine_t *p, char ch)
{
	switch (ch) {
		case 'A': /* ^ */
			cl_up (p);
			break;
		case 'B': /* v */
			cl_down (p);
			break;
		case 'C': /* -> */
			cl_right (p);
			break;
		case 'D': /* <- */
			cl_left (p);
			break;
		default:
			dbg_printf ("%c", BEL);
			break;
	}
}

/* fct2 -- <Esc>'['<number>'~' received. */

static void fct2 (CmdLine_t *p)
{
	switch (p->number) {
		case 1: /* Home */
			cl_home (p);
			break;
		case 2: /* Ins */
			cl_insert (p);
			break;
		case 3: /* Del */
			cl_erase (p);
			break;
		case 4: /* End */
			cl_end (p);
			break;
		case 5: /* PgUp */
			cl_pgup (p);
			break;
		case 6: /* PgDn */
			cl_pgdn (p);
			break;
		case 15: /* F5 */
		case 17: /* F6 */
		case 18: /* F7 */
		case 19: /* F8 */
		case 20: /* F9 */
		case 21: /* F10 */
		case 23: /* F11 */
		case 24: /* F12 */
		default:
			dbg_printf ("%c", BEL);
		  	break;
	}
}

/* fct3 -- <Esc>'['<number>';5~' received. */

static void fct3 (CmdLine_t *p)
{
	switch (p->number) {
		case 3: /* Ctrl-Del */
		case 5: /* Ctrl-PgUp */
		case 6: /* Ctrl-PgDn */
		case 15: /* Ctrl-F5 */
		case 17: /* Ctrl-F6 */
		case 18: /* Ctrl-F7 */
		case 19: /* Ctrl-F8 */
		case 20: /* Ctrl-F9 */
		case 21: /* Ctrl-F10 */
		case 23: /* Ctrl-F11 */
		case 24: /* Ctrl-F12 */
		default:
			dbg_printf ("%c", BEL);
		  	break;
	}
}

/* fct4 -- <Esc>'[1;5' followed by 'A'..'D'. */

static void fct4 (CmdLine_t *p, char ch)
{
	switch (ch) {
		case 'C': /* Ctrl--> */
			cl_word_right (p);
			break;
		case 'D': /* Ctrl-<- */
			cl_word_left (p);
			break;
		case 'A': /* Ctrl-^ */
		case 'B': /* Ctrl-v */
		default:
			dbg_printf ("%c", BEL);
			break;
	}
}

/* fct5 -- <Esc>'O' followed by 'A'..'Z'. */

static void fct5 (CmdLine_t *p, char ch)
{
	switch (ch) {
		case 'F': /* End */
			cl_end (p);
			break;
		case 'H': /* Home */
			cl_home (p);
			break;
		case 'P': /* F1 */
		case 'Q': /* F2 */
		case 'R': /* F3 */
		case 'S': /* F4 */
		default:
			dbg_printf ("%c", BEL);
			break;
	}
}

/* fct5 -- <Esc>'O1;5' followed by 'A'..'Z'. */

static void fct6 (CmdLine_t *p, char ch)
{
	ARG_NOT_USED (p)

	switch (ch) {
		case 'P': /* Ctrl-F1 */
		case 'Q': /* Ctrl-F2 */
		case 'R': /* Ctrl-F3 */
		case 'S': /* Ctrl-F4 */
		default:
			dbg_printf ("%c", BEL);
			break;
	}
}

/* cl_add_char -- Process a newly typed character in the command context. */

CmdLineStatus_t cl_add_char (CmdLine_t *p,
			     char      ch,
			     char      **cmd)
{
	static char	eol_ch = '\0';

	/*printf ("<%x>", ch); fflush (stdout);*/
	if (!p->line && cl_empty_line (p) != CLS_DONE_OK)
		return (CLS_DONE_ERROR);

	if (p->state == ES_Esc) {
		if (ch == '[')
			p->state = ES_Esc_LSq;
		else if (ch == 'O')
			p->state = ES_Esc_O;
		else {

		    inv_char:
			p->state = ES_Normal;
			dbg_printf ("%c", BEL);
		}
		return (CLS_INCOMPLETE);
	}
	else if (p->state == ES_Esc_LSq) {	/* Got <Esc>'[' */
		if (ch >= 'A' && ch <= 'E') {
			fct1 (p, ch);
			p->state = ES_Normal;
		}
		else if (ch >= '0' && ch <= '9') {
			p->number = ch - '0';
			p->state = ES_Esc_LSq_Num;
		}
		else
			goto inv_char;

		return (CLS_INCOMPLETE);
	}
	else if (p->state == ES_Esc_LSq_Num) {	/* Got <Esc>'['<number> */
		if (ch >= '0' && ch <= '9')
			p->number = p->number * 10 + ch - '0';
		else if (ch == '~') {
			fct2 (p);
			p->state = ES_Normal;
		}
		else if (ch == ';')
			p->state = ES_Esc_LSq_Num_Semi;
		else
			goto inv_char;

		return (CLS_INCOMPLETE);
	}
	else if (p->state == ES_Esc_LSq_Num_Semi) { /* Got <Esc>'['<number>';'*/
		if (ch == '5')
			p->state = ES_Esc_LSq_Num_Semi_5;
		else
			goto inv_char;

		return (CLS_INCOMPLETE);
	}
	else if (p->state == ES_Esc_LSq_Num_Semi) { /* Got <Esc>'['<number>';5'*/
		if (ch == '~')
			fct3 (p);
		else if (ch >= 'A' && ch <= 'D' && p->number == 1)
			fct4 (p, ch);
		else
			goto inv_char;

		p->state = ES_Normal;
		return (CLS_INCOMPLETE);
	}
	else if (p->state == ES_Esc_O) {	/* Got <Esc>'O' */
		if (ch >= 'A' && ch <= 'Z') {
			fct5 (p, ch);
			p->state = ES_Normal;
		}
		else if (ch == '1')
			p->state = ES_Esc_O1;
		else
			goto inv_char;

		return (CLS_INCOMPLETE);
	}
	else if (p->state == ES_Esc_O1)		/* Got <Esc>'O1' */
		if (ch == ';') {
			p->state = ES_Esc_O1_Semi;
			return (CLS_INCOMPLETE);
		}
		else
			goto inv_char;

	else if (p->state == ES_Esc_O1_Semi)	/* Got <Esc>'O1;' */
		if (ch == '5') {
			p->state = ES_Esc_O1_Semi_5;
			return (CLS_INCOMPLETE);
		}
		else
			goto inv_char;

	else if (p->state == ES_Esc_O1_Semi_5) { /* Got <Esc>'O1;5' */
		if (ch >= 'A' && ch <= 'Z') {
			fct6 (p, ch);
			p->state = ES_Normal;
			return (CLS_INCOMPLETE);
		}
		else
			goto inv_char;
	}
	if (ch == ESC)
		p->state = ES_Esc;
	else if (ch == CR) {
		if (p->length) {
			p->line->buffer [p->length] = '\0';
			*cmd = p->line->buffer;
		}
		else
			*cmd = &eol_ch;
		cl_eol (p);
		return (CLS_DONE_OK);
	}
	else if (ch == LF)
		return (CLS_INCOMPLETE);

	else if (ch == BACKSPACE) {
		if (p->length)
			cl_backspace (p);
		else
			dbg_printf ("%c", BEL);
	}
#ifndef UNIX_TERM
	else if (ch == DEL)
		cl_erase (p);
#endif
	else if (ch <= CTRLR)
		cl_control (p, ch);
	else if (p->length < MAX_CMD_LENGTH)
		cl_visible (p, ch);
	else
		dbg_printf ("%c", BEL);
	return (CLS_INCOMPLETE);
}

