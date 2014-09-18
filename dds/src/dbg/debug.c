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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef _WIN32
#include "win.h"
#include "ws2tcpip.h"
#else
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <string.h>
#include "libx.h"
#include "thread.h"
#include "tty.h"
#include "config.h"
#include "str.h"
#include "log.h"
#include "error.h"
#include "pool.h"
#include "sock.h"
#include "str.h"
#include "timer.h"
#include "ctrace.h"
#include "cmdline.h"
#include "prof.h"
#include "skiplist.h"
#include "db.h"
#include "typecode.h"
#include "locator.h"
#ifdef XTYPES_USED
#include "xtypes.h"
#include "xdata.h"
#endif
#include "uqos.h"
#include "dds.h"
#include "dynip.h"
#include "ri_data.h"
#include "rtps.h"
#include "rtps_cfg.h"
#include "rtps_ip.h"
#include "disc.h"
#include "dcps.h"
#ifdef DDS_SECURITY
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#include "sec_data.h"
#include "sec_crypto.h"
#include "sec_plugin.h"
#else
#include "msecplug/msecplug.h"
#endif
#endif
#ifdef DDS_FORWARD
#include "rtps_fwd.h"
#include "rtps_data.h"
#endif
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"
#include "debug.h"

#ifdef DDS_DEBUG

/*#define EVTRACE	** If defined, traces received events! */
/*#define CHTRACE	** If defined, character input is traced! */

#define	MAX_SESSIONS	4

typedef struct dbg_session {
#ifdef _WIN32
	int			sk_session;
	SOCKET			sk;
#endif
	HANDLE			in_fd, out_fd;
	unsigned		session;
	unsigned long		log_events;
	ssize_t			nchars;
	struct sockaddr_in	remote;
	CmdLine_t		*cmdline;
} DebugSession_t;

static DebugSession_t	*dbg_sessions [MAX_SESSIONS];
static DebugSession_t	*session;
static int		*aborting, *paused, *menu;
static unsigned		*max_events, *sleep_time, taflags = 15;
static SOCKET		dbg_server_fd;
static unsigned short	dbg_server_port;
static unsigned		dbg_server_clients;
static int		dbg_suspended;


void debug_help (void)
{
	const char *data;

	dbg_printf ("\tssys                  Display system-specific data.\r\n");
	dbg_printf ("\tstimer                Display the timers.\r\n");
	dbg_printf ("\tsstr                  Display the string cache.\r\n");
	dbg_printf ("\tspool                 Display the pools.\r\n");
	dbg_printf ("\tspoola                Display the pools (extended).\r\n");
#ifdef RTPS_USED
	dbg_printf ("\tscx [<cx>]            Display connections.\r\n");
	dbg_printf ("\tscxa [<cx>]           Display connections (extended).\r\n");
	dbg_printf ("\tscxq                  Display queued connections.\r\n");
#endif
	dbg_printf ("\tsloc                  Display locators.\r\n");
	dbg_printf ("\tsconfig               Display configuration data.\r\n");
	dbg_printf ("\tsdomain <d> <lf> <rf> Display domain (d) info.\r\n");
	dbg_printf ("\t                      <lf> and <rf> are bitmaps for local/remote info.\r\n");
	dbg_printf ("\t                      1=Locator, 2=Builtin, 4=Endp, 8=Type, 10=Topic.\r\n");
	dbg_printf ("\tsdisc                 Display discovery info.\r\n");
	dbg_printf ("\tsdisca                Display all discovery info (sdisc + endpoints)\r\n");
#ifdef XTYPES_USED
	dbg_printf ("\tstype [<name>]        Display Type information.\r\n");
#endif
	dbg_printf ("\tsqos                  Display QoS parameters.\r\n");
	dbg_printf ("\tstopic <d> [<name>]   Display Topic information.\r\n");
	dbg_printf ("\tsendpoints            Display the DCPS/RTPS Readers/Writers.\r\n");
#ifdef RTPS_USED
	dbg_printf ("\tscache <ep>           Display an RTPS Endpoint Cache.\r\n");
#endif
	dbg_printf ("\tsdcache <ep>          Display a DCPS Endpoint Cache.\r\n");
	dbg_printf ("\tqcache <ep> [<query>] Query cache data of the specified endpoint:\r\n");
	dbg_printf ("\t                      where: <ep>: endpoint, <query>: SQL Query string.\r\n");
#ifdef RTPS_USED
	dbg_printf ("\tsproxy [<ep>]         Display Proxy contexts.\r\n");
#ifdef RTPS_PROXY_INST
	dbg_printf ("\trproxy [<ep>]         Restart Proxy context.\r\n");
#endif
#endif
	dbg_printf ("\tseqos <ep>            Display endpoint QoS parameters.\r\n");
#ifdef DDS_NATIVE_SECURITY
	dbg_printf ("\tscrypto <ep>          Display entity crypto parameters.\r\n");
	dbg_printf ("\tsscache               Display security cache.\r\n");
	dbg_printf ("\trehs                  Request a rehandshake.\r\n");
#endif
#ifdef RTPS_USED
	dbg_printf ("\tsrx                   Display the RTPS Receiver context.\r\n");
	dbg_printf ("\tstx                   Display the RTPS Transmitter context.\r\n");
#endif
#ifndef _WIN32
	dbg_printf ("\tsfd                   Display the status of the file descriptors.\r\n");
#endif
	if (paused || (session && session->in_fd != tty_stdin)) {
		if (session && session->in_fd != tty_stdin)
			data = "logging";
		else
			data = "traffic";
		dbg_printf ("\tpause                 Pause %s.\r\n", data);
		dbg_printf ("\tresume [<n>]          Resume %s [for <n> samples].\r\n", data);
	}
	if (sleep_time)
		dbg_printf ("\tdelay <n>             Set sleep time in ms.\r\n");
	dbg_printf ("\tasp <d>               Assert participant.\r\n");
	dbg_printf ("\tase <ep>              Assert writer endpoint.\r\n");
#ifdef DDS_DYN_IP
	dbg_printf ("\tsdip                  Display the dynamic IP state.\r\n");
#endif
#ifdef DDS_SECURITY
	dbg_printf ("\tdtls                  Display DTLS connection related info.\r\n");
#ifdef DDS_NATIVE_SECURITY
	dbg_printf ("\tspdb                  Display the policy database.\r\n");
#ifdef DDS_QEO_TYPES
	dbg_printf ("\tspv                   Display the policy version numbers.\r\n");
#endif
#endif
#endif
#ifdef RTPS_USED
#ifdef DDS_FORWARD
	dbg_printf ("\tsfwd                  Display the forwarder state.\r\n");
	dbg_printf ("\tftrace <n>            Start forwarder tracing for <n> events.\r\n");
#endif
#ifdef DDS_TRACE
	dbg_printf ("\tdtrace [<t>]          Set/toggle default DDS tracing (t=0 or 1..15).\r\n");
	dbg_printf ("\ttrace [<ep> [<t>]]    Set/toggle tracing on endpoints.\r\n");
	dbg_printf ("\t                      <t>: 1=Frame, 2=Signal, 4=State, 8=Timer.\r\n");
#endif
#ifdef MSG_TRACE
	dbg_printf ("\tditrace [<t>]         Set/toggle default IP tracing (0 or 1).\r\n");
	dbg_printf ("\titrace [<h> [<t>]]    Set/toggle tracing on IP connections (t:0/1).\r\n");
#endif
#ifdef CTRACE_USED
	dbg_printf ("\tcton                  Enable the cyclic trace buffer.\r\n");
	dbg_printf ("\tctoff                 Disable the cyclic trace buffer.\r\n");
	dbg_printf ("\tctclr                 Clear the cyclic trace buffer.\r\n");
	dbg_printf ("\tctmode <m>            Set tracing mode ('C'=cyclic/'S'=stop).\r\n");
	dbg_printf ("\tctinfo                Display the state of the cyclic trace buffer.\r\n");
	dbg_printf ("\tctaadd <pos> <name> <dlen> [<data>] <action> {<action>}\r\n");
	dbg_printf ("\t                      Add an action list to a tracepoint.\r\n");
	dbg_printf ("\tctaclr                Clear all action lists.\r\n");
	dbg_printf ("\tctalist               Display all actions lists.\r\n");
	dbg_printf ("\tctainfo [<id>]        Display all id or all id:index values.\r\n");
	dbg_printf ("\tctcounters            Display all counters.\r\n");
	dbg_printf ("\tctcclr                Clear all counters.\r\n");
	dbg_printf ("\tctdump                Dump the cyclic trace buffer.\r\n");
	dbg_printf ("\tctsave <filename>     Save cyclic trace data to file.\r\n");
#endif
#ifdef PROFILE
	dbg_printf ("\tprofs                 Display profile results.\r\n");
	dbg_printf ("\tcprofs <delay> <dur>  Clear and restart profiling.\r\n");
#endif
#if defined (THREADS_USED) && defined (LOCK_TRACE)
	dbg_printf ("\tslstat                Display locking status.\r\n");
#endif
	dbg_printf ("\td [<p> [<n>]]         Dump memory.\r\n");
	dbg_printf ("\tda [<p> [<n>]]        Dump memory in ASCII.\r\n");
	dbg_printf ("\tdb [<p> [<n>]]        Dump memory in hex bytes.\r\n");
	dbg_printf ("\tds [<p> [<n>]]        Dump memory in hex 16-bit values.\r\n");
	dbg_printf ("\tdl [<p> [<n>]]        Dump memory in hex 32-bit values.\r\n");
	dbg_printf ("\tdm [<p> [<n>]]        Dump memory in mixed hex/ASCII.\r\n");
	dbg_printf ("\tindent <tab> <n>      Set indent type (if <tab>=1: use TABs).\r\n");
	dbg_printf ("\ttaflags <flags>       Set type attribute display flags.\r\n");
	dbg_printf ("\t                      <flags>: 1=header, 2=size, 4=elsize, 8=ofs.\r\n");
	dbg_printf ("\tserver [<port>]       Start debug server on the given port.\r\n");
	dbg_printf ("\tenv                   Display configuration data (=sconf).\r\n");
	dbg_printf ("\tset <var> <value>     Set the configuration variable to given value.\r\n");
	dbg_printf ("\tunset <var>           Unset the configuration variable.\r\n");
	dbg_printf ("\tsuspend <value>       Suspend with given mode.\r\n");
	dbg_printf ("\tactivate <value>      Activate with given mode.\r\n");
	dbg_printf ("\thelp                  Display general help.\r\n");
	if (aborting)
	      	dbg_printf ("\tquit                  Quit main DDS program.\r\n");
	dbg_printf ("\texit                  Close remote connection.\r\n");
}

