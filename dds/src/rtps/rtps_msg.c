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

/* rtps_msg.c -- Implements the RTPS message construction and parsing. */

#include "atomic.h"
#include "pool.h"
#include "log.h"
#include "error.h"
#include "ctrace.h"
#include "prof.h"
#include "list.h"
#include "pid.h"
#include "xcdr.h"
#include "dds.h"
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
#include "sec_crypto.h"
#endif
#include "rtps_cfg.h"
#include "rtps_data.h"
#include "rtps_priv.h"
#include "rtps_trace.h"
#include "rtps_mux.h"
#include "rtps_clist.h"
#ifdef RTPS_FRAGMENTS
#include "rtps_frag.h"
#endif
#include "rtps_slbr.h"
#include "rtps_msg.h"

/* rtps_msg_header_set -- Initialize the RTPS message header. */

static RMBUF *rtps_new_msg (Proxy_t *pp)
{
	RMBUF	*mp;

	mp = mds_pool_alloc (&rtps_mem_blocks [MB_MSG_BUF]);
	if (!mp)
		return (NULL);

	/* Initialize message header. */
	STATS_INC (pp->nmsg);
	protocol_set (mp->header.protocol);
	version_init (mp->header.version);
	vendor_id_init (mp->header.vendor_id);
	guid_prefix_cpy (mp->header.guid_prefix,
		pp->endpoint->u.publisher->domain->participant.p_guid_prefix);

	mp->first = mp->last = NULL;
	mp->size = 0;
	mp->prio = (pp->is_writer) ? pp->u.writer->prio : 0;
	mp->users = 1;
	if (pp->head)
		pp->tail->next = mp;
	else
		pp->head = mp;
	pp->tail = mp;
	pp->msg_time = 0;
	pp->id_prefix = 0;
	mp->next = NULL;
	return (mp);
}

/* new_proxy_element -- Allocate a new submessage element container. */

static RME *new_proxy_element (Proxy_t *pp, int new_message)
{
	RMBUF	*mp;
	RME	*mep;

	if (!pp->head || new_message ||
	    (pp->tail->last &&
	     (pp->tail->last->flags & RME_HEADER) != 0 &&
	     (pp->tail->last->header.id == ST_HEARTBEAT))) {
		mp = rtps_new_msg (pp);
		if (!mp)
			return (NULL);

		mep = &mp->element;
		mep->flags = RME_CONTAINED;
		mep->pad = 0;
	}
	else {
		mep = mds_pool_alloc (&rtps_mem_blocks [MB_MSG_ELEM_BUF]);
		if (!mep)
			return (NULL);

		mep->pad = 0;
		mep->flags = 0;
	}
	mep->next = NULL;
	mep->db = NULL;
	return (mep);
}

/* rtps_msg_add_info_destination -- Add an INFO_DEST submessage to an existing
				    message for a remote Reader/Writer. */

static int rtps_msg_add_info_destination (Proxy_t            *pp,
					  RME                *mep,
				          const GuidPrefix_t *prefix)
{
	InfoDestinationSMsg	*dp;

#ifndef RTPS_TRACE
	ARG_NOT_USED (pp)
#endif

	ctrc_printd (RTPS_ID, RTPS_TX_INFO_DEST, prefix, sizeof (GuidPrefix_t));

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_INFO_DST;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra INFO_DESTINATION submessage fields. */
	mep->data = mep->d;
	dp = (InfoDestinationSMsg *) mep->d;
	mep->length = mep->header.length = sizeof (InfoDestinationSMsg);
	dp->guid_prefix = *prefix;

	/* Trace INFO_DESTINATION frame if enabled. */
	TX_INFO_DST (&pp->u.writer->endpoint, dp);

	return (DDS_RETCODE_OK);
}

/* rtps_msg_time_add -- Add an INFO_TS submessage element. */

static void rtps_msg_time_add (Proxy_t *pp, RME *mep, FTime_t *time)
{
	InfoTimestampSMsg *tp;

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_INFO_TS;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra INFO_TS submessage fields. */
	mep->data = mep->d;
	tp = (InfoTimestampSMsg *) mep->d;
	mep->length = mep->header.length = sizeof (InfoTimestampSMsg);
	tp->seconds = FTIME_SEC (*time);
	tp->fraction = FTIME_FRACT (*time);
	pp->msg_time = 1;

	/* Trace INFO_TS frame if enabled. */
	TX_INFO_TS (pp->u.writer, tp, mep->header.flags);
}

#ifdef RTPS_COMBINE_MSGS
#define	RTPS_COMBINE	0
#else
#define	RTPS_COMBINE	1
#endif

static RME *rtps_msg_append_mep (RME *prev_mep)
{
	RME    		*mep;

	mep = mds_pool_alloc (&rtps_mem_blocks [MB_MSG_ELEM_BUF]);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_append_mep: no memory for submessage element.\r\n");
		return (NULL);
	}
	mep->flags = 0;
	mep->pad = 0;
	prev_mep->next = mep;
	mep->next = NULL;
	mep->db = NULL;
	return (mep);
}

static void add_proxy_elements (Proxy_t            *pp,
				RME                *mep,
				unsigned           size,
				int                ts_on_split,
				const GuidPrefix_t *prefix)
{
	RMBUF		*mp = pp->tail;
	RME		*new_mep, *last_mep, *ts_mep, *id_mep;
	FTime_t		ftime;

	if (pp->head &&
	    (!mp->size || mp->size + size < rtps_max_msg_size)) { /* Append. */

	    append_mep:
		if (!mp->first)
			mp->first = mep;
		else
			mp->last->next = mep;
		mp->last = mep;
		mp->size += size;
	}
	else if (ts_on_split || prefix) {
		new_mep = last_mep = new_proxy_element (pp, 1);
		if (!new_mep)
			goto append_mep; /* Just append to previous. */

		if (prefix) {
			id_mep = new_mep;
			rtps_msg_add_info_destination (pp, id_mep, prefix);
			if (ts_on_split) {
				ts_mep = rtps_msg_append_mep (mep);
				if (ts_mep)
					last_mep = ts_mep;
			}
			else
				ts_mep = NULL;
			size += sizeof (InfoDestinationSMsg);
		}
		else
			ts_mep = new_mep;
		if (ts_mep) {
			sys_getftime (&ftime);
			rtps_msg_time_add (pp, ts_mep, &ftime);
			size += sizeof (InfoTimestampSMsg);
		}
		mp = pp->tail;
		mp->first = new_mep;
		last_mep->next = mep;
		mp->last = mep;
		mp->size = size;
		if (!prefix)
			mp->element.flags |= RME_MCAST;
	}
	else { /* Can't add, new message buffer needed. */
		mp = rtps_new_msg (pp);
		if (!mp)
			fatal_printf ("RTPS: out of message headers!");

		if (mep->header.id == ST_DATA)
			log_printf (RTPS_ID, 0, "Missing INFO_TS!\r\n");
		mp->element = *mep;
		if (mep->data == mep->d)
			mp->element.data = mp->element.d;
		if (mep->next && (mep->next->flags & RME_CONTAINED) != 0)
			mp->element.next = (RME *) (mp->element.d + 
					((unsigned char *) mep->next - mep->d));
		mds_pool_free (&rtps_mem_blocks [MB_MSG_ELEM_BUF], mep);
		mep = &mp->element;
		mp->element.flags |= RME_CONTAINED;
		mp->element.pad = 0;
		mp->first = mp->last = &mp->element;
		mp->size = size;
	}
	if (mep->next) {
		do {
			mep = mep->next;
		}
		while (mep->next);
		mp->last = mep;
	}
}

static RME *rtps_msg_add_mep (RME *prev_mep, size_t dsize)
{
	RME    		*mep;

	if (dsize > MAX_ELEMENT_DATA) {
		warn_printf ("rtps_msg_add_mep: too much data requested!\r\n");
		return (NULL);
	}
	if (/*(prev_mep->flags & RME_CONTAINED) == 0 && <- First always is! */
	    ((prev_mep->length + WORDALIGN - 1) & ~(WORDALIGN - 1)) + 
	    RME_HDRSIZE + dsize <= MAX_ELEMENT_DATA) {
		mep = (RME *) &prev_mep->d [(prev_mep->length + WORDALIGN - 1) &
							      ~(WORDALIGN - 1)];
		mep->flags = RME_CONTAINED;
		mep->pad = 0;
	}
	else {	/* Doesn't fit :-(  Needs extra container! */
		mep = mds_pool_alloc (&rtps_mem_blocks [MB_MSG_ELEM_BUF]);
		if (!mep) {
			log_printf (RTPS_ID, 0, "rtps_msg_add_mep: no memory for submessage element.\r\n");
			return (NULL);
		}
		mep->flags = 0;
		mep->pad = 0;
	}
	prev_mep->next = mep;
	mep->next = NULL;
	return (mep);
}

#if 0
/* rtps_msg_add_info_ts -- Add an INFO_TS submessage element to an existing
			   message. */

static RME *rtps_msg_add_info_ts (RemReader_t *rrp, FTime_t *ftime)
{
	RME		*mep;

	ctrc_printd (RTPS_ID, RTPS_TX_INFO_TS, NULL, 0); 

	/* Get a new submessage element header. */
	mep = new_proxy_element (&rrp->proxy, 0/*RTPS_COMBINE*/);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_info_ts: no memory for submessage element.\r\n");
		return (NULL);
	}
	rtps_msg_time_add (&rrp->proxy, mep, ftime);
	return (mep);
}
#endif

/* rtps_msg_add_data -- Add either a DATA submessage element or a DATAFRAG
			submessage element to an existing message. */

