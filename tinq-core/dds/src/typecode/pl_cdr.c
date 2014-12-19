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

/* pl_cdr.c -- Implements the PL-CDR marshalling/unmarshalling functions. */

#include <stdio.h>
#include "error.h"
#include "prof.h"
#include "dds_data.h"
#include "pid.h"
#ifdef DDS_TYPECODE
#include "vtc.h"
#endif
#include "pl_cdr.h"

#ifdef PROFILE
PROF_PID (pl_m_size)
PROF_PID (pl_m)
PROF_PID (pl_um_size)
PROF_PID (pl_um)
PROF_PID (pl_k_size)
PROF_PID (pl_k_get)
#endif

static const Participant_t		*last_p;
static const Writer_t			*last_w;
static const Reader_t			*last_r;
static const Topic_t			*last_t;
static SPDPdiscoveredParticipantData	p_data;
static DiscoveredWriterData		w_data;
static DiscoveredReaderData		r_data;
static DiscoveredTopicData		t_data;

void pl_cache_reset (void)
{
	last_p = NULL;
	last_w = NULL;
	last_r = NULL;
	last_t = NULL;
#ifdef DDS_TYPECODE
	if (w_data.typecode) {
		xfree (w_data.typecode);
		w_data.typecode = NULL;
	}
	if (r_data.typecode) {
		xfree (r_data.typecode);
		r_data.typecode = NULL;
	}
#endif
}

static void pl_cache_participant (const Participant_t *p)
{
	p_data.proxy = p->p_proxy;
	p_data.user_data = p->p_user_data;
	p_data.entity_name = p->p_entity_name;
	p_data.lease_duration = p->p_domain->participant.p_lease_duration;
#ifdef DDS_SECURITY
	if (p->p_domain->security) {
		p_data.id_tokens = p->p_domain->participant.p_id_tokens;
		p_data.p_tokens = p->p_domain->participant.p_p_tokens;
	}
	else {
		p_data.id_tokens = NULL;
		p_data.p_tokens = NULL;
	}
#endif
	last_p = p;
}

static void pl_cache_publication (const Writer_t *w)
{
	w_data.proxy.guid.prefix = w->w_publisher->domain->participant.p_guid_prefix;
	w_data.proxy.guid.entity_id = w->w_entity_id;
	w_data.proxy.ucast = w->w_ucast;
	w_data.proxy.mcast = w->w_mcast;
	w_data.topic_name = w->w_topic->name;
	w_data.type_name = w->w_topic->type->type_name;
	qos_disc_writer_get (w->w_qos, &w_data.qos);
	w_data.qos.partition = w->w_publisher->qos.partition;
	w_data.qos.group_data = w->w_publisher->qos.group_data;
	w_data.qos.topic_data = w->w_topic->qos->qos.topic_data;
#ifdef DDS_TYPECODE
	if (w_data.typecode)
		xfree (w_data.typecode);
	if (w->w_topic->type->type_support)
		w_data.typecode = vtc_create (w->w_topic->type->type_support);
	else
		w_data.typecode = NULL;
	vendor_id_init (w_data.vendor_id);
#endif
	last_w = w;
}

static void pl_cache_subscription (const Reader_t *r)
{
	Topic_t		*tp;
	FilteredTopic_t	*ftp;

	r_data.proxy.guid.prefix = r->r_subscriber->domain->participant.p_guid_prefix;
	r_data.proxy.guid.entity_id = r->r_entity_id;
	r_data.proxy.exp_il_qos = (r->r_flags & EF_INLINE_QOS) != 0;
	r_data.proxy.ucast = r->r_ucast;
	r_data.proxy.mcast = r->r_mcast;
	tp = r->r_topic;
	if ((tp->entity.flags & EF_FILTERED) != 0) {
		ftp = (FilteredTopic_t *) tp;
		tp = ftp->related;
		r_data.filter = &ftp->data;
	}
	else
		r_data.filter = NULL;
	r_data.topic_name = tp->name;
	r_data.type_name = tp->type->type_name;
	qos_disc_reader_get (r->r_qos, &r_data.qos);
	r_data.qos.time_based_filter = r->r_time_based_filter;
	r_data.qos.partition = r->r_subscriber->qos.partition;
	r_data.qos.group_data = r->r_subscriber->qos.group_data;
	r_data.qos.topic_data = tp->qos->qos.topic_data;
#ifdef DDS_TYPECODE
	if (r_data.typecode)
		xfree (r_data.typecode);
	if (r->r_topic->type->type_support)
		r_data.typecode = vtc_create (r->r_topic->type->type_support);
	else
		r_data.typecode = NULL;
	vendor_id_init (r_data.vendor_id);
#endif
	last_r = r;
}


