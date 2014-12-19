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

/* cmdline.h -- Command line handler for the debug shell. */

#ifndef __cmdline_h_
#define __cmdline_h_

typedef enum {
	CLS_INCOMPLETE,	/* Partial command -- needs more input. */
	CLS_DONE_OK,	/* Done, command entered correctly. */
	CLS_DONE_ERROR	/* Done, error occurred. */
} CmdLineStatus_t;

typedef struct cmd_line_st CmdLine_t;

CmdLine_t *cl_new (void);

/* Create a new command line context. */

void cl_load (CmdLine_t *p, const char *filename);

/* Load the command history with the contents of a file. */

void cl_save (CmdLine_t *p, const char *filename);

/* Save the current command history in a file. */

void cl_delete (CmdLine_t *p);

/* Completely dispose of a command line context. */

CmdLineStatus_t cl_add_char (CmdLine_t *p,
			     char      ch,
			     char      **cmd);

/* Add a typed character to the command line.  If a command is completed, the
   *cmd argument will be set to the command string and CLS_DONE_OK will be
   returned.  If not completed yet, CLS_INCOMPLETE will be returned.
   If errors occurred, CLS_DONE_ERROR will be returned. */

#endif /* !__cmdline_h_ */

