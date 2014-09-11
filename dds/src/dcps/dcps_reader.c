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

/* dcps_reader.c -- DCPS API - DataReader functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#include <unistd.h>
#endif
#include "sys.h"
#include "ctrace.h"
#include "prof.h"
#include "log.h"
#include "pool.h"
#include "str.h"
#include "error.h"
#if defined (NUTTX_RTOS)
#include "dds/dds_plugin.h"
#else
#include "dds/dds_security.h"
#endif
#include "dds/dds_dcps.h"
#include "dds_data.h"
#include "domain.h"
#include "disc.h"
#include "pid.h"
#ifdef XTYPES_USED
#include "xcdr.h"
#include "xdata.h"
#else
#include "cdr.h"
#endif
#include "pl_cdr.h"
#include "dds.h"
#include "parse.h"
#include "dcps.h"
#include "dcps_priv.h"
#include "dcps_event.h"
#include "dcps_builtin.h"
#include "dcps_topic.h"
#include "dcps_reader.h"

void DDS_DataReaderSeq__init (DDS_DataReaderSeq *readers)
{
	DDS_SEQ_INIT (*readers);
}

void DDS_DataReaderSeq__clear (DDS_DataReaderSeq *readers)
{
	dds_seq_cleanup (readers);
}

DDS_DataReaderSeq *DDS_DataReaderSeq__alloc (void)
{
	DDS_DataReaderSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_DataReaderSeq));
	if (!p)
		return (NULL);

	DDS_DataReaderSeq__init (p);
	return (p);
}

void DDS_DataReaderSeq__free (DDS_DataReaderSeq *readers)
{
	if (!readers)
		return;

	DDS_DataReaderSeq__clear (readers);
	mm_fcts.free_ (readers);
}

DDS_ReturnCode_t DDS_DataReader_get_qos (DDS_DataReader rp,
					 DDS_DataReaderQos *qos)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_QOS, &rp, sizeof (rp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!qos)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	qos_reader_get (rp->r_qos, qos);
	qos->time_based_filter = rp->r_time_based_filter;
	qos->reader_data_lifecycle = rp->r_data_lifecycle;
	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DataReader_set_qos (DDS_DataReader rp,
					 DDS_DataReaderQos *qos)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_S_QOS, &rp, sizeof (rp));
	ctrc_contd (&qos, sizeof (qos));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	if (qos == DDS_DATAREADER_QOS_DEFAULT)
		qos = &rp->r_subscriber->def_reader_qos;
	else if (!qos_valid_reader_qos (qos)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	ret = qos_reader_update (&rp->r_qos, qos);
	if (!ret) {
		rp->r_time_based_filter = qos->time_based_filter;
		rp->r_data_lifecycle = qos->reader_data_lifecycle;
	}

    done:
	lock_release (rp->r_lock);

	if (ret) 
		return (ret);

	lock_take (rp->r_topic->domain->lock);
	lock_take (rp->r_topic->lock);
#ifdef RW_LOCKS
	lock_take (rp->r_lock);
#endif
	if ((rp->r_flags & EF_ENABLED) != 0) {
		hc_qos_update (rp->r_cache);
		disc_reader_update (rp->r_topic->domain, rp, 1, 0);
	}
#ifdef RW_LOCKS
	lock_release (rp->r_lock);
#endif
	lock_release (rp->r_topic->lock); 
	lock_release (rp->r_topic->domain->lock);

	return (ret);
}

DDS_DataReaderListener *DDS_DataReader_get_listener (DDS_DataReader rp)
{
	ctrc_printd (DCPS_ID, DCPS_DR_G_LIS, &rp, sizeof (rp));

	if (!reader_ptr (rp, 0, NULL))
		return (NULL);

	return (&rp->r_listener);
}

DDS_ReturnCode_t DDS_DataReader_set_listener (DDS_DataReader               rp,
					      const DDS_DataReaderListener *listener,
					      DDS_StatusMask               mask)
{
	DDS_ReturnCode_t ret;
	int old_avail, new_avail;

	ctrc_begind (DCPS_ID, DCPS_DR_S_LIS, &rp, sizeof (rp));
	ctrc_contd (&listener, sizeof (listener));
	ctrc_contd (&mask, sizeof (mask));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	old_avail = (rp->r_mask & DDS_DATA_AVAILABLE_STATUS) == 0 || 
	             rp->r_listener.on_data_available == NULL;
	new_avail = (mask & DDS_DATA_AVAILABLE_STATUS) != 0 &&
		    listener &&
		    listener->on_data_available &&
		    hc_avail (rp->r_cache, SKM_READ);

	if (listener) 
		rp->r_listener.cookie = listener->cookie;

	dcps_update_listener ((Entity_t *) rp, &rp->r_lock,
				&rp->r_mask, &rp->r_listener,
				mask, listener);

	if (old_avail && new_avail) {
		rp->r_status &= ~DDS_DATA_AVAILABLE_STATUS;
		rp->r_subscriber->status &= ~DDS_DATA_AVAILABLE_STATUS;
		dcps_data_available ((uintptr_t) rp, rp->r_cache);
	}
	lock_release (rp->r_lock);

	return (DDS_RETCODE_OK);
}

DDS_StatusMask DDS_DataReader_get_status_changes (DDS_DataReader rp)
{
	DDS_StatusMask	m;
	
	ctrc_printd (DCPS_ID, DCPS_DR_G_STAT, &rp, sizeof (rp));

	if (!reader_ptr (rp, 1, NULL))
		return (0);

	m = rp->r_status;
	lock_release (rp->r_lock);
	return (m);
}

DDS_ReturnCode_t DDS_DataReader_enable (DDS_DataReader rp)
{
	Topic_t			*tp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_DR_ENABLE, &rp, sizeof (rp));

	if (!reader_ptr (rp, 0, &ret))
		return (ret);

	tp = rp->r_topic;
	lock_take (tp->domain->lock);
	lock_take (tp->lock);
	if ((tp->entity.flags & EF_ENABLED) == 0 ||
	    (rp->r_subscriber->entity.flags & EF_ENABLED) == 0) {
		lock_release (tp->domain->lock);
		lock_release (tp->lock);
		return (DDS_RETCODE_NOT_ENABLED);
	}
#ifdef RW_LOCKS
	lock_take (rp->r_lock);
#endif
	if ((rp->r_flags & EF_ENABLED) == 0) {
		rp->r_flags |= EF_ENABLED | EF_NOT_IGNORED;
		hc_enable (rp->r_cache);

		/* Deliver new subscription endpoint to the Discovery subsystem. */
		if ((rp->r_flags & EF_BUILTIN) == 0)
			disc_reader_add (rp->r_subscriber->domain, rp);
	}
#ifdef RW_LOCKS
	lock_release (rp->r_lock);
#endif
	lock_release (tp->lock);
	lock_release (tp->domain->lock);
	return (DDS_RETCODE_OK);
}

DDS_StatusCondition DDS_DataReader_get_statuscondition (DDS_DataReader rp)
{
	Condition_t		*cp;
	StatusCondition_t	*scp;

	ctrc_printd (DCPS_ID, DCPS_DR_G_SCOND, &rp, sizeof (rp));

	if (!reader_ptr (rp, 1, NULL))
		return (NULL);

	cp = rp->r_conditions;
	while (cp && cp->class != CC_STATUS)
		cp = cp->e_next;
	if (!cp) {
		scp = dcps_new_status_condition ();
		if (!scp)
			return (NULL);

		scp->entity = (Entity_t *) rp;
		scp->c.e_next = rp->r_conditions;
		rp->r_conditions = scp;
	}
	else
		scp = (StatusCondition_t *) cp;
	lock_release (rp->r_lock);
	return ((DDS_StatusCondition) scp);
}

