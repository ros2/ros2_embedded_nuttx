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

/* dcps_writer.c -- DCPS API - DataWriter functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "sys.h"
#include "log.h"
#include "ctrace.h"
#include "prof.h"
#include "error.h"
#include "str.h"
#if defined (NUTTX_RTOS)
#include "dds/dds_plugin.h"
#else
#include "dds/dds_security.h"
#endif
#include "dds/dds_dcps.h"
#include "dds_data.h"
#include "domain.h"
#include "dds.h"
#include "disc.h"
#include "dcps_priv.h"
#include "dcps_event.h"
#include "dcps_pub.h"
#include "dcps_topic.h"
#include "dcps_builtin.h"
#include "dcps_writer.h"

DDS_ReturnCode_t DDS_DataWriter_get_qos (DDS_DataWriter wp,
					 DDS_DataWriterQos *qos)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_QOS, &wp, sizeof (wp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	qos_writer_get (wp->w_qos, qos);
	lock_release (wp->w_lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DataWriter_set_qos (DDS_DataWriter wp,
					 DDS_DataWriterQos *qos)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DW_S_QOS, &wp, sizeof (wp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	if (qos == DDS_DATAWRITER_QOS_DEFAULT)
		qos = &wp->w_publisher->def_writer_qos;
	else if (!qos_valid_writer_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = qos_writer_update (&wp->w_qos, qos);

    done:
	lock_release (wp->w_lock);

	if (ret)
		return (ret);

	lock_take (wp->w_topic->domain->lock);
	dcps_update_writer_qos (NULL, &wp, wp->w_publisher);
	lock_release (wp->w_topic->domain->lock);

	return (ret);
}

DDS_DataWriterListener *DDS_DataWriter_get_listener (DDS_DataWriter wp)
{
	ctrc_printd (DCPS_ID, DCPS_DW_G_LIS, &wp, sizeof (wp));

	if (!writer_ptr (wp, 0, NULL))
		return (NULL);

	return (&wp->w_listener);
}

DDS_ReturnCode_t DDS_DataWriter_set_listener (DDS_DataWriter wp,
					      DDS_DataWriterListener *listener,
					      DDS_StatusMask mask)
{
	DDS_ReturnCode_t ret;

	ctrc_printd (DCPS_ID, DCPS_DW_S_LIS, &wp, sizeof (wp));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	if (listener) 
		wp->w_listener.cookie = listener->cookie;

	dcps_update_listener ((Entity_t *) wp, &wp->w_lock,
			      &wp->w_mask, &wp->w_listener,
			      mask, listener);
	lock_release (wp->w_lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_DataWriter_get_status_changes (DDS_DataWriter wp)
{
	DDS_StatusMask	m;
	
	ctrc_printd (DCPS_ID, DCPS_DW_G_STAT, &wp, sizeof (wp));

	if (!writer_ptr (wp, 1, NULL))
		return (0);

	m = wp->w_status;
	lock_release (wp->w_lock);
	return (m);
}

DDS_ReturnCode_t DDS_DataWriter_enable (DDS_DataWriter wp)
{
	Topic_t			*tp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_DW_ENABLE, &wp, sizeof (wp));

	if (!writer_ptr (wp, 0, &ret))
		return (ret);

	tp = wp->w_topic;
	lock_take (tp->domain->lock);
	lock_take (tp->lock);
	if ((tp->entity.flags & EF_ENABLED) == 0 ||
	    (wp->w_publisher->entity.flags & EF_ENABLED) == 0) {
		lock_release (tp->domain->lock);
		lock_release (tp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
#ifdef RW_LOCKS
	lock_take (wp->w_lock);
#endif
	if ((wp->w_flags & EF_ENABLED) == 0) {

		/* Deliver new publication endpoint to the Discovery subsystem. */
		wp->w_flags |= EF_ENABLED | EF_NOT_IGNORED;
		hc_enable (wp->w_cache);
		if ((wp->w_publisher->entity.flags & EF_SUSPEND) != 0)
			dcps_suspended_publication_add (wp->w_publisher, wp, 1);
		else
			disc_writer_add (wp->w_publisher->domain, wp);

	}