int rtps_msg_add_data (RemReader_t        *rrp,
		       const GuidPrefix_t *prefix,
		       const EntityId_t   *eid,
		       Change_t           *cp,
		       const HCI          hci,
		       int                push
#ifdef RTPS_FRAGMENTS
		     , unsigned           first,
		       unsigned           *nfrags
#endif
			                         )
{
	RME			*mep, *data_mep, *first_mep;
	NOTIF_DATA		*np;
#ifdef RTPS_FRAGMENTS
	DataFragSMsg		*dfp = NULL;
#endif
	DataSMsg		*dp;
	const KeyHash_t		*hp;
	String_t		*kp;
	size_t			key_size, msize;
	ssize_t			n;
	StatusInfo_t		status;
	LocalEndpoint_t		*ep;
	DiscoveredReader_t	*drp;
	const TypeSupport_t	*ts;
	FTime_t			ftime, *ftimep;
	DDS_GUIDSeq		guid_seq;
	GUID_t			guids [MAX_DW_DESTS];
	unsigned		remain, ofs, dlen, clen, kh, fofs, nqospars = 0;
#ifdef RTPS_FRAGMENTS
	unsigned		f;
	size_t			tfsize;
#endif
	size_t			dsize;
	int			i, add_time_on_split = 0;
	Type			*tp;
#ifdef DDS_NATIVE_SECURITY
	DBW			dbw;
	unsigned char		*xp;
	DB			*ndbp, *kdbp;
	size_t			nlength;
	DDS_ReturnCode_t	ret;
	String_t		enc_key_str;
#ifdef MAX_KEY_BUFFER
	unsigned char		key_buffer [MAX_KEY_BUFFER];
#endif
#endif

	ep = rrp->rr_writer->endpoint.endpoint;

	ctrc_begind (RTPS_ID, RTPS_TX_DATA, &ep->ep.entity_id, sizeof (EntityId_t));
	ctrc_contd (&cp->c_seqnr, sizeof (SequenceNumber_t));
	ctrc_endd ();

	/* Get a new submessage element header. */
	first_mep = mep = new_proxy_element (&rrp->proxy, RTPS_COMBINE);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_data: no memory for first submessage element.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Add an InfoDestination element if this is not a multicast message. */
	if (prefix) {
		rtps_msg_add_info_destination (&rrp->proxy, mep, prefix);
		rrp->rr_id_prefix = 1;
		msize = mep->length + sizeof (SubmsgHeader);
		mep = rtps_msg_append_mep (first_mep);
		if (!mep)
			goto no_mem;
	}
	else {
		rrp->rr_id_prefix = 0;
		msize = 0;
	}

	/* Set timestamp if specified by user, or if not yet in message. */
	if (!FTIME_ZERO (cp->c_time)) {
		ftime = cp->c_time;
		ftimep = &ftime;
		add_time_on_split = 0;
	}
	else if (!rrp->proxy.msg_time) {
		sys_getftime (&ftime);
		ftimep = &ftime;
		add_time_on_split = 0;
	}
	else {
		add_time_on_split = 1;
		ftimep = NULL;
	}

	/* Add submessage element header for the timestamp. */
	if (ftimep) {
		rtps_msg_time_add (&rrp->proxy, mep, ftimep);
		msize += mep->length + sizeof (SubmsgHeader);
		mep = rtps_msg_append_mep (mep);
		if (!mep)
			goto no_mem;
	}
	data_mep = NULL;

	/* Check if key field present. */
	ts = ep->ep.topic->type->type_support;
	if (ts->ts_keys &&
	    !hc_inst_info (hci, &hp, &kp)) {
		if (kp && cp->c_kind != ALIVE) {
			key_size = str_len (kp);

#ifdef DDS_NATIVE_SECURITY
			if (ep->payload_prot) {
#ifdef MAX_KEY_BUFFER
				if (key_size + 4 < MAX_KEY_BUFFER) {
					xp = key_buffer;
					dbw.dbp = kdbp = NULL;
				}
				else
#endif
				     {

					/* Allocate a temporary key data DB. */
					kdbp = db_alloc_data (key_size + 4, 1);
					if (!kdbp) {
						str_unref (kp);
						kp = NULL;
						goto no_key_data;
					}
					dbw.dbp = kdbp;
					xp = kdbp->data;
				}
				dbw.data = xp;
				dbw.left = dbw.length = key_size + 4;
				*xp++ = 0;
				*xp++ = (MODE_CDR << 1) | ENDIAN_CPU;
				*xp++ = 0;
				*xp++ = 0;
				memcpy (xp, str_ptr (kp), key_size);
				str_unref (kp);

				/* Encrypt key fields. */
				ndbp = sec_encode_serialized_data (&dbw,
								   ep->crypto,
								   &nlength,
								   &ret);
				if (kdbp)
					db_free_data (kdbp);
				if (!ndbp) {
					log_printf (RTPS_ID, 0, "rtps_msg_add_data: no memory for encrypted key fields.\r\n");
					kp = NULL;
					goto no_key_data;
				}
				enc_key_str.length = key_size = nlength;
				enc_key_str.users = MAX_STR_REFS - 1;
				enc_key_str.mutable = 1;
				enc_key_str.dynamic = 0;
				enc_key_str.u.dp = (char *) ndbp->data;
				kp = &enc_key_str;
				kh = 0;
			}
			else
#endif
				kh = 4;
		}
		else {
			key_size = ts->ts_mkeysize;
			kh = 4;
		}
	}
	else {
		hp = NULL;
		kp = NULL;
		key_size = 0;
		kh = 0;
	}
	if (cp->c_kind == ALIVE)
		dsize = cp->c_length;
	else
		dsize = key_size + kh;
#ifdef RTPS_FRAGMENTS
	if (first)
		fofs = first * rtps_frag_size;
	else
		fofs = 0;
	f = first + 1;
	tfsize = rtps_max_msg_size;
	do {
		if (dsize - fofs > rtps_frag_size)
			clen = rtps_frag_size;
		else
			clen = dsize - fofs;
#else
		fofs = 0;
		clen = dsize;
#endif
		dlen = (clen + 3) & ~3;
#ifdef RTPS_FRAGMENTS

		/* Combine fragment with previous fragments if possible. */
		if (dsize > rtps_frag_size) {
			if (tfsize + clen <= rtps_max_msg_size) {
				dfp->num_fragments++;
				data_mep->length += dlen;
				tfsize += dlen;
				mep->header.length += dlen;
				msize += dlen;
				goto next_frag_chunk;
			}
			else if (dfp) {
				data_mep->pad = dlen - clen;
				TX_DATA_FRAG (rrp->rr_writer, dfp, mep->header.flags);
				STATS_INC (rrp->rr_ndatafrags);
				add_proxy_elements (&rrp->proxy,
						    first_mep,
						    msize,
						    add_time_on_split,
						    prefix);
				first_mep = NULL;
			}
			tfsize = dlen;
		}
#endif

		/* Get a new submessage element header. */
		if (!first_mep) {
			first_mep = mep = new_proxy_element (&rrp->proxy, RTPS_COMBINE);
			if (!mep) {
				log_printf (RTPS_ID, 0, "rtps_msg_add_data: no memory for first submessage element.\r\n");
				return (DDS_RETCODE_OUT_OF_RESOURCES);
			}
			msize = 0;
		}

		/* Setup submessage header. */
		mep->flags |= RME_HEADER;
#ifdef RTPS_FRAGMENTS
		if (dsize > rtps_frag_size)
			mep->header.id = ST_DATA_FRAG;
		else
#endif
			mep->header.id = ST_DATA;
		mep->header.flags = SMF_CPU_ENDIAN;
		if (!prefix)
			rrp->rr_tail->element.flags |= RME_MCAST;

		/* Add extra DATA submessage fields. */
		mep->data = mep->d;

#ifdef RTPS_FRAGMENTS
		if (dsize > rtps_frag_size) {
			dfp = (DataFragSMsg *) mep->d;
			mep->length = sizeof (DataFragSMsg);
			dfp->extra_flags = 0;
			dfp->inline_qos_ofs = sizeof (DataFragSMsg) - 4;
			dfp->reader_id = (eid) ? *eid : entity_id_unknown;
			dfp->writer_id = ep->ep.entity_id;
			dfp->writer_sn = cp->c_seqnr;
			dfp->frag_start = f;
			dfp->num_fragments = 1;
			dfp->frag_size = rtps_frag_size;
			dfp->sample_size = dsize;
			remain = MAX_ELEMENT_DATA - sizeof (DataFragSMsg);
			ofs = sizeof (DataFragSMsg);
		}
		else {
#endif
			dp = (DataSMsg *) mep->d;
			mep->length = sizeof (DataSMsg);
			dp->extra_flags = 0;
			dp->inline_qos_ofs = sizeof (DataSMsg) - 4;
			dp->reader_id = (eid) ? *eid : entity_id_unknown;
			dp->writer_id = ep->ep.entity_id;
			dp->writer_sn = cp->c_seqnr;
			remain = MAX_ELEMENT_DATA - sizeof (DataSMsg);
			ofs = sizeof (DataSMsg);
#ifdef RTPS_FRAGMENTS
		}
		if (!fofs) {
#endif
			/* Add some inline QoS parameters (HashKey, StatusInfo, DirectedWrite)
			   if necessary for correct operation.
			   Note: by design, there is always room for these PIDs, as well
		        	 as for the trailing SENTINEL PID so we don't check
			         how much room is still available. */

			/* 1. Add Hashkey if multi-instance. */
			if (hp) {
				n = pid_add (mep->d + ofs, PID_KEY_HASH, hp);
				if (n > 0) {
					ofs += n;
					remain -= n;
					nqospars++;
				}
			}

			/* 2. Add StatusInfo if != ALIVE. */
			if (cp->c_kind > ALIVE) {
				status.value [0] = status.value [1] = status.value [2] = 0;
				status.value [3] = cp->c_kind;
				n = pid_add (mep->d + ofs, PID_STATUS_INFO, &status);
				if (n > 0) {
					ofs += n;
					remain -= n;
					nqospars++;
				}
			}

			/* 3. Add DirectedWrite if destinations specified. */
			if (cp->c_dests [0]) {
				guid_seq._length = 0;
				guid_seq._maximum = MAX_DW_DESTS;
				guid_seq._esize = sizeof (GUID_t);
				guid_seq._own = 0;
				guid_seq._buffer = guids;
				for (i = 0, n = 0; cp->c_dests [i]; i++) {
					drp = (DiscoveredReader_t *) entity_ptr (cp->c_dests [i]);
					if (drp) {
						guids [n].prefix = drp->dr_participant->p_guid_prefix;
						guids [n++].entity_id = drp->dr_entity_id;
					}
				}
				if (n) {
					guid_seq._length = n;
					n = pid_add (mep->d + ofs, PID_DIRECTED_WRITE, &guid_seq);
					if (n > 0) {
						ofs += n;
						remain -= n;
						nqospars++;
					}
				}
			}

#ifdef DDS_INLINE_QOS
			if (rrp->rr_inline_qos) {

				/* ... add QoS data ... */

				ofs += QOS_DATA_LENGTH; /* -> sizeof (QoS data) */;
				remain -= QOS_DATA_LENGTH; /* -> sizeof (QoS data) */;
			}
#endif
			if (nqospars) {
				n = pid_finish (mep->d + ofs);
				if (n > 0) {
					ofs += n;
					remain -= n;
				}
				mep->header.flags |= SMF_INLINE_QOS;
			}
#ifdef RTPS_FRAGMENTS
		}
#endif
		mep->length = mep->header.length = ofs;
		msize += ofs + sizeof (SubmsgHeader);
		data_mep = NULL;
		if (cp->c_kind == ALIVE) {

			/* Add serializedPayload submessage element. */
#ifdef RTPS_FRAGMENTS
			if (dsize <= rtps_frag_size)
#endif
				mep->header.flags |= SMF_DATA;

			/* Check if change data fits directly in rest of buffer. */
			if (clen <= remain) { /* Fits! */

				/* Just copy change data. */
				memcpy (mep->d + ofs, cp->c_data + fofs, clen);
				mep->length += dlen;
				mep->pad = dlen - clen;
			}
			else {	/* Doesn't fit :-(  Needs extra container! */
				data_mep = rtps_msg_add_mep (mep, sizeof (NOTIF_DATA));
				if (!data_mep) {
					if (kp)
						str_unref (kp);
					kp = NULL;
					goto no_mem;
				}
				rcl_access (cp->c_db);
				cp->c_db->nrefs++;
				rcl_done (cp->c_db);
				data_mep->db = cp->c_db;
				data_mep->data = cp->c_data + fofs;
				data_mep->length = dlen;
				data_mep->pad = dlen - clen;

				/* Notification needed to clean up cache change
				   when message element is disposed. */
				data_mep->flags |= RME_NOTIFY;
				np = (NOTIF_DATA *) data_mep->d;
				np->type = NT_CACHE_FREE;
				rcl_access (cp);
				cp->c_nrefs++;
				rcl_done (cp);
				np->arg = cp;
			}
			mep->header.length += dlen;
			msize += dlen;
			if (kp) {
				str_unref (kp);
				kp = NULL;
			}
		}
		else {
			/* Add serializedKey submessage element. */
#ifdef RTPS_FRAGMENTS
			if (dsize > rtps_frag_size)
				mep->header.flags |= SMF_KEY_DF;
			else
#endif
				mep->header.flags |= SMF_KEY;

			/* Add marshalling header if needed. */
			if (!fofs && kh) {
				mep->length += 4;
				mep->header.length += 4;
				mep->d [ofs++] = 0;
				mep->d [ofs++] = (MODE_CDR << 1) | ENDIAN_CPU;
				mep->d [ofs++] = 0;
				mep->d [ofs++] = 0;
				remain -= 4;
				dlen -= 4;
				kh = 0;
			}

			/* Check if key data fits directly in rest of buffer. */
	 		if (dlen <= remain) { /* Fits! */
				tp= (ts->ts_prefer == MODE_CDR) ? ts->ts_cdr: ts->ts_pl->xtype;

				/* Just copy change data. */
				if (kp)
					memcpy (mep->d + ofs, str_ptr (kp) + fofs - kh, dlen);
				else if (!tp || (ENDIAN_CPU == ENDIAN_BIG && ts->ts_fksize))
					memcpy (mep->d + ofs, hp + fofs, dlen);
				else {
					cdr_key_fields (mep->d + ofs, 4, hp, 4 + fofs, tp,
							1, 1, ENDIAN_CPU ^ ENDIAN_BIG, 0);
					while (clen < dlen)
						mep->d [ofs + clen++] = 0;
				}
				mep->length += dlen;
			}
			else {	/* Doesn't fit :-(  Needs extra container! */
				data_mep = rtps_msg_append_mep (mep);
				if (!data_mep) {
					if (kp) {
						str_unref (kp);
						kp = NULL;
					}
					log_printf (RTPS_ID, 0, "rtps_msg_add_data: no memory for extra key element.\r\n");
					goto no_key_data;
				}
				if (!kp || (kp && dlen <= MAX_ELEMENT_DATA)) {
					data_mep->data = data_mep->d;
					memcpy (data_mep->data,
						(kp) ? str_ptr (kp) + fofs - kh : (char *) hp->hash + fofs,
						dlen);
					if (kp) {
						str_unref (kp);
						kp = NULL;
					}
				}
				else {
					np = (NOTIF_DATA *) data_mep->d;
					np->type = NT_STR_FREE;
					np->arg = kp;
					data_mep->data = (unsigned char *) str_ptr (str_ref (kp)) + fofs;
					data_mep->db = NULL;
				}
				data_mep->length = dlen;
			}
			mep->header.length += dlen;
			msize += dlen;
		}

	    no_key_data:

		/* Trace DATA/DATAFRAG frame if enabled. */
#ifdef RTPS_FRAGMENTS
		if (dsize <= rtps_frag_size) {
#endif
			TX_DATA (rrp->rr_writer, dp, mep->header.flags);
			STATS_INC (rrp->rr_ndata);

			/* Add submessages to message. */
			add_proxy_elements (&rrp->proxy, first_mep, msize,
						add_time_on_split, prefix);

#ifdef RTPS_FRAGMENTS
			break;
		}

	    next_frag_chunk:

		fofs += rtps_frag_size;
		f++;
	}
	while (fofs < dsize && (!nfrags || f - first <= *nfrags));
	if (nfrags)
		*nfrags = (dsize > rtps_frag_size) ? f - 1: 0;
	if (dsize > rtps_frag_size) {
		if (data_mep)
			data_mep->pad = dlen - clen;
		else
			mep->pad = dlen - clen;
		TX_DATA_FRAG (rrp->rr_writer, dfp, mep->header.flags);
		STATS_INC (rrp->rr_ndatafrags);
		add_proxy_elements (&rrp->proxy, first_mep, msize, 
					add_time_on_split, prefix);
	}
#endif
	if (kp)
		str_unref (kp);

	/* Don't force sending DATA immediately - a DATA message can still be
	   combined with other packets, such as DATA, GAP and HEARTBEAT. */
	if (push)
		proxy_activate (&rrp->proxy);

	return (DDS_RETCODE_OK);

    no_mem:
	log_printf (RTPS_ID, 0, "rtps_msg_add_data: no memory for extra element.\r\n");
	rtps_free_elements (first_mep);
	return (DDS_RETCODE_OUT_OF_RESOURCES);
}

/* rtps_msg_add_gap -- Add a GAP submessage element to an existing message. */