DDS_InstanceHandle_t DDS_DataReader_get_instance_handle (DDS_DataReader rp)
{
	DDS_InstanceHandle_t	h;

	ctrc_printd (DCPS_ID, DCPS_DR_G_HANDLE, &rp, sizeof (rp));

	if (!reader_ptr (rp, 1, NULL))
		return (0);

	h = rp->r_handle;
	lock_release (rp->r_lock);
	return (h);
}

static void *dcps_get_cdata_cdr (void                *bufp,
				 Change_t            *cp,
				 const TypeSupport_t *ts,
				 int                 dynamic,
				 DDS_ReturnCode_t    *error,
				 void                **auxp)
{
	DB		*sdbp, *ddbp;
	unsigned char	*sdata, *dp;
	unsigned	alloc_size;
#ifdef XTYPES_USED
	unsigned	type;
	const Type	*tp;
	int		copy_data;
	DynData_t	*ddp;
	DynDataRef_t	*drp;
#else
	CDR_TypeSupport	*tp;
#endif
     	int		swap;

	if (cp->c_db &&
	    cp->c_db->size &&
	    (unsigned long) cp->c_db->size - (cp->c_data - cp->c_db->data) < cp->c_length) {

		/* Non-linear data, use a temporary data buffer to linearize. */
		sdbp = db_alloc_data (cp->c_length, 1);
		if (!sdbp) {
			warn_printf ("dds_get_cdata: not enough memory for linearizing data!\r\n");
			*error = DCPS_ERR_NOMEM;
			return (NULL);
		}
		db_get_data (sdbp->data, cp->c_db, cp->c_data, 0, cp->c_length);
		sdata = sdbp->data + 4;
	}
	else {
		sdbp = NULL;
		sdata = cp->c_data + 4;
	}
	swap = (cp->c_data [1] & 1) ^ ENDIAN_CPU;
#ifdef XTYPES_USED
	type = (cp->c_data [0] << 8) | cp->c_data [1];
	if ((type >> 1) == MODE_CDR)
		tp = ts->ts_cdr;
	else
		tp = ts->ts_pl->xtype;
	if (dynamic) {
		copy_data = (sdbp != NULL && cp->c_db->nrefs != 1);

		/* Convert to dynamic data. */
		ddp = cdr_dynamic_data (sdata, 4, tp, 0, copy_data, swap);
#if defined (DDS_DEBUG) && defined (DUMP_DDATA)
		dbg_printf ("dcps_get_cdata_cdr:\r\n");
		xd_dump (1, ddp);
#endif
		if (ddp && (drp = xd_dyn_dref_alloc ()) != NULL) {
			drp->magic = DD_MAGIC;
			drp->nrefs = 1;
			drp->ddata = ddp;
			bufp = drp;
		}
		else
			bufp = NULL;
		if (sdbp) {
			if (cp->c_db->nrefs == 1) {

				/* Linearize CDR cache data permanently. */
				db_free_data (cp->c_db);
				cp->c_db = sdbp;
				cp->c_data = sdbp->data;
			}
			else	/* Free temporary linearized cache data. */
				db_free_data (sdbp);
		}
		if (!bufp)
			*error = DCPS_ERR_PARAM;
		*auxp = bufp;
		return (bufp);
	}
#else
	ARG_NOT_USED (dynamic)

	tp = ts->ts_cdr;
#endif
	alloc_size = cdr_unmarshalled_size (sdata, 4, tp, 0, 0, swap, 0, NULL);
	if (!alloc_size) {
		log_printf (DCPS_ID, 0, "dds_get_cdata: cdr_unmarshalled_size failed (writer=%u)!\r\n", cp->c_writer);
		/*log_print_region (RTPS_ID, 0, cp->c_data, cp->c_length, 1, 1);*/
		*error = DCPS_ERR_PARAM;
		goto free_src_data;
	}
	if (bufp && !ts->ts_dynamic) {

		/* Unmarshall directly to user buffer. */
		dp = bufp;
		ddbp = NULL;
		*auxp = NULL;
	}
	else if (!ts->ts_dynamic && cp->c_db && cp->c_db->nrefs == 1) {

		/* Replace CDR to RAW in cache. */
		ddbp = db_alloc_data (ts->ts_length + 4, 1);
		if (!ddbp) {
			warn_printf ("dds_get_cdata: not enough memory for unmarshalled data!\r\n");
			*error = DCPS_ERR_NOMEM;
			goto free_src_data;
		}
		dp = ddbp->data + 4;
		*auxp = NULL;
	}
	else { /* Unmarshall to a linear (temporary) memory block. */
		dp = xmalloc (alloc_size);
		if (!dp) {
			warn_printf ("dds_get_cdata: not enough memory for unmarshalled data!\r\n");
			*error = DCPS_ERR_NOMEM;
			goto free_src_data;
		}
#ifdef DCPS_ZERO_BUFFER
		memset (dp, 0, alloc_size);
#endif
		ddbp = NULL;
		*auxp = dp;
	}
	*error = cdr_unmarshall (dp, sdata, 4, tp, 0, 0, swap, 0);
	if (*error) {
		log_printf (DCPS_ID, 0, "dds_get_cdata: error %d unmarshalling CDR data (writer=%u)!\r\n", *error, cp->c_writer);
		/*log_print_region (RTPS_ID, 0, cp->c_data, cp->c_length, 1, 1);*/
		goto free_dst_data;
	}
	if (sdbp) {
		if (!ddbp && cp->c_db->nrefs == 1) {

			/* Linearize CDR cache data permanently. */
			db_free_data (cp->c_db);
			cp->c_db = sdbp;
			cp->c_data = sdbp->data;
		}
		else	/* Free temporary linearized cache data. */
			db_free_data (sdbp);
	}
	if (ddbp) {

		/* Replace CDR data with RAW data in cache. */
		ddbp->data [0] = ddbp->data [2] = ddbp->data [3] = 0;
		ddbp->data [1] = (MODE_RAW << 1) | ENDIAN_CPU;
		db_free_data (cp->c_db);
		cp->c_db = ddbp;
		cp->c_data = ddbp->data;
		cp->c_length = ts->ts_length + 4;
	}
	if (bufp && dp != bufp) /* Copy data to provided user buffer. */
		memcpy (bufp, dp, ts->ts_length);

	return (dp);

    free_dst_data:
    	if (!bufp || ts->ts_dynamic) {
		if (ddbp)
			db_free_data (ddbp);
		else
			xfree (dp);
	}
    free_src_data:
	if (sdbp)
		db_free_data (cp->c_db);
	return (NULL);
}

static void *dcps_get_cdata_pl_cdr (Change_t            *cp,
				    const TypeSupport_t *ts,
				    DDS_ReturnCode_t    *error)
{
	DBW		walk;
	unsigned char	*dp;
	unsigned	dsize, alloc_size;
	int		swap;

	if (cp->c_db &&
	    cp->c_db->size)
		dsize = cp->c_db->size - (cp->c_data - cp->c_db->data);
	else
		dsize = cp->c_length;
	walk.dbp = cp->c_db;
	walk.data = cp->c_data + 4;
	walk.left = dsize - 4;
	walk.length = cp->c_length - 4;
	swap = (cp->c_data [1] & 1) ^ ENDIAN_CPU;
	alloc_size = pl_unmarshalled_size (&walk, ts->ts_pl, NULL, swap);
	if (!alloc_size) {
		log_printf (DCPS_ID, 0, "dds_get_cdata: pl_unmarshalled_size failed (writer=%u!\r\n", cp->c_writer);
		/*log_print_region (RTPS_ID, 0, cp->c_data, cp->c_length, 1, 1);*/
		*error = DCPS_ERR_NOMEM;
		return (NULL);
	}
	dp = xmalloc (alloc_size);
	if (!dp) {
		warn_printf ("dds_get_cdata: not enough memory for unmarshalled data!\r\n");
		*error = DCPS_ERR_NOMEM;
		return (NULL);
	}
	*error = pl_unmarshall (dp, &walk, ts->ts_pl, swap);
	if (*error) {
		log_printf (DCPS_ID, 0, "dds_get_cdata: error %d unmarshalling PL-CDR data (writer=%u)!\r\n", *error, cp->c_writer);
		/*log_print_region (RTPS_ID, 0, cp->c_data, cp->c_length, 1, 1);*/
		xfree (dp);
		return (NULL);
	}
	return (dp);
}