#ifdef RW_LOCKS
	lock_release (wp->w_lock);
#endif
	lock_release (tp->lock);
	lock_release (tp->domain->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusCondition DDS_DataWriter_get_statuscondition (DDS_DataWriter wp)
{
	StatusCondition_t	*scp;

	ctrc_printd (DCPS_ID, DCPS_DW_G_SCOND, &wp, sizeof (wp));

	if (!writer_ptr (wp, 1, NULL))
		return (NULL);

	scp = wp->w_condition;
	if (!scp) {
		scp = dcps_new_status_condition ();
		if (!scp)
			return (NULL);

		scp->entity = (Entity_t *) wp;
		wp->w_condition = scp;
	}
	lock_release (wp->w_lock);
	return ((DDS_StatusCondition) scp);
}

DDS_InstanceHandle_t DDS_DataWriter_get_instance_handle (DDS_DataWriter wp)
{
	DDS_InstanceHandle_t	h;

	ctrc_printd (DCPS_ID, DCPS_DW_G_HANDLE, &wp, sizeof (wp));

	if (!writer_ptr (wp, 1, NULL))
		return (0);

	h = wp->w_handle;
	lock_release (wp->w_lock);
	return (h);
}

DDS_InstanceHandle_t dcps_register_instance (DDS_DataWriter   wp,
					     const void       *instance_data,
					     int              dynamic,
					     const FTime_t    *time)
{
	size_t			size;
	unsigned char		*keys;
	InstanceHandle		h;
	DDS_InstanceHandle_t	handle;
	DDS_ReturnCode_t	ret;
	unsigned char		buf [16];

	prof_start (dcps_register);

	if (!writer_ptr (wp, 1, NULL)) {
		warn_printf ("DDS_DataWriter_register_instance: invalid DataWriter!");
		return (DDS_HANDLE_NIL);
	}
	keys = dcps_key_data_get (wp->w_topic, instance_data, dynamic, 
					ENC_DATA (&wp->w_lep), buf, &size, &ret);
	if (!keys) {
		warn_printf ("DDS_DataWriter_register_instance: invalid parameters!");
		handle = DDS_HANDLE_NIL;
		goto done;
	}
	if (!hc_register (wp->w_cache, keys, size, time, &h)) {
		warn_printf ("DDS_DataWriter_register_instance: cache_register_instance() failed!");
		if (size > sizeof (buf))
			xfree (keys);

		handle = DDS_HANDLE_NIL;
		goto done;
	}
	if (size > sizeof (buf))
		xfree (keys);
	handle = (DDS_InstanceHandle_t) h;

    done:
	lock_release (wp->w_lock);
	prof_stop (dcps_register, 1);
	return (handle);
}

DDS_InstanceHandle_t DDS_DataWriter_register_instance_w_timestamp (
					DDS_DataWriter   wp,
					const void       *data,
					const DDS_Time_t *time)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_R_INST_TS, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_register_instance (wp, data, 0, &ftime));
}

DDS_InstanceHandle_t DDS_DataWriter_register_instance (DDS_DataWriter wp,
						       const void     *data)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_R_INST, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_register_instance (wp, data, 0, &time));
}

