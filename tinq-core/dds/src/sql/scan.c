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

/* scan.c -- Implements the SQL subset token scanner. */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#include <inttypes.h>
#endif
#include "win.h" 
#include "hash.h"
#include "error.h" 
#include "pool.h" 
#include "scan.h"
#include "debug.h"

#define	MAXHASH	16

typedef struct hnode_st HashNode;
struct hnode_st {
	HashNode	*next;
	const char	*word;
	unsigned	length;
	Token		token;
};

static unsigned		nkeywords;
static HashNode		*htable [MAXHASH];
static HashNode		keywords [] = {
	{ NULL, "ORDER",   5, TK_ORDER   },
	{ NULL, "BY",      2, TK_BY      },
	{ NULL, "SELECT",  6, TK_SELECT  },
	{ NULL, "FROM",    4, TK_FROM    },
	{ NULL, "AS",      2, TK_AS      },
	{ NULL, "INNER",   5, TK_INNER   },
	{ NULL, "NATURAL", 7, TK_NATURAL },
	{ NULL, "JOIN",    4, TK_JOIN    },
	{ NULL, "WHERE",   5, TK_WHERE   },
	{ NULL, "AND",     3, TK_AND     },
	{ NULL, "OR",      2, TK_OR      },
	{ NULL, "NOT",     3, TK_NOT     },
	{ NULL, "BETWEEN", 7, TK_BETWEEN },
	{ NULL, "LIKE",    4, TK_LIKE    }
};

#define KW_HASH(sp,len)		hashf((const unsigned char *) sp,len) & (MAXHASH - 1)

/* add_keyword -- Add a token to the token hash table. */

void add_keyword (HashNode *np)
{
	unsigned	h = KW_HASH (np->word, np->length);

	np->next = htable [h];
	htable [h] = np;
}

/* sql_init_scan -- Initialize scanner data structures. */

int sql_init_scan (void)
{
	unsigned	i;
	HashNode	*p;

	if (!nkeywords)
		for (i = 0, p = keywords;
		     i < sizeof (keywords) / sizeof (HashNode);
		     i++, p++) {
			add_keyword (p);
			nkeywords++;
		}
	return (0);
}

/* sql_init_scan -- Prepare to scan tokens from the given SQL input string. */

int sql_start_scan (const char *s, ScanData *sdp)
{
	if (!s)
		return (1);

	sdp->cch = *s++;
	sdp->instr = s;
	return (0);
}

/* nextch -- Get the next SQL input character from the input string. */

#define	nextch(sdp)	if (sdp->cch) sdp->cch=*sdp->instr++
#define peekch(sdp)	(sdp->cch) ? *sdp->instr: sdp->cch

#if 0
static void nextch (ScanData *sdp)
{
	if (!sdp->cch)
		return;

	sdp->cch = *sdp->instr++;
}
#endif

/* sql_keyword -- Returns either a keyword token or TK_ID, depending on whether
		  or not the given identifier is a token. */

static Token lookup_keyword (const char *name, unsigned length)
{
	unsigned	h, i;
	HashNode	*np;
	char		buf [8], ch;

	if (length >= sizeof (buf))
		return (TK_ID);

	for (i = 0; i < length; i++) {
		ch = name [i];
		if (ch >= 'a' && ch <= 'z')
			ch -= 'a' - 'A';
		buf [i] = ch;
	}
	buf [length] = '\0';
	h = KW_HASH (buf, length);
	for (np = htable [h]; np; np = np->next)
		if (np->length == length && !memcmp (np->word, buf, length))
			return (np->token);

	return (TK_ID);
}

static int sql_enlarge_buffer (ScanData *sdp)
{
	char		*nbp;

	if (sdp->dyn_buf)
		nbp = xrealloc (sdp->buffer, sdp->buf_size + 64);
	else {
		nbp = xmalloc (sdp->buf_size + 64);
		if (nbp)
			memcpy (nbp, sdp->buffer, sdp->buf_size);
	}
	if (!nbp) {
		warn_printf ("sql_enlarge_buffer: out-of-memory for string buffer!");
		return (0);
	}
	sdp->buffer = nbp;
	sdp->buf_size += 64;
	sdp->dyn_buf = 1;
	return (1);
}

#define	CHECKBUF(sdp,i,req)	if(i >= sdp->buf_size - req - 1 && 	\
				   !sql_enlarge_buffer (sdp)) {		\
					sdp->token = TK_EOL; return; }

/* sql_next_token -- Get the next SQL token from the input string. */