static ssize_t pl_participant_length (const Participant_t *p)
{
	if (p != last_p)
		pl_cache_participant (p);

	return (pid_participant_data_size (&p_data));
}

static ssize_t pl_publication_length (const Writer_t *w)
{
	if (w != last_w)
		pl_cache_publication (w);

	return (pid_writer_data_size (&w_data));
}


static ssize_t pl_subscription_length (const Reader_t *r)
{
	if (r != last_r)
		pl_cache_subscription (r);

	return (pid_reader_data_size (&r_data));
}


static ssize_t pl_topic_length (const Topic_t *t, int key)
{
	/* Special processing for Topic keys. */
	if (key)
		return (str_len (t->name) + str_len (t->type->type_name) + 10);

	if (t != last_t) {
		t_data.name = t->name;
		t_data.type_name = t->type->type_name;
		qos_disc_topic_get (t->qos, &t_data.qos);
	}
	return (pid_topic_data_size (&t_data));
}

/* pl_marshalled_size -- Parameter List mode marshalled data size retrieval.
			 The data argument is a pointer to unmarshalled data.
			 This can be either a handle to specific builtin data,
			 or a C/C++-structure containing the data. 
			 If key is set, only the key fields are processed and
			 the maximum sizes are returned. */

size_t pl_marshalled_size (const void           *data,
			   const PL_TypeSupport *ts,
			   int                  key,
			   DDS_ReturnCode_t     *error)
{
	DDS_HANDLE	h;
	Entity_t	*ep;
	unsigned	length;
	ssize_t		l;

	prof_start (pl_m_size);

	if (error)
		*error = DDS_RETCODE_OK;
	if (ts->builtin) {
		h = *((DDS_HANDLE *) data);
		ep = entity_ptr (h);
		if (!ep) {
			if (error)
				*error = DDS_RETCODE_ALREADY_DELETED;
			return (0);
		}
		switch (ts->type) {
			case BT_Participant:
				if (key)
					return (sizeof (GuidPrefix_t));

				l = pl_participant_length ((Participant_t *) ep);
				if (l <= 0)
					goto size_error;

				length = l;
				break;

			case BT_Publication:
				if (key)
					return (sizeof (GUID_t));

					l = pl_publication_length (
							  (Writer_t *) ep);
				if (l <= 0)
					goto size_error;

				length = l;
				break;

			case BT_Subscription:
				if (key)
					return (sizeof (GUID_t));

					l = pl_subscription_length (
							  (Reader_t *) ep);
				if (l <= 0)
					goto size_error;

				length = l;
				break;

			case BT_Topic:
				l = pl_topic_length ((Topic_t *) ep, key);
				if (l <= 0)
					goto size_error;

				length = l;
				break;

			default:
				goto size_error;
		}
	}
	else {

    size_error:
		length = 0;
		if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
	}

	prof_stop (pl_m_size, 1);
	return (length);
}

static ssize_t pl_participant_marshall (void *dp, const Participant_t *p)
{
	if (p != last_p)
		pl_cache_participant (p);

	return (pid_add_participant_data (dp, &p_data));
}

static ssize_t pl_publication_marshall (void *dp, const Writer_t *w)
{
	ssize_t	ss;

	if (w != last_w)
		pl_cache_publication (w);

	ss = pid_add_writer_data (dp, &w_data);
#ifdef DDS_TYPECODE
	if (w_data.typecode) {	/* Don't keep typecode lingering. */
		xfree (w_data.typecode);
		w_data.typecode = NULL;
	}
#endif
	return (ss);
}


static ssize_t pl_subscription_marshall (void *dp, const Reader_t *r)
{
	ssize_t	ss;

	if (r != last_r)
		pl_cache_subscription (r);

	ss = pid_add_reader_data (dp, &r_data);
#ifdef DDS_TYPECODE
	if (r_data.typecode) {	/* Don't keep typecode lingering. */
		xfree (r_data.typecode);
		r_data.typecode = NULL;
	}
#endif
	return (ss);
}