int rtps_msg_add_gap (RemReader_t              *rrp,
		      const DiscoveredReader_t *reader,
		      const SequenceNumber_t   *start,
		      const SequenceNumber_t   *base,
		      unsigned                 n_bits,
		      uint32_t                 *bits,
		      int                      push)
{
	GapSMsg		*dp;
	RME		*mep, *first_mep;
	const GuidPrefix_t	*prefix;
	unsigned	n_bytes, msize;

	/* Get a new submessage element header. */
	first_mep = mep = new_proxy_element (&rrp->proxy, 1/*RTPS_COMBINE*/);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_gap: no memory for submessage elements.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	if (reader && reader->dr_participant) {
		prefix = &reader->dr_participant->p_guid_prefix;
		rtps_msg_add_info_destination (&rrp->proxy, mep, prefix);
		msize = mep->length + sizeof (SubmsgHeader);
		mep = rtps_msg_append_mep (mep);
		if (!mep) {
			rtps_free_elements (first_mep);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
	}
	else {
		prefix = NULL;
		msize = 0;
	}
	ctrc_begind (RTPS_ID, RTPS_TX_GAP, &rrp->rr_writer->endpoint.endpoint->ep.entity_id, sizeof (EntityId_t));
	ctrc_contd (start, sizeof (SequenceNumber_t));
	ctrc_contd (base, sizeof (SequenceNumber_t));
	ctrc_contd (&n_bits, sizeof (n_bits));
	ctrc_endd ();

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_GAP;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra DATA submessage fields. */
	mep->data = mep->d;
	dp = (GapSMsg *) mep->d;
	n_bytes = ((n_bits + 31) & ~31) >> 3;
	mep->length = mep->header.length = sizeof (GapSMsg) + n_bytes - 4;
	dp->reader_id = (reader) ? reader->dr_entity_id : entity_id_unknown;
	dp->writer_id = rrp->rr_writer->endpoint.endpoint->ep.entity_id;
	dp->gap_start = *start;
	dp->gap_list.base = *base;
	dp->gap_list.numbits = n_bits;
	memcpy (dp->gap_list.bitmap, bits, n_bytes);
	msize += mep->length + sizeof (SubmsgHeader);

	/* Add submessage to message. */
	add_proxy_elements (&rrp->proxy, first_mep, msize, 0, prefix);

	STATS_INC (rrp->rr_ngap);

	/* Trace GAP frame if enabled. */
	TX_GAP (rrp->rr_writer, dp, mep->header.flags);

	/* Don't force sending GAP immediately - a GAP message can still be
	   combined with other packets, such as DATA, GAP and HEARTBEAT. */
	if (push)
		proxy_activate (&rrp->proxy);

	return (DDS_RETCODE_OK);
}

/* rtps_msg_add_heartbeat -- Add a HEARTBEAT submessage to an RTPS message. */

int rtps_msg_add_heartbeat (RemReader_t              *rrp,
			    const DiscoveredReader_t *reader,
			    unsigned                 flags,
			    const SequenceNumber_t   *min_seqnr,
			    const SequenceNumber_t   *max_seqnr)
{
	HeartbeatSMsg	*dp;
	RME		*first_mep, *mep;
	GuidPrefix_t	*prefix;
	unsigned	msize;

	/* Get a new submessage element header. */
	first_mep = mep = new_proxy_element (&rrp->proxy, RTPS_COMBINE);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_heartbeat: no memory for first submessage element.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Prepend with an INFO_DESTINATION if this is not a multicast. */
	if (reader && reader->dr_participant) {
		prefix = &reader->dr_participant->p_guid_prefix;
		rtps_msg_add_info_destination (&rrp->proxy, mep, prefix);
		msize = mep->length;
		mep = rtps_msg_append_mep (mep);
		if (!mep) {
			rtps_free_elements (first_mep);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
	}
	else {
		prefix = NULL;
		msize = 0;
	}
	ctrc_begind (RTPS_ID, RTPS_TX_HBEAT, &rrp->rr_writer->endpoint.endpoint->ep.entity_id, sizeof (EntityId_t));
	ctrc_contd (min_seqnr, sizeof (SequenceNumber_t));
	ctrc_contd (max_seqnr, sizeof (SequenceNumber_t));
	ctrc_endd ();

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_HEARTBEAT;
	mep->header.flags = SMF_CPU_ENDIAN;
	if (!prefix)
		rrp->rr_tail->element.flags |= RME_MCAST;
	mep->header.flags |= flags & (SMF_FINAL | SMF_LIVELINESS);

	/* Add extra HEARTBEAT submessage fields. */
	mep->data = mep->d;
	dp = (HeartbeatSMsg *) mep->d;
	mep->length = mep->header.length = DEF_HB_SIZE;
	dp->reader_id = (reader) ? reader->dr_entity_id : entity_id_unknown;
	dp->writer_id = rrp->rr_writer->endpoint.endpoint->ep.entity_id;
	dp->first_sn = *min_seqnr;
	dp->last_sn = *max_seqnr;
	dp->count = ++rrp->rr_writer->heartbeats;
#ifdef RTPS_PROXY_INST_TX
	if (reader &&
	    reader->dr_participant &&
	    reader->dr_participant->p_vendor_id [0] == VENDORID_H_TECHNICOLOR &&
	    reader->dr_participant->p_vendor_id [1] == VENDORID_L_TECHNICOLOR &&
	    reader->dr_participant->p_sw_version >= RTPS_PROXY_INST_VERSION) {
		mep->length += 4;
		mep->header.length += 4;
		dp->instance_id = rrp->rr_loc_inst;
	}
#endif
	msize += mep->length + sizeof (SubmsgHeader);

	/* Add submessage to message. */
	add_proxy_elements (&rrp->proxy, first_mep, msize, 0, prefix);

	STATS_INC (rrp->rr_nheartbeat);

	/* Trace HEARTBEAT frame if enabled. */
	TX_HEARTBEAT (rrp->rr_writer, dp, mep->header.flags);

	/* Always send HEARTBEAT immediately! */
	proxy_activate (&rrp->proxy);

	return (DDS_RETCODE_OK);
}

/* rtps_msg_add_acknack -- Add an ACKNACK submessage to an RTPS message. */

int rtps_msg_add_acknack (RemWriter_t              *rwp,
			  const DiscoveredWriter_t *writer,
			  int                      final,
			  const SequenceNumber_t   *base,
			  unsigned                 nbits,
			  const uint32_t           bitmaps [])
{
	AckNackSMsg	*dp;
	RME		*mep, *first_mep;
	GuidPrefix_t	*prefix;
	unsigned	n, msize;

	first_mep = mep = new_proxy_element (&rwp->proxy, RTPS_COMBINE);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_acknack: no memory for first submessage element.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	if (writer && writer->dw_participant)
		prefix = &writer->dw_participant->p_guid_prefix;
	else
		prefix = NULL;
	rtps_msg_add_info_destination (&rwp->proxy, mep, prefix);
	msize = mep->length + sizeof (SubmsgHeader);
	mep = rtps_msg_append_mep (first_mep);
	if (!mep) {
		rtps_free_elements (first_mep);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	ctrc_begind (RTPS_ID, RTPS_TX_ACKNACK, &rwp->rw_reader->endpoint.endpoint->ep.entity_id, sizeof (EntityId_t));
	ctrc_contd (base, sizeof (SequenceNumber_t));
	ctrc_contd (&nbits, sizeof (nbits));
	ctrc_endd ();

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_ACKNACK;
	mep->header.flags = SMF_CPU_ENDIAN;
	if (final)
		mep->header.flags |= SMF_FINAL;

	/* Add extra ACKNACK submessage fields. */
	mep->data = mep->d;
	dp = (AckNackSMsg *) mep->d;
	n = ((nbits + 31) >> 5) << 2;	/* # of bytes in bitmap. */
	mep->length = mep->header.length = sizeof (AckNackSMsg) + n - 4;
	dp->reader_id = rwp->rw_reader->endpoint.endpoint->ep.entity_id;
	dp->writer_id = writer->dw_entity_id;
	dp->reader_sn_state.base = *base;
	dp->reader_sn_state.numbits = nbits;
	memcpy (dp->reader_sn_state.bitmap, bitmaps, n);
	dp->reader_sn_state.bitmap [n >> 2] = ++rwp->rw_reader->acknacks;
#ifdef RTPS_PROXY_INST_TX
	if (writer->dw_participant->p_vendor_id [0] == VENDORID_H_TECHNICOLOR &&
	    writer->dw_participant->p_vendor_id [1] == VENDORID_L_TECHNICOLOR &&
	    writer->dw_participant->p_sw_version >= RTPS_PROXY_INST_VERSION) {
		mep->length += 4;
		mep->header.length += 4;
		dp->reader_sn_state.bitmap [(n >> 2) + 1] = rwp->rw_loc_inst;
	}
#endif
	msize += mep->length + sizeof (SubmsgHeader);

	/* Add submessage to message. */
	add_proxy_elements (&rwp->proxy, first_mep, msize, 0, prefix);

	STATS_INC (rwp->rw_nacknack);

	/* Trace ACKNACK frame if enabled. */
	TX_ACKNACK (rwp->rw_reader, dp, mep->header.flags);

	/* Always send ACKNACK immediately! */
	proxy_activate (&rwp->proxy);

	return (DDS_RETCODE_OK);
}

#ifdef RTPS_FRAGMENTS

/* rtps_msg_add_heartbeat_frag -- Add a HEARTBEAT_FRAG submessage to an RTPS msg. */

int rtps_msg_add_heartbeat_frag (RemReader_t              *rrp,
				 const DiscoveredReader_t *reader,
				 const SequenceNumber_t   *seqnr,
				 unsigned                 last_frag)
{
	HeartbeatFragSMsg	*dp;
	RME			*first_mep, *mep;
	GuidPrefix_t		*prefix;
	unsigned		msize;

	/* Get a new submessage element header. */
	first_mep = mep = new_proxy_element (&rrp->proxy, RTPS_COMBINE);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_heartbeat_frag: no memory for first submessage element.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Prepend with an INFO_DESTINATION if this is not a multicast. */
	if (reader && reader->dr_participant) {
		prefix = &reader->dr_participant->p_guid_prefix;
		rtps_msg_add_info_destination (&rrp->proxy, mep, prefix);
		msize = mep->length;
		mep = rtps_msg_append_mep (mep);
		if (!mep) {
			rtps_free_elements (first_mep);
			return (DDS_RETCODE_OUT_OF_RESOURCES);
		}
	}
	else {
		prefix = NULL;
		msize = 0;
	}

	ctrc_begind (RTPS_ID, RTPS_TX_HBEAT_FRAG, &rrp->rr_writer->endpoint.endpoint->ep.entity_id, sizeof (EntityId_t));
	ctrc_contd (seqnr, sizeof (SequenceNumber_t));
	ctrc_contd (&last_frag, sizeof (unsigned));
	ctrc_endd ();

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_HEARTBEAT_FRAG;
	mep->header.flags = SMF_CPU_ENDIAN;
	if (!reader)
		rrp->rr_tail->element.flags |= RME_MCAST;

	/* Add extra HEARTBEAT_FRAG submessage fields. */
	mep->data = mep->d;
	dp = (HeartbeatFragSMsg *) mep->d;
	mep->length = mep->header.length = sizeof (HeartbeatFragSMsg);
	dp->reader_id = (reader) ? reader->dr_entity_id : entity_id_unknown;
	dp->writer_id = rrp->rr_writer->endpoint.endpoint->ep.entity_id;
	dp->writer_sn = *seqnr;
	dp->last_frag = last_frag;
	dp->count = ++rrp->rr_writer->heartbeats;
	msize += mep->length + sizeof (SubmsgHeader);

	/* Add submessage to message. */
	add_proxy_elements (&rrp->proxy, first_mep, msize, 0, prefix);

	STATS_INC (rrp->rr_nheartbeatfrags);

	/* Trace HEARTBEAT frame if enabled. */
	TX_HEARTBEAT_FRAG (rrp->rr_writer, dp);

	/* Always send HEARTBEAT_FRAG immediately! */
	proxy_activate (&rrp->proxy);

	return (DDS_RETCODE_OK);
}

/* rtps_msg_add_nack_frag -- Add a NACK_FRAG submessage to an RTPS msg. */

int rtps_msg_add_nack_frag (RemWriter_t              *rwp,
			    const DiscoveredWriter_t *writer,
			    const SequenceNumber_t   *seqnr,
			    FragmentNumber_t         base,
			    unsigned                 nbits,
			    const uint32_t           bitmaps [])
{
	NackFragSMsg	*dp;
	RME		*first_mep, *mep;
	GuidPrefix_t	*prefix;
	unsigned	n, msize;

	/* Get a new submessage element header. */
	first_mep = mep = new_proxy_element (&rwp->proxy, RTPS_COMBINE);
	if (!mep) {
		log_printf (RTPS_ID, 0, "rtps_msg_add_nack_frag: no memory for first submessage element.\r\n");
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	/* Prepend with an INFO_DESTINATION. */
	prefix = &writer->dw_participant->p_guid_prefix;
	rtps_msg_add_info_destination (&rwp->proxy, mep, prefix);
	msize = mep->length + sizeof (SubmsgHeader);
	mep = rtps_msg_append_mep (mep);
	if (!mep) {
		rtps_free_elements (first_mep);
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}

	ctrc_begind (RTPS_ID, RTPS_TX_NACK_FRAG, &rwp->rw_reader->endpoint.endpoint->ep.entity_id, sizeof (EntityId_t));
	ctrc_contd (seqnr, sizeof (SequenceNumber_t));
	ctrc_contd (&base, sizeof (SequenceNumber_t));
	ctrc_contd (&nbits, sizeof (nbits));
	ctrc_endd ();

	/* Setup submessage header. */
	mep->flags |= RME_HEADER;
	mep->header.id = ST_NACK_FRAG;
	mep->header.flags = SMF_CPU_ENDIAN;

	/* Add extra NACK_FRAG submessage fields. */
	mep->data = mep->d;
	dp = (NackFragSMsg *) mep->d;
	n = ((nbits + 31) >> 5) << 2;	/* # of bytes in bitmap. */
	mep->length = mep->header.length = sizeof (NackFragSMsg) + n - 4;
	dp->reader_id = rwp->rw_reader->endpoint.endpoint->ep.entity_id;
	dp->writer_id = writer->dw_entity_id;
	dp->writer_sn = *seqnr;
	dp->frag_nr_state.base = base;
	dp->frag_nr_state.numbits = nbits;
	if (n)
		memcpy (dp->frag_nr_state.bitmap, bitmaps, n);
	dp->frag_nr_state.bitmap [n >> 2] = ++rwp->rw_reader->nackfrags;
	msize += mep->length + sizeof (SubmsgHeader);

	/* Add submessage to message. */
	add_proxy_elements (&rwp->proxy, first_mep, msize, 0, prefix);

	STATS_INC (rwp->rw_nnackfrags);

	/* Trace NACK_FRAG frame if enabled. */
	TX_NACK_FRAG (rwp->rw_reader, dp);

	/* Always send NACK_FRAG immediately! */
	proxy_activate (&rwp->proxy);

	return (DDS_RETCODE_OK);
}

#endif

#ifdef RTPS_TRACE_MEP
#define	RX_LL_TRACE(s)		log_printf (RTPS_ID, 0, "Rx: %s", s)
#define	RX_LL_ERROR(e)		log_printf (RTPS_ID, 0, " -> error: %s\r\n", e)
#define	RX_LL_INFO(s)		log_printf (RTPS_ID, 0, ", %s", s)
#define	RX_LL_END()		log_printf (RTPS_ID, 0, "\r\n")
#define	RX_LL_TRACE_MSG()	log_printf (RTPS_ID, 0, "[MSG]")
#define	RX_LL_TRACE_MSG_END()	log_printf (RTPS_ID, 0, "\r\n")
#else
#define	RX_LL_TRACE(s)
#define	RX_LL_ERROR(e)
#define	RX_LL_INFO(s)
#define	RX_LL_END()
#define	RX_LL_TRACE_MSG()
#define	RX_LL_TRACE_MSG_END()
#endif

/* submsg_data_ptr -- Return a linear submessage data field from a possibly
		      scattered submessage element.  If the specified range
		      is already linear, a straight pointer to the original
		      subelement field is returned.  If it is scattered, the
		      data is copied to the given data buffer and a pointer
		      to that buffer is returned. */

void *submsg_data_ptr (RME      *mep,	/* Subelement structure. */
           	       unsigned ofs,	/* Offset in subelement data. */
           	       unsigned len,	/* Length of data region. */
           	       void     *buf)	/* Buffer when scattered. */
{
	DB		*rdp;	/* Points to current data block. */
	unsigned char	*dp;	/* Direct data pointer in current data block. */
	unsigned char	*dbuf;	/* Fragment storage buffer. */
	unsigned	left;	/* # of bytes in data block from *dp onwards. */
	unsigned	n;

	if (!len || ofs + len > mep->length)	/* Region not available? */
		return (NULL);

	/* Setup rdp, dp and left variables. */
	rdp = mep->db;
	if (!rdp)
		return (&mep->d [ofs]);

	dp = mep->data;

	/* The DB mentioned in the RME can be a DB that was allocated for a
	 * previous submessage and reused for this submessage. To figure out
	 * how many bytes we have left inside this DB for the current
	 * submessage, we need to substract the start of the DB from the data
	 * pointer inside the DB, and substract this offset from the size of
	 * the DB to get the number of bytes still left in the DB. However,
	 * this offset also includes the DB header, which isn't counted in the
	 * size of the DB, so we have to substract the size of the header
	 * again, to find out how many bytes were used by preceding
	 * submessages. */
	left = rdp->size - ((dp - (unsigned char *) mep->db) - DB_HDRSIZE);

	/* Skip offset # of bytes. */
	if (ofs) {
		while (ofs >= left) {
			ofs -= left;
			rdp = rdp->next;
			dp = rdp->data;
			left = rdp->size;
		}
		dp += ofs;
		left -= ofs;
	}

	/* Check if data region is fragmented. */
	if (len <= left) /* Not fragmented: just return current data pointer. */
		return (dp);

	/* Data region is fragmented, copy data, fragment by fragment to buf. */
	dbuf = buf;
	for (;;) {
		if (len > left)
			n = left;
		else
			n = len;
		memcpy (dbuf, dp, n);
		dbuf += n;
		len -= n;
		if (!len)
			break;

		rdp = rdp->next;
		dp = rdp->data;
		left = rdp->size;
	}
	return (buf);
}

#define	reply_update_needed(lp,rxp)	(!lp ||						\
					 rxp->n_uc_replies || rxp->n_mc_replies ||	\
					 lp->locator.kind > rxp->src_locator.kind)

/* submsg_rx_data -- Receive actions for the RTPS.DATA subelement. */

static void submsg_rx_data (RECEIVER *rxp, RME *mep)
{
	DataSMsg	*dp;
	size_t		length;
	unsigned	dleft, n, sofs;
	int		swap, all_endpoints, new_key, error, qos_size, ignore;
	unsigned	size;
	Reader_t	*rp;
	Change_t	*cp;
	READER		*rdrp;
	RemWriter_t	*rwp;
	Endpoint_t	*peer_ep;
	RWType_t	sf_type;
	DB		*dbp;
	unsigned char	*ddp, *key;
	DBW		walk;
	InlineQos_t	qos;
	KeyHash_t	*hp = NULL;
	StatusInfo_t	*status = NULL;
	DDS_GUIDSeq	*dwrite = NULL;
	PIDSet_t	pids [2];
	uint32_t	ui32;
	uint16_t	ui16;
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	size_t		ofs;
#endif
	const TypeSupport_t *ts;
	unsigned char	data [sizeof (DataSMsg)];
#ifdef MAX_KEY_BUFFER
	unsigned char	key_buffer [MAX_KEY_BUFFER];
#endif
	PROF_ITER	(prof_nrdrs)

	prof_start (rtps_rx_data);
	RX_LL_TRACE ("DATA");
	
	if (mep->length < sizeof (DataSMsg)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		goto cleanup1;
	}
	dp = (DataSMsg *) SUBMSG_DATA (mep, 0, sizeof (DataSMsg), data);
	swap = (mep->flags & RME_SWAP);
	if (swap) {
		SWAP_UINT16 (dp->inline_qos_ofs, ui16);
		SWAP_SEQNR (dp->writer_sn, ui32);
	}

	ctrc_begind (RTPS_ID, RTPS_RX_DATA, &mep, sizeof (mep));
	ctrc_contd (&dp->writer_id, sizeof (dp->writer_id));
	ctrc_contd (&dp->writer_sn, sizeof (dp->writer_sn));
	ctrc_endd ();

	if ((dp->writer_sn.high == 0 && dp->writer_sn.low < 1) ||
	    (mep->header.flags & (SMF_DATA | SMF_KEY)) ==
	    			  (SMF_DATA | SMF_KEY)) { /* Invalid message! */
	    inv_submsg:

		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

	    save_msg:
		rxp->last_mep = *mep;
		rxp->msg_size = sizeof (DataSMsg);
		memcpy (rxp->msg_buffer, dp, sizeof (DataSMsg));
		goto cleanup1;
	}

	/* Check if this is a valid entity type. */
	if (!entity_id_writer (dp->writer_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (rxp->peer)
		peer_ep = endpoint_lookup (rxp->peer, &dp->writer_id);
	else
		peer_ep = NULL;
	if (!peer_ep || (rwp = peer_ep->rtps) == NULL) {
		if (entity_id_eq (rtps_builtin_eids [EPB_PARTICIPANT_W],
				  dp->writer_id))
			rp = (Reader_t *) rxp->domain->participant.
					p_builtin_ep [EPB_PARTICIPANT_R];
#ifdef DDS_NATIVE_SECURITY
		else if (entity_id_eq (rtps_builtin_eids [EPB_PARTICIPANT_SL_W],
				  dp->writer_id))
			rp = (Reader_t *) rxp->domain->participant.
					p_builtin_ep [EPB_PARTICIPANT_SL_R];
#endif
		else
			rp = NULL;

		if (rp) {
			rdrp = (READER *) rp->r_rtps;
			all_endpoints = 0;
			rwp = NULL;
		}
		else {
		    unknown_dest:

			RX_LL_ERROR ("UnknownDest");
			rxp->unkn_dest++;
			rxp->last_error = R_UNKN_DEST;
			goto save_msg;
		}
	}
	else if ((all_endpoints = entity_id_unknown (dp->reader_id)) != 0) {
		rdrp = rwp->rw_reader;
		rp = (Reader_t *) rdrp->endpoint.endpoint;
	}
	else {
		if (!entity_id_reader (dp->reader_id))
			goto inv_submsg;

		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			if (entity_id_eq (rwp->rw_reader->endpoint.endpoint->ep.entity_id,
					  dp->reader_id)) {
				rdrp = rwp->rw_reader;
				rp = (Reader_t *) rdrp->endpoint.endpoint;
				break;
			}
		if (!rwp)
			goto unknown_dest;
	}

	/* Allocate a Change_t descriptor. */
	cp = hc_change_new ();
	if (!cp) {
		warn_printf ("submsg_rx_data: out of memory for cache change!\r\n");
		RX_LL_END ();
		goto cleanup1;
	}
	TRC_CHANGE (cp, "submsg_rx_data", 1);

	/* Create a cache change object. */
	cp->c_writer = (peer_ep) ? peer_ep->entity.handle : 0;
	if (rxp->have_timestamp)
		cp->c_time = rxp->timestamp;
	else
		sys_getftime (&cp->c_time);
	cp->c_handle = 0;	/* Not specified yet -- derived in reader. */
	new_key = 0;
	ddp = NULL;
	dbp = NULL;
	length = mep->length;
	sofs = dp->inline_qos_ofs + 4;
	if (sofs >= length) {
		RX_LL_INFO ("sofs>=length - TooShort");
		cp->c_length = 0;
		goto cleanup2;
	}
	length -= sofs;	/* Total remaining data field length. */

	/* Setup message access parameters in walk structure. */
	if (mep->db) {

		/* Setup sbp, sdp and sleft variables. */
		walk.dbp = mep->db;
		walk.data = mep->data;
		walk.left = walk.dbp->size -
			    (walk.data - (unsigned char *) walk.dbp->data);

		/* Skip data offset bytes in large buffer. */
		while (sofs) {
			n = sofs;
			if (n >= walk.left) {
				n = walk.left;
				walk.dbp = walk.dbp->next;
				walk.data = walk.dbp->data;
				walk.left = walk.dbp->size;
			}
			else {
				walk.data += n;
				walk.left -= n;
			}
			sofs -= n;
		}
		if (walk.left > length)
			walk.left = length;
	}
	else {
		walk.dbp = NULL;
		walk.data = mep->data + sofs;
		walk.left = length;
	}

	/* Check message flags (Q, D and K). */
	if ((mep->header.flags & SMF_INLINE_QOS) != 0) {
		walk.length = length;
		qos_size = pid_parse_inline_qos (&walk, &qos, pids, swap);
		if (qos_size < 0) {

		    inv_qos:

			RX_LL_ERROR ("InvQos");
			rxp->inv_qos++;
			rxp->last_error = R_INV_QOS;

		    save_msg2:

			rxp->last_mep = *mep;
			rxp->msg_size = sizeof (DataSMsg);
			memcpy (rxp->msg_buffer, dp, sizeof (DataSMsg));
			goto cleanup2;
		}
		length -= qos_size;
		if (PID_INSET (pids [0], PID_KEY_HASH))
			hp = &qos.key_hash;
		if (PID_INSET (pids [0], PID_STATUS_INFO))
			status = &qos.status_info;
		if (PID_INSET (pids [0], PID_DIRECTED_WRITE) && 
		    qos.directed_write._length)
			dwrite = &qos.directed_write;
	}
	cp->c_kind = ALIVE;
	if ((mep->header.flags & SMF_DATA) != 0 && length)
		cp->c_length = length;
	else {
		cp->c_length = 0;
		if (status) {
			RX_LL_INFO ("<status>");
			if ((status->value [3] & 1) != 0)
				cp->c_kind |= NOT_ALIVE_DISPOSED;
			if ((status->value [3] & 2) != 0)
				cp->c_kind |= NOT_ALIVE_UNREGISTERED;
			if (!cp->c_kind)
				goto inv_qos;
		}
		else
			cp->c_kind = NOT_ALIVE_UNREGISTERED;
		goto deliver_data;
	}

	if (walk.dbp) { /* Data stored in large buffer. */

		/* If data size < 1/2 a data block, and there are other
		   data buffers (presumably with a much smaller size), then
		   just copy the data.  Otherwise we might end up with 8K
		   buffers carrying 100 bytes, quickly draining the receive
		   buffers. */
		dleft = (*walk.data << 8 | walk.data [1]) >> 1;
		if (dleft != MODE_RAW && walk.left < cp->c_length) {

			/* Marshalled data && not linear: copy it. */
			dbp = db_alloc_data (cp->c_length, 1);
			if (!dbp) {

			    no_buffers:

				RX_LL_ERROR ("NoBufs");
				rxp->no_bufs++;
				rxp->last_error = R_NO_BUFS;
				goto save_msg2;
			}
		}
		else if (cp->c_length <= (unsigned) (walk.dbp->size >> 1))
			dbp = db_alloc_data (cp->c_length, 1);
		else
			dbp = NULL;

		if (dbp) {
			/* Copy big buffer to the allocated buffer chain. */
			cp->c_db = dbp;
			cp->c_data = dbp->data;
			length = cp->c_length;
			dleft = dbp->size;
			ddp = dbp->data;
			while (length) {

				/* Copy a data chunk. */
				n = (walk.left > dleft) ? dleft : walk.left;
				if (n > length)
					n = length;
				if (n) {
					length -= n;
					memcpy (ddp, walk.data, n);
				}

				/* Update source pointers. */
				walk.left -= n;
				if (walk.left)
					walk.data += n;
				else if (length) {
					walk.dbp = walk.dbp->next;
					walk.left = walk.dbp->size;
				}

				/* Update destination pointers. */
				dleft -= n;
				if (dleft)
					ddp += n;
				else if (length) {
					dbp = dbp->next;
					dleft = dbp->size;
					ddp = dbp->data;
				}
			}
			dbp = NULL;
		}
		else {
			cp->c_db = (DB *) walk.dbp;
			cp->c_data = (unsigned char *) walk.data;
			mep->db = NULL;
		}
	}
	else { /* Data stored in element data. */
		if ((dbp = db_alloc_data (cp->c_length, 1)) == NULL) {
			warn_printf ("submsg_rx_data: out of memory for data buffer!\r\n");
			goto cleanup2;
		}
		cp->c_db = dbp;
		cp->c_data = dbp->data;
		memcpy (dbp->data, walk.data, cp->c_length);
		dbp = NULL;
	}

    deliver_data:

#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)

	/* Decrypt payload data if an encrypted payload is present. */
	if (rwp &&
	    rwp->rw_crypto &&
	    (cp->c_length || (mep->header.flags & SMF_KEY) != 0)) {
		if (cp->c_length) {	/* Set walk to real sample data. */
			walk.dbp = cp->c_db;
			walk.data = cp->c_data;
			walk.length = length = cp->c_length;
			walk.left = cp->c_db->size - (cp->c_data - cp->c_db->data);
			if (walk.left > length)
				walk.left = length;
		}
		/* else -- walk/length are still valid for key data. */

		dbp = sec_decode_serialized_data (&walk,
						  0,
						  rwp->rw_crypto,
						  &length,
						  &ofs,
						  (DDS_ReturnCode_t *) &error);
		if (!dbp)
			goto invalid_marshalled_data;

		if (cp->c_length) {	/* Real sample data decrypted now. */
			cp->c_length = length;
			cp->c_db = dbp;
			cp->c_data = dbp->data + ofs;
			dbp = NULL;
		}
		else {			/* Set walk to decrypted key data. */
			walk.dbp = dbp;
			walk.data = dbp->data + ofs;
			walk.length = walk.left = length;
		}
	}
#endif

	/* If data type indicates multi-instance data, we need the actual keys
	   in order to lookup instances properly. */
	ts = rp->r_topic->type->type_support;
	new_key = 0;
	if (ts->ts_keys &&
	    (cp->c_length || (mep->header.flags & SMF_KEY) != 0)) {

		/* Key somewhere in either marshalled data or marshalled key. */
		if (cp->c_length) {
			walk.dbp = cp->c_db;
			walk.data = cp->c_data;
			walk.left = walk.length = cp->c_length;
		}
		size = ts->ts_mkeysize;
		if (!size || !ts->ts_fksize) {
			size = DDS_KeySizeFromMarshalled (walk, ts,
					(mep->header.flags & SMF_KEY) != 0, NULL);
			if (!size) {

			    invalid_marshalled_data:

				RX_LL_ERROR ("InvMarshall(keysize)");
				rxp->inv_marshall++;
				rxp->last_error = R_INV_MARSHALL;
				goto save_msg2;
			}
		}
		if (ts->ts_mkeysize &&
		    ts->ts_fksize &&
		    size <= sizeof (KeyHash_t) &&
		    !ENC_DATA (&rp->r_lep)) {
			if (hp) {
				key = hp->hash;
				goto got_keys;
			}
			key = qos.key_hash.hash;
			if (size < sizeof (KeyHash_t))
				memset (qos.key_hash.hash + size, 0,
						     sizeof (KeyHash_t) - size);
		}
#ifdef MAX_KEY_BUFFER
		else if (size <= MAX_KEY_BUFFER)
			key = key_buffer;
#endif
		else {
			key = xmalloc (size);
			if (!key) {
				RX_LL_ERROR ("NoBufs");
				goto no_buffers;
			}
			new_key = 1;
		}
		error = DDS_KeyFromMarshalled (key, walk, ts,
					 (mep->header.flags & SMF_KEY) != 0,
					 ENC_DATA (&rp->r_lep));
		if (error) {
			if (new_key)
				xfree (key);

			goto invalid_marshalled_data;
		}
		if (!hp) {
			error = DDS_HashFromKey (qos.key_hash.hash, key, size,
							ENC_DATA (&rp->r_lep), ts);
			if (error) {
				if (new_key)
					xfree (key);
				goto invalid_marshalled_data;
			}
			hp = &qos.key_hash;
		}
	}
	else if (ts->ts_keys) {

		/* Key from hash. */
		if (!hp ||
		    !ts->ts_mkeysize ||
		    ts->ts_mkeysize > 16 ||
		    ENC_DATA (&rp->r_lep))
			goto invalid_marshalled_data;

		key = hp->hash;
		size = ts->ts_mkeysize;
	}
	else {
		key = NULL;
		new_key = 0;
		size = 0;
	}

    got_keys:

	/* Lock access to the reader. */
	lock_take (rp->r_lock);

	/* Trace DATA frame if tracing enabled. */
	RX_DATA (rdrp, rxp->peer, dp, mep->header.flags);

	/* Add to either writer proxy or directly to reader cache. */
	if (!rdrp->endpoint.stateful) {
		PROF_INC (prof_nrdrs);
		rcl_access (cp);
		cp->c_nrefs++;
		rcl_done (cp);
		cp->c_seqnr = dp->writer_sn;
		participant_add_prefix (key, size);
		reader_cache_add_key (rdrp, cp, hp, key, size);
	}
	else {
		for (;;) {
			PROF_INC (prof_nrdrs);
			sf_type = rw_type (rwp->rw_reliable);
			if (dwrite) {
				ignore = 1;
				for (n = 0; n < dwrite->_length; n++)
					if (guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
							    dwrite->_buffer [n].prefix) &&
					    entity_id_eq (rp->r_entity_id,
					    		  dwrite->_buffer [n].entity_id)) {
						ignore = 0;
						break;
					}
				if (n >= dwrite->_length && !rwp->rw_reliable)
					continue;
			}
			else
				ignore = 0;

			/* Update Reply locator if necessary. */
			if (rwp->rw_reliable &&
			    reply_update_needed (rwp->rw_uc_dreply, rxp)) {
				lrloc_print ("RTPS: Data: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rwp->proxy, rxp);
			}

			/* Handle message. */
			(*rtps_rw_event [sf_type]->data) (rwp, cp, &dp->writer_sn,
					    	hp, key, size, 
#ifdef RTPS_FRAGMENTS
						NULL, NULL,
#endif
						ignore);
			if (!all_endpoints)
				break;

			if ((rwp = (RemWriter_t *) rwp->rw_next_guid) == NULL)
				break;

			lock_release (rp->r_lock);
			rdrp = rwp->rw_reader;
			rp = (Reader_t *) rdrp->endpoint.endpoint;
			lock_take (rp->r_lock);
		}
	}
	lock_release (rp->r_lock);

	/* Cleanup extra allocated data. */
	if (new_key)
		xfree (key);

    cleanup2:
#if defined (DDS_SECURITY) && defined (DDS_NATIVE_SECURITY)
	if (dbp)
		db_free_data (dbp); 
#endif
	if (dwrite && dwrite->_length)
		xfree (dwrite->_buffer);

	RX_LL_END ();

	TRC_CHANGE (cp, "submsg_rx_data", 0);
	hc_change_free (cp);

    cleanup1:
	if (mep->db) {
		db_free_data (mep->db);
		mep->db = NULL;
	}
	prof_stop (rtps_rx_data, prof_nrdrs);
}

static int snr_set_valid (SequenceNumberSet *sp, unsigned remain)
{
	return (SEQNR_VALID (sp->base) &&
	        sp->numbits &&
		sp->numbits <= 256 &&
		(((sp->numbits + 31) & ~0x1f) >> 3) <= remain);
}

static void swap_snr_set (SequenceNumberSet *sp, int extra)
{
	unsigned	i;
	uint32_t	t;

	SWAP_SEQNR (sp->base, t);
	SWAP_UINT32 (sp->numbits, t);
	if (sp->numbits + extra == 0 || sp->numbits > 256)
		return;

	for (i = 0; i < ((sp->numbits + 31) >> 5) + extra; i++) {
		SWAP_UINT32 (sp->bitmap [i], t);
	}
}

/* submsg_rx_gap -- Receive actions for the RTPS.GAP subelement. */

static void submsg_rx_gap (RECEIVER *rxp, RME *mep)
{
	GapSMsg		*gp;
	unsigned	length;
	int		all_endpoints;
	Reader_t	*rp;
	READER		*rdrp;
	RemWriter_t	*rwp;
	Endpoint_t	*peer_ep;
	uint32_t	t;
	unsigned char	data [MAX_GAP_SIZE];
	PROF_ITER	(n);

	prof_start (rtps_rx_gap);

	RX_LL_TRACE ("GAP");

	if (mep->header.length < MIN_GAP_SIZE) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	length = mep->header.length;
	if (length > MAX_GAP_SIZE)
		length = MAX_GAP_SIZE;
	gp = (GapSMsg *) SUBMSG_DATA (mep, 0, length, data);

	ctrc_begind (RTPS_ID, RTPS_RX_GAP, &mep, sizeof (mep));
	ctrc_contd (&gp->writer_id, sizeof (gp->writer_id));
	ctrc_contd (&gp->gap_start, sizeof (gp->gap_start));
	ctrc_contd (&gp->gap_list.base, sizeof (gp->gap_list.base));
	ctrc_contd (&gp->gap_list.numbits, sizeof (gp->gap_list.numbits));
	ctrc_endd ();

	if ((mep->flags & RME_SWAP) != 0) {
		SWAP_SEQNR (gp->gap_start, t);
		swap_snr_set (&gp->gap_list, 0);
	}
	if (!SEQNR_VALID (gp->gap_start) ||
	    SEQNR_GT (gp->gap_start, gp->gap_list.base) ||
	    (mep->header.length > MIN_GAP_SIZE && 
	     !snr_set_valid (&gp->gap_list,
			    mep->length - MIN_GAP_SIZE))) {

		/* Invalid msg!*/

	    inv_submsg:

		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

	    save_msg:
		rxp->last_mep = *mep;
		rxp->msg_size = length;
		memcpy (rxp->msg_buffer, gp, length);
		return;
	}

	/* Check if this is a valid entity type. */
	if (!entity_id_writer (gp->writer_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (!rxp->peer ||
	    (peer_ep = endpoint_lookup (rxp->peer, &gp->writer_id)) == NULL ||
	    (rwp = peer_ep->rtps) == NULL) {

	    unknown_dest:

		RX_LL_ERROR ("UnknDest");
		rxp->unkn_dest++;
		rxp->last_error = R_UNKN_DEST;
		goto save_msg;
	}
	if ((all_endpoints = entity_id_unknown (gp->reader_id)) != 0)
		rdrp = rwp->rw_reader;
	else {
		if (!entity_id_reader (gp->reader_id))
			goto inv_submsg;

		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			if (entity_id_eq (rwp->rw_reader->endpoint.endpoint->ep.entity_id,
					  gp->reader_id)) {
				rdrp = rwp->rw_reader;
				break;
			}

		if (!rwp)
			goto unknown_dest;
	}
	rp = (Reader_t *) rdrp->endpoint.endpoint;

	/* Lock access to the reader. */
	lock_take (rp->r_lock);

	/* Trace GAP frame if enabled. */
	RX_GAP (rdrp, rxp->peer, gp, mep->header.flags);

	/* Process GAP on all reliable proxy writers. */
	for (;;) {
		PROF_INC (n);

		if (rwp->rw_reader->endpoint.stateful && rwp->rw_reliable)

			/* Update Reply locator if necessary. */
			if (reply_update_needed (rwp->rw_uc_dreply, rxp)) {
				lrloc_print ("RTPS: Gap: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rwp->proxy, rxp);
			}

			/* Handle message. */
			(*rtps_rw_event [RWT_SF_REL]->gap) (rwp, gp);

		if (!all_endpoints)
			break;

		if ((rwp = (RemWriter_t *) rwp->rw_next_guid) == NULL)
			break;

		lock_release (rp->r_lock);
		rdrp = rwp->rw_reader;
		rp = (Reader_t *) rdrp->endpoint.endpoint;
		lock_take (rp->r_lock);
	}
	lock_release (rp->r_lock);

	prof_stop (rtps_rx_gap, n);
	RX_LL_END ();
}

/* submsg_rx_heartbeat -- Receive actions for the RTPS.HEARTBEAT subelement. */

static void submsg_rx_heartbeat (RECEIVER *rxp, RME *mep)
{
	HeartbeatSMsg	*hp;
	unsigned	flags;
	int		all_endpoints;
	Reader_t	*rp;
	READER		*rdrp;
	RemWriter_t	*rwp;
	Endpoint_t	*peer_ep;
	uint32_t	t;
	unsigned char	data [sizeof (HeartbeatSMsg)];
	SequenceNumber_t s;
	PROF_ITER	(n);

	prof_start (rtps_rx_hbeat);
	RX_LL_TRACE ("HEARTBEAT");

	if (mep->header.length < DEF_HB_SIZE) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
#ifdef RTPS_PROXY_INST_TX
	if (mep->header.length >= MAX_HB_SIZE)
		t = MAX_HB_SIZE;
	else
#endif
		t = DEF_HB_SIZE;
	hp = (HeartbeatSMsg *) SUBMSG_DATA (mep, 0, t, data);
	if ((mep->flags & RME_SWAP) != 0) {
		SWAP_SEQNR (hp->first_sn, t);
		SWAP_SEQNR (hp->last_sn, t);
	}

	ctrc_begind (RTPS_ID, RTPS_RX_HBEAT, &mep, sizeof (mep));
	ctrc_contd (&hp->writer_id, sizeof (hp->writer_id));
	ctrc_contd (&hp->first_sn, sizeof (hp->first_sn));
	ctrc_contd (&hp->last_sn, sizeof (hp->first_sn));
	ctrc_endd ();

	s = hp->last_sn;
	SEQNR_INC (s);
	if (!SEQNR_VALID (hp->first_sn) ||
	    hp->last_sn.high < 0 ||
	    SEQNR_GT (hp->first_sn, s)) { /* Invalid message! */

	    inv_submsg:

		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

	    save_msg:
		rxp->last_mep = *mep;
		rxp->msg_size = sizeof (HeartbeatSMsg);
		memcpy (rxp->msg_buffer, hp, rxp->msg_size);
		return;
	}

	/* Check if this is a valid entity type. */
	if (!entity_id_writer (hp->writer_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (!rxp->peer ||
	    (peer_ep = endpoint_lookup (rxp->peer, &hp->writer_id)) == NULL ||
	    (rwp = peer_ep->rtps) == NULL) {

	    unknown_dest:

		RX_LL_ERROR ("UnknDest");
		rxp->unkn_dest++;
		rxp->last_error = R_UNKN_DEST;
		goto save_msg;
	}
#ifdef RTPS_PROXY_INST_TX
	if (mep->header.length < MAX_HB_SIZE ||
	    rxp->peer->p_vendor_id [0] != VENDORID_H_TECHNICOLOR ||
	    rxp->peer->p_vendor_id [1] != VENDORID_L_TECHNICOLOR ||
	    rxp->peer->p_sw_version < RTPS_PROXY_INST_VERSION)
		hp->instance_id = 0;
#endif
	if ((all_endpoints = entity_id_unknown (hp->reader_id)) != 0)
		rdrp = rwp->rw_reader;
	else {
		if (!entity_id_reader (hp->reader_id))
			goto inv_submsg;

		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			if (entity_id_eq (rwp->rw_reader->endpoint.endpoint->ep.entity_id,
					  hp->reader_id)) {
				rdrp = rwp->rw_reader;
				break;
			}

		if (!rwp)
			goto unknown_dest;
	}
	flags = mep->header.flags & (SMF_FINAL | SMF_LIVELINESS);
	rp = (Reader_t *) rdrp->endpoint.endpoint;

	/* Lock access to the reader. */
	lock_take (rp->r_lock);

	/* Trace HEARTBEAT frame if enabled. */
	RX_HEARTBEAT (rdrp, rxp->peer, hp, mep->header.flags);

	/* Process HEARTBEAT on all reliable proxy writers. */
	for (;;) {
		PROF_INC (n);

		if (rwp->rw_reader->endpoint.stateful && rwp->rw_reliable) {

			/* Update Reply locator if necessary. */
			if (reply_update_needed (rwp->rw_uc_dreply, rxp)) {
				lrloc_print ("RTPS: Heartbeat: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rwp->proxy, rxp);
			}

			/* Handle message. */
			(*rtps_rw_event [RWT_SF_REL]->heartbeat) (rwp, hp, flags);
		}
		if (!all_endpoints)
			break;

		if ((rwp = (RemWriter_t *) rwp->rw_next_guid) == NULL)
			break;

		lock_release (rp->r_lock);
		rdrp = rwp->rw_reader;
		rp = (Reader_t *) rdrp->endpoint.endpoint;
		lock_take (rp->r_lock);
	}
	lock_release (rp->r_lock);

	RX_LL_END ();
	prof_stop (rtps_rx_hbeat, n);
}

/* submsg_rx_acknack -- Receive actions for the RTPS.ACKNACK subelement. */

static void submsg_rx_acknack (RECEIVER *rxp, RME *mep)
{
	AckNackSMsg	*ap;
	Writer_t	*wp;
	WRITER		*lwp;
	RemReader_t	*rrp;
	Endpoint_t	*peer_ep;
	unsigned	length;
	int		all_endpoints;
#ifdef RTPS_PROXY_INST_TX
	unsigned	nw;
#endif
	unsigned char	data [MAX_ACKNACK_SIZE];
	PROF_ITER	(n);

	prof_start (rtps_rx_acknack);
	RX_LL_TRACE ("ACKNACK");

	if (mep->header.length < MIN_ACKNACK_SIZE
#ifdef RTPS_EMPTY_ACKNACK
					- 4
#endif
					 	 ) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	length = mep->header.length;
	if (length > MAX_ACKNACK_SIZE)
		length = MAX_ACKNACK_SIZE;
	ap = (AckNackSMsg *) SUBMSG_DATA (mep, 0, length, data);
	if ((mep->flags & RME_SWAP) != 0)
		swap_snr_set (&ap->reader_sn_state, 1);

	ctrc_begind (RTPS_ID, RTPS_RX_ACKNACK, &mep, sizeof (mep));
	ctrc_contd (&ap->writer_id, sizeof (ap->writer_id));
	ctrc_contd (&ap->reader_sn_state.base, sizeof (SequenceNumber_t));
	ctrc_contd (&ap->reader_sn_state.numbits, sizeof (uint32_t));
	ctrc_endd ();

#ifdef RTPS_PROXY_INST_TX
	nw = (ap->reader_sn_state.numbits + 31) >> 5;
	if (mep->header.length != sizeof (AckNackSMsg) + (nw << 2))
		ap->reader_sn_state.bitmap [nw + 1] = 0;
#endif
	if (ap->reader_sn_state.numbits &&
	    !snr_set_valid (&ap->reader_sn_state,
			    mep->length - MIN_ACKNACK_SIZE + sizeof (uint32_t))) {

	    inv_submsg:

		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

	    save_msg:
		rxp->last_mep = *mep;
		rxp->msg_size = length;
		memcpy (rxp->msg_buffer, ap, length);
		return;
	}

	/* Check if this is a valid entity type. */
	if (!entity_id_reader (ap->reader_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (!rxp->peer ||
	    (peer_ep = endpoint_lookup (rxp->peer, &ap->reader_id)) == NULL ||
	    (rrp = peer_ep->rtps) == NULL) {

	    unknown_dest:

		RX_LL_ERROR ("UnknDest");
		rxp->unkn_dest++;
		rxp->last_error = R_UNKN_DEST;
		goto save_msg;
	}
	if ((all_endpoints = entity_id_unknown (ap->writer_id)) != 0)
		lwp = rrp->rr_writer;
	else {
		if (!entity_id_writer (ap->writer_id))
			goto inv_submsg;

		for (; rrp; rrp = (RemReader_t *) rrp->rr_next_guid)
			if (entity_id_eq (rrp->rr_writer->endpoint.endpoint->ep.entity_id,
					  ap->writer_id)) {
				lwp = rrp->rr_writer;
				break;
			}

		if (!rrp)
			goto unknown_dest;
	}
	wp = (Writer_t *) lwp->endpoint.endpoint;

	/* Lock access to the writer. */
	lock_take (wp->w_lock);

	/* Trace ACKNACK frame if enabled. */
	RX_ACKNACK (lwp, rxp->peer, ap, mep->header.flags);

	/* Process ACKNACK on all reliable proxy readers. */
	for (;;) {
		PROF_INC (n);

		if (lwp->endpoint.stateful && rrp->rr_reliable) {

			/* Update Reply locator. */
			if (reply_update_needed (rrp->rr_uc_dreply, rxp)) {
				lrloc_print ("RTPS: Acknack: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rrp->proxy, rxp);
			}

			/* Handle message. */
			(*rtps_rr_event [RRT_SF_REL]->acknack) (rrp, ap,
						mep->header.flags & SMF_FINAL);
		}

		if (!all_endpoints)
			break;

		if ((rrp = (RemReader_t *) rrp->rr_next_guid) == NULL)
			break;

		lock_release (wp->w_lock);
		lwp = rrp->rr_writer;
		wp = (Writer_t *) lwp->endpoint.endpoint;
		lock_take (wp->w_lock);
	} 
	lock_release (wp->w_lock);

	RX_LL_END ();
	prof_stop (rtps_rx_acknack, n);
}

/* submsg_rx_info_ts -- Receive actions for the RTPS.INFO_TIMESTAMP
			subelement. */

static void submsg_rx_info_ts (RECEIVER *rxp, RME *mep)
{
	InfoTimestampSMsg	*ip;
	uint32_t		t;
	unsigned char		data [sizeof (InfoTimestampSMsg)];

	prof_start (rtps_rx_inf_ts);
	RX_LL_TRACE ("INFO-TS");

	ctrc_printd (RTPS_ID, RTPS_RX_INFO_TS, &mep, sizeof (mep));

	if ((mep->header.flags & SMF_INVALIDATE) == 0 &&
	    mep->length < sizeof (InfoTimestampSMsg)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	if ((mep->header.flags & SMF_INVALIDATE) == 0) {
		ip = (InfoTimestampSMsg *) SUBMSG_DATA (mep, 0, sizeof (InfoTimestampSMsg), data);
		if ((mep->flags & RME_SWAP) != 0) {
			SWAP_TS (*ip, t);
		}
		rxp->have_timestamp = 1;
		FTIME_SETF (rxp->timestamp, ip->seconds, ip->fraction);
	}
	else
		rxp->have_timestamp = 0;

	RX_LL_END ();
	prof_stop (rtps_rx_inf_ts, 1);
}

/* submsg_rx_info_reply -- Receive actions for the RTPS.INFO_REPLY subelement.*/

static void submsg_rx_info_reply (RECEIVER *rxp, RME *mep)
{
	unsigned	minsize;
	Locator_t	*locp;
	uint32_t	nuclocs, nmclocs = 0, i, l, *lp, t, kind;
	unsigned	n = 0, mcofs;

	ctrc_printd (RTPS_ID, RTPS_RX_INFO_REPLY, &mep, sizeof (mep));
	prof_start (rtps_rx_inf_rep);

	RX_LL_TRACE ("INFO-REPLY");

	if (mep->length < sizeof (uint32_t)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	rxp->n_uc_replies = rxp->n_mc_replies = 0;
	lp = (uint32_t *) SUBMSG_DATA (mep, 0, sizeof (uint32_t), &l);
	if (lp && (mep->flags & RME_SWAP) != 0) {
		SWAP_UINT32 (*lp, t);
	}
	nuclocs = (lp) ? *lp : 0;
	minsize = mcofs = sizeof (uint32_t) + nuclocs * MSG_LOCATOR_SIZE;
	if ((mep->header.flags & SMF_MULTICAST) != 0) {
		if (mep->length < minsize + sizeof (uint32_t))
			minsize = ~0;
		else {
			lp = (uint32_t *) SUBMSG_DATA (mep, mcofs,
						    sizeof (uint32_t), &l);
			if (lp && (mep->flags & RME_SWAP) != 0) {
				SWAP_UINT32 (*lp, t);
			}
			nmclocs = (lp) ? *lp : 0;
			minsize += sizeof (uint32_t) +
						nmclocs * MSG_LOCATOR_SIZE;
		}
	}
	if (mep->length < minsize) { /* Invalid message length! */
		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;
		rxp->last_mep = *mep;
		rxp->msg_size = mep->length;
		if (lp)
			memcpy (rxp->msg_buffer, lp, mep->length);
		return;
	}
	if (nuclocs) {
		if (nuclocs > MAXLLOCS) {
			n = MAXLLOCS;
			warn_printf ("submsg_rx_info_reply: unicast locator list truncated!\r\n");
		}
		else
			n = nuclocs;
		locp = (Locator_t *) SUBMSG_DATA (mep,
						  sizeof (uint32_t),
						  n * MSG_LOCATOR_SIZE,
						  rxp->reply_locs);
		if (locp != (Locator_t *) rxp->reply_locs)
			memcpy (rxp->reply_locs, locp, MSG_LOCATOR_SIZE * n);
		if ((mep->flags & RME_SWAP) != 0)
			for (i = 0, locp = (Locator_t *) rxp->reply_locs;
			     i < n;
			     i++, locp = (Locator_t *) ((char *) locp + MSG_LOCATOR_SIZE)) {
				kind = (unsigned) locp->kind;
				SWAP_UINT32 (kind, t);
				locp->kind = (LocatorKind_t) kind;
				SWAP_UINT32 (locp->port, t);
			}
		rxp->n_uc_replies = n;
	}
	if (nmclocs) {
		if (nmclocs > MAXLLOCS - nuclocs) {
			nmclocs = MAXLLOCS - nuclocs;
			warn_printf ("submsg_rx_info_reply: multicast locator list truncated!\r\n");
		}
		if (!nmclocs)
			return;

		locp = (Locator_t *) SUBMSG_DATA (mep,
						  mcofs + sizeof (uint32_t),
						  nmclocs * MSG_LOCATOR_SIZE,
						  &rxp->reply_locs [nuclocs * MSG_LOCATOR_SIZE]);
		if (locp != (Locator_t *) &rxp->reply_locs [nuclocs * MSG_LOCATOR_SIZE])
			memcpy (&rxp->reply_locs [nuclocs * MSG_LOCATOR_SIZE], locp, MSG_LOCATOR_SIZE * nmclocs);
		if ((mep->flags & RME_SWAP) != 0)
			for (i = 0, locp = (Locator_t *) &rxp->reply_locs [nuclocs * MSG_LOCATOR_SIZE];
			     i < n;
			     i++, locp = (Locator_t *) ((char *) locp + MSG_LOCATOR_SIZE)) {
				kind = (unsigned) locp->kind;
				SWAP_UINT32 (kind, t);
				locp->kind = (LocatorKind_t) kind;
				SWAP_UINT32 (locp->port, t);
			}
		rxp->n_mc_replies = nmclocs;
	}
	RX_LL_END ();
	prof_stop (rtps_rx_inf_rep, 1);
}

/* submsg_rx_info_dst -- Receive actions for the RTPS.INFO_DESTINATION 
			 subelement. */

static void submsg_rx_info_dst (RECEIVER *rxp, RME *mep)
{
	InfoDestinationSMsg	*ip;
	unsigned char		data [sizeof (InfoDestinationSMsg)];

	ctrc_printd (RTPS_ID, RTPS_RX_INFO_DEST, &mep, sizeof (mep));
	prof_start (rtps_rx_inf_dst);

	RX_LL_TRACE ("INFO-DEST");

	if (mep->length < sizeof (InfoDestinationSMsg)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	ip = (InfoDestinationSMsg *) SUBMSG_DATA (mep, 0,
					    sizeof (InfoDestinationSMsg), data);
	if (!memcmp (&ip->guid_prefix, &guid_prefix_unknown, 12))
		rxp->dst_guid_prefix = rxp->domain->participant.p_guid_prefix;
	else
		rxp->dst_guid_prefix = ip->guid_prefix;
	rxp->have_prefix = 1;
	RX_LL_END ();
	prof_stop (rtps_rx_inf_dst, 1);
}

/* submsg_rx_info_src -- Receive actions for the RTPS.INFO_SOURCE subelement. */

static void submsg_rx_info_src (RECEIVER *rxp, RME *mep)
{
	InfoSourceSMsg		*ip;
	unsigned char		data [sizeof (InfoSourceSMsg)];

	ctrc_printd (RTPS_ID, RTPS_RX_INFO_SRC, &mep, sizeof (mep));
	prof_start (rtps_rx_inf_src);

	RX_LL_TRACE ("INFO-SRC");

	if (mep->length < sizeof (InfoSourceSMsg)) { /* Invalid msg length! */
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	ip = (InfoSourceSMsg *) SUBMSG_DATA (mep, 0, sizeof (InfoSourceSMsg), data);
	rxp->src_guid_prefix = ip->guid_prefix;
	rxp->peer = participant_lookup (rxp->domain, &rxp->src_guid_prefix);
	if (rxp->peer) 
		rxp->peer->p_alive = 1;

	version_set (rxp->src_version, ip->version);
	vendor_id_set (rxp->src_vendor, ip->vendor);
	rxp->n_uc_replies = 0;
	rxp->n_mc_replies = 0;
	rxp->have_timestamp = 0;
	RX_LL_END ();
	prof_stop (rtps_rx_inf_src, 1);
}

/* submsg_rx_data_frag -- Receive actions for the RTPS.DATA_FRAG subelement. */

static void submsg_rx_data_frag (RECEIVER *rxp, RME *mep)
{
	DataFragSMsg	*dp;
	int		swap;
	uint32_t	ui32;
	uint16_t	ui16;
#ifdef RTPS_FRAGMENTS
	unsigned	length, n, sofs;
	int		ignore, qos_size, all_endpoints;
	PIDSet_t	pids [2];
	RWType_t	sf_type;
	KeyHash_t	*hp = NULL;
	StatusInfo_t	*status = NULL;
	DDS_GUIDSeq	*dwrite = NULL;
	Change_t	*cp;
	DBW		walk;
	InlineQos_t	qos;
	READER		*rdrp;
	Reader_t	*rp;
	RemWriter_t	*rwp;
	Endpoint_t	*peer_ep;
	FragInfo_t	*finfo;
#endif
	unsigned char	data [sizeof (DataFragSMsg)];
	PROF_ITER	(prof_nrdrs)

	prof_start (rtps_rx_data_frag);
	RX_LL_TRACE ("DATA-FRAG");

	if (mep->length < sizeof (DataFragSMsg)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	dp = (DataFragSMsg *) SUBMSG_DATA (mep, 0, sizeof (DataFragSMsg), data);
	swap = (mep->flags & RME_SWAP);
	if (swap) {
		SWAP_UINT16 (dp->inline_qos_ofs, ui16);
		SWAP_SEQNR (dp->writer_sn, ui32);
		SWAP_UINT32 (dp->frag_start, ui32);
		SWAP_UINT16 (dp->num_fragments, ui16);
		SWAP_UINT16 (dp->frag_size, ui16);
		SWAP_UINT32 (dp->sample_size, ui32);
	}
	ctrc_begind (RTPS_ID, RTPS_RX_DFRAG, &mep, sizeof (mep));
	ctrc_contd (&dp->writer_id, sizeof (dp->writer_id));
	ctrc_contd (&dp->writer_sn, sizeof (dp->writer_sn));
	ctrc_endd ();

#ifdef RTPS_FRAGMENTS

	/* Don't allow too large samples/fragments or running over allocated
	   buffers! Just silently ignore those. */
	if (dp->sample_size > dds_max_sample_size ||
	    dp->sample_size <= dp->frag_size ||
	    dp->frag_size < MIN_RTPS_FRAGMENT_SIZE ||
	    dp->frag_size > MAX_RTPS_FRAGMENT_SIZE)
		goto inv_submsg;
#endif
	if ((dp->writer_sn.high == 0 && dp->writer_sn.low < 1) ||
	    dp->frag_start < 1 ||
	    dp->frag_size > dp->sample_size ||
	    (dp->frag_start > 1 && 
	     (mep->header.flags & SMF_INLINE_QOS) != 0 /* &&
	    inline_qos_invalid (rxp, mhp, dp, len)*/)) { /* Invalid message! */

#ifdef RTPS_FRAGMENTS
	    inv_submsg:
#endif
		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

#ifdef RTPS_FRAGMENTS
	    save_msg:
#endif
		rxp->last_mep = *mep;
		rxp->msg_size = sizeof (DataFragSMsg);
		memcpy (rxp->msg_buffer, dp, sizeof (DataFragSMsg));
		goto cleanup1;
	}

#ifndef RTPS_FRAGMENTS

	/* TODO: Ignore these messages for now, since they are only used for
		 data samples > 64KB, which we don't support. */

#else
	/* Check if this is a valid entity type. */
	if (!entity_id_writer (dp->writer_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (rxp->peer)
		peer_ep = endpoint_lookup (rxp->peer, &dp->writer_id);
	else
		peer_ep = NULL;
	if (!peer_ep || (rwp = peer_ep->rtps) == NULL) {
		if (entity_id_eq (rtps_builtin_eids [EPB_PARTICIPANT_W], 
				  dp->writer_id)) {
			rp = (Reader_t *) rxp->domain->participant.
					p_builtin_ep [EPB_PARTICIPANT_R];
			if (!rp)
				goto unknown_dest;

			rdrp = (READER *) rp->r_rtps;
			all_endpoints = 0;
			rwp = NULL;
		}
		else {

		    unknown_dest:

			RX_LL_ERROR ("UnknownDest");
			rxp->unkn_dest++;
			rxp->last_error = R_UNKN_DEST;
			goto save_msg;
		}
	}
	else if ((all_endpoints = entity_id_unknown (dp->reader_id)) != 0) {
		rdrp = rwp->rw_reader;
		rp = (Reader_t *) rdrp->endpoint.endpoint;
	}
	else {
		if (!entity_id_reader (dp->reader_id))
			goto inv_submsg;

		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			if (entity_id_eq (rwp->rw_reader->endpoint.endpoint->ep.entity_id,
					  dp->reader_id)) {
				rdrp = rwp->rw_reader;
				rp = (Reader_t *) rdrp->endpoint.endpoint;
				break;
			}
		if (!rwp)
			goto unknown_dest;
	}

	/* Allocate a Change_t descriptor. */
	cp = hc_change_new ();
	if (!cp) {
		warn_printf ("submsg_rx_data_frag: out of memory for cache change!\r\n");
		RX_LL_END ();
		goto cleanup1;
	}
	TRC_CHANGE (cp, "submsg_rx_data_frag", 1);

	/* Create a cache change object. */
	cp->c_writer = (peer_ep) ? peer_ep->entity.handle : 0;
	if (rxp->have_timestamp)
		cp->c_time = rxp->timestamp;
	else
		sys_getftime (&cp->c_time);
	cp->c_handle = 0;	/* Not specified yet -- derived in reader. */

	length = mep->length;
	sofs = dp->inline_qos_ofs + 4;
	if (sofs >= length) {
		RX_LL_INFO ("sofs>=length - TooShort");
		cp->c_length = 0;
		goto cleanup2;
	}
	length -= sofs;	/* Total remaining data field length. */

	/* Setup message access parameters in walk structure. */
	if (mep->db) {

		/* Setup sbp, sdp and sleft variables. */
		walk.dbp = mep->db;
		walk.data = mep->data;
		walk.left = walk.dbp->size -
			    (walk.data - (unsigned char *) walk.dbp->data);

		/* Skip data offset bytes in large buffer. */
		while (sofs) {
			n = sofs;
			if (n >= walk.left) {
				n = walk.left;
				walk.dbp = walk.dbp->next;
				walk.data = walk.dbp->data;
				walk.left = walk.dbp->size;
			}
			else {
				walk.data += n;
				walk.left -= n;
			}
			sofs -= n;
		}
		if (walk.left > length)
			walk.left = length;
	}
	else {
		walk.dbp = NULL;
		walk.data = mep->data + sofs;
		walk.left = length;
	}

	/* Check message flags (Q, D and K). */
	if ((mep->header.flags & SMF_INLINE_QOS) != 0) {
		if (dp->frag_start != 1)
			goto inv_qos;

		walk.length = length;
		qos_size = pid_parse_inline_qos (&walk, &qos, pids, swap);
		if (qos_size < 0) {

		    inv_qos:

			RX_LL_ERROR ("InvQos");
			rxp->inv_qos++;
			rxp->last_error = R_INV_QOS;
			rxp->last_mep = *mep;
			rxp->msg_size = sizeof (DataSMsg);
			memcpy (rxp->msg_buffer, dp, sizeof (DataFragSMsg));
			goto cleanup2;
		}
		length -= qos_size;
		if (PID_INSET (pids [0], PID_KEY_HASH))
			hp = &qos.key_hash;
		if (PID_INSET (pids [0], PID_STATUS_INFO))
			status = &qos.status_info;
		if (PID_INSET (pids [0], PID_DIRECTED_WRITE) && 
		    qos.directed_write._length)
			dwrite = &qos.directed_write;
	}
	cp->c_kind = ALIVE;
	if ((mep->header.flags & SMF_KEY_DF) == 0 && length)
		cp->c_length = length;
	else {
		cp->c_length = 0;
		if (status) {
			RX_LL_INFO ("<status>");
			if ((status->value [3] & 1) != 0)
				cp->c_kind |= NOT_ALIVE_DISPOSED;
			if ((status->value [3] & 2) != 0)
				cp->c_kind |= NOT_ALIVE_UNREGISTERED;
			if (!cp->c_kind)
				goto inv_qos;
		}
		else
			cp->c_kind = NOT_ALIVE_UNREGISTERED;
		goto deliver_data;
	}

	/* Move data reference to change. */
	cp->c_db = (DB *) walk.dbp;
	cp->c_data = (unsigned char *) walk.data;
	if (cp->c_db)
		mep->db = NULL;

    deliver_data:

	/* Lock access to the reader. */
	lock_take (rp->r_lock);

	/* Trace DATAFRAG frame if tracing enabled. */
	RX_DATA_FRAG (rdrp, rxp->peer, dp, mep->header.flags);

	/* Add to either writer proxy or directly to reader cache. */
	if (!rdrp->endpoint.stateful) {
		PROF_INC (prof_nrdrs);
		rcl_access (cp);
		cp->c_nrefs++;
		rcl_done (cp);
		cp->c_seqnr = dp->writer_sn;
		if (hp)
			participant_add_prefix (hp->hash, 12);
		reader_add_fragment (rdrp, cp, hp, dp);
	}
	else if (rwp) {
		finfo = NULL;
		for (;;) {
			PROF_INC (prof_nrdrs);
			sf_type = rw_type (rwp->rw_reliable);
			if (dwrite) {
				ignore = 1;
				for (n = 0; n < dwrite->_length; n++)
					if (guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
							    dwrite->_buffer [n].prefix) &&
					    entity_id_eq (rp->r_entity_id,
							  dwrite->_buffer [n].entity_id)) {
						ignore = 0;
						break;
					}
				if (n >= dwrite->_length && !rwp->rw_reliable)
					continue;
			}
			else
				ignore = 0;

			/* Update Reply locator if necessary. */
			if (rwp->rw_reliable &&
			    reply_update_needed (rwp->rw_uc_dreply, rxp)) {
				lrloc_print ("RTPS: DataFrag: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rwp->proxy, rxp);
			}

			/* Handle message. */
			(*rtps_rw_event [sf_type]->data) (rwp, cp, &dp->writer_sn,
						hp, NULL, 0, dp, &finfo, ignore);

			if (!all_endpoints)
				break;

			if ((rwp = (RemWriter_t *) rwp->rw_next_guid) == NULL)
				break;

			lock_release (rp->r_lock);
			rdrp = rwp->rw_reader;
			rp = (Reader_t *) rdrp->endpoint.endpoint;
			lock_take (rp->r_lock);
		}
	}
	lock_release (rp->r_lock);

	/* Cleanup extra allocated data. */

    cleanup2:
	if (dwrite && dwrite->_length)
		xfree (dwrite->_buffer);

	TRC_CHANGE (cp, "submsg_rx_data_frag", 0);

	/* Avoid incorrect freeing of in-element data. */
	if (!cp->c_db && cp->c_nrefs == 1)
		cp->c_data = NULL;
	hc_change_free (cp);

#endif
	RX_LL_END ();

    cleanup1:
	if (mep->db) {
		db_free_data (mep->db);
		mep->db = NULL;
	}
	prof_stop (rtps_rx_data_frag, prof_nrdrs);
}

static int fragnr_set_valid (FragmentNumberSet *sp, unsigned len)
{
	return (sp->base &&
		sp->numbits &&
		sp->numbits <= 256 &&
		(((sp->numbits + 31) & ~0x1f) >> 3) == len);
}

static void swap_frag_set (FragmentNumberSet *sp, int extra)
{
	uint32_t	t;
	unsigned	i;

	SWAP_UINT32 (sp->base, t);
	SWAP_UINT32 (sp->numbits, t);
	if (sp->numbits + extra == 0 || sp->numbits > 256)
		return;

	for (i = 0; i < ((sp->numbits + 31) >> 5) + extra; i++) {
		SWAP_UINT32 (sp->bitmap [i], t);
	}
}

/* submsg_rx_nack_frag -- Receive actions for the RTPS.NACK_FRAG subelement. */

static void submsg_rx_nack_frag (RECEIVER *rxp, RME *mep)
{
	NackFragSMsg	*np;
	uint32_t	t;
	unsigned	length;
#ifdef RTPS_FRAGMENTS
	Writer_t	*wp;
	WRITER		*lwp;
	RemReader_t	*rrp;
	Endpoint_t	*peer_ep;
	int		all_endpoints;
	PROF_ITER	(n)
#endif
	unsigned char	data [sizeof (NackFragSMsg)];

	prof_start (rtps_rx_nack_frag);
	ctrc_printd (RTPS_ID, RTPS_RX_NACK_FRAG, &mep, sizeof (mep));

	RX_LL_TRACE ("NACK-FRAG");

	if (mep->header.length < sizeof (MIN_NACKFRAG_SIZE)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	length = mep->header.length;
	np = (NackFragSMsg *) SUBMSG_DATA (mep, 0, length, data);
	if ((mep->flags & RME_SWAP) != 0) {
		SWAP_SEQNR (np->writer_sn, t);
		swap_frag_set (&np->frag_nr_state, 1);
	}
	if (length > MAX_NACKFRAG_SIZE ||
	    length < MIN_NACKFRAG_SIZE ||
	    !fragnr_set_valid (&np->frag_nr_state,
			       mep->length - MIN_NACKFRAG_SIZE + sizeof (uint32_t))) {

		/* Invalid message. */
#ifdef RTPS_FRAGMENTS
	    inv_submsg:
#endif
		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

#ifdef RTPS_FRAGMENTS
	    save_msg:
#endif
		rxp->last_mep = *mep;
		rxp->msg_size = sizeof (NackFragSMsg);
		memcpy (rxp->msg_buffer, np, rxp->msg_size);
		return;
	}

#ifndef RTPS_FRAGMENTS

	/* TODO: Ignore these messages for now, since they are only used for
		 data samples > 64KB, which we don't support. */

#else
	/* Check if this is a valid entity type. */
	if (!entity_id_reader (np->reader_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (!rxp->peer ||
	    (peer_ep = endpoint_lookup (rxp->peer, &np->reader_id)) == NULL ||
	    (rrp = peer_ep->rtps) == NULL) {

	    unknown_dest:

		RX_LL_ERROR ("UnknDest");
		rxp->unkn_dest++;
		rxp->last_error = R_UNKN_DEST;
		goto save_msg;
	}
	if ((all_endpoints = entity_id_unknown (np->writer_id)) != 0)
		lwp = rrp->rr_writer;
	else {
		if (!entity_id_writer (np->writer_id))
			goto inv_submsg;

		for (; rrp; rrp = (RemReader_t *) rrp->rr_next_guid)
			if (entity_id_eq (rrp->rr_writer->endpoint.endpoint->ep.entity_id,
					  np->writer_id)) {
				lwp = rrp->rr_writer;
				break;
			}

		if (!rrp)
			goto unknown_dest;
	}
	wp = (Writer_t *) lwp->endpoint.endpoint;

	/* Lock access to the writer. */
	lock_take (wp->w_lock);

	/* Trace NACKFRAG frame if enabled. */
	RX_NACK_FRAG (lwp, rxp->peer, np, mep->header.flags);

	/* Process NACKFRAG on all reliable proxy readers. */
	for (;;) {
		PROF_INC (n);

		if (lwp->endpoint.stateful && rrp->rr_reliable) {

			/* Update Reply locator. */
			if (reply_update_needed (rrp->rr_uc_dreply, rxp)) {
				lrloc_print ("RTPS: NackFrag: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rrp->proxy, rxp);
			}
		
			/* Handle message. */
			(*rtps_rr_event [RRT_SF_REL]->nackfrag) (rrp, np);
		}

		if (!all_endpoints)
			break;

		if ((rrp = (RemReader_t *) rrp->rr_next_guid) == NULL)
			break;

		lock_release (wp->w_lock);
		lwp = rrp->rr_writer;
		wp = (Writer_t *) lwp->endpoint.endpoint;
		lock_take (wp->w_lock);
	} 
	lock_release (wp->w_lock);
	prof_stop (rtps_rx_nack_frag, n);
#endif

	RX_LL_END ();
}

/* submsg_rx_heartbeat_frag -- Receive actions for the RTPS.HEARTBEAT_FRAG
			       subelement. */

static void submsg_rx_heartbeat_frag (RECEIVER *rxp, RME *mep)
{
	HeartbeatFragSMsg	*hp;
	uint32_t		t;
#ifdef RTPS_FRAGMENTS
	Reader_t		*rp;
	RemWriter_t		*rwp;
	READER			*rdrp;
	Endpoint_t		*peer_ep;
	int			all_endpoints;
	PROF_ITER		(n)
#endif
	unsigned char		data [sizeof (HeartbeatFragSMsg)];

	prof_start (rtps_rx_hbeat_frag);
	ctrc_printd (RTPS_ID, RTPS_RX_HBEAT_FRAG, &mep, sizeof (mep));

	RX_LL_TRACE ("HEARTBEAT-FRAG");

	if (mep->length < sizeof (HeartbeatFragSMsg)) {
		RX_LL_ERROR ("TooShort");
		rxp->submsg_too_short++;
		rxp->last_error = R_TOO_SHORT;
		rxp->last_mep = *mep;
		rxp->msg_size = 0;
		return;
	}
	hp = (HeartbeatFragSMsg *) SUBMSG_DATA (mep, 0, sizeof (HeartbeatFragSMsg), data);
	if ((mep->flags & RME_SWAP) != 0) {
		SWAP_SEQNR (hp->writer_sn, t);
		SWAP_UINT32 (hp->last_frag, t);
		SWAP_UINT32 (hp->count, t);
	}
	if (!SEQNR_VALID (hp->writer_sn) ||
	    hp->last_frag <= 1) {	/* Invalid message! */

#ifdef RTPS_FRAGMENTS
	    inv_submsg:
#endif
		RX_LL_ERROR ("InvSubmsg");
		rxp->inv_submsgs++;
		rxp->last_error = R_INV_SUBMSG;

#ifdef RTPS_FRAGMENTS
	    save_msg:
#endif
		rxp->last_mep = *mep;
		rxp->msg_size = sizeof (HeartbeatFragSMsg);
		memcpy (rxp->msg_buffer, hp, rxp->msg_size);
		return;
	}

#ifndef RTPS_FRAGMENTS

	/* TODO: Ignore these messages for now, since they are only used for
		 data samples > 64KB, which we don't support. */

#else
	/* Check if this is a valid entity type. */
	if (!entity_id_writer (hp->writer_id))
		goto inv_submsg;

	if (rxp->have_prefix &&
	    !guid_prefix_eq (rxp->domain->participant.p_guid_prefix,
	    		     rxp->dst_guid_prefix))
		goto unknown_dest;

	if (!rxp->peer ||
	    (peer_ep = endpoint_lookup (rxp->peer, &hp->writer_id)) == NULL ||
	    (rwp = peer_ep->rtps) == NULL) {

	    unknown_dest:

		RX_LL_ERROR ("UnknDest");
		rxp->unkn_dest++;
		rxp->last_error = R_UNKN_DEST;
		goto save_msg;
	}
	if ((all_endpoints = entity_id_unknown (hp->reader_id)) != 0)
		rdrp = rwp->rw_reader;
	else {
		if (!entity_id_reader (hp->reader_id))
			goto inv_submsg;

		for (; rwp; rwp = (RemWriter_t *) rwp->rw_next_guid)
			if (entity_id_eq (rwp->rw_reader->endpoint.endpoint->ep.entity_id,
					  hp->reader_id)) {
				rdrp = rwp->rw_reader;
				break;
			}

		if (!rwp)
			goto unknown_dest;
	}
	rp = (Reader_t *) rdrp->endpoint.endpoint;

	/* Lock access to the reader. */
	lock_take (rp->r_lock);

	/* Trace HEARTBEAT frame if enabled. */
	RX_HEARTBEAT_FRAG (rdrp, rxp->peer, hp, mep->header.flags);

	/* Process HEARTBEAT on all reliable proxy writers. */
	for (;;) {
		PROF_INC (n);

		if (rwp->rw_reader->endpoint.stateful && rwp->rw_reliable) {

			/* Update Reply locator if necessary. */
			if (reply_update_needed (rwp->rw_uc_dreply, rxp)) {
				lrloc_print ("RTPS: HeartbeatFrag: ");
				proxy_update_reply_locators (rxp->domain->kinds,
							     &rwp->proxy, rxp);
			}

			/* Handle message. */
			(*rtps_rw_event [RWT_SF_REL]->hbfrag) (rwp, hp);
		}
		if (!all_endpoints)
			break;

		if ((rwp = (RemWriter_t *) rwp->rw_next_guid) == NULL)
			break;

		lock_release (rp->r_lock);
		rdrp = rwp->rw_reader;
		rp = (Reader_t *) rdrp->endpoint.endpoint;
		lock_take (rp->r_lock);
	}
	lock_release (rp->r_lock);
	prof_stop (rtps_rx_hbeat_frag, 1);
#endif

	RX_LL_END ();
}

#ifdef RTPS_TRC_LRBUFS
#define	TRC_LR_POOL()	rtps_trc_lrpool ()

static void rtps_trc_lrpool (void)
{
	log_printf (RTPS_ID, 0, "LRxPool: Total=%u, Extra=%u, Grown=%u!\r\n",
				mdata_pool->md_count, mdata_pool->md_xcount,
				mdata_pool->md_gcount);
}

#else
#define	TRC_LR_POOL()
#endif

int rtps_rx_active;

/* rtps_receive -- Received an RTPS message from some transport system. */

void rtps_receive (unsigned        id,		/* Participant id. */
		   RMBUF           *msgs,	/* RTPS messages. */
		   const Locator_t *src)	/* Source locator. */
{
	RECEIVER	*rxp = &rtps_receiver;
	Domain_t	*dp;
	RMBUF		*msgp;
	RME		*mep;
	MsgHeader	*mhp;
	PROF_ITER	(n);

	prof_start (rtps_rx_msgs);
	atomic_set_w (rtps_rx_active, 1);
	rxp->domain = dp = domain_get (id, 1, NULL);
	rxp->src_locator = *src;
	if (!dp) {
		atomic_set_w (rtps_rx_active, 0);
		rtps_free_messages (msgs);
		return;
	}
	for (msgp = msgs; msgp; msgp = msgp->next) {

#ifdef RTPS_LOG_MSGS
		rtps_log_message (RTPS_ID, 0, msgp, 'r', 0);
#endif
		ctrc_printd (RTPS_ID, RTPS_RX_MSGS, &id, sizeof (id));
		PROF_INC (n);

#ifdef RTPS_TRC_POOL
		log_printf (RTPS_ID, 0, "rtps_receive: start\r\n");
		rtps_pool_stats (1);
#endif
		/* Ignore messages we sent ourselves. */
		mhp = &msgp->header;
		/*if (!memcmp (&mhp->guid_prefix,
			     &dp->participant.p_guid_prefix,
			     sizeof (GuidPrefix_t)))
			continue;*/

		rxp->n_uc_replies = 0;
		/*rxp->reply_locs [0] = locator_invalid;*/
		rxp->n_mc_replies = 0;
		/*rxp->reply_locs [1] = locator_invalid;*/

		/* Set the initial state of the receiver. */
		rxp->src_guid_prefix = mhp->guid_prefix;
		/*rxp->dst_guid_prefix = dp->participant.p_guid_prefix;*/
		rxp->have_prefix = 0;
		version_set (rxp->src_version, mhp->version);
		vendor_id_set (rxp->src_vendor, mhp->vendor_id);
		rxp->have_timestamp = 0;

		/* Validate protocol type and version. */
		if (!protocol_valid (mhp->protocol) ||
		    version_compare (mhp->version,
				     rtps_protocol_version) > 0)
			continue;

		/* Get sending Participant info. */
		rxp->peer = participant_lookup (dp, &rxp->src_guid_prefix);
		if (rxp->peer)
			rxp->peer->p_alive = 1;

		/* Parse every received message element. */
		RX_LL_TRACE_MSG ();
		TRC_LR_POOL ();
		for (mep = msgp->first; mep; mep = mep->next) {
			if (mep->header.id >= SMF_VENDOR) {
				/* We don't support these ... */
				continue;
			}
			/*log_printf (RTPS_ID, 0, "[id:%u,data:%p,db:%p,len:%u,f:0x%x]",
				mep->header.id, mep->data, mep->db, mep->length, mep->flags);*/
			switch (mep->header.id) {
				case ST_DATA:
					submsg_rx_data (rxp, mep);
					break;
				case ST_GAP:
					submsg_rx_gap (rxp, mep);
					break;
				case ST_HEARTBEAT:
					submsg_rx_heartbeat (rxp, mep);
					break;
				case ST_ACKNACK:
					submsg_rx_acknack (rxp, mep);
					break;
				case ST_PAD:
					break;
				case ST_INFO_TS:
					submsg_rx_info_ts (rxp, mep);
					break;
				case ST_INFO_REPLY:
					submsg_rx_info_reply (rxp, mep);
					break;
				case ST_INFO_DST:
					submsg_rx_info_dst (rxp, mep);
					break;
				case ST_INFO_SRC:
					submsg_rx_info_src (rxp, mep);
					break;
				case ST_DATA_FRAG:
					submsg_rx_data_frag (rxp, mep);
					break;
				case ST_NACK_FRAG:
					submsg_rx_nack_frag (rxp, mep);
					break;
				case ST_HEARTBEAT_FRAG:
					submsg_rx_heartbeat_frag (rxp, mep);
					break;
				default: /* Ignore other messages. */
					log_printf (RTPS_ID, 0, "rtps_receive: unknown submessage type (0x%02x).\r\n", mep->header.id);
					break;
			}
		}
		/*log_printf (RTPS_ID, 0, "\r\n");*/
		RX_LL_TRACE_MSG_END ();
#ifdef RTPS_TRC_POOL
		log_printf (RTPS_ID, 0, "rtps_receive: end\r\n");
		rtps_pool_stats (1);
#endif
	}
	lock_release (dp->lock);
	atomic_set_w (rtps_rx_active, 0);
	rtps_free_messages (msgs);
	TRC_LR_POOL ();
	prof_stop (rtps_rx_msgs, n);
#ifdef RTPS_TRC_POOL
	log_printf (RTPS_ID, 0, "rtps_receive: all frames released\r\n");
	rtps_pool_stats (1);
#endif
}

