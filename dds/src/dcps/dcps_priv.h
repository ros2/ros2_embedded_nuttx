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

/* dcps_priv.h -- Private data of the DCPS layer. */

#ifndef __dcps_priv_h_
#define	__dcps_priv_h_

#define DCPS_BUILTIN_READERS	/* Define this for the DCPS Builtin Readers. */
/*#define DUMP_DDATA		** Define to dump rxed/txed dynamic data. */
/*#define TRACE_DELETE_CONTAINED** Define to trace deletion of contained data.*/

#ifdef TRACE_DELETE_CONTAINED
#define	dtrc_printf(s,n)	printf (s, n)
#else
#define	dtrc_printf(s,n)
#endif


enum mem_block_en {
	MB_SAMPLE_INFO,		/* DCPS SampleInfo. */
	MB_WAITSET,		/* DCPS WaitSet. */
	MB_STATUS_COND,		/* DCPS StatusCondition. */
	MB_READ_COND,		/* DCPS ReadCondition. */
	MB_QUERY_COND,		/* DCPS QueryCondition. */
	MB_GUARD_COND,		/* DCPS GuardCondition. */
	MB_TOPIC_WAIT,		/* Topic Wait context. */

	MB_END
};

extern MEM_DESC_ST 	dcps_mem_blocks [MB_END];  /* Memory used by DCPS. */
extern const char	*dcps_mem_names [];
extern unsigned		dcps_entity_count;

StatusCondition_t *dcps_new_status_condition (void);

void dcps_delete_status_condition (StatusCondition_t *cp);


Strings_t *dcps_new_str_pars (DDS_StringSeq *pars, int *error);

#define	dcps_free_str_pars	strings_delete

int dcps_update_str_pars (Strings_t **sp, DDS_StringSeq *pars);

int dcps_get_str_pars (DDS_StringSeq *pars, Strings_t *sp);

unsigned dcps_skip_mask (DDS_SampleStateMask   sample_states,
			 DDS_ViewStateMask     view_states,
			 DDS_InstanceStateMask instance_states);

#ifdef PROFILE
EXT_PROF_PID (dcps_create_part)
EXT_PROF_PID (dcps_delete_part)
EXT_PROF_PID (dcps_reg_type)
EXT_PROF_PID (dcps_unreg_type)
EXT_PROF_PID (dcps_create_pub)
EXT_PROF_PID (dcps_delete_pub)
EXT_PROF_PID (dcps_create_sub)
EXT_PROF_PID (dcps_delete_sub)
EXT_PROF_PID (dcps_create_topic)
EXT_PROF_PID (dcps_delete_topic_p)
EXT_PROF_PID (dcps_create_ftopic)
EXT_PROF_PID (dcps_delete_ftopic)
EXT_PROF_PID (dcps_create_writer)
EXT_PROF_PID (dcps_delete_writer_p)
EXT_PROF_PID (dcps_create_reader)
EXT_PROF_PID (dcps_delete_reader)
EXT_PROF_PID (dcps_register)
EXT_PROF_PID (dcps_write_p)
EXT_PROF_PID (dcps_dispose_p)
EXT_PROF_PID (dcps_unregister)
EXT_PROF_PID (dcps_w_key)
EXT_PROF_PID (dcps_w_lookup)
EXT_PROF_PID (dcps_read_take1)
EXT_PROF_PID (dcps_read_take2)
EXT_PROF_PID (dcps_read_take3)
EXT_PROF_PID (dcps_return_loan_p)
EXT_PROF_PID (dcps_r_key)
EXT_PROF_PID (dcps_r_lookup)
EXT_PROF_PID (dcps_ws_wait)
#endif
#ifdef CTRACE_USED

enum {
	DCPS_DTYPE_REG, DCPS_DTYPE_FREE, DCPS_SAMPLE_FREE,

	DCPS_E_G_SCOND, DCPS_E_G_SCH, DCPS_E_ENABLE, DCPS_E_G_HANDLE,

	DCPS_WS_ALLOC, DCPS_WS_FREE, DCPS_WS_ATT_COND, DCPS_WS_DET_COND,
	DCPS_WS_WAIT, DCPS_WS_G_CONDS,