void sql_next_token (ScanData *sdp)
{
	int		hex, negate, param, is_float;
	unsigned	n, i;
	char		nch;

	/* Skip whitespace characters (' ' or '\t' or '\r' or '\n'). */
	while (isspace ((unsigned char) sdp->cch))
		nextch (sdp);

	/* Check first character of token. */
	nch = peekch (sdp);
	if (!sdp->cch)
		sdp->token = TK_EOL;	/* End-of-line! */

	else if (sdp->cch == '-' ||
		 (sdp->cch == '%' && isdigit ((unsigned char) nch)) ||
	         isdigit ((unsigned char) sdp->cch) ||
	         (sdp->cch == '.' &&
		  isdigit ((unsigned char) nch))) { /* Number */
		hex = 0;
		negate = 0;
		param = 0;
		if (sdp->cch == '-') {
			nextch (sdp);
			negate = 1;
			nch = peekch (sdp);
			if ((!isdigit ((unsigned char) sdp->cch) && 
			     sdp->cch != '.') ||
			    (sdp->cch == '.' &&
			     !isdigit ((unsigned char) nch))) {
				sdp->token = TK_INVALID;
				return;
			}
		}
		else if (sdp->cch == '0' && (nch == 'x' || nch == 'X')) {
			nextch (sdp);
			nextch (sdp);
			hex = 1;
		}
		else if (sdp->cch == '%') {
			nextch (sdp);
			param = 1;
		}
		sdp->integer = 0L;
		i = 0;
		if (sdp->cch != '.') {
			do {
				if (hex) {
					if (isdigit ((unsigned char) sdp->cch))
						n = sdp->cch - '0';
					else if (sdp->cch >= 'a' && sdp->cch <= 'f')
						n = sdp->cch - 'a' + 10;
					else
						n = sdp->cch - 'A' + 10;
					sdp->integer = (sdp->integer << 4) | n;
				}
				else {
					CHECKBUF (sdp, i, 2);
					sdp->buffer [i++] = sdp->cch;
					sdp->integer = sdp->integer * 10 + (sdp->cch - '0');
				}
				nextch (sdp);
			}
			while ((!hex && isdigit ((unsigned char) sdp->cch)) ||
			       (hex && isxdigit ((unsigned char) sdp->cch)));
		}
		else
			sdp->buffer [i++] = '0';
		sdp->token = TK_PARAM;
		sdp->ptype = PT_INT;
		CHECKBUF (sdp, i, 8);
		if (!hex && !param) {
			is_float = 0;
			if (sdp->cch == '.') { /* Floating point! */
				sdp->buffer [i++] = '.';
				nextch (sdp);
				while (isdigit ((unsigned char) sdp->cch)) {
					CHECKBUF (sdp, i, 2);
					sdp->buffer [i++] = sdp->cch;
					nextch (sdp);
				}
				if (sdp->buffer [i - 1] == '.')
					sdp->buffer [i++] = '0';
				is_float = 1;
			}
			if (i < sdp->buf_size - 4 &&
			    (sdp->cch == 'e' || sdp->cch == 'E')) {
				sdp->buffer [i++] = 'e';
				nextch (sdp);
				if (sdp->cch == '+' || sdp->cch == '-') {
					sdp->buffer [i++] = sdp->cch;
					nextch (sdp);
				}
				while (isdigit ((unsigned char) sdp->cch)) {
					CHECKBUF (sdp, i, 2);
					sdp->buffer [i++] = sdp->cch;
					nextch (sdp);
				}
				is_float = 1;
			}
			if (is_float) {
				sdp->buffer [i] = '\0';
				if (sscanf (sdp->buffer, "%lf", &sdp->float_num) != 1)
					sdp->token = TK_INVALID;
				else {
					if (negate)
						sdp->float_num = -sdp->float_num;
					sdp->ptype = PT_FLOAT;
				}
			}
		}
		if (negate && sdp->ptype == PT_INT)
			sdp->integer = -sdp->integer;
		else if (param) {
			if (sdp->integer < 100) {
				sdp->ptype = PT_PARAM;
				sdp->par_index = (unsigned) sdp->integer;
			}
			else
				sdp->token = TK_INVALID;
		}
	}
	else if (isalpha ((unsigned char) sdp->cch) || 
		 sdp->cch == '_') { /* Identifier or keyword. */
		i = 0;
		do {
			CHECKBUF (sdp, i, 2);
			sdp->buffer [i++] = sdp->cch;
			nextch (sdp);
		}
		while (isalnum ((unsigned char) sdp->cch) || sdp->cch == '_');
		sdp->buffer [i] = '\0';
		sdp->token = lookup_keyword (sdp->buffer, i);
		if (sdp->token == TK_ID) {
			sdp->ident = sdp->buffer;
			sdp->length = i;
		}
	}
	else if (sdp->cch == '`' || sdp->cch == '\'') { /* String. */
		nextch (sdp);
		i = 0;
		while (sdp->cch && sdp->cch != '\'') {
			CHECKBUF (sdp, i, 2);
			sdp->buffer [i++] = sdp->cch;
			nextch (sdp);
		}
		if (sdp->cch == '\'') {
			sdp->token = TK_PARAM;
			if (i == 1) {
				sdp->ptype = PT_CHAR;
				sdp->character = sdp->buffer [0];
			}
			else {
				sdp->ptype = PT_STRING;
				sdp->buffer [i] = '\0';
				sdp->string = sdp->buffer;
				sdp->length = i;
			}
			nextch (sdp);
		}
		else
			sdp->token = TK_EOL;
	}
	else
		switch (sdp->cch) {
			case '.':
				nextch (sdp);
				sdp->token = TK_DOT;
				break;
			case ';':
				nextch (sdp);
				sdp->token = TK_SEMI;
				break;
			case ',':
				nextch (sdp);
				sdp->token = TK_COMMA;
				break;
			case '*':
				nextch (sdp);
				sdp->token = TK_ALL;
				break;
			case '(':
				nextch (sdp);
				sdp->token = TK_LPAR;
				break;
			case ')':
				nextch (sdp);
				sdp->token = TK_RPAR;
				break;
			case '=':
				nextch (sdp);
				sdp->token = TK_EQ;
				break;
			case '>':
				nextch (sdp);
				if (sdp->cch == '=') {
					sdp->token = TK_GE;
					nextch (sdp);
				}
				else
					sdp->token = TK_GT;
				break;
			case '<':
				nextch (sdp);
				if (sdp->cch == '>') {
					sdp->token = TK_NE;
					nextch (sdp);
				}
				else if (sdp->cch == '=') {
					sdp->token = TK_LE;
					nextch (sdp);
				}
				else
					sdp->token = TK_LT;
				break;
			default:
				sdp->token = TK_INVALID;
				nextch (sdp);
				break;
		}
}

