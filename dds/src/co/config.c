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

/* config.c -- Implements a DDS-wide configuration parameters store that is
	       populated from a number of sources, such as:
	       
	       		- Environment variables.
			- Configuration files. 
			- Default values.
			- System registry.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "dds/dds_error.h"
#include "error.h"
#include "libx.h"
#include "thread.h"
#include "dds.h"
#include "config.h"

typedef enum {
	V_String,
	V_Number,
	V_Range,
	V_Mode
} ValueType_t;

typedef enum {
	G_Common,
	G_Pool,
	G_RTPS,
	G_IP,
	G_UDP
#ifdef DDS_TCP
      ,	G_TCP
#endif
#ifdef DDS_IPV6
      ,	G_IPv6
#endif
} GroupType_t;

typedef struct par_val_st {
	GroupType_t	group;
	Config_t	id;
	const char	*name;
	ValueType_t	type;
	int		valid;
	CFG_NOTIFY_FCT	notify;
	union {
	  char		*str;
	  unsigned	num;
	  unsigned	range [2];
	  IP_MODE	mode;
	}		value;
} ParVal_t;

static ParVal_t common_pars [] = {
	{ G_Common, DC_Name,        "NAME",        V_String, 0, NULL, {0}},
	{ G_Common, DC_Environment, "ENVIRONMENT", V_Number, 0, NULL, {0}},
	{ G_Common, DC_PurgeDelay,  "PURGE_DELAY", V_Number, 0, NULL, {0}},
	{ G_Common, DC_SampleSize,  "MAX_SAMPLE",  V_Number, 0, NULL, {0}},
#ifdef DDS_DEBUG
	/* TODO: Remove this and all related code once dtls/tls are fully working */
	{ G_Common, DC_NoSecurity,  "NOSECURITY",  V_Number, 0, NULL, {0}},
#endif
	{ G_Common, DC_Forward,     "FORWARD",     V_Number, 0, NULL, {0}},
	{ G_Common, DC_LogDir,      "LOG_DIR",     V_String, 0, NULL, {0}}
};

#define N_COMMON_PARS	(sizeof (common_pars) / sizeof (ParVal_t))

static ParVal_t pool_pars [] = {
	{ G_Pool, DC_Pool_Domains,          "DOMAINS",       V_Number, 0, NULL, {0}},
	{ G_Pool, DC_Pool_Subscribers,      "SUBSCRIBERS",   V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Publishers,       "PUBLISHERS",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Readers,          "READERS",       V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Writers,          "WRITERS",       V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Topics,           "TOPICS",        V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_FilteredTopics,   "FILTERED",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_TopicTypes,       "TYPES",         V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_ReaderProxies,    "RPROXIES",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_WriterProxies,    "WPROXIES",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_DiscParticipants, "RPARTICIPANTS", V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_DiscReaders,      "RREADERS",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_DiscWriters,      "RWRITERS",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_PoolData,         "POOL_DATA",     V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_RxBuffers,        "RX_BUFFERS",    V_Number, 0, NULL, {0}},
	{ G_Pool, DC_Pool_Changes,          "CHANGES",       V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Instances,        "INSTANCES",     V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Samples,          "SAMPLES",       V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_LocalMatch,       "LOCAL_MATCH",   V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_CacheWait,        "CACHE_WAIT",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_CacheXfer,        "CACHE_XFER",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_TimeFilters,      "TIME_FILTERS",  V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_TimeInsts,        "TIME_INSTS",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Strings,          "STRINGS",       V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_StringData,       "STRING_DATA",   V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Locators,         "LOCATORS",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_QoS,              "QOS",           V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Lists,            "LISTS",         V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_ListNodes,        "LIST_NODES",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Timers,           "TIMERS",        V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_WaitSets,         "WAITSETS",      V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_StatusConds,      "STATUSCONDS",   V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_ReadConds,        "READCONDS",     V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_QueryConds,       "QUERYCONDS",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_GuardConds,       "GUARDCONDS",    V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Notifications,    "NOTIFICATIONS", V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_TopicWaits,       "TOPIC_WAITING", V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Guards,           "GUARDS",        V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_DynTypes,         "DYN_TYPES",     V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_DynSamples,       "DYN_SAMPLES",   V_Range,  0, NULL, {0}},
	{ G_Pool, DC_Pool_Growth,           "GROWTH",        V_Number, 0, NULL, {0}}
};