	DCPS_GC_ALLOC, DCPS_GC_FREE, DCPS_GC_G_TRIG, DCPS_GC_S_TRIG,

	DCPS_SC_G_TRIG, DCPS_SC_S_STAT, DCPS_SC_G_STAT, DCPS_SC_G_ENT,

	DCPS_RC_G_TRIG, DCPS_RC_G_READ,
	DCPS_RC_G_VMASK, DCPS_RC_G_IMASK, DCPS_RC_G_SMASK,

	DCPS_QC_G_TRIG, DCPS_QC_G_READ,
	DCPS_QC_G_EXPR, DCPS_QC_G_PARS, DCPS_QC_S_PARS,
	DCPS_QC_G_VMASK, DCPS_QC_G_IMASK, DCPS_QC_G_SMASK,

	DCPS_DPF_C_PART, DCPS_DPF_D_PART, DCPS_DPF_L_PART,
	DCPS_DPF_S_DP_QOS, DCPS_DPF_G_DP_QOS, DCPS_DPF_G_QOS, DCPS_DPF_S_QOS,

	DCPS_DP_R_TYPE, DCPS_DP_U_TYPE, DCPS_DP_D_TS,
	DCPS_DP_G_QOS, DCPS_DP_S_QOS, DCPS_DP_G_LIS, DCPS_DP_S_LIS,
	DCPS_DP_G_SCOND, DCPS_DP_G_STAT, DCPS_DP_ENABLE, DCPS_DP_G_HANDLE,
	DCPS_DP_C_PUB, DCPS_DP_D_PUB, DCPS_DP_C_SUB, DCPS_DP_D_SUB,
	DCPS_DP_C_TOP, DCPS_DP_D_TOP, DCPS_DP_C_FTOP, DCPS_DP_D_FTOP,
	DCPS_DP_C_MTOP, DCPS_DP_D_MTOP, DCPS_DP_F_TOP, DCPS_DP_L_TD,
	DCPS_DP_G_BI_SUB,
	DCPS_DP_IGN_PART, DCPS_DP_IGN_TOP, DCPS_DP_IGN_PUB, DCPS_DP_IGN_SUB,
	DCPS_DP_G_ID, DCPS_DP_D_CONT, DCPS_DP_ASSERT,
	DCPS_DP_S_P_QOS, DCPS_DP_G_P_QOS, DCPS_DP_S_S_QOS, DCPS_DP_G_S_QOS,
	DCPS_DP_S_T_QOS, DCPS_DP_G_T_QOS,
	DCPS_DP_G_DISC_P_S, DCPS_DP_G_DISC_P,
	DCPS_DP_G_DISC_T_S, DCPS_DP_G_DISC_T,
	DCPS_DP_CONT, DCPS_DP_G_TIME, 

	DCPS_TD_G_PART, DCPS_TD_G_TNAME, DCPS_TD_G_NAME,

	DCPS_T_G_QOS, DCPS_T_S_QOS, DCPS_T_G_LIS, DCPS_T_S_LIS,
	DCPS_T_G_SCOND, DCPS_T_G_STAT, DCPS_T_ENABLE, DCPS_T_G_HANDLE,
	DCPS_T_G_PART, DCPS_T_G_TNAME, DCPS_T_G_NAME,
	DCPS_T_G_INC_ST,

	DCPS_FT_REL, DCPS_FT_G_PARS, DCPS_FT_S_PARS, DCPS_FT_G_EXPR,
	DCPS_FT_G_PART, DCPS_FT_G_TNAME, DCPS_FT_G_NAME,

	DCPS_P_G_QOS, DCPS_P_S_QOS, DCPS_P_G_LIS, DCPS_P_S_LIS,
	DCPS_P_G_SCOND, DCPS_P_G_STAT, DCPS_P_ENABLE, DCPS_P_G_HANDLE,
	DCPS_P_C_DW, DCPS_P_D_DW, DCPS_P_L_DW, DCPS_P_WACK,
	DCPS_P_G_PART, DCPS_P_D_CONT,
	DCPS_P_S_DW_QOS, DCPS_P_G_DW_QOS, DCPS_P_DW_F_TQOS,
	DCPS_P_SUSP, DCPS_P_RES, DCPS_P_BC, DCPS_P_EC,

