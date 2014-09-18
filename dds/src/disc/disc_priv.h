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

/* disc_priv.h -- Defines discovery wide macros & data structures. */

#ifndef __disc_priv_h_
#define __disc_priv_h_

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
#define ACCESS_CONTROL(dp) ((dp)->security && (dp)->access_protected)
#define	NATIVE_SECURITY(dp) (dp->security && (dp->participant.p_sec_caps & \
				(SECC_DDS_SEC | (SECC_DDS_SEC << SECC_LOCAL))) != 0)
#define SECURE_DISCOVERY(dp,f) (NATIVE_SECURITY (dp) && f)
#else
#define ACCESS_CONTROL(dp) (dp)->security
#define	NATIVE_SECURITY(dp) 0
#define	SECURE_DISCOVERY(dp,f) 0
#endif

#define	local_active(fh) (((fh) & (EF_LOCAL | EF_ENABLED)) == (EF_LOCAL | EF_ENABLED))
#define	remote_active(fh) (((fh) & (EF_LOCAL | EF_NOT_IGNORED)) == EF_NOT_IGNORED)

#ifdef DISC_EV_TRACE
#define	dtrc_print0(s)		log_printf (DISC_ID, 0, s)
#define	dtrc_print1(s,a)	log_printf (DISC_ID, 0, s, a)
#define	dtrc_print2(s,a1,a2)	log_printf (DISC_ID, 0, s, a1, a2)
#else
#define	dtrc_print0(s)
#define	dtrc_print1(s,a)
#define	dtrc_print2(s,a1,a2)
#endif

#define	locator_list_swap(l1,l2,t)	t = l1; l1 = l2; l2 = t

extern int disc_log;	/* General Discovery logging. */
#ifdef SIMPLE_DISCOVERY
extern int spdp_log;	/* SPDP logging. */
#endif

#endif /* !__disc_priv_h_ */