static ssize_t pl_topic_marshall (void *dst, const Topic_t *t, int key)
{
	unsigned char	*dp;
	ssize_t		n, ss;

	if (key) {
		dp = (unsigned char *) dst;
		n = pid_str_write (dp, t->name);
		dp += n;
		n = pid_str_write (dp, t->type->type_name);
		dp += n;
		return (dp - (unsigned char *) dst);
	}
	if (t != last_t) {
		t_data.name = t->name;
		t_data.type_name = t->type->type_name;
		qos_disc_topic_get (t->qos, &t_data.qos);
	}
	ss = pid_add_topic_data (dst, &t_data);
#ifdef DDS_TYPECODE
	if (t_data.typecode) {	/* Don't keep typecode lingering. */
		xfree (t_data.typecode);
		t_data.typecode = NULL;
	}
#endif
	return (ss);
}

/* pl_marshall -- Parameter List mode marshalling for C/C++. The dest argument
		  is a pointer to a buffer that is large enough to contain the
		  resulting marshalled data.  The data argument points is a
		  pointer to marshalled data.  This can be either a handle to
		  specific builtin data, or a C/C++-structure containing data.
		  If key is set, only the key fields are taken into account, and
		  maximum sized fields are used.  If swap is set, the marshalled
		  data will have its endianness swapped. */

DDS_ReturnCode_t pl_marshall (void                 *dest,
			      const void           *data,
			      const PL_TypeSupport *ts,
			      int                  key,
			      int                  swap)
{
	DDS_HANDLE	h;
	Entity_t	*ep;
	ssize_t		l;

	ARG_NOT_USED (swap)

	prof_start (pl_m);

	if (ts->builtin) {
		h = *((DDS_HANDLE *) data);
		ep = entity_ptr (h);
		if (!ep)
			return (DDS_RETCODE_ALREADY_DELETED);

		switch (ts->type) {
			case BT_Participant:
				if (key)
					memcpy (dest, data, sizeof (GuidPrefix_t));
				else {
					l = pl_participant_marshall (dest, (Participant_t *) ep);
					if (l <= 0)
						return (DDS_RETCODE_BAD_PARAMETER);
				}
				break;

			case BT_Publication:
				if (key)
					memcpy (dest, data, sizeof (GUID_t));
				else {
						l = pl_publication_marshall (dest, (Writer_t *) ep);
					if (l <= 0)
						return (DDS_RETCODE_BAD_PARAMETER);
				}
				break;

			case BT_Subscription:
				if (key)
					memcpy (dest, data, sizeof (GUID_t));
				else {
						l = pl_subscription_marshall (dest, (Reader_t *) ep);
					if (l <= 0)
						return (DDS_RETCODE_BAD_PARAMETER);
				}
				break;

			case BT_Topic:
				l = pl_topic_marshall (dest, (Topic_t *) ep, key);
				if (l <= 0)
					return (DDS_RETCODE_BAD_PARAMETER);

				break;

			default:
				return (DDS_RETCODE_BAD_PARAMETER);
		}
	}
	else
		return (DDS_RETCODE_UNSUPPORTED);

	prof_stop (pl_m, 1);
	return (DDS_RETCODE_OK);
}

/* pl_unmarshall -- Unmarshalling for C/C++ to host data endian mode.  The dest
		    argument is a buffer that will contain the marshalled data.
		    The data argument is the original marshalled data.  The swap
		    argument specifies whether the marshalled data endianness is
		    wrong, i.e. whether it needs to be swapped. */

DDS_ReturnCode_t pl_unmarshall (void                 *dest,
				DBW                  *data,
				const PL_TypeSupport *ts,
				int                  swap)
{
	PIDSet_t	pids [2];
	ssize_t		res;

	prof_start (pl_um);

	if (!ts->builtin)
		return (DDS_RETCODE_UNSUPPORTED);

	memset (pids, 0, sizeof (pids));
	switch (ts->type) {
		case BT_Participant:
			res = pid_parse_participant_data (data, dest, pids, swap);
			break;

		case BT_Publication:
			res = pid_parse_writer_data (data, dest, pids, swap);
			break;

		case BT_Subscription:
			res = pid_parse_reader_data (data, dest, pids, swap);
			break;

		case BT_Topic:
			res = pid_parse_topic_data (data, dest, pids, swap);
			break;

		default:
			res = -DDS_RETCODE_UNSUPPORTED;
			break;
	}
	if (res < 0)
		return (-res);

	prof_stop (pl_um, 1);
	return (DDS_RETCODE_OK);
}

