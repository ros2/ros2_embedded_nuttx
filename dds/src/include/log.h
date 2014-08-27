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

#ifndef __log_h_
#define __log_h_

#define	LOG_DEF_ID	0	/* Default logging id. */
#define	LOG_MAX_ID	31	/* Maximum logging id. */

#define	PROF_ID		0	/* Profiling logging. */
#define	DOM_ID		1	/* Domain logging. */
#define	POOL_ID		2	/* Pool logging. */
#define	STR_ID		3	/* String pool logging. */
#define	LOC_ID		4	/* Locator pool logging. */
#define	TMR_ID		5	/* Timer pool logging. */
#define	DB_ID		6	/* Buffer pool logging. */
#define	THREAD_ID	7	/* Thread logging. */
#define	CACHE_ID	8	/* Cache logging. */
#define	IP_ID		9	/* IP transport logging. */
#define	RTPS_ID		10	/* RTPS protocol logging. */
#define	QOS_ID		11	/* QoS pool logging. */
#define	SPDP_ID		12	/* SPDP protocol logging. */
#define	SEDP_ID		13	/* SEDP protocol logging. */
#define	DISC_ID		14	/* Discovery logging. */
#define	DCPS_ID		15	/* DCPS protocol logging. */
#define	XTYPES_ID	16	/* X-Types extensions logging. */
#define	SEC_ID		17	/* Security logging. */
#define	DDS_ID		18	/* General DDS logging. */

#define	INFO_ID		19	/* DDS error info logging. */
#define	USER_ID		20	/* User-level logging offset. */

extern const char *log_id_str [];	/* Id strings. */
extern const char **log_fct_str [];	/* Function strings. */

#endif /* __log_h_ */