DDS_ReturnCode_t dcps_unregister_instance (DDS_DataWriter             wp,
					   const void                 *instance_data,
					   int                        dynamic,
					   const DDS_InstanceHandle_t handle,
					   const FTime_t              *time,
					   DDS_InstanceHandleSeq      *dests)
{
	HCI			hci;
	InstanceHandle		h;
	handle_t		d [MAX_DW_DESTS];
	unsigned		i, ndests;
	DDS_ReturnCode_t	ret;

	prof_start (dcps_unregister);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	if (dests) {
		if (!dests->_length || !dests->_buffer) {
			ret = DDS_RETCODE_BAD_PARAMETER;
			goto done;
		}
		else if (dests->_length > MAX_DW_DESTS) {
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto done;
		}
		for (i = 0; i < dests->_length; i++)
			d [i] = dests->_buffer [i];
		while (i < MAX_DW_DESTS)
			d [i++] = 0;
		ndests = dests->_length;
	}
	else
		ndests = 0;
	if (!wp->w_topic->type->type_support->ts_keys) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (instance_data && handle == DDS_HANDLE_NIL) {
		hci = handle_get (wp->w_topic, wp->w_cache, instance_data, 
				dynamic, ENC_DATA (&wp->w_lep), &h, &ret);
		if (!hci)
			goto done;
	}
	else if (handle != DDS_HANDLE_NIL) {
		h = (InstanceHandle) handle;
		hci = NULL;
	}
	else {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = hc_unregister (wp->w_cache, h, hci, time, d, ndests);

    done:
	lock_release (wp->w_lock);
	prof_stop (dcps_unregister, 1);
	return (ret);
}

DDS_ReturnCode_t DDS_DataWriter_unregister_instance_w_timestamp (
						DDS_DataWriter             wp,
						const void                 *instance_data,
						const DDS_InstanceHandle_t handle,
						const DDS_Time_t           *time)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_U_INST_TS, &wp, sizeof (wp));
	ctrc_contd (&instance_data, sizeof (instance_data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_unregister_instance (wp, instance_data, 0, handle, &ftime, NULL));
}

DDS_ReturnCode_t DDS_DataWriter_unregister_instance (DDS_DataWriter             wp,
						     const void                 *data,
						     const DDS_InstanceHandle_t handle)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_U_INST, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_unregister_instance (wp, data, 0, handle, &time, NULL));
}

DDS_ReturnCode_t DDS_DataWriter_unregister_instance_w_timestamp_directed (
						DDS_DataWriter             wp,
						const void                 *data,
						const DDS_InstanceHandle_t handle,
						const DDS_Time_t           *time,
						DDS_InstanceHandleSeq      *dests)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_U_INST_TS_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (time, sizeof (*time));
	ctrc_contd (dests, sizeof (*dests));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_unregister_instance (wp, data, 0, handle, &ftime, dests));
}

DDS_ReturnCode_t DDS_DataWriter_unregister_instance_directed (
						DDS_DataWriter             wp,
						const void                 *data,
						const DDS_InstanceHandle_t handle,
						DDS_InstanceHandleSeq      *dests)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_U_INST_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (dests, sizeof (*dests));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_unregister_instance (wp, data, 0, handle, &time, dests));
}

DDS_ReturnCode_t DDS_DataWriter_get_key_value (DDS_DataWriter             wp,
					       void                       *data,
					       const DDS_InstanceHandle_t h)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_KEY, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&h, sizeof (h));
	ctrc_endd ();

	prof_start (dcps_w_key);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	ret = hc_get_key (wp->w_cache, h, data, 0);
	lock_release (wp->w_lock);
	prof_stop (dcps_w_key, 1);
	return (ret);
}
			
DDS_InstanceHandle_t DDS_DataWriter_lookup_instance (DDS_DataWriter wp,
						     const void     *key_data)
{
	InstanceHandle		h;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DW_L_INST, &wp, sizeof (wp));
	ctrc_contd (&key_data, sizeof (key_data));
	ctrc_endd ();

	prof_start (dcps_w_lookup);
	if (!key_data)
		return (DDS_HANDLE_NIL);

	if (!writer_ptr (wp, 1, &ret))
		return (DDS_HANDLE_NIL);

	handle_get (wp->w_topic, wp->w_cache, key_data, 0, 
					ENC_DATA (&wp->w_lep), &h, &ret);
	lock_release (wp->w_lock);
	prof_stop (dcps_w_lookup, 1);
	return ((DDS_InstanceHandle_t) h);
}