/* dbg_print_uclist -- Display an unsigned char list in the format: {x,y,z} in
		       either decimal or hexadecimal format. */

void dbg_print_uclist (unsigned nchars, const unsigned char *cp, int hex)
{
	unsigned	i, c;

	dbg_printf ("{");
	for (i = 0; i < nchars; i++) {
		c = *cp++;
		if (i)
			dbg_printf (", ");
		if (hex)
			dbg_printf ("0x%x", c);
		else
			dbg_printf ("%u", c);
	}
	dbg_printf ("}");
}

/* dbg_print_entity -- Display an entity. */

void dbg_print_entity (const EntityId_t *eid)
{
	char	buffer [10];

	dbg_printf ("%s", entity_id_str (eid, buffer));
}

/* dbg_print_entity_id -- Display an entity Id preceded with '<name>:' if name
			  is non-NULL. */

void dbg_print_entity_id (const char *name, const EntityId_t *ep)
{
	if (name)
		dbg_printf ("%s:", name);
	dbg_print_entity (ep);
}

/* dbg_print_locator -- Display a locator. */

void dbg_print_locator (const Locator_t *lp)
{
	dbg_printf ("%s", locator_str (lp));
}

/* dbg_print_guid_prefix -- Display a GUID prefix. */

void dbg_print_guid_prefix (const GuidPrefix_t *gp)
{
	char	buffer [28];

	dbg_printf ("%s", guid_prefix_str (gp, buffer));
}

/* dbg_print_guid -- Display a GUID. */

void dbg_print_guid (const GUID_t *gp)
{
	dbg_print_guid_prefix (&gp->prefix);
	dbg_printf ("-");
	dbg_print_entity (&gp->entity_id);
}

/* dbg_print_timer -- Dump the remaining value of a timer. */

void dbg_print_timer (const Timer_t *tp)
{
	Ticks_t	remain = tmr_remain (tp);

	if (remain == ~0UL)
		dbg_printf ("<not running>");
	else
		dbg_printf ("%lu.%02lus", remain / 100, remain % 100);
}

/* dbg_print_ticks -- Display a time in ticks. */

void dbg_print_ticks (const char *name, Ticks_t ticks)
{
	if (name)
		dbg_printf ("%s:", name);
	dbg_printf ("%lums", ticks * 10);
}

/* data_dump -- Dump some common data parameters. */

void debug_data_dump (void)
{
        char    osname [100];
        char    osrelease [100];
        char    hostname [200];

	if (sys_username (hostname, sizeof (hostname)))
		dbg_printf ("%s@", hostname);
	if (sys_hostname (hostname, sizeof (hostname)))
		dbg_printf ("%s", hostname);
	dbg_printf ("\r\n");
	if (sys_osname (osname, sizeof (osname))) {
		dbg_printf ("%s", osname);
		if (sys_osrelease (osrelease, sizeof (osrelease)))
			dbg_printf (" %s", osrelease);
		dbg_printf (" (%u-bit)\r\n", WORDSIZE);
	}
	dbg_printf ("%s ", dds_entity_name);
	dbg_printf ("(%u)\r\n", sys_pid ());
	dbg_printf ("Maximum sample size = %lu bytes.\r\n",
						(unsigned long) dds_max_sample_size);
#ifdef RTPS_USED
	dbg_printf ("Accumulation size of RTPS submessages = %lu bytes.\r\n",
						(unsigned long) rtps_max_msg_size);
#if defined (BIGDATA) || (WORDSIZE == 64)
	if (rtps_frag_size == ~0U)
		dbg_printf ("RTPS fragmentation disabled.\r\n");
	else {
		dbg_printf ("RTPS fragment size = %lu bytes.\r\n",
						(unsigned long) rtps_frag_size);
		dbg_printf ("RTPS fragment burst = %u fragments.\r\n", rtps_frag_burst);
		dbg_printf ("RTPS fragment delay = %u us.\r\n", rtps_frag_delay);
	}
#endif
#endif
}

/* debug_pool_dump -- Dump the pool contents. */

void debug_pool_dump (int wide)
{
	size_t	sizes [PPT_SIZES];

	print_pool_format ((wide) ? PDT_LONG : PDT_NORMAL);
	print_pool_hdr (0);
	memset (sizes, 0, sizeof (sizes));
	dbg_printf ("List:\r\n");
	sl_pool_dump (sizes);
	dbg_printf ("String:\r\n");
	str_pool_dump (sizes);
	dbg_printf ("Timer:\r\n");
	tmr_pool_dump (sizes);
	dbg_printf ("Buffers:\r\n");
	db_pool_dump (sizes);
	dbg_printf ("Locators:\r\n");
	locator_pool_dump (sizes);
	dbg_printf ("QoS:\r\n");
	qos_pool_dump (sizes);
	dbg_printf ("Cache:\r\n");
	hc_pool_dump (sizes);
	dbg_printf ("Domain:\r\n");
	domain_pool_dump (sizes);
	dbg_printf ("DDS:\r\n");
	dds_pool_dump (sizes);
	dbg_printf ("DCPS:\r\n");
	dcps_pool_dump (sizes);
#ifdef RTPS_USED
	dbg_printf ("RTPS:\r\n");
	rtps_pool_dump (sizes);
	dbg_printf ("IP:\r\n");
	rtps_ip_pool_dump (sizes);
#endif
#ifdef XTYPES_USED
	dbg_printf ("XTYPES:\r\n");
	xd_pool_dump (sizes);
#endif
#ifdef DDS_NATIVE_SECURITY
	dbg_printf ("SECURITY:\r\n");
	sec_pool_dump (sizes);
#endif
	print_pool_end (sizes);
}

static Endpoint_t *endpoint_ptr (int handle, int disc_ok)
{
	Entity_t	*e;

	e = entity_ptr (handle);
	if (!e)
		return (NULL);

	if (entity_type (e) < ET_WRITER ||
	    (!disc_ok && entity_discovered (e->flags)))
		return (NULL);

	return ((Endpoint_t *) e);
}

#ifdef RTPS_USED

static void cache_dump (const char *cmd)
{
	Endpoint_t	*ep;

	ep = endpoint_ptr (atoi (cmd), 0);
	if (ep)
		rtps_cache_dump (ep);
}
#endif

static void dcache_dump (const char *cmd)
{
	Endpoint_t	*ep;

	ep = endpoint_ptr (atoi (cmd), 0);
	if (ep)
		dcps_cache_dump (ep);
}

void debug_cache_dump (void *p)
{
	Endpoint_t	*ep = (Endpoint_t *) p;

	if (!ep)
		return;

	if (!entity_writer (entity_type (&ep->entity)) &&
	    !entity_reader (entity_type (&ep->entity)))
		return;

	dcps_cache_dump (ep);
}

#ifdef RTPS_USED

