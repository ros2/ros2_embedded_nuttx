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

#ifndef __rtps_cfg_h_
#define __rtps_cfg_h_

/*#define RTPS_SL_REL		** Support the Stateless Reliable FSM type. */
#define RTPS_COMBINE_MSGS	/* Combine Message Elements in single packet. */
/*#define RTPS_IMM_HEARTBEAT	** Always send a Heartbeat after data. */
/*#define RTPS_TRC_POOL		** Define this for in-path pool display. */
#define RTPS_INIT_ACKNACK	/* Reader sends an initial ACKNACK. */
#define	RTPS_EMPTY_ACKNACK	/* Allow empty ACKNACK messages. */
/*#define RTPS_INIT_HEARTBEAT	** Send an initial HEARTBEAT before starting. */
#define	RTPS_W_WAIT_ALIVE	/* Writer waits for reader before sending. */
/*#define RTPS_HB_WAIT		** Writer polls reader before sending data. */
#if defined (BIGDATA) || (WORDSIZE == 64)
#define RTPS_FRAGMENTS		/* Enable fragments support. */
#endif
/*#define RTPS_TRC_WRITER	** Trace Writer context changes. */
/*#define RTPS_TRC_READER	** Trace Reader context changes. */
/*#define RTPS_CACHE_CHECK	** Define this to enable cache checking. */
/*#define RTPS_REL_CHECK	** Check reliability state. */
/*#define RTPS_RANGE_CHECK	** Validata queue entry ranges. */
/*#define RTPS_TRC_CHANGES	** Trace change allocations. */
/*#define RTPS_TRC_SEQNR	** Trace rx-seqnr modifications. */
/*#define RTPS_VERIFY_LOOP	** Verify endless loops. */
/*#define RTPS_TRACE_MEP	** Define this to do low-level MEP checks. */
/*#define RTPS_TRC_LRBUFS	** Trace long Rx buffer pool usage. */
/*#define RTPS_MARSHALL_ALWAYS	** Always marshall transported data. */
/*#define RTPS_MARKERS		** Add code markers for notifics./triggers. */
/*#define RTPS_LOG_REPL_LOC	** Log updates to the reply locator. */
/*#define RTPS_CHECK_LOCS	** Check locator updates. */
/*#define RTPS_LOG_MSGS		** Log received and transmitted messages. */
/*#define RTPS_SLOW		** Define this to slow down timers *100. */
/*#define RTPS_TRACE_BIVS	** Define to trace volatile reader/writers. */
#define RTPS_PROXY_INST		/* Enable proxy instance update handling. */
/*#define RTPS_PROXY_INST_TX	** Add proxy instance fields to HB/ACKNACK. */
#ifdef RTPS_PROXY_INST_TX
#define	RTPS_PROXY_INST_VERSION	0x03010000U	/* Min. supported version. */
#endif
#ifdef RTPS_SLOW
#define	RT_MULT			10
#else
#define	RT_MULT			1
#endif

#define	US			(1000 * RT_MULT)
#define	MS			(1000 * US)

#ifndef RTPS_HB_SAMPLES
#define	RTPS_HB_SAMPLES		4	/* # of samples before Heartbeat sent.*/
#endif
#ifndef RTPS_SL_RETRIES
#define	RTPS_SL_RETRIES		2	/* # fast stateless resends. */
#endif
#ifndef RTPS_SL_RETRY_TO
#define	RTPS_SL_RETRY_TO	(200 * MS)	/* Min. 200ms for fast retry. */
#endif
#ifndef MIN_RESEND_TIME_NS
#define	MIN_RESEND_TIME_NS	(100 * MS)	/* Min. 100ms */
#endif
#ifndef DEF_HEARTBEAT_S
#define	DEF_HEARTBEAT_S		0
#define	DEF_HEARTBEAT_NS	(100 * MS)	/* i.o. 2ms */
#endif
#ifndef DEF_NACK_RSP_S
#define	DEF_NACK_RSP_S		0
#define	DEF_NACK_RSP_NS		0 /*(1 * US) */
#endif
#ifndef DEF_NACK_SUPP_S
#define DEF_NACK_SUPP_S		0
#define DEF_NACK_SUPP_NS	0
#endif
#ifndef DEF_RESEND_PER_S
#define DEF_RESEND_PER_S	10
#define DEF_RESEND_PER_NS	0
#endif
#ifndef DEF_LEASE_PER_S
#define	DEF_LEASE_PER_S		((DEF_RESEND_PER_S * 4) + 10)
#define	DEF_LEASE_PER_NS	0
#endif
#ifndef DEF_HB_RSP_S
#define	DEF_HB_RSP_S		0
#define	DEF_HB_RSP_NS		500
#endif
#ifndef DEF_HB_SUPP_S
#define	DEF_HB_SUPP_S		0
#define	DEF_HB_SUPP_NS		0
#endif
#ifndef WALIVE_TO
#define	WALIVE_TO		((TICKS_PER_SEC >> 2) * RT_MULT)
#endif
#ifndef RALIVE_TO
#define	RALIVE_TO		((TICKS_PER_SEC >> 3) * RT_MULT)
#endif
#ifndef MIN_RTPS_MSG_SIZE
#define	MIN_RTPS_MSG_SIZE	528	/* 576 - IP+UDP+RTPS headers. */
#endif
#ifndef MAX_RTPS_MSG_SIZE
#define	MAX_RTPS_MSG_SIZE	65000	/* 64K - extra headers. */
#endif
#ifndef DEF_RTPS_MSG_SIZE
#define	DEF_RTPS_MSG_SIZE	1452	/* 1500 - IP+UDP+RTPS headers. */
#endif
#ifdef RTPS_FRAGMENTS
#ifndef MIN_RTPS_FRAGMENT_SIZE
#define	MIN_RTPS_FRAGMENT_SIZE	128
#endif
#ifndef MAX_RTPS_FRAGMENT_SIZE
#define	MAX_RTPS_FRAGMENT_SIZE	65000
#endif
#ifndef DEF_RTPS_FRAGMENT_SIZE
#define	DEF_RTPS_FRAGMENT_SIZE	8192
#endif
#ifndef DEF_RTPS_FRAGMENT_BURST
#define DEF_RTPS_FRAGMENT_BURST	512
#endif
#ifndef DEF_RTPS_FRAGMENT_DELAY
#define DEF_RTPS_FRAGMENT_DELAY	0
#endif
#endif
#endif /* !__rtps_cfg_h_ */

