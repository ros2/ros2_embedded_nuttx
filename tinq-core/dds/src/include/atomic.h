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

/* atomic.h -- Implements some system and compiler-specific atomic operations. */

#ifndef __atomic_h_
#define __atomic_h_

#ifndef NO_ATOMIC

#define	atomic_set_s(s,v)	s = v
#define	atomic_set_w(w,v)	w = v
#define	atomic_set_l(l,v)	l = v

#define	atomic_get_s(s)		s
#define	atomic_get_w(w)		w
#define	atomic_get_l(l)		l

#define	atomic_add_s(s,v)	__sync_fetch_and_add (&s, v)
#define	atomic_add_w(w,v)	__sync_fetch_and_add (&w, v)
#define	atomic_add_l(l,v)	__sync_fetch_and_add (&l, v)

#define	atomic_inc_s(s)		__sync_fetch_and_add (&s, 1)
#define	atomic_inc_w(w)		__sync_fetch_and_add (&w, 1)
#define	atomic_inc_l(l)		__sync_fetch_and_add (&l, 1)

#define	atomic_dec_s(s)		__sync_fetch_and_sub (&s, 1)
#define	atomic_dec_w(w)		__sync_fetch_and_sub (&w, 1)
#define	atomic_dec_l(l)		__sync_fetch_and_aub (&l, 1)

#else

#define	atomic_set_s(s,v)	s = v
#define	atomic_set_w(w,v)	w = v
#define	atomic_set_l(l,v)	l = v

#define	atomic_get_s(s)		s
#define	atomic_get_w(w)		w
#define	atomic_get_l(l)		l

#define	atomic_add_s(s,v)	s += v
#define	atomic_add_w(w,v)	w += v
#define	atomic_add_l(l,v)	l += v

#define	atomic_inc_s(s)		s++
#define	atomic_inc_w(w)		w++
#define	atomic_inc_l(l)		l++

#define	atomic_dec_s(s)		s--
#define	atomic_dec_w(w)		w--
#define	atomic_dec_l(l)		l--

#endif

#endif /* !__atomic_h_ */
