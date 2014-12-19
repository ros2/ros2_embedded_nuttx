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

/* rtps_trace.h -- RTPS message and state tracing submodule. */

#ifndef __rtps_trace_h_
#define __rtps_trace_h_

#include "rtps_cfg.h"

#ifdef RTPS_TRACE

#define	DEF_RTPS_TRACE	0 /*DRTRC_TRACE_ALL*/

void rtps_handle_trace_set (unsigned handle, unsigned mode);

/* Set the tracing mode for a specific handle. */

void rtps_name_trace_set (const char *name, unsigned mode);

/* Set the tracing mode for a specific name pattern. */

unsigned rtps_def_trace (unsigned handle, const char *name);

/* Return the default trace mode for a specific endpoint with the given
   handle and name. */

void rtps_rr_state_change (RemReader_t *rrp,
			   unsigned    s,
			   unsigned    prev_s,
			   int         init,
			   unsigned    type);

#define NEW_RR_CSTATE(rrp,s,i)	STMT_BEG				\
	{ if (rrp->rr_writer->endpoint.trace_state)			\
		rtps_rr_state_change (rrp, s, rrp->rr_cstate, i, 0);	\
	  rrp->rr_cstate = (s); } STMT_END

#define NEW_RR_TSTATE(rrp,s,i)	STMT_BEG				\
	{ if (rrp->rr_writer->endpoint.trace_state)			\
		rtps_rr_state_change (rrp, s, rrp->rr_tstate, i, 1);	\
	  rrp->rr_tstate = (s); } STMT_END

#define NEW_RR_ASTATE(rrp,s,i)	STMT_BEG				\
	{ if (rrp->rr_writer->endpoint.trace_state)			\
		rtps_rr_state_change (rrp, s, rrp->rr_astate, i, 2);	\
	  rrp->rr_astate = (s); } STMT_END

void rtps_rr_signal (RemReader_t *rrp, const char *s);

#define	RR_SIGNAL(rrp,s) STMT_BEG				\
	if (rrp->rr_writer->endpoint.trace_sigs) 		\
		rtps_rr_signal (rrp, s); STMT_END

void rtps_w_tmr_action (WRITER     *wp,
			const char *type,
			int        start,
			unsigned   ticks,
			const char *sp);

#define	RESEND_TMR_START(wp,t,v,u,f,l)	STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) 					\
		rtps_w_tmr_action (wp, "RESEND", 1, v, NULL);		\
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	RESEND_TMR_STOP(wp,t)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) 					\
		rtps_w_tmr_action (wp, "RESEND", 0, 0, NULL);		\
	tmr_stop (t); }			STMT_END

void rtps_trace_w_event (WRITER *wp, char event, const char *s);

#define RESEND_TMR_TO(wp)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) {					\
		rtps_trace_w_event (wp, 'T', "RESEND");			\
		log_printf (RTPS_ID, 0, "Time-out\r\n"); }		\
	}				STMT_END

#define	RESEND_TMR_ALLOC(wp)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) {					\
		rtps_trace_w_event (wp, 'T', "RESEND");			\
		log_printf (RTPS_ID, 0, "Allocate Timer\r\n"); }	\
	}				STMT_END

#define	RESEND_TMR_FREE(wp)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) {					\
		rtps_trace_w_event (wp, 'T', "RESEND");			\
		log_printf (RTPS_ID, 0, "Free Timer\r\n"); }		\
	}				STMT_END

#define	HEARTBEAT_TMR_START(wp,t,v,u,f,l) STMT_BEG		 	\
	{ if (wp->endpoint.trace_tmr) 					\
		rtps_w_tmr_action (wp, "HEARTBEAT", 1, v, NULL);	\
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	HEARTBEAT_TMR_STOP(wp,t)	STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) 					\
		rtps_w_tmr_action (wp, "HEARTBEAT", 0, 0, NULL);	\
	tmr_stop (t); }			STMT_END

#define HEARTBEAT_TMR_TO(wp)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) {					\
		rtps_trace_w_event (wp, 'T', "HEARTBEAT");		\
		log_printf (RTPS_ID, 0, "Time-out\r\n"); }		\
	}				STMT_END