static void proxy_dump (const char *cmd)
{
	Endpoint_t	*ep;

	if (!cmd || *cmd == '\0' || (*cmd == '*' && cmd [1] == '\0'))
		ep = NULL;
	else {
		ep = endpoint_ptr (atoi (cmd), 1);
		if (!ep) {
			dbg_printf ("Not a valid proxy!\r\n");
			return;
		}
	}
	rtps_proxy_dump (ep);
}

static void proxy_restart (const char *cmd)
{
	Endpoint_t	*ep;

	if (!cmd || *cmd == '\0' || (*cmd == '*' && cmd [1] == '\0'))
		ep = NULL;
	else {
		ep = endpoint_ptr (atoi (cmd), 1);
		if (!ep) {
			dbg_printf ("Not a valid proxy!\r\n");
			return;
		}
	}
	rtps_proxy_restart (ep);
}

#endif

void debug_proxy_dump (void *p)
{
#ifdef RTPS_USED
	Endpoint_t	*ep = (Endpoint_t *) p;

	if (ep &&
	    !entity_writer (entity_type (&ep->entity)) &&
	    !entity_reader (entity_type (&ep->entity)))
		return;

	rtps_proxy_dump (ep);
#else
	ARG_NOT_USED (p)
#endif
}

void debug_topic_dump (unsigned domain, const char *name)
{
	topic_dump (domain_lookup (domain), name, taflags);
}

static inline unsigned hexdigit (char c, int *error)
{
	unsigned	d;

	*error = 0;
	if (c >= '0' && c <= '9')
		d = c - '0';
	else if (c >= 'a' && c <= 'f')
		d = c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		d = c - 'A' + 10;
	else {
		*error = 1;
		d = 0;
	}
	return (d);
}

static unsigned long get_num_base (const char *cp, unsigned base)
{
	unsigned long	n = 0;
	unsigned	d, i = 0;
	int		error;

	do {
		d = hexdigit (*cp, &error);
		if (error) {
			if (!i)
				dbg_printf ("?number expected!\r\n");
			return (n);
		}
		if (d > base) {
			dbg_printf ("?invalid number!\r\n");
			return (0);
		}
		n = n * base + d;
		i++;
	}
	while (*++cp);
	return (n);
}

static unsigned long get_num (int hex, const char *cp)
{
	unsigned	base;

	if (*cp == '0') {
		cp++;
		if (*cp == 'x') {
			cp++;
			base = 16;
		}
		else if (*cp == '\0')
			return (0);

		else
			base = 8;
	}
	else
		base = (hex) ? 16 : 10;
	return (get_num_base (cp, base));
}

static void dbg_topic_dump (const char *cmd)
{
	unsigned	domain;
	char		buf [192], *cp;

	skip_blanks (&cmd);
	skip_string (&cmd, buf);
	domain = get_num (0, buf);
	skip_string (&cmd, buf);
	cp = buf;
	while (*cp) {
		if (*cp == '/') {
			*cp = '\0';
			break;
		}
		cp++;
	}
	topic_dump (domain_lookup (domain), buf, taflags);
}

#ifdef XTYPES_USED

void debug_type_dump (unsigned scope, const char *name)
{
	xt_type_dump (scope, name, taflags);
}

static void dbg_type_dump (const char *cmd)
{
	unsigned	scope;
	char		buf [64];

	skip_blanks (&cmd);
	skip_string (&cmd, buf);
	if (buf [0]) {
		scope = get_num (0, buf);
		skip_string (&cmd, buf);
	}
	else
		scope = 0;
	xt_type_dump (scope, buf, taflags);
}

#endif

void debug_disc_dump (void)
{
	disc_dump (1);
}

static void dbg_entity_qos_dump (const char *cmd)
{
	unsigned	entity;
	Entity_t	*ep;
	char		buf [64];

	skip_blanks (&cmd);
	skip_string (&cmd, buf);
	entity = get_num (0, buf);
	ep = entity_ptr (entity);
	if (!ep) {
		dbg_printf ("No such entity!\r\n");
		return;
	}
	qos_entity_dump (ep);
}

static void do_query (Endpoint_t *ep, const char *query)
{
	DDS_DataReader		dr = (DDS_DataReader) ep;
	DDS_QueryCondition	qc;
	DDS_TopicDescription	td;
	Topic_t			*tp;
	DDS_ReturnCode_t	rc;
	const TypeSupport_t	*ts;
	static DDS_DataSeq	rx_sample = DDS_SEQ_INITIALIZER (void *);
	static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
	unsigned		i;

	if (!entity_reader (entity_type (&ep->entity)) ||
	    (ep->entity.flags & EF_LOCAL) == 0) {
		dbg_printf ("Not a local Reader endpoint!\r\n");
		return;
	}
	td = DDS_DataReader_get_topicdescription (dr);
	if (!td) {
		dbg_printf ("Can't get topic description!\r\n");
		return;
	}
	tp = (Topic_t *) td;
	ts = tp->type->type_support;

	if (!query || *query == '\0' || (*query == '*' && query [1] == '\0')) {
		rc = DDS_DataReader_read (dr,
					  &rx_sample,
					  &rx_info,
					  DDS_LENGTH_UNLIMITED,
					  DDS_READ_SAMPLE_STATE,
					  DDS_ANY_VIEW_STATE,
					  DDS_ANY_INSTANCE_STATE);
		if (rc && rc != DDS_RETCODE_NO_DATA) {
			dbg_printf ("Can't do read()!\r\n");
			return;
		}
	}
	else {
		qc = DDS_DataReader_create_querycondition (dr,
							   DDS_READ_SAMPLE_STATE,
							   DDS_ANY_VIEW_STATE,
							   DDS_ANY_INSTANCE_STATE,
							   query,
							   NULL);
		if (!qc) {
			dbg_printf ("Can't create Query condition!\r\n");
			return;
		}
#ifdef DUMP_QUERY
		dcps_querycondition_dump (qc);
#endif
		rc = DDS_DataReader_read_w_condition (dr,
						      &rx_sample,
						      &rx_info,
						      DDS_LENGTH_UNLIMITED,
						      qc);
		if (rc && rc != DDS_RETCODE_NO_DATA) {
			dbg_printf ("Can't do read_w_condition()!\r\n");
			DDS_DataReader_delete_readcondition (dr, qc);
			return;
		}
		DDS_DataReader_delete_readcondition (dr, qc);
	}
	if (rc == DDS_RETCODE_NO_DATA) {
		dbg_printf ("No matching samples found.\r\n");
		return;
	}
	for (i = 0; i < DDS_SEQ_LENGTH (rx_sample); i++)
		if (DDS_SEQ_ITEM (rx_info, i)->valid_data) {
			dbg_printf ("%u: ", i + 1);
			DDS_TypeSupport_dump_data (1, ts,
						   DDS_SEQ_ITEM (rx_sample, i),
						   1, 0, 1);
			dbg_printf ("\r\n");
		}

	DDS_DataReader_return_loan (dr, &rx_sample, &rx_info);
}

static void cache_query (const char *cmd)
{
	Endpoint_t	*ep;
	char		buf [64];

	skip_blanks (&cmd);
	skip_string (&cmd, buf);
	ep = endpoint_ptr (get_num (0, buf), 0);
	if (!ep) {
		dbg_printf ("No such reader!\r\n");
		return;
	}
	do_query (ep, cmd);
}

static void dbg_assert_participant (unsigned domain)
{
	DDS_DomainParticipant	dp;
	DDS_ReturnCode_t	error;
	
	dp = DDS_DomainParticipantFactory_lookup_participant (domain);
	if (!dp)
		dbg_printf ("No such participant!\r\n");
	else {
		error = DDS_DomainParticipant_assert_liveliness (dp);
		if (error)
			dbg_printf ("Error #%d: %s!\r\n", error, DDS_error (error));
	}
}

static void dbg_assert_endpoint (unsigned h)
{
	Endpoint_t		*ep = endpoint_ptr (h, 0);
	DDS_ReturnCode_t	error;

	if (!ep || !entity_writer (entity_type (&ep->entity)))
		dbg_printf ("No such writer!\r\n");
	else {
		error = DDS_DataWriter_assert_liveliness ((DDS_DataWriter) ep);
		if (error)
			dbg_printf ("Error #%d: %s!\r\n", error, DDS_error (error));
	}
}

#ifdef CTRACE_USED