static void *dcps_get_cdata_raw (void                *bufp,
				 Change_t            *cp,
				 const TypeSupport_t *ts,
				 int                 dynamic,
				 DDS_ReturnCode_t    *error,
				 void                **auxp)
{
	DB		*ddbp;
	unsigned char	*dp;
	unsigned	dsize, sofs;
#ifdef XTYPES_USED
	DB		*l_db, *cdr_db;
	unsigned char	*cdr;
	unsigned	alloc_size;
	DynData_t	*ddp;
	DynDataRef_t	*drp;
#else
	ARG_NOT_USED (ts)
	ARG_NOT_USED (dynamic)
#endif

	if (!cp->c_db || !cp->c_db->size)
		dsize = cp->c_length;
	else
		dsize = cp->c_db->size - 
			((unsigned char *) cp->c_data - cp->c_db->data);
#ifdef XTYPES_USED
	if (dynamic) {

		/* Marshall into CDR-represented. */
		if (dsize < cp->c_length) { 

			/* Need to make the data linear! */
			l_db = db_alloc_data (cp->c_length - 4, 1);
			if (!l_db) {
				warn_printf ("dds_get_cdata: out of memory for linearized sample data.\r\n");
				*error = DDS_RETCODE_OUT_OF_RESOURCES;
				return (NULL);
			}
			dp = l_db->data;
			db_get_data (dp, cp->c_db, cp->c_data, 4, cp->c_length - 4);
		}
		else {
			l_db = NULL;
			dp = cp->c_data + 4;
		}

		/* Allocate a buffer of the correct size. */
		alloc_size = DDS_MarshalledDataSize (cp->c_data + 4, 0,
							     ts, error);
		cdr_db = db_alloc_data (alloc_size, 1);
		if (!cdr_db) {
			if (dsize < cp->c_length)
				db_free_data (l_db);

			warn_printf ("dds_get_cdata: out of memory for CDR-ed sample data.\r\n");
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
		cdr = cdr_db->data;
		*error = DDS_MarshallData (cdr, cp->c_data + 4, 0, ts);
		if (*error) {
			if (dsize < cp->c_length)
				db_free_data (l_db);

			db_free_data (cdr_db);
			return (NULL);
		}
		if (dsize < cp->c_length) /* No more need for linearized buffer. */
			db_free_data (l_db);

		/* Marshalled successfully in CDR -- save marshalled data if possible. */
		if (!cp->c_db || cp->c_db->nrefs == 1) {
			if (cp->c_db)
				db_free_data (cp->c_db);
			else
				xfree (cp->c_data);
			cp->c_db = cdr_db;
			cp->c_data = cdr;
			cp->c_length = alloc_size;
			cdr_db = NULL;
		}

		/* Finally convert to dynamic data. */
		ddp = cdr_dynamic_data (cdr + 4, 4, ts->ts_cdr, 0, 1, 0);
#if defined (DDS_DEBUG) && defined (DUMP_DDATA)
		dbg_printf ("dcps_get_cdata_raw:\r\n");
		xd_dump (1, ddp);
#endif
		if (ddp && (drp = xd_dyn_dref_alloc ()) != NULL) {
			drp->magic = DD_MAGIC;
			drp->nrefs = 1;
			drp->ddata = ddp;
			bufp = drp;
		}
		else
			bufp = NULL;

		/* Get rid of marshalled data if it couldn't be saved. */
		if (cdr_db)
			db_free_data (cdr_db);

		if (!bufp) {
			warn_printf ("dds_get_cdata: out of memory for Dynamic sample data.\r\n");
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
		*auxp = bufp;
		return (bufp);
	}
#endif
	if (dsize >= cp->c_length) {
		if (bufp)
			memcpy (bufp, cp->c_data + 4, cp->c_length - 4);
		else
			bufp = cp->c_data + 4;
		*auxp = NULL;
	}
	else {	/* Fragmented data: make linear by copying -- sigh :-( */
		if (bufp) {
			sofs = 4;
			dp = bufp;
			ddbp = NULL;
			*auxp = NULL;
		}
		else if (cp->c_db && cp->c_db->nrefs == 1) {
			sofs = 0;
			ddbp = db_alloc_data (cp->c_length, 1);
			if (!ddbp) {
				warn_printf ("dds_get_cdata: out of memory for sample data.\r\n");
				*error = DDS_RETCODE_OUT_OF_RESOURCES;
				return (NULL);
			}
			dp = ddbp->data;
			bufp = dp + 4;
			*auxp = NULL;
		}
		else {
			sofs = 4;
			ddbp = NULL;
			bufp = dp = xmalloc (cp->c_length - 4);
			if (!dp) {
				warn_printf ("dds_get_cdata: out of memory for sample data.\r\n");
				*error = DDS_RETCODE_OUT_OF_RESOURCES;
				return (NULL);
			}
			*auxp = dp;
		}
		db_get_data (dp, cp->c_db, cp->c_data, sofs, cp->c_length - sofs);
		if (ddbp) {

			/* Scattered data no longer needed! Replace
			   cache entry with linear buffer. */
			db_free_data (cp->c_db);
			cp->c_db = ddbp;
			cp->c_data = ddbp->data;
		}
	}
	return (bufp);
}

/* dcps_get_cdata -- Get cache data in a user buffer. */

void *dcps_get_cdata (void                *bufp,
		      Change_t            *cp,
		      const TypeSupport_t *ts,
		      int                 dynamic,
		      DDS_ReturnCode_t    *error,
		      void                **auxp)
{
	unsigned	type;

	type = (cp->c_data [0] << 8) | cp->c_data [1];
	if ((type >> 1) == MODE_CDR ||
	    ((type >> 1) == MODE_PL_CDR && !ts->ts_pl->builtin))
		bufp = dcps_get_cdata_cdr (bufp, cp, ts, dynamic, error, auxp);
	else if ((type >> 1) == MODE_PL_CDR) {
		if (bufp) {
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
		bufp = *auxp = dcps_get_cdata_pl_cdr (cp, ts, error);
	}
	else if ((type >> 1) == MODE_RAW)
		bufp = dcps_get_cdata_raw (bufp, cp, ts, dynamic, error, auxp);
	else {
#ifdef RTPS_USED
		rtps_rx_error (R_INV_MARSHALL, cp->c_data, cp->c_length);
#endif
		*error = DDS_RETCODE_UNSUPPORTED;
		return (NULL);
	}
	return (bufp);
}

void DDS_DataSeq__init (DDS_DataSeq *data)
{
	DDS_SEQ_INIT (*data);
}

void DDS_DataSeq__clear (DDS_DataSeq *data)
{
	dds_seq_cleanup (data);
}

DDS_DataSeq *DDS_DataSeq__alloc (void)
{
	DDS_DataSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_DataSeq));
	if (!p)
		return (NULL);

	DDS_DataSeq__init (p);
	return (p);
}

void DDS_DataSeq__free (DDS_DataSeq *data)
{
	if (!data)
		return;

	DDS_DataSeq__clear (data);
	mm_fcts.free_ (data);
}

void DDS_SampleInfoSeq__init (DDS_SampleInfoSeq *samples)
{
	DDS_SEQ_INIT (*samples);
}

void DDS_SampleInfoSeq__clear (DDS_SampleInfoSeq *samples)
{
	dds_seq_cleanup (samples);
}

DDS_SampleInfoSeq *DDS_SampleInfoSeq__alloc (void)
{
	DDS_SampleInfoSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_SampleInfoSeq));
	if (!p)
		return (NULL);

	DDS_SampleInfoSeq__init (p);
	return (p);
}

