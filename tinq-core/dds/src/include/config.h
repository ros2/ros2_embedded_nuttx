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

/* config.h -- Defines and allows to configure DDS-wide configuration parameters
	       from a number of sources, such as:
	       
	       		- Environment variables.
			- Configuration files. 
			- Default values.
			- Registries.
 */

#ifndef __config_h_
#define	__config_h_

typedef enum {
	MODE_DISABLED,
	MODE_ENABLED,
	MODE_PREFERRED
} IP_MODE;

typedef enum {
	/* Common data. */
	DC_Name,		/* Entity name (S). */
	DC_Environment,		/* Environment type (N). */
	DC_PurgeDelay,		/* Purge delay (N:ms). */
	DC_SampleSize,		/* Maximum sample size (N). */
#ifdef DDS_DEBUG
	/* TODO: Remove this and all related code once dtls/tls are fully working */
	DC_NoSecurity,		/* Security mode (N). */
#endif
	DC_Forward,		/* Forwarding mode (N). */
	DC_LogDir,		/* Location of logging file (S). */

	/* Memory pools. */
	DC_Pool_Domains,	/* Max. # of domains (N). */
	DC_Pool_Subscribers,
	DC_Pool_Publishers,
	DC_Pool_Readers,
	DC_Pool_Writers,
	DC_Pool_Topics,
	DC_Pool_FilteredTopics,
	DC_Pool_TopicTypes,
	DC_Pool_ReaderProxies,
	DC_Pool_WriterProxies,
	DC_Pool_DiscParticipants,
	DC_Pool_DiscReaders,
	DC_Pool_DiscWriters,
	DC_Pool_PoolData,
	DC_Pool_RxBuffers,
	DC_Pool_Changes,
	DC_Pool_Instances,
	DC_Pool_Samples,
	DC_Pool_LocalMatch,
	DC_Pool_CacheWait,
	DC_Pool_CacheXfer,
	DC_Pool_TimeFilters,
	DC_Pool_TimeInsts,
	DC_Pool_Strings,
	DC_Pool_StringData,
	DC_Pool_Locators,
	DC_Pool_QoS,
	DC_Pool_Lists,
	DC_Pool_ListNodes,
	DC_Pool_Timers,
	DC_Pool_WaitSets,
	DC_Pool_StatusConds,
	DC_Pool_ReadConds,
	DC_Pool_QueryConds,
	DC_Pool_GuardConds,
	DC_Pool_Notifications,
	DC_Pool_TopicWaits,
	DC_Pool_Guards,
	DC_Pool_DynTypes,
	DC_Pool_DynSamples,
	DC_Pool_Growth,

	/* RTPS parameters. */
	DC_RTPS_Mode,
	DC_RTPS_StatelessRetries,
	DC_RTPS_ResendPer,
	DC_RTPS_HeartbeatPer,
	DC_RTPS_NackRespTime,
	DC_RTPS_NackSuppTime,
	DC_RTPS_LeaseTime,
	DC_RTPS_HeartbeatResp,
	DC_RTPS_HeartbeatSupp,
	DC_RTPS_MsgSize,
	DC_RTPS_FragSize,
	DC_RTPS_FragBurst,
	DC_RTPS_FragDelay,
	DC_RTPS_DefTrace,

	/* General IP parameters. */
	DC_IP_Sockets,
	DC_IP_Mode,
	DC_IP_Scope,
	DC_IP_Intf,
	DC_IP_Address,
	DC_IP_Network,
	DC_IP_NoMCast,
	DC_IP_MCastTTL,
	DC_IP_MCastDest,
	DC_IP_MCastSrc,
	DC_IP_MCastAddr,

	/* UDP/IP-specific parameters. */
	DC_UDP_Mode,
	DC_UDP_PB,
	DC_UDP_DG,
	DC_UDP_PG,
	DC_UDP_D0,
	DC_UDP_D1,
	DC_UDP_D2,
	DC_UDP_D3,

#ifdef DDS_TCP
	/* TCP/IP-specific parameters. */
	DC_TCP_Mode,
	DC_TCP_Port,
	DC_TCP_Server,
#ifdef DDS_SECURITY
	DC_TCP_SecPort,
	DC_TCP_SecServer,
#endif
	DC_TCP_Public,
	DC_TCP_Private,
	DC_TCP_PB,
	DC_TCP_DG,
	DC_TCP_PG,
	DC_TCP_D0,
	DC_TCP_D1,
	DC_TCP_D2,
	DC_TCP_D3,
#endif

#ifdef DDS_IPV6
	/* General IPv6 parameters. */
	DC_IPv6_Mode,
	DC_IPv6_Scope,
	DC_IPv6_Intf,
	DC_IPv6_MCastHops,
	DC_IPv6_MCastIntf,
	DC_IPv6_MCastAddr,
#endif

	DC_END
} Config_t;

#define	N_CONFIG_PARS	((unsigned) DC_END)

extern char cfg_filename [128];

int config_load (void);

/* Load the general DDS configuration from all known sources. */

void config_flush (void);

/* Get rid of all cached configuration data. */

typedef void (*CFG_NOTIFY_FCT) (Config_t c);

/* Parameter notification function. */

int config_notify (Config_t c, CFG_NOTIFY_FCT fct);

/* Register a callback function to be notified when a configuration changes. */

int config_defined (Config_t c);

/* Returns a non-0 result if the parameter is set. */

int config_set_string (Config_t c, const char *value);

/* Set a configuration parameter as a string. */

const char *config_get_string (Config_t c, const char *def);

/* Get a configuration value as a string.  If it wasn't defined yet, return a
   default string (def). */

int config_set_number (Config_t c, unsigned value);

/* Set a configuration parameter as a number. */

unsigned config_get_number (Config_t c, unsigned def);

/* Get a configuration value as a number.  If not defined yet, returns def. */

int config_set_range (Config_t c, unsigned min, unsigned max);

/* Set a configuration parameter as a range of numbers. */

int config_get_range (Config_t c, unsigned *min, unsigned *max);

/* Return a configuration parameter as a range of two numbers. */

int config_set_mode (Config_t c, IP_MODE value);

/* Set a configuration parameter as a mode. */

IP_MODE config_get_mode (Config_t c, IP_MODE def);

/* Get a configuration parameter as a mode.  If not defined yet, return def. */

int config_unset (Config_t c);

/* Unset a configuration parameter. */

int config_parameter_set (const char *name, const char *value);

/* Generic parameter set function. Performs error checking on the name. */

const char *config_parameter_get (const char *name, char buffer [], size_t size);

/* Generic parameter get function. Returns the value if successful. */

int config_parameter_unset (const char *name);

/* Unset, i.e. remove a previously set configuration parameter. */

void config_dump (void);

/* Dump all configured parameters. */

#endif /* !__config_h_ */

