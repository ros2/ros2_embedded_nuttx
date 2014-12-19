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

#if defined (NUTTX_RTOS)
#include "dds/dds_plugin.h"
#else
#include "dds/dds_security.h"
#endif
#include "dds/dds_xtypes.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_dreader.h"
#include "dcps.h"
#include "log.h"
#include "ctrace.h"
#include "prof.h"
#include "domain.h"
#include "xtopic.h"

#ifdef PROFILE
PROF_PID (xt_w_key)
PROF_PID (xt_w_lookup)
#endif

#ifdef CTRACE_USED
enum {
	XT_DW_R_INST, XT_DW_R_INST_TS,
	XT_DW_U_INST, XT_DW_U_INST_TS, XT_DW_U_INST_D, XT_DW_U_INST_TS_D,
	XT_DW_G_KEY, XT_DW_L_INST,
	XT_DW_WRITE, XT_DW_WRITE_TS, XT_DW_WRITE_D, XT_DW_WRITE_TS_D,
	XT_DW_DISP, XT_DW_DISP_TS, XT_DW_DISP_D, XT_DW_DISP_TS_D,

	XT_DR_READ, XT_DR_TAKE,	XT_DR_READC, XT_DR_TAKEC,
	XT_DR_R_NS, XT_DR_T_NS,
	XT_DR_R_INST, XT_DR_T_INST, XT_DR_R_NINST, XT_DR_T_NINST,
	XT_DR_R_NINSTC, XT_DR_T_NINSTC,
	XT_DR_RLOAN,
	XT_DR_G_KEY, XT_DR_L_INST
};

static const char *xt_fct_str [XT_DR_L_INST + 1] = {
	"DynamicDataWriter_register_instance",
	"DynamicDataWriter_register_instance_w_timestamp",
	"DynamicDataWriter_unregister_instance",
	"DynamicDataWriter_unregister_instance_w_timestamp",
	"DynamicDataWriter_unregister_instance_directed",
	"DynamicDataWriter_unregister_instance_w_timestamp_directed",
	"DynamicDataWriter_get_key_value",
	"DynamicDataWriter_lookup_instance",
	"DynamicDataWriter_write",
	"DynamicDataWriter_write_w_timestamp",
	"DynamicDataWriter_write_directed",
	"DynamicDataWriter_write_w_timestamp_directed",
	"DynamicDataWriter_dispose",
	"DynamicDataWriter_dispose_w_timestamp",
	"DynamicDataWriter_dispose_directed",
	"DynamicDataWriter_dispose_w_timestamp_directed"

	"DynamicDataReader_read",
	"DynamicDataReader_take",
	"DynamicDataReader_read_w_condition",
	"DynamicDataReader_take_w_condition",
	"DynamicDataReader_read_next_sample",
	"DynamicDataReader_take_next_sample",
	"DynamicDataReader_read_instance",
	"DynamicDataReader_take_instance",
	"DynamicDataReader_read_next_instance",
	"DynamicDataReader_take_next_instance",
	"DynamicDataReader_read_next_instance_w_condition",
	"DynamicDataReader_take_next_instance_w_condition",
	"DynamicDataReader_return_loan",
	"DynamicDataReader_get_key_value",
	"DynamicDataReader_lookup_instance"
};

#endif

void DDS_DynamicDataSeq__init (DDS_DynamicDataSeq *dyndata)
{
	DDS_SEQ_INIT (*dyndata);
}

void DDS_DynamicDataSeq__clear (DDS_DynamicDataSeq *dyndata)
{
	dds_seq_cleanup (dyndata);
}

DDS_DynamicDataSeq *DDS_DynamicDataSeq__alloc (void)
{
	DDS_DynamicDataSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_DynamicDataSeq));
	if (!p)
		return (NULL);

	DDS_DynamicDataSeq__init (p);
	return (p);
}

void DDS_DynamicDataSeq__free (DDS_DynamicDataSeq *dyndata)
{
	if (!dyndata)
		return;

	DDS_DynamicDataSeq__clear (dyndata);
	mm_fcts.free_ (dyndata);
}

void xtopic_init (void)
{
#ifdef CTRACE_USED
	log_fct_str [XTYPES_ID] = xt_fct_str;
#endif
#ifdef PROFILE
	PROF_INIT ("X:WKey", xt_w_key);
	PROF_INIT ("X:WLookup", xt_w_lookup);
#endif
}

/* DynamicDataWriter API functions: */