#define N_POOL_PARS	(sizeof (pool_pars) / sizeof (ParVal_t))

static ParVal_t rtps_pars [] = {
	{ G_RTPS, DC_RTPS_Mode,             "MODE",          V_Mode,   0, NULL, {0}},
	{ G_RTPS, DC_RTPS_StatelessRetries, "SL_RETRIES",    V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_ResendPer,        "RESEND_TIME",   V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_HeartbeatPer,     "HB_TIME",       V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_NackRespTime,     "NACK_RESP_TIME",V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_NackSuppTime,     "NACK_SUPP_TIME",V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_LeaseTime,        "LEASE_TIME",    V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_HeartbeatResp,    "HB_RESP_TIME",  V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_HeartbeatSupp,    "HB_SUPP_TIME",  V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_MsgSize,          "MSG_SIZE",      V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_FragSize,         "FRAG_SIZE",     V_Number, 0, NULL, {0}},
	{ G_RTPS, DC_RTPS_FragBurst,        "FRAG_BURST",    V_Number, 0, NULL, {0}}, 
	{ G_RTPS, DC_RTPS_FragDelay,        "FRAG_DELAY",    V_Number, 0, NULL, {0}}, 
	{ G_RTPS, DC_RTPS_DefTrace,         "DTRACE",        V_Number, 0, NULL, {0}}
};

#define N_RTPS_PARS	(sizeof (rtps_pars) / sizeof (ParVal_t))

static ParVal_t ip_pars [] = {
	{ G_IP, DC_IP_Sockets,   "SOCKETS",         V_Number, 0, NULL, {0}},
	{ G_IP, DC_IP_Mode,      "MODE",            V_Mode,   0, NULL, {0}},
	{ G_IP, DC_IP_Scope,     "SCOPE",           V_Range,  0, NULL, {0}},
	{ G_IP, DC_IP_Intf,      "INTF",            V_String, 0, NULL, {0}},
	{ G_IP, DC_IP_Address,   "ADDRESS",         V_String, 0, NULL, {0}},
	{ G_IP, DC_IP_Network,   "NETWORK",         V_String, 0, NULL, {0}},
	{ G_IP, DC_IP_NoMCast,   "NO_MCAST",        V_String, 0, NULL, {0}},
	{ G_IP, DC_IP_MCastTTL,  "MCAST_TTL",       V_Number, 0, NULL, {0}},
	{ G_IP, DC_IP_MCastDest, "MCAST_DEST",      V_String, 0, NULL, {0}},
	{ G_IP, DC_IP_MCastSrc,  "MCAST_SRC",       V_String, 0, NULL, {0}},
	{ G_IP, DC_IP_MCastAddr, "GROUP",           V_String, 0, NULL, {0}}
};

#define N_IP_PARS	(sizeof (ip_pars) / sizeof (ParVal_t))

static ParVal_t udp_pars [] = {
	{ G_UDP, DC_UDP_Mode, "MODE",          V_Mode,   0, NULL, {0}},
	{ G_UDP, DC_UDP_PB,   "PB",            V_Number, 0, NULL, {0}},
	{ G_UDP, DC_UDP_DG,   "DG",            V_Number, 0, NULL, {0}},
	{ G_UDP, DC_UDP_PG,   "PG",            V_Number, 0, NULL, {0}},
	{ G_UDP, DC_UDP_D0,   "D0",            V_Number, 0, NULL, {0}},
	{ G_UDP, DC_UDP_D1,   "D1",            V_Number, 0, NULL, {0}},
	{ G_UDP, DC_UDP_D2,   "D2",            V_Number, 0, NULL, {0}},
	{ G_UDP, DC_UDP_D3,   "D3",            V_Number, 0, NULL, {0}}
};