DDS_ReturnCode_t dcps_write (DDS_DataWriter             wp,
			     const void                 *instance_data,
			     int                        dynamic,
			     const DDS_InstanceHandle_t handle,
			     const FTime_t              *time,
			     DDS_InstanceHandleSeq      *dests)
{
	Change_t		*cp;
	unsigned char		*keys;
	unsigned char		*dp;
	DB			*dbp;
	HCI			hci;
	size_t			tlen, ofs, size;
	InstanceHandle		h;
	const TypeSupport_t	*ts;
	unsigned		i;
	DDS_ReturnCode_t	ret;
	unsigned char		buf [16];

	prof_start (dcps_write_p);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	if (dests) {
		if (!dests->_length || !dests->_buffer) {
			ret = DDS_RETCODE_BAD_PARAMETER;
			goto done;
		}
		else if (dests->_length > MAX_DW_DESTS) {
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto done;
		}
	}
	if (!wp->w_pm_status.current_count &&
	    wp->w_qos->qos.durability_kind == DDS_VOLATILE_DURABILITY_QOS &&
	    (wp->w_flags & EF_BUILTIN) == 0) {
		ret = DDS_RETCODE_OK;
		goto done;
	}

	h = handle;
	ts = wp->w_topic->type->type_support;
	if (!handle && ts->ts_keys) {
		keys = dcps_key_data_get (wp->w_topic, instance_data, dynamic, ENC_DATA (&wp->w_lep), buf, &size, &ret);
		if (!keys) {
			warn_printf ("DDS_DataWriter_write: invalid parameters!");
			goto done;
		}
		hci = hc_register (wp->w_cache, keys, size, time, &h);
		if (!hci) {
			warn_printf ("DDS_DataWriter_write: cache_register_instance() failed!");
			if (size > sizeof (buf))
				xfree (keys);

			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto done;
		}
		if (size > sizeof (buf))
			xfree (keys);
	}
	else
		hci = NULL;

	if (!hc_write_required (wp->w_cache)) {
		ret = DDS_RETCODE_OK;
		goto done;
	}

	/* Allocate a new change record. */
	cp = hc_change_new ();
	if (!cp) {
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
		goto done;
	}
#if defined (DDS_DEBUG) && defined (DUMP_DDATA)
	if (dynamic) {
		dbg_printf ("dcps_write:\r\n");
		xd_dump (1, instance_data);
	}
#endif
	if (ts->ts_dynamic || ts->ts_length > 512) {
		ofs = 0;
		tlen = DDS_MarshalledDataSize (instance_data, dynamic, ts, &ret);
		if (ret) {
			log_printf (DCPS_ID, 0, "DDS_DataWriter_write({%u}): marshalled buffer size could not be determined (%d)!\r\n", 
						wp->w_handle, ret);
			goto free_data;
		}
		if (tlen > dds_max_sample_size) {
			log_printf (DCPS_ID, 0, "DDS_DataWriter_write({u}): marshalled buffer size exceeds system limits (%lu/%lu bytes)\r\n",
						(unsigned long) tlen, (unsigned long) dds_max_sample_size);
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto free_data;
		}
	}
	else {
		if (ts->ts_length > dds_max_sample_size) {
			log_printf (DCPS_ID, 0, "DDS_DataWriter_write({u}): sample size exceeds system limits (%lu/%lu bytes)\r\n",
						(unsigned long) ts->ts_length, (unsigned long) dds_max_sample_size);
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto free_data;
		}
		ofs = 4;
		tlen = ts->ts_length + 4;
		if (tlen <= C_DSIZE) {
			cp->c_db = NULL;
			dp = cp->c_xdata;
			memcpy (dp + ofs, instance_data, ts->ts_length);
			dp [0] = dp [2] = dp [3] = 0;
			dp [1] = (MODE_RAW << 1) | ENDIAN_CPU;
			goto data_copied;
		}
	}

	/* Allocate a container to store the data. */
	if ((dbp = db_alloc_data (tlen, 1)) == NULL) {
		warn_printf ("DDS_DataWriter_write({%u}): out of memory for data (%lu bytes)!\r\n",
					wp->w_handle, (unsigned long) tlen);
		ret = DDS_RETCODE_OUT_OF_RESOURCES;
		goto free_data;
	}
	cp->c_db = dbp;
	dp = dbp->data;
	if (ts->ts_dynamic || ts->ts_length > 512) {

		/* Add marshalled data to DB chain, prefixed with marshalling type. */
		ret = DDS_MarshallData (dp, instance_data, dynamic, ts);
		if (ret) {
			log_printf (DCPS_ID, 0, "DDS_DataWriter_write({%u}): error %u marshalling data!\r\n",
					wp->w_handle, ret);
			goto free_data;
		}
	}
	else {
		/* Add raw data to DB chain, prefixed with RAW identifier. */
		db_put_data (dbp, 4, instance_data, ts->ts_length);
		dp [0] = dp [2] = dp [3] = 0;
		dp [1] = (MODE_RAW << 1) | ENDIAN_CPU;
	}

    data_copied:
	cp->c_data = dp;

     /*	cp->c_wack = 0; */
	cp->c_kind = ALIVE;
	cp->c_linear = 1;
	cp->c_writer = wp->w_handle;
	cp->c_time = *time;
	cp->c_handle = h;
	cp->c_length = tlen;
	if (!dests)
		cp->c_dests [0] = 0;
	else {
		for (i = 0; i < dests->_length; i++)
			cp->c_dests [i] = dests->_buffer [i];
		while (i < MAX_DW_DESTS)
			cp->c_dests [i++] = 0;
	}
	ret = hc_add_inst (wp->w_cache, cp, hci, 0);
	goto done;

    free_data:
    	hc_change_free (cp);

    done:
	lock_release (wp->w_lock);
	prof_stop (dcps_write_p, 1);
	return (ret);
}