void DDS_SampleInfoSeq__free (DDS_SampleInfoSeq *samples)
{
	if (!samples)
		return;

	DDS_SampleInfoSeq__clear (samples);
	mm_fcts.free_ (samples);
}

static void reader_get_no_resources (Reader_t *rp, const char *subject)
{
#ifdef DDS_WARN_OOM
	warn_printf ("DDS_DataReader_get_data(%s/%s): out of resources due to %s!",
			str_ptr (rp->r_topic->name),
			str_ptr (rp->r_topic->type->type_name),
			subject);
#else
	log_printf (DCPS_ID, 0, "DDS_DataReader_get_data(%s/%s): out of resources due to %s!",
			str_ptr (rp->r_topic->name),
			str_ptr (rp->r_topic->type->type_name),
			subject);
#endif
}

DDS_ReturnCode_t dcps_reader_get (Reader_t              *rp,
				  DDS_VoidPtrSeq        *received_data,
				  int                   dynamic,
				  DDS_SampleInfoSeq     *info_seq,
				  unsigned              max_samples,
				  DDS_SampleStateMask   sample_states,
				  DDS_ViewStateMask     view_states,
				  DDS_InstanceStateMask instance_states,
				  QueryCondition_t      *qcp,
				  DDS_InstanceHandle_t  handle,
				  int                   next,
				  int                   take)
{
	Change_t		*cp, *max_cp;
	ChangeInfo_t		*cip;
	DDS_SampleInfo		*ip;
	unsigned		nsamples, i, j, skip, prev_samples;
	Strings_t		*pp;
	BCProgram		*fp, *op;
	void			*cache, *data;
	int			error;
	DDS_ReturnCode_t	ret;

	prof_start (dcps_read_take1);

	if (!received_data->_own &&  (info_seq->_length || received_data->_length))
		warn_printf("Non empty, non owned sequences passed to read/take!");

	/* Check if reception sequences are valid (from DDS spec v1.2 p.82): */
	/* 1. len, max_len and owns must be identical for both collections */
	if (received_data->_own != info_seq->_own ||
	    received_data->_length != info_seq->_length ||
	    received_data->_maximum != info_seq->_maximum ||

	    /* 4. If max_len==0 and owns is false => PRECONDITION_NOT_MET */
	    (received_data->_maximum &&
	     (!received_data->_own ||

	      /* 5. If maximum > 0, owns is true, max_samples !=
		 LENGTH_UNLIMITED and max_samples > max_len =>
		 PRECONDITION_NOT_MET */
	      (received_data->_own &&
	       max_samples != (unsigned) DDS_LENGTH_UNLIMITED &&
	       max_samples > received_data->_maximum))))
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/* For builtins, copying is not allowed since the structures are much
	   too complex, and we can't trust that the application is able to do
	   proper cleanup. */
	if (((rp->r_flags & EF_BUILTIN) != 0 || dynamic) &&
	    received_data->_maximum)
		return (DDS_RETCODE_PRECONDITION_NOT_MET);

	/* Setup parameters for cache data retrieval. */
	nsamples = max_samples;
	prev_samples = rp->r_changes.length;
	if (qcp) {
		fp = &qcp->filter;
		pp = qcp->expression_pars;
		cache = (void *) &qcp->cache;
		op = &qcp->order;
	}
	else {
		fp = NULL;
		pp = NULL;
		cache = NULL;
		op = NULL;
	}
	skip = dcps_skip_mask (sample_states, view_states, instance_states);

	/* Get cache data. */
	error = hc_get (rp->r_cache, &nsamples, &rp->r_changes,
				skip, fp, pp, cache, op, handle, next, take);
	if (error) {
		reader_get_no_resources (rp, "hc_get ()");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	received_data->_length = info_seq->_length = nsamples;
	if (!nsamples) {
		prof_stop (dcps_read_take1, 1);
		return (DDS_RETCODE_NO_DATA);
	}
	prof_stop (dcps_read_take1, 1);
	prof_start (dcps_read_take2);

	/* We got as many samples as returned in nsamples. */
	if (!received_data->_maximum) {
		ret = DDS_RETCODE_OK;
		if (rp->r_n_prev) {
			i = rp->r_n_prev;
			if (rp->r_n_prev >= nsamples) {
				info_seq->_buffer = rp->r_prev_info;
				received_data->_buffer = rp->r_prev_data;
				received_data->_maximum = info_seq->_maximum = rp->r_n_prev;
			}
			else {
				info_seq->_buffer = xrealloc (rp->r_prev_info,
					sizeof (DDS_SampleInfo *) * nsamples);
				received_data->_buffer = xrealloc (rp->r_prev_data,
					sizeof (void *) * nsamples);
				if (!info_seq->_buffer || !received_data->_buffer) {
					if (info_seq->_buffer) {
						xfree (info_seq->_buffer);
						info_seq->_buffer = NULL;
					}
					if (received_data->_buffer) {
						xfree (received_data->_buffer);
						received_data->_buffer = NULL;
					}
					reader_get_no_resources (rp, "xrealloc (_buffer)");
					ret = DDS_RETCODE_OUT_OF_RESOURCES;
				}
				else
					received_data->_maximum = info_seq->_maximum = nsamples;
			}
			rp->r_prev_info = NULL;
			rp->r_prev_data = NULL;
			rp->r_n_prev = 0;
			if (ret)
				goto done;
		}
		else {
			/* We have to create buffers for sample data and info. */
			i = 0;
			received_data->_buffer = xmalloc (sizeof (void *) * nsamples);
			if (!received_data->_buffer) {
				reader_get_no_resources (rp, "xmalloc (received_data._buffer)");
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto done;
			}
			info_seq->_buffer = xmalloc (sizeof (DDS_SampleInfo *) * nsamples);
			if (!info_seq->_buffer) {
				reader_get_no_resources (rp, "xmalloc (info_seq._buffer)");
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto done;
			}
			received_data->_maximum = info_seq->_maximum = nsamples;
		}
		for (; i < nsamples; i++) {
			info_seq->_buffer [i] = ip = 
				mds_pool_alloc (&dcps_mem_blocks [MB_SAMPLE_INFO]);
			if (!ip) {
				reader_get_no_resources (rp, "xmalloc (sample_info)");
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
			}
		}
		received_data->_own = info_seq->_own = 0;
		if (ret)
			goto done;
	}
	else
		received_data->_own = info_seq->_own = 1;
	prof_stop (dcps_read_take2, nsamples);
	prof_start (dcps_read_take3);

	/* Fill in the received data and info pointers. */
	for (i = 0, cip = &rp->r_changes.buffer [i + prev_samples];
	     i < nsamples;
	     i++, cip++) {
		cp = cip->change;
		ip = info_seq->_buffer [i];
		ip->sample_state = cp->c_sstate + 1;
		ip->view_state = cp->c_vstate + 1;
		ftime2time (&cp->c_time, (Time_t *) &ip->source_timestamp);
		ip->instance_handle = cp->c_handle;
		ip->publication_handle = (DDS_InstanceHandle_t) cp->c_writer;
		ip->disposed_generation_count = cp->c_disp_cnt;
		ip->no_writers_generation_count = cp->c_no_w_cnt;
		ip->sample_rank = 0;
		max_cp = cp;
		for (j = i + 1; j < nsamples; j++)
			if (rp->r_changes.buffer [j + prev_samples].change->c_handle == cp->c_handle) {
				max_cp = rp->r_changes.buffer [j + prev_samples].change;
				ip->sample_rank++;
			}
		ip->generation_rank = (max_cp->c_disp_cnt + max_cp->c_no_w_cnt) -
				      (cp->c_disp_cnt + cp->c_no_w_cnt);
		ip->absolute_generation_rank = cp->c_abs_cnt;
		switch (cp->c_istate) {
			case ALIVE:
				ip->instance_state = DDS_ALIVE_INSTANCE_STATE;
				break;
			case NOT_ALIVE_DISPOSED:
				ip->instance_state = DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
				break;
			case NOT_ALIVE_UNREGISTERED:
			case ZOMBIE:
				ip->instance_state = DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
				break;
		}
		ip->valid_data = (cp->c_length != 0);
                cip->sample_info = ip;
		if (!ip->valid_data) {
			cip->user = cip->bufp = NULL;
			continue;
		}
		if ((rp->r_flags & EF_BUILTIN) != 0) {
			received_data->_buffer [i] = dcps_read_builtin_data (rp, cp);
			if (!received_data->_buffer [i])
				goto free_samples;

			cip->bufp = received_data->_buffer [i];
		}
		else {
			cip->bufp = NULL;
			data = dcps_get_cdata ((received_data->_own) ?
					        received_data->_buffer [i] :
					        NULL,
					       cp,
					       rp->r_topic->type->type_support,
					       dynamic,
					       &ret,
					       &cip->bufp);
			if (!data)
				goto free_samples;

			if (!received_data->_own)
				received_data->_buffer [i] = data;
		}
		cip->user = received_data->_buffer [i];
	}
	if (received_data->_own) {
		hc_done (rp->r_cache, nsamples, rp->r_changes.buffer + prev_samples);
		rp->r_changes.length = prev_samples;
	}
	prof_stop (dcps_read_take3, nsamples);
	return (DDS_RETCODE_OK);

    free_samples:
	reader_get_no_resources (rp, "unmarshall_data");
    	ret = DDS_RETCODE_OUT_OF_RESOURCES;
    	for (j = 0, cip = &rp->r_changes.buffer [i + prev_samples];
	     j < i;
	     j++, cip++)
		if (cip->bufp)
			xfree (cip->bufp);

    done:
	if (!received_data->_own) {
		if (received_data->_maximum) {
			xfree (received_data->_buffer);
			received_data->_buffer = NULL;
		}
		if (info_seq->_maximum) {
			for (i = 0; i < info_seq->_length; i++)
				if ((ip = info_seq->_buffer [i]) != NULL)
					mds_pool_free (&dcps_mem_blocks [MB_SAMPLE_INFO], ip);
			xfree (info_seq->_buffer);
			info_seq->_buffer = NULL;
		}
		received_data->_maximum = info_seq->_maximum = 0;
	}
	received_data->_length = info_seq->_length = 0;
	hc_done (rp->r_cache, nsamples, rp->r_changes.buffer + prev_samples);
	rp->r_changes.length = prev_samples;
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_read (DDS_DataReader        rp,
				      DDS_DataSeq           *received_data,
				      DDS_SampleInfoSeq     *info_seq,
				      unsigned              max_samples,
				      DDS_SampleStateMask   sample_states,
				      DDS_ViewStateMask     view_states,
				      DDS_InstanceStateMask instance_states)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_READ, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, sample_states,
			       view_states, instance_states,
			       NULL, 0, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_take (DDS_DataReader        rp,
				      DDS_DataSeq           *received_data,
				      DDS_SampleInfoSeq     *info_seq,
				      unsigned              max_samples,
				      DDS_SampleStateMask   sample_states,
				      DDS_ViewStateMask     view_states,
				      DDS_InstanceStateMask instance_states)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_TAKE, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, sample_states,
			       view_states, instance_states,
			       NULL, 0, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_read_w_condition (DDS_DataReader    rp,
						  DDS_DataSeq       *received_data,
						  DDS_SampleInfoSeq *info_seq,
						  unsigned          max_samples,
						  DDS_Condition     condition)
{
	ReadCondition_t		*rcp = (ReadCondition_t *) condition;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_READC, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&condition, sizeof (condition));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != rp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, rcp->sample_states,
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp : 
					NULL, 0, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_take_w_condition (DDS_DataReader    rp,
						  DDS_DataSeq       *received_data,
						  DDS_SampleInfoSeq *info_seq,
						  unsigned          max_samples,
						  DDS_Condition     condition)
{
	ReadCondition_t		*rcp = (ReadCondition_t *) condition;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_TAKEC, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&condition, sizeof (condition));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != rp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, rcp->sample_states,
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp : NULL, 0, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_read_next_sample (DDS_DataReader rp,
						  void *data_value,
						  DDS_SampleInfo *sample_info)
{
	void			*ptr;
	DDS_SampleInfo		*info;
	DDS_VoidPtrSeq		data;
	DDS_SampleInfoSeq	sinfo;
	DDS_ReturnCode_t 	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_R_NS, &rp, sizeof (rp));
	ctrc_contd (&data_value, sizeof (data_value));
	ctrc_contd (&sample_info, sizeof (sample_info));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	if (rp->r_topic->type->type_support->ts_dynamic) {
		lock_release (rp->r_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_SEQ_INIT_PTR (data, ptr);
	ptr = data_value;
	DDS_SEQ_INIT_PTR (sinfo, info);
	info = sample_info;
	ret = dcps_reader_get (rp, &data, 0, &sinfo, 1,
			       DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			       DDS_ANY_INSTANCE_STATE, NULL, 0, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_take_next_sample (DDS_DataReader rp,
						  void *data_value,
						  DDS_SampleInfo *sample_info)
{
	void			*ptr;
	DDS_SampleInfo		*info;
	DDS_VoidPtrSeq		data;
	DDS_SampleInfoSeq	sinfo;
	DDS_ReturnCode_t 	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_T_NS, &rp, sizeof (rp));
	ctrc_contd (&data_value, sizeof (data_value));
	ctrc_contd (&sample_info, sizeof (sample_info));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	if (rp->r_topic->type->type_support->ts_dynamic) {
		lock_release (rp->r_lock);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_SEQ_INIT_PTR (data, ptr);
	ptr = data_value;
	DDS_SEQ_INIT_PTR (sinfo, info);
	info = sample_info;
	ret = dcps_reader_get (rp, &data, 0, &sinfo, 1,
			       DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			       DDS_ANY_INSTANCE_STATE, NULL, 0, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_read_instance (
			DDS_DataReader        rp,
			DDS_DataSeq           *received_data,
			DDS_SampleInfoSeq     *info_seq,
			unsigned              max_samples,
			DDS_InstanceHandle_t  handle,
			DDS_SampleStateMask   sample_states,
			DDS_ViewStateMask     view_states,
			DDS_InstanceStateMask instance_states)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_R_INST, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, sample_states,
			       view_states, instance_states,
			       NULL, handle, 0, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_take_instance (
			DDS_DataReader        rp,
			DDS_DataSeq           *received_data,
			DDS_SampleInfoSeq     *info_seq,
			unsigned              max_samples,
			DDS_InstanceHandle_t  handle,
			DDS_SampleStateMask   sample_states,
			DDS_ViewStateMask     view_states,
			DDS_InstanceStateMask instance_states)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_T_INST, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, sample_states,
			       view_states, instance_states,
			       NULL, handle, 0, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_read_next_instance (
			DDS_DataReader        rp,
			DDS_DataSeq           *received_data,
			DDS_SampleInfoSeq     *info_seq,
			unsigned              max_samples,
			DDS_InstanceHandle_t  handle,
			DDS_SampleStateMask   sample_states,
			DDS_ViewStateMask     view_states,
			DDS_InstanceStateMask instance_states)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_R_NINST, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, sample_states,
			       view_states, instance_states,
			       NULL, handle, 1, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_take_next_instance (
			DDS_DataReader        rp,
			DDS_DataSeq           *received_data,
			DDS_SampleInfoSeq     *info_seq,
			unsigned              max_samples,
			DDS_InstanceHandle_t  handle,
			DDS_SampleStateMask   sample_states,
			DDS_ViewStateMask     view_states,
			DDS_InstanceStateMask instance_states)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_T_NINST, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, sample_states,
			       view_states, instance_states,
			       NULL, handle, 1, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_read_next_instance_w_condition (
			DDS_DataReader       rp,
			DDS_DataSeq          *received_data,
			DDS_SampleInfoSeq    *info_seq,
			unsigned             max_samples,
			DDS_InstanceHandle_t handle,
			DDS_Condition        condition)
{
	ReadCondition_t		*rcp = (ReadCondition_t *) condition;
	DDS_ReturnCode_t 	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_R_NINSTC, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&condition, sizeof (condition));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != rp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, rcp->sample_states,
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp : 
					NULL, handle, 1, 0);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_take_next_instance_w_condition (
			DDS_DataReader       rp,
			DDS_DataSeq          *received_data,
			DDS_SampleInfoSeq    *info_seq,
			unsigned             max_samples,
			DDS_InstanceHandle_t handle,
			DDS_Condition        condition)
{
	ReadCondition_t		*rcp = (ReadCondition_t *) condition;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_T_NINSTC, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_contd (&max_samples, sizeof (max_samples));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_contd (&condition, sizeof (condition));
	ctrc_endd ();

	if (!rcp ||
	    (rcp->c.class != CC_READ && rcp->c.class != CC_QUERY) ||
	    rcp->rp != rp)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_reader_get (rp, (DDS_VoidPtrSeq *) received_data, 0,
			       info_seq, max_samples, rcp->sample_states,
			       rcp->view_states, rcp->instance_states,
			       (rcp->c.class == CC_QUERY) ?
					(QueryCondition_t *) rcp :
					NULL, handle, 1, 1);
	lock_release (rp->r_lock);
	return (ret);
}

DDS_ReturnCode_t dcps_return_loan (Reader_t          *rp,
				   DDS_VoidPtrSeq    *received_data,
				   int               dynamic,
				   DDS_SampleInfoSeq *info_seq)
{
	DDS_SampleInfo	*ip;
	ChangeInfo_t	*cip;
	unsigned	i, ofs;
	int		match;
	DDS_ReturnCode_t ret = DDS_RETCODE_OK;

	ctrc_begind (DCPS_ID, DCPS_DR_RLOAN, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_endd ();

	prof_start (dcps_return_loan_p);

#ifndef XTYPES_USED
	ARG_NOT_USED (dynamic)
#endif

	/* Check if return_loan() is needed/allowed. */
	if (info_seq->_length != received_data->_length ||
	    info_seq->_maximum != received_data->_maximum ||
	    info_seq->_own != received_data->_own) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (!info_seq->_length || info_seq->_own)
		goto done;

	if (info_seq->_length == rp->r_changes.length)
	    /*|| (rp->r_flags & EF_BUILTIN) != 0)*/

		/* Only one outstanding loan list. */
		ofs = 0;

	else {
		/* Which set of loans is this?  Check using a sliding window on
	           the changes sequence and verifying each data item in it. */
		match = 0;
		for (ofs = 0;
		     ofs + info_seq->_length <= rp->r_changes.length;
		     ofs++) {
			for (i = 0, cip = &rp->r_changes.buffer [i + ofs];
			     i < info_seq->_length;
			     i++, cip++) {
                                if (info_seq->_buffer [i] != cip->sample_info)
                                        break;

                                /* if (cip->change->c_kind == ALIVE &&
                                                received_data->_buffer [i] != cip->user)
                                        break; */
                        }
			if (i == info_seq->_length) {
				match = 1;
				break;
			}
		}
		if (!match) {
			ret = DDS_RETCODE_BAD_PARAMETER;
			goto done;
		}
	}

	/* Changes found in list, starting at ofs. */

	/* Release all extra allocated data (buffers or DynamicData). */
	for (i = 0, cip = &rp->r_changes.buffer [i + ofs];
	     i < info_seq->_length;
	     i++, cip++)
		if (cip->change->c_kind == ALIVE && cip->bufp) {
			if ((rp->r_flags & EF_BUILTIN) != 0)
				dcps_free_builtin_data (rp, cip->bufp);
#ifdef XTYPES_USED
			else if (dynamic) {
#if defined (DDS_DEBUG) && defined (DUMP_DDATA)
				dbg_printf ("dcps_return_loan:\r\n");
				xd_dump (1, ((DynDataRef_t *) cip->bufp)->ddata);
#endif
				DDS_DynamicDataFactory_delete_data (cip->bufp);
			}
#endif
			else
				xfree (cip->bufp);
			cip->bufp = NULL;
		}

	/* Release all change descriptors. */
	hc_done (rp->r_cache, info_seq->_length, &rp->r_changes.buffer [ofs]);

	/* Update the list of pending changes. */
	if (info_seq->_length == rp->r_changes.length)
		rp->r_changes.length = 0;
	else {
		if (ofs + info_seq->_length < rp->r_changes.length)
			memmove (&rp->r_changes.buffer [ofs],
				 &rp->r_changes.buffer [ofs + info_seq->_length],
				 (rp->r_changes.length - ofs - info_seq->_length) * 
			 				sizeof (ChangeInfo_t));
		rp->r_changes.length -= info_seq->_length;
	}

	/* Save used data (sequence buffers + sample info) for future reuse. */
	if (!rp->r_n_prev || rp->r_n_prev < info_seq->_maximum) {
		if (rp->r_n_prev) {
			for (i = 0; i < rp->r_n_prev; i++) {
				ip = rp->r_prev_info [i];
				mds_pool_free (&dcps_mem_blocks [MB_SAMPLE_INFO], ip);
			}
			xfree (rp->r_prev_info);
			xfree (rp->r_prev_data);
		}
		rp->r_prev_info = info_seq->_buffer;
		info_seq->_buffer = NULL;
		rp->r_prev_data = received_data->_buffer;
		rp->r_n_prev = info_seq->_maximum;
		received_data->_buffer = NULL;
	}
	else {
		for (i = 0; i < info_seq->_maximum; i++) {
			ip = info_seq->_buffer [i];
			mds_pool_free (&dcps_mem_blocks [MB_SAMPLE_INFO], ip);
		}
		xfree (info_seq->_buffer);
		xfree (received_data->_buffer);
	}
	info_seq->_length = received_data->_length = 0;
	info_seq->_maximum = received_data->_maximum = 0;

    done:
	prof_stop (dcps_return_loan_p, 1);
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_return_loan (DDS_DataReader    rp,
					     DDS_DataSeq       *received_data,
					     DDS_SampleInfoSeq *info_seq)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_RLOAN, &rp, sizeof (rp));
	ctrc_contd (&received_data, sizeof (received_data));
	ctrc_contd (&info_seq, sizeof (info_seq));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = dcps_return_loan (rp, (DDS_VoidPtrSeq *) received_data, 0, info_seq);

	lock_release (rp->r_lock);
	return (ret);
}

DDS_TopicDescription DDS_DataReader_get_topicdescription (DDS_DataReader rp)
{
	DDS_TopicDescription	td;

	ctrc_printd (DCPS_ID, DCPS_DR_G_TD, &rp, sizeof (rp));

	if (!reader_ptr (rp, 1, NULL))
		return (NULL);

	td = (DDS_TopicDescription) rp->r_topic;
	lock_release (rp->r_lock);
	return (td);
}

DDS_Subscriber DDS_DataReader_get_subscriber (DDS_DataReader rp)
{
	Subscriber_t	*sp;

	ctrc_printd (DCPS_ID, DCPS_DR_G_SUB, &rp, sizeof (rp));

	if (!reader_ptr (rp, 0, NULL))
		return (NULL);

	sp = rp->r_subscriber;
	return (sp);
}

DDS_ReadCondition DDS_DataReader_create_readcondition (DDS_DataReader rp,
						       DDS_SampleStateMask sample_states,
						       DDS_ViewStateMask view_states,
						       DDS_InstanceStateMask instance_states)
{
	ReadCondition_t	 *cp;

	ctrc_begind (DCPS_ID, DCPS_DR_C_RCOND, &rp, sizeof (rp));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, NULL))
		return (NULL);

	cp = mds_pool_alloc (&dcps_mem_blocks [MB_READ_COND]);
	if (!cp)
		return (NULL);

	cp->c.waitset = NULL;
	cp->c.class = CC_READ;
	cp->c.next = NULL;
	cp->c.e_next = rp->r_conditions;
	cp->c.deferred = 0;
	rp->r_conditions = cp;
	cp->rp = rp;
	cp->sample_states = sample_states;
	cp->view_states = view_states;
	cp->instance_states = instance_states;
	lock_release (rp->r_lock);
	return ((DDS_ReadCondition) cp);
}

static void delete_readcondition (Reader_t *rp, ReadCondition_t *cp, int pool)
{
	ReadCondition_t	 *xcp, *prev_cp;

	for (xcp = rp->r_conditions, prev_cp = NULL;
	     xcp;
	     prev_cp = xcp, xcp = (ReadCondition_t *) xcp->c.e_next)
		if (xcp == cp) {
			if (prev_cp)
				prev_cp->c.e_next = cp->c.e_next;
			else
				rp->r_conditions = cp->c.e_next;
			if (cp->c.waitset) {
				lock_release (rp->r_lock);
				DDS_WaitSet_detach_condition (cp->c.waitset, &cp->c);
				lock_take (rp->r_lock);
			}
			break;
		}
	if (xcp)
		mds_pool_free (&dcps_mem_blocks [pool], cp);
}

DDS_QueryCondition DDS_DataReader_create_querycondition (DDS_DataReader rp,
							 DDS_SampleStateMask sample_states,
							 DDS_ViewStateMask view_states,
							 DDS_InstanceStateMask instance_states,
							 const char *query_expression,
							 DDS_StringSeq *query_parameters)
{
	QueryCondition_t *cp;
	BCProgram	 mprog, oprog;
	int		 error;

	ctrc_begind (DCPS_ID, DCPS_DR_C_QCOND, &rp, sizeof (rp));
	ctrc_contd (&sample_states, sizeof (sample_states));
	ctrc_contd (&view_states, sizeof (view_states));
	ctrc_contd (&instance_states, sizeof (instance_states));
	ctrc_contd (query_expression, strlen (query_expression));
	ctrc_contd (&query_parameters, sizeof (query_parameters));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, NULL))
		return (NULL);

	cp = mds_pool_alloc (&dcps_mem_blocks [MB_QUERY_COND]);
	if (!cp)
		goto no_cond_mem;

	cp->rc.c.waitset = NULL;
	cp->rc.c.class = CC_QUERY;
	cp->rc.c.next = NULL;
	cp->rc.c.deferred = 0;
	cp->rc.sample_states = sample_states;
	cp->rc.view_states = view_states;
	cp->rc.instance_states = instance_states;

	error = sql_parse_query (rp->r_topic->type->type_support,
				 query_expression,
				 &mprog,
				 &oprog);
	if (error)
		goto inv_expr;

	cp->expression = str_new_cstr (query_expression);
	if (!cp->expression)
		goto no_expr_mem;

	if (query_parameters) {
		cp->expression_pars = dcps_new_str_pars (query_parameters, &error);
		if (!cp->expression_pars)
			goto no_pars_mem;
	}
	else
		cp->expression_pars = NULL;

	cp->filter = mprog;
	cp->order = oprog;
	bc_cache_init (&cp->cache);

	cp->rc.c.e_next = rp->r_conditions;
	rp->r_conditions = cp;
	cp->rc.rp = rp;

	lock_release (rp->r_lock);
	return ((DDS_QueryCondition) cp);

    no_pars_mem:
    	str_unref (cp->expression);

    no_expr_mem:
    	if (mprog.buffer)
	    	xfree (mprog.buffer);
	if (oprog.buffer)
		xfree (oprog.buffer);

    inv_expr:
    	mds_pool_free (&dcps_mem_blocks [MB_QUERY_COND], cp);

    no_cond_mem:
	lock_release (rp->r_lock);
	return (NULL);
}

static void delete_querycondition (Reader_t *rp, QueryCondition_t *cp)
{
	dcps_free_str_pars (cp->expression_pars);
    	str_unref (cp->expression);
    	if (cp->filter.buffer)
	    	xfree (cp->filter.buffer);
	if (cp->order.buffer)
		xfree (cp->order.buffer);
	bc_cache_flush (&cp->cache);
	delete_readcondition (rp, (ReadCondition_t *) cp, MB_QUERY_COND);
}

DDS_ReturnCode_t DDS_DataReader_delete_readcondition (DDS_DataReader rp,
						      void *cond)
{
	Condition_t	 *cp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_D_RCOND, &rp, sizeof (rp));
	ctrc_contd (&cond, sizeof (cond));
	ctrc_endd ();

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	for (cp = rp->r_conditions; cp; cp = cp->e_next)
		if (cp == (Condition_t *) cond)
			break;

	if (!cp || 
	    (cp->class != CC_READ && cp->class != CC_QUERY)) {
		ret = DDS_RETCODE_PRECONDITION_NOT_MET;
		goto done;
	}
	if (cp->deferred) {
		cp->deferred = 0;
		dds_defer_waitset_undo (rp, cp);
	}
	if (cp->class == CC_READ)
		delete_readcondition (rp, (ReadCondition_t *) cp, MB_READ_COND);
	else
		delete_querycondition (rp, (QueryCondition_t *) cp);

    done:
    	lock_release (rp->r_lock);
	return (DDS_RETCODE_OK);
}

#ifdef DDS_DEBUG

void dcps_readcondition_dump (ReadCondition_t *rp)
{
	dbg_printf ("sample_states=0x%x\r\n", rp->sample_states);
	dbg_printf ("view_states=0x%x\r\n", rp->view_states);
	dbg_printf ("instance_states=0x%x\r\n", rp->instance_states);
}

void dcps_querycondition_dump (QueryCondition_t *cp)
{
	String_t	**sp;
	unsigned	i;

	dcps_readcondition_dump (&cp->rc);
	dbg_printf ("expression=%s\r\n", str_ptr (cp->expression));
	if (cp->expression_pars) {
		dbg_printf ("parameters=");
		DDS_SEQ_FOREACH_ENTRY (*cp->expression_pars, i, sp) {
			if (i)
				dbg_printf (", ");
			dbg_printf ("%s", str_ptr (*sp));
		}
	}
	dbg_printf ("filter:\r\n");
	bc_dump (1, &cp->filter);
}

#endif

DDS_ReturnCode_t DDS_DataReader_get_key_value (DDS_DataReader       rp,
					       void                 *data,
					       DDS_InstanceHandle_t handle)
{
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_KEY, &rp, sizeof (rp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	prof_start (dcps_r_key);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ret = hc_get_key (rp->r_cache, handle, data, 0);
	lock_release (rp->r_lock);
	prof_stop (dcps_r_key, 1);
	return (ret);
}

HCI handle_get (Topic_t          *tp,
		Cache_t          *cp,
		const void       *data,
		int              dynamic,
		int              secure,
		InstanceHandle   *h,
		DDS_ReturnCode_t *ret)
{
	size_t			size;
	unsigned char		*keys;
	HCI			hci;
	unsigned char		buf [16];

	keys = dcps_key_data_get (tp, data, dynamic, secure, buf, &size, ret);
	if (!keys) {
		hci = NULL;
		*h = DDS_HANDLE_NIL;
	}
	else if ((hci = hc_lookup_key (cp, keys, size, h)) == NULL)
		*h = DDS_HANDLE_NIL;

	if (keys && size > sizeof (buf))
		xfree (keys);

	return (hci);
}

DDS_InstanceHandle_t DDS_DataReader_lookup_instance (DDS_DataReader rp,
						     const void     *key_data)
{
	InstanceHandle		h;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_L_INST, &rp, sizeof (rp));
	ctrc_contd (&key_data, sizeof (key_data));
	ctrc_endd ();

	prof_start (dcps_r_lookup);

	if (!key_data)
		return (DDS_HANDLE_NIL);

	if (!reader_ptr (rp, 1, &ret))
		return (DDS_HANDLE_NIL);

	handle_get (rp->r_topic, rp->r_cache, key_data, 0, 
					ENC_DATA (&rp->r_lep), &h, &ret);
	lock_release (rp->r_lock);
	prof_stop (dcps_r_lookup, 1);
	return ((DDS_InstanceHandle_t) h);
}

static DDS_ReadCondition DDS_DataReader_get_readcondition (DDS_DataReader rp)
{
	Condition_t	*cp;

	if (!reader_ptr (rp, 1, NULL))
		return (NULL);

	for (cp = rp->r_conditions; cp; cp = cp->e_next)
		if (cp->class == CC_READ || cp->class == CC_QUERY)
			break;

	lock_release (rp->r_lock);
	return ((DDS_ReadCondition) cp);
}

DDS_ReturnCode_t DDS_DataReader_delete_contained_entities (DDS_DataReader rp)
{
	DDS_ReadCondition	cp;
	DDS_ReturnCode_t	ret;

	ctrc_printd (DCPS_ID, DCPS_DR_D_CONT, &rp, sizeof (rp));

	while ((cp = DDS_DataReader_get_readcondition (rp)) != NULL) {
		ret = DDS_DataReader_delete_readcondition (rp, cp);
		if (ret)
			return (ret);
	}
	return (DDS_RETCODE_OK);
}

DDS_ReturnCode_t DDS_DataReader_wait_for_historical_data (DDS_DataReader rp,
							  DDS_Duration_t *max_wait)
{
	DDS_ReturnCode_t ret;

	ctrc_printd (DCPS_ID, DCPS_DR_W_HIST, &rp, sizeof (rp));

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	if (rp->r_qos->qos.durability_kind == DDS_VOLATILE_DURABILITY_QOS ||
	    !rp->r_sm_status.current_count ||
	    !rp->r_rtps) {
		lock_release (rp->r_lock);
		return (DDS_RETCODE_OK);
	}
	lock_release (rp->r_lock);

#ifdef RTPS_USED
	if (rtps_used)
		ret = rtps_wait_data (rp, (const Duration_t *) max_wait);
#else
	ARG_NOT_USED (max_wait)
#endif
	return (ret);
}

DDS_ReturnCode_t DDS_DataReader_get_matched_publication_data (DDS_DataReader rp,
						DDS_PublicationBuiltinTopicData *data,
						DDS_InstanceHandle_t handle)
{
	Entity_t		*ep;
	DDS_ReturnCode_t	ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_PUB_D, &rp, sizeof (rp));
	ctrc_contd (&data, sizeof (data));
	ctrc_contd (&handle, sizeof (handle));
	ctrc_endd ();

	if (!data || !handle)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!reader_ptr (rp, 1, &ret))
		return (ret);

	ep = entity_ptr (handle);
	if (!ep ||
	     ep->type != ET_WRITER ||
	     entity_ignored (ep->flags)) {
		ret = DDS_RETCODE_BAD_PARAMETER;
		goto done;
	}
	if (entity_discovered (ep->flags))
		ret = dcps_get_builtin_publication_data (data, (DiscoveredWriter_t *) ep);
	else
		ret = dcps_get_local_publication_data (data, (Writer_t *) ep);

    done:
	lock_release (rp->r_lock);
	return (ret);
}