#define N_UDP_PARS	(sizeof (udp_pars) / sizeof (ParVal_t))

#ifdef DDS_TCP

static ParVal_t tcp_pars [] = {
	{ G_TCP, DC_TCP_Mode,     "MODE",          V_Mode,   0, NULL, {0}},
	{ G_TCP, DC_TCP_Port,     "PORT",          V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_Server,   "SERVER",        V_String, 0, NULL, {0}},
#ifdef DDS_SECURITY
	{ G_TCP, DC_TCP_SecPort,  "SEC_PORT",      V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_SecServer,"SEC_SERVER",    V_String, 0, NULL, {0}},
#endif
	{ G_TCP, DC_TCP_Public,   "PUBLIC",        V_String, 0, NULL, {0}},
	{ G_TCP, DC_TCP_Private,  "PRIVATE",       V_Mode,   0, NULL, {0}},
	{ G_TCP, DC_TCP_PB,       "PB",            V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_DG,       "DG",            V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_PG,       "PG",            V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_D0,       "D0",            V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_D1,       "D1",            V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_D2,       "D2",            V_Number, 0, NULL, {0}},
	{ G_TCP, DC_TCP_D3,       "D3",            V_Number, 0, NULL, {0}}
};

#define N_TCP_PARS	(sizeof (tcp_pars) / sizeof (ParVal_t))

#endif

#ifdef DDS_IPV6

static ParVal_t ipv6_pars [] = {
	{ G_IPv6, DC_IPv6_Mode,      "MODE",          V_Mode,   0, NULL, {0}},
	{ G_IPv6, DC_IPv6_Scope,     "SCOPE",         V_Range,  0, NULL, {0}},
	{ G_IPv6, DC_IPv6_Intf,      "INTF",          V_String, 0, NULL, {0}},
	{ G_IPv6, DC_IPv6_MCastHops, "MCAST_HOPS",    V_Number, 0, NULL, {0}},
	{ G_IPv6, DC_IPv6_MCastIntf, "MCAST_INTF",    V_String, 0, NULL, {0}},
	{ G_IPv6, DC_IPv6_MCastAddr, "GROUP",         V_String, 0, NULL, {0}}
};

#define N_IPV6_PARS	(sizeof (ipv6_pars) / sizeof (ParVal_t))

#endif

typedef struct par_group_st {
	const char	*name;		/* Group name. */
	unsigned	npars;		/* # of parameters. */
	ParVal_t	*pars [48];	/* Sorted array of parameters. */
} ParGroup_t;

static ParGroup_t groups [] = {
	{ NULL,   0, {0}},
	{ "POOL", 0, {0}},
	{ "RTPS", 0, {0}},
	{ "IP",   0, {0}},
	{ "UDP",  0, {0}}
#ifdef DDS_TCP
      , { "TCP",  0, {0}}
#endif
#ifdef DDS_IPV6
      , { "IPV6", 0, {0}}
#endif
};

static ParVal_t *parameters [N_CONFIG_PARS];
static lock_t	cfg_lock = LOCK_STATIC_INIT;
static int	cfg_ready;

char cfg_filename [128];

static int gcmp (const void *g1p, const void *g2p)
{
	ParVal_t	*p1 = *(ParVal_t **) g1p;
	ParVal_t	*p2 = *(ParVal_t **) g2p;

	return (astrcmp (p1->name, p2->name));
}

/* config_init_group -- Initialize all parameters for a group. */

static void config_init_group (ParVal_t *pp, unsigned n)
{
	ParGroup_t	*gp;
	unsigned	i;

	gp = &groups [pp->group];
	gp->npars = 0;
	for (i = 0; i < n; i++, pp++) {
		gp->pars [gp->npars++] = pp;
		parameters [pp->id] = pp;
		pp->valid = 0;
	}
	qsort (gp->pars, gp->npars, sizeof (ParVal_t *), gcmp);
}