DDS_ReturnCode_t DDS_DataWriter_write (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_WRITE, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_write (wp, data, 0, handle, &time, NULL));
}

DDS_ReturnCode_t DDS_DataWriter_write_w_timestamp (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle,
				const DDS_Time_t           *time)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_WRITE_TS, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_write (wp, data, 0, handle, &ftime, NULL));
}

DDS_ReturnCode_t DDS_DataWriter_write_directed (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle,
				DDS_InstanceHandleSeq      *dests)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_WRITE_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&dests, sizeof (dests));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_write (wp, data, 0, handle, &time, dests));
}

DDS_ReturnCode_t DDS_DataWriter_write_w_timestamp_directed (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle,
				const DDS_Time_t           *time,
				DDS_InstanceHandleSeq      *dests)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_WRITE_TS_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (time, sizeof (*time));
	ctrc_contd (dests, sizeof (dests));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_write (wp, data, 0, handle, &ftime, dests));
}

DDS_ReturnCode_t dcps_dispose (DDS_DataWriter             wp,
			       const void                 *instance_data,
			       int                        dynamic,
			       const DDS_InstanceHandle_t handle,
			       const FTime_t              *time,
			       DDS_InstanceHandleSeq      *dests)
{
	const TypeSupport_t	*ts;
	InstanceHandle		h;
	HCI			hci;
	handle_t		d [MAX_DW_DESTS];
	unsigned		i, ndests;
	DDS_ReturnCode_t	ret;

	prof_start (dcps_dispose_p);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	if (dests) {
		if (!dests->_length || !dests->_buffer) {
			ret = DDS_RETCODE_BAD_PARAMETER;
			goto done;
		}
		else if (dests->_length > MAX_DW_DESTS) {
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto done;
		}
		for (i = 0; i < dests->_length; i++)
			d [i] = dests->_buffer [i];
		while (i < MAX_DW_DESTS)
			d [i++] = 0;
		ndests = dests->_length;
	}
	else
		ndests = 0;
	if (!wp->w_pm_status.current_count &&
	    wp->w_qos->qos.durability_kind == DDS_VOLATILE_DURABILITY_QOS) {
		ret = DDS_RETCODE_OK;
		goto done;
	}

	ts = wp->w_topic->type->type_support;
	if (!ts->ts_keys) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (instance_data && handle == DDS_HANDLE_NIL) {
		hci = handle_get (wp->w_topic, wp->w_cache, instance_data, 
				      dynamic, ENC_DATA (&wp->w_lep), &h, &ret);
		if (!hci)
			goto done;
	}
	else if (handle != DDS_HANDLE_NIL) {
		h = handle;
		hci = NULL;
	}
	else {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = hc_dispose (wp->w_cache, (InstanceHandle) h, hci, time, d, ndests);

    done:
	lock_release (wp->w_lock);
	prof_stop (dcps_dispose_p, 1);
	return (ret);
}

DDS_ReturnCode_t DDS_DataWriter_dispose (DDS_DataWriter             wp,
					 const void                 *data,
					 const DDS_InstanceHandle_t handle)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_DISP, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_dispose (wp, data, 0, handle, &time, NULL));
}