static void ctrace_add_actions (const char *cmd)
{
	char		buf [80];
	unsigned	pos, id, index, dlen, act, ofs, i, d, nargs;
	int		error;
	unsigned char	data [16];
	CTrcAction_t	args [16];

	skip_blanks (&cmd);
	skip_string (&cmd, buf);
	if (!*cmd || !buf [0])
		goto too_short;

	pos = get_num (0, buf);
	skip_string (&cmd, buf);
	if (!*cmd || !buf [0] || ctrc_trace_match (buf, &id, &index))
		goto too_short;

	skip_string (&cmd, buf);
	if (!*cmd || !buf [0])
		goto too_short;

	dlen = get_num (0, buf);
	if (dlen) {
		if (dlen > 16) {
			dbg_printf ("Match data too large (max=16)!");
			return;
		}
		skip_string (&cmd, buf);
		if (!*cmd || !buf [0] || strlen (buf) != dlen * 2)
			goto too_short;

		for (i = 0; i < dlen; i++) {
			d = hexdigit (buf [i * 2], &error);
			if (error)
				goto hex_expected;

			data [i] = (d << 4) | hexdigit (buf [i * 2] + 1, &error);
			if (error) {

		    hex_expected:
				dbg_printf ("hex data expected!");
				return;
			}
		}
	}
	act = 0;
	nargs = 0;
	do {
		skip_string (&cmd, buf);
		if (!buf [0])
			goto too_short;

		if (nargs > 16) {
			dbg_printf ("Too many commands (max=16)!\r\n");
			return;
		}
		if (!astrcmp (buf, "start"))
			act |= CTA_START;
		else if (!astrcmp (buf, "stop"))
			act |= CTA_STOP;
		else if (!astrcmp (buf, "clear"))
			act |= CTA_CLEAR;
		else if (!astrcmp (buf, "mode"))
			act |= CTA_MODE;
		else if (!astrcmp (buf, "mark"))
			act |= CTA_MARK;
		else if (!astrcmp (buf, "restore"))
			act |= CTA_RESTORE;
		else if (!astrcmp (buf, "signal"))
			act |= CTA_SIGNAL;
		else {
			ofs = 3;
			if (!astrncmp (buf, "inc", 3))
				act |= CTA_INC;
			else if (!astrncmp (buf, "dec", 3))
				act |= CTA_DEC;
			else if (!astrncmp (buf, "ifz", 3))
				act |= CTA_IFZ;
			else if (!astrncmp (buf, "ifnz", 4)) {
				act |= CTA_IFNZ;
				ofs++;
			}
			else if (!astrncmp (buf, "goto", 3))
				act |= CTA_GOTO;
			else {
				dbg_printf ("?unknown command\r\n");
				return;
			}
			if (buf [ofs] == '1' && buf [ofs + 1] != '\0') {
				d = hexdigit (buf [ofs + 1], &error);
				if (error) {
					dbg_printf ("?invalid number");
					return;
				}
				d += 10;
				ofs += 2;
			}
			else
				d = buf [ofs++] - '0';
			args [nargs++] = act | d;
			act = 0;
		}
	}
	while (*cmd);
	if (act)
		args [nargs++] = act;
	if (ctrc_actions_add (pos, id, index, data, dlen, args, nargs))
		dbg_printf ("Error adding actions!\r\n");
	return;

    too_short:
	dbg_printf ("Extra arguments expected!\r\n");
}

static void ctrace_list_actions (void)
{
	void		*data;
	unsigned char	*dp;
	CTrcAction_t	*acts, a;
	size_t		length;
	unsigned	i, j, id, index, nacts;

	for (i = 0; ; i++) {
		if (ctrc_actions_get (i,
				      &id, &index,
				      &data, &length,
				      &acts, &nacts)) {
			if (!i)
				dbg_printf ("No actions defined.\r\n");
			return;
		}
		dbg_printf ("%u %s %lu", i, ctrc_trace_name (id, index), 
						(unsigned long) length);
		if (length) {
			dbg_printf (" ");
			dp = (unsigned char *) data;
			for (j = 0; j < length; j++);
				dbg_printf ("%02x", dp [j]);
		}
		for (j = 0; j < nacts; j++) {
			a = acts [j];
			if ((a & CTA_START) != 0)
				dbg_printf (" start");
			if ((a & CTA_STOP) != 0)
				dbg_printf (" stop");
			if ((a & CTA_CLEAR) != 0)
				dbg_printf (" clear");
			if ((a & CTA_MODE) != 0)
				dbg_printf (" mode");
			if ((a & CTA_MARK) != 0)
				dbg_printf (" mark");
			if ((a & CTA_RESTORE) != 0)
				dbg_printf (" restore");
			if ((a & CTA_SIGNAL) != 0)
				dbg_printf (" signal");
			if ((a & CTA_INC) != 0)
				dbg_printf (" inc%u", a & CTA_PARAM);
			if ((a & CTA_DEC) != 0)
				dbg_printf (" dec%u", a & CTA_PARAM);
			if ((a & CTA_IFZ) != 0)
				dbg_printf (" ifz%u", a & CTA_PARAM);
			if ((a & CTA_IFNZ) != 0)
				dbg_printf (" ifnz%u", a & CTA_PARAM);
			if ((a & CTA_GOTO) != 0)
				dbg_printf (" goto%u", a & CTA_PARAM);
			dbg_printf (" ");
		}
		dbg_printf ("\r\n");
	}
}

static void ctrace_info (const char *args)
{
	unsigned	i, j;

	skip_blanks (&args);
	if (args [0] == '\0') {
		for (i = 0; i <= LOG_MAX_ID; i++)
			if (log_id_str [i]) {
				dbg_printf ("%s", log_id_str [i]);
				if (log_fct_str [i])
					dbg_printf (":*");
				dbg_printf (" ");
			}
		dbg_printf ("\r\n");
	}
	else {
		for (i = 0; i <= LOG_MAX_ID; i++) {
			if (!log_id_str [i])
				continue;

			if (!astrcmp (args, log_id_str [i])) {
				dbg_printf ("%s: ", log_id_str [i]);
				if (log_fct_str [i])
					for (j = 0; log_fct_str [i][j]; j++)
						dbg_printf ("%s ", log_fct_str [i][j]);
				dbg_printf ("\r\n");
				return;
			}
		}
		dbg_printf ("No such trace group!\r\n");
	}
}

static void ctrace_counters (void)
{
	unsigned	i;
	unsigned long	ctrs [16];

	ctrc_counters_get (ctrs);
	for (i = 0; i < 16; i++)
		if (ctrs [i])
			dbg_printf ("%4u: %lu\r\n", i, ctrs [i]);
}

#endif
#ifdef PROFILE

static void restart_profs (const char *args)
{
	unsigned long	delay, duration;
	char		buf [64];

	skip_blanks (&args);
	if (args [0] == '\0')
		delay = duration = 0;
	else {
		skip_string (&args, buf);
		delay = get_num (0, buf);
		if (!delay)
			return;

		if (args [0] == '\0')
			duration = 0;
		else
			duration = get_num (0, args);
	}
	prof_clear (delay, duration);
}

#endif

static void dump_memory (const char *cmd, const char *args)
{
	static int		ascii = -1;
	static unsigned char	*mp = NULL;
	char			buf [64];

	int			mode, in_str;
	unsigned		length, nv, i;
	unsigned char		*cp;
	union {
		unsigned char	b [4];
		unsigned short	s [2];
		uint32_t	l;
	}			u;

	if (cmd [1] == '\0')
		mode = ascii;
	else if (cmd [1] == 'a' && cmd [2] == '\0')
		mode = ascii = 0;
	else if (cmd [1] == 'b' && cmd [2] == '\0')
		mode = ascii = 1;
	else if (cmd [1] == 's' && cmd [2] == '\0')
		mode = ascii = 2;
	else if (cmd [1] == 'l' && cmd [2] == '\0')
		mode = ascii = 4;
	else if (cmd [1] == 'm' && cmd [2] == '\0')
		mode = ascii = -1;
	else {
		dbg_printf ("?%s\r\n", cmd);
		return;
	}
	skip_blanks (&args);
	if (args [0] == '\0') {
		cp = mp;
		length = 64;
	}
	else {
		skip_string (&args, buf);
		cp = (unsigned char *) get_num (1, buf);
		if (!cp)
			return;

		if (args [0] == '\0')
			length = 64;
		else
			length = get_num (0, args);
	}
	nv = 0;
	in_str = 0;
	buf [16] = '\0';
	for (i = 0; i < length; ) {
		if ((i & 0xf) == 0)
			dbg_printf ("%p: ", cp);

		u.b [nv++] = *cp++;
		if (mode <= 0 || nv == (unsigned) mode) {
			switch (mode) {
				case 0:
					if (u.b [0] >= ' ' && u.b [0] <= '~') {
						if (!in_str) {
							in_str = 1;
							dbg_printf (" '");
						}
						dbg_printf ("%c", u.b [0]);
					}
					else {
						if (in_str)
							dbg_printf ("'");
						in_str = 0;
						dbg_printf (" \\%03o", u.b [0]);
					}
					break;
				case -1:
				case 1:
					dbg_printf ("%02x ", u.b [0]);
					if (mode == -1)
						buf [i & 0xf] = (u.b [0] >= ' ' && 
								 u.b [0] <= '~') ? 
								 u.b [0] : '.';
					break;
				case 2:
					dbg_printf ("%04x ", u.s [0]);
					break;
				case 4:
					dbg_printf ("%08x ", u.l);
					break;
			}
			nv = 0;
		}
		i++;
		if ((i & 0xf) == 0) {
			if (mode == -1)
				dbg_printf ("   %s", buf);
			else if (mode == 0 && in_str) {
				dbg_printf ("'");
				in_str = 0;
			}
			dbg_printf ("\r\n");
		}
	}
	if ((i & 0xf) != 0) {
		if (mode == -1) {
			do {
				dbg_printf ("   ");
				buf [i & 0xf] = ' ';
				i++;
			}
			while ((i & 0xf) != 0);
			dbg_printf ("   %s", buf);
		}
		dbg_printf ("\r\n");
	}
	mp = cp;
}

