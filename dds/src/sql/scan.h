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

/* scan.h -- Defines the various tokens and the token scanner when parsing an
	     SQL-subset string as used in DDS. */

#ifndef __scan_h_
#define	__scan_h_

#include <stdint.h>

/* Parameter types: */
typedef enum {
	PT_INT,		/* Integer value (['-']|['0x']){0-9}*. */
	PT_CHAR,	/* Character value. */
	PT_FLOAT,	/* Floating point value. */
	PT_STRING,	/* String value '`'|'''.*'''. */
	PT_PARAM	/* Parameter '%nn' where 0 <= nn <= 99. */
} ParamType;

/* Parameter types are split off from the token definition for two reasons:

	1. They're always used atomically as a Parameter in the grammar.
	2. This allows for a compact token set as a 32-bit word, making the use
	   of first/follow more efficient in the parser.
*/

/* Token types: */
typedef enum {
	TK_EOL,		/* End-of-line */

	TK_PARAM,	/* Any parameter type: Int/Char/Float/Str/Par. */
	TK_ID,		/* Identifier. */

	TK_DOT,		/* '.' */
	TK_SEMI,	/* ';' */
	TK_COMMA,	/* ',' */
	TK_ALL,		/* '*' */
	TK_LPAR,	/* '(' */
	TK_RPAR,	/* ')' */
	TK_EQ,		/* '=' */
	TK_GT,		/* '>' */
	TK_GE,		/* '>=' */
	TK_LT,		/* '<' */
	TK_LE,		/* '<=' */
	TK_NE,		/* '<>' */

	TK_ORDER,	/* 'ORDER' */
	TK_BY,		/* 'BY' */
	TK_SELECT,	/* 'SELECT' */
	TK_FROM,	/* 'FROM' */
	TK_AS,		/* 'AS' */
	TK_INNER,	/* 'INNER' */
	TK_NATURAL,	/* 'NATURAL' */
	TK_JOIN,	/* 'JOIN' */
	TK_WHERE,	/* 'WHERE' */
	TK_AND,		/* 'AND' */
	TK_OR,		/* 'OR' */
	TK_NOT,		/* 'NOT' */
	TK_BETWEEN,	/* 'BETWEEN' */
	TK_LIKE,	/* 'LIKE' */

	TK_INVALID	/* Invalid token! */
} Token;

typedef struct scan_data_st {
	Token		token;		/* Found Token. */
	ParamType	ptype;		/* Parameter type if TK_PARAM. */
	int64_t		integer;	/* Integer:      TK_PARAM::PT_INT */
	char		character;	/* Character:    TK_PARAM::PT_CHAR. */
	double		float_num;	/* Float:        TK_PARAM::PT_FLOAT. */
	unsigned	par_index;	/* Parameter idx:TK_PARAM::PT_PARAM. */
	char		*string;	/* String value: TK_PARAM::PT_STRING. */
	char		*ident;		/* Identifier if TK_ID. */
	unsigned	length;		/* String/identifier length. */
	const char	*instr;		/* Input string. */
	char		cch;		/* Current character. */
	char		*buffer;	/* String buffer. */
	size_t		buf_size;	/* Size of string buffer. */
	int		dyn_buf;	/* Dynamic buffer? */
} ScanData;


int sql_init_scan (void);

/* Initialize scanner data. */

int sql_start_scan (const char *s, ScanData *sdp);

/* Prepare to scan tokens from the given SQL input string. */

void sql_next_token (ScanData *sdp);

/* Get the next SQL token from the input string. */

const char *sql_token_str (Token t);

/* Return a token description string. */

void sql_dump_token (ScanData *sdp);

/* Dump the current token. */

#endif /* __scan_h_ */