/* config_init -- Initialize parameters storage. */

int config_init (void)
{
	config_init_group (common_pars, N_COMMON_PARS);
	config_init_group (pool_pars,   N_POOL_PARS);
	config_init_group (rtps_pars,   N_RTPS_PARS);
	config_init_group (ip_pars,     N_IP_PARS);
	config_init_group (udp_pars,    N_UDP_PARS);
#ifdef DDS_TCP
	config_init_group (tcp_pars,    N_TCP_PARS);
#endif
#ifdef DDS_IPV6
	config_init_group (ipv6_pars,   N_IPV6_PARS);
#endif
	return (DDS_RETCODE_OK);
}

static int config_set (Config_t c, const char *value, int *diffs)
{
	ParVal_t	*pp;
	char		buf [16], *cp;

	if (diffs)
		*diffs = 0;
	if (c >= N_CONFIG_PARS)
		return (DDS_RETCODE_BAD_PARAMETER);

	pp = parameters [c];
	switch (pp->type) {
		case V_String:
			if (pp->valid && pp->value.str) {
				if (!strcmp (pp->value.str, value))
					break;

				free (pp->value.str);
			}
			pp->value.str = strdup (value);
			if (!pp->value.str) {
				warn_printf ("config_init: not enough memory for string!");
				break;
			}
			pp->valid = 1;
			if (diffs)
				*diffs = 1;
			break;

		case V_Number: {
			int	new_value = atoi (value);

			if (!pp->valid || pp->value.num != (unsigned) new_value) {
				if (diffs)
					*diffs = 1;
				pp->value.num = new_value;
				pp->valid = 1;
			}
			break;
		}
		case V_Range: {
			unsigned	min_val, max_val;

			cp = strchr (value, '-');
			if (!cp)
				min_val = max_val = atoi (value);
			else {
				memcpy (buf, value, cp - value);
				buf [cp - value] = '\0';
				min_val = atoi (buf);
				if (cp [1] == '*' || cp [1] == '\0')
					max_val = ~0;
				else
					max_val = atoi (cp + 1);
				if (max_val < min_val) {
					warn_printf ("config_init: %s: invalid range!", pp->name);
					break;
				}
			}
			if (!pp->valid ||
			    pp->value.range [0] != min_val ||
			    pp->value.range [1] != max_val) {
				if (diffs)
					*diffs = 1;
				pp->value.range [0] = min_val;
				pp->value.range [1] = max_val;
				pp->valid = 1;
			}
			break;
		}
		case V_Mode: {
			IP_MODE	mode;

			if (*value >= '0' && *value <= '2' && value [1] == '\0')
				mode = *value - '0';
			else if (!astrncmp (value, "disable", 7))
				mode = MODE_DISABLED;
			else if (!astrncmp (value, "enable", 6))
				mode = MODE_ENABLED;
			else if (!astrncmp (value, "prefer", 6))
				mode = MODE_PREFERRED;
			else {
				warn_printf ("config_init: %s: invalid mode!", pp->name);
				break;
			}
			if (!pp->valid ||
			    pp->value.mode != mode) {
				if (diffs)
					*diffs = 1;
				pp->value.mode = mode;
				pp->valid = 1;
			}
			break;
		}
		default:
			fatal_printf ("config_init: invalid type!");
			break;
	}
	return (DDS_RETCODE_OK);
}

/* config_lookup -- Lookup the name of a configuration parameter. */