static void domain_dump (const char *cmd)
{
	unsigned	domain, flags, i, j, sf, df, f [2];
	Domain_t	*dp;

	sscanf (cmd, "%u %x %x", &domain, &f [0], &f [1]);
	flags = 0;
	if (f [1])
		flags |= DDF_PEERS;
	for (i = 0; i < 2; i++)
		for (j = 0, sf = 1, df = i + 1; j < 5; j++, df <<= 2, sf <<= 1)
			if ((f [i] & sf) != 0)
				flags |= df;
	dp = domain_lookup (domain);
	if (!dp)
		dbg_printf ("No such domain!\r\n");
	else
		dump_domain (dp, flags);	
}

static void indent_set (const char *cmd)
{
	unsigned	i, nindents;
	int		use_tab;
	static char	buf [16];

	sscanf (cmd, "%d %u", &use_tab, &nindents);
	if (nindents > 15)
		dbg_printf ("Too many indents (max = 15)!\r\n");
	for (i = 0; i < nindents; i++)
		if (use_tab)
			buf [i] = '\t';
		else
			buf [i] = ' ';
	buf [i] = '\0';
	dbg_indent (buf);
}

static void taflags_set (const char *cmd)
{
	unsigned	f;

	sscanf (cmd, "%u", &f);
	if (f > 15) {
		dbg_printf ("Invalid flags set!\r\n");
		return;
	}
	taflags = f;
}

/* debug_log -- Output a logging string to the debug connections. */

void debug_log (const char *s, int nl)
{
	DebugSession_t	*sp;
	int		n, i;

	for (i = 0; i < MAX_SESSIONS; i++) {
		if ((sp = dbg_sessions [i]) == NULL ||
		    !sp->log_events)
			continue;

		if (!--sp->log_events)
			log_debug_count--;
#ifdef _WIN32
		WriteFile (sp->out_fd, s, strlen (s), &n, NULL);
#else
		n = write (sp->out_fd, s, strlen (s));
#endif
		if (n < 0)
			continue;

		if (nl)
#ifdef _WIN32
			WriteFile (sp->out_fd, "\r\n", 2, &n, NULL);
#else
			n = write (sp->out_fd, "\r\n", 2);
#endif
		if (n < 0)
			continue;
	}
}

