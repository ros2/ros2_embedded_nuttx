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

/* sec_log.h -- Security plugin logging support functions. */

#ifndef __sec_log_h_
#define __sec_log_h_

#ifdef SEC_TRACE
#define	sec_log_fct(f)		log_printf (SEC_ID, 0, "SEC: %s() ", f)
#define	sec_log_ret(s,v)	log_printf (SEC_ID, 0, "-> " s "\r\n", v)
#define	sec_log_retv()		log_printf (SEC_ID, 0, "\r\n")
#else
#define	sec_log_fct(f)
#define	sec_log_ret(s,v)
#define	sec_log_retv()
#endif
#endif /* !__sec_log_h_ */