static ParVal_t *config_lookup (const char *name)
{
	ParGroup_t	*gp = NULL;
	ParVal_t	*pp;
	const char	*cp;
	unsigned	i;
	int		l, h, m, d;
	char		group [12];

	cp = strchr (name, '_');
	if (!cp) {
		gp = &groups [0];
		cp = name;
	}
	else {
		memcpy (group, name, cp - name);
		group [cp - name] = '\0';
		if (!astrcmp (group, "purge") ||
		    !astrcmp (group, "max") ||
		    !astrcmp (group, "log")) {
			gp = &groups [0];
			cp = name;
		}
		else {
			for (i = 1; i < sizeof (groups) / sizeof (ParGroup_t); i++)
				if (groups [i].name != NULL && !astrcmp (group, groups [i].name)) {
					gp = &groups [i];
					break;
				}
			cp++;
		}
	}
	if (!gp)
		return (NULL);

	l = 0;
	h = gp->npars;
	while (l <= h) {
		m = l + ((h - l) >> 1);
		pp = gp->pars [m];
		d = astrcmp (cp, pp->name);
		if (d < 0)
			h = --m;
		else if (d > 0)
			l = ++m;
		else
			return (pp);
	}
	return (NULL);
}

static void syntax_error (const char *filename, unsigned line, const char *buf)
{
	warn_printf ("config_load: %s: syntax error on line %u.", filename, line);
	warn_printf (">>> %s", buf);
}

#define	getch()		c = buf [i++]
#define	skipblanks()	while (isblank (c)) getch()

/* config_load_file -- Try to load a config file. cfg_lock is assumed to be taken. */

static int config_load_file (const char *filename)
{
	FILE		*f;
	int		c, diffs;
	unsigned	i, j, value;
	char		*s;
	ParVal_t	*pp;
	char		group [10];
	char		name [16];
	char		buf [128];
	char		full_name [28];
	unsigned	line = 0;
	CFG_NOTIFY_FCT	cb;

	f = fopen (filename, "r");
	if (!f)
		return (0);

	group [0] = '\0';
	for (;;) {
		s = fgets (buf, sizeof (buf), f);
		if (!s)
			break;

		i = 0;
		line++;
		getch ();
		skipblanks ();
		if (c == '[') {
			getch ();
			skipblanks ();
			if (c == ']')
				group [0] = '\0';
			else if (isalpha (c)) {
				j = 0;
				do {
					 group [j++] = c;
					 getch ();
				}
				while (isalnum (c) && j < sizeof (group) - 1);
				group [j] = '\0';
				skipblanks ();
				if (c != ']') {
					syntax_error (filename, line, buf);
					break;
				}
			}
			else {
				syntax_error (filename, line, buf);
				break;
			}
		}
		else if (isalpha (c)) {
			j = 0;
			do {
				 name [j++] = c;
				 getch ();
			}
			while ((isalnum (c) || c == '_') && j < sizeof (name) - 1);
			name [j] = '\0';
			skipblanks ();
			if (c == '=') {
				getch ();
				skipblanks ();
				value = i - 1;
				while (c != '#' && c != '\n')
					getch ();
				i--;
				while (i > value + 1 && isblank (buf [i - 1]))
					i--;
				buf [i] = '\0';
				if (group [0])
					sprintf (full_name, "%s_%s", group, name);
				else
					strcpy (full_name, name);
				pp = config_lookup (full_name);
				if (!pp || config_set (pp->id, &buf [value], &diffs)) {
					syntax_error (filename, line, buf);
					break;
				}
				cb = (pp->valid && diffs && pp->notify) ? pp->notify : NULL;
				if (cb) {
					lock_release (cfg_lock);
					dds_config_update (pp->id, cb);
					lock_take (cfg_lock);
				}
			}
		}
		else if (c != '#' && c != '\n') {
			syntax_error (filename, line, buf);
			break;
		}
	}
	fclose (f);
	return (1);
}

/* config_load -- Load the general DDS configuration from all known sources. */