#define	HEARTBEAT_TMR_ALLOC(wp)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) {					\
		rtps_trace_w_event (wp, 'T', "HEARTBEAT");		\
		log_printf (RTPS_ID, 0, "Allocate Timer\r\n"); }	\
	}				STMT_END

#define	HEARTBEAT_TMR_FREE(wp)		STMT_BEG			\
	{ if (wp->endpoint.trace_tmr) {					\
		rtps_trace_w_event (wp, 'T', "HEARTBEAT");		\
		log_printf (RTPS_ID, 0, "Free Timer\r\n"); }		\
	}				STMT_END

#define NACKRSP_TMR_START(rrp,t,v,u,f,l) STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) 			\
		rtps_w_tmr_action (rrp->rr_writer, "NACKRSP", 1, v, NULL); \
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	NACKRSP_TMR_STOP(rrp,t)	STMT_BEG				\
	{ if (rrp->rr_writer->endpoint.trace_tmr) 			\
		rtps_w_tmr_action (rrp->rr_writer, "NACKRSP", 0, 0, NULL); \
	tmr_stop (t); }			STMT_END

#define NACKRSP_TMR_TO(rrp)		STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) {			\
		rtps_trace_w_event (rrp->rr_writer, 'T', "NACKRSP");	\
		log_printf (RTPS_ID, 0, "Time-out\r\n"); }		\
	}				STMT_END

#define	NACKRSP_TMR_ALLOC(rrp)		STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) {			\
		rtps_trace_w_event (rrp->rr_writer, 'T', "NACKRSP");	\
		log_printf (RTPS_ID, 0, "Allocate Timer\r\n"); }	\
	}				STMT_END

#define	NACKRSP_TMR_FREE(rrp)		STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) {			\
		rtps_trace_w_event (rrp->rr_writer, 'T', "NACKRSP");	\
		log_printf (RTPS_ID, 0, "Free Timer\r\n"); }		\
	}				STMT_END

#define WALIVE_TMR_START(rrp,t,v,u,f,l) STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) 			\
		rtps_w_tmr_action (rrp->rr_writer, "WALIVE", 1, v, NULL); \
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	WALIVE_TMR_STOP(rrp,t)	STMT_BEG				\
	{ if (rrp->rr_writer->endpoint.trace_tmr) 			\
		rtps_w_tmr_action (rrp->rr_writer, "WALIVE", 0, 0, NULL); \
	tmr_stop (t); }			STMT_END

#define WALIVE_TMR_TO(rrp)		STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) {			\
		rtps_trace_w_event (rrp->rr_writer, 'T', "WALIVE");	\
		log_printf (RTPS_ID, 0, "Time-out\r\n"); }		\
	}				STMT_END

#define	WALIVE_TMR_ALLOC(rrp)		STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) {			\
		rtps_trace_w_event (rrp->rr_writer, 'T', "WALIVE");	\
		log_printf (RTPS_ID, 0, "Allocate Timer\r\n"); }	\
	}				STMT_END

#define	WALIVE_TMR_FREE(rrp)		STMT_BEG			\
	{ if (rrp->rr_writer->endpoint.trace_tmr) {			\
		rtps_trace_w_event (rrp->rr_writer, 'T', "WALIVE");	\
		log_printf (RTPS_ID, 0, "Free Timer\r\n"); }		\
	}				STMT_END

void rtps_rw_state_change (RemWriter_t *rwp,
			   unsigned    s,
			   unsigned    prev_s,
			   int         init,
			   unsigned    type);

#define NEW_RW_CSTATE(rwp,s,i)	STMT_BEG				\
	{ if (rwp->rw_reader->endpoint.trace_state)			\
		rtps_rw_state_change (rwp, s, rwp->rw_cstate, i, 0);	\
	  rwp->rw_cstate = (s); } STMT_END

#define NEW_RW_ASTATE(rwp,s,i)	STMT_BEG				\
	{ if (rwp->rw_reader->endpoint.trace_state)			\
		rtps_rw_state_change (rwp, s, rwp->rw_astate, i, 1);	\
	  rwp->rw_astate = (s); } STMT_END

void rtps_rw_signal (RemWriter_t *rwp, const char *s);

#define	RW_SIGNAL(rwp,s) if (rwp->rw_reader->endpoint.trace_sigs) rtps_rw_signal (rwp, s)