void debug_command (const char *buf)
{
	char		cmd [64];
	unsigned short	port;
	unsigned	mode;
#ifdef RTPS_USED
#ifdef DDS_TRACE
	Endpoint_t	*ep;
#endif
#ifdef MSG_TRACE
	int		h;
	int		itmode;
#endif
#endif
	/* Strip leading spaces. */
	skip_blanks (&buf);
	skip_string (&buf, cmd);

	/* Check command. */
	if (cmd [0] == '#')
		; /* Nothing to do ... */
	else if (!strncmp (cmd, "ssys", 3))
		debug_data_dump ();
	else if (!strncmp (cmd, "stimer", 3))
		tmr_dump ();
	else if (!strncmp (cmd, "sstr", 3))
		str_dump ();
	else if (!strncmp (cmd, "spoola", 6))
		debug_pool_dump (1);
	else if (!strncmp (cmd, "spool", 3))
		debug_pool_dump (0);
#ifdef RTPS_USED
	else if (!strncmp (cmd, "scxq", 4))
		rtps_ip_dump_queued ();
	else if (!strncmp (cmd, "scxa", 4))
		rtps_ip_dump (buf, 1);
	else if (!strncmp (cmd, "scx", 3))
		rtps_ip_dump (buf, 0);
#endif
	else if (!strncmp (cmd, "sloc", 3))
		locator_dump ();
	else if (!strncmp (cmd, "sdomain", 3))
		domain_dump (buf);
	else if (!strncmp (cmd, "sconfig", 3) || !strncmp (cmd, "env", 3))
		config_dump ();
	else if (!strncmp (cmd, "sdisca", 6))
		debug_disc_dump ();
	else if (!strncmp (cmd, "sdisc", 4))
		disc_dump (0);
#ifdef XTYPES_USED
	else if (!strncmp (cmd, "stypes", 3))
		dbg_type_dump (buf);
#endif
	else if (!strncmp (cmd, "sqos", 2))
		qos_dump ();
	else if (!strncmp (cmd, "stopic", 3))
		dbg_topic_dump (buf);
	else if (!strncmp (cmd, "sendpoints", 3) ||
		 !strncmp (cmd, "sep", 3)) {	/* Often used ;-) */
#ifdef RTPS_USED
		dbg_printf ("DCPS:\r\n");
#endif
		dcps_endpoints_dump ();
#ifdef RTPS_USED
		dbg_printf ("RTPS:\r\n");
		rtps_endpoints_dump ();
#endif
	}
#ifdef RTPS_USED
	else if (!strncmp (cmd, "scache", 3))
		cache_dump (buf);
#endif
	else if (!strncmp (cmd, "sdcache", 3))
		dcache_dump (buf);
	else if (!strncmp (cmd, "qcache", 2))
		cache_query (buf);
#ifdef RTPS_USED
	else if (!strncmp (cmd, "sproxy", 3))
		proxy_dump (buf);
	else if (!strncmp (cmd, "rproxy", 3))
		proxy_restart (buf);
#endif
	else if (!strncmp (cmd, "seqos", 3))
		dbg_entity_qos_dump (buf);
#ifdef DDS_NATIVE_SECURITY
	else if (!strncmp (cmd, "scrypto", 3))
		sec_crypto_dump (atoi (buf));
	else if (!strncmp (cmd, "sscache", 3))
		sec_cache_dump ();
	else if (!strncmp (cmd, "rehs", 3))
		DDS_Security_permissions_changed ();
#endif
#ifdef RTPS_USED
	else if (!strncmp (cmd, "srx", 3))
		rtps_receiver_dump ();
	else if (!strncmp (cmd, "stx", 3))
		rtps_transmitter_dump ();
#endif
#ifndef _WIN32
	else if (!strncmp (cmd, "sfd", 3))
		sock_fd_dump ();
#endif
	else if (!strncmp (cmd, "asp", 3))
		dbg_assert_participant (atoi (buf));
	else if (!strncmp (cmd, "ase", 3))
		dbg_assert_endpoint (atoi (buf));
#ifdef DDS_DYN_IP
	else if (!strncmp (cmd, "sdip", 4))
		di_dump ();
#endif
#ifdef DDS_SECURITY
	else if (!strncmp (cmd, "dtls", 4))
		rtps_dtls_dump ();
#ifdef DDS_NATIVE_SECURITY
	else if (!strncmp (cmd, "spdb", 4))
		DDS_SP_access_db_dump ();
#ifdef DDS_QEO_TYPES
	else if (!strncmp (cmd, "spv", 3))
		dump_policy_version_list ();
#endif
#endif
#endif
#ifdef DDS_FORWARD
	else if (!strncmp (cmd, "sfwd", 2))
		rfwd_dump ();
	else if (!strncmp (cmd, "ftrace", 2)) {
		if (*buf >= '0' && *buf <= '9')
			rfwd_trace (atoi (buf));
		else
			rfwd_trace (-1);
	}
#endif
#ifdef PROFILE
	else if (!strncmp (cmd, "profs", 3))
		prof_list ();
	else if (!strncmp (cmd, "cprof", 4))
		restart_profs (buf);
#endif
	else if ((paused || (session && session->in_fd != tty_stdin)) &&
	         !strncmp (cmd, "pause", 1))
		if (session && session->in_fd != tty_stdin) {
			if (session->log_events) {
				session->log_events = 0;
				log_debug_count--;
			}
		}
		else
			*paused = 1;
	else if ((paused || (session && session->in_fd != tty_stdin)) &&
		 !strncmp (cmd, "resume", 1))
		if (session && session->in_fd != tty_stdin) {
			if (!session->log_events)
				log_debug_count++;
			if (*buf >= '1' && *buf <= '9')
				session->log_events = atoi (buf);
			else
				session->log_events = ~0;
		}
		else {
			*paused = 0;
			if (max_events && *buf >= '0' && *buf <= '9')
				*max_events = atoi (buf);
		}
	else if (sleep_time && !strncmp (cmd, "delay", 2)) {
		if (*buf >= '0' && *buf <= '9')
			*sleep_time = atoi (buf);
		else
			dbg_printf ("Sleep time = %ums\r\n", *sleep_time);
	}
#ifdef RTPS_USED
#ifdef DDS_TRACE
	else if (!strncmp (cmd, "dtrace", 2)) {
		if (*buf >= '0' && *buf <= '9')
			mode = atoi (buf);
		else
			mode = DDS_TRACE_MODE_TOGGLE;
		DDS_Trace_defaults_set (mode);
	}
	else if (!strncmp (cmd, "trace", 2)) {
		if (*buf >= '0' && *buf <= '9') {
			skip_string (&buf, cmd);
			ep = endpoint_ptr (atoi (cmd), 0);
			if (!ep)
				return;

			if (*buf >= '0' && *buf <= '9')
				mode = atoi (buf);
			else
				mode = DDS_TRACE_MODE_TOGGLE;
		}
		else {
			ep = DDS_TRACE_ALL_ENDPOINTS;
			mode = DDS_TRACE_MODE_TOGGLE;
		}
		DDS_Trace_set (ep, mode);
	}
#endif
#ifdef MSG_TRACE
	else if (!strncmp (cmd, "ditrace", 3)) {
		if (*buf >= '0' && *buf <= '1') {
			itmode = atoi (buf);
			rtps_ip_def_trace_mode (itmode);
		}
		else if (*buf == '\0') {
			itmode = -1;
			rtps_ip_def_trace_mode (itmode);
		}
	}
	else if (!strncmp (cmd, "itrace", 2)) {
		if (*buf == '\0')
			rtps_ip_cx_trace_mode (-1, -1);
		else if ((*buf >= '0' && *buf <= '9') || *buf == '*') {
			skip_string (&buf, cmd);
			if (cmd [0] == '*' && cmd [1] == '\0')
				h = -1;
			else
				h = atoi (cmd);
			if (!h)
				return;

			if (*buf == '0' || *buf == '1') {
				mode = atoi (buf);
				rtps_ip_cx_trace_mode (h, mode);
			}
			else if (*buf == '\0')
				rtps_ip_cx_trace_mode (h, -1);
		}
	}
#endif
#endif
#endif
#ifdef CTRACE_USED
	else if (!strncmp (cmd, "cton", 4))
		ctrc_start ();
	else if (!strncmp (cmd, "ctoff", 4))
		ctrc_stop ();
	else if (!strncmp (cmd, "ctclr", 4))
		ctrc_clear ();
	else if (!strncmp (cmd, "ctmode", 3)) {
		if (*buf == 'c' || *buf == 'C')
			ctrc_mode (1);
		else if (*buf == 's' || *buf == 'S')
			ctrc_mode (0);
		else {
			dbg_printf ("?");
			fflush (stdout);
		}
	}
	else if (!strncmp (cmd, "ctinfo", 3))
		ctrc_info ();
	else if (!strncmp (cmd, "ctaadd", 4))
		ctrace_add_actions (buf);
	else if (!strncmp (cmd, "ctaclr", 4))
		ctrc_actions_remove_all ();
	else if (!strncmp (cmd, "ctalist", 4))
		ctrace_list_actions ();
	else if (!strncmp (cmd, "ctainfo", 4))
		ctrace_info (buf);
	else if (!strncmp (cmd, "ctcounters", 4))
		ctrace_counters ();
	else if (!strncmp (cmd, "ctcclr", 4))
		ctrc_counters_clear ();
	else if (!strncmp (cmd, "ctdump", 3))
		ctrc_dump ();
	else if (!strncmp (cmd, "ctsave", 3))
		ctrc_save (buf);
#endif
#if defined (THREADS_USED) && defined (LOCK_TRACE)
	else if (!strncmp (cmd, "slstat", 3))
		trc_lock_info ();
#endif
	else if (!strncmp (cmd, "indent", 3))
		indent_set (buf);
	else if (!strncmp (cmd, "taflags", 3))
		taflags_set (buf);
	else if (!strncmp (cmd, "d", 1))
		dump_memory (cmd, buf);
	else if (!strncmp (cmd, "server", 4)) {
		if (dbg_server_fd)
			dbg_printf ("DDS Debug server already running on port %u\r\n", dbg_server_port);
		else {
			if (*buf >= '0' && *buf <= '9')
				port = atoi (buf);
			else
				port = dds_participant_id + DDS_DEBUG_PORT_OFS;
			debug_server_start (2, port);
		}
	}
	else if (!strncmp (cmd, "set", 3)) {
		char var [64];

		skip_string (&buf, var);
		if (DDS_parameter_set (var, buf) != DDS_RETCODE_OK)
			dbg_printf ("Could not set %s\r\n", var);
	}
	else if (!strncmp (cmd, "unset", 5)) {
		if (DDS_parameter_unset (buf) != DDS_RETCODE_OK)
			dbg_printf ("Unknown configuration variable: %s\r\n", buf);
	}
	else if (!strncmp (cmd, "suspend", 2)) {
		if (*buf >= '0' && *buf <= '9')
			mode = atoi (buf);
		else
			mode = DDS_ALL_ACTIVITY;
		dbg_printf ("Suspending DDS.\r\n");
		DDS_Activities_suspend (mode);
	}
	else if (!strncmp (cmd, "activate", 2)) {
		if (*buf >= '0' && *buf <= '9')
			mode = atoi (buf);
		else
			mode = DDS_ALL_ACTIVITY;
		dbg_printf ("Resuming DDS.\r\n");
		DDS_Activities_resume (mode);
	}
	else if (menu && !strncmp (cmd, "menu", 4)) {
		*menu = 1;
		if (aborting)
			*aborting = 1;
	}
	else if (aborting && !strncmp (cmd, "quit", 1)) {
		dbg_printf ("QUIT\r\n");
		dbg_printf ("\r\n");
		*aborting = 1;
	}
	else if (!strncmp (cmd, "help", 1) || *cmd == 'H' || *cmd == '?') {
		dbg_printf ("TDDS Debug shell -- (c) Technicolor, 2012\r\n");
		dbg_printf ("Following commands are available:\r\n");
		debug_help ();
	}
	else if (*cmd != '\0')
		dbg_printf ("?%s\r\n", cmd);
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static void debug_session_destroy (DebugSession_t *sp)
{
	static const char	close_msg [] = "TDDS closing connection!\r\n";
	int			n;

	if (!sp ||
	    sp->session >= MAX_SESSIONS ||
	    dbg_sessions [sp->session] != sp)
		return;

#ifdef _WIN32
	sock_fd_remove_socket (sp->sk);
	n = send (sp->sk, close_msg, sizeof (close_msg), 0);
	closesocket (sp->sk);
#else
	sock_fd_remove_handle (sp->in_fd);
	n = send (sp->out_fd, close_msg, sizeof (close_msg), MSG_NOSIGNAL);
	close (sp->in_fd);
#endif
	(void) n;
        printf ("TDDS Debug: connection to %s closed.\r\n", inet_ntoa (sp->remote.sin_addr));
	dbg_sessions [sp->session] = NULL;
	cl_delete (sp->cmdline);
	xfree (sp);
}

#ifdef _WIN32

void debug_input_sk (SOCKET sk, short events, void *udata)
{
	DebugSession_t	*sp = (DebugSession_t *) udata;
	char		ch;
	ssize_t		n;
	int		closedown = 0;

	ARG_NOT_USED (events)

	if (!sp ||
	    sp->session >= MAX_SESSIONS ||
	    dbg_sessions [sp->session] != sp)
		return;

#ifdef EVTRACE
	dbg_printf ("[cmd: %04x]", events);
	dbg_flush ();
#endif
	n = recv (sk, &sp->buf [sp->nchars], 1, 0);
	if (n == 0) {	/* Handle EOF properly. */
		closedown = 1;
		goto done;
	}
	dbg_redirect_sk (sk);
#ifdef CHTRACE
	dbg_printf ("{%d}", sp->buf [sp->nchars]);
	dbg_flush ();
#endif
	if ((ch = sp->buf [sp->nchars]) >= ' ' && sp->buf [sp->nchars] <= '~')
		sp->nchars++;
	else if (ch == 8 || ch == 0x7f) {
		if (sp->nchars) {
			dbg_printf ("%c %c", 8, 8);
			dbg_flush ();
			sp->nchars--;
		}
	}
	else if (ch == '\n' || ch == '\r') {
		sp->buf [sp->nchars] = '\0';
		if (!strncmp (sp->buf, "exit", 5))
			closedown = 1;
		else {
			debug_command (sp->buf);
			sp->nchars = 0;
			if (ch == '\n') {
				dbg_printf (">");
				dbg_flush ();
			}
		}
	}

    done:
	dbg_redirect_sk (0);
	if (closedown)
		debug_session_destroy (sp);
}
#endif

/* Telnet escape code: */
#define	IAC		((char) 0xff)

/* Telnet command codes: */
#define TCMD_SE		0	/* End of Sub-negotiation. */
#define	TCMD_NOP	1	/* No operation. */
#define	TCMD_DATA_MARK	2	/* Data stream portion of synch. */
#define	TCMD_BREAK	3	/* BRK character. */
#define TCMD_GO_AHEAD	9	/* Go Ahead signal. */
#define	TCMD_SB		10	/* Sub-negotiation follows. */
#define	TCMD_WILL	11	/* Will use following option. */
#define	TCMD_WONT	12	/* Won't use following option. */
#define	TCMD_DO		13	/* Request other party to use option. */
#define	TCMD_DONT	14	/* Request other party not to use an option. */

/* Telnet options: */
#define	TOPT_BINARY	0	/* Binary transmission. */
#define	TOPT_ECHO	1	/* Echo. */
#define	TOPT_RECONNECT	2	/* Reconnection. */
#define	TOPT_SUPP_GA	3	/* Suppress Go Ahead. */
#define	TOPT_STATUS	5	/* Status. */
#define	TOPT_TIMING	6	/* Timing Mark. */
#define	TOPT_REM_TECHO	7	/* Remote controlled Trans and Echo. */
#define	TOPT_O_CR_DISP	10	/* Output Carriage Return disposition. */
#define	TOPT_O_HT	11	/* Output Horizontal Tabstops. */
#define	TOPT_O_HT_DISP	12	/* Output Horizontal Tab Disposition. */
#define	TOPT_O_FF_DISP	13	/* Output Formfeed Disposition. */
#define	TOPT_O_VT	14	/* Output Vertical Tabstops. */
#define	TOPT_O_VT_DISP	15	/* Output Vertical Tab Disposition. */
#define	TOPT_O_LF_DISP	16	/* Output Linefeed Disposition. */
#define	TOPT_EXT_ASCII	17	/* Extended ASCII. */
#define	TOPT_LOGOUT	18	/* Logout. */
#define	TOPT_BYTE_MACRO	19	/* Byte macro. */
#define	TOPT_DE_TERM	20	/* Data Entry Terminal. */
#define	TOPT_SUPDUP	21	/* SUPDUP. */
#define	TOPT_SUPDUP_OUT	22	/* SUPDUP output. */
#define	TOPT_SEND_LOC	23	/* Send Location. */
#define	TOPT_TERM_TYPE	24	/* Terminal Type. */
#define	TOPT_EO_REC	25	/* End of Record. */
#define	TOPT_TACACS_UID	26	/* TACACS User Identification. */
#define	TOPT_OUT_MARK	27	/* Output Marking. */
#define	TOPT_TERM_LOC_N	28	/* Terminal Location Number. */
#define	TOPT_TN_3270	29	/* TELNET 3270 Regime. */
#define	TOPT_X3_PAD	30	/* X.3 PAD. */
#define	TOPT_NEG_WINDOW	31	/* Negotiate about Window size. */
#define	TOPT_TERM_SPEED	32	/* Terminal Speed. */
#define	TOPT_FLOW_CTRL	33	/* Remote Flow Control. */
#define	TOPT_LINEMODE	34	/* Linemode. */
#define	TOPT_X_LOCATION	35	/* X Display Location. */
#define	TOPT_AUTH	37	/* TELNET Authentication Option. */
#define	TOPT_ENV	39	/* TELNET Environment Option. */

#define	TOPT_EXTENDED	(char) 255	/* Extended negotiation. */

static int do_read_ch (HANDLE in_fd, char *ch)
{
	ssize_t		n;

#ifdef _WIN32
	ReadFile (in_fd, ch, 1, &n, NULL);
#else
	n = read (in_fd, ch, 1);
#endif
	return (n == 1);
}

static int do_telnet_negotiate (HANDLE        out_fd,
				unsigned char ch [3],
				unsigned      req)
{
	ssize_t		n;

	if (req == TCMD_WILL)
		ch [1] = TCMD_DONT;
	else if (req == TCMD_WONT)
		return (1);

	else if (req == TCMD_DO) {
		if (ch [2] == TOPT_SUPP_GA || ch [2] == TOPT_ECHO)
			ch [1] = TCMD_WILL;
		else
			ch [1] = TCMD_WONT;
	}
	else if (req == TCMD_DONT) {
		if (ch [2] == TOPT_SUPP_GA || ch [2] == TOPT_ECHO)
			return (0);

		ch [1] = TCMD_WONT;
	}
	ch [1] += 240;
#ifdef _WIN32
	WriteFile (out_fd, (char *) ch, 3, &n, NULL);
#else
	n = write (out_fd, (char *) ch, 3);
#endif
	return (n == 3);
}

static int do_telnet_cmd (HANDLE in_fd, HANDLE out_fd)
{
	unsigned	req;
	unsigned char	ch [3];

	ch [0] = IAC;
	if (!do_read_ch (in_fd, (char *) &ch [1]) || ch [1] < 0xf0)
		return (0);

	req = ch [1] - 0xf0;
	switch (req) {
		case TCMD_NOP:
		case TCMD_DATA_MARK:
		case TCMD_BREAK:
		case TCMD_GO_AHEAD:
			return (1);

		case TCMD_WILL:
		case TCMD_WONT:
		case TCMD_DO:
		case TCMD_DONT:
			if (!do_read_ch (in_fd, (char *) &ch [2]))
				return (0);

			do_telnet_negotiate (out_fd, ch, req);
			break;

		case TCMD_SB:
		case TCMD_SE:
		default:
			return (0);
	}
	return (1);
}

void debug_input (HANDLE fd, short events, void *udata)
{
	DebugSession_t	*sp = (DebugSession_t *) udata;
	char		ch;
	ssize_t		n;
	int		closedown = 0;
	char		*cmd;
	CmdLineStatus_t	ret;
#ifdef TTY_NORMAL
	static char	tty_buf [256];
#endif

	ARG_NOT_USED (fd)
	ARG_NOT_USED (events)

	if (!sp ||
	    sp->session >= MAX_SESSIONS ||
	    dbg_sessions [sp->session] != sp)
		return;

	session = sp;
#ifdef EVTRACE
	dbg_printf ("[cmd: %04x]", events);
	dbg_flush ();
#endif
	if (sp->in_fd == tty_stdin) {
#ifdef TTY_NORMAL
		n = tty_read (&tty_buf [sp->nchars], 1);
		ch = tty_buf [sp->nchars]; 
#else
		n = tty_read (&ch, 1);
#endif
	}
	else
#ifdef _WIN32
		ReadFile (sp->in_fd, &ch, 1, &n, NULL);
#else
		n = read (sp->in_fd, &ch, 1);
#endif

	if (n == 0) {	/* Handle EOF properly. */
		if (sp->in_fd != tty_stdin) {
			closedown = 1;
			goto done;
		}
		return;
	}
	if (sp->in_fd != tty_stdin) {
		if (ch == IAC) {
			if (!do_telnet_cmd (sp->in_fd, sp->out_fd)) {
				closedown = 1;
				goto destroy;
			}
			session = NULL;
			return;
		}
		dbg_redirect (sp->out_fd);
	}
#ifdef CHTRACE
	dbg_printf ("{%d}", ch);
	dbg_flush ();
#endif
#ifdef TTY_NORMAL
	if (sp->in_fd != tty_stdin) {
#endif
		ret = cl_add_char (sp->cmdline, ch, &cmd);
		if (ret == CLS_INCOMPLETE || ret == CLS_DONE_ERROR)
			goto done;

#ifdef TTY_NORMAL
	}
	else {
		if (ch >= ' ' && ch <= '~') {
			sp->nchars++;
			session = NULL;
			return;
		}
		else if (ch == 8 || ch == 0x7f) {
			if (sp->nchars) {
				dbg_printf ("%c %c", 8, 8);
				dbg_flush ();
				sp->nchars--;
			}
			session = NULL;
			return;
		}
		else if (ch == '\n' || ch == '\r') {
			tty_buf [sp->nchars] = '\0';
			cmd = tty_buf;
			sp->nchars = 0;
		}
		else {
			session = NULL;
			return;
		}
	}
#endif
	if (sp->in_fd != tty_stdin &&
	    !strncmp (cmd, "exit", 5))
		closedown = 1;
	else {
		debug_command (cmd);
		dbg_printf (">");
		dbg_flush ();
	}

    done:
	if (sp->in_fd != tty_stdin)
		dbg_redirect (0);

    destroy:
	if (closedown)
		debug_session_destroy (sp);

	session = NULL;
}

static int debug_session_create (HANDLE             fd_in,
				 HANDLE             fd_out,
#ifdef _WIN32
				 SOCKET             sk,
#endif
				 struct sockaddr_in *remote,
				 RHDATAFCT          fct)
{
	DebugSession_t	*sp;
	unsigned	i;

	sp = xmalloc (sizeof (DebugSession_t));
	if (!sp) {
		printf ("DDS Debug: out-of-memory for debug session.\r\n");
		return (-1);
	}
#ifdef _WIN32
	sp->sk_session = remote != NULL;
#endif
	sp->in_fd = fd_in;
	sp->out_fd = fd_out;
	sp->log_events = 0;
	sp->nchars = 0;
	sp->cmdline = cl_new ();
	if (remote) {
#ifdef _WIN32
		sp->sk = sk;
#endif
		sp->remote = *remote;
	}
	else {
		cl_load (sp->cmdline, ".tdds_hist");
		memset (&sp->remote, 0, sizeof (sp->remote));
	}

	/* Add the command line stream. */
#ifdef _WIN32
	if (remote)
		sock_fd_add_socket (sk, 
			     POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			     (RSDATAFCT) fct, sp);
	else
#endif
		sock_fd_add_handle (fd_in,
			     POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
			     fct, sp, "DDS.debug");

	for (i = 0; i < MAX_SESSIONS; i++)
		if (!dbg_sessions [i]) {
			dbg_sessions [i] = sp;
			sp->session = i;
			break;
		}

	return (0);
}

void debug_abort_enable (int *abort_program)
{
	aborting = abort_program;
}

void debug_control_enable (int *pause_program, unsigned *nsteps, unsigned *delay)
{
	paused = pause_program;
	max_events = nsteps;
	sleep_time = delay;
}

void debug_menu_enable (int *menu_screen)
{
	menu = menu_screen;
}

static void dbg_sessions_reset (int suspend)
{
	DebugSession_t	*sp;
	unsigned	i;

	for (i = 0; i < MAX_SESSIONS; i++) {
		if ((sp = dbg_sessions [i]) == NULL)
			continue;

		if (sp->in_fd == tty_stdin && suspend)
			continue;

		if (i) {
#ifdef _WIN32
			if (sp->sk_session)
				closesocket (sp->sk);
			else
				CloseHandle (sp->in_fd);
#else
				close (sp->in_fd);
#endif
		}
		if (sp->in_fd == tty_stdin && !suspend)
			cl_save (sp->cmdline, ".tdds_hist");

		cl_delete (sp->cmdline);
		xfree (sp);
		dbg_sessions [i] = NULL;
	}
}

static void dbg_sessions_cleanup (void)
{
	dbg_sessions_reset (0);
}

void debug_start (void)
{
#ifndef _WIN32
#if !defined (NUTTX_RTOS)
	if (!isatty (STDIN_FILENO))
		return;
#endif
#endif
	/* Initialize the TTY in raw mode. */
	tty_init ();

	debug_session_create (tty_stdin,
#ifdef _WIN32
				(HANDLE) 1, 0,
#elif defined (NUTTX_RTOS)		
				(HANDLE) 1,
#else
				STDOUT_FILENO,
#endif
				NULL, debug_input);
	atexit (dbg_sessions_cleanup);
}

void debug_start_fct (HANDLE inh, RHDATAFCT fct)
{
	debug_session_create (inh,
#ifdef _WIN32
			      (HANDLE) 1, 0,
#elif defined (NUTTX_RTOS)		
				(HANDLE) 1,
#else
			      STDOUT_FILENO,
#endif
			      NULL, fct);
	atexit (dbg_sessions_cleanup);
}

static void dbg_server_action (SOCKET fd, short events, void *udata)
{
	SOCKET			new_fd;
	int			n;
	struct sockaddr_in	rem_addr;
	socklen_t		rem_len = sizeof (rem_addr);
	static const char	telnet_options [] = {
		IAC, (char) (TCMD_WILL + 240), TOPT_ECHO,
		IAC, (char) (TCMD_WILL + 240), TOPT_SUPP_GA,
		IAC, (char) (TCMD_DONT + 240), TOPT_LINEMODE };
	static const char	intro [] = 
		"Welcome to the TDDS Debug shell.\r\n"
		"Type 'help' for more information on available commands.\r\n"
		">";

	ARG_NOT_USED (udata)

	if ((events & POLLIN) == 0) {
		printf ("TDDS Debug: unknown event: 0x%x\r\n", events);
		return;
	}
	new_fd = accept (fd, (struct sockaddr *) &rem_addr, &rem_len);
	if (new_fd == -1) {
		perror ("TDDS Debug: accept");
		return;
	}
        printf ("TDDS Debug: connected to %s\r\n", inet_ntoa (rem_addr.sin_addr));
#ifdef _WIN32
	debug_session_create (0, 0, new_fd, &rem_addr, (RHDATAFCT) debug_input_sk);
	n = send (new_fd, telnet_options, sizeof (telnet_options), 0);
	n = send (new_fd, intro, sizeof (intro), 0);
#else
	debug_session_create (new_fd, new_fd, &rem_addr, debug_input);
	n = write (new_fd, telnet_options, sizeof (telnet_options));
	n = write (new_fd, intro, sizeof (intro));
#endif
	(void) n;
}

void dbg_server_stop (void)
{
#ifdef _WIN32
	closesocket (dbg_server_fd);
#else
	close (dbg_server_fd);
#endif
	sock_fd_remove_socket (dbg_server_fd);
	dbg_server_fd = 0;
}

void dbg_server_exit (void)
{
	printf ("TDDS Debug: shutdown server connections.\r\n");
	if (!dbg_server_fd)
		return;

	dbg_sessions_cleanup ();
	dbg_server_stop ();
}

int debug_server_start (unsigned maxclients, unsigned short port)
{
	SOCKET			serverfd;
	struct sockaddr_in	addr;

	if (dbg_server_fd)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	serverfd = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverfd < 0) {
    		perror ("TDDS Debug: socket()");
		printf ("\r\n");
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	memset (&addr.sin_addr.s_addr, 0, 4);
	if (bind (serverfd, (struct sockaddr *) &addr, sizeof (addr))) {
#ifdef _WIN32
		closesocket (serverfd);
#else
		close (serverfd);
#endif
		perror ("TDDS Debug: bind()");
		printf ("\r\n");
		return (DDS_RETCODE_NOT_ALLOWED_BY_SEC);
	}
	listen (serverfd, maxclients);
	printf ("TDDS Debug: server started on port %u.\r\n", port);
	dbg_server_clients = maxclients;
	dbg_server_port = port;
	sock_fd_add_socket (serverfd,
		     POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL,
		     dbg_server_action, NULL, "DDS.debug-server");
	dbg_server_fd = serverfd;
	atexit (dbg_server_exit);
	return (DDS_RETCODE_OK);
}

void debug_suspend (void)
{
	if (dbg_suspended)
		return;

	if (!dbg_server_fd) {
		dbg_suspended = 1;
		return;
	}
	dbg_sessions_reset (1);
	dbg_server_stop ();
	dbg_suspended = 2;
}

void debug_resume (void)
{
	if (!dbg_suspended)
		return;

	if (dbg_suspended == 2)
		debug_server_start (dbg_server_clients, dbg_server_port);

	dbg_suspended = 0;
}

char *dbg_poll_event_str (short events)
{
	static char	buf [200];
	char		*sp = &buf [0];
	size_t		avail = sizeof (buf);
	int		n;

#define POLL_DUMP_EVENT_FLAG(f)  do {\
		if (events & f) {\
			n = snprintf (sp, avail, "%s", " " #f);\
			sp += n;\
			avail -= n;\
		}\
	} while (0)

	POLL_DUMP_EVENT_FLAG (POLLIN);
	POLL_DUMP_EVENT_FLAG (POLLPRI);
	POLL_DUMP_EVENT_FLAG (POLLOUT);
	POLL_DUMP_EVENT_FLAG (POLLRDNORM);
	POLL_DUMP_EVENT_FLAG (POLLRDBAND);
	POLL_DUMP_EVENT_FLAG (POLLWRNORM);
	POLL_DUMP_EVENT_FLAG (POLLWRBAND);
#ifdef POLLMSG
	POLL_DUMP_EVENT_FLAG (POLLMSG);
#endif
#ifdef POLLREMOVE
	POLL_DUMP_EVENT_FLAG (POLLREMOVE);
#endif
#ifdef POLLRDHUP
	POLL_DUMP_EVENT_FLAG (POLLRDHUP);
#endif
	POLL_DUMP_EVENT_FLAG (POLLERR);
	POLL_DUMP_EVENT_FLAG (POLLHUP);
	POLL_DUMP_EVENT_FLAG (POLLNVAL);

#undef POLL_DUMP_EVENT_FLAG

	return (&buf [0]);
}
#endif