int config_load (void)
{
	ParVal_t	*pp;
	unsigned	i, n, l;
	const char	*sp;
	char		name [64];
	const char	*value;
	CFG_NOTIFY_FCT	cb;
	int		update, rc;

	rc = lock_take (cfg_lock);
	if (rc)
		fatal_printf ("Error taking cfg lock\r\n");

	if (cfg_ready) {
		lock_release (cfg_lock);
		return (DDS_RETCODE_OK);
	}

	cfg_ready = 1;
	config_init ();

	if (cfg_filename [0])
		config_load_file (cfg_filename);
	else {
		sp = sys_getenv ("TDDS_CONFIG");
		if (sp)
			config_load_file (sp);
		else if (!config_load_file ("tdds.conf"))
			if (!config_load_file ("~/.tddsconf"))
				config_load_file ("/etc/tdds.conf");
	}
	sprintf (name, "TDDS_");
	for (i = 0; i < N_CONFIG_PARS; i++) {
		pp = parameters [i];
		n = 5;
		if (pp->group) {
			l = sprintf (name + n, "%s_", groups [pp->group].name);
			n += l;
		}
		sprintf (name + n, "%s", pp->name);
		value = sys_getenv (name);
		if (!value)
			continue;

		config_set (i, value, &update);
		cb = (pp->valid && update && pp->notify) ? pp->notify : NULL;
		if (cb) {
			lock_release (cfg_lock);
			dds_config_update (pp->id, cb);
			lock_take (cfg_lock);
		}
	}
	lock_release (cfg_lock);
	return (DDS_RETCODE_OK);
}

/* config_flush -- Get rid of all cached configuration data. */

void config_flush (void)
{
	ParVal_t	*pp;
	unsigned	i;

	if (!cfg_ready)
		return;

	lock_take (cfg_lock);
	for (i = 0; i < N_CONFIG_PARS; i++) {
		pp = parameters [i];
		if (pp->valid) {
			if (pp->type == V_String)
				free (pp->value.str);

			pp->valid = 0;
		}
	}
	lock_release (cfg_lock);
	lock_destroy (cfg_lock);
	cfg_ready = 0;
}

/* config_notify -- Register a callback function to be notified when a
		    configuration parameter changes. */

int config_notify (Config_t c, CFG_NOTIFY_FCT fct)
{
	ParVal_t	*pp;

	if (c >= N_CONFIG_PARS)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (cfg_lock);
	pp = parameters [c];
	pp->notify = fct;
	lock_release (cfg_lock);
	if (fct)
		dds_config_update (c, fct);
	return (DDS_RETCODE_OK);
}

/* config_set_string -- Set a configuration parameter as a string. */

int config_set_string (Config_t c, const char *value)
{
	ParVal_t	*pp;
	CFG_NOTIFY_FCT	cb;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS || parameters [c]->type != V_String)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid) {
		if ((!pp->value.str && !value) ||
		    (pp->value.str && !strcmp (pp->value.str, value))) {
			lock_release (cfg_lock);
			return (DDS_RETCODE_OK);
		}
		if (pp->value.str)
			free (pp->value.str);
	}
	pp->value.str = strdup (value);
	pp->valid = 1;
	cb = (pp->notify) ? pp->notify : NULL;
	lock_release (cfg_lock);

	if (cb)
		dds_config_update (c, cb);
	return (DDS_RETCODE_OK);
}

/* config_get_string -- Get a configuration value as a string.  If it wasn't
			defined yet, return a default string (def). */

const char *config_get_string (Config_t c, const char *def)
{
	ParVal_t	*pp;
	const char	*sp;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS || parameters [c]->type != V_String)
		return (NULL);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid)
		sp = pp->value.str;
	else
		sp = def;
	lock_release (cfg_lock);
	return (sp);
}

/* config_set_number -- Set a configuration parameter as a number. */

int config_set_number (Config_t c, unsigned num)
{
	ParVal_t	*pp;
	CFG_NOTIFY_FCT	cb;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS || parameters [c]->type != V_Number)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid && pp->value.num == num) {
		lock_release (cfg_lock);
		return (DDS_RETCODE_OK);
	}
	pp->value.num = num;
	pp->valid = 1;
	cb = (pp->notify) ? pp->notify : NULL;
	lock_release (cfg_lock);

	if (cb)
		dds_config_update (c, cb);
	return (DDS_RETCODE_OK);
}