void rtps_r_tmr_action (READER          *rp,
			const Locator_t *lp,
			const char      *type,
			int             start,
			unsigned        ticks,
			const char      *sp);

#define HBRSP_TMR_START(rw,t,v,u,f,l)	STMT_BEG				     \
	{ if (rw->rw_reader->endpoint.trace_tmr) 				     \
		rtps_r_tmr_action (rw->rw_reader, NULL, "HEARTBEATRSP", 1, v, NULL); \
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	HBRSP_TMR_STOP(rw,t)		STMT_BEG				     \
	{ if (rw->rw_reader->endpoint.trace_tmr)			 	     \
		rtps_r_tmr_action (rw->rw_reader, NULL, "HEARTBEATRSP", 0, 0, NULL); \
	tmr_stop (t); }			STMT_END

void rtps_trace_r_event (READER *rp, char event, const char *s);

#define HBRSP_TMR_TO(rw)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) {				\
		rtps_trace_r_event (rw->rw_reader, 'T', "HEARTBEATRSP");	\
		log_printf (RTPS_ID, 0, "Time-out\r\n"); }			\
	}				STMT_END

#define	HBRSP_TMR_ALLOC(rw)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) {				\
		rtps_trace_r_event (rw->rw_reader, 'T', "HEARTBEATRSP");	\
		log_printf (RTPS_ID, 0, "Allocate Timer\r\n"); }		\
	}				STMT_END

#define	HBRSP_TMR_FREE(rw)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) {				\
		rtps_trace_r_event (rw->rw_reader, 'T', "HEARTBEATRSP");	\
		log_printf (RTPS_ID, 0, "Free Timer\r\n"); }			\
	}				STMT_END

#define RALIVE_TMR_START(rw,t,v,u,f,l)	STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) 				\
		rtps_r_tmr_action (rw->rw_reader, NULL, "RALIVE", 1, v, NULL);	\
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	RALIVE_TMR_STOP(rw,t)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr)				\
		rtps_r_tmr_action (rw->rw_reader, NULL, "RALIVE", 0, 0, NULL);	\
	tmr_stop (t); }			STMT_END

#define RALIVE_TMR_TO(rw)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) {				\
		rtps_trace_r_event (rw->rw_reader, 'T', "RALIVE");		\
		log_printf (RTPS_ID, 0, "Time-out\r\n"); }			\
	}				STMT_END

#define	RALIVE_TMR_ALLOC(rw)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) {				\
		rtps_trace_r_event (rw->rw_reader, 'T', "RALIVE");		\
		log_printf (RTPS_ID, 0, "Allocate Timer\r\n"); }		\
	}				STMT_END

#define	RALIVE_TMR_FREE(rw)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr) {				\
		rtps_trace_r_event (rw->rw_reader, 'T', "RALIVE");		\
		log_printf (RTPS_ID, 0, "Free Timer\r\n"); }			\
	}				STMT_END

#ifdef RTPS_FRAGMENTS

#define	FRAGSC_TMR_START(rw,t,v,u,f,l)	STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr)				\
		rtps_r_tmr_action (rw->rw_reader, NULL, "FRAGSC", 1, v, NULL);  \
	tmr_start_lock (t, v, u, f, l); } STMT_END

#define	FRAGSC_TMR_STOP(rw,t)		STMT_BEG				\
	{ if (rw->rw_reader->endpoint.trace_tmr)			 	\
		rtps_r_tmr_action (rw->rw_reader, NULL, "FRAGSC", 0, 0, NULL);  \
	tmr_stop (t); }			STMT_END

#endif

void rtps_w_frame (WRITER        *wp,
		   int           rx,
		   unsigned      type,
		   Participant_t *p,
		   void          *pp,
		   unsigned      flags);

void rtps_r_frame (READER        *rp,
		   int           rx,
		   unsigned      type,
		   Participant_t *p,
		   void          *pp,
		   unsigned      flags);

#define	RX_INFO_DST(ep,p,pp)	STMT_BEG					\
	if ((ep)->trace_frames)							\
		rtps_w_frame ((WRITER *) ep, 1, ST_INFO_DST, pp, p, 0); STMT_END