void DDS_InstanceHandleSeq__init (DDS_InstanceHandleSeq *handles)
{
	DDS_SEQ_INIT (*handles);
}

void DDS_InstanceHandleSeq__clear (DDS_InstanceHandleSeq *handles)
{
	dds_seq_cleanup (handles);
}

DDS_InstanceHandleSeq *DDS_InstanceHandleSeq__alloc (void)
{
	DDS_InstanceHandleSeq	*p;

	p = mm_fcts.alloc_ (sizeof (DDS_InstanceHandleSeq));
	if (!p)
		return (NULL);

	DDS_InstanceHandleSeq__init (p);
	return (p);
}

void DDS_InstanceHandleSeq__free (DDS_InstanceHandleSeq *handles)
{
	if (!handles)
		return;

	DDS_InstanceHandleSeq__clear (handles);
	mm_fcts.free_ (handles);
}

static int check_matched_publication (Endpoint_t            *ep, 
				      Reader_t              *rp,
				      DDS_InstanceHandleSeq *handles)
{
	int			match;
	DDS_InstanceHandle_t	h;
	int			error;

#ifdef RTPS_USED
	if ((ep->entity.flags & EF_LOCAL) != 0)
#endif
		match = hc_matches (((Writer_t *) ep)->w_cache, rp->r_cache);
#ifdef RTPS_USED
	else if (rtps_used)
		match = rtps_reader_matches (rp, (DiscoveredWriter_t *) ep);
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

DDS_ReturnCode_t DDS_DataReader_get_matched_publications (DDS_DataReader rp,
						DDS_InstanceHandleSeq *handles)
{
	Topic_t		*tp;
	Endpoint_t	*ep;
	FilteredTopic_t	*ftp;
	DDS_ReturnCode_t ret;

	ctrc_begind (DCPS_ID, DCPS_DR_G_MATCH_P, &rp, sizeof (rp));
	ctrc_contd (&handles, sizeof (handles));
	ctrc_endd ();

	if (!handles)
		return (DDS_RETCODE_BAD_PARAMETER);

	DDS_SEQ_INIT (*handles);
	if (!reader_ptr (rp, 0, &ret))
		return (ret);

	tp = rp->r_topic;
	if (lock_take (tp->lock))
		return (DDS_RETCODE_ALREADY_DELETED);

#ifndef RW_TOPIC_LOCK
	if (lock_take (rp->r_lock)) {
		lock_release (tp->lock);
		return (DDS_RETCODE_ALREADY_DELETED);
	}
#endif
	for (ep = tp->readers; ep && ep != &rp->r_ep; ep = ep->next)
		;
	if (!ep) {
		ret = DDS_RETCODE_ALREADY_DELETED;
		goto done;
	}
	if ((rp->r_topic->entity.flags & EF_FILTERED) == 0)
		for (ep = tp->writers; ep; ep = ep->next) {
			if (check_matched_publication (ep, rp, handles)) {
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto done;
			}
		}
	else {
		ftp = (FilteredTopic_t *) rp->r_topic;
		for (ep = ftp->related->writers; ep; ep = ep->next)
			if (check_matched_publication (ep, rp, handles)) {
				ret = DDS_RETCODE_OUT_OF_RESOURCES;
				goto done;
			}
	}

    done:
#ifndef RW_TOPIC_LOCK
	lock_release (rp->r_lock);
#endif
	lock_release (tp->lock);
	return (ret);
}