static const char *token_str [] = {
	"<eol>", "<", "<id:", 
	".", ";", ",", "*", "(", ")", "=", ">", ">=", "<", "<=", "<>",
	"ORDER", "BY", "SELECT", "FROM", "AS", "INNER", "NATURAL",
	"JOIN", "WHERE", "AND", "OR", "NOT", "BETWEEN", "LIKE",
	"<invalid>"
};

/* Return a token description string. */

const char *sql_token_str (Token t)
{
	static char buf [10];

	strncpy (buf, token_str [t], sizeof (buf));
	return (buf);
}

void sql_dump_token (ScanData *sdp)
{
	printf ("%s", token_str [sdp->token]);
	if (sdp->token == TK_PARAM) {
		switch (sdp->ptype) {
			case PT_INT:
				printf ("%" PRId64, sdp->integer);
				break;
			case PT_CHAR:
				printf ("'%c'", *sdp->string);
				break;
			case PT_FLOAT:
				printf ("%f", sdp->float_num);
				break;
			case PT_STRING:
				printf ("\"%s\"", sdp->string);
				break;
			case PT_PARAM:
				printf ("%%%u", sdp->par_index);
				break;
		  	break;
		}
		printf (">");
	}
	else if (sdp->token == TK_ID)
		printf ("%s>", sdp->ident);
	printf (" ");
}

#ifdef SCAN_DEBUG

int main (void)
{
	unsigned	n = 0;
	ScanData	data;

	sql_init_scan ();
	sql_start_scan ("0 03 005 0x0 0x12 0xff -789 -.5 .79e-5 32e12 34.e7"
		        "`hoi there!' 'bye' `x'"
		        "%12 %99 %0"
		        "blabla order Order as JOiN"
		        ".;,*()=>>=<<=<>"
		        "ORDER BY SELECT AS INNER NATURAL JOIN "
		        "WHERE AND OR NOT BETWEEN LIKE", &data);

	do {
		sql_next_token (&data);
		sql_dump_token (&data);
		if ((++n & 7) == 0)
			printf ("\r\n");
	}
	while (data.token != TK_EOL);

	sql_start_scan ("SELECT flight_name, x, y, z AS height "
		        "FROM `Location' NATURAL JOIN 'FlightPlan' "
		        "WHERE height < 1000 AND x <23;", &data);

	do {
		sql_next_token (&data);
		sql_dump_token (&data);
		if ((++n & 7) == 0)
			printf ("\r\n");
	}
	while (data.token != TK_EOL);

	printf ("\r\n");
}

#endif


