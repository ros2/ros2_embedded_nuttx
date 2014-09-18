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

/* dds_debug.h -- Externally accessible logging, debugging and trace functions.

   Note that the resulting actions depend on whether or not the requested
   functionality is effectively enabled in the DDS library:

   - The DDS_Debug functions will typically only be effective when the library
     was compiled with the -DDDS_DEBUG flag.
   - The DDS_CTrace functions will only be effective when the library was
     compiled with the -DCTRACE_USED flag. */

#ifndef __dds_debug_h_
#define __dds_debug_h_

#include "dds/dds_error.h"
#ifdef XTYPES_USED
#include "dds/dds_xtypes.h"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef __GNUC__
#define	__attribute__(x)	/*NOTHING*/
#endif

/* === DDS Logging ========================================================== */

/* Enable log output to standard out. */
DDS_EXPORT void DDS_Log_stdio (int enable);

/* Assign user log function strings. */
DDS_EXPORT void DDS_Log_strings (
	const char *fcts []
);

/* === Debug shell support functions ======================================== */

/* Start the DDS Trace & Debug shell. */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_start (void);

/* Start debugging with the given function as top-level debug handler.
   If command is intended for Debug shell processing, user should dispatch to
   DDS_Debug_input(). */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_start_fct (
	HANDLE fd,
	void (*fct) (HANDLE fd,
		     short events,
		     void *udata)
);

/* Notify debug shell that input characters ready for processing. */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_input (
	HANDLE fd,
	short events,
	void *udata
);

#define DDS_DEBUG_PORT_OFS	3400	/* Port users will be connecting to. */
#define	DDS_DEBUG_PORT_DEFAULT	0	/* Select default DDS port. */

/* Start a DDS Debug server for the number of clients. */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_server_start (
	unsigned nclients,
	unsigned short port
);

/* Enable the 'quit' command in the Trace & Debug shell.
   If this command is given, the *abort_program will be set. */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_abort_enable (
	int *abort_program
);

/* Enable the 'pause', 'resume' and 'delay' commands.
   If the 'pause' command is given, *pause will be set.
   If the 'resume [<n>]' command is given, *pause will be cleared and
   *nsteps will be updated if an argument to 'resume' was given.
   If the 'delay <n> command is given, *delay will be set to the given nymber.*/
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_control_enable (
	int *pause_cmd,
	unsigned *nsteps,
	unsigned *delay
);

/* Enable the 'menu' command. */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_menu_enable (
	int *menu
);

/* Invoke a debug shell command. */
DDS_EXPORT DDS_ReturnCode_t DDS_Debug_command (
	const char *buf
);

/* Display debug shell commands list. */
DDS_EXPORT void DDS_Debug_help (void);

/* Dump a DDS reader or writer cache. */
DDS_EXPORT void DDS_Debug_cache_dump (
	void *ep
);

/* Dump a DDS reader or writer proxy. */
DDS_EXPORT void DDS_Debug_proxy_dump (
	void *ep
);

/* Dump a topic. */
DDS_EXPORT void DDS_Debug_topic_dump (
	unsigned domain,
	const char *name
);

/* Dump the pool contents. */
DDS_EXPORT void DDS_Debug_pool_dump (
	int wide
);

/* Dump discovery info. */
DDS_EXPORT void DDS_Debug_disc_dump (void);

/* Dump all registered types. */
DDS_EXPORT void DDS_Debug_type_dump (
	unsigned scope,
	const char *name
);

/* Dump a static Data item. */
DDS_EXPORT void DDS_Debug_dump_static (
	unsigned indent,
	DDS_TypeSupport ts,
	void *data,
	int key_only,
	int secure,
	int field_names
);

/* Dump a dynamic Data item. */
DDS_EXPORT void DDS_Debug_dump_dynamic (
	unsigned indent,
	DDS_DynamicTypeSupport ts,
	DDS_DynamicData data,
	int key_only,
	int secure,
	int field_names
);

/* === RTPS tracing support functions ======================================= */

#define	DDS_RTRC_FTRACE	1	/* Enable frame tracing. */
#define	DDS_RTRC_STRACE	2	/* Enable signal tracing. */
#define	DDS_RTRC_ETRACE	4	/* Enable state tracing. */
#define	DDS_RTRC_TTRACE	8	/* Enable timer tracing. */

#define	DDS_TRACE_NONE		0
#define	DDS_TRACE_ALL		(DDS_RTRC_FTRACE | DDS_RTRC_STRACE | \
				 DDS_RTRC_ETRACE | DDS_RTRC_TTRACE)

#define	DDS_TRACE_ALL_ENDPOINTS	NULL
#define	DDS_TRACE_MODE_TOGGLE	0xff

DDS_EXPORT DDS_ReturnCode_t DDS_Trace_set (
	DDS_Entity entity,
	unsigned mode
);

DDS_EXPORT DDS_ReturnCode_t DDS_Trace_get (
	DDS_Entity entity,
	unsigned *mode
);

DDS_EXPORT void DDS_Trace_defaults_set (
	unsigned mode
);

DDS_EXPORT void DDS_Trace_defaults_get (
	unsigned *mode
);

/* === Cyclic trace support functions ======================================= */

/* Start tracing. */
DDS_EXPORT DDS_ReturnCode_t DDS_CTrace_start (void);

/* Stop tracing. */
DDS_EXPORT void DDS_CTrace_stop (void);

/* Clear the trace buffer. */
DDS_EXPORT void DDS_CTrace_clear (void);

/* Set the trace mode to either cyclic (cyclic=1) or stop on full (cyclic=0). */
DDS_EXPORT DDS_ReturnCode_t DDS_CTrace_mode (
	int cyclic
);

/* Reentrant cyclic tracing function.  The index parameter is a user specific 
   index. The data/length arguments can be used to add extra binary data. */
DDS_EXPORT void DDS_CTrace_printd (
	unsigned index,
	const void *data,
	size_t length
);

/* Same as DDS_CTrace_printd, but with extra formatted arguments. */
DDS_EXPORT void DDS_CTrace_printf (
	unsigned index,
	const char *fmt, ...
)
__attribute__((format(printf, 2, 3)));

/* Start a long sample, consisting of a number of binary data chunks. */
DDS_EXPORT void DDS_CTrace_begind (
	unsigned index,
	const void *data,
	size_t length
);

/* Continue with the next binary data chunk. */
DDS_EXPORT void DDS_CTrace_contd (
	const void *data,
	size_t length
);

/* All data chunks given, store the data. */
DDS_EXPORT void DDS_CTrace_endd (void);

#ifdef  __cplusplus
}
#endif

#endif /* !__dds_debug_h_ */