/* config_get_number -- Get a configuration value as a number.  If not defined
			yet, returns def. */

unsigned config_get_number (Config_t c, unsigned def)
{
	ParVal_t	*pp;
	unsigned	n;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS || parameters [c]->type != V_Number)
		return (0);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid)
		n = pp->value.num;
	else
		n = def;
	lock_release (cfg_lock);
	return (n);
}

/* config_set_range -- Set a configuration parameter as a range. */

int config_set_range (Config_t c, unsigned min, unsigned max)
{
	ParVal_t	*pp;
	CFG_NOTIFY_FCT	cb;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS ||
	    parameters [c]->type != V_Range ||
	    min > max)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid &&
	    pp->value.range [0] == min &&
	    pp->value.range [1] == max) {
		lock_release (cfg_lock);
		return (DDS_RETCODE_OK);
	}
	pp->value.range [0] = min;
	pp->value.range [1] = max;
	pp->valid = 1;
	cb = (pp->notify) ? pp->notify : NULL;
	lock_release (cfg_lock);

	if (cb)
		dds_config_update (c, cb);
	return (DDS_RETCODE_OK);
}

/* config_get_range -- Return a configuration parameter as a range of two
		       numbers. */

int config_get_range (Config_t c, unsigned *min, unsigned *max)
{
	ParVal_t	*pp;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS ||
	    parameters [c]->type != V_Range ||
	    !min || !max || *min > *max)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid) {
		*min = pp->value.range [0];
		*max = pp->value.range [1];
	}
	lock_release (cfg_lock);
	return (DDS_RETCODE_OK);
}

/* config_set_mode -- Set a configuration parameter as a mode. */

int config_set_mode (Config_t c, IP_MODE value)
{
	ParVal_t	*pp;
	CFG_NOTIFY_FCT	cb;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS ||
	    parameters [c]->type != V_Mode ||
	    value > MODE_PREFERRED)
		return (DDS_RETCODE_BAD_PARAMETER);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid && pp->value.mode == value) {
		lock_release (cfg_lock);
		return (DDS_RETCODE_OK);
	}
	pp->value.mode = value;
	pp->valid = 1;
	cb = (pp->notify) ? pp->notify : NULL;
	lock_release (cfg_lock);

	if (cb)
		dds_config_update (c, cb);
	return (DDS_RETCODE_OK);
}

/* config_get_mode -- Get a configuration parameter as a mode.  If not defined
		      yet, return def. */

IP_MODE config_get_mode (Config_t c, IP_MODE def)
{
	ParVal_t	*pp;
	IP_MODE		m;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS ||
	    parameters [c]->type != V_Mode ||
	    def > MODE_PREFERRED)
		return (0);

	lock_take (cfg_lock);
	pp = parameters [c];
	if (pp->valid)
		m = pp->value.mode;
	else
		m = def;
	lock_release (cfg_lock);
	return (m);
}

/* config_unset -- Unset a configuration parameter. */

int config_unset (Config_t c)
{
	ParVal_t	*pp;

	if (c >= N_CONFIG_PARS)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!cfg_ready)
		config_load ();

	pp = parameters [c];
	if (pp->valid) {
		if (pp->type == V_String)
			free (pp->value.str);

		pp->valid = 0;
	}
	return (DDS_RETCODE_OK);
}

/* config_defined -- Check if the parameter has been set. */

int config_defined (Config_t c)
{
	ParVal_t	*pp;

	if (!cfg_ready)
		config_load ();

	if (c >= N_CONFIG_PARS ||
	    (pp = parameters [c]) == NULL)
		return (0);
	else
		return (pp->valid);
}

/* config_parameter_set -- Generic parameter set function. Performs error
			   checking on the name. */