	DCPS_DW_G_QOS, DCPS_DW_S_QOS, DCPS_DW_G_LIS, DCPS_DW_S_LIS,
	DCPS_DW_G_SCOND, DCPS_DW_G_STAT, DCPS_DW_ENABLE, DCPS_DW_G_HANDLE,
	DCPS_DW_R_INST, DCPS_DW_R_INST_TS,
	DCPS_DW_U_INST, DCPS_DW_U_INST_TS, DCPS_DW_U_INST_D, DCPS_DW_U_INST_TS_D,
	DCPS_DW_G_KEY, DCPS_DW_L_INST,
	DCPS_DW_WRITE, DCPS_DW_WRITE_TS, DCPS_DW_WRITE_D, DCPS_DW_WRITE_TS_D,
	DCPS_DW_DISP, DCPS_DW_DISP_TS, DCPS_DW_DISP_D, DCPS_DW_DISP_TS_D,
	DCPS_DW_WACKS,
	DCPS_DW_G_LL_ST, DCPS_DW_G_DLM_ST, DCPS_DW_G_OIQ_ST, DCPS_DW_G_PM_ST,
	DCPS_DW_G_TOP, DCPS_DW_G_PUB,
	DCPS_DW_G_SUBS_D, DCPS_DW_G_MATCH_S, DCPS_DW_G_REPLY_S,

	DCPS_S_G_QOS, DCPS_S_S_QOS, DCPS_S_G_LIS, DCPS_S_S_LIS,
	DCPS_S_G_SCOND, DCPS_S_G_STAT, DCPS_S_ENABLE, DCPS_S_G_HANDLE,
	DCPS_S_C_DR, DCPS_S_D_DR, DCPS_S_L_DR,
	DCPS_S_G_DR_S, DCPS_S_NOTIF_DR,
	DCPS_S_G_PART, DCPS_S_D_CONT,
	DCPS_S_S_DR_QOS, DCPS_S_G_DR_QOS, DCPS_S_DR_F_TQOS,
	DCPS_S_B_ACC, DCPS_S_E_ACC,

	DCPS_DR_G_QOS, DCPS_DR_S_QOS, DCPS_DR_G_LIS, DCPS_DR_S_LIS,
	DCPS_DR_G_SCOND, DCPS_DR_G_STAT, DCPS_DR_ENABLE, DCPS_DR_G_HANDLE,
	DCPS_DR_READ, DCPS_DR_TAKE, DCPS_DR_READC, DCPS_DR_TAKEC,
	DCPS_DR_R_NS, DCPS_DR_T_NS, DCPS_DR_R_INST, DCPS_DR_T_INST,
	DCPS_DR_R_NINST, DCPS_DR_T_NINST, DCPS_DR_R_NINSTC, DCPS_DR_T_NINSTC,
	DCPS_DR_RLOAN, DCPS_DR_G_KEY, DCPS_DR_L_INST,
	DCPS_DR_C_RCOND, DCPS_DR_C_QCOND, DCPS_DR_D_RCOND,
	DCPS_DR_G_LC_ST, DCPS_DR_G_RDM_ST, DCPS_DR_G_RIQ_ST, DCPS_DR_G_SL_ST,
	DCPS_DR_G_SR_ST, DCPS_DR_G_SM_ST,
	DCPS_DR_G_TD, DCPS_DR_G_SUB, DCPS_DR_D_CONT,
	DCPS_DR_W_HIST, DCPS_DR_G_PUB_D, DCPS_DR_G_MATCH_P,

	DCPS_NTF_D_AVAIL, DCPS_NTF_D_AV_IND, DCPS_NTF_D_OR_IND
};

extern const char *dcps_fct_str [];

#endif /* CTRACE_USED */

#endif	/* !__dcps_priv_h_ */


