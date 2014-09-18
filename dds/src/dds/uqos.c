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

/* uqos.c -- Implements common operations on QoS policies of different entities
	     such as Readers, Writers and Topics and attempts to optimize the
	     amount of storage that is required for the QoS parameters. */

#include <stdio.h>
#include "nmatch.h"
#include "hash.h"
#include "log.h"
#include "error.h"
#include "pl_cdr.h"
#include "dds_data.h"
#include "uqos.h"

#define	MAX_HASH	37	/* Just some number. */

typedef struct qos_ref_st QosRef_t;
struct qos_ref_st {
	QosRef_t	*next;
	Qos_t		*data;
};

enum mem_block_en {
	MB_QOS_REF,		/* DCPS QoS reference. */
	MB_QOS_DATA,		/* DCPS QoS data. */

	MB_END
};

static const char *mem_names [] = {
	"QOS_REF",
	"QOS_DATA"
};

static MEM_DESC_ST	mem_blocks [MB_END];	/* Memory blocks. */
static size_t		mem_size;		/* Total allocated memory. */
static QosRef_t		*qos_ht [MAX_HASH];	/* Hash table. */

/* QoS types: */
typedef enum {
	QT_INTEGER,			/* OWNERSHIP_STRENGTH/
					   TRANSPORT_PRIORITY. */
	QT_BOOLEAN,			/* WRITER_DATA_LIFECYCLE. */
	QT_OCTETSEQ,			/* Octet seq: USER/TOPIC/GROUP_DATA. */
	QT_PARTITION,			/* PARTITION. */
	QT_DURATION,			/* DEADLINE/LATENCY_BUDGET/
					   TIME_BASED_FILTER/LIFESPAN. */
	QT_TIMESTAMP_TYPE,		/* DESTINATION_ORDER. */
	QT_SHARED_EXCLUSIVE,		/* OWNERSHIP. */
	QT_DURABILITY,			/* DURABILITY. */
	QT_DURABILITY_SERVICE,		/* DURABILITY_SERVICE */
	QT_PRESENTATION,		/* PRESENTATION. */
	QT_LIVELINESS,			/* LIVELINESS. */
	QT_RELIABILITY,			/* RELIABILITY */
	QT_HISTORY,			/* HISTORY. */
	QT_RESOURCE_LIMITS,		/* RESOURCE_LIMITS. */
	QT_READER_DATA_LIFECYCLE,	/* READER_DATA_LIFECYCLE. */

	QT_MAX
} QTYPE;


typedef int (*QT_VALID_FCT) (const void *src, int disc);

/* Validate the given source parameters. */

typedef void (*QT_TO_UNI_FCT) (void *src, UniQos_t *uq, unsigned ofs, int disc);

/* Set/update data in the Unified QoS structure. */

typedef void (*QT_FROM_UNI_FCT) (const UniQos_t *uq, unsigned ofs, void *dst, int disc);

/* Get data from the unified QoS structure. */

typedef void (*QT_CLEAN_FCT) (UniQos_t *uq, unsigned ofs);

/* Data cleanup function. */

typedef int (*QT_MATCH_FCT) (const UniQos_t *wq, const UniQos_t *rq, unsigned ofs);

/* Check Writer/Reader QoS for compatibility. */

typedef void (*QT_DISP_UNI_FCT) (const UniQos_t *uq, unsigned ofs);

/* Display a unified data field. */

typedef struct qos_type_st {
	QT_VALID_FCT	valid_fct;	/* Validity check function. */
	QT_TO_UNI_FCT	set_fct;	/* Set from specific -> unified QoS. */
	QT_TO_UNI_FCT	update_fct;	/* Update from specific -> unified QoS*/
	QT_FROM_UNI_FCT	get_fct;	/* From unified -> specific QoS. */
	QT_CLEAN_FCT	clean_fct;	/* Cleanup before free. */
	QT_MATCH_FCT	match_fct;	/* Matched Writer/Reader QoS check. */
	QT_DISP_UNI_FCT	disp_uni_fct;	/* Display type in unified QoS. */
} QT_DESC;


/* Integer type functions. */
/* ----------------------- */

static void qt_int_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	int	*ip, *dst;

	ARG_NOT_USED (disc)

	dst = (int *)((uintptr_t) qp + ofs);
	ip = src;
	*dst = *ip;
}

static void qt_int_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	const int	*src;
	int		*ip;

	ARG_NOT_USED (disc)

	src = (const int *)((uintptr_t) qp + ofs);
	ip = dst;
	*ip = *src;
}

static void qt_int_disp (const UniQos_t *qp, unsigned ofs)
{
	const int *ip = (const int *)((uintptr_t) qp + ofs);

	dbg_printf ("%d", *ip);
}

static const QT_DESC qt_int = {	
	NULL,
	qt_int_set,
	qt_int_set,
	qt_int_get,
	NULL,
	NULL,
	qt_int_disp
};


/* Boolean type functions. */
/* ----------------------- */

static int qt_boolean_valid (const void *src, int disc)
{
	const int	*ip = (const int *) src;

	ARG_NOT_USED (disc)

	if (*ip < 0 || *ip > 1)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_boolean_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	int	*ip;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	ip = src;
	qp->no_autodispose = !*ip;
}

static void qt_boolean_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	int	*ip;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	ip = dst;
	*ip = !qp->no_autodispose;
}

static void qt_boolean_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)
	dbg_printf ("%d", !qp->no_autodispose);
}

static const QT_DESC qt_boolean = {
	qt_boolean_valid,
	qt_boolean_set,
	qt_boolean_set,
	qt_boolean_get,
	NULL,
	NULL,
	qt_boolean_disp
};


/* Octet sequence type functions. */
/* ------------------------------ */

String_t *qos_octets2str (const DDS_OctetSeq *sp)
{
	if (!DDS_SEQ_LENGTH (*sp))
		return (NULL);

	return (str_new ((const char *) sp->_buffer, sp->_length, sp->_length, 0));
}

void qos_str2octets (const String_t *s, DDS_OctetSeq *sp)
{
	DDS_SEQ_INIT (*sp);
	if (s) {
		if (!sp->_buffer || sp->_maximum < str_len (s)) {
			if (!sp->_buffer)
				sp->_buffer = xmalloc (str_len (s));
			else
				sp->_buffer = xrealloc (sp->_buffer, str_len (s));
			sp->_maximum = str_len (s);
		}
		memcpy (sp->_buffer, str_ptr (s), str_len (s));
		sp->_length = str_len (s);
	}
}

static int qt_os_valid (const void *src, int disc)
{
	const DDS_OctetSeq *osp;

	if (!disc) {
		osp = (DDS_OctetSeq *) src;
		if (osp->_length > osp->_maximum ||
		    (osp->_length && !osp->_buffer) ||
		    (osp->_buffer && !osp->_maximum) ||
		    osp->_esize != 1)
			return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

static void qt_os_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	String_t	**dst, **sp;

	dst = (String_t **)((uintptr_t) qp + ofs);
	if (!disc)
		*dst = qos_octets2str (src);
	else {	/* Simply swap data; discovery data cleanup does the rest. */
		sp = src;
		*dst = *sp;
		*sp = NULL;
	}
}

static void qt_os_update (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	String_t	**dst, **sp;

	dst = (String_t **)((uintptr_t) qp + ofs);
	str_unref (*dst);
	if (!disc)
		*dst = qos_octets2str (src);
	else {	/* Simply swap data; discovery data cleanup does the rest. */
		sp = src;
		*dst = *sp;
		*sp = NULL;
	}
}

static void qt_os_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	const String_t	**src;
	String_t	**sp;

	src = (const String_t **)((uintptr_t) qp + ofs);
	if (!disc)
		qos_str2octets (*src, dst);
	else {
		sp = dst;
		*sp = str_ref ((String_t *) *src);
	}
}

static void qt_os_clean (UniQos_t *qp, unsigned ofs)
{
	String_t	*sp, **spp;

	spp = (String_t **)((uintptr_t) qp + ofs);
	sp = *spp;
	if (!sp)
		return;

	str_unref (sp);
	*spp = NULL;
}

static void qt_os_dump (const String_t *sp)
{
	if (!sp)
		dbg_printf ("<empty>");
	else {
		dbg_printf ("\r\n");
		dbg_print_region (str_ptr (sp), str_len (sp), 1, 1);
	}
}

static void qt_os_disp (const UniQos_t *qp, unsigned ofs)
{
	const String_t	**src;

	src = (const String_t **)((uintptr_t) qp + ofs);
	qt_os_dump (*src);
}


static const QT_DESC qt_os = {
	qt_os_valid,		/* Always valid. */
	qt_os_set,
	qt_os_update,
	qt_os_get,
	qt_os_clean,
	NULL,
	qt_os_disp
};


/* Partition type functions. */
/* ------------------------- */

static int qt_partition_valid (const void *src, int disc)
{
	const DDS_StringSeq	*ssp;
	unsigned		i;

	if (!disc) {
		ssp = (DDS_StringSeq *) src;
		if (ssp->_length > ssp->_maximum ||
		    (ssp->_length && !ssp->_buffer) ||
		    (ssp->_buffer && !ssp->_maximum))
			return (DDS_RETCODE_BAD_PARAMETER);

		for (i = 0; i < ssp->_length; i++)
			if (ssp->_buffer [i] == NULL)
				return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

static void qt_partition_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	Strings_t	*sp, *tp, **spp;
	DDS_StringSeq	*ssp;
	unsigned	i;

	ARG_NOT_USED (ofs)

	if (!disc) {
		ssp = (DDS_StringSeq *) src;
		if (ssp->_length) {
			qp->partition =
			sp = (Strings_t *) xmalloc (sizeof (Strings_t) +
					       sizeof (String_t *) * ssp->_length);
			if (!sp)
				return;

			sp->_buffer = (String_t **) (sp + 1);
			sp->_length = sp->_maximum = ssp->_length;
			sp->_esize = sizeof (String_t *);
			for (i = 0; i < ssp->_length; i++)
				sp->_buffer [i] = str_new_cstr (ssp->_buffer [i]);
		}
		else
			qp->partition = NULL;
	}
	else {	/* Simply swap data; discovery data cleanup does the rest. */
		spp = (Strings_t **) src;
		tp = qp->partition;
		qp->partition = *spp;
		*spp = tp;
	}
}

static void qt_partition_update (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	Strings_t	*sp, *tp, **spp;
	DDS_StringSeq	*ssp;
	unsigned	i;

	sp = qp->partition;
	if (!disc) {
		ssp = (DDS_StringSeq *) src;
		if (!sp && !ssp->_length)
			return;

		if (!ssp->_length) {
			for (i = 0; i < sp->_length; i++)
				str_unref (sp->_buffer [i]);
			xfree (sp);
			qp->partition = NULL;
			return;
		}
		if (!sp) {
			qt_partition_set (src, qp, ofs, disc);
			return;
		}
		if (sp->_length < ssp->_length) {
			if (sp->_maximum < ssp->_length) {
				qp->partition = 
				sp = xrealloc (sp, sizeof (Strings_t) +
						sizeof (String_t *) * ssp->_length);
				if (!sp)
					return;

				sp->_buffer = (String_t **) (sp + 1);
				for (i = sp->_length; i < ssp->_length; i++)
					sp->_buffer [i] = NULL;
				sp->_maximum = sp->_length = ssp->_length;
			}
		}
		else if (sp->_length > ssp->_length) {
			for (i = ssp->_length; i < sp->_length; i++)
				str_unref (sp->_buffer [i]);

			qp->partition = sp = xrealloc (sp, sizeof (Strings_t) +
						sizeof (String_t *) * ssp->_length);
			if (!sp)
				return;

			sp->_maximum = sp->_length = ssp->_length;
		}
		for (i = 0; i < ssp->_length; i++) {
			if (sp->_buffer [i] && !strcmp (ssp->_buffer [i], (char *) sp->_buffer [i]))
				continue;

			if (sp->_buffer [i] != NULL)
				str_unref (sp->_buffer [i]);
			sp->_buffer [i] = str_new_cstr (ssp->_buffer [i]);
		}
	}
	else {	/* Simply swap data; discovery data cleanup does the rest. */
		spp = (Strings_t **) src;
		tp = qp->partition;
		qp->partition = *spp;
		*spp = tp;
	}
}

static void qt_partition_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	Strings_t	**sst, *dp;
	DDS_StringSeq	*dsp;
	unsigned	i;

	ARG_NOT_USED (ofs)

	if (!disc) {
		dsp = (DDS_StringSeq *) dst;
		DDS_SEQ_INIT (*dsp);
		if (!qp->partition)
			return;

		if (!dsp->_buffer || dsp->_maximum < qp->partition->_length) {
			if (!dsp->_buffer)
				dsp->_buffer = xmalloc (qp->partition->_length * 
							       sizeof (char *));
			else
				dsp->_buffer = xrealloc (dsp->_buffer,
							qp->partition->_length *
							       sizeof (char *));
			if (!dsp->_buffer)
				return;

			dsp->_maximum = qp->partition->_length;
		}
		dsp->_length = qp->partition->_length;
		for (i = 0; i < qp->partition->_length; i++)
			dsp->_buffer [i] = (char *) str_ptr (qp->partition->_buffer [i]);
	}
	else {
		sst = (Strings_t **) dst;
		if (!qp->partition) {
			*sst = NULL;
			return;
		}
		*sst = dp = xmalloc (sizeof (Strings_t) +
				   qp->partition->_length * sizeof (String_t *));
		if (!dp)
			return;

		dp->_length = dp->_maximum = qp->partition->_length;
		dp->_buffer = (String_t **) (dp + 1);
		for (i = 0; i < dp->_length; i++)
			dp->_buffer [i] = str_ref (qp->partition->_buffer [i]);
	}
}