DDS_InstanceHandle_t DDS_DynamicDataWriter_register_instance (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_R_INST, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_HANDLE_NIL);

	sys_getftime (&time);
	return (dcps_register_instance (dw, drp->ddata, 1, &time));
}

DDS_InstanceHandle_t DDS_DynamicDataWriter_register_instance_w_timestamp (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_Time_t *time)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_R_INST_TS, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_HANDLE_NIL);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_register_instance (dw, drp->ddata, 1, &ftime));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_U_INST, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sys_getftime (&time);
	return (dcps_unregister_instance (dw, (drp) ? drp->ddata : NULL, 1, h, &time, NULL));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance_w_timestamp (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_U_INST_TS, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_unregister_instance (dw, (drp) ? drp->ddata : NULL, 1, h, &ftime, NULL));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance_directed (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_U_INST_D, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (dests, sizeof (*dests));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sys_getftime (&time);
	return (dcps_unregister_instance (dw, (drp) ? drp->ddata : NULL, 1, h, &time, dests));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_unregister_instance_w_timestamp_directed (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_U_INST_TS_D, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (time, sizeof (*time));
	ctrc_contd (dests, sizeof (*dests));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_unregister_instance (dw, (drp) ? drp->ddata : NULL, 1, h, &ftime, dests));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_get_key_value (
						DDS_DynamicDataWriter dw,
						DDS_DynamicData key_data,
						const DDS_InstanceHandle_t h)
{
	DynDataRef_t	 *drp = (DynDataRef_t *) key_data;
	Writer_t	 *wp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DW_G_KEY, &dw, sizeof (dw));
	ctrc_contd (&key_data, sizeof (key_data));
	ctrc_contd (&h, sizeof (h));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	prof_start (xt_w_key);
	wp = writer_ptr (dw, 1, &ret);
	if (!wp)
		return (ret);

	ret = hc_get_key (wp->w_cache, h, drp->ddata, 1);
	lock_release (wp->w_lock);
	prof_stop (xt_w_key, 1);
	return (ret);
}

DDS_InstanceHandle_t DDS_DynamicDataWriter_lookup_instance (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data)
{
	DynDataRef_t		*drp = (DynDataRef_t *) data;
	Writer_t	 	*wp;
	InstanceHandle		h;
	DDS_ReturnCode_t	ret;

	ctrc_begind (XTYPES_ID, XT_DW_L_INST, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_HANDLE_NIL);

	prof_start (xt_w_lookup);
	if (!data)
		return (DDS_HANDLE_NIL);

	wp = writer_ptr (dw, 1, &ret);
	if (!wp)
		return (DDS_HANDLE_NIL);

	handle_get (wp->w_topic, wp->w_cache, drp->ddata, 1, 
					ENC_DATA (&wp->w_lep), &h, &ret);
	lock_release (wp->w_lock);
	prof_stop (xt_w_lookup, 1);
	return ((DDS_InstanceHandle_t) h);
}