/* pl_unmarshalled_size -- Return the C/C++ host data size of Parameter List
			   marshalled data.  The data argument is the source
			   marshalled data.  If swap is set, the data is in
			   non-native endian format. */

size_t pl_unmarshalled_size (const DBW            *data,
			     const PL_TypeSupport *ts,
			     DDS_ReturnCode_t     *error,
			     int                  swap)
{
	ARG_NOT_USED (data)
	ARG_NOT_USED (swap)

	prof_start (pl_um_size);

	if (!ts->builtin) {
		if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
		return (0);
	}
	switch (ts->type) {
		case BT_Participant:
			return (sizeof (SPDPdiscoveredParticipantData));

		case BT_Publication:
			return (sizeof (DiscoveredWriterData));

		case BT_Subscription:
			return (sizeof (DiscoveredReaderData));

		case BT_Topic:
			return (sizeof (DiscoveredTopicData));

		default:
			if (error)
				*error = DDS_RETCODE_BAD_PARAMETER;
			return (0);
	}

	prof_stop (pl_um_size, 1);
	return (0);
}

/* pl_key_size -- Return the fields size for Parameter List mode marshalled data.
		  The data argument is the original marshalled data.  If swap is
		  set, the original marshalled data endianness is wrong, i.e. it
		  needs to be swapped. */

size_t pl_key_size (DBW                   data,
		    const PL_TypeSupport  *ts,
		    int                   swap,
		    DDS_ReturnCode_t      *error)
{
	size_t	l;

	ARG_NOT_USED (data)
	ARG_NOT_USED (swap)

	prof_start (pl_k_size);

	if (!ts->builtin) {
		if (error)
			*error = DDS_RETCODE_UNSUPPORTED;
		return (0);
	}
	switch (ts->type) {
		case BT_Participant:
			l = pid_participant_key_size ();
			break;

		case BT_Publication:
			l = pid_writer_key_size ();
			break;

		case BT_Subscription:
			l = pid_reader_key_size ();
			break;

		case BT_Topic:
			l = pid_topic_key_size ();
			break;

		default:
			if (error)
				*error = DDS_RETCODE_BAD_PARAMETER;
			return (0);
	}

	prof_stop (pl_k_size, 1);
	return (l);
}

/* pl_key_fields -- Key fields extraction for Parameter List mode marshalled
		    data.  The data argument is the original marshalled data.
		    If swap is set, the original marshalled data endianness is
		    wrong, i.e. it needs to be swapped. */

DDS_ReturnCode_t pl_key_fields (void                 *dest,
				DBW                  *data,
				const PL_TypeSupport *ts,
				int                  swap)
{
	PIDSet_t	 pids [2];
	DDS_ReturnCode_t ret;

	ARG_NOT_USED (swap)

	prof_start (pl_k_get);

	if (!ts->builtin)
		return (DDS_RETCODE_UNSUPPORTED);

	memset (pids, 0, sizeof (pids));
	switch (ts->type) {
		case BT_Participant:
			ret = pid_parse_participant_key (*data, dest, swap);
			break;

		case BT_Publication:
			ret = pid_parse_writer_key (*data, dest, swap);
			break;

		case BT_Subscription:
			ret = pid_parse_reader_key (*data, dest, swap);
			break;

		case BT_Topic:
			ret = pid_parse_topic_key (*data, dest, swap);
			break;

		default:
			return (DDS_RETCODE_UNSUPPORTED);
	}

	prof_stop (pl_k_get, 1);
	return (ret);
}

void pl_init (void)
{
	PROF_INIT ("P:MSize", pl_m_size);
	PROF_INIT ("P:Marshall", pl_m);
	PROF_INIT ("P:UMSize", pl_um_size);
	PROF_INIT ("P:UMarshal", pl_um);
	PROF_INIT ("P:KeySize", pl_k_size);
	PROF_INIT ("P:KeyGet", pl_k_get);
}