int config_parameter_set (const char *name, const char *value)
{
	ParVal_t	*pp;
	int		ret, diffs;
	CFG_NOTIFY_FCT	cb;

	if (!cfg_ready)
		config_load ();

	lock_take (cfg_lock);
	pp = config_lookup (name);
	if (!pp) {
		lock_release (cfg_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	ret = config_set (pp->id, value, &diffs);
	cb = (pp->valid && diffs && pp->notify) ? pp->notify : NULL;
	lock_release (cfg_lock);

	if (cb)
		dds_config_update (pp->id, cb);
	return (ret);
}

static const char *par2str (ParVal_t *pp, char buffer [], size_t size)
{
	char	*sp;
	size_t	n, left;

	if (!buffer || !size)
		return (NULL);

	switch (pp->type) {
		case V_String:
			n = snprintf (buffer, size, "%s", pp->value.str);
			break;
		case V_Number:
			n = snprintf (buffer, size, "%u", pp->value.num);
			break;
		case V_Range:
			n = snprintf (buffer, size, "%u", pp->value.range [0]);
			sp = &buffer [n];
			left = size - n;
			if (pp->value.range [1] > pp->value.range [0]) {
				if (n >= size || !left)
					return (NULL);

				*sp++ = '-';
				left--;
				n++;
				if (pp->value.range [1] == ~0U) {
					if (!left)
						return (NULL);

					*sp++ = '*';
					left--;
					if (!left)
						return (NULL);

					*sp = '\0';
					n += 2;
				}
				else
					n += snprintf (sp, left, "%u", pp->value.range [1]);
			}
			break;
		case V_Mode:
			if (pp->value.mode == MODE_DISABLED)
				n = snprintf (buffer, size, "DISABLED");
			else if (pp->value.mode == MODE_ENABLED)
				n = snprintf (buffer, size, "ENABLED");
			else
				n = snprintf (buffer, size, "PREFERRED");
			break;
		default:
			n = 0;
			buffer [0] = '\0';
			break;
	}
	if (n >= size)
		return (NULL);

	return (buffer);
}

/* config_parameter_get -- Generic parameter get function. Performs error
			   checking on the name. */

const char *config_parameter_get (const char *name, char buffer [], size_t size)
{
	ParVal_t	*pp;
	const char	*sp;

	if (!cfg_ready)
		config_load ();

	lock_take (cfg_lock);
	pp = config_lookup (name);
	if (!pp || !pp->valid) {
		lock_release (cfg_lock);
		return (NULL);
	}
	sp = par2str (pp, buffer, size);
	lock_release (cfg_lock);
	return (sp);
}

/* config_parameter_unset -- Unset, i.e. remove a previously set configuration
			     parameter. */

int config_parameter_unset (const char *name)
{
	ParVal_t	*pp;
	int		ret, update;
	CFG_NOTIFY_FCT	cb;

	if (!cfg_ready)
		config_load ();

	lock_take (cfg_lock);
	pp = config_lookup (name);
	if (!pp) {
		lock_release (cfg_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	update = pp->valid;
	ret = config_unset (pp->id);
	cb = (update && pp->notify) ? pp->notify : NULL;
	lock_release (cfg_lock);

	if (cb)
		dds_config_update (pp->id, cb);
	return (ret);
}

#ifdef DDS_DEBUG

void config_dump (void)
{
	ParVal_t	*pp;
	unsigned	i;
	char		buf [128];

	lock_take (cfg_lock);
	for (i = 0; i < N_CONFIG_PARS; i++) {
		pp = parameters [i];
		if (!pp || !pp->valid)
			continue;

		if (pp->group)
			dbg_printf ("%s_", groups [pp->group].name);
		dbg_printf ("%s = ", pp->name);
		dbg_printf ("%s", par2str (pp, buf, sizeof (buf)));
		dbg_printf ("\r\n");
	}
	lock_release (cfg_lock);
}

#endif