#define	RX_DATA(rp,pp,dp,fl)	STMT_BEG					\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 1, ST_DATA, pp, dp, fl); STMT_END
#define	RX_GAP(rp,pp,gp,fl)	STMT_BEG					\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 1, ST_GAP, pp, gp, fl); STMT_END
#define	RX_HEARTBEAT(rp,pp,hp,fl)	STMT_BEG				\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 1, ST_HEARTBEAT, pp, hp, fl); STMT_END
#define	RX_ACKNACK(wp,pp,ap,fl)	STMT_BEG					\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 1, ST_ACKNACK, pp, ap, fl); STMT_END
#define	TX_INFO_TS(wp,tp,fl)	STMT_BEG					\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 0, ST_INFO_TS, NULL, tp, fl); STMT_END
#define	TX_INFO_DST(ep,pp)	STMT_BEG					\
	if ((ep)->trace_frames)							\
		rtps_w_frame ((WRITER *) ep, 0, ST_INFO_DST, NULL, pp, 0); STMT_END
#define	TX_DATA(wp,dp,fl)	STMT_BEG					\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 0, ST_DATA, NULL, dp, fl); STMT_END
#define	TX_GAP(wp,gp,fl)	STMT_BEG					\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 0, ST_GAP, NULL, gp, fl); STMT_END
#define	TX_HEARTBEAT(wp,hp,fl)	STMT_BEG					\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 0, ST_HEARTBEAT, NULL, hp, fl); STMT_END
#define	TX_ACKNACK(rp,ap,fl)	STMT_BEG					\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 0, ST_ACKNACK, NULL, ap, fl); STMT_END
#ifdef RTPS_FRAGMENTS
#define	RX_DATA_FRAG(rp,pp,hp,fl)	STMT_BEG				\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 1, ST_DATA_FRAG, pp, dp, fl); STMT_END
#define	RX_HEARTBEAT_FRAG(rp,pp,hp,fl)	STMT_BEG				\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 1, ST_HEARTBEAT_FRAG, pp, hp, fl); STMT_END
#define	RX_NACK_FRAG(wp,pp,ap,fl)	STMT_BEG				\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 1, ST_NACK_FRAG, pp, ap, fl); STMT_END
#define	TX_DATA_FRAG(wp,dp,fl)	STMT_BEG					\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 0, ST_DATA_FRAG, NULL, dp, fl); STMT_END
#define	TX_HEARTBEAT_FRAG(wp,hp)	STMT_BEG				\
	if (wp->endpoint.trace_frames)						\
		rtps_w_frame (wp, 0, ST_HEARTBEAT_FRAG, NULL, hp, 0); STMT_END
#define	TX_NACK_FRAG(rp,ap)	STMT_BEG					\
	if (rp->endpoint.trace_frames)						\
		rtps_r_frame (rp, 0, ST_NACK_FRAG, NULL, ap, 0); STMT_END
#endif

#else