DDS_ReturnCode_t DDS_DynamicDataWriter_write (DDS_DynamicDataWriter dw,
					      const DDS_DynamicData data,
					      const DDS_InstanceHandle_t h)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_WRITE, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sys_getftime (&time);
	return (dcps_write (dw, drp->ddata, 1, h, &time, NULL));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_write_w_timestamp (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_WRITE_TS, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_write (dw, drp->ddata, 1, h, &ftime, NULL));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_write_directed (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_WRITE_D, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (&dests, sizeof (dests));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sys_getftime (&time);
	return (dcps_write (dw, drp->ddata, 1, h, &time, dests));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_write_w_timestamp_directed (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_WRITE_TS_D, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (time, sizeof (*time));
	ctrc_contd (dests, sizeof (dests));
	ctrc_endd ();

	if (!drp || drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_write (dw, drp->ddata, 1, h, &ftime, dests));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_dispose (DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_DISP, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sys_getftime (&time);
	return (dcps_dispose (dw, (drp) ? drp->ddata : NULL, 1, h, &time, NULL));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_dispose_w_timestamp (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_DISP_TS, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_dispose (dw, (drp) ? drp->ddata : NULL, 1, h, &ftime, NULL));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_dispose_directed (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						DDS_InstanceHandleSeq *dests)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		time;

	ctrc_begind (XTYPES_ID, XT_DW_DISP_D, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (&dests, sizeof (dests));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	sys_getftime (&time);
	return (dcps_dispose (dw, (drp) ? drp->ddata : NULL, 1, h, &time, dests));
}

DDS_ReturnCode_t DDS_DynamicDataWriter_dispose_w_timestamp_directed (
						DDS_DynamicDataWriter dw,
						const DDS_DynamicData data,
						const DDS_InstanceHandle_t h,
						const DDS_Time_t *time,
						DDS_InstanceHandleSeq *dests)
{
	DynDataRef_t	*drp = (DynDataRef_t *) data;
	FTime_t		ftime;

	ctrc_begind (XTYPES_ID, XT_DW_DISP_TS_D, &dw, sizeof (dw));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_contd (time, sizeof (*time));
	ctrc_contd (&dests, sizeof (dests));
	ctrc_endd ();

	if (drp && drp->magic != DD_MAGIC)
		return (DDS_RETCODE_BAD_PARAMETER);

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_dispose (dw, (drp) ? drp->ddata : NULL, 1, h, &ftime, dests));
}


/* DynamicDataReader API functions: */

DDS_ReturnCode_t DDS_DynamicDataReader_read (DDS_DynamicDataReader dr,
					     DDS_DynamicDataSeq *data_seq,
					     DDS_SampleInfoSeq *info_seq,
					     unsigned max_samples,
					     DDS_SampleStateMask sample_states,
					     DDS_ViewStateMask view_states,
					     DDS_InstanceStateMask inst_states)
{
	Reader_t	 *rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_READ, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&inst_states, sizeof (inst_states));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, sample_states, view_states, 
			       inst_states, NULL, 0, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_take (DDS_DynamicDataReader dr,
					     DDS_DynamicDataSeq *data_seq,
					     DDS_SampleInfoSeq *info_seq,
					     unsigned max_samples,
					     DDS_SampleStateMask sample_states,
					     DDS_ViewStateMask view_states,
					     DDS_InstanceStateMask inst_states)
{
	Reader_t	 *rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_TAKE, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&inst_states, sizeof (inst_states));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, sample_states, view_states,
			       inst_states, NULL, 0, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_read_w_condition (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_ReadCondition rcp)
{
	Reader_t		*rp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (XTYPES_ID, XT_DR_READC, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&rcp, sizeof (rcp));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != (Reader_t *) dr)
		return (DDS_RETCODE_BAD_PARAMETER);

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, rcp->sample_states, 
			       rcp->view_states, rcp->instance_states,
				(rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp : 
					NULL, 0, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_take_w_condition (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_ReadCondition rcp)
{
	Reader_t		*rp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (XTYPES_ID, XT_DR_TAKEC, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&rcp, sizeof (rcp));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != (Reader_t *) dr)
		return (DDS_RETCODE_BAD_PARAMETER);

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, rcp->sample_states,
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
				    (QueryCondition_t *) rcp : NULL, 0, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_read_next_sample (
						DDS_DynamicDataReader dr,
						DDS_DynamicData *value,
						DDS_SampleInfo *sample_info)
{
	Reader_t		*rp;
	void			*ptr;
	DDS_SampleInfo		*info;
	DDS_VoidPtrSeq		data;
	DDS_SampleInfoSeq	sinfo;
	DDS_ReturnCode_t 	ret;

	ctrc_begind (XTYPES_ID, XT_DR_R_NS, &dr, sizeof (dr));
	ctrc_contd (value, sizeof (value));
	ctrc_contd (&sample_info, sizeof (sample_info));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	if (rp->r_topic->type->type_support->ts_dynamic) {
		lock_release (rp->r_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_SEQ_INIT_PTR (data, ptr);
	ptr = value;
	DDS_SEQ_INIT_PTR (sinfo, info);
	info = sample_info;
	ret = dcps_reader_get (rp, &data, 1, &sinfo, 1,
			  DDS_NOT_READ_SAMPLE_STATE,
			  DDS_ANY_VIEW_STATE,
			  DDS_ANY_INSTANCE_STATE, NULL, 0, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_take_next_sample (
						DDS_DynamicDataReader dr,
						DDS_DynamicData *value,
						DDS_SampleInfo *sample_info)
{
	Reader_t		*rp;
	void			*ptr;
	DDS_SampleInfo		*info;
	DDS_VoidPtrSeq		data;
	DDS_SampleInfoSeq	sinfo;
	DDS_ReturnCode_t 	ret;

	ctrc_begind (XTYPES_ID, XT_DR_T_NS, &dr, sizeof (dr));
	ctrc_contd (value, sizeof (value));
	ctrc_contd (&sample_info, sizeof (sample_info));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	if (rp->r_topic->type->type_support->ts_dynamic) {
		lock_release (rp->r_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_SEQ_INIT_PTR (data, ptr);
	ptr = value;
	DDS_SEQ_INIT_PTR (sinfo, info);
	info = sample_info;
	ret = dcps_reader_get (rp, &data, 1, &sinfo, 1,
			  DDS_NOT_READ_SAMPLE_STATE,
			  DDS_ANY_VIEW_STATE,
			  DDS_ANY_INSTANCE_STATE, NULL, 0, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_read_instance (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_InstanceHandle_t handle,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	Reader_t	 *rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_R_INST, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&inst_states, sizeof (inst_states));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, sample_states, view_states,
			       inst_states, NULL, handle, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_take_instance (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_InstanceHandle_t handle,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	Reader_t	*rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_T_INST, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&inst_states, sizeof (inst_states));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, sample_states, view_states,
			       inst_states, NULL, handle, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_read_next_instance (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_InstanceHandle_t handle,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	Reader_t	*rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_R_NINST, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&inst_states, sizeof (inst_states));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, sample_states, view_states,
			       inst_states, NULL, handle, 1, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_take_next_instance (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_InstanceHandle_t handle,
						DDS_SampleStateMask sample_states,
						DDS_ViewStateMask view_states,
						DDS_InstanceStateMask inst_states)
{
	Reader_t	*rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_T_NINST, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&inst_states, sizeof (inst_states));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, sample_states, view_states,
			       inst_states, NULL, handle, 1, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_read_next_instance_w_condition (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_InstanceHandle_t handle,
						DDS_ReadCondition rcp)
{
	Reader_t		*rp;
	DDS_ReturnCode_t 	ret;

	ctrc_begind (XTYPES_ID, XT_DR_R_NINSTC, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&rcp, sizeof (rcp));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != (Reader_t *) dr)
		return (DDS_RETCODE_BAD_PARAMETER);

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, rcp->sample_states, 
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp : 
					NULL, handle, 1, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_take_next_instance_w_condition (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq,
						unsigned max_samples,
						DDS_InstanceHandle_t handle,
						DDS_ReadCondition rcp)
{
	Reader_t		*rp;
	DDS_ReturnCode_t	ret;

	ctrc_begind (XTYPES_ID, XT_DR_T_NINSTC, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&rcp, sizeof (rcp));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != (Reader_t *) dr)
		return (DDS_RETCODE_BAD_PARAMETER);

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) data_seq, 1, info_seq,
			       max_samples, rcp->sample_states,
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp :
					NULL, handle, 1, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_return_loan (
						DDS_DynamicDataReader dr,
						DDS_DynamicDataSeq *data_seq,
						DDS_SampleInfoSeq *info_seq)
{
	Reader_t	 *rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_RLOAN, &dr, sizeof (dr));
	ctrc_contd (&data_seq, sizeof (data_seq));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = dcps_return_loan ((Reader_t *) dr, (DDS_VoidPtrSeq *) data_seq, 1, info_seq);

	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DynamicDataReader_get_key_value (
						DDS_DynamicDataReader dr,
						DDS_DynamicData data,
						DDS_InstanceHandle_t handle)
{
	Reader_t	 *rp;
	DDS_ReturnCode_t ret;

	ctrc_begind (XTYPES_ID, XT_DR_G_KEY, &dr, sizeof (dr));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (ret);

	ret = hc_get_key (rp->r_cache, handle, data, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_InstanceHandle_t DDS_DynamicDataReader_lookup_instance (
						DDS_DynamicDataReader dr,
						const DDS_DynamicData key_data)
{
	Reader_t	 	*rp;
	InstanceHandle		h;
	DDS_ReturnCode_t	ret;

	ctrc_begind (XTYPES_ID, XT_DR_L_INST, &dr, sizeof (dr));
	ctrc_contd (&key_data, sizeof (key_data));
	ctrc_endd ();

	if (!key_data)
		return (DDS_HANDLE_NIL);

	rp = reader_ptr (dr, 1, &ret);
	if (!rp)
		return (DDS_HANDLE_NIL);

	handle_get (rp->r_topic, rp->r_cache, key_data, 1,
					ENC_DATA (&rp->r_lep), &h, &ret);
	lock_release (rp->r_lock);
	return ((DDS_InstanceHandle_t) h);
}