static void partition_free (Strings_t *pp)
{
	unsigned	i;
	String_t	*sp;

	for (i = 0; i < pp->_length; i++) {
		sp = pp->_buffer [i];
		if (!sp)
			continue;

		str_unref (pp->_buffer [i]);
		pp->_buffer [i] = NULL;
	}
	xfree (pp);
}

static void qt_partition_clean (UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	if (!qp->partition)
		return;

	partition_free (qp->partition);
	qp->partition = NULL;
}

static void qt_partition_dump (Strings_t *p)
{
	unsigned	i;

	if (!p) {
		dbg_printf ("<none>");
		return;
	}
	for (i = 0; i < p->_length; i++) {
		if (i)
			dbg_printf (", ");
		dbg_printf ("%s", str_ptr (p->_buffer [i]));
	}
}

static void qt_partition_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	qt_partition_dump (qp->partition);
}

static const QT_DESC qt_partition = {
	qt_partition_valid,
	qt_partition_set,
	qt_partition_update,
	qt_partition_get,
	qt_partition_clean,
	NULL,
	qt_partition_disp
};


/* Duration type functions. */
/* ------------------------ */

static int qos_valid_duration (const DDS_Duration_t *dp)
{
	if (dp->sec == DDS_DURATION_INFINITE_SEC &&
	    dp->nanosec == DDS_DURATION_INFINITE_NSEC)
		return (DDS_RETCODE_OK);

	if (dp->sec < 0 ||
	    dp->sec >= DDS_DURATION_INFINITE_SEC ||
	    dp->nanosec >= 1000000000UL)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static int qt_duration_valid (const void *src, int disc)
{
	ARG_NOT_USED (disc)

	return (qos_valid_duration (src));
}

static void qt_duration_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_Duration_t	*dp, *sp = (DDS_Duration_t *) src;

	ARG_NOT_USED (disc)

	dp = (DDS_Duration_t *) ((uintptr_t) qp + ofs);
	*dp = *sp;
}

static void qt_duration_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	const DDS_Duration_t	*sp;
	DDS_Duration_t		*dp = (DDS_Duration_t *) dst;

	ARG_NOT_USED (disc)

	sp = (const DDS_Duration_t *) ((uintptr_t) qp + ofs);
	*dp = *sp;
}

static void display_duration (const DDS_Duration_t *dp)
{
	if (dp->sec == DDS_DURATION_INFINITE_SEC &&
	    dp->nanosec == DDS_DURATION_INFINITE_NSEC)
		dbg_printf ("<inf>");
	else if (!dp->nanosec)
		dbg_printf ("%ds", dp->sec);
	else
		dbg_printf ("%d.%09us", dp->sec, dp->nanosec);
}

static void qt_duration_disp (const UniQos_t *qp, unsigned ofs)
{
	const DDS_Duration_t	*dp;

	dp = (const DDS_Duration_t *) ((uintptr_t) qp + ofs);
	display_duration (dp);
}

static int qt_duration_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	const DDS_Duration_t	*wdp, *rdp;

	wdp = (const DDS_Duration_t *) ((uintptr_t) wp + ofs);
	rdp = (const DDS_Duration_t *) ((uintptr_t) rp + ofs);
	if (wdp->sec > rdp->sec ||
	    (wdp->sec == rdp->sec && wdp->nanosec > rdp->nanosec))
		return (0);

	return (1);
}

static const QT_DESC qt_duration = {
	qt_duration_valid,
	qt_duration_set,
	qt_duration_set,
	qt_duration_get,
	NULL,
	qt_duration_match,
	qt_duration_disp
};


/* Timestamp type functions. */
/* ------------------------- */

static int qt_timestamp_valid (const void *src, int disc)
{
	const DDS_DestinationOrderQosPolicy *tp = (DDS_DestinationOrderQosPolicy *) src;

	ARG_NOT_USED (disc)

	if (/*tp->kind < DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS ||*/
	    tp->kind > DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS)
	    	return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_timestamp_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	const DDS_DestinationOrderQosPolicy *tp = (DDS_DestinationOrderQosPolicy *) src;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)

	qp->destination_order_kind = tp->kind;
}

static void qt_timestamp_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_DestinationOrderQosPolicy *tp = (DDS_DestinationOrderQosPolicy *) dst;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)

	tp->kind = qp->destination_order_kind;
}

static int qt_timestamp_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	return (wp->destination_order_kind >= rp->destination_order_kind);
}

static const char *qos_timestamp_kind_str [] = {
	"BY_RECEPTION_TIMESTAMP", "BY_SOURCE_TIMESTAMP"
};

static void qt_timestamp_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	dbg_printf ("%s", qos_timestamp_kind_str [qp->destination_order_kind]);
}

static const QT_DESC qt_timestamp = {
	qt_timestamp_valid,
	qt_timestamp_set,
	qt_timestamp_set,
	qt_timestamp_get,
	NULL,
	qt_timestamp_match,
	qt_timestamp_disp
};


/* Shared/exclusive functions. */
/* --------------------------- */

static int qt_shared_excl_valid (const void *src, int disc)
{
	const DDS_OwnershipQosPolicyKind *k = (const DDS_OwnershipQosPolicyKind *) src;

	ARG_NOT_USED (disc)

	if (/**k < DDS_SHARED_OWNERSHIP_QOS || */*k > DDS_EXCLUSIVE_OWNERSHIP_QOS)
		return (DDS_RETCODE_BAD_PARAMETER);
	else
		return (DDS_RETCODE_OK);
}

static void qt_shared_excl_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_OwnershipQosPolicyKind *k = (DDS_OwnershipQosPolicyKind *) src;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)

	qp->ownership_kind = *k;
}

static void qt_shared_excl_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_OwnershipQosPolicyKind *k = (DDS_OwnershipQosPolicyKind *) dst;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)

	*k = qp->ownership_kind;
}

static int qt_shared_excl_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	return (wp->ownership_kind == rp->ownership_kind);
}

static const char *qos_ownership_kind_str [] = {
	"SHARED", "EXCLUSIVE"
};

static void qt_shared_excl_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	dbg_printf ("%s", qos_ownership_kind_str [qp->ownership_kind]);
}

static const QT_DESC qt_shared_excl = {
	qt_shared_excl_valid,
	qt_shared_excl_set,
	qt_shared_excl_set,
	qt_shared_excl_get,
	NULL,
	qt_shared_excl_match,
	qt_shared_excl_disp
};


/* Durability type functions. */
/* -------------------------- */

static int qt_durability_valid (const void *src, int disc)
{
	const DDS_DurabilityQosPolicyKind *k = (const DDS_DurabilityQosPolicyKind *) src;

	if ((!disc && *k > DDS_TRANSIENT_LOCAL_DURABILITY_QOS) ||
	    (disc && *k > DDS_PERSISTENT_DURABILITY_QOS))
		return (DDS_RETCODE_BAD_PARAMETER);
	else
		return (DDS_RETCODE_OK);
}

static void qt_durability_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_DurabilityQosPolicyKind *k = (DDS_DurabilityQosPolicyKind *) src;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	qp->durability_kind = *k;
}

static void qt_durability_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_DurabilityQosPolicyKind *k = (DDS_DurabilityQosPolicyKind *) dst;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)
	
	*k = qp->durability_kind;
}

static int qt_durability_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	return (wp->durability_kind >= rp->durability_kind);
}

static const char *qos_durability_kind_str [] = {
	"VOLATILE", "TRANSIENT_LOCAL", "TRANSIENT", "PERSISTENT"
};

static void qt_durability_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)
	dbg_printf ("%s", qos_durability_kind_str [qp->durability_kind]);
}

static const QT_DESC qt_durability = {
	qt_durability_valid,
	qt_durability_set,
	qt_durability_set,
	qt_durability_get,
	NULL,
	qt_durability_match,
	qt_durability_disp
};


/* Durability Service type functions. */
/* ---------------------------------- */

static int qt_durability_service_valid (const void *src, int disc)
{
	const DDS_DurabilityServiceQosPolicy	*dsp;
	int					ret;

	ARG_NOT_USED (disc)

	dsp = (const DDS_DurabilityServiceQosPolicy *) src;
	if ((ret = qt_duration_valid (&dsp->service_cleanup_delay, 0)) != DDS_RETCODE_OK)
		return (ret);

	if (dsp->history_kind != DDS_KEEP_LAST_HISTORY_QOS &&
	    dsp->history_kind != DDS_KEEP_ALL_HISTORY_QOS)
		return (DDS_RETCODE_UNSUPPORTED);

	if (dsp->history_depth < 1)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((dsp->max_samples != DDS_LENGTH_UNLIMITED &&
	     dsp->max_samples < 1) ||
	    (dsp->max_instances != DDS_LENGTH_UNLIMITED &&
	     dsp->max_instances < 1) ||
	    (dsp->max_samples_per_instance != DDS_LENGTH_UNLIMITED &&
	     dsp->max_samples_per_instance < 1) ||
	    (unsigned) dsp->max_samples < (unsigned) dsp->max_samples_per_instance ||
	    (unsigned) dsp->history_depth > (unsigned) dsp->max_samples_per_instance)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_durability_service_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
#ifdef DURABILITY_SERVICE
	DDS_DurabilityServiceQosPolicy	*dsp = (DDS_DurabilityServiceQosPolicy *) src;
#else
	ARG_NOT_USED (src)
#endif

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

#ifdef DURABILITY_SERVICE
	qp->ds_history_kind = dsp->history_kind;
	qp->ds_cleanup_delay = dsp->cleanup_delay;
	qp->ds_history_depth = dsp->history_depth;
	qp->ds_limits.max_samples = dsp->max_samples;
	qp->ds_limits.max_instances = dsp->max_instances;
	qp->ds_limits.max_samples_per_instance = dsp->max_samples_per_instance;
#else
	ARG_NOT_USED (qp)
#endif
}

static void qt_durability_service_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_DurabilityServiceQosPolicy	*dsp = (DDS_DurabilityServiceQosPolicy *) dst;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

#ifdef DURABILITY_SERVICE
	dsp->history_kind = qp->ds_history_kind;
	dsp->cleanup_delay = qp->ds_cleanup_delay;
	dsp->history_depth = qp->ds_history_depth;
	dsp->max_samples = qp->ds_limits.max_samples;
	dsp->max_instances = qp->ds_limits.max_instances;
	dsp->max_samples_per_instance = qp->ds_limits.max_samples_per_instance;
#else
	ARG_NOT_USED (qp)

	*dsp = qos_durability_service_init;
#endif
}

static const char *qos_history_kind_str [] = {
	"KEEP_LAST", "KEEP_ALL"
};

#ifdef DURABILITY_SERVICE

static void display_length (int length)
{
	if (length == -1)
		dbg_printf ("UNLIMITED");
	else
		dbg_printf ("%d", length);
}
#endif

static void qt_durability_service_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

#ifdef DURABILITY_SERVICE
	dbg_printf ("%s,", qos_hist_kind_str [qp->ds_history_kind]);
	display_duration (qp->ds_cleanup_delay);
	if (qp->ds_history_kind == DDS_KEEP_LAST_HISTORY_QOS)
		dbg_printf (",%u", qp->ds_history_depth);
	dbg_printf (",");
	display_length (qp->ds_limits.max_samples);
	dbg_printf (",");
	display_length (qp->ds_limits.max_instances);
	dbg_printf (",");
	display_length (qp->ds_limits.max_samples_per_instance);
#else
	ARG_NOT_USED (qp)
#endif
}

static const QT_DESC qt_durability_service = {
	qt_durability_service_valid,
	qt_durability_service_set,
	qt_durability_service_set,
	qt_durability_service_get,
	NULL,
	NULL,
	qt_durability_service_disp
};


