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

/* debug_c -- DDS debug module. */

#ifndef __debug_h_
#define	__debug_h_

#include "timer.h"
#include "rtps.h"
#include "sock.h"

// Avoid conflicts with dbg_printf
#define PRId64	"lld"
#define PRIu64	"llu"

/* Some utility functions: */

void dbg_print_uclist (unsigned nchars, const unsigned char *cp, int hex);

/* Display an unsigned char list in the format: {x,y,z} in either decimal or
   hexadecimal format. */

void dbg_print_entity_id (const char *name, const EntityId_t *ep);

/* Display an entity Id preceded with '<name>:' if name is non-NULL. */

void dbg_print_locator (const Locator_t *lp);

/* Display a locator. */

void dbg_print_guid_prefix (const GuidPrefix_t *gp);

/* Display a GUID prefix. */

void dbg_print_guid (const GUID_t *gp);

/* Display a GUID. */

void dbg_print_ticks (const char *name, Ticks_t ticks);

/* Display a time in ticks. */

void dbg_print_timer (const Timer_t *tp);

/* Display the remaining value of a timer. */


/* Externally accessible functions: */

void debug_start (void);

/* Start the DDS Trace & Debug shell. */

void debug_start_fct (HANDLE fd,
		      void (*fct) (HANDLE fd, short events, void *udata));

/* Start debugging with the given handle and function as top-level debug
   handler. If a command is intended for Debug shell processing, user should
   dispatch to DDS_Debug_input(). */

void debug_input (HANDLE fd, short events, void *udata);

/* Notify debug shell that input characters ready for processing. */

int debug_server_start (unsigned nclients, unsigned short port);

/* Start a DDS Debug server for the number of clients. */

void debug_suspend (void);

/* Suspend the DDS Debug server. */

void debug_resume (void);

/* Resume the DDS Debug server. */

void debug_abort_enable (int *abort_program);

/* Enable the 'quit' command in the Trace & Debug shell.
   If this command is given, the *abort_program will be set. */

void debug_control_enable (int *pause_cmd, unsigned *nsteps, unsigned *delay);

/* Enable the 'pause', 'resume' and 'delay' commands.
   If the 'pause' command is given, *pause will be set.
   If the 'resume [<n>]' command is given, *pause will be cleared and
   *nsteps will be updated if an argument to 'resume' was given.
   If the 'delay <n> command is given, *delay will be set to the given nymber.*/

void debug_menu_enable (int *menu);

/* Enable the 'menu' command. */

void debug_command (const char *buf);

/* Invoke a debug shell command. */

void debug_help (void);

/* Show debug shell commands. */

void debug_cache_dump (void *ep);

/* Dump a DDS reader or writer cache. */

void debug_proxy_dump (void *ep);

/* Dump a DDS reader or writer proxy. */

void debug_topic_dump (unsigned domain, const char *name);

/* Dump a topic. */

void debug_pool_dump (int wide);

/* Dump the pool contents. */

void debug_disc_dump (void);

/* Dump discovery info. */

void debug_type_dump (unsigned scope, const char *name);

/* Dump all registered types. */

void debug_log (const char *s, int nl);

/* Output a logging string to the requested debug connections. */

char *dbg_poll_event_str (short events);

/* Return a string with the poll events expanded (non-reentrant). */

#endif