#define	NEW_RR_CSTATE(rrp,s,i)		rrp->rr_cstate = s
#define	NEW_RR_TSTATE(rrp,s,i)		rrp->rr_tstate = s
#define	NEW_RR_ASTATE(rrp,s,i)		rrp->rr_astate = s
#define	RR_SIGNAL(rrp,s)
#define	RESEND_TMR_START(wp,t,v,u,f,l)	tmr_start_lock (t,v,u,f,l)
#define	RESEND_TMR_STOP(wp,t)		tmr_stop (t)
#define	RESEND_TMR_TO(rrp)
#define	RESEND_TMR_ALLOC(rrp)
#define	RESEND_TMR_FREE(rrp)
#define	HEARTBEAT_TMR_START(wp,t,v,u,f,l) tmr_start_lock (t,v,u,f,l)
#define	HEARTBEAT_TMR_STOP(wp,t)	tmr_stop (t)
#define	HEARTBEAT_TMR_TO(rrp)
#define	HEARTBEAT_TMR_ALLOC(rrp)
#define	HEARTBEAT_TMR_FREE(rrp)
#define	NACKRSP_TMR_START(rrp,t,v,u,f,l) tmr_start_lock (t,v,u,f,l)
#define	NACKRSP_TMR_STOP(rrp,t)		tmr_stop (t)
#define	NACKRSP_TMR_TO(rrp)
#define	NACKRSP_TMR_ALLOC(rrp)
#define	NACKRSP_TMR_FREE(rrp)
#define	WALIVE_TMR_START(rrp,t,v,u,f,l)	tmr_start_lock (t,v,u,f,l)
#define	WALIVE_TMR_STOP(rrp,t)		tmr_stop (t)
#define	WALIVE_TMR_TO(rrp)
#define	WALIVE_TMR_ALLOC(rrp)
#define	WALIVE_TMR_FREE(rrp)
#define	NEW_RW_STATE(rwp,s,i)		rwp->rw_state = s
#define	RW_SIGNAL(rwp,s)/*                ctrc_printf(RTPS_ID,0, s) */
#define	HBRSP_TMR_START(rwp,t,v,u,f,l)	tmr_start_lock (t,v,u,f,l)
#define	HBRSP_TMR_STOP(rwp,t)		tmr_stop (t)
#define	HBRSP_TMR_TO(rwp)
#define	HBRSP_TMR_ALLOC(rwp)
#define	HBRSP_TMR_FREE(rwp)
#define	RALIVE_TMR_START(rwp,t,v,u,f,l)	tmr_start_lock (t,v,u,f,l)
#define	RALIVE_TMR_STOP(rwp,t)		tmr_stop (t)
#define	RALIVE_TMR_TO(rwp)
#define	RALIVE_TMR_ALLOC(rwp)
#define	RALIVE_TMR_FREE(rwp)
#define NEW_RW_CSTATE(rwp,s,i)		rwp->rw_cstate = s
#define NEW_RW_ASTATE(rwp,s,i)		rwp->rw_astate = s
#define	RX_INFO_DST(ep,p,pp)
#define	RX_DATA(rwp,p,dp,fl)
#define	RX_GAP(rwp,p,gp,fl)
#define	RX_HEARTBEAT(rwp,p,hp,fl)
#define	RX_ACKNACK(rrp,p,ap,fl)
#define	TX_INFO_TS(rrp,tp,fl)
#define	TX_INFO_DST(ep,pp)
#define	TX_DATA(rrp,dp,fl)
#define	TX_GAP(rrp,gp,fl)
#define	TX_HEARTBEAT(rrp,hp,fl)
#define	TX_ACKNACK(rwp,ap,fl)
#ifdef RTPS_FRAGMENTS
#define	RX_DATA_FRAG(rwp,p,hp,fl)
#define	RX_HEARTBEAT_FRAG(rwp,p,hp,fl)
#define	RX_NACK_FRAG(rrp,p,ap,fl)
#define	TX_DATA_FRAG(rrp,dp,fl)
#define	TX_HEARTBEAT_FRAG(rrp,hp)
#define	TX_NACK_FRAG(rwp,hp)
#define	FRAGSC_TMR_START(rwp,t,v,u,f,l)	tmr_start_lock (t,v,u,f,l)
#define	FRAGSC_TMR_STOP(rwp,t)		tmr_stop (t)
#endif
#endif

#if defined (RTPS_TRC_READER) || defined (RTPS_TRC_WRITER)

void sfx_dump_ccref (CCREF *rp, int show_state);

#endif
#ifdef RTPS_TRC_SEQNR

void trc_seqnr (RemWriter_t *rwp, const char *s);

#define	RW_SNR_TRACE(rwp,s) trc_seqnr (rwp, s)

#else
#define	RW_SNR_TRACE(rwp,s)
#endif

#ifdef RTPS_LOG_REPL_LOC
#define	lrloc_print(s)		 log_printf (RTPS_ID, 0, s)
#define	lrloc_print1(s,a)	 log_printf (RTPS_ID, 0, s, a)
#define	lrloc_print2(s,a1,a2)	 log_printf (RTPS_ID, 0, s, a1, a2)
#define	lrloc_print3(s,a1,a2,a3) log_printf (RTPS_ID, 0, s, a1, a2, a3)
#else
#define	lrloc_print(s)
#define	lrloc_print1(s,a)
#define	lrloc_print2(s,a1,a2)
#define	lrloc_print3(s,a1,a2,a3)
#endif

#endif /* !__rtps_trace_h_ */