/* Presentation type functions. */
/* ---------------------------- */

static int qt_presentation_valid (const void *src, int disc)
{
	const DDS_PresentationQosPolicy	*pp = (const DDS_PresentationQosPolicy *) src;

	ARG_NOT_USED (disc)

	if (/*pp->access_scope < DDS_INSTANCE_PRESENTATION_QOS ||*/
	    pp->access_scope > DDS_GROUP_PRESENTATION_QOS)
		return (DDS_RETCODE_UNSUPPORTED);

	if (pp->coherent_access < 0 || pp->coherent_access > 1)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (pp->ordered_access < 0 || pp->ordered_access > 1)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_presentation_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_PresentationQosPolicy	*pp = (DDS_PresentationQosPolicy *) src;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)

	qp->presentation_access_scope = pp->access_scope;
	qp->presentation_coherent_access = pp->coherent_access;
	qp->presentation_ordered_access = pp->ordered_access;
}

static void qt_presentation_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_PresentationQosPolicy	*pp = (DDS_PresentationQosPolicy *) dst;

	ARG_NOT_USED (ofs)
	ARG_NOT_USED (disc)

	pp->access_scope = qp->presentation_access_scope;
	pp->coherent_access = qp->presentation_coherent_access;
	pp->ordered_access = qp->presentation_ordered_access;
}

#define qos_presentation_match(w_sc,r_sc,w_co,r_co,w_order,r_order)	\
		((w_sc) >= (r_sc) && 					\
		(!(r_co) || (w_co) == (r_co)) && 			\
		(!(r_order) || (w_order)==(r_order)))

static int qt_presentation_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	return (qos_presentation_match (wp->presentation_access_scope,
					rp->presentation_access_scope,
					wp->presentation_coherent_access,
					rp->presentation_coherent_access,
					wp->presentation_ordered_access,
					rp->presentation_ordered_access));
}

static const char *qos_presentation_scope_str [] = {
	"INSTANCE", "TOPIC", "GROUP"
};

static void qt_presentation_dump (int scope, int coherent, int ordered)
{
	if (scope <= DDS_GROUP_PRESENTATION_QOS)
		dbg_printf ("%s", qos_presentation_scope_str [scope]);
	else
		dbg_printf ("?(%d)", scope);
	dbg_printf (", coherent_access=%u, ordered_access=%u",
		coherent, ordered);
}

static void qt_presentation_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	qt_presentation_dump (qp->presentation_access_scope,
			      qp->presentation_coherent_access,
			      qp->presentation_ordered_access);
}

static const QT_DESC qt_presentation = {
	qt_presentation_valid,
	qt_presentation_set,
	qt_presentation_set,
	qt_presentation_get,
	NULL,
	qt_presentation_match,
	qt_presentation_disp
};


/* Liveliness type functions. */
/* -------------------------- */

static int qt_liveliness_valid (const void *src, int disc)
{
	const DDS_LivelinessQosPolicy	*lp = (const DDS_LivelinessQosPolicy *) src;
	int				ret;

	ARG_NOT_USED (disc)

	if (/*lp->kind < DDS_AUTOMATIC_LIVELINESS_QOS ||*/
	    lp->kind > DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS)
		return (DDS_RETCODE_BAD_PARAMETER);

	if ((ret = qos_valid_duration (&lp->lease_duration)) != DDS_RETCODE_OK)
		return (ret);

	if (lp->lease_duration.sec == DDS_DURATION_ZERO_SEC &&
	    lp->lease_duration.nanosec == DDS_DURATION_ZERO_NSEC)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (lp->kind != DDS_AUTOMATIC_LIVELINESS_QOS &&
	    (lp->lease_duration.sec == DDS_DURATION_INFINITE_SEC ||
	     lp->lease_duration.nanosec == DDS_DURATION_INFINITE_NSEC))
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_liveliness_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_LivelinessQosPolicy	*lp = (DDS_LivelinessQosPolicy *) src;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	qp->liveliness_kind = lp->kind;
	qp->liveliness_lease_duration = lp->lease_duration;
}

static void qt_liveliness_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_LivelinessQosPolicy	*lp = (DDS_LivelinessQosPolicy *) dst;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	lp->kind = qp->liveliness_kind;
	lp->lease_duration = qp->liveliness_lease_duration;
}

static int qt_liveliness_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	if (wp->liveliness_kind < rp->liveliness_kind)
		return (0);

	if (wp->liveliness_lease_duration.sec > rp->liveliness_lease_duration.sec ||
	    (wp->liveliness_lease_duration.sec == rp->liveliness_lease_duration.sec &&
	     wp->liveliness_lease_duration.nanosec > rp->liveliness_lease_duration.nanosec))
		return (0);

	return (1);
}

static const char *qos_liveliness_kind_str [] = {
	"AUTOMATIC", "MANUAL_BY_PARTICIPANT", "MANUAL_BY_TOPIC"
};

static void qt_liveliness_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	dbg_printf ("%s, lease_duration=", qos_liveliness_kind_str [qp->liveliness_kind]);
	display_duration (&qp->liveliness_lease_duration);
}

static const QT_DESC qt_liveliness = {
	qt_liveliness_valid,
	qt_liveliness_set,
	qt_liveliness_set,
	qt_liveliness_get,
	NULL,
	qt_liveliness_match,
	qt_liveliness_disp
};


/* Reliability type functions. */
/* -------------------------- */

static int qt_reliability_valid (const void *src, int disc)
{
	const DDS_ReliabilityQosPolicy *rp = (const DDS_ReliabilityQosPolicy *) src;

	ARG_NOT_USED (disc)

	if (/*rp->kind < DDS_BEST_EFFORT_RELIABILITY_QOS ||*/
	    rp->kind > DDS_RELIABLE_RELIABILITY_QOS)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (qos_valid_duration (&rp->max_blocking_time));
}

static void qt_reliability_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_ReliabilityQosPolicy *rp = (DDS_ReliabilityQosPolicy *) src;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	qp->reliability_kind = rp->kind;
	qp->reliability_max_blocking_time = rp->max_blocking_time;
}

static void qt_reliability_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_ReliabilityQosPolicy *rp = (DDS_ReliabilityQosPolicy *) dst;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	rp->kind = qp->reliability_kind;
	rp->max_blocking_time = qp->reliability_max_blocking_time;
}

static int qt_reliability_match (const UniQos_t *wp, const UniQos_t *rp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	return (wp->reliability_kind >= rp->reliability_kind);
}

static const char *qos_reliability_kind_str [] = {
	"BEST_EFFORT", "RELIABLE"
};

static void qt_reliability_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	dbg_printf ("%s, max_blocking_time=", qos_reliability_kind_str [qp->reliability_kind]);
	display_duration (&qp->reliability_max_blocking_time);
}

static const QT_DESC qt_reliability = {
	qt_reliability_valid,
	qt_reliability_set,
	qt_reliability_set,
	qt_reliability_get,
	NULL,
	qt_reliability_match,
	qt_reliability_disp
};


/* History type functions. */
/* ----------------------- */

static int qt_history_valid (const void *src, int disc)
{
	const DDS_HistoryQosPolicy *hp = (const DDS_HistoryQosPolicy *) src;

	ARG_NOT_USED (disc)

	if (/*hp->kind < DDS_KEEP_LAST_HISTORY_QOS ||*/
	    hp->kind > DDS_KEEP_ALL_HISTORY_QOS)
		return (DDS_RETCODE_BAD_PARAMETER);

	if (hp->kind == DDS_KEEP_LAST_HISTORY_QOS && hp->depth < 1)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_history_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_HistoryQosPolicy *hp = (DDS_HistoryQosPolicy *) src;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	qp->history_kind = hp->kind;
	qp->history_depth = hp->depth;
}

static void qt_history_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_HistoryQosPolicy *hp = (DDS_HistoryQosPolicy *) dst;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	hp->kind = qp->history_kind;
	hp->depth = qp->history_depth;
}

static void qt_history_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	dbg_printf ("%s", qos_history_kind_str [qp->history_kind]);
	if (qp->history_kind == DDS_KEEP_LAST_HISTORY_QOS)
		dbg_printf (", depth=%d", qp->history_depth);
}

static const QT_DESC qt_history = {
	qt_history_valid,
	qt_history_set,
	qt_history_set,
	qt_history_get,
	NULL,
	NULL,
	qt_history_disp
};


/* Resource limits type functions. */
/* ------------------------------- */

static int qt_resource_limits_valid (const void *src, int disc)
{
	const DDS_ResourceLimitsQosPolicy *lp = (const DDS_ResourceLimitsQosPolicy *) src;

	ARG_NOT_USED (disc)

	if ((lp->max_samples != DDS_LENGTH_UNLIMITED &&
	     lp->max_samples < 1) ||
	    (lp->max_instances != DDS_LENGTH_UNLIMITED &&
	     lp->max_instances < 1) ||
	    (lp->max_samples_per_instance != DDS_LENGTH_UNLIMITED &&
	     lp->max_samples_per_instance < 1) ||
	    (unsigned) lp->max_samples < (unsigned) lp->max_samples_per_instance)
		return (DDS_RETCODE_BAD_PARAMETER);

	return (DDS_RETCODE_OK);
}

static void qt_resource_limits_set (void *src, UniQos_t *qp, unsigned ofs, int disc)
{
	DDS_ResourceLimitsQosPolicy *lp = (DDS_ResourceLimitsQosPolicy *) src;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	qp->resource_limits = *lp;
}

static void qt_resource_limits_get (const UniQos_t *qp, unsigned ofs, void *dst, int disc)
{
	DDS_ResourceLimitsQosPolicy *lp = (DDS_ResourceLimitsQosPolicy *) dst;

	ARG_NOT_USED (disc)
	ARG_NOT_USED (ofs)

	*lp = qp->resource_limits;
}

static void qt_resource_limits_disp (const UniQos_t *qp, unsigned ofs)
{
	ARG_NOT_USED (ofs)

	dbg_printf ("max_samples=");
	if (qp->resource_limits.max_samples == -1)
		dbg_printf ("<inf>");
	else
		dbg_printf ("%d", qp->resource_limits.max_samples);
	dbg_printf (", max_inst=");
	if (qp->resource_limits.max_instances == -1)
		dbg_printf ("<inf>");
	else
		dbg_printf ("%d", qp->resource_limits.max_instances);
	dbg_printf (", max_samples_per_inst=");
	if (qp->resource_limits.max_samples_per_instance == -1)
		dbg_printf ("<inf>");
	else
		dbg_printf ("%d", qp->resource_limits.max_samples_per_instance);
}

static const QT_DESC qt_resource_limits = {
	qt_resource_limits_valid,
	qt_resource_limits_set,
	qt_resource_limits_set,
	qt_resource_limits_get,
	NULL,
	NULL,
	qt_resource_limits_disp
};


/* Time Based Filter type functions. */
/* --------------------------------- */

#ifdef DDS_DEBUG

static void qt_time_filter_dump (DDS_TimeBasedFilterQosPolicy *qp)
{
	dbg_printf ("minimum_separation=");
	display_duration (&qp->minimum_separation);
}

#endif /* DDS_DEBUG */


/* Reader data lifecycle type functions. */
/* ------------------------------------- */

static int qt_rd_lifecycle_valid (const void *src, int disc)
{
	const DDS_ReaderDataLifecycleQosPolicy *rp = (const DDS_ReaderDataLifecycleQosPolicy *) src;

	if (qos_valid_duration (&rp->autopurge_nowriter_samples_delay) ||
	    qos_valid_duration (&rp->autopurge_disposed_samples_delay))
		return (DDS_RETCODE_BAD_PARAMETER);

	if (!disc &&
	    (rp->autopurge_nowriter_samples_delay.sec != DDS_DURATION_INFINITE_SEC ||
	     rp->autopurge_nowriter_samples_delay.nanosec != DDS_DURATION_INFINITE_NSEC ||
	     rp->autopurge_disposed_samples_delay.sec != DDS_DURATION_INFINITE_SEC ||
	     rp->autopurge_disposed_samples_delay.nanosec != DDS_DURATION_INFINITE_NSEC))
		return (0);

	return (DDS_RETCODE_OK);
}

#ifdef DDS_DEBUG

static void qt_rd_lifecycle_dump (DDS_ReaderDataLifecycleQosPolicy *qp)
{
	dbg_printf ("autopurge_nowriter=");
	display_duration (&qp->autopurge_nowriter_samples_delay);
	dbg_printf (", autopurge_disposed=");
	display_duration (&qp->autopurge_disposed_samples_delay);
}

#endif