DDS_ReturnCode_t DDS_DataWriter_dispose_w_timestamp (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle,
				const DDS_Time_t           *time)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_DISP_TS, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (time, sizeof (*time));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_dispose (wp, data, 0, handle, &ftime, NULL));
}

DDS_ReturnCode_t DDS_DataWriter_dispose_directed (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle,
				DDS_InstanceHandleSeq      *dests)
{
	FTime_t	time;

	ctrc_begind (DCPS_ID, DCPS_DW_DISP_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&dests, sizeof (dests));
	ctrc_endd ();

	sys_getftime (&time);
	return (dcps_dispose (wp, data, 0, handle, &time, dests));
}

DDS_ReturnCode_t DDS_DataWriter_dispose_w_timestamp_directed (
				DDS_DataWriter             wp,
				const void                 *data,
				const DDS_InstanceHandle_t handle,
				const DDS_Time_t           *time,
				DDS_InstanceHandleSeq      *dests)
{
	FTime_t	ftime;

	ctrc_begind (DCPS_ID, DCPS_DW_DISP_TS_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (time, sizeof (*time));
	ctrc_contd (&dests, sizeof (dests));
	ctrc_endd ();

	FTIME_SET (ftime, time->sec, time->nanosec);
	return (dcps_dispose (wp, data, 0, handle, &ftime, dests));
}

DDS_ReturnCode_t DDS_DataWriter_wait_for_acknowledgments (
					DDS_DataWriter wp,
					const DDS_Duration_t *max_wait)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DW_WACKS, &wp, sizeof (wp));
	ctrc_contd (max_wait, sizeof (max_wait));
	ctrc_endd ();

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	ret = hc_wait_acks (wp->w_cache, (const Duration_t *) max_wait);

	lock_release (wp->w_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataWriter_assert_liveliness (DDS_DataWriter wp)
{
	DDS_ReturnCode_t ret;

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	ret = hc_alive (wp->w_cache);

	lock_release (wp->w_lock);
	return (ret);
}

DDS_Topic DDS_DataWriter_get_topic (DDS_DataWriter wp)
{
	Endpoint_t	*ep;
	Topic_t		*tp;

	ctrc_printd (DCPS_ID, DCPS_DW_G_TOP, &wp, sizeof (wp));

	if (!writer_ptr (wp, 0, NULL))
		return (NULL);

	tp = wp->w_topic;
	if (lock_take (tp->lock))
		return (NULL);

	for (ep = tp->writers; ep && ep != &wp->w_ep; ep = ep->next)
		;
	if (!ep) {
		lock_release (tp->lock);
		return (NULL);
	}
	lock_release (tp->lock);
	return (tp);
}

DDS_Publisher DDS_DataWriter_get_publisher (DDS_DataWriter wp)
{
	Publisher_t	*up;

	ctrc_printd (DCPS_ID, DCPS_DW_G_PUB, &wp, sizeof (wp));

	if (!writer_ptr (wp, 0, NULL))
		return (NULL);

	up = wp->w_publisher;
	return (up);
}

DDS_ReturnCode_t DDS_DataWriter_get_matched_subscription_data (
				DDS_DataWriter                   wp,
				DDS_SubscriptionBuiltinTopicData *data,
				DDS_InstanceHandle_t             handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_SUBS_D, &wp, sizeof (wp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!data || !handle)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!writer_ptr (wp, 1, &ret))
		return (ret);

	ep = entity_ptr (handle);
	if (!ep ||
	     ep->type != ET_READER ||
	     entity_ignored (ep->flags)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (entity_discovered (ep->flags))
		ret = dcps_get_builtin_subscription_data (data, (DiscoveredReader_t *) ep);
	else
		ret = dcps_get_local_subscription_data (data, (Reader_t *) ep);

    done:
	lock_release (wp->w_lock);
	return (ret);
}

static int same_participant (Endpoint_t *ep, Participant_t *pp)
{
	if ((ep->entity.flags & EF_LOCAL) != 0)
		return (&ep->u.subscriber->domain->participant == pp);
	else
		return (ep->u.participant == pp);
}

static int check_matched_subscription (Writer_t              *wp,
				       Endpoint_t            *ep, 
				       DDS_InstanceHandleSeq *handles,
				       Participant_t         *pp)
{
	int			match;
	DDS_InstanceHandle_t	h;
	int			error;

	if (pp && !same_participant (ep, pp))
		match = 0;
	else
#ifdef RTPS_USED
	    if ((ep->entity.flags & EF_LOCAL) != 0)
#endif
		match = hc_matches (wp->w_cache, ((Reader_t *) ep)->r_cache);
#ifdef RTPS_USED
	else if (rtps_used)
		match = rtps_writer_matches (wp, (DiscoveredReader_t *) ep);
	else
		match = 0;
#endif
	if (match) {
		h = ep->entity.handle;
		error = dds_seq_append (handles, &h);
	}
	else
		error = DDS_RETCODE_OK;
	return (error);
}

DDS_ReturnCode_t DDS_DataWriter_get_matched_subscriptions(
					DDS_DataWriter        wp,
					DDS_InstanceHandleSeq *handles)
{
	Topic_t		*tp;
	Endpoint_t	*ep;
	FilteredTopic_t	*ftp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_MATCH_S, &wp, sizeof (wp));
	ctrc_contd (&handles, sizeof (handles));
	ctrc_endd ();

	if (!handles)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_SEQ_INIT (*handles);
	if (!writer_ptr (wp, 0, &ret))
		return (ret);

	tp = wp->w_topic;
	if (lock_take (tp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

#ifndef RW_TOPIC_LOCK
	if (lock_take (wp->w_lock)) {
		lock_release (tp->lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
#endif
	for (ep = tp->writers; ep && ep != &wp->w_ep; ep = ep->next)
		;
	if (!ep) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	for (ep = tp->readers; ep; ep = ep->next)
		if (check_matched_subscription (wp, ep, handles, NULL)) {
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto done;
		}

	for (ftp = tp->filters; ftp; ftp = ftp->next)
		for (ep = ftp->topic.readers; ep; ep = ep->next)
			if (check_matched_subscription (wp, ep, handles, NULL)) {
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto done;
			}

    done:
#ifndef RW_TOPIC_LOCK
	lock_release (wp->w_lock);
#endif
	lock_release (tp->lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataWriter_get_reply_subscriptions(
				DDS_DataWriter        wp,
				DDS_InstanceHandle_t  publication_handle,
				DDS_InstanceHandleSeq *handles)
{
	Endpoint_t	*rwp;
	Participant_t	*pp;
	Entity_t	*p;
	Topic_t		*tp;
	Endpoint_t	*ep;
	FilteredTopic_t	*ftp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DW_G_REPLY_S, &wp, sizeof (wp));
	ctrc_contd (&handles, sizeof (handles));
	ctrc_endd ();

	p = entity_ptr (publication_handle);
	if (!p || p->type != ET_WRITER || !handles)
		return (DDS_RETCODE_BAD_PARAMETER);

	rwp = (Endpoint_t *) p;
	if ((rwp->entity.flags & EF_LOCAL) != 0)
		pp = &rwp->u.publisher->domain->participant;
	else
		pp = rwp->u.participant;

	DDS_SEQ_INIT (*handles);
	if (!writer_ptr (wp, 0, &ret))
		return (ret);

	tp = wp->w_topic;
	if (lock_take (tp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

#ifndef RW_TOPIC_LOCK
	if (lock_take (wp->w_lock)) {
		lock_release (tp->lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
#endif
	for (ep = tp->writers; ep && ep != &wp->w_ep; ep = ep->next)
		;
	if (!ep) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	for (ep = tp->readers; ep; ep = ep->next)
		if (check_matched_subscription (wp, ep, handles, pp)) {
			ret = DDS_RETCODE_OUT_OF_RESOURCES;
			goto done;
		}

	for (ftp = tp->filters; ftp; ftp = ftp->next)
		for (ep = ftp->topic.readers; ep; ep = ep->next)
			if (check_matched_subscription (wp, ep, handles, pp)) {
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto done;
			}

    done:
#ifndef RW_TOPIC_LOCK
	lock_release (wp->w_lock);
#endif
	lock_release (tp->lock);
	return (ret);
}