static const QT_DESC qt_rd_lifecycle = {
	qt_rd_lifecycle_valid,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/* Table of QoS type descriptors: */
static const QT_DESC *qos_type_ops [] = {	/* Indexed by QTYPE. */
	&qt_int,
	&qt_boolean,
	&qt_os,
	&qt_partition,
	&qt_duration,
	&qt_timestamp,
	&qt_shared_excl,
	&qt_durability,
	&qt_durability_service,
	&qt_presentation,
	&qt_liveliness,
	&qt_reliability,
	&qt_history,
	&qt_resource_limits,
	&qt_rd_lifecycle
};

/* QoS parameters: */
typedef enum {
	QP_USER_DATA,
	QP_TOPIC_DATA,
	QP_GROUP_DATA,
	QP_DURABILITY,
	QP_DURABILITY_SERVICE,
	QP_PRESENTATION,
	QP_DEADLINE,
	QP_LATENCY_BUDGET,
	QP_OWNERSHIP,
	QP_OWNERSHIP_STRENGTH,
	QP_LIVELINESS,
	QP_TIME_BASED_FILTER,
	QP_PARTITION,
	QP_RELIABILITY,
	QP_TRANSPORT_PRIORITY,
	QP_LIFESPAN,
	QP_DESTINATION_ORDER,
	QP_HISTORY,
	QP_RESOURCE_LIMITS,
	QP_ENTITY_FACTORY,
	QP_WRITER_DATA_LIFECYCLE,
	QP_READER_DATA_LIFECYCLE,

	QP_MAX
} QPAR;

#define	QP_MIN	QP_USER_DATA

typedef enum {
	UF_NU,				/* Not significant. */
	UF_DURABILITY_KIND,		/* DURABILITY.kind field. */
	UF_OWNERSHIP_KIND,		/* OWNERSHIP.kind field. */
	UF_DESTINATION_ORDER_KIND,	/* DESTINATION_ORDER.kind field. */
	UF_NO_AUTO_DISPOSE_UNREG,	/* !QT_WRITER_DATA_LIFECYCLE.autodispose. */
	UF_NO_AUTO_ENABLE		/* !QT_WRITER_DATA_LIFECYCLE.autodispose. */
} UQ_BITFIELD;

#define	OFS_NU	-1

typedef enum {
	QG_TOPIC,			/* Topic QoS group. */
	QG_WRITER,			/* DataWriter QoS group. */
	QG_READER,			/* DataReader QoS group. */
	QG_DISC_TOPIC,			/* Discovered Topic QoS group. */
	QG_DISC_WRITER,			/* Discovered DataWriter QoS group. */
	QG_DISC_READER,			/* Discovered DataReader QoS group. */

	QG_MAX
} QGROUP;

typedef struct qos_desc_st {
	const char	*name;		/* Name of QoS parameter. */
	unsigned	check:1;	/* Compatibility check required. */
	unsigned	changeable:1;	/* Can be updated afterwards. */
	unsigned	in_parent:1;	/* Use parent for local value. */
	unsigned	in_topic:1;	/* Use topic for local value. */
	DDS_QOS_POLICY_ID policy_id;	/* QoS policy identification. */
	QTYPE		type;		/* What QoS type. */
	int		uni_ofs;	/* Offset in unified QoS or */
	UQ_BITFIELD	uni_field;	/* Field type in unified QoS. */
	size_t		size;		/* Size of DDS QoS parameter. */
	const void	*init;		/* Initial value of DDS QoS parameter.*/
	const void	*disc_init;	/* Initial value of Discovered QoS. */
	int		offsets [QG_MAX]; /* Offset in group record. */
} Q_DESC;

const DDS_OctetSeq qos_data_init = DDS_SEQ_INITIALIZER (unsigned char);
const String_t *qos_disc_data_init = NULL;
const DDS_DurabilityQosPolicy qos_durability_init = {
	DDS_VOLATILE_DURABILITY_QOS
};
const DDS_DurabilityServiceQosPolicy qos_durability_service_init = {
	{ DDS_DURATION_ZERO_SEC, DDS_DURATION_ZERO_NSEC },
	  DDS_KEEP_LAST_HISTORY_QOS, 1,
	  DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED
};
const DDS_PresentationQosPolicy qos_presentation_init = {
	DDS_INSTANCE_PRESENTATION_QOS, 0, 0 
};
const DDS_DeadlineQosPolicy qos_deadline_init = {
	{ DDS_DURATION_INFINITE_SEC, DDS_DURATION_INFINITE_NSEC }
};
const DDS_LatencyBudgetQosPolicy qos_latency_init = {
	{ DDS_DURATION_ZERO_SEC, DDS_DURATION_ZERO_NSEC }
};
const DDS_OwnershipQosPolicy qos_ownership_init = {
	DDS_SHARED_OWNERSHIP_QOS
};
const DDS_OwnershipStrengthQosPolicy qos_ownership_strength_init = {
	0
};
const DDS_LivelinessQosPolicy qos_liveliness_init = {
	DDS_AUTOMATIC_LIVELINESS_QOS,
	{ DDS_DURATION_INFINITE_SEC, DDS_DURATION_INFINITE_NSEC }
};
const DDS_TimeBasedFilterQosPolicy qos_time_based_filter_init = {
	{ DDS_DURATION_ZERO_SEC, DDS_DURATION_ZERO_NSEC }
};
const DDS_PartitionQosPolicy qos_partition_init = {
	DDS_SEQ_INITIALIZER (char *)
};
const Strings_t *qos_disc_partition_init = NULL;

#define	MS	1000000

const DDS_ReliabilityQosPolicy qos_reliability_init = {
	DDS_BEST_EFFORT_RELIABILITY_QOS,
	{ 0, 100 * MS }
};
const DDS_TransportPriorityQosPolicy qos_transport_prio_init = {
	0
};
const DDS_LifespanQosPolicy qos_lifespan_init = {
	{ DDS_DURATION_INFINITE_SEC, DDS_DURATION_INFINITE_NSEC }
};
const DDS_DestinationOrderQosPolicy qos_destination_order_init = {
	DDS_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS
};
const DDS_HistoryQosPolicy qos_history_init = {
	DDS_KEEP_LAST_HISTORY_QOS, 1
};
const DDS_ResourceLimitsQosPolicy qos_resource_limits_init = {
	DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED
};
const DDS_EntityFactoryQosPolicy qos_entity_factory_init = {
	1
};
const DDS_WriterDataLifecycleQosPolicy qos_writer_data_lifecycle_init = {
	1
};
const DDS_ReaderDataLifecycleQosPolicy qos_reader_data_lifecycle_init = {
	{ DDS_DURATION_INFINITE_SEC, DDS_DURATION_INFINITE_NSEC },
	{ DDS_DURATION_INFINITE_SEC, DDS_DURATION_INFINITE_NSEC }
};

static const Q_DESC qd_user_data = {
	"USER_DATA", 0, 1, 0, 0,
	DDS_USERDATA_QOS_POLICY_ID, QT_OCTETSEQ,
	offsetof (UniQos_t, user_data), UF_NU,
	sizeof (DDS_UserDataQosPolicy),
	&qos_data_init, &qos_disc_data_init,
	{
		OFS_NU,
		offsetof (DDS_DataWriterQos, user_data),
		offsetof (DDS_DataReaderQos, user_data),
	 	OFS_NU,
		offsetof (DiscoveredWriterQos, user_data),
		offsetof (DiscoveredReaderQos, user_data),
	}
}, qd_topic_data = {
	"TOPIC_DATA", 0, 1, 0, 1,
	DDS_TOPICDATA_QOS_POLICY_ID, QT_OCTETSEQ,
	offsetof (UniQos_t, topic_data), UF_NU,
	sizeof (DDS_TopicDataQosPolicy),
	&qos_data_init, &qos_disc_data_init,
	{
		offsetof (DDS_TopicQos, topic_data),
		OFS_NU,
		OFS_NU,
		offsetof (DiscoveredTopicQos, topic_data),
		offsetof (DiscoveredWriterQos, topic_data),
		offsetof (DiscoveredReaderQos, topic_data)
	}
}, qd_group_data = {
	"GROUP_DATA", 0, 1, 1, 0,
	DDS_GROUPDATA_QOS_POLICY_ID, QT_OCTETSEQ,
	offsetof (UniQos_t, group_data), UF_NU,
	sizeof (DDS_GroupDataQosPolicy),
	&qos_data_init, &qos_disc_data_init,
	{ 
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU,
		offsetof (DiscoveredWriterQos, group_data),
		offsetof (DiscoveredReaderQos, group_data)
	}
}, qd_durability = {
	"DURABILITY", 1, 0, 0, 0,
	DDS_DURABILITY_QOS_POLICY_ID, QT_DURABILITY,
	OFS_NU, UF_DURABILITY_KIND,
	sizeof (DDS_DurabilityQosPolicy),
	&qos_durability_init, &qos_durability_init,
	{
		offsetof (DDS_TopicQos, durability),
		offsetof (DDS_DataWriterQos, durability),
		offsetof (DDS_DataReaderQos, durability),
		offsetof (DiscoveredTopicQos, durability),
		offsetof (DiscoveredWriterQos, durability),
		offsetof (DiscoveredReaderQos, durability)
	}
}, qd_durability_service = {
	"DURABILITY_SERVICE", 0, 0, 0, 0,
	DDS_DURABILITYSERVICE_QOS_POLICY_ID, QT_DURABILITY_SERVICE,
	OFS_NU, UF_NU,
	sizeof (DDS_DurabilityServiceQosPolicy),
	&qos_durability_service_init, &qos_durability_service_init,
	{
		offsetof (DDS_TopicQos, durability_service),
		offsetof (DDS_DataWriterQos, durability_service),
		OFS_NU,
		offsetof (DiscoveredTopicQos, durability_service),
		offsetof (DiscoveredWriterQos, durability_service),
		OFS_NU
	}
}, qd_presentation = {
	"PRESENTATION", 1, 0, 1, 0,
	DDS_PRESENTATION_QOS_POLICY_ID, QT_PRESENTATION,
	OFS_NU, UF_NU,
	sizeof (DDS_PresentationQosPolicy),
	&qos_presentation_init, &qos_presentation_init,
	{
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU,
		offsetof (DiscoveredWriterQos, presentation),
		offsetof (DiscoveredReaderQos, presentation)
	}
}, qd_deadline = {
	"DEADLINE", 1, 1, 0, 0, 
	DDS_DEADLINE_QOS_POLICY_ID, QT_DURATION,
	offsetof (UniQos_t, deadline), UF_NU,
	sizeof (DDS_DeadlineQosPolicy),
	&qos_deadline_init, &qos_deadline_init,
	{
		offsetof (DDS_TopicQos, deadline),
		offsetof (DDS_DataWriterQos, deadline),
		offsetof (DDS_DataReaderQos, deadline),
		offsetof (DiscoveredTopicQos, deadline),
		offsetof (DiscoveredWriterQos, deadline),
		offsetof (DiscoveredReaderQos, deadline)
	}
}, qd_latency_budget = {
	"LATENCY_BUDGET", 1, 1, 0, 0, 
	DDS_LATENCYBUDGET_QOS_POLICY_ID, QT_DURATION,
	offsetof (UniQos_t, latency_budget), UF_NU,
	sizeof (DDS_LatencyBudgetQosPolicy),
	&qos_latency_init, &qos_latency_init,
	{
		offsetof (DDS_TopicQos, latency_budget),
		offsetof (DDS_DataWriterQos, latency_budget),
		offsetof (DDS_DataReaderQos, latency_budget),
		offsetof (DiscoveredTopicQos, latency_budget),
		offsetof (DiscoveredWriterQos, latency_budget),
		offsetof (DiscoveredReaderQos, latency_budget)
	}
}, qd_ownership = {
	"OWNERSHIP", 1, 0, 0, 0, 
	DDS_OWNERSHIP_QOS_POLICY_ID, QT_SHARED_EXCLUSIVE,
	OFS_NU, UF_OWNERSHIP_KIND,
	sizeof (DDS_OwnershipQosPolicy),
	&qos_ownership_init, &qos_ownership_init,
	{
		offsetof (DDS_TopicQos, ownership),
		offsetof (DDS_DataWriterQos, ownership),
		offsetof (DDS_DataReaderQos, ownership),
		offsetof (DiscoveredTopicQos, ownership),
		offsetof (DiscoveredWriterQos, ownership),
		offsetof (DiscoveredReaderQos, ownership)
	}
}, qd_ownership_strength = {
	"OWNERSHIP_STRENGTH", 0, 1, 0, 0, 
	DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID, QT_INTEGER,
	offsetof (UniQos_t, ownership_strength), UF_NU,
	sizeof (DDS_OwnershipStrengthQosPolicy),
	&qos_ownership_strength_init, &qos_ownership_strength_init,
	{
		OFS_NU,
		offsetof (DDS_DataWriterQos, ownership_strength),
		OFS_NU,
		OFS_NU,
		offsetof (DiscoveredWriterQos, ownership_strength),
		OFS_NU
	}
}, qd_liveliness = {
	"LIVELINESS", 1, 0, 0, 0, 
	DDS_LIVELINESS_QOS_POLICY_ID, QT_LIVELINESS,
	OFS_NU, UF_NU,
	sizeof (DDS_LivelinessQosPolicy),
	&qos_liveliness_init, &qos_liveliness_init,
	{
		offsetof (DDS_TopicQos, liveliness),
		offsetof (DDS_DataWriterQos, liveliness),
		offsetof (DDS_DataReaderQos, liveliness),
		offsetof (DiscoveredTopicQos, liveliness),
		offsetof (DiscoveredWriterQos, liveliness),
		offsetof (DiscoveredReaderQos, liveliness)
	}
}, qd_time_based_filter = {
	"TIME_BASED_FILTER", 0, 1, 0, 0, 
	DDS_TIMEBASEDFILTER_QOS_POLICY_ID, QT_DURATION,
	OFS_NU, UF_NU,
	sizeof (DDS_TimeBasedFilterQosPolicy), 
	&qos_time_based_filter_init, &qos_time_based_filter_init,
	{
		OFS_NU,
		OFS_NU,
		offsetof (DDS_DataReaderQos, time_based_filter),
		OFS_NU,
		OFS_NU,
		offsetof (DiscoveredReaderQos, time_based_filter)
	}
}, qd_partition = {
	"PARTITION", 0, 1, 1, 0,
	DDS_PARTITION_QOS_POLICY_ID, QT_PARTITION,
	offsetof (UniQos_t, partition), UF_NU,
	sizeof (DDS_PartitionQosPolicy),
	&qos_partition_init, &qos_disc_partition_init,
	{
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU,
		offsetof (DiscoveredWriterQos, partition),
		offsetof (DiscoveredReaderQos, partition)
	}
}, qd_reliability = {
	"RELIABILITY", 1, 0, 0, 0, 
	DDS_RELIABILITY_QOS_POLICY_ID, QT_RELIABILITY,
	OFS_NU, UF_NU,
	sizeof (DDS_ReliabilityQosPolicy),
	&qos_reliability_init, &qos_reliability_init,
	{
		offsetof (DDS_TopicQos, reliability),
		offsetof (DDS_DataWriterQos, reliability),
		offsetof (DDS_DataReaderQos, reliability),
		offsetof (DiscoveredTopicQos, reliability),
		offsetof (DiscoveredWriterQos, reliability),
		offsetof (DiscoveredReaderQos, reliability)
	}
}, qd_transport_priority = {
	"TRANSPORT_PRIORITY", 0, 1, 0, 0,
	DDS_TRANSPORTPRIORITY_QOS_POLICY_ID, QT_INTEGER,
	offsetof (UniQos_t, transport_priority), UF_NU,
	sizeof (DDS_TransportPriorityQosPolicy),
	&qos_transport_prio_init, &qos_transport_prio_init,
	{
		offsetof (DDS_TopicQos, transport_priority),
		offsetof (DDS_DataWriterQos, transport_priority),
		OFS_NU,
		offsetof (DiscoveredTopicQos, transport_priority),
		OFS_NU,
		OFS_NU
	}
}, qd_lifespan = {
	"LIFESPAN", 0, 1, 0, 0,
	DDS_LIFESPAN_QOS_POLICY_ID, QT_DURATION,
	offsetof (UniQos_t, lifespan), UF_NU,
	sizeof (DDS_LifespanQosPolicy),
	&qos_lifespan_init, &qos_lifespan_init,
	{
		offsetof (DDS_TopicQos, lifespan),
		offsetof (DDS_DataWriterQos, lifespan),
		OFS_NU,
		offsetof (DiscoveredTopicQos, lifespan),
		offsetof (DiscoveredWriterQos, lifespan),
		OFS_NU
	}
}, qd_destination_order = {
	"DESTINATION_ORDER", 1, 0, 0, 0,
	DDS_DESTINATIONORDER_QOS_POLICY_ID, QT_TIMESTAMP_TYPE,
	OFS_NU, UF_DESTINATION_ORDER_KIND,
	sizeof (DDS_DestinationOrderQosPolicy),
	&qos_destination_order_init, &qos_destination_order_init,
	{
		offsetof (DDS_TopicQos, destination_order),
		offsetof (DDS_DataWriterQos, destination_order),
		offsetof (DDS_DataReaderQos, destination_order),
		offsetof (DiscoveredTopicQos, destination_order),
		offsetof (DiscoveredWriterQos, destination_order),
		offsetof (DiscoveredReaderQos, destination_order)
	}
}, qd_history = {
	"HISTORY", 0, 0, 0, 0, 
	DDS_HISTORY_QOS_POLICY_ID, QT_HISTORY,
	OFS_NU, UF_NU,
	sizeof (DDS_HistoryQosPolicy),
	&qos_history_init, &qos_history_init,
	{
		offsetof (DDS_TopicQos, history),
		offsetof (DDS_DataWriterQos, history),
		offsetof (DDS_DataReaderQos, history),
		offsetof (DiscoveredTopicQos, history),
		OFS_NU,
		OFS_NU
	}
}, qd_resource_limits = {
	"RESOURCE_LIMITS", 0, 0, 0, 0,
	DDS_RESOURCELIMITS_QOS_POLICY_ID, QT_RESOURCE_LIMITS,
	offsetof (UniQos_t, resource_limits), UF_NU,
	sizeof (DDS_ResourceLimitsQosPolicy),
	&qos_resource_limits_init, &qos_resource_limits_init,
	{
		offsetof (DDS_TopicQos, resource_limits),
		offsetof (DDS_DataWriterQos, resource_limits),
		offsetof (DDS_DataReaderQos, resource_limits),
		offsetof (DiscoveredTopicQos, resource_limits),
		OFS_NU,
		OFS_NU
	}
}, qd_entity_factory = {
	"ENTITY_FACTORY", 0, 1, 1, 0, 
	DDS_ENTITYFACTORY_QOS_POLICY_ID, QT_BOOLEAN,
	OFS_NU, UF_NO_AUTO_ENABLE,
	sizeof (DDS_EntityFactoryQosPolicy),
	&qos_entity_factory_init, &qos_entity_factory_init,
	{
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU
	}
}, qd_writer_data_lifecycle = {
	"WRITER_DATA_LIFECYCLE", 0, 1, 0, 0, 
	DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID, QT_BOOLEAN,
	OFS_NU, UF_NO_AUTO_DISPOSE_UNREG,
	sizeof (DDS_WriterDataLifecycleQosPolicy),
	&qos_writer_data_lifecycle_init, &qos_writer_data_lifecycle_init,
	{
		OFS_NU,
		offsetof (DDS_DataWriterQos, writer_data_lifecycle),
		OFS_NU,
		OFS_NU,
		OFS_NU,
		OFS_NU
	}
}, qd_reader_data_lifecycle = {
	"READER_DATA_LIFECYCLE", 0, 1, 0, 0, 
	DDS_READERDATALIFECYCLE_QOS_POLICY_ID, QT_READER_DATA_LIFECYCLE,
	OFS_NU, UF_NU,
	sizeof (DDS_ReaderDataLifecycleQosPolicy),
	&qos_reader_data_lifecycle_init, &qos_reader_data_lifecycle_init,
	{
		OFS_NU,
		OFS_NU,
		offsetof (DDS_DataReaderQos, reader_data_lifecycle),
		OFS_NU,
		OFS_NU,
		OFS_NU
	}
};

static const Q_DESC *qos_descs [] = {
	&qd_user_data, &qd_topic_data, &qd_group_data,
	&qd_durability, &qd_durability_service,
	&qd_presentation, &qd_deadline, &qd_latency_budget,
	&qd_ownership, &qd_ownership_strength,
	&qd_liveliness, &qd_time_based_filter,
	&qd_partition, &qd_reliability, &qd_transport_priority,
	&qd_lifespan, &qd_destination_order, &qd_history,
	&qd_resource_limits, &qd_entity_factory,
	&qd_writer_data_lifecycle, &qd_reader_data_lifecycle
};

typedef struct qos_list_desc_st {
	size_t	ofs;
	QPAR	qos;
} Q_LFIELD;

static const Q_LFIELD qos_participant_list [] = {
	{ offsetof (DDS_DomainParticipantQos, user_data), QP_USER_DATA },
	{ offsetof (DDS_DomainParticipantQos, entity_factory), QP_ENTITY_FACTORY }
};

static const Q_LFIELD qos_publisher_list [] = {
	{ offsetof (DDS_PublisherQos, presentation), QP_PRESENTATION },
	{ offsetof (DDS_PublisherQos, partition), QP_PARTITION },
	{ offsetof (DDS_PublisherQos, group_data), QP_GROUP_DATA },
	{ offsetof (DDS_PublisherQos, entity_factory), QP_ENTITY_FACTORY }
};

static const Q_LFIELD qos_subscriber_list [] = {
	{ offsetof (DDS_SubscriberQos, presentation), QP_PRESENTATION },
	{ offsetof (DDS_SubscriberQos, partition), QP_PARTITION },
	{ offsetof (DDS_SubscriberQos, group_data), QP_GROUP_DATA },
	{ offsetof (DDS_SubscriberQos, entity_factory), QP_ENTITY_FACTORY }
};

typedef enum {
	QL_PARTICIPANT,
	QL_PUBLISHER,
	QL_SUBSCRIBER
} QLIST;

typedef struct {
	const Q_LFIELD	*fields;
	size_t		nfields;
} Q_LDESC;

static const Q_LDESC qos_ldescs [] = {
	{ qos_participant_list, sizeof (qos_participant_list) / sizeof (Q_LFIELD) },
	{ qos_publisher_list,   sizeof (qos_publisher_list) / sizeof (Q_LFIELD) },
	{ qos_subscriber_list,  sizeof (qos_subscriber_list) / sizeof (Q_LFIELD) }
};

/* Predefined defaults for the various QoS parameter sets: */
DDS_DomainParticipantQos qos_def_participant_qos;
DDS_TopicQos qos_def_topic_qos;
DDS_PublisherQos qos_def_publisher_qos;
DDS_SubscriberQos qos_def_subscriber_qos;
DDS_DataReaderQos qos_def_reader_qos;
DDS_DataWriterQos qos_def_writer_qos;
DiscoveredTopicQos qos_def_disc_topic_qos;
DiscoveredReaderQos qos_def_disc_reader_qos;
DiscoveredWriterQos qos_def_disc_writer_qos;

static void qos_init_default_list (QLIST l, void *dp)
{
	unsigned	f;
	const Q_LFIELD	*fp;
	const Q_DESC	*qdp;

	for (f = 0, fp = qos_ldescs [l].fields;
	     f < qos_ldescs [l].nfields;
	     f++, fp++) {
		qdp = qos_descs [fp->qos];
		memcpy ((char *) dp + fp->ofs, qdp->init, qdp->size);
	}
}

static void qos_init_default_group (QGROUP g, void *dp)
{
	char		*dst;
	QPAR		p;
	const Q_DESC	*qdp;

	for (p = 0; p < QP_MAX; p++) {
		qdp = qos_descs [p];
		if (qdp->offsets [g] != OFS_NU) {
			dst = (char *) dp + qdp->offsets [g];
			if (g >= QG_DISC_TOPIC &&
			    (p <= QP_GROUP_DATA || p == QP_PARTITION))
				memset (dst, 0, sizeof (void *));
			else
				memcpy (dst, qdp->init, qdp->size);
		}
	}
}

static void qos_init_defaults (void)
{
	QLIST	l;
	QGROUP	g;
	void	*list_qos [] = {
		&qos_def_participant_qos,
		&qos_def_publisher_qos,
		&qos_def_subscriber_qos
	};
	void	*group_qos [] = {
		&qos_def_topic_qos,
		&qos_def_writer_qos,
		&qos_def_reader_qos,
		&qos_def_disc_topic_qos,
		&qos_def_disc_writer_qos,
		&qos_def_disc_reader_qos
	};

	/* Default Partiticipant/Publisher/Subscriber QoS. */
	for (l = QL_PARTICIPANT; l <= QL_SUBSCRIBER; l++)
		qos_init_default_list (l, list_qos [l]);

	/* Default Topic/DataWriter/DataReader + Discovered entities QoS. */
	for (g = QG_TOPIC; g <= QG_DISC_READER; g++)
		qos_init_default_group (g, group_qos [g]);
		
}

static int qos_valid_qos_list (QLIST l, const void *qp)
{
	const Q_LFIELD	*fp;
	const Q_DESC	*qdp;
	QT_VALID_FCT	fct;
	unsigned	i;

	for (i = 0, fp = qos_ldescs [l].fields;
	     i < qos_ldescs [l].nfields;
	     i++, fp++) {
		qdp = qos_descs [fp->qos];
		fct = qos_type_ops [qdp->type]->valid_fct;
		if (fct && (*fct) ((const char *) qp + fp->ofs, 0))
			return (0);
	}
	return (1);
}

static int qos_valid_qos_group (const QGROUP g, const void *qp, int disc)
{
	QPAR		qos;
	QTYPE		type;
	QT_VALID_FCT	fct;
	int		ofs;

	ARG_NOT_USED (disc)

	for (qos = QP_MIN; qos < QP_MAX; qos++) {
		ofs = qos_descs [qos]->offsets [g];
		if (ofs == OFS_NU)
			continue;

		type = qos_descs [qos]->type;
		fct = qos_type_ops [type]->valid_fct;
		if (fct && (*fct) ((const char *) qp + ofs, 0))
			return (0);
	}
	return (1);
}

int qos_valid_participant_qos (const DDS_DomainParticipantQos *qos)
{
	return (qos_valid_qos_list (QL_PARTICIPANT, qos));
}

int qos_valid_publisher_qos (const DDS_PublisherQos *qos)
{
	return (qos_valid_qos_list (QL_PUBLISHER, qos));
}

int qos_valid_subscriber_qos (const DDS_SubscriberQos *qos)
{
	return (qos_valid_qos_list (QL_SUBSCRIBER, qos));
}

int qos_valid_topic_qos (const DDS_TopicQos *qos)
{
	if (qos->history.kind == DDS_KEEP_LAST_HISTORY_QOS &&
	    (unsigned) qos->history.depth >
	    (unsigned) qos->resource_limits.max_samples_per_instance)
		return (0);

	return (qos_valid_qos_group (QG_TOPIC, qos, 0));
}

int qos_valid_writer_qos (const DDS_DataWriterQos *qos)
{
	if (qos->history.kind == DDS_KEEP_LAST_HISTORY_QOS &&
	    (unsigned) qos->history.depth >
	    (unsigned) qos->resource_limits.max_samples_per_instance)
		return (0);

	return (qos_valid_qos_group (QG_WRITER, qos, 0));
}

int qos_valid_reader_qos (const DDS_DataReaderQos *qos)
{
	if (qos->history.kind == DDS_KEEP_LAST_HISTORY_QOS &&
	    (unsigned) qos->history.depth >
	     (unsigned) qos->resource_limits.max_samples_per_instance)
		return (0);

	return (qos_valid_qos_group (QG_READER, qos, 0));
}

/* qos_pool_init -- Setup a pool of QoS entries for the respective minimum/extra
	            amount of Qos nodes and node references. */

int qos_pool_init (const POOL_LIMITS *qosrefs, const POOL_LIMITS *qosdata)
{
	/* Check if already initialized. */
	if (mem_blocks [0].md_addr) {	/* Was already initialized -- reset. */
		mds_reset (mem_blocks, MB_END);
		memset (qos_ht, 0, sizeof (qos_ht));
		return (0);
	}

	/* Define the different pool attributes. */
	MDS_POOL_TYPE (mem_blocks, MB_QOS_REF, *qosrefs, sizeof (QosRef_t));
	MDS_POOL_TYPE (mem_blocks, MB_QOS_DATA, *qosdata, sizeof (Qos_t));

	/* All pools defined: allocate one big chunk of data that will be split in
	   separate pools. */
	mem_size = mds_alloc (mem_blocks, mem_names, MB_END);
#ifndef FORCE_MALLOC
	if (!mem_size) {
		fatal_printf ("qos_init: not enough memory available!\r\n");
		return (1);
	}
	log_printf (QOS_ID, 0, "qos_init: %lu bytes allocated for pools.\r\n", (unsigned long) mem_size);
#endif

	/* Setup defaults. */
	qos_init_defaults ();

	return (0);
}

/* qos_pool_free -- Free all QoS pools. */

void qos_pool_free (void)
{
	mds_free (mem_blocks, MB_END);
	memset (qos_ht, 0, sizeof (qos_ht));
}


/* Group QoS functionality as used for Publisher and Subscriber QoS.
   ----------------------------------------------------------------- */

static UniQos_t	temp_uq;

/* qos_group_new -- Initialize Group QoS parameters for the given group. */

static int qos_group_new (GroupQos_t *gp, void *qp, QLIST group)
{
	unsigned	f;
	const Q_LFIELD	*fp;
	QTYPE		type;
	QT_TO_UNI_FCT	fct;

	for (f = 0, fp = qos_ldescs [group].fields;
	     f < qos_ldescs [group].nfields;
	     f++, fp++) {
		type = qos_descs [fp->qos]->type;
		fct = qos_type_ops [type]->set_fct;
		(*fct) ((char *) qp + fp->ofs, &temp_uq,
			qos_descs [fp->qos]->uni_ofs, 0);
	}
	gp->group_data = temp_uq.group_data;
	gp->partition = temp_uq.partition;
	gp->presentation_access_scope = temp_uq.presentation_access_scope;
	gp->presentation_ordered_access = temp_uq.presentation_ordered_access;
	gp->presentation_coherent_access = temp_uq.presentation_coherent_access;
	gp->no_autoenable = temp_uq.no_autodispose;
	return (DDS_RETCODE_OK);
}

/* qos_group_update -- Update Group QoS parameters for the given group. */

static int qos_group_update (GroupQos_t *gp, void *qp, QLIST group)
{
	unsigned	f;
	const Q_LFIELD	*fp;
	QTYPE		type;
	QT_TO_UNI_FCT	fct;

	temp_uq.group_data = gp->group_data;
	temp_uq.partition = gp->partition;
	for (f = 0, fp = qos_ldescs [group].fields;
	     f < qos_ldescs [group].nfields;
	     f++, fp++) {
		type = qos_descs [fp->qos]->type;
		fct = qos_type_ops [type]->update_fct;
		(*fct) ((char *) qp + fp->ofs, &temp_uq,
			qos_descs [fp->qos]->uni_ofs, 0);
	}
	gp->group_data = temp_uq.group_data;
	gp->partition = temp_uq.partition;
	gp->presentation_access_scope = temp_uq.presentation_access_scope;
	gp->presentation_ordered_access = temp_uq.presentation_ordered_access;
	gp->presentation_coherent_access = temp_uq.presentation_coherent_access;
	gp->no_autoenable = temp_uq.no_autodispose;
	return (DDS_RETCODE_OK);
}

/* qos_group_get -- Get Group QoS parameters. */

static void qos_group_get (GroupQos_t *gp, void *qp, QLIST group)
{
	unsigned	f;
	const Q_LFIELD	*fp;
	QTYPE		type;
	QT_FROM_UNI_FCT	fct;

	temp_uq.group_data = gp->group_data;
	temp_uq.partition = gp->partition;
	temp_uq.presentation_access_scope = gp->presentation_access_scope;
	temp_uq.presentation_ordered_access = gp->presentation_ordered_access;
	temp_uq.presentation_coherent_access = gp->presentation_coherent_access;
	temp_uq.no_autodispose = gp->no_autoenable;
	for (f = 0, fp = qos_ldescs [group].fields;
	     f < qos_ldescs [group].nfields;
	     f++, fp++) {
		type = qos_descs [fp->qos]->type;
		fct = qos_type_ops [type]->get_fct;
		(*fct) (&temp_uq, qos_descs [fp->qos]->uni_ofs,
			(char *) qp + fp->ofs, 0);
	}
}

/* qos_group_free -- Free dynamic group QoS data. */

static void qos_group_free (GroupQos_t *gp)
{
	if (gp->group_data) {
		str_unref (gp->group_data);
		gp->group_data = NULL;
	}
	if (gp->partition) {
		partition_free (gp->partition);
		gp->partition = NULL;
	}
}

/* qos_hash -- Calculate the hash key of a QoS data structure. */

#define qos_hash	hashf

/* qos_lookup -- Lookup a QoS data structure in the hash table. */

static Qos_t *qos_lookup (unsigned       h,
			  const UniQos_t *dp)
{
	QosRef_t	*p;

	for (p = qos_ht [h]; p; p = p->next)
		if (!memcmp (&p->data->qos, dp, sizeof (UniQos_t)))
			return (p->data);

	return (NULL);
}

/* qos_set -- Set a QoS dataset. */

static void qos_set (UniQos_t *dp, void *sp, QGROUP g)
{
	char		*qdp;
	QT_TO_UNI_FCT	fct;
	QTYPE		type;
	QPAR		qos;
	int		ofs;

	memset (dp, 0, sizeof (UniQos_t));
	for (qos = QP_MIN; qos < QP_MAX; qos++) {
		ofs = qos_descs [qos]->offsets [g];
		if (ofs == OFS_NU ||
		    qos == QP_TIME_BASED_FILTER ||
		    qos == QP_READER_DATA_LIFECYCLE) {
			if (qos_descs [qos]->uni_ofs == OFS_NU)
				continue;

			else if (g >= QG_DISC_TOPIC)
				qdp = (char *) qos_descs [qos]->disc_init;
			else
				qdp = (char *) qos_descs [qos]->init;
		}
		else
			qdp = (char *) sp + ofs;
		type = qos_descs [qos]->type;
		fct = qos_type_ops [type]->set_fct;
		if (fct)
			(*fct) (qdp, dp, qos_descs [qos]->uni_ofs, g >= QG_DISC_TOPIC);
	}
}

/* qos_add -- Add a new QoS dataset to the QoS hash table. */

Qos_t *qos_add (const UniQos_t *dp)
{
	unsigned	h;
	Qos_t		*qp;
	QosRef_t	*rp;

	h = qos_hash ((const unsigned char *) dp, sizeof (UniQos_t)) % MAX_HASH;
	if ((qp = qos_lookup (h, dp)) != NULL) {
		qp->users++;
		return (qp);
	}
	if ((qp = mds_pool_alloc (&mem_blocks [MB_QOS_DATA])) == NULL) {
		warn_printf ("qos_new: out of memory!");
		return (NULL);
	}
	qp->users = 1;
	qp->qos = *dp;
	if ((rp = mds_pool_alloc (&mem_blocks [MB_QOS_REF])) != NULL) {
		rp->data = qp;
		rp->next = qos_ht [h];
		qos_ht [h] = rp;
	}

	/* else --> we just don't do lookups. */

	return (qp);
}

/* qos_new -- Create a new QoS dataset. */

static Qos_t *qos_new (void *sp, QGROUP g)
{
	UniQos_t	dp;

	qos_set (&dp, sp, g);
	return (qos_add (&dp));
}

/* qos_update -- Update a QoS dataset. */

static int qos_update (Qos_t **qpp, void *sp, QGROUP g)
{
	unsigned	prev_h, new_h;
	int		ofs;
	QosRef_t	*rp, *prev;
	UniQos_t	*dp;
	QT_TO_UNI_FCT	fct;
	QTYPE		type;
	QPAR		qos;
	Qos_t		*qp, *nqp;

	qp = *qpp;
	if (!qp || qp->users > 1) {
		if (qp)
			qp->users--;
		nqp = qos_new (sp, g);
		if (!nqp) {
			if (qp)
				qp->users++;
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
		else
			*qpp = nqp;
		return (DDS_RETCODE_OK);
	}
	dp = &qp->qos;
	prev_h = qos_hash ((unsigned char *) dp, sizeof (UniQos_t)) % MAX_HASH;
	for (qos = QP_MIN; qos < QP_MAX; qos++) {
		ofs = qos_descs [qos]->offsets [g];
		if (ofs == OFS_NU ||
		    qos == QP_TIME_BASED_FILTER ||
		    qos == QP_READER_DATA_LIFECYCLE)
			continue;

		type = qos_descs [qos]->type;
		fct = qos_type_ops [type]->update_fct;
		(*fct) ((char *) sp + ofs, dp,
			qos_descs [qos]->uni_ofs,
			g >= QG_DISC_TOPIC);
	}
	new_h = qos_hash ((unsigned char *) &qp->qos, sizeof (UniQos_t)) % MAX_HASH;
	if (prev_h == new_h)
		return (DDS_RETCODE_OK);

	for (rp = qos_ht [prev_h], prev = NULL;
	     rp && rp->data != qp;
	     prev = rp, rp = rp->next)
		;
	
	if (rp) {
		if (prev)
			prev->next = rp->next;
		else
			qos_ht [prev_h] = rp->next;
	}
	else {
		rp = mds_pool_alloc (&mem_blocks [MB_QOS_REF]);
		if (!rp)
			return (DDS_RETCODE_OUT_OF_RESOURCES);

		rp->data = qp;
	}
	rp->next = qos_ht [new_h];
	qos_ht [new_h] = rp;

	return (DDS_RETCODE_OK);
}

/* qos_get -- Get a QoS data set. */

static void qos_get (Qos_t *rp, void *dp, QGROUP g)
{
	int		ofs;
	UniQos_t	*sp;
	QT_FROM_UNI_FCT	fct;
	QTYPE		type;
	QPAR		qos;

	sp = &rp->qos;
	for (qos = QP_MIN; qos < QP_MAX; qos++) {
		ofs = qos_descs [qos]->offsets [g];
		if (ofs == OFS_NU ||
		    qos == QP_TIME_BASED_FILTER ||
		    qos == QP_READER_DATA_LIFECYCLE)
			continue;

		type = qos_descs [qos]->type;
		fct = qos_type_ops [type]->get_fct;
		(*fct) (sp, qos_descs [qos]->uni_ofs,
			(char *) dp + ofs,
			g >= QG_DISC_TOPIC);
	}
}


/* Publisher QoS.
   -------------- */

/* qos_publisher_new -- Create Publisher QoS parameters from the given data in
			*qp. */

int qos_publisher_new (GroupQos_t *gp, const DDS_PublisherQos *qp)
{
	return (qos_group_new (gp, (void *) qp, QL_PUBLISHER));
}

/* qos_publisher_update -- Update a Publisher QoS reference with new values. If
			   updating is not possible, an error value is returned. */

int qos_publisher_update (GroupQos_t *gp, const DDS_PublisherQos *qp)
{
	pl_cache_reset ();
	return (qos_group_update (gp, (void *) qp, QL_PUBLISHER));
}

/* qos_publisher_get -- Get Publisher QoS data. */

void qos_publisher_get (GroupQos_t *gp, DDS_PublisherQos *qp)
{
	qos_group_get (gp, (void *) qp, QL_PUBLISHER);
}

/* qos_publisher_free -- Release the allocated Publisher QoS data. */

void qos_publisher_free (GroupQos_t *gp)
{
	qos_group_free (gp);
}


/* Subscriber QoS.
   -------------- */

/* qos_subscriber_new -- Create Subscriber QoS parameters from the given data 
			 in *qp. */

int qos_subscriber_new (GroupQos_t *gp, const DDS_SubscriberQos *qp)
{
	return (qos_group_new (gp, (void *) qp, QL_SUBSCRIBER));
}

/* qos_subscriber_update -- Update a Subscriber QoS reference with new values.
			    If updating is not possible, an error value is
			    returned. */

int qos_subscriber_update (GroupQos_t *gp, const DDS_SubscriberQos *qp)
{
	pl_cache_reset ();
	return (qos_group_update (gp, (void *) qp, QL_SUBSCRIBER));
}

/* qos_subscriber_get -- Get Subscriber QoS data. */

void qos_subscriber_get (GroupQos_t *gp, DDS_SubscriberQos *qp)
{
	qos_group_get (gp, (void *) qp, QL_SUBSCRIBER);
}

/* qos_subscriber_free -- Release the allocated Subscriber QoS data. */

void qos_subscriber_free (GroupQos_t *gp)
{
	qos_group_free (gp);
}


/* Topic QoS.
   ---------- */

/* qos_topic_new -- Get a new Topic QoS reference. */

Qos_t *qos_topic_new (const DDS_TopicQos *qp)
{
	return (qos_new ((void *) qp, QG_TOPIC));
}

/* qos_topic_update -- Update an existing Topic QoS reference. */

int qos_topic_update (Qos_t **rp, const DDS_TopicQos *qp)
{
	pl_cache_reset ();
	return (qos_update (rp, (void *) qp, QG_TOPIC));
}

/* qos_topic_get -- Get a Topic QoS reference. */

void qos_topic_get (Qos_t *rp, DDS_TopicQos *qp)
{
	qos_get (rp, qp, QG_TOPIC);
}


/* Writer QoS.
   ----------- */

/* qos_writer_new -- Get a new Writer QoS reference. */

Qos_t *qos_writer_new (const DDS_DataWriterQos *qp)
{
	return (qos_new ((void *) qp, QG_WRITER));
}

/* qos_writer_update -- Update an existing Writer QoS reference. */

int qos_writer_update (Qos_t **rp, const DDS_DataWriterQos *qp)
{
	pl_cache_reset ();
	return (qos_update (rp, (void *) qp, QG_WRITER));
}

/* qos_writer_get -- Get a Writer QoS reference. */

void qos_writer_get (Qos_t *rp, DDS_DataWriterQos *qp)
{
	qos_get (rp, qp, QG_WRITER);
}


/* Reader QoS.
   ----------- */

/* qos_reader_new -- Get a new Reader QoS reference. */

Qos_t *qos_reader_new (const DDS_DataReaderQos *qp)
{
	return (qos_new ((void *) qp, QG_READER));
}

/* qos_reader_update -- Update an existing Reader QoS reference. */

int qos_reader_update (Qos_t **rp, const DDS_DataReaderQos *qp)
{
	pl_cache_reset ();
	return (qos_update (rp, (void *) qp, QG_READER));
}

/* qos_reader_get -- Get a Reader QoS reference. */

void qos_reader_get (Qos_t *rp, DDS_DataReaderQos *qp)
{
	qos_get (rp, qp, QG_READER);
}

/* qos_init_time_based_filter -- Initialise time-based-filter QoS. */

void qos_init_time_based_filter (DDS_TimeBasedFilterQosPolicy *qp)
{
	*qp = qos_time_based_filter_init;
}

/* qos_init_reader_data_lifecycle -- Initialise reader data lifecycle QoS. */

void qos_init_reader_data_lifecycle (DDS_ReaderDataLifecycleQosPolicy *qp)
{
	*qp = qos_reader_data_lifecycle_init;
}


/* Discovered Topic QoS.
   --------------------- */

/* qos_disc_topic_new -- Get a new Discovered Topic QoS reference. */

Qos_t *qos_disc_topic_new (DiscoveredTopicQos *qp)
{
	return (qos_new (qp, QG_DISC_TOPIC));
}

/* qos_disc_topic_update -- Update an existing Discovered Topic QoS reference. */

int qos_disc_topic_update (Qos_t **rp, DiscoveredTopicQos *qp)
{
	pl_cache_reset ();
	return (qos_update (rp, qp, QG_DISC_TOPIC));
}

/* qos_disc_topic_get -- Get a Discovered Topic QoS reference. */

void qos_disc_topic_get (Qos_t *rp, DiscoveredTopicQos *qp)
{
	qos_get (rp, qp, QG_DISC_TOPIC);
}


/* Discovered Writer QoS.
   ---------------------- */

/* qos_disc_writer_set -- Set a new Discovered Writer QoS dataset. */

void qos_disc_writer_set (UniQos_t *up, DiscoveredWriterQos *qp)
{
	pl_cache_reset ();
	qos_set (up, qp, QG_DISC_WRITER);
}

/* qos_disc_writer_restore -- Restore Discovered Writer QoS dataset after
                              qos_disc_writer_set. */

void qos_disc_writer_restore (DiscoveredWriterQos *qp, UniQos_t *up)
{
	qp->partition = up->partition;
	qp->user_data = up->user_data;
	qp->topic_data = up->topic_data;
	qp->group_data = up->group_data;

	up->partition = NULL;
	up->user_data = NULL;
	up->topic_data = NULL;
	up->group_data = NULL;
}

/* qos_disc_writer_update -- Update an existing Discovered Writer QoS reference. */

int qos_disc_writer_update (Qos_t **rp, DiscoveredWriterQos *qp)
{
	pl_cache_reset ();
	return (qos_update (rp, qp, QG_DISC_WRITER));
}

/* qos_disc_writer_get -- Get a Discovered Writer QoS reference. */

void qos_disc_writer_get (Qos_t *rp, DiscoveredWriterQos *qp)
{
	qos_get (rp, qp, QG_DISC_WRITER);
}


/* Discovered Reader QoS.
   ---------------------- */

/* qos_disc_reader_set -- Set a new Discovered Reader QoS dataset. */

void qos_disc_reader_set (UniQos_t *up, DiscoveredReaderQos *qp)
{
	pl_cache_reset ();
	qos_set (up, qp, QG_DISC_READER);
}

/* qos_disc_reader_restore -- Restore Discovered Reader QoS dataset after
                              qos_disc_reader_set. */

void qos_disc_reader_restore (DiscoveredReaderQos *qp, UniQos_t *up)
{
	qp->partition = up->partition;
	qp->user_data = up->user_data;
	qp->topic_data = up->topic_data;
	qp->group_data = up->group_data;

	up->partition = NULL;
	up->user_data = NULL;
	up->topic_data = NULL;
	up->group_data = NULL;
}

/* qos_disc_reader_update -- Update an existing Discovered Reader QoS reference. */

int qos_disc_reader_update (Qos_t **rp, DiscoveredReaderQos *qp)
{
	pl_cache_reset ();
	return (qos_update (rp, qp, QG_DISC_READER));
}

/* qos_disc_reader_get -- Get a Discovered Reader QoS reference. */

void qos_disc_reader_get (Qos_t *rp, DiscoveredReaderQos *qp)
{
	qos_get (rp, qp, QG_DISC_READER);
}


/* QoS matching.
   ------------- */

/* qos_match -- Returns a non-0 result if the Writer/Reader QoS values are
		compatible. */

int qos_match (const UniQos_t *wp, const GroupQos_t *wgp,
	       const UniQos_t *rp, const GroupQos_t *rgp,
	       DDS_QOS_POLICY_ID *qid)
{
	QT_MATCH_FCT	fct;
	const Q_DESC	*qdp;
	QPAR		qos;
	unsigned	w_scope, r_scope, w_coherent, r_coherent, w_ordered, r_ordered;

	for (qos = QP_MIN; qos < QP_MAX; qos++, qdp++) {
		qdp = qos_descs [qos];
		if (!qdp->check)
			continue;

		if (qdp->in_parent && qdp->type == QT_PRESENTATION) {
			if (wgp) {
				w_scope    = wgp->presentation_access_scope;
				w_coherent = wgp->presentation_coherent_access;
				w_ordered  = wgp->presentation_ordered_access;
			}
			else {
				w_scope    = wp->presentation_access_scope;
				w_coherent = wp->presentation_coherent_access;
				w_ordered  = wp->presentation_ordered_access;
			}
			if (rgp) {
				r_scope    = rgp->presentation_access_scope;
				r_coherent = rgp->presentation_coherent_access;
				r_ordered  = rgp->presentation_ordered_access;
			}
			else {
				r_scope    = rp->presentation_access_scope;
				r_coherent = rp->presentation_coherent_access;
				r_ordered  = rp->presentation_ordered_access;
			}
			if (!qos_presentation_match (w_scope,    r_scope,
						     w_coherent, r_coherent,
						     w_ordered,  r_ordered)) {
				if (qid)
					*qid = DDS_PRESENTATION_QOS_POLICY_ID;
				return (0);
			}
		}
		fct = qos_type_ops [qdp->type]->match_fct;
		if (!(*fct) (wp, rp, qos_descs [qos]->uni_ofs)) {
			if (qid)
				*qid = qos_descs [qos]->policy_id;
			return (0);
		}
	}
	return (1);
}

/* wildcards -- Check if a string contains wildcard characters. */

static int wildcards (const char *sp)
{
	int	bslash;

	for (bslash = 0; *sp; sp++) {
		if (*sp == '\\') {
			bslash = 1;
			continue;
		}
		else if (!bslash &&
			 (*sp == '[' || *sp == '?' || *sp == '*'))
			return (1);
		else
			bslash = 0;
	}
	return (0);
}

#define	DEF_NPARTITIONS	16

static const char *sbuf [DEF_NPARTITIONS];
static char wbuf [DEF_NPARTITIONS];

/* qos_same_partition -- Returns a non-0 result if Writer and Reader are in the
			 same partition. */

int qos_same_partition (Strings_t *wp, Strings_t *rp)
{
	const char	**ws, **rs, **ps;
	char		*ww, *rw, *pw;
	String_t	*s;
	unsigned	i, j, wlen, rlen, wn, rn;
	int		wc_src, wc_dst, res;
	static const char e = '\0';
	static const char *empty = &e;
	static char	w = 0;

	/* Match if both sets are empty. */
	if (!wp && !rp)
		return (1);

	/* Check Publisher partitions: set wlen and wn. */
	if (!wp || !wp->_length) {
		wlen = 1;
		ws = &empty;
		ww = &w;
		wn = 0;
	}
	else
		wlen = wn = wp->_length;

	/* Check Subscriber partitions: set rlen and rn. */
	if (!rp || !rp->_length) {
		rlen = 1;
		rs = &empty;
		rw = &w;
		rn = 0;
	}
	else
		rlen = rn = rp->_length;

	/* If the total number of partition strings exceeds the sbuf capacity,
	   then allocate temporary buffers for storing string/flag pointers. */
	if (wn + rn > DEF_NPARTITIONS) {
		ps = xmalloc (sizeof (char *) * (wn + rn));
		if (!ps)
			return (0);

		pw = xmalloc (sizeof (char) * (wn + rn));
		if (!pw) {
			xfree ((char *) ps);
			return (0);
		}
	}
	else { /* Use sbuf/wbuf since they are big enough. */
		ps = sbuf;
		pw = wbuf;
	}

	/* Set ws, ww, rs and rw now. */
	if (wn) {
		ws = ps;
		ww = pw;
	}
	if (rn) {
		rs = &ps [wn];
		rw = &pw [wn];
	}

	/* Populate the ws and and ww arrays with Publisher partition info. */
	for (i = 0; i < wn; i++) {
		s = wp->_buffer [i];
		if (s) {
			ws [i] = str_ptr (s);
			ww [i] = wildcards (ws [i]) > 0;
		}
		else {
			ws [i] = empty;
			ww [i] = 0;
		}
	}

	/* Populate the rs and and rw arrays with Subscriber partition info. */
	for (i = 0; i < rn; i++) {
		s = rp->_buffer [i];
		if (s) {
			rs [i] = str_ptr (s);
			rw [i] = wildcards (rs [i]) > 0;
		}
		else {
			rs [i] = empty;
			rw [i] = 0;
		}
	}

	/* Dump partitions: */
# if 0
	dbg_printf ("Partition check: P:("); 
	for (i = 0; i < wlen; i++) {
		if (i)
			dbg_printf (",");
		dbg_printf ("'%s'", ws [i]);
		if (ww [i])
			dbg_printf ("*");
	} 
	dbg_printf ("), S:("); 
	for (i = 0; i < rlen; i++) {
		if (i)
			dbg_printf (",");
		dbg_printf ("'%s'", rs [i]);
		if (rw [i])
			dbg_printf ("*");
	} 
	dbg_printf (") -> ");
# endif

	/* Finally, check if any Publisher partition matches any Subscriber
	   partition. */
	res = 1;
	for (i = 0; i < wlen; i++)
		for (j = 0; j < rlen; j++) {
			wc_src = ww [i];
			wc_dst = rw [j];
			if (wc_src && !wc_dst)
				res = nmatch (ws [i], rs [j], 0);
			else if (!wc_src && wc_dst)
				res = nmatch (rs [j], ws [i], 0);
			else if (!wc_src && !wc_dst)
				res = strcmp (ws [i], rs [j]);

			if (!res) {
# if 0
				dbg_printf ("'%s' and '%s' match!\r\n", ws [i], rs [j]);
# endif
				goto done;
			}
		}

    done:

	/* Matching result is in res now.  Cleanup the temporary buffers. */
	if (wn + rn > DEF_NPARTITIONS) {
		xfree ((char *) ps);
		xfree (pw);
	}
# if 0
	dbg_printf ("%s\r\n", (!res) ? "match!" : "no match.");
# endif
	return (!res);
}

/* qos_free -- Release allocated QoS data. */

void qos_free (Qos_t *qp)
{
	unsigned	h;
	QosRef_t	*p, *prev;

	if (!qp || --qp->users)
		return;

	h = qos_hash ((unsigned char *) &qp->qos, sizeof (UniQos_t)) % MAX_HASH;
	for (p = qos_ht [h], prev = NULL;
	     p && p->data != qp;
	     prev = p, p = p->next)
		;
	if (p) {
		if (prev)
			prev->next = p->next;
		else
			qos_ht [h] = p->next;
		mds_pool_free (&mem_blocks [MB_QOS_REF], p);
	}
	if (qp->qos.user_data) {
		str_unref (qp->qos.user_data);
		qp->qos.user_data = NULL;
	}
	if (qp->qos.topic_data) {
		str_unref (qp->qos.topic_data);
		qp->qos.topic_data = NULL;
	}
	if (qp->qos.group_data) {
		str_unref (qp->qos.group_data);
		qp->qos.group_data = NULL;
	}
	if (qp->qos.partition) {
		partition_free (qp->qos.partition);
		qp->qos.partition = NULL;
	}
	mds_pool_free (&mem_blocks [MB_QOS_DATA], qp);
}

/* DDS_qos_policy -- Return a QoS string corresponding with the QoS policy. */

const char *DDS_qos_policy (DDS_QosPolicyId_t policy_id)
{
	static const char *policy_id_str [] = {
		NULL,
		DDS_USERDATA_QOS_POLICY_NAME,
		DDS_DURABILITY_QOS_POLICY_NAME,
		DDS_PRESENTATION_QOS_POLICY_NAME,
		DDS_DEADLINE_QOS_POLICY_NAME,
		DDS_LATENCYBUDGET_QOS_POLICY_NAME,
		DDS_OWNERSHIP_QOS_POLICY_NAME,
		DDS_OWNERSHIPSTRENGTH_QOS_POLICY_NAME,
		DDS_LIVELINESS_QOS_POLICY_NAME,
		DDS_TIMEBASEDFILTER_QOS_POLICY_NAME,
		DDS_PARTITION_QOS_POLICY_NAME,
		DDS_RELIABILITY_QOS_POLICY_NAME,
		DDS_DESTINATIONORDER_QOS_POLICY_NAME,
		DDS_HISTORY_QOS_POLICY_NAME,
		DDS_RESOURCELIMITS_QOS_POLICY_NAME,
		DDS_ENTITYFACTORY_QOS_POLICY_NAME,
		DDS_WRITERDATALIFECYCLE_QOS_POLICY_NAME,
		DDS_READERDATALIFECYCLE_QOS_POLICY_NAME,
		DDS_TOPICDATA_QOS_POLICY_NAME,
		DDS_GROUPDATA_QOS_POLICY_NAME,
		DDS_TRANSPORTPRIORITY_QOS_POLICY_NAME,
		DDS_LIFESPAN_QOS_POLICY_NAME,
		DDS_DURABILITYSERVICE_POLICY_NAME
	};

	if (policy_id <= DDS_INVALID_QOS_POLICY_ID ||
	    policy_id > DDS_DURABILITYSERVICE_QOS_POLICY_ID)
		return (NULL);

	return (policy_id_str [policy_id]);
}

#ifdef DDS_DEBUG

/* qos_dump -- Dump all QoS data records. */

void qos_dump (void)
{
	QosRef_t	*p;
	unsigned	h, n;

	for (h = 0; h < MAX_HASH; h++)
		for (n = 0, p = qos_ht [h]; p; p = p->next, n++) {
			if (!n)
				dbg_printf ("\r\n%u: ", h);
			dbg_printf ("%p*%u ", (void *) p->data, p->data->users);
		}
	dbg_printf ("\r\n");
}

static void qos_participant_dump (Participant_t *pp)
{
	dbg_printf ("\tUser data:");
	qt_os_dump (pp->p_user_data);
	dbg_printf ("\r\n");
}

static void qos_group_inherited_dump (GroupQos_t *qp, int prefix)
{
	if (prefix)
		dbg_printf ("    G:");
	dbg_printf ("\tPARTITION: ");
	qt_partition_dump (qp->partition);
	dbg_printf ("\r\n\tGROUP_DATA: ");
	qt_os_dump (qp->group_data);
	dbg_printf ("\r\n");
}

static void qos_group_dump (GroupQos_t *qp)
{
	dbg_printf ("\tPRESENTATION: ");
	qt_presentation_dump (qp->presentation_access_scope,
			      qp->presentation_coherent_access,
			      qp->presentation_ordered_access);
	dbg_printf ("\r\n");
	qos_group_inherited_dump (qp, 0);
	dbg_printf ("\tENTITY_FACTORY: autoenable_created_entities=%d\r\n",
			!qp->no_autoenable);
}

static void qos_topic_inherited_dump (UniQos_t *qp)
{
	dbg_printf ("    T:\tTOPIC_DATA: ");
	qt_os_dump (qp->topic_data);
	dbg_printf ("\r\n");
}

static void qos_uni_dump (UniQos_t *qp, QGROUP g)
{
	QPAR		qos;
	QT_DISP_UNI_FCT	fct;
	int		ofs;
	QTYPE		type;

	for (qos = QP_MIN; qos < QP_MAX; qos++) {
		ofs = qos_descs [qos]->offsets [g];
		if (ofs == OFS_NU || qos == QP_TIME_BASED_FILTER) {
			/*if (qos_descs [qos]->uni_ofs == OFS_NU)*/
				continue;
		}
		type = qos_descs [qos]->type;
		fct = qos_type_ops [type]->disp_uni_fct;
		if (fct) {
			dbg_printf ("\t%s: ", qos_descs [qos]->name);
			if (qos == QP_WRITER_DATA_LIFECYCLE)
				dbg_printf ("autodispose_unregistered_instances=");
			(*fct) (qp, qos_descs [qos]->uni_ofs);
			dbg_printf ("\r\n");
		}
	}
}

/* qos_entity_dump -- Dump Entity QoS parameters. */

void qos_entity_dump (void *ep)
{
	Participant_t	*p;
	Topic_t		*tp;
	Publisher_t	*up;
	Subscriber_t	*sp;
	DiscoveredWriter_t *dwp;
	DiscoveredReader_t *drp;
	Writer_t	*wp;
	Reader_t	*rp;

	switch (((Entity_t *) ep)->type) {
		case ET_PARTICIPANT:
			p = (Participant_t *) ep;
			if (entity_discovered (p->p_entity.flags))
				dbg_printf ("Discovered ");
			dbg_printf ("Participant QoS:\r\n");
			qos_participant_dump (p);
			break;
		case ET_TOPIC:
			dbg_printf ("Topic QoS:\r\n");
			tp = (Topic_t *) ep;
			if (tp->qos)
				qos_uni_dump (&tp->qos->qos, QG_TOPIC);
			break;
		case ET_PUBLISHER:
			dbg_printf ("Publisher QoS:\r\n");
			up = (Publisher_t *) ep;
			qos_group_dump (&up->qos);
			break;
		case ET_SUBSCRIBER:
			dbg_printf ("Subscriber QoS:\r\n");
			sp = (Subscriber_t *) ep;
			qos_group_dump (&sp->qos);
			break;
		case ET_WRITER:
			if (entity_discovered (((Entity_t *) ep)->flags)) {
				dbg_printf ("Discovered Writer QoS:\r\n");
				dwp = (DiscoveredWriter_t *) ep;
				qos_uni_dump (&dwp->dw_qos->qos, QG_DISC_WRITER);
			}
			else {
				dbg_printf ("Writer QoS:\r\n");
				wp = (Writer_t *) ep;
				qos_uni_dump (&wp->w_qos->qos, QG_WRITER);
				qos_group_inherited_dump (&wp->w_publisher->qos, 1);
				if (wp->w_topic->qos)
					qos_topic_inherited_dump (&wp->w_topic->qos->qos);
			}
			break;
		case ET_READER:
			if (entity_discovered (((Entity_t *) ep)->flags)) {
				dbg_printf ("Discovered Reader QoS:\r\n");
				drp = (DiscoveredReader_t *) ep;
				qos_uni_dump (&drp->dr_qos->qos, QG_DISC_READER);
				dbg_printf ("\tTIME_BASED_FILTER: ");
				qt_time_filter_dump (&drp->dr_time_based_filter);
				dbg_printf ("\r\n");
			}
			else {
				dbg_printf ("Reader QoS:\r\n");
				rp = (Reader_t *) ep;
				qos_uni_dump (&rp->r_qos->qos, QG_READER);
				dbg_printf ("\tTIME_BASED_FILTER: ");
				qt_time_filter_dump (&rp->r_time_based_filter);
				dbg_printf ("\r\n");
				dbg_printf ("\tREADER_DATA_LIFECYCLE: ");
				qt_rd_lifecycle_dump (&rp->r_data_lifecycle);
				dbg_printf ("\r\n");
				qos_group_inherited_dump (&rp->r_subscriber->qos, 1);
				if (rp->r_topic->qos)
					qos_topic_inherited_dump (&rp->r_topic->qos->qos);
			}
			break;
		default:
			break;
	}
}

/* qos_pool_dump -- Dump all QoS pool statistics. */

void qos_pool_dump (size_t sizes [])
{
	print_pool_table (mem_blocks, (unsigned) MB_END, sizes);
}

#endif

